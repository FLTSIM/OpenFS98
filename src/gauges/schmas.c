/* SCHMAS.GAU -- SCHWEIZER.MASTER: the master switch: a constant and the update of its element */
#include "gauge.h"

#define GAUGE_IMPORTS 0x31d8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3148u
#define GAUGE_MOUSE 0x3150u
#define GAUGE_MOUSE_ARGUMENT 0x3134u


/* 0x1020: a constant */
static long double __stdcall schmas_1020(uint8_t *element)
{
    (void)element;
    return GR64(0x2000);
}
GAUGE_INSTALL(schmas_install)   /* 0x1030 */
GAUGE_INITIALIZE(schmas_initialize)   /* 0x1080 */

/* 0x1090, header +0x18: update: PANELS +0x48 with the element at +0x3068, not the list */
static void __stdcall schmas_1090(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3068));
}
GAUGE_GENERATE(schmas_generate)   /* 0x10a0 */
GAUGE_DRAW(schmas_draw)   /* 0x10c0 */
GAUGE_KILL(schmas_kill)   /* 0x10d0 */
GAUGE_MODULE(schmas_module)   /* 0x1100 */
GAUGE_QUERY(schmas_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)schmas_1020}, {0x1030, (void *)schmas_install}, {0x1080, (void *)schmas_initialize}, {0x1090, (void *)schmas_1090}, {0x10a0, (void *)schmas_generate}, {0x10c0, (void *)schmas_draw}, {0x10d0, (void *)schmas_kill}, {0x1100, (void *)schmas_module}, {0x1110, (void *)schmas_query})
