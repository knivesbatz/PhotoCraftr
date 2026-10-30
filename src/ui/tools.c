/*
 * Canvas tools: what each tool does with the mouse and keyboard, and the
 * overlays it draws (marquees, lasso lines, crop box, transform box,
 * paths, gradient and measure lines, brush-size cursor).
 */
#include "app.h"
#include "dlg.h"

/* ---------------- defaults ---------------- */

void tool_defaults(void)
{
    int t;
    for (t = 0; t < T_COUNT; t++) {
        ToolOpts *o = &g_app.topt[t];
        memset(o, 0, sizeof *o);
        o->brush.diameter = 13; o->brush.hardness = 0; o->brush.spacing = 25; o->brush.roundness = 100;
        o->opacity = 100; o->blend = PC_BLEND_NORMAL; o->antialias = 1; o->tolerance = 32; o->contiguous = 1;
        o->fixed_w = 64; o->fixed_h = 64; o->aligned = 1; o->lasso_width = 10; o->lasso_contrast = 10;
        o->lasso_freq = 57; o->pen_auto_add = 1; o->line_weight = 1; o->grad_dither = 1; o->grad_transparency = 1;
        o->font_size = 24; o->range = 1;
    }
    g_app.topt[T_PENCIL].brush.diameter = 1; g_app.topt[T_PENCIL].brush.hardness = 100;
    g_app.topt[T_AIRBRUSH].opacity = 50; g_app.topt[T_BLUR].opacity = 50; g_app.topt[T_SHARPEN].opacity = 50;
    g_app.topt[T_SMUDGE].opacity = 50; g_app.topt[T_DODGE].opacity = 50; g_app.topt[T_BURN].opacity = 50;
    g_app.topt[T_SPONGE].opacity = 50; g_app.topt[T_SPONGE].range = 0;
    g_app.topt[T_PEN_FREEFORM].tolerance = 2;
    for (t = 0; t < 32; t++) g_app.group_tool[t] = -1;
    for (t = T_COUNT - 1; t >= 0; t--) g_app.group_tool[g_tool_info[t].group] = t;
}

int tool_is_painting(int t)
{
    return (t >= T_BRUSH && t <= T_ERASER) || (t >= T_BLUR && t <= T_SPONGE) || t == T_GRADIENT || t == T_BUCKET || t == T_SHAPE_RECT || t == T_SHAPE_ELLIPSE || t == T_SHAPE_LINE;
}

static int brush_tool(int t) { return (t >= T_BRUSH && t <= T_ERASER) || (t >= T_BLUR && t <= T_SPONGE); }

static int paint_kind(int t)
{
    switch (t) {
    case T_BRUSH: return PC_PAINT_BRUSH; case T_AIRBRUSH: return PC_PAINT_AIRBRUSH; case T_PENCIL: return PC_PAINT_PENCIL;
    case T_ERASER: return PC_PAINT_ERASER; case T_STAMP: return PC_PAINT_CLONE; case T_PATTERN_STAMP: return PC_PAINT_PATTERN;
    case T_HISTORY_BRUSH: return PC_PAINT_HISTORY; case T_BLUR: return PC_PAINT_BLUR; case T_SHARPEN: return PC_PAINT_SHARPEN;
    case T_SMUDGE: return PC_PAINT_SMUDGE; case T_DODGE: return PC_PAINT_DODGE; case T_BURN: return PC_PAINT_BURN;
    default: return PC_PAINT_SPONGE;
    }
}

static int effective_tool(DocView *v)
{
    if (v->temp_tool >= 0) return v->temp_tool;
    return g_app.tool;
}

/* ---------------- shared helpers ---------------- */

static void save_state(DocView *v)
{
    if (v->has_saved) pc_state_release(&v->saved);
    pc_state_copy(&v->saved, &v->doc->s);
    v->has_saved = 1;
}

static void restore_state(DocView *v)
{
    if (!v->has_saved) return;
    pc_state_release(&v->doc->s);
    pc_state_copy(&v->doc->s, &v->saved);
    pc_doc_touch(v->doc);
}

static void drop_state(DocView *v)
{
    if (v->has_saved) { pc_state_release(&v->saved); v->has_saved = 0; }
}

static void poly_add(DocView *v, float x, float y)
{
    if (v->poly_n >= v->poly_cap) {
        int nc = v->poly_cap ? v->poly_cap * 2 : 256;
        float *n = (float *)realloc(v->poly, sizeof(float) * 2 * (size_t)nc);
        if (!n) return;
        v->poly = n; v->poly_cap = nc;
    }
    v->poly[v->poly_n * 2] = x; v->poly[v->poly_n * 2 + 1] = y;
    v->poly_n++;
}

static int sel_op_from_keys(int opt_op)
{
    int sh = GetKeyState(VK_SHIFT) < 0, al = GetKeyState(VK_MENU) < 0;
    if (sh && al) return PC_SEL_INTERSECT;
    if (sh) return PC_SEL_ADD;
    if (al) return PC_SEL_SUBTRACT;
    return opt_op;
}

static void refresh_after_edit(DocView *v, const PcRect *dirty)
{
    view_invalidate(v, dirty);
    UpdateWindow(v->hwnd);
}

static PcPaintOpts paint_opts(DocView *v, int t)
{
    PcPaintOpts o;
    ToolOpts *to = &g_app.topt[t];
    memset(&o, 0, sizeof o);
    o.tool = paint_kind(t);
    o.brush = to->brush;
    if (o.brush.spacing <= 0) o.brush.spacing = 25;
    if (o.brush.roundness <= 0) o.brush.roundness = 100;
    o.opacity = to->opacity;
    o.blend = to->blend;
    o.color = g_app.fg;
    o.bg = g_app.bg;
    o.fade_steps = to->fade;
    o.eraser_mode = to->eraser_mode;
    o.range = to->range;
    o.aligned = to->aligned;
    o.sample_merged = to->sample_merged;
    o.finger = to->finger;
    o.erase_to_history = to->erase_to_history;
    if (t == T_PENCIL && to->auto_erase) {
        /* Auto Erase: starting on the foreground colour paints the background colour. */
        u32 c = app_sample_color(v, (int)v->doc_x, (int)v->doc_y, 0, 0);
        if (c == g_app.fg) o.color = g_app.bg;
    }
    if (g_app.quickmask) o.target_mask = 2;
    else if (v->edit_channel >= 100) { o.target_mask = 3; o.channel = v->edit_channel - 100; }
    else if (panels_mask_target()) o.target_mask = 1;
    return o;
}

/* Marquee drag rectangle in doc space honouring Shift (square) / Alt (centre) / Style. */
static void marquee_rect(DocView *v, int t, float *x0, float *y0, float *x1, float *y1)
{
    ToolOpts *o = &g_app.topt[t];
    float sx = v->doc_start_x, sy = v->doc_start_y, ex = v->doc_x, ey = v->doc_y;
    float w = ex - sx, h = ey - sy;
    int square = GetKeyState(VK_SHIFT) < 0 && !pc_sel_has(v->doc);
    if (o->style == 2) { w = (float)(w < 0 ? -o->fixed_w : o->fixed_w); h = (float)(h < 0 ? -o->fixed_h : o->fixed_h); if (!v->dragging) { w = (float)o->fixed_w; h = (float)o->fixed_h; } }
    else if (o->style == 1 && o->fixed_h > 0) { float a = (float)o->fixed_w / o->fixed_h; h = (h < 0 ? -1 : 1) * fabsf(w) / a; }
    else if (square) { float m = PC_MAX(fabsf(w), fabsf(h)); w = w < 0 ? -m : m; h = h < 0 ? -m : m; }
    if (GetKeyState(VK_MENU) < 0 && v->dragging && o->style != 2) { *x0 = sx - w; *y0 = sy - h; *x1 = sx + w; *y1 = sy + h; }
    else { *x0 = sx; *y0 = sy; *x1 = sx + w; *y1 = sy + h; }
    if (*x0 > *x1) { float t2 = *x0; *x0 = *x1; *x1 = t2; }
    if (*y0 > *y1) { float t2 = *y0; *y0 = *y1; *y1 = t2; }
    *x0 = (float)floor(*x0 + 0.5f); *y0 = (float)floor(*y0 + 0.5f); *x1 = (float)floor(*x1 + 0.5f); *y1 = (float)floor(*y1 + 0.5f);
}

/* Strongest edge near (x,y) within r pixels: magnetic lasso / pen. */
static void snap_edge(DocView *v, float *x, float *y, int r)
{
    PcDoc *d = v->doc;
    int cx = (int)*x, cy = (int)*y, i, j, best = -1, bx = cx, by = cy;
    if (!v->comp || v->comp_rev != d->revision) { pc_buf_unref(v->comp); v->comp = pc_doc_composite(d); v->comp_rev = d->revision; }
    if (!v->comp) return;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++) {
            int px = cx + i, py = cy + j, g;
            const u8 *a, *b, *c, *e;
            if (px < 1 || py < 1 || px >= d->s.w - 1 || py >= d->s.h - 1 || i * i + j * j > r * r) continue;
            a = v->comp->px + ((size_t)py * d->s.w + px - 1) * 4; b = v->comp->px + ((size_t)py * d->s.w + px + 1) * 4;
            c = v->comp->px + ((size_t)(py - 1) * d->s.w + px) * 4; e = v->comp->px + ((size_t)(py + 1) * d->s.w + px) * 4;
            g = abs(pc_luma(a[0], a[1], a[2]) - pc_luma(b[0], b[1], b[2])) + abs(pc_luma(c[0], c[1], c[2]) - pc_luma(e[0], e[1], e[2]));
            g = g * 16 - (i * i + j * j);   /* prefer near points on ties */
            if (g > best) { best = g; bx = px; by = py; }
        }
    if (best > 16 * g_app.topt[T_LASSO_MAGNETIC].lasso_contrast / 4) { *x = (float)bx; *y = (float)by; }
}

