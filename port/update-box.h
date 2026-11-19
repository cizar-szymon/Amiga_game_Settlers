/* Update box of the Amiga port (the UPDATE button of the Options box):
   this version and the newest one on the update server (the data server,
   not the game server) and what changed (amiga_update.c). The game only
   checks; the update itself is the work of the WARP program on the
   Workbench. */
#ifndef PORT_UPDATE_BOX_H_
#define PORT_UPDATE_BOX_H_

#include <string>

#include "src/gui.h"

#include "port/amiga_net.h"

class Interface;

class UpdateBox : public GuiObject {
 public:
  static const int kWidth = 288;
  static const int kHeight = 216;
  /* The update server "host:port" (Settlers.ini [update] server, or the
     option UPDATESERVER), set by main. */
  static std::string server;
  /* UPDATEBOX (a test): the box opens over the start box a few seconds
     after the start. */
  static Interface *open_later;

 protected:
  Interface *interface;
  UpdateInfo info;
  /* The state the box last drew (it draws again when it changed). */
  int shown_state;

  static UpdateBox *open_box;

 public:
  explicit UpdateBox(Interface *interface);
  virtual ~UpdateBox();

  /* Every frame (the event loop): the state of the check. */
  static void tick_open();

 protected:
  void tick();
  void check();
  bool newer() const;
  void draw_button(int bx, int by, int bw, const std::string &text,
                   bool enabled);
  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // PORT_UPDATE_BOX_H_
