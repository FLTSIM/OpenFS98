/* LEARPFD.GAU -- LEAR_45.PFD: the PFD: its tapes, pointers, readouts and annunciators */
#include "gauge.h"

#define GAUGE_IMPORTS 0x5ae8u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x53e0u
#define GAUGE_MOUSE 0x53e8u
#define GAUGE_MOUSE_ARGUMENT 0x53ccu


/* 0x1020: the vertical speed's needle: the value through _ftol over 256 (truncating), times 60, times the first constant through _ftol, kept to [-4000, 4000] */
static long double __stdcall learpfd_1020(double value)
{
    int32_t rate = (int32_t)(uint32_t)gauge_ftol(value);
    int32_t needle = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)((uint32_t)(rate / 256) * 60u) * GR64(0x2000));
    if (needle > 4000) needle = 4000;
    if (needle < -4000) needle = -4000;
    return (long double)needle;
}

/* 0x1080: a constant */
static long double __stdcall learpfd_1080(uint8_t *element)
{
    (void)element;
    return GR64(0x2008);
}

/* 0x1090: the third constant less the variable, which is held at 1000 in place when above it (ordered) */
static long double __stdcall learpfd_1090(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2010), GAUGE_C0C3)) PF64(element, 0xc) = 1000.0;
    return (long double)GR64(0x2010) - PF64(element, 0xc);
}

/* 0x10c0: +0x3498 kept to [0, 940] in place, printed through _ftol into the element's buffer (+0x3508) with +0x5a9c; returns it */
static long double __stdcall learpfd_10c0(double value)
{
    (void)value;
    if (!GAUGE_TEST(GD64(0x3498), GR64(0x2018), GAUGE_C0C3)) GD64(0x3498) = 940.0;
    if (GAUGE_TEST(GD64(0x3498), GR64(0x2020), GAUGE_C0)) GD64(0x3498) = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3508), (const char *)GAUGE_D(0x5a9c), (uint32_t)gauge_ftol(GD64(0x3498)));
    return GD64(0x3498);
}

/* 0x1140: +0x3728 as its whole part and -(100 times it) less the scaled value (each through _ftol, in 32-bit arithmetic) into the element's buffer (+0x3798) with +0x5aa4; returns +0x3728 */
static long double __stdcall learpfd_1140(double value)
{
    uint32_t whole = (uint32_t)gauge_ftol(GD64(0x3728));
    uint32_t scaled = (uint32_t)gauge_ftol((long double)GD64(0x3728) * GR64(0x2028));
    uint32_t rest = (((((0u - whole) << 2) - whole) * 5u) << 2) - scaled;
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3798), (const char *)GAUGE_D(0x5aa4), whole, rest);
    return GD64(0x3728);
}

/* 0x11a0: a constant */
static long double __stdcall learpfd_11a0(uint8_t *element)
{
    (void)element;
    return GR64(0x2008);
}

/* 0x11b0: a drum: 1000 less the variable (through _ftol) modulo 1000, unsigned, scaled */
static long double __stdcall learpfd_11b0(uint8_t *element)
{
    uint32_t height = (uint32_t)gauge_ftol(PF64(element, 0xc));
    return (long double)(uint64_t)(1000u - height % 1000u) * GR64(0x2030);
}

/* 0x11f0: the value, at most 99999 (in the argument's slot), through _ftol into the element's buffer (+0x38d8) with +0x5ab4; returns it */
static long double __stdcall learpfd_11f0(double value)
{
    if (!GAUGE_TEST(value, GR64(0x2038), GAUGE_C0C3)) value = 99999.0;
    wsprintfA((char *)(uintptr_t)GD32(0x38d8), (const char *)GAUGE_D(0x5ab4), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1240: the variable offset */
static long double __stdcall learpfd_1240(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2040);
}

/* 0x1250: the limit: the variable above the tenth constant (ordered) gives it, below the eleventh (or unordered) that, otherwise the variable */
static long double __stdcall learpfd_1250(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2048), GAUGE_C0C3)) return GR64(0x2048);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2050), GAUGE_C0)) return GR64(0x2050);
    return PF64(element, 0xc);
}