/* Douglas-Peucker simplification for the freeform pen. */
static int simplify(const float *pts, int n, float tol, float *out)
{
    unsigned char *keep;
    int *stack, sp = 0, i, m = 0;
    if (n <= 2) { memcpy(out, pts, sizeof(float) * 2 * (size_t)n); return n; }
    keep = (unsigned char *)calloc((size_t)n, 1);
    stack = (int *)malloc(sizeof(int) * 2 * (size_t)n);
    if (!keep || !stack) { free(keep); free(stack); memcpy(out, pts, sizeof(float) * 2 * (size_t)n); return n; }
    keep[0] = keep[n - 1] = 1;
    stack[sp++] = 0; stack[sp++] = n - 1;
    while (sp) {
        int b = stack[--sp], a = stack[--sp], idx = -1;
        float dmax = 0, ax = pts[a * 2], ay = pts[a * 2 + 1], bx = pts[b * 2], by = pts[b * 2 + 1];
        float len = (float)sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        for (i = a + 1; i < b; i++) {
            float dd = len > 0 ? fabsf((bx - ax) * (ay - pts[i * 2 + 1]) - (ax - pts[i * 2]) * (by - ay)) / len
                               : (float)sqrt((pts[i * 2] - ax) * (pts[i * 2] - ax) + (pts[i * 2 + 1] - ay) * (pts[i * 2 + 1] - ay));
            if (dd > dmax) { dmax = dd; idx = i; }
        }
        if (idx >= 0 && dmax > tol) { keep[idx] = 1; stack[sp++] = a; stack[sp++] = idx; stack[sp++] = idx; stack[sp++] = b; }
    }
    for (i = 0; i < n; i++) if (keep[i]) { out[m * 2] = pts[i * 2]; out[m * 2 + 1] = pts[i * 2 + 1]; m++; }
    free(keep); free(stack);
    return m;
}

/* ---------------- path helpers ---------------- */

static PcPath *active_path(PcDoc *d)
{
    if (d->s.active_path >= 0 && d->s.active_path < d->s.npaths) return &d->s.paths[d->s.active_path];
    return NULL;
}

static int find_anchor(DocView *v, PcPath *p, int x, int y, int *handle)
{
    int i;
    for (i = p ? p->n - 1 : -1; i >= 0; i--) {
        int ax, ay;
        PcPathPt *q = &p->pts[i];
        view_doc_to_screen(v, q->x, q->y, &ax, &ay);
        if (abs(ax - x) <= 4 && abs(ay - y) <= 4) { *handle = 0; return i; }
        view_doc_to_screen(v, q->ix, q->iy, &ax, &ay);
        if ((q->ix != q->x || q->iy != q->y) && abs(ax - x) <= 4 && abs(ay - y) <= 4) { *handle = 1; return i; }
        view_doc_to_screen(v, q->ox, q->oy, &ax, &ay);
        if ((q->ox != q->x || q->oy != q->y) && abs(ax - x) <= 4 && abs(ay - y) <= 4) { *handle = 2; return i; }
    }
    return -1;
}

static void commit_text_layer(DocView *v, float x, float y, int vertical, int mask)
{
    PcDoc *d = v->doc;
    PcText t;
    memset(&t, 0, sizeof t);
    options_get_type(&t);
    t.x = (int)x; t.y = (int)y; t.vertical = vertical;
    if (!dlg_type(d, &t, 1)) return;
    if (!t.text[0]) return;
    if (mask) {
        /* Type Mask: render to a coverage buffer and make it the selection. */
        PcBuf *b = pc_buf_new(d->s.w, d->s.h, 4, 1);
        if (b && !pc_text_render(&t, b)) {
            u8 *m = (u8 *)malloc((size_t)d->s.w * d->s.h);
            if (m) {
                size_t i, n = (size_t)d->s.w * d->s.h;
                for (i = 0; i < n; i++) m[i] = b->px[i * 4 + 3];
                pc_sel_combine(d, m, PC_SEL_REPLACE);
                free(m);
                app_commit(d, "Type Mask");
            }
        }
        pc_buf_unref(b);
        return;
    }
    {
        PcLayer *l;
        char name[64];
        int k;
        for (k = 0; t.text[k] && k < 40 && t.text[k] != '\n' && t.text[k] != '\r'; k++) name[k] = t.text[k];
        name[k] = 0;
        l = pc_doc_add_layer(d, name[0] ? name : "Type");
        if (!l) return;
        l->kind = PC_LAYER_TEXT;
        l->text = t;
        if (pc_layer_rerender_text(l, d->s.w, d->s.h)) app_error("The font \"%s\" could not be rendered.", t.font);
        app_commit(d, "Type Tool");
    }
}

static void edit_text_layer(DocView *v, PcLayer *l)
{
    PcText t = l->text;
    if (!dlg_type(v->doc, &t, 0)) return;
    l->text = t;
    pc_layer_rerender_text(l, v->doc->s.w, v->doc->s.h);
    app_commit(v->doc, "Edit Type Layer");
}

/* ---------------- free transform ---------------- */

void tool_begin_free_transform(DocView *v)
{
    PcDoc *d = v->doc;
    PcLayer *l = pc_doc_active(d);
    PcRect r;
    if (!l) return;
    if (l->is_background && !pc_sel_has(d)) { app_error("Could not complete the Free Transform command because the Background layer is locked. Double-click it to make it a normal layer."); return; }
    if (!pc_sel_bounds(d, &r) && !pc_layer_bounds(l, d->s.w, d->s.h, &r)) return;
    tool_commit(v);
    save_state(v);
    v->xf_active = 1;
    v->xf_src = r;
    v->xf_quad[0] = (float)r.x0; v->xf_quad[1] = (float)r.y0; v->xf_quad[2] = (float)r.x1; v->xf_quad[3] = (float)r.y0;
    v->xf_quad[4] = (float)r.x1; v->xf_quad[5] = (float)r.y1; v->xf_quad[6] = (float)r.x0; v->xf_quad[7] = (float)r.y1;
    memcpy(v->xf_orig, v->xf_quad, sizeof v->xf_quad);
    v->xf_handle = -1;
    view_invalidate(v, NULL);
}

static void xf_preview(DocView *v)
{
    restore_state(v);
    pc_layer_transform_quad(v->doc, &v->xf_src, v->xf_quad, PC_RESAMPLE_BILINEAR);
    view_invalidate(v, NULL);
}

static int xf_hit(DocView *v, int x, int y)
{
    /* 0..3 corners, 4..7 edge midpoints, 8 inside (move), 9 outside (rotate). */
    int k, sx, sy;
    float cx = 0, cy = 0;
    for (k = 0; k < 4; k++) {
        view_doc_to_screen(v, v->xf_quad[k * 2], v->xf_quad[k * 2 + 1], &sx, &sy);
        if (abs(sx - x) <= 4 && abs(sy - y) <= 4) return k;
    }
    for (k = 0; k < 4; k++) {
        int j = (k + 1) % 4;
        view_doc_to_screen(v, (v->xf_quad[k * 2] + v->xf_quad[j * 2]) / 2, (v->xf_quad[k * 2 + 1] + v->xf_quad[j * 2 + 1]) / 2, &sx, &sy);
        if (abs(sx - x) <= 4 && abs(sy - y) <= 4) return 4 + k;
    }
    {
        /* inside test by winding */
        float px, py;
        int inside = 0, i, j;
        view_screen_to_doc(v, x, y, &px, &py);
        for (i = 0, j = 3; i < 4; j = i++) {
            float xi = v->xf_quad[i * 2], yi = v->xf_quad[i * 2 + 1], xj = v->xf_quad[j * 2], yj = v->xf_quad[j * 2 + 1];
            if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) inside = !inside;
        }
        for (k = 0; k < 4; k++) { cx += v->xf_quad[k * 2] / 4; cy += v->xf_quad[k * 2 + 1] / 4; }
        (void)cx; (void)cy;
        return inside ? 8 : 9;
    }
}

