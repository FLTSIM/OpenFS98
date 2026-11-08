/* EXCON.GAU -- EXTRA-300.CONTROLS: the throttle, propeller and mixture levers */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34f0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32d8u
#define GAUGE_MOUSE 0x32e0u
#define GAUGE_MOUSE_ARGUMENT 0x32c4u


/* A control lever's mouse callback (0x1020, 0x1080, 0x10e0, one per event): the lever's height
 * (+8 of the rectangle +0x32c4 points at) less the point's y, times 0x3fff, over the height
 * (fidiv), through _ftol, kept to [0, 0x3fff], sent with the event (PANELS +0x90). Returns 0. */
static uint32_t excon_lever(uint8_t *point, uint32_t event)
{
    int32_t height = PI32((uintptr_t)GD32(0x32c4), 8);
    uint32_t travel = (uint32_t)height - P32(point, 4);
    int32_t lever = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)((travel << 14) - travel) / (long double)height);
    if (lever > 0x3fff) lever = 0x3fff;
    if (lever < 0) lever = 0;
    GAUGE_PANELS_CALL(0x90, 2)(event, (uint32_t)lever);
    return 0;
}

/* 0x1020: the lever for event 0x100a1 */
static uint32_t __stdcall excon_1020(uint8_t *point, uint32_t flags)
{
    (void)flags;
    return excon_lever(point, 0x100a1);
}

/* 0x1080: the lever for event 0x100e7 */
static uint32_t __stdcall excon_1080(uint8_t *point, uint32_t flags)
{
    (void)flags;
    return excon_lever(point, 0x100e7);
}

/* 0x10e0: the lever for event 0x100ed */
static uint32_t __stdcall excon_10e0(uint8_t *point, uint32_t flags)
{
    (void)flags;
    return excon_lever(point, 0x100ed);
}
GAUGE_INSTALL(excon_install)   /* 0x1140 */
GAUGE_INITIALIZE(excon_initialize)   /* 0x1190 */
GAUGE_UPDATE(excon_update)   /* 0x11a0 */
GAUGE_GENERATE(excon_generate)   /* 0x11b0 */
GAUGE_DRAW(excon_draw)   /* 0x11d0 */
GAUGE_KILL(excon_kill)   /* 0x11e0 */
GAUGE_MODULE(excon_module)   /* 0x1210 */

GAUGE_QUERY(excon_1220)   /* 0x1220 (the _ftol import thunk at 0x1240 follows it) */

GAUGE_ROUTINES({0x1020, (void *)excon_1020}, {0x1080, (void *)excon_1080}, {0x10e0, (void *)excon_10e0}, {0x1140, (void *)excon_install}, {0x1190, (void *)excon_initialize}, {0x11a0, (void *)excon_update}, {0x11b0, (void *)excon_generate}, {0x11d0, (void *)excon_draw}, {0x11e0, (void *)excon_kill}, {0x1210, (void *)excon_module}, {0x1220, (void *)excon_1220})
