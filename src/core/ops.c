/*
 * Shared operation helpers: applying results through the selection, colour
 * space conversions, PRNG, clipboard.
 */
#include "pcops.h"

unsigned pc_rand(unsigned *s)
{
    /* xorshift32 */
    unsigned x = *s ? *s : 2463534242u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

int pc_apply_result_masked(PcDoc *d, PcLayer *l, const u8 *res, const u8 *weights)
{
    size_t i, n = (size_t)d->s.w * d->s.h;
    const u8 *sel = weights ? weights : (d->s.sel ? d->s.sel->px : NULL);
    u8 *p;
    if (!l || l->lock_pixels) return -1;
    p = pc_layer_pixels_w(l);
    if (!p) return -1;
    for (i = 0; i < n; i++, p += 4, res += 4) {
        int w = sel ? sel[i] : 255, j;
        int lock = l->lock_alpha || l->is_background;
        if (!w) continue;
        if (w == 255) {
            p[0] = res[0]; p[1] = res[1]; p[2] = res[2];
            if (!lock) p[3] = res[3];
        } else {
            for (j = 0; j < 3; j++) p[j] = (u8)((res[j] * w + p[j] * (255 - w) + 127) / 255);
            if (!lock) p[3] = (u8)((res[3] * w + p[3] * (255 - w) + 127) / 255);
        }
        if (d->s.mode == PC_MODE_GRAY) p[0] = p[1] = p[2] = (u8)pc_luma(p[0], p[1], p[2]);
    }
    pc_doc_touch(d);
    return 0;
}

int pc_apply_result(PcDoc *d, PcLayer *l, const u8 *res) { return pc_apply_result_masked(d, l, res, NULL); }

/* ---------------- colour spaces ---------------- */

void pc_rgb_to_hsl(int r, int g, int b, float *h, float *s, float *l)
{
    float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    float mx = PC_MAX(rf, PC_MAX(gf, bf)), mn = PC_MIN(rf, PC_MIN(gf, bf)), d = mx - mn;
    *l = (mx + mn) / 2;
    if (d <= 0) { *h = 0; *s = 0; return; }
    *s = *l > 0.5f ? d / (2 - mx - mn) : d / (mx + mn);
    if (mx == rf) *h = (gf - bf) / d + (gf < bf ? 6 : 0);
    else if (mx == gf) *h = (bf - rf) / d + 2;
    else *h = (rf - gf) / d + 4;
    *h *= 60;
}

static float hue2rgb(float p, float q, float t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
}

void pc_hsl_to_rgb(float h, float s, float l, int *r, int *g, int *b)
{
    float rf, gf, bf;
    h = (float)fmod(h, 360); if (h < 0) h += 360;
    s = PC_CLAMP(s, 0, 1); l = PC_CLAMP(l, 0, 1);
    if (s <= 0) { rf = gf = bf = l; }
    else {
        float q = l < 0.5f ? l * (1 + s) : l + s - l * s, p = 2 * l - q, hh = h / 360;
        rf = hue2rgb(p, q, hh + 1.0f / 3); gf = hue2rgb(p, q, hh); bf = hue2rgb(p, q, hh - 1.0f / 3);
    }
    *r = (int)(rf * 255 + 0.5f); *g = (int)(gf * 255 + 0.5f); *b = (int)(bf * 255 + 0.5f);
}

void pc_rgb_to_hsb(int r, int g, int b, float *h, float *s, float *v)
{
    int mx = PC_MAX(r, PC_MAX(g, b)), mn = PC_MIN(r, PC_MIN(g, b)), d = mx - mn;
    *v = mx / 255.0f;
    *s = mx ? (float)d / mx : 0;
    if (!d) { *h = 0; return; }
    if (mx == r) *h = 60.0f * (g - b) / d;
    else if (mx == g) *h = 60.0f * (b - r) / d + 120;
    else *h = 60.0f * (r - g) / d + 240;
    if (*h < 0) *h += 360;
}

void pc_hsb_to_rgb(float h, float s, float v, int *r, int *g, int *b)
{
    float c, x, m, rf, gf, bf;
    int sect;
    h = (float)fmod(h, 360); if (h < 0) h += 360;
    c = v * s; sect = (int)(h / 60); x = c * (1 - (float)fabs(fmod(h / 60, 2) - 1)); m = v - c;
    switch (sect) {
    case 0: rf = c; gf = x; bf = 0; break;
    case 1: rf = x; gf = c; bf = 0; break;
    case 2: rf = 0; gf = c; bf = x; break;
    case 3: rf = 0; gf = x; bf = c; break;
    case 4: rf = x; gf = 0; bf = c; break;
    default: rf = c; gf = 0; bf = x; break;
    }
    *r = (int)((rf + m) * 255 + 0.5f); *g = (int)((gf + m) * 255 + 0.5f); *b = (int)((bf + m) * 255 + 0.5f);
}

static float srgb_lin(float c) { return c <= 0.04045f ? c / 12.92f : (float)pow((c + 0.055f) / 1.055f, 2.4); }
static float lin_srgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * (float)pow(c, 1 / 2.4) - 0.055f; }
static float lab_f(float t) { return t > 0.008856f ? (float)pow(t, 1.0 / 3) : 7.787f * t + 16.0f / 116; }
static float lab_finv(float t) { return t > 0.206893f ? t * t * t : (t - 16.0f / 116) / 7.787f; }

