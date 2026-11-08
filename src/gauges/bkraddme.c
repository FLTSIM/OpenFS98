/* BKRADDME.GAU -- BENDIX_KING_RADIO.DME: the DME: its source switch, distance, speed and the hold light */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3528u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x33e8u
#define GAUGE_MOUSE 0x33f0u
#define GAUGE_MOUSE_ARGUMENT 0x33d4u


/* 0x1020: the source switch's mouse callback: event 0x10021, then by the DME's flag (+0x3568)
 * 0x100fc (setting it) or 0x100fb (clearing it), each with 0 (PANELS +0x90). Returns 1. */
static uint32_t __stdcall bkraddme_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0x90, 2)(0x10021, 0);
    if (!GD32(0x3568)) {
        GAUGE_PANELS_CALL(0x90, 2)(0x100fc, 0);
        GD32(0x3568) = 1;
    } else {
        GAUGE_PANELS_CALL(0x90, 2)(0x100fb, 0);
        GD32(0x3568) = 0;
    }
    return 1;
}

/* 0x1090: the distance as text into the element's buffer (+0x3148): from the first DME (+0x30d8,
 * the flag cleared) when the source (+0x3110) is 1, otherwise the second (+0x30f4, the flag set);
 * at the first constant (equal, or unordered) the text at +0x34f8; otherwise kept to its range
 * (above the second constant, ordered, it; below the third, or unordered, it), above the fourth
 * (ordered) printed whole with +0x350c, otherwise as whole and tenths (the fraction times the
 * fifth constant, each through _ftol) with +0x3500. Returns the distance as retail parks it in
 * the argument's slot. */
static long double __stdcall bkraddme_1090(double value)
{
    long double distance;
    uint16_t blank;
    if (GD32(0x3110) == 1) {
        distance = GD64(0x30d8);
        GD32(0x3568) = 0;
    } else {
        distance = GD64(0x30f4);
        GD32(0x3568) = 1;
    }
    blank = gauge_fcom(distance, GR64(0x2000));
    gauge_fst(&value, distance);     /* the fst into the argument's slot, kept (its flags) */
    if (blank & GAUGE_C3) {
        wsprintfA((char *)(uintptr_t)GD32(0x3148), (const char *)GAUGE_D(0x34f8));
        return value;
    }
    if (!GAUGE_TEST(distance, GR64(0x2008), GAUGE_C0C3)) value = (double)(distance = GR64(0x2008));
    else if (GAUGE_TEST(distance, GR64(0x2010), GAUGE_C0)) value = (double)(distance = GR64(0x2010));
    if (!GAUGE_TEST(distance, GR64(0x2018), GAUGE_C0C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x3148), (const char *)GAUGE_D(0x350c), (uint32_t)gauge_ftol(distance));
    } else {
        uint32_t whole = (uint32_t)gauge_ftol(distance);
        uint32_t part = (uint32_t)gauge_ftol((distance - (long double)(int32_t)whole) * GR64(0x2020));
        wsprintfA((char *)(uintptr_t)GD32(0x3148), (const char *)GAUGE_D(0x3500), whole, part);
    }
    return value;
}

/* 0x11a0: the speed as text into the element's buffer (+0x3280): from the first DME (+0x3210,
 * the flag cleared) when its source (+0x3248) is 1, otherwise the second (+0x322c, the flag
 * set); at the first constant (equal, or unordered) the text at +0x3514; otherwise kept to its
 * range (above the sixth constant, ordered, it; below the third, or unordered, it) and printed
 * through _ftol with +0x351c. Returns the third constant. */
static long double __stdcall bkraddme_11a0(double value)
{
    long double speed;
    (void)value;
    if (GD32(0x3248) == 1) {
        speed = GD64(0x3210);
        GD32(0x3568) = 0;
    } else {
        speed = GD64(0x322c);
        GD32(0x3568) = 1;
    }
    if (GAUGE_TEST(speed, GR64(0x2000), GAUGE_C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x3280), (const char *)GAUGE_D(0x3514));
        return GR64(0x2010);
    }
    if (!GAUGE_TEST(speed, GR64(0x2028), GAUGE_C0C3)) speed = GR64(0x2028);
    else if (GAUGE_TEST(speed, GR64(0x2010), GAUGE_C0)) speed = GR64(0x2010);
    wsprintfA((char *)(uintptr_t)GD32(0x3280), (const char *)GAUGE_D(0x351c), (uint32_t)gauge_ftol(speed));
    return GR64(0x2010);
}

/* 0x1250: the hold light: the flag set the seventh constant, clear the third */
static long double __stdcall bkraddme_1250(uint8_t *element)
{
    (void)element;
    return GD32(0x3568) ? GR64(0x2030) : GR64(0x2010);
}
GAUGE_INSTALL(bkraddme_install)   /* 0x1270 */
GAUGE_INITIALIZE(bkraddme_initialize)   /* 0x12c0 */
GAUGE_UPDATE(bkraddme_update)   /* 0x12d0 */
GAUGE_GENERATE(bkraddme_generate)   /* 0x12e0 */
GAUGE_DRAW(bkraddme_draw)   /* 0x1300 */
GAUGE_KILL(bkraddme_kill)   /* 0x1310 */
GAUGE_MODULE(bkraddme_module)   /* 0x1340 */

GAUGE_QUERY(bkraddme_1350)   /* 0x1350 (the _ftol import thunk at 0x1370 follows it) */

GAUGE_ROUTINES({0x1020, (void *)bkraddme_1020}, {0x1090, (void *)bkraddme_1090}, {0x11a0, (void *)bkraddme_11a0}, {0x1250, (void *)bkraddme_1250}, {0x1270, (void *)bkraddme_install}, {0x12c0, (void *)bkraddme_initialize}, {0x12d0, (void *)bkraddme_update}, {0x12e0, (void *)bkraddme_generate}, {0x1300, (void *)bkraddme_draw}, {0x1310, (void *)bkraddme_kill}, {0x1340, (void *)bkraddme_module}, {0x1350, (void *)bkraddme_1350})
