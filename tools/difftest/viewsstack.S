/* Identical caller stack for either reference or rebuilt thiscall handler.
 * CALL pushes a return address, so handler entry ESP-16 is our current ESP-20.
 * No nonvolatile registers are modified by this driver. */
.text
.p2align 4
.globl _fs98_views_test_call
.def _fs98_views_test_call; .scl 2; .type 32; .endef
_fs98_views_test_call:
    movl 4(%esp), %eax
    movl 8(%esp), %ecx
    movl 12(%esp), %edx
    movl %edx, -20(%esp)
    call *%eax
    ret
