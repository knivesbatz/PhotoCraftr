/*
 * Dialogs, laid out after Photoshop 5.0 for Windows: OK / Cancel stacked
 * at the top right (60x20, 28 px apart), Load/Save/Auto below them, a
 * Preview check box, MS Sans Serif 8 labels, triangle sliders and 20 px
 * edit fields.
 */
#include "dlg.h"

#define BW 64
#define BH 22
static int bx(int w) { return w - BW - 10; }

static void ok_cancel(Dlg *d, int w)
{
    dlg_button(d, IDOK, bx(w), 10, BW, BH, "OK", 1);
    dlg_button(d, IDCANCEL, bx(w), 38, BW, BH, "Cancel", 0);
}

static const char *const k_blends[PC_BLEND_COUNT] = {
    "Normal", "Dissolve", "Multiply", "Screen", "Overlay", "Soft Light", "Hard Light", "Color Dodge",
    "Color Burn", "Darken", "Lighten", "Difference", "Exclusion", "Hue", "Saturation", "Color", "Luminosity"
};

/* ---------------- message box ---------------- */

static void yesno_change(Dlg *d, int id, int code)
{
    if ((id == IDYES || id == IDNO) && code == DLG_CODE_CLICK) dlg_end(d, id);
}


typedef struct { int icon; const char *text; } MsgCtx;

static void msg_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    MsgCtx *m = (MsgCtx *)d->user;
    static const int icons[4] = { PC_ICON_INFO, PC_ICON_WARNING, PC_ICON_ERROR, PC_ICON_QUESTION };
    RECT t = *r;
    (void)id;
    w95_icon(dc, icons[PC_CLAMP(m->icon, 0, 3)], 0, 0);
    t.left = 44;
    w95_text(dc, W95_FONT_NORMAL, m->text, &t, DT_LEFT | DT_WORDBREAK, W95_TEXT);
}

int dlg_message(const char *title, const char *text, int icon, int buttons)
{
    Dlg *d;
    MsgCtx m;
    int w = 360, th, rc, nb = buttons == 0 ? 1 : buttons == 1 ? 2 : 3, x;
    RECT tr;
    HDC dc = GetDC(NULL);
    HFONT of = (HFONT)SelectObject(dc, w95_font(W95_FONT_NORMAL));
    SetRect(&tr, 0, 0, w - 70, 1000);
    DrawTextA(dc, text, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
    SelectObject(dc, of);
    ReleaseDC(NULL, dc);
    th = PC_MAX(36, tr.bottom);
    if (th > 420) th = 420;
    m.icon = icon; m.text = text;
    d = dlg_create(title, w, th + 64, yesno_change, &m);
    if (!d) return MessageBoxA(g_app.main, text, title, MB_OK) == IDOK ? 1 : 0;
    dlg_custom(d, 900, 12, 14, w - 24, th + 4, msg_paint, NULL);
    x = (w - nb * 82 + 7) / 2;
    if (buttons == 0) dlg_button(d, IDOK, x, th + 30, 75, 23, "OK", 1);
    else if (buttons == 1) { dlg_button(d, IDOK, x, th + 30, 75, 23, "OK", 1); dlg_button(d, IDCANCEL, x + 82, th + 30, 75, 23, "Cancel", 0); }
    else {
        dlg_button(d, IDYES, x, th + 30, 75, 23, "&Yes", 1);
        dlg_button(d, IDNO, x + 82, th + 30, 75, 23, "&No", 0);
        dlg_button(d, IDCANCEL, x + 164, th + 30, 75, 23, "Cancel", 0);
    }
    MessageBeep(icon == MB_ICON_ERR_ ? MB_ICONHAND : icon == MB_ICON_WARN_ ? MB_ICONEXCLAMATION : MB_ICONASTERISK);
    rc = dlg_run(d);
    if (rc == 0) rc = IDCANCEL;
    dlg_destroy(d);
    if (buttons == 2) return rc == IDYES ? 1 : rc == IDNO ? 0 : -1;
    return rc == IDOK;
}

/* ---------------- simple inputs ---------------- */

int dlg_text_input(const char *title, const char *label, char *buf, int n)
{
    Dlg *d = dlg_create(title, 330, 76, NULL, NULL);
    int rc;
    if (!d) return 0;
    dlg_label(d, 12, 16, 90, 16, label);
    dlg_edit(d, 100, 100, 12, 140, 20, buf, 0);
    ok_cancel(d, 330);
    rc = dlg_run(d);
    if (rc == IDOK) dlg_get_text(d, 100, buf, n);
    dlg_destroy(d);
    return rc == IDOK;
}

int dlg_action_name(char *name, int n) { return dlg_text_input("New Action", "Name:", name, n); }

/* ---------------- New ---------------- */

static const char *const k_units[] = { "pixels", "inches", "cm" };
typedef struct { double dpi; int unit; int w, h; } NewCtx;

static void new_change(Dlg *d, int id, int code)
{
    NewCtx *c = (NewCtx *)d->user;
    char b[48];
    if (id == 101 || id == 102 || id == 103 || id == 104 || id == 105) {
        double f = c->unit == 0 ? 1 : c->unit == 1 ? c->dpi : c->dpi / 2.54;
        c->dpi = PC_MAX(1.0, dlg_get_double(d, 105));
        if (id == 103) {
            /* unit change: re-express the size */
            int nu = dlg_get_combo(d, 103);
            double nf = nu == 0 ? 1 : nu == 1 ? c->dpi : c->dpi / 2.54;
            c->unit = nu;
            dlg_set_double(d, 101, c->w / nf, nu ? 2 : 0);
            dlg_set_double(d, 102, c->h / nf, nu ? 2 : 0);
        } else if (id == 101 || id == 102) {
            c->w = PC_CLAMP((int)(dlg_get_double(d, 101) * f + 0.5), 1, 30000);
            c->h = PC_CLAMP((int)(dlg_get_double(d, 102) * f + 0.5), 1, 30000);
        }
        {
            double mb = (double)c->w * c->h * (dlg_get_combo(d, 106) == 1 ? 1 : 3) / 1048576.0;
            pc_snprintf(b, sizeof b, "Image Size: %.1fM", mb);
            dlg_set_text(d, 110, b);
        }
    }
    (void)code;
}

int dlg_new(int *w, int *h, double *dpi, int *mode, int *fill, char *name, int namen)
{
    static const char *const modes[] = { "RGB Color", "Grayscale" };
    Dlg *d;
    NewCtx c;
    int rc, W = 380;
    c.dpi = *dpi; c.unit = 0; c.w = *w; c.h = *h;
    d = dlg_create("New", W, 250, new_change, &c);
    if (!d) return 0;
    dlg_label(d, 12, 15, 40, 16, "Name:");
    dlg_edit(d, 100, 60, 12, 220, 20, name[0] ? name : "Untitled", 0);
    dlg_group(d, 10, 42, 285, 118, "");
    dlg_label_id(d, 110, 18, 42, 150, 14, "", 0);
    dlg_label(d, 22, 66, 60, 16, "Width:");
    dlg_edit(d, 101, 90, 63, 70, 20, "", DLG_NUMBER | DLG_DECIMAL);
    dlg_label(d, 22, 92, 60, 16, "Height:");
    dlg_edit(d, 102, 90, 89, 70, 20, "", DLG_NUMBER | DLG_DECIMAL);
    dlg_combo(d, 103, 168, 63, 90, 20, k_units, 3, 0);
    dlg_label(d, 22, 118, 66, 16, "Resolution:");
    dlg_edit(d, 105, 90, 115, 70, 20, "", DLG_NUMBER | DLG_DECIMAL);
    dlg_label(d, 168, 118, 100, 16, "pixels/inch");
    dlg_label(d, 22, 140, 40, 16, "Mode:");
    dlg_combo(d, 106, 90, 137, 120, 20, modes, 2, *mode == PC_MODE_GRAY);
    dlg_group(d, 10, 166, 285, 74, "Contents");
    dlg_radio(d, 120, 22, 184, 120, 16, "&White", *fill == 0, 1);
    dlg_radio(d, 121, 22, 202, 160, 16, "&Background Color", *fill == 1, 0);
    dlg_radio(d, 122, 22, 220, 120, 16, "&Transparent", *fill == 2, 0);
    ok_cancel(d, W);
    dlg_set_int(d, 101, *w); dlg_set_int(d, 102, *h); dlg_set_double(d, 105, *dpi, 0);
    new_change(d, 101, 0);
    rc = dlg_run(d);
    if (rc == IDOK) {
        new_change(d, 101, 0);
        *w = c.w; *h = c.h; *dpi = c.dpi;
        *mode = dlg_get_combo(d, 106) == 1 ? PC_MODE_GRAY : PC_MODE_RGB;
        *fill = dlg_get_check(d, 121) ? 1 : dlg_get_check(d, 122) ? 2 : 0;
        dlg_get_text(d, 100, name, namen);
        if (!strncmp(name, "Untitled", 8)) name[0] = 0;
    }
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- Image Size ---------------- */

typedef struct { int ow, oh; double odpi; int lock; int busy; } SizeCtx;

static void imgsize_change(Dlg *d, int id, int code)
{
    SizeCtx *c = (SizeCtx *)d->user;
    char b[64];
    if (c->busy) return;
    c->busy = 1;
    if (id == 103) c->lock = dlg_get_check(d, 103);
    if (dlg_get_check(d, 104) == 0) {
        /* Resample off: pixel size is fixed; width/height in inches follow resolution. */
        double dpi = PC_MAX(1.0, dlg_get_double(d, 107));
        dlg_set_int(d, 100, c->ow); dlg_set_int(d, 101, c->oh);
        dlg_set_double(d, 105, c->ow / dpi, 3); dlg_set_double(d, 106, c->oh / dpi, 3);
    } else if (id == 100 && c->lock) {
        int w = dlg_get_int(d, 100);
        dlg_set_int(d, 101, PC_MAX(1, (int)((double)w * c->oh / c->ow + 0.5)));
    } else if (id == 101 && c->lock) {
        int h = dlg_get_int(d, 101);
        dlg_set_int(d, 100, PC_MAX(1, (int)((double)h * c->ow / c->oh + 0.5)));
    } else if (id == 105 || id == 106) {
        double dpi = PC_MAX(1.0, dlg_get_double(d, 107)), win = dlg_get_double(d, 105), hin = dlg_get_double(d, 106);
        if (id == 105) { dlg_set_int(d, 100, PC_MAX(1, (int)(win * dpi + 0.5))); if (c->lock) { hin = win * c->oh / c->ow; dlg_set_double(d, 106, hin, 3); dlg_set_int(d, 101, PC_MAX(1, (int)(hin * dpi + 0.5))); } }
        else { dlg_set_int(d, 101, PC_MAX(1, (int)(hin * dpi + 0.5))); if (c->lock) { win = hin * c->ow / c->oh; dlg_set_double(d, 105, win, 3); dlg_set_int(d, 100, PC_MAX(1, (int)(win * dpi + 0.5))); } }
    }
    if (id != 105 && id != 106 && dlg_get_check(d, 104)) {
        double dpi = PC_MAX(1.0, dlg_get_double(d, 107));
        dlg_set_double(d, 105, dlg_get_int(d, 100) / dpi, 3);
        dlg_set_double(d, 106, dlg_get_int(d, 101) / dpi, 3);
    }
    dlg_enable(d, 108, dlg_get_check(d, 104));
    {
        double mb = (double)PC_MAX(1, dlg_get_int(d, 100)) * PC_MAX(1, dlg_get_int(d, 101)) * 3 / 1048576.0;
        pc_snprintf(b, sizeof b, "Pixel Dimensions: %.1fM (was %.1fM)", mb, (double)c->ow * c->oh * 3 / 1048576.0);
        dlg_set_text(d, 110, b);
    }
    c->busy = 0;
    (void)code;
}

int dlg_image_size(PcDoc *doc, int *w, int *h, double *dpi, int *method)
{
    static const char *const methods[] = { "Nearest Neighbor", "Bilinear", "Bicubic" };
    Dlg *d;
    SizeCtx c;
    int rc, W = 390;
    c.ow = doc->s.w; c.oh = doc->s.h; c.odpi = doc->s.dpi; c.lock = 1; c.busy = 0;
    d = dlg_create("Image Size", W, 280, imgsize_change, &c);
    if (!d) return 0;
    dlg_group(d, 10, 8, 290, 82, "");
    dlg_label_id(d, 110, 18, 8, 250, 14, "Pixel Dimensions:", 0);
    dlg_label(d, 24, 32, 60, 16, "Width:");
    dlg_edit(d, 100, 90, 29, 70, 20, "", DLG_NUMBER);
    dlg_label(d, 166, 32, 50, 16, "pixels");
    dlg_label(d, 24, 58, 60, 16, "Height:");
    dlg_edit(d, 101, 90, 55, 70, 20, "", DLG_NUMBER);
    dlg_label(d, 166, 58, 50, 16, "pixels");
    dlg_group(d, 10, 98, 290, 110, "Print Size:");
    dlg_label(d, 24, 122, 60, 16, "Width:");
    dlg_edit(d, 105, 90, 119, 70, 20, "", DLG_NUMBER | DLG_DECIMAL);
    dlg_label(d, 166, 122, 50, 16, "inches");
    dlg_label(d, 24, 148, 60, 16, "Height:");
    dlg_edit(d, 106, 90, 145, 70, 20, "", DLG_NUMBER | DLG_DECIMAL);
    dlg_label(d, 166, 148, 50, 16, "inches");
    dlg_label(d, 24, 176, 64, 16, "Resolution:");
    dlg_edit(d, 107, 90, 173, 70, 20, "", DLG_NUMBER | DLG_DECIMAL);
    dlg_label(d, 166, 176, 80, 16, "pixels/inch");
    dlg_check(d, 103, 14, 218, 160, 16, "&Constrain Proportions", 1);
    dlg_check(d, 104, 14, 238, 120, 16, "&Resample Image:", 1);
    dlg_combo(d, 108, 136, 235, 140, 20, methods, 3, *method);
    ok_cancel(d, W);
    dlg_button(d, 120, bx(W), 76, BW, BH, "&Auto...", 0);
    dlg_enable(d, 120, 0);
    dlg_set_int(d, 100, c.ow); dlg_set_int(d, 101, c.oh); dlg_set_double(d, 107, c.odpi, 0);
    imgsize_change(d, 0, 0);
    rc = dlg_run(d);
    if (rc == IDOK) {
        *w = PC_CLAMP(dlg_get_int(d, 100), 1, 30000); *h = PC_CLAMP(dlg_get_int(d, 101), 1, 30000);
        *dpi = PC_MAX(1.0, dlg_get_double(d, 107));
        *method = dlg_get_combo(d, 108);
        if (!dlg_get_check(d, 104)) { *w = c.ow; *h = c.oh; }
        if ((size_t)*w * *h > PC_MAX_PIXELS) { app_error("The image would be too large (maximum 100 million pixels)."); rc = IDCANCEL; }
    }
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- Canvas Size ---------------- */

static int g_anchor = 4;
static void anchor_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    int k;
    (void)d; (void)id;
    for (k = 0; k < 9; k++) {
        RECT c;
        int x = r->left + (k % 3) * 22, y = r->top + (k / 3) * 22;
        SetRect(&c, x, y, x + 21, y + 21);
        w95_fill(dc, &c, W95_FACE);
        if (k == g_anchor) w95_thin_sunken(dc, &c); else w95_thin_raised(dc, &c);
        if (k == g_anchor) { RECT q = c; InflateRect(&q, -6, -6); w95_fill(dc, &q, W95_SHADOW); }
        else {
            /* Arrows pointing away from the anchor. */
            int dx = (k % 3) - (g_anchor % 3), dy = (k / 3) - (g_anchor / 3);
            if (abs(dx) <= 1 && abs(dy) <= 1) {
                int cx = x + 10, cy = y + 10;
                if (dx == 0) w95_draw_arrow(dc, cx, cy, dy < 0 ? 0 : 1, 4, W95_TEXT);
                else if (dy == 0) w95_draw_arrow(dc, cx, cy, dx < 0 ? 2 : 3, 4, W95_TEXT);
                else { w95_pixel(dc, cx, cy, W95_TEXT); w95_pixel(dc, cx + dx, cy + dy, W95_TEXT); w95_pixel(dc, cx + 2 * dx, cy + 2 * dy, W95_TEXT); }
            }
        }
    }
}

static void anchor_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    (void)wp;
    if (msg != WM_LBUTTONDOWN) return;
    if (x < 66 && y < 66) { g_anchor = (y / 22) * 3 + x / 22; dlg_redraw(d, id); }
}

int dlg_canvas_size(PcDoc *doc, int *w, int *h, int *anchor)
{
    Dlg *d;
    int rc, W = 340;
    char b[64];
    d = dlg_create("Canvas Size", W, 210, NULL, NULL);
    if (!d) return 0;
    pc_snprintf(b, sizeof b, "Current Size: %.1fM", (double)doc->s.w * doc->s.h * 3 / 1048576.0);
    dlg_group(d, 10, 8, 240, 60, b);
    pc_snprintf(b, sizeof b, "Width:  %d pixels", doc->s.w); dlg_label(d, 24, 28, 200, 16, b);
    pc_snprintf(b, sizeof b, "Height: %d pixels", doc->s.h); dlg_label(d, 24, 46, 200, 16, b);
    dlg_group(d, 10, 76, 240, 70, "New Size:");
    dlg_label(d, 24, 98, 50, 16, "Width:");
    dlg_edit(d, 100, 76, 95, 70, 20, "", DLG_NUMBER);
    dlg_label(d, 152, 98, 50, 16, "pixels");
    dlg_label(d, 24, 122, 50, 16, "Height:");
    dlg_edit(d, 101, 76, 119, 70, 20, "", DLG_NUMBER);
    dlg_label(d, 152, 122, 50, 16, "pixels");
    dlg_label(d, 24, 160, 50, 16, "Anchor:");
    g_anchor = *anchor;
    dlg_custom(d, 102, 80, 152, 67, 67, anchor_paint, anchor_mouse);
    ok_cancel(d, W);
    dlg_set_int(d, 100, doc->s.w); dlg_set_int(d, 101, doc->s.h);
    rc = dlg_run(d);
    if (rc == IDOK) {
        *w = PC_CLAMP(dlg_get_int(d, 100), 1, 30000); *h = PC_CLAMP(dlg_get_int(d, 101), 1, 30000); *anchor = g_anchor;
        if ((size_t)*w * *h > PC_MAX_PIXELS) { app_error("The canvas would be too large."); rc = IDCANCEL; }
    }
    dlg_destroy(d);
    return rc == IDOK;
}

int dlg_rotate(double *deg)
{
    Dlg *d = dlg_create("Rotate Canvas", 300, 90, NULL, NULL);
    int rc;
    if (!d) return 0;
    dlg_label(d, 14, 18, 50, 16, "Angle:");
    dlg_edit(d, 100, 64, 15, 60, 20, "0", DLG_NUMBER | DLG_DECIMAL | DLG_SIGNED);
    dlg_radio(d, 101, 134, 12, 80, 16, "\xB0" "CW", 1, 1);
    dlg_radio(d, 102, 134, 32, 80, 16, "\xB0" "CCW", 0, 0);
    ok_cancel(d, 300);
    rc = dlg_run(d);
    if (rc == IDOK) { *deg = dlg_get_double(d, 100); if (dlg_get_check(d, 102)) *deg = -*deg; }
    dlg_destroy(d);
    return rc == IDOK && *deg != 0;
}

/* ---------------- adjustment previews ---------------- */

/* While dlg_adjust_layer runs, previews set the active adjustment layer's
 * parameters (the compositor shows them) instead of changing pixels. */
static int g_adj_layer_mode;

static void preview_adjust(PcDoc *doc, const PcAdjust *a)
{
    PcLayer *l = pc_doc_active(doc);
    if (g_adj_layer_mode && l && l->kind == PC_LAYER_ADJUST) { l->adj = *a; pc_doc_touch(doc); }
    else if (!g_adj_layer_mode) pc_adjust_apply(doc, a);
}

static void preview_levels(PcDoc *doc, const PcLevels *lv) { PcAdjust a; pc_adjust_default(&a, PC_ADJ_LEVELS); a.levels = *lv; preview_adjust(doc, &a); }
static void preview_curves(PcDoc *doc, const PcCurves *cv) { PcAdjust a; pc_adjust_default(&a, PC_ADJ_CURVES); a.curves = *cv; preview_adjust(doc, &a); }
static void preview_huesat(PcDoc *doc, const PcHueSat *hs) { PcAdjust a; pc_adjust_default(&a, PC_ADJ_HUE_SAT); a.huesat = *hs; preview_adjust(doc, &a); }
static void preview_balance(PcDoc *doc, const PcColorBalance *cb) { PcAdjust a; pc_adjust_default(&a, PC_ADJ_COLOR_BALANCE); a.balance = *cb; preview_adjust(doc, &a); }

static void preview_value(PcDoc *doc, int type, int v1, int v2)
{
    PcAdjust a;
    pc_adjust_default(&a, type);
    a.brightness = v1; a.contrast = v2; a.threshold = v1; a.posterize = v1;
    preview_adjust(doc, &a);
}

/* ---------------- Levels ---------------- */

typedef struct { PcDoc *doc; PcLevels lv; unsigned hist[4][256]; int ch; DlgPreview pv; int drag; } LevelsCtx;

static void levels_apply_preview(Dlg *d)
{
    LevelsCtx *c = (LevelsCtx *)d->user;
    if (!dlg_get_check(d, 150)) { preview_restore(&c->pv); app_refresh(R_CANVAS); return; }
    preview_restore(&c->pv);
    preview_levels(c->doc, &c->lv);
    app_refresh(R_CANVAS);
}

static void levels_sync_fields(Dlg *d)
{
    LevelsCtx *c = (LevelsCtx *)d->user;
    int k = c->ch;
    dlg_set_int(d, 101, c->lv.in_black[k]);
    dlg_set_double(d, 102, c->lv.gamma[k], 2);
    dlg_set_int(d, 103, c->lv.in_white[k]);
    dlg_set_int(d, 104, c->lv.out_black[k]);
    dlg_set_int(d, 105, c->lv.out_white[k]);
    dlg_redraw(d, 120); dlg_redraw(d, 121);
}

static void tri(HDC dc, int x, int y, COLORREF fill)
{
    int k;
    for (k = 0; k < 6; k++) {
        w95_hline(dc, x - k, x + k + 1, y + k, k == 5 ? W95_TEXT : fill);
        w95_pixel(dc, x - k, y + k, W95_TEXT); w95_pixel(dc, x + k, y + k, W95_TEXT);
    }
}

static void levels_paint_hist(Dlg *d, int id, HDC dc, const RECT *r)
{
    LevelsCtx *c = (LevelsCtx *)d->user;
    unsigned mx = 1;
    int v, k = c->ch, hh = r->bottom - r->top - 14;
    RECT box;
    (void)id;
    SetRect(&box, r->left, r->top, r->left + 258, r->top + hh);
    w95_fill(dc, &box, W95_WINDOW);
    w95_rect(dc, box.left, box.top, box.right, box.bottom, W95_DKSHADOW);
    for (v = 0; v < 256; v++) if (c->hist[k][v] > mx) mx = c->hist[k][v];
    for (v = 0; v < 256; v++) {
        int bh = (int)((double)c->hist[k][v] * (hh - 4) / mx);
        if (bh) w95_vline(dc, box.left + 1 + v, box.bottom - 1 - bh, box.bottom - 1, W95_TEXT);
    }
    {
        int g = c->lv.in_black[k] + (int)((c->lv.in_white[k] - c->lv.in_black[k]) * pow(0.5, c->lv.gamma[k]));
        tri(dc, box.left + 1 + c->lv.in_black[k], box.bottom + 1, W95_TEXT);
        tri(dc, box.left + 1 + g, box.bottom + 1, W95_SHADOW);
        tri(dc, box.left + 1 + c->lv.in_white[k], box.bottom + 1, W95_WINDOW);
    }
}

static void levels_paint_out(Dlg *d, int id, HDC dc, const RECT *r)
{
    LevelsCtx *c = (LevelsCtx *)d->user;
    int v, k = c->ch;
    (void)id;
    for (v = 0; v < 256; v++) {
        COLORREF col = k == 1 ? RGB(v, 0, 0) : k == 2 ? RGB(0, v, 0) : k == 3 ? RGB(0, 0, v) : RGB(v, v, v);
        w95_vline(dc, r->left + 1 + v, r->top, r->top + 8, col);
    }
    w95_rect(dc, r->left, r->top - 1, r->left + 258, r->top + 9, W95_DKSHADOW);
    tri(dc, r->left + 1 + c->lv.out_black[k], r->top + 10, W95_TEXT);
    tri(dc, r->left + 1 + c->lv.out_white[k], r->top + 10, W95_WINDOW);
}

static void levels_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    LevelsCtx *c = (LevelsCtx *)d->user;
    int k = c->ch, v = PC_CLAMP(x - 1, 0, 255);
    (void)wp; (void)y;
    if (msg == WM_LBUTTONDOWN) {
        if (id == 120) {
            int g = c->lv.in_black[k] + (int)((c->lv.in_white[k] - c->lv.in_black[k]) * pow(0.5, c->lv.gamma[k]));
            int db = abs(v - c->lv.in_black[k]), dg = abs(v - g), dw = abs(v - c->lv.in_white[k]);
            c->drag = db <= dg && db <= dw ? 1 : dw <= dg ? 3 : 2;
        } else {
            c->drag = abs(v - c->lv.out_black[k]) <= abs(v - c->lv.out_white[k]) ? 4 : 5;
        }
    }
    if ((msg == WM_LBUTTONDOWN || msg == WM_MOUSEMOVE) && c->drag) {
        if (msg == WM_MOUSEMOVE && !(wp & MK_LBUTTON)) return;
        switch (c->drag) {
        case 1: c->lv.in_black[k] = PC_MIN(v, c->lv.in_white[k] - 2); break;
        case 3: c->lv.in_white[k] = PC_MAX(v, c->lv.in_black[k] + 2); break;
        case 2: {
            double t = (double)(v - c->lv.in_black[k]) / PC_MAX(1, c->lv.in_white[k] - c->lv.in_black[k]);
            t = PC_CLAMP(t, 0.01, 0.99);
            c->lv.gamma[k] = (float)PC_CLAMP(log(t) / log(0.5), 0.1, 9.99);
            break;
        }
        case 4: c->lv.out_black[k] = v; break;
        case 5: c->lv.out_white[k] = v; break;
        }
        levels_sync_fields(d);
    }
    if (msg == WM_LBUTTONUP) { c->drag = 0; levels_apply_preview(d); }
}

