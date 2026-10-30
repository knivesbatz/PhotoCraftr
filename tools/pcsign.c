/*
 * pcsign - Ed25519 signing tool for PhotoCraft Classic update manifests.
 * Host tool (Linux/macOS), built by `make build/native/pcsign`.
 *
 *   pcsign keygen <secretfile> <pubfile>   new key pair; prints the C array
 *                                          for src/updater/pubkey.h
 *   pcsign pubkey [<secretfile>|-]         print public key of a secret key
 *   pcsign sign [<secretfile>|-] <unsigned-manifest> > signed-manifest
 *   pcsign verify <pubhex|pubfile> <signed-manifest>
 *
 * The secret file holds the 32-byte Ed25519 seed as 64 hex digits.  Instead
 * of a file, "-" (or omitting the file for sign/pubkey) reads the hex seed
 * from the environment variable PC_UPDATE_SIGNING_KEY (used by CI).
 *
 * The signature covers every byte of the unsigned manifest exactly as given;
 * the output is that manifest followed by one line "sig <base64>\n".
 * See docs/updater.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <monocypher/monocypher.h>
#include <monocypher/optional/monocypher-ed25519.h>

#define MAX_MANIFEST 16384

static void die(const char *msg)
{
    fprintf(stderr, "pcsign: %s\n", msg);
    exit(1);
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse exactly 2*n hex digits, surrounding whitespace allowed. */
static int parse_hex(const char *s, unsigned char *out, size_t n)
{
    size_t i = 0;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    for (i = 0; i < n; i++) {
        int hi = hexval((unsigned char)s[2 * i]), lo;
        if (hi < 0) return -1;
        lo = hexval((unsigned char)s[2 * i + 1]);
        if (lo < 0) return -1;
        out[i] = (unsigned char)(hi << 4 | lo);
    }
    s += 2 * n;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    return *s ? -1 : 0;
}

static void print_hex(FILE *f, const unsigned char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) fprintf(f, "%02x", b[i]);
}

static size_t read_file(const char *path, char *buf, size_t max)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) { fprintf(stderr, "pcsign: cannot open %s: %s\n", path, strerror(errno)); exit(1); }
    n = fread(buf, 1, max, f);
    if (n == max && fgetc(f) != EOF) die("input file too large");
    fclose(f);
    return n;
}

static void load_seed(const char *path, unsigned char seed[32])
{
    char buf[256];
    if (!path || !strcmp(path, "-")) {
        const char *e = getenv("PC_UPDATE_SIGNING_KEY");
        if (!e || !*e) die("no secret key file given and PC_UPDATE_SIGNING_KEY is not set");
        if (strlen(e) >= sizeof buf) die("PC_UPDATE_SIGNING_KEY is malformed");
        strcpy(buf, e);
    } else {
        size_t n = read_file(path, buf, sizeof buf - 1);
        buf[n] = 0;
    }
    if (parse_hex(buf, seed, 32)) die("secret key must be 64 hex digits (the Ed25519 seed)");
    crypto_wipe(buf, sizeof buf);
}

static void keypair_from_seed(unsigned char seed[32], unsigned char sk[64], unsigned char pk[32])
{
    unsigned char tmp[32];
    memcpy(tmp, seed, 32);
    crypto_ed25519_key_pair(sk, pk, tmp);   /* wipes tmp */
}

