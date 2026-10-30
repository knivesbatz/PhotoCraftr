/*
 * Windows 95 drawing kit: colours, fonts, bevels, controls, icons, cursors
 * and the hand-drawn window frame.  Plain GDI only (Windows 95 API set).
 */
#include "w95.h"
#include "icons.h"

static HINSTANCE g_inst;
static HFONT g_fonts[W95_FONT_COUNT];
static HBRUSH g_dither;
int g_w95_gradient_captions;

HINSTANCE w95_inst(void) { return g_inst; }

static HFONT make_font(const char *face, int pt, int bold)
{
    HDC dc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(NULL, dc);
    /* NONANTIALIASED_QUALITY keeps ClearType off even on Windows 10; the
     * bitmap faces never smooth anyway. */
    return CreateFontA(h, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, face);
}

typedef void (WINAPI *SetThemeAppPropertiesFn)(DWORD);
typedef BOOL (WINAPI *SetProcessDPIAwareFn)(void);

static SetThemeAppPropertiesFn g_set_theme_props;

void w95_allow_themes(int on)
{
    /* STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT */
    if (g_set_theme_props) g_set_theme_props(on ? 7 : 0);
}

void w95_init(HINSTANCE inst)
{
    HMODULE m;
    WORD bits[8];
    HBITMAP bmp;
    int i;
    g_inst = inst;
    /* Windows XP+: switch visual styles off for this process so scroll bars
     * and standard controls keep the classic look.  Looked up at run time
     * because uxtheme.dll does not exist before XP. */
    m = LoadLibraryA("uxtheme.dll");
    if (m) {
        g_set_theme_props = (SetThemeAppPropertiesFn)(void *)GetProcAddress(m, "SetThemeAppProperties");
        w95_allow_themes(0);
    }
    /* Vista+: render 1:1 instead of letting DWM blur-stretch the pixel art. */
    m = GetModuleHandleA("user32.dll");
    if (m) {
        SetProcessDPIAwareFn f = (SetProcessDPIAwareFn)(void *)GetProcAddress(m, "SetProcessDPIAware");
        if (f) f();
    }
    g_fonts[W95_FONT_NORMAL] = make_font("MS Sans Serif", 8, 0);
    g_fonts[W95_FONT_BOLD] = make_font("MS Sans Serif", 8, 1);
    g_fonts[W95_FONT_SMALL] = make_font("Small Fonts", 7, 0);
    g_fonts[W95_FONT_SMALL_BOLD] = make_font("Small Fonts", 7, 1);
    for (i = 0; i < 8; i++) bits[i] = (WORD)((i & 1) ? 0xAAAA : 0x5555);
    bmp = CreateBitmap(8, 8, 1, 1, bits);
    g_dither = CreatePatternBrush(bmp);
    DeleteObject(bmp);
}

HFONT w95_font(W95FontId id) { return g_fonts[id]; }

int w95_text_width(HDC dc, W95FontId f, const char *s)
{
    SIZE sz;
    HFONT old = (HFONT)SelectObject(dc, g_fonts[f]);
    GetTextExtentPoint32A(dc, s, (int)strlen(s), &sz);
    SelectObject(dc, old);
    return sz.cx;
}

HBRUSH w95_dither_brush(void) { return g_dither; }

/* ---------------- primitives ---------------- */

void w95_fill(HDC dc, const RECT *r, COLORREF c)
{
    COLORREF old = SetBkColor(dc, c);
    ExtTextOutA(dc, 0, 0, ETO_OPAQUE, r, NULL, 0, NULL);
    SetBkColor(dc, old);
}

void w95_hline(HDC dc, int x0, int x1, int y, COLORREF c)
{
    RECT r;
    if (x1 <= x0) return;
    r.left = x0; r.right = x1; r.top = y; r.bottom = y + 1;
    w95_fill(dc, &r, c);
}

void w95_vline(HDC dc, int x, int y0, int y1, COLORREF c)
{
    RECT r;
    if (y1 <= y0) return;
    r.left = x; r.right = x + 1; r.top = y0; r.bottom = y1;
    w95_fill(dc, &r, c);
}

void w95_pixel(HDC dc, int x, int y, COLORREF c) { SetPixel(dc, x, y, c); }

void w95_rect(HDC dc, int x0, int y0, int x1, int y1, COLORREF c)
{
    w95_hline(dc, x0, x1, y0, c);
    w95_hline(dc, x0, x1, y1 - 1, c);
    w95_vline(dc, x0, y0, y1, c);
    w95_vline(dc, x1 - 1, y0, y1, c);
}

static void edge2(HDC dc, const RECT *r, COLORREF tl_out, COLORREF br_out, COLORREF tl_in, COLORREF br_in)
{
    int l = r->left, t = r->top, rr = r->right, b = r->bottom;
    w95_hline(dc, l, rr - 1, t, tl_out);
    w95_vline(dc, l, t, b - 1, tl_out);
    w95_hline(dc, l, rr, b - 1, br_out);
    w95_vline(dc, rr - 1, t, b, br_out);
    w95_hline(dc, l + 1, rr - 2, t + 1, tl_in);
    w95_vline(dc, l + 1, t + 1, b - 2, tl_in);
    w95_hline(dc, l + 1, rr - 1, b - 2, br_in);
    w95_vline(dc, rr - 2, t + 1, b - 1, br_in);
}

void w95_raised(HDC dc, const RECT *r) { edge2(dc, r, W95_LIGHT, W95_DKSHADOW, W95_HILIGHT, W95_SHADOW); }
void w95_sunken(HDC dc, const RECT *r) { edge2(dc, r, W95_SHADOW, W95_HILIGHT, W95_DKSHADOW, W95_LIGHT); }

