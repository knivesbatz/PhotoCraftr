/*
 * Image Size, Canvas Size, Crop, Rotate Canvas, Flip, layer transforms,
 * mode conversion.  Resampling kernels follow PhotoCraft
 * crates/algo/src/resample.rs (bicubic with a = -0.5, box-filtered
 * downscaling) in a simpler fixed form.
 */
#include "pcops.h"

/* Apply fn to every buffer of the document (layers, masks, selection,
 * channels) producing new buffers of size nw x nh. */
typedef PcBuf *(*BufXform)(const PcBuf *b, int nw, int nh, void *ctx);

static int xform_doc(PcDoc *d, int nw, int nh, BufXform fn, void *ctx)
{
    int i;
    PcBuf *nb;
    PcDocState ns;
    if (nw <= 0 || nh <= 0 || nw > PC_MAX_LAYERS * 1000 || (size_t)nw * nh > (size_t)100 * 1000 * 1000) return -1;
    /* Build into a copy so a failure leaves the document unchanged. */
    pc_state_copy(&ns, &d->s);
    for (i = 0; i < ns.nlayers; i++) {
        PcLayer *l = ns.layers[i];
        if (l->pix) {                           /* (adjustment layers have none) */
            nb = fn(l->pix, nw, nh, ctx);
            if (!nb) goto fail;
            pc_buf_unref(l->pix); l->pix = nb;
        }
        if (l->mask) {
            nb = fn(l->mask, nw, nh, ctx);
            if (!nb) goto fail;
            pc_buf_unref(l->mask); l->mask = nb;
        }
    }
    if (ns.sel) {
        nb = fn(ns.sel, nw, nh, ctx);
        if (!nb) goto fail;
        pc_buf_unref(ns.sel); ns.sel = nb;
    }
    for (i = 0; i < ns.nchannels; i++) {
        nb = fn(ns.channels[i].buf, nw, nh, ctx);
        if (!nb) goto fail;
        pc_buf_unref(ns.channels[i].buf); ns.channels[i].buf = nb;
    }
    ns.w = nw; ns.h = nh;
    pc_state_release(&d->s);
    d->s = ns;
    pc_doc_touch(d);
    return 0;
fail:
    pc_state_release(&ns);
    return -1;
}

/* ---------------- resampling ---------------- */

static float cubic(float x)
{
    const float a = -0.5f;
    x = x < 0 ? -x : x;
    if (x < 1) return ((a + 2) * x - (a + 3)) * x * x + 1;
    if (x < 2) return ((a * x - 5 * a) * x + 8 * a) * x - 4 * a;
    return 0;
}

/* Generic separable resampler on `ch`-channel images. RGBA colour is
 * premultiplied during filtering. */
