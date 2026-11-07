/* Audio output for the Settlers port: a process of its own fills buffers
   from a mix function that the game provides (music and sound effects,
   16-bit stereo) - either a ring of 8-bit buffers in chip RAM for one left
   and one right channel of audio.device (Paula), or two 16-bit stereo
   buffers for ahi.device (AHI: a sound card, or Paula through AHI's
   drivers). The game and the mixer share their state under
   amiga_audio_lock(). Pattern of the ring and the channel allocation:
   Vastar's main_amiga.c. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <devices/audio.h>
#include <devices/ahi.h>
#include <utility/hooks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <graphics/gfxbase.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/ahi.h>
#include <clib/alib_protos.h>

#include <string.h>

#include "amiga_audio.h"

#define BUFS 4

static AmigaAudioFill fill_fn;
static void *fill_user;
static int frames;              /* samples per buffer and channel */
static int period;
static int rate;
static short *mix;              /* frames * 2, fast RAM */
static BYTE *chip[BUFS][2];     /* left, right */

static struct SignalSemaphore lock;
static struct Process *proc;
static struct Task *parent;
static BYTE parent_sig = -1;
static volatile int proc_state; /* 0 starting, 1 running, -1 failed, 2 done */
/* AHI instead of Paula (amiga_audio_use_ahi, before amiga_audio_open). */
static int use_ahi;
static int open_ahi;            /* the output running is AHI */

void amiga_audio_use_ahi(int on) { use_ahi = on; }
int amiga_audio_is_ahi(void) { return open_ahi; }

void amiga_audio_lock(void) { ObtainSemaphore(&lock); }
void amiga_audio_unlock(void) { ReleaseSemaphore(&lock); }
int amiga_audio_rate(void) { return rate; }

/* Diagnostics for STATS: times every queued buffer had played out before
   it was refilled (a gap in the sound), buffers rendered, E-clock ticks
   spent rendering, and the period and PAL flag the rate came from. */
static volatile unsigned long underruns, rendered, render_ticks;
static int pal_flag;
/* STATS: the highest level of the mix and its samples at full scale
   (clipped), since the last amiga_audio_levels. */
static volatile int level_peak;
static volatile unsigned long level_full;

void amiga_audio_levels(int *peak, unsigned long *full)
{
  *peak = level_peak;
  *full = level_full;
  level_peak = 0;
  level_full = 0;
}

static void measure(const short *m, int count)
{
  int i, peak = level_peak;
  unsigned long full = 0;
  for (i = 0; i < count; i++) {
    int v = m[i] < 0 ? -m[i] : m[i];
    if (v > peak) peak = v;
    if (v >= 32767) full++;
  }
  level_peak = peak;
  level_full += full;
}
extern unsigned long amiga_eclock(void);

void amiga_audio_stats(unsigned long *under, unsigned long *buffers,
                       unsigned long *ticks, int *per, int *pal)
{
  *under = underruns;
  *buffers = rendered;
  *ticks = render_ticks;
  *per = period;
  *pal = pal_flag;
}

/* One buffer of both channels from the mix function. */
static void render(int b)
{
  int i;
  BYTE *l = chip[b][0];
  BYTE *r = chip[b][1];
  unsigned long t0 = amiga_eclock();
  memset(mix, 0, frames * 2 * sizeof(short));
  ObtainSemaphore(&lock);
  fill_fn(mix, frames, fill_user);
  ReleaseSemaphore(&lock);
  measure(mix, frames * 2);
  for (i = 0; i < frames; i++) {
    int vl = mix[2 * i] >> 8;
    int vr = mix[2 * i + 1] >> 8;
    l[i] = (BYTE)(vl < -128 ? -128 : vl > 127 ? 127 : vl);
    r[i] = (BYTE)(vr < -128 ? -128 : vr > 127 ? 127 : vr);
  }
  render_ticks += amiga_eclock() - t0;
  rendered++;
}

/* AHI through the library interface of ahi.device (AHI_AllocAudio): two
   channels, the left one panned left and the right one right, each with a
   ring of AHI_RING dynamic 16-bit mono sounds (0..AHI_RING-1 left, the
   same + AHI_RING right), played in turn. The sound function comes for
   each channel when its next sound starts and queues the one after it on
   that channel only (the right channel starts its sound a moment after
   the left one: queuing for it from the left channel's call made it skip
   a sound - clicks). The left channel's call counts the sounds started and
   signals this process, which keeps the ring filled two sounds ahead of
   the one playing (the one after the queued one), catching up if it
   missed a signal. Both channels start in one AHI_Play, with the same
   length and rate.
   - The device interface (CMD_WRITE with ahir_Link) played at about half
     speed with the Warp driver: a gap after every buffer.
   - One channel with a stereo sound lost a side in a stereo mode without
     panning (Warp: HiFi 16 bit stereo): half of the effects (the original
     plays them on the left or the right) and of the music. */
