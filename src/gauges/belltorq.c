/* BELLTORQ.GAU -- BELL_206B.TORQUE: the torque needle's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3278u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x31e8u
#define GAUGE_MOUSE 0x31f0u
#define GAUGE_MOUSE_ARGUMENT 0x31d4u


/* 0x1020: the needle: the value scaled, clamped */
static long double __stdcall belltorq_1020(double value)
{
    return gauge_clamp((long double)value * GR64(0x2000), GR64(0x2008), GR64(0x2010));
}
GAUGE_INSTALL(belltorq_install)   /* 0x1060 */
GAUGE_INITIALIZE(belltorq_initialize)   /* 0x10b0 */
GAUGE_UPDATE(belltorq_update)   /* 0x10c0 */
GAUGE_GENERATE(belltorq_generate)   /* 0x10d0 */
GAUGE_DRAW(belltorq_draw)   /* 0x10f0 */
GAUGE_KILL(belltorq_kill)   /* 0x1100 */
GAUGE_MODULE(belltorq_module)   /* 0x1130 */
GAUGE_QUERY(belltorq_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)belltorq_1020}, {0x1060, (void *)belltorq_install}, {0x10b0, (void *)belltorq_initialize}, {0x10c0, (void *)belltorq_update}, {0x10d0, (void *)belltorq_generate}, {0x10f0, (void *)belltorq_draw}, {0x1100, (void *)belltorq_kill}, {0x1130, (void *)belltorq_module}, {0x1140, (void *)belltorq_query})
