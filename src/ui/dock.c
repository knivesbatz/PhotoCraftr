/*
 * Panel dock and floating palettes.
 *
 * Arrangement follows Photoshop CS6's Essentials workspace (icon column +
 * expanded column of tabbed groups at the right); the chrome follows
 * Photoshop 5.0 (raised gray palettes, trapezoid tabs, white bold active
 * tab, palette menu triangle, thin navy caption on floating palettes).
 * Every panel's content lives in its own body window (panels.c) which is
 * re-parented into whichever host currently shows it.
 */
#include "app.h"

enum { HOST_COLUMN, HOST_ICONS, HOST_FLOAT };

typedef struct Group {
    int panels[P_COUNT];
    int n, active;
    int host;
    int collapsed;
    int visible;
    int height;          /* column height incl. tab row */
    RECT frect;          /* floating window rect (screen) */
    HWND fwnd;
    W95Frame fr;
} Group;

#define MAXG P_COUNT
static Group g_g[MAXG];
static int g_ng;
static HWND g_dock;
static HWND g_body[P_COUNT];
static int g_flyout_group = -1;
static HWND g_flyout;
static int g_column_iconic;     /* expanded column collapsed to icons (CS6 chevron) */

#define TAB_H 16
#define HEADER_H 11
#define ICON_COL_W 32
#define PANEL_COL_W 214
#define ICON_BTN_H 26
#define ADJ_GROUP_H 68      /* Adjustments: tab row, caption and one row of buttons */

static LRESULT CALLBACK body_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK float_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void relayout(void);

/* ---------------- group helpers ---------------- */

static int group_of(int p)
{
    int g, k;
    for (g = 0; g < g_ng; g++) for (k = 0; k < g_g[g].n; k++) if (g_g[g].panels[k] == p) return g;
    return -1;
}

static int new_group(int host)
{
    Group *gr;
    if (g_ng >= MAXG) return -1;
    gr = &g_g[g_ng];
    memset(gr, 0, sizeof *gr);
    gr->host = host; gr->visible = 1; gr->height = 160;
    return g_ng++;
}

static void group_destroy_window(Group *gr)
{
    int k;
    for (k = 0; k < gr->n; k++) if (g_body[gr->panels[k]]) { ShowWindow(g_body[gr->panels[k]], SW_HIDE); SetParent(g_body[gr->panels[k]], g_dock); }
    if (gr->fwnd) { HWND w = gr->fwnd; gr->fwnd = NULL; DestroyWindow(w); }
}

static void remove_empty_groups(void)
{
    int g = 0;
    while (g < g_ng) {
        if (g_g[g].n == 0) {
            group_destroy_window(&g_g[g]);
            memmove(&g_g[g], &g_g[g + 1], sizeof(Group) * (g_ng - g - 1));
            g_ng--;
            /* Window user data holds group indices; refresh them. */
            {
                int k;
                for (k = 0; k < g_ng; k++) if (g_g[k].fwnd) SetWindowLongA(g_g[k].fwnd, GWL_USERDATA, k);
            }
            if (g_flyout_group == g) g_flyout_group = -1;
            else if (g_flyout_group > g) g_flyout_group--;
        } else {
            g++;
        }
    }
}

static void detach_panel(int p)
{
    int g = group_of(p), k;
    Group *gr;
    if (g < 0) return;
    gr = &g_g[g];
    for (k = 0; k < gr->n; k++) if (gr->panels[k] == p) break;
    memmove(&gr->panels[k], &gr->panels[k + 1], sizeof(int) * (gr->n - k - 1));
    gr->n--;
    if (gr->active >= gr->n) gr->active = gr->n - 1;
    if (gr->active < 0) gr->active = 0;
    ShowWindow(g_body[p], SW_HIDE);
    SetParent(g_body[p], g_dock);
}

static void add_panel_to(int g, int p, int activate)
{
    Group *gr = &g_g[g];
    if (gr->n >= P_COUNT) return;
    gr->panels[gr->n++] = p;
    if (activate) gr->active = gr->n - 1;
    gr->visible = 1;
}

/* ---------------- floating hosts ---------------- */

static void ensure_float_window(int g)
{
    Group *gr = &g_g[g];
    static int reg;
    if (gr->fwnd) return;
    if (!reg) { w95_register_class("PCPalette", float_proc, CS_DBLCLKS, NULL); reg = 1; }
    if (gr->frect.right <= gr->frect.left) {
        RECT mr;
        GetWindowRect(g_app.main, &mr);
        SetRect(&gr->frect, mr.right - PANEL_COL_W - 40, mr.top + 80, mr.right - 40, mr.top + 80 + 220);
    }
    gr->fwnd = CreateWindowExA(WS_EX_TOOLWINDOW, "PCPalette", panel_name(gr->panels[gr->active]), WS_POPUP | WS_CLIPCHILDREN,
                               gr->frect.left, gr->frect.top, gr->frect.right - gr->frect.left, gr->frect.bottom - gr->frect.top,
                               g_app.main, NULL, w95_inst(), NULL);
    if (!gr->fwnd) return;
    SetWindowLongA(gr->fwnd, GWL_USERDATA, g);
    w95_frame_attach(gr->fwnd, &gr->fr, W95_FRAME_SMALL | W95_FRAME_CLOSE | W95_FRAME_SIZABLE);
    gr->fr.min_w = 150; gr->fr.min_h = 40;
}

/* ---------------- geometry ---------------- */

static int has_host(int host)
{
    int g;
    for (g = 0; g < g_ng; g++) if (g_g[g].host == host && g_g[g].visible && g_g[g].n) return 1;
    return 0;
}

int dock_width(void)
{
    int w = 0;
    if (has_host(HOST_ICONS) || (g_column_iconic && has_host(HOST_COLUMN))) w += ICON_COL_W;
    if (!g_column_iconic && has_host(HOST_COLUMN)) w += PANEL_COL_W;
    return w ? w + 2 : 0;
}

