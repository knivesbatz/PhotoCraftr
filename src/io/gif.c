/*
 * CompuServe GIF.  Loading goes through stb_image (first frame of an
 * animation, transparency kept).  Saving is our own GIF89a encoder:
 *
 *  - images with at most 256 colours (255 plus a transparent index) keep
 *    them exactly; others are reduced by median cut on a 15-bit histogram
 *    with optional Floyd-Steinberg diffusion (gif_dither);
 *  - fully or mostly transparent pixels (alpha < 128) become the
 *    transparent index when gif_transparency is on; partial alpha is
 *    matted against white like Photoshop's GIF89a Export;
 *  - Grayscale documents without transparency get a 256-level gray
 *    palette;
 *  - LZW with variable code width (GIF flavour: no early change), clear
 *    code when the 4096-entry table fills, optional interlacing.
 */
#include "io/ioint.h"

PcDoc *pc_gif_load(const u8 *data, size_t n, char *err, size_t errn)
{
    int w, h, comp;
    u8 *px;
    if (n < 13 || memcmp(data, "GIF8", 4)) { pcio_seterr(err, errn, "This is not a GIF file."); return NULL; }
    if (!pcio_stbi_info(data, n, &w, &h, &comp)) { pcio_seterr(err, errn, "The GIF file is damaged or unsupported."); return NULL; }
    if (!pcio_dims_ok(w, h)) { pcio_seterr(err, errn, "The image is too large."); return NULL; }
    /* an LZW code (>= 3 bits) expands to at most 4096 pixels */
    if (!pcio_plausible(w, h, n, 8192)) { pcio_seterr(err, errn, "The GIF file is truncated or damaged."); return NULL; }
    px = pcio_stbi_load(data, n, &w, &h, &comp, err, errn);
    if (!px) return NULL;
    return pcio_doc_from_buf(pcio_buf_wrap(px, w, h), PC_MODE_RGB, 72, PC_FMT_GIF, err, errn);
}

/* ---------------- quantization ---------------- */

typedef struct {
    int lo[3], hi[3];       /* inclusive 5-bit bounds */
    unsigned long long count;
} Box;

static void box_shrink(Box *b, const u32 *hist)
{
    int lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 }, r, g, bb;
    unsigned long long cnt = 0;
    for (r = b->lo[0]; r <= b->hi[0]; r++)
        for (g = b->lo[1]; g <= b->hi[1]; g++)
            for (bb = b->lo[2]; bb <= b->hi[2]; bb++) {
                u32 c = hist[(r << 10) | (g << 5) | bb];
                if (!c) continue;
                cnt += c;
                lo[0] = PC_MIN(lo[0], r); hi[0] = PC_MAX(hi[0], r);
                lo[1] = PC_MIN(lo[1], g); hi[1] = PC_MAX(hi[1], g);
                lo[2] = PC_MIN(lo[2], bb); hi[2] = PC_MAX(hi[2], bb);
            }
    b->count = cnt;
    if (cnt) { memcpy(b->lo, lo, sizeof lo); memcpy(b->hi, hi, sizeof hi); }
}

