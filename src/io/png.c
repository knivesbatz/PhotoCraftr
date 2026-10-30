/*
 * PNG: decoding by stb_image (8/16-bit, palette, gray, alpha; 16-bit is
 * reduced to 8), encoding by stb_image_write.  The resolution travels in
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

int pc_png_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcBuf *c;
    size_t np;
    int alpha, ch, rc;
    u8 *px;
    PcWr w;
    memset(&w, 0, sizeof w);
    *out = NULL; *outn = 0;
    c = pcio_composite(d, err, errn);
    if (!c) return -1;
    np = (size_t)c->w * c->h;
    alpha = (!o || o->save_alpha) && pcio_has_alpha(c->px, np);
    if (!alpha) pcio_matte_white(c->px, np);
    ch = d->s.mode == PC_MODE_GRAY ? (alpha ? 2 : 1) : (alpha ? 4 : 3);
    px = pack(c->px, np, ch);
    if (!px) { pc_buf_unref(c); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    rc = pcio_stbi_write_png(&w, c->w, c->h, ch, px);
    free(px);
    pc_buf_unref(c);
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
