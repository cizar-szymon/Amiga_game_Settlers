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
std::string VideoAmiga::start_picture;
unsigned long VideoAmiga::start_picture_ms = 0;
bool VideoAmiga::profile = false;
bool VideoAmiga::verify = false;
int VideoAmiga::zoom_gap = 16;
bool VideoAmiga::render_vram = true;
bool VideoAmiga::lock_per_call = false;
bool VideoAmiga::view_incremental = false;
bool VideoAmiga::view_none = false;
bool VideoAmiga::asm_runs = true;

/* port/draw_runs_68k.c: draw_runs<true> in 68k assembly. */
extern "C" void draw_runs_vram68k(uint8_t *d, int stride, const uint8_t *r,
                                  int h, const uint8_t *shade);
int VideoAmiga::view_kinds = 0xf;
static uint32_t image_serial = 0;

/* The block being drawn again (VideoAmiga::sig_end): every drawing call
   is clipped to it as well as to its destination. */
static int clip_l = 0, clip_t = 0, clip_r = 0x7fff, clip_b = 0x7fff;

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
  , verify_errors(0)
  , frame_no(0)
  , scaled_frame(0)
  , fast_images(false)
  , sig_on(false)
  , replaying(false)
  , sig_other_all(true)
  , sig_cols(0)
  , sig_rows(0)
  , sig_blocks(0)
  , sig_dirty(0)
  , sig_ticks(0)
  , sig_spans(0)
  , sig_calls(0)
  , sig_frames(0) {
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
  /* Until the first frame: the start picture, or at least a black screen
     instead of the default grey colour 0. */
  if (!start_picture.empty() &&
      amiga_show_picture(start_picture.c_str()) == 0) {
    start_picture_ms = amiga_now_ms();
  } else {
    amiga_set_colors(0, 2, rgb);
  }
  screen = create_frame(config_width, config_height);
  vram = render_vram && amiga_vram_open() == 0;
  set_fast_images();
  if (vram) {
    /* Start from a cleared back bitmap, like the fast RAM frame. */
    for (unsigned int y = 0; y < screen->h; y++) {
      std::memset(row_ptr(screen, 0, y), kTransparent, screen->w);
    }
    release_vram();
  }
}

static inline uint32_t
sig_mix(uint32_t h, uint32_t v) {
  h ^= v;
  h *= 0x01000193u;
  return h ^ (h >> 15);
}

void
VideoAmiga::sig_begin() {
  int cols = (screen->w + kSigBlock - 1) / kSigBlock;
  int rows = (screen->h + kSigBlock - 1) / kSigBlock;
  if (cols != sig_cols || rows != sig_rows) {
    sig_cols = cols;
    sig_rows = rows;
    sig_prev.assign(cols * rows, 0);
    sig_other.assign(cols * rows, 1);
    row_ops.assign(rows, std::vector<uint32_t>());
  }
  sig_cur.assign(cols * rows, 0x811c9dc5u);
  view_ops.clear();
  sig_on = true;
}

/* Something not of the view drawn on the screen: its blocks are drawn
   again with the next view. */
void
VideoAmiga::sig_mark_other(int x, int y, int w, int h) {
  if (sig_other.empty() || w <= 0 || h <= 0) return;
  int c0 = x / kSigBlock, c1 = (x + w - 1) / kSigBlock;
  int r0 = y / kSigBlock, r1 = (y + h - 1) / kSigBlock;
  if (c1 >= sig_cols) c1 = sig_cols - 1;
  if (r1 >= sig_rows) r1 = sig_rows - 1;
  for (int r = r0; r <= r1; r++) {
    uint8_t *p = &sig_other[r * sig_cols];
    for (int c = c0; c <= c1; c++) p[c] = 1;
  }
}

bool
VideoAmiga::view_record(const ViewOp &op, uint32_t sig) {
  if (replaying || to_fast) return false;
  if (!sig_on) {
    sig_mark_other(op.bx, op.by, op.bw, op.bh);
    return false;
  }
  sig_add(op.bx, op.by, op.bw, op.bh, sig);
  if (!view_incremental) return false;
  view_ops.push_back(op);
  return true;
}

