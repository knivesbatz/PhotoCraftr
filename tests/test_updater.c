/*
 * Unit tests for the updater's portable logic: manifest parsing and
 * signature verification, version comparison and downgrade policy, the
 * HTTP/URL/proxy parsers, plus a deterministic fuzz pass over all parsers.
 *
 * Entry point: int test_updater(void) - returns the number of failures and
 * also counts into g_test_checks / g_test_failures (tests/testutil.h).
 * Called from tests/main.c.  Build with -DPC_TEST_UPDATER_MAIN to get a
 * standalone main() for running these tests alone.
 *
 * httpparse.c is included directly so that the shared test Makefile rule
 * (which only adds src/updater/manifest.c) does not need to change.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcrt.h"
#include "updater/manifest.h"
#include "updater/pubkey.h"
#include "../src/updater/httpparse.c"

#include <monocypher/monocypher.h>
#include <monocypher/optional/monocypher-ed25519.h>

/* "sig" line produced by: build/native/pcsign sign <test key> for the
 * vector in test_pcsign_vector() (test key = the one in pubkey.h). */
#define PCSIGN_VECTOR_SIG_LINE \
    "sig rEgs5sIMmUQWVSxGM6laBJZNleJmJ3RaWhpu440N+sDXCqymXR7cWtLYbbIJRmgfS1eY8m5pK5OiIvDJYF3/CA==\n"

/* Counters shared with the tests/main.c harness (tests/testutil.h). */
extern int g_test_failures, g_test_checks;

static int tu_fail;
#define CHECK(cond) do { g_test_checks++; if (!(cond)) { tu_fail++; g_test_failures++; \
    fprintf(stderr, "test_updater: %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---------- helpers ---------- */

static void b64enc(const unsigned char *in, size_t n, char *out)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, o = 0;
    for (i = 0; i + 2 < n; i += 3) {
        unsigned long w = (unsigned long)in[i] << 16 | (unsigned long)in[i + 1] << 8 | in[i + 2];
        out[o++] = T[w >> 18 & 63]; out[o++] = T[w >> 12 & 63]; out[o++] = T[w >> 6 & 63]; out[o++] = T[w & 63];
    }
    if (n - i == 1) { unsigned long w = (unsigned long)in[i] << 16; out[o++] = T[w >> 18 & 63]; out[o++] = T[w >> 12 & 63]; out[o++] = '='; out[o++] = '='; }
    else if (n - i == 2) { unsigned long w = (unsigned long)in[i] << 16 | (unsigned long)in[i + 1] << 8; out[o++] = T[w >> 18 & 63]; out[o++] = T[w >> 12 & 63]; out[o++] = T[w >> 6 & 63]; out[o++] = '='; }
    out[o] = 0;
}

static unsigned char test_sk[64], test_pk[32], other_pk[32];

static void make_keys(void)
{
    unsigned char seed[32], sk2[64];
    int i;
    for (i = 0; i < 32; i++) seed[i] = (unsigned char)(i * 7 + 1);
    crypto_ed25519_key_pair(test_sk, test_pk, seed);
    for (i = 0; i < 32; i++) seed[i] = (unsigned char)(i * 13 + 5);
    crypto_ed25519_key_pair(sk2, other_pk, seed);
}

/* Sign 'body' (must end with '\n') into out; returns length. */
static size_t sign_body(const char *body, char *out, size_t outsz)
{
    unsigned char sig[64];
    char b64[100];
    size_t n = strlen(body);
    crypto_ed25519_sign(sig, test_sk, (const unsigned char *)body, n);
    b64enc(sig, 64, b64);
    snprintf(out, outsz, "%ssig %s\n", body, b64);
    return strlen(out);
}

static const char *GOOD_BODY =
    "PCUPDATE 1\n"
    "product PhotoCraftClassic\n"
    "channel stable\n"
    "version 0.2.0\n"
    "sequence 7\n"
    "released 2026-11-01\n"
    "msi_url https://github.com/knivesbatz/PhotoCraftr/releases/download/v0.2.0/PhotoCraftClassic-Setup.msi\n"
    "msi_size 123456\n"
    "msi_sha256 00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF\n"
    "notes First line.\n"
    "notes\n"
    "notes Third line with caf\xe9.\n";

