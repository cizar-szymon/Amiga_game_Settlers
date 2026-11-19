/* Update box: is there a newer release on the update server (see
   update-box.h). */

#include "port/update-box.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "src/interface.h"
#include "src/popup.h"
#include "src/box-frame.h"
#include "src/audio.h"
#include "src/data.h"
#include "src/gfx.h"
#include "src/translation.h"
#include "src/event_loop.h"

#include "port/settlers_version.h"

std::string UpdateBox::server;
Interface *UpdateBox::open_later = nullptr;
UpdateBox *UpdateBox::open_box = nullptr;

static const int kExitSize = 16;
static const int kLabelX = 16;
static const int kValueX = 152;
static const int kRowH = 10;
static const int kStatusY = 50;
static const int kNotesY = 82;
static const int kNotesRows = 9;
static const int kChars = 32;     /* letters in a row of the box */
static const int kButtonY = UpdateBox::kHeight - 32;
static const int kCheckX = 16, kCheckW = 96;
static const int kUpdatePort = 6830;

UpdateBox::UpdateBox(Interface *_interface)
  : interface(_interface)
  , shown_state(-1) {
  opaque = true;
  set_size(kWidth, kHeight);
  update_poll(&info);
  open_box = this;
  /* At once, unless a check still runs. */
  if (info.state != 1) check();
}

UpdateBox::~UpdateBox() {
  if (open_box == this) open_box = nullptr;
}

void
UpdateBox::tick_open() {
  if (open_later != nullptr) {
    static unsigned int due = 0;
    unsigned int now = EventLoop::get_instance().get_ticks();
    if (due == 0) due = now + 5000;
    if (static_cast<int>(now - due) >= 0) {
      Interface *i = open_later;
      open_later = nullptr;
      i->open_update_box();
    }
  }
  if (open_box != nullptr) open_box->tick();
}

void
UpdateBox::tick() {
  update_poll(&info);
  if (info.state != shown_state) {
    shown_state = info.state;
    set_redraw();
  }
}

void
UpdateBox::check() {
  std::string host = server;
  int port = kUpdatePort;
  size_t colon = host.rfind(':');
  if (colon != std::string::npos) {
    port = atoi(host.c_str() + colon + 1);
    host = host.substr(0, colon);
  }
  std::string lang = Translation::get_instance().get_language();
  lang = lang.substr(0, 2) == "pl" ? "pl" : "en";
  if (update_check(host.c_str(), port, SETTLERS_VERSION, lang.c_str())) {
    update_poll(&info);
  }
  set_redraw();
}

/* The version as numbers ("0.139" -> 0, 139). */
static std::vector<int>
version_numbers(const std::string &v) {
  std::vector<int> n;
  size_t at = 0;
  while (at < v.size()) {
    size_t dot = v.find('.', at);
    if (dot == std::string::npos) dot = v.size();
    n.push_back(atoi(v.substr(at, dot - at).c_str()));
    at = dot + 1;
  }
  return n;
}

bool
UpdateBox::newer() const {
  return info.latest[0] != 0 &&
         version_numbers(info.latest) > version_numbers(SETTLERS_VERSION);
}

/* The rows of a text of kChars letters, longer lines broken at spaces. A
   line that goes on an item of the history of releases ("  more words")
   joins the line before it. */
static void
wrap(const std::string &text, std::vector<std::string> *rows) {
  std::vector<std::string> lines;
  size_t at = 0;
  while (at <= text.size()) {
    size_t nl = text.find('\n', at);
    std::string line = text.substr(at, nl == std::string::npos
                                       ? std::string::npos : nl - at);
    if (line.size() > 2 && line[0] == ' ' && line[1] == ' ' &&
        !lines.empty() && !lines.back().empty()) {
      lines.back() += " " + line.substr(line.find_first_not_of(' '));
    } else {
      lines.push_back(line);
    }
    if (nl == std::string::npos) break;
    at = nl + 1;
  }
  for (std::string line : lines) {
    while (line.size() > static_cast<size_t>(kChars)) {
      size_t cut = line.rfind(' ', kChars);
      if (cut == std::string::npos || cut == 0) cut = kChars;
      rows->push_back(line.substr(0, cut));
      line = "  " + line.substr(cut + (line[cut] == ' ' ? 1 : 0));
    }
    rows->push_back(line);
  }
}

