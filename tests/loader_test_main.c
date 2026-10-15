/* Deterministic host-side stubs for the synthetic loader test. */
#include "dm.h"
#include "hle.h"
#include <stdio.h>
#include <string.h>

struct dm_flags g_flag;
uint64_t g_t0_ns;

uint64_t now_ns(void) { return 1; }
void gate_register(uint64_t r, const char *k, int a, uint64_t o) { (void)r;(void)k;(void)a;(void)o; }
void hlecount_seal(void) {}
void *hlecount_wrap(const char *a, const char *b, void *p) { (void)a;(void)b; return p; }
uint64_t resolve_import(module_t *m, const char *name) {
    (void)m;
    return strcmp(name, "external") == 0 ? 0x123456789abcdef0ull : 0;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: loader_test <dir>\n"); return 2; }
    const char *names[] = { "synthetic" };
    load_modules(argv[1], names, 1);
    module_t *m = &g_mod[0];
    uint64_t relative = *(uint64_t *)(m->base + 0x5000);
    uint64_t absolute = *(uint64_t *)(m->base + 0x5008);
    uint64_t imported = *(uint64_t *)(m->base + 0x5010);
    void *answer = module_sym(m, "answer");
    run_module_inits();
    uint64_t result = ((uint64_t (*)(void))answer)();
    int ok = g_nmod == 1 && relative == (uint64_t)m->base + 0x1000 &&
             absolute == (uint64_t)m->base + 0x1004 &&
             imported == 0x123456789abcdef0ull && result == 42;
    printf("loader test: %s\n", ok ? "PASS" : "FAIL");
    printf("modules=%d relative=%#llx absolute=%#llx imported=%#llx entry=%llu\n",
           g_nmod, (unsigned long long)relative, (unsigned long long)absolute,
           (unsigned long long)imported, (unsigned long long)result);
    return ok ? 0 : 1;
}
