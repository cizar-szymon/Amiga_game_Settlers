/* The network of a LAN game: a process of its own (see amiga_net.h). */
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

struct Library *SocketBase = NULL;

/* ---- Rings: one writer, one reader, no locks. A frame is
   length (2 bytes) | kind (1 byte) | data. The writer moves head after the
   data, the reader tail after reading it. */

#define RING_SIZE 8192

typedef struct Ring {
  volatile ULONG head, tail;
  UBYTE buf[RING_SIZE];
} Ring;

static Ring to_net, from_net;

static ULONG ring_free(const Ring *r)
{
  return RING_SIZE - 1 - ((r->head - r->tail) & (RING_SIZE - 1));
}

static void ring_write(Ring *r, ULONG *pos, const void *src, int n)
{
  const UBYTE *s = (const UBYTE *)src;
  while (n-- > 0) {
    r->buf[*pos] = *s++;
    *pos = (*pos + 1) & (RING_SIZE - 1);
  }
}

static int ring_put(Ring *r, int kind, const void *a, int alen,
                    const void *b, int blen)
{
  ULONG pos = r->head;
  int len = 1 + alen + blen;
  UBYTE hdr[3];
  if ((ULONG)(len + 2) > ring_free(r)) return 0;
  hdr[0] = (UBYTE)(len >> 8);
  hdr[1] = (UBYTE)len;
  hdr[2] = (UBYTE)kind;
  ring_write(r, &pos, hdr, 3);
  ring_write(r, &pos, a, alen);
  ring_write(r, &pos, b, blen);
  r->head = pos;
  return 1;
}

/* The kind and the length of the next frame, without taking it. */
static int ring_peek(const Ring *r, int *kind, int *length)
{
  ULONG pos = r->tail;
  int len;
  if (pos == r->head) return 0;
  len = r->buf[pos] << 8;
  len |= r->buf[(pos + 1) & (RING_SIZE - 1)];
  *kind = r->buf[(pos + 2) & (RING_SIZE - 1)];
  *length = len - 1;
  return 1;
}

/* data gets at most size bytes; the rest of a longer frame is skipped. */
static int ring_get(Ring *r, int *kind, UBYTE *data, int size, int *length)
{
  ULONG pos = r->tail;
  int len, i;
  if (pos == r->head) return 0;
  len = r->buf[pos] << 8;
  pos = (pos + 1) & (RING_SIZE - 1);
  len |= r->buf[pos];
  pos = (pos + 1) & (RING_SIZE - 1);
  *kind = r->buf[pos];
  pos = (pos + 1) & (RING_SIZE - 1);
  len--;
  for (i = 0; i < len; i++) {
    if (i < size) data[i] = r->buf[pos];
    pos = (pos + 1) & (RING_SIZE - 1);
  }
  *length = len < size ? len : size;
  r->tail = pos;
  return 1;
}

/* ---- The process and the game side */

enum { C_SERVE = 1, C_SEARCH, C_CONNECT, C_STOP, C_SEND, C_NODELAY };

static struct Task *volatile net_task = NULL;
static volatile ULONG wake_mask = 0;
static struct Task *parent_task = NULL;
static BYTE parent_signal = -1;
static volatile int start_failed = 0;
/* net_quit asks the process to end. Not only by Ctrl-C: warpSOCKET (the stack
   of Amiga 2) returned from WaitSelect with no signals while the Ctrl-C was
   sent - the process went on, the game waited for it for ever and the Amiga
   stood (found in the OpenDUNE port, 29.10.2026). */
static volatile int quit_request = 0;
static char net_version[16];

static volatile ULONG count_eclocks, count_frames_out, count_frames_in;
/* The process stopped taking frames: the ring to the game is full. */
static volatile int rx_blocked = 0;
static volatile ULONG count_bytes_out, count_bytes_in;

static void command(int kind, const void *a, int alen, const void *b,
                    int blen)
{
  if (net_task == NULL) return;
  if (ring_put(&to_net, kind, a, alen, b, blen)) {
    Signal((struct Task *)net_task, wake_mask);
  }
}

static void put_be32(UBYTE *p, ULONG v)
{
  p[0] = (UBYTE)(v >> 24); p[1] = (UBYTE)(v >> 16);
  p[2] = (UBYTE)(v >> 8); p[3] = (UBYTE)v;
}

static ULONG get_be32(const UBYTE *p)
{
  return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) |
         p[3];
}

