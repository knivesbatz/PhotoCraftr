/*
 * Document model: shared buffers, layers, layer operations, history, paths.
 * The history model (immutable states sharing pixel buffers) follows
 * PhotoCraft's crates/ops history design.
 */
#include "pcdoc.h"

/* ---------------- buffers ---------------- */

PcBuf *pc_buf_new(int w, int h, int ch, int zero)
{
    PcBuf *b;
    if (w <= 0 || h <= 0 || (ch != 1 && ch != 4)) return NULL;
    b = (PcBuf *)calloc(1, sizeof *b);
    if (!b) return NULL;
    b->px = (u8 *)pc_alloc_checked((size_t)w, (size_t)h, (size_t)ch, zero);
    if (!b->px) { free(b); return NULL; }
    b->refs = 1; b->w = w; b->h = h; b->ch = ch;
    return b;
}

PcBuf *pc_buf_clone(const PcBuf *s)
{
    PcBuf *b;
    if (!s) return NULL;
    b = pc_buf_new(s->w, s->h, s->ch, 0);
    if (b) memcpy(b->px, s->px, (size_t)s->w * s->h * s->ch);
    return b;
}

PcBuf *pc_buf_ref(PcBuf *b) { if (b) b->refs++; return b; }

void pc_buf_unref(PcBuf *b)
{
    if (b && --b->refs <= 0) { free(b->px); free(b); }
}

PcBuf *pc_buf_writable(PcBuf **pb)
{
    PcBuf *c;
    if (!*pb) return NULL;
    if ((*pb)->refs == 1) return *pb;
    c = pc_buf_clone(*pb);
    if (!c) return NULL;
    pc_buf_unref(*pb);
    *pb = c;
    return c;
}

u32 pc_rgb(int r, int g, int b) { return ((u32)(r & 255) << 16) | ((u32)(g & 255) << 8) | (u32)(b & 255); }

/* ---------------- blend mode names ---------------- */

static const char *const blend_names[PC_BLEND_COUNT] = {
    "Normal", "Dissolve", "Multiply", "Screen", "Overlay", "Soft Light", "Hard Light",
    "Color Dodge", "Color Burn", "Darken", "Lighten", "Difference", "Exclusion",
    "Hue", "Saturation", "Color", "Luminosity"
};
static const char *const blend_keys[PC_BLEND_COUNT] = {
    "norm", "diss", "mul ", "scrn", "over", "sLit", "hLit", "div ", "idiv", "dark",
    "lite", "diff", "smud", "hue ", "sat ", "colr", "lum "
};

const char *pc_blend_name(int b) { return (b >= 0 && b < PC_BLEND_COUNT) ? blend_names[b] : "Normal"; }
const char *pc_blend_psd_key(int b) { return (b >= 0 && b < PC_BLEND_COUNT) ? blend_keys[b] : "norm"; }

int pc_blend_from_psd_key(const char *k)
{
    int i;
    for (i = 0; i < PC_BLEND_COUNT; i++) if (!memcmp(k, blend_keys[i], 4)) return i;
    /* Later Photoshop modes fall back to the nearest 1998 mode. */
    if (!memcmp(k, "lbrn", 4)) return PC_BLEND_MULTIPLY;
    if (!memcmp(k, "lddg", 4)) return PC_BLEND_SCREEN;
    if (!memcmp(k, "dkCl", 4)) return PC_BLEND_DARKEN;
    if (!memcmp(k, "lgCl", 4)) return PC_BLEND_LIGHTEN;
    if (!memcmp(k, "vLit", 4) || !memcmp(k, "lLit", 4) || !memcmp(k, "pLit", 4) || !memcmp(k, "hMix", 4)) return PC_BLEND_HARD_LIGHT;
    if (!memcmp(k, "fsub", 4)) return PC_BLEND_DIFFERENCE;
    return PC_BLEND_NORMAL;
}

/* ---------------- layers ---------------- */

static void layer_defaults(PcLayer *l)
{
    l->opacity = 255; l->blend = PC_BLEND_NORMAL; l->visible = 1; l->mask_linked = 1;
    l->text.size = 24; l->text.antialias = 2;
    pc_strlcpy(l->text.font, "Arial", sizeof l->text.font);
    l->fx_shadow_opacity = 191; l->fx_shadow_angle = 120; l->fx_shadow_dist = 5; l->fx_shadow_blur = 5;
    l->fx_glow_opacity = 191; l->fx_glow_size = 5; l->fx_glow_color = 0xFFFFBE;
    l->fx_bevel_depth = 3; l->fx_bevel_size = 5; l->fx_bevel_angle = 120;
    l->fx_inner_opacity = 191; l->fx_inner_dist = 5; l->fx_inner_blur = 5;
}

