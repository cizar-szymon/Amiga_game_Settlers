/* Amiga screen, input and timing for the Settlers port (see amiga_screen.h).
   First version: an 8-bit CLUT Picasso96 screen, pixels sent with
   WriteChunkyPixels(). */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <devices/timer.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <libraries/Picasso96.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
#include <proto/Picasso96.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>

#include "amiga_screen.h"

struct Library *P96Base;
struct Library *KeymapBase;

/* libnix's read() and write() call __chkabort(), which on Ctrl-C raises
   SIGINT and so calls exit() from wherever the program is, also from the
   middle of the clean-up after the game loop: the static destructors then
   ran twice and freed memory twice ("Freeing memory that is already free",
   2026-10-25). The game loop handles Ctrl-C itself (amiga_wait). */
void __chkabort(void) {}

static struct Screen *screen;
static struct Window *window;
static struct MsgPort *timer_port;
static struct timerequest *timer_req;
static int timer_open;
static int width, height;
static int left_down, mid_down, right_down;
static ULONG palette32[1 + 256 * 3 + 1];
/* An empty sprite in chip RAM: the system pointer hidden while the game
   draws its own (amiga_hide_pointer). */
static UWORD *blank_pointer;
/* Intuition keeps pointers to the titles, not copies. */
static char screen_title[64];
static char window_title[256];

static void set_err(char *err, int errlen, const char *msg)
{
  if (err != NULL && errlen > 0) {
    strncpy(err, msg, errlen - 1);
    err[errlen - 1] = 0;
  }
}

/* The 8-bit mode of exactly w x h, otherwise the smallest one that holds
   it. p96BestModeIDTags() is not used: on a Warp it answered 320x200 for
   640x480 although the 640x480 mode exists. */
static ULONG find_mode(int w, int h)
{
  struct List *ml;
  struct P96Mode *m;
  ULONG best = INVALID_ID;
  ULONG best_area = 0xFFFFFFFFUL;

  ml = p96AllocModeListTags(P96MA_FormatsAllowed, RGBFF_CLUT, TAG_DONE);
  if (ml == NULL) return INVALID_ID;
  for (m = (struct P96Mode *)ml->lh_Head; m->Node.ln_Succ != NULL;
       m = (struct P96Mode *)m->Node.ln_Succ) {
    ULONG area;
    if (m->Depth != 8 || m->Width < w || m->Height < h) continue;
    area = (ULONG)m->Width * m->Height;
    if (area < best_area) {
      best_area = area;
      best = m->DisplayID;
    }
  }
  p96FreeModeList(ml);
  return best;
}

static int open_display(int w, int h, char *err, int errlen);

int amiga_screen_open(int w, int h, const char *title, char *err, int errlen)
{
  strncpy(screen_title, title, sizeof(screen_title) - 1);
  strncpy(window_title, title, sizeof(window_title) - 1);
  P96Base = OpenLibrary((CONST_STRPTR)"Picasso96API.library", 2);
  if (P96Base == NULL) {
    set_err(err, errlen, "Picasso96API.library v2 not found");
    return -1;
  }
  KeymapBase = OpenLibrary((CONST_STRPTR)"keymap.library", 37);

  if (open_display(w, h, err, errlen) != 0) return -1;

  timer_port = CreateMsgPort();
  if (timer_port != NULL) {
    timer_req = (struct timerequest *)CreateIORequest(timer_port,
                                         sizeof(struct timerequest));
  }
  if (timer_req == NULL ||
      OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                 (struct IORequest *)timer_req, 0) != 0) {
    set_err(err, errlen, "cannot open timer.device");
    return -1;
  }
  timer_open = 1;
  return 0;
}

