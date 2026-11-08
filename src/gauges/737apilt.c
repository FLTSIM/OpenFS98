/* 737APILT.GAU -- 737-400.AUTOPILOT: the autopilot's mode control panel */
#include "gauge.h"

#define GAUGE_IMPORTS 0x46c8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3d08u
#define GAUGE_MOUSE 0x3d10u
#define GAUGE_MOUSE_ARGUMENT 0x3cf4u


/* 0x1020: the IAS/Mach button's first half: the selector (+0x4708) 0, event 0x10163 with 0. Returns 0. */
static uint32_t __stdcall g737apilt_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x4708) = 0;
    GAUGE_PANELS_CALL(0x90, 2)(0x10163, 0);
    return 0;
}

/* 0x1050: the other half: the selector 1, event 0x1017b with 0. Returns 0. */
static uint32_t __stdcall g737apilt_1050(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x4708) = 1;
    GAUGE_PANELS_CALL(0x90, 2)(0x1017b, 0);
    return 0;
}

/* 0x1080: a speed knob: with the selector at 1 event 0x1017c, otherwise 0x10168, with 0. Returns 0. */
static uint32_t __stdcall g737apilt_1080(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0x90, 2)(GD32(0x4708) == 1 ? 0x1017c : 0x10168, 0);
    return 0;
}

/* 0x10c0: a speed knob: with the selector at 1 event 0x1017d, otherwise 0x10169, with 0. Returns 0. */
static uint32_t __stdcall g737apilt_10c0(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0x90, 2)(GD32(0x4708) == 1 ? 0x1017d : 0x10169, 0);
    return 0;
}

/* 0x1100: a heading or course as text: the value less the first constant through _ftol, brought
 * to 1..360 (0 is 360; above 360 less 360 as many times as (n - 1) / 360), into the element's
 * buffer (+0x3138) with the format at +0x46b0; returns it */
static long double __stdcall g737apilt_1100(double value)
{
    uint32_t heading = (uint32_t)gauge_ftol((long double)value - GR64(0x2000));
    if (heading == 0) heading += (360u - heading) / 360u * 360u;
    if (heading > 360u) heading -= (heading - 1u) / 360u * 360u;
    wsprintfA((char *)(uintptr_t)GD32(0x3138), (const char *)GAUGE_D(0x46b0), heading);
    return (long double)(uint64_t)heading;
}

/* 0x1190: a heading or course as text: the value less the first constant through _ftol, brought
 * to 1..360 (0 is 360; above 360 less 360 as many times as (n - 1) / 360), into the element's
 * buffer (+0x3270) with the format at +0x46b0; returns it */
static long double __stdcall g737apilt_1190(double value)
{
    uint32_t heading = (uint32_t)gauge_ftol((long double)value - GR64(0x2000));
    if (heading == 0) heading += (360u - heading) / 360u * 360u;
    if (heading > 360u) heading -= (heading - 1u) / 360u * 360u;
    wsprintfA((char *)(uintptr_t)GD32(0x3270), (const char *)GAUGE_D(0x46b0), heading);
    return (long double)(uint64_t)heading;
}

/* 0x1220: the altitude as text: the value less the first constant, through _ftol, into the
 * element's buffer (+0x33a8) with the format at +0x46b8; returns the value */
static long double __stdcall g737apilt_1220(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x33a8), (const char *)GAUGE_D(0x46b8), (uint32_t)gauge_ftol((long double)value - GR64(0x2000)));
    return value;
}

/* 0x1250: the speed as text into the element's buffer (+0x34e0): with the selector 0 the IAS
 * (+0x3470) less the first constant through _ftol with +0x46b0; otherwise the Mach (+0x348c): its
 * fraction (less its unsigned whole part through _ftol) times the second constant, parked as a
 * double in the argument's own slot (where the whole part went first, for its fild), less the
 * first, through _ftol, with +0x46c0; the slot less the third returned */
