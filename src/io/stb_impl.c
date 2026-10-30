/*
 * The single translation unit that compiles the vendored stb_image /
 * stb_image_write (public domain / MIT, third_party/stb), configured for
 * the runtime-less Windows build: memory buffers only (no stdio), no SIMD,
 * no thread-locals, no assert, our allocator, and our dimension limit.
 *
 * Only the decoders we do not implement ourselves are compiled in
 * (PNG, JPEG, GIF, TGA); PSD, TIFF and BMP have their own codecs.  The
 * rest of src/io talks to stb through the small wrappers at the bottom,
 * declared in ioint.h, so no other file includes the stb headers.
 */
#include "io/ioint.h"
#include <limits.h>

/* stb's decoders do fixed-point arithmetic that can overflow on hostile
 * input (e.g. the JPEG IDCT on garbage coefficients).  Compile them with
 * wrapping signed arithmetic so that is well defined rather than
 * undefined behaviour the optimizer could exploit. */
#if defined(__GNUC__)
#pragma GCC optimize("wrapv")
#endif

/* stb copies zero bytes from/to NULL in a few paths (an empty PNG chunk
 * before any IDAT data); memcpy(NULL, p, 0) is undefined, so guard it. */
static void *stb_memcpy(void *d, const void *s, size_t n) { return n ? memcpy(d, s, n) : d; }
#define memcpy(d, s, n) stb_memcpy(d, s, n)

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_TGA
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_REALLOC(p, newsz) realloc(p, newsz)
#define STBI_FREE(p) free(p)
#define STBI_MAX_DIMENSIONS PC_MAX_DIM
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(sz) malloc(sz)
#define STBIW_REALLOC(p, newsz) realloc(p, newsz)
#define STBIW_FREE(p) free(p)
#define STBIW_MEMMOVE(a, b, sz) memmove(a, b, sz)
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
/* (Diagnostics stay relaxed to the end of the file: unused static stb
 * functions are reported when the translation unit ends.) */

u8 *pcio_stbi_load(const u8 *data, size_t n, int *w, int *h, int *file_comp, char *err, size_t errn)
{
    u8 *px;
    int comp = 0;
    if (!data || n == 0 || n > (size_t)INT_MAX) { pcio_seterr(err, errn, "The file is empty or too large."); return NULL; }
    px = stbi_load_from_memory(data, (int)n, w, h, &comp, 4);
    if (!px) {
        const char *why = stbi_failure_reason();
        pcio_seterr(err, errn, "The file could not be read (%s).", why ? why : "damaged data");
        return NULL;
    }
    if (!pcio_dims_ok(*w, *h)) {
        free(px);
        pcio_seterr(err, errn, "The image is too large.");
        return NULL;
    }
    if (file_comp) *file_comp = comp;
    return px;
}

int pcio_stbi_info(const u8 *data, size_t n, int *w, int *h, int *comp)
{
    if (!data || n == 0 || n > (size_t)INT_MAX) return 0;
    return stbi_info_from_memory(data, (int)n, w, h, comp);
}

long pcio_inflate(u8 *out, size_t outn, const u8 *in, size_t inn, int zlib_header)
{
    int r;
    if (outn > (size_t)INT_MAX || inn > (size_t)INT_MAX) return -1;
    if (zlib_header) r = stbi_zlib_decode_buffer((char *)out, (int)outn, (const char *)in, (int)inn);
    else r = stbi_zlib_decode_noheader_buffer((char *)out, (int)outn, (const char *)in, (int)inn);
    return r < 0 ? -1 : (long)r;
}

static void wr_func(void *ctx, void *data, int size)
{
    if (size > 0) wr_bytes((PcWr *)ctx, data, (size_t)size);
}

int pcio_stbi_write_png(PcWr *w, int width, int height, int comp, const u8 *px)
{
    if (!stbi_write_png_to_func(wr_func, w, width, height, comp, px, width * comp)) return -1;
    return w->err ? -1 : 0;
}

int pcio_stbi_write_tga(PcWr *w, int width, int height, int comp, const u8 *px, int rle)
{
    int ok;
    stbi_write_tga_with_rle = rle ? 1 : 0;
    ok = stbi_write_tga_to_func(wr_func, w, width, height, comp, px);
    stbi_write_tga_with_rle = 1;
    return (!ok || w->err) ? -1 : 0;
}
