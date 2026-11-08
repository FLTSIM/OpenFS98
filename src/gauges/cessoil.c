/* CESSOIL.GAU -- CESSNA_182.OIL: the oil needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3560u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3420u
#define GAUGE_MOUSE 0x3428u
#define GAUGE_MOUSE_ARGUMENT 0x340cu


/* 0x1020: the needle: the value scaled and offset, limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall cessoil_1020(double value)
{
    return gauge_limit(((long double)value * GR64(0x2000)) - GR64(0x2008), GR64(0x2010), GR64(0x2018));
}

/* 0x1060: the needle: the value scaled, limited (above the high, ordered, the high at once; below the low or unordered, the low) */
static long double __stdcall cessoil_1060(double value)
{
    return gauge_limit((long double)value * GR64(0x2020), GR64(0x2028), GR64(0x2030));
}
GAUGE_INSTALL(cessoil_install)   /* 0x10a0 */
GAUGE_INITIALIZE(cessoil_initialize)   /* 0x10f0 */
GAUGE_UPDATE(cessoil_update)   /* 0x1100 */
GAUGE_GENERATE(cessoil_generate)   /* 0x1110 */
GAUGE_DRAW(cessoil_draw)   /* 0x1130 */
GAUGE_KILL(cessoil_kill)   /* 0x1140 */
GAUGE_MODULE(cessoil_module)   /* 0x1170 */
GAUGE_QUERY(cessoil_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)cessoil_1020}, {0x1060, (void *)cessoil_1060}, {0x10a0, (void *)cessoil_install}, {0x10f0, (void *)cessoil_initialize}, {0x1100, (void *)cessoil_update}, {0x1110, (void *)cessoil_generate}, {0x1130, (void *)cessoil_draw}, {0x1140, (void *)cessoil_kill}, {0x1170, (void *)cessoil_module}, {0x1180, (void *)cessoil_query})
