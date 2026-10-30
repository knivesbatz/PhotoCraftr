/*
 * Colour reduction: Photoshop 5-style palettes and dithers, the game
 * studio colour key, and 16-bit high colour.  See pcquant.h.
 *
 * Median cut works on a 15-bit histogram (32 levels per channel) and
 * averages the real colours in each box.  Nearest-colour searches use a
 * weighted RGB distance (3:4:2) and a 5-6-5 inverse cache; colours that
 * are already in the palette are found exactly through a small hash, so
 * Exact and Document palettes never move a pixel.
 */
#include "core/pcquant.h"

void pc_quant_defaults(PcQuantOpts *q)
{
    memset(q, 0, sizeof *q);
    q->palette = PC_PAL_ADAPTIVE;
    q->colors = 256;
    q->dither = PC_DITHER_DIFFUSION;
    q->transparency = 1;
    q->colorkey = 0;
    q->key_rgb = PC_COLORKEY_DEFAULT;
}

const char *pc_palette_name(int p)
{
    static const char *const names[PC_PAL_COUNT] = {
        "Exact", "System (Windows)", "System (Mac OS)", "Web", "Uniform", "Adaptive", "Document"
    };
    return (p >= 0 && p < PC_PAL_COUNT) ? names[p] : "";
}

const char *pc_dither_name(int d)
{
    static const char *const names[PC_DITHER_COUNT] = { "None", "Pattern", "Diffusion" };
    return (d >= 0 && d < PC_DITHER_COUNT) ? names[d] : "";
}

/* ---------------- fixed palettes ---------------- */

static void put_rgb(u8 *pal, int i, int r, int g, int b) { pal[i * 3] = (u8)r; pal[i * 3 + 1] = (u8)g; pal[i * 3 + 2] = (u8)b; }

/* The 256-colour Windows system palette: the 20 static colours at 0-9 and
 * 246-255 that Windows reserves in every 8-bit display mode, and between
 * them the 6x6x6 colour cube (minus the corners the statics already hold)
 * and a gray ramp, as in the halftone palette. */
static int pal_windows(u8 *pal)
{
    static const u32 lo[10] = { 0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0, 0xC0DCC0, 0xA6CAF0 };
    static const u32 hi[10] = { 0xFFFBF0, 0xA0A0A4, 0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF };
    int i, r, g, b, n = 10, k;
    for (i = 0; i < 10; i++) {
        put_rgb(pal, i, pc_r(lo[i]), pc_g(lo[i]), pc_b(lo[i]));
        put_rgb(pal, 246 + i, pc_r(hi[i]), pc_g(hi[i]), pc_b(hi[i]));
    }
    for (r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++) {
                int corner = (r == 0 || r == 5) && (g == 0 || g == 5) && (b == 0 || b == 5);
                if (corner) continue;          /* black, white and the primaries are statics */
                put_rgb(pal, n++, r * 51, g * 51, b * 51);
            }
    /* 208 cube colours: 28 grays fill slots up to 245 */
    for (k = 1; n < 246; k++) {
        int v = (k * 255 + 14) / 29;
        put_rgb(pal, n++, v, v, v);
    }
    return 256;
}

/* The Mac OS 8-bit system palette: white first, the 6x6x6 cube in
 * descending order, ten-step red, green, blue and gray ramps, black last. */
static int pal_macos(u8 *pal)
{
    static const u8 ramp[10] = { 0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11 };
    int n = 0, r, g, b, k;
    for (r = 5; r >= 0; r--)
        for (g = 5; g >= 0; g--)
            for (b = 5; b >= 0; b--)
                if (r || g || b) put_rgb(pal, n++, r * 51, g * 51, b * 51);
    for (k = 0; k < 10; k++) put_rgb(pal, n++, ramp[k], 0, 0);
    for (k = 0; k < 10; k++) put_rgb(pal, n++, 0, ramp[k], 0);
    for (k = 0; k < 10; k++) put_rgb(pal, n++, 0, 0, ramp[k]);
    for (k = 0; k < 10; k++) put_rgb(pal, n++, ramp[k], ramp[k], ramp[k]);
    put_rgb(pal, n++, 0, 0, 0);
    return n;
}

/* Uniform: an evenly spaced r x g x b grid with r*g*b <= colors, grown a
 * level at a time (green first, then red, then blue; 256 gives 6x7x6). */
