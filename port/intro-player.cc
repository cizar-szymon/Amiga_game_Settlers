/*
 * intro-player.cc - the original Amiga intro replayed from its script
 * (see intro-player.h; the reference is tools/intro/player_ref.py)
 */

#include "port/intro-player.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/* Guard bytes before and after every block, as in player_ref.py (PAD). */
const int kPad = 64;

/* Records of intro.script (tools/intro/make_script.py). */
enum {
  kFrame = 0x01, kIrq = 0x02, kMain = 0x03,
  kAlloc = 0x10, kLoad = 0x11, kFill = 0x12, kCopy = 0x13, kPoke = 0x14,
  kUnpack = 0x15, kDelta = 0x16, kBlit = 0x17, kSfxBank = 0x18,
  kSfx = 0x19, kSfxOn = 0x1A, kSfxOff = 0x1B, kMusic = 0x1C,
  kScene = 0x1D, kSfxStop = 0x1E, kView = 0x20
};

inline unsigned be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
inline uint32_t be32(const uint8_t *p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) | (p[2] << 8) | p[3];
}
inline unsigned rd16(const uint8_t *&p) {
  unsigned v = be16(p);
  p += 2;
  return v;
}
inline uint32_t rd32(const uint8_t *&p) {
  uint32_t v = be32(p);
  p += 4;
  return v;
}

/* Words of the Amiga bitmaps (always at even addresses: the blocks are
   16-byte aligned and the blitter ignores bit 0). On the 68k a plain word
   access; elsewhere byte by byte. */
typedef uint16_t __attribute__((may_alias)) alias16;
typedef uint32_t __attribute__((may_alias)) alias32;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline unsigned word_at(const uint8_t *p) {
  return *reinterpret_cast<const alias16 *>(p);
}
inline void word_to(uint8_t *p, unsigned v) {
  *reinterpret_cast<alias16 *>(p) = static_cast<uint16_t>(v);
}
#else
inline unsigned word_at(const uint8_t *p) { return (p[0] << 8) | p[1]; }
inline void word_to(uint8_t *p, unsigned v) {
  p[0] = static_cast<uint8_t>(v >> 8);
  p[1] = static_cast<uint8_t>(v);
}
#endif

/* Planar to chunky: the 8 pixels of a plane byte as 8 bytes of 0 or 1 (two
   longs in the host's order), shifted by the plane number and or-ed. */
uint32_t c2p_tab[256][2];
bool c2p_ready = false;

void
c2p_init() {
  if (c2p_ready) return;
  for (int b = 0; b < 256; b++) {
    uint8_t px[8];
    for (int i = 0; i < 8; i++) px[i] = (b >> (7 - i)) & 1;
    std::memcpy(&c2p_tab[b][0], px, 4);
    std::memcpy(&c2p_tab[b][1], px + 4, 4);
  }
  c2p_ready = true;
}

/* The intro's RLE: byte 0 the escape E, then E n v -> n + 1 times v,
   other bytes as they are (tools/intro/introlib.py). n = 0: only the
   size. */
size_t
unrle(const uint8_t *src, size_t len, uint8_t *dst, size_t max) {
  size_t out = 0;
  if (len == 0) return 0;
  uint8_t esc = src[0];
  size_t i = 1;
  while (i < len) {
    uint8_t b = src[i];
    if (b == esc) {
      if (i + 2 >= len) break;
      size_t cnt = static_cast<size_t>(src[i + 1]) + 1;
      if (dst != nullptr) {
        size_t n = out + cnt <= max ? cnt : (out < max ? max - out : 0);
        std::memset(dst + out, src[i + 2], n);
      }
      out += cnt;
      i += 3;
    } else {
      if (dst != nullptr && out < max) dst[out] = b;
      out++;
      i++;
    }
  }
  return out;
}

