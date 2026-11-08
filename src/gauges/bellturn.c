/* BELLTURN.GAU -- BELL_206B.TURN_INDICATOR: the turn coordinator */
#include "gauge.h"

#define GAUGE_IMPORTS 0x33f8u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3368u
#define GAUGE_MOUSE 0x3370u
#define GAUGE_MOUSE_ARGUMENT 0x3354u


/* 0x1020: the turn coordinator's flag: 0 the second constant, 1 the first.
 * RETAIL BUG (kept): GAUGE-TURN-EMPTY-RETURN (docs/retail-bugs.md): any other state returns
 * with nothing in st0, so the thunk loads only what the selector chose. */
uint32_t __stdcall __attribute__((used)) bellturn_select(uint8_t *element, double *value)
{
    uint32_t state = P32(element, 0xc);
    if (state == 0) { *value = GR64(0x2008); return 1; }
    if (state == 1) { *value = GR64(0x2000); return 1; }
    return 0;
}
GAUGE_MAYBE_FP(bellturn_1020, bellturn_select)

/* 0x1050: the limit: the variable above the first constant (ordered) gives that constant,
 * below the second (or unordered) the second, otherwise the variable */
static long double __stdcall bellturn_1050(uint8_t *element)
{
    if (!GAUGE_TEST(PF64(element, 0xc), GR64(0x2010), GAUGE_C0C3)) return GR64(0x2010);
    if (GAUGE_TEST(PF64(element, 0xc), GR64(0x2018), GAUGE_C0)) return GR64(0x2018);
    return PF64(element, 0xc);
}
GAUGE_INSTALL(bellturn_install)   /* 0x1090 */
GAUGE_INITIALIZE(bellturn_initialize)   /* 0x10e0 */

/* 0x10f0, header +0x18: update: the elements, then the ball's deflection (+0x3194) negated
 * while it is above the second constant (ordered) */
static void __stdcall bellturn_10f0(void)
{
    GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS));
    if (!GAUGE_TEST(GD64(0x3194), GR64(0x2008), GAUGE_C0C3)) GD64(0x3194) = (double)gauge_fchs(GD64(0x3194));
}
GAUGE_GENERATE(bellturn_generate)   /* 0x1130 */
GAUGE_DRAW(bellturn_draw)   /* 0x1150 */
GAUGE_KILL(bellturn_kill)   /* 0x1160 */
GAUGE_MODULE(bellturn_module)   /* 0x1190 */
GAUGE_QUERY(bellturn_query)   /* 0x11a0 */

GAUGE_ROUTINES({0x1020, (void *)bellturn_1020}, {0x1050, (void *)bellturn_1050}, {0x1090, (void *)bellturn_install}, {0x10e0, (void *)bellturn_initialize}, {0x10f0, (void *)bellturn_10f0}, {0x1130, (void *)bellturn_generate}, {0x1150, (void *)bellturn_draw}, {0x1160, (void *)bellturn_kill}, {0x1190, (void *)bellturn_module}, {0x11a0, (void *)bellturn_query})