void w95_button_edge(HDC dc, const RECT *r, int pressed, int is_default)
{
    RECT q = *r;
    if (is_default) {
        w95_rect(dc, q.left, q.top, q.right, q.bottom, W95_DKSHADOW);
        InflateRect(&q, -1, -1);
    }
    if (pressed) {
        /* Pressed push button: dark outline + shadow inside. */
        w95_rect(dc, q.left, q.top, q.right, q.bottom, W95_DKSHADOW);
        w95_rect(dc, q.left + 1, q.top + 1, q.right - 1, q.bottom - 1, W95_SHADOW);
    } else {
        edge2(dc, &q, W95_HILIGHT, W95_DKSHADOW, W95_LIGHT, W95_SHADOW);
    }
}

void w95_thin_raised(HDC dc, const RECT *r)
{
    w95_hline(dc, r->left, r->right - 1, r->top, W95_HILIGHT);
    w95_vline(dc, r->left, r->top, r->bottom - 1, W95_HILIGHT);
    w95_hline(dc, r->left, r->right, r->bottom - 1, W95_SHADOW);
    w95_vline(dc, r->right - 1, r->top, r->bottom, W95_SHADOW);
}

void w95_thin_sunken(HDC dc, const RECT *r)
{
    w95_hline(dc, r->left, r->right - 1, r->top, W95_SHADOW);
    w95_vline(dc, r->left, r->top, r->bottom - 1, W95_SHADOW);
    w95_hline(dc, r->left, r->right, r->bottom - 1, W95_HILIGHT);
    w95_vline(dc, r->right - 1, r->top, r->bottom, W95_HILIGHT);
}

void w95_etched(HDC dc, const RECT *r)
{
    w95_rect(dc, r->left, r->top, r->right - 1, r->bottom - 1, W95_SHADOW);
    w95_rect(dc, r->left + 1, r->top + 1, r->right, r->bottom, W95_HILIGHT);
    w95_vline(dc, r->left + 1, r->top + 1, r->bottom - 2, W95_SHADOW);
}

void w95_etched_hline(HDC dc, int x0, int x1, int y)
{
    w95_hline(dc, x0, x1, y, W95_SHADOW);
    w95_hline(dc, x0, x1, y + 1, W95_HILIGHT);
}

void w95_focus_rect(HDC dc, const RECT *r)
{
    /* DrawFocusRect is the authentic dotted XOR rectangle on every version. */
    COLORREF ot = SetTextColor(dc, RGB(0, 0, 0)), ob = SetBkColor(dc, RGB(255, 255, 255));
    DrawFocusRect(dc, r);
    SetTextColor(dc, ot); SetBkColor(dc, ob);
}

void w95_text(HDC dc, W95FontId f, const char *s, const RECT *r, UINT flags, COLORREF c)
{
    HFONT old = (HFONT)SelectObject(dc, g_fonts[f]);
    RECT q = *r;
    int om = SetBkMode(dc, TRANSPARENT);
    COLORREF oc = SetTextColor(dc, c);
    DrawTextA(dc, s, -1, &q, flags | DT_NOPREFIX);
    SetTextColor(dc, oc); SetBkMode(dc, om);
    SelectObject(dc, old);
}

void w95_text_disabled(HDC dc, W95FontId f, const char *s, const RECT *r, UINT flags)
{
    RECT q = *r;
    OffsetRect(&q, 1, 1);
    w95_text(dc, f, s, &q, flags, W95_HILIGHT);
    w95_text(dc, f, s, r, flags, W95_SHADOW);
}

void w95_text_accel(HDC dc, W95FontId f, const char *s, int x, int y, COLORREF c, int disabled)
{
    char buf[256];
    int ul = -1, n = 0, i;
    HFONT old;
    for (i = 0; s[i] && n < 255; i++) {
        if (s[i] == '&' && s[i + 1] && s[i + 1] != '&') { ul = n; continue; }
        if (s[i] == '&' && s[i + 1] == '&') i++;
        buf[n++] = s[i];
    }
    buf[n] = 0;
    old = (HFONT)SelectObject(dc, g_fonts[f]);
    SetBkMode(dc, TRANSPARENT);
    {
        int pass, passes = disabled ? 2 : 1;
        for (pass = 0; pass < passes; pass++) {
            int ox = disabled && pass == 0 ? 1 : 0;
            COLORREF col = disabled ? (pass == 0 ? W95_HILIGHT : W95_SHADOW) : c;
            SetTextColor(dc, col);
            TextOutA(dc, x + ox, y + ox, buf, n);
            if (ul >= 0) {
                SIZE a, b;
                TEXTMETRICA tm;
                GetTextExtentPoint32A(dc, buf, ul, &a);
                GetTextExtentPoint32A(dc, buf, ul + 1, &b);
                GetTextMetricsA(dc, &tm);
                w95_hline(dc, x + ox + a.cx, x + ox + b.cx, y + ox + tm.tmAscent + 1, col);
            }
        }
    }
    SelectObject(dc, old);
}

void w95_draw_button(HDC dc, const RECT *r, const char *label, int pressed, int disabled, int focused, int is_default)
{
    RECT q = *r;
    w95_fill(dc, r, W95_FACE);
    w95_button_edge(dc, r, pressed, is_default);
    if (pressed) OffsetRect(&q, 1, 1);
    if (label) {
        HFONT old = (HFONT)SelectObject(dc, g_fonts[W95_FONT_NORMAL]);
        SIZE sz;
        char plain[128];
        int i, n = 0;
        TEXTMETRICA tm;
        for (i = 0; label[i] && n < 127; i++) { if (label[i] == '&' && label[i + 1] != '&') continue; plain[n++] = label[i]; }
        plain[n] = 0;
        GetTextExtentPoint32A(dc, plain, n, &sz);
        GetTextMetricsA(dc, &tm);
        SelectObject(dc, old);
        w95_text_accel(dc, W95_FONT_NORMAL, label, (q.left + q.right - sz.cx) / 2, (q.top + q.bottom - tm.tmHeight) / 2, W95_TEXT, disabled);
    }
    if (focused) {
        RECT f = *r;
        InflateRect(&f, is_default ? -4 : -3, is_default ? -4 : -3);
        w95_focus_rect(dc, &f);
    }
}

