/*
 * Portable helpers shared by the Windows and native builds: formatting,
 * ctype replacements, number parsing.  No libc dependencies beyond the
 * mem/str functions.
 */
#include "pcrt.h"

int pc_tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int pc_toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int pc_isdigit(int c) { return c >= '0' && c <= '9'; }
int pc_isspace(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

int pc_stricmp(const char *a, const char *b)
{
    for (;;) {
        int ca = pc_tolower((unsigned char)*a++), cb = pc_tolower((unsigned char)*b++);
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
}

void pc_strlcpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    if (!n) return;
    while (i + 1 < n && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void pc_strlcat(char *dst, const char *src, size_t n)
{
    size_t l = strlen(dst);
    if (l < n) pc_strlcpy(dst + l, src, n - l);
}

long pc_atol(const char *s)
{
    long v = 0; int neg = 0;
    while (pc_isspace((unsigned char)*s)) s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    while (pc_isdigit((unsigned char)*s)) { v = v * 10 + (*s - '0'); s++; if (v > 1000000000L) break; }
    return neg ? -v : v;
}

double pc_atof(const char *s)
{
    double v = 0, scale = 1; int neg = 0;
    while (pc_isspace((unsigned char)*s)) s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    while (pc_isdigit((unsigned char)*s)) { v = v * 10 + (*s - '0'); s++; }
    if (*s == '.' || *s == ',') {
        s++;
        while (pc_isdigit((unsigned char)*s)) { scale /= 10; v += (*s - '0') * scale; s++; }
    }
    return neg ? -v : v;
}

void *pc_alloc_checked(size_t a, size_t b, size_t c, int zero)
{
    const size_t lim = (size_t)512 * 1024 * 1024;
    size_t n;
    if (!a || !b || !c) return NULL;
    if (a > lim / b) return NULL;
    n = a * b;
    if (n > lim / c) return NULL;
    n *= c;
    return zero ? calloc(1, n) : malloc(n);
}

/* ---- formatting ---- */

typedef struct { char *buf; size_t n, len; } Out;

static void put(Out *o, char c)
{
    if (o->len + 1 < o->n) o->buf[o->len] = c;
    o->len++;
}

/* Right-aligned output; zero padding goes after a leading minus sign. */
static void put_padded(Out *o, const char *s, size_t sl, int width, char pad)
{
    int k;
    if (pad == '0' && sl && *s == '-') { put(o, '-'); s++; sl--; width--; }
    for (k = (int)sl; k < width; k++) put(o, pad);
    while (sl--) put(o, *s++);
}

static size_t utoa_base(unsigned long v, unsigned base, int upper, char *tmp)
{
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char r[24]; size_t n = 0, i;
    do { r[n++] = dig[v % base]; v /= base; } while (v);
    for (i = 0; i < n; i++) tmp[i] = r[n - 1 - i];
    return n;
}

int pc_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    Out o; o.buf = buf; o.n = n; o.len = 0;
    while (*fmt) {
        int left = 0, width = 0, prec = -1, lng = 0; char pad = ' ';
        char tmp[64]; const char *s = tmp; size_t sl = 0;
        if (*fmt != '%') { put(&o, *fmt++); continue; }
        fmt++;
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') pad = '0';
            else break;
        }
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else while (pc_isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (pc_isdigit((unsigned char)*fmt)) prec = prec * 10 + (*fmt++ - '0');
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z') { if (*fmt == 'l' || *fmt == 'z') lng = 1; fmt++; }
        if (left) pad = ' ';
        switch (*fmt) {
        case 'd': case 'i': {
            long v = lng ? va_arg(ap, long) : va_arg(ap, int);
            unsigned long u = v < 0 ? (unsigned long)(-(v + 1)) + 1 : (unsigned long)v;
            size_t k = 0;
            if (v < 0) tmp[k++] = '-';
            sl = k + utoa_base(u, 10, 0, tmp + k);
            break;
        }
        case 'u': case 'x': case 'X': {
            unsigned long v = lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            sl = utoa_base(v, *fmt == 'u' ? 10 : 16, *fmt == 'X', tmp);
            break;
        }
        case 'p': {
            unsigned long v = (unsigned long)(size_t)va_arg(ap, void *);
            sl = utoa_base(v, 16, 1, tmp);
            break;
        }
        case 'c': tmp[0] = (char)va_arg(ap, int); sl = 1; break;
        case 's':
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            sl = strlen(s);
            if (prec >= 0 && (size_t)prec < sl) sl = (size_t)prec;
            break;
        case 'f': case 'g': case 'e': {
            double v = va_arg(ap, double);
            size_t k = 0; unsigned long ip; double frac; int p = prec < 0 ? (*fmt == 'g' ? 2 : 6) : prec, i;
            double r = 0.5;
            if (p > 9) p = 9;
            if (v != v) { s = "nan"; sl = 3; break; }
            if (v < 0) { tmp[k++] = '-'; v = -v; }
            for (i = 0; i < p; i++) r /= 10;
            v += r;
            if (v > 4.0e9) v = 4.0e9;
            ip = (unsigned long)v;
            frac = v - (double)ip;
            k += utoa_base(ip, 10, 0, tmp + k);
            if (p > 0) {
                tmp[k++] = '.';
                for (i = 0; i < p; i++) { int d; frac *= 10; d = (int)frac; if (d > 9) d = 9; tmp[k++] = (char)('0' + d); frac -= d; }
                if (*fmt == 'g') { while (tmp[k - 1] == '0') k--; if (tmp[k - 1] == '.') k--; }
            }
            sl = k;
            break;
        }
        case '%': tmp[0] = '%'; sl = 1; break;
        case 0: goto done;
        default: tmp[0] = '%'; tmp[1] = *fmt; sl = 2; break;
        }
        fmt++;
        if (left) {
            int k; size_t q;
            for (q = 0; q < sl; q++) put(&o, s[q]);
            for (k = (int)sl; k < width; k++) put(&o, ' ');
        } else {
            put_padded(&o, s, sl, width, pad);
        }
    }
done:
    if (n) buf[o.len < n ? o.len : n - 1] = 0;
    return (int)o.len;
}

int pc_snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = pc_vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}
