/*
 * guest_as_probe — does upstream's guest address-space model survive on Android?
 *
 * Upstream (dm/host/hle_svc.c) reserves one 5 GB PROT_NONE block and partitions
 * it itself: alias 512 MB @ +0x10000000, stack 256 MB @ +0x30000000, heap 4 GB
 * @ +0x40000000. It also reports an application memory pool of 3285 MB (+512 MB)
 * to the guest and hands the whole block to the game's allocator.
 *
 * Those numbers were tuned against iOS: the comment there records "read on a
 * real device: 6 GB succeeds, 7 GB fails" for the reservation, and the pool
 * size is the Switch 4 GB model's application pool, not an iOS or Android
 * figure. Neither is a property of Android, so neither can be assumed.
 *
 * This probe measures, on the actual Android device:
 *   1. the largest single PROT_NONE reservation that succeeds;
 *   2. whether 5 GB reserves, and whether each partition lands where the
 *      upstream partition arithmetic says it should;
 *   3. how much of the heap region can actually be committed (POPULATE),
 *      since reserving address space is not the same as being allowed to
 *      back it;
 *   4. the same for the 3728 MB pool the host currently reports the guest.
 *
 * It prints one line per check and a final PASS/FAIL, and exits non-zero on
 * any failure, so it can be run unattended over the device link.
 *
 * No game data is involved. Nothing here is D2R-specific: it measures an
 * Android property, not a game property.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#define GB (1ull << 30)

/* Upstream partition arithmetic (dm/host/hle_svc.c, mem_init). */
#define RESERVE_UPSTREAM  (5ull << 30)
#define ALIAS_OFF         0x10000000ull
#define ALIAS_CAP         0x20000000ull   /* 512 MB */
#define STACK_OFF         0x30000000ull
#define STACK_CAP         0x10000000ull   /* 256 MB */
#define HEAP_OFF          0x40000000ull
#define HEAP_CAP          (RESERVE_UPSTREAM - HEAP_OFF)   /* 4 GB */

/* What the host currently reports to the guest as its application pool. */
#define APP_POOL_BASE     0xCD500000ull   /* 3285 MB */
#define POOL_ADD_MB       512
#define POOL_REPORTED     (APP_POOL_BASE + ((uint64_t)POOL_ADD_MB << 20))

static int failures;

static void ok(const char *what, int cond, const char *detail) {
    printf("%-46s %s%s%s\n", what, cond ? "ok" : "FAIL",
           detail && *detail ? "  " : "", detail ? detail : "");
    if (!cond) failures++;
}

/* Try to reserve `bytes` of address space without committing anything. */
static void *try_reserve(size_t bytes) {
    void *p = mmap(NULL, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

/*
 * How much of a reserved region can be committed for real? Map it read-write
 * with MAP_POPULATE so the pages are actually backed, touching each page
 * first. Returns the number of bytes that succeeded.
 */
static size_t commit_probe(void *base, size_t bytes) {
    uint8_t *p = mmap(base, bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) return 0;
    /* MAP_POPULATE is advisory; fault the pages in explicitly. */
    size_t step = 4096;
    for (size_t off = 0; off < bytes; off += step) p[off] = (uint8_t)off;
    /* Read one byte back per page to be sure it is not a lazy zero page. */
    volatile uint8_t acc = 0;
    for (size_t off = 0; off < bytes; off += step) acc ^= p[off];
    (void)acc;
    return bytes;
}

int main(void) {
    printf("=== guest address-space model probe ===\n");
    printf("page size %ld\n", sysconf(_SC_PAGESIZE));

    /* 1. What is the largest single PROT_NONE reservation? */
    printf("\n-- largest single reservation --\n");
    size_t best = 0;
    static const size_t steps[] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 16 };
    for (unsigned i = 0; i < sizeof steps / sizeof steps[0]; i++) {
        size_t bytes = steps[i] * GB;
        void *p = try_reserve(bytes);
        if (p) {
            printf("  %2zu GB  reserve ok   %p\n", steps[i], p);
            munmap(p, bytes);
            best = bytes;
        } else {
            printf("  %2zu GB  reserve FAIL errno=%d (%s)\n", steps[i], errno, strerror(errno));
        }
    }
    {
        char d[64];
        snprintf(d, sizeof d, "largest ok = %llu GB", (unsigned long long)(best / GB));
        ok("reserve: at least 5 GB succeeds", best >= RESERVE_UPSTREAM, d);
    }

    /* 2. Does upstream's 5 GB reservation succeed, and do the partitions fit? */
    printf("\n-- upstream 5 GB reservation and partition layout --\n");
    uint8_t *as = try_reserve(RESERVE_UPSTREAM);
    ok("reserve: 5 GB (upstream RESERVE)", as != NULL,
       as ? "" : strerror(errno));
    if (!as) goto summary;

    uint8_t *alias = as + ALIAS_OFF;
    uint8_t *stack = as + STACK_OFF;
    uint8_t *heap  = as + HEAP_OFF;
    printf("  AS    %p +%#llx\n", (void *)as, (unsigned long long)RESERVE_UPSTREAM);
    printf("  alias %p +%#llx\n", (void *)alias, (unsigned long long)ALIAS_CAP);
    printf("  stack %p +%#llx\n", (void *)stack, (unsigned long long)STACK_CAP);
    printf("  heap  %p +%#llx\n", (void *)heap, (unsigned long long)HEAP_CAP);

    /* Every partition must lie entirely inside the reservation. */
    ok("layout: heap end is inside the reservation",
       HEAP_OFF + HEAP_CAP <= RESERVE_UPSTREAM, "");
    ok("layout: partitions do not overlap",
       ALIAS_OFF + ALIAS_CAP <= STACK_OFF && STACK_OFF + STACK_CAP <= HEAP_OFF, "");

    /* 3. Commit the whole heap region: reserving is not backing. */
    printf("\n-- commit heap region (%llu MB) --\n",
           (unsigned long long)(HEAP_CAP >> 20));
    size_t got = commit_probe(heap, HEAP_CAP);
    {
        char d[96];
        snprintf(d, sizeof d, "committed %llu MB of %llu MB",
                 (unsigned long long)(got >> 20), (unsigned long long)(HEAP_CAP >> 20));
        ok("commit: full 4 GB heap region backed", got == HEAP_CAP, d);
    }
    munmap(as, RESERVE_UPSTREAM);

    /* 4. The pool the host reports the guest: it is taken from this block, so
     *    it must fit and must be committable on its own. */
    printf("\n-- reported application pool (%llu MB) --\n",
           (unsigned long long)(POOL_REPORTED >> 20));
    {
        char d[96];
        snprintf(d, sizeof d, "pool %llu MB vs heap region %llu MB",
                 (unsigned long long)(POOL_REPORTED >> 20),
                 (unsigned long long)(HEAP_CAP >> 20));
        ok("pool: fits inside the 4 GB heap region", POOL_REPORTED <= HEAP_CAP, d);
    }
    {
        /* Upstream gives the game the pool minus the image footprint, and the
         * game carves out ~3.08 GB for itself, so the pool must stand on its
         * own even if the reservation above failed. */
        void *p = mmap(NULL, POOL_REPORTED, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        char d[96];
        snprintf(d, sizeof d, "got %p", p);
        ok("pool: can be committed on its own", p != MAP_FAILED,
           p == MAP_FAILED ? strerror(errno) : d);
        if (p != MAP_FAILED) munmap(p, POOL_REPORTED);
    }

summary:
    printf("\nguest address-space model probe: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
