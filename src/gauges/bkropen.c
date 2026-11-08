/* BKROPEN.GAU -- BENDIX_KING_RADIOS_OPEN: the radio panel's cover: one element handled directly (PANELS +0x40..+0x58) */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3178u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x0u
#define GAUGE_MOUSE 0x30c8u
#define GAUGE_MOUSE_ARGUMENT 0x0u


/* 0x1020: the mouse callback (point, flags): PANELS +0x8c (the key event) with 0x32 */
static uint32_t __stdcall bkropen_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    return GAUGE_PANELS_CALL(0x8c, 1)(0x32);
}

/* 0x1030, header +0x10: install: the element (+0x3070) installed itself (PANELS +0x40), then
 * the mouse rectangles (+0x30c8, with +4 of the pointer at +0x30ac) */
static void __stdcall bkropen_1030(uint32_t context)
{
    GAUGE_PANELS_CALL(0x40, 2)(GAUGE_ADDRESS(0x3070), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(0x30c8), GAUGE_ADDRESS(GAUGE_HEADER), GD32(0x30ac) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(0x30c8), GAUGE_ADDRESS(GAUGE_HEADER));
}

/* 0x1080, header +0x14: initialise the element (PANELS +0x44) */
static void __stdcall bkropen_1080(void) { GAUGE_PANELS_CALL(0x44, 1)(GAUGE_ADDRESS(0x3070)); }

/* 0x1090, header +0x18: update the element (PANELS +0x48) */
static void __stdcall bkropen_1090(void) { GAUGE_PANELS_CALL(0x48, 1)(GAUGE_ADDRESS(0x3070)); }

/* 0x10a0, header +0x1c: generate the element (PANELS +0x4c) */
static void __stdcall bkropen_10a0(uint32_t phase) { GAUGE_PANELS_CALL(0x4c, 2)(GAUGE_ADDRESS(0x3070), phase); }

/* 0x10c0, header +0x20: draw the element (PANELS +0x50) */
static void __stdcall bkropen_10c0(void) { GAUGE_PANELS_CALL(0x50, 1)(GAUGE_ADDRESS(0x3070)); }

/* 0x10d0, header +0x24: kill: the mouse rectangles, then the element (PANELS +0x58) */
static void __stdcall bkropen_10d0(void)
{
    GAUGE_PANELS_CALL(0x88, 2)(GAUGE_ADDRESS(0x30c8), GAUGE_ADDRESS(GAUGE_HEADER));
    GAUGE_PANELS_CALL(0x58, 1)(GAUGE_ADDRESS(0x3070));
}
GAUGE_MODULE(bkropen_module)   /* 0x1100 */
GAUGE_QUERY(bkropen_query)   /* 0x1110 */

GAUGE_ROUTINES({0x1020, (void *)bkropen_1020}, {0x1030, (void *)bkropen_1030}, {0x1080, (void *)bkropen_1080}, {0x1090, (void *)bkropen_1090}, {0x10a0, (void *)bkropen_10a0}, {0x10c0, (void *)bkropen_10c0}, {0x10d0, (void *)bkropen_10d0}, {0x1100, (void *)bkropen_module}, {0x1110, (void *)bkropen_query})
