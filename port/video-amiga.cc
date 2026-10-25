/*
 * video-amiga.cc - 8-bit CLUT video for the Amiga (see video-amiga.h)
 *
 * First version: everything is drawn in fast RAM and the whole screen
 * frame is sent with WriteChunkyPixels() on every swap.
 */

#include "port/video-amiga.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "port/amiga_screen.h"

/* Longword access to pixel rows. The 68020 and later read and write
   unaligned longwords, and with may_alias gcc emits a plain move.l; a
   std::memcpy of 4 bytes went through the stack at -O1 (two moves per
   longword, 2026-10-25). */
typedef uint32_t __attribute__((__may_alias__)) Long;

static inline uint32_t
get_long(const uint8_t *p) {
  return *reinterpret_cast<const Long *>(p);
}

static inline void
put_long(uint8_t *p, uint32_t v) {
  *reinterpret_cast<Long *>(p) = v;
}

unsigned int VideoAmiga::config_width = 640;
unsigned int VideoAmiga::config_height = 480;
bool VideoAmiga::profile = false;
bool VideoAmiga::verify = false;
bool VideoAmiga::render_vram = true;
bool VideoAmiga::lock_per_call = false;

/* Times a drawing call with the E-clock when profiling is on. */
class ProfScope {
  VideoAmiga *video;
  int kind;
  uint32_t start;

 public:
  ProfScope(VideoAmiga *v, int k, uint32_t pixels) : video(v), kind(k) {
    if (VideoAmiga::profile) {
      video->prof_calls[kind]++;
      video->prof_pixels[kind] += pixels;
      start = static_cast<uint32_t>(amiga_eclock());
    }
  }
  ~ProfScope() {
    if (VideoAmiga::profile) {
      video->prof_ticks[kind] += static_cast<uint32_t>(amiga_eclock()) - start;
    }
  }
};

void
VideoAmiga::prof_reset() {
  for (int i = 0; i < ProfCount; i++) {
    prof_ticks[i] = prof_calls[i] = prof_pixels[i] = 0;
  }
  prof_opaque_calls = prof_opaque_pixels = 0;
  prof_lock_ticks = prof_locks = 0;
  prof_img_runs_path = prof_img_clip_path = 0;
  prof_runs = prof_copy_px = prof_shade_px = prof_box_px = 0;
}

Video &
Video::get_instance() {
  static VideoAmiga instance;
  return instance;
}

VideoAmiga::VideoAmiga()
  : screen(nullptr)
  , colors_used(2)
  , dirty_first(0)
  , dirty_last(1)
  , vram(false)
  , vram_base(nullptr)
  , vram_bpr(0)
  , to_fast(false)
  , pending_w(0)
  , pending_h(0)
  , cursor(nullptr)
  , mouse_x(0)
  , mouse_y(0)
  , under_x(0)
  , under_y(0)
  , under_w(0)
  , under_h(0)
  , blit_ms(0)
  , verify_errors(0) {
  prof_reset();
  std::memset(rgb, 0, sizeof(rgb));
  /* 0 transparent, 1 shade: both black on the screen. Darkening a
     transparent pixel gives a shade pixel, so a shadow drawn into an
     empty frame still darkens what the frame is drawn on later. */
  for (int i = 0; i < 256; i++) shade[i] = static_cast<uint8_t>(i);
  shade[kTransparent] = kShade;

  char err[128];
  if (amiga_screen_open(config_width, config_height, "Settlers",
                        err, sizeof(err)) != 0) {
    amiga_screen_close();
    throw ExceptionAmiga(err);
  }
  screen = create_frame(config_width, config_height);
  vram = render_vram && amiga_vram_open() == 0;
  if (vram) {
    /* Start from a cleared back bitmap, like the fast RAM frame. */
    for (unsigned int y = 0; y < screen->h; y++) {
      std::memset(row_ptr(screen, 0, y), kTransparent, screen->w);
    }
    release_vram();
  }
}

void
VideoAmiga::release_vram() {
  if (vram_base != nullptr) {
    uint32_t t = profile ? static_cast<uint32_t>(amiga_eclock()) : 0;
    amiga_back_unlock();
    if (profile) {
      prof_lock_ticks += static_cast<uint32_t>(amiga_eclock()) - t;
    }
    vram_base = nullptr;
  }
}

