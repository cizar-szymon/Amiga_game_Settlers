/*
 * intro-amiga.h - the original intro on the Amiga screen and Paula
 */
#ifndef PORT_INTRO_AMIGA_H_
#define PORT_INTRO_AMIGA_H_

/* Plays the intro from dir (intro.script and the files of the original
   disks 1 and 2) on a screen of its own, 640x512 (or 640x480 with 8 lines
   cut at the top and the bottom), with the music and the effects when
   sound is true. Any key or mouse button ends it. Without the files it
   does nothing. stats: the frame times to RAM:Settlers-intro.txt.
   Returns 1 when Ctrl-C was pressed (quit the program), else 0. */
int intro_play(const char *dir, bool sound, bool stats);

#endif  // PORT_INTRO_AMIGA_H_
