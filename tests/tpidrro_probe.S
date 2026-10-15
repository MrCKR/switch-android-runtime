.text
.globl tls_probe_read
.type tls_probe_read,%function
tls_probe_read:
    mrs x0, tpidrro_el0
    ret
.globl tls_probe_write
.type tls_probe_write,%function
tls_probe_write:
    msr tpidrro_el0, x0
    ret
