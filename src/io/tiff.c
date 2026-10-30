/*
 * TIFF reader and writer (baseline plus the common extensions).
 *
 * Reading: Intel and Motorola byte order, first image only; strips or
 * tiles, chunky or planar; uncompressed, PackBits, LZW (with horizontal
 * predictor) and Adobe Deflate; 1/2/4/8/16 bits per sample; bilevel,
 * gray (WhiteIsZero/BlackIsZero), palette, RGB and CMYK (naive); extra
 * samples as transparency (associated alpha is un-premultiplied);
 * X/YResolution with ResolutionUnit.  JPEG, CCITT and YCbCr files are
 * refused with a message.
 *
 * Writing: Intel byte order, 8-bit gray or RGB, unassociated alpha when
 * the image is transparent and save_alpha is on, about 8 KB per strip,
 * compression none / LZW / PackBits (tiff_compress), resolution in dpi.
 */
#include "io/ioint.h"

typedef struct {
    const u8 *d;
    size_t n;
    int be;
} Tif;

typedef struct {
    int present;
    unsigned type;
    u32 count;
    const u8 *p;          /* value bytes (validated to lie in the file) */
} TEnt;

enum {
    T_WIDTH = 256, T_HEIGHT = 257, T_BPS = 258, T_COMP = 259, T_PHOTO = 262, T_FILL = 266,
    T_STRIPOFF = 273, T_SPP = 277, T_RPS = 278, T_STRIPCNT = 279, T_XRES = 282, T_YRES = 283,
    T_PLANAR = 284, T_RESUNIT = 296, T_PREDICT = 317, T_CMAP = 320, T_TILEW = 322, T_TILEH = 323,
    T_TILEOFF = 324, T_TILECNT = 325, T_INKSET = 332, T_EXTRA = 338, T_SFMT = 339
};

static const int type_size[13] = { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8 };

static unsigned t16(const Tif *t, const u8 *p) { return t->be ? get_u16be(p) : get_u16le(p); }
static u32 t32(const Tif *t, const u8 *p) { return t->be ? get_u32be(p) : get_u32le(p); }

static u32 ent_uint(const Tif *t, const TEnt *e, u32 i, u32 def)
{
    if (!e->present || i >= e->count) return def;
    switch (e->type) {
    case 1: case 6: case 7: return e->p[i];
    case 3: case 8: return t16(t, e->p + 2 * i);
    case 4: case 9: return t32(t, e->p + 4 * i);
    default: return def;
    }
}

static double ent_rational(const Tif *t, const TEnt *e)
{
    u32 num, den;
    if (!e->present || e->count < 1) return 0;
    if (e->type == 5) {
        num = t32(t, e->p); den = t32(t, e->p + 4);
        return den ? (double)num / den : 0;
    }
    return ent_uint(t, e, 0, 0);
}

/* ---------------- decompression ---------------- */

typedef struct {
    u16 prefix[4096];
    u16 len[4096];
    u8 suffix[4096], first[4096];
} LzwTab;

static void lzw_emit(const LzwTab *t, int code, u8 *out, size_t o, size_t outn)
{
    int L = t->len[code], k;
    for (k = L - 1; k >= 0; k--) {
        if (o + (size_t)k < outn) out[o + k] = t->suffix[code];
        code = t->prefix[code];
    }
}

/* TIFF LZW: MSB-first codes, 9..12 bits, "early change" width switch. */
static void lzw_decode(LzwTab *t, const u8 *src, size_t n, u8 *out, size_t outn)
{
    size_t bitpos = 0, nbitsall = n * 8, o = 0;
    int nbits = 9, next = 258, prev = -1, i;
    for (i = 0; i < 256; i++) { t->len[i] = 1; t->suffix[i] = t->first[i] = (u8)i; t->prefix[i] = 0; }
    while (o < outn) {
        int code = 0, k;
        if (bitpos + (size_t)nbits > nbitsall) break;
        for (k = 0; k < nbits; k++, bitpos++) code = (code << 1) | ((src[bitpos >> 3] >> (7 - (bitpos & 7))) & 1);
        if (code == 257) break;
        if (code == 256) { nbits = 9; next = 258; prev = -1; continue; }
        if (prev < 0) {
            if (code > 255) break;
            out[o++] = (u8)code;
            prev = code;
            continue;
        }
        if (code < next) {
            lzw_emit(t, code, out, o, outn);
            o += t->len[code];
            if (next < 4096) {
                t->prefix[next] = (u16)prev; t->suffix[next] = t->first[code];
                t->first[next] = t->first[prev]; t->len[next] = (u16)(t->len[prev] + 1);
                next++;
            }
        } else if (code == next && next < 4096) {
            t->prefix[next] = (u16)prev; t->suffix[next] = t->first[prev];
            t->first[next] = t->first[prev]; t->len[next] = (u16)(t->len[prev] + 1);
            next++;
            lzw_emit(t, code, out, o, outn);
            o += t->len[code];
        } else {
            break;                               /* corrupt stream */
        }
        if (next >= (1 << nbits) - 1 && nbits < 12) nbits++;
        prev = code;
    }
    if (o < outn) memset(out + o, 0, outn - o);
}

