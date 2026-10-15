.text
.globl tls_probe_tpidr_read
.type tls_probe_tpidr_read,%function
tls_probe_tpidr_read:
    mrs x0, tpidr_el0
    ret
.globl tls_probe_tpidr_write
.type tls_probe_tpidr_write,%function
tls_probe_tpidr_write:
    msr tpidr_el0, x0
    ret