/* As the buttons of the music box: the wood of the Advanced button
   (button-blank.png) stretched to bw, the text in yellow over it. */
void
UpdateBox::draw_button(int bx, int by, int bw, const std::string &text,
                       bool enabled) {
  const int kPlateW = 56, kPlateH = 16;
  const int kMidX = 12, kMidW = 42;
  const int kEdgeW = 2;
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
  int tx = bx + (bw - frame->get_string_width(text)) / 2;
  frame->draw_string(tx, by + 4, text, ink, Color(0x43, 0x23, 0x13));
}

void
UpdateBox::internal_draw() {
  for (int y = 0; y < height; y += 16) {
    for (int x = 0; x < width; x += 16) {
      frame->draw_sprite(x, y, Data::AssetIcon,
                         PopupBox::PatternDiagonalGreen);
    }
  }
  draw_box_frame(frame, width, height);
  std::string title = _("Update");
  frame->draw_string((width - frame->get_string_width(title)) / 2, 9, title,
                     Color::green, Color::black);

  bool working = info.state == 1;
  bool done = info.state == 2;
  frame->draw_string(kLabelX, 24, _("This version"), Color::green,
                     Color::black);
  frame->draw_string(kValueX, 24, SETTLERS_VERSION, Color::green,
                     Color::black);
  frame->draw_string(kLabelX, 34, _("Newest version"), Color::green,
                     Color::black);
  frame->draw_string(kValueX, 34, info.latest[0] != 0
                                  ? std::string(info.latest)
                                  : std::string(working ? "..." : "-"),
                     Color::green, Color::black);

  /* What the check found, in yellow. */
  std::string status;
  if (working) {
    status = _("Asking the update server...");
  } else if (done && !info.ok) {
    /* The texts of amiga_update.c, here for the translations. */
    std::string why = info.text;
    if (why == "The update server did not answer.") {
      status = _("The update server did not answer.");
    } else if (why == "Update server not found.") {
      status = _("Update server not found.");
    } else if (why == "No TCP/IP stack (bsdsocket).") {
      status = _("No TCP/IP stack (bsdsocket).");
    } else if (why == "The answer of the update server came broken.") {
      status = _("The answer of the update server came broken.");
    } else {
      status = why;   /* from the server, English */
    }
  } else if (done && newer()) {
    status = _("A new version is out. Get it with the WARP program on the "
               "Workbench.");
  } else if (done) {
    status = _("You have the newest version.");
  }
  std::vector<std::string> rows;
  wrap(status, &rows);
  int y = kStatusY;
  for (size_t i = 0; i < rows.size() && i < 3; i++, y += kRowH) {
    frame->draw_string(kLabelX, y, rows[i], Color(0xdd, 0xdd, 0x00),
                       Color::black);
  }

  /* What changed since this version (the history of releases). */
  if (done && newer() && info.notes[0] != 0) {
    std::vector<std::string> notes;
    wrap(info.notes, &notes);
    y = kNotesY;
    for (int i = 0; i < kNotesRows && i < static_cast<int>(notes.size());
         i++, y += kRowH) {
      frame->draw_string(kLabelX, y, notes[i], Color::green, Color::black);
    }
  }

  draw_button(kCheckX, kButtonY, kCheckW, _("CHECK"), !working);
  frame->draw_sprite(width - 8 - kExitSize, height - 7 - kExitSize,
                     Data::AssetIcon, 60);  // Exit
}

bool
UpdateBox::handle_click_left(int x, int y) {
  if (y >= kButtonY && y < kButtonY + 16 && x >= kCheckX &&
      x < kCheckX + kCheckW) {
    if (info.state == 1) {
      play_sound(Audio::TypeSfxNotAccepted);
    } else {
      play_sound(Audio::TypeSfxClick);
      check();
    }
    return true;
  }
  int exit_x = width - 8 - kExitSize;
  int exit_y = height - 7 - kExitSize;
  if (x >= exit_x && x < exit_x + kExitSize &&
      y >= exit_y && y < exit_y + kExitSize) {
    play_sound(Audio::TypeSfxClick);
    interface->close_update_box();
  }
  return true;
}
