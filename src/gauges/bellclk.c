/* BELLCLK.GAU -- BELL_206B.CLOCK: the clock's hands and the timer as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x36c0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x34c8u
#define GAUGE_MOUSE 0x34d0u
#define GAUGE_MOUSE_ARGUMENT 0x34b4u


/* 0x1020: a hand: the value scaled, offset */
static long double __stdcall bellclk_1020(double value)
{
    return (long double)value * GR64(0x2000) - GR64(0x2008);
}

/* 0x1040: a hand: the value scaled, offset */
static long double __stdcall bellclk_1040(double value)
{
    return (long double)value * GR64(0x2000) - GR64(0x2008);
}

/* 0x1060: a hand: the value scaled, offset */
static long double __stdcall bellclk_1060(double value)
{
    return (long double)value * GR64(0x2010) - GR64(0x2008);
}

/* 0x1080: the value as whole and fraction (the fraction times the fourth constant, each through
 * _ftol) into the element's buffer (+0x33f0) with the format at +0x36b4; returns the value */
static long double __stdcall bellclk_1080(double value)
{
    uint32_t whole = (uint32_t)gauge_ftol(value);
    uint32_t part = (uint32_t)gauge_ftol(((long double)value - (long double)(int32_t)whole) * GR64(0x2018));
    wsprintfA((char *)(uintptr_t)GD32(0x33f0), (const char *)GAUGE_D(0x36b4), whole, part);
    return value;
}
GAUGE_INSTALL(bellclk_install)   /* 0x10d0 */
GAUGE_INITIALIZE(bellclk_initialize)   /* 0x1120 */
GAUGE_UPDATE(bellclk_update)   /* 0x1130 */
GAUGE_GENERATE(bellclk_generate)   /* 0x1140 */
GAUGE_DRAW(bellclk_draw)   /* 0x1160 */
GAUGE_KILL(bellclk_kill)   /* 0x1170 */
GAUGE_MODULE(bellclk_module)   /* 0x11a0 */

GAUGE_QUERY(bellclk_11b0)   /* 0x11b0 (the _ftol import thunk at 0x11d0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)bellclk_1020}, {0x1040, (void *)bellclk_1040}, {0x1060, (void *)bellclk_1060}, {0x1080, (void *)bellclk_1080}, {0x10d0, (void *)bellclk_install}, {0x1120, (void *)bellclk_initialize}, {0x1130, (void *)bellclk_update}, {0x1140, (void *)bellclk_generate}, {0x1160, (void *)bellclk_draw}, {0x1170, (void *)bellclk_kill}, {0x11a0, (void *)bellclk_module}, {0x11b0, (void *)bellclk_11b0})
