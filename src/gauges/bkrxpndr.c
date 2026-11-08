/* BKRXPNDR.GAU -- BENDIX_KING_RADIO.XPNDR: the transponder's code as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3408u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3268u
#define GAUGE_MOUSE 0x3270u
#define GAUGE_MOUSE_ARGUMENT 0x3254u


/* 0x1020: the transponder's text callback: the code (+0x3120, four BCD digits) printed into the
 * element's buffer (+0x3190) with the format at +0x33fc; returns the code */
static long double __stdcall bkrxpndr_1020(double value)
{
    uint32_t code = GD32(0x3120);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3190), (const char *)GAUGE_D(0x33fc),
              (code >> 12) & 0xf, (code >> 8) & 0xf, (code >> 4) & 0xf, code & 0xf);
    return (long double)(uint64_t)code;
}
GAUGE_INSTALL(bkrxpndr_install)   /* 0x1080 */
GAUGE_INITIALIZE(bkrxpndr_initialize)   /* 0x10d0 */
GAUGE_UPDATE(bkrxpndr_update)   /* 0x10e0 */
GAUGE_GENERATE(bkrxpndr_generate)   /* 0x10f0 */
GAUGE_DRAW(bkrxpndr_draw)   /* 0x1110 */
GAUGE_KILL(bkrxpndr_kill)   /* 0x1120 */
GAUGE_MODULE(bkrxpndr_module)   /* 0x1150 */
GAUGE_QUERY(bkrxpndr_query)   /* 0x1160 */

GAUGE_ROUTINES({0x1020, (void *)bkrxpndr_1020}, {0x1080, (void *)bkrxpndr_install}, {0x10d0, (void *)bkrxpndr_initialize}, {0x10e0, (void *)bkrxpndr_update}, {0x10f0, (void *)bkrxpndr_generate}, {0x1110, (void *)bkrxpndr_draw}, {0x1120, (void *)bkrxpndr_kill}, {0x1150, (void *)bkrxpndr_module}, {0x1160, (void *)bkrxpndr_query})
