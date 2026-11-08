/* 737XFEED.GAU -- 737-400.X-FEED: the crossfeed switch's callback */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3250u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3140u
#define GAUGE_MOUSE 0x3148u
#define GAUGE_MOUSE_ARGUMENT 0x312cu


/* 0x1020: the switch's state, the variable's integer */
static long double __stdcall g737xfeed_1020(uint8_t *element)
{
    return (long double)PI32(element, 0xc);
}
GAUGE_INSTALL(g737xfeed_install)   /* 0x1030 */
GAUGE_INITIALIZE(g737xfeed_initialize)   /* 0x1080 */
GAUGE_UPDATE(g737xfeed_update)   /* 0x1090 */
GAUGE_GENERATE(g737xfeed_generate)   /* 0x10a0 */
GAUGE_DRAW(g737xfeed_draw)   /* 0x10c0 */
GAUGE_KILL(g737xfeed_kill)   /* 0x10d0 */
GAUGE_MODULE(g737xfeed_module)   /* 0x1100 */
GAUGE_QUERY(g737xfeed_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)g737xfeed_1020}, {0x1030, (void *)g737xfeed_install}, {0x1080, (void *)g737xfeed_initialize}, {0x1090, (void *)g737xfeed_update}, {0x10a0, (void *)g737xfeed_generate}, {0x10c0, (void *)g737xfeed_draw}, {0x10d0, (void *)g737xfeed_kill}, {0x1100, (void *)g737xfeed_module}, {0x1110, (void *)g737xfeed_query})