/* An event: value, then the texts one after another with their zeros. */
static int serving;
static void event(int kind, long value, const char *t1, const char *t2,
                  const char *t3)
{
  UBYTE buf[4 + 32 + 32 + 16];
  int n = 4;
  const char *t[3];
  int i;
  t[0] = t1; t[1] = t2; t[2] = t3;
  if (kind == NEV_ERROR) value = serving ? 0 : 1;  /* 1: all is off */
  put_be32(buf, (ULONG)value);
  for (i = 0; i < 3; i++) {
    const char *s = t[i] != NULL ? t[i] : "";
    int max = i == 2 ? 15 : 31;
    int len = (int)strlen(s);
    if (len > max) len = max;
    memcpy(buf + n, s, len);
    buf[n + len] = 0;
    n += len + 1;
  }
  ring_put(&from_net, kind, buf, n, NULL, 0);
}

static void copy_text(char *dst, int size, const UBYTE **p, const UBYTE *end)
{
  int n = 0;
  while (*p < end && **p != 0) {
    if (n < size - 1) dst[n++] = (char)**p;
    (*p)++;
  }
  if (*p < end) (*p)++;
  dst[n] = 0;
}

int net_event(NetEvent *ev)
{
  UBYTE buf[4 + 1 + NET_MAX_DATA + 8];
  int kind, len;
  const UBYTE *p, *end;
  if (!ring_get(&from_net, &kind, buf, sizeof(buf), &len)) return 0;
  if (rx_blocked && net_task != NULL &&
      ring_free(&from_net) >= RING_SIZE / 2) {
    rx_blocked = 0;
    Signal((struct Task *)net_task, wake_mask);
  }
  memset(ev, 0, sizeof(*ev) - sizeof(ev->data));
  ev->kind = kind;
  if (len < 4) return 1;
  ev->value = (long)get_be32(buf);
  if (kind == NEV_MSG) {
    ev->length = len - 4;
    if (ev->length > NET_MAX_DATA) ev->length = NET_MAX_DATA;
    memcpy(ev->data, buf + 4, ev->length);
    return 1;
  }
  p = buf + 4;
  end = buf + len;
  copy_text(ev->text1, sizeof(ev->text1), &p, end);
  copy_text(ev->text2, sizeof(ev->text2), &p, end);
  copy_text(ev->text3, sizeof(ev->text3), &p, end);
  return 1;
}

void net_serve(const char *name)
{
  command(C_SERVE, name, (int)strlen(name), NULL, 0);
}

void net_search(int ms)
{
  UBYTE b[4];
  put_be32(b, (ULONG)ms);
  command(C_SEARCH, b, 4, NULL, 0);
}

void net_connect(const char *address, const char *name)
{
  command(C_CONNECT, address, (int)strlen(address) + 1, name,
          (int)strlen(name) + 1);
}

void net_stop(void)
{
  command(C_STOP, NULL, 0, NULL, 0);
}

void net_set_nodelay(int on)
{
  UBYTE b = (UBYTE)(on != 0);
  command(C_NODELAY, &b, 1, NULL, 0);
}

int net_send(int type, const void *data, int length)
{
  UBYTE t = (UBYTE)type;
  if (net_task == NULL || length > NET_MAX_DATA) return 0;
  if (!ring_put(&to_net, C_SEND, &t, 1, data, length)) return 0;
  Signal((struct Task *)net_task, wake_mask);
  return 1;
}

void net_counters(unsigned long *eclocks, unsigned long *frames_out,
                  unsigned long *frames_in, unsigned long *bytes_out,
                  unsigned long *bytes_in)
{
  *eclocks = count_eclocks;
  *frames_out = count_frames_out;
  *frames_in = count_frames_in;
  *bytes_out = count_bytes_out;
  *bytes_in = count_bytes_in;
}

/* ---- Inside the process */

enum { F_HELLO = 1, F_BYE, F_PING, F_PONG };

static long listen_fd = -1, udp_fd = -1, peer_fd = -1;
static int searching, connecting, peer_ok, nodelay = 1;
static ULONG next_announce, search_end, connect_end, next_ping;
static char my_name[32], peer_ip[16];
static UBYTE in_buf[4096];
static int in_len;
static UBYTE out_buf[8192];
static int out_len;

static void close_fd(long *fd)
{
  if (*fd >= 0) CloseSocket(*fd);
  *fd = -1;
}

