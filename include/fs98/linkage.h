/* FS98 module ABI.
 *
 * Reverse engineered from the retail 1997-08-07 build; see docs/architecture.md.
 * Every FS98 module -- the DLLs in Modules/ and the .GAU gauges -- is a plain
 * Win32 DLL that exports exactly two symbols as DATA:
 *
 *   Linkage      this module's identity + service table
 *   ImportTable  the modules it needs, by numeric id
 *
 * FLTSIM98.EXE enumerates every DLL in Modules/, LoadLibrary's each one, reads the
 * module id out of Linkage, then walks every ImportTable and fills in each
 * entry's `link` with a pointer to that module's Linkage.  All cross-module
 * calls then go through those pointers.  There is no static import linkage
 * between modules at all -- which is why a reimplemented module can simply be
 * dropped into Modules/ and the retail exe will load and use it.
 */
#ifndef FS98_LINKAGE_H
#define FS98_LINKAGE_H

#include <stddef.h>
#include <stdint.h>
#include "fs98/abi.h"

/* Module ids, from the Linkage structs of the retail modules.
 * Id 1 is the host (FLTSIM98.EXE) itself; id 19 is imported by PANELS and is
 * not a file in Modules/ -- believed to be the gauge host.  Ids 5, 8, 11, 12,
 * 25, 28, 30, 31, 33, 36, 38 are unused by the retail module set. */
enum fs98_module_id {
    FS98_MOD_HOST      =  1,   /* FLTSIM98.EXE */
    FS98_MOD_GLOBAL    =  2,
    FS98_MOD_MAIN      =  3,
    FS98_MOD_CHAIN     =  4,
    FS98_MOD_DYNAMIC   =  6,
    FS98_MOD_DEMO      =  7,
    FS98_MOD_SLEW      =  9,
    FS98_MOD_WINDOW    = 10,
    FS98_MOD_HG2D      = 13,
    FS98_MOD_G3D       = 14,
    FS98_MOD_PANELS    = 15,
    FS98_MOD_CONTROLS  = 16,
    FS98_MOD_NUMERIC   = 17,
    FS98_MOD_FE        = 18,
    FS98_MOD_GAUGES    = 19,   /* unconfirmed; imported by PANELS */
    FS98_MOD_UTIL      = 20,
    FS98_MOD_ADVDRV    = 21,
    FS98_MOD_WINMSG    = 22,
    FS98_MOD_ACONTAIN  = 23,
    FS98_MOD_ATC       = 24,
    FS98_MOD_LANDME    = 26,
    FS98_MOD_WEATHERM  = 27,
    FS98_MOD_ANALYSIS  = 29,
    FS98_MOD_REALITY   = 32,
    FS98_MOD_BOOSTER   = 34,
    FS98_MOD_MULTIP    = 35,
    FS98_MOD_FACIL     = 37,
    FS98_MOD_SOUND     = 39
};

/* One ImportTable entry.  `link` is zero on disk and is written by the host
 * during module init.  The array is terminated by an all-zero entry. */
#pragma pack(push, 4)
struct fs98_import {
    uint32_t  id;
#if defined(_WIN32) && defined(__i386__)
    /* Retail-only, transitional (the Story 1.7 bridge, see abi.h): the same word as a native pointer, so the
     * code gcc generates for it stays byte-identical to the pointer field this replaced. */
    union {
        fs98_addr link;         /* struct fs98_linkage * */
        struct fs98_linkage *link_ptr;
    };
#else
    fs98_addr link;             /* struct fs98_linkage * */
#endif
};

/* The service table a module publishes.
 *
 * Slot 0 and slot 1 are present in every retail module and both point into
 * .text; in the modules examined so far they are a bare `ret` (no-op), so the
 * names below are inferred from position, not yet proven.  Slots from 2 on are
 * module-specific: the host and the importing modules call them by index, so
 * the index assignment is part of each module's contract and has to be
 * recovered per module.
 *
 * GLOBAL (id 2) is the special case: its Linkage sits at the base of a 4776
 * byte .data block and other modules address shared state as byte offsets from
 * the Linkage pointer rather than calling through it. */
struct fs98_linkage {
    uint32_t  id;
    fs98_code init;            /* slot 0 -- inferred; void (*)(void) */
    fs98_code deinit;          /* slot 1 -- inferred; void (*)(void) */
    fs98_code service[];       /* slot 2.. -- module specific, each a code handle */
};
#pragma pack(pop)

#if defined(_WIN32) && defined(__i386__)
/* Retail-only, transitional (see abi.h): an import's `link` as the Linkage pointer it holds. */
#  define FS98_LINK(import) ((import).link_ptr)
#endif

/* Measured on win32-retail before the conversion (Story 1.7). */
_Static_assert(sizeof(struct fs98_import) == 8 && _Alignof(struct fs98_import) == 4, "fs98_import");
_Static_assert(offsetof(struct fs98_import, id) == 0 && offsetof(struct fs98_import, link) == 4, "fs98_import");
_Static_assert(sizeof(struct fs98_linkage) == 12 && _Alignof(struct fs98_linkage) == 4, "fs98_linkage header");
_Static_assert(offsetof(struct fs98_linkage, id) == 0 && offsetof(struct fs98_linkage, init) == 4 &&
               offsetof(struct fs98_linkage, deinit) == 8, "fs98_linkage");
_Static_assert(offsetof(struct fs98_linkage, service) == 12, "fs98_linkage service tail");
_Static_assert(sizeof(((struct fs98_linkage *)0)->service[0]) == 4, "fs98_linkage service stride");

#endif /* FS98_LINKAGE_H */
