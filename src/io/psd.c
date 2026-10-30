/*
 * Photoshop document (PSD, version 1) reader and writer.
 *
 * File structure, layer record layout, mask record forms, tagged block
 * padding rules, channel compression (raw / PackBits RLE / ZIP / ZIP with
 * prediction) and the merged-image conventions are ported from PhotoCraft
 * crates/psd (file.rs, layer.rs, compression.rs, resources.rs,
 * builder.rs; MIT OR Apache-2.0).
 *
 * Reading: Bitmap, Grayscale, Indexed, RGB, CMYK, Multichannel, Duotone
 * and Lab at 1/8/16/32 bits are converted to our 8-bit RGB or Grayscale
 * model (CMYK and Lab without colour management).  Layers (also from the
 * Lr16/Lr32 global blocks of deep files), masks (user and rasterized
 * vector masks combined, densities applied, feather ignored), blend modes
 * (later modes map to the nearest 1998 one), opacity, clipping,
 * visibility, transparency lock, Unicode or UTF-8 names, alpha channels
 * and resolution are kept.  Groups are flattened away (their divider
 * records are skipped; a hidden group hides its layers).  Text, shape,
 * fill, smart-object and adjustment layers load as whatever pixels they
 * store (fill opacity, layer effects and vector-only content are not
 * rendered).  PSB (version 2) is refused.  At most PC_MAX_LAYERS layers
 * load; the rest are dropped with a note in err.
 *
 * Writing: version 1, 8 bits, RGB or Grayscale, RLE everywhere, every
 * layer with its tight bounds, -1 transparency (except a Background),
 * -2 user mask, 'luni' Unicode names, resolution, alpha channel names, and
 * a merged composite (matted against white when transparent, like
 * Photoshop) followed by the document's alpha channels.  Text layers are
 * written as plain pixel layers (their rendered pixels): we do not write
 * Photoshop's 'TySh' type data.
 */
#include "io/ioint.h"

enum { PSD_MAX_LAYER_CH = 64, PSD_MAX_FILE_CH = 56, PSD_MAX_LAYER_DIM = 300000 };
enum { M_BITMAP = 0, M_GRAY = 1, M_INDEXED = 2, M_RGB = 3, M_CMYK = 4, M_MULTI = 7, M_DUOTONE = 8, M_LAB = 9 };

/* ======================================================================
 * Reading
 * ====================================================================== */

typedef struct {
    int id;
    const u8 *p;          /* compression field + data, NULL when absent */
    size_t len;
} PsdChan;

typedef struct {
    int top, left, bottom, right;
    int nch, ch0;         /* channels: chans[ch0 .. ch0+nch-1] */
    char key[4];
    int opacity, clipping, flags;
    int has_mask, mtop, mleft, mbottom, mright, mdef, mflags;
    int has_real, rtop, rleft, rbottom, rright, rdef, rflags;
    int udens, vdens;     /* mask densities from the mask parameters (255 = full) */
    int protect;          /* 'lspf': bit 0 transparency, 1 pixels, 2 position */
    char name[64];
    int section;          /* 0 layer, 1/2 group (open/closed) record, 3 group end divider */
    int hidden_by_group;
} PsdRec;

typedef struct {
    /* header */
    int w, h, depth, mode, nchan;
    int ncolor;           /* colour channels of the mode */
    u8 palette[768];
    int has_palette, trans_index;
    double dpi;
    int target_layer;
    /* alpha channel names */
    char anames[PC_MAX_CHANNELS][32];
    int nanames;
    /* layers */
    PsdRec *recs;
    int nrecs;
    PsdChan *chans;
    int nchans, chans_cap;
    int merged_alpha;
    int has_alpha_ids, alpha_id_zero;   /* resource 1053; index of id 0 (transparency) or -1 */
    int layers_in_block;                /* layer info came from an Lr16/Lr32/Layr block */
    char *err;
    size_t errn;
} PsdCtx;

typedef struct {
    u8 *dst;              /* canvas-sized buffer, NULL = discard */
    int stride, off;      /* bytes per pixel, byte offset in the pixel */
    long long ox, oy;     /* plane origin in canvas coordinates */
} PsdSink;

static int is_sig(const u8 *p) { return !memcmp(p, "8BIM", 4) || !memcmp(p, "8B64", 4); }

static void sink_row(const PsdCtx *c, const PsdSink *s, int y, const u8 *row, int w)
{
    long long cy = s->oy + y, x0, x1, x;
    u8 *d;
    if (!s->dst || cy < 0 || cy >= c->h) return;
    x0 = s->ox < 0 ? -s->ox : 0;
    x1 = c->w - s->ox;
    if (x1 > w) x1 = w;
    if (x0 >= x1) return;
    d = s->dst + ((size_t)cy * c->w + (size_t)(s->ox + x0)) * s->stride + s->off;
    for (x = x0; x < x1; x++, d += s->stride) *d = row[x];
}

/* One stored row (depth bits per sample) to 8-bit samples. */
static void row_to8(const u8 *src, u8 *dst, int w, int depth)
{
    int x;
    switch (depth) {
    case 1:
        for (x = 0; x < w; x++) dst[x] = (src[x >> 3] >> (7 - (x & 7))) & 1 ? 0 : 255;
        break;
    case 16:
        for (x = 0; x < w; x++) dst[x] = (u8)((get_u16be(src + 2 * x) * 255u + 32767u) / 65535u);
        break;
    case 32:
        for (x = 0; x < w; x++) {
            union { u32 u; float f; } v;
            double f;
            v.u = get_u32be(src + 4 * x);
            f = v.f;
            if (!(f > 0)) f = 0;          /* also NaN */
            if (f > 1) f = 1;
            f = pow(f, 1 / 2.2);          /* 32-bit documents are linear */
            dst[x] = (u8)(f * 255 + 0.5);
        }
        break;
    default:
        memcpy(dst, src, (size_t)w);
    }
}

/* Reverse ZIP prediction for one row (port of crates/psd unpredict). */
static void unpredict_row(u8 *row, size_t rb, int w, int depth, u8 *scratch)
{
    size_t i;
    int x, k;
    if (depth == 16) {
        for (x = 1; x < w; x++) {
            unsigned a = get_u16be(row + 2 * x), b = get_u16be(row + 2 * x - 2), v = (a + b) & 0xFFFF;
            row[2 * x] = (u8)(v >> 8); row[2 * x + 1] = (u8)v;
        }
    } else if (depth == 32) {
        for (i = 1; i < rb; i++) row[i] = (u8)(row[i] + row[i - 1]);
        memcpy(scratch, row, rb);
        for (x = 0; x < w; x++)
            for (k = 0; k < 4; k++) row[4 * x + k] = scratch[(size_t)k * w + x];
    } else {
        for (i = 1; i < rb; i++) row[i] = (u8)(row[i] + row[i - 1]);
    }
}

static size_t row_bytes(int w, int depth) { return depth == 1 ? ((size_t)w + 7) / 8 : (size_t)w * (depth / 8); }

/*
 * Decode nplanes planes of w x h samples (compression field first in
 * data) and hand 8-bit rows to sinks[plane].  Damaged or short data is
 * zero-filled rather than refused; structurally impossible data fails.
 */
