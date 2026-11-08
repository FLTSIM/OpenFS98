/* BELLTURB.GAU -- BELL_206B.TURBINE_TEMPERATURE: the turbine temperature needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32f8u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3268u
#define GAUGE_MOUSE 0x3270u
#define GAUGE_MOUSE_ARGUMENT 0x3254u


/* 0x1020: the needle: the value scaled */
static long double __stdcall bellturb_1020(double value)
{
    return (long double)value * GR64(0x2000);
}
GAUGE_INSTALL(bellturb_install)   /* 0x1030 */
GAUGE_INITIALIZE(bellturb_initialize)   /* 0x1080 */
GAUGE_UPDATE(bellturb_update)   /* 0x1090 */
GAUGE_GENERATE(bellturb_generate)   /* 0x10a0 */
GAUGE_DRAW(bellturb_draw)   /* 0x10c0 */
GAUGE_KILL(bellturb_kill)   /* 0x10d0 */
GAUGE_MODULE(bellturb_module)   /* 0x1100 */
GAUGE_QUERY(bellturb_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)bellturb_1020}, {0x1030, (void *)bellturb_install}, {0x1080, (void *)bellturb_initialize}, {0x1090, (void *)bellturb_update}, {0x10a0, (void *)bellturb_generate}, {0x10c0, (void *)bellturb_draw}, {0x10d0, (void *)bellturb_kill}, {0x1100, (void *)bellturb_module}, {0x1110, (void *)bellturb_query})