static void levels_change(Dlg *d, int id, int code)
{
    LevelsCtx *c = (LevelsCtx *)d->user;
    int k = c->ch;
    if (id == 100 && code == DLG_CODE_CHANGE) { c->ch = PC_CLAMP(dlg_get_combo(d, 100), 0, 3); levels_sync_fields(d); return; }
    if (id >= 101 && id <= 105 && code == DLG_CODE_CHANGE) {
        c->lv.in_black[k] = PC_CLAMP(dlg_get_int(d, 101), 0, 253);
        c->lv.gamma[k] = (float)PC_CLAMP(dlg_get_double(d, 102), 0.1, 9.99);
        c->lv.in_white[k] = PC_CLAMP(dlg_get_int(d, 103), c->lv.in_black[k] + 2, 255);
        c->lv.out_black[k] = PC_CLAMP(dlg_get_int(d, 104), 0, 255);
        c->lv.out_white[k] = PC_CLAMP(dlg_get_int(d, 105), 0, 255);
        dlg_redraw(d, 120); dlg_redraw(d, 121);
        levels_apply_preview(d);            /* typed values preview too */
        return;
    }
    if (id == 130 && code == DLG_CODE_CLICK) {
        /* Auto: per-channel clipping like Image > Adjust > Auto Levels. */
        int ch, v;
        pc_levels_default(&c->lv);
        for (ch = 1; ch <= 3; ch++) {
            unsigned total = 0, acc = 0, clip;
            int lo = 0, hi = 255;
            for (v = 0; v < 256; v++) total += c->hist[ch][v];
            clip = total / 200;
            for (v = 0; v < 256; v++) { acc += c->hist[ch][v]; if (acc > clip) { lo = v; break; } }
            acc = 0;
            for (v = 255; v >= 0; v--) { acc += c->hist[ch][v]; if (acc > clip) { hi = v; break; } }
            if (hi > lo + 2) { c->lv.in_black[ch] = lo; c->lv.in_white[ch] = hi; }
        }
        levels_sync_fields(d);
        levels_apply_preview(d);
        return;
    }
    if (id == 131 && code == DLG_CODE_CLICK) { pc_levels_default(&c->lv); levels_sync_fields(d); levels_apply_preview(d); return; }
    if (id == 150 || (code == DLG_CODE_CHANGE && id >= 101 && id <= 105)) levels_apply_preview(d);
    if (id == 150) levels_apply_preview(d);
}

int dlg_levels(PcDoc *doc, PcLevels *lv)
{
    static const char *const chans[] = { "RGB", "Red", "Green", "Blue" };
    Dlg *d;
    LevelsCtx *c = (LevelsCtx *)calloc(1, sizeof *c);
    int rc, W = 380, k;
    if (!c) return 0;
    c->doc = doc; c->lv = *lv;
    for (k = 0; k < 4; k++) pc_histogram(doc, k, 0, c->hist[k]);
    d = dlg_create("Levels", W, 290, levels_change, c);
    if (!d) { free(c); return 0; }
    dlg_label(d, 12, 15, 56, 16, "Channel:");
    dlg_combo(d, 100, 68, 12, 100, 20, chans, doc->s.mode == PC_MODE_GRAY ? 1 : 4, 0);
    dlg_label(d, 12, 43, 80, 16, "Input Levels:");
    dlg_edit(d, 101, 92, 40, 40, 20, "0", DLG_NUMBER);
    dlg_edit(d, 102, 136, 40, 44, 20, "1.00", DLG_NUMBER | DLG_DECIMAL);
    dlg_edit(d, 103, 184, 40, 40, 20, "255", DLG_NUMBER);
    dlg_custom(d, 120, 12, 66, 260, 132, levels_paint_hist, levels_mouse);
    dlg_label(d, 12, 211, 80, 16, "Output Levels:");
    dlg_edit(d, 104, 100, 208, 40, 20, "0", DLG_NUMBER);
    dlg_edit(d, 105, 144, 208, 40, 20, "255", DLG_NUMBER);
    dlg_custom(d, 121, 12, 236, 260, 22, levels_paint_out, levels_mouse);
    ok_cancel(d, W);
    dlg_button(d, 131, bx(W), 76, BW, BH, "&Reset", 0);
    dlg_button(d, 130, bx(W), 104, BW, BH, "&Auto", 0);
    dlg_check(d, 150, bx(W), 140, BW + 4, 16, "&Preview", 1);
    levels_sync_fields(d);
    preview_begin(&c->pv, doc);
    rc = dlg_run(d);
    if (rc == IDOK) { levels_change(d, 101, DLG_CODE_CHANGE); *lv = c->lv; }
    preview_end(&c->pv, 0);
    dlg_destroy(d);
    free(c);
    return rc == IDOK;
}

/* ---------------- Curves ---------------- */

typedef struct { PcDoc *doc; PcCurves cv; int ch; int drag; DlgPreview pv; int mx, my; } CurvesCtx;

static void curves_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    CurvesCtx *c = (CurvesCtx *)d->user;
    RECT g;
    u8 lut[256];
    int k, i;
    (void)id;
    SetRect(&g, r->left + 12, r->top, r->left + 12 + 258, r->top + 258);
    w95_fill(dc, &g, W95_WINDOW);
    w95_rect(dc, g.left, g.top, g.right, g.bottom, W95_DKSHADOW);
    for (k = 1; k < 4; k++) {
        int p = k * 64;
        for (i = 1; i < 257; i += 2) { w95_pixel(dc, g.left + p, g.top + i, W95_SHADOW); w95_pixel(dc, g.left + i, g.top + p, W95_SHADOW); }
    }
    /* Gradient bars along the axes, as in PS5. */
    for (i = 0; i < 256; i++) {
        w95_vline(dc, g.left + 1 + i, g.bottom + 2, g.bottom + 9, RGB(i, i, i));
        w95_hline(dc, r->left, r->left + 8, g.bottom - 2 - i, RGB(i, i, i));
    }
    pc_curve_lut(&c->cv.c[c->ch], lut);
    for (i = 0; i < 256; i++) {
        w95_pixel(dc, g.left + 1 + i, g.bottom - 2 - lut[i], W95_TEXT);
        if (i) { int a = lut[i - 1], b = lut[i], lo = PC_MIN(a, b), hi = PC_MAX(a, b); w95_vline(dc, g.left + 1 + i, g.bottom - 2 - hi, g.bottom - 1 - lo, W95_TEXT); }
    }
    for (k = 0; k < c->cv.c[c->ch].n; k++) {
        RECT p;
        int x = g.left + 1 + c->cv.c[c->ch].x[k], y = g.bottom - 2 - c->cv.c[c->ch].y[k];
        SetRect(&p, x - 3, y - 3, x + 4, y + 4);
        w95_rect(dc, p.left, p.top, p.right, p.bottom, W95_TEXT);
    }
}

static void curves_preview(Dlg *d)
{
    CurvesCtx *c = (CurvesCtx *)d->user;
    preview_restore(&c->pv);
    if (dlg_get_check(d, 150)) preview_curves(c->doc, &c->cv);
    app_refresh(R_CANVAS);
}

static void curves_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    CurvesCtx *c = (CurvesCtx *)d->user;
    PcCurve *cu = &c->cv.c[c->ch];
    int ix = PC_CLAMP(x - 13, 0, 255), iy = PC_CLAMP(256 - y, 0, 255), k;
    char b[32];
    (void)id;
    pc_snprintf(b, sizeof b, "Input: %d", ix); dlg_set_text(d, 110, b);
    pc_snprintf(b, sizeof b, "Output: %d", iy); dlg_set_text(d, 111, b);
    if (msg == WM_LBUTTONDOWN) {
        c->drag = -1;
        for (k = 0; k < cu->n; k++) if (abs(cu->x[k] - ix) <= 4 && abs(cu->y[k] - iy) <= 6) c->drag = k;
        if (c->drag < 0 && cu->n < 16) {
            /* Insert sorted. */
            for (k = 0; k < cu->n && cu->x[k] < ix; k++) {}
            if (k < cu->n && cu->x[k] == ix) { c->drag = k; }
            else {
                memmove(&cu->x[k + 1], &cu->x[k], sizeof(int) * (cu->n - k));
                memmove(&cu->y[k + 1], &cu->y[k], sizeof(int) * (cu->n - k));
                cu->x[k] = ix; cu->y[k] = iy; cu->n++;
                c->drag = k;
            }
        }
    }
    if (msg == WM_MOUSEMOVE && (wp & MK_LBUTTON) && c->drag >= 0 && c->drag < cu->n) {
        int lo = c->drag > 0 ? cu->x[c->drag - 1] + 1 : 0, hi = c->drag < cu->n - 1 ? cu->x[c->drag + 1] - 1 : 255;
        if ((y < -20 || y > 280) && cu->n > 2) {
            /* Dragging a point off the graph deletes it. */
            memmove(&cu->x[c->drag], &cu->x[c->drag + 1], sizeof(int) * (cu->n - c->drag - 1));
            memmove(&cu->y[c->drag], &cu->y[c->drag + 1], sizeof(int) * (cu->n - c->drag - 1));
            cu->n--; c->drag = -1;
        } else {
            cu->x[c->drag] = PC_CLAMP(ix, lo, hi); cu->y[c->drag] = iy;
        }
        dlg_redraw(d, 120);
    }
    if (msg == WM_LBUTTONDOWN) dlg_redraw(d, 120);
    if (msg == WM_LBUTTONUP) { c->drag = -1; curves_preview(d); }
}

static void curves_change(Dlg *d, int id, int code)
{
    CurvesCtx *c = (CurvesCtx *)d->user;
    if (id == 100 && code == DLG_CODE_CHANGE) { c->ch = PC_CLAMP(dlg_get_combo(d, 100), 0, 3); dlg_redraw(d, 120); }
    if (id == 131 && code == DLG_CODE_CLICK) { pc_curves_default(&c->cv); dlg_redraw(d, 120); curves_preview(d); }
    if (id == 132 && code == DLG_CODE_CLICK) {
        /* Smooth: drop interior points closest to the line. */
        PcCurve *cu = &c->cv.c[c->ch];
        if (cu->n > 2) { memmove(&cu->x[1], &cu->x[2], sizeof(int) * (cu->n - 2)); memmove(&cu->y[1], &cu->y[2], sizeof(int) * (cu->n - 2)); cu->n--; }
        dlg_redraw(d, 120); curves_preview(d);
    }
    if (id == 150) curves_preview(d);
}

int dlg_curves(PcDoc *doc, PcCurves *cv)
{
    static const char *const chans[] = { "RGB", "Red", "Green", "Blue" };
    Dlg *d;
    CurvesCtx *c = (CurvesCtx *)calloc(1, sizeof *c);
    int rc, W = 380;
    if (!c) return 0;
    c->doc = doc; c->cv = *cv; c->drag = -1;
    d = dlg_create("Curves", W, 330, curves_change, c);
    if (!d) { free(c); return 0; }
    dlg_label(d, 12, 15, 56, 16, "Channel:");
    dlg_combo(d, 100, 68, 12, 100, 20, chans, doc->s.mode == PC_MODE_GRAY ? 1 : 4, 0);
    dlg_custom(d, 120, 12, 40, 282, 270, curves_paint, curves_mouse);
    dlg_label_id(d, 110, 26, 310, 90, 14, "Input:", 0);
    dlg_label_id(d, 111, 120, 310, 90, 14, "Output:", 0);
    ok_cancel(d, W);
    dlg_button(d, 131, bx(W), 76, BW, BH, "&Reset", 0);
    dlg_button(d, 132, bx(W), 104, BW, BH, "S&mooth", 0);
    dlg_check(d, 150, bx(W), 140, BW + 4, 16, "&Preview", 1);
    preview_begin(&c->pv, doc);
    rc = dlg_run(d);
    if (rc == IDOK) *cv = c->cv;
    preview_end(&c->pv, 0);
    dlg_destroy(d);
    free(c);
    return rc == IDOK;
}

/* ---------------- Hue/Saturation ---------------- */

typedef struct { PcDoc *doc; PcHueSat hs; int range; DlgPreview pv; } HsCtx;

static void hs_preview(Dlg *d)
{
    HsCtx *c = (HsCtx *)d->user;
    preview_restore(&c->pv);
    if (dlg_get_check(d, 150)) preview_huesat(c->doc, &c->hs);
    app_refresh(R_CANVAS);
    dlg_redraw(d, 160);
}

static void hs_sync(Dlg *d)
{
    HsCtx *c = (HsCtx *)d->user;
    int r = c->range;
    dlg_slider_set(d, 110, c->hs.hue[r]); dlg_set_int(d, 101, c->hs.hue[r]);
    dlg_slider_set(d, 111, c->hs.sat[r]); dlg_set_int(d, 102, c->hs.sat[r]);
    dlg_slider_set(d, 112, c->hs.light[r]); dlg_set_int(d, 103, c->hs.light[r]);
}

