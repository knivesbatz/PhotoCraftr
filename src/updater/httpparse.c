/*
 * PhotoCraft Classic updater - portable HTTP helpers.  See httpparse.h.
 * Compiled into PCUpdate.exe and (via tests/test_updater.c) into the
 * native unit tests, where it is fuzzed.
 */
#include "pcrt.h"
#include "httpparse.h"

static int lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int is_digit(int c) { return c >= '0' && c <= '9'; }

static int mem_ieq(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) if (lc((unsigned char)a[i]) != lc((unsigned char)b[i])) return 0;
    return 1;
}

static int host_char(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '-' || c == '.' || c == '_';
}

int pc_parse_ipv4(const char *s, unsigned char out[4])
{
    int part;
    for (part = 0; part < 4; part++) {
        unsigned v = 0, nd = 0;
        while (is_digit((unsigned char)*s)) {
            v = v * 10 + (unsigned)(*s++ - '0');
            if (++nd > 3 || v > 255) return -1;
        }
        if (!nd) return -1;
        out[part] = (unsigned char)v;
        if (part < 3) { if (*s++ != '.') return -1; }
    }
    return *s ? -1 : 0;
}

/* Parse "host[:port]" from s[0..n).  Port defaults to defport. */
static int parse_hostport(const char *s, size_t n, char *host, size_t hostsz, unsigned short *port, unsigned short defport)
{
    size_t hl = 0, i;
    while (hl < n && s[hl] != ':') hl++;
    if (hl == 0 || hl >= hostsz || hl > 253) return -1;
    for (i = 0; i < hl; i++) {
        if (!host_char((unsigned char)s[i])) return -1;
        host[i] = (char)lc((unsigned char)s[i]);
    }
    host[hl] = 0;
    if (host[0] == '.' || host[0] == '-' || host[hl - 1] == '-') return -1;
    if (hl < n) {
        unsigned long p = 0;
        size_t k = hl + 1;
        if (k == n || n - k > 5) return -1;
        for (; k < n; k++) {
            if (!is_digit((unsigned char)s[k])) return -1;
            p = p * 10 + (unsigned long)(s[k] - '0');
        }
        if (p == 0 || p > 65535) return -1;
        *port = (unsigned short)p;
    } else {
        *port = defport;
    }
    return 0;
}

int pc_url_parse_https(const char *url, pc_url *u)
{
    size_t n, auth_end, plen;
    const char *p;
    if (!url || !u) return PC_URL_BAD;
    memset(u, 0, sizeof *u);
    n = strlen(url);
    if (n >= PC_URL_MAX) return PC_URL_BAD;
    if (n < 8 || !mem_ieq(url, "https://", 8)) {
        size_t k = 0;
        while (k < n && k < 16 && url[k] != ':') k++;
        return (k < n && url[k] == ':') ? PC_URL_INSECURE : PC_URL_BAD;
    }
    p = url + 8;
    n -= 8;
    for (auth_end = 0; auth_end < n; auth_end++) {
        char c = p[auth_end];
        if (c == '/' || c == '?' || c == '#') break;
        if (c == '@' || c == '[' || c == ']' || c == '\\') return PC_URL_BAD;
    }
    if (parse_hostport(p, auth_end, u->host, sizeof u->host, &u->port, 443)) return PC_URL_BAD;
    p += auth_end;
    n -= auth_end;
    /* path + query, fragment dropped */
    plen = 0;
    while (plen < n && p[plen] != '#') plen++;
    if (plen == 0) { strcpy(u->path, "/"); return PC_URL_OK; }
    {
        size_t off = 0, i;
        if (p[0] == '?') u->path[off++] = '/';
        if (off + plen >= sizeof u->path) return PC_URL_BAD;
        for (i = 0; i < plen; i++) {
            int c = (unsigned char)p[i];
            if (c <= 0x20 || c >= 0x7F) return PC_URL_BAD;
            u->path[off++] = (char)c;
        }
        u->path[off] = 0;
    }
    return PC_URL_OK;
}

void pc_url_format(const pc_url *u, char *out, size_t outsz)
{
    if (u->port == 443) pc_snprintf(out, outsz, "https://%s%s", u->host, u->path);
    else pc_snprintf(out, outsz, "https://%s:%u%s", u->host, (unsigned)u->port, u->path);
}

