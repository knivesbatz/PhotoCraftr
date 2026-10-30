/*
 * PhotoCraft Classic - internal helpers shared by the codecs in src/io.
 *
 * Not part of the public API (see pcio.h).  Everything here is portable C
 * on top of the minimal runtime (src/rt/pcrt.h): no stdio, no ctype.
 *
 * Reading never trusts the input: PcRd tracks a position and an error
 * flag, and every read past the end sets the flag and returns zeros, so
 * parsers can read a whole structure and check the flag once.
 */
#ifndef PCIO_INT_H
#define PCIO_INT_H

#include "io/pcio.h"
#include "core/pcquant.h"

typedef unsigned short u16;

/* ---------------- bounds-checked reader ---------------- */

typedef struct PcRd {
    const u8 *p;
    size_t n, pos;
    int err;
} PcRd;

static inline void rd_init(PcRd *r, const u8 *p, size_t n) { r->p = p; r->n = p ? n : 0; r->pos = 0; r->err = 0; }
static inline size_t rd_left(const PcRd *r) { return r->err ? 0 : r->n - r->pos; }

/* Returns a pointer to the next k bytes and advances, or NULL (and sets err). */
static inline const u8 *rd_bytes(PcRd *r, size_t k)
{
    const u8 *q;
    if (r->err || k > r->n - r->pos) { r->err = 1; r->pos = r->n; return NULL; }
    q = r->p + r->pos;
    r->pos += k;
    return q;
}
static inline void rd_skip(PcRd *r, size_t k) { (void)rd_bytes(r, k); }
static inline void rd_seek(PcRd *r, size_t pos) { if (pos > r->n) { r->err = 1; r->pos = r->n; } else r->pos = pos; }
static inline unsigned rd_u8(PcRd *r) { const u8 *q = rd_bytes(r, 1); return q ? q[0] : 0; }
static inline unsigned rd_u16be(PcRd *r) { const u8 *q = rd_bytes(r, 2); return q ? (unsigned)(q[0] << 8 | q[1]) : 0; }
static inline unsigned rd_u16le(PcRd *r) { const u8 *q = rd_bytes(r, 2); return q ? (unsigned)(q[1] << 8 | q[0]) : 0; }
static inline u32 rd_u32be(PcRd *r)
{
    const u8 *q = rd_bytes(r, 4);
    return q ? ((u32)q[0] << 24 | (u32)q[1] << 16 | (u32)q[2] << 8 | q[3]) : 0;
}
static inline u32 rd_u32le(PcRd *r)
{
    const u8 *q = rd_bytes(r, 4);
    return q ? ((u32)q[3] << 24 | (u32)q[2] << 16 | (u32)q[1] << 8 | q[0]) : 0;
}
static inline int rd_i16be(PcRd *r) { unsigned v = rd_u16be(r); return v >= 0x8000 ? (int)v - 0x10000 : (int)v; }
static inline int rd_i32be(PcRd *r) { u32 v = rd_u32be(r); return v >= 0x80000000u ? (int)(v - 0x80000000u) - 0x7FFFFFFF - 1 : (int)v; }
/* A sub-reader over the next len bytes (the parent skips them). */
static inline PcRd rd_sub(PcRd *r, size_t len)
{
    PcRd s;
    const u8 *q = rd_bytes(r, len);
    rd_init(&s, q, q ? len : 0);
    if (!q) s.err = 1;
    return s;
}

static inline unsigned get_u16be(const u8 *q) { return (unsigned)(q[0] << 8 | q[1]); }
static inline unsigned get_u16le(const u8 *q) { return (unsigned)(q[1] << 8 | q[0]); }
static inline u32 get_u32be(const u8 *q) { return (u32)q[0] << 24 | (u32)q[1] << 16 | (u32)q[2] << 8 | q[3]; }
static inline u32 get_u32le(const u8 *q) { return (u32)q[3] << 24 | (u32)q[2] << 16 | (u32)q[1] << 8 | q[0]; }

/* ---------------- growable writer ---------------- */

typedef struct PcWr {
    u8 *p;
    size_t n, cap;
    int err;            /* out of memory */
} PcWr;

int wr_reserve(PcWr *w, size_t add);   /* 0 ok */
void wr_bytes(PcWr *w, const void *src, size_t k);
void wr_fill(PcWr *w, int byte, size_t k);
static inline void wr_u8(PcWr *w, unsigned v) { if (w->n < w->cap || !wr_reserve(w, 1)) w->p[w->n++] = (u8)v; }
void wr_u16be(PcWr *w, unsigned v);
void wr_u16le(PcWr *w, unsigned v);
void wr_u32be(PcWr *w, u32 v);
void wr_u32le(PcWr *w, u32 v);
void wr_put_u32be(PcWr *w, size_t at, u32 v);  /* patch a placeholder */
void wr_put_u32le(PcWr *w, size_t at, u32 v);
void wr_free(PcWr *w);
/* Hands the buffer to the caller (*out, *outn). Returns 0, or -1 when an
 * allocation failed earlier (the buffer is freed and err is set). */
int wr_finish(PcWr *w, u8 **out, size_t *outn, char *err, size_t errn);

/* ---------------- common helpers ---------------- */