/* ByteRun1 (DISP $111c): n bytes from src. */
void
byterun1(const uint8_t *src, const uint8_t *src_end, uint8_t *dst,
         size_t n) {
  size_t out = 0;
  while (out < n && src < src_end) {
    int c = *src++;
    if (c < 128) {
      size_t k = static_cast<size_t>(c) + 1;
      if (k > n - out) k = n - out;
      if (src + k > src_end) break;
      std::memcpy(dst + out, src, k);
      src += c + 1;
      out += k;
    } else if (c > 128) {
      size_t k = static_cast<size_t>(257 - c);
      if (k > n - out) k = n - out;
      std::memset(dst + out, *src++, k);
      out += k;
    }
  }
}

}  // namespace

IntroPlayer::IntroPlayer()
  : script(nullptr), script_size(0), pos(0), nframes(0), cur_frame(0),
    arena(nullptr), block(nullptr), block_size(nullptr), nblocks(0),
    nmodules(0), musi(nullptr), musi_size(0), music_ev(MusicNone),
    nbands(0), dirty(true), sfx_bank(nullptr), sfx_on(false),
    blit_tmp(nullptr) {
  err[0] = 0;
  dir[0] = 0;
  std::memset(module_data, 0, sizeof(module_data));
  std::memset(voices, 0, sizeof(voices));
  /* SD00's volume table: floor(s * vol / 64) / 3, the division towards
     zero (muls, swap, divs #3). */
  for (int vol = 0; vol < 64; vol++) {
    for (int b = 0; b < 256; b++) {
      int s = b > 127 ? b - 256 : b;
      int v = (s * vol * 1024) >> 16;
      int q = (v < 0 ? -v : v) / 3;
      vol_tab[vol][b] = static_cast<int8_t>(v >= 0 ? q : -q);
    }
  }
}

IntroPlayer::~IntroPlayer() {
  std::free(script);
  std::free(arena);
  delete[] block;
  delete[] block_size;
  for (int i = 0; i < nmodules; i++) std::free(module_data[i]);
  std::free(musi);
  delete[] blit_tmp;
}

bool
IntroPlayer::fail(const char *fmt, const char *arg) {
  std::snprintf(err, sizeof(err), fmt, arg);
  return false;
}

/* A whole file of the intro directory; rle: unpacked. */
bool
IntroPlayer::read_file(const char *name, uint8_t **data, size_t *size,
                       bool rle) {
  char path[300];
  std::snprintf(path, sizeof(path), "%s/%s", dir, name);
  FILE *f = std::fopen(path, "rb");
  if (f == nullptr) return fail("%s not found", path);
  std::fseek(f, 0, SEEK_END);
  long len = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  uint8_t *raw = static_cast<uint8_t *>(std::malloc(len > 0 ? len : 1));
  bool ok = raw != nullptr && len >= 0 &&
            std::fread(raw, 1, len, f) == static_cast<size_t>(len);
  std::fclose(f);
  if (!ok) {
    std::free(raw);
    return fail("cannot read %s", path);
  }
  if (!rle) {
    *data = raw;
    *size = len;
    return true;
  }
  size_t n = unrle(raw, len, nullptr, 0);
  uint8_t *out = static_cast<uint8_t *>(std::malloc(n + 1));
  if (out == nullptr) {
    std::free(raw);
    return fail("no memory for %s", name);
  }
  unrle(raw, len, out, n);
  std::free(raw);
  *data = out;
  *size = n;
  return true;
}

