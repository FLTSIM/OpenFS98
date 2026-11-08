/* SCHCOM.GAU -- SCHWEIZER.COM-RADIO: the radio's frequency as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33e8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3248u
#define GAUGE_MOUSE 0x3250u
#define GAUGE_MOUSE_ARGUMENT 0x3234u


/* 0x1020: the radio's text callback: the frequency (+0x3100, four BCD digits) printed into
 * the element's buffer (+0x3170) with the format at +0x33dc; returns the value */
static long double __stdcall schcom_1020(double value)
{
    uint32_t frequency = GD32(0x3100);
    wsprintfA((char *)(uintptr_t)GD32(0x3170), (const char *)GAUGE_D(0x33dc),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return value;
}
GAUGE_INSTALL(schcom_install)   /* 0x1060 */
GAUGE_INITIALIZE(schcom_initialize)   /* 0x10b0 */
GAUGE_UPDATE(schcom_update)   /* 0x10c0 */
GAUGE_GENERATE(schcom_generate)   /* 0x10d0 */
GAUGE_DRAW(schcom_draw)   /* 0x10f0 */
GAUGE_KILL(schcom_kill)   /* 0x1100 */
GAUGE_MODULE(schcom_module)   /* 0x1130 */
GAUGE_QUERY(schcom_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)schcom_1020}, {0x1060, (void *)schcom_install}, {0x10b0, (void *)schcom_initialize}, {0x10c0, (void *)schcom_update}, {0x10d0, (void *)schcom_generate}, {0x10f0, (void *)schcom_draw}, {0x1100, (void *)schcom_kill}, {0x1130, (void *)schcom_module}, {0x1140, (void *)schcom_query})