static int pal_uniform(int colors, u8 *pal)
{
    static const int order[3] = { 1, 0, 2 };
    int lv[3] = { 1, 1, 1 }, grew = 1, r, g, b, n = 0, k;
    colors = PC_CLAMP(colors, 2, 256);
    if (colors < 8) {                      /* too few for a colour grid: a gray ramp */
        for (k = 0; k < colors; k++) put_rgb(pal, k, k * 255 / (colors - 1), k * 255 / (colors - 1), k * 255 / (colors - 1));
        return colors;
    }
    while (grew) {
        grew = 0;
        for (k = 0; k < 3; k++) {
            int c = order[k], t[3];
            t[0] = lv[0]; t[1] = lv[1]; t[2] = lv[2];
            t[c]++;
            if (t[0] * t[1] * t[2] <= colors) { lv[c]++; grew = 1; }
        }
    }
    for (r = 0; r < lv[0]; r++)
        for (g = 0; g < lv[1]; g++)
            for (b = 0; b < lv[2]; b++)
                put_rgb(pal, n++, r * 255 / (lv[0] - 1), g * 255 / (lv[1] - 1), b * 255 / (lv[2] - 1));
    return n;
}

int pc_palette_builtin(int palette, int colors, u8 *pal)
{
    int r, g, b, n = 0;
    switch (palette) {
    case PC_PAL_WINDOWS: return pal_windows(pal);
    case PC_PAL_MACOS: return pal_macos(pal);
    case PC_PAL_WEB:
        for (r = 0; r < 6; r++) for (g = 0; g < 6; g++) for (b = 0; b < 6; b++) put_rgb(pal, n++, r * 51, g * 51, b * 51);
        return n;
    case PC_PAL_UNIFORM: return pal_uniform(colors, pal);
    default: return 0;
    }
}

/* ---------------- exact colours ---------------- */

typedef struct { u32 key; int idx; } HEnt;     /* key = rgb + 1 (0 = empty) */
enum { HBITS = 10, HSIZE = 1 << HBITS };

static int hash_find(const HEnt *tab, u32 rgb)
{
    u32 key = rgb + 1, h = (key * 2654435761u) >> (32 - HBITS);
    while (tab[h].key) {
        if (tab[h].key == key) return tab[h].idx;
        h = (h + 1) & (HSIZE - 1);
    }
    return -1;
}

static void hash_add(HEnt *tab, u32 rgb, int idx)
{
    u32 key = rgb + 1, h = (key * 2654435761u) >> (32 - HBITS);
    while (tab[h].key && tab[h].key != key) h = (h + 1) & (HSIZE - 1);
    if (!tab[h].key) { tab[h].key = key; tab[h].idx = idx; }
}

static u32 px_rgb(const u8 *p) { return (u32)p[0] << 16 | (u32)p[1] << 8 | p[2]; }

/* Distinct colours of opaque (alpha 255 after matting) pixels into pal;
 * returns the count, or -1 when there are more than maxc. */
static int exact_colors(const u8 *px, size_t np, int maxc, u8 *pal)
{
    HEnt *tab = (HEnt *)calloc(HSIZE, sizeof *tab);
    size_t i;
    int n = 0;
    if (!tab) return -2;
    for (i = 0; i < np; i++, px += 4) {
        u32 c;
        if (!px[3]) continue;
        c = px_rgb(px);
        if (hash_find(tab, c) >= 0) continue;
        if (n >= maxc) { free(tab); return -1; }
        hash_add(tab, c, n);
        put_rgb(pal, n++, px[0], px[1], px[2]);
    }
    free(tab);
    return n;
}

int pc_count_colors(const u8 *rgba, size_t npx, int limit)
{
    HEnt *tab;
    size_t i;
    int n = 0;
    if (limit > HSIZE / 2) limit = HSIZE / 2;
    tab = (HEnt *)calloc(HSIZE, sizeof *tab);
    if (!tab) return limit + 1;
    for (i = 0; i < npx && n <= limit; i++, rgba += 4) {
        u32 c;
        if (rgba[3] < 128) continue;
        c = px_rgb(rgba);
        if (hash_find(tab, c) >= 0) continue;
        hash_add(tab, c, n++);
    }
    free(tab);
    return n;
}

/* ---------------- median cut ---------------- */

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

