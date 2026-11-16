/* Music box: the popup of the MP3 player (see music-box.h). The list of
   tracks lives in amiga_mp3.c so that closing and reopening the box keeps
   it. */

#include "port/music-box.h"

#include <algorithm>
#include <string>

#include "src/interface.h"
#include "src/popup.h"
#include "src/box-frame.h"
#include "src/audio.h"
#include "src/data.h"
#include "src/gfx.h"

#include "port/amiga_mp3.h"

MusicBox::MusicBox(Interface *_interface)
  : interface(_interface)
  , selected(-1)
  , first_visible(0)
  , dragging_volume(false) {
  opaque = true;
  set_size(kWidth, kHeight);
  /* The decoder of the Warp card: opens at the first play too, but opening
     it here means the title says what driver was found. */
  amiga_mp3_open();
}

void
MusicBox::scroll_to_selection() {
  if (selected < 0) return;
  if (selected < first_visible) first_visible = selected;
  else if (selected >= first_visible + kVisibleRows) {
    first_visible = selected - kVisibleRows + 1;
  }
}

void
MusicBox::set_volume_from(int x) {
  Rect r = volume_slider();
  int rel = x - r.x;
  if (rel < 0) rel = 0;
  if (rel > r.w) rel = r.w;
  amiga_mp3_set_volume(rel * 100 / r.w);
  set_redraw();
}

/* A button on the wood of the Advanced button of the Options box
   (button-blank.png, 56x16: the plate without its lettering, from
   tools/make_button_blank.py), stretched to bw: its left part, the middle
   repeated, its right edge; the text in yellow over it. */
void
MusicBox::draw_button(int bx, int by, int bw, const char *text,
                      bool pressed, bool enabled) {
  const int kPlateW = 56, kPlateH = 16;
  const int kMidX = 12, kMidW = 42;   /* the repeated part: x 12..53 */
  const int kEdgeW = 2;               /* the right edge: x 54..55 */
  Frame *plate = Graphics::get_instance().create_frame(kPlateW, kPlateH);
  plate->draw_resource_image(0, 0, "button-blank.png");
  int left = std::min(bw - kEdgeW, kMidX + kMidW);
  frame->draw_frame(bx, by, 0, 0, plate, left, kPlateH);
  for (int x = bx + left; x < bx + bw - kEdgeW; x += kMidW) {
    int w = std::min(kMidW, bx + bw - kEdgeW - x);
    frame->draw_frame(x, by, kMidX, 0, plate, w, kPlateH);
  }
  frame->draw_frame(bx + bw - kEdgeW, by, kPlateW - kEdgeW, 0, plate,
                    kEdgeW, kPlateH);
  delete plate;

  Color ink = enabled ? Color(0xff, 0xff, 0x00) : Color(0xbb, 0xaa, 0x77);
  Color dark(0x43, 0x23, 0x13);
  if (text[0] == '^' || text[0] == 'v') {
    /* The arrows of the list (the font has no '^'): a triangle, 5 rows
       of 1, 3, 5, 7, 9 pixels, with a dark outline. */
    bool up = text[0] == '^';
    int cx = bx + bw / 2, top = by + 5;
    for (int pass = 0; pass < 2; pass++) {
      for (int r = 0; r < 5; r++) {
        int row = up ? r : 4 - r;
        int y = top + r;
        if (pass == 0) {
          frame->fill_rect(cx - row - 1, y - 1, 2 * row + 3, 3, dark);
        } else {
          frame->fill_rect(cx - row, y, 2 * row + 1, 1, ink);
        }
      }
    }
  } else {
    int tx = bx + (bw - frame->get_string_width(text)) / 2;
    frame->draw_string(tx, by + 4, text, ink, dark);
  }
  if (pressed) {
    /* A yellow inner outline when a button's state is on (Pause pressed,
       say). */
    frame->draw_rect(bx + 1, by + 1, bw - 2, kPlateH - 2,
                     Color(0xdd, 0xdd, 0x00));
  }
}