/* The screen and its backdrop window in a w x h 8-bit mode. */
static int open_display(int w, int h, char *err, int errlen)
{
  ULONG mode;

  width = w;
  height = h;
  mode = find_mode(w, h);
  if (mode == INVALID_ID) {
    set_err(err, errlen, "no 8-bit RTG screen mode of that size");
    return -1;
  }

  screen = OpenScreenTags(NULL,
                          SA_DisplayID, mode,
                          SA_Width, w,
                          SA_Height, h,
                          SA_Depth, 8,
                          SA_Type, CUSTOMSCREEN,
                          SA_Quiet, TRUE,
                          SA_ShowTitle, FALSE,
                          SA_Title, (ULONG)screen_title,
                          SA_Exclusive, TRUE,
                          TAG_DONE);
  if (screen == NULL) {
    set_err(err, errlen, "cannot open the screen");
    return -1;
  }

  window = OpenWindowTags(NULL,
                          WA_CustomScreen, (ULONG)screen,
                          WA_Left, 0, WA_Top, 0,
                          WA_Width, w, WA_Height, h,
                          WA_Backdrop, TRUE,
                          WA_Borderless, TRUE,
                          WA_Activate, TRUE,
                          WA_RMBTrap, TRUE,
                          WA_ReportMouse, TRUE,
                          WA_SimpleRefresh, TRUE,
                          WA_NoCareRefresh, TRUE,
                          WA_Title, (ULONG)window_title,
                          WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
                                    IDCMP_RAWKEY | IDCMP_CLOSEWINDOW |
                                    IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW,
                          TAG_DONE);
  if (window == NULL) {
    set_err(err, errlen, "cannot open the window");
    return -1;
  }
  SetRast(window->RPort, 0);
  ShowTitle(screen, FALSE);
  return 0;
}

