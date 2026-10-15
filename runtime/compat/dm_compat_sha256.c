/*
 * SHA-256 over the CommonCrypto entry points used by upstream (loader.c).
 *
 * Upstream digests module content to identify a build. This must be a real
 * SHA-256 — a wrong digest would silently misidentify modules — so the
 * implementation is checked against the FIPS 180-4 test vectors in
 * tests/test_compat_hashes.c.
 */
#include <CommonCrypto/CommonDigest.h>
#include <string.h>

static const uint32_t k256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
};

static inline uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_block(CC_SHA256_CTX *ctx, const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + k256[i] + w[i];
        uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void CC_SHA256_Init(CC_SHA256_CTX *ctx) {
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

void CC_SHA256_Update(CC_SHA256_CTX *ctx, const void *data, CC_LONG len) {
    const uint8_t *p = (const uint8_t *)data;
    ctx->bit_count += (uint64_t)len * 8;

    if (ctx->buffer_len) {
        size_t take = 64 - ctx->buffer_len;
        if (take > (size_t)len) take = (size_t)len;
        memcpy(ctx->buffer + ctx->buffer_len, p, take);
        ctx->buffer_len += take; p += take; len -= (CC_LONG)take;
        if (ctx->buffer_len == 64) { sha256_block(ctx, ctx->buffer); ctx->buffer_len = 0; }
    }
    while (len >= 64) { sha256_block(ctx, p); p += 64; len -= 64; }
    if (len) { memcpy(ctx->buffer, p, len); ctx->buffer_len = len; }
}

void CC_SHA256_Final(uint8_t *md, CC_SHA256_CTX *ctx) {
    uint64_t bits = ctx->bit_count;
    uint8_t pad[72];
    size_t pad_len = (ctx->buffer_len < 56) ? (56 - ctx->buffer_len)
                                            : (120 - ctx->buffer_len);
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    /* Length is appended big-endian, unlike MD5. */
    for (int i = 0; i < 8; i++) pad[pad_len + i] = (uint8_t)(bits >> (8 * (7 - i)));
    CC_SHA256_Update(ctx, pad, (CC_LONG)(pad_len + 8));

    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++)
            md[i * 4 + j] = (uint8_t)(ctx->state[i] >> (8 * (3 - j)));
}

uint8_t *CC_SHA256(const void *data, CC_LONG len, uint8_t *md) {
    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);
    CC_SHA256_Update(&ctx, data, len);
    CC_SHA256_Final(md, &ctx);
    return md;
}