void pc_rgb_to_lab(int r, int g, int b, float *L, float *A, float *B)
{
    float rl = srgb_lin(r / 255.0f), gl = srgb_lin(g / 255.0f), bl = srgb_lin(b / 255.0f);
    float x = (0.4124f * rl + 0.3576f * gl + 0.1805f * bl) / 0.95047f;
    float y = 0.2126f * rl + 0.7152f * gl + 0.0722f * bl;
    float z = (0.0193f * rl + 0.1192f * gl + 0.9505f * bl) / 1.08883f;
    float fx = lab_f(x), fy = lab_f(y), fz = lab_f(z);
    *L = 116 * fy - 16; *A = 500 * (fx - fy); *B = 200 * (fy - fz);
}

void pc_lab_to_rgb(float L, float A, float B, int *r, int *g, int *bb)
{
    float fy = (L + 16) / 116, fx = fy + A / 500, fz = fy - B / 200;
    float x = lab_finv(fx) * 0.95047f, y = lab_finv(fy), z = lab_finv(fz) * 1.08883f;
    float rl = 3.2406f * x - 1.5372f * y - 0.4986f * z;
    float gl = -0.9689f * x + 1.8758f * y + 0.0415f * z;
    float bl = 0.0557f * x - 0.2040f * y + 1.0570f * z;
    int v;
    v = (int)(lin_srgb(PC_CLAMP(rl, 0, 1)) * 255 + 0.5f); *r = PC_CLAMP(v, 0, 255);
    v = (int)(lin_srgb(PC_CLAMP(gl, 0, 1)) * 255 + 0.5f); *g = PC_CLAMP(v, 0, 255);
    v = (int)(lin_srgb(PC_CLAMP(bl, 0, 1)) * 255 + 0.5f); *bb = PC_CLAMP(v, 0, 255);
}

void pc_rgb_to_cmyk(int r, int g, int b, int *c, int *m, int *y, int *k)
{
    int kk = 255 - PC_MAX(r, PC_MAX(g, b));
    if (kk == 255) { *c = *m = *y = 0; *k = 100; return; }
    *c = (255 - r - kk) * 100 / (255 - kk);
    *m = (255 - g - kk) * 100 / (255 - kk);
    *y = (255 - b - kk) * 100 / (255 - kk);
    *k = kk * 100 / 255;
}

void pc_cmyk_to_rgb(int c, int m, int y, int k, int *r, int *g, int *b)
{
    *r = 255 * (100 - c) * (100 - k) / 10000;
    *g = 255 * (100 - m) * (100 - k) / 10000;
    *b = 255 * (100 - y) * (100 - k) / 10000;
}

/* ---------------- clipboard ---------------- */

void pc_clip_free(PcClip *c) { free(c->rgba); c->rgba = NULL; c->w = c->h = 0; }

