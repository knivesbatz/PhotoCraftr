/*
 * Dialog framework: modal Windows 95 dialogs built in code.
 *
 * Standard BUTTON/EDIT/COMBOBOX/LISTBOX controls are used where Windows 95
 * used them (with visual styles switched off they draw in the classic 3D
 * look on every Windows version); Photoshop-specific widgets (triangle
 * sliders, previews, curve and histogram graphs, colour fields) are custom.
 */
#ifndef DLG_H
#define DLG_H

#include "app.h"

typedef struct Dlg Dlg;
typedef void (*DlgChangeFn)(Dlg *d, int id, int code);
typedef void (*DlgPaintFn)(Dlg *d, int id, HDC dc, const RECT *r);
typedef void (*DlgMouseFn)(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp);

struct Dlg {
    HWND hwnd;
    W95Frame fr;
    int result;
    int done;
    DlgChangeFn on_change;
    void *user;
    int busy;               /* suppress change callbacks while setting values */
    void (*draw_item)(struct Dlg *d, const DRAWITEMSTRUCT *di);   /* owner-drawn list boxes */
    int ncustom;
    struct { int id; HWND hwnd; DlgPaintFn paint; DlgMouseFn mouse; } custom[16];
    int nslider;
    struct { int id; HWND hwnd; int min, max, val, edit_id, drag; int flags; } slider[24];
};

enum { DLG_NUMBER = 1, DLG_READONLY = 2, DLG_MULTILINE = 4, DLG_SIGNED = 8, DLG_DECIMAL = 16 };
enum { SLIDER_CENTER_ZERO = 1, SLIDER_HUE_BAR = 2, SLIDER_SAT_BAR = 4, SLIDER_LIGHT_BAR = 8 };
enum { DLG_CODE_CHANGE = 1, DLG_CODE_CLICK = 2, DLG_CODE_SLIDER = 3, DLG_CODE_DBLCLK = 4 };

Dlg *dlg_create(const char *title, int w, int h, DlgChangeFn on_change, void *user);
HWND dlg_label(Dlg *d, int x, int y, int w, int h, const char *text);
HWND dlg_label_id(Dlg *d, int id, int x, int y, int w, int h, const char *text, int align_right);
HWND dlg_edit(Dlg *d, int id, int x, int y, int w, int h, const char *text, int flags);
HWND dlg_button(Dlg *d, int id, int x, int y, int w, int h, const char *label, int is_default);
HWND dlg_check(Dlg *d, int id, int x, int y, int w, int h, const char *label, int checked);
HWND dlg_radio(Dlg *d, int id, int x, int y, int w, int h, const char *label, int checked, int first_in_group);
HWND dlg_combo(Dlg *d, int id, int x, int y, int w, int h, const char *const *items, int n, int sel);
HWND dlg_list(Dlg *d, int id, int x, int y, int w, int h);
HWND dlg_list_owner(Dlg *d, int id, int x, int y, int w, int h);   /* owner-drawn, 17 px items */
HWND dlg_group(Dlg *d, int x, int y, int w, int h, const char *label);
HWND dlg_custom(Dlg *d, int id, int x, int y, int w, int h, DlgPaintFn paint, DlgMouseFn mouse);
HWND dlg_slider(Dlg *d, int id, int x, int y, int w, int min, int max, int val, int edit_id, int flags);
int dlg_run(Dlg *d);            /* returns IDOK / IDCANCEL / button id */
void dlg_end(Dlg *d, int result);
void dlg_destroy(Dlg *d);

int dlg_get_int(Dlg *d, int id);
double dlg_get_double(Dlg *d, int id);
void dlg_set_int(Dlg *d, int id, int v);
void dlg_set_double(Dlg *d, int id, double v, int decimals);
void dlg_get_text(Dlg *d, int id, char *buf, int n);
void dlg_set_text(Dlg *d, int id, const char *s);
int dlg_get_check(Dlg *d, int id);
void dlg_set_check(Dlg *d, int id, int on);
int dlg_get_combo(Dlg *d, int id);
void dlg_set_combo(Dlg *d, int id, int sel);
void dlg_enable(Dlg *d, int id, int on);
void dlg_redraw(Dlg *d, int id);
int dlg_slider_get(Dlg *d, int id);
void dlg_slider_set(Dlg *d, int id, int v);
HWND dlg_item(Dlg *d, int id);

/* Live preview on the active document: snapshot, re-apply, restore. */
typedef struct DlgPreview { PcDoc *doc; PcDocState saved; int active; } DlgPreview;
void preview_begin(DlgPreview *p, PcDoc *doc);
void preview_restore(DlgPreview *p);               /* back to the snapshot */
void preview_end(DlgPreview *p, int keep);         /* keep: leave current pixels */

/* Popup slider (palette and options-bar fields with a small arrow button). */
int popup_slider(HWND owner, int screen_x, int screen_y, int min, int max, int *value, void (*live)(int v, void *u), void *u);

#endif
