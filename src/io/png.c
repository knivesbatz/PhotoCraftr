/*
 * PNG: decoding by stb_image (8/16-bit, palette, gray, alpha; 16-bit is
 * reduced to 8).  Encoding at the 1998 depths only: 8-bit palette (our
 * own PLTE/tRNS writer, zlib from stb_image_write) or 8-bit gray, 24-bit
 * RGB and 32-bit RGBA (gray + alpha for Grayscale documents) through
 * stb_image_write - never 16 bits per channel.  The resolution travels in
 * a pHYs chunk, which we parse and insert ourselves.
 */
#include "io/ioint.h"

static const u8 png_sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

static double png_dpi(const u8 *data, size_t n)
{
    PcRd r;
    rd_init(&r, data, n);
    rd_skip(&r, 8);
    while (rd_left(&r) >= 12) {
        u32 len = rd_u32be(&r);
        const u8 *type = rd_bytes(&r, 4);
        const u8 *body = rd_bytes(&r, len);
        rd_skip(&r, 4);                       /* CRC */
        if (!type || !body) break;
        if (!memcmp(type, "pHYs", 4) && len == 9 && body[8] == 1)
            return pcio_dpi_from_ppm((double)get_u32be(body));
        if (!memcmp(type, "IDAT", 4) || !memcmp(type, "IEND", 4)) break;
    }
    return 72;
}

PcDoc *pc_png_load(const u8 *data, size_t n, char *err, size_t errn)
{
    int w, h, comp;
    u8 *px;
    if (n < 8 || memcmp(data, png_sig, 8)) { pcio_seterr(err, errn, "This is not a PNG file."); return NULL; }
    if (!pcio_stbi_info(data, n, &w, &h, &comp)) { pcio_seterr(err, errn, "The PNG file is damaged or unsupported."); return NULL; }
    if (!pcio_dims_ok(w, h)) { pcio_seterr(err, errn, "The image is too large."); return NULL; }
    /* 1-bit rows through deflate (~1032:1) give at most ~8300 pixels per byte */
    if (!pcio_plausible(w, h, n, 8400)) { pcio_seterr(err, errn, "The PNG file is truncated or damaged."); return NULL; }
    px = pcio_stbi_load(data, n, &w, &h, &comp, err, errn);
    if (!px) return NULL;
    return pcio_doc_from_buf(pcio_buf_wrap(px, w, h), comp <= 2 ? PC_MODE_GRAY : PC_MODE_RGB, png_dpi(data, n), PC_FMT_PNG, err, errn);
}

/* Pack the composite into ch (1..4) interleaved channels. */
static u8 *pack(const u8 *rgba, size_t np, int ch)
{
    u8 *o = (u8 *)pc_alloc_checked(np, (size_t)ch, 1, 0), *q = o;
    size_t i;
    if (!o) return NULL;
    for (i = 0; i < np; i++, rgba += 4) {
        switch (ch) {
        case 1: *q++ = rgba[0]; break;
        case 2: *q++ = rgba[0]; *q++ = rgba[3]; break;
        case 3: *q++ = rgba[0]; *q++ = rgba[1]; *q++ = rgba[2]; break;
        default: memcpy(q, rgba, 4); q += 4;
        }
    }
    return o;
}

static void png_chunk(PcWr *w, const char *type, const u8 *body, size_t len)
{
    size_t at;
    wr_u32be(w, (u32)len);
    at = w->n;
    wr_bytes(w, type, 4);
    if (len) wr_bytes(w, body, len);
    wr_u32be(w, w->err ? 0 : pcio_crc32(0, w->p + at, len + 4));
}

/* 8-bit colour-indexed PNG: IHDR, PLTE, tRNS (when any palette entry is
 * not opaque: one transparent entry, or partial alpha per entry - the
 * modern PNG-8 exception), one IDAT of unfiltered rows, IEND. */
