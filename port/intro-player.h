/*
 * intro-player.h - the original Amiga intro of The Settlers (Blue Byte
 * 1993), replayed from a recorded script (docs/INTRO.md, chapters 6 and 9)
 *
 * The player knows nothing of the original 68k code. It reads the intro
 * script (intro.script: per 50 Hz frame the memory operations, blits,
 * sound commands and the display window of the original, made by
 * tools/intro/make_script.py) and the player's own intro files (RLE,
 * ByteRun1, 8SVX banks, the MOD), keeps the Amiga bitmaps in planar form
 * in fast RAM, runs the recorded blits with a model of the Amiga blitter
 * and turns the shown bitmap into 8-bit pixels. It is the C++ version of
 * tools/intro/player_ref.py and gives the same pixels and effect samples.
 *
 * Plain C++ without AmigaOS calls (stdio for the files), so that it also
 * builds on a PC for the comparison with the Python reference
 * (port/introtest.cc).
 */
#ifndef PORT_INTRO_PLAYER_H_
#define PORT_INTRO_PLAYER_H_

#include <cstddef>
#include <cstdint>

class IntroPlayer {
 public:
  /* The PAL low-res window of the original. */
  static const int kWidth = 320;
  static const int kHeight = 256;
  /* Effect samples mixed per frame (SD00's 160-byte Paula buffers). */
  static const int kSfxPerFrame = 160;
  /* The music fades out in 65 frames after the last frame (SOUN -$3c). */
  static const int kMusicFadeFrames = 65;
  /* Colour of the lines outside the display window (black). */
  static const uint8_t kBlack = 255;

  enum Music { MusicNone, MusicPlay, MusicFade, MusicStop };

  IntroPlayer();
  ~IntroPlayer();

  /* Reads dir/intro.script and checks that every file it needs is in dir.
     False: no intro (the reason in error()). */
  bool open(const char *dir);
  const char *error() const { return err; }

  int frame_count() const { return nframes; }
  /* The frame step() runs next. */
  int frame() const { return cur_frame; }
  /* The scene of the original (1..7) the last step() was in. */
  int scene() const { return scene_no; }

  /* Runs the operations of the next frame; sfx gets its kSfxPerFrame
     effect samples (signed 8 bit, Paula channel 3). False when all frames
     have been played (sfx is then silence). */
  bool step(int8_t *sfx);

  /* The music command of the last step(), MusicNone if none. */
  Music music_event() const { return music_ev; }
  /* The music module (MUSI, a ProTracker module), for the player of the
     host; nullptr when it was not read. */
  int8_t *music_module(size_t *size) {
    *size = musi_size;
    return musi;
  }

  /* Has the shown picture (memory or display window) changed since the
     last render()? */
  bool picture_changed() const { return dirty; }
  /* The shown picture: kWidth x kHeight colour indices (band b of the
     display uses colours b * 64 .. b * 64 + 63, kBlack outside it) and the
     palette (256 RGB triplets, 8 bit). Line y is at row(pixels, y). Only
     the lines whose plane data changed since the last render() are
     converted (pixels must be the same buffer every time). scroll = d when
     the picture moved up by d lines (down when d < 0): line y then shows
     what line y + d showed before, unless changed[y] = 1. */
  void render(uint8_t *pixels, uint8_t *rgb, uint8_t *changed, int *scroll);
  const uint8_t *row(const uint8_t *pixels, int y) const {
    return pixels + slot[y] * kWidth;
  }

 protected:
  struct Band {
    int first_line;
    int depth;
    uint8_t *planes[6];
    unsigned con1, ddfstrt, ddfstop, diwstrt, diwstop;
    int mod1, mod2;
    uint16_t colours[32];
  };
  struct Voice {
    const uint8_t *ptr, *end, *start;
    unsigned vol;
    unsigned cnt;
    bool used;
  };
  enum { kMaxBands = 3, kMaxModules = 4 };

  char err[160];
  char dir[256];
  uint8_t *script;
  size_t script_size;
  size_t pos;
  int nframes;
  int cur_frame;
  int scene_no;

  /* The memory blocks of the original, one arena, kPad guard bytes before
     and after each (blits may start a word before a buffer). */
  uint8_t *arena;
  uint8_t **block;
  uint32_t *block_size;
  int nblocks;

  /* Static data inside code modules (the FHRS font), RLE unpacked. */
  char module_name[kMaxModules][5];
  uint8_t *module_data[kMaxModules];
  size_t module_size[kMaxModules];
  int nmodules;

  /* The files of the intro as they are on the disk, read by open(). */
  enum { kMaxFiles = 64 };
  struct File {
    char name[5];
    uint8_t *raw;
    size_t size;
  } files[kMaxFiles];
  int nfiles;

  int8_t *musi;
  size_t musi_size;
  Music music_ev;

  Band bands[kMaxBands];
  int nbands;
  bool dirty;

  /* SD00's effect mixer: three voices into one byte stream. */
  int8_t vol_tab[64][256];
  Voice voices[3];
  const uint8_t *sfx_bank;
  bool sfx_on;

  uint16_t *blit_tmp;

  /* What each line of pixels shows: its plane data when it was converted
     (the usual 320-pixel window), so that unchanged lines are skipped. */
  enum { kLineNone, kLineBlack, kLineFast, kLineOther };
  uint8_t line_state[kHeight];
  uint8_t line_depth[kHeight];
  uint32_t line_data[kHeight][60];
  /* Storage row of each line (rotated when the band scrolls). */
  uint8_t slot[kHeight];
  const uint8_t *last_planes[6];
  int last_depth, last_first;
  int band_scroll();

  bool fail(const char *fmt, const char *arg = "");
  bool read_file(const char *name, uint8_t **data, size_t *size,
                 bool rle);
  bool check_script();
  uint8_t *pointer(const uint8_t *&p);
  bool skip_pointer(const uint8_t *&p, const char **module);
  void op_load(const uint8_t *&p);
  void op_blit(const uint8_t *&p);
  void op_view(const uint8_t *&p);
  void mix(int8_t *out);
  void sfx_start(unsigned k, unsigned vol, unsigned rep);
  void render_band(const Band &b, int bi, int y0, int y1, uint8_t *pixels,
                   uint8_t *changed);
  void black_lines(int y0, int y1, uint8_t *pixels, uint8_t *changed);
  bool line_same(int y, const uint8_t *const *pl, int depth);
};

#endif  // PORT_INTRO_PLAYER_H_
