/*
 * Menu bar and popup menus, drawn by hand in the Windows 95 style (the
 * system menus would turn into flat Segoe UI menus on Windows 10).
 *
 * Behaviour follows Windows 95: menus open on click, hovering another title
 * while a menu is open switches to it, items highlight navy with white
 * text, submenus open on hover, keyboard navigation with arrows, Enter,
 * Esc and underlined access keys, Alt / F10 activate the bar.
 */
#include "app.h"

/* ---------------- menu tree (CS6 menu set without 3D, PS 5.0 commands) ---------------- */

static const MenuItem g_menu[] = {
    { 0, 0, "&File" },
    { 1, CMD_NEW, NULL }, { 1, CMD_OPEN, NULL },
    { 1, 0, "Open &Recent" },
    { 2, CMD_RECENT_0, NULL }, { 2, CMD_RECENT_1, NULL }, { 2, CMD_RECENT_2, NULL }, { 2, CMD_RECENT_3, NULL },
    { 2, CMD_RECENT_4, NULL }, { 2, CMD_RECENT_5, NULL }, { 2, CMD_RECENT_6, NULL }, { 2, CMD_RECENT_7, NULL },
    { 1, 0, "-" },
    { 1, CMD_CLOSE, NULL }, { 1, CMD_CLOSE_ALL, NULL },
    { 1, CMD_SAVE, NULL }, { 1, CMD_SAVE_AS, NULL }, { 1, CMD_SAVE_COPY, NULL }, { 1, CMD_REVERT, NULL },
    { 1, 0, "-" },
    { 1, CMD_PAGE_SETUP, NULL }, { 1, CMD_PRINT, NULL },
    { 1, 0, "-" },
    { 1, CMD_EXIT, NULL },

    { 0, 0, "&Edit" },
    { 1, CMD_UNDO, NULL }, { 1, CMD_STEP_FORWARD, NULL }, { 1, CMD_STEP_BACKWARD, NULL },
    { 1, 0, "-" },
    { 1, CMD_CUT, NULL }, { 1, CMD_COPY, NULL }, { 1, CMD_COPY_MERGED, NULL },
    { 1, CMD_PASTE, NULL }, { 1, CMD_PASTE_INTO, NULL }, { 1, CMD_CLEAR, NULL },
    { 1, 0, "-" },
    { 1, CMD_FILL, NULL }, { 1, CMD_STROKE, NULL },
    { 1, 0, "-" },
    { 1, CMD_FREE_TRANSFORM, NULL },
    { 1, 0, "Transfor&m" },
    { 2, CMD_TRANSFORM_NUMERIC, NULL }, { 2, 0, "-" },
    { 2, CMD_LAYER_ROT180, NULL }, { 2, CMD_LAYER_ROT90CW, NULL }, { 2, CMD_LAYER_ROT90CCW, NULL },
    { 2, 0, "-" }, { 2, CMD_LAYER_FLIPH, NULL }, { 2, CMD_LAYER_FLIPV, NULL },
    { 1, 0, "-" },
    { 1, CMD_DEFINE_PATTERN, NULL },
    { 1, 0, "Pur&ge" },
    { 2, CMD_PURGE_UNDO, NULL }, { 2, CMD_PURGE_CLIPBOARD, NULL }, { 2, CMD_PURGE_HISTORIES, NULL }, { 2, CMD_PURGE_ALL, NULL },
    { 1, 0, "-" },
    { 1, 0, "Pre&ferences" },
    { 2, CMD_PREFERENCES, NULL },

    { 0, 0, "&Image" },
    { 1, 0, "&Mode" },
    { 2, CMD_MODE_GRAY, NULL }, { 2, CMD_MODE_RGB, NULL },
    { 1, 0, "&Adjustments" },
    { 2, CMD_LEVELS, NULL }, { 2, CMD_AUTO_LEVELS, NULL }, { 2, CMD_CURVES, NULL }, { 2, CMD_COLOR_BALANCE, NULL },
    { 2, CMD_BRIGHTNESS_CONTRAST, NULL }, { 2, 0, "-" },
    { 2, CMD_HUE_SAT, NULL }, { 2, CMD_DESATURATE, NULL }, { 2, CMD_CHANNEL_MIXER, NULL }, { 2, 0, "-" },
    { 2, CMD_INVERT, NULL }, { 2, CMD_EQUALIZE, NULL }, { 2, CMD_THRESHOLD, NULL }, { 2, CMD_POSTERIZE, NULL },
    { 1, 0, "-" },
    { 1, CMD_DUPLICATE, NULL },
    { 1, 0, "-" },
    { 1, CMD_IMAGE_SIZE, NULL }, { 1, CMD_CANVAS_SIZE, NULL },
    { 1, 0, "Rotate C&anvas" },
    { 2, CMD_ROT180, NULL }, { 2, CMD_ROT90CW, NULL }, { 2, CMD_ROT90CCW, NULL }, { 2, CMD_ROT_ARBITRARY, NULL },
    { 2, 0, "-" }, { 2, CMD_FLIP_CANVAS_H, NULL }, { 2, CMD_FLIP_CANVAS_V, NULL },
    { 1, CMD_CROP, NULL }, { 1, CMD_TRIM, NULL },
    { 1, 0, "-" },
    { 1, CMD_HISTOGRAM, NULL },

    { 0, 0, "&Layer" },
    { 1, 0, "Ne&w" },
    { 2, CMD_NEW_LAYER, NULL }, { 2, CMD_LAYER_FROM_BG, NULL }, { 2, 0, "-" },
    { 2, CMD_LAYER_VIA_COPY, NULL }, { 2, CMD_LAYER_VIA_CUT, NULL },
    { 1, CMD_DUPLICATE_LAYER, NULL }, { 1, CMD_DELETE_LAYER, NULL },
    { 1, CMD_LAYER_PROPS, NULL },
    { 1, 0, "Layer St&yle" },
    { 2, CMD_LAYER_STYLE, NULL }, { 2, 0, "-" },
    { 2, CMD_LAYER_STYLE_SHADOW, NULL }, { 2, CMD_LAYER_STYLE_INNER, NULL }, { 2, CMD_LAYER_STYLE_GLOW, NULL },
    { 2, CMD_LAYER_STYLE_BEVEL, NULL }, { 2, 0, "-" }, { 2, CMD_STYLE_CLEAR, NULL },
    { 1, 0, "-" },
    { 1, 0, "Layer &Mask" },
    { 2, CMD_MASK_REVEAL_ALL, NULL }, { 2, CMD_MASK_HIDE_ALL, NULL }, { 2, CMD_MASK_REVEAL_SEL, NULL },
    { 2, CMD_MASK_HIDE_SEL, NULL }, { 2, 0, "-" }, { 2, CMD_MASK_DELETE, NULL }, { 2, CMD_MASK_APPLY, NULL },
    { 2, 0, "-" }, { 2, CMD_MASK_TOGGLE, NULL },
    { 1, 0, "-" },
    { 1, CMD_CLIP_GROUP, NULL }, { 1, CMD_CLIP_UNGROUP, NULL },
    { 1, 0, "-" },
    { 1, 0, "Arra&nge" },
    { 2, CMD_BRING_FRONT, NULL }, { 2, CMD_BRING_FORWARD, NULL }, { 2, CMD_SEND_BACKWARD, NULL }, { 2, CMD_SEND_BACK, NULL },
    { 1, 0, "-" },
    { 1, CMD_MERGE_DOWN, NULL }, { 1, CMD_MERGE_VISIBLE, NULL }, { 1, CMD_FLATTEN, NULL },

    { 0, 0, "&Type" },
    { 1, CMD_TYPE_EDIT, NULL },
    { 1, 0, "&Anti-Alias" },
    { 2, CMD_TYPE_AA_NONE, NULL }, { 2, CMD_TYPE_AA_CRISP, NULL }, { 2, CMD_TYPE_AA_STRONG, NULL }, { 2, CMD_TYPE_AA_SMOOTH, NULL },
    { 1, 0, "&Orientation" },
    { 2, CMD_TYPE_HORIZONTAL, NULL }, { 2, CMD_TYPE_VERTICAL, NULL },
    { 1, 0, "-" },
    { 1, CMD_TYPE_RASTERIZE, NULL },

    { 0, 0, "&Select" },
    { 1, CMD_SELECT_ALL, NULL }, { 1, CMD_DESELECT, NULL }, { 1, CMD_RESELECT, NULL }, { 1, CMD_INVERSE, NULL },
    { 1, 0, "-" },
    { 1, CMD_COLOR_RANGE, NULL },
    { 1, 0, "-" },
    { 1, CMD_FEATHER, NULL },
    { 1, 0, "&Modify" },
    { 2, CMD_SEL_BORDER, NULL }, { 2, CMD_SEL_SMOOTH, NULL }, { 2, CMD_SEL_EXPAND, NULL }, { 2, CMD_SEL_CONTRACT, NULL },
    { 1, CMD_GROW, NULL }, { 1, CMD_SIMILAR, NULL },
    { 1, 0, "-" },
    { 1, CMD_QUICK_MASK, NULL },
    { 1, 0, "-" },
    { 1, CMD_LOAD_SELECTION, NULL }, { 1, CMD_SAVE_SELECTION, NULL },

    { 0, 0, "Filte&r" },
    { 1, CMD_LAST_FILTER, NULL },
    { 1, 0, "-" },
    { 1, 0, "&Blur" },
    { 2, CMD_F_BLUR, NULL }, { 2, CMD_F_BLUR_MORE, NULL }, { 2, CMD_F_GAUSSIAN, NULL }, { 2, CMD_F_MOTION, NULL }, { 2, CMD_F_RADIAL, NULL },
    { 1, 0, "&Distort" },
    { 2, CMD_F_PINCH, NULL }, { 2, CMD_F_RIPPLE, NULL }, { 2, CMD_F_SPHERIZE, NULL }, { 2, CMD_F_TWIRL, NULL },
    { 1, 0, "&Noise" },
    { 2, CMD_F_NOISE, NULL }, { 2, CMD_F_DESPECKLE, NULL }, { 2, CMD_F_DUST, NULL }, { 2, CMD_F_MEDIAN, NULL },
    { 1, 0, "&Pixelate" },
    { 2, CMD_F_MOSAIC, NULL },
    { 1, 0, "&Render" },
    { 2, CMD_F_CLOUDS, NULL }, { 2, CMD_F_DIFF_CLOUDS, NULL },
    { 1, 0, "&Sharpen" },
    { 2, CMD_F_SHARPEN, NULL }, { 2, CMD_F_SHARPEN_EDGES, NULL }, { 2, CMD_F_SHARPEN_MORE, NULL }, { 2, CMD_F_UNSHARP, NULL },
    { 1, 0, "St&ylize" },
    { 2, CMD_F_DIFFUSE, NULL }, { 2, CMD_F_EMBOSS, NULL }, { 2, CMD_F_FIND_EDGES, NULL }, { 2, CMD_F_SOLARIZE, NULL },
    { 2, CMD_F_TRACE, NULL }, { 2, CMD_F_WIND, NULL },
    { 1, 0, "&Other" },
    { 2, CMD_F_CUSTOM, NULL }, { 2, CMD_F_HIGH_PASS, NULL }, { 2, CMD_F_MAXIMUM, NULL }, { 2, CMD_F_MINIMUM, NULL }, { 2, CMD_F_OFFSET, NULL },

    { 0, 0, "&View" },
    { 1, CMD_ZOOM_IN, NULL }, { 1, CMD_ZOOM_OUT, NULL }, { 1, CMD_FIT_SCREEN, NULL }, { 1, CMD_ACTUAL_PIXELS, NULL }, { 1, CMD_PRINT_SIZE, NULL },
    { 1, 0, "-" },
    { 1, 0, "&Screen Mode" },
    { 2, CMD_SCREEN_STANDARD, NULL }, { 2, CMD_SCREEN_FULL_MENU, NULL }, { 2, CMD_SCREEN_FULL, NULL },
    { 1, 0, "-" },
    { 1, 0, "Sho&w" },
    { 2, CMD_SHOW_EDGES, NULL }, { 2, CMD_SHOW_GRID, NULL },
    { 1, CMD_SHOW_RULERS, NULL },

    { 0, 0, "&Window" },
    { 1, 0, "&Arrange" },
    { 2, CMD_CASCADE, NULL }, { 2, CMD_TILE_H, NULL }, { 2, CMD_TILE_V, NULL }, { 2, 0, "-" },
    { 2, CMD_FLOAT_WINDOW, NULL }, { 2, CMD_FLOAT_ALL, NULL }, { 2, CMD_CONSOLIDATE, NULL },
    { 1, 0, "&Workspace" },
    { 2, CMD_WS_ESSENTIALS, NULL }, { 2, CMD_WS_CLASSIC98, NULL }, { 2, 0, "-" },
    { 2, CMD_WS_RESET, NULL }, { 2, CMD_WS_SAVE, NULL }, { 2, CMD_WS_LOAD, NULL },
    { 1, 0, "-" },
    { 1, CMD_WIN_ACTIONS, NULL }, { 1, CMD_WIN_BRUSHES, NULL }, { 1, CMD_WIN_CHANNELS, NULL }, { 1, CMD_WIN_COLOR, NULL },
    { 1, CMD_WIN_HISTORY, NULL }, { 1, CMD_WIN_INFO, NULL }, { 1, CMD_WIN_LAYERS, NULL }, { 1, CMD_WIN_NAVIGATOR, NULL },
    { 1, CMD_WIN_PATHS, NULL }, { 1, CMD_WIN_SWATCHES, NULL },
    { 1, 0, "-" },
    { 1, CMD_WIN_OPTIONS, NULL }, { 1, CMD_WIN_TOOLS, NULL },
    { 1, 0, "-" },
    { 1, CMD_WIN_DOC_0, NULL }, { 1, CMD_WIN_DOC_1, NULL }, { 1, CMD_WIN_DOC_2, NULL }, { 1, CMD_WIN_DOC_3, NULL },
    { 1, CMD_WIN_DOC_4, NULL }, { 1, CMD_WIN_DOC_5, NULL }, { 1, CMD_WIN_DOC_6, NULL }, { 1, CMD_WIN_DOC_7, NULL },
    { 1, CMD_WIN_DOC_8, NULL },

    { 0, 0, "&Help" },
    { 1, CMD_HELP, NULL }, { 1, CMD_SHORTCUTS, NULL },
    { 1, 0, "-" },
    { 1, CMD_CHECK_UPDATES, NULL }, { 1, CMD_AUTO_UPDATES, NULL }, { 1, CMD_RELEASE_NOTES, NULL },
    { 1, 0, "-" },
    { 1, CMD_ABOUT, NULL },
};
#define NMENU ((int)(sizeof g_menu / sizeof g_menu[0]))