static int psd_decode(PsdCtx *c, const u8 *data, size_t len, int nplanes, int w, int h, PsdSink *sinks)
{
    PcRd r;
    int comp, p, y, ok = 0;
    size_t rb = row_bytes(w, c->depth);
    unsigned long long total = (unsigned long long)nplanes * h * rb;
    u8 *raw = NULL, *row8 = NULL, *scratch = NULL;
    if (w <= 0 || h <= 0 || nplanes <= 0) return 0;
    if (w > PSD_MAX_LAYER_DIM || h > PSD_MAX_LAYER_DIM) return -1;
    rd_init(&r, data, len);
    comp = (int)rd_u16be(&r);
    if (r.err) return -1;
    row8 = (u8 *)malloc((size_t)w);
    if (!row8) return -1;
    if (comp == 0) {
        const u8 *base = data + 2;
        size_t avail = len - 2;
        u8 *zero = (u8 *)calloc(1, rb);
        if (!zero) goto done;
        for (p = 0; p < nplanes; p++)
            for (y = 0; y < h; y++) {
                unsigned long long at = ((unsigned long long)p * h + y) * rb;
                const u8 *src = (at + rb <= avail) ? base + (size_t)at : zero;
                if (!sinks[p].dst) continue;
                row_to8(src, row8, w, c->depth);
                sink_row(c, &sinks[p], y, row8, w);
            }
        free(zero);
        ok = 1;
    } else if (comp == 1) {
        unsigned long long nrows = (unsigned long long)nplanes * h;
        const u8 *counts = rd_bytes(&r, (size_t)(nrows * 2 <= rd_left(&r) ? nrows * 2 : rd_left(&r) + 1));
        size_t pos;
        u8 *rowbuf;
        if (!counts) goto done;
        /* PackBits expands at most 64x: refuse data that cannot fill the planes. */
        if (total > (unsigned long long)rd_left(&r) * 64 + 64) goto done;
        rowbuf = (u8 *)malloc(rb);
        if (!rowbuf) goto done;
        pos = r.pos;
        for (p = 0; p < nplanes; p++)
            for (y = 0; y < h; y++) {
                size_t k = (size_t)p * h + y;
                size_t cnt = get_u16be(counts + 2 * k), avail = len - pos;
                if (cnt > avail) cnt = avail;
                if (sinks[p].dst) {
                    pcio_packbits_decode(data + pos, cnt, rowbuf, rb);
                    row_to8(rowbuf, row8, w, c->depth);
                    sink_row(c, &sinks[p], y, row8, w);
                }
                pos += cnt;
            }
        free(rowbuf);
        ok = 1;
    } else if (comp == 2 || comp == 3) {
        size_t avail = len - 2;
        long got;
        /* deflate expands at most ~1032x */
        if (total > (unsigned long long)avail * 1032 + 1024 || total > (size_t)-1) goto done;
        raw = (u8 *)pc_alloc_checked((size_t)total, 1, 1, 1);
        if (!raw) goto done;
        got = pcio_inflate(raw, (size_t)total, data + 2, avail, 1);
        if (got < 0) goto done;
        if (comp == 3) {
            scratch = (u8 *)malloc(rb);
            if (!scratch) goto done;
            for (y = 0; y < nplanes * h; y++) unpredict_row(raw + (size_t)y * rb, rb, w, c->depth, scratch);
        }
        for (p = 0; p < nplanes; p++)
            for (y = 0; y < h; y++) {
                if (!sinks[p].dst) continue;
                row_to8(raw + ((size_t)p * h + y) * rb, row8, w, c->depth);
                sink_row(c, &sinks[p], y, row8, w);
            }
        ok = 1;
    }
done:
    free(raw); free(row8); free(scratch);
    return ok ? 0 : -1;
}

/* Convert colour planes of non-RGB modes to RGB in px (alpha untouched). */
static void convert_planes(const PsdCtx *c, u8 **planes, u8 *px, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++, px += 4) {
        int r, g, b;
        switch (c->mode) {
        case M_CMYK: {
            /* stored inverted: 255 = no ink; naive conversion without a profile */
            int k = planes[3] ? planes[3][i] : 255;
            r = pc_mul255(planes[0][i], k); g = pc_mul255(planes[1][i], k); b = pc_mul255(planes[2][i], k);
            break;
        }
        case M_LAB: {
            /* CIE L*a*b* (D50) -> sRGB, approximate (no chromatic adaptation) */
            double L = planes[0][i] * 100.0 / 255, A = planes[1][i] - 128.0, B = planes[2][i] - 128.0;
            double fy = (L + 16) / 116, fx = fy + A / 500, fz = fy - B / 200, X, Y, Z, rl, gl, bl;
            double v[3];
            int k;
            X = fx > 6.0 / 29 ? fx * fx * fx : (fx - 16.0 / 116) * 3 * (6.0 / 29) * (6.0 / 29);
            Y = fy > 6.0 / 29 ? fy * fy * fy : (fy - 16.0 / 116) * 3 * (6.0 / 29) * (6.0 / 29);
            Z = fz > 6.0 / 29 ? fz * fz * fz : (fz - 16.0 / 116) * 3 * (6.0 / 29) * (6.0 / 29);
            X *= 0.9642; Z *= 0.8251;
            rl = 3.1339 * X - 1.6169 * Y - 0.4906 * Z;
            gl = -0.9788 * X + 1.9161 * Y + 0.0335 * Z;
            bl = 0.0719 * X - 0.2290 * Y + 1.4052 * Z;
            v[0] = rl; v[1] = gl; v[2] = bl;
            for (k = 0; k < 3; k++) {
                double t = v[k];
                if (t <= 0) t = 0;
                else if (t >= 1) t = 1;
                else t = t <= 0.0031308 ? 12.92 * t : 1.055 * pow(t, 1 / 2.4) - 0.055;
                v[k] = t * 255 + 0.5;
            }
            r = (int)v[0]; g = (int)v[1]; b = (int)v[2];
            break;
        }
        case M_INDEXED: {
            int idx = planes[0][i];
            r = c->palette[idx]; g = c->palette[256 + idx]; b = c->palette[512 + idx];
            break;
        }
        default:
            r = g = b = planes[0][i];
        }
        px[0] = (u8)r; px[1] = (u8)g; px[2] = (u8)b;
    }
}

static int direct_mode(int mode) { return mode == M_RGB || mode == M_GRAY || mode == M_BITMAP || mode == M_DUOTONE || mode == M_MULTI; }

/* Legacy (Pascal) names: UTF-8 when valid (GIMP and others write that),
 * otherwise the system code page (taken as Windows-1252). */
static void name_from_pascal(char *out, size_t outn, const u8 *p, size_t n)
{
    size_t i, o = 0, nu = 0;
    u16 units[256];
    int utf8 = 0;
    for (i = 0; i < n; ) {                     /* validate UTF-8 */
        unsigned c = p[i], cp, k, len;
        if (c < 0x80) { units[nu++] = (u16)c; i++; continue; }
        len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (!len || i + len > n) { utf8 = -1; break; }
        cp = c & (0x7F >> len);
        for (k = 1; k < len; k++) {
            if ((p[i + k] & 0xC0) != 0x80) { utf8 = -1; break; }
            cp = cp << 6 | (p[i + k] & 0x3F);
        }
        if (utf8 < 0 || cp < 0x80 || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000) || cp > 0x10FFFF) { utf8 = -1; break; }
        units[nu++] = cp >= 0x10000 ? 0xD800 : (u16)cp;   /* astral: shown as '?' */
        if (cp >= 0x10000) units[nu++] = 0xDC00;
        utf8 = 1;
        i += len;
    }
    if (utf8 > 0) { pcio_utf16_to_ansi(units, nu, out, outn); return; }
    for (i = 0; i < n && o + 1 < outn; i++) out[o++] = (char)(p[i] < 32 ? '?' : p[i]);
    out[o] = 0;
}

