/*
 * CPU compositor: the reliable baseline renderer (no GPU is ever needed).
 *
 * Blend formulas are ported from PhotoCraft crates/color/src/blend.rs
 * (MIT OR Apache-2.0): separable modes per channel, Photoshop's Soft Light
 * curve, and the W3C non-separable Hue/Saturation/Color/Luminosity with
 * ClipColor.  Separable modes are tabulated into 256x256 lookup tables on
 * first use so a Pentium composites with table lookups instead of floats.
 */
#include "pcdoc.h"

/* ---------------- separable blend functions (0..1 floats) ---------------- */

static float bl_color_burn(float cb, float cs)
{
    if (cb >= 1) return 1;
    if (cs <= 0) return 0;
    { float v = (1 - cb) / cs; return 1 - (v > 1 ? 1 : v); }
}

static float bl_color_dodge(float cb, float cs)
{
    if (cb <= 0) return 0;
    if (cs >= 1) return 1;
    { float v = cb / (1 - cs); return v > 1 ? 1 : v; }
}

static float bl_hard_light(float cb, float cs)
{
    if (cs <= 0.5f) return cb * 2 * cs;
    { float s = 2 * cs - 1; return cb + s - cb * s; }
}

static float bl_soft_light(float cb, float cs)
{
    if (cs <= 0.5f) return 2 * cb * cs + cb * cb * (1 - 2 * cs);
    {
        float d = cb <= 0.25f ? ((16 * cb - 12) * cb + 4) * cb : (float)sqrt(cb);
        return 2 * cb * (1 - cs) + d * (2 * cs - 1);
    }
}

static float blend_channel(int mode, float cb, float cs)
{
    switch (mode) {
    case PC_BLEND_MULTIPLY: return cb * cs;
    case PC_BLEND_SCREEN: return cb + cs - cb * cs;
    case PC_BLEND_OVERLAY: return bl_hard_light(cs, cb);
    case PC_BLEND_SOFT_LIGHT: return bl_soft_light(cb, cs);
    case PC_BLEND_HARD_LIGHT: return bl_hard_light(cb, cs);
    case PC_BLEND_COLOR_DODGE: return bl_color_dodge(cb, cs);
    case PC_BLEND_COLOR_BURN: return bl_color_burn(cb, cs);
    case PC_BLEND_DARKEN: return cb < cs ? cb : cs;
    case PC_BLEND_LIGHTEN: return cb > cs ? cb : cs;
    case PC_BLEND_DIFFERENCE: return cb > cs ? cb - cs : cs - cb;
    case PC_BLEND_EXCLUSION: return cb + cs - 2 * cb * cs;
    default: return cs;
    }
}

static u8 *g_lut[PC_BLEND_COUNT];   /* [cb*256 + cs] */

static const u8 *blend_lut(int mode)
{
    if (!g_lut[mode]) {
        u8 *t = (u8 *)malloc(65536);
        int b, s;
        if (!t) return NULL;
        for (b = 0; b < 256; b++)
            for (s = 0; s < 256; s++) {
                float v = blend_channel(mode, b / 255.0f, s / 255.0f);
                int iv = (int)(v * 255 + 0.5f);
                t[b * 256 + s] = (u8)PC_CLAMP(iv, 0, 255);
            }
        g_lut[mode] = t;
    }
    return g_lut[mode];
}

/* ---------------- non-separable (W3C) ---------------- */

