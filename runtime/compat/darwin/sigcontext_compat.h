/* Android signal-context compatibility.
 * Bionic exposes Linux AArch64's sigcontext directly: regs[31], sp, pc,
 * pstate and fault_address. The ESR is in the optional signal context chain;
 * this shim intentionally returns zero when that optional record is absent.
 */
#ifndef DM_COMPAT_SIGCONTEXT_H
#define DM_COMPAT_SIGCONTEXT_H
#include <signal.h>
#include <sys/ucontext.h>

#if defined(__aarch64__)
#define DM_UC_PC(uc)       ((uc)->uc_mcontext.pc)
#define DM_UC_LR(uc)       ((uc)->uc_mcontext.regs[30])
#define DM_UC_SP(uc)       ((uc)->uc_mcontext.sp)
#define DM_UC_FP(uc)       ((uc)->uc_mcontext.regs[29])
#define DM_UC_X(uc, n)     ((uc)->uc_mcontext.regs[(n)])
#define DM_UC_ESR(uc)      (0ull)
#else
#define DM_UC_PC(uc)       ((uc)->uc_mcontext.pc)
#define DM_UC_LR(uc)       ((uc)->uc_mcontext.regs[30])
#define DM_UC_SP(uc)       ((uc)->uc_mcontext.sp)
#define DM_UC_FP(uc)       ((uc)->uc_mcontext.regs[29])
#define DM_UC_X(uc, n)     ((uc)->uc_mcontext.regs[(n)])
#define DM_UC_ESR(uc)      (0ull)
#endif
#endif
