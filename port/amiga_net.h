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
 * On the wire (TCP, port 6810 unless the players choose another one,
 * TCP_NODELAY) a frame is
 *   length (1 byte: type + data) | type (1 byte) | data
 * HELLO, BYE, PING and PONG are handled by the process; the frames of the
 * game (types from NET_MSG_GAME) come out as NEV_MSG. A running server
 * announces itself on UDP (the same port) once a second:
 *   "SETTLERS-LAN <protocol> <version> <name>"
 * A search listens on its port, so it finds only servers on the same one.
 *
 * Through the game server (net_relay) the connection goes to it, and
 * before HELLO the two talk in RELAY frames (type 5, text): LIST / CREATE /
 * JOIN from here, SESSION / END / CREATED / PAIRED / ERR from it; after
 * PAIRED the server passes the frames of the two players unchanged.
 *
 * The internet address of this side (for a game over the internet: the
 * other player types it in) comes from a process of its own, once a run:
 * an HTTP GET to api.ipify.org (amiga_extip.c).
 */

#ifndef PORT_AMIGA_NET_H_
#define PORT_AMIGA_NET_H_

#ifdef __cplusplus
extern "C" {
#endif

/* The default port, TCP and UDP. */
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
  NEV_MSG,          /* value: type, data / length: a frame of the game */
  NEV_RELAY_GAME,   /* a game waiting on the game server (net_relay list):
                       text1 its name, text2 its player, text3 its code,
                       value seconds waiting (bit 30: a password is
                       needed); the end: NEV_SEARCH_DONE */
  NEV_RELAY_CODE    /* the game made on the game server: text1 its code */
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

/* Each replaces what the process did before (port: 1..65535): */
void net_serve(const char *name, int port);   /* RUN SERVER */
void net_search(int ms, int port);            /* listen for servers for ms */
void net_connect(const char *address, const char *name, int port);
void net_stop(void);                /* back to nothing */
/* A game over the internet through the game server (server/ of the repo)
   at host (a name or an address) and port; op 'L': the games waiting
   there (NEV_RELAY_GAME...), 'C': make one, arg its name, a tab and a
   password or nothing (NEV_RELAY_CODE, then NEV_PEER when a player comes),
   'J': join one, arg its code, a tab and the password (NEV_PEER). After
   NEV_PEER all goes as with net_connect. */
void net_relay(const char *host, int port, int op, const char *name,
               const char *arg);

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

/* The internet address of this side (amiga_extip.c). The first call of
   net_external_lookup starts the question, later ones do nothing.
   net_external_address: 0 while it is not asked or goes on; 1 when done,
   text is then the address or "" (no internet, no answer in time, not an
   address). Neither waits. net_external_quit (at the end of the program)
   waits for the process if it still runs. */
void net_external_lookup(void);
int net_external_address(char *text, int size);
void net_external_quit(void);

/* A bug report to the game server (amiga_report.c): a process sends the
   fields and up to 4 files (missing ones are left out). report_send: 0 if
   one is still being sent. report_result: 0 while it goes on; 1 when done,
   *ok 1 and text the number of the report, or *ok 0 and text why not.
   report_quit at the end of the program. */
int report_send(const char *host, int port, const char *version,
                const char *kind, const char *email, const char *text,
                const char *machine, const char *const *files, int nfiles);
int report_result(char *text, int size, int *ok);
void report_quit(void);
/* 1: a TCP/IP stack is running (its bsdsocket.library in memory) - a
   report can go; else F10 only saves the state. */
int report_network(void);

#ifdef __cplusplus
}
#endif

#endif  /* PORT_AMIGA_NET_H_ */