/* 0x1290: the value offset */
static long double __stdcall learpfd_1290(double value)
{
    return (long double)value - GR64(0x2058);
}

/* 0x12a0: the variable less the heading (+0x4710) and a constant */
static long double __stdcall learpfd_12a0(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) - GD64(0x4710)) - GR64(0x2058);
}

/* 0x12c0: the variable less the heading (+0x4710) */
static long double __stdcall learpfd_12c0(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GD64(0x4710);
}

/* 0x12d0: the variable less the heading (+0x4710) and a constant */
static long double __stdcall learpfd_12d0(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) - GD64(0x4710)) - GR64(0x2058);
}

/* 0x12f0: the variable as it is */
static long double __stdcall learpfd_12f0(uint8_t *element)
{
    return PF64(element, 0xc);
}

/* 0x1300: the variable less the heading (+0x4710) and a constant, negated */
static long double __stdcall learpfd_1300(uint8_t *element)
{
    return gauge_fchs(((long double)PF64(element, 0xc) - GD64(0x4710)) - GR64(0x2058));
}

/* 0x1320: the variable offset */
static long double __stdcall learpfd_1320(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2058);
}

/* 0x1330: the course pointer by the source (+0x308c): 1 the variable less the heading and a constant, 2 less another (each with bit 30 of +0x4308 cleared); otherwise hidden (the bit set) at a constant */
static long double __stdcall learpfd_1330(uint8_t *element)
{
    uint32_t source = GD32(0x308c);
    if (source == 1) {
        GD32(0x4308) &= 0xbfffffffu;
        return ((long double)PF64(element, 0xc) - GD64(0x4710)) - GR64(0x2058);
    }
    if (source == 2) {
        GD32(0x4308) &= 0xbfffffffu;
        return ((long double)PF64(element, 0xc) - GD64(0x4710)) - GR64(0x2060);
    }
    GD32(0x4308) |= 0x40000000u;
    return GR64(0x2020);
}

/* 0x13a0: four characters (+0x5abc) written into the element's buffer (+0x4870); returns the value */
static long double __stdcall learpfd_13a0(double value)
{
    memcpy((void *)(uintptr_t)GD32(0x4870), GAUGE_D(0x5abc), 4);
    return value;
}

/* 0x13c0: the value through _ftol into the element's buffer (+0x49a8) with +0x5a9c; returns the value */
static long double __stdcall learpfd_13c0(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x49a8), (const char *)GAUGE_D(0x5a9c), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x13f0: a readout into the element's buffer (+0x4c20): +0x4bcc at +0x2070 (equal, or unordered) one character (+0x5b28) written in; otherwise kept to [0, 999] in place, printed through _ftol with +0x5a9c. Returns the value. */
static long double __stdcall learpfd_13f0(double value)
{
    if (GAUGE_TEST(GD64(0x4bcc), GR64(0x2070), GAUGE_C3)) {
        *(uint8_t *)(uintptr_t)GD32(0x4c20) = GD8(0x5b28);
        return value;
    }
    if (!GAUGE_TEST(GD64(0x4bcc), GR64(0x2078), GAUGE_C0C3)) GD64(0x4bcc) = 999.0;
    else if (GAUGE_TEST(GD64(0x4bcc), GR64(0x2020), GAUGE_C0)) GD64(0x4bcc) = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x4c20), (const char *)GAUGE_D(0x5a9c), (uint32_t)gauge_ftol(GD64(0x4bcc)));
    return value;
}