static long double __stdcall g737apilt_1250(double value)
{
    double parked;
    if (!GD32(0x4708)) {
        wsprintfA((char *)(uintptr_t)GD32(0x34e0), (const char *)GAUGE_D(0x46b0), (uint32_t)gauge_ftol((long double)GD64(0x3470) - GR64(0x2000)));
        return *(volatile double *)&value;     /* loaded here only, as retail loads it (a denormal's DE) */
    }
    {
        long double mach = GD64(0x348c);
        uint32_t whole = (uint32_t)gauge_ftol(mach);
        gauge_fst(&parked, (mach - (long double)(uint64_t)whole) * GR64(0x2008));
    }
    wsprintfA((char *)(uintptr_t)GD32(0x34e0), (const char *)GAUGE_D(0x46c0), (uint32_t)gauge_ftol((long double)parked - GR64(0x2000)));
    return (long double)parked - GR64(0x2010);
}

/* 0x12f0: the vertical speed as text: the value less the fifth constant when below the fourth
 * (or unordered), otherwise less the first, parked as a double, through _ftol into the element's
 * buffer (+0x3618) with the format at +0x46b8; returns it */
static long double __stdcall g737apilt_12f0(double value)
{
    uint16_t below = gauge_fcom(value, GR64(0x2018));
    value = (double)((long double)value - (below & GAUGE_C0 ? GR64(0x2020) : GR64(0x2000)));
    wsprintfA((char *)(uintptr_t)GD32(0x3618), (const char *)GAUGE_D(0x46b8), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1340: a knob's card: the variable offset */
static long double __stdcall g737apilt_1340(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2028);
}

/* 0x1350: a light: the variable at the sixth constant (equal, or unordered) sets the selector to
 * 0 and gives the fourth constant; otherwise the seventh */
static long double __stdcall g737apilt_1350(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2028), GAUGE_C3)) {
        GD32(0x4708) = 0;
        return GR64(0x2018);
    }
    return GR64(0x2030);
}

/* 0x1380: a light: the variable at the sixth constant (equal, or unordered) sets the selector to
 * 1 and gives the fourth constant; otherwise the seventh */
static long double __stdcall g737apilt_1380(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2028), GAUGE_C3)) {
        GD32(0x4708) = 1;
        return GR64(0x2018);
    }
    return GR64(0x2030);
}
GAUGE_INSTALL(g737apilt_install)   /* 0x13b0 */
GAUGE_INITIALIZE(g737apilt_initialize)   /* 0x1400 */
GAUGE_UPDATE(g737apilt_update)   /* 0x1410 */
GAUGE_GENERATE(g737apilt_generate)   /* 0x1420 */
GAUGE_DRAW(g737apilt_draw)   /* 0x1440 */
GAUGE_KILL(g737apilt_kill)   /* 0x1450 */
GAUGE_MODULE(g737apilt_module)   /* 0x1480 */

GAUGE_QUERY(g737apilt_1490)   /* 0x1490 (the _ftol import thunk at 0x14b0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737apilt_1020}, {0x1050, (void *)g737apilt_1050}, {0x1080, (void *)g737apilt_1080}, {0x10c0, (void *)g737apilt_10c0}, {0x1100, (void *)g737apilt_1100}, {0x1190, (void *)g737apilt_1190}, {0x1220, (void *)g737apilt_1220}, {0x1250, (void *)g737apilt_1250}, {0x12f0, (void *)g737apilt_12f0}, {0x1340, (void *)g737apilt_1340}, {0x1350, (void *)g737apilt_1350}, {0x1380, (void *)g737apilt_1380}, {0x13b0, (void *)g737apilt_install}, {0x1400, (void *)g737apilt_initialize}, {0x1410, (void *)g737apilt_update}, {0x1420, (void *)g737apilt_generate}, {0x1440, (void *)g737apilt_draw}, {0x1450, (void *)g737apilt_kill}, {0x1480, (void *)g737apilt_module}, {0x1490, (void *)g737apilt_1490})
