/*
 * Quantizer tests (src/core/quantize.c): palette sizes and layout, Exact
 * keeps colours, every dither stays in the palette, high-colour rounding,
 * the colour key, full-alpha PNG-8 quantization and determinism.
 */
#include "testutil.h"
#include <stdlib.h>
#include <string.h>
#include "core/pcquant.h"

static u8 *mk_rgba(int w, int h, int kind)
{
    u8 *p = (u8 *)malloc((size_t)w * h * 4);
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            u8 *q = p + ((size_t)y * w + x) * 4;
            q[0] = (u8)(x * 255 / (w - 1)); q[1] = (u8)(y * 255 / (h - 1)); q[2] = (u8)((x * 7 + y * 13) & 255); q[3] = 255;
            if (kind == 1) { q[0] = (u8)(q[0] / 64 * 64); q[1] = (u8)(q[1] / 64 * 64); q[2] = (u8)(q[2] / 128 * 128); }   /* few colours */
            if (kind == 2 && (x + y) % 3 == 0) q[3] = 0;                                                               /* binary alpha */
            if (kind == 3) q[3] = (u8)((x * 255) / (w - 1));                                                           /* alpha ramp */
        }
    return p;
}

static int pal_has(const u8 *pal, int n, int r, int g, int b)
{
    int i;
    for (i = 0; i < n; i++) if (pal[i * 3] == r && pal[i * 3 + 1] == g && pal[i * 3 + 2] == b) return i;
    return -1;
}

static int unique_entries(const u8 *pal, int n)
{
    int i, j;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) if (!memcmp(pal + i * 3, pal + j * 3, 3)) return 0;
    return 1;
}

static void test_palettes(void)
{
    u8 pal[768];
    CHECK_EQ(pc_palette_builtin(PC_PAL_WINDOWS, 256, pal), 256);
    CHECK(unique_entries(pal, 256));
    CHECK_EQ(pal_has(pal, 256, 0, 0, 0), 0);                 /* statics at both ends */
    CHECK_EQ(pal_has(pal, 256, 0x80, 0, 0), 1);
    CHECK_EQ(pal_has(pal, 256, 0xC0, 0xC0, 0xC0), 7);
    CHECK_EQ(pal_has(pal, 256, 0xFF, 0x00, 0xFF), 253);
    CHECK_EQ(pal_has(pal, 256, 255, 255, 255), 255);
    CHECK(pal_has(pal, 256, 0x33, 0x66, 0x99) >= 10);        /* the cube in between */
    CHECK_EQ(pc_palette_builtin(PC_PAL_MACOS, 256, pal), 256);
    CHECK(unique_entries(pal, 256));
    CHECK_EQ(pal_has(pal, 256, 255, 255, 255), 0);
    CHECK_EQ(pal_has(pal, 256, 0, 0, 0), 255);
    CHECK_EQ(pal_has(pal, 256, 0xEE, 0, 0), 215);            /* ramps after the 215 cube colours */
    CHECK_EQ(pal_has(pal, 256, 0x11, 0x11, 0x11), 254);
    CHECK_EQ(pc_palette_builtin(PC_PAL_WEB, 256, pal), 216);
    CHECK(unique_entries(pal, 216));
    CHECK(pal_has(pal, 216, 0x33, 0xCC, 0xFF) >= 0);
    CHECK_EQ(pc_palette_builtin(PC_PAL_UNIFORM, 256, pal), 252);   /* 6 x 7 x 6 */
    CHECK(unique_entries(pal, 252));
    CHECK_EQ(pc_palette_builtin(PC_PAL_UNIFORM, 216, pal), 216);
    CHECK_EQ(pc_palette_builtin(PC_PAL_UNIFORM, 8, pal), 8);
    CHECK_EQ(pc_palette_builtin(PC_PAL_UNIFORM, 2, pal), 2);
    CHECK_EQ(pal_has(pal, 2, 0, 0, 0), 0); CHECK_EQ(pal_has(pal, 2, 255, 255, 255), 1);
    CHECK(pc_palette_builtin(PC_PAL_UNIFORM, 100, pal) <= 100);
    CHECK_EQ(pc_palette_builtin(PC_PAL_ADAPTIVE, 256, pal), 0);
}

