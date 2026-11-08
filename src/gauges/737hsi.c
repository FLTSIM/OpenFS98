/* 737HSI.GAU -- 737-400.HSI: the HSI: card, pointers, flags and the DME */
#include "gauge.h"

#define GAUGE_IMPORTS 0x43b8u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x42f0u
#define GAUGE_MOUSE 0x42f8u
#define GAUGE_MOUSE_ARGUMENT 0x42dcu


/* 0x1020: a flag: bit 3 of the variable the first constant, else bit 7 the second, else the third */
static long double __stdcall g737hsi_1020(uint8_t *element)
{
    uint32_t bits = P32(element, 0xc);
    if (bits & 8) return GR64(0x2000);
    if (bits & 0x80) return GR64(0x2008);
    return GR64(0x2010);
}

/* 0x1050: the variable plus the heading (+0x4200) less a constant */
static long double __stdcall g737hsi_1050(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) + GD64(0x4200)) - GR64(0x2018);
}

/* 0x1070: the variable offset */
static long double __stdcall g737hsi_1070(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2020);
}

/* 0x1080: the variable plus the heading */
static long double __stdcall g737hsi_1080(uint8_t *element)
{
    return (long double)PF64(element, 0xc) + GD64(0x4200);
}

/* 0x1090: the variable plus the heading less a constant */
static long double __stdcall g737hsi_1090(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) + GD64(0x4200)) - GR64(0x2018);
}

/* 0x10b0: the variable as it is */
static long double __stdcall g737hsi_10b0(uint8_t *element)
{
    return PF64(element, 0xc);
}

/* 0x10c0: the variable plus the heading less a constant, negated */
static long double __stdcall g737hsi_10c0(uint8_t *element)
{
    return gauge_fchs(((long double)PF64(element, 0xc) + GD64(0x4200)) - GR64(0x2018));
}

/* 0x10e0: the course pointer by the source (+0x30f4): 1 the variable plus the heading less the fourth constant, 2 less the sixth (each with bit 30 of +0x3d38 cleared); otherwise hidden (the bit set) at the second */
static long double __stdcall g737hsi_10e0(uint8_t *element)
{
    uint32_t source = GD32(0x30f4);
    if (source == 1) {
        GD32(0x3d38) &= 0xbfffffffu;
        return ((long double)PF64(element, 0xc) + GD64(0x4200)) - GR64(0x2018);
    }
    if (source == 2) {
        GD32(0x3d38) &= 0xbfffffffu;
        return ((long double)PF64(element, 0xc) + GD64(0x4200)) - GR64(0x2028);
    }
    GD32(0x3d38) |= 0x40000000u;
    return GR64(0x2008);
}

/* 0x1150: the value through _ftol into the element's buffer (+0x31e0) with the format at +0x437c; returns the value */
static long double __stdcall g737hsi_1150(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x31e0), (const char *)GAUGE_D(0x437c), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1180: the value through _ftol into the element's buffer (+0x3450) with the format at +0x437c; returns the value */
static long double __stdcall g737hsi_1180(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x3450), (const char *)GAUGE_D(0x437c), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x11b0: a frequency (+0x32a8, four BCD digits) printed into the element's buffer (+0x3318) with
 * the format at +0x4384; returns the frequency */
static long double __stdcall g737hsi_11b0(double value)
{
    uint32_t frequency = GD32(0x32a8);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3318), (const char *)GAUGE_D(0x4384),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x1210: a constant less the variable */
static long double __stdcall g737hsi_1210(uint8_t *element)
{
    return (long double)GR64(0x2030) - PF64(element, 0xc);
}

/* 0x1220: the DME speed into the element's buffer (+0x38b8): at the first constant (equal, or unordered) four characters (+0x4390) written in; otherwise +0x3864 kept to [0, 999] in place and printed through _ftol with +0x437c. Returns the value. */
static long double __stdcall g737hsi_1220(double value)
{
    if (GAUGE_TEST(GD64(0x3864), GR64(0x2000), GAUGE_C3)) {
        memcpy((void *)(uintptr_t)GD32(0x38b8), GAUGE_D(0x4390), 4);
        return value;
    }
    if (!GAUGE_TEST(GD64(0x3864), GR64(0x2038), GAUGE_C0C3)) GD64(0x3864) = 999.0;
    else if (GAUGE_TEST(GD64(0x3864), GR64(0x2008), GAUGE_C0)) GD64(0x3864) = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x38b8), (const char *)GAUGE_D(0x437c), (uint32_t)gauge_ftol(GD64(0x3864)));
    return value;
}

