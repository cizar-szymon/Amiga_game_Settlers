/*
 * video-amiga.h - 8-bit CLUT video for the Amiga (Picasso96)
 *
 * Every frame and image holds palette indices. Index 0 is transparent and
 * index 1 is "shade": a pixel of the shadow sprites (black at half alpha),
 * which darkens what is under it through a table. The palette is built as
 * the colours turn up; each colour of the game gets a darker twin for the
 * shadows.
 */
#ifndef PORT_VIDEO_AMIGA_H_
#define PORT_VIDEO_AMIGA_H_

#include <cstdint>
#include <string>
#include <unordered_map>

#include "src/video.h"

class Video::Frame {
 public:
  unsigned int w;
  unsigned int h;
  uint8_t *pixels;   /* 16-byte aligned (MOVE16 to the screen) */
  uint8_t *raw;      /* the allocation pixels lies in */
  /* changed: drawn into since opaque was worked out. opaque: no
     transparent and no shade pixel, so a copy of it is a plain copy (the
     landscape tiles of the map view, drawn once and copied every frame). */
  bool changed;
  bool opaque;
  /* RENDER=VRAM: a copy in the card's memory for the blitter (landscape
     tiles), stale when the frame was drawn into since the upload. */
  void *vram;
  bool vram_stale;
  bool vram_failed;
  Frame() : w(0), h(0), pixels(nullptr), raw(nullptr), changed(true),
            opaque(false), vram(nullptr), vram_stale(true),
            vram_failed(false) {}
};

class Video::Image {
 public:
  unsigned int w;
  unsigned int h;
  uint8_t *pixels;
  /* The rows as runs (see VideoAmiga::encode_runs), for drawing without
     a test per pixel; rows[i] is the offset of row i in runs. */
  uint8_t *runs;
  uint32_t *rows;
  /* For the probes: run codes, copied and shaded pixels of the image. */
  uint32_t n_runs, n_copy, n_shade;
  Image() : w(0), h(0), pixels(nullptr), runs(nullptr), rows(nullptr),
            n_runs(0), n_copy(0), n_shade(0) {}
};

class ExceptionAmiga : public ExceptionVideo {
 public:
  explicit ExceptionAmiga(const std::string &description) throw()
    : ExceptionVideo(description) {}
  virtual ~ExceptionAmiga() throw() {}
  virtual std::string get_platform() const { return "Amiga"; }
};

class VramRelease;

class VideoAmiga : public Video {
  friend class VramRelease;
 public:
  static const uint8_t kTransparent = 0;
  static const uint8_t kShade = 1;

  /* Set by main before the first Video::get_instance(). */
  static unsigned int config_width;
  static unsigned int config_height;
  /* RENDER=VRAM (default): compose the frame in the card's memory;
     RENDER=FAST: in fast RAM, sent whole at the swap. */
  static bool render_vram;
  /* LOCK=CALL: unlock the back bitmap after every drawing call (see
     VramRelease); nothing may draw on the card while it is locked. */
  static bool lock_per_call;

 protected:
  Video::Frame *screen;
  uint8_t rgb[256 * 3];
  uint8_t shade[256];
  unsigned int colors_used;
  int dirty_first, dirty_last;
  std::unordered_map<uint32_t, uint8_t> color_index;
  /* RENDER=VRAM: on, and the locked back bitmap (nullptr: not locked). */
  bool vram;
  uint8_t *vram_base;
  int vram_bpr;
  /* VERIFY with VRAM: the screen operations are done again in the fast
     RAM screen frame, which is compared with the back bitmap. */
  bool to_fast;
  unsigned int pending_w, pending_h;
  /* The cursor and what it covers while the frame is shown. */
  Video::Image *cursor;
  int mouse_x, mouse_y;
  int under_x, under_y, under_w, under_h;
  uint8_t under[64 * 64];
  void put_cursor();
  void take_cursor();

 public:
  /* Time spent sending frames to the screen, for the statistics. */
  uint32_t blit_ms;

  /* STATS=FULL: E-clock ticks, calls and pixels per kind of drawing. */
  enum { ProfFrame, ProfImage, ProfFill, ProfOther, ProfCount };
  static bool profile;
  uint32_t prof_ticks[ProfCount];
  uint32_t prof_calls[ProfCount];
  uint32_t prof_pixels[ProfCount];
  /* frame copies that were plain copies (opaque source) */
  uint32_t prof_opaque_calls, prof_opaque_pixels;
  /* time spent locking and unlocking the back bitmap, and how often */
  uint32_t prof_lock_ticks, prof_locks;
  /* sprites drawn from runs / cut at the sides, and their runs and pixels */
  uint32_t prof_img_runs_path, prof_img_clip_path;
  uint32_t prof_runs, prof_copy_px, prof_shade_px, prof_box_px;
  void prof_reset();