static void hs_bars(Dlg *d, int id, HDC dc, const RECT *r)
{
    HsCtx *c = (HsCtx *)d->user;
    int x, w = r->right - r->left;
    (void)id;
    for (x = 0; x < w; x++) {
        int rr, gg, bb, r2, g2, b2;
        float h = (float)x * 360 / w;
        pc_hsb_to_rgb(h, 1, 1, &rr, &gg, &bb);
        pc_hsb_to_rgb(h + (float)c->hs.hue[0], c->hs.sat[0] < 0 ? 1 + c->hs.sat[0] / 100.0f : 1, 1, &r2, &g2, &b2);
        w95_vline(dc, r->left + x, r->top, r->top + 8, RGB(rr, gg, bb));
        w95_vline(dc, r->left + x, r->top + 11, r->top + 19, RGB(r2, g2, b2));
    }
}

static void hs_change(Dlg *d, int id, int code)
{
    HsCtx *c = (HsCtx *)d->user;
    int r = c->range;
    if (id == 100 && code == DLG_CODE_CHANGE) { c->range = PC_CLAMP(dlg_get_combo(d, 100), 0, 6); hs_sync(d); return; }
    if (id == 110 || id == 111 || id == 112) {
        c->hs.hue[r] = dlg_slider_get(d, 110); c->hs.sat[r] = dlg_slider_get(d, 111); c->hs.light[r] = dlg_slider_get(d, 112);
        if (code == DLG_CODE_CLICK) hs_preview(d);
        return;
    }
    if ((id == 101 || id == 102 || id == 103) && code == DLG_CODE_CHANGE) {
        c->hs.hue[r] = PC_CLAMP(dlg_get_int(d, 101), -180, 180);
        c->hs.sat[r] = PC_CLAMP(dlg_get_int(d, 102), -100, 100);
        c->hs.light[r] = PC_CLAMP(dlg_get_int(d, 103), -100, 100);
        dlg_slider_set(d, 110, c->hs.hue[r]); dlg_slider_set(d, 111, c->hs.sat[r]); dlg_slider_set(d, 112, c->hs.light[r]);
        hs_preview(d);
        return;
    }
    if (id == 120) {
        c->hs.colorize = dlg_get_check(d, 120);
        if (c->hs.colorize) { c->range = 0; dlg_set_combo(d, 100, 0); if (c->hs.sat[0] <= 0) c->hs.sat[0] = 25; hs_sync(d); }
        dlg_enable(d, 100, !c->hs.colorize);
        hs_preview(d);
    }
    if (id == 150) hs_preview(d);
}

int dlg_hue_sat(PcDoc *doc, PcHueSat *hs)
{
    static const char *const ranges[] = { "Master", "Reds", "Yellows", "Greens", "Cyans", "Blues", "Magentas" };
    Dlg *d;
    HsCtx *c = (HsCtx *)calloc(1, sizeof *c);
    int rc, W = 400;
    if (!c) return 0;
    c->doc = doc; c->hs = *hs;
    d = dlg_create("Hue/Saturation", W, 250, hs_change, c);
    if (!d) { free(c); return 0; }
    dlg_label(d, 12, 15, 30, 16, "Edit:");
    dlg_combo(d, 100, 48, 12, 110, 20, ranges, 7, 0);
    dlg_label(d, 20, 48, 70, 16, "Hue:");
    dlg_edit(d, 101, 240, 45, 44, 20, "0", DLG_NUMBER | DLG_SIGNED);
    dlg_slider(d, 110, 20, 68, 266, -180, 180, 0, 101, SLIDER_HUE_BAR | SLIDER_CENTER_ZERO);
    dlg_label(d, 20, 98, 70, 16, "Saturation:");
    dlg_edit(d, 102, 240, 95, 44, 20, "0", DLG_NUMBER | DLG_SIGNED);
    dlg_slider(d, 111, 20, 118, 266, -100, 100, 0, 102, SLIDER_SAT_BAR | SLIDER_CENTER_ZERO);
    dlg_label(d, 20, 148, 70, 16, "Lightness:");
    dlg_edit(d, 103, 240, 145, 44, 20, "0", DLG_NUMBER | DLG_SIGNED);
    dlg_slider(d, 112, 20, 168, 266, -100, 100, 0, 103, SLIDER_LIGHT_BAR | SLIDER_CENTER_ZERO);
    dlg_custom(d, 160, 20, 200, 266, 20, hs_bars, NULL);
    ok_cancel(d, W);
    dlg_check(d, 120, bx(W), 180, BW + 6, 16, "C&olorize", hs->colorize);
    dlg_check(d, 150, bx(W), 202, BW + 6, 16, "&Preview", 1);
    hs_sync(d);
    preview_begin(&c->pv, doc);
    rc = dlg_run(d);
    if (rc == IDOK) *hs = c->hs;
    preview_end(&c->pv, 0);
    dlg_destroy(d);
    free(c);
    return rc == IDOK;
}

/* ---------------- Color Balance ---------------- */

typedef struct { PcDoc *doc; PcColorBalance cb; int tone; DlgPreview pv; } CbCtx;

static void cb_preview(Dlg *d)
{
    CbCtx *c = (CbCtx *)d->user;
    preview_restore(&c->pv);
    if (dlg_get_check(d, 150)) preview_balance(c->doc, &c->cb);
    app_refresh(R_CANVAS);
}

static void cb_sync(Dlg *d)
{
    CbCtx *c = (CbCtx *)d->user;
    int k;
    for (k = 0; k < 3; k++) { dlg_set_int(d, 101 + k, c->cb.v[c->tone][k]); dlg_slider_set(d, 110 + k, c->cb.v[c->tone][k]); }
}

static void cb_change(Dlg *d, int id, int code)
{
    CbCtx *c = (CbCtx *)d->user;
    int k;
    if (id >= 120 && id <= 122 && code == DLG_CODE_CLICK) { c->tone = id - 120; cb_sync(d); return; }
    if (id >= 110 && id <= 112) { c->cb.v[c->tone][id - 110] = dlg_slider_get(d, id); if (code == DLG_CODE_CLICK) cb_preview(d); return; }
    if (id >= 101 && id <= 103 && code == DLG_CODE_CHANGE) {
        for (k = 0; k < 3; k++) { c->cb.v[c->tone][k] = PC_CLAMP(dlg_get_int(d, 101 + k), -100, 100); dlg_slider_set(d, 110 + k, c->cb.v[c->tone][k]); }
        cb_preview(d);
        return;
    }
    if (id == 130) { c->cb.preserve_luminosity = dlg_get_check(d, 130); cb_preview(d); }
    if (id == 150) cb_preview(d);
}

int dlg_color_balance(PcDoc *doc, PcColorBalance *cb)
{
    static const char *const left[3] = { "Cyan", "Magenta", "Yellow" }, *const right[3] = { "Red", "Green", "Blue" };
    Dlg *d;
    CbCtx *c = (CbCtx *)calloc(1, sizeof *c);
    int rc, W = 400, k;
    if (!c) return 0;
    c->doc = doc; c->cb = *cb; c->tone = 1;
    d = dlg_create("Color Balance", W, 230, cb_change, c);
    if (!d) { free(c); return 0; }
    dlg_group(d, 10, 8, 300, 128, "Color Balance");
    dlg_label(d, 22, 28, 76, 16, "Color Levels:");
    for (k = 0; k < 3; k++) dlg_edit(d, 101 + k, 104 + k * 46, 25, 40, 20, "0", DLG_NUMBER | DLG_SIGNED);
    for (k = 0; k < 3; k++) {
        dlg_label(d, 22, 58 + k * 24, 54, 16, left[k]);
        dlg_slider(d, 110 + k, 78, 60 + k * 24, 170, -100, 100, 0, 0, SLIDER_CENTER_ZERO);
        dlg_label(d, 254, 58 + k * 24, 50, 16, right[k]);
    }
    dlg_group(d, 10, 142, 300, 76, "Tone Balance");
    dlg_radio(d, 120, 22, 160, 80, 16, "&Shadows", 0, 1);
    dlg_radio(d, 121, 104, 160, 80, 16, "&Midtones", 1, 0);
    dlg_radio(d, 122, 190, 160, 90, 16, "&Highlights", 0, 0);
    dlg_check(d, 130, 22, 186, 160, 16, "Preserve &Luminosity", cb->preserve_luminosity);
    ok_cancel(d, W);
    dlg_check(d, 150, bx(W), 76, BW + 4, 16, "&Preview", 1);
    cb_sync(d);
    preview_begin(&c->pv, doc);
    rc = dlg_run(d);
    if (rc == IDOK) *cb = c->cb;
    preview_end(&c->pv, 0);
    dlg_destroy(d);
    free(c);
    return rc == IDOK;
}

/* ---------------- one- and two-value adjustment dialogs ---------------- */

typedef struct { PcDoc *doc; int kind; DlgPreview pv; int a, b; } SimpleCtx;

static void simple_preview(Dlg *d)
{
    SimpleCtx *c = (SimpleCtx *)d->user;
    preview_restore(&c->pv);
    if (dlg_get_check(d, 150)) {
        if (c->kind == 0) preview_value(c->doc, PC_ADJ_BRIGHTNESS_CONTRAST, c->a, c->b);
        else if (c->kind == 1) preview_value(c->doc, PC_ADJ_THRESHOLD, PC_CLAMP(c->a, 1, 255), 0);
        else if (c->kind == 2) preview_value(c->doc, PC_ADJ_POSTERIZE, PC_CLAMP(c->a, 2, 255), 0);
    }
    app_refresh(R_CANVAS);
}

static void simple_change(Dlg *d, int id, int code)
{
    SimpleCtx *c = (SimpleCtx *)d->user;
    if (id == 110 || id == 111) { if (id == 110) c->a = dlg_slider_get(d, 110); else c->b = dlg_slider_get(d, 111); if (code == DLG_CODE_CLICK) simple_preview(d); return; }
    if ((id == 101 || id == 102) && code == DLG_CODE_CHANGE) {
        c->a = dlg_get_int(d, 101); c->b = dlg_get_int(d, 102);
        dlg_slider_set(d, 110, c->a); dlg_slider_set(d, 111, c->b);
        simple_preview(d);
    }
    if (id == 150) simple_preview(d);
}

int dlg_brightness_contrast(PcDoc *doc, int *b, int *ct)
{
    Dlg *d;
    SimpleCtx c;
    int rc, W = 380;
    memset(&c, 0, sizeof c);
    c.doc = doc; c.kind = 0; c.a = *b; c.b = *ct;
    d = dlg_create("Brightness/Contrast", W, 130, simple_change, &c);
    if (!d) return 0;
    dlg_group(d, 10, 8, 280, 112, "");
    dlg_label(d, 22, 22, 80, 16, "Brightness:");
    dlg_edit(d, 101, 230, 19, 44, 20, "0", DLG_NUMBER | DLG_SIGNED);
    dlg_slider(d, 110, 22, 44, 254, -100, 100, *b, 101, SLIDER_CENTER_ZERO);
    dlg_label(d, 22, 70, 80, 16, "Contrast:");
    dlg_edit(d, 102, 230, 67, 44, 20, "0", DLG_NUMBER | DLG_SIGNED);
    dlg_slider(d, 111, 22, 92, 254, -100, 100, *ct, 102, SLIDER_CENTER_ZERO);
    ok_cancel(d, W);
    dlg_check(d, 150, bx(W), 76, BW + 4, 16, "&Preview", 1);
    preview_begin(&c.pv, doc);
    rc = dlg_run(d);
    if (rc == IDOK) { *b = c.a; *ct = c.b; }
    preview_end(&c.pv, 0);
    dlg_destroy(d);
    return rc == IDOK;
}

static void thr_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    SimpleCtx *c = (SimpleCtx *)d->user;
    static unsigned h[256];
    static PcDoc *last;
    unsigned mx = 1;
    int v, hh = r->bottom - r->top;
    (void)id;
    if (last != c->doc) { pc_histogram(c->doc, 0, 0, h); last = c->doc; }
    w95_fill(dc, r, W95_WINDOW);
    w95_rect(dc, r->left, r->top, r->left + 258, r->bottom, W95_DKSHADOW);
    for (v = 0; v < 256; v++) if (h[v] > mx) mx = h[v];
    for (v = 0; v < 256; v++) { int bh = (int)((double)h[v] * (hh - 4) / mx); if (bh) w95_vline(dc, r->left + 1 + v, r->bottom - 1 - bh, r->bottom - 1, W95_TEXT); }
}

int dlg_threshold(PcDoc *doc, int *level)
{
    Dlg *d;
    SimpleCtx c;
    int rc, W = 380;
    memset(&c, 0, sizeof c);
    c.doc = doc; c.kind = 1; c.a = *level;
    d = dlg_create("Threshold", W, 170, simple_change, &c);
    if (!d) return 0;
    dlg_label(d, 12, 15, 100, 16, "Threshold Level:");
    dlg_edit(d, 101, 112, 12, 44, 20, "128", DLG_NUMBER);
    dlg_custom(d, 120, 12, 40, 260, 96, thr_paint, NULL);
    dlg_slider(d, 110, 9, 138, 266, 1, 255, *level, 101, 0);
    dlg_set_int(d, 101, *level);
    ok_cancel(d, W);
    dlg_check(d, 150, bx(W), 76, BW + 4, 16, "&Preview", 1);
    preview_begin(&c.pv, doc);
    simple_preview(d);
    rc = dlg_run(d);
    if (rc == IDOK) *level = PC_CLAMP(c.a, 1, 255);
    preview_end(&c.pv, 0);
    dlg_destroy(d);
    return rc == IDOK;
}

static void post_change(Dlg *d, int id, int code)
{
    SimpleCtx *c = (SimpleCtx *)d->user;
    if (id == 101 && code == DLG_CODE_CHANGE) { c->a = dlg_get_int(d, 101); if (c->a >= 2) simple_preview(d); }
    if (id == 150) simple_preview(d);
}

int dlg_posterize(PcDoc *doc, int *levels)
{
    Dlg *d;
    SimpleCtx c;
    int rc;
    char b[16];
    memset(&c, 0, sizeof c);
    c.doc = doc; c.kind = 2; c.a = *levels;
    d = dlg_create("Posterize", 280, 76, post_change, &c);
    if (!d) return 0;
    dlg_label(d, 14, 18, 50, 16, "Levels:");
    pc_snprintf(b, sizeof b, "%d", *levels);
    dlg_edit(d, 101, 64, 15, 50, 20, b, DLG_NUMBER);
    ok_cancel(d, 280);
    dlg_check(d, 150, 14, 48, BW + 4, 16, "&Preview", 1);
    preview_begin(&c.pv, doc);
    simple_preview(d);
    rc = dlg_run(d);
    if (rc == IDOK) *levels = PC_CLAMP(dlg_get_int(d, 101), 2, 255);
    preview_end(&c.pv, 0);
    dlg_destroy(d);
    return rc == IDOK;
}

typedef struct { PcDoc *doc; int m[3][4]; int mono, out; DlgPreview pv; } MixCtx;

static void mix_sync(Dlg *d)
{
    MixCtx *c = (MixCtx *)d->user;
    int k, o = c->mono ? 0 : c->out;
    for (k = 0; k < 3; k++) dlg_set_int(d, 101 + k, c->m[o][k]);
    dlg_set_int(d, 104, c->m[o][3]);
}

static void mix_preview(Dlg *d)
{
    MixCtx *c = (MixCtx *)d->user;
    preview_restore(&c->pv);
    if (dlg_get_check(d, 150)) {
        PcAdjust a;
        pc_adjust_default(&a, PC_ADJ_CHANNEL_MIXER);
        memcpy(a.mixer, c->m, sizeof a.mixer);
        a.mixer_mono = c->mono;
        if (c->mono) { memcpy(a.mixer[1], c->m[0], sizeof a.mixer[1]); memcpy(a.mixer[2], c->m[0], sizeof a.mixer[2]); }
        preview_adjust(c->doc, &a);
    }
    app_refresh(R_CANVAS);
}

static void mix_change(Dlg *d, int id, int code)
{
    MixCtx *c = (MixCtx *)d->user;
    if (id == 100 && code == DLG_CODE_CHANGE) { c->out = PC_CLAMP(dlg_get_combo(d, 100), 0, 2); mix_sync(d); return; }
    if (id >= 101 && id <= 104 && code == DLG_CODE_CHANGE) {
        int k, o = c->mono ? 0 : c->out;
        for (k = 0; k < 3; k++) c->m[o][k] = PC_CLAMP(dlg_get_int(d, 101 + k), -200, 200);
        c->m[o][3] = PC_CLAMP(dlg_get_int(d, 104), -200, 200);
        mix_preview(d);
        return;
    }
    if (id == 105) {
        c->mono = dlg_get_check(d, 105);
        if (c->mono) { c->out = 0; dlg_set_combo(d, 100, 0); }
        dlg_enable(d, 100, !c->mono);
        mix_sync(d);
        mix_preview(d);
    }
    if (id == 150) mix_preview(d);
}

int dlg_channel_mixer(PcDoc *doc, int m[3][4], int *mono)
{
    static const char *const outs[] = { "Red", "Green", "Blue" };
    Dlg *d;
    MixCtx *c = (MixCtx *)calloc(1, sizeof *c);
    int rc, W = 380, k;
    if (!c) return 0;
    c->doc = doc; memcpy(c->m, m, sizeof c->m); c->mono = *mono;
    d = dlg_create("Channel Mixer", W, 210, mix_change, c);
    if (!d) { free(c); return 0; }
    dlg_label(d, 12, 15, 90, 16, "Output Channel:");
    dlg_combo(d, 100, 104, 12, 90, 20, outs, 3, 0);
    dlg_group(d, 10, 42, 280, 110, "Source Channels");
    for (k = 0; k < 3; k++) {
        dlg_label(d, 22, 64 + k * 26, 50, 16, outs[k]);
        dlg_edit(d, 101 + k, 80, 61 + k * 26, 50, 20, "", DLG_NUMBER | DLG_SIGNED);
        dlg_label(d, 134, 64 + k * 26, 20, 16, "%");
    }
    dlg_label(d, 22, 164, 60, 16, "Constant:");
    dlg_edit(d, 104, 80, 161, 50, 20, "", DLG_NUMBER | DLG_SIGNED);
    dlg_check(d, 105, 12, 190, 120, 16, "&Monochrome", *mono);
    ok_cancel(d, W);
    dlg_check(d, 150, bx(W), 76, BW + 4, 16, "&Preview", 1);
    dlg_enable(d, 100, !c->mono);
    mix_sync(d);
    preview_begin(&c->pv, doc);
    rc = dlg_run(d);
    if (rc == IDOK) {
        mix_change(d, 101, DLG_CODE_CHANGE);
        *mono = c->mono;
        if (c->mono) for (k = 1; k < 3; k++) memcpy(c->m[k], c->m[0], sizeof c->m[0]);
        memcpy(m, c->m, sizeof c->m);
    }
    preview_end(&c->pv, 0);
    dlg_destroy(d);
    free(c);
    return rc == IDOK;
}

/* ---------------- adjustment layer dialogs ---------------- */