PcLayer *pc_layer_new(int w, int h, const char *name)
{
    PcLayer *l = (PcLayer *)calloc(1, sizeof *l);
    if (!l) return NULL;
    l->pix = pc_buf_new(w, h, 4, 1);
    if (!l->pix) { free(l); return NULL; }
    layer_defaults(l);
    pc_strlcpy(l->name, name ? name : "Layer", sizeof l->name);
    return l;
}

PcLayer *pc_layer_clone(const PcLayer *s)
{
    PcLayer *l = (PcLayer *)malloc(sizeof *l);
    if (!l) return NULL;
    *l = *s;
    pc_buf_ref(l->pix);
    pc_buf_ref(l->mask);
    return l;
}

void pc_layer_free(PcLayer *l)
{
    if (!l) return;
    pc_buf_unref(l->pix);
    pc_buf_unref(l->mask);
    free(l);
}

u8 *pc_layer_pixels_w(PcLayer *l)
{
    PcBuf *b = pc_buf_writable(&l->pix);
    return b ? b->px : NULL;
}

u8 *pc_layer_mask_w(PcLayer *l)
{
    PcBuf *b;
    if (!l->mask) return NULL;
    b = pc_buf_writable(&l->mask);
    return b ? b->px : NULL;
}

/* ---------------- document ---------------- */

void pc_doc_touch(PcDoc *d) { d->dirty = 1; d->revision++; }

PcDoc *pc_doc_new(int w, int h, int mode, double dpi, int fill, u32 rgb)
{
    PcDoc *d;
    PcLayer *l;
    if (w <= 0 || h <= 0 || w > 30000 || h > 30000 || (size_t)w * h > (size_t)100 * 1000 * 1000) return NULL;
    d = (PcDoc *)calloc(1, sizeof *d);
    if (!d) return NULL;
    d->s.w = w; d->s.h = h; d->s.mode = mode; d->s.dpi = dpi > 0 ? dpi : 72;
    d->s.active_path = -1;
    d->hist_limit = 20;
    d->history_source = 0;
    l = pc_layer_new(w, h, fill == 2 ? "Layer 1" : "Background");
    if (!l) { free(d); return NULL; }
    if (fill != 2) {
        u32 c = fill == 0 ? 0xFFFFFF : rgb;
        size_t i, n = (size_t)w * h;
        u8 *p = l->pix->px;
        int r = pc_r(c), g = pc_g(c), b = pc_b(c);
        if (mode == PC_MODE_GRAY) r = g = b = pc_luma(r, g, b);
        for (i = 0; i < n; i++, p += 4) { p[0] = (u8)r; p[1] = (u8)g; p[2] = (u8)b; p[3] = 255; }
        l->is_background = 1;
        l->lock_position = 0;
    }
    d->s.layers[0] = l;
    d->s.nlayers = 1;
    d->s.active = 0;
    pc_hist_reset(d, "New");
    d->dirty = 0;
    return d;
}

static void state_free_contents(PcDocState *s)
{
    int i;
    for (i = 0; i < s->nlayers; i++) pc_layer_free(s->layers[i]);
    s->nlayers = 0;
    pc_buf_unref(s->sel); s->sel = NULL;
    for (i = 0; i < s->nchannels; i++) pc_buf_unref(s->channels[i].buf);
    s->nchannels = 0;
    for (i = 0; i < s->npaths; i++) pc_path_free_pts(&s->paths[i]);
    s->npaths = 0;
}

void pc_state_release(PcDocState *s) { state_free_contents(s); }

void pc_state_copy(PcDocState *dst, const PcDocState *src)
{
    int i;
    *dst = *src;
    for (i = 0; i < src->nlayers; i++) {
        dst->layers[i] = pc_layer_clone(src->layers[i]);
        if (!dst->layers[i]) { dst->nlayers = i; break; }
    }
    pc_buf_ref(dst->sel);
    for (i = 0; i < src->nchannels; i++) pc_buf_ref(dst->channels[i].buf);
    for (i = 0; i < src->npaths; i++) {
        dst->paths[i].pts = NULL; dst->paths[i].n = dst->paths[i].cap = 0;
        pc_path_copy(&dst->paths[i], &src->paths[i]);
    }
}

