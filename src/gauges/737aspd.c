/* 737ASPD.GAU -- 737-400.AIRSPEED: the airspeed indicator: its needles, bugs and the speeds as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3b80u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3a30u
#define GAUGE_MOUSE 0x3a38u
#define GAUGE_MOUSE_ARGUMENT 0x3a1cu


/* 0x1020: the needle: the variable kept to [60, 450] in place (above the first constant,
 * ordered, 450; below the second or unordered, 60), then returned */
static long double __stdcall g737aspd_1020(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C0C3)) PF64(element, 0xc) = 450.0;
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2008), GAUGE_C0)) PF64(element, 0xc) = 60.0;
    return PF64(element, 0xc);
}

/* 0x1060: the needle: the variable kept to [60, 450] in place (above the first constant,
 * ordered, 450; below the second or unordered, 60), then returned */
static long double __stdcall g737aspd_1060(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C0C3)) PF64(element, 0xc) = 450.0;
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2008), GAUGE_C0)) PF64(element, 0xc) = 60.0;
    return PF64(element, 0xc);
}

/* 0x10a0: the needle: the variable scaled in place, then kept to [60, 450] (the high test on
 * the unrounded product) */
static long double __stdcall g737aspd_10a0(uint8_t *element)
{
    long double speed = (long double)PF64(element, 0xc) * GR64(0x2010);
    uint16_t high = gauge_fcom(speed, GR64(0x2000));
    gauge_fst(element + 0xc, speed);     /* the fst, kept though it may be overwritten (its flags) */
    if (!(high & GAUGE_C0C3)) PF64(element, 0xc) = 450.0;
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2008), GAUGE_C0)) PF64(element, 0xc) = 60.0;
    return PF64(element, 0xc);
}

/* 0x10f0: the speed bug (+0x37a8) kept to [0, 450] in place, printed through _ftol into the
 * element's buffer (+0x3818) with the format at +0x3b6c; returns it */
static long double __stdcall g737aspd_10f0(double value)
{
    (void)value;
    if (!GAUGE_TEST(GD64(0x37a8), GR64(0x2000), GAUGE_C0C3)) GD64(0x37a8) = 450.0;
    if (GAUGE_TEST(GD64(0x37a8), GR64(0x2018), GAUGE_C0)) GD64(0x37a8) = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3818), (const char *)GAUGE_D(0x3b6c), (uint32_t)gauge_ftol(GD64(0x37a8)));
    return GD64(0x37a8);
}

/* 0x1170: the Mach number (+0x38e8) scaled in place, at most 99 (the test on the unrounded
 * product), printed through _ftol into the element's buffer (+0x3958) with the format at
 * +0x3b74; returns it */
static long double __stdcall g737aspd_1170(double value)
{
    long double mach = (long double)GD64(0x38e8) * GR64(0x2020);
    (void)value;
    gauge_fst(GAUGE_D(0x38e8), mach);     /* the fst, kept though it may be overwritten (its flags) */
    if (!GAUGE_TEST(mach, GR64(0x2028), GAUGE_C0C3)) GD64(0x38e8) = 99.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3958), (const char *)GAUGE_D(0x3b74), (uint32_t)gauge_ftol(GD64(0x38e8)));
    return GD64(0x38e8);
}
GAUGE_INSTALL(g737aspd_install)   /* 0x11d0 */
GAUGE_INITIALIZE(g737aspd_initialize)   /* 0x1220 */
GAUGE_UPDATE(g737aspd_update)   /* 0x1230 */
GAUGE_GENERATE(g737aspd_generate)   /* 0x1240 */
GAUGE_DRAW(g737aspd_draw)   /* 0x1260 */
GAUGE_KILL(g737aspd_kill)   /* 0x1270 */
GAUGE_MODULE(g737aspd_module)   /* 0x12a0 */

GAUGE_QUERY(g737aspd_12b0)   /* 0x12b0 (the _ftol import thunk at 0x12d0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737aspd_1020}, {0x1060, (void *)g737aspd_1060}, {0x10a0, (void *)g737aspd_10a0}, {0x10f0, (void *)g737aspd_10f0}, {0x1170, (void *)g737aspd_1170}, {0x11d0, (void *)g737aspd_install}, {0x1220, (void *)g737aspd_initialize}, {0x1230, (void *)g737aspd_update}, {0x1240, (void *)g737aspd_generate}, {0x1260, (void *)g737aspd_draw}, {0x1270, (void *)g737aspd_kill}, {0x12a0, (void *)g737aspd_module}, {0x12b0, (void *)g737aspd_12b0})