int cmd_label(int cmd, char *buf, int n);   /* cmds.c: 0 = hide item */

/* ---------------- shared item rendering ---------------- */

typedef struct Popup {
    HWND hwnd;
    const MenuItem *items;
    int nitems;
    int idx[80];          /* indices into items[] of the entries at this level */
    int sub[80];          /* first child index for submenu entries, -1 otherwise */
    int n;
    int sel;
    int x, y, w, h;
    int text_w, key_w;
} Popup;

static Popup g_pop[4];
static int g_npop;
static int g_bar_sel = -1;     /* highlighted menu bar title */
static int g_bar_open;         /* a menu from the bar is open */
static int g_tracking;
static HWND g_bar;

static int item_is_sep(const MenuItem *it) { return it->cmd == 0 && it->label && !strcmp(it->label, "-"); }

/* Ids at or above CMD_COUNT are private to one popup (panel menus): always
 * enabled, labelled by the item itself. */
static int item_label(const MenuItem *it, char *buf, int n)
{
    if (it->cmd >= CMD_COUNT) { pc_strlcpy(buf, it->label ? it->label : "", (size_t)n); return 1; }
    if (it->cmd && it->label) { pc_strlcpy(buf, it->label, (size_t)n); return 1; }
    if (it->cmd) return cmd_label(it->cmd, buf, n);
    pc_strlcpy(buf, it->label ? it->label : "", (size_t)n);
    return 1;
}

