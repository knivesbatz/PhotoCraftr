/*
 * PhotoCraft Classic - editing operations on a PcDoc.
 *
 * Functions that change pixels act on the active layer (or the selection
 * area of it), honour "lock transparent pixels", and leave history
 * recording to the caller (UI / command layer calls pc_hist_commit with the
 * menu label).  They return 0 on success, -1 on failure/no-op.
 */
#ifndef PCOPS_H
#define PCOPS_H

#include "pcdoc.h"

/* ---------------- shared helpers (ops.c) ---------------- */

/* Mix `res` into the layer's pixels through the selection (or everywhere),
 * respecting lock_alpha.  `res` is a full-size RGBA buffer computed from
 * the current pixels. */
int pc_apply_result(PcDoc *d, PcLayer *l, const u8 *res);
/* Same but for an arbitrary destination mask/weights (NULL = selection). */
int pc_apply_result_masked(PcDoc *d, PcLayer *l, const u8 *res, const u8 *weights);
/* Target buffer for painting/filters: active layer pixels, or the active
 * layer mask when `mask_target` is set (then values are gray RGBA). */
void pc_rgb_to_hsl(int r, int g, int b, float *h, float *s, float *l);
void pc_hsl_to_rgb(float h, float s, float l, int *r, int *g, int *b);
void pc_rgb_to_hsb(int r, int g, int b, float *h, float *s, float *v);
void pc_hsb_to_rgb(float h, float s, float v, int *r, int *g, int *b);
void pc_rgb_to_lab(int r, int g, int b, float *L, float *A, float *B);
void pc_lab_to_rgb(float L, float A, float B, int *r, int *g, int *bb);
void pc_rgb_to_cmyk(int r, int g, int b, int *c, int *m, int *y, int *k);
void pc_cmyk_to_rgb(int c, int m, int y, int k, int *r, int *g, int *b);
unsigned pc_rand(unsigned *state);

/* ---------------- adjustments (adjust.c) ---------------- */

typedef struct PcLevels {
    int in_black[4], in_white[4];   /* [0] = composite RGB, 1..3 = R,G,B */
    float gamma[4];
    int out_black[4], out_white[4];
} PcLevels;
void pc_levels_default(PcLevels *lv);
void pc_levels_lut(const PcLevels *lv, u8 lut[3][256]);
int pc_adj_levels(PcDoc *d, const PcLevels *lv);
int pc_adj_auto_levels(PcDoc *d);

typedef struct PcCurve { int n; int x[16], y[16]; } PcCurve;   /* points sorted by x, 0..255 */
typedef struct PcCurves { PcCurve c[4]; } PcCurves;            /* [0] composite */
void pc_curves_default(PcCurves *cv);
void pc_curve_lut(const PcCurve *c, u8 lut[256]);              /* natural cubic spline */
int pc_adj_curves(PcDoc *d, const PcCurves *cv);

int pc_adj_brightness_contrast(PcDoc *d, int brightness, int contrast);   /* -100..100 */

typedef struct PcColorBalance {
    int v[3][3];     /* [shadows/midtones/highlights][cyan-red, magenta-green, yellow-blue] -100..100 */
    int preserve_luminosity;
} PcColorBalance;
int pc_adj_color_balance(PcDoc *d, const PcColorBalance *cb);

typedef struct PcHueSat {
    int hue[7], sat[7], light[7];   /* [0] master, 1..6 reds yellows greens cyans blues magentas */
    int colorize;                   /* uses hue[0] 0..360, sat[0] 0..100, light[0] */
} PcHueSat;
int pc_adj_hue_sat(PcDoc *d, const PcHueSat *hs);

