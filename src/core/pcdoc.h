/*
 * PhotoCraft Classic - document model.
 *
 * Portable C (no Win32): compiled into the Windows app and into the native
 * test binary.  The model follows PhotoCraft's engine/UI seam
 * (crates/doc + crates/ops): the document is plain data, every change goes
 * through a function here, and history snapshots share pixel buffers
 * copy-on-write so undo costs memory only for what actually changed.
 *
 * Pixels are 8-bit RGBA, straight (non-premultiplied) alpha.  Every layer
 * covers the whole canvas.  Grayscale documents keep R=G=B and are written
 * as single-channel files.
 *
 * Invariant: never write into a PcBuf obtained from a layer without first
 * calling pc_buf_writable() (or the pc_layer_*_w helpers) - buffers may be
 * shared with history states.
 */
#ifndef PCDOC_H
#define PCDOC_H

#include "pcrt.h"

typedef unsigned char u8;
typedef unsigned int u32;

/* ---------------- shared pixel buffers ---------------- */

typedef struct PcBuf {
    int refs;
    int w, h, ch;      /* ch: 4 (RGBA) or 1 (mask/selection/alpha channel) */
    u8 *px;            /* w*h*ch bytes */
} PcBuf;

PcBuf *pc_buf_new(int w, int h, int ch, int zero);
PcBuf *pc_buf_clone(const PcBuf *b);
PcBuf *pc_buf_ref(PcBuf *b);
void pc_buf_unref(PcBuf *b);
/* Ensure *pb is exclusively owned (clone when shared). Returns NULL on OOM. */
PcBuf *pc_buf_writable(PcBuf **pb);

/* ---------------- layers ---------------- */

typedef enum {
    PC_BLEND_NORMAL, PC_BLEND_DISSOLVE, PC_BLEND_MULTIPLY, PC_BLEND_SCREEN,
    PC_BLEND_OVERLAY, PC_BLEND_SOFT_LIGHT, PC_BLEND_HARD_LIGHT,
    PC_BLEND_COLOR_DODGE, PC_BLEND_COLOR_BURN, PC_BLEND_DARKEN,
    PC_BLEND_LIGHTEN, PC_BLEND_DIFFERENCE, PC_BLEND_EXCLUSION,
    PC_BLEND_HUE, PC_BLEND_SATURATION, PC_BLEND_COLOR, PC_BLEND_LUMINOSITY,
    PC_BLEND_COUNT
} PcBlend;

const char *pc_blend_name(int blend);        /* "Normal", "Multiply", ... */
const char *pc_blend_psd_key(int blend);     /* "norm", "mul ", ... (4 chars) */
int pc_blend_from_psd_key(const char *key4); /* unknown -> NORMAL */

typedef enum { PC_LAYER_PIXEL = 0, PC_LAYER_TEXT = 1, PC_LAYER_ADJUST = 2 } PcLayerKind;

/* ---------------- adjustment parameters ---------------- */

/* Shared by Image > Adjust (destructive, pcops.h) and adjustment layers. */
typedef struct PcLevels {
    int in_black[4], in_white[4];   /* [0] = composite RGB, 1..3 = R,G,B */
    float gamma[4];
    int out_black[4], out_white[4];
} PcLevels;

typedef struct PcCurve { int n; int x[16], y[16]; } PcCurve;   /* points sorted by x, 0..255 */
typedef struct PcCurves { PcCurve c[4]; } PcCurves;            /* [0] composite */

typedef struct PcColorBalance {
    int v[3][3];     /* [shadows/midtones/highlights][cyan-red, magenta-green, yellow-blue] -100..100 */
    int preserve_luminosity;
} PcColorBalance;

typedef struct PcHueSat {
    int hue[7], sat[7], light[7];   /* [0] master, 1..6 reds yellows greens cyans blues magentas */
    int colorize;                   /* uses hue[0] 0..360, sat[0] 0..100, light[0] */
} PcHueSat;

