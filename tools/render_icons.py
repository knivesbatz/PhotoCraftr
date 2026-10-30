#!/usr/bin/env python3
"""Render every PhotoCraft Classic icon and cursor to a review contact sheet.

    python3 tools/render_icons.py [--out docs/images]

The C source (src/ui/icons_data.c) is the single source of truth: this
script parses its string-literal grids and tables with regular expressions,
checks that every grid is exactly w*h characters of the documented palette
(and fails loudly otherwise), then writes

    docs/images/icon-sheet.png     each icon at 1x and 4x, with labels
    docs/images/icon-sheet-4x.png  every icon at 1x on a button face, the
                                   whole sheet magnified 4x (nearest neighbour)

Cursors are drawn over a white/black split background so the white halo can
be judged; their hotspot is marked with a red square in the 4x view and the
'x' (inverted screen) pixels show as the inverse of the background.

Pillow is used when available; otherwise a minimal built-in PNG writer and
5x7 font are used, so the script has no hard dependency.
"""
import os
import re
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src', 'ui', 'icons_data.c')
HDR = os.path.join(ROOT, 'src', 'ui', 'icons.h')

PALETTE = {
    'k': 0x000000, 'm': 0x800000, 'G': 0x008000, 'o': 0x808000,
    'n': 0x000080, 'p': 0x800080, 't': 0x008080, 'g': 0xC0C0C0,
    'd': 0x808080, 'R': 0xFF0000, 'L': 0x00FF00, 'Y': 0xFFFF00,
    'B': 0x0000FF, 'F': 0xFF00FF, 'C': 0x00FFFF, 'w': 0xFFFFFF,
}
FACE = (0xC0, 0xC0, 0xC0)
CURSOR_CHARS = set('.kwx')


def die(msg):
    sys.stderr.write('render_icons: ERROR: %s\n' % msg)
    sys.exit(1)


# --------------------------------------------------------------------- parse
def parse_c(path=SRC):
    text = open(path).read()
    arrays = {}
    for m in re.finditer(r'static\s+const\s+char\s+(\w+)\s*\[\]\s*=\s*((?:"[^"\n]*"\s*)+);', text):
        arrays[m.group(1)] = ''.join(re.findall(r'"([^"\n]*)"', m.group(2)))
    icons = []
    for m in re.finditer(r'\[(PC_ICON_\w+)\]\s*=\s*\{\s*"([^"]*)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*\}', text):
        icons.append(dict(id=m.group(1), name=m.group(2), w=int(m.group(3)), h=int(m.group(4)),
                          arr=m.group(5), hot=None))
    cursors = []
    for m in re.finditer(r'\[(PC_CURSOR_\w+)\]\s*=\s*\{\s*"([^"]*)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,'
                         r'\s*(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*\}', text):
        cursors.append(dict(id=m.group(1), name=m.group(2), w=int(m.group(3)), h=int(m.group(4)),
                            hot=(int(m.group(5)), int(m.group(6))), arr=m.group(7)))
    return arrays, icons, cursors


def enum_ids(prefix, path=HDR):
    text = open(path).read()
    m = re.search(r'enum\s+%s\s*\{(.*?)\}' % prefix, text, re.S)
    if not m:
        die('enum %s not found in %s' % (prefix, path))
    body = re.sub(r'/\*.*?\*/', '', m.group(1), flags=re.S)
    return [t.strip() for t in body.split(',') if t.strip() and not t.strip().endswith('_COUNT')]


