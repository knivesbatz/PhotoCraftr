/*
 * Pinned Ed25519 public key(s) that update manifests must be signed with.
 *
 * !!! TEST KEY !!!  The key below was generated with `pcsign keygen` for
 * development; its secret half is NOT in this repository.  Replace it with
 * the release key before shipping (docs/release.md, "Signing key").
 *
 * More than one key may be listed: a manifest is accepted if it verifies
 * with ANY of them.  Key rotation = ship a release that lists old + new key,
 * start signing with the new key, later drop the old one.
 */
#ifndef PC_UPDATE_PUBKEY_H
#define PC_UPDATE_PUBKEY_H

#define PC_UPDATE_PUBKEY_IS_TEST_KEY 1

static const unsigned char pc_update_pubkeys[][32] = {
    /* test key 2026-10-30: 4ffffa0e8f62271c602499c6521db25d890897124e0675ebabdfef1a05574f5c */
    {
        0x4f, 0xff, 0xfa, 0x0e, 0x8f, 0x62, 0x27, 0x1c,
        0x60, 0x24, 0x99, 0xc6, 0x52, 0x1d, 0xb2, 0x5d,
        0x89, 0x08, 0x97, 0x12, 0x4e, 0x06, 0x75, 0xeb,
        0xab, 0xdf, 0xef, 0x1a, 0x05, 0x57, 0x4f, 0x5c
    }
};
#define PC_UPDATE_NUM_PUBKEYS ((int)(sizeof pc_update_pubkeys / sizeof pc_update_pubkeys[0]))

#endif
