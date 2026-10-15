/*
 * MD5 implementation for the CommonCrypto shim (RFC 1321).
 * Upstream only needs CC_MD5 for integrity checks over decompressed data.
 */
#include <string.h>
#include <CommonCrypto/CommonDigest.h>

#define ROTL(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static const uint32_t K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

static const uint8_t S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

static void md5_block(CC_MD5_CTX *ctx, const uint8_t *p) {
    uint32_t m[16], a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    for (int i = 0; i < 16; i++)
        m[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8) |
               ((uint32_t)p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);

    for (int i = 0; i < 64; i++) {
        uint32_t f; int g;
        if (i < 16)      { f = (b & c) | (~b & d);        g = i; }
        else if (i < 32) { f = (d & b) | (~d & c);        g = (5 * i + 1) % 16; }
        else if (i < 48) { f = b ^ c ^ d;                 g = (3 * i + 5) % 16; }
        else             { f = c ^ (b | ~d);              g = (7 * i) % 16; }
        uint32_t tmp = d;
        d = c; c = b;
        b = b + ROTL(a + f + K[i] + m[g], S[i]);
        a = tmp;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
}

void CC_MD5_Init(CC_MD5_CTX *ctx) {
    ctx->state[0] = 0x67452301; ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe; ctx->state[3] = 0x10325476;
    ctx->bit_count = 0; ctx->buffer_len = 0;
}

void CC_MD5_Update(CC_MD5_CTX *ctx, const void *data, CC_LONG len) {
    const uint8_t *p = (const uint8_t *)data;
    ctx->bit_count += (uint64_t)len * 8;

    if (ctx->buffer_len) {
        size_t need = 64 - ctx->buffer_len;
        size_t take = len < need ? len : need;
        memcpy(ctx->buffer + ctx->buffer_len, p, take);
        ctx->buffer_len += take; p += take; len -= take;
        if (ctx->buffer_len == 64) { md5_block(ctx, ctx->buffer); ctx->buffer_len = 0; }
    }
    while (len >= 64) { md5_block(ctx, p); p += 64; len -= 64; }
    if (len) { memcpy(ctx->buffer, p, len); ctx->buffer_len = len; }
}

void CC_MD5_Final(uint8_t *md, CC_MD5_CTX *ctx) {
    uint64_t bits = ctx->bit_count;
    uint8_t pad[72];
    size_t pad_len = (ctx->buffer_len < 56) ? (56 - ctx->buffer_len)
                                            : (120 - ctx->buffer_len);
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    for (int i = 0; i < 8; i++) pad[pad_len + i] = (uint8_t)(bits >> (8 * i));
    CC_MD5_Update(ctx, pad, (CC_LONG)(pad_len + 8));

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            md[i * 4 + j] = (uint8_t)(ctx->state[i] >> (8 * j));
}

uint8_t *CC_MD5(const void *data, CC_LONG len, uint8_t *md) {
    CC_MD5_CTX ctx;
    CC_MD5_Init(&ctx);
    CC_MD5_Update(&ctx, data, len);
    CC_MD5_Final(md, &ctx);
    return md;
}
