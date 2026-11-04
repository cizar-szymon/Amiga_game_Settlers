/*
 * intro-amiga.cc - the original intro on the Amiga screen and Paula
 * (see intro-amiga.h; the player itself is intro-player.cc)
 *
 * Every 50 Hz frame (E-clock): the player runs the frame's operations and
 * mixes its 160 effect samples; when the picture changed, it is turned
 * into 320x256 pixels, and only the lines that differ from the last frame
 * are doubled to 640 pixels and written to the card. The music module
 * (MUSI) plays through micromod and the effects (8000 Hz, as recorded)
 * through a small queue in the audio process of amiga_audio.c: music
 * channels 0 left, 1 and 2 right, effects left, as Paula played them.
 */

#include "port/intro-amiga.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "src/log.h"
#include "port/amiga_audio.h"
#include "port/amiga_screen.h"
#include "port/intro-player.h"

extern "C" {
#include "port/thirdparty/micromod/micromod.h"
}

namespace {

const int kW = IntroPlayer::kWidth;
const int kH = IntroPlayer::kHeight;

/* ---- Sound: shared with the audio process under amiga_audio_lock() ---- */

const unsigned kQueue = 4096;    /* effect samples, a power of 2 */
const unsigned kQueueMax = 3 * IntroPlayer::kSfxPerFrame;

struct Sound {
  bool music;
  int8_t queue[kQueue];
  unsigned rd, wr, count;
  uint32_t frac, step;           /* 16.16, effect samples per output one */
  unsigned long gaps;            /* output samples without an effect sample */
} sound;

void
fill(short *out, int frames, void *) {
  if (sound.music) micromod_get_audio(out, frames);
  uint32_t frac = sound.frac, step = sound.step;
  unsigned rd = sound.rd, count = sound.count;
  for (int i = 0; i < frames; i++) {
    if (count != 0) {
      int s = out[2 * i] + sound.queue[rd] * 128;
      out[2 * i] = static_cast<short>(s < -32768 ? -32768
                                      : s > 32767 ? 32767 : s);
    } else {
      sound.gaps++;
    }
    frac += step;
    while (frac >= 0x10000) {
      frac -= 0x10000;
      if (count != 0) {
        rd = (rd + 1) & (kQueue - 1);
        count--;
      }
    }
  }
  sound.frac = frac;
  sound.rd = rd;
  sound.count = count;
}

/* One frame of effect samples into the queue; it is kept short, so that
   the effects stay with the picture after a slow frame. */
void
queue_sfx(const int8_t *sfx) {
  amiga_audio_lock();
  for (int i = 0; i < IntroPlayer::kSfxPerFrame; i++) {
    sound.queue[sound.wr] = sfx[i];
    sound.wr = (sound.wr + 1) & (kQueue - 1);
    sound.count++;
  }
  while (sound.count > kQueueMax) {
    sound.rd = (sound.rd + 1) & (kQueue - 1);
    sound.count--;
  }
  amiga_audio_unlock();
}

/* ---- Picture ---- */

struct Screen {
  int crop;              /* source lines cut at the top and the bottom */
  uint8_t *out;          /* the screen's pixels, 640 per row, 16-aligned */
  void *out_mem;
  uint8_t *cur;          /* 320x256 colour indices */
  uint8_t changed[kH];   /* the lines render() converted */
  uint8_t rgb[256 * 3], prev_rgb[256 * 3];
  bool full;             /* the next present sends every line */
};

/* Source lines first..last (inclusive) to the card. */
void
send(const Screen &s, int first, int last) {
  int r0 = (first - s.crop) * 2;
  int n = (last - first + 1) * 2;
  amiga_screen_write(s.out + r0 * 640, 640, 0, r0, 640, n);
}

/* The changed colours and source lines (doubled) to the card; returns
   the number of lines. scroll = d: the picture moved up by d lines, which
   the card's blitter does on the screen; only the lines that came in and
   the changed ones are written. */
unsigned long
present(Screen &s, const IntroPlayer &player, int scroll) {
  int lo = 0, hi = 255;
  if (!s.full) {
    while (lo < 256 && std::memcmp(s.rgb + 3 * lo, s.prev_rgb + 3 * lo, 3) == 0) lo++;
    while (hi > lo && std::memcmp(s.rgb + 3 * hi, s.prev_rgb + 3 * hi, 3) == 0) hi--;
  }
  if (lo <= hi && lo < 256) {
    amiga_set_colors(lo, hi - lo + 1, s.rgb + 3 * lo);
    std::memcpy(s.prev_rgb + 3 * lo, s.rgb + 3 * lo, 3 * (hi - lo + 1));
  }
  const int top = s.crop, bottom = kH - s.crop;    /* visible lines */
  int moved = 0;
  if (!s.full && scroll != 0) {
    moved = bottom - top - (scroll > 0 ? scroll : -scroll);
    if (moved <= 0) {
      s.full = true;
    } else if (scroll > 0) {
      amiga_screen_move(2 * scroll, 0, 2 * moved);
    } else {
      amiga_screen_move(0, -2 * scroll, 2 * moved);
    }
  }
  int first = -1, last = -1;
  unsigned long changed = 0;
  for (int y = top; y < bottom; y++) {
    bool send_it = s.full || s.changed[y] ||
                   (scroll != 0 && (y + scroll < top || y + scroll >= bottom));
    if (!send_it) {
      if (first >= 0) {
        send(s, first, last);
        first = -1;
      }
      continue;
    }
    const uint8_t *src = player.row(s.cur, y);
    uint32_t *d = reinterpret_cast<uint32_t *>(s.out + (y - s.crop) * 2 * 640);
    uint32_t *d2 = d + 640 / 4;
    for (int x = 0; x < kW; x += 2) {
      uint32_t a = src[x], b = src[x + 1];
      uint32_t v = (a << 24) | (a << 16) | (b << 8) | b;
      *d++ = v;
      *d2++ = v;
    }
    if (first < 0) first = y;
    last = y;
    changed++;
  }
  if (first >= 0) send(s, first, last);
  s.full = false;
  return changed;
}

}  // namespace