int dlg_adjust_layer(PcDoc *doc, PcAdjust *a)
{
    int ok = 1;
    g_adj_layer_mode = 1;
    switch (a->type) {
    case PC_ADJ_BRIGHTNESS_CONTRAST: ok = dlg_brightness_contrast(doc, &a->brightness, &a->contrast); break;
    case PC_ADJ_LEVELS: ok = dlg_levels(doc, &a->levels); break;
    case PC_ADJ_CURVES: ok = dlg_curves(doc, &a->curves); break;
    case PC_ADJ_COLOR_BALANCE: ok = dlg_color_balance(doc, &a->balance); break;
    case PC_ADJ_HUE_SAT: ok = dlg_hue_sat(doc, &a->huesat); break;
    case PC_ADJ_CHANNEL_MIXER: ok = dlg_channel_mixer(doc, a->mixer, &a->mixer_mono); break;
    case PC_ADJ_THRESHOLD: ok = dlg_threshold(doc, &a->threshold); break;
    case PC_ADJ_POSTERIZE: ok = dlg_posterize(doc, &a->posterize); break;
    default: break;                         /* Invert has no settings */
    }
    g_adj_layer_mode = 0;
    return ok;
}

/* ---------------- filter dialogs ---------------- */

enum { FP_FLOAT, FP_INT, FP_RADIO, FP_CHECK };
typedef struct { int type; const char *label; int field; double min, max; const char *suffix; const char *opts; } FParam;
/* fields: 0 radius 1 amount 2 threshold 3 angle 4 distance 5 mode 6 mono 7 cell 8 dx 9 dy 10 wrap */

typedef struct { PcDoc *doc; int filter; PcFilterParams *p; const FParam *fp; int nfp; u8 *prev_src, *prev_dst; int pw, ph, px, py; DlgPreview pv; } FilterCtx;

static double fp_get(const PcFilterParams *p, int f)
{
    switch (f) { case 0: return p->radius; case 1: return p->amount; case 2: return p->threshold; case 3: return p->angle; case 4: return p->distance;
    case 5: return p->mode; case 6: return p->mono; case 7: return p->cell; case 8: return p->dx; case 9: return p->dy; default: return p->wrap; }
}

static void fp_set(PcFilterParams *p, int f, double v)
{
    switch (f) { case 0: p->radius = (float)v; break; case 1: p->amount = (float)v; break; case 2: p->threshold = (int)v; break; case 3: p->angle = (int)v; break;
    case 4: p->distance = (int)v; break; case 5: p->mode = (int)v; break; case 6: p->mono = (int)v; break; case 7: p->cell = (int)v; break;
    case 8: p->dx = (int)v; break; case 9: p->dy = (int)v; break; default: p->wrap = (int)v; break; }
}

static const FParam *filter_params(int f, int *n)
{
    static const FParam gauss[] = { { FP_FLOAT, "Radius:", 0, 0.1, 250, "pixels", NULL } };
    static const FParam motion[] = { { FP_INT, "Angle:", 3, -360, 360, "\xB0", NULL }, { FP_INT, "Distance:", 4, 1, 999, "pixels", NULL } };
    static const FParam radial[] = { { FP_INT, "Amount:", 1, 1, 100, NULL, NULL }, { FP_RADIO, "Blur Method", 5, 0, 1, NULL, "Spin\0Zoom\0" } };
    static const FParam unsharp[] = { { FP_INT, "Amount:", 1, 1, 500, "%", NULL }, { FP_FLOAT, "Radius:", 0, 0.1, 250, "pixels", NULL }, { FP_INT, "Threshold:", 2, 0, 255, "levels", NULL } };
    static const FParam noise[] = { { FP_INT, "Amount:", 1, 1, 999, NULL, NULL }, { FP_RADIO, "Distribution", 5, 0, 1, NULL, "Uniform\0Gaussian\0" }, { FP_CHECK, "Monochromatic", 6, 0, 1, NULL, NULL } };
    static const FParam dust[] = { { FP_INT, "Radius:", 0, 1, 16, "pixels", NULL }, { FP_INT, "Threshold:", 2, 0, 255, "levels", NULL } };
    static const FParam median[] = { { FP_INT, "Radius:", 0, 1, 16, "pixels", NULL } };
    static const FParam emboss[] = { { FP_INT, "Angle:", 3, -360, 360, "\xB0", NULL }, { FP_INT, "Height:", 4, 1, 10, "pixels", NULL }, { FP_INT, "Amount:", 1, 1, 500, "%", NULL } };
    static const FParam diffuse[] = { { FP_RADIO, "Mode", 5, 0, 2, NULL, "Normal\0Darken Only\0Lighten Only\0" } };
    static const FParam wind[] = { { FP_RADIO, "Direction", 3, 0, 1, NULL, "From the Right\0From the Left\0" } };
    static const FParam trace[] = { { FP_INT, "Level:", 2, 0, 255, NULL, NULL } };
    static const FParam mosaic[] = { { FP_INT, "Cell Size:", 7, 2, 200, "square", NULL } };
    static const FParam twirl[] = { { FP_INT, "Angle:", 1, -999, 999, "degrees", NULL } };
    static const FParam pinch[] = { { FP_INT, "Amount:", 1, -100, 100, "%", NULL } };
    static const FParam ripple[] = { { FP_INT, "Amount:", 1, -999, 999, NULL, NULL }, { FP_RADIO, "Size", 5, 0, 2, NULL, "Small\0Medium\0Large\0" } };
    static const FParam offset[] = { { FP_INT, "Horizontal:", 8, -30000, 30000, "pixels right", NULL }, { FP_INT, "Vertical:", 9, -30000, 30000, "pixels down", NULL },
                                     { FP_RADIO, "Undefined Areas", 10, 0, 2, NULL, "Set to Transparent\0Repeat Edge Pixels\0Wrap Around\0" } };
    static const FParam highpass[] = { { FP_FLOAT, "Radius:", 0, 0.1, 250, "pixels", NULL } };
    static const FParam maxmin[] = { { FP_INT, "Radius:", 0, 1, 10, "pixels", NULL } };
#define R(a) (*n = (int)(sizeof a / sizeof a[0]), a)
    switch (f) {
    case PC_FILTER_GAUSSIAN_BLUR: return R(gauss);
    case PC_FILTER_MOTION_BLUR: return R(motion);
    case PC_FILTER_RADIAL_BLUR: return R(radial);
    case PC_FILTER_UNSHARP_MASK: return R(unsharp);
    case PC_FILTER_ADD_NOISE: return R(noise);
    case PC_FILTER_DUST_SCRATCHES: return R(dust);
    case PC_FILTER_MEDIAN: return R(median);
    case PC_FILTER_EMBOSS: return R(emboss);
    case PC_FILTER_DIFFUSE: return R(diffuse);
    case PC_FILTER_WIND: return R(wind);
    case PC_FILTER_TRACE_CONTOUR: return R(trace);
    case PC_FILTER_MOSAIC: return R(mosaic);
    case PC_FILTER_TWIRL: return R(twirl);
    case PC_FILTER_PINCH: case PC_FILTER_SPHERIZE: return R(pinch);
    case PC_FILTER_RIPPLE: return R(ripple);
    case PC_FILTER_OFFSET: return R(offset);
    case PC_FILTER_HIGH_PASS: return R(highpass);
    case PC_FILTER_MAXIMUM: case PC_FILTER_MINIMUM: return R(maxmin);
    }
#undef R
    *n = 0;
    return NULL;
}

static void filter_read(Dlg *d)
{
    FilterCtx *c = (FilterCtx *)d->user;
    int i, id = 200;
    for (i = 0; i < c->nfp; i++) {
        const FParam *f = &c->fp[i];
        if (f->type == FP_FLOAT || f->type == FP_INT) {
            double v = dlg_get_double(d, id);
            fp_set(c->p, f->field, PC_CLAMP(v, f->min, f->max));
            id += 2;
        } else if (f->type == FP_CHECK) { fp_set(c->p, f->field, dlg_get_check(d, id)); id++; }
        else {
            int k, n = (int)f->max + 1;
            for (k = 0; k < n; k++) if (dlg_get_check(d, id + k)) fp_set(c->p, f->field, k);
            id += n;
        }
    }
    if (c->filter == PC_FILTER_CUSTOM) {
        int k;
        for (k = 0; k < 25; k++) c->p->kernel[k] = PC_CLAMP(dlg_get_int(d, 300 + k), -999, 999);
        c->p->scale = PC_CLAMP(dlg_get_int(d, 330), 1, 9999);
        if (!c->p->scale) c->p->scale = 1;
        c->p->offset = PC_CLAMP(dlg_get_int(d, 331), -9999, 9999);
    }
}

static void filter_preview_compute(Dlg *d)
{
    FilterCtx *c = (FilterCtx *)d->user;
    if (!c->prev_src) return;
    filter_read(d);
    pc_filter_run(c->filter, c->p, c->prev_src, c->prev_dst, c->pw, c->ph);
    dlg_redraw(d, 150);
}

static void filter_paint_preview(Dlg *d, int id, HDC dc, const RECT *r)
{
    FilterCtx *c = (FilterCtx *)d->user;
    BITMAPINFO bi;
    u8 *bgrx;
    int i, n;
    (void)id;
    w95_fill(dc, r, W95_WINDOW);
    w95_sunken(dc, r);
    if (!c->prev_dst) return;
    n = c->pw * c->ph;
    bgrx = (u8 *)malloc((size_t)n * 4);
    if (!bgrx) return;
    for (i = 0; i < n; i++) {
        const u8 *p = c->prev_dst + i * 4;
        int a = p[3], bg = ((((i % c->pw) >> 3) ^ ((i / c->pw) >> 3)) & 1) ? 204 : 255;
        bgrx[i * 4] = (u8)((p[2] * a + bg * (255 - a)) / 255); bgrx[i * 4 + 1] = (u8)((p[1] * a + bg * (255 - a)) / 255);
        bgrx[i * 4 + 2] = (u8)((p[0] * a + bg * (255 - a)) / 255); bgrx[i * 4 + 3] = 0;
    }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = c->pw; bi.bmiHeader.biHeight = -c->ph;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, r->left + 2, r->top + 2, (DWORD)c->pw, (DWORD)c->ph, 0, 0, 0, (UINT)c->ph, bgrx, &bi, DIB_RGB_COLORS);
    free(bgrx);
}

static void filter_preview_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    /* Drag in the preview to look at another part of the image (PS5). */
    FilterCtx *c = (FilterCtx *)d->user;
    static int lx, ly;
    PcLayer *l = pc_doc_active(c->doc);
    (void)id;
    if (msg == WM_LBUTTONDOWN) { lx = x; ly = y; }
    if (msg == WM_MOUSEMOVE && (wp & MK_LBUTTON) && l) {
        int yy;
        c->px = PC_CLAMP(c->px - (x - lx), 0, PC_MAX(0, c->doc->s.w - c->pw));
        c->py = PC_CLAMP(c->py - (y - ly), 0, PC_MAX(0, c->doc->s.h - c->ph));
        lx = x; ly = y;
        for (yy = 0; yy < c->ph; yy++) memcpy(c->prev_src + (size_t)yy * c->pw * 4, l->pix->px + ((size_t)(c->py + yy) * c->doc->s.w + c->px) * 4, (size_t)c->pw * 4);
        memcpy(c->prev_dst, c->prev_src, (size_t)c->pw * c->ph * 4);
        dlg_redraw(d, 150);
    }
    if (msg == WM_LBUTTONUP) filter_preview_compute(d);
}

static void filter_change(Dlg *d, int id, int code)
{
    FilterCtx *c = (FilterCtx *)d->user;
    if (code == DLG_CODE_SLIDER) return;
    if (id == 160) {
        preview_restore(&c->pv);
        if (dlg_get_check(d, 160)) { filter_read(d); pc_filter_apply(c->doc, c->filter, c->p); }
        app_refresh(R_CANVAS);
        return;
    }
    if ((id >= 200 && id < 300) || (id >= 300 && id < 340)) {
        int i, sid = 200;
        /* Keep slider and edit in step. */
        for (i = 0; i < c->nfp; i++) {
            const FParam *f = &c->fp[i];
            if (f->type == FP_FLOAT || f->type == FP_INT) {
                if (id == sid + 1) { dlg_set_double(d, sid, dlg_slider_get(d, sid + 1) / (f->type == FP_FLOAT ? 10.0 : 1.0), f->type == FP_FLOAT ? 1 : 0); }
                else if (id == sid) { dlg_slider_set(d, sid + 1, (int)(dlg_get_double(d, sid) * (f->type == FP_FLOAT ? 10 : 1))); }
                sid += 2;
            } else if (f->type == FP_CHECK) sid++;
            else sid += (int)f->max + 1;
        }
        if (code == DLG_CODE_CHANGE || code == DLG_CODE_CLICK) filter_preview_compute(d);
    }
}

int dlg_filter(PcDoc *doc, int filter, PcFilterParams *p)
{
    Dlg *d;
    FilterCtx *c = (FilterCtx *)calloc(1, sizeof *c);
    PcLayer *l = pc_doc_active(doc);
    int rc, W = 380, H, i, y = 154, id = 200;
    if (!c || !l) { free(c); return 0; }
    c->doc = doc; c->filter = filter; c->p = p;
    c->fp = filter_params(filter, &c->nfp);
    H = 172 + c->nfp * 50 + (filter == PC_FILTER_CUSTOM ? 170 : 0);
    if (H < 190) H = 190;
    d = dlg_create(pc_filter_name(filter), W, H, filter_change, c);
    if (!d) { free(c); return 0; }
    /* 140x140 preview window at the top, centred on the image. */
    c->pw = PC_MIN(136, doc->s.w); c->ph = PC_MIN(136, doc->s.h);
    c->px = (doc->s.w - c->pw) / 2; c->py = (doc->s.h - c->ph) / 2;
    c->prev_src = (u8 *)malloc((size_t)c->pw * c->ph * 4);
    c->prev_dst = (u8 *)malloc((size_t)c->pw * c->ph * 4);
    if (c->prev_src && c->prev_dst) {
        int yy;
        for (yy = 0; yy < c->ph; yy++) memcpy(c->prev_src + (size_t)yy * c->pw * 4, l->pix->px + ((size_t)(c->py + yy) * doc->s.w + c->px) * 4, (size_t)c->pw * 4);
    }
    dlg_custom(d, 150, 60, 8, 140, 140, filter_paint_preview, filter_preview_mouse);
    {
        char zl[16] = "100%";
        dlg_label(d, 205, 132, 40, 14, zl);
    }
    for (i = 0; i < c->nfp; i++) {
        const FParam *f = &c->fp[i];
        double v = fp_get(p, f->field);
        if (f->type == FP_FLOAT || f->type == FP_INT) {
            int scale = f->type == FP_FLOAT ? 10 : 1;
            dlg_label(d, 14, y + 3, 80, 16, f->label);
            dlg_edit(d, id, 100, y, 60, 20, "", DLG_NUMBER | DLG_DECIMAL | DLG_SIGNED);
            dlg_set_double(d, id, v, f->type == FP_FLOAT ? 1 : 0);
            if (f->suffix) dlg_label(d, 166, y + 3, 100, 16, f->suffix);
            dlg_slider(d, id + 1, 12, y + 24, 260, (int)(f->min * scale), (int)(PC_MIN(f->max, f->type == FP_FLOAT ? 100 : f->max) * scale), (int)(v * scale), 0, 0);
            y += 46; id += 2;
        } else if (f->type == FP_CHECK) {
            dlg_check(d, id, 14, y, 160, 16, f->label, v != 0);
            y += 24; id++;
        } else {
            const char *o = f->opts;
            int k, n = (int)f->max + 1;
            dlg_group(d, 10, y, 262, 22 + n * 18, f->label);
            for (k = 0; k < n; k++) { dlg_radio(d, id + k, 22, y + 18 + k * 18, 200, 16, o, (int)v == k, k == 0); o += strlen(o) + 1; }
            y += 28 + n * 18; id += n;
        }
    }
    if (filter == PC_FILTER_CUSTOM) {
        int k;
        for (k = 0; k < 25; k++) {
            dlg_edit(d, 300 + k, 20 + (k % 5) * 46, y + (k / 5) * 24, 42, 20, "", DLG_NUMBER | DLG_SIGNED);
            dlg_set_int(d, 300 + k, p->kernel[k]);
        }
        y += 126;
        dlg_label(d, 20, y + 3, 40, 16, "Scale:"); dlg_edit(d, 330, 60, y, 50, 20, "", DLG_NUMBER); dlg_set_int(d, 330, p->scale);
        dlg_label(d, 124, y + 3, 44, 16, "Offset:"); dlg_edit(d, 331, 168, y, 50, 20, "", DLG_NUMBER | DLG_SIGNED); dlg_set_int(d, 331, p->offset);
    }
    ok_cancel(d, W);
    dlg_check(d, 160, bx(W), 76, BW + 4, 16, "&Preview", 0);
    preview_begin(&c->pv, doc);
    filter_preview_compute(d);
    rc = dlg_run(d);
    if (rc == IDOK) filter_read(d);
    preview_end(&c->pv, 0);
    dlg_destroy(d);
    free(c->prev_src); free(c->prev_dst); free(c);
    return rc == IDOK;
}

/* ---------------- Color Picker (Photoshop 5.0 layout) ---------------- */

typedef struct { float h, s, b; int mode; u32 orig; int drag; int busy; } PickCtx;
/* mode: 0 H, 1 S, 2 B, 3 R, 4 G, 5 B(blue) */

static u32 pick_rgb(PickCtx *c) { int r, g, b; pc_hsb_to_rgb(c->h, c->s, c->b, &r, &g, &b); return pc_rgb(r, g, b); }

static u32 field_color(PickCtx *c, int x, int y)
{
    /* x, y in 0..255 across the big field for the current mode. */
    float fx = x / 255.0f, fy = 1 - y / 255.0f;
    int r, g, b;
    u32 cur = pick_rgb(c);
    switch (c->mode) {
    case 0: pc_hsb_to_rgb(c->h, fx, fy, &r, &g, &b); break;
    case 1: pc_hsb_to_rgb(fx * 360, c->s, fy, &r, &g, &b); break;
    case 2: pc_hsb_to_rgb(fx * 360, fy, c->b, &r, &g, &b); break;
    case 3: r = pc_r(cur); g = (int)(fy * 255); b = (int)(fx * 255); break;
    case 4: g = pc_g(cur); r = (int)(fy * 255); b = (int)(fx * 255); break;
    default: b = pc_b(cur); r = (int)(fx * 255); g = (int)(fy * 255); break;
    }
    return pc_rgb(r, g, b);
}

static u32 strip_color(PickCtx *c, int y)
{
    float t = 1 - y / 255.0f;
    int r, g, b;
    u32 cur = pick_rgb(c);
    switch (c->mode) {
    case 0: pc_hsb_to_rgb(t * 360, 1, 1, &r, &g, &b); break;
    case 1: pc_hsb_to_rgb(c->h, t, c->b, &r, &g, &b); break;
    case 2: pc_hsb_to_rgb(c->h, c->s, t, &r, &g, &b); break;
    case 3: r = (int)(t * 255); g = pc_g(cur); b = pc_b(cur); break;
    case 4: g = (int)(t * 255); r = pc_r(cur); b = pc_b(cur); break;
    default: b = (int)(t * 255); r = pc_r(cur); g = pc_g(cur); break;
    }
    return pc_rgb(r, g, b);
}

