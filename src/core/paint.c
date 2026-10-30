/*
 * Brush engine: dab-based strokes for the painting and retouching tools.
 *
 * Deposit tools (paintbrush, airbrush, pencil, eraser, stamps, history
 * brush) accumulate a per-stroke coverage buffer and recomposite the
 * stroke over the pixels saved at stroke start, so a stroke never exceeds
 * its opacity however often it crosses itself - Photoshop's behaviour, and
 * PhotoCraft's crates/paint model.  The airbrush builds coverage up over
 * time instead.  Retouch tools (blur, sharpen, smudge, dodge, burn, sponge)
 * modify the pixels directly per dab.
 */
#include "pcops.h"

enum { TARGET_LAYER, TARGET_MASK, TARGET_QUICKMASK, TARGET_CHANNEL };

struct PcStroke {
    PcDoc *d;
    PcLayer *l;
    PcPaintOpts o;
    int target;
    int w, h;
    u8 *px;            /* target pixels (RGBA, or 1 channel for masks) */
    int ch;
    u8 *orig;          /* copy of px at stroke start */
    u8 *cov;           /* stroke coverage 0..255 (deposit tools) */
    float lx, ly;      /* last dab */
    float resid;
    int ndabs;
    int dab_w, dab_h;  /* dab footprint (odd sizes) */
    float *dab;        /* tip alpha 0..1 */
    float src_dx, src_dy;
    int have_src;
    u8 *src_pix;       /* clone source snapshot (RGBA doc-size) */
    PcBuf *hist_buf;   /* history brush source */
    float *carry;      /* smudge colour, dab-sized RGBA */
    unsigned rng;
};

/* Persistent between strokes. */
static float g_clone_x = -1, g_clone_y = -1;
static PcDoc *g_clone_doc;
static float g_clone_dx, g_clone_dy;
static int g_clone_have_offset;
static u8 *g_pattern;
static int g_pat_w, g_pat_h;

void pc_clone_set_source(PcDoc *d, float x, float y)
{
    g_clone_doc = d; g_clone_x = x; g_clone_y = y; g_clone_have_offset = 0;
}

void pc_pattern_set(const u8 *rgba, int w, int h)
{
    u8 *n;
    if (!rgba || w <= 0 || h <= 0) return;
    n = (u8 *)pc_alloc_checked((size_t)w, (size_t)h, 4, 0);
    if (!n) return;
    memcpy(n, rgba, (size_t)w * h * 4);
    free(g_pattern);
    g_pattern = n; g_pat_w = w; g_pat_h = h;
}

int pc_pattern_get(const u8 **rgba, int *w, int *h)
{
    if (!g_pattern) return -1;
    *rgba = g_pattern; *w = g_pat_w; *h = g_pat_h;
    return 0;
}

static void build_dab(PcStroke *s)
{
    const PcBrush *b = &s->o.brush;
    int dia = PC_CLAMP(b->diameter, 1, 999), x, y;
    float r = dia / 2.0f, hard = PC_CLAMP(b->hardness, 0, 100) / 100.0f;
    float rnd = PC_CLAMP(b->roundness, 1, 100) / 100.0f;
    double ang = b->angle * M_PI / 180.0, ca = cos(ang), sa = sin(ang);
    int pencil = s->o.tool == PC_PAINT_PENCIL || (s->o.tool == PC_PAINT_ERASER && s->o.eraser_mode >= 2);
    int block = s->o.tool == PC_PAINT_ERASER && s->o.eraser_mode == 3;
    if (block) { dia = 16; r = 8; }
    s->dab_w = s->dab_h = (dia | 1) + 2;
    s->dab = (float *)malloc(sizeof(float) * (size_t)s->dab_w * s->dab_h);
    if (!s->dab) return;
    for (y = 0; y < s->dab_h; y++)
        for (x = 0; x < s->dab_w; x++) {
            float dx = x - (s->dab_w - 1) / 2.0f, dy = y - (s->dab_h - 1) / 2.0f;
            float rx = (float)(dx * ca + dy * sa), ry = (float)(-dx * sa + dy * ca) / rnd;
            float dist = (float)sqrt(rx * rx + ry * ry), a;
            if (block) a = (fabs(dx) <= r && fabs(dy) <= r) ? 1.0f : 0.0f;
            else if (pencil) a = dist <= r - 0.25f || (dia == 1 && dist < 0.5f) ? 1.0f : 0.0f;
            else {
                float edge = r * hard;
                if (dist <= edge) a = 1;
                else if (dist >= r + 0.5f) a = 0;
                else {
                    /* Smooth falloff from the hard core to the edge (+1/2 px AA). */
                    float t = (dist - edge) / (r + 0.5f - edge);
                    a = 1 - t;
                    a = a * a * (3 - 2 * a);
                }
            }
            s->dab[y * s->dab_w + x] = a;
        }
}