static void resample(const u8 *src, int sw, int sh, u8 *dst, int dw, int dh, int ch, int method)
{
    int x, y, c, k;
    float *tmp, *pre = NULL;
    const int pm = ch == 4;
    if (method == PC_RESAMPLE_NEAREST) {
        for (y = 0; y < dh; y++) {
            int sy = PC_MIN((int)((y + 0.5) * sh / dh), sh - 1);
            for (x = 0; x < dw; x++) {
                int sx = PC_MIN((int)((x + 0.5) * sw / dw), sw - 1);
                memcpy(dst + ((size_t)y * dw + x) * ch, src + ((size_t)sy * sw + sx) * ch, (size_t)ch);
            }
        }
        return;
    }
    tmp = (float *)pc_alloc_checked((size_t)dw * sh, (size_t)ch, sizeof(float), 0);
    if (pm) pre = (float *)pc_alloc_checked((size_t)sw * sh, 4, sizeof(float), 0);
    if (!tmp || (pm && !pre)) { free(tmp); free(pre); memset(dst, 0, (size_t)dw * dh * ch); return; }
    if (pm) {
        size_t i, n = (size_t)sw * sh;
        for (i = 0; i < n; i++) {
            float a = src[i * 4 + 3] / 255.0f;
            pre[i * 4] = src[i * 4] * a; pre[i * 4 + 1] = src[i * 4 + 1] * a; pre[i * 4 + 2] = src[i * 4 + 2] * a; pre[i * 4 + 3] = src[i * 4 + 3];
        }
    }
#define SRCV(i) (pm ? pre[i] : (float)src[i])
    /* Horizontal pass. */
    {
        float scale = (float)sw / dw, support = method == PC_RESAMPLE_BICUBIC ? 2.0f : 1.0f;
        float fs = scale > 1 ? scale : 1;
        for (x = 0; x < dw; x++) {
            float center = (x + 0.5f) * scale - 0.5f;
            int lo = (int)floor(center - support * fs), hi = (int)ceil(center + support * fs);
            float wsum = 0, wts[512];
            int nw = 0;
            if (hi - lo + 1 > 512) hi = lo + 511;
            for (k = lo; k <= hi; k++) {
                float t = (k - center) / fs;
                float wv = method == PC_RESAMPLE_BICUBIC ? cubic(t) : (1 - (float)fabs(t) > 0 ? 1 - (float)fabs(t) : 0);
                wts[nw++] = wv; wsum += wv;
            }
            if (wsum == 0) wsum = 1;
            for (y = 0; y < sh; y++) {
                float acc[4] = { 0, 0, 0, 0 };
                for (k = lo; k <= hi; k++) {
                    int sx = PC_CLAMP(k, 0, sw - 1);
                    float wv = wts[k - lo];
                    for (c = 0; c < ch; c++) acc[c] += SRCV(((size_t)y * sw + sx) * ch + c) * wv;
                }
                for (c = 0; c < ch; c++) tmp[((size_t)y * dw + x) * ch + c] = acc[c] / wsum;
            }
        }
    }
    /* Vertical pass. */
    {
        float scale = (float)sh / dh, support = method == PC_RESAMPLE_BICUBIC ? 2.0f : 1.0f;
        float fs = scale > 1 ? scale : 1;
        for (y = 0; y < dh; y++) {
            float center = (y + 0.5f) * scale - 0.5f;
            int lo = (int)floor(center - support * fs), hi = (int)ceil(center + support * fs);
            float wsum = 0, wts[512];
            int nw = 0;
            if (hi - lo + 1 > 512) hi = lo + 511;
            for (k = lo; k <= hi; k++) {
                float t = (k - center) / fs;
                float wv = method == PC_RESAMPLE_BICUBIC ? cubic(t) : (1 - (float)fabs(t) > 0 ? 1 - (float)fabs(t) : 0);
                wts[nw++] = wv; wsum += wv;
            }
            if (wsum == 0) wsum = 1;
            for (x = 0; x < dw; x++) {
                float acc[4] = { 0, 0, 0, 0 };
                u8 *o = dst + ((size_t)y * dw + x) * ch;
                for (k = lo; k <= hi; k++) {
                    int sy = PC_CLAMP(k, 0, sh - 1);
                    float wv = wts[k - lo];
                    for (c = 0; c < ch; c++) acc[c] += tmp[((size_t)sy * dw + x) * ch + c] * wv;
                }
                for (c = 0; c < ch; c++) acc[c] /= wsum;
                if (pm) {
                    float a = PC_CLAMP(acc[3], 0, 255);
                    for (c = 0; c < 3; c++) { float v = a > 0.5f ? acc[c] * 255 / a : 0; o[c] = (u8)PC_CLAMP((int)(v + 0.5f), 0, 255); }
                    o[3] = (u8)(a + 0.5f);
                } else {
                    for (c = 0; c < ch; c++) o[c] = (u8)PC_CLAMP((int)(acc[c] + 0.5f), 0, 255);
                }
            }
        }
    }
#undef SRCV
    free(tmp); free(pre);
}

void pc_resample_rgba(const u8 *src, int sw, int sh, u8 *dst, int dw, int dh, int method)
{
    resample(src, sw, sh, dst, dw, dh, 4, method);
}

static PcBuf *xf_resize(const PcBuf *b, int nw, int nh, void *ctx)
{
    PcBuf *o = pc_buf_new(nw, nh, b->ch, 0);
    if (o) resample(b->px, b->w, b->h, o->px, nw, nh, b->ch, *(int *)ctx);
    return o;
}

int pc_image_resize(PcDoc *d, int nw, int nh, int method, double dpi)
{
    int rc = 0;
    if (nw != d->s.w || nh != d->s.h) rc = xform_doc(d, nw, nh, xf_resize, &method);
    if (!rc && dpi > 0) d->s.dpi = dpi;
    /* Text layers become pixels when resampled (their geometry changed). */
    if (!rc) {
        int i;
        for (i = 0; i < d->s.nlayers; i++) if (d->s.layers[i]->kind == PC_LAYER_TEXT) d->s.layers[i]->kind = PC_LAYER_PIXEL;
    }
    return rc;
}

