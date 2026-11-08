/* 737OMI.GAU -- 737-400.OMI: the marker lights' callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3240u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x31b0u
#define GAUGE_MOUSE 0x31b8u
#define GAUGE_MOUSE_ARGUMENT 0x319cu


/* 0x1020: the marker callback: at the first constant (equal, or unordered) that constant, with
 * bit 30 of +0x30a8 set; otherwise the value less the second, the bit cleared */
static long double __stdcall g737omi_1020(uint8_t *element)
{
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C3)) {
        GD32(0x30a8) |= 0x40000000u;
        return GR64(0x2000);
    }
    GD32(0x30a8) &= 0xbfffffffu;
    return (long double)PF64(element, 0xc) - GR64(0x2008);
}
GAUGE_INSTALL(g737omi_install)   /* 0x1070 */
GAUGE_INITIALIZE(g737omi_initialize)   /* 0x10c0 */
GAUGE_UPDATE(g737omi_update)   /* 0x10d0 */
GAUGE_GENERATE(g737omi_generate)   /* 0x10e0 */
GAUGE_DRAW(g737omi_draw)   /* 0x1100 */
GAUGE_KILL(g737omi_kill)   /* 0x1110 */
GAUGE_MODULE(g737omi_module)   /* 0x1140 */
GAUGE_QUERY(g737omi_query)   /* 0x1150 */

GAUGE_ROUTINES({0x1020, (void *)g737omi_1020}, {0x1070, (void *)g737omi_install}, {0x10c0, (void *)g737omi_initialize}, {0x10d0, (void *)g737omi_update}, {0x10e0, (void *)g737omi_generate}, {0x1100, (void *)g737omi_draw}, {0x1110, (void *)g737omi_kill}, {0x1140, (void *)g737omi_module}, {0x1150, (void *)g737omi_query})
