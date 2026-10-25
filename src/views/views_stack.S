/* Retail's uninitialized Map metric flag and Zoom float both live at entry
 * ESP-16. Capture those bytes before a compiled-C frame can overwrite them.
 * Valid units / successful sscanf replace the captured value in the C body.
 * These entry points otherwise retain the original no-argument thiscall ABI. */
.text
.p2align 4
.globl _fs98_views_map
.def _fs98_views_map; .scl 2; .type 32; .endef
_fs98_views_map:
    movl -16(%esp), %eax
    pushl %eax
    pushl %ecx
    call _fs98_views_map_body
    addl $8, %esp
    ret

.p2align 4
.globl _fs98_views_zoom
.def _fs98_views_zoom; .scl 2; .type 32; .endef
_fs98_views_zoom:
    movl -16(%esp), %eax
    pushl %eax
    pushl %ecx
    call _fs98_views_zoom_body
    addl $8, %esp
    ret
