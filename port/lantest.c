/* lantest - the network of a LAN game from the Shell, through the same
   process as the game (amiga_net.c), printing what happens:
     lantest serve [name] [seconds]       a server (RUN SERVER)
     lantest search                       the servers in the network
     lantest connect <address> [name] [seconds] [NAGLE]
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

static void print_event(const NetEvent *ev, unsigned long ms)
{
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
    case NEV_MSG: printf("game frame %ld, %d bytes\n", ev->value,
                         ev->length); break;
    default: printf("event %d\n", ev->kind); break;
  }
}

int main(int argc, char **argv)
{
  const char *mode = argc > 1 ? argv[1] : "";
  int seconds = 0;
  unsigned long start, end, ecl, fo, fi, bo, bi;
  NetEvent ev;

  if (!net_start("lantest")) {
    printf("no bsdsocket.library or no process\n");
    return 20;
  }
  start = amiga_now_ms();
  if (strcmp(mode, "serve") == 0) {
    net_serve(argc > 2 ? argv[2] : "MASTER");
    seconds = argc > 3 ? atoi(argv[3]) : 60;
  } else if (strcmp(mode, "search") == 0) {
    net_search(3000);
    seconds = 4;
  } else if (strcmp(mode, "connect") == 0 && argc > 2) {
    if (argc > 5 && strcmp(argv[5], "NAGLE") == 0) net_set_nodelay(0);
    net_connect(argv[2], argc > 3 ? argv[3] : "SLAVE");
    seconds = argc > 4 ? atoi(argv[4]) : 20;
  } else {
    printf("lantest serve [name] [seconds] | search | "
           "connect <address> [name] [seconds] [NAGLE]\n");
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
  printf("network process: %lu us in %lu s; frames out %lu in %lu, "
         "bytes out %lu in %lu\n",
         ecl * 1000 / amiga_eclock_per_ms(),
         (amiga_now_ms() - start) / 1000, fo, fi, bo, bi);
  net_stop();
  net_quit();
  amiga_time_close();
  return 0;
}
