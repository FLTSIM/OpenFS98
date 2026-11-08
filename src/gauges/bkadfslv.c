/* BKADFSLV.GAU -- BENDIX_KING.ADF.SLAVED: the slaved ADF card, which installs and updates its variable */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3528u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x33e8u
#define GAUGE_MOUSE 0x33f0u
#define GAUGE_MOUSE_ARGUMENT 0x33d4u


/* 0x1020: the card: the value less the card's setting (+0x3074) and a constant */
static long double __stdcall bkadfslv_1020(double value)
{
    return ((long double)value - GD64(0x3074)) - GR64(0x2000);
}

/* 0x1040: the knob: the variable plus the setting */
static long double __stdcall bkadfslv_1040(uint8_t *element)
{
    return (long double)PF64(element, 0xc) + GD64(0x3074);
}

/* 0x1050, header +0x10: install: the standard install, then the variable at +0x3068
 * initialised (PANELS +0x98) */
static void __stdcall bkadfslv_1050(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3068));
}
GAUGE_INITIALIZE(bkadfslv_initialize)   /* 0x10b0 */

/* 0x10c0, header +0x18: update: the variable looked up (PANELS +0xa0), then the elements */
static void __stdcall bkadfslv_10c0(void)
{
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3068));
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
}
GAUGE_GENERATE(bkadfslv_generate)   /* 0x10f0 */
GAUGE_DRAW(bkadfslv_draw)   /* 0x1110 */
GAUGE_KILL(bkadfslv_kill)   /* 0x1120 */
GAUGE_MODULE(bkadfslv_module)   /* 0x1150 */
GAUGE_QUERY(bkadfslv_query)   /* 0x1160 */

GAUGE_ROUTINES({0x1020, (void *)bkadfslv_1020}, {0x1040, (void *)bkadfslv_1040}, {0x1050, (void *)bkadfslv_1050}, {0x10b0, (void *)bkadfslv_initialize}, {0x10c0, (void *)bkadfslv_10c0}, {0x10f0, (void *)bkadfslv_generate}, {0x1110, (void *)bkadfslv_draw}, {0x1120, (void *)bkadfslv_kill}, {0x1150, (void *)bkadfslv_module}, {0x1160, (void *)bkadfslv_query})
