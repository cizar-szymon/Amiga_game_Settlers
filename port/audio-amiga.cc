/*
 * audio-amiga.cc - sound effects and music of the Amiga port
 * (see audio-amiga.h)
 */

#include "port/audio-amiga.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "src/buffer.h"
#include "src/data.h"
#include "src/data-source.h"
#include "src/log.h"
#include "src/settings.h"
#include "port/amiga_audio.h"

extern "C" {
#include "port/thirdparty/micromod/micromod.h"
}

bool AudioAmiga::disabled = false;
bool AudioAmiga::dump_music = false;

/* ---- The mixer, run by the audio process under amiga_audio_lock() ---- */

namespace {

struct Voice {
  const int8_t *data;
  uint32_t end;    /* length << 16 */
  uint32_t pos;    /* 16.16 */
  uint32_t step;   /* 16.16 */
  int volume;      /* 0..128 */
  int side;        /* 0 left, 1 right */
};

Voice voices[Audio::kSfxChannels];
bool music_on = false;
bool music_loaded = false;
/* micromod's gain: 64 is full scale for 4 channels; half of it leaves
   room for the sound effects. micromod_initialise() sets 64 again. */
long music_gain = 32;

/* Paula's channels 0 and 3 are on the left, 1 and 2 on the right; the
   original plays the effects there. */
const int kChannelSide[Audio::kSfxChannels] = { 0, 1, 1, 0 };

/* Options -> Volume: the whole mix times master_gain / 256. */
int master_gain = 256;

void
mix(short *out, int frames, void *) {
  if (music_on) {
    micromod_get_audio(out, frames);
  }
  for (Voice &v : voices) {
    if (v.data == nullptr) continue;
    short *o = out + v.side;
    int vol = v.volume;
    uint32_t pos = v.pos, step = v.step, end = v.end;
    for (int i = 0; i < frames; i++) {
      if (pos >= end) {
        v.data = nullptr;
        break;
      }
      int s = o[2 * i] + v.data[pos >> 16] * vol;
      o[2 * i] = static_cast<short>(s < -32768 ? -32768
                                    : s > 32767 ? 32767 : s);
      pos += step;
    }
    v.pos = pos;
  }
  if (master_gain < 256) {
    for (int i = 0; i < 2 * frames; i++) {
      out[i] = static_cast<short>((out[i] * master_gain) >> 8);
    }
  }
}

}  // namespace

/* ---- Sound effects ---- */

/* A volume in steps of 10% (0.1 added ten times is not 1.0 in a float). */
static float
volume_step(float v) {
  int tenths = static_cast<int>(v * 10.f + 0.5f);
  return static_cast<float>(std::max(0, std::min(tenths, 10))) / 10.f;
}

void
AudioAmiga::Volume::set_volume(float v) {
  volume = volume_step(v);
  apply();
}

void
AudioAmiga::Volume::apply() {
  int gain = static_cast<int>(volume * 256.f + 0.5f);
  /* One word written: no lock needed (and with NOSOUND there is none). */
  master_gain = gain;
}

class TrackSFX : public Audio::Track {
 public:
  std::vector<int8_t> data;
  uint32_t rate;
  const float *master;

  TrackSFX() : rate(0), master(nullptr) {}

  virtual void play() { play_on_channel(0, 1.f, 1.f); }
  virtual void play_on_channel(int channel, float volume, float ratio) {
    if (channel < 0 || channel >= Audio::kSfxChannels || data.empty()) {
      return;
    }
    int out_rate = amiga_audio_rate();
    if (out_rate <= 0) return;
    float m = master != nullptr ? *master : 1.f;
    amiga_audio_lock();
    Voice &v = voices[channel];
    v.data = &data[0];
    v.end = static_cast<uint32_t>(data.size()) << 16;
    v.pos = 0;
    v.step = static_cast<uint32_t>(static_cast<float>(rate) * ratio /
                                   static_cast<float>(out_rate) * 65536.f);
    v.volume = static_cast<int>(volume * m * 128.f);
    v.side = kChannelSide[channel];
    amiga_audio_unlock();
  }
};