static int is_deposit(int tool)
{
    return tool <= PC_PAINT_HISTORY;
}

PcStroke *pc_stroke_begin(PcDoc *d, const PcPaintOpts *o, float x, float y)
{
    PcStroke *s;
    PcLayer *l = pc_doc_active(d);
    size_t n = (size_t)d->s.w * d->s.h;
    if (!l && !o->target_mask) return NULL;
    s = (PcStroke *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->d = d; s->l = l; s->o = *o; s->w = d->s.w; s->h = d->s.h;
    s->rng = 0x9E3779B9u ^ (unsigned)(x * 31 + y * 17);
    s->target = o->target_mask == 3 ? TARGET_CHANNEL : o->target_mask == 2 ? TARGET_QUICKMASK :
                (o->target_mask && l && l->mask) ? TARGET_MASK : TARGET_LAYER;
    if (s->target == TARGET_CHANNEL && (o->channel < 0 || o->channel >= d->s.nchannels)) { free(s); return NULL; }
    if (s->target == TARGET_LAYER) {
        if (!l || l->lock_pixels) { free(s); return NULL; }
        s->px = pc_layer_pixels_w(l); s->ch = 4;
    } else if (s->target == TARGET_MASK) {
        s->px = pc_layer_mask_w(l); s->ch = 1;
    } else if (s->target == TARGET_CHANNEL) {
        PcBuf *cb = pc_buf_writable(&d->s.channels[o->channel].buf);
        s->px = cb ? cb->px : NULL; s->ch = 1;
    } else {
        PcBuf *sb = pc_sel_get(d, 1);
        s->px = sb ? sb->px : NULL; s->ch = 1;
    }
    if (!s->px) { free(s); return NULL; }
    s->orig = (u8 *)pc_alloc_checked(n, (size_t)s->ch, 1, 0);
    if (!s->orig) { free(s); return NULL; }
    memcpy(s->orig, s->px, n * s->ch);
    if (is_deposit(o->tool)) {
        s->cov = (u8 *)pc_alloc_checked(n, 1, 1, 1);
        if (!s->cov) { free(s->orig); free(s); return NULL; }
    }
    build_dab(s);
    if (!s->dab) { free(s->cov); free(s->orig); free(s); return NULL; }
    if (o->tool == PC_PAINT_CLONE) {
        PcBuf *src = o->sample_merged ? pc_doc_composite(d) : NULL;
        s->src_pix = (u8 *)malloc(n * 4);
        if (s->src_pix) {
            if (src) memcpy(s->src_pix, src->px, n * 4);
            else if (s->ch == 4) memcpy(s->src_pix, s->orig, n * 4);
            else memcpy(s->src_pix, l->pix->px, n * 4);
        }
        pc_buf_unref(src);
        if (g_clone_x >= 0 && g_clone_doc == d) {
            if (o->aligned && g_clone_have_offset) { s->src_dx = g_clone_dx; s->src_dy = g_clone_dy; }
            else { s->src_dx = g_clone_x - x; s->src_dy = g_clone_y - y; g_clone_dx = s->src_dx; g_clone_dy = s->src_dy; g_clone_have_offset = 1; }
            s->have_src = 1;
        }
    }
    if (o->tool == PC_PAINT_HISTORY || (o->tool == PC_PAINT_ERASER && o->erase_to_history)) {
        const PcDocState *hs = pc_hist_state(d, d->history_source);
        if (hs && hs->w == d->s.w && hs->h == d->s.h && hs->nlayers > 0) {
            /* Same layer index if it exists, else the merged state. */
            int li = d->s.active < hs->nlayers ? d->s.active : hs->nlayers - 1;
            s->hist_buf = pc_buf_ref(hs->layers[li]->pix);
        }
    }
    if (o->tool == PC_PAINT_SMUDGE) {
        s->carry = (float *)calloc((size_t)s->dab_w * s->dab_h * 4, sizeof(float));
    }
    s->lx = x; s->ly = y;
    return s;
}

static u8 gray_of(u32 c) { return (u8)pc_luma(pc_r(c), pc_g(c), pc_b(c)); }

/* Recomposite a deposit stroke inside rect r. */
static void recomposite(PcStroke *s, int x0, int y0, int x1, int y1)
{
    const PcPaintOpts *o = &s->o;
    int x, y, w = s->w;
    int opacity = PC_CLAMP(o->opacity, 0, 100) * 255 / 100;
    int lock = s->target == TARGET_LAYER && s->l && (s->l->lock_alpha);
    int bg_layer = s->target == TARGET_LAYER && s->l && s->l->is_background;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            size_t i = (size_t)y * w + x;
            int a = s->cov[i];
            if (!a) continue;
            a = pc_mul255(a, opacity);
            if (s->target != TARGET_QUICKMASK) a = pc_mul255(a, pc_sel_at(s->d, x, y));
            if (s->ch == 1) {
                int v;
                u8 g;
                if (o->tool == PC_PAINT_ERASER) g = gray_of(o->bg);
                else g = gray_of(o->color);
                v = (g * a + s->orig[i] * (255 - a) + 127) / 255;
                s->px[i] = (u8)v;
                continue;
            }
            {
                u8 *dst = s->px + i * 4;
                const u8 *org = s->orig + i * 4;
                u8 src[4];
                int sa = a;
                memcpy(dst, org, 4);
                switch (o->tool) {
                case PC_PAINT_ERASER:
                    if (o->erase_to_history && s->hist_buf) {
                        memcpy(src, s->hist_buf->px + i * 4, 4);
                        sa = pc_mul255(a, src[3]);
                        if (!bg_layer && !lock) {
                            /* Restore exactly: lerp all four channels. */
                            int k;
                            for (k = 0; k < 4; k++) dst[k] = (u8)((src[k] * a + org[k] * (255 - a) + 127) / 255);
                            continue;
                        }
                        break;
                    }
                    if (bg_layer || lock) {
                        src[0] = (u8)pc_r(o->bg); src[1] = (u8)pc_g(o->bg); src[2] = (u8)pc_b(o->bg); src[3] = 255;
                        break;
                    }
                    dst[3] = (u8)(org[3] * (255 - a) / 255);
                    continue;
                case PC_PAINT_CLONE: {
                    int sx = (int)floor(x + s->src_dx + 0.5f), sy = (int)floor(y + s->src_dy + 0.5f);
                    if (!s->have_src || !s->src_pix || sx < 0 || sy < 0 || sx >= w || sy >= s->h) continue;
                    memcpy(src, s->src_pix + ((size_t)sy * w + sx) * 4, 4);
                    sa = pc_mul255(a, src[3]);
                    break;
                }
                case PC_PAINT_PATTERN: {
                    int px, py;
                    if (!g_pattern) continue;
                    px = x % g_pat_w; py = y % g_pat_h;
                    memcpy(src, g_pattern + ((size_t)py * g_pat_w + px) * 4, 4);
                    sa = pc_mul255(a, src[3]);
                    break;
                }
                case PC_PAINT_HISTORY:
                    if (!s->hist_buf) continue;
                    memcpy(src, s->hist_buf->px + i * 4, 4);
                    if (!bg_layer && !lock) {
                        int k;
                        for (k = 0; k < 4; k++) dst[k] = (u8)((src[k] * a + org[k] * (255 - a) + 127) / 255);
                        continue;
                    }
                    break;
                default:
                    src[0] = (u8)pc_r(o->color); src[1] = (u8)pc_g(o->color); src[2] = (u8)pc_b(o->color); src[3] = 255;
                    break;
                }
                pc_blend_pixel(o->blend, dst, src, sa, s->rng ^ (unsigned)(x * 7919 + y * 104729));
                if (lock || bg_layer) dst[3] = org[3];
                if (s->d->s.mode == PC_MODE_GRAY) dst[0] = dst[1] = dst[2] = (u8)pc_luma(dst[0], dst[1], dst[2]);
            }
        }
}

