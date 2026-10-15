#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

uint64_t tls_probe_tpidr_read(void);
void tls_probe_tpidr_write(uint64_t);
static sigjmp_buf env;
static volatile sig_atomic_t sig;
static void on_signal(int s) { sig = s; siglongjmp(env, 1); }
int main(void) {
    struct sigaction oldill, oldseg, sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGILL, &sa, &oldill); sigaction(SIGSEGV, &sa, &oldseg);
    uint64_t before = 0, after = 0; int read_ok = 0, write_ok = 0;
    sig = 0;
    if (sigsetjmp(env, 1) == 0) { before = tls_probe_tpidr_read(); read_ok = 1; }
    sig = 0;
    if (sigsetjmp(env, 1) == 0) { tls_probe_tpidr_write(0xfeedface12345678ull); after = tls_probe_tpidr_read(); write_ok = 1; }
    printf("tpidr probe: read_ok=%d write_ok=%d before=%#llx after=%#llx signal=%d\n",
           read_ok, write_ok, (unsigned long long)before, (unsigned long long)after, sig);
    sigaction(SIGILL, &oldill, NULL); sigaction(SIGSEGV, &oldseg, NULL);
    return 0;
}
