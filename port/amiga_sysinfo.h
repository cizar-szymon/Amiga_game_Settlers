/*
 * amiga_sysinfo.h - The Amiga the game runs on, for the Amiga box of the
 * options (CPU and its revision, Kickstart, memory, graphics card, and a Warp
 * accelerator through SYS:C/WarpTool).
 */

#ifndef PORT_AMIGA_SYSINFO_H_
#define PORT_AMIGA_SYSINFO_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AmigaInfo {
  char cpu[24];            /* "MC68060", "MC68LC060", "MC68040", ... */
  int is_68060;
  int cpu_rev;             /* 68060: the revision in its PCR, else -1 */
  int fpu;
  int kick_version, kick_revision;
  unsigned long chip_kb, fast_kb;
  char board[40];          /* the Picasso96 board, "" if none */
  unsigned long vram_kb;
  int warp;                /* 1: WarpTool answered; 2: a Warp among the
                              expansion boards, no WarpTool */
  char warp_model[24];
  char warp_firmware[16];
  int warp_mhz;            /* the CPU clock it reports, 0 if unknown */
} AmigaInfo;

/* Fills info; the Warp part runs "WarpTool Diag" (a fraction of a
   second). */
void amiga_info_read(AmigaInfo *info);

/* Sets the CPU clock of the Warp (WarpTool CPUFreqHz, at once), 20 to 100
   MHz. 0: done. */
int amiga_warp_set_mhz(int mhz);

#ifdef __cplusplus
}
#endif

#endif  /* PORT_AMIGA_SYSINFO_H_ */
