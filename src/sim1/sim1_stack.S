/* Volatile register results are observable at the retail module boundary. */
.text
.globl _sim1_descriptor
_sim1_descriptor:
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl 16(%esp)
    call _sim1_descriptor_core@4
    popl %edx
    popl %ecx
    popl %eax
    ret $4

.globl _sim1_word
_sim1_word:
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl 16(%esp)
    call _sim1_word_core@4
    movw %ax, 8(%esp)
    popl %edx
    popl %ecx
    popl %eax
    ret $4

.globl _sim1_scaled
_sim1_scaled:
    pushl %edx
    pushl 8(%esp)
    call _sim1_scaled_core@4
    movl 8(%esp), %ecx
    popl %edx
    ret $4

.globl _sim1_vector
_sim1_vector:
    pushl 12(%esp)
    pushl 12(%esp)
    pushl 12(%esp)
    call _sim1_vector_core@12
    movl 8(%esp), %ecx
    movl 12(%esp), %edx
    ret $12

.globl _sim1_reset
_sim1_reset:
    pushl %ecx
    pushl %edx
    pushl 12(%esp)
    call _sim1_reset_core@4
    popl %edx
    popl %ecx
    ret $4

.globl _sim1_wind
_sim1_wind:
    pushl %eax
    pushl %ecx
    pushl %edx
    call _sim1_wind_core
    popl %edx
    popl %ecx
    popl %eax
    ret