int pc_url_resolve(const pc_url *base, const char *loc, char *out, size_t outsz)
{
    pc_url tmp;
    size_t n, i;
    if (!base || !loc || !out || outsz < 16) return PC_URL_BAD;
    out[0] = 0;
    n = strlen(loc);
    if (n == 0 || n >= PC_URL_MAX) return PC_URL_BAD;
    for (i = 0; i < n; i++) {
        int c = (unsigned char)loc[i];
        if (c <= 0x20 || c >= 0x7F) return PC_URL_BAD;
    }
    if (n >= 2 && loc[0] == '/' && loc[1] == '/') {
        /* scheme-relative: inherits https */
        char buf[PC_URL_MAX + 8];
        if (n + 6 >= sizeof buf) return PC_URL_BAD;
        memcpy(buf, "https:", 6);
        memcpy(buf + 6, loc, n + 1);
        if (pc_url_parse_https(buf, &tmp)) return PC_URL_BAD;
    } else if (loc[0] == '/') {
        tmp = *base;
        if (n >= sizeof tmp.path) return PC_URL_BAD;
        memcpy(tmp.path, loc, n + 1);
        { char *h = strchr(tmp.path, '#'); if (h) *h = 0; }
    } else {
        /* absolute URL?  scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ":" */
        size_t k = 0;
        while (k < n && ((loc[k] >= 'a' && loc[k] <= 'z') || (loc[k] >= 'A' && loc[k] <= 'Z') ||
                         (k > 0 && (is_digit((unsigned char)loc[k]) || loc[k] == '+' || loc[k] == '-' || loc[k] == '.'))))
            k++;
        if (k > 0 && k < n && loc[k] == ':') {
            int rc = pc_url_parse_https(loc, &tmp);
            if (rc) return rc;
        } else {
            /* relative path: replace the last segment of the base path */
            size_t dir = 0;
            tmp = *base;
            for (i = 0; tmp.path[i] && tmp.path[i] != '?'; i++) if (tmp.path[i] == '/') dir = i + 1;
            if (dir + n >= sizeof tmp.path) return PC_URL_BAD;
            memcpy(tmp.path + dir, loc, n + 1);
            { char *h = strchr(tmp.path, '#'); if (h) *h = 0; }
        }
    }
    pc_url_format(&tmp, out, outsz);
    if (strlen(out) + 1 >= outsz) return PC_URL_BAD;  /* truncated */
    return PC_URL_OK;
}

size_t pc_http_head_end(const char *buf, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        if (buf[i] != '\n') continue;
        if (i + 1 < len && buf[i + 1] == '\n') return i + 2;
        if (i + 2 < len && buf[i + 1] == '\r' && buf[i + 2] == '\n') return i + 3;
    }
    return 0;
}

static const char *trim(const char *s, size_t *n)
{
    while (*n && (*s == ' ' || *s == '\t')) { s++; (*n)--; }
    while (*n && (s[*n - 1] == ' ' || s[*n - 1] == '\t')) (*n)--;
    return s;
}

int pc_http_parse_head(const char *buf, size_t len, pc_http_head *h)
{
    size_t pos = 0, i;
    int first = 1;
    if (!buf || !h) return -1;
    memset(h, 0, sizeof *h);
    h->content_length = -1;
    if (len == 0 || len > PC_HTTP_HEAD_MAX) return -1;
    if (memchr(buf, 0, len)) return -1;
    while (pos < len) {
        const char *ln = buf + pos, *nl = memchr(ln, '\n', len - pos);
        size_t ll;
        if (!nl) return -1;
        ll = (size_t)(nl - ln);
        pos += ll + 1;
        if (ll && ln[ll - 1] == '\r') ll--;
        if (first) {
            /* HTTP/1.x SP 3DIGIT [SP reason] */
            first = 0;
            if (ll < 12 || memcmp(ln, "HTTP/1.", 7) || !is_digit((unsigned char)ln[7]) || ln[8] != ' ') return -1;
            for (i = 9; i < 12; i++) if (!is_digit((unsigned char)ln[i])) return -1;
            if (ll > 12 && ln[12] != ' ') return -1;
            h->minor_version = ln[7] - '0';
            h->status = (ln[9] - '0') * 100 + (ln[10] - '0') * 10 + (ln[11] - '0');
            if (h->status < 100) return -1;
            continue;
        }
        if (ll == 0) break;                         /* end of head */
        if (ln[0] == ' ' || ln[0] == '\t') return -1; /* obsolete line folding */
        {
            const char *colon = memchr(ln, ':', ll), *v;
            size_t nlen, vlen;
            if (!colon || colon == ln) return -1;
            nlen = (size_t)(colon - ln);
            for (i = 0; i < nlen; i++) {
                int c = (unsigned char)ln[i];
                if (c <= 0x20 || c >= 0x7F) return -1;
            }
            vlen = ll - nlen - 1;
            v = trim(colon + 1, &vlen);
            if (nlen == 14 && mem_ieq(ln, "content-length", 14)) {
                long cl = 0;
                if (vlen == 0 || vlen > 10) return -1;
                for (i = 0; i < vlen; i++) {
                    if (!is_digit((unsigned char)v[i])) return -1;
                    if (cl > (0x7FFFFFFFL - 9) / 10) return -1;
                    cl = cl * 10 + (v[i] - '0');
                }
                if (h->content_length >= 0 && h->content_length != cl) return -1;
                h->content_length = cl;
            } else if (nlen == 17 && mem_ieq(ln, "transfer-encoding", 17)) {
                for (i = 0; i + 7 <= vlen; i++) if (mem_ieq(v + i, "chunked", 7)) h->chunked = 1;
                if (vlen && !h->chunked) h->chunked = 2;  /* some other coding: unsupported */
            } else if (nlen == 8 && mem_ieq(ln, "location", 8)) {
                if (vlen == 0 || vlen >= sizeof h->location || h->location[0]) return -1;
                memcpy(h->location, v, vlen);
                h->location[vlen] = 0;
            }
        }
    }
    return first ? -1 : 0;
}