void pc_doc_free(PcDoc *d)
{
    int i;
    if (!d) return;
    state_free_contents(&d->s);
    for (i = 0; i < d->nhist; i++) {
        if (d->hist[i].state) { state_free_contents(d->hist[i].state); free(d->hist[i].state); }
    }
    free(d->hist);
    free(d);
}

PcLayer *pc_doc_active(PcDoc *d)
{
    if (d->s.active < 0 || d->s.active >= d->s.nlayers) return NULL;
    return d->s.layers[d->s.active];
}

void pc_doc_unique_layer_name(PcDoc *d, char *out, size_t n)
{
    int k, i, used;
    for (k = 1; k < 10000; k++) {
        char cand[32];
        pc_snprintf(cand, sizeof cand, "Layer %d", k);
        used = 0;
        for (i = 0; i < d->s.nlayers; i++) if (!strcmp(d->s.layers[i]->name, cand)) used = 1;
        if (!used) { pc_strlcpy(out, cand, n); return; }
    }
    pc_strlcpy(out, "Layer", n);
}

int pc_doc_insert_layer(PcDoc *d, int index, PcLayer *l)
{
    int i;
    if (!l || d->s.nlayers >= PC_MAX_LAYERS) { pc_layer_free(l); return -1; }
    if (index < 0) index = 0;
    if (index > d->s.nlayers) index = d->s.nlayers;
    /* Nothing may go below a Background layer. */
    if (index == 0 && d->s.nlayers > 0 && d->s.layers[0]->is_background) index = 1;
    for (i = d->s.nlayers; i > index; i--) d->s.layers[i] = d->s.layers[i - 1];
    d->s.layers[index] = l;
    d->s.nlayers++;
    d->s.active = index;
    pc_doc_touch(d);
    return 0;
}

PcLayer *pc_doc_add_layer(PcDoc *d, const char *name)
{
    char nm[64];
    PcLayer *l;
    if (!name) { pc_doc_unique_layer_name(d, nm, sizeof nm); name = nm; }
    l = pc_layer_new(d->s.w, d->s.h, name);
    if (!l) return NULL;
    if (pc_doc_insert_layer(d, d->s.active + 1, l)) return NULL;
    return l;
}

int pc_doc_delete_layer(PcDoc *d, int index)
{
    int i;
    if (index < 0 || index >= d->s.nlayers || d->s.nlayers <= 1) return -1;
    pc_layer_free(d->s.layers[index]);
    for (i = index; i < d->s.nlayers - 1; i++) d->s.layers[i] = d->s.layers[i + 1];
    d->s.nlayers--;
    if (d->s.active >= d->s.nlayers) d->s.active = d->s.nlayers - 1;
    else if (d->s.active > index) d->s.active--;
    pc_doc_touch(d);
    return 0;
}

int pc_doc_move_layer(PcDoc *d, int from, int to)
{
    PcLayer *l;
    int i;
    if (from < 0 || from >= d->s.nlayers || to < 0 || to >= d->s.nlayers || from == to) return -1;
    if (d->s.layers[from]->is_background) return -1;
    if (to == 0 && d->s.layers[0]->is_background) return -1;
    l = d->s.layers[from];
    if (from < to) for (i = from; i < to; i++) d->s.layers[i] = d->s.layers[i + 1];
    else for (i = from; i > to; i--) d->s.layers[i] = d->s.layers[i - 1];
    d->s.layers[to] = l;
    d->s.active = to;
    pc_doc_touch(d);
    return 0;
}

int pc_doc_duplicate_layer(PcDoc *d, int index)
{
    PcLayer *l;
    if (index < 0 || index >= d->s.nlayers) return -1;
    l = pc_layer_clone(d->s.layers[index]);
    if (!l) return -1;
    l->is_background = 0;
    if (strlen(l->name) + 6 < sizeof l->name) strcat(l->name, " copy");
    return pc_doc_insert_layer(d, index + 1, l);
}

/* Composite layers [from, to] (inclusive) into a fresh RGBA buffer. */
static PcBuf *composite_range(const PcDoc *d, int from, int to, int visible_only);

