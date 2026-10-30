/*
 * PhotoCraft Classic updater - signed update manifest.  Portable C: this
 * file is compiled into PCUpdate.exe (with src/rt/pcrt.c as the C library)
 * and into the native unit tests (host libc).  See manifest.h and
 * docs/updater.md for the format.
 */
#include "pcrt.h"
#include "manifest.h"

#include <bearssl_hash.h>
#include <monocypher/optional/monocypher-ed25519.h>

/* ------------------------------------------------------------------ */
/* small helpers                                                       */

static void set_err(char *err, size_t errlen, const char *fmt, int line, const char *what)
{
    if (!err || !errlen) return;
    pc_snprintf(err, errlen, fmt, line, what ? what : "");
}

static int is_digit(int c) { return c >= '0' && c <= '9'; }
static int is_lower_alnum(int c) { return (c >= 'a' && c <= 'z') || is_digit(c); }

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int pc_hex_decode(const char *s, size_t slen, unsigned char *out, size_t outmax)
{
    size_t i;
    if (!s || !out || (slen & 1) || slen / 2 > outmax) return -1;
    for (i = 0; i < slen; i += 2) {
        int hi = hexval((unsigned char)s[i]), lo = hexval((unsigned char)s[i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i / 2] = (unsigned char)(hi << 4 | lo);
    }
    return (int)(slen / 2);
}

void pc_hex_encode(const unsigned char *in, size_t n, char *out)
{
    static const char dig[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        out[2 * i] = dig[in[i] >> 4];
        out[2 * i + 1] = dig[in[i] & 15];
    }
    out[2 * n] = 0;
}

static int b64val(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Strict RFC 4648 base64: length multiple of 4, '=' padding only at the
 * end, unused bits of the last symbol must be zero (canonical form). */
int pc_base64_decode(const char *s, size_t slen, unsigned char *out, size_t outmax)
{
    size_t i, n = 0;
    if (!s || !out || slen == 0 || (slen & 3)) return -1;
    for (i = 0; i < slen; i += 4) {
        int v[4], k, pad = 0;
        unsigned long w;
        for (k = 0; k < 4; k++) {
            int c = (unsigned char)s[i + k];
            if (c == '=') {
                if (i + 4 != slen || k < 2) return -1;
                v[k] = 0; pad++;
            } else {
                if (pad) return -1;      /* data after '=' */
                v[k] = b64val(c);
                if (v[k] < 0) return -1;
            }
        }
        w = ((unsigned long)v[0] << 18) | ((unsigned long)v[1] << 12) | ((unsigned long)v[2] << 6) | (unsigned long)v[3];
        if (pad == 2 && (w & 0xFFFF)) return -1;
        if (pad == 1 && (w & 0xFF)) return -1;
        if (n + 3 - pad > outmax) return -1;
        out[n++] = (unsigned char)(w >> 16);
        if (pad < 2) out[n++] = (unsigned char)(w >> 8);
        if (pad < 1) out[n++] = (unsigned char)w;
    }
    return (int)n;
}

void pc_sha256(const void *data, size_t len, unsigned char out[32])
{
    br_sha256_context c;
    br_sha256_init(&c);
    br_sha256_update(&c, data, len);
    br_sha256_out(&c, out);
}

long pc_days_from_civil(int y, int m, int d)
{
    static const unsigned char mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int leap, era;
    long yoe, doy, doe;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1) return -1;
    leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    if (d > mdays[m - 1] + (m == 2 && leap)) return -1;
    /* H. Hinnant's days_from_civil, shifted to the 0000-01-01 epoch. */
    y -= m <= 2;
    era = y / 400;
    yoe = y - era * 400;
    doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long)era * 146097L + doe - 719468L + 719528L;
}

/* ------------------------------------------------------------------ */
/* versions                                                            */

static int parse_num(const char **pp, unsigned max, unsigned *out)
{
    const char *p = *pp;
    unsigned long v = 0;
    int nd = 0;
    if (!is_digit((unsigned char)*p)) return -1;
    if (p[0] == '0' && is_digit((unsigned char)p[1])) return -1;   /* no leading zeros */
    while (is_digit((unsigned char)*p)) {
        if (++nd > 5) return -1;
        v = v * 10 + (unsigned long)(*p - '0');
        p++;
    }
    if (v > max) return -1;
    *out = (unsigned)v;
    *pp = p;
    return 0;
}

