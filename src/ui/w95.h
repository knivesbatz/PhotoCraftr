/*
 * PhotoCraft Classic - Windows 95 look, drawn by hand.
 *
 * Everything visible is painted here with plain GDI so it looks the same
 * on Windows 95 and on Windows 10 (where the OS would otherwise draw DWM
 * title bars, flat menus and Segoe UI).  Colours are the fixed Windows 95
 * "Windows Standard" scheme, not the host's system colours.
 */
#ifndef W95_H
#define W95_H

#include <windows.h>
#include "rt/pcrt.h"

/* ---------------- palette ---------------- */
#define W95_FACE      RGB(192, 192, 192)
#define W95_LIGHT     RGB(223, 223, 223)
#define W95_HILIGHT   RGB(255, 255, 255)
#define W95_SHADOW    RGB(128, 128, 128)
#define W95_DKSHADOW  RGB(0, 0, 0)
#define W95_TEXT      RGB(0, 0, 0)
#define W95_GRAYTEXT  RGB(128, 128, 128)
#define W95_WINDOW    RGB(255, 255, 255)
#define W95_SELBK     RGB(0, 0, 128)
#define W95_SELTEXT   RGB(255, 255, 255)
#define W95_CAPTION   RGB(0, 0, 128)
#define W95_CAPTION2  RGB(16, 132, 208)    /* Windows 98 gradient end */
#define W95_INACTIVE  RGB(128, 128, 128)
#define W95_INACTIVE2 RGB(192, 192, 192)
#define W95_INACTTEXT RGB(192, 192, 192)
#define W95_WORKSPACE RGB(128, 128, 128)
#define W95_TOOLTIP   RGB(255, 255, 225)

/* ---------------- metrics (pixels at 96 dpi) ---------------- */
enum {
    W95_CAPTION_H = 18,      /* top-level caption bar */
    W95_BORDER = 4,          /* sizable frame */
    W95_DLGBORDER = 3,       /* dialog frame */
    W95_MENU_H = 19,         /* menu bar */
    W95_MENUITEM_H = 17,     /* popup item */
    W95_SCROLL = 16,
    W95_SMCAPTION_H = 11,    /* palette (tool window) caption */
    W95_CAPBTN_W = 16, W95_CAPBTN_H = 14
};

/* ---------------- fonts ---------------- */
typedef enum { W95_FONT_NORMAL, W95_FONT_BOLD, W95_FONT_SMALL, W95_FONT_SMALL_BOLD, W95_FONT_COUNT } W95FontId;
HFONT w95_font(W95FontId id);
int w95_text_width(HDC dc, W95FontId f, const char *s);

/* ---------------- init ---------------- */
void w95_init(HINSTANCE inst);
HINSTANCE w95_inst(void);
extern int g_w95_gradient_captions;   /* Windows 98 style captions (Preferences) */

/* ---------------- primitives ---------------- */
void w95_fill(HDC dc, const RECT *r, COLORREF c);
void w95_rect(HDC dc, int x0, int y0, int x1, int y1, COLORREF c);   /* outline, x1/y1 exclusive */
void w95_hline(HDC dc, int x0, int x1, int y, COLORREF c);
void w95_vline(HDC dc, int x, int y0, int y1, COLORREF c);
void w95_pixel(HDC dc, int x, int y, COLORREF c);
/* Two-pixel edges in the DrawEdge layouts of Windows 95. */
void w95_raised(HDC dc, const RECT *r);       /* window / button outer: light-hilite / shadow-dkshadow */
void w95_sunken(HDC dc, const RECT *r);       /* field: shadow-dkshadow / hilite-light */
void w95_button_edge(HDC dc, const RECT *r, int pressed, int is_default);
void w95_thin_raised(HDC dc, const RECT *r);  /* 1px: hilite / shadow (status bar panes, palette buttons) */
void w95_thin_sunken(HDC dc, const RECT *r);
void w95_etched(HDC dc, const RECT *r);       /* group box */
void w95_etched_hline(HDC dc, int x0, int x1, int y);
void w95_focus_rect(HDC dc, const RECT *r);
/* Text helpers. flags: DT_* subset. Disabled text is embossed. */
void w95_text(HDC dc, W95FontId f, const char *s, const RECT *r, UINT flags, COLORREF c);
void w95_text_disabled(HDC dc, W95FontId f, const char *s, const RECT *r, UINT flags);
/* Text with '&' accelerators underlined. */
void w95_text_accel(HDC dc, W95FontId f, const char *s, int x, int y, COLORREF c, int disabled);

