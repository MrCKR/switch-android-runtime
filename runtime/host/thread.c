// 客体线程环境：门槽 = 宿主门入口；x18 槽清零；tpidr_el0 槽 = ELF TLS 块；tpidrro_el0 槽 = Horizon ThreadLocalRegion（0x200）。
// SDK 的内部临界区从 TLR[0x1f8] 取当前线程对象指针，这里给每线程分配一个零初始化对象并写入。
// Guest thread environment: gate slot = host gate entry; x18 slot cleared; tpidr_el0 slot = ELF TLS block; tpidrro_el0 slot = Horizon ThreadLocalRegion (0x200).
// The SDK's internal critical section takes the current thread object pointer from TLR[0x1f8]; here each thread gets a zero-initialized object allocated and written in.
#include "dm.h"
#define TLS_SIZE 8192
#define TLR_SIZE 0x200
#define THREAD_OBJ_SIZE 0x400
// 客体槽控制块：8 个槽 × 0x4000（与改写器 androidize_nso.py 的 SLOT_STRIDE 一致）
// Guest slot control block: 8 slots x 0x4000 (matches SLOT_STRIDE in tools/androidize_nso.py)
#define DM_GUEST_SLOT_BLOCK   0x20000
#define DM_GUEST_SLOT(n)      ((n) * 0x4000)
#define DM_GUEST_SLOT_K18     DM_GUEST_SLOT(0)
#define DM_GUEST_SLOT_KTLS    DM_GUEST_SLOT(1)
#define DM_GUEST_SLOT_KTLR    DM_GUEST_SLOT(2)
#define DM_GUEST_SLOT_KGATE   DM_GUEST_SLOT(3)
#define DM_GUEST_SLOT_KRES0   DM_GUEST_SLOT(4)
#define DM_GUEST_SLOT_KRES1   DM_GUEST_SLOT(5)
#define DM_GUEST_SLOT_KASPECT DM_GUEST_SLOT(6)
#define DM_GUEST_SLOT_KASPECTUI DM_GUEST_SLOT(7)
extern void dm_gate(void);            // entry.S：门公共入口 | entry.S: common gate entry
#include "front.h"
#include "dm_uiscreen.h"
static pthread_key_t g_keyown;   // 本线程申请到的 TLS 块 / 线程本地区的原始指针（见函数末尾） | raw pointers of the TLS block / thread-local region allocated by this thread (see the end of the function)
static pthread_once_t g_keyown_once = PTHREAD_ONCE_INIT;
static void own_free(void *p) { void **own = p; free(own[0]); free(own[1]); free(own); }
static void own_key_init(void) { if (pthread_key_create(&g_keyown, own_free)) die("创建线程本地块的释放键失败"); }
void guest_thread_init(void) {
    pthread_setspecific(g_keygate, (void *)dm_gate);
    for (int i = 0; i < 2; i++) if (g_keyres[i]) pthread_setspecific(g_keyres[i], (void *)dm_game_res(i));   // 分辨率参数位：{u32 宽, u32 高} 打包 | resolution parameter slot: {u32 width, u32 height} packed
    if (g_keyaspect || g_keyaspectui) {   // 宽高比上限参数位：max(19/9, 两种模式输出宽 / 高)，float 位模式放低 32 位 | aspect ratio limit parameter slot: max(19/9, output width / height of the two modes), float bit pattern in the low 32 bits
        float a = 19.0f / 9.0f; int wide = 0;
        for (int i = 0; i < 2; i++) { uint64_t r = front_game_res(i); float w = (float)(uint32_t)r, h = (float)(uint32_t)(r >> 32); if (h > 0 && w / h > a) { a = w / h; wide = 1; } }
        // 界面边柱着色器的上限：屏幕宽于 19:9 时另加 0.15，把边柱向内的柔边推出屏幕；16:9 不变。
        // 推导：边柱内缘 x = 屏高 × 上限 / 2，上限每加 Δ，内缘每侧外移 屏高 × Δ / 2。Mac 1952x900 实测柔边宽度 ≤ 30 像素
        // （上限 = 屏幕比例时右缘第 0–20 列由黑渐亮，上限推到 2.4 即消失）⇒ 需 Δ ≥ 2 × 30 / 900 ≈ 0.067；取 0.15（每侧外移约 67 像素）留出一倍以上余量。
        // 不是魔数：屏高变大时余量按比例不变（Δ 是相对屏高的量）
        // Limit of the UI side-pillar shader: when the screen is wider than 19:9 add another 0.15, pushing the pillars' inward soft edge off the screen; 16:9 is unchanged.
        // Derivation: pillar inner edge x = screen height * limit / 2, so for every delta added to the limit the inner edge moves outward by screen height * delta / 2 on each side. Measured on Mac 1952x900, the soft edge is <= 30 pixels wide
        // (with limit = screen aspect ratio, columns 0–20 at the right edge fade from black to bright, and it disappears once the limit is pushed to 2.4) => need delta >= 2 * 30 / 900 ~ 0.067; 0.15 is taken (about 67 pixels outward per side), leaving more than double the margin.
        // Not a magic number: when the screen height grows the margin stays proportional (delta is relative to screen height)
        float ui = wide ? a + 0.15f : a;
        uint32_t bits;
        if (g_keyaspect) { memcpy(&bits, &a, 4); pthread_setspecific(g_keyaspect, (void *)(uintptr_t)bits); }
        if (g_keyaspectui) { memcpy(&bits, &ui, 4); pthread_setspecific(g_keyaspectui, (void *)(uintptr_t)bits); }
    }
    pthread_setspecific(g_key18, 0);
    // 这两块随线程退出释放。客体会改写 TLS / 线程本地区这两把键的槽（它自己设线程指针），所以不能按键里的当前值去 free：
    // 申请到的原始指针另记在本文件自己的键 g_keyown 上，由它的析构放（键析构在 pthread_exit 时调用，同信号备用栈 crash.c altstack_free；
    // 析构里只有 free，客体线程的小栈上也安全）。原先不放：游戏频繁起短命线程，每条漏 8192 + 0x200 + 0x400 = 9728 字节。
    // 线程本地区与当前线程对象并成一块（TLR[0x1f8] 那一格 SDK 也可能改写）
    // These two blocks are freed when the thread exits. The guest overwrites the slots of the TLS / thread-local-region keys (it sets its own thread pointer), so they cannot be freed by the current value in the key:
    // the raw pointers that were allocated are recorded separately on this file's own key g_keyown and freed by its destructor (key destructors are called at pthread_exit, same as the signal alternate stack, crash.c altstack_free;
    // the destructor only calls free, which is safe on the small stack of a guest thread as well). They used not to be freed: the game starts short-lived threads frequently, and each leaked 8192 + 0x200 + 0x400 = 9728 bytes.
    // The thread-local region and the current thread object are merged into one block (the slot TLR[0x1f8] may also be overwritten by the SDK)
    // 客体槽控制块：8 个槽按 0x4000 步距排列，改写后的客体代码用
    // 「基址 + 键页偏移」直接读写；键页里存的就是槽内偏移（见 loader.c）。
    // 槽 0/1/2 由客体自己写，槽 3 起是宿主参数，必须在这里填好。
    void **own = calloc(2, sizeof *own);
    uint8_t *blk = calloc(1, DM_GUEST_SLOT_BLOCK);
    uint8_t *tls = blk + 0x2000;                     // 客体 ELF TLS 块 | guest ELF TLS block
    uint8_t *tlr = blk + 0x1000;                     // Horizon ThreadLocalRegion
    own[0] = blk; own[1] = tlr;
    memset(tls, 0, TLS_SIZE);
    memset(tlr, 0, TLR_SIZE);
    *(void **)(tlr + 0x1f8) = blk + 0x1800;          // 当前线程对象 | current thread object
    // The slot words are what the guest reads after "base + key-page offset".
    // On Apple those slots were the pthread TSD cells populated by
    // pthread_setspecific; here the block IS the TSD, so the same values have
    // to be written straight into it -- KTLR holds the thread-local-region
    // pointer and KTLS the guest TLS block, both of which the SDK dereferences.
    *(uint64_t *)(blk + DM_GUEST_SLOT_KTLS) = (uint64_t)(uintptr_t)tls;
    *(uint64_t *)(blk + DM_GUEST_SLOT_KTLR) = (uint64_t)(uintptr_t)tlr;
    *(uint64_t *)(blk + DM_GUEST_SLOT_K18) = 0;
    *(uint64_t *)(blk + DM_GUEST_SLOT_KGATE) = (uint64_t)(uintptr_t)dm_gate;
    *(uint64_t *)(blk + DM_GUEST_SLOT_KRES0) = dm_game_res(0);
    *(uint64_t *)(blk + DM_GUEST_SLOT_KRES1) = dm_game_res(1);
    {
        float a = 19.0f / 9.0f; int wide = 0;
        for (int i = 0; i < 2; i++) { uint64_t r = front_game_res(i); float w = (float)(uint32_t)r, h = (float)(uint32_t)(r >> 32); if (h > 0 && w / h > a) { a = w / h; wide = 1; } }
        float ui = wide ? a + 0.15f : a;
        uint32_t bits;
        memcpy(&bits, &a, 4); *(uint64_t *)(blk + DM_GUEST_SLOT_KASPECT) = bits;
        memcpy(&bits, &ui, 4); *(uint64_t *)(blk + DM_GUEST_SLOT_KASPECTUI) = bits;
    }
    pthread_once(&g_keyown_once, own_key_init); pthread_setspecific(g_keytls, blk); pthread_setspecific(g_keytlr, tlr); pthread_setspecific(g_keyown, own);
}

// 客体槽基址：Android 上 tpidrro_el0 不可用（实测写会 SIGILL、读恒为 0），改写器把
// 「mrs xN, tpidrro_el0」换成经 thunk 取本地址，其余（key 页偏移 + 载入/存出）保持上游原样。
// Guest slot base: TPIDRRO_EL0 is unusable on Android (measured: writes SIGILL, reads are always 0),
// so the rewriter turns "mrs xN, tpidrro_el0" into a thunk call returning this address; everything
// else (key-page offset + load/store) stays exactly as upstream.
void *dm_guest_tls_base(void) { return pthread_getspecific(g_keytls); }

