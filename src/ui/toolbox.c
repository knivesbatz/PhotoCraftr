/*
 * Tools panel (left).  CS6 placement and grouping, Photoshop 5.0 button
 * look: 1998 pixel icons on raised square buttons, small flyout triangles,
 * flyouts that pop out to the right as a row of buttons, the colour
 * squares, Quick Mask and screen mode buttons at the bottom.  Switches to
 * two columns automatically when the screen is too short (800x600).
 */
#include "app.h"

const ToolInfo g_tool_info[T_COUNT] = {
    [T_MOVE] = { "Move Tool", PC_ICON_TOOL_MOVE, 'V', 0, PC_CURSOR_MOVE },
    [T_MARQUEE_RECT] = { "Rectangular Marquee Tool", PC_ICON_TOOL_MARQUEE_RECT, 'M', 1, PC_CURSOR_MARQUEE },
    [T_MARQUEE_ELLIPSE] = { "Elliptical Marquee Tool", PC_ICON_TOOL_MARQUEE_ELLIPSE, 'M', 1, PC_CURSOR_MARQUEE },
    [T_MARQUEE_ROW] = { "Single Row Marquee Tool", PC_ICON_TOOL_MARQUEE_ROW, 0, 1, PC_CURSOR_MARQUEE },
    [T_MARQUEE_COL] = { "Single Column Marquee Tool", PC_ICON_TOOL_MARQUEE_COL, 0, 1, PC_CURSOR_MARQUEE },
    [T_LASSO] = { "Lasso Tool", PC_ICON_TOOL_LASSO, 'L', 2, PC_CURSOR_LASSO },
    [T_LASSO_POLY] = { "Polygonal Lasso Tool", PC_ICON_TOOL_LASSO_POLY, 'L', 2, PC_CURSOR_LASSO_POLY },
    [T_LASSO_MAGNETIC] = { "Magnetic Lasso Tool", PC_ICON_TOOL_LASSO_MAGNETIC, 'L', 2, PC_CURSOR_LASSO },
    [T_WAND] = { "Magic Wand Tool", PC_ICON_TOOL_WAND, 'W', 3, PC_CURSOR_WAND },
    [T_CROP] = { "Crop Tool", PC_ICON_TOOL_CROP, 'C', 4, PC_CURSOR_CROP },
    [T_EYEDROPPER] = { "Eyedropper Tool", PC_ICON_TOOL_EYEDROPPER, 'I', 5, PC_CURSOR_EYEDROPPER },
    [T_MEASURE] = { "Measure Tool", PC_ICON_TOOL_MEASURE, 'I', 5, PC_CURSOR_MEASURE },
    [T_BRUSH] = { "Paintbrush Tool", PC_ICON_TOOL_BRUSH, 'B', 6, PC_CURSOR_BRUSH },
    [T_AIRBRUSH] = { "Airbrush Tool", PC_ICON_TOOL_AIRBRUSH, 'B', 6, PC_CURSOR_AIRBRUSH },
    [T_PENCIL] = { "Pencil Tool", PC_ICON_TOOL_PENCIL, 'B', 6, PC_CURSOR_PENCIL },
    [T_STAMP] = { "Rubber Stamp Tool", PC_ICON_TOOL_STAMP, 'S', 7, PC_CURSOR_STAMP },
    [T_PATTERN_STAMP] = { "Pattern Stamp Tool", PC_ICON_TOOL_PATTERN_STAMP, 'S', 7, PC_CURSOR_STAMP },
    [T_HISTORY_BRUSH] = { "History Brush Tool", PC_ICON_TOOL_HISTORY_BRUSH, 'Y', 8, PC_CURSOR_BRUSH },
    [T_ERASER] = { "Eraser Tool", PC_ICON_TOOL_ERASER, 'E', 9, PC_CURSOR_ERASER },
    [T_GRADIENT] = { "Gradient Tool", PC_ICON_TOOL_GRADIENT, 'G', 10, PC_CURSOR_GRADIENT },
    [T_BUCKET] = { "Paint Bucket Tool", PC_ICON_TOOL_BUCKET, 'G', 10, PC_CURSOR_BUCKET },
    [T_BLUR] = { "Blur Tool", PC_ICON_TOOL_BLUR, 'R', 11, PC_CURSOR_BLUR },
    [T_SHARPEN] = { "Sharpen Tool", PC_ICON_TOOL_SHARPEN, 'R', 11, PC_CURSOR_SHARPEN },
    [T_SMUDGE] = { "Smudge Tool", PC_ICON_TOOL_SMUDGE, 'R', 11, PC_CURSOR_SMUDGE },
    [T_DODGE] = { "Dodge Tool", PC_ICON_TOOL_DODGE, 'O', 12, PC_CURSOR_DODGE },
    [T_BURN] = { "Burn Tool", PC_ICON_TOOL_BURN, 'O', 12, PC_CURSOR_BURN },
    [T_SPONGE] = { "Sponge Tool", PC_ICON_TOOL_SPONGE, 'O', 12, PC_CURSOR_SPONGE },
    [T_PEN] = { "Pen Tool", PC_ICON_TOOL_PEN, 'P', 13, PC_CURSOR_PEN },
    [T_PEN_FREEFORM] = { "Freeform Pen Tool", PC_ICON_TOOL_PEN_FREEFORM, 'P', 13, PC_CURSOR_PEN },
    [T_PEN_MAGNETIC] = { "Magnetic Pen Tool", PC_ICON_TOOL_PEN_MAGNETIC, 'P', 13, PC_CURSOR_PEN },
    [T_PEN_ADD] = { "Add Anchor Point Tool", PC_ICON_TOOL_PEN_ADD, 0, 13, PC_CURSOR_PEN_ADD },
    [T_PEN_DELETE] = { "Delete Anchor Point Tool", PC_ICON_TOOL_PEN_DELETE, 0, 13, PC_CURSOR_PEN_DELETE },
    [T_PEN_CONVERT] = { "Convert Point Tool", PC_ICON_TOOL_PEN_CONVERT, 0, 13, PC_CURSOR_DIRECT_SELECT },
    [T_PATH_SELECT] = { "Path Component Selection Tool", PC_ICON_TOOL_PATH_SELECT, 'A', 14, PC_CURSOR_DIRECT_SELECT },
    [T_DIRECT_SELECT] = { "Direct Selection Tool", PC_ICON_TOOL_DIRECT_SELECT, 'A', 14, PC_CURSOR_DIRECT_SELECT },
    [T_TYPE] = { "Type Tool", PC_ICON_TOOL_TYPE, 'T', 15, PC_CURSOR_IBEAM },
    [T_TYPE_VERTICAL] = { "Vertical Type Tool", PC_ICON_TOOL_TYPE_VERTICAL, 'T', 15, PC_CURSOR_IBEAM_VERTICAL },
    [T_TYPE_MASK] = { "Type Mask Tool", PC_ICON_TOOL_TYPE_MASK, 'T', 15, PC_CURSOR_IBEAM },
    [T_TYPE_MASK_VERTICAL] = { "Vertical Type Mask Tool", PC_ICON_TOOL_TYPE_MASK_VERTICAL, 'T', 15, PC_CURSOR_IBEAM_VERTICAL },
    [T_SHAPE_RECT] = { "Rectangle Tool", PC_ICON_TOOL_SHAPE_RECT, 'U', 16, PC_CURSOR_CROSSHAIR },
    [T_SHAPE_ELLIPSE] = { "Ellipse Tool", PC_ICON_TOOL_SHAPE_ELLIPSE, 'U', 16, PC_CURSOR_CROSSHAIR },
    [T_SHAPE_LINE] = { "Line Tool", PC_ICON_TOOL_SHAPE_LINE, 'U', 16, PC_CURSOR_CROSSHAIR },
    [T_HAND] = { "Hand Tool", PC_ICON_TOOL_HAND, 'H', 17, PC_CURSOR_HAND },
    [T_ZOOM] = { "Zoom Tool", PC_ICON_TOOL_ZOOM, 'Z', 18, PC_CURSOR_ZOOM_IN },
};