static void pick_paint_field(Dlg *d, int id, HDC dc, const RECT *r)
{
    PickCtx *c = (PickCtx *)d->user;
    BITMAPINFO bi;
    u8 *px = (u8 *)malloc(256 * 256 * 4);
    int x, y, cx = 0, cy = 0;
    u32 cur = pick_rgb(c);
    (void)id;
    if (!px) return;
    for (y = 0; y < 256; y++)
        for (x = 0; x < 256; x++) {
            u32 col = field_color(c, x, y);
            u8 *o = px + ((size_t)y * 256 + x) * 4;
            o[0] = (u8)pc_b(col); o[1] = (u8)pc_g(col); o[2] = (u8)pc_r(col); o[3] = 0;
        }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 256; bi.bmiHeader.biHeight = -256;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, r->left + 1, r->top + 1, 256, 256, 0, 0, 0, 256, px, &bi, DIB_RGB_COLORS);
    free(px);
    w95_rect(dc, r->left, r->top, r->left + 258, r->top + 258, W95_DKSHADOW);
    switch (c->mode) {
    case 0: cx = (int)(c->s * 255); cy = (int)((1 - c->b) * 255); break;
    case 1: cx = (int)(c->h / 360 * 255); cy = (int)((1 - c->b) * 255); break;
    case 2: cx = (int)(c->h / 360 * 255); cy = (int)((1 - c->s) * 255); break;
    case 3: cx = pc_b(cur); cy = 255 - pc_g(cur); break;
    case 4: cx = pc_b(cur); cy = 255 - pc_r(cur); break;
    default: cx = pc_r(cur); cy = 255 - pc_g(cur); break;
    }
    {
        /* The small circle marker. */
        int om = SetROP2(dc, R2_NOT);
        HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, r->left + 1 + cx - 5, r->top + 1 + cy - 5, r->left + 1 + cx + 6, r->top + 1 + cy + 6);
        SelectObject(dc, ob);
        SetROP2(dc, om);
    }
}

static int strip_pos(PickCtx *c)
{
    u32 cur = pick_rgb(c);
    switch (c->mode) {
    case 0: return (int)((1 - c->h / 360) * 255);
    case 1: return (int)((1 - c->s) * 255);
    case 2: return (int)((1 - c->b) * 255);
    case 3: return 255 - pc_r(cur);
    case 4: return 255 - pc_g(cur);
    default: return 255 - pc_b(cur);
    }
}

static void pick_paint_strip(Dlg *d, int id, HDC dc, const RECT *r)
{
    PickCtx *c = (PickCtx *)d->user;
    int y, k, sp = strip_pos(c);
    (void)id;
    for (y = 0; y < 256; y++) { u32 col = strip_color(c, y); w95_hline(dc, r->left + 8, r->left + 28, r->top + 1 + y, RGB(pc_r(col), pc_g(col), pc_b(col))); }
    w95_rect(dc, r->left + 7, r->top, r->left + 29, r->top + 258, W95_DKSHADOW);
    for (k = 0; k < 5; k++) {
        w95_vline(dc, r->left + 1 + k, r->top + 1 + sp - (4 - k), r->top + 2 + sp + (4 - k), W95_TEXT);
        w95_vline(dc, r->left + 34 - k, r->top + 1 + sp - (4 - k), r->top + 2 + sp + (4 - k), W95_TEXT);
    }
}

static void pick_paint_swatch(Dlg *d, int id, HDC dc, const RECT *r)
{
    PickCtx *c = (PickCtx *)d->user;
    RECT a = *r, b = *r;
    u32 n = pick_rgb(c);
    (void)id;
    a.bottom = (r->top + r->bottom) / 2; b.top = a.bottom;
    w95_fill(dc, &a, RGB(pc_r(n), pc_g(n), pc_b(n)));
    w95_fill(dc, &b, RGB(pc_r(c->orig), pc_g(c->orig), pc_b(c->orig)));
    w95_rect(dc, r->left, r->top, r->right, r->bottom, W95_DKSHADOW);
}

static void pick_fields(Dlg *d)
{
    PickCtx *c = (PickCtx *)d->user;
    u32 col = pick_rgb(c);
    float L, A, B;
    int cc, m, y, k;
    char hx[16];
    c->busy = 1;
    dlg_set_int(d, 110, (int)(c->h + 0.5f)); dlg_set_int(d, 111, (int)(c->s * 100 + 0.5f)); dlg_set_int(d, 112, (int)(c->b * 100 + 0.5f));
    dlg_set_int(d, 113, pc_r(col)); dlg_set_int(d, 114, pc_g(col)); dlg_set_int(d, 115, pc_b(col));
    pc_rgb_to_lab(pc_r(col), pc_g(col), pc_b(col), &L, &A, &B);
    dlg_set_int(d, 116, (int)floor(L + 0.5)); dlg_set_int(d, 117, (int)floor(A + 0.5)); dlg_set_int(d, 118, (int)floor(B + 0.5));
    pc_rgb_to_cmyk(pc_r(col), pc_g(col), pc_b(col), &cc, &m, &y, &k);
    dlg_set_int(d, 119, cc); dlg_set_int(d, 120, m); dlg_set_int(d, 121, y); dlg_set_int(d, 122, k);
    pc_snprintf(hx, sizeof hx, "%02X%02X%02X", pc_r(col), pc_g(col), pc_b(col));
    dlg_set_text(d, 123, hx);
    c->busy = 0;
    dlg_redraw(d, 100); dlg_redraw(d, 101); dlg_redraw(d, 102);
}

static void pick_set_rgb(PickCtx *c, int r, int g, int b)
{
    float h, s, v;
    pc_rgb_to_hsb(PC_CLAMP(r, 0, 255), PC_CLAMP(g, 0, 255), PC_CLAMP(b, 0, 255), &h, &s, &v);
    if (s > 0) c->h = h;
    c->s = s; c->b = v;
}

static void pick_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    PickCtx *c = (PickCtx *)d->user;
    if (msg == WM_MOUSEMOVE && !(wp & MK_LBUTTON)) return;
    if (msg != WM_LBUTTONDOWN && msg != WM_MOUSEMOVE) return;
    x = PC_CLAMP(x - 1, 0, 255); y = PC_CLAMP(y - 1, 0, 255);
    if (id == 100) {
        u32 col = field_color(c, x, y);
        switch (c->mode) {
        case 0: c->s = x / 255.0f; c->b = 1 - y / 255.0f; break;
        case 1: c->h = x / 255.0f * 360; c->b = 1 - y / 255.0f; break;
        case 2: c->h = x / 255.0f * 360; c->s = 1 - y / 255.0f; break;
        default: pick_set_rgb(c, pc_r(col), pc_g(col), pc_b(col)); break;
        }
    } else {
        float t = 1 - y / 255.0f;
        u32 cur = pick_rgb(c);
        switch (c->mode) {
        case 0: c->h = t * 360; if (c->h >= 360) c->h = 359.9f; break;
        case 1: c->s = t; break;
        case 2: c->b = t; break;
        case 3: pick_set_rgb(c, (int)(t * 255), pc_g(cur), pc_b(cur)); break;
        case 4: pick_set_rgb(c, pc_r(cur), (int)(t * 255), pc_b(cur)); break;
        default: pick_set_rgb(c, pc_r(cur), pc_g(cur), (int)(t * 255)); break;
        }
    }
    pick_fields(d);
}

static void pick_change(Dlg *d, int id, int code)
{
    PickCtx *c = (PickCtx *)d->user;
    if (c->busy) return;
    if (id >= 130 && id <= 135 && code == DLG_CODE_CLICK) { c->mode = id - 130; dlg_redraw(d, 100); dlg_redraw(d, 101); return; }
    if (code != DLG_CODE_CHANGE) return;
    if (id >= 110 && id <= 112) {
        c->h = (float)PC_CLAMP(dlg_get_int(d, 110), 0, 359); c->s = PC_CLAMP(dlg_get_int(d, 111), 0, 100) / 100.0f; c->b = PC_CLAMP(dlg_get_int(d, 112), 0, 100) / 100.0f;
    } else if (id >= 113 && id <= 115) {
        pick_set_rgb(c, dlg_get_int(d, 113), dlg_get_int(d, 114), dlg_get_int(d, 115));
    } else if (id >= 116 && id <= 118) {
        int r, g, b;
        pc_lab_to_rgb((float)PC_CLAMP(dlg_get_int(d, 116), 0, 100), (float)PC_CLAMP(dlg_get_int(d, 117), -128, 127), (float)PC_CLAMP(dlg_get_int(d, 118), -128, 127), &r, &g, &b);
        pick_set_rgb(c, r, g, b);
    } else if (id >= 119 && id <= 122) {
        int r, g, b;
        pc_cmyk_to_rgb(PC_CLAMP(dlg_get_int(d, 119), 0, 100), PC_CLAMP(dlg_get_int(d, 120), 0, 100), PC_CLAMP(dlg_get_int(d, 121), 0, 100), PC_CLAMP(dlg_get_int(d, 122), 0, 100), &r, &g, &b);
        pick_set_rgb(c, r, g, b);
    } else if (id == 123) {
        char hx[16];
        long v;
        dlg_get_text(d, 123, hx, sizeof hx);
        if (strlen(hx) != 6) return;
        v = strtol(hx, NULL, 16);
        pick_set_rgb(c, (int)((v >> 16) & 255), (int)((v >> 8) & 255), (int)(v & 255));
    } else return;
    {
        /* Update the other fields without fighting the one being typed in. */
        HWND f = GetFocus();
        int fid = f ? GetDlgCtrlID(f) : 0, k;
        char keep[16];
        dlg_get_text(d, fid, keep, sizeof keep);
        pick_fields(d);
        if (fid >= 110 && fid <= 123) { c->busy = 1; dlg_set_text(d, fid, keep); c->busy = 0; SendMessageA(f, EM_SETSEL, 15, 15); }
        (void)k;
    }
}

