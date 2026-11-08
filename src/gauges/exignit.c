/* EXIGNIT.GAU -- EXTRA-300.IGNITION: the ignition switch's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3230u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3148u
#define GAUGE_MOUSE 0x3150u
#define GAUGE_MOUSE_ARGUMENT 0x3134u


/* 0x1020: the switch's state, the variable's integer */
static long double __stdcall exignit_1020(uint8_t *element)
{
    return (long double)PI32(element, 0xc);
}
GAUGE_INSTALL(exignit_install)   /* 0x1030 */
GAUGE_INITIALIZE(exignit_initialize)   /* 0x1080 */
GAUGE_UPDATE(exignit_update)   /* 0x1090 */
GAUGE_GENERATE(exignit_generate)   /* 0x10a0 */
GAUGE_DRAW(exignit_draw)   /* 0x10c0 */
GAUGE_KILL(exignit_kill)   /* 0x10d0 */
GAUGE_MODULE(exignit_module)   /* 0x1100 */
GAUGE_QUERY(exignit_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)exignit_1020}, {0x1030, (void *)exignit_install}, {0x1080, (void *)exignit_initialize}, {0x1090, (void *)exignit_update}, {0x10a0, (void *)exignit_generate}, {0x10c0, (void *)exignit_draw}, {0x10d0, (void *)exignit_kill}, {0x1100, (void *)exignit_module}, {0x1110, (void *)exignit_query})