/* ---------------- canvas / crop ---------------- */

typedef struct { int ox, oy; u32 bg; int bg_layer_fill; } CanvasCtx;

static PcBuf *xf_canvas(const PcBuf *b, int nw, int nh, void *ctx)
{
    const CanvasCtx *c = (const CanvasCtx *)ctx;
    PcBuf *o = pc_buf_new(nw, nh, b->ch, 1);
    int y;
    if (!o) return NULL;
    for (y = 0; y < nh; y++) {
        int sy = y - c->oy, x0 = PC_MAX(0, c->ox), x1 = PC_MIN(nw, c->ox + b->w);
        if (sy < 0 || sy >= b->h || x0 >= x1) continue;
        memcpy(o->px + ((size_t)y * nw + x0) * b->ch, b->px + ((size_t)sy * b->w + (x0 - c->ox)) * b->ch, (size_t)(x1 - x0) * b->ch);
    }
    return o;
}

int pc_canvas_resize(PcDoc *d, int nw, int nh, int anchor, u32 bg)
{
    CanvasCtx c;
    int ow = d->s.w, oh = d->s.h, rc, ax = anchor % 3, ay = anchor / 3;
    c.ox = ax == 0 ? 0 : ax == 1 ? (nw - ow) / 2 : nw - ow;
    c.oy = ay == 0 ? 0 : ay == 1 ? (nh - oh) / 2 : nh - oh;
    c.bg = bg; c.bg_layer_fill = 1;
    rc = xform_doc(d, nw, nh, xf_canvas, &c);
    if (!rc && d->s.nlayers && d->s.layers[0]->is_background) {
        /* New canvas area of the Background takes the background colour. */
        PcLayer *l = d->s.layers[0];
        u8 *p = pc_layer_pixels_w(l);
        int x, y;
        if (p) for (y = 0; y < nh; y++) for (x = 0; x < nw; x++) {
            u8 *q = p + ((size_t)y * nw + x) * 4;
            if (x < c.ox || y < c.oy || x >= c.ox + ow || y >= c.oy + oh) { q[0] = (u8)pc_r(bg); q[1] = (u8)pc_g(bg); q[2] = (u8)pc_b(bg); q[3] = 255; }
        }
    }
    if (!rc) {
        int i;
        for (i = 0; i < d->s.nlayers; i++) if (d->s.layers[i]->kind == PC_LAYER_TEXT) { d->s.layers[i]->text.x += c.ox; d->s.layers[i]->text.y += c.oy; }
    }
    return rc;
}

int pc_image_crop(PcDoc *d, const PcRect *r)
{
    CanvasCtx c;
    int rc, i;
    PcRect q = *r;
    q.x0 = PC_MAX(q.x0, 0); q.y0 = PC_MAX(q.y0, 0);
    q.x1 = PC_MIN(q.x1, d->s.w); q.y1 = PC_MIN(q.y1, d->s.h);
    if (q.x1 - q.x0 < 1 || q.y1 - q.y0 < 1) return -1;
    c.ox = -q.x0; c.oy = -q.y0; c.bg = 0; c.bg_layer_fill = 0;
    rc = xform_doc(d, q.x1 - q.x0, q.y1 - q.y0, xf_canvas, &c);
    if (!rc) {
        for (i = 0; i < d->s.nlayers; i++) if (d->s.layers[i]->kind == PC_LAYER_TEXT) { d->s.layers[i]->text.x -= q.x0; d->s.layers[i]->text.y -= q.y0; }
        pc_buf_unref(d->s.sel); d->s.sel = NULL;
    }
    return rc;
}

/* ---------------- rotation / flip ---------------- */

static PcBuf *xf_rot(const PcBuf *b, int nw, int nh, void *ctx)
{
    int q = *(int *)ctx, x, y, ch = b->ch;
    PcBuf *o = pc_buf_new(nw, nh, ch, 0);
    if (!o) return NULL;
    for (y = 0; y < b->h; y++)
        for (x = 0; x < b->w; x++) {
            int nx, ny;
            if (q == 1) { nx = b->h - 1 - y; ny = x; }
            else if (q == 2) { nx = b->w - 1 - x; ny = b->h - 1 - y; }
            else { nx = y; ny = b->w - 1 - x; }
            memcpy(o->px + ((size_t)ny * nw + nx) * ch, b->px + ((size_t)y * b->w + x) * ch, (size_t)ch);
        }
    return o;
}

