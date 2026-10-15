/*
 * Minimal Blocks runtime.
 *
 * Bionic ships no Blocks runtime and the NDK has no prebuilt one, but upstream
 * uses blocks in one place (hle_lifecycle.c, two dispatch calls). Implementing
 * the runtime is smaller and less invasive than rewriting those call sites,
 * and it keeps the "upstream source untouched" rule.
 *
 * Only what clang actually emits for those two call sites is implemented:
 * a stack block that captures nothing, copied to the heap by dispatch. Capture
 * by value works; __block variables do not, and the copy/dispose helpers are
 * invoked through the descriptor when a block has them.
 */
#include <Block.h>
#include <stdlib.h>
#include <string.h>

/*
 * Block literals are laid out so that isa sits at offset 0. These are the
 * marker objects clang compares against. The size is arbitrary but must be
 * distinct; clang only ever compares the pointer.
 */
static char g_global_block_isa[32];
static char g_stack_block_isa[32];
static char g_malloc_block_isa[32];

void *_NSConcreteGlobalBlock = g_global_block_isa;
void *_NSConcreteStackBlock = g_stack_block_isa;
void *_NSConcreteMallocBlock = g_malloc_block_isa;

void _Block_object_assign(void *dst, const void *src, const int flags) {
    (void)flags;
    /* Only by-value captures are supported; copy the pointer-sized slot. */
    memcpy(dst, src, sizeof(void *));
}

void _Block_object_dispose(const void *obj, const int flags) {
    (void)obj; (void)flags;
}

void *_Block_copy(const void *block) {
    if (!block) return NULL;

    const struct Block_literal *src = (const struct Block_literal *)block;
    if (src->flags & BLOCK_IS_GLOBAL) return (void *)block;
    if (src->isa == g_malloc_block_isa) return (void *)block;

    /* A stack block: move it to the heap, then let it own its captures. */
    size_t size = src->descriptor ? (size_t)src->descriptor->size : sizeof(struct Block_literal);
    struct Block_literal *dst = (struct Block_literal *)malloc(size);
    if (!dst) return NULL;

    memcpy(dst, src, size);
    dst->isa = g_malloc_block_isa;
    dst->flags |= BLOCK_HAS_STRET;

    if (src->flags & BLOCK_HAS_COPY_DISPOSE) {
        if (src->descriptor && src->descriptor->copy)
            src->descriptor->copy(dst, src);
    }
    return dst;
}

void _Block_release(const void *block) {
    if (!block) return;

    const struct Block_literal *b = (const struct Block_literal *)block;
    if (b->isa == g_global_block_isa) return;
    if (b->isa != g_malloc_block_isa) return;   /* still on the stack */

    if (b->flags & BLOCK_HAS_COPY_DISPOSE) {
        if (b->descriptor && b->descriptor->dispose)
            b->descriptor->dispose(block);
    }
    free((void *)block);
}