static void non_blocking(long fd)
{
  long one = 1;
  IoctlSocket(fd, FIONBIO, (char *)&one);
}

static void address_text(ULONG addr, char *text, int size)
{
  const char *a = (const char *)Inet_NtoA(addr);
  strncpy(text, a != NULL ? a : "", size - 1);
  text[size - 1] = 0;
}

/* The address the way out of this Amiga has: a UDP socket "connected" to
   an address outside the network (nothing is sent) knows its own one.
   Without a route there, the address of the host name. */
static void local_address(char *text, int size)
{
  struct sockaddr_in to, me;
  socklen_t len = sizeof(me);
  long fd = socket(AF_INET, SOCK_DGRAM, 0);
  text[0] = 0;
  if (fd >= 0) {
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(9);
    to.sin_addr.s_addr = htonl(0xc0000201UL);  /* 192.0.2.1, TEST-NET */
    memset(&me, 0, sizeof(me));
    if (connect(fd, (struct sockaddr *)&to, sizeof(to)) == 0 &&
        getsockname(fd, (struct sockaddr *)&me, &len) == 0 &&
        me.sin_addr.s_addr != 0) {
      address_text(me.sin_addr.s_addr, text, size);
    }
    CloseSocket(fd);
  }
  if (text[0] == 0) {
    char host[64];
    if (gethostname(host, sizeof(host)) == 0) {
      struct hostent *he = gethostbyname((STRPTR)host);
      if (he != NULL && he->h_addr_list[0] != NULL) {
        struct in_addr a;
        memcpy(&a, he->h_addr_list[0], sizeof(a));
        address_text(a.s_addr, text, size);
      }
    }
  }
}

static void fail(const char *what)
{
  char text[32];
  snprintf(text, sizeof(text), "%s failed (%ld).", what, (long)Errno());
  event(NEV_ERROR, 0, text, NULL, NULL);
}

/* A frame to the other player, through out_buf (sent in the loop). */
static void frame_out(int type, const void *data, int length)
{
  if (peer_fd < 0 || out_len + 2 + length > (int)sizeof(out_buf)) return;
  out_buf[out_len] = (UBYTE)(1 + length);
  out_buf[out_len + 1] = (UBYTE)type;
  if (length > 0) memcpy(out_buf + out_len + 2, data, length);
  out_len += 2 + length;
  count_frames_out++;
}

static void flush_out(void)
{
  long n;
  if (peer_fd < 0 || out_len == 0 || connecting) return;
  n = send(peer_fd, out_buf, out_len, 0);
  if (n > 0) {
    count_bytes_out += n;
    memmove(out_buf, out_buf + n, out_len - n);
    out_len -= (int)n;
  }
}

static void send_hello(void)
{
  UBYTE b[1 + 16 + 32];
  int n = 1, len;
  b[0] = NET_PROTOCOL;
  len = (int)strlen(net_version);
  memcpy(b + n, net_version, len + 1);
  n += len + 1;
  len = (int)strlen(my_name);
  memcpy(b + n, my_name, len + 1);
  n += len + 1;
  frame_out(F_HELLO, b, n);
}

static void drop_peer(const char *why)
{
  if (peer_fd >= 0 && why != NULL) {
    if (peer_ok || connecting) event(NEV_LOST, 0, why, NULL, NULL);
  }
  close_fd(&peer_fd);
  peer_ok = 0;
  connecting = 0;
  in_len = 0;
  out_len = 0;
}

static void bye(const char *why)
{
  frame_out(F_BYE, why, (int)strlen(why));
  flush_out();
}

static void stop_all(void)
{
  drop_peer(NULL);
  close_fd(&listen_fd);
  close_fd(&udp_fd);
  serving = searching = 0;
}

static void set_nodelay(long fd)
{
  long on = nodelay;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

static void start_serving(const UBYTE *name, int len)
{
  struct sockaddr_in addr;
  long one = 1;
  stop_all();
  if (len > 31) len = 31;
  memcpy(my_name, name, len);
  my_name[len] = 0;

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) { fail("TCP"); return; }
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NET_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
      listen(listen_fd, 2) < 0) {
    fail("TCP port 6810");
    close_fd(&listen_fd);
    return;
  }
  non_blocking(listen_fd);

  udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd < 0 ||
      setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0) {
    fail("UDP broadcast");
    stop_all();
    return;
  }
  non_blocking(udp_fd);
  serving = 1;
  next_announce = amiga_now_ms();
  event(NEV_SERVING, 0, NULL, NULL, NULL);
}

