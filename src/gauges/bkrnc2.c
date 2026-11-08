/* BKRNC2.GAU -- BENDIX_KING_RADIO.NAV_COMM_2: the radio's fixed text and its frequency as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3670u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3440u
#define GAUGE_MOUSE 0x3448u
#define GAUGE_MOUSE_ARGUMENT 0x342cu


/* 0x1020: a fixed text (the format at +0x3658, nothing to format) into the element's buffer
 * (+0x3200); returns the frequency word (+0x3190) */
static long double __stdcall bkrnc2_1020(double value)
{
    uint32_t frequency = GD32(0x3190);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3200), (const char *)GAUGE_D(0x3658));
    return (long double)(uint64_t)frequency;
}

/* 0x1060: a radio's text callback: the frequency (0x32f0, four BCD digits) printed into the
 * element's buffer (0x3360) with the format at 0x3660; returns the frequency */
static long double __stdcall bkrnc2_1060(double value)
{
    uint32_t frequency = GD32(0x32f0);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3360), (const char *)GAUGE_D(0x3660),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}
GAUGE_INSTALL(bkrnc2_install)   /* 0x10c0 */
GAUGE_INITIALIZE(bkrnc2_initialize)   /* 0x1110 */
GAUGE_UPDATE(bkrnc2_update)   /* 0x1120 */
GAUGE_GENERATE(bkrnc2_generate)   /* 0x1130 */
GAUGE_DRAW(bkrnc2_draw)   /* 0x1150 */
GAUGE_KILL(bkrnc2_kill)   /* 0x1160 */
GAUGE_MODULE(bkrnc2_module)   /* 0x1190 */
GAUGE_QUERY(bkrnc2_query)   /* 0x11a0 */

GAUGE_ROUTINES({0x1020, (void *)bkrnc2_1020}, {0x1060, (void *)bkrnc2_1060}, {0x10c0, (void *)bkrnc2_install}, {0x1110, (void *)bkrnc2_initialize}, {0x1120, (void *)bkrnc2_update}, {0x1130, (void *)bkrnc2_generate}, {0x1150, (void *)bkrnc2_draw}, {0x1160, (void *)bkrnc2_kill}, {0x1190, (void *)bkrnc2_module}, {0x11a0, (void *)bkrnc2_query})