static void test_exact_and_dither(void)
{
    const int W = 61, H = 37;
    u8 *few = mk_rgba(W, H, 1), *many = mk_rgba(W, H, 0);
    PcQuantOpts q;
    PcIndexed ix;
    int pk, dk, cols[] = { 2, 16, 256 }, ci;
    size_t i;
    /* Exact (and Adaptive with few colours) keep every colour */
    for (pk = 0; pk < 2; pk++) {
        pc_quant_defaults(&q);
        q.palette = pk ? PC_PAL_ADAPTIVE : PC_PAL_EXACT;
        q.dither = PC_DITHER_DIFFUSION;
        CHECK_EQ(pc_quantize(few, W, H, &q, NULL, 0, -1, &ix), 0);
        CHECK_EQ(ix.ncolors, pc_count_colors(few, (size_t)W * H, 256));
        CHECK_EQ(ix.transparent, -1);
        for (i = 0; i < (size_t)W * H; i++) CHECK_EQ(memcmp(ix.pal + ix.idx[i] * 3, few + i * 4, 3), 0);
        pc_indexed_free(&ix);
    }
    /* every palette x dither x colour count stays inside the palette */
    for (pk = PC_PAL_EXACT; pk < PC_PAL_COUNT; pk++)
        for (dk = 0; dk < PC_DITHER_COUNT; dk++)
            for (ci = 0; ci < 3; ci++) {
                u8 fixed[768];
                int nf = pc_palette_builtin(pk, cols[ci], fixed), bad = 0;
                pc_quant_defaults(&q);
                q.palette = pk; q.dither = dk; q.colors = cols[ci];
                CHECK_EQ(pc_quantize(many, W, H, &q, pk == PC_PAL_DOCUMENT ? fixed : NULL, 0, -1, &ix), 0);
                if (!ix.idx) continue;
                CHECK(ix.ncolors >= 1 && ix.ncolors <= 256);
                if (pk == PC_PAL_ADAPTIVE || pk == PC_PAL_UNIFORM) CHECK(ix.ncolors <= cols[ci]);
                for (i = 0; i < (size_t)W * H; i++) if (ix.idx[i] >= ix.ncolors) bad++;
                if (nf) for (i = 0; i < (size_t)ix.ncolors; i++) if (pal_has(fixed, nf, ix.pal[i * 3], ix.pal[i * 3 + 1], ix.pal[i * 3 + 2]) < 0) bad++;
                CHECK_EQ(bad, 0);
                pc_indexed_free(&ix);
            }
    /* Document palette: given entries, no others; colours in it are kept */
    {
        u8 dp[6 * 3] = { 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 128, 128, 128 };
        pc_quant_defaults(&q);
        q.palette = PC_PAL_DOCUMENT;
        q.dither = PC_DITHER_PATTERN;
        CHECK_EQ(pc_quantize(many, W, H, &q, dp, 6, -1, &ix), 0);
        CHECK_EQ(ix.ncolors, 6);
        CHECK(!memcmp(ix.pal, dp, sizeof dp));
        pc_indexed_free(&ix);
    }
    /* deterministic */
    {
        PcIndexed a, b;
        pc_quant_defaults(&q);
        q.colors = 64;
        pc_quantize(many, W, H, &q, NULL, 0, -1, &a);
        pc_quantize(many, W, H, &q, NULL, 0, -1, &b);
        CHECK_EQ(a.ncolors, b.ncolors);
        CHECK(!memcmp(a.pal, b.pal, 768));
        CHECK(!memcmp(a.idx, b.idx, (size_t)W * H));
        pc_indexed_free(&a); pc_indexed_free(&b);
    }
    free(few); free(many);
}

