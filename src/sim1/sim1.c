/* SIM1.sim -- module 25, partial services/state lifecycle. Not installable.
 * The mapped state follows retail .data (0x22016000, virtual size 0x43f8).
 * Unrecovered decoder/frame entries fail immediately rather than silently
 * behaving as no-ops. Unused pointers in extracted state remain reference VAs;
 * only the explicitly recovered services below may read this partial image.
 */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fs98/linkage.h"

#define SIM1_DATA_SIZE 0x43f8u

void sim1_descriptor(void), sim1_word(void), sim1_scaled(void);
void sim1_vector(void), sim1_reset(void), sim1_wind(void);
void sim1_deinit(void), sim1_record_write(void), sim1_save_situation(void), sim1_retain_state(void);
void sim1_chain_tick(void), sim1_chain_frame(void), sim1_situation_callback(void);
void sim1_init(void);

void sim1_unsupported(void)
{
    RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION, EXCEPTION_NONCONTINUABLE, 0, NULL);
    ExitProcess(3);
}

struct sim1_state {
    struct fs98_import imports[11];
    uintptr_t linkage[15];
    uint8_t data[SIM1_DATA_SIZE - 0x94];
};

struct sim1_state sim1_image = {
    {{1, 0}, {2, 0}, {23, 0}, {4, 0}, {16, 0}, {3, 0},
     {32, 0}, {39, 0}, {20, 0}, {10, 0}, {0, 0}},
    {25, (uintptr_t)sim1_init, (uintptr_t)sim1_deinit, 0, 0, 0,
     (uintptr_t)sim1_descriptor, (uintptr_t)sim1_unsupported,
     (uintptr_t)((uint8_t *)&sim1_image + 0x1150),
     (uintptr_t)((uint8_t *)&sim1_image + 0x1158),
     (uintptr_t)sim1_word, (uintptr_t)sim1_scaled, (uintptr_t)sim1_vector,
     (uintptr_t)sim1_reset, (uintptr_t)sim1_wind},
    {
#include "sim1_data.inc"
    }
};

typedef char sim1_layout_check[(sizeof(struct sim1_state) == SIM1_DATA_SIZE &&
                              offsetof(struct sim1_state, linkage) == 0x58 &&
                              offsetof(struct sim1_state, data) == 0x94) ? 1 : -1];

#define D ((uint8_t *)&sim1_image)
__asm__(".globl _ImportTable\n.set _ImportTable, _sim1_image\n"
        ".globl _Linkage\n.set _Linkage, _sim1_image + 0x58\n");

/* +0x18, 0x2200ac0d: rep movsb, including forward overlap behavior. */
void __stdcall sim1_descriptor_core(uint8_t *out)
{
    uint32_t i;
    volatile uint8_t *source = D + 0x1730;
    volatile uint8_t *destination = out;
    for (i = 0; i < 0x2c; ++i)
        destination[i] = source[i];
}

/* +0x28, 0x2200ac2f: one WORD, leaving the upper half of EAX intact. */
uint32_t __stdcall sim1_word_core(void *out)
{
    uint16_t value;
    memcpy(&value, D + 0x1518, sizeof value);
    memcpy(out, &value, sizeof value);
    return value;
}

/* +0x2c, 0x220117c0: signed integer / 32768, rounded on the output store. */
int32_t __stdcall sim1_scaled_core(void *out)
{
    static const double scale = 0x1p-15;
    __asm__ volatile("fildl %1\n\tfmull %2\n\tfstpl (%0)"
        : : "r"(out), "m"(*(int32_t *)(D + 0x17a4)), "m"(scale) : "st", "memory");
    return 0;
}

/* +0x30, 0x220117e0: ordered x87 loads/stores, not memcpy (NaNs quiet). */
int32_t __stdcall sim1_vector_core(void *a, void *b, void *c)
{
    __asm__ volatile("fldl %3\n\tfstpl (%0)\n\tfldl %4\n\tfstpl (%1)\n\t"
                     "fldl %5\n\tfstpl (%2)"
        : : "r"(a), "r"(b), "r"(c), "m"(*(double *)(D + 0x13c8)),
            "m"(*(double *)(D + 0x13b8)), "m"(*(double *)(D + 0x13c0))
        : "st", "memory");
    return 0;
}

/* +0x34, 0x22011750: unsigned conversion and exact retail x87 ordering. */
uint32_t __stdcall sim1_reset_core(uint32_t value)
{
    int64_t wide = (uint64_t)value;
    static const double zero = 0.0;
    __asm__ volatile("fildq %0\n\tfldl %1\n\tfldl %1\n\tfxch %%st(2)\n\t"
        "fmull %2\n\tfldl %1\n\tfldl %1\n\tfldl %1\n\tfxch %%st(3)\n\t"
        "fstpl %3\n\tfxch %%st(3)\n\tfstpl %4\n\tfxch %%st(3)\n\t"
        "fstpl %5\n\tfxch %%st(2)\n\tfstpl %6\n\tfstpl %7\n\tfstpl %8"
        : : "m"(wide), "m"(zero), "m"(*(double *)(D + 0x740)),
            "m"(*(double *)(D + 0x13b0)), "m"(*(double *)(D + 0x13a0)),
            "m"(*(double *)(D + 0x13a8)), "m"(*(double *)(D + 0x13e8)),
            "m"(*(double *)(D + 0x13f0)), "m"(*(double *)(D + 0x13f8))
        : "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "memory");
    return value;
}

/* +0x38, 0x220096b8: leaves one x87 result. */
double sim1_wind_core(void)
{
    double value;
    memcpy(&value, D + 0x640, sizeof value);
    return value;
}

#include "sim1_state.inc"
#include "sim1_air.inc"
