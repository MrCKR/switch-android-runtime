/*
 * CommonCrypto — Android shim over the platform's own digest code.
 *
 * Upstream uses CC_MD5 only, as a data-integrity check over decompressed
 * buffers (hle_tact.c). Implement the same entry point over a small MD5 that
 * is compiled into the runtime, so behaviour is identical without depending on
 * a Darwin framework.
 */
#ifndef DM_COMPAT_COMMONCRYPTO_H
#define DM_COMPAT_COMMONCRYPTO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t CC_LONG;

#define CC_MD5_DIGEST_LENGTH 16
#define CC_SHA256_DIGEST_LENGTH 32

/* Bytes needed to hold the running MD5 state. */
typedef struct {
    uint32_t state[4];
    uint64_t bit_count;
    uint8_t buffer[64];
    size_t buffer_len;
} CC_MD5_CTX;

void CC_MD5_Init(CC_MD5_CTX *ctx);
void CC_MD5_Update(CC_MD5_CTX *ctx, const void *data, CC_LONG len);
void CC_MD5_Final(uint8_t *md, CC_MD5_CTX *ctx);

/* One-shot form used by upstream. */
uint8_t *CC_MD5(const void *data, CC_LONG len, uint8_t *md);

/*
 * SHA-256, used by upstream as the module-content digest when loading a
 * module image (loader.c). Same shape as the MD5 entry points above.
 */
typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[64];
    size_t buffer_len;
} CC_SHA256_CTX;

void CC_SHA256_Init(CC_SHA256_CTX *ctx);
void CC_SHA256_Update(CC_SHA256_CTX *ctx, const void *data, CC_LONG len);
void CC_SHA256_Final(uint8_t *md, CC_SHA256_CTX *ctx);

/*
 * One-shot form. Upstream calls the three-step form; this is provided for
 * parity with CC_MD5 and for the compat test.
 */
uint8_t *CC_SHA256(const void *data, CC_LONG len, uint8_t *md);

#ifdef __cplusplus
}
#endif

#include <CommonCrypto/CommonDigestSHA1.h>

#endif /* DM_COMPAT_COMMONCRYPTO_H */