/* Retouch tools: direct modification inside the dab. */
static void retouch_dab(PcStroke *s, int cx, int cy, float strength)
{
    int x, y, w = s->w, h = s->h, ch = s->ch;
    int hw = s->dab_w / 2, hh = s->dab_h / 2;
    const PcPaintOpts *o = &s->o;
    u8 *area;
    int ax0 = PC_MAX(cx - hw - 1, 0), ay0 = PC_MAX(cy - hh - 1, 0), ax1 = PC_MIN(cx + hw + 2, w), ay1 = PC_MIN(cy + hh + 2, h);
    int aw = ax1 - ax0, ah = ay1 - ay0;
    if (aw <= 0 || ah <= 0) return;
    area = (u8 *)malloc((size_t)aw * ah * ch);
    if (!area) return;
    for (y = 0; y < ah; y++) memcpy(area + (size_t)y * aw * ch, s->px + ((size_t)(ay0 + y) * w + ax0) * ch, (size_t)aw * ch);
    for (y = 0; y < s->dab_h; y++)
        for (x = 0; x < s->dab_w; x++) {
            int px = cx - hw + x, py = cy - hh + y, k;
            float a = s->dab[y * s->dab_w + x] * strength;
            u8 *p;
            if (px < 0 || py < 0 || px >= w || py >= h || a <= 0.003f) continue;
            a *= pc_sel_at(s->d, px, py) / 255.0f;
            if (a <= 0.003f) continue;
            p = s->px + ((size_t)py * w + px) * ch;
            if (o->tool == PC_PAINT_BLUR || o->tool == PC_PAINT_SHARPEN) {
                for (k = 0; k < ch; k++) {
                    int sum = 0, i, j, cnt = 0, v;
                    for (j = -1; j <= 1; j++)
                        for (i = -1; i <= 1; i++) {
                            int qx = PC_CLAMP(px + i, ax0, ax1 - 1) - ax0, qy = PC_CLAMP(py + j, ay0, ay1 - 1) - ay0;
                            sum += area[((size_t)qy * aw + qx) * ch + k]; cnt++;
                        }
                    if (o->tool == PC_PAINT_BLUR) v = sum / cnt;
                    else { int c0 = area[((size_t)(py - ay0) * aw + (px - ax0)) * ch + k]; v = c0 + (c0 * cnt - sum) / 4; }
                    v = PC_CLAMP(v, 0, 255);
                    if (ch == 4 && k == 3 && o->tool == PC_PAINT_SHARPEN) continue;
                    p[k] = (u8)(p[k] + (v - p[k]) * a);
                }
            } else if (o->tool == PC_PAINT_SMUDGE && s->carry) {
                float *c = s->carry + ((size_t)y * s->dab_w + x) * 4;
                for (k = 0; k < ch; k++) {
                    float cur = p[k];
                    float nv = cur + (c[k] - cur) * a;
                    p[k] = (u8)PC_CLAMP((int)(nv + 0.5f), 0, 255);
                    c[k] = c[k] + (cur - c[k]) * (1 - strength);
                }
            } else if (ch == 4) {
                float hh2, ss, ll;
                int r = p[0], g = p[1], b = p[2];
                if (o->tool == PC_PAINT_DODGE || o->tool == PC_PAINT_BURN) {
                    int lum = pc_luma(r, g, b);
                    float wgt = o->range == 0 ? (1 - lum / 255.0f) : o->range == 2 ? lum / 255.0f : 1 - (float)fabs(lum / 127.5f - 1);
                    float e = a * wgt * 0.5f;
                    for (k = 0; k < 3; k++) {
                        float v = p[k];
                        v = o->tool == PC_PAINT_DODGE ? v + (255 - v) * e : v * (1 - e);
                        p[k] = (u8)PC_CLAMP((int)(v + 0.5f), 0, 255);
                    }
                } else if (o->tool == PC_PAINT_SPONGE) {
                    pc_rgb_to_hsl(r, g, b, &hh2, &ss, &ll);
                    ss = o->range ? ss + (1 - ss) * a * 0.5f : ss * (1 - a * 0.5f);
                    pc_hsl_to_rgb(hh2, ss, ll, &r, &g, &b);
                    p[0] = (u8)PC_CLAMP(r, 0, 255); p[1] = (u8)PC_CLAMP(g, 0, 255); p[2] = (u8)PC_CLAMP(b, 0, 255);
                }
            } else if (o->tool == PC_PAINT_DODGE || o->tool == PC_PAINT_BURN) {
                float v = p[0], e = a * 0.5f;
                v = o->tool == PC_PAINT_DODGE ? v + (255 - v) * e : v * (1 - e);
                p[0] = (u8)PC_CLAMP((int)(v + 0.5f), 0, 255);
            }
        }
    free(area);
}