int pc_image_rotate(PcDoc *d, int q)
{
    int rc, i;
    q = ((q % 4) + 4) % 4;
    if (!q) return 0;
    rc = xform_doc(d, q == 2 ? d->s.w : d->s.h, q == 2 ? d->s.h : d->s.w, xf_rot, &q);
    if (!rc) for (i = 0; i < d->s.nlayers; i++) if (d->s.layers[i]->kind == PC_LAYER_TEXT) d->s.layers[i]->kind = PC_LAYER_PIXEL;
    return rc;
}

static PcBuf *xf_flip(const PcBuf *b, int nw, int nh, void *ctx)
{
    int v = *(int *)ctx, x, y, ch = b->ch;
    PcBuf *o = pc_buf_new(nw, nh, ch, 0);
    if (!o) return NULL;
    for (y = 0; y < nh; y++)
        for (x = 0; x < nw; x++) {
            int sx = v ? x : nw - 1 - x, sy = v ? nh - 1 - y : y;
            memcpy(o->px + ((size_t)y * nw + x) * ch, b->px + ((size_t)sy * nw + sx) * ch, (size_t)ch);
        }
    return o;
}

int pc_image_flip(PcDoc *d, int vertical)
{
    int rc = xform_doc(d, d->s.w, d->s.h, xf_flip, &vertical), i;
    if (!rc) for (i = 0; i < d->s.nlayers; i++) if (d->s.layers[i]->kind == PC_LAYER_TEXT) d->s.layers[i]->kind = PC_LAYER_PIXEL;
    return rc;
}

static void rotate_rgba(const u8 *src, int sw, int sh, u8 *dst, int dw, int dh, double deg, int method)
{
    double a = deg * M_PI / 180.0, ca = cos(a), sa = sin(a);
    double cx = sw / 2.0, cy = sh / 2.0, dcx = dw / 2.0, dcy = dh / 2.0;
    int x, y, c;
    for (y = 0; y < dh; y++)
        for (x = 0; x < dw; x++) {
            double dx = x + 0.5 - dcx, dy = y + 0.5 - dcy;
            double sx = dx * ca + dy * sa + cx - 0.5, sy = -dx * sa + dy * ca + cy - 0.5;
            u8 *o = dst + ((size_t)y * dw + x) * 4;
            int x0 = (int)floor(sx), y0 = (int)floor(sy);
            if (sx < -1 || sy < -1 || sx > sw || sy > sh) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
            if (method == PC_RESAMPLE_NEAREST) {
                int nx = (int)floor(sx + 0.5), ny = (int)floor(sy + 0.5);
                if (nx < 0 || ny < 0 || nx >= sw || ny >= sh) { o[0] = o[1] = o[2] = o[3] = 0; }
                else memcpy(o, src + ((size_t)ny * sw + nx) * 4, 4);
                continue;
            }
            {
                double fx = sx - x0, fy = sy - y0, acc[4] = { 0, 0, 0, 0 };
                int i, j;
                for (j = 0; j < 2; j++)
                    for (i = 0; i < 2; i++) {
                        int px = x0 + i, py = y0 + j;
                        double wv = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
                        const u8 *s;
                        if (px < 0 || py < 0 || px >= sw || py >= sh) continue;
                        s = src + ((size_t)py * sw + px) * 4;
                        for (c = 0; c < 3; c++) acc[c] += s[c] * (s[3] / 255.0) * wv;
                        acc[3] += s[3] * wv;
                    }
                o[3] = (u8)PC_CLAMP((int)(acc[3] + 0.5), 0, 255);
                for (c = 0; c < 3; c++) o[c] = acc[3] > 0.5 ? (u8)PC_CLAMP((int)(acc[c] * 255 / acc[3] + 0.5), 0, 255) : 0;
            }
        }
}

typedef struct { double deg; } RotCtx;

