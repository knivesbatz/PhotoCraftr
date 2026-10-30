/*
 * Native integration test for src/updater/net.c (NOT part of `make test`:
 * it needs the network).  Built and run by tools/run-net-test.sh.
 *
 *   nettest <url> [expected-substring]     download into memory
 *   nettest --expect-error <code> <url>    expect a specific PC_NET_ERR_*
 *
 * Proxy: taken from $HTTPS_PROXY (like the Windows build takes it from the
 * Internet Explorer settings).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcrt.h"
#include "updater/net.h"
#include "updater/httpparse.h"

static void on_status(void *ctx, const char *t) { (void)ctx; fprintf(stderr, "  [status] %s\n", t); }
static void on_progress(void *ctx, unsigned long d, unsigned long t)
{
    (void)ctx;
    fprintf(stderr, "  [progress] %lu / %lu\n", d, t);
}

int main(int argc, char **argv)
{
    pc_http_opts o;
    pc_http_result r;
    pc_membuf mb;
    const char *url, *want = NULL, *px = getenv("HTTPS_PROXY");
    int expect_err = 0, rc;
    if (!px) px = getenv("https_proxy");
    if (argc >= 4 && !strcmp(argv[1], "--expect-error")) { expect_err = atoi(argv[2]); url = argv[3]; }
    else if (argc >= 2) { url = argv[1]; want = argc > 2 ? argv[2] : NULL; }
    else { fprintf(stderr, "usage: nettest <url> [substring] | --expect-error <code> <url>\n"); return 64; }

    pc_http_opts_init(&o);
    o.max_bytes = 4 * 1024 * 1024;
    o.status = on_status;
    o.progress = on_progress;
    memset(&mb, 0, sizeof mb);
    mb.max = o.max_bytes;
    o.sink = pc_membuf_sink;
    o.ctx = &mb;
    if (px && *px && pc_proxy_parse(px, o.proxy_host, sizeof o.proxy_host, &o.proxy_port) == 0)
        fprintf(stderr, "  using proxy %s:%u\n", o.proxy_host, (unsigned)o.proxy_port);

    rc = pc_https_get(url, &o, &r);
    fprintf(stderr, "GET %s -> error=%d http=%d tls=%d bytes=%lu final=%s\n  message: %s\n",
            url, rc, r.http_status, r.tls_error, r.bytes, r.final_url, r.message);
    if (expect_err) {
        int ok = rc == expect_err;
        fprintf(stderr, "%s (expected error %d)\n", ok ? "PASS" : "FAIL", expect_err);
        return ok ? 0 : 1;
    }
    if (rc) { fprintf(stderr, "FAIL\n"); return 1; }
    if (want && (!mb.data || !strstr(mb.data, want))) { fprintf(stderr, "FAIL: body lacks '%s'\n", want); return 1; }
    fprintf(stderr, "PASS: %lu bytes; first line: %.60s\n", (unsigned long)mb.len, mb.data ? mb.data : "");
    pc_membuf_free(&mb);
    return 0;
}
