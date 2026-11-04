/*
 * introtest.cc - the intro player on a PC, for the comparison with the
 * Python reference (tools/intro/check_player.py)
 *
 *   introtest DIR OUTDIR [FRAMES [DUMPFRAME]]
 * DIR holds intro.script and the intro files. Writes OUTDIR/crc.txt (CRC-32
 * of every frame's 320x256 RGB picture, as player_ref.py would save it)
 * and OUTDIR/sfx.raw (the effect samples, 160 per frame).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "port/intro-player.h"

static uint32_t crc_tab[256];

static void
crc_init() {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crc_tab[i] = c;
  }
}

static uint32_t
crc32(uint32_t crc, const uint8_t *p, size_t n) {
  crc = ~crc;
  while (n--) crc = crc_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

int
main(int argc, char *argv[]) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: introtest DIR OUTDIR [FRAMES]\n");
    return 5;
  }
  crc_init();
  IntroPlayer player;
  if (!player.open(argv[1])) {
    std::fprintf(stderr, "no intro: %s\n", player.error());
    return 10;
  }
  int frames = player.frame_count();
  if (argc > 3) frames = std::atoi(argv[3]);
  int dump = argc > 4 ? std::atoi(argv[4]) : -1;
  char path[512];
  std::snprintf(path, sizeof(path), "%s/crc.txt", argv[2]);
  FILE *crc = std::fopen(path, "w");
  std::snprintf(path, sizeof(path), "%s/sfx.raw", argv[2]);
  FILE *sfx = std::fopen(path, "wb");
  if (crc == nullptr || sfx == nullptr) return 10;
  static uint8_t pixels[IntroPlayer::kWidth * IntroPlayer::kHeight];
  static uint8_t rgb[IntroPlayer::kWidth * IntroPlayer::kHeight * 3];
  uint8_t pal[256 * 3];
  uint8_t changed[IntroPlayer::kHeight];
  int8_t buf[IntroPlayer::kSfxPerFrame];
  static uint8_t screen[IntroPlayer::kWidth * IntroPlayer::kHeight];
  int screen_errors = 0;
  for (int f = 0; f < frames; f++) {
    player.step(buf);
    if (player.music_event() != IntroPlayer::MusicNone) {
      std::printf("frame %d: music %d\n", f, player.music_event());
    }
    std::fwrite(buf, 1, sizeof(buf), sfx);
    /* As intro-amiga.cc does it: render only after a change; the screen
       moved by the scroll, then the changed lines and those that came in
       written. The screen must equal the picture in every frame. */
    int scroll = 0;
    if (player.picture_changed() || f == 0) {
      player.render(pixels, pal, changed, &scroll);
      const int w = IntroPlayer::kWidth, h = IntroPlayer::kHeight;
      if (scroll > 0 && scroll < h) {
        std::memmove(screen, screen + scroll * w, (h - scroll) * w);
      } else if (scroll < 0 && -scroll < h) {
        std::memmove(screen - scroll * w, screen, (h + scroll) * w);
      }
      for (int y = 0; y < h; y++) {
        if (f == 0 || changed[y] ||
            (scroll != 0 && (y + scroll < 0 || y + scroll >= h))) {
          std::memcpy(screen + y * w, player.row(pixels, y), w);
        }
      }
    }
    for (int y = 0; y < IntroPlayer::kHeight; y++) {
      if (std::memcmp(screen + y * IntroPlayer::kWidth, player.row(pixels, y),
                      IntroPlayer::kWidth) != 0) {
        screen_errors++;
        break;
      }
    }
    for (int y = 0; y < IntroPlayer::kHeight; y++) {
      const uint8_t *r = player.row(pixels, y);
      uint8_t *o = rgb + y * IntroPlayer::kWidth * 3;
      for (int x = 0; x < IntroPlayer::kWidth; x++) {
        std::memcpy(o + 3 * x, pal + 3 * r[x], 3);
      }
    }
    if (f == dump) {
      std::snprintf(path, sizeof(path), "%s/frame%05d.ppm", argv[2], f);
      FILE *ppm = std::fopen(path, "wb");
      if (ppm != nullptr) {
        std::fprintf(ppm, "P6 %d %d 255\n", IntroPlayer::kWidth,
                     IntroPlayer::kHeight);
        std::fwrite(rgb, 1, sizeof(rgb), ppm);
        std::fclose(ppm);
      }
    }
    std::fprintf(crc, "%d %08x\n", f,
                 static_cast<unsigned>(crc32(0, rgb, sizeof(rgb))));
  }
  std::printf("frames where the updated screen differs: %d\n", screen_errors);
  std::fclose(crc);
  std::fclose(sfx);
  return 0;
}