static PcBuf *xf_rot_any(const PcBuf *b, int nw, int nh, void *ctx)
{
    const RotCtx *r = (const RotCtx *)ctx;
    PcBuf *o = pc_buf_new(nw, nh, b->ch, 1);
    if (!o) return NULL;
    if (b->ch == 4) rotate_rgba(b->px, b->w, b->h, o->px, nw, nh, r->deg, PC_RESAMPLE_BILINEAR);
    else {
        /* Single channel: expand to RGBA alpha and back. */
        u8 *t = (u8 *)pc_alloc_checked((size_t)b->w, (size_t)b->h, 4, 1), *t2 = (u8 *)pc_alloc_checked((size_t)nw, (size_t)nh, 4, 1);
        size_t i;
        if (t && t2) {
            for (i = 0; i < (size_t)b->w * b->h; i++) { t[i * 4] = t[i * 4 + 1] = t[i * 4 + 2] = 255; t[i * 4 + 3] = b->px[i]; }
            rotate_rgba(t, b->w, b->h, t2, nw, nh, r->deg, PC_RESAMPLE_BILINEAR);
            for (i = 0; i < (size_t)nw * nh; i++) o->px[i] = t2[i * 4 + 3];
        }
        free(t); free(t2);
    }
    return o;
}

int pc_image_rotate_arbitrary(PcDoc *d, double deg, u32 bg)
{
    RotCtx c;
    double a = deg * M_PI / 180.0;
    int nw = (int)ceil(fabs(d->s.w * cos(a)) + fabs(d->s.h * sin(a)) - 0.001);
    int nh = (int)ceil(fabs(d->s.w * sin(a)) + fabs(d->s.h * cos(a)) - 0.001);
    int rc, i;
    c.deg = deg;
    rc = xform_doc(d, nw, nh, xf_rot_any, &c);
    if (!rc) {
        for (i = 0; i < d->s.nlayers; i++) if (d->s.layers[i]->kind == PC_LAYER_TEXT) d->s.layers[i]->kind = PC_LAYER_PIXEL;
        if (d->s.nlayers && d->s.layers[0]->is_background) {
            u8 *p = pc_layer_pixels_w(d->s.layers[0]);
            size_t k, n = (size_t)nw * nh;
            if (p) for (k = 0; k < n; k++) {
                u8 *q = p + k * 4;
                int al = q[3];
                q[0] = (u8)((q[0] * al + pc_r(bg) * (255 - al)) / 255);
                q[1] = (u8)((q[1] * al + pc_g(bg) * (255 - al)) / 255);
                q[2] = (u8)((q[2] * al + pc_b(bg) * (255 - al)) / 255);
                q[3] = 255;
            }
        }
    }
    return rc;
}

/* ---------------- layer transforms ---------------- */

int pc_layer_bounds(const PcLayer *l, int w, int h, PcRect *r)
{
    int x, y, x0 = w, y0 = h, x1 = -1, y1 = -1;
    if (!l->pix) { r->x0 = 0; r->y0 = 0; r->x1 = w; r->y1 = h; return 1; }   /* adjustment: whole canvas */
    for (y = 0; y < h; y++) {
        const u8 *p = l->pix->px + (size_t)y * w * 4;
        for (x = 0; x < w; x++, p += 4)
            if (p[3]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; y1 = y; }
    }
    if (x1 < 0) return 0;
    r->x0 = x0; r->y0 = y0; r->x1 = x1 + 1; r->y1 = y1 + 1;
    return 1;
}

int pc_layer_offset(PcDoc *d, int dx, int dy)
{
    PcLayer *l = pc_doc_active(d);
    PcBuf *nb;
    int x, y, w = d->s.w, h = d->s.h;
    if (!l || l->lock_position || l->is_background) return -1;
    if (!l->pix && !l->mask) return -1;
    if (l->pix) {
        nb = pc_buf_new(w, h, 4, 1);
        if (!nb) return -1;
        for (y = 0; y < h; y++) {
            int sy = y - dy;
            if (sy < 0 || sy >= h) continue;
            for (x = 0; x < w; x++) {
                int sx = x - dx;
                if (sx < 0 || sx >= w) continue;
                memcpy(nb->px + ((size_t)y * w + x) * 4, l->pix->px + ((size_t)sy * w + sx) * 4, 4);
            }
        }
        pc_buf_unref(l->pix);
        l->pix = nb;
    }
    /* An adjustment layer moves its mask (outside it the mask hides). */
    if (l->mask && (l->mask_linked || !l->pix)) {
        PcBuf *nm = pc_buf_new(w, h, 1, 0);
        if (nm) {
            memset(nm->px, 0, (size_t)w * h);
            for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
                int sx = x - dx, sy = y - dy;
                nm->px[(size_t)y * w + x] = (sx >= 0 && sy >= 0 && sx < w && sy < h) ? l->mask->px[(size_t)sy * w + sx] : 0;
            }
            pc_buf_unref(l->mask); l->mask = nm;
        }
    }
    if (l->kind == PC_LAYER_TEXT) { l->text.x += dx; l->text.y += dy; }
    pc_doc_touch(d);
    return 0;
}

