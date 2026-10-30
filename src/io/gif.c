/*
 * CompuServe GIF.  Loading goes through stb_image (first frame of an
 * animation, transparency kept).  Saving is our own GIF89a encoder:
 *
 *  - colours go through the shared quantizer (src/core/quantize.c) with
 *    the palette and dither chosen in the dialog; images with few enough
 *    colours keep them exactly;
 *  - fully or mostly transparent pixels (alpha < 128) become the
 *    transparent index when transparency is on (holding the colour key
 *    when that is on too); partial alpha is matted against white like
 *    Photoshop's GIF89a Export;
 *  - Grayscale documents without transparency get a 256-level gray
 *    palette;
 *  - LZW with variable code width (GIF flavour: no early change), clear
 *    code when the 4096-entry table fills, optional interlacing.
 */
#include "io/ioint.h"

PcDoc *pc_gif_load(const u8 *data, size_t n, char *err, size_t errn)
{
    int w, h, comp;
    u8 *px;
    if (n < 13 || memcmp(data, "GIF8", 4)) { pcio_seterr(err, errn, "This is not a GIF file."); return NULL; }
    if (!pcio_stbi_info(data, n, &w, &h, &comp)) { pcio_seterr(err, errn, "The GIF file is damaged or unsupported."); return NULL; }
    if (!pcio_dims_ok(w, h)) { pcio_seterr(err, errn, "The image is too large."); return NULL; }
    /* an LZW code (>= 3 bits) expands to at most 4096 pixels */
    if (!pcio_plausible(w, h, n, 8192)) { pcio_seterr(err, errn, "The GIF file is truncated or damaged."); return NULL; }
    px = pcio_stbi_load(data, n, &w, &h, &comp, err, errn);
    if (!px) return NULL;
    return pcio_doc_from_buf(pcio_buf_wrap(px, w, h), PC_MODE_RGB, 72, PC_FMT_GIF, err, errn);
}

/* ---------------- LZW (GIF flavour) ---------------- */

typedef struct {
    PcWr *w;
    u8 block[256];
    int nblock;
    u32 acc;
    int nacc;
} GifBits;

static void gb_byte(GifBits *g, unsigned b)
{
    g->block[1 + g->nblock++] = (u8)b;
    if (g->nblock == 255) { g->block[0] = 255; wr_bytes(g->w, g->block, 256); g->nblock = 0; }
}

static void gb_code(GifBits *g, unsigned code, int nbits)
{
    g->acc |= (u32)code << g->nacc;
    g->nacc += nbits;
    while (g->nacc >= 8) { gb_byte(g, g->acc & 255); g->acc >>= 8; g->nacc -= 8; }
}

static void gb_flush(GifBits *g)
{
    if (g->nacc > 0) gb_byte(g, g->acc & 255);
    g->acc = 0; g->nacc = 0;
    if (g->nblock) { g->block[0] = (u8)g->nblock; wr_bytes(g->w, g->block, (size_t)g->nblock + 1); g->nblock = 0; }
    wr_u8(g->w, 0);                     /* block terminator */
}

enum { HSIZE = 8192 };

static int lzw_gif(PcWr *w, const u8 *idx, const int *order, int W, int H, int mincode)
{
    u32 *keys = (u32 *)malloc(sizeof(u32) * HSIZE);
    u16 *vals = (u16 *)malloc(sizeof(u16) * HSIZE);
    GifBits g;
    int clear = 1 << mincode, eoi = clear + 1, next, nbits, ent = -1, y, x;
    if (!keys || !vals) { free(keys); free(vals); return -1; }
    memset(&g, 0, sizeof g);
    g.w = w;
    wr_u8(w, (unsigned)mincode);
    memset(keys, 0, sizeof(u32) * HSIZE);
    next = clear + 2;
    nbits = mincode + 1;
    gb_code(&g, (unsigned)clear, nbits);
    for (y = 0; y < H; y++) {
        const u8 *row = idx + (size_t)order[y] * W;
        for (x = 0; x < W; x++) {
            int c = row[x];
            u32 key, hsh;
            if (ent < 0) { ent = c; continue; }
            key = ((u32)ent << 8 | (u32)c) + 1;
            hsh = (key * 2654435761u) >> 19;
            while (keys[hsh] && keys[hsh] != key) hsh = (hsh + 1) & (HSIZE - 1);
            if (keys[hsh]) { ent = vals[hsh]; continue; }
            gb_code(&g, (unsigned)ent, nbits);
            if (next < 4096) {
                keys[hsh] = key;
                vals[hsh] = (u16)next;
                next++;
                if (next > (1 << nbits) && nbits < 12) nbits++;
            }
            if (next >= 4096) {
                gb_code(&g, (unsigned)clear, nbits);
                memset(keys, 0, sizeof(u32) * HSIZE);
                next = clear + 2;
                nbits = mincode + 1;
            }
            ent = c;
        }
    }
    if (ent >= 0) {
        gb_code(&g, (unsigned)ent, nbits);
        /* the decoder adds an entry for this code before reading EOI */
        if (next < 4096) { next++; if (next > (1 << nbits) && nbits < 12) nbits++; }
    }
    gb_code(&g, (unsigned)eoi, nbits);
    gb_flush(&g);
    free(keys); free(vals);
    return w->err ? -1 : 0;
}

