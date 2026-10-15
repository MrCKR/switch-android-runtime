#!/usr/bin/env python3
"""Make an Android-test copy of upstream loader.c.

Only tsd_check is replaced: upstream's implementation reads Darwin's private
pthread TSD layout through tpidrro_el0. Android/Bionic deliberately does not
expose that layout. The real Android replacement belongs in the runtime loader
spec; this test copy keeps the synthetic image test focused on mapping,
MOD0/dynamic parsing, relocations, symbol lookup and entry execution.
"""
import pathlib, re, sys
src = pathlib.Path(sys.argv[1]).read_text()
out = pathlib.Path(sys.argv[2])
needle = 'static void tsd_check(void) {'
start = src.index(needle)
# Find the matching brace, respecting C strings/comments sufficiently for this
# function (which contains no nested function definitions).
brace = src.index('{', start)
depth = 0
end = None
for i in range(brace, len(src)):
    if src[i] == '{': depth += 1
    elif src[i] == '}':
        depth -= 1
        if depth == 0:
            end = i + 1
            break
assert end is not None
replacement = '''static void tsd_check(void) {
    /* Android/Bionic does not expose Darwin's private tpidrro_el0 TSD layout.
     * The Android loader will install an explicit TLS bridge before real guest
     * execution; synthetic loader tests do not need this Darwin-only probe. */
}'''
src = src[:start] + replacement + src[end:]
# Add progress markers to make a target-side crash location explicit.
src = src.replace('static void build_symhash(module_t *m);\nstatic void parse_dynamic(module_t *m) {', 'static void build_symhash(module_t *m);\nstatic void parse_dynamic(module_t *m) { fprintf(stderr, "LOADER_TEST parse enter\\n");')
src = src.replace('    m->nsym = (unsigned)(((const uint8_t *)m->strtab - (const uint8_t *)m->symtab) / sizeof(Elf64_Sym));\n    build_symhash(m);', '    fprintf(stderr, "LOADER_TEST dyn done sym=%p str=%p rela=%p\\n", (void *)m->symtab, (void *)m->strtab, (void *)m->dyn);\n    m->nsym = (unsigned)(((const uint8_t *)m->strtab - (const uint8_t *)m->symtab) / sizeof(Elf64_Sym));\n    fprintf(stderr, "LOADER_TEST nsym=%u\\n", m->nsym);\n    build_symhash(m);\n    fprintf(stderr, "LOADER_TEST symhash done\\n");')
src = src.replace('        t = now_ns(); map_image(m, dir);', '        fprintf(stderr, "LOADER_TEST map begin %s\\n", names[i]); t = now_ns(); map_image(m, dir); fprintf(stderr, "LOADER_TEST map done\\n");')
src = src.replace('        t = now_ns(); parse_dynamic(m);', '        fprintf(stderr, "LOADER_TEST parse begin\\n"); t = now_ns(); parse_dynamic(m); fprintf(stderr, "LOADER_TEST parse done\\n");')
src = src.replace('    force_init();', '    fprintf(stderr, "LOADER_TEST force init\\n"); force_init(); fprintf(stderr, "LOADER_TEST force done\\n");')
src = src.replace('    t = now_ns(); for (int i = 0; i < g_nmod; i++) relocate(&g_mod[i]);', '    fprintf(stderr, "LOADER_TEST relocate begin\\n"); t = now_ns(); for (int i = 0; i < g_nmod; i++) relocate(&g_mod[i]); fprintf(stderr, "LOADER_TEST relocate done\\n");')
out.write_text(src)
print(out)
