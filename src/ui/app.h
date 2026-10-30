/*
 * PhotoCraft Classic - application shell shared declarations.
 *
 * Layout (Photoshop CS6 arrangement, Windows 95 drawing):
 *
 *   +--------------------------------------------------------------+
 *   | caption                                                      |
 *   | File Edit Image Layer Type Select Filter View Window Help    |  menu bar
 *   | [tool] options for the current tool ............ Essentials |  options bar
 *   +----+------------------------------------------+---+---------+
 *   |tool| tabs: doc1.psd @ 100% | doc2.jpg @ 50%    |ico| Color   |
 *   | s  |                                          |col| Navig.  |
 *   |    |   document view (tabbed or floating)     |   | Layers  |
 *   |    | [100%][Doc: 1.2M/1.2M >]  scroll bars    |   |         |
 *   +----+------------------------------------------+---+---------+
 */
#ifndef APP_H
#define APP_H

#include "w95.h"
#include "icons.h"
#include "core/pcops.h"
#include "io/pcio.h"

#define APP_NAME "PhotoCraft Classic"
#define APP_REGKEY "Software\\PhotoCraft Classic"
#define APP_FRAME_CLASS "PhotoCraftClassicFrame"

/* ---------------- commands ---------------- */
#include "cmds.h"

/* ---------------- tools ---------------- */
typedef enum {
    T_MOVE, T_MARQUEE_RECT, T_MARQUEE_ELLIPSE, T_MARQUEE_ROW, T_MARQUEE_COL,
    T_LASSO, T_LASSO_POLY, T_LASSO_MAGNETIC, T_WAND, T_CROP,
    T_EYEDROPPER, T_MEASURE,
    T_BRUSH, T_AIRBRUSH, T_PENCIL,
    T_STAMP, T_PATTERN_STAMP, T_HISTORY_BRUSH, T_ERASER,
    T_GRADIENT, T_BUCKET,
    T_BLUR, T_SHARPEN, T_SMUDGE, T_DODGE, T_BURN, T_SPONGE,
    T_PEN, T_PEN_FREEFORM, T_PEN_MAGNETIC, T_PEN_ADD, T_PEN_DELETE, T_PEN_CONVERT,
    T_PATH_SELECT, T_DIRECT_SELECT,
    T_TYPE, T_TYPE_VERTICAL, T_TYPE_MASK, T_TYPE_MASK_VERTICAL,
    T_SHAPE_RECT, T_SHAPE_ELLIPSE, T_SHAPE_LINE,
    T_HAND, T_ZOOM,
    T_COUNT
} ToolId;

typedef struct ToolInfo {
    const char *name;
    int icon;
    char key;            /* shortcut letter, Shift+key cycles the group */
    int group;           /* toolbox slot index */
    int cursor;
} ToolInfo;
extern const ToolInfo g_tool_info[T_COUNT];

/* Per-tool options (the Options bar edits these). */
typedef struct ToolOpts {
    PcBrush brush;
    int opacity;         /* or pressure/exposure */
    int blend;
    int feather;         /* marquee/lasso, pixels */
    int antialias;
    int style;           /* marquee: 0 normal 1 constrained aspect 2 fixed size */
    int fixed_w, fixed_h;
    int tolerance;
    int contiguous;
    int sample_merged;
    int sample_size;     /* eyedropper: 0 point 1 3x3 2 5x5 */
    int fade;
    int wet_edges;
    int aligned;
    int range;
    int eraser_mode;
    int erase_to_history;
    int auto_erase;
    int finger;
    int grad_type, grad_preset, grad_reverse, grad_dither, grad_transparency;
    int use_pattern;
    int lasso_width, lasso_contrast, lasso_freq;
    int pen_rubber_band, pen_auto_add;
    int shape_fill;      /* shape tools: 0 path (outline) 1 filled pixels */
    int line_weight;
    int zoom_resize;
    int sel_op;          /* PC_SEL_* default op from the options bar buttons */
    int font_size;
} ToolOpts;