void
MusicBox::internal_draw() {
  for (int y = 0; y < height; y += 16) {
    for (int x = 0; x < width; x += 16) {
      frame->draw_sprite(x, y, Data::AssetIcon,
                         PopupBox::PatternDiagonalGreen);
    }
  }
  draw_box_frame(frame, width, height);

  /* Title: "WARP MP3 Player", with a note only when no MHI is loaded. */
  std::string title = "WARP MP3 Player";
  if (!amiga_mp3_have_decoder()) {
    title = title + " (no MHI)";
  }
  frame->draw_string((width - frame->get_string_width(title)) / 2, 9,
                     title, Color::green, Color::black);

  /* The list on a dark background. */
  Rect lr = list_rect();
  frame->fill_rect(lr.x - 2, lr.y - 2, lr.w + 4, lr.h + 4,
                   Color(0x00, 0x33, 0x1a));
  int n = amiga_mp3_count();
  int playing = amiga_mp3_current();
  for (int i = 0; i < kVisibleRows; i++) {
    int idx = first_visible + i;
    if (idx >= n) break;
    int row_y = lr.y + i * kRowH;
    if (idx == selected) {
      frame->fill_rect(lr.x, row_y, lr.w, kRowH,
                       Color(0x00, 0x66, 0x33));
    }
    std::string row;
    if (idx == playing) row = ">";
    else row = " ";
    row += " ";
    row += amiga_mp3_name(idx);
    /* Clip to the row (31 chars at 8 px each): the font has no proportional
       widths, so a character count is enough. */
    const int kMaxChars = (kListW - 4) / 8;
    if (static_cast<int>(row.size()) > kMaxChars) {
      row.resize(kMaxChars);
    }
    frame->draw_string(lr.x + 2, row_y + 1, row, Color::green, Color::black);
  }

  /* Up / down arrows at the right of the list. */
  Rect up = up_rect();
  Rect dn = down_rect();
  draw_button(up.x, up.y, up.w, "^", false, first_visible > 0);
  draw_button(dn.x, dn.y, dn.w, "v", false,
              first_visible + kVisibleRows < n);

  /* Volume: label, bar, value. */
  frame->draw_string(volume_label().x, volume_label().y, "Vol",
                     Color::green, Color::black);
  Rect vs = volume_slider();
  frame->fill_rect(vs.x - 1, vs.y - 1, vs.w + 2, vs.h + 2,
                   Color(0x00, 0x33, 0x1a));
  int fill = vs.w * amiga_mp3_get_volume() / 100;
  frame->fill_rect(vs.x, vs.y, fill, vs.h, Color(0x00, 0xaa, 0x44));
  frame->fill_rect(vs.x + fill, vs.y, vs.w - fill, vs.h, Color::black);
  /* The knob (a bright bar) at the right edge of the fill. */
  int knob_x = vs.x + fill - 1;
  if (knob_x < vs.x) knob_x = vs.x;
  if (knob_x > vs.x + vs.w - 3) knob_x = vs.x + vs.w - 3;
  frame->fill_rect(knob_x, vs.y - 2, 3, vs.h + 4, Color(0xdd, 0xdd, 0x00));

  char val[8];
  snprintf(val, sizeof(val), "%d", amiga_mp3_get_volume());
  /* Right-aligned to the right edge of the arrows. */
  Rect vv = volume_value();
  frame->draw_string(vv.x + vv.w - frame->get_string_width(val), vv.y, val,
                     Color::green, Color::black);

  /* Buttons. */
  Rect r;
  r = add_file_btn();
  draw_button(r.x, r.y, r.w, "Add file", false);
  r = add_dir_btn();
  draw_button(r.x, r.y, r.w, "Add dir", false);
  r = remove_btn();
  draw_button(r.x, r.y, r.w, "Remove", false);
  r = play_btn();
  draw_button(r.x, r.y, r.w, "Play", amiga_mp3_is_playing());
  r = pause_btn();
  draw_button(r.x, r.y, r.w, "Pause", amiga_mp3_is_paused());
  r = stop_btn();
  draw_button(r.x, r.y, r.w, "Stop", false);

  /* Exit. */
  Rect ex = exit_rect();
  frame->draw_sprite(ex.x, ex.y, Data::AssetIcon, 60);
}