/* Classic push button face (bevel + centered text). */
void w95_draw_button(HDC dc, const RECT *r, const char *label, int pressed, int disabled, int focused, int is_default);
/* Small square button with an icon (toolbox, palette buttons). */
void w95_draw_icon_button(HDC dc, const RECT *r, int icon, int state /*0 up 1 hover 2 down 3 checked*/, int disabled);
void w95_draw_checkbox(HDC dc, int x, int y, int checked, int disabled);   /* 13x13 */
void w95_draw_radio(HDC dc, int x, int y, int checked, int disabled);      /* 12x12 */
void w95_draw_arrow(HDC dc, int cx, int cy, int dir /*0 up 1 down 2 left 3 right*/, int size, COLORREF c);
/* Scrollbar pieces. */
void w95_draw_scrollbar(HDC dc, const RECT *r, int vertical, int pos, int page, int max, int pressed_part);
int w95_scroll_hit(const RECT *r, int vertical, int pos, int page, int max, int x, int y, int *thumb_off);
/* Checkerboard / dither pattern brush (Windows 95 scrollbar track, disabled fills). */
HBRUSH w95_dither_brush(void);

/* ---------------- icons (src/ui/icons.h) ---------------- */
void w95_icon(HDC dc, int icon, int x, int y);
void w95_icon_disabled(HDC dc, int icon, int x, int y);   /* embossed like Win95 disabled toolbar buttons */
int w95_icon_w(int icon);
int w95_icon_h(int icon);
HCURSOR w95_cursor(int cursor);
HICON w95_app_icon(int small);

/* ---------------- double buffering ---------------- */
typedef struct { HDC dc; HBITMAP bmp, old; int w, h; HDC target; int x, y; } W95Buf;
HDC w95_buf_begin(W95Buf *b, HDC target, const RECT *r);
void w95_buf_end(W95Buf *b);

/* ---------------- custom window frame ----------------
 * Top-level windows are WS_POPUP and draw their own Windows 95 caption and
 * border so Windows 10 can't substitute its own.  A window opts in by
 * calling w95_frame_proc() first in its window procedure. */
enum {
    W95_FRAME_SIZABLE = 1, W95_FRAME_MINMAX = 2, W95_FRAME_CLOSE = 4, W95_FRAME_SMALL = 8,
    W95_FRAME_DIALOG = 16, W95_FRAME_HELP = 32, W95_FRAME_NOCAPTION = 64
};
typedef struct W95Frame {
    int flags;
    int active;
    int maximized;
    RECT restore;
    int pressed;         /* caption button being pressed: 0 none, 1 close, 2 max, 3 min, 4 help */
    int hot;
    int tracking;
    int drag_mode;       /* hit-test code while moving/sizing */
    POINT drag_start;
    RECT drag_rect;
    int min_w, min_h;
    HWND hwnd;
} W95Frame;
void w95_frame_attach(HWND hwnd, W95Frame *f, int flags);
/* Returns 1 when the message was handled (then *res is the result). */
int w95_frame_proc(HWND hwnd, W95Frame *f, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res);
void w95_frame_metrics(const W95Frame *f, int *border, int *caption);
void w95_frame_maximize(HWND hwnd, W95Frame *f, int on);
/* Client size for a desired outer size and vice versa. */
void w95_frame_adjust(int flags, int *w, int *h);

/* Register the generic classes used by dialogs and palettes. */
ATOM w95_register_class(const char *name, WNDPROC proc, UINT style, HCURSOR cur);

/* Owner-drawn tooltip (yellow box) shared by toolbox and panels. */
void w95_tooltip_show(HWND owner, int screen_x, int screen_y, const char *text);
void w95_tooltip_hide(void);

#endif
