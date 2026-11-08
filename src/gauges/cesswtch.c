/* CESSWTCH.GAU -- CESSNA_182.SWITCHES: the switches' mouse callback and state */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3748u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3378u
#define GAUGE_MOUSE 0x3380u
#define GAUGE_MOUSE_ARGUMENT 0x3364u


/* 0x1020: the mouse callback (point, flags): PANELS +0x8c (the key event) with 0x32 */
static uint32_t __stdcall cesswtch_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return GAUGE_PANELS_CALL(0x8c, 1)(0x32);
}

/* 0x1030: the switch's state, the variable's integer */
static long double __stdcall cesswtch_1030(uint8_t *element)
{
    return (long double)PI32(element, 0xc);
}
GAUGE_INSTALL(cesswtch_install)   /* 0x1040 */
GAUGE_INITIALIZE(cesswtch_initialize)   /* 0x1090 */
GAUGE_UPDATE(cesswtch_update)   /* 0x10a0 */
GAUGE_GENERATE(cesswtch_generate)   /* 0x10b0 */
GAUGE_DRAW(cesswtch_draw)   /* 0x10d0 */
GAUGE_KILL(cesswtch_kill)   /* 0x10e0 */
GAUGE_MODULE(cesswtch_module)   /* 0x1110 */
GAUGE_QUERY(cesswtch_query)   /* 0x1120 */

GAUGE_ROUTINES({0x1020, (void *)cesswtch_1020}, {0x1030, (void *)cesswtch_1030}, {0x1040, (void *)cesswtch_install}, {0x1090, (void *)cesswtch_initialize}, {0x10a0, (void *)cesswtch_update}, {0x10b0, (void *)cesswtch_generate}, {0x10d0, (void *)cesswtch_draw}, {0x10e0, (void *)cesswtch_kill}, {0x1110, (void *)cesswtch_module}, {0x1120, (void *)cesswtch_query})
