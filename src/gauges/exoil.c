/* EXOIL.GAU -- EXTRA-300.OIL-PRESSURE-TEMPATURE: the oil needles' callbacks */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34e0u
#define GAUGE_HEADER 0x3028u
#define GAUGE_ELEMENTS 0x33a0u
#define GAUGE_MOUSE 0x33a8u
#define GAUGE_MOUSE_ARGUMENT 0x338cu


/* 0x1020: the needle: the variable through two scales, limited */
static long double __stdcall exoil_1020(uint8_t *element)
{
    return gauge_limit(((long double)PF64(element, 0xc) * GR64(0x2000)) * GR64(0x2008), GR64(0x2010), GR64(0x2018));
}

/* 0x1070: the needle: a constant less the variable through three scales, limited */
static long double __stdcall exoil_1070(uint8_t *element)
{
    return gauge_limit((long double)GR64(0x2030) - (((long double)PF64(element, 0xc) * GR64(0x2000)) * GR64(0x2020)) * GR64(0x2028),
                       GR64(0x2038), GR64(0x2018));
}
GAUGE_INSTALL(exoil_install)   /* 0x10c0 */
GAUGE_INITIALIZE(exoil_initialize)   /* 0x1110 */
GAUGE_UPDATE(exoil_update)   /* 0x1120 */
GAUGE_GENERATE(exoil_generate)   /* 0x1130 */
GAUGE_DRAW(exoil_draw)   /* 0x1150 */
GAUGE_KILL(exoil_kill)   /* 0x1160 */
GAUGE_MODULE(exoil_module)   /* 0x1190 */
GAUGE_QUERY(exoil_query)   /* 0x11a0 */

GAUGE_ROUTINES({0x1020, (void *)exoil_1020}, {0x1070, (void *)exoil_1070}, {0x10c0, (void *)exoil_install}, {0x1110, (void *)exoil_initialize}, {0x1120, (void *)exoil_update}, {0x1130, (void *)exoil_generate}, {0x1150, (void *)exoil_draw}, {0x1160, (void *)exoil_kill}, {0x1190, (void *)exoil_module}, {0x11a0, (void *)exoil_query})