int dlg_color_picker(const char *title, u32 *color)
{
    static const char *const labels[] = { "H:", "S:", "B:", "R:", "G:", "B:" };
    static const char *const sfx[] = { "\xB0", "%", "%", "", "", "" };
    Dlg *d;
    PickCtx c;
    int rc, W = 520, k;
    memset(&c, 0, sizeof c);
    c.orig = *color;
    pick_set_rgb(&c, pc_r(*color), pc_g(*color), pc_b(*color));
    d = dlg_create(title, W, 290, pick_change, &c);
    if (!d) return 0;
    dlg_custom(d, 100, 10, 12, 260, 260, pick_paint_field, pick_mouse);
    dlg_custom(d, 101, 276, 11, 36, 260, pick_paint_strip, pick_mouse);
    dlg_custom(d, 102, 322, 14, 60, 60, pick_paint_swatch, NULL);
    for (k = 0; k < 6; k++) {
        int y = 96 + k * 24;
        dlg_radio(d, 130 + k, 322, y + 2, 32, 16, labels[k], k == 0, k == 0);
        dlg_edit(d, 110 + k, 356, y, 40, 20, "", DLG_NUMBER);
        dlg_label(d, 400, y + 3, 14, 16, sfx[k]);
    }
    {
        static const char *const lab[3] = { "L:", "a:", "b:" }, *const cm[4] = { "C:", "M:", "Y:", "K:" };
        for (k = 0; k < 3; k++) { dlg_label(d, 420, 98 + k * 24, 16, 16, lab[k]); dlg_edit(d, 116 + k, 438, 96 + k * 24, 40, 20, "", DLG_NUMBER | DLG_SIGNED); }
        for (k = 0; k < 4; k++) { dlg_label(d, 420, 172 + k * 24, 16, 16, cm[k]); dlg_edit(d, 119 + k, 438, 170 + k * 24, 40, 20, "", DLG_NUMBER); dlg_label(d, 482, 173 + k * 24, 14, 16, "%"); }
    }
    dlg_label(d, 322, 248, 12, 16, "#");
    dlg_edit(d, 123, 336, 245, 60, 20, "", 0);
    dlg_button(d, IDOK, W - 74, 10, BW, BH, "OK", 1);
    dlg_button(d, IDCANCEL, W - 74, 38, BW, BH, "Cancel", 0);
    pick_fields(d);
    rc = dlg_run(d);
    if (rc == IDOK) *color = pick_rgb(&c);
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- Preferences (PS5: page combo with Prev/Next) ---------------- */

static const char *const k_pref_pages[] = { "General", "Saving Files", "Display & Cursors", "Transparency", "Units & Rulers", "Guides & Grid", "Memory & History" };
#define NPAGES 7
typedef struct { int page; HWND ctl[NPAGES][16]; int nctl[NPAGES]; } PrefCtx;

static void pref_show(Dlg *d)
{
    PrefCtx *c = (PrefCtx *)d->user;
    int p, k;
    for (p = 0; p < NPAGES; p++) for (k = 0; k < c->nctl[p]; k++) ShowWindow(c->ctl[p][k], p == c->page ? SW_SHOW : SW_HIDE);
    dlg_set_combo(d, 100, c->page);
}

static void pref_change(Dlg *d, int id, int code)
{
    PrefCtx *c = (PrefCtx *)d->user;
    if (id == 100 && code == DLG_CODE_CHANGE) { c->page = PC_CLAMP(dlg_get_combo(d, 100), 0, NPAGES - 1); pref_show(d); }
    if (id == 101 && code == DLG_CODE_CLICK) { c->page = (c->page + NPAGES - 1) % NPAGES; pref_show(d); }
    if (id == 102 && code == DLG_CODE_CLICK) { c->page = (c->page + 1) % NPAGES; pref_show(d); }
}

int dlg_preferences(void)
{
    static const char *const interp[] = { "Nearest Neighbor", "Bilinear", "Bicubic" };
    static const char *const cursors[] = { "Standard", "Precise", "Brush Size" };
    static const char *const grid[] = { "Small", "Medium", "Large" };
    static const char *const units[] = { "pixels", "inches", "cm" };
    static const char *const quality[] = { "0 (Low)", "3 (Low)", "5 (Medium)", "8 (High)", "10 (Maximum)", "12 (Maximum)" };
    static const int qvals[] = { 0, 3, 5, 8, 10, 12 };
    Dlg *d;
    PrefCtx c;
    Prefs *p = &g_app.prefs;
    int rc, W = 440, q = 3, k;
    char b[16];
    memset(&c, 0, sizeof c);
    d = dlg_create("Preferences", W, 250, pref_change, &c);
    if (!d) return 0;
    dlg_combo(d, 100, 10, 10, 170, 20, k_pref_pages, NPAGES, 0);
#define ADD(pg, h) (c.ctl[pg][c.nctl[pg]++] = (h))
    /* General */
    ADD(0, dlg_label(d, 14, 44, 90, 16, "Interpolation:"));
    ADD(0, dlg_combo(d, 110, 110, 41, 140, 20, interp, 3, p->interpolation));
    ADD(0, dlg_group(d, 10, 72, 330, 150, "Options"));
    ADD(0, dlg_check(d, 111, 22, 92, 150, 16, "&Export Clipboard", p->export_clipboard));
    ADD(0, dlg_check(d, 112, 22, 112, 150, 16, "&Beep When Done", p->beep_when_done));
    ADD(0, dlg_check(d, 113, 22, 132, 300, 16, "Windows &98-style gradient title bars", p->gradient_captions));
    ADD(0, dlg_check(d, 114, 22, 152, 300, 16, "Open documents as &tabs (CS6); off = floating windows", p->tab_docs));
    ADD(0, dlg_check(d, 115, 22, 172, 300, 16, "Check for &updates at startup", reg_get_int("Update", "CheckOnStartup", 1)));
    ADD(0, dlg_check(d, 116, 22, 192, 300, 16, "Use &classic Open and Save dialogs", p->classic_file_dialogs));
    /* Saving Files */
    for (k = 0; k < 6; k++) if (qvals[k] <= p->jpeg_quality) q = k;
    ADD(1, dlg_label(d, 14, 44, 140, 16, "Default JPEG quality:"));
    ADD(1, dlg_combo(d, 120, 160, 41, 120, 20, quality, 6, q));
    ADD(1, dlg_label(d, 14, 74, 320, 40, "Layered images can only be saved as Photoshop (PSD). Other formats save a flattened copy."));
    /* Display & Cursors */
    ADD(2, dlg_group(d, 10, 40, 200, 90, "Painting Cursors"));
    ADD(2, dlg_radio(d, 130, 22, 58, 160, 16, "&Standard", !p->precise_cursors && !p->brush_size_cursor, 1));
    ADD(2, dlg_radio(d, 131, 22, 78, 160, 16, "&Precise", p->precise_cursors, 0));
    ADD(2, dlg_radio(d, 132, 22, 98, 160, 16, "&Brush Size", p->brush_size_cursor && !p->precise_cursors, 0));
    (void)cursors;
    /* Transparency */
    ADD(3, dlg_label(d, 14, 44, 80, 16, "Grid Size:"));
    ADD(3, dlg_combo(d, 140, 100, 41, 100, 20, grid, 3, PC_CLAMP(p->checker_size, 0, 2)));
    /* Units & Rulers */
    ADD(4, dlg_label(d, 14, 44, 80, 16, "Rulers:"));
    ADD(4, dlg_combo(d, 150, 100, 41, 100, 20, units, 3, PC_CLAMP(p->units, 0, 2)));
    /* Guides & Grid */
    ADD(5, dlg_label(d, 14, 44, 120, 16, "Gridline every:"));
    pc_snprintf(b, sizeof b, "%d", p->grid_spacing);
    ADD(5, dlg_edit(d, 160, 120, 41, 60, 20, b, DLG_NUMBER));
    ADD(5, dlg_label(d, 186, 44, 50, 16, "pixels"));
    /* Memory & History */
    ADD(6, dlg_label(d, 14, 44, 120, 16, "History States:"));
    pc_snprintf(b, sizeof b, "%d", p->history_states);
    ADD(6, dlg_edit(d, 170, 120, 41, 50, 20, b, DLG_NUMBER));
    ADD(6, dlg_label(d, 14, 74, 320, 40, "Each state shares unchanged pixels with the others, so many states cost little memory."));
#undef ADD
    ok_cancel(d, W);
    dlg_button(d, 101, bx(W), 76, BW, BH, "&Prev", 0);
    dlg_button(d, 102, bx(W), 104, BW, BH, "&Next", 0);
    pref_show(d);
    rc = dlg_run(d);
    if (rc == IDOK) {
        p->interpolation = dlg_get_combo(d, 110);
        p->export_clipboard = dlg_get_check(d, 111);
        p->beep_when_done = dlg_get_check(d, 112);
        p->gradient_captions = dlg_get_check(d, 113);
        p->tab_docs = dlg_get_check(d, 114);
        reg_set_int("Update", "CheckOnStartup", dlg_get_check(d, 115));
        p->classic_file_dialogs = dlg_get_check(d, 116);
        p->jpeg_quality = qvals[PC_CLAMP(dlg_get_combo(d, 120), 0, 5)];
        p->precise_cursors = dlg_get_check(d, 131);
        p->brush_size_cursor = dlg_get_check(d, 132);
        p->checker_size = dlg_get_combo(d, 140);
        p->units = dlg_get_combo(d, 150);
        p->grid_spacing = PC_CLAMP(dlg_get_int(d, 160), 1, 1000);
        p->history_states = PC_CLAMP(dlg_get_int(d, 170), 1, 100);
        for (k = 0; k < g_app.nviews; k++) g_app.views[k]->doc->hist_limit = p->history_states;
        g_w95_gradient_captions = p->gradient_captions;
        settings_save();
        RedrawWindow(g_app.main, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        app_refresh(R_ALL);
    }
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- layer dialogs ---------------- */

int dlg_layer_props(char *name, int n, int *opacity, int *blend, int *clip)
{
    Dlg *d = dlg_create("Layer Options", 400, 130, NULL, NULL);
    int rc;
    char b[16];
    if (!d) return 0;
    dlg_label(d, 12, 15, 40, 16, "Name:");
    dlg_edit(d, 100, 56, 12, 240, 20, name, 0);
    dlg_label(d, 12, 45, 50, 16, "Opacity:");
    pc_snprintf(b, sizeof b, "%d", *opacity);
    dlg_edit(d, 101, 66, 42, 40, 20, b, DLG_NUMBER);
    dlg_label(d, 110, 45, 14, 16, "%");
    dlg_label(d, 130, 45, 40, 16, "Mode:");
    dlg_combo(d, 102, 170, 42, 126, 20, k_blends, PC_BLEND_COUNT, *blend);
    dlg_check(d, 103, 12, 76, 160, 16, "&Group With Previous Layer", *clip);
    ok_cancel(d, 400);
    rc = dlg_run(d);
    if (rc == IDOK) {
        dlg_get_text(d, 100, name, n);
        *opacity = PC_CLAMP(dlg_get_int(d, 101), 0, 100);
        *blend = dlg_get_combo(d, 102);
        *clip = dlg_get_check(d, 103);
    }
    dlg_destroy(d);
    return rc == IDOK;
}

int dlg_new_layer(char *name, int n, int *opacity, int *blend, int *clip)
{
    return dlg_layer_props(name, n, opacity, blend, clip);
}

int dlg_duplicate(char *name, int n, int merged_option, int *merged)
{
    Dlg *d = dlg_create("Duplicate Image", 360, 90, NULL, NULL);
    int rc;
    if (!d) return 0;
    dlg_label(d, 12, 15, 40, 16, "As:");
    dlg_edit(d, 100, 50, 12, 200, 20, name, 0);
    if (merged_option) dlg_check(d, 101, 50, 42, 180, 16, "&Merged Layers Only", 0);
    ok_cancel(d, 360);
    rc = dlg_run(d);
    if (rc == IDOK) { dlg_get_text(d, 100, name, n); *merged = merged_option && dlg_get_check(d, 101); }
    dlg_destroy(d);
    return rc == IDOK;
}

/* Layer Style: PS5 "Effects" dialog - an effect selector with an Apply box. */
typedef struct { PcDoc *doc; PcLayer *l; PcLayer saved; int fx; DlgPreview pv; } FxCtx;

static void fx_load(Dlg *d)
{
    FxCtx *c = (FxCtx *)d->user;
    PcLayer *l = c->l;
    int on = 0, op = 0, a = 0, dist = 0, blur = 0;
    switch (c->fx) {
    case 0: on = l->fx_shadow; op = l->fx_shadow_opacity; a = l->fx_shadow_angle; dist = l->fx_shadow_dist; blur = l->fx_shadow_blur; break;
    case 1: on = l->fx_inner_shadow; op = l->fx_inner_opacity; a = 120; dist = l->fx_inner_dist; blur = l->fx_inner_blur; break;
    case 2: on = l->fx_glow; op = l->fx_glow_opacity; a = 0; dist = 0; blur = l->fx_glow_size; break;
    default: on = l->fx_bevel; op = 255; a = l->fx_bevel_angle; dist = l->fx_bevel_depth; blur = l->fx_bevel_size; break;
    }
    dlg_set_check(d, 101, on);
    dlg_set_int(d, 102, (op * 100 + 127) / 255);
    dlg_set_int(d, 103, a); dlg_set_int(d, 104, dist); dlg_set_int(d, 105, blur);
    dlg_enable(d, 102, c->fx != 3); dlg_enable(d, 103, c->fx == 0 || c->fx == 3);
    dlg_set_text(d, 114, c->fx == 3 ? "Depth:" : "Distance:");
    dlg_set_text(d, 115, c->fx == 2 || c->fx == 3 ? "Size:" : "Blur:");
}

static void fx_store(Dlg *d)
{
    FxCtx *c = (FxCtx *)d->user;
    PcLayer *l = c->l;
    int on = dlg_get_check(d, 101), op = PC_CLAMP(dlg_get_int(d, 102), 0, 100) * 255 / 100;
    int a = dlg_get_int(d, 103), dist = PC_CLAMP(dlg_get_int(d, 104), 0, 100), blur = PC_CLAMP(dlg_get_int(d, 105), 0, 64);
    switch (c->fx) {
    case 0: l->fx_shadow = on; l->fx_shadow_opacity = op; l->fx_shadow_angle = a; l->fx_shadow_dist = dist; l->fx_shadow_blur = blur; break;
    case 1: l->fx_inner_shadow = on; l->fx_inner_opacity = op; l->fx_inner_dist = dist; l->fx_inner_blur = blur; break;
    case 2: l->fx_glow = on; l->fx_glow_opacity = op; l->fx_glow_size = PC_MAX(1, blur); break;
    default: l->fx_bevel = on; l->fx_bevel_angle = a; l->fx_bevel_depth = PC_MAX(1, dist); l->fx_bevel_size = PC_MAX(1, blur); break;
    }
    l->fx_enabled = l->fx_shadow || l->fx_inner_shadow || l->fx_glow || l->fx_bevel;
    pc_doc_touch(c->doc);
    app_refresh(R_CANVAS);
}

static void fx_change(Dlg *d, int id, int code)
{
    FxCtx *c = (FxCtx *)d->user;
    if (id == 100 && code == DLG_CODE_CHANGE) { fx_store(d); c->fx = dlg_get_combo(d, 100); fx_load(d); return; }
    if (id == 110 && code == DLG_CODE_CLICK) { fx_store(d); c->fx = (c->fx + 3) % 4; dlg_set_combo(d, 100, c->fx); fx_load(d); return; }
    if (id == 111 && code == DLG_CODE_CLICK) { fx_store(d); c->fx = (c->fx + 1) % 4; dlg_set_combo(d, 100, c->fx); fx_load(d); return; }
    if (id == 120 && code == DLG_CODE_CLICK) {
        u32 col = c->fx == 2 ? c->l->fx_glow_color : c->l->fx_shadow_color;
        if (dlg_color_picker("Effect Color", &col)) { if (c->fx == 2) c->l->fx_glow_color = col; else c->l->fx_shadow_color = col; fx_store(d); }
        return;
    }
    if ((id >= 101 && id <= 105) && (code == DLG_CODE_CHANGE || code == DLG_CODE_CLICK)) fx_store(d);
}

int dlg_layer_style(PcDoc *doc, PcLayer *l)
{
    static const char *const fx[] = { "Drop Shadow", "Inner Shadow", "Outer Glow", "Bevel and Emboss" };
    Dlg *d;
    FxCtx c;
    int rc, W = 380;
    memset(&c, 0, sizeof c);
    c.doc = doc; c.l = l; c.saved = *l;
    c.fx = l->fx_bevel && !l->fx_shadow ? 3 : l->fx_glow && !l->fx_shadow ? 2 : l->fx_inner_shadow && !l->fx_shadow ? 1 : 0;
    d = dlg_create("Effects", W, 210, fx_change, &c);
    if (!d) return 0;
    dlg_combo(d, 100, 12, 12, 150, 20, fx, 4, c.fx);
    dlg_check(d, 101, 172, 14, 80, 16, "&Apply", 1);
    dlg_label(d, 14, 52, 70, 16, "Opacity:");
    dlg_edit(d, 102, 90, 49, 44, 20, "", DLG_NUMBER);
    dlg_label(d, 138, 52, 14, 16, "%");
    dlg_button(d, 120, 170, 48, 70, 22, "&Color...", 0);
    dlg_label(d, 14, 82, 70, 16, "Angle:");
    dlg_edit(d, 103, 90, 79, 44, 20, "", DLG_NUMBER | DLG_SIGNED);
    dlg_label(d, 138, 82, 14, 16, "\xB0");
    dlg_label_id(d, 114, 14, 112, 70, 16, "Distance:", 0);
    dlg_edit(d, 104, 90, 109, 44, 20, "", DLG_NUMBER);
    dlg_label(d, 138, 112, 40, 16, "pixels");
    dlg_label_id(d, 115, 14, 142, 70, 16, "Blur:", 0);
    dlg_edit(d, 105, 90, 139, 44, 20, "", DLG_NUMBER);
    dlg_label(d, 138, 142, 40, 16, "pixels");
    ok_cancel(d, W);
    dlg_button(d, 110, bx(W), 76, BW, BH, "&Prev", 0);
    dlg_button(d, 111, bx(W), 104, BW, BH, "&Next", 0);
    fx_load(d);
    fx_store(d);
    rc = dlg_run(d);
    if (rc == IDOK) fx_store(d);
    else { PcBuf *pix = l->pix, *mask = l->mask; *l = c.saved; l->pix = pix; l->mask = mask; pc_doc_touch(doc); app_refresh(R_CANVAS); }
    dlg_destroy(d);
    return rc == IDOK;
}

int dlg_transform_numeric(float *dx, float *dy, float *sw, float *sh, float *angle, float *skx, float *sky)
{
    Dlg *d = dlg_create("Numeric Transform", 380, 230, NULL, NULL);
    int rc;
    if (!d) return 0;
    dlg_group(d, 10, 8, 280, 54, "Position");
    dlg_label(d, 22, 30, 14, 16, "X:"); dlg_edit(d, 100, 38, 27, 60, 20, "0", DLG_NUMBER | DLG_DECIMAL | DLG_SIGNED);
    dlg_label(d, 120, 30, 14, 16, "Y:"); dlg_edit(d, 101, 136, 27, 60, 20, "0", DLG_NUMBER | DLG_DECIMAL | DLG_SIGNED);
    dlg_group(d, 10, 68, 280, 54, "Scale");
    dlg_label(d, 22, 90, 40, 16, "Width:"); dlg_edit(d, 102, 64, 87, 50, 20, "100", DLG_NUMBER | DLG_DECIMAL); dlg_label(d, 118, 90, 14, 16, "%");
    dlg_label(d, 140, 90, 40, 16, "Height:"); dlg_edit(d, 103, 184, 87, 50, 20, "100", DLG_NUMBER | DLG_DECIMAL); dlg_label(d, 238, 90, 14, 16, "%");
    dlg_group(d, 10, 128, 280, 54, "Skew");
    dlg_label(d, 22, 150, 50, 16, "Horiz:"); dlg_edit(d, 104, 64, 147, 50, 20, "0", DLG_NUMBER | DLG_DECIMAL | DLG_SIGNED);
    dlg_label(d, 140, 150, 40, 16, "Vert:"); dlg_edit(d, 105, 184, 147, 50, 20, "0", DLG_NUMBER | DLG_DECIMAL | DLG_SIGNED);
    dlg_label(d, 22, 196, 40, 16, "Rotate:"); dlg_edit(d, 106, 70, 193, 50, 20, "0", DLG_NUMBER | DLG_DECIMAL | DLG_SIGNED); dlg_label(d, 124, 196, 14, 16, "\xB0");
    ok_cancel(d, 380);
    rc = dlg_run(d);
    if (rc == IDOK) {
        *dx = (float)dlg_get_double(d, 100); *dy = (float)dlg_get_double(d, 101);
        *sw = (float)PC_CLAMP(dlg_get_double(d, 102), 1, 10000); *sh = (float)PC_CLAMP(dlg_get_double(d, 103), 1, 10000);
        *skx = (float)PC_CLAMP(dlg_get_double(d, 104), -89, 89); *sky = (float)PC_CLAMP(dlg_get_double(d, 105), -89, 89);
        *angle = (float)dlg_get_double(d, 106);
    }
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- Edit dialogs ---------------- */

int dlg_fill(int *use, int *opacity, int *blend, int *preserve)
{
    static const char *const uses[] = { "Foreground Color", "Background Color", "Pattern", "Saved/History", "Black", "50% Gray", "White" };
    Dlg *d = dlg_create("Fill", 360, 160, NULL, NULL);
    int rc;
    char b[16];
    if (!d) return 0;
    dlg_group(d, 10, 8, 260, 48, "Contents");
    dlg_label(d, 22, 28, 30, 16, "Use:");
    dlg_combo(d, 100, 56, 25, 180, 20, uses, 7, *use);
    dlg_group(d, 10, 62, 260, 88, "Blending");
    dlg_label(d, 22, 84, 50, 16, "Opacity:");
    pc_snprintf(b, sizeof b, "%d", *opacity);
    dlg_edit(d, 101, 76, 81, 40, 20, b, DLG_NUMBER);
    dlg_label(d, 120, 84, 14, 16, "%");
    dlg_label(d, 140, 84, 30, 16, "Mode:");
    dlg_combo(d, 102, 174, 81, 90, 20, k_blends, PC_BLEND_COUNT, *blend);
    dlg_check(d, 103, 22, 114, 180, 16, "Preserve &Transparency", *preserve);
    ok_cancel(d, 360);
    rc = dlg_run(d);
    if (rc == IDOK) { *use = dlg_get_combo(d, 100); *opacity = PC_CLAMP(dlg_get_int(d, 101), 1, 100); *blend = dlg_get_combo(d, 102); *preserve = dlg_get_check(d, 103); }
    dlg_destroy(d);
    return rc == IDOK;
}

int dlg_stroke(int *width, int *location, int *opacity, int *blend)
{
    Dlg *d = dlg_create("Stroke", 360, 200, NULL, NULL);
    int rc;
    char b[16];
    if (!d) return 0;
    dlg_group(d, 10, 8, 260, 48, "Stroke");
    dlg_label(d, 22, 28, 40, 16, "Width:");
    pc_snprintf(b, sizeof b, "%d", *width);
    dlg_edit(d, 100, 64, 25, 40, 20, b, DLG_NUMBER);
    dlg_label(d, 108, 28, 40, 16, "pixels");
    dlg_group(d, 10, 62, 260, 42, "Location");
    dlg_radio(d, 110, 22, 80, 70, 16, "&Inside", *location == 0, 1);
    dlg_radio(d, 111, 96, 80, 70, 16, "&Center", *location == 1, 0);
    dlg_radio(d, 112, 170, 80, 80, 16, "&Outside", *location == 2, 0);
    dlg_group(d, 10, 110, 260, 52, "Blending");
    dlg_label(d, 22, 132, 50, 16, "Opacity:");
    pc_snprintf(b, sizeof b, "%d", *opacity);
    dlg_edit(d, 101, 76, 129, 40, 20, b, DLG_NUMBER);
    dlg_label(d, 120, 132, 14, 16, "%");
    dlg_combo(d, 102, 140, 129, 120, 20, k_blends, PC_BLEND_COUNT, *blend);
    ok_cancel(d, 360);
    rc = dlg_run(d);
    if (rc == IDOK) {
        *width = PC_CLAMP(dlg_get_int(d, 100), 1, 16);
        *location = dlg_get_check(d, 110) ? 0 : dlg_get_check(d, 112) ? 2 : 1;
        *opacity = PC_CLAMP(dlg_get_int(d, 101), 1, 100);
        *blend = dlg_get_combo(d, 102);
    }
    dlg_destroy(d);
    return rc == IDOK;
}

int dlg_feather(float *radius)
{
    Dlg *d = dlg_create("Feather Selection", 330, 70, NULL, NULL);
    int rc;
    char b[16];
    if (!d) return 0;
    dlg_label(d, 12, 18, 96, 16, "Feather Radius:");
    pc_snprintf(b, sizeof b, "%.1f", *radius);
    dlg_edit(d, 100, 108, 15, 50, 20, b, DLG_NUMBER | DLG_DECIMAL);
    dlg_label(d, 164, 18, 40, 16, "pixels");
    ok_cancel(d, 330);
    rc = dlg_run(d);
    if (rc == IDOK) *radius = (float)PC_CLAMP(dlg_get_double(d, 100), 0.2, 250);
    dlg_destroy(d);
    return rc == IDOK;
}

int dlg_modify_sel(const char *title, const char *label, int *px)
{
    Dlg *d = dlg_create(title, 320, 70, NULL, NULL);
    int rc;
    char b[16];
    if (!d) return 0;
    dlg_label(d, 12, 18, 90, 16, label);
    pc_snprintf(b, sizeof b, "%d", *px);
    dlg_edit(d, 100, 104, 15, 44, 20, b, DLG_NUMBER);
    dlg_label(d, 154, 18, 40, 16, "pixels");
    ok_cancel(d, 320);
    rc = dlg_run(d);
    if (rc == IDOK) *px = PC_CLAMP(dlg_get_int(d, 100), 1, 100);
    dlg_destroy(d);
    return rc == IDOK;
}

typedef struct { PcDoc *doc; u32 color; int fuzz; } CrCtx;
static void cr_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    CrCtx *c = (CrCtx *)d->user;
    PcBuf *comp = pc_doc_composite(c->doc);
    int x, y, w = r->right - r->left - 2, h = r->bottom - r->top - 2;
    (void)id;
    w95_fill(dc, r, W95_DKSHADOW);
    if (!comp) return;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const u8 *p = comp->px + ((size_t)(y * comp->h / h) * comp->w + x * comp->w / w) * 4;
            int dd = PC_MAX(abs(p[0] - pc_r(c->color)), PC_MAX(abs(p[1] - pc_g(c->color)), abs(p[2] - pc_b(c->color))));
            int v = c->fuzz ? 255 - dd * 255 / (c->fuzz + 1) : (dd ? 0 : 255);
            v = PC_CLAMP(v, 0, 255);
            w95_pixel(dc, r->left + 1 + x, r->top + 1 + y, RGB(v, v, v));
        }
    pc_buf_unref(comp);
}

static void cr_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    CrCtx *c = (CrCtx *)d->user;
    PcBuf *comp;
    (void)wp;
    if (msg != WM_LBUTTONDOWN) return;
    comp = pc_doc_composite(c->doc);
    if (comp) {
        int px = PC_CLAMP(x * comp->w / 180, 0, comp->w - 1), py = PC_CLAMP(y * comp->h / 180, 0, comp->h - 1);
        const u8 *p = comp->px + ((size_t)py * comp->w + px) * 4;
        c->color = pc_rgb(p[0], p[1], p[2]);
        pc_buf_unref(comp);
        dlg_redraw(d, id);
    }
}

static void cr_change(Dlg *d, int id, int code)
{
    CrCtx *c = (CrCtx *)d->user;
    if (id == 101 && code == DLG_CODE_SLIDER) { c->fuzz = dlg_slider_get(d, 101); }
    if (id == 101 && code == DLG_CODE_CLICK) dlg_redraw(d, 110);
    if (id == 100 && code == DLG_CODE_CHANGE) { c->fuzz = PC_CLAMP(dlg_get_int(d, 100), 0, 200); dlg_slider_set(d, 101, c->fuzz); dlg_redraw(d, 110); }
}

int dlg_color_range(PcDoc *doc, int *fuzz, u32 *color)
{
    Dlg *d;
    CrCtx c;
    int rc, W = 380;
    c.doc = doc; c.color = *color; c.fuzz = *fuzz;
    d = dlg_create("Color Range", W, 260, cr_change, &c);
    if (!d) return 0;
    dlg_label(d, 12, 15, 60, 16, "Fuzziness:");
    dlg_edit(d, 100, 230, 12, 44, 20, "", DLG_NUMBER);
    dlg_set_int(d, 100, *fuzz);
    dlg_slider(d, 101, 12, 36, 262, 0, 200, *fuzz, 100, 0);
    dlg_custom(d, 110, 52, 56, 182, 182, cr_paint, cr_mouse);
    dlg_label(d, 12, 242, 280, 14, "Click in the preview to sample a color.");
    ok_cancel(d, W);
    rc = dlg_run(d);
    if (rc == IDOK) { *fuzz = c.fuzz; *color = c.color; }
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- Type Tool dialog (Photoshop 5.0 style) ---------------- */

void fill_font_combo(HWND c);

typedef struct { PcText *t; } TypeCtx;

static void type_change(Dlg *d, int id, int code)
{
    TypeCtx *c = (TypeCtx *)d->user;
    if (id == 110 && code == DLG_CODE_CLICK) {
        u32 col = c->t->color;
        if (dlg_color_picker("Select Text Color:", &col)) { c->t->color = col; dlg_redraw(d, 111); }
    }
}

static void type_swatch(Dlg *d, int id, HDC dc, const RECT *r)
{
    TypeCtx *c = (TypeCtx *)d->user;
    RECT q = *r;
    (void)id;
    w95_sunken(dc, &q);
    InflateRect(&q, -2, -2);
    w95_fill(dc, &q, RGB(pc_r(c->t->color), pc_g(c->t->color), pc_b(c->t->color)));
}

int dlg_type(PcDoc *doc, PcText *t, int is_new)
{
    static const char *const aa[] = { "None", "Crisp", "Strong", "Smooth" };
    static const char *const align[] = { "Left", "Center", "Right" };
    static const char *const style[] = { "Regular", "Bold", "Italic", "Bold Italic" };
    Dlg *d;
    TypeCtx c;
    PcText work = *t;
    int rc, W = 470, k;
    char b[32];
    HWND fc;
    (void)doc;
    c.t = &work;
    d = dlg_create("Type Tool", W, 300, type_change, &c);
    if (!d) return 0;
    dlg_label(d, 12, 15, 36, 16, "Font:");
    fc = dlg_combo(d, 100, 50, 12, 170, 20, NULL, 0, 0);
    fill_font_combo(fc);
    SendMessageA(fc, CB_SETCURSEL, (WPARAM)PC_MAX(0, (int)SendMessageA(fc, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)work.font)), 0);
    dlg_combo(d, 101, 226, 12, 100, 20, style, 4, (work.bold ? 1 : 0) | (work.italic ? 2 : 0));
    dlg_label(d, 12, 45, 36, 16, "Size:");
    pc_snprintf(b, sizeof b, "%d", work.size);
    dlg_edit(d, 102, 50, 42, 44, 20, b, DLG_NUMBER);
    dlg_label(d, 98, 45, 40, 16, "pixels");
    dlg_label(d, 150, 45, 54, 16, "Leading:");
    pc_snprintf(b, sizeof b, "%d", work.leading);
    dlg_edit(d, 103, 204, 42, 40, 20, work.leading ? b : "", DLG_NUMBER);
    dlg_label(d, 254, 45, 54, 16, "Spacing:");
    pc_snprintf(b, sizeof b, "%d", work.tracking);
    dlg_edit(d, 104, 310, 42, 40, 20, b, DLG_NUMBER | DLG_SIGNED);
    dlg_label(d, 12, 75, 36, 16, "Color:");
    dlg_custom(d, 111, 50, 72, 40, 20, type_swatch, NULL);
    dlg_button(d, 110, 96, 71, 60, 22, "Change...", 0);
    dlg_label(d, 170, 75, 60, 16, "Anti-Alias:");
    dlg_combo(d, 105, 232, 72, 80, 20, aa, 4, PC_CLAMP(work.antialias, 0, 3));
    dlg_label(d, 320, 75, 40, 16, "Align:");
    dlg_combo(d, 106, 360, 72, 80, 20, align, 3, PC_CLAMP(work.align, 0, 2));
    dlg_check(d, 107, 12, 100, 120, 16, "&Underline", work.underline);
    dlg_check(d, 108, 140, 100, 120, 16, "&Vertical", work.vertical);
    {
        /* Text box rendered in the chosen font (PS5 showed the typeface). */
        HWND e;
        char txt[1024];
        int i, j = 0;
        for (i = 0; work.text[i] && j < 1020; i++) { if (work.text[i] == '\n' && (i == 0 || work.text[i - 1] != '\r')) txt[j++] = '\r'; txt[j++] = work.text[i]; }
        txt[j] = 0;
        e = dlg_edit(d, 120, 12, 124, W - 24, 164, txt, DLG_MULTILINE);
        if (e) {
            static HFONT pf;
            if (pf) DeleteObject(pf);
            pf = CreateFontA(-PC_CLAMP(work.size, 9, 28), 0, 0, 0, work.bold ? FW_BOLD : FW_NORMAL, (BYTE)work.italic, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, work.font);
            if (pf) SendMessageA(e, WM_SETFONT, (WPARAM)pf, 0);
        }
    }
    dlg_button(d, IDOK, W - 74, 10, BW, BH, "OK", 0);
    dlg_button(d, IDCANCEL, W - 74, 38, BW, BH, "Cancel", 0);
    rc = dlg_run(d);
    if (rc == IDOK) {
        char txt[1024];
        int i, j = 0;
        k = (int)SendMessageA(fc, CB_GETCURSEL, 0, 0);
        if (k >= 0) SendMessageA(fc, CB_GETLBTEXT, (WPARAM)k, (LPARAM)work.font);
        k = dlg_get_combo(d, 101);
        work.bold = k & 1; work.italic = (k & 2) != 0;
        work.size = PC_CLAMP(dlg_get_int(d, 102), 1, 1296);
        work.leading = PC_CLAMP(dlg_get_int(d, 103), 0, 5000);
        work.tracking = PC_CLAMP(dlg_get_int(d, 104), -50, 500);
        work.antialias = dlg_get_combo(d, 105);
        work.align = dlg_get_combo(d, 106);
        work.underline = dlg_get_check(d, 107);
        work.vertical = dlg_get_check(d, 108);
        dlg_get_text(d, 120, txt, sizeof txt);
        for (i = 0; txt[i] && j < (int)sizeof work.text - 1; i++) if (txt[i] != '\r') work.text[j++] = txt[i];
        work.text[j] = 0;
        *t = work;
        options_set_type(t);
    }
    dlg_destroy(d);
    (void)is_new;
    return rc == IDOK;
}

/* ---------------- Brush Options ---------------- */

int dlg_brush_options(PcBrush *b)
{
    Dlg *d = dlg_create("Brush Options", 380, 190, NULL, NULL);
    int rc;
    if (!d) return 0;
    dlg_label(d, 12, 15, 60, 16, "Diameter:");
    dlg_edit(d, 100, 230, 12, 44, 20, "", DLG_NUMBER); dlg_set_int(d, 100, b->diameter);
    dlg_slider(d, 110, 12, 36, 262, 1, 999, b->diameter, 100, 0);
    dlg_label(d, 12, 59, 60, 16, "Hardness:");
    dlg_edit(d, 101, 230, 56, 44, 20, "", DLG_NUMBER); dlg_set_int(d, 101, b->hardness);
    dlg_slider(d, 111, 12, 80, 262, 0, 100, b->hardness, 101, 0);
    dlg_label(d, 12, 103, 60, 16, "Spacing:");
    dlg_edit(d, 102, 230, 100, 44, 20, "", DLG_NUMBER); dlg_set_int(d, 102, b->spacing ? b->spacing : 25);
    dlg_slider(d, 112, 12, 124, 262, 1, 999, b->spacing ? b->spacing : 25, 102, 0);
    dlg_label(d, 12, 150, 40, 16, "Angle:");
    dlg_edit(d, 103, 56, 147, 44, 20, "", DLG_NUMBER | DLG_SIGNED); dlg_set_int(d, 103, b->angle);
    dlg_label(d, 120, 150, 70, 16, "Roundness:");
    dlg_edit(d, 104, 192, 147, 44, 20, "", DLG_NUMBER); dlg_set_int(d, 104, b->roundness ? b->roundness : 100);
    ok_cancel(d, 380);
    rc = dlg_run(d);
    if (rc == IDOK) {
        b->diameter = PC_CLAMP(dlg_get_int(d, 100), 1, 999);
        b->hardness = PC_CLAMP(dlg_get_int(d, 101), 0, 100);
        b->spacing = PC_CLAMP(dlg_get_int(d, 102), 1, 999);
        b->angle = PC_CLAMP(dlg_get_int(d, 103), -180, 180);
        b->roundness = PC_CLAMP(dlg_get_int(d, 104), 1, 100);
    }
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- Histogram ---------------- */

typedef struct { PcDoc *doc; unsigned h[4][256]; int ch; } HistCtx;

static void hist_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    HistCtx *c = (HistCtx *)d->user;
    unsigned mx = 1;
    int v, hh = r->bottom - r->top - 12;
    (void)id;
    w95_fill(dc, r, W95_WINDOW);
    w95_rect(dc, r->left, r->top, r->left + 258, r->top + hh, W95_DKSHADOW);
    for (v = 0; v < 256; v++) if (c->h[c->ch][v] > mx) mx = c->h[c->ch][v];
    for (v = 0; v < 256; v++) { int bh = (int)((double)c->h[c->ch][v] * (hh - 4) / mx); if (bh) w95_vline(dc, r->left + 1 + v, r->top + hh - 1 - bh, r->top + hh - 1, W95_TEXT); }
    for (v = 0; v < 256; v++) w95_vline(dc, r->left + 1 + v, r->top + hh + 2, r->top + hh + 10, RGB(v, v, v));
}

static void hist_stats(Dlg *d)
{
    HistCtx *c = (HistCtx *)d->user;
    double sum = 0, sq = 0, n = 0, mean, sd;
    int v, med = 0;
    char b[64];
    for (v = 0; v < 256; v++) { n += c->h[c->ch][v]; sum += (double)v * c->h[c->ch][v]; sq += (double)v * v * c->h[c->ch][v]; }
    mean = n ? sum / n : 0; sd = n ? sqrt(PC_MAX(0, sq / n - mean * mean)) : 0;
    { double acc = 0; for (v = 0; v < 256; v++) { acc += c->h[c->ch][v]; if (acc >= n / 2) { med = v; break; } } }
    pc_snprintf(b, sizeof b, "Mean: %.2f", mean); dlg_set_text(d, 110, b);
    pc_snprintf(b, sizeof b, "Std Dev: %.2f", sd); dlg_set_text(d, 111, b);
    pc_snprintf(b, sizeof b, "Median: %d", med); dlg_set_text(d, 112, b);
    pc_snprintf(b, sizeof b, "Pixels: %ld", (long)n); dlg_set_text(d, 113, b);
    dlg_redraw(d, 120);
}

static void hist_change(Dlg *d, int id, int code)
{
    HistCtx *c = (HistCtx *)d->user;
    if (id == 100 && code == DLG_CODE_CHANGE) { c->ch = PC_CLAMP(dlg_get_combo(d, 100), 0, 3); hist_stats(d); }
}

int dlg_histogram(PcDoc *doc)
{
    static const char *const ch[] = { "Luminosity", "Red", "Green", "Blue" };
    Dlg *d;
    HistCtx *c = (HistCtx *)calloc(1, sizeof *c);
    int k, W = 380;
    if (!c) return 0;
    c->doc = doc;
    for (k = 0; k < 4; k++) pc_histogram(doc, k, 1, c->h[k]);
    d = dlg_create("Histogram", W, 240, hist_change, c);
    if (!d) { free(c); return 0; }
    dlg_label(d, 12, 15, 56, 16, "Channel:");
    dlg_combo(d, 100, 68, 12, 110, 20, ch, doc->s.mode == PC_MODE_GRAY ? 1 : 4, 0);
    dlg_custom(d, 120, 12, 42, 262, 120, hist_paint, NULL);
    dlg_label_id(d, 110, 12, 172, 130, 14, "", 0);
    dlg_label_id(d, 111, 12, 188, 130, 14, "", 0);
    dlg_label_id(d, 112, 12, 204, 130, 14, "", 0);
    dlg_label_id(d, 113, 150, 172, 150, 14, "", 0);
    dlg_button(d, IDOK, bx(W), 10, BW, BH, "OK", 1);
    hist_stats(d);
    dlg_run(d);
    dlg_destroy(d);
    free(c);
    return 1;
}

/* ---------------- Page Setup ---------------- */

int dlg_print_setup(DocView *v)
{
    char list[2048], cur[128];
    const char *items[32];
    int n = 0, sel = 0, rc, k;
    Dlg *d;
    (void)v;
    /* Printers are listed in WIN.INI [Devices] on every Windows version. */
    GetProfileStringA("Devices", NULL, "", list, sizeof list);
    reg_get_str("Print", "Printer", cur, sizeof cur);
    {
        char *p = list;
        while (*p && n < 32) { items[n] = p; if (!pc_stricmp(p, cur)) sel = n; n++; p += strlen(p) + 1; }
    }
    d = dlg_create("Page Setup", 400, 150, NULL, NULL);
    if (!d) return 0;
    dlg_group(d, 10, 8, 300, 56, "Printer");
    dlg_label(d, 22, 30, 40, 16, "Name:");
    if (n) dlg_combo(d, 100, 66, 27, 230, 20, items, n, sel);
    else dlg_label(d, 66, 30, 230, 16, "(no printers installed)");
    dlg_group(d, 10, 70, 300, 66, "Options");
    dlg_check(d, 101, 22, 90, 200, 16, "&Center Image", reg_get_int("Print", "Center", 1));
    dlg_label(d, 22, 110, 280, 16, "Prints at the image resolution, reduced to fit the page.");
    ok_cancel(d, 400);
    rc = dlg_run(d);
    if (rc == IDOK) {
        if (n) { k = dlg_get_combo(d, 100); if (k >= 0 && k < n) reg_set_str("Print", "Printer", items[k]); }
        reg_set_int("Print", "Center", dlg_get_check(d, 101));
    }
    dlg_destroy(d);
    return rc == IDOK;
}

/* ---------------- About ---------------- */

static void about_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    RECT t = *r, band;
    (void)d; (void)id;
    SetRect(&band, r->left, r->top, r->right, r->top + 64);
    {
        int x;
        for (x = 0; x < band.right - band.left; x++) {
            int c = 128 - x * 128 / PC_MAX(1, band.right - band.left);
            w95_vline(dc, band.left + x, band.top, band.bottom, RGB(0, 0, 64 + c));
        }
    }
    DrawIconEx(dc, r->left + 12, r->top + 16, w95_app_icon(0), 32, 32, 0, NULL, DI_NORMAL);
    t.left += 56; t.top += 14;
    {
        HFONT f = CreateFontA(-22, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Times New Roman");
        HFONT o = (HFONT)SelectObject(dc, f);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutA(dc, t.left, t.top, "PhotoCraft Classic", 18);
        SelectObject(dc, o);
        DeleteObject(f);
    }
    t.top += 28;
    w95_text(dc, W95_FONT_NORMAL, "Version " PC_VERSION, &t, DT_LEFT | DT_SINGLELINE, RGB(255, 255, 255));
    t = *r; t.top += 76; t.left += 12; t.right -= 12;
    w95_text(dc, W95_FONT_NORMAL,
             "A 1998-style image editor for Windows 95 through Windows 10.\r\n\r\n"
             "Open source under the MIT or Apache 2.0 license. Image processing derived in part from "
             "PhotoCraft (github.com/storytold/photocraft). Includes stb (public domain), BearSSL (MIT) "
             "and Monocypher (BSD-2-Clause / CC0).\r\n\r\n"
             "PhotoCraft Classic is an independent project. It is not affiliated with or endorsed by Adobe Systems; "
             "Photoshop is a trademark of its owner and is mentioned only to describe the style recreated.",
             &t, DT_LEFT | DT_WORDBREAK, W95_TEXT);
}

int dlg_about(void)
{
    Dlg *d = dlg_create("About PhotoCraft Classic", 420, 270, NULL, NULL);
    if (!d) return 0;
    dlg_custom(d, 100, 0, 0, 420, 230, about_paint, NULL);
    dlg_button(d, IDOK, 172, 238, 75, 23, "OK", 1);
    dlg_run(d);
    dlg_destroy(d);
    return 1;
}

/* ---------------- "<Format> Options" and Indexed Color (the 1998 output rule) ----------------
 *
 * One dialog for every flat format: Color Depth radio buttons offer only
 * the depths a 1998 studio could write in that format (pc_format_depths),
 * then the Indexed Color controls of Photoshop 5 (Palette, Colors,
 * Dither), transparency and the sprite colour key, the format's own
 * options, and a 100% before/after preview of exactly what the file will
 * hold.  fmt PC_FMT_UNKNOWN is Image > Mode > Indexed Color.
 */

enum {
    EX_DEPTH = 110, EX_GREEN6 = 114, EX_PALETTE = 120, EX_COLORS = 121, EX_DITHER = 122, EX_TRANSP = 123,
    EX_PNGT = 124, EX_KEY = 125, EX_SWATCH = 126, EX_INTERLACE = 127, EX_RLE = 128, EX_TIFFC = 140,
    EX_QUAL = 150, EX_QCOMBO = 151, EX_QSLIDER = 152, EX_ORIG = 160, EX_RES = 161, EX_INFO = 162
};

typedef struct {
    const PcDoc *doc;
    int fmt;
    PcSaveOpts o;
    int ndepth, depths[4];
    int gray, docpal, transparent;
    int pw, ph, px, py;
    PcBuf *orig, *res;      /* full-size: composite and the converted result */
} ExpCtx;

static const char *depth_label(const ExpCtx *c, int depth)
{
    switch (depth) {
    case 8:
        if (c->fmt == PC_FMT_JPEG) return "&8 Bit Grayscale";
        return c->gray ? "&8 Bit Grayscale" : "&8 Bit Indexed (256 colors)";
    case 16: return c->fmt == PC_FMT_TGA ? "1&6 Bit High Color (1-5-5-5)" : "1&6 Bit High Color (5-5-5)";
    case 24: return "&24 Bit True Color";
    default: return c->fmt == PC_FMT_PNG ? "&32 Bit with Alpha (modern PNG)" : "&32 Bit with 8-bit Alpha";
    }
}

static int exp_depth(Dlg *d)
{
    ExpCtx *c = (ExpCtx *)d->user;
    int i;
    if (c->fmt == PC_FMT_UNKNOWN) return 8;
    for (i = 0; i < c->ndepth; i++) if (dlg_get_check(d, EX_DEPTH + i)) return c->depths[i];
    return c->ndepth ? c->depths[0] : 24;
}

static void exp_read(Dlg *d)
{
    ExpCtx *c = (ExpCtx *)d->user;
    PcSaveOpts *o = &c->o;
    o->depth = exp_depth(d);
    if (dlg_item(d, EX_GREEN6)) o->green6 = dlg_get_check(d, EX_GREEN6);
    if (dlg_item(d, EX_PALETTE)) {
        o->q.palette = PC_CLAMP(dlg_get_combo(d, EX_PALETTE), 0, PC_PAL_COUNT - 1);
        o->q.colors = PC_CLAMP(dlg_get_int(d, EX_COLORS), 2, 256);
        o->q.dither = PC_CLAMP(dlg_get_combo(d, EX_DITHER), 0, PC_DITHER_COUNT - 1);
    }
    if (dlg_item(d, EX_TRANSP)) o->q.transparency = dlg_get_check(d, EX_TRANSP);
    if (dlg_item(d, EX_PNGT)) {
        int t = dlg_get_combo(d, EX_PNGT);          /* None / Color key (1-bit) / Full alpha */
        o->q.transparency = t > 0;
        o->q.full_alpha = t == 2;
    }
    if (dlg_item(d, EX_KEY)) o->q.colorkey = dlg_get_check(d, EX_KEY);
    if (dlg_item(d, EX_INTERLACE)) o->interlace = dlg_get_check(d, EX_INTERLACE);
    if (dlg_item(d, EX_RLE)) o->tga_rle = dlg_get_check(d, EX_RLE);
    if (dlg_item(d, EX_TIFFC)) o->tiff_compress = dlg_get_check(d, EX_TIFFC + 1) ? 1 : dlg_get_check(d, EX_TIFFC + 2) ? 2 : 0;
    if (dlg_item(d, EX_QUAL)) o->jpeg_quality = PC_CLAMP(dlg_get_int(d, EX_QUAL), 0, 12);
}

/* Enable what the chosen depth and palette use. */
static void exp_sync(Dlg *d)
{
    ExpCtx *c = (ExpCtx *)d->user;
    int depth = exp_depth(d), idx = depth == 8 && !c->gray && c->fmt != PC_FMT_JPEG;
    int pal = dlg_item(d, EX_PALETTE) ? dlg_get_combo(d, EX_PALETTE) : 0;
    if (dlg_item(d, EX_PALETTE)) {
        dlg_enable(d, EX_PALETTE, idx);
        dlg_enable(d, EX_COLORS, idx && (pal == PC_PAL_UNIFORM || pal == PC_PAL_ADAPTIVE));
        dlg_enable(d, EX_DITHER, (idx && pal != PC_PAL_EXACT) || depth == 16);
    }
    if (dlg_item(d, EX_GREEN6)) dlg_enable(d, EX_GREEN6, depth == 16);
    if (dlg_item(d, EX_PNGT)) dlg_enable(d, EX_PNGT, depth == 8);
    if (dlg_item(d, EX_TRANSP) && c->fmt == PC_FMT_TGA) dlg_enable(d, EX_TRANSP, depth == 16);
    if (dlg_item(d, EX_TRANSP) && c->fmt == PC_FMT_UNKNOWN) dlg_enable(d, EX_TRANSP, c->transparent);
    if (dlg_item(d, EX_KEY)) { dlg_enable(d, EX_KEY, depth != 32); dlg_enable(d, EX_SWATCH, depth != 32); }
}

static void exp_crop(ExpCtx *c, const PcBuf *full, u8 *dst)
{
    int y;
    for (y = 0; y < c->ph; y++)
        memcpy(dst + (size_t)y * c->pw * 4, full->px + ((size_t)(c->py + y) * full->w + c->px) * 4, (size_t)c->pw * 4);
}

static void exp_preview(Dlg *d)
{
    ExpCtx *c = (ExpCtx *)d->user;
    char err[160] = "", info[96] = "";
    HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
    exp_read(d);
    pc_buf_unref(c->res);
    c->res = NULL;
    if (c->fmt == PC_FMT_UNKNOWN) {
        /* Indexed Color mode: the conversion pc_doc_to_indexed will make */
        PcBuf *b = pc_buf_clone(c->orig);
        PcIndexed ix;
        if (b && !pc_quantize(b->px, b->w, b->h, &c->o.q, c->doc->index_ncolors ? c->doc->index_pal : NULL,
                              c->doc->index_ncolors, c->doc->index_transparent, &ix)) {
            size_t i, np = (size_t)b->w * b->h;
            for (i = 0; i < np; i++) {
                u8 *p = b->px + i * 4;
                int e = ix.idx[i];
                p[0] = ix.pal[e * 3]; p[1] = ix.pal[e * 3 + 1]; p[2] = ix.pal[e * 3 + 2];
                p[3] = c->o.q.transparency ? ix.alpha[e] : 255;
            }
            pc_snprintf(info, sizeof info, "%d colors", ix.ncolors);
            pc_indexed_free(&ix);
            c->res = b;
        } else {
            pc_buf_unref(b);
        }
    } else {
        c->res = pc_save_preview(c->doc, c->fmt, &c->o, err, sizeof err);
        if (c->res) {
            int depth = c->o.depth ? c->o.depth : pc_format_default_depth(c->fmt, c->gray, c->transparent);
            if (c->fmt == PC_FMT_JPEG) pc_snprintf(info, sizeof info, "Baseline JPEG, %s", depth == 8 ? "grayscale" : "4:2:0");
            else if (depth == 8) {
                int n = pc_count_colors(c->res->px, (size_t)c->res->w * c->res->h, 256);
                pc_snprintf(info, sizeof info, "%s%d colors", n > 256 ? "> " : "", PC_MIN(n, 256));
            } else if (depth == 16) pc_snprintf(info, sizeof info, "High Color (%s colors)", c->o.green6 && c->fmt == PC_FMT_BMP ? "65,536" : "32,768");
            else pc_snprintf(info, sizeof info, depth == 32 ? "True Color + 8-bit alpha" : "True Color (16.7 million)");
        } else {
            pc_strlcpy(info, err, sizeof info);
        }
    }
    SetCursor(old);
    dlg_set_text(d, EX_INFO, info);
    dlg_redraw(d, EX_RES);
}

static void exp_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    ExpCtx *c = (ExpCtx *)d->user;
    const PcBuf *full = id == EX_ORIG ? c->orig : c->res;
    BITMAPINFO bi;
    u8 *px, *bgrx;
    int i, n = c->pw * c->ph;
    w95_fill(dc, r, W95_WINDOW);
    w95_sunken(dc, r);
    if (!full) return;
    px = (u8 *)malloc((size_t)n * 4);
    bgrx = (u8 *)malloc((size_t)n * 4);
    if (px && bgrx) {
        exp_crop(c, full, px);
        for (i = 0; i < n; i++) {
            const u8 *p = px + i * 4;
            int a = p[3], bg = ((((i % c->pw) >> 3) ^ ((i / c->pw) >> 3)) & 1) ? 204 : 255;
            bgrx[i * 4] = (u8)((p[2] * a + bg * (255 - a)) / 255); bgrx[i * 4 + 1] = (u8)((p[1] * a + bg * (255 - a)) / 255);
            bgrx[i * 4 + 2] = (u8)((p[0] * a + bg * (255 - a)) / 255); bgrx[i * 4 + 3] = 0;
        }
        memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = c->pw; bi.bmiHeader.biHeight = -c->ph;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(dc, r->left + 2, r->top + 2, (DWORD)c->pw, (DWORD)c->ph, 0, 0, 0, (UINT)c->ph, bgrx, &bi, DIB_RGB_COLORS);
    }
    free(px); free(bgrx);
}

