/* 737CNTRL.GAU -- 737-400.CONTROLS: the speed brake and the thrust levers */
#include "gauge.h"

#define GAUGE_IMPORTS 0x35d8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3368u
#define GAUGE_MOUSE 0x3370u
#define GAUGE_MOUSE_ARGUMENT 0x3354u


/* 0x1020: the speed brake's indicator: the variable offset */
static long double __stdcall g737cntrl_1020(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2000);
}

/* 0x1030: the speed brake's mouse callback (point, flags): the point's y times 16390 over the
 * lever's height (+8 of the rectangle +0x3354 points at) scaled, through _ftol, kept to [0,
 * 0x3fff]; outside (0xcdf, 0x202d) sent as event 0x100fa with the position (PANELS +0x90),
 * inside and with flag 0x20000000 event 0x1013d with 0; returns 0 */
static uint32_t __stdcall g737cntrl_1030(uint8_t *point, uint32_t flags)
{
    uint32_t y = P32(point, 4);
    int32_t position = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)(y * 16390u) /
                                                     ((long double)PI32((uintptr_t)GD32(0x3354), 8) * GR64(0x2008)));
    if (position > 0x3fff) position = 0x3fff;
    if (position < 0) position = 0;
    if (position > 0xcdf && position < 0x202d) {
        if (flags & 0x20000000u) GAUGE_PANELS_CALL(0x90, 2)(0x1013d, 0);
        return 0;
    }
    GAUGE_PANELS_CALL(0x90, 2)(0x100fa, (uint32_t)position);
    return 0;
}

/* A thrust lever's mouse callback (0x10c0, 0x1150, 0x11e0): the lever's height (+8 of the
 * rectangle +0x3354 points at) scaled; through _ftol, less the point's y, times 16390, over the
 * scaled height, through _ftol, at most 0x3fff (no lower bound); the events in order with 0,
 * then 0x100a1 with the position (PANELS +0x90). Returns 0. */
static uint32_t g737cntrl_lever(uint8_t *point, const uint32_t *events, unsigned count)
{
    long double height = (long double)PI32((uintptr_t)GD32(0x3354), 8) * GR64(0x2010);
    uint32_t travel = (uint32_t)gauge_ftol(height) - P32(point, 4);
    int32_t lever = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)(travel * 16390u) / height);
    unsigned at;
    if (lever > 0x3fff) lever = 0x3fff;
    for (at = 0; at < count; ++at) GAUGE_PANELS_CALL(0x90, 2)(events[at], 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x100a1, (uint32_t)lever);
    return 0;
}

/* 0x10c0: a lever, after events 0x10012, 0x10002 */
static uint32_t __stdcall g737cntrl_10c0(uint8_t *point, uint32_t flags)
{
    static const uint32_t events[] = {0x10012, 0x10002};
    (void)flags;
    return g737cntrl_lever(point, events, 2);
}

/* 0x1150: a lever, after events 0x10012, 0x10003 */
static uint32_t __stdcall g737cntrl_1150(uint8_t *point, uint32_t flags)
{
    static const uint32_t events[] = {0x10012, 0x10003};
    (void)flags;
    return g737cntrl_lever(point, events, 2);
}

/* 0x11e0: a lever, after events 0x10012, 0x10002, 0x10003 */
static uint32_t __stdcall g737cntrl_11e0(uint8_t *point, uint32_t flags)
{
    static const uint32_t events[] = {0x10012, 0x10002, 0x10003};
    (void)flags;
    return g737cntrl_lever(point, events, 3);
}
GAUGE_INSTALL(g737cntrl_install)   /* 0x1280 */
GAUGE_INITIALIZE(g737cntrl_initialize)   /* 0x12d0 */
GAUGE_UPDATE(g737cntrl_update)   /* 0x12e0 */
GAUGE_GENERATE(g737cntrl_generate)   /* 0x12f0 */
GAUGE_DRAW(g737cntrl_draw)   /* 0x1310 */
GAUGE_KILL(g737cntrl_kill)   /* 0x1320 */
GAUGE_MODULE(g737cntrl_module)   /* 0x1350 */

GAUGE_QUERY(g737cntrl_1360)   /* 0x1360 (the _ftol import thunk at 0x1380 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737cntrl_1020}, {0x1030, (void *)g737cntrl_1030}, {0x10c0, (void *)g737cntrl_10c0}, {0x1150, (void *)g737cntrl_1150}, {0x11e0, (void *)g737cntrl_11e0}, {0x1280, (void *)g737cntrl_install}, {0x12d0, (void *)g737cntrl_initialize}, {0x12e0, (void *)g737cntrl_update}, {0x12f0, (void *)g737cntrl_generate}, {0x1310, (void *)g737cntrl_draw}, {0x1320, (void *)g737cntrl_kill}, {0x1350, (void *)g737cntrl_module}, {0x1360, (void *)g737cntrl_1360})
