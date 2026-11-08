/* EXGMETER.GAU -- EXTRA-300.G-METER: the g-meter, its peak and minimum and their annunciators */
#include "gauge.h"

#define GAUGE_IMPORTS 0x4288u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x4160u
#define GAUGE_MOUSE 0x4168u
#define GAUGE_MOUSE_ARGUMENT 0x414cu


/* 0x1020: the reset knob's mouse callback: the peak and minimum (+0x42c0, +0x42c8) cleared;
 * returns 0 */
static uint32_t __stdcall exgmeter_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x42c0) = 0; GD32(0x42c4) = 0; GD32(0x42c8) = 0; GD32(0x42cc) = 0;
    return 0;
}

/* 0x1050: the load factor (+0x3e78 through two scales, kept at +0x42d0) raises the peak
 * (+0x42c0, the test on the unrounded value, the copy the stored one); the peak printed as
 * whole and tenths (the fraction times the fourth constant through _ftol, its magnitude) into
 * the element's buffer (+0x3ee8) with the format at +0x4270; returns the peak */
static long double __stdcall exgmeter_1050(double value)
{
    long double load = ((long double)GD64(0x3e78) * GR64(0x2008)) * GR64(0x2010);
    uint32_t whole, tenths;
    (void)value;
    gauge_fst(GAUGE_D(0x42d0), load);     /* the fst (no reload) */
    if (!GAUGE_TEST(load, GD64(0x42c0), GAUGE_C0C3)) { GD32(0x42c0) = GD32(0x42d0); GD32(0x42c4) = GD32(0x42d4); }
    whole = (uint32_t)gauge_ftol(GD64(0x42c0));
    tenths = (uint32_t)gauge_ftol(((long double)GD64(0x42c0) - (long double)(int32_t)whole) * GR64(0x2018));
    if ((int32_t)tenths < 0) tenths = 0u - tenths;
    wsprintfA((char *)(uintptr_t)GD32(0x3ee8), (const char *)GAUGE_D(0x4270), whole, tenths);
    return GD64(0x42c0);
}

/* 0x10e0: the load factor (+0x3fb0 through two scales) lowers the minimum (+0x42c8); the minimum
 * printed as the magnitudes of its whole and tenths (each through _ftol) into the element's
 * buffer (+0x4020) with the format at +0x427c; returns the value */
static long double __stdcall exgmeter_10e0(double value)
{
    long double load = ((long double)GD64(0x3fb0) * GR64(0x2008)) * GR64(0x2010);
    uint32_t whole, tenths;
    if (GAUGE_TEST(load, GD64(0x42c8), GAUGE_C0)) GD64(0x42c8) = (double)load;
    whole = (uint32_t)gauge_ftol(GD64(0x42c8));
    tenths = (uint32_t)gauge_ftol(((long double)GD64(0x42c8) - (long double)(int32_t)whole) * GR64(0x2018));
    if ((int32_t)tenths < 0) tenths = 0u - tenths;
    if ((int32_t)whole < 0) whole = 0u - whole;
    wsprintfA((char *)(uintptr_t)GD32(0x4020), (const char *)GAUGE_D(0x427c), whole, tenths);
    return value;
}
GAUGE_INSTALL(exgmeter_install)   /* 0x1160 */

/* 0x11b0, header +0x14: initialise: the elements, then +0x37c0 1 */
static void __stdcall exgmeter_11b0(void)
{
    GAUGE_PANELS_CALL(0x68, 1)(GD32(GAUGE_ELEMENTS));
    GD64(0x37c0) = 1.0;
}

/* 0x11e0, header +0x18: update: the elements, then the annunciators: each of twelve
 * (+0x3190, 0x84 apart) -1 while the load factor (+0x42d0) is above its threshold (+0x3080, 8
 * apart; ordered) and 0 otherwise, each of twelve more (+0x3844) -1 while it is below its
 * threshold (+0x30e8; or unordered) */
static void __stdcall exgmeter_11e0(void)
{
    unsigned at;
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
    for (at = 0; at < 12; ++at)
        GD64(0x3190 + at * 0x84) = GAUGE_TEST(GD64(0x42d0), GD64(0x3080 + at * 8), GAUGE_C0C3) ? 0.0 : -1.0;
    for (at = 0; at < 12; ++at)
        GD64(0x3844 + at * 0x84) = GAUGE_TEST(GD64(0x42d0), GD64(0x30e8 + at * 8), GAUGE_C0) ? -1.0 : 0.0;
}
GAUGE_GENERATE(exgmeter_generate)   /* 0x1280 */
GAUGE_DRAW(exgmeter_draw)   /* 0x12a0 */
GAUGE_KILL(exgmeter_kill)   /* 0x12b0 */
GAUGE_MODULE(exgmeter_module)   /* 0x12e0 */

GAUGE_QUERY(exgmeter_12f0)   /* 0x12f0 (the _ftol import thunk at 0x1310 follows it) */

GAUGE_ROUTINES({0x1020, (void *)exgmeter_1020}, {0x1050, (void *)exgmeter_1050}, {0x10e0, (void *)exgmeter_10e0}, {0x1160, (void *)exgmeter_install}, {0x11b0, (void *)exgmeter_11b0}, {0x11e0, (void *)exgmeter_11e0}, {0x1280, (void *)exgmeter_generate}, {0x12a0, (void *)exgmeter_draw}, {0x12b0, (void *)exgmeter_kill}, {0x12e0, (void *)exgmeter_module}, {0x12f0, (void *)exgmeter_12f0})
