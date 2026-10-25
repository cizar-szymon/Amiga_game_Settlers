/*
 * event_loop-amiga.cc - Amiga input, timers and the game loop
 *
 * Follows event_loop-sdl.cc: the game advances in steps of TICK_LENGTH ms
 * (50 Hz), missed steps are made up before the next drawing (up to 5),
 * and between the steps the loop sleeps until input or the next step.
 */

#include "port/event_loop-amiga.h"

#include <cstdio>
#include <cstring>
#include <list>

#include "src/freeserf.h"
#include "src/gfx.h"
#include "src/game.h"
#include "src/game-manager.h"
#include "src/savegame.h"
#include "port/settlers_version.h"
#include "port/amiga_screen.h"
#include "port/video-amiga.h"
#include "port/amiga_audio.h"

bool EventLoopAmiga::show_stats = false;
bool EventLoopAmiga::show_fps = false;

/* Called once per frame when set: the diagnostic build Settlers-hg sets it
   to the heap check of amiga_heapguard.c (amiga_heapguard_new.cpp). */
void (*settlers_frame_check)(const char *where) = nullptr;

/* How fast consecutive clicks make a double click, and how far the mouse
   may move between them (as in event_loop-sdl.cc). */
#define MOUSE_TIME_SENSITIVITY  600
#define MOUSE_MOVE_SENSITIVITY  8

EventLoop &
EventLoop::get_instance() {
  static EventLoopAmiga event_loop;
  return event_loop;
}

/* Timers are checked by the loop itself, between the steps. */
class TimerAmiga : public Timer {
 public:
  static std::list<TimerAmiga *> active;
  unsigned int next;

  TimerAmiga(unsigned int _id, unsigned int _interval,
             Timer::Handler *_handler)
    : Timer(_id, _interval, _handler), next(0) {}
  virtual ~TimerAmiga() { stop(); }

  virtual void run() {
    stop();
    next = EventLoop::get_instance().get_ticks() + interval;
    active.push_back(this);
  }
  virtual void stop() { active.remove(this); }

  static void fire_due(unsigned int now) {
    std::list<TimerAmiga *> due;
    for (TimerAmiga *t : active) {
      if (static_cast<int>(now - t->next) >= 0) due.push_back(t);
    }
    for (TimerAmiga *t : due) {
      t->next += t->interval;
      if (static_cast<int>(now - t->next) >= 0) t->next = now + t->interval;
      if (t->handler != nullptr) t->handler->on_timer_fired(t->id);
    }
  }
};

std::list<TimerAmiga *> TimerAmiga::active;

Timer *
Timer::create(unsigned int _id, unsigned int _interval,
              Timer::Handler *_handler) {
  return new TimerAmiga(_id, _interval, _handler);
}

EventLoopAmiga::EventLoopAmiga()
  : running(false)
  , drag_button(0)
  , drag_x(0)
  , drag_y(0)
  , last_x(0)
  , last_y(0)
  , dragged(false)
  , last_click_x(0)
  , last_click_y(0)
  , message_until(0) {
  start_ms = static_cast<unsigned int>(amiga_now_ms());
  for (int i = 0; i < 4; i++) {
    buttons_down[i] = 0;
    last_click[i] = 0;
  }
}

unsigned int
EventLoopAmiga::get_ticks() {
  return static_cast<unsigned int>(amiga_now_ms()) - start_ms;
}

void
EventLoopAmiga::quit() {
  running = false;
}

void
EventLoopAmiga::deferred_call(DeferredCall call, void *data) {
  deferred_calls.push_back(call);
}

