/*
 * PhotoCraft Classic - Windows 95 style window chrome and controls.
 * See chrome.h.  Win95 APIs only (ANSI); uxtheme is optional.
 */
#include <windows.h>
#include "pcrt.h"
#include "chrome.h"

/* Child-control flags kept in GWL_USERDATA (owner-drawn statics use it for
 * their icon handle instead and are told apart by control ID). */
#define CF_MAGIC   0x50430000L
#define CF_MASK    0xFFFF0000L
#define CF_FRAMED  0x0001L
#define CF_CHECK   0x0002L
#define CF_CHECKED 0x0004L
#define CF_WHITE   0x0008L
#define CF_BOLD    0x0010L

#define ID_SEPARATOR 0xFFF0
#define ID_ICON      0xFFF1

/* PC_WX_STATE bits */
#define ST_ACTIVE   1
#define ST_PRESSED  2
#define ST_TRACKING 4

static HINSTANCE g_inst;
static HFONT g_font, g_bold;
static HBRUSH g_face, g_white;

static const char PROGRESS_CLASS[] = "PCClassicProgress";
static const char MSGBOX_CLASS[] = "PCClassicMsgBox";

/* ------------------------------------------------------------------ */
/* drawing                                                             */

void pc_fill(HDC dc, RECT r, COLORREF c)
{
    COLORREF old = SetBkColor(dc, c);
    ExtTextOutA(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
    SetBkColor(dc, old);
}

static void hline(HDC dc, int x1, int x2, int y, COLORREF c) { RECT r; SetRect(&r, x1, y, x2, y + 1); pc_fill(dc, r, c); }
static void vline(HDC dc, int x, int y1, int y2, COLORREF c) { RECT r; SetRect(&r, x, y1, x + 1, y2); pc_fill(dc, r, c); }

/* One 1-pixel ring: top/left in 'tl', bottom/right in 'br'. */
static void ring(HDC dc, RECT r, COLORREF tl, COLORREF br)
{
    hline(dc, r.left, r.right - 1, r.top, tl);
    vline(dc, r.left, r.top, r.bottom - 1, tl);
    hline(dc, r.left, r.right, r.bottom - 1, br);
    vline(dc, r.right - 1, r.top, r.bottom, br);
}

void pc_draw_frame(HDC dc, RECT r, COLORREF tl_out, COLORREF br_out, COLORREF tl_in, COLORREF br_in)
{
    ring(dc, r, tl_out, br_out);
    InflateRect(&r, -1, -1);
    ring(dc, r, tl_in, br_in);
}

void pc_draw_raised(HDC dc, RECT r) { pc_draw_frame(dc, r, PC_RGB_LIGHT, PC_RGB_DARK, PC_RGB_HILITE, PC_RGB_SHADOW); }
void pc_draw_sunken(HDC dc, RECT r) { pc_draw_frame(dc, r, PC_RGB_SHADOW, PC_RGB_HILITE, PC_RGB_DARK, PC_RGB_LIGHT); }

/* ------------------------------------------------------------------ */
/* init                                                                */

static LRESULT CALLBACK progress_proc(HWND h, UINT m, WPARAM wp, LPARAM lp);

HINSTANCE pc_chrome_instance(void) { return g_inst; }
HFONT pc_chrome_font(void) { return g_font; }
HFONT pc_chrome_bold(void) { return g_bold; }
HBRUSH pc_chrome_face_brush(void) { return g_face; }

static HFONT make_font(int px, int weight)
{
    return CreateFontA(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, 3 /* NONANTIALIASED_QUALITY */, DEFAULT_PITCH | FF_SWISS, "MS Sans Serif");
}

void pc_chrome_init(HINSTANCE hinst)
{
    HDC dc;
    int px;
    HMODULE ux;
    WNDCLASSA wc;
    if (g_inst) return;
    g_inst = hinst;
    /* XP+: stop themed scroll bars etc. in this process. */
    {
        /* optional DLL: no "file not found" box on Windows 9x */
        UINT em = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
        ux = LoadLibraryA("UXTHEME.DLL");
        SetErrorMode(em);
    }
    if (ux) {
        FARPROC p = GetProcAddress(ux, "SetThemeAppProperties");
        if (p) {
            void (WINAPI *set_props)(DWORD);
            memcpy(&set_props, &p, sizeof p);
            set_props(0);
        }
        /* keep the library loaded: the setting lives in it */
    }
    dc = GetDC(NULL);
    px = MulDiv(8, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(NULL, dc);
    g_font = make_font(px, FW_NORMAL);
    g_bold = make_font(px, FW_BOLD);
    g_face = CreateSolidBrush(PC_RGB_FACE);
    g_white = (HBRUSH)GetStockObject(WHITE_BRUSH);

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = progress_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.lpszClassName = PROGRESS_CLASS;
    RegisterClassA(&wc);
}

ATOM pc_chrome_register_class(const char *name, WNDPROC proc)
{
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.cbWndExtra = PC_WX_BYTES;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = g_face;
    wc.lpszClassName = name;
    wc.hIcon = LoadIconA(g_inst, MAKEINTRESOURCEA(1));
    return RegisterClassA(&wc);
}

HWND pc_chrome_create(const char *cls, const char *title, int cw, int ch, HWND owner, void *param)
{
    int w = cw + 2 * PC_FRAME_BORDER, h = ch + 2 * PC_FRAME_BORDER + PC_FRAME_CAPTION, x, y;
    RECT ref;
    if (owner && IsWindowVisible(owner) && !IsIconic(owner)) GetWindowRect(owner, &ref);
    else if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &ref, 0)) SetRect(&ref, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    x = ref.left + (ref.right - ref.left - w) / 2;
    y = ref.top + (ref.bottom - ref.top - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return CreateWindowExA(owner ? 0 : WS_EX_APPWINDOW, cls, title, WS_POPUP | WS_CLIPCHILDREN | WS_SYSMENU,
                           x, y, w, h, owner, NULL, g_inst, param);
}

void *pc_chrome_param(HWND hwnd) { return (void *)GetWindowLongA(hwnd, PC_WX_USER); }

/* ------------------------------------------------------------------ */
/* non-client area                                                     */

static void caption_rects(HWND h, RECT *win, RECT *cap, RECT *close)
{
    GetWindowRect(h, win);
    OffsetRect(win, -win->left, -win->top);
    SetRect(cap, PC_FRAME_BORDER, PC_FRAME_BORDER, win->right - PC_FRAME_BORDER, PC_FRAME_BORDER + PC_FRAME_CAPTION - 1);
    SetRect(close, cap->right - 2 - 16, cap->top + 2, cap->right - 2, cap->top + 2 + 14);
}

static void draw_close_glyph(HDC dc, int x, int y)
{
    int i;
    /* 8x7 "X" with 2-pixel strokes, like Marlett's close glyph */
    for (i = 0; i < 7; i++) {
        RECT a, b;
        SetRect(&a, x + i, y + i, x + i + 2, y + i + 1);
        SetRect(&b, x + 6 - i, y + i, x + 8 - i, y + i + 1);
        pc_fill(dc, a, PC_RGB_DARK);
        pc_fill(dc, b, PC_RGB_DARK);
    }
}

static void paint_nc(HWND h)
{
    RECT win, cap, cl, r;
    HDC dc;
    LONG st = GetWindowLongA(h, PC_WX_STATE);
    char title[256];
    HFONT of;
    int active = (st & ST_ACTIVE) != 0, pressed = (st & ST_PRESSED) != 0;

    dc = GetWindowDC(h);
    if (!dc) return;
    caption_rects(h, &win, &cap, &cl);
    pc_draw_raised(dc, win);
    r = win;
    InflateRect(&r, -2, -2);
    ring(dc, r, PC_RGB_FACE, PC_RGB_FACE);
    /* caption bar + the face-coloured line under it */
    pc_fill(dc, cap, active ? PC_RGB_NAVY : PC_RGB_SHADOW);
    hline(dc, cap.left, cap.right, cap.bottom, PC_RGB_FACE);

    title[0] = 0;
    GetWindowTextA(h, title, sizeof title);
    SetRect(&r, cap.left + 3, cap.top, cl.left - 3, cap.bottom);
    of = (HFONT)SelectObject(dc, g_bold);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, active ? PC_RGB_HILITE : PC_RGB_FACE);
    DrawTextA(dc, title, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, of);

    /* close button */
    r = cl;
    pc_fill(dc, r, PC_RGB_FACE);
    if (pressed) pc_draw_frame(dc, r, PC_RGB_DARK, PC_RGB_HILITE, PC_RGB_SHADOW, PC_RGB_FACE);
    else pc_draw_frame(dc, r, PC_RGB_HILITE, PC_RGB_DARK, PC_RGB_FACE, PC_RGB_SHADOW);
    draw_close_glyph(dc, cl.left + 4 + pressed, cl.top + 3 + pressed);
    ReleaseDC(h, dc);
}

static int pt_in_close(HWND h, int sx, int sy)
{
    RECT win, cap, cl, wr;
    POINT p;
    GetWindowRect(h, &wr);
    caption_rects(h, &win, &cap, &cl);
    p.x = sx - wr.left;
    p.y = sy - wr.top;
    return PtInRect(&cl, p);
}

static void set_state(HWND h, LONG clear, LONG set)
{
    LONG st = GetWindowLongA(h, PC_WX_STATE), ns = (st & ~clear) | set;
    if (ns != st) { SetWindowLongA(h, PC_WX_STATE, ns); paint_nc(h); }
}

/* ------------------------------------------------------------------ */
/* owner-drawn controls                                                */

static LONG ctl_flags(HWND c)
{
    LONG f = GetWindowLongA(c, GWL_USERDATA);
    return (f & CF_MASK) == CF_MAGIC ? f : 0;
}

static int is_push_button(HWND c)
{
    char cls[16];
    if (!c || !GetClassNameA(c, cls, sizeof cls) || pc_stricmp(cls, "Button")) return 0;
    return !(ctl_flags(c) & CF_CHECK);
}

static void draw_text_state(HDC dc, const char *text, RECT r, UINT fmt, int disabled)
{
    SetBkMode(dc, TRANSPARENT);
    if (disabled) {
        RECT r2 = r;
        OffsetRect(&r2, 1, 1);
        SetTextColor(dc, PC_RGB_HILITE);
        DrawTextA(dc, text, -1, &r2, fmt);
        SetTextColor(dc, PC_RGB_SHADOW);
    } else {
        SetTextColor(dc, PC_RGB_DARK);
    }
    DrawTextA(dc, text, -1, &r, fmt);
}

static void draw_push(const DRAWITEMSTRUCT *d)
{
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    HWND parent = GetParent(d->hwndItem), focus = GetFocus();
    char text[128];
    int pressed = (d->itemState & ODS_SELECTED) != 0, focused = (d->itemState & ODS_FOCUS) != 0;
    int disabled = (d->itemState & ODS_DISABLED) != 0, isdef;
    HFONT of;
    isdef = focused || ((int)GetWindowLongA(parent, PC_WX_DEFID) == (int)d->CtlID && !(is_push_button(focus) && focus != d->hwndItem));
    pc_fill(dc, r, PC_RGB_FACE);
    if (isdef) { ring(dc, r, PC_RGB_DARK, PC_RGB_DARK); InflateRect(&r, -1, -1); }
    if (pressed) ring(dc, r, PC_RGB_SHADOW, PC_RGB_SHADOW);
    else pc_draw_frame(dc, r, PC_RGB_HILITE, PC_RGB_DARK, PC_RGB_LIGHT, PC_RGB_SHADOW);
    GetWindowTextA(d->hwndItem, text, sizeof text);
    of = (HFONT)SelectObject(dc, g_font);
    if (pressed) OffsetRect(&r, 1, 1);
    draw_text_state(dc, text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, disabled);
    SelectObject(dc, of);
    if (focused) {
        RECT f = d->rcItem;
        InflateRect(&f, -4, -4);
        if (pressed) OffsetRect(&f, 1, 1);
        SetTextColor(dc, PC_RGB_DARK);
        SetBkColor(dc, PC_RGB_FACE);
        DrawFocusRect(dc, &f);
    }
}

static void draw_check(const DRAWITEMSTRUCT *d)
{
    HDC dc = d->hDC;
    RECT r = d->rcItem, box, tr;
    char text[256];
    LONG f = ctl_flags(d->hwndItem);
    int disabled = (d->itemState & ODS_DISABLED) != 0, pressed = (d->itemState & ODS_SELECTED) != 0;
    int y0 = r.top + (r.bottom - r.top - 13) / 2, i;
    HFONT of;
    SIZE sz;
    pc_fill(dc, r, PC_RGB_FACE);
    SetRect(&box, r.left, y0, r.left + 13, y0 + 13);
    pc_draw_sunken(dc, box);
    InflateRect(&box, -2, -2);
    pc_fill(dc, box, (disabled || pressed) ? PC_RGB_FACE : PC_RGB_WINDOW);
    if (f & CF_CHECKED) {
        COLORREF c = disabled ? PC_RGB_SHADOW : PC_RGB_DARK;
        /* 7x7 check mark, 3 pixels thick */
        static const int ys[7] = { 2, 3, 4, 3, 2, 1, 0 };
        for (i = 0; i < 7; i++) {
            RECT p;
            SetRect(&p, box.left + 1 + i, box.top + 1 + ys[i] + 1, box.left + 2 + i, box.top + 1 + ys[i] + 4);
            pc_fill(dc, p, c);
        }
    }
    GetWindowTextA(d->hwndItem, text, sizeof text);
    of = (HFONT)SelectObject(dc, g_font);
    SetRect(&tr, r.left + 17, r.top, r.right, r.bottom);
    draw_text_state(dc, text, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, disabled);
    if (d->itemState & ODS_FOCUS) {
        RECT fr;
        GetTextExtentPoint32A(dc, text, (int)strlen(text), &sz);
        SetRect(&fr, tr.left - 1, tr.top + (tr.bottom - tr.top - sz.cy) / 2 - 1, tr.left + sz.cx + 1, 0);
        fr.bottom = fr.top + sz.cy + 2;
        SetTextColor(dc, PC_RGB_DARK);
        SetBkColor(dc, PC_RGB_FACE);
        DrawFocusRect(dc, &fr);
    }
    SelectObject(dc, of);
}

static void draw_static(const DRAWITEMSTRUCT *d)
{
    RECT r = d->rcItem;
    pc_fill(d->hDC, r, PC_RGB_FACE);
    if (d->CtlID == ID_SEPARATOR) {
        hline(d->hDC, r.left, r.right, r.top, PC_RGB_SHADOW);
        hline(d->hDC, r.left, r.right, r.top + 1, PC_RGB_HILITE);
    } else if (d->CtlID == ID_ICON) {
        HICON ic = (HICON)GetWindowLongA(d->hwndItem, GWL_USERDATA);
        if (ic) DrawIcon(d->hDC, r.left, r.top, ic);
    }
}

/* ------------------------------------------------------------------ */
/* progress bar                                                        */

static LRESULT CALLBACK progress_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        RECT r, in;
        int pos = (int)GetWindowLongA(h, GWL_USERDATA), bw, x, limit;
        HDC dc = BeginPaint(h, &ps);
        GetClientRect(h, &r);
        ring(dc, r, PC_RGB_SHADOW, PC_RGB_HILITE);      /* thin sunken border */
        in = r;
        InflateRect(&in, -1, -1);
        pc_fill(dc, in, PC_RGB_FACE);
        InflateRect(&in, -1, -1);
        bw = (in.bottom - in.top) * 2 / 3;
        if (bw < 3) bw = 3;
        if (pos < 0) pos = 0;
        if (pos > 1000) pos = 1000;
        limit = in.left + (int)((long)(in.right - in.left) * pos / 1000);
        for (x = in.left; x < limit; x += bw + 2) {
            RECT b;
            SetRect(&b, x, in.top, x + bw, in.bottom);
            if (b.right > in.right) b.right = in.right;
            pc_fill(dc, b, PC_RGB_NAVY);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcA(h, m, wp, lp);
}

void pc_progress_set(HWND bar, int permille)
{
    if (permille < 0) permille = 0;
    if (permille > 1000) permille = 1000;
    if (GetWindowLongA(bar, GWL_USERDATA) != permille) {
        SetWindowLongA(bar, GWL_USERDATA, permille);
        InvalidateRect(bar, NULL, FALSE);
    }
}

/* ------------------------------------------------------------------ */
/* control creation                                                    */

static HWND mk(HWND parent, DWORD ex, const char *cls, const char *text, DWORD style, int x, int y, int w, int h, int id, HFONT font)
{
    HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent, (HMENU)(INT_PTR)id, g_inst, NULL);
    if (c) SendMessageA(c, WM_SETFONT, (WPARAM)font, 0);
    return c;
}