class AudioAmiga::PlayerSFX : public Audio::Player,
                              public Audio::VolumeController {
 protected:
  float volume;

 public:
  PlayerSFX() : volume(1.f) {}

  virtual void enable(bool enable) {
    enabled = enable;
    if (!enable) stop();
  }
  virtual Audio::PVolumeController get_volume_controller() {
    return Audio::PVolumeController(this, [](Audio::VolumeController *) {});
  }
  virtual float get_volume() { return volume; }
  virtual void set_volume(float v) { volume = volume_step(v); }
  virtual void volume_up() { set_volume(volume + 0.1f); }
  virtual void volume_down() { set_volume(volume - 0.1f); }

 protected:
  /* The game data gives a WAV: 16-bit little-endian mono samples (the
     8-bit Paula samples times 256) at the sample's own rate. */
  virtual Audio::PTrack create_track(int track_id) {
    Data::PSource source = Data::get_instance().get_data_source();
    if (!source) return nullptr;
    PBuffer wav = source->get_sound(track_id);
    if (!wav || wav->get_size() < 44) return nullptr;
    const uint8_t *p = static_cast<const uint8_t *>(wav->get_data());
    size_t size = wav->get_size();
    std::shared_ptr<TrackSFX> track = std::make_shared<TrackSFX>();
    track->rate = p[24] | (p[25] << 8) | (p[26] << 16) |
                  (static_cast<uint32_t>(p[27]) << 24);
    track->master = &volume;
    /* Find the data chunk. */
    size_t pos = 12;
    while (pos + 8 <= size) {
      uint32_t len = p[pos + 4] | (p[pos + 5] << 8) | (p[pos + 6] << 16) |
                     (static_cast<uint32_t>(p[pos + 7]) << 24);
      if (std::memcmp(p + pos, "data", 4) == 0) {
        size_t n = std::min<size_t>(len, size - pos - 8) / 2;
        track->data.resize(n);
        for (size_t i = 0; i < n; i++) {
          track->data[i] = static_cast<int8_t>(p[pos + 8 + 2 * i + 1]);
        }
        break;
      }
      pos += 8 + len + (len & 1);
    }
    if (track->data.empty() || track->rate == 0) return nullptr;
    return track;
  }
  virtual void stop() {
    amiga_audio_lock();
    for (Voice &v : voices) v.data = nullptr;
    amiga_audio_unlock();
  }
};

/* ---- Music ---- */

class TrackMusic : public Audio::Track {
 public:
  std::vector<int8_t> module;
  bool *player_enabled;

  TrackMusic() : player_enabled(nullptr) {}

  virtual void play() {
    if (module.empty()) return;
    amiga_audio_lock();
    music_loaded = micromod_initialise(
      reinterpret_cast<signed char *>(&module[0]), amiga_audio_rate()) >= 0;
    micromod_set_gain(music_gain);
    music_on = music_loaded &&
               (player_enabled == nullptr || *player_enabled);
    amiga_audio_unlock();
  }
};

class AudioAmiga::PlayerMusic : public Audio::Player,
                                public Audio::VolumeController {
 protected:
  float volume;

 public:
  PlayerMusic() : volume(1.f) {}

  virtual void enable(bool enable) {
    enabled = enable;
    amiga_audio_lock();
    music_on = enable && music_loaded;
    amiga_audio_unlock();
  }
  /* The module is loaded also with the music off (TrackMusic::play then
     does not start it), so that switching it on later (M, Options) plays
     it. */
  virtual Audio::PTrack play_track(int track_id) {
    Audio::PTrack track = get_track(track_id);
    if (track != nullptr) track->play();
    return is_enabled() ? track : nullptr;
  }
  virtual Audio::PVolumeController get_volume_controller() {
    return Audio::PVolumeController(this, [](Audio::VolumeController *) {});
  }
  virtual float get_volume() { return volume; }
  virtual void set_volume(float v) {
    volume = volume_step(v);
    amiga_audio_lock();
    music_gain = static_cast<long>(32.f * volume);
    micromod_set_gain(music_gain);
    amiga_audio_unlock();
  }
  virtual void volume_up() { set_volume(volume + 0.1f); }
  virtual void volume_down() { set_volume(volume - 0.1f); }

 protected:
  virtual Audio::PTrack create_track(int track_id) {
    Data::PSource source = Data::get_instance().get_data_source();
    if (!source || source->get_music_format() != Data::MusicFormatMod) {
      return nullptr;
    }
    PBuffer data = source->get_music(track_id);
    if (!data) return nullptr;
    std::shared_ptr<TrackMusic> track = std::make_shared<TrackMusic>();
    const int8_t *p = static_cast<const int8_t *>(data->get_data());
    track->module.assign(p, p + data->get_size());
    track->player_enabled = &enabled;
    if (dump_music) {
      /* STATS=FULL: the module as the game data gives it, for checking. */
      char name[64];
      snprintf(name, sizeof(name), "RAM:Settlers-music%d.mod", track_id);
      FILE *f = fopen(name, "wb");
      if (f != nullptr) {
        fwrite(p, 1, data->get_size(), f);
        fclose(f);
      }
    }
    return track;
  }
  virtual void stop() {
    amiga_audio_lock();
    music_on = false;
    amiga_audio_unlock();
  }
};

