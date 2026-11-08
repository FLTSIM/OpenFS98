/* LREICAS.GAU -- LEAR_45.EICAS: the EICAS: its readouts and messages */
#include "gauge.h"

#define GAUGE_IMPORTS 0x5ef8u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x5af8u
#define GAUGE_MOUSE 0x5b00u
#define GAUGE_MOUSE_ARGUMENT 0x5ae4u


/* 0x1020: a fixed text (+0x5e9c) copied into the element's buffer (+0x3258, lstrcpyA); returns the value */
static long double __stdcall lreicas_1020(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x3258), (const char *)GAUGE_D(0x5e9c));
    return value;
}

/* 0x1040: the value times +0x2000 through _ftol, kept to [0, 999], into the element's buffer (+0x3478) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1040(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2000));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x3478), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x1080: the value times +0x2000 through _ftol, kept to [0, 999], into the element's buffer (+0x36f0) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1080(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2000));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x36f0), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x10c0: a fixed text (+0x5ea8) copied into the element's buffer (+0x38e0, lstrcpyA); returns the value */
static long double __stdcall lreicas_10c0(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x38e0), (const char *)GAUGE_D(0x5ea8));
    return value;
}

/* 0x10e0: the value scaled (parked in the argument's slot), kept to [0, 999] (the high test on the
 * unrounded product), through _ftol into the element's buffer (+0x3b00) with the format at +0x5ea0; returns it */
static long double __stdcall lreicas_10e0(double value)
{
    long double scaled = (long double)value * GR64(0x2008);
    uint16_t above = gauge_fcom(scaled, GR64(0x2010));
    gauge_fst(&value, scaled);
    if (!(above & GAUGE_C0C3)) value = 999.0;
    if (GAUGE_TEST(value, GR64(0x2018), GAUGE_C0)) value = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3b00), (const char *)GAUGE_D(0x5ea0), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1160: the value scaled (parked in the argument's slot), kept to [0, 999] (the high test on the
 * unrounded product), through _ftol into the element's buffer (+0x3d78) with the format at +0x5ea0; returns it */
static long double __stdcall lreicas_1160(double value)
{
    long double scaled = (long double)value * GR64(0x2008);
    uint16_t above = gauge_fcom(scaled, GR64(0x2010));
    gauge_fst(&value, scaled);
    if (!(above & GAUGE_C0C3)) value = 999.0;
    if (GAUGE_TEST(value, GR64(0x2018), GAUGE_C0)) value = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3d78), (const char *)GAUGE_D(0x5ea0), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x11e0: a needle: the variable scaled, clamped */
static long double __stdcall lreicas_11e0(uint8_t *element)
{
    return gauge_clamp((long double)PF64(element, 0xc) * GR64(0x2008), GR64(0x2010), GR64(0x2018));
}

/* 0x1220: a fixed text (+0x5eac) copied into the element's buffer (+0x3f68, lstrcpyA); returns the value */
static long double __stdcall lreicas_1220(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x3f68), (const char *)GAUGE_D(0x5eac));
    return value;
}

/* 0x1240: the value times +0x2000 through _ftol, kept to [0, 999], into the element's buffer (+0x4188) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1240(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2000));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x4188), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x1280: the value times +0x2000 through _ftol, kept to [0, 999], into the element's buffer (+0x4400) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1280(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2000));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x4400), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x12c0: a fixed text (+0x5eb0) copied into the element's buffer (+0x45e0, lstrcpyA); returns the value */
static long double __stdcall lreicas_12c0(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x45e0), (const char *)GAUGE_D(0x5eb0));
    return value;
}

/* 0x12e0: a fixed text (+0x5eb8) copied into the element's buffer (+0x4718, lstrcpyA); returns the value */
static long double __stdcall lreicas_12e0(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x4718), (const char *)GAUGE_D(0x5eb8));
    return value;
}

/* 0x1300: a fixed text (+0x5ec0) copied into the element's buffer (+0x4850, lstrcpyA); returns the value */
static long double __stdcall lreicas_1300(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x4850), (const char *)GAUGE_D(0x5ec0));
    return value;
}

/* 0x1320: the value times +0x2020 through _ftol, kept to [0, 999], into the element's buffer (+0x4988) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1320(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2020));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x4988), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x1360: the value times +0x2028 through _ftol, kept to [0, 999], into the element's buffer (+0x4ac0) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1360(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2028));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x4ac0), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x13a0: the value kept to [0, 9999] in the argument's slot, through _ftol into the element's buffer
 * (+0x4bf8) with the format at +0x5ec8; returns it */
