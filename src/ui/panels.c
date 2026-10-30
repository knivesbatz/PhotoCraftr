/*
 * Panel contents.  Photoshop 5.0 internals (row layouts, eye column,
 * navy selection, triangle sliders, swatch grid, brush grid) inside CS6
 * placements.  Everything reads and edits the active document.
 */
#include "app.h"
#include "dlg.h"

void options_set_readout(const char *s);
int actions_panel_rows(void);
void actions_panel_paint(HDC dc, const RECT *r, int scroll);
void actions_panel_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y, int scroll);
void actions_panel_menu(HWND hwnd, int sx, int sy);
void actions_panel_buttons(int which);
void dock_classic_workspace(void);

static const char *const k_names[P_COUNT] = {
    "Navigator", "Info", "Color", "Swatches", "Brushes", "Layers", "Channels", "Paths", "History", "Actions"
};
static const int k_icons[P_COUNT] = {
    PC_ICON_PANEL_NAVIGATOR, PC_ICON_PANEL_INFO, PC_ICON_PANEL_COLOR, PC_ICON_PANEL_SWATCHES, PC_ICON_PANEL_BRUSHES,
    PC_ICON_PANEL_LAYERS, PC_ICON_PANEL_CHANNELS, PC_ICON_PANEL_PATHS, PC_ICON_PANEL_HISTORY, PC_ICON_PANEL_ACTIONS
};
static const int k_pref_h[P_COUNT] = { 140, 92, 92, 92, 120, 240, 150, 120, 180, 180 };

const char *panel_name(int p) { return (p >= 0 && p < P_COUNT) ? k_names[p] : ""; }
int panel_icon(int p) { return (p >= 0 && p < P_COUNT) ? k_icons[p] : -1; }
int panel_min_height(int p) { (void)p; return 40; }
int panel_pref_height(int p) { return (p >= 0 && p < P_COUNT) ? k_pref_h[p] : 120; }

static int g_scroll[P_COUNT];
static int g_mask_target;            /* Layers: editing the layer mask */
int panels_mask_target(void) { return g_mask_target; }
void panels_set_mask_target(int on) { g_mask_target = on; dock_refresh(R_LAYERS); }

/* ---------------- shared helpers ---------------- */

static void bottom_bar(HDC dc, const RECT *r, const int *icons, int n, RECT *btns)
{
    RECT bar;
    int k, x;
    SetRect(&bar, r->left, r->bottom - 20, r->right, r->bottom);
    w95_fill(dc, &bar, W95_FACE);
    w95_hline(dc, bar.left, bar.right, bar.top, W95_SHADOW);
    w95_hline(dc, bar.left, bar.right, bar.top + 1, W95_HILIGHT);
    x = r->right - 4 - n * 22;
    for (k = 0; k < n; k++) {
        SetRect(&btns[k], x + k * 22, bar.top + 3, x + k * 22 + 21, bar.bottom - 1);
        w95_draw_icon_button(dc, &btns[k], icons[k], 0, app_doc() == NULL);
    }
}

static int bottom_hit(const RECT *r, int n, int x, int y)
{
    int x0 = r->right - 4 - n * 22;
    if (y < r->bottom - 18 || x < x0 || x >= x0 + n * 22) return -1;
    return (x - x0) / 22;
}

static void list_area(const RECT *r, int top, int has_bottom, RECT *out)
{
    SetRect(out, r->left, r->top + top, r->right, r->bottom - (has_bottom ? 20 : 0));
}

/* Draw a vertical scroll bar when rows overflow; returns usable width. */
static int list_scroll(HDC dc, int p, const RECT *area, int rows, int row_h)
{
    int vis = (area->bottom - area->top) / row_h;
    if (rows <= vis) { g_scroll[p] = 0; return area->right - area->left; }
    g_scroll[p] = PC_CLAMP(g_scroll[p], 0, rows - vis);
    if (dc) {
        RECT sb;
        SetRect(&sb, area->right - W95_SCROLL, area->top, area->right, area->bottom);
        w95_draw_scrollbar(dc, &sb, 1, g_scroll[p], vis, rows, 0);
    }
    return area->right - area->left - W95_SCROLL;
}

static int list_scroll_click(int p, const RECT *area, int rows, int row_h, int x, int y)
{
    int vis = (area->bottom - area->top) / row_h;
    RECT sb;
    int part;
    if (rows <= vis || x < area->right - W95_SCROLL) return 0;
    SetRect(&sb, area->right - W95_SCROLL, area->top, area->right, area->bottom);
    part = w95_scroll_hit(&sb, 1, g_scroll[p], vis, rows, x, y, NULL);
    if (part == 1) g_scroll[p]--;
    else if (part == 2) g_scroll[p]++;
    else if (part == 3) g_scroll[p] -= vis;
    else if (part == 4) g_scroll[p] += vis;
    else if (part == 5) {
        int tl = (sb.bottom - sb.top) - 2 * W95_SCROLL;
        g_scroll[p] = (int)((double)(y - sb.top - W95_SCROLL) / PC_MAX(1, tl) * rows) - vis / 2;
    }
    g_scroll[p] = PC_CLAMP(g_scroll[p], 0, rows - vis);
    return 1;
}

/* Small composited thumbnail of an RGBA (or 1-channel) buffer. */
static void draw_thumb(HDC dc, const RECT *box, const PcBuf *b, int checker)
{
    int bw = box->right - box->left, bh = box->bottom - box->top, tw, th, x, y, ox, oy;
    BITMAPINFO bi;
    u8 *px;
    if (!b || bw <= 2 || bh <= 2) return;
    if ((double)b->w / b->h > (double)bw / bh) { tw = bw; th = PC_MAX(1, b->h * bw / b->w); }
    else { th = bh; tw = PC_MAX(1, b->w * bh / b->h); }
    ox = box->left + (bw - tw) / 2; oy = box->top + (bh - th) / 2;
    px = (u8 *)malloc((size_t)tw * th * 4);
    if (!px) return;
    for (y = 0; y < th; y++)
        for (x = 0; x < tw; x++) {
            int sx = x * b->w / tw, sy = y * b->h / th;
            u8 *o = px + ((size_t)y * tw + x) * 4;
            if (b->ch == 1) { u8 v = b->px[(size_t)sy * b->w + sx]; o[0] = o[1] = o[2] = v; }
            else {
                const u8 *s = b->px + ((size_t)sy * b->w + sx) * 4;
                int a = s[3], bg = checker ? ((((x >> 2) ^ (y >> 2)) & 1) ? 204 : 255) : 255;
                o[0] = (u8)((s[2] * a + bg * (255 - a)) / 255);
                o[1] = (u8)((s[1] * a + bg * (255 - a)) / 255);
                o[2] = (u8)((s[0] * a + bg * (255 - a)) / 255);
            }
            o[3] = 0;
        }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = tw; bi.bmiHeader.biHeight = -th;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, ox, oy, (DWORD)tw, (DWORD)th, 0, 0, 0, (UINT)th, px, &bi, DIB_RGB_COLORS);
    free(px);
    w95_rect(dc, ox - 1, oy - 1, ox + tw + 1, oy + th + 1, W95_DKSHADOW);
}

/* ---------------- Navigator ---------------- */

static RECT g_nav_thumb;
static int g_nav_drag;

static void nav_geometry(const RECT *r, RECT *thumb, RECT *row)
{
    SetRect(thumb, r->left + 4, r->top + 4, r->right - 4, r->bottom - 26);
    SetRect(row, r->left + 2, r->bottom - 22, r->right - 2, r->bottom - 2);
}

