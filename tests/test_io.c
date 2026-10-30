/*
 * Codec tests: round trips for every format, PSD structure (layers, masks,
 * names, blend modes, alpha channels, resolution, adjustment layers, hand-built files with
 * out-of-canvas layers, groups, ZIP channels, 16-bit, CMYK, indexed),
 * detection, and fuzzing (truncations + random mutations) under ASan.
 *
 * An independent stb_image (PSD/BMP decoders) is compiled in statically to
 * cross-check our writers.  Set PC_TEST_DUMP=<dir> to write sample files
 * for external validation (psd-tools, Pillow).
 */
#include "testutil.h"
#include <stdlib.h>
#include <string.h>
#include "io/pcio.h"

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PSD
#define STBI_ONLY_BMP
#define STBI_ONLY_PNG
#define STBI_NO_THREAD_LOCALS
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb/stb_image_write.h"

/* ---------------- helpers ---------------- */

static void fuzz_one(const u8 *orig, size_t n, const char *name, int mutations);

static unsigned rng_state = 12345u;
static unsigned rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void dump(const char *name, const u8 *p, size_t n)
{
    const char *dir = getenv("PC_TEST_DUMP");
    char path[512];
    FILE *f;
    if (!dir || !*dir) return;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (!f) return;
    fwrite(p, 1, n, f);
    fclose(f);
}

/* alpha: 0 opaque (Background), 1 mixed alpha, 2 binary alpha (0/255);
 * palette: limit colours to a 6x6x6-ish cube (<= 216 colours) */
static PcDoc *mk_doc(int w, int h, int mode, int alpha, int palette)
{
    PcDoc *d = pc_doc_new(w, h, mode, 72, alpha ? 2 : 0, 0);
    u8 *p;
    int x, y;
    if (!d) return NULL;
    p = pc_layer_pixels_w(d->s.layers[0]);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++, p += 4) {
            int r = w > 1 ? x * 255 / (w - 1) : 128, g = h > 1 ? y * 255 / (h - 1) : 64, b = (x * 31 + y * 17) & 255;
            int a = 255;
            if (palette) { r = r / 51 * 51; g = g / 51 * 51; b = b / 51 * 51; }
            if (mode == PC_MODE_GRAY) r = g = b = pc_luma(r, g, b);
            if (alpha == 1) a = ((x + y) % 5 == 0) ? 0 : ((x * 37 + y * 11) % 3 == 0 ? 255 : (x * 13 + y * 7) & 255);
            if (alpha == 2) a = ((x + 2 * y) % 4 == 0) ? 0 : 255;
            p[0] = (u8)r; p[1] = (u8)g; p[2] = (u8)b; p[3] = (u8)a;
        }
    return d;
}

static PcDoc *mk_smooth(int w, int h, int mode)
{
    PcDoc *d = pc_doc_new(w, h, mode, 72, 0, 0);
    u8 *p = pc_layer_pixels_w(d->s.layers[0]);
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++, p += 4) {
            int r = x * 255 / (w - 1), g = y * 255 / (h - 1), b = (x + y) * 255 / (w + h - 2);
            if (mode == PC_MODE_GRAY) r = g = b = pc_luma(r, g, b);
            p[0] = (u8)r; p[1] = (u8)g; p[2] = (u8)b;
        }
    return d;
}

/* Compare composites; colours of fully transparent pixels are ignored.
 * Returns the maximum channel difference, *mean gets the mean. */
static int cmp_comp(const PcDoc *a, const PcDoc *b, double *mean)
{
    PcBuf *ca, *cb;
    size_t i, n;
    int maxd = 0;
    double sum = 0;
    if (a->s.w != b->s.w || a->s.h != b->s.h) return 999;
    ca = pc_doc_composite(a);
    cb = pc_doc_composite(b);
    if (!ca || !cb) { pc_buf_unref(ca); pc_buf_unref(cb); return 999; }
    n = (size_t)a->s.w * a->s.h;
    for (i = 0; i < n; i++) {
        int k, lim = (ca->px[i * 4 + 3] == 0 && cb->px[i * 4 + 3] == 0) ? 0 : 3;
        for (k = 3 - (lim ? 3 : 0); k < 4; k++) {
            int dd = abs(ca->px[i * 4 + k] - cb->px[i * 4 + k]);
            if (dd > maxd) maxd = dd;
            sum += dd;
        }
    }
    if (mean) *mean = sum / (double)(n * 4);
    pc_buf_unref(ca); pc_buf_unref(cb);
    return maxd;
}

/* Matte a doc's composite against white into a fresh opaque doc. */
static PcDoc *matted(const PcDoc *d)
{
    PcDoc *m = pc_doc_new(d->s.w, d->s.h, d->s.mode, 72, 0, 0);
    PcBuf *c = pc_doc_composite(d);
    u8 *p = pc_layer_pixels_w(m->s.layers[0]);
    size_t i, n = (size_t)d->s.w * d->s.h;
    for (i = 0; i < n; i++) {
        int a = c->px[i * 4 + 3], k;
        for (k = 0; k < 3; k++) p[i * 4 + k] = (u8)((c->px[i * 4 + k] * a + 255 * (255 - a) + 127) / 255);
        p[i * 4 + 3] = 255;
    }
    pc_buf_unref(c);
    return m;
}

static PcDoc *roundtrip(const PcDoc *d, int fmt, const PcSaveOpts *o, const char *dumpname)
{
    u8 *buf = NULL;
    size_t n = 0;
    char err[256] = "";
    PcDoc *r;
    int rc = pc_save_mem(d, fmt, o, &buf, &n, err, sizeof err);
    CHECK_EQ(rc, 0);
    if (rc) { fprintf(stderr, "save %s: %s\n", pc_format_name(fmt), err); return NULL; }
    CHECK_EQ(pc_format_detect(buf, n), fmt == PC_FMT_TGA ? PC_FMT_UNKNOWN : fmt);
    if (dumpname) dump(dumpname, buf, n);
    r = pc_load_mem(buf, n, fmt == PC_FMT_TGA ? "x.tga" : "noext", err, sizeof err);
    CHECK(r != NULL);
    if (!r) fprintf(stderr, "load %s: %s\n", pc_format_name(fmt), err);
    else CHECK_EQ(r->format, fmt);
    free(buf);
    return r;
}

/* ---------------- per-format round trips ---------------- */

static void test_lossless(int fmt, const PcSaveOpts *o, int keeps_alpha, const char *tag)
{
    static const int sizes[][2] = { { 37, 23 }, { 1, 1 }, { 2, 3 }, { 130, 70 } };
    unsigned si;
    int mode, alpha;
    for (si = 0; si < sizeof sizes / sizeof sizes[0]; si++)
        for (mode = 0; mode < 2; mode++)
            for (alpha = 0; alpha < 2; alpha++) {
                PcDoc *d = mk_doc(sizes[si][0], sizes[si][1], mode, alpha, 0), *r, *ref;
                char name[64];
                int md;
                d->s.dpi = 150;
                snprintf(name, sizeof name, "%s_%dx%d_m%d_a%d.%s", tag, sizes[si][0], sizes[si][1], mode, alpha, pc_format_ext(fmt));
                r = roundtrip(d, fmt, o, si == 0 ? name : NULL);
                if (r) {
                    ref = (alpha && !keeps_alpha) ? matted(d) : d;
                    md = cmp_comp(ref, r, NULL);
                    CHECK_EQ(md, 0);
                    if (md) fprintf(stderr, "  (%s %dx%d mode %d alpha %d)\n", tag, sizes[si][0], sizes[si][1], mode, alpha);
                    /* (GIF reopens as RGB like an Indexed file; gray Targas with
                     * transparency are written as 32-bit RGBA) */
                    if (fmt != PC_FMT_GIF) CHECK_EQ(r->s.mode, (fmt == PC_FMT_TGA && alpha) ? PC_MODE_RGB : mode);
                    if (fmt != PC_FMT_TGA && fmt != PC_FMT_GIF) CHECK_NEAR(r->s.dpi, 150, 0);
                    CHECK_EQ(r->s.nlayers, 1);
                    CHECK_EQ(r->s.layers[0]->is_background, !(alpha && keeps_alpha));
                    CHECK_EQ(r->dirty, 0);
                    CHECK_EQ(r->nhist, 1);
                    if (ref != d) pc_doc_free(ref);
                    pc_doc_free(r);
                }
                pc_doc_free(d);
            }
}

