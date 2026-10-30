/*
 * Selections: marquees, lasso polygons, magic wand, modify, feather,
 * colour range, channels.  The selection is a doc-sized 8-bit coverage
 * mask (PhotoCraft crates/algo/src/selection.rs uses the same model).
 */
#include "pcops.h"

int pc_sel_combine(PcDoc *d, const u8 *mask, int op)
{
    size_t i, n = (size_t)d->s.w * d->s.h;
    PcBuf *s;
    if (op == PC_SEL_REPLACE || (!d->s.sel && op == PC_SEL_ADD)) {
        s = pc_buf_new(d->s.w, d->s.h, 1, 0);
        if (!s) return -1;
        memcpy(s->px, mask, n);
        pc_buf_unref(d->s.sel);
        d->s.sel = s;
    } else if (!d->s.sel) {
        if (op == PC_SEL_SUBTRACT) return 0;
        /* Intersect with "everything" = the mask itself. */
        s = pc_buf_new(d->s.w, d->s.h, 1, 0);
        if (!s) return -1;
        memcpy(s->px, mask, n);
        d->s.sel = s;
    } else {
        s = pc_sel_get(d, 1);
        if (!s) return -1;
        for (i = 0; i < n; i++) {
            int a = s->px[i], b = mask[i];
            if (op == PC_SEL_ADD) a = a + b - pc_mul255(a, b);
            else if (op == PC_SEL_SUBTRACT) a = pc_mul255(a, 255 - b);
            else a = pc_mul255(a, b);
            s->px[i] = (u8)a;
        }
    }
    /* An empty selection is no selection. */
    for (i = 0; i < n; i++) if (d->s.sel->px[i]) break;
    if (i == n) { pc_buf_unref(d->s.sel); d->s.sel = NULL; }
    pc_doc_touch(d);
    return 0;
}

static u8 *new_mask(const PcDoc *d) { return (u8 *)pc_alloc_checked((size_t)d->s.w, (size_t)d->s.h, 1, 1); }

static int finish(PcDoc *d, u8 *m, int op, float feather)
{
    int rc;
    if (feather > 0.05f) pc_gaussian_gray(m, d->s.w, d->s.h, feather);
    rc = pc_sel_combine(d, m, op);
    free(m);
    return rc;
}

int pc_sel_rect(PcDoc *d, int x0, int y0, int x1, int y1, int op, float feather)
{
    u8 *m;
    int y, t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    x0 = PC_MAX(x0, 0); y0 = PC_MAX(y0, 0); x1 = PC_MIN(x1, d->s.w); y1 = PC_MIN(y1, d->s.h);
    m = new_mask(d);
    if (!m) return -1;
    for (y = y0; y < y1; y++) if (x1 > x0) memset(m + (size_t)y * d->s.w + x0, 255, (size_t)(x1 - x0));
    return finish(d, m, op, feather);
}

int pc_sel_ellipse(PcDoc *d, int x0, int y0, int x1, int y1, int op, int aa, float feather)
{
    u8 *m;
    int x, y, t, w = d->s.w, h = d->s.h;
    double cx, cy, rx, ry;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    m = new_mask(d);
    if (!m) return -1;
    cx = (x0 + x1) / 2.0; cy = (y0 + y1) / 2.0; rx = (x1 - x0) / 2.0; ry = (y1 - y0) / 2.0;
    if (rx > 0 && ry > 0)
        for (y = PC_MAX(y0, 0); y < PC_MIN(y1, h); y++)
            for (x = PC_MAX(x0, 0); x < PC_MIN(x1, w); x++) {
                int cov = 0, sx, sy, ss = aa ? 4 : 1;
                for (sy = 0; sy < ss; sy++)
                    for (sx = 0; sx < ss; sx++) {
                        double px = x + (sx + 0.5) / ss, py = y + (sy + 0.5) / ss;
                        double nx = (px - cx) / rx, ny = (py - cy) / ry;
                        if (nx * nx + ny * ny <= 1.0) cov++;
                    }
                m[(size_t)y * w + x] = (u8)(cov * 255 / (ss * ss));
            }
    return finish(d, m, op, feather);
}

