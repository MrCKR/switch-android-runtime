/*
 * Block.h — minimal Blocks runtime interface.
 *
 * Upstream uses blocks in exactly one host file (hle_lifecycle.c: two
 * dispatch call sites). Bionic has no Blocks runtime, so one is provided
 * rather than restructuring upstream's code.
 *
 * Only the ABI that clang emits is implemented. See dm_compat_blocks.c for the
 * layout and the ownership rules.
 */
#ifndef DM_COMPAT_BLOCK_H
#define DM_COMPAT_BLOCK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Block ABI flags, as emitted by clang. */
#define BLOCK_HAS_COPY_DISPOSE (1 << 25)
#define BLOCK_HAS_CTOR (1 << 26)
#define BLOCK_IS_GLOBAL (1 << 28)
#define BLOCK_HAS_STRET (1 << 29)
#define BLOCK_HAS_SIGNATURE (1 << 30)

struct Block_descriptor {
    unsigned long reserved;
    unsigned long size;
    void (*copy)(void *dst, const void *src);
    void (*dispose)(const void *);
};

struct Block_literal {
    void *isa;
    int flags;
    int reserved;
    void (*invoke)(void *, ...);
    struct Block_descriptor *descriptor;
    /* captured variables follow */
};

/* The three isa values the runtime recognises. */
extern void *_NSConcreteGlobalBlock;
extern void *_NSConcreteStackBlock;
extern void *_NSConcreteMallocBlock;

void *_Block_copy(const void *block);
void _Block_release(const void *block);

void _Block_object_assign(void *dst, const void *src, const int flags);
void _Block_object_dispose(const void *obj, const int flags);

#define Block_copy(b) _Block_copy(b)
#define Block_release(b) _Block_release(b)

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_BLOCK_H */
