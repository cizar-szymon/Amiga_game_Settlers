/*
 * mapgentest.cc - time of the map generator by map size (the start menu
 * makes a new map for its preview at each change), and a CRC of the
 * generated landscape to check that faster code gives the same maps.
 *
 * usage: mapgentest [SIZE ...]   (default 3 to 8)
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

#include "src/map.h"
#include "src/map-generator.h"
#include "src/map-geometry.h"
#include "src/random.h"

#include "amiga_time.h"

static uint32_t crc_table[256];

static void crc_init() {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
    crc_table[i] = c;
  }
}

static uint32_t crc_add(uint32_t crc, uint32_t v) {
  for (int i = 0; i < 4; i++, v >>= 8) {
    crc = crc_table[(crc ^ v) & 0xff] ^ (crc >> 8);
  }
  return crc;
}

/* The generator's phase clock (map-generator.cc). */
extern unsigned long (*mapgen_ms)();
extern unsigned long mapgen_phase[16];

int main(int argc, char *argv[]) {
  mapgen_ms = amiga_now_ms;
  std::vector<unsigned int> sizes;
  for (int i = 1; i < argc; i++) sizes.push_back(atoi(argv[i]));
  if (sizes.empty()) {
    for (unsigned int s = 3; s <= 8; s++) sizes.push_back(s);
  }
  crc_init();

  for (unsigned int size : sizes) {
    unsigned long t0 = amiga_now_ms();
    Map map{MapGeometry(size)};
    ClassicMapGenerator generator(map, Random("8667715887436237"));
    generator.init(MapGenerator::HeightGeneratorMidpoints, true);
    unsigned long t1 = amiga_now_ms();
    generator.generate();
    unsigned long t2 = amiga_now_ms();
    map.init_tiles(generator);
    unsigned long t3 = amiga_now_ms();

    uint32_t crc = 0xffffffffu;
    for (unsigned int y = 0; y < map.get_rows(); y++) {
      for (unsigned int x = 0; x < map.get_cols(); x++) {
        MapPos pos = map.pos(x, y);
        crc = crc_add(crc, (map.get_height(pos) << 24) |
                           (map.type_up(pos) << 16) |
                           (map.type_down(pos) << 8) | map.get_obj(pos));
        crc = crc_add(crc, (map.get_res_type(pos) << 8) |
                           map.get_res_amount(pos));
      }
    }
    printf("size %2u (%4ux%4u): init %5lu ms  generate %6lu ms  tiles %4lu ms"
           "  crc %08lx\n", size, map.get_cols(), map.get_rows(),
           t1 - t0, t2 - t1, t3 - t2,
           static_cast<unsigned long>(~crc));
    static const char *names[] = {"squares", "midpoints+clamp", "water",
      "rebase", "types", "islands", "rescale", "shore water", "shore grass",
      "deserts", "objects", "minerals", "clean up"};
    for (int i = 0; i < 13; i++) {
      printf("   %-18s %6lu ms\n", names[i], mapgen_phase[i]);
      mapgen_phase[i] = 0;
    }
    fflush(stdout);
  }
  amiga_time_close();
  return 0;
}
