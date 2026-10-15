/*
 * Upstream refers to _mh_execute_header directly for its own image. On Android
 * that Mach-O header does not exist, so provide the symbol the same header
 * resolves to: the registered guest image (or null before registration).
 */
#include <mach-o/loader.h>

const struct mach_header_64 *dm_compat_guest_image_header(void);

const struct mach_header_64 *_mh_execute_header = 0;

/* Called by the runtime after it maps the guest module. */
void dm_compat_bind_exec_header(void) {
    _mh_execute_header = dm_compat_guest_image_header();
}
