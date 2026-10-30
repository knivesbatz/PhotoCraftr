/*
 * Windows / OS/2 bitmap reader and writer.
 *
 * Reads 1/4/8/16/24/32-bit, BI_RGB, BI_RLE8, BI_RLE4, BI_BITFIELDS and
 * BI_ALPHABITFIELDS, bottom-up and top-down, with OS/2 1.x (12-byte),
 * Windows 3 (40-byte), OS/2 2.x and V2..V5 headers.  A 32-bit BI_RGB file
 * whose fourth bytes are not all zero is taken to carry alpha.
 *
 * Writes a plain BITMAPINFOHEADER (what Windows 95 understands): 24-bit,
 * 32-bit with alpha (bmp_depth 32), or an 8-bit gray palette for
 * Grayscale documents.
 */
#include "io/ioint.h"

typedef struct { u32 mask; int shift, bits; } BmpField;

static void field_init(BmpField *f, u32 m)
{
    f->mask = m; f->shift = 0; f->bits = 0;
    if (!m) return;
    while (!(m & 1)) { m >>= 1; f->shift++; }
    while (m & 1) { m >>= 1; f->bits++; }
}

static int field_get(const BmpField *f, u32 v)
{
    u32 x, maxv;
    if (!f->mask) return 0;
    x = (v & f->mask) >> f->shift;
    if (f->bits >= 32) return (int)(x >> 24);
    maxv = (f->bits >= 31) ? 0x7FFFFFFFu : ((1u << f->bits) - 1);
    if (f->bits > 16) return (int)(x >> (f->bits - 8));
    return (int)((x * 255u + maxv / 2) / maxv);
}

/* RLE8 / RLE4 into an index image (bottom-up row order as stored). */
static void bmp_rle(const u8 *src, size_t n, int rle4, u8 *idx, int w, int h)
{
    size_t i = 0;
    int x = 0, y = 0;
    while (i + 1 < n && y < h) {
        int cnt = src[i], val = src[i + 1];
        i += 2;
        if (cnt) {
            int k;
            for (k = 0; k < cnt && x < w; k++, x++)
                idx[(size_t)y * w + x] = (u8)(rle4 ? ((k & 1) ? (val & 15) : (val >> 4)) : val);
            continue;
        }
        if (val == 0) { x = 0; y++; }
        else if (val == 1) break;
        else if (val == 2) {
            if (i + 1 >= n) break;
            x += src[i]; y += src[i + 1];
            i += 2;
        } else {
            int k, bytes = rle4 ? (val + 1) / 2 : val;
            if ((size_t)bytes > n - i) bytes = (int)(n - i);
            for (k = 0; k < val; k++) {
                int b = rle4 ? k / 2 : k, v;
                if (b >= bytes) break;
                v = rle4 ? ((k & 1) ? (src[i + b] & 15) : (src[i + b] >> 4)) : src[i + b];
                if (x < w) idx[(size_t)y * w + x] = (u8)v;
                x++;
            }
            i += (size_t)((bytes + 1) & ~1);      /* word aligned */
        }
    }
}

