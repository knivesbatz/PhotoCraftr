/*
 * Shared codec helpers: growable output buffer, document construction,
 * compositing for flat formats, CRC-32, PackBits and code page conversion.
 */
#include "io/ioint.h"

/* ---------------- writer ---------------- */

int wr_reserve(PcWr *w, size_t add)
{
    size_t need, nc;
    u8 *np;
    if (w->err) return -1;
    if (add > (size_t)-1 - w->n) { w->err = 1; return -1; }
    need = w->n + add;
    if (need <= w->cap) return 0;
    nc = w->cap ? w->cap : 4096;
    while (nc < need) {
        if (nc > (size_t)-1 / 2) { nc = need; break; }
        nc *= 2;
    }
    np = (u8 *)realloc(w->p, nc);
    if (!np) { w->err = 1; return -1; }
    w->p = np;
    w->cap = nc;
    return 0;
}

void wr_bytes(PcWr *w, const void *src, size_t k)
{
    if (!k || wr_reserve(w, k)) return;
    memcpy(w->p + w->n, src, k);
    w->n += k;
}

void wr_fill(PcWr *w, int byte, size_t k)
{
    if (!k || wr_reserve(w, k)) return;
    memset(w->p + w->n, byte, k);
    w->n += k;
}

void wr_u16be(PcWr *w, unsigned v) { u8 b[2]; b[0] = (u8)(v >> 8); b[1] = (u8)v; wr_bytes(w, b, 2); }
void wr_u16le(PcWr *w, unsigned v) { u8 b[2]; b[0] = (u8)v; b[1] = (u8)(v >> 8); wr_bytes(w, b, 2); }
void wr_u32be(PcWr *w, u32 v) { u8 b[4]; b[0] = (u8)(v >> 24); b[1] = (u8)(v >> 16); b[2] = (u8)(v >> 8); b[3] = (u8)v; wr_bytes(w, b, 4); }
void wr_u32le(PcWr *w, u32 v) { u8 b[4]; b[0] = (u8)v; b[1] = (u8)(v >> 8); b[2] = (u8)(v >> 16); b[3] = (u8)(v >> 24); wr_bytes(w, b, 4); }

void wr_put_u32be(PcWr *w, size_t at, u32 v)
{
    if (w->err || at + 4 > w->n) return;
    w->p[at] = (u8)(v >> 24); w->p[at + 1] = (u8)(v >> 16); w->p[at + 2] = (u8)(v >> 8); w->p[at + 3] = (u8)v;
}

void wr_put_u32le(PcWr *w, size_t at, u32 v)
{
    if (w->err || at + 4 > w->n) return;
    w->p[at] = (u8)v; w->p[at + 1] = (u8)(v >> 8); w->p[at + 2] = (u8)(v >> 16); w->p[at + 3] = (u8)(v >> 24);
}

void wr_free(PcWr *w) { free(w->p); w->p = NULL; w->n = w->cap = 0; }

int wr_finish(PcWr *w, u8 **out, size_t *outn, char *err, size_t errn)
{
    if (w->err || !w->p) {
        wr_free(w);
        pcio_seterr(err, errn, "Not enough memory to save the file.");
        return -1;
    }
    *out = w->p;
    *outn = w->n;
    w->p = NULL; w->n = w->cap = 0;
    return 0;
}

/* ---------------- errors and limits ---------------- */

void pcio_seterr(char *err, size_t errn, const char *fmt, ...)
{
    va_list ap;
    if (!err || !errn) return;
    va_start(ap, fmt);
    pc_vsnprintf(err, errn, fmt, ap);
    va_end(ap);
}

int pcio_dims_ok(long long w, long long h)
{
    if (w < 1 || h < 1 || w > PC_MAX_DIM || h > PC_MAX_DIM) return 0;
    return (unsigned long long)w * (unsigned long long)h <= (unsigned long long)PC_MAX_PIXELS;
}

/* ---------------- documents ---------------- */

