/*
 * lan-amiga.h - The network of a LAN game on the Amiga: LanLink over the
 * network process of amiga_net.c.
 */

#ifndef PORT_LAN_AMIGA_H_
#define PORT_LAN_AMIGA_H_

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "src/lan-link.h"

class LanLinkAmiga : public LanLink {
 protected:
  bool started;
  State state;
  bool server;
  std::string address;
  std::string message;
  std::vector<Server> servers;
  std::string peer_name;
  std::string peer_address;
  int ping_ms;
  /* The internet address asked for, its answer seen by poll(). */
  bool external_asked;
  bool external_seen;
  std::deque<std::pair<int, std::string>> received;
  /* Through the game server, and the code of the game made there. */
  bool relay;
  std::string relay_code;

 public:
  LanLinkAmiga();
  virtual ~LanLinkAmiga();

  virtual bool poll();
  virtual State get_state() { return state; }
  virtual bool is_server() { return server; }
  virtual std::string get_address();
  virtual std::string take_message();

  virtual bool get_external_address(std::string *address);

  virtual void start_server(const std::string &name, int port);
  virtual void search(int port);
  virtual std::vector<Server> get_servers() { return servers; }
  virtual void connect(const std::string &address, const std::string &name,
                       int port);
  virtual void close();

  virtual std::string get_peer_name() { return peer_name; }
  virtual std::string get_peer_address() { return peer_address; }
  virtual int get_ping_ms() { return ping_ms; }

  virtual void relay_list(const std::string &server);
  virtual void relay_create(const std::string &server,
                            const std::string &name,
                            const std::string &game,
                            const std::string &password);
  virtual void relay_join(const std::string &server, const std::string &name,
                          const std::string &code,
                          const std::string &password);
  virtual std::string get_relay_code() { return relay_code; }
  virtual bool is_relay() { return relay; }

  virtual bool send(int type, const std::string &data);
  virtual bool receive(int *type, std::string *data);

 protected:
  bool start();
  /* "host:port" to the net process (net_relay). */
  void relay_start(const std::string &server, int op, const std::string &name,
                   const std::string &arg);
  /* keep_received: the frames already here stay for the game (the end of
     the link: a QUIT came before it). */
  void forget_peer(bool keep_received = false);
};

#endif  // PORT_LAN_AMIGA_H_