#define NSLOTS 19
#define BTN_W 25
#define BTN_H 21
/* Separators after these slots, like the CS6 tool groups. */
static const int g_sep_after[] = { 4, 12, 16 };

static HWND g_tb;
static int g_two_col;
static int g_hot = -1, g_pressed = -1;
static int g_tip_slot = -1;
static int g_hold_slot = -1;

int toolbox_width(void) { return g_two_col ? 2 * BTN_W + 10 : BTN_W + 9; }

static int is_sep_after(int slot)
{
    unsigned i;
    for (i = 0; i < sizeof g_sep_after / sizeof g_sep_after[0]; i++) if (g_sep_after[i] == slot) return 1;
    return 0;
}

static int slot_has_group(int slot)
{
    int t, n = 0;
    for (t = 0; t < T_COUNT; t++) if (g_tool_info[t].group == slot) n++;
    return n > 1;
}

/* Rect of slot `s` in client coordinates. */
static void slot_rect(int s, RECT *r)
{
    int top = 12, i, row = 0, col = 0, y;
    if (g_two_col) {
        row = s / 2; col = s % 2;
        y = top + row * BTN_H;
        /* separators between groups take 4 px in two-column mode too */
        for (i = 0; i < (int)(sizeof g_sep_after / sizeof g_sep_after[0]); i++) if (g_sep_after[i] / 2 < row) y += 4;
        SetRect(r, 4 + col * BTN_W, y, 4 + col * BTN_W + BTN_W, y + BTN_H);
    } else {
        y = top;
        for (i = 0; i < s; i++) { y += BTN_H; if (is_sep_after(i)) y += 5; }
        SetRect(r, 4, y, 4 + BTN_W, y + BTN_H);
    }
}

