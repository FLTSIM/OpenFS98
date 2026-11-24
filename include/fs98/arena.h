/* The one home of the address macros (AD-15).
 *
 * FS98 memory is addressed by `fs98_addr`, an offset into the arena.  Every access goes through
 * `fs98_ptr` and every pointer-to-address conversion through `fs98_handle`, so that only this header
 * knows where the arena is.  On i386 (retail and native linux-i386) the base is the constant 0 and
 * both are plain casts; elsewhere the base is a variable that Epic 3 defines.
 *
 * A module includes this directly, after its system and ABI includes.  No shared header includes it
 * during the migration: an unconverted module's private M32 or D would collide under -Werror.
 * `tools/check_arena.py` fails a build on a second definition of any name below, or on a
 * `(uint32_t)(uintptr_t)` cast, in src/ outside src/platform/arena/.
 *
 * A module that wants `D`, `RDATA` or `DA` defines `FS98_DATA_IMAGE` (the whole object that is its
 * .data image), `FS98_RDATA_IMAGE` or `FS98_DATA_VA` before including this. */
#ifndef FS98_ARENA_H
#define FS98_ARENA_H

#include <stdint.h>
#include "fs98/abi.h"

/* Access types.  The byte type has no may_alias: it already aliases everything. */
typedef volatile uint8_t                                fs98_v8;
typedef volatile uint16_t __attribute__((may_alias))    fs98_v16;
typedef volatile uint32_t __attribute__((may_alias))    fs98_v32;
typedef volatile float    __attribute__((may_alias))    fs98_vf;
typedef volatile double   __attribute__((may_alias))    fs98_vd;

#if defined(__i386__)
#  define FS98_ARENA_BASE 0
#else
extern uintptr_t fs98_arena_base;   /* defined by Epic 3 */
#  define FS98_ARENA_BASE fs98_arena_base
#endif

/* an address to a pointer */
#define fs98_ptr(a) ((void *)(FS98_ARENA_BASE + (uintptr_t)(uint32_t)(a)))

/* a pointer (struct, array, volatile or function) to an address; on i386 any 32-bit pointer is in the
 * arena.  Elsewhere it needs Epic 3's arena bounds and is not defined yet. */
#if defined(__i386__)
#  define fs98_handle(p) ((fs98_addr)(uintptr_t)(p))
#endif

/* a real code value, such as a stored callback, to a pointer.  Not for slot loads (a slot holds a
 * pointer to the code, not the code). */
#if defined(_WIN32) && defined(__i386__)
#  define fs98_call(code, fn_type) ((fn_type *)(uintptr_t)(code))
#endif

/* memory access */
#define M8(a)   (*(fs98_v8  *)fs98_ptr(a))
#define M16(a)  (*(fs98_v16 *)fs98_ptr(a))
#define M32(a)  (*(fs98_v32 *)fs98_ptr(a))
#define MF(a)   (*(fs98_vf  *)fs98_ptr(a))
#define MD(a)   (*(fs98_vd  *)fs98_ptr(a))

/* record fields: a record's address plus a byte offset */
#define R8(p, o)   M8((uint32_t)(p) + (o))
#define R16(p, o)  M16((uint32_t)(p) + (o))
#define R32(p, o)  M32((uint32_t)(p) + (o))
#define RF(p, o)   MF((uint32_t)(p) + (o))
#define RDB(p, o)  MD((uint32_t)(p) + (o))

/* module images */
#ifdef FS98_DATA_IMAGE
#  define D       fs98_handle(&FS98_DATA_IMAGE)
#endif
#ifdef FS98_RDATA_IMAGE
#  define RDATA   fs98_handle(&FS98_RDATA_IMAGE)
#endif
#ifdef FS98_DATA_VA
#  define DA(va)  (D + ((uint32_t)(va) - FS98_DATA_VA))
#endif

#endif /* FS98_ARENA_H */
