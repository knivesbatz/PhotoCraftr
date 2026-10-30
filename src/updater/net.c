/*
 * PhotoCraft Classic updater - HTTPS GET.  See net.h.
 *
 * Windows build: Winsock 1.1 (wsock32.dll, loaded at run time) + BearSSL.
 * POSIX build (native integration test only): BSD sockets + BearSSL.
 */
#include "pcrt.h"

#ifdef PC_WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#endif

#include "net.h"
#include "httpparse.h"
#include "manifest.h"
#include "trust_anchors.h"

#ifndef PC_VERSION
#define PC_VERSION "0.0.0"
#endif

#define SLICE_MS 250

/* ================================================================== */
/* sockets                                                             */

#ifdef PC_WIN32

typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET

static struct {
    HMODULE dll;
    int started;
    int (PASCAL *startup)(WORD, LPWSADATA);
    int (PASCAL *cleanup)(void);
    int (PASCAL *lasterror)(void);
    struct hostent *(PASCAL *gethostbyname_)(const char *);
    SOCKET (PASCAL *socket_)(int, int, int);
    int (PASCAL *connect_)(SOCKET, const struct sockaddr *, int);
    int (PASCAL *send_)(SOCKET, const char *, int, int);
    int (PASCAL *recv_)(SOCKET, char *, int, int);
    int (PASCAL *select_)(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
    int (PASCAL *closesocket_)(SOCKET);
    int (PASCAL *ioctlsocket_)(SOCKET, long, u_long *);
} ws;

static int load_proc(void *dst, const char *name)
{
    FARPROC p = GetProcAddress(ws.dll, name);
    memcpy(dst, &p, sizeof p);
    return p != NULL;
}

static int net_init(void)
{
    WSADATA wd;
    if (ws.started) return 0;
    if (!ws.dll) {
        /* absent on Windows 95 without networking: fail quietly */
        UINT em = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
        ws.dll = LoadLibraryA("WSOCK32.DLL");
        SetErrorMode(em);
    }
    if (!ws.dll) return -1;
    if (!load_proc(&ws.startup, "WSAStartup") || !load_proc(&ws.cleanup, "WSACleanup") ||
        !load_proc(&ws.lasterror, "WSAGetLastError") || !load_proc(&ws.gethostbyname_, "gethostbyname") ||
        !load_proc(&ws.socket_, "socket") || !load_proc(&ws.connect_, "connect") ||
        !load_proc(&ws.send_, "send") || !load_proc(&ws.recv_, "recv") ||
        !load_proc(&ws.select_, "select") || !load_proc(&ws.closesocket_, "closesocket") ||
        !load_proc(&ws.ioctlsocket_, "ioctlsocket"))
        return -1;
    if (ws.startup(MAKEWORD(1, 1), &wd) != 0) return -1;
    ws.started = 1;
    return 0;
}

void pc_net_cleanup(void)
{
    if (ws.started) ws.cleanup();
    ws.started = 0;
}

static void sock_close(sock_t s) { if (s != BAD_SOCK) ws.closesocket_(s); }

static int set_nonblock(sock_t s, int on)
{
    u_long v = (u_long)on;
    return ws.ioctlsocket_(s, FIONBIO, &v) == 0 ? 0 : -1;
}

/* Wait until s is readable (want_write == 0) or writable.  Returns 1 ready,
 * 0 timeout slice, -1 error.  Uses fd_set fields directly (FD_ISSET would
 * need __WSAFDIsSet). */
static int wait_sock(sock_t s, int want_write, int ms)
{
    fd_set fs, es;
    struct timeval tv;
    int n;
    fs.fd_count = 1; fs.fd_array[0] = s;
    es.fd_count = 1; es.fd_array[0] = s;
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    n = ws.select_(0, want_write ? NULL : &fs, want_write ? &fs : NULL, &es, &tv);
    if (n < 0) return -1;
    if (n == 0) return 0;
    if (es.fd_count) return -1;
    return fs.fd_count ? 1 : 0;
}

static int resolve_host(const char *host, unsigned char ip[4])
{
    struct hostent *he;
    if (pc_parse_ipv4(host, ip) == 0) return 0;
    he = ws.gethostbyname_(host);
    if (!he || he->h_addrtype != AF_INET || he->h_length != 4 || !he->h_addr_list || !he->h_addr_list[0]) return -1;
    memcpy(ip, he->h_addr_list[0], 4);
    return 0;
}

static int sock_send(sock_t s, const void *p, int n) { return ws.send_(s, (const char *)p, n, 0); }
static int sock_recv(sock_t s, void *p, int n) { return ws.recv_(s, (char *)p, n, 0); }
static sock_t sock_new(void) { return ws.socket_(AF_INET, SOCK_STREAM, 0); }
static int sock_connect_start(sock_t s, const struct sockaddr_in *sa)
{
    if (ws.connect_(s, (const struct sockaddr *)sa, sizeof *sa) == 0) return 1;
    return ws.lasterror() == WSAEWOULDBLOCK ? 0 : -1;
}
static int connect_result(sock_t s) { (void)s; return 0; }  /* select() reported it */

#else /* POSIX */

typedef int sock_t;
#define BAD_SOCK (-1)

static int net_init(void) { return 0; }
void pc_net_cleanup(void) { }
static void sock_close(sock_t s) { if (s >= 0) close(s); }

static int set_nonblock(sock_t s, int on)
{
    int fl = fcntl(s, F_GETFL, 0);
    if (fl < 0) return -1;
    return fcntl(s, F_SETFL, on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
}

static int wait_sock(sock_t s, int want_write, int ms)
{
    fd_set fs;
    struct timeval tv;
    int n;
    if (s >= FD_SETSIZE) return -1;
    FD_ZERO(&fs);
    FD_SET(s, &fs);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    n = select(s + 1, want_write ? NULL : &fs, want_write ? &fs : NULL, NULL, &tv);
    if (n < 0) return errno == EINTR ? 0 : -1;
    return n > 0 ? 1 : 0;
}

static int resolve_host(const char *host, unsigned char ip[4])
{
    struct addrinfo hints, *res = NULL;
    if (pc_parse_ipv4(host, ip) == 0) return 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res) || !res) return -1;
    memcpy(ip, &((struct sockaddr_in *)res->ai_addr)->sin_addr, 4);
    freeaddrinfo(res);
    return 0;
}