static float lumf(const float *c) { return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

static void clip_color(float *c)
{
    float l = lumf(c);
    float n = PC_MIN(c[0], PC_MIN(c[1], c[2]));
    float x = PC_MAX(c[0], PC_MAX(c[1], c[2]));
    int i;
    if (n < 0) {
        float d = l - n;
        for (i = 0; i < 3; i++) c[i] = (l > 0 && d > 0) ? l + (c[i] - l) * l / d : l;
    }
    if (x > 1) {
        float d = x - l;
        for (i = 0; i < 3; i++) c[i] = (l < 1 && d > 0) ? l + (c[i] - l) * (1 - l) / d : l;
    }
}

static void set_lum(float *c, float l)
{
    float d = l - lumf(c);
    c[0] += d; c[1] += d; c[2] += d;
    clip_color(c);
}

static float satf(const float *c)
{
    return PC_MAX(c[0], PC_MAX(c[1], c[2])) - PC_MIN(c[0], PC_MIN(c[1], c[2]));
}

static void set_sat(float *c, float s)
{
    int imin = 0, imid = 1, imax = 2, t;
    if (c[imin] > c[imid]) { t = imin; imin = imid; imid = t; }
    if (c[imid] > c[imax]) { t = imid; imid = imax; imax = t; }
    if (c[imin] > c[imid]) { t = imin; imin = imid; imid = t; }
    if (c[imax] > c[imin]) {
        c[imid] = (c[imid] - c[imin]) * s / (c[imax] - c[imin]);
        c[imax] = s;
    } else {
        c[imid] = c[imax] = 0;
    }
    c[imin] = 0;
}

static void blend_nonsep(int mode, const u8 *b, const u8 *s, u8 *out)
{
    float cb[3], cs[3], r[3];
    int i;
    for (i = 0; i < 3; i++) { cb[i] = b[i] / 255.0f; cs[i] = s[i] / 255.0f; }
    switch (mode) {
    case PC_BLEND_HUE:
        memcpy(r, cs, sizeof r); set_sat(r, satf(cb)); set_lum(r, lumf(cb)); break;
    case PC_BLEND_SATURATION:
        memcpy(r, cb, sizeof r); set_sat(r, satf(cs)); set_lum(r, lumf(cb)); break;
    case PC_BLEND_COLOR:
        memcpy(r, cs, sizeof r); set_lum(r, lumf(cb)); break;
    default: /* LUMINOSITY */
        memcpy(r, cb, sizeof r); set_lum(r, lumf(cs)); break;
    }
    for (i = 0; i < 3; i++) { int v = (int)(r[i] * 255 + 0.5f); out[i] = (u8)PC_CLAMP(v, 0, 255); }
}

/* ---------------- pixel blending ---------------- */

/* dst: straight RGBA backdrop (modified). src: straight RGB colour, sa its
 * effective alpha 0..255 (layer alpha * opacity * mask). */
void pc_blend_pixel(int blend, u8 *dst, const u8 *src, int sa, unsigned noise)
{
    int ab = dst[3], ao, i;
    u8 mix[3];
    if (blend == PC_BLEND_DISSOLVE) {
        sa = ((int)(noise & 255) < sa) ? 255 : 0;
        blend = PC_BLEND_NORMAL;
    }
    if (sa <= 0) return;
    if (ab == 0 || blend == PC_BLEND_NORMAL) {
        mix[0] = src[0]; mix[1] = src[1]; mix[2] = src[2];
    } else {
        u8 bm[3];
        if (blend >= PC_BLEND_HUE) {
            blend_nonsep(blend, dst, src, bm);
        } else {
            const u8 *t = blend_lut(blend);
            if (t) for (i = 0; i < 3; i++) bm[i] = t[dst[i] * 256 + src[i]];
            else for (i = 0; i < 3; i++) bm[i] = src[i];
        }
        /* Where the backdrop is partly transparent the source shows through unblended. */
        if (ab == 255) { mix[0] = bm[0]; mix[1] = bm[1]; mix[2] = bm[2]; }
        else for (i = 0; i < 3; i++) mix[i] = (u8)((src[i] * (255 - ab) + bm[i] * ab + 127) / 255);
    }
    if (sa == 255) {
        dst[0] = mix[0]; dst[1] = mix[1]; dst[2] = mix[2]; dst[3] = 255;
        return;
    }
    if (ab == 255) {
        for (i = 0; i < 3; i++) dst[i] = (u8)((mix[i] * sa + dst[i] * (255 - sa) + 127) / 255);
        return;
    }
    /* General straight-alpha source-over. */
    {
        int wb = ab * (255 - sa);          /* backdrop weight, scaled by 255 */
        ao = sa * 255 + wb;                /* alpha * 255 */
        for (i = 0; i < 3; i++) dst[i] = (u8)((mix[i] * sa * 255 + dst[i] * wb + ao / 2) / ao);
        dst[3] = (u8)((ao + 127) / 255);
    }
}

/* ---------------- layer effects (applied at composite time) ---------------- */

/* Box-blur a single-channel buffer in place (3 passes ~ gaussian). */
static void blur_mask(u8 *m, int w, int h, int radius)
{
    u8 *tmp;
    int pass, x, y;
    if (radius < 1) return;
    tmp = (u8 *)malloc((size_t)PC_MAX(w, h));
    if (!tmp) return;
    for (pass = 0; pass < 3; pass++) {
        for (y = 0; y < h; y++) {
            u8 *row = m + (size_t)y * w;
            int sum = 0, win = radius * 2 + 1;
            for (x = -radius; x <= radius; x++) sum += row[PC_CLAMP(x, 0, w - 1)];
            for (x = 0; x < w; x++) {
                tmp[x] = (u8)(sum / win);
                sum += row[PC_MIN(x + radius + 1, w - 1)] - row[PC_MAX(x - radius, 0)];
            }
            memcpy(row, tmp, (size_t)w);
        }
        for (x = 0; x < w; x++) {
            int sum = 0, win = radius * 2 + 1;
            for (y = -radius; y <= radius; y++) sum += m[(size_t)PC_CLAMP(y, 0, h - 1) * w + x];
            for (y = 0; y < h; y++) {
                tmp[y] = (u8)(sum / win);
                sum += m[(size_t)PC_MIN(y + radius + 1, h - 1) * w + x] - m[(size_t)PC_MAX(y - radius, 0) * w + x];
            }
            for (y = 0; y < h; y++) m[(size_t)y * w + x] = tmp[y];
        }
    }
    free(tmp);
}

/* Effective layer alpha (pixel alpha * mask) as a separate buffer. */
static u8 *layer_alpha(const PcLayer *l, int w, int h)
{
    size_t i, n = (size_t)w * h;
    u8 *a = (u8 *)malloc(n);
    if (!a) return NULL;
    for (i = 0; i < n; i++) {
        int v = l->pix->px[i * 4 + 3];
        if (l->mask && l->mask_enabled) v = pc_mul255(v, l->mask->px[i]);
        a[i] = (u8)v;
    }
    return a;
}

static void shift_mask(const u8 *src, u8 *dst, int w, int h, int dx, int dy)
{
    int x, y;
    for (y = 0; y < h; y++) {
        int sy = y - dy;
        for (x = 0; x < w; x++) {
            int sx = x - dx;
            dst[(size_t)y * w + x] = (sx >= 0 && sx < w && sy >= 0 && sy < h) ? src[(size_t)sy * w + sx] : 0;
        }
    }
}

static void composite_color_mask(PcBuf *out, const u8 *m, u32 color, int opacity, int blend, const PcRect *r)
{
    int x, y, w = out->w;
    u8 c[3];
    c[0] = (u8)pc_r(color); c[1] = (u8)pc_g(color); c[2] = (u8)pc_b(color);
    for (y = r->y0; y < r->y1; y++)
        for (x = r->x0; x < r->x1; x++) {
            size_t i = (size_t)y * w + x;
            int a = pc_mul255(m[i], opacity);
            if (a) pc_blend_pixel(blend, out->px + i * 4, c, a, 0);
        }
}

/* Drop shadow and outer glow go below the layer. */
static void fx_below(const PcLayer *l, PcBuf *out, const PcRect *r)
{
    int w = out->w, h = out->h;
    u8 *a, *t;
    if (!(l->fx_shadow || l->fx_glow)) return;
    a = layer_alpha(l, w, h);
    t = (u8 *)malloc((size_t)w * h);
    if (!a || !t) { free(a); free(t); return; }
    if (l->fx_glow) {
        int k, sz = PC_CLAMP(l->fx_glow_size, 1, 64);
        memcpy(t, a, (size_t)w * h);
        blur_mask(t, w, h, (sz + 2) / 3);
        for (k = 0; k < w * h; k++) t[k] = (u8)PC_MIN(255, t[k] * 2);
        composite_color_mask(out, t, l->fx_glow_color, l->fx_glow_opacity, PC_BLEND_SCREEN, r);
    }
    if (l->fx_shadow) {
        double ang = l->fx_shadow_angle * M_PI / 180.0;
        int dx = (int)floor(-cos(ang) * l->fx_shadow_dist + 0.5);
        int dy = (int)floor(sin(ang) * l->fx_shadow_dist + 0.5);
        shift_mask(a, t, w, h, dx, dy);
        blur_mask(t, w, h, (PC_CLAMP(l->fx_shadow_blur, 0, 64) + 2) / 3);
        composite_color_mask(out, t, l->fx_shadow_color, l->fx_shadow_opacity, PC_BLEND_MULTIPLY, r);
    }
    free(a); free(t);
}

/* Bevel and inner shadow modify the layer colour inside its alpha. Returns a
 * temporary RGBA copy (caller frees) or NULL when there is nothing to do. */
static u8 *fx_inside(const PcLayer *l, int w, int h)
{
    size_t i, n = (size_t)w * h;
    u8 *a, *rgba, *t;
    if (!(l->fx_bevel || l->fx_inner_shadow)) return NULL;
    a = layer_alpha(l, w, h);
    rgba = (u8 *)malloc(n * 4);
    t = (u8 *)malloc(n);
    if (!a || !rgba || !t) { free(a); free(rgba); free(t); return NULL; }
    memcpy(rgba, l->pix->px, n * 4);
    if (l->fx_bevel) {
        int x, y, sz = PC_CLAMP(l->fx_bevel_size, 1, 32);
        double ang = l->fx_bevel_angle * M_PI / 180.0;
        double lx = cos(ang), ly = -sin(ang);
        memcpy(t, a, n);
        blur_mask(t, w, h, (sz + 1) / 2);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                size_t k = (size_t)y * w + x;
                int gx, gy, s, j;
                double d;
                if (!a[k]) continue;
                gx = t[(size_t)y * w + PC_MIN(x + 1, w - 1)] - t[(size_t)y * w + PC_MAX(x - 1, 0)];
                gy = t[(size_t)PC_MIN(y + 1, h - 1) * w + x] - t[(size_t)PC_MAX(y - 1, 0) * w + x];
                d = -(gx * lx + gy * ly) * l->fx_bevel_depth / 4.0;
                s = (int)d;
                for (j = 0; j < 3; j++) {
                    int v = rgba[k * 4 + j] + s;
                    rgba[k * 4 + j] = (u8)PC_CLAMP(v, 0, 255);
                }
            }
    }
    if (l->fx_inner_shadow) {
        int dx = l->fx_inner_dist / 2 + 1, dy = l->fx_inner_dist / 2 + 1;
        u8 *inv = (u8 *)malloc(n);
        if (inv) {
            for (i = 0; i < n; i++) inv[i] = (u8)(255 - a[i]);
            shift_mask(inv, t, w, h, dx, dy);
            blur_mask(t, w, h, (PC_CLAMP(l->fx_inner_blur, 0, 64) + 2) / 3);
            for (i = 0; i < n; i++) {
                int s = pc_mul255(pc_mul255(t[i], a[i]), l->fx_inner_opacity), j;
                for (j = 0; j < 3; j++) rgba[i * 4 + j] = (u8)(rgba[i * 4 + j] * (255 - s) / 255);
            }
            free(inv);
        }
    }
    free(a); free(t);
    return rgba;
}

