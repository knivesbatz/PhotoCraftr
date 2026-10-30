/*
 * Filter menu: Blur, Sharpen, Noise, Stylize, Pixelate, Distort, Render,
 * Other.  Each filter computes a full result from the source pixels; the
 * selection is applied afterwards by pc_apply_result.
 *
 * Blurs work on premultiplied colour so transparent areas don't bleed
 * dark fringes (same approach as PhotoCraft crates/algo/src/blur.rs).
 */
#include "pcops.h"

static const char *const filter_names[PC_FILTER_COUNT] = {
    "Blur", "Blur More", "Gaussian Blur", "Motion Blur", "Radial Blur",
    "Sharpen", "Sharpen More", "Sharpen Edges", "Unsharp Mask",
    "Add Noise", "Despeckle", "Dust & Scratches", "Median",
    "Emboss", "Find Edges", "Solarize", "Diffuse", "Wind", "Trace Contour",
    "Mosaic", "Twirl", "Pinch", "Spherize", "Ripple", "Clouds", "Difference Clouds",
    "Offset", "High Pass", "Maximum", "Minimum", "Custom"
};

const char *pc_filter_name(int f) { return (f >= 0 && f < PC_FILTER_COUNT) ? filter_names[f] : "Filter"; }

void pc_filter_defaults(int f, PcFilterParams *p)
{
    memset(p, 0, sizeof *p);
    p->radius = 1; p->amount = 50; p->threshold = 0; p->angle = 0; p->distance = 10;
    p->cell = 10; p->seed = 12345; p->fg = 0x000000; p->bg = 0xFFFFFF; p->scale = 1;
    switch (f) {
    case PC_FILTER_GAUSSIAN_BLUR: p->radius = 2.0f; break;
    case PC_FILTER_UNSHARP_MASK: p->amount = 50; p->radius = 1.0f; p->threshold = 0; break;
    case PC_FILTER_ADD_NOISE: p->amount = 32; p->mode = 0; break;
    case PC_FILTER_MEDIAN: p->radius = 1; break;
    case PC_FILTER_DUST_SCRATCHES: p->radius = 1; p->threshold = 0; break;
    case PC_FILTER_EMBOSS: p->angle = 135; p->distance = 3; p->amount = 100; break;
    case PC_FILTER_MOTION_BLUR: p->angle = 0; p->distance = 10; break;
    case PC_FILTER_RADIAL_BLUR: p->amount = 10; p->mode = 0; p->dx = 50; p->dy = 50; break;
    case PC_FILTER_TWIRL: p->amount = 50; break;
    case PC_FILTER_PINCH: p->amount = 50; break;
    case PC_FILTER_SPHERIZE: p->amount = 100; break;
    case PC_FILTER_RIPPLE: p->amount = 100; p->mode = 1; break;
    case PC_FILTER_HIGH_PASS: p->radius = 10; break;
    case PC_FILTER_TRACE_CONTOUR: p->threshold = 128; break;
    case PC_FILTER_OFFSET: p->dx = 0; p->dy = 0; p->wrap = 2; break;
    case PC_FILTER_CUSTOM: p->kernel[12] = 1; p->scale = 1; break;
    default: break;
    }
}

/* ---------------- helpers ---------------- */

static void premultiply(const u8 *s, float *f, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++, s += 4, f += 4) {
        float a = s[3] / 255.0f;
        f[0] = s[0] * a; f[1] = s[1] * a; f[2] = s[2] * a; f[3] = s[3];
    }
}

static void unpremultiply(const float *f, u8 *d, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++, f += 4, d += 4) {
        float a = f[3];
        int k;
        if (a <= 0.5f) { d[0] = d[1] = d[2] = d[3] = 0; continue; }
        for (k = 0; k < 3; k++) { int v = (int)(f[k] * 255.0f / a + 0.5f); d[k] = (u8)PC_CLAMP(v, 0, 255); }
        { int v = (int)(a + 0.5f); d[3] = (u8)PC_CLAMP(v, 0, 255); }
    }
}

