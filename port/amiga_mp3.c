/* The MP3 player of the music box: mhi.library of the Warp card (Thomas
   Wenzel and Paul Qureshi's MHI standard) for the decoding, asl.library
   for the file picker. The popup (music-box.cc) calls only this; the
   AmigaOS headers stay here.

   MHI uses a buffer queue (not a stream): the game queues chunks of the
   MP3 file (MHIQueueBuffer), MHI decodes, and the game recycles drained
   chunks (MHIGetEmpty) and refills them from the file. The API is from
   Aminet driver/audio/mhi_dev (Autodoc/mhi.doc, Include/fd/mhi_lib.fd,
   MHIplay/MHIplay.c). */

/* The base variable of asl.library's inline wrappers is named here, so
   that the one global of this module is Mp3AslBase (and does not clash
   with a library the rest of the port may open on its own). */
#define AslBase Mp3AslBase

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/exall.h>
#include <libraries/asl.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/utility.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "amiga_mp3.h"

/* The list stays between opens of the music box; one list, 128 tracks at
   most. Paths include the volume so that the list goes with the player's
   file no matter which drawer is current. */
#define MP3_MAX_TRACKS 128
#define MP3_PATH_LEN 256

static char mp3_paths[MP3_MAX_TRACKS][MP3_PATH_LEN];
static int mp3_count_v;
static int mp3_volume = 80;

/* mhi.library (mhi_lib.fd of the MHI developer kit):
     -30 MHIAllocDecoder(task A0, mhisignal_mask D0) -> handle (D0)
     -36 MHIFreeDecoder(handle A3)
     -42 MHIQueueBuffer(handle A3, buffer A0, size D0) -> BOOL (D0)
     -48 MHIGetEmpty(handle A3) -> buffer (D0)
     -54 MHIGetStatus(handle A3) -> status UBYTE (D0)
     -60 MHIPlay(handle A3)
     -66 MHIStop(handle A3)
     -72 MHIPause(handle A3)
     -78 MHIQuery(query D1) -> string ptr (D0)
     -84 MHISetParam(handle A3, param D0, value D1)
   The handle goes in A3 for every call but AllocDecoder. */

#define MHIP_VOLUME 0     /* MHI parameter: 0 muted .. 100 full */

#define MHIF_PLAYING      0
#define MHIF_STOPPED      1
#define MHIF_OUT_OF_DATA  2
#define MHIF_PAUSED       3

static struct Library *MHIBase;
static APTR mhi_handle;
static BYTE mhi_signal = -1;
static ULONG mhi_sigmask;
static char mhi_driver[32];

/* bebbo gcc miscompiled the function-pointer form (it kept folding
   `_bn - bias` into `jsr +bias(An)` with the wrong sign), so each MHI
   call is written as an inline asm that does `jsr -<bias>(a6)`
   explicitly. */

static inline APTR mhi_alloc_decoder(struct Task *task, ULONG sigmask) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register struct Task *a0 __asm("a0") = task;
  register ULONG d0 __asm("d0") = sigmask;
  register APTR ret __asm("d0");
  __asm volatile ("jsr %%a6@(-30)"
                  : "=r"(ret)
                  : "r"(a6), "r"(a0), "r"(d0)
                  : "d1", "a1", "cc", "memory");
  return ret;
}
#define MHIAllocDecoder(task, sigmask) mhi_alloc_decoder((task), (sigmask))

static inline void mhi_free_decoder(APTR handle) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  __asm volatile ("jsr %%a6@(-36)"
                  :: "r"(a6), "r"(a3)
                  : "d0", "d1", "a0", "a1", "cc", "memory");
}
#define MHIFreeDecoder(h) mhi_free_decoder((h))

static inline BOOL mhi_queue_buffer(APTR handle, APTR buf, ULONG size) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  register APTR a0 __asm("a0") = buf;
  register ULONG d0 __asm("d0") = size;
  register BOOL ret __asm("d0");
  __asm volatile ("jsr %%a6@(-42)"
                  : "=r"(ret)
                  : "r"(a6), "r"(a3), "r"(a0), "r"(d0)
                  : "d1", "a1", "cc", "memory");
  return ret;
}
#define MHIQueueBuffer(h, b, s) mhi_queue_buffer((h), (b), (s))

static inline APTR mhi_get_empty(APTR handle) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  register APTR ret __asm("d0");
  __asm volatile ("jsr %%a6@(-48)"
                  : "=r"(ret)
                  : "r"(a6), "r"(a3)
                  : "d1", "a0", "a1", "cc", "memory");
  return ret;
}
#define MHIGetEmpty(h) mhi_get_empty((h))