static void xf_drag(DocView *v, float dx, float dy)
{
    int h = v->xf_handle, k;
    float *q = v->xf_quad;
    const float *o = v->xf_orig;
    float cx = (o[0] + o[2] + o[4] + o[6]) / 4, cy = (o[1] + o[3] + o[5] + o[7]) / 4;
    int ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
    memcpy(q, o, sizeof v->xf_quad);
    if (h == 8) { for (k = 0; k < 4; k++) { q[k * 2] += dx; q[k * 2 + 1] += dy; } return; }
    if (h == 9) {
        double a0 = atan2(v->doc_start_y - cy, v->doc_start_x - cx), a1 = atan2(v->doc_start_y + dy - cy, v->doc_start_x + dx - cx);
        double a = a1 - a0, ca, sa;
        if (shift) a = floor(a / (M_PI / 12) + 0.5) * (M_PI / 12);
        ca = cos(a); sa = sin(a);
        for (k = 0; k < 4; k++) {
            float x = o[k * 2] - cx, y = o[k * 2 + 1] - cy;
            q[k * 2] = cx + (float)(x * ca - y * sa); q[k * 2 + 1] = cy + (float)(x * sa + y * ca);
        }
        return;
    }
    if (h >= 0 && h < 4) {
        if (ctrl) { q[h * 2] += dx; q[h * 2 + 1] += dy; return; }   /* distort: free corner */
        {
            /* scale from the opposite corner (axis-aligned original box) */
            int opp = (h + 2) % 4;
            float ox = o[opp * 2], oy = o[opp * 2 + 1];
            float nx = o[h * 2] + dx, ny = o[h * 2 + 1] + dy;
            float sx = (o[h * 2] - ox) != 0 ? (nx - ox) / (o[h * 2] - ox) : 1, sy = (o[h * 2 + 1] - oy) != 0 ? (ny - oy) / (o[h * 2 + 1] - oy) : 1;
            if (shift) { float s = fabsf(sx) > fabsf(sy) ? sx : sy; sx = sy = s; }
            for (k = 0; k < 4; k++) { q[k * 2] = ox + (o[k * 2] - ox) * sx; q[k * 2 + 1] = oy + (o[k * 2 + 1] - oy) * sy; }
        }
        return;
    }
    if (h >= 4 && h < 8) {
        int a = h - 4, b = (a + 1) % 4;
        if (ctrl) { q[a * 2] += dx; q[a * 2 + 1] += dy; q[b * 2] += dx; q[b * 2 + 1] += dy; return; }   /* skew */
        {
            /* scale one axis: move the edge along its normal */
            int horiz = (a == 0 || a == 2);
            for (k = 0; k < 2; k++) {
                int idx = k ? b : a;
                if (horiz) q[idx * 2 + 1] += dy; else q[idx * 2] += dx;
            }
        }
    }
}

/* ---------------- crop ---------------- */

static int crop_hit(DocView *v, int x, int y)
{
    int sx0, sy0, sx1, sy1, k;
    view_doc_to_screen(v, v->crop[0], v->crop[1], &sx0, &sy0);
    view_doc_to_screen(v, v->crop[2], v->crop[3], &sx1, &sy1);
    {
        int hx[8] = { sx0, (sx0 + sx1) / 2, sx1, sx1, sx1, (sx0 + sx1) / 2, sx0, sx0 };
        int hy[8] = { sy0, sy0, sy0, (sy0 + sy1) / 2, sy1, sy1, sy1, (sy0 + sy1) / 2 };
        for (k = 0; k < 8; k++) if (abs(hx[k] - x) <= 4 && abs(hy[k] - y) <= 4) return k;
    }
    if (x > sx0 && x < sx1 && y > sy0 && y < sy1) return 8;
    return -1;
}

static void crop_commit(DocView *v)
{
    PcRect r;
    if (!v->crop_active) return;
    r.x0 = (int)floor(PC_MIN(v->crop[0], v->crop[2]) + 0.5f); r.y0 = (int)floor(PC_MIN(v->crop[1], v->crop[3]) + 0.5f);
    r.x1 = (int)floor(PC_MAX(v->crop[0], v->crop[2]) + 0.5f); r.y1 = (int)floor(PC_MAX(v->crop[1], v->crop[3]) + 0.5f);
    v->crop_active = 0;
    if (r.x1 - r.x0 >= 1 && r.y1 - r.y0 >= 1 && !pc_image_crop(v->doc, &r)) {
        app_commit(v->doc, "Crop");
        view_zoom_fit(v, 0);
    }
    view_invalidate(v, NULL);
}

/* ---------------- commit / cancel ---------------- */

void tool_commit(DocView *v)
{
    if (!v) return;
    if (v->stroke) { pc_stroke_end(v->stroke); v->stroke = NULL; app_commit(v->doc, g_tool_info[g_app.tool].name); }
    if (v->xf_active) {
        v->xf_active = 0;
        if (memcmp(v->xf_quad, v->xf_orig, sizeof v->xf_quad)) {
            xf_preview(v);
            drop_state(v);
            app_commit(v->doc, "Free Transform");
        } else {
            restore_state(v);
            drop_state(v);
        }
    }
    if (v->crop_active) crop_commit(v);
    if (v->poly_n && (g_app.tool == T_LASSO_POLY || g_app.tool == T_LASSO_MAGNETIC)) {
        if (v->poly_n >= 3 && !pc_sel_polygon(v->doc, v->poly, v->poly_n, sel_op_from_keys(g_app.topt[g_app.tool].sel_op),
                                            g_app.topt[g_app.tool].antialias, (float)g_app.topt[g_app.tool].feather))
            app_commit(v->doc, g_tool_info[g_app.tool].name);
        v->poly_n = 0;
    }
    v->measure_active = 0;
    view_invalidate(v, NULL);
}

void tool_cancel(DocView *v)
{
    if (!v) return;
    if (v->stroke) { pc_stroke_end(v->stroke); v->stroke = NULL; restore_state(v); }
    if (v->xf_active) { v->xf_active = 0; restore_state(v); }
    if (v->dragging && v->has_saved) restore_state(v);
    drop_state(v);
    v->crop_active = 0;
    v->poly_n = 0;
    v->dragging = 0;
    if (GetCapture() == v->hwnd) ReleaseCapture();
    view_invalidate(v, NULL);
}

/* ---------------- mouse ---------------- */

static int g_last_paint_valid;
static float g_last_paint_x, g_last_paint_y;

