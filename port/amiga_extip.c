/*
 * amiga_extip.c - The internet address of this Amiga, for a game over the
 * internet: the other player types it in (see amiga_net.h).
 *
 * Asked once a run, when the server view of the LAN box opens: a process
 * of its own ("Settlers IP") sends "GET / HTTP/1.0" to api.ipify.org port 80;
 * the answer's body is the address. Nothing else goes out. The game never
 * waits: it reads the result when it is there.
 *
 * The process has its own bsdsocket.library base (a library base belongs
 * to the task that opened it), hence the name below for this file.
 */

#define SocketBase ExtIpSocketBase

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
#include <netdb.h>

#include <string.h>

#include "amiga_net.h"
#include "amiga_time.h"

struct Library *ExtIpSocketBase = NULL;

static const char kHost[] = "api.ipify.org";
/* The connection and the answer; the name lookup before it is the
   stack's (it has its own limit). */
static const ULONG kTimeoutMs = 5000;

static volatile int ext_state = 0;      /* 0 not asked, 1 asking, 2 done */
static char ext_text[48];
static struct Task *volatile ext_task = NULL;
static struct Task *ext_parent = NULL;
static BYTE ext_signal = -1;
/* Asked to end (as amiga_net.c: not only Ctrl-C, warpSOCKET may not show
   it from WaitSelect). */
static volatile int ext_quit = 0;

static int is_digit(char c) { return c >= '0' && c <= '9'; }

static int is_hex(char c)
{
  return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* a.b.c.d, each 0..255 */
static int is_ipv4(const char *s)
{
  int parts = 0;
  while (parts < 4) {
    int value = 0, digits = 0;
    while (is_digit(*s) && digits < 3) {
      value = value * 10 + (*s++ - '0');
      digits++;
    }
    if (digits == 0 || value > 255) return 0;
    parts++;
    if (parts < 4) {
      if (*s != '.') return 0;
      s++;
    }
  }
  return *s == 0;
}

/* Hex digits and colons (the last part may be IPv4 with dots). */
static int is_ipv6(const char *s)
{
  int colons = 0, hex = 0, len = (int)strlen(s), i;
  if (len < 2 || len > 45) return 0;
  for (i = 0; i < len; i++) {
    if (s[i] == ':') colons++;
    else if (is_hex(s[i])) hex++;
    else if (s[i] != '.') return 0;
  }
  return colons >= 2 && colons <= 7 && hex > 0;
}

static void wait_ms(long fd, int write, ULONG end)
{
  fd_set set;
  struct timeval tv;
  ULONG sigs = SIGBREAKF_CTRL_C;
  LONG left = (LONG)(end - amiga_now_ms());
  if (left <= 0) return;
  /* Short waits: the quit flag is seen even if the signal is not. */
  if (left > 200) left = 200;
  FD_ZERO(&set);
  FD_SET(fd, &set);
  tv.tv_secs = 0;
  tv.tv_micro = (ULONG)left * 1000;
  WaitSelect(fd + 1, write ? NULL : &set, write ? &set : NULL, NULL, &tv,
             &sigs);
  if (sigs & SIGBREAKF_CTRL_C) ext_quit = 1;
}

/* The body of a "200" answer, checked; "" otherwise. */
static void take_answer(char *answer, int len, char *text, int size)
{
  char *body, *end;
  answer[len] = 0;
  text[0] = 0;
  if (strncmp(answer, "HTTP/1.", 7) != 0 || len < 12 ||
      strncmp(answer + 8, " 200", 4) != 0) {
    return;
  }
  body = strstr(answer, "\r\n\r\n");
  if (body == NULL) return;
  body += 4;
  while (*body == ' ' || *body == '\r' || *body == '\n' || *body == '\t') {
    body++;
  }
  end = body + strlen(body);
  while (end > body && (end[-1] == ' ' || end[-1] == '\r' ||
                        end[-1] == '\n' || end[-1] == '\t')) {
    *--end = 0;
  }
  if ((int)strlen(body) >= size) return;
  if (is_ipv4(body) || is_ipv6(body)) strcpy(text, body);
}

static void lookup(char *text, int size)
{
  static const char request[] =
    "GET / HTTP/1.0\r\nHost: api.ipify.org\r\n\r\n";
  static char answer[1024];
  struct hostent *he;
  struct sockaddr_in to;
  long fd, one = 1, r, err = 0;
  socklen_t elen = sizeof(err);
  int len = 0, sent = 0;
  ULONG end;

  text[0] = 0;
  he = gethostbyname((STRPTR)kHost);
  if (he == NULL || he->h_addr_list[0] == NULL || ext_quit) return;
  memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_port = htons(80);
  memcpy(&to.sin_addr, he->h_addr_list[0], sizeof(to.sin_addr));

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return;
  IoctlSocket(fd, FIONBIO, (char *)&one);
  end = amiga_now_ms() + kTimeoutMs;
  r = connect(fd, (struct sockaddr *)&to, sizeof(to));
  if (r < 0 && Errno() != EINPROGRESS) {
    CloseSocket(fd);
    return;
  }
  /* Connected when the socket can be written to. */
  if (r < 0) {
    for (;;) {
      fd_set set;
      struct timeval tv;
      if (ext_quit || (LONG)(end - amiga_now_ms()) <= 0) {
        CloseSocket(fd);
        return;
      }
      wait_ms(fd, 1, end);
      FD_ZERO(&set);
      FD_SET(fd, &set);
      tv.tv_secs = 0;
      tv.tv_micro = 0;
      if (WaitSelect(fd + 1, NULL, &set, NULL, &tv, NULL) > 0) break;
    }
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err != 0) {
      CloseSocket(fd);
      return;
    }
  }
  while (sent < (int)sizeof(request) - 1) {
    long n = send(fd, (char *)request + sent, sizeof(request) - 1 - sent, 0);
    if (n > 0) {
      sent += (int)n;
      continue;
    }
    if (n < 0 && Errno() != EWOULDBLOCK) break;
    if (ext_quit || (LONG)(end - amiga_now_ms()) <= 0) break;
    wait_ms(fd, 1, end);
  }
  /* The answer to its end (HTTP/1.0: the server closes) or as much as
     fits. */
  while (sent == (int)sizeof(request) - 1 && len < (int)sizeof(answer) - 1) {
    long n = recv(fd, answer + len, sizeof(answer) - 1 - len, 0);
    if (n > 0) {
      len += (int)n;
      continue;
    }
    if (n == 0) break;
    if (Errno() != EWOULDBLOCK) {
      len = 0;
      break;
    }
    if (ext_quit || (LONG)(end - amiga_now_ms()) <= 0) {
      len = 0;
      break;
    }
    wait_ms(fd, 0, end);
  }
  CloseSocket(fd);
  if (len > 0) take_answer(answer, len, text, size);
}

