/*
 * Document area and document views.
 *
 * Documents open as CS6-style tabs across the top of the document area,
 * and can float in their own hand-framed Windows 95 child windows
 * (Window > Arrange > Float in Window / Cascade / Tile).  Each view draws
 * the image with the CPU compositor into a 32-bit DIB (GDI does any
 * dithering on 16/256-colour displays), plus rulers, Windows 95 scroll
 * bars and the CS6 per-document status row.
 */
#include "app.h"
#include "dlg.h"

#define TABS_H 18
#define STATUS_H 17
#define RULER 16

const double g_zoom_steps[] = { 0.01, 0.02, 0.03, 0.04, 0.05, 0.0625, 0.0833, 0.125, 0.1667, 0.25, 0.3333, 0.5, 0.6667,
                                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
const int g_zoom_nsteps = (int)(sizeof g_zoom_steps / sizeof g_zoom_steps[0]);

static HWND g_area;

static LRESULT CALLBACK area_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK view_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK frame_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static DocView *view_of(HWND h)
{
    int i;
    for (i = 0; i < g_app.nviews; i++) if (g_app.views[i]->hwnd == h || g_app.views[i]->frame == h) return g_app.views[i];
    return NULL;
}

static int index_of(DocView *v)
{
    int i;
    for (i = 0; i < g_app.nviews; i++) if (g_app.views[i] == v) return i;
    return -1;
}

static int has_tabs(void)
{
    int i;
    if (g_app.screen_mode) return 0;
    for (i = 0; i < g_app.nviews; i++) if (!g_app.views[i]->frame) return 1;
    return 0;
}

/* ---------------- titles ---------------- */

void view_title(DocView *v, char *out, int n)
{
    PcLayer *l = pc_doc_active(v->doc);
    double z = v->zoom * 100;
    char zs[24];
    if (z >= 10 || fabs(z - floor(z + 0.5)) < 0.01) pc_snprintf(zs, sizeof zs, "%.1f", z);
    else pc_snprintf(zs, sizeof zs, "%.2f", z);
    if (zs[strlen(zs) - 1] == '0' && strchr(zs, '.')) { zs[strlen(zs) - 1] = 0; if (zs[strlen(zs) - 1] == '.') zs[strlen(zs) - 1] = 0; }
    pc_snprintf(out, (size_t)n, "%s @ %s%% (%s, %s/8)%s", v->doc->title, zs, l ? l->name : "",
                v->doc->s.mode == PC_MODE_GRAY ? "Gray" : "RGB", v->doc->dirty ? " *" : "");
}

/* ---------------- geometry ---------------- */

void view_canvas_rect(DocView *v, RECT *r)
{
    RECT c;
    int ru = v->show_rulers ? RULER : 0;
    GetClientRect(v->hwnd, &c);
    if (g_app.screen_mode) { *r = c; return; }
    SetRect(r, ru, ru, PC_MAX(ru, c.right - W95_SCROLL), PC_MAX(ru, c.bottom - STATUS_H));
}

static int canvas_w(DocView *v) { return (int)ceil(v->doc->s.w * v->zoom); }
static int canvas_h(DocView *v) { return (int)ceil(v->doc->s.h * v->zoom); }

/* Image origin in view coordinates (centred when smaller than the window). */
static void image_origin(DocView *v, int *ox, int *oy)
{
    RECT r;
    int cw = canvas_w(v), ch = canvas_h(v);
    view_canvas_rect(v, &r);
    *ox = cw < r.right - r.left ? r.left + (r.right - r.left - cw) / 2 : r.left - v->sx;
    *oy = ch < r.bottom - r.top ? r.top + (r.bottom - r.top - ch) / 2 : r.top - v->sy;
}

void view_screen_to_doc(DocView *v, int x, int y, float *dx, float *dy)
{
    int ox, oy;
    image_origin(v, &ox, &oy);
    *dx = (float)((x - ox) / v->zoom);
    *dy = (float)((y - oy) / v->zoom);
}

void view_doc_to_screen(DocView *v, float dx, float dy, int *x, int *y)
{
    int ox, oy;
    image_origin(v, &ox, &oy);
    *x = ox + (int)floor(dx * v->zoom + 0.5);
    *y = oy + (int)floor(dy * v->zoom + 0.5);
}

void view_clamp_scroll(DocView *v)
{
    RECT r;
    view_canvas_rect(v, &r);
    v->sx = PC_CLAMP(v->sx, 0, PC_MAX(0, canvas_w(v) - (r.right - r.left)));
    v->sy = PC_CLAMP(v->sy, 0, PC_MAX(0, canvas_h(v) - (r.bottom - r.top)));
}

void view_scroll_to(DocView *v, int sx, int sy)
{
    v->sx = sx; v->sy = sy;
    view_clamp_scroll(v);
    InvalidateRect(v->hwnd, NULL, FALSE);
    dock_refresh(R_NAV);
}

void view_set_zoom(DocView *v, double zoom, int ax, int ay)
{
    RECT r;
    float dx, dy;
    int nx, ny;
    zoom = PC_CLAMP(zoom, 0.01, 16.0);
    view_canvas_rect(v, &r);
    if (ax < 0) { ax = (r.left + r.right) / 2; ay = (r.top + r.bottom) / 2; }
    view_screen_to_doc(v, ax, ay, &dx, &dy);
    v->zoom = zoom;
    v->sx = (int)(dx * zoom) - (ax - r.left);
    v->sy = (int)(dy * zoom) - (ay - r.top);
    view_clamp_scroll(v);
    view_doc_to_screen(v, dx, dy, &nx, &ny);
    (void)nx; (void)ny;
    v->dirty_full = 1;
    InvalidateRect(v->hwnd, NULL, FALSE);
    app_refresh(R_TITLE | R_NAV);
    if (g_app.topt[T_ZOOM].zoom_resize && v->frame && !v->fr.maximized) {
        RECT fr;
        int w = PC_MIN(canvas_w(v) + W95_SCROLL, 900), h = PC_MIN(canvas_h(v) + STATUS_H, 700);
        w95_frame_adjust(W95_FRAME_SIZABLE | W95_FRAME_MINMAX | W95_FRAME_CLOSE, &w, &h);
        GetWindowRect(v->frame, &fr);
        MapWindowPoints(NULL, g_area, (POINT *)&fr, 2);
        SetWindowPos(v->frame, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void view_zoom_fit(DocView *v, int actual)
{
    RECT r;
    double z;
    int i;
    if (actual) { view_set_zoom(v, 1.0, -1, -1); return; }
    view_canvas_rect(v, &r);
    z = PC_MIN((double)(r.right - r.left - 4) / v->doc->s.w, (double)(r.bottom - r.top - 4) / v->doc->s.h);
    /* Snap down to a standard zoom step like Photoshop. */
    for (i = g_zoom_nsteps - 1; i > 0 && g_zoom_steps[i] > z; i--) {}
    view_set_zoom(v, PC_MAX(g_zoom_steps[i], 0.01), -1, -1);
}

/* ---------------- display rendering ---------------- */

void view_invalidate(DocView *v, const PcRect *r)
{
    if (!v || !v->hwnd) return;
    if (!r) v->dirty_full = 1;
    else if (!v->dirty_full) {
        if (v->dirty.x1 <= v->dirty.x0) v->dirty = *r;
        else {
            v->dirty.x0 = PC_MIN(v->dirty.x0, r->x0); v->dirty.y0 = PC_MIN(v->dirty.y0, r->y0);
            v->dirty.x1 = PC_MAX(v->dirty.x1, r->x1); v->dirty.y1 = PC_MAX(v->dirty.y1, r->y1);
        }
    }
    InvalidateRect(v->hwnd, NULL, FALSE);
}

static void update_composite(DocView *v)
{
    PcDoc *d = v->doc;
    if (!v->comp || v->comp->w != d->s.w || v->comp->h != d->s.h) {
        pc_buf_unref(v->comp);
        v->comp = pc_doc_composite(d);
        v->comp_rev = d->revision;
        v->dirty_full = 0; v->dirty.x1 = v->dirty.x0 = 0;
        return;
    }
    if (v->comp->refs > 1) { PcBuf *c = pc_buf_clone(v->comp); if (c) { pc_buf_unref(v->comp); v->comp = c; } }
    /* Strokes change pixels before the revision bumps (at stroke end), so a
     * pending dirty rect is always recomposited. */
    if (v->dirty.x1 > v->dirty.x0 && !v->dirty_full) {
        pc_doc_composite_into(d, v->comp, &v->dirty);
    } else if (v->comp_rev != d->revision || v->dirty_full) {
        pc_doc_composite_into(d, v->comp, NULL);
    }
    v->comp_rev = d->revision;
    v->dirty_full = 0;
    v->dirty.x0 = v->dirty.x1 = 0;
}

static void build_ants(DocView *v, int ox, int oy, const RECT *cr)
{
    PcDoc *d = v->doc;
    int x, y, cap = 4096, x0, y0, x1, y1;
    const u8 *sel = d->s.sel ? d->s.sel->px : NULL;
    free(v->ants); v->ants = NULL; v->nants = 0;
    v->ants_key_valid = 1; v->ants_rev = d->revision; v->ants_zoom = v->zoom; v->ants_sx = ox; v->ants_sy = oy;
    v->ants_w = cr->right; v->ants_h = cr->bottom;
    if (!sel || d->s.quickmask) return;
    v->ants = (int *)malloc(sizeof(int) * (size_t)cap);
    if (!v->ants) return;
    x0 = PC_MAX(cr->left, ox); y0 = PC_MAX(cr->top, oy);
    x1 = PC_MIN(cr->right, ox + canvas_w(v)); y1 = PC_MIN(cr->bottom, oy + canvas_h(v));
    for (y = y0; y < y1; y++) {
        int dy = PC_MIN((int)((y - oy) / v->zoom), d->s.h - 1);
        int dyn = PC_MIN((int)((y + 1 - oy) / v->zoom), d->s.h - 1);
        for (x = x0; x < x1; x++) {
            int dx = PC_MIN((int)((x - ox) / v->zoom), d->s.w - 1);
            int dxn = PC_MIN((int)((x + 1 - ox) / v->zoom), d->s.w - 1);
            int in = sel[(size_t)dy * d->s.w + dx] >= 128, edge = 0;
            if (x + 1 >= x1 ? in : (in != (sel[(size_t)dy * d->s.w + dxn] >= 128))) edge = 1;
            if (!edge && (y + 1 >= y1 ? in : (in != (sel[(size_t)dyn * d->s.w + dx] >= 128)))) edge = 1;
            if (!edge && (x == x0 || y == y0) && in) edge = 1;
            if (!edge) continue;
            if (v->nants >= cap) {
                int *n = (int *)realloc(v->ants, sizeof(int) * (size_t)cap * 2);
                if (!n) return;
                v->ants = n; cap *= 2;
            }
            v->ants[v->nants++] = x | (y << 16);
        }
    }
}

static void paint_canvas(DocView *v, HDC dc, const RECT *cr)
{
    PcDoc *d = v->doc;
    int ox, oy, x, y, w = cr->right - cr->left, h = cr->bottom - cr->top;
    int ix0, iy0, ix1, iy1;
    u8 *row;
    BITMAPINFO bi;
    COLORREF bgc = g_app.screen_mode == 2 ? RGB(0, 0, 0) : W95_SHADOW;
    update_composite(v);
    image_origin(v, &ox, &oy);
    /* Background around the image. */
    w95_fill(dc, cr, bgc);
    ix0 = PC_MAX(cr->left, ox); iy0 = PC_MAX(cr->top, oy);
    ix1 = PC_MIN(cr->right, ox + canvas_w(v)); iy1 = PC_MIN(cr->bottom, oy + canvas_h(v));
    if (ix1 <= ix0 || iy1 <= iy0 || !v->comp) return;
    /* Drop shadow / frame like PS's document edge. */
    if (ox > cr->left) w95_vline(dc, ix0 - 1, iy0, iy1, W95_DKSHADOW);
    if (oy > cr->top) w95_hline(dc, ix0, ix1, iy0 - 1, W95_DKSHADOW);
    if (ox + canvas_w(v) < cr->right) w95_vline(dc, ix1, iy0 + 2, iy1 + 1, W95_DKSHADOW);
    if (oy + canvas_h(v) < cr->bottom) w95_hline(dc, ix0 + 2, ix1 + 1, iy1, W95_DKSHADOW);
    w = ix1 - ix0; h = iy1 - iy0;
    row = (u8 *)malloc((size_t)w * h * 4);
    if (!row) return;
    {
        int *xmap = (int *)malloc(sizeof(int) * (size_t)w);
        const u8 *qm = d->s.quickmask && d->s.sel ? d->s.sel->px : NULL;
        int chk = g_app.prefs.checker_size == 0 ? 2 : g_app.prefs.checker_size == 2 ? 4 : 3;   /* small/medium/large */
        if (!xmap) { free(row); return; }
        for (x = 0; x < w; x++) xmap[x] = PC_MIN((int)((ix0 + x - ox) / v->zoom), d->s.w - 1);
        for (y = 0; y < h; y++) {
            int dy = PC_MIN((int)((iy0 + y - oy) / v->zoom), d->s.h - 1);
            const u8 *src = v->comp->px + (size_t)dy * d->s.w * 4;
            u8 *o = row + (size_t)y * w * 4;
            int sy = iy0 + y;
            for (x = 0; x < w; x++, o += 4) {
                const u8 *p = src + xmap[x] * 4;
                int a = p[3], r = p[0], g = p[1], b = p[2];
                if (v->show_channel >= 0 && v->show_channel <= 2) { r = g = b = p[v->show_channel]; }
                if (a < 255) {
                    int on = ((((ix0 + x) >> chk) ^ (sy >> chk)) & 1);
                    int bgv = on ? 204 : 255;
                    r = (r * a + bgv * (255 - a)) / 255; g = (g * a + bgv * (255 - a)) / 255; b = (b * a + bgv * (255 - a)) / 255;
                }
                if (qm) {
                    /* Quick Mask: unselected areas tinted 50% red. */
                    int m = 255 - qm[(size_t)dy * d->s.w + xmap[x]];
                    int t = m / 2;
                    r = (r * (255 - t) + 255 * t) / 255; g = g * (255 - t) / 255; b = b * (255 - t) / 255;
                }
                {
                    int k;
                    for (k = 0; k < d->s.nchannels; k++) if (d->s.channels[k].visible) {
                        int m = (255 - d->s.channels[k].buf->px[(size_t)dy * d->s.w + xmap[x]]) / 2;
                        r = (r * (255 - m) + pc_r(d->s.channels[k].color) * m) / 255;
                        g = (g * (255 - m) + pc_g(d->s.channels[k].color) * m) / 255;
                        b = (b * (255 - m) + pc_b(d->s.channels[k].color) * m) / 255;
                    }
                }
                o[0] = (u8)b; o[1] = (u8)g; o[2] = (u8)r; o[3] = 0;
            }
        }
        /* Pixel grid at 800%+ is not PS5; the optional View > Grid is. */
        if (v->show_grid && g_app.prefs.grid_spacing > 0) {
            int gs = g_app.prefs.grid_spacing;
            u32 gc = g_app.prefs.grid_color;
            for (y = 0; y < h; y++) {
                int dy = (int)((iy0 + y - oy) / v->zoom), dyp = (int)((iy0 + y - 1 - oy) / v->zoom);
                u8 *o = row + (size_t)y * w * 4;
                int hline = y > 0 && dy / gs != dyp / gs;
                for (x = 0; x < w; x++) {
                    int dx = xmap[x], dxp = x > 0 ? xmap[x - 1] : dx;
                    if (hline || (x > 0 && dx / gs != dxp / gs)) { o[x * 4] = (u8)pc_b(gc); o[x * 4 + 1] = (u8)pc_g(gc); o[x * 4 + 2] = (u8)pc_r(gc); }
                }
            }
        }
        free(xmap);
    }
    /* Marching ants. */
    if (v->show_edges && d->s.sel && !d->s.quickmask) {
        int k;
        if (!v->ants_key_valid || v->ants_rev != d->revision || v->ants_zoom != v->zoom || v->ants_sx != ox || v->ants_sy != oy ||
            v->ants_w != cr->right || v->ants_h != cr->bottom) build_ants(v, ox, oy, cr);
        for (k = 0; k < v->nants; k++) {
            int ax = (v->ants[k] & 0xFFFF) - ix0, ay = (v->ants[k] >> 16) - iy0;
            u8 c;
            if (ax < 0 || ay < 0 || ax >= w || ay >= h) continue;
            c = ((((ax + ix0) + (ay + iy0) + v->ants_phase) >> 2) & 1) ? 0 : 255;
            row[((size_t)ay * w + ax) * 4] = row[((size_t)ay * w + ax) * 4 + 1] = row[((size_t)ay * w + ax) * 4 + 2] = c;
        }
    }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, ix0, iy0, (DWORD)w, (DWORD)h, 0, 0, 0, (UINT)h, row, &bi, DIB_RGB_COLORS);
    free(row);
}

static void paint_rulers(DocView *v, HDC dc)
{
    RECT c, top, left, corner;
    int ox, oy, step, k;
    double unit = 1;
    GetClientRect(v->hwnd, &c);
    image_origin(v, &ox, &oy);
    SetRect(&corner, 0, 0, RULER, RULER);
    SetRect(&top, RULER, 0, c.right - W95_SCROLL, RULER);
    SetRect(&left, 0, RULER, RULER, c.bottom - STATUS_H);
    w95_fill(dc, &corner, W95_FACE);
    w95_fill(dc, &top, W95_WINDOW);
    w95_fill(dc, &left, W95_WINDOW);
    w95_hline(dc, 0, c.right, RULER - 1, W95_DKSHADOW);
    w95_vline(dc, RULER - 1, 0, c.bottom - STATUS_H, W95_DKSHADOW);
    if (g_app.prefs.units == 1) unit = v->doc->s.dpi;
    else if (g_app.prefs.units == 2) unit = v->doc->s.dpi / 2.54;
    /* Choose a tick step that leaves ~50 px between labels. */
    {
        static const double steps[] = { 1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 5000 };
        int i;
        step = 1;
        for (i = 0; i < 14; i++) if (steps[i] * unit * v->zoom >= 50) { step = (int)steps[i]; break; }
        if (i == 14) step = 10000;
    }
    for (k = 0; ; k++) {
        double dpos = k * step * unit;
        int x = ox + (int)(dpos * v->zoom), y = oy + (int)(dpos * v->zoom), t;
        char lb[16];
        RECT tr;
        if (x > top.right && y > left.bottom) break;
        if (dpos > PC_MAX(v->doc->s.w, v->doc->s.h) * 4) break;
        pc_snprintf(lb, sizeof lb, "%d", k * step);
        if (x >= top.left && x < top.right) {
            w95_vline(dc, x, 0, RULER - 1, W95_TEXT);
            SetRect(&tr, x + 2, 0, x + 50, 10);
            w95_text(dc, W95_FONT_SMALL, lb, &tr, DT_LEFT | DT_SINGLELINE, W95_TEXT);
            for (t = 1; t < 10; t++) {
                int xx = x + (int)(t * step * unit * v->zoom / 10);
                if (xx >= top.right) break;
                w95_vline(dc, xx, t == 5 ? RULER - 7 : RULER - 4, RULER - 1, W95_TEXT);
            }
        }
        if (y >= left.top && y < left.bottom) {
            w95_hline(dc, 0, RULER - 1, y, W95_TEXT);
            SetRect(&tr, 1, y + 1, RULER - 2, y + 30);
            w95_text(dc, W95_FONT_SMALL, lb, &tr, DT_LEFT | DT_WORDBREAK, W95_TEXT);
            for (t = 1; t < 10; t++) {
                int yy = y + (int)(t * step * unit * v->zoom / 10);
                if (yy >= left.bottom) break;
                w95_hline(dc, t == 5 ? RULER - 7 : RULER - 4, RULER - 1, yy, W95_TEXT);
            }
        }
    }
    /* Cursor position markers (dotted lines in PS). */
    if (v->mouse_in) {
        int k2;
        for (k2 = 0; k2 < RULER - 1; k2 += 2) { w95_pixel(dc, v->mouse.x, k2, W95_TEXT); w95_pixel(dc, k2, v->mouse.y, W95_TEXT); }
    }
}

static void status_rects(DocView *v, RECT *zoom, RECT *info, RECT *hs, RECT *vs, RECT *grip)
{
    RECT c;
    int ru = v->show_rulers ? RULER : 0;
    GetClientRect(v->hwnd, &c);
    SetRect(zoom, 0, c.bottom - STATUS_H, 52, c.bottom);
    SetRect(info, 52, c.bottom - STATUS_H, PC_MIN(52 + 150, c.right / 2), c.bottom);
    SetRect(hs, info->right, c.bottom - STATUS_H, c.right - W95_SCROLL, c.bottom);
    SetRect(vs, c.right - W95_SCROLL, ru, c.right, c.bottom - STATUS_H);
    SetRect(grip, c.right - W95_SCROLL, c.bottom - STATUS_H, c.right, c.bottom);
}

static void fmt_bytes(char *b, int n, size_t bytes)
{
    if (bytes >= 1024 * 1024) pc_snprintf(b, (size_t)n, "%.1fM", bytes / 1048576.0);
    else pc_snprintf(b, (size_t)n, "%dK", (int)((bytes + 1023) / 1024));
}

static void paint_status(DocView *v, HDC dc)
{
    RECT z, info, hs, vs, grip, cr;
    char b[96], b1[24], b2[24];
    status_rects(v, &z, &info, &hs, &vs, &grip);
    w95_fill(dc, &z, W95_FACE);
    w95_fill(dc, &info, W95_FACE);
    {
        RECT f = z;
        InflateRect(&f, -1, -1);
        w95_thin_sunken(dc, &f);
        f.right -= 3;
        pc_snprintf(b, sizeof b, v->zoom * 100 >= 10 ? "%.2f%%" : "%.3f%%", v->zoom * 100);
        w95_text(dc, W95_FONT_NORMAL, b, &f, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
    }
    {
        RECT f = info;
        size_t flat = (size_t)v->doc->s.w * v->doc->s.h * (v->doc->s.mode == PC_MODE_GRAY ? 1 : 3);
        InflateRect(&f, -1, -1);
        w95_thin_sunken(dc, &f);
        f.left += 3; f.right -= 12;
        if (v->status_mode == 1) pc_snprintf(b, sizeof b, "%d px x %d px (%d ppi)", v->doc->s.w, v->doc->s.h, (int)(v->doc->s.dpi + 0.5));
        else if (v->status_mode == 2) pc_snprintf(b, sizeof b, "%s", g_tool_info[g_app.tool].name);
        else { fmt_bytes(b1, sizeof b1, flat); fmt_bytes(b2, sizeof b2, pc_doc_memory(v->doc) * 3 / 4); pc_snprintf(b, sizeof b, "Doc: %s/%s", b1, b2); }
        w95_text(dc, W95_FONT_NORMAL, b, &f, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, W95_TEXT);
        w95_draw_arrow(dc, info.right - 7, (info.top + info.bottom) / 2, 3, 4, W95_TEXT);
    }
    view_canvas_rect(v, &cr);
    w95_draw_scrollbar(dc, &hs, 0, v->sx, cr.right - cr.left, PC_MAX(canvas_w(v), cr.right - cr.left), v->sb_drag == 1 ? v->sb_part : 0);
    w95_draw_scrollbar(dc, &vs, 1, v->sy, cr.bottom - cr.top, PC_MAX(canvas_h(v), cr.bottom - cr.top), v->sb_drag == 2 ? v->sb_part : 0);
    w95_fill(dc, &grip, W95_FACE);
    if (v->frame) w95_icon(dc, PC_ICON_SIZE_GRIP, grip.right - 13, grip.bottom - 13);
}

static void view_paint(DocView *v, HDC target)
{
    RECT c, cr;
    W95Buf b;
    HDC dc;
    GetClientRect(v->hwnd, &c);
    dc = w95_buf_begin(&b, target, &c);
    view_canvas_rect(v, &cr);
    paint_canvas(v, dc, &cr);
    {
        HRGN clip = CreateRectRgn(cr.left, cr.top, cr.right, cr.bottom);
        SelectClipRgn(dc, clip);
        tool_paint_overlay(v, dc);
        SelectClipRgn(dc, NULL);
        DeleteObject(clip);
    }
    if (!g_app.screen_mode) {
        if (v->show_rulers) paint_rulers(v, dc);
        paint_status(v, dc);
    }
    w95_buf_end(&b);
}

/* ---------------- cursors ---------------- */

void view_update_cursor(DocView *v)
{
    POINT p;
    RECT cr;
    if (!v || !v->hwnd) return;
    GetCursorPos(&p);
    ScreenToClient(v->hwnd, &p);
    view_canvas_rect(v, &cr);
    if (!PtInRect(&cr, p)) return;
    SetCursor(w95_cursor(tool_cursor(v)));
}

/* ---------------- scroll bar interaction ---------------- */

static void sb_step(DocView *v)
{
    RECT cr;
    int page, line = 16;
    view_canvas_rect(v, &cr);
    if (v->sb_drag == 1) {
        page = cr.right - cr.left;
        if (v->sb_part == 1) v->sx -= line; else if (v->sb_part == 2) v->sx += line;
        else if (v->sb_part == 3) v->sx -= page; else if (v->sb_part == 4) v->sx += page;
    } else {
        page = cr.bottom - cr.top;
        if (v->sb_part == 1) v->sy -= line; else if (v->sb_part == 2) v->sy += line;
        else if (v->sb_part == 3) v->sy -= page; else if (v->sb_part == 4) v->sy += page;
    }
    view_clamp_scroll(v);
    InvalidateRect(v->hwnd, NULL, FALSE);
    dock_refresh(R_NAV);
}

static void zoom_field_edit(DocView *v)
{
    RECT z, info, hs, vs, grip;
    char b[32];
    status_rects(v, &z, &info, &hs, &vs, &grip);
    pc_snprintf(b, sizeof b, "%.2f", v->zoom * 100);
    v->zoom_edit = CreateWindowExA(0, "EDIT", b, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_RIGHT | ES_AUTOHSCROLL,
                                   z.left + 1, z.top + 1, z.right - z.left - 2, z.bottom - z.top - 2, v->hwnd, (HMENU)50, w95_inst(), NULL);
    if (!v->zoom_edit) return;
    SendMessageA(v->zoom_edit, WM_SETFONT, (WPARAM)w95_font(W95_FONT_NORMAL), 0);
    SendMessageA(v->zoom_edit, EM_SETSEL, 0, -1);
    SetFocus(v->zoom_edit);
}

static void zoom_field_done(DocView *v, int apply)
{
    HWND e = v->zoom_edit;
    if (!e) return;
    v->zoom_edit = NULL;
    if (apply) {
        char b[32];
        GetWindowTextA(e, b, sizeof b);
        if (pc_atof(b) > 0) view_set_zoom(v, pc_atof(b) / 100.0, -1, -1);
    }
    DestroyWindow(e);
    SetFocus(g_app.main);
}


/* ---------------- view window ---------------- */

static LRESULT CALLBACK view_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DocView *v = view_of(hwnd);
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    if (!v) return DefWindowProcA(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        view_paint(v, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE:
        view_clamp_scroll(v);
        v->ants_key_valid = 0;
        InvalidateRect(hwnd, NULL, FALSE);
        dock_refresh(R_NAV);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT p;
            RECT cr;
            GetCursorPos(&p); ScreenToClient(hwnd, &p);
            view_canvas_rect(v, &cr);
            if (PtInRect(&cr, p)) { SetCursor(w95_cursor(tool_cursor(v))); return TRUE; }
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wp) == 50 && HIWORD(wp) == EN_KILLFOCUS) zoom_field_done(v, 1);
        return 0;
    case WM_MOUSEWHEEL: {
        /* WM_MOUSEWHEEL exists from Win98/NT4 with IntelliMouse; harmless elsewhere. */
        int delta = (short)HIWORD(wp);
        if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) {
            cmd_run(delta > 0 ? CMD_ZOOM_IN : CMD_ZOOM_OUT);
        } else if (GetKeyState(VK_SHIFT) < 0) view_scroll_to(v, v->sx - delta / 2, v->sy);
        else view_scroll_to(v, v->sx, v->sy - delta / 2);
        return 0;
    }
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_LBUTTONDBLCLK: {
        RECT z, info, hs, vs, grip, cr;
        POINT p = { x, y };
        int idx = index_of(v);
        if (idx != g_app.active) app_set_active(idx);
        SetFocus(g_app.main);
        dock_close_flyout();
        if (g_app.screen_mode) { tool_mouse(v, msg, x, y, wp); return 0; }
        status_rects(v, &z, &info, &hs, &vs, &grip);
        view_canvas_rect(v, &cr);
        if (PtInRect(&z, p) && msg == WM_LBUTTONDOWN) { zoom_field_edit(v); return 0; }
        if (PtInRect(&info, p) && msg == WM_LBUTTONDOWN) {
            static const MenuItem real[] = { { 1, CMD_COUNT + 1, "Document Sizes" }, { 1, CMD_COUNT + 2, "Document Dimensions" }, { 1, CMD_COUNT + 3, "Current Tool" } };
            POINT sp = { info.left, info.top };
            int c;
            ClientToScreen(hwnd, &sp);
            c = menu_popup_items(real, 3, sp.x, sp.y - 3 * W95_MENUITEM_H - 6, hwnd);
            if (c) { v->status_mode = c - CMD_COUNT - 1; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        if ((PtInRect(&hs, p) || PtInRect(&vs, p)) && msg != WM_RBUTTONDOWN) {
            int vert = PtInRect(&vs, p);
            int part = vert ? w95_scroll_hit(&vs, 1, v->sy, cr.bottom - cr.top, PC_MAX(canvas_h(v), cr.bottom - cr.top), x, y, &v->sb_thumb_off)
                            : w95_scroll_hit(&hs, 0, v->sx, cr.right - cr.left, PC_MAX(canvas_w(v), cr.right - cr.left), x, y, &v->sb_thumb_off);
            if (!part) return 0;
            v->sb_drag = vert ? 2 : 1; v->sb_part = part;
            SetCapture(hwnd);
            if (part != 5) { sb_step(v); SetTimer(hwnd, 7, 350, NULL); }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (PtInRect(&cr, p)) tool_mouse(v, msg, x, y, wp);
        return 0;
    }
    case WM_TIMER:
        if (wp == 7 && v->sb_drag && v->sb_part != 5) { SetTimer(hwnd, 7, 50, NULL); sb_step(v); }
        return 0;
    case WM_MOUSEMOVE:
        if (v->sb_drag) {
            if (v->sb_part == 5) {
                RECT z, info, hs, vs, grip, cr;
                status_rects(v, &z, &info, &hs, &vs, &grip);
                view_canvas_rect(v, &cr);
                if (v->sb_drag == 1) {
                    int tl = hs.right - hs.left - 2 * W95_SCROLL;
                    int tsz = PC_MAX(8, (int)((double)tl * (cr.right - cr.left) / PC_MAX(1, canvas_w(v))));
                    v->sx = (int)((double)(x - hs.left - W95_SCROLL - v->sb_thumb_off) * (canvas_w(v) - (cr.right - cr.left)) / PC_MAX(1, tl - tsz));
                } else {
                    int tl = vs.bottom - vs.top - 2 * W95_SCROLL;
                    int tsz = PC_MAX(8, (int)((double)tl * (cr.bottom - cr.top) / PC_MAX(1, canvas_h(v))));
                    v->sy = (int)((double)(y - vs.top - W95_SCROLL - v->sb_thumb_off) * (canvas_h(v) - (cr.bottom - cr.top)) / PC_MAX(1, tl - tsz));
                }
                view_clamp_scroll(v);
                InvalidateRect(hwnd, NULL, FALSE);
                dock_refresh(R_NAV);
            }
            return 0;
        }
        {
            float dx, dy;
            RECT cr;
            POINT p = { x, y };
            view_canvas_rect(v, &cr);
            v->mouse.x = x; v->mouse.y = y;
            v->mouse_in = PtInRect(&cr, p);
            view_screen_to_doc(v, x, y, &dx, &dy);
            app_update_info(v, dx, dy);
            if (v->show_rulers) InvalidateRect(hwnd, NULL, FALSE);
        }
        tool_mouse(v, msg, x, y, wp);
        return 0;
    case WM_LBUTTONUP: case WM_RBUTTONUP:
        if (v->sb_drag) { v->sb_drag = 0; KillTimer(hwnd, 7); ReleaseCapture(); InvalidateRect(hwnd, NULL, FALSE); return 0; }
        tool_mouse(v, msg, x, y, wp);
        return 0;
    case WM_CAPTURECHANGED:
        if (v->sb_drag) { v->sb_drag = 0; KillTimer(hwnd, 7); }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

HWND view_create(DocView *v)
{
    static int reg;
    if (!reg) {
        w95_register_class("PCView", view_proc, CS_DBLCLKS, NULL);
        w95_register_class("PCDocFrame", frame_proc, CS_DBLCLKS, NULL);
        reg = 1;
    }
    v->hwnd = CreateWindowExA(0, "PCView", "", WS_CHILD | WS_CLIPCHILDREN, 0, 0, 10, 10, g_area, NULL, w95_inst(), NULL);
    return v->hwnd;
}

void view_destroy(DocView *v)
{
    if (v->hwnd) { DestroyWindow(v->hwnd); v->hwnd = NULL; }
    if (v->frame) { HWND f = v->frame; v->frame = NULL; DestroyWindow(f); }
    free(v->ants); v->ants = NULL;
}

/* ---------------- floating document frames ---------------- */

static LRESULT CALLBACK frame_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DocView *v = view_of(hwnd);
    LRESULT res;
    if (v && v->frame == hwnd) {
        if (msg == WM_NCLBUTTONDOWN || msg == WM_NCRBUTTONDOWN) {
            int idx = index_of(v);
            if (idx != g_app.active) app_set_active(idx);
        }
        if (w95_frame_proc(hwnd, &v->fr, msg, wp, lp, &res)) {
            if (msg == WM_NCLBUTTONUP || msg == WM_LBUTTONUP) {}
            return res;
        }
    }
    switch (msg) {
    case WM_SIZE:
        if (v && v->hwnd) {
            RECT c;
            GetClientRect(hwnd, &c);
            SetWindowPos(v->hwnd, NULL, 0, 0, c.right, c.bottom, SWP_NOZORDER);
            if (v->fr.maximized) {
                /* Maximizing a floating document docks it back into tabs (CS6). */
                v->fr.maximized = 0;
                PostMessageA(g_app.main, WM_COMMAND, CMD_CONSOLIDATE, 0);
            }
        }
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE:
        if (v) { int idx = index_of(v); if (idx != g_app.active) app_set_active(idx); }
        return MA_ACTIVATE;
    case WM_CLOSE:
        if (v) app_close_view(index_of(v), 1);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void view_float(DocView *v, int floating)
{
    char t[300];
    if (floating && !v->frame) {
        RECT ar;
        int w, h, n = 0, i;
        GetClientRect(g_area, &ar);
        for (i = 0; i < g_app.nviews; i++) if (g_app.views[i]->frame) n++;
        w = PC_MIN(PC_MAX(200, (int)(v->doc->s.w * v->zoom) + W95_SCROLL), ar.right - 40);
        h = PC_MIN(PC_MAX(150, (int)(v->doc->s.h * v->zoom) + STATUS_H), ar.bottom - 40);
        w95_frame_adjust(W95_FRAME_SIZABLE | W95_FRAME_MINMAX | W95_FRAME_CLOSE, &w, &h);
        view_title(v, t, sizeof t);
        v->frame = CreateWindowExA(0, "PCDocFrame", t, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                   10 + (n % 8) * 22, 10 + (n % 8) * 22, w, h, g_area, NULL, w95_inst(), NULL);
        if (!v->frame) return;
        w95_frame_attach(v->frame, &v->fr, W95_FRAME_SIZABLE | W95_FRAME_MINMAX | W95_FRAME_CLOSE);
        v->fr.min_w = 140; v->fr.min_h = 100;
        SetParent(v->hwnd, v->frame);
        ShowWindow(v->hwnd, SW_SHOW);
        {
            RECT c;
            GetClientRect(v->frame, &c);
            SetWindowPos(v->hwnd, NULL, 0, 0, c.right, c.bottom, SWP_NOZORDER);
        }
    } else if (!floating && v->frame) {
        HWND f = v->frame;
        SetParent(v->hwnd, g_area);
        v->frame = NULL;
        DestroyWindow(f);
    }
    docarea_layout();
}

/* ---------------- document area ---------------- */

static int tab_x[MAX_VIEWS + 1];

static void tab_layout(HDC dc)
{
    int i, x = 2;
    for (i = 0; i < g_app.nviews; i++) {
        char t[300];
        tab_x[i] = x;
        if (g_app.views[i]->frame) continue;
        view_title(g_app.views[i], t, sizeof t);
        x += PC_MIN(w95_text_width(dc, i == g_app.active ? W95_FONT_BOLD : W95_FONT_NORMAL, t) + 30, 260);
    }
    tab_x[g_app.nviews] = x;
}

static int tab_end(int i)
{
    int k;
    for (k = i + 1; k <= g_app.nviews; k++) if (k == g_app.nviews || !g_app.views[k]->frame) return tab_x[k];
    return tab_x[g_app.nviews];
}

static void area_paint(HWND hwnd, HDC target)
{
    RECT c;
    W95Buf b;
    HDC dc;
    int i;
    GetClientRect(hwnd, &c);
    dc = w95_buf_begin(&b, target, &c);
    w95_fill(dc, &c, g_app.screen_mode == 2 ? RGB(0, 0, 0) : W95_WORKSPACE);
    if (has_tabs()) {
        RECT strip;
        SetRect(&strip, 0, 0, c.right, TABS_H);
        w95_fill(dc, &strip, W95_FACE);
        w95_hline(dc, 0, c.right, TABS_H - 1, W95_HILIGHT);
        tab_layout(dc);
        /* Windows 95 property-sheet tabs: the active one is taller and joins the page. */
        for (i = 0; i < g_app.nviews; i++) {
            char t[300];
            RECT tr, xr;
            int act = i == g_app.active;
            if (g_app.views[i]->frame) continue;
            SetRect(&tr, tab_x[i], act ? 0 : 2, tab_end(i), TABS_H - (act ? 0 : 1));
            if (act) { tr.left -= 2; tr.right += 2; }
            w95_fill(dc, &tr, W95_FACE);
            w95_vline(dc, tr.left, tr.top + 2, tr.bottom, W95_HILIGHT);
            w95_pixel(dc, tr.left + 1, tr.top + 1, W95_HILIGHT);
            w95_hline(dc, tr.left + 2, tr.right - 2, tr.top, W95_HILIGHT);
            w95_pixel(dc, tr.right - 2, tr.top + 1, W95_DKSHADOW);
            w95_vline(dc, tr.right - 1, tr.top + 2, tr.bottom, W95_DKSHADOW);
            w95_vline(dc, tr.right - 2, tr.top + 2, tr.bottom, W95_SHADOW);
            if (!act) w95_hline(dc, tr.left, tr.right, TABS_H - 1, W95_HILIGHT);
            view_title(g_app.views[i], t, sizeof t);
            {
                RECT tt = tr;
                tt.left += 6; tt.right -= 18;
                w95_text(dc, act ? W95_FONT_BOLD : W95_FONT_NORMAL, t, &tt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, W95_TEXT);
            }
            /* small close box */
            SetRect(&xr, tr.right - 15, tr.top + 3, tr.right - 4, tr.top + 13);
            {
                int k;
                for (k = 0; k < 6; k++) { w95_pixel(dc, xr.left + 2 + k, xr.top + 2 + k, W95_TEXT); w95_pixel(dc, xr.left + 7 - k, xr.top + 2 + k, W95_TEXT); }
            }
        }
    }
    w95_buf_end(&b);
}

void docarea_layout(void)
{
    RECT c;
    int i, top;
    DocView *act = app_view();
    if (!g_area) return;
    GetClientRect(g_area, &c);
    top = has_tabs() ? TABS_H : 0;
    for (i = 0; i < g_app.nviews; i++) {
        DocView *v = g_app.views[i];
        if (v->frame) {
            ShowWindow(v->frame, g_app.screen_mode ? (v == act ? SW_SHOW : SW_HIDE) : SW_SHOW);
            if (g_app.screen_mode && v == act) {
                SetParent(v->hwnd, g_area);
                SetWindowPos(v->hwnd, HWND_TOP, 0, 0, c.right, c.bottom, SWP_SHOWWINDOW);
            } else if (GetParent(v->hwnd) != v->frame) {
                RECT fc;
                SetParent(v->hwnd, v->frame);
                GetClientRect(v->frame, &fc);
                SetWindowPos(v->hwnd, NULL, 0, 0, fc.right, fc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
            }
            if (!g_app.screen_mode) {
                char t[300];
                view_title(v, t, sizeof t);
                SetWindowTextA(v->frame, t);
                v->fr.active = v == act;
                SendMessageA(v->frame, WM_NCPAINT, 1, 0);
            }
            continue;
        }
        if (v == act) SetWindowPos(v->hwnd, HWND_TOP, 0, top, c.right, PC_MAX(1, c.bottom - top), SWP_SHOWWINDOW);
        else ShowWindow(v->hwnd, SW_HIDE);
    }
    if (g_app.screen_mode && act && act->frame) ShowWindow(act->frame, SW_HIDE);
    InvalidateRect(g_area, NULL, FALSE);
}

void docarea_float_all(int floating)
{
    int i;
    for (i = 0; i < g_app.nviews; i++) view_float(g_app.views[i], floating);
    docarea_layout();
}

void docarea_cascade(void)
{
    int i, n = 0;
    RECT c;
    docarea_float_all(1);
    GetClientRect(g_area, &c);
    for (i = 0; i < g_app.nviews; i++) {
        DocView *v = g_app.views[i];
        int w = c.right * 2 / 3, h = c.bottom * 2 / 3;
        SetWindowPos(v->frame, HWND_TOP, n * 20, n * 20, w, h, 0);
        n = (n + 1) % 10;
    }
    if (app_view() && app_view()->frame) SetWindowPos(app_view()->frame, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void docarea_tile(int vertical)
{
    int i, n = g_app.nviews;
    RECT c;
    if (!n) return;
    docarea_float_all(1);
    GetClientRect(g_area, &c);
    for (i = 0; i < n; i++) {
        DocView *v = g_app.views[i];
        if (vertical) SetWindowPos(v->frame, NULL, c.right * i / n, 0, c.right / n, c.bottom, SWP_NOZORDER);
        else SetWindowPos(v->frame, NULL, 0, c.bottom * i / n, c.right, c.bottom / n, SWP_NOZORDER);
    }
}

static LRESULT CALLBACK area_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        area_paint(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: docarea_layout(); return 0;
    case WM_LBUTTONDOWN: case WM_MBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), i;
        HDC dc;
        if (!has_tabs() || y >= TABS_H) {
            /* Double-click the empty workspace opens a file, as in Photoshop. */
            return 0;
        }
        dc = GetDC(hwnd);
        tab_layout(dc);
        ReleaseDC(hwnd, dc);
        for (i = 0; i < g_app.nviews; i++) {
            if (g_app.views[i]->frame) continue;
            if (x >= tab_x[i] && x < tab_end(i)) {
                if (msg == WM_MBUTTONDOWN || x >= tab_end(i) - 16) { app_close_view(i, 1); return 0; }
                app_set_active(i);
                /* Dragging a tab down out of the strip floats the document. */
                SetCapture(hwnd);
                return 0;
            }
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        if ((short)HIWORD(lp) >= TABS_H || !has_tabs()) cmd_run(CMD_OPEN);
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == hwnd && (short)HIWORD(lp) > TABS_H + 20) {
            DocView *v = app_view();
            ReleaseCapture();
            if (v && !v->frame) {
                POINT p = { (short)LOWORD(lp), (short)HIWORD(lp) };
                view_float(v, 1);
                if (v->frame) SetWindowPos(v->frame, HWND_TOP, PC_MAX(0, p.x - 60), PC_MAX(0, p.y - 10), 0, 0, SWP_NOSIZE);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == hwnd) ReleaseCapture();
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

HWND docarea_create(HWND parent)
{
    w95_register_class("PCDocArea", area_proc, CS_DBLCLKS, NULL);
    g_area = CreateWindowExA(0, "PCDocArea", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 10, 10, parent, NULL, w95_inst(), NULL);
    return g_area;
}
