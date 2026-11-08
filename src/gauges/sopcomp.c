/* SOPCOMP.GAU -- SOPWITH.COMPASS: the compass card as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32a8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3210u
#define GAUGE_MOUSE 0x3218u
#define GAUGE_MOUSE_ARGUMENT 0x31fcu


/* 0x1020: the compass: the heading brought into range by repeated subtraction (above the first
 * constant, ordered, less it until it is not; below the second, or unordered, less the third
 * until it is not; each time parked as a double), printed through _ftol into the element's
 * buffer (+0x3138) with the format at +0x329c; returns the parked heading.
 * RETAIL BUG (kept): GAUGE-COMPASS-WRAP-HANG (docs/retail-bugs.md): a NaN, an infinity or a
 * heading too large for the subtraction to change never leaves the loop. */
static long double __stdcall sopcomp_1020(double value)
{
    long double heading = value;
    if (!GAUGE_TEST(heading, GR64(0x2000), GAUGE_C0C3)) {
        do heading = heading - GR64(0x2000); while (!GAUGE_TEST(heading, GR64(0x2000), GAUGE_C0C3));
        gauge_fst(&value, heading);     /* the fst, kept though the next may overwrite it */
    }
    if (GAUGE_TEST(heading, GR64(0x2008), GAUGE_C0)) {
        do heading = heading - GR64(0x2010); while (GAUGE_TEST(heading, GR64(0x2008), GAUGE_C0));
        gauge_fst(&value, heading);     /* the fst, kept though the next may overwrite it */
    }
    wsprintfA((char *)(uintptr_t)GD32(0x3138), (const char *)GAUGE_D(0x329c), (uint32_t)gauge_ftol(heading));
    return value;
}
GAUGE_INSTALL(sopcomp_install)   /* 0x1090 */
GAUGE_INITIALIZE(sopcomp_initialize)   /* 0x10e0 */

/* 0x10f0, header +0x18: update: PANELS +0x48 with the element at +0x3080, not the list */
static void __stdcall sopcomp_10f0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3080));
}
GAUGE_GENERATE(sopcomp_generate)   /* 0x1100 */
GAUGE_DRAW(sopcomp_draw)   /* 0x1120 */
GAUGE_KILL(sopcomp_kill)   /* 0x1130 */
GAUGE_MODULE(sopcomp_module)   /* 0x1160 */

GAUGE_QUERY(sopcomp_1170)   /* 0x1170 (the _ftol import thunk at 0x1190 follows it) */

GAUGE_ROUTINES({0x1020, (void *)sopcomp_1020}, {0x1090, (void *)sopcomp_install}, {0x10e0, (void *)sopcomp_initialize}, {0x10f0, (void *)sopcomp_10f0}, {0x1100, (void *)sopcomp_generate}, {0x1120, (void *)sopcomp_draw}, {0x1130, (void *)sopcomp_kill}, {0x1160, (void *)sopcomp_module}, {0x1170, (void *)sopcomp_1170})
