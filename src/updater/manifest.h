/*
 * PhotoCraft Classic updater - signed update manifest (portable C, no Win32).
 *
 * Format (full specification in docs/updater.md, "Manifest format"):
 *
 *   PCUPDATE 1
 *   product PhotoCraftClassic
 *   channel stable
 *   version 0.2.0
 *   sequence 7
 *   released 2026-11-01
 *   msi_url https://github.com/.../PhotoCraftClassic-Setup.msi
 *   msi_size 123456
 *   msi_sha256 <64 hex digits>
 *   notes <one line; may repeat>
 *   sig <base64 Ed25519 signature over every byte before this line>
 *
 * pc_manifest_load() first checks the Ed25519 signature over the raw bytes
 * and only then parses the signed body, so unauthenticated input only ever
 * reaches the (tiny) signature-line splitter and base64 decoder.  Every
 * function here is bounded and never reads outside the given buffer.
 */
#ifndef PC_MANIFEST_H
#define PC_MANIFEST_H

#include <stddef.h>

#define PC_MANIFEST_MAX_BYTES   16384   /* whole file, signature included   */
#define PC_MANIFEST_MAX_LINE    1024    /* one line, terminator excluded     */
#define PC_MANIFEST_URL_MAX     512
#define PC_MANIFEST_NOTES_MAX   4096    /* joined notes incl. CRLFs and NUL  */
#define PC_MANIFEST_NOTES_LINES 64
#define PC_MANIFEST_MSI_MAX     (512UL * 1024UL * 1024UL)  /* 512 MB cap     */
#define PC_PRODUCT_ID           "PhotoCraftClassic"

/* Error codes (negative); PC_MF_OK == 0. */
enum {
    PC_MF_OK = 0,
    PC_MF_ERR_ARG = -1,         /* NULL pointer etc.                          */
    PC_MF_ERR_SIZE = -2,        /* empty or larger than PC_MANIFEST_MAX_BYTES */
    PC_MF_ERR_NOSIG = -3,       /* no "sig " line / trailing data after it    */
    PC_MF_ERR_SIGFORMAT = -4,   /* signature is not 64 bytes of base64        */
    PC_MF_ERR_BADSIG = -5,      /* signature does not verify with any key     */
    PC_MF_ERR_SYNTAX = -6,      /* bad line, bad character, too long ...      */
    PC_MF_ERR_HEADER = -7,      /* first line is not "PCUPDATE 1"             */
    PC_MF_ERR_FIELD = -8,       /* a field value is malformed                 */
    PC_MF_ERR_DUP = -9,         /* a single-valued field appears twice        */
    PC_MF_ERR_MISSING = -10,    /* a required field is missing                */
    PC_MF_ERR_PRODUCT = -11     /* signed, but for another product            */
};

/* Semantic version MAJOR.MINOR.PATCH[-PRERELEASE] (no build metadata).
 * MAJOR and MINOR are limited to 0..255 and PATCH to 0..65535 because the
 * MSI ProductVersion has those limits. */
typedef struct {
    unsigned major, minor, patch;
    char pre[32];               /* "" for a release                         */
} pc_version;

typedef struct {
    int format;                         /* 1                                 */
    char product[64];
    char channel[32];
    char version_str[48];
    pc_version version;
    unsigned long sequence;             /* 0 .. 4294967295                   */
    char released[11];                  /* YYYY-MM-DD                         */
    char msi_url[PC_MANIFEST_URL_MAX];
    unsigned long msi_size;
    unsigned char msi_sha256[32];
    char notes[PC_MANIFEST_NOTES_MAX];  /* lines joined with CRLF            */
    size_t signed_len;                  /* bytes covered by the signature    */
    int key_index;                      /* which pinned key verified it      */
} pc_manifest;

/* Parse + authenticate.  'keys' is an array of 'nkeys' 32-byte Ed25519
 * public keys (normally pc_update_pubkeys from pubkey.h); the manifest must
 * verify with one of them.  On failure returns a PC_MF_ERR_* code and, if
 * err != NULL, a short English description. */
int pc_manifest_load(const char *buf, size_t len,
                     const unsigned char (*keys)[32], int nkeys,
                     pc_manifest *m, char *err, size_t errlen);

/* Locate the signature line.  On success *body_len is the number of signed
 * bytes (offset of the "sig " line) and sig receives the decoded signature. */
int pc_manifest_split(const char *buf, size_t len, size_t *body_len, unsigned char sig[64]);

/* Parse the signed body (everything before the "sig " line) WITHOUT any
 * authentication.  Exposed for tests and tools; the updater never trusts a
 * manifest that did not pass pc_manifest_load(). */
int pc_manifest_parse_body(const char *body, size_t len, pc_manifest *m, char *err, size_t errlen);

/* Policy.  Given an authenticated manifest, decide whether to offer it. */
enum {
    PC_POLICY_UPDATE = 1,       /* newer version, sequence ok: offer it         */
    PC_POLICY_CURRENT = 0,      /* not newer than what is installed             */
    PC_POLICY_REPLAY = -1,      /* sequence older than one already seen         */
    PC_POLICY_WRONG_CHANNEL = -2,
    PC_POLICY_BAD_INSTALLED = -3 /* installed version string unparsable        */
};
int pc_manifest_policy(const pc_manifest *m, const char *installed_version,
                       unsigned long last_seen_sequence, const char *channel);

/* Versions.  pc_version_parse returns 0 on success, -1 if malformed.
 * pc_version_cmp follows semver precedence: <0, 0, >0. */
int pc_version_parse(const char *s, pc_version *v);
int pc_version_cmp(const pc_version *a, const pc_version *b);
/* Convenience: compare two version strings; returns -2 if either is invalid. */
int pc_version_cmp_str(const char *a, const char *b);

/* Hex / base64 helpers (strict).  Return number of bytes decoded, -1 on error. */
int pc_hex_decode(const char *s, size_t slen, unsigned char *out, size_t outmax);
void pc_hex_encode(const unsigned char *in, size_t n, char *out /* 2n+1 */);
int pc_base64_decode(const char *s, size_t slen, unsigned char *out, size_t outmax);

/* SHA-256 (BearSSL br_sha256_*). */
void pc_sha256(const void *data, size_t len, unsigned char out[32]);

/* Days since 0000-01-01 (proleptic Gregorian), the convention used by
 * br_x509_minimal_set_time(); 1970-01-01 -> 719528.  Returns -1 for an
 * invalid date. */
long pc_days_from_civil(int year, int month, int day);

/* Human-readable text for an error code. */
const char *pc_manifest_strerror(int code);

#endif