int
intro_play(const char *dir, bool with_sound, bool stats) {
  IntroPlayer *player = new IntroPlayer;
  if (!player->open(dir)) {
    Log::Info["intro"] << "No intro: " << player->error();
    delete player;
    return 0;
  }

  Screen s;
  std::memset(&s, 0, sizeof(s));
  int sh = 512;
  if (!amiga_screen_mode_exists(640, 512)) {
    sh = 480;
    s.crop = 8;
  }
  char err[128];
  if (amiga_screen_open(640, sh, "Settlers intro", err, sizeof(err)) != 0) {
    Log::Warn["intro"] << "No intro screen: " << err;
    amiga_screen_close();
    delete player;
    return 0;
  }
  amiga_hide_pointer();
  amiga_set_colors(0, 256, s.rgb);
  s.out_mem = std::malloc(640 * sh + 16);
  s.cur = static_cast<uint8_t *>(std::malloc(kW * kH));
  if (s.out_mem == nullptr || s.cur == nullptr) {
    std::free(s.out_mem);
    std::free(s.cur);
    amiga_screen_close();
    delete player;
    return 0;
  }
  s.out = reinterpret_cast<uint8_t *>(
    (reinterpret_cast<uintptr_t>(s.out_mem) + 15) & ~static_cast<uintptr_t>(15));
  s.full = true;

  std::memset(&sound, 0, sizeof(sound));
  bool audio = with_sound && amiga_audio_open(256, fill, nullptr) == 0;
  if (audio) {
    amiga_audio_lock();
    sound.step = (8000UL << 16) / static_cast<unsigned long>(amiga_audio_rate());
    amiga_audio_unlock();
  }

  const int frames = player->frame_count();
  const int total = frames + IntroPlayer::kMusicFadeFrames;
  const unsigned long per_ms = amiga_eclock_per_ms();
  const unsigned long per_frame = per_ms * 20;
  int8_t sfx[IntroPlayer::kSfxPerFrame];
  int fade = -1;                 /* frames of the music fade left */
  int result = 0;
  bool done = false;
  int since_shown = 0;
  /* Statistics per scene (0: after the last one): E-clock ticks of the
     operations, the c2p and the sending, the worst frame, the pictures
     shown and dropped, the frames that ended after their time. */
  struct Stats {
    int frames, shown, dropped, late;
    unsigned long step, render, present, worst, lines;
    unsigned long step_max, render_max, present_max, over_max;
  } st[8];
  std::memset(st, 0, sizeof(st));
  int played = 0;
  /* Frames that took more than 15 ms: number, operations, picture (us). */
  struct Spike { int frame; unsigned long step, picture; } spikes[96];
  int nspikes = 0;

  /* Above the Workbench and the other programs of priority 0 and 1, which
     otherwise took the CPU for 20-40 ms at a time; below the audio process
     (10) and input.device (20). */
  int old_pri = amiga_task_priority(5);
  const unsigned long t0 = amiga_eclock();
  for (int f = 0; f < total && !done; f++) {
    unsigned long tb = amiga_eclock();
    if (f < frames) {
      player->step(sfx);
      if (audio) {
        queue_sfx(sfx);
        IntroPlayer::Music m = player->music_event();
        if (m == IntroPlayer::MusicPlay) {
          size_t size;
          int8_t *mod = player->music_module(&size);
          amiga_audio_lock();
          sound.music = mod != nullptr &&
            micromod_initialise(reinterpret_cast<signed char *>(mod),
                                amiga_audio_rate()) >= 0;
          /* Full gain: three channels on two outputs, never two at full
             volume on one side with the effects. */
          micromod_set_gain(64);
          amiga_audio_unlock();
        } else if (m == IntroPlayer::MusicFade) {
          fade = IntroPlayer::kMusicFadeFrames;
        } else if (m == IntroPlayer::MusicStop) {
          amiga_audio_lock();
          sound.music = false;
          amiga_audio_unlock();
        }
      }
    } else if (f == frames) {
      /* After the last frame: the screen black, the music fades out. */
      if (fade < 0) fade = IntroPlayer::kMusicFadeFrames;
      std::memset(s.rgb, 0, sizeof(s.rgb));
      amiga_set_colors(0, 256, s.rgb);
      std::memcpy(s.prev_rgb, s.rgb, sizeof(s.rgb));
    }
    if (audio && fade >= 0) {
      amiga_audio_lock();
      micromod_set_gain(64L * fade / IntroPlayer::kMusicFadeFrames);
      if (fade == 0) sound.music = false;
      amiga_audio_unlock();
      fade--;
    }
    Stats &sc = st[f < frames ? (player->scene() & 7) : 0];
    unsigned long ts = amiga_eclock();
    sc.step += ts - tb;
    if (ts - tb > sc.step_max) sc.step_max = ts - tb;

    /* The picture: dropped when this frame's time is already over (at most
       4 in a row), never for the game logic or the sound. */
    if (f < frames && player->picture_changed()) {
      bool over = ts - t0 > static_cast<unsigned long>(f + 1) * per_frame;
      if (!over || since_shown >= 4) {
        int scroll = 0;
        player->render(s.cur, s.rgb, s.changed, &scroll);
        unsigned long tr = amiga_eclock();
        sc.lines += present(s, *player, scroll);
        unsigned long te = amiga_eclock();
        sc.render += tr - ts;
        sc.present += te - tr;
        if (tr - ts > sc.render_max) sc.render_max = tr - ts;
        if (te - tr > sc.present_max) sc.present_max = te - tr;
        sc.shown++;
        since_shown = 0;
      } else {
        sc.dropped++;
        since_shown++;
      }
    }
    unsigned long used = amiga_eclock() - tb;
    if (used > sc.worst) sc.worst = used;
    if (used > 15 * per_ms && nspikes < 96) {
      spikes[nspikes].frame = f;
      spikes[nspikes].step = (ts - tb) * 1000 / per_ms;
      spikes[nspikes].picture = (used - (ts - tb)) * 1000 / per_ms;
      nspikes++;
    }
    sc.frames++;
    played++;

    /* Until the next frame: any key or button ends the intro. */
    for (;;) {
      AmigaEvent ev;
      while (amiga_get_event(&ev)) {
        if ((ev.type == AEV_KEY || ev.type == AEV_BUTTON) && ev.down) {
          done = true;
        } else if (ev.type == AEV_CLOSE) {
          done = true;
        }
      }
      if (done) break;
      unsigned long now = amiga_eclock() - t0;
      unsigned long due = static_cast<unsigned long>(f + 1) * per_frame;
      if (static_cast<long>(due - now) <= 0) {
        if (now - due > per_frame / 2) sc.late++;
        if (now - due > sc.over_max) sc.over_max = now - due;
        break;
      }
      unsigned long ms = (due - now) / per_ms;
      if (ms == 0) ms = 1;
      if (amiga_wait(ms)) {
        done = true;
        result = 1;
        break;
      }
    }
  }
  unsigned long elapsed = amiga_eclock() - t0;
  amiga_task_priority(old_pri);

  unsigned long gaps = sound.gaps;
  if (audio) amiga_audio_close();
  amiga_screen_close();

  if (stats) {
    FILE *f = std::fopen("RAM:Settlers-intro.txt", "w");
    if (f != nullptr) {
      std::fprintf(f, "intro %s, screen 640x%d\n", dir, sh);
      std::fprintf(f, "frames %d of %d, %s\n", played, total,
                   done ? (result ? "Ctrl-C" : "skipped") : "to the end");
      std::fprintf(f, "time %lu ms (%lu ms at 50 Hz)\n", elapsed / per_ms,
                   static_cast<unsigned long>(played) * 20);
      std::fprintf(f, "sound %s, output samples without effects %lu\n",
                   audio ? "on" : "off", gaps);
      std::fprintf(f, "scene frames shown dropped late | average us: "
                   "operations c2p send | worst us: frame operations c2p send, after "
                   "its time | lines sent/frame | CPU%%\n");
      unsigned long busy = 0;
      for (int i = 1; i <= 8; i++) {
        const Stats &c = st[i & 7];
        if (c.frames == 0) continue;
        unsigned long all = c.step + c.render + c.present;
        busy += all;
        unsigned long n = static_cast<unsigned long>(c.frames);
        std::fprintf(f, "%5d %6d %5d %7d %4d | %6lu %4lu %4lu | %6lu %6lu %6lu %6lu %6lu "
                     "| %4lu | %lu\n",
                     i & 7, c.frames, c.shown, c.dropped, c.late,
                     c.step / n * 1000 / per_ms,
                     c.render / n * 1000 / per_ms,
                     c.present / n * 1000 / per_ms,
                     c.worst * 1000 / per_ms, c.step_max * 1000 / per_ms,
                     c.render_max * 1000 / per_ms,
                     c.present_max * 1000 / per_ms,
                     c.over_max * 1000 / per_ms, c.lines / n,
                     all / (n * per_frame / 100));
      }
      unsigned long permille = busy / (elapsed / 1000 ? elapsed / 1000 : 1);
      std::fprintf(f, "CPU %lu.%lu%% of the frames' time\n", permille / 10,
                   permille % 10);
      std::fprintf(f, "frames over 15 ms (frame: operations, picture us):");
      for (int i = 0; i < nspikes; i++) {
        std::fprintf(f, "%s%d: %lu %lu", i % 6 ? ", " : "\n", spikes[i].frame,
                     spikes[i].step, spikes[i].picture);
      }
      std::fprintf(f, "\n");
      std::fclose(f);
    }
  }
  std::free(s.out_mem);
  std::free(s.cur);
  delete player;
  return result;
}