int pc_doc_merge_down(PcDoc *d, int index)
{
    PcBuf *m;
    PcLayer *below;
    if (index <= 0 || index >= d->s.nlayers) return -1;
    below = d->s.layers[index - 1];
    m = composite_range(d, index - 1, index, 0);
    if (!m) return -1;
    if (below->is_background) {
        /* Background stays opaque. */
        size_t i, n = (size_t)d->s.w * d->s.h;
        for (i = 0; i < n; i++) m->px[i * 4 + 3] = 255;
    }
    pc_buf_unref(below->pix);
    below->pix = m;
    if (below->mask) { pc_buf_unref(below->mask); below->mask = NULL; }
    below->opacity = 255; below->blend = PC_BLEND_NORMAL; below->kind = PC_LAYER_PIXEL;
    below->fx_enabled = 0;
    pc_doc_delete_layer(d, index);
    d->s.active = index - 1;
    pc_doc_touch(d);
    return 0;
}

int pc_doc_merge_visible(PcDoc *d)
{
    PcBuf *m;
    int i, first = -1;
    for (i = 0; i < d->s.nlayers; i++) if (d->s.layers[i]->visible) { first = i; break; }
    if (first < 0) return -1;
    m = composite_range(d, 0, d->s.nlayers - 1, 1);
    if (!m) return -1;
    for (i = d->s.nlayers - 1; i >= 0; i--) {
        if (i != first && d->s.layers[i]->visible) {
            pc_layer_free(d->s.layers[i]);
            memmove(&d->s.layers[i], &d->s.layers[i + 1], sizeof(PcLayer *) * (d->s.nlayers - 1 - i));
            d->s.nlayers--;
            if (i < first) first--;
        }
    }
    {
        PcLayer *l = d->s.layers[first];
        pc_buf_unref(l->pix); l->pix = m;
        pc_buf_unref(l->mask); l->mask = NULL;
        l->opacity = 255; l->blend = PC_BLEND_NORMAL; l->kind = PC_LAYER_PIXEL; l->fx_enabled = 0;
        if (l->is_background) {
            size_t k, n = (size_t)d->s.w * d->s.h;
            for (k = 0; k < n; k++) m->px[k * 4 + 3] = 255;
        }
    }
    d->s.active = first;
    pc_doc_touch(d);
    return 0;
}

int pc_doc_flatten(PcDoc *d)
{
    PcBuf *m = composite_range(d, 0, d->s.nlayers - 1, 1);
    PcLayer *l;
    size_t i, n;
    int k;
    if (!m) return -1;
    /* Flatten composites onto white, like Photoshop. */
    n = (size_t)d->s.w * d->s.h;
    for (i = 0; i < n; i++) {
        u8 *p = m->px + i * 4;
        int a = p[3];
        p[0] = (u8)(p[0] * a / 255 + 255 - a);
        p[1] = (u8)(p[1] * a / 255 + 255 - a);
        p[2] = (u8)(p[2] * a / 255 + 255 - a);
        p[3] = 255;
    }
    l = pc_layer_new(1, 1, "Background");
    if (!l) { pc_buf_unref(m); return -1; }
    pc_buf_unref(l->pix);
    l->pix = m;
    l->is_background = 1;
    for (k = 0; k < d->s.nlayers; k++) pc_layer_free(d->s.layers[k]);
    d->s.layers[0] = l;
    d->s.nlayers = 1;
    d->s.active = 0;
    pc_doc_touch(d);
    return 0;
}

int pc_doc_layer_from_background(PcDoc *d)
{
    PcLayer *l = d->s.nlayers ? d->s.layers[0] : NULL;
    if (!l || !l->is_background) return -1;
    l->is_background = 0;
    pc_strlcpy(l->name, "Layer 0", sizeof l->name);
    pc_doc_touch(d);
    return 0;
}

int pc_doc_add_mask(PcDoc *d, int index, int reveal_all, int from_selection)
{
    PcLayer *l;
    PcBuf *m;
    if (index < 0 || index >= d->s.nlayers) return -1;
    l = d->s.layers[index];
    if (l->is_background || l->mask) return -1;
    if (from_selection && d->s.sel) {
        m = pc_buf_clone(d->s.sel);
        if (!m) return -1;
        if (!reveal_all) {
            size_t i, n = (size_t)d->s.w * d->s.h;
            for (i = 0; i < n; i++) m->px[i] = (u8)(255 - m->px[i]);
        }
    } else {
        m = pc_buf_new(d->s.w, d->s.h, 1, 0);
        if (!m) return -1;
        memset(m->px, reveal_all ? 255 : 0, (size_t)d->s.w * d->s.h);
    }
    l->mask = m;
    l->mask_enabled = 1;
    pc_doc_touch(d);
    return 0;
}