static void nav_paint(HDC dc, const RECT *r)
{
    RECT th, row, img;
    DocView *v = app_view();
    nav_geometry(r, &th, &row);
    w95_fill(dc, &th, W95_SHADOW);
    w95_sunken(dc, &th);
    InflateRect(&th, -4, -4);
    g_nav_thumb = th;
    if (v) {
        PcBuf *c = v->comp;
        int bw = th.right - th.left, bh = th.bottom - th.top, tw, tht;
        if (!c || v->comp_rev != v->doc->revision) { pc_buf_unref(v->comp); v->comp = pc_doc_composite(v->doc); v->comp_rev = v->doc->revision; c = v->comp; }
        draw_thumb(dc, &th, c, 1);
        if ((double)v->doc->s.w / v->doc->s.h > (double)bw / bh) { tw = bw; tht = v->doc->s.h * bw / v->doc->s.w; }
        else { tht = bh; tw = v->doc->s.w * bh / v->doc->s.h; }
        SetRect(&img, th.left + (bw - tw) / 2, th.top + (bh - tht) / 2, 0, 0);
        img.right = img.left + tw; img.bottom = img.top + tht;
        g_nav_thumb = img;
        {
            /* Red view box (#FF4A4A, two pixels). */
            RECT cr, vb;
            float x0, y0, x1, y1;
            view_canvas_rect(v, &cr);
            view_screen_to_doc(v, cr.left, cr.top, &x0, &y0);
            view_screen_to_doc(v, cr.right, cr.bottom, &x1, &y1);
            x0 = PC_MAX(0, x0); y0 = PC_MAX(0, y0); x1 = PC_MIN(v->doc->s.w, x1); y1 = PC_MIN(v->doc->s.h, y1);
            SetRect(&vb, img.left + (int)(x0 * tw / v->doc->s.w), img.top + (int)(y0 * tht / v->doc->s.h),
                    img.left + (int)(x1 * tw / v->doc->s.w), img.top + (int)(y1 * tht / v->doc->s.h));
            w95_rect(dc, vb.left, vb.top, vb.right, vb.bottom, RGB(255, 74, 74));
            w95_rect(dc, vb.left + 1, vb.top + 1, vb.right - 1, vb.bottom - 1, RGB(255, 74, 74));
        }
    }
    /* Zoom row: [ zoom% ] small mountains ---slider--- big mountains */
    {
        RECT sl;
        int x0 = row.left + 56, x1 = row.right - 22, tx;
        double z = v ? v->zoom : 1.0;
        w95_icon(dc, PC_ICON_ZOOM_OUT_MOUNTAIN, x0 - 14, row.top + 6);
        w95_icon(dc, PC_ICON_ZOOM_IN_MOUNTAIN, x1 + 3, row.top + 4);
        SetRect(&sl, x0, row.top + 8, x1, row.top + 11);
        w95_thin_sunken(dc, &sl);
        /* log scale 1%..1600% */
        tx = x0 + (int)((log(z * 100) - log(1.0)) / (log(1600.0) - log(1.0)) * (x1 - x0));
        tx = PC_CLAMP(tx, x0, x1);
        {
            int k;
            for (k = 0; k < 5; k++) { w95_hline(dc, tx - k, tx + k + 1, row.top + 12 + k, k == 4 ? W95_TEXT : W95_FACE); w95_pixel(dc, tx - k, row.top + 12 + k, W95_TEXT); w95_pixel(dc, tx + k, row.top + 12 + k, W95_TEXT); }
        }
    }
}

static void nav_scroll_to(int x, int y)
{
    DocView *v = app_view();
    RECT cr;
    float dx, dy;
    if (!v || g_nav_thumb.right <= g_nav_thumb.left) return;
    dx = (float)(x - g_nav_thumb.left) * v->doc->s.w / (g_nav_thumb.right - g_nav_thumb.left);
    dy = (float)(y - g_nav_thumb.top) * v->doc->s.h / (g_nav_thumb.bottom - g_nav_thumb.top);
    view_canvas_rect(v, &cr);
    view_scroll_to(v, (int)(dx * v->zoom) - (cr.right - cr.left) / 2, (int)(dy * v->zoom) - (cr.bottom - cr.top) / 2);
}

static void nav_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y)
{
    RECT th, row;
    DocView *v = app_view();
    nav_geometry(r, &th, &row);
    if (!v) return;
    if (msg == WM_LBUTTONDOWN) {
        POINT p = { x, y };
        if (PtInRect(&th, p)) { g_nav_drag = 1; nav_scroll_to(x, y); }
        else if (y >= row.top) {
            int x0 = row.left + 56, x1 = row.right - 22;
            if (x < x0 && x >= x0 - 16) cmd_run(CMD_ZOOM_OUT);
            else if (x > x1) cmd_run(CMD_ZOOM_IN);
            else if (x >= x0) g_nav_drag = 2;
        }
    }
    if ((msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN) && g_nav_drag == 1) nav_scroll_to(x, y);
    if ((msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN) && g_nav_drag == 2) {
        int x0 = row.left + 56, x1 = row.right - 22;
        double t = (double)PC_CLAMP(x, x0, x1) - x0;
        double z = exp(t / PC_MAX(1, x1 - x0) * log(1600.0)) / 100.0;
        view_set_zoom(v, z, -1, -1);
    }
    if (msg == WM_LBUTTONUP) g_nav_drag = 0;
    (void)hwnd;
}

/* ---------------- Info ---------------- */

static struct { int valid; int x, y, r, g, b, a; int sel_w, sel_h; float angle, dist; int measure; } g_info;

void app_update_info(DocView *v, float dx, float dy)
{
    PcRect sr;
    g_info.valid = 0;
    if (v && dx >= 0 && dy >= 0 && dx < v->doc->s.w && dy < v->doc->s.h) {
        u32 c;
        int x = (int)dx, y = (int)dy;
        if (!v->comp || v->comp_rev != v->doc->revision) { pc_buf_unref(v->comp); v->comp = pc_doc_composite(v->doc); v->comp_rev = v->doc->revision; }
        if (v->comp) {
            const u8 *p = v->comp->px + ((size_t)y * v->doc->s.w + x) * 4;
            c = pc_rgb(p[0], p[1], p[2]);
            g_info.a = p[3];
        } else c = 0;
        g_info.valid = 1; g_info.x = x; g_info.y = y;
        g_info.r = pc_r(c); g_info.g = pc_g(c); g_info.b = pc_b(c);
    }
    g_info.sel_w = g_info.sel_h = 0;
    if (v && pc_sel_bounds(v->doc, &sr)) { g_info.sel_w = sr.x1 - sr.x0; g_info.sel_h = sr.y1 - sr.y0; }
    g_info.measure = v && v->measure_active;
    if (g_info.measure) {
        float mx = v->measure[2] - v->measure[0], my = v->measure[3] - v->measure[1];
        g_info.angle = (float)(atan2(-my, mx) * 180.0 / M_PI);
        g_info.dist = (float)sqrt(mx * mx + my * my);
        {
            char buf[160];
            pc_snprintf(buf, sizeof buf, "X: %d   Y: %d   W: %d   H: %d   A: %.1f\xB0   D: %.1f",
                        (int)v->measure[0], (int)v->measure[1], (int)mx, (int)my, g_info.angle, g_info.dist);
            options_set_readout(buf);
        }
    }
    dock_refresh(R_INFO);
}

static void info_paint(HDC dc, const RECT *r)
{
    char b[64];
    RECT q, cell;
    int c, m, y, k, mid = (r->left + r->right) / 2;
    /* Two readout quadrants separated by etched lines, as in PS5. */
    w95_etched_hline(dc, r->left + 2, r->right - 2, r->top + 46);
    w95_vline(dc, mid, r->top + 4, r->bottom - 4, W95_SHADOW); w95_vline(dc, mid + 1, r->top + 4, r->bottom - 4, W95_HILIGHT);
    SetRect(&q, r->left + 6, r->top + 4, mid - 4, r->top + 17);
    if (g_info.valid) {
        static const char *const lbl[3] = { "R:", "G:", "B:" };
        int vals[3] = { g_info.r, g_info.g, g_info.b };
        for (k = 0; k < 3; k++) {
            cell = q; OffsetRect(&cell, 0, k * 13);
            w95_text(dc, W95_FONT_NORMAL, lbl[k], &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
            pc_snprintf(b, sizeof b, "%d", vals[k]);
            w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT);
        }
        pc_rgb_to_cmyk(g_info.r, g_info.g, g_info.b, &c, &m, &y, &k);
        {
            static const char *const l2[4] = { "C:", "M:", "Y:", "K:" };
            int v2[4] = { c, m, y, k }, j;
            for (j = 0; j < 4; j++) {
                SetRect(&cell, mid + 6, r->top + 2 + j * 11, r->right - 6, r->top + 13 + j * 11);
                w95_text(dc, W95_FONT_SMALL, l2[j], &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
                pc_snprintf(b, sizeof b, "%d%%", v2[j]);
                w95_text(dc, W95_FONT_SMALL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT);
            }
        }
    }
    SetRect(&cell, r->left + 6, r->top + 50, mid - 4, r->top + 63);
    pc_snprintf(b, sizeof b, "X:"); w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
    if (g_info.valid) { pc_snprintf(b, sizeof b, "%d", g_info.x); w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT); }
    OffsetRect(&cell, 0, 13);
    w95_text(dc, W95_FONT_NORMAL, "Y:", &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
    if (g_info.valid) { pc_snprintf(b, sizeof b, "%d", g_info.y); w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT); }
    SetRect(&cell, mid + 6, r->top + 50, r->right - 6, r->top + 63);
    if (g_info.measure) {
        w95_text(dc, W95_FONT_NORMAL, "A:", &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
        pc_snprintf(b, sizeof b, "%.1f\xB0", g_info.angle); w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT);
        OffsetRect(&cell, 0, 13);
        w95_text(dc, W95_FONT_NORMAL, "D:", &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
        pc_snprintf(b, sizeof b, "%.1f", g_info.dist); w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT);
    } else {
        w95_text(dc, W95_FONT_NORMAL, "W:", &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
        if (g_info.sel_w) { pc_snprintf(b, sizeof b, "%d", g_info.sel_w); w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT); }
        OffsetRect(&cell, 0, 13);
        w95_text(dc, W95_FONT_NORMAL, "H:", &cell, DT_LEFT | DT_SINGLELINE, W95_TEXT);
        if (g_info.sel_h) { pc_snprintf(b, sizeof b, "%d", g_info.sel_h); w95_text(dc, W95_FONT_NORMAL, b, &cell, DT_RIGHT | DT_SINGLELINE, W95_TEXT); }
    }
}

