/*
 * font-amiga.cc - The fonts of the texts for the Amiga port
 *
 * Instead of the TrueType font (SDL_ttf): the font of the game data
 * (no Font object, Options -> Font -> Original) or the readable font
 * (load_resource("readable"), Options -> Font -> Readable) - 8x8 glyphs
 * of port/font_readable.h with small letters, 8 pixels apart as the
 * original letters, so the texts keep their places. The Polish and German
 * letters: a letter of the table and its mark.
 *
 * This file is part of the Settlers port of freeserf (GPL-3.0).
 */

#include "src/font.h"

#include "font_readable.h"

/* Above the letters: the marks of the capitals two rows over them. */
static const int kTop = -2;
static const int kHeight = 11;   /* rows -2 .. 8 (the ogonek) */

Font::Font()
  : font(nullptr) {
}

Font::~Font() {
}

bool
Font::load_resource(const std::string &name) {
  if (name != "readable") return false;
  font = this;   /* only "loaded"; the glyphs are in the table */
  return true;
}

bool
Font::load(const std::string &path) {
  return false;
}

const Font::Glyph *
Font::get_glyph(uint32_t ch) {
  if (font == nullptr) return nullptr;
  auto it = glyphs.find(ch);
  if (it != glyphs.end()) return it->second.get();
  std::unique_ptr<Glyph> glyph = render(ch);
  const Glyph *result = glyph.get();
  if (glyph) glyphs[ch] = std::move(glyph);
  return result;
}

static const char *const *
table_rows(uint32_t ch) {
  for (const auto &g : kReadableGlyphs) {
    if (g.ch == ch) return g.rows;
  }
  return nullptr;
}

/* The marks: pairs x, y in the cell (y < 0 over it), -1 at the end; two
   pixels wide, as the strokes. Capitals have them over row 0, the small
   letters over row 2. */
static const int8_t kAcuteBig[] = { 2, -1, 3, -1, 3, -2, 4, -2, -1 };
static const int8_t kAcuteSmall[] = { 2, 0, 3, 0, 3, -1, 4, -1, -1 };
static const int8_t kDotBig[] = { 2, -2, 3, -2, 2, -1, 3, -1, -1 };
static const int8_t kDotSmall[] = { 2, -1, 3, -1, 2, 0, 3, 0, -1 };
static const int8_t kUmlautBig[] = { 0, -2, 1, -2, 0, -1, 1, -1,
                                     4, -2, 5, -2, 4, -1, 5, -1, -1 };
static const int8_t kUmlautSmall[] = { 0, -1, 1, -1, 0, 0, 1, 0,
                                       4, -1, 5, -1, 4, 0, 5, 0, -1 };
static const int8_t kOgonekA[] = { 4, 7, 5, 7, 5, 8, 6, 8, -1 };
static const int8_t kOgonekE[] = { 3, 7, 4, 7, 4, 8, 5, 8, -1 };

static const struct {
  uint32_t ch;
  char base;
  const int8_t *mark;
} kComposed[] = {
  { 0x104, 'A', kOgonekA }, { 0x105, 'a', kOgonekA },
  { 0x106, 'C', kAcuteBig }, { 0x107, 'c', kAcuteSmall },
  { 0x118, 'E', kOgonekE }, { 0x119, 'e', kOgonekE },
  { 0x143, 'N', kAcuteBig }, { 0x144, 'n', kAcuteSmall },
  { 0xd3, 'O', kAcuteBig }, { 0xf3, 'o', kAcuteSmall },
  { 0x15a, 'S', kAcuteBig }, { 0x15b, 's', kAcuteSmall },
  { 0x179, 'Z', kAcuteBig }, { 0x17a, 'z', kAcuteSmall },
  { 0x17b, 'Z', kDotBig }, { 0x17c, 'z', kDotSmall },
  { 0xc4, 'A', kUmlautBig }, { 0xe4, 'a', kUmlautSmall },
  { 0xd6, 'O', kUmlautBig }, { 0xf6, 'o', kUmlautSmall },
  { 0xdc, 'U', kUmlautBig }, { 0xfc, 'u', kUmlautSmall },
};

std::unique_ptr<Font::Glyph>
Font::render(uint32_t ch) {
  const char *const *rows = table_rows(ch);
  const int8_t *mark = nullptr;
  if (rows == nullptr) {
    for (const auto &c : kComposed) {
      if (c.ch == ch) {
        rows = table_rows(static_cast<unsigned char>(c.base));
        mark = c.mark;
        break;
      }
    }
  }
  if (rows == nullptr) return nullptr;

  std::unique_ptr<Glyph> glyph(new Glyph());
  glyph->advance = 8;
  glyph->left = 0;
  if (ch == ' ') {
    glyph->top = 0;
    glyph->width = 0;
    glyph->height = 0;
    return glyph;
  }
  glyph->top = kTop;
  glyph->width = 8;
  glyph->height = kHeight;
  glyph->pixels.assign(8 * kHeight, false);
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8 && rows[r][c] != '\0'; c++) {
      if (rows[r][c] == '#') glyph->pixels[(r - kTop) * 8 + c] = true;
    }
  }
  for (const int8_t *q = mark; q != nullptr && q[0] >= 0; q += 2) {
    glyph->pixels[(q[1] - kTop) * 8 + q[0]] = true;
  }
  return glyph;
}
