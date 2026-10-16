/* Android guest loader API.
 *
 * The APK accepts a complete rewritten module set.  Game data remains user
 * supplied; this API only loads the staged rewritten images and their tables.
 */
#include "dm.h"
#include "hle.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <android/log.h>

#define TAG "SwitchRuntime"
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

struct dm_flags g_flag;
uint64_t g_t0_ns;
_Atomic int g_dm_frozen;
_Atomic int g_frz_by_panel;
_Atomic int g_bgsave;

/* Generic host implementations are linked from hle_*.c below. */

/* HLE/loader helpers are supplied by the full generic host objects. */

void dm_android_start_guest(void);
void guest_thread_init(void);
uint32_t svc_thread_main_init(void);

int dm_android_load_module_dir(const char *dir) {
    if (!dir || !*dir) return -1;
#if DM_TARGET_ANDROID
    __android_log_print(ANDROID_LOG_INFO, "dmhost", "[dm] LOG routing check: dmhost tag reachable");
#endif
    /* Diagnostic switch handling lives in main.c's flags_init, which the APK
     * does not call; without it every DM_SVC / DM_THR line stays silent. */
    { void dm_host_flags_init(void); dm_host_flags_init(); }
    /* The 1.0.34 program load chain is rtld, main, sdk, subsdk0.  rtld is
     * metadata-only here; the executable modules are loaded in dependency
     * order so cross-module symbol resolution sees the complete set. */
    const char *names[] = {"main", "sdk", "subsdk0"};
    ALOG("guest staging accepted: %s", dir);
    load_modules(dir, names, 3);
    for (int i = 0; i < 3; ++i) {
        module_t *m = &g_mod[i];
        ALOG("module %s mapped: base=%p size=%#zx patch_off=%#llx text_end=%#llx",
             m->name, (void *)m->base, m->size,
             (unsigned long long)m->patch_off,
             (unsigned long long)m->text_end);
    }
    ALOG("guest modules relocated: count=%d", g_nmod);
    ALOG("guest gates registered");
    ALOG("guest entry pending: HLE/NVN bootstrap");
    dm_android_start_guest();
    return 0;
}

/* Minimal real guest bootstrap entry.  This deliberately uses the already
 * loaded modules and reports the first failing initialization stage. */
static void *guest_req(const char *name) {
    void *p = guest_sym(name);
    if (!p) { ALOG("guest bootstrap missing symbol: %s", name); return NULL; }
    return p;
}

static void *android_guest_bootstrap(void *unused) {
    (void)unused;
    ALOG("guest entry: bootstrap thread starting");
    ALOG("guest entry: before thread environment");
    guest_thread_init();
    ALOG("guest entry: thread environment ready");
    /* The main-thread handle passed to nnosInitialize must come from the host
     * thread table; the SDK stores it in a global and ThreadManagerImplByHorizon
     * looks it up.  A fabricated handle makes the SDK spin forever. */
    uint32_t hmain = svc_thread_main_init();
    ALOG("guest entry: main thread handle=%#x", hmain);
    void (*fn0)(void);
    void (*fn2)(uint64_t, uint64_t);
    fn0 = (void (*)(void))guest_req("_init_libc0");
    if (fn0) { ALOG("guest entry: _init_libc0"); fn0(); }
    fn2 = (void (*)(uint64_t, uint64_t))guest_req("nnosInitialize");
    if (fn2) {
        ALOG("guest entry: nnosInitialize");
        { void dm_android_sample_start(const char *, uint64_t, int); const char *s = getenv("DM_SAMPLE"); dm_android_sample_start("sdk", (uint64_t)(uintptr_t)g_mod[1].base, s ? atoi(s) : 0); }
        fn2(hmain, 0);
        ALOG("guest entry: nnosInitialize returned");
    }
    fn0 = (void (*)(void))guest_req("_init_libc1");
    if (fn0) { ALOG("guest entry: _init_libc1"); fn0(); }
    fn0 = (void (*)(void))guest_req("nninitInitializeSdkModule");
    if (fn0) { ALOG("guest entry: nninitInitializeSdkModule"); fn0(); }
    fn0 = (void (*)(void))guest_req("nninitStartup");
    if (fn0) { ALOG("guest entry: nninitStartup"); fn0(); }
    fn0 = (void (*)(void))guest_req("_init_libc2");
    if (fn0) { ALOG("guest entry: _init_libc2"); fn0(); }
    ALOG("guest entry: module inits");
    run_module_inits();
    {
        extern void dm_android_set_host_args(void);
        dm_android_set_host_args();
    }
    fn0 = (void (*)(void))guest_req("nnMain");
    if (fn0) { ALOG("guest entry: nnMain"); fn0(); }
    ALOG("guest entry: nnMain returned");
    return NULL;
}