static inline UBYTE mhi_get_status(APTR handle) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  register ULONG ret __asm("d0");
  __asm volatile ("jsr %%a6@(-54)"
                  : "=r"(ret)
                  : "r"(a6), "r"(a3)
                  : "d1", "a0", "a1", "cc", "memory");
  return (UBYTE)ret;
}
#define MHIGetStatus(h) mhi_get_status((h))

static inline void mhi_play(APTR handle) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  __asm volatile ("jsr %%a6@(-60)"
                  :: "r"(a6), "r"(a3)
                  : "d0", "d1", "a0", "a1", "cc", "memory");
}
#define MHIPlay(h) mhi_play((h))

static inline void mhi_stop(APTR handle) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  __asm volatile ("jsr %%a6@(-66)"
                  :: "r"(a6), "r"(a3)
                  : "d0", "d1", "a0", "a1", "cc", "memory");
}
#define MHIStop(h) mhi_stop((h))

static inline void mhi_pause(APTR handle) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  __asm volatile ("jsr %%a6@(-72)"
                  :: "r"(a6), "r"(a3)
                  : "d0", "d1", "a0", "a1", "cc", "memory");
}
#define MHIPause(h) mhi_pause((h))

static inline void mhi_set_param(APTR handle, UWORD param, ULONG value) {
  register struct Library *a6 __asm("a6") = MHIBase;
  register APTR a3 __asm("a3") = handle;
  register ULONG d0 __asm("d0") = param;
  register ULONG d1 __asm("d1") = value;
  __asm volatile ("jsr %%a6@(-84)"
                  :: "r"(a6), "r"(a3), "r"(d0), "r"(d1)
                  : "a0", "a1", "cc", "memory");
}
#define MHISetParam(h, p, v) mhi_set_param((h), (p), (v))

/* Reading the file in chunks of MP3_BUF; a pool of MP3_BUFS chunks is
   given to MHI, and reused chunks come back through MHIGetEmpty. */
#define MP3_BUF    (128 * 128)   /* 16 KB per chunk, as MHIplay.c does */
#define MP3_BUFS   8             /* 8 chunks * 16 KB = ~128 KB of data */

static BPTR mp3_fh;
static UBYTE *mp3_bufs[MP3_BUFS];
static int mp3_bufs_alloc;
static int mp3_current = -1;      /* -1: nothing is playing */
static int mp3_paused_flag;
static int mp3_eof;               /* 1: the file has all been read */

/* Drivers tried in order; the first one that opens is kept. The MHI
   drivers live in LIBS:MHI/ (a plain subdirectory of LIBS:, not on the
   path OpenLibrary knows about), so each name is tried with a LIBS:MHI/
   prefix and as a bare name (LIBS: itself). No "MHI:" assign is tried:
   when the standard MHI SDK is not installed, it would pop a volume
   requester - and pr_WindowPtr is also set to -1 in the probe below. */
static const char * const mhi_names[] = {
  "LIBS:MHI/mhiArmedWARP.library",  /* Warp's MHI driver, as installed */
  "mhiArmedWARP.library",           /* bare name, LIBS: itself */
  "LIBS:MHI/mhiwarp.library",       /* older or stripped-down name */
  "mhiwarp.library",
  "LIBS:MHI/mhimas.library",        /* MAS Player */
  "mhimas.library",
  "LIBS:MHI/mhimaspro.library",     /* MAS Player Pro from the MHI SDK */
  "mhimaspro.library",
  "LIBS:MHI/mhimasstd.library",
  "mhimasstd.library",
  "LIBS:MHI/mhimelody.library",     /* Melody 1200 */
  "mhimelody.library",
  NULL
};

int amiga_mp3_have_decoder(void) { return MHIBase != NULL && mhi_handle != NULL; }
const char *amiga_mp3_driver(void) { return mhi_driver; }