/* Even-odd scanline fill with optional 4x4 supersampling. */
void pc_raster_polygons(u8 *mask, int w, int h, const float *xy, const u8 *starts, int n, int aa)
{
    int ss = aa ? 4 : 1, y, sy, i, k;
    float *xs;
    unsigned short *acc;
    if (n < 3) return;
    xs = (float *)malloc(sizeof(float) * (size_t)n);
    acc = (unsigned short *)calloc((size_t)w, sizeof(unsigned short));
    if (!xs || !acc) { free(xs); free(acc); return; }
    for (y = 0; y < h; y++) {
        memset(acc, 0, sizeof(unsigned short) * (size_t)w);
        for (sy = 0; sy < ss; sy++) {
            float fy = y + (sy + 0.5f) / ss;
            int nx = 0, s0 = 0;
            /* Edges per sub-path, closing each polygon. */
            for (i = 0; i < n; i++) {
                int j;
                float ax, ay, bx, by;
                if (starts && starts[i]) s0 = i;
                j = i + 1;
                if (j >= n || (starts && starts[j])) j = s0;
                ax = xy[i * 2]; ay = xy[i * 2 + 1]; bx = xy[j * 2]; by = xy[j * 2 + 1];
                if ((ay <= fy && by > fy) || (by <= fy && ay > fy)) xs[nx++] = ax + (fy - ay) * (bx - ax) / (by - ay);
            }
            /* Sort crossings (insertion sort: counts are small). */
            for (i = 1; i < nx; i++) { float v = xs[i]; k = i - 1; while (k >= 0 && xs[k] > v) { xs[k + 1] = xs[k]; k--; } xs[k + 1] = v; }
            for (i = 0; i + 1 < nx; i += 2) {
                float a = xs[i], b = xs[i + 1];
                int sx;
                int xa = PC_MAX(0, (int)floor(a)), xb = PC_MIN(w - 1, (int)ceil(b));
                for (k = xa; k <= xb; k++)
                    for (sx = 0; sx < ss; sx++) {
                        float fx = k + (sx + 0.5f) / ss;
                        if (fx >= a && fx < b) acc[k]++;
                    }
            }
        }
        for (k = 0; k < w; k++) {
            int v = acc[k] * 255 / (ss * ss);
            u8 *q = mask + (size_t)y * w + k;
            if (v > *q) *q = (u8)v;
        }
    }
    free(xs); free(acc);
}

int pc_sel_polygons(PcDoc *d, const float *xy, const u8 *starts, int n, int op, int aa, float feather)
{
    u8 *m = new_mask(d);
    if (!m) return -1;
    pc_raster_polygons(m, d->s.w, d->s.h, xy, starts, n, aa);
    return finish(d, m, op, feather);
}

int pc_sel_polygon(PcDoc *d, const float *xy, int n, int op, int aa, float feather)
{
    return pc_sel_polygons(d, xy, NULL, n, op, aa, feather);
}

/* Flood/colour match with tolerance (Photoshop: per-channel max difference). */
static int color_match(const u8 *a, const u8 *b, int tol)
{
    int d0 = abs(a[0] - b[0]), d1 = abs(a[1] - b[1]), d2 = abs(a[2] - b[2]), d3 = abs(a[3] - b[3]);
    return d0 <= tol && d1 <= tol && d2 <= tol && d3 <= tol;
}

/* Build a 0/255 region mask of pixels matching the seed. */
static u8 *region_mask(const u8 *px, int w, int h, int x, int y, int tol, int contiguous)
{
    u8 *m = (u8 *)pc_alloc_checked((size_t)w, (size_t)h, 1, 1);
    u8 seed[4];
    if (!m) return NULL;
    memcpy(seed, px + ((size_t)y * w + x) * 4, 4);
    if (!contiguous) {
        size_t i, n = (size_t)w * h;
        for (i = 0; i < n; i++) if (color_match(px + i * 4, seed, tol)) m[i] = 255;
    } else {
        /* Scanline flood fill with an explicit stack (no recursion). */
        size_t cap = 4096, sp = 0;
        int *stack = (int *)malloc(sizeof(int) * 2 * cap);
        if (!stack) { free(m); return NULL; }
        stack[0] = x; stack[1] = y; sp = 1;
        while (sp) {
            int cx, cy, lx, rx;
            sp--;
            cx = stack[sp * 2]; cy = stack[sp * 2 + 1];
            if (m[(size_t)cy * w + cx]) continue;
            lx = cx; rx = cx;
            while (lx > 0 && !m[(size_t)cy * w + lx - 1] && color_match(px + ((size_t)cy * w + lx - 1) * 4, seed, tol)) lx--;
            while (rx < w - 1 && !m[(size_t)cy * w + rx + 1] && color_match(px + ((size_t)cy * w + rx + 1) * 4, seed, tol)) rx++;
            memset(m + (size_t)cy * w + lx, 255, (size_t)(rx - lx + 1));
            {
                int dy;
                for (dy = -1; dy <= 1; dy += 2) {
                    int ny = cy + dy, k, prev = 0;
                    if (ny < 0 || ny >= h) continue;
                    for (k = lx; k <= rx; k++) {
                        int ok = !m[(size_t)ny * w + k] && color_match(px + ((size_t)ny * w + k) * 4, seed, tol);
                        if (ok && !prev) {
                            if (sp >= cap) {
                                int *ns = (int *)realloc(stack, sizeof(int) * 4 * cap);
                                if (!ns) { free(stack); return m; }
                                stack = ns; cap *= 2;
                            }
                            stack[sp * 2] = k; stack[sp * 2 + 1] = ny; sp++;
                        }
                        prev = ok;
                    }
                }
            }
        }
        free(stack);
    }
    return m;
}