/* LOCK=CALL: unlocks the back bitmap at the end of every drawing call.
   By default (LOCK=FRAME) it stays locked while the frame is drawn and is
   unlocked for the blits and the swap: a lock and unlock cost about 29 us
   on the Warp, some 8 ms for the ~290 drawing calls of a frame. */
class VramRelease {
  VideoAmiga *video;

 public:
  explicit VramRelease(VideoAmiga *v) : video(v) {}
  ~VramRelease() {
    if (VideoAmiga::lock_per_call) video->release_vram();
  }
};

uint8_t *
VideoAmiga::row_ptr(Video::Frame *dest, int x, int y) {
  if (in_vram(dest)) {
    if (vram_base == nullptr) {
      uint32_t t = profile ? static_cast<uint32_t>(amiga_eclock()) : 0;
      vram_base = amiga_back_lock(&vram_bpr);
      if (profile) {
        prof_lock_ticks += static_cast<uint32_t>(amiga_eclock()) - t;
        prof_locks++;
      }
    }
    if (vram_base != nullptr) {
      return vram_base + y * vram_bpr + x;
    }
  }
  return dest->pixels + y * dest->w + x;
}

int
VideoAmiga::row_stride(const Video::Frame *dest) const {
  return (in_vram(dest) && vram_base != nullptr) ? vram_bpr
                                                 : static_cast<int>(dest->w);
}

VideoAmiga::~VideoAmiga() {
  if (cursor != nullptr) {
    destroy_image(cursor);
    cursor = nullptr;
  }
  if (screen != nullptr) {
    destroy_frame(screen);
    screen = nullptr;
  }
  amiga_screen_close();
}

void
VideoAmiga::set_resolution(unsigned int width, unsigned int height,
                           bool fullscreen) {
  if (width == screen->w && height == screen->h) return;
  pending_w = width;
  pending_h = height;
}

/* A new screen in the asked mode, the screen frame resized in place (the
   GUI keeps pointers to it), the whole palette loaded again. The tiles'
   bitmaps on the card stay: their format does not change. */
bool
VideoAmiga::apply_mode() {
  unsigned int w = pending_w, h = pending_h;
  pending_w = pending_h = 0;
  release_vram();
  vram = false;
  char err[128];
  if (amiga_screen_reopen(w, h, err, sizeof(err)) != 0) {
    w = screen->w;
    h = screen->h;
  }
  delete[] screen->raw;
  screen->w = w;
  screen->h = h;
  screen->raw = new uint8_t[w * h + 15];
  screen->pixels = reinterpret_cast<uint8_t *>(
    (reinterpret_cast<uintptr_t>(screen->raw) + 15) & ~uintptr_t(15));
  std::memset(screen->pixels, kTransparent, w * h);
  screen->changed = true;
  config_width = w;
  config_height = h;

  vram = render_vram && amiga_vram_open() == 0;
  if (vram) {
    for (unsigned int y = 0; y < screen->h; y++) {
      std::memset(row_ptr(screen, 0, y), kTransparent, screen->w);
    }
    release_vram();
  }
  dirty_first = 0;
  dirty_last = static_cast<int>(colors_used) - 1;
  if (cursor != nullptr) amiga_hide_pointer();
  return true;
}

void
VideoAmiga::get_resolution(unsigned int *width, unsigned int *height) {
  if (width != nullptr) *width = screen->w;
  if (height != nullptr) *height = screen->h;
}

Video::Frame *
VideoAmiga::get_screen_frame() {
  return screen;
}

Video::Frame *
VideoAmiga::create_frame(unsigned int width, unsigned int height) {
  Video::Frame *frame = new Video::Frame;
  frame->w = width;
  frame->h = height;
  frame->raw = new uint8_t[width * height + 15];
  frame->pixels = reinterpret_cast<uint8_t *>(
    (reinterpret_cast<uintptr_t>(frame->raw) + 15) & ~uintptr_t(15));
  std::memset(frame->pixels, kTransparent, width * height);
  return frame;
}

void
VideoAmiga::destroy_frame(Video::Frame *frame) {
  if (frame->vram != nullptr) {
    release_vram();
    amiga_tile_free(frame->vram);
  }
  delete[] frame->raw;
  delete frame;
}

/* Sprite pixels are B, G, R, A. Fully transparent pixels become index 0,
   the half transparent black of the shadows index 1, all others the
   palette index of their colour. */