/* ---------------- Color ---------------- */

static int g_color_drag = -1;
static int g_color_bg_target;     /* editing the background colour */

static void color_slider_rect(const RECT *r, int k, RECT *out) { SetRect(out, r->left + 62, r->top + 6 + k * 18, r->right - 12, r->top + 6 + k * 18 + 14); }

static void color_paint(HDC dc, const RECT *r)
{
    static const char *const lbl[3] = { "R", "G", "B" };
    u32 c = g_color_bg_target ? g_app.bg : g_app.fg;
    int k, x;
    RECT fg, bg, bar;
    /* FG/BG squares at the left (the target one framed). */
    SetRect(&bg, r->left + 16, r->top + 18, r->left + 36, r->top + 38);
    SetRect(&fg, r->left + 6, r->top + 8, r->left + 26, r->top + 28);
    w95_fill(dc, &bg, RGB(pc_r(g_app.bg), pc_g(g_app.bg), pc_b(g_app.bg)));
    w95_rect(dc, bg.left, bg.top, bg.right, bg.bottom, g_color_bg_target ? W95_DKSHADOW : W95_SHADOW);
    if (g_color_bg_target) w95_rect(dc, bg.left - 1, bg.top - 1, bg.right + 1, bg.bottom + 1, W95_DKSHADOW);
    w95_fill(dc, &fg, RGB(pc_r(g_app.fg), pc_g(g_app.fg), pc_b(g_app.fg)));
    w95_rect(dc, fg.left, fg.top, fg.right, fg.bottom, !g_color_bg_target ? W95_DKSHADOW : W95_SHADOW);
    if (!g_color_bg_target) w95_rect(dc, fg.left - 1, fg.top - 1, fg.right + 1, fg.bottom + 1, W95_DKSHADOW);
    for (k = 0; k < 3; k++) {
        RECT s, t;
        int v = k == 0 ? pc_r(c) : k == 1 ? pc_g(c) : pc_b(c), tx;
        color_slider_rect(r, k, &s);
        SetRect(&t, s.left - 18, s.top - 3, s.left - 8, s.bottom);
        w95_text(dc, W95_FONT_NORMAL, lbl[k], &t, DT_LEFT | DT_SINGLELINE, W95_TEXT);
        /* Ramp showing what this channel does with the others fixed (PS5 dynamic sliders). */
        for (x = s.left; x < s.right; x++) {
            int t2 = (x - s.left) * 255 / PC_MAX(1, s.right - s.left - 1);
            int rr = k == 0 ? t2 : pc_r(c), gg = k == 1 ? t2 : pc_g(c), bb = k == 2 ? t2 : pc_b(c);
            w95_vline(dc, x, s.top, s.top + 6, RGB(rr, gg, bb));
        }
        w95_rect(dc, s.left - 1, s.top - 1, s.right + 1, s.top + 7, W95_SHADOW);
        tx = s.left + v * (s.right - s.left - 1) / 255;
        {
            int q;
            for (q = 0; q < 5; q++) { w95_hline(dc, tx - q, tx + q + 1, s.top + 7 + q, q == 4 ? W95_TEXT : W95_FACE); w95_pixel(dc, tx - q, s.top + 7 + q, W95_TEXT); w95_pixel(dc, tx + q, s.top + 7 + q, W95_TEXT); }
        }
    }
    /* Colour bar (spectrum) at the bottom. */
    SetRect(&bar, r->left + 6, r->bottom - 16, r->right - 6, r->bottom - 4);
    for (x = bar.left; x < bar.right; x++) {
        int y, rr, gg, bb;
        float h = (float)(x - bar.left) * 360 / PC_MAX(1, bar.right - bar.left);
        for (y = bar.top; y < bar.bottom; y++) {
            float t = (float)(y - bar.top) / PC_MAX(1, bar.bottom - bar.top - 1);
            pc_hsb_to_rgb(h, t < 0.5f ? 1 : 1 - (t - 0.5f) * 2, t < 0.5f ? t * 2 : 1, &rr, &gg, &bb);
            w95_pixel(dc, x, y, RGB(rr, gg, bb));
        }
    }
    w95_rect(dc, bar.left - 1, bar.top - 1, bar.right + 1, bar.bottom + 1, W95_SHADOW);
}

static void color_set(u32 c)
{
    if (g_color_bg_target) app_set_colors(g_app.fg, c);
    else app_set_colors(c, g_app.bg);
}

static void color_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y)
{
    RECT bar, fg, bg;
    POINT p = { x, y };
    int k;
    SetRect(&bar, r->left + 6, r->bottom - 16, r->right - 6, r->bottom - 4);
    SetRect(&fg, r->left + 6, r->top + 8, r->left + 26, r->top + 28);
    SetRect(&bg, r->left + 16, r->top + 18, r->left + 36, r->top + 38);
    if (msg == WM_LBUTTONDBLCLK && (PtInRect(&fg, p) || PtInRect(&bg, p))) {
        u32 c = PtInRect(&fg, p) ? g_app.fg : g_app.bg;
        g_color_bg_target = !PtInRect(&fg, p);
        if (dlg_color_picker("Color Picker", &c)) color_set(c);
        return;
    }
    if (msg == WM_LBUTTONDOWN) {
        if (PtInRect(&fg, p)) { g_color_bg_target = 0; InvalidateRect(hwnd, NULL, FALSE); options_refresh(); return; }
        if (PtInRect(&bg, p)) { g_color_bg_target = 1; InvalidateRect(hwnd, NULL, FALSE); return; }
        g_color_drag = -1;
        for (k = 0; k < 3; k++) { RECT s; color_slider_rect(r, k, &s); InflateRect(&s, 2, 2); if (PtInRect(&s, p)) g_color_drag = k; }
        if (PtInRect(&bar, p)) g_color_drag = 3;
    }
    if ((msg == WM_LBUTTONDOWN || msg == WM_MOUSEMOVE) && g_color_drag >= 0) {
        u32 c = g_color_bg_target ? g_app.bg : g_app.fg;
        if (g_color_drag == 3) {
            int rr, gg, bb;
            float h = (float)(PC_CLAMP(x, bar.left, bar.right - 1) - bar.left) * 360 / PC_MAX(1, bar.right - bar.left);
            float t = (float)(PC_CLAMP(y, bar.top, bar.bottom - 1) - bar.top) / PC_MAX(1, bar.bottom - bar.top - 1);
            pc_hsb_to_rgb(h, t < 0.5f ? 1 : 1 - (t - 0.5f) * 2, t < 0.5f ? t * 2 : 1, &rr, &gg, &bb);
            c = pc_rgb(rr, gg, bb);
            if (GetKeyState(VK_MENU) < 0) { app_set_colors(g_app.fg, c); return; }
        } else {
            RECT s;
            int v;
            color_slider_rect(r, g_color_drag, &s);
            v = PC_CLAMP((x - s.left) * 255 / PC_MAX(1, s.right - s.left - 1), 0, 255);
            c = g_color_drag == 0 ? pc_rgb(v, pc_g(c), pc_b(c)) : g_color_drag == 1 ? pc_rgb(pc_r(c), v, pc_b(c)) : pc_rgb(pc_r(c), pc_g(c), v);
        }
        color_set(c);
    }
    if (msg == WM_LBUTTONUP) g_color_drag = -1;
}

/* ---------------- Swatches ---------------- */

#define MAX_SWATCHES 160
static u32 g_sw[MAX_SWATCHES];
static int g_nsw = -1;

static void swatches_default(void)
{
    /* A 1998-style default set: web-safe-ish primaries, then gray ramp, then pastels. */
    static const u32 base[] = {
        0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF, 0xE6E6E6, 0xCCCCCC, 0xB3B3B3,
        0x999999, 0x808080, 0x666666, 0x4D4D4D, 0x333333, 0x1A1A1A, 0x000000, 0xF7977A, 0xF9AD81, 0xFDC68A,
        0xFFF79A, 0xC4DF9B, 0xA2D39C, 0x82CA9D, 0x7BCDC8, 0x6ECFF6, 0x7EA7D8, 0x8493CA, 0x8882BE, 0xA187BE,
        0xBC8DBF, 0xF49AC2, 0xF6989D, 0xED1C24, 0xF26522, 0xF7941D, 0xFFF200, 0x8DC63F, 0x39B54A, 0x00A651,
        0x00A99D, 0x00AEEF, 0x0072BC, 0x0054A6, 0x2E3192, 0x662D91, 0x92278F, 0xEC008C, 0xED145B, 0x9E0B0F,
        0xA0410D, 0xA36209, 0xABA000, 0x598527, 0x197B30, 0x007236, 0x00746B, 0x0076A3, 0x004B80, 0x003471,
        0x1B1464, 0x440E62, 0x630460, 0x9E005D, 0x9E0039, 0x790000, 0x7B2E00, 0x7D4900, 0x827B00, 0x406618,
        0x005E20, 0x005826, 0x005952, 0x005B7F, 0x003663, 0x002157, 0x0D004C, 0x32004B, 0x4B0049, 0x7B0046,
        0x7A0026, 0xC69C6D, 0xA67C52, 0x8C6239, 0x754C24, 0x603913, 0x998675, 0x736357, 0x534741, 0x362F2D
    };
    int i;
    g_nsw = (int)(sizeof base / sizeof base[0]);
    for (i = 0; i < g_nsw; i++) g_sw[i] = base[i];
}