static int sock_send(sock_t s, const void *p, int n) { return (int)send(s, p, (size_t)n, MSG_NOSIGNAL); }
static int sock_recv(sock_t s, void *p, int n) { return (int)recv(s, p, (size_t)n, 0); }
static sock_t sock_new(void) { return socket(AF_INET, SOCK_STREAM, 0); }
static int sock_connect_start(sock_t s, const struct sockaddr_in *sa)
{
    if (connect(s, (const struct sockaddr *)sa, sizeof *sa) == 0) return 1;
    return errno == EINPROGRESS ? 0 : -1;
}
static int connect_result(sock_t s)
{
    int e = 0;
    socklen_t l = sizeof e;
    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &e, &l) || e) return -1;
    return 0;
}

#endif

/* ================================================================== */
/* entropy                                                             */

static br_sha256_context pool;
static int pool_ready;

void pc_net_add_entropy(const void *data, size_t len)
{
    if (!pool_ready) { br_sha256_init(&pool); pool_ready = 1; }
    br_sha256_update(&pool, data, len);
}

static void pool_derive(const char *label, unsigned char out[32])
{
    br_sha256_context c;
    if (!pool_ready) pc_net_add_entropy("", 0);
    c = pool;
    br_sha256_update(&c, label, strlen(label));
    br_sha256_out(&c, out);
    /* ratchet: future outputs depend on this one, but cannot reveal it */
    pc_net_add_entropy(label, strlen(label));
    pc_net_add_entropy(out, 1);
}

void pc_net_seed_out(unsigned char out[32]) { pool_derive("PCUpdate seed-file v1", out); }

