/* Music box of the Amiga port: a popup with the MP3 playlist, the keys
   Play/Pause/Stop, a volume slider and two Add buttons (File / Drawer).
   Opened with M (and closed again), plays MP3 files through mhi.library
   of the Warp card (amiga_mp3.c). */
#ifndef PORT_MUSIC_BOX_H_
#define PORT_MUSIC_BOX_H_

#include "src/gui.h"

class Interface;

class MusicBox : public GuiObject {
 public:
  static const int kWidth = 288;
  static const int kHeight = 200;

 protected:
  Interface *interface;
  int selected;        /* -1: no selected row */
  int first_visible;
  /* The slider being dragged (left down on it). */
  bool dragging_volume;

 public:
  explicit MusicBox(Interface *interface);
  virtual ~MusicBox() {}

 protected:
  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
  virtual bool handle_press(int x, int y);
  virtual bool handle_dbl_click(int x, int y, Event::Button button);
  virtual bool handle_hold(int x, int y);
  virtual bool handle_release();

 private:
  struct Rect { int x, y, w, h; };
  static const int kRowH = 10;
  static const int kListX = 12;
  static const int kListY = 22;
  static const int kListW = 244;
  static const int kVisibleRows = 8;
  static const int kListH = kRowH * kVisibleRows;

  Rect list_rect() const { return { kListX, kListY, kListW, kListH }; }
  Rect up_rect() const { return { kListX + kListW + 2, kListY, 16, 16 }; }
  Rect down_rect() const {
    return { kListX + kListW + 2, kListY + kListH - 16, 16, 16 };
  }
  /* Everything under the list as wide as the list with its arrows (its
     dark background from kListX - 2 to the right edge of the arrows). */
  static const int kLeft = kListX - 2;
  static const int kRight = kListX + kListW + 2 + 16;
  static const int kBtnGap = 9;
  static const int kBtnW = (kRight - kLeft - 2 * kBtnGap) / 3;
  static int btn_x(int col) { return kLeft + col * (kBtnW + kBtnGap); }
  Rect volume_label() const { return { kLeft, 108, 24, 12 }; }
  Rect volume_slider() const {
    return { kLeft + 32, 112, kRight - 32 - (kLeft + 32), 8 };
  }
  Rect volume_value() const { return { kRight - 24, 108, 24, 12 }; }
  Rect add_file_btn() const { return { btn_x(0), 128, kBtnW, 16 }; }
  Rect add_dir_btn() const { return { btn_x(1), 128, kBtnW, 16 }; }
  Rect remove_btn() const { return { btn_x(2), 128, kBtnW, 16 }; }
  Rect play_btn() const { return { btn_x(0), 152, kBtnW, 16 }; }
  Rect pause_btn() const { return { btn_x(1), 152, kBtnW, 16 }; }
  Rect stop_btn() const { return { btn_x(2), 152, kBtnW, 16 }; }
  Rect exit_rect() const { return { kWidth - 8 - 16, kHeight - 7 - 16, 16, 16 }; }

  static bool hit(int x, int y, Rect r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
  }
  void scroll_to_selection();
  void set_volume_from(int x);
  void draw_button(int bx, int by, int bw, const char *text, bool pressed,
                   bool enabled = true);
};

#endif  // PORT_MUSIC_BOX_H_
