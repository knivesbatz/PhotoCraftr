/*
 * Format dispatcher: names, extensions, signature detection, load/save.
 */
#include "io/ioint.h"

void pc_save_opts_default(PcSaveOpts *o)
{
    memset(o, 0, sizeof *o);
    o->depth = 0;               /* the format's default for the document */
    o->green6 = 0;
    pc_quant_defaults(&o->q);   /* Adaptive 256, Diffusion, transparency, magenta key off */
    o->interlace = 0;
    o->tiff_compress = 0;
    o->tga_rle = 0;
    o->jpeg_quality = 8;        /* "High" */
}

/* ---------------- the 1998 output rule (see pcio.h) ---------------- */

int pc_format_depths(int fmt, int *dd)
{
    static const int bmp[] = { 8, 16, 24 }, tga[] = { 8, 16, 24, 32 }, gif[] = { 8 }, png[] = { 8, 24, 32 },
                     tif[] = { 8, 24 }, jpg[] = { 8, 24 }, psd[] = { 8, 24, 32 };
    const int *t;
    int n, i;
    switch (fmt) {
    case PC_FMT_BMP: t = bmp; n = 3; break;
    case PC_FMT_TGA: t = tga; n = 4; break;
    case PC_FMT_GIF: t = gif; n = 1; break;
    case PC_FMT_PNG: t = png; n = 3; break;
    case PC_FMT_TIFF: t = tif; n = 2; break;
    case PC_FMT_JPEG: t = jpg; n = 2; break;
    case PC_FMT_PSD: t = psd; n = 3; break;
    default: return 0;
    }
    for (i = 0; i < n; i++) dd[i] = t[i];
    return n;
}

int pc_format_depth_ok(int fmt, int depth)
{
    int dd[4], n, i;
    if (depth == 0) return fmt > PC_FMT_UNKNOWN && fmt < PC_FMT_COUNT;
    n = pc_format_depths(fmt, dd);
    for (i = 0; i < n; i++) if (dd[i] == depth) return 1;
    return 0;
}

int pc_format_default_depth(int fmt, int gray, int transparent)
{
    switch (fmt) {
    case PC_FMT_GIF: return 8;
    case PC_FMT_TGA: case PC_FMT_PNG: return transparent ? 32 : gray ? 8 : 24;
    case PC_FMT_PSD: return gray ? 8 : transparent ? 32 : 24;
    default: return gray ? 8 : 24;
    }
}

int pc_doc_has_transparency(const PcDoc *d)
{
    PcBuf *c = pc_doc_composite(d);
    int r;
    if (!c) return 0;
    r = pcio_has_alpha(c->px, (size_t)c->w * c->h);
    pc_buf_unref(c);
    return r;
}

PcBuf *pc_save_preview(const PcDoc *d, int fmt, const PcSaveOpts *o, char *err, size_t errn)
{
    PcFlat f;
    size_t np, i;
    if (err && errn) err[0] = 0;
    if (!d || d->s.nlayers <= 0) { pcio_seterr(err, errn, "The document has no layers."); return NULL; }
    if (fmt == PC_FMT_JPEG) {
        /* lossy: encode and decode, so the preview shows the real artefacts */
        u8 *buf = NULL;
        size_t n = 0;
        PcDoc *r;
        PcBuf *b;
        if (pc_save_mem(d, fmt, o, &buf, &n, err, errn)) return NULL;
        r = pc_load_mem(buf, n, "preview.jpg", err, errn);
        free(buf);
        if (!r) return NULL;
        b = pc_doc_composite(r);
        pc_doc_free(r);
        return b;
    }
    if (fmt == PC_FMT_PSD) return pcio_composite(d, err, errn);
    if (pcio_flat(d, fmt, o, &f, err, errn)) return NULL;
    np = (size_t)f.c->w * f.c->h;
    if (f.ix.idx) {
        /* (the per-entry alpha is 255 unless the file stores transparency) */
        for (i = 0; i < np; i++) {
            u8 *p = f.c->px + i * 4;
            int e = f.ix.idx[i];
            p[0] = f.ix.pal[e * 3]; p[1] = f.ix.pal[e * 3 + 1]; p[2] = f.ix.pal[e * 3 + 2];
            p[3] = f.ix.alpha[e];
        }
    }
    {
        PcBuf *b = f.c;
        f.c = NULL;
        pcio_flat_free(&f);
        return b;
    }
}

static const char *const fmt_names[PC_FMT_COUNT] = {
    "", "Photoshop (*.PSD)", "BMP (*.BMP)", "CompuServe GIF (*.GIF)", "JPEG (*.JPG)",
    "PNG (*.PNG)", "TIFF (*.TIF)", "Targa (*.TGA)"
};
static const char *const fmt_exts[PC_FMT_COUNT] = { "", "psd", "bmp", "gif", "jpg", "png", "tif", "tga" };

const char *pc_format_name(int fmt) { return (fmt > 0 && fmt < PC_FMT_COUNT) ? fmt_names[fmt] : "Unknown"; }
const char *pc_format_ext(int fmt) { return (fmt > 0 && fmt < PC_FMT_COUNT) ? fmt_exts[fmt] : ""; }
int pc_format_can_layers(int fmt) { return fmt == PC_FMT_PSD; }

