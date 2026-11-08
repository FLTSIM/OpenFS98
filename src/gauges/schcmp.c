/* SCHCMP.GAU -- SCHWEIZER.COMPASS: the compass card as text and the comm switch's light */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34c8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3350u
#define GAUGE_MOUSE 0x3358u
#define GAUGE_MOUSE_ARGUMENT 0x333cu


/* 0x1020: the compass: the heading brought into range by repeated subtraction (above the first
 * constant, ordered, less it until it is not; below the second, or unordered, less the third
 * until it is not; each time parked as a double), printed through _ftol into the element's
 * buffer (+0x3138) with the format at 0x34b8; returns the parked heading.
 * RETAIL BUG (kept): GAUGE-COMPASS-WRAP-HANG (docs/retail-bugs.md): a NaN, an infinity or a
 * heading too large for the subtraction to change never leaves the loop. */
static long double __stdcall schcmp_1020(double value)
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
    wsprintfA((char *)(uintptr_t)GD32(0x3138), (const char *)GAUGE_D(0x34b8), (uint32_t)gauge_ftol(heading));
    return value;
}

/* 0x1090: the switch light: with the switch (+0x3200) at the second constant (equal, or
 * unordered) colour 0xffffff and the text at +0x34c0, otherwise 0xff and +0x34c4 (+0x3288
 * the colour), into the element's buffer (+0x3270); returns the value */
static long double __stdcall schcmp_1090(double value)
{
    if (GAUGE_TEST(GD64(0x3200), GR64(0x2008), GAUGE_C3)) {
        GD32(0x3288) = 0xffffff;
        wsprintfA((char *)(uintptr_t)GD32(0x3270), (const char *)GAUGE_D(0x34c0));
    } else {
        GD32(0x3288) = 0xff;
        wsprintfA((char *)(uintptr_t)GD32(0x3270), (const char *)GAUGE_D(0x34c4));
    }
    return value;
}
GAUGE_INSTALL(schcmp_install)   /* 0x10f0 */
GAUGE_INITIALIZE(schcmp_initialize)   /* 0x1140 */

/* 0x1150, header +0x18: update: PANELS +0x48 with +0x3080, +0x31b8, not the list */
static void __stdcall schcmp_1150(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3080));
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x31b8));
}
GAUGE_GENERATE(schcmp_generate)   /* 0x1170 */
GAUGE_DRAW(schcmp_draw)   /* 0x1190 */
GAUGE_KILL(schcmp_kill)   /* 0x11a0 */
GAUGE_MODULE(schcmp_module)   /* 0x11d0 */

GAUGE_QUERY(schcmp_11e0)   /* 0x11e0 (the _ftol import thunk at 0x1200 follows it) */

GAUGE_ROUTINES({0x1020, (void *)schcmp_1020}, {0x1090, (void *)schcmp_1090}, {0x10f0, (void *)schcmp_install}, {0x1140, (void *)schcmp_initialize}, {0x1150, (void *)schcmp_1150}, {0x1170, (void *)schcmp_generate}, {0x1190, (void *)schcmp_draw}, {0x11a0, (void *)schcmp_kill}, {0x11d0, (void *)schcmp_module}, {0x11e0, (void *)schcmp_11e0})
