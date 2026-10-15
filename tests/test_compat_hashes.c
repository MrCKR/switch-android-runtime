/*
 * Compat-layer hash checks, using the published FIPS 180-4 / RFC 1321 vectors.
 *
 * The module loader identifies builds by digest, so a wrong digest would
 * silently load the wrong thing. These vectors are the reason the compat layer
 * can be trusted rather than assumed correct.
 *
 * Build and run on device:
 *   tools/build_compat_tests.sh
 */
#include <CommonCrypto/CommonDigest.h>
#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

static void to_hex(const uint8_t *d, int n, char *out) {
    static const char h[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) { out[i * 2] = h[d[i] >> 4]; out[i * 2 + 1] = h[d[i] & 15]; }
    out[n * 2] = 0;
}

static void check(const char *what, const char *got, const char *want) {
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("  FAIL %-34s\n    got  %s\n    want %s\n", what, got, want);
    } else {
        printf("  ok   %-34s %s\n", what, got);
    }
}

static void check_sha256(const char *in, size_t len, const char *want, const char *label) {
    uint8_t d[CC_SHA256_DIGEST_LENGTH]; char hex[CC_SHA256_DIGEST_LENGTH * 2 + 1];
    CC_SHA256(in, (CC_LONG)len, d);
    to_hex(d, CC_SHA256_DIGEST_LENGTH, hex);
    check(label, hex, want);
}

/* The three-step form, which is what upstream loader.c actually calls. */
static void check_sha256_streamed(const char *in, size_t len, const char *want,
                                  size_t chunk, const char *label) {
    CC_SHA256_CTX c; uint8_t d[CC_SHA256_DIGEST_LENGTH]; char hex[CC_SHA256_DIGEST_LENGTH * 2 + 1];
    CC_SHA256_Init(&c);
    for (size_t off = 0; off < len; off += chunk) {
        size_t n = (len - off < chunk) ? len - off : chunk;
        CC_SHA256_Update(&c, in + off, (CC_LONG)n);
    }
    CC_SHA256_Final(d, &c);
    to_hex(d, CC_SHA256_DIGEST_LENGTH, hex);
    check(label, hex, want);
}

int main(void) {
    printf("compat hash checks\n");

    /* FIPS 180-4 examples for SHA-256. */
    check_sha256("", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256(\"\")");
    check_sha256("abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256(\"abc\")");
    check_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "sha256(448-bit message)");

    /* The same 448-bit message fed in ragged chunks exercises buffering. */
    check_sha256_streamed("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        7, "sha256(448-bit, 7-byte chunks)");

    /* A million 'a', the standard bulk vector. */
    {
        static char million[1000000];
        memset(million, 'a', sizeof million);
        check_sha256(million, sizeof million,
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            "sha256(1e6 x 'a')");
    }

    /* Padding boundary: 55 and 56 bytes straddle the length-field layout. */
    check_sha256_streamed("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 55,
        "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
        31, "sha256(55 x 'a')");
    check_sha256_streamed("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 56,
        "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a",
        13, "sha256(56 x 'a')");

    /* MD5 regression: the shim predates this change and must not drift. */
    {
        uint8_t d[CC_MD5_DIGEST_LENGTH]; char hex[CC_MD5_DIGEST_LENGTH * 2 + 1];
        CC_MD5("abc", 3, d);
        to_hex(d, CC_MD5_DIGEST_LENGTH, hex);
        check("md5(\"abc\")", hex, "900150983cd24fb0d6963f7d28e17f72");
        CC_MD5("", 0, d);
        to_hex(d, CC_MD5_DIGEST_LENGTH, hex);
        check("md5(\"\")", hex, "d41d8cd98f00b204e9800998ecf8427e");
    }

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
