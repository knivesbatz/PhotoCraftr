/*
 * Modal dialog framework (see dlg.h).
 */
#include "dlg.h"

static LRESULT CALLBACK dlg_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK custom_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK slider_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void ensure_classes(void)
{
    static int done;
    if (done) return;
    done = 1;
    w95_register_class("PCDialog", dlg_proc, CS_DBLCLKS, NULL);
    w95_register_class("PCDlgCustom", custom_proc, CS_DBLCLKS, NULL);
    w95_register_class("PCSlider", slider_proc, 0, NULL);
}

Dlg *dlg_create(const char *title, int w, int h, DlgChangeFn on_change, void *user)
{
    Dlg *d = (Dlg *)calloc(1, sizeof *d);
    RECT owner;
    int ow = w, oh = h, x, y;
    HWND parent = g_app.main;
    if (!d) return NULL;
    ensure_classes();
    d->on_change = on_change;
    d->user = user;
    w95_frame_adjust(W95_FRAME_DIALOG | W95_FRAME_CLOSE, &ow, &oh);
    if (parent && IsWindowVisible(parent)) GetWindowRect(parent, &owner);
    else SystemParametersInfoA(SPI_GETWORKAREA, 0, &owner, 0);
    x = (owner.left + owner.right - ow) / 2; y = (owner.top + owner.bottom - oh) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    d->hwnd = CreateWindowExA(WS_EX_CONTROLPARENT, "PCDialog", title,
                              WS_POPUP | WS_CLIPCHILDREN, x, y, ow, oh, parent, NULL, w95_inst(), d);
    if (!d->hwnd) { free(d); return NULL; }
    SetWindowLongA(d->hwnd, GWL_USERDATA, (LONG)(size_t)d);
    w95_frame_attach(d->hwnd, &d->fr, W95_FRAME_DIALOG | W95_FRAME_CLOSE);
    SetWindowPos(d->hwnd, NULL, x, y, ow, oh, SWP_NOZORDER | SWP_FRAMECHANGED);
    return d;
}

static HWND add_ctl(Dlg *d, const char *cls, const char *text, DWORD style, DWORD ex, int id, int x, int y, int w, int h)
{
    HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, d->hwnd, (HMENU)(size_t)id, w95_inst(), NULL);
    if (c) SendMessageA(c, WM_SETFONT, (WPARAM)w95_font(W95_FONT_NORMAL), 0);
    return c;
}

HWND dlg_label(Dlg *d, int x, int y, int w, int h, const char *text) { return add_ctl(d, "STATIC", text, SS_LEFT, 0, -1, x, y, w, h); }

HWND dlg_label_id(Dlg *d, int id, int x, int y, int w, int h, const char *text, int right)
{
    return add_ctl(d, "STATIC", text, right ? SS_RIGHT : SS_LEFT, 0, id, x, y, w, h);
}

HWND dlg_edit(Dlg *d, int id, int x, int y, int w, int h, const char *text, int flags)
{
    DWORD st = WS_TABSTOP | ES_AUTOHSCROLL;
    if (flags & DLG_MULTILINE) st = WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
    if (flags & DLG_READONLY) st |= ES_READONLY;
    if ((flags & DLG_NUMBER) && !(flags & (DLG_SIGNED | DLG_DECIMAL))) st |= ES_NUMBER;
    return add_ctl(d, "EDIT", text, st, WS_EX_CLIENTEDGE, id, x, y, w, h);
}

HWND dlg_button(Dlg *d, int id, int x, int y, int w, int h, const char *label, int is_default)
{
    return add_ctl(d, "BUTTON", label, WS_TABSTOP | (is_default ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), 0, id, x, y, w, h);
}

HWND dlg_check(Dlg *d, int id, int x, int y, int w, int h, const char *label, int checked)
{
    HWND c = add_ctl(d, "BUTTON", label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, id, x, y, w, h);
    if (c && checked) SendMessageA(c, BM_SETCHECK, BST_CHECKED, 0);
    return c;
}

HWND dlg_radio(Dlg *d, int id, int x, int y, int w, int h, const char *label, int checked, int first)
{
    HWND c = add_ctl(d, "BUTTON", label, BS_AUTORADIOBUTTON | (first ? WS_GROUP | WS_TABSTOP : 0), 0, id, x, y, w, h);
    if (c && checked) SendMessageA(c, BM_SETCHECK, BST_CHECKED, 0);
    return c;
}