int amiga_mp3_open(void) {
  int i;
  struct Process *me = (struct Process *)FindTask(NULL);
  APTR old_wptr;
  BPTR mhilog;
  if (MHIBase != NULL && mhi_handle != NULL) return 1;
  mhilog = Open((CONST_STRPTR)"RAM:Settlers-mhi.txt", MODE_NEWFILE);
  if (MHIBase == NULL) {
    /* pr_WindowPtr = -1 keeps dos.library from asking "Please insert
       volume ... in any drive" on a bad path. */
    old_wptr = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    for (i = 0; mhi_names[i] != NULL; i++) {
      MHIBase = OpenLibrary((CONST_STRPTR)mhi_names[i], 0);
      if (mhilog != 0) {
        char line[160];
        int n;
        if (MHIBase != NULL) {
          n = snprintf(line, sizeof(line),
                       "OK   %s (base=%p, ver=%d.%d)\n",
                       mhi_names[i], (void *)MHIBase,
                       MHIBase->lib_Version, MHIBase->lib_Revision);
        } else {
          n = snprintf(line, sizeof(line), "fail %s (IoErr %ld)\n",
                       mhi_names[i], (long)IoErr());
        }
        if (n > 0) Write(mhilog, line, n);
      }
      if (MHIBase != NULL) {
        const char *base = mhi_names[i];
        const char *c;
        for (c = base; *c; c++) {
          if (*c == '/' || *c == ':') base = c + 1;
        }
        strncpy(mhi_driver, base, sizeof(mhi_driver) - 1);
        mhi_driver[sizeof(mhi_driver) - 1] = 0;
        break;
      }
    }
    me->pr_WindowPtr = old_wptr;
  }
  if (MHIBase == NULL) {
    if (mhilog != 0) Close(mhilog);
    return 0;
  }
  mhi_signal = AllocSignal(-1);
  if (mhi_signal < 0) {
    if (mhilog != 0) {
      const char *line = "AllocSignal failed\n";
      Write(mhilog, (APTR)line, strlen(line));
      Close(mhilog);
    }
    CloseLibrary(MHIBase);
    MHIBase = NULL;
    return 0;
  }
  mhi_sigmask = 1UL << mhi_signal;
  mhi_handle = MHIAllocDecoder(FindTask(NULL), mhi_sigmask);
  if (mhilog != 0) {
    char line[128];
    int n = snprintf(line, sizeof(line),
                     "AllocSignal -> %d, mask=0x%08lx; "
                     "MHIAllocDecoder -> %p\n",
                     (int)mhi_signal, (unsigned long)mhi_sigmask, mhi_handle);
    if (n > 0) Write(mhilog, line, n);
    Close(mhilog);
  }
  if (mhi_handle == NULL) {
    FreeSignal(mhi_signal);
    mhi_signal = -1;
    CloseLibrary(MHIBase);
    MHIBase = NULL;
    return 0;
  }
  /* The buffer pool is kept for the game run: allocating 128 KB every
     time a track starts would be wasteful. */
  for (i = 0; i < MP3_BUFS; i++) {
    mp3_bufs[i] = (UBYTE *)AllocMem(MP3_BUF, MEMF_ANY);
    if (mp3_bufs[i] == NULL) {
      while (--i >= 0) { FreeMem(mp3_bufs[i], MP3_BUF); mp3_bufs[i] = NULL; }
      MHIFreeDecoder(mhi_handle);
      mhi_handle = NULL;
      FreeSignal(mhi_signal);
      mhi_signal = -1;
      CloseLibrary(MHIBase);
      MHIBase = NULL;
      return 0;
    }
  }
  mp3_bufs_alloc = MP3_BUFS;
  MHISetParam(mhi_handle, MHIP_VOLUME, (ULONG)mp3_volume);
  return 1;
}

void amiga_mp3_close(void) {
  int i;
  amiga_mp3_stop();
  for (i = 0; i < mp3_bufs_alloc; i++) {
    if (mp3_bufs[i] != NULL) { FreeMem(mp3_bufs[i], MP3_BUF); mp3_bufs[i] = NULL; }
  }
  mp3_bufs_alloc = 0;
  if (mhi_handle != NULL && MHIBase != NULL) MHIFreeDecoder(mhi_handle);
  mhi_handle = NULL;
  if (mhi_signal >= 0) { FreeSignal(mhi_signal); mhi_signal = -1; }
  mhi_sigmask = 0;
  if (MHIBase != NULL) { CloseLibrary(MHIBase); MHIBase = NULL; }
  if (AslBase != NULL) { CloseLibrary(AslBase); AslBase = NULL; }
  mhi_driver[0] = 0;
}

int amiga_mp3_count(void) { return mp3_count_v; }

const char *amiga_mp3_path(int index) {
  if (index < 0 || index >= mp3_count_v) return "";
  return mp3_paths[index];
}

const char *amiga_mp3_name(int index) {
  const char *p = amiga_mp3_path(index);
  const char *name = p;
  const char *c;
  for (c = p; *c; c++) {
    if (*c == '/' || *c == ':') name = c + 1;
  }
  return name;
}