/* Median cut over the opaque pixels; returns the palette size (0 = OOM). */
static int median_cut(const u8 *px, size_t np, int maxc, u8 *pal)
{
    u32 *hist = (u32 *)calloc(32768, sizeof(u32));
    unsigned long long *sum = (unsigned long long *)calloc(32768 * 3, sizeof *sum);
    Box *boxes = (Box *)malloc(sizeof(Box) * (size_t)maxc);
    size_t i;
    int nb = 1, k;
    if (!hist || !sum || !boxes) { free(hist); free(sum); free(boxes); return 0; }
    for (i = 0; i < np; i++) {
        const u8 *p = px + i * 4;
        int c = (p[0] >> 3) << 10 | (p[1] >> 3) << 5 | (p[2] >> 3);
        if (!p[3]) continue;
        if (hist[c] < 0xFFFFFFFFu) hist[c]++;
        sum[c * 3] += p[0]; sum[c * 3 + 1] += p[1]; sum[c * 3 + 2] += p[2];
    }
    boxes[0].lo[0] = boxes[0].lo[1] = boxes[0].lo[2] = 0;
    boxes[0].hi[0] = boxes[0].hi[1] = boxes[0].hi[2] = 31;
    box_shrink(&boxes[0], hist);
    if (!boxes[0].count) { put_rgb(pal, 0, 255, 255, 255); free(hist); free(sum); free(boxes); return 1; }
    while (nb < maxc) {
        int best = -1, axis = 0, b;
        unsigned long long score = 0;
        for (b = 0; b < nb; b++) {
            int a, len = 0, ax = 0;
            for (a = 0; a < 3; a++) if (boxes[b].hi[a] - boxes[b].lo[a] > len) { len = boxes[b].hi[a] - boxes[b].lo[a]; ax = a; }
            if (len == 0) continue;
            if (boxes[b].count * (unsigned long long)len > score) { score = boxes[b].count * (unsigned long long)len; best = b; axis = ax; }
        }
        if (best < 0) break;
        {
            Box *bp = &boxes[best], nbx;
            unsigned long long half = bp->count / 2, acc = 0;
            int cut = bp->lo[axis], v;
            for (v = bp->lo[axis]; v < bp->hi[axis]; v++) {
                int r, g, bb;
                for (r = (axis == 0 ? v : bp->lo[0]); r <= (axis == 0 ? v : bp->hi[0]); r++)
                    for (g = (axis == 1 ? v : bp->lo[1]); g <= (axis == 1 ? v : bp->hi[1]); g++)
                        for (bb = (axis == 2 ? v : bp->lo[2]); bb <= (axis == 2 ? v : bp->hi[2]); bb++)
                            acc += hist[(r << 10) | (g << 5) | bb];
                cut = v;
                if (acc >= half) break;
            }
            nbx = *bp;
            bp->hi[axis] = cut;
            nbx.lo[axis] = cut + 1;
            box_shrink(bp, hist);
            box_shrink(&nbx, hist);
            if (!nbx.count || !bp->count) break;     /* cannot happen with a proper cut */
            boxes[nb++] = nbx;
        }
    }
    for (k = 0; k < nb; k++) {
        unsigned long long t[3] = { 0, 0, 0 }, cnt = 0;
        int r, g, bb, ch;
        for (r = boxes[k].lo[0]; r <= boxes[k].hi[0]; r++)
            for (g = boxes[k].lo[1]; g <= boxes[k].hi[1]; g++)
                for (bb = boxes[k].lo[2]; bb <= boxes[k].hi[2]; bb++) {
                    int c = (r << 10) | (g << 5) | bb;
                    if (!hist[c]) continue;
                    cnt += hist[c]; t[0] += sum[c * 3]; t[1] += sum[c * 3 + 1]; t[2] += sum[c * 3 + 2];
                }
        if (!cnt) cnt = 1;
        for (ch = 0; ch < 3; ch++) pal[k * 3 + ch] = (u8)((t[ch] + cnt / 2) / cnt);
    }
    free(hist); free(sum); free(boxes);
    return nb;
}

/* ---------------- nearest colour ---------------- */

static long dist(const u8 *e, int r, int g, int b)
{
    long dr = r - e[0], dg = g - e[1], db = b - e[2];
    return dr * dr * 3 + dg * dg * 4 + db * db * 2;
}

static int nearest(const u8 *pal, int n, int excl, int r, int g, int b)
{
    int i, best = excl == 0 ? 1 : 0;
    long bd = 0x7FFFFFFF;
    for (i = 0; i < n; i++) {
        long dd;
        if (i == excl) continue;
        dd = dist(pal + i * 3, r, g, b);
        if (dd < bd) { bd = dd; best = i; }
    }
    return best;
}

/* The entry whose nearest neighbour is closest (the one a reserved slot
 * costs least to replace). */
static int most_redundant(const u8 *pal, int n)
{
    int i, j, best = n - 1;
    long bd = 0x7FFFFFFF;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            long dd;
            if (i == j) continue;
            dd = dist(pal + i * 3, pal[j * 3], pal[j * 3 + 1], pal[j * 3 + 2]);
            if (dd < bd) { bd = dd; best = i; }
        }
    return best;
}

static const u8 bayer8[64] = {
     0, 32,  8, 40,  2, 34, 10, 42, 48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38, 60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41, 51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37, 63, 31, 55, 23, 61, 29, 53, 21
};

/* Bayer offset in -spread/2 .. +spread/2 */
static int bayer_off(int x, int y, int spread) { return ((bayer8[(y & 7) * 8 + (x & 7)] * 2 + 1 - 64) * spread) / 128; }

typedef struct {
    const u8 *pal;
    int n, excl, key_exact;     /* key_exact: opaque pixels equal to the reserved colour may use it */
    HEnt *exact;
    short *inv;                 /* 5-6-5 inverse cache, -1 = not computed */
} Mapper;

