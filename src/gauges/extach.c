/* EXTACH.GAU -- EXTRA-300.TACHOMETER: the tachometer and its hour meter */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3560u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3418u
#define GAUGE_MOUSE 0x3420u
#define GAUGE_MOUSE_ARGUMENT 0x3404u


/* The hour meter's mode (+0x3590, 0..5) and its display flag (bit 30 of +0x30b0) */
#define EXTACH_MODE GD32(0x3590)
#define EXTACH_SHOW() (GD32(0x30b0) |= 0x40000000u)

/* 0x1020: the meter's first button: mode 2 to 1; mode 4 to 5 with the display flag cleared;
 * otherwise back to 0, shown. Returns 0. */
static uint32_t __stdcall extach_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    if (EXTACH_MODE == 2) { EXTACH_MODE = 1; return 0; }
    if (EXTACH_MODE == 4) { EXTACH_MODE = 5; GD32(0x30b0) &= 0xbfffffffu; return 0; }
    EXTACH_MODE = 0;
    EXTACH_SHOW();
    return 0;
}

/* 0x1080: the second button: mode 3 to 2, otherwise back to 0, shown. Returns 0. */
static uint32_t __stdcall extach_1080(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    if (EXTACH_MODE == 3) { EXTACH_MODE = 2; return 0; }
    EXTACH_MODE = 0;
    EXTACH_SHOW();
    return 0;
}

/* 0x10c0: the third button: mode 0 to 3, 1 to 4, otherwise back to 0, shown. Returns 0. */
static uint32_t __stdcall extach_10c0(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    if (EXTACH_MODE == 0) { EXTACH_MODE = 3; return 0; }
    if (EXTACH_MODE == 1) { EXTACH_MODE = 4; return 0; }
    EXTACH_MODE = 0;
    EXTACH_SHOW();
    return 0;
}

/* 0x1110: anywhere else: back to 0, shown. Returns 0. */
static uint32_t __stdcall extach_1110(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    EXTACH_MODE = 0;
    EXTACH_SHOW();
    return 0;
}

/* 0x1130: the rpm (+0x32ec times +0x32d0 and a scale, parked at +0x35a0; the low test on the
 * unrounded value) kept to [0, 9999] in place, printed through _ftol into the element's buffer
 * (+0x3340) with the format at +0x3554; returns it */
static long double __stdcall extach_1130(double value)
{
    long double rpm = ((long double)GD64(0x32ec) * GD64(0x32d0)) * GR64(0x2000);
    (void)value;
    gauge_fst(GAUGE_D(0x35a0), rpm);
    if (GAUGE_TEST(rpm, GR64(0x2008), GAUGE_C0)) GD64(0x35a0) = 0.0;
    else if (!GAUGE_TEST(GD64(0x35a0), GR64(0x2010), GAUGE_C0C3)) GD64(0x35a0) = 9999.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3340), (const char *)GAUGE_D(0x3554), (uint32_t)gauge_ftol(GD64(0x35a0)));
    return GD64(0x35a0);
}

/* 0x11c0: a constant */
static long double __stdcall extach_11c0(uint8_t *element)
{
    (void)element;
    return GR64(0x2008);
}

/* 0x11d0: a light: the rpm above the fourth constant (ordered) the second, otherwise the fifth */
static long double __stdcall extach_11d0(uint8_t *element)
{
    (void)element;
    return !GAUGE_TEST(GD64(0x35a0), GR64(0x2018), GAUGE_C0C3) ? GR64(0x2008) : GR64(0x2020);
}

/* 0x1200: a light: the rpm above the sixth constant (ordered) the second, otherwise the fifth */
static long double __stdcall extach_1200(uint8_t *element)
{
    (void)element;
    return !GAUGE_TEST(GD64(0x35a0), GR64(0x2028), GAUGE_C0C3) ? GR64(0x2008) : GR64(0x2020);
}
GAUGE_INSTALL(extach_install)   /* 0x1230 */
GAUGE_INITIALIZE(extach_initialize)   /* 0x1280 */
GAUGE_UPDATE(extach_update)   /* 0x1290 */
GAUGE_GENERATE(extach_generate)   /* 0x12a0 */
GAUGE_DRAW(extach_draw)   /* 0x12c0 */
GAUGE_KILL(extach_kill)   /* 0x12d0 */
GAUGE_MODULE(extach_module)   /* 0x1300 */

GAUGE_QUERY(extach_1310)   /* 0x1310 (the _ftol import thunk at 0x1330 follows it) */

GAUGE_ROUTINES({0x1020, (void *)extach_1020}, {0x1080, (void *)extach_1080}, {0x10c0, (void *)extach_10c0}, {0x1110, (void *)extach_1110}, {0x1130, (void *)extach_1130}, {0x11c0, (void *)extach_11c0}, {0x11d0, (void *)extach_11d0}, {0x1200, (void *)extach_1200}, {0x1230, (void *)extach_install}, {0x1280, (void *)extach_initialize}, {0x1290, (void *)extach_update}, {0x12a0, (void *)extach_generate}, {0x12c0, (void *)extach_draw}, {0x12d0, (void *)extach_kill}, {0x1300, (void *)extach_module}, {0x1310, (void *)extach_1310})
