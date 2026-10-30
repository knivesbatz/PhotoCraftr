#!/usr/bin/env python3
"""Build the Windows .ico files from the ASCII art in res/icons/*.txt.

    python3 tools/make_ico.py                  # writes res/{app,doc,update,setup}.ico
    python3 tools/make_ico.py --preview x.png  # ...and renders the written files back

Each res/icons/<name>-<size>.txt holds one image (16 or 32 pixels square)
as rows of palette characters; lines starting with '#' are comments.  Every
<name> gets one .ico with a 32x32 and a 16x16 entry, each stored as a
classic 4-bit (16-colour) BMP-format icon image with a 1-bit AND mask -- the
format Windows 95 understands (no PNG-compressed entries, no alpha).

Palette characters (Windows 95 16-colour VGA palette):
  '.' transparent   'k' black   'm' maroon  'G' green  'o' olive  'n' navy
  'p' purple        't' teal    'g' silver  'd' gray   'R' red    'L' lime
  'Y' yellow        'B' blue    'F' fuchsia 'C' aqua   'w' white
"""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART_DIR = os.path.join(ROOT, 'res', 'icons')
OUT_DIR = os.path.join(ROOT, 'res')
NAMES = ('app', 'doc', 'update', 'setup')
SIZES = (32, 16)

# Standard VGA palette order (index -> RGB) and the art characters for it.
VGA = [(0x00, 0x00, 0x00), (0x80, 0x00, 0x00), (0x00, 0x80, 0x00), (0x80, 0x80, 0x00),
       (0x00, 0x00, 0x80), (0x80, 0x00, 0x80), (0x00, 0x80, 0x80), (0xC0, 0xC0, 0xC0),
       (0x80, 0x80, 0x80), (0xFF, 0x00, 0x00), (0x00, 0xFF, 0x00), (0xFF, 0xFF, 0x00),
       (0x00, 0x00, 0xFF), (0xFF, 0x00, 0xFF), (0x00, 0xFF, 0xFF), (0xFF, 0xFF, 0xFF)]
CHARS = 'kmGonptgdRLYBFCw'   # index order: black maroon green olive navy purple teal silver gray red lime yellow blue fuchsia aqua white
INDEX = {c: i for i, c in enumerate(CHARS)}


def die(msg):
    sys.stderr.write('make_ico: ERROR: %s\n' % msg)
    sys.exit(1)


def read_art(path, size):
    rows = []
    for line in open(path):
        line = line.rstrip('\r\n')
        if not line or line.startswith('#'):
            continue
        rows.append(line)
    if len(rows) != size or any(len(r) != size for r in rows):
        die('%s: expected %dx%d, got %d rows of widths %s' % (
            path, size, size, len(rows), sorted({len(r) for r in rows})))
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch != '.' and ch not in INDEX:
                die('%s:%d:%d: unknown palette char %r' % (path, y + 1, x + 1, ch))
    return rows