struct Library *AHIBase;
#define AHI_RING 4               /* sounds per channel: 4 x 23 ms */
static struct Task *ahi_task;
static BYTE ahi_sig = -1;
static volatile UWORD ahi_pos[2];      /* the sound playing, per channel */
static volatile ULONG ahi_started;     /* sounds started on the left */

static ULONG ahi_sound_func(struct Hook *hook, struct AHIAudioCtrl *ac,
                            struct AHISoundMessage *msg)
{
  UWORD ch = msg->ahism_Channel;
  UWORD pos;
  (void)hook;
  if (ch > 1) return 0;
  pos = (UWORD)((ahi_pos[ch] + 1) % AHI_RING);
  ahi_pos[ch] = pos;
  AHI_SetSound(ch, (UWORD)(ch * AHI_RING + (pos + 1) % AHI_RING), 0, 0, ac,
               0);
  if (ch == 0) {
    ahi_started++;
    Signal(ahi_task, 1UL << ahi_sig);
  }
  return 0;
}

static struct Hook ahi_hook;

/* Sound s of both rings (left, right) from the mix (stereo, mixbuf). */
static void ahi_fill(short *mixbuf, short **buf, int s, int n)
{
  int i;
  short *left = buf[s], *right = buf[AHI_RING + s];
  unsigned long t0 = amiga_eclock();
  memset(mixbuf, 0, (size_t)n * 4);
  ObtainSemaphore(&lock);
  fill_fn(mixbuf, n, fill_user);
  ReleaseSemaphore(&lock);
  measure(mixbuf, n * 2);
  for (i = 0; i < n; i++) {
    left[i] = mixbuf[2 * i];
    right[i] = mixbuf[2 * i + 1];
  }
  render_ticks += amiga_eclock() - t0;
  rendered++;
}

static void ahi_process(void)
{
  struct MsgPort *port = NULL;
  struct AHIRequest *io = NULL;
  struct AHIAudioCtrl *ac = NULL;
  struct AHISampleInfo info;
  short *buf[2 * AHI_RING];
  short *mixbuf = NULL;
  int n = frames;
  ULONG bytes = (ULONG)n * 2;
  ULONG filled = 0;                    /* sounds filled since the start */
  int ok = 0, playing = 0, b;

  for (b = 0; b < 2 * AHI_RING; b++) buf[b] = NULL;
  ahi_task = FindTask(NULL);
  ahi_sig = AllocSignal(-1);
  port = CreateMsgPort();
  if (port != NULL) {
    io = (struct AHIRequest *)CreateIORequest(port, sizeof(*io));
  }
  if (io != NULL && ahi_sig >= 0) {
    io->ahir_Version = 4;
    if (OpenDevice((CONST_STRPTR)AHINAME, AHI_NO_UNIT,
                   (struct IORequest *)io, 0) == 0) {
      AHIBase = (struct Library *)io->ahir_Std.io_Device;
      ok = 1;
    } else {
      DeleteIORequest((struct IORequest *)io);
      io = NULL;
    }
  }
  if (ok) {
    mixbuf = (short *)AllocMem(bytes * 2, MEMF_PUBLIC | MEMF_CLEAR);
    ok = mixbuf != NULL;
    for (b = 0; b < 2 * AHI_RING && ok; b++) {
      buf[b] = (short *)AllocMem(bytes, MEMF_PUBLIC | MEMF_CLEAR);
      ok = buf[b] != NULL;
    }
  }
  if (ok) {
    ahi_hook.h_Entry = (HOOKFUNC)HookEntry;
    ahi_hook.h_SubEntry = (HOOKFUNC)ahi_sound_func;
    ahi_hook.h_Data = NULL;
    ac = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
                        AHIA_MixFreq, AHI_DEFAULT_FREQ,
                        AHIA_Channels, 2,
                        AHIA_Sounds, 2 * AHI_RING,
                        AHIA_SoundFunc, (ULONG)&ahi_hook,
                        TAG_DONE);
    ok = ac != NULL;
  }
  for (b = 0; b < 2 * AHI_RING && ok; b++) {
    info.ahisi_Type = AHIST_M16S;
    info.ahisi_Address = buf[b];
    info.ahisi_Length = (ULONG)n;
    if (AHI_LoadSound((UWORD)b, AHIST_DYNAMICSAMPLE, &info, ac) != AHIE_OK) {
      ok = 0;
    }
  }
  if (ok) {
    /* The first two sounds now, the third at the first start. The sound
       function comes for the first sound too: the rings start at their
       last sound, so that it makes them 0. */
    ahi_fill(mixbuf, buf, 0, n);
    ahi_fill(mixbuf, buf, 1, n);
    filled = 2;
    ahi_pos[0] = ahi_pos[1] = AHI_RING - 1;
    ahi_started = 0;
    if (AHI_ControlAudio(ac, AHIC_Play, TRUE, TAG_DONE) != AHIE_OK) {
      ok = 0;
    } else {
      playing = 1;
      AHI_Play(ac,
               AHIP_BeginChannel, 0UL,
               AHIP_Freq, (ULONG)rate,
               AHIP_Vol, 0x10000UL,
               AHIP_Pan, 0UL,
               AHIP_Sound, 0UL,
               AHIP_EndChannel, 0UL,
               AHIP_BeginChannel, 1UL,
               AHIP_Freq, (ULONG)rate,
               AHIP_Vol, 0x10000UL,
               AHIP_Pan, 0x10000UL,
               AHIP_Sound, (ULONG)AHI_RING,
               AHIP_EndChannel, 0UL,
               TAG_DONE);
    }
  }

  proc_state = ok ? 1 : -1;
  Signal(parent, 1UL << parent_sig);

  if (ok) {
    for (;;) {
      ULONG got = Wait((1UL << ahi_sig) | SIGBREAKF_CTRL_C);
      ULONG want;
      if (got & SIGBREAKF_CTRL_C) break;
      /* Sound k (counted from 0) playing since the k+1-th start: filled
         up to k + 2 (the one after the queued one). */
      want = ahi_started + 2;
      if (want > filled + 1) underruns++;   /* more than one to catch up */
      while (filled < want) {
        ahi_fill(mixbuf, buf, (int)(filled % AHI_RING), n);
        filled++;
      }
    }
  }

  if (ac != NULL) {
    if (playing) AHI_ControlAudio(ac, AHIC_Play, FALSE, TAG_DONE);
    AHI_FreeAudio(ac);
  }
  for (b = 0; b < 2 * AHI_RING; b++) {
    if (buf[b] != NULL) FreeMem(buf[b], bytes);
  }
  if (mixbuf != NULL) FreeMem(mixbuf, bytes * 2);
  if (io != NULL) {
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
  }
  if (port != NULL) DeleteMsgPort(port);
  if (ahi_sig >= 0) {
    FreeSignal(ahi_sig);
    ahi_sig = -1;
  }

  Forbid();          /* the parent may free our code once signalled */
  proc_state = 2;
  Signal(parent, 1UL << parent_sig);
}

