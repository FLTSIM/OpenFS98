.text
.macro air_stub name, bytes
.globl _air_\name\()@\bytes
_air_\name\()@\bytes:
    .rept \bytes/4
        pushl \bytes(%esp)
    .endr
    call _air_\name\()_core@\bytes
    movl $0xabc01234, %ecx
    movl $0xdef05678, %edx
    ret $\bytes
.endm
air_stub query, 16
air_stub open, 16
air_stub size, 8
air_stub read, 16
air_stub alloc, 16
air_stub free, 4
air_stub close, 4
air_stub parser, 8
air_stub move, 12
air_stub add, 8
air_stub remove, 8
