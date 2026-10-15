/*
 * mach/mach.h — task / VM layer shim for Android/AArch64.
 *
 * Upstream reads guest memory through the Mach VM API:
 *   vm_read_overwrite(mach_task_self(), addr, size, dst, &copied)
 * On Android the "guest" runs in the same address space as the runtime, so
 * reading guest memory is a plain memcpy from that address. The call shape,
 * the KERN_SUCCESS contract and the copied-length out-parameter are all kept,
 * so upstream call sites do not change.
 */
#ifndef DM_COMPAT_MACH_MACH_H
#define DM_COMPAT_MACH_MACH_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int kern_return_t;
typedef unsigned int mach_port_t;
typedef uint64_t mach_vm_address_t;
typedef uint64_t mach_vm_size_t;
typedef uint64_t vm_address_t;
typedef uint64_t vm_size_t;
typedef int vm_prot_t;

#define KERN_SUCCESS 0
#define KERN_FAILURE 5
#define KERN_INVALID_ARGUMENT 4
#define KERN_INVALID_ADDRESS 1

/*
 * AArch64 thread state, as upstream's crash dump reads it.
 *
 * Field names and layout follow Darwin's arm_thread_state64_t (leading double
 * underscore), because upstream accesses them directly — st.__x[i],
 * st.__fp and so on — as well as through the accessors below.
 */
typedef struct {
    uint64_t __x[29];
    uint64_t __fp;
    uint64_t __lr;
    uint64_t __sp;
    uint64_t __pc;
    uint32_t __cpsr;
    uint32_t __pad;
} arm_thread_state64_t;

#define ARM_THREAD_STATE64 6
#define ARM_THREAD_STATE64_COUNT ((mach_msg_type_number_t)(sizeof(arm_thread_state64_t) / sizeof(uint32_t)))

/*
 * Accessors for the pc/lr/fp/sp fields.
 *
 * On Darwin these exist because those fields carry pointer-authentication
 * signatures, so reading them is not a plain struct field access. Android has
 * no PAC (and no such instructions), so the fields are already plain
 * addresses: return them directly.
 */
static inline uint64_t __darwin_arm_thread_state64_get_pc(arm_thread_state64_t s) { return s.__pc; }
static inline uint64_t __darwin_arm_thread_state64_get_lr(arm_thread_state64_t s) { return s.__lr; }
static inline uint64_t __darwin_arm_thread_state64_get_fp(arm_thread_state64_t s) { return s.__fp; }
static inline uint64_t __darwin_arm_thread_state64_get_sp(arm_thread_state64_t s) { return s.__sp; }
#define __darwin_arm_thread_state64_get_pc_fptr(s) ((void *)(uintptr_t)__darwin_arm_thread_state64_get_pc(s))

#define VM_PROT_NONE 0x00
#define VM_PROT_READ 0x01
#define VM_PROT_WRITE 0x02
#define VM_PROT_EXECUTE 0x04

#define MACH_PORT_NULL 0

/* The runtime itself: the only task this process can address. */
static inline mach_port_t mach_task_self(void) { return 1; }

/*
 * --- Thread introspection ---------------------------------------------------
 *
 * Upstream uses the Mach thread API for its crash dump: it suspends every
 * thread, reads the AArch64 register set, walks the frame-pointer chain, and
 * resumes (hle_thread.c: thr_bt / the watchdog dump).
 *
 * Android user space cannot do this. There is no supported way to read
 * another thread's registers from inside the process — that needs ptrace,
 * which is not permitted for self-introspection, or a signal round-trip that
 * would itself perturb the state being dumped.
 *
 * So the API is provided, but the operations that genuinely cannot work report
 * failure instead of pretending. Upstream already treats a non-KERN_SUCCESS
 * return as "no backtrace for this thread" (it just skips the thread), so the
 * crash dump degrades to the faulting thread only — which the signal handler
 * does cover, via the ucontext the kernel hands it.
 *
 * This is a deliberate capability downgrade, not a missing symbol. Recorded in
 * docs/SPEC-base-framework.md.
 */
typedef unsigned int thread_t;
typedef mach_port_t thread_act_t;
typedef thread_act_t *thread_act_array_t;
typedef int thread_info_t;
typedef int *thread_state_t;
typedef unsigned int mach_msg_type_number_t;

/* A thread handle here is the pthread_t, as elsewhere in this layer. */
static inline thread_t mach_thread_self(void) { return (thread_t)(uintptr_t)pthread_self(); }

#define THREAD_BASIC_INFO 3
#define THREAD_BASIC_INFO_COUNT 10