bool
IntroPlayer::open(const char *path) {
  std::snprintf(dir, sizeof(dir), "%s", path);
  uint8_t *data = nullptr;
  size_t size = 0;
  if (!read_file("intro.script", &data, &size, false)) return false;
  script = data;
  script_size = size;
  if (size < 12 || std::memcmp(script, "STIS", 4) != 0 ||
      be16(script + 4) != 1) {
    return fail("%s/intro.script: not a version 1 intro script", dir);
  }
  nblocks = be16(script + 6);
  nframes = static_cast<int>(be32(script + 8));
  if (size < 12 + 4 * static_cast<size_t>(nblocks)) {
    return fail("%s/intro.script: cut short", dir);
  }
  block = new uint8_t *[nblocks];
  block_size = new uint32_t[nblocks];
  size_t total = 0;
  for (int b = 0; b < nblocks; b++) {
    block_size[b] = be32(script + 12 + 4 * b);
    if (block_size[b] > 4 * 1024 * 1024) {
      return fail("%s/intro.script: a block too large", dir);
    }
    total += kPad + ((block_size[b] + 15) & ~15u) + kPad;
  }
  /* All the blocks the original allocates, at once (2.7 MB). */
  arena = static_cast<uint8_t *>(std::calloc(total + 16, 1));
  if (arena == nullptr) return fail("no memory for the intro%s");
  uint8_t *a = reinterpret_cast<uint8_t *>(
    (reinterpret_cast<uintptr_t>(arena) + 15) & ~static_cast<uintptr_t>(15));
  for (int b = 0; b < nblocks; b++) {
    block[b] = a + kPad;
    a += kPad + ((block_size[b] + 15) & ~15u) + kPad;
  }
  blit_tmp = new uint16_t[64 * 1024];
  pos = 12 + 4 * nblocks;
  if (!check_script()) return false;
  if (!read_file("MUSI", &data, &size, true)) return false;
  musi = reinterpret_cast<int8_t *>(data);
  musi_size = size;
  c2p_init();
  return true;
}

/* A pointer of a record: u16 block + s32 offset, or 0xFFFF + module name
   + u32 offset. */
uint8_t *
IntroPlayer::pointer(const uint8_t *&p) {
  unsigned b = rd16(p);
  if (b == 0xFFFF) {
    const uint8_t *name = p;
    p += 4;
    uint32_t off = rd32(p);
    for (int i = 0; i < nmodules; i++) {
      if (std::memcmp(module_name[i], name, 4) == 0) {
        return module_data[i] + off;
      }
    }
    return module_data[0] + off;  /* checked by check_script() */
  }
  int32_t off = static_cast<int32_t>(rd32(p));
  return block[b] + off;
}

bool
IntroPlayer::skip_pointer(const uint8_t *&p, const char **module) {
  unsigned b = rd16(p);
  *module = nullptr;
  if (b == 0xFFFF) {
    *module = reinterpret_cast<const char *>(p);
    p += 8;
    return true;
  }
  int32_t off = static_cast<int32_t>(rd32(p));
  return b < static_cast<unsigned>(nblocks) && off >= -kPad &&
         off <= static_cast<int32_t>(block_size[b]) + kPad;
}

/* One pass over the whole script: are the records sound, which files and
   modules does it need? Every file is looked for now, so that a missing
   one means no intro at all instead of a broken one. */
