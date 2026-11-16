/* MP3 playback over mhi.library (Warp cards, MAS Player, Melody 1200 etc.).
   Plain C: the AmigaOS headers clash with Freeserf's class names, so the
   popup (music-box.cc) only calls these. The MP3 playlist is kept here
   too so that closing and reopening the popup keeps it. */
#ifndef AMIGA_MP3_H
#define AMIGA_MP3_H

#ifdef __cplusplus
extern "C" {
#endif

/* The decoder (mhi.library of the Warp card) opens at the first play.
   Returns 1 when ready (the ASL file picker still opens without it, so
   the player can build a playlist on an Amiga without a decoder). */
int amiga_mp3_open(void);
void amiga_mp3_close(void);

/* Opens the ASL file picker (multi-select, *.mp3) and adds each picked
   file to the playlist. Returns the number of files added. */
int amiga_mp3_add_files(void);
/* Opens the ASL drawer picker, scans it for *.mp3 and adds them to the
   playlist. Returns the number of files added. */
int amiga_mp3_add_drawer(void);
/* Adds the *.mp3 of the drawer MP3 in the game drawer (PROGDIR:MP3),
   sorted by name; called once at the start. Returns the number added. */
int amiga_mp3_scan_game_drawer(void);

/* The playlist: paths as full "Volume:dir/file.mp3". index 0..count-1. */
int amiga_mp3_count(void);
/* The name shown in the list (the file part of the path). */
const char *amiga_mp3_name(int index);
const char *amiga_mp3_path(int index);
/* Removes one entry from the list (not from disk). Stops playback when
   it was that entry. */
void amiga_mp3_remove(int index);
/* Empties the playlist. */
void amiga_mp3_clear(void);

/* Plays the entry at index. Stops anything playing first. */
void amiga_mp3_play(int index);
void amiga_mp3_stop(void);
/* Pauses when playing, resumes when paused. */
void amiga_mp3_pause(void);
/* 1 playing, 0 stopped or no decoder. */
int amiga_mp3_is_playing(void);
int amiga_mp3_is_paused(void);
/* Which entry is playing (or -1). */
int amiga_mp3_current(void);

/* 0..100, the volume given to MHI; stays in Settlers.ini. */
void amiga_mp3_set_volume(int volume);
int amiga_mp3_get_volume(void);

/* Pumps the MHI buffer from the current file; at the end of a track it
   plays the next one, after the last the first (a loop). Called every
   frame. */
void amiga_mp3_tick(void);

/* 1 when the port opened mhi.library (any driver), otherwise 0. The popup
   shows "No MHI library" in the title when it is 0. */
int amiga_mp3_have_decoder(void);
/* Short name of the opened driver ("mhiwarp.library", or "" when none). */
const char *amiga_mp3_driver(void);

#ifdef __cplusplus
}
#endif

#endif  /* AMIGA_MP3_H */