static void swatches_load(void)
{
    int n = reg_get_int("Swatches", "Count", -1);
    if (n > 0 && n <= MAX_SWATCHES && reg_get_bin("Swatches", "Colors", g_sw, (int)(sizeof(u32) * n))) g_nsw = n;
    else swatches_default();
}

static void swatches_save(void)
{
    reg_set_int("Swatches", "Count", g_nsw);
    reg_set_bin("Swatches", "Colors", g_sw, (int)(sizeof(u32) * g_nsw));
}

#define SW_CELL 11
static void swatches_paint(HDC dc, const RECT *r)
{
    int i, cols = PC_MAX(1, (r->right - r->left - 6) / SW_CELL);
    if (g_nsw < 0) swatches_load();
    for (i = 0; i < g_nsw; i++) {
        int x = r->left + 3 + (i % cols) * SW_CELL, y = r->top + 3 + (i / cols) * SW_CELL - g_scroll[P_SWATCHES] * SW_CELL;
        RECT c;
        if (y < r->top || y + SW_CELL > r->bottom) continue;
        SetRect(&c, x, y, x + SW_CELL - 1, y + SW_CELL - 1);
        w95_fill(dc, &c, RGB(pc_r(g_sw[i]), pc_g(g_sw[i]), pc_b(g_sw[i])));
        w95_rect(dc, c.left, c.top, c.right, c.bottom, W95_SHADOW);
    }
}