#ifdef PC_WIN32
typedef BOOL (WINAPI *acquire_fn)(ULONG_PTR *, LPCSTR, LPCSTR, DWORD, DWORD);
typedef BOOL (WINAPI *genrandom_fn)(ULONG_PTR, DWORD, BYTE *);
typedef BOOL (WINAPI *release_fn)(ULONG_PTR, DWORD);
typedef BOOLEAN (WINAPI *rtlgen_fn)(PVOID, ULONG);

static void os_random(void)
{
    /* CryptGenRandom is missing from retail Windows 95 (it arrived with
     * OSR2/IE3), so it is only used when present. */
    HMODULE adv = LoadLibraryA("ADVAPI32.DLL");
    unsigned char buf[32];
    if (!adv) return;
    {
        FARPROC p1 = GetProcAddress(adv, "CryptAcquireContextA");
        FARPROC p2 = GetProcAddress(adv, "CryptGenRandom");
        FARPROC p3 = GetProcAddress(adv, "CryptReleaseContext");
        FARPROC p4 = GetProcAddress(adv, "SystemFunction036");   /* RtlGenRandom, XP+ */
        if (p1 && p2 && p3) {
            acquire_fn acq; genrandom_fn gen; release_fn rel;
            ULONG_PTR prov = 0;
            memcpy(&acq, &p1, sizeof p1); memcpy(&gen, &p2, sizeof p2); memcpy(&rel, &p3, sizeof p3);
            if (acq(&prov, NULL, NULL, 1 /* PROV_RSA_FULL */, 0xF0000000UL /* CRYPT_VERIFYCONTEXT */)) {
                if (gen(prov, sizeof buf, buf)) pc_net_add_entropy(buf, sizeof buf);
                rel(prov, 0);
            }
        }
        if (p4) {
            rtlgen_fn rg;
            memcpy(&rg, &p4, sizeof p4);
            if (rg(buf, sizeof buf)) pc_net_add_entropy(buf, sizeof buf);
        }
    }
    memset(buf, 0, sizeof buf);
    FreeLibrary(adv);
}

static void gather_entropy(void)
{
    struct {
        DWORD tick, pid, tid;
        LARGE_INTEGER qpc, qpf;
        POINT cursor;
        MEMORYSTATUS mem;
        SYSTEMTIME st;
        HWND fg;
        void *stack, *heap;
        DWORD jitter[64];
    } e;
    int i;
    memset(&e, 0, sizeof e);
    e.tick = GetTickCount();
    e.pid = GetCurrentProcessId();
    e.tid = GetCurrentThreadId();
    QueryPerformanceCounter(&e.qpc);
    QueryPerformanceFrequency(&e.qpf);
    GetCursorPos(&e.cursor);
    e.mem.dwLength = sizeof e.mem;
    GlobalMemoryStatus(&e.mem);
    GetSystemTime(&e.st);
    e.fg = GetForegroundWindow();
    e.stack = &e;
    e.heap = GetProcessHeap();
    /* timing jitter: scheduler and busy-loop timing measured with QPC
     * (the 1.19 MHz PIT on Windows 95) and GetTickCount */
    for (i = 0; i < 64; i++) {
        LARGE_INTEGER a, b;
        volatile unsigned k, spin = 200 + (e.tick ^ (unsigned)i * 2654435761U) % 997;
        QueryPerformanceCounter(&a);
        for (k = 0; k < spin; k++) { }
        if ((i & 7) == 7) Sleep(0);
        QueryPerformanceCounter(&b);
        e.jitter[i] = (DWORD)(b.LowPart - a.LowPart) ^ (GetTickCount() << 16);
    }
    pc_net_add_entropy(&e, sizeof e);
    os_random();
}

static void now_for_x509(unsigned long *days, unsigned long *secs)
{
    SYSTEMTIME st;
    long d;
    GetSystemTime(&st);
    d = pc_days_from_civil(st.wYear, st.wMonth, st.wDay);
    *days = d > 0 ? (unsigned long)d : 0;
    *secs = (unsigned long)st.wHour * 3600UL + st.wMinute * 60UL + st.wSecond;
}

