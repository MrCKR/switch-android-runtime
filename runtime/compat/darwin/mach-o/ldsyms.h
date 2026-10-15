/*
 * mach-o/ldsyms.h — Android/AArch64 shim.
 *
 * Apple declares _mh_execute_header here. On Android that symbol is provided
 * by runtime/compat/dm_compat_macho_exec.c and resolves to the guest image the
 * runtime has mapped, so including this header simply pulls in the Mach-O
 * type definitions upstream needs.
 */
#ifndef DM_COMPAT_MACH_O_LDSYMS_H
#define DM_COMPAT_MACH_O_LDSYMS_H

#include <mach-o/loader.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const struct mach_header_64 *_mh_execute_header;

void dm_compat_bind_exec_header(void);

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_MACH_O_LDSYMS_H */