void
EventLoopAmiga::handle_key(const AmigaEvent &ev) {
  unsigned char modifier = 0;
  if (ev.qual & AQUAL_CTRL) modifier |= 1;
  if (ev.qual & AQUAL_SHIFT) modifier |= 2;
  if (ev.qual & AQUAL_ALT) modifier |= 4;

  switch (ev.code) {
    /* Map scroll */
    case ARAW_UP:    notify_drag(0, 0, 0, -32, Event::ButtonLeft); return;
    case ARAW_DOWN:  notify_drag(0, 0, 0, 32, Event::ButtonLeft); return;
    case ARAW_LEFT:  notify_drag(0, 0, -32, 0, Event::ButtonLeft); return;
    case ARAW_RIGHT: notify_drag(0, 0, 32, 0, Event::ButtonLeft); return;
    case ARAW_ESC:
      /* Shift+Esc ends the program; Esc alone closes boxes in the game. */
      if (modifier & 2) {
        quit();
      } else {
        notify_key_pressed(27, 0);
      }
      return;
    case ARAW_TAB:
      notify_key_pressed('\t', modifier);
      return;
    case ARAW_F10:
      save_state();
      return;
    case ARAW_F9:
      static_cast<VideoAmiga &>(Video::get_instance())
        .save_screenshot("RAM:Settlers-shot.ppm");
      return;
    default:
      break;
  }

  if (ev.ascii == 'q' && (modifier & 1)) {
    quit();
    return;
  }
  if (ev.ascii != 0) {
    notify_key_pressed(static_cast<unsigned char>(ev.ascii), modifier);
  }
}

void
EventLoopAmiga::handle_event(const AmigaEvent &ev) {
  if (ev.type == AEV_MOVE || ev.type == AEV_BUTTON) {
    static_cast<VideoAmiga &>(Video::get_instance()).set_mouse(ev.x, ev.y);
  }
  switch (ev.type) {
    case AEV_BUTTON: {
      int b = ev.button;
      if (ev.down) {
        buttons_down[b] = 1;
        last_x = ev.x;
        last_y = ev.y;
        break;
      }
      buttons_down[b] = 0;
      /* After a drag SDL has put the pointer back where the drag began,
         so the click of the release is there too. */
      int x = ev.x;
      int y = ev.y;
      if (drag_button == b) {
        if (dragged) {
          x = drag_x;
          y = drag_y;
        }
        drag_button = 0;
        dragged = false;
      }
      notify_click(x, y, static_cast<Event::Button>(b));

      unsigned int now = get_ticks();
      if (now - last_click[b] < MOUSE_TIME_SENSITIVITY &&
          x >= last_click_x - MOUSE_MOVE_SENSITIVITY &&
          x <= last_click_x + MOUSE_MOVE_SENSITIVITY &&
          y >= last_click_y - MOUSE_MOVE_SENSITIVITY &&
          y <= last_click_y + MOUSE_MOVE_SENSITIVITY) {
        notify_dbl_click(x, y, static_cast<Event::Button>(b));
      }
      last_click[b] = now;
      last_click_x = x;
      last_click_y = y;
      break;
    }
    case AEV_MOVE: {
      for (int b = 1; b <= 3; b++) {
        if (!buttons_down[b]) continue;
        if (drag_button == 0) {
          drag_button = b;
          drag_x = last_x;
          drag_y = last_y;
          dragged = false;
        }
        if (drag_button != b) break;
        int dx = ev.x - last_x;
        int dy = ev.y - last_y;
        if (dx != 0 || dy != 0) {
          notify_drag(drag_x, drag_y, dx, dy, static_cast<Event::Button>(b));
          dragged = true;
        }
        break;
      }
      last_x = ev.x;
      last_y = ev.y;
      break;
    }
    case AEV_KEY:
      if (ev.down) handle_key(ev);
      break;
    case AEV_CLOSE:
      quit();
      break;
    default:
      break;
  }
}

/* F10: the whole game state for bug reports (the standard of the ports):
   a Freeserf save (text, the same on every machine; the PC version loads
   it with -l) and a short description. */
void
EventLoopAmiga::save_state() {
  PGame game = GameManager::get_instance().get_current_game();
  if (!game) return;
  bool ok = GameStore::get_instance().save("RAM:Settlers-state.save",
                                           game.get());
  FILE *f = fopen("RAM:Settlers-state.txt", "w");
  if (f != nullptr) {
    unsigned int w = 0, h = 0;
    Video::get_instance().get_resolution(&w, &h);
    fprintf(f, "Settlers %s (%s)\n", SETTLERS_VERSION, SETTLERS_DATE);
    fprintf(f, "game tick %u%s, screen %ux%u, %s\n", game->get_tick(),
            game->is_paused() ? " (paused)" : "", w, h,
            VideoAmiga::render_vram ? "RENDER=VRAM" : "RENDER=FAST");
    fprintf(f, "save: RAM:Settlers-state.save (%s)\n", ok ? "ok" : "FAILED");
    fclose(f);
  }
  message_text = ok ? "STATE SAVED: RAM:SETTLERS-STATE.SAVE"
                    : "STATE NOT SAVED";
  message_until = get_ticks() + 2000;
}

