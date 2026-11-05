/* Amiga screen, input and timing for the Settlers port. Plain C, so that
   the AmigaOS headers stay out of the Freeserf sources (name clashes such
   as struct Message). */
#ifndef AMIGA_SCREEN_H
#define AMIGA_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
  AEV_NONE,
  AEV_MOVE,       /* x, y; rel: dx, dy (the pointer held, see below) */
  AEV_BUTTON,     /* x, y, button (1 left, 2 middle, 3 right), down */
  AEV_KEY,        /* rawkey code, qual, ascii (0 if none), down */
  AEV_CLOSE,      /* close gadget */
  AEV_ACTIVE,     /* window became active / inactive: down */
};

/* Qualifier bits in AmigaEvent.qual */
#define AQUAL_SHIFT 1
#define AQUAL_CTRL  2
#define AQUAL_ALT   4

typedef struct AmigaEvent {
  int type;
  int x, y;
  int button;
  int down;
  int code;
  int qual;
  int ascii;
  /* While the right button is held the pointer stays where it went down
     (as in the original: the map scrolls on and on): a move is then
     rel = 1 with the motion in dx, dy, and x, y stay at that place. */
  int rel;
  int dx, dy;
} AmigaEvent;

/* Raw key codes the port uses */
#define ARAW_ESC    0x45
#define ARAW_UP     0x4c
#define ARAW_DOWN   0x4d
#define ARAW_RIGHT  0x4e
#define ARAW_LEFT   0x4f
#define ARAW_F1     0x50
#define ARAW_F9     0x58
#define ARAW_F10    0x59
#define ARAW_TAB    0x42
#define ARAW_HELP   0x5f

/* Opens an 8-bit CLUT RTG screen of width x height with a backdrop window.
   window_mode != 0: a window on the Workbench instead (not yet).
   Returns 0 on success, otherwise prints the reason to buf. */
int amiga_screen_open(int width, int height, const char *title,
                      char *err, int errlen);
void amiga_screen_close(void);
/* Is there an 8-bit RTG mode of exactly w x h? */
int amiga_screen_mode_exists(int w, int h);
/* Another w x h mode while the game runs; on failure the old one is
   opened again and -1 returned. */
int amiga_screen_reopen(int w, int h, char *err, int errlen);
int amiga_screen_width(void);
int amiga_screen_height(void);

/* Sets count colours from first; rgb holds count RGB triplets. */
void amiga_set_colors(int first, int count, const unsigned char *rgb);

/* Copies a w x h rectangle of 8-bit pixels (rows modulo bytes apart) to
   the screen at x, y. */
void amiga_screen_write(const unsigned char *pixels, int modulo,
                        int x, int y, int w, int h);

void amiga_set_title(const char *title);

/* Hides the system pointer over the game's window (the game draws its own
   cursor). */
void amiga_hide_pointer(void);

/* Shows an IFF ILBM picture on the screen, scaled down to fit and centred
   (its palette replaces colours 0..n-1). 0: shown, -1: none or not read. */
int amiga_show_picture(const char *path);

/* How amiga_screen_write sends pixels: 0 WriteChunkyPixels, 1 locked
   bitmap with long moves, 2 the same with MOVE16 where possible. */
extern int amiga_blit_mode;

/* Composing in the screen memory (amiga_screen.c, RENDER=VRAM). */
int amiga_vram_open(void);          /* 0: the back bitmap is on the card */
void amiga_vram_close(void);
/* The back bitmap locked for the CPU (stays locked until a blit, present
   or unlock); NULL when it cannot be locked. */
unsigned char *amiga_back_lock(int *bytes_per_row);
void amiga_back_unlock(void);
void *amiga_tile_new(int w, int h);  /* NULL: no room on the card */
void amiga_tile_free(void *tile);
int amiga_tile_upload(void *tile, const unsigned char *pixels, int modulo,
                      int w, int h);
void amiga_tile_blit(void *tile, int sx, int sy, int dx, int dy, int w,
                     int h);
void amiga_present(void);
void amiga_back_read(int x, int y, int w, int h, unsigned char *dst,
                     int modulo);

/* Next pending event; 0 when there is none. */
int amiga_get_event(AmigaEvent *ev);

/* Sleeps until an input event arrives, Ctrl-C is pressed or ms
   milliseconds have passed. Returns 1 when Ctrl-C was pressed. */
int amiga_wait(unsigned long ms);

/* Moves h rows of the screen from row sy to row dy (blitter, may overlap). */
void amiga_screen_move(int sy, int dy, int h);

/* Sets the priority of the calling task; returns the old one. */
int amiga_task_priority(int pri);

/* Ctrl-C pending (clears it)? */
int amiga_break(void);

/* Milliseconds from the E-clock (amiga_time.c). */
unsigned long amiga_now_ms(void);
unsigned long amiga_eclock(void);
unsigned long amiga_eclock_per_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA_SCREEN_H */