static void name_from_unicode(char *out, size_t outn, PcRd *r)
{
    u32 cnt = rd_u32be(r), i;
    u16 units[256];
    size_t n = 0;
    if (r->err || (unsigned long long)cnt * 2 > rd_left(r)) return;
    for (i = 0; i < cnt; i++) {
        unsigned u = rd_u16be(r);
        if (n < 256) units[n++] = (u16)u;
    }
    if (!r->err) pcio_utf16_to_ansi(units, n, out, outn);
}

static int add_chan(PsdCtx *c, int id, u32 len)
{
    if (c->nchans >= c->chans_cap) {
        int nc = c->chans_cap ? c->chans_cap * 2 : 64;
        PsdChan *n = (PsdChan *)realloc(c->chans, sizeof *n * nc);
        if (!n) return -1;
        c->chans = n;
        c->chans_cap = nc;
    }
    c->chans[c->nchans].id = id;
    c->chans[c->nchans].p = NULL;
    c->chans[c->nchans].len = len;
    c->nchans++;
    return 0;
}

static int read_record(PsdCtx *c, PcRd *r, PsdRec *L)
{
    int i;
    u32 extra;
    PcRd x, m;
    const u8 *q;
    L->top = rd_i32be(r); L->left = rd_i32be(r); L->bottom = rd_i32be(r); L->right = rd_i32be(r);
    L->nch = (int)rd_u16be(r);
    if (r->err || L->nch > PSD_MAX_LAYER_CH) return -1;
    L->ch0 = c->nchans;
    for (i = 0; i < L->nch; i++) {
        int id = rd_i16be(r);
        u32 len = rd_u32be(r);
        if (r->err || add_chan(c, id, len)) return -1;
    }
    q = rd_bytes(r, 4);
    if (!q || memcmp(q, "8BIM", 4)) return -1;
    q = rd_bytes(r, 4);
    if (!q) return -1;
    memcpy(L->key, q, 4);
    L->opacity = (int)rd_u8(r);
    L->clipping = (int)rd_u8(r);
    L->flags = (int)rd_u8(r);
    rd_skip(r, 1);
    extra = rd_u32be(r);
    x = rd_sub(r, extra);
    if (r->err) return -1;

    m = rd_sub(&x, rd_u32be(&x));
    L->udens = L->vdens = 255;
    if (!m.err && m.n >= 18) {
        size_t real_at = 18, par_at = 0;
        L->has_mask = 1;
        L->mtop = rd_i32be(&m); L->mleft = rd_i32be(&m); L->mbottom = rd_i32be(&m); L->mright = rd_i32be(&m);
        L->mdef = (int)rd_u8(&m) ? 255 : 0;
        L->mflags = (int)rd_u8(&m);
        /* Optional real-mask fields (18 bytes) and mask parameters (flag bit
         * 4).  Photoshop writes the real fields first although the spec lists
         * the parameters first; tell them apart as crates/psd does, by
         * whether a plausible real-mask background (0 or 255) is at 19. */
        if (L->mflags & 0x10) {
            if (m.n >= 37 && (m.p[19] == 0 || m.p[19] == 255) && !(m.p[36] & 0xF0)) { real_at = 18; par_at = 36; L->has_real = 1; }
            else {
                unsigned pf = m.p[18];
                par_at = 18;
                real_at = 19 + (pf & 1) + 8 * !!(pf & 2) + !!(pf & 4) + 8 * !!(pf & 8);
                L->has_real = m.n >= real_at + 18;
            }
        } else {
            L->has_real = m.n >= 36;
        }
        if (L->has_real) {
            PcRd q;
            rd_init(&q, m.p + real_at, 18);
            L->rflags = (int)rd_u8(&q);
            L->rdef = (int)rd_u8(&q) ? 255 : 0;
            L->rtop = rd_i32be(&q); L->rleft = rd_i32be(&q); L->rbottom = rd_i32be(&q); L->rright = rd_i32be(&q);
        }
        if (par_at && par_at < m.n) {
            unsigned pf = m.p[par_at];
            size_t at = par_at + 1;
            if ((pf & 1) && at < m.n) L->udens = m.p[at++];
            if (pf & 2) at += 8;
            if ((pf & 4) && at < m.n) L->vdens = m.p[at++];
        }
    }
    rd_skip(&x, rd_u32be(&x));                    /* blending ranges */
    {
        unsigned n = rd_u8(&x);
        const u8 *nm = rd_bytes(&x, n);
        if (nm) name_from_pascal(L->name, sizeof L->name, nm, n);
        rd_skip(&x, (4 - (1 + n) % 4) % 4);
    }
    if (x.err) {           /* tolerate a damaged extra-data field */
        x.err = 0;
        x.pos = x.n;
    }
    /* additional layer information */
    while (rd_left(&x) >= 12 && is_sig(x.p + x.pos)) {
        const u8 *key;
        u32 len;
        PcRd b;
        size_t k, left;
        rd_skip(&x, 4);
        key = rd_bytes(&x, 4);
        len = rd_u32be(&x);
        b = rd_sub(&x, len);
        if (x.err) break;
        if (!memcmp(key, "luni", 4)) {
            name_from_unicode(L->name, sizeof L->name, &b);
        } else if (!memcmp(key, "lspf", 4)) {
            u32 v = rd_u32be(&b);
            if (!b.err) L->protect = (int)(v & 7);
        } else if (!memcmp(key, "lsct", 4) || (!memcmp(key, "lsdk", 4) && !L->section)) {
            u32 t = rd_u32be(&b);
            if (!b.err && t >= 1 && t <= 3) L->section = (int)t;
        }
        /* padding: up to 3 bytes before the next signature (or the end) */
        left = rd_left(&x);
        for (k = 0; k <= 3; k++) {
            if (k == left || (k + 4 <= left && is_sig(x.p + x.pos + k))) break;
        }
        if (k > 3) k = (len & 1) && left ? 1 : 0;
        rd_skip(&x, k);
    }
    if (L->opacity > 255) L->opacity = 255;
    return 0;
}

/* Parse a layer info body (count, records, channel data). */
static int parse_layer_info(PsdCtx *c, const u8 *p, size_t n)
{
    PcRd r;
    int count, i, j;
    rd_init(&r, p, n);
    count = rd_i16be(&r);
    if (r.err) return -1;
    if (count < 0) { c->merged_alpha = 1; count = -count; }
    if (count == 0) return 0;
    if ((unsigned long long)count * 34 > rd_left(&r)) return -1;
    c->recs = (PsdRec *)calloc((size_t)count, sizeof *c->recs);
    if (!c->recs) return -1;
    for (i = 0; i < count; i++) {
        if (read_record(c, &r, &c->recs[i])) { c->nrecs = i; return -1; }
    }
    c->nrecs = count;
    /* channel image data follows all records, in the same order */
    for (i = 0; i < count; i++) {
        PsdRec *L = &c->recs[i];
        for (j = 0; j < L->nch; j++) {
            PsdChan *ch = &c->chans[L->ch0 + j];
            size_t len = ch->len, left = rd_left(&r);
            if (len < 2) { ch->len = 0; rd_skip(&r, len <= left ? len : left); continue; }
            if (len > left) len = left;              /* truncated: decode what is there */
            ch->p = len >= 2 ? r.p + r.pos : NULL;
            ch->len = len >= 2 ? len : 0;
            rd_skip(&r, len);
        }
    }
    return 0;
}