/* ---------------- encoder ---------------- */

int pc_gif_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcFlat f;
    int W = d->s.w, H = d->s.h, inter = o ? o->interlace : 0;
    size_t np = (size_t)W * H, i;
    u8 *idx = NULL, *gidx = NULL, pal[768];
    int *order = NULL, ncol, tindex = -1, palbits, y, rc = -1;
    PcWr w;
    memset(&w, 0, sizeof w);
    memset(pal, 0, sizeof pal);
    *out = NULL; *outn = 0;
    if (W > 65535 || H > 65535) { pcio_seterr(err, errn, "The image is too large for GIF."); return -1; }
    if (pcio_flat(d, PC_FMT_GIF, o, &f, err, errn)) return -1;
    order = (int *)malloc(sizeof(int) * (size_t)H);
    if (!order) goto oom;
    if (f.gray) {
        gidx = (u8 *)pc_alloc_checked(np, 1, 1, 0);
        if (!gidx) goto oom;
        for (i = 0; i < 256; i++) pal[i * 3] = pal[i * 3 + 1] = pal[i * 3 + 2] = (u8)i;
        for (i = 0; i < np; i++) gidx[i] = f.c->px[i * 4];
        idx = gidx;
        ncol = 256;
    } else {
        idx = f.ix.idx;
        ncol = f.ix.ncolors;
        memcpy(pal, f.ix.pal, (size_t)ncol * 3);
        if (!o || o->q.transparency) tindex = f.ix.transparent;
    }
    for (palbits = 1; (1 << palbits) < ncol; palbits++) {}

    /* row order (interlaced: rows 0,8,..; 4,12,..; 2,6,..; 1,3,..) */
    if (inter) {
        static const int start[4] = { 0, 4, 2, 1 }, step[4] = { 8, 8, 4, 2 };
        int p, k = 0;
        for (p = 0; p < 4; p++) for (y = start[p]; y < H; y += step[p]) order[k++] = y;
    } else {
        for (y = 0; y < H; y++) order[y] = y;
    }

    wr_bytes(&w, "GIF89a", 6);
    wr_u16le(&w, (unsigned)W);
    wr_u16le(&w, (unsigned)H);
    wr_u8(&w, 0x80 | 0x70 | (unsigned)(palbits - 1));
    wr_u8(&w, 0);                              /* background index */
    wr_u8(&w, 0);                              /* aspect */
    wr_bytes(&w, pal, (size_t)3 << palbits);
    if (tindex >= 0) {
        u8 gce[8] = { 0x21, 0xF9, 4, 1, 0, 0, 0, 0 };
        gce[6] = (u8)tindex;
        wr_bytes(&w, gce, 8);
    }
    wr_u8(&w, 0x2C);
    wr_u16le(&w, 0); wr_u16le(&w, 0);
    wr_u16le(&w, (unsigned)W); wr_u16le(&w, (unsigned)H);
    wr_u8(&w, inter ? 0x40 : 0);
    if (lzw_gif(&w, idx, order, W, H, palbits < 2 ? 2 : palbits)) goto oom;
    wr_u8(&w, 0x3B);
    rc = 0;
oom:
    free(gidx); free(order);
    pcio_flat_free(&f);
    if (rc) { wr_free(&w); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    return wr_finish(&w, out, outn, err, errn);
}