void tool_mouse(DocView *v, UINT msg, int x, int y, WPARAM wp)
{
    PcDoc *d = v->doc;
    int t = effective_tool(v);
    ToolOpts *o = &g_app.topt[t];
    float dx, dy;
    view_screen_to_doc(v, x, y, &dx, &dy);
    v->doc_x = dx; v->doc_y = dy;

    /* Alt temporarily turns painting tools into the eyedropper (PS5). */
    if (msg == WM_LBUTTONDOWN && GetKeyState(VK_MENU) < 0 && (t == T_BRUSH || t == T_AIRBRUSH || t == T_PENCIL || t == T_GRADIENT || t == T_BUCKET))
        t = T_EYEDROPPER;
    /* Ctrl temporarily selects the Move tool (except with path / transform tools). */
    if (msg == WM_LBUTTONDOWN && GetKeyState(VK_CONTROL) < 0 && t != T_ZOOM && t != T_HAND && t < T_PEN && !v->xf_active && t != T_MOVE && t != T_CROP)
        t = T_MOVE, v->temp_tool = T_MOVE;

    if (v->xf_active) {
        if (msg == WM_LBUTTONDOWN) { v->xf_handle = xf_hit(v, x, y); v->doc_start_x = dx; v->doc_start_y = dy; memcpy(v->xf_orig, v->xf_quad, sizeof v->xf_quad); v->dragging = 1; SetCapture(v->hwnd); }
        else if (msg == WM_LBUTTONDBLCLK) tool_commit(v);
        else if (msg == WM_MOUSEMOVE && v->dragging) { xf_drag(v, dx - v->doc_start_x, dy - v->doc_start_y); view_invalidate(v, NULL); }
        else if (msg == WM_LBUTTONUP && v->dragging) { v->dragging = 0; ReleaseCapture(); xf_preview(v); }
        return;
    }

    switch (t) {
    case T_HAND:
        if (msg == WM_LBUTTONDBLCLK) { view_zoom_fit(v, 0); return; }
        if (msg == WM_LBUTTONDOWN) { v->dragging = 1; v->drag_start.x = x; v->drag_start.y = y; v->drag_last.x = v->sx; v->drag_last.y = v->sy; SetCapture(v->hwnd); SetCursor(w95_cursor(PC_CURSOR_GRAB)); }
        else if (msg == WM_MOUSEMOVE && v->dragging) view_scroll_to(v, v->drag_last.x - (x - v->drag_start.x), v->drag_last.y - (y - v->drag_start.y));
        else if (msg == WM_LBUTTONUP && v->dragging) { v->dragging = 0; ReleaseCapture(); }
        return;
    case T_ZOOM:
        if (msg == WM_LBUTTONDBLCLK) { view_set_zoom(v, 1.0, -1, -1); return; }
        if (msg == WM_LBUTTONDOWN) { v->dragging = 1; v->drag_start.x = x; v->drag_start.y = y; v->drag_last = v->drag_start; SetCapture(v->hwnd); }
        else if (msg == WM_MOUSEMOVE && v->dragging) { v->drag_last.x = x; v->drag_last.y = y; InvalidateRect(v->hwnd, NULL, FALSE); }
        else if (msg == WM_LBUTTONUP && v->dragging) {
            int out = GetKeyState(VK_MENU) < 0, i;
            v->dragging = 0; ReleaseCapture();
            if (!out && abs(x - v->drag_start.x) > 4 && abs(y - v->drag_start.y) > 4) {
                RECT cr;
                float a, b, c, e;
                view_canvas_rect(v, &cr);
                view_screen_to_doc(v, PC_MIN(x, v->drag_start.x), PC_MIN(y, v->drag_start.y), &a, &b);
                view_screen_to_doc(v, PC_MAX(x, v->drag_start.x), PC_MAX(y, v->drag_start.y), &c, &e);
                {
                    double z = PC_MIN((cr.right - cr.left) / PC_MAX(1.0, c - a), (cr.bottom - cr.top) / PC_MAX(1.0, e - b));
                    view_set_zoom(v, z, -1, -1);
                    view_scroll_to(v, (int)(a * v->zoom), (int)(b * v->zoom));
                }
            } else {
                for (i = 0; i < g_zoom_nsteps; i++) if (g_zoom_steps[i] > v->zoom + 1e-6) break;
                if (out) { for (i = g_zoom_nsteps - 1; i >= 0; i--) if (g_zoom_steps[i] < v->zoom - 1e-6) break; }
                if (i >= 0 && i < g_zoom_nsteps) view_set_zoom(v, g_zoom_steps[i], x, y);
                else if (!out) MessageBeep(MB_OK);
            }
            InvalidateRect(v->hwnd, NULL, FALSE);
        }
        return;
    case T_EYEDROPPER:
        if (msg == WM_LBUTTONDOWN || (msg == WM_MOUSEMOVE && (wp & MK_LBUTTON))) {
            u32 c = app_sample_color(v, (int)dx, (int)dy, g_app.topt[T_EYEDROPPER].sample_size, 1);
            if (GetKeyState(VK_MENU) < 0 && g_app.tool == T_EYEDROPPER) app_set_colors(g_app.fg, c);
            else app_set_colors(c, g_app.bg);
        }
        return;
    case T_MEASURE:
        if (msg == WM_LBUTTONDOWN) { v->measure_active = 1; v->measure[0] = v->measure[2] = dx; v->measure[1] = v->measure[3] = dy; v->dragging = 1; SetCapture(v->hwnd); }
        else if (msg == WM_MOUSEMOVE && v->dragging) {
            v->measure[2] = dx; v->measure[3] = dy;
            if (GetKeyState(VK_SHIFT) < 0) {
                /* constrain to 45 degrees */
                float mx = dx - v->measure[0], my = dy - v->measure[1];
                double a = floor(atan2(my, mx) / (M_PI / 4) + 0.5) * (M_PI / 4), len = sqrt(mx * mx + my * my);
                v->measure[2] = v->measure[0] + (float)(cos(a) * len); v->measure[3] = v->measure[1] + (float)(sin(a) * len);
            }
            app_update_info(v, dx, dy);
            InvalidateRect(v->hwnd, NULL, FALSE);
        } else if (msg == WM_LBUTTONUP && v->dragging) { v->dragging = 0; ReleaseCapture(); }
        return;
    case T_MOVE: {
        PcLayer *l;
        if (msg == WM_LBUTTONDOWN) {
            if (options_auto_select() || (GetKeyState(VK_CONTROL) < 0 && g_app.tool == T_MOVE)) {
                /* Pick the topmost layer with a visible pixel under the cursor. */
                int li;
                for (li = d->s.nlayers - 1; li >= 0; li--) {
                    PcLayer *q = d->s.layers[li];
                    if (q->visible && q->pix && dx >= 0 && dy >= 0 && dx < d->s.w && dy < d->s.h && q->pix->px[((size_t)(int)dy * d->s.w + (int)dx) * 4 + 3] > 20) { d->s.active = li; break; }
                }
                app_refresh(R_LAYERS);
            }
            l = pc_doc_active(d);
            if (!l) return;
            if (l->is_background && !pc_sel_has(d)) { app_error("Could not use the move tool because the layer is locked."); return; }
            save_state(v);
            v->dragging = 1; v->doc_start_x = dx; v->doc_start_y = dy;
            SetCapture(v->hwnd);
        } else if (msg == WM_MOUSEMOVE && v->dragging) {
            int ox = (int)floor(dx - v->doc_start_x + 0.5f), oy = (int)floor(dy - v->doc_start_y + 0.5f);
            if (GetKeyState(VK_SHIFT) < 0) { if (abs(ox) > abs(oy)) oy = 0; else ox = 0; }
            restore_state(v);
            if (pc_sel_has(d)) {
                PcRect r;
                float q[8];
                pc_sel_bounds(d, &r);
                q[0] = (float)(r.x0 + ox); q[1] = (float)(r.y0 + oy); q[2] = (float)(r.x1 + ox); q[3] = (float)(r.y0 + oy);
                q[4] = (float)(r.x1 + ox); q[5] = (float)(r.y1 + oy); q[6] = (float)(r.x0 + ox); q[7] = (float)(r.y1 + oy);
                pc_layer_transform_quad(d, &r, q, PC_RESAMPLE_NEAREST);
            } else {
                pc_layer_offset(d, ox, oy);
            }
            refresh_after_edit(v, NULL);
        } else if (msg == WM_LBUTTONUP && v->dragging) {
            v->dragging = 0;
            ReleaseCapture();
            drop_state(v);
            if (fabsf(dx - v->doc_start_x) >= 0.5f || fabsf(dy - v->doc_start_y) >= 0.5f) app_commit(d, "Move");
            v->temp_tool = -1;
        }
        return;
    }
    case T_MARQUEE_RECT: case T_MARQUEE_ELLIPSE: case T_MARQUEE_ROW: case T_MARQUEE_COL:
        if (msg == WM_LBUTTONDOWN) {
            v->doc_start_x = (float)floor(dx + 0.5f); v->doc_start_y = (float)floor(dy + 0.5f);
            v->dragging = 1;
            SetCapture(v->hwnd);
            if (t == T_MARQUEE_ROW || t == T_MARQUEE_COL || o->style == 2) {
                int op = sel_op_from_keys(o->sel_op);
                v->dragging = 0; ReleaseCapture();
                if (t == T_MARQUEE_ROW) pc_sel_rect(d, 0, (int)dy, d->s.w, (int)dy + 1, op, 0);
                else if (t == T_MARQUEE_COL) pc_sel_rect(d, (int)dx, 0, (int)dx + 1, d->s.h, op, 0);
                else if (t == T_MARQUEE_ELLIPSE) pc_sel_ellipse(d, (int)dx, (int)dy, (int)dx + o->fixed_w, (int)dy + o->fixed_h, op, o->antialias, (float)o->feather);
                else pc_sel_rect(d, (int)dx, (int)dy, (int)dx + o->fixed_w, (int)dy + o->fixed_h, op, (float)o->feather);
                app_commit(d, g_tool_info[t].name);
            }
        } else if (msg == WM_MOUSEMOVE && v->dragging) {
            app_update_info(v, dx, dy);
            InvalidateRect(v->hwnd, NULL, FALSE);
        } else if (msg == WM_LBUTTONUP && v->dragging) {
            float x0, y0, x1, y1;
            int op = sel_op_from_keys(o->sel_op);
            v->dragging = 0; ReleaseCapture();
            marquee_rect(v, t, &x0, &y0, &x1, &y1);
            if (x1 - x0 < 1 || y1 - y0 < 1) {
                if (op == PC_SEL_REPLACE && pc_sel_has(d)) { pc_sel_clear(d); app_commit(d, "Deselect"); }
            } else {
                if (t == T_MARQUEE_ELLIPSE) pc_sel_ellipse(d, (int)x0, (int)y0, (int)x1, (int)y1, op, o->antialias, (float)o->feather);
                else pc_sel_rect(d, (int)x0, (int)y0, (int)x1, (int)y1, op, (float)o->feather);
                app_commit(d, g_tool_info[t].name);
            }
            view_invalidate(v, NULL);
        }
        return;
    case T_LASSO:
        if (msg == WM_LBUTTONDOWN) { v->poly_n = 0; poly_add(v, dx, dy); v->dragging = 1; SetCapture(v->hwnd); }
        else if (msg == WM_MOUSEMOVE && v->dragging) { poly_add(v, dx, dy); InvalidateRect(v->hwnd, NULL, FALSE); }
        else if (msg == WM_LBUTTONUP && v->dragging) {
            int op = sel_op_from_keys(o->sel_op);
            v->dragging = 0; ReleaseCapture();
            if (v->poly_n >= 3) { pc_sel_polygon(d, v->poly, v->poly_n, op, o->antialias, (float)o->feather); app_commit(d, "Lasso"); }
            else if (op == PC_SEL_REPLACE && pc_sel_has(d)) { pc_sel_clear(d); app_commit(d, "Deselect"); }
            v->poly_n = 0;
            view_invalidate(v, NULL);
        }
        return;
    case T_LASSO_POLY: case T_LASSO_MAGNETIC:
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
            int close = 0;
            if (t == T_LASSO_MAGNETIC) snap_edge(v, &dx, &dy, o->lasso_width);
            if (v->poly_n >= 3) {
                int sx, sy;
                view_doc_to_screen(v, v->poly[0], v->poly[1], &sx, &sy);
                if (abs(sx - x) <= 5 && abs(sy - y) <= 5) close = 1;
            }
            if (msg == WM_LBUTTONDBLCLK) close = 1;
            if (!close) poly_add(v, dx, dy);
            if (close) {
                int op = sel_op_from_keys(o->sel_op);
                if (v->poly_n >= 3) { pc_sel_polygon(d, v->poly, v->poly_n, op, o->antialias, (float)o->feather); app_commit(d, g_tool_info[t].name); }
                v->poly_n = 0;
            }
            view_invalidate(v, NULL);
        } else if (msg == WM_MOUSEMOVE) {
            if (t == T_LASSO_MAGNETIC && v->poly_n > 0) {
                float lx = v->poly[(v->poly_n - 1) * 2], ly = v->poly[(v->poly_n - 1) * 2 + 1];
                float spacing = 4.0f + (100 - o->lasso_freq) / 4.0f;
                if ((dx - lx) * (dx - lx) + (dy - ly) * (dy - ly) > spacing * spacing) { float sx = dx, sy = dy; snap_edge(v, &sx, &sy, o->lasso_width); poly_add(v, sx, sy); }
            }
            if (v->poly_n) InvalidateRect(v->hwnd, NULL, FALSE);
        }
        return;
    case T_WAND:
        if (msg == WM_LBUTTONDOWN) {
            if (!pc_sel_magic_wand(d, (int)dx, (int)dy, o->tolerance, o->contiguous, o->sample_merged, o->antialias, sel_op_from_keys(o->sel_op)))
                app_commit(d, "Magic Wand");
            view_invalidate(v, NULL);
        }
        return;
    case T_CROP:
        if (msg == WM_LBUTTONDBLCLK && v->crop_active) { crop_commit(v); return; }
        if (msg == WM_LBUTTONDOWN) {
            v->crop_handle = v->crop_active ? crop_hit(v, x, y) : -1;
            if (v->crop_handle < 0) { v->crop_active = 1; v->crop[0] = v->crop[2] = (float)floor(dx + 0.5f); v->crop[1] = v->crop[3] = (float)floor(dy + 0.5f); v->crop_handle = 4; }
            v->doc_start_x = dx; v->doc_start_y = dy;
            memcpy(v->xf_orig, v->crop, sizeof v->crop);
            v->dragging = 1; SetCapture(v->hwnd);
        } else if (msg == WM_MOUSEMOVE && v->dragging) {
            float ddx = dx - v->doc_start_x, ddy = dy - v->doc_start_y;
            float *c = v->crop;
            const float *o0 = v->xf_orig;
            int h = v->crop_handle;
            memcpy(c, o0, sizeof v->crop);
            if (h == 8) { c[0] += ddx; c[2] += ddx; c[1] += ddy; c[3] += ddy; }
            else {
                if (h == 0 || h == 6 || h == 7) c[0] += ddx;
                if (h == 2 || h == 3 || h == 4) c[2] += ddx;
                if (h == 0 || h == 1 || h == 2) c[1] += ddy;
                if (h == 4 || h == 5 || h == 6) c[3] += ddy;
            }
            for (h = 0; h < 4; h++) c[h] = (float)floor(c[h] + 0.5f);
            InvalidateRect(v->hwnd, NULL, FALSE);
        } else if (msg == WM_LBUTTONUP && v->dragging) {
            v->dragging = 0; ReleaseCapture();
            if (fabsf(v->crop[2] - v->crop[0]) < 1 || fabsf(v->crop[3] - v->crop[1]) < 1) v->crop_active = 0;
            InvalidateRect(v->hwnd, NULL, FALSE);
        }
        return;
    case T_GRADIENT:
        if (msg == WM_LBUTTONDOWN) { v->doc_start_x = dx; v->doc_start_y = dy; v->dragging = 1; SetCapture(v->hwnd); }
        else if (msg == WM_MOUSEMOVE && v->dragging) {
            if (GetKeyState(VK_SHIFT) < 0) {
                float mx = dx - v->doc_start_x, my = dy - v->doc_start_y;
                double a = floor(atan2(my, mx) / (M_PI / 4) + 0.5) * (M_PI / 4), len = sqrt(mx * mx + my * my);
                v->doc_x = v->doc_start_x + (float)(cos(a) * len); v->doc_y = v->doc_start_y + (float)(sin(a) * len);
            }
            InvalidateRect(v->hwnd, NULL, FALSE);
        } else if (msg == WM_LBUTTONUP && v->dragging) {
            PcGradient g;
            v->dragging = 0; ReleaseCapture();
            memset(&g, 0, sizeof g);
            g.type = o->grad_type; g.preset = o->grad_preset; g.fg = g_app.fg; g.bg = g_app.bg;
            g.opacity = o->opacity; g.blend = o->blend; g.reverse = o->grad_reverse; g.dither = o->grad_dither; g.transparency = o->grad_transparency;
            if (GetKeyState(VK_SHIFT) < 0) { dx = v->doc_x; dy = v->doc_y; }
            {
                HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
                int rc = pc_gradient_fill(d, &g, v->doc_start_x, v->doc_start_y, dx, dy);
                SetCursor(old);
                if (!rc) app_commit(d, "Gradient");
            }
            view_invalidate(v, NULL);
        }
        return;
    case T_BUCKET:
        if (msg == WM_LBUTTONDOWN) {
            if (!pc_bucket_fill(d, (int)dx, (int)dy, g_app.fg, o->tolerance, o->contiguous, o->antialias, o->sample_merged, o->use_pattern, o->opacity, o->blend))
                app_commit(d, "Paint Bucket");
            view_invalidate(v, NULL);
        }
        return;
    case T_TYPE: case T_TYPE_VERTICAL: case T_TYPE_MASK: case T_TYPE_MASK_VERTICAL:
        if (msg == WM_LBUTTONDOWN) {
            PcLayer *l = pc_doc_active(d);
            /* Clicking inside the active type layer's text edits it. */
            if (l && l->kind == PC_LAYER_TEXT && dx >= 0 && dy >= 0 && dx < d->s.w && dy < d->s.h &&
                (t == T_TYPE || t == T_TYPE_VERTICAL)) {
                PcRect lb;
                if (pc_layer_bounds(l, d->s.w, d->s.h, &lb) && dx >= lb.x0 - 4 && dx < lb.x1 + 4 && dy >= lb.y0 - 4 && dy < lb.y1 + 4) { edit_text_layer(v, l); return; }
            }
            commit_text_layer(v, dx, dy, t == T_TYPE_VERTICAL || t == T_TYPE_MASK_VERTICAL, t == T_TYPE_MASK || t == T_TYPE_MASK_VERTICAL);
            view_invalidate(v, NULL);
        }
        return;
    case T_SHAPE_RECT: case T_SHAPE_ELLIPSE: case T_SHAPE_LINE:
        if (msg == WM_LBUTTONDOWN) { v->doc_start_x = dx; v->doc_start_y = dy; v->dragging = 1; SetCapture(v->hwnd); }
        else if (msg == WM_MOUSEMOVE && v->dragging) InvalidateRect(v->hwnd, NULL, FALSE);
        else if (msg == WM_LBUTTONUP && v->dragging) {
            float x0 = PC_MIN(v->doc_start_x, dx), y0 = PC_MIN(v->doc_start_y, dy), x1 = PC_MAX(v->doc_start_x, dx), y1 = PC_MAX(v->doc_start_y, dy);
            float pts[2 * 72];
            int n = 0, k;
            v->dragging = 0; ReleaseCapture();
            if (t == T_SHAPE_LINE) {
                float lx = dx - v->doc_start_x, ly = dy - v->doc_start_y, len = (float)sqrt(lx * lx + ly * ly);
                float wgt = PC_MAX(1, o->line_weight) / 2.0f, nx = len > 0 ? -ly / len * wgt : 0, ny = len > 0 ? lx / len * wgt : 0;
                pts[0] = v->doc_start_x + nx; pts[1] = v->doc_start_y + ny; pts[2] = dx + nx; pts[3] = dy + ny;
                pts[4] = dx - nx; pts[5] = dy - ny; pts[6] = v->doc_start_x - nx; pts[7] = v->doc_start_y - ny; n = 4;
            } else if (t == T_SHAPE_RECT) {
                pts[0] = x0; pts[1] = y0; pts[2] = x1; pts[3] = y0; pts[4] = x1; pts[5] = y1; pts[6] = x0; pts[7] = y1; n = 4;
            } else {
                for (k = 0; k < 64; k++) {
                    double a = k * 2 * M_PI / 64;
                    pts[k * 2] = (x0 + x1) / 2 + (float)cos(a) * (x1 - x0) / 2; pts[k * 2 + 1] = (y0 + y1) / 2 + (float)sin(a) * (y1 - y0) / 2;
                }
                n = 64;
            }
            if (x1 - x0 < 1 && y1 - y0 < 1 && t != T_SHAPE_LINE) return;
            if (o->shape_fill) {
                /* Fill Pixels: rasterise into the active layer with the foreground colour. */
                u8 *m = (u8 *)pc_alloc_checked((size_t)d->s.w, (size_t)d->s.h, 1, 1);
                if (m) {
                    PcBuf *old = d->s.sel ? pc_buf_ref(d->s.sel) : NULL;
                    PcBuf *tmp = pc_buf_new(d->s.w, d->s.h, 1, 1);
                    pc_raster_polygons(m, d->s.w, d->s.h, pts, NULL, n, o->antialias);
                    if (tmp) {
                        size_t i, nn = (size_t)d->s.w * d->s.h;
                        for (i = 0; i < nn; i++) tmp->px[i] = old ? (u8)pc_mul255(m[i], old->px[i]) : m[i];
                        d->s.sel = tmp;
                        pc_fill_color(d, g_app.fg, o->opacity, o->blend, 0);
                        d->s.sel = old;
                        pc_buf_unref(tmp);
                        app_commit(d, g_tool_info[t].name);
                    } else pc_buf_unref(old);
                    free(m);
                }
            } else {
                /* Create Work Path: a closed sub-path of corner points. */
                PcPath *p = pc_path_work(d);
                if (p) {
                    int step = t == T_SHAPE_ELLIPSE ? 16 : 1;
                    for (k = 0; k < n; k += step) {
                        PcPathPt q;
                        memset(&q, 0, sizeof q);
                        q.x = q.ix = q.ox = pts[k * 2]; q.y = q.iy = q.oy = pts[k * 2 + 1];
                        q.start = k == 0; q.closed = k == 0;
                        if (t == T_SHAPE_ELLIPSE) {
                            /* Four smooth anchors with circle handles (kappa 0.5523). */
                            double a = k * 2 * M_PI / 64, kp = 0.5523;
                            float rx = (x1 - x0) / 2, ry = (y1 - y0) / 2;
                            q.ix = q.x + (float)(sin(a) * rx * kp); q.iy = q.y - (float)(cos(a) * ry * kp);
                            q.ox = q.x - (float)(sin(a) * rx * kp); q.oy = q.y + (float)(cos(a) * ry * kp);
                            q.smooth = 1;
                        }
                        pc_path_add_point(p, &q);
                    }
                    app_commit(d, g_tool_info[t].name);
                }
            }
            view_invalidate(v, NULL);
        }
        return;
    case T_PEN: case T_PEN_FREEFORM: case T_PEN_MAGNETIC: case T_PEN_ADD: case T_PEN_DELETE: case T_PEN_CONVERT:
    case T_PATH_SELECT: case T_DIRECT_SELECT: {
        PcPath *p = active_path(d);
        int handle = 0, ai;
        if (t == T_PEN_FREEFORM || t == T_PEN_MAGNETIC) {
            if (msg == WM_LBUTTONDOWN) { v->poly_n = 0; poly_add(v, dx, dy); v->dragging = 1; SetCapture(v->hwnd); }
            else if (msg == WM_MOUSEMOVE && v->dragging) {
                float sx = dx, sy = dy;
                if (t == T_PEN_MAGNETIC) snap_edge(v, &sx, &sy, o->lasso_width);
                poly_add(v, sx, sy);
                InvalidateRect(v->hwnd, NULL, FALSE);
            } else if (msg == WM_LBUTTONUP && v->dragging) {
                float *out;
                int n, k;
                v->dragging = 0; ReleaseCapture();
                out = (float *)malloc(sizeof(float) * 2 * (size_t)PC_MAX(1, v->poly_n));
                p = pc_path_work(d);
                if (out && p && v->poly_n >= 2) {
                    n = simplify(v->poly, v->poly_n, (float)PC_MAX(1, t == T_PEN_FREEFORM ? o->tolerance : 2), out);
                    for (k = 0; k < n; k++) {
                        PcPathPt q;
                        float px = out[PC_MAX(0, k - 1) * 2], py = out[PC_MAX(0, k - 1) * 2 + 1], nx = out[PC_MIN(n - 1, k + 1) * 2], ny = out[PC_MIN(n - 1, k + 1) * 2 + 1];
                        memset(&q, 0, sizeof q);
                        q.x = out[k * 2]; q.y = out[k * 2 + 1];
                        /* Catmull-Rom style handles give the smooth curve fit. */
                        q.ix = q.x - (nx - px) / 6; q.iy = q.y - (ny - py) / 6; q.ox = q.x + (nx - px) / 6; q.oy = q.y + (ny - py) / 6;
                        q.smooth = 1; q.start = k == 0;
                        pc_path_add_point(p, &q);
                    }
                    app_commit(d, g_tool_info[t].name);
                }
                free(out);
                v->poly_n = 0;
                view_invalidate(v, NULL);
            }
            return;
        }
        if (msg == WM_LBUTTONDOWN) {
            ai = find_anchor(v, p, x, y, &handle);
            if (t == T_PEN_DELETE || (t == T_PEN && g_app.topt[T_PEN].pen_auto_add && ai >= 0 && handle == 0 && GetKeyState(VK_CONTROL) >= 0 && ai != 0 && p && p->n > 1 && !(p->pts[0].closed))) {
                if (t == T_PEN_DELETE && ai >= 0 && handle == 0) {
                    int was_start = p->pts[ai].start, was_closed = p->pts[ai].closed;
                    memmove(&p->pts[ai], &p->pts[ai + 1], sizeof(PcPathPt) * (p->n - ai - 1));
                    p->n--;
                    if (was_start && ai < p->n) { p->pts[ai].start = 1; p->pts[ai].closed = was_closed; }
                    app_commit(d, "Delete Anchor Point");
                    view_invalidate(v, NULL);
                }
                if (t == T_PEN_DELETE) return;
            }
            if (t == T_PEN_ADD || (t == T_PEN && g_app.topt[T_PEN].pen_auto_add && ai < 0 && p && p->n >= 2 && GetKeyState(VK_SHIFT) >= 0)) {
                /* Insert on the nearest segment if the click is on the path. */
                int k, best = -1;
                float bd = 25;
                for (k = 0; p && k < p->n; k++) {
                    int nk = (k + 1 < p->n && !p->pts[k + 1].start) ? k + 1 : -1;
                    float mx, my, ddx, ddy;
                    int sx, sy;
                    if (nk < 0) continue;
                    mx = (p->pts[k].x + p->pts[nk].x) / 2 + (p->pts[k].ox - p->pts[k].x + p->pts[nk].ix - p->pts[nk].x) * 0.375f;
                    my = (p->pts[k].y + p->pts[nk].y) / 2 + (p->pts[k].oy - p->pts[k].y + p->pts[nk].iy - p->pts[nk].y) * 0.375f;
                    view_doc_to_screen(v, mx, my, &sx, &sy);
                    ddx = (float)(sx - x); ddy = (float)(sy - y);
                    if (ddx * ddx + ddy * ddy < bd) { bd = ddx * ddx + ddy * ddy; best = k; }
                }
                if (best >= 0) {
                    PcPathPt q;
                    memset(&q, 0, sizeof q);
                    q.x = q.ix = q.ox = dx; q.y = q.iy = q.oy = dy;
                    pc_path_add_point(p, &q);
                    memmove(&p->pts[best + 2], &p->pts[best + 1], sizeof(PcPathPt) * (p->n - best - 2));
                    p->pts[best + 1] = q;
                    app_commit(d, "Add Anchor Point");
                    view_invalidate(v, NULL);
                    return;
                }
                if (t == T_PEN_ADD) return;
            }
            if (t == T_PEN_CONVERT) {
                if (ai >= 0 && handle == 0) {
                    PcPathPt *q = &p->pts[ai];
                    if (q->ix != q->x || q->ox != q->x || q->iy != q->y || q->oy != q->y) { q->ix = q->ox = q->x; q->iy = q->oy = q->y; q->smooth = 0; app_commit(d, "Convert Point"); }
                    else { v->pen_drag_pt = ai; v->pen_drag_handle = 3; v->dragging = 1; SetCapture(v->hwnd); }
                    view_invalidate(v, NULL);
                }
                return;
            }
            if (t == T_DIRECT_SELECT || t == T_PATH_SELECT || (t == T_PEN && GetKeyState(VK_CONTROL) < 0)) {
                if (t == T_PATH_SELECT && p) { save_state(v); v->pen_drag_pt = -1; v->doc_start_x = dx; v->doc_start_y = dy; v->dragging = 1; SetCapture(v->hwnd); return; }
                if (ai >= 0) { v->pen_drag_pt = ai; v->pen_drag_handle = handle; v->dragging = 1; SetCapture(v->hwnd); }
                return;
            }
            /* Pen: add a point; dragging pulls out smooth handles. */
            p = pc_path_work(d);
            if (!p) return;
            if (p->n >= 2 && ai == 0 && handle == 0 && !p->pts[0].closed) {
                int s = 0, k;
                for (k = 0; k < p->n; k++) if (p->pts[k].start) s = k;
                p->pts[s].closed = 1;
                app_commit(d, "Close Path");
                view_invalidate(v, NULL);
                return;
            }
            {
                PcPathPt q;
                int s = 0, k, closed_last = 0;
                memset(&q, 0, sizeof q);
                for (k = 0; k < p->n; k++) if (p->pts[k].start) s = k;
                if (p->n && p->pts[s].closed) closed_last = 1;
                q.x = q.ix = q.ox = dx; q.y = q.iy = q.oy = dy;
                q.start = p->n == 0 || closed_last;
                pc_path_add_point(p, &q);
                v->pen_drag_pt = p->n - 1; v->pen_drag_handle = 3;
                v->dragging = 1; SetCapture(v->hwnd);
                view_invalidate(v, NULL);
            }
        } else if (msg == WM_MOUSEMOVE && v->dragging) {
            if (t == T_PATH_SELECT && v->has_saved) {
                PcPath *sp;
                int k;
                restore_state(v);
                sp = active_path(d);
                for (k = 0; sp && k < sp->n; k++) {
                    float ox = dx - v->doc_start_x, oy = dy - v->doc_start_y;
                    sp->pts[k].x += ox; sp->pts[k].y += oy; sp->pts[k].ix += ox; sp->pts[k].iy += oy; sp->pts[k].ox += ox; sp->pts[k].oy += oy;
                }
            } else if (p && v->pen_drag_pt >= 0 && v->pen_drag_pt < p->n) {
                PcPathPt *q = &p->pts[v->pen_drag_pt];
                if (v->pen_drag_handle == 0) {
                    float ox = dx - q->x, oy = dy - q->y;
                    q->x += ox; q->y += oy; q->ix += ox; q->iy += oy; q->ox += ox; q->oy += oy;
                } else if (v->pen_drag_handle == 3) {
                    /* New smooth point: symmetric handles. */
                    q->ox = dx; q->oy = dy; q->ix = 2 * q->x - dx; q->iy = 2 * q->y - dy; q->smooth = 1;
                } else if (v->pen_drag_handle == 1) {
                    q->ix = dx; q->iy = dy;
                    if (q->smooth && GetKeyState(VK_MENU) >= 0) { q->ox = 2 * q->x - dx; q->oy = 2 * q->y - dy; }
                } else {
                    q->ox = dx; q->oy = dy;
                    if (q->smooth && GetKeyState(VK_MENU) >= 0) { q->ix = 2 * q->x - dx; q->iy = 2 * q->y - dy; }
                }
            }
            InvalidateRect(v->hwnd, NULL, FALSE);
        } else if (msg == WM_LBUTTONUP && v->dragging) {
            v->dragging = 0; ReleaseCapture();
            drop_state(v);
            app_commit(d, t == T_PEN ? "Pen" : t == T_PATH_SELECT ? "Move Path" : "Drag Point");
        }
        return;
    }
    default:
        break;
    }

    /* Brush-based tools. */
    if (brush_tool(t)) {
        if (msg == WM_LBUTTONDOWN) {
            PcPaintOpts po;
            PcRect dirty;
            if (t == T_STAMP && GetKeyState(VK_MENU) < 0) { pc_clone_set_source(d, dx, dy); return; }
            po = paint_opts(v, t);
            {
                PcLayer *l = pc_doc_active(d);
                if (po.target_mask == 0 && l && (l->lock_pixels || (l->kind == PC_LAYER_TEXT))) {
                    if (l->kind == PC_LAYER_TEXT) app_error("This type layer must be rasterized before painting on it (Type > Rasterize Type Layer).");
                    else app_error("Could not use the %s because the layer is locked.", g_tool_info[t].name);
                    return;
                }
            }
            if (t == T_PATTERN_STAMP || (t == T_BUCKET)) {
                const u8 *pp; int pw, ph;
                if (pc_pattern_get(&pp, &pw, &ph)) { app_error("Could not use the Pattern Stamp tool because the pattern is not defined. Select an area and choose Edit > Define Pattern."); return; }
            }
            save_state(v);
            if (GetKeyState(VK_SHIFT) < 0 && g_last_paint_valid) {
                /* Shift-click paints a straight line from the last point. */
                v->stroke = pc_stroke_begin(d, &po, g_last_paint_x, g_last_paint_y);
                if (v->stroke) pc_stroke_to(v->stroke, dx, dy, &dirty);
            } else {
                v->stroke = pc_stroke_begin(d, &po, dx, dy);
                if (v->stroke) pc_stroke_to(v->stroke, dx, dy, &dirty);
            }
            if (!v->stroke) { drop_state(v); return; }
            v->dragging = 1;
            SetCapture(v->hwnd);
            refresh_after_edit(v, &dirty);
        } else if (msg == WM_MOUSEMOVE && v->dragging && v->stroke) {
            PcRect dirty;
            pc_stroke_to(v->stroke, dx, dy, &dirty);
            if (dirty.x1 > dirty.x0) refresh_after_edit(v, &dirty);
        } else if (msg == WM_LBUTTONUP && v->dragging) {
            v->dragging = 0;
            ReleaseCapture();
            if (v->stroke) {
                pc_stroke_end(v->stroke);
                v->stroke = NULL;
                drop_state(v);
                g_last_paint_valid = 1; g_last_paint_x = dx; g_last_paint_y = dy;
                app_commit(d, g_tool_info[t].name);
            }
            v->temp_tool = -1;
        } else if (msg == WM_MOUSEMOVE && !v->dragging && g_app.prefs.brush_size_cursor) {
            InvalidateRect(v->hwnd, NULL, FALSE);
        }
    }
}