/* ---------------- reader ---------------- */

typedef struct {
    int w, h, spp, bps, comp, photo, planar, pred, fill, extra_alpha;
    int ncolor;
    u32 rps, tw, th;
    int tiled;
    TEnt off, cnt, cmap;
} TInfo;

static int get_sample(const u8 *row, size_t i, int bps, int be)
{
    switch (bps) {
    case 1: return (row[i >> 3] >> (7 - (i & 7))) & 1;
    case 2: return (row[i >> 2] >> (6 - 2 * (i & 3))) & 3;
    case 4: return (row[i >> 1] >> ((i & 1) ? 0 : 4)) & 15;
    case 16: return be ? (int)get_u16be(row + 2 * i) : (int)get_u16le(row + 2 * i);
    default: return row[i];
    }
}

static int to8(int v, int bps)
{
    switch (bps) {
    case 1: return v ? 255 : 0;
    case 2: return v * 85;
    case 4: return v * 17;
    case 16: return (int)(((unsigned)v * 255u + 32767u) / 65535u);
    default: return v;
    }
}

static u8 bitrev(u8 b)
{
    b = (u8)((b & 0xF0) >> 4 | (b & 0x0F) << 4);
    b = (u8)((b & 0xCC) >> 2 | (b & 0x33) << 2);
    return (u8)((b & 0xAA) >> 1 | (b & 0x55) << 1);
}