void dm_android_start_guest(void) {
    static int started;
    if (__atomic_exchange_n(&started, 1, __ATOMIC_ACQ_REL)) return;
    pthread_t t;
    if (pthread_create(&t, NULL, (void *(*)(void *))android_guest_bootstrap, NULL) == 0) {
        pthread_detach(t);
        ALOG("guest entry: thread launched");
    } else {
        ALOG("guest entry: pthread_create failed");
    }
}


/* Game process arguments, passed to the SDK the way nn::init does on a real
 * device (through SetHostArgc / SetHostArgv). */
static char *g_guest_argv[64] = {"D2R"};
static int g_guest_argc = 1;
void dm_android_set_host_args(void) {
    void (*setc)(void *) = (void (*)(void *))guest_sym("_ZN2nn2os11SetHostArgcEi");
    void (*setv)(void *) = (void (*)(void *))guest_sym("_ZN2nn2os11SetHostArgvEPPc");
    if (setc) { ALOG("guest entry: os::SetHostArgc(%d)", g_guest_argc); setc((void *)(uint64_t)g_guest_argc); }
    if (setv) { ALOG("guest entry: os::SetHostArgv"); setv(g_guest_argv); }
}

/* Diagnostic flags normally set by main.c's flags_init, which the APK path
 * does not call: without this every DM_SVC / DM_THR line stays silent. */
void dm_host_flags_init(void) {
    g_flag.thr = getenv("DM_THR") != NULL;
    g_flag.svc = getenv("DM_SVC") != NULL;
    g_flag.fs = getenv("DM_FS") != NULL;
    g_flag.movie = getenv("DM_MOVIE") != NULL;
    g_flag.dbg = getenv("DM_DBG") != NULL;
    g_flag.imports = getenv("DM_IMPORTS") != NULL;
    g_flag.bootprof = getenv("DM_BOOTPROF") != NULL;
    LOG("host flags: svc=%d thr=%d fs=%d imports=%d", g_flag.svc, g_flag.thr,
        g_flag.fs, g_flag.imports);
}

/* Periodic PC sampler for bring-up.
 *
 * When the guest spins inside a vendor routine there is no gate call to log
 * and no SVC to count, and Android has no usable in-process debugger (debuggerd
 * needs root).  This installs a SIGALRM handler that reports the interrupted PC
 * and link register, mapped to module+offset, so a stuck loop can be located
 * from logcat alone.  Enable with DM_SAMPLE=<seconds between samples>.
 */
#if DM_TARGET_ANDROID
#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>

static int g_sample_on;
static const char *g_sample_mod;
static uint64_t g_sample_base;
static uint64_t g_sample_size;   // module extent, so an out-of-module pc is reported as such

static void sample_handler(int sig, siginfo_t *si, void *uc_) {
    (void)sig; (void)si;
    ucontext_t *uc = (ucontext_t *)uc_;
    uint64_t pc = uc->uc_mcontext.pc, lr = uc->uc_mcontext.regs[30];
    uint64_t fp = uc->uc_mcontext.regs[29];
    // pc - base is only meaningful when pc actually lies inside the sampler's
    // module; a host-library frame (the thread parked in sched_yield, say) yields
    // an absurd offset like 0x340000000 and reads as if the guest were stuck
    // somewhere it never went.  Report the two cases differently.
    uint64_t off = pc - g_sample_base;
    int inside = off < g_sample_size;
    __android_log_print(ANDROID_LOG_ERROR, "dmhost",
        "[dm] SAMPLE pc=%#llx lr=%#llx fp=%#llx (%s %s+%#llx) hl=%s x0=%#llx x1=%#llx x2=%#llx x19=%#llx x20=%#llx",
        (unsigned long long)pc, (unsigned long long)lr, (unsigned long long)fp,
        inside ? "in" : "OUTSIDE", g_sample_mod, (unsigned long long)(inside ? off : 0),
        dm_hle_trace_get(),
        (unsigned long long)uc->uc_mcontext.regs[0], (unsigned long long)uc->uc_mcontext.regs[1],
        (unsigned long long)uc->uc_mcontext.regs[2], (unsigned long long)uc->uc_mcontext.regs[19],
        (unsigned long long)uc->uc_mcontext.regs[20]);
    if (g_sample_on) alarm((unsigned)g_sample_on);
}
#endif

void dm_android_sample_start(const char *mod, uint64_t base, int seconds) {
#if DM_TARGET_ANDROID
    if (seconds <= 0) return;
    g_sample_mod = mod; g_sample_base = base; g_sample_on = seconds;
    g_sample_size = g_mod[1].size;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = sample_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);
    alarm((unsigned)seconds);
    ALOG("sampler armed: every %ds, module=%s base=%p", seconds, mod, (void *)base);
#else
    (void)mod; (void)base; (void)seconds;
#endif
}