static void swatches_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y)
{
    int cols = PC_MAX(1, (r->right - r->left - 6) / SW_CELL), i;
    if (msg != WM_LBUTTONDOWN) return;
    i = ((y - r->top - 3) / SW_CELL + g_scroll[P_SWATCHES]) * cols + (x - r->left - 3) / SW_CELL;
    if (x < r->left + 3 || x >= r->left + 3 + cols * SW_CELL || y < r->top + 3) return;
    if (i >= 0 && i < g_nsw) {
        if (GetKeyState(VK_CONTROL) < 0) {
            /* Ctrl+click deletes a swatch (PS5). */
            memmove(&g_sw[i], &g_sw[i + 1], sizeof(u32) * (g_nsw - i - 1));
            g_nsw--;
            swatches_save();
        } else if (GetKeyState(VK_MENU) < 0) app_set_colors(g_app.fg, g_sw[i]);
        else app_set_colors(g_sw[i], g_app.bg);
    } else if (g_nsw < MAX_SWATCHES) {
        /* Clicking the empty area adds the foreground colour. */
        g_sw[g_nsw++] = g_app.fg;
        swatches_save();
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ---------------- Brushes ---------------- */

typedef struct { int dia, hard; } BrushPreset;
static BrushPreset g_br[48];
static int g_nbr;

static void brushes_default(void)
{
    static const BrushPreset def[] = {
        { 1, 100 }, { 3, 100 }, { 5, 100 }, { 9, 100 }, { 13, 100 }, { 19, 100 },
        { 5, 0 }, { 9, 0 }, { 13, 0 }, { 17, 0 }, { 21, 0 }, { 27, 0 },
        { 35, 0 }, { 45, 0 }, { 65, 0 }, { 100, 0 }, { 200, 0 }, { 300, 0 }
    };
    g_nbr = (int)(sizeof def / sizeof def[0]);
    memcpy(g_br, def, sizeof def);
}

static void brushes_load(void)
{
    int n = reg_get_int("Brushes", "Count", -1);
    if (n > 0 && n <= 48 && reg_get_bin("Brushes", "Presets", g_br, (int)(sizeof(BrushPreset) * n))) g_nbr = n;
    else brushes_default();
}

static void brushes_save(void)
{
    reg_set_int("Brushes", "Count", g_nbr);
    reg_set_bin("Brushes", "Presets", g_br, (int)(sizeof(BrushPreset) * g_nbr));
}

#define BR_CELL 33
static void brushes_paint(HDC dc, const RECT *r)
{
    int i, cols = PC_MAX(1, (r->right - r->left - 4) / BR_CELL);
    const PcBrush *cur = &g_app.topt[g_app.tool].brush;
    if (!g_nbr) brushes_load();
    for (i = 0; i < g_nbr; i++) {
        int x = r->left + 2 + (i % cols) * BR_CELL, y = r->top + 2 + (i / cols - g_scroll[P_BRUSHES]) * BR_CELL;
        RECT c;
        int cx, cy, rad, px, py;
        if (y < r->top || y + BR_CELL > r->bottom) continue;
        SetRect(&c, x, y, x + BR_CELL - 1, y + BR_CELL - 1);
        w95_fill(dc, &c, W95_WINDOW);
        if (cur->diameter == g_br[i].dia && cur->hardness == g_br[i].hard) { w95_sunken(dc, &c); }
        else w95_rect(dc, c.left, c.top, c.right, c.bottom, W95_LIGHT);
        cx = (c.left + c.right) / 2; cy = (c.top + c.bottom) / 2;
        rad = PC_MIN(g_br[i].dia / 2, 13);
        for (py = -rad; py <= rad; py++)
            for (px = -rad; px <= rad; px++) {
                double d = sqrt((double)px * px + (double)py * py);
                double core = rad * g_br[i].hard / 100.0;
                int v;
                if (d > rad + 0.5) continue;
                v = d <= core ? 0 : (int)(255 * (d - core) / PC_MAX(1.0, rad - core));
                v = PC_CLAMP(v, 0, 255);
                w95_pixel(dc, cx + px, cy + py, RGB(v, v, v));
            }
        if (g_br[i].dia > 27) {
            char t[8];
            RECT tr = c;
            pc_snprintf(t, sizeof t, "%d", g_br[i].dia);
            tr.top = tr.bottom - 9;
            w95_text(dc, W95_FONT_SMALL, t, &tr, DT_CENTER | DT_SINGLELINE, W95_SELBK);
        }
    }
}

static void brushes_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y)
{
    int cols = PC_MAX(1, (r->right - r->left - 4) / BR_CELL), i;
    if (msg != WM_LBUTTONDOWN && msg != WM_LBUTTONDBLCLK) return;
    if (x < r->left + 2 || x >= r->left + 2 + cols * BR_CELL) return;
    i = ((y - r->top - 2) / BR_CELL + g_scroll[P_BRUSHES]) * cols + (x - r->left - 2) / BR_CELL;
    if (i < 0 || i >= g_nbr) return;
    {
        PcBrush *b = &g_app.topt[g_app.tool].brush;
        b->diameter = g_br[i].dia; b->hardness = g_br[i].hard;
        if (msg == WM_LBUTTONDBLCLK && dlg_brush_options(b)) { g_br[i].dia = b->diameter; g_br[i].hard = b->hardness; brushes_save(); }
    }
    app_refresh(R_OPTIONS);
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ---------------- Layers ---------------- */

#define LAYER_ROW 28
static HWND g_lay_mode, g_lay_opacity;
static int g_lay_drag = -1, g_lay_drag_to = -1;

static const char *const k_blend_names[PC_BLEND_COUNT] = {
    "Normal", "Dissolve", "Multiply", "Screen", "Overlay", "Soft Light", "Hard Light", "Color Dodge",
    "Color Burn", "Darken", "Lighten", "Difference", "Exclusion", "Hue", "Saturation", "Color", "Luminosity"
};

static void lock_rects(const RECT *r, RECT *out)
{
    int k;
    for (k = 0; k < 4; k++) SetRect(&out[k], r->left + 40 + k * 18, r->top + 27, r->left + 40 + k * 18 + 16, r->top + 43);
}

static void layers_paint(HDC dc, const RECT *r)
{
    PcDoc *d = app_doc();
    RECT area, btn[4], lk[4];
    static const int bicons[4] = { PC_ICON_LAYER_STYLE, PC_ICON_ADD_MASK, PC_ICON_NEW_LAYER, PC_ICON_TRASH };
    static const int licons[4] = { PC_ICON_LOCK_TRANSPARENT, PC_ICON_LOCK_PIXELS, PC_ICON_LOCK_POSITION, PC_ICON_LOCK_ALL };
    int rows, w, i, k;
    {
        RECT t;
        SetRect(&t, r->left + 4, r->top + 27, r->left + 40, r->top + 43);
        w95_text(dc, W95_FONT_NORMAL, "Lock:", &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE, d ? W95_TEXT : W95_GRAYTEXT);
        lock_rects(r, lk);
        for (k = 0; k < 4; k++) {
            PcLayer *l = d ? pc_doc_active(d) : NULL;
            int on = l && ((k == 0 && l->lock_alpha) || (k == 1 && l->lock_pixels) || (k == 2 && l->lock_position) ||
                           (k == 3 && l->lock_alpha && l->lock_pixels && l->lock_position));
            w95_draw_icon_button(dc, &lk[k], licons[k], on ? 3 : 0, !l);
            if (!on) w95_thin_raised(dc, &lk[k]);
        }
    }
    w95_etched_hline(dc, r->left, r->right, r->top + 46);
    list_area(r, 48, 1, &area);
    w95_fill(dc, &area, W95_WINDOW);
    rows = d ? d->s.nlayers : 0;
    w = list_scroll(dc, P_LAYERS, &area, rows, LAYER_ROW);
    for (i = 0; i < rows; i++) {
        int li = d->s.nlayers - 1 - (i + g_scroll[P_LAYERS]);
        PcLayer *l;
        RECT row, eye, tgt, th, nm;
        int y = area.top + i * LAYER_ROW, sel;
        if (li < 0 || y + LAYER_ROW > area.bottom) break;
        l = d->s.layers[li];
        sel = li == d->s.active;
        SetRect(&row, area.left, y, area.left + w, y + LAYER_ROW);
        /* Eye column (sunken cell), target column, then thumbnail and name. */
        SetRect(&eye, row.left, y, row.left + 20, y + LAYER_ROW - 1);
        w95_fill(dc, &eye, W95_FACE);
        w95_thin_raised(dc, &eye);
        if (l->visible) w95_icon(dc, PC_ICON_EYE, eye.left + 3, y + 8);
        SetRect(&tgt, eye.right, y, eye.right + 18, y + LAYER_ROW - 1);
        w95_fill(dc, &tgt, W95_FACE);
        w95_thin_raised(dc, &tgt);
        if (sel) w95_icon(dc, g_mask_target && l->mask ? PC_ICON_MASK_SMALL : PC_ICON_BRUSH_SMALL, tgt.left + 2, y + 7);
        if (l->clip) w95_icon(dc, PC_ICON_LINK, tgt.left + 5, y + 7);
        SetRect(&nm, tgt.right, y, row.right, y + LAYER_ROW - 1);
        w95_fill(dc, &nm, sel ? W95_SELBK : W95_WINDOW);
        SetRect(&th, nm.left + 3, y + 3, nm.left + 25, y + LAYER_ROW - 4);
        if (l->kind == PC_LAYER_TEXT) w95_icon(dc, PC_ICON_TEXT_LAYER, th.left + 3, y + 6);
        else { w95_fill(dc, &th, W95_WINDOW); draw_thumb(dc, &th, l->pix, 1); }
        nm.left = th.right + 4;
        if (l->mask) {
            RECT mt;
            SetRect(&mt, nm.left, y + 3, nm.left + 22, y + LAYER_ROW - 4);
            draw_thumb(dc, &mt, l->mask, 0);
            if (sel && g_mask_target) w95_rect(dc, mt.left - 2, mt.top - 2, mt.right + 2, mt.bottom + 2, W95_SELTEXT);
            if (!l->mask_enabled) { w95_hline(dc, mt.left, mt.right, mt.top + 1, RGB(255, 0, 0)); w95_pixel(dc, mt.left, mt.bottom - 1, RGB(255, 0, 0)); }
            nm.left = mt.right + 4;
        }
        if (l->fx_enabled) { w95_icon(dc, PC_ICON_FX, nm.right - 16, y + 8); nm.right -= 18; }
        {
            char label[80];
            pc_snprintf(label, sizeof label, "%s", l->name);
            w95_text(dc, l->is_background ? W95_FONT_NORMAL : W95_FONT_NORMAL, label, &nm,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, sel ? W95_SELTEXT : W95_TEXT);
        }
        if (l->is_background) w95_icon(dc, PC_ICON_LOCK_ALL, row.right - 15, y + 8);
        w95_hline(dc, row.left, row.right, y + LAYER_ROW - 1, W95_SHADOW);
        if (g_lay_drag >= 0 && g_lay_drag_to == li) w95_hline(dc, row.left, row.right, y, W95_TEXT), w95_hline(dc, row.left, row.right, y + 1, W95_TEXT);
    }
    bottom_bar(dc, r, bicons, 4, btn);
}

static int layer_at(const RECT *r, int y)
{
    PcDoc *d = app_doc();
    RECT area;
    int i;
    if (!d) return -1;
    list_area(r, 48, 1, &area);
    if (y < area.top || y >= area.bottom) return -1;
    i = (y - area.top) / LAYER_ROW + g_scroll[P_LAYERS];
    if (i >= d->s.nlayers) return -1;
    return d->s.nlayers - 1 - i;
}

static void layer_context_menu(HWND hwnd, int sx, int sy)
{
    static const MenuItem items[] = {
        { 1, CMD_LAYER_PROPS, NULL }, { 1, CMD_LAYER_STYLE, NULL }, { 1, 0, "-" },
        { 1, CMD_DUPLICATE_LAYER, NULL }, { 1, CMD_DELETE_LAYER, NULL }, { 1, 0, "-" },
        { 1, CMD_MASK_REVEAL_ALL, "Add Layer Mask" }, { 1, CMD_MASK_APPLY, "Apply Layer Mask" }, { 1, CMD_MASK_TOGGLE, NULL }, { 1, 0, "-" },
        { 1, CMD_TYPE_RASTERIZE, NULL }, { 1, CMD_CLIP_GROUP, NULL }, { 1, 0, "-" },
        { 1, CMD_MERGE_DOWN, NULL }, { 1, CMD_MERGE_VISIBLE, NULL }, { 1, CMD_FLATTEN, NULL }
    };
    int cmd = menu_popup_items(items, (int)(sizeof items / sizeof items[0]), sx, sy, hwnd);
    if (cmd) cmd_run(cmd);
}

static void layers_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y, WPARAM wp)
{
    PcDoc *d = app_doc();
    RECT area, lk[4];
    int li, b;
    POINT p = { x, y };
    if (!d) return;
    list_area(r, 48, 1, &area);
    if (msg == WM_LBUTTONDOWN && list_scroll_click(P_LAYERS, &area, d->s.nlayers, LAYER_ROW, x, y)) { InvalidateRect(hwnd, NULL, FALSE); return; }
    lock_rects(r, lk);
    if (msg == WM_LBUTTONDOWN) {
        int k;
        PcLayer *l = pc_doc_active(d);
        for (k = 0; k < 4 && l; k++) if (PtInRect(&lk[k], p)) {
            if (k == 0) l->lock_alpha = !l->lock_alpha;
            else if (k == 1) l->lock_pixels = !l->lock_pixels;
            else if (k == 2) l->lock_position = !l->lock_position;
            else { int on = !(l->lock_alpha && l->lock_pixels && l->lock_position); l->lock_alpha = l->lock_pixels = l->lock_position = on; }
            app_commit(d, "Layer Lock");
            return;
        }
        b = bottom_hit(r, 4, x, y);
        if (b == 0) { cmd_run(CMD_LAYER_STYLE); return; }
        if (b == 1) { cmd_run(CMD_ADD_MASK_QUICK); return; }
        if (b == 2) { cmd_run(GetKeyState(VK_MENU) < 0 ? CMD_NEW_LAYER : CMD_NEW_LAYER_QUICK); return; }
        if (b == 3) { cmd_run(CMD_DELETE_LAYER); return; }
    }
    li = layer_at(r, y);
    if (msg == WM_RBUTTONDOWN) {
        POINT sp = { x, y };
        if (li >= 0 && li != d->s.active) { d->s.active = li; app_refresh(R_LAYERS | R_CHANNELS); }
        ClientToScreen(hwnd, &sp);
        layer_context_menu(hwnd, sp.x, sp.y);
        return;
    }
    if (msg == WM_LBUTTONDOWN && li >= 0) {
        int col = x - area.left;
        PcLayer *l = d->s.layers[li];
        if (col < 20) { l->visible = !l->visible; app_commit(d, l->visible ? "Show Layer" : "Hide Layer"); return; }
        if (col < 38) {
            if (li == d->s.active && li > 0 && !l->is_background) { l->clip = !l->clip; app_commit(d, l->clip ? "Group with Previous" : "Ungroup"); }
            return;
        }
        if (d->s.active != li) {
            DocView *v = app_view();
            if (v) tool_commit(v);
            d->s.active = li;
            g_mask_target = 0;
        }
        if (l->mask) {
            /* Clicking the mask thumbnail targets the mask; the image thumbnail the pixels. */
            int mx0 = 38 + 3 + 22 + 4;
            g_mask_target = col >= mx0 && col < mx0 + 22;
            if (g_mask_target && (wp & MK_SHIFT)) { l->mask_enabled = !l->mask_enabled; app_commit(d, l->mask_enabled ? "Enable Layer Mask" : "Disable Layer Mask"); return; }
        }
        g_lay_drag = l->is_background ? -1 : li;
        g_lay_drag_to = -1;
        app_refresh(R_LAYERS | R_CHANNELS | R_OPTIONS);
        {
            /* Selecting a type layer loads its settings into the Options bar. */
            extern void options_set_type(const PcText *t);
            if (l->kind == PC_LAYER_TEXT) options_set_type(&l->text);
        }
        return;
    }
    if (msg == WM_LBUTTONDBLCLK && li >= 0) {
        int col = x - area.left;
        d->s.active = li;
        if (col >= 38 && col < 38 + 28) cmd_run(d->s.layers[li]->kind == PC_LAYER_TEXT ? CMD_TYPE_EDIT : CMD_LAYER_STYLE);
        else if (col >= 38) cmd_run(d->s.layers[li]->is_background ? CMD_LAYER_FROM_BG : CMD_LAYER_PROPS);
        return;
    }
    if (msg == WM_MOUSEMOVE && g_lay_drag >= 0 && (wp & MK_LBUTTON)) {
        int to = layer_at(r, y);
        if (to != g_lay_drag_to) { g_lay_drag_to = to; InvalidateRect(hwnd, NULL, FALSE); }
    }
    if (msg == WM_LBUTTONUP && g_lay_drag >= 0) {
        int from = g_lay_drag, to = g_lay_drag_to;
        g_lay_drag = -1; g_lay_drag_to = -1;
        if (to >= 0 && to != from && !pc_doc_move_layer(d, from, to)) app_commit(d, "Layer Order");
        else InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void layers_controls(HWND host, const RECT *r)
{
    PcDoc *d = app_doc();
    PcLayer *l = d ? pc_doc_active(d) : NULL;
    char b[16];
    if (!g_lay_mode) {
        int k;
        g_lay_mode = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 220, host, (HMENU)10, w95_inst(), NULL);
        g_lay_opacity = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "100", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, 0, 0, 10, 20, host, (HMENU)11, w95_inst(), NULL);
        SendMessageA(g_lay_mode, WM_SETFONT, (WPARAM)w95_font(W95_FONT_NORMAL), 0);
        SendMessageA(g_lay_opacity, WM_SETFONT, (WPARAM)w95_font(W95_FONT_NORMAL), 0);
        for (k = 0; k < PC_BLEND_COUNT; k++) SendMessageA(g_lay_mode, CB_ADDSTRING, 0, (LPARAM)k_blend_names[k]);
    }
    if (GetParent(g_lay_mode) != host) { SetParent(g_lay_mode, host); SetParent(g_lay_opacity, host); }
    SetWindowPos(g_lay_mode, NULL, r->left + 3, r->top + 3, 90, 220, SWP_NOZORDER);
    SetWindowPos(g_lay_opacity, NULL, r->right - 56, r->top + 3, 32, 20, SWP_NOZORDER);
    EnableWindow(g_lay_mode, l && !l->is_background);
    EnableWindow(g_lay_opacity, l && !l->is_background);
    if (l) {
        SendMessageA(g_lay_mode, CB_SETCURSEL, (WPARAM)l->blend, 0);
        if (GetFocus() != g_lay_opacity) { pc_snprintf(b, sizeof b, "%d", (l->opacity * 100 + 127) / 255); SetWindowTextA(g_lay_opacity, b); }
    }
}

/* "Opacity:" label + popup arrow are drawn around the edit. */
static void layers_paint_top(HDC dc, const RECT *r)
{
    RECT t, a;
    SetRect(&t, r->right - 104, r->top + 3, r->right - 58, r->top + 23);
    w95_text(dc, W95_FONT_NORMAL, "Opacity:", &t, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, app_doc() ? W95_TEXT : W95_GRAYTEXT);
    SetRect(&a, r->right - 24, r->top + 3, r->right - 13, r->top + 23);
    w95_fill(dc, &a, W95_FACE);
    w95_thin_raised(dc, &a);
    w95_draw_arrow(dc, a.left + 5, a.top + 10, 3, 4, W95_TEXT);
    SetRect(&t, r->right - 12, r->top + 3, r->right - 2, r->top + 23);
    w95_text(dc, W95_FONT_NORMAL, "%", &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
}

static void opacity_live(int v, void *u)
{
    PcDoc *d = app_doc();
    PcLayer *l = d ? pc_doc_active(d) : NULL;
    char b[16];
    (void)u;
    if (!l) return;
    l->opacity = v * 255 / 100;
    pc_doc_touch(d);
    pc_snprintf(b, sizeof b, "%d", v);
    SetWindowTextA(g_lay_opacity, b);
    app_refresh(R_CANVAS | R_NAV);
}

/* ---------------- Channels ---------------- */

#define CH_ROW 26
static void channels_paint(HDC dc, const RECT *r)
{
    DocView *v = app_view();
    PcDoc *d = v ? v->doc : NULL;
    RECT area, btn[4];
    static const int bicons[4] = { PC_ICON_LOAD_SELECTION, PC_ICON_SAVE_SELECTION, PC_ICON_NEW_LAYER, PC_ICON_TRASH };
    static const char *const names_rgb[4] = { "RGB", "Red", "Green", "Blue" };
    static const char *const keys[4] = { "Ctrl+~", "Ctrl+1", "Ctrl+2", "Ctrl+3" };
    int rows, w, i;
    list_area(r, 0, 1, &area);
    w95_fill(dc, &area, W95_WINDOW);
    rows = d ? (d->s.mode == PC_MODE_GRAY ? 1 : 4) + d->s.nchannels : 0;
    w = list_scroll(dc, P_CHANNELS, &area, rows, CH_ROW);
    for (i = 0; i < rows; i++) {
        int ci = i + g_scroll[P_CHANNELS], y = area.top + i * CH_ROW, sel, vis;
        int ncolor = d->s.mode == PC_MODE_GRAY ? 1 : 4;
        RECT eye, nm, th, kr;
        const char *name;
        if (y + CH_ROW > area.bottom || ci >= rows) break;
        if (ci < ncolor) {
            name = d->s.mode == PC_MODE_GRAY ? "Gray" : names_rgb[ci];
            sel = (ci == 0 && v->edit_channel < 0) || (ci > 0 && v->edit_channel == ci - 1);
            vis = v->show_channel < 0 || v->show_channel == ci - 1 || (ci == 0 && v->show_channel < 0);
        } else {
            name = d->s.channels[ci - ncolor].name;
            sel = v->edit_channel == 100 + (ci - ncolor);
            vis = d->s.channels[ci - ncolor].visible;
        }
        SetRect(&eye, area.left, y, area.left + 20, y + CH_ROW - 1);
        w95_fill(dc, &eye, W95_FACE); w95_thin_raised(dc, &eye);
        if (vis) w95_icon(dc, PC_ICON_EYE, eye.left + 3, y + 7);
        SetRect(&nm, eye.right, y, area.left + w, y + CH_ROW - 1);
        w95_fill(dc, &nm, sel ? W95_SELBK : W95_WINDOW);
        SetRect(&th, nm.left + 3, y + 3, nm.left + 23, y + CH_ROW - 4);
        if (ci < ncolor && ci > 0) {
            /* Channel thumbnail in grayscale. */
            PcBuf *tb = pc_buf_new(PC_MAX(1, d->s.w / 16), PC_MAX(1, d->s.h / 16), 1, 0);
            if (!v->comp || v->comp_rev != d->revision) { pc_buf_unref(v->comp); v->comp = pc_doc_composite(d); v->comp_rev = d->revision; }
            if (tb && v->comp) {
                int x, yy;
                for (yy = 0; yy < tb->h; yy++) for (x = 0; x < tb->w; x++) tb->px[yy * tb->w + x] = v->comp->px[((size_t)(yy * 16) * d->s.w + x * 16) * 4 + ci - 1];
                draw_thumb(dc, &th, tb, 0);
            }
            pc_buf_unref(tb);
        } else if (ci >= ncolor) {
            draw_thumb(dc, &th, d->s.channels[ci - ncolor].buf, 0);
        } else if (v->comp) {
            draw_thumb(dc, &th, v->comp, 1);
        }
        nm.left = th.right + 5;
        w95_text(dc, W95_FONT_NORMAL, name, &nm, DT_LEFT | DT_VCENTER | DT_SINGLELINE, sel ? W95_SELTEXT : W95_TEXT);
        if (ci < ncolor && d->s.mode == PC_MODE_RGB) {
            kr = nm; kr.right -= 4;
            w95_text(dc, W95_FONT_NORMAL, keys[ci], &kr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, sel ? W95_SELTEXT : W95_TEXT);
        }
        w95_hline(dc, area.left, area.left + w, y + CH_ROW - 1, W95_SHADOW);
    }
    bottom_bar(dc, r, bicons, 4, btn);
}

static void channels_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y)
{
    DocView *v = app_view();
    PcDoc *d = v ? v->doc : NULL;
    RECT area;
    int b, ci, ncolor, rows;
    if (!d) return;
    list_area(r, 0, 1, &area);
    ncolor = d->s.mode == PC_MODE_GRAY ? 1 : 4;
    rows = ncolor + d->s.nchannels;
    if (msg == WM_LBUTTONDOWN && list_scroll_click(P_CHANNELS, &area, rows, CH_ROW, x, y)) { InvalidateRect(hwnd, NULL, FALSE); return; }
    if (msg == WM_LBUTTONDOWN) {
        b = bottom_hit(r, 4, x, y);
        if (b == 0) {
            int sel = v->edit_channel >= 100 ? v->edit_channel - 100 : d->s.nchannels - 1;
            if (sel >= 0 && !pc_sel_load_channel(d, sel, 0, PC_SEL_REPLACE)) app_commit(d, "Load Selection");
            return;
        }
        if (b == 1) { cmd_run(CMD_SAVE_SELECTION); return; }
        if (b == 2) { cmd_run(CMD_CHANNEL_NEW); return; }
        if (b == 3) { cmd_run(CMD_CHANNEL_DELETE); return; }
    }
    if (y < area.top || y >= area.bottom) return;
    ci = (y - area.top) / CH_ROW + g_scroll[P_CHANNELS];
    if (ci >= rows) return;
    if (msg == WM_LBUTTONDOWN) {
        if (x - area.left < 20) {
            if (ci >= ncolor) { d->s.channels[ci - ncolor].visible = !d->s.channels[ci - ncolor].visible; pc_doc_touch(d); }
            else v->show_channel = (ci == 0 || v->show_channel == ci - 1) ? -1 : ci - 1;
        } else if (ci == 0) { v->edit_channel = -1; v->show_channel = -1; }
        else if (ci < ncolor) { v->edit_channel = ci - 1; v->show_channel = ci - 1; }
        else { v->edit_channel = 100 + ci - ncolor; }
        app_refresh(R_CANVAS | R_CHANNELS);
    } else if (msg == WM_LBUTTONDBLCLK && ci >= ncolor) {
        char name[32];
        pc_strlcpy(name, d->s.channels[ci - ncolor].name, sizeof name);
        if (dlg_text_input("Channel Options", "Name:", name, sizeof name)) { pc_strlcpy(d->s.channels[ci - ncolor].name, name, 32); app_commit(d, "Channel Options"); }
    }
}

/* ---------------- Paths ---------------- */

#define PATH_ROW 26
static void paths_paint(HDC dc, const RECT *r)
{
    PcDoc *d = app_doc();
    RECT area, btn[6];
    static const int bicons[6] = { PC_ICON_FILL_PATH, PC_ICON_STROKE_PATH, PC_ICON_PATH_TO_SELECTION, PC_ICON_SELECTION_TO_PATH, PC_ICON_NEW_LAYER, PC_ICON_TRASH };
    int i, rows, w;
    list_area(r, 0, 1, &area);
    w95_fill(dc, &area, W95_WINDOW);
    rows = d ? d->s.npaths : 0;
    w = list_scroll(dc, P_PATHS, &area, rows, PATH_ROW);
    for (i = 0; i < rows; i++) {
        int pi = i + g_scroll[P_PATHS], y = area.top + i * PATH_ROW, sel;
        RECT nm, th;
        if (pi >= rows || y + PATH_ROW > area.bottom) break;
        sel = pi == d->s.active_path;
        SetRect(&nm, area.left, y, area.left + w, y + PATH_ROW - 1);
        w95_fill(dc, &nm, sel ? W95_SELBK : W95_WINDOW);
        SetRect(&th, nm.left + 4, y + 3, nm.left + 24, y + PATH_ROW - 4);
        w95_fill(dc, &th, W95_WINDOW);
        w95_rect(dc, th.left - 1, th.top - 1, th.right + 1, th.bottom + 1, W95_DKSHADOW);
        {
            /* Path thumbnail: flattened outline. */
            float xy[2 * 512];
            u8 st[512];
            int n = pc_path_flatten(&d->s.paths[pi], xy, st, 512), k;
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0)), op = (HPEN)SelectObject(dc, pen);
            for (k = 0; k < n; k++) {
                int px = th.left + (int)(xy[k * 2] * (th.right - th.left) / PC_MAX(1, d->s.w));
                int py = th.top + (int)(xy[k * 2 + 1] * (th.bottom - th.top) / PC_MAX(1, d->s.h));
                if (st[k]) MoveToEx(dc, px, py, NULL); else LineTo(dc, px, py);
            }
            SelectObject(dc, op); DeleteObject(pen);
        }
        nm.left = th.right + 6;
        w95_text(dc, W95_FONT_NORMAL, d->s.paths[pi].name, &nm, DT_LEFT | DT_VCENTER | DT_SINGLELINE, sel ? W95_SELTEXT : W95_TEXT);
        w95_hline(dc, area.left, area.left + w, y + PATH_ROW - 1, W95_SHADOW);
    }
    bottom_bar(dc, r, bicons, 6, btn);
}

