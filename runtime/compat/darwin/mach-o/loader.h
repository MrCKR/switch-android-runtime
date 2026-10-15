/*
 * mach-o/loader.h + dyld.h — Android/AArch64 shim.
 *
 * Provides the Mach-O image identifiers upstream references while scanning for
 * its embedded guest image. On Android the guest half is a normal loaded
 * module tracked by the runtime, so the dyld iteration helpers report exactly
 * that single image instead of walking a dyld list.
 */
#ifndef DM_COMPAT_MACH_O_LOADER_H
#define DM_COMPAT_MACH_O_LOADER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mach_header {
    uint32_t magic;
    int32_t cputype;
    int32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
};

struct mach_header_64 {
    struct mach_header hdr;
    uint32_t reserved;
};

/* Registered by the runtime once the guest module is mapped. */
void dm_compat_set_guest_image(const void *header, const char *name);

/* Header of the "main" image: the runtime's own guest module. */
const struct mach_header_64 *dm_compat_guest_image_header(void);
const char *dm_compat_guest_image_name(void);

/* Single-entry view of the guest image, matching upstream's scan loop. */
static inline uint32_t _dyld_image_count(void) { return 1u; }

static inline const char *_dyld_get_image_name(uint32_t i) {
    return i == 0 ? dm_compat_guest_image_name() : 0;
}

static inline const void *_dyld_get_image_header(uint32_t i) {
    return i == 0 ? (const void *)dm_compat_guest_image_header() : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_MACH_O_LOADER_H */
