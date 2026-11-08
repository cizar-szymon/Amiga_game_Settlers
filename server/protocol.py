"""The frames of the Settlers LAN link (port/amiga_net.c) as the server sees
them: len (1 byte, 1 + data length) | type (1 byte) | data, at most 251
bytes. Link frames below 16, game frames from 16 on (passed on untouched).

Before HELLO a client talks to the server with F_RELAY text frames:
  client: LIST <proto> <version>
          CREATE <proto> <version> <player>\t<game name>[\t<password>]
          JOIN <proto> <version> <code> <player>[\t<password>]
          REPORT <proto> <version> <kind>
  server: SESSION <code> <waiting s> <version>\t<player>\t<game name>\t<P>
                                   (P: 1 when the game has a password)
          END                      (end of the list)
          CREATED <code>
          PAIRED <player of the other side>
          REPORTED <number>
          ERR <text>
A report goes on with F_RELAY "FIELD <key> <value>", "FILE <name> <size>",
the file in F_RELAY_DATA frames, and "DONE".
"""
import asyncio

F_HELLO = 1
F_BYE = 2
F_PING = 3
F_PONG = 4
F_RELAY = 5
F_RELAY_DATA = 6
GAME_BASE = 16

MAX_FRAME = 251         # the length byte: 1 + data
MAX_DATA = MAX_FRAME - 1
PROTOCOL = 1            # NET_PROTOCOL of port/amiga_net.h


class FrameError(Exception):
    """A frame the game never sends: the connection is dropped."""


def frame(ftype, data=b''):
    if isinstance(data, str):
        data = data.encode('latin-1', 'replace')
    if len(data) > MAX_DATA:
        raise FrameError('frame too long')
    return bytes([1 + len(data), ftype]) + data


async def read_frame(reader):
    """(type, data) of the next frame; EOF: asyncio.IncompleteReadError."""
    head = await reader.readexactly(1)
    n = head[0]
    if n == 0:
        raise FrameError('empty frame')
    body = await reader.readexactly(n)
    return body[0], body[1:]


def text(data):
    return data.decode('latin-1', 'replace')


def clean(s, length, extra=' -_.'):
    """Only letters, digits and a few signs (names shown on the Amigas and
    on the page)."""
    out = ''.join(c for c in s if c.isalnum() and ord(c) < 128 or c in extra)
    return out.strip()[:length]
