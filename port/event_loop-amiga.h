/*
 * event_loop-amiga.h - Amiga input, timers and the game loop
 */
#ifndef PORT_EVENT_LOOP_AMIGA_H_
#define PORT_EVENT_LOOP_AMIGA_H_

#include <functional>
#include <list>
#include <string>

#include "src/event_loop.h"

struct AmigaEvent;

class EventLoopAmiga : public EventLoop {
 public:
  /* STATS: frame rate and time split on the screen and in
     RAM:Settlers-stats.txt. */
  static bool show_stats;
  /* FPS (an icon Tool Type): switches on the interface's frame rate
     counter at the start (also in the Advanced box). */
  static bool show_fps;
  /* Called after a change of the screen mode (main gives the interface
     the screen frame again). */
  std::function<void()> on_mode_change;
  /* The map view magnified (main): 0 the next step, back to normal after
     the last (Z), +1 a step more (wheel up), -1 a step less (wheel
     down). */
  std::function<void(int)> on_zoom;
  /* More lines of the F10 report (main: where the map view is). */
  std::function<std::string()> state_info;

  virtual void draw_now();

 protected:
  /* The screen of run(), for draw_now(). */
  Frame *screen_frame = nullptr;
  std::list<DeferredCall> deferred_calls;
  bool running;
  unsigned int start_ms;
  int buttons_down[4];
  /* A left click was made while the right button was held: its release
     is no click. */
  bool right_used;
  int drag_button;
  int drag_x, drag_y;
  int last_x, last_y;
  bool dragged;
  /* How far the pointer went in the drag (|dx| + |dy|, pixels). */
  int drag_dist;
  unsigned int last_click[4];
  int last_click_x, last_click_y;
  /* Auto repeat of the + and - buttons: the left button held, when it
     repeats next (0: no more in this press), how many times it did. */
  unsigned int hold_next;
  int hold_count;
  /* The press of the left button was used (a hint): its release is no
     click, it closes the hint. */
  bool press_used;
  std::string stats_lines[4];
  std::string fps_text;
  std::string message_text;
  unsigned int message_until;
  /* Loading a game (load_progress): how far, -1 not loading; when the
     screen showed it last. */
  int load_percent;
  unsigned int load_shown;
  static void on_load_progress(int percent);
  /* A screen mode of the Options box waits for the player's yes: until
     when (0: none), the mode to go back to. */
  unsigned int confirm_until;
  unsigned int confirm_w, confirm_h;
  void confirm_box(int *x, int *y, int *w, int *h);
  void mode_confirm(bool keep);

 public:
  EventLoopAmiga();

  virtual void run();
  virtual void quit();
  virtual void deferred_call(DeferredCall call, void *data);
  virtual unsigned int get_ticks();

 protected:
  void handle_event(const AmigaEvent &ev);
  void handle_key(const AmigaEvent &ev);
  void check_hold();
  void draw_stats(class Frame *screen);
  void draw_message(class Frame *screen);
  void save_state();

 public:
  /* After F10 saved the state (main: the bug report box). */
  static std::function<void()> on_state_saved;
  /* Set by the Options box before it asks for another screen mode: the
     new mode is kept only when the player says yes (Return or a click on
     the small box) within kConfirmMs; else the old one comes back. A
     monitor may not show a mode - the old one stays saved. */
  static bool mode_asked;
  /* MODETEST=WxH,s (a test): s seconds after the start the mode is asked
     for as the Options box does (with the yes box). */
  static unsigned int test_mode_w, test_mode_h, test_mode_s;
  static const unsigned int kConfirmMs = 10000;
  bool mode_confirm_pending() const { return confirm_until != 0; }
  /* The player kept the new mode (main: it is saved). */
  std::function<void()> on_mode_confirmed;
};

#endif  // PORT_EVENT_LOOP_AMIGA_H_