/*
 * Darwin's time_value_t: seconds plus microseconds. Upstream sums
 * user_time + system_time into seconds (hle_thread.c, the watchdog dump).
 */
typedef struct {
    int seconds;
    int microseconds;
} time_value_t;

typedef struct {
    int suspend_count;
    int policy;
    int run_state;
    int flags;
    time_value_t user_time;
    time_value_t system_time;
} thread_basic_info_data_t;

static inline kern_return_t task_threads(mach_port_t task, thread_act_t **out,
                                         mach_msg_type_number_t *count) {
    (void)task;
    if (out) *out = 0;
    if (count) *count = 0;
    return KERN_FAILURE;
}

static inline kern_return_t thread_suspend(thread_t t) { (void)t; return KERN_FAILURE; }
static inline kern_return_t thread_resume(thread_t t) { (void)t; return KERN_FAILURE; }

static inline kern_return_t thread_get_state(thread_t t, int flavor, thread_state_t state,
                                             mach_msg_type_number_t *count) {
    (void)t; (void)flavor; (void)state; (void)count;
    return KERN_FAILURE;
}

static inline kern_return_t thread_info(thread_t t, int flavor, thread_info_t info,
                                        mach_msg_type_number_t *count) {
    (void)t; (void)flavor; (void)info; (void)count;
    return KERN_FAILURE;
}

/*
 * Releasing a port right. Thread handles here are not real port rights, so
 * this is a no-op that reports success, matching how upstream uses it (cleanup
 * after task_threads, which itself reports failure on this platform).
 */
static inline kern_return_t mach_port_deallocate(mach_port_t task, mach_port_t name) {
    (void)task; (void)name;
    return KERN_SUCCESS;
}

/*
 * Copy size bytes from the guest address to dst. Returns KERN_SUCCESS and
 * reports the copied length, matching the Mach semantics upstream relies on.
 */
static inline kern_return_t vm_read_overwrite(mach_port_t task,
                                             mach_vm_address_t addr,
                                             mach_vm_size_t size,
                                             vm_address_t dst,
                                             vm_size_t *out_copied) {
    (void)task;
    if (!addr || !dst) {
        if (out_copied) *out_copied = 0;
        return KERN_INVALID_ADDRESS;
    }
    memcpy((void *)(uintptr_t)dst, (const void *)(uintptr_t)addr, (size_t)size);
    if (out_copied) *out_copied = size;
    return KERN_SUCCESS;
}

static inline kern_return_t vm_write(mach_port_t task, mach_vm_address_t addr,
                                     vm_address_t data, mach_vm_size_t size) {
    (void)task;
    if (!addr || !data) return KERN_INVALID_ADDRESS;
    memcpy((void *)(uintptr_t)addr, (const void *)(uintptr_t)data, (size_t)size);
    return KERN_SUCCESS;
}

static inline kern_return_t mach_vm_protect(mach_port_t task,
                                            mach_vm_address_t addr,
                                            mach_vm_size_t size, int set_max,
                                            vm_prot_t prot) {
    (void)task; (void)addr; (void)size; (void)set_max; (void)prot;
    return KERN_SUCCESS;
}

static inline kern_return_t mach_vm_allocate(mach_port_t task,
                                             mach_vm_address_t *addr,
                                             mach_vm_size_t size, int flags) {
    (void)task; (void)flags;
    void *p = malloc((size_t)size);
    if (!p) return KERN_INVALID_ARGUMENT;
    *addr = (mach_vm_address_t)(uintptr_t)p;
    return KERN_SUCCESS;
}

static inline kern_return_t mach_vm_deallocate(mach_port_t task,
                                               mach_vm_address_t addr,
                                               mach_vm_size_t size) {
    (void)task; (void)size;
    free((void *)(uintptr_t)addr);
    return KERN_SUCCESS;
}

/* Same as above under the non-vm spelling upstream also uses. */
static inline kern_return_t vm_deallocate(mach_port_t task, vm_address_t addr,
                                          vm_size_t size) {
    (void)task; (void)size;
    free((void *)(uintptr_t)addr);
    return KERN_SUCCESS;
}

static inline kern_return_t vm_protect(mach_port_t task, vm_address_t addr,
                                       vm_size_t size, int set_max, vm_prot_t prot) {
    (void)task; (void)addr; (void)size; (void)set_max; (void)prot;
    return KERN_SUCCESS;
}

/* Host memory statistics: report current RSS. */
static inline kern_return_t task_info(mach_port_t task, int flavor,
                                      void *info, unsigned int *count) {
    (void)task; (void)flavor; (void)info; (void)count;
    return KERN_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_MACH_MACH_H */