def validate(arrays, icons, cursors):
    errors = []
    for kind, items, chars, enum in (('icon', icons, set(PALETTE) | {'.'}, 'PcIconId'),
                                     ('cursor', cursors, CURSOR_CHARS, 'PcCursorId')):
        ids = enum_ids(enum)
        got = [it['id'] for it in items]
        if got != ids:
            errors.append('%s table order/ids differ from enum %s (missing %s, extra %s)' % (
                kind, enum, sorted(set(ids) - set(got)), sorted(set(got) - set(ids))))
        for it in items:
            px = arrays.get(it['arr'])
            if px is None:
                errors.append('%s %s: array %s not found' % (kind, it['id'], it['arr']))
                continue
            if len(px) != it['w'] * it['h']:
                errors.append('%s %s: %d chars, expected %dx%d=%d' % (
                    kind, it['id'], len(px), it['w'], it['h'], it['w'] * it['h']))
                continue
            bad = set(px) - chars
            if bad:
                errors.append('%s %s: unknown chars %s' % (kind, it['id'], ''.join(sorted(bad))))
            if kind == 'cursor':
                if (it['w'], it['h']) != (32, 32):
                    errors.append('cursor %s is not 32x32' % it['id'])
                hx, hy = it['hot']
                if not (0 <= hx < it['w'] and 0 <= hy < it['h']):
                    errors.append('cursor %s hotspot out of range' % it['id'])
            it['px'] = px
    if errors:
        die('\n  '.join(['art check failed:'] + errors))


# --------------------------------------------------------------------- canvas
class Canvas:
    def __init__(self, w, h, bg=(0xFF, 0xFF, 0xFF)):
        self.w, self.h = w, h
        self.px = bytearray(bytes(bg) * (w * h))

    def put(self, x, y, rgb):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 3
            self.px[i:i + 3] = bytes(rgb)

    def get(self, x, y):
        i = (y * self.w + x) * 3
        return tuple(self.px[i:i + 3])

    def fill(self, x0, y0, w, h, rgb):
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                self.put(x, y, rgb)

    def frame(self, x0, y0, w, h, rgb):
        for x in range(x0, x0 + w):
            self.put(x, y0, rgb)
            self.put(x, y0 + h - 1, rgb)
        for y in range(y0, y0 + h):
            self.put(x0, y, rgb)
            self.put(x0 + w - 1, y, rgb)

    def bevel(self, x0, y0, w, h, sunken=False):
        """Windows 95 style 3D box around a face-coloured area."""
        hi, lo = (0xFF, 0xFF, 0xFF), (0x80, 0x80, 0x80)
        if sunken:
            hi, lo = lo, hi
        self.fill(x0, y0, w, h, FACE)
        for x in range(x0, x0 + w):
            self.put(x, y0, hi)
            self.put(x, y0 + h - 1, lo)
        for y in range(y0, y0 + h):
            self.put(x0, y, hi)
            self.put(x0 + w - 1, y, lo)

    def art(self, x0, y0, w, h, px, scale=1, cursor=False):
        for y in range(h):
            for x in range(w):
                ch = px[y * w + x]
                if ch == '.':
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        X, Y = x0 + x * scale + sx, y0 + y * scale + sy
                        if not (0 <= X < self.w and 0 <= Y < self.h):
                            continue
                        if ch == 'x':
                            r, g, b = self.get(X, Y)
                            self.put(X, Y, (255 - r, 255 - g, 255 - b))
                        else:
                            v = PALETTE[ch]
                            self.put(X, Y, (v >> 16, (v >> 8) & 255, v & 255))

    def scaled(self, s):
        out = Canvas(self.w * s, self.h * s)
        for y in range(self.h):
            row = bytearray()
            for x in range(self.w):
                row += bytes(self.get(x, y)) * s
            for sy in range(s):
                i = (y * s + sy) * out.w * 3
                out.px[i:i + len(row)] = row
        return out


# --------------------------------------------------------------------- text
# Tiny 5x7 fallback font (only used without Pillow): digits, A-Z, '_', '-', '(', ')', 'x', '/'.
_F = {
    'A': '01110100011000111111100011000110001', 'B': '11110100011000111110100011000111110',
    'C': '01110100011000010000100001000101110', 'D': '11110100011000110001100011000111110',
    'E': '11111100001000011110100001000011111', 'F': '11111100001000011110100001000010000',
    'G': '01110100011000010111100011000101111', 'H': '10001100011000111111100011000110001',
    'I': '01110001000010000100001000010001110', 'J': '00111000100001000010000101001001100',
    'K': '10001100101010011000101001001010001', 'L': '10000100001000010000100001000011111',
    'M': '10001110111010110101100011000110001', 'N': '10001110011010110011100011000110001',
    'O': '01110100011000110001100011000101110', 'P': '11110100011000111110100001000010000',
    'Q': '01110100011000110001101011001001101', 'R': '11110100011000111110101001001010001',
    'S': '01111100001000001110000010000111110', 'T': '11111001000010000100001000010000100',
    'U': '10001100011000110001100011000101110', 'V': '10001100011000110001100010101000100',
    'W': '10001100011000110101101011010101010', 'X': '10001100010101000100010101000110001',
    'Y': '10001100010101000100001000010000100', 'Z': '11111000010001000100010001000011111',
    '0': '01110100011001110101110011000101110', '1': '00100011000010000100001000010001110',
    '2': '01110100010000100010001000100011111', '3': '11111000100010000010000011000101110',
    '4': '00010001100101010010111110001000010', '5': '11111100001111000001000011000101110',
    '6': '00110010001000011110100011000101110', '7': '11111000010001000100010000100001000',
    '8': '01110100011000101110100011000101110', '9': '01110100011000101111000010001001100',
    '_': '00000000000000000000000000000011111', '-': '00000000000000011111000000000000000',
    '(': '00010001000100001000010000010000010', ')': '01000001000001000010000100010001000',
    '/': '00000000010001000100010001000000000', ' ': '0' * 35, ',': '00000000000000000000011000010001000',
}


class Text:
    def __init__(self):
        try:
            from PIL import ImageFont
            self.font = ImageFont.load_default()
        except Exception:
            self.font = None
        self.h = 11 if self.font else 8

    def width(self, s):
        if self.font:
            try:
                return int(self.font.getlength(s))
            except Exception:
                return len(s) * 6
        return len(s) * 6

    def draw(self, cv, x, y, s, rgb=(0, 0, 0)):
        if self.font:
            from PIL import Image, ImageDraw
            w, h = max(1, self.width(s) + 2), self.h + 2
            im = Image.new('L', (w, h), 0)
            ImageDraw.Draw(im).text((0, 0), s, fill=255, font=self.font)
            data = im.tobytes()
            for j in range(h):
                for i in range(w):
                    if data[j * w + i] > 127:
                        cv.put(x + i, y + j, rgb)
            return
        for k, ch in enumerate(s.upper()):
            bits = _F.get(ch, _F[' '])
            for j in range(7):
                for i in range(5):
                    if bits[j * 5 + i] == '1':
                        cv.put(x + k * 6 + i, y + j, rgb)


def save_png(cv, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    try:
        from PIL import Image
        Image.frombytes('RGB', (cv.w, cv.h), bytes(cv.px)).save(path, optimize=True)
        return
    except ImportError:
        pass

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b''.join(b'\0' + bytes(cv.px[y * cv.w * 3:(y + 1) * cv.w * 3]) for y in range(cv.h))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', cv.w, cv.h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


# --------------------------------------------------------------------- sheets
def cursor_bg(cv, x0, y0, w, h):
    """White top half, near-black bottom half: shows the halo on both."""
    cv.fill(x0, y0, w, h // 2, (0xFF, 0xFF, 0xFF))
    cv.fill(x0, y0 + h // 2, w, h - h // 2, (0x20, 0x20, 0x20))


def group_of(icon_id):
    n = icon_id[len('PC_ICON_'):]
    if n.startswith('TOOL_'):
        return 'Tools'
    return None


def detailed_sheet(icons, cursors, txt):
    """1x + 4x of every icon / cursor with its enum id."""
    S = 4
    cols = 6
    cell_w = 4 + 34 + 32 * S + 8
    label_w = 230
    cell_w = max(cell_w, label_w)
    entries = [('icon', it) for it in icons] + [('cursor', it) for it in cursors]

    def cell_h(it):
        return txt.h + 6 + max(it['h'] * S, 34) + 8

    rows = []
    for i in range(0, len(entries), cols):
        rows.append(entries[i:i + cols])
    title_h = 24
    H = title_h + sum(max(cell_h(it) for _, it in r) for r in rows) + 8
    W = cols * cell_w + 8
    cv = Canvas(W, H, FACE)
    txt.draw(cv, 6, 6, 'PhotoCraft Classic - %d icons, %d cursors (1x and 4x). '
             'Cursors: hotspot = red box.' % (len(icons), len(cursors)))
    y = title_h
    for r in rows:
        rh = max(cell_h(it) for _, it in r)
        for ci, (kind, it) in enumerate(r):
            x = 4 + ci * cell_w
            label = it['id'].replace('PC_ICON_', '').replace('PC_CURSOR_', 'CURSOR_')
            txt.draw(cv, x, y, '%s %dx%d' % (label, it['w'], it['h']))
            ay = y + txt.h + 4
            w, h = it['w'], it['h']
            if kind == 'icon':
                # 1x on a raised button, 4x on a plain face
                bw, bh = max(w, 16) + 6, max(h, 16) + 6
                cv.bevel(x, ay, bw, bh)
                cv.art(x + (bw - w) // 2, ay + (bh - h) // 2, w, h, it['px'])
                cv.frame(x + 33, ay - 1, w * S + 2, h * S + 2, (0x80, 0x80, 0x80))
                cv.art(x + 34, ay, w, h, it['px'], S)
            else:
                cursor_bg(cv, x, ay, 32, 32)
                cv.art(x, ay, w, h, it['px'])
                cursor_bg(cv, x + 34, ay, w * S, h * S)
                cv.art(x + 34, ay, w, h, it['px'], S)
                hx, hy = it['hot']
                cv.frame(x + 34 + hx * S - 1, ay + hy * S - 1, S + 2, S + 2, (0xFF, 0, 0))
        y += rh
    return cv


def compact_sheet(icons, cursors):
    """All art at 1x, icons on raised buttons, cursors on split tiles."""
    pad = 2
    cols = 16
    cell = 36
    n_icon_rows = (len(icons) + cols - 1) // cols
    n_cur_rows = (len(cursors) + cols - 1) // cols
    W = cols * cell + 2 * pad
    H = (n_icon_rows + n_cur_rows) * cell + 2 * pad + 4
    cv = Canvas(W, H, FACE)
    for i, it in enumerate(icons):
        x = pad + (i % cols) * cell
        y = pad + (i // cols) * cell
        w, h = it['w'], it['h']
        if w <= 16 and h <= 16:
            cv.bevel(x + 8, y + 8, 22, 22)
            cv.art(x + 8 + (22 - w) // 2, y + 8 + (22 - h) // 2, w, h, it['px'])
        else:
            cv.art(x + (cell - w) // 2, y + (cell - h) // 2, w, h, it['px'])
    y0 = pad + n_icon_rows * cell + 4
    for i, it in enumerate(cursors):
        x = pad + (i % cols) * cell + 2
        y = y0 + (i // cols) * cell + 2
        cursor_bg(cv, x, y, 32, 32)
        cv.art(x, y, 32, 32, it['px'])
    return cv


def main(argv):
    out = os.path.join(ROOT, 'docs', 'images')
    if '--out' in argv:
        out = argv[argv.index('--out') + 1]
    arrays, icons, cursors = parse_c()
    if not icons or not cursors:
        die('no icon/cursor tables found in %s' % SRC)
    validate(arrays, icons, cursors)
    txt = Text()
    save_png(detailed_sheet(icons, cursors, txt), os.path.join(out, 'icon-sheet.png'))
    save_png(compact_sheet(icons, cursors).scaled(4), os.path.join(out, 'icon-sheet-4x.png'))
    print('render_icons: %d icons, %d cursors OK -> %s' % (len(icons), len(cursors), out))


if __name__ == '__main__':
    main(sys.argv[1:])
