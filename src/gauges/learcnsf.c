/* LEARCNSF.GAU -- LEAR_45.CONTROL_SURFACES: only the gauge header's callbacks, in their standard form; PANELS draws the
 * elements from the data alone. */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3460u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x32c8u
#define GAUGE_MOUSE 0x32d0u
#define GAUGE_MOUSE_ARGUMENT 0x32b4u

GAUGE_INSTALL(learcnsf_install)   /* 0x1020 */
GAUGE_INITIALIZE(learcnsf_initialize)   /* 0x1070 */
GAUGE_UPDATE(learcnsf_update)   /* 0x1080 */
GAUGE_GENERATE(learcnsf_generate)   /* 0x1090 */
GAUGE_DRAW(learcnsf_draw)   /* 0x10b0 */
GAUGE_KILL(learcnsf_kill)   /* 0x10c0 */
GAUGE_MODULE(learcnsf_module)   /* 0x10f0 */
GAUGE_QUERY(learcnsf_query)   /* 0x1100 */

GAUGE_ROUTINES({0x1020, (void *)learcnsf_install}, {0x1070, (void *)learcnsf_initialize}, {0x1080, (void *)learcnsf_update}, {0x1090, (void *)learcnsf_generate}, {0x10b0, (void *)learcnsf_draw}, {0x10c0, (void *)learcnsf_kill}, {0x10f0, (void *)learcnsf_module}, {0x1100, (void *)learcnsf_query})