#else

static void gather_entropy(void)
{
    struct { struct timespec t1, t2; pid_t pid; void *stack; unsigned char rnd[32]; } e;
    int fd;
    memset(&e, 0, sizeof e);
    clock_gettime(CLOCK_REALTIME, &e.t1);
    clock_gettime(CLOCK_MONOTONIC, &e.t2);
    e.pid = getpid();
    e.stack = &e;
    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) { if (read(fd, e.rnd, sizeof e.rnd) != (ssize_t)sizeof e.rnd) e.rnd[0] ^= 1; close(fd); }
    pc_net_add_entropy(&e, sizeof e);
}

static void now_for_x509(unsigned long *days, unsigned long *secs)
{
    time_t t = time(NULL);
    *days = (unsigned long)(t / 86400) + 719528UL;
    *secs = (unsigned long)(t % 86400);
}

#endif

/* ================================================================== */
/* connection                                                          */

typedef struct {
    br_ssl_client_context sc;
    br_x509_minimal_context xc;
    br_sslio_context ioc;
    sock_t s;
    const pc_http_opts *o;
    int io_err;         /* PC_NET_ERR_* recorded by the socket callbacks */
    unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
} conn_t;

static int cancelled(const pc_http_opts *o) { return o->cancel && *o->cancel; }

static void say(const pc_http_opts *o, const char *fmt, const char *arg)
{
    char buf[300];
    if (!o->status) return;
    pc_snprintf(buf, sizeof buf, fmt, arg);
    o->status(o->ctx, buf);
}

static void fail(pc_http_result *r, int code, const char *fmt, const char *arg)
{
    r->error = code;
    pc_snprintf(r->message, sizeof r->message, fmt, arg ? arg : "");
}

/* Wait for readiness in SLICE_MS steps; honours cancel and timeout. */
static int wait_io(conn_t *c, int want_write, int timeout_ms)
{
    int waited = 0;
    for (;;) {
        int w;
        if (cancelled(c->o)) { c->io_err = PC_NET_ERR_CANCELLED; return -1; }
        w = wait_sock(c->s, want_write, SLICE_MS);
        if (w > 0) return 0;
        if (w < 0) { c->io_err = PC_NET_ERR_IO; return -1; }
        waited += SLICE_MS;
        if (waited >= timeout_ms) { c->io_err = PC_NET_ERR_TIMEOUT; return -1; }
    }
}

static int raw_read(conn_t *c, unsigned char *buf, size_t len)
{
    int n;
    if (len > 0x7FFF0000UL) len = 0x7FFF0000UL;
    if (wait_io(c, 0, c->o->io_timeout_ms)) return -1;
    n = sock_recv(c->s, buf, (int)len);
    if (n <= 0) { if (!c->io_err) c->io_err = n == 0 ? PC_NET_ERR_TRUNCATED : PC_NET_ERR_IO; return -1; }
    return n;
}

static int raw_write(conn_t *c, const unsigned char *buf, size_t len)
{
    int n;
    if (len > 0x7FFF0000UL) len = 0x7FFF0000UL;
    if (wait_io(c, 1, c->o->io_timeout_ms)) return -1;
    n = sock_send(c->s, buf, (int)len);
    if (n <= 0) { c->io_err = PC_NET_ERR_IO; return -1; }
    return n;
}

static int cb_read(void *ctx, unsigned char *buf, size_t len) { return raw_read((conn_t *)ctx, buf, len); }
static int cb_write(void *ctx, const unsigned char *buf, size_t len) { return raw_write((conn_t *)ctx, buf, len); }

static int raw_write_all(conn_t *c, const char *p, size_t n)
{
    while (n) {
        int k = raw_write(c, (const unsigned char *)p, n);
        if (k < 0) return -1;
        p += k; n -= (size_t)k;
    }
    return 0;
}

