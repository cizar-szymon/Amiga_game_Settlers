/*
 * main-amiga.cc - main program of the Amiga port (replaces src/freeserf.cc)
 *
 * Options, in the Shell or as Tool Types of the icon:
 *   MODE=400x300|640x360|640x400|640x480|800x600|960x540|WxH  DATA=path  LOAD=savefile  NOSOUND  FPS
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
#include "src/viewport.h"
#include "port/amiga-box.h"
#include "port/lan-amiga.h"
#include "src/lan-game.h"

extern "C" const char ver[];
const char ver[] = "$VER: Settlers " SETTLERS_VERSION " (" SETTLERS_DATE ")";

/* Value of KEY=value (key case-insensitive), or nullptr. */
static const char *
option_value(const char *arg, const char *key) {
  size_t n = strlen(key);
  if (strncasecmp(arg, key, n) == 0 && arg[n] == '=') return arg + n + 1;
  return nullptr;
}

/* A wide screen (16:9): 640x360, 960x540, 1280x720. */
static bool
wide_mode(unsigned int width, unsigned int height) {
  return width * 9 == height * 16;
}

/* The steps of the magnified map view a screen allows: x2 above 400x300,
   x3 too from 800x600 (a view of about 267x200, the original's size).
   The wide screens: x2 and x3 on 960x540 (and 1280x720 of MODE=; since
   0.105: only the changed pieces of the view go to the card), none on
   640x360. */
static int
max_map_zoom(unsigned int width, unsigned int height) {
  if (wide_mode(width, height)) {
    return width >= 960 ? 3 : 1;
  }
  return width >= 800 ? 3 : (width > 400 ? 2 : 1);
}

