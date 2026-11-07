.text
.macro stub name, bytes
.globl _\name\()@\bytes
_\name\()@\bytes:
    .rept \bytes/4
        pushl \bytes(%esp)
    .endr
    call _\name\()_core@\bytes
    movl $0xabc01234, %ecx
    movl $0xdef05678, %edx
    ret $\bytes
.endm
stub decode_move, 12
stub decode_message, 8
stub decode_scenery, 4

.globl _decode_rand
_decode_rand:
    movl _decode_random, %eax
    ret

/* The decoder harness resumes here only after a paired invalid AIR fixture
 * faults. The observer restores its actual caller stack; modeled GPRs at an
 * interrupted instruction are not compared as function return values.
 */
.globl _decode_recover
_decode_recover:
    fnsave _call_fp
    movl $0, _call_registers+28
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

/* Known answers for every register, flags and stack cleanup. Callees
 * intentionally clobber nonvolatiles: SIM1's native bridge captures them and
 * must still return to its C caller with the actual C ABI intact.
 */
.macro probe bytes
.globl _decode_probe_\bytes
_decode_probe_\bytes:
    xorl %eax,%eax
    .set offset,4
    .rept \bytes/4
        addl offset(%esp),%eax
        .set offset,offset+4
    .endr
    movl $0x11112222,%ecx
    movl $0x33334444,%edx
    movl $0x55556666,%ebx
    movl $0x77778888,%ebp
    movl $0x9999aaaa,%esi
    movl $0xbbbbcccc,%edi
    pushl $0xad7
    popfl
    ret $\bytes
.endm
probe 0
probe 4
probe 12
probe 36
probe 256

.globl _decode_probe_nested
_decode_probe_nested:
    call _decode_nested_core
    jmp _decode_probe_0

.globl _decode_probe_cdecl
_decode_probe_cdecl:
    movl 4(%esp),%eax
    addl 8(%esp),%eax
    addl 12(%esp),%eax
    movl $0x11112222,%ecx
    movl $0x33334444,%edx
    movl $0x55556666,%ebx
    movl $0x77778888,%ebp
    movl $0x9999aaaa,%esi
    movl $0xbbbbcccc,%edi
    pushl $0x202
    popfl
    ret

.globl _decode_probe_fp
_decode_probe_fp:
    fld1
    jmp _decode_probe_0