bool
IntroPlayer::check_script() {
  const uint8_t *p = script + pos;
  const uint8_t *end = script + script_size;
  int frames = 0;
  char names[64][5];
  int nnames = 0;
  while (p < end) {
    const uint8_t *rec = p;
    int code = *p++;
    int nptr = 0;
    size_t fixed_after = 0;
    switch (code) {
      case kFrame: fixed_after = 2; frames++; break;
      case kIrq: case kMain: case kSfxOn: case kSfxOff: break;
      case kAlloc: fixed_after = 6; break;
      case kLoad: {
        if (end - p < 4) return fail("%s/intro.script: broken", dir);
        bool known = false;
        for (int i = 0; i < nnames; i++) {
          if (std::memcmp(names[i], p, 4) == 0) known = true;
        }
        if (!known && nnames < 64) {
          std::memcpy(names[nnames], p, 4);
          names[nnames++][4] = 0;
        }
        p += 4;
        nptr = 1;
        fixed_after = 4;
        break;
      }
      case kFill: nptr = 1; fixed_after = 5; break;
      case kCopy: nptr = 2; fixed_after = 4; break;
      case kPoke: nptr = 1; break;
      case kUnpack: nptr = 2; fixed_after = 4; break;
      case kDelta: nptr = 2; break;
      case kBlit: p += 18; break;
      case kSfxBank: nptr = 1; break;
      case kSfx: fixed_after = 4; break;
      case kMusic: case kScene: case kSfxStop: fixed_after = 1; break;
      case kView: break;
      default: {
        char at[32];
        std::snprintf(at, sizeof(at), "%lu",
                      static_cast<unsigned long>(rec - script));
        return fail("intro.script: unknown record at %s", at);
      }
    }
    if (p > end) return fail("%s/intro.script: broken", dir);
    const char *module;
    if (code == kBlit) {
      int mask = *p++;
      for (int c = 0; c < 4; c++) {
        if ((mask & (1 << c)) && !skip_pointer(p, &module)) {
          return fail("%s/intro.script: bad pointer", dir);
        }
      }
      p += 6;
    } else if (code == kView) {
      int n = *p++;
      if (n > kMaxBands) return fail("%s/intro.script: too many bands", dir);
      for (int i = 0; i < n; i++) {
        p += 2;
        int depth = *p++;
        if (depth < 1 || depth > 6) {
          return fail("%s/intro.script: bad display depth", dir);
        }
        for (int k = 0; k < depth; k++) {
          if (!skip_pointer(p, &module) || module != nullptr) {
            return fail("%s/intro.script: bad plane pointer", dir);
          }
        }
        p += 14 + 64;
      }
    }
    for (int i = 0; i < nptr; i++) {
      if (!skip_pointer(p, &module)) {
        return fail("%s/intro.script: bad pointer", dir);
      }
      if (module != nullptr) {
        bool known = false;
        for (int m = 0; m < nmodules; m++) {
          if (std::memcmp(module_name[m], module, 4) == 0) known = true;
        }
        if (!known) {
          if (nmodules == kMaxModules) {
            return fail("%s/intro.script: too many modules", dir);
          }
          std::memcpy(module_name[nmodules], module, 4);
          module_name[nmodules][4] = 0;
          uint8_t *data;
          size_t size;
          if (!read_file(module_name[nmodules], &data, &size, true)) {
            return false;
          }
          /* Room after the module, as player_ref.py gives it. */
          uint8_t *more = static_cast<uint8_t *>(
            std::realloc(data, size + 0x1000));
          if (more == nullptr) {
            std::free(data);
            return fail("no memory for %s", module_name[nmodules]);
          }
          std::memset(more + size, 0, 0x1000);
          module_data[nmodules] = more;
          module_size[nmodules] = size + 0x1000;
          nmodules++;
        }
      }
    }
    if (code == kPoke) {
      if (end - p < 2) return fail("%s/intro.script: broken", dir);
      p += 2 + be16(p);
    }
    p += fixed_after;
    if (p > end) return fail("%s/intro.script: broken", dir);
  }
  if (frames != nframes) {
    return fail("%s/intro.script: wrong number of frames", dir);
  }
  for (int i = 0; i < nnames; i++) {
    char path[300];
    std::snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
    FILE *f = std::fopen(path, "rb");
    if (f == nullptr) return fail("%s not found", path);
    std::fclose(f);
  }
  return true;
}

void
IntroPlayer::op_load(const uint8_t *&p) {
  char name[5];
  std::memcpy(name, p, 4);
  name[4] = 0;
  p += 4;
  uint8_t *dst = pointer(p);
  uint32_t n = rd32(p);
  uint8_t *data;
  size_t size;
  /* PICx are ByteRun1 pictures without the disk's RLE. */
  if (!read_file(name, &data, &size, std::strncmp(name, "PIC", 3) != 0)) {
    return;
  }
  std::memcpy(dst, data, size < n ? size : n);
  std::free(data);
}

/* The Amiga blitter in area mode: D = f(A, B, C) with the shifts of A and
   B, the first and last word masks of A, ascending or descending. The
   reference reads all sources before it writes D; where D overlaps a
   source in another way than in place, D goes through a buffer first. */