/* Separable convolution of a float RGBA image with a symmetric 1-D kernel. */
static void conv_sep(float *img, int w, int h, const float *k, int r)
{
    float *line = (float *)malloc(sizeof(float) * 4 * (size_t)(PC_MAX(w, h) + 2 * r));
    int x, y, i, c;
    if (!line) return;
    for (y = 0; y < h; y++) {
        float *row = img + (size_t)y * w * 4;
        for (x = -r; x < w + r; x++) memcpy(line + (x + r) * 4, row + PC_CLAMP(x, 0, w - 1) * 4, sizeof(float) * 4);
        for (x = 0; x < w; x++) {
            float acc[4] = { 0, 0, 0, 0 };
            for (i = -r; i <= r; i++) { const float *s = line + (x + r + i) * 4; float kv = k[i < 0 ? -i : i]; for (c = 0; c < 4; c++) acc[c] += s[c] * kv; }
            memcpy(row + x * 4, acc, sizeof acc);
        }
    }
    for (x = 0; x < w; x++) {
        for (y = -r; y < h + r; y++) memcpy(line + (y + r) * 4, img + ((size_t)PC_CLAMP(y, 0, h - 1) * w + x) * 4, sizeof(float) * 4);
        for (y = 0; y < h; y++) {
            float acc[4] = { 0, 0, 0, 0 };
            for (i = -r; i <= r; i++) { const float *s = line + (y + r + i) * 4; float kv = k[i < 0 ? -i : i]; for (c = 0; c < 4; c++) acc[c] += s[c] * kv; }
            memcpy(img + ((size_t)y * w + x) * 4, acc, sizeof acc);
        }
    }
    free(line);
}

static int gauss_kernel(float sigma, float *k, int maxr)
{
    int r = (int)ceil(sigma * 3), i;
    float sum = 0;
    if (r > maxr) r = maxr;
    if (r < 1) r = 1;
    for (i = 0; i <= r; i++) { k[i] = (float)exp(-(double)i * i / (2.0 * sigma * sigma)); sum += i ? 2 * k[i] : k[i]; }
    for (i = 0; i <= r; i++) k[i] /= sum;
    return r;
}

void pc_gaussian_rgba(const u8 *src, u8 *dst, int w, int h, float radius)
{
    size_t n = (size_t)w * h;
    float *f;
    float k[257];
    int r;
    if (radius <= 0.05f) { memcpy(dst, src, n * 4); return; }
    f = (float *)pc_alloc_checked(n, 4, sizeof(float), 0);
    if (!f) { memcpy(dst, src, n * 4); return; }
    premultiply(src, f, n);
    /* Photoshop's radius behaves like a standard deviation close to radius. */
    r = gauss_kernel(radius, k, 256);
    conv_sep(f, w, h, k, r);
    unpremultiply(f, dst, n);
    free(f);
}

void pc_gaussian_gray(u8 *buf, int w, int h, float radius)
{
    float k[257], *line;
    int r, x, y, i;
    if (radius <= 0.05f) return;
    r = gauss_kernel(radius, k, 256);
    line = (float *)malloc(sizeof(float) * (size_t)(PC_MAX(w, h) + 2 * r));
    if (!line) return;
    for (y = 0; y < h; y++) {
        u8 *row = buf + (size_t)y * w;
        for (x = -r; x < w + r; x++) line[x + r] = row[PC_CLAMP(x, 0, w - 1)];
        for (x = 0; x < w; x++) {
            float acc = 0;
            for (i = -r; i <= r; i++) acc += line[x + r + i] * k[i < 0 ? -i : i];
            row[x] = (u8)PC_CLAMP((int)(acc + 0.5f), 0, 255);
        }
    }
    for (x = 0; x < w; x++) {
        for (y = -r; y < h + r; y++) line[y + r] = buf[(size_t)PC_CLAMP(y, 0, h - 1) * w + x];
        for (y = 0; y < h; y++) {
            float acc = 0;
            for (i = -r; i <= r; i++) acc += line[y + r + i] * k[i < 0 ? -i : i];
            buf[(size_t)y * w + x] = (u8)PC_CLAMP((int)(acc + 0.5f), 0, 255);
        }
    }
    free(line);
}

