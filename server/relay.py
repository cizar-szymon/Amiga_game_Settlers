"""Game sessions: an Amiga makes a game (CREATE) and gets a code, another
one joins it (JOIN, from the list or by the code); then the server passes
the frames between them both ways until one side ends.

The frames are read one by one (the length byte), so the server counts the
bytes and measures the connection: a PING of one side (4 bytes of its
clock) goes to the other side, whose PONG echoes it back through the
server - the time between is the server's round trip to that side.
"""
import asyncio
import json
import logging
import os
import random
import time

import protocol as P

log = logging.getLogger('relay')

CODE_LETTERS = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'


class Side:
    """One Amiga of a session."""

    def __init__(self, reader, writer, address, player):
        self.reader = reader
        self.writer = writer
        self.address = address
        self.player = player
        self.bytes_in = 0
        self.frames_in = 0
        self.last_frame = time.time()
        self.pings = {}          # ping payload -> time sent on to this side
        self.rtt_ms = []         # the round trips server <-> this side
        self.closed = False

    def rtt_now(self):
        return self.rtt_ms[-1] if self.rtt_ms else None

    def rtt_avg(self):
        return round(sum(self.rtt_ms) / len(self.rtt_ms)) if self.rtt_ms else None

    def rtt_max(self):
        return max(self.rtt_ms) if self.rtt_ms else None


class Link:
    """One connection of the maker and the player that joined on it. A game
    of 2 has one; a game of 3 a second one (ATTACH)."""

    def __init__(self, host):
        self.host = host                 # Side: the maker, this connection
        self.guest = None                # Side: joined
        self.paired = asyncio.Event()
        self.ended = asyncio.Event()
        self.reason = None               # why it ended


class Session:
    def __init__(self, code, name, version, host, password='', max_players=2):
        self.code = code
        self.name = name
        self.password = password         # '' : anyone may join
        self.version = version
        self.max_players = max_players
        self.host = host                 # Side: made the game (MASTER)
        self.links = [Link(host)]
        self.quit_type = P.game_of(version)[3]
        self.created = time.time()
        self.started = None              # when the first player joined
        self.end_reason = None
        self.last_reason = None          # of the last pair that ended

    @property
    def guest(self):
        return self.links[0].guest

    @property
    def guest2(self):
        return self.links[1].guest if len(self.links) > 1 else None

    def players(self):
        return 1 + len([l for l in self.links if l.guest])

    def free_link(self):
        """The connection of the maker the next player is paired with."""
        for link in self.links:
            if link.guest is None and not link.ended.is_set():
                return link
        return None

    def info(self, now):
        def side(s):
            if s is None:
                return None
            return {'player': s.player, 'address': s.address,
                    'rtt_ms': s.rtt_now(),
                    'rtt_avg_ms': s.rtt_avg(), 'rtt_max_ms': s.rtt_max(),
                    'bytes': s.bytes_in, 'idle_s': round(now - s.last_frame)}
        return {'code': self.code, 'name': self.name, 'version': self.version,
                'password': bool(self.password),
                'waiting_s': None if self.started else round(now - self.created),
                'playing_s': round(now - self.started) if self.started else None,
                'host': side(self.host), 'guest': side(self.guest),
                'guest2': side(self.guest2),
                'players': self.players(), 'max_players': self.max_players,
                'game': P.game_of(self.version)[1]}