int pc_doc_apply_mask(PcDoc *d, int index, int apply)
{
    PcLayer *l;
    if (index < 0 || index >= d->s.nlayers) return -1;
    l = d->s.layers[index];
    if (!l->mask) return -1;
    if (apply) {
        u8 *p = pc_layer_pixels_w(l);
        size_t i, n = (size_t)d->s.w * d->s.h;
        if (!p) return -1;
        for (i = 0; i < n; i++) p[i * 4 + 3] = (u8)pc_mul255(p[i * 4 + 3], l->mask->px[i]);
    }
    pc_buf_unref(l->mask);
    l->mask = NULL;
    pc_doc_touch(d);
    return 0;
}

/* ---------------- compositing helpers ---------------- */

static PcBuf *composite_range(const PcDoc *d, int from, int to, int visible_only)
{
    PcDoc tmp;
    PcBuf *out;
    int i, n = 0;
    memset(&tmp, 0, sizeof tmp);
    tmp.s.w = d->s.w; tmp.s.h = d->s.h; tmp.s.mode = d->s.mode;
    for (i = from; i <= to; i++) {
        PcLayer *l = d->s.layers[i];
        if (visible_only && !l->visible) continue;
        tmp.s.layers[n++] = l;
    }
    tmp.s.nlayers = n;
    out = pc_buf_new(d->s.w, d->s.h, 4, 1);
    if (!out) return NULL;
    /* Merging ignores the per-layer visibility only when asked to merge hidden ones too. */
    if (!visible_only) {
        int vis[PC_MAX_LAYERS];
        for (i = 0; i < n; i++) { vis[i] = tmp.s.layers[i]->visible; tmp.s.layers[i]->visible = 1; }
        pc_doc_composite_into(&tmp, out, NULL);
        for (i = 0; i < n; i++) tmp.s.layers[i]->visible = vis[i];
    } else {
        pc_doc_composite_into(&tmp, out, NULL);
    }
    return out;
}

PcBuf *pc_doc_composite(const PcDoc *d)
{
    PcBuf *out = pc_buf_new(d->s.w, d->s.h, 4, 1);
    if (out) pc_doc_composite_into(d, out, NULL);
    return out;
}

/* ---------------- selection ---------------- */

PcBuf *pc_sel_get(PcDoc *d, int create)
{
    if (!d->s.sel) {
        if (!create) return NULL;
        d->s.sel = pc_buf_new(d->s.w, d->s.h, 1, 1);
        return d->s.sel;
    }
    return pc_buf_writable(&d->s.sel);
}

void pc_sel_clear(PcDoc *d)
{
    pc_buf_unref(d->s.sel);
    d->s.sel = NULL;
    pc_doc_touch(d);
}

int pc_sel_bounds(const PcDoc *d, PcRect *r)
{
    int x, y, w = d->s.w, h = d->s.h;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    const u8 *m;
    if (!d->s.sel) return 0;
    m = d->s.sel->px;
    for (y = 0; y < h; y++) {
        const u8 *row = m + (size_t)y * w;
        int any = 0;
        for (x = 0; x < w; x++) {
            if (row[x]) {
                any = 1;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
            }
        }
        if (any) { if (y < y0) y0 = y; y1 = y; }
    }
    if (x1 < 0) return 0;
    r->x0 = x0; r->y0 = y0; r->x1 = x1 + 1; r->y1 = y1 + 1;
    return 1;
}

int pc_sel_has(const PcDoc *d)
{
    PcRect r;
    return pc_sel_bounds(d, &r);
}

/* ---------------- history ---------------- */

static void hist_free_entry(PcHistEntry *e)
{
    if (e->state) { state_free_contents(e->state); free(e->state); e->state = NULL; }
}

static PcDocState *state_dup(const PcDocState *s)
{
    PcDocState *c = (PcDocState *)malloc(sizeof *c);
    if (c) pc_state_copy(c, s);
    return c;
}

static int hist_grow(PcDoc *d)
{
    if (d->nhist >= d->hist_cap) {
        int nc = d->hist_cap ? d->hist_cap * 2 : 32;
        PcHistEntry *n = (PcHistEntry *)realloc(d->hist, sizeof *n * nc);
        if (!n) return -1;
        d->hist = n; d->hist_cap = nc;
    }
    return 0;
}

