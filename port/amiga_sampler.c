/*
 * amiga_sampler.c - a sampling profiler for the Amiga (PCSAMPLE=s,n,us)
 *
 * A process of higher priority wakes every `us` microseconds (timer.device,
 * UNIT_MICROHZ) and notes where the game's task was stopped: the program
 * counter exec saved on its stack (tc_SPReg, under the FPU state) when the
 * timer interrupt took the CPU from it. After `delay` seconds it takes `count`
 * samples and writes them to RAM:Settlers-pcs.bin: "PCS1", the address of
 * amiga_sampler_start (to find the code hunk), the number of samples,
 * then the samples (0: the game was waiting). tools/pcprof.py turns them
 * into a list of functions.
 *
 * Probes in the code read the CIA timer (~18 us each) and change what
 * they measure; this does not touch the game at all.
 */
#include <exec/exec.h>
#include <exec/tasks.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "port/amiga_sampler.h"

static struct Task *target;
static ULONG *samples;
static volatile ULONG nsamples;
static ULONG maxsamples;
static ULONG delay_s;
static ULONG interval_us;
static volatile int quit;
static volatile int done;

static void sampler_main(void) {
  struct MsgPort *port = CreateMsgPort();
  struct timerequest *tr = NULL;
  ULONG waited = 0;
  BPTR fh;
  if (port != NULL) {
    tr = (struct timerequest *)CreateIORequest(port, sizeof(*tr));
  }
  if (tr == NULL || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                               (struct IORequest *)tr, 0) != 0) {
    if (tr != NULL) DeleteIORequest((struct IORequest *)tr);
    if (port != NULL) DeleteMsgPort(port);
    done = 1;
    return;
  }
  /* The delay a second at a time, so that quitting is noticed. */
  while (!quit && waited < delay_s) {
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = 1;
    tr->tr_time.tv_micro = 0;
    DoIO((struct IORequest *)tr);
    waited++;
  }
  while (!quit && nsamples < maxsamples) {
    ULONG pc = 0;
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = 0;
    tr->tr_time.tv_micro = interval_us;
    DoIO((struct IORequest *)tr);
    Disable();
    if (target->tc_State == TS_READY) {
      /* AmigaOS 3.2 on the 68060 (seen on the Warp): at tc_SPReg -1, the
         FPU control registers, FP0-FP7 and the FSAVE frame (31 longs),
         then the PC. Another layout: 1. */
      const ULONG *sp = (const ULONG *)target->tc_SPReg;
      pc = sp[0] == 0xffffffffUL ? sp[31] : 1;
    }
    Enable();
    samples[nsamples++] = pc;
  }
  CloseDevice((struct IORequest *)tr);
  DeleteIORequest((struct IORequest *)tr);
  DeleteMsgPort(port);

  fh = Open((CONST_STRPTR)"RAM:Settlers-pcs.bin", MODE_NEWFILE);
  if (fh != 0) {
    ULONG head[3];
    head[0] = 0x50435331;  /* "PCS1" */
    head[1] = (ULONG)amiga_sampler_start;
    head[2] = nsamples;
    Write(fh, head, sizeof(head));
    Write(fh, samples, nsamples * sizeof(ULONG));
    Close(fh);
  }
  Forbid();
  done = 1;
}

int amiga_sampler_start(unsigned long delay, unsigned long count,
                        unsigned long interval) {
  struct Process *p;
  if (count == 0) return -1;
  samples = (ULONG *)AllocVec(count * sizeof(ULONG), MEMF_ANY | MEMF_CLEAR);
  if (samples == NULL) return -1;
  maxsamples = count;
  delay_s = delay;
  interval_us = interval < 200 ? 200 : interval;
  target = FindTask(NULL);
  p = CreateNewProcTags(NP_Entry, (ULONG)sampler_main,
                        NP_Name, (ULONG)"Settlers sampler",
                        NP_Priority, 20,
                        NP_StackSize, 8192,
                        TAG_DONE);
  if (p == NULL) {
    FreeVec(samples);
    samples = NULL;
    done = 1;
    return -1;
  }
  return 0;
}

void amiga_sampler_stop(void) {
  if (samples == NULL) return;
  quit = 1;
  while (!done) Delay(2);
  FreeVec(samples);
  samples = NULL;
}
