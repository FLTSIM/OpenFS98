/* The module ABI's base types (AD-14, AD-4).
 *
 * FS98 memory is 32-bit on every build, so a shared layout or service signature
 * never uses a C pointer.  Data is an `fs98_addr`, code an `fs98_code`.  On
 * win32-retail both are the real 32-bit values; elsewhere they are arena
 * offsets and code handles (AD-17), which Story 1.8 introduces.
 *
 * Only <stdint.h> and "retail_abi.h" (preprocessor lines only) may be included here: every other fs98 header
 * includes this one. */
#ifndef FS98_ABI_H
#define FS98_ABI_H

#include <stdint.h>
#include "retail_abi.h"

typedef uint32_t fs98_addr;     /* a data address: an offset into the arena */
typedef uint32_t fs98_code;     /* a code handle */
typedef uint16_t FS98_WCHAR;    /* a Win32 WCHAR; the literal macro FS98_W is deferred (u"" needs C11) */

_Static_assert(sizeof(fs98_addr) == 4, "fs98_addr is 32-bit");
_Static_assert(sizeof(fs98_code) == 4, "fs98_code is 32-bit");
_Static_assert(sizeof(FS98_WCHAR) == 2, "FS98_WCHAR is 16-bit");

/* stdcall only where retail's own convention applies: win32-retail, the one Windows i386 build.
 * Other targets take their platform default (AD-4). */
#if defined(FS98_RETAIL_ABI)
#  define FS98_ABI_STDCALL_ __attribute__((stdcall))
#else
#  define FS98_ABI_STDCALL_
#endif
#define FS98_STDCALL FS98_ABI_STDCALL_

/* RETAIL-ONLY, TRANSITIONAL (the Story 1.7 bridge, removed with it in its own story, deferred-work.md): read an
 * `fs98_addr` field as the native pointer it holds on win32-retail.  The load is pointer-typed, which keeps the
 * code gcc generates for the pointer fields this replaced byte-identical. */
#if defined(FS98_RETAIL_ABI)
#  define FS98_RETAIL_PTR(type, field) (*(type *)&(field))
#endif

#endif /* FS98_ABI_H */