PcDoc *pc_bmp_load(const u8 *data, size_t n, char *err, size_t errn)
{
    PcRd r;
    u32 off, hs, comp = 0, clrused = 0, masks[4] = { 0, 0, 0, 0 };
    long long w, h;
    int bpp, topdown = 0, npal = 0, palsz, i, gray, use_alpha = 0;
    double dpi = 72;
    u8 pal[256][4];
    const u8 *pix;
    size_t avail, stride, y, x;
    PcBuf *buf;
    u8 *idx = NULL;
    BmpField f[4];

    rd_init(&r, data, n);
    if (n < 26 || data[0] != 'B' || data[1] != 'M') { pcio_seterr(err, errn, "This is not a BMP file."); return NULL; }
    rd_skip(&r, 10);
    off = rd_u32le(&r);
    hs = rd_u32le(&r);
    if (hs == 12) {
        w = rd_u16le(&r); h = rd_u16le(&r);
        rd_skip(&r, 2);
        bpp = (int)rd_u16le(&r);
        palsz = 3;
    } else if (hs >= 16 && hs <= 1024) {
        w = (int)rd_u32le(&r);
        h = (int)rd_u32le(&r);
        rd_skip(&r, 2);
        bpp = (int)rd_u16le(&r);
        if (hs >= 20) comp = rd_u32le(&r);
        if (hs >= 40) {
            u32 xppm;
            rd_skip(&r, 4);
            xppm = rd_u32le(&r);
            rd_skip(&r, 4);
            clrused = rd_u32le(&r);
            rd_skip(&r, 4);
            if (xppm && xppm < 0x7FFFFFFF) dpi = pcio_dpi_from_ppm((double)xppm);
        }
        if (hs >= 52 && hs != 64) { masks[0] = rd_u32le(&r); masks[1] = rd_u32le(&r); masks[2] = rd_u32le(&r); }
        if (hs >= 56 && hs != 64) masks[3] = rd_u32le(&r);
        if (hs == 64 && comp >= 3) { pcio_seterr(err, errn, "OS/2 Huffman or RLE24 bitmaps are not supported."); return NULL; }
        palsz = 4;
    } else {
        pcio_seterr(err, errn, "Unsupported BMP header.");
        return NULL;
    }
    rd_seek(&r, 14 + (size_t)hs);
    if ((comp == 3 || comp == 6) && hs == 40) {
        masks[0] = rd_u32le(&r); masks[1] = rd_u32le(&r); masks[2] = rd_u32le(&r);
        if (comp == 6) masks[3] = rd_u32le(&r);
    }
    if (r.err) { pcio_seterr(err, errn, "The BMP file is truncated."); return NULL; }
    if (h < 0) { topdown = 1; h = -h; }
    if (!pcio_dims_ok(w, h)) { pcio_seterr(err, errn, "The image is too large (or has no pixels)."); return NULL; }
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) { pcio_seterr(err, errn, "Unsupported BMP bit depth (%d).", bpp); return NULL; }
    if (comp > 3 && comp != 6) { pcio_seterr(err, errn, "Unsupported BMP compression (%u).", (unsigned)comp); return NULL; }
    if ((comp == 1 && bpp != 8) || (comp == 2 && bpp != 4) || ((comp == 3 || comp == 6) && bpp != 16 && bpp != 32)) {
        pcio_seterr(err, errn, "The BMP file is damaged (compression does not match depth)."); return NULL;
    }
    /* palette */
    if (bpp <= 8) {
        npal = clrused ? (int)PC_MIN(clrused, 256u) : 1 << bpp;
        memset(pal, 0, sizeof pal);
        for (i = 0; i < npal; i++) {
            const u8 *e = rd_bytes(&r, (size_t)palsz);
            if (!e) { npal = i; break; }
            pal[i][0] = e[2]; pal[i][1] = e[1]; pal[i][2] = e[0];
        }
        r.err = 0;
        if (npal == 0) { pcio_seterr(err, errn, "The BMP colour table is missing."); return NULL; }
    }
    if (off < r.pos || off >= n) off = (u32)r.pos;          /* bad offset: data follows the palette */
    pix = data + off;
    avail = n - off;
    stride = (((size_t)w * bpp + 31) / 32) * 4;
    if (comp == 1 || comp == 2) {
        if (!pcio_plausible((int)w, (int)h, avail, 1024)) { pcio_seterr(err, errn, "The BMP file is truncated or damaged."); return NULL; }
    } else if ((unsigned long long)stride * (h - 1) + ((size_t)w * bpp + 7) / 8 > avail) {
        pcio_seterr(err, errn, "The BMP file is truncated.");
        return NULL;
    }
    buf = pc_buf_new((int)w, (int)h, 4, 1);
    if (!buf) { pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
    if (comp == 1 || comp == 2) {
        idx = (u8 *)pc_alloc_checked((size_t)w, (size_t)h, 1, 1);
        if (!idx) { pc_buf_unref(buf); pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
        bmp_rle(pix, avail, comp == 2, idx, (int)w, (int)h);
    }
    if (bpp == 16 || bpp == 32) {
        if (comp == 0) {
            if (bpp == 16) { masks[0] = 0x7C00; masks[1] = 0x03E0; masks[2] = 0x001F; masks[3] = 0; }
            else { masks[0] = 0xFF0000; masks[1] = 0xFF00; masks[2] = 0xFF; masks[3] = 0xFF000000u; }
        }
        for (i = 0; i < 4; i++) field_init(&f[i], masks[i]);
    }
    for (y = 0; y < (size_t)h; y++) {
        size_t sy = topdown ? y : (size_t)h - 1 - y;    /* stored row for output row y */
        const u8 *row = pix + sy * stride;
        u8 *o = buf->px + y * (size_t)w * 4;
        for (x = 0; x < (size_t)w; x++, o += 4) {
            int v;
            switch (bpp) {
            case 1: case 4: case 8:
                if (idx) v = idx[sy * (size_t)w + x];
                else if (bpp == 8) v = row[x];
                else if (bpp == 4) v = (row[x >> 1] >> ((x & 1) ? 0 : 4)) & 15;
                else v = (row[x >> 3] >> (7 - (x & 7))) & 1;
                if (v >= npal) v = 0;
                o[0] = pal[v][0]; o[1] = pal[v][1]; o[2] = pal[v][2]; o[3] = 255;
                break;
            case 24:
                o[0] = row[x * 3 + 2]; o[1] = row[x * 3 + 1]; o[2] = row[x * 3]; o[3] = 255;
                break;
            default: {
                u32 px = bpp == 16 ? get_u16le(row + x * 2) : get_u32le(row + x * 4);
                o[0] = (u8)field_get(&f[0], px); o[1] = (u8)field_get(&f[1], px); o[2] = (u8)field_get(&f[2], px);
                o[3] = (u8)(f[3].mask ? field_get(&f[3], px) : 255);
                if (f[3].mask && o[3]) use_alpha = 1;
            }
            }
        }
    }
    free(idx);
    /* an alpha mask that is all zero means "no alpha" (most 32-bit BMPs) */
    if ((bpp == 16 || bpp == 32) && f[3].mask && !use_alpha) {
        size_t k, np = (size_t)w * h;
        for (k = 0; k < np; k++) buf->px[k * 4 + 3] = 255;
    }
    gray = bpp <= 8;
    for (i = 0; gray && i < npal; i++) if (pal[i][0] != pal[i][1] || pal[i][1] != pal[i][2]) gray = 0;
    return pcio_doc_from_buf(buf, gray ? PC_MODE_GRAY : PC_MODE_RGB, dpi, PC_FMT_BMP, err, errn);
}

int pc_bmp_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcBuf *c;
    int W = d->s.w, H = d->s.h, gray = d->s.mode == PC_MODE_GRAY, bpp, x, y;
    size_t np = (size_t)W * H, stride, pal, imgsz;
    u32 ppm;
    PcWr w;
    memset(&w, 0, sizeof w);
    *out = NULL; *outn = 0;
    c = pcio_composite(d, err, errn);
    if (!c) return -1;
    bpp = gray ? 8 : ((o && o->bmp_depth == 32) ? 32 : 24);
    if (bpp != 32) pcio_matte_white(c->px, np);
    stride = (((size_t)W * bpp + 31) / 32) * 4;
    pal = gray ? 1024 : 0;
    imgsz = stride * H;
    ppm = pcio_uround(pcio_doc_dpi(d) / 0.0254);
    if (wr_reserve(&w, 54 + pal + imgsz)) { pc_buf_unref(c); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    wr_bytes(&w, "BM", 2);
    wr_u32le(&w, (u32)(54 + pal + imgsz));
    wr_u32le(&w, 0);
    wr_u32le(&w, (u32)(54 + pal));
    wr_u32le(&w, 40);
    wr_u32le(&w, (u32)W);
    wr_u32le(&w, (u32)H);              /* positive: bottom-up */
    wr_u16le(&w, 1);
    wr_u16le(&w, (unsigned)bpp);
    wr_u32le(&w, 0);                   /* BI_RGB */
    wr_u32le(&w, (u32)imgsz);
    wr_u32le(&w, ppm);
    wr_u32le(&w, ppm);
    wr_u32le(&w, gray ? 256 : 0);
    wr_u32le(&w, 0);
    if (gray) for (x = 0; x < 256; x++) { wr_u8(&w, (unsigned)x); wr_u8(&w, (unsigned)x); wr_u8(&w, (unsigned)x); wr_u8(&w, 0); }
    for (y = H - 1; y >= 0; y--) {
        const u8 *s = c->px + (size_t)y * W * 4;
        size_t start = w.n;
        for (x = 0; x < W; x++, s += 4) {
            if (bpp == 8) wr_u8(&w, s[0]);
            else { wr_u8(&w, s[2]); wr_u8(&w, s[1]); wr_u8(&w, s[0]); if (bpp == 32) wr_u8(&w, s[3]); }
        }
        wr_fill(&w, 0, stride - (w.n - start));
    }
    pc_buf_unref(c);
    return wr_finish(&w, out, outn, err, errn);
}