/* Adjustment layer types: the ones Photoshop 5.0 (1998) offered, in this order. */
enum {
    PC_ADJ_BRIGHTNESS_CONTRAST, PC_ADJ_LEVELS, PC_ADJ_CURVES, PC_ADJ_COLOR_BALANCE, PC_ADJ_HUE_SAT,
    PC_ADJ_CHANNEL_MIXER, PC_ADJ_INVERT, PC_ADJ_THRESHOLD, PC_ADJ_POSTERIZE, PC_ADJ_COUNT
};

typedef struct PcAdjust {
    int type;                       /* PC_ADJ_*; only the matching fields are used */
    int brightness, contrast;       /* -100..100 */
    PcLevels levels;
    PcCurves curves;
    PcColorBalance balance;
    PcHueSat huesat;
    int mixer[3][4];                /* output R,G,B from source R,G,B %, last = constant % */
    int mixer_mono;
    int threshold;                  /* 1..255 */
    int posterize;                  /* 2..255 */
} PcAdjust;

/* Prepared form: lookup tables or a per-pixel function (adjust.c). */
typedef struct PcAdjPrep {
    int type, use_lut;
    u8 lut[3][256];
    PcAdjust a;
} PcAdjPrep;

const char *pc_adjust_name(int type);            /* "Brightness/Contrast", ... */
void pc_adjust_default(PcAdjust *a, int type);   /* identity parameters (Threshold 128, Posterize 4) */
void pc_adjust_prepare(const PcAdjust *a, PcAdjPrep *p);
void pc_adjust_pixel(const PcAdjPrep *p, u8 *rgb);    /* in place, 3 bytes */

typedef struct PcText {
    char text[1024];      /* lines separated by '\n' */
    char font[64];        /* face name */
    int size;             /* pixels */
    u32 color;            /* 0xRRGGBB */
    int x, y;             /* anchor (top-left of the first line, doc pixels) */
    int bold, italic, underline;
    int vertical;
    int antialias;        /* 0 none, 1 crisp, 2 strong, 3 smooth */
    int align;            /* 0 left, 1 center, 2 right */
    int leading;          /* line spacing in pixels, 0 = auto */
    int tracking;         /* extra pixels between characters */
} PcText;

typedef struct PcLayer {
    char name[64];
    PcBuf *pix;           /* RGBA, doc size */
    PcBuf *mask;          /* 1 channel, doc size, or NULL (255 = visible) */
    int mask_enabled;
    int mask_linked;
    int opacity;          /* 0..255 */
    int blend;            /* PcBlend */
    int visible;
    int lock_alpha;       /* "Lock transparent pixels" / PS5 Preserve Transparency */
    int lock_pixels;
    int lock_position;
    int is_background;
    int clip;             /* clipped to the layer below (PS5 clipping group) */
    int kind;             /* PcLayerKind */
    PcText text;
    PcAdjust adj;         /* PC_LAYER_ADJUST: pix is NULL; the mask says where it applies */
    /* Layer style (a 1998-sized subset: drop shadow, inner shadow, outer glow, bevel). */
    int fx_enabled;
    int fx_shadow, fx_shadow_opacity, fx_shadow_angle, fx_shadow_dist, fx_shadow_blur;
    u32 fx_shadow_color;
    int fx_glow, fx_glow_opacity, fx_glow_size;
    u32 fx_glow_color;
    int fx_bevel, fx_bevel_depth, fx_bevel_size, fx_bevel_angle;
    int fx_inner_shadow, fx_inner_opacity, fx_inner_dist, fx_inner_blur;
} PcLayer;

/* ---------------- paths ---------------- */

typedef struct PcPathPt {
    float x, y;           /* anchor */
    float ix, iy;         /* incoming control point (absolute) */
    float ox, oy;         /* outgoing control point (absolute) */
    int smooth;           /* handles are kept collinear */
    int start;            /* first point of a sub-path */
    int closed;           /* sub-path containing this point is closed (set on its start point) */
} PcPathPt;

typedef struct PcPath {
    char name[32];
    int n, cap;
    PcPathPt *pts;
} PcPath;

/* ---------------- alpha channels ---------------- */

typedef struct PcChannel {
    char name[32];
    PcBuf *buf;           /* 1 channel, doc size */
    u32 color;            /* overlay colour when shown */
    int visible;
} PcChannel;

