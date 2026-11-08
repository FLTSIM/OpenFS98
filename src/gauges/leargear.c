/* LEARGEAR.GAU -- LEAR_45.GEAR: the gear lights' callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33b8u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3300u
#define GAUGE_MOUSE 0x3308u
#define GAUGE_MOUSE_ARGUMENT 0x32ecu


/* 0x1020: the gear lights: unless the variable equals the gear's last state (+0x30c0), the
 * first constant; equal to it, the second; otherwise the third when the variable is exactly
 * 0x40cfff80:00000000 (16383), else the first */
static long double __stdcall leargear_1020(uint8_t *element)
{
    if (!GAUGE_TEST(GD64(0x30c0), PF64(element, 0xc), GAUGE_C3)) return GR64(0x2000);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2000), GAUGE_C3)) return GR64(0x2008);
    if (P32(element, 0xc) == 0 && P32(element, 0x10) == 0x40cfff80u) return GR64(0x2010);
    return GR64(0x2000);
}
GAUGE_INSTALL(leargear_install)   /* 0x1070 */
GAUGE_INITIALIZE(leargear_initialize)   /* 0x10c0 */
GAUGE_UPDATE(leargear_update)   /* 0x10d0 */
GAUGE_GENERATE(leargear_generate)   /* 0x10e0 */
GAUGE_DRAW(leargear_draw)   /* 0x1100 */
GAUGE_KILL(leargear_kill)   /* 0x1110 */
GAUGE_MODULE(leargear_module)   /* 0x1140 */
GAUGE_QUERY(leargear_query)   /* 0x1150 */

GAUGE_ROUTINES({0x1020, (void *)leargear_1020}, {0x1070, (void *)leargear_install}, {0x10c0, (void *)leargear_initialize}, {0x10d0, (void *)leargear_update}, {0x10e0, (void *)leargear_generate}, {0x1100, (void *)leargear_draw}, {0x1110, (void *)leargear_kill}, {0x1140, (void *)leargear_module}, {0x1150, (void *)leargear_query})