static int column_x(void) { return (has_host(HOST_ICONS) || g_column_iconic) ? ICON_COL_W + 1 : 1; }

/* Body rect of each column group in dock client coords. */
static void column_rects(RECT *out, int *idx, int *count)
{
    RECT r;
    int g, n = 0, y, total, fixed = 0, last = -1, x0, x1;
    GetClientRect(g_dock, &r);
    x0 = column_x(); x1 = x0 + PANEL_COL_W;
    y = HEADER_H + 1;
    for (g = 0; g < g_ng; g++) if (g_g[g].host == HOST_COLUMN && g_g[g].visible && g_g[g].n) { idx[n++] = g; last = g; }
    total = r.bottom - y - 1;
    for (g = 0; g < n; g++) if (idx[g] != last) fixed += g_g[idx[g]].collapsed ? TAB_H + 4 : g_g[idx[g]].height;
    for (g = 0; g < n; g++) {
        Group *gr = &g_g[idx[g]];
        int h = gr->collapsed ? TAB_H + 4 : idx[g] == last ? PC_MAX(TAB_H + 4, total - fixed) : gr->height;
        if (y + h > r.bottom - 1) h = PC_MAX(TAB_H + 4, r.bottom - 1 - y);
        SetRect(&out[g], x0, y, x1, y + h);
        y += h + 1;
    }
    *count = n;
}

static void icon_rects(RECT *out, int *idx, int *count)
{
    int g, n = 0, y = HEADER_H + 1;
    for (g = 0; g < g_ng; g++) {
        Group *gr = &g_g[g];
        int k;
        if (!gr->visible || !gr->n) continue;
        if (!(gr->host == HOST_ICONS || (g_column_iconic && gr->host == HOST_COLUMN))) continue;
        for (k = 0; k < gr->n && n < 32; k++) {
            SetRect(&out[n], 1, y, ICON_COL_W, y + ICON_BTN_H);
            idx[n] = g * 100 + k;
            n++;
            y += ICON_BTN_H;
        }
        y += 4;
    }
    *count = n;
}

/* Tab geometry within a host rect: trapezoid tabs, PS5 style. */
static int tab_rects(const Group *gr, const RECT *host, RECT *tabs, HDC dc)
{
    int k, x = host->left + 3;
    for (k = 0; k < gr->n; k++) {
        int w = w95_text_width(dc, gr->active == k ? W95_FONT_BOLD : W95_FONT_NORMAL, panel_name(gr->panels[k])) + 18;
        SetRect(&tabs[k], x, host->top + 2, x + w, host->top + TAB_H + 1);
        x += w - 6;
    }
    return gr->n;
}