static void audio_process(void)
{
  static UBYTE channels[] = { 3, 5, 10, 12 };
  struct MsgPort *port;
  struct IOAudio *alloc;
  struct IOAudio *io[BUFS][2];
  int busy[BUFS];
  int next = 0;
  ULONG units;
  int b, c, ok = 0;

  memset(io, 0, sizeof(io));
  port = CreateMsgPort();
  alloc = port ? (struct IOAudio *)CreateIORequest(port, sizeof(*alloc))
               : NULL;
  if (alloc != NULL) {
    alloc->ioa_Request.io_Message.mn_Node.ln_Pri = 1;
    alloc->ioa_Request.io_Flags = ADIOF_NOWAIT;
    alloc->ioa_Data = channels;
    alloc->ioa_Length = sizeof(channels);
    if (OpenDevice((CONST_STRPTR)AUDIONAME, 0,
                   (struct IORequest *)alloc, 0) == 0) {
      ok = 1;
    } else {
      DeleteIORequest((struct IORequest *)alloc);
      alloc = NULL;
    }
  }
  if (ok) {
    units = (ULONG)alloc->ioa_Request.io_Unit;
    for (b = 0; b < BUFS && ok; b++) {
      for (c = 0; c < 2; c++) {
        io[b][c] = (struct IOAudio *)AllocMem(sizeof(struct IOAudio),
                                              MEMF_PUBLIC | MEMF_CLEAR);
        if (io[b][c] == NULL) {
          ok = 0;
          break;
        }
        *io[b][c] = *alloc;
        /* left: channels 0 and 3, right: 1 and 2 */
        io[b][c]->ioa_Request.io_Unit =
          (struct Unit *)(units & (c ? 0x06 : 0x09));
        io[b][c]->ioa_Data = (UBYTE *)chip[b][c];
        io[b][c]->ioa_Period = (UWORD)period;
        io[b][c]->ioa_Volume = 64;
        io[b][c]->ioa_Cycles = 1;
      }
    }
  }

  proc_state = ok ? 1 : -1;
  Signal(parent, 1UL << parent_sig);

  if (ok) {
    for (b = 0; b < BUFS; b++) {
      render(b);
      for (c = 0; c < 2; c++) {
        io[b][c]->ioa_Request.io_Command = CMD_WRITE;
        io[b][c]->ioa_Request.io_Flags = ADIOF_PERVOL;
        io[b][c]->ioa_Length = frames;
        BeginIO((struct IORequest *)io[b][c]);
      }
      busy[b] = 2;
    }
    for (;;) {
      ULONG got = Wait((1UL << port->mp_SigBit) | SIGBREAKF_CTRL_C);
      struct Message *m;
      if (got & SIGBREAKF_CTRL_C) break;
      while ((m = GetMsg(port)) != NULL) {
        for (b = 0; b < BUFS; b++) {
          for (c = 0; c < 2; c++) {
            if (m == &io[b][c]->ioa_Request.io_Message) busy[b]--;
          }
        }
      }
      {
        int idle = 1;
        for (b = 0; b < BUFS; b++) {
          if (busy[b] != 0) idle = 0;
        }
        if (idle) underruns++;
      }
      /* Refill the buffers whose two halves both finished, in the order
         of the ring, so that they are queued again in the same order. */
      while (busy[next] == 0) {
        b = next;
        render(b);
        for (c = 0; c < 2; c++) {
          io[b][c]->ioa_Request.io_Command = CMD_WRITE;
          io[b][c]->ioa_Request.io_Flags = ADIOF_PERVOL;
          io[b][c]->ioa_Length = frames;
          BeginIO((struct IORequest *)io[b][c]);
        }
        busy[b] = 2;
        next = (next + 1) % BUFS;
      }
    }
    for (b = 0; b < BUFS; b++) {
      for (c = 0; c < 2; c++) {
        if (busy[b] > 0) {
          AbortIO((struct IORequest *)io[b][c]);
          WaitIO((struct IORequest *)io[b][c]);
        }
      }
    }
  }

  for (b = 0; b < BUFS; b++) {
    for (c = 0; c < 2; c++) {
      if (io[b][c] != NULL) FreeMem(io[b][c], sizeof(struct IOAudio));
    }
  }
  if (alloc != NULL) {
    CloseDevice((struct IORequest *)alloc);
    DeleteIORequest((struct IORequest *)alloc);
  }
  if (port != NULL) DeleteMsgPort(port);

  Forbid();          /* the parent may free our code once signalled */
  proc_state = 2;
  Signal(parent, 1UL << parent_sig);
}

