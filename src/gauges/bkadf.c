/* BKADF.GAU -- BENDIX_KING_RADIO.ADF: the ADF's frequency as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3488u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x32e0u
#define GAUGE_MOUSE 0x32e8u
#define GAUGE_MOUSE_ARGUMENT 0x32ccu


/* 0x1020: the ADF's text callback: the frequency's three BCD digits (+0x3190) and, while the
 * tenths (+0x31c8, through _ftol) are not zero, the standby's two (+0x31ac) after a point
 * (otherwise three spaces), printed into the element's buffer (+0x3200) with the format at
 * +0x3474; returns the frequency word */
static long double __stdcall bkadf_1020(double value)
{
    uint32_t standby = GD32(0x31ac), frequency = GD32(0x3190);
    uint8_t ones = (uint8_t)((uint8_t)(frequency & 0xf) + 0x30);
    uint8_t tens = (uint8_t)((uint8_t)((frequency >> 4) & 0xf) + 0x30);
    uint8_t hundreds = (uint8_t)((uint8_t)((frequency >> 8) & 0xf) + 0x30);
    uint8_t first, point, second;
    (void)value;
    if ((uint32_t)gauge_ftol(GD64(0x31c8))) {
        point = 0x2e;
        first = (uint8_t)((uint8_t)((standby >> 8) & 0xf) + 0x30);
        second = (uint8_t)((uint8_t)(standby & 0xf) + 0x30);
    } else {
        first = point = second = 0x20;
    }
    wsprintfA((char *)(uintptr_t)GD32(0x3200), (const char *)GAUGE_D(0x3474), (int32_t)(int8_t)first, (int32_t)(int8_t)hundreds,
              (int32_t)(int8_t)tens, (int32_t)(int8_t)ones, (int32_t)(int8_t)point, (int32_t)(int8_t)second, 0);
    return (long double)(uint64_t)frequency;
}
GAUGE_INSTALL(bkadf_install)   /* 0x10e0 */
GAUGE_INITIALIZE(bkadf_initialize)   /* 0x1130 */
GAUGE_UPDATE(bkadf_update)   /* 0x1140 */
GAUGE_GENERATE(bkadf_generate)   /* 0x1150 */
GAUGE_DRAW(bkadf_draw)   /* 0x1170 */
GAUGE_KILL(bkadf_kill)   /* 0x1180 */
GAUGE_MODULE(bkadf_module)   /* 0x11b0 */

GAUGE_QUERY(bkadf_11c0)   /* 0x11c0 (the _ftol import thunk at 0x11e0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)bkadf_1020}, {0x10e0, (void *)bkadf_install}, {0x1130, (void *)bkadf_initialize}, {0x1140, (void *)bkadf_update}, {0x1150, (void *)bkadf_generate}, {0x1170, (void *)bkadf_draw}, {0x1180, (void *)bkadf_kill}, {0x11b0, (void *)bkadf_module}, {0x11c0, (void *)bkadf_11c0})
