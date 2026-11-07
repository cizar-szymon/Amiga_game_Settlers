/*
 * simtest - headless determinism and speed test of the Freeserf game logic.
 *
 * Starts a mission (or loads a save), runs N logic steps and prints a CRC32
 * of the saved game state every K steps, plus the time taken. The same
 * program is built for the PC and for the Amiga; the checksums must match.
 *
 * usage: simtest [-m MISSION] [-l SAVE] [-n STEPS] [-k EVERY] [-s OUTFILE]
 *                [-r SEED] [-c] [-z SIZE]
 *
 * -z starts a new game on a random map of that size (1-10) with four
 * computer players instead of a mission.
 *
 * -c builds player 0's castle at the free place nearest to the map centre,
 * so that a save made with -s shows a game in progress (the scene of the
 * performance tests of the Amiga port).
 *
 * Freeserf seeds the game's random generator from the clock (as the original
 * does), so a new mission differs on every run. Unless -r 0 is given, the
 * game's "random" field is replaced by SEED and the game reloaded from that
 * state, which makes runs repeatable without changing Freeserf.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>

#include "src/game-manager.h"
#include "src/game.h"
#include "src/mission.h"
#include "src/savegame.h"
#include "src/log.h"
#include "src/map.h"
#include "src/player.h"

#ifdef __AMIGA__
#include "amiga_time.h"
extern "C" const char ver[];
const char ver[] = "$VER: simtest 0.1 (25.10.2026)";
#endif

static uint32_t crc_table[256];

static void crc_init() {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) {
      c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    crc_table[i] = c;
  }
}

static uint32_t crc32(const std::string &s) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < s.size(); i++) {
    c = crc_table[(c ^ static_cast<uint8_t>(s[i])) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

/* Milliseconds from a monotonic clock. */
static uint32_t now_ms() {
#ifdef __AMIGA__
  return static_cast<uint32_t>(amiga_now_ms());
#else
  return static_cast<uint32_t>(clock() * 1000.0 / CLOCKS_PER_SEC);
#endif
}

static std::string save_state(Game *game) {
  std::ostringstream os;
  GameStore::get_instance().write(&os, game);
  return os.str();
}

/* A copy of the game with its random state set to seed. */
static PGame reseed(Game *game, const std::string &seed) {
  std::string st = save_state(game);
  const std::string key = "\n  random = ";
  size_t a = st.find(key);
  if (a == std::string::npos) return nullptr;
  a += key.size();
  size_t b = st.find('\n', a);
  st.replace(a, b - a, seed);
  PGame copy = std::make_shared<Game>();
  std::istringstream is(st);
  if (!GameStore::get_instance().read(&is, copy.get())) return nullptr;
  return copy;
}

/* Player 0's castle at the place nearest to the centre of the map. */
static bool build_castle_near_centre(Game *game) {
  PMap map = game->get_map();
  Player *player = game->get_player(0);
  int cols = map->get_cols(), rows = map->get_rows();
  int best_d = -1;
  MapPos best = 0;
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      MapPos pos = map->pos(x, y);
      if (!game->can_build_castle(pos, player)) continue;
      int d = (x - cols / 2) * (x - cols / 2) +
              (y - rows / 2) * (y - rows / 2);
      if (best_d < 0 || d < best_d) {
        best_d = d;
        best = pos;
      }
    }
  }
  return best_d >= 0 && game->build_castle(best, player);
}

/* The parts of Game::update (game.cc, -p). */
extern unsigned long (*game_prof_ms)();
extern unsigned long (*game_prof_detail_ms)();
extern unsigned long game_prof[64];
extern unsigned long game_prof_serf[64], game_prof_serf_n[64];
extern unsigned long flag_search_count[8];
static unsigned long prof_now() { return now_ms(); }