HWND dlg_combo(Dlg *d, int id, int x, int y, int w, int h, const char *const *items, int n, int sel)
{
    HWND c = add_ctl(d, "COMBOBOX", "", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, id, x, y, w, h + 160);
    int i;
    if (!c) return NULL;
    for (i = 0; i < n; i++) SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)items[i]);
    SendMessageA(c, CB_SETCURSEL, (WPARAM)sel, 0);
    return c;
}

HWND dlg_list(Dlg *d, int id, int x, int y, int w, int h)
{
    return add_ctl(d, "LISTBOX", "", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE, id, x, y, w, h);
}

HWND dlg_list_owner(Dlg *d, int id, int x, int y, int w, int h)
{
    return add_ctl(d, "LISTBOX", "", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED,
                   WS_EX_CLIENTEDGE, id, x, y, w, h);
}

HWND dlg_group(Dlg *d, int x, int y, int w, int h, const char *label)
{
    return add_ctl(d, "BUTTON", label, BS_GROUPBOX, 0, -1, x, y, w, h);
}

HWND dlg_custom(Dlg *d, int id, int x, int y, int w, int h, DlgPaintFn paint, DlgMouseFn mouse)
{
    HWND c;
    if (d->ncustom >= 16) return NULL;
    c = CreateWindowExA(0, "PCDlgCustom", "", WS_CHILD | WS_VISIBLE, x, y, w, h, d->hwnd, (HMENU)(size_t)id, w95_inst(), NULL);
    if (!c) return NULL;
    d->custom[d->ncustom].id = id; d->custom[d->ncustom].hwnd = c;
    d->custom[d->ncustom].paint = paint; d->custom[d->ncustom].mouse = mouse;
    d->ncustom++;
    return c;
}

HWND dlg_slider(Dlg *d, int id, int x, int y, int w, int min, int max, int val, int edit_id, int flags)
{
    HWND c;
    int k;
    if (d->nslider >= 24) return NULL;
    k = d->nslider++;
    c = CreateWindowExA(0, "PCSlider", "", WS_CHILD | WS_VISIBLE, x, y, w, (flags & (SLIDER_HUE_BAR | SLIDER_SAT_BAR | SLIDER_LIGHT_BAR)) ? 18 : 12,
                        d->hwnd, (HMENU)(size_t)id, w95_inst(), NULL);
    d->slider[k].id = id; d->slider[k].hwnd = c; d->slider[k].min = min; d->slider[k].max = max;
    d->slider[k].val = PC_CLAMP(val, min, max); d->slider[k].edit_id = edit_id; d->slider[k].flags = flags;
    return c;
}

HWND dlg_item(Dlg *d, int id) { return GetDlgItem(d->hwnd, id); }

int dlg_get_int(Dlg *d, int id)
{
    char b[64];
    dlg_get_text(d, id, b, sizeof b);
    return (int)pc_atol(b);
}

double dlg_get_double(Dlg *d, int id)
{
    char b[64];
    dlg_get_text(d, id, b, sizeof b);
    return pc_atof(b);
}

void dlg_set_int(Dlg *d, int id, int v)
{
    char b[32];
    pc_snprintf(b, sizeof b, "%d", v);
    dlg_set_text(d, id, b);
}

void dlg_set_double(Dlg *d, int id, double v, int dec)
{
    char b[32];
    pc_snprintf(b, sizeof b, "%.*f", dec, v);
    dlg_set_text(d, id, b);
}

void dlg_get_text(Dlg *d, int id, char *buf, int n)
{
    buf[0] = 0;
    GetDlgItemTextA(d->hwnd, id, buf, n);
}

void dlg_set_text(Dlg *d, int id, const char *s)
{
    d->busy++;
    SetDlgItemTextA(d->hwnd, id, s);
    d->busy--;
}

int dlg_get_check(Dlg *d, int id) { return SendDlgItemMessageA(d->hwnd, id, BM_GETCHECK, 0, 0) == BST_CHECKED; }
void dlg_set_check(Dlg *d, int id, int on) { SendDlgItemMessageA(d->hwnd, id, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0); }
int dlg_get_combo(Dlg *d, int id) { return (int)SendDlgItemMessageA(d->hwnd, id, CB_GETCURSEL, 0, 0); }
void dlg_set_combo(Dlg *d, int id, int sel) { d->busy++; SendDlgItemMessageA(d->hwnd, id, CB_SETCURSEL, (WPARAM)sel, 0); d->busy--; }
void dlg_enable(Dlg *d, int id, int on) { HWND h = GetDlgItem(d->hwnd, id); if (h) EnableWindow(h, on); }

void dlg_redraw(Dlg *d, int id)
{
    HWND h = GetDlgItem(d->hwnd, id);
    if (h) { InvalidateRect(h, NULL, FALSE); UpdateWindow(h); }
}

static int slider_index(Dlg *d, int id)
{
    int i;
    for (i = 0; i < d->nslider; i++) if (d->slider[i].id == id) return i;
    return -1;
}

int dlg_slider_get(Dlg *d, int id) { int k = slider_index(d, id); return k >= 0 ? d->slider[k].val : 0; }

void dlg_slider_set(Dlg *d, int id, int v)
{
    int k = slider_index(d, id);
    if (k < 0) return;
    v = PC_CLAMP(v, d->slider[k].min, d->slider[k].max);
    if (v == d->slider[k].val) return;
    d->slider[k].val = v;
    InvalidateRect(d->slider[k].hwnd, NULL, FALSE);
}

void dlg_end(Dlg *d, int result) { d->result = result; d->done = 1; }

int dlg_run(Dlg *d)
{
    MSG m;
    HWND owner = g_app.main;
    HWND first;
    int was_modal = g_app.modal;
    if (!d) return IDCANCEL;
    g_app.modal++;
    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(d->hwnd, SW_SHOW);
    first = GetNextDlgTabItem(d->hwnd, NULL, FALSE);
    if (first) SetFocus(first);
    while (!d->done && GetMessageA(&m, NULL, 0, 0)) {
        if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE && (m.hwnd == d->hwnd || IsChild(d->hwnd, m.hwnd))) {
            dlg_end(d, IDCANCEL);
            continue;
        }
        if (m.message == WM_KEYDOWN && m.wParam == VK_RETURN && (m.hwnd == d->hwnd || IsChild(d->hwnd, m.hwnd))) {
            char cls[16];
            GetClassNameA(m.hwnd, cls, sizeof cls);
            if (!(pc_stricmp(cls, "Edit") == 0 && (GetWindowLongA(m.hwnd, GWL_STYLE) & ES_MULTILINE))) {
                HWND f = GetFocus();
                int fid = f ? GetDlgCtrlID(f) : 0;
                char fc[16] = "";
                if (f) GetClassNameA(f, fc, sizeof fc);
                if (!pc_stricmp(fc, "Button") && (GetWindowLongA(f, GWL_STYLE) & 0x0F) <= BS_DEFPUSHBUTTON && fid > 0) {
                    SendMessageA(d->hwnd, WM_COMMAND, (WPARAM)fid, (LPARAM)f);
                } else {
                    /* Commit the edited field first so OK sees it. */
                    if (d->on_change && fid > 0) d->on_change(d, fid, DLG_CODE_CHANGE);
                    SendMessageA(d->hwnd, WM_COMMAND, IDOK, 0);
                }
                continue;
            }
        }
        if (IsDialogMessageA(d->hwnd, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    if (owner) EnableWindow(owner, TRUE);
    g_app.modal = was_modal;
    ShowWindow(d->hwnd, SW_HIDE);
    if (owner) SetActiveWindow(owner);
    return d->result;
}

void dlg_destroy(Dlg *d)
{
    if (!d) return;
    if (d->hwnd) DestroyWindow(d->hwnd);
    free(d);
}

static LRESULT CALLBACK dlg_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Dlg *d = (Dlg *)(size_t)GetWindowLongA(hwnd, GWL_USERDATA);
    LRESULT res;
    if (d && w95_frame_proc(hwnd, &d->fr, msg, wp, lp, &res)) return res;
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(hwnd, &r);
        w95_fill((HDC)wp, &r, W95_FACE);
        return 1;
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: case WM_CTLCOLORDLG: {
        static HBRUSH face;
        if (!face) face = CreateSolidBrush(W95_FACE);
        SetBkColor((HDC)wp, W95_FACE);
        SetTextColor((HDC)wp, W95_TEXT);
        return (LRESULT)face;
    }
    case WM_COMMAND:
        if (!d) break;
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
            if (d->on_change) d->on_change(d, LOWORD(wp), DLG_CODE_CLICK);
            if (!d->done) dlg_end(d, LOWORD(wp));
            return 0;
        }
        if (d->busy || !d->on_change) return 0;
        {
            int code = HIWORD(wp), id = LOWORD(wp);
            if (code == EN_CHANGE || code == CBN_SELCHANGE || code == LBN_SELCHANGE) d->on_change(d, id, DLG_CODE_CHANGE);
            else if (code == BN_CLICKED) d->on_change(d, id, DLG_CODE_CLICK);
            else if (code == LBN_DBLCLK) d->on_change(d, id, DLG_CODE_DBLCLK);
            else if (code == EN_KILLFOCUS) d->on_change(d, id, DLG_CODE_CHANGE);
        }
        return 0;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)lp)->itemHeight = 17;
        return TRUE;
    case WM_DRAWITEM:
        if (d && d->draw_item) { d->draw_item(d, (const DRAWITEMSTRUCT *)lp); return TRUE; }
        break;
    case WM_CLOSE:
        if (d) dlg_end(d, IDCANCEL);
        return 0;
    case WM_HELP:
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------------- custom controls ---------------- */

