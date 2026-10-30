/*
 * PhotoCraft Classic - CRT replacement for the Windows build.
 *
 * Windows 95 retail does not ship msvcrt.dll, so the executables are linked
 * with -nostdlib and this file supplies everything the code base needs:
 * program entry, heap, mem/str functions, qsort and x87 math.  Only
 * KERNEL32 functions that exist on Windows 95 are used here.
 *
 * Compiled with -fno-builtin -fno-tree-loop-distribute-patterns so GCC
 * cannot turn these loops back into calls to themselves.
 */
#include <windows.h>
#include "pcrt.h"

/* ---------------- entry point ---------------- */

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

static char *skip_program_name(char *p)
{
    if (*p == '"') {
        p++;
        while (*p && *p != '"') p++;
        if (*p == '"') p++;
    } else {
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

void pc_entry(void)
{
    STARTUPINFOA si;
    int show = SW_SHOWDEFAULT, rc;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    GetStartupInfoA(&si);
    if (si.dwFlags & STARTF_USESHOWWINDOW) show = si.wShowWindow;
    rc = WinMain(GetModuleHandleA(NULL), NULL, skip_program_name(GetCommandLineA()), show);
    ExitProcess((UINT)rc);
}

/* ---------------- heap ---------------- */

void *malloc(size_t n)
{
    return HeapAlloc(GetProcessHeap(), 0, n ? n : 1);
}

void *calloc(size_t a, size_t b)
{
    if (b && a > ((size_t)-1) / b) return NULL;
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, a * b ? a * b : 1);
}

void *realloc(void *p, size_t n)
{
    if (!p) return malloc(n);
    if (!n) { HeapFree(GetProcessHeap(), 0, p); return NULL; }
    return HeapReAlloc(GetProcessHeap(), 0, p, n);
}

void free(void *p)
{
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

/* ---------------- memory / strings ---------------- */

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dp = d; const unsigned char *sp = s;
    if (!(((size_t)dp | (size_t)sp) & 3)) {
        while (n >= 4) { *(unsigned int *)dp = *(const unsigned int *)sp; dp += 4; sp += 4; n -= 4; }
    }
    while (n--) *dp++ = *sp++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *dp = d; const unsigned char *sp = s;
    if (dp == sp || !n) return d;
    if (dp < sp || dp >= sp + n) return memcpy(d, s, n);
    dp += n; sp += n;
    while (n--) *--dp = *--sp;
    return d;
}

void *memset(void *d, int c, size_t n)
{
    unsigned char *dp = d;
    unsigned int v = (unsigned char)c;
    v |= v << 8; v |= v << 16;
    while (n && ((size_t)dp & 3)) { *dp++ = (unsigned char)c; n--; }
    while (n >= 4) { *(unsigned int *)dp = v; dp += 4; n -= 4; }
    while (n--) *dp++ = (unsigned char)c;
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++) if (*p == (unsigned char)c) return (void *)p;
    return NULL;
}

size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (!*a) return 0;
    }
    return 0;
}

char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }

char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }

char *strchr(const char *s, int c)
{
    for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return NULL; }
}

char *strrchr(const char *s, int c)
{
    const char *r = NULL;
    for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; }
}

char *strstr(const char *h, const char *n)
{
    size_t nl = strlen(n);
    if (!nl) return (char *)h;
    for (; *h; h++) if (*h == *n && !strncmp(h, n, nl)) return (char *)h;
    return NULL;
}

int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }
int atoi(const char *s) { return (int)pc_atol(s); }

long strtol(const char *s, char **end, int base)
{
    long v = 0; int neg = 0;
    while (pc_isspace((unsigned char)*s)) s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; }
    if (!base) base = 10;
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
    }
    if (end) *end = (char *)s;
    return neg ? -v : v;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = pc_vsnprintf(buf, 0x7fffffff, fmt, ap);
    va_end(ap);
    return r;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = pc_vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) { return pc_vsnprintf(buf, n, fmt, ap); }

/* ---------------- qsort (heap sort: no recursion, no extra memory) ---------------- */

static void swap_bytes(unsigned char *a, unsigned char *b, size_t n)
{
    while (n--) { unsigned char t = *a; *a++ = *b; *b++ = t; }
}

static void sift(unsigned char *base, size_t start, size_t end, size_t sz, int (*cmp)(const void *, const void *))
{
    size_t root = start;
    while (root * 2 + 1 <= end) {
        size_t child = root * 2 + 1, sw = root;
        if (cmp(base + sw * sz, base + child * sz) < 0) sw = child;
        if (child + 1 <= end && cmp(base + sw * sz, base + (child + 1) * sz) < 0) sw = child + 1;
        if (sw == root) return;
        swap_bytes(base + root * sz, base + sw * sz, sz);
        root = sw;
    }
}

