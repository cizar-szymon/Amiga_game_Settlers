#!/usr/bin/env python3
"""Settlers game server: Amigas play over the internet through it (the
INTERNET GAME of the game's LAN box), bug reports from the game are kept on
its disk, a page shows the games (home network only).

  python3 settlers_server.py [config.json]

Everything it writes goes to data/ (config "data_dir") next to it.
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

from relay import Relay        # noqa: E402
from reports import Reports    # noqa: E402
from web import Web            # noqa: E402
from load import Load          # noqa: E402


def load_config(path=None):
    with open(path or os.path.join(HERE, 'config.json'), encoding='utf-8') as f:
        cfg = json.load(f)
    if not os.path.isabs(cfg['data_dir']):
        cfg['data_dir'] = os.path.join(HERE, cfg['data_dir'])
    return cfg


def setup_logging(cfg):
    os.makedirs(cfg['data_dir'], exist_ok=True)
    handler = logging.handlers.RotatingFileHandler(
        os.path.join(cfg['data_dir'], 'server.log'), maxBytes=2 << 20,
        backupCount=5, encoding='utf-8')
    handler.setFormatter(logging.Formatter('%(asctime)s %(name)s %(message)s'))
    root = logging.getLogger()
    root.setLevel(logging.INFO)
    root.addHandler(handler)
    root.addHandler(logging.StreamHandler())


async def start(cfg):
    """The servers (game port, page); returns them (tests stop them)."""
    reports = Reports(cfg)
    relay = Relay(cfg, reports)
    web = Web(cfg, relay, reports)
    relay.load = Load(relay)
    relay.load_task = asyncio.ensure_future(relay.load.run())
    game = await asyncio.start_server(relay.handle, cfg['listen'],
                                      cfg['game_port'])
    page = await asyncio.start_server(web.handle, cfg['listen'],
                                      cfg['web_port'])
    return relay, game, page


async def main(cfg):
    relay, game, page = await start(cfg)
    logging.info('Settlers server: games on %s:%d, page on port %d',
                 cfg['listen'], cfg['game_port'], cfg['web_port'])
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:      # Windows
            pass
    await stop.wait()
    relay.load_task.cancel()
    game.close()
    page.close()
    logging.info('stopped')


if __name__ == '__main__':
    config = load_config(sys.argv[1] if len(sys.argv) > 1 else None)
    setup_logging(config)
    try:
        asyncio.run(main(config))
    except KeyboardInterrupt:
        pass