/* Soften a hard 0/255 region by one pixel at its boundary (anti-alias). */
static void soften_edges(u8 *m, int w, int h)
{
    u8 *t = (u8 *)malloc((size_t)w * h);
    int x, y;
    if (!t) return;
    memcpy(t, m, (size_t)w * h);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int sum = 0, i, j;
            if (t[(size_t)y * w + x] == 0) continue;
            for (j = -1; j <= 1; j++) for (i = -1; i <= 1; i++) sum += t[(size_t)PC_CLAMP(y + j, 0, h - 1) * w + PC_CLAMP(x + i, 0, w - 1)];
            if (sum < 255 * 9) m[(size_t)y * w + x] = (u8)PC_MAX(128, sum / 9);
        }
    free(t);
}

int pc_sel_magic_wand(PcDoc *d, int x, int y, int tol, int contiguous, int merged, int aa, int op)
{
    PcBuf *src;
    u8 *m;
    PcLayer *l = pc_doc_active(d);
    if (x < 0 || y < 0 || x >= d->s.w || y >= d->s.h) return -1;
    src = merged ? pc_doc_composite(d) : (l ? pc_buf_ref(l->pix) : NULL);
    if (!src) return -1;
    m = region_mask(src->px, d->s.w, d->s.h, x, y, PC_CLAMP(tol, 0, 255), contiguous);
    pc_buf_unref(src);
    if (!m) return -1;
    if (aa) soften_edges(m, d->s.w, d->s.h);
    return finish(d, m, op, 0);
}

int pc_sel_all(PcDoc *d)
{
    PcBuf *s = pc_buf_new(d->s.w, d->s.h, 1, 0);
    if (!s) return -1;
    memset(s->px, 255, (size_t)d->s.w * d->s.h);
    pc_buf_unref(d->s.sel);
    d->s.sel = s;
    pc_doc_touch(d);
    return 0;
}

int pc_sel_inverse(PcDoc *d)
{
    size_t i, n = (size_t)d->s.w * d->s.h;
    PcBuf *s;
    if (!d->s.sel) return pc_sel_all(d);
    s = pc_sel_get(d, 0);
    if (!s) return -1;
    for (i = 0; i < n; i++) s->px[i] = (u8)(255 - s->px[i]);
    for (i = 0; i < n; i++) if (s->px[i]) break;
    if (i == n) pc_sel_clear(d);
    pc_doc_touch(d);
    return 0;
}

int pc_sel_feather(PcDoc *d, float radius)
{
    PcBuf *s = pc_sel_get(d, 0);
    if (!s) return -1;
    pc_gaussian_gray(s->px, d->s.w, d->s.h, PC_CLAMP(radius, 0.2f, 250.0f));
    pc_doc_touch(d);
    return 0;
}

/* Grey-level dilation/erosion with a square kernel, separable. */
static void morph(u8 *m, int w, int h, int r, int dilate)
{
    u8 *t = (u8 *)malloc((size_t)w * h);
    int x, y, i;
    if (!t || r < 1) { free(t); return; }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int v = dilate ? 0 : 255;
            for (i = -r; i <= r; i++) {
                int xx = x + i, s = (xx < 0 || xx >= w) ? 0 : m[(size_t)y * w + xx];
                v = dilate ? PC_MAX(v, s) : PC_MIN(v, s);
            }
            t[(size_t)y * w + x] = (u8)v;
        }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int v = dilate ? 0 : 255;
            for (i = -r; i <= r; i++) {
                int yy = y + i, s = (yy < 0 || yy >= h) ? 0 : t[(size_t)yy * w + x];
                v = dilate ? PC_MAX(v, s) : PC_MIN(v, s);
            }
            m[(size_t)y * w + x] = (u8)v;
        }
    free(t);
}