void amiga_mp3_remove(int index) {
  int i;
  if (index < 0 || index >= mp3_count_v) return;
  if (index == mp3_current) amiga_mp3_stop();
  for (i = index; i < mp3_count_v - 1; i++) {
    strcpy(mp3_paths[i], mp3_paths[i + 1]);
  }
  mp3_count_v--;
  if (mp3_current > index) mp3_current--;
}

void amiga_mp3_clear(void) {
  amiga_mp3_stop();
  mp3_count_v = 0;
}

static int has_mp3_ext(const char *name) {
  size_t n = strlen(name);
  if (n < 5) return 0;
  return tolower(name[n - 4]) == '.' &&
         tolower(name[n - 3]) == 'm' &&
         tolower(name[n - 2]) == 'p' &&
         tolower(name[n - 1]) == '3';
}

static int path_in_list(const char *path) {
  int i;
  for (i = 0; i < mp3_count_v; i++) {
    if (strcmp(mp3_paths[i], path) == 0) return 1;
  }
  return 0;
}

static int add_path(const char *path) {
  if (mp3_count_v >= MP3_MAX_TRACKS) return 0;
  if (strlen(path) >= MP3_PATH_LEN) return 0;
  if (path_in_list(path)) return 0;
  strcpy(mp3_paths[mp3_count_v], path);
  mp3_count_v++;
  return 1;
}

static void join_path(char *dst, size_t dst_size,
                      const char *drawer, const char *file) {
  size_t d = strlen(drawer);
  if (d >= dst_size) { dst[0] = 0; return; }
  strcpy(dst, drawer);
  if (d > 0 && drawer[d - 1] != '/' && drawer[d - 1] != ':') {
    if (d + 1 >= dst_size) return;
    dst[d++] = '/';
    dst[d] = 0;
  }
  if (d + strlen(file) >= dst_size) return;
  strcat(dst, file);
}

struct Library *AslBase;

static int open_asl(void) {
  if (AslBase != NULL) return 1;
  AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 38);
  if (AslBase == NULL) return 0;
  return 1;
}

static char last_drawer[MP3_PATH_LEN] = "PROGDIR:MP3";

int amiga_mp3_add_files(void) {
  struct FileRequester *req;
  int added = 0;
  if (!open_asl()) return 0;
  req = (struct FileRequester *)
    AllocAslRequestTags(ASL_FileRequest,
                        ASLFR_TitleText, (ULONG)"Add MP3 files",
                        ASLFR_InitialDrawer, (ULONG)last_drawer,
                        ASLFR_InitialPattern, (ULONG)"#?.(mp3|MP3)",
                        ASLFR_DoPatterns, TRUE,
                        ASLFR_DoMultiSelect, TRUE,
                        ASLFR_RejectIcons, TRUE,
                        TAG_DONE);
  if (req == NULL) return 0;
  if (AslRequestTags(req, TAG_DONE)) {
    strncpy(last_drawer, (char *)req->fr_Drawer, sizeof(last_drawer) - 1);
    last_drawer[sizeof(last_drawer) - 1] = 0;
    if (req->fr_NumArgs > 0 && req->fr_ArgList != NULL) {
      LONG i;
      for (i = 0; i < req->fr_NumArgs; i++) {
        char full[MP3_PATH_LEN];
        join_path(full, sizeof(full), (char *)req->fr_Drawer,
                  (char *)req->fr_ArgList[i].wa_Name);
        if (add_path(full)) added++;
      }
    } else if (req->fr_File != NULL && req->fr_File[0] != 0) {
      char full[MP3_PATH_LEN];
      join_path(full, sizeof(full), (char *)req->fr_Drawer,
                (char *)req->fr_File);
      if (add_path(full)) added++;
    }
  }
  FreeAslRequest(req);
  return added;
}

#define EXALL_BUF 4096

/* Case-insensitive order of names: the tracks of a drawer in the order
   of their names (01, 02, 03... or A, B, C...). */