static void paint_tabs(HDC dc, const Group *gr, const RECT *host)
{
    RECT tabs[P_COUNT], line, mb;
    int k, n = tab_rects(gr, host, tabs, dc);
    /* baseline */
    line = *host;
    w95_hline(dc, host->left + 1, host->right - 1, host->top + TAB_H + 1, W95_DKSHADOW);
    /* inactive tabs first, then the active one on top */
    for (k = n - 1; k >= -1; k--) {
        int t = k >= 0 ? k : gr->active, act = k < 0;
        RECT r;
        int y;
        if (!act && t == gr->active) continue;
        if (t < 0 || t >= n) continue;
        r = tabs[t];
        for (y = r.top; y <= r.bottom; y++) {
            int slant = y - r.top;
            int x0 = r.left, x1 = r.right - 1 - (TAB_H - 1 - slant) / 2;
            (void)x1;
            w95_hline(dc, x0, r.right - (r.bottom - y) / 2 - 1, y, act ? W95_WINDOW : W95_FACE);
        }
        w95_vline(dc, r.left, r.top, r.bottom + (act ? 1 : 0), W95_HILIGHT);
        w95_hline(dc, r.left, r.right - TAB_H / 2, r.top, act ? W95_DKSHADOW : W95_SHADOW);
        /* slanted right edge (45 degrees) */
        for (y = r.top; y <= r.bottom; y++) w95_pixel(dc, r.right - (r.bottom - y) / 2 - 1, y, W95_DKSHADOW);
        w95_vline(dc, r.left - 1, r.top + 1, r.bottom + 1, act ? W95_DKSHADOW : W95_SHADOW);
        {
            RECT tr = r;
            tr.left += 6;
            w95_text(dc, act ? W95_FONT_BOLD : W95_FONT_NORMAL, panel_name(gr->panels[t]), &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
        }
        if (act) w95_hline(dc, r.left, r.right - 2, host->top + TAB_H + 1, W95_WINDOW);
    }
    /* Palette menu button (triangle with lines) at the right end of the tab row. */
    SetRect(&mb, host->right - 16, host->top + 3, host->right - 3, host->top + TAB_H);
    w95_icon(dc, PC_ICON_PANEL_MENU, mb.left + 1, mb.top + 2);
    (void)line;
}

static void position_bodies(void)
{
    RECT rects[MAXG];
    int idx[MAXG], n, i, g, k;
    /* Hide every body first; show the active ones in their hosts. */
    for (k = 0; k < P_COUNT; k++) if (g_body[k]) ShowWindow(g_body[k], SW_HIDE);
    column_rects(rects, idx, &n);
    if (!g_column_iconic) {
        for (i = 0; i < n; i++) {
            Group *gr = &g_g[idx[i]];
            int p = gr->panels[gr->active];
            if (gr->collapsed) continue;
            SetParent(g_body[p], g_dock);
            SetWindowPos(g_body[p], NULL, rects[i].left + 2, rects[i].top + TAB_H + 2, rects[i].right - rects[i].left - 4,
                         PC_MAX(1, rects[i].bottom - rects[i].top - TAB_H - 4), SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
    for (g = 0; g < g_ng; g++) {
        Group *gr = &g_g[g];
        if (gr->host == HOST_FLOAT) {
            if (!gr->visible || !gr->n || g_app.hide_all || g_app.hide_panels) { if (gr->fwnd) ShowWindow(gr->fwnd, SW_HIDE); continue; }
            ensure_float_window(g);
            if (gr->fwnd) {
                RECT c;
                int p = gr->panels[gr->active];
                SetWindowTextA(gr->fwnd, "");
                GetClientRect(gr->fwnd, &c);
                if (!gr->collapsed) {
                    SetParent(g_body[p], gr->fwnd);
                    SetWindowPos(g_body[p], NULL, 2, TAB_H + 2, PC_MAX(1, c.right - 4), PC_MAX(1, c.bottom - TAB_H - 4), SWP_NOZORDER | SWP_SHOWWINDOW);
                }
                ShowWindow(gr->fwnd, SW_SHOWNOACTIVATE);
                InvalidateRect(gr->fwnd, NULL, FALSE);
            }
        } else if (gr->fwnd && g != g_flyout_group) {
            group_destroy_window(gr);
        }
    }
    if (g_flyout_group >= 0 && g_flyout_group < g_ng && g_flyout) {
        Group *gr = &g_g[g_flyout_group];
        RECT c;
        int p = gr->panels[gr->active];
        GetClientRect(g_flyout, &c);
        SetParent(g_body[p], g_flyout);
        SetWindowPos(g_body[p], NULL, 2, TAB_H + 2, c.right - 4, PC_MAX(1, c.bottom - TAB_H - 4), SWP_NOZORDER | SWP_SHOWWINDOW);
        for (k = 0; k < gr->n; k++) panel_layout_controls(gr->panels[k], g_body[gr->panels[k]], &c, k == gr->active);
    }
}

static void relayout(void)
{
    int old = 0;
    RECT r;
    if (!g_dock) return;
    GetClientRect(g_dock, &r);
    old = r.right;
    if (old != dock_width()) app_layout();
    position_bodies();
    InvalidateRect(g_dock, NULL, FALSE);
}

/* ---------------- painting ---------------- */

static void paint_header(HDC dc, int x0, int x1, int chevron_left)
{
    RECT h;
    SetRect(&h, x0, 1, x1, HEADER_H);
    w95_fill(dc, &h, W95_FACE);
    w95_thin_raised(dc, &h);
    w95_hline(dc, x0 + 3, x1 - 12, 4, W95_HILIGHT); w95_hline(dc, x0 + 3, x1 - 12, 5, W95_SHADOW);
    w95_hline(dc, x0 + 3, x1 - 12, 7, W95_HILIGHT);
    w95_icon(dc, chevron_left ? PC_ICON_CHEVRON_LEFT : PC_ICON_CHEVRON_RIGHT, x1 - 10, 2);
}

static void dock_paint(HWND hwnd, HDC target)
{
    RECT r, rects[MAXG], ir[32];
    int idx[MAXG], iidx[32], n, ni, i;
    W95Buf b;
    HDC dc;
    GetClientRect(hwnd, &r);
    dc = w95_buf_begin(&b, target, &r);
    w95_fill(dc, &r, W95_SHADOW);
    /* Icon column. */
    icon_rects(ir, iidx, &ni);
    if (ni || g_column_iconic) {
        RECT c;
        SetRect(&c, 0, 0, ICON_COL_W + 1, r.bottom);
        w95_fill(dc, &c, W95_FACE);
        w95_raised(dc, &c);
        paint_header(dc, 1, ICON_COL_W, 1);
        for (i = 0; i < ni; i++) {
            Group *gr = &g_g[iidx[i] / 100];
            int p = gr->panels[iidx[i] % 100];
            int open = g_flyout_group == iidx[i] / 100 && gr->active == iidx[i] % 100;
            RECT bt = ir[i];
            InflateRect(&bt, -2, -1);
            w95_draw_icon_button(dc, &bt, panel_icon(p), open ? 3 : 0, 0);
            if (!open) w95_thin_raised(dc, &bt);
        }
    }
    /* Panel column. */
    if (!g_column_iconic && has_host(HOST_COLUMN)) {
        int x0 = column_x();
        RECT col;
        SetRect(&col, x0, 0, x0 + PANEL_COL_W + 1, r.bottom);
        w95_fill(dc, &col, W95_SHADOW);
        paint_header(dc, x0, x0 + PANEL_COL_W, 0);
        column_rects(rects, idx, &n);
        for (i = 0; i < n; i++) {
            RECT gr = rects[i];
            w95_fill(dc, &gr, W95_FACE);
            w95_raised(dc, &gr);
            paint_tabs(dc, &g_g[idx[i]], &gr);
        }
    }
    w95_buf_end(&b);
}

/* ---------------- drag & drop of tabs ---------------- */

typedef struct { int panel; int whole_group; int group; POINT start; RECT feedback; int shown; } TabDrag;
static TabDrag g_drag;

static void draw_feedback(const RECT *r)
{
    HDC dc = GetDC(NULL);
    RECT a = *r;
    int k;
    for (k = 0; k < 3; k++) { DrawFocusRect(dc, &a); InflateRect(&a, -1, -1); }
    ReleaseDC(NULL, dc);
}

/* Where would a drop at screen point p go? Returns target group (or -1),
 * *mode: 0 merge into group, 1 new column group before index *at, 2 new icon group, 3 float. */
static int drop_target(POINT p, int *mode, int *at)
{
    RECT dr, rects[MAXG];
    int idx[MAXG], n, i, g;
    POINT dp = p;
    *mode = 3; *at = -1;
    /* Floating palettes' tab rows. */
    for (g = 0; g < g_ng; g++) {
        RECT wr;
        if (!g_g[g].fwnd || !IsWindowVisible(g_g[g].fwnd)) continue;
        GetWindowRect(g_g[g].fwnd, &wr);
        if (PtInRect(&wr, p)) { *mode = 0; return g; }
    }
    GetWindowRect(g_dock, &dr);
    if (!IsWindowVisible(g_dock) || !PtInRect(&dr, p)) return -1;
    ScreenToClient(g_dock, &dp);
    if (dp.x < column_x() - 1 && (has_host(HOST_ICONS) || g_column_iconic)) { *mode = 2; return -1; }
    column_rects(rects, idx, &n);
    for (i = 0; i < n; i++) {
        if (dp.y >= rects[i].top && dp.y < rects[i].top + TAB_H + 2) { *mode = 0; return idx[i]; }
        if (dp.y >= rects[i].top && dp.y < rects[i].bottom) {
            /* Lower part of a group: insert a new group after it. */
            if (dp.y > rects[i].bottom - 12) { *mode = 1; *at = i + 1; return -1; }
            *mode = 0; return idx[i];
        }
    }
    *mode = 1; *at = n;
    return -1;
}

static void do_drop(POINT p)
{
    int mode, at, tg = drop_target(p, &mode, &at), src = g_drag.group;
    int panels[P_COUNT], np = 0, k;
    if (src < 0 || src >= g_ng) return;
    if (g_drag.whole_group) { for (k = 0; k < g_g[src].n; k++) panels[np++] = g_g[src].panels[k]; }
    else panels[np++] = g_drag.panel;
    if (mode == 0 && tg == src) return;
    if (mode == 3 && !g_drag.whole_group && g_g[src].n == 1 && g_g[src].host == HOST_FLOAT) {
        /* Dragging the only tab of a floating palette just moves it. */
        RECT wr;
        GetWindowRect(g_g[src].fwnd, &wr);
        OffsetRect(&wr, p.x - g_drag.start.x, p.y - g_drag.start.y);
        g_g[src].frect = wr;
        SetWindowPos(g_g[src].fwnd, NULL, wr.left, wr.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }
    for (k = 0; k < np; k++) detach_panel(panels[k]);
    if (mode == 0 && tg >= 0) {
        for (k = 0; k < np; k++) add_panel_to(tg, panels[k], 1);
    } else {
        int host = mode == 1 ? HOST_COLUMN : mode == 2 ? HOST_ICONS : HOST_FLOAT;
        int g = new_group(host);
        if (g < 0) return;
        for (k = 0; k < np; k++) add_panel_to(g, panels[k], k == 0);
        if (host == HOST_FLOAT) {
            int h = PC_MAX(140, panel_pref_height(panels[0]) + TAB_H + 20);
            SetRect(&g_g[g].frect, p.x - 40, p.y - 8, p.x - 40 + PANEL_COL_W + 6, p.y - 8 + h);
        }
        if (host == HOST_COLUMN) {
            /* Move the new group into position `at` among column groups. */
            int ci = 0, pos = g_ng - 1, gg;
            Group tmp = g_g[g];
            g_g[g].height = PC_MAX(100, panel_pref_height(panels[0]) + TAB_H + 6);
            tmp = g_g[g];
            for (gg = 0; gg < g_ng - 1; gg++) if (g_g[gg].host == HOST_COLUMN) { if (ci == at) { pos = gg; break; } ci++; }
            memmove(&g_g[pos + 1], &g_g[pos], sizeof(Group) * (g_ng - 1 - pos));
            g_g[pos] = tmp;
            for (gg = 0; gg < g_ng; gg++) if (g_g[gg].fwnd) SetWindowLongA(g_g[gg].fwnd, GWL_USERDATA, gg);
        }
    }
    remove_empty_groups();
    relayout();
    dock_save_workspace();
}

/* Shared tab-row mouse handling for the dock and floating hosts.
 * Returns 1 when consumed. */
static int tabrow_mouse(HWND hwnd, int g, const RECT *host, UINT msg, int x, int y)
{
    Group *gr = &g_g[g];
    RECT tabs[P_COUNT], mb;
    HDC dc;
    int k, n;
    POINT p;
    if (y < host->top || y >= host->top + TAB_H + 2 || x < host->left || x >= host->right) return 0;
    p.x = x; p.y = y;
    SetRect(&mb, host->right - 16, host->top + 2, host->right - 2, host->top + TAB_H);
    if (PtInRect(&mb, p) && msg == WM_LBUTTONDOWN) {
        POINT sp = { mb.left, mb.bottom };
        ClientToScreen(hwnd, &sp);
        panel_menu(gr->panels[gr->active], hwnd, sp.x, sp.y);
        return 1;
    }
    dc = GetDC(hwnd);
    n = tab_rects(gr, host, tabs, dc);
    ReleaseDC(hwnd, dc);
    if (msg == WM_LBUTTONDBLCLK) { gr->collapsed = !gr->collapsed; relayout(); if (gr->fwnd) { RECT wr; GetWindowRect(gr->fwnd, &wr); if (gr->collapsed) { gr->frect = wr; SetWindowPos(gr->fwnd, NULL, 0, 0, wr.right - wr.left, TAB_H + 4 + W95_SMCAPTION_H + 6, SWP_NOMOVE | SWP_NOZORDER); } else SetWindowPos(gr->fwnd, NULL, 0, 0, gr->frect.right - gr->frect.left, gr->frect.bottom - gr->frect.top, SWP_NOMOVE | SWP_NOZORDER); position_bodies(); } return 1; }
    if (msg != WM_LBUTTONDOWN) return 0;
    for (k = 0; k < n; k++) {
        if (PtInRect(&tabs[k], p)) {
            if (gr->active != k) { gr->active = k; relayout(); if (gr->fwnd) InvalidateRect(gr->fwnd, NULL, FALSE); }
            g_drag.panel = gr->panels[k]; g_drag.whole_group = 0;
            break;
        }
    }
    if (k == n) { g_drag.whole_group = 1; g_drag.panel = gr->panels[gr->active]; }
    g_drag.group = g;
    GetCursorPos(&g_drag.start);
    g_drag.shown = 0;
    SetCapture(hwnd);
    return 1;
}

static void drag_move(void)
{
    POINT p;
    RECT fb;
    GetCursorPos(&p);
    if (!g_drag.shown && abs(p.x - g_drag.start.x) < 4 && abs(p.y - g_drag.start.y) < 4) return;
    if (g_drag.shown) draw_feedback(&g_drag.feedback);
    SetRect(&fb, p.x - 40, p.y - 8, p.x - 40 + PANEL_COL_W, p.y - 8 + 160);
    {
        int mode, at, tg = drop_target(p, &mode, &at);
        if (mode == 0 && tg >= 0) {
            if (g_g[tg].fwnd) GetWindowRect(g_g[tg].fwnd, &fb);
            else {
                RECT rects[MAXG];
                int idx[MAXG], n, i;
                column_rects(rects, idx, &n);
                for (i = 0; i < n; i++) if (idx[i] == tg) { fb = rects[i]; MapWindowPoints(g_dock, NULL, (POINT *)&fb, 2); }
            }
        }
    }
    g_drag.feedback = fb;
    draw_feedback(&fb);
    g_drag.shown = 1;
}

static int drag_end(void)
{
    POINT p;
    int was = g_drag.shown;
    GetCursorPos(&p);
    if (g_drag.shown) draw_feedback(&g_drag.feedback);
    g_drag.shown = 0;
    ReleaseCapture();
    if (was) do_drop(p);
    g_drag.group = -1;
    return was;
}

/* ---------------- icon column flyouts ---------------- */

static LRESULT CALLBACK flyout_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void close_flyout(void)
{
    if (g_flyout) {
        HWND w = g_flyout;
        int g = g_flyout_group;
        g_flyout = NULL;
        g_flyout_group = -1;
        if (g >= 0 && g < g_ng) {
            int k;
            for (k = 0; k < g_g[g].n; k++) { ShowWindow(g_body[g_g[g].panels[k]], SW_HIDE); SetParent(g_body[g_g[g].panels[k]], g_dock); }
        }
        DestroyWindow(w);
        InvalidateRect(g_dock, NULL, FALSE);
    }
}

static void open_flyout(int g, int k, int y)
{
    POINT p;
    int h;
    static int reg;
    if (!reg) { w95_register_class("PCDockFlyout", flyout_proc, CS_DBLCLKS, NULL); reg = 1; }
    close_flyout();
    g_g[g].active = k;
    g_flyout_group = g;
    h = PC_MAX(150, panel_pref_height(g_g[g].panels[k]) + TAB_H + 8);
    p.x = 0; p.y = y;
    ClientToScreen(g_dock, &p);
    g_flyout = CreateWindowExA(WS_EX_TOOLWINDOW, "PCDockFlyout", "", WS_POPUP | WS_CLIPCHILDREN | WS_BORDER,
                               p.x - PANEL_COL_W - 2, PC_MAX(0, p.y - 4), PANEL_COL_W + 2, h, g_app.main, NULL, w95_inst(), NULL);
    if (!g_flyout) { g_flyout_group = -1; return; }
    ShowWindow(g_flyout, SW_SHOWNOACTIVATE);
    position_bodies();
    InvalidateRect(g_dock, NULL, FALSE);
}

static LRESULT CALLBACK flyout_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int g = g_flyout_group;
    RECT c;
    GetClientRect(hwnd, &c);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        w95_fill(dc, &c, W95_FACE);
        w95_raised(dc, &c);
        if (g >= 0 && g < g_ng) paint_tabs(dc, &g_g[g], &c);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        if (g >= 0 && g < g_ng) tabrow_mouse(hwnd, g, &c, msg, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_MOUSEMOVE: if (GetCapture() == hwnd && g_drag.group >= 0) drag_move(); return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == hwnd) { if (drag_end()) close_flyout(); }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------------- window procedures ---------------- */

static LRESULT CALLBACK dock_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        dock_paint(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: position_bodies(); InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
        RECT rects[MAXG], ir[32];
        int idx[MAXG], iidx[32], n, ni, i;
        POINT p;
        p.x = x; p.y = y;
        if (y < HEADER_H) {
            /* Header chevrons: collapse the panel column to icons / expand it. */
            if (x > column_x() + PANEL_COL_W - 14 || (g_column_iconic && x < ICON_COL_W)) {
                g_column_iconic = !g_column_iconic;
                close_flyout();
                app_layout();
                relayout();
                dock_save_workspace();
            }
            return 0;
        }
        icon_rects(ir, iidx, &ni);
        for (i = 0; i < ni; i++) {
            if (PtInRect(&ir[i], p)) {
                int g = iidx[i] / 100, k = iidx[i] % 100;
                if (g_flyout_group == g && g_g[g].active == k) close_flyout();
                else open_flyout(g, k, ir[i].top);
                return 0;
            }
        }
        if (!g_column_iconic) {
            column_rects(rects, idx, &n);
            for (i = 0; i < n; i++) if (tabrow_mouse(hwnd, idx[i], &rects[i], msg, x, y)) return 0;
            /* Splitter drag between groups (CS6). */
            for (i = 0; i + 1 < n; i++) {
                if (y >= rects[i].bottom - 2 && y <= rects[i].bottom + 2) {
                    g_drag.group = -2 - i;
                    SetCapture(hwnd);
                    return 0;
                }
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (GetCapture() == hwnd) {
            if (g_drag.group >= 0) drag_move();
            else if (g_drag.group <= -2) {
                RECT rects[MAXG];
                int idx[MAXG], n, i = -2 - g_drag.group;
                column_rects(rects, idx, &n);
                if (i < n) { g_g[idx[i]].height = PC_MAX(TAB_H + 30, y - rects[i].top); relayout(); }
            }
        } else {
            RECT rects[MAXG];
            int idx[MAXG], n, i;
            column_rects(rects, idx, &n);
            for (i = 0; i + 1 < n; i++) if (y >= rects[i].bottom - 2 && y <= rects[i].bottom + 2 && !g_column_iconic) SetCursor(LoadCursor(NULL, IDC_SIZENS));
        }
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == hwnd) {
            if (g_drag.group >= 0) drag_end();
            else { ReleaseCapture(); g_drag.group = -1; dock_save_workspace(); }
        }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK float_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int g = (int)GetWindowLongA(hwnd, GWL_USERDATA);
    Group *gr = (g >= 0 && g < g_ng && g_g[g].fwnd == hwnd) ? &g_g[g] : NULL;
    LRESULT res;
    if (gr && w95_frame_proc(hwnd, &gr->fr, msg, wp, lp, &res)) return res;
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT c;
        GetClientRect(hwnd, &c);
        w95_fill(dc, &c, W95_FACE);
        if (gr) paint_tabs(dc, gr, &c);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: case WM_MOVE:
        if (gr) {
            GetWindowRect(hwnd, &gr->frect);
            if (msg == WM_SIZE && !gr->collapsed) {
                RECT c;
                int p = gr->panels[gr->active];
                GetClientRect(hwnd, &c);
                if (GetParent(g_body[p]) == hwnd) SetWindowPos(g_body[p], NULL, 2, TAB_H + 2, PC_MAX(1, c.right - 4), PC_MAX(1, c.bottom - TAB_H - 4), SWP_NOZORDER);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    case WM_NCLBUTTONDBLCLK:
        /* Double-click the palette caption: collapse to the tab row (PS5). */
        if (gr && wp == HTCAPTION) {
            RECT c;
            GetClientRect(hwnd, &c);
            tabrow_mouse(hwnd, g, &c, WM_LBUTTONDBLCLK, 5, 5);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        if (gr) {
            RECT c;
            GetClientRect(hwnd, &c);
            tabrow_mouse(hwnd, g, &c, msg, (short)LOWORD(lp), (short)HIWORD(lp));
        }
        return 0;
    case WM_MOUSEMOVE: if (GetCapture() == hwnd && g_drag.group >= 0) drag_move(); return 0;
    case WM_LBUTTONUP: if (GetCapture() == hwnd) drag_end(); return 0;
    case WM_CLOSE:
        if (gr) { gr->visible = 0; ShowWindow(hwnd, SW_HIDE); dock_save_workspace(); }
        return 0;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* Panel body: delegates to panels.c. */
static LRESULT CALLBACK body_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int p = (int)GetWindowLongA(hwnd, GWL_USERDATA);
    if (p < 0 || p >= P_COUNT) return DefWindowProcA(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r;
        W95Buf b;
        HDC bdc;
        GetClientRect(hwnd, &r);
        bdc = w95_buf_begin(&b, dc, &r);
        w95_fill(bdc, &r, W95_FACE);
        panel_paint(p, bdc, &r);
        w95_buf_end(&b);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: {
        RECT r;
        GetClientRect(hwnd, &r);
        panel_layout_controls(p, hwnd, &r, 1);
        return 0;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_MOUSEMOVE: case WM_LBUTTONDBLCLK: case WM_RBUTTONDOWN: case WM_RBUTTONUP: {
        RECT r;
        GetClientRect(hwnd, &r);
        if (msg == WM_LBUTTONDOWN) SetCapture(hwnd);
        if (msg == WM_LBUTTONUP && GetCapture() == hwnd) ReleaseCapture();
        panel_mouse(p, hwnd, &r, msg, (short)LOWORD(lp), (short)HIWORD(lp), wp);
        return 0;
    }
    case WM_TIMER: panel_timer(p); return 0;
    case WM_COMMAND: panel_command(p, LOWORD(wp), HIWORD(wp), (HWND)lp); return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: {
        static HBRUSH face;
        if (!face) face = CreateSolidBrush(W95_FACE);
        SetBkColor((HDC)wp, W95_FACE);
        return (LRESULT)face;
    }
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------------- workspace ---------------- */

static void build_default(void)
{
    int g, k;
    for (g = 0; g < g_ng; g++) group_destroy_window(&g_g[g]);
    g_ng = 0;
    g_column_iconic = 0;
    /* Icon column: History | Actions, Brushes.  Panel column: Color | Swatches,
     * Navigator | Info, Adjustments (CS6 keeps it right above Layers),
     * Layers | Channels | Paths (see docs/historical-ui-reference.md). */
    g = new_group(HOST_ICONS); add_panel_to(g, P_HISTORY, 1); add_panel_to(g, P_ACTIONS, 0); g_g[g].active = 0;
    g = new_group(HOST_ICONS); add_panel_to(g, P_BRUSHES, 1);
    g = new_group(HOST_COLUMN); add_panel_to(g, P_COLOR, 1); add_panel_to(g, P_SWATCHES, 0); g_g[g].active = 0; g_g[g].height = 112;
    g = new_group(HOST_COLUMN); add_panel_to(g, P_NAVIGATOR, 1); add_panel_to(g, P_INFO, 0); g_g[g].active = 0; g_g[g].height = 142;
    g = new_group(HOST_COLUMN); add_panel_to(g, P_ADJUSTMENTS, 1); g_g[g].height = ADJ_GROUP_H;
    g = new_group(HOST_COLUMN); add_panel_to(g, P_LAYERS, 1); add_panel_to(g, P_CHANNELS, 0); add_panel_to(g, P_PATHS, 0); g_g[g].active = 0;
    /* 800x600: Navigator|Info starts collapsed; 640x480: Adjustments too (double-click a tab to open). */
    if (GetSystemMetrics(SM_CYSCREEN) < 700) g_g[3].collapsed = 1;
    if (GetSystemMetrics(SM_CYSCREEN) < 560) g_g[4].collapsed = 1;
    for (k = 0; k < P_COUNT; k++) if (g_body[k]) ShowWindow(g_body[k], SW_HIDE);
}

static void build_classic98(void)
{
    /* Photoshop 5.0's own default: four floating palette groups down the right edge. */
    static const int groups[4][4] = { { P_NAVIGATOR, P_INFO, -1, -1 }, { P_COLOR, P_SWATCHES, P_BRUSHES, -1 }, { P_HISTORY, P_ACTIONS, -1, -1 },
                                      { P_LAYERS, P_CHANNELS, P_PATHS, P_ADJUSTMENTS } };
    static const int heights[4] = { 190, 170, 170, 230 };
    RECT wa;
    int g, i, k, y;
    for (g = 0; g < g_ng; g++) group_destroy_window(&g_g[g]);
    g_ng = 0;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    y = wa.top + 60;
    for (i = 0; i < 4; i++) {
        g = new_group(HOST_FLOAT);
        for (k = 0; k < 4; k++) if (groups[i][k] >= 0) add_panel_to(g, groups[i][k], k == 0);
        g_g[g].active = 0;
        SetRect(&g_g[g].frect, wa.right - PANEL_COL_W - 14, y, wa.right - 8, y + heights[i]);
        y += heights[i] + 4;
        if (y > wa.bottom - 100) y = wa.top + 60 + i * 20;
    }
}

void dock_reset_workspace(void)
{
    close_flyout();
    build_default();
    app_layout();
    relayout();
    dock_refresh(R_ALL);
    dock_save_workspace();
}

void dock_classic_workspace(void)
{
    close_flyout();
    build_classic98();
    app_layout();
    relayout();
    dock_refresh(R_ALL);
    dock_save_workspace();
}

/* Registry layout.  "Layout2" holds SavedGroup[MAXG]; 0.1.0 wrote "Layout"
 * with room for its 10 panels only (SavedGroupV1), which is still read: panels
 * it does not know (Adjustments) are then added where Essentials has them. */
typedef struct { int host, n, active, collapsed, visible, height; int panels[P_COUNT]; RECT frect; } SavedGroup;
typedef struct { int host, n, active, collapsed, visible, height; int panels[P_COUNT_V1]; RECT frect; } SavedGroupV1;

void dock_save_workspace(void)
{
    SavedGroup s[MAXG];
    int g;
    memset(s, 0, sizeof s);
    for (g = 0; g < g_ng; g++) {
        s[g].host = g_g[g].host; s[g].n = g_g[g].n; s[g].active = g_g[g].active; s[g].collapsed = g_g[g].collapsed;
        s[g].visible = g_g[g].visible; s[g].height = g_g[g].height; s[g].frect = g_g[g].frect;
        memcpy(s[g].panels, g_g[g].panels, sizeof s[g].panels);
    }
    reg_set_int("Workspace", "Groups", g_ng);
    reg_set_int("Workspace", "ColumnIconic", g_column_iconic);
    reg_set_bin("Workspace", "Layout2", s, sizeof s);
}

/* Put a panel missing from a loaded layout back where Essentials keeps it. */
static void add_missing_panel(int p)
{
    int g, lg = group_of(P_LAYERS), ng;
    if (p == P_ADJUSTMENTS && lg >= 0 && g_g[lg].host == HOST_COLUMN && g_ng < MAXG) {
        Group t;
        ng = new_group(HOST_COLUMN);
        add_panel_to(ng, p, 1);
        g_g[ng].height = ADJ_GROUP_H;
        g_g[ng].collapsed = GetSystemMetrics(SM_CYSCREEN) < 560;
        /* move it just above the Layers group */
        t = g_g[ng];
        memmove(&g_g[lg + 1], &g_g[lg], sizeof(Group) * (ng - lg));
        g_g[lg] = t;
        return;
    }
    if (lg >= 0) { add_panel_to(lg, p, 0); return; }
    g = new_group(HOST_FLOAT);
    if (g >= 0) add_panel_to(g, p, 1);
    else if (g_ng > 0) add_panel_to(0, p, 0);
}

static int load_from(const char *sub)
{
    SavedGroup s[MAXG];
    int n = reg_get_int(sub, "Groups", 0), g, k, seen[P_COUNT];
    if (n <= 0 || n > MAXG) return 0;
    memset(s, 0, sizeof s);
    if (!reg_get_bin(sub, "Layout2", s, sizeof s)) {
        SavedGroupV1 v[P_COUNT_V1];
        if (n > P_COUNT_V1 || !reg_get_bin(sub, "Layout", v, sizeof v)) return 0;
        for (g = 0; g < n; g++) {
            s[g].host = v[g].host; s[g].n = v[g].n; s[g].active = v[g].active; s[g].collapsed = v[g].collapsed;
            s[g].visible = v[g].visible; s[g].height = v[g].height; s[g].frect = v[g].frect;
            if (v[g].n < 0 || v[g].n > P_COUNT_V1) return 0;
            for (k = 0; k < v[g].n; k++) s[g].panels[k] = v[g].panels[k];
        }
    }
    memset(seen, 0, sizeof seen);
    for (g = 0; g < n; g++) {
        if (s[g].n < 0 || s[g].n > P_COUNT || s[g].host < 0 || s[g].host > HOST_FLOAT) return 0;
        for (k = 0; k < s[g].n; k++) { if (s[g].panels[k] < 0 || s[g].panels[k] >= P_COUNT || seen[s[g].panels[k]]) return 0; seen[s[g].panels[k]] = 1; }
    }
    for (k = 0; k < P_COUNT_V1; k++) if (!seen[k]) return 0;
    for (g = 0; g < g_ng; g++) group_destroy_window(&g_g[g]);
    g_ng = 0;
    for (g = 0; g < n; g++) {
        int ng = new_group(s[g].host);
        for (k = 0; k < s[g].n; k++) add_panel_to(ng, s[g].panels[k], 0);
        g_g[ng].active = PC_CLAMP(s[g].active, 0, PC_MAX(0, s[g].n - 1));
        g_g[ng].collapsed = s[g].collapsed; g_g[ng].visible = s[g].visible;
        g_g[ng].height = PC_CLAMP(s[g].height, TAB_H + 20, 2000); g_g[ng].frect = s[g].frect;
    }
    for (k = P_COUNT_V1; k < P_COUNT; k++) if (!seen[k]) add_missing_panel(k);
    g_column_iconic = reg_get_int(sub, "ColumnIconic", 0);
    return 1;
}

void dock_load_workspace(void)
{
    if (!load_from("Workspace")) build_default();
    relayout();
    dock_refresh(R_ALL);
}

/* Window > Workspace > Save / Load Saved Workspace keep a named copy. */
void dock_store_named(void)
{
    SavedGroup s[MAXG];
    dock_save_workspace();
    if (reg_get_bin("Workspace", "Layout2", s, sizeof s)) {
        reg_set_bin("Workspace\\Saved", "Layout2", s, sizeof s);
        reg_set_int("Workspace\\Saved", "Groups", reg_get_int("Workspace", "Groups", 0));
        reg_set_int("Workspace\\Saved", "ColumnIconic", g_column_iconic);
    }
}

int dock_load_named(void)
{
    close_flyout();
    if (!load_from("Workspace\\Saved")) return -1;
    app_layout();
    relayout();
    dock_refresh(R_ALL);
    dock_save_workspace();
    return 0;
}

/* ---------------- public ---------------- */

/* CS6: an expanded icon-column panel collapses again when you work elsewhere. */
void dock_close_flyout(void) { close_flyout(); }

HWND dock_create(HWND parent)
{
    int p;
    w95_register_class("PCDock", dock_proc, CS_DBLCLKS, NULL);
    w95_register_class("PCPanelBody", body_proc, CS_DBLCLKS, NULL);
    g_dock = CreateWindowExA(WS_EX_CONTROLPARENT, "PCDock", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 100, 100, parent, NULL, w95_inst(), NULL);
    for (p = 0; p < P_COUNT; p++) {
        g_body[p] = CreateWindowExA(WS_EX_CONTROLPARENT, "PCPanelBody", panel_name(p), WS_CHILD | WS_CLIPCHILDREN, 0, 0, 10, 10, g_dock, NULL, w95_inst(), NULL);
        SetWindowLongA(g_body[p], GWL_USERDATA, p);
    }
    g_drag.group = -1;
    build_default();
    return g_dock;
}

void dock_refresh(int what)
{
    int p;
    static const int masks[P_COUNT] = {
        [P_NAVIGATOR] = R_NAV | R_CANVAS, [P_INFO] = R_INFO, [P_COLOR] = R_COLOR, [P_SWATCHES] = R_COLOR,
        [P_BRUSHES] = R_OPTIONS | R_TOOLS, [P_LAYERS] = R_LAYERS, [P_CHANNELS] = R_CHANNELS | R_LAYERS,
        [P_PATHS] = R_PATHS, [P_HISTORY] = R_HISTORY, [P_ACTIONS] = R_ALL, [P_ADJUSTMENTS] = R_LAYERS | R_TABS | R_TITLE
    };
    for (p = 0; p < P_COUNT; p++) {
        if (!g_body[p] || !IsWindowVisible(g_body[p])) continue;
        if (what == R_ALL || (what & masks[p])) {
            RECT r;
            GetClientRect(g_body[p], &r);
            panel_layout_controls(p, g_body[p], &r, 1);
            InvalidateRect(g_body[p], NULL, FALSE);
        }
    }
}

int dock_panel_visible(int p)
{
    int g = group_of(p);
    if (g < 0) return 0;
    if (g_g[g].host == HOST_ICONS || (g_column_iconic && g_g[g].host == HOST_COLUMN)) return g_flyout_group == g && g_g[g].panels[g_g[g].active] == p;
    return g_g[g].visible && g_g[g].panels[g_g[g].active] == p && !g_app.hide_all && !g_app.hide_panels;
}

void dock_show_panel(int p)
{
    int g = group_of(p), k;
    if (g < 0) return;
    for (k = 0; k < g_g[g].n; k++) if (g_g[g].panels[k] == p) g_g[g].active = k;
    g_g[g].visible = 1;
    g_g[g].collapsed = 0;
    if (g_app.hide_all || g_app.hide_panels) { g_app.hide_all = 0; g_app.hide_panels = 0; app_layout(); }
    if (g_g[g].host == HOST_ICONS || (g_column_iconic && g_g[g].host == HOST_COLUMN)) {
        RECT ir[32];
        int iidx[32], ni, i;
        icon_rects(ir, iidx, &ni);
        for (i = 0; i < ni; i++) if (iidx[i] / 100 == g && iidx[i] % 100 == g_g[g].active) open_flyout(g, g_g[g].active, ir[i].top);
    }
    relayout();
    dock_refresh(R_ALL);
}

void dock_toggle_panel(int p)
{
    int g = group_of(p);
    if (g < 0) return;
    if (dock_panel_visible(p)) {
        if (g == g_flyout_group) close_flyout();
        else if (g_g[g].host == HOST_FLOAT || g_g[g].host == HOST_COLUMN) { g_g[g].visible = 0; relayout(); }
    } else {
        dock_show_panel(p);
    }
    dock_save_workspace();
}
