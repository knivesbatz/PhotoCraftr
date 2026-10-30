/*
 * PhotoCraft Classic - minimal C runtime interface.
 *
 * On Windows the program is linked WITHOUT msvcrt.dll (it is missing from
 * retail Windows 95), so src/rt/pcrt.c provides the handful of libc
 * functions the code base uses, implemented on KERNEL32 only.  On the
 * native (Linux) test build the real libc is used instead.
 *
 * Code everywhere may use the standard names declared in <string.h>,
 * <stdlib.h> and <math.h>.  Formatting goes through pc_snprintf, which
 * behaves identically on both builds (no locale, no wide chars).
 */
#ifndef PCRT_H
#define PCRT_H

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define PC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define PC_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

/* printf-style formatting: %d %i %u %x %X %c %s %p %f %g(as %f) %%,
 * flags '-' '0', width, .precision, 'l' modifier. Always NUL terminates. */
int pc_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int pc_snprintf(char *buf, size_t n, const char *fmt, ...);

/* Locale-free helpers (avoid msvcrt's ctype tables). */
int pc_tolower(int c);
int pc_toupper(int c);
int pc_isdigit(int c);
int pc_isspace(int c);
int pc_stricmp(const char *a, const char *b);
double pc_atof(const char *s);
long pc_atol(const char *s);
void pc_strlcpy(char *dst, const char *src, size_t n);
void pc_strlcat(char *dst, const char *src, size_t n);

/* Overflow-checked allocation for width*height*bpp style sizes.
 * Returns NULL when the product overflows or exceeds 512 MB. */
void *pc_alloc_checked(size_t a, size_t b, size_t c, int zero);

#endif
