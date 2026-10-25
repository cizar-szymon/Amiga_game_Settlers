/*
 * datacheck - loads the game data, decodes every sprite, sound and music
 * resource and prints a CRC32 and the time taken per resource.
 *
 * With -d DIR it also writes one contact sheet per sprite resource
 * (DIR/<name>.ppm, transparent pixels blended over magenta), so that the
 * decoding can be checked by eye.
 *
 * usage: datacheck [-g DATA-PATH] [-d DIR]
 */

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "src/data.h"
#include "src/data-source.h"
#include "src/buffer.h"
#include "src/log.h"

#ifdef __AMIGA__
#include "amiga_time.h"
extern "C" const char ver[];
const char ver[] = "$VER: datacheck 0.1 (25.10.2026)";
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

static uint32_t crc_add(uint32_t c, const void *p, size_t n) {
  const uint8_t *b = static_cast<const uint8_t *>(p);
  c = ~c;
  for (size_t i = 0; i < n; i++) {
    c = crc_table[(c ^ b[i]) & 0xFF] ^ (c >> 8);
  }
  return ~c;
}

/* Adds a value byte by byte (little-endian on every host). */
static uint32_t crc_int(uint32_t c, int32_t v) {
  uint8_t b[4] = { static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8),
                   static_cast<uint8_t>(v >> 16),
                   static_cast<uint8_t>(v >> 24) };
  return crc_add(c, b, 4);
}

static uint32_t now_ms() {
#ifdef __AMIGA__
  return static_cast<uint32_t>(amiga_now_ms());
#else
  return static_cast<uint32_t>(clock() * 1000.0 / CLOCKS_PER_SEC);
#endif
}

/* Contact sheet: sprites placed left to right, wrapping at kSheetWidth. */
class Sheet {
 public:
  static const int kSheetWidth = 640;
  std::vector<uint8_t> rgb;
  int width = kSheetWidth, height = 0, x = 0, y = 0, row_h = 0;

  void add(const Data::PSprite &s) {
    int w = static_cast<int>(s->get_width());
    int h = static_cast<int>(s->get_height());
    if (w > width) w = width;
    if (x + w + 1 > width) {
      x = 0;
      y += row_h + 1;
      row_h = 0;
    }
    if (h > row_h) row_h = h;
    grow(y + row_h + 1);
    const uint8_t *p = s->get_data();   /* B, G, R, A per pixel */
    for (int j = 0; j < h; j++) {
      for (int i = 0; i < w; i++) {
        const uint8_t *px = p + (j * s->get_width() + i) * 4;
        uint8_t *d = &rgb[((y + j) * width + x + i) * 3];
        unsigned a = px[3];
        d[0] = static_cast<uint8_t>((px[2] * a + 0xFF * (255 - a)) / 255);
        d[1] = static_cast<uint8_t>((px[1] * a) / 255);
        d[2] = static_cast<uint8_t>((px[0] * a + 0xFF * (255 - a)) / 255);
      }
    }
    x += w + 1;
  }

  void grow(int h) {
    if (h <= height) return;
    rgb.resize(static_cast<size_t>(width) * h * 3, 0x40);
    height = h;
  }

  bool write(const std::string &path) const {
    if (height == 0) return false;
    FILE *f = fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fwrite(&rgb[0], 1, rgb.size(), f);
    fclose(f);
    return true;
  }
};

int main(int argc, char *argv[]) {
  std::string data_path = "data";
  std::string dump_dir;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-g") && i + 1 < argc) data_path = argv[++i];
    else if (!strcmp(argv[i], "-d") && i + 1 < argc) dump_dir = argv[++i];
    else {
      printf("usage: datacheck [-g DATA-PATH] [-d DIR]\n");
      return 5;
    }
  }

  Log::set_level(Log::LevelWarn);
  crc_init();

  Data &data = Data::get_instance();
  uint32_t t0 = now_ms();
  if (!data.load(data_path)) {
    printf("ERROR: no game data in '%s'\n", data_path.c_str());
    return 10;
  }
  Data::PSource src = data.get_data_source();
  printf("data: %s, load %u ms\n", src->get_name().c_str(),
         static_cast<unsigned>(now_ms() - t0));

  uint32_t total = 0, total_ms = 0;
  unsigned total_sprites = 0, missing = 0;
  for (int r = Data::AssetNone + 1; r <= Data::AssetStar; r++) {
    Data::Resource res = static_cast<Data::Resource>(r);
    Data::Type type = Data::get_resource_type(res);
    unsigned count = Data::get_resource_count(res);
    uint32_t crc = 0;
    unsigned found = 0;
    Sheet sheet;
    uint32_t t = now_ms();
    for (unsigned i = 0; i < count; i++) {
      if (type == Data::TypeSprite) {
        Data::PSprite s = src->get_sprite(res, i, {0xFF, 0xFF, 0xFF, 0xFF});
        if (!s) {
          missing++;
          continue;
        }
        found++;
        crc = crc_int(crc, static_cast<int32_t>(s->get_width()));
        crc = crc_int(crc, static_cast<int32_t>(s->get_height()));
        crc = crc_int(crc, s->get_offset_x());
        crc = crc_int(crc, s->get_offset_y());
        crc = crc_int(crc, s->get_delta_x());
        crc = crc_int(crc, s->get_delta_y());
        crc = crc_add(crc, s->get_data(),
                      s->get_width() * s->get_height() * 4);
        if (!dump_dir.empty()) sheet.add(s);
      } else if (type == Data::TypeSound || type == Data::TypeMusic) {
        PBuffer b = (type == Data::TypeSound) ? src->get_sound(i)
                                              : src->get_music(i);
        if (!b) {
          missing++;
          continue;
        }
        found++;
        crc = crc_int(crc, static_cast<int32_t>(b->get_size()));
        crc = crc_add(crc, b->get_data(), b->get_size());
      } else if (type == Data::TypeAnimation) {
        size_t phases = src->get_animation_phase_count(i);
        found++;
        for (size_t ph = 0; ph < phases; ph++) {
          Data::Animation a = src->get_animation(i, ph);
          crc = crc_int(crc, a.sprite);
          crc = crc_int(crc, a.x);
          crc = crc_int(crc, a.y);
        }
      }
    }
    uint32_t dt = now_ms() - t;
    total_ms += dt;
    if (type == Data::TypeSprite) total_sprites += found;
    total = crc_int(total, static_cast<int32_t>(crc));
    printf("%-16s %4u/%-4u crc %08lx %6u ms\n",
           Data::get_resource_name(res).c_str(), found, count,
           static_cast<unsigned long>(crc), static_cast<unsigned>(dt));
    if (!dump_dir.empty() && sheet.height > 0) {
      std::string path = dump_dir;
      char last = path.empty() ? ':' : path[path.size() - 1];
      if (last != ':' && last != '/') path += "/";
      sheet.write(path + Data::get_resource_name(res) + ".ppm");
    }
  }
  printf("total: %u sprites, %u missing, crc %08lx, %u ms\n", total_sprites,
         missing, static_cast<unsigned long>(total),
         static_cast<unsigned>(total_ms));
#ifdef __AMIGA__
  amiga_time_close();
#endif
  return 0;
}