static void test_formats(void)
{
    PcSaveOpts o;
    PcDoc *d, *r;
    double mean;
    int q;

    pc_save_opts_default(&o);
    test_lossless(PC_FMT_PNG, &o, 1, "png");
    o.tiff_compress = 0; test_lossless(PC_FMT_TIFF, &o, 1, "tif_raw");
    o.tiff_compress = 1; test_lossless(PC_FMT_TIFF, &o, 1, "tif_lzw");
    o.tiff_compress = 2; test_lossless(PC_FMT_TIFF, &o, 1, "tif_pb");
    o.tga_rle = 0; test_lossless(PC_FMT_TGA, &o, 1, "tga_raw");
    o.tga_rle = 1; test_lossless(PC_FMT_TGA, &o, 1, "tga_rle");
    o.bmp_depth = 24; test_lossless(PC_FMT_BMP, &o, 0, "bmp24");
    pc_save_opts_default(&o);
    o.save_alpha = 0; test_lossless(PC_FMT_PNG, &o, 0, "png_noalpha");
    pc_save_opts_default(&o);

    /* BMP 32-bit keeps alpha (RGB docs; gray always writes 8-bit) */
    o.bmp_depth = 32;
    d = mk_doc(37, 23, PC_MODE_RGB, 1, 0);
    r = roundtrip(d, PC_FMT_BMP, &o, "bmp32.bmp");
    if (r) { CHECK_EQ(cmp_comp(d, r, NULL), 0); CHECK_EQ(r->s.layers[0]->is_background, 0); pc_doc_free(r); }
    pc_doc_free(d);
    pc_save_opts_default(&o);

    /* TIFF LZW across table resets: a noisy 300x200 RGB image */
    {
        u8 *p;
        size_t i;
        d = pc_doc_new(300, 200, PC_MODE_RGB, 72, 0, 0);
        p = pc_layer_pixels_w(d->s.layers[0]);
        for (i = 0; i < 300 * 200; i++) { p[i * 4] = (u8)rng(); p[i * 4 + 1] = (u8)(rng() & 0x0F); p[i * 4 + 2] = (u8)(i / 300); }
        o.tiff_compress = 1;
        r = roundtrip(d, PC_FMT_TIFF, &o, "noise_lzw.tif");
        if (r) { CHECK_EQ(cmp_comp(d, r, NULL), 0); pc_doc_free(r); }
        o.tiff_compress = 2;
        r = roundtrip(d, PC_FMT_TIFF, &o, NULL);
        if (r) { CHECK_EQ(cmp_comp(d, r, NULL), 0); pc_doc_free(r); }
        r = roundtrip(d, PC_FMT_PNG, &o, NULL);
        if (r) { CHECK_EQ(cmp_comp(d, r, NULL), 0); pc_doc_free(r); }
        /* GIF of a noisy image: quantized, close on average */
        o.gif_dither = 0;
        r = roundtrip(d, PC_FMT_GIF, &o, "noise.gif");
        if (r) { cmp_comp(d, r, &mean); CHECK(mean < 12); pc_doc_free(r); }
        o.gif_dither = 1;
        r = roundtrip(d, PC_FMT_GIF, &o, "noise_dither.gif");
        if (r) { cmp_comp(d, r, &mean); CHECK(mean < 16); pc_doc_free(r); }
        pc_doc_free(d);
        pc_save_opts_default(&o);
    }

    /* GIF: <= 255 colours + binary transparency round-trip exactly */
    {
        int inter, alpha;
        for (inter = 0; inter < 2; inter++)
            for (alpha = 0; alpha < 3; alpha += 2) {
                o.gif_interlace = inter;
                d = mk_doc(37, 23, PC_MODE_RGB, alpha, 1);
                r = roundtrip(d, PC_FMT_GIF, &o, inter ? "pal_inter.gif" : "pal.gif");
                if (r) { CHECK_EQ(cmp_comp(d, r, NULL), 0); pc_doc_free(r); }
                pc_doc_free(d);
            }
        /* gray docs: 256-level palette, exact */
        d = mk_doc(64, 9, PC_MODE_GRAY, 0, 0);
        r = roundtrip(d, PC_FMT_GIF, &o, "gray.gif");
        if (r) { CHECK_EQ(cmp_comp(d, r, NULL), 0); pc_doc_free(r); }
        pc_doc_free(d);
        d = mk_doc(1, 1, PC_MODE_RGB, 0, 0);
        r = roundtrip(d, PC_FMT_GIF, &o, NULL);
        if (r) { CHECK_EQ(cmp_comp(d, r, NULL), 0); pc_doc_free(r); }
        pc_doc_free(d);
        /* partial alpha: transparent below 128, matted against white above */
        d = mk_doc(37, 23, PC_MODE_RGB, 1, 1);
        r = roundtrip(d, PC_FMT_GIF, &o, NULL);
        if (r) {
            PcBuf *ca = pc_doc_composite(d), *cb = pc_doc_composite(r);
            size_t i, bad = 0;
            for (i = 0; i < 37 * 23; i++) {
                int a = ca->px[i * 4 + 3];
                if ((a < 128) != (cb->px[i * 4 + 3] == 0)) bad++;
                if (a >= 128 && cb->px[i * 4 + 3] != 255) bad++;
            }
            CHECK_EQ(bad, 0);
            pc_buf_unref(ca); pc_buf_unref(cb);
            pc_doc_free(r);
        }
        pc_doc_free(d);
        pc_save_opts_default(&o);
    }

    /* JPEG: lossy, close (on a smooth image: JPEG is not meant for noise) */
    for (q = 0; q <= 12; q += 4) {
        o.jpeg_quality = q;
        d = mk_smooth(37, 23, PC_MODE_RGB);
        d->s.dpi = 300;
        r = roundtrip(d, PC_FMT_JPEG, &o, q == 12 ? "q12.jpg" : NULL);
        if (r) {
            cmp_comp(d, r, &mean);
            CHECK(mean < (q >= 8 ? 4 : 10));
            if (!(mean < (q >= 8 ? 4 : 10))) fprintf(stderr, "  jpeg q%d mean %.2f\n", q, mean);
            CHECK_NEAR(r->s.dpi, 300, 0);
            CHECK_EQ(r->s.mode, PC_MODE_RGB);
            pc_doc_free(r);
        }
        pc_doc_free(d);
    }
    o.jpeg_quality = 10;
    d = mk_smooth(40, 24, PC_MODE_GRAY);
    r = roundtrip(d, PC_FMT_JPEG, &o, "gray.jpg");
    if (r) {
        cmp_comp(d, r, &mean);
        CHECK(mean < 3);
        CHECK_EQ(r->s.mode, PC_MODE_GRAY);
        pc_doc_free(r);
    }
    pc_doc_free(d);
    d = mk_doc(1, 1, PC_MODE_RGB, 0, 0);
    r = roundtrip(d, PC_FMT_JPEG, &o, NULL);
    if (r) { CHECK_NEAR(cmp_comp(d, r, NULL), 0, 8); pc_doc_free(r); }
    pc_doc_free(d);

    /* our BMP and PSD writers read back by an independent decoder (stb) */
    {
        u8 *buf;
        size_t n;
        char err[128];
        int w, h, c;
        unsigned char *px;
        d = mk_doc(37, 23, PC_MODE_RGB, 0, 0);
        o.bmp_depth = 24;
        CHECK_EQ(pc_save_mem(d, PC_FMT_BMP, &o, &buf, &n, err, sizeof err), 0);
        px = stbi_load_from_memory(buf, (int)n, &w, &h, &c, 4);
        CHECK(px != NULL);
        if (px) {
            PcBuf *cc = pc_doc_composite(d);
            CHECK_EQ(w, 37); CHECK_EQ(h, 23);
            CHECK(!memcmp(px, cc->px, 37 * 23 * 4));
            pc_buf_unref(cc);
            stbi_image_free(px);
        }
        free(buf);
        pc_doc_free(d);
    }
}

/* ---------------- PSD ---------------- */

static void fill_layer(PcLayer *l, int w, int h, int x0, int y0, int x1, int y1, int r, int g, int b, int a)
{
    u8 *p = pc_layer_pixels_w(l);
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            u8 *q = p + ((size_t)y * w + x) * 4;
            q[0] = (u8)((r + x * 3) & 255); q[1] = (u8)((g + y * 5) & 255); q[2] = (u8)b; q[3] = (u8)(a - ((x + y) & 15));
        }
}

static PcDoc *mk_layered(int mode)
{
    int W = 41, H = 29;
    PcDoc *d = pc_doc_new(W, H, mode, 300, 0, 0);
    PcLayer *l;
    u8 *m;
    int x, y;
    /* Background gradient */
    {
        u8 *p = pc_layer_pixels_w(d->s.layers[0]);
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++, p += 4) {
                int v = (x * 6 + y * 3) & 255;
                p[0] = (u8)v; p[1] = (u8)(mode ? v : 255 - v); p[2] = (u8)(mode ? v : (x * y) & 255); p[3] = 255;
            }
    }
    l = pc_doc_add_layer(d, "Ebene \xC9t\xE9 \x96 \x80uro");     /* non-ASCII cp1252 */
    fill_layer(l, W, H, 3, 4, 30, 20, 200, 10, 60, 255);
    l->opacity = 128; l->blend = PC_BLEND_MULTIPLY;
    l = pc_doc_add_layer(d, "Hidden screen");
    fill_layer(l, W, H, 10, 0, 41, 29, 20, 100, 220, 200);
    l->visible = 0; l->blend = PC_BLEND_SCREEN; l->lock_alpha = 1;
    l = pc_doc_add_layer(d, "Masked + clipped");
    fill_layer(l, W, H, 0, 10, 25, 29, 90, 90, 90, 255);
    l->clip = 1; l->blend = PC_BLEND_OVERLAY; l->lock_pixels = 1; l->lock_position = 1;
    pc_doc_add_mask(d, d->s.active, 1, 0);
    m = pc_layer_mask_w(l);
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) m[y * W + x] = (u8)(x < 20 ? 255 : (x * 9) & 255);
    l = pc_doc_add_layer(d, "Empty");
    l = pc_doc_add_layer(d, "Dark mask");
    fill_layer(l, W, H, 5, 5, 15, 15, 1, 2, 3, 255);
    pc_doc_add_mask(d, d->s.active, 1, 0);
    m = pc_layer_mask_w(l);
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) m[y * W + x] = (u8)((x > 6 && x < 9 && y > 7 && y < 12) ? 200 : 0);
    l->mask_enabled = 0;
    if (mode == PC_MODE_GRAY)
        for (y = 1; y < d->s.nlayers; y++) {
            u8 *p = pc_layer_pixels_w(d->s.layers[y]);
            size_t i;
            for (i = 0; i < (size_t)W * H; i++) p[i * 4] = p[i * 4 + 1] = p[i * 4 + 2] = (u8)pc_luma(p[i * 4], p[i * 4 + 1], p[i * 4 + 2]);
        }
    /* alpha channels */
    d->s.nchannels = 2;
    pc_strlcpy(d->s.channels[0].name, "Selection \xC4", sizeof d->s.channels[0].name);
    d->s.channels[0].buf = pc_buf_new(W, H, 1, 1);
    for (x = 0; x < W * H; x++) d->s.channels[0].buf->px[x] = (u8)(x * 7);
    pc_strlcpy(d->s.channels[1].name, "Alpha 2", sizeof d->s.channels[1].name);
    d->s.channels[1].buf = pc_buf_new(W, H, 1, 1);
    d->s.active = 2;
    return d;
}