static void item_state(const MenuItem *it, int *en, int *ck)
{
    *en = 1; *ck = 0;
    if (it->cmd && it->cmd < CMD_COUNT) cmd_state(it->cmd, en, ck);
}

/* Collect the children of the entry at `parent` (or of top-level menu `parent`). */
static void popup_fill(Popup *p, const MenuItem *items, int nitems, int first, int depth)
{
    int i, lastsep = 1;
    p->items = items; p->nitems = nitems; p->n = 0; p->sel = -1;
    for (i = first; i < nitems && items[i].depth >= depth; i++) {
        char buf[128];
        if (items[i].depth != depth) continue;
        if (item_is_sep(&items[i])) {
            if (lastsep) continue;
            lastsep = 1;
        } else {
            if (!item_label(&items[i], buf, sizeof buf)) continue;
            lastsep = 0;
        }
        if (p->n >= 80) break;
        p->idx[p->n] = i;
        p->sub[p->n] = (items[i].cmd == 0 && !item_is_sep(&items[i]) && i + 1 < nitems && items[i + 1].depth == depth + 1) ? i + 1 : -1;
        p->n++;
    }
    /* Drop a trailing separator and empty submenus. */
    while (p->n && item_is_sep(&items[p->idx[p->n - 1]])) p->n--;
}

