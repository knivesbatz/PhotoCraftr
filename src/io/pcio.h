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

typedef enum {
    PC_FMT_UNKNOWN = 0,
    PC_FMT_PSD, PC_FMT_BMP, PC_FMT_GIF, PC_FMT_JPEG, PC_FMT_PNG, PC_FMT_TIFF, PC_FMT_TGA,
    PC_FMT_COUNT
} PcFormat;

typedef struct PcSaveOpts {
    int jpeg_quality;     /* 0..12 Photoshop-style scale (mapped to 1..100); default 8 */
    int tiff_compress;    /* 0 none, 1 LZW, 2 PackBits */
    int gif_dither;       /* 0 none, 1 diffusion */
    int gif_transparency; /* keep fully transparent pixels transparent */
    int gif_interlace;
    int png_interlace;    /* ignored (always non-interlaced) */
    int bmp_depth;        /* 24 or 32 (32 keeps alpha); gray docs write 8-bit */
    int tga_rle;
    int save_alpha;       /* keep transparency where the format allows */
} PcSaveOpts;

void pc_save_opts_default(PcSaveOpts *o);

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