int pc_format_from_ext(const char *filename)
{
    static const struct { const char *ext; int fmt; } map[] = {
        { "psd", PC_FMT_PSD }, { "pdd", PC_FMT_PSD },
        { "bmp", PC_FMT_BMP }, { "dib", PC_FMT_BMP }, { "rle", PC_FMT_BMP },
        { "gif", PC_FMT_GIF },
        { "jpg", PC_FMT_JPEG }, { "jpeg", PC_FMT_JPEG }, { "jpe", PC_FMT_JPEG }, { "jfif", PC_FMT_JPEG }, { "jfi", PC_FMT_JPEG },
        { "png", PC_FMT_PNG },
        { "tif", PC_FMT_TIFF }, { "tiff", PC_FMT_TIFF },
        { "tga", PC_FMT_TGA }, { "vda", PC_FMT_TGA }, { "icb", PC_FMT_TGA }, { "vst", PC_FMT_TGA }, { "tpic", PC_FMT_TGA },
    };
    const char *dot, *p;
    unsigned i;
    if (!filename) return PC_FMT_UNKNOWN;
    dot = strrchr(filename, '.');
    if (!dot) return PC_FMT_UNKNOWN;
    for (p = dot + 1; *p; p++) if (*p == '/' || *p == '\\') return PC_FMT_UNKNOWN;   /* dot in a directory name */
    for (i = 0; i < sizeof map / sizeof map[0]; i++)
        if (!pc_stricmp(dot + 1, map[i].ext)) return map[i].fmt;
    return PC_FMT_UNKNOWN;
}

int pc_format_detect(const u8 *p, size_t n)
{
    if (!p) return PC_FMT_UNKNOWN;
    if (n >= 4 && !memcmp(p, "8BPS", 4)) return PC_FMT_PSD;
    if (n >= 8 && !memcmp(p, "\x89PNG\r\n\x1a\n", 8)) return PC_FMT_PNG;
    if (n >= 3 && p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF) return PC_FMT_JPEG;
    if (n >= 6 && (!memcmp(p, "GIF87a", 6) || !memcmp(p, "GIF89a", 6))) return PC_FMT_GIF;
    if (n >= 4 && ((p[0] == 'I' && p[1] == 'I' && p[2] == 42 && p[3] == 0) || (p[0] == 'M' && p[1] == 'M' && p[2] == 0 && p[3] == 42)))
        return PC_FMT_TIFF;
    if (n >= 18 && p[0] == 'B' && p[1] == 'M') {
        u32 hs = get_u32le(p + 14);
        if (hs == 12 || hs == 16 || hs == 40 || hs == 52 || hs == 56 || hs == 64 || hs == 108 || hs == 124) return PC_FMT_BMP;
    }
    return PC_FMT_UNKNOWN;      /* Targa has no signature */
}

PcDoc *pc_load_mem(const u8 *data, size_t n, const char *filename, char *err, size_t errn)
{
    int fmt = pc_format_detect(data, n);
    if (err && errn) err[0] = 0;
    if (!data || n == 0) { pcio_seterr(err, errn, "The file is empty."); return NULL; }
    if (fmt == PC_FMT_UNKNOWN) fmt = pc_format_from_ext(filename);
    switch (fmt) {
    case PC_FMT_PSD: return pc_psd_load(data, n, err, errn);
    case PC_FMT_BMP: return pc_bmp_load(data, n, err, errn);
    case PC_FMT_GIF: return pc_gif_load(data, n, err, errn);
    case PC_FMT_JPEG: return pc_jpeg_load(data, n, err, errn);
    case PC_FMT_PNG: return pc_png_load(data, n, err, errn);
    case PC_FMT_TIFF: return pc_tiff_load(data, n, err, errn);
    case PC_FMT_TGA: return pc_tga_load(data, n, err, errn);
    default:
        pcio_seterr(err, errn, "The file format is not recognised.");
        return NULL;
    }
}

int pc_save_mem(const PcDoc *d, int fmt, const PcSaveOpts *o, u8 **out, size_t *outn, char *err, size_t errn)
{
    PcSaveOpts def;
    if (err && errn) err[0] = 0;
    if (!out || !outn) return -1;
    *out = NULL; *outn = 0;
    if (!d || d->s.nlayers <= 0) { pcio_seterr(err, errn, "The document has no layers."); return -1; }
    if (!o) { pc_save_opts_default(&def); o = &def; }
    /* the 1998 output rule holds for every caller (dialogs, Actions, code) */
    if (pcio_check_depth(fmt, o->depth, err, errn)) return -1;
    switch (fmt) {
    case PC_FMT_PSD: return pc_psd_save(d, out, outn, err, errn);
    case PC_FMT_BMP: return pc_bmp_save(d, o, out, outn, err, errn);
    case PC_FMT_GIF: return pc_gif_save(d, o, out, outn, err, errn);
    case PC_FMT_JPEG: return pc_jpeg_save(d, o, out, outn, err, errn);
    case PC_FMT_PNG: return pc_png_save(d, o, out, outn, err, errn);
    case PC_FMT_TIFF: return pc_tiff_save(d, o, out, outn, err, errn);
    case PC_FMT_TGA: return pc_tga_save(d, o, out, outn, err, errn);
    default:
        pcio_seterr(err, errn, "Unknown file format.");
        return -1;
    }
}