Video::Image *
VideoAmiga::create_image(void *data, unsigned int width, unsigned int height) {
  Video::Image *image = new Video::Image;
  image->w = width;
  image->h = height;
  image->pixels = new uint8_t[width * height];
  const uint8_t *p = static_cast<const uint8_t *>(data);
  for (unsigned int i = 0; i < width * height; i++, p += 4) {
    uint8_t a = p[3];
    if (a == 0) {
      image->pixels[i] = kTransparent;
    } else if (a < 0xff && p[0] == 0 && p[1] == 0 && p[2] == 0) {
      image->pixels[i] = kShade;
    } else {
      image->pixels[i] = get_index(p[2], p[1], p[0]);
    }
  }
  encode_runs(image);
  return image;
}

void
VideoAmiga::destroy_image(Video::Image *image) {
  delete[] image->pixels;
  delete[] image->runs;
  delete[] image->rows;
  delete image;
}

/* Run codes: the top two bits say what, the low six bits are the length
   minus one (1..64 pixels). A copy run is followed by its pixels. Every
   row ends with RunEnd. */
enum {
  RunSkip = 0x00,
  RunCopy = 0x40,
  RunShade = 0x80,
  RunEnd = 0xc0,
};

static int
pixel_kind(uint8_t v) {
  return v == VideoAmiga::kTransparent ? RunSkip
       : v == VideoAmiga::kShade ? RunShade : RunCopy;
}

void
VideoAmiga::encode_runs(Video::Image *image) {
  /* Worst case: a code byte per pixel plus the pixel, and the row end. */
  std::vector<uint8_t> out;
  out.reserve(image->w * image->h + image->h * 2);
  image->rows = new uint32_t[image->h];
  for (unsigned int y = 0; y < image->h; y++) {
    image->rows[y] = static_cast<uint32_t>(out.size());
    const uint8_t *row = image->pixels + y * image->w;
    unsigned int x = 0;
    while (x < image->w) {
      int kind = pixel_kind(row[x]);
      unsigned int n = 1;
      while (x + n < image->w && n < 64 && pixel_kind(row[x + n]) == kind) {
        n++;
      }
      /* A skip at the end of the row is left out. */
      bool trailing = kind == RunSkip;
      for (unsigned int k = x + n; trailing && k < image->w; k++) {
        if (row[k] != kTransparent) trailing = false;
      }
      if (trailing) break;
      out.push_back(static_cast<uint8_t>(kind | (n - 1)));
      image->n_runs++;
      if (kind == RunShade) image->n_shade += n;
      if (kind == RunCopy) {
        image->n_copy += n;
        out.insert(out.end(), row + x, row + x + n);
      }
      x += n;
    }
    out.push_back(RunEnd);
  }
  image->runs = new uint8_t[out.size()];
  std::memcpy(image->runs, &out[0], out.size());
}

/* One row of an image from its runs; the row is drawn whole (no
   clipping in x). */
static void
blend_runs_row(uint8_t *d, const uint8_t *r, const uint8_t *shade) {
  for (;;) {
    uint8_t code = *r++;
    unsigned int n = (code & 0x3f) + 1;
    switch (code & 0xc0) {
      case RunSkip:
        d += n;
        break;
      case RunCopy:
        while (n >= 4) {
          put_long(d, get_long(r));
          r += 4;
          d += 4;
          n -= 4;
        }
        while (n-- > 0) *d++ = *r++;
        break;
      case RunShade:
        while (n-- > 0) {
          *d = shade[*d];
          d++;
        }
        break;
      default:
        return;
    }
  }
}

/* h rows of an image from its runs (r: the runs of the first row; the
   rows follow each other), all in one function: a call per row and a
   switch per run cost more than the pixels (2026-10-25: ~11800 runs of
   ~7 pixels per frame). VRAM: writes with aligned longwords, a shade run
   read whole before it is written (see blend_runs_row_vram). */
