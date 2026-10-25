/*
 * loadtest - how long loading a Freeserf save takes, step by step.
 *
 *   1. the file read into memory
 *   2. ConfigFile::read: the text split into sections and values
 *   3. GameStore::read: the whole load (2 again, the values split into
 *      lists, the game objects built, the integrity check)
 *   4. GameStore::write: the save, for comparison
 *
 * usage: loadtest SAVEFILE [ROUNDS]
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "src/configfile.h"
#include "src/game.h"
#include "src/integrity.h"
#include "src/log.h"
#include "src/savegame.h"

#ifdef __AMIGA__
#include "amiga_time.h"
extern "C" const char ver[];
const char ver[] = "$VER: loadtest 0.1 (25.10.2026)";
#endif

static uint32_t crc32(const std::string &s) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < s.size(); i++) {
    c ^= static_cast<uint8_t>(s[i]);
    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
  }
  return c ^ 0xFFFFFFFFu;
}

static uint32_t now_ms() {
#ifdef __AMIGA__
  return static_cast<uint32_t>(amiga_now_ms());
#else
  return 0;
#endif
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("usage: loadtest SAVEFILE [ROUNDS]\n");
    return 5;
  }
  int rounds = argc > 2 ? atoi(argv[2]) : 2;
  Log::set_level(Log::LevelError);

  for (int r = 1; r <= rounds; r++) {
    uint32_t t0 = now_ms();
    std::ifstream file(argv[1], std::ios::binary);
    std::stringstream buf;
    buf << file.rdbuf();
    std::string text = buf.str();
    uint32_t t1 = now_ms();

    unsigned long nvalues = 0, nparts = 0;
    uint32_t ts;
    {
      std::istringstream is(text);
      ConfigFile config;
      config.read(&is);
      ts = now_ms();
      /* The values split into lists, as SaveReaderTextSection does. */
      for (const std::string &sect : config.get_sections()) {
        for (const std::string &name : config.get_values(sect)) {
          SaveReaderTextValue v(config.value(sect, name, ""));
          nvalues++;
          nparts += v.size();
        }
      }
    }
    uint32_t t2 = now_ms();
    printf("  split %lu values into %lu parts: %lu ms\n", nvalues, nparts,
           static_cast<unsigned long>(t2 - ts));

    std::shared_ptr<Game> game = std::make_shared<Game>();
    bool ok;
    {
      std::istringstream is(text);
      ok = GameStore::get_instance().read(&is, game.get());
    }
    uint32_t t3 = now_ms();
    IntegrityCheck::run(game.get(), true);
    printf("  integrity check alone: %lu ms\n",
           static_cast<unsigned long>(now_ms() - t3));
    t3 = now_ms();

    std::ostringstream os;
    GameStore::get_instance().write(&os, game.get());
    uint32_t t4 = now_ms();

    game.reset();
    uint32_t t5 = now_ms();

    printf("  saved %lu bytes, crc %08lx (must stay the same when the loader "
           "or the saver changes)\n",
           static_cast<unsigned long>(os.str().size()),
           static_cast<unsigned long>(crc32(os.str())));
    printf("round %d: %lu bytes, read file %lu ms, ConfigFile %lu ms, "
           "load %lu ms (%s), save %lu ms, free %lu ms\n", r,
           static_cast<unsigned long>(text.size()),
           static_cast<unsigned long>(t1 - t0),
           static_cast<unsigned long>(t2 - t1),
           static_cast<unsigned long>(t3 - t2), ok ? "ok" : "FAILED",
           static_cast<unsigned long>(t4 - t3),
           static_cast<unsigned long>(t5 - t4));
    fflush(stdout);
  }
#ifdef __AMIGA__
  amiga_time_close();
#endif
  return 0;
}
