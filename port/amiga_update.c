/*
 * amiga_update.c - The check for a newer release of the game on the update
 * server (the data server, update-server/ of the port; not the game
 * server), by a process of its own ("Settlers update"). The game never
 * waits: it starts the check and reads the state when it draws its box.
 * The game only checks; downloading and updating is the work of the WARP
 * program on the Workbench.
 *
 * On the wire (the frames of amiga_net.c): RELAY "UPDATE 1 <version>
 * <lang>"; the server: "LATEST <version>", "NOTE <line>"... (what changed
 * since <version>), "FILE ..." lines (for the WARP program; skipped here),
 * "END"; then "BYE" from here.
 *
 * Its own bsdsocket.library base (a library base belongs to the task that
 * opened it).
 */

#define SocketBase UpdateSocketBase

#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>

#include "amiga_net.h"
#include "amiga_time.h"

struct Library *UpdateSocketBase = NULL;

enum { F_RELAY = 5 };
/* Each step (connecting, the answer) within this. */
static const ULONG kStepMs = 20000;

static volatile int up_state = 0;   /* 0 idle, 1 working, 2 done */
static UpdateInfo info;             /* written by the process */
static struct Task *volatile up_task = NULL;
static struct Task *up_parent = NULL;
static BYTE up_signal = -1;
static volatile int up_quit = 0;

static char u_host[64], u_version[16], u_lang[4];
static int u_port;

static long fd = -1;
static UBYTE in_buf[4096];
static int in_len;

static void wait_fd(int write, ULONG end)
{
  fd_set set;
  struct timeval tv;
  ULONG sigs = SIGBREAKF_CTRL_C;
  LONG left = (LONG)(end - amiga_now_ms());
  if (left <= 0) return;
  if (left > 200) left = 200;
  FD_ZERO(&set);
  FD_SET(fd, &set);
  tv.tv_secs = 0;
  tv.tv_micro = (ULONG)left * 1000;
  WaitSelect(fd + 1, write ? NULL : &set, write ? &set : NULL, NULL, &tv,
             &sigs);
  if (sigs & SIGBREAKF_CTRL_C) up_quit = 1;
}

static int send_all(const UBYTE *buf, int len)
{
  ULONG end = amiga_now_ms() + kStepMs;
  int sent = 0;
  while (sent < len) {
    long n = send(fd, (UBYTE *)buf + sent, len - sent, 0);
    if (n > 0) {
      sent += (int)n;
      end = amiga_now_ms() + kStepMs;
      continue;
    }
    if (n < 0 && Errno() != EWOULDBLOCK) return 0;
    if (up_quit || (LONG)(end - amiga_now_ms()) <= 0) return 0;
    wait_fd(1, end);
  }
  return 1;
}

static int text_frame(const char *line)
{
  UBYTE b[2 + 250];
  int len = (int)strlen(line);
  if (len > 250) len = 250;
  b[0] = (UBYTE)(1 + len);
  b[1] = F_RELAY;
  memcpy(b + 2, line, len);
  return send_all(b, 2 + len);
}

/* The next frame (data with a zero after it); 0 at the end or an error. */
static int next_frame(int *type, UBYTE *data, int *len)
{
  ULONG end = amiga_now_ms() + kStepMs;
  for (;;) {
    if (in_len >= 2 && in_buf[0] == 0) return 0;
    if (in_len >= 2 && in_len >= 1 + in_buf[0]) {
      int n = in_buf[0];
      *type = in_buf[1];
      *len = n - 1;
      memcpy(data, in_buf + 2, n - 1);
      data[n - 1] = 0;
      memmove(in_buf, in_buf + 1 + n, in_len - 1 - n);
      in_len -= 1 + n;
      return 1;
    }
    {
      long r = recv(fd, in_buf + in_len, sizeof(in_buf) - in_len, 0);
      if (r > 0) {
        in_len += (int)r;
        end = amiga_now_ms() + kStepMs;
        continue;
      }
      if (r == 0 || Errno() != EWOULDBLOCK) return 0;
    }
    if (up_quit || (LONG)(end - amiga_now_ms()) <= 0) return 0;
    wait_fd(0, end);
  }
}

static void say(int ok, const char *text)
{
  info.ok = ok;
  strncpy(info.text, text, sizeof(info.text) - 1);
  info.text[sizeof(info.text) - 1] = 0;
}