static void check_same_layers(const PcDoc *a, const PcDoc *b)
{
    int i;
    size_t n = (size_t)a->s.w * a->s.h, k;
    CHECK_EQ(b->s.nlayers, a->s.nlayers);
    if (b->s.nlayers != a->s.nlayers) return;
    for (i = 0; i < a->s.nlayers; i++) {
        const PcLayer *x = a->s.layers[i], *y = b->s.layers[i];
        size_t bad = 0;
        CHECK(!strcmp(x->name, y->name));
        if (strcmp(x->name, y->name)) fprintf(stderr, "  name '%s' vs '%s'\n", x->name, y->name);
        CHECK_EQ(y->opacity, x->opacity);
        CHECK_EQ(y->blend, x->blend);
        CHECK_EQ(y->visible, x->visible);
        CHECK_EQ(y->clip, x->clip);
        CHECK_EQ(y->lock_alpha, x->lock_alpha);
        CHECK_EQ(y->lock_pixels, x->lock_pixels);
        CHECK_EQ(y->lock_position, x->lock_position);
        CHECK_EQ(y->is_background, x->is_background);
        CHECK_EQ(!!y->mask, !!x->mask);
        for (k = 0; k < n; k++) {
            const u8 *p = x->pix->px + k * 4, *q = y->pix->px + k * 4;
            if (p[3] != q[3] || (p[3] && memcmp(p, q, 3))) bad++;
        }
        CHECK_EQ(bad, 0);
        if (x->mask && y->mask) {
            CHECK(!memcmp(x->mask->px, y->mask->px, n));
            CHECK_EQ(y->mask_enabled, x->mask_enabled);
        }
    }
}

static void test_psd_roundtrip(void)
{
    int mode;
    for (mode = 0; mode < 2; mode++) {
        PcDoc *d = mk_layered(mode), *r;
        u8 *buf;
        size_t n;
        char err[128];
        CHECK_EQ(pc_psd_save(d, &buf, &n, err, sizeof err), 0);
        dump(mode ? "layered_gray.psd" : "layered.psd", buf, n);
        r = pc_psd_load(buf, n, err, sizeof err);
        CHECK(r != NULL);
        if (r) {
            int i;
            check_same_layers(d, r);
            CHECK_EQ(r->s.mode, mode);
            CHECK_NEAR(r->s.dpi, 300, 0);
            CHECK_EQ(r->s.active, 2);
            CHECK_EQ(r->s.nchannels, 2);
            if (r->s.nchannels == 2) {
                CHECK(!strcmp(r->s.channels[0].name, "Selection \xC4"));
                CHECK(!strcmp(r->s.channels[1].name, "Alpha 2"));
                for (i = 0; i < 2; i++) CHECK(!memcmp(r->s.channels[i].buf->px, d->s.channels[i].buf->px, (size_t)d->s.w * d->s.h));
            }
            CHECK_EQ(cmp_comp(d, r, NULL), 0);
            CHECK_EQ(r->format, PC_FMT_PSD);
            pc_doc_free(r);
        }
        /* stb's PSD loader reads our merged image (without alpha channels:
         * stb would take the first one for transparency) */
        if (mode == 0) {
            int w, h, c, i;
            unsigned char *px;
            for (i = 0; i < d->s.nchannels; i++) pc_buf_unref(d->s.channels[i].buf);
            d->s.nchannels = 0;
            free(buf);
            CHECK_EQ(pc_psd_save(d, &buf, &n, err, sizeof err), 0);
            px = stbi_load_from_memory(buf, (int)n, &w, &h, &c, 4);
            CHECK(px != NULL);
            if (px) {
                PcBuf *cc = pc_doc_composite(d);
                size_t k, bad = 0;
                CHECK_EQ(w, d->s.w); CHECK_EQ(h, d->s.h);
                for (k = 0; k < (size_t)w * h * 4; k++) if (abs(px[k] - cc->px[k]) > 1) bad++;
                CHECK_EQ(bad, 0);
                pc_buf_unref(cc);
                stbi_image_free(px);
            }
        }
        free(buf);
        pc_doc_free(d);
    }
    /* transparent-only doc (no Background): merged alpha and negative layer count */
    {
        PcDoc *d = mk_doc(19, 11, PC_MODE_RGB, 1, 0), *r;
        u8 *buf;
        size_t n;
        char err[128];
        int w, h, c;
        unsigned char *px;
        CHECK_EQ(pc_psd_save(d, &buf, &n, err, sizeof err), 0);
        dump("transparent.psd", buf, n);
        r = pc_load_mem(buf, n, "a.psd", err, sizeof err);
        if (r) { check_same_layers(d, r); CHECK_EQ(r->s.nchannels, 0); pc_doc_free(r); }
        else CHECK(0);
        px = stbi_load_from_memory(buf, (int)n, &w, &h, &c, 4);
        CHECK(px != NULL);
        if (px) {
            PcBuf *cc = pc_doc_composite(d);
            size_t k, bad = 0;
            for (k = 0; k < (size_t)w * h; k++) {
                if (abs(px[k * 4 + 3] - cc->px[k * 4 + 3]) > 0) bad++;
                else if (cc->px[k * 4 + 3] > 16) { int j; for (j = 0; j < 3; j++) if (abs(px[k * 4 + j] - cc->px[k * 4 + j]) > 12) bad++; }
            }
            CHECK_EQ(bad, 0);
            pc_buf_unref(cc);
            stbi_image_free(px);
        }
        free(buf);
        pc_doc_free(d);
    }
}

/* ---- hand-built PSDs ---- */

typedef struct { u8 *p; size_t n, cap; } B;
static void b_put(B *b, const void *s, size_t k)
{
    if (b->n + k > b->cap) { b->cap = (b->n + k) * 2 + 64; b->p = (u8 *)realloc(b->p, b->cap); }
    memcpy(b->p + b->n, s, k); b->n += k;
}
static void b8(B *b, unsigned v) { u8 c = (u8)v; b_put(b, &c, 1); }
static void b16(B *b, unsigned v) { b8(b, v >> 8); b8(b, v); }
static void b32(B *b, unsigned v) { b16(b, v >> 16); b16(b, v & 0xFFFF); }
static void bpatch32(B *b, size_t at, unsigned v) { b->p[at] = (u8)(v >> 24); b->p[at + 1] = (u8)(v >> 16); b->p[at + 2] = (u8)(v >> 8); b->p[at + 3] = (u8)v; }

typedef struct {
    int top, left, bottom, right;
    int nch;
    int ids[5];
    int comp;                 /* 0 raw, 1 RLE, 2 zip, 3 zip+prediction */
    unsigned char fill[5];    /* constant plane values per channel (with a ramp in channel 0) */
    const char *name;
    const char *uname;        /* ASCII stand-in for UTF-16 'luni' content, or NULL */
    const unsigned short *uni; int nuni;
    int section;              /* lsct type, 0 none */
    int flags, opacity;
    const char *key;
} HLayer;

static void plane_for(const HLayer *L, int c, int w, int h, int depth, B *out)
{
    int x, y, bps = depth / 8;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            unsigned v = L->fill[c];
            if (c == 1 && L->ids[c] == 0) v = (unsigned)(L->fill[c] + x * 10 + y) & 255;
            if (bps == 2) b16(out, v * 257); else b8(out, v);
        }
}

static void encode_plane(int comp, const u8 *raw, int w, int h, int depth, B *out)
{
    int y;
    b16(out, (unsigned)comp);
    if (comp == 0) { b_put(out, raw, (size_t)w * h * (depth / 8)); return; }
    if (comp == 1) {
        size_t rb = (size_t)w * (depth / 8);
        B rows = { 0, 0, 0 };
        for (y = 0; y < h; y++) {
            /* literal-only PackBits, max 128 per packet */
            size_t o = 0, start = rows.n;
            const u8 *s = raw + y * rb;
            while (o < rb) { size_t k = rb - o > 128 ? 128 : rb - o; b8(&rows, (unsigned)(k - 1)); b_put(&rows, s + o, k); o += k; }
            b16(out, (unsigned)(rows.n - start));
        }
        b_put(out, rows.p, rows.n);
        free(rows.p);
        return;
    }
    {
        size_t rb = (size_t)w * (depth / 8), i;
        u8 *t = (u8 *)malloc(rb * h + 1);
        int len;
        unsigned char *z;
        memcpy(t, raw, rb * h);
        if (comp == 3) {
            for (y = 0; y < h; y++) {
                u8 *row = t + y * rb;
                if (depth == 16) {
                    for (i = (size_t)w - 1; i >= 1; i--) {
                        unsigned a = row[2 * i] << 8 | row[2 * i + 1], b = row[2 * i - 2] << 8 | row[2 * i - 1], v = (a - b) & 0xFFFF;
                        row[2 * i] = (u8)(v >> 8); row[2 * i + 1] = (u8)v;
                    }
                } else for (i = rb - 1; i >= 1; i--) row[i] = (u8)(row[i] - row[i - 1]);
            }
        }
        z = stbi_zlib_compress(t, (int)(rb * h), &len, 6);
        b_put(out, z, (size_t)len);
        STBIW_FREE(z);
        free(t);
    }
}