/* ---------------- document ---------------- */

enum { PC_MODE_RGB = 0, PC_MODE_GRAY = 1 };
enum { PC_MAX_LAYERS = 256, PC_MAX_CHANNELS = 24, PC_MAX_PATHS = 32 };

typedef struct PcDocState {
    int w, h, mode;
    double dpi;
    int nlayers, active;
    PcLayer *layers[PC_MAX_LAYERS];   /* index 0 = bottom */
    PcBuf *sel;                        /* selection mask, NULL = none */
    int nchannels;
    PcChannel channels[PC_MAX_CHANNELS];
    int npaths, active_path;           /* active_path -1 = none */
    PcPath paths[PC_MAX_PATHS];        /* "Work Path" is created on demand */
    int quickmask;                     /* Quick Mask mode: sel shown as overlay & edited as pixels */
} PcDocState;

typedef struct PcHistEntry {
    char label[48];
    PcDocState *state;
    int snapshot;                      /* named snapshot (kept when list is trimmed) */
} PcHistEntry;

typedef struct PcDoc {
    PcDocState s;                      /* live state (owns references) */
    /* history */
    PcHistEntry *hist;
    int nhist, hist_cap, hist_cur;     /* hist_cur = index of the state the doc is in */
    int hist_limit;                    /* max non-snapshot states (PS default 20) */
    /* not part of history */
    char path[260];                    /* full file name, "" when never saved */
    char title[128];                   /* window title base, e.g. "Untitled-1" */
    int format;                        /* PcFormat it was loaded from */
    int dirty;
    unsigned revision;                 /* bumps on every change (view caches key on it) */
    int history_source;                /* hist index used by the History Brush, -1 = initial */
} PcDoc;

PcDoc *pc_doc_new(int w, int h, int mode, double dpi, int fill, u32 rgb);
        /* fill: 0 white, 1 background colour rgb, 2 transparent (no background layer) */
void pc_doc_free(PcDoc *d);

PcLayer *pc_layer_new(int w, int h, const char *name);
PcLayer *pc_layer_new_adjust(const PcAdjust *a, const char *name);   /* no pixels, no mask */
PcLayer *pc_layer_clone(const PcLayer *l);      /* shares buffers */
void pc_layer_free(PcLayer *l);
u8 *pc_layer_pixels_w(PcLayer *l);              /* writable RGBA pointer or NULL (also for adjustment layers) */
static inline int pc_layer_is_adjust(const PcLayer *l) { return l && l->kind == PC_LAYER_ADJUST; }
u8 *pc_layer_mask_w(PcLayer *l);                /* writable mask pointer or NULL (no mask/OOM) */

PcLayer *pc_doc_active(PcDoc *d);
int pc_doc_insert_layer(PcDoc *d, int index, PcLayer *l);  /* takes ownership; returns 0 ok */
PcLayer *pc_doc_add_layer(PcDoc *d, const char *name);     /* above active, becomes active */
/* Adjustment layer above the active layer (becomes active) with a layer mask:
 * the selection when from_selection and there is one, else reveal all.
 * name NULL = the type's name. */
PcLayer *pc_doc_add_adjustment(PcDoc *d, const PcAdjust *a, const char *name, int from_selection);
int pc_doc_delete_layer(PcDoc *d, int index);
int pc_doc_move_layer(PcDoc *d, int from, int to);
int pc_doc_duplicate_layer(PcDoc *d, int index);
int pc_doc_merge_down(PcDoc *d, int index);
int pc_doc_merge_visible(PcDoc *d);
int pc_doc_flatten(PcDoc *d);
int pc_doc_layer_from_background(PcDoc *d);    /* "Layer 0" from Background */
int pc_doc_add_mask(PcDoc *d, int index, int reveal_all, int from_selection);
int pc_doc_apply_mask(PcDoc *d, int index, int apply); /* apply=0 discards */
void pc_doc_unique_layer_name(PcDoc *d, char *out, size_t n);  /* "Layer N" */

/* Composite visible layers into a new RGBA buffer (alpha = coverage).
 * rect may be NULL for the whole canvas; otherwise only that area is
 * computed into the full-size buffer `out` (which must be w*h RGBA). */