int amiga_audio_open(int samples_per_buffer, AmigaAudioFill fn, void *user)
{
  ULONG clock;
  int b, c;

  fill_fn = fn;
  fill_user = user;
  frames = samples_per_buffer;
  pal_flag = (GfxBase->DisplayFlags & PAL) != 0;
  clock = pal_flag ? 3546895UL : 3579545UL;
  /* 22 kHz; the period rounded up, so the rate stays at most 22050
     (PAL 161 -> 22030 Hz). */
  period = (int)((clock + 22049UL) / 22050UL);
  rate = (int)(clock / period);

  InitSemaphore(&lock);
  mix = (short *)AllocMem(frames * 2 * sizeof(short), MEMF_ANY);
  if (mix == NULL) return -1;
  for (b = 0; b < BUFS; b++) {
    for (c = 0; c < 2; c++) {
      chip[b][c] = (BYTE *)AllocMem(frames, MEMF_CHIP | MEMF_CLEAR);
      if (chip[b][c] == NULL) {
        amiga_audio_close();
        return -1;
      }
    }
  }

  parent = FindTask(NULL);
  parent_sig = AllocSignal(-1);
  if (parent_sig < 0) {
    amiga_audio_close();
    return -1;
  }
  proc_state = 0;
  open_ahi = use_ahi;
  proc = CreateNewProcTags(NP_Entry,
                           (ULONG)(use_ahi ? ahi_process : audio_process),
                           NP_Name, (ULONG)"Settlers audio",
                           NP_Priority, 10,
                           NP_StackSize, 16384,
                           TAG_DONE);
  if (proc == NULL) {
    amiga_audio_close();
    return -1;
  }
  while (proc_state == 0) Wait(1UL << parent_sig);
  if (proc_state < 0) {
    while (proc_state != 2) Wait(1UL << parent_sig);
    proc = NULL;
    open_ahi = 0;
    amiga_audio_close();
    return -1;
  }
  return 0;
}

void amiga_audio_close(void)
{
  int b, c;
  if (proc != NULL) {
    Signal(&proc->pr_Task, SIGBREAKF_CTRL_C);
    while (proc_state != 2) Wait(1UL << parent_sig);
    proc = NULL;
  }
  if (parent_sig >= 0) {
    FreeSignal(parent_sig);
    parent_sig = -1;
  }
  for (b = 0; b < BUFS; b++) {
    for (c = 0; c < 2; c++) {
      if (chip[b][c] != NULL) {
        FreeMem(chip[b][c], frames);
        chip[b][c] = NULL;
      }
    }
  }
  if (mix != NULL) {
    FreeMem(mix, frames * 2 * sizeof(short));
    mix = NULL;
  }
}
