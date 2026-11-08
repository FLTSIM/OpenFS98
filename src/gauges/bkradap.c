/* BKRADAP.GAU -- BENDIX_KING_RADIO.AP: the autopilot head */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3e40u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3910u
#define GAUGE_MOUSE 0x3918u
#define GAUGE_MOUSE_ARGUMENT 0x38fcu


/* 0x1020: the AP button: engaged (+0x30c8 not at the first constant) disengages it (events
 * 0x1002c, 0x100bf, 0x10107); otherwise with the flight director (+0x3370) at the first constant
 * (equal, or unordered) events 0x10118 and 0x10162, else 0x10107. Each with 0. Returns 0. */
static uint32_t __stdcall bkradap_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    if (!GAUGE_TEST(GD64(0x30c8), GR64(0x2000), GAUGE_C3)) {
    GAUGE_PANELS_CALL(0x90, 2)(0x1002c, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x100bf, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x10107, 0);
        return 0;
    }
    if (GAUGE_TEST(GD64(0x3370), GR64(0x2000), GAUGE_C3)) {
    GAUGE_PANELS_CALL(0x90, 2)(0x10118, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x10162, 0);
        return 0;
    }
    GAUGE_PANELS_CALL(0x90, 2)(0x10107, 0);
    return 0;
}

/* 0x10d0: the disconnect: events 0x1002c, 0x100bf and 0x10162, each with 0. Returns 0. */
static uint32_t __stdcall bkradap_10d0(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0x90, 2)(0x1002c, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x100bf, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x10162, 0);
    return 0;
}

/* 0x1110: the FD button: engaged (+0x30c8 not at the first constant) events 0x1002c and 0x10106;
 * otherwise 0x10106, then with +0x3150 at the first constant (equal, or unordered) 0x100bf. Each
 * with 0. Returns 0. */
static uint32_t __stdcall bkradap_1110(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    if (!GAUGE_TEST(GD64(0x30c8), GR64(0x2000), GAUGE_C3)) {
    GAUGE_PANELS_CALL(0x90, 2)(0x1002c, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x10106, 0);
        return 0;
    }
    if (!GAUGE_TEST(GD64(0x3150), GR64(0x2000), GAUGE_C3)) {
    GAUGE_PANELS_CALL(0x90, 2)(0x10106, 0);
        return 0;
    }
    GAUGE_PANELS_CALL(0x90, 2)(0x10106, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x100bf, 0);
    return 0;
}

/* 0x11a0: a knob's card: the variable offset */
static long double __stdcall bkradap_11a0(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2008);
}

/* 0x11b0: an annunciator's text into the element's buffer (+0x3468): the value at the second
 * constant (equal, or unordered) the text at +0x3e14, otherwise +0x3e70; returns the value */
static long double __stdcall bkradap_11b0(double value)
{
    if (GAUGE_TEST(value, GR64(0x2008), GAUGE_C3))
        wsprintfA((char *)(uintptr_t)GD32(0x3468), (const char *)GAUGE_D(0x3e14));
    else
        wsprintfA((char *)(uintptr_t)GD32(0x3468), (const char *)GAUGE_D(0x3e70));
    return value;
}

/* 0x1200: the mode's text into the element's buffer (+0x35a0): the first of +0x3530, +0x354c and
 * +0x3568 at the second constant (equal, or unordered) picks +0x3e20, +0x3e1c or +0x3e18; none,
 * +0x3e70. Returns the value. */
static long double __stdcall bkradap_1200(double value)
{
    const char *text;
    if (GAUGE_TEST(GD64(0x3530), GR64(0x2008), GAUGE_C3)) text = (const char *)GAUGE_D(0x3e20);
    else if (GAUGE_TEST(GD64(0x354c), GR64(0x2008), GAUGE_C3)) text = (const char *)GAUGE_D(0x3e1c);
    else if (GAUGE_TEST(GD64(0x3568), GR64(0x2008), GAUGE_C3)) text = (const char *)GAUGE_D(0x3e18);
    else text = (const char *)GAUGE_D(0x3e70);
    wsprintfA((char *)(uintptr_t)GD32(0x35a0), text);
    return value;
}

/* 0x12a0: the value less the third constant, through _ftol, into the element's buffer (+0x36d8)
 * with the format at +0x3e24; returns the value */
static long double __stdcall bkradap_12a0(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x36d8), (const char *)GAUGE_D(0x3e24), (uint32_t)gauge_ftol((long double)value - GR64(0x2010)));
    return value;
}

/* 0x12d0: the value less the fourth constant when below the first (or unordered), otherwise less
 * the third, parked as a double, through _ftol into the element's buffer (+0x3810) with the
 * format at +0x3e30; returns it */
static long double __stdcall bkradap_12d0(double value)
{
    uint16_t below = gauge_fcom(value, GR64(0x2000));
    value = (double)((long double)value - (below & GAUGE_C0 ? GR64(0x2018) : GR64(0x2010)));
    wsprintfA((char *)(uintptr_t)GD32(0x3810), (const char *)GAUGE_D(0x3e30), (uint32_t)gauge_ftol(value));
    return value;
}
GAUGE_INSTALL(bkradap_install)   /* 0x1320 */
GAUGE_INITIALIZE(bkradap_initialize)   /* 0x1370 */
GAUGE_UPDATE(bkradap_update)   /* 0x1380 */
GAUGE_GENERATE(bkradap_generate)   /* 0x1390 */
GAUGE_DRAW(bkradap_draw)   /* 0x13b0 */
GAUGE_KILL(bkradap_kill)   /* 0x13c0 */
GAUGE_MODULE(bkradap_module)   /* 0x13f0 */

GAUGE_QUERY(bkradap_1400)   /* 0x1400 (the _ftol import thunk at 0x1420 follows it) */

GAUGE_ROUTINES({0x1020, (void *)bkradap_1020}, {0x10d0, (void *)bkradap_10d0}, {0x1110, (void *)bkradap_1110}, {0x11a0, (void *)bkradap_11a0}, {0x11b0, (void *)bkradap_11b0}, {0x1200, (void *)bkradap_1200}, {0x12a0, (void *)bkradap_12a0}, {0x12d0, (void *)bkradap_12d0}, {0x1320, (void *)bkradap_install}, {0x1370, (void *)bkradap_initialize}, {0x1380, (void *)bkradap_update}, {0x1390, (void *)bkradap_generate}, {0x13b0, (void *)bkradap_draw}, {0x13c0, (void *)bkradap_kill}, {0x13f0, (void *)bkradap_module}, {0x1400, (void *)bkradap_1400})
