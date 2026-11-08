/* BELLTRNS.GAU -- BELL_206B.TRANSMISSION: the transmission needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34e0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x33a0u
#define GAUGE_MOUSE 0x33a8u
#define GAUGE_MOUSE_ARGUMENT 0x338cu


/* 0x1020: the needle: the value scaled */
static long double __stdcall belltrns_1020(double value)
{
    return (long double)value * GR64(0x2000);
}

/* 0x1030: the needle: the value scaled */
static long double __stdcall belltrns_1030(double value)
{
    return (long double)value * GR64(0x2000);
}
GAUGE_INSTALL(belltrns_install)   /* 0x1040 */
GAUGE_INITIALIZE(belltrns_initialize)   /* 0x1090 */
GAUGE_UPDATE(belltrns_update)   /* 0x10a0 */
GAUGE_GENERATE(belltrns_generate)   /* 0x10b0 */
GAUGE_DRAW(belltrns_draw)   /* 0x10d0 */
GAUGE_KILL(belltrns_kill)   /* 0x10e0 */
GAUGE_MODULE(belltrns_module)   /* 0x1110 */
GAUGE_QUERY(belltrns_query)   /* 0x1120 */

GAUGE_ROUTINES({0x1020, (void *)belltrns_1020}, {0x1030, (void *)belltrns_1030}, {0x1040, (void *)belltrns_install}, {0x1090, (void *)belltrns_initialize}, {0x10a0, (void *)belltrns_update}, {0x10b0, (void *)belltrns_generate}, {0x10d0, (void *)belltrns_draw}, {0x10e0, (void *)belltrns_kill}, {0x1110, (void *)belltrns_module}, {0x1120, (void *)belltrns_query})
