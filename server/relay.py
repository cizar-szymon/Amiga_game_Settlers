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


class Session:
    def __init__(self, code, name, version, host, password=''):
        self.code = code
        self.name = name
        self.password = password         # '' : anyone may join
        self.version = version
        self.host = host                 # Side: made the game (MASTER)
        self.guest = None                # Side: joined (SLAVE)
        self.created = time.time()
        self.started = None              # when paired
        self.paired = asyncio.Event()
        self.ended = asyncio.Event()
        self.end_reason = None

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
                'host': side(self.host), 'guest': side(self.guest)}


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
        self.totals['games'] += 1
        self.totals['seconds'] += rec['played_s']
        self.totals['longest_s'] = max(self.totals['longest_s'], rec['played_s'])
        if rec.get('end') not in ('game ended', 'left'):
            self.totals['dropped'] += 1
        self.recent.append(rec)
        del self.recent[:-30]

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
        for key, side in (('host', s.host), ('guest', s.guest)):
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
            if s.started is None:
                line = 'SESSION %s %d %s\t%s\t%s\t%d' % (
                    s.code, now - s.created, s.version, s.host.player, s.name,
                    1 if s.password else 0)
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
        name, _, password = rest.partition('\t')
        player = P.clean(player, 15) or 'PLAYER 1'
        name = P.clean(name, 15) or player
        password = P.clean(password, 10, extra='').upper()
        if len(self.sessions) >= self.cfg['max_sessions']:
            self.totals['refused'] += 1
            await self.say(writer, P.frame(P.F_RELAY, 'ERR Server full.'))
            return
        code = self.new_code()
        s = Session(code, name, version, Side(reader, writer, address, player),
                    password)
        self.sessions[code] = s
        log.info('session %s %r made by %s (%s, %s)', code, name, player,
                 address, version)
        await self.say(writer, P.frame(P.F_RELAY, 'CREATED ' + code))
        try:
            # Waiting for the other player: the maker sends nothing, but
            # a closed connection shows as EOF.
            eof = asyncio.ensure_future(reader.read(1))
            paired = asyncio.ensure_future(s.paired.wait())
            done, _ = await asyncio.wait({eof, paired},
                                         timeout=self.cfg['waiting_s'],
                                         return_when=asyncio.FIRST_COMPLETED)
            if paired not in done:
                paired.cancel()
                eof.cancel()
                s.end_reason = 'no player came' if not done else 'maker left'
                return
            if eof.done() and eof.result():
                # A byte from the maker before the pairing: not the game.
                s.end_reason = 'bad data'
                return
            eof.cancel()
            await self.forward(s)
        finally:
            self.sessions.pop(code, None)
            s.ended.set()
            if s.guest:
                try:
                    s.guest.writer.close()
                except Exception:
                    pass
            self.record(s)

    async def join(self, reader, writer, address, version, rest):
        code, _, rest = rest.partition(' ')
        player, _, password = rest.partition('\t')
        code = code.strip().upper()
        player = P.clean(player, 15) or 'PLAYER 2'
        password = P.clean(password, 10, extra='').upper()
        s = self.sessions.get(code)
        if s is None or s.started is not None:
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
        s.guest = Side(reader, writer, address, player)
        s.started = time.time()
        log.info('session %s %r: %s joined (%s)', code, s.name, player, address)
        await self.say(s.host.writer, P.frame(P.F_RELAY, 'PAIRED ' + player))
        await self.say(writer, P.frame(P.F_RELAY, 'PAIRED ' + s.host.player))
        s.paired.set()
        # The maker's task passes the frames of both; this one waits.
        await s.ended.wait()

    async def forward(self, s):
        a, b = s.host, s.guest
        t1 = asyncio.ensure_future(self.pump(s, a, b))
        t2 = asyncio.ensure_future(self.pump(s, b, a))
        done, pending = await asyncio.wait({t1, t2},
                                           return_when=asyncio.FIRST_COMPLETED)
        for t in pending:
            t.cancel()
        for t in done:
            reason = t.result() if not t.exception() else 'error'
            s.end_reason = s.end_reason or reason
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
            elif ftype == P.GAME_BASE + 3:        # MessageQuit
                s.end_reason = s.end_reason or 'game ended'
            await self.say(dst.writer, P.frame(ftype, data))

    def status(self):
        now = time.time()
        return {'uptime_s': round(now - self.started),
                'connections': self.connections,
                'sessions': [s.info(now) for s in self.sessions.values()],
                'totals': dict(self.totals),
                'recent': list(reversed(self.recent))}

