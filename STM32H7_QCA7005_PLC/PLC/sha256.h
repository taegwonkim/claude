/*
 * sha256.h - minimal SHA-256 (FIPS 180-4), used to derive the NID from the NMK
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef PLC_SHA256_H
#define PLC_SHA256_H

#include <stdint.h>
#include <stddef.h>

#define SHA256_DIGEST_LEN 32u

typedef struct {
    uint32_t h[8];
    uint64_t bits;
    uint8_t  buf[64];
    uint32_t used;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *c);
void sha256_update(sha256_ctx_t *c, const void *data, size_t len);
void sha256_final(sha256_ctx_t *c, uint8_t digest[SHA256_DIGEST_LEN]);
void sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_LEN]);

#endif
