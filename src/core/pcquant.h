/*
 * PhotoCraft Classic - colour reduction the way a 1998 studio did it.
 *
 * One quantizer serves Image > Mode > Indexed Color and every writer that
 * stores fewer than 24 bits per pixel, so what the dialog previews is what
 * lands in the file.  The palettes are Photoshop 5's Indexed Color list
 * (Exact, System (Windows), System (Mac OS), Web, Uniform, Adaptive, plus
 * the document's own palette after an Indexed Color conversion), the
 * dithers are None, Pattern (8x8 Bayer) and Diffusion (Floyd-Steinberg).
 *
 * Game studios keyed sprites on a solid colour rather than alpha: with
 * `colorkey` on, transparent pixels become key_rgb (magenta by default)
 * and that entry is reserved in the palette so no opaque pixel lands on it
 * by accident.
 *
 * Integer arithmetic only (deterministic on x87 and on the host).
 */
#ifndef PCQUANT_H
#define PCQUANT_H

#include "pcdoc.h"

enum {
    PC_PAL_EXACT, PC_PAL_WINDOWS, PC_PAL_MACOS, PC_PAL_WEB, PC_PAL_UNIFORM, PC_PAL_ADAPTIVE,
    PC_PAL_DOCUMENT,      /* the palette kept by Image > Mode > Indexed Color */
    PC_PAL_COUNT
};
enum { PC_DITHER_NONE, PC_DITHER_PATTERN, PC_DITHER_DIFFUSION, PC_DITHER_COUNT };

#define PC_COLORKEY_DEFAULT 0xFF00FFu    /* magenta, the classic sprite key */

/* Plain data (no pointers): recorded as-is by the Actions palette. */
typedef struct PcQuantOpts {
    int palette;          /* PC_PAL_* */
    int colors;           /* 2..256 for Uniform and Adaptive */
    int dither;           /* PC_DITHER_*: indexed and 16-bit high colour */
    int transparency;     /* pixels with alpha < 128 get a transparent index (GIF, PNG-8) */
    int colorkey;         /* pixels with alpha < 128 become key_rgb, reserved in the palette */
    u32 key_rgb;          /* 0xRRGGBB */
    int full_alpha;       /* modern PNG-8 exception: alpha is a fourth component and palette
                           * entries carry partial alpha (Exact / Adaptive palettes; only
                           * fully transparent pixels share the transparent entry) */
} PcQuantOpts;

typedef struct PcIndexed {
    int w, h;
    int ncolors;          /* palette entries in use, 1..256 */
    int transparent;      /* reserved (transparent / colour key) index, -1 = none */
    u8 pal[768];          /* RGB triples */
    u8 alpha[256];        /* per-entry alpha: 255, 0 for the transparent entry when
                           * transparency is on, any value with full_alpha */
    u8 *idx;              /* w*h indices (free with pc_indexed_free) */
} PcIndexed;

void pc_quant_defaults(PcQuantOpts *q);    /* Adaptive 256, Diffusion, transparency on, magenta key off */
const char *pc_palette_name(int palette);  /* "System (Windows)" ... */
const char *pc_dither_name(int dither);    /* "None", "Pattern", "Diffusion" */

/* The fixed palettes (Windows, Mac OS, Web, Uniform with `colors`):
 * fills pal (768 bytes) and returns the entry count, 0 for the others. */
int pc_palette_builtin(int palette, int colors, u8 *pal);

/* Distinct colours of the opaque pixels (alpha >= 128), counting up to
 * limit + 1 (so "more than 256" is cheap to ask). */
int pc_count_colors(const u8 *rgba, size_t npx, int limit);

/* Reduce RGBA to a palette.  Pixels with alpha < 128 are transparent when
 * q->transparency or q->colorkey is on; all others are matted on white.
 * doc_pal/doc_n/doc_t: the document palette for PC_PAL_DOCUMENT (NULL
 * falls back to Adaptive).  Colours already in the palette are kept
 * exactly.  Returns 0, or -1 when out of memory. */
int pc_quantize(const u8 *rgba, int w, int h, const PcQuantOpts *q,
                const u8 *doc_pal, int doc_n, int doc_t, PcIndexed *out);
void pc_indexed_free(PcIndexed *x);

/* ---- 16-bit high colour (5-5-5, or 5-6-5 with green6) ---- */

static inline int pc_q5(int v) { return (v * 31 + 127) / 255; }     /* 8 -> 5 bits, nearest */
static inline int pc_x5(int q) { return (q * 255 + 15) / 31; }      /* 5 -> 8 bits */
static inline int pc_q6(int v) { return (v * 63 + 127) / 255; }
static inline int pc_x6(int q) { return (q * 255 + 31) / 63; }
static inline unsigned pc_hicolor_pack(int r, int g, int b, int green6)
{
    return green6 ? (unsigned)(pc_q5(r) << 11 | pc_q6(g) << 5 | pc_q5(b))
                  : (unsigned)(pc_q5(r) << 10 | pc_q5(g) << 5 | pc_q5(b));
}
/* Round every pixel's colour to high colour in place (stored back as the
 * 8-bit values a reader expands them to), with the given dither.  Alpha
 * is left alone. */
void pc_hicolor_reduce(u8 *rgba, int w, int h, int green6, int dither);

/* Image > Mode > Indexed Color: flattens to one layer (transparent pixels
 * stay transparent when q->transparency is on), maps every pixel onto the
 * palette and remembers the palette in the document.  The caller records
 * history.  Returns 0 or -1. */
int pc_doc_to_indexed(PcDoc *d, const PcQuantOpts *q);

#endif