/* 3x3 convolution on colour, alpha preserved. */
static void conv3(const u8 *s, u8 *d, int w, int h, const int k[9], int div, int bias)
{
    int x, y, c, i, j;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            u8 *o = d + ((size_t)y * w + x) * 4;
            for (c = 0; c < 3; c++) {
                int acc = 0;
                for (j = -1; j <= 1; j++)
                    for (i = -1; i <= 1; i++)
                        acc += s[((size_t)PC_CLAMP(y + j, 0, h - 1) * w + PC_CLAMP(x + i, 0, w - 1)) * 4 + c] * k[(j + 1) * 3 + i + 1];
                acc = acc / div + bias;
                o[c] = (u8)PC_CLAMP(acc, 0, 255);
            }
            o[3] = s[((size_t)y * w + x) * 4 + 3];
        }
}

/* Median of each channel in a (2r+1)^2 window, using a sliding histogram. */
static void median(const u8 *s, u8 *d, int w, int h, int r)
{
    int x, y, c, i, j;
    unsigned hist[3][256];
    int win = (2 * r + 1) * (2 * r + 1), half = win / 2;
    for (y = 0; y < h; y++) {
        memset(hist, 0, sizeof hist);
        for (j = -r; j <= r; j++)
            for (i = -r; i <= r; i++) {
                const u8 *p = s + ((size_t)PC_CLAMP(y + j, 0, h - 1) * w + PC_CLAMP(i, 0, w - 1)) * 4;
                for (c = 0; c < 3; c++) hist[c][p[c]]++;
            }
        for (x = 0; x < w; x++) {
            u8 *o = d + ((size_t)y * w + x) * 4;
            for (c = 0; c < 3; c++) {
                int acc = 0, v = 0;
                while (v < 255 && (acc += (int)hist[c][v]) <= half) v++;
                o[c] = (u8)v;
            }
            o[3] = s[((size_t)y * w + x) * 4 + 3];
            if (x + 1 < w) {
                for (j = -r; j <= r; j++) {
                    int yy = PC_CLAMP(y + j, 0, h - 1);
                    const u8 *out = s + ((size_t)yy * w + PC_CLAMP(x - r, 0, w - 1)) * 4;
                    const u8 *in = s + ((size_t)yy * w + PC_CLAMP(x + r + 1, 0, w - 1)) * 4;
                    for (c = 0; c < 3; c++) { hist[c][out[c]]--; hist[c][in[c]]++; }
                }
            }
        }
    }
}

static void minmax(const u8 *s, u8 *d, int w, int h, int r, int take_max)
{
    int x, y, c, i;
    u8 *tmp = (u8 *)malloc((size_t)w * h * 4);
    if (!tmp) { memcpy(d, s, (size_t)w * h * 4); return; }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            for (c = 0; c < 4; c++) {
                int v = take_max ? 0 : 255;
                for (i = -r; i <= r; i++) {
                    int t = s[((size_t)y * w + PC_CLAMP(x + i, 0, w - 1)) * 4 + c];
                    v = take_max ? PC_MAX(v, t) : PC_MIN(v, t);
                }
                tmp[((size_t)y * w + x) * 4 + c] = (u8)v;
            }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            for (c = 0; c < 4; c++) {
                int v = take_max ? 0 : 255;
                for (i = -r; i <= r; i++) {
                    int t = tmp[((size_t)PC_CLAMP(y + i, 0, h - 1) * w + x) * 4 + c];
                    v = take_max ? PC_MAX(v, t) : PC_MIN(v, t);
                }
                d[((size_t)y * w + x) * 4 + c] = (u8)v;
            }
    for (y = 0; y < w * h; y++) d[y * 4 + 3] = s[y * 4 + 3];
    free(tmp);
}

