/*
 * lan-amiga.cc - The network of a LAN game on the Amiga: LanLink over the
 * network process of amiga_net.c (which has the protocol).
 */

#include "port/lan-amiga.h"

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
  , external_seen(false) {
}

LanLinkAmiga::~LanLinkAmiga() {
  if (started) {
    net_quit();
  }
  net_external_quit();
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
LanLinkAmiga::forget_peer() {
  peer_name.clear();
  peer_address.clear();
  ping_ms = -1;
  received.clear();
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
        /* What was asked for is off, unless a server only turned away a
           player (another version). */
        if (ev.value != 0) {
          state = StateIdle;
          server = false;
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
      case NEV_SEARCH_DONE:
        if (state == StateSearching) {
          state = StateIdle;
        }
        break;
      case NEV_PEER:
        peer_name = ev.text1;
        peer_address = ev.text2;
        state = StateConnected;
        break;
      case NEV_LOST:
        message = ev.text1;
        forget_peer();
        state = server ? StateServing : StateIdle;
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
LanLinkAmiga::close() {
  if (started) {
    net_stop();
  }
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