/* ---------------- compositing ---------------- */

static unsigned hash_noise(unsigned x, unsigned y)
{
    unsigned h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void pc_doc_composite_into(const PcDoc *d, PcBuf *out, const PcRect *rr)
{
    PcRect r;
    int li, x, y, w = d->s.w;
    if (rr) r = *rr; else { r.x0 = 0; r.y0 = 0; r.x1 = d->s.w; r.y1 = d->s.h; }
    r.x0 = PC_MAX(r.x0, 0); r.y0 = PC_MAX(r.y0, 0);
    r.x1 = PC_MIN(r.x1, d->s.w); r.y1 = PC_MIN(r.y1, d->s.h);
    if (r.x0 >= r.x1 || r.y0 >= r.y1) return;
    for (y = r.y0; y < r.y1; y++) memset(out->px + ((size_t)y * w + r.x0) * 4, 0, (size_t)(r.x1 - r.x0) * 4);

    for (li = 0; li < d->s.nlayers; li++) {
        const PcLayer *l = d->s.layers[li];
        const PcLayer *base = NULL;
        const u8 *px;
        u8 *fxpx = NULL;
        if (!l->visible || l->opacity == 0) continue;
        /* Clipping group: alpha limited by the base layer below. */
        if (l->clip) {
            int bi = li - 1;
            while (bi >= 0 && d->s.layers[bi]->clip) bi--;
            if (bi >= 0) {
                base = d->s.layers[bi];
                if (!base->visible) continue;
            }
        }
        if (l->fx_enabled) {
            fx_below(l, out, &r);
            fxpx = fx_inside(l, d->s.w, d->s.h);
        }
        px = fxpx ? fxpx : l->pix->px;
        for (y = r.y0; y < r.y1; y++) {
            size_t i = (size_t)y * w + r.x0;
            const u8 *s = px + i * 4;
            const u8 *m = (l->mask && l->mask_enabled) ? l->mask->px + i : NULL;
            u8 *o = out->px + i * 4;
            for (x = r.x0; x < r.x1; x++, s += 4, o += 4, i++) {
                int a = s[3];
                if (!a) { if (m) m++; continue; }
                if (m) { a = pc_mul255(a, *m); m++; }
                if (base) {
                    int ba = base->pix->px[i * 4 + 3];
                    if (base->mask && base->mask_enabled) ba = pc_mul255(ba, base->mask->px[i]);
                    a = pc_mul255(a, ba);
                }
                if (l->opacity != 255) a = pc_mul255(a, l->opacity);
                if (a) pc_blend_pixel(l->blend, o, s, a, l->blend == PC_BLEND_DISSOLVE ? hash_noise((unsigned)x, (unsigned)y) : 0);
            }
        }
        free(fxpx);
    }
}

/* Convert straight RGBA to display BGRX over a checkerboard (mode 0), white
 * (mode 1) or a flat colour bg (mode 2).  `count` pixels; x0 is used for the
 * checker phase via the low bits stored by the caller in bg when mode 0:
 * bg = (y << 16) | x of the first pixel in document coordinates. */
void pc_composite_rgba_over(const u8 *src, int count, u8 *dst, int mode, u32 bg)
{
    int i;
    int cx = (int)(bg & 0xFFFF), cy = (int)(bg >> 16);
    for (i = 0; i < count; i++, src += 4, dst += 4) {
        int a = src[3], br, bgc, bb;
        if (mode == 0) {
            int on = (((cx + i) >> 3) ^ (cy >> 3)) & 1;
            br = bgc = bb = on ? 204 : 255;
        } else if (mode == 1) {
            br = bgc = bb = 255;
        } else {
            br = pc_r(bg); bgc = pc_g(bg); bb = pc_b(bg);
        }
        if (a == 255) { dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; }
        else if (a == 0) { dst[0] = (u8)bb; dst[1] = (u8)bgc; dst[2] = (u8)br; }
        else {
            dst[0] = (u8)((src[2] * a + bb * (255 - a) + 127) / 255);
            dst[1] = (u8)((src[1] * a + bgc * (255 - a) + 127) / 255);
            dst[2] = (u8)((src[0] * a + br * (255 - a) + 127) / 255);
        }
        dst[3] = 0;
    }
}
