/*
 * Targa: decoding by stb_image (colour-mapped, true-colour and gray,
 * raw and RLE).  Encoding is our own, at the 1998 depths only:
 *
 *   8   colour-mapped (24-bit colour map), or 8-bit gray (type 3) for
 *       Grayscale documents
 *   16  A1R5G5B5 true colour (the attribute bit is the alpha when
 *       transparency is kept)
 *   24  BGR
 *   32  BGRA with 8 alpha bits
 *
 * Rows are stored bottom-up (origin lower left, as Photoshop 5 and most
 * game tools of the time expect); RLE packets never cross a scanline.
 * TGA has no signature: the dispatcher picks it by extension.
 */
#include "io/ioint.h"

PcDoc *pc_tga_load(const u8 *data, size_t n, char *err, size_t errn)
{
    int w, h, comp;
    u8 *px;
    if (n < 18 || !pcio_stbi_info(data, n, &w, &h, &comp)) { pcio_seterr(err, errn, "This is not a Targa file, or it is damaged."); return NULL; }
    if (!pcio_dims_ok(w, h)) { pcio_seterr(err, errn, "The image is too large."); return NULL; }
    /* RLE packets hold at most 128 pixels */
    if (!pcio_plausible(w, h, n, 128)) { pcio_seterr(err, errn, "The Targa file is truncated or damaged."); return NULL; }
    px = pcio_stbi_load(data, n, &w, &h, &comp, err, errn);
    if (!px) return NULL;
    return pcio_doc_from_buf(pcio_buf_wrap(px, w, h), comp == 1 ? PC_MODE_GRAY : PC_MODE_RGB, 72, PC_FMT_TGA, err, errn);
}

/* One scanline of bpp-byte pixels, raw or RLE. */
static void tga_row(PcWr *w, const u8 *row, int width, int bpp, int rle)
{
    int x = 0;
    if (!rle) { wr_bytes(w, row, (size_t)width * bpp); return; }
    while (x < width) {
        int run = 1;
        while (x + run < width && run < 128 && !memcmp(row + (size_t)(x + run) * bpp, row + (size_t)x * bpp, (size_t)bpp)) run++;
        if (run > 1) {
            wr_u8(w, 0x80u | (unsigned)(run - 1));
            wr_bytes(w, row + (size_t)x * bpp, (size_t)bpp);
            x += run;
        } else {
            /* literal packet up to the next run of two or more */
            int lit = 1;
            while (x + lit < width && lit < 128 &&
                   !(x + lit + 1 < width && !memcmp(row + (size_t)(x + lit) * bpp, row + (size_t)(x + lit + 1) * bpp, (size_t)bpp)))
                lit++;
            wr_u8(w, (unsigned)(lit - 1));
            wr_bytes(w, row + (size_t)x * bpp, (size_t)lit * bpp);
            x += lit;
        }
    }
}

int pc_tga_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcFlat f;
    int W = d->s.w, H = d->s.h, rle = o ? o->tga_rle : 0, bpp, cmap, type, x, y, i;
    u8 *row;
    PcWr w;
    memset(&w, 0, sizeof w);
    *out = NULL; *outn = 0;
    if (W > 65535 || H > 65535) { pcio_seterr(err, errn, "The image is too large for Targa."); return -1; }
    if (pcio_flat(d, PC_FMT_TGA, o, &f, err, errn)) return -1;
    cmap = f.depth == 8 && !f.gray;
    bpp = f.depth / 8;
    type = cmap ? 1 : f.gray ? 3 : 2;
    row = (u8 *)malloc((size_t)W * 4);
    if (!row) { pcio_flat_free(&f); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    wr_u8(&w, 0);                                   /* no image ID */
    wr_u8(&w, cmap ? 1u : 0u);
    wr_u8(&w, (unsigned)(type + (rle ? 8 : 0)));
    wr_u16le(&w, 0);                                /* first colour map entry */
    wr_u16le(&w, cmap ? (unsigned)f.ix.ncolors : 0u);
    wr_u8(&w, cmap ? 24u : 0u);
    wr_u16le(&w, 0); wr_u16le(&w, 0);               /* origin */
    wr_u16le(&w, (unsigned)W); wr_u16le(&w, (unsigned)H);
    wr_u8(&w, (unsigned)f.depth);
    wr_u8(&w, f.depth == 32 ? 8u : (f.depth == 16 && f.alpha) ? 1u : 0u);   /* alpha bits; origin lower left */
    if (cmap)
        for (i = 0; i < f.ix.ncolors; i++) { wr_u8(&w, f.ix.pal[i * 3 + 2]); wr_u8(&w, f.ix.pal[i * 3 + 1]); wr_u8(&w, f.ix.pal[i * 3]); }
    for (y = H - 1; y >= 0; y--) {
        const u8 *s = f.c->px + (size_t)y * W * 4;
        u8 *q = row;
        for (x = 0; x < W; x++, s += 4) {
            switch (f.depth) {
            case 8: *q++ = cmap ? f.ix.idx[(size_t)y * W + x] : s[0]; break;
            case 16: {
                unsigned v = pc_hicolor_pack(s[0], s[1], s[2], 0) | ((!f.alpha || s[3] >= 128) ? 0x8000u : 0u);
                *q++ = (u8)v; *q++ = (u8)(v >> 8);
                break;
            }
            default:
                *q++ = s[2]; *q++ = s[1]; *q++ = s[0];
                if (f.depth == 32) *q++ = s[3];
            }
        }
        tga_row(&w, row, W, bpp, rle);
    }
    free(row);
    pcio_flat_free(&f);
    return wr_finish(&w, out, outn, err, errn);
}