static int tcp_connect(conn_t *c, const char *host, unsigned short port, pc_http_result *r)
{
    unsigned char ip[4];
    struct sockaddr_in sa;
    int rc, waited = 0;
    say(c->o, "Looking up %s...", host);
    if (resolve_host(host, ip)) {
        fail(r, PC_NET_ERR_DNS, "Could not find the server %s. Check your Internet connection.", host);
        return -1;
    }
    if (cancelled(c->o)) { fail(r, PC_NET_ERR_CANCELLED, "Cancelled.%s", ""); return -1; }
    say(c->o, "Connecting to %s...", host);
    c->s = sock_new();
    if (c->s == BAD_SOCK) { fail(r, PC_NET_ERR_IO, "Could not create a network socket.%s", ""); return -1; }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = (unsigned short)((port >> 8) | (port << 8));  /* htons, little-endian host */
    memcpy(&sa.sin_addr, ip, 4);
    if (set_nonblock(c->s, 1)) { fail(r, PC_NET_ERR_IO, "Network socket error.%s", ""); return -1; }
    rc = sock_connect_start(c->s, &sa);
    while (rc == 0) {
        int w;
        if (cancelled(c->o)) { fail(r, PC_NET_ERR_CANCELLED, "Cancelled.%s", ""); return -1; }
        w = wait_sock(c->s, 1, SLICE_MS);
        if (w > 0) { rc = connect_result(c->s) == 0 ? 1 : -1; break; }
        if (w < 0) { rc = -1; break; }
        waited += SLICE_MS;
        if (waited >= c->o->connect_timeout_ms) {
            fail(r, PC_NET_ERR_TIMEOUT, "Timed out connecting to %s.", host);
            return -1;
        }
    }
    if (rc < 0) { fail(r, PC_NET_ERR_CONNECT, "Could not connect to %s.", host); return -1; }
    if (set_nonblock(c->s, 0)) { fail(r, PC_NET_ERR_IO, "Network socket error.%s", ""); return -1; }
    return 0;
}

static int proxy_connect(conn_t *c, const pc_url *u, const char *ua, pc_http_result *r)
{
    char req[600], head[4096];
    size_t got = 0, end = 0;
    pc_http_head h;
    if (tcp_connect(c, c->o->proxy_host, c->o->proxy_port, r)) {
        if (r->error == PC_NET_ERR_DNS || r->error == PC_NET_ERR_CONNECT)
            pc_snprintf(r->message, sizeof r->message, "Could not connect to the proxy server %s.", c->o->proxy_host);
        return -1;
    }
    say(c->o, "Opening a tunnel through proxy %s...", c->o->proxy_host);
    if (pc_http_build_connect(req, sizeof req, u->host, u->port, ua) < 0 || raw_write_all(c, req, strlen(req))) {
        fail(r, c->io_err ? c->io_err : PC_NET_ERR_PROXY, "The proxy server %s did not respond.", c->o->proxy_host);
        return -1;
    }
    /* Read the proxy's answer byte by byte so no TLS bytes are consumed. */
    while (!end) {
        if (got >= sizeof head || raw_read(c, (unsigned char *)head + got, 1) != 1) {
            fail(r, c->io_err == PC_NET_ERR_CANCELLED ? PC_NET_ERR_CANCELLED : PC_NET_ERR_PROXY,
                 "The proxy server %s sent an invalid response.", c->o->proxy_host);
            return -1;
        }
        got++;
        end = pc_http_head_end(head, got);
    }
    if (pc_http_parse_head(head, end, &h) || h.status != 200) {
        char msg[64];
        pc_snprintf(msg, sizeof msg, "%d", h.status);
        r->http_status = h.status;
        fail(r, PC_NET_ERR_PROXY, "The proxy server refused the connection (HTTP %s).", msg);
        return -1;
    }
    return 0;
}