int pc_sel_modify(PcDoc *d, int how, int px)
{
    PcBuf *s = pc_sel_get(d, 0);
    int w = d->s.w, h = d->s.h;
    size_t i, n = (size_t)w * h;
    if (!s) return -1;
    px = PC_CLAMP(px, 1, 100);
    if (how == 2) morph(s->px, w, h, px, 1);
    else if (how == 3) morph(s->px, w, h, px, 0);
    else if (how == 1) {
        pc_gaussian_gray(s->px, w, h, px / 2.0f);
        for (i = 0; i < n; i++) s->px[i] = s->px[i] >= 128 ? 255 : 0;
    } else {
        u8 *a = (u8 *)malloc(n), *b = (u8 *)malloc(n);
        if (!a || !b) { free(a); free(b); return -1; }
        memcpy(a, s->px, n); memcpy(b, s->px, n);
        morph(a, w, h, (px + 1) / 2, 1);
        morph(b, w, h, px / 2, 0);
        for (i = 0; i < n; i++) s->px[i] = (u8)pc_mul255(a[i], 255 - b[i]);
        pc_gaussian_gray(s->px, w, h, 1.0f);
        free(a); free(b);
    }
    for (i = 0; i < n; i++) if (s->px[i]) break;
    if (i == n) pc_sel_clear(d);
    pc_doc_touch(d);
    return 0;
}

int pc_sel_grow(PcDoc *d, int tol, int similar)
{
    PcLayer *l = pc_doc_active(d);
    PcBuf *s = d->s.sel;
    int w = d->s.w, h = d->s.h, x, y;
    u8 *acc;
    if (!s || !l) return -1;
    acc = new_mask(d);
    if (!acc) return -1;
    /* Grow from every selected pixel's colour (sampled sparsely for speed). */
    for (y = 0; y < h; y += 2)
        for (x = 0; x < w; x += 2) {
            if (s->px[(size_t)y * w + x] < 128 || acc[(size_t)y * w + x]) continue;
            {
                u8 *m = region_mask(l->pix->px, w, h, x, y, tol, !similar);
                size_t i, n = (size_t)w * h;
                if (!m) continue;
                for (i = 0; i < n; i++) if (m[i]) acc[i] = 255;
                free(m);
            }
            if (similar) break;
        }
    return finish(d, acc, PC_SEL_ADD, 0);
}

int pc_sel_color_range(PcDoc *d, u32 color, int fuzz, int op)
{
    PcBuf *c = pc_doc_composite(d);
    u8 *m;
    size_t i, n = (size_t)d->s.w * d->s.h;
    int r = pc_r(color), g = pc_g(color), b = pc_b(color);
    if (!c) return -1;
    m = new_mask(d);
    if (!m) { pc_buf_unref(c); return -1; }
    fuzz = PC_CLAMP(fuzz, 0, 200);
    for (i = 0; i < n; i++) {
        const u8 *p = c->px + i * 4;
        int dd = PC_MAX(abs(p[0] - r), PC_MAX(abs(p[1] - g), abs(p[2] - b)));
        int v = fuzz ? 255 - dd * 255 / (fuzz + 1) : (dd == 0 ? 255 : 0);
        m[i] = (u8)PC_CLAMP(v, 0, 255);
    }
    pc_buf_unref(c);
    return finish(d, m, op, 0);
}

int pc_sel_save_channel(PcDoc *d, const char *name)
{
    PcChannel *ch;
    if (!d->s.sel || d->s.nchannels >= PC_MAX_CHANNELS) return -1;
    ch = &d->s.channels[d->s.nchannels];
    memset(ch, 0, sizeof *ch);
    ch->buf = pc_buf_clone(d->s.sel);
    if (!ch->buf) return -1;
    if (name && *name) pc_strlcpy(ch->name, name, sizeof ch->name);
    else pc_snprintf(ch->name, sizeof ch->name, "Alpha %d", d->s.nchannels + 1);
    ch->color = 0xFF0000;
    d->s.nchannels++;
    pc_doc_touch(d);
    return 0;
}

int pc_sel_load_channel(PcDoc *d, int channel, int invert, int op)
{
    u8 *m;
    size_t i, n = (size_t)d->s.w * d->s.h;
    if (channel < 0 || channel >= d->s.nchannels) return -1;
    m = new_mask(d);
    if (!m) return -1;
    for (i = 0; i < n; i++) m[i] = invert ? (u8)(255 - d->s.channels[channel].buf->px[i]) : d->s.channels[channel].buf->px[i];
    return finish(d, m, op, 0);
}

int pc_sel_from_layer_alpha(PcDoc *d, int layer, int op)
{
    u8 *m;
    size_t i, n = (size_t)d->s.w * d->s.h;
    PcLayer *l;
    if (layer < 0 || layer >= d->s.nlayers) return -1;
    l = d->s.layers[layer];
    m = new_mask(d);
    if (!m) return -1;
    for (i = 0; i < n; i++) m[i] = l->pix->px[i * 4 + 3];
    return finish(d, m, op, 0);
}