/* ---- Audio ---- */

Audio &
Audio::get_instance() {
  static AudioAmiga instance;
  return instance;
}

AudioAmiga::AudioAmiga()
  : open(false)
  , master(1.f) {
  if (disabled) return;
  /* The output of the settings (Advanced -> Audio): Paula at first; AHI
     that does not open falls back to Paula. 512 samples a buffer: 23 ms
     each at 22 kHz (four buffers for Paula, four per side for AHI). */
  /* The settings keep it in small letters ("ahi"). */
  std::string output = Settings::get_instance().get("audio", "output",
                                                    std::string("Paula"));
  bool ahi = strcasecmp(output.c_str(), "AHI") == 0;
  amiga_audio_use_ahi(ahi ? 1 : 0);
  int r = amiga_audio_open(512, mix, nullptr);
  if (r != 0 && ahi) {
    Log::Warn["audio"] << "No AHI audio (ahi.device), Paula instead";
    amiga_audio_use_ahi(0);
    r = amiga_audio_open(512, mix, nullptr);
  }
  if (r != 0) {
    Log::Warn["audio"] << "No Paula audio (audio.device)";
    return;
  }
  open = true;
  sfx_player = std::make_shared<PlayerSFX>();
  music_player = std::make_shared<PlayerMusic>();
}

AudioAmiga::~AudioAmiga() {
  if (open) {
    amiga_audio_close();
    open = false;
  }
}

std::vector<std::string>
AudioAmiga::get_outputs() const {
  return disabled ? std::vector<std::string>()
                  : std::vector<std::string>{ "Paula", "AHI" };
}

std::string
AudioAmiga::get_output() const {
  if (!open) return "-";
  return amiga_audio_is_ahi() ? "AHI" : "Paula";
}

/* Another output at once: the audio process stops and starts again on it
   (the mixer state - music, sounds playing - stays); one that does not
   open gives the one before back. */
bool
AudioAmiga::set_output(const std::string &output) {
  if (disabled) return false;
  bool ahi = strcasecmp(output.c_str(), "AHI") == 0;
  bool was_ahi = amiga_audio_is_ahi() != 0;
  if (open) amiga_audio_close();
  open = false;
  amiga_audio_use_ahi(ahi ? 1 : 0);
  bool ok = amiga_audio_open(512, mix, nullptr) == 0;
  if (!ok) {
    Log::Warn["audio"] << "No " << output << " audio, back to the one before";
    amiga_audio_use_ahi(was_ahi ? 1 : 0);
    if (amiga_audio_open(512, mix, nullptr) != 0) return false;
  }
  open = true;
  if (!sfx_player) sfx_player = std::make_shared<PlayerSFX>();
  if (!music_player) music_player = std::make_shared<PlayerMusic>();
  return ok;
}

void
AudioAmiga::data_changed(bool sounds, bool music) {
  if (sounds && sfx_player) sfx_player->clear_cache();
  if (music && music_player) {
    music_player->enable(false);
    music_player->clear_cache();
  }
}