typedef struct { int x0, y0, x1, y1; } PcRect;   /* half-open */
PcBuf *pc_doc_composite(const PcDoc *d);
PcBuf *pc_doc_composite_below(const PcDoc *d, int index);   /* visible layers under index */
void pc_doc_composite_into(const PcDoc *d, PcBuf *out, const PcRect *r);
/* Composite against the transparency checkerboard or a flat colour, as BGRX
 * scanlines for the display (top-down). */
void pc_composite_rgba_over(const u8 *src, int count, u8 *dst, int mode, u32 bg);

/* Blend one layer pixel (sr,sg,sb,sa already multiplied by opacity/mask)
 * onto a destination pixel in place. */
void pc_blend_pixel(int blend, u8 *dst, const u8 *src, int sa, unsigned noise);

/* ---------------- selection ---------------- */

PcBuf *pc_sel_get(PcDoc *d, int create);        /* writable selection, created empty when create */
void pc_sel_clear(PcDoc *d);                    /* Deselect */
int pc_sel_bounds(const PcDoc *d, PcRect *r);   /* 0 when nothing selected */
int pc_sel_has(const PcDoc *d);
/* Value 0..255 of the selection at a pixel (255 when there is no selection). */
static inline int pc_sel_at(const PcDoc *d, int x, int y)
{
    return d->s.sel ? d->s.sel->px[(size_t)y * d->s.w + x] : 255;
}

/* ---------------- history ---------------- */

/* Record the current state as a new history entry (after an edit). */
void pc_hist_commit(PcDoc *d, const char *label);
int pc_hist_can_undo(const PcDoc *d);
int pc_hist_can_redo(const PcDoc *d);
int pc_hist_goto(PcDoc *d, int index);          /* jump to any state (History palette) */
int pc_hist_undo(PcDoc *d);
int pc_hist_redo(PcDoc *d);
int pc_hist_snapshot(PcDoc *d, const char *name);
void pc_hist_reset(PcDoc *d, const char *label); /* clears history; current state becomes the first entry */
const PcDocState *pc_hist_state(const PcDoc *d, int index);
/* Undo the last commit without recording (cancelling an interactive edit). */
void pc_state_copy(PcDocState *dst, const PcDocState *src);   /* shares buffers */
void pc_state_release(PcDocState *s);

/* ---------------- paths ---------------- */

PcPath *pc_path_work(PcDoc *d);                 /* active path, creating "Work Path" */
void pc_path_add_point(PcPath *p, const PcPathPt *pt);
void pc_path_free_pts(PcPath *p);
void pc_path_copy(PcPath *dst, const PcPath *src);
/* Flatten to a polygon list for rasterising; returns number of points written.
 * Sub-path starts are marked in `starts` (1 = new polygon). */
int pc_path_flatten(const PcPath *p, float *xy, u8 *starts, int max_pts);

/* ---------------- utilities ---------------- */

void pc_doc_touch(PcDoc *d);                    /* dirty + revision++ */
size_t pc_doc_memory(const PcDoc *d);           /* approximate bytes held (for Info/status) */
u32 pc_rgb(int r, int g, int b);
static inline int pc_r(u32 c) { return (int)((c >> 16) & 255); }
static inline int pc_g(u32 c) { return (int)((c >> 8) & 255); }
static inline int pc_b(u32 c) { return (int)(c & 255); }
static inline int pc_mul255(int a, int b) { int t = a * b + 128; return (t + (t >> 8)) >> 8; }
static inline int pc_luma(int r, int g, int b) { return (r * 77 + g * 151 + b * 28) >> 8; }

/* Text rendering is supplied by the platform layer (GDI on Windows): it must
 * render t into a doc-sized RGBA buffer (premultiplication not applied). */
typedef int (*PcTextRenderFn)(const PcText *t, PcBuf *rgba, void *user);
void pc_set_text_renderer(PcTextRenderFn fn, void *user);
int pc_text_render(const PcText *t, PcBuf *rgba);  /* 0 ok; -1 no renderer */
int pc_layer_rerender_text(PcLayer *l, int w, int h);

#endif
