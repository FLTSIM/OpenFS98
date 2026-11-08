/* CESSASPD.GAU -- CESSNA_182.AIRSPEED: the airspeed needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3558u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3418u
#define GAUGE_MOUSE 0x3420u
#define GAUGE_MOUSE_ARGUMENT 0x3404u


/* 0x1020: the needle: clamped */
static long double __stdcall cessaspd_1020(uint8_t *element)
{
    return gauge_clamp(PF64(element, 0xc), GR64(0x2000), GR64(0x2008));
}

/* 0x1060: the needle: clamped */
static long double __stdcall cessaspd_1060(double value)
{
    return gauge_clamp(value, GR64(0x2010), GR64(0x2008));
}
GAUGE_INSTALL(cessaspd_install)   /* 0x10a0 */
GAUGE_INITIALIZE(cessaspd_initialize)   /* 0x10f0 */
GAUGE_UPDATE(cessaspd_update)   /* 0x1100 */
GAUGE_GENERATE(cessaspd_generate)   /* 0x1110 */
GAUGE_DRAW(cessaspd_draw)   /* 0x1130 */
GAUGE_KILL(cessaspd_kill)   /* 0x1140 */
GAUGE_MODULE(cessaspd_module)   /* 0x1170 */
GAUGE_QUERY(cessaspd_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)cessaspd_1020}, {0x1060, (void *)cessaspd_1060}, {0x10a0, (void *)cessaspd_install}, {0x10f0, (void *)cessaspd_initialize}, {0x1100, (void *)cessaspd_update}, {0x1110, (void *)cessaspd_generate}, {0x1130, (void *)cessaspd_draw}, {0x1140, (void *)cessaspd_kill}, {0x1170, (void *)cessaspd_module}, {0x1180, (void *)cessaspd_query})
