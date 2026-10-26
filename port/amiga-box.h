/*
 * amiga-box.h - The Amiga box of the options (Amiga port): the computer the
 * game runs on and, with a Warp accelerator, its CPU clock.
 */

#ifndef PORT_AMIGA_BOX_H_
#define PORT_AMIGA_BOX_H_

#include "src/gui.h"
#include "port/amiga_sysinfo.h"

class Interface;

class AmigaBox : public GuiObject {
 public:
  static const int kWidth = 288;
  static const int kHeight = 216;

 protected:
  Interface *interface;
  AmigaInfo info;
  /* The clock when the box opened: a MC68060 other than rev. 6 can be
     lowered and raised back to this, not above. */
  int start_mhz;
  bool is_68040;

 public:
  explicit AmigaBox(Interface *interface);
  virtual ~AmigaBox() {}

 protected:
  /* The range and the step of the clock: MC68060 50-100 MHz in steps of
     10, MC68040 20-50 MHz in steps of 1; 0, 0, 0 for other CPUs. */
  void clock_range(int *low, int *high, int *step) const;
  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // PORT_AMIGA_BOX_H_