bool
MusicBox::handle_click_left(int x, int y) {
  /* Exit box. */
  if (hit(x, y, exit_rect())) {
    play_sound(Audio::TypeSfxClick);
    interface->close_music_box();
    return true;
  }
  /* Up / down arrows. */
  if (hit(x, y, up_rect())) {
    if (first_visible > 0) { first_visible--; set_redraw(); }
    play_sound(Audio::TypeSfxClick);
    return true;
  }
  if (hit(x, y, down_rect())) {
    int n = amiga_mp3_count();
    if (first_visible + kVisibleRows < n) {
      first_visible++;
      set_redraw();
    }
    play_sound(Audio::TypeSfxClick);
    return true;
  }
  /* List row. */
  Rect lr = list_rect();
  if (hit(x, y, lr)) {
    int row = (y - lr.y) / kRowH;
    int idx = first_visible + row;
    if (idx >= 0 && idx < amiga_mp3_count()) {
      selected = idx;
      play_sound(Audio::TypeSfxClick);
      set_redraw();
    }
    return true;
  }
  /* Buttons. */
  if (hit(x, y, add_file_btn())) {
    play_sound(Audio::TypeSfxClick);
    int added = amiga_mp3_add_files();
    if (added > 0 && selected < 0) selected = 0;
    scroll_to_selection();
    set_redraw();
    return true;
  }
  if (hit(x, y, add_dir_btn())) {
    play_sound(Audio::TypeSfxClick);
    int added = amiga_mp3_add_drawer();
    if (added > 0 && selected < 0) selected = 0;
    scroll_to_selection();
    set_redraw();
    return true;
  }
  if (hit(x, y, remove_btn())) {
    if (selected >= 0 && selected < amiga_mp3_count()) {
      play_sound(Audio::TypeSfxClick);
      amiga_mp3_remove(selected);
      int n = amiga_mp3_count();
      if (selected >= n) selected = n - 1;
      if (first_visible > 0 && first_visible + kVisibleRows > n) {
        first_visible = n - kVisibleRows;
        if (first_visible < 0) first_visible = 0;
      }
      set_redraw();
    }
    return true;
  }
  if (hit(x, y, play_btn())) {
    if (selected >= 0 && selected < amiga_mp3_count()) {
      play_sound(Audio::TypeSfxClick);
      amiga_mp3_play(selected);
      /* The original music (micromod) stops while MP3 plays. */
      Audio::PPlayer m = Audio::get_instance().get_music_player();
      if (m) m->enable(false);
      set_redraw();
    }
    return true;
  }
  if (hit(x, y, pause_btn())) {
    play_sound(Audio::TypeSfxClick);
    amiga_mp3_pause();
    set_redraw();
    return true;
  }
  if (hit(x, y, stop_btn())) {
    play_sound(Audio::TypeSfxClick);
    amiga_mp3_stop();
    set_redraw();
    return true;
  }
  return true;
}

/* The left button pressed on the volume slider: it starts following the
   pointer right now, and handle_hold then keeps it in sync until the
   button goes up (handle_release). A press elsewhere lets handle_click_left
   get the click on release. */
bool
MusicBox::handle_press(int x, int y) {
  Rect vs = volume_slider();
  if (hit(x, y, { vs.x - 2, vs.y - 4, vs.w + 4, vs.h + 8 })) {
    set_volume_from(x);
    dragging_volume = true;
    return true;
  }
  return false;
}

bool
MusicBox::handle_hold(int x, int y) {
  (void)y;
  if (dragging_volume) {
    set_volume_from(x);
    return true;
  }
  return false;
}

bool
MusicBox::handle_release() {
  dragging_volume = false;
  return false;
}

bool
MusicBox::handle_dbl_click(int x, int y, Event::Button button) {
  if (button != Event::ButtonLeft) return false;
  Rect lr = list_rect();
  if (hit(x, y, lr)) {
    int row = (y - lr.y) / kRowH;
    int idx = first_visible + row;
    if (idx >= 0 && idx < amiga_mp3_count()) {
      selected = idx;
      amiga_mp3_play(selected);
      Audio::PPlayer m = Audio::get_instance().get_music_player();
      if (m) m->enable(false);
      set_redraw();
    }
    return true;
  }
  return false;
}