class Relay:
    def __init__(self, config, reports):
        self.cfg = config
        self.reports = reports
        self.sessions = {}           # code -> Session
        self.connections = 0
        self.per_address = {}
        self.started = time.time()
        self.data_dir = config['data_dir']
        self.history_path = os.path.join(self.data_dir, 'sessions.jsonl')
        self.totals = {'games': 0, 'seconds': 0, 'longest_s': 0,
                       'dropped': 0, 'refused': 0}
        self.recent = []             # the last ended sessions (the page)
        # the same for each game (the subpages): key -> totals, recent
        self.games = {g[1]: {'totals': {'games': 0, 'seconds': 0,
                                        'longest_s': 0, 'dropped': 0},
                             'recent': []} for g in P.GAMES}
        self.bytes_in = 0           # game data from the Amigas (all sessions)
        self.bytes_out = 0           # game data passed on to them
        self.load = None             # load.Load, set by the server
        self.load_history()

    # --- history -------------------------------------------------------
    def load_history(self):
        try:
            with open(self.history_path, encoding='utf-8') as f:
                for line in f:
                    try:
                        self.count(json.loads(line))
                    except ValueError:
                        pass
        except FileNotFoundError:
            pass

    def count(self, rec):
        if rec.get('played_s') is None:
            return
        game = self.games[P.game_of(rec.get('version', ''))[1]]
        for totals, recent in ((self.totals, self.recent),
                               (game['totals'], game['recent'])):
            totals['games'] += 1
            totals['seconds'] += rec['played_s']
            totals['longest_s'] = max(totals['longest_s'], rec['played_s'])
            if rec.get('end') not in ('game ended', 'left'):
                totals['dropped'] += 1
            recent.append(rec)
            del recent[:-30]

    def record(self, s):
        now = time.time()
        rec = {'code': s.code, 'name': s.name, 'version': s.version,
               'created': time.strftime('%Y-%m-%d %H:%M:%S',
                                        time.localtime(s.created)),
               'started': time.strftime('%Y-%m-%d %H:%M:%S',
                                        time.localtime(s.started)) if s.started else None,
               'ended': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(now)),
               'played_s': round(now - s.started) if s.started else None,
               'end': s.end_reason,
               'host': s.host.player,
               'guest': s.guest.player if s.guest else None}
        if s.guest2:
            rec['guest2'] = s.guest2.player
        for key, side in (('host', s.host), ('guest', s.guest),
                          ('guest2', s.guest2)):
            if side:
                rec[key + '_rtt_avg_ms'] = side.rtt_avg()
                rec[key + '_rtt_max_ms'] = side.rtt_max()
                rec[key + '_bytes'] = side.bytes_in
                rec[key + '_address'] = side.address
        os.makedirs(self.data_dir, exist_ok=True)
        with open(self.history_path, 'a', encoding='utf-8') as f:
            f.write(json.dumps(rec) + '\n')
        self.count(rec)
        log.info('session %s %r ended: %s, played %s s', s.code, s.name,
                 s.end_reason, rec['played_s'])

    # --- connections ---------------------------------------------------
    async def handle(self, reader, writer):
        peer = writer.get_extra_info('peername') or ('?', 0)
        address = peer[0]
        if (self.connections >= self.cfg['max_connections'] or
                self.per_address.get(address, 0) >= self.cfg['max_per_address']):
            self.totals['refused'] += 1
            await self.say(writer, P.frame(P.F_RELAY, 'ERR Too many connections.'))
            writer.close()
            return
        self.connections += 1
        self.per_address[address] = self.per_address.get(address, 0) + 1
        try:
            await self.serve(reader, writer, address)
        except (asyncio.IncompleteReadError, ConnectionError, P.FrameError,
                asyncio.TimeoutError, OSError) as e:
            log.debug('%s: %s', address, e)
        finally:
            self.connections -= 1
            self.per_address[address] -= 1
            if self.per_address[address] == 0:
                del self.per_address[address]
            try:
                writer.close()
            except Exception:
                pass

    @staticmethod
    async def say(writer, data):
        try:
            writer.write(data)
            await writer.drain()
        except (ConnectionError, OSError):
            pass

    async def serve(self, reader, writer, address):
        ftype, data = await asyncio.wait_for(P.read_frame(reader),
                                             self.cfg['first_frame_s'])
        if ftype == P.F_HELLO:
            # An Amiga connecting straight (JOIN SERVER): not a game here.
            await self.say(writer, P.frame(P.F_BYE, 'This is the game server: use INTERNET GAME.'))
            return
        if ftype != P.F_RELAY:
            return
        words = P.text(data).split(' ', 3)
        cmd = words[0]
        if len(words) < 3 or words[1] != str(P.PROTOCOL):
            await self.say(writer, P.frame(P.F_RELAY, 'ERR Different version.'))
            return
        version = P.clean(words[2], 20, extra='.-_')
        rest = words[3] if len(words) > 3 else ''
        if cmd == 'LIST':
            await self.send_list(writer, version)
        elif cmd == 'CREATE':
            await self.create(reader, writer, address, version, rest)
        elif cmd == 'ATTACH':
            await self.attach(reader, writer, address, version, rest)
        elif cmd == 'JOIN':
            await self.join(reader, writer, address, version, rest)
        elif cmd == 'REPORT':
            await self.reports.receive(reader, writer, address, version,
                                       rest, self)
        else:
            await self.say(writer, P.frame(P.F_RELAY, 'ERR Unknown command.'))

    async def send_list(self, writer, version):
        now = time.time()
        out = b''
        for s in list(self.sessions.values()):
            # only the games of the asking game (Settlers never sees an
            # OpenDUNE one); its other versions are listed, JOIN refuses them
            if s.free_link() is not None and \
                    P.game_of(s.version) is P.game_of(version):
                line = 'SESSION %s %d %s\t%s\t%s\t%d' % (
                    s.code, now - s.created, s.version, s.host.player, s.name,
                    1 if s.password else 0)
                if s.max_players > 2:
                    line += '\t%d/%d' % (s.players(), s.max_players)
                out += P.frame(P.F_RELAY, line[:P.MAX_DATA])
        out += P.frame(P.F_RELAY, 'END')
        await self.say(writer, out)

    def new_code(self):
        while True:
            code = ''.join(random.choice(CODE_LETTERS) for _ in range(4))
            if code not in self.sessions:
                return code

    async def create(self, reader, writer, address, version, rest):
        player, _, rest = rest.partition('\t')
        name, _, rest = rest.partition('\t')
        password, _, most = rest.partition('\t')
        player = P.clean(player, 15) or 'PLAYER 1'
        name = P.clean(name, 15) or player
        password = P.clean(password, 10, extra='').upper()
        max_players = 3 if most.strip() == '3' else 2
        if len(self.sessions) >= self.cfg['max_sessions']:
            self.totals['refused'] += 1
            await self.say(writer, P.frame(P.F_RELAY, 'ERR Server full.'))
            return
        code = self.new_code()
        s = Session(code, name, version, Side(reader, writer, address, player),
                    password, max_players)
        self.sessions[code] = s
        log.info('session %s %r made by %s (%s, %s, %d players)', code, name,
                 player, address, version, max_players)
        await self.say(writer, P.frame(P.F_RELAY, 'CREATED ' + code))
        await self.run_link(s, s.links[0], reader)

    async def attach(self, reader, writer, address, version, rest):
        """Another connection of the maker of a game of 3, for the second
        player that joins. Only from the maker's address."""
        s = self.sessions.get(rest.strip().upper())
        # (a connection whose player left may be replaced by a new one)
        if (s is None or s.version != version or address != s.host.address or
                len([l for l in s.links if not l.ended.is_set()])
                >= s.max_players - 1):
            await self.say(writer, P.frame(P.F_RELAY, 'ERR No such game.'))
            return
        link = Link(Side(reader, writer, address, s.host.player))
        s.links.append(link)
        await self.say(writer, P.frame(P.F_RELAY, 'ATTACHED'))
        await self.run_link(s, link, reader)

    async def run_link(self, s, link, reader):
        """A connection of the maker from its start to its end: waits for a
        player, then passes the frames of the two."""
        try:
            # Waiting for the other player: the maker sends nothing, but
            # a closed connection shows as EOF.
            eof = asyncio.ensure_future(reader.read(1))
            paired = asyncio.ensure_future(link.paired.wait())
            done, _ = await asyncio.wait({eof, paired},
                                         timeout=self.cfg['waiting_s'],
                                         return_when=asyncio.FIRST_COMPLETED)
            if paired not in done:
                paired.cancel()
                eof.cancel()
                link.reason = 'no player came' if not done else 'maker left'
                return
            if eof.done() and eof.result():
                # A byte from the maker before the pairing: not the game.
                link.reason = 'bad data'
                return
            eof.cancel()
            await self.forward(s, link)
        finally:
            self.end_link(s, link)

    def end_link(self, s, link):
        link.ended.set()
        if link.guest:
            s.last_reason = link.reason
            try:
                link.guest.writer.close()
            except Exception:
                pass
        elif link is s.links[0]:
            # The maker's first connection went before anyone joined: the
            # game is off, a connection still waiting goes too.
            for other in s.links[1:]:
                if other.guest is None:
                    try:
                        other.host.writer.close()
                    except Exception:
                        pass
        if all(l.ended.is_set() for l in s.links) and \
                self.sessions.get(s.code) is s:
            del self.sessions[s.code]
            s.end_reason = s.end_reason or s.last_reason or s.links[0].reason
            self.record(s)

    async def join(self, reader, writer, address, version, rest):
        code, _, rest = rest.partition(' ')
        player, _, password = rest.partition('\t')
        code = code.strip().upper()
        player = P.clean(player, 15) or 'PLAYER 2'
        password = P.clean(password, 10, extra='').upper()
        s = self.sessions.get(code)
        link = s.free_link() if s else None
        if link is None:
            await self.say(writer, P.frame(P.F_RELAY, 'ERR No such game.'))
            return
        if s.version != version:
            await self.say(writer, P.frame(P.F_RELAY,
                                           'ERR The game needs version %s.' % s.version))
            return
        if s.password and password != s.password:
            await self.say(writer, P.frame(P.F_RELAY, 'ERR Password needed.'
                                           if not password else 'ERR Wrong password.'))
            return
        link.guest = Side(reader, writer, address, player)
        if s.started is None:
            s.started = time.time()
        log.info('session %s %r: %s joined (%s)', code, s.name, player, address)
        await self.say(link.host.writer, P.frame(P.F_RELAY, 'PAIRED ' + player))
        await self.say(writer, P.frame(P.F_RELAY, 'PAIRED ' + s.host.player))
        link.paired.set()
        # The maker's task passes the frames of both; this one waits.
        await link.ended.wait()

    async def forward(self, s, link):
        a, b = link.host, link.guest
        t1 = asyncio.ensure_future(self.pump(s, a, b))
        t2 = asyncio.ensure_future(self.pump(s, b, a))
        done, pending = await asyncio.wait({t1, t2},
                                           return_when=asyncio.FIRST_COMPLETED)
        for t in pending:
            t.cancel()
        for t in done:
            link.reason = link.reason or \
                (t.result() if not t.exception() else 'error')
        for side in (a, b):
            side.closed = True
            try:
                side.writer.close()
            except Exception:
                pass

    async def pump(self, s, src, dst):
        """The frames of src to dst; returns why it stopped."""
        idle = self.cfg['idle_s']
        while True:
            try:
                ftype, data = await asyncio.wait_for(P.read_frame(src.reader), idle)
            except asyncio.TimeoutError:
                return 'no data'
            except (asyncio.IncompleteReadError, ConnectionError, OSError):
                await self.say(dst.writer, P.frame(P.F_BYE, 'The other player left.'))
                return 'connection lost'
            except P.FrameError:
                return 'bad data'
            now = time.time()
            src.last_frame = now
            src.frames_in += 1
            src.bytes_in += len(data) + 2
            self.bytes_in += len(data) + 2
            if ftype == P.F_PING:
                dst.pings[bytes(data)] = now
                if len(dst.pings) > 20:
                    dst.pings.pop(next(iter(dst.pings)))
            elif ftype == P.F_PONG:
                t = src.pings.pop(bytes(data), None)
                if t is not None:
                    src.rtt_ms.append(round((now - t) * 1000))
                    del src.rtt_ms[:-120]
            elif ftype == P.F_BYE:
                await self.say(dst.writer, P.frame(ftype, data))
                return 'left'
            elif ftype == s.quit_type:            # the game's quit frame
                s.end_reason = s.end_reason or 'game ended'
            await self.say(dst.writer, P.frame(ftype, data))
            self.bytes_out += len(data) + 2

    def status(self):
        now = time.time()
        return {'uptime_s': round(now - self.started),
                'connections': self.connections,
                'sessions': [s.info(now) for s in self.sessions.values()],
                'totals': dict(self.totals),
                'recent': list(reversed(self.recent)),
                'games': {k: {'totals': dict(g['totals']),
                              'recent': list(reversed(g['recent']))}
                          for k, g in self.games.items()},
                'load': self.load.status() if self.load else None}