int pc_copy(PcDoc *d, int merged, PcClip *out)
{
    PcRect r;
    PcBuf *src = NULL;
    PcLayer *l = pc_doc_active(d);
    int x, y, w = d->s.w;
    if (!pc_sel_bounds(d, &r)) { r.x0 = 0; r.y0 = 0; r.x1 = d->s.w; r.y1 = d->s.h; }
    if (merged) src = pc_doc_composite(d);
    else if (l) src = pc_buf_ref(l->pix);
    if (!src) return -1;
    out->w = r.x1 - r.x0; out->h = r.y1 - r.y0; out->x = r.x0; out->y = r.y0;
    out->rgba = (u8 *)pc_alloc_checked((size_t)out->w, (size_t)out->h, 4, 0);
    if (!out->rgba) { pc_buf_unref(src); return -1; }
    for (y = r.y0; y < r.y1; y++)
        for (x = r.x0; x < r.x1; x++) {
            const u8 *s = src->px + ((size_t)y * w + x) * 4;
            u8 *o = out->rgba + ((size_t)(y - r.y0) * out->w + (x - r.x0)) * 4;
            int a = s[3];
            if (!merged && l && l->mask && l->mask_enabled) a = pc_mul255(a, l->mask->px[(size_t)y * w + x]);
            a = pc_mul255(a, pc_sel_at(d, x, y));
            o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = (u8)a;
        }
    pc_buf_unref(src);
    return 0;
}

int pc_clear(PcDoc *d, u32 bg)
{
    PcLayer *l = pc_doc_active(d);
    size_t i, n = (size_t)d->s.w * d->s.h;
    u8 *p;
    if (!l || l->lock_pixels) return -1;
    p = pc_layer_pixels_w(l);
    if (!p) return -1;
    for (i = 0; i < n; i++, p += 4) {
        int s = d->s.sel ? d->s.sel->px[i] : 255;
        if (!s) continue;
        if (l->is_background || l->lock_alpha) {
            p[0] = (u8)((pc_r(bg) * s + p[0] * (255 - s)) / 255);
            p[1] = (u8)((pc_g(bg) * s + p[1] * (255 - s)) / 255);
            p[2] = (u8)((pc_b(bg) * s + p[2] * (255 - s)) / 255);
        } else {
            p[3] = (u8)(p[3] * (255 - s) / 255);
        }
    }
    pc_doc_touch(d);
    return 0;
}

int pc_paste(PcDoc *d, const PcClip *c, int into)
{
    PcLayer *l;
    PcRect r;
    int ox, oy, x, y, w = d->s.w, h = d->s.h;
    u8 *p;
    if (!c->rgba || c->w <= 0 || c->h <= 0) return -1;
    if (into && pc_sel_bounds(d, &r)) {
        ox = (r.x0 + r.x1 - c->w) / 2; oy = (r.y0 + r.y1 - c->h) / 2;
    } else if (c->x >= 0 && c->y >= 0 && c->x + c->w <= w && c->y + c->h <= h && c->w < w) {
        ox = c->x; oy = c->y;
    } else {
        ox = (w - c->w) / 2; oy = (h - c->h) / 2;
    }
    l = pc_doc_add_layer(d, NULL);
    if (!l) return -1;
    p = l->pix->px;
    for (y = 0; y < c->h; y++) {
        int dy = oy + y;
        if (dy < 0 || dy >= h) continue;
        for (x = 0; x < c->w; x++) {
            int dx = ox + x;
            if (dx < 0 || dx >= w) continue;
            memcpy(p + ((size_t)dy * w + dx) * 4, c->rgba + ((size_t)y * c->w + x) * 4, 4);
        }
    }
    if (into && d->s.sel) {
        l->mask = pc_buf_clone(d->s.sel);
        l->mask_enabled = l->mask != NULL;
    }
    if (d->s.mode == PC_MODE_GRAY) {
        size_t i, n = (size_t)w * h;
        for (i = 0; i < n; i++) p[i * 4] = p[i * 4 + 1] = p[i * 4 + 2] = (u8)pc_luma(p[i * 4], p[i * 4 + 1], p[i * 4 + 2]);
    }
    pc_sel_clear(d);
    pc_doc_touch(d);
    return 0;
}
