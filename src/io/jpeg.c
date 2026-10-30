/*
 * JPEG: decoding by stb_image (baseline and progressive, gray/YCbCr/
 * Adobe CMYK); encoding by our own small baseline encoder below (JFIF,
 * 1 component for Grayscale documents, YCbCr with 4:2:0 chroma below
 * quality 90 otherwise, the standard ITU T.81 Annex K tables).  The
 * resolution is read from and written to the JFIF APP0 header.
 *
 * (stb_image_write's encoder always writes three components, so it
 * cannot produce a true grayscale JPEG.)
 *
 * Quality: Photoshop's 0..12 scale maps linearly onto the IJG 1..100
 * scale as  q = 10 + quality * 90 / 12,  so 0 -> 10 (Low), 3 -> 32,
 * 5 -> 47, 8 -> 70 (High), 10 -> 85, 12 -> 100 (Maximum).
 */
#include "io/ioint.h"

static double jfif_dpi(const u8 *data, size_t n)
{
    PcRd r;
    rd_init(&r, data, n);
    rd_skip(&r, 2);
    while (rd_left(&r) >= 4) {
        unsigned m, len;
        const u8 *seg;
        if (rd_u8(&r) != 0xFF) break;
        m = rd_u8(&r);
        while (m == 0xFF && rd_left(&r)) m = rd_u8(&r);     /* fill bytes */
        if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01) continue;
        if (m == 0xDA || m == 0xD9) break;                     /* start of scan / end */
        len = rd_u16be(&r);
        if (len < 2) break;
        seg = rd_bytes(&r, len - 2);
        if (!seg) break;
        if (m == 0xE0 && len - 2 >= 12 && !memcmp(seg, "JFIF\0", 5)) {
            unsigned units = seg[7], xd = get_u16be(seg + 8);
            if (xd && units == 1) return xd;
            if (xd && units == 2) return (double)pcio_uround(xd * 2.54);
            return 72;
        }
    }
    return 72;
}

PcDoc *pc_jpeg_load(const u8 *data, size_t n, char *err, size_t errn)
{
    int w, h, comp;
    u8 *px;
    if (n < 4 || data[0] != 0xFF || data[1] != 0xD8) { pcio_seterr(err, errn, "This is not a JPEG file."); return NULL; }
    if (!pcio_stbi_info(data, n, &w, &h, &comp)) { pcio_seterr(err, errn, "The JPEG file is damaged or unsupported."); return NULL; }
    if (!pcio_dims_ok(w, h)) { pcio_seterr(err, errn, "The image is too large."); return NULL; }
    /* even a blank progressive JPEG needs about a bit per 8x8 block */
    if (!pcio_plausible(w, h, n, 2048)) { pcio_seterr(err, errn, "The JPEG file is truncated or damaged."); return NULL; }
    px = pcio_stbi_load(data, n, &w, &h, &comp, err, errn);
    if (!px) return NULL;
    return pcio_doc_from_buf(pcio_buf_wrap(px, w, h), comp == 1 ? PC_MODE_GRAY : PC_MODE_RGB, jfif_dpi(data, n), PC_FMT_JPEG, err, errn);
}

/* ---------------- baseline encoder ---------------- */

static const u8 zigzag[64] = {      /* natural index -> zigzag position */
    0, 1, 5, 6, 14, 15, 27, 28, 2, 4, 7, 13, 16, 26, 29, 42, 3, 8, 12, 17, 25, 30, 41, 43, 9, 11, 18,
    24, 31, 40, 44, 53, 10, 19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60, 21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};
