/* 737MAIN.GAU -- 737-400.MAIN-SET: the main panel's engine readouts */
#include "gauge.h"

#define GAUGE_IMPORTS 0x4980u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x4730u
#define GAUGE_MOUSE 0x4738u
#define GAUGE_MOUSE_ARGUMENT 0x471cu


/* 0x1020: a needle: the variable above the first constant (ordered) gives that constant,
 * otherwise the variable */
static long double __stdcall g737main_1020(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C0C3)) return GR64(0x2000);
    return PF64(element, 0xc);
}

/* 0x1050: a needle: the variable scaled in place; the unrounded product, unless above the third
 * constant (ordered: that constant) */
static long double __stdcall g737main_1050(uint8_t *element)
{
    long double needle = (long double)PF64(element, 0xc) * GR64(0x2008);
    uint16_t high = gauge_fcom(needle, GR64(0x2010));
    gauge_fst(element + 0xc, needle);
    if (!(high & GAUGE_C0C3)) return GR64(0x2010);
    return needle;
}

/* 0x1080: +0x3248 times the fourth constant, through _ftol, into the element's buffer (+0x32b8)
 * with the format at +0x4974; returns the value */
static long double __stdcall g737main_1080(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x32b8), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol((long double)GD64(0x3248) * GR64(0x2018)));
    return value;
}

/* 0x10b0: +0x3380 times the fourth constant, through _ftol, into the element's buffer (+0x33f0)
 * with the format at +0x4974; returns the value */
static long double __stdcall g737main_10b0(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x33f0), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol((long double)GD64(0x3380) * GR64(0x2018)));
    return value;
}

/* 0x10e0: the value scaled (parked in the argument's slot), at most 999 (the test on the unrounded
 * product), through _ftol into the element's buffer (+0x3528) with the format at +0x4974;
 * returns it */
static long double __stdcall g737main_10e0(double value)
{
    long double scaled = (long double)value * GR64(0x2020);
    uint16_t high = gauge_fcom(scaled, GR64(0x2028));
    gauge_fst(&value, scaled);
    if (!(high & GAUGE_C0C3)) value = 999.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3528), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1130: the value scaled (parked in the argument's slot), at most 999 (the test on the unrounded
 * product), through _ftol into the element's buffer (+0x3660) with the format at +0x4974;
 * returns it */
static long double __stdcall g737main_1130(double value)
{
    long double scaled = (long double)value * GR64(0x2020);
    uint16_t high = gauge_fcom(scaled, GR64(0x2028));
    gauge_fst(&value, scaled);
    if (!(high & GAUGE_C0C3)) value = 999.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3660), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1180: the value times the fourth constant, through _ftol, into the element's buffer (+0x3798)
 * with the format at +0x4974; returns the value */
static long double __stdcall g737main_1180(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x3798), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol((long double)value * GR64(0x2018)));
    return value;
}

/* 0x11b0: the value times the fourth constant, through _ftol, into the element's buffer (+0x38d0)
 * with the format at +0x4974; returns the value */
static long double __stdcall g737main_11b0(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x38d0), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol((long double)value * GR64(0x2018)));
    return value;
}

/* 0x11e0: the value scaled (parked in the argument's slot), at most 999 (the test on the unrounded
 * product), through _ftol into the element's buffer (+0x3a08) with the format at +0x4974;
 * returns it */
static long double __stdcall g737main_11e0(double value)
{
    long double scaled = (long double)value * GR64(0x2008);
    uint16_t high = gauge_fcom(scaled, GR64(0x2028));
    gauge_fst(&value, scaled);
    if (!(high & GAUGE_C0C3)) value = 999.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3a08), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1230: the value scaled (parked in the argument's slot), at most 999 (the test on the unrounded
 * product), through _ftol into the element's buffer (+0x3b40) with the format at +0x4974;
 * returns it */
static long double __stdcall g737main_1230(double value)
{
    long double scaled = (long double)value * GR64(0x2008);
    uint16_t high = gauge_fcom(scaled, GR64(0x2028));
    gauge_fst(&value, scaled);
    if (!(high & GAUGE_C0C3)) value = 999.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3b40), (const char *)GAUGE_D(0x4974), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1280: an annunciator's text, written straight into the element's buffer (+0x3c78): at the
 * seventh constant (equal, or unordered) one character (+0x49b0), otherwise three (+0x497c..);
 * the value stays in st0 for the return */
static long double __stdcall g737main_1280(double value)
{
    uint8_t *text = (uint8_t *)(uintptr_t)GD32(0x3c78);
    if (GAUGE_TEST(value, GR64(0x2030), GAUGE_C3)) {
        text[0] = GD8(0x49b0);
    } else {
        memcpy(text, GAUGE_D(0x497c), 2);
        text[2] = GD8(0x497e);
    }
    return value;
}
GAUGE_INSTALL(g737main_install)   /* 0x12c0 */
GAUGE_INITIALIZE(g737main_initialize)   /* 0x1310 */
GAUGE_UPDATE(g737main_update)   /* 0x1320 */
GAUGE_GENERATE(g737main_generate)   /* 0x1330 */
GAUGE_DRAW(g737main_draw)   /* 0x1350 */
GAUGE_KILL(g737main_kill)   /* 0x1360 */
GAUGE_MODULE(g737main_module)   /* 0x1390 */

GAUGE_QUERY(g737main_13a0)   /* 0x13a0 (the _ftol import thunk at 0x13c0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737main_1020}, {0x1050, (void *)g737main_1050}, {0x1080, (void *)g737main_1080}, {0x10b0, (void *)g737main_10b0}, {0x10e0, (void *)g737main_10e0}, {0x1130, (void *)g737main_1130}, {0x1180, (void *)g737main_1180}, {0x11b0, (void *)g737main_11b0}, {0x11e0, (void *)g737main_11e0}, {0x1230, (void *)g737main_1230}, {0x1280, (void *)g737main_1280}, {0x12c0, (void *)g737main_install}, {0x1310, (void *)g737main_initialize}, {0x1320, (void *)g737main_update}, {0x1330, (void *)g737main_generate}, {0x1350, (void *)g737main_draw}, {0x1360, (void *)g737main_kill}, {0x1390, (void *)g737main_module}, {0x13a0, (void *)g737main_13a0})