static void ext_main(void)
{
  char result[sizeof(ext_text)];
  result[0] = 0;
  ExtIpSocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
  if (ExtIpSocketBase != NULL) {
    lookup(result, sizeof(result));
    CloseLibrary(ExtIpSocketBase);
    ExtIpSocketBase = NULL;
  }
  strcpy(ext_text, result);
  Forbid();
  ext_state = 2;
  ext_task = NULL;
  Signal(ext_parent, 1UL << ext_signal);
}

void net_external_lookup(void)
{
  struct Process *p;
  if (ext_state != 0) return;
  ext_state = 2;  /* done, with nothing, unless the process starts */
  ext_text[0] = 0;
  amiga_now_ms();  /* timer.device open before the process uses it */
  ext_parent = FindTask(NULL);
  ext_signal = AllocSignal(-1);
  if (ext_signal < 0) return;
  SetSignal(0, 1UL << ext_signal);
  ext_quit = 0;
  ext_state = 1;
  Forbid();
  p = CreateNewProcTags(NP_Entry, (ULONG)ext_main,
                        NP_Name, (ULONG)"Settlers IP",
                        NP_Priority, 0,
                        NP_StackSize, 16384,
                        TAG_DONE);
  if (p != NULL) ext_task = (struct Task *)p;
  Permit();
  if (p == NULL) {
    ext_state = 2;
    FreeSignal(ext_signal);
    ext_signal = -1;
  }
}

int net_external_address(char *text, int size)
{
  if (ext_state != 2) return 0;
  strncpy(text, ext_text, size - 1);
  text[size - 1] = 0;
  return 1;
}

void net_external_quit(void)
{
  if (ext_signal < 0) return;
  if (ext_task != NULL) {
    ext_quit = 1;
    Signal((struct Task *)ext_task, SIGBREAKF_CTRL_C);
  }
  while (ext_task != NULL) {
    Wait(1UL << ext_signal);
  }
  FreeSignal(ext_signal);
  ext_signal = -1;
}
