/*
 * Options bar: CS6 placement (a strip under the menu bar), Photoshop 5.0
 * Options palette contents and widgets.  Rebuilt whenever the tool
 * changes; every field edits the tool's ToolOpts directly.
 */
#include "app.h"
#include "dlg.h"

enum { F_LABEL, F_CHECK, F_COMBO, F_NUM, F_BUTTON, F_BRUSH, F_SELOP, F_SWATCH, F_FONT, F_READOUT, F_SEP };

typedef struct Field {
    int type;
    const char *label;
    int *value;
    int min, max;
    const char *suffix;
    const char *const *items;
    int nitems;
    int cmd;
    int width;
    /* runtime */
    HWND ctl;
    RECT r;          /* custom-drawn area */
} Field;

#define MAXF 24
static Field g_f[MAXF];
static int g_nf;
static HWND g_bar;
static int g_built_tool = -1;
static int g_auto_select;
static char g_font_name[64] = "Arial";
static int g_font_style, g_font_size = 24, g_font_aa = 2, g_font_align;
static char g_readout[160];
static int g_ws_combo_x;
static HWND g_ws_combo;

static const char *const k_blend_items[PC_BLEND_COUNT] = {
    "Normal", "Dissolve", "Multiply", "Screen", "Overlay", "Soft Light", "Hard Light", "Color Dodge",
    "Color Burn", "Darken", "Lighten", "Difference", "Exclusion", "Hue", "Saturation", "Color", "Luminosity"
};
static const char *const k_style_items[] = { "Normal", "Constrained Aspect Ratio", "Fixed Size" };
static const char *const k_sample_items[] = { "Point Sample", "3 by 3 Average", "5 by 5 Average" };
static const char *const k_eraser_items[] = { "Paintbrush", "Airbrush", "Pencil", "Block" };
static const char *const k_range_items[] = { "Shadows", "Midtones", "Highlights" };
static const char *const k_sponge_items[] = { "Desaturate", "Saturate" };
static const char *const k_grad_items[] = { "Foreground to Background", "Foreground to Transparent", "Transparent to Foreground", "Black, White", "Spectrum" };
static const char *const k_gtype_items[] = { "Linear", "Radial", "Angle", "Reflected", "Diamond" };
static const char *const k_fill_items[] = { "Foreground", "Pattern" };
static const char *const k_shape_items[] = { "Create Work Path", "Fill Pixels" };
static const char *const k_fstyle_items[] = { "Regular", "Bold", "Italic", "Bold Italic" };
static const char *const k_aa_items[] = { "None", "Crisp", "Strong", "Smooth" };
static const char *const k_align_items[] = { "Left", "Center", "Right" };
static const char *const k_ws_items[] = { "Essentials", "1998 Palettes" };

int options_height(void) { return 30; }

static void add(int type, const char *label, int *value, int min, int max, const char *suffix, const char *const *items, int nitems, int cmd, int width)
{
    Field *f;
    if (g_nf >= MAXF) return;
    f = &g_f[g_nf++];
    memset(f, 0, sizeof *f);
    f->type = type; f->label = label; f->value = value; f->min = min; f->max = max; f->suffix = suffix;
    f->items = items; f->nitems = nitems; f->cmd = cmd; f->width = width;
}

#define CHECK(lbl, v) add(F_CHECK, lbl, v, 0, 1, NULL, NULL, 0, 0, 0)
#define COMBO(lbl, v, items, w) add(F_COMBO, lbl, v, 0, 0, NULL, items, (int)(sizeof(items) / sizeof(items[0])), 0, w)
#define NUM(lbl, v, lo, hi, sfx, w) add(F_NUM, lbl, v, lo, hi, sfx, NULL, 0, 0, w)
#define BUTTON(lbl, cmd) add(F_BUTTON, lbl, NULL, 0, 0, NULL, NULL, 0, cmd, 0)
#define SEP() add(F_SEP, NULL, NULL, 0, 0, NULL, NULL, 0, 0, 0)

