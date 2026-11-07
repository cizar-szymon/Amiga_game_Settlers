/* amiga_sampler.h - sampling profiler (PCSAMPLE=s,n), see amiga_sampler.c */
#ifndef PORT_AMIGA_SAMPLER_H_
#define PORT_AMIGA_SAMPLER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* After delay seconds, count samples interval_us apart of the calling
   task; written to RAM:Settlers-pcs.bin. 0 when started. */
int amiga_sampler_start(unsigned long delay, unsigned long count,
                        unsigned long interval_us);
/* Stops it (written if it had finished) before the program ends. */
void amiga_sampler_stop(void);

#ifdef __cplusplus
}
#endif

#endif  // PORT_AMIGA_SAMPLER_H_