PcDoc *pcio_doc_alloc(int w, int h, int mode, double dpi)
{
    /* pc_doc_new allocates a canvas-sized layer; start from 1x1 so loading
     * a large image does not need twice its memory, then resize the empty
     * document. pcio_finish() resets the history afterwards. */
    PcDoc *d;
    if (!pcio_dims_ok(w, h)) return NULL;
    d = pc_doc_new(1, 1, mode, dpi > 0 ? dpi : 72, 2, 0);
    if (!d) return NULL;
    pc_layer_free(d->s.layers[0]);
    d->s.layers[0] = NULL;
    d->s.nlayers = 0;
    d->s.active = 0;
    d->s.w = w;
    d->s.h = h;
    pc_hist_reset(d, "Open");
    return d;
}

PcBuf *pcio_buf_wrap(u8 *px, int w, int h)
{
    PcBuf *b;
    if (!px) return NULL;
    b = (PcBuf *)calloc(1, sizeof *b);
    if (!b) { free(px); return NULL; }
    b->refs = 1; b->w = w; b->h = h; b->ch = 4; b->px = px;
    return b;
}

int pcio_plausible(int w, int h, size_t n, unsigned long long px_per_byte)
{
    return (unsigned long long)w * (unsigned long long)h <= (unsigned long long)n * px_per_byte + 65536ull;
}

u32 pcio_uround(double v)
{
    if (!(v > 0)) return 0;
    if (v >= 4294967295.0) return 0xFFFFFFFFu;
    return (u32)(v + 0.5);
}

double pcio_dpi_from_ppm(double ppm)
{
    double dpi = ppm * 0.0254, r;
    if (!(dpi >= 1 && dpi <= 100000)) return 72;
    r = (double)pcio_uround(dpi);
    return fabs(dpi - r) < 0.1 ? r : dpi;
}

PcLayer *pcio_layer_from_buf(PcBuf *buf, const char *name)
{
    PcLayer *l = pc_layer_new(1, 1, name);
    if (!l) { pc_buf_unref(buf); return NULL; }
    pc_buf_unref(l->pix);
    l->pix = buf;
    return l;
}

void pcio_finish(PcDoc *d, int fmt)
{
    if (d->s.nlayers > 0 && (d->s.active < 0 || d->s.active >= d->s.nlayers)) d->s.active = d->s.nlayers - 1;
    pc_hist_reset(d, "Open");
    d->dirty = 0;
    d->format = fmt;
}

int pcio_has_alpha(const u8 *rgba, size_t npx)
{
    size_t i;
    for (i = 0; i < npx; i++) if (rgba[i * 4 + 3] != 255) return 1;
    return 0;
}

