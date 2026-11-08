/* LEARWARN.GAU -- LEAR_45.WARNING_ENUNCIATORS: the warning test light: its state and its click */
#include "gauge.h"

#define GAUGE_IMPORTS 0x3638u
#define GAUGE_HEADER 0x3020u
#define GAUGE_ELEMENTS 0x3528u
#define GAUGE_MOUSE 0x3530u
#define GAUGE_MOUSE_ARGUMENT 0x3514u


/* 0x1020: the light: the test state (+0x3670) less one */
static long double __stdcall learwarn_1020(uint8_t *element)
{
    (void)element;
    return (long double)(int32_t)(GD32(0x3670) - 1);
}

/* 0x1040: the mouse callback (point, flags): the test state toggled; returns 0 */
static uint32_t __stdcall learwarn_1040(uint32_t point, uint32_t flags)
{
    (void)point; (void)flags;
    GD32(0x3670) = GD32(0x3670) == 0;
    return 0;
}
GAUGE_INSTALL(learwarn_install)   /* 0x1060 */
GAUGE_INITIALIZE(learwarn_initialize)   /* 0x10b0 */
GAUGE_UPDATE(learwarn_update)   /* 0x10c0 */
GAUGE_GENERATE(learwarn_generate)   /* 0x10d0 */
GAUGE_DRAW(learwarn_draw)   /* 0x10f0 */
GAUGE_KILL(learwarn_kill)   /* 0x1100 */
GAUGE_MODULE(learwarn_module)   /* 0x1130 */
GAUGE_QUERY(learwarn_query)   /* 0x1140 */

GAUGE_ROUTINES({0x1020, (void *)learwarn_1020}, {0x1040, (void *)learwarn_1040}, {0x1060, (void *)learwarn_install}, {0x10b0, (void *)learwarn_initialize}, {0x10c0, (void *)learwarn_update}, {0x10d0, (void *)learwarn_generate}, {0x10f0, (void *)learwarn_draw}, {0x1100, (void *)learwarn_kill}, {0x1130, (void *)learwarn_module}, {0x1140, (void *)learwarn_query})