static void build_fields(int tool)
{
    ToolOpts *o = &g_app.topt[tool];
    g_nf = 0;
    switch (tool) {
    case T_MOVE:
        CHECK("Auto Select Layer", &g_auto_select);
        break;
    case T_MARQUEE_RECT: case T_MARQUEE_ELLIPSE: case T_MARQUEE_ROW: case T_MARQUEE_COL:
        add(F_SELOP, NULL, &o->sel_op, 0, 3, NULL, NULL, 0, 0, 0);
        SEP();
        NUM("Feather:", &o->feather, 0, 250, "px", 34);
        if (tool == T_MARQUEE_ELLIPSE) CHECK("Anti-aliased", &o->antialias);
        if (tool <= T_MARQUEE_ELLIPSE) {
            COMBO("Style:", &o->style, k_style_items, 150);
            NUM("Width:", &o->fixed_w, 1, 30000, NULL, 40);
            NUM("Height:", &o->fixed_h, 1, 30000, NULL, 40);
        }
        break;
    case T_LASSO: case T_LASSO_POLY: case T_LASSO_MAGNETIC:
        add(F_SELOP, NULL, &o->sel_op, 0, 3, NULL, NULL, 0, 0, 0);
        SEP();
        NUM("Feather:", &o->feather, 0, 250, "px", 34);
        CHECK("Anti-aliased", &o->antialias);
        if (tool == T_LASSO_MAGNETIC) {
            NUM("Lasso Width:", &o->lasso_width, 1, 40, "px", 28);
            NUM("Edge Contrast:", &o->lasso_contrast, 1, 100, "%", 30);
            NUM("Frequency:", &o->lasso_freq, 0, 100, NULL, 30);
        }
        break;
    case T_WAND:
        add(F_SELOP, NULL, &o->sel_op, 0, 3, NULL, NULL, 0, 0, 0);
        SEP();
        NUM("Tolerance:", &o->tolerance, 0, 255, NULL, 34);
        CHECK("Anti-aliased", &o->antialias);
        CHECK("Contiguous", &o->contiguous);
        CHECK("Use All Layers", &o->sample_merged);
        break;
    case T_CROP:
        add(F_LABEL, "Drag to set the crop area. Enter crops, Esc cancels.", NULL, 0, 0, NULL, NULL, 0, 0, 0);
        BUTTON("Crop", CMD_COMMIT);
        BUTTON("Cancel", CMD_CANCEL);
        break;
    case T_EYEDROPPER:
        COMBO("Sample Size:", &o->sample_size, k_sample_items, 110);
        break;
    case T_MEASURE:
        add(F_READOUT, NULL, NULL, 0, 0, NULL, NULL, 0, 0, 380);
        break;
    case T_BRUSH: case T_AIRBRUSH: case T_PENCIL: case T_ERASER: case T_STAMP: case T_PATTERN_STAMP: case T_HISTORY_BRUSH:
    case T_BLUR: case T_SHARPEN: case T_SMUDGE: case T_DODGE: case T_BURN: case T_SPONGE:
        add(F_BRUSH, "Brush:", NULL, 0, 0, NULL, NULL, 0, 0, 0);
        if (tool == T_ERASER) COMBO("Mode:", &o->eraser_mode, k_eraser_items, 82);
        else if (tool == T_DODGE || tool == T_BURN) COMBO("Range:", &o->range, k_range_items, 82);
        else if (tool == T_SPONGE) COMBO("Mode:", &o->range, k_sponge_items, 82);
        else if (tool != T_BLUR && tool != T_SHARPEN && tool != T_SMUDGE) COMBO("Mode:", &o->blend, k_blend_items, 92);
        NUM(tool == T_AIRBRUSH || tool == T_BLUR || tool == T_SHARPEN || tool == T_SMUDGE ? "Pressure:" :
            tool == T_DODGE || tool == T_BURN || tool == T_SPONGE ? (tool == T_SPONGE ? "Pressure:" : "Exposure:") : "Opacity:",
            &o->opacity, 1, 100, "%", 30);
        if (tool == T_BRUSH || tool == T_AIRBRUSH || tool == T_PENCIL) NUM("Fade:", &o->fade, 0, 9999, "steps", 34);
        if (tool == T_BRUSH) CHECK("Wet Edges", &o->wet_edges);
        if (tool == T_PENCIL) CHECK("Auto Erase", &o->auto_erase);
        if (tool == T_ERASER) CHECK("Erase to History", &o->erase_to_history);
        if (tool == T_STAMP || tool == T_PATTERN_STAMP) CHECK("Aligned", &o->aligned);
        if (tool == T_STAMP || tool == T_BLUR || tool == T_SHARPEN || tool == T_SMUDGE) CHECK("Use All Layers", &o->sample_merged);
        if (tool == T_SMUDGE) CHECK("Finger Painting", &o->finger);
        break;
    case T_GRADIENT:
        COMBO("Gradient:", &o->grad_preset, k_grad_items, 150);
        COMBO("Type:", &o->grad_type, k_gtype_items, 76);
        COMBO("Mode:", &o->blend, k_blend_items, 92);
        NUM("Opacity:", &o->opacity, 1, 100, "%", 30);
        CHECK("Reverse", &o->grad_reverse);
        CHECK("Dither", &o->grad_dither);
        CHECK("Transparency", &o->grad_transparency);
        break;
    case T_BUCKET:
        COMBO("Fill:", &o->use_pattern, k_fill_items, 86);
        COMBO("Mode:", &o->blend, k_blend_items, 92);
        NUM("Opacity:", &o->opacity, 1, 100, "%", 30);
        NUM("Tolerance:", &o->tolerance, 0, 255, NULL, 30);
        CHECK("Anti-aliased", &o->antialias);
        CHECK("Contiguous", &o->contiguous);
        CHECK("Use All Layers", &o->sample_merged);
        break;
    case T_PEN: case T_PEN_FREEFORM: case T_PEN_MAGNETIC: case T_PEN_ADD: case T_PEN_DELETE: case T_PEN_CONVERT:
        if (tool == T_PEN) { CHECK("Rubber Band", &o->pen_rubber_band); CHECK("Auto Add/Delete", &o->pen_auto_add); }
        if (tool == T_PEN_FREEFORM) NUM("Curve Fit:", &o->tolerance, 1, 10, "px", 26);
        if (tool == T_PEN_MAGNETIC) {
            NUM("Width:", &o->lasso_width, 1, 40, "px", 28);
            NUM("Edge Contrast:", &o->lasso_contrast, 1, 100, "%", 30);
            NUM("Frequency:", &o->lasso_freq, 0, 100, NULL, 30);
        }
        SEP();
        BUTTON("Make Selection", CMD_PATH_TO_SEL);
        BUTTON("Fill Path", CMD_PATH_FILL);
        BUTTON("Stroke Path", CMD_PATH_STROKE);
        break;
    case T_PATH_SELECT: case T_DIRECT_SELECT:
        BUTTON("Make Selection", CMD_PATH_TO_SEL);
        BUTTON("Fill Path", CMD_PATH_FILL);
        BUTTON("Stroke Path", CMD_PATH_STROKE);
        BUTTON("Delete Path", CMD_PATH_DELETE);
        break;
    case T_TYPE: case T_TYPE_VERTICAL: case T_TYPE_MASK: case T_TYPE_MASK_VERTICAL:
        add(F_FONT, NULL, NULL, 0, 0, NULL, NULL, 0, 0, 150);
        COMBO(NULL, &g_font_style, k_fstyle_items, 84);
        NUM(NULL, &g_font_size, 1, 1296, "pt", 34);
        COMBO("Anti-alias:", &g_font_aa, k_aa_items, 70);
        COMBO(NULL, &g_font_align, k_align_items, 64);
        if (tool == T_TYPE || tool == T_TYPE_VERTICAL) add(F_SWATCH, "Color:", NULL, 0, 0, NULL, NULL, 0, 0, 0);
        BUTTON("Edit Text...", CMD_TYPE_EDIT);
        break;
    case T_SHAPE_RECT: case T_SHAPE_ELLIPSE: case T_SHAPE_LINE:
        COMBO(NULL, &o->shape_fill, k_shape_items, 120);
        if (tool == T_SHAPE_LINE) NUM("Weight:", &o->line_weight, 1, 1000, "px", 34);
        COMBO("Mode:", &o->blend, k_blend_items, 92);
        NUM("Opacity:", &o->opacity, 1, 100, "%", 30);
        CHECK("Anti-aliased", &o->antialias);
        break;
    case T_HAND: case T_ZOOM:
        if (tool == T_ZOOM) CHECK("Resize Windows To Fit", &o->zoom_resize);
        BUTTON("Actual Pixels", CMD_ACTUAL_PIXELS);
        BUTTON("Fit on Screen", CMD_FIT_SCREEN);
        BUTTON("Print Size", CMD_PRINT_SIZE);
        break;
    }
}