static int bottom_of_slots(void)
{
    RECT r;
    slot_rect(NSLOTS - 1, &r);
    if (g_two_col && (NSLOTS & 1)) return r.bottom;
    return r.bottom;
}

/* Lower widgets. */
static void colors_rect(RECT *r) { int y = bottom_of_slots() + 6; SetRect(r, 3, y, toolbox_width() - 3, y + 36); }
static void qm_rect(int which, RECT *r)
{
    RECT c;
    int w = g_two_col ? BTN_W : (BTN_W + 1) / 2 + 1;
    colors_rect(&c);
    if (g_two_col) SetRect(r, 4 + which * BTN_W, c.bottom + 4, 4 + which * BTN_W + BTN_W, c.bottom + 4 + 20);
    else SetRect(r, 3 + which * w, c.bottom + 4, 3 + which * w + w, c.bottom + 4 + 18);
}
static void screen_rect(int which, RECT *r)
{
    RECT q;
    qm_rect(0, &q);
    if (g_two_col) {
        int w = (2 * BTN_W) / 3;
        SetRect(r, 4 + which * w, q.bottom + 3, 4 + which * w + w, q.bottom + 3 + 18);
    } else {
        SetRect(r, 4, q.bottom + 3 + which * 19, 4 + BTN_W, q.bottom + 3 + which * 19 + 18);
    }
}

static void fg_rect(RECT *r) { RECT c; colors_rect(&c); SetRect(r, c.left + (g_two_col ? 10 : 2), c.top + 2, c.left + (g_two_col ? 10 : 2) + 16, c.top + 18); }
static void bg_rect(RECT *r) { RECT f; fg_rect(&f); SetRect(r, f.left + 9, f.top + 9, f.left + 9 + 16, f.top + 9 + 16); }
static void default_rect(RECT *r) { RECT f; fg_rect(&f); SetRect(r, f.left, f.bottom + 2, f.left + 10, f.bottom + 12); }
static void swap_rect(RECT *r) { RECT f; fg_rect(&f); SetRect(r, f.right + 1, f.top - 1, f.right + 11, f.top + 9); }

static void draw_swatch(HDC dc, const RECT *r, u32 c)
{
    RECT q = *r;
    w95_rect(dc, q.left, q.top, q.right, q.bottom, W95_DKSHADOW);
    InflateRect(&q, -1, -1);
    w95_rect(dc, q.left, q.top, q.right, q.bottom, W95_HILIGHT);
    InflateRect(&q, -1, -1);
    w95_fill(dc, &q, RGB(pc_r(c), pc_g(c), pc_b(c)));
}