void tool_timer(DocView *v)
{
    /* Marching ants and airbrush build-up. */
    if (v->stroke && v->dragging && (g_app.tool == T_AIRBRUSH || (g_app.tool == T_ERASER && g_app.topt[T_ERASER].eraser_mode == 1))) {
        PcRect dirty;
        pc_stroke_tick(v->stroke, &dirty);
        if (dirty.x1 > dirty.x0) view_invalidate(v, &dirty);
    }
    if (v->doc->s.sel && v->show_edges && IsWindowVisible(v->hwnd) && !v->doc->s.quickmask) {
        v->ants_phase = (v->ants_phase + 1) & 7;
        InvalidateRect(v->hwnd, NULL, FALSE);
    }
}

/* ---------------- keyboard ---------------- */

int tool_key(DocView *v, int vk, int down)
{
    PcDoc *d = v->doc;
    if (!down) {
        if (vk == VK_SPACE && v->temp_tool >= 0 && !v->dragging) { v->temp_tool = -1; view_update_cursor(v); return 1; }
        if ((vk == VK_CONTROL || vk == VK_MENU) && v->temp_tool == T_MOVE && !v->dragging) { v->temp_tool = -1; view_update_cursor(v); }
        if (vk == VK_MENU || vk == VK_CONTROL || vk == VK_CAPITAL) view_update_cursor(v);
        return 0;
    }
    if (vk == VK_SPACE) {
        if (v->temp_tool < 0 && !v->dragging) {
            v->temp_tool = GetKeyState(VK_CONTROL) < 0 ? T_ZOOM : GetKeyState(VK_MENU) < 0 ? T_ZOOM : T_HAND;
            view_update_cursor(v);
        }
        return 1;
    }
    if (vk == VK_MENU || vk == VK_CONTROL || vk == VK_CAPITAL) { view_update_cursor(v); return 0; }
    if (vk == VK_RETURN) {
        if (v->crop_active || v->xf_active || v->poly_n) { tool_commit(v); return 1; }
        return 0;
    }
    if (vk == VK_ESCAPE) {
        if (v->crop_active || v->xf_active || v->poly_n || v->dragging) { tool_cancel(v); return 1; }
        return 0;
    }
    if (vk == VK_BACK && v->poly_n > 0 && (g_app.tool == T_LASSO_POLY || g_app.tool == T_LASSO_MAGNETIC)) {
        v->poly_n--;
        InvalidateRect(v->hwnd, NULL, FALSE);
        return 1;
    }
    if (vk == VK_BACK || vk == VK_DELETE) {
        if (vk == VK_DELETE || GetKeyState(VK_MENU) < 0) return 0;   /* Edit > Clear / Alt+Backspace fill */
    }
    if ((vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) && GetKeyState(VK_CONTROL) >= 0) {
        int step = GetKeyState(VK_SHIFT) < 0 ? 10 : 1;
        int dx = vk == VK_LEFT ? -step : vk == VK_RIGHT ? step : 0, dy = vk == VK_UP ? -step : vk == VK_DOWN ? step : 0;
        if (g_app.tool == T_MOVE) {
            PcLayer *l = pc_doc_active(d);
            if (!l || (l->is_background && !pc_sel_has(d))) return 1;
            if (pc_sel_has(d)) {
                PcRect r;
                float q[8];
                pc_sel_bounds(d, &r);
                q[0] = (float)(r.x0 + dx); q[1] = (float)(r.y0 + dy); q[2] = (float)(r.x1 + dx); q[3] = (float)(r.y0 + dy);
                q[4] = (float)(r.x1 + dx); q[5] = (float)(r.y1 + dy); q[6] = (float)(r.x0 + dx); q[7] = (float)(r.y1 + dy);
                pc_layer_transform_quad(d, &r, q, PC_RESAMPLE_NEAREST);
            } else pc_layer_offset(d, dx, dy);
            app_commit(d, "Nudge");
            return 1;
        }
        if (g_app.tool <= T_LASSO_MAGNETIC && d->s.sel) {
            /* Selection tools nudge the selection outline. */
            PcBuf *s = pc_sel_get(d, 0), *n;
            int x, y;
            if (!s) return 1;
            n = pc_buf_new(d->s.w, d->s.h, 1, 1);
            if (!n) return 1;
            for (y = 0; y < d->s.h; y++) for (x = 0; x < d->s.w; x++) {
                int sx = x - dx, sy = y - dy;
                if (sx >= 0 && sy >= 0 && sx < d->s.w && sy < d->s.h) n->px[(size_t)y * d->s.w + x] = s->px[(size_t)sy * d->s.w + sx];
            }
            pc_buf_unref(d->s.sel);
            d->s.sel = n;
            app_commit(d, "Nudge Outline");
            return 1;
        }
    }
    return 0;
}