PcDoc *pc_tiff_load(const u8 *data, size_t n, char *err, size_t errn)
{
    Tif t;
    TInfo I;
    TEnt e[T_SFMT + 1 - T_WIDTH];
    PcRd r;
    u32 ifd, nent, k, across, down, nchunks, planes;
    double dpi = 72;
    unsigned long long total_in = 0, need, ratio;
    u8 *samp = NULL, *chunk = NULL, *tmpin = NULL;
    LzwTab *lzw = NULL;
    PcBuf *buf = NULL;
    size_t np, i, chunk_cap = 0;
    int s, gray;
    u8 cmap[3][256];

    memset(&I, 0, sizeof I);
    memset(e, 0, sizeof e);
    if (n < 8 || !((data[0] == 'I' && data[1] == 'I') || (data[0] == 'M' && data[1] == 'M'))) {
        pcio_seterr(err, errn, "This is not a TIFF file."); return NULL;
    }
    t.d = data; t.n = n; t.be = data[0] == 'M';
    if (t16(&t, data + 2) == 43) { pcio_seterr(err, errn, "BigTIFF files are not supported."); return NULL; }
    if (t16(&t, data + 2) != 42) { pcio_seterr(err, errn, "This is not a TIFF file."); return NULL; }
    ifd = t32(&t, data + 4);
    rd_init(&r, data, n);
    rd_seek(&r, ifd);
    nent = t.be ? rd_u16be(&r) : rd_u16le(&r);
    if (r.err || nent == 0 || nent > 4096 || (unsigned long long)nent * 12 > rd_left(&r)) {
        pcio_seterr(err, errn, "The TIFF file is damaged (directory)."); return NULL;
    }
    for (k = 0; k < nent; k++) {
        const u8 *q = data + r.pos + (size_t)k * 12;
        unsigned tag = t16(&t, q), type = t16(&t, q + 2);
        u32 count = t32(&t, q + 4);
        unsigned long long sz;
        TEnt *x;
        if (tag < T_WIDTH || tag > T_SFMT || type < 1 || type > 12) continue;
        x = &e[tag - T_WIDTH];
        sz = (unsigned long long)count * type_size[type];
        if (count == 0) continue;
        if (sz <= 4) x->p = q + 8;
        else {
            u32 off = t32(&t, q + 8);
            if (off > n || sz > n - off) continue;      /* points outside the file: ignore */
            x->p = data + off;
        }
        x->present = 1; x->type = type; x->count = count;
    }
#define TAG(id) (&e[(id) - T_WIDTH])
    I.w = (int)PC_MIN(ent_uint(&t, TAG(T_WIDTH), 0, 0), 0x7FFFFFFFu);
    I.h = (int)PC_MIN(ent_uint(&t, TAG(T_HEIGHT), 0, 0), 0x7FFFFFFFu);
    I.spp = (int)ent_uint(&t, TAG(T_SPP), 0, 1);
    I.bps = (int)ent_uint(&t, TAG(T_BPS), 0, 1);
    I.comp = (int)ent_uint(&t, TAG(T_COMP), 0, 1);
    I.photo = (int)ent_uint(&t, TAG(T_PHOTO), 0, I.spp >= 3 ? 2 : 1);
    I.planar = (int)ent_uint(&t, TAG(T_PLANAR), 0, 1);
    I.pred = (int)ent_uint(&t, TAG(T_PREDICT), 0, 1);
    I.fill = (int)ent_uint(&t, TAG(T_FILL), 0, 1);
    if (!pcio_dims_ok(I.w, I.h)) { pcio_seterr(err, errn, "The image is too large (or has no pixels)."); return NULL; }
    if (I.spp < 1 || I.spp > 16) { pcio_seterr(err, errn, "Unsupported TIFF sample count (%d).", I.spp); return NULL; }
    for (s = 1; s < I.spp; s++)
        if ((int)ent_uint(&t, TAG(T_BPS), (u32)s, (u32)I.bps) != I.bps) { pcio_seterr(err, errn, "TIFF files with mixed sample sizes are not supported."); return NULL; }
    if (I.bps != 1 && I.bps != 2 && I.bps != 4 && I.bps != 8 && I.bps != 16) { pcio_seterr(err, errn, "Unsupported TIFF bit depth (%d bits).", I.bps); return NULL; }
    if (ent_uint(&t, TAG(T_SFMT), 0, 1) != 1) { pcio_seterr(err, errn, "Floating-point or signed TIFF samples are not supported."); return NULL; }
    switch (I.comp) {
    case 1: ratio = 1; break;
    case 32773: ratio = 128; break;
    case 5: ratio = 4096; break;
    case 8: case 32946: ratio = 1032; break;
    case 7: case 6: pcio_seterr(err, errn, "JPEG-compressed TIFF files are not supported."); return NULL;
    case 2: case 3: case 4: pcio_seterr(err, errn, "Fax (CCITT) compressed TIFF files are not supported."); return NULL;
    default: pcio_seterr(err, errn, "Unsupported TIFF compression (%d).", I.comp); return NULL;
    }
    switch (I.photo) {
    case 0: case 1: I.ncolor = 1; break;
    case 2: I.ncolor = 3; break;
    case 3: I.ncolor = 1; break;
    case 5:
        I.ncolor = 4;
        if (ent_uint(&t, TAG(T_INKSET), 0, 1) != 1) { pcio_seterr(err, errn, "Only CMYK separations are supported."); return NULL; }
        break;
    case 6: pcio_seterr(err, errn, "YCbCr TIFF files are not supported."); return NULL;
    default: pcio_seterr(err, errn, "Unsupported TIFF colour space (%d).", I.photo); return NULL;
    }
    if (I.spp < I.ncolor) { pcio_seterr(err, errn, "The TIFF file is damaged (too few samples)."); return NULL; }
    if (I.photo == 3) {
        TEnt *cm = TAG(T_CMAP);
        u32 nc = 1u << I.bps;
        if (I.bps > 8 || !cm->present || cm->type != 3 || cm->count < 3 * nc) { pcio_seterr(err, errn, "The TIFF colour map is missing."); return NULL; }
        memset(cmap, 0, sizeof cmap);
        for (k = 0; k < nc; k++)
            for (s = 0; s < 3; s++) cmap[s][k] = (u8)(ent_uint(&t, cm, (u32)s * nc + k, 0) >> 8);
    }
    I.extra_alpha = 0;
    if (I.spp > I.ncolor) {
        u32 ex = ent_uint(&t, TAG(T_EXTRA), 0, 0);
        if (ex == 1 || ex == 2) I.extra_alpha = (int)ex;
    }
    if (I.planar != 1 && I.planar != 2) I.planar = 1;
    if (I.spp == 1) I.planar = 1;
    I.tiled = TAG(T_TILEOFF)->present;
    if (I.tiled) {
        I.tw = ent_uint(&t, TAG(T_TILEW), 0, 0);
        I.th = ent_uint(&t, TAG(T_TILEH), 0, 0);
        if (I.tw < 1 || I.th < 1 || I.tw > 65536 || I.th > 65536) { pcio_seterr(err, errn, "The TIFF tile size is invalid."); return NULL; }
        I.off = *TAG(T_TILEOFF); I.cnt = *TAG(T_TILECNT);
    } else {
        I.rps = ent_uint(&t, TAG(T_RPS), 0, (u32)I.h);
        if (I.rps < 1 || I.rps > (u32)I.h) I.rps = (u32)I.h;
        I.tw = (u32)I.w; I.th = I.rps;
        I.off = *TAG(T_STRIPOFF); I.cnt = *TAG(T_STRIPCNT);
    }
    across = (u32)((I.w + I.tw - 1) / I.tw);
    down = (u32)((I.h + I.th - 1) / I.th);
    planes = I.planar == 2 ? (u32)I.spp : 1;
    nchunks = across * down * planes;
    if (!I.off.present || I.off.count < nchunks) { pcio_seterr(err, errn, "The TIFF file is damaged (strip offsets)."); return NULL; }
    {
        TEnt *x = TAG(T_XRES);
        unsigned unit = ent_uint(&t, TAG(T_RESUNIT), 0, 2);
        double v = ent_rational(&t, x);
        if (v > 0 && unit == 2) dpi = v;
        else if (v > 0 && unit == 3) dpi = v * 2.54;
        if (!(dpi >= 1 && dpi <= 100000)) dpi = 72;
        else if (fabs(dpi - pcio_uround(dpi)) < 0.01) dpi = pcio_uround(dpi);
    }
    /* how much compressed data is there? (bounds the work done) */
    for (k = 0; k < nchunks; k++) {
        u32 off = ent_uint(&t, &I.off, k, 0), cnt = I.cnt.present ? ent_uint(&t, &I.cnt, k, 0) : (u32)(n > off ? n - off : 0);
        if (off < n) total_in += PC_MIN((unsigned long long)cnt, (unsigned long long)(n - off));
    }
    need = (unsigned long long)I.w * I.h * I.spp * I.bps / 8;
    if (need > total_in * ratio + 4096) { pcio_seterr(err, errn, "The TIFF file is truncated or damaged."); return NULL; }

    np = (size_t)I.w * I.h;
    samp = (u8 *)pc_alloc_checked(np, (size_t)I.spp, 1, 1);
    if (!samp) { pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
    if (I.comp == 5) {
        lzw = (LzwTab *)malloc(sizeof *lzw);
        if (!lzw) goto oom;
    }
    for (k = 0; k < nchunks; k++) {
        u32 plane = k / (across * down), ci = k % (across * down);
        u32 cx = (ci % across) * I.tw, cy = (ci / across) * I.th;
        int sppc = I.planar == 2 ? 1 : I.spp;
        size_t rb = ((size_t)I.tw * sppc * I.bps + 7) / 8;
        u32 rows = I.tiled ? I.th : PC_MIN(I.th, (u32)I.h - cy);
        size_t outsz = rb * rows, y, x;
        u32 off = ent_uint(&t, &I.off, k, 0), cnt = I.cnt.present ? ent_uint(&t, &I.cnt, k, 0) : 0;
        const u8 *src;
        size_t srcn;
        if (off >= n) continue;
        if (!I.cnt.present) cnt = (u32)PC_MIN((size_t)0xFFFFFFFFu, n - off);
        src = data + off;
        srcn = PC_MIN((size_t)cnt, n - off);
        if (outsz > chunk_cap) {
            free(chunk);
            chunk = (u8 *)pc_alloc_checked(outsz, 1, 1, 0);
            chunk_cap = chunk ? outsz : 0;
            if (!chunk) goto oom;
        }
        if (I.fill == 2) {
            free(tmpin);
            tmpin = (u8 *)malloc(srcn ? srcn : 1);
            if (!tmpin) goto oom;
            for (i = 0; i < srcn; i++) tmpin[i] = bitrev(src[i]);
            src = tmpin;
        }
        switch (I.comp) {
        case 1:
            memcpy(chunk, src, PC_MIN(srcn, outsz));
            if (srcn < outsz) memset(chunk + srcn, 0, outsz - srcn);
            break;
        case 32773: {
            size_t pos = 0;
            for (y = 0; y < rows; y++)
                pos += pcio_packbits_decode(src + PC_MIN(pos, srcn), srcn - PC_MIN(pos, srcn), chunk + y * rb, rb);
            break;
        }
        case 5:
            lzw_decode(lzw, src, srcn, chunk, outsz);
            break;
        default: {
            long got = pcio_inflate(chunk, outsz, src, srcn, 1);
            if (got < 0) got = 0;
            if ((size_t)got < outsz) memset(chunk + got, 0, outsz - (size_t)got);
        }
        }
        if (I.pred == 2 && (I.bps == 8 || I.bps == 16)) {
            for (y = 0; y < rows; y++) {
                u8 *row = chunk + y * rb;
                size_t ns = (size_t)I.tw * sppc;
                if (I.bps == 8) for (i = (size_t)sppc; i < ns; i++) row[i] = (u8)(row[i] + row[i - sppc]);
                else for (i = (size_t)sppc; i < ns; i++) {
                    unsigned v = (get_sample(row, i, 16, t.be) + get_sample(row, i - sppc, 16, t.be)) & 0xFFFF;
                    if (t.be) { row[2 * i] = (u8)(v >> 8); row[2 * i + 1] = (u8)v; }
                    else { row[2 * i] = (u8)v; row[2 * i + 1] = (u8)(v >> 8); }
                }
            }
        }
        for (y = 0; y < rows; y++) {
            size_t iy = cy + y;
            const u8 *row = chunk + y * rb;
            if (iy >= (size_t)I.h) break;
            for (x = 0; x < I.tw; x++) {
                size_t ix = cx + x;
                u8 *o;
                if (ix >= (size_t)I.w) break;
                o = samp + (iy * I.w + ix) * I.spp;
                if (I.planar == 2) {
                    int v = get_sample(row, x, I.bps, t.be);
                    o[plane] = (u8)(I.photo == 3 ? v : to8(v, I.bps));
                } else {
                    for (s = 0; s < I.spp; s++) {
                        int v = get_sample(row, x * I.spp + s, I.bps, t.be);
                        o[s] = (u8)(I.photo == 3 && s == 0 ? v : to8(v, I.bps));
                    }
                }
            }
        }
    }
    free(chunk); chunk = NULL;
    free(tmpin); tmpin = NULL;
    free(lzw); lzw = NULL;

    buf = pc_buf_new(I.w, I.h, 4, 0);
    if (!buf) goto oom;
    for (i = 0; i < np; i++) {
        const u8 *q = samp + i * I.spp;
        u8 *o = buf->px + i * 4;
        int a = I.extra_alpha ? q[I.ncolor] : 255;
        switch (I.photo) {
        case 0: o[0] = o[1] = o[2] = (u8)(255 - q[0]); break;
        case 1: o[0] = o[1] = o[2] = q[0]; break;
        case 3: o[0] = cmap[0][q[0]]; o[1] = cmap[1][q[0]]; o[2] = cmap[2][q[0]]; break;
        case 5: {
            int kk = 255 - q[3];
            o[0] = (u8)pc_mul255(255 - q[0], kk); o[1] = (u8)pc_mul255(255 - q[1], kk); o[2] = (u8)pc_mul255(255 - q[2], kk);
            break;
        }
        default: o[0] = q[0]; o[1] = q[1]; o[2] = q[2];
        }
        if (I.extra_alpha == 1 && a > 0 && a < 255) {          /* associated: un-premultiply */
            int c2;
            for (c2 = 0; c2 < 3; c2++) { int v = (o[c2] * 255 + a / 2) / a; o[c2] = (u8)PC_MIN(v, 255); }
        }
        o[3] = (u8)a;
    }
    free(samp);
    gray = I.photo == 0 || I.photo == 1;
    return pcio_doc_from_buf(buf, gray ? PC_MODE_GRAY : PC_MODE_RGB, dpi, PC_FMT_TIFF, err, errn);
oom:
    free(samp); free(chunk); free(tmpin); free(lzw);
    pc_buf_unref(buf);
    pcio_seterr(err, errn, "Not enough memory to open the file.");
    return NULL;
#undef TAG
}

/* ---------------- writer ---------------- */

typedef struct {
    PcWr *w;
    u32 acc;
    int nacc;
} TBits;

static void tb_code(TBits *b, unsigned code, int nbits)
{
    b->acc = (b->acc << nbits) | code;
    b->nacc += nbits;
    while (b->nacc >= 8) { b->nacc -= 8; wr_u8(b->w, (b->acc >> b->nacc) & 255); }
    b->acc &= (1u << b->nacc) - 1;
}

/* TIFF LZW encoder; code-width and table-reset rules match libtiff. */
static int lzw_encode(PcWr *w, const u8 *src, size_t n, u32 *keys, u16 *vals)
{
    TBits b;
    int next = 258, nbits = 9, ent;
    size_t i;
    b.w = w; b.acc = 0; b.nacc = 0;
    memset(keys, 0, sizeof(u32) * 8192);
    tb_code(&b, 256, nbits);
    if (n) {
        ent = src[0];
        for (i = 1; i < n; i++) {
            int c = src[i];
            u32 key = ((u32)ent << 8 | (u32)c) + 1, h = (key * 2654435761u) >> 19;
            while (keys[h] && keys[h] != key) h = (h + 1) & 8191;
            if (keys[h]) { ent = vals[h]; continue; }
            tb_code(&b, (unsigned)ent, nbits);
            keys[h] = key;
            vals[h] = (u16)next;
            next++;
            if (next == 4094) {
                tb_code(&b, 256, nbits);
                memset(keys, 0, sizeof(u32) * 8192);
                next = 258;
                nbits = 9;
            } else if (next > (1 << nbits) - 1) {
                nbits++;
            }
            ent = c;
        }
        tb_code(&b, (unsigned)ent, nbits);
        next++;
        if (next == 4094) { tb_code(&b, 256, nbits); nbits = 9; }
        else if (next > (1 << nbits) - 1) nbits++;
    }
    tb_code(&b, 257, nbits);
    if (b.nacc) wr_u8(w, (b.acc << (8 - b.nacc)) & 255);
    return w->err ? -1 : 0;
}

typedef struct { unsigned tag, type; u32 count, value; } OutEnt;

int pc_tiff_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcBuf *c;
    int W = d->s.w, H = d->s.h, gray = d->s.mode == PC_MODE_GRAY, alpha, spp, comp, x, y, i;
    int cmode = o ? o->tiff_compress : 0;
    size_t np = (size_t)W * H, rb, k;
    u32 rps, nstrips, *offs = NULL, *cnts = NULL, *keys = NULL, xres;
    u16 *vals = NULL;
    u8 *row = NULL, *pk = NULL;
    OutEnt ents[20];
    int ne = 0, rc = -1;
    size_t bps_at = 0, offs_at = 0, cnts_at = 0, res_at, sw_at, ifd_at;
    static const char sw[] = "PhotoCraft Classic";
    PcWr w;
    memset(&w, 0, sizeof w);
    *out = NULL; *outn = 0;
    c = pcio_composite(d, err, errn);
    if (!c) return -1;
    alpha = (!o || o->save_alpha) && pcio_has_alpha(c->px, np);
    if (!alpha) pcio_matte_white(c->px, np);
    spp = (gray ? 1 : 3) + alpha;
    comp = cmode == 1 ? 5 : cmode == 2 ? 32773 : 1;
    rb = (size_t)W * spp;
    rps = (u32)PC_MAX(1, 8192 / rb);
    if (rps > (u32)H) rps = (u32)H;
    nstrips = ((u32)H + rps - 1) / rps;
    offs = (u32 *)malloc(sizeof(u32) * nstrips);
    cnts = (u32 *)malloc(sizeof(u32) * nstrips);
    row = (u8 *)malloc(rb * rps);
    pk = (u8 *)malloc(rb + rb / 128 + 16);
    if (comp == 5) { keys = (u32 *)malloc(sizeof(u32) * 8192); vals = (u16 *)malloc(sizeof(u16) * 8192); }
    if (!offs || !cnts || !row || !pk || (comp == 5 && (!keys || !vals))) goto done;

    wr_bytes(&w, "II", 2);
    wr_u16le(&w, 42);
    wr_u32le(&w, 0);                           /* IFD offset, patched */
    for (k = 0; k < nstrips; k++) {
        u32 y0 = (u32)k * rps, rows = PC_MIN(rps, (u32)H - y0);
        u8 *q = row;
        for (y = (int)y0; y < (int)(y0 + rows); y++) {
            const u8 *s = c->px + (size_t)y * W * 4;
            for (x = 0; x < W; x++, s += 4) {
                if (gray) *q++ = s[0];
                else { *q++ = s[0]; *q++ = s[1]; *q++ = s[2]; }
                if (alpha) *q++ = s[3];
            }
        }
        offs[k] = (u32)w.n;
        if (comp == 1) wr_bytes(&w, row, rb * rows);
        else if (comp == 32773) {
            for (y = 0; y < (int)rows; y++) {
                size_t m = pcio_packbits_encode(row + (size_t)y * rb, rb, pk);
                wr_bytes(&w, pk, m);
            }
        } else if (lzw_encode(&w, row, rb * rows, keys, vals)) goto done;
        cnts[k] = (u32)(w.n - offs[k]);
        if (w.n & 1) wr_u8(&w, 0);
    }
    /* out-of-line values */
    if (spp > 2) { bps_at = w.n; for (i = 0; i < spp; i++) wr_u16le(&w, 8); }
    if (nstrips > 1) {
        offs_at = w.n; for (k = 0; k < nstrips; k++) wr_u32le(&w, offs[k]);
        cnts_at = w.n; for (k = 0; k < nstrips; k++) wr_u32le(&w, cnts[k]);
    }
    xres = pcio_uround(pcio_doc_dpi(d) * 1000);
    res_at = w.n;
    wr_u32le(&w, xres); wr_u32le(&w, 1000);
    wr_u32le(&w, xres); wr_u32le(&w, 1000);
    sw_at = w.n;
    wr_bytes(&w, sw, sizeof sw);
    if (w.n & 1) wr_u8(&w, 0);

#define ADD(tg, ty, cn, v) do { ents[ne].tag = (tg); ents[ne].type = (ty); ents[ne].count = (cn); ents[ne].value = (v); ne++; } while (0)
    ADD(254, 4, 1, 0);
    ADD(T_WIDTH, 4, 1, (u32)W);
    ADD(T_HEIGHT, 4, 1, (u32)H);
    ADD(T_BPS, 3, (u32)spp, spp > 2 ? (u32)bps_at : (spp == 2 ? (8u | 8u << 16) : 8u));
    ADD(T_COMP, 3, 1, (u32)comp);
    ADD(T_PHOTO, 3, 1, gray ? 1u : 2u);
    ADD(T_STRIPOFF, 4, nstrips, nstrips > 1 ? (u32)offs_at : offs[0]);
    ADD(T_SPP, 3, 1, (u32)spp);
    ADD(T_RPS, 4, 1, rps);
    ADD(T_STRIPCNT, 4, nstrips, nstrips > 1 ? (u32)cnts_at : cnts[0]);
    ADD(T_XRES, 5, 1, (u32)res_at);
    ADD(T_YRES, 5, 1, (u32)res_at + 8);
    ADD(T_PLANAR, 3, 1, 1);
    ADD(T_RESUNIT, 3, 1, 2);
    ADD(305, 2, (u32)sizeof sw, (u32)sw_at);
    if (alpha) ADD(T_EXTRA, 3, 1, 2);           /* unassociated alpha */
#undef ADD
    ifd_at = w.n;
    wr_u16le(&w, (unsigned)ne);
    for (i = 0; i < ne; i++) {
        wr_u16le(&w, ents[i].tag);
        wr_u16le(&w, ents[i].type);
        wr_u32le(&w, ents[i].count);
        if (ents[i].type == 3 && ents[i].count == 1) { wr_u16le(&w, ents[i].value); wr_u16le(&w, 0); }
        else wr_u32le(&w, ents[i].value);
    }
    wr_u32le(&w, 0);                           /* no next IFD */
    wr_put_u32le(&w, 4, (u32)ifd_at);
    rc = w.err ? -1 : 0;
done:
    free(offs); free(cnts); free(row); free(pk); free(keys); free(vals);
    pc_buf_unref(c);
    if (rc) { wr_free(&w); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    return wr_finish(&w, out, outn, err, errn);
}
