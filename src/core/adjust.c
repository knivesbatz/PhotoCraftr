/*
 * Image > Adjust: Levels, Curves, Brightness/Contrast, Color Balance,
 * Hue/Saturation, Invert, Equalize, Threshold, Posterize, Desaturate,
 * Channel Mixer, Auto Levels, histogram.
 *
 * Color Balance is ported from PhotoCraft crates/compose/src/adjust/color.rs
 * (BalanceCurve, MIT OR Apache-2.0), which was reverse-engineered against
 * Photoshop: the three tone ranges fold into one Levels per channel.
 */
#include "pcops.h"

/* Apply an RGB->RGB function to the active layer through the selection. */
typedef void (*PixFn)(u8 *rgb, void *ctx);

static int adjust_pixels(PcDoc *d, PixFn fn, void *ctx)
{
    PcLayer *l = pc_doc_active(d);
    size_t i, n = (size_t)d->s.w * d->s.h;
    u8 *res;
    int rc;
    if (!l || l->lock_pixels) return -1;
    res = (u8 *)pc_alloc_checked(n, 4, 1, 0);
    if (!res) return -1;
    memcpy(res, l->pix->px, n * 4);
    for (i = 0; i < n; i++) if (res[i * 4 + 3]) fn(res + i * 4, ctx);
    rc = pc_apply_result(d, l, res);
    free(res);
    return rc;
}

static void lut_fn(u8 *p, void *ctx)
{
    u8 (*lut)[256] = (u8 (*)[256])ctx;
    p[0] = lut[0][p[0]]; p[1] = lut[1][p[1]]; p[2] = lut[2][p[2]];
}

int pc_adj_luts(PcDoc *d, u8 lut[3][256]) { return adjust_pixels(d, lut_fn, lut); }

/* ---------------- Levels ---------------- */

void pc_levels_default(PcLevels *lv)
{
    int c;
    for (c = 0; c < 4; c++) {
        lv->in_black[c] = 0; lv->in_white[c] = 255; lv->gamma[c] = 1.0f;
        lv->out_black[c] = 0; lv->out_white[c] = 255;
    }
}

static int levels_one(int v, int ib, int iw, float g, int ob, int ow)
{
    float t;
    if (iw <= ib) iw = ib + 1;
    t = (float)(v - ib) / (iw - ib);
    t = PC_CLAMP(t, 0, 1);
    if (g > 0 && g != 1.0f) t = (float)pow(t, 1.0 / g);
    {
        int r = (int)(ob + t * (ow - ob) + 0.5f);
        return PC_CLAMP(r, 0, 255);
    }
}

void pc_levels_lut(const PcLevels *lv, u8 lut[3][256])
{
    int c, v;
    for (c = 0; c < 3; c++)
        for (v = 0; v < 256; v++) {
            int a = levels_one(v, lv->in_black[c + 1], lv->in_white[c + 1], lv->gamma[c + 1], lv->out_black[c + 1], lv->out_white[c + 1]);
            lut[c][v] = (u8)levels_one(a, lv->in_black[0], lv->in_white[0], lv->gamma[0], lv->out_black[0], lv->out_white[0]);
        }
}

int pc_adj_levels(PcDoc *d, const PcLevels *lv)
{
    u8 lut[3][256];
    pc_levels_lut(lv, lut);
    return pc_adj_luts(d, lut);
}

void pc_histogram(const PcDoc *d, int channel, int use_composite, unsigned hist[256])
{
    PcBuf *b = NULL;
    const u8 *p;
    size_t i, n = (size_t)d->s.w * d->s.h;
    memset(hist, 0, sizeof(unsigned) * 256);
    if (use_composite) b = pc_doc_composite(d);
    else if (d->s.active >= 0 && d->s.active < d->s.nlayers) b = pc_buf_ref(d->s.layers[d->s.active]->pix);
    if (!b) return;
    p = b->px;
    for (i = 0; i < n; i++, p += 4) {
        if (!p[3]) continue;
        if (d->s.sel && !d->s.sel->px[i]) continue;
        hist[channel == 0 ? pc_luma(p[0], p[1], p[2]) : p[PC_CLAMP(channel - 1, 0, 2)]]++;
    }
    pc_buf_unref(b);
}