/* ---------------- document views ---------------- */
typedef struct DocView {
    PcDoc *doc;
    HWND hwnd;           /* view window */
    HWND frame;          /* floating frame window (NULL when tabbed) */
    W95Frame fr;
    int number;          /* Untitled-N */
    double zoom;
    int sx, sy;          /* scroll offset in screen pixels */
    /* display cache */
    PcBuf *comp;
    unsigned comp_rev;
    PcRect dirty;        /* pending partial recomposite (doc coords) */
    int dirty_full;
    int status_mode;     /* 0 doc sizes, 1 dimensions, 2 current tool */
    HWND zoom_edit;
    int ants_key_valid;
    unsigned ants_rev; double ants_zoom; int ants_sx, ants_sy, ants_w, ants_h;
    int *ants;           /* cached marching-ants edge pixels (x | y << 16) */
    int nants;
    int ants_phase;
    int show_channel;    /* -1 composite, 0..2 single colour channel, 3+ alpha channel overlay */
    int edit_channel;    /* -1 all colour channels, 0..2 one channel, 100+k alpha channel k */
    int show_rulers;
    int show_grid;
    int show_edges;      /* View > Hide Edges toggles marching ants */
    /* scroll bar interaction */
    int sb_drag, sb_part, sb_thumb_off;
    /* canvas interaction state (tools.c) */
    int dragging;
    POINT drag_start, drag_last;       /* screen (view) coords */
    float doc_start_x, doc_start_y, doc_x, doc_y;
    PcStroke *stroke;
    PcDocState saved;                   /* state before an interactive op */
    int has_saved;
    int poly_n, poly_cap;
    float *poly;                        /* lasso / polygonal lasso points */
    int crop_active;
    float crop[4];                      /* x0 y0 x1 y1 */
    int crop_handle;
    int xf_active;                      /* free transform */
    float xf_quad[8], xf_orig[8];
    PcRect xf_src;
    int xf_handle;
    int pen_drag_pt, pen_drag_handle;
    int measure_active;
    float measure[4];
    int temp_tool;                      /* space-bar hand etc, -1 none */
    POINT mouse;
    int mouse_in;
} DocView;

/* ---------------- application state ---------------- */
typedef struct Prefs {
    int history_states;
    int gradient_captions;
    int precise_cursors;     /* Caps Lock style crosshair always */
    int brush_size_cursor;
    int check_updates;
    int interpolation;       /* PC_RESAMPLE_* for Image Size default */
    int units;               /* 0 pixels 1 inches 2 cm */
    int checker_size;
    int export_clipboard;
    int beep_when_done;
    int save_previews;
    int jpeg_quality;
    int ruler_units;
    int grid_spacing;
    u32 grid_color, guide_color;
    int tab_docs;            /* open documents as tabs (CS6) instead of floating windows */
    int classic_file_dialogs;/* built-in Windows 95-style Open/Save instead of the system's */
} Prefs;

#define MAX_VIEWS 64
#define MAX_RECENT 8

typedef struct App {
    HINSTANCE inst;
    HWND main, menubar, options, toolbox, dock, docarea;
    W95Frame fr;
    DocView *views[MAX_VIEWS];
    int nviews;
    int active;              /* index into views, -1 none */
    int next_untitled;
    int tool;
    int group_tool[32];      /* tool shown in each toolbox slot */
    ToolOpts topt[T_COUNT];
    u32 fg, bg;
    int screen_mode;         /* 0 standard, 1 full with menu, 2 full */
    int hide_all;            /* Tab */
    int hide_panels;         /* Shift+Tab */
    int tools_hidden, options_hidden;
    Prefs prefs;
    char recent[MAX_RECENT][260];
    int nrecent;
    char last_dir[260];
    PcClip clip;
    int modal;               /* a modal dialog is up */
    UINT msg_update_close;   /* registered by the updater */
    int recording;           /* actions */
    int quickmask;
} App;

extern App g_app;

/* app.c */
DocView *app_view(void);
PcDoc *app_doc(void);
void app_set_active(int index);
DocView *app_add_doc(PcDoc *d, const char *path);
int app_close_view(int index, int ask);
void app_layout(void);
void app_refresh(int what);    /* bit flags below */
void app_status(const char *fmt, ...);
void app_set_tool(int tool);
void app_set_colors(u32 fg, u32 bg);
void app_toggle_panels(int all);
void app_set_screen_mode(int mode);
int app_open_file(const char *path);
int app_save_view(DocView *v, int save_as);
void app_add_recent(const char *path);
void app_commit(PcDoc *d, const char *label);   /* history + refresh + action recording hook */
void app_error(const char *fmt, ...);
int app_confirm(const char *title, const char *fmt, ...);  /* Yes/No/Cancel -> 1/0/-1 */
void app_beep_done(void);
enum { R_CANVAS = 1, R_LAYERS = 2, R_HISTORY = 4, R_CHANNELS = 8, R_PATHS = 16, R_NAV = 32, R_INFO = 64,
       R_COLOR = 128, R_TABS = 256, R_TOOLS = 512, R_OPTIONS = 1024, R_TITLE = 2048, R_ALL = 0xFFFF };