static int load_with_test_key(const char *buf, size_t len, pc_manifest *m)
{
    unsigned char keys[2][32];
    char err[160];
    memcpy(keys[0], other_pk, 32);
    memcpy(keys[1], test_pk, 32);
    return pc_manifest_load(buf, len, (const unsigned char (*)[32])keys, 2, m, err, sizeof err);
}

/* Body with one line replaced/added, signed. Returns load() result. */
static int load_variant(const char *from, const char *to)
{
    static char body[8192], signedbuf[9000];
    static pc_manifest m;
    const char *p = strstr(GOOD_BODY, from);
    if (!p) { fprintf(stderr, "variant base not found: %s\n", from); tu_fail++; return 999; }
    snprintf(body, sizeof body, "%.*s%s%s", (int)(p - GOOD_BODY), GOOD_BODY, to, p + strlen(from));
    return load_with_test_key(signedbuf, sign_body(body, signedbuf, sizeof signedbuf), &m);
}

/* ---------- tests ---------- */

static void test_encodings(void)
{
    unsigned char b[64];
    char hex[9];
    CHECK(pc_hex_decode("00ffA0", 6, b, 3) == 3 && b[0] == 0 && b[1] == 0xff && b[2] == 0xa0);
    CHECK(pc_hex_decode("0g", 2, b, 1) == -1);
    CHECK(pc_hex_decode("abc", 3, b, 2) == -1);
    CHECK(pc_hex_decode("abcd", 4, b, 1) == -1);
    pc_hex_encode((const unsigned char *)"\x01\xab\xff\x00", 4, hex);
    CHECK(!strcmp(hex, "01abff00"));
    CHECK(pc_base64_decode("TWFu", 4, b, 3) == 3 && !memcmp(b, "Man", 3));
    CHECK(pc_base64_decode("TWE=", 4, b, 3) == 2 && !memcmp(b, "Ma", 2));
    CHECK(pc_base64_decode("TQ==", 4, b, 3) == 1 && b[0] == 'M');
    CHECK(pc_base64_decode("TR==", 4, b, 3) == -1);     /* non-canonical */
    CHECK(pc_base64_decode("TWF", 3, b, 3) == -1);
    CHECK(pc_base64_decode("T=Fu", 4, b, 3) == -1);
    CHECK(pc_base64_decode("TQ==TWFu", 8, b, 6) == -1);  /* padding in the middle */
    CHECK(pc_base64_decode("TW u", 4, b, 3) == -1);
    CHECK(pc_base64_decode("TWFu", 4, b, 2) == -1);      /* output too small */
    {
        unsigned char h[32], ref[32];
        static const unsigned char abc_sha[32] = {
            0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
            0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad };
        pc_sha256("abc", 3, h);
        memcpy(ref, abc_sha, 32);
        CHECK(!memcmp(h, ref, 32));
    }
}

static void test_dates(void)
{
    CHECK(pc_days_from_civil(1970, 1, 1) == 719528);    /* BearSSL's convention */
    CHECK(pc_days_from_civil(1970, 1, 2) == 719529);
    CHECK(pc_days_from_civil(2000, 3, 1) - pc_days_from_civil(2000, 2, 28) == 2);
    CHECK(pc_days_from_civil(1900, 3, 1) - pc_days_from_civil(1900, 2, 28) == 1);
    CHECK(pc_days_from_civil(2026, 11, 1) == 719528 + 20758);
    CHECK(pc_days_from_civil(2025, 2, 29) == -1);
    CHECK(pc_days_from_civil(2024, 2, 29) > 0);
    CHECK(pc_days_from_civil(2024, 13, 1) == -1);
    CHECK(pc_days_from_civil(2024, 4, 31) == -1);
    CHECK(pc_days_from_civil(0, 1, 1) == -1);
}