/* The newest version and what changed since this one. */
static int read_answer(void)
{
  UBYTE data[256];
  int type, len;
  info.latest[0] = 0;
  info.notes[0] = 0;
  for (;;) {
    char *line;
    if (!next_frame(&type, data, &len) || type != F_RELAY) return 0;
    line = (char *)data;
    if (strncmp(line, "LATEST ", 7) == 0) {
      strncpy(info.latest, line + 7, sizeof(info.latest) - 1);
      info.latest[sizeof(info.latest) - 1] = 0;
    } else if (strncmp(line, "NOTE", 4) == 0) {
      const char *text = len > 5 ? line + 5 : "";
      if (strlen(info.notes) + strlen(text) + 2 < sizeof(info.notes)) {
        strcat(info.notes, text);
        strcat(info.notes, "\n");
      }
    } else if (strcmp(line, "END") == 0) {
      return info.latest[0] != 0;
    } else if (strncmp(line, "ERR ", 4) == 0) {
      say(0, line + 4);
      return 0;
    }
    /* FILE lines: the list of the release, for the WARP program. */
  }
}

static void work(void)
{
  struct sockaddr_in to;
  struct hostent *he;
  long one = 1, r, err = 0;
  socklen_t elen = sizeof(err);
  ULONG end;
  char line[64];

  say(0, "The update server did not answer.");
  memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_port = htons(u_port);
  to.sin_addr.s_addr = inet_addr((STRPTR)u_host);
  if (to.sin_addr.s_addr == INADDR_NONE) {
    he = gethostbyname((STRPTR)u_host);
    if (he == NULL || he->h_addr_list[0] == NULL) {
      say(0, "Update server not found.");
      return;
    }
    memcpy(&to.sin_addr, he->h_addr_list[0], sizeof(to.sin_addr));
  }
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return;
  in_len = 0;
  IoctlSocket(fd, FIONBIO, (char *)&one);
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  end = amiga_now_ms() + kStepMs;
  r = connect(fd, (struct sockaddr *)&to, sizeof(to));
  if (r < 0 && Errno() != EINPROGRESS) goto done;
  if (r < 0) {
    for (;;) {
      fd_set set;
      struct timeval tv;
      if (up_quit || (LONG)(end - amiga_now_ms()) <= 0) goto done;
      wait_fd(1, end);
      FD_ZERO(&set);
      FD_SET(fd, &set);
      tv.tv_secs = 0;
      tv.tv_micro = 0;
      if (WaitSelect(fd + 1, NULL, &set, NULL, &tv, NULL) > 0) break;
    }
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err != 0) goto done;
  }
  snprintf(line, sizeof(line), "UPDATE %d %s %s", NET_PROTOCOL, u_version,
           u_lang);
  if (!text_frame(line)) goto done;
  if (read_answer()) {
    text_frame("BYE");
    say(1, "");
  } else if (strncmp(info.text, "The update", 10) == 0) {
    say(0, "The answer of the update server came broken.");
  }
done:
  CloseSocket(fd);
  fd = -1;
}

static void up_main(void)
{
  UpdateSocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
  if (UpdateSocketBase != NULL) {
    work();
    CloseLibrary(UpdateSocketBase);
    UpdateSocketBase = NULL;
  } else {
    say(0, "No TCP/IP stack (bsdsocket).");
  }
  Forbid();
  up_state = 2;
  up_task = NULL;
  Signal(up_parent, 1UL << up_signal);
}

static void copy(char *dst, int size, const char *src)
{
  strncpy(dst, src != NULL ? src : "", size - 1);
  dst[size - 1] = 0;
}

int update_check(const char *host, int port, const char *version,
                 const char *lang)
{
  struct Process *p;
  if (up_state == 1) return 0;
  copy(u_host, sizeof(u_host), host);
  copy(u_version, sizeof(u_version), version);
  copy(u_lang, sizeof(u_lang), lang);
  u_port = port > 0 ? port : 6830;
  memset(&info, 0, sizeof(info));
  info.state = 1;
  amiga_now_ms();  /* timer.device open before the process uses it */
  if (up_signal < 0) {
    up_parent = FindTask(NULL);
    up_signal = AllocSignal(-1);
    if (up_signal < 0) return 0;
  }
  SetSignal(0, 1UL << up_signal);
  up_quit = 0;
  up_state = 1;
  Forbid();
  p = CreateNewProcTags(NP_Entry, (ULONG)up_main,
                        NP_Name, (ULONG)"Settlers update",
                        NP_Priority, 0,
                        NP_StackSize, 16384,
                        TAG_DONE);
  if (p != NULL) up_task = (struct Task *)p;
  Permit();
  if (p == NULL) {
    up_state = 0;
    return 0;
  }
  return 1;
}

int update_poll(UpdateInfo *out)
{
  *out = info;
  out->state = up_state;
  return up_state;
}

void update_quit(void)
{
  if (up_signal < 0) return;
  if (up_task != NULL) {
    up_quit = 1;
    Signal((struct Task *)up_task, SIGBREAKF_CTRL_C);
  }
  while (up_task != NULL) {
    Wait(1UL << up_signal);
  }
  FreeSignal(up_signal);
  up_signal = -1;
}
