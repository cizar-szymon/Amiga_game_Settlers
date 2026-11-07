/* Audio output for the Settlers port: Paula or AHI (amiga_audio.c). */
#ifndef AMIGA_AUDIO_H
#define AMIGA_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Adds frames stereo samples (left, right, 16 bit) to out, which is
   zeroed. Called by the audio process with the lock held. */
typedef void (*AmigaAudioFill)(short *out, int frames, void *user);

/* Starts the audio process; 0 on success. */
int amiga_audio_open(int samples_per_buffer, AmigaAudioFill fill,
                     void *user);
void amiga_audio_close(void);
/* AHI (ahi.device) instead of Paula for the next amiga_audio_open. */
void amiga_audio_use_ahi(int on);
/* The output running is AHI. */
int amiga_audio_is_ahi(void);
/* Output rate in Hz (22030 on PAL). */
int amiga_audio_rate(void);
/* The game's side of the shared mixer state. */
void amiga_audio_lock(void);
void amiga_audio_unlock(void);
/* Diagnostics: gaps, buffers rendered, E-clock ticks rendering, period. */
/* STATS: the peak of the mix and its samples at full scale since the last
   call. */
void amiga_audio_levels(int *peak, unsigned long *full);
void amiga_audio_stats(unsigned long *underruns, unsigned long *buffers,
                       unsigned long *ticks, int *period, int *pal);

#ifdef __cplusplus
}
#endif

#endif /* AMIGA_AUDIO_H */