static void parse_resources(PsdCtx *c, PcRd *res)
{
    int have_unicode_names = 0;
    while (rd_left(res) >= 12) {
        const u8 *sig = rd_bytes(res, 4);
        unsigned id, nl;
        u32 len;
        PcRd d;
        if (!sig || memcmp(sig, "8BIM", 4)) {
            if (!sig || (memcmp(sig, "MeSa", 4) && memcmp(sig, "AgHg", 4) && memcmp(sig, "PHUT", 4) && memcmp(sig, "DCSR", 4))) break;
        }
        id = rd_u16be(res);
        nl = rd_u8(res);
        rd_skip(res, nl + ((1 + nl) & 1));
        len = rd_u32be(res);
        d = rd_sub(res, len);
        if (res->err) break;
        if (len & 1) rd_skip(res, 1);
        if (res->err) res->err = 0, res->pos = res->n;
        switch (id) {
        case 0x03ED: {          /* ResolutionInfo: 16.16 fixed, always pixels per inch */
            u32 hres = rd_u32be(&d);
            if (!d.err && hres) {
                double dpi = hres / 65536.0;
                if (dpi >= 1 && dpi <= 100000) c->dpi = dpi;
            }
            break;
        }
        case 0x03EE:            /* alpha channel names, Pascal strings */
            if (!have_unicode_names) {
                c->nanames = 0;
                while (rd_left(&d) >= 1 && c->nanames < PC_MAX_CHANNELS) {
                    unsigned n = rd_u8(&d);
                    const u8 *s = rd_bytes(&d, n);
                    if (!s) break;
                    name_from_pascal(c->anames[c->nanames++], 32, s, n);
                }
            }
            break;
        case 0x0415:            /* Unicode alpha channel names */
            have_unicode_names = 1;
            c->nanames = 0;
            while (rd_left(&d) >= 4 && c->nanames < PC_MAX_CHANNELS) {
                char nm[32] = "";
                name_from_unicode(nm, sizeof nm, &d);
                if (d.err) break;
                pc_strlcpy(c->anames[c->nanames++], nm, 32);
            }
            break;
        case 0x0400: {          /* layer state: target layer index */
            unsigned t = rd_u16be(&d);
            if (!d.err) c->target_layer = (int)t;
            break;
        }
        case 0x041D: {          /* alpha identifiers: one u32 per extra channel, 0 = transparency */
            int k = 0;
            c->has_alpha_ids = 1;
            c->alpha_id_zero = -1;
            while (rd_left(&d) >= 4) {
                u32 id = rd_u32be(&d);
                if (id == 0 && c->alpha_id_zero < 0) c->alpha_id_zero = k;
                k++;
            }
            break;
        }
        case 0x0417: {          /* transparency index (indexed colour) */
            unsigned t = rd_u16be(&d);
            if (!d.err && t < 256) c->trans_index = (int)t;
            break;
        }
        default:
            break;
        }
    }
}

static int chan_index(const PsdCtx *c, const PsdRec *L, int id)
{
    int j;
    for (j = 0; j < L->nch; j++) if (c->chans[L->ch0 + j].id == id) return L->ch0 + j;
    return -1;
}

/* Hidden groups hide their layers.  Records run bottom to top: a group's
 * end divider (3) comes first, its folder record (1/2) last. */
static void apply_group_visibility(PsdCtx *c)
{
    int stack[64], sp = 0, hidden = 0, i;
    for (i = c->nrecs - 1; i >= 0; i--) {
        PsdRec *L = &c->recs[i];
        if (L->section == 1 || L->section == 2) {
            if (sp < 64) stack[sp++] = hidden;
            if (L->flags & 2) hidden = 1;
        } else if (L->section == 3) {
            if (sp > 0) hidden = stack[--sp];
        } else {
            L->hidden_by_group = hidden;
        }
    }
}

static PcLayer *build_layer(PsdCtx *c, const PsdRec *L, int bottom)
{
    PcLayer *l;
    u8 *px, *planes[4] = { NULL, NULL, NULL, NULL };
    int j, ia = chan_index(c, L, -1), direct = direct_mode(c->mode);
    long long lw = (long long)L->right - L->left, lh = (long long)L->bottom - L->top;
    size_t n = (size_t)c->w * c->h, i;
    l = pc_layer_new(c->w, c->h, L->name[0] ? L->name : "Layer");
    if (!l) return NULL;
    px = l->pix->px;
    if (!direct) {
        for (j = 0; j < c->ncolor && j < 4; j++) {
            planes[j] = (u8 *)malloc(n);
            if (!planes[j]) goto fail;
            memset(planes[j], c->mode == M_CMYK ? 255 : 0, n);
        }
    }
    if (ia < 0 && lw > 0 && lh > 0) {
        /* no transparency channel: opaque inside the bounds */
        long long y, x, y0 = PC_MAX(L->top, 0), y1 = PC_MIN((long long)L->bottom, c->h);
        long long x0 = PC_MAX(L->left, 0), x1 = PC_MIN((long long)L->right, c->w);
        for (y = y0; y < y1; y++)
            for (x = x0; x < x1; x++) px[((size_t)y * c->w + (size_t)x) * 4 + 3] = 255;
    }
    for (j = 0; j < L->nch; j++) {
        const PsdChan *ch = &c->chans[L->ch0 + j];
        PsdSink s;
        if (!ch->p || lw <= 0 || lh <= 0) continue;
        s.dst = NULL; s.stride = 4; s.off = 0; s.ox = L->left; s.oy = L->top;
        if (ch->id == -1) { s.dst = px; s.off = 3; }
        else if (ch->id >= 0 && ch->id < c->ncolor) {
            if (direct) { if (c->mode == M_RGB) { s.dst = px; s.off = ch->id; } else if (ch->id == 0) s.dst = px; }
            else if (ch->id < 4) { s.dst = planes[ch->id]; s.stride = 1; }
        }
        if (!s.dst) continue;
        if (lw > PSD_MAX_LAYER_DIM || lh > PSD_MAX_LAYER_DIM) continue;
        psd_decode(c, ch->p, ch->len, 1, (int)lw, (int)lh, &s);   /* damaged data stays zero */
    }
    if (!direct) convert_planes(c, planes, px, n);
    else if (c->mode != M_RGB) for (i = 0; i < n; i++) px[i * 4 + 1] = px[i * 4 + 2] = px[i * 4];
    for (j = 0; j < 4; j++) { free(planes[j]); planes[j] = NULL; }

    /* Layer mask.  With both a pixel mask and a vector mask, -3 is the
     * user (pixel) mask with the "real" rectangle and -2 the rasterized
     * vector mask; we have no vector masks, so the layer gets their
     * product (what Photoshop shows).  Mask densities are applied. */
    if (L->has_mask) {
        int iu = L->has_real ? chan_index(c, L, -3) : -1, iv = chan_index(c, L, -2), pass;
        if (iu < 0) { iu = iv; iv = -1; }
        if (iu >= 0) {
            u8 *tmp = NULL;
            l->mask = pc_buf_new(c->w, c->h, 1, 0);
            if (!l->mask) goto fail;
            for (pass = 0; pass < 2; pass++) {
                int ich = pass ? iv : iu, def, dens;
                long long t, le, b, r;
                u8 *dst;
                if (ich < 0) break;
                if (pass == 0 && iu != chan_index(c, L, -2)) { def = L->rdef; t = L->rtop; le = L->rleft; b = L->rbottom; r = L->rright; dens = L->udens; }
                else {
                    /* -2: flag bit 3 says it was rendered from vector data */
                    def = L->mdef; t = L->mtop; le = L->mleft; b = L->mbottom; r = L->mright;
                    dens = (L->mflags & 0x08) ? L->vdens : L->udens;
                }
                if (pass) { tmp = (u8 *)malloc(n); if (!tmp) break; dst = tmp; }
                else dst = l->mask->px;
                memset(dst, def, n);
                if (c->chans[ich].p && r > le && b > t && r - le <= PSD_MAX_LAYER_DIM && b - t <= PSD_MAX_LAYER_DIM) {
                    PsdSink s;
                    s.dst = dst; s.stride = 1; s.off = 0; s.ox = le; s.oy = t;
                    psd_decode(c, c->chans[ich].p, c->chans[ich].len, 1, (int)(r - le), (int)(b - t), &s);
                }
                if (dens < 255) for (i = 0; i < n; i++) dst[i] = (u8)(255 - pc_mul255(255 - dst[i], dens));
                if (pass) for (i = 0; i < n; i++) l->mask->px[i] = (u8)pc_mul255(l->mask->px[i], tmp[i]);
            }
            free(tmp);
            l->mask_enabled = !((iv >= 0 ? L->rflags : L->mflags) & 2);
        }
    }
    l->opacity = L->opacity;
    l->blend = pc_blend_from_psd_key(L->key);
    l->clip = L->clipping != 0;
    l->visible = !(L->flags & 2) && !L->hidden_by_group;
    l->lock_alpha = (L->flags & 1) != 0 || (L->protect & 1);
    l->lock_pixels = (L->protect & 2) != 0;
    l->lock_position = (L->protect & 4) != 0;
    if (bottom && ia < 0 && L->left <= 0 && L->top <= 0 && L->right >= c->w && L->bottom >= c->h) {
        l->is_background = 1;
        l->clip = 0;
        l->lock_alpha = 0;
    }
    return l;
fail:
    for (j = 0; j < 4; j++) free(planes[j]);
    pc_layer_free(l);
    return NULL;
}

