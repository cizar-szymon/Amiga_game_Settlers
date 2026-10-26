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
  std::deque<std::pair<int, std::string>> received;

 public:
  LanLinkAmiga();
  virtual ~LanLinkAmiga();

  virtual bool poll();
  virtual State get_state() { return state; }
  virtual bool is_server() { return server; }
  virtual std::string get_address();
  virtual std::string take_message();

  virtual void start_server(const std::string &name);
  virtual void search();
  virtual std::vector<Server> get_servers() { return servers; }
  virtual void connect(const std::string &address, const std::string &name);
  virtual void close();

  virtual std::string get_peer_name() { return peer_name; }
  virtual std::string get_peer_address() { return peer_address; }
  virtual int get_ping_ms() { return ping_ms; }

  virtual bool send(int type, const std::string &data);
  virtual bool receive(int *type, std::string *data);

 protected:
  bool start();
  void forget_peer();
};

#endif  // PORT_LAN_AMIGA_H_