/* 0x1490: a distance into the element's buffer (+0x4d60): +0x4cf0 at +0x2070 (equal, or unordered) one character (+0x5b28); otherwise kept to [0, 9999] in place, above +0x2088 (ordered) whole with +0x5ac8, otherwise whole and tenths (times +0x2090, each through _ftol) with +0x5ac0. Returns the value. */
static long double __stdcall learpfd_1490(double value)
{
    if (GAUGE_TEST(GD64(0x4cf0), GR64(0x2070), GAUGE_C3)) {
        *(uint8_t *)(uintptr_t)GD32(0x4d60) = GD8(0x5b28);
        return value;
    }
    if (!GAUGE_TEST(GD64(0x4cf0), GR64(0x2080), GAUGE_C0C3)) GD64(0x4cf0) = 9999.0;
    else if (GAUGE_TEST(GD64(0x4cf0), GR64(0x2020), GAUGE_C0)) GD64(0x4cf0) = 0.0;
    if (!GAUGE_TEST(GD64(0x4cf0), GR64(0x2088), GAUGE_C0C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x4d60), (const char *)GAUGE_D(0x5ac8), (uint32_t)gauge_ftol(GD64(0x4cf0)));
    } else {
        uint32_t whole = (uint32_t)gauge_ftol(GD64(0x4cf0));
        uint32_t part = (uint32_t)gauge_ftol(((long double)GD64(0x4cf0) - (long double)(int32_t)whole) * GR64(0x2090));
        wsprintfA((char *)(uintptr_t)GD32(0x4d60), (const char *)GAUGE_D(0x5ac0), whole, part);
    }
    return value;
}

/* 0x1590: an annunciator written into the element's buffer (+0x4ae0): with +0x4a8c at +0x2070 (equal, or unordered) one character (+0x5b28), otherwise five (+0x5ad0). Returns the value. */
static long double __stdcall learpfd_1590(double value)
{
    uint8_t *text = (uint8_t *)(uintptr_t)GD32(0x4ae0);
    if (GAUGE_TEST(GD64(0x4a8c), GR64(0x2070), GAUGE_C3)) {
        text[0] = GD8(0x5b28);
        return value;
    }
    memcpy(text, GAUGE_D(0x5ad0), 4);
    text[4] = GD8(0x5ad4);
    return value;
}

/* 0x15d0: a readout into the element's buffer (+0x4fd8): +0x4f84 at +0x2070 (equal, or unordered) one character (+0x5b28) written in; otherwise kept to [0, 999] in place, printed through _ftol with +0x5a9c. Returns the value. */
static long double __stdcall learpfd_15d0(double value)
{
    if (GAUGE_TEST(GD64(0x4f84), GR64(0x2070), GAUGE_C3)) {
        *(uint8_t *)(uintptr_t)GD32(0x4fd8) = GD8(0x5b28);
        return value;
    }
    if (!GAUGE_TEST(GD64(0x4f84), GR64(0x2078), GAUGE_C0C3)) GD64(0x4f84) = 999.0;
    else if (GAUGE_TEST(GD64(0x4f84), GR64(0x2020), GAUGE_C0)) GD64(0x4f84) = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x4fd8), (const char *)GAUGE_D(0x5a9c), (uint32_t)gauge_ftol(GD64(0x4f84)));
    return value;
}

/* 0x1670: a distance into the element's buffer (+0x5118): +0x50a8 at +0x2070 (equal, or unordered) one character (+0x5b28); otherwise kept to [0, 9999] in place, above +0x2088 (ordered) whole with +0x5ac8, otherwise whole and tenths (times +0x2090, each through _ftol) with +0x5ac0. Returns the value. */
static long double __stdcall learpfd_1670(double value)
{
    if (GAUGE_TEST(GD64(0x50a8), GR64(0x2070), GAUGE_C3)) {
        *(uint8_t *)(uintptr_t)GD32(0x5118) = GD8(0x5b28);
        return value;
    }
    if (!GAUGE_TEST(GD64(0x50a8), GR64(0x2080), GAUGE_C0C3)) GD64(0x50a8) = 9999.0;
    else if (GAUGE_TEST(GD64(0x50a8), GR64(0x2020), GAUGE_C0)) GD64(0x50a8) = 0.0;
    if (!GAUGE_TEST(GD64(0x50a8), GR64(0x2088), GAUGE_C0C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x5118), (const char *)GAUGE_D(0x5ac8), (uint32_t)gauge_ftol(GD64(0x50a8)));
    } else {
        uint32_t whole = (uint32_t)gauge_ftol(GD64(0x50a8));
        uint32_t part = (uint32_t)gauge_ftol(((long double)GD64(0x50a8) - (long double)(int32_t)whole) * GR64(0x2090));
        wsprintfA((char *)(uintptr_t)GD32(0x5118), (const char *)GAUGE_D(0x5ac0), whole, part);
    }
    return value;
}

