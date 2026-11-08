/* CESSOMI.GAU -- CESSNA_182.OMI: the marker lights' callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x32f0u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3158u
#define GAUGE_MOUSE 0x3160u
#define GAUGE_MOUSE_ARGUMENT 0x3144u


/* 0x1020: the marker callback: at the first constant (equal, or unordered) that constant, with
 * bit 30 of +0x30a8 set; otherwise the value less the second, the bit cleared */
static long double __stdcall cessomi_1020(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C3)) {
        GD32(0x30a8) |= 0x40000000u;
        return GR64(0x2000);
    }
    GD32(0x30a8) &= 0xbfffffffu;
    return (long double)PF64(element, 0xc) - GR64(0x2008);
}
GAUGE_INSTALL(cessomi_install)   /* 0x1070 */
GAUGE_INITIALIZE(cessomi_initialize)   /* 0x10c0 */
GAUGE_UPDATE(cessomi_update)   /* 0x10d0 */
GAUGE_GENERATE(cessomi_generate)   /* 0x10e0 */
GAUGE_DRAW(cessomi_draw)   /* 0x1100 */
GAUGE_KILL(cessomi_kill)   /* 0x1110 */
GAUGE_MODULE(cessomi_module)   /* 0x1140 */
GAUGE_QUERY(cessomi_query)   /* 0x1150 */

GAUGE_ROUTINES({0x1020, (void *)cessomi_1020}, {0x1070, (void *)cessomi_install}, {0x10c0, (void *)cessomi_initialize}, {0x10d0, (void *)cessomi_update}, {0x10e0, (void *)cessomi_generate}, {0x1100, (void *)cessomi_draw}, {0x1110, (void *)cessomi_kill}, {0x1140, (void *)cessomi_module}, {0x1150, (void *)cessomi_query})