int pc_adj_auto_levels(PcDoc *d)
{
    PcLevels lv;
    unsigned h[256];
    int c;
    pc_levels_default(&lv);
    for (c = 1; c <= 3; c++) {
        unsigned total = 0, acc = 0, clip;
        int lo = 0, hi = 255, v;
        pc_histogram(d, c, 0, h);
        for (v = 0; v < 256; v++) total += h[v];
        if (!total) return -1;
        clip = total / 200;   /* 0.5% like Photoshop's default clipping */
        for (v = 0; v < 256; v++) { acc += h[v]; if (acc > clip) { lo = v; break; } }
        acc = 0;
        for (v = 255; v >= 0; v--) { acc += h[v]; if (acc > clip) { hi = v; break; } }
        if (hi <= lo) { lo = 0; hi = 255; }
        lv.in_black[c] = lo; lv.in_white[c] = hi;
    }
    return pc_adj_levels(d, &lv);
}

/* ---------------- Curves ---------------- */

void pc_curves_default(PcCurves *cv)
{
    int c;
    for (c = 0; c < 4; c++) {
        cv->c[c].n = 2;
        cv->c[c].x[0] = 0; cv->c[c].y[0] = 0;
        cv->c[c].x[1] = 255; cv->c[c].y[1] = 255;
    }
}

void pc_curve_lut(const PcCurve *c, u8 lut[256])
{
    double y2[16], u[16];
    int n = PC_CLAMP(c->n, 0, 16), i, k, v;
    if (n < 2) { for (v = 0; v < 256; v++) lut[v] = (u8)v; return; }
    /* Natural cubic spline (second derivatives), as in Numerical Recipes. */
    y2[0] = u[0] = 0;
    for (i = 1; i < n - 1; i++) {
        double h0 = c->x[i] - c->x[i - 1], h1 = c->x[i + 1] - c->x[i];
        double sig, p;
        if (h0 <= 0 || h1 <= 0) { y2[i] = u[i] = 0; continue; }
        sig = h0 / (h0 + h1);
        p = sig * y2[i - 1] + 2;
        y2[i] = (sig - 1) / p;
        u[i] = (c->y[i + 1] - c->y[i]) / h1 - (c->y[i] - c->y[i - 1]) / h0;
        u[i] = (6 * u[i] / (h0 + h1) - sig * u[i - 1]) / p;
    }
    y2[n - 1] = 0;
    for (k = n - 2; k >= 0; k--) y2[k] = y2[k] * y2[k + 1] + u[k];
    for (v = 0; v < 256; v++) {
        double out;
        if (v <= c->x[0]) out = c->y[0];
        else if (v >= c->x[n - 1]) out = c->y[n - 1];
        else {
            int lo = 0;
            double h, a, b;
            while (lo < n - 2 && v > c->x[lo + 1]) lo++;
            h = c->x[lo + 1] - c->x[lo];
            if (h <= 0) out = c->y[lo];
            else {
                a = (c->x[lo + 1] - v) / h; b = (v - c->x[lo]) / h;
                out = a * c->y[lo] + b * c->y[lo + 1] + ((a * a * a - a) * y2[lo] + (b * b * b - b) * y2[lo + 1]) * h * h / 6;
            }
        }
        {
            int iv = (int)floor(out + 0.5);
            lut[v] = (u8)PC_CLAMP(iv, 0, 255);
        }
    }
}

int pc_adj_curves(PcDoc *d, const PcCurves *cv)
{
    u8 master[256], ch[256], lut[3][256];
    int c, v;
    pc_curve_lut(&cv->c[0], master);
    for (c = 0; c < 3; c++) {
        pc_curve_lut(&cv->c[c + 1], ch);
        for (v = 0; v < 256; v++) lut[c][v] = master[ch[v]];
    }
    return pc_adj_luts(d, lut);
}

/* ---------------- Brightness/Contrast (PS 5.0 linear behaviour) ---------------- */

int pc_adj_brightness_contrast(PcDoc *d, int brightness, int contrast)
{
    u8 lut[3][256];
    double f;
    int v, c;
    brightness = PC_CLAMP(brightness, -100, 100);
    contrast = PC_CLAMP(contrast, -100, 100);
    f = contrast >= 0 ? (contrast >= 100 ? 255.0 : 1.0 / (1.0 - contrast / 100.0)) : 1.0 + contrast / 100.0;
    for (v = 0; v < 256; v++) {
        double o = (v + brightness * 1.275 - 127.5) * f + 127.5;
        int iv = (int)floor(o + 0.5);
        for (c = 0; c < 3; c++) lut[c][v] = (u8)PC_CLAMP(iv, 0, 255);
    }
    return pc_adj_luts(d, lut);
}