/* Type options are shared with the Type dialog. */
void options_get_type(PcText *t)
{
    pc_strlcpy(t->font, g_font_name, sizeof t->font);
    t->size = g_font_size;
    t->bold = g_font_style & 1;
    t->italic = (g_font_style & 2) != 0;
    t->antialias = g_font_aa;
    t->align = g_font_align;
    t->color = g_app.fg;
}

void options_set_type(const PcText *t)
{
    pc_strlcpy(g_font_name, t->font, sizeof g_font_name);
    g_font_size = t->size;
    g_font_style = (t->bold ? 1 : 0) | (t->italic ? 2 : 0);
    g_font_aa = t->antialias;
    g_font_align = t->align;
    g_built_tool = -1;
    options_refresh();
}

void options_set_readout(const char *s)
{
    pc_strlcpy(g_readout, s, sizeof g_readout);
    if (g_bar) InvalidateRect(g_bar, NULL, FALSE);
}

int options_auto_select(void) { return g_auto_select; }

static int CALLBACK font_enum(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD type, LPARAM lp)
{
    HWND c = (HWND)lp;
    (void)tm;
    if (lf->lfFaceName[0] == '@') return 1;
    if (!(type & TRUETYPE_FONTTYPE) && !(type & DEVICE_FONTTYPE)) {
        /* Bitmap faces are fine for type layers at their native size only; keep TrueType. */
        return 1;
    }
    if (SendMessageA(c, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)lf->lfFaceName) == CB_ERR)
        SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
    return 1;
}

