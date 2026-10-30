/*
 * PhotoCraft Classic updater - portable HTTP helpers (no sockets, no Win32):
 * https URL parsing, redirect resolution, HTTP response head parsing, and
 * proxy-setting parsing.  Everything here handles hostile input: all
 * functions are bounded by explicit lengths and never write past their
 * output buffers.
 */
#ifndef PC_HTTPPARSE_H
#define PC_HTTPPARSE_H

#include <stddef.h>

#define PC_URL_HOST_MAX 256
#define PC_URL_PATH_MAX 4096    /* signed CDN redirect URLs can be long */
#define PC_URL_MAX      (PC_URL_HOST_MAX + PC_URL_PATH_MAX + 16)
#define PC_HTTP_HEAD_MAX 16384

typedef struct {
    char host[PC_URL_HOST_MAX];     /* lower-case DNS name or dotted IPv4 */
    unsigned short port;            /* 443 unless given                  */
    char path[PC_URL_PATH_MAX];     /* starts with '/', fragment removed */
} pc_url;

enum {
    PC_URL_OK = 0,
    PC_URL_BAD = -1,        /* malformed / too long                         */
    PC_URL_INSECURE = -2    /* http:// or another non-https scheme          */
};

/* Parse an absolute https:// URL.  No user-info, no IPv6 literals. */
int pc_url_parse_https(const char *url, pc_url *u);

/* Resolve a redirect Location against the URL that produced it.  The result
 * is an absolute https:// URL written to out (NUL-terminated).  Returns
 * PC_URL_INSECURE for http:// (or other scheme) targets. */
int pc_url_resolve(const pc_url *base, const char *location, char *out, size_t outsz);

/* Format a pc_url back to "https://host[:port]/path". */
void pc_url_format(const pc_url *u, char *out, size_t outsz);

typedef struct {
    int status;                 /* e.g. 200                              */
    int minor_version;          /* HTTP/1.x                              */
    long content_length;        /* -1 if absent                          */
    int chunked;                /* Transfer-Encoding contains "chunked"  */
    char location[PC_URL_MAX];  /* "" if absent                          */
} pc_http_head;

/* Find the end of the response head ("\r\n\r\n" or "\n\n").  Returns the
 * number of bytes of the head including the blank line, or 0 if not yet
 * complete. */
size_t pc_http_head_end(const char *buf, size_t len);

/* Parse a complete response head (as delimited by pc_http_head_end).
 * Returns 0, or -1 if it is malformed (bad status line, conflicting or
 * invalid Content-Length, folded headers, NUL bytes, overlong Location). */
int pc_http_parse_head(const char *buf, size_t len, pc_http_head *h);

/* Build the request texts.  Return the length, or -1 if it does not fit. */
int pc_http_build_get(char *out, size_t outsz, const pc_url *u, const char *user_agent);
int pc_http_build_connect(char *out, size_t outsz, const char *host, unsigned short port, const char *user_agent);

/* Proxy settings.
 * pc_proxy_parse understands Internet Explorer's ProxyServer value
 * ("host:port" for all protocols, or "http=h:p;https=h:p;ftp=..."), as well
 * as "http://host:port/" (the HTTPS_PROXY environment variable form).  It
 * picks the entry used for https.  Returns 0 and fills host/port, or -1 if
 * there is no usable proxy for https. */
int pc_proxy_parse(const char *value, char *host, size_t hostsz, unsigned short *port);

/* IE ProxyOverride bypass list: entries separated by ';' (or spaces/commas),
 * '*' wildcards, case-insensitive; "<local>" matches names without a dot.
 * Returns 1 if 'host' should be contacted directly. */
int pc_proxy_bypass(const char *override_list, const char *host);

/* Parse dotted IPv4 "a.b.c.d" (decimal only).  Returns 0 and the address in
 * network byte order as 4 bytes, -1 if not an IPv4 literal. */
int pc_parse_ipv4(const char *s, unsigned char out[4]);

#endif