/* 0x12c0: the DME distance into the element's buffer (+0x3b38): at the first constant (equal, or unordered) five characters (+0x4394) written in; otherwise +0x3ac8 kept to [0, 9999] in place, above the tenth constant (ordered) printed whole with +0x43a4, otherwise as whole and tenths (the fraction times the eleventh, each through _ftol) with +0x439c. Returns the value. */
static long double __stdcall g737hsi_12c0(double value)
{
    if (GAUGE_TEST(GD64(0x3ac8), GR64(0x2000), GAUGE_C3)) {
        uint8_t *text = (uint8_t *)(uintptr_t)GD32(0x3b38);
        memcpy(text, GAUGE_D(0x4394), 4);
        text[4] = GD8(0x4398);
        return value;
    }
    if (!GAUGE_TEST(GD64(0x3ac8), GR64(0x2040), GAUGE_C0C3)) GD64(0x3ac8) = 9999.0;
    else if (GAUGE_TEST(GD64(0x3ac8), GR64(0x2008), GAUGE_C0)) GD64(0x3ac8) = 0.0;
    if (!GAUGE_TEST(GD64(0x3ac8), GR64(0x2048), GAUGE_C0C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x3b38), (const char *)GAUGE_D(0x43a4), (uint32_t)gauge_ftol(GD64(0x3ac8)));
    } else {
        uint32_t whole = (uint32_t)gauge_ftol(GD64(0x3ac8));
        uint32_t part = (uint32_t)gauge_ftol(((long double)GD64(0x3ac8) - (long double)(int32_t)whole) * GR64(0x2050));
        wsprintfA((char *)(uintptr_t)GD32(0x3b38), (const char *)GAUGE_D(0x439c), whole, part);
    }
    return value;
}

/* 0x13d0: four characters (+0x43ac) written into the element's buffer (+0x3778); returns the value */
static long double __stdcall g737hsi_13d0(double value)
{
    memcpy((void *)(uintptr_t)GD32(0x3778), GAUGE_D(0x43ac), 4);
    return value;
}

/* 0x13f0: three characters (+0x43b0) written into the element's buffer (+0x39f8); returns the value */
static long double __stdcall g737hsi_13f0(double value)
{
    uint8_t *text = (uint8_t *)(uintptr_t)GD32(0x39f8);
    memcpy(text, GAUGE_D(0x43b0), 2);
    text[2] = GD8(0x43b2);
    return value;
}

/* 0x1410, header +0x10: install: the three variables (+0x30c8, +0x30e8, +0x3108) initialised
 * (PANELS +0x98), then the standard install */
static void __stdcall g737hsi_1410(uint32_t context)
{
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x30c8));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x30e8));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3108));
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(g737hsi_initialize)   /* 0x1490 */

/* 0x14a0, header +0x18: update: the three variables looked up (PANELS +0xa0), the elements; then
 * bit 30: with bit 3 of +0x3114 set on +0x3bf0 and off +0x3640, otherwise the other way; and on
 * +0x40d8 and +0x4188 while +0x30d4 is at the second constant (equal, or unordered), else off */
static void __stdcall g737hsi_14a0(void)
{
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x30c8));
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x30e8));
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3108));
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
    if (GD8(0x3114) & 8) {
        GD32(0x3bf0) |= 0x40000000u;
        GD32(0x3640) &= 0xbfffffffu;
    } else {
        GD32(0x3640) |= 0x40000000u;
        GD32(0x3bf0) &= 0xbfffffffu;
    }
    if (GAUGE_TEST(GD64(0x30d4), GR64(0x2008), GAUGE_C3)) {
        GD32(0x40d8) |= 0x40000000u;
        GD32(0x4188) |= 0x40000000u;
    } else {
        GD32(0x40d8) &= 0xbfffffffu;
        GD32(0x4188) &= 0xbfffffffu;
    }
}
GAUGE_GENERATE(g737hsi_generate)   /* 0x1580 */
GAUGE_DRAW(g737hsi_draw)   /* 0x15a0 */
GAUGE_KILL(g737hsi_kill)   /* 0x15b0 */
GAUGE_MODULE(g737hsi_module)   /* 0x15e0 */

GAUGE_QUERY(g737hsi_15f0)   /* 0x15f0 (the _ftol import thunk at 0x1610 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737hsi_1020}, {0x1050, (void *)g737hsi_1050}, {0x1070, (void *)g737hsi_1070}, {0x1080, (void *)g737hsi_1080}, {0x1090, (void *)g737hsi_1090}, {0x10b0, (void *)g737hsi_10b0}, {0x10c0, (void *)g737hsi_10c0}, {0x10e0, (void *)g737hsi_10e0}, {0x1150, (void *)g737hsi_1150}, {0x1180, (void *)g737hsi_1180}, {0x11b0, (void *)g737hsi_11b0}, {0x1210, (void *)g737hsi_1210}, {0x1220, (void *)g737hsi_1220}, {0x12c0, (void *)g737hsi_12c0}, {0x13d0, (void *)g737hsi_13d0}, {0x13f0, (void *)g737hsi_13f0}, {0x1410, (void *)g737hsi_1410}, {0x1490, (void *)g737hsi_initialize}, {0x14a0, (void *)g737hsi_14a0}, {0x1580, (void *)g737hsi_generate}, {0x15a0, (void *)g737hsi_draw}, {0x15b0, (void *)g737hsi_kill}, {0x15e0, (void *)g737hsi_module}, {0x15f0, (void *)g737hsi_15f0})