void
IntroPlayer::op_blit(const uint8_t *&p) {
  unsigned con0 = rd16(p), con1 = rd16(p), size = rd16(p);
  unsigned fwm = rd16(p), lwm = rd16(p);
  int mods[4];
  for (int c = 0; c < 4; c++) mods[c] = static_cast<int16_t>(rd16(p));
  int mask = *p++;
  uint8_t *ptr[4] = { nullptr, nullptr, nullptr, nullptr };
  for (int c = 0; c < 4; c++) {
    if (mask & (1 << c)) ptr[c] = pointer(p);
  }
  unsigned dat[3];
  for (int c = 0; c < 3; c++) dat[c] = rd16(p);

  bool use[4];
  for (int c = 0; c < 4; c++) use[c] = ((con0 >> (11 - c)) & 1) && ptr[c];
  if (!use[3]) return;
  dirty = true;
  int h = (size >> 6) ? static_cast<int>(size >> 6) : 1024;
  int w = (size & 0x3F) ? static_cast<int>(size & 0x3F) : 64;
  bool desc = (con1 & 2) != 0;
  int sgn = desc ? -1 : 1;
  unsigned ash = con0 >> 12, bsh = con1 >> 12, mint = con0 & 0xFF;

  /* Does D run over a source ahead of reading it? */
  bool buffered = false;
  {
    intptr_t dlo, dhi;
    intptr_t lo[4], hi[4];
    for (int c = 0; c < 4; c++) {
      if (!use[c]) continue;
      intptr_t s = reinterpret_cast<intptr_t>(ptr[c]);
      intptr_t last_row = static_cast<intptr_t>(sgn) * (h - 1) * (2 * w + mods[c]);
      intptr_t last_col = static_cast<intptr_t>(sgn) * (w - 1) * 2;
      intptr_t e[4] = { s, s + last_col, s + last_row, s + last_row + last_col };
      lo[c] = hi[c] = s;
      for (int i = 1; i < 4; i++) {
        if (e[i] < lo[c]) lo[c] = e[i];
        if (e[i] > hi[c]) hi[c] = e[i];
      }
      hi[c] += 2;
    }
#ifdef INTRO_DEBUG
    for (int c = 0; c < 4; c++) {
      if (!use[c]) continue;
      bool inside = false;
      for (int b = 0; b < nblocks; b++) {
        intptr_t s = reinterpret_cast<intptr_t>(block[b]);
        if (lo[c] >= s - kPad && hi[c] <= s + block_size[b] + kPad) {
          inside = true;
          if (lo[c] < s || hi[c] > s + static_cast<intptr_t>(block_size[b])) {
            std::fprintf(stderr, "frame %d blit ch %d block %d out by %ld/%ld\n",
                         cur_frame, c, b, static_cast<long>(lo[c] - s),
                         static_cast<long>(hi[c] - s - block_size[b]));
          }
        }
      }
      if (!inside) {
        std::fprintf(stderr, "frame %d blit ch %d outside all blocks\n",
                     cur_frame, c);
      }
    }
#endif
    dlo = lo[3];
    dhi = hi[3];
    for (int c = 0; c < 3; c++) {
      if (!use[c] || dhi <= lo[c] || hi[c] <= dlo) continue;
      /* In place (the same words, rows apart) is safe. */
      if (ptr[c] == ptr[3] && mods[c] == mods[3] && mods[c] >= 0) continue;
      buffered = true;
    }
  }

  int step = 2 * sgn;
  int row_add[4];
  for (int c = 0; c < 4; c++) row_add[c] = sgn * mods[c];
  const uint8_t *pa = ptr[0], *pb = ptr[1], *pc = ptr[2];
  uint8_t *pd = ptr[3];
  unsigned prev_a = 0, prev_b = 0;
  uint16_t *t = blit_tmp;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned a = use[0] ? word_at(pa) : dat[0];
      if (x == 0) a &= fwm;
      if (x == w - 1) a &= lwm;
      unsigned as = a;
      if (ash) {
        as = desc ? ((a << ash) | (prev_a >> (16 - ash))) & 0xFFFF
                  : (((prev_a << 16) | a) >> ash) & 0xFFFF;
      }
      prev_a = a;
      unsigned d;
      if (mint == 0xF0) {
        d = as;
      } else {
        unsigned b = use[1] ? word_at(pb) : dat[1];
        unsigned bs = b;
        if (bsh) {
          bs = desc ? ((b << bsh) | (prev_b >> (16 - bsh))) & 0xFFFF
                    : (((prev_b << 16) | b) >> bsh) & 0xFFFF;
        }
        prev_b = b;
        unsigned c = use[2] ? word_at(pc) : dat[2];
        if (mint == 0xCA) {
          d = (as & bs) | (~as & c);
        } else {
          unsigned na = ~as, nb = ~bs, nc = ~c;
          d = 0;
          if (mint & 0x01) d |= na & nb & nc;
          if (mint & 0x02) d |= na & nb & c;
          if (mint & 0x04) d |= na & bs & nc;
          if (mint & 0x08) d |= na & bs & c;
          if (mint & 0x10) d |= as & nb & nc;
          if (mint & 0x20) d |= as & nb & c;
          if (mint & 0x40) d |= as & bs & nc;
          if (mint & 0x80) d |= as & bs & c;
        }
        d &= 0xFFFF;
      }
      if (buffered) {
        *t++ = static_cast<uint16_t>(d);
      } else {
        word_to(pd, d);
      }
      pa += step;
      pb += step;
      pc += step;
      pd += step;
    }
    pa += row_add[0];
    pb += row_add[1];
    pc += row_add[2];
    pd += row_add[3];
  }
  if (buffered) {
    pd = ptr[3];
    t = blit_tmp;
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        word_to(pd, *t++);
        pd += step;
      }
      pd += row_add[3];
    }
  }
}

