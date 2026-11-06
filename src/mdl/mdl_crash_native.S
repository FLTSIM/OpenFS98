/* Native imported-service calls with explicit virtual register transfer.
 * f: eax,ecx,edx,ebx,esp,ebp,esi,edi,eflags (nine uint32_t words).
 * All imported services used by crash models are stdcall, 1..3 arguments.
 */
.text
.macro native count
.globl _mdl_native\count
_mdl_native\count:
    pushfl
    pushal
    movl 40(%esp), %eax
    movl 44(%esp), %edx
    pushl %eax
    pushl %edx
    leal 56(%eax), %esi
.if \count >= 3
    pushl 8(%esi)
.endif
.if \count >= 2
    pushl 4(%esi)
.endif
    pushl (%esi)
    movl 4(%eax), %ecx
    movl 8(%eax), %edx
    movl 12(%eax), %ebx
    movl 20(%eax), %ebp
    movl 24(%eax), %esi
    movl 28(%eax), %edi
    pushl 32(%eax)
    popfl
    movl (%eax), %eax
    call *(\count * 4)(%esp)
    pushfl
    pushal
    movl 40(%esp), %eax
    movl 28(%esp), %edx
    movl %edx, (%eax)
    movl 24(%esp), %edx
    movl %edx, 4(%eax)
    movl 20(%esp), %edx
    movl %edx, 8(%eax)
    movl 16(%esp), %edx
    movl %edx, 12(%eax)
    movl 8(%esp), %edx
    movl %edx, 20(%eax)
    movl 4(%esp), %edx
    movl %edx, 24(%eax)
    movl (%esp), %edx
    movl %edx, 28(%eax)
    movl 32(%esp), %edx
    movl %edx, 32(%eax)
    addl $44, %esp
    popal
    popfl
    ret
.endm
native 1
native 2
native 3