void fill_font_combo(HWND c)
{
    HDC dc = GetDC(NULL);
    EnumFontFamiliesA(dc, NULL, (FONTENUMPROCA)font_enum, (LPARAM)c);
    ReleaseDC(NULL, dc);
}

static void destroy_controls(void)
{
    int i;
    for (i = 0; i < g_nf; i++) if (g_f[i].ctl) { DestroyWindow(g_f[i].ctl); g_f[i].ctl = NULL; }
}

static HWND mk(const char *cls, const char *text, DWORD style, DWORD ex, int id, int x, int y, int w, int h)
{
    HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_bar, (HMENU)(size_t)id, w95_inst(), NULL);
    if (c) SendMessageA(c, WM_SETFONT, (WPARAM)w95_font(W95_FONT_NORMAL), 0);
    return c;
}

static void layout_fields(void)
{
    HDC dc = GetDC(g_bar);
    int x = 38, i;
    for (i = 0; i < g_nf; i++) {
        Field *f = &g_f[i];
        int lw = f->label ? w95_text_width(dc, W95_FONT_NORMAL, f->label) + 4 : 0;
        int id = 100 + i;
        char buf[32];
        switch (f->type) {
        case F_LABEL:
            SetRect(&f->r, x, 4, x + lw + 4, 26);
            x += lw + 10;
            break;
        case F_SEP:
            SetRect(&f->r, x + 2, 4, x + 4, 26);
            x += 10;
            break;
        case F_CHECK:
            f->ctl = mk("BUTTON", f->label, BS_AUTOCHECKBOX, 0, id, x, 7, lw + 18, 16);
            if (f->ctl && *f->value) SendMessageA(f->ctl, BM_SETCHECK, BST_CHECKED, 0);
            x += lw + 26;
            break;
        case F_COMBO: {
            int k;
            SetRect(&f->r, x, 4, x + lw, 26);
            x += lw;
            f->ctl = mk("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0, id, x, 4, f->width, 200);
            for (k = 0; k < f->nitems; k++) SendMessageA(f->ctl, CB_ADDSTRING, 0, (LPARAM)f->items[k]);
            SendMessageA(f->ctl, CB_SETCURSEL, (WPARAM)PC_CLAMP(*f->value, 0, f->nitems - 1), 0);
            x += f->width + 8;
            break;
        }
        case F_NUM: {
            int sw = f->suffix ? w95_text_width(dc, W95_FONT_NORMAL, f->suffix) + 3 : 0;
            SetRect(&f->r, x, 4, x + lw, 26);
            x += lw;
            pc_snprintf(buf, sizeof buf, "%d", *f->value);
            f->ctl = mk("EDIT", buf, ES_AUTOHSCROLL | ES_NUMBER, WS_EX_CLIENTEDGE, id, x, 5, f->width, 20);
            x += f->width;
            /* PS5 pop-up slider arrow for percentage fields. */
            if (f->max <= 255) { SetRect(&f->r, f->r.left, 4, x + 13 + sw, 26); f->min = f->min; x += 12; }
            if (sw) x += sw;
            x += 8;
            break;
        }
        case F_BUTTON:
            f->ctl = mk("BUTTON", f->label, BS_PUSHBUTTON, 0, id, x, 4, lw + 16, 22);
            x += lw + 22;
            break;
        case F_BRUSH:
            SetRect(&f->r, x, 3, x + lw + 34, 27);
            x += lw + 42;
            break;
        case F_SELOP:
            SetRect(&f->r, x, 4, x + 4 * 23, 26);
            x += 4 * 23 + 4;
            break;
        case F_SWATCH:
            SetRect(&f->r, x, 4, x + lw + 30, 26);
            x += lw + 36;
            break;
        case F_FONT: {
            int k;
            f->ctl = mk("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | CBS_SORT, 0, id, x, 4, f->width, 260);
            fill_font_combo(f->ctl);
            k = (int)SendMessageA(f->ctl, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)g_font_name);
            SendMessageA(f->ctl, CB_SETCURSEL, (WPARAM)(k >= 0 ? k : 0), 0);
            x += f->width + 6;
            break;
        }
        case F_READOUT:
            SetRect(&f->r, x, 4, x + f->width, 26);
            x += f->width + 8;
            break;
        }
    }
    ReleaseDC(g_bar, dc);
}

void options_refresh(void)
{
    if (!g_bar) return;
    if (g_built_tool != g_app.tool) {
        destroy_controls();
        build_fields(g_app.tool);
        layout_fields();
        g_built_tool = g_app.tool;
    } else {
        int i;
        for (i = 0; i < g_nf; i++) {
            Field *f = &g_f[i];
            char buf[32];
            if (f->type == F_NUM && f->ctl && GetFocus() != f->ctl) {
                pc_snprintf(buf, sizeof buf, "%d", *f->value);
                SetWindowTextA(f->ctl, buf);
            } else if (f->type == F_CHECK && f->ctl) {
                SendMessageA(f->ctl, BM_SETCHECK, *f->value ? BST_CHECKED : BST_UNCHECKED, 0);
            } else if (f->type == F_COMBO && f->ctl) {
                SendMessageA(f->ctl, CB_SETCURSEL, (WPARAM)*f->value, 0);
            }
        }
    }
    InvalidateRect(g_bar, NULL, FALSE);
}

static void draw_selop(HDC dc, const RECT *r, int op)
{
    int k;
    for (k = 0; k < 4; k++) {
        RECT b, a, c;
        SetRect(&b, r->left + k * 23, r->top, r->left + k * 23 + 22, r->bottom);
        w95_draw_icon_button(dc, &b, -1, op == k ? 3 : 0, 0);
        if (op != k) w95_thin_raised(dc, &b);
        /* Glyph: one square (new), two squares combined (add/subtract/intersect). */
        SetRect(&a, b.left + 4, b.top + 5, b.left + 13, b.top + 14);
        SetRect(&c, b.left + 9, b.top + 9, b.left + 18, b.top + 18);
        if (op == k) { OffsetRect(&a, 1, 1); OffsetRect(&c, 1, 1); }
        if (k == 0) { InflateRect(&a, 1, 1); OffsetRect(&a, 2, 2); w95_fill(dc, &a, W95_WINDOW); w95_rect(dc, a.left, a.top, a.right, a.bottom, W95_TEXT); }
        else if (k == 1) { w95_fill(dc, &a, W95_WINDOW); w95_fill(dc, &c, W95_WINDOW); w95_rect(dc, a.left, a.top, a.right, a.bottom, W95_TEXT); w95_rect(dc, c.left, c.top, c.right, c.bottom, W95_TEXT); { RECT in = a; IntersectRect(&in, &a, &c); InflateRect(&in, -1, -1); w95_fill(dc, &in, W95_WINDOW); } }
        else if (k == 2) { w95_fill(dc, &a, W95_WINDOW); w95_rect(dc, a.left, a.top, a.right, a.bottom, W95_TEXT); w95_fill(dc, &c, W95_FACE); w95_rect(dc, c.left, c.top, c.right, c.bottom, W95_TEXT); }
        else { RECT in; w95_rect(dc, a.left, a.top, a.right, a.bottom, W95_TEXT); w95_rect(dc, c.left, c.top, c.right, c.bottom, W95_TEXT); IntersectRect(&in, &a, &c); w95_fill(dc, &in, W95_WINDOW); w95_rect(dc, in.left, in.top, in.right, in.bottom, W95_TEXT); }
    }
}

static void draw_brush_preview(HDC dc, const RECT *r, const PcBrush *b)
{
    RECT box;
    int cx, cy, rad, hard;
    SetRect(&box, r->right - 34, r->top, r->right - 4, r->bottom);
    w95_fill(dc, &box, W95_WINDOW);
    w95_sunken(dc, &box);
    cx = (box.left + box.right) / 2; cy = (box.top + box.bottom) / 2;
    rad = PC_MIN(b->diameter / 2, 9);
    hard = b->hardness;
    {
        int x, y;
        for (y = -rad; y <= rad; y++)
            for (x = -rad; x <= rad; x++) {
                double d = sqrt((double)x * x + (double)y * y);
                int v;
                if (d > rad + 0.5) continue;
                v = d <= rad * hard / 100.0 ? 0 : (int)(255 * (d - rad * hard / 100.0) / PC_MAX(1, rad - rad * hard / 100.0));
                v = PC_CLAMP(v, 0, 255);
                w95_pixel(dc, cx + x, cy + y, RGB(v, v, v));
            }
    }
    if (b->diameter > 18) {
        char t[16];
        RECT tr = box;
        pc_snprintf(t, sizeof t, "%d", b->diameter);
        tr.top = tr.bottom - 10;
        w95_text(dc, W95_FONT_SMALL, t, &tr, DT_CENTER | DT_SINGLELINE, W95_SELBK);
    }
    w95_draw_arrow(dc, r->right - 1, cy, 1, 3, W95_TEXT);
}

static void bar_paint(HWND hwnd, HDC target)
{
    RECT r, ir;
    W95Buf b;
    HDC dc;
    int i;
    GetClientRect(hwnd, &r);
    dc = w95_buf_begin(&b, target, &r);
    w95_fill(dc, &r, W95_FACE);
    w95_raised(dc, &r);
    /* Tool preset icon box at the far left. */
    SetRect(&ir, 4, 3, 33, 27);
    w95_thin_raised(dc, &ir);
    w95_icon(dc, g_tool_info[g_app.tool].icon, 7, 7);
    w95_draw_arrow(dc, 28, 15, 1, 3, W95_TEXT);
    for (i = 0; i < g_nf; i++) {
        Field *f = &g_f[i];
        RECT t = f->r;
        switch (f->type) {
        case F_LABEL: case F_COMBO: case F_NUM: case F_BRUSH: case F_SWATCH:
            if (f->label) w95_text(dc, W95_FONT_NORMAL, f->label, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
            if (f->type == F_NUM) {
                /* suffix + popup slider arrow */
                RECT er;
                if (f->ctl) {
                    GetWindowRect(f->ctl, &er);
                    MapWindowPoints(NULL, hwnd, (POINT *)&er, 2);
                    if (f->max <= 255) {
                        RECT ab;
                        SetRect(&ab, er.right, er.top, er.right + 11, er.bottom);
                        w95_fill(dc, &ab, W95_FACE);
                        w95_thin_raised(dc, &ab);
                        w95_draw_arrow(dc, ab.left + 5, (ab.top + ab.bottom) / 2, 3, 4, W95_TEXT);
                        er.right = ab.right;
                    }
                    if (f->suffix) {
                        RECT sr;
                        SetRect(&sr, er.right + 3, 4, er.right + 60, 26);
                        w95_text(dc, W95_FONT_NORMAL, f->suffix, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
                    }
                }
            }
            if (f->type == F_BRUSH) draw_brush_preview(dc, &t, &g_app.topt[g_app.tool].brush);
            if (f->type == F_SWATCH) {
                RECT s;
                SetRect(&s, t.right - 26, t.top + 2, t.right - 4, t.bottom - 2);
                w95_sunken(dc, &s);
                InflateRect(&s, -2, -2);
                w95_fill(dc, &s, RGB(pc_r(g_app.fg), pc_g(g_app.fg), pc_b(g_app.fg)));
            }
            break;
        case F_SEP: w95_vline(dc, t.left, 4, 26, W95_SHADOW); w95_vline(dc, t.left + 1, 4, 26, W95_HILIGHT); break;
        case F_SELOP: draw_selop(dc, &t, *f->value); break;
        case F_READOUT: w95_text(dc, W95_FONT_NORMAL, g_readout[0] ? g_readout : "Drag to measure distance and angle.", &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE, W95_TEXT); break;
        }
    }
    {
        RECT wl;
        SetRect(&wl, g_ws_combo_x - 4, 4, g_ws_combo_x, 26);
        w95_vline(dc, g_ws_combo_x - 8, 4, 26, W95_SHADOW); w95_vline(dc, g_ws_combo_x - 7, 4, 26, W95_HILIGHT);
        (void)wl;
    }
    w95_buf_end(&b);
}

static void field_changed(Field *f)
{
    char buf[32];
    PcDoc *d = app_doc();
    switch (f->type) {
    case F_CHECK: *f->value = SendMessageA(f->ctl, BM_GETCHECK, 0, 0) == BST_CHECKED; break;
    case F_COMBO: *f->value = (int)SendMessageA(f->ctl, CB_GETCURSEL, 0, 0); break;
    case F_NUM:
        GetWindowTextA(f->ctl, buf, sizeof buf);
        if (buf[0]) *f->value = PC_CLAMP((int)pc_atol(buf), f->min, f->max);
        break;
    case F_FONT: {
        int k = (int)SendMessageA(f->ctl, CB_GETCURSEL, 0, 0);
        if (k >= 0) SendMessageA(f->ctl, CB_GETLBTEXT, (WPARAM)k, (LPARAM)g_font_name);
        break;
    }
    }
    /* Type options apply live to the selected type layer (CS6 behaviour). */
    if (d && (g_app.tool >= T_TYPE && g_app.tool <= T_TYPE_MASK_VERTICAL)) {
        PcLayer *l = pc_doc_active(d);
        if (l && l->kind == PC_LAYER_TEXT && (f->type == F_FONT || f->value == &g_font_size || f->value == &g_font_style || f->value == &g_font_aa || f->value == &g_font_align)) {
            PcText t = l->text;
            u32 col = t.color;
            options_get_type(&t);
            t.color = col;
            l->text = t;
            if (!pc_layer_rerender_text(l, d->s.w, d->s.h)) app_commit(d, "Edit Type Layer");
        }
    }
    if (f->type == F_COMBO && f->value == &g_app.topt[g_app.tool].sel_op) InvalidateRect(g_bar, NULL, FALSE);
}

static void slider_live(int v, void *u)
{
    Field *f = (Field *)u;
    char buf[16];
    *f->value = v;
    pc_snprintf(buf, sizeof buf, "%d", v);
    SetWindowTextA(f->ctl, buf);
}

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
    case WM_SIZE:
        g_ws_combo_x = LOWORD(lp) - 112;
        if (g_ws_combo) SetWindowPos(g_ws_combo, NULL, g_ws_combo_x, 4, 104, 200, SWP_NOZORDER);
        return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: {
        static HBRUSH face;
        if (!face) face = CreateSolidBrush(W95_FACE);
        SetBkColor((HDC)wp, W95_FACE);
        return (LRESULT)face;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == 99 && code == CBN_SELCHANGE) {
            int k = (int)SendMessageA(g_ws_combo, CB_GETCURSEL, 0, 0);
            cmd_run(k == 1 ? CMD_WS_CLASSIC98 : CMD_WS_ESSENTIALS);
            SetFocus(g_app.main);
            return 0;
        }
        if (id >= 100 && id < 100 + g_nf) {
            Field *f = &g_f[id - 100];
            if (f->type == F_BUTTON && code == BN_CLICKED) { cmd_run(f->cmd); SetFocus(g_app.main); return 0; }
            if (f->type == F_CHECK && code == BN_CLICKED) { field_changed(f); SetFocus(g_app.main); }
            if ((f->type == F_COMBO || f->type == F_FONT) && code == CBN_SELCHANGE) { field_changed(f); SetFocus(g_app.main); }
            if (f->type == F_NUM && (code == EN_CHANGE || code == EN_KILLFOCUS)) field_changed(f);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), i;
        POINT p;
        p.x = x; p.y = y;
        if (x < 34) {
            /* Tool preset box: cycles through the tools of the current slot. */
            int cur = g_app.tool, t;
            for (t = cur + 1; t < cur + T_COUNT; t++)
                if (g_tool_info[t % T_COUNT].group == g_tool_info[cur].group) { app_set_tool(t % T_COUNT); break; }
            return 0;
        }
        for (i = 0; i < g_nf; i++) {
            Field *f = &g_f[i];
            if (f->type == F_SELOP && PtInRect(&f->r, p)) {
                *f->value = PC_CLAMP((x - f->r.left) / 23, 0, 3);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (f->type == F_BRUSH && PtInRect(&f->r, p)) {
                if (dlg_brush_options(&g_app.topt[g_app.tool].brush)) { InvalidateRect(hwnd, NULL, FALSE); dock_refresh(R_ALL); }
                return 0;
            }
            if (f->type == F_SWATCH && PtInRect(&f->r, p)) {
                u32 c = g_app.fg;
                if (dlg_color_picker("Select Text Color:", &c)) {
                    PcDoc *d = app_doc();
                    app_set_colors(c, g_app.bg);
                    if (d) {
                        PcLayer *l = pc_doc_active(d);
                        if (l && l->kind == PC_LAYER_TEXT) {
                            l->text.color = c;
                            if (!pc_layer_rerender_text(l, d->s.w, d->s.h)) app_commit(d, "Edit Type Layer");
                        }
                    }
                }
                return 0;
            }
            if (f->type == F_NUM && f->ctl && f->max <= 255) {
                RECT er, ab;
                GetWindowRect(f->ctl, &er);
                MapWindowPoints(NULL, hwnd, (POINT *)&er, 2);
                SetRect(&ab, er.right, er.top, er.right + 11, er.bottom);
                if (PtInRect(&ab, p)) {
                    POINT sp;
                    int v = *f->value;
                    sp.x = ab.left; sp.y = ab.bottom;
                    ClientToScreen(hwnd, &sp);
                    popup_slider(hwnd, sp.x, sp.y, f->min, f->max, &v, slider_live, f);
                    *f->value = v;
                    options_refresh();
                    return 0;
                }
            }
        }
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

HWND options_create(HWND parent)
{
    int i;
    w95_register_class("PCOptionsBar", bar_proc, 0, NULL);
    g_bar = CreateWindowExA(WS_EX_CONTROLPARENT, "PCOptionsBar", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 100, 30, parent, NULL, w95_inst(), NULL);
    g_ws_combo = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 4, 104, 200, g_bar, (HMENU)99, w95_inst(), NULL);
    SendMessageA(g_ws_combo, WM_SETFONT, (WPARAM)w95_font(W95_FONT_NORMAL), 0);
    for (i = 0; i < 2; i++) SendMessageA(g_ws_combo, CB_ADDSTRING, 0, (LPARAM)k_ws_items[i]);
    SendMessageA(g_ws_combo, CB_SETCURSEL, 0, 0);
    options_refresh();
    return g_bar;
}