  /* VERIFY: every blended row is checked against the reference version. */
  static bool verify;
  uint32_t verify_errors;
  void blend_row(uint8_t *d, const uint8_t *s, int w);
  void copy_row(uint8_t *d, const uint8_t *s, int w);
  void blend_runs(uint8_t *d, const uint8_t *runs, const uint8_t *s, int w,
                  bool to_vram);
  static void encode_runs(Video::Image *image);

  VideoAmiga();
  virtual ~VideoAmiga();

  virtual void set_resolution(unsigned int width, unsigned int height,
                              bool fullscreen);
  virtual void get_resolution(unsigned int *width, unsigned int *height);
  virtual void set_fullscreen(bool enable) {}
  virtual bool is_fullscreen() { return true; }

  virtual Video::Frame *get_screen_frame();
  virtual Video::Frame *create_frame(unsigned int width, unsigned int height);
  virtual void destroy_frame(Video::Frame *frame);

  virtual Video::Image *create_image(void *data, unsigned int width,
                                     unsigned int height);
  virtual void destroy_image(Video::Image *image);

  virtual void warp_mouse(int x, int y) {}

  virtual void draw_image(const Video::Image *image, int x, int y,
                          int y_offset, Video::Frame *dest);
  virtual void draw_frame(int dx, int dy, Video::Frame *dest, int sx, int sy,
                          Video::Frame *src, int w, int h);
  virtual void draw_frame_scaled(int dx, int dy, int dw, int dh,
                                 Video::Frame *dest, Video::Frame *src,
                                 int w, int h);
  virtual void draw_rect(int x, int y, unsigned int width, unsigned int height,
                         const Video::Color color, Video::Frame *dest);
  virtual void fill_rect(int x, int y, unsigned int width, unsigned int height,
                         const Video::Color color, Video::Frame *dest);
  virtual void draw_line(int x, int y, int x1, int y1,
                         const Video::Color color, Video::Frame *dest);

  virtual void swap_buffers();

  /* The game's cursor, drawn by us on every swap (the system pointer is
     hidden): put on the frame before it is shown, taken off after. */
  virtual void set_cursor(void *data, unsigned int width,
                          unsigned int height);
  void set_mouse(int x, int y) { mouse_x = x; mouse_y = y; }

  /* Another screen mode (the Options box): set_resolution() only asks for
     it; the game loop calls apply_mode() between frames. */
  bool mode_pending() const { return pending_w != 0; }
  bool apply_mode();

  virtual float get_zoom_factor() { return 1.f; }
  virtual bool set_zoom_factor(float factor) { return factor == 1.f; }
  virtual void get_screen_factor(float *fx, float *fy) {
    if (fx != nullptr) *fx = 1.f;
    if (fy != nullptr) *fy = 1.f;
  }

  /* Palette index of an opaque colour. */
  uint8_t get_index(uint8_t r, uint8_t g, uint8_t b);
  /* F9: the screen frame in its true colours as a PPM file. */
  bool save_screenshot(const char *path);
  unsigned int get_colors_used() const { return colors_used; }

 protected:
  uint8_t add_color(uint8_t r, uint8_t g, uint8_t b, bool with_shade);
  /* Start of the pixel at x, y of dest: in the back bitmap for the screen
     with RENDER=VRAM, otherwise in the frame's fast RAM. */
  uint8_t *row_ptr(Video::Frame *dest, int x, int y);
  int row_stride(const Video::Frame *dest) const;
  bool in_vram(const Video::Frame *dest) const {
    return vram && !to_fast && dest == screen;
  }
  void touch(Video::Frame *dest) {
    dest->changed = true;
    dest->vram_stale = true;
  }
  void vram_unlocked() { vram_base = nullptr; }
  /* Unlocks the back bitmap. Never keep it locked across calls: any OS
     call that draws on the card (a palette change, a line of text in the
     Shell window) waits for the lock, and the machine stops (0.15). */
  void release_vram();
  void compare_vram();
  uint8_t nearest(uint8_t r, uint8_t g, uint8_t b) const;
};

#endif  // PORT_VIDEO_AMIGA_H_