template <bool kVram>
static void
draw_runs(uint8_t *d, int stride, const uint8_t *r, int h,
          const uint8_t *shade) {
  for (; h > 0; h--, d += stride) {
    uint8_t *p = d;
    for (;;) {
      unsigned int code = *r++;
      unsigned int n = (code & 0x3f) + 1;
      if (code < RunCopy) {           /* skip */
        p += n;
      } else if (code < RunShade) {   /* copy */
        if (kVram) {
          while (n > 0 && (reinterpret_cast<uintptr_t>(p) & 3) != 0) {
            *p++ = *r++;
            n--;
          }
        }
        while (n >= 4) {
          put_long(p, get_long(r));
          r += 4;
          p += 4;
          n -= 4;
        }
        while (n > 0) {
          *p++ = *r++;
          n--;
        }
      } else if (code < RunEnd) {     /* shade */
        if (kVram) {
          uint8_t *a = reinterpret_cast<uint8_t *>(
            reinterpret_cast<uintptr_t>(p) & ~uintptr_t(3));
          unsigned int lead = static_cast<unsigned int>(p - a);
          unsigned int longs = (lead + n + 3) >> 2;
          uint32_t buf[18];
          for (unsigned int i = 0; i < longs; i++) buf[i] = get_long(a + 4 * i);
          uint8_t *b = reinterpret_cast<uint8_t *>(buf) + lead;
          for (unsigned int i = 0; i < n; i++) b[i] = shade[b[i]];
          for (unsigned int i = 0; i < longs; i++) put_long(a + 4 * i, buf[i]);
          p += n;
        } else {
          while (n > 0) {
            *p = shade[*p];
            p++;
            n--;
          }
        }
      } else {
        break;                        /* end of the row */
      }
    }
  }
}

/* The same into the card's memory, following the Warp RTG notes: a copy
   run is written with aligned longwords (bytes only up to the alignment
   and at the end), and a shade run is read whole first, darkened in fast
   RAM and written back: one turn from writing to reading per run instead
   of one per pixel. */
static void
blend_runs_row_vram(uint8_t *d, const uint8_t *r, const uint8_t *shade) {
  for (;;) {
    uint8_t code = *r++;
    unsigned int n = (code & 0x3f) + 1;
    switch (code & 0xc0) {
      case RunSkip:
        d += n;
        break;
      case RunCopy:
        while (n > 0 && (reinterpret_cast<uintptr_t>(d) & 3) != 0) {
          *d++ = *r++;
          n--;
        }
        while (n >= 4) {
          put_long(d, get_long(r));
          r += 4;
          d += 4;
          n -= 4;
        }
        while (n-- > 0) *d++ = *r++;
        break;
      case RunShade: {
        /* Longwords covering d .. d+n (at most 64 + 6 bytes). */
        uint8_t *a = reinterpret_cast<uint8_t *>(
          reinterpret_cast<uintptr_t>(d) & ~uintptr_t(3));
        unsigned int lead = static_cast<unsigned int>(d - a);
        unsigned int longs = (lead + n + 3) >> 2;
        uint32_t buf[18];
        for (unsigned int i = 0; i < longs; i++) {
          buf[i] = get_long(a + 4 * i);
        }
        uint8_t *b = reinterpret_cast<uint8_t *>(buf) + lead;
        for (unsigned int i = 0; i < n; i++) b[i] = shade[b[i]];
        for (unsigned int i = 0; i < longs; i++) {
          put_long(a + 4 * i, buf[i]);
        }
        d += n;
        break;
      }
      default:
        return;
    }
  }
}

/* One row of a frame or image onto the destination: index 0 is skipped,
   index 1 darkens the destination, everything else is copied. This is the
   reference version, a pixel at a time. */
static void
blend_row_ref(uint8_t *d, const uint8_t *s, int w, const uint8_t *shade) {
  for (int i = 0; i < w; i++) {
    uint8_t v = s[i];
    if (v > VideoAmiga::kShade) {
      d[i] = v;
    } else if (v == VideoAmiga::kShade) {
      d[i] = shade[d[i]];
    }
  }
}

/* The same four pixels at a time: a longword without a byte below 2 (no
   transparent and no shade pixel) is copied whole. The 68020+ allows the
   unaligned longword accesses. */
static void
blend_row_fast(uint8_t *d, const uint8_t *s, int w, const uint8_t *shade) {
  while (w >= 4) {
    uint32_t v = get_long(s);
    if (((v - 0x02020202u) & ~v & 0x80808080u) == 0) {
      put_long(d, v);
    } else {
      blend_row_ref(d, s, 4, shade);
    }
    s += 4;
    d += 4;
    w -= 4;
  }
  blend_row_ref(d, s, w, shade);
}

/* VERIFY: rows that came out different from the reference version. */
static uint8_t verify_row[4096];

