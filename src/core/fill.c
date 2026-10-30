/*
 * Edit > Fill, Edit > Stroke, Paint Bucket and Gradient tools.
 */
#include "pcops.h"

/* Composite a per-pixel source colour function into the active layer with
 * weights w (0..255, already including the selection). */
typedef void (*SrcFn)(int x, int y, u8 *out, void *ctx);

static int fill_with(PcDoc *d, const u8 *weights, SrcFn fn, void *ctx, int opacity, int blend, int preserve)
{
    PcLayer *l = pc_doc_active(d);
    int x, y, w = d->s.w, h = d->s.h;
    int op = PC_CLAMP(opacity, 0, 100) * 255 / 100;
    u8 *p;
    if (!l || l->lock_pixels) return -1;
    p = pc_layer_pixels_w(l);
    if (!p) return -1;
    preserve = preserve || l->lock_alpha || l->is_background;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            size_t i = (size_t)y * w + x;
            int a = weights ? weights[i] : pc_sel_at(d, x, y);
            u8 src[4], *dst = p + i * 4, oa;
            if (!a) continue;
            fn(x, y, src, ctx);
            a = pc_mul255(pc_mul255(a, op), src[3]);
            if (!a) continue;
            oa = dst[3];
            pc_blend_pixel(blend, dst, src, a, (unsigned)(x * 2654435761u) ^ (unsigned)(y * 40503u));
            if (preserve) dst[3] = oa;
            if (d->s.mode == PC_MODE_GRAY) dst[0] = dst[1] = dst[2] = (u8)pc_luma(dst[0], dst[1], dst[2]);
        }
    pc_doc_touch(d);
    return 0;
}

static void src_color(int x, int y, u8 *o, void *ctx)
{
    u32 c = *(u32 *)ctx;
    o[0] = (u8)pc_r(c); o[1] = (u8)pc_g(c); o[2] = (u8)pc_b(c); o[3] = 255;
    (void)x; (void)y;
}

static void src_pattern(int x, int y, u8 *o, void *ctx)
{
    const u8 *pat; int pw, ph;
    if (pc_pattern_get(&pat, &pw, &ph)) { o[0] = o[1] = o[2] = o[3] = 0; return; }
    memcpy(o, pat + ((size_t)(y % ph) * pw + (x % pw)) * 4, 4);
    (void)ctx;
}

static void src_buf(int x, int y, u8 *o, void *ctx)
{
    const PcBuf *b = (const PcBuf *)ctx;
    memcpy(o, b->px + ((size_t)y * b->w + x) * 4, 4);
}

int pc_fill_color(PcDoc *d, u32 rgb, int opacity, int blend, int preserve)
{
    return fill_with(d, NULL, src_color, &rgb, opacity, blend, preserve);
}

int pc_fill_pattern(PcDoc *d, int opacity, int blend)
{
    const u8 *p; int w, h;
    if (pc_pattern_get(&p, &w, &h)) return -1;
    return fill_with(d, NULL, src_pattern, NULL, opacity, blend, 0);
}

int pc_fill_history(PcDoc *d, int opacity, int blend)
{
    const PcDocState *hs = pc_hist_state(d, d->history_source);
    int li;
    if (!hs || hs->w != d->s.w || hs->h != d->s.h || hs->nlayers < 1) return -1;
    li = d->s.active < hs->nlayers ? d->s.active : hs->nlayers - 1;
    return fill_with(d, NULL, src_buf, hs->layers[li]->pix, opacity, blend, 0);
}

/* Grey morphology helper (square, radius r). */
static void dilate_erode(u8 *m, int w, int h, int r, int dilate)
{
    u8 *t;
    int x, y, i;
    if (r < 1) return;
    t = (u8 *)malloc((size_t)w * h);
    if (!t) return;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int v = dilate ? 0 : 255;
            for (i = -r; i <= r; i++) { int xx = x + i; int s = (xx < 0 || xx >= w) ? 0 : m[(size_t)y * w + xx]; v = dilate ? PC_MAX(v, s) : PC_MIN(v, s); }
            t[(size_t)y * w + x] = (u8)v;
        }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int v = dilate ? 0 : 255;
            for (i = -r; i <= r; i++) { int yy = y + i; int s = (yy < 0 || yy >= h) ? 0 : t[(size_t)yy * w + x]; v = dilate ? PC_MAX(v, s) : PC_MIN(v, s); }
            m[(size_t)y * w + x] = (u8)v;
        }
    free(t);
}