static void announce(void)
{
  char text[96];
  struct sockaddr_in to;
  snprintf(text, sizeof(text), "SETTLERS-LAN %d %s %s", NET_PROTOCOL,
           net_version, my_name);
  memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_port = htons(NET_PORT);
  to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  sendto(udp_fd, text, strlen(text), 0, (struct sockaddr *)&to, sizeof(to));
}

static void start_search(ULONG ms)
{
  struct sockaddr_in addr;
  long one = 1;
  stop_all();
  udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd < 0) { fail("UDP"); return; }
  setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NET_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(udp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fail("UDP port 6810");
    close_fd(&udp_fd);
    return;
  }
  non_blocking(udp_fd);
  searching = 1;
  search_end = amiga_now_ms() + ms;
}

/* "SETTLERS-LAN <protocol> <version> <name>" from a server. */
static void read_announcement(void)
{
  char text[100], ip[16], version[16];
  struct sockaddr_in from;
  socklen_t len = sizeof(from);
  int proto = 0, pos = 0;
  long n = recvfrom(udp_fd, text, sizeof(text) - 1, 0,
                    (struct sockaddr *)&from, &len);
  if (n <= 0) return;
  text[n] = 0;
  if (sscanf(text, "SETTLERS-LAN %d %15s %n", &proto, version, &pos) < 2 ||
      pos == 0) {
    return;
  }
  address_text(from.sin_addr.s_addr, ip, sizeof(ip));
  event(NEV_SERVER, proto, text + pos, ip, version);
}

static void start_connect(const UBYTE *data, int len)
{
  struct sockaddr_in to;
  const UBYTE *p = data, *end = data + len;
  char ip[32];
  long r;
  stop_all();
  copy_text(ip, sizeof(ip), &p, end);
  copy_text(my_name, sizeof(my_name), &p, end);
  memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_port = htons(NET_PORT);
  to.sin_addr.s_addr = inet_addr((STRPTR)ip);
  if (to.sin_addr.s_addr == INADDR_NONE) {
    event(NEV_ERROR, 0, "Not an address.", NULL, NULL);
    return;
  }
  strncpy(peer_ip, ip, sizeof(peer_ip) - 1);
  peer_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (peer_fd < 0) { fail("TCP"); return; }
  non_blocking(peer_fd);
  set_nodelay(peer_fd);
  r = connect(peer_fd, (struct sockaddr *)&to, sizeof(to));
  if (r < 0 && Errno() != EINPROGRESS) {
    fail("Connecting");
    close_fd(&peer_fd);
    return;
  }
  connecting = 1;
  connect_end = amiga_now_ms() + 5000;
  if (r == 0) {
    connecting = 0;
    send_hello();
  }
}

static void accept_peer(void)
{
  struct sockaddr_in from;
  socklen_t len = sizeof(from);
  long fd = accept(listen_fd, (struct sockaddr *)&from, &len);
  if (fd < 0) return;
  if (peer_fd >= 0) {
    /* One player only. */
    UBYTE f[6] = { 5, F_BYE, 'b', 'u', 's', 'y' };
    send(fd, f, sizeof(f), 0);
    CloseSocket(fd);
    return;
  }
  peer_fd = fd;
  non_blocking(peer_fd);
  set_nodelay(peer_fd);
  address_text(from.sin_addr.s_addr, peer_ip, sizeof(peer_ip));
  in_len = out_len = 0;
  send_hello();
}

