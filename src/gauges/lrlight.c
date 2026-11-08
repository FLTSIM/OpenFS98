/* LRLIGHT.GAU -- LEAR_45.LIGHT_SWITCHES: the lights' callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x36b0u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3410u
#define GAUGE_MOUSE 0x3418u
#define GAUGE_MOUSE_ARGUMENT 0x33fcu


/* 0x1020: the variable offset */
static long double __stdcall lrlight_1020(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2000);
}
GAUGE_INSTALL(lrlight_install)   /* 0x1030 */
GAUGE_INITIALIZE(lrlight_initialize)   /* 0x1080 */
GAUGE_UPDATE(lrlight_update)   /* 0x1090 */
GAUGE_GENERATE(lrlight_generate)   /* 0x10a0 */
GAUGE_DRAW(lrlight_draw)   /* 0x10c0 */
GAUGE_KILL(lrlight_kill)   /* 0x10d0 */
GAUGE_MODULE(lrlight_module)   /* 0x1100 */
GAUGE_QUERY(lrlight_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)lrlight_1020}, {0x1030, (void *)lrlight_install}, {0x1080, (void *)lrlight_initialize}, {0x1090, (void *)lrlight_update}, {0x10a0, (void *)lrlight_generate}, {0x10c0, (void *)lrlight_draw}, {0x10d0, (void *)lrlight_kill}, {0x1100, (void *)lrlight_module}, {0x1110, (void *)lrlight_query})
