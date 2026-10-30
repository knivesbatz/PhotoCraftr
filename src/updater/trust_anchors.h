/*
 * Root CAs bundled with PCUpdate.exe (generated into trust_anchors.c by
 * tools/trust/regen-trust-anchors.sh).  The updater never uses the
 * operating system's certificate store: Windows 95 has none that BearSSL
 * could use, and old stores lack every modern root anyway.
 */
#ifndef PC_TRUST_ANCHORS_H
#define PC_TRUST_ANCHORS_H

#include <stddef.h>
#include <bearssl.h>

extern const br_x509_trust_anchor *const pc_trust_anchors;
extern const size_t pc_trust_anchors_num;

#endif