static void exp_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    /* drag either pane to look at another part of the image (both follow) */
    ExpCtx *c = (ExpCtx *)d->user;
    static int lx, ly;
    (void)id;
    if (msg == WM_LBUTTONDOWN) { lx = x; ly = y; }
    if (msg == WM_MOUSEMOVE && (wp & MK_LBUTTON)) {
        c->px = PC_CLAMP(c->px - (x - lx), 0, PC_MAX(0, c->doc->s.w - c->pw));
        c->py = PC_CLAMP(c->py - (y - ly), 0, PC_MAX(0, c->doc->s.h - c->ph));
        lx = x; ly = y;
        dlg_redraw(d, EX_ORIG);
        dlg_redraw(d, EX_RES);
    }
}

static void exp_swatch_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    ExpCtx *c = (ExpCtx *)d->user;
    RECT in = *r;
    u32 k = c->o.q.key_rgb;
    (void)id;
    w95_sunken(dc, r);
    InflateRect(&in, -2, -2);
    w95_fill(dc, &in, RGB(pc_r(k), pc_g(k), pc_b(k)));
}

static void exp_swatch_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    ExpCtx *c = (ExpCtx *)d->user;
    u32 k = c->o.q.key_rgb;
    (void)id; (void)x; (void)y; (void)wp;
    if (msg != WM_LBUTTONUP || !IsWindowEnabled(dlg_item(d, EX_SWATCH))) return;
    if (dlg_color_picker("Color Key", &k)) {
        c->o.q.key_rgb = k & 0xFFFFFF;
        dlg_redraw(d, EX_SWATCH);
        exp_preview(d);
    }
}

