"""The page of the server (home network only): the games now, the games
played (times, quality of the connections), the bug reports. A tiny HTTP
server on asyncio, no framework: GET /, /stats.json, /reports.json, and
for each report /reports/<folder>/ (its details), /reports/<folder>.zip,
/reports/<folder>/<file>; all of them /reports/all.zip. POST
/reports/<folder>/delete (the Delete button: a joke, an empty report) moves
the report to data/deleted/ - off the page and the counts, still on the
disk until that folder is emptied by hand.
"""
import asyncio
import html
import io
import ipaddress
import json
import logging
import os
import re
import shutil
import time
import zipfile

log = logging.getLogger('web')


def hms(s):
    if s is None:
        return '-'
    s = int(s)
    return '%d:%02d:%02d' % (s // 3600, s % 3600 // 60, s % 60) if s >= 3600 \
        else '%d:%02d' % (s // 60, s % 60)


def quality(ms):
    """The round trip to the server, in words."""
    if ms is None:
        return '-'
    if ms < 80:
        return 'very good'
    if ms < 200:
        return 'good'
    if ms < 400:
        return 'fair'
    return 'poor'


def rate(bps):
    """Bytes a second in words, with the bits (a connection's speed)."""
    if bps is None:
        return '-'
    kb = bps / 1024.0
    text = '%.1f KB/s' % kb if kb < 100 else '%d KB/s' % kb
    bits = bps * 8 / 1000.0
    btext = '%.0f kbit/s' % bits if bits < 1000 else '%.1f Mbit/s' % (bits / 1000)
    return '%s <span class="dim">(%s)</span>' % (text, btext)


def pct(v):
    return '-' if v is None else '%.1f %%' % v


def spark(values, top):
    """A small line of the hour (SVG), 0 at the bottom, top at the top."""
    pts = [v for v in values]
    if len(pts) < 2 or not top:
        return ''
    w, h = 240, 28
    step = w / (len(pts) - 1)
    line = ' '.join('%.1f,%.1f' % (i * step, h - 1 - (h - 2) * (v or 0) / top)
                    for i, v in enumerate(pts))
    return ('<svg width="%d" height="%d" viewBox="0 0 %d %d"><polyline points="%s" '
            'fill="none" stroke="#3b6ea5" stroke-width="1.5"/></svg>' % (w, h, w, h, line))


# The subpages of the page: (path, name). Another server on this Pi adds its
# own load as a block of the Load page (Web.load_blocks).
PAGES = (('/', 'Games and reports'), ('/load', 'Load'))


def nav(active):
    return '<nav>%s</nav>' % ''.join(
        '<a class="nav%s" href="%s">%s</a>' % (' on' if p == active else '', p, n)
        for p, n in PAGES)


STYLE = """
nav{display:flex;gap:6px;margin:10px 0 4px}
a.nav{padding:4px 12px;border:1px solid #9aa5b1;border-radius:4px;background:#fff;color:#1f2933;text-decoration:none;font-size:14px}
a.nav:hover{background:#e1e6eb} a.nav.on{background:#1f2933;color:#fff;border-color:#1f2933}
body{font-family:system-ui,Arial,sans-serif;margin:20px;background:#f6f7f9;color:#1f2933}
h1{font-size:22px;margin:0 0 4px} h2{font-size:16px;margin:24px 0 6px}
.sub{color:#52606d;font-size:13px}
.tiles{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}
.tile{background:#fff;border:1px solid #d5dae0;border-radius:6px;padding:8px 14px;min-width:110px;color:#52606d;font-size:13px}
.tile b{display:block;font-size:20px;color:#1f2933}
.wrap{overflow-x:auto}
table{border-collapse:collapse;background:#fff;font-size:14px;border:1px solid #d5dae0}
th,td{padding:5px 12px;text-align:left;vertical-align:top;white-space:nowrap}
td{border-top:1px solid #e4e7eb;font-variant-numeric:tabular-nums}
th{background:#eef1f4;font-weight:600;color:#323f4b}
tr.grp th{background:#e1e6eb;text-align:center;border-left:2px solid #c3cbd3}
.sep{border-left:2px solid #c3cbd3}
.ip,.mono{font-family:Consolas,Menlo,monospace;font-size:13px}
.dim{color:#7b8794}
a.btn{display:inline-block;padding:2px 10px;margin-right:4px;border:1px solid #9aa5b1;border-radius:4px;background:#fff;color:#1f2933;text-decoration:none;font-size:13px}
a.btn:hover,button.btn:hover{background:#e1e6eb}
button.btn{display:inline-block;padding:2px 10px;margin-right:4px;border:1px solid #9aa5b1;border-radius:4px;background:#fff;color:#1f2933;font:inherit;font-size:13px;cursor:pointer}
button.del{border-color:#d9a3a3;color:#a61b1b} button.del:hover{background:#fbe9e9}
form.inl{display:inline}
td.text{white-space:pre-wrap;max-width:560px;min-width:260px}
.head{display:flex;align-items:baseline;gap:14px}
pre{background:#fff;border:1px solid #d5dae0;padding:10px;white-space:pre-wrap;max-width:900px}
dl{display:grid;grid-template-columns:max-content auto;gap:4px 16px;background:#fff;border:1px solid #d5dae0;padding:10px;max-width:900px}
dt{font-weight:600;color:#323f4b} dd{margin:0}
.q-verygood,.q-good{color:#1b7f3b;font-weight:600}.q-fair{color:#b7791f;font-weight:600}.q-poor{color:#c0392b;font-weight:600}
"""


class Web:
    def __init__(self, config, relay, reports):
        self.cfg = config
        self.relay = relay
        self.reports = reports
        self.allowed = [ipaddress.ip_network(n) for n in config['web_allowed']]

    def allowed_address(self, address):
        try:
            ip = ipaddress.ip_address(address)
        except ValueError:
            return False
        return any(ip in n for n in self.allowed)

    async def handle(self, reader, writer):
        peer = writer.get_extra_info('peername') or ('?', 0)
        try:
            request = await asyncio.wait_for(reader.readuntil(b'\r\n\r\n'), 10)
            line = request.split(b'\r\n', 1)[0].decode('latin-1')
            parts = line.split()
            method = parts[0] if parts else 'GET'
            path = parts[1] if len(parts) > 1 else '/'
            if not self.allowed_address(peer[0]):
                await self.send(writer, 403, 'text/plain', b'Home network only.')
            elif method == 'POST' and path.startswith('/reports/') and \
                    path.endswith('/delete'):
                folder = path[len('/reports/'):-len('/delete')]
                if self.delete_report(folder, peer[0]):
                    await self.redirect(writer, '/#reports')
                else:
                    await self.send(writer, 404, 'text/plain', b'Not found.')
            elif path == '/stats.json':
                await self.send(writer, 200, 'application/json',
                                json.dumps(self.relay.status(), indent=1).encode())
            elif path == '/reports.json':
                await self.send(writer, 200, 'application/json',
                                json.dumps(self.reports.listing(), indent=1).encode())
            elif path.startswith('/reports/'):
                await self.report_path(writer, path[len('/reports/'):])
            elif path == '/load':
                await self.send(writer, 200, 'text/html; charset=utf-8',
                                self.load_page().encode('utf-8'))
            elif path == '/load.json':
                await self.send(writer, 200, 'application/json',
                                json.dumps(self.relay.load.status() if self.relay.load
                                           else None, indent=1).encode())
            elif path == '/':
                await self.send(writer, 200, 'text/html; charset=utf-8',
                                self.page().encode('utf-8'))
            else:
                await self.send(writer, 404, 'text/plain', b'Not found.')
        except (asyncio.TimeoutError, asyncio.IncompleteReadError,
                asyncio.LimitOverrunError, ConnectionError, OSError):
            pass
        finally:
            try:
                writer.close()
            except Exception:
                pass

    # --- the reports ---------------------------------------------------
    FOLDER = re.compile(r'^\d{8}-\d{6}-\d+$')
    FILE = re.compile(r'^[A-Za-z0-9_.-]{1,60}$')

    def folder_path(self, folder):
        if not self.FOLDER.match(folder):
            return None
        path = os.path.join(self.reports.dir, folder)
        return path if os.path.isdir(path) else None

    def zip_of(self, folders):
        out = io.BytesIO()
        with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
            for folder in folders:
                path = os.path.join(self.reports.dir, folder)
                for name in sorted(os.listdir(path)):
                    z.write(os.path.join(path, name), folder + '/' + name)
        return out.getvalue()

    def delete_report(self, folder, who):
        path = self.folder_path(folder)
        if path is None:
            return False
        bin_dir = os.path.join(os.path.dirname(self.reports.dir), 'deleted')
        os.makedirs(bin_dir, exist_ok=True)
        shutil.move(path, os.path.join(bin_dir, folder))
        log.info('report %s deleted from the page (%s)', folder, who)
        return True

    @staticmethod
    async def redirect(writer, where):
        writer.write(('HTTP/1.0 303 See Other\r\nLocation: %s\r\n'
                      'Content-Length: 0\r\nConnection: close\r\n\r\n'
                      % where).encode())
        await writer.drain()

    @staticmethod
    def delete_button(folder):
        return ('<form class="inl" method="post" action="/reports/%s/delete" '
                'onsubmit="return confirm(\'Delete report %s?\')">'
                '<button class="btn del" type="submit">Delete</button></form>'
                % (folder, folder))

    async def report_path(self, writer, rest):
        loop = asyncio.get_running_loop()
        if rest == 'all.zip':
            folders = sorted(d for d in os.listdir(self.reports.dir)
                             if self.folder_path(d))
            body = await loop.run_in_executor(None, self.zip_of, folders)
            await self.send(writer, 200, 'application/zip', body,
                            'settlers-reports-%s.zip' % time.strftime('%Y%m%d'))
            return
        if rest.endswith('.zip') and self.folder_path(rest[:-4]):
            folder = rest[:-4]
            body = await loop.run_in_executor(None, self.zip_of, [folder])
            await self.send(writer, 200, 'application/zip', body,
                            'settlers-report-%s.zip' % folder)
            return
        folder, _, name = rest.partition('/')
        path = self.folder_path(folder)
        if path is None:
            await self.send(writer, 404, 'text/plain', b'Not found.')
        elif name == '':
            await self.send(writer, 200, 'text/html; charset=utf-8',
                            self.report_page(folder, path).encode('utf-8'))
        elif self.FILE.match(name) and os.path.isfile(os.path.join(path, name)):
            with open(os.path.join(path, name), 'rb') as f:
                body = f.read()
            text = name.endswith('.txt') or name.endswith('.json')
            await self.send(writer, 200,
                            'text/plain; charset=utf-8' if text
                            else 'application/octet-stream', body,
                            None if text else name)
        else:
            await self.send(writer, 404, 'text/plain', b'Not found.')

    def report_page(self, folder, path):
        e = html.escape
        try:
            with open(os.path.join(path, 'report.json'), encoding='utf-8') as f:
                r = json.load(f)
        except (OSError, ValueError):
            r = {}
        out = ['<!doctype html><html><head><meta charset="utf-8">'
               '<meta name="viewport" content="width=device-width">'
               '<title>Report %s</title><style>%s</style></head><body>'
               % (e(str(r.get('number', folder))), STYLE),
               '<div class="head"><h1>Bug report %s</h1><a class="btn" href="/">'
               'Back</a><a class="btn" href="/reports/%s.zip">Download .zip</a>%s'
               '</div>' % (e(str(r.get('number', ''))), e(folder),
                           self.delete_button(e(folder))),
               '<h2>Report</h2><dl>']
        for key, label in (('date', 'Date'), ('time', 'Time'),
                           ('version', 'Version'), ('kind', 'Kind'),
                           ('email', 'E-mail'), ('machine', 'Machine'),
                           ('address', 'IP'), ('session', 'Game'),
                           ('status', 'Status')):
            if r.get(key):
                out.append('<dt>%s</dt><dd>%s</dd>' % (label, e(str(r[key]))))
        out.append('<dt>Folder</dt><dd class="mono">%s</dd></dl>' % e(folder))
        text = e(r.get('text') or '') or '<span class="dim">(nothing written)</span>'
        out.append('<h2>What happened</h2><pre>%s</pre>' % text)
        out.append('<h2>Files</h2><table><tr><th>File</th><th>Size</th></tr>')
        for name in sorted(os.listdir(path)):
            size = os.path.getsize(os.path.join(path, name))
            out.append('<tr><td><a href="/reports/%s/%s">%s</a></td><td>%d KB</td></tr>'
                       % (e(folder), e(name), e(name), (size + 1023) // 1024))
        out.append('</table></body></html>')
        return '\n'.join(out)

    @staticmethod
    async def send(writer, code, ctype, body, filename=None):
        reason = {200: 'OK', 403: 'Forbidden', 404: 'Not Found'}[code]
        head = ('HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n'
                'Cache-Control: no-store\r\nConnection: close\r\n'
                % (code, reason, ctype, len(body)))
        if filename:
            head += 'Content-Disposition: attachment; filename="%s"\r\n' % filename
        head += '\r\n'
        writer.write(head.encode() + body)
        await writer.drain()

    def load_blocks(self, load):
        """(title, rows) of the Load page: rows (key, label, format). The Pi
        first, then each server; a new server on this Pi adds a block."""
        return (('Raspberry Pi', (
                    ('cpu_pi_pct', 'CPU (all cores)', pct),
                    ('net_in_bps', 'Network, received', rate),
                    ('net_out_bps', 'Network, sent', rate))),
                ('Settlers game server (port %d)' % self.cfg['game_port'], (
                    ('cpu_server_pct', 'CPU of the server (one core = 100 %)', pct),
                    ('game_in_bps', 'Game data from the Amigas', rate),
                    ('game_out_bps', 'Game data to the Amigas', rate))))

    def load_page(self):
        e = html.escape
        load = self.relay.load.status() if self.relay.load else None
        out = ['<!doctype html><html><head><meta charset="utf-8">'
               '<meta http-equiv="refresh" content="15">'
               '<meta name="viewport" content="width=device-width">'
               '<title>Server load</title><style>%s</style></head><body>' % STYLE,
               '<h1>Settlers – game server</h1>', nav('/load'),
               '<div class="sub">%s · sampled every %d s, kept for an hour · '
               'refreshed every 15 s · <a href="/load.json">load.json</a></div>'
               % (time.strftime('%Y-%m-%d %H:%M:%S'),
                  load['step_s'] if load else 5)]
        if not load or not load['now']:
            out.append('<p class="dim">The first numbers come 5 s after the start.</p>')
            out.append('</body></html>')
            return ''.join(out)
        now, top, hour = load['now'], load['max_hour'], load['hour']
        for title, rows in self.load_blocks(load):
            out.append('<h2>%s</h2><div class="wrap"><table><tr><th></th>'
                       '<th>Now</th><th>Highest in the last hour</th>'
                       '<th>Last hour</th></tr>' % e(title))
            for key, label, fmt in rows:
                vals = [x[key] for x in hour]
                scale = top[key] or 0
                if key.endswith('pct'):
                    scale = max(scale, 10)
                out.append('<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>'
                           % (e(label), fmt(now[key]), fmt(top[key]),
                              spark(vals, scale) if now[key] is not None else ''))
            out.append('</table></div>')
        out.append('<div class="sub" style="margin-top:8px">The network of the Pi '
                   'counts everything: games, this page, ssh, updates.</div>')
        out.append('</body></html>')
        return ''.join(out)

    def page(self):
        st = self.relay.status()
        t = st['totals']
        e = html.escape
        playing = [s for s in st['sessions'] if s['playing_s'] is not None]
        waiting = [s for s in st['sessions'] if s['playing_s'] is None]
        avg = t['seconds'] / t['games'] if t['games'] else None
        out = ['<!doctype html><html><head><meta charset="utf-8">'
               '<meta http-equiv="refresh" content="15">'
               '<meta name="viewport" content="width=device-width">'
               '<title>Settlers server</title><style>%s</style></head><body>' % STYLE,
               '<h1>Settlers – game server</h1>', nav('/'),
               '<div class="sub">running %s · %s · port %d · refreshed every 15 s</div>'
               % (hms(st['uptime_s']), time.strftime('%Y-%m-%d %H:%M:%S'),
                  self.cfg['game_port']),
               '<div class="tiles">']
        for label, value in (('games now', len(playing)),
                             ('waiting for a player', len(waiting)),
                             ('connections', st['connections']),
                             ('games played', t['games']),
                             ('average game', hms(avg)),
                             ('longest game', hms(t['longest_s'])),
                             ('connections lost', t['dropped']),
                             ('bug reports', self.reports.count())):
            out.append('<div class="tile">%s<b>%s</b></div>' % (e(label), e(str(value))))
        out.append('</div>')

        def ms(v):
            return '-' if v is None else '%s ms' % v

        def side(x):
            """Player, IP, ping avg / max, quality: 5 cells."""
            if not x:
                return ('<td class="sep dim">-</td><td></td><td></td><td></td>'
                        '<td></td>')
            q = quality(x.get('rtt_avg_ms'))
            return ('<td class="sep">%s</td><td class="ip">%s</td><td>%s</td>'
                    '<td>%s</td><td class="q-%s">%s</td>'
                    % (e(x['player']), e(x.get('address') or '-'),
                       ms(x.get('rtt_avg_ms')), ms(x.get('rtt_max_ms')),
                       q.replace(' ', ''), q))

        side_head = ('<th class="sep">Name</th><th>IP</th><th>Ping avg</th>'
                     '<th>Ping max</th><th>Quality</th>')

        out.append('<h2>Games now</h2><div class="wrap"><table>'
                   '<tr class="grp"><th colspan="4" style="border-left:0"></th>'
                   '<th colspan="5">Player 1 (made the game)</th>'
                   '<th colspan="5">Player 2 (joined)</th></tr>'
                   '<tr><th>Code</th><th>Game</th><th>Version</th><th>Time</th>'
                   + side_head + side_head + '</tr>')
        for s in playing:
            out.append('<tr><td class="mono">%s</td><td>%s</td><td>%s</td>'
                       '<td>%s</td>%s%s</tr>'
                       % (e(s['code']), e(s['name']), e(s['version']),
                          hms(s['playing_s']), side(s['host']), side(s['guest'])))
        if not playing:
            out.append('<tr><td colspan="14" class="dim">none</td></tr>')
        out.append('</table></div>')

        out.append('<h2>Waiting for a player</h2><div class="wrap"><table><tr>'
                   '<th>Code</th><th>Game</th><th>Version</th><th>Player</th>'
                   '<th>IP</th><th>Password</th><th>Waiting</th></tr>')
        for s in waiting:
            out.append('<tr><td class="mono">%s</td><td>%s</td><td>%s</td><td>%s</td>'
                       '<td class="ip">%s</td><td>%s</td><td>%s</td></tr>'
                       % (e(s['code']), e(s['name']), e(s['version']),
                          e(s['host']['player']), e(s['host'].get('address') or '-'),
                          'yes' if s['password'] else '-', hms(s['waiting_s'])))
        if not waiting:
            out.append('<tr><td colspan="7" class="dim">none</td></tr>')
        out.append('</table></div>')

        def past(r, key):
            if not r.get(key):
                return side(None)
            return side({'player': r[key], 'address': r.get(key + '_address'),
                         'rtt_avg_ms': r.get(key + '_rtt_avg_ms'),
                         'rtt_max_ms': r.get(key + '_rtt_max_ms')})

        out.append('<h2>Last games</h2><div class="wrap"><table>'
                   '<tr class="grp"><th colspan="4" style="border-left:0"></th>'
                   '<th colspan="5">Player 1 (made the game)</th>'
                   '<th colspan="5">Player 2 (joined)</th>'
                   '<th style="border-left:2px solid #c3cbd3"></th></tr>'
                   '<tr><th>Ended</th><th>Game</th><th>Version</th><th>Played</th>'
                   + side_head + side_head + '<th class="sep">End</th></tr>')
        for r in st['recent'][:20]:
            out.append('<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td>%s%s'
                       '<td class="sep">%s</td></tr>'
                       % (e(r['ended']), e(r['name']), e(r['version']),
                          hms(r.get('played_s')), past(r, 'host'), past(r, 'guest'),
                          e(r.get('end') or 'server restarted')))
        if not st['recent']:
            out.append('<tr><td colspan="15" class="dim">none</td></tr>')
        out.append('</table></div>')

        out.append('<div class="head"><h2 id="reports">Bug reports</h2>'
                   '<a class="btn" href="/reports/all.zip">Download all (.zip)</a>'
                   '</div><div class="wrap"><table><tr><th>No.</th>'
                   '<th>Date</th><th>Version</th><th>Kind</th><th>E-mail</th>'
                   '<th>Text</th><th>Files</th><th>Status</th><th></th></tr>')
        reports = self.reports.listing(30)
        for r in reports:
            text = e(r.get('text', ''))
            folder = e(r.get('folder', ''))
            if len(r.get('text', '')) > 400:
                text = e(r.get('text', '')[:390]) + \
                    ' ... <a href="/reports/%s/">(all %d letters)</a>' % (
                        folder, len(r.get('text', '')))
            out.append('<tr><td>%s</td><td>%s %s</td><td>%s</td><td>%s</td><td>%s</td>'
                       '<td class="text">%s</td><td>%s</td><td>%s</td>'
                       '<td><a class="btn" href="/reports/%s/">Details</a>'
                       '<a class="btn" href="/reports/%s.zip">Download .zip</a>%s</td></tr>'
                       % (r.get('number'), e(r.get('date', '')), e(r.get('time', '')),
                          e(r.get('version', '')), e(r.get('kind', '')),
                          e(r.get('email', '')), text,
                          len(r.get('files', [])), e(r.get('status', '')),
                          folder, folder, self.delete_button(folder)))
        if not reports:
            out.append('<tr><td colspan="9" class="dim">none</td></tr>')
        out.append('</table></div>')
        out.append('</body></html>')
        return '\n'.join(out)