/* 0x1770: an annunciator written into the element's buffer (+0x4e98): with +0x4e44 at +0x2070 (equal, or unordered) one character (+0x5b28), otherwise five (+0x5ad8). Returns the value. */
static long double __stdcall learpfd_1770(double value)
{
    uint8_t *text = (uint8_t *)(uintptr_t)GD32(0x4e98);
    if (GAUGE_TEST(GD64(0x4e44), GR64(0x2070), GAUGE_C3)) {
        text[0] = GD8(0x5b28);
        return value;
    }
    memcpy(text, GAUGE_D(0x5ad8), 4);
    text[4] = GD8(0x5adc);
    return value;
}

/* 0x17b0: a light: the variable at a constant (equal, or unordered) sets bit 30 of +0x51c8 and gives the constant; otherwise the bit cleared and the variable offset */
static long double __stdcall learpfd_17b0(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2020), GAUGE_C3)) {
        GD32(0x51c8) |= 0x40000000u;
        return GR64(0x2020);
    }
    GD32(0x51c8) &= 0xbfffffffu;
    return (long double)PF64(element, 0xc) - GR64(0x2008);
}

/* 0x1800: +0x5268 scaled in place (the test on the unrounded product), at most 99, through _ftol into the element's buffer (+0x52d8) with +0x5ae0; returns it */
static long double __stdcall learpfd_1800(double value)
{
    long double scaled = (long double)GD64(0x5268) * GR64(0x2098);
    (void)value;
    gauge_fst(GAUGE_D(0x5268), scaled);
    if (!GAUGE_TEST(scaled, GR64(0x20a0), GAUGE_C0C3)) GD64(0x5268) = 99.0;
    wsprintfA((char *)(uintptr_t)GD32(0x52d8), (const char *)GAUGE_D(0x5ae0), (uint32_t)gauge_ftol(GD64(0x5268)));
    return GD64(0x5268);
}

/* 0x1860: a button's mouse callback: the flag (+0x5b20) toggled; returns 0 */
static uint32_t __stdcall learpfd_1860(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x5b20) = GD32(0x5b20) == 0;
    return 0;
}

/* 0x1880: a button's mouse callback: the flag (+0x5b24) toggled; returns 0 */
static uint32_t __stdcall learpfd_1880(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x5b24) = GD32(0x5b24) == 0;
    return 0;
}

/* 0x18a0, header +0x10: install: four variables initialised (PANELS +0x98), then the standard install */
static void __stdcall learpfd_18a0(uint32_t context)
{
    static const uint16_t variables[4] = {0x3080, 0x3060, 0x30a0, 0x30c0};
    unsigned at;
    for (at = 0; at < 4; ++at) GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(variables[at]));
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(learpfd_initialize)   /* 0x1930 */

#define LEARPFD_ON(rva) (GD32(rva) |= 0x40000000u)
#define LEARPFD_OFF(rva) (GD32(rva) &= 0xbfffffffu)
/* 0x1940, header +0x18: update: the four variables looked up (PANELS +0xa0) and the elements;
 * +0x3df4 negated while above a constant (ordered); then the annunciators' bit 30: +0x4220 on
 * unless +0x5b24; with +0x30dc set, +0x3f08 off and, by bit 3 of +0x30ac, +0x3ff0 and +0x45c0 on
 * and +0x44d8 off or the other way round; with it clear all four on; +0x4138 off while +0x5b20;
 * +0x3a88 and +0x3b38 on while +0x306c is at the constant (equal, or unordered) */