int pc_adj_invert(PcDoc *d);
int pc_adj_desaturate(PcDoc *d);
int pc_adj_equalize(PcDoc *d);
int pc_adj_threshold(PcDoc *d, int level);
int pc_adj_posterize(PcDoc *d, int levels);
int pc_adj_channel_mixer(PcDoc *d, const int m[3][4], int monochrome);  /* %, last = constant */
void pc_histogram(const PcDoc *d, int channel, int use_composite, unsigned hist[256]);  /* channel 0 lum, 1..3 RGB */

/* Generic per-pixel adjustment through 3 LUTs. */
int pc_adj_luts(PcDoc *d, u8 lut[3][256]);

/* ---------------- filters (filters.c) ---------------- */

enum {
    PC_FILTER_BLUR, PC_FILTER_BLUR_MORE, PC_FILTER_GAUSSIAN_BLUR, PC_FILTER_MOTION_BLUR, PC_FILTER_RADIAL_BLUR,
    PC_FILTER_SHARPEN, PC_FILTER_SHARPEN_MORE, PC_FILTER_SHARPEN_EDGES, PC_FILTER_UNSHARP_MASK,
    PC_FILTER_ADD_NOISE, PC_FILTER_DESPECKLE, PC_FILTER_DUST_SCRATCHES, PC_FILTER_MEDIAN,
    PC_FILTER_EMBOSS, PC_FILTER_FIND_EDGES, PC_FILTER_SOLARIZE, PC_FILTER_DIFFUSE, PC_FILTER_WIND,
    PC_FILTER_TRACE_CONTOUR, PC_FILTER_MOSAIC, PC_FILTER_TWIRL, PC_FILTER_PINCH, PC_FILTER_SPHERIZE,
    PC_FILTER_RIPPLE, PC_FILTER_CLOUDS, PC_FILTER_DIFF_CLOUDS, PC_FILTER_OFFSET, PC_FILTER_HIGH_PASS,
    PC_FILTER_MAXIMUM, PC_FILTER_MINIMUM, PC_FILTER_CUSTOM, PC_FILTER_COUNT
};

typedef struct PcFilterParams {
    float radius;      /* gaussian, unsharp, median, high pass, max/min, dust */
    float amount;      /* unsharp %, noise amount, emboss amount %, twirl angle, pinch %, spherize %, ripple amount */
    int threshold;     /* unsharp, dust & scratches */
    int angle;         /* motion blur, emboss, wind direction (0 right 1 left) */
    int distance;      /* motion blur, emboss height */
    int mode;          /* noise: 0 uniform 1 gaussian; radial: 0 spin 1 zoom; diffuse: 0 normal 1 darken 2 lighten; ripple size 0..2 */
    int mono;          /* noise monochromatic */
    int cell;          /* mosaic cell size */
    int dx, dy;        /* offset; radial center (percent) */
    int wrap;          /* offset: 0 transparent/background 1 repeat 2 wrap around */
    u32 fg, bg;        /* clouds colours */
    unsigned seed;
    int kernel[25];    /* custom 5x5 */
    int scale, offset; /* custom */
} PcFilterParams;

void pc_filter_defaults(int filter, PcFilterParams *p);
const char *pc_filter_name(int filter);
/* Compute the filter of `src` (w*h RGBA) into `dst`. Selection is not
 * applied here (pc_filter_apply does that). */
int pc_filter_run(int filter, const PcFilterParams *p, const u8 *src, u8 *dst, int w, int h);
int pc_filter_apply(PcDoc *d, int filter, const PcFilterParams *p);
/* Separable gaussian on RGBA or single-channel buffers (also used by feather). */
void pc_gaussian_rgba(const u8 *src, u8 *dst, int w, int h, float radius);
void pc_gaussian_gray(u8 *buf, int w, int h, float radius);

/* ---------------- transforms (transform.c) ---------------- */