static int map_color(Mapper *m, int r, int g, int b)
{
    int k, i = hash_find(m->exact, (u32)r << 16 | (u32)g << 8 | (u32)b);
    if (i >= 0 && (i != m->excl || m->key_exact)) return i;
    k = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
    if (m->inv[k] < 0) m->inv[k] = (short)nearest(m->pal, m->n, m->excl, (r & ~7) | 4, (g & ~3) | 2, (b & ~7) | 4);
    return m->inv[k];
}

/* ---------------- full alpha (the modern PNG-8 exception) ---------------- */

/* A colour with alpha in the 4-4-4-4 histogram used by the RGBA median cut. */
typedef struct { u8 c[4]; u32 count; unsigned long long sum[4]; } APt;
typedef struct { int lo, hi; unsigned long long count; } ABox;    /* points [lo, hi) */

static int g_axis;      /* qsort has no context argument */
static int apt_cmp(const void *a, const void *b) { return (int)((const APt *)a)->c[g_axis] - (int)((const APt *)b)->c[g_axis]; }

/* Distance of two RGBA colours in premultiplied space, alpha weighted like red. */
static long dist4(const u8 *e, int ea, int r, int g, int b, int a)
{
    long dr = r * a / 255 - e[0] * ea / 255, dg = g * a / 255 - e[1] * ea / 255, db = b * a / 255 - e[2] * ea / 255, da = a - ea;
    return dr * dr * 3 + dg * dg * 4 + db * db * 2 + da * da * 3;
}

static int nearest4(const PcIndexed *x, int n, int excl, int r, int g, int b, int a)
{
    int i, best = excl == 0 ? 1 : 0;
    long bd = 0x7FFFFFFF;
    for (i = 0; i < n; i++) {
        long dd;
        if (i == excl) continue;
        dd = dist4(x->pal + i * 3, x->alpha[i], r, g, b, a);
        if (dd < bd) { bd = dd; best = i; }
    }
    return best;
}