static void tls_fail(conn_t *c, pc_http_result *r, const char *host)
{
    int e = br_ssl_engine_last_error(&c->sc.eng);
    r->tls_error = e;
    if (c->io_err == PC_NET_ERR_CANCELLED) { fail(r, PC_NET_ERR_CANCELLED, "Cancelled.%s", ""); return; }
    if (c->io_err == PC_NET_ERR_TIMEOUT) { fail(r, PC_NET_ERR_TIMEOUT, "The connection to %s timed out.", host); return; }
    switch (e) {
    case BR_ERR_X509_EXPIRED:
        fail(r, PC_NET_ERR_TLS, "The security certificate of %s is not valid at the current date. "
             "Please check that your computer's date and time are set correctly.", host);
        break;
    case BR_ERR_X509_NOT_TRUSTED:
        fail(r, PC_NET_ERR_TLS, "The security certificate of %s was not issued by a trusted authority, "
             "or a network device is intercepting the connection.", host);
        break;
    case BR_ERR_X509_BAD_SERVER_NAME:
        fail(r, PC_NET_ERR_TLS, "The security certificate does not belong to %s.", host);
        break;
    case BR_ERR_OK:
    case BR_ERR_IO:
        fail(r, c->io_err ? c->io_err : PC_NET_ERR_IO, "The connection to %s was closed unexpectedly.", host);
        break;
    default: {
        char msg[300];
        pc_snprintf(msg, sizeof msg, "A secure connection to %s could not be established (TLS error %d).", host, e);
        fail(r, PC_NET_ERR_TLS, "%s", msg);
        break;
    }
    }
}

