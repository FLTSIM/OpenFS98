/* LEARCHRN.GAU -- LEAR_45.CHRONOMETER: the chronometer */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3630u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x33a0u
#define GAUGE_MOUSE 0x33a8u
#define GAUGE_MOUSE_ARGUMENT 0x338cu


/* 0x1020 (called by 0x1050): the elapsed time's start (+0x3660): the hours, minutes and
 * seconds (+0x3288, +0x326c, +0x3250) as seconds, through _ftol */
static void learchrn_1020(void)
{
    GD32(0x3660) = (uint32_t)gauge_ftol(((long double)GD64(0x3288) * GR64(0x2000) + (long double)GD64(0x326c) * GR64(0x2008)) + GD64(0x3250));
}

/* 0x1050: the chronometer as text, into the element's buffer (+0x32c0) with the format at
 * +0x3610: a reset (+0x3080) starts the elapsed time again; showing the clock (+0x3084), its
 * hours, minutes and seconds; otherwise the elapsed time (+0x3664 the time now, +0x3668 since
 * the start) as hours modulo 24, minutes and seconds (the seconds a signed remainder, the
 * minutes and hours from the unsigned quotients). Returns the value. */
static long double __stdcall learchrn_1050(double value)
{
    uint32_t elapsed;
    if (GD32(0x3080)) {
        GD32(0x3080) = 0;
        learchrn_1020();
    }
    if (GD32(0x3084)) {
        uint32_t seconds = (uint32_t)gauge_ftol(GD64(0x3250));
        uint32_t minutes = (uint32_t)gauge_ftol(GD64(0x326c));
        uint32_t hours = (uint32_t)gauge_ftol(GD64(0x3288));
        wsprintfA((char *)(uintptr_t)GD32(0x32c0), (const char *)GAUGE_D(0x3610), hours, minutes, seconds);
        return value;
    }
    GD32(0x3664) = (uint32_t)gauge_ftol(((long double)GD64(0x3288) * GR64(0x2000) + (long double)GD64(0x326c) * GR64(0x2008)) + GD64(0x3250));
    elapsed = GD32(0x3664) - GD32(0x3660);
    GD32(0x3668) = elapsed;
    wsprintfA((char *)(uintptr_t)GD32(0x32c0), (const char *)GAUGE_D(0x3610), (int32_t)(elapsed / 3600) % 24,
              (int32_t)(elapsed / 60) % 60, (int32_t)elapsed % 60);
    return value;
}

/* 0x1140: the timer: the time (+0x30e8) as whole and fraction (the fraction times the third
 * constant, each through _ftol) into the element's buffer (+0x3158) with the format at +0x3620;
 * returns the value */
static long double __stdcall learchrn_1140(double value)
{
    uint32_t whole = (uint32_t)gauge_ftol(GD64(0x30e8));
    uint32_t part = (uint32_t)gauge_ftol(((long double)GD64(0x30e8) - (long double)(int32_t)whole) * GR64(0x2010));
    wsprintfA((char *)(uintptr_t)GD32(0x3158), (const char *)GAUGE_D(0x3620), whole, part);
    return value;
}
GAUGE_INSTALL(learchrn_install)   /* 0x1190 */
GAUGE_INITIALIZE(learchrn_initialize)   /* 0x11e0 */
GAUGE_UPDATE(learchrn_update)   /* 0x11f0 */
GAUGE_GENERATE(learchrn_generate)   /* 0x1200 */
GAUGE_DRAW(learchrn_draw)   /* 0x1220 */
GAUGE_KILL(learchrn_kill)   /* 0x1230 */
GAUGE_MODULE(learchrn_module)   /* 0x1260 */

GAUGE_QUERY(learchrn_1270)   /* 0x1270 (the _ftol import thunk at 0x1290 follows it) */

GAUGE_ROUTINES({0x1020, (void *)learchrn_1020}, {0x1050, (void *)learchrn_1050}, {0x1140, (void *)learchrn_1140}, {0x1190, (void *)learchrn_install}, {0x11e0, (void *)learchrn_initialize}, {0x11f0, (void *)learchrn_update}, {0x1200, (void *)learchrn_generate}, {0x1220, (void *)learchrn_draw}, {0x1230, (void *)learchrn_kill}, {0x1260, (void *)learchrn_module}, {0x1270, (void *)learchrn_1270})
