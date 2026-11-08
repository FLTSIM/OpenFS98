/* 737RADIO.GAU -- 737-400.RADIO-STACK: the radio stack: the knob's card and the frequencies as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x4328u
#define GAUGE_HEADER 0x3018u
#define GAUGE_ELEMENTS 0x3a30u
#define GAUGE_MOUSE 0x3a38u
#define GAUGE_MOUSE_ARGUMENT 0x3a1cu


/* 0x1020: the card: the variable offset */
static long double __stdcall g737radio_1020(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2000);
}

/* 0x1030: a radio's text callback: the frequency (+0x3328, four BCD digits) printed into the
 * element's buffer (+0x3398) with the format at +0x42fc; returns the frequency */
static long double __stdcall g737radio_1030(double value)
{
    uint32_t frequency = GD32(0x3328);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3398), (const char *)GAUGE_D(0x42fc),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x1090: a radio's text callback: the frequency (+0x3488, four BCD digits) printed into the
 * element's buffer (+0x34f8) with the format at +0x42fc; returns the frequency */
static long double __stdcall g737radio_1090(double value)
{
    uint32_t frequency = GD32(0x3488);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x34f8), (const char *)GAUGE_D(0x42fc),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x10f0: a radio's text callback: the frequency (+0x35e8, four BCD digits) printed into the
 * element's buffer (+0x3658) with the format at +0x42fc; returns the frequency */
static long double __stdcall g737radio_10f0(double value)
{
    uint32_t frequency = GD32(0x35e8);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3658), (const char *)GAUGE_D(0x42fc),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}

/* 0x1150: the ADF's text callback: the frequency's three BCD digits (+0x3750) and, while the
 * tenths (+0x3788, through _ftol) are not zero, the standby's two (+0x376c) after a point
 * (otherwise three spaces), printed into the element's buffer (+0x37c0) with the format at
 * +0x4308; returns the frequency word */
static long double __stdcall g737radio_1150(double value)
{
    uint32_t standby = GD32(0x376c), frequency = GD32(0x3750);
    uint8_t ones = (uint8_t)((uint8_t)(frequency & 0xf) + 0x30);
    uint8_t tens = (uint8_t)((uint8_t)((frequency >> 4) & 0xf) + 0x30);
    uint8_t hundreds = (uint8_t)((uint8_t)((frequency >> 8) & 0xf) + 0x30);
    uint8_t first, point, second;
    (void)value;
    if ((uint32_t)gauge_ftol(GD64(0x3788))) {
        point = 0x2e;
        first = (uint8_t)((uint8_t)((standby >> 8) & 0xf) + 0x30);
        second = (uint8_t)((uint8_t)(standby & 0xf) + 0x30);
    } else {
        first = point = second = 0x20;
    }
    wsprintfA((char *)(uintptr_t)GD32(0x37c0), (const char *)GAUGE_D(0x4308), (int32_t)(int8_t)first, (int32_t)(int8_t)hundreds,
              (int32_t)(int8_t)tens, (int32_t)(int8_t)ones, (int32_t)(int8_t)point, (int32_t)(int8_t)second, 0);
    return (long double)(uint64_t)frequency;
}

/* 0x1210: a radio's text callback: the frequency (+0x38c8, four BCD digits) printed into the
 * element's buffer (+0x3938) with the format at +0x4318; returns the frequency */
static long double __stdcall g737radio_1210(double value)
{
    uint32_t frequency = GD32(0x38c8);
    (void)value;
    wsprintfA((char *)(uintptr_t)GD32(0x3938), (const char *)GAUGE_D(0x4318),
              (frequency >> 12) & 0xf, (frequency >> 8) & 0xf, (frequency >> 4) & 0xf, frequency & 0xf);
    return (long double)(uint64_t)frequency;
}
GAUGE_INSTALL(g737radio_install)   /* 0x1270 */
GAUGE_INITIALIZE(g737radio_initialize)   /* 0x12c0 */
GAUGE_UPDATE(g737radio_update)   /* 0x12d0 */
GAUGE_GENERATE(g737radio_generate)   /* 0x12e0 */
GAUGE_DRAW(g737radio_draw)   /* 0x1300 */
GAUGE_KILL(g737radio_kill)   /* 0x1310 */
GAUGE_MODULE(g737radio_module)   /* 0x1340 */

GAUGE_QUERY(g737radio_1350)   /* 0x1350 (the _ftol import thunk at 0x1370 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737radio_1020}, {0x1030, (void *)g737radio_1030}, {0x1090, (void *)g737radio_1090}, {0x10f0, (void *)g737radio_10f0}, {0x1150, (void *)g737radio_1150}, {0x1210, (void *)g737radio_1210}, {0x1270, (void *)g737radio_install}, {0x12c0, (void *)g737radio_initialize}, {0x12d0, (void *)g737radio_update}, {0x12e0, (void *)g737radio_generate}, {0x1300, (void *)g737radio_draw}, {0x1310, (void *)g737radio_kill}, {0x1340, (void *)g737radio_module}, {0x1350, (void *)g737radio_1350})
