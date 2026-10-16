/* CommonCrypto SHA-1 compatibility API used by the D2R adapter. */
#ifndef DM_COMPAT_COMMONCRYPTO_SHA1_H
#define DM_COMPAT_COMMONCRYPTO_SHA1_H

#include <stdint.h>
#include <stddef.h>

#define CC_SHA1_DIGEST_LENGTH 20

typedef uint32_t CC_LONG;
typedef struct {
    uint32_t h[5];
    uint64_t bits;
    uint8_t buf[64];
    size_t n;
} CC_SHA1_CTX;

void CC_SHA1_Init(CC_SHA1_CTX *ctx);
void CC_SHA1_Update(CC_SHA1_CTX *ctx, const void *data, CC_LONG len);
void CC_SHA1_Final(uint8_t *digest, CC_SHA1_CTX *ctx);
uint8_t *CC_SHA1(const void *data, CC_LONG len, uint8_t *digest);

#endif