static void paths_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y)
{
    PcDoc *d = app_doc();
    RECT area;
    int b, pi;
    if (!d) return;
    list_area(r, 0, 1, &area);
    if (msg == WM_LBUTTONDOWN && list_scroll_click(P_PATHS, &area, d->s.npaths, PATH_ROW, x, y)) { InvalidateRect(hwnd, NULL, FALSE); return; }
    if (msg == WM_LBUTTONDOWN) {
        static const int cmds[6] = { CMD_PATH_FILL, CMD_PATH_STROKE, CMD_PATH_TO_SEL, CMD_SEL_TO_PATH, CMD_PATH_SAVE, CMD_PATH_DELETE };
        b = bottom_hit(r, 6, x, y);
        if (b >= 0) { cmd_run(cmds[b]); return; }
    }
    if (y < area.top || y >= area.bottom) return;
    pi = (y - area.top) / PATH_ROW + g_scroll[P_PATHS];
    if (msg == WM_LBUTTONDOWN) {
        d->s.active_path = pi < d->s.npaths ? pi : -1;
        app_refresh(R_CANVAS | R_PATHS);
    } else if (msg == WM_LBUTTONDBLCLK && pi < d->s.npaths) {
        d->s.active_path = pi;
        cmd_run(CMD_PATH_SAVE);
    }
}

