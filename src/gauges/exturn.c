/* EXTURN.GAU -- EXTRA-300.TURN-COORDINATOR: the turn coordinator */
#include "gauge.h"

#define GAUGE_IMPORTS 0x34c8u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3388u
#define GAUGE_MOUSE 0x3390u
#define GAUGE_MOUSE_ARGUMENT 0x3374u


/* 0x1020: the turn coordinator's flag: 0 the second constant, 1 the first.
 * RETAIL BUG (kept): GAUGE-TURN-EMPTY-RETURN (docs/retail-bugs.md): any other state returns
 * with nothing in st0, so the thunk loads only what the selector chose. */
uint32_t __stdcall __attribute__((used)) exturn_select(uint8_t *element, double *value)
{
    uint32_t state = P32(element, 0xc);
    if (state == 0) { *value = GR64(0x2008); return 1; }
    if (state == 1) { *value = GR64(0x2000); return 1; }
    return 0;
}
GAUGE_MAYBE_FP(exturn_1020, exturn_select)

/* 0x1050: the limit: the variable above the third constant (ordered) gives that constant,
 * below the fourth (or unordered) the fourth, otherwise the variable */
static long double __stdcall exturn_1050(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2010), GAUGE_C0C3)) return GR64(0x2010);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2018), GAUGE_C0)) return GR64(0x2018);
    return PF64(element, 0xc);
}
GAUGE_INSTALL(exturn_install)   /* 0x1090 */
GAUGE_INITIALIZE(exturn_initialize)   /* 0x10e0 */

/* 0x10f0, header +0x18: update: the elements, then the ball's deflection (+0x310c) negated
 * while it is above the constant (ordered) */
static void __stdcall exturn_10f0(void)
{
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
    if (!GAUGE_TEST(GD64(0x310c), GR64(0x2000), GAUGE_C0C3)) GD64(0x310c) = (double)gauge_fchs(GD64(0x310c));
}
GAUGE_GENERATE(exturn_generate)   /* 0x1130 */
GAUGE_DRAW(exturn_draw)   /* 0x1150 */
GAUGE_KILL(exturn_kill)   /* 0x1160 */
GAUGE_MODULE(exturn_module)   /* 0x1190 */
GAUGE_QUERY(exturn_query)   /* 0x11a0 */

GAUGE_ROUTINES({0x1020, (void *)exturn_1020}, {0x1050, (void *)exturn_1050}, {0x1090, (void *)exturn_install}, {0x10e0, (void *)exturn_initialize}, {0x10f0, (void *)exturn_10f0}, {0x1130, (void *)exturn_generate}, {0x1150, (void *)exturn_draw}, {0x1160, (void *)exturn_kill}, {0x1190, (void *)exturn_module}, {0x11a0, (void *)exturn_query})
