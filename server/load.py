"""The load of the server, sampled every few seconds for the page: the CPU
of the whole Pi (/proc/stat) and of this server process, the game data it
passes between the Amigas (bytes in and out, counted by the relay) and all
the traffic of the Pi's network (/proc/net/dev, without lo). Where /proc is
missing (the tests on Windows) those values are None.

Kept for an hour: the value now (the last sample) and the highest of the
hour.
"""
import asyncio
import time

STEP_S = 5
KEEP = 3600 // STEP_S


def read_cpu():
    """(busy, total) jiffies of all the CPUs, or None."""
    try:
        with open('/proc/stat') as f:
            v = [int(x) for x in f.readline().split()[1:]]
    except (OSError, ValueError):
        return None
    idle = v[3] + (v[4] if len(v) > 4 else 0)         # idle + iowait
    return sum(v) - idle, sum(v)


def read_net():
    """(received, sent) bytes of all the interfaces but lo, or None."""
    try:
        with open('/proc/net/dev') as f:
            lines = f.readlines()[2:]
    except OSError:
        return None
    rx = tx = 0
    for line in lines:
        name, _, rest = line.partition(':')
        if name.strip() == 'lo':
            continue
        v = rest.split()
        rx += int(v[0])
        tx += int(v[8])
    return rx, tx


class Load:
    def __init__(self, relay):
        self.relay = relay
        self.samples = []            # dicts, oldest first
        self.last = None

    def take(self):
        now = time.time()
        cur = {'t': now, 'cpu': read_cpu(), 'proc': time.process_time(),
               'net': read_net(),
               'game': (self.relay.bytes_in, self.relay.bytes_out)}
        prev, self.last = self.last, cur
        if prev is None:
            return
        dt = now - prev['t']
        if dt <= 0:
            return
        s = {'time': round(now),
             'cpu_pi_pct': None, 'cpu_server_pct': None,
             'game_in_bps': None, 'game_out_bps': None,
             'net_in_bps': None, 'net_out_bps': None}
        if cur['cpu'] and prev['cpu']:
            busy = cur['cpu'][0] - prev['cpu'][0]
            total = cur['cpu'][1] - prev['cpu'][1]
            if total > 0:
                s['cpu_pi_pct'] = round(100.0 * busy / total, 1)
        s['cpu_server_pct'] = round(100.0 * (cur['proc'] - prev['proc']) / dt, 1)
        s['game_in_bps'] = round((cur['game'][0] - prev['game'][0]) / dt)
        s['game_out_bps'] = round((cur['game'][1] - prev['game'][1]) / dt)
        if cur['net'] and prev['net']:
            s['net_in_bps'] = round((cur['net'][0] - prev['net'][0]) / dt)
            s['net_out_bps'] = round((cur['net'][1] - prev['net'][1]) / dt)
        self.samples.append(s)
        del self.samples[:-KEEP]

    def status(self):
        """The last sample ('now') and the highest of each value in the hour
        ('max_hour'), and the samples of the hour for the charts."""
        if not self.samples:
            return {'step_s': STEP_S, 'now': None, 'max_hour': None,
                    'hour': []}
        top = {}
        for k in self.samples[-1]:
            if k == 'time':
                continue
            vals = [x[k] for x in self.samples if x[k] is not None]
            top[k] = max(vals) if vals else None
        return {'step_s': STEP_S, 'now': self.samples[-1], 'max_hour': top,
                'hour': self.samples}

    async def run(self):
        while True:
            self.take()
            await asyncio.sleep(STEP_S)