void w95_draw_icon_button(HDC dc, const RECT *r, int icon, int state, int disabled)
{
    int iw = w95_icon_w(icon), ih = w95_icon_h(icon);
    int x = (r->left + r->right - iw) / 2, y = (r->top + r->bottom - ih) / 2;
    if (state == 3) {
        /* Checked (latched) tool button: dither background, sunken. */
        HBRUSH ob;
        COLORREF ot = SetTextColor(dc, W95_FACE), obk = SetBkColor(dc, W95_HILIGHT);
        ob = (HBRUSH)SelectObject(dc, g_dither);
        PatBlt(dc, r->left, r->top, r->right - r->left, r->bottom - r->top, PATCOPY);
        SelectObject(dc, ob);
        SetTextColor(dc, ot); SetBkColor(dc, obk);
        w95_thin_sunken(dc, r);
        x++; y++;
    } else {
        w95_fill(dc, r, W95_FACE);
        if (state == 1) w95_thin_raised(dc, r);
        else if (state == 2) { w95_thin_sunken(dc, r); x++; y++; }
    }
    if (icon >= 0) {
        if (disabled) w95_icon_disabled(dc, icon, x, y);
        else w95_icon(dc, icon, x, y);
    }
}

void w95_draw_checkbox(HDC dc, int x, int y, int checked, int disabled)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + 13; r.bottom = y + 13;
    w95_sunken(dc, &r);
    InflateRect(&r, -2, -2);
    w95_fill(dc, &r, disabled ? W95_FACE : W95_WINDOW);
    if (checked) {
        /* The 7x7 Win95 check mark, three pixels thick. */
        static const int ys[7] = { 2, 3, 4, 3, 2, 1, 0 };
        int i, k;
        COLORREF c = disabled ? W95_SHADOW : W95_TEXT;
        for (i = 0; i < 7; i++) for (k = 0; k < 3; k++) w95_pixel(dc, x + 3 + i, y + 3 + ys[i] + k, c);
    }
}

void w95_draw_radio(HDC dc, int x, int y, int checked, int disabled)
{
    /* 12x12 round radio button built from the Win95 pixel pattern. */
    static const char *const pat[12] = {
        "....ssss....", "..ssddddss..", ".sdd....ddh.", ".sd......lh.", "sd........lh", "sd........lh",
        "sd........lh", "sd........lh", ".sd......lh.", ".sll....llh.", "..hhllllhh..", "....hhhh...."
    };
    int i, j;
    for (j = 0; j < 12; j++)
        for (i = 0; i < 12; i++) {
            char c = pat[j][i];
            COLORREF col;
            int inside = (c == '.') && j > 1 && j < 10 && i > 1 && i < 10 && !((j == 2 || j == 9) && (i == 2 || i == 9));
            if (c == 's') col = W95_SHADOW;
            else if (c == 'd') col = W95_DKSHADOW;
            else if (c == 'h') col = W95_HILIGHT;
            else if (c == 'l') col = W95_LIGHT;
            else if (inside) col = disabled ? W95_FACE : W95_WINDOW;
            else continue;
            w95_pixel(dc, x + i, y + j, col);
        }
    if (checked) {
        COLORREF c = disabled ? W95_SHADOW : W95_TEXT;
        RECT r;
        r.left = x + 5; r.top = y + 4; r.right = x + 7; r.bottom = y + 8; w95_fill(dc, &r, c);
        r.left = x + 4; r.top = y + 5; r.right = x + 8; r.bottom = y + 7; w95_fill(dc, &r, c);
    }
}

void w95_draw_arrow(HDC dc, int cx, int cy, int dir, int size, COLORREF c)
{
    int i;
    for (i = 0; i < size; i++) {
        switch (dir) {
        case 0: w95_hline(dc, cx - i, cx + i + 1, cy - size / 2 + i, c); break;
        case 1: w95_hline(dc, cx - (size - 1 - i), cx + (size - 1 - i) + 1, cy - size / 2 + i, c); break;
        case 2: w95_vline(dc, cx - size / 2 + i, cy - i, cy + i + 1, c); break;
        default: w95_vline(dc, cx - size / 2 + i, cy - (size - 1 - i), cy + (size - 1 - i) + 1, c); break;
        }
    }
}

/* ---------------- scroll bars ---------------- */

static void sb_geometry(const RECT *r, int vertical, int pos, int page, int max, RECT *a1, RECT *a2, RECT *thumb, RECT *track)
{
    int len = vertical ? r->bottom - r->top : r->right - r->left;
    int bs = vertical ? PC_MIN(W95_SCROLL, len / 2) : PC_MIN(W95_SCROLL, len / 2);
    int tl = len - 2 * bs, tsz, tpos;
    *a1 = *r; *a2 = *r; *track = *r;
    if (vertical) { a1->bottom = r->top + bs; a2->top = r->bottom - bs; track->top = a1->bottom; track->bottom = a2->top; }
    else { a1->right = r->left + bs; a2->left = r->right - bs; track->left = a1->right; track->right = a2->left; }
    if (max <= page || tl < 8) { SetRectEmpty(thumb); return; }
    tsz = PC_MAX(8, (int)((double)tl * page / max));
    if (tsz > tl) tsz = tl;
    tpos = (int)((double)(tl - tsz) * pos / PC_MAX(1, max - page));
    *thumb = *track;
    if (vertical) { thumb->top = track->top + tpos; thumb->bottom = thumb->top + tsz; }
    else { thumb->left = track->left + tpos; thumb->right = thumb->left + tsz; }
}

