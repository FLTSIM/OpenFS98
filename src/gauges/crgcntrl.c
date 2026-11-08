/* CRGCNTRL.GAU -- CESSNA_182_RG.CONTROLS: the levers and the gear lights, whose variable is installed and updated */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3e28u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x38a0u
#define GAUGE_MOUSE 0x38a8u
#define GAUGE_MOUSE_ARGUMENT 0x388cu


/* 0x1020: a lever's mouse callback (point, flags): 0x66 less the point's x, times 0x4000 and
 * a scale, through _ftol, kept to [0, 0x3fff], sent as event 0x100a1 (PANELS +0x90). Returns 0. */
static uint32_t __stdcall crgcntrl_1020(uint8_t *point, uint32_t flags)
{
    int32_t lever = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)((0x66u - P32(point, 0)) << 14) * GR64(0x2000));
    (void)flags;
    if (lever > 0x3fff) lever = 0x3fff;
    if (lever < 0) lever = 0;
    GAUGE_PANELS_CALL(0x90, 2)(0x100a1, (uint32_t)lever);
    return 0;
}

/* 0x1070: a lever's mouse callback (point, flags): 0x5a less the point's x, times 0x4000 and
 * a scale, through _ftol, kept to [0, 0x3fff], sent as event 0x100e7 (PANELS +0x90). Returns 0. */
static uint32_t __stdcall crgcntrl_1070(uint8_t *point, uint32_t flags)
{
    int32_t lever = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)((0x5au - P32(point, 0)) << 14) * GR64(0x2008));
    (void)flags;
    if (lever > 0x3fff) lever = 0x3fff;
    if (lever < 0) lever = 0;
    GAUGE_PANELS_CALL(0x90, 2)(0x100e7, (uint32_t)lever);
    return 0;
}

/* 0x10c0: a lever's mouse callback (point, flags): 0x5a less the point's x, times 0x4000 and
 * a scale, through _ftol, kept to [0, 0x3fff], sent as event 0x100ed (PANELS +0x90). Returns 0. */
static uint32_t __stdcall crgcntrl_10c0(uint8_t *point, uint32_t flags)
{
    int32_t lever = (int32_t)(uint32_t)gauge_ftol((long double)(int32_t)((0x5au - P32(point, 0)) << 14) * GR64(0x2008));
    (void)flags;
    if (lever > 0x3fff) lever = 0x3fff;
    if (lever < 0) lever = 0;
    GAUGE_PANELS_CALL(0x90, 2)(0x100ed, (uint32_t)lever);
    return 0;
}

/* 0x1110: the gear lights: unless the variable equals the gear's last state (+0x30a4), the
 * third constant; equal to it, the fourth; otherwise the fifth when the variable is exactly
 * 0x40cfff80:00000000 (16383), else the third */
static long double __stdcall crgcntrl_1110(uint8_t *element)
{
    if (!GAUGE_TEST(GD64(0x30a4), PF64(element, 0xc), GAUGE_C3)) return GR64(0x2010);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2010), GAUGE_C3)) return GR64(0x2018);
    if (P32(element, 0xc) == 0 && P32(element, 0x10) == 0x40cfff80u) return GR64(0x2020);
    return GR64(0x2010);
}

/* 0x1160, header +0x10: install: the standard install, then the variable at +0x3098
 * initialised (PANELS +0x98) */
static void __stdcall crgcntrl_1160(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
    GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4);
    GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER));
    GAUGE_PANELS_CALL(0x98, 1)(GAUGE_ADDRESS(0x3098));
}
GAUGE_INITIALIZE(crgcntrl_initialize)   /* 0x11c0 */

/* 0x11d0, header +0x18: update: the variable looked up (PANELS +0xa0), then the elements */
static void __stdcall crgcntrl_11d0(void)
{
    GAUGE_PANELS_CALL(0xa0, 1)(GAUGE_ADDRESS(0x3098));
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
}
GAUGE_GENERATE(crgcntrl_generate)   /* 0x1200 */
GAUGE_DRAW(crgcntrl_draw)   /* 0x1220 */
GAUGE_KILL(crgcntrl_kill)   /* 0x1230 */
GAUGE_MODULE(crgcntrl_module)   /* 0x1260 */

GAUGE_QUERY(crgcntrl_1270)   /* 0x1270 (the _ftol import thunk at 0x1290 follows it) */

GAUGE_ROUTINES({0x1020, (void *)crgcntrl_1020}, {0x1070, (void *)crgcntrl_1070}, {0x10c0, (void *)crgcntrl_10c0}, {0x1110, (void *)crgcntrl_1110}, {0x1160, (void *)crgcntrl_1160}, {0x11c0, (void *)crgcntrl_initialize}, {0x11d0, (void *)crgcntrl_11d0}, {0x1200, (void *)crgcntrl_generate}, {0x1220, (void *)crgcntrl_draw}, {0x1230, (void *)crgcntrl_kill}, {0x1260, (void *)crgcntrl_module}, {0x1270, (void *)crgcntrl_1270})