static void smudge_pick(PcStroke *s, int cx, int cy)
{
    int x, y, k, hw = s->dab_w / 2, hh = s->dab_h / 2;
    for (y = 0; y < s->dab_h; y++)
        for (x = 0; x < s->dab_w; x++) {
            int px = PC_CLAMP(cx - hw + x, 0, s->w - 1), py = PC_CLAMP(cy - hh + y, 0, s->h - 1);
            float *c = s->carry + ((size_t)y * s->dab_w + x) * 4;
            for (k = 0; k < s->ch; k++) c[k] = s->px[((size_t)py * s->w + px) * s->ch + k];
            if (s->o.finger && s->ch == 4) { c[0] = (float)pc_r(s->o.color); c[1] = (float)pc_g(s->o.color); c[2] = (float)pc_b(s->o.color); }
        }
}

static void dab_at(PcStroke *s, float fx, float fy, PcRect *dirty)
{
    int cx = (int)floor(fx + 0.5f), cy = (int)floor(fy + 0.5f);
    int hw = s->dab_w / 2, hh = s->dab_h / 2, x, y;
    int x0 = PC_MAX(cx - hw, 0), y0 = PC_MAX(cy - hh, 0), x1 = PC_MIN(cx - hw + s->dab_w, s->w), y1 = PC_MIN(cy - hh + s->dab_h, s->h);
    float fade = 1;
    if (x0 >= x1 || y0 >= y1) { s->ndabs++; return; }
    if (s->o.fade_steps > 0) {
        fade = 1.0f - (float)s->ndabs / s->o.fade_steps;
        if (fade <= 0) return;
    }
    if (is_deposit(s->o.tool)) {
        int air = s->o.tool == PC_PAINT_AIRBRUSH || (s->o.tool == PC_PAINT_ERASER && s->o.eraser_mode == 1);
        float flow = air ? PC_CLAMP(s->o.opacity, 1, 100) / 400.0f : 1.0f;
        for (y = y0; y < y1; y++)
            for (x = x0; x < x1; x++) {
                float a = s->dab[(y - (cy - hh)) * s->dab_w + (x - (cx - hw))] * fade;
                size_t i = (size_t)y * s->w + x;
                int v;
                if (a <= 0) continue;
                if (air) { v = s->cov[i] + (int)((255 - s->cov[i]) * a * flow + 0.5f); }
                else { v = (int)(a * 255 + 0.5f); if (v < s->cov[i]) v = s->cov[i]; }
                s->cov[i] = (u8)PC_CLAMP(v, 0, 255);
            }
        if (air) {
            /* Airbrush paints at full stroke opacity; flow controls build-up. */
            int save = s->o.opacity;
            s->o.opacity = 100;
            recomposite(s, x0, y0, x1, y1);
            s->o.opacity = save;
        } else {
            recomposite(s, x0, y0, x1, y1);
        }
    } else {
        float strength = PC_CLAMP(s->o.opacity, 1, 100) / 100.0f * fade;
        if (s->o.tool == PC_PAINT_SMUDGE && s->ndabs == 0 && s->carry) smudge_pick(s, cx, cy);
        retouch_dab(s, cx, cy, strength);
    }
    s->ndabs++;
    if (dirty) {
        if (dirty->x1 <= dirty->x0) { dirty->x0 = x0; dirty->y0 = y0; dirty->x1 = x1; dirty->y1 = y1; }
        else { dirty->x0 = PC_MIN(dirty->x0, x0); dirty->y0 = PC_MIN(dirty->y0, y0); dirty->x1 = PC_MAX(dirty->x1, x1); dirty->y1 = PC_MAX(dirty->y1, y1); }
    }
}

