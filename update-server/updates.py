"""Updates of the games: the newest release on GitHub, file by file.

A server of its own (update_server.py), on the data server - not the game
server: downloading releases and sending files of megabytes must not slow
the games.

Every hour (and at the start) the server looks for the newest
releases/amiga/<prefix><version>.zip of each game in config "updates"
(GitHub API), downloads it once and keeps its files under
data/updates/<game>/<version>/ with a list of them (manifest.json: the path in
the game drawer, the size, the FNV-1a sum - the same as lan_data_checksum of
the game). An Amiga asks:

  client: UPDATE <proto> <version> [<lang>]
  server: LATEST <version>                       the newest release here
          NOTE <line>        ...                 what changed since <version>
                                                 (from its history of
                                                 releases, pl or en)
          FILE <path> <size> <sum>   ...         every file of the release
          END
  client: GET <path>        (any number of times, one after another)
  server: SIZE <size> and the file in F_RELAY_DATA frames
  client: BYE (or closes the connection)

or ERR <text>. The Amiga compares the list with its own files and asks only
for those that differ. Only the files of the public archive are there (no
data of the game, no ROMs, no settings of the player).
"""
import asyncio
import io
import json
import logging
import os
import re
import shutil
import time
import urllib.request
import zipfile

import frames as P

log = logging.getLogger('updates')

DEFAULT = {
    # version_prefix: the start of the version the game sends ("0.139";
    # OpenDUNE would send "OpenDUNE-0.52"); '' takes the rest.
    'settlers': {'version_prefix': '',
                 'repo': 'cizar-szymon/Amiga_game_Settlers',
                 'branch': 'amiga-port', 'dir': 'releases/amiga',
                 'prefix': 'Settlers-', 'root': 'Settlers/',
                 'notes_pl': 'doc/Settlers_release.txt',
                 'notes_en': 'doc/Settlers_release_en.txt'},
}
PATH_OK = re.compile(r'^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$')
MAX_NOTE_LINES = 200       # all the releases since the version asked about


def fnv1a(data):
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xffffffff
    return h


def version_key(v):
    """'0.139' -> (0, 139); not a version: None."""
    try:
        return tuple(int(x) for x in v.split('.'))
    except ValueError:
        return None


class Release:
    def __init__(self, version, files, notes):
        self.version = version
        self.files = files        # path -> bytes
        self.notes = notes        # lang -> text of the history of releases
        self.manifest = [(p, len(d), fnv1a(d)) for p, d in sorted(files.items())]


def read_zip(data, root):
    """The files of the archive under root (the game drawer), path -> bytes."""
    files = {}
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for info in z.infolist():
            name = info.filename
            if info.is_dir() or not name.startswith(root):
                continue
            path = name[len(root):]
            if not PATH_OK.match(path) or '..' in path:
                continue
            files[path] = z.read(info)
    return files


def notes_since(text, newest, since):
    """The lines of the history of releases from the heading of newest down
    to (not with) the heading of since or an older one."""
    since_key = version_key(since) or (0,)
    out = []
    on = False
    for line in text.splitlines():
        m = re.match(r'^(\d+(?:\.\d+)+) \(', line)
        if m:
            key = version_key(m.group(1))
            on = key is not None and key > since_key
        if on:
            out.append(line.rstrip())
        if len(out) >= MAX_NOTE_LINES:
            break
    while out and not out[-1]:
        out.pop()
    return out