static void exp_change(Dlg *d, int id, int code)
{
    ExpCtx *c = (ExpCtx *)d->user;
    if (id == IDOK || id == IDCANCEL) return;
    if (id == EX_QSLIDER) {
        dlg_set_int(d, EX_QUAL, dlg_slider_get(d, EX_QSLIDER));
        if (code != DLG_CODE_SLIDER) exp_preview(d);
        return;
    }
    if (id == EX_QCOMBO && code == DLG_CODE_CHANGE) {
        static const int qv[4] = { 3, 5, 8, 10 };
        int k = PC_CLAMP(dlg_get_combo(d, EX_QCOMBO), 0, 3);
        d->busy = 1;
        dlg_set_int(d, EX_QUAL, qv[k]);
        dlg_slider_set(d, EX_QSLIDER, qv[k]);
        d->busy = 0;
        exp_preview(d);
        return;
    }
    if (id == EX_QUAL) {
        int q = PC_CLAMP(dlg_get_int(d, EX_QUAL), 0, 12);
        d->busy = 1;
        dlg_slider_set(d, EX_QSLIDER, q);
        dlg_set_combo(d, EX_QCOMBO, q >= 10 ? 3 : q >= 8 ? 2 : q >= 5 ? 1 : 0);
        d->busy = 0;
    }
    if (id == EX_COLORS && code == DLG_CODE_CHANGE && GetFocus() == dlg_item(d, EX_COLORS)) return;   /* wait for focus loss / Enter */
    (void)c;
    exp_sync(d);
    exp_preview(d);
}

/* Shared body: fmt PC_FMT_UNKNOWN for Image > Mode > Indexed Color. */
static int export_dialog(const PcDoc *doc, int fmt, PcSaveOpts *o)
{
    static const char *const dith[PC_DITHER_COUNT] = { "None", "Pattern", "Diffusion" };
    static const char *const qnames[] = { "Low", "Medium", "High", "Maximum" };
    static const char *const pngt[] = { "None", "Color Key (1-bit)", "Full Alpha" };
    const char *pals[PC_PAL_COUNT];
    char title[48], b[16];
    ExpCtx *c = (ExpCtx *)calloc(1, sizeof *c);
    Dlg *d;
    int W = 480, x0 = 10, gw = 240, y = 8, i, rc, sel = 0, h, depth, idx_ok;
    if (!c) return 0;
    c->doc = doc; c->fmt = fmt; c->o = *o;
    c->gray = doc->s.mode == PC_MODE_GRAY && fmt != PC_FMT_UNKNOWN;
    c->docpal = doc->index_ncolors > 0;
    c->orig = pc_doc_composite(doc);
    if (!c->orig) { free(c); return 0; }
    if (doc->s.mode == PC_MODE_GRAY) {
        size_t k, np = (size_t)doc->s.w * doc->s.h;
        for (k = 0; k < np; k++) { u8 *p = c->orig->px + k * 4; p[0] = p[1] = p[2] = (u8)pc_luma(p[0], p[1], p[2]); }
    }
    {
        size_t k, np = (size_t)doc->s.w * doc->s.h;
        for (k = 0; k < np && !c->transparent; k++) c->transparent = c->orig->px[k * 4 + 3] != 255;
    }
    c->ndepth = fmt == PC_FMT_UNKNOWN ? 0 : pc_format_depths(fmt, c->depths);
    depth = c->o.depth ? c->o.depth : pc_format_default_depth(fmt, c->gray, c->transparent);
    if (!pc_format_depth_ok(fmt, depth)) depth = pc_format_default_depth(fmt, c->gray, c->transparent);
    if (c->o.q.palette == PC_PAL_DOCUMENT && !c->docpal) c->o.q.palette = PC_PAL_ADAPTIVE;
    for (i = 0; i < PC_PAL_COUNT; i++) pals[i] = pc_palette_name(i);
    idx_ok = fmt == PC_FMT_UNKNOWN || (fmt != PC_FMT_JPEG && pc_format_depth_ok(fmt, 8));
    if (fmt == PC_FMT_UNKNOWN) pc_strlcpy(title, "Indexed Color", sizeof title);
    else {
        const char *nm = pc_format_name(fmt), *sp = strchr(nm, ' ');
        char base[24];
        size_t L = sp ? (size_t)(sp - nm) : strlen(nm);
        if (fmt == PC_FMT_GIF) { nm = "GIF"; L = 3; }
        if (L >= sizeof base) L = sizeof base - 1;
        memcpy(base, nm, L); base[L] = 0;
        pc_snprintf(title, sizeof title, "%s Options", base);
    }
    /* height of the left column (the same steps as the layout below) */
    h = 8;
    if (c->ndepth) h += 30 + c->ndepth * 18 + (fmt == PC_FMT_BMP ? 18 : 0);
    if (idx_ok) h += 106;
    if (fmt == PC_FMT_JPEG) h += 98;
    else h += 30 + 22 * (1 + (fmt == PC_FMT_GIF || fmt == PC_FMT_PNG || fmt == PC_FMT_TGA || fmt == PC_FMT_UNKNOWN));
    if (fmt == PC_FMT_GIF || fmt == PC_FMT_TGA) h += 52;
    if (fmt == PC_FMT_TIFF) h += 88;
    h = PC_MAX(h + 2, 300);
    d = dlg_create(title, W, h, exp_change, c);
    if (!d) { pc_buf_unref(c->orig); free(c); return 0; }
    d->busy = 1;

    /* ---- Color Depth ---- */
    if (c->ndepth) {
        int gh = 22 + c->ndepth * 18 + (fmt == PC_FMT_BMP ? 18 : 0);
        dlg_group(d, x0, y, gw, gh, "Color Depth");
        for (i = 0; i < c->ndepth; i++) {
            dlg_radio(d, EX_DEPTH + i, x0 + 12, y + 18 + i * 18, gw - 20, 16, depth_label(c, c->depths[i]), c->depths[i] == depth, i == 0);
            if (c->depths[i] == depth) sel = 1;
        }
        if (!sel) dlg_set_check(d, EX_DEPTH, 1);
        if (fmt == PC_FMT_BMP) dlg_check(d, EX_GREEN6, x0 + 30, y + 18 + c->ndepth * 18, gw - 40, 16, "Use 5-&6-5 (BI_BITFIELDS)", c->o.green6);
        y += gh + 8;
    }
    /* ---- Indexed Color (also the dither of 16-bit high colour) ---- */
    if (idx_ok) {
        dlg_group(d, x0, y, gw, 98, fmt == PC_FMT_UNKNOWN ? "Palette" : "Indexed Color");
        dlg_label(d, x0 + 12, y + 22, 60, 16, "&Palette:");
        dlg_combo(d, EX_PALETTE, x0 + 76, y + 19, gw - 88, 20, pals, c->docpal ? PC_PAL_COUNT : PC_PAL_COUNT - 1, c->o.q.palette);
        dlg_label(d, x0 + 12, y + 48, 60, 16, "&Colors:");
        pc_snprintf(b, sizeof b, "%d", c->o.q.colors);
        dlg_edit(d, EX_COLORS, x0 + 76, y + 45, 44, 20, b, DLG_NUMBER);
        dlg_label(d, x0 + 126, y + 48, 100, 16, "(2-256)");
        dlg_label(d, x0 + 12, y + 74, 60, 16, "&Dither:");
        dlg_combo(d, EX_DITHER, x0 + 76, y + 71, gw - 88, 20, dith, PC_DITHER_COUNT, c->o.q.dither);
        y += 106;
    }
    /* ---- JPEG quality (Photoshop 0-12) ---- */
    if (fmt == PC_FMT_JPEG) {
        int q = c->o.jpeg_quality;
        dlg_group(d, x0, y, gw, 90, "Image Options");
        dlg_label(d, x0 + 12, y + 22, 50, 16, "&Quality:");
        pc_snprintf(b, sizeof b, "%d", q);
        dlg_edit(d, EX_QUAL, x0 + 64, y + 19, 36, 20, b, DLG_NUMBER);
        dlg_combo(d, EX_QCOMBO, x0 + 108, y + 19, 100, 20, qnames, 4, q >= 10 ? 3 : q >= 8 ? 2 : q >= 5 ? 1 : 0);
        dlg_slider(d, EX_QSLIDER, x0 + 12, y + 48, gw - 24, 0, 12, q, 0, 0);
        dlg_label(d, x0 + 12, y + 68, 60, 14, "small file");
        dlg_label(d, x0 + gw - 70, y + 68, 60, 14, "large file");
        y += 98;
    }
    /* ---- Transparency and the colour key ---- */
    if (fmt != PC_FMT_JPEG) {
        int rows = 1 + (fmt == PC_FMT_GIF || fmt == PC_FMT_PNG || fmt == PC_FMT_TGA || fmt == PC_FMT_UNKNOWN);
        int ry = y + 18;
        dlg_group(d, x0, y, gw, 22 + rows * 22, "Transparency");
        if (fmt == PC_FMT_GIF || fmt == PC_FMT_UNKNOWN) {
            dlg_check(d, EX_TRANSP, x0 + 12, ry, gw - 24, 16, "&Transparency", c->o.q.transparency);
            ry += 22;
        } else if (fmt == PC_FMT_TGA) {
            dlg_check(d, EX_TRANSP, x0 + 12, ry, gw - 24, 16, "1-bit &alpha at 16 bit", c->o.q.transparency);
            ry += 22;
        } else if (fmt == PC_FMT_PNG) {
            dlg_label(d, x0 + 12, ry + 3, 76, 16, "&Transparency:");
            dlg_combo(d, EX_PNGT, x0 + 92, ry, gw - 104, 20, pngt, 3, !c->o.q.transparency ? 0 : c->o.q.full_alpha ? 2 : 1);
            ry += 22;
        }
        dlg_check(d, EX_KEY, x0 + 12, ry, gw - 54, 16, "Transparent = color &key:", c->o.q.colorkey);
        dlg_custom(d, EX_SWATCH, x0 + gw - 40, ry - 2, 28, 20, exp_swatch_paint, exp_swatch_mouse);
        y += 30 + rows * 22;
    }
    /* ---- the format's own options ---- */
    if (fmt == PC_FMT_GIF) {
        dlg_group(d, x0, y, gw, 44, "Options");
        dlg_check(d, EX_INTERLACE, x0 + 12, y + 18, gw - 24, 16, "&Interlaced", c->o.interlace);
        y += 52;
    } else if (fmt == PC_FMT_TGA) {
        dlg_group(d, x0, y, gw, 44, "Options");
        dlg_check(d, EX_RLE, x0 + 12, y + 18, gw - 24, 16, "Compress (&RLE)", c->o.tga_rle);
        y += 52;
    } else if (fmt == PC_FMT_TIFF) {
        dlg_group(d, x0, y, gw, 80, "Compression");
        dlg_radio(d, EX_TIFFC, x0 + 12, y + 18, 150, 16, "&None", c->o.tiff_compress == 0, 1);
        dlg_radio(d, EX_TIFFC + 1, x0 + 12, y + 36, 150, 16, "&LZW", c->o.tiff_compress == 1, 0);
        dlg_radio(d, EX_TIFFC + 2, x0 + 12, y + 54, 150, 16, "Pack&Bits", c->o.tiff_compress == 2, 0);
        y += 88;
    }
    /* ---- 100% before / after preview ---- */
    c->pw = PC_MIN(132, doc->s.w); c->ph = PC_MIN(104, doc->s.h);
    c->px = (doc->s.w - c->pw) / 2; c->py = (doc->s.h - c->ph) / 2;
    dlg_label(d, 260, 8, 136, 14, "Original:");
    dlg_custom(d, EX_ORIG, 260, 22, 136, 108, exp_paint, exp_mouse);
    dlg_label(d, 260, 136, 136, 14, fmt == PC_FMT_UNKNOWN ? "Indexed:" : "As saved (1998):");
    dlg_custom(d, EX_RES, 260, 150, 136, 108, exp_paint, exp_mouse);
    dlg_label_id(d, EX_INFO, 260, 262, 210, 30, "", 0);
    ok_cancel(d, W);

    d->busy = 0;
    exp_sync(d);
    exp_preview(d);
    rc = dlg_run(d);
    if (rc == IDOK) {
        exp_read(d);
        if (fmt == PC_FMT_UNKNOWN) c->o.depth = 0;
        *o = c->o;
    }
    dlg_destroy(d);
    pc_buf_unref(c->orig);
    pc_buf_unref(c->res);
    free(c);
    return rc == IDOK;
}

int dlg_format_options(const PcDoc *doc, int fmt, PcSaveOpts *o)
{
    if (fmt == PC_FMT_PSD || fmt <= PC_FMT_UNKNOWN || fmt >= PC_FMT_COUNT) return 1;   /* PSD: always 8 bits/channel */
    return export_dialog(doc, fmt, o);
}

int dlg_indexed_color(PcDoc *doc, PcQuantOpts *q)
{
    PcSaveOpts o;
    pc_save_opts_default(&o);
    o.q = *q;
    o.q.full_alpha = 0;                    /* Indexed Color mode is the 1998 binary kind */
    if (!pc_doc_has_transparency(doc)) o.q.transparency = 0;
    if (!export_dialog(doc, PC_FMT_UNKNOWN, &o)) return 0;
    *q = o.q;
    return 1;
}