static int g_alpha_ids;     /* build_psd: write an alpha identifiers resource */

/* Build a PSD: merged planes are constant colour (value 77) unless given. */
static B build_psd(int W, int H, int mode, int depth, int nchan, const HLayer *L, int nl, int lr16, const u8 *palette)
{
    B b = { 0, 0, 0 };
    int i, c;
    size_t at, li_at;
    b_put(&b, "8BPS", 4); b16(&b, 1); b32(&b, 0); b16(&b, 0);
    b16(&b, (unsigned)nchan); b32(&b, (unsigned)H); b32(&b, (unsigned)W); b16(&b, (unsigned)depth); b16(&b, (unsigned)mode);
    if (palette) { b32(&b, 768); b_put(&b, palette, 768); } else b32(&b, 0);
    /* resources: resolution 200 dpi [, alpha identifiers] */
    b32(&b, g_alpha_ids ? 28 + 12 + 4 * (nchan - (mode == 3 || mode == 9 ? 3 : mode == 4 ? 4 : 1)) : 28);
    b_put(&b, "8BIM", 4); b16(&b, 0x03ED); b16(&b, 0); b32(&b, 16);
    b32(&b, 200u << 16); b16(&b, 1); b16(&b, 1); b32(&b, 200u << 16); b16(&b, 1); b16(&b, 1);
    if (g_alpha_ids) {
        int k, ex = nchan - (mode == 3 || mode == 9 ? 3 : mode == 4 ? 4 : 1);
        b_put(&b, "8BIM", 4); b16(&b, 0x041D); b16(&b, 0); b32(&b, (unsigned)(4 * ex));
        for (k = 0; k < ex; k++) b32(&b, (unsigned)(5 + k));
    }
    at = b.n; b32(&b, 0);
    if (nl) {
        B li = { 0, 0, 0 };
        B data = { 0, 0, 0 };
        size_t *lens = (size_t *)calloc((size_t)nl * 5, sizeof(size_t));
        b16(&li, (unsigned)nl);
        for (i = 0; i < nl; i++)
            for (c = 0; c < L[i].nch; c++) {
                int w = L[i].right - L[i].left, h = L[i].bottom - L[i].top;
                size_t before = data.n;
                if (w <= 0 || h <= 0) { b16(&data, 0); }
                else {
                    B raw = { 0, 0, 0 };
                    plane_for(&L[i], c, w, h, depth, &raw);
                    encode_plane(L[i].comp, raw.p, w, h, depth, &data);
                    free(raw.p);
                }
                lens[i * 5 + c] = data.n - before;
            }
        for (i = 0; i < nl; i++) {
            size_t ex;
            b32(&li, (unsigned)L[i].top); b32(&li, (unsigned)L[i].left); b32(&li, (unsigned)L[i].bottom); b32(&li, (unsigned)L[i].right);
            b16(&li, (unsigned)L[i].nch);
            for (c = 0; c < L[i].nch; c++) { b16(&li, (unsigned)(L[i].ids[c] & 0xFFFF)); b32(&li, (unsigned)lens[i * 5 + c]); }
            b_put(&li, "8BIM", 4); b_put(&li, L[i].key ? L[i].key : "norm", 4);
            b8(&li, (unsigned)L[i].opacity); b8(&li, 0); b8(&li, (unsigned)L[i].flags); b8(&li, 0);
            ex = li.n; b32(&li, 0);
            b32(&li, 0);                      /* no mask */
            b32(&li, 0);                      /* no blending ranges */
            {
                size_t nl2 = strlen(L[i].name), pad;
                b8(&li, (unsigned)nl2); b_put(&li, L[i].name, nl2);
                for (pad = (4 - (1 + nl2) % 4) % 4; pad; pad--) b8(&li, 0);
            }
            if (L[i].uni) {
                int k;
                b_put(&li, "8BIMluni", 8); b32(&li, (unsigned)(4 + 2 * L[i].nuni + ((2 * L[i].nuni) % 4)));
                b32(&li, (unsigned)L[i].nuni);
                for (k = 0; k < L[i].nuni; k++) b16(&li, L[i].uni[k]);
                for (k = (2 * L[i].nuni) % 4; k; k--) b8(&li, 0);
            }
            if (L[i].section) { b_put(&li, "8BIMlsct", 8); b32(&li, 4); b32(&li, (unsigned)L[i].section); }
            bpatch32(&li, ex, (unsigned)(li.n - ex - 4));
        }
        b_put(&li, data.p, data.n);
        if (li.n & 1) b8(&li, 0);
        if (!lr16) {
            li_at = b.n; b32(&b, 0);
            b_put(&b, li.p, li.n);
            bpatch32(&b, li_at, (unsigned)(b.n - li_at - 4));
            b32(&b, 0);
        } else {
            b32(&b, 0);                       /* empty layer info */
            b32(&b, 0);                       /* global mask */
            b_put(&b, "8BIMLr16", 8); b32(&b, (unsigned)li.n); b_put(&b, li.p, li.n);
            while ((b.n - at) % 4) b8(&b, 0);
        }
        free(li.p); free(data.p); free(lens);
    }
    bpatch32(&b, at, (unsigned)(b.n - at - 4));
    /* merged image, raw */
    b16(&b, 0);
    for (c = 0; c < nchan; c++) {
        size_t k, np = (size_t)W * H * (depth == 1 ? 1 : depth / 8);
        if (depth == 1) np = (size_t)((W + 7) / 8) * H;
        for (k = 0; k < np; k++) b8(&b, depth == 1 ? 0x55 : (unsigned)(c == 0 ? 77 : c == 1 ? 100 + (k & 15) : 200));
    }
    return b;
}

