/* 737ADI.GAU -- 737-400.ADI: the ADI and its readouts */
#include "gauge.h"

#define GAUGE_IMPORTS 0x4038u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3f30u
#define GAUGE_MOUSE 0x3f38u
#define GAUGE_MOUSE_ARGUMENT 0x3f1cu


/* 0x1020: the pitch: the variable above the first constant (ordered) gives that constant, below
 * the second (or unordered) the second, otherwise the variable */
static long double __stdcall g737adi_1020(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C0C3)) return GR64(0x2000);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2008), GAUGE_C0)) return GR64(0x2008);
    return PF64(element, 0xc);
}

/* 0x1060: a constant */
static long double __stdcall g737adi_1060(uint8_t *element)
{
    (void)element;
    return GR64(0x2010);
}

/* 0x1070: the radio altitude's drum: 1000 less the variable (through _ftol) modulo 1000, unsigned */
static long double __stdcall g737adi_1070(uint8_t *element)
{
    uint32_t height = (uint32_t)gauge_ftol(PF64(element, 0xc));
    return (long double)(uint64_t)(1000u - height % 1000u);
}

/* 0x10b0: a constant */
static long double __stdcall g737adi_10b0(uint8_t *element)
{
    (void)element;
    return GR64(0x2010);
}

/* 0x10c0: the radio altitude (+0x3148) kept to [0, 999] in place (above the fourth constant,
 * ordered, 999; below the fifth or unordered, 0), printed through _ftol into the element's buffer
 * (+0x31b8) with the format at +0x4014; returns it */
static long double __stdcall g737adi_10c0(double value)
{
    (void)value;
    if (!GAUGE_TEST(GD64(0x3148), GR64(0x2018), GAUGE_C0C3)) GD64(0x3148) = 999.0;
    if (GAUGE_TEST(GD64(0x3148), GR64(0x2020), GAUGE_C0)) GD64(0x3148) = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x31b8), (const char *)GAUGE_D(0x4014), (uint32_t)gauge_ftol(GD64(0x3148)));
    return GD64(0x3148);
}

/* 0x1140: the value scaled (parked in the argument's slot), at most 99 (the test on the unrounded
 * product), through _ftol into the element's buffer (+0x33f0) with the format at +0x401c; returns it */
static long double __stdcall g737adi_1140(double value)
{
    long double scaled = (long double)value * GR64(0x2028);
    uint16_t high = gauge_fcom(scaled, GR64(0x2030));
    gauge_fst(&value, scaled);
    if (!(high & GAUGE_C0C3)) value = 99.0;
    wsprintfA((char *)(uintptr_t)GD32(0x33f0), (const char *)GAUGE_D(0x401c), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1190: the value through _ftol plus 1000 times the scaled value through _ftol (in 32-bit
 * arithmetic, as retail builds it: -((-n * 4 - n) * 25) * 8), into the element's buffer (+0x3528)
 * with the format at +0x4014; returns the value */
static long double __stdcall g737adi_1190(double value)
{
    uint32_t thousands = (uint32_t)gauge_ftol((long double)value * GR64(0x2038));
    uint32_t scaled = ((((0u - thousands) << 2) - thousands) * 5u * 5u) << 3;
    uint32_t units = (uint32_t)gauge_ftol(value);
    wsprintfA((char *)(uintptr_t)GD32(0x3528), (const char *)GAUGE_D(0x4014), units - scaled);
    return value;
}

/* 0x11e0: +0x3b98 through _ftol into the element's buffer (+0x3c08) with the format at +0x401c;
 * returns it */
static long double __stdcall g737adi_11e0(double value)
{
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3c08), (const char *)GAUGE_D(0x401c), (uint32_t)gauge_ftol(GD64(0x3b98)));
    return GD64(0x3b98);
}

/* 0x1210: the fraction of +0x3cd0 times the ninth constant (through _ftol) into the element's
 * buffer (+0x3d40) with the format at +0x4024; returns +0x3cd0 */
static long double __stdcall g737adi_1210(double value)
{
    uint32_t whole = (uint32_t)gauge_ftol(GD64(0x3cd0));
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3d40), (const char *)GAUGE_D(0x4024),
              (uint32_t)gauge_ftol(((long double)GD64(0x3cd0) - (long double)(int32_t)whole) * GR64(0x2040)));
    return GD64(0x3cd0);
}

/* 0x1260: +0x3928 scaled in place (the test on the unrounded product), at most 99, through _ftol
 * into the element's buffer (+0x3998) with the format at +0x4014; returns it */
static long double __stdcall g737adi_1260(double value)
{
    long double scaled = (long double)GD64(0x3928) * GR64(0x2048);
    (void)value;
    gauge_fst(GAUGE_D(0x3928), scaled);
    if (!GAUGE_TEST(scaled, GR64(0x2030), GAUGE_C0C3)) GD64(0x3928) = 99.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3998), (const char *)GAUGE_D(0x4014), (uint32_t)gauge_ftol(GD64(0x3928)));
    return GD64(0x3928);
}

/* 0x12c0: the value through _ftol into the element's buffer (+0x3ad0) with the format at +0x402c;
 * returns the value */
static long double __stdcall g737adi_12c0(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x3ad0), (const char *)GAUGE_D(0x402c), (uint32_t)gauge_ftol(value));
    return value;
}
GAUGE_INSTALL(g737adi_install)   /* 0x12f0 */
GAUGE_INITIALIZE(g737adi_initialize)   /* 0x1340 */

/* 0x1350, header +0x18: update: the elements, then +0x3e34 negated while it is above the fifth
 * constant (ordered) */
static void __stdcall g737adi_1350(void)
{
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
    if (!GAUGE_TEST(GD64(0x3e34), GR64(0x2020), GAUGE_C0C3)) GD64(0x3e34) = (double)gauge_fchs(GD64(0x3e34));
}
GAUGE_GENERATE(g737adi_generate)   /* 0x1390 */
GAUGE_DRAW(g737adi_draw)   /* 0x13b0 */
GAUGE_KILL(g737adi_kill)   /* 0x13c0 */
GAUGE_MODULE(g737adi_module)   /* 0x13f0 */

GAUGE_QUERY(g737adi_1400)   /* 0x1400 (the _ftol import thunk at 0x1420 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737adi_1020}, {0x1060, (void *)g737adi_1060}, {0x1070, (void *)g737adi_1070}, {0x10b0, (void *)g737adi_10b0}, {0x10c0, (void *)g737adi_10c0}, {0x1140, (void *)g737adi_1140}, {0x1190, (void *)g737adi_1190}, {0x11e0, (void *)g737adi_11e0}, {0x1210, (void *)g737adi_1210}, {0x1260, (void *)g737adi_1260}, {0x12c0, (void *)g737adi_12c0}, {0x12f0, (void *)g737adi_install}, {0x1340, (void *)g737adi_initialize}, {0x1350, (void *)g737adi_1350}, {0x1390, (void *)g737adi_generate}, {0x13b0, (void *)g737adi_draw}, {0x13c0, (void *)g737adi_kill}, {0x13f0, (void *)g737adi_module}, {0x1400, (void *)g737adi_1400})