void
VideoAmiga::blend_row(uint8_t *d, const uint8_t *s, int w) {
  if (!verify || w > static_cast<int>(sizeof(verify_row))) {
    blend_row_fast(d, s, w, shade);
    return;
  }
  std::memcpy(verify_row, d, w);
  blend_row_ref(verify_row, s, w, shade);
  blend_row_fast(d, s, w, shade);
  if (std::memcmp(verify_row, d, w) != 0) verify_errors++;
}

/* A whole image row from its runs; s is the same row as pixels, for
   VERIFY. */
void
VideoAmiga::blend_runs(uint8_t *d, const uint8_t *runs, const uint8_t *s,
                       int w, bool to_vram) {
  if (!verify || w > static_cast<int>(sizeof(verify_row))) {
    if (to_vram) {
      blend_runs_row_vram(d, runs, shade);
    } else {
      blend_runs_row(d, runs, shade);
    }
    return;
  }
  std::memcpy(verify_row, d, w);
  blend_row_ref(verify_row, s, w, shade);
  if (to_vram) {
    blend_runs_row_vram(d, runs, shade);
  } else {
    blend_runs_row(d, runs, shade);
  }
  if (std::memcmp(verify_row, d, w) != 0) verify_errors++;
}

/* No transparent and no shade pixel in the frame? Stops at the first
   one, so a frame with transparency costs little to check. */
static bool
is_opaque(const Video::Frame *frame) {
  const uint8_t *p = frame->pixels;
  uint32_t n = frame->w * frame->h;
  while (n >= 4) {
    uint32_t v = get_long(p);
    if (((v - 0x02020202u) & ~v & 0x80808080u) != 0) return false;
    p += 4;
    n -= 4;
  }
  while (n-- > 0) {
    if (*p++ <= VideoAmiga::kShade) return false;
  }
  return true;
}

/* A plain copy of a row, sixteen bytes a turn. */
static void
copy_row_fast(uint8_t *d, const uint8_t *s, int w) {
  while (w >= 16) {
    put_long(d, get_long(s));
    put_long(d + 4, get_long(s + 4));
    put_long(d + 8, get_long(s + 8));
    put_long(d + 12, get_long(s + 12));
    s += 16;
    d += 16;
    w -= 16;
  }
  while (w >= 4) {
    put_long(d, get_long(s));
    s += 4;
    d += 4;
    w -= 4;
  }
  while (w-- > 0) *d++ = *s++;
}

/* A row of an opaque frame; VERIFY compares it with the reference blend. */
void
VideoAmiga::copy_row(uint8_t *d, const uint8_t *s, int w) {
  if (!verify || w > static_cast<int>(sizeof(verify_row))) {
    copy_row_fast(d, s, w);
    return;
  }
  std::memcpy(verify_row, d, w);
  blend_row_ref(verify_row, s, w, shade);
  copy_row_fast(d, s, w);
  if (std::memcmp(verify_row, d, w) != 0) verify_errors++;
}

/* Clips a w x h block at x, y to the destination; returns false when
   nothing is left. sx, sy are moved by what was cut off. */
static bool
clip(int *x, int *y, int *w, int *h, int *sx, int *sy,
     const Video::Frame *dest) {
  if (*x < 0) { *sx -= *x; *w += *x; *x = 0; }
  if (*y < 0) { *sy -= *y; *h += *y; *y = 0; }
  if (*x + *w > static_cast<int>(dest->w)) *w = dest->w - *x;
  if (*y + *h > static_cast<int>(dest->h)) *h = dest->h - *y;
  return *w > 0 && *h > 0;
}