class Updates:
    def __init__(self, config):
        self.cfg = config
        self.games = config.get('updates', DEFAULT)
        self.dir = os.path.join(config['data_dir'], 'updates')
        self.check_s = config.get('check_s', 3600)
        self.releases = {}        # game -> Release
        for game in self.games:
            self.load_kept(game)

    # --- the releases on disk -------------------------------------------
    def load_kept(self, game):
        gdir = os.path.join(self.dir, game)
        best = None
        try:
            for v in os.listdir(gdir):
                if version_key(v) and (best is None or
                                       version_key(v) > version_key(best)):
                    best = v
        except FileNotFoundError:
            return
        if best is None:
            return
        vdir = os.path.join(gdir, best)
        try:
            with open(os.path.join(vdir, 'manifest.json'), encoding='utf-8') as f:
                listing = json.load(f)
            files = {}
            for path in listing['files']:
                with open(os.path.join(vdir, 'files', *path.split('/')), 'rb') as f:
                    files[path] = f.read()
            self.releases[game] = Release(best, files, listing.get('notes', {}))
            log.info('%s %s from disk: %d files', game, best, len(files))
        except (OSError, ValueError, KeyError) as e:
            log.warning('%s %s on disk unreadable: %s', game, best, e)

    def keep(self, game, release):
        vdir = os.path.join(self.dir, game, release.version)
        tmp = vdir + '.tmp'
        shutil.rmtree(tmp, ignore_errors=True)
        for path, data in release.files.items():
            out = os.path.join(tmp, 'files', *path.split('/'))
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, 'wb') as f:
                f.write(data)
        with open(os.path.join(tmp, 'manifest.json'), 'w', encoding='utf-8') as f:
            json.dump({'version': release.version,
                       'files': sorted(release.files),
                       'notes': release.notes}, f, indent=1)
        shutil.rmtree(vdir, ignore_errors=True)
        os.replace(tmp, vdir)

    # --- GitHub -------------------------------------------------------------
    @staticmethod
    def fetch(url, limit=64 << 20):
        req = urllib.request.Request(url, headers={'User-Agent': 'game-server'})
        with urllib.request.urlopen(req, timeout=60) as r:
            data = r.read(limit + 1)
        if len(data) > limit:
            raise ValueError('too large')
        return data

    def newest_on_github(self, game):
        """(version, download url) of the newest archive, or None."""
        g = self.games[game]
        url = 'https://api.github.com/repos/%s/contents/%s?ref=%s' % (
            g['repo'], g['dir'], g['branch'])
        best = None
        for item in json.loads(self.fetch(url, 4 << 20)):
            m = re.match(r'^%s(\d+(?:\.\d+)+)\.zip$' % re.escape(g['prefix']),
                         item.get('name', ''))
            if m and item.get('download_url') and (
                    best is None or version_key(m.group(1)) > version_key(best[0])):
                best = (m.group(1), item['download_url'])
        return best

    def make_release(self, game, version, data):
        g = self.games[game]
        files = read_zip(data, g['root'])
        if not files:
            raise ValueError('no files under %s' % g['root'])
        notes = {}
        for lang in ('pl', 'en'):
            path = g.get('notes_' + lang)
            if path in files:
                notes[lang] = files[path].decode('latin-1')
        return Release(version, files, notes)

    def refresh(self, game):
        """Blocking (a thread): the newest release of game, if new."""
        found = self.newest_on_github(game)
        if found is None:
            return None
        version, url = found
        have = self.releases.get(game)
        if have is not None and version_key(have.version) >= version_key(version):
            return None
        release = self.make_release(game, version, self.fetch(url))
        self.keep(game, release)
        return release

    async def run(self):
        loop = asyncio.get_running_loop()
        while True:
            for game in self.games:
                try:
                    release = await loop.run_in_executor(None, self.refresh, game)
                    if release is not None:
                        self.releases[game] = release
                        log.info('%s %s: %d files', game, release.version,
                                 len(release.files))
                except Exception as e:      # the network, GitHub, the archive
                    log.warning('update check of %s: %s', game, e)
            await asyncio.sleep(self.check_s)

    def game_of(self, version):
        """The game of a version: the longest prefix that fits."""
        best = None
        for game, g in self.games.items():
            prefix = g.get('version_prefix', '')
            # after the prefix only the number ("OpenDUNE-0.52" is no
            # version of a game whose versions are "0.139")
            if version.startswith(prefix) and \
                    version_key(version[len(prefix):]) is not None and (
                    best is None or len(prefix) > len(best[1])):
                best = (game, prefix)
        return best[0] if best else None

    def status(self):
        return {g: (r.version if r else None)
                for g, r in ((g, self.releases.get(g)) for g in self.games)}

    # --- an Amiga asks ----------------------------------------------------
    async def serve(self, reader, writer, version, rest, say):
        game = self.game_of(version)
        release = self.releases.get(game)
        if release is None:
            await say(writer, P.frame(P.F_RELAY, 'ERR No release known.'))
            return
        lang = 'pl' if rest.strip().lower().startswith('pl') else 'en'
        out = [P.frame(P.F_RELAY, 'LATEST ' + release.version)]
        text = release.notes.get(lang) or release.notes.get('en', '')
        number = version[len(self.games[game].get('version_prefix', '')):]
        for line in notes_since(text, release.version, number):
            out.append(P.frame(P.F_RELAY, ('NOTE ' + line)[:P.MAX_DATA]))
        for path, size, s in release.manifest:
            out.append(P.frame(P.F_RELAY, 'FILE %s %d %d' % (path, size, s)))
        out.append(P.frame(P.F_RELAY, 'END'))
        await say(writer, b''.join(out))
        sent = 0
        while True:
            try:
                ftype, data = await asyncio.wait_for(P.read_frame(reader), 60)
            except (asyncio.TimeoutError, asyncio.IncompleteReadError):
                break
            if ftype != P.F_RELAY:
                break
            cmd, _, path = P.text(data).partition(' ')
            if cmd != 'GET' or path not in release.files:
                break
            content = release.files[path]
            await say(writer, P.frame(P.F_RELAY, 'SIZE %d' % len(content)))
            for at in range(0, len(content), 240):
                writer.write(P.frame(P.F_RELAY_DATA, content[at:at + 240]))
                if at % (240 * 64) == 0:
                    await writer.drain()
            await writer.drain()
            sent += 1
        log.info('update %s %s -> %s: %d files sent', game, version,
                 release.version, sent)