/* ---------------- cursor ---------------- */

int tool_cursor(DocView *v)
{
    int t = effective_tool(v);
    int caps = GetKeyState(VK_CAPITAL) & 1;
    if (v->dragging && t == T_HAND) return PC_CURSOR_GRAB;
    if (v->xf_active) {
        POINT p;
        int h;
        GetCursorPos(&p); ScreenToClient(v->hwnd, &p);
        h = xf_hit(v, p.x, p.y);
        if (h == 8) return PC_CURSOR_MOVE;
        if (h == 9) return PC_CURSOR_ROTATE;
        if (h == 0 || h == 2) return PC_CURSOR_SCALE_NWSE;
        if (h == 1 || h == 3) return PC_CURSOR_SCALE_NESW;
        if (h == 4 || h == 6) return PC_CURSOR_SCALE_NS;
        return PC_CURSOR_SCALE_EW;
    }
    if (t == T_ZOOM) return GetKeyState(VK_MENU) < 0 ? PC_CURSOR_ZOOM_OUT : (v->zoom >= 16 ? PC_CURSOR_ZOOM_NONE : PC_CURSOR_ZOOM_IN);
    if (t == T_STAMP && GetKeyState(VK_MENU) < 0) return PC_CURSOR_STAMP_SOURCE;
    if (brush_tool(t) && GetKeyState(VK_MENU) < 0 && (t == T_BRUSH || t == T_AIRBRUSH || t == T_PENCIL)) return PC_CURSOR_EYEDROPPER;
    if ((t >= T_MARQUEE_RECT && t <= T_WAND) && !v->dragging) {
        int sh = GetKeyState(VK_SHIFT) < 0, al = GetKeyState(VK_MENU) < 0;
        if (sh && al) return PC_CURSOR_MARQUEE_INT;
        if (sh) return PC_CURSOR_MARQUEE_ADD;
        if (al) return PC_CURSOR_MARQUEE_SUB;
    }
    if (caps || g_app.prefs.precise_cursors) return PC_CURSOR_CROSSHAIR;
    if (brush_tool(t) && g_app.prefs.brush_size_cursor && g_app.topt[t].brush.diameter * v->zoom > 4) return PC_CURSOR_CROSSHAIR;
    return g_tool_info[t].cursor;
}

