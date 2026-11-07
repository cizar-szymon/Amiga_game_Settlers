/* The Amiga the game runs on (see amiga_sysinfo.h). */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <libraries/Picasso96.h>

#include <libraries/configvars.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/Picasso96.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "amiga_sysinfo.h"

extern struct ExecBase *SysBase;
extern struct Library *P96Base;   /* opened by amiga_screen.c */

/* MOVEC PCR,D0 (68060 only), in supervisor mode by Supervisor(): the
   processor's identity in bits 31-16 (0x0430 MC68060, 0x0431 MC68LC060 /
   EC060) and its revision in bits 15-8. */
ULONG pcr_trap(void);
__asm__(".text\n"
        "\t.even\n"
        "_pcr_trap:\n"
        "\t.short 0x4e7a,0x0808\n"
        "\trte\n");

/* WarpTool: where CS-Lab's installer puts it, else other places of the
   system; the one found is used to set the clock too. */
static const char *const kWarpTools[] = {
  "SYS:C/WarpTool", "C:WarpTool", "SYS:Tools/WarpTool",
  "SYS:Utilities/WarpTool", "SYS:System/WarpTool", "SYS:WarpTool"
};
static const char *warp_tool = NULL;
static const char kDiagFile[] = "RAM:Settlers-warp.txt";
/* The expansion boards of a Warp (as ShowConfig lists them: "CS-LAB Warp
   DDR3 FastRam", "Warp GFX RAM" ...): manufacturer 5120 ($1400). */
#define CSLAB_MANUFACTURER 5120

/* Runs a command with no input and its output to a file (or NIL:). */
static LONG run(const char *command, const char *out)
{
  BPTR in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
  BPTR o = Open((CONST_STRPTR)out, MODE_NEWFILE);
  LONG rc = -1;
  if (in != 0 && o != 0) {
    rc = SystemTags((CONST_STRPTR)command, SYS_Input, (ULONG)in,
                    SYS_Output, (ULONG)o, TAG_DONE);
  }
  if (o != 0) Close(o);
  if (in != 0) Close(in);
  return rc;
}

/* The value of a "Name...... : value" line of WarpTool Diag, without its
   colour codes (ESC [ ... m) and spaces at the ends. */
static void diag_value(const char *line, char *value, size_t size)
{
  const char *p = strstr(line, ": ");
  size_t n = 0;
  value[0] = 0;
  if (p == NULL) return;
  for (p += 2; *p != 0 && *p != '\n' && n + 1 < size; p++) {
    if (*p == 0x1b) {
      while (*p != 0 && *p != 'm') p++;
      if (*p == 0) break;
      continue;
    }
    if (*p == ' ' && n == 0) continue;
    value[n++] = *p;
  }
  while (n > 0 && value[n - 1] == ' ') n--;
  value[n] = 0;
}

/* Without WarpTool: a Warp among the expansion boards (the list that
   ShowConfig shows), so the box still names the accelerator. */
static int warp_board(void)
{
  struct Library *ExpansionBase;
  struct ConfigDev *cd;
  int found = 0;
  ExpansionBase = OpenLibrary((CONST_STRPTR)"expansion.library", 37);
  if (ExpansionBase == NULL) return 0;
  cd = FindConfigDev(NULL, CSLAB_MANUFACTURER, -1);
  found = cd != NULL;
  CloseLibrary(ExpansionBase);
  return found;
}