void
VideoAmiga::draw_image(const Video::Image *image, int x, int y, int y_offset,
                       Video::Frame *dest) {
  if (vram && verify && dest == screen && !to_fast) {
    to_fast = true;
    draw_image(image, x, y, y_offset, dest);
    to_fast = false;
  }
  VramRelease release(this);
  ProfScope prof(this, ProfImage, image->w * (image->h - y_offset));
  touch(dest);
  int w = image->w;
  int h = image->h - y_offset;
  int sx = 0;
  int sy = y_offset;
  y += y_offset;
  if (!clip(&x, &y, &w, &h, &sx, &sy, dest)) return;

  uint8_t *d = row_ptr(dest, x, y);
  int stride = row_stride(dest);
  if (profile) {
    if (sx == 0 && w == static_cast<int>(image->w)) {
      prof_img_runs_path++;
      prof_runs += image->n_runs;
      prof_copy_px += image->n_copy;
      prof_shade_px += image->n_shade;
      prof_box_px += image->w * image->h;
    } else {
      prof_img_clip_path++;
    }
  }
  if (sx == 0 && w == static_cast<int>(image->w)) {
    /* Not cut at the sides: from the runs. */
    if (!verify) {
      if (in_vram(dest)) {
        draw_runs<true>(d, stride, image->runs + image->rows[sy], h, shade);
      } else {
        draw_runs<false>(d, stride, image->runs + image->rows[sy], h, shade);
      }
      return;
    }
    /* VERIFY: draw_runs row by row against the reference blend. */
    for (int j = 0; j < h; j++, d += stride) {
      const uint8_t *runs = image->runs + image->rows[sy + j];
      if (w > static_cast<int>(sizeof(verify_row))) {
        blend_runs(d, runs, image->pixels + (sy + j) * image->w, w,
                   in_vram(dest));
        continue;
      }
      std::memcpy(verify_row, d, w);
      blend_row_ref(verify_row, image->pixels + (sy + j) * image->w, w,
                    shade);
      if (in_vram(dest)) {
        draw_runs<true>(d, stride, runs, 1, shade);
      } else {
        draw_runs<false>(d, stride, runs, 1, shade);
      }
      if (std::memcmp(verify_row, d, w) != 0) verify_errors++;
    }
    return;
  }
  for (int j = 0; j < h; j++, d += stride) {
    blend_row(d, image->pixels + (sy + j) * image->w + sx, w);
  }
}

void
VideoAmiga::draw_frame(int dx, int dy, Video::Frame *dest, int sx, int sy,
                       Video::Frame *src, int w, int h) {
  if (vram && verify && dest == screen && !to_fast) {
    to_fast = true;
    draw_frame(dx, dy, dest, sx, sy, src, w, h);
    to_fast = false;
  }
  VramRelease release(this);
  ProfScope prof(this, ProfFrame, w * h);
  touch(dest);
  if (sx < 0) { dx -= sx; w += sx; sx = 0; }
  if (sy < 0) { dy -= sy; h += sy; sy = 0; }
  if (sx + w > static_cast<int>(src->w)) w = src->w - sx;
  if (sy + h > static_cast<int>(src->h)) h = src->h - sy;
  if (!clip(&dx, &dy, &w, &h, &sx, &sy, dest)) return;

  if (src->changed) {
    src->opaque = is_opaque(src);
    src->changed = false;
  }
  if (src->opaque) {
    if (profile) {
      prof_opaque_calls++;
      prof_opaque_pixels += w * h;
    }
    if (in_vram(dest) && !src->vram_failed) {
      /* A landscape tile: by the card's blitter from its copy in the
         card's memory, uploaded again only after it was drawn into. The
         back bitmap is unlocked first: allocating, uploading and
         blitting on the card wait for that lock (0.18 stopped here). */
      release_vram();
      if (src->vram == nullptr) {
        src->vram = amiga_tile_new(src->w, src->h);
        src->vram_failed = src->vram == nullptr;
        src->vram_stale = true;
      }
      if (src->vram != nullptr) {
        if (src->vram_stale) {
          amiga_tile_upload(src->vram, src->pixels, src->w, src->w, src->h);
          src->vram_stale = false;
        }
        amiga_tile_blit(src->vram, sx, sy, dx, dy, w, h);
        vram_unlocked();
        return;
      }
    }
    uint8_t *d = row_ptr(dest, dx, dy);
    int stride = row_stride(dest);
    for (int j = 0; j < h; j++, d += stride) {
      copy_row(d, src->pixels + (sy + j) * src->w + sx, w);
    }
    return;
  }
  uint8_t *d = row_ptr(dest, dx, dy);
  int stride = row_stride(dest);
  for (int j = 0; j < h; j++, d += stride) {
    blend_row(d, src->pixels + (sy + j) * src->w + sx, w);
  }
}

void
VideoAmiga::draw_frame_scaled(int dx, int dy, int dw, int dh,
                              Video::Frame *dest, Video::Frame *src,
                              int w, int h) {
  if (vram && verify && dest == screen && !to_fast) {
    to_fast = true;
    draw_frame_scaled(dx, dy, dw, dh, dest, src, w, h);
    to_fast = false;
  }
  VramRelease release(this);
  ProfScope prof(this, ProfOther, dw * dh);
  touch(dest);
  if (dw <= 0 || dh <= 0) return;
  for (int j = 0; j < dh; j++) {
    int y = dy + j;
    if (y < 0 || y >= static_cast<int>(dest->h)) continue;
    int syy = j * h / dh;
    for (int i = 0; i < dw; i++) {
      int x = dx + i;
      if (x < 0 || x >= static_cast<int>(dest->w)) continue;
      uint8_t v = src->pixels[syy * src->w + i * w / dw];
      uint8_t *d = row_ptr(dest, x, y);
      if (v > kShade) {
        *d = v;
      } else if (v == kShade) {
        *d = shade[*d];
      }
    }
  }
}