void pcio_seterr(char *err, size_t errn, const char *fmt, ...);
int pcio_dims_ok(long long w, long long h);          /* 1..PC_MAX_DIM, w*h <= PC_MAX_PIXELS */

/* Create an empty document of the given size (no layers yet). */
PcDoc *pcio_doc_alloc(int w, int h, int mode, double dpi);
/* Turn a decoded RGBA image into a document: opaque images get a
 * "Background" layer, transparent ones a "Layer 0".  Takes ownership of
 * buf (also on failure). */
PcDoc *pcio_doc_from_buf(PcBuf *buf, int mode, double dpi, int fmt, char *err, size_t errn);
/* Final touches on a loaded document: history "Open", clean, format. */
void pcio_finish(PcDoc *d, int fmt);
/* Wrap a malloc'd RGBA pixel array into a PcBuf (takes ownership of px). */
PcBuf *pcio_buf_wrap(u8 *px, int w, int h);
/* Plausibility check before decoding: refuses images whose pixel count
 * could not come from n bytes at the format's best compression ratio. */
int pcio_plausible(int w, int h, size_t n, unsigned long long px_per_byte);
/* Round a non-negative value to the nearest integer (saturating). */
u32 pcio_uround(double v);
/* Document resolution, 72 when unset. */
static inline double pcio_doc_dpi(const PcDoc *d) { return d->s.dpi > 0 ? d->s.dpi : 72; }
/* dpi from pixels per metre (rounded to whole dpi when within 0.1). */
double pcio_dpi_from_ppm(double ppm);
/* Wrap a buffer into a layer (takes ownership of buf). */
PcLayer *pcio_layer_from_buf(PcBuf *buf, const char *name);

/* Flattened RGBA composite of the visible layers (gray docs keep R=G=B). */
PcBuf *pcio_composite(const PcDoc *d, char *err, size_t errn);
int pcio_has_alpha(const u8 *rgba, size_t npx);
void pcio_matte_white(u8 *rgba, size_t npx);         /* over white, alpha = 255 */
static inline int pcio_gray_of(const u8 *p) { return pc_luma(p[0], p[1], p[2]); }

u32 pcio_crc32(u32 crc, const u8 *p, size_t n);      /* zlib/PNG CRC; start with 0 */

/* ---------------- the 1998 output rule (iocommon.c) ---------------- */

/* The composite reduced to what a flat format stores at a depth:
 *  - depth 32: RGBA untouched (gray documents keep R=G=B);
 *  - depth 24: matted on white, or transparent pixels set to the colour key;
 *  - depth 16: as 24 (Targa with transparency keeps a 1-bit alpha), then
 *    rounded to high colour with the chosen dither;
 *  - depth 8: an 8-bit gray image (gray set; gray documents without
 *    transparency to keep) or palette indices in ix.
 * Refuses depths outside the format's set with a message. */
typedef struct PcFlat {
    PcBuf *c;             /* RGBA (alpha 0/255 except at depth 32) */
    int depth;            /* resolved bits per pixel */
    int gray;             /* depth 8: single gray channel (c holds R=G=B) */
    int alpha;            /* alpha is written (32-bit, Targa 16-bit A1R5G5B5) */
    PcIndexed ix;         /* depth 8 colour (ix.idx NULL otherwise) */
} PcFlat;
int pcio_flat(const PcDoc *d, int fmt, const PcSaveOpts *o, PcFlat *f, char *err, size_t errn);
void pcio_flat_free(PcFlat *f);
/* Depth check alone (used by the writers that do not need pcio_flat). */
int pcio_check_depth(int fmt, int depth, char *err, size_t errn);

/* PackBits (TIFF / Photoshop RLE). */
size_t pcio_packbits_encode(const u8 *src, size_t n, u8 *dst);   /* dst >= n + n/128 + 2 */
/* Decodes into exactly outn bytes (zero-fills when the input ends early).
 * Returns the number of input bytes consumed. */
size_t pcio_packbits_decode(const u8 *src, size_t n, u8 *out, size_t outn);

/* Windows-1252 <-> UTF-16 (best effort, '?' for unrepresentable). */
void pcio_utf16_to_ansi(const u16 *units, size_t n, char *out, size_t outn);
size_t pcio_ansi_to_utf16(const char *s, u16 *out, size_t outmax);

/* ---------------- stb glue (stb_impl.c) ---------------- */

/* Decodes PNG/JPEG/GIF(first frame)/TGA into RGBA (free with free()).
 * *file_comp receives the channel count stored in the file. */
u8 *pcio_stbi_load(const u8 *data, size_t n, int *w, int *h, int *file_comp, char *err, size_t errn);
int pcio_stbi_info(const u8 *data, size_t n, int *w, int *h, int *comp);
/* zlib inflate into a buffer of exactly outn bytes; returns bytes written or -1. */
long pcio_inflate(u8 *out, size_t outn, const u8 *in, size_t inn, int zlib_header);
int pcio_stbi_write_png(PcWr *w, int width, int height, int comp, const u8 *px);
/* zlib stream of n bytes (free with free()); NULL when out of memory. */
u8 *pcio_zlib_compress(const u8 *data, size_t n, size_t *outn);

#endif