static int name_cmp(const char *a, const char *b) {
  while (*a && *b) {
    int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
    if (ca != cb) return ca - cb;
    a++; b++;
  }
  return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* Adds the *.mp3 of a drawer to the list, sorted by name among
   themselves. Returns the number of files added. */
static int scan_drawer(const char *drawer) {
  int added = 0;
  int first = mp3_count_v;
  {
    BPTR lock = Lock((CONST_STRPTR)drawer, SHARED_LOCK);
    if (lock != 0) {
      struct ExAllControl *eac;
      UBYTE *eabuf;
      eac = (struct ExAllControl *)
        AllocDosObject(DOS_EXALLCONTROL, NULL);
      eabuf = (UBYTE *)AllocMem(EXALL_BUF, MEMF_ANY);
      if (eac != NULL && eabuf != NULL) {
        BOOL more;
        eac->eac_LastKey = 0;
        eac->eac_MatchString = NULL;
        eac->eac_MatchFunc = NULL;
        do {
          more = ExAll(lock, (struct ExAllData *)eabuf, EXALL_BUF,
                       ED_NAME, eac);
          if (!more && IoErr() != ERROR_NO_MORE_ENTRIES) break;
          if (eac->eac_Entries > 0) {
            struct ExAllData *ed = (struct ExAllData *)eabuf;
            ULONG n = eac->eac_Entries;
            while (n--) {
              if (has_mp3_ext((char *)ed->ed_Name)) {
                char full[MP3_PATH_LEN];
                join_path(full, sizeof(full), drawer,
                          (char *)ed->ed_Name);
                if (add_path(full)) added++;
              }
              ed = ed->ed_Next;
              if (ed == NULL) break;
            }
          }
        } while (more);
      }
      if (eac != NULL) FreeDosObject(DOS_EXALLCONTROL, eac);
      if (eabuf != NULL) FreeMem(eabuf, EXALL_BUF);
      UnLock(lock);
    }
  }
  {
    /* Insertion sort of the new entries (at most 128). */
    int i, j;
    char tmp[MP3_PATH_LEN];
    for (i = first + 1; i < mp3_count_v; i++) {
      strcpy(tmp, mp3_paths[i]);
      for (j = i; j > first && name_cmp(mp3_paths[j - 1], tmp) > 0; j--) {
        strcpy(mp3_paths[j], mp3_paths[j - 1]);
      }
      strcpy(mp3_paths[j], tmp);
    }
  }
  return added;
}

/* The drawer MP3 of the game drawer (PROGDIR:MP3): its files are on the
   list from the start. The path of the lock (NameFromLock) goes on the
   list, as with the files the player adds. */
int amiga_mp3_scan_game_drawer(void) {
  char drawer[MP3_PATH_LEN];
  BPTR lock = Lock((CONST_STRPTR)"PROGDIR:MP3", SHARED_LOCK);
  if (lock == 0) return 0;
  if (!NameFromLock(lock, (STRPTR)drawer, sizeof(drawer))) {
    strcpy(drawer, "PROGDIR:MP3");
  }
  UnLock(lock);
  strncpy(last_drawer, drawer, sizeof(last_drawer) - 1);
  last_drawer[sizeof(last_drawer) - 1] = 0;
  return scan_drawer(drawer);
}

int amiga_mp3_add_drawer(void) {
  struct FileRequester *req;
  char drawer[MP3_PATH_LEN];
  if (!open_asl()) return 0;
  req = (struct FileRequester *)
    AllocAslRequestTags(ASL_FileRequest,
                        ASLFR_TitleText, (ULONG)"Add a drawer of MP3 files",
                        ASLFR_InitialDrawer, (ULONG)last_drawer,
                        ASLFR_DrawersOnly, TRUE,
                        ASLFR_RejectIcons, TRUE,
                        TAG_DONE);
  if (req == NULL) return 0;
  drawer[0] = 0;
  if (AslRequestTags(req, TAG_DONE)) {
    strncpy(drawer, (char *)req->fr_Drawer, sizeof(drawer) - 1);
    drawer[sizeof(drawer) - 1] = 0;
    strncpy(last_drawer, drawer, sizeof(last_drawer) - 1);
    last_drawer[sizeof(last_drawer) - 1] = 0;
  }
  FreeAslRequest(req);
  if (drawer[0] == 0) return 0;
  return scan_drawer(drawer);
}

/* State of each pool buffer: "free" (owned by us, not queued) or "queued"
   (given to MHI). The pool itself lives in mp3_bufs[]; a bit set here
   says the buffer is queued. */
static ULONG mp3_queued_bits;

static int next_free_buffer(int *out_index) {
  int i;
  for (i = 0; i < mp3_bufs_alloc; i++) {
    if (!(mp3_queued_bits & (1UL << i))) {
      *out_index = i;
      return 1;
    }
  }
  return 0;
}

static void mark_queued(int index) { mp3_queued_bits |= (1UL << index); }

static int buffer_index_of(APTR p) {
  int i;
  for (i = 0; i < mp3_bufs_alloc; i++) {
    if (mp3_bufs[i] == p) return i;
  }
  return -1;
}

/* Fills one free buffer from the open file and queues it. Returns 1 when
   anything was queued, 0 at end of file or on failure. */
static int queue_one(void) {
  int idx;
  LONG n;
  if (mp3_fh == 0 || mp3_eof) return 0;
  if (mhi_handle == NULL) return 0;
  if (!next_free_buffer(&idx)) return 0;
  n = Read(mp3_fh, mp3_bufs[idx], MP3_BUF);
  if (n <= 0) {
    mp3_eof = 1;
    return 0;
  }
  if (!MHIQueueBuffer(mhi_handle, mp3_bufs[idx], n)) {
    /* Not queued: MHI refused it (its own queue full?); try again later. */
    return 0;
  }
  mark_queued(idx);
  return 1;
}

static void close_file(void) {
  if (mp3_fh != 0) { Close(mp3_fh); mp3_fh = 0; }
  mp3_eof = 0;
}

void amiga_mp3_stop(void) {
  if (mhi_handle != NULL) MHIStop(mhi_handle);
  close_file();
  mp3_queued_bits = 0;
  mp3_current = -1;
  mp3_paused_flag = 0;
}

void amiga_mp3_play(int index) {
  int i;
  if (index < 0 || index >= mp3_count_v) return;
  amiga_mp3_stop();
  if (mhi_handle == NULL) {
    if (!amiga_mp3_open()) return;
  }
  mp3_fh = Open((CONST_STRPTR)mp3_paths[index], MODE_OLDFILE);
  if (mp3_fh == 0) return;
  mp3_current = index;
  mp3_paused_flag = 0;
  mp3_eof = 0;
  mp3_queued_bits = 0;
  /* Pre-fill the whole queue (MHIplay.c: "Preloading"). */
  for (i = 0; i < mp3_bufs_alloc; i++) {
    if (!queue_one()) break;
  }
  MHIPlay(mhi_handle);
}

void amiga_mp3_pause(void) {
  if (mhi_handle == NULL) return;
  if (mp3_current < 0) return;
  if (mp3_paused_flag) {
    /* Resume: MHIPlay from pause (MHIPlay doc "may begin again at
       exactly where it left off"). */
    MHIPlay(mhi_handle);
    mp3_paused_flag = 0;
  } else {
    MHIPause(mhi_handle);
    mp3_paused_flag = 1;
  }
}

int amiga_mp3_is_playing(void) {
  return mhi_handle != NULL && mp3_current >= 0 && !mp3_paused_flag;
}
int amiga_mp3_is_paused(void) { return mp3_paused_flag != 0; }
int amiga_mp3_current(void) { return mp3_current; }

void amiga_mp3_set_volume(int volume) {
  if (volume < 0) volume = 0;
  if (volume > 100) volume = 100;
  mp3_volume = volume;
  if (mhi_handle != NULL) {
    MHISetParam(mhi_handle, MHIP_VOLUME, (ULONG)mp3_volume);
  }
}

int amiga_mp3_get_volume(void) { return mp3_volume; }

void amiga_mp3_tick(void) {
  APTR empty;
  if (mhi_handle == NULL || mp3_current < 0) return;
  /* A buffer at a time: MHIGetEmpty returns one drained buffer, or NULL
     when none drained yet. Clear the MHI signal that may have fired. */
  SetSignal(0, mhi_sigmask);
  while ((empty = MHIGetEmpty(mhi_handle)) != NULL) {
    int idx = buffer_index_of(empty);
    if (idx >= 0) mp3_queued_bits &= ~(1UL << idx);
    if (!mp3_paused_flag) queue_one();
  }
  /* Out of data at EOF: wait for MHI to drain, then close. */
  if (mp3_eof && mp3_queued_bits == 0) {
    UBYTE s = MHIGetStatus(mhi_handle);
    if (s == MHIF_STOPPED || s == MHIF_OUT_OF_DATA) {
      /* The next track, after the last one the first again (1, 2, 3, 1,
         2, 3...); one that does not open is skipped, all of them failing
         stops. */
      int from = mp3_current, tries;
      MHIStop(mhi_handle);
      close_file();
      mp3_current = -1;
      for (tries = 1; tries <= mp3_count_v; tries++) {
        amiga_mp3_play((from + tries) % mp3_count_v);
        if (mp3_current >= 0) break;
      }
    }
  }
}