void pc_stroke_to(PcStroke *s, float x, float y, PcRect *dirty)
{
    float dx, dy, dist, step, t;
    if (!s) return;
    if (dirty) { dirty->x0 = dirty->y0 = 0; dirty->x1 = dirty->y1 = 0; }
    if (s->ndabs == 0) { dab_at(s, s->lx, s->ly, dirty); }
    dx = x - s->lx; dy = y - s->ly;
    dist = (float)sqrt(dx * dx + dy * dy);
    step = PC_MAX(1.0f, s->o.brush.diameter * PC_CLAMP(s->o.brush.spacing, 1, 999) / 100.0f);
    if (s->o.tool >= PC_PAINT_BLUR) step = PC_MAX(1.0f, step * 0.5f);
    if (dist <= 0) return;
    t = step - s->resid;
    while (t <= dist) {
        dab_at(s, s->lx + dx * t / dist, s->ly + dy * t / dist, dirty);
        t += step;
    }
    s->resid = dist - (t - step);
    s->lx = x; s->ly = y;
}

void pc_stroke_tick(PcStroke *s, PcRect *dirty)
{
    if (!s) return;
    if (dirty) { dirty->x0 = dirty->y0 = 0; dirty->x1 = dirty->y1 = 0; }
    if (s->o.tool == PC_PAINT_AIRBRUSH || (s->o.tool == PC_PAINT_ERASER && s->o.eraser_mode == 1)) dab_at(s, s->lx, s->ly, dirty);
}

