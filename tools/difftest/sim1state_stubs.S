/* Deterministic volatile return registers expose caller dependencies without
 * letting logging code's own compiled-C leftovers become an accidental model.
 */
.text
.macro stub name, core, bytes
.globl _\name\()@\bytes
_\name\()@\bytes:
    .rept \bytes/4
        pushl \bytes(%esp)
    .endr
    call _\core\()@\bytes
    movl $0xabc01234, %ecx
    movl $0xdef05678, %edx
    ret $\bytes
.endm
stub state_begin, state_begin_core, 8
stub state_write, state_write_core, 12
stub state_alloc, state_alloc_core, 16
stub state_free, state_free_core, 4
stub state_remove, state_remove_core, 8
stub state_find, state_find_core, 12