/* Minimum number of bytes merged image data of this geometry needs. */
static unsigned long long merged_min_bytes(const PsdCtx *c, int comp)
{
    unsigned long long rb = row_bytes(c->w, c->depth), rows = (unsigned long long)c->nchan * c->h;
    if (comp == 0) return rows * rb;
    if (comp == 1) return rows * 2 + rows * 2 * ((rb + 127) / 128);
    return rows * rb / 1032;
}

PcDoc *pc_psd_load(const u8 *data, size_t n, char *err, size_t errn)
{
    PsdCtx *c;
    PcRd r, lm;
    PcDoc *d = NULL;
    const u8 *q, *merged;
    size_t merged_len;
    u32 len;
    int version, i, kept = 0, comp, extra, first_alpha, nalpha;
    char warn[128] = "";

    c = (PsdCtx *)calloc(1, sizeof *c);
    if (!c) { pcio_seterr(err, errn, "Not enough memory to open the file."); return NULL; }
    c->err = err; c->errn = errn;
    c->dpi = 72;
    c->trans_index = -1;
    c->target_layer = -1;
    rd_init(&r, data, n);
    q = rd_bytes(&r, 4);
    if (!q || memcmp(q, "8BPS", 4)) { pcio_seterr(err, errn, "This is not a Photoshop file."); goto out; }
    version = (int)rd_u16be(&r);
    rd_skip(&r, 6);
    c->nchan = (int)rd_u16be(&r);
    {
        u32 hh = rd_u32be(&r), ww = rd_u32be(&r);     /* (not inside PC_MIN: it evaluates twice) */
        c->h = (int)PC_MIN(hh, 0x7FFFFFFFu);
        c->w = (int)PC_MIN(ww, 0x7FFFFFFFu);
    }
    c->depth = (int)rd_u16be(&r);
    c->mode = (int)rd_u16be(&r);
    if (r.err) { pcio_seterr(err, errn, "The Photoshop file is truncated."); goto out; }
    if (version == 2) { pcio_seterr(err, errn, "Large Document Format (PSB) files are not supported."); goto out; }
    if (version != 1) { pcio_seterr(err, errn, "Unsupported Photoshop file version %d.", version); goto out; }
    if (!pcio_dims_ok(c->w, c->h)) { pcio_seterr(err, errn, "The image is too large (or has no pixels)."); goto out; }
    if (c->nchan < 1 || c->nchan > PSD_MAX_FILE_CH) { pcio_seterr(err, errn, "The Photoshop file is damaged (channel count)."); goto out; }
    if (c->depth != 1 && c->depth != 8 && c->depth != 16 && c->depth != 32) {
        pcio_seterr(err, errn, "Unsupported bit depth (%d bits/channel).", c->depth); goto out;
    }
    switch (c->mode) {
    case M_BITMAP: c->ncolor = 1; if (c->depth != 1) goto badmode; break;
    case M_GRAY: case M_DUOTONE: case M_MULTI: c->ncolor = 1; break;
    case M_INDEXED: c->ncolor = 1; if (c->depth != 8) goto badmode; break;
    case M_RGB: case M_LAB: c->ncolor = 3; break;
    case M_CMYK: c->ncolor = 4; break;
    default:
    badmode:
        pcio_seterr(err, errn, "Unsupported Photoshop colour mode (%d).", c->mode); goto out;
    }
    if (c->depth == 1 && c->mode != M_BITMAP) goto badmode;
    if (c->nchan < c->ncolor) { pcio_seterr(err, errn, "The Photoshop file is damaged (too few channels)."); goto out; }

    /* colour mode data */
    len = rd_u32be(&r);
    q = rd_bytes(&r, len);
    if (!q) { pcio_seterr(err, errn, "The Photoshop file is truncated."); goto out; }
    if (c->mode == M_INDEXED) {
        if (len < 768) { pcio_seterr(err, errn, "The indexed-colour table is missing."); goto out; }
        memcpy(c->palette, q, 768);
        c->has_palette = 1;
    }
    /* image resources */
    {
        PcRd res = rd_sub(&r, rd_u32be(&r));
        if (r.err) { pcio_seterr(err, errn, "The Photoshop file is truncated."); goto out; }
        parse_resources(c, &res);
    }
    /* layer and mask information */
    len = rd_u32be(&r);
    lm = rd_sub(&r, len);
    if (r.err) { pcio_seterr(err, errn, "The Photoshop file is truncated."); goto out; }
    merged = r.p + r.pos;
    merged_len = rd_left(&r);
    comp = merged_len >= 2 ? (int)get_u16be(merged) : -1;
    /* The merged image must be there: that bounds what a small file can make
     * us allocate.  Small canvases with short data are still read (some
     * writers truncate it); what is missing decodes as zeros. */
    if ((comp < 0 || comp > 3 || merged_min_bytes(c, comp) > merged_len - 2) && (size_t)c->w * c->h > ((size_t)1 << 20)) {
        pcio_seterr(err, errn, "The Photoshop file is truncated or damaged.");
        goto out;
    }
    if (len) {
        u32 lilen = rd_u32be(&lm);
        PcRd li = rd_sub(&lm, lilen);
        int parsed = 0;
        if (lm.err) { pcio_seterr(err, errn, "The layer data is damaged."); goto out; }
        if (lilen > 0) {
            if (parse_layer_info(c, li.p, li.n)) { pcio_seterr(err, errn, "The layer data is damaged."); goto out; }
            parsed = 1;
        }
        /* global layer mask, then global blocks (Lr16 / Lr32 hold the layers of deep files) */
        rd_skip(&lm, rd_u32be(&lm));
        while (!lm.err && rd_left(&lm) >= 12 && is_sig(lm.p + lm.pos)) {
            const u8 *key;
            u32 blen;
            PcRd b;
            size_t k, left;
            rd_skip(&lm, 4);
            key = rd_bytes(&lm, 4);
            blen = rd_u32be(&lm);
            b = rd_sub(&lm, blen);
            if (lm.err) break;
            if (!parsed && (!memcmp(key, "Lr16", 4) || !memcmp(key, "Lr32", 4) || !memcmp(key, "Layr", 4))) {
                if (parse_layer_info(c, b.p, b.n)) { pcio_seterr(err, errn, "The layer data is damaged."); goto out; }
                parsed = 1;
                c->layers_in_block = 1;
            } else if (!memcmp(key, "Mtrn", 4) || !memcmp(key, "Mt16", 4) || !memcmp(key, "Mt32", 4)) {
                c->merged_alpha = 1;
            }
            left = rd_left(&lm);
            for (k = 0; k <= 3; k++) if (k == left || (k + 4 <= left && is_sig(lm.p + lm.pos + k))) break;
            if (k <= 3) rd_skip(&lm, k);
            else break;
        }
    }

    d = pcio_doc_alloc(c->w, c->h, (c->mode == M_RGB || c->mode == M_CMYK || c->mode == M_LAB || c->mode == M_INDEXED) ? PC_MODE_RGB : PC_MODE_GRAY, c->dpi);
    if (!d) { pcio_seterr(err, errn, "Not enough memory to open the file."); goto out; }

    /* layers */
    if (c->nrecs > 0) {
        int bottom = 1;
        apply_group_visibility(c);
        for (i = 0; i < c->nrecs; i++) {
            PcLayer *l;
            if (c->recs[i].section) continue;
            if (kept >= PC_MAX_LAYERS) { pc_snprintf(warn, sizeof warn, "Only the first %d layers were loaded.", PC_MAX_LAYERS); break; }
            l = build_layer(c, &c->recs[i], bottom);
            if (!l || pc_doc_insert_layer(d, d->s.nlayers, l)) {
                pcio_seterr(err, errn, "Not enough memory to open the file.");
                pc_doc_free(d); d = NULL; goto out;
            }
            bottom = 0;
            kept++;
        }
        if (c->target_layer >= 0 && c->target_layer < d->s.nlayers) d->s.active = c->target_layer;
        else d->s.active = d->s.nlayers - 1;
    }

    /* merged image: the picture of a flat file, plus the alpha channels.
     * Is its first extra channel the merged transparency?  (Rules of
     * psd-tools / crates/psd: negative layer count or Mtrn block; else the
     * alpha identifiers resource decides; else yes for files without
     * layer records; Photoshop also names it "Transparency".) */
    extra = c->nchan - c->ncolor;
    if (extra > 0 && !c->merged_alpha) {
        if (c->has_alpha_ids) c->merged_alpha = c->alpha_id_zero == 0;
        else if (c->nrecs == 0) c->merged_alpha = 1;
        else if (c->layers_in_block && c->nanames == extra && !strcmp(c->anames[0], "Transparency")) c->merged_alpha = 1;
    }
    if (c->merged_alpha && extra > 0 && c->nanames > 0 && !strcmp(c->anames[0], "Transparency")) {
        /* drop its name so the names line up with the real alpha channels */
        memmove(c->anames[0], c->anames[1], sizeof c->anames[0] * (size_t)(c->nanames - 1));
        c->nanames--;
    }
    first_alpha = c->ncolor + (c->merged_alpha && extra > 0 ? 1 : 0);
    nalpha = PC_MIN(c->nchan - first_alpha, PC_MAX_CHANNELS);
    if (nalpha < 0) nalpha = 0;
    if (kept == 0 || nalpha > 0) {
        PsdSink sinks[PSD_MAX_FILE_CH];
        u8 *planes[4] = { NULL, NULL, NULL, NULL };
        PcBuf *buf = NULL;
        size_t np = (size_t)c->w * c->h, k;
        int p, direct = direct_mode(c->mode), ok = 1;
        memset(sinks, 0, sizeof sinks);
        for (p = 0; p < c->nchan; p++) sinks[p].stride = 1;
        if (kept == 0) {
            buf = pc_buf_new(c->w, c->h, 4, 1);
            if (!buf) ok = 0;
            for (p = 0; ok && p < c->ncolor; p++) {
                if (direct) {
                    if (c->mode == M_RGB || p == 0) { sinks[p].dst = buf->px; sinks[p].stride = 4; sinks[p].off = p; }
                } else if (p < 4) {
                    planes[p] = (u8 *)malloc(np);
                    if (!planes[p]) ok = 0;
                    else { memset(planes[p], c->mode == M_CMYK ? 255 : 0, np); sinks[p].dst = planes[p]; }
                }
            }
            if (ok && c->merged_alpha && extra > 0) { sinks[c->ncolor].dst = buf->px; sinks[c->ncolor].stride = 4; sinks[c->ncolor].off = 3; }
        }
        for (p = 0; ok && p < nalpha; p++) {
            PcChannel *ch = &d->s.channels[p];
            ch->buf = pc_buf_new(c->w, c->h, 1, 1);
            if (!ch->buf) { ok = 0; break; }
            d->s.nchannels = p + 1;
            if (p < c->nanames && c->anames[p][0]) pc_strlcpy(ch->name, c->anames[p], sizeof ch->name);
            else pc_snprintf(ch->name, sizeof ch->name, "Alpha %d", p + 1);
            ch->color = 0xFF0000;
            ch->visible = 0;
            sinks[first_alpha + p].dst = ch->buf->px;
        }
        if (!ok) {
            for (p = 0; p < 4; p++) free(planes[p]);
            pc_buf_unref(buf);
            pcio_seterr(err, errn, "Not enough memory to open the file.");
            pc_doc_free(d); d = NULL; goto out;
        }
        if (psd_decode(c, merged, merged_len, c->nchan, c->w, c->h, sinks)) {
            if (kept == 0) {
                for (p = 0; p < 4; p++) free(planes[p]);
                pc_buf_unref(buf);
                pcio_seterr(err, errn, "The image data is damaged.");
                pc_doc_free(d); d = NULL; goto out;
            }
            /* layered file with a damaged merged image: drop the alpha channels */
            for (p = 0; p < d->s.nchannels; p++) pc_buf_unref(d->s.channels[p].buf);
            d->s.nchannels = 0;
        }
        if (kept == 0) {
            u8 *px = buf->px;
            PcLayer *l;
            int opaque;
            if (!direct) convert_planes(c, planes, px, np);
            else if (c->mode != M_RGB) for (k = 0; k < np; k++) px[k * 4 + 1] = px[k * 4 + 2] = px[k * 4];
            if (c->merged_alpha && extra > 0) {
                /* remove the white matte Photoshop stores transparent composites with */
                for (k = 0; k < np; k++, px += 4) {
                    int a = px[3], j;
                    if (a == 0 || a == 255) continue;
                    for (j = 0; j < 3; j++) {
                        int v = ((px[j] - (255 - a)) * 255 + a / 2) / a;
                        px[j] = (u8)PC_CLAMP(v, 0, 255);
                    }
                }
            } else {
                for (k = 0; k < np; k++) px[k * 4 + 3] = 255;
                if (c->mode == M_INDEXED && c->trans_index >= 0 && planes[0])
                    for (k = 0; k < np; k++) if (planes[0][k] == c->trans_index) px[k * 4 + 3] = 0;
            }
            for (p = 0; p < 4; p++) free(planes[p]);
            opaque = !pcio_has_alpha(buf->px, np);
            l = pcio_layer_from_buf(buf, opaque ? "Background" : "Layer 0");
            if (!l || pc_doc_insert_layer(d, 0, l)) {
                pcio_seterr(err, errn, "Not enough memory to open the file.");
                pc_doc_free(d); d = NULL; goto out;
            }
            l->is_background = opaque;
        }
    }
    pcio_finish(d, PC_FMT_PSD);
    if (warn[0]) pcio_seterr(err, errn, "%s", warn);
out:
    if (c) { free(c->recs); free(c->chans); free(c); }
    return d;
}

