/*
 * main-amiga.cc - main program of the Amiga port (replaces src/freeserf.cc)
 *
 * Options, in the Shell or as Tool Types of the icon:
 *   MODE=400x300|640x400|640x480|800x600|WxH  DATA=path  LOAD=savefile  NOSOUND  FPS
 *   STATS[=FULL]  VERIFY  RENDER=VRAM|FAST  LOCK=FRAME|CALL  BLIT=0|1|2
 *   DEBUG=0..4
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <fstream>
#include <string>

#include "src/freeserf.h"
#include "src/log.h"
#include "src/data.h"
#include "src/audio.h"
#include "src/gfx.h"
#include "src/interface.h"
#include "src/settings.h"
#include "src/game-manager.h"
#include "src/translation.h"
#include "port/event_loop-amiga.h"
#include "port/video-amiga.h"
#include "port/settlers_version.h"
#include "port/amiga_screen.h"
#include "port/audio-amiga.h"
#include "port/amiga_wb.h"

extern "C" const char ver[];
const char ver[] = "$VER: Settlers " SETTLERS_VERSION " (" SETTLERS_DATE ")";

/* Value of KEY=value (key case-insensitive), or nullptr. */
static const char *
option_value(const char *arg, const char *key) {
  size_t n = strlen(key);
  if (strncasecmp(arg, key, n) == 0 && arg[n] == '=') return arg + n + 1;
  return nullptr;
}

static std::list<std::string> data_dirs;
static std::string save_file;
static unsigned int width = 640, height = 480;
static bool mode_given = false;

/* One option, from the Shell or a Tool Type of the icon; false when it is
   not known. */
static bool
parse_option(const char *a) {
  const char *v;
  if ((v = option_value(a, "MODE")) != nullptr) {
    unsigned int w = 0, h = 0;
    if (sscanf(v, "%ux%u", &w, &h) == 2 && w >= 320 && h >= 200) {
      width = w;
      height = h;
      mode_given = true;
    }
  } else if ((v = option_value(a, "DATA")) != nullptr) {
    data_dirs.push_back(v);
  } else if ((v = option_value(a, "LOAD")) != nullptr) {
    save_file = v;
  } else if ((v = option_value(a, "DEBUG")) != nullptr) {
    int d = atoi(v);
    if (d >= 0 && d < Log::LevelMax) {
      Log::set_level(static_cast<Log::Level>(d));
    }
  } else if (strcasecmp(a, "FPS") == 0) {
    EventLoopAmiga::show_fps = true;
  } else if (strcasecmp(a, "STATS") == 0) {
    EventLoopAmiga::show_stats = true;
  } else if ((v = option_value(a, "RENDER")) != nullptr) {
    VideoAmiga::render_vram = strcasecmp(v, "FAST") != 0;
  } else if (strcasecmp(a, "NOSOUND") == 0) {
    AudioAmiga::disabled = true;
  } else if ((v = option_value(a, "LOCK")) != nullptr) {
    VideoAmiga::lock_per_call = strcasecmp(v, "CALL") == 0;
  } else if ((v = option_value(a, "BLIT")) != nullptr) {
    amiga_blit_mode = atoi(v);
  } else if (strcasecmp(a, "VERIFY") == 0) {
    VideoAmiga::verify = true;
  } else if (strcasecmp(a, "STATS=FULL") == 0) {
    EventLoopAmiga::show_stats = true;
    VideoAmiga::profile = true;
    AudioAmiga::dump_music = true;
  } else {
    return false;
  }
  return true;
}

int
main(int argc, char *argv[]) {
  /* Messages go to a file: text in the Shell window is drawn on the card,
     which waits while the game holds the screen bitmap locked; and with a
     Workbench start it would open a console window. The file first: the
     loggers take the stream when the level is set. */
  static std::ofstream log_file("RAM:Settlers-log.txt");
  Log::set_file(&log_file);
  Log::set_level(Log::LevelWarn);

  if (amiga_wb_start()) {
    /* From the icon: its Tool Types; unknown ones are left alone. */
    char **tt = nullptr;
    int n = amiga_tooltypes(&tt);
    for (int i = 0; i < n; i++) parse_option(tt[i]);
  } else {
    for (int i = 1; i < argc; i++) {
      if (!parse_option(argv[i])) {
        amiga_message("Settlers " SETTLERS_VERSION "\n"
                      "usage: Settlers [MODE=400x300|640x400|640x480|800x600] "
                      "[DATA=path] [LOAD=savefile] [NOSOUND] [FPS] [STATS[=FULL]] "
                      "[VERIFY] [RENDER=VRAM|FAST] [LOCK=FRAME|CALL] "
                      "[BLIT=0|1|2] [DEBUG=n]");
        return 5;
      }
    }
  }
  if (data_dirs.empty()) {
    data_dirs.push_back("PROGDIR:data");
  }

  Log::Info["main"] << "Settlers " << SETTLERS_VERSION;

  try {
    Settings &settings = Settings::get_instance();
    settings.load();
    if (!mode_given) {
      /* The mode chosen in the Options box last time. */
      std::string mode = settings.get("amiga", "mode", std::string());
      unsigned int w = 0, h = 0;
      if (sscanf(mode.c_str(), "%ux%u", &w, &h) == 2 && w >= 320 &&
          h >= 200) {
        width = w;
        height = h;
      }
    }

    Data &data = Data::get_instance();
    if (!data.load(data_dirs)) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "Could not find the game data.\n"
               "Copy gfxheader, gfxfast, gfxchip, gfxpics, sounds and music\n"
               "from the Amiga disks into %s/amiga, or give DATA=path.",
               data_dirs.front().c_str());
      amiga_message(msg);
      return 10;
    }

    VideoAmiga::config_width = width;
    VideoAmiga::config_height = height;
    VideoAmiga::start_picture = data_dirs.front() + "/start.iff";
    Graphics &gfx = Graphics::get_instance();

    Audio &audio = Audio::get_instance();
    Audio::PPlayer player = audio.get_music_player();
    if (player) {
      player->play_track(Audio::TypeMidiTrack0);
    }

    GameManager &game_manager = GameManager::get_instance();
    if (!save_file.empty()) {
      if (!game_manager.load_game(save_file)) {
        return 10;
      }
    } else if (!game_manager.start_random_game()) {
      return 10;
    }

    Interface interface;
    unsigned int sw = 0, sh = 0;
    gfx.get_resolution(&sw, &sh);
    interface.set_size(sw, sh);
    interface.set_displayed(true);
    interface.apply_settings();
    if (EventLoopAmiga::show_fps && !interface.get_show_fps()) {
      interface.switch_show_fps();
    }
    /* The interface draws straight into the screen frame: one copy of the
       whole screen less per frame. */
    Frame *screen_frame = gfx.get_screen_frame();
    interface.use_frame(screen_frame);

    if (save_file.empty()) {
      interface.open_game_init();
    }

    EventLoopAmiga &event_loop =
      static_cast<EventLoopAmiga &>(EventLoop::get_instance());
    event_loop.on_mode_change = [&interface, screen_frame]() {
      interface.use_frame(screen_frame);
      Settings &s = Settings::get_instance();
      unsigned int w = 0, h = 0;
      Graphics::get_instance().get_resolution(&w, &h);
      s.set("amiga", "mode", std::to_string(w) + "x" + std::to_string(h));
      s.save();
    };
    event_loop.add_handler(&interface);
    event_loop.run();
    event_loop.del_handler(&interface);

    interface.store_settings();
    delete screen_frame;
  } catch (ExceptionFreeserf &e) {
    amiga_message(e.what());
    return 20;
  }

  return 0;
}