void
VideoAmiga::replay(const ViewOp &op) {
  switch (op.kind) {
    case OpImage:
      draw_image(static_cast<const Video::Image *>(op.src), op.a, op.b, op.c,
                 screen);
      break;
    case OpFrame:
      draw_frame(op.a, op.b, screen, op.c, op.d,
                 static_cast<Video::Frame *>(const_cast<void *>(op.src)),
                 op.e, op.f);
      break;
    case OpFill:
      fill_rect(op.a, op.b, op.c, op.d, op.color, screen);
      break;
    default:
      draw_line(op.a, op.b, op.c, op.d, op.color, screen);
      break;
  }
}

void
VideoAmiga::sig_end() {
  if (!sig_on) return;
  sig_on = false;
  uint32_t n = static_cast<uint32_t>(sig_cur.size());
  uint8_t *dirty = &sig_other[0];
  for (uint32_t i = 0; i < n; i++) {
    if (sig_cur[i] != sig_prev[i] || sig_other_all) dirty[i] = 1;
    sig_dirty += dirty[i];
  }
  sig_other_all = false;
  sig_blocks += n;
  sig_prev.swap(sig_cur);
  if (!view_incremental) {
    std::memset(dirty, 0, n);
    return;
  }

  /* The calls by rows of blocks, then each run of dirty blocks of a row
     drawn again from them. */
  for (int r = 0; r < sig_rows; r++) row_ops[r].clear();
  for (size_t i = 0; i < view_ops.size(); i++) {
    const ViewOp &op = view_ops[i];
    int r0 = op.by / kSigBlock, r1 = (op.by + op.bh - 1) / kSigBlock;
    for (int r = r0; r <= r1; r++) {
      row_ops[r].push_back(static_cast<uint32_t>(i));
    }
  }
  uint32_t t0 = static_cast<uint32_t>(amiga_eclock());
  replaying = true;
  for (int r = 0; r < sig_rows && !view_none; r++) {
    const uint8_t *row = dirty + r * sig_cols;
    const std::vector<uint32_t> &ops = row_ops[r];
    for (int c = 0; c < sig_cols;) {
      if (!row[c]) {
        c++;
        continue;
      }
      int c0 = c;
      while (c < sig_cols && row[c]) c++;
      clip_l = c0 * kSigBlock;
      clip_r = std::min(static_cast<int>(screen->w), c * kSigBlock);
      clip_t = r * kSigBlock;
      clip_b = std::min(static_cast<int>(screen->h), (r + 1) * kSigBlock);
      sig_spans++;
      for (size_t k = 0; k < ops.size(); k++) {
        const ViewOp &op = view_ops[ops[k]];
        if (op.bx < clip_r && op.bx + op.bw > clip_l &&
            (view_kinds & (1 << op.kind))) {
          replay(op);
          sig_calls++;
        }
      }
    }
  }
  replaying = false;
  sig_ticks += static_cast<uint32_t>(amiga_eclock()) - t0;
  sig_frames++;
  clip_l = clip_t = 0;
  clip_r = clip_b = 0x7fff;
  std::memset(dirty, 0, n);
}