/* ======================================================================
 * Writing
 * ====================================================================== */

typedef struct {
    int id;
    PcWr data;            /* compression field + data */
} OutChan;

typedef struct {
    int top, left, bottom, right;
    int nch;
    OutChan ch[5];
    int has_mask, mtop, mleft, mbottom, mright, mdef, mdisabled;
} OutLayer;

/* RLE-encode a w x h plane (compression field first). */
static void rle_plane(PcWr *out, const u8 *plane, int w, int h, u8 *tmp)
{
    size_t counts_at, y;
    wr_u16be(out, 1);
    counts_at = out->n;
    wr_fill(out, 0, (size_t)h * 2);
    for (y = 0; y < (size_t)h; y++) {
        size_t k = pcio_packbits_encode(plane + y * w, (size_t)w, tmp);
        if (out->err) return;
        out->p[counts_at + y * 2] = (u8)(k >> 8);
        out->p[counts_at + y * 2 + 1] = (u8)k;
        wr_bytes(out, tmp, k);
    }
}

/* Extract channel k (0..3 RGBA, 4 gray, 5 mask) of a rect into plane. */
static void extract(const u8 *src, int ch, int stride_px, int x0, int y0, int w, int h, u8 *plane)
{
    int x, y;
    for (y = 0; y < h; y++) {
        const u8 *s = src + ((size_t)(y0 + y) * stride_px + x0) * (ch == 5 ? 1 : 4);
        u8 *o = plane + (size_t)y * w;
        if (ch == 5) memcpy(o, s, (size_t)w);
        else if (ch == 4) for (x = 0; x < w; x++, s += 4) *o++ = (u8)pcio_gray_of(s);
        else for (x = 0; x < w; x++, s += 4) *o++ = s[ch];
    }
}