void
IntroPlayer::op_view(const uint8_t *&p) {
  nbands = *p++;
  for (int i = 0; i < nbands; i++) {
    Band &b = bands[i];
    b.first_line = static_cast<int>(rd16(p));
    b.depth = *p++;
    for (int k = 0; k < b.depth; k++) b.planes[k] = pointer(p);
    b.con1 = rd16(p);
    b.ddfstrt = rd16(p);
    b.ddfstop = rd16(p);
    b.mod1 = static_cast<int16_t>(rd16(p));
    b.mod2 = static_cast<int16_t>(rd16(p));
    b.diwstrt = rd16(p);
    b.diwstop = rd16(p);
    for (int c = 0; c < 32; c++) b.colours[c] = static_cast<uint16_t>(rd16(p));
  }
  dirty = true;
}

void
IntroPlayer::sfx_start(unsigned k, unsigned vol, unsigned rep) {
  if (sfx_bank == nullptr) return;
  const uint8_t *s = sfx_bank + be32(sfx_bank + 4 * k) + 0x68;
  const uint8_t *e = sfx_bank + be32(sfx_bank + 4 * k + 4);
  for (Voice &v : voices) {
    if (v.ptr == v.end) {
      v.ptr = v.start = s;
      v.end = e;
      v.vol = vol & 0x3F;
      v.cnt = (rep - 1) & 0xFFFF;
      v.used = true;
      return;
    }
  }
}

/* SD00 $558: the three voices summed into one byte (the adds wrap). */
void
IntroPlayer::mix(int8_t *out) {
  int16_t sum[kSfxPerFrame];
  std::memset(sum, 0, sizeof(sum));
  if (sfx_on) {
    for (Voice &v : voices) {
      if (!v.used) continue;
      const int8_t *tab = vol_tab[v.vol];
      for (int i = 0; i < kSfxPerFrame; i++) {
        if (v.ptr == v.end) {
          if (v.cnt == 0) break;
          v.cnt--;
          v.ptr = v.start;
        }
        sum[i] = static_cast<int16_t>(sum[i] + tab[*v.ptr++]);
      }
    }
  }
  for (int i = 0; i < kSfxPerFrame; i++) {
    out[i] = static_cast<int8_t>(static_cast<uint8_t>(sum[i]));
  }
}