/* One request.  On a redirect, writes the absolute target into 'next'. */
static int fetch_once(const pc_url *u, const pc_http_opts *o, pc_http_result *r, char *next, size_t nextsz)
{
    conn_t *c;
    static const char *const no_ua = "PhotoCraftClassic-Updater/" PC_VERSION;
    const char *ua = o->user_agent ? o->user_agent : no_ua;
    unsigned char seed[32];
    unsigned long days, secs, delivered = 0;
    char *req = NULL, *head = NULL;
    size_t got = 0, hend = 0;
    pc_http_head h;
    int ret = -1;

    next[0] = 0;
    c = (conn_t *)calloc(1, sizeof *c);
    req = (char *)malloc(PC_URL_MAX + 512);
    head = (char *)malloc(PC_HTTP_HEAD_MAX);
    if (!c || !req || !head) { fail(r, PC_NET_ERR_NOMEM, "Out of memory.%s", ""); goto out; }
    c->s = BAD_SOCK;
    c->o = o;

    if (o->proxy_host[0] && !pc_proxy_bypass(o->proxy_bypass, u->host)) {
        if (proxy_connect(c, u, ua, r)) goto out;
    } else {
        if (tcp_connect(c, u->host, u->port, r)) goto out;
    }

    say(o, "Establishing a secure connection to %s...", u->host);
    gather_entropy();                   /* includes DNS/connect timing */
    br_ssl_client_init_full(&c->sc, &c->xc, o->tas ? o->tas : pc_trust_anchors, o->tas ? o->tas_num : pc_trust_anchors_num);
    br_ssl_engine_set_versions(&c->sc.eng, BR_TLS12, BR_TLS12);
    br_ssl_engine_set_buffer(&c->sc.eng, c->iobuf, sizeof c->iobuf, 1);
    now_for_x509(&days, &secs);
    br_x509_minimal_set_time(&c->xc, (uint32_t)days, (uint32_t)secs);
    pool_derive("PCUpdate TLS seed v1", seed);
    br_ssl_engine_inject_entropy(&c->sc.eng, seed, sizeof seed);
    memset(seed, 0, sizeof seed);
    if (!br_ssl_client_reset(&c->sc, u->host, 0)) { tls_fail(c, r, u->host); goto out; }
    br_sslio_init(&c->ioc, &c->sc.eng, cb_read, c, cb_write, c);

    if (pc_http_build_get(req, PC_URL_MAX + 512, u, ua) < 0) { fail(r, PC_NET_ERR_URL, "The address is too long.%s", ""); goto out; }
    if (br_sslio_write_all(&c->ioc, req, strlen(req)) < 0 || br_sslio_flush(&c->ioc) < 0) { tls_fail(c, r, u->host); goto out; }
    say(o, "Waiting for %s to answer...", u->host);

    /* response head */
    while (!hend) {
        int n;
        if (got >= PC_HTTP_HEAD_MAX) { fail(r, PC_NET_ERR_PROTOCOL, "The server %s sent an invalid response.", u->host); goto out; }
        n = br_sslio_read(&c->ioc, head + got, PC_HTTP_HEAD_MAX - got);
        if (n < 0) {
            if (br_ssl_engine_last_error(&c->sc.eng) == BR_ERR_OK && !c->io_err)
                fail(r, PC_NET_ERR_PROTOCOL, "The server %s closed the connection without answering.", u->host);
            else
                tls_fail(c, r, u->host);
            goto out;
        }
        got += (size_t)n;
        hend = pc_http_head_end(head, got);
    }
    if (pc_http_parse_head(head, hend, &h)) { fail(r, PC_NET_ERR_PROTOCOL, "The server %s sent an invalid response.", u->host); goto out; }
    r->http_status = h.status;

    if (h.status == 301 || h.status == 302 || h.status == 303 || h.status == 307 || h.status == 308) {
        int rc;
        if (!h.location[0]) { fail(r, PC_NET_ERR_PROTOCOL, "The server %s sent a redirect without a target.", u->host); goto out; }
        rc = pc_url_resolve(u, h.location, next, nextsz);
        if (rc == PC_URL_INSECURE) { fail(r, PC_NET_ERR_INSECURE, "The server %s redirected to an insecure (non-HTTPS) address; refused.", u->host); goto out; }
        if (rc) { fail(r, PC_NET_ERR_PROTOCOL, "The server %s sent an invalid redirect.", u->host); goto out; }
        ret = 0;
        goto out;
    }
    if (h.status != 200) {
        char msg[80];
        if (h.status == 404) pc_snprintf(msg, sizeof msg, "404 Not Found");
        else pc_snprintf(msg, sizeof msg, "HTTP status %d", h.status);
        fail(r, PC_NET_ERR_HTTP, "The server answered: %s.", msg);
        goto out;
    }
    if (h.chunked) { fail(r, PC_NET_ERR_PROTOCOL, "The server %s used an unsupported transfer encoding.", u->host); goto out; }
    if (h.content_length >= 0 && (unsigned long)h.content_length > o->max_bytes) {
        fail(r, PC_NET_ERR_TOOBIG, "The file on %s is larger than expected.", u->host); goto out;
    }
    if (o->expect_size && h.content_length >= 0 && (unsigned long)h.content_length != o->expect_size) {
        fail(r, PC_NET_ERR_TOOBIG, "The file on %s does not have the expected size.", u->host); goto out;
    }

    say(o, "Downloading from %s...", u->host);
    {
        unsigned long total = h.content_length >= 0 ? (unsigned long)h.content_length : o->expect_size;
        size_t pending = got - hend;
        const char *p = head + hend;
        if (o->progress) o->progress(o->ctx, 0, total);
        for (;;) {
            if (pending) {
                if (h.content_length >= 0 && delivered + pending > (unsigned long)h.content_length)
                    pending = (size_t)((unsigned long)h.content_length - delivered);   /* ignore excess */
                if (delivered + pending > o->max_bytes) { fail(r, PC_NET_ERR_TOOBIG, "The file on %s is larger than expected.", u->host); goto out; }
                if (pending && o->sink && o->sink(o->ctx, p, pending)) { fail(r, PC_NET_ERR_SINK, "Could not save the downloaded data.%s", ""); goto out; }
                delivered += (unsigned long)pending;
                r->bytes = delivered;
                if (o->progress) o->progress(o->ctx, delivered, total);
            }
            if (h.content_length >= 0 && delivered >= (unsigned long)h.content_length) break;
            if (cancelled(o)) { fail(r, PC_NET_ERR_CANCELLED, "Cancelled.%s", ""); goto out; }
            {
                int n = br_sslio_read(&c->ioc, head, PC_HTTP_HEAD_MAX);
                if (n < 0) {
                    int e = br_ssl_engine_last_error(&c->sc.eng);
                    if (h.content_length < 0 && e == BR_ERR_OK && !c->io_err) break;   /* clean close_notify */
                    if (c->io_err == PC_NET_ERR_CANCELLED || c->io_err == PC_NET_ERR_TIMEOUT || (e != BR_ERR_OK && e != BR_ERR_IO)) {
                        tls_fail(c, r, u->host);
                        goto out;
                    }
                    fail(r, PC_NET_ERR_TRUNCATED, "The download from %s was interrupted.", u->host);
                    goto out;
                }
                p = head;
                pending = (size_t)n;
            }
        }
    }
    if (o->expect_size && delivered != o->expect_size) {
        fail(r, PC_NET_ERR_TRUNCATED, "The download from %s is incomplete.", u->host);
        goto out;
    }
    ret = 0;
out:
    if (c) { sock_close(c->s); memset(c, 0, sizeof *c); free(c); }
    free(req);
    free(head);
    return ret;
}

