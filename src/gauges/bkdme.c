/* BKDME.GAU -- BENDIX_KING.DME: the DME: its source switch, distance, speed, time to station and the hold light */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3a20u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x38d8u
#define GAUGE_MOUSE 0x38e0u
#define GAUGE_MOUSE_ARGUMENT 0x38c4u


/* 0x1020: the source switch's mouse callback: event 0x10021, then by the DME's flag (+0x3a58)
 * 0x100fc (setting it) or 0x100fb (clearing it), each with 0 (PANELS +0x90). Returns 1. */
static uint32_t __stdcall bkdme_1020(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GAUGE_PANELS_CALL(0x90, 2)(0x10021, 0);
    if (!GD32(0x3a58)) {
        GAUGE_PANELS_CALL(0x90, 2)(0x100fc, 0);
        GD32(0x3a58) = 1;
    } else {
        GAUGE_PANELS_CALL(0x90, 2)(0x100fb, 0);
        GD32(0x3a58) = 0;
    }
    return 1;
}

/* 0x1090: the distance as text into the element's buffer (+0x3148): from the first DME (+0x30d8,
 * the flag cleared) when the source (+0x3110) is 1, otherwise the second (+0x30f4, the flag set);
 * at the first constant (equal, or unordered) the text at +0x39e8; otherwise kept to its range
 * (above the second constant, ordered, it; below the third, or unordered, it), above the fourth
 * (ordered) printed whole with +0x39f8, otherwise as whole and tenths (the fraction times the
 * fifth constant, each through _ftol) with +0x39f0. Returns the distance as retail parks it in
 * the argument's slot. */
static long double __stdcall bkdme_1090(double value)
{
    long double distance;
    uint16_t blank;
    if (GD32(0x3110) == 1) {
        distance = GD64(0x30d8);
        GD32(0x3a58) = 0;
    } else {
        distance = GD64(0x30f4);
        GD32(0x3a58) = 1;
    }
    blank = gauge_fcom(distance, GR64(0x2000));
    gauge_fst(&value, distance);     /* the fst into the argument's slot, kept (its flags) */
    if (blank & GAUGE_C3) {
        wsprintfA((char *)(uintptr_t)GD32(0x3148), (const char *)GAUGE_D(0x39e8));
        return value;
    }
    if (!GAUGE_TEST(distance, GR64(0x2008), GAUGE_C0C3)) value = (double)(distance = GR64(0x2008));
    else if (GAUGE_TEST(distance, GR64(0x2010), GAUGE_C0)) value = (double)(distance = GR64(0x2010));
    if (!GAUGE_TEST(distance, GR64(0x2018), GAUGE_C0C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x3148), (const char *)GAUGE_D(0x39f8), (uint32_t)gauge_ftol(distance));
    } else {
        uint32_t whole = (uint32_t)gauge_ftol(distance);
        uint32_t part = (uint32_t)gauge_ftol((distance - (long double)(int32_t)whole) * GR64(0x2020));
        wsprintfA((char *)(uintptr_t)GD32(0x3148), (const char *)GAUGE_D(0x39f0), whole, part);
    }
    return value;
}

/* 0x11a0: the speed as text into the element's buffer (+0x33b8): from the first DME (+0x3348,
 * the flag cleared) when its source (+0x3380) is 1, otherwise the second (+0x3364, the flag set);
 * at the first constant (equal, or unordered) the text at +0x39e8; otherwise kept to [0, 999]
 * (above the sixth constant, ordered, 999; below the third, or unordered, 0), printed through
 * _ftol with +0x3a00. Returns the speed as parked in the argument's slot. */
