/* SOPMAGS.GAU -- SOPWITH.MAGNETOS: the magneto switch */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32f8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3210u
#define GAUGE_MOUSE 0x3218u
#define GAUGE_MOUSE_ARGUMENT 0x31fcu


/* 0x1020: the switch's left click: the magnetos' variable (+0x3080) looked up (PANELS +0xa0);
 * by its state (+0x308c) the step's event twice (0 and 1: 0x10063, 2 and 3: 0x10062; PANELS
 * +0x90 with 0). Returns 1. */
static uint32_t __stdcall sopmags_1020(uint32_t point, uint32_t flags)
{
    uint32_t state;
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3080));
    state = GD32(0x308c);
    if (state == 0 || state == 1) {
        GAUGE_PANELS_CALL(0x90, 2)(0x10063, 0);
        GAUGE_PANELS_CALL(0x90, 2)(0x10063, 0);
    } else if (state == 2 || state == 3) {
        GAUGE_PANELS_CALL(0x90, 2)(0x10062, 0);
        GAUGE_PANELS_CALL(0x90, 2)(0x10062, 0);
    }
    return 1;
}

/* 0x1100: the switch's right click: the step's event once (0 and 2: 0x10063, 1 and 3:
 * 0x10062). Returns 1. */
static uint32_t __stdcall sopmags_1100(uint32_t point, uint32_t flags)
{
    uint32_t state;
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3080));
    state = GD32(0x308c);
    if (state == 0 || state == 2) GAUGE_PANELS_CALL(0x90, 2)(0x10063, 0);
    else if (state == 1 || state == 3) GAUGE_PANELS_CALL(0x90, 2)(0x10062, 0);
    return 1;
}

/* 0x11a0, header +0x10: install: the elements, the variable at +0x3080 initialised (PANELS
 * +0x98), then the mouse rectangles */
static void __stdcall sopmags_11a0(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3080));
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(sopmags_initialize)   /* 0x1200 */
GAUGE_UPDATE(sopmags_update)   /* 0x1210 */
GAUGE_GENERATE(sopmags_generate)   /* 0x1220 */
GAUGE_DRAW(sopmags_draw)   /* 0x1240 */
GAUGE_KILL(sopmags_kill)   /* 0x1250 */
GAUGE_MODULE(sopmags_module)   /* 0x1280 */
GAUGE_QUERY(sopmags_query)   /* 0x1290 */

GAUGE_ROUTINES({0x1020, (void *)sopmags_1020}, {0x1100, (void *)sopmags_1100}, {0x11a0, (void *)sopmags_11a0}, {0x1200, (void *)sopmags_initialize}, {0x1210, (void *)sopmags_update}, {0x1220, (void *)sopmags_generate}, {0x1240, (void *)sopmags_draw}, {0x1250, (void *)sopmags_kill}, {0x1280, (void *)sopmags_module}, {0x1290, (void *)sopmags_query})