void pc_http_opts_init(pc_http_opts *o)
{
    memset(o, 0, sizeof *o);
    o->max_bytes = 65536;
    o->connect_timeout_ms = 20000;
    o->io_timeout_ms = 30000;
}

int pc_https_get(const char *url, const pc_http_opts *o, pc_http_result *r)
{
    char *cur, *next;
    int hop, rc;
    pc_url *u;
    memset(r, 0, sizeof *r);
    if (!url || !o || !o->max_bytes) { fail(r, PC_NET_ERR_URL, "Internal error.%s", ""); return r->error; }
    cur = (char *)malloc(PC_URL_MAX);
    next = (char *)malloc(PC_URL_MAX);
    u = (pc_url *)malloc(sizeof *u);
    if (!cur || !next || !u) { fail(r, PC_NET_ERR_NOMEM, "Out of memory.%s", ""); goto done; }
    if (strlen(url) >= PC_URL_MAX) { fail(r, PC_NET_ERR_URL, "The address is too long.%s", ""); goto done; }
    strcpy(cur, url);
    if (net_init()) {
        fail(r, PC_NET_ERR_NOWINSOCK, "TCP/IP networking (Windows Sockets) is not installed or could not be started.%s", "");
        goto done;
    }
    for (hop = 0; hop <= 5; hop++) {
        rc = pc_url_parse_https(cur, u);
        if (rc == PC_URL_INSECURE) { fail(r, PC_NET_ERR_INSECURE, "Refusing to use an insecure (non-HTTPS) address.%s", ""); goto done; }
        if (rc) { fail(r, PC_NET_ERR_URL, "The update address is not valid.%s", ""); goto done; }
        pc_url_format(u, r->final_url, sizeof r->final_url);
        if (fetch_once(u, o, r, next, PC_URL_MAX)) goto done;
        if (!next[0]) { r->error = PC_NET_OK; r->message[0] = 0; goto done; }
        strcpy(cur, next);
    }
    fail(r, PC_NET_ERR_REDIRECTS, "Too many redirects.%s", "");
done:
    free(cur);
    free(next);
    free(u);
    return r->error;
}

int pc_membuf_sink(void *ctx, const void *data, size_t len)
{
    pc_membuf *b = (pc_membuf *)ctx;
    if (len > b->max || b->len > b->max - len) return -1;
    if (b->len + len + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 4096;
        char *nd;
        while (nc < b->len + len + 1) nc *= 2;
        nd = (char *)realloc(b->data, nc);
        if (!nd) return -1;
        b->data = nd;
        b->cap = nc;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    b->data[b->len] = 0;
    return 0;
}

void pc_membuf_free(pc_membuf *b)
{
    free(b->data);
    memset(b, 0, sizeof *b);
}
