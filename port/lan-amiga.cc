/*
 * lan-amiga.cc - The network of a LAN game on the Amiga: LanLink over the
 * network process of amiga_net.c (which has the protocol).
 */

#include "port/lan-amiga.h"

#include <cstdlib>

#include "src/log.h"

#include "port/amiga_net.h"
#include "port/settlers_version.h"

/* How long SEARCH listens for the announcements (one a second). */
static const int kSearchMs = 3000;

LanLinkAmiga::LanLinkAmiga()
  : started(false)
  , state(StateIdle)
  , server(false)
  , ping_ms(-1)
  , external_asked(false)
  , external_seen(false)
  , relay(false) {
}

LanLinkAmiga::~LanLinkAmiga() {
  if (started) {
    net_quit();
  }
  net_external_quit();
  report_quit();
}

/* The process, at the first use. */
bool
LanLinkAmiga::start() {
  if (!started) {
    started = net_start(SETTLERS_VERSION) != 0;
    if (!started) {
      message = "No TCP/IP stack (bsdsocket).";
    }
  }
  return started;
}

void
LanLinkAmiga::forget_peer(bool keep_received) {
  peer_name.clear();
  peer_address.clear();
  ping_ms = -1;
  if (!keep_received) received.clear();
}

bool
LanLinkAmiga::poll() {
  bool changed = false;
  if (external_asked && !external_seen) {
    char text[2];
    if (net_external_address(text, sizeof(text))) {
      external_seen = true;
      changed = true;
    }
  }
  if (!started) {
    return changed;
  }
  NetEvent ev;
  while (net_event(&ev)) {
    changed = true;
    switch (ev.kind) {
      case NEV_ADDRESS:
        address = ev.text1;
        break;
      case NEV_ERROR:
        message = ev.text1;
        Log::Warn["lan"] << "Link error: " << ev.text1;
        /* What was asked for is off, unless a server only turned away a
           player (another version). */
        if (ev.value != 0) {
          state = StateIdle;
          server = false;
          relay = false;
          relay_code.clear();
        } else {
          state = StateServing;
        }
        forget_peer();
        break;
      case NEV_SERVING:
        break;
      case NEV_SERVER: {
        Server s = { ev.text1, ev.text2, ev.text3,
                     std::string(ev.text3) == SETTLERS_VERSION };
        bool known = false;
        for (Server &old : servers) {
          if (old.address == s.address) {
            old = s;
            known = true;
          }
        }
        if (!known) {
          servers.push_back(s);
        }
        break;
      }
      case NEV_RELAY_GAME: {
        Server s = { ev.text1, ev.text3, SETTLERS_VERSION, true,
                     ev.text2,
                     static_cast<unsigned int>(ev.value & 0x3fffffffL),
                     (ev.value & 0x40000000L) != 0 };
        servers.push_back(s);
        break;
      }
      case NEV_RELAY_CODE:
        relay_code = ev.text1;
        break;
      case NEV_SEARCH_DONE:
        if (state == StateSearching) {
          state = StateIdle;
        }
        break;
      case NEV_PEER:
        received.clear();  /* nothing of an earlier player */
        peer_name = ev.text1;
        peer_address = ev.text2;
        state = StateConnected;
        break;
      case NEV_LOST:
        message = ev.text1;
        Log::Warn["lan"] << "Link lost: " << ev.text1;
        /* What came before the end stays for the game: the QUIT of a
           player who left comes together with the close of the link
           (cleared, it looked like a lost link: "connection lost" and a
           LAN_ERR save after a player ended the game on purpose). */
        forget_peer(true);
        /* A game on the game server ends with its player: no waiting for
           another one there. */
        state = server && !relay ? StateServing : StateIdle;
        if (relay) {
          server = false;
          relay = false;
          relay_code.clear();
        }
        break;
      case NEV_PING:
        ping_ms = static_cast<int>(ev.value);
        break;
      case NEV_MSG:
        received.push_back(std::make_pair(
          static_cast<int>(ev.value),
          std::string(reinterpret_cast<char *>(ev.data), ev.length)));
        break;
      default:
        break;
    }
  }
  return changed;
}

std::string
LanLinkAmiga::get_address() {
  start();
  poll();
  return address;
}

bool
LanLinkAmiga::get_external_address(std::string *address) {
  char text[48];
  if (!external_asked) {
    external_asked = true;
    net_external_lookup();
  }
  if (!net_external_address(text, sizeof(text))) {
    return false;
  }
  *address = text;
  return true;
}

std::string
LanLinkAmiga::take_message() {
  std::string m = message;
  message.clear();
  return m;
}

void
LanLinkAmiga::start_server(const std::string &name, int port) {
  if (!start()) return;
  forget_peer();
  server = true;
  state = StateServing;
  net_serve(name.c_str(), port);
}

void
LanLinkAmiga::search(int port) {
  if (!start()) return;
  forget_peer();
  servers.clear();
  server = false;
  state = StateSearching;
  net_search(kSearchMs, port);
}

void
LanLinkAmiga::connect(const std::string &address, const std::string &name,
                      int port) {
  if (!start()) return;
  forget_peer();
  server = false;
  state = StateConnecting;
  net_connect(address.c_str(), name.c_str(), port);
}

void
LanLinkAmiga::relay_start(const std::string &server_address, int op,
                          const std::string &name, const std::string &arg) {
  std::string host = server_address;
  int port = kDefaultPort;
  size_t colon = host.rfind(':');
  if (colon != std::string::npos) {
    port = atoi(host.c_str() + colon + 1);
    host = host.substr(0, colon);
    if (port <= 0 || port > 65535) port = kDefaultPort;
  }
  net_relay(host.c_str(), port, op, name.c_str(), arg.c_str());
}

void
LanLinkAmiga::relay_list(const std::string &server_address) {
  if (!start()) return;
  forget_peer();
  servers.clear();
  server = false;
  relay = false;
  relay_code.clear();
  state = StateSearching;
  relay_start(server_address, 'L', "", "");
}

void
LanLinkAmiga::relay_create(const std::string &server_address,
                           const std::string &name, const std::string &game,
                           const std::string &password) {
  if (!start()) return;
  forget_peer();
  server = true;
  relay = true;
  relay_code.clear();
  state = StateServing;
  relay_start(server_address, 'C', name, game + "\t" + password);
}

void
LanLinkAmiga::relay_join(const std::string &server_address,
                         const std::string &name, const std::string &code,
                         const std::string &password) {
  if (!start()) return;
  forget_peer();
  server = false;
  relay = true;
  relay_code.clear();
  state = StateConnecting;
  relay_start(server_address, 'J', name, code + "\t" + password);
}

void
LanLinkAmiga::close() {
  if (started) {
    net_stop();
  }
  relay = false;
  relay_code.clear();
  forget_peer();
  server = false;
  state = StateIdle;
}

bool
LanLinkAmiga::send(int type, const std::string &data) {
  if (state != StateConnected) return false;
  return net_send(NET_MSG_GAME + type, data.data(),
                  static_cast<int>(data.size())) != 0;
}

bool
LanLinkAmiga::receive(int *type, std::string *data) {
  if (received.empty()) return false;
  *type = received.front().first - NET_MSG_GAME;
  *data = received.front().second;
  received.pop_front();
  return true;
}