static void test_versions(void)
{
    pc_version v;
    CHECK(pc_version_parse("0.1.0", &v) == 0 && v.major == 0 && v.minor == 1 && v.patch == 0 && !v.pre[0]);
    CHECK(pc_version_parse("255.255.65535", &v) == 0);
    CHECK(pc_version_parse("256.0.0", &v) == -1);
    CHECK(pc_version_parse("1.256.0", &v) == -1);
    CHECK(pc_version_parse("1.0.65536", &v) == -1);
    CHECK(pc_version_parse("01.0.0", &v) == -1);
    CHECK(pc_version_parse("1.0", &v) == -1);
    CHECK(pc_version_parse("1.0.0.0", &v) == -1);
    CHECK(pc_version_parse("1.0.0-", &v) == -1);
    CHECK(pc_version_parse("1.0.0-a..b", &v) == -1);
    CHECK(pc_version_parse("1.0.0-01", &v) == -1);
    CHECK(pc_version_parse("1.0.0+build", &v) == -1);
    CHECK(pc_version_parse("1.0.0-rc.1", &v) == 0 && !strcmp(v.pre, "rc.1"));
    CHECK(pc_version_parse(" 1.0.0", &v) == -1);
    CHECK(pc_version_parse("", &v) == -1);
    CHECK(pc_version_cmp_str("0.2.0", "0.1.9") > 0);
    CHECK(pc_version_cmp_str("0.10.0", "0.9.0") > 0);
    CHECK(pc_version_cmp_str("1.0.0", "1.0.0") == 0);
    CHECK(pc_version_cmp_str("1.0.0-rc.1", "1.0.0") < 0);
    /* semver 11.4 example chain */
    {
        static const char *chain[] = { "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta",
                                       "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0" };
        size_t i, j;
        for (i = 0; i < 8; i++)
            for (j = 0; j < 8; j++) {
                int c = pc_version_cmp_str(chain[i], chain[j]);
                CHECK((i < j && c < 0) || (i == j && c == 0) || (i > j && c > 0));
            }
    }
    CHECK(pc_version_cmp_str("x", "1.0.0") == -2);
}

static void test_manifest_good(void)
{
    char buf[8192];
    pc_manifest m;
    size_t n = sign_body(GOOD_BODY, buf, sizeof buf);
    int rc = load_with_test_key(buf, n, &m);
    CHECK(rc == PC_MF_OK);
    CHECK(m.key_index == 1);
    CHECK(!strcmp(m.product, "PhotoCraftClassic"));
    CHECK(!strcmp(m.channel, "stable"));
    CHECK(!strcmp(m.version_str, "0.2.0") && m.version.minor == 2);
    CHECK(m.sequence == 7);
    CHECK(!strcmp(m.released, "2026-11-01"));
    CHECK(!strncmp(m.msi_url, "https://github.com/", 19));
    CHECK(m.msi_size == 123456);
    CHECK(m.msi_sha256[0] == 0x00 && m.msi_sha256[31] == 0xff && m.msi_sha256[9] == 0x99);
    CHECK(!strcmp(m.notes, "First line.\r\n\r\nThird line with caf\xe9."));
    CHECK(m.signed_len == strlen(GOOD_BODY));

    /* CRLF line endings are fine (signature covers the exact bytes) */
    {
        char crlf[8192], sb[9000];
        size_t i, o = 0;
        for (i = 0; GOOD_BODY[i]; i++) { if (GOOD_BODY[i] == '\n') crlf[o++] = '\r'; crlf[o++] = GOOD_BODY[i]; }
        crlf[o] = 0;
        n = sign_body(crlf, sb, sizeof sb);
        /* make the sig line CRLF too */
        sb[n - 1] = '\r'; sb[n] = '\n'; sb[n + 1] = 0;
        CHECK(load_with_test_key(sb, n + 1, &m) == PC_MF_OK);
        CHECK(!strcmp(m.notes, "First line.\r\n\r\nThird line with caf\xe9."));
    }
    /* no trailing newline after the signature is fine */
    n = sign_body(GOOD_BODY, buf, sizeof buf);
    CHECK(load_with_test_key(buf, n - 1, &m) == PC_MF_OK);
    /* unknown keys are tolerated (forward compatible) */
    CHECK(load_variant("msi_size 123456\n", "msi_size 123456\nmin_windows 4.0\n") == PC_MF_OK);
}

