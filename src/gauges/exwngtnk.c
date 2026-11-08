/* EXWNGTNK.GAU -- EXTRA-300.WING-TANK: the wing tank needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3288u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x31f8u
#define GAUGE_MOUSE 0x3200u
#define GAUGE_MOUSE_ARGUMENT 0x31e4u


/* 0x1020: the needle: the variable scaled */
static long double __stdcall exwngtnk_1020(uint8_t *element)
{
    return (long double)PF64(element, 0xc) * GR64(0x2000);
}
GAUGE_INSTALL(exwngtnk_install)   /* 0x1030 */
GAUGE_INITIALIZE(exwngtnk_initialize)   /* 0x1080 */
GAUGE_UPDATE(exwngtnk_update)   /* 0x1090 */
GAUGE_GENERATE(exwngtnk_generate)   /* 0x10a0 */
GAUGE_DRAW(exwngtnk_draw)   /* 0x10c0 */
GAUGE_KILL(exwngtnk_kill)   /* 0x10d0 */
GAUGE_MODULE(exwngtnk_module)   /* 0x1100 */
GAUGE_QUERY(exwngtnk_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)exwngtnk_1020}, {0x1030, (void *)exwngtnk_install}, {0x1080, (void *)exwngtnk_initialize}, {0x1090, (void *)exwngtnk_update}, {0x10a0, (void *)exwngtnk_generate}, {0x10c0, (void *)exwngtnk_draw}, {0x10d0, (void *)exwngtnk_kill}, {0x1100, (void *)exwngtnk_module}, {0x1110, (void *)exwngtnk_query})