typedef struct { u32 key; int idx; int used; } HEnt4;
static int hash4_find(const HEnt4 *t, u32 key)
{
    u32 h = (key * 2654435761u) >> (32 - HBITS);
    while (t[h].used) { if (t[h].key == key) return t[h].idx; h = (h + 1) & (HSIZE - 1); }
    return -1;
}
static void hash4_add(HEnt4 *t, u32 key, int idx)
{
    u32 h = (key * 2654435761u) >> (32 - HBITS);
    while (t[h].used && t[h].key != key) h = (h + 1) & (HSIZE - 1);
    if (!t[h].used) { t[h].used = 1; t[h].key = key; t[h].idx = idx; }
}
static u32 rgba_key(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

/* Palette entries in ascending alpha order (translucent first keeps the
 * PNG tRNS chunk short); remaps the indices. */
static void sort_by_alpha(PcIndexed *x, size_t np)
{
    u8 perm[256], inv[256], pal[768], al[256];
    int i, k = 0, a;
    size_t j;
    for (a = 0; a < 256; a++)
        for (i = 0; i < x->ncolors; i++) if (x->alpha[i] == a) perm[k++] = (u8)i;
    for (i = 0; i < x->ncolors; i++) {
        inv[perm[i]] = (u8)i;
        memcpy(pal + i * 3, x->pal + perm[i] * 3, 3);
        al[i] = x->alpha[perm[i]];
    }
    memcpy(x->pal, pal, (size_t)x->ncolors * 3);
    memcpy(x->alpha, al, (size_t)x->ncolors);
    for (j = 0; j < np; j++) x->idx[j] = inv[x->idx[j]];
    if (x->transparent >= 0) x->transparent = inv[x->transparent];
}

static int quantize_rgba(const u8 *rgba, int w, int h, const PcQuantOpts *q, PcIndexed *out)
{
    size_t np = (size_t)w * h, i;
    int maxc = q->palette == PC_PAL_EXACT ? 256 : PC_CLAMP(q->colors, 2, 256);
    int any_t = 0, n = 0, tindex = -1, dither = PC_CLAMP(q->dither, 0, PC_DITHER_COUNT - 1), x, y, k, rc = -1, ch;
    u32 key = q->colorkey ? (q->key_rgb & 0xFFFFFF) : 0;
    HEnt4 *exact = (HEnt4 *)calloc(HSIZE, sizeof *exact);
    short *inv = (short *)malloc(sizeof(short) * 65536);
    int *errbuf = NULL;
    APt *pts = NULL;
    ABox *boxes = NULL;
    if (!exact || !inv) goto done;
    for (i = 0; i < np; i++) if (!rgba[i * 4 + 3]) { any_t = 1; break; }
    /* exact colours (fully transparent pixels all share one entry) */
    for (i = 0; i < np && n >= 0; i++) {
        const u8 *p = rgba + i * 4;
        if (!p[3] || hash4_find(exact, rgba_key(p)) >= 0) continue;
        if (n >= maxc - any_t) { n = -1; break; }
        hash4_add(exact, rgba_key(p), n);
        memcpy(out->pal + n * 3, p, 3);
        out->alpha[n++] = p[3];
    }
    if (n >= 0) {
        dither = PC_DITHER_NONE;
    } else {
        /* median cut in RGBA over a 4-4-4-4 histogram, entries are the means of the real colours */
        u32 *hist = (u32 *)calloc(65536, sizeof(u32));
        int npts = 0, nb = 1, lim = PC_MAX(1, maxc - any_t);
        if (!hist) goto done;
        for (i = 0; i < np; i++) {
            const u8 *p = rgba + i * 4;
            if (p[3] && hist[(p[0] >> 4) << 12 | (p[1] >> 4) << 8 | (p[2] >> 4) << 4 | (p[3] >> 4)]++ == 0) npts++;
        }
        pts = (APt *)calloc((size_t)npts + 1, sizeof *pts);
        boxes = (ABox *)malloc(sizeof(ABox) * (size_t)lim);
        if (!pts || !boxes) { free(hist); goto done; }
        {
            int *slot = (int *)hist;      /* reuse: bucket -> point index (counts copied first) */
            int m2 = 0, b;
            for (b = 0; b < 65536; b++) {
                if (!hist[b]) { slot[b] = -1; continue; }
                pts[m2].c[0] = (u8)(b >> 12); pts[m2].c[1] = (u8)((b >> 8) & 15); pts[m2].c[2] = (u8)((b >> 4) & 15); pts[m2].c[3] = (u8)(b & 15);
                pts[m2].count = hist[b];
                slot[b] = m2++;
            }
            for (i = 0; i < np; i++) {
                const u8 *p = rgba + i * 4;
                APt *t;
                if (!p[3]) continue;
                t = &pts[slot[(p[0] >> 4) << 12 | (p[1] >> 4) << 8 | (p[2] >> 4) << 4 | (p[3] >> 4)]];
                for (ch = 0; ch < 4; ch++) t->sum[ch] += p[ch];
            }
        }
        free(hist);
        boxes[0].lo = 0; boxes[0].hi = npts; boxes[0].count = 0;
        for (k = 0; k < npts; k++) boxes[0].count += pts[k].count;
        while (nb < lim) {
            int best = -1, axis = 0, b, len, j;
            unsigned long long score = 0, half, acc = 0;
            for (b = 0; b < nb; b++) {
                int lo[4] = { 15, 15, 15, 15 }, hi[4] = { 0, 0, 0, 0 }, a2;
                if (boxes[b].hi - boxes[b].lo < 2) continue;
                for (j = boxes[b].lo; j < boxes[b].hi; j++)
                    for (a2 = 0; a2 < 4; a2++) { lo[a2] = PC_MIN(lo[a2], pts[j].c[a2]); hi[a2] = PC_MAX(hi[a2], pts[j].c[a2]); }
                for (a2 = 0; a2 < 4; a2++) {
                    len = hi[a2] - lo[a2];
                    if (len > 0 && boxes[b].count * (unsigned long long)len > score) { score = boxes[b].count * (unsigned long long)len; best = b; axis = a2; }
                }
            }
            if (best < 0) break;
            g_axis = axis;
            qsort(pts + boxes[best].lo, (size_t)(boxes[best].hi - boxes[best].lo), sizeof *pts, apt_cmp);
            half = boxes[best].count / 2;
            for (j = boxes[best].lo; j < boxes[best].hi - 1; j++) {
                acc += pts[j].count;
                if (acc >= half) break;
            }
            /* do not split inside a run of equal coordinates when avoidable */
            while (j + 1 < boxes[best].hi - 1 && pts[j + 1].c[axis] == pts[j].c[axis]) { j++; acc += pts[j].count; }
            boxes[nb].lo = j + 1; boxes[nb].hi = boxes[best].hi; boxes[nb].count = boxes[best].count - acc;
            boxes[best].hi = j + 1; boxes[best].count = acc;
            nb++;
        }
        for (n = 0; n < nb; n++) {
            unsigned long long s[4] = { 0, 0, 0, 0 }, cnt = 0;
            for (k = boxes[n].lo; k < boxes[n].hi; k++) { cnt += pts[k].count; for (ch = 0; ch < 4; ch++) s[ch] += pts[k].sum[ch]; }
            if (!cnt) cnt = 1;
            for (ch = 0; ch < 3; ch++) out->pal[n * 3 + ch] = (u8)((s[ch] + cnt / 2) / cnt);
            out->alpha[n] = (u8)PC_MAX(1, (int)((s[3] + cnt / 2) / cnt));
        }
        memset(exact, 0, sizeof(HEnt4) * HSIZE);
        for (k = 0; k < n; k++) {
            u8 e[4];
            memcpy(e, out->pal + k * 3, 3); e[3] = out->alpha[k];
            hash4_add(exact, rgba_key(e), k);
        }
    }
    if (n <= 0) { n = 1; put_rgb(out->pal, 0, 255, 255, 255); out->alpha[0] = 255; }
    if (any_t) { tindex = n++; put_rgb(out->pal, tindex, pc_r(key), pc_g(key), pc_b(key)); out->alpha[tindex] = 0; }
    for (k = 0; k < 65536; k++) inv[k] = -1;
    if (dither == PC_DITHER_DIFFUSION) {
        errbuf = (int *)calloc((size_t)(w + 2) * 8, sizeof(int));
        if (!errbuf) goto done;
    }
    for (y = 0; y < h; y++) {
        int *cur = errbuf ? errbuf + ((y & 1) ? 4 * (w + 2) : 0) : NULL;
        int *nxt = errbuf ? errbuf + ((y & 1) ? 0 : 4 * (w + 2)) : NULL;
        if (nxt) memset(nxt, 0, sizeof(int) * 4 * (w + 2));
        for (x = 0; x < w; x++) {
            size_t pi = (size_t)y * w + x;
            const u8 *p = rgba + pi * 4;
            int v[4], best;
            u8 vv[4];
            if (!p[3]) { out->idx[pi] = (u8)tindex; continue; }
            for (ch = 0; ch < 4; ch++) {
                int t = p[ch];
                if (cur) t += (cur[(x + 1) * 4 + ch] + 8) >> 4;
                else if (dither == PC_DITHER_PATTERN && ch < 3) t += bayer_off(x, y, 32);
                v[ch] = PC_CLAMP(t, ch == 3 ? 1 : 0, 255);
                vv[ch] = (u8)v[ch];
            }
            best = hash4_find(exact, rgba_key(vv));
            if (best < 0) {
                int kk = (v[0] >> 4) << 12 | (v[1] >> 4) << 8 | (v[2] >> 4) << 4 | (v[3] >> 4);
                if (inv[kk] < 0) inv[kk] = (short)nearest4(out, n, tindex, (v[0] & ~15) | 8, (v[1] & ~15) | 8, (v[2] & ~15) | 8, (v[3] & ~15) | 8);
                best = inv[kk];
            }
            out->idx[pi] = (u8)best;
            if (cur)
                for (ch = 0; ch < 4; ch++) {
                    int e = v[ch] - (ch < 3 ? out->pal[best * 3 + ch] : out->alpha[best]);
                    cur[(x + 2) * 4 + ch] += e * 7;
                    nxt[x * 4 + ch] += e * 3;
                    nxt[(x + 1) * 4 + ch] += e * 5;
                    nxt[(x + 2) * 4 + ch] += e;
                }
        }
    }
    out->ncolors = n;
    out->transparent = tindex;
    sort_by_alpha(out, np);
    rc = 0;
done:
    free(exact); free(inv); free(errbuf); free(pts); free(boxes);
    return rc;
}

int pc_quantize(const u8 *rgba, int w, int h, const PcQuantOpts *q, const u8 *doc_pal, int doc_n, int doc_t, PcIndexed *out)
{
    size_t np = (size_t)w * h, i;
    u8 *px = NULL;
    int reserve = 0, n = 0, tindex = -1, pal_kind, dither, spread, maxc, x, y, k, rc = -1;
    int want_t = q->transparency || q->colorkey;
    u32 key = q->key_rgb & 0xFFFFFF;
    int *errbuf = NULL;
    Mapper m;
    memset(out, 0, sizeof *out);
    memset(&m, 0, sizeof m);
    memset(out->alpha, 255, sizeof out->alpha);
    out->w = w; out->h = h; out->transparent = -1;
    if (w <= 0 || h <= 0) return -1;
    if (q->full_alpha && (q->palette == PC_PAL_EXACT || q->palette == PC_PAL_ADAPTIVE ||
                          (q->palette == PC_PAL_DOCUMENT && !(doc_pal && doc_n >= 1)))) {
        int partial = 0;
        for (i = 0; i < np && !partial; i++) partial = rgba[i * 4 + 3] != 0 && rgba[i * 4 + 3] != 255;
        if (partial) {
            out->idx = (u8 *)pc_alloc_checked(np, 1, 1, 0);
            if (!out->idx || quantize_rgba(rgba, w, h, q, out)) { pc_indexed_free(out); return -1; }
            return 0;
        }
        /* only 0 / 255 alpha: the binary path below gives the same result */
    }
    px = (u8 *)pc_alloc_checked(np, 4, 1, 0);
    out->idx = (u8 *)pc_alloc_checked(np, 1, 1, 0);
    m.exact = (HEnt *)calloc(HSIZE, sizeof *m.exact);
    m.inv = (short *)malloc(sizeof(short) * 65536);
    if (!px || !out->idx || !m.exact || !m.inv) goto done;
    /* matte; alpha becomes 0 (transparent) or 255 */
    for (i = 0; i < np; i++) {
        const u8 *s = rgba + i * 4;
        u8 *p = px + i * 4;
        int a = s[3], ch;
        if (want_t && a < 128) { p[0] = (u8)pc_r(key); p[1] = (u8)pc_g(key); p[2] = (u8)pc_b(key); p[3] = 0; reserve = 1; continue; }
        for (ch = 0; ch < 3; ch++) p[ch] = (u8)((s[ch] * a + 255 * (255 - a) + 127) / 255);
        p[3] = 255;
    }

    /* palette */
    pal_kind = q->palette;
    maxc = PC_CLAMP(q->colors, 2, 256);
    dither = PC_CLAMP(q->dither, 0, PC_DITHER_COUNT - 1);
    if (pal_kind == PC_PAL_DOCUMENT) {
        if (doc_pal && doc_n >= 1 && doc_n <= 256) {
            n = doc_n;
            memcpy(out->pal, doc_pal, (size_t)n * 3);
            if (reserve && doc_t >= 0 && doc_t < n) tindex = doc_t;
        } else {
            pal_kind = PC_PAL_ADAPTIVE;
        }
    }
    if (pal_kind == PC_PAL_EXACT) {
        n = exact_colors(px, np, 256 - reserve, out->pal);
        if (n == -2) goto done;
        if (n < 0) { pal_kind = PC_PAL_ADAPTIVE; maxc = 256; }
        else dither = PC_DITHER_NONE;
    }
    if (pal_kind == PC_PAL_ADAPTIVE) {
        int lim = PC_MAX(1, maxc - reserve);
        n = exact_colors(px, np, lim, out->pal);      /* few colours: keep them all */
        if (n == -2) goto done;
        if (n < 0) n = median_cut(px, np, lim, out->pal);
        else dither = PC_DITHER_NONE;
        if (n <= 0) goto done;
    } else if (pal_kind != PC_PAL_EXACT && pal_kind != PC_PAL_DOCUMENT) {
        n = pc_palette_builtin(pal_kind, maxc, out->pal);
        if (n <= 0) n = pc_palette_builtin(PC_PAL_WEB, 0, out->pal);
    }
    if (n <= 0) { n = 1; put_rgb(out->pal, 0, 255, 255, 255); }

    /* the reserved transparent / colour key entry */
    if (reserve && tindex < 0) {
        if (q->colorkey)
            for (k = 0; k < n && tindex < 0; k++)
                if (pc_rgb(out->pal[k * 3], out->pal[k * 3 + 1], out->pal[k * 3 + 2]) == key) tindex = k;
        if (tindex < 0) {
            if (n < 256) { tindex = n++; put_rgb(out->pal, tindex, 255, 255, 255); }
            else tindex = most_redundant(out->pal, n);
        }
    }
    if (reserve && q->colorkey) put_rgb(out->pal, tindex, pc_r(key), pc_g(key), pc_b(key));
    if (n == 1 && tindex == 0) { put_rgb(out->pal, 1, 255, 255, 255); n = 2; }   /* room for opaque pixels */

    /* map */
    m.pal = out->pal; m.n = n; m.excl = tindex; m.key_exact = q->colorkey;
    for (k = 0; k < n; k++) hash_add(m.exact, pc_rgb(out->pal[k * 3], out->pal[k * 3 + 1], out->pal[k * 3 + 2]), k);
    for (k = 0; k < 65536; k++) m.inv[k] = -1;
    for (spread = 1; (spread + 1) * (spread + 1) * (spread + 1) <= n; spread++) {}
    spread = 256 / spread;                    /* typical palette step */
    if (dither == PC_DITHER_DIFFUSION) {
        errbuf = (int *)calloc((size_t)(w + 2) * 6, sizeof(int));
        if (!errbuf) goto done;
    }
    for (y = 0; y < h; y++) {
        int *cur = errbuf ? errbuf + ((y & 1) ? 3 * (w + 2) : 0) : NULL;
        int *nxt = errbuf ? errbuf + ((y & 1) ? 0 : 3 * (w + 2)) : NULL;
        if (nxt) memset(nxt, 0, sizeof(int) * 3 * (w + 2));
        for (x = 0; x < w; x++) {
            size_t pi = (size_t)y * w + x;
            const u8 *p = px + pi * 4;
            int v[3], ch, best;
            if (!p[3]) { out->idx[pi] = (u8)tindex; continue; }
            if (dither == PC_DITHER_PATTERN) {
                int e = hash_find(m.exact, px_rgb(p));
                if (e >= 0 && (e != tindex || m.key_exact)) { out->idx[pi] = (u8)e; continue; }
                for (ch = 0; ch < 3; ch++) { int t = p[ch] + bayer_off(x, y, spread); v[ch] = PC_CLAMP(t, 0, 255); }
            } else {
                for (ch = 0; ch < 3; ch++) {
                    int t = p[ch] + (cur ? (cur[(x + 1) * 3 + ch] + 8) >> 4 : 0);
                    v[ch] = PC_CLAMP(t, 0, 255);
                }
            }
            best = map_color(&m, v[0], v[1], v[2]);
            out->idx[pi] = (u8)best;
            if (cur) {
                for (ch = 0; ch < 3; ch++) {
                    int e = v[ch] - out->pal[best * 3 + ch];
                    cur[(x + 2) * 3 + ch] += e * 7;
                    nxt[x * 3 + ch] += e * 3;
                    nxt[(x + 1) * 3 + ch] += e * 5;
                    nxt[(x + 2) * 3 + ch] += e;
                }
            }
        }
    }
    out->ncolors = n;
    out->transparent = reserve ? tindex : -1;
    if (reserve && q->transparency) out->alpha[tindex] = 0;
    rc = 0;
done:
    free(px); free(m.exact); free(m.inv); free(errbuf);
    if (rc) pc_indexed_free(out);
    return rc;
}

void pc_indexed_free(PcIndexed *x)
{
    if (!x) return;
    free(x->idx);
    x->idx = NULL;
}

/* ---------------- high colour ---------------- */

void pc_hicolor_reduce(u8 *rgba, int w, int h, int green6, int dither)
{
    int x, y, ch, *errbuf = NULL;
    if (dither == PC_DITHER_DIFFUSION) errbuf = (int *)calloc((size_t)(w + 2) * 6, sizeof(int));
    for (y = 0; y < h; y++) {
        int *cur = errbuf ? errbuf + ((y & 1) ? 3 * (w + 2) : 0) : NULL;
        int *nxt = errbuf ? errbuf + ((y & 1) ? 0 : 3 * (w + 2)) : NULL;
        if (nxt) memset(nxt, 0, sizeof(int) * 3 * (w + 2));
        for (x = 0; x < w; x++) {
            u8 *p = rgba + ((size_t)y * w + x) * 4;
            for (ch = 0; ch < 3; ch++) {
                int six = green6 && ch == 1, v = p[ch], r;
                if (dither == PC_DITHER_PATTERN) v += bayer_off(x, y, six ? 4 : 8);
                else if (cur) v += (cur[(x + 1) * 3 + ch] + 8) >> 4;
                v = PC_CLAMP(v, 0, 255);
                r = six ? pc_x6(pc_q6(v)) : pc_x5(pc_q5(v));
                if (cur) {
                    int e = v - r;
                    cur[(x + 2) * 3 + ch] += e * 7;
                    nxt[x * 3 + ch] += e * 3;
                    nxt[(x + 1) * 3 + ch] += e * 5;
                    nxt[(x + 2) * 3 + ch] += e;
                }
                p[ch] = (u8)r;
            }
        }
    }
    free(errbuf);
}

/* ---------------- Image > Mode > Indexed Color ---------------- */

int pc_doc_to_indexed(PcDoc *d, const PcQuantOpts *q)
{
    PcBuf *c = pc_doc_composite(d);
    PcIndexed ix;
    PcLayer *l;
    size_t i, np;
    int k, any_t = 0;
    u8 *p;
    if (!c) return -1;
    np = (size_t)d->s.w * d->s.h;
    if (d->s.mode == PC_MODE_GRAY)
        for (i = 0; i < np; i++) c->px[i * 4] = c->px[i * 4 + 1] = c->px[i * 4 + 2] = (u8)pc_luma(c->px[i * 4], c->px[i * 4 + 1], c->px[i * 4 + 2]);
    if (pc_quantize(c->px, d->s.w, d->s.h, q, d->index_ncolors ? d->index_pal : NULL, d->index_ncolors, d->index_transparent, &ix)) {
        pc_buf_unref(c);
        return -1;
    }
    for (i = 0, p = c->px; i < np; i++, p += 4) {
        int e = ix.idx[i];
        p[0] = ix.pal[e * 3]; p[1] = ix.pal[e * 3 + 1]; p[2] = ix.pal[e * 3 + 2];
        p[3] = q->transparency ? ix.alpha[e] : 255;
        if (p[3] != 255) any_t = 1;
    }
    l = pc_layer_new(1, 1, any_t ? "Layer 0" : "Background");
    if (!l) { pc_indexed_free(&ix); pc_buf_unref(c); return -1; }
    pc_buf_unref(l->pix);
    l->pix = c;
    l->is_background = !any_t;
    for (k = 0; k < d->s.nlayers; k++) pc_layer_free(d->s.layers[k]);
    d->s.layers[0] = l;
    d->s.nlayers = 1;
    d->s.active = 0;
    d->s.mode = PC_MODE_RGB;
    d->index_ncolors = ix.ncolors;
    d->index_transparent = ix.transparent;
    memcpy(d->index_pal, ix.pal, sizeof d->index_pal);
    pc_indexed_free(&ix);
    pc_doc_touch(d);
    return 0;
}