/* ---------------- Color Balance (PhotoCraft BalanceCurve port) ---------------- */

static double balance_eval(double b, double w, double e, int knee, double u0, double s0, double p, double dd, double v)
{
    double u;
    v = PC_CLAMP(v, 0, 1);
    if (v >= w) return 1;
    if (v <= b) return 0;
    u = (v - b) / (w - b);
    if (knee && u < u0) {
        double t = u / u0, t2 = t * t, t3 = t2 * t;
        double r = (t3 - 2 * t2 + t) * u0 * s0 + (3 * t2 - 2 * t3) * p + (t3 - t2) * u0 * dd;
        return PC_CLAMP(r, 0, 1);
    }
    return pow(u, e);
}

int pc_adj_color_balance(PcDoc *d, const PcColorBalance *cb)
{
    u8 lut[3][256];
    int ch, v, t;
    double tones[3][3];
    for (t = 0; t < 3; t++) for (ch = 0; ch < 3; ch++) tones[t][ch] = PC_CLAMP(cb->v[t][ch], -100, 100);
    for (ch = 0; ch < 3; ch++) {
        double black, white, m, gamma, e, u0 = 0, s0 = 0, p = 0, dd = 0, b, w;
        int knee = 0;
        if (cb->preserve_luminosity) {
            double smax = PC_MAX(tones[0][0], PC_MAX(tones[0][1], tones[0][2]));
            double hmin = PC_MIN(tones[2][0], PC_MIN(tones[2][1], tones[2][2]));
            double mmax = PC_MAX(tones[1][0], PC_MAX(tones[1][1], tones[1][2]));
            double mmin = PC_MIN(tones[1][0], PC_MIN(tones[1][1], tones[1][2]));
            black = smax - tones[0][ch];
            white = tones[2][ch] - hmin;
            m = tones[1][ch] - (mmax + mmin) / 2;
        } else {
            black = PC_MAX(-tones[0][ch], 0);
            white = PC_MAX(tones[2][ch], 0);
            m = tones[1][ch] + (tones[0][ch] + tones[2][ch]) / 2;
        }
        gamma = pow(2.0, m / 100.0);
        e = 1.0 / PC_MAX(gamma, 0.01);
        if (gamma > 1.0) {
            s0 = pow(2.0, gamma);
            u0 = PC_MIN(2.0 * pow(s0, 1.0 / (e - 1.0)), 1.0);
            if (u0 > 0) { knee = 1; p = pow(u0, e); dd = e * pow(u0, e - 1.0); }
        }
        b = black / 255.0; w = 1.0 - white / 255.0;
        for (v = 0; v < 256; v++) {
            double o = balance_eval(b, w, e, knee, u0, s0, p, dd, v / 255.0);
            lut[ch][v] = (u8)PC_CLAMP((int)(o * 255 + 0.5), 0, 255);
        }
    }
    return pc_adj_luts(d, lut);
}

/* ---------------- Hue/Saturation ---------------- */

/* Weight of hue h (degrees) in range k (1..6 centred at 0,60,...,300), with
 * Photoshop's default 30-degree core and 30-degree falloff either side. */
static float range_weight(float h, int k)
{
    float c = (k - 1) * 60.0f, dd = (float)fabs(fmod(h - c + 540.0, 360.0) - 180.0);
    if (dd <= 15) return 1;
    if (dd >= 45) return 0;
    return (45 - dd) / 30;
}

static void huesat_fn(u8 *p, void *ctx)
{
    const PcHueSat *hs = (const PcHueSat *)ctx;
    float h, s, l, dh = 0, ds = 0, dl = 0;
    int r, g, b, k;
    pc_rgb_to_hsl(p[0], p[1], p[2], &h, &s, &l);
    if (hs->colorize) {
        float lum = l;
        float light = hs->light[0] / 100.0f;
        if (light > 0) lum = lum * (1 - light) + light;
        else lum = lum * (1 + light);
        pc_hsl_to_rgb((float)hs->hue[0], hs->sat[0] / 100.0f, lum, &r, &g, &b);
    } else {
        dh = (float)hs->hue[0]; ds = hs->sat[0] / 100.0f; dl = hs->light[0] / 100.0f;
        if (s > 0) {
            for (k = 1; k <= 6; k++) {
                float wgt = range_weight(h, k);
                if (wgt > 0) { dh += hs->hue[k] * wgt; ds += hs->sat[k] / 100.0f * wgt; dl += hs->light[k] / 100.0f * wgt; }
            }
        }
        h += dh;
        if (ds > 0) s = s + (1 - s) * ds * (s > 0 ? 1 : 0);
        else s = s * (1 + ds);
        pc_hsl_to_rgb(h, s, l, &r, &g, &b);
        if (dl > 0) { r = (int)(r + (255 - r) * dl); g = (int)(g + (255 - g) * dl); b = (int)(b + (255 - b) * dl); }
        else if (dl < 0) { r = (int)(r * (1 + dl)); g = (int)(g * (1 + dl)); b = (int)(b * (1 + dl)); }
    }
    p[0] = (u8)PC_CLAMP(r, 0, 255); p[1] = (u8)PC_CLAMP(g, 0, 255); p[2] = (u8)PC_CLAMP(b, 0, 255);
}