static long double __stdcall lreicas_13a0(double value)
{
    if (!GAUGE_TEST(value, GR64(0x2030), GAUGE_C0C3)) value = 9999.0;
    if (GAUGE_TEST(value, GR64(0x2018), GAUGE_C0)) value = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x4bf8), (const char *)GAUGE_D(0x5ec8), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1410: the value times +0x2020 through _ftol, kept to [0, 999], into the element's buffer (+0x4d30) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1410(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2020));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x4d30), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x1450: the value times +0x2028 through _ftol, kept to [0, 999], into the element's buffer (+0x4e68) with
 * the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_1450(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2028));
    if (shown > 999) shown = 999;
    if (shown < 0) shown = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x4e68), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x1490: the value kept to [0, 9999] in the argument's slot, through _ftol into the element's buffer
 * (+0x4fa0) with the format at +0x5ec8; returns it */
static long double __stdcall lreicas_1490(double value)
{
    if (!GAUGE_TEST(value, GR64(0x2030), GAUGE_C0C3)) value = 9999.0;
    if (GAUGE_TEST(value, GR64(0x2018), GAUGE_C0)) value = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x4fa0), (const char *)GAUGE_D(0x5ec8), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1500: a fixed text (+0x5ed0) copied into the element's buffer (+0x5128, lstrcpyA); returns the value */
static long double __stdcall lreicas_1500(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x5128), (const char *)GAUGE_D(0x5ed0));
    return value;
}

/* 0x1520: a fixed text (+0x5ed8) copied into the element's buffer (+0x5260, lstrcpyA); returns the value */
static long double __stdcall lreicas_1520(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x5260), (const char *)GAUGE_D(0x5ed8));
    return value;
}

/* 0x1540: a fixed text (+0x5ee0) copied into the element's buffer (+0x5398, lstrcpyA); returns the value */
static long double __stdcall lreicas_1540(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x5398), (const char *)GAUGE_D(0x5ee0));
    return value;
}

/* 0x1560: a fixed text (+0x5ee8) copied into the element's buffer (+0x54d0, lstrcpyA); returns the value */
static long double __stdcall lreicas_1560(double value)
{
    lstrcpyA((char *)(uintptr_t)GD32(0x54d0), (const char *)GAUGE_D(0x5ee8));
    return value;
}

/* 0x1580: a fuel total: tank +0x55b4 times +0x310c and tank +0x5598 times +0x30ec, each times +0x2038 and +0x312c
 * (the second's partial product parked as a double on the way), summed, through _ftol,
 * kept to [0, 9999], into the element's buffer (+0x5608) with the format at +0x5ec8; returns the value */
static long double __stdcall lreicas_1580(double value)
{
    double parked;
    long double second = ((long double)GD64(0x5598) * GD64(0x30ec)) * GR64(0x2038);
    long double first = (((long double)GD64(0x55b4) * GD64(0x310c)) * GR64(0x2038)) * GD64(0x312c);
    int32_t total;
    gauge_fst(&parked, second);
    second = second * GD64(0x312c);
    total = (int32_t)(uint32_t)gauge_ftol(first + second);
    if (total > 9999) total = 9999;
    if (total < 0) total = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x5608), (const char *)GAUGE_D(0x5ec8), total);
    return value;
}

/* 0x1600: a fuel total: tank +0x56ec times +0x310c and tank +0x56d0 times +0x30ec, each times +0x2038 and +0x312c
 * (the second's partial product parked as a double on the way), summed (that one first), through _ftol,
 * kept to [0, 9999], into the element's buffer (+0x5740) with the format at +0x5ec8; returns the value */
static long double __stdcall lreicas_1600(double value)
{
    double parked;
    long double second = ((long double)GD64(0x56d0) * GD64(0x30ec)) * GR64(0x2038);
    long double first = (((long double)GD64(0x56ec) * GD64(0x310c)) * GR64(0x2038)) * GD64(0x312c);
    int32_t total;
    gauge_fst(&parked, second);
    second = second * GD64(0x312c);
    total = (int32_t)(uint32_t)gauge_ftol(second + first);
    if (total > 9999) total = 9999;
    if (total < 0) total = 0;
    wsprintfA((char *)(uintptr_t)GD32(0x5740), (const char *)GAUGE_D(0x5ec8), total);
    return value;
}