static void test_hicolor(void)
{
    int v, qv, bad = 0;
    for (v = 0; v < 256; v++) {
        /* nearest 5- and 6-bit level */
        int q5 = pc_q5(v), q6 = pc_q6(v), k, best5 = 0, best6 = 0;
        for (k = 0; k < 32; k++) if (abs(pc_x5(k) - v) < abs(pc_x5(best5) - v)) best5 = k;
        for (k = 0; k < 64; k++) if (abs(pc_x6(k) - v) < abs(pc_x6(best6) - v)) best6 = k;
        if (abs(pc_x5(q5) - v) != abs(pc_x5(best5) - v)) bad++;
        if (abs(pc_x6(q6) - v) != abs(pc_x6(best6) - v)) bad++;
        if (abs(pc_x5(q5) - v) > 4 || abs(pc_x6(q6) - v) > 2) bad++;
    }
    for (qv = 0; qv < 32; qv++) if (pc_q5(pc_x5(qv)) != qv) bad++;     /* (x5 rounds like our BMP reader) */
    for (qv = 0; qv < 64; qv++) if (pc_q6(pc_x6(qv)) != qv) bad++;
    CHECK_EQ(bad, 0);
    CHECK_EQ(pc_q5(0), 0); CHECK_EQ(pc_q5(255), 31); CHECK_EQ(pc_q5(4), 0); CHECK_EQ(pc_q5(5), 1);
    CHECK_EQ(pc_hicolor_pack(255, 0, 255, 0), 0x7C1F);
    CHECK_EQ(pc_hicolor_pack(255, 0, 255, 1), 0xF81F);
    CHECK_EQ(pc_hicolor_pack(255, 255, 255, 0), 0x7FFF);
    CHECK_EQ(pc_hicolor_pack(255, 255, 255, 1), 0xFFFF);
    CHECK_EQ(pc_hicolor_pack(0, 255, 0, 1), 0x07E0);
    {
        const int W = 40, H = 30;
        int dk, g6;
        for (dk = 0; dk < PC_DITHER_COUNT; dk++)
            for (g6 = 0; g6 < 2; g6++) {
                u8 *p = mk_rgba(W, H, 0);
                size_t i;
                int off = 0;
                pc_hicolor_reduce(p, W, H, g6, dk);
                for (i = 0; i < (size_t)W * H; i++) {
                    if (pc_x5(pc_q5(p[i * 4])) != p[i * 4] || pc_x5(pc_q5(p[i * 4 + 2])) != p[i * 4 + 2]) off++;
                    if ((g6 ? pc_x6(pc_q6(p[i * 4 + 1])) : pc_x5(pc_q5(p[i * 4 + 1]))) != p[i * 4 + 1]) off++;
                    if (p[i * 4 + 3] != 255) off++;
                }
                CHECK_EQ(off, 0);
                free(p);
            }
    }
}

static void test_colorkey(void)
{
    const int W = 50, H = 30;
    u8 *img = mk_rgba(W, H, 2);
    PcQuantOpts q;
    PcIndexed ix;
    int pk;
    size_t i;
    for (pk = PC_PAL_EXACT; pk < PC_PAL_DOCUMENT; pk++) {
        int bad = 0;
        pc_quant_defaults(&q);
        q.palette = pk;
        q.colorkey = 1;
        q.transparency = 0;
        q.dither = PC_DITHER_DIFFUSION;
        CHECK_EQ(pc_quantize(img, W, H, &q, NULL, 0, -1, &ix), 0);
        CHECK(ix.transparent >= 0);
        if (ix.transparent < 0) { pc_indexed_free(&ix); continue; }
        CHECK_EQ(pc_rgb(ix.pal[ix.transparent * 3], ix.pal[ix.transparent * 3 + 1], ix.pal[ix.transparent * 3 + 2]), PC_COLORKEY_DEFAULT);
        CHECK_EQ(ix.alpha[ix.transparent], 255);           /* a key is an opaque colour */
        for (i = 0; i < (size_t)W * H; i++) {
            int t = img[i * 4 + 3] < 128;
            if (t != (ix.idx[i] == ix.transparent)) bad++;  /* reserved: no opaque pixel lands on it */
        }
        CHECK_EQ(bad, 0);
        if (pk == PC_PAL_WINDOWS) CHECK_EQ(ix.transparent, 253);   /* the static magenta is reused */
        if (pk == PC_PAL_WEB) CHECK_EQ(ix.ncolors, 216);
        pc_indexed_free(&ix);
    }
    /* transparency without the key: a transparent entry, alpha 0 */
    pc_quant_defaults(&q);
    CHECK_EQ(pc_quantize(img, W, H, &q, NULL, 0, -1, &ix), 0);
    CHECK(ix.transparent >= 0);
    if (ix.transparent >= 0) CHECK_EQ(ix.alpha[ix.transparent], 0);
    pc_indexed_free(&ix);
    /* other key colours work too */
    pc_quant_defaults(&q);
    q.colorkey = 1; q.key_rgb = 0x00FF00;
    q.palette = PC_PAL_MACOS;
    CHECK_EQ(pc_quantize(img, W, H, &q, NULL, 0, -1, &ix), 0);
    if (ix.transparent >= 0) CHECK_EQ(pc_rgb(ix.pal[ix.transparent * 3], ix.pal[ix.transparent * 3 + 1], ix.pal[ix.transparent * 3 + 2]), 0x00FF00);
    pc_indexed_free(&ix);
    free(img);
}

