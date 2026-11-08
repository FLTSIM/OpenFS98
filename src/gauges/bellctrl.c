/* BELLCTRL.GAU -- BELL_206B.CONTROL_SURFACES: only the gauge header's callbacks, in their standard form; PANELS draws the
 * elements from the data alone. */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3460u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x32c8u
#define GAUGE_MOUSE 0x32d0u
#define GAUGE_MOUSE_ARGUMENT 0x32b4u

GAUGE_INSTALL(bellctrl_install)   /* 0x1020 */
GAUGE_INITIALIZE(bellctrl_initialize)   /* 0x1070 */
GAUGE_UPDATE(bellctrl_update)   /* 0x1080 */
GAUGE_GENERATE(bellctrl_generate)   /* 0x1090 */
GAUGE_DRAW(bellctrl_draw)   /* 0x10b0 */
GAUGE_KILL(bellctrl_kill)   /* 0x10c0 */
GAUGE_MODULE(bellctrl_module)   /* 0x10f0 */
GAUGE_QUERY(bellctrl_query)   /* 0x1100 */

GAUGE_ROUTINES({0x1020, (void *)bellctrl_install}, {0x1070, (void *)bellctrl_initialize}, {0x1080, (void *)bellctrl_update}, {0x1090, (void *)bellctrl_generate}, {0x10b0, (void *)bellctrl_draw}, {0x10c0, (void *)bellctrl_kill}, {0x10f0, (void *)bellctrl_module}, {0x1100, (void *)bellctrl_query})
