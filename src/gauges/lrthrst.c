/* LRTHRST.GAU -- LEAR_45.THRUST_LEVERS: the thrust levers */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33d0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32c0u
#define GAUGE_MOUSE 0x32c8u
#define GAUGE_MOUSE_ARGUMENT 0x32acu


/* A thrust lever's mouse callback (0x1020, 0x10b0, 0x1140): the lever's height (+8 of the
 * rectangle +0x32ac points at) scaled; through _ftol, less the point's y, times 16390, over the
 * scaled height, through _ftol, at most 0x3fff (no lower bound); the events in order with 0,
 * then 0x100a1 with the position (PANELS +0x90). Returns 0. */
static uint32_t lrthrst_lever(uint8_t *point, const uint32_t *events, unsigned count)
{
    long double height = (long double)PI32((uintptr_t)GD32(0x32ac), 8) * GR64(0x2000);
    uint32_t travel = (uint32_t)gauge_ftol(height) - P32(point, 4);
    int32_t lever = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)(travel * 16390u) / height);
    unsigned at;
    if (lever > 0x3fff) lever = 0x3fff;
    for (at = 0; at < count; ++at) GAUGE_PANELS_CALL(0x90, 2)(events[at], 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x100a1, (uint32_t)lever);
    return 0;
}

/* 0x1020: a lever, after events 0x10012, 0x10002 */
static uint32_t __stdcall lrthrst_1020(uint8_t *point, uint32_t flags)
{
    static const uint32_t events[] = {0x10012, 0x10002};
    (void)flags;
    return lrthrst_lever(point, events, 2);
}

/* 0x10b0: a lever, after events 0x10012, 0x10003 */
static uint32_t __stdcall lrthrst_10b0(uint8_t *point, uint32_t flags)
{
    static const uint32_t events[] = {0x10012, 0x10003};
    (void)flags;
    return lrthrst_lever(point, events, 2);
}

/* 0x1140: a lever, after events 0x10012, 0x10002, 0x10003 */
static uint32_t __stdcall lrthrst_1140(uint8_t *point, uint32_t flags)
{
    static const uint32_t events[] = {0x10012, 0x10002, 0x10003};
    (void)flags;
    return lrthrst_lever(point, events, 3);
}
GAUGE_INSTALL(lrthrst_install)   /* 0x11e0 */
GAUGE_INITIALIZE(lrthrst_initialize)   /* 0x1230 */
GAUGE_UPDATE(lrthrst_update)   /* 0x1240 */
GAUGE_GENERATE(lrthrst_generate)   /* 0x1250 */
GAUGE_DRAW(lrthrst_draw)   /* 0x1270 */
GAUGE_KILL(lrthrst_kill)   /* 0x1280 */
GAUGE_MODULE(lrthrst_module)   /* 0x12b0 */

GAUGE_QUERY(lrthrst_12c0)   /* 0x12c0 (the _ftol import thunk at 0x12e0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)lrthrst_1020}, {0x10b0, (void *)lrthrst_10b0}, {0x1140, (void *)lrthrst_1140}, {0x11e0, (void *)lrthrst_install}, {0x1230, (void *)lrthrst_initialize}, {0x1240, (void *)lrthrst_update}, {0x1250, (void *)lrthrst_generate}, {0x1270, (void *)lrthrst_draw}, {0x1280, (void *)lrthrst_kill}, {0x12b0, (void *)lrthrst_module}, {0x12c0, (void *)lrthrst_12c0})
