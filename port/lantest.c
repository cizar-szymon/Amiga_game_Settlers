/* lantest - the network of a LAN game from the Shell, through the same
   process as the game (amiga_net.c), printing what happens:
     lantest serve [name] [seconds]       a server (RUN SERVER)
     lantest search                       the servers in the network
     lantest connect <address> [name] [seconds] [NAGLE]
     lantest send <address> <file>        connect, then send the file as a
                                          saved game is sent (frames of 240
                                          bytes, waiting while the ring is
                                          full) and print the time
     lantest extip                        the internet address (the HTTP
                                          question of the server view)
   PORT=n as the first argument: another port than 6810.
   Ctrl-C ends it. At the end: the time the network process ran and what
   went through. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "amiga_net.h"
#include "amiga_time.h"

/* The game frames of a saved game (LanGame::MessageSave*). */
#define SAVE_BEGIN (NET_MSG_GAME + 4)
#define SAVE_DATA (NET_MSG_GAME + 5)
#define SAVE_END (NET_MSG_GAME + 6)

static long save_bytes;
static unsigned long save_hash;

static void print_event(const NetEvent *ev, unsigned long ms)
{
  if (ev->kind == NEV_MSG && ev->value == SAVE_DATA) {
    int i;
    for (i = 0; i < ev->length; i++) {
      save_hash ^= ev->data[i];
      save_hash *= 16777619UL;
    }
    save_bytes += ev->length;
    return;
  }
  printf("%6lu ms  ", ms);
  switch (ev->kind) {
    case NEV_ADDRESS: printf("address %s\n", ev->text1); break;
    case NEV_ERROR: printf("error %s (%s)\n", ev->text1,
                           ev->value ? "off" : "server goes on"); break;
    case NEV_SERVING: printf("serving\n"); break;
    case NEV_SERVER: printf("server %s at %s, version %s\n", ev->text1,
                            ev->text2, ev->text3); break;
    case NEV_SEARCH_DONE: printf("search done\n"); break;
    case NEV_PEER: printf("player %s at %s, version %s\n", ev->text1,
                          ev->text2, ev->text3); break;
    case NEV_LOST: printf("lost: %s\n", ev->text1); break;
    case NEV_PING: printf("ping %ld ms\n", ev->value); break;
    case NEV_MSG:
      /* A saved game coming: counted, not printed frame by frame. */
      if (ev->value == SAVE_BEGIN) {
        save_bytes = 0;
        save_hash = 2166136261UL;
        printf("saved game begins: %.*s\n", ev->length,
               (const char *)ev->data);
      } else if (ev->value == SAVE_END) {
        printf("saved game ends: %ld bytes, checksum %lu\n", save_bytes,
               save_hash);
      } else {
        printf("game frame %ld, %d bytes\n", ev->value, ev->length);
      }
      break;
    default: printf("event %d\n", ev->kind); break;
  }
}

/* The file to the other side once it is there. */
static int send_file(const char *path, unsigned long start)
{
  NetEvent ev;
  FILE *f = fopen(path, "rb");
  static char buf[1 << 20];
  long size, sent = 0;
  unsigned long hash = 2166136261UL, t0, waits = 0;
  char text[64];
  int peer = 0;
  long i;
  if (f == NULL) {
    printf("no file %s\n", path);
    return 10;
  }
  size = (long)fread(buf, 1, sizeof(buf), f);
  fclose(f);
  for (i = 0; i < size; i++) {
    hash ^= (unsigned char)buf[i];
    hash *= 16777619UL;
  }
  /* The other side first. */
  while (!peer && (long)(amiga_now_ms() - start) < 10000) {
    while (net_event(&ev)) {
      print_event(&ev, amiga_now_ms() - start);
      if (ev.kind == NEV_PEER) peer = 1;
    }
    Delay(1);
  }
  if (!peer) {
    printf("no other side\n");
    return 10;
  }
  t0 = amiga_now_ms();
  sprintf(text, "%ld %lu 2", size, hash);
  while (!net_send(SAVE_BEGIN, text, (int)strlen(text))) Delay(1);
  while (sent < size) {
    int n = size - sent > 240 ? 240 : (int)(size - sent);
    if (!net_send(SAVE_DATA, buf + sent, n)) {
      waits++;
      Delay(1);
      continue;
    }
    sent += n;
  }
  while (!net_send(SAVE_END, "", 0)) Delay(1);
  printf("%ld bytes (checksum %lu) in %lu ms, %lu waits for the ring\n",
         size, hash, amiga_now_ms() - t0, waits);
  return 0;
}

int main(int argc, char **argv)
{
  const char *mode;
  int seconds = 0, rc = 0, port = NET_PORT;
  unsigned long start, end, ecl, fo, fi, bo, bi;
  NetEvent ev;

  if (argc > 1 && strncmp(argv[1], "PORT=", 5) == 0) {
    port = atoi(argv[1] + 5);
    if (port < 1 || port > 65535) port = NET_PORT;
    argv++;
    argc--;
  }
  mode = argc > 1 ? argv[1] : "";
  if (strcmp(mode, "extip") == 0) {
    char text[48];
    unsigned long t0 = amiga_now_ms();
    net_external_lookup();
    while (!net_external_address(text, sizeof(text))) {
      if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
      Delay(1);
    }
    printf("internet address: \"%s\" after %lu ms\n", text,
           amiga_now_ms() - t0);
    net_external_quit();
    amiga_time_close();
    return 0;
  }
  printf("port %d\n", port);
  if (!net_start("lantest")) {
    printf("no bsdsocket.library or no process\n");
    return 20;
  }
  start = amiga_now_ms();
  if (strcmp(mode, "serve") == 0) {
    net_serve(argc > 2 ? argv[2] : "MASTER", port);
    seconds = argc > 3 ? atoi(argv[3]) : 60;
  } else if (strcmp(mode, "search") == 0) {
    net_search(3000, port);
    seconds = 4;
  } else if (strcmp(mode, "connect") == 0 && argc > 2) {
    if (argc > 5 && strcmp(argv[5], "NAGLE") == 0) net_set_nodelay(0);
    net_connect(argv[2], argc > 3 ? argv[3] : "SLAVE", port);
    seconds = argc > 4 ? atoi(argv[4]) : 20;
  } else if (strcmp(mode, "send") == 0 && argc > 3) {
    net_connect(argv[2], "SENDER", port);
    rc = send_file(argv[3], start);
    seconds = 3;  /* the rest goes out */
    start = amiga_now_ms();
  } else {
    printf("lantest serve [name] [seconds] | search | "
           "connect <address> [name] [seconds] [NAGLE] | "
           "send <address> <file> | extip\n"
           "PORT=n first: another port\n");
    net_quit();
    return 5;
  }

  end = start + (unsigned long)seconds * 1000;
  while ((long)(amiga_now_ms() - end) < 0) {
    if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
    while (net_event(&ev)) print_event(&ev, amiga_now_ms() - start);
    Delay(1);
  }
  while (net_event(&ev)) print_event(&ev, amiga_now_ms() - start);

  net_counters(&ecl, &fo, &fi, &bo, &bi);
  printf("network process: %lu us; frames out %lu in %lu, "
         "bytes out %lu in %lu\n",
         ecl * 1000 / amiga_eclock_per_ms(), fo, fi, bo, bi);
  net_stop();
  net_quit();
  amiga_time_close();
  return rc;
}