/* settings.c */
void settings_load(void);
void settings_save(void);
int reg_get_int(const char *sub, const char *name, int def);
void reg_set_int(const char *sub, const char *name, int v);
int reg_get_str(const char *sub, const char *name, char *out, int n);
void reg_set_str(const char *sub, const char *name, const char *v);
int reg_get_bin(const char *sub, const char *name, void *out, int n);
void reg_set_bin(const char *sub, const char *name, const void *v, int n);

/* platform.c */
int plat_read_file(const char *path, u8 **data, size_t *n, char *err, size_t errn);
int plat_write_file(const char *path, const u8 *data, size_t n, char *err, size_t errn);
int plat_file_exists(const char *path);
void plat_clipboard_put(const PcClip *c);
int plat_clipboard_get(PcClip *c);
int plat_text_render(const PcText *t, PcBuf *rgba, void *user);
int plat_print(DocView *v, HWND owner);
void plat_launch_updater(const char *args);
const char *plat_basename(const char *path);

/* menu.c */
HWND menubar_create(HWND parent);
int menubar_height(void);
int menu_popup(int menu_index, int screen_x, int screen_y, HWND owner);
int menu_popup_items(const struct MenuItem *items, int n, int x, int y, HWND owner);
int menubar_key(HWND owner, int vk, int alt);
void menubar_cancel(void);

/* cmds.c */
void cmd_run(int cmd);
void cmd_state(int cmd, int *enabled, int *checked);
int cmd_from_key(int vk, int ctrl, int shift, int alt);

/* toolbox.c */
HWND toolbox_create(HWND parent);
int toolbox_width(void);
void toolbox_refresh(void);

/* options.c */
HWND options_create(HWND parent);
int options_height(void);
void options_refresh(void);

/* dock.c */
HWND dock_create(HWND parent);
int dock_width(void);
void dock_refresh(int what);
void dock_reset_workspace(void);
void dock_save_workspace(void);
void dock_load_workspace(void);
void dock_show_panel(int panel);
int dock_panel_visible(int panel);
void dock_toggle_panel(int panel);
void dock_close_flyout(void);
/* Saved workspaces store these numbers: new panels go at the end. */
enum { P_NAVIGATOR, P_INFO, P_COLOR, P_SWATCHES, P_BRUSHES, P_LAYERS, P_CHANNELS, P_PATHS, P_HISTORY, P_ACTIONS,
       P_ADJUSTMENTS, P_COUNT };
#define P_COUNT_V1 10    /* panels in 0.1.0 workspaces */

/* panels.c: per-panel drawing/interaction, used by dock.c */
typedef struct PanelCtx { HWND hwnd; RECT r; } PanelCtx;
const char *panel_name(int p);
int panel_icon(int p);
int panel_min_height(int p);
int panel_pref_height(int p);
void panel_paint(int p, HDC dc, const RECT *r);
void panel_mouse(int p, HWND hwnd, const RECT *r, UINT msg, int x, int y, WPARAM wp);
void panel_menu(int p, HWND hwnd, int sx, int sy);
void panel_layout_controls(int p, HWND host, const RECT *r, int visible);
void panel_command(int p, int id, int code, HWND ctl);
void panel_timer(int p);

/* docarea.c / view.c */
HWND docarea_create(HWND parent);
void docarea_layout(void);
void docarea_cascade(void);
void docarea_tile(int vertical);
void docarea_float_all(int floating);
void view_float(DocView *v, int floating);
HWND view_create(DocView *v);
void view_destroy(DocView *v);
void view_invalidate(DocView *v, const PcRect *doc_rect);
void view_set_zoom(DocView *v, double zoom, int anchor_x, int anchor_y);
void view_zoom_fit(DocView *v, int actual);
void view_title(DocView *v, char *out, int n);
void view_screen_to_doc(DocView *v, int x, int y, float *dx, float *dy);
void view_doc_to_screen(DocView *v, float dx, float dy, int *x, int *y);
void view_canvas_rect(DocView *v, RECT *r);
void view_scroll_to(DocView *v, int sx, int sy);
void view_clamp_scroll(DocView *v);
void view_update_cursor(DocView *v);
extern const double g_zoom_steps[];
extern const int g_zoom_nsteps;

