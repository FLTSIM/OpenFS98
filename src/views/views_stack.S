#include "../../include/fs98/retail_abi.h"
#if defined(FS98_RETAIL_ABI)
/* Retail's uninitialized Map metric flag and Zoom float both live at entry
 * ESP-16. Capture those bytes before a compiled-C frame can overwrite them.
 * Valid units / successful sscanf replace the captured value in the C body.
 * These entry points otherwise retain the original no-argument thiscall ABI. */
.text
.p2align 4
.globl _fs98_views_map
.def _fs98_views_map; .scl 2; .type 32; .endef
_fs98_views_map:
    /* RETAIL BUG (kept): VIEW-1 -- the metric flag is the uninitialised word at entry ESP-16. */
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
    /* RETAIL BUG (kept): VIEW-1 -- the zoom float is the uninitialised word at entry ESP-16. */
    movl -16(%esp), %eax
    pushl %eax
    pushl %ecx
    call _fs98_views_zoom_body
    addl $8, %esp
    ret
#endif