bool
IntroPlayer::step(int8_t *sfx) {
  music_ev = MusicNone;
  if (cur_frame >= nframes || pos >= script_size ||
      script[pos] != kFrame) {
    std::memset(sfx, 0, kSfxPerFrame);
    return false;
  }
  const uint8_t *p = script + pos + 3;
  const uint8_t *end = script + script_size;
  bool mixed = false;
  while (p < end && *p != kFrame) {
    int code = *p++;
    switch (code) {
      case kIrq:
        break;
      case kMain:
        /* The effect buffer of this frame comes between the operations of
           the interrupt and those of the main program. */
        if (!mixed) {
          mix(sfx);
          mixed = true;
        }
        break;
      case kAlloc: {
        unsigned b = rd16(p);
        rd32(p);
        std::memset(block[b] - kPad, 0,
                    kPad + ((block_size[b] + 15) & ~15u) + kPad);
        dirty = true;
        break;
      }
      case kLoad:
        op_load(p);
        dirty = true;
        break;
      case kFill: {
        uint8_t *d = pointer(p);
        uint32_t n = rd32(p);
        std::memset(d, *p++, n);
        dirty = true;
        break;
      }
      case kCopy: {
        uint8_t *s = pointer(p);
        uint8_t *d = pointer(p);
        uint32_t n = rd32(p);
        std::memmove(d, s, n);
        dirty = true;
        break;
      }
      case kPoke: {
        uint8_t *d = pointer(p);
        unsigned n = rd16(p);
        std::memcpy(d, p, n);
        p += n;
        dirty = true;
        break;
      }
      case kUnpack: {
        uint8_t *s = pointer(p);
        uint8_t *d = pointer(p);
        uint32_t n = rd32(p);
        /* Not past the arena (the source runs to the end of its block). */
        const uint8_t *arena_end = block[nblocks - 1] +
                                   ((block_size[nblocks - 1] + 15) & ~15u) +
                                   kPad;
        if (d + n > s && d < s + n) {
          uint8_t *tmp = static_cast<uint8_t *>(std::malloc(n));
          if (tmp != nullptr) {
            byterun1(s, arena_end, tmp, n);
            std::memcpy(d, tmp, n);
            std::free(tmp);
          }
        } else {
          byterun1(s, arena_end, d, n);
        }
        dirty = true;
        break;
      }
      case kDelta: {
        const uint8_t *s = pointer(p);
        uint8_t *d = pointer(p);
        for (;;) {
          uint32_t off = be32(s);
          s += 4;
          if (off & 0x80000000u) break;
          uint8_t *base = d + off;
          for (int k = 0; k < 5; k++) {
            std::memcpy(base + 40 * k, s, 4);
            s += 4;
          }
        }
        dirty = true;
        break;
      }
      case kBlit:
        op_blit(p);
        break;
      case kSfxBank:
        sfx_bank = pointer(p);
        break;
      case kSfx: {
        unsigned k = *p++;
        unsigned vol = *p++;
        unsigned rep = rd16(p);
        sfx_start(k, vol, rep);
        break;
      }
      case kSfxOn:
        std::memset(voices, 0, sizeof(voices));
        sfx_on = true;
        break;
      case kSfxOff:
        sfx_on = false;
        break;
      case kSfxStop: {
        unsigned v = *p++;
        if (v < 3) {
          voices[v].vol = 0;
          voices[v].cnt = 0;
        }
        break;
      }
      case kMusic: {
        unsigned m = *p++;
        if (m >= MusicPlay && m <= MusicStop) {
          music_ev = static_cast<Music>(m);
        }
        break;
      }
      case kScene:
        p++;
        break;
      case kView:
        op_view(p);
        break;
      default:
        /* check_script() saw every record. */
        p = end;
        break;
    }
  }
  if (!mixed) mix(sfx);
  pos = p - script;
  cur_frame++;
  return true;
}