/* A drawing call on x, y, w, h (clipped) into the blocks it covers. */
void
VideoAmiga::sig_add(int x, int y, int w, int h, uint32_t op) {
  int c0 = x / kSigBlock, c1 = (x + w - 1) / kSigBlock;
  int r0 = y / kSigBlock, r1 = (y + h - 1) / kSigBlock;
  for (int r = r0; r <= r1; r++) {
    uint32_t *p = &sig_cur[r * sig_cols];
    for (int c = c0; c <= c1; c++) p[c] = sig_mix(p[c], op);
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
  set_fast_images();
  if (vram) {
    for (unsigned int y = 0; y < screen->h; y++) {
      std::memset(row_ptr(screen, 0, y), kTransparent, screen->w);
    }
    release_vram();
  }
  dirty_first = 0;
  dirty_last = static_cast<int>(colors_used) - 1;
  sig_cols = 0;
  sig_other_all = true;
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
  image->serial = ++image_serial;
  image->w = width;
  image->h = height;
  image->pixels = new uint8_t[width * height];
  const uint8_t *p = static_cast<const uint8_t *>(data);
  /* The last colours, so that most pixels skip the hash table: an image
     has few colours (a new landscape tile makes hundreds of images). */
  uint32_t memo_key[32];
  uint8_t memo_index[32];
  std::memset(memo_key, 0xff, sizeof(memo_key));
  for (unsigned int i = 0; i < width * height; i++, p += 4) {
    uint8_t a = p[3];
    if (a == 0) {
      image->pixels[i] = kTransparent;
    } else if (a < 0xff && p[0] == 0 && p[1] == 0 && p[2] == 0) {
      image->pixels[i] = kShade;
    } else {
      uint32_t key = (static_cast<uint32_t>(p[2]) << 16) |
                     (static_cast<uint32_t>(p[1]) << 8) | p[0];
      unsigned int slot = (key ^ (key >> 8) ^ (key >> 16)) & 31;
      if (memo_key[slot] != key) {
        memo_key[slot] = key;
        memo_index[slot] = get_index(p[2], p[1], p[0]);
      }
      image->pixels[i] = memo_index[slot];
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
  delete[] image->mask;
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
typedef uint16_t __attribute__((__may_alias__)) Word;

/* A copy run of n pixels: a byte and a word up to a longword boundary,
   longwords, a word and a byte - no loop by bytes (the runs average ~7
   pixels; 0.129). Into VRAM the longwords are aligned (Warp RTG notes). */
static inline void
copy_span(uint8_t *p, const uint8_t *s, unsigned int n) {
  if (n >= 4) {
    if ((reinterpret_cast<uintptr_t>(p) & 1) != 0) {
      *p++ = *s++;
      n--;
    }
    if ((reinterpret_cast<uintptr_t>(p) & 2) != 0) {
      *reinterpret_cast<Word *>(p) = *reinterpret_cast<const Word *>(s);
      p += 2;
      s += 2;
      n -= 2;
    }
    while (n >= 4) {
      put_long(p, get_long(s));
      s += 4;
      p += 4;
      n -= 4;
    }
  }
  if ((n & 2) != 0) {
    *reinterpret_cast<Word *>(p) = *reinterpret_cast<const Word *>(s);
    p += 2;
    s += 2;
  }
  if ((n & 1) != 0) *p = *s;
}

template <bool kVram>
static void
draw_runs(uint8_t *d, int stride, const uint8_t *r, int h,
          const uint8_t *shade) {
  for (; h > 0; h--, d += stride) {
    uint8_t *p = d;
    for (;;) {
      unsigned int code = *r++;
      if (code < RunCopy) {           /* skip */
        p += code + 1;
        continue;
      }
      if (code < RunShade) {          /* copy */
        unsigned int n = code - (RunCopy - 1);
        copy_span(p, r, n);
        p += n;
        r += n;
        continue;
      }
      if (code >= RunEnd) break;      /* end of the row */
      unsigned int n = code - (RunShade - 1);
      {                               /* shade */
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
      }
    }
  }
}

/* One row of an image from its runs, only its columns x0 .. x0+w-1; d is
   the pixel of column x0. VRAM as in draw_runs. */
template <bool kVram>
static void
draw_runs_clip(uint8_t *d, const uint8_t *r, int x0, int w,
               const uint8_t *shade) {
  int px = 0;
  const int x1 = x0 + w;
  while (px < x1) {
    unsigned int code = *r++;
    int n = static_cast<int>(code & 0x3f) + 1;
    if (code >= RunEnd) break;
    int lo = std::max(px, x0), hi = std::min(px + n, x1);
    if (code < RunCopy) {             /* skip */
    } else if (code < RunShade) {     /* copy */
      if (lo < hi) {
        uint8_t *p = d + (lo - x0);
        const uint8_t *s = r + (lo - px);
        copy_span(p, s, static_cast<unsigned int>(hi - lo));
      }
      r += n;
    } else if (lo < hi) {             /* shade */
      uint8_t *p = d + (lo - x0);
      int m = hi - lo;
      if (kVram) {
        uint8_t *a = reinterpret_cast<uint8_t *>(
          reinterpret_cast<uintptr_t>(p) & ~uintptr_t(3));
        unsigned int lead = static_cast<unsigned int>(p - a);
        unsigned int longs = (lead + m + 3) >> 2;
        uint32_t buf[18];
        for (unsigned int i = 0; i < longs; i++) buf[i] = get_long(a + 4 * i);
        uint8_t *b = reinterpret_cast<uint8_t *>(buf) + lead;
        for (int i = 0; i < m; i++) b[i] = shade[b[i]];
        for (unsigned int i = 0; i < longs; i++) put_long(a + 4 * i, buf[i]);
      } else {
        for (int i = 0; i < m; i++) p[i] = shade[p[i]];
      }
    }
    px += n;
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

/* A row into fast RAM by long words: dest = (dest & ~mask) | pixels (a
   transparent pixel is 0). Reading the destination is cheap there, and the
   ground triangles, dithered at the edges between kinds of land, have a
   run for almost every pixel: by the runs a landscape tile took ~50 ms,
   this way a fraction. */
static void
masked_row(uint8_t *d, const uint8_t *s, const uint8_t *m, int w) {
  while (w >= 4) {
    uint32_t mk = get_long(m);
    if (mk == 0xffffffffu) {
      put_long(d, get_long(s));
    } else if (mk != 0) {
      put_long(d, (get_long(d) & ~mk) | get_long(s));
    }
    d += 4;
    s += 4;
    m += 4;
    w -= 4;
  }
  for (; w > 0; w--, d++, s++, m++) {
    if (*m) *d = *s;
  }
}

/* Clips a w x h block at x, y to the destination; returns false when
   nothing is left. sx, sy are moved by what was cut off. */
static bool
clip(int *x, int *y, int *w, int *h, int *sx, int *sy,
     const Video::Frame *dest) {
  int l = clip_l, t = clip_t;
  int r = std::min(static_cast<int>(dest->w), clip_r);
  int b = std::min(static_cast<int>(dest->h), clip_b);
  if (*x < l) { *sx += l - *x; *w -= l - *x; *x = l; }
  if (*y < t) { *sy += t - *y; *h -= t - *y; *y = t; }
  if (*x + *w > r) *w = r - *x;
  if (*y + *h > b) *h = b - *y;
  return *w > 0 && *h > 0;
}

void
VideoAmiga::draw_image(const Video::Image *image, int x, int y, int y_offset,
                       Video::Frame *dest) {
  if (fast_images && dest == screen && x >= 0 &&
      x + static_cast<int>(image->w) <= static_cast<int>(dest->w) &&
      touched.empty()) {
    /* The usual case, the short way: the same as below without the test
       modes - cut only at the top and bottom, drawn from the runs. */
    int h = static_cast<int>(image->h) - y_offset;
    int sy = y_offset;
    int yy = y + y_offset;
    if (yy < 0) {
      sy -= yy;
      h += yy;
      yy = 0;
    }
    if (yy + h > static_cast<int>(dest->h)) h = dest->h - yy;
    if (h <= 0) return;
    touch(dest);
    uint8_t *d = row_ptr(dest, x, yy);
    if (vram_base != nullptr) {
      draw_runs_vram68k(d, vram_bpr, image->runs + image->rows[sy], h, shade);
      return;
    }
  }
  if (vram && verify && dest == screen && !to_fast && !replaying) {
    to_fast = true;
    draw_image(image, x, y, y_offset, dest);
    to_fast = false;
  }
  VramRelease release(this);
  ProfScope prof(this, ProfImage, image->w * (image->h - y_offset));
  touch(dest);
  const int x0 = x, y0 = y;
  int w = image->w;
  int h = image->h - y_offset;
  int sx = 0;
  int sy = y_offset;
  y += y_offset;
  if (!clip(&x, &y, &w, &h, &sx, &sy, dest)) return;
  mark_rows(dest, y, h);
  if (view_incremental && dest == screen && !replaying) {
    ViewOp op;
    op.kind = OpImage;
    op.bx = x; op.by = y; op.bw = w; op.bh = h;
    op.src = image;
    op.a = x0; op.b = y0; op.c = y_offset;
    uint32_t s = sig_mix(image->serial, x | (y << 16));
    if (view_record(op, sig_mix(sig_mix(s, sx | (sy << 16)), w | (h << 16)))) return;
  }

  uint8_t *d = row_ptr(dest, x, y);
  int stride = row_stride(dest);
  if (!in_vram(dest) && image->n_shade == 0 &&
      image->n_runs * 4 > image->n_copy) {
    /* Into fast RAM, an image of short runs (under 4 pixels on average:
       the dithered ground triangles of the landscape tiles): masked. A
       sprite of long runs is faster by its runs - the mask goes through
       its whole box (the magnified map view is drawn in fast RAM). */
    if (image->mask == nullptr) {
      Video::Image *im = const_cast<Video::Image *>(image);
      size_t n = static_cast<size_t>(im->w) * im->h;
      im->mask = new uint8_t[n];
      for (size_t i = 0; i < n; i++) {
        im->mask[i] = im->pixels[i] != kTransparent ? 0xff : 0x00;
      }
    }
    for (int j = 0; j < h; j++, d += stride) {
      size_t o = static_cast<size_t>(sy + j) * image->w + sx;
      masked_row(d, image->pixels + o, image->mask + o, w);
    }
    return;
  }
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
      if (in_vram(dest) && asm_runs) {
        draw_runs_vram68k(d, stride, image->runs + image->rows[sy], h, shade);
      } else if (in_vram(dest)) {
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
  if (!verify) {
    /* Cut at the sides (a block of the view drawn again): by the runs. */
    for (int j = 0; j < h; j++, d += stride) {
      const uint8_t *runs = image->runs + image->rows[sy + j];
      if (in_vram(dest)) {
        draw_runs_clip<true>(d, runs, sx, w, shade);
      } else {
        draw_runs_clip<false>(d, runs, sx, w, shade);
      }
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
  if (vram && verify && dest == screen && !to_fast && !replaying) {
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
  mark_rows(dest, dy, h);
  if (view_incremental && dest == screen && !replaying) {
    ViewOp op;
    op.kind = OpFrame;
    op.bx = dx; op.by = dy; op.bw = w; op.bh = h;
    op.src = src;
    op.a = dx; op.b = dy; op.c = sx; op.d = sy; op.e = w; op.f = h;
    uint32_t s = sig_mix(static_cast<uint32_t>(
                           reinterpret_cast<uintptr_t>(src)), src->version);
    s = sig_mix(s, dx | (dy << 16));
    if (view_record(op, sig_mix(sig_mix(s, sx | (sy << 16)), w | (h << 16)))) return;
  }

  if (src->changed) {
    src->opaque = is_opaque(src);
    src->changed = false;
  }
  if (src->opaque) {
    if (profile) {
      prof_opaque_calls++;
      prof_opaque_pixels += w * h;
    }
    if (in_vram(dest) && !src->vram_failed && !replaying) {
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

/* The widest magnified row: a 1280x720 screen, x3 of a view 427 wide is
   1281 (the rest clipped). */
static const int kWideMax = 1296;

void
VideoAmiga::draw_frame_scaled(int dx, int dy, int dw, int dh,
                              Video::Frame *dest, Video::Frame *src,
                              int w, int h) {
  if (vram && verify && dest == screen && !to_fast && !replaying) {
    to_fast = true;
    draw_frame_scaled(dx, dy, dw, dh, dest, src, w, h);
    to_fast = false;
  }
  VramRelease release(this);
  ProfScope prof(this, ProfOther, dw * dh);
  touch(dest);
  if (dest == screen && !to_fast) sig_other_all = true;
  if (dw <= 0 || dh <= 0) return;
  int f = (w > 0 && dw % w == 0) ? dw / w : 0;
  if ((f == 2 || f == 3) && dh == f * h && dw <= kWideMax) {
    /* The map view magnified 2 or 3 times (Z, the wheel): a row widened
       into a buffer and copied to f rows by memcpy. Into the card's
       memory only what changed since the last frame (all of an 800x600
       screen took 16-21 ms a frame): a row the same as last time is left
       out, of a changed one the part from its first to its last change;
       rows something else was drawn on since are sent whole. A row with a
       transparent or shade pixel goes pixel by pixel. */
    uint32_t wide_longs[kWideMax / 4];
    uint8_t *wide = reinterpret_cast<uint8_t *>(wide_longs);
    int x0 = std::max(0, dx), x1 = std::min(static_cast<int>(dest->w), dx + dw);
    bool diff = in_vram(dest);
    if (diff) {
      size_t n = static_cast<size_t>(w) * h;
      if (touched.size() != dest->h || scaled_prev.size() != n ||
          scaled_frame + 1 != frame_no) {
        /* No picture of the last frame to compare with: all of it. */
        touched.assign(dest->h, 1);
        scaled_prev.assign(n, 0);
      }
      scaled_frame = frame_no;
    }
    /* The parts of a changed row to send: pieces of kPiece source pixels
       that changed, joined when less than zoom_gap pixels apart (0.104; the
       part from the first to the last change sent all the pixels between
       two serfs at the ends of a row). */
    static const int kPiece = 8;
    static const int kMaxSpans = 64;
    int spans[kMaxSpans][2];
    for (int j = 0; j < h && x0 < x1; j++) {
      const uint8_t *s = src->pixels + j * src->w;
      int nspans = 1;
      spans[0][0] = 0;
      spans[0][1] = w - 1;
      if (diff) {
        uint8_t *prev = &scaled_prev[static_cast<size_t>(j) * w];
        bool whole = false;
        for (int k = 0; k < f; k++) {
          int y = dy + f * j + k;
          if (y >= 0 && y < static_cast<int>(dest->h) && touched[y]) {
            whole = true;
          }
        }
        if (!whole) {
          if (std::memcmp(prev, s, w) == 0) continue;
          int a = 0, b = w - 1;
          while (prev[a] == s[a]) a++;
          while (prev[b] == s[b]) b--;
          nspans = 0;
          int start = a, end = a - 1;
          for (int i = a; i <= b; i += kPiece) {
            int n = std::min(kPiece, b + 1 - i);
            if (std::memcmp(prev + i, s + i, n) == 0) continue;
            if (i - end > zoom_gap && end >= start && nspans < kMaxSpans - 1) {
              spans[nspans][0] = start;
              spans[nspans][1] = end;
              nspans++;
              start = i;
            }
            end = i + n - 1;
          }
          spans[nspans][0] = start;
          spans[nspans][1] = std::min(end, b);
          nspans++;
        }
        std::memcpy(prev, s, w);
      }
      for (int p = 0; p < nspans; p++) {
        int a = spans[p][0], b = spans[p][1];
        bool plain = true;
        for (int i = a; i <= b; i++) {
          uint8_t v = s[i];
          if (v <= kShade) {
            plain = false;
            break;
          }
          if (f == 2) {
            reinterpret_cast<uint16_t *>(wide)[i] =
              static_cast<uint16_t>((v << 8) | v);
          } else {
            wide[3 * i] = wide[3 * i + 1] = wide[3 * i + 2] = v;
          }
        }
        int sx0 = std::max(x0, dx + f * a);
        int sx1 = std::min(x1, dx + f * (b + 1));
        if (sx0 >= sx1) continue;
        for (int k = 0; k < f; k++) {
          int y = dy + f * j + k;
          if (y < 0 || y >= static_cast<int>(dest->h)) continue;
          if (plain) {
            std::memcpy(row_ptr(dest, sx0, y), wide + (sx0 - dx), sx1 - sx0);
            continue;
          }
          for (int x = sx0; x < sx1; x++) {
            uint8_t v = s[(x - dx) / f];
            uint8_t *d = row_ptr(dest, x, y);
            if (v > kShade) {
              *d = v;
            } else if (v == kShade) {
              *d = shade[*d];
            }
          }
        }
      }
    }
    if (diff) {
      /* What is drawn over the view from now on marks its rows again. */
      std::fill(touched.begin(), touched.end(), 0);
    }
    return;
  }
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
  if (vram && verify && dest == screen && !to_fast && !replaying) {
    to_fast = true;
    fill_rect(x, y, width, height, color, dest);
    to_fast = false;
  }
  VramRelease release(this);
  ProfScope prof(this, ProfFill, width * height);
  touch(dest);
  const int x0 = x, y0 = y;
  int w = width;
  int h = height;
  int sx = 0;
  int sy = 0;
  if (!clip(&x, &y, &w, &h, &sx, &sy, dest)) return;
  mark_rows(dest, y, h);
  uint8_t v = get_index(color.r, color.g, color.b);
  if (view_incremental && dest == screen && !replaying) {
    ViewOp op;
    op.kind = OpFill;
    op.bx = x; op.by = y; op.bw = w; op.bh = h;
    op.src = nullptr;
    op.a = x0; op.b = y0; op.c = width; op.d = height;
    op.color = color;
    if (view_record(op, sig_mix(sig_mix(v, x | (y << 16)),
                                w | (h << 16)))) return;
  }
  uint8_t *d = row_ptr(dest, x, y);
  int stride = row_stride(dest);
  for (int j = 0; j < h; j++, d += stride) {
    std::memset(d, v, w);
  }
}

void
VideoAmiga::draw_line(int x, int y, int x1, int y1, const Video::Color color,
                      Video::Frame *dest) {
  if (vram && verify && dest == screen && !to_fast && !replaying) {
    to_fast = true;
    draw_line(x, y, x1, y1, color, dest);
    to_fast = false;
  }
  VramRelease release(this);
  uint8_t v = get_index(color.r, color.g, color.b);
  touch(dest);
  mark_rows(dest, std::min(y, y1), std::abs(y1 - y) + 1);
  if (view_incremental && dest == screen && !replaying) {
    int bx = std::max(0, std::min(x, x1)), by = std::max(0, std::min(y, y1));
    int ex = std::min(static_cast<int>(dest->w) - 1, std::max(x, x1));
    int ey = std::min(static_cast<int>(dest->h) - 1, std::max(y, y1));
    if (bx > ex || by > ey) return;
    ViewOp op;
    op.kind = OpLine;
    op.bx = bx; op.by = by; op.bw = ex - bx + 1; op.bh = ey - by + 1;
    op.src = nullptr;
    op.a = x; op.b = y; op.c = x1; op.d = y1;
    op.color = color;
    if (view_record(op, sig_mix(sig_mix(v, x | (y << 16)),
                                x1 | (y1 << 16)))) return;
  }
  int dx = std::abs(x1 - x);
  int dy = -std::abs(y1 - y);
  int stx = x < x1 ? 1 : -1;
  int sty = y < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (x >= std::max(0, clip_l) && y >= std::max(0, clip_t) &&
        x < std::min(static_cast<int>(dest->w), clip_r) &&
        y < std::min(static_cast<int>(dest->h), clip_b)) {
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
  frame_no++;
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
bool
VideoAmiga::frames_equal(const Video::Frame *a, const Video::Frame *b) {
  return a->w == b->w && a->h == b->h &&
         std::memcmp(a->pixels, b->pixels, a->w * a->h) == 0;
}

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