static void print_c_array(const unsigned char pk[32])
{
    int i;
    printf("/* paste into src/updater/pubkey.h */\n{\n    ");
    for (i = 0; i < 32; i++) {
        printf("0x%02x%s", pk[i], i == 31 ? "" : ",");
        if (i % 8 == 7 && i != 31) printf("\n    ");
        else if (i != 31) printf(" ");
    }
    printf("\n}\n");
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode(const unsigned char *in, size_t n, char *out)
{
    size_t i, o = 0;
    for (i = 0; i + 2 < n; i += 3) {
        unsigned long w = (unsigned long)in[i] << 16 | (unsigned long)in[i + 1] << 8 | in[i + 2];
        out[o++] = B64[w >> 18 & 63]; out[o++] = B64[w >> 12 & 63];
        out[o++] = B64[w >> 6 & 63];  out[o++] = B64[w & 63];
    }
    if (n - i == 1) {
        unsigned long w = (unsigned long)in[i] << 16;
        out[o++] = B64[w >> 18 & 63]; out[o++] = B64[w >> 12 & 63]; out[o++] = '='; out[o++] = '=';
    } else if (n - i == 2) {
        unsigned long w = (unsigned long)in[i] << 16 | (unsigned long)in[i + 1] << 8;
        out[o++] = B64[w >> 18 & 63]; out[o++] = B64[w >> 12 & 63]; out[o++] = B64[w >> 6 & 63]; out[o++] = '=';
    }
    out[o] = 0;
}

static int b64_decode64(const char *s, size_t n, unsigned char out[64])
{
    size_t i, o = 0;
    if (n != 88 || s[86] != '=' || s[87] != '=') return -1;
    for (i = 0; i < 88; i += 4) {
        int k, v[4];
        unsigned long w;
        for (k = 0; k < 4; k++) {
            const char *p;
            if (s[i + k] == '=' && i == 84 && k >= 2) { v[k] = 0; continue; }
            p = s[i + k] ? strchr(B64, s[i + k]) : NULL;
            if (!p) return -1;
            v[k] = (int)(p - B64);
        }
        w = (unsigned long)v[0] << 18 | (unsigned long)v[1] << 12 | (unsigned long)v[2] << 6 | (unsigned long)v[3];
        if (o < 64) out[o++] = (unsigned char)(w >> 16);
        if (o < 64 && i < 84) out[o++] = (unsigned char)(w >> 8);
        if (o < 64 && i < 84) out[o++] = (unsigned char)w;
        if (i == 84 && (w & 0xFFFF)) return -1;
    }
    return o == 64 ? 0 : -1;
}

/* Returns offset of the first line starting with "sig ", or -1. */
static long find_sig_line(const char *buf, size_t n)
{
    size_t pos = 0;
    while (pos < n) {
        const char *nl;
        if (pos + 4 <= n && !memcmp(buf + pos, "sig ", 4)) return (long)pos;
        nl = memchr(buf + pos, '\n', n - pos);
        if (!nl) break;
        pos = (size_t)(nl - buf) + 1;
    }
    return -1;
}

static int cmd_keygen(const char *secpath, const char *pubpath)
{
    unsigned char seed[32], sk[64], pk[32];
    FILE *f;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0 || read(fd, seed, 32) != 32) die("cannot read /dev/urandom");
    close(fd);
    keypair_from_seed(seed, sk, pk);

    fd = open(secpath, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { fprintf(stderr, "pcsign: cannot create %s (exists?): %s\n", secpath, strerror(errno)); return 1; }
    f = fdopen(fd, "w");
    print_hex(f, seed, 32); fputc('\n', f);
    if (fclose(f)) die("write error (secret key)");
    f = fopen(pubpath, "w");
    if (!f) die("cannot create public key file");
    print_hex(f, pk, 32); fputc('\n', f);
    if (fclose(f)) die("write error (public key)");

    printf("public key: "); print_hex(stdout, pk, 32); printf("\n");
    print_c_array(pk);
    crypto_wipe(seed, sizeof seed); crypto_wipe(sk, sizeof sk);
    return 0;
}

static int cmd_pubkey(const char *secpath)
{
    unsigned char seed[32], sk[64], pk[32];
    load_seed(secpath, seed);
    keypair_from_seed(seed, sk, pk);
    print_hex(stdout, pk, 32); printf("\n");
    print_c_array(pk);
    crypto_wipe(seed, sizeof seed); crypto_wipe(sk, sizeof sk);
    return 0;
}

static int cmd_sign(const char *secpath, const char *path)
{
    static char buf[MAX_MANIFEST + 2];
    unsigned char seed[32], sk[64], pk[32], sig[64];
    char b64[100];
    size_t n = read_file(path, buf, MAX_MANIFEST - 100);
    if (n < 11 || memcmp(buf, "PCUPDATE 1", 10) || (buf[10] != '\n' && buf[10] != '\r'))
        die("input does not start with 'PCUPDATE 1'");
    if (memchr(buf, 0, n)) die("input contains NUL bytes");
    if (find_sig_line(buf, n) >= 0) die("input already contains a 'sig ' line");
    if (buf[n - 1] != '\n') buf[n++] = '\n';

    load_seed(secpath, seed);
    keypair_from_seed(seed, sk, pk);
    crypto_ed25519_sign(sig, sk, (const unsigned char *)buf, n);
    if (crypto_ed25519_check(sig, pk, (const unsigned char *)buf, n)) die("self-check of signature failed");
    crypto_wipe(seed, sizeof seed); crypto_wipe(sk, sizeof sk);

    b64_encode(sig, 64, b64);
    fwrite(buf, 1, n, stdout);
    printf("sig %s\n", b64);
    fprintf(stderr, "pcsign: signed %lu bytes with key ", (unsigned long)n);
    print_hex(stderr, pk, 32);
    fprintf(stderr, "\n");
    return fflush(stdout) ? 1 : 0;
}

static int cmd_verify(const char *pubarg, const char *path)
{
    static char buf[MAX_MANIFEST + 1];
    unsigned char pk[32], sig[64];
    char pbuf[256];
    size_t n, end;
    long off;
    if (parse_hex(pubarg, pk, 32)) {
        size_t k = read_file(pubarg, pbuf, sizeof pbuf - 1);
        pbuf[k] = 0;
        if (parse_hex(pbuf, pk, 32)) die("public key must be 64 hex digits or a file containing them");
    }
    n = read_file(path, buf, MAX_MANIFEST);
    off = find_sig_line(buf, n);
    if (off <= 0) die("no 'sig ' line found");
    end = n;
    if (end > (size_t)off && buf[end - 1] == '\n') end--;
    if (end > (size_t)off && buf[end - 1] == '\r') end--;
    if (b64_decode64(buf + off + 4, end - (size_t)off - 4, sig)) die("malformed signature line (must be the last line)");
    if (crypto_ed25519_check(sig, pk, (const unsigned char *)buf, (size_t)off)) {
        fprintf(stderr, "pcsign: BAD signature\n");
        return 2;
    }
    fprintf(stderr, "pcsign: signature OK (%ld signed bytes)\n", off);
    return 0;
}

static void usage(void)
{
    fprintf(stderr,
        "usage: pcsign keygen <secretfile> <pubfile>\n"
        "       pcsign pubkey [<secretfile>|-]\n"
        "       pcsign sign [<secretfile>|-] <unsigned-manifest> > signed\n"
        "       pcsign verify <pubhex|pubfile> <signed-manifest>\n"
        "  '-' or an omitted secret file means: hex seed from $PC_UPDATE_SIGNING_KEY\n");
    exit(64);
}

int main(int argc, char **argv)
{
    if (argc < 2) usage();
    if (!strcmp(argv[1], "keygen") && argc == 4) return cmd_keygen(argv[2], argv[3]);
    if (!strcmp(argv[1], "pubkey") && argc <= 3) return cmd_pubkey(argc == 3 ? argv[2] : NULL);
    if (!strcmp(argv[1], "sign") && argc == 3) return cmd_sign(NULL, argv[2]);
    if (!strcmp(argv[1], "sign") && argc == 4) return cmd_sign(argv[2], argv[3]);
    if (!strcmp(argv[1], "verify") && argc == 4) return cmd_verify(argv[2], argv[3]);
    usage();
    return 64;
}
