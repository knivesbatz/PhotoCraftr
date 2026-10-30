/*
 * PhotoCraft Classic - Windows 95 style window chrome and controls.
 *
 * Top-level windows are created as WS_POPUP (so Windows Vista..10 do not
 * draw a modern DWM title bar) and this module paints a classic 3D frame
 * and navy caption bar in the non-client area itself.  Push buttons and
 * check boxes are owner-drawn with the Windows 95 bevel colours so they
 * look identical on every version (with no visual-styles manifest Windows
 * 10 would use its own lighter "classic" palette).  The progress bar is
 * drawn here too, so nothing depends on comctl32.dll.
 *
 * Only Windows 95 APIs are used; SetThemeAppProperties is called through
 * GetProcAddress when uxtheme.dll exists (XP and later).
 *
 * Usage:
 *   pc_chrome_init(hinst);
 *   pc_chrome_register_class("MyWnd", MyProc);      (cbWndExtra reserved)
 *   hwnd = pc_chrome_create("MyWnd", "Title", 320, 120, owner, param);
 *   In MyProc, first:
 *       LRESULT lr;
 *       if (pc_chrome_proc(hwnd, msg, wp, lp, &lr)) return lr;
 *   Child controls: pc_ctl_*().  WM_CLOSE is sent when the caption close
 *   button is clicked.
 */
#ifndef PC_CHROME_H
#define PC_CHROME_H

#include <windows.h>

#define PC_RGB_FACE    RGB(192, 192, 192)
#define PC_RGB_LIGHT   RGB(223, 223, 223)
#define PC_RGB_HILITE  RGB(255, 255, 255)
#define PC_RGB_SHADOW  RGB(128, 128, 128)
#define PC_RGB_DARK    RGB(0, 0, 0)
#define PC_RGB_NAVY    RGB(0, 0, 128)
#define PC_RGB_WINDOW  RGB(255, 255, 255)

#define PC_FRAME_BORDER  3
#define PC_FRAME_CAPTION 18

/* Window extra bytes used by the chrome (classes must reserve them;
 * pc_chrome_register_class does).  Offset PC_WX_USER is free for the app. */
#define PC_WX_USER   0
#define PC_WX_STATE  4
#define PC_WX_DEFID  8
#define PC_WX_BYTES  12

void pc_chrome_init(HINSTANCE hinst);
HINSTANCE pc_chrome_instance(void);
HFONT pc_chrome_font(void);         /* MS Sans Serif 8pt          */
HFONT pc_chrome_bold(void);         /* MS Sans Serif 8pt bold     */
HBRUSH pc_chrome_face_brush(void);  /* #C0C0C0                    */

ATOM pc_chrome_register_class(const char *name, WNDPROC proc);

/* Create a top-level classic window with the given CLIENT size, centred on
 * the owner (or the screen).  'param' is stored at PC_WX_USER before
 * WM_CREATE is delivered.  The window is created hidden. */
HWND pc_chrome_create(const char *cls, const char *title, int cw, int ch, HWND owner, void *param);
void *pc_chrome_param(HWND hwnd);

/* Handles non-client painting/hit-testing, owner-drawn controls, control
 * colours and sunken frames.  Returns 1 if the message was consumed. */
int pc_chrome_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT *result);

/* Controls (all use the 8pt font).  Coordinates are client pixels. */
HWND pc_ctl_label(HWND parent, int id, const char *text, int x, int y, int w, int h);
HWND pc_ctl_label_bold(HWND parent, int id, const char *text, int x, int y, int w, int h);
HWND pc_ctl_button(HWND parent, int id, const char *text, int x, int y, int w, int h, int is_default);
HWND pc_ctl_check(HWND parent, int id, const char *text, int x, int y, int w, int h, int checked);
/* Edit with a classic sunken frame drawn around it (x,y,w,h = outer frame). */
HWND pc_ctl_edit(HWND parent, int id, const char *text, int x, int y, int w, int h, DWORD style);
/* Listbox with sunken frame (x,y,w,h = outer frame). */
HWND pc_ctl_list(HWND parent, int id, int x, int y, int w, int h, DWORD style);
/* Progress bar (navy blocks in a sunken field); position 0..1000. */
HWND pc_ctl_progress(HWND parent, int id, int x, int y, int w, int h);
void pc_progress_set(HWND bar, int permille);
/* Etched horizontal separator. */
HWND pc_ctl_separator(HWND parent, int x, int y, int w);
/* Static icon (system icon such as IDI_INFORMATION) drawn 32x32. */
HWND pc_ctl_icon(HWND parent, LPCSTR system_icon, int x, int y);

int pc_ctl_get_check(HWND check);
void pc_ctl_set_check(HWND check, int on);
void pc_ctl_set_default(HWND parent, int id);

/* Drawing helpers. */
void pc_draw_frame(HDC dc, RECT r, COLORREF tl_out, COLORREF br_out, COLORREF tl_in, COLORREF br_in);
void pc_draw_raised(HDC dc, RECT r);    /* window frame bevel */
void pc_draw_sunken(HDC dc, RECT r);    /* field bevel        */
void pc_fill(HDC dc, RECT r, COLORREF c);

/* Show 'hwnd' and run a modal message loop (owner disabled) until *done
 * becomes non-zero.  Keyboard navigation via IsDialogMessage. */
void pc_chrome_modal(HWND hwnd, HWND owner, volatile int *done);

/* Classic message box. icon = IDI_INFORMATION / IDI_EXCLAMATION /
 * IDI_HAND / IDI_QUESTION or NULL.  Up to three buttons (NULL = absent);
 * returns 1..3 for the button pressed; closing/Esc returns the LAST
 * button's number (treated as Cancel). */
int pc_msgbox(HWND owner, const char *title, const char *text, LPCSTR icon,
              const char *b1, const char *b2, const char *b3);

/* Run all pending messages (keeps windows painted during blocking work). */
void pc_chrome_pump(void);

#endif
