/*
 * PhotoCraft Classic updater - HTTPS GET over Winsock 1.1 + BearSSL TLS 1.2.
 *
 * Windows: wsock32.dll is loaded at run time (LoadLibrary), so PCUpdate.exe
 * still starts on a Windows 95 machine without TCP/IP installed (the
 * "install update from file" path needs no network).  The same code builds
 * on POSIX for the native integration test (tests/integration).
 *
 * Only https:// is ever fetched; redirects to anything else are refused.
 * Requests are HTTP/1.0 (no chunked responses), at most 5 redirects.
 */
#ifndef PC_NET_H
#define PC_NET_H

#include <stddef.h>
#include <bearssl.h>

enum {
    PC_NET_OK = 0,
    PC_NET_ERR_NOWINSOCK = -1,  /* wsock32.dll missing / WSAStartup failed  */
    PC_NET_ERR_URL = -2,        /* malformed URL                            */
    PC_NET_ERR_INSECURE = -3,   /* http:// URL or redirect refused          */
    PC_NET_ERR_DNS = -4,
    PC_NET_ERR_CONNECT = -5,
    PC_NET_ERR_PROXY = -6,      /* proxy refused CONNECT                    */
    PC_NET_ERR_TLS = -7,        /* handshake / certificate failure          */
    PC_NET_ERR_TIMEOUT = -8,
    PC_NET_ERR_CANCELLED = -9,
    PC_NET_ERR_HTTP = -10,      /* non-200 final status                     */
    PC_NET_ERR_PROTOCOL = -11,  /* malformed HTTP response                  */
    PC_NET_ERR_TOOBIG = -12,    /* larger than max_bytes / expected size    */
    PC_NET_ERR_TRUNCATED = -13, /* connection closed early                  */
    PC_NET_ERR_REDIRECTS = -14, /* more than 5 redirects                    */
    PC_NET_ERR_SINK = -15,      /* sink callback failed (disk full ...)     */
    PC_NET_ERR_NOMEM = -16,
    PC_NET_ERR_IO = -17         /* socket error                             */
};

typedef struct {
    /* HTTP CONNECT proxy; proxy_host[0] == 0 for a direct connection. */
    char proxy_host[256];
    unsigned short proxy_port;
    char proxy_bypass[512];     /* IE ProxyOverride list, may be empty      */

    unsigned long max_bytes;    /* hard cap on the body (required, > 0)     */
    unsigned long expect_size;  /* if non-zero, Content-Length must match   */
    int connect_timeout_ms;     /* default 20000                            */
    int io_timeout_ms;          /* default 30000 (inactivity)               */
    volatile long *cancel;      /* polled; non-zero aborts                  */

    /* Optional callbacks (called on the downloading thread). */
    void (*status)(void *ctx, const char *text);
    void (*progress)(void *ctx, unsigned long done, unsigned long total /* 0 = unknown */);
    int (*sink)(void *ctx, const void *data, size_t len);   /* 0 = ok */
    void *ctx;

    /* Trust anchors; NULL = the bundled pc_trust_anchors. */
    const br_x509_trust_anchor *tas;
    size_t tas_num;

    const char *user_agent;     /* NULL = "PhotoCraftClassic-Updater/<ver>" */
} pc_http_opts;

typedef struct {
    int error;                  /* PC_NET_*                                 */
    int http_status;            /* last HTTP status seen                    */
    int tls_error;              /* BearSSL error code, if any               */
    unsigned long bytes;        /* body bytes delivered to the sink         */
    char final_url[4400];       /* >= PC_URL_MAX */
    char message[256];          /* English, suitable for a dialog           */
} pc_http_result;

/* Growable memory sink for small downloads (the manifest). */
typedef struct {
    char *data;
    size_t len, cap, max;
} pc_membuf;
int pc_membuf_sink(void *ctx, const void *data, size_t len);
void pc_membuf_free(pc_membuf *b);

void pc_http_opts_init(pc_http_opts *o);

/* Blocking GET.  Returns result->error. */
int pc_https_get(const char *url, const pc_http_opts *o, pc_http_result *r);

/* Entropy pool shared by all connections.  pc_net_add_entropy() mixes in
 * caller data (e.g. a seed saved in the registry by the previous run);
 * pc_net_seed_out() derives 32 bytes to save for the next run (never equal
 * to the value used for keying). */
void pc_net_add_entropy(const void *data, size_t len);
void pc_net_seed_out(unsigned char out[32]);

/* Release Winsock (call once at exit; harmless if never started). */
void pc_net_cleanup(void);

#endif
