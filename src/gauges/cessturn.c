/* CESSTURN.GAU -- CESSNA_182.TURN_COORDINATOR: the turn coordinator */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3428u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3398u
#define GAUGE_MOUSE 0x33a0u
#define GAUGE_MOUSE_ARGUMENT 0x3384u


/* 0x1020: the turn coordinator's flag: 0 the second constant, 1 the first.
 * RETAIL BUG (kept): GAUGE-TURN-EMPTY-RETURN (docs/retail-bugs.md): any other state returns
 * with nothing in st0, so the thunk loads only what the selector chose. */
uint32_t __stdcall __attribute__((used)) cessturn_select(uint8_t *element, double *value)
{
    uint32_t state = P32(element, 0xc);
    if (state == 0) { *value = GR64(0x2008); return 1; }
    if (state == 1) { *value = GR64(0x2000); return 1; }
    return 0;
}
GAUGE_MAYBE_FP(cessturn_1020, cessturn_select)

/* 0x1050: the needle: the value limited */
static long double __stdcall cessturn_1050(double value)
{
    return gauge_limit(value, GR64(0x2010), GR64(0x2018));
}
GAUGE_INSTALL(cessturn_install)   /* 0x1090 */
GAUGE_INITIALIZE(cessturn_initialize)   /* 0x10e0 */

/* 0x10f0, header +0x18: update: the elements, then the ball's deflection (+0x326c) negated
 * while it is above the constant (ordered) */
static void __stdcall cessturn_10f0(void)
{
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
    if (!GAUGE_TEST(GD64(0x326c), GR64(0x2008), GAUGE_C0C3)) GD64(0x326c) = (double)gauge_fchs(GD64(0x326c));
}
GAUGE_GENERATE(cessturn_generate)   /* 0x1130 */
GAUGE_DRAW(cessturn_draw)   /* 0x1150 */
GAUGE_KILL(cessturn_kill)   /* 0x1160 */
GAUGE_MODULE(cessturn_module)   /* 0x1190 */
GAUGE_QUERY(cessturn_query)   /* 0x11a0 */

GAUGE_ROUTINES({0x1020, (void *)cessturn_1020}, {0x1050, (void *)cessturn_1050}, {0x1090, (void *)cessturn_install}, {0x10e0, (void *)cessturn_initialize}, {0x10f0, (void *)cessturn_10f0}, {0x1130, (void *)cessturn_generate}, {0x1150, (void *)cessturn_draw}, {0x1160, (void *)cessturn_kill}, {0x1190, (void *)cessturn_module}, {0x11a0, (void *)cessturn_query})
