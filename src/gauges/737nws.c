/* 737NWS.GAU -- 737-400.NWS: the nose-wheel steering lever's mouse callback sends a key event */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3168u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x30b0u
#define GAUGE_MOUSE 0x30b8u
#define GAUGE_MOUSE_ARGUMENT 0x309cu


/* 0x1020: the mouse callback (point, flags): PANELS +0x8c (the key event) with 0x32 */
static uint32_t __stdcall g737nws_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return GAUGE_PANELS_CALL(0x8c, 1)(0x32);
}
GAUGE_INSTALL(g737nws_install)   /* 0x1030 */
GAUGE_INITIALIZE(g737nws_initialize)   /* 0x1080 */
GAUGE_UPDATE(g737nws_update)   /* 0x1090 */
GAUGE_GENERATE(g737nws_generate)   /* 0x10a0 */
GAUGE_DRAW(g737nws_draw)   /* 0x10c0 */
GAUGE_KILL(g737nws_kill)   /* 0x10d0 */
GAUGE_MODULE(g737nws_module)   /* 0x1100 */
GAUGE_QUERY(g737nws_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)g737nws_1020}, {0x1030, (void *)g737nws_install}, {0x1080, (void *)g737nws_initialize}, {0x1090, (void *)g737nws_update}, {0x10a0, (void *)g737nws_generate}, {0x10c0, (void *)g737nws_draw}, {0x10d0, (void *)g737nws_kill}, {0x1100, (void *)g737nws_module}, {0x1110, (void *)g737nws_query})