static void close_display(void)
{
  amiga_vram_close();
  if (window != NULL && blank_pointer != NULL) {
    ClearPointer(window);
  }
  if (window != NULL) {
    struct IntuiMessage *m;
    while ((m = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
      ReplyMsg((struct Message *)m);
    CloseWindow(window);
    window = NULL;
  }
  if (screen != NULL) {
    CloseScreen(screen);
    screen = NULL;
  }
}

/* Another screen mode while the game runs (the Options box): a new screen
   and window; the timer and the libraries stay. */
int amiga_screen_reopen(int w, int h, char *err, int errlen)
{
  int old_w = width, old_h = height;
  close_display();
  if (open_display(w, h, err, errlen) == 0) return 0;
  close_display();
  /* Back to the old mode, which worked. */
  open_display(old_w, old_h, NULL, 0);
  return -1;
}

void amiga_screen_close(void)
{
  if (timer_open) {
    CloseDevice((struct IORequest *)timer_req);
    timer_open = 0;
  }
  if (timer_req != NULL) {
    DeleteIORequest((struct IORequest *)timer_req);
    timer_req = NULL;
  }
  if (timer_port != NULL) {
    DeleteMsgPort(timer_port);
    timer_port = NULL;
  }
  close_display();
  if (blank_pointer != NULL) {
    FreeVec(blank_pointer);
    blank_pointer = NULL;
  }
  if (KeymapBase != NULL) {
    CloseLibrary(KeymapBase);
    KeymapBase = NULL;
  }
  if (P96Base != NULL) {
    CloseLibrary(P96Base);
    P96Base = NULL;
  }
}

void amiga_hide_pointer(void)
{
  if (window == NULL) return;
  if (blank_pointer == NULL) {
    blank_pointer = (UWORD *)AllocVec(16, MEMF_CHIP | MEMF_CLEAR);
    if (blank_pointer == NULL) return;
  }
  SetPointer(window, blank_pointer, 1, 16, 0, 0);
}

int amiga_screen_width(void) { return width; }
int amiga_screen_height(void) { return height; }

void amiga_set_colors(int first, int count, const unsigned char *rgb)
{
  int i;
  if (screen == NULL || count <= 0) return;
  palette32[0] = ((ULONG)count << 16) | (ULONG)first;
  for (i = 0; i < count * 3; i++) {
    palette32[1 + i] = (ULONG)rgb[i] * 0x01010101UL;
  }
  palette32[1 + count * 3] = 0;
  LoadRGB32(&screen->ViewPort, palette32);
}

/* 0: WriteChunkyPixels, 1: locked bitmap with long moves, 2: locked bitmap
   with MOVE16 where the rows allow it (BLIT= option). */
int amiga_blit_mode = 2;

/* Rows of n bytes (a multiple of 64; source and destination 16-byte
   aligned) with MOVE16 (a0)+,(a1)+, which the assembler for the 68020
   does not know: encoded by hand. 68040/68060 only. */
static void copy_move16(UBYTE *dst, const UBYTE *src, ULONG n)
{
  register const UBYTE *a0 __asm("a0") = src;
  register UBYTE *a1 __asm("a1") = dst;
  register ULONG d0 __asm("d0") = n >> 6;
  __asm__ volatile(
    "1:\n\t"
    ".word 0xf620,0x9000\n\t"
    ".word 0xf620,0x9000\n\t"
    ".word 0xf620,0x9000\n\t"
    ".word 0xf620,0x9000\n\t"
    "subq.l #1,%2\n\t"
    "bne.s 1b"
    : "+a"(a0), "+a"(a1), "+d"(d0) : : "memory", "cc");
}

static void copy_long(UBYTE *dst, const UBYTE *src, ULONG n)
{
  ULONG *d = (ULONG *)dst;
  const ULONG *s = (const ULONG *)src;
  while (n >= 16) {
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = s[3];
    d += 4;
    s += 4;
    n -= 16;
  }
  while (n >= 4) {
    *d++ = *s++;
    n -= 4;
  }
  {
    UBYTE *db = (UBYTE *)d;
    const UBYTE *sb = (const UBYTE *)s;
    while (n-- > 0) *db++ = *sb++;
  }
}

/* Following the Warp RTG notes: pixels go to the screen memory only by
   writes, long words or MOVE16, with the bitmap locked just for the copy
   (and no OS call while it is locked). */
void amiga_screen_write(const unsigned char *pixels, int modulo,
                        int x, int y, int w, int h)
{
  struct BitMap *bm;
  struct RenderInfo ri;
  LONG lock;
  int move16;
  int j;

  if (window == NULL || w <= 0 || h <= 0) return;
  if (amiga_blit_mode == 0) {
    WriteChunkyPixels(window->RPort, x, y, x + w - 1, y + h - 1,
                      (UBYTE *)pixels, modulo);
    return;
  }

  move16 = amiga_blit_mode == 2 &&
           (SysBase->AttnFlags & (AFF_68040 | AFF_68060)) != 0 &&
           ((ULONG)pixels & 15) == 0 && (modulo & 15) == 0 &&
           (w & 63) == 0 && (x & 15) == 0;
  if (move16) {
    /* MOVE16 reads memory, not the data cache: write back what the
       copyback cache still holds of the frame. */
    CacheClearE((APTR)pixels, (ULONG)modulo * h, CACRF_ClearD);
  }

  bm = screen->RastPort.BitMap;
  lock = p96LockBitMap(bm, (UBYTE *)&ri, sizeof(ri));
  if (lock == 0 || ri.Memory == NULL || ri.RGBFormat != RGBFB_CLUT) {
    if (lock != 0) p96UnlockBitMap(bm, lock);
    WriteChunkyPixels(window->RPort, x, y, x + w - 1, y + h - 1,
                      (UBYTE *)pixels, modulo);
    return;
  }
  {
    UBYTE *dst = (UBYTE *)ri.Memory + (LONG)y * ri.BytesPerRow + x;
    if (move16 && ((ULONG)dst & 15) == 0 && (ri.BytesPerRow & 15) == 0) {
      for (j = 0; j < h; j++) {
        copy_move16(dst, pixels, w);
        dst += ri.BytesPerRow;
        pixels += modulo;
      }
    } else {
      for (j = 0; j < h; j++) {
        copy_long(dst, pixels, w);
        dst += ri.BytesPerRow;
        pixels += modulo;
      }
    }
  }
  p96UnlockBitMap(bm, lock);
}

void amiga_set_title(const char *title)
{
  if (window != NULL) {
    strncpy(window_title, title, sizeof(window_title) - 1);
    SetWindowTitles(window, (CONST_STRPTR)window_title, (CONST_STRPTR)-1);
    /* Keep the screen title bar hidden: Intuition may bring it up when a
       title of the active window changes. */
    ShowTitle(screen, FALSE);
  }
}

static int map_key(UWORD code, UWORD qual, APTR iaddr)
{
  struct InputEvent ie;
  char buf[4];
  if (KeymapBase == NULL) return 0;
  memset(&ie, 0, sizeof(ie));
  ie.ie_Class = IECLASS_RAWKEY;
  ie.ie_Code = code & ~IECODE_UP_PREFIX;
  ie.ie_Qualifier = qual & ~(IEQUALIFIER_CONTROL | IEQUALIFIER_LALT |
                             IEQUALIFIER_RALT);
  ie.ie_EventAddress = iaddr != NULL ? *(APTR *)iaddr : NULL;
  if (MapRawKey(&ie, (STRPTR)buf, sizeof(buf), NULL) == 1)
    return (unsigned char)buf[0];
  return 0;
}

int amiga_get_event(AmigaEvent *ev)
{
  struct IntuiMessage *m;
  if (window == NULL) return 0;
  while ((m = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
    ULONG cls = m->Class;
    UWORD code = m->Code;
    UWORD qual = m->Qualifier;
    APTR iaddr = m->IAddress;
    int mx = m->MouseX, my = m->MouseY;
    ReplyMsg((struct Message *)m);

    memset(ev, 0, sizeof(*ev));
    ev->x = mx;
    ev->y = my;
    if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ev->qual |= AQUAL_SHIFT;
    if (qual & IEQUALIFIER_CONTROL) ev->qual |= AQUAL_CTRL;
    if (qual & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) ev->qual |= AQUAL_ALT;

    switch (cls) {
      case IDCMP_MOUSEMOVE:
        ev->type = AEV_MOVE;
        return 1;
      case IDCMP_MOUSEBUTTONS:
        ev->type = AEV_BUTTON;
        switch (code) {
          case SELECTDOWN: ev->button = 1; ev->down = 1; left_down = 1; break;
          case SELECTUP:   ev->button = 1; ev->down = 0; left_down = 0; break;
          case MIDDLEDOWN: ev->button = 2; ev->down = 1; mid_down = 1; break;
          case MIDDLEUP:   ev->button = 2; ev->down = 0; mid_down = 0; break;
          case MENUDOWN:   ev->button = 3; ev->down = 1; right_down = 1; break;
          case MENUUP:     ev->button = 3; ev->down = 0; right_down = 0; break;
          default: continue;
        }
        return 1;
      case IDCMP_RAWKEY:
        ev->type = AEV_KEY;
        ev->code = code & ~IECODE_UP_PREFIX;
        ev->down = (code & IECODE_UP_PREFIX) == 0;
        if (ev->down) ev->ascii = map_key(code, qual, iaddr);
        return 1;
      case IDCMP_CLOSEWINDOW:
        ev->type = AEV_CLOSE;
        return 1;
      case IDCMP_ACTIVEWINDOW:
      case IDCMP_INACTIVEWINDOW:
        ev->type = AEV_ACTIVE;
        ev->down = cls == IDCMP_ACTIVEWINDOW;
        return 1;
      default:
        break;
    }
  }
  return 0;
}

int amiga_wait(unsigned long ms)
{
  ULONG winsig, timersig, got;
  if (window == NULL || !timer_open) return 0;
  winsig = 1UL << window->UserPort->mp_SigBit;
  timersig = 1UL << timer_port->mp_SigBit;
  if (ms > 0) {
    timer_req->tr_node.io_Command = TR_ADDREQUEST;
    timer_req->tr_time.tv_secs = ms / 1000;
    timer_req->tr_time.tv_micro = (ms % 1000) * 1000;
    SetSignal(0, timersig);
    SendIO((struct IORequest *)timer_req);
    got = Wait(winsig | timersig | SIGBREAKF_CTRL_C);
    if (!CheckIO((struct IORequest *)timer_req))
      AbortIO((struct IORequest *)timer_req);
    WaitIO((struct IORequest *)timer_req);
    SetSignal(0, timersig);
  } else {
    got = SetSignal(0, 0);
  }
  if (got & SIGBREAKF_CTRL_C) {
    SetSignal(0, SIGBREAKF_CTRL_C);
    return 1;
  }
  return 0;
}

int amiga_break(void)
{
  if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
    SetSignal(0, SIGBREAKF_CTRL_C);
    return 1;
  }
  return 0;
}

/* ---- Composing in the screen memory (RENDER=VRAM) ----

   The frame is put together in a bitmap in the card's memory (back): the
   landscape tiles are copied there by the card's blitter from bitmaps of
   their own, the CPU writes the sprites and boxes in place, and the
   finished frame goes to the screen with one more blit. Following the Warp
   RTG notes, the CPU only writes to it (except the shadows), and the
   bitmap is locked while the CPU works on it and unlocked for the blits. */

static struct BitMap *back;
static LONG back_lock;
static struct RenderInfo back_ri;

static int on_board(struct BitMap *bm)
{
  return bm != NULL && p96GetBitMapAttr(bm, P96BMA_ISONBOARD) != 0;
}

int amiga_vram_open(void)
{
  if (screen == NULL) return -1;
  back = AllocBitMap(width, height, 8, BMF_MINPLANES | BMF_DISPLAYABLE,
                     screen->RastPort.BitMap);
  if (!on_board(back)) {
    if (back != NULL) FreeBitMap(back);
    back = NULL;
    return -1;
  }
  return 0;
}

void amiga_vram_close(void)
{
  amiga_back_unlock();
  if (back != NULL) {
    WaitBlit();
    FreeBitMap(back);
    back = NULL;
  }
}

unsigned char *amiga_back_lock(int *bytes_per_row)
{
  if (back == NULL) return NULL;
  if (back_lock == 0) {
    back_lock = p96LockBitMap(back, (UBYTE *)&back_ri, sizeof(back_ri));
    if (back_lock == 0) return NULL;
  }
  *bytes_per_row = back_ri.BytesPerRow;
  return (unsigned char *)back_ri.Memory;
}

void amiga_back_unlock(void)
{
  if (back_lock != 0) {
    p96UnlockBitMap(back, back_lock);
    back_lock = 0;
  }
}

void *amiga_tile_new(int w, int h)
{
  struct BitMap *bm;
  if (screen == NULL) return NULL;
  bm = AllocBitMap(w, h, 8, BMF_MINPLANES, screen->RastPort.BitMap);
  if (!on_board(bm)) {
    if (bm != NULL) FreeBitMap(bm);
    return NULL;
  }
  return bm;
}

void amiga_tile_free(void *tile)
{
  if (tile == NULL) return;
  amiga_back_unlock();
  WaitBlit();
  FreeBitMap((struct BitMap *)tile);
}

int amiga_tile_upload(void *tile, const unsigned char *pixels, int modulo,
                      int w, int h)
{
  struct BitMap *bm = (struct BitMap *)tile;
  struct RenderInfo ri;
  LONG lock;
  UBYTE *dst;
  int j;

  amiga_back_unlock();
  lock = p96LockBitMap(bm, (UBYTE *)&ri, sizeof(ri));
  if (lock == 0) return -1;
  dst = (UBYTE *)ri.Memory;
  for (j = 0; j < h; j++) {
    copy_long(dst, pixels, w);
    dst += ri.BytesPerRow;
    pixels += modulo;
  }
  p96UnlockBitMap(bm, lock);
  return 0;
}

void amiga_tile_blit(void *tile, int sx, int sy, int dx, int dy, int w, int h)
{
  amiga_back_unlock();
  BltBitMap((struct BitMap *)tile, sx, sy, back, dx, dy, w, h, 0xc0, 0xff,
            NULL);
}

/* The finished frame to the screen. */
void amiga_present(void)
{
  amiga_back_unlock();
  BltBitMapRastPort(back, 0, 0, window->RPort, 0, 0, width, height, 0xc0);
}

/* Rows of the back bitmap into fast RAM (VERIFY, F9 screenshots). */
void amiga_back_read(int x, int y, int w, int h, unsigned char *dst,
                     int modulo)
{
  int bpr;
  const unsigned char *src = amiga_back_lock(&bpr);
  int j;
  if (src == NULL) return;
  src += (LONG)y * bpr + x;
  for (j = 0; j < h; j++) {
    copy_long(dst, src, w);
    dst += modulo;
    src += bpr;
  }
}