void pc_stroke_set_clone_source(PcStroke *s, float sx, float sy)
{
    if (!s) return;
    s->src_dx = sx - s->lx; s->src_dy = sy - s->ly; s->have_src = 1;
}

void pc_stroke_end(PcStroke *s)
{
    if (!s) return;
    if (s->target == TARGET_QUICKMASK) {
        /* A fully deselected quick mask leaves no selection. */
        size_t i, n = (size_t)s->w * s->h;
        for (i = 0; i < n; i++) if (s->px[i]) break;
        (void)i;
    }
    if (s->o.tool == PC_PAINT_CLONE && s->have_src && !s->o.aligned) g_clone_have_offset = 0;
    pc_doc_touch(s->d);
    free(s->orig); free(s->cov); free(s->dab); free(s->src_pix); free(s->carry);
    pc_buf_unref(s->hist_buf);
    free(s);
}

int pc_paint_polyline(PcDoc *d, const PcPaintOpts *o, const float *xy, const u8 *starts, int n)
{
    int i;
    PcStroke *s = NULL;
    PcRect r;
    if (n < 1) return -1;
    for (i = 0; i < n; i++) {
        if (!s || (starts && starts[i])) {
            if (s) pc_stroke_end(s);
            s = pc_stroke_begin(d, o, xy[i * 2], xy[i * 2 + 1]);
            if (!s) return -1;
            pc_stroke_to(s, xy[i * 2], xy[i * 2 + 1], &r);
            continue;
        }
        pc_stroke_to(s, xy[i * 2], xy[i * 2 + 1], &r);
    }
    if (s) pc_stroke_end(s);
    return 0;
}
