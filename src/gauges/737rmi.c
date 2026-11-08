/* 737RMI.GAU -- 737-400.RMI: the RMI's cards and the DME as text */
#include "gauge.h"

#define GAUGE_IMPORTS 0x36e8u
#define GAUGE_HEADER 0x3010u
#define GAUGE_ELEMENTS 0x35a8u
#define GAUGE_MOUSE 0x35b0u
#define GAUGE_MOUSE_ARGUMENT 0x3594u


/* 0x1020: a card: the value offset */
static long double __stdcall g737rmi_1020(double value)
{
    return (long double)value - GR64(0x2000);
}

/* 0x1030: a card: the variable offset */
static long double __stdcall g737rmi_1030(uint8_t *element)
{
    return (long double)PF64(element, 0xc) - GR64(0x2000);
}

/* 0x1040: the DME window, into the element's buffer (+0x34c8). With the mode (+0x3490) at the
 * second constant (equal, or unordered): the speed (+0x3474), unless it is at the third
 * constant (equal or unordered: the text at +0x36d4), kept to [0, 999] in place and printed
 * through _ftol with +0x36dc. Otherwise the distance (+0x3458): at the third constant (equal
 * or unordered) the text at +0x36b8; else kept to [0, 9999] in place, above the seventh
 * constant (ordered) printed whole with +0x36cc, otherwise as whole and tenths (the fraction
 * times the eighth constant, each through _ftol) with +0x36c0. Returns the value. */
static long double __stdcall g737rmi_1040(double value)
{
    char *text = (char *)(uintptr_t)GD32(0x34c8);
    if (GAUGE_TEST(GD64(0x3490), GR64(0x2008), GAUGE_C3)) {
        if (GAUGE_TEST(GD64(0x3474), GR64(0x2010), GAUGE_C3)) {
            wsprintfA((char *)(uintptr_t)GD32(0x34c8), (const char *)GAUGE_D(0x36d4));
            return value;
        }
        if (!GAUGE_TEST(GD64(0x3474), GR64(0x2018), GAUGE_C0C3)) GD64(0x3474) = 999.0;
        else if (GAUGE_TEST(GD64(0x3474), GR64(0x2020), GAUGE_C0)) GD64(0x3474) = 0.0;
        wsprintfA(text, (const char *)GAUGE_D(0x36dc), (uint32_t)gauge_ftol(GD64(0x3474)));
        return value;
    }
    if (GAUGE_TEST(GD64(0x3458), GR64(0x2010), GAUGE_C3)) {
        wsprintfA((char *)(uintptr_t)GD32(0x34c8), (const char *)GAUGE_D(0x36b8));
        return value;
    }
    if (!GAUGE_TEST(GD64(0x3458), GR64(0x2028), GAUGE_C0C3)) GD64(0x3458) = 9999.0;
    else if (GAUGE_TEST(GD64(0x3458), GR64(0x2020), GAUGE_C0)) GD64(0x3458) = 0.0;
    if (!GAUGE_TEST(GD64(0x3458), GR64(0x2030), GAUGE_C0C3)) {
        uint32_t whole = (uint32_t)gauge_ftol(GD64(0x3458));
        wsprintfA((char *)(uintptr_t)GD32(0x34c8), (const char *)GAUGE_D(0x36cc), whole);
    } else {
        uint32_t whole = (uint32_t)gauge_ftol(GD64(0x3458));
        uint32_t part = (uint32_t)gauge_ftol(((long double)GD64(0x3458) - (long double)(int32_t)whole) * GR64(0x2038));
        wsprintfA((char *)(uintptr_t)GD32(0x34c8), (const char *)GAUGE_D(0x36c0), whole, part);
    }
    return value;
}
GAUGE_INSTALL(g737rmi_install)   /* 0x11f0 */
GAUGE_INITIALIZE(g737rmi_initialize)   /* 0x1240 */
GAUGE_UPDATE(g737rmi_update)   /* 0x1250 */
GAUGE_GENERATE(g737rmi_generate)   /* 0x1260 */
GAUGE_DRAW(g737rmi_draw)   /* 0x1280 */
GAUGE_KILL(g737rmi_kill)   /* 0x1290 */
GAUGE_MODULE(g737rmi_module)   /* 0x12c0 */

GAUGE_QUERY(g737rmi_12d0)   /* 0x12d0 (the _ftol import thunk at 0x12f0 follows it) */

GAUGE_ROUTINES({0x1020, (void *)g737rmi_1020}, {0x1030, (void *)g737rmi_1030}, {0x1040, (void *)g737rmi_1040}, {0x11f0, (void *)g737rmi_install}, {0x1240, (void *)g737rmi_initialize}, {0x1250, (void *)g737rmi_update}, {0x1260, (void *)g737rmi_generate}, {0x1280, (void *)g737rmi_draw}, {0x1290, (void *)g737rmi_kill}, {0x12c0, (void *)g737rmi_module}, {0x12d0, (void *)g737rmi_12d0})
