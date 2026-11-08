/* EXCSURF.GAU -- EXTRA-300.CONTROL_SURFACES: only the gauge header's callbacks, in their standard form; PANELS draws the
 * elements from the data alone. */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3568u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3378u
#define GAUGE_MOUSE 0x3380u
#define GAUGE_MOUSE_ARGUMENT 0x3364u

GAUGE_INSTALL(excsurf_install)   /* 0x1020 */
GAUGE_INITIALIZE(excsurf_initialize)   /* 0x1070 */
GAUGE_UPDATE(excsurf_update)   /* 0x1080 */
GAUGE_GENERATE(excsurf_generate)   /* 0x1090 */
GAUGE_DRAW(excsurf_draw)   /* 0x10b0 */
GAUGE_KILL(excsurf_kill)   /* 0x10c0 */
GAUGE_MODULE(excsurf_module)   /* 0x10f0 */
GAUGE_QUERY(excsurf_query)   /* 0x1100 */

GAUGE_ROUTINES({0x1020, (void *)excsurf_install}, {0x1070, (void *)excsurf_initialize}, {0x1080, (void *)excsurf_update}, {0x1090, (void *)excsurf_generate}, {0x10b0, (void *)excsurf_draw}, {0x10c0, (void *)excsurf_kill}, {0x10f0, (void *)excsurf_module}, {0x1100, (void *)excsurf_query})
