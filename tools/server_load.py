#!/usr/bin/env python3
"""Load on the game server: N games of two fake Amigas each, every side a
turn of ~56 bytes every 80 ms and a ping a second (what the game sends),
for S seconds; prints the turns sent and passed on and the round trips.

  python tools/server_load.py HOST [PORT] [-n 50] [-s 60] [-v VERSION]
"""
import argparse
import asyncio
import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'server'))
import protocol as P   # noqa: E402


async def frame_in(r):
    return await P.read_frame(r)


async def main(a):
    pairs = []
    for i in range(a.n):
        ra, wa = await asyncio.open_connection(a.host, a.port)
        wa.write(P.frame(P.F_RELAY, 'CREATE 1 %s A%d\tLOAD%d' % (a.v, i, i)))
        t, d = await frame_in(ra)
        code = P.text(d).split()[1]
        rb, wb = await asyncio.open_connection(a.host, a.port)
        wb.write(P.frame(P.F_RELAY, 'JOIN 1 %s %s B%d' % (a.v, code, i)))
        await frame_in(rb)
        await frame_in(ra)
        pairs.append(((ra, wa), (rb, wb)))
    print('%d games made' % a.n)
    sent = [0]
    got = [0]
    rtts = []
    end = time.time() + a.s

    async def talk(w):
        n = 0
        nxt = time.time()
        while time.time() < end:
            w.write(P.frame(P.GAME_BASE + 2, bytes(54)))
            sent[0] += 1
            if n % 12 == 0:
                w.write(P.frame(P.F_PING, struct.pack('>d', time.time())[:4] +
                                struct.pack('>I', n)[:0]))
            n += 1
            nxt += 0.08
            await asyncio.sleep(max(0, nxt - time.time()))

    async def listen(r, w):
        try:
            while True:
                t, d = await asyncio.wait_for(frame_in(r), a.s + 10)
                if t == P.GAME_BASE + 2:
                    got[0] += 1
                elif t == P.F_PING:
                    w.write(P.frame(P.F_PONG, d))
        except (asyncio.TimeoutError, asyncio.IncompleteReadError, OSError):
            pass

    tasks = []
    for (ra, wa), (rb, wb) in pairs:
        tasks += [talk(wa), talk(wb), listen(ra, wa), listen(rb, wb)]
    await asyncio.wait([asyncio.ensure_future(t) for t in tasks],
                       timeout=a.s + 5)
    print('%d s: %d turns sent, %d passed on (%.1f%%), %.0f turns/s'
          % (a.s, sent[0], got[0], 100.0 * got[0] / max(1, sent[0]),
             sent[0] / a.s))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('host')
    ap.add_argument('port', nargs='?', type=int, default=6810)
    ap.add_argument('-n', type=int, default=50)
    ap.add_argument('-s', type=float, default=60)
    ap.add_argument('-v', default='0.131')
    asyncio.run(main(ap.parse_args()))
