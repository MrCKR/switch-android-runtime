/*
 * Runtime-startup compatibility self-test.
 *
 * This intentionally exercises the same public shim entry points that the
 * upstream host uses, but does not load game data or call any D2R code. It is
 * safe to run at APK startup and gives the runtime a deterministic health
 * signal before the loader and graphics layers are connected.
 */
#include <CommonCrypto/CommonDigest.h>
#include <pthread_compat.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void hex(const uint8_t *in, size_t n, char *out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 15];
    }
    out[n * 2] = '\0';
}

static int expect_digest(const char *label, const char *got, const char *want,
                         char *error, size_t error_size) {
    if (strcmp(got, want) == 0) return 1;
    snprintf(error, error_size, "%s mismatch: got %s", label, got);
    return 0;
}

int dm_compat_self_test(char *error, size_t error_size) {
    uint8_t digest[CC_SHA256_DIGEST_LENGTH];
    char actual[CC_SHA256_DIGEST_LENGTH * 2 + 1];
    CC_SHA256_CTX sha;

    CC_SHA256_Init(&sha);
    CC_SHA256_Update(&sha, "a", 1);
    CC_SHA256_Update(&sha, "bc", 2);
    CC_SHA256_Final(digest, &sha);
    hex(digest, CC_SHA256_DIGEST_LENGTH, actual);
    if (!expect_digest("sha256(abc)", actual,
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            error, error_size)) return 0;

    uint8_t md5[CC_MD5_DIGEST_LENGTH];
    char md5_text[CC_MD5_DIGEST_LENGTH * 2 + 1];
    CC_MD5("abc", 3, md5);
    hex(md5, CC_MD5_DIGEST_LENGTH, md5_text);
    if (!expect_digest("md5(abc)", md5_text,
            "900150983cd24fb0d6963f7d28e17f72", error, error_size)) return 0;

    pthread_t self = pthread_self();
    if (!pthread_mach_thread_np(self)) {
        snprintf(error, error_size, "pthread_mach_thread_np returned zero");
        return 0;
    }
    if (!pthread_get_stackaddr_np(self) || pthread_get_stacksize_np(self) == 0) {
        snprintf(error, error_size, "pthread stack shim returned no stack");
        return 0;
    }
    if (clock_gettime_nsec_np(CLOCK_MONOTONIC) == 0) {
        snprintf(error, error_size, "monotonic clock shim returned zero");
        return 0;
    }

    if (error_size) error[0] = '\0';
    return 1;
}