PcDoc *pcio_doc_from_buf(PcBuf *buf, int mode, double dpi, int fmt, char *err, size_t errn)
{
    PcDoc *d;
    PcLayer *l;
    int opaque;
    size_t i, n;
    if (!buf) { pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
    n = (size_t)buf->w * buf->h;
    if (mode == PC_MODE_GRAY) {
        u8 *p = buf->px;
        for (i = 0; i < n; i++, p += 4) p[1] = p[2] = p[0];
    }
    opaque = !pcio_has_alpha(buf->px, n);
    d = pcio_doc_alloc(buf->w, buf->h, mode, dpi);
    if (!d) { pc_buf_unref(buf); pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
    l = pcio_layer_from_buf(buf, opaque ? "Background" : "Layer 0");
    if (!l) { pc_doc_free(d); pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
    l->is_background = opaque;
    if (pc_doc_insert_layer(d, 0, l)) { pc_doc_free(d); pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
    pcio_finish(d, fmt);
    return d;
}

PcBuf *pcio_composite(const PcDoc *d, char *err, size_t errn)
{
    PcBuf *b = pc_doc_composite(d);
    if (!b) { pcio_seterr(err, errn, "Not enough memory to save the file."); return NULL; }
    if (d->s.mode == PC_MODE_GRAY) {
        size_t i, n = (size_t)b->w * b->h;
        u8 *p = b->px;
        for (i = 0; i < n; i++, p += 4) p[0] = p[1] = p[2] = (u8)pcio_gray_of(p);
    }
    return b;
}

void pcio_matte_white(u8 *rgba, size_t npx)
{
    size_t i;
    for (i = 0; i < npx; i++, rgba += 4) {
        int a = rgba[3], k;
        if (a == 255) continue;
        for (k = 0; k < 3; k++) rgba[k] = (u8)((rgba[k] * a + 255 * (255 - a) + 127) / 255);
        rgba[3] = 255;
    }
}

/* ---------------- CRC-32 (PNG / zlib polynomial) ---------------- */

u32 pcio_crc32(u32 crc, const u8 *p, size_t n)
{
    static u32 table[256];
    static int init;
    size_t i;
    if (!init) {
        u32 c, k, j;
        for (k = 0; k < 256; k++) {
            c = k;
            for (j = 0; j < 8; j++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[k] = c;
        }
        init = 1;
    }
    crc ^= 0xFFFFFFFFu;
    for (i = 0; i < n; i++) crc = table[(crc ^ p[i]) & 255] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* ---------------- PackBits ---------------- */

/* Encoder structure follows PhotoCraft crates/psd (compression.rs,
 * MIT OR Apache-2.0): runs of 2+ become run packets, literals stop before
 * a run of 3, and the -128 no-op is never emitted. */
size_t pcio_packbits_encode(const u8 *src, size_t n, u8 *dst)
{
    size_t i = 0, o = 0;
    while (i < n) {
        size_t run = 1, start, len;
        while (i + run < n && run < 128 && src[i + run] == src[i]) run++;
        if (run >= 2) {
            dst[o++] = (u8)(257 - run);       /* 1 - run as a signed byte */
            dst[o++] = src[i];
            i += run;
            continue;
        }
        start = i;
        len = 0;
        while (i < n && len < 128) {
            if (i + 2 < n && src[i] == src[i + 1] && src[i] == src[i + 2]) break;
            i++;
            len++;
        }
        dst[o++] = (u8)(len - 1);
        memcpy(dst + o, src + start, len);
        o += len;
    }
    return o;
}

size_t pcio_packbits_decode(const u8 *src, size_t n, u8 *out, size_t outn)
{
    size_t i = 0, o = 0;
    while (o < outn && i < n) {
        int h = src[i++];
        if (h < 128) {
            size_t len = (size_t)h + 1, take;
            if (len > n - i) len = n - i;           /* truncated literal */
            take = len > outn - o ? outn - o : len;
            memcpy(out + o, src + i, take);
            o += take;
            i += len;
        } else if (h != 128) {
            size_t len = 257 - (size_t)h;
            if (i >= n) break;
            if (len > outn - o) len = outn - o;
            memset(out + o, src[i++], len);
            o += len;
        }
    }
    if (o < outn) memset(out + o, 0, outn - o);
    return i;
}

/* ---------------- Windows-1252 ---------------- */

static const u16 cp1252_hi[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

static int ansi_of(unsigned u)
{
    int k;
    if (u < 0x80 || (u >= 0xA0 && u < 0x100)) return (int)u;
    for (k = 0; k < 32; k++) if (cp1252_hi[k] == u) return 0x80 + k;
    return '?';
}

void pcio_utf16_to_ansi(const u16 *units, size_t n, char *out, size_t outn)
{
    size_t i, o = 0;
    if (!outn) return;
    for (i = 0; i < n && o + 1 < outn; i++) {
        unsigned u = units[i];
        int c;
        if (u == 0) break;
        if (u >= 0xD800 && u <= 0xDBFF) {          /* surrogate pair: one character */
            if (i + 1 < n && units[i + 1] >= 0xDC00 && units[i + 1] <= 0xDFFF) i++;
            c = '?';
        } else if (u >= 0xDC00 && u <= 0xDFFF) {
            c = '?';
        } else {
            c = ansi_of(u);
        }
        if (c < 32) c = '?';
        out[o++] = (char)c;
    }
    out[o] = 0;
}

size_t pcio_ansi_to_utf16(const char *s, u16 *out, size_t outmax)
{
    size_t o = 0;
    for (; *s && o < outmax; s++) {
        unsigned c = (u8)*s;
        out[o++] = (u16)(c >= 0x80 && c < 0xA0 ? cp1252_hi[c - 0x80] : c);
    }
    return o;
}
