"""The frames on the wire, as the games send them (Settlers port/amiga_net.c,
the same as the game server's server/protocol.py - a copy, so that this
server needs nothing of the game server): len (1 byte, 1 + data length) |
type (1 byte) | data, at most 251 bytes.
"""

F_RELAY = 5             # text
F_RELAY_DATA = 6        # bytes of a file
MAX_FRAME = 251
MAX_DATA = MAX_FRAME - 1
PROTOCOL = 1


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
