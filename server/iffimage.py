"""IFF PBM (chunky 8-bit with a palette, as the Syndicate Wars port saves the
screen of a bug report) to PNG, with the standard library only: the report
page shows the screen in the browser.
"""
import struct
import zlib


def _chunks(data):
    pos = 12
    while pos + 8 <= len(data):
        name = data[pos:pos + 4]
        size = struct.unpack('>I', data[pos + 4:pos + 8])[0]
        yield name, data[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)


def _byterun1(src, size):
    out = bytearray()
    i = 0
    while len(out) < size and i < len(src):
        n = src[i]
        i += 1
        if n < 128:
            out += src[i:i + n + 1]
            i += n + 1
        elif n > 128:
            out += src[i:i + 1] * (257 - n)
            i += 1
    return bytes(out[:size])


def _png_chunk(kind, data):
    return (struct.pack('>I', len(data)) + kind + data
            + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF))


def pbm_to_png(data):
    """PNG bytes of an IFF PBM picture, or None when it is not one."""
    if data[:4] != b'FORM' or data[8:12] != b'PBM ':
        return None
    w = h = comp = 0
    pal = body = None
    for name, c in _chunks(data):
        if name == b'BMHD' and len(c) >= 11:
            w, h = struct.unpack('>HH', c[:4])
            comp = c[10]
        elif name == b'CMAP':
            pal = c
        elif name == b'BODY':
            body = c
    if not w or not h or body is None or w > 4096 or h > 4096:
        return None
    row = w + (w & 1)
    pix = _byterun1(body, row * h) if comp == 1 else body[:row * h]
    if len(pix) < row * h:
        pix += bytes(row * h - len(pix))
    raw = b''.join(b'\0' + pix[y * row:y * row + w] for y in range(h))
    pal = (pal or bytes(range(256)) * 3)[:768]
    pal += bytes((3 - len(pal) % 3) % 3)
    return (b'\x89PNG\r\n\x1a\n'
            + _png_chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0))
            + _png_chunk(b'PLTE', pal)
            + _png_chunk(b'IDAT', zlib.compress(raw, 6))
            + _png_chunk(b'IEND', b''))
