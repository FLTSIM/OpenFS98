/* CESSCTRL.GAU -- CESSNA_182.CONTROLS: the throttle, propeller and mixture levers */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3ab8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3638u
#define GAUGE_MOUSE 0x3640u
#define GAUGE_MOUSE_ARGUMENT 0x3624u


/* A control lever's mouse callback (0x1020, 0x10a0, 0x1120, one per event): the lever's height
 * (+8 of the rectangle +0x3624 points at) times the first constant, parked as a double; the
 * height times the second, through _ftol, less the point's y, plus the parked one through
 * _ftol, times 0x4000, over the parked height, through _ftol, kept to [0, 0x3fff], sent with
 * the event (PANELS +0x90). Returns 0. */
static uint32_t cessctrl_lever(uint8_t *point, uint32_t event)
{
    long double height = (long double)PI32((uintptr_t)GD32(0x3624), 8);
    double scaled = (double)(height * GR64(0x2000));
    uint32_t position = (uint32_t)gauge_ftol(height * GR64(0x2008)) - P32(point, 4);
    int32_t lever;
    position = (position + (uint32_t)gauge_ftol(scaled)) << 14;
    lever = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)position / scaled);
    if (lever > 0x3fff) lever = 0x3fff;
    if (lever < 0) lever = 0;
    GAUGE_PANELS_CALL(0x90, 2)(event, (uint32_t)lever);
    return 0;
}

/* 0x1020: the lever for event 0x100a1 */
static uint32_t __stdcall cessctrl_1020(uint8_t *point, uint32_t flags)
{
    (void)flags;
    return cessctrl_lever(point, 0x100a1);
}

/* 0x10a0: the lever for event 0x100e7 */
static uint32_t __stdcall cessctrl_10a0(uint8_t *point, uint32_t flags)
{
    (void)flags;
    return cessctrl_lever(point, 0x100e7);
}

/* 0x1120: the lever for event 0x100ed */
static uint32_t __stdcall cessctrl_1120(uint8_t *point, uint32_t flags)
{
    (void)flags;
    return cessctrl_lever(point, 0x100ed);
}
GAUGE_INSTALL(cessctrl_install)   /* 0x11a0 */
GAUGE_INITIALIZE(cessctrl_initialize)   /* 0x11f0 */
GAUGE_UPDATE(cessctrl_update)   /* 0x1200 */
GAUGE_GENERATE(cessctrl_generate)   /* 0x1210 */
GAUGE_DRAW(cessctrl_draw)   /* 0x1230 */
GAUGE_KILL(cessctrl_kill)   /* 0x1240 */
GAUGE_MODULE(cessctrl_module)   /* 0x1270 */

GAUGE_QUERY(cessctrl_1280)   /* 0x1280 (the _ftol import thunk at 0x12a0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)cessctrl_1020}, {0x10a0, (void *)cessctrl_10a0}, {0x1120, (void *)cessctrl_1120}, {0x11a0, (void *)cessctrl_install}, {0x11f0, (void *)cessctrl_initialize}, {0x1200, (void *)cessctrl_update}, {0x1210, (void *)cessctrl_generate}, {0x1230, (void *)cessctrl_draw}, {0x1240, (void *)cessctrl_kill}, {0x1270, (void *)cessctrl_module}, {0x1280, (void *)cessctrl_1280})