static void sample_bilinear(const u8 *s, int w, int h, float fx, float fy, u8 *o)
{
    int x0, y0, c;
    float ax, ay;
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > w - 1) fx = (float)(w - 1);
    if (fy > h - 1) fy = (float)(h - 1);
    x0 = (int)fx; y0 = (int)fy;
    ax = fx - x0; ay = fy - y0;
    {
        int x1 = PC_MIN(x0 + 1, w - 1), y1 = PC_MIN(y0 + 1, h - 1);
        const u8 *p00 = s + ((size_t)y0 * w + x0) * 4, *p10 = s + ((size_t)y0 * w + x1) * 4;
        const u8 *p01 = s + ((size_t)y1 * w + x0) * 4, *p11 = s + ((size_t)y1 * w + x1) * 4;
        for (c = 0; c < 4; c++) {
            float v = (p00[c] * (1 - ax) + p10[c] * ax) * (1 - ay) + (p01[c] * (1 - ax) + p11[c] * ax) * ay;
            o[c] = (u8)(v + 0.5f);
        }
    }
}

/* Value-noise turbulence for Clouds. */
static float lattice(int x, int y, unsigned seed)
{
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

static float vnoise(float x, float y, unsigned seed)
{
    int xi = (int)floor(x), yi = (int)floor(y);
    float fx = x - xi, fy = y - yi;
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    float a = lattice(xi, yi, seed), b = lattice(xi + 1, yi, seed);
    float c = lattice(xi, yi + 1, seed), d = lattice(xi + 1, yi + 1, seed);
    return (a + (b - a) * sx) * (1 - sy) + (c + (d - c) * sx) * sy;
}

/* ---------------- filter dispatcher ---------------- */

int pc_filter_run(int f, const PcFilterParams *p, const u8 *s, u8 *d, int w, int h)
{
    size_t n = (size_t)w * h, i;
    int x, y, c;
    unsigned rng = p->seed ? p->seed : 1;
    memcpy(d, s, n * 4);
    switch (f) {
    case PC_FILTER_BLUR: { static const int k[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 }; conv3(s, d, w, h, k, 16, 0); break; }
    case PC_FILTER_BLUR_MORE: pc_gaussian_rgba(s, d, w, h, 1.5f); break;
    case PC_FILTER_GAUSSIAN_BLUR: pc_gaussian_rgba(s, d, w, h, PC_CLAMP(p->radius, 0.1f, 250.0f)); break;
    case PC_FILTER_MOTION_BLUR: {
        double a = p->angle * M_PI / 180.0;
        int dist = PC_CLAMP(p->distance, 1, 999), steps = dist, k;
        float dx = (float)cos(a), dy = (float)-sin(a);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                float acc[4] = { 0, 0, 0, 0 };
                for (k = 0; k <= steps; k++) {
                    float t = (float)k - steps / 2.0f;
                    u8 px[4];
                    sample_bilinear(s, w, h, x + dx * t, y + dy * t, px);
                    for (c = 0; c < 4; c++) acc[c] += px[c];
                }
                for (c = 0; c < 4; c++) d[((size_t)y * w + x) * 4 + c] = (u8)(acc[c] / (steps + 1) + 0.5f);
            }
        break;
    }
    case PC_FILTER_RADIAL_BLUR: {
        float cx = w * p->dx / 100.0f, cy = h * p->dy / 100.0f;
        int k, steps = 16;
        float amt = PC_CLAMP(p->amount, 1, 100) / 100.0f;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                float acc[4] = { 0, 0, 0, 0 }, rx = x - cx, ry = y - cy;
                for (k = 0; k < steps; k++) {
                    float t = (k / (float)(steps - 1) - 0.5f) * amt;
                    float sx, sy;
                    u8 px[4];
                    if (p->mode == 0) { float ca = (float)cos(t * 0.6f), sa = (float)sin(t * 0.6f); sx = cx + rx * ca - ry * sa; sy = cy + rx * sa + ry * ca; }
                    else { sx = cx + rx * (1 + t * 0.5f); sy = cy + ry * (1 + t * 0.5f); }
                    sample_bilinear(s, w, h, sx, sy, px);
                    for (c = 0; c < 4; c++) acc[c] += px[c];
                }
                for (c = 0; c < 4; c++) d[((size_t)y * w + x) * 4 + c] = (u8)(acc[c] / steps + 0.5f);
            }
        break;
    }
    case PC_FILTER_SHARPEN: { static const int k[9] = { 0, -1, 0, -1, 8, -1, 0, -1, 0 }; conv3(s, d, w, h, k, 4, 0); break; }
    case PC_FILTER_SHARPEN_MORE: { static const int k[9] = { -1, -1, -1, -1, 12, -1, -1, -1, -1 }; conv3(s, d, w, h, k, 4, 0); break; }
    case PC_FILTER_SHARPEN_EDGES:
    case PC_FILTER_UNSHARP_MASK: {
        float rad = f == PC_FILTER_UNSHARP_MASK ? PC_CLAMP(p->radius, 0.1f, 250.0f) : 1.0f;
        float amt = f == PC_FILTER_UNSHARP_MASK ? PC_CLAMP(p->amount, 1, 500) / 100.0f : 1.0f;
        int thr = f == PC_FILTER_UNSHARP_MASK ? p->threshold : 8;
        u8 *bl = (u8 *)malloc(n * 4);
        if (!bl) return -1;
        pc_gaussian_rgba(s, bl, w, h, rad);
        for (i = 0; i < n; i++)
            for (c = 0; c < 3; c++) {
                int diff = s[i * 4 + c] - bl[i * 4 + c];
                if (abs(diff) * 2 >= thr) {
                    int v = (int)(s[i * 4 + c] + diff * amt);
                    d[i * 4 + c] = (u8)PC_CLAMP(v, 0, 255);
                }
            }
        free(bl);
        break;
    }
    case PC_FILTER_ADD_NOISE: {
        float amt = PC_CLAMP(p->amount, 1, 999) * 1.28f;
        for (i = 0; i < n; i++) {
            float nv[3];
            for (c = 0; c < 3; c++) {
                if (p->mode == 1) {
                    float u1 = (pc_rand(&rng) % 10000 + 1) / 10001.0f, u2 = (pc_rand(&rng) % 10000) / 10000.0f;
                    nv[c] = (float)(sqrt(-2.0 * log(u1)) * cos(2 * M_PI * u2)) * amt * 0.5f;
                } else {
                    nv[c] = ((pc_rand(&rng) % 20001) / 10000.0f - 1.0f) * amt;
                }
                if (p->mono && c > 0) nv[c] = nv[0];
                { int v = (int)(s[i * 4 + c] + nv[c]); d[i * 4 + c] = (u8)PC_CLAMP(v, 0, 255); }
            }
        }
        break;
    }
    case PC_FILTER_DESPECKLE: {
        /* Median where the local contrast is low: keeps edges, removes speckles. */
        u8 *med = (u8 *)malloc(n * 4);
        if (!med) return -1;
        median(s, med, w, h, 1);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                size_t k = (size_t)y * w + x;
                int gx = pc_luma(s[(k - (x > 0)) * 4], s[(k - (x > 0)) * 4 + 1], s[(k - (x > 0)) * 4 + 2]) -
                         pc_luma(s[(k + (x < w - 1)) * 4], s[(k + (x < w - 1)) * 4 + 1], s[(k + (x < w - 1)) * 4 + 2]);
                if (abs(gx) < 40) memcpy(d + k * 4, med + k * 4, 3);
            }
        free(med);
        break;
    }
    case PC_FILTER_DUST_SCRATCHES:
    case PC_FILTER_MEDIAN: {
        int r = PC_CLAMP((int)(p->radius + 0.5f), 1, 16);
        median(s, d, w, h, r);
        if (f == PC_FILTER_DUST_SCRATCHES && p->threshold > 0) {
            for (i = 0; i < n; i++)
                for (c = 0; c < 3; c++)
                    if (abs(s[i * 4 + c] - d[i * 4 + c]) < p->threshold) d[i * 4 + c] = s[i * 4 + c];
        }
        break;
    }
    case PC_FILTER_EMBOSS: {
        double a = p->angle * M_PI / 180.0;
        float dx = (float)cos(a) * PC_CLAMP(p->distance, 1, 10), dy = (float)-sin(a) * PC_CLAMP(p->distance, 1, 10);
        float amt = PC_CLAMP(p->amount, 1, 500) / 100.0f;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                u8 p1[4], p2[4];
                int v;
                sample_bilinear(s, w, h, x + dx, y + dy, p1);
                sample_bilinear(s, w, h, x - dx, y - dy, p2);
                v = 128 + (int)((pc_luma(p1[0], p1[1], p1[2]) - pc_luma(p2[0], p2[1], p2[2])) * amt);
                v = PC_CLAMP(v, 0, 255);
                d[((size_t)y * w + x) * 4] = d[((size_t)y * w + x) * 4 + 1] = d[((size_t)y * w + x) * 4 + 2] = (u8)v;
            }
        break;
    }
    case PC_FILTER_FIND_EDGES:
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                for (c = 0; c < 3; c++) {
                    #define PX(xx, yy) s[((size_t)PC_CLAMP(yy, 0, h - 1) * w + PC_CLAMP(xx, 0, w - 1)) * 4 + c]
                    int gx = PX(x + 1, y - 1) + 2 * PX(x + 1, y) + PX(x + 1, y + 1) - PX(x - 1, y - 1) - 2 * PX(x - 1, y) - PX(x - 1, y + 1);
                    int gy = PX(x - 1, y + 1) + 2 * PX(x, y + 1) + PX(x + 1, y + 1) - PX(x - 1, y - 1) - 2 * PX(x, y - 1) - PX(x + 1, y - 1);
                    #undef PX
                    int m = (int)sqrt((double)gx * gx + (double)gy * gy) / 2;
                    d[((size_t)y * w + x) * 4 + c] = (u8)(255 - PC_CLAMP(m, 0, 255));
                }
        break;
    case PC_FILTER_SOLARIZE:
        for (i = 0; i < n; i++) for (c = 0; c < 3; c++) { int v = s[i * 4 + c]; d[i * 4 + c] = (u8)(v > 127 ? 255 - v : v); }
        break;
    case PC_FILTER_DIFFUSE:
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                int rx = x + (int)(pc_rand(&rng) % 3) - 1, ry = y + (int)(pc_rand(&rng) % 3) - 1;
                int sx = PC_CLAMP(rx, 0, w - 1), sy = PC_CLAMP(ry, 0, h - 1);
                const u8 *q = s + ((size_t)sy * w + sx) * 4;
                u8 *o = d + ((size_t)y * w + x) * 4;
                int lq = pc_luma(q[0], q[1], q[2]), lo = pc_luma(o[0], o[1], o[2]);
                if (p->mode == 0 || (p->mode == 1 && lq < lo) || (p->mode == 2 && lq > lo)) memcpy(o, q, 3);
            }
        break;
    case PC_FILTER_WIND:
        for (y = 0; y < h; y++) {
            int len = 0;
            u8 carry[3] = { 0, 0, 0 };
            for (x = 0; x < w; x++) {
                int xx = p->angle ? w - 1 - x : x;
                u8 *o = d + ((size_t)y * w + xx) * 4;
                const u8 *q = s + ((size_t)y * w + xx) * 4;
                if (pc_luma(q[0], q[1], q[2]) > 160 && pc_rand(&rng) % 8 == 0) { memcpy(carry, q, 3); len = 4 + (int)(pc_rand(&rng) % 20); }
                if (len > 0) { for (c = 0; c < 3; c++) o[c] = (u8)((o[c] + carry[c] * 2) / 3); len--; }
            }
        }
        break;
    case PC_FILTER_TRACE_CONTOUR:
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                const u8 *q = s + ((size_t)y * w + x) * 4;
                const u8 *r = s + ((size_t)y * w + PC_MIN(x + 1, w - 1)) * 4;
                const u8 *b = s + ((size_t)PC_MIN(y + 1, h - 1) * w + x) * 4;
                for (c = 0; c < 3; c++) {
                    int t = p->threshold, a0 = q[c] >= t, a1 = r[c] >= t, a2 = b[c] >= t;
                    d[((size_t)y * w + x) * 4 + c] = (u8)((a0 != a1 || a0 != a2) ? 0 : 255);
                }
            }
        break;
    case PC_FILTER_MOSAIC: {
        int cs = PC_CLAMP(p->cell, 2, 200), bx, by;
        for (by = 0; by < h; by += cs)
            for (bx = 0; bx < w; bx += cs) {
                unsigned acc[4] = { 0, 0, 0, 0 }, cnt = 0;
                for (y = by; y < PC_MIN(by + cs, h); y++)
                    for (x = bx; x < PC_MIN(bx + cs, w); x++) { for (c = 0; c < 4; c++) acc[c] += s[((size_t)y * w + x) * 4 + c]; cnt++; }
                for (y = by; y < PC_MIN(by + cs, h); y++)
                    for (x = bx; x < PC_MIN(bx + cs, w); x++) for (c = 0; c < 4; c++) d[((size_t)y * w + x) * 4 + c] = (u8)(acc[c] / cnt);
            }
        break;
    }
    case PC_FILTER_TWIRL: case PC_FILTER_PINCH: case PC_FILTER_SPHERIZE: case PC_FILTER_RIPPLE: {
        float cx = (w - 1) / 2.0f, cy = (h - 1) / 2.0f, rad = PC_MIN(w, h) / 2.0f;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                float dx = x - cx, dy = y - cy, dist = (float)sqrt(dx * dx + dy * dy), sx = (float)x, sy = (float)y;
                if (f == PC_FILTER_RIPPLE) {
                    float wl = p->mode == 0 ? 6.0f : p->mode == 1 ? 12.0f : 24.0f, amp = PC_CLAMP(p->amount, -999, 999) / 100.0f * wl / 3;
                    sx = x + amp * (float)sin(y * 2 * M_PI / wl);
                    sy = y + amp * (float)sin(x * 2 * M_PI / wl);
                } else if (dist < rad && rad > 0) {
                    float t = dist / rad;
                    if (f == PC_FILTER_TWIRL) {
                        float a = (float)(PC_CLAMP(p->amount, -999, 999) * M_PI / 180.0) * (1 - t);
                        float ca = (float)cos(a), sa = (float)sin(a);
                        sx = cx + dx * ca - dy * sa; sy = cy + dx * sa + dy * ca;
                    } else if (f == PC_FILTER_PINCH) {
                        float amt = PC_CLAMP(p->amount, -100, 100) / 100.0f;
                        float sc = (float)pow(sin(M_PI / 2 * t), -amt);
                        if (dist > 0) { sx = cx + dx * sc; sy = cy + dy * sc; }
                    } else {
                        float amt = PC_CLAMP(p->amount, -100, 100) / 100.0f;
                        float nt = amt >= 0 ? (float)(asin(t) / (M_PI / 2)) : (float)sin(t * M_PI / 2);
                        float mix = (float)fabs(amt), sc = t > 0 ? ((nt * mix + t * (1 - mix)) / t) : 1;
                        sx = cx + dx * sc; sy = cy + dy * sc;
                    }
                }
                sample_bilinear(s, w, h, sx, sy, d + ((size_t)y * w + x) * 4);
            }
        break;
    }
    case PC_FILTER_CLOUDS: case PC_FILTER_DIFF_CLOUDS: {
        float base = (float)PC_MAX(w, h) / 2;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                float v = 0, amp = 0.5f, fr = 1.0f / base * 4;
                int o;
                for (o = 0; o < 7; o++) { v += vnoise(x * fr, y * fr, p->seed + (unsigned)o) * amp; amp *= 0.5f; fr *= 2; }
                v = PC_CLAMP(v, 0, 1);
                {
                    u8 *q = d + ((size_t)y * w + x) * 4;
                    int r = (int)(pc_r(p->fg) + (pc_r(p->bg) - pc_r(p->fg)) * v);
                    int g = (int)(pc_g(p->fg) + (pc_g(p->bg) - pc_g(p->fg)) * v);
                    int b = (int)(pc_b(p->fg) + (pc_b(p->bg) - pc_b(p->fg)) * v);
                    if (f == PC_FILTER_DIFF_CLOUDS) { q[0] = (u8)abs(q[0] - r); q[1] = (u8)abs(q[1] - g); q[2] = (u8)abs(q[2] - b); }
                    else { q[0] = (u8)r; q[1] = (u8)g; q[2] = (u8)b; q[3] = 255; }
                }
            }
        break;
    }
    case PC_FILTER_OFFSET:
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                int sx = x - p->dx, sy = y - p->dy;
                u8 *o = d + ((size_t)y * w + x) * 4;
                if (p->wrap == 2) { sx = ((sx % w) + w) % w; sy = ((sy % h) + h) % h; }
                else if (p->wrap == 1) { sx = PC_CLAMP(sx, 0, w - 1); sy = PC_CLAMP(sy, 0, h - 1); }
                if (sx < 0 || sy < 0 || sx >= w || sy >= h) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
                memcpy(o, s + ((size_t)sy * w + sx) * 4, 4);
            }
        break;
    case PC_FILTER_HIGH_PASS: {
        u8 *bl = (u8 *)malloc(n * 4);
        if (!bl) return -1;
        pc_gaussian_rgba(s, bl, w, h, PC_CLAMP(p->radius, 0.1f, 250.0f));
        for (i = 0; i < n; i++) for (c = 0; c < 3; c++) { int v = s[i * 4 + c] - bl[i * 4 + c] + 128; d[i * 4 + c] = (u8)PC_CLAMP(v, 0, 255); }
        free(bl);
        break;
    }
    case PC_FILTER_MAXIMUM: minmax(s, d, w, h, PC_CLAMP((int)p->radius, 1, 10), 1); break;
    case PC_FILTER_MINIMUM: minmax(s, d, w, h, PC_CLAMP((int)p->radius, 1, 10), 0); break;
    case PC_FILTER_CUSTOM: {
        int sc = p->scale ? p->scale : 1, j, k;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                for (c = 0; c < 3; c++) {
                    int acc = 0;
                    for (j = -2; j <= 2; j++)
                        for (k = -2; k <= 2; k++)
                            acc += s[((size_t)PC_CLAMP(y + j, 0, h - 1) * w + PC_CLAMP(x + k, 0, w - 1)) * 4 + c] * p->kernel[(j + 2) * 5 + k + 2];
                    acc = acc / sc + p->offset;
                    d[((size_t)y * w + x) * 4 + c] = (u8)PC_CLAMP(acc, 0, 255);
                }
        break;
    }
    default: return -1;
    }
    return 0;
}

int pc_filter_apply(PcDoc *d, int f, const PcFilterParams *p)
{
    PcLayer *l = pc_doc_active(d);
    size_t n = (size_t)d->s.w * d->s.h;
    u8 *res;
    int rc;
    if (!l || l->lock_pixels) return -1;
    res = (u8 *)pc_alloc_checked(n, 4, 1, 0);
    if (!res) return -1;
    rc = pc_filter_run(f, p, l->pix->px, res, d->s.w, d->s.h);
    if (!rc) rc = pc_apply_result(d, l, res);
    free(res);
    return rc;
}
