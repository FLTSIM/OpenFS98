/* LEARAUTO.GAU -- LEAR_45.AUTOPILOT: the autopilot's mode control panel */
#include "gauge.h"

#define GAUGE_IMPORTS 0x45b8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3c78u
#define GAUGE_MOUSE 0x3c80u
#define GAUGE_MOUSE_ARGUMENT 0x3c64u


/* 0x1020: a knob's card: the variable offset */
static long double __stdcall learauto_1020(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2000);
}

/* 0x1030: a light: the variable at the sixth constant (equal, or unordered) sets the selector to
 * 0 and gives the fourth constant; otherwise the seventh */
static long double __stdcall learauto_1030(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C3)) {
        GD32(0x45f8) = 0;
        return GR64(0x2008);
    }
    return GR64(0x2010);
}

/* 0x1060: a light: the variable at the sixth constant (equal, or unordered) sets the selector to
 * 1 and gives the fourth constant; otherwise the seventh */
static long double __stdcall learauto_1060(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C3)) {
        GD32(0x45f8) = 1;
        return GR64(0x2008);
    }
    return GR64(0x2010);
}

/* 0x1090: a heading or course as text: the value less the first constant through _ftol, brought
 * to 1..360 (0 is 360; above 360 less 360 as many times as (n - 1) / 360), into the element's
 * buffer (+0x3138) with the format at +0x459c; returns it */
static long double __stdcall learauto_1090(double value)
{
    uint32_t heading = (uint32_t)gauge_ftol((long double)value - GR64(0x2018));
    if (heading == 0) heading += (360u - heading) / 360u * 360u;
    if (heading > 360u) heading -= (heading - 1u) / 360u * 360u;
    wsprintfA((char *)(uintptr_t)GD32(0x3138), (const char *)GAUGE_D(0x459c), heading);
    return (long double)(uint64_t)heading;
}

/* 0x1120: a heading or course as text: the value less the first constant through _ftol, brought
 * to 1..360 (0 is 360; above 360 less 360 as many times as (n - 1) / 360), into the element's
 * buffer (+0x3270) with the format at +0x459c; returns it */
static long double __stdcall learauto_1120(double value)
{
    uint32_t heading = (uint32_t)gauge_ftol((long double)value - GR64(0x2018));
    if (heading == 0) heading += (360u - heading) / 360u * 360u;
    if (heading > 360u) heading -= (heading - 1u) / 360u * 360u;
    wsprintfA((char *)(uintptr_t)GD32(0x3270), (const char *)GAUGE_D(0x459c), heading);
    return (long double)(uint64_t)heading;
}

/* 0x11b0: the altitude as text: the value less the first constant, through _ftol, into the
 * element's buffer (+0x33a8) with the format at +0x45a4; returns the value */
static long double __stdcall learauto_11b0(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x33a8), (const char *)GAUGE_D(0x45a4), (uint32_t)gauge_ftol((long double)value - GR64(0x2018)));
    return value;
}

/* 0x11e0: the vertical speed as text: the value less the fifth constant when below the fourth
 * (or unordered), otherwise less the first, parked as a double, through _ftol into the element's
 * buffer (+0x34e0) with the format at +0x45a4; returns it */
static long double __stdcall learauto_11e0(double value)
{
    uint16_t below = gauge_fcom(value, GR64(0x2008));
    value = (double)((long double)value - (below & GAUGE_C0 ? GR64(0x2020) : GR64(0x2018)));
    wsprintfA((char *)(uintptr_t)GD32(0x34e0), (const char *)GAUGE_D(0x45a4), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1230: the speed as text into the element's buffer (+0x3618): with the selector 0 the IAS
 * (+0x35a8) less the first constant through _ftol with +0x459c; otherwise the Mach (+0x35c4): its
 * fraction (less its unsigned whole part through _ftol) times the second constant, parked as a
 * double in the argument's own slot (where the whole part went first, for its fild), less the
 * first, through _ftol, with +0x45ac; the slot less the third returned */
static long double __stdcall learauto_1230(double value)
{
    double parked;
    if (!GD32(0x45f8)) {
        wsprintfA((char *)(uintptr_t)GD32(0x3618), (const char *)GAUGE_D(0x459c), (uint32_t)gauge_ftol((long double)GD64(0x35a8) - GR64(0x2018)));
        return *(volatile double *)&value;     /* loaded here only, as retail loads it (a denormal's DE) */
    }
    {
        long double mach = GD64(0x35c4);
        uint32_t whole = (uint32_t)gauge_ftol(mach);
        gauge_fst(&parked, (mach - (long double)(uint64_t)whole) * GR64(0x2028));
    }
    wsprintfA((char *)(uintptr_t)GD32(0x3618), (const char *)GAUGE_D(0x45ac), (uint32_t)gauge_ftol((long double)parked - GR64(0x2018)));
    return (long double)parked - GR64(0x2030);
}

/* 0x12d0: the IAS/Mach button's first half: the selector (+0x45f8) 0, event 0x10163 with 0. Returns 0. */
static uint32_t __stdcall learauto_12d0(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x45f8) = 0;
    GAUGE_PANELS_CALL(0x90, 2)(0x10163, 0);
    return 0;
}

/* 0x1300: the other half: the selector 1, event 0x1017b with 0. Returns 0. */
static uint32_t __stdcall learauto_1300(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x45f8) = 1;
    GAUGE_PANELS_CALL(0x90, 2)(0x1017b, 0);
    return 0;
}

/* 0x1330: a speed knob: with the selector at 1 event 0x1017c, otherwise 0x10168, with 0. Returns 0. */
static uint32_t __stdcall learauto_1330(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0x90, 2)(GD32(0x45f8) == 1 ? 0x1017c : 0x10168, 0);
    return 0;
}

/* 0x1370: a speed knob: with the selector at 1 event 0x1017d, otherwise 0x10169, with 0. Returns 0. */
static uint32_t __stdcall learauto_1370(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0x90, 2)(GD32(0x45f8) == 1 ? 0x1017d : 0x10169, 0);
    return 0;
}
GAUGE_INSTALL(learauto_install)   /* 0x13b0 */
GAUGE_INITIALIZE(learauto_initialize)   /* 0x1400 */
GAUGE_UPDATE(learauto_update)   /* 0x1410 */
GAUGE_GENERATE(learauto_generate)   /* 0x1420 */
GAUGE_DRAW(learauto_draw)   /* 0x1440 */
GAUGE_KILL(learauto_kill)   /* 0x1450 */
GAUGE_MODULE(learauto_module)   /* 0x1480 */

GAUGE_QUERY(learauto_1490)   /* 0x1490 (the _ftol import thunk at 0x14b0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)learauto_1020}, {0x1030, (void *)learauto_1030}, {0x1060, (void *)learauto_1060}, {0x1090, (void *)learauto_1090}, {0x1120, (void *)learauto_1120}, {0x11b0, (void *)learauto_11b0}, {0x11e0, (void *)learauto_11e0}, {0x1230, (void *)learauto_1230}, {0x12d0, (void *)learauto_12d0}, {0x1300, (void *)learauto_1300}, {0x1330, (void *)learauto_1330}, {0x1370, (void *)learauto_1370}, {0x13b0, (void *)learauto_install}, {0x1400, (void *)learauto_initialize}, {0x1410, (void *)learauto_update}, {0x1420, (void *)learauto_generate}, {0x1440, (void *)learauto_draw}, {0x1450, (void *)learauto_kill}, {0x1480, (void *)learauto_module}, {0x1490, (void *)learauto_1490})