/* tools.c: canvas interaction */
void tool_mouse(DocView *v, UINT msg, int x, int y, WPARAM wp);
int tool_key(DocView *v, int vk, int down);
void tool_paint_overlay(DocView *v, HDC dc);
void tool_cancel(DocView *v);
void tool_commit(DocView *v);
int tool_cursor(DocView *v);
void tool_defaults(void);
int tool_is_painting(int tool);
void tool_begin_free_transform(DocView *v);
void tool_timer(DocView *v);

/* dialogs */
int dlg_new(int *w, int *h, double *dpi, int *mode, int *fill, char *name, int namen);
int dlg_image_size(PcDoc *d, int *w, int *h, double *dpi, int *method);
int dlg_canvas_size(PcDoc *d, int *w, int *h, int *anchor);
int dlg_rotate(double *deg);
int dlg_levels(PcDoc *d, PcLevels *lv);
int dlg_curves(PcDoc *d, PcCurves *cv);
int dlg_hue_sat(PcDoc *d, PcHueSat *hs);
int dlg_color_balance(PcDoc *d, PcColorBalance *cb);
int dlg_brightness_contrast(PcDoc *d, int *b, int *c);
int dlg_threshold(PcDoc *d, int *level);
int dlg_posterize(PcDoc *d, int *levels);
int dlg_channel_mixer(PcDoc *d, int m[3][4], int *mono);
/* An adjustment layer's dialog: previews by changing the active (adjustment)
 * layer's parameters; Invert has none and returns 1 at once. */
int dlg_adjust_layer(PcDoc *d, PcAdjust *a);
int dlg_filter(PcDoc *d, int filter, PcFilterParams *p);
int dlg_color_picker(const char *title, u32 *color);
int dlg_preferences(void);
int dlg_layer_props(char *name, int n, int *opacity, int *blend, int *clip);
int dlg_layer_style(PcDoc *d, PcLayer *l);
int dlg_transform_numeric(float *dx, float *dy, float *sw, float *sh, float *angle, float *skx, float *sky);
int dlg_print_setup(DocView *v);
int dlg_fill(int *use, int *opacity, int *blend, int *preserve);
int dlg_stroke(int *width, int *location, int *opacity, int *blend);
int dlg_feather(float *radius);
int dlg_modify_sel(const char *title, const char *label, int *px);
int dlg_color_range(PcDoc *d, int *fuzz, u32 *color);
int dlg_type(PcDoc *d, PcText *t, int is_new);
int dlg_brush_options(PcBrush *b);
int dlg_histogram(PcDoc *d);
int dlg_about(void);
int dlg_open_file(char *path, int n);
int dlg_save_file(char *path, int n, int *fmt, int layered_ok);
int dlg_format_options(int fmt, PcSaveOpts *o);
int dlg_text_input(const char *title, const char *label, char *buf, int n);
int dlg_message(const char *title, const char *text, int icon, int buttons);  /* buttons: 0 OK, 1 OK/Cancel, 2 Yes/No/Cancel */
int dlg_duplicate(char *name, int n, int merged_option, int *merged);
int dlg_new_layer(char *name, int n, int *opacity, int *blend, int *clip);
int dlg_action_name(char *name, int n);
enum { MB_ICON_INFO_, MB_ICON_WARN_, MB_ICON_ERR_, MB_ICON_Q_ };

/* actions.c */
void actions_record(int cmd, const void *params, int nbytes);
void actions_begin_record(void);
void actions_stop(void);
void actions_play_selected(void);
int actions_is_playing(void);
const void *actions_params(int *nbytes);   /* parameters while playing a step, else NULL */
void actions_load(void);
void actions_save(void);

/* options.c / panels.c */
void options_get_type(PcText *t);
void options_set_type(const PcText *t);
void options_set_readout(const char *s);
int options_auto_select(void);
int panels_mask_target(void);
void panels_set_mask_target(int on);
void dock_classic_workspace(void);
void dock_store_named(void);
int dock_load_named(void);
int cmd_label(int cmd, char *buf, int n);
int adjust_icon(int type);                  /* panels.c: PC_ICON_ADJ_* of a PC_ADJ_* type */

/* misc helpers (app.c) */
u32 app_sample_color(DocView *v, int x, int y, int size, int merged);
void app_update_info(DocView *v, float dx, float dy);
void app_apply_channel_restriction(DocView *v, const PcDocState *before);

#endif