static void __stdcall learpfd_1940(void)
{
    static const uint16_t variables[4] = {0x3080, 0x3060, 0x30a0, 0x30c0};
    unsigned at;
    for (at = 0; at < 4; ++at) GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(variables[at]));
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
    if (!GAUGE_TEST(GD64(0x3df4), GR64(0x2020), GAUGE_C0C3)) GD64(0x3df4) = (double)gauge_fchs(GD64(0x3df4));
    if (!GD32(0x5b24)) LEARPFD_ON(0x4220); else LEARPFD_OFF(0x4220);
    if (GD32(0x30dc)) {
        LEARPFD_OFF(0x3f08);
        if (GD8(0x30ac) & 8) { LEARPFD_ON(0x3ff0); LEARPFD_ON(0x45c0); LEARPFD_OFF(0x44d8); }
        else { LEARPFD_OFF(0x3ff0); LEARPFD_OFF(0x45c0); LEARPFD_ON(0x44d8); }
    } else {
        LEARPFD_ON(0x3ff0); LEARPFD_ON(0x3f08); LEARPFD_ON(0x45c0); LEARPFD_ON(0x44d8);
    }
    if (GD32(0x5b20)) LEARPFD_OFF(0x4138); else LEARPFD_ON(0x4138);
    if (GAUGE_TEST(GD64(0x306c), GR64(0x2020), GAUGE_C3)) { LEARPFD_ON(0x3a88); LEARPFD_ON(0x3b38); }
    else { LEARPFD_OFF(0x3a88); LEARPFD_OFF(0x3b38); }
}
GAUGE_GENERATE(learpfd_generate)   /* 0x1ac0 */
GAUGE_DRAW(learpfd_draw)   /* 0x1ae0 */
GAUGE_KILL(learpfd_kill)   /* 0x1af0 */
GAUGE_MODULE(learpfd_module)   /* 0x1b20 */

GAUGE_QUERY(learpfd_1b30)   /* 0x1b30 (the _ftol import thunk at 0x1b50 follows it) */

GAUGE_ROUTINES({0x1020, (void *)learpfd_1020}, {0x1080, (void *)learpfd_1080}, {0x1090, (void *)learpfd_1090}, {0x10c0, (void *)learpfd_10c0}, {0x1140, (void *)learpfd_1140}, {0x11a0, (void *)learpfd_11a0}, {0x11b0, (void *)learpfd_11b0}, {0x11f0, (void *)learpfd_11f0}, {0x1240, (void *)learpfd_1240}, {0x1250, (void *)learpfd_1250}, {0x1290, (void *)learpfd_1290}, {0x12a0, (void *)learpfd_12a0}, {0x12c0, (void *)learpfd_12c0}, {0x12d0, (void *)learpfd_12d0}, {0x12f0, (void *)learpfd_12f0}, {0x1300, (void *)learpfd_1300}, {0x1320, (void *)learpfd_1320}, {0x1330, (void *)learpfd_1330}, {0x13a0, (void *)learpfd_13a0}, {0x13c0, (void *)learpfd_13c0}, {0x13f0, (void *)learpfd_13f0}, {0x1490, (void *)learpfd_1490}, {0x1590, (void *)learpfd_1590}, {0x15d0, (void *)learpfd_15d0}, {0x1670, (void *)learpfd_1670}, {0x1770, (void *)learpfd_1770}, {0x17b0, (void *)learpfd_17b0}, {0x1800, (void *)learpfd_1800}, {0x1860, (void *)learpfd_1860}, {0x1880, (void *)learpfd_1880}, {0x18a0, (void *)learpfd_18a0}, {0x1930, (void *)learpfd_initialize}, {0x1940, (void *)learpfd_1940}, {0x1ac0, (void *)learpfd_generate}, {0x1ae0, (void *)learpfd_draw}, {0x1af0, (void *)learpfd_kill}, {0x1b20, (void *)learpfd_module}, {0x1b30, (void *)learpfd_1b30})
