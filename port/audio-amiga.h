/*
 * audio-amiga.h - sound effects and music of the Amiga port
 *
 * Everything is mixed in software into one left and one right Paula
 * channel (amiga_audio.c): the music module (micromod) and the four sound
 * effect channels of the original, which the game schedules itself
 * (Audio::update_sfx) with a volume and a frequency ratio per channel.
 */
#ifndef PORT_AUDIO_AMIGA_H_
#define PORT_AUDIO_AMIGA_H_

#include <memory>
#include <vector>

#include "src/audio.h"

class AudioAmiga : public Audio {
 public:
  /* NOSOUND: no audio at all. */
  static bool disabled;
  /* STATS=FULL: write the music modules to RAM: for checking. */
  static bool dump_music;

  class Volume : public Audio::VolumeController {
   protected:
    float volume;
   public:
    explicit Volume(float v) : volume(v) {}
    virtual float get_volume() { return volume; }
    virtual void set_volume(float v);
    virtual void volume_up() { set_volume(volume + 0.1f); }
    virtual void volume_down() { set_volume(volume - 0.1f); }
    virtual void apply() {}
  };

  class PlayerSFX;
  class PlayerMusic;

 protected:
  bool open;
  Audio::PPlayer sfx_player;
  Audio::PPlayer music_player;
  Volume master;

 public:
  AudioAmiga();
  virtual ~AudioAmiga();

  virtual Audio::VolumeController *get_volume_controller() { return &master; }
  virtual Audio::PPlayer get_sound_player() { return sfx_player; }
  virtual Audio::PPlayer get_music_player() { return music_player; }
  virtual void data_changed(bool sounds, bool music);
};

#endif  // PORT_AUDIO_AMIGA_H_