static void tb_paint(HWND hwnd, HDC target)
{
    RECT r, q;
    W95Buf b;
    HDC dc;
    int s;
    GetClientRect(hwnd, &r);
    dc = w95_buf_begin(&b, target, &r);
    w95_fill(dc, &r, W95_FACE);
    w95_raised(dc, &r);
    /* Gripper strip with the CS6 one/two column chevron. */
    SetRect(&q, 2, 2, r.right - 2, 9);
    w95_hline(dc, 4, r.right - 12, 4, W95_HILIGHT); w95_hline(dc, 4, r.right - 12, 5, W95_SHADOW);
    w95_hline(dc, 4, r.right - 12, 7, W95_HILIGHT); w95_hline(dc, 4, r.right - 12, 8, W95_SHADOW);
    w95_icon(dc, g_two_col ? PC_ICON_CHEVRON_LEFT : PC_ICON_CHEVRON_RIGHT, r.right - 10, 3);
    for (s = 0; s < NSLOTS; s++) {
        int tool = g_app.group_tool[s];
        int state = 0;
        RECT br;
        if (tool < 0) continue;
        slot_rect(s, &br);
        if (g_tool_info[g_app.tool].group == s) state = 3;
        else if (g_pressed == s) state = 2;
        else if (g_hot == s) state = 1;
        w95_draw_icon_button(dc, &br, g_tool_info[tool].icon, state == 3 ? 2 : state, 0);
        if (state == 3) {
            /* Selected tool: PS5 shows it pressed in with a white-dither face. */
            w95_draw_icon_button(dc, &br, g_tool_info[tool].icon, 3, 0);
        }
        if (slot_has_group(s)) w95_icon(dc, PC_ICON_FLYOUT, br.right - 5, br.bottom - 5);
        if (!g_two_col && is_sep_after(s)) w95_etched_hline(dc, 4, r.right - 4, br.bottom + 2);
    }
    /* Colours. */
    {
        RECT fr, bgr, dr, sr;
        fg_rect(&fr); bg_rect(&bgr); default_rect(&dr); swap_rect(&sr);
        draw_swatch(dc, &bgr, g_app.bg);
        draw_swatch(dc, &fr, g_app.fg);
        w95_icon(dc, PC_ICON_DEFAULT_COLORS, dr.left, dr.top);
        w95_icon(dc, PC_ICON_SWAP_COLORS, sr.left, sr.top);
    }
    /* Quick Mask: standard / quick mask buttons. */
    {
        RECT a, c;
        qm_rect(0, &a); qm_rect(1, &c);
        w95_draw_icon_button(dc, &a, PC_ICON_STANDARD_MODE, g_app.quickmask ? 0 : 3, 0);
        w95_draw_icon_button(dc, &c, PC_ICON_QUICKMASK_MODE, g_app.quickmask ? 3 : 0, 0);
        if (!g_app.quickmask) w95_thin_raised(dc, &c); else w95_thin_raised(dc, &a);
    }
    /* Screen modes (three buttons as in Photoshop 5.0). */
    {
        static const int icons[3] = { PC_ICON_SCREEN_STANDARD, PC_ICON_SCREEN_FULL_MENU, PC_ICON_SCREEN_FULL };
        int k;
        for (k = 0; k < 3; k++) {
            RECT a;
            screen_rect(k, &a);
            if (a.bottom > r.bottom - 2) break;
            w95_draw_icon_button(dc, &a, icons[k], g_app.screen_mode == k ? 3 : 0, 0);
            if (g_app.screen_mode != k) w95_thin_raised(dc, &a);
        }
    }
    w95_buf_end(&b);
}

static int hit_slot(int x, int y)
{
    int s;
    POINT p;
    p.x = x; p.y = y;
    for (s = 0; s < NSLOTS; s++) {
        RECT r;
        slot_rect(s, &r);
        if (PtInRect(&r, p)) return s;
    }
    return -1;
}