/* Median cut over a 32x32x32 histogram; returns the palette size. */
static int median_cut(const u32 *hist, int maxc, u8 *pal,
                      const unsigned long long *sr, const unsigned long long *sg, const unsigned long long *sb)
{
    Box *boxes = (Box *)malloc(sizeof(Box) * (size_t)maxc);
    int nb = 1, i, k;
    if (!boxes) return 0;
    boxes[0].lo[0] = boxes[0].lo[1] = boxes[0].lo[2] = 0;
    boxes[0].hi[0] = boxes[0].hi[1] = boxes[0].hi[2] = 31;
    box_shrink(&boxes[0], hist);
    while (nb < maxc) {
        int best = -1, axis = 0;
        unsigned long long score = 0;
        for (i = 0; i < nb; i++) {
            int a, len = 0, ax = 0;
            for (a = 0; a < 3; a++) if (boxes[i].hi[a] - boxes[i].lo[a] > len) { len = boxes[i].hi[a] - boxes[i].lo[a]; ax = a; }
            if (len == 0) continue;
            if (boxes[i].count * (unsigned long long)len > score) { score = boxes[i].count * (unsigned long long)len; best = i; axis = ax; }
        }
        if (best < 0) break;
        {
            Box *b = &boxes[best], nbx;
            unsigned long long half = b->count / 2, acc = 0;
            int cut = b->lo[axis], v;
            for (v = b->lo[axis]; v < b->hi[axis]; v++) {
                int r, g, bb;
                for (r = (axis == 0 ? v : b->lo[0]); r <= (axis == 0 ? v : b->hi[0]); r++)
                    for (g = (axis == 1 ? v : b->lo[1]); g <= (axis == 1 ? v : b->hi[1]); g++)
                        for (bb = (axis == 2 ? v : b->lo[2]); bb <= (axis == 2 ? v : b->hi[2]); bb++)
                            acc += hist[(r << 10) | (g << 5) | bb];
                cut = v;
                if (acc >= half) break;
            }
            nbx = *b;
            b->hi[axis] = cut;
            nbx.lo[axis] = cut + 1;
            box_shrink(b, hist);
            box_shrink(&nbx, hist);
            if (!nbx.count || !b->count) break;     /* cannot happen with a proper cut */
            boxes[nb++] = nbx;
        }
    }
    for (i = 0; i < nb; i++) {
        unsigned long long tr = 0, tg = 0, tb = 0, cnt = 0;
        int r, g, bb;
        for (r = boxes[i].lo[0]; r <= boxes[i].hi[0]; r++)
            for (g = boxes[i].lo[1]; g <= boxes[i].hi[1]; g++)
                for (bb = boxes[i].lo[2]; bb <= boxes[i].hi[2]; bb++) {
                    int c = (r << 10) | (g << 5) | bb;
                    if (!hist[c]) continue;
                    cnt += hist[c]; tr += sr[c]; tg += sg[c]; tb += sb[c];
                }
        if (!cnt) cnt = 1;
        k = i * 3;
        pal[k] = (u8)((tr + cnt / 2) / cnt);
        pal[k + 1] = (u8)((tg + cnt / 2) / cnt);
        pal[k + 2] = (u8)((tb + cnt / 2) / cnt);
    }
    free(boxes);
    return nb;
}

static int nearest(const u8 *pal, int n, int r, int g, int b)
{
    int i, best = 0;
    long bd = 0x7FFFFFFF;
    for (i = 0; i < n; i++) {
        long dr = r - pal[i * 3], dg = g - pal[i * 3 + 1], db = b - pal[i * 3 + 2];
        long dd = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if (dd < bd) { bd = dd; best = i; }
    }
    return best;
}

/* Exact palette when the image has few colours: returns count or -1. */
typedef struct { u32 key; int idx; } HEnt;

static int exact_palette(const u8 *rgba, size_t np, const u8 *transp, int maxc, u8 *pal, u8 *out, int tindex)
{
    HEnt *tab = (HEnt *)calloc(1024, sizeof *tab);   /* open addressing, <= 256 entries */
    size_t i;
    int n = 0;
    if (!tab) return -2;
    for (i = 0; i < np; i++) {
        u32 key, hsh;
        const u8 *p = rgba + i * 4;
        if (transp[i]) { out[i] = (u8)tindex; continue; }
        key = ((u32)p[0] << 16 | (u32)p[1] << 8 | p[2]) + 1;
        hsh = (key * 2654435761u) >> 22;
        while (tab[hsh].key && tab[hsh].key != key) hsh = (hsh + 1) & 1023;
        if (!tab[hsh].key) {
            if (n >= maxc) { free(tab); return -1; }
            tab[hsh].key = key;
            tab[hsh].idx = n;
            pal[n * 3] = p[0]; pal[n * 3 + 1] = p[1]; pal[n * 3 + 2] = p[2];
            n++;
        }
        out[i] = (u8)tab[hsh].idx;
    }
    free(tab);
    return n;
}

/* ---------------- LZW (GIF flavour) ---------------- */

typedef struct {
    PcWr *w;
    u8 block[256];
    int nblock;
    u32 acc;
    int nacc;
} GifBits;

static void gb_byte(GifBits *g, unsigned b)
{
    g->block[1 + g->nblock++] = (u8)b;
    if (g->nblock == 255) { g->block[0] = 255; wr_bytes(g->w, g->block, 256); g->nblock = 0; }
}

static void gb_code(GifBits *g, unsigned code, int nbits)
{
    g->acc |= (u32)code << g->nacc;
    g->nacc += nbits;
    while (g->nacc >= 8) { gb_byte(g, g->acc & 255); g->acc >>= 8; g->nacc -= 8; }
}

static void gb_flush(GifBits *g)
{
    if (g->nacc > 0) gb_byte(g, g->acc & 255);
    g->acc = 0; g->nacc = 0;
    if (g->nblock) { g->block[0] = (u8)g->nblock; wr_bytes(g->w, g->block, (size_t)g->nblock + 1); g->nblock = 0; }
    wr_u8(g->w, 0);                     /* block terminator */
}

enum { HSIZE = 8192 };