static void frame_in(int type, const UBYTE *d, int len)
{
  count_frames_in++;
  switch (type) {
    case F_HELLO: {
      char version[16], name[32];
      const UBYTE *p = d + 1, *end = d + len;
      if (len < 1) return;
      copy_text(version, sizeof(version), &p, end);
      copy_text(name, sizeof(name), &p, end);
      if (d[0] != NET_PROTOCOL || strcmp(version, net_version) != 0) {
        char why[32];
        snprintf(why, sizeof(why), "Different version: %s", version);
        bye("Different version");
        event(NEV_ERROR, 0, why, NULL, NULL);
        drop_peer(NULL);
        return;
      }
      peer_ok = 1;
      next_ping = amiga_now_ms();
      event(NEV_PEER, 0, name, peer_ip, version);
      break;
    }
    case F_BYE: {
      char why[32];
      int n = len < 31 ? len : 31;
      memcpy(why, d, n);
      why[n] = 0;
      if (strcmp(why, "busy") == 0) strcpy(why, "The server is busy.");
      if (!peer_ok) event(NEV_ERROR, 0, why, NULL, NULL);
      drop_peer(peer_ok ? why : NULL);
      break;
    }
    case F_PING:
      frame_out(F_PONG, d, len);
      break;
    case F_PONG:
      if (len == 4) {
        event(NEV_PING, (long)(amiga_now_ms() - get_be32(d)), NULL, NULL,
              NULL);
      }
      break;
    default:
      if (type >= NET_MSG_GAME && peer_ok) {
        UBYTE hdr[4];
        put_be32(hdr, (ULONG)type);
        ring_put(&from_net, NEV_MSG, hdr, 4, d, len);
      }
      break;
  }
}

/* The whole frames in in_buf to the game. A frame of the game waits there
   while the ring to the game is full (a saved game comes in hundreds of
   frames faster than the game takes them); rx_blocked then, and the game
   wakes the process when it has taken events (net_event). */
static void take_frames(void)
{
  int pos = 0;
  rx_blocked = 0;
  while (in_len - pos >= 2 && in_len - pos >= 1 + in_buf[pos]) {
    int len = in_buf[pos];
    if (len == 0) { drop_peer("Bad data."); return; }
    if (in_buf[pos + 1] >= NET_MSG_GAME &&
        ring_free(&from_net) < (ULONG)(len + 8)) {
      rx_blocked = 1;
      break;
    }
    frame_in(in_buf[pos + 1], in_buf + pos + 2, len - 1);
    if (peer_fd < 0) return;
    pos += 1 + len;
  }
  memmove(in_buf, in_buf + pos, in_len - pos);
  in_len -= pos;
}

static void read_peer(void)
{
  long n = recv(peer_fd, in_buf + in_len, sizeof(in_buf) - in_len, 0);
  if (n <= 0) {
    if (n < 0 && Errno() == EWOULDBLOCK) return;
    drop_peer("Disconnected.");
    return;
  }
  count_bytes_in += n;
  in_len += (int)n;
  take_frames();
}

static void do_command(int kind, const UBYTE *d, int len)
{
  switch (kind) {
    case C_SERVE: start_serving(d, len); break;
    case C_SEARCH: start_search(len >= 4 ? get_be32(d) : 3000); break;
    case C_CONNECT: start_connect(d, len); break;
    case C_STOP:
      if (peer_fd >= 0 && !connecting) bye("Disconnected.");
      stop_all();
      break;
    case C_SEND:
      if (len >= 1 && peer_ok) frame_out(d[0], d + 1, len - 1);
      break;
    case C_NODELAY:
      nodelay = len >= 1 && d[0] != 0;
      break;
  }
}