/* PS5 flyout: a row of tool buttons popping out to the right. */
static int flyout(int slot)
{
    int tools[8], n = 0, t, sel = -1;
    RECT sr;
    POINT p;
    for (t = 0; t < T_COUNT && n < 8; t++) if (g_tool_info[t].group == slot) tools[n++] = t;
    if (n < 2) return -1;
    slot_rect(slot, &sr);
    p.x = sr.right; p.y = sr.top;
    ClientToScreen(g_tb, &p);
    {
        /* Draw the flyout as its own little popup window of buttons. */
        HWND w;
        MSG m;
        int hot = -1, done = 0, i;
        RECT wr;
        HDC dc;
        static int reg;
        if (!reg) { w95_register_class("PCFlyout", DefWindowProcA, CS_SAVEBITS, NULL); reg = 1; }
        w = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "PCFlyout", "", WS_POPUP, p.x, p.y - 2, n * BTN_W + 4, BTN_H + 4, g_tb, NULL, w95_inst(), NULL);
        if (!w) return -1;
        ShowWindow(w, SW_SHOWNOACTIVATE);
        SetCapture(w);
        while (!done) {
            int nh;
            POINT cp;
            dc = GetDC(w);
            GetClientRect(w, &wr);
            w95_fill(dc, &wr, W95_FACE);
            w95_raised(dc, &wr);
            for (i = 0; i < n; i++) {
                RECT b;
                SetRect(&b, 2 + i * BTN_W, 2, 2 + i * BTN_W + BTN_W, 2 + BTN_H);
                w95_draw_icon_button(dc, &b, g_tool_info[tools[i]].icon, i == hot ? 2 : (tools[i] == g_app.group_tool[slot] ? 1 : 0), 0);
            }
            ReleaseDC(w, dc);
            if (hot >= 0) {
                char tip[96];
                pc_snprintf(tip, sizeof tip, g_tool_info[tools[hot]].key ? "%s (%c)" : "%s", g_tool_info[tools[hot]].name, g_tool_info[tools[hot]].key);
                w95_tooltip_show(w, p.x + hot * BTN_W, p.y + BTN_H + 4, tip);
            }
            if (!GetMessageA(&m, NULL, 0, 0)) break;
            GetCursorPos(&cp);
            ScreenToClient(w, &cp);
            nh = (cp.y >= 0 && cp.y < BTN_H + 4 && cp.x >= 2 && cp.x < 2 + n * BTN_W) ? (cp.x - 2) / BTN_W : -1;
            if (m.message == WM_MOUSEMOVE) hot = nh;
            else if (m.message == WM_LBUTTONUP || m.message == WM_LBUTTONDOWN) { if (nh >= 0) { sel = tools[nh]; done = 1; } else if (m.message == WM_LBUTTONDOWN) done = 1; }
            else if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE) done = 1;
            else { TranslateMessage(&m); DispatchMessageA(&m); }
        }
        ReleaseCapture();
        DestroyWindow(w);
        w95_tooltip_hide();
    }
    return sel;
}