void pc_hist_reset(PcDoc *d, const char *label)
{
    int i;
    for (i = 0; i < d->nhist; i++) hist_free_entry(&d->hist[i]);
    d->nhist = 0; d->hist_cur = -1;
    if (hist_grow(d)) return;
    d->hist[0].state = state_dup(&d->s);
    d->hist[0].snapshot = 1;            /* the opening state is PS's automatic snapshot */
    pc_strlcpy(d->hist[0].label, label, sizeof d->hist[0].label);
    d->nhist = 1; d->hist_cur = 0;
    d->history_source = 0;
}

void pc_hist_commit(PcDoc *d, const char *label)
{
    int i, nonsnap;
    pc_doc_touch(d);
    /* Drop redo states (keep named snapshots, which live at the front). */
    for (i = d->hist_cur + 1; i < d->nhist; i++) hist_free_entry(&d->hist[i]);
    d->nhist = d->hist_cur + 1;
    if (hist_grow(d)) return;
    d->hist[d->nhist].state = state_dup(&d->s);
    d->hist[d->nhist].snapshot = 0;
    pc_strlcpy(d->hist[d->nhist].label, label, sizeof d->hist[d->nhist].label);
    d->nhist++;
    d->hist_cur = d->nhist - 1;
    /* Trim the oldest non-snapshot states beyond the limit. */
    for (;;) {
        int oldest = -1;
        nonsnap = 0;
        for (i = 0; i < d->nhist; i++) if (!d->hist[i].snapshot) { nonsnap++; if (oldest < 0) oldest = i; }
        if (nonsnap <= d->hist_limit || oldest < 0 || oldest == d->hist_cur) break;
        hist_free_entry(&d->hist[oldest]);
        memmove(&d->hist[oldest], &d->hist[oldest + 1], sizeof(PcHistEntry) * (d->nhist - oldest - 1));
        d->nhist--;
        d->hist_cur--;
        if (d->history_source == oldest) d->history_source = 0;
        else if (d->history_source > oldest) d->history_source--;
    }
}

int pc_hist_can_undo(const PcDoc *d) { return d->hist_cur > 0; }
int pc_hist_can_redo(const PcDoc *d) { return d->hist_cur + 1 < d->nhist; }

int pc_hist_goto(PcDoc *d, int index)
{
    if (index < 0 || index >= d->nhist || !d->hist[index].state) return -1;
    state_free_contents(&d->s);
    pc_state_copy(&d->s, d->hist[index].state);
    d->hist_cur = index;
    pc_doc_touch(d);
    return 0;
}

int pc_hist_undo(PcDoc *d) { return d->hist_cur > 0 ? pc_hist_goto(d, d->hist_cur - 1) : -1; }
int pc_hist_redo(PcDoc *d) { return pc_hist_can_redo(d) ? pc_hist_goto(d, d->hist_cur + 1) : -1; }

int pc_hist_snapshot(PcDoc *d, const char *name)
{
    int i, pos = 0;
    if (hist_grow(d)) return -1;
    /* Snapshots sit at the top of the list after existing snapshots. */
    while (pos < d->nhist && d->hist[pos].snapshot) pos++;
    memmove(&d->hist[pos + 1], &d->hist[pos], sizeof(PcHistEntry) * (d->nhist - pos));
    d->hist[pos].state = state_dup(&d->s);
    d->hist[pos].snapshot = 1;
    pc_strlcpy(d->hist[pos].label, name, sizeof d->hist[pos].label);
    d->nhist++;
    if (d->hist_cur >= pos) d->hist_cur++;
    if (d->history_source >= pos) d->history_source++;
    (void)i;
    return pos;
}

const PcDocState *pc_hist_state(const PcDoc *d, int index)
{
    if (index < 0 || index >= d->nhist) return NULL;
    return d->hist[index].state;
}

size_t pc_doc_memory(const PcDoc *d)
{
    size_t n = 0;
    int i;
    for (i = 0; i < d->s.nlayers; i++) {
        n += (size_t)d->s.w * d->s.h * 4;
        if (d->s.layers[i]->mask) n += (size_t)d->s.w * d->s.h;
    }
    return n;
}

/* ---------------- paths ---------------- */

void pc_path_free_pts(PcPath *p) { free(p->pts); p->pts = NULL; p->n = p->cap = 0; }