void w95_draw_scrollbar(HDC dc, const RECT *r, int vertical, int pos, int page, int max, int pressed)
{
    RECT a1, a2, th, tr;
    int dis = max <= page;
    COLORREF ot, ob;
    HBRUSH obr;
    sb_geometry(r, vertical, pos, page, max, &a1, &a2, &th, &tr);
    ot = SetTextColor(dc, W95_FACE); ob = SetBkColor(dc, W95_HILIGHT);
    obr = (HBRUSH)SelectObject(dc, g_dither);
    PatBlt(dc, tr.left, tr.top, tr.right - tr.left, tr.bottom - tr.top, PATCOPY);
    SelectObject(dc, obr);
    SetTextColor(dc, ot); SetBkColor(dc, ob);
    if (pressed == 3 && !IsRectEmpty(&th)) {
        RECT q = tr;
        if (vertical) q.bottom = th.top; else q.right = th.left;
        w95_fill(dc, &q, W95_DKSHADOW);
    } else if (pressed == 4 && !IsRectEmpty(&th)) {
        RECT q = tr;
        if (vertical) q.top = th.bottom; else q.left = th.right;
        w95_fill(dc, &q, W95_DKSHADOW);
    }
    {
        int k;
        for (k = 0; k < 2; k++) {
            RECT *a = k ? &a2 : &a1;
            int pr = pressed == k + 1;
            int cx = (a->left + a->right) / 2 + pr, cy = (a->top + a->bottom) / 2 + pr;
            int dir = vertical ? k : 2 + k;
            w95_fill(dc, a, W95_FACE);
            if (pr) w95_rect(dc, a->left, a->top, a->right, a->bottom, W95_SHADOW);
            else w95_raised(dc, a);
            if (dis) { w95_draw_arrow(dc, cx + 1, cy + 1, dir, 4, W95_HILIGHT); w95_draw_arrow(dc, cx, cy, dir, 4, W95_SHADOW); }
            else w95_draw_arrow(dc, cx, cy, dir, 4, W95_TEXT);
        }
    }
    if (!IsRectEmpty(&th)) { w95_fill(dc, &th, W95_FACE); w95_raised(dc, &th); }
}

int w95_scroll_hit(const RECT *r, int vertical, int pos, int page, int max, int x, int y, int *thumb_off)
{
    RECT a1, a2, th, tr;
    POINT p;
    p.x = x; p.y = y;
    sb_geometry(r, vertical, pos, page, max, &a1, &a2, &th, &tr);
    if (PtInRect(&a1, p)) return 1;
    if (PtInRect(&a2, p)) return 2;
    if (PtInRect(&th, p)) { if (thumb_off) *thumb_off = vertical ? y - th.top : x - th.left; return 5; }
    if (PtInRect(&tr, p)) {
        if (IsRectEmpty(&th)) return 0;
        return (vertical ? y < th.top : x < th.left) ? 3 : 4;
    }
    return 0;
}

/* ---------------- icons ---------------- */

typedef struct { HBITMAP color, mask, shape; int w, h; } IconCache;
static IconCache g_icons[PC_ICON_COUNT];

static void build_icon(int id)
{
    const PcIconDef *def = pc_icon_def(id);
    IconCache *c = &g_icons[id];
    HDC sdc, cdc, mdc, sh;
    HBITMAP oc, om, os;
    int x, y;
    if (!def || c->color) return;
    sdc = GetDC(NULL);
    cdc = CreateCompatibleDC(sdc); mdc = CreateCompatibleDC(sdc); sh = CreateCompatibleDC(sdc);
    c->w = def->w; c->h = def->h;
    c->color = CreateCompatibleBitmap(sdc, def->w, def->h);
    c->mask = CreateBitmap(def->w, def->h, 1, 1, NULL);
    c->shape = CreateBitmap(def->w, def->h, 1, 1, NULL);
    oc = (HBITMAP)SelectObject(cdc, c->color);
    om = (HBITMAP)SelectObject(mdc, c->mask);
    os = (HBITMAP)SelectObject(sh, c->shape);
    for (y = 0; y < def->h; y++)
        for (x = 0; x < def->w; x++) {
            unsigned rgb;
            int opaque = pc_icon_rgb(def->px[y * def->w + x], &rgb);
            COLORREF col = opaque ? RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255) : RGB(0, 0, 0);
            int lum = opaque ? (int)(((rgb >> 16) & 255) * 3 + ((rgb >> 8) & 255) * 6 + (rgb & 255)) / 10 : 255;
            SetPixel(cdc, x, y, col);
            SetPixel(mdc, x, y, opaque ? RGB(0, 0, 0) : RGB(255, 255, 255));
            /* Disabled shape: the darker pixels, as Win95 toolbars emboss them. */
            SetPixel(sh, x, y, (opaque && lum < 160) ? RGB(255, 255, 255) : RGB(0, 0, 0));
        }
    SelectObject(cdc, oc); SelectObject(mdc, om); SelectObject(sh, os);
    DeleteDC(cdc); DeleteDC(mdc); DeleteDC(sh);
    ReleaseDC(NULL, sdc);
}

int w95_icon_w(int id) { const PcIconDef *d = pc_icon_def(id); return d ? d->w : 0; }
int w95_icon_h(int id) { const PcIconDef *d = pc_icon_def(id); return d ? d->h : 0; }

