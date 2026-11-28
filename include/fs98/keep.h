/* Entries that keep registers, as retail's hand-written services do.
 *
 * Retail's assembly modules keep values in registers across calls into each other, so what
 * a service leaves is part of its contract, and C does not honour it.  tools/reg_contracts.sh
 * finds where that differs.  `symbol` is the function's assembler name and `bytes` what it
 * pops.  win32-retail only (FS98_RETAIL_ABI): on anything else the slot holds the function itself.
 * tools/check_keep_entries.py pins every entry here to tools/keep_entries.txt.
 */
#ifndef FS98_KEEP_H
#define FS98_KEEP_H

#include "retail_abi.h"

#if defined(FS98_RETAIL_ABI)
#define FS98_ENTRY(entry, symbol, bytes, count, saves, restores) \
    __asm__(".text\n.globl _" #entry "\n_" #entry ":\n\t" saves \
            ".rept " #bytes "/4\n\tpushl 4*" #count "+" #bytes "(%esp)\n\t.endr\n\t" \
            "call " symbol "\n\t" restores "ret $" #bytes "\n");
/* An entry that pops nothing: a plain `ret`, no stack copy (UTIL's rand and rand32, `count` kept for symmetry). */
#define FS98_ENTRY_NOPOP(entry, symbol, count, saves, restores) \
    __asm__(".text\n.globl _" #entry "\n_" #entry ":\n\t" saves "call " symbol "\n\t" restores "ret\n");
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
#define FS98_ENTRY_NOPOP(entry, symbol, count, saves, restores)
#define FS98_KEEP_ALL(entry, symbol, bytes)
#define FS98_KEEP_ECX_EDX(entry, symbol, bytes)
#define FS98_KEEP_ECX(entry, symbol, bytes)
#define FS98_KEEP_EDX(entry, symbol, bytes)
#define FS98_KEPT(entry, function) function
#endif

#endif