static Dlg *owner_dlg(HWND child) { return (Dlg *)(size_t)GetWindowLongA(GetParent(child), GWL_USERDATA); }

static LRESULT CALLBACK custom_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Dlg *d = owner_dlg(hwnd);
    int i, id = GetDlgCtrlID(hwnd);
    if (!d) return DefWindowProcA(hwnd, msg, wp, lp);
    for (i = 0; i < d->ncustom; i++) if (d->custom[i].hwnd == hwnd) break;
    if (i == d->ncustom) return DefWindowProcA(hwnd, msg, wp, lp);
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
        if (d->custom[i].paint) d->custom[i].paint(d, id, bdc, &r);
        w95_buf_end(&b);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_MOUSEMOVE: case WM_LBUTTONDBLCLK: case WM_RBUTTONDOWN:
        if (msg == WM_LBUTTONDOWN) SetCapture(hwnd);
        if (msg == WM_LBUTTONUP) ReleaseCapture();
        if (d->custom[i].mouse) d->custom[i].mouse(d, id, msg, (short)LOWORD(lp), (short)HIWORD(lp), wp);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void draw_slider(HDC dc, const RECT *r, int min, int max, int val, int flags, int disabled)
{
    int w = r->right - r->left, x0 = 4, x1 = w - 5, tx;
    int bar = (flags & (SLIDER_HUE_BAR | SLIDER_SAT_BAR | SLIDER_LIGHT_BAR)) != 0;
    int ly = bar ? 1 : 2;
    RECT t;
    w95_fill(dc, r, W95_FACE);
    if (bar) {
        int x;
        for (x = x0; x <= x1; x++) {
            int rr, gg, bb;
            float f = (float)(x - x0) / PC_MAX(1, x1 - x0);
            if (flags & SLIDER_HUE_BAR) pc_hsb_to_rgb(f * 360, 1, 1, &rr, &gg, &bb);
            else if (flags & SLIDER_SAT_BAR) { int v = (int)(f * 255); rr = 128 + (v - 128) / 2; gg = 128 - (v - 128) / 4; bb = 128 - (v - 128) / 4; rr = PC_CLAMP(rr, 0, 255); }
            else { int v = (int)(f * 255); rr = gg = bb = v; }
            w95_vline(dc, x, ly, ly + 6, RGB(rr, gg, bb));
        }
        SetRect(&t, x0 - 1, ly - 1, x1 + 2, ly + 7);
        w95_rect(dc, t.left, t.top, t.right, t.bottom, W95_SHADOW);
        ly += 7;
    } else {
        /* Photoshop's thin sunken slider line. */
        SetRect(&t, x0, ly, x1 + 1, ly + 3);
        w95_thin_sunken(dc, &t);
        ly += 3;
    }
    if (max > min) tx = x0 + (int)((double)(val - min) * (x1 - x0) / (max - min) + 0.5);
    else tx = x0;
    if (flags & SLIDER_CENTER_ZERO) { int cz = x0 + (int)((double)(-min) * (x1 - x0) / PC_MAX(1, max - min)); w95_vline(dc, cz, ly, ly + 2, W95_SHADOW); }
    {
        /* Triangle thumb pointing up: black outline, face fill. */
        int k;
        COLORREF c = disabled ? W95_SHADOW : W95_TEXT;
        for (k = 0; k < 5; k++) {
            w95_hline(dc, tx - k, tx + k + 1, ly + 1 + k, k == 4 ? c : W95_FACE);
            w95_pixel(dc, tx - k, ly + 1 + k, c);
            w95_pixel(dc, tx + k, ly + 1 + k, c);
        }
    }
}

