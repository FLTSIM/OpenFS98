/* MINICNTR.GAU -- MINICONTROLS: the mini control panel's lights */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3608u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x35f8u
#define GAUGE_MOUSE 0x0u
#define GAUGE_MOUSE_ARGUMENT 0x0u


/* 0x1020: +0x30a8 kept to [0, 999] in place (above the first constant, ordered, 999; below the
 * second or unordered, 0), printed through _ftol into the element's buffer (+0x3118) with the
 * format at +0x35fc; returns it */
static long double __stdcall minicntr_1020(double value)
{
    (void)value;
    if (!GAUGE_TEST(GD64(0x30a8), GR64(0x2000), GAUGE_C0C3)) GD64(0x30a8) = 999.0;
    if (GAUGE_TEST(GD64(0x30a8), GR64(0x2008), GAUGE_C0)) GD64(0x30a8) = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x3118), (const char *)GAUGE_D(0x35fc), (uint32_t)gauge_ftol(GD64(0x30a8)));
    return GD64(0x30a8);
}

/* 0x10a0: a light: bit 30 of +0x3318 set while +0x3330 is at (equal, or unordered) the constant at
 * 0x2008, cleared otherwise; returns the second constant either way */
static long double __stdcall minicntr_10a0(uint8_t *element)
{
    (void)element;
    if (GAUGE_TEST(GD64(0x3330), GR64(0x2008), GAUGE_C3)) GD32(0x3318) |= 0x40000000u;
    else GD32(0x3318) &= 0xbfffffffu;
    return GR64(0x2008);
}

/* 0x10e0: a light: bit 30 of +0x33c0 set while +0x33d8 is below (or unordered) the constant at
 * 0x2010, cleared otherwise; returns the second constant either way */
static long double __stdcall minicntr_10e0(uint8_t *element)
{
    (void)element;
    if (GAUGE_TEST(GD64(0x33d8), GR64(0x2010), GAUGE_C0)) GD32(0x33c0) |= 0x40000000u;
    else GD32(0x33c0) &= 0xbfffffffu;
    return GR64(0x2008);
}

/* 0x1120: a light: bit 30 of +0x3468 set while +0x3480 is below (or unordered) the constant at
 * 0x2018, cleared otherwise; returns the second constant either way */
static long double __stdcall minicntr_1120(uint8_t *element)
{
    (void)element;
    if (GAUGE_TEST(GD64(0x3480), GR64(0x2018), GAUGE_C0)) GD32(0x3468) |= 0x40000000u;
    else GD32(0x3468) &= 0xbfffffffu;
    return GR64(0x2008);
}

/* 0x1160: a light: bit 30 of +0x3510 set while +0x3528 is below (or unordered) the constant at
 * 0x2020, cleared otherwise; returns the second constant either way */
static long double __stdcall minicntr_1160(uint8_t *element)
{
    (void)element;
    if (GAUGE_TEST(GD64(0x3528), GR64(0x2020), GAUGE_C0)) GD32(0x3510) |= 0x40000000u;
    else GD32(0x3510) &= 0xbfffffffu;
    return GR64(0x2008);
}

/* 0x11a0: the variable as it is */
static long double __stdcall minicntr_11a0(uint8_t *element)
{
    return PF64(element, 0xc);
}

/* 0x11b0, header +0x10: install: the elements only */
static void __stdcall minicntr_11b0(uint32_t context)
{
    GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context);
}
GAUGE_INITIALIZE(minicntr_initialize)   /* 0x11d0 */
GAUGE_UPDATE(minicntr_update)   /* 0x11e0 */
GAUGE_GENERATE(minicntr_generate)   /* 0x11f0 */
GAUGE_DRAW(minicntr_draw)   /* 0x1210 */

/* 0x1220, header +0x24: kill: the elements only */
static void __stdcall minicntr_1220(void)
{
    GAUGE_PANELS_CALL(0x7c, 1)(GD32(GAUGE_ELEMENTS));
}
GAUGE_MODULE(minicntr_module)   /* 0x1230 */

GAUGE_QUERY(minicntr_1240)   /* 0x1240 (the _ftol import thunk at 0x1260 follows it) */

GAUGE_ROUTINES({0x1020, (void *)minicntr_1020}, {0x10a0, (void *)minicntr_10a0}, {0x10e0, (void *)minicntr_10e0}, {0x1120, (void *)minicntr_1120}, {0x1160, (void *)minicntr_1160}, {0x11a0, (void *)minicntr_11a0}, {0x11b0, (void *)minicntr_11b0}, {0x11d0, (void *)minicntr_initialize}, {0x11e0, (void *)minicntr_update}, {0x11f0, (void *)minicntr_generate}, {0x1210, (void *)minicntr_draw}, {0x1220, (void *)minicntr_1220}, {0x1230, (void *)minicntr_module}, {0x1240, (void *)minicntr_1240})