int pc_stroke_selection(PcDoc *d, int width, int location, u32 rgb, int opacity, int blend)
{
    size_t i, n = (size_t)d->s.w * d->s.h;
    u8 *a, *b, *ring;
    int rc, rin, rout;
    if (!d->s.sel) return -1;
    width = PC_CLAMP(width, 1, 16);
    a = (u8 *)malloc(n); b = (u8 *)malloc(n); ring = (u8 *)malloc(n);
    if (!a || !b || !ring) { free(a); free(b); free(ring); return -1; }
    memcpy(a, d->s.sel->px, n); memcpy(b, d->s.sel->px, n);
    for (i = 0; i < n; i++) { a[i] = a[i] >= 128 ? 255 : 0; b[i] = a[i]; }
    rout = location == 0 ? 0 : location == 1 ? width / 2 : width;
    rin = width - rout;
    dilate_erode(a, d->s.w, d->s.h, rout, 1);
    dilate_erode(b, d->s.w, d->s.h, rin, 0);
    for (i = 0; i < n; i++) ring[i] = (u8)pc_mul255(a[i], 255 - b[i]);
    rc = fill_with(d, ring, src_color, &rgb, opacity, blend, 0);
    free(a); free(b); free(ring);
    return rc;
}

/* ---------------- paint bucket ---------------- */

int pc_bucket_fill(PcDoc *d, int x, int y, u32 rgb, int tol, int contiguous, int aa, int merged, int use_pattern, int opacity, int blend)
{
    PcLayer *l = pc_doc_active(d);
    PcBuf *src;
    u8 *m, seed[4];
    int w = d->s.w, h = d->s.h, rc;
    size_t i, n = (size_t)w * h;
    if (!l || x < 0 || y < 0 || x >= w || y >= h) return -1;
    src = merged ? pc_doc_composite(d) : pc_buf_ref(l->pix);
    if (!src) return -1;
    m = (u8 *)pc_alloc_checked(n, 1, 1, 1);
    if (!m) { pc_buf_unref(src); return -1; }
    memcpy(seed, src->px + ((size_t)y * w + x) * 4, 4);
#define MATCH(p) (abs((p)[0] - seed[0]) <= tol && abs((p)[1] - seed[1]) <= tol && abs((p)[2] - seed[2]) <= tol && abs((p)[3] - seed[3]) <= tol)
    if (!contiguous) {
        for (i = 0; i < n; i++) if (MATCH(src->px + i * 4)) m[i] = 255;
    } else {
        size_t cap = 4096, sp = 1;
        int *st = (int *)malloc(sizeof(int) * 2 * cap);
        if (!st) { free(m); pc_buf_unref(src); return -1; }
        st[0] = x; st[1] = y;
        while (sp) {
            int cx, cy, lx, rx, dy;
            sp--; cx = st[sp * 2]; cy = st[sp * 2 + 1];
            if (m[(size_t)cy * w + cx]) continue;
            lx = rx = cx;
            while (lx > 0 && !m[(size_t)cy * w + lx - 1] && MATCH(src->px + ((size_t)cy * w + lx - 1) * 4)) lx--;
            while (rx < w - 1 && !m[(size_t)cy * w + rx + 1] && MATCH(src->px + ((size_t)cy * w + rx + 1) * 4)) rx++;
            memset(m + (size_t)cy * w + lx, 255, (size_t)(rx - lx + 1));
            for (dy = -1; dy <= 1; dy += 2) {
                int ny = cy + dy, k, prev = 0;
                if (ny < 0 || ny >= h) continue;
                for (k = lx; k <= rx; k++) {
                    int ok = !m[(size_t)ny * w + k] && MATCH(src->px + ((size_t)ny * w + k) * 4);
                    if (ok && !prev) {
                        if (sp >= cap) {
                            int *ns = (int *)realloc(st, sizeof(int) * 4 * cap);
                            if (!ns) break;
                            st = ns; cap *= 2;
                        }
                        st[sp * 2] = k; st[sp * 2 + 1] = ny; sp++;
                    }
                    prev = ok;
                }
            }
        }
        free(st);
    }
#undef MATCH
    pc_buf_unref(src);
    if (aa) {
        /* One-pixel soft edge like PS's anti-aliased fill. */
        u8 *t = (u8 *)malloc(n);
        if (t) {
            int xx, yy;
            memcpy(t, m, n);
            for (yy = 0; yy < h; yy++)
                for (xx = 0; xx < w; xx++) {
                    size_t k = (size_t)yy * w + xx;
                    if (t[k]) continue;
                    if ((xx > 0 && t[k - 1]) || (xx < w - 1 && t[k + 1]) || (yy > 0 && t[k - w]) || (yy < h - 1 && t[k + w])) m[k] = 96;
                }
            free(t);
        }
    }
    for (i = 0; i < n; i++) if (d->s.sel) m[i] = (u8)pc_mul255(m[i], d->s.sel->px[i]);
    rc = use_pattern ? fill_with(d, m, src_pattern, NULL, opacity, blend, 0) : fill_with(d, m, src_color, &rgb, opacity, blend, 0);
    free(m);
    return rc;
}

