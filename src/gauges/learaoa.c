/* LEARAOA.GAU -- LEAR_45.AOA: the angle of attack needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3258u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x31c8u
#define GAUGE_MOUSE 0x31d0u
#define GAUGE_MOUSE_ARGUMENT 0x31b4u


/* 0x1020: the needle: the value as it is */
static long double __stdcall learaoa_1020(double value)
{
    return value;
}
GAUGE_INSTALL(learaoa_install)   /* 0x1030 */
GAUGE_INITIALIZE(learaoa_initialize)   /* 0x1080 */
GAUGE_UPDATE(learaoa_update)   /* 0x1090 */
GAUGE_GENERATE(learaoa_generate)   /* 0x10a0 */
GAUGE_DRAW(learaoa_draw)   /* 0x10c0 */
GAUGE_KILL(learaoa_kill)   /* 0x10d0 */
GAUGE_MODULE(learaoa_module)   /* 0x1100 */
GAUGE_QUERY(learaoa_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)learaoa_1020}, {0x1030, (void *)learaoa_install}, {0x1080, (void *)learaoa_initialize}, {0x1090, (void *)learaoa_update}, {0x10a0, (void *)learaoa_generate}, {0x10c0, (void *)learaoa_draw}, {0x10d0, (void *)learaoa_kill}, {0x1100, (void *)learaoa_module}, {0x1110, (void *)learaoa_query})