static const u8 qt_luma[64] = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99
};
static const u8 qt_chroma[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
};
/* Huffman tables: 16 code counts, then the symbol values. */
static const u8 dc_luma_bits[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const u8 dc_chroma_bits[16] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const u8 dc_vals[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const u8 ac_luma_bits[16] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const u8 ac_luma_vals[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa
};
static const u8 ac_chroma_bits[16] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const u8 ac_chroma_vals[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa
};

typedef struct { u16 code[256]; u8 size[256]; } Huff;

typedef struct {
    PcWr *w;
    u32 acc;
    int nacc;
    Huff dc[2], ac[2];
    float fq[2][64];          /* 1 / (quantizer * AAN scale), natural order */
    int pred[3];
} JEnc;

static void huff_build(Huff *h, const u8 *bits, const u8 *vals)
{
    int len, i, k = 0;
    unsigned code = 0;
    memset(h, 0, sizeof *h);
    for (len = 1; len <= 16; len++) {
        for (i = 0; i < bits[len - 1]; i++, k++) { h->code[vals[k]] = (u16)code++; h->size[vals[k]] = (u8)len; }
        code <<= 1;
    }
}

static void put_bits(JEnc *e, unsigned code, int n)
{
    e->acc = (e->acc << n) | (code & ((1u << n) - 1));
    e->nacc += n;
    while (e->nacc >= 8) {
        unsigned b = (e->acc >> (e->nacc - 8)) & 255;
        wr_u8(e->w, b);
        if (b == 0xFF) wr_u8(e->w, 0);       /* byte stuffing */
        e->nacc -= 8;
    }
    e->acc &= (1u << e->nacc) - 1;
}

static int nbits_of(int v) { int n = 0; unsigned t = (unsigned)(v < 0 ? -v : v); while (t) { n++; t >>= 1; } return n; }

/* Forward DCT, the AAN algorithm (as in IJG jfdctflt.c); the output
 * scale factors are folded into the quantizer (JEnc.fq). */
static void fdct8(float *d, int step)
{
    float t0 = d[0] + d[7 * step], t7 = d[0] - d[7 * step], t1 = d[step] + d[6 * step], t6 = d[step] - d[6 * step];
    float t2 = d[2 * step] + d[5 * step], t5 = d[2 * step] - d[5 * step], t3 = d[3 * step] + d[4 * step], t4 = d[3 * step] - d[4 * step];
    float t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2, z1, z2, z3, z4, z5, z11, z13;
    d[0] = t10 + t11;
    d[4 * step] = t10 - t11;
    z1 = (t12 + t13) * 0.707106781f;
    d[2 * step] = t13 + z1;
    d[6 * step] = t13 - z1;
    t10 = t4 + t5; t11 = t5 + t6; t12 = t6 + t7;
    z5 = (t10 - t12) * 0.382683433f;
    z2 = 0.541196100f * t10 + z5;
    z4 = 1.306562965f * t12 + z5;
    z3 = t11 * 0.707106781f;
    z11 = t7 + z3; z13 = t7 - z3;
    d[5 * step] = z13 + z2;
    d[3 * step] = z13 - z2;
    d[step] = z11 + z4;
    d[7 * step] = z11 - z4;
}

static void encode_block(JEnc *e, float *blk, int comp, int table)
{
    int q[64], i, run = 0, last = 0, v, n;
    const Huff *dc = &e->dc[table], *ac = &e->ac[table];
    for (i = 0; i < 8; i++) fdct8(blk + i * 8, 1);
    for (i = 0; i < 8; i++) fdct8(blk + i, 8);
    for (i = 0; i < 64; i++) {
        float f = blk[i] * e->fq[table][i];
        v = (int)(f < 0 ? f - 0.5f : f + 0.5f);
        q[zigzag[i]] = PC_CLAMP(v, -1023, 1023);
    }
    v = PC_CLAMP(q[0] - e->pred[comp], -2047, 2047);
    e->pred[comp] = q[0];
    n = nbits_of(v);
    put_bits(e, dc->code[n], dc->size[n]);
    if (n) put_bits(e, (unsigned)(v < 0 ? v - 1 : v), n);
    for (i = 63; i > 0 && !q[i]; i--) {}
    last = i;
    for (i = 1; i <= last; i++) {
        if (!q[i]) { run++; continue; }
        while (run >= 16) { put_bits(e, ac->code[0xF0], ac->size[0xF0]); run -= 16; }
        n = nbits_of(q[i]);
        put_bits(e, ac->code[(run << 4) | n], ac->size[(run << 4) | n]);
        put_bits(e, (unsigned)(q[i] < 0 ? q[i] - 1 : q[i]), n);
        run = 0;
    }
    if (last < 63) put_bits(e, ac->code[0], ac->size[0]);       /* EOB */
}

static void put_dht(PcWr *w, int cls_id, const u8 *bits, const u8 *vals, int nvals)
{
    wr_u8(w, (unsigned)cls_id);
    wr_bytes(w, bits, 16);
    wr_bytes(w, vals, (size_t)nvals);
}

/* px: ch (1 or 3) interleaved 8-bit samples. */
static int jpeg_encode(PcWr *w, const u8 *px, int W, int H, int ch, int quality, unsigned dpi)
{
    static const float aan[8] = { 1.0f, 1.387039845f, 1.306562965f, 1.175875602f, 1.0f, 0.785694958f, 0.541196100f, 0.275899379f };
    JEnc *e = (JEnc *)calloc(1, sizeof *e);
    int sub = ch == 3 && quality <= 90, scale, t, i, k, x, y, mcu = sub ? 16 : 8;
    u8 qt[2][64];
    if (!e) return -1;
    e->w = w;
    quality = PC_CLAMP(quality, 1, 100);
    scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
    for (t = 0; t < 2; t++)
        for (i = 0; i < 64; i++) {
            int v = ((t ? qt_chroma[i] : qt_luma[i]) * scale + 50) / 100;
            qt[t][i] = (u8)PC_CLAMP(v, 1, 255);
            e->fq[t][i] = 1.0f / (qt[t][i] * aan[i >> 3] * aan[i & 7] * 8.0f);
        }
    huff_build(&e->dc[0], dc_luma_bits, dc_vals);
    huff_build(&e->ac[0], ac_luma_bits, ac_luma_vals);
    huff_build(&e->dc[1], dc_chroma_bits, dc_vals);
    huff_build(&e->ac[1], ac_chroma_bits, ac_chroma_vals);

    /* SOI, JFIF APP0 with the resolution */
    wr_u16be(w, 0xFFD8);
    wr_u16be(w, 0xFFE0); wr_u16be(w, 16); wr_bytes(w, "JFIF\0", 5); wr_u16be(w, 0x0101);
    wr_u8(w, 1); wr_u16be(w, dpi); wr_u16be(w, dpi); wr_u16be(w, 0);
    /* quantization tables (zigzag order) */
    for (t = 0; t < (ch == 3 ? 2 : 1); t++) {
        u8 z[64];
        for (i = 0; i < 64; i++) z[zigzag[i]] = qt[t][i];
        wr_u16be(w, 0xFFDB); wr_u16be(w, 67); wr_u8(w, (unsigned)t); wr_bytes(w, z, 64);
    }
    /* frame header */
    wr_u16be(w, 0xFFC0); wr_u16be(w, (unsigned)(8 + 3 * ch)); wr_u8(w, 8);
    wr_u16be(w, (unsigned)H); wr_u16be(w, (unsigned)W); wr_u8(w, (unsigned)ch);
    for (k = 0; k < ch; k++) { wr_u8(w, (unsigned)(k + 1)); wr_u8(w, k == 0 && sub ? 0x22 : 0x11); wr_u8(w, k ? 1 : 0); }
    /* Huffman tables */
    wr_u16be(w, 0xFFC4); wr_u16be(w, (unsigned)(2 + (ch == 3 ? 2 : 1) * (17 + 12 + 17 + 162)));
    put_dht(w, 0x00, dc_luma_bits, dc_vals, 12);
    put_dht(w, 0x10, ac_luma_bits, ac_luma_vals, 162);
    if (ch == 3) { put_dht(w, 0x01, dc_chroma_bits, dc_vals, 12); put_dht(w, 0x11, ac_chroma_bits, ac_chroma_vals, 162); }
    /* scan header */
    wr_u16be(w, 0xFFDA); wr_u16be(w, (unsigned)(6 + 2 * ch)); wr_u8(w, (unsigned)ch);
    for (k = 0; k < ch; k++) { wr_u8(w, (unsigned)(k + 1)); wr_u8(w, k ? 0x11 : 0x00); }
    wr_u8(w, 0); wr_u8(w, 63); wr_u8(w, 0);

    for (y = 0; y < H; y += mcu)
        for (x = 0; x < W; x += mcu) {
            float Y[256], U[256], V[256], blk[64];
            int r, c, bx, by;
            for (r = 0; r < mcu; r++)
                for (c = 0; c < mcu; c++) {
                    const u8 *p = px + ((size_t)PC_MIN(y + r, H - 1) * W + PC_MIN(x + c, W - 1)) * ch;
                    int o = r * mcu + c;
                    if (ch == 1) { Y[o] = p[0] - 128.0f; continue; }
                    Y[o] = 0.29900f * p[0] + 0.58700f * p[1] + 0.11400f * p[2] - 128;
                    U[o] = -0.16874f * p[0] - 0.33126f * p[1] + 0.50000f * p[2];
                    V[o] = 0.50000f * p[0] - 0.41869f * p[1] - 0.08131f * p[2];
                }
            for (by = 0; by < mcu; by += 8)
                for (bx = 0; bx < mcu; bx += 8) {
                    for (r = 0; r < 8; r++) for (c = 0; c < 8; c++) blk[r * 8 + c] = Y[(by + r) * mcu + bx + c];
                    encode_block(e, blk, 0, 0);
                }
            if (ch == 3) {
                for (k = 0; k < 2; k++) {
                    const float *S = k ? V : U;
                    for (r = 0; r < 8; r++)
                        for (c = 0; c < 8; c++)
                            blk[r * 8 + c] = sub ? (S[2 * r * 16 + 2 * c] + S[2 * r * 16 + 2 * c + 1] + S[(2 * r + 1) * 16 + 2 * c] + S[(2 * r + 1) * 16 + 2 * c + 1]) * 0.25f
                                                 : S[r * 8 + c];
                    encode_block(e, blk, 1 + k, 1);
                }
            }
        }
    put_bits(e, 0x7F, 7);                 /* pad the last byte with ones */
    wr_u16be(w, 0xFFD9);
    free(e);
    return w->err ? -1 : 0;
}

int pc_jpeg_save(const PcDoc *d, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcBuf *c;
    size_t np, i;
    int gray = d->s.mode == PC_MODE_GRAY, ch = gray ? 1 : 3, q, rc;
    int qual = o ? PC_CLAMP(o->jpeg_quality, 0, 12) : 8;
    u8 *px, *p;
    PcWr w;
    memset(&w, 0, sizeof w);
    *out = NULL; *outn = 0;
    if (d->s.w > 65535 || d->s.h > 65535) { pcio_seterr(err, errn, "The image is too large for JPEG."); return -1; }
    c = pcio_composite(d, err, errn);
    if (!c) return -1;
    np = (size_t)c->w * c->h;
    pcio_matte_white(c->px, np);                /* JPEG has no transparency */
    px = (u8 *)pc_alloc_checked(np, (size_t)ch, 1, 0);
    if (!px) { pc_buf_unref(c); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    for (i = 0, p = px; i < np; i++) {
        *p++ = c->px[i * 4];
        if (!gray) { *p++ = c->px[i * 4 + 1]; *p++ = c->px[i * 4 + 2]; }
    }
    q = 10 + qual * 90 / 12;
    rc = jpeg_encode(&w, px, c->w, c->h, ch, q, (unsigned)PC_CLAMP(pcio_uround(pcio_doc_dpi(d)), 1u, 65535u));
    free(px);
    pc_buf_unref(c);
    if (rc) { wr_free(&w); pcio_seterr(err, errn, "Not enough memory to save the file."); return -1; }
    return wr_finish(&w, out, outn, err, errn);
}
