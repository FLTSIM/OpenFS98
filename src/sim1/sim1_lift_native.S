/* All-GPR native boundary. The modeled ESP points into the entry's real
 * stack allocation, within the Windows stack bounds used by SEH. TLS permits
 * callbacks into SIM1 while an imported service is still on that stack.
 */
.text
/* Probe the real thread stack, then place the modeled ESP below the C
 * interpreter's frames. Native callees and callbacks grow downwards from
 * there, so a nested entry cannot overwrite suspended interpreter frames.
 * __chkstk_ms probes without moving ESP, as in a normal compiler prologue.
 */
.globl _sim1_lift_stack
_sim1_lift_stack:
    movl $98304, %eax
    call ___chkstk_ms
    leal -65536(%esp), %eax
    ret

.globl _sim1_lift_native
_sim1_lift_native:
    pushfl
    pushal
    movl 40(%esp), %eax
    movl %esp, 68(%eax)
    movl 44(%esp), %edx
    movl 16(%eax), %esp
    pushl $native_return
    pushl %edx
    pushl 32(%eax)
    popfl
    movl 4(%eax), %ecx
    movl 8(%eax), %edx
    movl 12(%eax), %ebx
    movl 20(%eax), %ebp
    movl 24(%eax), %esi
    movl 28(%eax), %edi
    movl (%eax), %eax
    ret
native_return:
    pushfl
    pushal
    movl %esp, %ebx
    pushl _sim1_lift_tls
    call _TlsGetValue@4
    movl 28(%ebx), %edx
    movl %edx, 0(%eax)
    movl 24(%ebx), %edx
    movl %edx, 4(%eax)
    movl 20(%ebx), %edx
    movl %edx, 8(%eax)
    movl 16(%ebx), %edx
    movl %edx, 12(%eax)
    movl 12(%ebx), %edx
    addl $4, %edx
    movl %edx, 16(%eax)
    movl 8(%ebx), %edx
    movl %edx, 20(%eax)
    movl 4(%ebx), %edx
    movl %edx, 24(%eax)
    movl 0(%ebx), %edx
    movl %edx, 28(%eax)
    movl 32(%ebx), %edx
    movl %edx, 32(%eax)
    movl 68(%eax), %esp
    popal
    popfl
    ret
