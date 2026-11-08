/* 737OIL.GAU -- 737-400.OIL: the engines' oil readouts and needles */
#include "gauge.h"

#define GAUGE_IMPORTS 0x4540u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x4238u
#define GAUGE_MOUSE 0x4240u
#define GAUGE_MOUSE_ARGUMENT 0x4224u


/* 0x1020: a readout: +0x3240 scaled, through _ftol, into the element's buffer (+0x32b0) with the
 * format at +0x452c; returns the value */
static long double __stdcall g737oil_1020(double value)
{
    
    wsprintfA((char *)(uintptr_t)GD32(0x32b0), (const char *)GAUGE_D(0x452c), (uint32_t)gauge_ftol((long double)GD64(0x3240) * GR64(0x2000)));
    return value;
}

/* 0x1050: a readout: +0x3378 scaled, through _ftol, into the element's buffer (+0x33e8) with the
 * format at +0x452c; returns the value */
static long double __stdcall g737oil_1050(double value)
{
    
    wsprintfA((char *)(uintptr_t)GD32(0x33e8), (const char *)GAUGE_D(0x452c), (uint32_t)gauge_ftol((long double)GD64(0x3378) * GR64(0x2000)));
    return value;
}

/* 0x1080: a readout: +0x34b0 scaled, through _ftol, into the element's buffer (+0x3520) with the
 * format at +0x452c; returns the value */
static long double __stdcall g737oil_1080(double value)
{
    
    wsprintfA((char *)(uintptr_t)GD32(0x3520), (const char *)GAUGE_D(0x452c), (uint32_t)gauge_ftol((long double)GD64(0x34b0) * GR64(0x2000)));
    return value;
}

/* 0x10b0: a readout: +0x35e8 scaled, through _ftol, into the element's buffer (+0x3658) with the
 * format at +0x452c; returns the value */
static long double __stdcall g737oil_10b0(double value)
{
    
    wsprintfA((char *)(uintptr_t)GD32(0x3658), (const char *)GAUGE_D(0x452c), (uint32_t)gauge_ftol((long double)GD64(0x35e8) * GR64(0x2000)));
    return value;
}

/* 0x10e0: a readout: +0x3720 scaled, through _ftol, into the element's buffer (+0x3790) with the
 * format at +0x4534; returns the source */
static long double __stdcall g737oil_10e0(double value)
{
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3790), (const char *)GAUGE_D(0x4534), (uint32_t)gauge_ftol((long double)GD64(0x3720) * GR64(0x2008)));
    return GD64(0x3720);
}

/* 0x1110: a needle: the variable scaled, limited */
static long double __stdcall g737oil_1110(uint8_t *element)
{
    return gauge_limit((long double)PF64(element, 0xc) * GR64(0x2010), GR64(0x2018), GR64(0x2020));
}

/* 0x1150: a needle: the variable scaled, limited */
static long double __stdcall g737oil_1150(uint8_t *element)
{
    return gauge_limit((long double)PF64(element, 0xc) * GR64(0x2028), GR64(0x2030), GR64(0x2020));
}
GAUGE_INSTALL(g737oil_install)   /* 0x1190 */
GAUGE_INITIALIZE(g737oil_initialize)   /* 0x11e0 */
GAUGE_UPDATE(g737oil_update)   /* 0x11f0 */
GAUGE_GENERATE(g737oil_generate)   /* 0x1200 */
GAUGE_DRAW(g737oil_draw)   /* 0x1220 */
GAUGE_KILL(g737oil_kill)   /* 0x1230 */
GAUGE_MODULE(g737oil_module)   /* 0x1260 */

GAUGE_QUERY(g737oil_1270)   /* 0x1270 (the _ftol import thunk at 0x1290 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737oil_1020}, {0x1050, (void *)g737oil_1050}, {0x1080, (void *)g737oil_1080}, {0x10b0, (void *)g737oil_10b0}, {0x10e0, (void *)g737oil_10e0}, {0x1110, (void *)g737oil_1110}, {0x1150, (void *)g737oil_1150}, {0x1190, (void *)g737oil_install}, {0x11e0, (void *)g737oil_initialize}, {0x11f0, (void *)g737oil_update}, {0x1200, (void *)g737oil_generate}, {0x1220, (void *)g737oil_draw}, {0x1230, (void *)g737oil_kill}, {0x1260, (void *)g737oil_module}, {0x1270, (void *)g737oil_1270})