static void test_full_alpha(void)
{
    const int W = 64, H = 20;
    u8 *img = mk_rgba(W, H, 3);
    PcQuantOpts q;
    PcIndexed ix;
    size_t i;
    int partial = 0, maxd = 0, k;
    for (i = 0; i < (size_t)W * H; i++) if (!(i % W)) img[i * 4 + 3] = 0;   /* a fully transparent column */
    pc_quant_defaults(&q);
    q.full_alpha = 1;
    q.dither = PC_DITHER_NONE;
    CHECK_EQ(pc_quantize(img, W, H, &q, NULL, 0, -1, &ix), 0);
    CHECK(ix.ncolors <= 256);
    CHECK(ix.transparent >= 0);
    for (k = 0; k < ix.ncolors; k++) if (ix.alpha[k] != 0 && ix.alpha[k] != 255) partial++;
    CHECK(partial > 4);
    for (k = 1; k < ix.ncolors; k++) CHECK(ix.alpha[k] >= ix.alpha[k - 1]);      /* translucent entries first */
    for (i = 0; i < (size_t)W * H; i++) {
        int a = img[i * 4 + 3], e = ix.idx[i];
        if (!a) { CHECK_EQ(e, ix.transparent); continue; }
        maxd = PC_MAX(maxd, abs(ix.alpha[e] - a));
    }
    CHECK(maxd <= 24);
    pc_indexed_free(&ix);
    /* binary path when full alpha is off: alpha only 0 or 255 */
    q.full_alpha = 0;
    CHECK_EQ(pc_quantize(img, W, H, &q, NULL, 0, -1, &ix), 0);
    for (k = 0; k < ix.ncolors; k++) CHECK(ix.alpha[k] == 0 || ix.alpha[k] == 255);
    pc_indexed_free(&ix);
    free(img);
}

static void test_indexed_mode(void)
{
    PcDoc *d = pc_doc_new(40, 30, PC_MODE_RGB, 72, 0, 0);
    u8 *p = pc_layer_pixels_w(d->s.layers[0]), *img = mk_rgba(40, 30, 0);
    PcQuantOpts q;
    PcLayer *l;
    memcpy(p, img, 40 * 30 * 4);
    pc_doc_add_layer(d, "Top");
    l = pc_doc_active(d);
    p = pc_layer_pixels_w(l);
    memset(p, 0, 40 * 30 * 4);
    p[0] = 10; p[3] = 255;
    pc_quant_defaults(&q);
    q.palette = PC_PAL_WEB;
    q.dither = PC_DITHER_PATTERN;
    CHECK_EQ(pc_doc_to_indexed(d, &q), 0);
    CHECK_EQ(d->s.nlayers, 1);                                /* flattened */
    CHECK_EQ(d->s.layers[0]->is_background, 1);
    CHECK_EQ(d->index_ncolors, 216);
    {
        size_t i;
        int bad = 0;
        const u8 *px = d->s.layers[0]->pix->px;
        for (i = 0; i < 40 * 30; i++) if (pal_has(d->index_pal, d->index_ncolors, px[i * 4], px[i * 4 + 1], px[i * 4 + 2]) < 0) bad++;
        CHECK_EQ(bad, 0);
    }
    pc_doc_free(d);
    free(img);
}

int test_quant(void)
{
    test_palettes();
    test_exact_and_dither();
    test_hicolor();
    test_colorkey();
    test_full_alpha();
    test_indexed_mode();
    return 0;
}