int pc_adj_hue_sat(PcDoc *d, const PcHueSat *hs) { return adjust_pixels(d, huesat_fn, (void *)hs); }

/* ---------------- simple adjustments ---------------- */

int pc_adj_invert(PcDoc *d)
{
    u8 lut[3][256];
    int v;
    for (v = 0; v < 256; v++) lut[0][v] = lut[1][v] = lut[2][v] = (u8)(255 - v);
    return pc_adj_luts(d, lut);
}

static void desat_fn(u8 *p, void *ctx)
{
    /* Photoshop's Desaturate: lightness = (max + min) / 2. */
    int mx = PC_MAX(p[0], PC_MAX(p[1], p[2])), mn = PC_MIN(p[0], PC_MIN(p[1], p[2]));
    p[0] = p[1] = p[2] = (u8)((mx + mn + 1) / 2);
    (void)ctx;
}

int pc_adj_desaturate(PcDoc *d) { return adjust_pixels(d, desat_fn, NULL); }

int pc_adj_equalize(PcDoc *d)
{
    unsigned h[256];
    u8 lut[3][256];
    unsigned total = 0, acc = 0, first = 0;
    int v;
    pc_histogram(d, 0, 0, h);
    for (v = 0; v < 256; v++) total += h[v];
    if (!total) return -1;
    for (v = 0; v < 256; v++) if (h[v]) { first = h[v]; break; }
    for (v = 0; v < 256; v++) {
        int o;
        acc += h[v];
        o = total > first ? (int)((double)(acc - first) * 255.0 / (total - first) + 0.5) : v;
        lut[0][v] = lut[1][v] = lut[2][v] = (u8)PC_CLAMP(o, 0, 255);
    }
    return pc_adj_luts(d, lut);
}

static void threshold_fn(u8 *p, void *ctx)
{
    int t = *(int *)ctx;
    u8 v = pc_luma(p[0], p[1], p[2]) >= t ? 255 : 0;
    p[0] = p[1] = p[2] = v;
}

int pc_adj_threshold(PcDoc *d, int level) { level = PC_CLAMP(level, 1, 255); return adjust_pixels(d, threshold_fn, &level); }

int pc_adj_posterize(PcDoc *d, int levels)
{
    u8 lut[3][256];
    int v;
    levels = PC_CLAMP(levels, 2, 255);
    for (v = 0; v < 256; v++) {
        int q = (v * levels) / 256;
        int o = q * 255 / (levels - 1);
        lut[0][v] = lut[1][v] = lut[2][v] = (u8)PC_CLAMP(o, 0, 255);
    }
    return pc_adj_luts(d, lut);
}

typedef struct { int m[3][4]; int mono; } MixCtx;

static void mixer_fn(u8 *p, void *ctx)
{
    const MixCtx *c = (const MixCtx *)ctx;
    int o[3], k;
    for (k = 0; k < 3; k++) {
        int v = (p[0] * c->m[k][0] + p[1] * c->m[k][1] + p[2] * c->m[k][2]) / 100 + c->m[k][3] * 255 / 100;
        o[k] = PC_CLAMP(v, 0, 255);
    }
    if (c->mono) o[1] = o[2] = o[0];
    p[0] = (u8)o[0]; p[1] = (u8)o[1]; p[2] = (u8)o[2];
}

int pc_adj_channel_mixer(PcDoc *d, const int m[3][4], int monochrome)
{
    MixCtx c;
    memcpy(c.m, m, sizeof c.m);
    c.mono = monochrome;
    return adjust_pixels(d, mixer_fn, &c);
}
