/* BKRNC1.GAU -- BENDIX_KING_RADIO.NAV_COMM_1: the radio's two frequencies as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x37f0u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3468u
#define GAUGE_MOUSE 0x3470u
#define GAUGE_MOUSE_ARGUMENT 0x3454u


/* 0x1020: a radio's text callback: the frequency (0x31b8, four BCD digits) printed into the
 * element's buffer (0x3228) with the format at +0x37e0; returns the frequency */
static long double __stdcall bkrnc1_1020(double value)
{
    uint32_t frequency = GD32(0x31b8);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3228), (const char *)GAUGE_D(0x37e0),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x1080: a radio's text callback: the frequency (0x3318, four BCD digits) printed into the
 * element's buffer (0x3388) with the format at +0x37e0; returns the frequency */
static long double __stdcall bkrnc1_1080(double value)
{
    uint32_t frequency = GD32(0x3318);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3388), (const char *)GAUGE_D(0x37e0),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}
GAUGE_INSTALL(bkrnc1_install)   /* 0x10e0 */
GAUGE_INITIALIZE(bkrnc1_initialize)   /* 0x1130 */
GAUGE_UPDATE(bkrnc1_update)   /* 0x1140 */
GAUGE_GENERATE(bkrnc1_generate)   /* 0x1150 */
GAUGE_DRAW(bkrnc1_draw)   /* 0x1170 */
GAUGE_KILL(bkrnc1_kill)   /* 0x1180 */
GAUGE_MODULE(bkrnc1_module)   /* 0x11b0 */
GAUGE_QUERY(bkrnc1_query)   /* 0x11c0 */

GAUGE_ROUTINES({0x1020, (void *)bkrnc1_1020}, {0x1080, (void *)bkrnc1_1080}, {0x10e0, (void *)bkrnc1_install}, {0x1130, (void *)bkrnc1_initialize}, {0x1140, (void *)bkrnc1_update}, {0x1150, (void *)bkrnc1_generate}, {0x1170, (void *)bkrnc1_draw}, {0x1180, (void *)bkrnc1_kill}, {0x11b0, (void *)bkrnc1_module}, {0x11c0, (void *)bkrnc1_query})