/* One display band (the copper's view from line y0 to y1). */
void
IntroPlayer::render_band(const Band &b, int bi, int y0, int y1,
                         uint8_t *pixels) {
  int nwords = (static_cast<int>(b.ddfstop) - static_cast<int>(b.ddfstrt)) /
               8 + 1;
  if (nwords < 1) nwords = 1;
  if (nwords > 40) nwords = 40;
  int hs = static_cast<int>(b.diwstrt & 0xFF) - 0x81;
  int he = static_cast<int>(b.diwstop & 0xFF) + 0x100 - 0x81;
  int x0 = static_cast<int>(b.ddfstrt) * 2 + 17 - 0x81;
  const uint8_t *pl[6];
  for (int k = 0; k < b.depth; k++) pl[k] = b.planes[k];
  int line_bytes = nwords * 2;
  uint8_t base = static_cast<uint8_t>(bi * 64);
  bool fast = b.con1 == 0 && x0 == 0 && hs <= 0 && he >= kWidth &&
              nwords * 16 >= kWidth && base == 0;
  for (int y = y0; y < y1; y++) {
    uint8_t *row = pixels + y * kWidth;
    if (fast) {
      /* The usual window: 320 pixels from the first bit of each plane. */
      alias32 *o = reinterpret_cast<alias32 *>(row);
      for (int j = 0; j < kWidth / 8; j++) {
        uint32_t hi = 0, lo = 0;
        for (int k = 0; k < b.depth; k++) {
          unsigned v = pl[k][j];
          hi |= c2p_tab[v][0] << k;
          lo |= c2p_tab[v][1] << k;
        }
        o[2 * j] = hi;
        o[2 * j + 1] = lo;
      }
    } else {
      /* Any window, fine scroll per odd / even plane (BPLCON1). */
      uint8_t pix[40 * 16 + 16];
      std::memset(pix, 0, sizeof(pix));
      for (int k = 0; k < b.depth; k++) {
        int delay = (b.con1 >> ((k & 1) ? 4 : 0)) & 0xF;
        for (int i = 0; i < line_bytes * 8; i++) {
          if (pl[k][i >> 3] & (0x80 >> (i & 7))) {
            pix[delay + i] |= static_cast<uint8_t>(1 << k);
          }
        }
      }
      int n = line_bytes * 8 + 16;
      for (int x = 0; x < kWidth; x++) {
        int i = x - x0;
        uint8_t v = (i >= 0 && i < n && x >= hs && x < he) ? pix[i] : 0;
        row[x] = static_cast<uint8_t>(base + v);
      }
    }
    for (int k = 0; k < b.depth; k++) {
      pl[k] += line_bytes + ((k & 1) ? b.mod2 : b.mod1);
    }
  }
}

void
IntroPlayer::render(uint8_t *pixels, uint8_t *rgb) {
  dirty = false;
  std::memset(rgb, 0, 256 * 3);
  int y = 0;
  for (int bi = 0; bi < nbands; bi++) {
    const Band &b = bands[bi];
    int y0 = b.first_line < kHeight ? b.first_line : kHeight;
    int y1 = bi + 1 < nbands ? bands[bi + 1].first_line : kHeight;
    if (y1 > kHeight) y1 = kHeight;
    if (bi == 0) {
      if (y0 > 0) std::memset(pixels, kBlack, y0 * kWidth);
      y = y0;
    }
    if (y1 > y0) render_band(b, bi, y0, y1, pixels);
    if (y1 > y) y = y1;
    uint8_t *c = rgb + bi * 64 * 3;
    for (int i = 0; i < 32; i++) {
      unsigned v = b.colours[i];
      uint8_t r = static_cast<uint8_t>(((v >> 8) & 15) * 17);
      uint8_t g = static_cast<uint8_t>(((v >> 4) & 15) * 17);
      uint8_t bl = static_cast<uint8_t>((v & 15) * 17);
      c[3 * i] = r;
      c[3 * i + 1] = g;
      c[3 * i + 2] = bl;
      /* Extra Half-Brite: colours 32..63 at half the brightness. */
      c[3 * (32 + i)] = r >> 1;
      c[3 * (32 + i) + 1] = g >> 1;
      c[3 * (32 + i) + 2] = bl >> 1;
    }
  }
  if (y < kHeight) {
    std::memset(pixels + y * kWidth, kBlack, (kHeight - y) * kWidth);
  }
}