def icon_image(rows):
    """BITMAPINFOHEADER + palette + XOR (4bpp) + AND (1bpp), bottom-up."""
    n = len(rows)
    xor_stride = ((n * 4 + 31) // 32) * 4
    and_stride = ((n + 31) // 32) * 4
    xor = bytearray()
    andm = bytearray()
    for r in reversed(rows):
        line = bytearray(xor_stride)
        mask = bytearray(and_stride)
        for x, ch in enumerate(r):
            idx = 0 if ch == '.' else INDEX[ch]
            if x & 1:
                line[x >> 1] |= idx
            else:
                line[x >> 1] |= idx << 4
            if ch == '.':
                mask[x >> 3] |= 0x80 >> (x & 7)
        xor += line
        andm += mask
    hdr = struct.pack('<IiiHHIIiiII', 40, n, n * 2, 1, 4, 0, len(xor) + len(andm), 0, 0, 16, 0)
    pal = b''.join(struct.pack('<BBBB', b, g, r, 0) for (r, g, b) in VGA)
    return hdr + pal + bytes(xor) + bytes(andm)


def build_ico(images):
    """images: list of (size, bytes)."""
    out = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images)
    data = b''
    for size, img in images:
        out += struct.pack('<BBBBHHII', size, size, 16, 0, 1, 4, len(img), offset + len(data))
        data += img
    return out + data


# ------------------------------------------------------------------ read back
def decode_ico(blob):
    """Independent decoder used to verify the files: returns [(size, rows)]."""
    reserved, typ, count = struct.unpack_from('<HHH', blob, 0)
    if reserved != 0 or typ != 1:
        raise ValueError('not an icon file')
    out = []
    for i in range(count):
        w, h, colors, _r, planes, bpp, size, off = struct.unpack_from('<BBBBHHII', blob, 6 + 16 * i)
        if blob[off:off + 8] == b'\x89PNG\r\n\x1a\n':
            raise ValueError('PNG entry found (not allowed for Windows 95)')
        (bsize, bw, bh, bplanes, bbpp, comp) = struct.unpack_from('<IiiHHI', blob, off)
        if bsize != 40 or bbpp != 4 or comp != 0 or bh != 2 * bw:
            raise ValueError('unexpected entry format: %r' % ((bsize, bw, bh, bbpp, comp),))
        pal_off = off + 40
        pal = [struct.unpack_from('<BBBB', blob, pal_off + 4 * j)[:3][::-1] for j in range(16)]
        xs = ((bw * 4 + 31) // 32) * 4
        ms = ((bw + 31) // 32) * 4
        xo = pal_off + 64
        mo = xo + xs * bw
        rows = []
        for y in range(bw):
            sy = bw - 1 - y
            row = []
            for x in range(bw):
                transparent = blob[mo + sy * ms + (x >> 3)] & (0x80 >> (x & 7))
                b = blob[xo + sy * xs + (x >> 1)]
                idx = (b & 0x0F) if x & 1 else (b >> 4)
                row.append(None if transparent else pal[idx])
            rows.append(row)
        out.append((bw, rows))
    return out


def preview(paths, png):
    S = 4
    decoded = [(os.path.basename(p), decode_ico(open(p, 'rb').read())) for p in paths]
    W = len(decoded) * (32 * S + 32 + 24) + 8
    H = 32 * S + 16 + 16
    face = (0xC0, 0xC0, 0xC0)
    px = [[face] * W for _ in range(H)]
    x0 = 8
    for _name, entries in decoded:
        for size, rows in entries:
            scale = S if size == 32 else 1
            ox = x0 if size == 32 else x0 + 32 * S + 8
            for y in range(size):
                for x in range(size):
                    c = rows[y][x]
                    if c is None:
                        continue
                    for sy in range(scale):
                        for sx in range(scale):
                            px[8 + y * scale + sy][ox + x * scale + sx] = c
        x0 += 32 * S + 32 + 24
    try:
        from PIL import Image
        im = Image.new('RGB', (W, H))
        im.putdata([c for r in px for c in r])
        im.save(png)
    except ImportError:
        def chunk(t, d):
            return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
        raw = b''.join(b'\0' + bytes(v for c in r for v in c) for r in px)
        with open(png, 'wb') as f:
            f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) +
                    chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def main(argv):
    written = []
    for name in NAMES:
        images = []
        for size in SIZES:
            path = os.path.join(ART_DIR, '%s-%d.txt' % (name, size))
            if not os.path.exists(path):
                die('missing %s' % path)
            images.append((size, icon_image(read_art(path, size))))
        out = os.path.join(OUT_DIR, name + '.ico')
        blob = build_ico(images)
        # round-trip check before writing
        back = decode_ico(blob)
        for (size, _img), (bsize, rows) in zip(images, back):
            art = read_art(os.path.join(ART_DIR, '%s-%d.txt' % (name, size)), size)
            for y in range(size):
                for x in range(size):
                    ch = art[y][x]
                    want = None if ch == '.' else VGA[INDEX[ch]]
                    if rows[y][x] != want:
                        die('%s: round-trip mismatch at %d,%d (%dx%d)' % (out, x, y, size, size))
        with open(out, 'wb') as f:
            f.write(blob)
        written.append(out)
        print('make_ico: wrote %s (%d bytes: 32x32 + 16x16, 4-bit + AND mask)' % (
            os.path.relpath(out, ROOT), len(blob)))
    if '--preview' in argv:
        png = argv[argv.index('--preview') + 1]
        preview(written, png)
        print('make_ico: preview %s' % png)


if __name__ == '__main__':
    main(sys.argv[1:])