/* 0x1680: the value times +0x2040 through _ftol, at most 999 (and only then not below -99), into the
 * element's buffer (+0x5878): negative as its magnitude with +0x5ef0, otherwise with +0x5ea0; returns the value */
static long double __stdcall lreicas_1680(double value)
{
    int32_t shown = (int32_t)(uint32_t)gauge_ftol((long double)value * GR64(0x2040));
    if (shown > 999) shown = 999;
    else if (shown < -99) shown = -99;
    if (shown < 0) wsprintfA((char *)(uintptr_t)GD32(0x5878), (const char *)GAUGE_D(0x5ef0), (int32_t)(0u - (uint32_t)shown));
    else wsprintfA((char *)(uintptr_t)GD32(0x5878), (const char *)GAUGE_D(0x5ea0), shown);
    return value;
}

/* 0x16f0: a step: below +0x2048 (or unordered) 0, below +0x2050 8, below +0x2058 20, otherwise 40, into the
 * element's buffer (+0x59b0) with the format at +0x5ea0; returns the value */
static long double __stdcall lreicas_16f0(double value)
{
    uint32_t step;
    if (GAUGE_TEST(value, GR64(0x2048), GAUGE_C0)) step = 0;
    else if (GAUGE_TEST(value, GR64(0x2050), GAUGE_C0)) step = 8;
    else step = GAUGE_TEST(value, GR64(0x2058), GAUGE_C0) ? 0x14 : 0x28;
    wsprintfA((char *)(uintptr_t)GD32(0x59b0), (const char *)GAUGE_D(0x5ea0), step);
    return value;
}

/* 0x1760, header +0x10: install: the five variables initialised (PANELS +0x98) and looked up
 * (+0xa0), then the standard install */
static void __stdcall lreicas_1760(uint32_t context)
{
    static const uint16_t variables[5] = {0x30a0, 0x30c0, 0x30e0, 0x3100, 0x3120};
    unsigned at;
    for (at = 0; at < 5; ++at) GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(variables[at]));
    for (at = 0; at < 5; ++at) GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(variables[at]));
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(lreicas_initialize)   /* 0x1860 */
GAUGE_UPDATE(lreicas_update)   /* 0x1870 */
GAUGE_GENERATE(lreicas_generate)   /* 0x1880 */
GAUGE_DRAW(lreicas_draw)   /* 0x18a0 */
GAUGE_KILL(lreicas_kill)   /* 0x18b0 */
GAUGE_MODULE(lreicas_module)   /* 0x18e0 */

GAUGE_QUERY(lreicas_18f0)   /* 0x18f0 (the _ftol import thunk at 0x1910 follows it) */

GAUGE_ROUTINES({0x1020, (void *)lreicas_1020}, {0x1040, (void *)lreicas_1040}, {0x1080, (void *)lreicas_1080}, {0x10c0, (void *)lreicas_10c0}, {0x10e0, (void *)lreicas_10e0}, {0x1160, (void *)lreicas_1160}, {0x11e0, (void *)lreicas_11e0}, {0x1220, (void *)lreicas_1220}, {0x1240, (void *)lreicas_1240}, {0x1280, (void *)lreicas_1280}, {0x12c0, (void *)lreicas_12c0}, {0x12e0, (void *)lreicas_12e0}, {0x1300, (void *)lreicas_1300}, {0x1320, (void *)lreicas_1320}, {0x1360, (void *)lreicas_1360}, {0x13a0, (void *)lreicas_13a0}, {0x1410, (void *)lreicas_1410}, {0x1450, (void *)lreicas_1450}, {0x1490, (void *)lreicas_1490}, {0x1500, (void *)lreicas_1500}, {0x1520, (void *)lreicas_1520}, {0x1540, (void *)lreicas_1540}, {0x1560, (void *)lreicas_1560}, {0x1580, (void *)lreicas_1580}, {0x1600, (void *)lreicas_1600}, {0x1680, (void *)lreicas_1680}, {0x16f0, (void *)lreicas_16f0}, {0x1760, (void *)lreicas_1760}, {0x1860, (void *)lreicas_initialize}, {0x1870, (void *)lreicas_update}, {0x1880, (void *)lreicas_generate}, {0x18a0, (void *)lreicas_draw}, {0x18b0, (void *)lreicas_kill}, {0x18e0, (void *)lreicas_module}, {0x18f0, (void *)lreicas_18f0})
