/* EXSWITCH.GAU -- EXTRA-300.SWITCHES: the switches' mouse callback sends a key event */
#include "gauge.h"

#define GAUGE_IMPORTS 0x35e0u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3340u
#define GAUGE_MOUSE 0x3348u
#define GAUGE_MOUSE_ARGUMENT 0x332cu


/* 0x1020: the mouse callback (point, flags): PANELS +0x8c (the key event) with 0x32 */
static uint32_t __stdcall exswitch_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return GAUGE_PANELS_CALL(0x8c, 1)(0x32);
}
GAUGE_INSTALL(exswitch_install)   /* 0x1030 */
GAUGE_INITIALIZE(exswitch_initialize)   /* 0x1080 */
GAUGE_UPDATE(exswitch_update)   /* 0x1090 */
GAUGE_GENERATE(exswitch_generate)   /* 0x10a0 */
GAUGE_DRAW(exswitch_draw)   /* 0x10c0 */
GAUGE_KILL(exswitch_kill)   /* 0x10d0 */
GAUGE_MODULE(exswitch_module)   /* 0x1100 */
GAUGE_QUERY(exswitch_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)exswitch_1020}, {0x1030, (void *)exswitch_install}, {0x1080, (void *)exswitch_initialize}, {0x1090, (void *)exswitch_update}, {0x10a0, (void *)exswitch_generate}, {0x10c0, (void *)exswitch_draw}, {0x10d0, (void *)exswitch_kill}, {0x1100, (void *)exswitch_module}, {0x1110, (void *)exswitch_query})