static std::list<std::string> data_dirs;
static std::string save_file;
static unsigned int width = 640, height = 480;
static bool mode_given = false;
/* FREEZE: the loaded game paused (screenshots to compare). */
static bool freeze = false;

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
  } else if ((v = option_value(a, "SFXAREA")) != nullptr) {
    /* Map sounds from a WxH area around the centre (default 320x200, the
       original's screen), ALL: from the whole screen. */
    unsigned int w = 0, h = 0;
    if (strcasecmp(v, "ALL") == 0) {
      Viewport::hear_width = Viewport::hear_height = 0;
    } else if (sscanf(v, "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
      Viewport::hear_width = static_cast<int>(w);
      Viewport::hear_height = static_cast<int>(h);
    }
  } else if (strcasecmp(a, "FREEZE") == 0) {
    freeze = true;
  } else if ((v = option_value(a, "PATHS")) != nullptr) {
    /* SCREEN: paths and borders drawn into every frame (to compare). */
    Viewport::paths_in_tiles = strcasecmp(v, "SCREEN") != 0;
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
  } else if (strcasecmp(a, "LANTEST") == 0) {
    Interface::lan_test_keys = true;
  } else if (strcasecmp(a, "LANINFO") == 0) {
    LanGame::show_info = true;
  } else if (strcasecmp(a, "NOSOUND") == 0) {
    AudioAmiga::disabled = true;
  } else if ((v = option_value(a, "LOCK")) != nullptr) {
    VideoAmiga::lock_per_call = strcasecmp(v, "CALL") == 0;
  } else if ((v = option_value(a, "BLIT")) != nullptr) {
    amiga_blit_mode = atoi(v);
  } else if ((v = option_value(a, "ZOOMGAP")) != nullptr) {
    VideoAmiga::zoom_gap = atoi(v);
  } else if (strcasecmp(a, "VERIFY") == 0) {
    VideoAmiga::verify = true;
    Viewport::verify_tiles = true;
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
                      "usage: Settlers [MODE=400x300|640x360|640x400|640x480|"
                      "800x600|960x540] "
                      "[DATA=path] [LOAD=savefile] [NOSOUND] [SFXAREA=WxH|ALL] [FPS] "
                      "[STATS[=FULL]] "
                      "[VERIFY] [RENDER=VRAM|FAST] [LOCK=FRAME|CALL] "
                      "[BLIT=0|1|2] [PATHS=TILES|SCREEN] [FREEZE] [DEBUG=n]");
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
        /* 1280x720 of 0.99-0.105 is no longer in the Options box: the
           largest wide screen there. */
        if (w == 1280 && h == 720) {
          w = 960;
          h = 540;
        }
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

    /* The music as set last time, from the start picture on: started
       before the settings were applied it played for a moment even when
       it was off. */
    Audio &audio = Audio::get_instance();
    Audio::PPlayer player = audio.get_music_player();
    if (player) {
      player->enable(settings.get("options", "music", player->is_enabled()));
      player->play_track(Audio::TypeMidiTrack0);
    }

    GameManager &game_manager = GameManager::get_instance();
    if (!save_file.empty()) {
      if (!game_manager.load_game(save_file)) {
        return 10;
      }
      if (freeze) game_manager.get_current_game()->pause();
    } else if (!game_manager.start_random_game()) {
      return 10;
    }

    /* The Amiga button of the options box: this computer and its Warp. */
    Interface::platform_box_factory = [](Interface *i) -> GuiObject * {
      return new AmigaBox(i);
    };
    Interface::platform_box_width = AmigaBox::kWidth;
    Interface::platform_box_height = AmigaBox::kHeight;

    /* The network of a LAN game (bsdsocket.library, opened when used).
       Made before the interface, so that it goes after it. */
    LanLinkAmiga lan_link;
    Interface::lan_link = &lan_link;

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
      unsigned int w = 0, h = 0;
      Graphics::get_instance().get_resolution(&w, &h);
      int most = max_map_zoom(w, h);
      if (Graphics::get_instance().get_map_zoom() > most) {
        /* Less magnified at a smaller mode. */
        Graphics::get_instance().set_map_zoom(static_cast<float>(most));
        interface.set_size(w, h);
      }
      interface.use_frame(screen_frame);
      /* A LAN game's 640x480 is not the mode of the Options box. */
      if (interface.get_lan_game() == nullptr) {
        Settings &s = Settings::get_instance();
        s.set("amiga", "mode", std::to_string(w) + "x" + std::to_string(h));
        s.save();
      }
    };
    /* Z and the mouse wheel: the map view magnified around the middle of
       the screen, in steps of x2 and x3 (max_map_zoom). The panel and the
       boxes stay as they are. */
    event_loop.on_zoom = [&interface, screen_frame](int step) {
      Graphics &g = Graphics::get_instance();
      unsigned int w = 0, h = 0;
      g.get_resolution(&w, &h);
      Viewport *viewport = interface.get_viewport();
      int most = max_map_zoom(w, h);
      int now = static_cast<int>(g.get_map_zoom());
      int want = (step == 0) ? (now >= most ? 1 : now + 1) : now + step;
      want = std::max(1, std::min(most, want));
      float zoom = static_cast<float>(want);
      if (viewport == nullptr || zoom == g.get_map_zoom()) {
        return;
      }
      MapPos centre = viewport->get_current_map_pos();
      g.set_map_zoom(zoom);
      interface.set_size(w, h);
      interface.use_frame(screen_frame);
      interface.get_viewport()->move_to_map_pos(centre);
    };
    Interface::zoom_hook = event_loop.on_zoom;
    if (SETTLERS_LABEL[0] != 0) {
      Interface::build_label = SETTLERS_LABEL " " SETTLERS_VERSION;
    }
    /* The start picture stays at least 4 s (with the music, when it is
       on); Ctrl-C quits. */
    if (VideoAmiga::start_picture_ms != 0) {
      static const unsigned long kStartPictureMs = 4000;
      for (;;) {
        unsigned long shown = amiga_now_ms() - VideoAmiga::start_picture_ms;
        if (shown >= kStartPictureMs) break;
        if (amiga_wait(kStartPictureMs - shown)) {
          interface.store_settings();
          delete screen_frame;
          return 0;
        }
      }
    }

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
