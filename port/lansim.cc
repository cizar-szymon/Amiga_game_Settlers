/*
 * lansim - a LAN game without the network.
 *
 * Makes the game from a SETUP the way both computers do (encode, decode,
 * the seed of the random numbers), runs it and prints the LAN checksum
 * every K steps, and the CRC32 of the whole saved state to check that the
 * checksum follows it. Two runs, on any two Amigas (or WinUAE), must print
 * the same lines.
 *
 * With -l, two games side by side in lockstep, as the server and the one
 * who joined: each side's player builds a castle, a lumberjack and a road
 * and changes settings, the commands go to the other game encoded and are
 * done on both at the same turn (LanCommand, turns of 4 steps, delay 2).
 * Both games must stay the same.
 *
 * -s PCT: the size of the deserts (Sand of the Advanced box); the number
 * of desert triangles is printed.
 *
 * usage: lansim [-z MAPSIZE] [-n STEPS] [-k EVERY] [-a AIPLAYERS] [-l]
 *               [-s PCT]
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "src/game.h"
#include "src/mission.h"
#include "src/map.h"
#include "src/savegame.h"
#include "src/log.h"
#include "src/pathfinder.h"
#include "src/lan-sync.h"
#include "src/lan-command.h"

#include "amiga_time.h"

extern "C" const char ver[];
const char ver[] = "$VER: lansim 0.2 (26.10.2026)";

static uint32_t crc_table[256];

static uint32_t crc32(const std::string &s) {
  if (crc_table[1] == 0) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      crc_table[i] = c;
    }
  }
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < s.size(); i++) {
    c = crc_table[(c ^ static_cast<uint8_t>(s[i])) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

static std::string save_state(Game *game) {
  std::ostringstream os;
  GameStore::get_instance().write(&os, game);
  return os.str();
}

static PGame make_game(const std::string &setup) {
  Random seed;
  unsigned int delay = 0;
  PGameInfo info = lan_decode_setup(setup, &seed, &delay);
  if (!info) return nullptr;
  info->set_game_random(seed);
  return info->instantiate();
}

/* One side of the lockstep: its game, its player and its commands. */
typedef struct Side {
  PGame game;
  unsigned int player;
  MapPos castle;
  MapPos building;
  std::vector<LanCommand> pending;
  /* What it sent, by turn (encoded, as on the wire). */
  std::vector<std::pair<unsigned int, std::string>> sent;
} Side;

static const unsigned int kTurnSteps = 4;
static const unsigned int kDelay = 2;

/* The first place from (col, row) on where player may build a castle. */
static MapPos find_castle(Game *game, unsigned int player, unsigned int col,
                          unsigned int row) {
  PMap map = game->get_map();
  unsigned int cols = map->get_cols(), rows = map->get_rows();
  for (unsigned int i = 0; i < cols * rows; i++) {
    MapPos pos = map->pos((col + i % cols) % cols,
                          (row + i / cols) % rows);
    if (game->can_build_castle(pos, game->get_player(player))) return pos;
  }
  return bad_map_pos;
}

static MapPos find_building(Game *game, unsigned int player, MapPos near) {
  PMap map = game->get_map();
  for (unsigned int off = 10; off < 250; off++) {
    MapPos pos = map->pos_add_spirally(near, off);
    if (game->can_build_building(pos, Building::TypeLumberjack,
                                 game->get_player(player))) {
      return pos;
    }
  }
  return bad_map_pos;
}

/* What the player of a side does at step s, from its own game. */
static void script(Side *side, unsigned int s) {
  Game *game = side->game.get();
  PMap map = game->get_map();
  unsigned int p = side->player;
  if (s == 20) {
    unsigned int cols = map->get_cols(), rows = map->get_rows();
    side->castle = p == 0 ? find_castle(game, p, cols / 4, rows / 4)
                          : find_castle(game, p, 3 * cols / 4, 3 * rows / 4);
    printf("player %u castle at %u\n", p, side->castle);
    side->pending.push_back(LanCommand(LanCommand::TypeBuildCastle, p,
                                       side->castle));
  } else if (s == 400 && side->castle != bad_map_pos) {
    side->building = find_building(game, p, side->castle);
    printf("player %u lumberjack at %u\n", p, side->building);
    side->pending.push_back(LanCommand(LanCommand::TypeBuildBuilding, p,
                                       side->building,
                                       Building::TypeLumberjack));
  } else if (s == 800 && side->building != bad_map_pos) {
    MapPos from = map->move_down_right(side->building);
    MapPos to = map->move_down_right(side->castle);
    LanCommand road(LanCommand::TypeBuildRoad, p);
    road.road = pathfinder_map(map.get(), from, to);
    printf("player %u road of %u\n", p,
           static_cast<unsigned int>(road.road.get_length()));
    side->pending.push_back(road);
  } else if (s == 1200) {
    side->pending.push_back(LanCommand(LanCommand::TypeSetting, p, 0,
                                       LanCommand::SettingFoodStonemine,
                                       20000));
    side->pending.push_back(LanCommand(LanCommand::TypeSetting, p, 0,
                                       LanCommand::SettingKnightOccupation,
                                       0, 3));
  }
}