HWND pc_ctl_label(HWND parent, int id, const char *text, int x, int y, int w, int h)
{
    return mk(parent, 0, "STATIC", text, SS_LEFT | SS_NOPREFIX, x, y, w, h, id, g_font);
}

HWND pc_ctl_label_bold(HWND parent, int id, const char *text, int x, int y, int w, int h)
{
    return mk(parent, 0, "STATIC", text, SS_LEFT | SS_NOPREFIX, x, y, w, h, id, g_bold);
}

HWND pc_ctl_button(HWND parent, int id, const char *text, int x, int y, int w, int h, int is_default)
{
    HWND b = mk(parent, 0, "BUTTON", text, BS_OWNERDRAW | BS_NOTIFY | WS_TABSTOP, x, y, w, h, id, g_font);
    if (b) SetWindowLongA(b, GWL_USERDATA, CF_MAGIC);
    if (is_default) SetWindowLongA(parent, PC_WX_DEFID, id);
    return b;
}

HWND pc_ctl_check(HWND parent, int id, const char *text, int x, int y, int w, int h, int checked)
{
    HWND b = mk(parent, 0, "BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, x, y, w, h, id, g_font);
    if (b) SetWindowLongA(b, GWL_USERDATA, CF_MAGIC | CF_CHECK | (checked ? CF_CHECKED : 0));
    return b;
}

int pc_ctl_get_check(HWND c) { return (ctl_flags(c) & CF_CHECKED) != 0; }

void pc_ctl_set_check(HWND c, int on)
{
    LONG f = ctl_flags(c);
    if (!(f & CF_CHECK)) return;
    SetWindowLongA(c, GWL_USERDATA, on ? (f | CF_CHECKED) : (f & ~CF_CHECKED));
    InvalidateRect(c, NULL, FALSE);
}

void pc_ctl_set_default(HWND parent, int id)
{
    HWND old = GetDlgItem(parent, (int)GetWindowLongA(parent, PC_WX_DEFID));
    SetWindowLongA(parent, PC_WX_DEFID, id);
    if (old) InvalidateRect(old, NULL, FALSE);
    if (GetDlgItem(parent, id)) InvalidateRect(GetDlgItem(parent, id), NULL, FALSE);
}

HWND pc_ctl_edit(HWND parent, int id, const char *text, int x, int y, int w, int h, DWORD style)
{
    HWND e = mk(parent, 0, "EDIT", text, style | WS_TABSTOP, x + 2, y + 2, w - 4, h - 4, id, g_font);
    if (e) SetWindowLongA(e, GWL_USERDATA, CF_MAGIC | CF_FRAMED | CF_WHITE);
    return e;
}

HWND pc_ctl_list(HWND parent, int id, int x, int y, int w, int h, DWORD style)
{
    HWND l = mk(parent, 0, "LISTBOX", "", style | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                x + 2, y + 2, w - 4, h - 4, id, g_font);
    if (l) SetWindowLongA(l, GWL_USERDATA, CF_MAGIC | CF_FRAMED | CF_WHITE);
    return l;
}

HWND pc_ctl_progress(HWND parent, int id, int x, int y, int w, int h)
{
    HWND p = CreateWindowExA(0, PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE, x, y, w, h, parent, (HMENU)(INT_PTR)id, g_inst, NULL);
    if (p) SetWindowLongA(p, GWL_USERDATA, 0);
    return p;
}

HWND pc_ctl_separator(HWND parent, int x, int y, int w)
{
    return mk(parent, 0, "STATIC", "", SS_OWNERDRAW, x, y, w, 2, ID_SEPARATOR, g_font);
}

HWND pc_ctl_icon(HWND parent, LPCSTR system_icon, int x, int y)
{
    HWND s = mk(parent, 0, "STATIC", "", SS_OWNERDRAW, x, y, 32, 32, ID_ICON, g_font);
    if (s) SetWindowLongA(s, GWL_USERDATA, (LONG)LoadIconA(NULL, system_icon));
    return s;
}

/* ------------------------------------------------------------------ */
/* window procedure helper                                             */

static void paint_frames(HWND h, HDC dc)
{
    HWND c;
    for (c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        RECT r;
        if (!(ctl_flags(c) & CF_FRAMED) || !IsWindowVisible(c)) continue;
        GetWindowRect(c, &r);
        MapWindowPoints(NULL, h, (POINT *)&r, 2);
        InflateRect(&r, 2, 2);
        pc_draw_sunken(dc, r);
    }
}

int pc_chrome_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    *res = 0;
    switch (msg) {
    case WM_NCCREATE:
        SetWindowLongA(h, PC_WX_USER, (LONG)((CREATESTRUCTA *)lp)->lpCreateParams);
        return 0;
    case WM_NCCALCSIZE: {
        RECT *r = wp ? &((NCCALCSIZE_PARAMS *)lp)->rgrc[0] : (RECT *)lp;
        r->left += PC_FRAME_BORDER;
        r->right -= PC_FRAME_BORDER;
        r->top += PC_FRAME_BORDER + PC_FRAME_CAPTION;
        r->bottom -= PC_FRAME_BORDER;
        return 1;
    }
    case WM_NCPAINT:
        paint_nc(h);
        return 1;
    case WM_NCACTIVATE:
        set_state(h, ST_ACTIVE, wp ? ST_ACTIVE : 0);
        paint_nc(h);
        *res = TRUE;
        return 1;
    case WM_SETTEXT:
        *res = DefWindowProcA(h, msg, wp, lp);
        paint_nc(h);
        return 1;
    case WM_NCHITTEST: {
        RECT wr;
        int x, y;
        GetWindowRect(h, &wr);
        x = (short)LOWORD(lp) - wr.left;
        y = (short)HIWORD(lp) - wr.top;
        if (x < PC_FRAME_BORDER || y < PC_FRAME_BORDER || x >= wr.right - wr.left - PC_FRAME_BORDER ||
            y >= wr.bottom - wr.top - PC_FRAME_BORDER)
            *res = HTBORDER;
        else if (y < PC_FRAME_BORDER + PC_FRAME_CAPTION)
            *res = pt_in_close(h, (short)LOWORD(lp), (short)HIWORD(lp)) ? HTBORDER : HTCAPTION;
        else
            *res = HTCLIENT;
        return 1;
    }
    case WM_NCLBUTTONDOWN:
        if (pt_in_close(h, (short)LOWORD(lp), (short)HIWORD(lp))) {
            set_state(h, 0, ST_PRESSED | ST_TRACKING);
            SetCapture(h);
            return 1;
        }
        return 0;
    case WM_NCLBUTTONDBLCLK:
        return wp == HTCAPTION;     /* no maximize */
    case WM_MOUSEMOVE:
        if (GetWindowLongA(h, PC_WX_STATE) & ST_TRACKING) {
            POINT p;
            p.x = (short)LOWORD(lp);
            p.y = (short)HIWORD(lp);
            ClientToScreen(h, &p);
            set_state(h, ST_PRESSED, pt_in_close(h, p.x, p.y) ? ST_PRESSED : 0);
            return 1;
        }
        return 0;
    case WM_LBUTTONUP:
        if (GetWindowLongA(h, PC_WX_STATE) & ST_TRACKING) {
            int inside = (GetWindowLongA(h, PC_WX_STATE) & ST_PRESSED) != 0;
            set_state(h, ST_PRESSED | ST_TRACKING, 0);
            ReleaseCapture();
            if (inside) PostMessageA(h, WM_CLOSE, 0, 0);
            return 1;
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (GetWindowLongA(h, PC_WX_STATE) & ST_TRACKING) set_state(h, ST_PRESSED | ST_TRACKING, 0);
        return 0;
    case WM_ERASEBKGND: {
        RECT r;
        HDC dc = (HDC)wp;
        GetClientRect(h, &r);
        pc_fill(dc, r, PC_RGB_FACE);
        paint_frames(h, dc);
        *res = 1;
        return 1;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT *d = (const DRAWITEMSTRUCT *)lp;
        if (d->CtlType == ODT_BUTTON) {
            if (ctl_flags(d->hwndItem) & CF_CHECK) draw_check(d); else draw_push(d);
            *res = TRUE;
            return 1;
        }
        if (d->CtlType == ODT_STATIC) { draw_static(d); *res = TRUE; return 1; }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if (ctl_flags((HWND)lp) & CF_WHITE) {
            SetBkColor((HDC)wp, PC_RGB_WINDOW);
            SetTextColor((HDC)wp, PC_RGB_DARK);
            *res = (LRESULT)g_white;
        } else {
            SetBkColor((HDC)wp, PC_RGB_FACE);
            SetTextColor((HDC)wp, PC_RGB_DARK);
            *res = (LRESULT)g_face;
        }
        return 1;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)wp, PC_RGB_WINDOW);
        SetTextColor((HDC)wp, PC_RGB_DARK);
        *res = (LRESULT)g_white;
        return 1;
    case WM_COMMAND: {
        HWND c = (HWND)lp;
        int code = HIWORD(wp);
        if (c && (ctl_flags(c) & CF_CHECK) && (code == BN_CLICKED || code == BN_DOUBLECLICKED)) {
            /* a double click is two clicks: toggle for BN_DOUBLECLICKED too */
            pc_ctl_set_check(c, !pc_ctl_get_check(c));
            if (code == BN_DOUBLECLICKED) { *res = 0; return 1; }
        }
        if (c && (code == BN_SETFOCUS || code == BN_KILLFOCUS) && (ctl_flags(c) & CF_MAGIC)) {
            /* BS_NOTIFY is only there to repaint the default button; the
             * application never sees these (they are not clicks). */
            HWND def = GetDlgItem(h, (int)GetWindowLongA(h, PC_WX_DEFID));
            if (def && def != c) InvalidateRect(def, NULL, FALSE);
            *res = 0;
            return 1;
        }
        return 0;   /* the application still sees it */
    }
    case DM_GETDEFID:
        if (GetWindowLongA(h, PC_WX_DEFID)) { *res = MAKELONG(GetWindowLongA(h, PC_WX_DEFID), DC_HASDEFID); return 1; }
        return 0;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* modal loop and message box                                          */

void pc_chrome_pump(void)
{
    MSG m;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT) { PostQuitMessage((int)m.wParam); return; }
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
}

void pc_chrome_modal(HWND h, HWND owner, volatile int *done)
{
    MSG m;
    HWND first;
    int reenable = owner && IsWindowEnabled(owner);
    if (reenable) EnableWindow(owner, FALSE);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    UpdateWindow(h);
    first = GetNextDlgTabItem(h, NULL, FALSE);
    if (GetWindowLongA(h, PC_WX_DEFID) && GetDlgItem(h, (int)GetWindowLongA(h, PC_WX_DEFID)) &&
        IsWindowEnabled(GetDlgItem(h, (int)GetWindowLongA(h, PC_WX_DEFID))))
        first = GetDlgItem(h, (int)GetWindowLongA(h, PC_WX_DEFID));
    if (first) SetFocus(first);
    while (!*done) {
        int r = GetMessageA(&m, NULL, 0, 0);
        if (r <= 0) { if (r == 0) PostQuitMessage((int)m.wParam); break; }
        if (IsWindow(h) && IsDialogMessageA(h, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    if (reenable) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
}

typedef struct {
    volatile int done;
    int result, nbuttons;
} msgbox_state;

static LRESULT CALLBACK msgbox_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    LRESULT lr;
    msgbox_state *s;
    if (pc_chrome_proc(h, m, wp, lp, &lr)) return lr;
    s = (msgbox_state *)pc_chrome_param(h);
    switch (m) {
    case WM_COMMAND:
        if (!s) break;
        if (LOWORD(wp) >= 101 && LOWORD(wp) <= 103 && HIWORD(wp) == BN_CLICKED) { s->result = LOWORD(wp) - 100; s->done = 1; }
        else if (LOWORD(wp) == IDCANCEL) { s->result = s->nbuttons; s->done = 1; }
        else if (LOWORD(wp) == IDOK) { s->result = 1; s->done = 1; }
        return 0;
    case WM_CLOSE:
        if (s) { s->result = s->nbuttons; s->done = 1; }
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

int pc_msgbox(HWND owner, const char *title, const char *text, LPCSTR icon,
              const char *b1, const char *b2, const char *b3)
{
    static int registered;
    msgbox_state st;
    const char *btn[3];
    HWND h;
    HDC dc;
    RECT tr;
    HFONT of;
    int nb = 0, i, tx, cw, ch, bw = 75, bh = 23, gap = 6, textw, by, bx;
    if (!registered) { pc_chrome_register_class(MSGBOX_CLASS, msgbox_proc); registered = 1; }
    if (b1) btn[nb++] = b1;
    if (b2) btn[nb++] = b2;
    if (b3) btn[nb++] = b3;
    if (!nb) btn[nb++] = "OK";
    for (i = 0; i < nb; i++) {
        /* widen buttons for long captions */
        int need = (int)strlen(btn[i]) * 6 + 16;
        if (need > bw) bw = need;
    }
    memset(&st, 0, sizeof st);
    st.nbuttons = nb;

    /* measure text */
    dc = GetDC(NULL);
    of = (HFONT)SelectObject(dc, g_font);
    SetRect(&tr, 0, 0, 320, 0);
    DrawTextA(dc, text, -1, &tr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EXPANDTABS);
    SelectObject(dc, of);
    ReleaseDC(NULL, dc);
    textw = tr.right < 120 ? 120 : tr.right;
    tx = icon ? 12 + 32 + 14 : 14;
    cw = tx + textw + 14;
    if (cw < nb * bw + (nb - 1) * gap + 24) cw = nb * bw + (nb - 1) * gap + 24;
    ch = (tr.bottom > 32 ? tr.bottom : 32) + 14 + 16 + bh + 12;
    if (icon) MessageBeep(icon == (LPCSTR)IDI_HAND ? MB_ICONHAND : icon == (LPCSTR)IDI_EXCLAMATION ? MB_ICONEXCLAMATION :
                          icon == (LPCSTR)IDI_QUESTION ? MB_ICONQUESTION : MB_ICONASTERISK);

    h = pc_chrome_create(MSGBOX_CLASS, title, cw, ch, owner, &st);
    if (!h) return nb;
    if (icon) pc_ctl_icon(h, icon, 12, 14);
    {
        int ty = 14 + (tr.bottom < 32 && icon ? (32 - tr.bottom) / 2 : 0);
        HWND lbl = mk(h, 0, "STATIC", text, SS_LEFT | SS_NOPREFIX, tx, ty, textw, tr.bottom, -1, g_font);
        (void)lbl;
    }
    by = ch - 12 - bh;
    bx = (cw - (nb * bw + (nb - 1) * gap)) / 2;
    for (i = 0; i < nb; i++) pc_ctl_button(h, 101 + i, btn[i], bx + i * (bw + gap), by, bw, bh, i == 0);
    pc_chrome_modal(h, owner, &st.done);
    DestroyWindow(h);
    return st.result ? st.result : nb;
}