static void popup_measure(Popup *p)
{
    HDC dc = GetDC(NULL);
    int i;
    p->text_w = 40; p->key_w = 0; p->h = 4;
    for (i = 0; i < p->n; i++) {
        const MenuItem *it = &p->items[p->idx[i]];
        char buf[128];
        if (item_is_sep(it)) { p->h += 9; continue; }
        item_label(it, buf, sizeof buf);
        p->text_w = PC_MAX(p->text_w, w95_text_width(dc, W95_FONT_NORMAL, buf));
        if (it->cmd && it->cmd < CMD_COUNT && g_cmd_info[it->cmd].shortcut[0] && it->depth > 0)
            p->key_w = PC_MAX(p->key_w, w95_text_width(dc, W95_FONT_NORMAL, g_cmd_info[it->cmd].shortcut));
        p->h += W95_MENUITEM_H;
    }
    ReleaseDC(NULL, dc);
    p->w = 4 + 18 + p->text_w + (p->key_w ? 24 + p->key_w : 0) + 18 + 4;
}

static int popup_item_y(const Popup *p, int k)
{
    int i, y = 2;
    for (i = 0; i < k; i++) y += item_is_sep(&p->items[p->idx[i]]) ? 9 : W95_MENUITEM_H;
    return y;
}

static int popup_hit(const Popup *p, int x, int y)
{
    int i, yy = 2;
    if (x < 2 || x >= p->w - 2) return -1;
    for (i = 0; i < p->n; i++) {
        int h = item_is_sep(&p->items[p->idx[i]]) ? 9 : W95_MENUITEM_H;
        if (y >= yy && y < yy + h) return item_is_sep(&p->items[p->idx[i]]) ? -1 : i;
        yy += h;
    }
    return -1;
}