int pc_http_build_get(char *out, size_t outsz, const pc_url *u, const char *ua)
{
    int n;
    /* HTTP/1.0 keeps responses simple: no chunked transfer coding. */
    if (u->port == 443)
        n = pc_snprintf(out, outsz, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nAccept: */*\r\n"
                        "Accept-Encoding: identity\r\nConnection: close\r\n\r\n", u->path, u->host, ua);
    else
        n = pc_snprintf(out, outsz, "GET %s HTTP/1.0\r\nHost: %s:%u\r\nUser-Agent: %s\r\nAccept: */*\r\n"
                        "Accept-Encoding: identity\r\nConnection: close\r\n\r\n", u->path, u->host, (unsigned)u->port, ua);
    return (n < 0 || (size_t)n >= outsz) ? -1 : n;
}

int pc_http_build_connect(char *out, size_t outsz, const char *host, unsigned short port, const char *ua)
{
    int n = pc_snprintf(out, outsz, "CONNECT %s:%u HTTP/1.0\r\nHost: %s:%u\r\nUser-Agent: %s\r\n\r\n",
                        host, (unsigned)port, host, (unsigned)port, ua);
    return (n < 0 || (size_t)n >= outsz) ? -1 : n;
}

/* Parse one proxy address "[scheme://]host[:port][/]". */
static int parse_proxy_addr(const char *s, size_t n, char *host, size_t hostsz, unsigned short *port)
{
    const char *p;
    s = trim(s, &n);
    p = s;
    if (n >= 7 && mem_ieq(s, "http://", 7)) { s += 7; n -= 7; }
    else {
        size_t k;
        for (k = 0; k + 2 < n; k++) if (s[k] == ':' && s[k + 1] == '/' && s[k + 2] == '/') return -1; /* socks:// etc. */
    }
    (void)p;
    while (n && s[n - 1] == '/') n--;
    if (n == 0) return -1;
    return parse_hostport(s, n, host, hostsz, port, 80);
}

int pc_proxy_parse(const char *value, char *host, size_t hostsz, unsigned short *port)
{
    size_t n;
    const char *p, *end;
    if (!value || !host || !port) return -1;
    n = strlen(value);
    if (n == 0 || n > 1024) return -1;
    if (!memchr(value, '=', n)) return parse_proxy_addr(value, n, host, hostsz, port);
    /* "http=a:1;https=b:2;ftp=c:3" - use the https (IE: "Secure") entry. */
    p = value;
    end = value + n;
    while (p < end) {
        const char *semi = p, *eq;
        while (semi < end && *semi != ';' && *semi != ' ') semi++;
        eq = memchr(p, '=', (size_t)(semi - p));
        if (eq) {
            size_t kl = (size_t)(eq - p);
            const char *k = trim(p, &kl);
            if (kl == 5 && mem_ieq(k, "https", 5))
                return parse_proxy_addr(eq + 1, (size_t)(semi - eq - 1), host, hostsz, port);
        }
        p = semi + 1;
    }
    return -1;
}

static int glob_match(const char *pat, size_t pl, const char *s)
{
    /* iterative '*' matcher, case-insensitive, bounded by pl */
    size_t pi = 0, si = 0, star_p = (size_t)-1, star_s = 0, sl = strlen(s);
    while (si < sl) {
        if (pi < pl && pat[pi] == '*') { star_p = pi++; star_s = si; }
        else if (pi < pl && lc((unsigned char)pat[pi]) == lc((unsigned char)s[si])) { pi++; si++; }
        else if (star_p != (size_t)-1) { pi = star_p + 1; si = ++star_s; }
        else return 0;
    }
    while (pi < pl && pat[pi] == '*') pi++;
    return pi == pl;
}

int pc_proxy_bypass(const char *list, const char *host)
{
    const char *p, *end;
    if (!list || !host || !*host) return 0;
    p = list;
    end = list + strlen(list);
    while (p < end) {
        const char *e = p;
        size_t n;
        const char *t;
        while (e < end && *e != ';' && *e != ',' && *e != ' ') e++;
        n = (size_t)(e - p);
        t = trim(p, &n);
        if (n == 7 && mem_ieq(t, "<local>", 7)) {
            if (!strchr(host, '.')) return 1;
        } else if (n && glob_match(t, n, host)) {
            return 1;
        }
        p = e + 1;
    }
    return 0;
}
