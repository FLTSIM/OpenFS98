/* 737WARNG.GAU -- 737-400.WARNING_ENUNCIATORS: the warning lights: an install and kill without mouse rectangles */
#include "gauge.h"

#define GAUGE_IMPORTS 0x30c8u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x30c0u
#define GAUGE_MOUSE 0x0u
#define GAUGE_MOUSE_ARGUMENT 0x0u


/* 0x1020, header +0x10: install: the elements only */
static void __stdcall g737warng_1020(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
}
GAUGE_INITIALIZE(g737warng_initialize)   /* 0x1040 */
GAUGE_UPDATE(g737warng_update)   /* 0x1050 */
GAUGE_GENERATE(g737warng_generate)   /* 0x1060 */
GAUGE_DRAW(g737warng_draw)   /* 0x1080 */

/* 0x1090, header +0x24: kill: the elements only */
static void __stdcall g737warng_1090(void)
{
    GAUGE_PANELS_CALL(0x7c, 1)(GD32(GAUGE_ELEMENTS));
}
GAUGE_MODULE(g737warng_module)   /* 0x10a0 */
GAUGE_QUERY(g737warng_query)   /* 0x10b0 */

GAUGE_ROUTINES({0x1020, (void *)g737warng_1020}, {0x1040, (void *)g737warng_initialize}, {0x1050, (void *)g737warng_update}, {0x1060, (void *)g737warng_generate}, {0x1080, (void *)g737warng_draw}, {0x1090, (void *)g737warng_1090}, {0x10a0, (void *)g737warng_module}, {0x10b0, (void *)g737warng_query})
