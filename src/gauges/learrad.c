/* LEARRAD.GAU -- LEAR_45.RADIO_STACK: the radio stack: the knob's card and the frequencies as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x4168u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x3a28u
#define GAUGE_MOUSE 0x3a30u
#define GAUGE_MOUSE_ARGUMENT 0x3a14u


/* 0x1020: the card: the variable offset */
static long double __stdcall learrad_1020(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2000);
}

/* 0x1030: a radio's text callback: the frequency (+0x3320, four BCD digits) printed into the
 * element's buffer (+0x3390) with the format at +0x413c; returns the frequency */
static long double __stdcall learrad_1030(double value)
{
    uint32_t frequency = GD32(0x3320);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3390), (const char *)GAUGE_D(0x413c),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x1090: a radio's text callback: the frequency (+0x3480, four BCD digits) printed into the
 * element's buffer (+0x34f0) with the format at +0x413c; returns the frequency */
static long double __stdcall learrad_1090(double value)
{
    uint32_t frequency = GD32(0x3480);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x34f0), (const char *)GAUGE_D(0x413c),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x10f0: a radio's text callback: the frequency (+0x35e0, four BCD digits) printed into the
 * element's buffer (+0x3650) with the format at +0x413c; returns the frequency */
static long double __stdcall learrad_10f0(double value)
{
    uint32_t frequency = GD32(0x35e0);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3650), (const char *)GAUGE_D(0x413c),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x1150: the ADF's text callback: the frequency's three BCD digits (+0x3748) and, while the
 * tenths (+0x3780, through _ftol) are not zero, the standby's two (+0x3764) after a point
 * (otherwise three spaces), printed into the element's buffer (+0x37b8) with the format at
 * +0x4148; returns the frequency word */
static long double __stdcall learrad_1150(double value)
{
    uint32_t standby = GD32(0x3764), frequency = GD32(0x3748);
    uint8_t ones = (uint8_t)((uint8_t)(frequency & 0xf) + 0x30);
    uint8_t tens = (uint8_t)((uint8_t)((frequency >> 4) & 0xf) + 0x30);
    uint8_t hundreds = (uint8_t)((uint8_t)((frequency >> 8) & 0xf) + 0x30);
    uint8_t first, point, second;
    (void)value;
    if ((uint32_t)gauge_ftol(GD64(0x3780))) {
        point = 0x2e;
        first = (uint8_t)((uint8_t)((standby >> 8) & 0xf) + 0x30);
        second = (uint8_t)((uint8_t)(standby & 0xf) + 0x30);
    } else {
        first = point = second = 0x20;
    }
    wsprintfA((char *)(uintptr_t)GD32(0x37b8), (const char *)GAUGE_D(0x4148), (int32_t)(int8_t)first, (int32_t)(int8_t)hundreds,
              (int32_t)(int8_t)tens, (int32_t)(int8_t)ones, (int32_t)(int8_t)point, (int32_t)(int8_t)second, 0);
    return (long double)(uint64_t)frequency;
}

/* 0x1210: a radio's text callback: the frequency (+0x38c0, four BCD digits) printed into the
 * element's buffer (+0x3930) with the format at +0x4158; returns the frequency */
static long double __stdcall learrad_1210(double value)
{
    uint32_t frequency = GD32(0x38c0);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3930), (const char *)GAUGE_D(0x4158),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}
GAUGE_INSTALL(learrad_install)   /* 0x1270 */
GAUGE_INITIALIZE(learrad_initialize)   /* 0x12c0 */
GAUGE_UPDATE(learrad_update)   /* 0x12d0 */
GAUGE_GENERATE(learrad_generate)   /* 0x12e0 */
GAUGE_DRAW(learrad_draw)   /* 0x1300 */
GAUGE_KILL(learrad_kill)   /* 0x1310 */
GAUGE_MODULE(learrad_module)   /* 0x1340 */

GAUGE_QUERY(learrad_1350)   /* 0x1350 (the _ftol import thunk at 0x1370 follows it) */

GAUGE_ROUTINES({0x1020, (void *)learrad_1020}, {0x1030, (void *)learrad_1030}, {0x1090, (void *)learrad_1090}, {0x10f0, (void *)learrad_10f0}, {0x1150, (void *)learrad_1150}, {0x1210, (void *)learrad_1210}, {0x1270, (void *)learrad_install}, {0x12c0, (void *)learrad_initialize}, {0x12d0, (void *)learrad_update}, {0x12e0, (void *)learrad_generate}, {0x1300, (void *)learrad_draw}, {0x1310, (void *)learrad_kill}, {0x1340, (void *)learrad_module}, {0x1350, (void *)learrad_1350})