static void read_warp(AmigaInfo *info)
{
  char line[160], value[64], command[64];
  FILE *f;
  size_t i;
  warp_tool = NULL;
  /* Tests: SetEnv SETTLERS_NOWARPTOOL 1 - as if there were no WarpTool. */
  for (i = 0; getenv("SETTLERS_NOWARPTOOL") == NULL && i < sizeof(kWarpTools) / sizeof(kWarpTools[0]); i++) {
    BPTR lock = Lock((CONST_STRPTR)kWarpTools[i], SHARED_LOCK);
    if (lock != 0) {
      UnLock(lock);
      warp_tool = kWarpTools[i];
      break;
    }
  }
  if (warp_tool == NULL) {
    if (warp_board()) {
      info->warp = 2;
      strcpy(info->warp_model, "CS-Lab Warp");
    }
    return;
  }
  snprintf(command, sizeof(command), "%s Diag", warp_tool);
  if (run(command, kDiagFile) != 0) return;
  f = fopen(kDiagFile, "r");
  if (f == NULL) return;
  while (fgets(line, sizeof(line), f) != NULL) {
    if (strstr(line, "Model name") != NULL) {
      diag_value(line, info->warp_model, sizeof(info->warp_model));
      info->warp = 1;
    } else if (strstr(line, "Firmware version") != NULL) {
      /* WarpTool gives the version without its dot: 3054 is 3.054. */
      size_t n, i;
      diag_value(line, value, sizeof(value));
      n = strlen(value);
      for (i = 0; i < n && value[i] >= '0' && value[i] <= '9'; i++) {}
      if (n >= 2 && i == n && n + 2 <= sizeof(info->warp_firmware)) {
        info->warp_firmware[0] = value[0];
        info->warp_firmware[1] = '.';
        strcpy(info->warp_firmware + 2, value + 1);
      } else {
        strncpy(info->warp_firmware, value, sizeof(info->warp_firmware) - 1);
      }
    } else if (strstr(line, "clk freq") != NULL) {
      int mhz = 0;
      diag_value(line, value, sizeof(value));
      if (sscanf(value, "%d", &mhz) == 1) info->warp_mhz = mhz;
    }
  }
  fclose(f);
  DeleteFile((CONST_STRPTR)kDiagFile);
  /* WarpTool there but without an answer: the boards then. */
  if (!info->warp && warp_board()) {
    info->warp = 2;
    strcpy(info->warp_model, "CS-Lab Warp");
  }
}

void amiga_info_read(AmigaInfo *info)
{
  UWORD attn = SysBase->AttnFlags;
  memset(info, 0, sizeof(*info));
  info->cpu_rev = -1;

  if (attn & AFF_68060) {
    ULONG pcr = Supervisor((ULONG (*)())pcr_trap);
    info->is_68060 = 1;
    info->cpu_rev = (int)((pcr >> 8) & 0xff);
    strcpy(info->cpu, (pcr >> 16) == 0x0431 ? "MC68LC060" : "MC68060");
  } else if (attn & AFF_68040) {
    strcpy(info->cpu, "MC68040");
  } else if (attn & AFF_68030) {
    strcpy(info->cpu, "MC68030");
  } else if (attn & AFF_68020) {
    strcpy(info->cpu, "MC68020");
  } else {
    strcpy(info->cpu, "MC68000");
  }
  info->fpu = (attn & (AFF_68881 | AFF_68882 | AFF_FPU40)) != 0;
  info->kick_version = SysBase->LibNode.lib_Version;
  info->kick_revision = SysBase->LibNode.lib_Revision;
  info->chip_kb = AvailMem(MEMF_CHIP | MEMF_TOTAL) / 1024;
  info->fast_kb = AvailMem(MEMF_FAST | MEMF_TOTAL) / 1024;

  if (P96Base != NULL) {
    STRPTR name = NULL;
    ULONG mem = 0;
    p96GetBoardDataTags(0, P96BD_BoardName, (ULONG)&name,
                        P96BD_TotalMemory, (ULONG)&mem, TAG_DONE);
    if (name != NULL) {
      strncpy(info->board, (const char *)name, sizeof(info->board) - 1);
    }
    info->vram_kb = mem / 1024;
  }

  read_warp(info);
}

int amiga_warp_set_mhz(int mhz)
{
  char command[64];
  if (mhz < 20 || mhz > 100 || warp_tool == NULL) return -1;
  snprintf(command, sizeof(command), "%s CPUFreqHz=%d000000", warp_tool, mhz);
  return run(command, "NIL:") == 0 ? 0 : -1;
}