static int lzw_gif(PcWr *w, const u8 *idx, const int *order, int W, int H, int mincode)
{
    u32 *keys = (u32 *)malloc(sizeof(u32) * HSIZE);
    u16 *vals = (u16 *)malloc(sizeof(u16) * HSIZE);
    GifBits g;
    int clear = 1 << mincode, eoi = clear + 1, next, nbits, ent = -1, y, x;
    if (!keys || !vals) { free(keys); free(vals); return -1; }
    memset(&g, 0, sizeof g);
    g.w = w;
    wr_u8(w, (unsigned)mincode);
    memset(keys, 0, sizeof(u32) * HSIZE);
    next = clear + 2;
    nbits = mincode + 1;
    gb_code(&g, (unsigned)clear, nbits);
    for (y = 0; y < H; y++) {
        const u8 *row = idx + (size_t)order[y] * W;
        for (x = 0; x < W; x++) {
            int c = row[x];
            u32 key, hsh;
            if (ent < 0) { ent = c; continue; }
            key = ((u32)ent << 8 | (u32)c) + 1;
            hsh = (key * 2654435761u) >> 19;
            while (keys[hsh] && keys[hsh] != key) hsh = (hsh + 1) & (HSIZE - 1);
            if (keys[hsh]) { ent = vals[hsh]; continue; }
            gb_code(&g, (unsigned)ent, nbits);
            if (next < 4096) {
                keys[hsh] = key;
                vals[hsh] = (u16)next;
                next++;
                if (next > (1 << nbits) && nbits < 12) nbits++;
            }
            if (next >= 4096) {
                gb_code(&g, (unsigned)clear, nbits);
                memset(keys, 0, sizeof(u32) * HSIZE);
                next = clear + 2;
                nbits = mincode + 1;
            }
            ent = c;
        }
    }
    if (ent >= 0) {
        gb_code(&g, (unsigned)ent, nbits);
        /* the decoder adds an entry for this code before reading EOI */
        if (next < 4096) { next++; if (next > (1 << nbits) && nbits < 12) nbits++; }
    }
    gb_code(&g, (unsigned)eoi, nbits);
    gb_flush(&g);
    free(keys); free(vals);
    return w->err ? -1 : 0;
}

/* ---------------- encoder ---------------- */