enum { PC_RESAMPLE_NEAREST, PC_RESAMPLE_BILINEAR, PC_RESAMPLE_BICUBIC };
int pc_image_resize(PcDoc *d, int nw, int nh, int method, double dpi);
int pc_canvas_resize(PcDoc *d, int nw, int nh, int anchor /*0..8*/, u32 bg_rgb);
int pc_image_crop(PcDoc *d, const PcRect *r);
int pc_image_crop_rotated(PcDoc *d, float cx, float cy, float w, float h, float angle_deg);
int pc_image_rotate(PcDoc *d, int quarter_turns);          /* 1 = 90 CW, 2 = 180, 3 = 90 CCW */
int pc_image_rotate_arbitrary(PcDoc *d, double degrees, u32 bg_rgb);
int pc_image_flip(PcDoc *d, int vertical);
int pc_layer_flip(PcDoc *d, int vertical);
int pc_layer_rotate(PcDoc *d, int quarter_turns);
int pc_layer_offset(PcDoc *d, int dx, int dy);             /* move tool */
/* Free transform: maps the source quad (selection bounds or layer bounds)
 * to the four destination corners (TL, TR, BR, BL) with perspective. */
int pc_layer_transform_quad(PcDoc *d, const PcRect *src, const float dst[8], int method);
int pc_layer_bounds(const PcLayer *l, int w, int h, PcRect *r);
void pc_resample_rgba(const u8 *src, int sw, int sh, u8 *dst, int dw, int dh, int method);
int pc_doc_to_gray(PcDoc *d);
int pc_doc_to_rgb(PcDoc *d);
int pc_image_trim(PcDoc *d, int transparent);

/* ---------------- selection (select.c) ---------------- */

enum { PC_SEL_REPLACE, PC_SEL_ADD, PC_SEL_SUBTRACT, PC_SEL_INTERSECT };
/* Combine a coverage mask (doc size) into the selection. */
int pc_sel_combine(PcDoc *d, const u8 *mask, int op);
int pc_sel_rect(PcDoc *d, int x0, int y0, int x1, int y1, int op, float feather);
int pc_sel_ellipse(PcDoc *d, int x0, int y0, int x1, int y1, int op, int antialias, float feather);
int pc_sel_polygon(PcDoc *d, const float *xy, int n, int op, int antialias, float feather);
int pc_sel_polygons(PcDoc *d, const float *xy, const u8 *starts, int n, int op, int antialias, float feather);
int pc_sel_magic_wand(PcDoc *d, int x, int y, int tolerance, int contiguous, int sample_merged, int antialias, int op);
int pc_sel_all(PcDoc *d);
int pc_sel_inverse(PcDoc *d);
int pc_sel_feather(PcDoc *d, float radius);
int pc_sel_modify(PcDoc *d, int how /*0 border 1 smooth 2 expand 3 contract*/, int px);
int pc_sel_grow(PcDoc *d, int tolerance, int similar);
int pc_sel_color_range(PcDoc *d, u32 color, int fuzziness, int op);
int pc_sel_save_channel(PcDoc *d, const char *name);
int pc_sel_load_channel(PcDoc *d, int channel, int invert, int op);
int pc_sel_from_layer_alpha(PcDoc *d, int layer, int op);
/* Rasterise polygons into a coverage mask with 4x4 supersampling when aa. */
void pc_raster_polygons(u8 *mask, int w, int h, const float *xy, const u8 *starts, int n, int aa);

/* ---------------- painting (paint.c) ---------------- */

enum {
    PC_PAINT_BRUSH, PC_PAINT_AIRBRUSH, PC_PAINT_PENCIL, PC_PAINT_ERASER,
    PC_PAINT_CLONE, PC_PAINT_PATTERN, PC_PAINT_HISTORY,
    PC_PAINT_BLUR, PC_PAINT_SHARPEN, PC_PAINT_SMUDGE,
    PC_PAINT_DODGE, PC_PAINT_BURN, PC_PAINT_SPONGE
};

typedef struct PcBrush {
    int diameter;     /* pixels */
    int hardness;     /* 0..100 */
    int spacing;      /* % of diameter */
    int roundness;    /* % */
    int angle;
} PcBrush;

