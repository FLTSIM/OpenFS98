/* Entries that keep registers, as retail's hand-written services do.
 *
 * Retail's assembly modules keep values in registers across calls into each other, so what
 * a service leaves is part of its contract, and C does not honour it.  tools/reg_contracts.sh
 * finds where that differs.  `symbol` is the function's assembler name and `bytes` what it
 * pops.  x86 only: on anything else the slot holds the function itself.
 */
#ifndef FS98_KEEP_H
#define FS98_KEEP_H

#if defined(__i386__)
#define FS98_ENTRY(entry, symbol, bytes, count, saves, restores) \
    __asm__(".text\n.globl _" #entry "\n_" #entry ":\n\t" saves \
            ".rept " #bytes "/4\n\tpushl 4*" #count "+" #bytes "(%esp)\n\t.endr\n\t" \
            "call " symbol "\n\t" restores "ret $" #bytes "\n");
#define FS98_KEEP_ALL(entry, symbol, bytes) \
    FS98_ENTRY(entry, symbol, bytes, 3, "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t", \
               "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\t")
#define FS98_KEEP_ECX_EDX(entry, symbol, bytes) \
    FS98_ENTRY(entry, symbol, bytes, 2, "pushl %ecx\n\tpushl %edx\n\t", "popl %edx\n\tpopl %ecx\n\t")
#define FS98_KEEP_ECX(entry, symbol, bytes) \
    FS98_ENTRY(entry, symbol, bytes, 1, "pushl %ecx\n\t", "popl %ecx\n\t")
#define FS98_KEEP_EDX(entry, symbol, bytes) \
    FS98_ENTRY(entry, symbol, bytes, 1, "pushl %edx\n\t", "popl %edx\n\t")
#define FS98_KEPT(entry, function) entry
#else
#define FS98_KEEP_ALL(entry, symbol, bytes)
#define FS98_KEEP_ECX_EDX(entry, symbol, bytes)
#define FS98_KEEP_ECX(entry, symbol, bytes)
#define FS98_KEEP_EDX(entry, symbol, bytes)
#define FS98_KEPT(entry, function) function
#endif

#endif