int pc_gif_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcBuf *c;
    int W = d->s.w, H = d->s.h, gray = d->s.mode == PC_MODE_GRAY;
    int want_t = o ? o->gif_transparency : 1, dither = o ? o->gif_dither : 1, inter = o ? o->gif_interlace : 0;
    size_t np = (size_t)W * H, i;
    u8 *idx = NULL, *transp = NULL, pal[768];
    int *order = NULL, ncol = 0, tindex = -1, any_t = 0, palbits, y, x, rc = -1;
    PcWr w;
    memset(&w, 0, sizeof w);
    memset(pal, 0, sizeof pal);
    *out = NULL; *outn = 0;
    c = pcio_composite(d, err, errn);
    if (!c) return -1;
    idx = (u8 *)pc_alloc_checked(np, 1, 1, 0);
    transp = (u8 *)pc_alloc_checked(np, 1, 1, 1);
    order = (int *)malloc(sizeof(int) * (size_t)H);
    if (!idx || !transp || !order) goto oom;
    if (want_t)
        for (i = 0; i < np; i++) if (c->px[i * 4 + 3] < 128) { transp[i] = 1; any_t = 1; }
    pcio_matte_white(c->px, np);

    if (gray && !any_t) {
        for (i = 0; i < 256; i++) pal[i * 3] = pal[i * 3 + 1] = pal[i * 3 + 2] = (u8)i;
        for (i = 0; i < np; i++) idx[i] = c->px[i * 4];
        ncol = 256;
    } else {
        int maxc = any_t ? 255 : 256;
        ncol = exact_palette(c->px, np, transp, maxc, pal, idx, maxc);
        if (ncol == -2) goto oom;
        if (ncol < 0) {
            /* median cut on a 15-bit histogram */
            u32 *hist = (u32 *)calloc(32768, sizeof(u32));
            unsigned long long *sr = (unsigned long long *)calloc(32768, sizeof *sr);
            unsigned long long *sg = (unsigned long long *)calloc(32768, sizeof *sg);
            unsigned long long *sb = (unsigned long long *)calloc(32768, sizeof *sb);
            short *inv = (short *)malloc(sizeof(short) * 32768);
            int *errbuf = NULL;
            if (!hist || !sr || !sg || !sb || !inv) { free(hist); free(sr); free(sg); free(sb); free(inv); goto oom; }
            for (i = 0; i < np; i++) {
                const u8 *p = c->px + i * 4;
                int k = (p[0] >> 3) << 10 | (p[1] >> 3) << 5 | (p[2] >> 3);
                if (transp[i]) continue;
                if (hist[k] < 0xFFFFFFFFu) hist[k]++;
                sr[k] += p[0]; sg[k] += p[1]; sb[k] += p[2];
            }
            ncol = median_cut(hist, maxc, pal, sr, sg, sb);
            free(hist); free(sr); free(sg); free(sb);
            if (ncol <= 0) { free(inv); goto oom; }
            for (i = 0; i < 32768; i++) inv[i] = -1;
            if (dither) {
                /* Floyd-Steinberg; errors kept for this row and the next (x16) */
                errbuf = (int *)calloc((size_t)(W + 2) * 6, sizeof(int));
                if (!errbuf) { free(inv); goto oom; }
            }
            for (y = 0; y < H; y++) {
                int *cur = errbuf ? errbuf + ((y & 1) ? 3 * (W + 2) : 0) : NULL;
                int *nxt = errbuf ? errbuf + ((y & 1) ? 0 : 3 * (W + 2)) : NULL;
                if (nxt) memset(nxt, 0, sizeof(int) * 3 * (W + 2));
                for (x = 0; x < W; x++) {
                    size_t k = (size_t)y * W + x;
                    const u8 *p = c->px + k * 4;
                    int v[3], ch, key, best;
                    if (transp[k]) { idx[k] = (u8)maxc; continue; }
                    for (ch = 0; ch < 3; ch++) {
                        int t = p[ch] + (cur ? (cur[(x + 1) * 3 + ch] + 8) >> 4 : 0);
                        v[ch] = PC_CLAMP(t, 0, 255);
                    }
                    key = (v[0] >> 3) << 10 | (v[1] >> 3) << 5 | (v[2] >> 3);
                    if (inv[key] < 0) inv[key] = (short)nearest(pal, ncol, (v[0] & ~7) | 4, (v[1] & ~7) | 4, (v[2] & ~7) | 4);
                    best = inv[key];
                    idx[k] = (u8)best;
                    if (cur) {
                        for (ch = 0; ch < 3; ch++) {
                            int e = v[ch] - pal[best * 3 + ch];
                            cur[(x + 2) * 3 + ch] += e * 7;
                            nxt[x * 3 + ch] += e * 3;
                            nxt[(x + 1) * 3 + ch] += e * 5;
                            nxt[(x + 2) * 3 + ch] += e;
                        }
                    }
                }
            }
            free(errbuf);
            free(inv);
        }
        if (any_t) { tindex = maxc == 255 ? (ncol < 255 ? ncol : 255) : -1; }
        if (any_t && tindex != maxc) {
            /* transparent pixels were indexed as maxc: move them to tindex */
            for (i = 0; i < np; i++) if (transp[i]) idx[i] = (u8)tindex;
        }
        if (tindex >= 0 && tindex + 1 > ncol) ncol = tindex + 1;
    }
    for (palbits = 1; (1 << palbits) < ncol; palbits++) {}

    /* row order (interlaced: rows 0,8,..; 4,12,..; 2,6,..; 1,3,..) */
    if (inter) {
        static const int start[4] = { 0, 4, 2, 1 }, step[4] = { 8, 8, 4, 2 };
        int p, k = 0;
        for (p = 0; p < 4; p++) for (y = start[p]; y < H; y += step[p]) order[k++] = y;
    } else {
        for (y = 0; y < H; y++) order[y] = y;
    }

    wr_bytes(&w, "GIF89a", 6);
    wr_u16le(&w, (unsigned)W);
    wr_u16le(&w, (unsigned)H);
    wr_u8(&w, 0x80 | 0x70 | (unsigned)(palbits - 1));
    wr_u8(&w, 0);                              /* background index */
    wr_u8(&w, 0);                              /* aspect */
    wr_bytes(&w, pal, (size_t)3 << palbits);
    if (tindex >= 0) {
        u8 gce[8] = { 0x21, 0xF9, 4, 1, 0, 0, 0, 0 };
        gce[6] = (u8)tindex;
        wr_bytes(&w, gce, 8);
    }
    wr_u8(&w, 0x2C);
    wr_u16le(&w, 0); wr_u16le(&w, 0);
    wr_u16le(&w, (unsigned)W); wr_u16le(&w, (unsigned)H);
    wr_u8(&w, inter ? 0x40 : 0);
    if (lzw_gif(&w, idx, order, W, H, palbits < 2 ? 2 : palbits)) goto oom;
    wr_u8(&w, 0x3B);
    rc = 0;
oom:
    free(idx); free(transp); free(order);
    pc_buf_unref(c);
    if (rc) { wr_free(&w); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    return wr_finish(&w, out, outn, err, errn);
}