static void net_main(void)
{
  struct Task *me = FindTask(NULL);
  BYTE wake = AllocSignal(-1);
  int quit = 0;

  SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
  if (SocketBase == NULL || wake < 0) {
    start_failed = 1;
    if (SocketBase != NULL) CloseLibrary(SocketBase);
    SocketBase = NULL;
    Forbid();
    Signal(parent_task, 1UL << parent_signal);
    return;
  }
  wake_mask = 1UL << wake;
  net_task = me;
  Signal(parent_task, 1UL << parent_signal);

  {
    char address[32];
    local_address(address, sizeof(address));
    event(NEV_ADDRESS, 0, address, NULL, NULL);
  }

  while (!quit) {
    fd_set rd, wr;
    long top = -1;
    struct timeval tv;
    ULONG sigs = wake_mask | SIGBREAKF_CTRL_C;
    ULONG start;
    int timed = serving || searching || connecting || peer_ok;
    UBYTE data[1 + NET_MAX_DATA + 8];
    int kind, len;

    FD_ZERO(&rd);
    FD_ZERO(&wr);
    if (listen_fd >= 0) { FD_SET(listen_fd, &rd); top = listen_fd; }
    if (udp_fd >= 0 && searching) {
      FD_SET(udp_fd, &rd);
      if (udp_fd > top) top = udp_fd;
    }
    if (peer_fd >= 0) {
      if (connecting || out_len > 0) FD_SET(peer_fd, &wr);
      if (!connecting && !rx_blocked && in_len < (int)sizeof(in_buf)) {
        FD_SET(peer_fd, &rd);
      }
      if (peer_fd > top) top = peer_fd;
    }
    tv.tv_secs = 0;
    tv.tv_micro = 200000;
    WaitSelect(top + 1, &rd, &wr, NULL, timed ? &tv : NULL, &sigs);

    start = amiga_eclock();
    if ((sigs & SIGBREAKF_CTRL_C) || quit_request) quit = 1;

    for (;;) {
      /* A frame of the game waits in the ring until it fits into out_buf
         (sent below and as the socket takes it). */
      if (ring_peek(&to_net, &kind, &len) && kind == C_SEND && peer_ok &&
          out_len + 2 + (len - 1) > (int)sizeof(out_buf)) {
        break;
      }
      if (!ring_get(&to_net, &kind, data, sizeof(data), &len)) break;
      do_command(kind, data, len);
    }

    if (listen_fd >= 0 && FD_ISSET(listen_fd, &rd)) accept_peer();
    if (searching && udp_fd >= 0 && FD_ISSET(udp_fd, &rd)) {
      read_announcement();
    }
    if (peer_fd >= 0 && connecting && FD_ISSET(peer_fd, &wr)) {
      long err = 0;
      socklen_t elen = sizeof(err);
      getsockopt(peer_fd, SOL_SOCKET, SO_ERROR, &err, &elen);
      if (err != 0) {
        char text[32];
        snprintf(text, sizeof(text), "No server there (%ld).", err);
        event(NEV_ERROR, 0, text, NULL, NULL);
        drop_peer(NULL);
      } else {
        connecting = 0;
        send_hello();
      }
    }
    if (peer_fd >= 0 && !connecting && in_len > 0) take_frames();
    if (peer_fd >= 0 && !connecting && FD_ISSET(peer_fd, &rd)) read_peer();

    {
      ULONG now = amiga_now_ms();
      if (serving && !peer_ok && (LONG)(now - next_announce) >= 0) {
        announce();
        next_announce = now + 1000;
      }
      if (searching && (LONG)(now - search_end) >= 0) {
        searching = 0;
        close_fd(&udp_fd);
        event(NEV_SEARCH_DONE, 0, NULL, NULL, NULL);
      }
      if (connecting && (LONG)(now - connect_end) >= 0) {
        event(NEV_ERROR, 0, "No answer from the server.", NULL, NULL);
        drop_peer(NULL);
      }
      if (peer_ok && (LONG)(now - next_ping) >= 0) {
        UBYTE b[4];
        put_be32(b, now);
        frame_out(F_PING, b, 4);
        next_ping = now + 1000;
      }
    }
    flush_out();
    count_eclocks += amiga_eclock() - start;
  }

  if (peer_fd >= 0 && !connecting) bye("Disconnected.");
  stop_all();
  CloseLibrary(SocketBase);
  SocketBase = NULL;
  FreeSignal(wake);
  Forbid();
  net_task = NULL;
  Signal(parent_task, 1UL << parent_signal);
}

int net_start(const char *version)
{
  if (net_task != NULL) return 1;
  strncpy(net_version, version, sizeof(net_version) - 1);
  amiga_now_ms();  /* timer.device open before the process uses it */
  to_net.head = to_net.tail = 0;
  from_net.head = from_net.tail = 0;
  parent_task = FindTask(NULL);
  parent_signal = AllocSignal(-1);
  if (parent_signal < 0) return 0;
  start_failed = 0;
  quit_request = 0;
  SetSignal(0, 1UL << parent_signal);
  if (CreateNewProcTags(NP_Entry, (ULONG)net_main,
                        NP_Name, (ULONG)"Settlers LAN",
                        NP_Priority, 1,
                        NP_StackSize, 16384,
                        TAG_DONE) == NULL) {
    FreeSignal(parent_signal);
    parent_signal = -1;
    return 0;
  }
  Wait(1UL << parent_signal);
  if (start_failed) {
    FreeSignal(parent_signal);
    parent_signal = -1;
    return 0;
  }
  return 1;
}

void net_quit(void)
{
  if (net_task == NULL) return;
  quit_request = 1;
  Signal((struct Task *)net_task, SIGBREAKF_CTRL_C);
  Wait(1UL << parent_signal);
  FreeSignal(parent_signal);
  parent_signal = -1;
}
