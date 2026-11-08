/* EXDG.GAU -- EXTRA-300.DIRECTIONAL-GYRO: the directional gyro's update hands PANELS its card */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3378u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3238u
#define GAUGE_MOUSE 0x3240u
#define GAUGE_MOUSE_ARGUMENT 0x3224u

GAUGE_INSTALL(exdg_install)   /* 0x1020 */
GAUGE_INITIALIZE(exdg_initialize)   /* 0x1070 */

/* 0x1080, header +0x18: update: PANELS +0x48 with the card element (+0x30a0), not the list */
static void __stdcall exdg_1080(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x30a0));
}
GAUGE_GENERATE(exdg_generate)   /* 0x1090 */
GAUGE_DRAW(exdg_draw)   /* 0x10b0 */
GAUGE_KILL(exdg_kill)   /* 0x10c0 */
GAUGE_MODULE(exdg_module)   /* 0x10f0 */
GAUGE_QUERY(exdg_query)   /* 0x1100 */

GAUGE_ROUTINES({0x1020, (void *)exdg_install}, {0x1070, (void *)exdg_initialize}, {0x1080, (void *)exdg_1080}, {0x1090, (void *)exdg_generate}, {0x10b0, (void *)exdg_draw}, {0x10c0, (void *)exdg_kill}, {0x10f0, (void *)exdg_module}, {0x1100, (void *)exdg_query})
