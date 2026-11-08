/* SOPFUEL.GAU -- SOPWITH.FUEL: the fuel gauge, reading two variables */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3280u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x31f0u
#define GAUGE_MOUSE 0x31f8u
#define GAUGE_MOUSE_ARGUMENT 0x31dcu


/* 0x1020: the needle: the two tanks' variables (+0x3078, +0x3098) looked up (PANELS +0xa0),
 * their values (+0x30a4, +0x3084) summed through three scales, offset */
static long double __stdcall sopfuel_1020(uint8_t *element)
{
    (void)element;
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3078));
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3098));
    return (((((long double)GD64(0x30a4) + GD64(0x3084)) * GR64(0x2000)) * GR64(0x2008)) * GR64(0x2010)) - GR64(0x2018);
}

/* 0x1070, header +0x10: install: the variables (+0x3078, +0x3098) initialised
 * (PANELS +0x98), then the standard install */
static void __stdcall sopfuel_1070(uint32_t context)
{
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3078));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3098));
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(sopfuel_initialize)   /* 0x10e0 */

/* 0x10f0, header +0x18: update: PANELS +0x48 with the element at +0x30b8, not the list */
static void __stdcall sopfuel_10f0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x30b8));
}
GAUGE_GENERATE(sopfuel_generate)   /* 0x1100 */
GAUGE_DRAW(sopfuel_draw)   /* 0x1120 */
GAUGE_KILL(sopfuel_kill)   /* 0x1130 */
GAUGE_MODULE(sopfuel_module)   /* 0x1160 */
GAUGE_QUERY(sopfuel_query)   /* 0x1170 */

GAUGE_ROUTINES({0x1020, (void *)sopfuel_1020}, {0x1070, (void *)sopfuel_1070}, {0x10e0, (void *)sopfuel_initialize}, {0x10f0, (void *)sopfuel_10f0}, {0x1100, (void *)sopfuel_generate}, {0x1120, (void *)sopfuel_draw}, {0x1130, (void *)sopfuel_kill}, {0x1160, (void *)sopfuel_module}, {0x1170, (void *)sopfuel_query})