int main(int argc, char *argv[]) {
  bool profile = false, detail = false;
  int mission = 0;
  unsigned int steps = 2000;
  unsigned int every = 500;
  std::string load_file, out_file;
  std::string seed = "4764524674278577";
  bool castle = false;
  unsigned int map_size = 0;

  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const char *v = (i + 1 < argc) ? argv[i + 1] : nullptr;
    if (!strcmp(a, "-m") && v) { mission = atoi(v); i++; }
    else if (!strcmp(a, "-l") && v) { load_file = v; i++; }
    else if (!strcmp(a, "-n") && v) { steps = strtoul(v, nullptr, 10); i++; }
    else if (!strcmp(a, "-k") && v) { every = strtoul(v, nullptr, 10); i++; }
    else if (!strcmp(a, "-s") && v) { out_file = v; i++; }
    else if (!strcmp(a, "-r") && v) { seed = v; i++; }
    else if (!strcmp(a, "-c")) { castle = true; }
    else if (!strcmp(a, "-p")) { profile = true; }
    else if (!strcmp(a, "-P")) { profile = true; detail = true; }
    else if (!strcmp(a, "-z") && v) {
      map_size = strtoul(v, nullptr, 10);
      i++;
    }
    else {
      printf("usage: simtest [-m MISSION] [-l SAVE] [-n STEPS] [-k EVERY]"
             " [-s OUTFILE] [-r SEED] [-c] [-z SIZE]\n");
      return 5;
    }
  }
  if (every == 0) every = steps;

  Log::set_level(Log::LevelWarn);
  crc_init();

  GameManager &gm = GameManager::get_instance();
  uint32_t t0 = now_ms();
  bool ok;
  if (map_size != 0) {
    /* A random map with four computer players. */
    PGameInfo info = std::make_shared<GameInfo>(Random(seed));
    info->set_map_size(map_size);
    info->remove_all_players();
    static const Player::Color colors[] = {
      {0x00, 0xe3, 0xe3}, {0xcf, 0x63, 0x63}, {0xdf, 0x7f, 0xef},
      {0xef, 0xef, 0x8f} };
    for (unsigned int i = 0; i < 4; i++) {
      info->add_player(1 + i, colors[i], 40, 40, 40);
    }
    ok = gm.start_game(info);
  } else {
    ok = load_file.empty()
           ? gm.start_game(GameInfo::get_mission(mission))
           : gm.load_game(load_file);
  }
  if (!ok) {
    printf("ERROR: cannot start game\n");
    return 10;
  }
  PGame game = gm.get_current_game();
  if (!load_file.empty() && game->is_paused()) game->pause();  // unpause
  if (seed != "0") {
    game = reseed(game.get(), seed);
    if (!game) {
      printf("ERROR: cannot reseed game\n");
      return 10;
    }
  }
  /* A game read from a save is paused (speed 0 is not stored). */
  if (game->is_paused()) game->speed_reset();
  if (castle && !build_castle_near_centre(game.get())) {
    printf("ERROR: no place for player 0's castle\n");
    return 10;
  }
  uint32_t t1 = now_ms();
  printf("start: %s, %u ms\n",
         map_size != 0 ? ("map size " + std::to_string(map_size)).c_str()
         : load_file.empty() ? ("mission " + std::to_string(mission)).c_str()
                           : load_file.c_str(),
         static_cast<unsigned>(t1 - t0));
  printf("step %6u crc %08lx\n", 0u,
         static_cast<unsigned long>(crc32(save_state(game.get()))));

  uint32_t tsum = 0, tmax = 0;
  if (profile) game_prof_ms = prof_now;
  if (detail) game_prof_detail_ms = prof_now;
  for (unsigned int s = 1; s <= steps; s++) {
    uint32_t a = now_ms();
    game->update();
    uint32_t d = now_ms() - a;
    tsum += d;
    if (d > tmax) tmax = d;
    if (s % every == 0 || s == steps) {
      std::string st = save_state(game.get());
      printf("step %6u crc %08lx  game tick %u%s  tick %lu.%02lu ms avg,"
             " %lu ms max\n", s,
             static_cast<unsigned long>(crc32(st)), game->get_tick(),
             game->is_paused() ? " PAUSED" : "",
             static_cast<unsigned long>(tsum / s),
             static_cast<unsigned long>((tsum % s) * 100 / s),
             static_cast<unsigned long>(tmax));
      fflush(stdout);
    }
  }
  if (!out_file.empty()) {
    GameStore::get_instance().save(out_file, game.get());
  }
  printf("done: %u steps in %u ms\n", steps,
         static_cast<unsigned>(tsum));
  printf("searches %lu/step, flags visited %lu/step, failed %lu/step, "
         "call_transporter %lu/step, send_serf_to_flag %lu/step\n",
         flag_search_count[0] / steps, flag_search_count[1] / steps,
         flag_search_count[2] / steps, flag_search_count[3] / steps,
         flag_search_count[4] / steps);
  if (profile) {
    static const char *names[] = { "request failure", "map", "players",
      "AI site scan", "AI damping+emergency", "AI update", "knight morale",
      "inventories", "flags", "buildings", "serfs", "game stats" };
    for (int i = 0; i < 12; i++) {
      printf("  %-22s %7lu ms  %lu.%02lu ms/step\n", names[i], game_prof[i],
             game_prof[i] / steps, (game_prof[i] * 100 / steps) % 100);
    }
    printf("  (in flags) known dest %lu ms, unknown dest %lu ms\n",
           game_prof[12], game_prof[13]);
    printf("  (in buildings) request serf %lu ms\n", game_prof[14]);
    for (int t = 0; t < 64; t++) {
      if (game_prof_serf_n[t] != 0) {
        printf("  serf state %2d: %6lu ms, %lu serfs/step\n", t,
               game_prof_serf[t], game_prof_serf_n[t] / steps);
      }
    }
    for (int t = 0; t < 32; t++) {
      if (game_prof[16 + t] >= 20) {
        printf("  building type %2d: %lu ms\n", t, game_prof[16 + t]);
      }
    }
  }
#ifdef __AMIGA__
  amiga_time_close();
#endif
  return 0;
}