void w95_icon(HDC dc, int id, int x, int y)
{
    HDC m;
    HBITMAP o;
    COLORREF ot, ob;
    if (id < 0 || id >= PC_ICON_COUNT) return;
    build_icon(id);
    if (!g_icons[id].color) return;
    m = CreateCompatibleDC(dc);
    ot = SetTextColor(dc, RGB(0, 0, 0)); ob = SetBkColor(dc, RGB(255, 255, 255));
    o = (HBITMAP)SelectObject(m, g_icons[id].mask);
    BitBlt(dc, x, y, g_icons[id].w, g_icons[id].h, m, 0, 0, SRCAND);
    SelectObject(m, g_icons[id].color);
    BitBlt(dc, x, y, g_icons[id].w, g_icons[id].h, m, 0, 0, SRCPAINT);
    SelectObject(m, o);
    SetTextColor(dc, ot); SetBkColor(dc, ob);
    DeleteDC(m);
}

static void paint_shape(HDC dc, HBITMAP shape, int x, int y, int w, int h, COLORREF c)
{
    HDC m = CreateCompatibleDC(dc);
    HBRUSH br = CreateSolidBrush(c), ob;
    HBITMAP o = (HBITMAP)SelectObject(m, shape);
    COLORREF ot = SetTextColor(dc, RGB(0, 0, 0)), obk = SetBkColor(dc, RGB(255, 255, 255));
    ob = (HBRUSH)SelectObject(dc, br);
    BitBlt(dc, x, y, w, h, m, 0, 0, 0x00E20746);   /* DSPDxax: brush where shape bit set */
    SelectObject(dc, ob);
    SetTextColor(dc, ot); SetBkColor(dc, obk);
    SelectObject(m, o);
    DeleteObject(br);
    DeleteDC(m);
}

void w95_icon_disabled(HDC dc, int id, int x, int y)
{
    if (id < 0 || id >= PC_ICON_COUNT) return;
    build_icon(id);
    if (!g_icons[id].shape) return;
    paint_shape(dc, g_icons[id].shape, x + 1, y + 1, g_icons[id].w, g_icons[id].h, W95_HILIGHT);
    paint_shape(dc, g_icons[id].shape, x, y, g_icons[id].w, g_icons[id].h, W95_SHADOW);
}

static HCURSOR g_cursors[PC_CURSOR_COUNT];

HCURSOR w95_cursor(int id)
{
    const PcCursorDef *d;
    BYTE andm[128], xorm[128];
    int x, y;
    if (id < 0 || id >= PC_CURSOR_COUNT) return LoadCursor(NULL, IDC_ARROW);
    if (g_cursors[id]) return g_cursors[id];
    d = pc_cursor_def(id);
    if (!d || d->w != 32 || d->h != 32) return LoadCursor(NULL, IDC_ARROW);
    memset(andm, 0xFF, sizeof andm);
    memset(xorm, 0, sizeof xorm);
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            char c = d->px[y * 32 + x];
            int byte = y * 4 + x / 8, bit = 0x80 >> (x & 7);
            if (c == 'k') { andm[byte] &= (BYTE)~bit; }
            else if (c == 'w') { andm[byte] &= (BYTE)~bit; xorm[byte] |= (BYTE)bit; }
            else if (c == 'x') { xorm[byte] |= (BYTE)bit; }
        }
    g_cursors[id] = CreateCursor(g_inst, d->hotx, d->hoty, 32, 32, andm, xorm);
    if (!g_cursors[id]) g_cursors[id] = LoadCursor(NULL, IDC_ARROW);
    return g_cursors[id];
}

HICON w95_app_icon(int small)
{
    (void)small;
    return LoadIconA(g_inst, MAKEINTRESOURCEA(1));
}

/* ---------------- double buffering ---------------- */

HDC w95_buf_begin(W95Buf *b, HDC target, const RECT *r)
{
    b->target = target; b->x = r->left; b->y = r->top;
    b->w = PC_MAX(1, r->right - r->left); b->h = PC_MAX(1, r->bottom - r->top);
    b->dc = CreateCompatibleDC(target);
    b->bmp = CreateCompatibleBitmap(target, b->w, b->h);
    b->old = (HBITMAP)SelectObject(b->dc, b->bmp);
    SetWindowOrgEx(b->dc, b->x, b->y, NULL);
    return b->dc;
}

void w95_buf_end(W95Buf *b)
{
    SetWindowOrgEx(b->dc, 0, 0, NULL);
    BitBlt(b->target, b->x, b->y, b->w, b->h, b->dc, 0, 0, SRCCOPY);
    SelectObject(b->dc, b->old);
    DeleteObject(b->bmp);
    DeleteDC(b->dc);
}

/* ---------------- window frame ---------------- */

void w95_frame_metrics(const W95Frame *f, int *border, int *caption)
{
    int b = (f->flags & W95_FRAME_SIZABLE) ? W95_BORDER : W95_DLGBORDER;
    if (f->maximized) b = 0;
    if (f->flags & W95_FRAME_SMALL) b = (f->flags & W95_FRAME_SIZABLE) ? 3 : 2;
    *border = b;
    *caption = (f->flags & W95_FRAME_NOCAPTION) ? 0 : (f->flags & W95_FRAME_SMALL) ? W95_SMCAPTION_H + 1 : W95_CAPTION_H + 1;
}

void w95_frame_adjust(int flags, int *w, int *h)
{
    W95Frame f;
    int b, c;
    memset(&f, 0, sizeof f);
    f.flags = flags;
    w95_frame_metrics(&f, &b, &c);
    *w += 2 * b;
    *h += 2 * b + c;
}