/* Homography from unit square to quad (Heckbert). */
static void square_to_quad(const float q[8], double m[9])
{
    double x0 = q[0], y0 = q[1], x1 = q[2], y1 = q[3], x2 = q[4], y2 = q[5], x3 = q[6], y3 = q[7];
    double sx = x0 - x1 + x2 - x3, sy = y0 - y1 + y2 - y3;
    if (fabs(sx) < 1e-9 && fabs(sy) < 1e-9) {
        m[0] = x1 - x0; m[1] = x2 - x1; m[2] = x0;
        m[3] = y1 - y0; m[4] = y2 - y1; m[5] = y0;
        m[6] = 0; m[7] = 0; m[8] = 1;
    } else {
        double dx1 = x1 - x2, dx2 = x3 - x2, dy1 = y1 - y2, dy2 = y3 - y2;
        double den = dx1 * dy2 - dx2 * dy1;
        double g = den ? (sx * dy2 - dx2 * sy) / den : 0, h = den ? (dx1 * sy - sx * dy1) / den : 0;
        m[0] = x1 - x0 + g * x1; m[1] = x3 - x0 + h * x3; m[2] = x0;
        m[3] = y1 - y0 + g * y1; m[4] = y3 - y0 + h * y3; m[5] = y0;
        m[6] = g; m[7] = h; m[8] = 1;
        /* m maps (u,v) -> quad with columns (u -> corner1, v -> corner3) */
    }
}

static int invert3(const double m[9], double o[9])
{
    double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (fabs(det) < 1e-12) return -1;
    o[0] = (m[4] * m[8] - m[5] * m[7]) / det; o[1] = (m[2] * m[7] - m[1] * m[8]) / det; o[2] = (m[1] * m[5] - m[2] * m[4]) / det;
    o[3] = (m[5] * m[6] - m[3] * m[8]) / det; o[4] = (m[0] * m[8] - m[2] * m[6]) / det; o[5] = (m[2] * m[3] - m[0] * m[5]) / det;
    o[6] = (m[3] * m[7] - m[4] * m[6]) / det; o[7] = (m[1] * m[6] - m[0] * m[7]) / det; o[8] = (m[0] * m[4] - m[1] * m[3]) / det;
    return 0;
}