/* ---------------- History ---------------- */

#define HIST_ROW 18
static void history_paint(HDC dc, const RECT *r)
{
    PcDoc *d = app_doc();
    RECT area, btn[3];
    static const int bicons[3] = { PC_ICON_NEW_DOC_FROM_STATE, PC_ICON_SNAPSHOT, PC_ICON_TRASH };
    int i, rows, w, nsnap = 0;
    list_area(r, 0, 1, &area);
    w95_fill(dc, &area, W95_WINDOW);
    rows = d ? d->nhist : 0;
    if (d) while (nsnap < d->nhist && d->hist[nsnap].snapshot) nsnap++;
    w = list_scroll(dc, P_HISTORY, &area, rows + (nsnap ? 1 : 0), HIST_ROW);
    for (i = 0; i < rows; i++) {
        int hi = i + g_scroll[P_HISTORY], y = area.top + i * HIST_ROW + (hi >= nsnap && nsnap ? 4 : 0), sel, after;
        RECT src, nm;
        if (hi >= rows || y + HIST_ROW > area.bottom) break;
        sel = hi == d->hist_cur;
        after = hi > d->hist_cur && !d->hist[hi].snapshot;
        if (hi == nsnap && nsnap) w95_etched_hline(dc, area.left, area.left + w, y - 3);
        SetRect(&src, area.left, y, area.left + 18, y + HIST_ROW - 1);
        w95_fill(dc, &src, W95_FACE); w95_thin_raised(dc, &src);
        if (d->history_source == hi) w95_icon(dc, PC_ICON_HISTORY_SOURCE, src.left + 2, y + 2);
        SetRect(&nm, src.right, y, area.left + w, y + HIST_ROW - 1);
        w95_fill(dc, &nm, sel ? W95_SELBK : W95_WINDOW);
        nm.left += 4;
        w95_text(dc, W95_FONT_NORMAL, d->hist[hi].label, &nm, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS,
                 sel ? W95_SELTEXT : after ? W95_GRAYTEXT : W95_TEXT);
        w95_hline(dc, area.left, area.left + w, y + HIST_ROW - 1, W95_LIGHT);
    }
    bottom_bar(dc, r, bicons, 3, btn);
}