static void test_psd_handbuilt(void)
{
    char err[128];
    PcDoc *d;
    /* layers partly outside the canvas, RLE / ZIP / ZIP+prediction, luni, groups */
    {
        static const unsigned short uni[] = { 'G', 0x00FC, 'n', 't', 'e', 'r', ' ', 0x4E2D, 0xD83D, 0xDE00, '!' };
        HLayer L[6];
        B b;
        int i;
        memset(L, 0, sizeof L);
        /* 0: background without transparency, full canvas, raw */
        L[0].top = 0; L[0].left = 0; L[0].bottom = 8; L[0].right = 10; L[0].nch = 3;
        L[0].ids[0] = 0; L[0].ids[1] = 1; L[0].ids[2] = 2; L[0].fill[0] = 10; L[0].fill[1] = 20; L[0].fill[2] = 30;
        L[0].name = "Background"; L[0].opacity = 255;
        /* 1: group end divider (hidden in Photoshop) */
        L[1].nch = 0; L[1].name = "</Layer group>"; L[1].section = 3; L[1].opacity = 255;
        /* 2: layer inside the group, out of canvas on the top-left, ZIP */
        L[2].top = -3; L[2].left = -4; L[2].bottom = 3; L[2].right = 2; L[2].nch = 4;
        L[2].ids[0] = -1; L[2].ids[1] = 0; L[2].ids[2] = 1; L[2].ids[3] = 2;
        L[2].fill[0] = 255; L[2].fill[1] = 50; L[2].fill[2] = 60; L[2].fill[3] = 70; L[2].comp = 2;
        L[2].name = "inner"; L[2].opacity = 200; L[2].key = "mul ";
        /* 3: group folder record, hidden */
        L[3].nch = 0; L[3].name = "Group 1"; L[3].section = 1; L[3].flags = 2; L[3].opacity = 255; L[3].key = "pass";
        /* 4: out of canvas bottom-right, ZIP with prediction, Unicode name */
        L[4].top = 5; L[4].left = 7; L[4].bottom = 12; L[4].right = 15; L[4].nch = 4;
        L[4].ids[0] = -1; L[4].ids[1] = 0; L[4].ids[2] = 1; L[4].ids[3] = 2;
        L[4].fill[0] = 128; L[4].fill[1] = 1; L[4].fill[2] = 2; L[4].fill[3] = 3; L[4].comp = 3;
        L[4].name = "legacy"; L[4].uni = uni; L[4].nuni = 11; L[4].opacity = 255; L[4].flags = 1;
        /* 5: completely outside, RLE */
        L[5].top = 20; L[5].left = 20; L[5].bottom = 30; L[5].right = 30; L[5].nch = 2;
        L[5].ids[0] = -1; L[5].ids[1] = 0; L[5].fill[0] = 255; L[5].comp = 1;
        L[5].name = "far \xC3\xA4way"; L[5].opacity = 255;     /* UTF-8 legacy name: a-umlaut */
        b = build_psd(10, 8, 3, 8, 3, L, 6, 0, NULL);
        dump("handbuilt.psd", b.p, b.n);
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (!d) fprintf(stderr, "handbuilt: %s\n", err);
        if (d) {
            const u8 *p;
            CHECK_EQ(d->s.nlayers, 4);
            CHECK_NEAR(d->s.dpi, 200, 0);
            if (d->s.nlayers == 4) {
                CHECK(d->s.layers[0]->is_background);
                CHECK(!strcmp(d->s.layers[0]->name, "Background"));
                p = d->s.layers[0]->pix->px;
                CHECK_EQ(p[0], 10); CHECK_EQ(p[1], 20); CHECK_EQ(p[2], 30); CHECK_EQ(p[3], 255);
                /* inner: canvas (0,0) is layer (4,3); visible area x 0..1, y 0..2; hidden by its group */
                CHECK(!strcmp(d->s.layers[1]->name, "inner"));
                CHECK_EQ(d->s.layers[1]->visible, 0);
                CHECK_EQ(d->s.layers[1]->opacity, 200);
                CHECK_EQ(d->s.layers[1]->blend, PC_BLEND_MULTIPLY);
                p = d->s.layers[1]->pix->px;
                CHECK_EQ(p[3], 255); CHECK_EQ(p[0], (50 + 4 * 10 + 3) & 255); CHECK_EQ(p[1], 60); CHECK_EQ(p[2], 70);
                CHECK_EQ(p[(2 * 10 + 1) * 4 + 3], 255);             /* (1,2) inside */
                CHECK_EQ(p[(2 * 10 + 2) * 4 + 3], 0);               /* (2,2) outside */
                CHECK_EQ(p[(3 * 10 + 0) * 4 + 3], 0);               /* (0,3) outside */
                /* Unicode name: u-umlaut kept, CJK and emoji become '?' */
                CHECK(!strcmp(d->s.layers[2]->name, "G\xFCnter ?" "?!"));
                if (strcmp(d->s.layers[2]->name, "G\xFCnter ?" "?!")) fprintf(stderr, "  name '%s'\n", d->s.layers[2]->name);
                CHECK_EQ(d->s.layers[2]->lock_alpha, 1);
                CHECK_EQ(d->s.layers[2]->visible, 1);
                p = d->s.layers[2]->pix->px;
                CHECK_EQ(p[(5 * 10 + 7) * 4 + 3], 128);
                CHECK_EQ(p[(5 * 10 + 7) * 4 + 0], 1);
                CHECK_EQ(p[(7 * 10 + 9) * 4 + 0], (1 + 2 * 10 + 2) & 255);
                CHECK_EQ(p[(4 * 10 + 9) * 4 + 3], 0);
                CHECK_EQ(p[(5 * 10 + 6) * 4 + 3], 0);
                CHECK(!strcmp(d->s.layers[3]->name, "far \xE4way"));
                /* far away: empty */
                p = d->s.layers[3]->pix->px;
                for (i = 0; i < 80; i++) if (p[i * 4 + 3]) break;
                CHECK_EQ(i, 80);
            }
            pc_doc_free(d);
        }
        free(b.p);
        /* the same layers in a 16-bit file (Lr16 block) */
        for (i = 0; i < 6; i++) if (L[i].comp == 1) L[i].comp = 0;
        b = build_psd(10, 8, 3, 16, 3, L, 6, 1, NULL);
        dump("handbuilt16.psd", b.p, b.n);
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            CHECK_EQ(d->s.nlayers, 4);
            if (d->s.nlayers == 4) {
                const u8 *p = d->s.layers[2]->pix->px;
                CHECK_EQ(p[(7 * 10 + 9) * 4 + 0], (1 + 2 * 10 + 2) & 255);
                CHECK_EQ(p[(5 * 10 + 7) * 4 + 3], 128);
                CHECK_EQ(d->s.layers[1]->pix->px[1], 60);
            }
            pc_doc_free(d);
        }
        free(b.p);
    }
    /* more layers than we hold: the first PC_MAX_LAYERS load, with a note */
    {
        int nl = PC_MAX_LAYERS + 4, i;
        HLayer *L = (HLayer *)calloc((size_t)nl, sizeof *L);
        B b;
        for (i = 0; i < nl; i++) {
            L[i].top = i % 4; L[i].left = 0; L[i].bottom = L[i].top + 1; L[i].right = 4; L[i].nch = 4;
            L[i].ids[0] = -1; L[i].ids[1] = 0; L[i].ids[2] = 1; L[i].ids[3] = 2;
            L[i].fill[0] = 255; L[i].fill[1] = (unsigned char)i; L[i].name = "many"; L[i].opacity = 255; L[i].comp = 1;
        }
        b = build_psd(4, 4, 3, 8, 3, L, nl, 0, NULL);
        err[0] = 0;
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            CHECK_EQ(d->s.nlayers, PC_MAX_LAYERS);
            CHECK(strstr(err, "layers") != NULL);
            pc_doc_free(d);
        }
        free(b.p);
        free(L);
    }
    /* flat files in other modes: 16-bit RGB, CMYK, gray + alpha channel, bitmap, indexed, Lab */
    {
        B b;
        u8 pal[768];
        int k;
        b = build_psd(5, 4, 3, 16, 3, NULL, 0, 0, NULL);
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            /* 16-bit raw bytes 77,77 -> 0x4D4D -> 77 */
            CHECK_EQ(d->s.layers[0]->pix->px[0], 77);
            CHECK(d->s.layers[0]->is_background);
            CHECK_EQ(d->s.nchannels, 0);
            pc_doc_free(d);
        }
        free(b.p);
        g_alpha_ids = 1;                                      /* extra channels are alpha channels */
        b = build_psd(5, 4, 4, 8, 5, NULL, 0, 0, NULL);       /* CMYK + 1 alpha */
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            /* C=77 M=100 Y=200 K=200 stored inverted: R = 77*200/255 */
            CHECK_NEAR(d->s.layers[0]->pix->px[0], 77 * 200 / 255, 1);
            CHECK_EQ(d->s.mode, PC_MODE_RGB);
            CHECK_EQ(d->s.nchannels, 1);
            if (d->s.nchannels == 1) CHECK(!strcmp(d->s.channels[0].name, "Alpha 1"));
            pc_doc_free(d);
        }
        free(b.p);
        g_alpha_ids = 0;                                      /* flat file, no ids: transparency */
        b = build_psd(5, 4, 4, 8, 5, NULL, 0, 0, NULL);
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            CHECK_EQ(d->s.nchannels, 0);
            CHECK_EQ(d->s.layers[0]->is_background, 0);
            CHECK_EQ(d->s.layers[0]->pix->px[3], 200);
            pc_doc_free(d);
        }
        free(b.p);
        g_alpha_ids = 1;
        b = build_psd(5, 4, 1, 8, 2, NULL, 0, 0, NULL);       /* gray + alpha channel */
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            CHECK_EQ(d->s.mode, PC_MODE_GRAY);
            CHECK_EQ(d->s.layers[0]->pix->px[2], 77);
            CHECK_EQ(d->s.nchannels, 1);
            if (d->s.nchannels) CHECK_EQ(d->s.channels[0].buf->px[1], 101);
            pc_doc_free(d);
        }
        free(b.p);
        g_alpha_ids = 0;
        b = build_psd(12, 3, 0, 1, 1, NULL, 0, 0, NULL);      /* bitmap 0x55: alternating */
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            CHECK_EQ(d->s.layers[0]->pix->px[0], 255);
            CHECK_EQ(d->s.layers[0]->pix->px[4], 0);
            pc_doc_free(d);
        }
        free(b.p);
        for (k = 0; k < 256; k++) { pal[k] = (u8)k; pal[256 + k] = (u8)(255 - k); pal[512 + k] = 9; }
        b = build_psd(4, 4, 2, 8, 1, NULL, 0, 0, pal);
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) {
            const u8 *p = d->s.layers[0]->pix->px;
            CHECK_EQ(p[0], 77); CHECK_EQ(p[1], 255 - 77); CHECK_EQ(p[2], 9);
            pc_doc_free(d);
        }
        free(b.p);
        b = build_psd(4, 4, 9, 8, 3, NULL, 0, 0, NULL);       /* Lab: approximate */
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d != NULL);
        if (d) pc_doc_free(d);
        free(b.p);
        b = build_psd(4, 4, 3, 8, 3, NULL, 0, 0, NULL);
        b.p[5] = 2;                                           /* PSB */
        d = pc_load_mem(b.p, b.n, NULL, err, sizeof err);
        CHECK(d == NULL);
        CHECK(strstr(err, "PSB") != NULL);
        free(b.p);
    }
}

/* ---- hand-built TIFFs: byte order, planar, tiles, 16-bit ---- */

static unsigned tsamp(int x, int y, int s) { return (unsigned)(x * 40 + y * 25 + s * 60) & 255; }

static void t16w(B *b, int be, unsigned v) { if (be) b16(b, v); else { b8(b, v & 255); b8(b, v >> 8); } }
static void t32w(B *b, int be, unsigned v) { if (be) b32(b, v); else { t16w(b, 0, v & 0xFFFF); t16w(b, 0, v >> 16); } }