/* ---------------- gradient ---------------- */

typedef struct {
    const PcGradient *g;
    float x0, y0, dx, dy, len2, len;
    unsigned seed;
} GradCtx;

static void grad_color(const PcGradient *g, float t, u8 *o)
{
    int r0, g0, b0, a0, r1, g1, b1, a1;
    if (g->reverse) t = 1 - t;
    t = PC_CLAMP(t, 0, 1);
    switch (g->preset) {
    case 1: r0 = r1 = pc_r(g->fg); g0 = g1 = pc_g(g->fg); b0 = b1 = pc_b(g->fg); a0 = 255; a1 = 0; break;
    case 2: r0 = r1 = pc_r(g->fg); g0 = g1 = pc_g(g->fg); b0 = b1 = pc_b(g->fg); a0 = 0; a1 = 255; break;
    case 3: r0 = g0 = b0 = 0; r1 = g1 = b1 = 255; a0 = a1 = 255; break;
    case 4: {
        int r, gg, b;
        pc_hsb_to_rgb(t * 300.0f, 1, 1, &r, &gg, &b);
        o[0] = (u8)r; o[1] = (u8)gg; o[2] = (u8)b; o[3] = 255;
        return;
    }
    default: r0 = pc_r(g->fg); g0 = pc_g(g->fg); b0 = pc_b(g->fg); r1 = pc_r(g->bg); g1 = pc_g(g->bg); b1 = pc_b(g->bg); a0 = a1 = 255; break;
    }
    if (!g->transparency) a0 = a1 = 255;
    o[0] = (u8)(r0 + (r1 - r0) * t + 0.5f); o[1] = (u8)(g0 + (g1 - g0) * t + 0.5f);
    o[2] = (u8)(b0 + (b1 - b0) * t + 0.5f); o[3] = (u8)(a0 + (a1 - a0) * t + 0.5f);
}

static void src_grad(int x, int y, u8 *o, void *ctx)
{
    GradCtx *c = (GradCtx *)ctx;
    float px = x + 0.5f - c->x0, py = y + 0.5f - c->y0, t;
    switch (c->g->type) {
    case PC_GRAD_RADIAL: t = c->len > 0 ? (float)sqrt(px * px + py * py) / c->len : 0; break;
    case PC_GRAD_ANGLE: {
        double a0 = atan2(c->dy, c->dx), a = atan2(py, px) - a0;
        while (a < 0) a += 2 * M_PI;
        while (a >= 2 * M_PI) a -= 2 * M_PI;
        t = (float)(1 - a / (2 * M_PI));
        break;
    }
    case PC_GRAD_REFLECTED: t = c->len2 > 0 ? (float)fabs((px * c->dx + py * c->dy) / c->len2) : 0; break;
    case PC_GRAD_DIAMOND: {
        /* Distance in the rotated frame, L-infinity. */
        float u = c->len > 0 ? (px * c->dx + py * c->dy) / c->len : 0;
        float v = c->len > 0 ? (-px * c->dy + py * c->dx) / c->len : 0;
        t = c->len > 0 ? PC_MAX((float)fabs(u), (float)fabs(v)) / c->len : 0;
        break;
    }
    default: t = c->len2 > 0 ? (px * c->dx + py * c->dy) / c->len2 : 0; break;
    }
    if (c->g->dither) {
        /* +-0.5 level ordered dither removes banding like PS's Dither option. */
        static const int bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
        t += (bayer[(y & 3) * 4 + (x & 3)] - 7.5f) / 16.0f / 255.0f;
    }
    grad_color(c->g, t, o);
}

int pc_gradient_fill(PcDoc *d, const PcGradient *g, float x0, float y0, float x1, float y1)
{
    GradCtx c;
    c.g = g; c.x0 = x0; c.y0 = y0; c.dx = x1 - x0; c.dy = y1 - y0;
    c.len2 = c.dx * c.dx + c.dy * c.dy; c.len = (float)sqrt(c.len2); c.seed = 1;
    if (c.len2 < 0.5f) return -1;
    return fill_with(d, NULL, src_grad, &c, g->opacity, g->blend, 0);
}
