/*
 * amiga_report.c - A bug report to the game server (server/ of the port):
 * the e-mail address and words of the player, the state of the game (F10)
 * and the log, sent by a process of its own ("Settlers report"). The game
 * never waits: it starts the sending and reads the result when it is there.
 *
 * On the wire (port 6810, the frames of amiga_net.c): RELAY "REPORT 1
 * <version> <kind>", "FIELD <key> <value>" (email, text, machine), for each
 * file "FILE <name> <size>" and its bytes in RELAY_DATA frames (6), "DONE";
 * the server answers "REPORTED <number>" or "ERR <why>".
 *
 * Its own bsdsocket.library base (a library base belongs to the task that
 * opened it).
 */

#define SocketBase ReportSocketBase

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

struct Library *ReportSocketBase = NULL;

enum { F_RELAY = 5, F_RELAY_DATA = 6 };
/* Each step (connecting, a piece sent, the answer) within this. */
static const ULONG kStepMs = 20000;

#define MAX_FILES 4

static volatile int rep_state = 0;   /* 0 idle, 1 sending, 2 done */
static int rep_ok;
static char rep_text[64];
static struct Task *volatile rep_task = NULL;
static struct Task *rep_parent = NULL;
static BYTE rep_signal = -1;
static volatile int rep_quit = 0;

/* What to send (copied when it starts). */
static char r_host[64], r_version[16], r_kind[12], r_email[96],
            r_text[1100], r_machine[128];
static int r_port, r_files;
static char r_file[MAX_FILES][108];

static long fd = -1;

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
  if (sigs & SIGBREAKF_CTRL_C) rep_quit = 1;
}

/* All of buf out; 0 on a timeout or an error. */
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
    if (rep_quit || (LONG)(end - amiga_now_ms()) <= 0) return 0;
    wait_fd(1, end);
  }
  return 1;
}

static int frame(int type, const void *data, int len)
{
  UBYTE b[2 + 250];
  if (len > 250) len = 250;
  b[0] = (UBYTE)(1 + len);
  b[1] = (UBYTE)type;
  memcpy(b + 2, data, len);
  return send_all(b, 2 + len);
}

static int text_frame(const char *line)
{
  return frame(F_RELAY, line, (int)strlen(line));
}

/* "FIELD key value" in pieces of up to 200 letters (the server joins
   them). */
static int field(const char *key, const char *value)
{
  char line[230];
  int len = (int)strlen(value), pos = 0;
  do {
    int n = len - pos > 200 ? 200 : len - pos;
    snprintf(line, sizeof(line), "FIELD %s %.*s", key, n, value + pos);
    if (!text_frame(line)) return 0;
    pos += n;
  } while (pos < len);
  return 1;
}

/* The name of a file without its path. */
static const char *base_name(const char *path)
{
  const char *b = path, *p;
  for (p = path; *p; p++) {
    if (*p == ':' || *p == '/') b = p + 1;
  }
  return b;
}

/* The file in RELAY_DATA frames of 240 bytes, 32 frames to a send: one
   send a frame made the sending wait for each acknowledgement (1.5 KB/s
   with a save of 786 KB, 0.131). */
#define FILE_FRAMES 32
static UBYTE file_out[FILE_FRAMES * 242];

static int send_file(const char *path)
{
  BPTR fh = Open((CONST_STRPTR)path, MODE_OLDFILE);
  LONG size, n;
  char line[160];
  UBYTE buf[240];
  int out = 0;
  if (fh == 0) return 1;  /* not there: nothing to send */
  Seek(fh, 0, OFFSET_END);
  size = Seek(fh, 0, OFFSET_BEGINNING);
  if (size < 0) size = 0;
  snprintf(line, sizeof(line), "FILE %s %ld", base_name(path), (long)size);
  if (!text_frame(line)) { Close(fh); return 0; }
  while (size > 0 && (n = Read(fh, buf, sizeof(buf))) > 0) {
    if (n > size) n = size;
    file_out[out] = (UBYTE)(1 + n);
    file_out[out + 1] = F_RELAY_DATA;
    memcpy(file_out + out + 2, buf, n);
    out += 2 + (int)n;
    size -= n;
    if (out > (int)sizeof(file_out) - 242) {
      if (!send_all(file_out, out)) { Close(fh); return 0; }
      out = 0;
    }
  }
  if (out > 0 && !send_all(file_out, out)) { Close(fh); return 0; }
  Close(fh);
  /* A file shorter than it said: the server waits for the rest - the
     rest as zeros. */
  memset(buf, 0, sizeof(buf));
  while (size > 0) {
    n = size > 240 ? 240 : size;
    if (!frame(F_RELAY_DATA, buf, (int)n)) return 0;
    size -= n;
  }
  return 1;
}

static void result(int ok, const char *text)
{
  rep_ok = ok;
  strncpy(rep_text, text, sizeof(rep_text) - 1);
  rep_text[sizeof(rep_text) - 1] = 0;
}

