/* LRSPOIL.GAU -- LEAR_45.SPOILER: the spoiler lever's mouse callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3220u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3168u
#define GAUGE_MOUSE 0x3170u
#define GAUGE_MOUSE_ARGUMENT 0x3154u


/* 0x1020: the mouse callback (point, flags): the lever's position from the point's y less 5,
 * times 16390, over the lever's height (+8 of the rectangle at +0x3154), through _ftol, kept to
 * [0, 0x3fff]; outside (0xcdf, 0x202d) sent as event 0x100fa with the position (PANELS +0x90),
 * inside and with flag 0x20000000 event 0x1013d with 0; returns 0 */
static uint32_t __stdcall lrspoil_1020(uint8_t *point, uint32_t flags)
{
    uint32_t y = P32(point, 4) - 5;
    int32_t position = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)(y * 16390u) / (long double)PI32((uintptr_t)GD32(0x3154), 8));
    if (position > 0x3fff) position = 0x3fff;
    if (position < 0) position = 0;
    if (position > 0xcdf && position < 0x202d) {
        if (flags & 0x20000000u) GAUGE_PANELS_CALL(0x90, 2)(0x1013d, 0);
        return 0;
    }
    GAUGE_PANELS_CALL(0x90, 2)(0x100fa, (uint32_t)position);
    return 0;
}
GAUGE_INSTALL(lrspoil_install)   /* 0x10b0 */
GAUGE_INITIALIZE(lrspoil_initialize)   /* 0x1100 */
GAUGE_UPDATE(lrspoil_update)   /* 0x1110 */
GAUGE_GENERATE(lrspoil_generate)   /* 0x1120 */
GAUGE_DRAW(lrspoil_draw)   /* 0x1140 */
GAUGE_KILL(lrspoil_kill)   /* 0x1150 */
GAUGE_MODULE(lrspoil_module)   /* 0x1180 */

GAUGE_QUERY(lrspoil_1190)   /* 0x1190 (the _ftol import thunk at 0x11b0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)lrspoil_1020}, {0x10b0, (void *)lrspoil_install}, {0x1100, (void *)lrspoil_initialize}, {0x1110, (void *)lrspoil_update}, {0x1120, (void *)lrspoil_generate}, {0x1140, (void *)lrspoil_draw}, {0x1150, (void *)lrspoil_kill}, {0x1180, (void *)lrspoil_module}, {0x1190, (void *)lrspoil_1190})
