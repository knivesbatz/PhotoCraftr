/*
 * PhotoCraft Classic - file formats.
 *
 * All codecs work on memory buffers so they are portable and testable; the
 * platform layer reads and writes the files.  Loading never trusts input:
 * every size and offset is bounds-checked and absurd dimensions are refused.
 */
#ifndef PCIO_H
#define PCIO_H

#include "core/pcdoc.h"
#include "core/pcquant.h"

typedef enum {
    PC_FMT_UNKNOWN = 0,
    PC_FMT_PSD, PC_FMT_BMP, PC_FMT_GIF, PC_FMT_JPEG, PC_FMT_PNG, PC_FMT_TIFF, PC_FMT_TGA,
    PC_FMT_COUNT
} PcFormat;

/*
 * The 1998 output rule: flat formats are written only at the colour depths
 * a game studio of 1998 could produce (bits per pixel):
 *
 *   BMP   8 (palette), 16 (5-5-5 BI_RGB, or 5-6-5 BI_BITFIELDS), 24
 *   TGA   8 (colour-mapped), 16 (A1R5G5B5), 24, 32 (8-bit alpha); RLE optional
 *   GIF   8 (GIF89a, transparency, interlace)
 *   PNG   8 (PLTE, tRNS), 24, 32 - never 16 bits per channel
 *   TIFF  8 (palette colour), 24; none / LZW / PackBits
 *   JPEG  8 (grayscale) or 24: baseline, 4:2:0 chroma, Photoshop 0-12 quality
 *   PSD   8 bits per channel (8 = gray, 24/32 = RGB): depth is not a choice
 *
 * Grayscale documents write depth 8 as an 8-bit gray image (gray palette,
 * gray Targa/PNG/TIFF/JPEG).  Depth 0 means "the format's default for this
 * document".  pc_save_mem refuses every other depth, whatever the caller.
 */
typedef struct PcSaveOpts {
    int depth;            /* bits per pixel (see above); 0 = default */
    int green6;           /* 16-bit BMP: 5-6-5 via BI_BITFIELDS instead of 5-5-5 */
    PcQuantOpts q;        /* depth 8 palette / colours / dither (dither also for 16-bit),
                           * transparency, colour key */
    int interlace;        /* GIF */
    int tiff_compress;    /* 0 none, 1 LZW, 2 PackBits */
    int tga_rle;
    int jpeg_quality;     /* 0..12 Photoshop-style scale (mapped to 1..100); default 8 */
} PcSaveOpts;

void pc_save_opts_default(PcSaveOpts *o);

/* Allowed depths of a format, ascending (at most 4); returns the count. */
int pc_format_depths(int fmt, int *depths);
int pc_format_depth_ok(int fmt, int depth);  /* 0 is always allowed */
/* The default depth: BMP/TIFF/JPEG 24, TGA/PNG 24 (32 with transparency),
 * GIF 8; gray documents 8 (PNG 32 with transparency, TGA 32 likewise). */
int pc_format_default_depth(int fmt, int gray, int transparent);
/* Whether the document's composite has any transparent pixel. */
int pc_doc_has_transparency(const PcDoc *d);
/* The image exactly as it will be stored (palette colours, high colour,
 * JPEG artefacts; alpha 0 where the file is transparent), for previews.
 * Free with pc_buf_unref. */
PcBuf *pc_save_preview(const PcDoc *d, int fmt, const PcSaveOpts *o, char *err, size_t errn);

const char *pc_format_name(int fmt);          /* "Photoshop (*.PSD)" style label */
const char *pc_format_ext(int fmt);           /* "psd" */
int pc_format_from_ext(const char *filename); /* by extension */
int pc_format_detect(const u8 *data, size_t n);/* by signature */
int pc_format_can_layers(int fmt);            /* only PSD keeps layers */

/* Load: returns a new document (history reset to an "Open" state) or NULL
 * with a human-readable message in err. */
PcDoc *pc_load_mem(const u8 *data, size_t n, const char *filename, char *err, size_t errn);

/* Save: allocates *out (free with free()). Formats other than PSD save the
 * flattened composite. Returns 0 on success. */
int pc_save_mem(const PcDoc *d, int fmt, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn);

/* Individual codecs (used by the dispatcher and the tests). */
PcDoc *pc_psd_load(const u8 *data, size_t n, char *err, size_t errn);
int pc_psd_save(const PcDoc *d, u8 **out, size_t *outn, char *err, size_t errn);
PcDoc *pc_tiff_load(const u8 *data, size_t n, char *err, size_t errn);
int pc_tiff_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn);
PcDoc *pc_bmp_load(const u8 *data, size_t n, char *err, size_t errn);
int pc_bmp_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn);
PcDoc *pc_gif_load(const u8 *data, size_t n, char *err, size_t errn);
int pc_gif_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn);
PcDoc *pc_tga_load(const u8 *data, size_t n, char *err, size_t errn);
int pc_tga_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn);
PcDoc *pc_png_load(const u8 *data, size_t n, char *err, size_t errn);
int pc_png_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn);
PcDoc *pc_jpeg_load(const u8 *data, size_t n, char *err, size_t errn);
int pc_jpeg_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn);

/* Limits applied by every loader. */
enum { PC_MAX_DIM = 30000 };
#define PC_MAX_PIXELS ((size_t)100 * 1000 * 1000)

#endif