static long double __stdcall bkdme_11a0(double value)
{
    if (GD32(0x3380) == 1) {
        value = GD64(0x3348);
        GD32(0x3a58) = 0;
    } else {
        GD32(0x3a58) = 1;
        value = GD64(0x3364);
    }
    if (GAUGE_TEST(value, GR64(0x2000), GAUGE_C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x33b8), (const char *)GAUGE_D(0x39e8));
        return value;
    }
    if (!GAUGE_TEST(value, GR64(0x2028), GAUGE_C0C3)) value = 999.0;
    else if (GAUGE_TEST(value, GR64(0x2010), GAUGE_C0)) value = 0.0;
    wsprintfA((char *)(uintptr_t)GD32(0x33b8), (const char *)GAUGE_D(0x3a00), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1280: the time to station as text into the element's buffer (+0x3628): the distance over the
 * speed times the seventh constant (the DME the flag names), unless the scaled speed is at the
 * third constant or below it (or unordered); then, or above the eighth constant (ordered), 99.
 * Printed through _ftol with +0x3a08; returns the time as parked. */
static long double __stdcall bkdme_1280(double value)
{
    long double distance, speed;
    if (!GD32(0x3a58)) { distance = GD64(0x30d8); speed = GD64(0x3348); }
    else { distance = GD64(0x30f4); speed = GD64(0x3364); }
    speed = speed * GR64(0x2030);
    if (GAUGE_TEST(speed, GR64(0x2010), GAUGE_C3) || GAUGE_TEST(speed, GR64(0x2010), GAUGE_C0)) value = 99.0;
    else {
        value = (double)(distance / speed);
        if (!GAUGE_TEST(value, GR64(0x2038), GAUGE_C0C3)) value = 99.0;
    }
    wsprintfA((char *)(uintptr_t)GD32(0x3628), (const char *)GAUGE_D(0x3a08), (uint32_t)gauge_ftol(value));
    return value;
}

/* 0x1320: a fixed text (the format at +0x3a10) into the element's buffer (+0x3280); returns the value */
static long double __stdcall bkdme_1320(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x3280), (const char *)GAUGE_D(0x3a10));
    return value;
}

/* 0x1340: a fixed text (the format at +0x3a14) into the element's buffer (+0x34f0); returns the value */
static long double __stdcall bkdme_1340(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x34f0), (const char *)GAUGE_D(0x3a14));
    return value;
}

/* 0x1360: a fixed text (the format at +0x3a18) into the element's buffer (+0x3760); returns the value */
static long double __stdcall bkdme_1360(double value)
{
    wsprintfA((char *)(uintptr_t)GD32(0x3760), (const char *)GAUGE_D(0x3a18));
    return value;
}

/* 0x1380: the hold light: the flag set the ninth constant, clear the third */
static long double __stdcall bkdme_1380(uint8_t *element)
{
    (void)element;
    return GD32(0x3a58) ? GR64(0x2040) : GR64(0x2010);
}
GAUGE_INSTALL(bkdme_install)   /* 0x13a0 */
GAUGE_INITIALIZE(bkdme_initialize)   /* 0x13f0 */
GAUGE_UPDATE(bkdme_update)   /* 0x1400 */
GAUGE_GENERATE(bkdme_generate)   /* 0x1410 */
GAUGE_DRAW(bkdme_draw)   /* 0x1430 */
GAUGE_KILL(bkdme_kill)   /* 0x1440 */
GAUGE_MODULE(bkdme_module)   /* 0x1470 */

GAUGE_QUERY(bkdme_1480)   /* 0x1480 (the _ftol import thunk at 0x14a0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)bkdme_1020}, {0x1090, (void *)bkdme_1090}, {0x11a0, (void *)bkdme_11a0}, {0x1280, (void *)bkdme_1280}, {0x1320, (void *)bkdme_1320}, {0x1340, (void *)bkdme_1340}, {0x1360, (void *)bkdme_1360}, {0x1380, (void *)bkdme_1380}, {0x13a0, (void *)bkdme_install}, {0x13f0, (void *)bkdme_initialize}, {0x1400, (void *)bkdme_update}, {0x1410, (void *)bkdme_generate}, {0x1430, (void *)bkdme_draw}, {0x1440, (void *)bkdme_kill}, {0x1470, (void *)bkdme_module}, {0x1480, (void *)bkdme_1480})
