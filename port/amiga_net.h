/*
 * amiga_net.h - The network of a LAN game on the Amiga, in C: the
 * functions of bsdsocket.library are macros that clash with C++ names
 * (connect, send).
 *
 * All the socket work happens in a process of its own ("Settlers LAN",
 * priority 1) that sleeps in WaitSelect: it costs nothing while nothing
 * comes, answers a packet at once and never calls the game. The game talks
 * to it through two rings in memory: commands go in with the net_*
 * calls below, events come out with net_event(). None of them waits.
 *
 * On the wire (TCP port 6810, TCP_NODELAY) a frame is
 *   length (1 byte: type + data) | type (1 byte) | data
 * HELLO, BYE, PING and PONG are handled by the process; the frames of the
 * game (types from NET_MSG_GAME) come out as NEV_MSG. A running server
 * announces itself on UDP port 6810 once a second:
 *   "SETTLERS-LAN <protocol> <version> <name>"
 */

#ifndef PORT_AMIGA_NET_H_
#define PORT_AMIGA_NET_H_

#ifdef __cplusplus
extern "C" {
#endif

#define NET_PORT 6810
#define NET_PROTOCOL 1
/* The first type of a frame of the game; smaller ones are the link's. */
#define NET_MSG_GAME 16
#define NET_MAX_DATA 250

typedef enum NetEventKind {
  NEV_NONE,
  NEV_ADDRESS,      /* text1: the address of this Amiga */
  NEV_ERROR,        /* text1: what failed; value 1: the server / search /
                       connection asked for is off, 0: a server goes on */
  NEV_SERVING,      /* RUN SERVER worked */
  NEV_SERVER,       /* found by a search: text1 name, text2 address,
                       text3 version */
  NEV_SEARCH_DONE,
  NEV_PEER,         /* the other player is there (HELLO): text1 name,
                       text2 address, text3 version */
  NEV_LOST,         /* the other player is gone: text1 why; a server
                       goes on waiting for another */
  NEV_PING,         /* value: the time of a PING there and back, ms */
  NEV_MSG           /* value: type, data / length: a frame of the game */
} NetEventKind;

typedef struct NetEvent {
  int kind;
  long value;
  char text1[32];
  char text2[32];
  char text3[16];
  unsigned char data[NET_MAX_DATA];
  int length;
} NetEvent;

/* Starts the process (bsdsocket.library is opened there). 0: no TCP/IP
   stack or no process. Calling it again does nothing. */
int net_start(const char *version);
/* Ends the process and closes everything. */
void net_quit(void);

/* Each replaces what the process did before: */
void net_serve(const char *name);   /* RUN SERVER */
void net_search(int ms);            /* listen for servers for ms */
void net_connect(const char *address, const char *name);
void net_stop(void);                /* back to nothing */

/* TCP_NODELAY on the connections made from now on (default on); off only
   to measure what Nagle's delay costs. */
void net_set_nodelay(int on);

/* A frame of the game to the other player. 0: no room (or too long). */
int net_send(int type, const void *data, int length);

/* The next event, 0 if none. */
int net_event(NetEvent *event);

/* For the statistics: ticks of the E-clock the process ran, frames and
   bytes sent and received since the start. */
void net_counters(unsigned long *eclocks, unsigned long *frames_out,
                  unsigned long *frames_in, unsigned long *bytes_out,
                  unsigned long *bytes_in);

#ifdef __cplusplus
}
#endif

#endif  /* PORT_AMIGA_NET_H_ */
