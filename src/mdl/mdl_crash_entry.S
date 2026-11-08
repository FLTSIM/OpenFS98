/* The crash models' entry points whose registers C does not give (see mdl_crash.c).
 * Linkage +0x04/+0x08 are bare returns and +0x18 writes the header pointer, as in the
 * normal models; +0x1c and +0x20 are crash_init and crash_detach themselves. */
.text
.globl _crash_nop
_crash_nop:
    ret

.globl _crash_attach@4
_crash_attach@4:
    movl 4(%esp), %eax
    movl $_mdl_header, (%eax)
    ret $4

/* header +0x10: retail returns with edi = GLOBAL, which WINDOW reads */
.globl _crash_update_entry
_crash_update_entry:
    pushl 4(%esp)
    call _crash_update@4
    movl _ImportTable+12, %edi
    ret $4

/* CHAIN list 15 (a, b): retail's last act is G3D +0x18 (program, state), entered with eax
 * G3D's Linkage and the callback's own ecx and edx, so the callback returns with G3D's eax,
 * ecx and edx, which CHAIN passes on */
.globl _crash_chain_entry
_crash_chain_entry:
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl %ecx
    pushl %edx
    subl $8, %esp
    pushl %esp
    call _crash_chain_prepare@4
    movl 8(%esp), %edx
    movl 12(%esp), %ecx
    call *0x18(%eax)
    addl $8, %esp
    popl %edi
    popl %esi
    popl %ebx
    ret $8