static void bbox_where(const u8 *m, int stride, int off, int w, int h, int not_value, int *r)
{
    int x, y, x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (y = 0; y < h; y++) {
        const u8 *p = m + (size_t)y * w * stride + off;
        int any = 0;
        for (x = 0; x < w; x++, p += stride)
            if (*p != not_value) { any = 1; if (x < x0) x0 = x; if (x > x1) x1 = x; }
        if (any) { if (y < y0) y0 = y; y1 = y; }
    }
    if (x1 < 0) { r[0] = r[1] = r[2] = r[3] = 0; return; }
    r[0] = y0; r[1] = x0; r[2] = y1 + 1; r[3] = x1 + 1;   /* top, left, bottom, right */
}

static void write_unicode(PcWr *w, const char *s)
{
    u16 u[256];
    size_t n = pcio_ansi_to_utf16(s, u, 255), i;
    wr_u32be(w, (u32)n);
    for (i = 0; i < n; i++) wr_u16be(w, u[i]);
}

static void write_pascal(PcWr *w, const char *s, int align)
{
    size_t n = strlen(s), total;
    if (n > 255) n = 255;
    wr_u8(w, (unsigned)n);
    wr_bytes(w, s, n);
    total = 1 + n;
    wr_fill(w, 0, (align - total % align) % align);
}

static void write_resource(PcWr *w, unsigned id, const PcWr *data)
{
    wr_bytes(w, "8BIM", 4);
    wr_u16be(w, id);
    wr_u16be(w, 0);                         /* empty name, padded to even */
    wr_u32be(w, (u32)data->n);
    wr_bytes(w, data->p, data->n);
    if (data->n & 1) wr_u8(w, 0);
}

int pc_psd_save(const PcDoc *d, u8 **out, size_t *outn, char *err, size_t errn)
{
    int W = d->s.w, H = d->s.h, gray = d->s.mode == PC_MODE_GRAY, cc = gray ? 1 : 3;
    int nl = d->s.nlayers, nalpha = d->s.nchannels, has_alpha, i, j, k, nch, ok = 0;
    size_t np = (size_t)W * H;
    PcBuf *comp = NULL;
    OutLayer *ol = NULL;
    u8 *plane = NULL, *tmp = NULL;
    PcWr w, sub;
    size_t lm_at, li_at, li_start;

    memset(&w, 0, sizeof w);
    memset(&sub, 0, sizeof sub);
    *out = NULL; *outn = 0;
    if (nalpha > PSD_MAX_FILE_CH - 4) nalpha = PSD_MAX_FILE_CH - 4;
    comp = pcio_composite(d, err, errn);
    if (!comp) return -1;
    has_alpha = pcio_has_alpha(comp->px, np);
    plane = (u8 *)pc_alloc_checked((size_t)W, (size_t)H, 1, 0);
    tmp = (u8 *)malloc((size_t)W + (size_t)W / 128 + 16);
    ol = (OutLayer *)calloc((size_t)(nl > 0 ? nl : 1), sizeof *ol);
    if (!plane || !tmp || !ol) goto oom;

    /* encode every layer's channels first: records need their lengths */
    for (i = 0; i < nl; i++) {
        const PcLayer *l = d->s.layers[i];
        OutLayer *o = &ol[i];
        int r[4], bg, lw, lh;
        bg = l->is_background && i == 0 && !pcio_has_alpha(l->pix->px, np);
        if (bg) { r[0] = 0; r[1] = 0; r[2] = H; r[3] = W; }
        else bbox_where(l->pix->px, 4, 3, W, H, 0, r);
        o->top = r[0]; o->left = r[1]; o->bottom = r[2]; o->right = r[3];
        lw = r[3] - r[1]; lh = r[2] - r[0];
        o->nch = 0;
        if (!bg) o->ch[o->nch++].id = -1;
        for (j = 0; j < cc; j++) o->ch[o->nch++].id = j;
        for (j = 0; j < o->nch; j++) {
            OutChan *ch = &o->ch[j];
            if (lw <= 0 || lh <= 0) { wr_u16be(&ch->data, 0); continue; }     /* empty layer */
            extract(l->pix->px, ch->id == -1 ? 3 : (gray ? 4 : ch->id), W, r[1], r[0], lw, lh, plane);
            rle_plane(&ch->data, plane, lw, lh, tmp);
            if (ch->data.err) goto oom;
        }
        if (l->mask) {
            int ra[4], rb[4], aa, ab;
            OutChan *ch = &o->ch[o->nch++];
            ch->id = -2;
            bbox_where(l->mask->px, 1, 0, W, H, 255, ra);
            bbox_where(l->mask->px, 1, 0, W, H, 0, rb);
            aa = (ra[2] - ra[0]) * (ra[3] - ra[1]);
            ab = (rb[2] - rb[0]) * (rb[3] - rb[1]);
            o->has_mask = 1;
            o->mdisabled = !l->mask_enabled;
            o->mdef = aa <= ab ? 255 : 0;
            o->mtop = (o->mdef == 255 ? ra : rb)[0]; o->mleft = (o->mdef == 255 ? ra : rb)[1];
            o->mbottom = (o->mdef == 255 ? ra : rb)[2]; o->mright = (o->mdef == 255 ? ra : rb)[3];
            lw = o->mright - o->mleft; lh = o->mbottom - o->mtop;
            if (lw <= 0 || lh <= 0) wr_u16be(&ch->data, 0);
            else {
                extract(l->mask->px, 5, W, o->mleft, o->mtop, lw, lh, plane);
                rle_plane(&ch->data, plane, lw, lh, tmp);
            }
            if (ch->data.err) goto oom;
        }
    }

    /* header */
    nch = cc + has_alpha + nalpha;
    wr_bytes(&w, "8BPS", 4);
    wr_u16be(&w, 1);
    wr_fill(&w, 0, 6);
    wr_u16be(&w, (unsigned)nch);
    wr_u32be(&w, (u32)H);
    wr_u32be(&w, (u32)W);
    wr_u16be(&w, 8);
    wr_u16be(&w, gray ? M_GRAY : M_RGB);
    wr_u32be(&w, 0);                                /* colour mode data */

    /* image resources */
    {
        size_t at = w.n;
        u32 fx = pcio_uround(pcio_doc_dpi(d) * 65536.0);
        wr_u32be(&w, 0);
        sub.n = 0;
        wr_u32be(&sub, fx); wr_u16be(&sub, 1); wr_u16be(&sub, 1);
        wr_u32be(&sub, fx); wr_u16be(&sub, 1); wr_u16be(&sub, 1);
        write_resource(&w, 0x03ED, &sub);
        if (nalpha > 0) {
            sub.n = 0;
            for (i = 0; i < nalpha; i++) write_pascal(&sub, d->s.channels[i].name, 1);
            write_resource(&w, 0x03EE, &sub);
            sub.n = 0;
            for (i = 0; i < nalpha; i++) write_unicode(&sub, d->s.channels[i].name);
            write_resource(&w, 0x0415, &sub);
            sub.n = 0;                              /* alpha identifiers (non-zero: not transparency) */
            for (i = 0; i < nalpha; i++) wr_u32be(&sub, (u32)(10 + i));
            write_resource(&w, 0x041D, &sub);
        }
        if (nl > 0) {
            sub.n = 0;
            wr_u16be(&sub, (unsigned)PC_CLAMP(d->s.active, 0, nl - 1));
            write_resource(&w, 0x0400, &sub);
        }
        sub.n = 0;                                  /* version info: real merged data present */
        wr_u32be(&sub, 1); wr_u8(&sub, 1);
        write_unicode(&sub, "PhotoCraft Classic");
        write_unicode(&sub, "PhotoCraft Classic");
        wr_u32be(&sub, 1);
        write_resource(&w, 0x0421, &sub);
        if (sub.err) goto oom;
        wr_put_u32be(&w, at, (u32)(w.n - at - 4));
    }

    /* layer and mask information */
    lm_at = w.n;
    wr_u32be(&w, 0);
    li_at = w.n;
    wr_u32be(&w, 0);
    li_start = w.n;
    if (nl > 0) {
        wr_u16be(&w, (unsigned)(has_alpha ? (0x10000 - nl) & 0xFFFF : nl));
        for (i = 0; i < nl; i++) {
            const PcLayer *l = d->s.layers[i];
            OutLayer *o = &ol[i];
            size_t ex_at, ex_start;
            int flags = 8;
            wr_u32be(&w, (u32)o->top); wr_u32be(&w, (u32)o->left);
            wr_u32be(&w, (u32)o->bottom); wr_u32be(&w, (u32)o->right);
            wr_u16be(&w, (unsigned)o->nch);
            for (j = 0; j < o->nch; j++) {
                wr_u16be(&w, (unsigned)(o->ch[j].id & 0xFFFF));
                wr_u32be(&w, (u32)o->ch[j].data.n);
            }
            wr_bytes(&w, "8BIM", 4);
            wr_bytes(&w, pc_blend_psd_key(l->blend), 4);
            wr_u8(&w, (unsigned)PC_CLAMP(l->opacity, 0, 255));
            wr_u8(&w, l->clip && i > 0 ? 1 : 0);
            if (l->lock_alpha) flags |= 1;
            if (!l->visible) flags |= 2;
            wr_u8(&w, (unsigned)flags);
            wr_u8(&w, 0);
            ex_at = w.n;
            wr_u32be(&w, 0);
            ex_start = w.n;
            if (o->has_mask) {
                wr_u32be(&w, 20);
                wr_u32be(&w, (u32)o->mtop); wr_u32be(&w, (u32)o->mleft);
                wr_u32be(&w, (u32)o->mbottom); wr_u32be(&w, (u32)o->mright);
                wr_u8(&w, (unsigned)o->mdef);
                wr_u8(&w, o->mdisabled ? 2 : 0);
                wr_u16be(&w, 0);
            } else {
                wr_u32be(&w, 0);
            }
            wr_u32be(&w, (u32)(cc + 1) * 8);       /* blending ranges: full */
            for (k = 0; k <= cc; k++) { wr_u16be(&w, 0x0000); wr_u16be(&w, 0xFFFF); wr_u16be(&w, 0x0000); wr_u16be(&w, 0xFFFF); }
            write_pascal(&w, l->name, 4);
            /* 'luni': Unicode name, data padded to 4 */
            sub.n = 0;
            write_unicode(&sub, l->name);
            while (sub.n % 4) wr_u8(&sub, 0);
            wr_bytes(&w, "8BIMluni", 8);
            wr_u32be(&w, (u32)sub.n);
            wr_bytes(&w, sub.p, sub.n);
            /* 'lspf': protection (locks) */
            if (l->lock_alpha || l->lock_pixels || l->lock_position) {
                wr_bytes(&w, "8BIMlspf", 8);
                wr_u32be(&w, 4);
                wr_u32be(&w, (u32)((l->lock_alpha ? 1 : 0) | (l->lock_pixels ? 2 : 0) | (l->lock_position ? 4 : 0)));
            }
            /* 'lyid': layer id */
            wr_bytes(&w, "8BIMlyid", 8);
            wr_u32be(&w, 4);
            wr_u32be(&w, (u32)(i + 1));
            wr_put_u32be(&w, ex_at, (u32)(w.n - ex_start));
        }
        for (i = 0; i < nl; i++)
            for (j = 0; j < ol[i].nch; j++) wr_bytes(&w, ol[i].ch[j].data.p, ol[i].ch[j].data.n);
        if ((w.n - li_start) & 1) wr_u8(&w, 0);
        wr_put_u32be(&w, li_at, (u32)(w.n - li_start));
    }
    wr_u32be(&w, 0);                                /* global layer mask info */
    wr_put_u32be(&w, lm_at, (u32)(w.n - lm_at - 4));

    /* merged image: all row counts first, then the rows */
    {
        PcWr counts, rows;
        int p;
        memset(&counts, 0, sizeof counts);
        memset(&rows, 0, sizeof rows);
        for (p = 0; p < nch; p++) {
            size_t y, q;
            if (p < cc) {
                const u8 *s = comp->px;
                for (q = 0; q < np; q++, s += 4) {
                    int v = gray ? s[0] : s[p], a = s[3];
                    plane[q] = (u8)(has_alpha && a != 255 ? (v * a + 127) / 255 + 255 - a : v);
                }
            } else if (has_alpha && p == cc) {
                for (q = 0; q < np; q++) plane[q] = comp->px[q * 4 + 3];
            } else {
                const PcChannel *ch = &d->s.channels[p - cc - has_alpha];
                if (ch->buf && ch->buf->w == W && ch->buf->h == H) memcpy(plane, ch->buf->px, np);
                else memset(plane, 0, np);
            }
            for (y = 0; y < (size_t)H; y++) {
                size_t kk = pcio_packbits_encode(plane + y * W, (size_t)W, tmp);
                wr_u16be(&counts, (unsigned)kk);
                wr_bytes(&rows, tmp, kk);
            }
        }
        wr_u16be(&w, 1);
        wr_bytes(&w, counts.p, counts.n);
        wr_bytes(&w, rows.p, rows.n);
        if (counts.err || rows.err) w.err = 1;
        wr_free(&counts);
        wr_free(&rows);
    }
    ok = !w.err;
    goto done;
oom:
    ok = 0;
done:
    if (ol) for (i = 0; i < nl; i++) for (j = 0; j < 5; j++) wr_free(&ol[i].ch[j].data);
    free(ol); free(plane); free(tmp);
    wr_free(&sub);
    pc_buf_unref(comp);
    if (!ok) { wr_free(&w); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    return wr_finish(&w, out, outn, err, errn);
}