static void test_manifest_bad(void)
{
    char buf[8192];
    pc_manifest m;
    size_t n;
    unsigned char keys[1][32];

    /* tampering with any signed byte breaks the signature */
    n = sign_body(GOOD_BODY, buf, sizeof buf);
    {
        size_t i;
        int all_rejected = 1;
        for (i = 0; i < strlen(GOOD_BODY); i += 7) {
            buf[i] ^= 0x01;
            if (load_with_test_key(buf, n, &m) == PC_MF_OK) all_rejected = 0;
            buf[i] ^= 0x01;
        }
        CHECK(all_rejected);
    }
    /* signature bit flip */
    buf[n - 5] = buf[n - 5] == 'A' ? 'B' : 'A';
    CHECK(load_with_test_key(buf, n, &m) == PC_MF_ERR_BADSIG || load_with_test_key(buf, n, &m) == PC_MF_ERR_SIGFORMAT);
    /* wrong key only */
    n = sign_body(GOOD_BODY, buf, sizeof buf);
    memcpy(keys[0], other_pk, 32);
    CHECK(pc_manifest_load(buf, n, (const unsigned char (*)[32])keys, 1, &m, NULL, 0) == PC_MF_ERR_BADSIG);
    CHECK(m.version_str[0] == 0);   /* nothing leaks out on failure */
    /* data after the signature line */
    memcpy(buf + n, "x\n", 3);
    CHECK(load_with_test_key(buf, n + 2, &m) == PC_MF_ERR_NOSIG);
    /* unsigned */
    CHECK(load_with_test_key(GOOD_BODY, strlen(GOOD_BODY), &m) == PC_MF_ERR_NOSIG);
    CHECK(load_with_test_key("", 0, &m) == PC_MF_ERR_SIZE);
    CHECK(load_with_test_key("sig AAAA\n", 9, &m) == PC_MF_ERR_NOSIG);
    /* too large */
    {
        static char big[PC_MANIFEST_MAX_BYTES + 10];
        memset(big, 'a', sizeof big);
        CHECK(load_with_test_key(big, sizeof big, &m) == PC_MF_ERR_SIZE);
    }
    /* correctly signed but semantically bad */
    CHECK(load_variant("PCUPDATE 1\n", "PCUPDATE 2\n") == PC_MF_ERR_HEADER);
    CHECK(load_variant("product PhotoCraftClassic\n", "product SomethingElse\n") == PC_MF_ERR_PRODUCT);
    CHECK(load_variant("version 0.2.0\n", "version 0.2\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("version 0.2.0\n", "version 0.2.0\nversion 0.3.0\n") == PC_MF_ERR_DUP);
    CHECK(load_variant("sequence 7\n", "") == PC_MF_ERR_MISSING);
    CHECK(load_variant("sequence 7\n", "sequence 4294967296\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("sequence 7\n", "sequence 4294967295\n") == PC_MF_OK);
    CHECK(load_variant("sequence 7\n", "sequence -1\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("sequence 7\n", "sequence 07\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("released 2026-11-01\n", "released 2026-02-30\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("msi_url https://", "msi_url http://") == PC_MF_ERR_FIELD);
    CHECK(load_variant("msi_url https://github.com", "msi_url https://@github.com") == PC_MF_ERR_FIELD);
    CHECK(load_variant("msi_size 123456\n", "msi_size 0\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("msi_size 123456\n", "msi_size 999999999999\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("msi_sha256 00", "msi_sha256 0") == PC_MF_ERR_FIELD);
    CHECK(load_variant("msi_sha256 00", "msi_sha256 zz") == PC_MF_ERR_FIELD);
    CHECK(load_variant("channel stable\n", "channel Stable\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("channel stable\n", "channel  stable\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("channel stable\n", "channel\n") == PC_MF_ERR_FIELD);
    CHECK(load_variant("channel stable\n", "Channel stable\n") == PC_MF_ERR_SYNTAX);
    CHECK(load_variant("channel stable\n", "channel stable\n\n") == PC_MF_ERR_SYNTAX);
    CHECK(load_variant("channel stable\n", "channel sta\tble\n") == PC_MF_ERR_SYNTAX);
    CHECK(load_variant("channel stable\n", "channel st\rable\n") == PC_MF_ERR_SYNTAX);
    CHECK(load_variant("product PhotoCraftClassic\n", "product PhotoCraft\xc3\xa9\n") == PC_MF_ERR_FIELD);
    {
        char line[1100];
        memset(line, 'x', sizeof line);
        memcpy(line, "notes ", 6);
        line[1030] = '\n'; line[1031] = 0;
        CHECK(load_variant("notes First line.\n", line) == PC_MF_ERR_SYNTAX);
    }
    {
        static char many[8000];
        int i;
        many[0] = 0;
        for (i = 0; i < 70; i++) strcat(many, "notes x\n");
        CHECK(load_variant("notes First line.\n", many) == PC_MF_ERR_FIELD);
    }
}

static void test_policy(void)
{
    char buf[8192];
    pc_manifest m;
    size_t n = sign_body(GOOD_BODY, buf, sizeof buf);   /* 0.2.0, seq 7, stable */
    CHECK(load_with_test_key(buf, n, &m) == PC_MF_OK);
    CHECK(pc_manifest_policy(&m, "0.1.0", 0, "stable") == PC_POLICY_UPDATE);
    CHECK(pc_manifest_policy(&m, "0.1.0", 7, "stable") == PC_POLICY_UPDATE);   /* same sequence ok */
    CHECK(pc_manifest_policy(&m, "0.1.0", 8, "stable") == PC_POLICY_REPLAY);   /* rollback attack */
    CHECK(pc_manifest_policy(&m, "0.2.0", 0, "stable") == PC_POLICY_CURRENT);
    CHECK(pc_manifest_policy(&m, "0.3.0", 0, "stable") == PC_POLICY_CURRENT);  /* no downgrade */
    CHECK(pc_manifest_policy(&m, "0.2.0-rc.1", 0, "stable") == PC_POLICY_UPDATE);
    CHECK(pc_manifest_policy(&m, "0.1.0", 0, "beta") == PC_POLICY_WRONG_CHANNEL);
    CHECK(pc_manifest_policy(&m, "garbage", 0, "stable") == PC_POLICY_BAD_INSTALLED);
}

/* A manifest signed by tools/pcsign with the TEST key from pubkey.h:
 * proves pcsign output and the updater's verifier agree. */
static void test_pcsign_vector(void)
{
#if PC_UPDATE_PUBKEY_IS_TEST_KEY
    static const char v[] =
        "PCUPDATE 1\n"
        "product PhotoCraftClassic\n"
        "channel stable\n"
        "version 0.1.1\n"
        "sequence 1793318400\n"
        "released 2026-10-30\n"
        "msi_url https://github.com/knivesbatz/PhotoCraftr/releases/download/v0.1.1/PhotoCraftClassic-Setup.msi\n"
        "msi_size 3\n"
        "msi_sha256 ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
        "notes Test vector.\n"
        PCSIGN_VECTOR_SIG_LINE;
    pc_manifest m;
    char err[160];
    CHECK(pc_manifest_load(v, sizeof v - 1, pc_update_pubkeys, PC_UPDATE_NUM_PUBKEYS, &m, err, sizeof err) == PC_MF_OK);
    CHECK(m.sequence == 1793318400UL && m.msi_size == 3);
#endif
}

/* ---------- HTTP helpers ---------- */

static void test_urls(void)
{
    pc_url u, b;
    char out[PC_URL_MAX];
    CHECK(pc_url_parse_https("https://github.com/a/b?x=1#frag", &u) == 0 && !strcmp(u.host, "github.com") && u.port == 443 && !strcmp(u.path, "/a/b?x=1"));
    CHECK(pc_url_parse_https("HTTPS://GitHub.COM:8443", &u) == 0 && !strcmp(u.host, "github.com") && u.port == 8443 && !strcmp(u.path, "/"));
    CHECK(pc_url_parse_https("https://h?q", &u) == 0 && !strcmp(u.path, "/?q"));
    CHECK(pc_url_parse_https("http://github.com/", &u) == PC_URL_INSECURE);
    CHECK(pc_url_parse_https("ftp://github.com/", &u) == PC_URL_INSECURE);
    CHECK(pc_url_parse_https("https://user@github.com/", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https://[::1]/", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https://github.com:0/", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https://github.com:65536/", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https://github.com:/", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https:///path", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https://git hub.com/", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https://github.com/a b", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("https://github.com/\x01", &u) == PC_URL_BAD);
    CHECK(pc_url_parse_https("github.com", &u) == PC_URL_BAD);

    CHECK(pc_url_parse_https("https://github.com/o/r/releases/download/v1/x.msi", &b) == 0);
    CHECK(pc_url_resolve(&b, "https://objects.githubusercontent.com/x?sig=1", out, sizeof out) == 0 && !strcmp(out, "https://objects.githubusercontent.com/x?sig=1"));
    CHECK(pc_url_resolve(&b, "http://evil.example/x", out, sizeof out) == PC_URL_INSECURE);
    CHECK(pc_url_resolve(&b, "HTTP://evil.example/x", out, sizeof out) == PC_URL_INSECURE);
    CHECK(pc_url_resolve(&b, "javascript:alert(1)", out, sizeof out) == PC_URL_INSECURE);
    CHECK(pc_url_resolve(&b, "//cdn.example/y", out, sizeof out) == 0 && !strcmp(out, "https://cdn.example/y"));
    CHECK(pc_url_resolve(&b, "/other", out, sizeof out) == 0 && !strcmp(out, "https://github.com/other"));
    CHECK(pc_url_resolve(&b, "y.msi", out, sizeof out) == 0 && !strcmp(out, "https://github.com/o/r/releases/download/v1/y.msi"));
    CHECK(pc_url_resolve(&b, "", out, sizeof out) == PC_URL_BAD);
    CHECK(pc_url_resolve(&b, "/a b", out, sizeof out) == PC_URL_BAD);
    CHECK(pc_url_resolve(&b, "/x", out, 20) == PC_URL_BAD);   /* would truncate */
}

static void test_heads(void)
{
    pc_http_head h;
    const char *r1 = "HTTP/1.1 302 Found\r\nLocation: https://x.example/y\r\nContent-Length: 0\r\n\r\nBODY";
    const char *r2 = "HTTP/1.0 200 OK\nContent-Type: text/plain\ncontent-length:  42 \n\n";
    CHECK(pc_http_head_end(r1, strlen(r1)) == strlen(r1) - 4);
    CHECK(pc_http_head_end(r2, strlen(r2)) == strlen(r2));
    CHECK(pc_http_head_end("HTTP/1.1 200 OK\r\n", 17) == 0);
    CHECK(pc_http_parse_head(r1, strlen(r1) - 4, &h) == 0 && h.status == 302 && !strcmp(h.location, "https://x.example/y") && h.content_length == 0);
    CHECK(pc_http_parse_head(r2, strlen(r2), &h) == 0 && h.status == 200 && h.content_length == 42 && !h.chunked);
#define BADHEAD(s) CHECK(pc_http_parse_head(s, strlen(s), &h) == -1)
    BADHEAD("HTTP/2 200 OK\r\n\r\n");
    BADHEAD("HTTP/1.1 20 OK\r\n\r\n");
    BADHEAD("HTTP/1.1 200OK\r\n\r\n");
    BADHEAD("ICY 200 OK\r\n\r\n");
    BADHEAD("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n");
    BADHEAD("HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n");
    BADHEAD("HTTP/1.1 200 OK\r\nContent-Length: 99999999999\r\n\r\n");
    BADHEAD("HTTP/1.1 200 OK\r\nContent-Length: 1 2\r\n\r\n");
    BADHEAD("HTTP/1.1 200 OK\r\nX: a\r\n folded\r\n\r\n");
    BADHEAD("HTTP/1.1 200 OK\r\nNoColon\r\n\r\n");
    BADHEAD("HTTP/1.1 302 Found\r\nLocation: /a\r\nLocation: /b\r\n\r\n");
    CHECK(pc_http_parse_head("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\n", 57, &h) == 0 && h.content_length == 5);
    {
        const char *c = "HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, Chunked\r\n\r\n";
        CHECK(pc_http_parse_head(c, strlen(c), &h) == 0 && h.chunked == 1);
    }
    {
        static char longloc[PC_URL_MAX + 100];
        memset(longloc, 0, sizeof longloc);
        strcpy(longloc, "HTTP/1.1 302 F\r\nLocation: https://a/");
        memset(longloc + strlen(longloc), 'a', PC_URL_MAX);
        strcat(longloc, "\r\n\r\n");
        CHECK(pc_http_parse_head(longloc, strlen(longloc), &h) == -1);
    }
    {
        char req[2048];
        pc_url u;
        pc_url_parse_https("https://github.com/x", &u);
        CHECK(pc_http_build_get(req, sizeof req, &u, "UA/1") > 0 && !strncmp(req, "GET /x HTTP/1.0\r\nHost: github.com\r\n", 35));
        CHECK(pc_http_build_get(req, 20, &u, "UA/1") == -1);
        CHECK(pc_http_build_connect(req, sizeof req, "github.com", 443, "UA/1") > 0 && !strncmp(req, "CONNECT github.com:443 HTTP/1.0\r\n", 33));
    }
}

static void test_proxy(void)
{
    char host[256];
    unsigned short port = 0;
    unsigned char ip[4];
    CHECK(pc_proxy_parse("proxy.corp:8080", host, sizeof host, &port) == 0 && !strcmp(host, "proxy.corp") && port == 8080);
    CHECK(pc_proxy_parse("proxy.corp", host, sizeof host, &port) == 0 && port == 80);
    CHECK(pc_proxy_parse("http://127.0.0.1:44825", host, sizeof host, &port) == 0 && !strcmp(host, "127.0.0.1") && port == 44825);
    CHECK(pc_proxy_parse("http://127.0.0.1:44825/", host, sizeof host, &port) == 0 && port == 44825);
    CHECK(pc_proxy_parse("http=a:1;https=b.example:3128;ftp=c:2", host, sizeof host, &port) == 0 && !strcmp(host, "b.example") && port == 3128);
    CHECK(pc_proxy_parse("http=a:1;ftp=c:2", host, sizeof host, &port) == -1);
    CHECK(pc_proxy_parse("socks://a:1", host, sizeof host, &port) == -1);
    CHECK(pc_proxy_parse("", host, sizeof host, &port) == -1);
    CHECK(pc_proxy_parse("a:99999", host, sizeof host, &port) == -1);
    CHECK(pc_proxy_bypass("<local>;*.corp.example", "intranet") == 1);
    CHECK(pc_proxy_bypass("<local>;*.corp.example", "github.com") == 0);
    CHECK(pc_proxy_bypass("<local>;*.corp.example", "x.CORP.example") == 1);
    CHECK(pc_proxy_bypass("*github*", "objects.githubusercontent.com") == 1);
    CHECK(pc_proxy_bypass("", "github.com") == 0);
    CHECK(pc_parse_ipv4("127.0.0.1", ip) == 0 && ip[0] == 127 && ip[3] == 1);
    CHECK(pc_parse_ipv4("256.0.0.1", ip) == -1);
    CHECK(pc_parse_ipv4("1.2.3", ip) == -1);
    CHECK(pc_parse_ipv4("1.2.3.4.5", ip) == -1);
    CHECK(pc_parse_ipv4("github.com", ip) == -1);
}

/* ---------- fuzz (deterministic) ---------- */

static unsigned long fz_state = 12345;
static unsigned fz_rand(void) { fz_state = fz_state * 1103515245UL + 12345UL; return (unsigned)(fz_state >> 16) & 0x7FFF; }

static void mutate(char *buf, size_t *len, size_t cap)
{
    int k, nm = 1 + (int)(fz_rand() % 8);
    static const char interesting[] = "\n\r \t=:/.-0123456789aAzZ\x80\xff";
    for (k = 0; k < nm; k++) {
        size_t pos = *len ? fz_rand() % *len : 0;
        switch (fz_rand() % 6) {
        case 0: if (*len) buf[pos] = (char)fz_rand(); break;
        case 1: if (*len) buf[pos] = interesting[fz_rand() % (sizeof interesting - 1)]; break;
        case 2: if (*len) { memmove(buf + pos, buf + pos + 1, *len - pos - 1); (*len)--; } break;
        case 3: if (*len + 1 < cap) { memmove(buf + pos + 1, buf + pos, *len - pos); buf[pos] = interesting[fz_rand() % (sizeof interesting - 1)]; (*len)++; } break;
        case 4: *len = pos; break;
        case 5: if (*len && *len * 2 < cap) { size_t l = fz_rand() % (*len - pos + 1); if (*len + l < cap) { memmove(buf + pos + l, buf + pos, *len - pos); memcpy(buf + pos, buf, l > pos ? pos : l); *len += l; } } break;
        }
    }
}

static void test_fuzz(void)
{
    static char seedbuf[9000], work[20000], body[20000];
    pc_manifest m;
    pc_http_head h;
    pc_url u, b;
    char out[PC_URL_MAX], host[256];
    unsigned short port;
    size_t seedlen = sign_body(GOOD_BODY, seedbuf, sizeof seedbuf), len;
    int i, accepted = 0;
    static const char *heads[] = {
        "HTTP/1.1 302 Found\r\nLocation: https://x.example/y\r\nContent-Length: 0\r\n\r\n",
        "HTTP/1.0 200 OK\nTransfer-Encoding: chunked\ncontent-length: 42\n\n",
    };
    pc_url_parse_https("https://github.com/o/r/releases/download/v1/x.msi", &b);

    for (i = 0; i < 30000; i++) {
        /* signed manifest: must never crash; any accepted mutant must still
         * be byte-identical in its signed part (i.e. only trailing-whitespace
         * mutations after the signature are acceptable). */
        memcpy(work, seedbuf, seedlen);
        len = seedlen;
        mutate(work, &len, sizeof work);
        if (load_with_test_key(work, len, &m) == PC_MF_OK) {
            accepted++;
            CHECK(m.signed_len == strlen(GOOD_BODY) && !memcmp(work, GOOD_BODY, m.signed_len));
        }
        /* unsigned body parser directly (bypassing the signature) */
        memcpy(body, GOOD_BODY, strlen(GOOD_BODY));
        len = strlen(GOOD_BODY);
        mutate(body, &len, sizeof body);
        if (pc_manifest_parse_body(body, len, &m, out, sizeof out) == PC_MF_OK) {
            CHECK(strlen(m.notes) < sizeof m.notes && strlen(m.msi_url) < sizeof m.msi_url);
            CHECK(!strncmp(m.msi_url, "https://", 8));
        }
        /* HTTP heads */
        len = strlen(heads[i & 1]);
        memcpy(work, heads[i & 1], len);
        mutate(work, &len, 4000);
        if (pc_http_parse_head(work, len, &h) == 0) {
            CHECK(strlen(h.location) < sizeof h.location);
            CHECK(h.content_length >= -1);
        }
        (void)pc_http_head_end(work, len);
        /* URLs and redirects */
        len = strlen("https://objects.githubusercontent.com:443/a/b?x=1#f");
        memcpy(work, "https://objects.githubusercontent.com:443/a/b?x=1#f", len);
        mutate(work, &len, 3000);
        work[len] = 0;
        if (pc_url_parse_https(work, &u) == 0) CHECK(u.path[0] == '/' && u.port != 0 && strlen(u.host) > 0);
        if (pc_url_resolve(&b, work, out, sizeof out) == 0) CHECK(!strncmp(out, "https://", 8));
        /* proxy strings */
        len = strlen("http=a:1;https=b.example:3128;ftp=c:2");
        memcpy(work, "http=a:1;https=b.example:3128;ftp=c:2", len);
        mutate(work, &len, 2000);
        work[len] = 0;
        if (pc_proxy_parse(work, host, sizeof host, &port) == 0) CHECK(port != 0 && strlen(host) > 0);
        (void)pc_proxy_bypass(work, "github.com");
    }
    /* pure garbage */
    for (i = 0; i < 5000; i++) {
        size_t k;
        len = fz_rand() % 3000;
        for (k = 0; k < len; k++) work[k] = (char)fz_rand();
        CHECK(load_with_test_key(work, len, &m) != PC_MF_OK);
        (void)pc_manifest_parse_body(work, len, &m, out, sizeof out);
        (void)pc_http_parse_head(work, len, &h);
    }
    (void)accepted;
}

int test_updater(void)
{
    tu_fail = 0;
    make_keys();
    test_encodings();
    test_dates();
    test_versions();
    test_manifest_good();
    test_manifest_bad();
    test_policy();
    test_pcsign_vector();
    test_urls();
    test_heads();
    test_proxy();
    test_fuzz();
    fprintf(stderr, "test_updater: %s (%d failure%s)\n", tu_fail ? "FAILED" : "ok", tu_fail, tu_fail == 1 ? "" : "s");
    return tu_fail;
}

#ifdef PC_TEST_UPDATER_MAIN
int g_test_failures, g_test_checks;
int main(void) { return test_updater() ? 1 : 0; }
#endif
