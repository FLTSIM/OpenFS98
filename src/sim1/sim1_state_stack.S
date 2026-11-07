/* Explicit saved registers keep the caller-visible compiled-C leftovers
 * reproducible while record/ownership logic is expressed in C.
 */
.text
.macro state_entry name, core, bytes
.globl _\name
_\name:
    pushal
    movl %esp, %eax
    .if \bytes == 8
        pushl 40(%esp)
        pushl 40(%esp)
    .elseif \bytes == 4
        pushl 36(%esp)
    .endif
    pushl %eax
    call _\core
    addl $(\bytes + 4), %esp
    popal
    ret $\bytes
.endm

state_entry sim1_record_write, sim1_record_write_core, 4
state_entry sim1_record_read, sim1_record_read_core, 8
state_entry sim1_legacy_read, sim1_legacy_read_core, 4
state_entry sim1_restore_state, sim1_restore_state_core, 0
state_entry sim1_save_situation, sim1_save_situation_core, 0
state_entry sim1_retain_state, sim1_retain_state_core, 0
state_entry sim1_deinit, sim1_deinit_core, 0
state_entry sim1_situation_callback, sim1_situation_core, 8
state_entry sim1_load_air, sim1_load_air_core, 0
state_entry sim1_publish, sim1_publish_core, 0
state_entry sim1_init, sim1_init_core, 0

/* Init/deinit register/remove these identities. Execution of tick/frame
 * callbacks remains unsupported until the simulation routines are recovered.
 */
#ifndef SIM1_COMPLETE
.globl _sim1_chain_tick
_sim1_chain_tick:
    jmp _sim1_unsupported
.globl _sim1_chain_frame
_sim1_chain_frame:
    jmp _sim1_unsupported
#endif

/* cdecl(registers, function, argument_count, arguments). The imports obey
 * stdcall; seed/capture EAX/ECX/EDX instead of inventing callee leftovers.
 */
.globl _sim1_import
_sim1_import:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, %ebx
    movl 8(%ebp), %esi
    movl 16(%ebp), %ecx
    movl 20(%ebp), %edi
1:
    testl %ecx, %ecx
    jz 2f
    decl %ecx
    pushl (%edi,%ecx,4)
    jmp 1b
2:
    movl 28(%esi), %eax
    movl 24(%esi), %ecx
    movl 20(%esi), %edx
    call *12(%ebp)
    movl %eax, 28(%esi)
    movl %ecx, 24(%esi)
    movl %edx, 20(%esi)
    movl %ebx, %esp
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret
