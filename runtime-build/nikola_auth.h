/*
 * NikolaChess Authentication Client
 * Copyright (c) 2025-2026 STARGA, Inc. All rights reserved.
 *
 * Include this in NikolaChess to authenticate with the MIND runtime.
 * The runtime will NOT work without proper authentication.
 *
 * Usage:
 *   #include "nikola_auth.h"
 *
 *   int main() {
 *       // Authenticate with runtime before any operations
 *       if (nikola_auth_runtime() != 0) {
 *           return 1; // Runtime rejected us
 *       }
 *       // Now runtime functions work...
 *   }
 */

#ifndef NIKOLA_AUTH_H
#define NIKOLA_AUTH_H

#include <stdint.h>
#include <string.h>

/* External runtime functions (from libmind_*.so) */
extern uint64_t mind_auth_get_challenge(void);
extern int mind_auth_verify(uint64_t response);

/*
 * SECRET KEY - Must match the runtime's key exactly!
 * XOR-obfuscated to prevent easy extraction
 * CHANGE THIS for each release and keep in sync with protection.h
 */
static const uint8_t _nikola_key_enc[32] = {
    0x4D, 0x31, 0x4E, 0x44, 0x5F, 0x52, 0x55, 0x4E,
    0x54, 0x31, 0x4D, 0x45, 0x5F, 0x53, 0x45, 0x43,
    0x52, 0x45, 0x54, 0x5F, 0x4B, 0x45, 0x59, 0x5F,
    0x32, 0x30, 0x32, 0x36, 0x5F, 0x56, 0x31, 0x00
};
static const uint8_t _nikola_key_xor = 0x1F;

/* SipHash-2-4 implementation (must match runtime) */
static inline uint64_t _nikola_rotl64(uint64_t x, int b) {
    return (x << b) | (x >> (64 - b));
}

static uint64_t _nikola_siphash(const uint8_t *data, size_t len, const uint8_t key[16]) {
    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;

    uint64_t k0 = ((uint64_t)key[0]) | ((uint64_t)key[1] << 8) |
                  ((uint64_t)key[2] << 16) | ((uint64_t)key[3] << 24) |
                  ((uint64_t)key[4] << 32) | ((uint64_t)key[5] << 40) |
                  ((uint64_t)key[6] << 48) | ((uint64_t)key[7] << 56);
    uint64_t k1 = ((uint64_t)key[8]) | ((uint64_t)key[9] << 8) |
                  ((uint64_t)key[10] << 16) | ((uint64_t)key[11] << 24) |
                  ((uint64_t)key[12] << 32) | ((uint64_t)key[13] << 40) |
                  ((uint64_t)key[14] << 48) | ((uint64_t)key[15] << 56);

    v0 ^= k0; v1 ^= k1; v2 ^= k0; v3 ^= k1;

    const uint8_t *end = data + (len - (len % 8));
    for (; data < end; data += 8) {
        uint64_t m = ((uint64_t)data[0]) | ((uint64_t)data[1] << 8) |
                     ((uint64_t)data[2] << 16) | ((uint64_t)data[3] << 24) |
                     ((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) |
                     ((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
        v3 ^= m;
        for (int i = 0; i < 2; i++) {
            v0 += v1; v1 = _nikola_rotl64(v1, 13); v1 ^= v0; v0 = _nikola_rotl64(v0, 32);
            v2 += v3; v3 = _nikola_rotl64(v3, 16); v3 ^= v2;
            v0 += v3; v3 = _nikola_rotl64(v3, 21); v3 ^= v0;
            v2 += v1; v1 = _nikola_rotl64(v1, 17); v1 ^= v2; v2 = _nikola_rotl64(v2, 32);
        }
        v0 ^= m;
    }

    uint64_t b = ((uint64_t)len) << 56;
    switch (len & 7) {
        case 7: b |= ((uint64_t)data[6]) << 48; /* fallthrough */
        case 6: b |= ((uint64_t)data[5]) << 40; /* fallthrough */
        case 5: b |= ((uint64_t)data[4]) << 32; /* fallthrough */
        case 4: b |= ((uint64_t)data[3]) << 24; /* fallthrough */
        case 3: b |= ((uint64_t)data[2]) << 16; /* fallthrough */
        case 2: b |= ((uint64_t)data[1]) << 8;  /* fallthrough */
        case 1: b |= ((uint64_t)data[0]); break;
        case 0: break;
    }

    v3 ^= b;
    for (int i = 0; i < 2; i++) {
        v0 += v1; v1 = _nikola_rotl64(v1, 13); v1 ^= v0; v0 = _nikola_rotl64(v0, 32);
        v2 += v3; v3 = _nikola_rotl64(v3, 16); v3 ^= v2;
        v0 += v3; v3 = _nikola_rotl64(v3, 21); v3 ^= v0;
        v2 += v1; v1 = _nikola_rotl64(v1, 17); v1 ^= v2; v2 = _nikola_rotl64(v2, 32);
    }
    v0 ^= b;
    v2 ^= 0xff;

    for (int i = 0; i < 4; i++) {
        v0 += v1; v1 = _nikola_rotl64(v1, 13); v1 ^= v0; v0 = _nikola_rotl64(v0, 32);
        v2 += v3; v3 = _nikola_rotl64(v3, 16); v3 ^= v2;
        v0 += v3; v3 = _nikola_rotl64(v3, 21); v3 ^= v0;
        v2 += v1; v1 = _nikola_rotl64(v1, 17); v1 ^= v2; v2 = _nikola_rotl64(v2, 32);
    }

    return v0 ^ v1 ^ v2 ^ v3;
}

/* Compute response for a challenge (matches runtime's _compute_auth_response) */
static uint64_t nikola_compute_response(uint64_t challenge) {
    /* Decode the secret key */
    uint8_t key[16];
    for (int i = 0; i < 16; i++) {
        key[i] = _nikola_key_enc[i] ^ _nikola_key_xor;
    }

    /* HMAC: H(key || challenge || key) */
    uint8_t data[24];
    memcpy(data, key, 8);
    memcpy(data + 8, &challenge, 8);
    memcpy(data + 16, key + 8, 8);

    uint64_t result = _nikola_siphash(data, 24, key);

    /* Clear sensitive data from memory */
    memset(key, 0, 16);
    memset(data, 0, 24);

    return result;
}

/*
 * Main authentication function - call this before using runtime!
 * Returns 0 on success, non-zero on failure (program will likely exit)
 */
static inline int nikola_auth_runtime(void) {
    /* Get challenge from runtime */
    uint64_t challenge = mind_auth_get_challenge();

    /* Compute our response */
    uint64_t response = nikola_compute_response(challenge);

    /* Verify with runtime - this will exit(99) if wrong! */
    return mind_auth_verify(response);
}

#endif /* NIKOLA_AUTH_H */
