/*
 * Which of the ARM64 thread-pointer sysregs can a Linux/Android user process
 * actually write? The earlier answer for TPIDRRO_EL0 was "read only, write
 * faults". TPIDR_EL0 was never measured on device (the build failed and a 404
 * page was executed instead), yet the whole rewrite design depends on it.
 *
 * TPIDR_EL0 is saved and restored around the probe so a successful write does
 * not disturb the C runtime's own TLS on this thread.
 */
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>

static sigjmp_buf env;
static volatile sig_atomic_t caught;
static void on_signal(int s) { caught = s; siglongjmp(env, 1); }

static uint64_t rd_tpidr(void) { uint64_t v; __asm__ volatile("mrs %0, tpidr_el0" : "=r"(v)); return v; }
static void wr_tpidr(uint64_t v) { __asm__ volatile("msr tpidr_el0, %0" :: "r"(v)); }
static uint64_t rd_tpidrro(void) { uint64_t v; __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(v)); return v; }
static void wr_tpidrro(uint64_t v) { __asm__ volatile("msr tpidrro_el0, %0" :: "r"(v)); }

int main(void) {
    struct sigaction sa = {0}, old_ill, old_seg;
    sa.sa_handler = on_signal;
    sigaction(SIGILL, &sa, &old_ill);
    sigaction(SIGSEGV, &sa, &old_seg);

    uint64_t tpidr_orig = 0, tpidrro = 0;
    caught = 0;
    if (sigsetjmp(env, 1) == 0) tpidr_orig = rd_tpidr();
    caught = 0;
    if (sigsetjmp(env, 1) == 0) tpidrro = rd_tpidrro();
    printf("read: tpidr_el0=%#llx tpidrro_el0=%#llx\n",
           (unsigned long long)tpidr_orig, (unsigned long long)tpidrro);

    const uint64_t mark = 0x0000feed0000beefull;
    int tpidr_write_ok = 0, tpidrro_write_ok = 0;
    uint64_t tpidr_back = 0, tpidrro_back = 0;

    caught = 0;
    if (sigsetjmp(env, 1) == 0) {
        wr_tpidr(mark);
        tpidr_back = rd_tpidr();
        tpidr_write_ok = (tpidr_back == mark);
        wr_tpidr(tpidr_orig);           /* always restore */
    }
    int tpidr_fault = caught;

    caught = 0;
    if (sigsetjmp(env, 1) == 0) {
        wr_tpidrro(mark);
        tpidrro_back = rd_tpidrro();
        tpidrro_write_ok = (tpidrro_back == mark);
    }
    int tpidrro_fault = caught;

    printf("write tpidr_el0: ok=%d back=%#llx fault_signal=%d\n",
           tpidr_write_ok, (unsigned long long)tpidr_back, tpidr_fault);
    printf("write tpidrro_el0: ok=%d back=%#llx fault_signal=%d\n",
           tpidrro_write_ok, (unsigned long long)tpidrro_back, tpidrro_fault);

    sigaction(SIGILL, &old_ill, NULL);
    sigaction(SIGSEGV, &old_seg, NULL);
    return 0;
}