static B build_tiff(int be, int w, int h, int spp, int bps, int planar, int tile, int photo, int extra)
{
    B b = { 0, 0, 0 };
    int tw = tile ? 16 : w, th = tile ? 16 : h, across = (w + tw - 1) / tw, down = (h + th - 1) / th;
    int planes = planar == 2 ? spp : 1, nchunk = across * down * planes, k, x, y, s;
    unsigned offs[64], cnts[64];
    size_t bps_at, offs_at, cnts_at;
    int ne = 0;
    struct { unsigned tag, type, count, val; } e[16];
    b_put(&b, be ? "MM" : "II", 2); t16w(&b, be, 42); t32w(&b, be, 0);
    for (k = 0; k < nchunk; k++) {
        int plane = k / (across * down), ci = k % (across * down), cx = (ci % across) * tw, cy = (ci / across) * th;
        int sppc = planar == 2 ? 1 : spp;
        offs[k] = (unsigned)b.n;
        for (y = 0; y < th; y++)
            for (x = 0; x < tw; x++)
                for (s = 0; s < sppc; s++) {
                    int ss = planar == 2 ? plane : s;
                    unsigned v = (cx + x < w && cy + y < h) ? tsamp(cx + x, cy + y, ss) : 0;
                    if (bps == 16) t16w(&b, be, v * 257); else b8(&b, v);
                }
        cnts[k] = (unsigned)(b.n - offs[k]);
    }
    bps_at = b.n; for (s = 0; s < spp; s++) t16w(&b, be, (unsigned)bps);
    offs_at = b.n; for (k = 0; k < nchunk; k++) t32w(&b, be, offs[k]);
    cnts_at = b.n; for (k = 0; k < nchunk; k++) t32w(&b, be, cnts[k]);
#define E(tg, ty, cn, v) do { e[ne].tag = tg; e[ne].type = ty; e[ne].count = cn; e[ne].val = v; ne++; } while (0)
    E(256, 3, 1, (unsigned)w); E(257, 3, 1, (unsigned)h);
    E(258, 3, (unsigned)spp, spp > 2 ? (unsigned)bps_at : (unsigned)bps);
    E(259, 3, 1, 1); E(262, 3, 1, (unsigned)photo);
    if (!tile) E(273, 4, (unsigned)nchunk, nchunk > 1 ? (unsigned)offs_at : offs[0]);
    E(277, 3, 1, (unsigned)spp);
    if (!tile) { E(278, 4, 1, (unsigned)h); E(279, 4, (unsigned)nchunk, nchunk > 1 ? (unsigned)cnts_at : cnts[0]); }
    E(284, 3, 1, (unsigned)planar);
    if (tile) { E(322, 3, 1, 16); E(323, 3, 1, 16); E(324, 4, (unsigned)nchunk, (unsigned)offs_at); E(325, 4, (unsigned)nchunk, (unsigned)cnts_at); }
    if (extra) E(338, 3, 1, (unsigned)extra);
#undef E
    if (b.n & 1) b8(&b, 0);
    if (be) bpatch32(&b, 4, (unsigned)b.n);
    else { b.p[4] = (u8)b.n; b.p[5] = (u8)(b.n >> 8); b.p[6] = (u8)(b.n >> 16); b.p[7] = (u8)(b.n >> 24); }
    t16w(&b, be, (unsigned)ne);
    for (k = 0; k < ne; k++) {
        t16w(&b, be, e[k].tag); t16w(&b, be, e[k].type); t32w(&b, be, e[k].count);
        if (e[k].type == 3 && e[k].count == 1) { t16w(&b, be, e[k].val); t16w(&b, be, 0); }
        else if (e[k].type == 3 && e[k].count == 2) { t16w(&b, be, e[k].val); t16w(&b, be, e[k].val); }
        else t32w(&b, be, e[k].val);
    }
    t32w(&b, be, 0);
    return b;
}

static void test_tiff_handbuilt(void)
{
    static const struct { int be, spp, bps, planar, tile, photo, extra; } cfg[] = {
        { 1, 3, 8, 1, 0, 2, 0 }, { 1, 3, 16, 1, 0, 2, 0 }, { 0, 3, 8, 2, 0, 2, 0 }, { 1, 4, 8, 2, 0, 2, 2 },
        { 0, 3, 8, 1, 1, 2, 0 }, { 1, 4, 16, 1, 1, 2, 2 }, { 0, 1, 8, 1, 0, 0, 0 }, { 1, 2, 16, 2, 1, 1, 2 },
        { 0, 4, 8, 1, 0, 2, 1 },
    };
    unsigned i;
    for (i = 0; i < sizeof cfg / sizeof cfg[0]; i++) {
        int w = 20, h = 18, x, y, bad = 0;
        char err[128];
        B b = build_tiff(cfg[i].be, w, h, cfg[i].spp, cfg[i].bps, cfg[i].planar, cfg[i].tile, cfg[i].photo, cfg[i].extra);
        PcDoc *d = pc_load_mem(b.p, b.n, "x.tif", err, sizeof err);
        CHECK(d != NULL);
        if (!d) fprintf(stderr, "  tiff cfg %u: %s\n", i, err);
        if (d) {
            const u8 *p = d->s.layers[0]->pix->px;
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++, p += 4) {
                    int a = cfg[i].extra ? (int)tsamp(x, y, cfg[i].spp - 1) : 255, k;
                    for (k = 0; k < 3; k++) {
                        int v = (int)tsamp(x, y, cfg[i].spp >= 3 ? k : 0);
                        if (cfg[i].photo == 0) v = 255 - v;
                        if (cfg[i].extra == 1 && a > 0 && a < 255) v = PC_MIN(255, (v * 255 + a / 2) / a);   /* associated */
                        if (a && p[k] != v) bad++;
                    }
                    if (p[3] != a) bad++;
                }
            CHECK_EQ(bad, 0);
            if (bad) fprintf(stderr, "  tiff cfg %u: %d bad samples\n", i, bad);
            CHECK_EQ(d->s.mode, cfg[i].spp >= 3 ? PC_MODE_RGB : PC_MODE_GRAY);
            pc_doc_free(d);
        }
        fuzz_one(b.p, b.n, "x.tif", 400);
        free(b.p);
    }
}

/* ---------------- detection ---------------- */

static void test_detect(void)
{
    CHECK_EQ(pc_format_from_ext("C:\\Images\\photo.JPEG"), PC_FMT_JPEG);
    CHECK_EQ(pc_format_from_ext("a.jpe"), PC_FMT_JPEG);
    CHECK_EQ(pc_format_from_ext("a.JFIF"), PC_FMT_JPEG);
    CHECK_EQ(pc_format_from_ext("a.jpg"), PC_FMT_JPEG);
    CHECK_EQ(pc_format_from_ext("scan.TIFF"), PC_FMT_TIFF);
    CHECK_EQ(pc_format_from_ext("scan.tif"), PC_FMT_TIFF);
    CHECK_EQ(pc_format_from_ext("x.Psd"), PC_FMT_PSD);
    CHECK_EQ(pc_format_from_ext("x.bmp"), PC_FMT_BMP);
    CHECK_EQ(pc_format_from_ext("x.dib"), PC_FMT_BMP);
    CHECK_EQ(pc_format_from_ext("x.gif"), PC_FMT_GIF);
    CHECK_EQ(pc_format_from_ext("x.png"), PC_FMT_PNG);
    CHECK_EQ(pc_format_from_ext("x.tga"), PC_FMT_TGA);
    CHECK_EQ(pc_format_from_ext("x.vda"), PC_FMT_TGA);
    CHECK_EQ(pc_format_from_ext("x.txt"), PC_FMT_UNKNOWN);
    CHECK_EQ(pc_format_from_ext("dir.png\\file"), PC_FMT_UNKNOWN);
    CHECK_EQ(pc_format_from_ext("noext"), PC_FMT_UNKNOWN);
    CHECK_EQ(pc_format_from_ext(NULL), PC_FMT_UNKNOWN);
    CHECK_EQ(pc_format_detect((const u8 *)"8BPS\0\1", 6), PC_FMT_PSD);
    CHECK_EQ(pc_format_detect((const u8 *)"GIF87a", 6), PC_FMT_GIF);
    CHECK_EQ(pc_format_detect((const u8 *)"MM\0*", 4), PC_FMT_TIFF);
    CHECK_EQ(pc_format_detect((const u8 *)"II*\0", 4), PC_FMT_TIFF);
    CHECK_EQ(pc_format_detect((const u8 *)"\xFF\xD8\xFF\xE0", 4), PC_FMT_JPEG);
    CHECK_EQ(pc_format_detect((const u8 *)"BMxx", 4), PC_FMT_UNKNOWN);
    CHECK_EQ(pc_format_detect((const u8 *)"hello", 5), PC_FMT_UNKNOWN);
    CHECK_EQ(pc_format_detect(NULL, 0), PC_FMT_UNKNOWN);
    CHECK(!strcmp(pc_format_name(PC_FMT_PSD), "Photoshop (*.PSD)"));
    CHECK(!strcmp(pc_format_name(PC_FMT_GIF), "CompuServe GIF (*.GIF)"));
    CHECK(!strcmp(pc_format_ext(PC_FMT_JPEG), "jpg"));
    CHECK(pc_format_can_layers(PC_FMT_PSD));
    CHECK(!pc_format_can_layers(PC_FMT_PNG));
    {
        char err[128] = "";
        CHECK(pc_load_mem((const u8 *)"garbage!", 8, "x.xyz", err, sizeof err) == NULL);
        CHECK(err[0] != 0);
        CHECK(pc_load_mem((const u8 *)"garbage!", 8, "x.psd", err, sizeof err) == NULL);
        CHECK(pc_load_mem((const u8 *)"", 0, "x.png", err, sizeof err) == NULL);
        /* signature wins over a wrong extension */
        {
            PcDoc *d = mk_doc(5, 5, PC_MODE_RGB, 0, 0), *r;
            u8 *buf;
            size_t n;
            pc_save_mem(d, PC_FMT_PNG, NULL, &buf, &n, err, sizeof err);
            r = pc_load_mem(buf, n, "misnamed.jpg", err, sizeof err);
            CHECK(r != NULL);
            if (r) { CHECK_EQ(r->format, PC_FMT_PNG); pc_doc_free(r); }
            free(buf);
            pc_doc_free(d);
        }
    }
}

/* ---------------- fuzzing ---------------- */

static int g_fuzz_loaded;

static void try_load(const u8 *p, size_t n, const char *name)
{
    char err[160];
    PcDoc *d;
    err[0] = 0;
    d = pc_load_mem(p, n, name, err, sizeof err);
    if (d) {
        CHECK(d->s.w > 0 && d->s.h > 0 && d->s.nlayers >= 1);
        g_fuzz_loaded++;
        pc_doc_free(d);
    } else {
        CHECK(err[0] != 0);
    }
}

