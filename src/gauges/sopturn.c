/* SOPTURN.GAU -- SOPWITH.TURN-COORDINATOR: the turn needle and the update of its element */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3218u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3188u
#define GAUGE_MOUSE 0x3190u
#define GAUGE_MOUSE_ARGUMENT 0x3174u


/* 0x1020: the needle: the rate (the integer at +8 of the record +0x3174 points at, scaled)
 * less its product with the cosine of the scaled variable */
static long double __stdcall sopturn_1020(uint8_t *element)
{
    long double angle = (long double)PF64(element, 0xc) * GR64(0x2008);
    long double rate = (long double)PI32((uintptr_t)GD32(0x3174), 8);
    long double cosine = gauge_fcos(angle);
    rate = rate * GR64(0x2000);
    return rate - cosine * rate;
}
GAUGE_INSTALL(sopturn_install)   /* 0x1050 */
GAUGE_INITIALIZE(sopturn_initialize)   /* 0x10a0 */

/* 0x10b0, header +0x18: update: PANELS +0x48 with the element at +0x3088, not the list */
static void __stdcall sopturn_10b0(void)
{
    GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3088));
}
GAUGE_GENERATE(sopturn_generate)   /* 0x10c0 */
GAUGE_DRAW(sopturn_draw)   /* 0x10e0 */
GAUGE_KILL(sopturn_kill)   /* 0x10f0 */
GAUGE_MODULE(sopturn_module)   /* 0x1120 */
GAUGE_QUERY(sopturn_query)   /* 0x1130 */

GAUGE_ROUTINES({0x1020, (void *)sopturn_1020}, {0x1050, (void *)sopturn_install}, {0x10a0, (void *)sopturn_initialize}, {0x10b0, (void *)sopturn_10b0}, {0x10c0, (void *)sopturn_generate}, {0x10e0, (void *)sopturn_draw}, {0x10f0, (void *)sopturn_kill}, {0x1120, (void *)sopturn_module}, {0x1130, (void *)sopturn_query})
