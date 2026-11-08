/* 737FUELQ.GAU -- 737-400.FUEL-QUANTITY: the fuel quantities as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3708u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3678u
#define GAUGE_MOUSE 0x3680u
#define GAUGE_MOUSE_ARGUMENT 0x3664u


/* 0x1020: a fuel total: tank +0x3194 times +0x30d4 and tank +0x3178 times +0x30b4, each times the first
 * constant and +0x30f4 (the second product parked as a double on the way), summed, times the
 * second constant, through _ftol, into the element's buffer (+0x31e8) with the format at +0x3704;
 * returns tank +0x3178 */
static long double __stdcall g737fuelq_1020(double value)
{
    double parked;
    long double second = ((long double)GD64(0x3178) * GD64(0x30b4)) * GR64(0x2000);
    long double first = (((long double)GD64(0x3194) * GD64(0x30d4)) * GR64(0x2000)) * GD64(0x30f4);
    (void)value;
    gauge_fst(&parked, second);
    second = second * GD64(0x30f4);
    wsprintfA((char *)(uintptr_t)GD32(0x31e8), (const char *)GAUGE_D(0x3704), (uint32_t)gauge_ftol((first + second) * GR64(0x2008)));
    return GD64(0x3178);
}

/* 0x1090: a fuel total in two parts: tank +0x32cc times +0x30d4 and tank +0x32b0 times +0x30b4, each times
 * the first constant and +0x30f4 (parked as doubles on the way, the second's added as parked);
 * the sum times the third constant and times the second, each through _ftol, printed as
 * -(10 * the second) - the first into the element's buffer (+0x3320) with the format at +0x3704;
 * returns tank +0x32b0 */
static long double __stdcall g737fuelq_1090(double value)
{
    double parked;
    long double first = ((long double)GD64(0x32cc) * GD64(0x30d4)) * GR64(0x2000);
    long double second = ((long double)GD64(0x32b0) * GD64(0x30b4)) * GR64(0x2000);
    long double sum;
    uint32_t units, tens, shown;
    (void)value;
    first = first * GD64(0x30f4);
    gauge_fst(&parked, second);
    second = second * GD64(0x30f4);
    gauge_fst(&parked, first);
    gauge_fst(&parked, second);     /* the fstp the fadd reads back */
    sum = first + parked;
    units = (uint32_t)gauge_ftol(sum * GR64(0x2010));
    tens = (uint32_t)gauge_ftol(sum * GR64(0x2008));
    shown = ((((0u - tens) << 2) - tens) << 1) - units;
    wsprintfA((char *)(uintptr_t)GD32(0x3320), (const char *)GAUGE_D(0x3704), shown);
    return GD64(0x32b0);
}

/* 0x1130: a fuel total: tank +0x3404 times +0x3094 and tank +0x33e8 times +0x3074, each times the first
 * constant and +0x30f4 (the second product parked as a double on the way), summed, times the
 * second constant, through _ftol, into the element's buffer (+0x3458) with the format at +0x3704;
 * returns tank +0x33e8 */
static long double __stdcall g737fuelq_1130(double value)
{
    double parked;
    long double second = ((long double)GD64(0x33e8) * GD64(0x3074)) * GR64(0x2000);
    long double first = (((long double)GD64(0x3404) * GD64(0x3094)) * GR64(0x2000)) * GD64(0x30f4);
    (void)value;
    gauge_fst(&parked, second);
    second = second * GD64(0x30f4);
    wsprintfA((char *)(uintptr_t)GD32(0x3458), (const char *)GAUGE_D(0x3704), (uint32_t)gauge_ftol((first + second) * GR64(0x2008)));
    return GD64(0x33e8);
}

/* 0x11a0: a fuel total in two parts: tank +0x353c times +0x3094 and tank +0x3520 times +0x3074, each times
 * the first constant and +0x30f4 (parked as doubles on the way, the second's added as parked);
 * the sum times the third constant and times the second, each through _ftol, printed as
 * -(10 * the second) - the first into the element's buffer (+0x3590) with the format at +0x3704;
 * returns tank +0x3520 */
static long double __stdcall g737fuelq_11a0(double value)
{
    double parked;
    long double first = ((long double)GD64(0x353c) * GD64(0x3094)) * GR64(0x2000);
    long double second = ((long double)GD64(0x3520) * GD64(0x3074)) * GR64(0x2000);
    long double sum;
    uint32_t units, tens, shown;
    (void)value;
    first = first * GD64(0x30f4);
    gauge_fst(&parked, second);
    second = second * GD64(0x30f4);
    gauge_fst(&parked, first);
    gauge_fst(&parked, second);     /* the fstp the fadd reads back */
    sum = first + parked;
    units = (uint32_t)gauge_ftol(sum * GR64(0x2010));
    tens = (uint32_t)gauge_ftol(sum * GR64(0x2008));
    shown = ((((0u - tens) << 2) - tens) << 1) - units;
    wsprintfA((char *)(uintptr_t)GD32(0x3590), (const char *)GAUGE_D(0x3704), shown);
    return GD64(0x3520);
}

/* 0x1240, header +0x10: install: the five variables initialised (PANELS +0x98) and looked up
 * (+0xa0), then the standard install */
static void __stdcall g737fuelq_1240(uint32_t context)
{
    static const uint16_t variables[5] = {0x3068, 0x3088, 0x30a8, 0x30c8, 0x30e8};
    unsigned at;
    for (at = 0; at < 5; ++at) GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(variables[at]));
    for (at = 0; at < 5; ++at) GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(variables[at]));
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
}
GAUGE_INITIALIZE(g737fuelq_initialize)   /* 0x1340 */
GAUGE_UPDATE(g737fuelq_update)   /* 0x1350 */
GAUGE_GENERATE(g737fuelq_generate)   /* 0x1360 */
GAUGE_DRAW(g737fuelq_draw)   /* 0x1380 */
GAUGE_KILL(g737fuelq_kill)   /* 0x1390 */
GAUGE_MODULE(g737fuelq_module)   /* 0x13c0 */

GAUGE_QUERY(g737fuelq_13d0)   /* 0x13d0 (the _ftol import thunk at 0x13f0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737fuelq_1020}, {0x1090, (void *)g737fuelq_1090}, {0x1130, (void *)g737fuelq_1130}, {0x11a0, (void *)g737fuelq_11a0}, {0x1240, (void *)g737fuelq_1240}, {0x1340, (void *)g737fuelq_initialize}, {0x1350, (void *)g737fuelq_update}, {0x1360, (void *)g737fuelq_generate}, {0x1380, (void *)g737fuelq_draw}, {0x1390, (void *)g737fuelq_kill}, {0x13c0, (void *)g737fuelq_module}, {0x13d0, (void *)g737fuelq_13d0})