static void popup_paint(Popup *p, HDC target)
{
    RECT r;
    W95Buf b;
    HDC dc;
    int i, y = 2;
    SetRect(&r, 0, 0, p->w, p->h);
    dc = w95_buf_begin(&b, target, &r);
    w95_fill(dc, &r, W95_FACE);
    w95_raised(dc, &r);
    for (i = 0; i < p->n; i++) {
        const MenuItem *it = &p->items[p->idx[i]];
        char buf[128];
        int en, ck, sel = i == p->sel;
        RECT ir;
        COLORREF tc;
        if (item_is_sep(it)) { w95_etched_hline(dc, 3, p->w - 3, y + 4); y += 9; continue; }
        item_label(it, buf, sizeof buf);
        item_state(it, &en, &ck);
        SetRect(&ir, 3, y, p->w - 3, y + W95_MENUITEM_H);
        if (sel) w95_fill(dc, &ir, W95_SELBK);
        tc = sel ? W95_SELTEXT : W95_TEXT;
        if (ck) {
            int cx = 6, cy = y + 5, k;
            static const int ys[7] = { 2, 3, 4, 3, 2, 1, 0 };
            COLORREF cc = !en ? W95_SHADOW : tc;
            for (k = 0; k < 7; k++) { w95_pixel(dc, cx + k, cy + ys[k], cc); w95_pixel(dc, cx + k, cy + ys[k] + 1, cc); w95_pixel(dc, cx + k, cy + ys[k] + 2, cc); }
        }
        if (!en && !sel) w95_text_accel(dc, W95_FONT_NORMAL, buf, 21, y + 2, W95_TEXT, 1);
        else w95_text_accel(dc, W95_FONT_NORMAL, buf, 21, y + 2, en ? tc : W95_SHADOW, 0);
        if (it->cmd && it->cmd < CMD_COUNT && g_cmd_info[it->cmd].shortcut[0] && it->depth > 0) {
            RECT kr;
            SetRect(&kr, 21 + p->text_w + 24, y, p->w - 18, y + W95_MENUITEM_H);
            if (!en && !sel) w95_text_disabled(dc, W95_FONT_NORMAL, g_cmd_info[it->cmd].shortcut, &kr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            else w95_text(dc, W95_FONT_NORMAL, g_cmd_info[it->cmd].shortcut, &kr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, en ? tc : W95_SHADOW);
        }
        if (p->sub[i] >= 0) w95_draw_arrow(dc, p->w - 10, y + W95_MENUITEM_H / 2, 3, 4, tc);
        y += W95_MENUITEM_H;
    }
    w95_buf_end(&b);
}

static LRESULT CALLBACK popup_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        int i;
        for (i = 0; i < g_npop; i++) if (g_pop[i].hwnd == hwnd) popup_paint(&g_pop[i], dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void popup_open(int level, const MenuItem *items, int nitems, int first, int depth, int x, int y, int avoid_x0)
{
    Popup *p = &g_pop[level];
    RECT wa;
    static int registered;
    if (!registered) { w95_register_class("PCMenuPopup", popup_proc, CS_SAVEBITS, NULL); registered = 1; }
    popup_fill(p, items, nitems, first, depth);
    popup_measure(p);
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    {
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        if (x + p->w > sw) x = avoid_x0 >= 0 ? avoid_x0 - p->w : sw - p->w;
        if (y + p->h > sh) y = PC_MAX(0, sh - p->h);
        if (x < 0) x = 0;
    }
    p->x = x; p->y = y;
    p->hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "PCMenuPopup", "", WS_POPUP, x, y, p->w, p->h,
                              NULL, NULL, w95_inst(), NULL);
    g_npop = level + 1;
    if (p->hwnd) {
        ShowWindow(p->hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(p->hwnd);
    }
}

static void popup_close_from(int level)
{
    while (g_npop > level) {
        g_npop--;
        if (g_pop[g_npop].hwnd) DestroyWindow(g_pop[g_npop].hwnd);
        g_pop[g_npop].hwnd = NULL;
    }
}

static void popup_set_sel(int level, int sel)
{
    Popup *p = &g_pop[level];
    if (p->sel == sel) return;
    p->sel = sel;
    InvalidateRect(p->hwnd, NULL, FALSE);
    UpdateWindow(p->hwnd);
}

static void open_submenu(int level, int k)
{
    Popup *p = &g_pop[level];
    int depth;
    if (k < 0 || p->sub[k] < 0) return;
    if (g_npop > level + 1 && g_pop[level + 1].items == p->items && g_pop[level + 1].n &&
        g_pop[level + 1].idx[0] >= p->sub[k] && g_pop[level + 1].idx[0] <= p->sub[k] + 1) return;
    popup_close_from(level + 1);
    if (level + 1 >= 4) return;
    depth = p->items[p->sub[k]].depth;
    popup_open(level + 1, p->items, p->nitems, p->sub[k], depth,
               p->x + p->w - 3, p->y + popup_item_y(p, k) - 2, p->x);
}

/* ---------------- menu bar ---------------- */

static int g_title_x[16], g_title_w[16], g_ntitles;

static int title_index(int k)
{
    int i, n = 0;
    for (i = 0; i < NMENU; i++) if (g_menu[i].depth == 0) { if (n == k) return i; n++; }
    return -1;
}

static void bar_measure(HDC dc)
{
    int i, x = 3;
    g_ntitles = 0;
    for (i = 0; i < NMENU && g_ntitles < 16; i++) {
        char plain[64];
        int k, n = 0;
        if (g_menu[i].depth) continue;
        for (k = 0; g_menu[i].label[k] && n < 63; k++) if (g_menu[i].label[k] != '&') plain[n++] = g_menu[i].label[k];
        plain[n] = 0;
        g_title_x[g_ntitles] = x;
        g_title_w[g_ntitles] = w95_text_width(dc, W95_FONT_NORMAL, plain) + 14;
        x += g_title_w[g_ntitles];
        g_ntitles++;
    }
}

static int bar_hit(int x, int y)
{
    int i;
    if (y < 0 || y >= W95_MENU_H) return -1;
    for (i = 0; i < g_ntitles; i++) if (x >= g_title_x[i] && x < g_title_x[i] + g_title_w[i]) return i;
    return -1;
}

static void bar_paint(HWND hwnd, HDC target)
{
    RECT r;
    W95Buf b;
    HDC dc;
    int i;
    GetClientRect(hwnd, &r);
    dc = w95_buf_begin(&b, target, &r);
    w95_fill(dc, &r, W95_FACE);
    bar_measure(dc);
    for (i = 0; i < g_ntitles; i++) {
        RECT t;
        int ti = title_index(i);
        SetRect(&t, g_title_x[i], 1, g_title_x[i] + g_title_w[i], W95_MENU_H - 1);
        if (i == g_bar_sel) {
            /* Win95: an open menu's title is drawn sunken; a keyboard-selected title raised. */
            if (g_bar_open) w95_thin_sunken(dc, &t); else w95_thin_raised(dc, &t);
        }
        w95_text_accel(dc, W95_FONT_NORMAL, g_menu[ti].label, t.left + 7 + (i == g_bar_sel && g_bar_open), t.top + 2 + (i == g_bar_sel && g_bar_open), W95_TEXT, 0);
    }
    w95_buf_end(&b);
}

static void bar_set(int sel, int open)
{
    if (sel == g_bar_sel && open == g_bar_open) return;
    g_bar_sel = sel; g_bar_open = open;
    if (g_bar) { InvalidateRect(g_bar, NULL, FALSE); UpdateWindow(g_bar); }
}

static void open_bar_menu(int k)
{
    POINT pt;
    popup_close_from(0);
    pt.x = g_title_x[k]; pt.y = W95_MENU_H - 1;
    ClientToScreen(g_bar, &pt);
    bar_set(k, 1);
    popup_open(0, g_menu, NMENU, title_index(k) + 1, 1, pt.x, pt.y, -1);
}

/* ---------------- tracking loop ---------------- */

static int find_popup(POINT sp, int *item)
{
    int l;
    for (l = g_npop - 1; l >= 0; l--) {
        Popup *p = &g_pop[l];
        if (sp.x >= p->x && sp.x < p->x + p->w && sp.y >= p->y && sp.y < p->y + p->h) {
            *item = popup_hit(p, sp.x - p->x, sp.y - p->y);
            return l;
        }
    }
    return -1;
}

static int activate_item(int level, int k)
{
    Popup *p = &g_pop[level];
    const MenuItem *it;
    int en, ck;
    if (k < 0 || k >= p->n) return 0;
    it = &p->items[p->idx[k]];
    if (p->sub[k] >= 0) { open_submenu(level, k); if (g_npop > level + 1) popup_set_sel(level + 1, 0); return 0; }
    item_state(it, &en, &ck);
    if (!en) return 0;
    return it->cmd ? it->cmd : 0;
}

static int next_item(Popup *p, int from, int dir)
{
    int i, k = from;
    for (i = 0; i < p->n; i++) {
        k = (k + dir + p->n) % p->n;
        if (!item_is_sep(&p->items[p->idx[k]])) return k;
    }
    return from;
}

static int track(HWND owner, int from_bar, int button_down)
{
    MSG m;
    int result = 0, done = 0, moved = 0;
    HWND cap = g_npop ? g_pop[0].hwnd : NULL;
    if (!cap) return 0;
    g_tracking = 1;
    SetCapture(cap);
    while (!done && GetMessageA(&m, NULL, 0, 0)) {
        if (m.message == WM_MOUSEMOVE || m.message == WM_LBUTTONDOWN || m.message == WM_LBUTTONUP ||
            m.message == WM_RBUTTONDOWN || m.message == WM_RBUTTONUP) {
            POINT sp;
            int item = -1, level;
            sp.x = (short)LOWORD(m.lParam); sp.y = (short)HIWORD(m.lParam);
            ClientToScreen(m.hwnd, &sp);
            level = find_popup(sp, &item);
            if (m.message == WM_MOUSEMOVE) {
                moved = 1;
                if (level >= 0) {
                    popup_set_sel(level, item);
                    if (item >= 0 && g_pop[level].sub[item] >= 0) open_submenu(level, item);
                    else if (g_npop > level + 1 && item >= 0) popup_close_from(level + 1);
                } else if (from_bar) {
                    POINT bp = sp;
                    int t;
                    ScreenToClient(g_bar, &bp);
                    t = bar_hit(bp.x, bp.y);
                    if (t >= 0 && t != g_bar_sel) { open_bar_menu(t); cap = g_pop[0].hwnd; SetCapture(cap); }
                    else if (g_npop) popup_set_sel(g_npop - 1, g_pop[g_npop - 1].sub[PC_MAX(0, g_pop[g_npop - 1].sel)] >= 0 ? g_pop[g_npop - 1].sel : -1);
                }
            } else if (m.message == WM_LBUTTONDOWN || m.message == WM_RBUTTONDOWN) {
                if (level < 0) {
                    POINT bp = sp;
                    int t = -1;
                    if (from_bar) { ScreenToClient(g_bar, &bp); t = bar_hit(bp.x, bp.y); }
                    if (t >= 0 && t != g_bar_sel) { open_bar_menu(t); cap = g_pop[0].hwnd; SetCapture(cap); }
                    else done = 1;
                }
            } else if (m.message == WM_LBUTTONUP || m.message == WM_RBUTTONUP) {
                if (level >= 0 && item >= 0) {
                    result = activate_item(level, item);
                    if (result) done = 1;
                } else if (level < 0 && (moved || !button_down)) {
                    /* Release over the bar title that opened the menu keeps it open. */
                    POINT bp = sp;
                    int t = -1;
                    if (from_bar) { ScreenToClient(g_bar, &bp); t = bar_hit(bp.x, bp.y); }
                    if (t < 0 && moved && button_down) done = 1;
                }
                button_down = 0;
            }
            continue;
        }
        if (m.message == WM_KEYDOWN || m.message == WM_SYSKEYDOWN) {
            int l = g_npop - 1;
            Popup *p = &g_pop[l];
            switch (m.wParam) {
            case VK_ESCAPE:
                if (l > 0) popup_close_from(l);
                else done = 1;
                break;
            case VK_UP: popup_set_sel(l, next_item(p, p->sel < 0 ? 0 : p->sel, -1)); break;
            case VK_DOWN: popup_set_sel(l, next_item(p, p->sel < 0 ? p->n - 1 : p->sel, 1)); break;
            case VK_RIGHT:
                if (p->sel >= 0 && p->sub[p->sel] >= 0) { open_submenu(l, p->sel); if (g_npop > l + 1) popup_set_sel(l + 1, 0); }
                else if (from_bar) { open_bar_menu((g_bar_sel + 1) % g_ntitles); cap = g_pop[0].hwnd; SetCapture(cap); popup_set_sel(0, next_item(&g_pop[0], -1, 1)); }
                break;
            case VK_LEFT:
                if (l > 0) popup_close_from(l);
                else if (from_bar) { open_bar_menu((g_bar_sel + g_ntitles - 1) % g_ntitles); cap = g_pop[0].hwnd; SetCapture(cap); popup_set_sel(0, next_item(&g_pop[0], -1, 1)); }
                break;
            case VK_RETURN:
                result = activate_item(l, p->sel);
                if (result) done = 1;
                break;
            case VK_MENU: case VK_F10:
                done = 1;
                break;
            default: {
                /* Access key: underlined letter. */
                int k, ch = (int)MapVirtualKeyA((UINT)m.wParam, 2);
                ch = pc_tolower(ch & 0xFF);
                for (k = 0; k < p->n && ch; k++) {
                    char buf[128], *a;
                    if (item_is_sep(&p->items[p->idx[k]])) continue;
                    item_label(&p->items[p->idx[k]], buf, sizeof buf);
                    a = strchr(buf, '&');
                    if (a && pc_tolower((unsigned char)a[1]) == ch) {
                        popup_set_sel(l, k);
                        result = activate_item(l, k);
                        if (result) done = 1;
                        break;
                    }
                }
            }
            }
            continue;
        }
        if (m.message == WM_CHAR || m.message == WM_SYSCHAR || m.message == WM_KEYUP || m.message == WM_SYSKEYUP) continue;
        if (m.message == WM_CANCELMODE) { done = 1; continue; }
        TranslateMessage(&m);
        DispatchMessageA(&m);
        if (GetCapture() != cap && g_npop) {
            /* Something stole the capture (e.g. app switch): close. */
            if (GetActiveWindow() != owner) done = 1;
            else SetCapture(cap);
        }
    }
    ReleaseCapture();
    popup_close_from(0);
    bar_set(-1, 0);
    g_tracking = 0;
    (void)owner;
    return result;
}

/* ---------------- public API ---------------- */

static LRESULT CALLBACK bar_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        bar_paint(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: {
        int t, cmd;
        HDC dc = GetDC(hwnd);
        bar_measure(dc);
        ReleaseDC(hwnd, dc);
        t = bar_hit((short)LOWORD(lp), (short)HIWORD(lp));
        if (t < 0 || g_tracking) return 0;
        open_bar_menu(t);
        cmd = track(GetParent(hwnd), 1, 1);
        if (cmd) cmd_run(cmd);
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

HWND menubar_create(HWND parent)
{
    w95_register_class("PCMenuBar", bar_proc, 0, NULL);
    g_bar = CreateWindowExA(0, "PCMenuBar", "", WS_CHILD | WS_VISIBLE, 0, 0, 100, W95_MENU_H, parent, NULL, w95_inst(), NULL);
    return g_bar;
}

int menubar_height(void) { return W95_MENU_H; }

int menu_popup(int menu_index, int x, int y, HWND owner)
{
    int ti = title_index(menu_index);
    if (ti < 0 || g_tracking) return 0;
    popup_open(0, g_menu, NMENU, ti + 1, 1, x, y, -1);
    return track(owner, 0, 0);
}

int menu_popup_items(const MenuItem *items, int n, int x, int y, HWND owner)
{
    if (g_tracking || n <= 0) return 0;
    popup_open(0, items, n, 0, 1, x, y, -1);
    if (!g_pop[0].n) { popup_close_from(0); return 0; }
    return track(owner, 0, GetAsyncKeyState(VK_LBUTTON) < 0 || GetAsyncKeyState(VK_RBUTTON) < 0);
}

/* Alt+letter opens the matching menu; Alt or F10 alone selects File. */
int menubar_key(HWND owner, int vk, int alt)
{
    int k, cmd;
    HDC dc;
    if (g_tracking || !g_bar || !IsWindowVisible(g_bar)) return 0;
    dc = GetDC(g_bar);
    bar_measure(dc);
    ReleaseDC(g_bar, dc);
    if (vk == VK_F10 || (vk == VK_MENU && !alt)) k = 0;
    else {
        int ch = pc_tolower((int)MapVirtualKeyA((UINT)vk, 2) & 0xFF);
        for (k = 0; k < g_ntitles; k++) {
            const char *a = strchr(g_menu[title_index(k)].label, '&');
            if (a && pc_tolower((unsigned char)a[1]) == ch) break;
        }
        if (k >= g_ntitles || !ch) return 0;
    }
    open_bar_menu(k);
    popup_set_sel(0, next_item(&g_pop[0], -1, 1));
    cmd = track(owner, 1, 0);
    if (cmd) cmd_run(cmd);
    return 1;
}

void menubar_cancel(void)
{
    if (g_tracking && g_npop) PostMessageA(g_pop[0].hwnd, WM_CANCELMODE, 0, 0);
}
