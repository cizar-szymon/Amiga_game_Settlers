/*
 * amiga-box.cc - The Amiga box of the options (Amiga port).
 */

#include "port/amiga-box.h"

#include <string>

#include "src/interface.h"
#include "src/popup.h"
#include "src/box-frame.h"
#include "src/audio.h"
#include "src/data.h"
#include "src/gfx.h"
#include "src/translation.h"

static const int kExitSize = 16;
static const int kLabelX = 16;
static const int kValueX = 128;
static const int kFirstRowY = 22;
static const int kRowHeight = 10;
static const int kArrowSize = 16;
/* The clock row and its arrows (previous: lower, next: higher). */
static const int kClockRow = 11;

AmigaBox::AmigaBox(Interface *_interface)
  : interface(_interface) {
  set_size(kWidth, kHeight);
  amiga_info_read(&info);
  start_mhz = info.warp_mhz;
  is_68040 = std::string(info.cpu) == "MC68040";
}

/* A MC68060 goes from 50 to 100 MHz in steps of 10, above the clock it
   had when the box opened (overclocking) only a rev. 6; a MC68040 from 20
   to 50 MHz in steps of 1 (its revision cannot be read). */
void
AmigaBox::clock_range(int *low, int *high, int *step) const {
  *low = *high = *step = 0;
  if (info.is_68060) {
    *low = 50;
    *high = (info.cpu_rev >= 6) ? 100 : start_mhz;
    *step = 10;
  } else if (is_68040) {
    *low = 20;
    *high = 50;
    *step = 1;
  }
}

static std::string
size_text(unsigned long kb) {
  if (kb >= 2048) return std::to_string(kb / 1024) + " MB";
  return std::to_string(kb) + " KB";
}

void
AmigaBox::internal_draw() {
  for (int y = 0; y < height; y += 16) {
    for (int x = 0; x < width; x += 16) {
      frame->draw_sprite(x, y, Data::AssetIcon,
                         PopupBox::PatternDiagonalGreen);
    }
  }
  draw_box_frame(frame, width, height);

  std::string title = "Amiga";
  frame->draw_string((width - frame->get_string_width(title)) / 2, 9, title,
                     Color::green, Color::black);

  unsigned int sw = 0, sh = 0;
  Graphics::get_instance().get_resolution(&sw, &sh);
  std::string cpu = info.cpu;
  if (info.cpu_rev >= 0) cpu += " rev. " + std::to_string(info.cpu_rev);
  std::string board = info.board[0] != 0 ? std::string(info.board) : "none";
  if (board.size() > 19) board.resize(19);

  const std::string rows[][2] = {
    { _("Processor"), cpu },
    { "FPU", info.fpu ? _("yes") : _("no") },
    { "Kickstart", std::to_string(info.kick_version) + "." +
                   std::to_string(info.kick_revision) },
    { "Chip RAM", size_text(info.chip_kb) },
    { "Fast RAM", size_text(info.fast_kb) },
    { _("Graphics"), board },
    { _("Card memory"), size_text(info.vram_kb) },
    { _("Screen"), std::to_string(sw) + "x" + std::to_string(sh) + " 8 bit" },
  };
  int y = kFirstRowY;
  for (const auto &r : rows) {
    frame->draw_string(kLabelX, y, r[0], Color::green, Color::black);
    frame->draw_string(kValueX, y, r[1], Color::green, Color::black);
    y += kRowHeight;
  }

  y += kRowHeight / 2;
  if (!info.warp) {
    frame->draw_string(kLabelX, y, _("No Warp: SYS:C/WarpTool"), Color::green,
                       Color::black);
    frame->draw_string(kLabelX, y + kRowHeight, _("not found."), Color::green,
                       Color::black);
  } else {
    frame->draw_string(kLabelX, y, _("Accelerator"), Color::green, Color::black);
    frame->draw_string(kValueX, y, info.warp_model, Color::green,
                       Color::black);
    y += kRowHeight;
    frame->draw_string(kLabelX, y, _("Firmware"), Color::green, Color::black);
    frame->draw_string(kValueX, y, info.warp_firmware, Color::green,
                       Color::black);
    y += kRowHeight;
    int clock_y = kFirstRowY + kClockRow * kRowHeight - 2;
    frame->draw_string(kLabelX, clock_y + 4, _("CPU clock"), Color::green,
                       Color::black);
    frame->draw_string(kValueX, clock_y + 4,
                       std::to_string(info.warp_mhz) + " MHz", Color::green,
                       Color::black);
    int low = 0, high = 0, step = 0;
    clock_range(&low, &high, &step);
    if (step != 0) {
      int arrows_x = width - 24 - 2 * kArrowSize;
      frame->draw_sprite(arrows_x, clock_y, Data::AssetIcon, 220);
      frame->draw_sprite(arrows_x + kArrowSize, clock_y, Data::AssetIcon,
                         221);
    }

    y = clock_y + kArrowSize + 6;
    const std::string warning[] = {  /* at most 31 characters */
      _("Raising the clock overclocks"),
      _("the CPU - the system may hang."),
      is_68040 ? _("MC68040: 20 to 50 MHz.") : _("Only for a MC68060 rev. 6."),
      _("The clock changes at once.")
    };
    for (const std::string &line : warning) {
      frame->draw_string(kLabelX, y, line, Color::green, Color::black);
      y += kRowHeight;
    }
  }

  frame->draw_sprite(width - 8 - kExitSize, height - 7 - kExitSize,
                     Data::AssetIcon, 60);  // Exit
}

bool
AmigaBox::handle_click_left(int x, int y) {
  int low = 0, high = 0, step = 0;
  clock_range(&low, &high, &step);
  if (info.warp && info.warp_mhz > 0 && step != 0) {
    int clock_y = kFirstRowY + kClockRow * kRowHeight - 2;
    int arrows_x = width - 24 - 2 * kArrowSize;
    if (y >= clock_y && y < clock_y + kArrowSize &&
        x >= arrows_x && x < arrows_x + 2 * kArrowSize) {
      bool up = x >= arrows_x + kArrowSize;
      int mhz = info.warp_mhz + (up ? step : -step);
      mhz = mhz - mhz % step;
      if (mhz < low || mhz > high) {
        play_sound(Audio::TypeSfxNotAccepted);
        return true;
      }
      play_sound(Audio::TypeSfxClick);
      amiga_warp_set_mhz(mhz);
      amiga_info_read(&info);  /* what the Warp says now */
      set_redraw();
      return true;
    }
  }

  int exit_x = width - 8 - kExitSize;
  int exit_y = height - 7 - kExitSize;
  if (x >= exit_x && x < exit_x + kExitSize &&
      y >= exit_y && y < exit_y + kExitSize) {
    play_sound(Audio::TypeSfxClick);
    interface->close_platform_box();
  }
  return true;
}
