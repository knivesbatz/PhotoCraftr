/*
 * Targa: decoding by stb_image (colour-mapped, true-colour and gray,
 * raw and RLE), encoding by stb_image_write (24/32-bit or 8-bit gray,
 * optional RLE).  TGA has no signature: the dispatcher picks it by
 * extension.
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

int pc_tga_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcBuf *c;
    size_t np, i;
    int alpha, ch, rc;
    u8 *px, *p;
    PcWr w;
    memset(&w, 0, sizeof w);
    *out = NULL; *outn = 0;
    c = pcio_composite(d, err, errn);
    if (!c) return -1;
    np = (size_t)c->w * c->h;
    alpha = (!o || o->save_alpha) && pcio_has_alpha(c->px, np);
    if (!alpha) pcio_matte_white(c->px, np);
    /* gray with transparency is written as 32-bit: 16-bit gray+alpha Targas are rarely readable */
    ch = alpha ? 4 : (d->s.mode == PC_MODE_GRAY ? 1 : 3);
    px = (u8 *)pc_alloc_checked(np, (size_t)ch, 1, 0);
    if (!px) { pc_buf_unref(c); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    for (i = 0, p = px; i < np; i++) {
        memcpy(p, c->px + i * 4, (size_t)ch);
        p += ch;
    }
    rc = pcio_stbi_write_tga(&w, c->w, c->h, ch, px, o ? o->tga_rle : 0);
    free(px);
    pc_buf_unref(c);
    if (rc) { wr_free(&w); pcio_seterr(err, errn, "The Targa file could not be written."); return -1; }
    return wr_finish(&w, out, outn, err, errn);
}
