/* 737CLKSM.GAU -- 737-400.CLOCK-SIM-RATE: the clock: its hands, the time and the elapsed time as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3bc0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3718u
#define GAUGE_MOUSE 0x3720u
#define GAUGE_MOUSE_ARGUMENT 0x3704u


/* 0x1020: the elapsed time's reset (a mouse callback, and called by 0x10c0): its start (+0x3bf0)
 * the time now in seconds (+0x34b0, +0x3494, +0x3478: hours, minutes, seconds), through _ftol;
 * returns 0 */
static uint32_t __stdcall g737clksm_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x3bf0) = (uint32_t)gauge_ftol(((long double)GD64(0x34b0) * GR64(0x2000) + (long double)GD64(0x3494) * GR64(0x2008)) + GD64(0x3478));
    return 0;
}

/* 0x1060: the elapsed seconds' hand: the seconds since the start, unsigned modulo 60, scaled
 * and offset */
static long double __stdcall g737clksm_1060(uint8_t *element)
{
    uint32_t elapsed = (uint32_t)gauge_ftol(((long double)GD64(0x34b0) * GR64(0x2000) + (long double)GD64(0x3494) * GR64(0x2008)) + GD64(0x3478)) - GD32(0x3bf0);
    (void)element;
    return (long double)(uint64_t)(elapsed % 60) * GR64(0x2010) - GR64(0x2018);
}

/* 0x10c0: the time as text: hours, minutes and seconds (+0x34b0, +0x3494, +0x3478, each through
 * _ftol) into the element's buffer (+0x34e8) with the format at +0x3b6c; a pending reset
 * (+0x3080) done; returns the seconds */
static long double __stdcall g737clksm_10c0(double value)
{
    uint32_t seconds = (uint32_t)gauge_ftol(GD64(0x3478));
    uint32_t minutes = (uint32_t)gauge_ftol(GD64(0x3494));
    uint32_t hours = (uint32_t)gauge_ftol(GD64(0x34b0));
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x34e8), (const char *)GAUGE_D(0x3b6c), hours, minutes, seconds);
    if (GD32(0x3080)) {
        GD32(0x3080) = 0;
        g737clksm_1020(0, 0);
    }
    return GD64(0x3478);
}

/* 0x1120: the chronometer's text into the element's buffer (+0x3638): below the fifth constant
 * (or unordered), whole and hundredths (the value times the sixth, through _ftol, unsigned
 * modulo 100) with +0x3b88; below the sixth (or unordered), whole and tenths (times the fifth,
 * modulo 10) with +0x3b80; otherwise whole with +0x3b7c. Returns the value. */
static long double __stdcall g737clksm_1120(double value)
{
    char *text = (char *)(uintptr_t)GD32(0x3638);
    if (GAUGE_TEST(value, GR64(0x2020), GAUGE_C0)) {
        uint32_t part = (uint32_t)gauge_ftol((long double)value * GR64(0x2028)) % 100;
        wsprintfA((char *)(uintptr_t)GD32(0x3638), (const char *)GAUGE_D(0x3b88), (uint32_t)gauge_ftol(value), part);
    } else if (GAUGE_TEST(value, GR64(0x2028), GAUGE_C0)) {
        uint32_t part = (uint32_t)gauge_ftol((long double)value * GR64(0x2020)) % 10;
        wsprintfA((char *)(uintptr_t)GD32(0x3638), (const char *)GAUGE_D(0x3b80), (uint32_t)gauge_ftol(value), part);
    } else {
        wsprintfA(text, (const char *)GAUGE_D(0x3b7c), (uint32_t)gauge_ftol(value));
    }
    return value;
}

/* 0x11c0: the elapsed minutes as text: the seconds since the start over 60 (unsigned), modulo
 * 60, into the element's buffer (+0x3228) with the format at +0x3b94; returns the seconds */
static long double __stdcall g737clksm_11c0(double value)
{
    uint32_t elapsed = (uint32_t)gauge_ftol(((long double)GD64(0x34b0) * GR64(0x2000) + (long double)GD64(0x3494) * GR64(0x2008)) + GD64(0x3478)) - GD32(0x3bf0);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3228), (const char *)GAUGE_D(0x3b94), (elapsed / 60) % 60);
    return GD64(0x3478);
}

/* 0x1230: the timer (+0x3310) as text into the element's buffer (+0x3380): with the value at
 * or above the sixth constant (ordered), whole with +0x3bb4; at or above the fifth, whole and
 * the fraction times the fifth with +0x3ba8; below, whole and the fraction times the sixth with
 * +0x3b9c (each through _ftol). Returns the value. */
static long double __stdcall g737clksm_1230(double value)
{
    uint32_t whole, part;
    if (!GAUGE_TEST(value, GR64(0x2028), GAUGE_C0)) {
        wsprintfA((char *)(uintptr_t)GD32(0x3380), (const char *)GAUGE_D(0x3bb4), (uint32_t)gauge_ftol(GD64(0x3310)));
        return value;
    }
    if (!GAUGE_TEST(value, GR64(0x2020), GAUGE_C0)) {
        whole = (uint32_t)gauge_ftol(GD64(0x3310));
        part = (uint32_t)gauge_ftol(((long double)GD64(0x3310) - (long double)(int32_t)whole) * GR64(0x2020));
        wsprintfA((char *)(uintptr_t)GD32(0x3380), (const char *)GAUGE_D(0x3ba8), whole, part);
        return value;
    }
    whole = (uint32_t)gauge_ftol(GD64(0x3310));
    part = (uint32_t)gauge_ftol(((long double)GD64(0x3310) - (long double)(int32_t)whole) * GR64(0x2028));
    wsprintfA((char *)(uintptr_t)GD32(0x3380), (const char *)GAUGE_D(0x3b9c), whole, part);
    return value;
}
GAUGE_INSTALL(g737clksm_install)   /* 0x1310 */
GAUGE_INITIALIZE(g737clksm_initialize)   /* 0x1360 */
GAUGE_UPDATE(g737clksm_update)   /* 0x1370 */
GAUGE_GENERATE(g737clksm_generate)   /* 0x1380 */
GAUGE_DRAW(g737clksm_draw)   /* 0x13a0 */
GAUGE_KILL(g737clksm_kill)   /* 0x13b0 */
GAUGE_MODULE(g737clksm_module)   /* 0x13e0 */

GAUGE_QUERY(g737clksm_13f0)   /* 0x13f0 (the _ftol import thunk at 0x1410 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737clksm_1020}, {0x1060, (void *)g737clksm_1060}, {0x10c0, (void *)g737clksm_10c0}, {0x1120, (void *)g737clksm_1120}, {0x11c0, (void *)g737clksm_11c0}, {0x1230, (void *)g737clksm_1230}, {0x1310, (void *)g737clksm_install}, {0x1360, (void *)g737clksm_initialize}, {0x1370, (void *)g737clksm_update}, {0x1380, (void *)g737clksm_generate}, {0x13a0, (void *)g737clksm_draw}, {0x13b0, (void *)g737clksm_kill}, {0x13e0, (void *)g737clksm_module}, {0x13f0, (void *)g737clksm_13f0})
