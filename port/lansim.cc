/*
 * lansim - the start of a LAN game without the network: makes the game
 * from a SETUP the way both computers do (encode, decode, the seed of the
 * random numbers), runs it and prints the LAN checksum every K steps, and
 * the CRC32 of the whole saved state to check that the checksum follows it.
 * Two runs, on any two Amigas (or WinUAE), must print the same lines.
 *
 * usage: lansim [-z MAPSIZE] [-n STEPS] [-k EVERY] [-a AIPLAYERS]
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "src/game.h"
#include "src/mission.h"
#include "src/savegame.h"
#include "src/log.h"
#include "src/lan-sync.h"

#include "amiga_time.h"

extern "C" const char ver[];
const char ver[] = "$VER: lansim 0.1 (26.10.2026)";

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

int main(int argc, char *argv[]) {
  unsigned int map_size = 4, steps = 3000, every = 250, ai = 1;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : nullptr;
    if (!strcmp(a, "-z") && v) { map_size = atoi(v); i++; }
    else if (!strcmp(a, "-n") && v) { steps = atoi(v); i++; }
    else if (!strcmp(a, "-k") && v) { every = atoi(v); i++; }
    else if (!strcmp(a, "-a") && v) { ai = atoi(v); i++; }
    else {
      printf("usage: lansim [-z MAPSIZE] [-n STEPS] [-k EVERY] [-a AI]\n");
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

  std::string setup = lan_encode_setup(mission, Random("5612341278563412"));
  printf("SETUP %s\n", setup.c_str());
  Random seed;
  PGameInfo again = lan_decode_setup(setup, &seed);
  if (!again) {
    printf("ERROR: SETUP not read back\n");
    return 10;
  }
  again->set_game_random(seed);
  uint32_t t0 = amiga_now_ms();
  PGame game = again->instantiate();
  if (!game) {
    printf("ERROR: no game\n");
    return 10;
  }
  printf("start %lu ms\n", static_cast<unsigned long>(amiga_now_ms() - t0));

  for (unsigned int s = 1; s <= steps; s++) {
    game->update();
    if ((s - 1) % every == 0 || s == steps) {
      printf("step %5u  lan %08lx  state %08lx\n", s,
             static_cast<unsigned long>(lan_checksum(game.get())),
             static_cast<unsigned long>(crc32(save_state(game.get()))));
    }
  }
  return 0;
}
