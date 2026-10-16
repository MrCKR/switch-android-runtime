/* Android ELF section bounds for the three static registration tables. */
#ifndef DM_COMPAT_ELF_SECTIONS_H
#define DM_COMPAT_ELF_SECTIONS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern const uint8_t __start_dmsec_swhle[] __attribute__((weak));
extern const uint8_t __stop_dmsec_swhle[] __attribute__((weak));
extern const uint8_t __start_dmsec_swforce[] __attribute__((weak));
extern const uint8_t __stop_dmsec_swforce[] __attribute__((weak));
extern const uint8_t __start_dmsec_swnvn[] __attribute__((weak));
extern const uint8_t __stop_dmsec_swnvn[] __attribute__((weak));

static inline const uint8_t *dm_elf_section(const char *seg, const char *sect,
                                            unsigned long *size) {
    (void)seg;
    const uint8_t *a = NULL, *b = NULL;
    if (!sect) { if (size) *size = 0; return NULL; }
    if (!strcmp(sect, "swhle")) { a=__start_dmsec_swhle; b=__stop_dmsec_swhle; }
    else if (!strcmp(sect, "swforce")) { a=__start_dmsec_swforce; b=__stop_dmsec_swforce; }
    else if (!strcmp(sect, "swnvn")) { a=__start_dmsec_swnvn; b=__stop_dmsec_swnvn; }
    if (a && b && b >= a) { if (size) *size=(unsigned long)(b-a); return a; }
    if (size) *size=0;
    return NULL;
}
#endif
