#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>

#define DM_MAX_SECTIONS 16

struct dm_section {
    char seg[DM_SECTION_NAME_MAX];
    char sect[DM_SECTION_NAME_MAX];
    const uint8_t *data;
    unsigned long size;
};

static struct dm_section g_sections[DM_MAX_SECTIONS];
static int g_section_count;

static const struct mach_header_64 *g_guest_header;
static char g_guest_name[256] = "";

static int name_eq(const char *a, const char *b) {
    return a && b && strncmp(a, b, DM_SECTION_NAME_MAX) == 0;
}

int dm_compat_register_section(const char *seg, const char *sect,
                               const void *data, unsigned long size) {
    if (!seg || !sect || !data) return -1;
    if (g_section_count >= DM_MAX_SECTIONS) return -1;

    struct dm_section *s = &g_sections[g_section_count];
    strncpy(s->seg, seg, DM_SECTION_NAME_MAX - 1);
    s->seg[DM_SECTION_NAME_MAX - 1] = '\0';
    strncpy(s->sect, sect, DM_SECTION_NAME_MAX - 1);
    s->sect[DM_SECTION_NAME_MAX - 1] = '\0';
    s->data = (const uint8_t *)data;
    s->size = size;
    g_section_count++;
    return 0;
}

void dm_compat_reset_sections(void) {
    g_section_count = 0;
    memset(g_sections, 0, sizeof(g_sections));
}

const uint8_t *dm_compat_find_section(const char *seg, const char *sect,
                                      unsigned long *out_size) {
    if (sect && (!strcmp(sect, "swhle") || !strcmp(sect, "swforce") || !strcmp(sect, "swnvn"))) {
        const uint8_t *p = dm_elf_section(seg, sect, out_size);
        if (p) return p;
    }
    for (int i = 0; i < g_section_count; i++) {
        if (name_eq(g_sections[i].seg, seg) && name_eq(g_sections[i].sect, sect)) {
            if (out_size) *out_size = g_sections[i].size;
            return g_sections[i].data;
        }
    }
    if (out_size) *out_size = 0;
    return NULL;
}

void dm_compat_set_guest_image(const void *header, const char *name) {
    g_guest_header = (const struct mach_header_64 *)header;
    if (name) {
        strncpy(g_guest_name, name, sizeof(g_guest_name) - 1);
        g_guest_name[sizeof(g_guest_name) - 1] = '\0';
    }
}

const struct mach_header_64 *dm_compat_guest_image_header(void) {
    return g_guest_header;
}

const char *dm_compat_guest_image_name(void) {
    return g_guest_name;
}