static void report(void)
{
  struct sockaddr_in to;
  struct hostent *he;
  long one = 1, r, err = 0;
  socklen_t elen = sizeof(err);
  ULONG end;
  char line[160];
  UBYTE answer[256];
  int len = 0, i;

  result(0, "The game server did not answer.");
  memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_port = htons(r_port);
  to.sin_addr.s_addr = inet_addr((STRPTR)r_host);
  if (to.sin_addr.s_addr == INADDR_NONE) {
    he = gethostbyname((STRPTR)r_host);
    if (he == NULL || he->h_addr_list[0] == NULL) {
      result(0, "Game server not found.");
      return;
    }
    memcpy(&to.sin_addr, he->h_addr_list[0], sizeof(to.sin_addr));
  }
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return;
  IoctlSocket(fd, FIONBIO, (char *)&one);
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  {
    long buf = 32768;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
  }
  end = amiga_now_ms() + kStepMs;
  r = connect(fd, (struct sockaddr *)&to, sizeof(to));
  if (r < 0 && Errno() != EINPROGRESS) goto done;
  if (r < 0) {
    for (;;) {
      fd_set set;
      struct timeval tv;
      if (rep_quit || (LONG)(end - amiga_now_ms()) <= 0) goto done;
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
  result(0, "Sending stopped.");
  snprintf(line, sizeof(line), "REPORT %d %s %s", NET_PROTOCOL, r_version,
           r_kind);
  if (!text_frame(line)) goto done;
  if (!field("email", r_email) || !field("text", r_text) ||
      !field("machine", r_machine)) {
    goto done;
  }
  for (i = 0; i < r_files; i++) {
    if (!send_file(r_file[i])) goto done;
  }
  if (!text_frame("DONE")) goto done;
  /* The answer: one RELAY frame. */
  end = amiga_now_ms() + kStepMs;
  while (len < 2 || len < 1 + answer[0]) {
    long n = recv(fd, answer + len, sizeof(answer) - 1 - len, 0);
    if (n > 0) { len += (int)n; continue; }
    if (n == 0 || Errno() != EWOULDBLOCK) break;
    if (rep_quit || (LONG)(end - amiga_now_ms()) <= 0) break;
    wait_fd(0, end);
  }
  if (len >= 2 && len >= 1 + answer[0] && answer[1] == F_RELAY) {
    int n = answer[0] - 1;
    answer[2 + n] = 0;
    if (strncmp((char *)answer + 2, "REPORTED ", 9) == 0) {
      result(1, (char *)answer + 11);    /* the number */
    } else if (strncmp((char *)answer + 2, "ERR ", 4) == 0) {
      result(0, (char *)answer + 6);
    }
  }
done:
  CloseSocket(fd);
  fd = -1;
}

static void rep_main(void)
{
  ReportSocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
  if (ReportSocketBase != NULL) {
    report();
    CloseLibrary(ReportSocketBase);
    ReportSocketBase = NULL;
  } else {
    result(0, "No TCP/IP stack (bsdsocket).");
  }
  Forbid();
  rep_state = 2;
  rep_task = NULL;
  Signal(rep_parent, 1UL << rep_signal);
}

static void copy(char *dst, int size, const char *src)
{
  strncpy(dst, src != NULL ? src : "", size - 1);
  dst[size - 1] = 0;
}

int report_send(const char *host, int port, const char *version,
                const char *kind, const char *email, const char *text,
                const char *machine, const char *const *files, int nfiles)
{
  struct Process *p;
  int i;
  if (rep_state == 1) return 0;
  copy(r_host, sizeof(r_host), host);
  copy(r_version, sizeof(r_version), version);
  copy(r_kind, sizeof(r_kind), kind);
  copy(r_email, sizeof(r_email), email);
  copy(r_text, sizeof(r_text), text);
  copy(r_machine, sizeof(r_machine), machine);
  r_port = port > 0 ? port : NET_PORT;
  r_files = nfiles > MAX_FILES ? MAX_FILES : nfiles;
  for (i = 0; i < r_files; i++) copy(r_file[i], sizeof(r_file[i]), files[i]);
  amiga_now_ms();  /* timer.device open before the process uses it */
  if (rep_signal < 0) {
    rep_parent = FindTask(NULL);
    rep_signal = AllocSignal(-1);
    if (rep_signal < 0) return 0;
  }
  SetSignal(0, 1UL << rep_signal);
  rep_quit = 0;
  rep_state = 1;
  Forbid();
  p = CreateNewProcTags(NP_Entry, (ULONG)rep_main,
                        NP_Name, (ULONG)"Settlers report",
                        /* Over the game (it mostly waits for the
                           network): at 0 a busy game left it so little
                           time that a save of 786 KB took 4 minutes. */
                        NP_Priority, 1,
                        NP_StackSize, 16384,
                        TAG_DONE);
  if (p != NULL) rep_task = (struct Task *)p;
  Permit();
  if (p == NULL) {
    rep_state = 0;
    return 0;
  }
  return 1;
}

int report_result(char *text, int size, int *ok)
{
  if (rep_state != 2) return 0;
  strncpy(text, rep_text, size - 1);
  text[size - 1] = 0;
  *ok = rep_ok;
  rep_state = 0;
  return 1;
}

int report_network(void)
{
  /* The TCP/IP stack running: its bsdsocket.library in memory. Not opened
     here - a library base belongs to the task that opened it, and the
     game's task opening and closing it stopped the sending of the report
     process (0.131, Amiga 1). */
  struct Node *lib;
  Forbid();
  lib = FindName(&SysBase->LibList, (CONST_STRPTR)"bsdsocket.library");
  Permit();
  return lib != NULL;
}

void report_quit(void)
{
  if (rep_signal < 0) return;
  if (rep_task != NULL) {
    rep_quit = 1;
    Signal((struct Task *)rep_task, SIGBREAKF_CTRL_C);
  }
  while (rep_task != NULL) {
    Wait(1UL << rep_signal);
  }
  FreeSignal(rep_signal);
  rep_signal = -1;
}
