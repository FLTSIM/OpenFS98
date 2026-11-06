/* Capture the ABI without making the C compiler interpret the returned x87
 * value. FNSAVE records TOP/tags/status and 80-bit values before resetting it.
 */
.text
.globl _sim1_call
_sim1_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, _call_sp
    fninit
    fldcw _call_control
    cmpl $0, _call_preload
    je 1f
    fld1
    fldpi
1:
    cmpl $3, _call_argc
    jne 2f
    pushl _call_args+8
    pushl _call_args+4
2:
    cmpl $0, _call_argc
    je 3f
    pushl _call_args
3:
    movl _call_inputs, %eax
    movl _call_inputs+4, %ebx
    movl _call_inputs+8, %ecx
    movl _call_inputs+12, %edx
    movl _call_inputs+16, %esi
    movl _call_inputs+20, %edi
    movl _call_inputs+24, %ebp
    call *_call_target
    movl %eax, _call_registers
    movl %ebx, _call_registers+4
    movl %ecx, _call_registers+8
    movl %edx, _call_registers+12
    movl %esi, _call_registers+16
    movl %edi, _call_registers+20
    movl %ebp, _call_registers+24
    movl %esp, %eax
    subl _call_sp, %eax
    movl %eax, _call_registers+28
    fnsave _call_fp
    movl _call_sp, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
