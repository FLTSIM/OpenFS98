/* CESSTIMR.GAU -- CESSNA_182.TIMER: the clock and the timer as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3628u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3398u
#define GAUGE_MOUSE 0x33a0u
#define GAUGE_MOUSE_ARGUMENT 0x3384u


/* 0x1020: the clock: the hours, minutes and seconds (+0x30f8, +0x3114, +0x3130, each through
 * _ftol) printed into the element's buffer (+0x3168) with the format at +0x3608, last first;
 * returns the value */
static long double __stdcall cesstimr_1020(double value)
{
    uint32_t first = (uint32_t)gauge_ftol(GD64(0x30f8));
    uint32_t second = (uint32_t)gauge_ftol(GD64(0x3114));
    uint32_t third = (uint32_t)gauge_ftol(GD64(0x3130));
    wsprintfA((char *)(uintptr_t)GD32(0x3168), (const char *)GAUGE_D(0x3608), third, second, first);
    return value;
}

/* 0x1060: the timer: the time (+0x3248) as whole and fraction (the fraction times the first
 * constant, each through _ftol) printed into the element's buffer (+0x32b8) with the format at
 * +0x3618; returns the value */
static long double __stdcall cesstimr_1060(double value)
{
    uint32_t whole = (uint32_t)gauge_ftol(GD64(0x3248));
    uint32_t part = (uint32_t)gauge_ftol(((long double)GD64(0x3248) - (long double)(int32_t)whole) * GR64(0x2000));
    wsprintfA((char *)(uintptr_t)GD32(0x32b8), (const char *)GAUGE_D(0x3618), whole, part);
    return value;
}
GAUGE_INSTALL(cesstimr_install)   /* 0x10b0 */
GAUGE_INITIALIZE(cesstimr_initialize)   /* 0x1100 */
GAUGE_UPDATE(cesstimr_update)   /* 0x1110 */
GAUGE_GENERATE(cesstimr_generate)   /* 0x1120 */
GAUGE_DRAW(cesstimr_draw)   /* 0x1140 */
GAUGE_KILL(cesstimr_kill)   /* 0x1150 */
GAUGE_MODULE(cesstimr_module)   /* 0x1180 */

GAUGE_QUERY(cesstimr_1190)   /* 0x1190 (the _ftol import thunk at 0x11b0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)cesstimr_1020}, {0x1060, (void *)cesstimr_1060}, {0x10b0, (void *)cesstimr_install}, {0x1100, (void *)cesstimr_initialize}, {0x1110, (void *)cesstimr_update}, {0x1120, (void *)cesstimr_generate}, {0x1140, (void *)cesstimr_draw}, {0x1150, (void *)cesstimr_kill}, {0x1180, (void *)cesstimr_module}, {0x1190, (void *)cesstimr_1190})