static void history_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y)
{
    PcDoc *d = app_doc();
    RECT area;
    int b, hi, nsnap = 0;
    if (!d) return;
    list_area(r, 0, 1, &area);
    while (nsnap < d->nhist && d->hist[nsnap].snapshot) nsnap++;
    if (msg == WM_LBUTTONDOWN && list_scroll_click(P_HISTORY, &area, d->nhist + 1, HIST_ROW, x, y)) { InvalidateRect(hwnd, NULL, FALSE); return; }
    if (msg == WM_LBUTTONDOWN) {
        b = bottom_hit(r, 3, x, y);
        if (b == 0) { cmd_run(CMD_HISTORY_NEWDOC); return; }
        if (b == 1) { cmd_run(CMD_HISTORY_SNAPSHOT); return; }
        if (b == 2) { cmd_run(CMD_HISTORY_DELETE); return; }
    }
    if (msg != WM_LBUTTONDOWN || y < area.top || y >= area.bottom) return;
    hi = (y - area.top) / HIST_ROW + g_scroll[P_HISTORY];
    if (nsnap && hi >= nsnap) hi = (y - area.top - 4) / HIST_ROW + g_scroll[P_HISTORY];
    if (hi < 0 || hi >= d->nhist) return;
    if (x - area.left < 18) { d->history_source = hi; InvalidateRect(hwnd, NULL, FALSE); return; }
    {
        DocView *v = app_view();
        if (v) tool_cancel(v);
    }
    if (!pc_hist_goto(d, hi)) app_refresh(R_CANVAS | R_LAYERS | R_HISTORY | R_CHANNELS | R_PATHS | R_NAV | R_TITLE);
}

/* ---------------- dispatch ---------------- */

void panel_paint(int p, HDC dc, const RECT *r)
{
    switch (p) {
    case P_NAVIGATOR: nav_paint(dc, r); break;
    case P_INFO: info_paint(dc, r); break;
    case P_COLOR: color_paint(dc, r); break;
    case P_SWATCHES: swatches_paint(dc, r); break;
    case P_BRUSHES: brushes_paint(dc, r); break;
    case P_LAYERS: layers_paint_top(dc, r); layers_paint(dc, r); break;
    case P_CHANNELS: channels_paint(dc, r); break;
    case P_PATHS: paths_paint(dc, r); break;
    case P_HISTORY: history_paint(dc, r); break;
    case P_ACTIONS: {
        RECT area;
        list_area(r, 0, 1, &area);
        actions_panel_paint(dc, r, g_scroll[P_ACTIONS]);
        break;
    }
    }
}

void panel_mouse(int p, HWND hwnd, const RECT *r, UINT msg, int x, int y, WPARAM wp)
{
    switch (p) {
    case P_NAVIGATOR: nav_mouse(hwnd, r, msg, x, y); break;
    case P_COLOR: color_mouse(hwnd, r, msg, x, y); break;
    case P_SWATCHES: swatches_mouse(hwnd, r, msg, x, y); break;
    case P_BRUSHES: brushes_mouse(hwnd, r, msg, x, y); break;
    case P_LAYERS: {
        RECT a;
        POINT pt = { x, y };
        SetRect(&a, r->right - 24, r->top + 3, r->right - 13, r->top + 23);
        if (msg == WM_LBUTTONDOWN && PtInRect(&a, pt) && app_doc() && pc_doc_active(app_doc())) {
            PcDoc *d = app_doc();
            PcLayer *l = pc_doc_active(d);
            int v = (l->opacity * 100 + 127) / 255, before = l->opacity;
            POINT sp = { a.left, a.bottom };
            ReleaseCapture();
            ClientToScreen(hwnd, &sp);
            popup_slider(hwnd, sp.x, sp.y, 0, 100, &v, opacity_live, NULL);
            if (l->opacity != before) app_commit(d, "Layer Opacity");
            return;
        }
        layers_mouse(hwnd, r, msg, x, y, wp);
        break;
    }
    case P_CHANNELS: channels_mouse(hwnd, r, msg, x, y); break;
    case P_PATHS: paths_mouse(hwnd, r, msg, x, y); break;
    case P_HISTORY: history_mouse(hwnd, r, msg, x, y); break;
    case P_ACTIONS: {
        int b;
        if (msg == WM_LBUTTONDOWN && (b = bottom_hit(r, 6, x, y)) >= 0) { actions_panel_buttons(b); InvalidateRect(hwnd, NULL, FALSE); return; }
        actions_panel_mouse(hwnd, r, msg, x, y, g_scroll[P_ACTIONS]);
        break;
    }
    }
    (void)wp;
}

void panel_menu(int p, HWND hwnd, int sx, int sy)
{
    static const MenuItem lay[] = {
        { 1, CMD_NEW_LAYER, "New Layer..." }, { 1, CMD_DUPLICATE_LAYER, NULL }, { 1, CMD_DELETE_LAYER, NULL }, { 1, 0, "-" },
        { 1, CMD_LAYER_PROPS, NULL }, { 1, CMD_LAYER_STYLE, "Layer Style..." }, { 1, 0, "-" },
        { 1, CMD_MERGE_DOWN, NULL }, { 1, CMD_MERGE_VISIBLE, NULL }, { 1, CMD_FLATTEN, NULL }
    };
    static const MenuItem chan[] = {
        { 1, CMD_CHANNEL_NEW, NULL }, { 1, CMD_CHANNEL_DUPLICATE, NULL }, { 1, CMD_CHANNEL_DELETE, NULL }, { 1, 0, "-" },
        { 1, CMD_SAVE_SELECTION, NULL }, { 1, CMD_LOAD_SELECTION, NULL }
    };
    static const MenuItem path[] = {
        { 1, CMD_PATH_SAVE, NULL }, { 1, CMD_PATH_DELETE, NULL }, { 1, 0, "-" }, { 1, CMD_SEL_TO_PATH, NULL },
        { 1, CMD_PATH_TO_SEL, NULL }, { 1, CMD_PATH_FILL, NULL }, { 1, CMD_PATH_STROKE, NULL }
    };
    static const MenuItem hist[] = {
        { 1, CMD_STEP_FORWARD, NULL }, { 1, CMD_STEP_BACKWARD, NULL }, { 1, 0, "-" },
        { 1, CMD_HISTORY_SNAPSHOT, NULL }, { 1, CMD_HISTORY_DELETE, NULL }, { 1, CMD_PURGE_HISTORIES, "Clear History" }, { 1, 0, "-" },
        { 1, CMD_HISTORY_NEWDOC, NULL }, { 1, CMD_PREFERENCES, "History Options..." }
    };
    static const MenuItem nav[] = { { 1, CMD_ZOOM_IN, NULL }, { 1, CMD_ZOOM_OUT, NULL }, { 1, CMD_FIT_SCREEN, NULL }, { 1, CMD_ACTUAL_PIXELS, NULL } };
    static const MenuItem brushes[] = { { 1, CMD_COUNT + 1, "Reset Brushes" } };
    static const MenuItem generic[] = { { 1, CMD_WS_RESET, "Reset Palette Locations" } };
    int cmd = 0;
    switch (p) {
    case P_LAYERS: cmd = menu_popup_items(lay, (int)(sizeof lay / sizeof lay[0]), sx, sy, hwnd); break;
    case P_CHANNELS: cmd = menu_popup_items(chan, (int)(sizeof chan / sizeof chan[0]), sx, sy, hwnd); break;
    case P_PATHS: cmd = menu_popup_items(path, (int)(sizeof path / sizeof path[0]), sx, sy, hwnd); break;
    case P_HISTORY: cmd = menu_popup_items(hist, (int)(sizeof hist / sizeof hist[0]), sx, sy, hwnd); break;
    case P_NAVIGATOR: cmd = menu_popup_items(nav, (int)(sizeof nav / sizeof nav[0]), sx, sy, hwnd); break;
    case P_ACTIONS: actions_panel_menu(hwnd, sx, sy); return;
    case P_BRUSHES:
        if (menu_popup_items(brushes, 1, sx, sy, hwnd)) { brushes_default(); brushes_save(); dock_refresh(R_ALL); }
        return;
    case P_SWATCHES: {
        static const MenuItem sw[] = { { 1, CMD_COUNT + 1, "Reset Swatches" } };
        if (menu_popup_items(sw, 1, sx, sy, hwnd)) { swatches_default(); swatches_save(); dock_refresh(R_ALL); }
        return;
    }
    default: cmd = menu_popup_items(generic, 1, sx, sy, hwnd); break;
    }
    if (cmd) cmd_run(cmd);
}

void panel_layout_controls(int p, HWND host, const RECT *r, int visible)
{
    if (!visible) return;
    if (p == P_LAYERS) layers_controls(host, r);
}

void panel_command(int p, int id, int code, HWND ctl)
{
    PcDoc *d = app_doc();
    PcLayer *l = d ? pc_doc_active(d) : NULL;
    (void)ctl;
    if (p == P_LAYERS && l) {
        if (id == 10 && code == CBN_SELCHANGE) {
            int b = (int)SendMessageA(g_lay_mode, CB_GETCURSEL, 0, 0);
            if (b >= 0 && b != l->blend) { l->blend = b; app_commit(d, "Blending Change"); }
            SetFocus(g_app.main);
        } else if (id == 11 && code == EN_KILLFOCUS) {
            char b[16];
            int v;
            GetWindowTextA(g_lay_opacity, b, sizeof b);
            v = PC_CLAMP((int)pc_atol(b), 0, 100) * 255 / 100;
            if (b[0] && v != l->opacity) { l->opacity = v; app_commit(d, "Layer Opacity"); }
        }
    }
}

void panel_timer(int p) { (void)p; }