int pc_layer_transform_quad(PcDoc *d, const PcRect *src, const float dst[8], int method)
{
    PcLayer *l = pc_doc_active(d);
    int w = d->s.w, h = d->s.h, x, y, c;
    double m[9], inv[9];
    u8 *orig, *p, *floating;
    PcRect sr = *src;
    size_t n = (size_t)w * h;
    if (!l || l->lock_position || l->lock_pixels) return -1;
    if (sr.x1 - sr.x0 < 1 || sr.y1 - sr.y0 < 1) return -1;
    square_to_quad(dst, m);
    if (invert3(m, inv)) return -1;
    p = pc_layer_pixels_w(l);
    if (!p) return -1;
    orig = (u8 *)malloc(n * 4);
    floating = (u8 *)malloc(n * 4);
    if (!orig || !floating) { free(orig); free(floating); return -1; }
    memcpy(orig, p, n * 4);
    /* Lift the floating pixels (selection-weighted) out of the layer. */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            size_t i = (size_t)y * w + x;
            int in = x >= sr.x0 && x < sr.x1 && y >= sr.y0 && y < sr.y1;
            int s = in ? pc_sel_at(d, x, y) : 0;
            memcpy(floating + i * 4, orig + i * 4, 4);
            floating[i * 4 + 3] = (u8)pc_mul255(orig[i * 4 + 3], s);
            if (s) {
                if (l->is_background) { p[i * 4] = p[i * 4 + 1] = p[i * 4 + 2] = 255; }
                else p[i * 4 + 3] = (u8)pc_mul255(p[i * 4 + 3], 255 - s);
            }
        }
    /* Inverse-map each destination pixel into the source rect and composite. */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            double X = x + 0.5, Y = y + 0.5;
            double ww = inv[6] * X + inv[7] * Y + inv[8];
            double u, v, sx, sy;
            u8 px[4];
            if (fabs(ww) < 1e-12) continue;
            u = (inv[0] * X + inv[1] * Y + inv[2]) / ww;
            v = (inv[3] * X + inv[4] * Y + inv[5]) / ww;
            if (u < -0.01 || v < -0.01 || u > 1.01 || v > 1.01) continue;
            sx = sr.x0 + u * (sr.x1 - sr.x0) - 0.5; sy = sr.y0 + v * (sr.y1 - sr.y0) - 0.5;
            if (method == PC_RESAMPLE_NEAREST) {
                int nx = PC_CLAMP((int)floor(sx + 0.5), 0, w - 1), ny = PC_CLAMP((int)floor(sy + 0.5), 0, h - 1);
                memcpy(px, floating + ((size_t)ny * w + nx) * 4, 4);
            } else {
                int x0 = (int)floor(sx), y0 = (int)floor(sy), i, j;
                double fx = sx - x0, fy = sy - y0, acc[4] = { 0, 0, 0, 0 };
                for (j = 0; j < 2; j++)
                    for (i = 0; i < 2; i++) {
                        int qx = PC_CLAMP(x0 + i, sr.x0, sr.x1 - 1), qy = PC_CLAMP(y0 + j, sr.y0, sr.y1 - 1);
                        const u8 *s = floating + ((size_t)qy * w + qx) * 4;
                        double wv = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
                        for (c = 0; c < 3; c++) acc[c] += s[c] * s[3] * wv;
                        acc[3] += s[3] * wv;
                    }
                px[3] = (u8)PC_CLAMP((int)(acc[3] + 0.5), 0, 255);
                for (c = 0; c < 3; c++) px[c] = acc[3] > 0.5 ? (u8)PC_CLAMP((int)(acc[c] / acc[3] + 0.5), 0, 255) : 0;
            }
            if (px[3]) pc_blend_pixel(PC_BLEND_NORMAL, p + ((size_t)y * w + x) * 4, px, px[3], 0);
        }
    free(orig); free(floating);
    if (l->kind == PC_LAYER_TEXT) l->kind = PC_LAYER_PIXEL;
    /* The selection follows the transformed pixels' bounding box. */
    if (d->s.sel) {
        float minx = dst[0], maxx = dst[0], miny = dst[1], maxy = dst[1];
        int k;
        for (k = 1; k < 4; k++) { minx = PC_MIN(minx, dst[k * 2]); maxx = PC_MAX(maxx, dst[k * 2]); miny = PC_MIN(miny, dst[k * 2 + 1]); maxy = PC_MAX(maxy, dst[k * 2 + 1]); }
        pc_sel_polygon(d, dst, 4, PC_SEL_REPLACE, 1, 0);
    }
    pc_doc_touch(d);
    return 0;
}

int pc_layer_flip(PcDoc *d, int vertical)
{
    PcLayer *l = pc_doc_active(d);
    PcRect r;
    float q[8];
    if (!l) return -1;
    if (!pc_sel_bounds(d, &r)) { r.x0 = 0; r.y0 = 0; r.x1 = d->s.w; r.y1 = d->s.h; }
    if (vertical) { q[0] = (float)r.x0; q[1] = (float)r.y1; q[2] = (float)r.x1; q[3] = (float)r.y1; q[4] = (float)r.x1; q[5] = (float)r.y0; q[6] = (float)r.x0; q[7] = (float)r.y0; }
    else { q[0] = (float)r.x1; q[1] = (float)r.y0; q[2] = (float)r.x0; q[3] = (float)r.y0; q[4] = (float)r.x0; q[5] = (float)r.y1; q[6] = (float)r.x1; q[7] = (float)r.y1; }
    return pc_layer_transform_quad(d, &r, q, PC_RESAMPLE_NEAREST);
}

