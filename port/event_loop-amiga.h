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

 protected:
  std::list<DeferredCall> deferred_calls;
  bool running;
  unsigned int start_ms;
  int buttons_down[4];
  int drag_button;
  int drag_x, drag_y;
  int last_x, last_y;
  bool dragged;
  /* How far the pointer went in the drag (|dx| + |dy|, pixels). */
  int drag_dist;
  unsigned int last_click[4];
  int last_click_x, last_click_y;
  std::string stats_lines[4];
  std::string fps_text;
  std::string message_text;
  unsigned int message_until;

 public:
  EventLoopAmiga();

  virtual void run();
  virtual void quit();
  virtual void deferred_call(DeferredCall call, void *data);
  virtual unsigned int get_ticks();

 protected:
  void handle_event(const AmigaEvent &ev);
  void handle_key(const AmigaEvent &ev);
  void draw_stats(class Frame *screen);
  void draw_message(class Frame *screen);
  void save_state();
};

#endif  // PORT_EVENT_LOOP_AMIGA_H_