static void fuzz_one(const u8 *orig, size_t n, const char *name, int mutations)
{
    const char *scale = getenv("PC_TEST_FUZZ_SCALE");   /* e.g. 20 for a longer soak */
    u8 *buf = (u8 *)malloc(n + 1);
    size_t len, step = n > 3000 ? n / 1500 : 1;
    int m, k;
    if (scale && atoi(scale) > 1) mutations *= atoi(scale);
    for (len = 0; len < n; len += step) {
        u8 *t = (u8 *)malloc(len ? len : 1);          /* exact size so ASan catches overreads */
        memcpy(t, orig, len);
        try_load(t, len, name);
        free(t);
    }
    for (m = 0; m < mutations; m++) {
        int nm = 1 + (int)(rng() % 4);
        memcpy(buf, orig, n);
        for (k = 0; k < nm; k++) {
            size_t at = rng() % n;
            switch (rng() % 4) {
            case 0: buf[at] = (u8)rng(); break;
            case 1: buf[at] ^= (u8)(1u << (rng() % 8)); break;
            case 2: buf[at] = (rng() & 1) ? 0xFF : 0x00; break;
            default: buf[at] = (u8)(buf[at] + 1 - (int)(rng() % 3)); break;
            }
        }
        try_load(buf, n, name);
    }
    free(buf);
}

static void test_fuzz(void)
{
    PcSaveOpts o;
    PcDoc *d = mk_doc(13, 7, PC_MODE_RGB, 1, 0), *dl = mk_layered(PC_MODE_RGB), *dg = mk_doc(9, 5, PC_MODE_GRAY, 1, 0);
    struct { int fmt; int opt; const PcDoc *doc; const char *name; } cases[] = {
        { PC_FMT_PSD, 0, NULL, "f.psd" }, { PC_FMT_PSD, 0, NULL, "f.psd" },
        { PC_FMT_PNG, 0, NULL, "f.png" }, { PC_FMT_JPEG, 0, NULL, "f.jpg" }, { PC_FMT_GIF, 0, NULL, "f.gif" },
        { PC_FMT_GIF, 1, NULL, "f.gif" }, { PC_FMT_BMP, 0, NULL, "f.bmp" }, { PC_FMT_BMP, 1, NULL, "f.bmp" },
        { PC_FMT_TIFF, 0, NULL, "f.tif" }, { PC_FMT_TIFF, 1, NULL, "f.tif" }, { PC_FMT_TIFF, 2, NULL, "f.tif" },
        { PC_FMT_TIFF, 1, NULL, "f.tif" }, { PC_FMT_TGA, 0, NULL, "f.tga" }, { PC_FMT_TGA, 1, NULL, "f.tga" },
        { PC_FMT_PNG, 0, NULL, "f.png" },
    };
    unsigned i;
    char err[128];
    cases[0].doc = d; cases[1].doc = dl;
    for (i = 2; i < sizeof cases / sizeof cases[0]; i++) cases[i].doc = d;
    cases[11].doc = dg; cases[14].doc = dg;
    rng_state = 0xC0FFEEu;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        u8 *buf;
        size_t n;
        pc_save_opts_default(&o);
        o.tiff_compress = cases[i].opt;
        o.gif_interlace = cases[i].opt;
        o.bmp_depth = cases[i].opt ? 32 : 24;
        o.tga_rle = cases[i].opt;
        if (pc_save_mem(cases[i].doc, cases[i].fmt, &o, &buf, &n, err, sizeof err)) { CHECK(0); continue; }
        fuzz_one(buf, n, cases[i].name, 2500);
        free(buf);
    }
    /* hand-made inputs: RLE8 / RLE4 BMP, 16-bit and OS/2 BMP, hand-built PSD with ZIP layers */
    {
        static const u8 rle8[] = {
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0,
            40, 0, 0, 0, 6, 0, 0, 0, 3, 0, 0, 0, 1, 0, 8, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 255, 255, 255, 0,
            3, 1, 0, 3, 0, 1, 0, 0, 0, 0, 2, 1, 0, 2, 1, 0, 1, 1, 0, 0, 6, 1, 0, 1
        };
        static const u8 rle4[] = {
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0,
            40, 0, 0, 0, 6, 0, 0, 0, 2, 0, 0, 0, 1, 0, 4, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 255, 255, 255, 0,
            6, 0x01, 0, 0, 0, 4, 0x10, 0x01, 0, 0, 0, 1
        };
        static const u8 os2[] = {
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0,
            12, 0, 0, 0, 2, 0, 2, 0, 1, 0, 1, 0,
            0, 0, 0, 255, 255, 255,
            0x40, 0, 0, 0, 0x80, 0, 0, 0
        };
        PcDoc *r = pc_load_mem(rle8, sizeof rle8, NULL, err, sizeof err);
        CHECK(r != NULL);
        if (r) {
            const u8 *p = r->s.layers[0]->pix->px;
            /* bottom row (stored first): 1 1 1 0 1 0 -> white white white black white black */
            CHECK_EQ(p[(2 * 6 + 0) * 4], 255); CHECK_EQ(p[(2 * 6 + 3) * 4], 0); CHECK_EQ(p[(2 * 6 + 4) * 4], 255);
            CHECK_EQ(p[(0 * 6 + 5) * 4], 255);
            CHECK_EQ(p[(1 * 6 + 2) * 4], 0); CHECK_EQ(p[(1 * 6 + 3) * 4], 255);   /* after the delta */
            CHECK_EQ(r->s.mode, PC_MODE_GRAY);
            pc_doc_free(r);
        }
        r = pc_load_mem(rle4, sizeof rle4, NULL, err, sizeof err);
        CHECK(r != NULL);
        if (r) {
            const u8 *p = r->s.layers[0]->pix->px;
            CHECK_EQ(p[(1 * 6 + 0) * 4], 0); CHECK_EQ(p[(1 * 6 + 1) * 4], 255);
            CHECK_EQ(p[(0 * 6 + 0) * 4], 255); CHECK_EQ(p[(0 * 6 + 1) * 4], 0);
            pc_doc_free(r);
        }
        r = pc_load_mem(os2, sizeof os2, NULL, err, sizeof err);
        CHECK(r != NULL);
        if (r) {
            const u8 *p = r->s.layers[0]->pix->px;
            /* top row = second stored row (0x80): white, black; bottom row (0x40): black, white */
            CHECK_EQ(p[0], 255); CHECK_EQ(p[4], 0); CHECK_EQ(p[8], 0); CHECK_EQ(p[12], 255);
            pc_doc_free(r);
        }
        fuzz_one(rle8, sizeof rle8, "f.bmp", 3000);
        fuzz_one(rle4, sizeof rle4, "f.bmp", 3000);
        fuzz_one(os2, sizeof os2, "f.bmp", 2000);
    }
    {
        HLayer L[2];
        B b;
        memset(L, 0, sizeof L);
        L[0].top = -1; L[0].left = -1; L[0].bottom = 4; L[0].right = 5; L[0].nch = 4;
        L[0].ids[0] = -1; L[0].ids[1] = 0; L[0].ids[2] = 1; L[0].ids[3] = 2; L[0].fill[0] = 200; L[0].comp = 3; L[0].name = "z"; L[0].opacity = 255;
        L[1] = L[0]; L[1].comp = 2; L[1].section = 0; L[1].name = "zz";
        b = build_psd(6, 5, 3, 8, 3, L, 2, 0, NULL);
        fuzz_one(b.p, b.n, NULL, 3000);
        free(b.p);
        b = build_psd(6, 5, 3, 16, 4, L, 2, 1, NULL);
        fuzz_one(b.p, b.n, NULL, 3000);
        free(b.p);
    }
    pc_doc_free(d); pc_doc_free(dl); pc_doc_free(dg);
    fprintf(stderr, "  fuzz: %d mutated/truncated inputs still loaded as documents\n", g_fuzz_loaded);
}

/* external files (e.g. psd-tools test files): PC_TEST_FILES=<dir with a list.txt> */
static void test_external(void)
{
    const char *dir = getenv("PC_TEST_FILES");
    char list[512], line[512];
    FILE *lf;
    if (!dir || !*dir) return;
    snprintf(list, sizeof list, "%s/list.txt", dir);
    lf = fopen(list, "r");
    if (!lf) return;
    while (fgets(line, sizeof line, lf)) {
        char path[1100], err[160] = "";
        FILE *f;
        long n;
        u8 *p;
        PcDoc *d;
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        snprintf(path, sizeof path, "%s/%s", dir, line);
        f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
        p = (u8 *)malloc((size_t)n + 1);
        if (fread(p, 1, (size_t)n, f) != (size_t)n) n = 0;
        fclose(f);
        d = pc_load_mem(p, (size_t)n, line, err, sizeof err);
        if (d) {
            int i;
            fprintf(stderr, "  ext %-40s %dx%d mode %d dpi %.0f layers %d ch %d:", line, d->s.w, d->s.h, d->s.mode, d->s.dpi, d->s.nlayers, d->s.nchannels);
            for (i = 0; i < d->s.nlayers && i < 6; i++) fprintf(stderr, " [%s%s %s %d]", d->s.layers[i]->visible ? "" : "-", d->s.layers[i]->name, pc_blend_name(d->s.layers[i]->blend), d->s.layers[i]->opacity);
            fprintf(stderr, "%s\n", d->s.nlayers > 6 ? " ..." : "");
            pc_doc_free(d);
        } else {
            fprintf(stderr, "  ext %-40s FAILED: %s\n", line, err);
        }
        if (getenv("PC_TEST_FILES_FUZZ")) fuzz_one(p, (size_t)n, line, 300);
        free(p);
    }
    fclose(lf);
}