void qsort(void *b, size_t n, size_t sz, int (*cmp)(const void *, const void *))
{
    unsigned char *base = b;
    size_t start, end;
    if (n < 2) return;
    start = (n - 2) / 2;
    for (;;) { sift(base, start, n - 1, sz, cmp); if (!start) break; start--; }
    for (end = n - 1; end > 0; end--) {
        swap_bytes(base, base + end * sz, sz);
        sift(base, 0, end - 1, sz, cmp);
    }
}

/* ---------------- x87 math ----------------
 * Every Windows 95 machine has a 486DX/Pentium-class FPU (or the OS
 * emulator), so the transcendental functions map onto x87 instructions. */

double fabs(double x) { return x < 0 ? -x : x; }
double sqrt(double x) { double r; if (x <= 0) return 0; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
double sin(double x) { double r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
double cos(double x) { double r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
double tan(double x) { return sin(x) / cos(x); }
/* GCC fuses sin(x)/cos(x) pairs into sincos() calls. */
void sincos(double x, double *s, double *c) { *s = sin(x); *c = cos(x); }
void sincosf(float x, float *s, float *c) { *s = (float)sin(x); *c = (float)cos(x); }

double atan2(double y, double x)
{
    double r;
    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

double atan(double x) { return atan2(x, 1.0); }
double asin(double x) { return atan2(x, sqrt(1 - x * x)); }
double acos(double x) { return atan2(sqrt(1 - x * x), x); }

static double x87_log2(double x)
{
    double r;
    __asm__("fld1; fxch; fyl2x" : "=t"(r) : "0"(x));
    return r;
}

static double x87_exp2(double x)
{
    double r;
    if (x > 1000) x = 1000;
    if (x < -1000) return 0;
    __asm__(
        "fld %%st(0)\n\t"   /* x x */
        "frndint\n\t"       /* i x */
        "fsubr %%st, %%st(1)\n\t" /* i f */
        "fxch\n\t"          /* f i */
        "f2xm1\n\t"         /* 2^f-1 i */
        "fld1\n\t"
        "faddp\n\t"         /* 2^f i */
        "fscale\n\t"        /* 2^(f+i) i */
        "fstp %%st(1)"
        : "=t"(r) : "0"(x));
    return r;
}

double log(double x) { if (x <= 0) return -1e300; return x87_log2(x) * 0.69314718055994530942; }
double log10(double x) { if (x <= 0) return -1e300; return x87_log2(x) * 0.30102999566398119521; }
double exp(double x) { return x87_exp2(x * 1.44269504088896340736); }

double pow(double x, double y)
{
    double iy;
    if (y == 0) return 1;
    if (x == 0) return y > 0 ? 0 : 1e300;
    if (x > 0) return x87_exp2(y * x87_log2(x));
    iy = (double)(long)y;
    if (iy != y) return 0; /* NaN in libm; callers never rely on it */
    {
        double r = x87_exp2(y * x87_log2(-x));
        return ((long)iy & 1) ? -r : r;
    }
}

double floor(double x)
{
    double t;
    if (x >= 4503599627370496.0 || x <= -4503599627370496.0) return x;
    t = (double)(long long)x;
    return t > x ? t - 1 : t;
}

double ceil(double x) { return -floor(-x); }
double round(double x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }

double fmod(double x, double y)
{
    double r;
    if (y == 0) return 0;
    __asm__("1: fprem\n\t fnstsw %%ax\n\t sahf\n\t jp 1b" : "=t"(r) : "0"(x), "u"(y) : "ax", "cc");
    return r;
}

double ldexp(double x, int e) { return x * x87_exp2((double)e); }

double frexp(double x, int *e)
{
    int k = 0;
    if (x == 0) { *e = 0; return 0; }
    while (fabs(x) >= 1) { x *= 0.5; k++; }
    while (fabs(x) < 0.5) { x *= 2; k--; }
    *e = k;
    return x;
}

float sqrtf(float x) { return (float)sqrt(x); }
float powf(float x, float y) { return (float)pow(x, y); }
float floorf(float x) { return (float)floor(x); }
float ceilf(float x) { return (float)ceil(x); }
float fabsf(float x) { return x < 0 ? -x : x; }
float expf(float x) { return (float)exp(x); }
float logf(float x) { return (float)log(x); }
float sinf(float x) { return (float)sin(x); }
float cosf(float x) { return (float)cos(x); }
float atan2f(float y, float x) { return (float)atan2(y, x); }
float roundf(float x) { return (float)round(x); }
float fmodf(float x, float y) { return (float)fmod(x, y); }