void w95_frame_attach(HWND hwnd, W95Frame *f, int flags)
{
    memset(f, 0, sizeof *f);
    f->flags = flags;
    f->hwnd = hwnd;
    f->active = 1;
    f->min_w = 120; f->min_h = 60;
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

/* Caption buttons right-to-left: close, maximize, minimize (or help). */
static int caption_buttons(const W95Frame *f, const RECT *wr, RECT out[4], int ids[4])
{
    int b, c, n = 0, bw, bh, x, y;
    w95_frame_metrics(f, &b, &c);
    if (f->flags & W95_FRAME_NOCAPTION) return 0;
    if (f->flags & W95_FRAME_SMALL) { bw = 9; bh = 8; }
    else { bw = W95_CAPBTN_W; bh = W95_CAPBTN_H; }
    y = b + (c - 1 - bh) / 2;
    x = (wr->right - wr->left) - b - 2 - bw;
    if (f->flags & W95_FRAME_CLOSE) { SetRect(&out[n], x, y, x + bw, y + bh); ids[n++] = 1; x -= bw + 2; }
    if (f->flags & W95_FRAME_HELP) { SetRect(&out[n], x, y, x + bw, y + bh); ids[n++] = 4; x -= bw; }
    if (f->flags & W95_FRAME_MINMAX) {
        SetRect(&out[n], x, y, x + bw, y + bh); ids[n++] = 2; x -= bw;
        SetRect(&out[n], x, y, x + bw, y + bh); ids[n++] = 3;
    }
    return n;
}

static void draw_caption_glyph(HDC dc, const RECT *r, int id, int pressed, int small, int maximized)
{
    int ox = r->left + (pressed ? 1 : 0), oy = r->top + (pressed ? 1 : 0);
    int w = r->right - r->left, h = r->bottom - r->top;
    if (small) {
        /* 9x8 palette close box: a raised square with a tiny cross. */
        int i;
        for (i = 0; i < 5; i++) { w95_pixel(dc, ox + 2 + i, oy + 1 + i, W95_TEXT); w95_pixel(dc, ox + 6 - i, oy + 1 + i, W95_TEXT); }
        (void)w; (void)h;
        return;
    }
    switch (id) {
    case 1: { /* X */
        int i, cx = ox + w / 2 - 4, cy = oy + h / 2 - 4;
        for (i = 0; i < 7; i++) {
            w95_pixel(dc, cx + i, cy + i, W95_TEXT); w95_pixel(dc, cx + i + 1, cy + i, W95_TEXT);
            w95_pixel(dc, cx + 6 - i, cy + i, W95_TEXT); w95_pixel(dc, cx + 7 - i, cy + i, W95_TEXT);
        }
        break;
    }
    case 2: /* maximize / restore */
        if (maximized) {
            int x0 = ox + 3, y0 = oy + 2;
            w95_rect(dc, x0 + 2, y0, x0 + 8, y0 + 5, W95_TEXT); w95_hline(dc, x0 + 2, x0 + 8, y0 + 1, W95_TEXT);
            w95_fill(dc, &(RECT){ x0, y0 + 3, x0 + 6, y0 + 9 }, W95_FACE);
            w95_rect(dc, x0, y0 + 3, x0 + 6, y0 + 9, W95_TEXT); w95_hline(dc, x0, x0 + 6, y0 + 4, W95_TEXT);
        } else {
            int x0 = ox + 3, y0 = oy + 2;
            w95_rect(dc, x0, y0, x0 + 9, y0 + 9, W95_TEXT); w95_hline(dc, x0, x0 + 9, y0 + 1, W95_TEXT);
        }
        break;
    case 3: /* minimize */
        w95_fill(dc, &(RECT){ ox + 4, oy + h - 5, ox + 10, oy + h - 3 }, W95_TEXT);
        break;
    case 4: { /* ? */
        RECT t = *r;
        OffsetRect(&t, pressed, pressed);
        w95_text(dc, W95_FONT_BOLD, "?", &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
        break;
    }
    }
}

static void frame_paint(HWND hwnd, W95Frame *f)
{
    HDC wdc = GetWindowDC(hwnd);
    RECT wr, r, cap;
    int b, c, n, i, ids[4];
    RECT btn[4];
    char title[256];
    W95Buf buf;
    HDC dc;
    GetWindowRect(hwnd, &wr);
    OffsetRect(&wr, -wr.left, -wr.top);
    w95_frame_metrics(f, &b, &c);
    /* Exclude the client area; paint the frame double-buffered. */
    ExcludeClipRect(wdc, b, b + c, wr.right - b, wr.bottom - b);
    dc = w95_buf_begin(&buf, wdc, &wr);
    w95_fill(dc, &wr, W95_FACE);
    if (b >= 2) {
        r = wr;
        w95_raised(dc, &r);
    } else if (b == 1) {
        w95_rect(dc, 0, 0, wr.right, wr.bottom, W95_SHADOW);
    }
    if (c > 0) {
        COLORREF c1 = f->active ? W95_CAPTION : W95_INACTIVE;
        COLORREF c2 = f->active ? W95_CAPTION2 : W95_INACTIVE2;
        int small = (f->flags & W95_FRAME_SMALL) != 0;
        SetRect(&cap, b, b, wr.right - b, b + c - 1);
        if (g_w95_gradient_captions && !small) {
            int x, wd = cap.right - cap.left;
            for (x = 0; x < wd; x += 2) {
                RECT s;
                int t = wd > 1 ? x * 255 / (wd - 1) : 0;
                COLORREF col = RGB((GetRValue(c1) * (255 - t) + GetRValue(c2) * t) / 255,
                                   (GetGValue(c1) * (255 - t) + GetGValue(c2) * t) / 255,
                                   (GetBValue(c1) * (255 - t) + GetBValue(c2) * t) / 255);
                SetRect(&s, cap.left + x, cap.top, PC_MIN(cap.left + x + 2, cap.right), cap.bottom);
                w95_fill(dc, &s, col);
            }
        } else {
            w95_fill(dc, &cap, c1);
        }
        GetWindowTextA(hwnd, title, sizeof title);
        r = cap;
        if (!small && !(f->flags & W95_FRAME_DIALOG)) {
            HICON ic = w95_app_icon(1);
            if (ic) DrawIconEx(dc, cap.left + 1, cap.top + 1, ic, 16, 16, 0, NULL, DI_NORMAL);
            r.left += 20;
        } else {
            r.left += small ? 2 : 3;
        }
        n = caption_buttons(f, &wr, btn, ids);
        r.right = n ? btn[n - 1].left - 2 : cap.right;
        if (small) {
            w95_text(dc, W95_FONT_SMALL_BOLD, title, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, f->active ? W95_HILIGHT : W95_INACTTEXT);
        } else {
            w95_text(dc, W95_FONT_BOLD, title, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, f->active ? W95_HILIGHT : W95_INACTTEXT);
        }
        for (i = 0; i < n; i++) {
            int pressed = f->pressed == ids[i] && f->hot == ids[i];
            w95_fill(dc, &btn[i], W95_FACE);
            if (small) { if (pressed) w95_thin_sunken(dc, &btn[i]); else w95_thin_raised(dc, &btn[i]); }
            else w95_button_edge(dc, &btn[i], pressed, 0);
            draw_caption_glyph(dc, &btn[i], ids[i], pressed, small, f->maximized);
        }
        /* The separator line between caption and client in Win95 is face colour (already filled). */
    }
    w95_buf_end(&buf);
    ReleaseDC(hwnd, wdc);
}

void w95_frame_maximize(HWND hwnd, W95Frame *f, int on)
{
    int child = (GetWindowLongA(hwnd, GWL_STYLE) & WS_CHILD) != 0;
    if (on && !f->maximized) {
        RECT wa;
        GetWindowRect(hwnd, &f->restore);
        if (child) {
            MapWindowPoints(NULL, GetParent(hwnd), (POINT *)&f->restore, 2);
            GetClientRect(GetParent(hwnd), &wa);
        } else {
            SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
        }
        f->maximized = 1;
        SetWindowPos(hwnd, NULL, wa.left, wa.top, wa.right - wa.left, wa.bottom - wa.top, SWP_NOZORDER | SWP_FRAMECHANGED);
    } else if (!on && f->maximized) {
        f->maximized = 0;
        SetWindowPos(hwnd, NULL, f->restore.left, f->restore.top, f->restore.right - f->restore.left,
                     f->restore.bottom - f->restore.top, SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

static int frame_hittest(HWND hwnd, W95Frame *f, int sx, int sy)
{
    RECT wr, btn[4];
    int b, c, x, y, n, i, ids[4], grip = 8;
    GetWindowRect(hwnd, &wr);
    x = sx - wr.left; y = sy - wr.top;
    OffsetRect(&wr, -wr.left, -wr.top);
    w95_frame_metrics(f, &b, &c);
    n = caption_buttons(f, &wr, btn, ids);
    for (i = 0; i < n; i++) {
        POINT p; p.x = x; p.y = y;
        if (PtInRect(&btn[i], p)) return ids[i] == 1 ? HTCLOSE : ids[i] == 2 ? HTMAXBUTTON : ids[i] == 3 ? HTMINBUTTON : HTHELP;
    }
    if ((f->flags & W95_FRAME_SIZABLE) && !f->maximized) {
        int l = x < b + (y < grip || y > wr.bottom - grip ? grip - b : 0);
        int rr = x >= wr.right - b - (y < grip || y > wr.bottom - grip ? grip - b : 0);
        int t = y < b, bt = y >= wr.bottom - b;
        if (x < grip && y < grip) return HTTOPLEFT;
        if (x >= wr.right - grip && y < grip) return HTTOPRIGHT;
        if (x < grip && y >= wr.bottom - grip) return HTBOTTOMLEFT;
        if (x >= wr.right - grip && y >= wr.bottom - grip) return HTBOTTOMRIGHT;
        if (l) return HTLEFT;
        if (rr) return HTRIGHT;
        if (t) return HTTOP;
        if (bt) return HTBOTTOM;
    }
    if (y < b + c && c > 0) return HTCAPTION;
    if (x < b || y < b || x >= wr.right - b || y >= wr.bottom - b) return HTBORDER;
    return HTCLIENT;
}

int w95_frame_proc(HWND hwnd, W95Frame *f, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    switch (msg) {
    case WM_NCCALCSIZE: {
        RECT *r = (RECT *)lp;
        int b, c;
        w95_frame_metrics(f, &b, &c);
        r->left += b; r->right -= b; r->top += b + c; r->bottom -= b;
        if (r->right < r->left) r->right = r->left;
        if (r->bottom < r->top) r->bottom = r->top;
        *res = 0;
        return 1;
    }
    case WM_NCPAINT:
        frame_paint(hwnd, f);
        *res = 0;
        return 1;
    case WM_NCACTIVATE:
        f->active = wp != 0;
        frame_paint(hwnd, f);
        *res = TRUE;
        return 1;
    case WM_SETTEXT:
        *res = DefWindowProcA(hwnd, msg, wp, lp);
        frame_paint(hwnd, f);
        return 1;
    case WM_NCHITTEST:
        *res = frame_hittest(hwnd, f, (short)LOWORD(lp), (short)HIWORD(lp));
        return 1;
    case WM_NCLBUTTONDBLCLK:
        if (wp == HTCAPTION && (f->flags & W95_FRAME_MINMAX)) { w95_frame_maximize(hwnd, f, !f->maximized); *res = 0; return 1; }
        if (wp == HTCAPTION && (f->flags & W95_FRAME_SMALL)) return 0;   /* let the owner collapse palettes */
        break;
    case WM_NCLBUTTONDOWN:
        if (wp == HTCLOSE || wp == HTMAXBUTTON || wp == HTMINBUTTON || wp == HTHELP) {
            f->pressed = f->hot = wp == HTCLOSE ? 1 : wp == HTMAXBUTTON ? 2 : wp == HTMINBUTTON ? 3 : 4;
            f->tracking = 1;
            SetCapture(hwnd);
            frame_paint(hwnd, f);
            *res = 0;
            return 1;
        }
        if (wp >= HTLEFT && wp <= HTBOTTOMRIGHT) {
            f->drag_mode = (int)wp;
            GetCursorPos(&f->drag_start);
            GetWindowRect(hwnd, &f->drag_rect);
            f->tracking = 2;
            SetCapture(hwnd);
            *res = 0;
            return 1;
        }
        if (wp == HTCAPTION && f->maximized) { SetActiveWindow(hwnd); *res = 0; return 1; }
        break;
    case WM_MOUSEMOVE:
        if (f->tracking == 1) {
            POINT p;
            int hot;
            GetCursorPos(&p);
            hot = frame_hittest(hwnd, f, p.x, p.y);
            hot = hot == HTCLOSE ? 1 : hot == HTMAXBUTTON ? 2 : hot == HTMINBUTTON ? 3 : hot == HTHELP ? 4 : 0;
            if (hot != f->hot) { f->hot = hot; frame_paint(hwnd, f); }
            *res = 0;
            return 1;
        }
        if (f->tracking == 2) {
            POINT p;
            RECT r = f->drag_rect;
            int dx, dy, m = f->drag_mode;
            GetCursorPos(&p);
            dx = p.x - f->drag_start.x; dy = p.y - f->drag_start.y;
            if (m == HTLEFT || m == HTTOPLEFT || m == HTBOTTOMLEFT) r.left = PC_MIN(r.left + dx, r.right - f->min_w);
            if (m == HTRIGHT || m == HTTOPRIGHT || m == HTBOTTOMRIGHT) r.right = PC_MAX(r.right + dx, r.left + f->min_w);
            if (m == HTTOP || m == HTTOPLEFT || m == HTTOPRIGHT) r.top = PC_MIN(r.top + dy, r.bottom - f->min_h);
            if (m == HTBOTTOM || m == HTBOTTOMLEFT || m == HTBOTTOMRIGHT) r.bottom = PC_MAX(r.bottom + dy, r.top + f->min_h);
            if (GetWindowLongA(hwnd, GWL_STYLE) & WS_CHILD) MapWindowPoints(NULL, GetParent(hwnd), (POINT *)&r, 2);
            SetWindowPos(hwnd, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
            *res = 0;
            return 1;
        }
        break;
    case WM_LBUTTONUP:
        if (f->tracking == 1) {
            int which = f->hot == f->pressed ? f->pressed : 0;
            f->tracking = 0; f->pressed = 0; f->hot = 0;
            ReleaseCapture();
            frame_paint(hwnd, f);
            if (which == 1) PostMessageA(hwnd, WM_CLOSE, 0, 0);
            else if (which == 2) w95_frame_maximize(hwnd, f, !f->maximized);
            else if (which == 3) ShowWindow(hwnd, SW_MINIMIZE);
            else if (which == 4) PostMessageA(hwnd, WM_HELP, 0, 0);
            *res = 0;
            return 1;
        }
        if (f->tracking == 2) {
            f->tracking = 0;
            ReleaseCapture();
            *res = 0;
            return 1;
        }
        break;
    case WM_CAPTURECHANGED:
        if (f->tracking) { f->tracking = 0; f->pressed = 0; frame_paint(hwnd, f); }
        break;
    case WM_GETMINMAXINFO:
        break;
    }
    return 0;
}

ATOM w95_register_class(const char *name, WNDPROC proc, UINT style, HCURSOR cur)
{
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.style = style;
    wc.lpfnWndProc = proc;
    wc.hInstance = g_inst;
    wc.hCursor = cur ? cur : LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = w95_app_icon(0);
    wc.hbrBackground = NULL;
    wc.lpszClassName = name;
    return RegisterClassA(&wc);
}

/* ---------------- tooltip ---------------- */

static HWND g_tip;
static char g_tip_text[128];

static LRESULT CALLBACK tip_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        w95_fill(dc, &r, W95_TOOLTIP);
        w95_rect(dc, r.left, r.top, r.right, r.bottom, W95_DKSHADOW);
        r.left += 3;
        w95_text(dc, W95_FONT_NORMAL, g_tip_text, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void w95_tooltip_show(HWND owner, int sx, int sy, const char *text)
{
    HDC dc;
    int w;
    if (!g_tip) {
        w95_register_class("PCW95Tip", tip_proc, 0, NULL);
        g_tip = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "PCW95Tip", "", WS_POPUP, 0, 0, 10, 10, owner, NULL, g_inst, NULL);
    }
    if (!g_tip) return;
    pc_strlcpy(g_tip_text, text, sizeof g_tip_text);
    dc = GetDC(g_tip);
    w = w95_text_width(dc, W95_FONT_NORMAL, text) + 7;
    ReleaseDC(g_tip, dc);
    /* Keep it on screen (panels sit at the right edge). */
    if (sx + w > GetSystemMetrics(SM_CXSCREEN)) sx = PC_MAX(0, GetSystemMetrics(SM_CXSCREEN) - w);
    SetWindowPos(g_tip, HWND_TOPMOST, sx, sy, w, 17, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_tip, NULL, FALSE);
}

void w95_tooltip_hide(void)
{
    if (g_tip) ShowWindow(g_tip, SW_HIDE);
}