/* ---------------- PSD adjustment layers ---------------- */

static PcDoc *adj_ramp(int mode)
{
    PcDoc *d = pc_doc_new(20, 12, mode, 72, 0, 0);
    u8 *p;
    int x, y;
    if (!d) return NULL;
    p = pc_layer_pixels_w(d->s.layers[0]);
    for (y = 0; y < 12; y++)
        for (x = 0; x < 20; x++, p += 4) {
            p[0] = (u8)(x * 13); p[1] = (u8)(y * 21); p[2] = (u8)((x * 7 + y * 11) & 255); p[3] = 255;
            if (mode == PC_MODE_GRAY) p[1] = p[2] = p[0];
        }
    return d;
}

static void adj_params(PcAdjust *a, int t, int variant)
{
    pc_adjust_default(a, t);
    switch (t) {
    case PC_ADJ_BRIGHTNESS_CONTRAST: a->brightness = -35; a->contrast = 60; break;
    case PC_ADJ_LEVELS:
        a->levels.in_black[0] = 12; a->levels.in_white[0] = 230; a->levels.gamma[0] = 1.25f;
        a->levels.out_black[1] = 20; a->levels.out_white[2] = 200; a->levels.gamma[3] = 0.6f;
        if (variant) { a->levels.in_black[1] = 40; a->levels.gamma[1] = 2.0f; }
        break;
    case PC_ADJ_CURVES:
        a->curves.c[0].n = 3; a->curves.c[0].x[1] = 90; a->curves.c[0].y[1] = 140; a->curves.c[0].x[2] = 255; a->curves.c[0].y[2] = 255;
        a->curves.c[2].n = 4; a->curves.c[2].x[0] = 10; a->curves.c[2].y[0] = 0; a->curves.c[2].x[1] = 64; a->curves.c[2].y[1] = 40;
        a->curves.c[2].x[2] = 190; a->curves.c[2].y[2] = 220; a->curves.c[2].x[3] = 250; a->curves.c[2].y[3] = 255;
        break;
    case PC_ADJ_COLOR_BALANCE: a->balance.v[0][1] = -20; a->balance.v[1][0] = 45; a->balance.v[2][2] = 30; a->balance.preserve_luminosity = variant; break;
    case PC_ADJ_HUE_SAT:
        if (variant) { a->huesat.colorize = 1; a->huesat.hue[0] = 250; a->huesat.sat[0] = 60; a->huesat.light[0] = -10; }
        else { a->huesat.hue[0] = -40; a->huesat.sat[0] = 30; a->huesat.light[0] = 5; a->huesat.hue[2] = 20; a->huesat.sat[5] = -50; a->huesat.light[6] = 15; }
        break;
    case PC_ADJ_CHANNEL_MIXER:
        a->mixer[0][0] = 60; a->mixer[0][1] = 30; a->mixer[0][2] = 10; a->mixer[1][3] = -12; a->mixer[2][0] = 150;
        if (variant) { a->mixer_mono = 1; memcpy(a->mixer[1], a->mixer[0], sizeof a->mixer[0]); memcpy(a->mixer[2], a->mixer[0], sizeof a->mixer[0]); }
        break;
    case PC_ADJ_THRESHOLD: a->threshold = 97; break;
    case PC_ADJ_POSTERIZE: a->posterize = 5; break;
    }
}

static int adj_same(const PcAdjust *a, const PcAdjust *b, int gray)
{
    int k, ch;
    if (a->type != b->type) return 0;
    switch (a->type) {
    case PC_ADJ_BRIGHTNESS_CONTRAST: return a->brightness == b->brightness && a->contrast == b->contrast;
    case PC_ADJ_LEVELS:
        for (ch = 0; ch < (gray ? 2 : 4); ch++)
            if (a->levels.in_black[ch] != b->levels.in_black[ch] || a->levels.in_white[ch] != b->levels.in_white[ch] ||
                a->levels.out_black[ch] != b->levels.out_black[ch] || a->levels.out_white[ch] != b->levels.out_white[ch] ||
                (int)(a->levels.gamma[ch] * 100 + 0.5f) != (int)(b->levels.gamma[ch] * 100 + 0.5f)) return 0;
        return 1;
    case PC_ADJ_CURVES:
        for (ch = 0; ch < 4; ch++) {
            if (a->curves.c[ch].n != b->curves.c[ch].n) return 0;
            for (k = 0; k < a->curves.c[ch].n; k++)
                if (a->curves.c[ch].x[k] != b->curves.c[ch].x[k] || a->curves.c[ch].y[k] != b->curves.c[ch].y[k]) return 0;
        }
        return 1;
    case PC_ADJ_COLOR_BALANCE: return !memcmp(&a->balance, &b->balance, sizeof a->balance);
    case PC_ADJ_HUE_SAT:
        if (a->huesat.colorize != b->huesat.colorize) return 0;
        for (k = a->huesat.colorize ? 0 : 0; k < (a->huesat.colorize ? 1 : 7); k++)
            if (a->huesat.hue[k] != b->huesat.hue[k] || a->huesat.sat[k] != b->huesat.sat[k] || a->huesat.light[k] != b->huesat.light[k]) return 0;
        return 1;
    case PC_ADJ_CHANNEL_MIXER: return a->mixer_mono == b->mixer_mono && !memcmp(a->mixer, b->mixer, sizeof a->mixer);
    case PC_ADJ_THRESHOLD: return a->threshold == b->threshold;
    case PC_ADJ_POSTERIZE: return a->posterize == b->posterize;
    }
    return 1;
}

static void test_psd_adjust(void)
{
    int t, variant, mode;
    for (mode = 0; mode < 2; mode++)
    for (t = 0; t < PC_ADJ_COUNT; t++)
        for (variant = 0; variant < 2; variant++) {
            PcDoc *d = adj_ramp(mode), *r = NULL;
            PcAdjust a;
            PcLayer *l;
            u8 *buf = NULL;
            size_t n = 0, k;
            char err[128] = "", name[48];
            if (mode && variant) { pc_doc_free(d); continue; }
            adj_params(&a, t, variant);
            if (mode) {
                /* Grayscale documents have one channel: the dialogs offer only the composite. */
                PcAdjust id;
                pc_adjust_default(&id, t);
                for (k = 1; k < 4; k++) {
                    a.levels.in_black[k] = 0; a.levels.in_white[k] = 255; a.levels.gamma[k] = 1.0f;
                    a.levels.out_black[k] = 0; a.levels.out_white[k] = 255;
                    a.curves.c[k] = id.curves.c[k];
                }
            }
            l = d ? pc_doc_add_adjustment(d, &a, NULL, 0) : NULL;
            if (!l) { CHECK(0); pc_doc_free(d); continue; }
            for (k = 0; k < 20 * 12; k++) if (k % 20 >= 14) l->mask->px[k] = (u8)(k % 20 == 14 ? 128 : 0);
            l->opacity = variant ? 255 : 210;
            l->blend = variant ? PC_BLEND_NORMAL : PC_BLEND_SCREEN;
            if (variant) { pc_doc_add_layer(d, NULL); d->s.layers[2]->visible = 0; d->s.active = 1; }
            CHECK_EQ(pc_psd_save(d, &buf, &n, err, sizeof err), 0);
            snprintf(name, sizeof name, "adjust_%d%s%s.psd", t, variant ? "b" : "", mode ? "_gray" : "");
            dump(name, buf, n);
            r = buf ? pc_psd_load(buf, n, err, sizeof err) : NULL;
            CHECK(r != NULL);
            if (r) {
                CHECK_EQ(r->s.nlayers, d->s.nlayers);
                if (r->s.nlayers >= 2) {
                    PcLayer *q = r->s.layers[1];
                    CHECK_EQ(q->kind, PC_LAYER_ADJUST);
                    CHECK(q->pix == NULL);
                    CHECK(adj_same(&l->adj, &q->adj, mode));
                    CHECK(!strcmp(q->name, l->name));
                    CHECK_EQ(q->opacity, l->opacity);
                    CHECK_EQ(q->blend, l->blend);
                    CHECK(q->mask && !memcmp(q->mask->px, l->mask->px, 20 * 12));
                    CHECK(!q->is_background);
                    CHECK_EQ(cmp_comp(d, r, NULL), 0);
                }
                pc_doc_free(r);
            }
            /* stb reads the merged image: the adjustment is baked into it */
            if (buf && !mode) {
                int w, h, c;
                unsigned char *px = stbi_load_from_memory(buf, (int)n, &w, &h, &c, 4);
                PcBuf *cc = pc_doc_composite(d);
                CHECK(px != NULL);
                if (px && cc) {
                    int bad = 0;
                    for (k = 0; k < 20 * 12 * 4; k++) if (k % 4 != 3 && px[k] != cc->px[k]) bad++;
                    CHECK_EQ(bad, 0);
                }
                free(px);
                pc_buf_unref(cc);
            }
            if (buf && t == PC_ADJ_HUE_SAT && !variant && !mode) fuzz_one(buf, n, name, 300);
            free(buf);
            pc_doc_free(d);
        }
}

int test_io(void)
{
    test_detect();
    test_formats();
    test_psd_roundtrip();
    test_psd_handbuilt();
    test_psd_adjust();
    test_tiff_handbuilt();
    test_fuzz();
    test_external();
    return 0;
}