void
VideoAmiga::draw_rect(int x, int y, unsigned int width, unsigned int height,
                      const Video::Color color, Video::Frame *dest) {
  fill_rect(x, y, width, 1, color, dest);
  fill_rect(x, y + height - 1, width, 1, color, dest);
  fill_rect(x, y, 1, height, color, dest);
  fill_rect(x + width - 1, y, 1, height, color, dest);
}

void
VideoAmiga::fill_rect(int x, int y, unsigned int width, unsigned int height,
                      const Video::Color color, Video::Frame *dest) {
  if (vram && verify && dest == screen && !to_fast) {
    to_fast = true;
    fill_rect(x, y, width, height, color, dest);
    to_fast = false;
  }
  VramRelease release(this);
  ProfScope prof(this, ProfFill, width * height);
  touch(dest);
  int w = width;
  int h = height;
  int sx = 0;
  int sy = 0;
  if (!clip(&x, &y, &w, &h, &sx, &sy, dest)) return;
  uint8_t v = get_index(color.r, color.g, color.b);
  uint8_t *d = row_ptr(dest, x, y);
  int stride = row_stride(dest);
  for (int j = 0; j < h; j++, d += stride) {
    std::memset(d, v, w);
  }
}

void
VideoAmiga::draw_line(int x, int y, int x1, int y1, const Video::Color color,
                      Video::Frame *dest) {
  if (vram && verify && dest == screen && !to_fast) {
    to_fast = true;
    draw_line(x, y, x1, y1, color, dest);
    to_fast = false;
  }
  VramRelease release(this);
  uint8_t v = get_index(color.r, color.g, color.b);
  touch(dest);
  int dx = std::abs(x1 - x);
  int dy = -std::abs(y1 - y);
  int stx = x < x1 ? 1 : -1;
  int sty = y < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (x >= 0 && y >= 0 && x < static_cast<int>(dest->w) &&
        y < static_cast<int>(dest->h)) {
      *row_ptr(dest, x, y) = v;
    }
    if (x == x1 && y == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x += stx; }
    if (e2 <= dx) { err += dx; y += sty; }
  }
}

void
VideoAmiga::swap_buffers() {
  uint32_t t0 = static_cast<uint32_t>(amiga_now_ms());
  release_vram();
  if (dirty_first <= dirty_last) {
    amiga_set_colors(dirty_first, dirty_last - dirty_first + 1,
                     rgb + dirty_first * 3);
    dirty_first = 256;
    dirty_last = -1;
  }
  if (vram) {
    if (verify) compare_vram();
    put_cursor();
    release_vram();
    amiga_present();
    vram_unlocked();
  } else {
    put_cursor();
    amiga_screen_write(screen->pixels, screen->w, 0, 0, screen->w,
                       screen->h);
  }
  take_cursor();
  release_vram();
  blit_ms += static_cast<uint32_t>(amiga_now_ms()) - t0;
}

void
VideoAmiga::set_cursor(void *data, unsigned int width, unsigned int height) {
  if (cursor != nullptr) {
    destroy_image(cursor);
    cursor = nullptr;
  }
  if (data == nullptr || width > 64 || height > 64) return;
  cursor = create_image(data, width, height);
  amiga_hide_pointer();
}

/* The cursor's hot spot is its centre (8, 8), as the SDL version's. What
   it covers is saved first and put back by take_cursor() after the frame
   was shown, so the frame stays as the game drew it. */
void
VideoAmiga::put_cursor() {
  under_w = 0;
  if (cursor == nullptr) return;
  int x = mouse_x - 8, y = mouse_y - 8;
  int w = cursor->w, h = cursor->h, sx = 0, sy = 0;
  if (!clip(&x, &y, &w, &h, &sx, &sy, screen)) return;
  under_x = x;
  under_y = y;
  under_w = w;
  under_h = h;
  uint8_t *p = row_ptr(screen, x, y);
  int stride = row_stride(screen);
  for (int j = 0; j < h; j++, p += stride) {
    std::memcpy(under + j * w, p, w);
  }
  /* Not part of the game's frame: no VERIFY copy, no probes. */
  bool v = verify, pr = profile;
  verify = profile = false;
  draw_image(cursor, mouse_x - 8, mouse_y - 8, 0, screen);
  verify = v;
  profile = pr;
}

