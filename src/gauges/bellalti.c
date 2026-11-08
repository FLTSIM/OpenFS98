/* BELLALTI.GAU -- BELL_206B.ALTIMETER: the altimeter's window and needles */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3638u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x34f8u
#define GAUGE_MOUSE 0x3500u
#define GAUGE_MOUSE_ARGUMENT 0x34e4u


/* 0x1020: the Kollsman window: the variable less a constant, scaled */
static long double __stdcall bellalti_1020(uint8_t *element)
{
    return ((long double)PF64(element, 0xc) - GR64(0x2000)) * GR64(0x2008);
}

/* 0x1040: a needle: the value scaled, offset */
static long double __stdcall bellalti_1040(double value)
{
    return (long double)value * GR64(0x2010) - GR64(0x2018);
}

/* 0x1060: a needle: the value scaled, offset */
static long double __stdcall bellalti_1060(double value)
{
    return (long double)value * GR64(0x2020) - GR64(0x2018);
}

/* 0x1080: a needle: the value scaled, offset */
static long double __stdcall bellalti_1080(double value)
{
    return (long double)value * GR64(0x2028) - GR64(0x2018);
}
GAUGE_INSTALL(bellalti_install)   /* 0x10a0 */
GAUGE_INITIALIZE(bellalti_initialize)   /* 0x10f0 */
GAUGE_UPDATE(bellalti_update)   /* 0x1100 */
GAUGE_GENERATE(bellalti_generate)   /* 0x1110 */
GAUGE_DRAW(bellalti_draw)   /* 0x1130 */
GAUGE_KILL(bellalti_kill)   /* 0x1140 */
GAUGE_MODULE(bellalti_module)   /* 0x1170 */
GAUGE_QUERY(bellalti_query)   /* 0x1180 */

GAUGE_ROUTINES({0x1020, (void *)bellalti_1020}, {0x1040, (void *)bellalti_1040}, {0x1060, (void *)bellalti_1060}, {0x1080, (void *)bellalti_1080}, {0x10a0, (void *)bellalti_install}, {0x10f0, (void *)bellalti_initialize}, {0x1100, (void *)bellalti_update}, {0x1110, (void *)bellalti_generate}, {0x1130, (void *)bellalti_draw}, {0x1140, (void *)bellalti_kill}, {0x1170, (void *)bellalti_module}, {0x1180, (void *)bellalti_query})
