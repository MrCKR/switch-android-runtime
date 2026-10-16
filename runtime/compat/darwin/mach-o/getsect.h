/*
 * mach-o/getsect.h — Android/AArch64 shim.
 *
 * Upstream reaches into Mach-O sections of its own executable to find the
 * embedded guest image, the HLE dispatch table and the patch-force table
 * (getsectiondata(&_mh_execute_header, "__DATA", "swhle", &sz) etc.).
 *
 * Android has no Mach-O image, so those sections do not exist in the ELF.
 * Instead of emulating the Mach-O loader, this shim keeps an explicit
 * registry of named sections. The runtime fills it at startup by mapping the
 * files produced by the offline build stage (the same artefacts that were
 * embedded as sections on Apple platforms).
 *
 * This is a functional replacement: lookups return the same pointers and
 * sizes the upstream code expects, only the storage location differs.
 */
#ifndef DM_COMPAT_MACH_O_GETSECT_H
#define DM_COMPAT_MACH_O_GETSECT_H

#include <mach-o/elf_sections.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DM_SECTION_NAME_MAX 32

struct mach_header_64;
struct section_64 {
    char sectname[16];
    char segname[16];
    uint64_t addr;
    uint64_t size;
};

/* Register a section under (segment, section). Returns 0 on success. */
int dm_compat_register_section(const char *seg, const char *sect,
                               const void *data, unsigned long size);

/* Drop every registration (used between guest launches). */
void dm_compat_reset_sections(void);

/* Look up a section; returns NULL when absent. */
const uint8_t *dm_compat_find_section(const char *seg, const char *sect,
                                      unsigned long *out_size);

/*
 * Upstream calls these with a Mach-O header pointer. On Android the argument
 * is only ever compared against the main image, so section lookup ignores it
 * and resolves against the registry.
 */
static inline const uint8_t *getsectiondata(const struct mach_header_64 *mh,
                                            const char *seg, const char *sect,
                                            unsigned long *size) {
    (void)mh;
    return dm_compat_find_section(seg, sect, size);
}

static inline const uint8_t *getsectdata(const char *img, const char *seg,
                                         const char *sect, unsigned long *size) {
    (void)img;
    return dm_compat_find_section(seg, sect, size);
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_MACH_O_GETSECT_H */