void
VideoAmiga::take_cursor() {
  if (under_w == 0) return;
  uint8_t *p = row_ptr(screen, under_x, under_y);
  int stride = row_stride(screen);
  for (int j = 0; j < under_h; j++, p += stride) {
    std::memcpy(p, under + j * under_w, under_w);
  }
  under_w = 0;
}

/* VERIFY with VRAM: rows of the back bitmap that differ from the same
   frame drawn in fast RAM. */
void
VideoAmiga::compare_vram() {
  static uint8_t row[4096];
  if (screen->w > sizeof(row)) return;
  for (unsigned int y = 0; y < screen->h; y++) {
    amiga_back_read(0, y, screen->w, 1, row, screen->w);
    if (std::memcmp(row, screen->pixels + y * screen->w, screen->w) != 0) {
      verify_errors++;
    }
  }
  amiga_back_unlock();
}

bool
VideoAmiga::save_screenshot(const char *path) {
  FILE *f = fopen(path, "wb");
  if (f == nullptr) return false;
  fprintf(f, "P6\n%u %u\n255\n", screen->w, screen->h);
  uint8_t row[3 * 2048];
  static uint8_t back_row[2048];
  for (unsigned int y = 0; y < screen->h; y++) {
    const uint8_t *p = screen->pixels + y * screen->w;
    if (vram && screen->w <= sizeof(back_row)) {
      amiga_back_read(0, y, screen->w, 1, back_row, screen->w);
      amiga_back_unlock();
      p = back_row;
    }
    for (unsigned int x = 0; x < screen->w && x < 2048; x++) {
      std::memcpy(row + 3 * x, rgb + 3 * p[x], 3);
    }
    fwrite(row, 3, screen->w, f);
  }
  fclose(f);
  return true;
}

uint8_t
VideoAmiga::get_index(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t key = (static_cast<uint32_t>(r) << 16) |
                 (static_cast<uint32_t>(g) << 8) | b;
  auto it = color_index.find(key);
  if (it != color_index.end()) return it->second;
  return add_color(r, g, b, true);
}

/* A new palette entry for the colour, and with with_shade one for its
   darker twin (the colour under a shadow). When the palette is full the
   nearest colour is used. */
uint8_t
VideoAmiga::add_color(uint8_t r, uint8_t g, uint8_t b, bool with_shade) {
  uint32_t key = (static_cast<uint32_t>(r) << 16) |
                 (static_cast<uint32_t>(g) << 8) | b;
  if (colors_used >= 256) {
    uint8_t n = nearest(r, g, b);
    color_index[key] = n;
    return n;
  }
  uint8_t index = static_cast<uint8_t>(colors_used++);
  rgb[index * 3 + 0] = r;
  rgb[index * 3 + 1] = g;
  rgb[index * 3 + 2] = b;
  color_index[key] = index;
  dirty_first = std::min(dirty_first, static_cast<int>(index));
  dirty_last = std::max(dirty_last, static_cast<int>(index));

  /* Half alpha black over the colour: half the colour. */
  uint8_t dr = r / 2, dg = g / 2, db = b / 2;
  uint32_t dkey = (static_cast<uint32_t>(dr) << 16) |
                  (static_cast<uint32_t>(dg) << 8) | db;
  auto it = color_index.find(dkey);
  if (it != color_index.end()) {
    shade[index] = it->second;
  } else if (with_shade) {
    shade[index] = add_color(dr, dg, db, false);
  } else {
    shade[index] = nearest(dr, dg, db);
  }
  return index;
}

uint8_t
VideoAmiga::nearest(uint8_t r, uint8_t g, uint8_t b) const {
  unsigned int best = 2;
  unsigned int best_d = ~0u;
  for (unsigned int i = 2; i < colors_used; i++) {
    int er = rgb[i * 3 + 0] - r;
    int eg = rgb[i * 3 + 1] - g;
    int eb = rgb[i * 3 + 2] - b;
    unsigned int d = er * er + eg * eg + eb * eb;
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return static_cast<uint8_t>(best);
}
