/* BELLTEMP.GAU -- BELL_206B.TEMPERATURE: the temperature light and the outside air temperature as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3330u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32a0u
#define GAUGE_MOUSE 0x32a8u
#define GAUGE_MOUSE_ARGUMENT 0x328cu


/* 0x1020: the light: the variable at the first constant (equal, or unordered) that constant,
 * otherwise the second */
static long double __stdcall belltemp_1020(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C3)) return GR64(0x2000);
    return GR64(0x2008);
}

/* 0x1050: the temperature as text: the value scaled (parked as a double), and with the
 * units switch (+0x30e4) at the first constant (equal, or unordered) converted; kept to
 * [-99, 999] (above the fourth constant, ordered, 999; below the fifth or unordered, -99);
 * printed through _ftol into the element's buffer (+0x3138) with the format at +0x332c;
 * returns the temperature */
static long double __stdcall belltemp_1050(double value)
{
    uint16_t units = gauge_fcom(GD64(0x30e4), GR64(0x2000));
    double temperature = (double)((long double)value * GR64(0x2010));
    if (units & GAUGE_C3) temperature = (double)((long double)GR64(0x2020) - (long double)temperature * GR64(0x2018));
    if (!GAUGE_TEST(temperature, GR64(0x2028), GAUGE_C0C3)) temperature = 999.0;
    else if (GAUGE_TEST(temperature, GR64(0x2030), GAUGE_C0)) temperature = -99.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3138), (const char *)GAUGE_D(0x332c), (uint32_t)gauge_ftol(temperature));
    return temperature;
}
GAUGE_INSTALL(belltemp_install)   /* 0x10f0 */
GAUGE_INITIALIZE(belltemp_initialize)   /* 0x1140 */
GAUGE_UPDATE(belltemp_update)   /* 0x1150 */
GAUGE_GENERATE(belltemp_generate)   /* 0x1160 */
GAUGE_DRAW(belltemp_draw)   /* 0x1180 */
GAUGE_KILL(belltemp_kill)   /* 0x1190 */
GAUGE_MODULE(belltemp_module)   /* 0x11c0 */

GAUGE_QUERY(belltemp_11d0)   /* 0x11d0 (the _ftol import thunk at 0x11f0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)belltemp_1020}, {0x1050, (void *)belltemp_1050}, {0x10f0, (void *)belltemp_install}, {0x1140, (void *)belltemp_initialize}, {0x1150, (void *)belltemp_update}, {0x1160, (void *)belltemp_generate}, {0x1180, (void *)belltemp_draw}, {0x1190, (void *)belltemp_kill}, {0x11c0, (void *)belltemp_module}, {0x11d0, (void *)belltemp_11d0})