static LRESULT CALLBACK slider_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Dlg *d = owner_dlg(hwnd);
    int k = -1, i;
    if (d) for (i = 0; i < d->nslider; i++) if (d->slider[i].hwnd == hwnd) k = i;
    if (k < 0) return DefWindowProcA(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r;
        W95Buf b;
        HDC bdc;
        GetClientRect(hwnd, &r);
        bdc = w95_buf_begin(&b, dc, &r);
        draw_slider(bdc, &r, d->slider[k].min, d->slider[k].max, d->slider[k].val, d->slider[k].flags, !IsWindowEnabled(hwnd));
        w95_buf_end(&b);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: case WM_MOUSEMOVE: {
        RECT r;
        int x = (short)LOWORD(lp), v, x0 = 4, x1;
        if (msg == WM_MOUSEMOVE && !d->slider[k].drag) return 0;
        if (msg == WM_LBUTTONDOWN) { d->slider[k].drag = 1; SetCapture(hwnd); }
        GetClientRect(hwnd, &r);
        x1 = r.right - 5;
        v = d->slider[k].min + (int)((double)(x - x0) * (d->slider[k].max - d->slider[k].min) / PC_MAX(1, x1 - x0) + 0.5);
        v = PC_CLAMP(v, d->slider[k].min, d->slider[k].max);
        if (v != d->slider[k].val) {
            d->slider[k].val = v;
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            if (d->slider[k].edit_id) dlg_set_int(d, d->slider[k].edit_id, v);
            if (d->on_change) d->on_change(d, d->slider[k].id, DLG_CODE_SLIDER);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (d->slider[k].drag) {
            d->slider[k].drag = 0;
            ReleaseCapture();
            if (d->on_change) d->on_change(d, d->slider[k].id, DLG_CODE_CLICK);
        }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------------- preview ---------------- */

void preview_begin(DlgPreview *p, PcDoc *doc)
{
    p->doc = doc;
    p->active = doc != NULL;
    if (doc) pc_state_copy(&p->saved, &doc->s);
}

void preview_restore(DlgPreview *p)
{
    if (!p->active) return;
    pc_state_release(&p->doc->s);
    pc_state_copy(&p->doc->s, &p->saved);
    pc_doc_touch(p->doc);
}

void preview_end(DlgPreview *p, int keep)
{
    if (!p->active) return;
    if (!keep) preview_restore(p);
    pc_state_release(&p->saved);
    p->active = 0;
    app_refresh(R_CANVAS | R_NAV);
}

/* ---------------- popup slider ---------------- */

typedef struct { int min, max, val; void (*live)(int, void *); void *u; int done; } PopSl;
static PopSl *g_popsl;

static LRESULT CALLBACK popsl_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PopSl *s = g_popsl;
    if (!s) return DefWindowProcA(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r, sr;
        GetClientRect(hwnd, &r);
        w95_fill(dc, &r, W95_FACE);
        w95_raised(dc, &r);
        sr = r; InflateRect(&sr, -3, -3);
        draw_slider(dc, &sr, s->min, s->max, s->val, 0, 0);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: case WM_LBUTTONDOWN: {
        RECT r;
        int x = (short)LOWORD(lp) - 3, v;
        if (msg == WM_MOUSEMOVE && !(wp & MK_LBUTTON)) return 0;
        GetClientRect(hwnd, &r);
        v = s->min + (int)((double)(x - 4) * (s->max - s->min) / PC_MAX(1, r.right - 6 - 9) + 0.5);
        v = PC_CLAMP(v, s->min, s->max);
        if (v != s->val) { s->val = v; InvalidateRect(hwnd, NULL, FALSE); if (s->live) s->live(v, s->u); }
        return 0;
    }
    case WM_LBUTTONUP: s->done = 1; return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int popup_slider(HWND owner, int sx, int sy, int min, int max, int *value, void (*live)(int, void *), void *u)
{
    static int reg;
    PopSl s;
    HWND w;
    MSG m;
    if (!reg) { w95_register_class("PCPopSlider", popsl_proc, 0, NULL); reg = 1; }
    s.min = min; s.max = max; s.val = PC_CLAMP(*value, min, max); s.live = live; s.u = u; s.done = 0;
    g_popsl = &s;
    w = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "PCPopSlider", "", WS_POPUP, sx - 60, sy, 130, 20, owner, NULL, w95_inst(), NULL);
    if (!w) { g_popsl = NULL; return 0; }
    ShowWindow(w, SW_SHOWNOACTIVATE);
    UpdateWindow(w);
    SetCapture(w);
    while (!s.done && GetMessageA(&m, NULL, 0, 0)) {
        if (m.message == WM_KEYDOWN && (m.wParam == VK_ESCAPE || m.wParam == VK_RETURN)) { s.done = 1; continue; }
        if (m.message == WM_LBUTTONDOWN && m.hwnd != w) {
            POINT p; RECT wr;
            GetCursorPos(&p); GetWindowRect(w, &wr);
            if (!PtInRect(&wr, p)) { s.done = 1; continue; }
        }
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    ReleaseCapture();
    DestroyWindow(w);
    g_popsl = NULL;
    *value = s.val;
    return 1;
}