static int png_write_indexed(PcWr *w, const PcIndexed *ix)
{
    u8 ihdr[13], *raw, *z;
    size_t rb = (size_t)ix->w + 1, zn, y;
    int i, ntrns = 0;
    raw = (u8 *)pc_alloc_checked(rb, (size_t)ix->h, 1, 0);
    if (!raw) return -1;
    for (y = 0; y < (size_t)ix->h; y++) {
        raw[y * rb] = 0;                                  /* filter: none (the usual choice for palettes) */
        memcpy(raw + y * rb + 1, ix->idx + y * ix->w, (size_t)ix->w);
    }
    z = pcio_zlib_compress(raw, rb * ix->h, &zn);
    free(raw);
    if (!z) return -1;
    wr_bytes(w, png_sig, 8);
    ihdr[0] = (u8)(ix->w >> 24); ihdr[1] = (u8)(ix->w >> 16); ihdr[2] = (u8)(ix->w >> 8); ihdr[3] = (u8)ix->w;
    ihdr[4] = (u8)(ix->h >> 24); ihdr[5] = (u8)(ix->h >> 16); ihdr[6] = (u8)(ix->h >> 8); ihdr[7] = (u8)ix->h;
    ihdr[8] = 8; ihdr[9] = 3; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    png_chunk(w, "IHDR", ihdr, 13);
    png_chunk(w, "PLTE", ix->pal, (size_t)ix->ncolors * 3);
    for (i = 0; i < ix->ncolors; i++) if (ix->alpha[i] != 255) ntrns = i + 1;
    if (ntrns) png_chunk(w, "tRNS", ix->alpha, (size_t)ntrns);
    png_chunk(w, "IDAT", z, zn);
    png_chunk(w, "IEND", NULL, 0);
    free(z);
    return w->err ? -1 : 0;
}

int pc_png_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcFlat f;
    size_t np;
    int ch, rc;
    u8 *px;
    PcWr w;
    memset(&w, 0, sizeof w);
    *out = NULL; *outn = 0;
    if (pcio_flat(d, PC_FMT_PNG, o, &f, err, errn)) return -1;
    np = (size_t)f.c->w * f.c->h;
    if (f.ix.idx) {
        rc = png_write_indexed(&w, &f.ix);
    } else {
        /* 8: gray; 24: RGB; 32: RGBA (gray + alpha for Grayscale documents) */
        int gray = d->s.mode == PC_MODE_GRAY;
        ch = f.depth == 8 ? 1 : f.depth == 24 ? 3 : gray ? 2 : 4;
        px = pack(f.c->px, np, ch);
        if (!px) { pcio_flat_free(&f); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
        rc = pcio_stbi_write_png(&w, f.c->w, f.c->h, ch, px);
        free(px);
    }
    pcio_flat_free(&f);
    if (rc || w.n < 33 || memcmp(w.p + 12, "IHDR", 4)) {
        wr_free(&w);
        pcio_seterr(err, errn, "The PNG file could not be written.");
        return -1;
    }
    /* insert pHYs right after IHDR (8-byte signature + 25-byte IHDR chunk) */
    {
        u8 chunk[21];
        u32 ppm = pcio_uround(pcio_doc_dpi(d) / 0.0254), crc;
        chunk[0] = 0; chunk[1] = 0; chunk[2] = 0; chunk[3] = 9;
        memcpy(chunk + 4, "pHYs", 4);
        chunk[8] = (u8)(ppm >> 24); chunk[9] = (u8)(ppm >> 16); chunk[10] = (u8)(ppm >> 8); chunk[11] = (u8)ppm;
        memcpy(chunk + 12, chunk + 8, 4);
        chunk[16] = 1;                                  /* unit: metre */
        crc = pcio_crc32(0, chunk + 4, 13);
        chunk[17] = (u8)(crc >> 24); chunk[18] = (u8)(crc >> 16); chunk[19] = (u8)(crc >> 8); chunk[20] = (u8)crc;
        if (wr_reserve(&w, sizeof chunk) == 0) {
            memmove(w.p + 33 + sizeof chunk, w.p + 33, w.n - 33);
            memcpy(w.p + 33, chunk, sizeof chunk);
            w.n += sizeof chunk;
        }
    }
    return wr_finish(&w, out, outn, err, errn);
}