int pc_version_parse(const char *s, pc_version *v)
{
    const char *p = s;
    size_t i, n, idstart;
    if (!s || !v) return -1;
    memset(v, 0, sizeof *v);
    if (parse_num(&p, 255, &v->major) || *p++ != '.') return -1;
    if (parse_num(&p, 255, &v->minor) || *p++ != '.') return -1;
    if (parse_num(&p, 65535, &v->patch)) return -1;
    if (*p == 0) return 0;
    if (*p++ != '-') return -1;
    /* pre-release: dot-separated non-empty identifiers of [0-9A-Za-z-] */
    n = strlen(p);
    if (n == 0 || n >= sizeof v->pre) return -1;
    idstart = 0;
    for (i = 0; i <= n; i++) {
        int c = (unsigned char)p[i];
        if (c == '.' || c == 0) {
            size_t len = i - idstart, k;
            int numeric = 1;
            if (len == 0) return -1;
            for (k = idstart; k < i; k++) if (!is_digit((unsigned char)p[k])) numeric = 0;
            if (numeric && len > 1 && p[idstart] == '0') return -1;
            idstart = i + 1;
        } else if (!(is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-')) {
            return -1;
        }
    }
    memcpy(v->pre, p, n + 1);
    return 0;
}

static size_t ident_len(const char *s)
{
    size_t n = 0;
    while (s[n] && s[n] != '.') n++;
    return n;
}

/* semver 11.4: compare dot-separated identifiers. */
static int cmp_pre(const char *a, const char *b)
{
    if (!*a && !*b) return 0;
    if (!*a) return 1;      /* release > pre-release */
    if (!*b) return -1;
    for (;;) {
        size_t la = ident_len(a), lb = ident_len(b), k;
        int na = 1, nb = 1, r;
        for (k = 0; k < la; k++) if (!is_digit((unsigned char)a[k])) na = 0;
        for (k = 0; k < lb; k++) if (!is_digit((unsigned char)b[k])) nb = 0;
        if (na && nb) {
            if (la != lb) return la < lb ? -1 : 1;   /* no leading zeros: longer is bigger */
            r = memcmp(a, b, la);
        } else if (na) {
            return -1;
        } else if (nb) {
            return 1;
        } else {
            r = memcmp(a, b, la < lb ? la : lb);
            if (!r && la != lb) r = la < lb ? -1 : 1;
        }
        if (r) return r < 0 ? -1 : 1;
        a += la; b += lb;
        if (!*a && !*b) return 0;
        if (!*a) return -1;  /* fewer identifiers = lower precedence */
        if (!*b) return 1;
        a++; b++;
    }
}

int pc_version_cmp(const pc_version *a, const pc_version *b)
{
    if (a->major != b->major) return a->major < b->major ? -1 : 1;
    if (a->minor != b->minor) return a->minor < b->minor ? -1 : 1;
    if (a->patch != b->patch) return a->patch < b->patch ? -1 : 1;
    return cmp_pre(a->pre, b->pre);
}

int pc_version_cmp_str(const char *a, const char *b)
{
    pc_version va, vb;
    if (pc_version_parse(a, &va) || pc_version_parse(b, &vb)) return -2;
    return pc_version_cmp(&va, &vb);
}

/* ------------------------------------------------------------------ */
/* manifest                                                            */

const char *pc_manifest_strerror(int code)
{
    switch (code) {
    case PC_MF_OK: return "OK";
    case PC_MF_ERR_ARG: return "internal error (bad argument)";
    case PC_MF_ERR_SIZE: return "manifest is empty or too large";
    case PC_MF_ERR_NOSIG: return "manifest is not signed";
    case PC_MF_ERR_SIGFORMAT: return "manifest signature is malformed";
    case PC_MF_ERR_BADSIG: return "manifest signature is not valid";
    case PC_MF_ERR_SYNTAX: return "manifest syntax error";
    case PC_MF_ERR_HEADER: return "not an update manifest (bad header)";
    case PC_MF_ERR_FIELD: return "manifest field is malformed";
    case PC_MF_ERR_DUP: return "manifest field repeated";
    case PC_MF_ERR_MISSING: return "manifest field missing";
    case PC_MF_ERR_PRODUCT: return "manifest is for a different product";
    default: return "unknown manifest error";
    }
}

int pc_manifest_split(const char *buf, size_t len, size_t *body_len, unsigned char sig[64])
{
    size_t pos = 0, end, vlen;
    if (!buf || !body_len || !sig) return PC_MF_ERR_ARG;
    if (len == 0 || len > PC_MANIFEST_MAX_BYTES) return PC_MF_ERR_SIZE;
    /* Find the first line that starts with "sig ". */
    for (;;) {
        const char *nl;
        if (pos + 4 <= len && !memcmp(buf + pos, "sig ", 4)) break;
        nl = memchr(buf + pos, '\n', len - pos);
        if (!nl) return PC_MF_ERR_NOSIG;
        pos = (size_t)(nl - buf) + 1;
        if (pos >= len) return PC_MF_ERR_NOSIG;
    }
    if (pos == 0) return PC_MF_ERR_NOSIG;   /* nothing signed */
    /* The signature line must be the last one: value, optional CR, optional LF, EOF. */
    end = len;
    if (end > pos && buf[end - 1] == '\n') end--;
    if (end > pos && buf[end - 1] == '\r') end--;
    vlen = end - (pos + 4);
    if (memchr(buf + pos + 4, '\n', vlen) || memchr(buf + pos + 4, '\r', vlen)) return PC_MF_ERR_NOSIG;
    if (vlen != 88 || pc_base64_decode(buf + pos + 4, vlen, sig, 64) != 64) return PC_MF_ERR_SIGFORMAT;
    *body_len = pos;
    return PC_MF_OK;
}

enum { F_PRODUCT = 1, F_CHANNEL = 2, F_VERSION = 4, F_SEQUENCE = 8, F_RELEASED = 16,
       F_URL = 32, F_SIZE = 64, F_SHA = 128 };

static int parse_ulong(const char *s, size_t n, unsigned long max, unsigned long *out)
{
    unsigned long v = 0;
    size_t i;
    if (n == 0 || n > 10) return -1;
    if (s[0] == '0' && n > 1) return -1;
    for (i = 0; i < n; i++) {
        unsigned d;
        if (!is_digit((unsigned char)s[i])) return -1;
        d = (unsigned)(s[i] - '0');
        if (v > (max - d) / 10) return -1;
        v = v * 10 + d;
    }
    *out = v;
    return 0;
}

static int copy_field(char *dst, size_t dstsz, const char *v, size_t n)
{
    if (n == 0 || n >= dstsz) return -1;
    memcpy(dst, v, n);
    dst[n] = 0;
    return 0;
}

static int valid_url(const char *v, size_t n)
{
    size_t i;
    if (n <= 8 || n >= PC_MANIFEST_URL_MAX || memcmp(v, "https://", 8)) return 0;
    if (v[8] == '/' || v[8] == '?' || v[8] == '#' || v[8] == ':' || v[8] == '@') return 0;
    for (i = 0; i < n; i++) {
        int c = (unsigned char)v[i];
        if (c <= 0x20 || c >= 0x7F || c == '\\' || c == '"') return 0;
    }
    return 1;
}

int pc_manifest_parse_body(const char *body, size_t len, pc_manifest *m, char *err, size_t errlen)
{
    size_t pos = 0, notes_len = 0;
    int line = 0, seen = 0, nnotes = 0;
    if (err && errlen) err[0] = 0;
    if (!body || !m) return PC_MF_ERR_ARG;
    memset(m, 0, sizeof *m);
    m->key_index = -1;
    if (len == 0 || len > PC_MANIFEST_MAX_BYTES) { set_err(err, errlen, "line %d: manifest empty or too large%s", 0, ""); return PC_MF_ERR_SIZE; }
    if (body[len - 1] != '\n') { set_err(err, errlen, "line %d: missing line terminator%s", 1, ""); return PC_MF_ERR_SYNTAX; }

    while (pos < len) {
        const char *ln = body + pos, *nl, *val;
        size_t ll, klen, vlen, i;
        char key[32];
        nl = memchr(ln, '\n', len - pos);
        if (!nl) return PC_MF_ERR_SYNTAX;   /* cannot happen: body ends with '\n' */
        ll = (size_t)(nl - ln);
        pos += ll + 1;
        line++;
        if (ll && ln[ll - 1] == '\r') ll--;
        if (ll > PC_MANIFEST_MAX_LINE) { set_err(err, errlen, "line %d: too long%s", line, ""); return PC_MF_ERR_SYNTAX; }
        /* character set: printable ASCII; bytes >= 0x80 are checked per field below */
        for (i = 0; i < ll; i++) {
            int c = (unsigned char)ln[i];
            if (c < 0x20 || c == 0x7F) { set_err(err, errlen, "line %d: control character%s", line, ""); return PC_MF_ERR_SYNTAX; }
        }
        if (line == 1) {
            if (ll != 10 || memcmp(ln, "PCUPDATE 1", 10)) { set_err(err, errlen, "line %d: expected 'PCUPDATE 1'%s", line, ""); return PC_MF_ERR_HEADER; }
            m->format = 1;
            continue;
        }
        /* key [SP value] */
        for (klen = 0; klen < ll && ln[klen] != ' '; klen++) {
            if (!(is_lower_alnum((unsigned char)ln[klen]) || ln[klen] == '_')) {
                set_err(err, errlen, "line %d: bad key%s", line, "");
                return PC_MF_ERR_SYNTAX;
            }
        }
        if (klen == 0 || klen >= sizeof key) { set_err(err, errlen, "line %d: bad key%s", line, ""); return PC_MF_ERR_SYNTAX; }
        memcpy(key, ln, klen);
        key[klen] = 0;
        val = ln + klen + (klen < ll ? 1 : 0);
        vlen = ll - (size_t)(val - ln);

        if (!strcmp(key, "notes")) {
            /* free text; bytes >= 0x80 allowed (shown as Windows-1252) */
            size_t need = vlen + (nnotes ? 2 : 0);
            if (++nnotes > PC_MANIFEST_NOTES_LINES || notes_len + need >= sizeof m->notes) {
                set_err(err, errlen, "line %d: release notes too long%s", line, "");
                return PC_MF_ERR_FIELD;
            }
            if (nnotes > 1) { m->notes[notes_len++] = '\r'; m->notes[notes_len++] = '\n'; }
            memcpy(m->notes + notes_len, val, vlen);
            notes_len += vlen;
            m->notes[notes_len] = 0;
            continue;
        }
        for (i = 0; i < vlen; i++) {
            if ((unsigned char)val[i] >= 0x80) { set_err(err, errlen, "line %d: non-ASCII value for '%s'", line, key); return PC_MF_ERR_FIELD; }
        }
        if (vlen == 0 || val[0] == ' ' || val[vlen - 1] == ' ') {
            set_err(err, errlen, "line %d: empty or padded value for '%s'", line, key);
            return PC_MF_ERR_FIELD;
        }

#define ONCE(flag) do { if (seen & (flag)) { set_err(err, errlen, "line %d: '%s' repeated", line, key); return PC_MF_ERR_DUP; } seen |= (flag); } while (0)
#define BAD() do { set_err(err, errlen, "line %d: bad value for '%s'", line, key); return PC_MF_ERR_FIELD; } while (0)
        if (!strcmp(key, "product")) {
            ONCE(F_PRODUCT);
            if (copy_field(m->product, sizeof m->product, val, vlen)) BAD();
        } else if (!strcmp(key, "channel")) {
            ONCE(F_CHANNEL);
            for (i = 0; i < vlen; i++) if (!(is_lower_alnum((unsigned char)val[i]) || val[i] == '-')) BAD();
            if (copy_field(m->channel, sizeof m->channel, val, vlen)) BAD();
        } else if (!strcmp(key, "version")) {
            ONCE(F_VERSION);
            if (copy_field(m->version_str, sizeof m->version_str, val, vlen)) BAD();
            if (pc_version_parse(m->version_str, &m->version)) BAD();
        } else if (!strcmp(key, "sequence")) {
            ONCE(F_SEQUENCE);
            if (parse_ulong(val, vlen, 0xFFFFFFFFUL, &m->sequence)) BAD();
        } else if (!strcmp(key, "released")) {
            int y, mo, d;
            ONCE(F_RELEASED);
            if (vlen != 10 || val[4] != '-' || val[7] != '-') BAD();
            for (i = 0; i < 10; i++) if (i != 4 && i != 7 && !is_digit((unsigned char)val[i])) BAD();
            y = (val[0] - '0') * 1000 + (val[1] - '0') * 100 + (val[2] - '0') * 10 + (val[3] - '0');
            mo = (val[5] - '0') * 10 + (val[6] - '0');
            d = (val[8] - '0') * 10 + (val[9] - '0');
            if (pc_days_from_civil(y, mo, d) < 0) BAD();
            memcpy(m->released, val, 10);
            m->released[10] = 0;
        } else if (!strcmp(key, "msi_url")) {
            ONCE(F_URL);
            if (!valid_url(val, vlen)) BAD();
            if (copy_field(m->msi_url, sizeof m->msi_url, val, vlen)) BAD();
        } else if (!strcmp(key, "msi_size")) {
            ONCE(F_SIZE);
            if (parse_ulong(val, vlen, PC_MANIFEST_MSI_MAX, &m->msi_size) || m->msi_size == 0) BAD();
        } else if (!strcmp(key, "msi_sha256")) {
            ONCE(F_SHA);
            if (vlen != 64 || pc_hex_decode(val, vlen, m->msi_sha256, 32) != 32) BAD();
        } else if (!strcmp(key, "sig")) {
            set_err(err, errlen, "line %d: unexpected '%s'", line, key);
            return PC_MF_ERR_SYNTAX;
        } else {
            /* Unknown keys are allowed (and covered by the signature) so that
             * newer manifests can add fields older updaters ignore. */
        }
#undef ONCE
#undef BAD
    }
    if (m->format != 1) { set_err(err, errlen, "line %d: expected 'PCUPDATE 1'%s", 1, ""); return PC_MF_ERR_HEADER; }
    {
        static const struct { int flag; const char *name; } req[] = {
            { F_PRODUCT, "product" }, { F_CHANNEL, "channel" }, { F_VERSION, "version" },
            { F_SEQUENCE, "sequence" }, { F_RELEASED, "released" }, { F_URL, "msi_url" },
            { F_SIZE, "msi_size" }, { F_SHA, "msi_sha256" }
        };
        size_t k;
        for (k = 0; k < sizeof req / sizeof req[0]; k++) {
            if (!(seen & req[k].flag)) {
                set_err(err, errlen, "line %d: required field '%s' missing", line, req[k].name);
                return PC_MF_ERR_MISSING;
            }
        }
    }
    return PC_MF_OK;
}

int pc_manifest_load(const char *buf, size_t len, const unsigned char (*keys)[32], int nkeys,
                     pc_manifest *m, char *err, size_t errlen)
{
    unsigned char sig[64];
    size_t body_len = 0;
    int rc, k, ok = -1;
    if (err && errlen) err[0] = 0;
    if (!buf || !m || !keys || nkeys <= 0) return PC_MF_ERR_ARG;
    memset(m, 0, sizeof *m);
    rc = pc_manifest_split(buf, len, &body_len, sig);
    if (rc) goto fail;
    for (k = 0; k < nkeys; k++) {
        if (crypto_ed25519_check(sig, keys[k], (const unsigned char *)buf, body_len) == 0) { ok = k; break; }
    }
    if (ok < 0) { rc = PC_MF_ERR_BADSIG; goto fail; }
    rc = pc_manifest_parse_body(buf, body_len, m, err, errlen);
    if (rc) { memset(m, 0, sizeof *m); return rc; }
    m->signed_len = body_len;
    m->key_index = ok;
    if (strcmp(m->product, PC_PRODUCT_ID)) { rc = PC_MF_ERR_PRODUCT; memset(m, 0, sizeof *m); goto fail; }
    return PC_MF_OK;
fail:
    if (err && errlen) pc_strlcpy(err, pc_manifest_strerror(rc), errlen);
    return rc;
}

int pc_manifest_policy(const pc_manifest *m, const char *installed_version,
                       unsigned long last_seen_sequence, const char *channel)
{
    pc_version inst;
    if (!m || !installed_version) return PC_POLICY_BAD_INSTALLED;
    if (channel && *channel && strcmp(channel, m->channel)) return PC_POLICY_WRONG_CHANNEL;
    if (m->sequence < last_seen_sequence) return PC_POLICY_REPLAY;
    if (pc_version_parse(installed_version, &inst)) return PC_POLICY_BAD_INSTALLED;
    return pc_version_cmp(&m->version, &inst) > 0 ? PC_POLICY_UPDATE : PC_POLICY_CURRENT;
}
