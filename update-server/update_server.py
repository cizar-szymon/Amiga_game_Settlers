#!/usr/bin/env python3
"""The update server of the games (the data server - a Raspberry Pi of its
own, not the game server): the newest releases on GitHub, sent to the games
file by file (updates.py has the protocol).

  python3 update_server.py [config.json]

Everything it writes goes to data/ (config "data_dir") next to it. Nothing
of the game server is used or changed.
"""
import asyncio
import json
import logging
import logging.handlers
import os
import signal
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import frames as P             # noqa: E402
from updates import Updates    # noqa: E402

log = logging.getLogger('server')


def load_config(path=None):
    with open(path or os.path.join(HERE, 'config.json'), encoding='utf-8') as f:
        cfg = json.load(f)
    if not os.path.isabs(cfg['data_dir']):
        cfg['data_dir'] = os.path.join(HERE, cfg['data_dir'])
    return cfg


def setup_logging(cfg):
    os.makedirs(cfg['data_dir'], exist_ok=True)
    handler = logging.handlers.RotatingFileHandler(
        os.path.join(cfg['data_dir'], 'update-server.log'), maxBytes=2 << 20,
        backupCount=3, encoding='utf-8')
    handler.setFormatter(logging.Formatter('%(asctime)s %(name)s %(message)s'))
    root = logging.getLogger()
    root.setLevel(logging.INFO)
    root.addHandler(handler)
    root.addHandler(logging.StreamHandler())


class Server:
    def __init__(self, cfg):
        self.cfg = cfg
        self.updates = Updates(cfg)
        self.connections = 0
        self.per_address = {}

    @staticmethod
    async def say(writer, data):
        try:
            writer.write(data)
            await writer.drain()
        except (ConnectionError, OSError):
            pass

    async def handle(self, reader, writer):
        peer = writer.get_extra_info('peername') or ('?', 0)
        address = peer[0]
        if (self.connections >= self.cfg['max_connections'] or
                self.per_address.get(address, 0) >= self.cfg['max_per_address']):
            await self.say(writer, P.frame(P.F_RELAY, 'ERR Too many connections.'))
            writer.close()
            return
        self.connections += 1
        self.per_address[address] = self.per_address.get(address, 0) + 1
        try:
            ftype, data = await asyncio.wait_for(P.read_frame(reader),
                                                 self.cfg['first_frame_s'])
            words = P.text(data).split(' ', 3)
            if ftype != P.F_RELAY or len(words) < 3 or words[0] != 'UPDATE':
                await self.say(writer, P.frame(P.F_RELAY, 'ERR Unknown command.'))
            elif words[1] != str(P.PROTOCOL):
                await self.say(writer, P.frame(P.F_RELAY, 'ERR Different version.'))
            else:
                version = ''.join(c for c in words[2] if c.isalnum() or c in '.-_')[:20]
                await self.updates.serve(reader, writer, version,
                                         words[3] if len(words) > 3 else '',
                                         self.say)
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


async def start(cfg):
    """The server (tests stop it); the check of GitHub runs unless check_s
    is 0."""
    server = Server(cfg)
    if cfg.get('check_s', 3600) > 0:
        server.task = asyncio.ensure_future(server.updates.run())
    listening = await asyncio.start_server(server.handle, cfg['listen'],
                                           cfg['port'])
    return server, listening


async def main(cfg):
    server, listening = await start(cfg)
    log.info('update server on %s:%d', cfg['listen'], cfg['port'])
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:      # Windows
            pass
    await stop.wait()
    listening.close()
    log.info('stopped')


if __name__ == '__main__':
    config = load_config(sys.argv[1] if len(sys.argv) > 1 else None)
    setup_logging(config)
    try:
        asyncio.run(main(config))
    except KeyboardInterrupt:
        pass