static int lockstep(const std::string &setup, unsigned int steps,
                    unsigned int every) {
  Side side[2];
  for (unsigned int i = 0; i < 2; i++) {
    side[i].game = make_game(setup);
    if (!side[i].game) {
      printf("ERROR: no game\n");
      return 10;
    }
    side[i].player = i;
    side[i].castle = side[i].building = bad_map_pos;
  }
  unsigned int done[2] = {0, 0}, failed[2] = {0, 0};
  for (unsigned int s = 0; s < steps; s++) {
    for (unsigned int i = 0; i < 2; i++) script(&side[i], s + 1);
    if (s % kTurnSteps == 0) {
      unsigned int turn = s / kTurnSteps;
      /* Each game: the server's commands, then the other side's - its
         own as they are, the other's decoded from what was sent. */
      for (unsigned int g = 0; g < 2; g++) {
        for (unsigned int from = 0; from < 2; from++) {
          for (const auto &t : side[from].sent) {
            if (t.first != turn) continue;
            size_t at = 0;
            LanCommand c;
            while (at < t.second.size() && c.decode(t.second, &at)) {
              if (c.apply(side[g].game.get())) {
                done[g]++;
              } else {
                failed[g]++;
              }
            }
          }
        }
      }
      for (unsigned int i = 0; i < 2; i++) {
        std::string wire;
        for (const LanCommand &c : side[i].pending) c.encode(&wire);
        side[i].sent.push_back(std::make_pair(turn + kDelay, wire));
        side[i].pending.clear();
      }
    }
    for (unsigned int g = 0; g < 2; g++) side[g].game->update();
    unsigned int n = s + 1;
    if ((n - 1) % every == 0 || n == steps) {
      uint32_t a = lan_checksum(side[0].game.get(), n);
      uint32_t b = lan_checksum(side[1].game.get(), n);
      uint32_t sa = crc32(save_state(side[0].game.get()));
      uint32_t sb = crc32(save_state(side[1].game.get()));
      printf("step %5u  lan %08lx %08lx  state %08lx %08lx  %s\n", n,
             static_cast<unsigned long>(a), static_cast<unsigned long>(b),
             static_cast<unsigned long>(sa), static_cast<unsigned long>(sb),
             (a == b && sa == sb) ? "same" : "DIFFERENT");
    }
  }
  printf("commands done %u/%u, failed %u/%u\n", done[0], done[1],
         failed[0], failed[1]);
  return 0;
}

int main(int argc, char *argv[]) {
  unsigned int map_size = 4, steps = 3000, every = 250, ai = 1;
  bool lock = false;
  unsigned int sand = 100, rocks = 100;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : nullptr;
    if (!strcmp(a, "-z") && v) { map_size = atoi(v); i++; }
    else if (!strcmp(a, "-n") && v) { steps = atoi(v); i++; }
    else if (!strcmp(a, "-k") && v) { every = atoi(v); i++; }
    else if (!strcmp(a, "-a") && v) { ai = atoi(v); i++; }
    else if (!strcmp(a, "-l")) { lock = true; }
    else if (!strcmp(a, "-s") && v) { sand = atoi(v); i++; }
    else if (!strcmp(a, "-r") && v) { rocks = atoi(v); i++; }
    else {
      printf("usage: lansim [-z MAPSIZE] [-n STEPS] [-k EVERY] [-a AI] "
             "[-l] [-s SAND%%] [-r ROCKS%%]\n");
      return 5;
    }
  }
  Log::set_level(Log::LevelWarn);

  /* The settings of a LAN game in the start box: two people, AI. */
  PGameInfo mission = std::make_shared<GameInfo>(Random("3141512624714231"));
  mission->set_map_size(map_size);
  mission->remove_all_players();
  mission->add_player(12, {0x00, 0xe3, 0xe3}, 40, 40, 40);
  mission->add_player(13, {0xcf, 0x63, 0x63}, 40, 40, 40);
  for (unsigned int i = 0; i < ai && i < 2; i++) {
    mission->add_player(3 + i, {0x00, 0x00, 0x00}, 40, 30, 30);
  }
  mission->set_default_colors();
  mission->set_sand_percent(sand);
  mission->set_rock_percent(rocks);

  std::string setup = lan_encode_setup(mission, Random("5612341278563412"),
                                       kDelay);
  printf("SETUP %s\n", setup.c_str());
  if (lock) {
    return lockstep(setup, steps, every);
  }

  uint32_t t0 = amiga_now_ms();
  PGame game = make_game(setup);
  if (!game) {
    printf("ERROR: no game\n");
    return 10;
  }
  printf("start %lu ms\n", static_cast<unsigned long>(amiga_now_ms() - t0));
  {
    PMap map = game->get_map();
    unsigned int desert = 0, all = 0, boulders = 0;
    for (unsigned int r = 0; r < map->get_rows(); r++) {
      for (unsigned int c = 0; c < map->get_cols(); c++) {
        MapPos pos = map->pos(c, r);
        Map::Object o = map->get_obj(pos);
        if (o == Map::ObjectSandstone0 || o == Map::ObjectSandstone1 ||
            o == Map::ObjectStone || o == Map::ObjectSandstone3) {
          boulders++;
        }
        Map::Terrain t[2] = { map->type_up(pos), map->type_down(pos) };
        for (Map::Terrain x : t) {
          all++;
          if (x >= Map::TerrainDesert0 && x <= Map::TerrainDesert2) desert++;
        }
      }
    }
    printf("sand %u%%: %u of %u triangles desert; rocks %u%%: %u boulders\n",
           sand, desert, all, rocks, boulders);
  }

  for (unsigned int s = 1; s <= steps; s++) {
    game->update();
    if ((s - 1) % every == 0 || s == steps) {
      /* The time of the LAN checksum (every 5 s in a LAN game). */
      unsigned long e0 = amiga_eclock();
      uint32_t sum = lan_checksum(game.get(), s);
      unsigned long us = (amiga_eclock() - e0) * 1000 /
                         amiga_eclock_per_ms();
      printf("step %5u  lan %08lx (%lu us)  state %08lx\n", s,
             static_cast<unsigned long>(sum), us,
             static_cast<unsigned long>(crc32(save_state(game.get()))));
    }
  }
  return 0;
}
