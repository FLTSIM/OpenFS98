/* 737START.GAU -- 737-400.ENGINE-START: the starter switches */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3420u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x31d8u
#define GAUGE_MOUSE 0x31e0u
#define GAUGE_MOUSE_ARGUMENT 0x31c4u


/* A starter switch's mouse callback (0x1020..0x1350: two engines, three positions): unless the
 * engine's switch (its state through _ftol) is at the position already, events 0x10012, the
 * engine's (0x10002 or 0x10003) and 0x10024 (PANELS +0x90 with 0), then by the state read again
 * the step's event once or twice. Returns 0. */
struct g737start_step { uint32_t event, count; };
static uint32_t g737start_switch(uint32_t state, uint32_t engine, uint32_t target, const struct g737start_step *steps)
{
    uint32_t now = (uint32_t)gauge_ftol(GD64(state)), at;
    if (now == target) return 0;
    GAUGE_PANELS_CALL(0x90, 2)(0x10012, 0);
    GAUGE_PANELS_CALL(0x90, 2)(engine, 0);
    GAUGE_PANELS_CALL(0x90, 2)(0x10024, 0);
    now = (uint32_t)gauge_ftol(GD64(state));
    if (now < 3)
        for (at = 0; at < steps[now].count; ++at) GAUGE_PANELS_CALL(0x90, 2)(steps[now].event, 0);
    return 0;
}
/* from the state the switch is in (0, 1, 2) to each position: the step's event and count */
static const struct g737start_step g737start_to[3][3] = {
    {{0, 0}, {0x1000c, 1}, {0x1000d, 1}},
    {{0x1000d, 1}, {0, 0}, {0x1000d, 2}},
    {{0x1000c, 1}, {0x1000c, 2}, {0, 0}},
};

/* 0x1020: engine 1's switch to position 0 */
static uint32_t __stdcall g737start_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return g737start_switch(0x30b0, 0x10002, 0, g737start_to[0]);
}

/* 0x10b0: engine 1's switch to position 1 */
static uint32_t __stdcall g737start_10b0(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return g737start_switch(0x30b0, 0x10002, 1, g737start_to[1]);
}

/* 0x1160: engine 1's switch to position 2 */
static uint32_t __stdcall g737start_1160(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return g737start_switch(0x30b0, 0x10002, 2, g737start_to[2]);
}

/* 0x1210: engine 2's switch to position 0 */
static uint32_t __stdcall g737start_1210(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return g737start_switch(0x3138, 0x10003, 0, g737start_to[0]);
}

/* 0x12a0: engine 2's switch to position 1 */
static uint32_t __stdcall g737start_12a0(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return g737start_switch(0x3138, 0x10003, 1, g737start_to[1]);
}

/* 0x1350: engine 2's switch to position 2 */
static uint32_t __stdcall g737start_1350(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return g737start_switch(0x3138, 0x10003, 2, g737start_to[2]);
}

/* 0x1400: a switch's state, the variable's integer */
static long double __stdcall g737start_1400(uint8_t *element)
{
    return (long double)PI32(element, 0xc);
}
GAUGE_INSTALL(g737start_install)   /* 0x1410 */
GAUGE_INITIALIZE(g737start_initialize)   /* 0x1460 */
GAUGE_UPDATE(g737start_update)   /* 0x1470 */
GAUGE_GENERATE(g737start_generate)   /* 0x1480 */
GAUGE_DRAW(g737start_draw)   /* 0x14a0 */
GAUGE_KILL(g737start_kill)   /* 0x14b0 */
GAUGE_MODULE(g737start_module)   /* 0x14e0 */

GAUGE_QUERY(g737start_14f0)   /* 0x14f0 (the _ftol import thunk at 0x1510 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737start_1020}, {0x10b0, (void *)g737start_10b0}, {0x1160, (void *)g737start_1160}, {0x1210, (void *)g737start_1210}, {0x12a0, (void *)g737start_12a0}, {0x1350, (void *)g737start_1350}, {0x1400, (void *)g737start_1400}, {0x1410, (void *)g737start_install}, {0x1460, (void *)g737start_initialize}, {0x1470, (void *)g737start_update}, {0x1480, (void *)g737start_generate}, {0x14a0, (void *)g737start_draw}, {0x14b0, (void *)g737start_kill}, {0x14e0, (void *)g737start_module}, {0x14f0, (void *)g737start_14f0})