static LRESULT CALLBACK tb_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        tb_paint(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: {
        /* Two columns when one column does not fit (CS6's chevron toggles it too). */
        int h = HIWORD(lp), want, saved = g_two_col;
        RECT r;
        g_two_col = 0;
        screen_rect(2, &r);
        want = r.bottom + 4 > h ? 1 : reg_get_int("Settings", "ToolsTwoColumns", 0);
        g_two_col = saved;
        if (want != g_two_col) { g_two_col = want; app_layout(); }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int s = hit_slot(x, y);
        if (s != g_hot) {
            g_hot = s;
            InvalidateRect(hwnd, NULL, FALSE);
            w95_tooltip_hide();
            g_tip_slot = -1;
            KillTimer(hwnd, 2);
            if (s >= 0) SetTimer(hwnd, 2, 600, NULL);
        }
        SetCapture(hwnd);
        {
            RECT cr;
            POINT pt; pt.x = x; pt.y = y;
            GetClientRect(hwnd, &cr);
            if (!PtInRect(&cr, pt) && g_pressed < 0) {
                ReleaseCapture();
                g_hot = -1; w95_tooltip_hide(); KillTimer(hwnd, 2);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == 2) {
            KillTimer(hwnd, 2);
            if (g_hot >= 0) {
                char tip[96];
                RECT r;
                POINT p;
                int t = g_app.group_tool[g_hot];
                slot_rect(g_hot, &r);
                p.x = r.left + 8; p.y = r.bottom + 2;
                ClientToScreen(hwnd, &p);
                pc_snprintf(tip, sizeof tip, g_tool_info[t].key ? "%s (%c)" : "%s", g_tool_info[t].name, g_tool_info[t].key);
                w95_tooltip_show(hwnd, p.x, p.y, tip);
                g_tip_slot = g_hot;
            }
        } else if (wp == 3) {
            /* Press-and-hold opens the flyout. */
            KillTimer(hwnd, 3);
            if (g_hold_slot >= 0 && g_pressed == g_hold_slot) {
                int s = g_hold_slot, t;
                g_pressed = -1;
                ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                t = flyout(s);
                if (t >= 0) app_set_tool(t);
            }
            g_hold_slot = -1;
        }
        return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_RBUTTONDOWN: {
        int s = hit_slot(x, y);
        RECT r;
        POINT p;
        p.x = x; p.y = y;
        w95_tooltip_hide();
        if (y < 10) {
            g_two_col = !g_two_col;
            reg_set_int("Settings", "ToolsTwoColumns", g_two_col);
            app_layout();
            return 0;
        }
        if (s >= 0) {
            int cur = g_app.group_tool[s];
            if (msg == WM_RBUTTONDOWN && slot_has_group(s)) { int t = flyout(s); if (t >= 0) app_set_tool(t); return 0; }
            if (GetKeyState(VK_MENU) < 0 && slot_has_group(s)) {
                /* Alt+click cycles the hidden tools, as in Photoshop 5.0. */
                int t;
                for (t = cur + 1; t < cur + T_COUNT; t++) if (g_tool_info[t % T_COUNT].group == s) { app_set_tool(t % T_COUNT); break; }
                return 0;
            }
            if (msg == WM_LBUTTONDBLCLK) {
                /* Double-clicking a tool brings its options forward. */
                app_set_tool(cur);
                return 0;
            }
            g_pressed = s;
            SetCapture(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            if (slot_has_group(s)) { g_hold_slot = s; SetTimer(hwnd, 3, 350, NULL); }
            return 0;
        }
        fg_rect(&r);
        if (PtInRect(&r, p)) { u32 c = g_app.fg; if (dlg_color_picker("Color Picker", &c)) app_set_colors(c, g_app.bg); return 0; }
        bg_rect(&r);
        if (PtInRect(&r, p)) { u32 c = g_app.bg; if (dlg_color_picker("Color Picker", &c)) app_set_colors(g_app.fg, c); return 0; }
        default_rect(&r);
        if (PtInRect(&r, p)) { cmd_run(CMD_DEFAULT_COLORS); return 0; }
        swap_rect(&r);
        if (PtInRect(&r, p)) { cmd_run(CMD_SWAP_COLORS); return 0; }
        qm_rect(0, &r);
        if (PtInRect(&r, p)) { if (g_app.quickmask) cmd_run(CMD_QUICK_MASK); return 0; }
        qm_rect(1, &r);
        if (PtInRect(&r, p)) { if (!g_app.quickmask) cmd_run(CMD_QUICK_MASK); return 0; }
        {
            int k;
            for (k = 0; k < 3; k++) { screen_rect(k, &r); if (PtInRect(&r, p)) { app_set_screen_mode(k); return 0; } }
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_pressed >= 0) {
            int s = hit_slot(x, y), was = g_pressed;
            g_pressed = -1;
            KillTimer(hwnd, 3);
            g_hold_slot = -1;
            ReleaseCapture();
            if (s == was) app_set_tool(g_app.group_tool[s]);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

HWND toolbox_create(HWND parent)
{
    w95_register_class("PCToolbox", tb_proc, CS_DBLCLKS, NULL);
    g_two_col = reg_get_int("Settings", "ToolsTwoColumns", 0);
    g_tb = CreateWindowExA(0, "PCToolbox", "Tools", WS_CHILD | WS_VISIBLE, 0, 0, toolbox_width(), 400, parent, NULL, w95_inst(), NULL);
    return g_tb;
}

void toolbox_refresh(void)
{
    if (g_tb) InvalidateRect(g_tb, NULL, FALSE);
}