/* ---------------- overlays ---------------- */

static void xor_line(HDC dc, int x0, int y0, int x1, int y1)
{
    int om = SetROP2(dc, R2_NOT);
    MoveToEx(dc, x0, y0, NULL);
    LineTo(dc, x1, y1);
    SetROP2(dc, om);
}

static void dashed_rect(HDC dc, int x0, int y0, int x1, int y1, int phase)
{
    int x, y;
    for (x = x0; x <= x1; x++) {
        COLORREF c = (((x + phase) >> 2) & 1) ? RGB(0, 0, 0) : RGB(255, 255, 255);
        SetPixel(dc, x, y0, c); SetPixel(dc, x, y1, c);
    }
    for (y = y0; y <= y1; y++) {
        COLORREF c = (((y + phase) >> 2) & 1) ? RGB(0, 0, 0) : RGB(255, 255, 255);
        SetPixel(dc, x0, y, c); SetPixel(dc, x1, y, c);
    }
}

static void handle_box(HDC dc, int x, int y)
{
    RECT r;
    SetRect(&r, x - 3, y - 3, x + 4, y + 4);
    w95_fill(dc, &r, RGB(255, 255, 255));
    w95_rect(dc, r.left, r.top, r.right, r.bottom, RGB(0, 0, 0));
}