typedef struct PcPaintOpts {
    int tool;
    PcBrush brush;
    int opacity;      /* 0..100 (pressure for airbrush/blur/sharpen/smudge, exposure for dodge/burn/sponge) */
    int blend;        /* painting mode */
    u32 color;        /* foreground */
    u32 bg;           /* background (eraser on Background layer) */
    int wet_edges;
    int fade_steps;   /* 0 = no fade */
    int eraser_mode;  /* 0 paintbrush 1 airbrush 2 pencil 3 block */
    int range;        /* dodge/burn: 0 shadows 1 midtones 2 highlights; sponge: 0 desaturate 1 saturate */
    int aligned;      /* clone/pattern */
    int sample_merged;
    int finger;       /* smudge finger painting */
    int erase_to_history;
    int auto_erase;   /* pencil */
    int target_mask;  /* 0 layer pixels, 1 layer mask, 2 quick mask (selection), 3 alpha channel */
    int channel;      /* alpha channel index when target_mask == 3 */
} PcPaintOpts;

typedef struct PcStroke PcStroke;
PcStroke *pc_stroke_begin(PcDoc *d, const PcPaintOpts *o, float x, float y);
/* Continue to (x,y); returns the dirty rect touched since the last call. */
void pc_stroke_to(PcStroke *s, float x, float y, PcRect *dirty);
void pc_stroke_tick(PcStroke *s, PcRect *dirty);         /* airbrush build-up while held */
void pc_stroke_end(PcStroke *s);
void pc_stroke_set_clone_source(PcStroke *s, float sx, float sy);
/* Clone source is remembered between strokes (Alt+click). */
void pc_clone_set_source(PcDoc *d, float x, float y);
void pc_pattern_set(const u8 *rgba, int w, int h);      /* Edit > Define Pattern */
int pc_pattern_get(const u8 **rgba, int *w, int *h);
/* Stroke a path / polygon with the given paint options (Stroke Path). */
int pc_paint_polyline(PcDoc *d, const PcPaintOpts *o, const float *xy, const u8 *starts, int n);

/* ---------------- fills (fill.c) ---------------- */

int pc_fill_color(PcDoc *d, u32 rgb, int opacity, int blend, int preserve_alpha);   /* selection or all */
int pc_fill_pattern(PcDoc *d, int opacity, int blend);
int pc_fill_history(PcDoc *d, int opacity, int blend);
int pc_stroke_selection(PcDoc *d, int width, int location /*0 inside 1 center 2 outside*/, u32 rgb, int opacity, int blend);
int pc_bucket_fill(PcDoc *d, int x, int y, u32 rgb, int tolerance, int contiguous, int antialias,
                   int sample_merged, int use_pattern, int opacity, int blend);

enum { PC_GRAD_LINEAR, PC_GRAD_RADIAL, PC_GRAD_ANGLE, PC_GRAD_REFLECTED, PC_GRAD_DIAMOND };
typedef struct PcGradient {
    int type;
    int preset;       /* 0 fg->bg, 1 fg->transparent, 2 transparent->fg, 3 black->white, 4 spectrum */
    u32 fg, bg;
    int opacity, blend, reverse, dither, transparency;
} PcGradient;
int pc_gradient_fill(PcDoc *d, const PcGradient *g, float x0, float y0, float x1, float y1);

/* ---------------- clipboard (ops.c) ---------------- */

typedef struct PcClip { int w, h; u8 *rgba; int x, y; } PcClip;
int pc_copy(PcDoc *d, int merged, PcClip *out);           /* selection bounds of the active layer */
int pc_clear(PcDoc *d, u32 bg);                           /* Edit > Clear */
int pc_paste(PcDoc *d, const PcClip *c, int into);       /* new layer, centred / at source position */
void pc_clip_free(PcClip *c);

#endif
