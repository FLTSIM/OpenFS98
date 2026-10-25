/* About publishes a local Name buffer through descriptor+8. Retain its
 * retail address, entry ESP-0x270, rather than changing the dangling pointer
 * into persistent storage. The C body lives below this reserved local area. */
.text
.p2align 4
.globl _fs98_help_about_populate
.def _fs98_help_about_populate; .scl 2; .type 32; .endef
_fs98_help_about_populate:
    subl $0x318, %esp
    movl %esp, %eax
    pushl %eax
    pushl %ecx
    call _fs98_help_about_populate_body
    addl $0x320, %esp
    ret

/* OnInit's internal populate call enters at OnInit entry ESP-0x24. Its Name
 * pointer therefore lands at OnInit entry ESP-0x294. Reserve both frames. */
.p2align 4
.globl _fs98_help_about_init
.def _fs98_help_about_init; .scl 2; .type 32; .endef
_fs98_help_about_init:
    subl $0x33c, %esp
    movl %esp, %eax
    pushl %eax
    pushl %ecx
    call _fs98_help_about_init_body
    addl $0x344, %esp
    ret
