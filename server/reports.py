"""Bug reports from the Amigas: REPORT <proto> <version> <kind>, then
FIELD <key> <value> (email, text, session, machine), FILE <name> <size> with
the file in F_RELAY_DATA frames, and DONE. Each report goes to its own
folder data/reports/<yyyymmdd-hhmmss>-<number>/ with report.json and the
files; the Amiga gets REPORTED <number>.
"""
import asyncio
import json
import logging
import os
import re
import time

import protocol as P

log = logging.getLogger('reports')

FIELDS = {'email': 80, 'text': 1100, 'session': 20, 'machine': 120}
NAME_OK = re.compile(r'^[A-Za-z0-9_.-]{1,40}$')


class Reports:
    def __init__(self, config):
        self.cfg = config
        self.dir = os.path.join(config['data_dir'], 'reports')
        os.makedirs(self.dir, exist_ok=True)
        self.per_address = {}            # address -> [day, count]
        self.number = self.last_number()

    def last_number(self):
        n = 0
        for d in os.listdir(self.dir):
            try:
                n = max(n, int(d.rsplit('-', 1)[1]))
            except (IndexError, ValueError):
                pass
        return n

    def used_bytes(self):
        total = 0
        for root, _, files in os.walk(self.dir):
            for f in files:
                try:
                    total += os.path.getsize(os.path.join(root, f))
                except OSError:
                    pass
        return total

    def listing(self, limit=50, game=None):
        """The newest reports; game: only those of that game (a key of
        protocol.GAMES, by the version of the report)."""
        out = []
        for d in sorted(os.listdir(self.dir), reverse=True):
            if limit is not None and len(out) >= limit:
                break
            try:
                with open(os.path.join(self.dir, d, 'report.json'),
                          encoding='utf-8') as f:
                    rec = json.load(f)
                if game and P.game_of(rec.get('version', ''))[1] != game:
                    continue
                rec['folder'] = d
                out.append(rec)
            except (OSError, ValueError):
                pass
        return out

    def count(self, game=None):
        if game:
            return len(self.listing(None, game))
        return len([d for d in os.listdir(self.dir)
                    if os.path.isdir(os.path.join(self.dir, d))])

    async def receive(self, reader, writer, address, version, kind, relay):
        say = relay.say
        day = time.strftime('%Y%m%d')
        seen = self.per_address.get(address)
        if seen is None or seen[0] != day:
            seen = self.per_address[address] = [day, 0]
        if seen[1] >= self.cfg['reports_per_address_day']:
            await say(writer, P.frame(P.F_RELAY, 'ERR Too many reports today.'))
            return
        if self.used_bytes() >= self.cfg['reports_total_bytes']:
            await say(writer, P.frame(P.F_RELAY, 'ERR Server full.'))
            return
        fields = {}
        files = []                       # (name, bytes)
        total = 0
        current = None                   # [name, size, bytearray]
        limit = self.cfg['report_max_bytes']
        while True:
            ftype, data = await asyncio.wait_for(P.read_frame(reader), 60)
            if ftype == P.F_RELAY_DATA:
                if current is None:
                    return
                current[2] += data
                total += len(data)
                if total > limit or len(current[2]) > current[1]:
                    await say(writer, P.frame(P.F_RELAY, 'ERR Report too large.'))
                    return
                if len(current[2]) == current[1]:
                    files.append((current[0], bytes(current[2])))
                    current = None
                continue
            if ftype != P.F_RELAY:
                return
            line = P.text(data)
            cmd, _, rest = line.partition(' ')
            if cmd == 'FIELD':
                key, _, value = rest.partition(' ')
                if key in FIELDS:
                    fields[key] = fields.get(key, '') + value
                    fields[key] = fields[key][:FIELDS[key]]
            elif cmd == 'FILE':
                name, _, size = rest.partition(' ')
                try:
                    size = int(size)
                except ValueError:
                    return
                if not NAME_OK.match(name) or size < 0 or size > limit:
                    await say(writer, P.frame(P.F_RELAY, 'ERR Bad file.'))
                    return
                if size == 0:
                    files.append((name, b''))
                else:
                    current = [name, size, bytearray()]
            elif cmd == 'DONE':
                break
            else:
                return
        self.number += 1
        seen[1] += 1
        now = time.localtime()
        folder = '%s-%d' % (time.strftime('%Y%m%d-%H%M%S', now), self.number)
        path = os.path.join(self.dir, folder)
        os.makedirs(path)
        for name, content in files:
            with open(os.path.join(path, name), 'wb') as f:
                f.write(content)
        rec = {'number': self.number,
               'date': time.strftime('%Y-%m-%d', now),
               'time': time.strftime('%H:%M:%S', now),
               'version': version, 'kind': P.clean(kind, 12) or 'F10',
               'email': fields.get('email', ''),
               'text': fields.get('text', ''),
               'session': fields.get('session', ''),
               'machine': fields.get('machine', ''),
               'address': address,
               'files': [{'name': n, 'bytes': len(c)} for n, c in files],
               'status': 'new'}
        with open(os.path.join(path, 'report.json'), 'w', encoding='utf-8') as f:
            json.dump(rec, f, indent=1, ensure_ascii=False)
        log.info('report %d from %s (%s, %s): %d files, %d bytes', self.number,
                 address, version, rec['email'], len(files), total)
        await say(writer, P.frame(P.F_RELAY, 'REPORTED %d' % self.number))
