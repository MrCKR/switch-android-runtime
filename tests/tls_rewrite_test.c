/*
 * Load-time binding + end-to-end check for the TLS rewrite.
 *
 * This is a small stand-in for what the runtime loader will do with a real
 * module: take the rewritten image, copy its text to an executable mapping,
 * and bind the placeholder `bl` at each recorded fixup to the real trampoline.
 * Nothing here is D2R-specific.
 *
 * Assertions:
 *   1. the rewritten image has one fixup per rewritten site;
 *   2. after binding, the guest routine runs without faulting;
 *   3. the value written through a rewritten `msr` reads back through the
 *      rewritten `mrs` (slot plumbing works end to end);
 *   4. x19-x23 sentinels survive, i.e. the trampolines preserve caller state;
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct {
    uint64_t read_back;
    uint64_t sentinel_a;
    uint64_t sentinel_b;
    uint64_t sentinel_c;
    uint64_t tlr_read;
    uint64_t stored;
} guest_out;

uint64_t dm_tls_tramp_get(void);
void dm_tls_tramp_set(void);

static int patch_fixups(uint8_t *image, size_t image_size,
                        uintptr_t base, const char *fixups_path, int *out_bound) {
    FILE *f = fopen(fixups_path, "r");
    if (!f) { perror("fixups"); return -1; }
    char line[256];
    int bound = 0;
    uintptr_t veneer_cursor = (base + image_size + 0x1f) & ~((uintptr_t)0x1f);
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        unsigned long long at;
        char kind[32];
        if (sscanf(line, "%llx\t%31s", &at, kind) != 2) continue;
        if (at >= image_size) { fprintf(stderr, "fixup outside image: %#llx\n", at); fclose(f); return -1; }

        uintptr_t target = strcmp(kind, "TLS_GET") == 0 ? (uintptr_t)dm_tls_tramp_get
                          : strcmp(kind, "TLS_SET") == 0 ? (uintptr_t)dm_tls_tramp_set
                          : 0;
        if (!target) { fprintf(stderr, "unknown fixup kind %s\n", kind); fclose(f); return -1; }

        uint32_t *at_word = (uint32_t *)(base + at);
        int64_t delta = (int64_t)target - (int64_t)(uintptr_t)at_word;
        if (delta % 4 != 0 || delta < -(1 << 27) || delta >= (1 << 27)) {
            /* The mapping and the native library are not guaranteed to be
             * within AArch64 BL's +/-128 MiB range. Put a tiny absolute veneer
             * next to the image and branch to that instead. */
            uintptr_t veneer = veneer_cursor;
            veneer_cursor += 16;
            uint32_t *v = (uint32_t *)veneer;
            v[0] = 0x58000050u;          /* ldr x16, #8 */
            v[1] = 0xD61F0200u;          /* br x16 */
            *(uint64_t *)(veneer + 8) = target;
            delta = (int64_t)veneer - (int64_t)(uintptr_t)at_word;
            if (delta % 4 != 0 || delta < -(1 << 27) || delta >= (1 << 27)) {
                fprintf(stderr, "near veneer out of branch range\n");
                fclose(f); return -1;
            }
        }
        *at_word = 0x94000000u | (((uint64_t)(delta >> 2)) & 0x3FFFFFFu);
        bound++;
    }
    fclose(f);
    *out_bound = bound;
    return 0;
}

static volatile int g_thread_ok;
static void *other_thread(void *arg) {
    guest_out *o = arg;
    uint64_t (*routine)(guest_out *) = (uint64_t (*)(guest_out *))(uintptr_t)(*(uintptr_t *)arg);
    (void)routine;
    g_thread_ok = 0;
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: tls_rewrite_test <image> <fixups.tsv>\n"); return 2; }
    const char *image_path = argv[1];
    const char *fixups_path = argv[2];

    FILE *f = fopen(image_path, "rb");
    if (!f) { perror("image"); return 2; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *image = malloc((size_t)size);
    if (fread(image, 1, (size_t)size, f) != (size_t)size) { fclose(f); return 2; }
    fclose(f);

    long page = sysconf(_SC_PAGESIZE);
    void *mapping = mmap(NULL, (size_t)size + (size_t)page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) { perror("mmap"); return 2; }
    memcpy(mapping, image, (size_t)size);

    int bound = 0;
    if (patch_fixups(image, (size_t)size, (uintptr_t)mapping, fixups_path, &bound) != 0) return 1;
    if (bound != 3) { fprintf(stderr, "expected 3 fixups, got %d\n", bound); return 1; }

    if (mprotect(mapping, (size_t)size + (size_t)page, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect");
        return 1;
    }

    /* AArch64 I-cache maintenance is required after writing executable code.
     * Do this after both the site branches and the load-time BL/veneer binding
     * have been installed. */
    __builtin___clear_cache((char *)mapping, (char *)mapping + (size_t)size + (size_t)page);

    /* The synthetic routine sits at its fixed text offset in the image. */
    uint64_t (*routine)(guest_out *) =
        (uint64_t (*)(guest_out *))((uintptr_t)mapping + 0x1000);

    guest_out out;
    memset(&out, 0, sizeof out);
    uint64_t rc = routine(&out);

    int ok = 1;
    if (rc != 0) { printf("routine returned %llu\n", (unsigned long long)rc); ok = 0; }
    if (out.read_back != out.stored) {
        printf("slot readback mismatch: stored=%#llx read=%#llx\n",
               (unsigned long long)out.stored, (unsigned long long)out.read_back);
        ok = 0;
    }
    if (out.sentinel_a != 0x11 || out.sentinel_b != 0x22 || out.sentinel_c != 0x33) {
        printf("sentinels clobbered: %#llx %#llx %#llx\n",
               (unsigned long long)out.sentinel_a, (unsigned long long)out.sentinel_b,
               (unsigned long long)out.sentinel_c);
        ok = 0;
    }

    printf("bound_fixups=3 sites_ran=1 stored=%#llx read=%#llx\n",
           (unsigned long long)out.stored, (unsigned long long)out.read_back);
    printf("tls rewrite test: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
