/* LRELTRIM.GAU -- LEAR_45.ELEVATOR-TRIM: only the gauge header's callbacks, in their standard form; PANELS draws the
 * elements from the data alone. */
#include "gauge.h"

#define GAUGE_IMPORTS 0x31f8u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3168u
#define GAUGE_MOUSE 0x3170u
#define GAUGE_MOUSE_ARGUMENT 0x3154u

GAUGE_INSTALL(lreltrim_install)   /* 0x1020 */
GAUGE_INITIALIZE(lreltrim_initialize)   /* 0x1070 */
GAUGE_UPDATE(lreltrim_update)   /* 0x1080 */
GAUGE_GENERATE(lreltrim_generate)   /* 0x1090 */
GAUGE_DRAW(lreltrim_draw)   /* 0x10b0 */
GAUGE_KILL(lreltrim_kill)   /* 0x10c0 */
GAUGE_MODULE(lreltrim_module)   /* 0x10f0 */
GAUGE_QUERY(lreltrim_query)   /* 0x1100 */

GAUGE_ROUTINES({0x1020, (void *)lreltrim_install}, {0x1070, (void *)lreltrim_initialize}, {0x1080, (void *)lreltrim_update}, {0x1090, (void *)lreltrim_generate}, {0x10b0, (void *)lreltrim_draw}, {0x10c0, (void *)lreltrim_kill}, {0x10f0, (void *)lreltrim_module}, {0x1100, (void *)lreltrim_query})