void pc_path_copy(PcPath *dst, const PcPath *src)
{
    char name[32];
    memcpy(name, src->name, sizeof name);
    dst->pts = NULL; dst->n = dst->cap = 0;
    memcpy(dst->name, name, sizeof name);
    if (src->n > 0) {
        dst->pts = (PcPathPt *)malloc(sizeof(PcPathPt) * src->n);
        if (dst->pts) { memcpy(dst->pts, src->pts, sizeof(PcPathPt) * src->n); dst->n = dst->cap = src->n; }
    }
}

void pc_path_add_point(PcPath *p, const PcPathPt *pt)
{
    if (p->n >= p->cap) {
        int nc = p->cap ? p->cap * 2 : 16;
        PcPathPt *np = (PcPathPt *)realloc(p->pts, sizeof *np * nc);
        if (!np) return;
        p->pts = np; p->cap = nc;
    }
    p->pts[p->n++] = *pt;
}

PcPath *pc_path_work(PcDoc *d)
{
    int i;
    if (d->s.active_path >= 0 && d->s.active_path < d->s.npaths) return &d->s.paths[d->s.active_path];
    for (i = 0; i < d->s.npaths; i++) if (!strcmp(d->s.paths[i].name, "Work Path")) { d->s.active_path = i; return &d->s.paths[i]; }
    if (d->s.npaths >= PC_MAX_PATHS) return NULL;
    i = d->s.npaths++;
    memset(&d->s.paths[i], 0, sizeof d->s.paths[i]);
    pc_strlcpy(d->s.paths[i].name, "Work Path", sizeof d->s.paths[i].name);
    d->s.active_path = i;
    return &d->s.paths[i];
}

static int emit(float *xy, u8 *starts, int n, int max, float x, float y, int st)
{
    if (n >= max) return n;
    xy[n * 2] = x; xy[n * 2 + 1] = y; starts[n] = (u8)st;
    return n + 1;
}

static int flatten_seg(float *xy, u8 *starts, int n, int max, const PcPathPt *a, const PcPathPt *b)
{
    int k, steps = 16;
    float dx = b->x - a->x, dy = b->y - a->y;
    int straight = a->ox == a->x && a->oy == a->y && b->ix == b->x && b->iy == b->y;
    if (straight) return emit(xy, starts, n, max, b->x, b->y, 0);
    {
        float len = (float)sqrt(dx * dx + dy * dy);
        steps = (int)(len / 4) + 8;
        if (steps > 64) steps = 64;
    }
    for (k = 1; k <= steps; k++) {
        float t = (float)k / steps, u = 1 - t;
        float x = u * u * u * a->x + 3 * u * u * t * a->ox + 3 * u * t * t * b->ix + t * t * t * b->x;
        float y = u * u * u * a->y + 3 * u * u * t * a->oy + 3 * u * t * t * b->iy + t * t * t * b->y;
        n = emit(xy, starts, n, max, x, y, 0);
    }
    return n;
}

int pc_path_flatten(const PcPath *p, float *xy, u8 *starts, int max_pts)
{
    int i = 0, n = 0;
    while (i < p->n) {
        int s = i, e = i;
        while (e + 1 < p->n && !p->pts[e + 1].start) e++;
        n = emit(xy, starts, n, max_pts, p->pts[s].x, p->pts[s].y, 1);
        for (i = s; i < e; i++) n = flatten_seg(xy, starts, n, max_pts, &p->pts[i], &p->pts[i + 1]);
        if (p->pts[s].closed && e > s) n = flatten_seg(xy, starts, n, max_pts, &p->pts[e], &p->pts[s]);
        i = e + 1;
    }
    return n;
}

/* ---------------- text ---------------- */

static PcTextRenderFn g_text_fn;
static void *g_text_user;

void pc_set_text_renderer(PcTextRenderFn fn, void *user) { g_text_fn = fn; g_text_user = user; }

int pc_text_render(const PcText *t, PcBuf *rgba)
{
    if (!g_text_fn) return -1;
    return g_text_fn(t, rgba, g_text_user);
}

int pc_layer_rerender_text(PcLayer *l, int w, int h)
{
    PcBuf *b;
    if (l->kind != PC_LAYER_TEXT) return -1;
    b = pc_buf_new(w, h, 4, 1);
    if (!b) return -1;
    if (pc_text_render(&l->text, b)) { pc_buf_unref(b); return -1; }
    pc_buf_unref(l->pix);
    l->pix = b;
    return 0;
}