int pc_layer_rotate(PcDoc *d, int quarter)
{
    PcLayer *l = pc_doc_active(d);
    PcRect r;
    float cx, cy, hw, hh, q[8];
    int k;
    if (!l) return -1;
    if (!pc_sel_bounds(d, &r) && !pc_layer_bounds(l, d->s.w, d->s.h, &r)) return -1;
    cx = (r.x0 + r.x1) / 2.0f; cy = (r.y0 + r.y1) / 2.0f; hw = (r.x1 - r.x0) / 2.0f; hh = (r.y1 - r.y0) / 2.0f;
    {
        float corners[8] = { -hw, -hh, hw, -hh, hw, hh, -hw, hh };
        quarter = ((quarter % 4) + 4) % 4;
        for (k = 0; k < 4; k++) {
            float x = corners[k * 2], y = corners[k * 2 + 1], nx = x, ny = y;
            if (quarter == 1) { nx = -y; ny = x; }
            else if (quarter == 2) { nx = -x; ny = -y; }
            else if (quarter == 3) { nx = y; ny = -x; }
            q[k * 2] = cx + nx; q[k * 2 + 1] = cy + ny;
        }
    }
    return pc_layer_transform_quad(d, &r, q, PC_RESAMPLE_NEAREST);
}

/* ---------------- mode ---------------- */

int pc_doc_to_gray(PcDoc *d)
{
    int i;
    if (d->s.mode == PC_MODE_GRAY) return -1;
    for (i = 0; i < d->s.nlayers; i++) {
        PcLayer *l = d->s.layers[i];
        u8 *p;
        size_t k, n = (size_t)d->s.w * d->s.h;
        if (!l->pix) continue;                  /* adjustment layer */
        p = pc_layer_pixels_w(l);
        if (!p) return -1;
        for (k = 0; k < n; k++, p += 4) p[0] = p[1] = p[2] = (u8)pc_luma(p[0], p[1], p[2]);
        l->text.color = (u32)pc_luma(pc_r(l->text.color), pc_g(l->text.color), pc_b(l->text.color)) * 0x010101u;
    }
    d->s.mode = PC_MODE_GRAY;
    pc_doc_touch(d);
    return 0;
}

int pc_doc_to_rgb(PcDoc *d)
{
    if (d->s.mode == PC_MODE_RGB) return -1;
    d->s.mode = PC_MODE_RGB;
    pc_doc_touch(d);
    return 0;
}

int pc_image_trim(PcDoc *d, int transparent)
{
    PcBuf *c = pc_doc_composite(d);
    PcRect r;
    int x, y, w = d->s.w, h = d->s.h, x0 = w, y0 = h, x1 = -1, y1 = -1, rc;
    u8 ref[4];
    if (!c) return -1;
    memcpy(ref, c->px, 4);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const u8 *p = c->px + ((size_t)y * w + x) * 4;
            int keep = transparent ? p[3] != 0 : memcmp(p, ref, 4) != 0;
            if (keep) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; y1 = y; }
        }
    pc_buf_unref(c);
    if (x1 < 0) return -1;
    r.x0 = x0; r.y0 = y0; r.x1 = x1 + 1; r.y1 = y1 + 1;
    rc = pc_image_crop(d, &r);
    return rc;
}

int pc_image_crop_rotated(PcDoc *d, float cx, float cy, float w, float h, float angle)
{
    int rc;
    PcRect r;
    if (fabs(angle) < 0.01) {
        r.x0 = (int)floor(cx - w / 2 + 0.5); r.y0 = (int)floor(cy - h / 2 + 0.5);
        r.x1 = r.x0 + (int)floor(w + 0.5); r.y1 = r.y0 + (int)floor(h + 0.5);
        return pc_image_crop(d, &r);
    }
    /* Rotate the canvas so the crop box is axis-aligned, then crop it. */
    {
        int ow = d->s.w, oh = d->s.h;
        double a = -angle * M_PI / 180.0;
        double nw, nh, dx, dy, ncx, ncy;
        rc = pc_image_rotate_arbitrary(d, -angle, 0xFFFFFF);
        if (rc) return rc;
        nw = d->s.w; nh = d->s.h;
        dx = cx - ow / 2.0; dy = cy - oh / 2.0;
        ncx = dx * cos(a) - dy * sin(a) + nw / 2.0;
        ncy = dx * sin(a) + dy * cos(a) + nh / 2.0;
        r.x0 = (int)floor(ncx - w / 2 + 0.5); r.y0 = (int)floor(ncy - h / 2 + 0.5);
        r.x1 = r.x0 + (int)floor(w + 0.5); r.y1 = r.y0 + (int)floor(h + 0.5);
        return pc_image_crop(d, &r);
    }
}