static void draw_path(DocView *v, HDC dc, PcPath *p, int show_points)
{
    float xy[2 * 2048];
    u8 st[2048];
    int n, k;
    HPEN pen, op;
    if (!p || !p->n) return;
    n = pc_path_flatten(p, xy, st, 2048);
    pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    op = (HPEN)SelectObject(dc, pen);
    SetROP2(dc, R2_NOT);
    for (k = 0; k < n; k++) {
        int sx, sy;
        view_doc_to_screen(v, xy[k * 2], xy[k * 2 + 1], &sx, &sy);
        if (st[k]) MoveToEx(dc, sx, sy, NULL); else LineTo(dc, sx, sy);
    }
    SetROP2(dc, R2_COPYPEN);
    SelectObject(dc, op);
    DeleteObject(pen);
    if (!show_points) return;
    for (k = 0; k < p->n; k++) {
        PcPathPt *q = &p->pts[k];
        int ax, ay, hx, hy;
        RECT r;
        view_doc_to_screen(v, q->x, q->y, &ax, &ay);
        if (q->ix != q->x || q->iy != q->y) { view_doc_to_screen(v, q->ix, q->iy, &hx, &hy); xor_line(dc, ax, ay, hx, hy); SetRect(&r, hx - 2, hy - 2, hx + 3, hy + 3); w95_fill(dc, &r, RGB(0, 0, 0)); }
        if (q->ox != q->x || q->oy != q->y) { view_doc_to_screen(v, q->ox, q->oy, &hx, &hy); xor_line(dc, ax, ay, hx, hy); SetRect(&r, hx - 2, hy - 2, hx + 3, hy + 3); w95_fill(dc, &r, RGB(0, 0, 0)); }
        SetRect(&r, ax - 2, ay - 2, ax + 3, ay + 3);
        w95_fill(dc, &r, RGB(255, 255, 255));
        w95_rect(dc, r.left, r.top, r.right, r.bottom, RGB(0, 0, 0));
    }
}

void tool_paint_overlay(DocView *v, HDC dc)
{
    int t = effective_tool(v);
    PcDoc *d = v->doc;
    /* Paths are visible with path tools and when a path is selected in the Paths panel. */
    if (d->s.active_path >= 0 && d->s.active_path < d->s.npaths)
        draw_path(v, dc, &d->s.paths[d->s.active_path], (t >= T_PEN && t <= T_DIRECT_SELECT));
    if (v->xf_active) {
        int k, sx[4], sy[4];
        for (k = 0; k < 4; k++) view_doc_to_screen(v, v->xf_quad[k * 2], v->xf_quad[k * 2 + 1], &sx[k], &sy[k]);
        for (k = 0; k < 4; k++) xor_line(dc, sx[k], sy[k], sx[(k + 1) % 4], sy[(k + 1) % 4]);
        for (k = 0; k < 4; k++) { handle_box(dc, sx[k], sy[k]); handle_box(dc, (sx[k] + sx[(k + 1) % 4]) / 2, (sy[k] + sy[(k + 1) % 4]) / 2); }
        {
            int cx = (sx[0] + sx[1] + sx[2] + sx[3]) / 4, cy = (sy[0] + sy[1] + sy[2] + sy[3]) / 4;
            xor_line(dc, cx - 4, cy, cx + 5, cy); xor_line(dc, cx, cy - 4, cx, cy + 5);
        }
        return;
    }
    if (v->crop_active) {
        int x0, y0, x1, y1;
        view_doc_to_screen(v, PC_MIN(v->crop[0], v->crop[2]), PC_MIN(v->crop[1], v->crop[3]), &x0, &y0);
        view_doc_to_screen(v, PC_MAX(v->crop[0], v->crop[2]), PC_MAX(v->crop[1], v->crop[3]), &x1, &y1);
        dashed_rect(dc, x0, y0, x1, y1, v->ants_phase);
        handle_box(dc, x0, y0); handle_box(dc, (x0 + x1) / 2, y0); handle_box(dc, x1, y0); handle_box(dc, x1, (y0 + y1) / 2);
        handle_box(dc, x1, y1); handle_box(dc, (x0 + x1) / 2, y1); handle_box(dc, x0, y1); handle_box(dc, x0, (y0 + y1) / 2);
        return;
    }
    if (v->measure_active) {
        int x0, y0, x1, y1;
        view_doc_to_screen(v, v->measure[0], v->measure[1], &x0, &y0);
        view_doc_to_screen(v, v->measure[2], v->measure[3], &x1, &y1);
        xor_line(dc, x0, y0, x1, y1);
        xor_line(dc, x0 - 3, y0, x0 + 4, y0); xor_line(dc, x0, y0 - 3, x0, y0 + 4);
        xor_line(dc, x1 - 3, y1, x1 + 4, y1); xor_line(dc, x1, y1 - 3, x1, y1 + 4);
    }
    if (v->dragging) {
        if (t >= T_MARQUEE_RECT && t <= T_MARQUEE_ELLIPSE) {
            float x0, y0, x1, y1;
            int a, b, c, e;
            marquee_rect(v, t, &x0, &y0, &x1, &y1);
            view_doc_to_screen(v, x0, y0, &a, &b);
            view_doc_to_screen(v, x1, y1, &c, &e);
            if (t == T_MARQUEE_ELLIPSE) {
                HPEN pen = CreatePen(PS_DOT, 1, RGB(0, 0, 0)), op = (HPEN)SelectObject(dc, pen);
                HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
                SetBkColor(dc, RGB(255, 255, 255));
                Ellipse(dc, a, b, c + 1, e + 1);
                SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
            } else dashed_rect(dc, a, b, c, e, v->ants_phase);
        } else if (t == T_ZOOM) {
            dashed_rect(dc, PC_MIN(v->drag_start.x, v->drag_last.x), PC_MIN(v->drag_start.y, v->drag_last.y),
                        PC_MAX(v->drag_start.x, v->drag_last.x), PC_MAX(v->drag_start.y, v->drag_last.y), 0);
        } else if (t == T_GRADIENT || t == T_SHAPE_LINE) {
            int a, b, c, e;
            view_doc_to_screen(v, v->doc_start_x, v->doc_start_y, &a, &b);
            view_doc_to_screen(v, v->doc_x, v->doc_y, &c, &e);
            xor_line(dc, a, b, c, e);
            if (t == T_GRADIENT) { xor_line(dc, a - 3, b, a + 4, b); xor_line(dc, a, b - 3, a, b + 4); }
        } else if (t == T_SHAPE_RECT || t == T_SHAPE_ELLIPSE) {
            int a, b, c, e;
            view_doc_to_screen(v, PC_MIN(v->doc_start_x, v->doc_x), PC_MIN(v->doc_start_y, v->doc_y), &a, &b);
            view_doc_to_screen(v, PC_MAX(v->doc_start_x, v->doc_x), PC_MAX(v->doc_start_y, v->doc_y), &c, &e);
            if (t == T_SHAPE_RECT) { xor_line(dc, a, b, c, b); xor_line(dc, c, b, c, e); xor_line(dc, c, e, a, e); xor_line(dc, a, e, a, b); }
            else {
                int om = SetROP2(dc, R2_NOT);
                HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
                Ellipse(dc, a, b, c, e);
                SelectObject(dc, ob); SetROP2(dc, om);
            }
        }
    }
    if (v->poly_n > 0) {
        int k, px, py, sx = 0, sy = 0;
        for (k = 0; k < v->poly_n; k++) {
            view_doc_to_screen(v, v->poly[k * 2], v->poly[k * 2 + 1], &px, &py);
            if (k) xor_line(dc, sx, sy, px, py);
            sx = px; sy = py;
        }
        if ((t == T_LASSO_POLY || t == T_LASSO_MAGNETIC) && v->mouse_in) xor_line(dc, sx, sy, v->mouse.x, v->mouse.y);
    }
    /* Brush Size painting cursor: the brush outline follows the mouse. */
    if (brush_tool(t) && g_app.prefs.brush_size_cursor && v->mouse_in && !(GetKeyState(VK_CAPITAL) & 1)) {
        int r = (int)(g_app.topt[t].brush.diameter * v->zoom / 2 + 0.5);
        if (r > 2) {
            int om = SetROP2(dc, R2_NOT);
            HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, v->mouse.x - r, v->mouse.y - r, v->mouse.x + r + 1, v->mouse.y + r + 1);
            SelectObject(dc, ob);
            SetROP2(dc, om);
        }
    }
}