/* A short line in the top middle of the screen (F10). */
void
EventLoopAmiga::draw_message(Frame *screen) {
  if (message_text.empty()) return;
  if (static_cast<int>(get_ticks() - message_until) >= 0) {
    message_text.clear();
    return;
  }
  unsigned int w = 0, h = 0;
  Video::get_instance().get_resolution(&w, &h);
  int tw = 8 * static_cast<int>(message_text.size());
  int x = (static_cast<int>(w) - tw) / 2;
  screen->fill_rect(x - 2, 20, tw + 4, 11, Color::black);
  screen->draw_string(x, 22, message_text, Color::green, Color::transparent);
}

/* STATS: the frame rate of the last second in the top left corner. The
   other numbers only go to RAM:Settlers-stats.txt. */
void
EventLoopAmiga::draw_stats(Frame *screen) {
  if (fps_text.empty()) return;
  screen->fill_rect(0, 0, 8 * static_cast<int>(fps_text.size()) + 4, 11,
                    Color::black);
  screen->draw_string(2, 2, fps_text, Color::green, Color::transparent);
}

void
EventLoopAmiga::run() {
  static const int kMaxCatchUpSteps = 5;

  Graphics &gfx = Graphics::get_instance();
  VideoAmiga &video = static_cast<VideoAmiga &>(Video::get_instance());
  Frame *screen = gfx.get_screen_frame();

  unsigned int next_step = get_ticks() + TICK_LENGTH;
  unsigned int stats_start = get_ticks();
  unsigned int frames = 0, steps_done = 0;
  unsigned int logic_ms = 0, draw_ms = 0;
  video.blit_ms = 0;

  running = true;
  while (running) {
    unsigned int now = get_ticks();
    if (static_cast<int>(next_step - now) > 0) {
      if (amiga_wait(next_step - now)) {
        break;  /* Ctrl-C */
      }
    } else if (amiga_break()) {
      break;
    }

    AmigaEvent ev;
    while (running && amiga_get_event(&ev)) {
      handle_event(ev);
    }
    while (!deferred_calls.empty()) {
      deferred_calls.front()(nullptr);
      deferred_calls.pop_front();
    }
    if (!running) break;

    if (video.mode_pending()) {
      /* Between frames: a new screen, then the GUI laid out again. */
      video.apply_mode();
      unsigned int w = 0, h = 0;
      video.get_resolution(&w, &h);
      notify_resize(w, h);
      if (on_mode_change) on_mode_change();
      next_step = get_ticks() + TICK_LENGTH;
    }

    now = get_ticks();
    TimerAmiga::fire_due(now);
    if (static_cast<int>(next_step - now) > 0) {
      continue;
    }

    int steps = 0;
    unsigned int t0 = get_ticks();
    while (static_cast<int>(now - next_step) >= 0 &&
           steps < kMaxCatchUpSteps) {
      notify_update();
      next_step += TICK_LENGTH;
      steps++;
    }
    if (static_cast<int>(now - next_step) >= 0) {
      next_step = now + TICK_LENGTH;
    }
    unsigned int t1 = get_ticks();
    notify_draw(screen);
    unsigned int t2 = get_ticks();
    if (show_stats) draw_stats(screen);
    draw_message(screen);
    if (settlers_frame_check != nullptr) settlers_frame_check("frame");
    gfx.swap_buffers();

    logic_ms += t1 - t0;
    draw_ms += t2 - t1;
    steps_done += steps;
    frames++;

    unsigned int span = get_ticks() - stats_start;
    if (show_stats && span >= 1000) {
      /* The font of the game has capitals, digits, '.', ':' and spaces. */
      char line[256];
      snprintf(line, sizeof(line),
               "FPS %u.%u  LOGIC %u  DRAW %u  BLIT %u  STEPS %u  COL %u%s",
               frames * 1000 / span, (frames * 10000 / span) % 10,
               logic_ms * 1000 / span, draw_ms * 1000 / span,
               video.blit_ms * 1000 / span, steps_done * 1000 / span,
               video.get_colors_used(), VideoAmiga::verify ? "  VERIFY" : "");
      if (VideoAmiga::verify) {
        size_t n = strlen(line);
        snprintf(line + n, sizeof(line) - n, " %lu",
                 (unsigned long)video.verify_errors);
      }
      stats_lines[0] = line;
      stats_lines[1].clear();
      snprintf(line, sizeof(line), "FPS %u.%u", frames * 1000 / span,
               (frames * 10000 / span) % 10);
      fps_text = line;
      if (VideoAmiga::profile) {
        /* Split of the drawing, ms:calls per second, for frame copies,
           sprites, fills and scaled copies. */
        uint32_t per = static_cast<uint32_t>(amiga_eclock_per_ms());
        snprintf(line, sizeof(line),
                 "FRAME %lu:%lu %luK OPAQUE %lu:%luK  IMAGE %lu:%lu %luK"
                 "  FILL %lu:%lu  OTHER %lu  LOCK %lu:%lu",
                 (unsigned long)(video.prof_ticks[0] / per * 1000 / span),
                 (unsigned long)(video.prof_calls[0] * 1000 / span),
                 (unsigned long)(video.prof_pixels[0] / span),
                 (unsigned long)(video.prof_opaque_calls * 1000 / span),
                 (unsigned long)(video.prof_opaque_pixels / span),
                 (unsigned long)(video.prof_ticks[1] / per * 1000 / span),
                 (unsigned long)(video.prof_calls[1] * 1000 / span),
                 (unsigned long)(video.prof_pixels[1] / span),
                 (unsigned long)(video.prof_ticks[2] / per * 1000 / span),
                 (unsigned long)(video.prof_calls[2] * 1000 / span),
                 (unsigned long)(video.prof_ticks[3] / per * 1000 / span),
                 (unsigned long)(video.prof_lock_ticks / per * 1000 / span),
                 (unsigned long)(video.prof_locks * 1000 / span));
        stats_lines[1] = line;
        snprintf(line, sizeof(line),
                 "SPRITES %lu:%lu  RUNS %luK  COPY %luK  SHADE %luK  BOX %luK",
                 (unsigned long)(video.prof_img_runs_path * 1000 / span),
                 (unsigned long)(video.prof_img_clip_path * 1000 / span),
                 (unsigned long)(video.prof_runs / span),
                 (unsigned long)(video.prof_copy_px / span),
                 (unsigned long)(video.prof_shade_px / span),
                 (unsigned long)(video.prof_box_px / span));
        stats_lines[2] = line;
        video.prof_reset();
      }
      /* STATS, for AmiAgent: the same lines in a file, with the game tick
         to see that the game runs. FPS alone writes nothing. */
      FILE *f = fopen("RAM:Settlers-stats.tmp", "w");
      if (f != nullptr) {
        PGame game = GameManager::get_instance().get_current_game();
        fprintf(f, "%s\n%s\n%s\nGAME TICK %u%s\n", stats_lines[0].c_str(),
                stats_lines[1].c_str(), stats_lines[2].c_str(),
                game ? game->get_tick() : 0,
                (game && game->is_paused()) ? " PAUSED" : "");
        unsigned long under = 0, bufs = 0, ticks = 0;
        int period = 0, pal = 0;
        amiga_audio_stats(&under, &bufs, &ticks, &period, &pal);
        fprintf(f, "AUDIO period %d %s rate %d  buffers %lu  gaps %lu  "
                "render %lu us/buffer\n", period, pal ? "PAL" : "NTSC",
                amiga_audio_rate(), bufs, under,
                bufs ? ticks / bufs * 1000 /
                       static_cast<unsigned long>(amiga_eclock_per_ms())
                     : 0UL);
        fclose(f);
        /* Replaced by a rename, so that a reader never finds it open. */
        std::remove("RAM:Settlers-stats.txt");
        std::rename("RAM:Settlers-stats.tmp", "RAM:Settlers-stats.txt");
      }
      stats_start = get_ticks();
      frames = steps_done = logic_ms = draw_ms = 0;
      video.blit_ms = 0;
    }
  }

  delete screen;
}
