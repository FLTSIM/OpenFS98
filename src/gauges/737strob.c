/* 737STROB.GAU -- 737-400.STROB-NAV-SWITCH: only the gauge header's callbacks, in their standard form; PANELS draws the
 * elements from the data alone. */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3480u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3268u
#define GAUGE_MOUSE 0x3270u
#define GAUGE_MOUSE_ARGUMENT 0x3254u

GAUGE_INSTALL(g737strob_install)   /* 0x1020 */
GAUGE_INITIALIZE(g737strob_initialize)   /* 0x1070 */
GAUGE_UPDATE(g737strob_update)   /* 0x1080 */
GAUGE_GENERATE(g737strob_generate)   /* 0x1090 */
GAUGE_DRAW(g737strob_draw)   /* 0x10b0 */
GAUGE_KILL(g737strob_kill)   /* 0x10c0 */
GAUGE_MODULE(g737strob_module)   /* 0x10f0 */
GAUGE_QUERY(g737strob_query)   /* 0x1100 */

GAUGE_ROUTINES({0x1020, (void *)g737strob_install}, {0x1070, (void *)g737strob_initialize}, {0x1080, (void *)g737strob_update}, {0x1090, (void *)g737strob_generate}, {0x10b0, (void *)g737strob_draw}, {0x10c0, (void *)g737strob_kill}, {0x10f0, (void *)g737strob_module}, {0x1100, (void *)g737strob_query})
