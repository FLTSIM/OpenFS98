/* Shared by the decompiled gauges (src/gauges/<name>.c), each built over its generated
 * image.inc (tools/gen_gauges.py: retail's .data and .rdata at their RVAs, the exports, and the
 * relocated pointers rebound at attach, the code ones through the gauge's gauge_routine).
 *
 * A gauge's code names retail's addresses by RVA: GD64(0x37a8) is the double retail keeps at
 * base + 0x37a8, GR64(0x2000) a constant. Its callbacks are stdcall C with retail's argument
 * bytes; one that leaves a value in st0 returns long double.
 *
 * x87 fidelity (the harness, tools/difftest/allgaugesdiff.c, compares the 80-bit result and all
 * six exception flags under every precision and rounding mode): a comparison is fcom's, which
 * raises invalid for a quiet NaN as well (gauge_fcom; C's own comparisons compile to fucomi),
 * and its result is read as retail's fnstsw/test reads it (GAUGE_C0 and the others). */
#ifndef OPENFS98_GAUGE_H
#define OPENFS98_GAUGE_H
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "image.inc"

#define GAUGE_D(rva) (gauge_data + (rva) - GAUGE_DATA_RVA)
#define GAUGE_R(rva) (gauge_rdata + (rva) - GAUGE_RDATA_RVA)
#define GD8(rva) (*(uint8_t *)GAUGE_D(rva))
#define GD16(rva) (*(uint16_t *)GAUGE_D(rva))
#define GDI16(rva) (*(int16_t *)GAUGE_D(rva))
#define GD32(rva) (*(uint32_t *)GAUGE_D(rva))
#define GDI32(rva) (*(int32_t *)GAUGE_D(rva))
#define GD64(rva) (*(double *)GAUGE_D(rva))
/* constants are read through volatile, as the loads retail makes: GCC would otherwise fold them at
 * compile time (ignoring the rounding mode: 1000 - 1000 is -0 rounding down) or narrow them to floats */
#define GR64(rva) (*(const volatile double *)GAUGE_R(rva))
#define GR32(rva) (*(const volatile float *)GAUGE_R(rva))
/* a field through a pointer */
#define P8(p, n) (*(uint8_t *)((uint8_t *)(p) + (n)))
#define P16(p, n) (*(uint16_t *)((uint8_t *)(p) + (n)))
#define PI16(p, n) (*(int16_t *)((uint8_t *)(p) + (n)))
#define P32(p, n) (*(uint32_t *)((uint8_t *)(p) + (n)))
#define PI32(p, n) (*(int32_t *)((uint8_t *)(p) + (n)))
#define PF64(p, n) (*(double *)((uint8_t *)(p) + (n)))
/* a double stored as retail's two immediate dword moves write it */
#define GAUGE_SET64(lvalue, low, high) \
    do { uint32_t words_[2] = {(low), (high)}; memcpy(&(lvalue), words_, 8); } while (0)

/* fcom: the status word's condition bits for left against right (C0 below or unordered, C3
 * equal or unordered, C2 unordered) */
static inline uint16_t gauge_fcom(long double left, long double right)
{
    uint16_t status;
    __asm__ volatile("fcom %%st(1); fnstsw %0" : "=am"(status) : "t"(left), "u"(right));
    return status;
}
#define GAUGE_C0 0x100u
#define GAUGE_C3 0x4000u
#define GAUGE_C0C3 0x4100u
/* retail's "fcomp b; fnstsw ax; test ah, mask; jne/je": the bits tested */
#define GAUGE_TEST(left, right, mask) (gauge_fcom((left), (right)) & (mask))

/* a value kept in st0 across C (retail copies through the x87: a signalling NaN comes out quiet) */
static inline long double gauge_x87(long double value)
{
    __asm__ volatile("" : "+t"(value));
    return value;
}

/* retail's "fst m64": the value stored as a double straight from st0 (a C conversion may store
 * and reload it, and reloading a denormal raises DE where retail raises nothing) */
static inline void gauge_fst(void *destination, long double value)
{
    __asm__ volatile("fstl %0" : "=m"(*(double *)destination) : "t"(value));
}

/* MSVCRT's _ftol: truncation through fistp qword under chop rounding (edx:eax) */
static inline int64_t gauge_ftol(long double value)
{
    uint16_t saved, control;
    int64_t result;
    __asm__ volatile("" : "+t"(value));
    __asm__ volatile("fnstcw %0" : "=m"(saved));
    control = saved | 0xc00;
    __asm__ volatile("fldcw %1; fistpll %0; fldcw %2"
                     : "=m"(result) : "m"(control), "m"(saved), "t"(value) : "st");
    return result;
}

/* the SDK gauges' clamp of a value kept in st0: "fcom high; test ah,0x41; jne" replaces a
 * value above high (ordered) with high, then "fcom low; test ah,0x1; je" one below low, or
 * unordered, with low (so a NaN comes out as low) */
static inline long double gauge_clamp(long double value, double high, double low)
{
    if (!GAUGE_TEST(value, high, GAUGE_C0C3)) value = high;
    if (GAUGE_TEST(value, low, GAUGE_C0)) value = low;
    return value;
}

/* the same with a "ret" after the high replacement: above high (ordered) returns high at once */
static inline long double gauge_limit(long double value, double high, double low)
{
    if (!GAUGE_TEST(value, high, GAUGE_C0C3)) return high;
    if (GAUGE_TEST(value, low, GAUGE_C0)) return low;
    return value;
}
/* the low bound first: below low (or unordered) low, then above high (ordered) high */
static inline long double gauge_clamp_low_first(long double value, double low, double high)
{
    if (GAUGE_TEST(value, low, GAUGE_C0)) value = low;
    if (!GAUGE_TEST(value, high, GAUGE_C0C3)) value = high;
    return value;
}

static inline long double gauge_fsin(long double value) { __asm__("fsin" : "+t"(value)); return value; }
static inline long double gauge_fcos(long double value) { __asm__("fcos" : "+t"(value)); return value; }
static inline long double gauge_fsqrt(long double value) { __asm__("fsqrt" : "+t"(value)); return value; }
static inline long double gauge_fchs(long double value) { __asm__("fchs" : "+t"(value)); return value; }
static inline long double gauge_fabs(long double value) { __asm__("fabs" : "+t"(value)); return value; }

/* PANELS: the ImportTable's one link (+4 past the import's module id), a Linkage of stdcall
 * services */
#define GAUGE_PANELS(import_rva) ((const uint32_t *)(uintptr_t)GD32((import_rva) + 4))
#define GAUGE_SERVICE(import_rva, offset, type) ((type)(uintptr_t)GAUGE_PANELS(import_rva)[(offset) / 4])

/* The gauge header's callbacks as the SDK's header macros wrote them: each hands the element
 * list to PANELS. A gauge defines GAUGE_IMPORTS (its ImportTable), GAUGE_HEADER, GAUGE_ELEMENTS
 * (the list's pointer), GAUGE_MOUSE (its mouse rectangles) and GAUGE_MOUSE_ARGUMENT (a pointer
 * whose +4 the mouse install takes), then names each callback it has the standard form of.
 * PANELS' services by offset: 0x60 query, 0x64 install, 0x68 initialise, 0x6c update, 0x70
 * generate, 0x74 draw, 0x7c kill, 0x80 mouse install, 0x84 mouse register, 0x88 mouse
 * unregister (each reads after the last: a service may move what the next one reads). */
typedef uint32_t (__stdcall *gauge_service0)(void);
typedef uint32_t (__stdcall *gauge_service1)(uint32_t);
typedef uint32_t (__stdcall *gauge_service2)(uint32_t, uint32_t);
typedef uint32_t (__stdcall *gauge_service3)(uint32_t, uint32_t, uint32_t);
#define GAUGE_PANELS_CALL(offset, arity) GAUGE_SERVICE(GAUGE_IMPORTS, offset, gauge_service##arity)
#define GAUGE_ADDRESS(rva) ((uint32_t)(uintptr_t)GAUGE_D(rva))
#define GAUGE_INSTALL(name) \
    static void __stdcall name(uint32_t context) \
    { \
        GAUGE_PANELS_CALL(0x64, 2)(GD32(GAUGE_ELEMENTS), context); \
        GAUGE_PANELS_CALL(0x80, 3)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER), GD32(GAUGE_MOUSE_ARGUMENT) + 4); \
        GAUGE_PANELS_CALL(0x84, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER)); \
    }
#define GAUGE_INITIALIZE(name) static void __stdcall name(void) { GAUGE_PANELS_CALL(0x68, 1)(GD32(GAUGE_ELEMENTS)); }
#define GAUGE_UPDATE(name) static void __stdcall name(void) { GAUGE_PANELS_CALL(0x6c, 1)(GD32(GAUGE_ELEMENTS)); }
#define GAUGE_GENERATE(name) \
    static void __stdcall name(uint32_t phase) { GAUGE_PANELS_CALL(0x70, 2)(GD32(GAUGE_ELEMENTS), phase); }
#define GAUGE_DRAW(name) static void __stdcall name(void) { GAUGE_PANELS_CALL(0x74, 1)(GD32(GAUGE_ELEMENTS)); }
#define GAUGE_KILL(name) \
    static void __stdcall name(void) \
    { \
        GAUGE_PANELS_CALL(0x88, 2)(GAUGE_ADDRESS(GAUGE_MOUSE), GAUGE_ADDRESS(GAUGE_HEADER)); \
        GAUGE_PANELS_CALL(0x7c, 1)(GD32(GAUGE_ELEMENTS)); \
    }
/* Linkage +0x04 and +0x08: nothing */
#define GAUGE_MODULE(name) static void __stdcall name(void) {}
/* header +0x0c: the header's element list's first element */
#define GAUGE_QUERY(name) \
    static void __stdcall name(void) { GAUGE_PANELS_CALL(0x60, 1)(P32((uintptr_t)GD32(GAUGE_HEADER + 8), 0)); }

/* A stdcall callback of one pointer that leaves a value in st0 only sometimes (retail returns
 * with the x87 stack empty on its other paths), which C cannot say: name is a thunk around
 * select(argument, &value), which answers whether to load value. */
#define GAUGE_MAYBE_FP(name, select) \
    __asm__(".text\n_" #name "@4:\n" \
            "    subl $8, %esp\n" \
            "    pushl %esp\n" \
            "    pushl 16(%esp)\n" \
            "    call _" #select "@8\n" \
            "    testl %eax, %eax\n" \
            "    je 1f\n" \
            "    fldl (%esp)\n" \
            "1:  addl $8, %esp\n" \
            "    ret $4\n"); \
    void __stdcall name(uint8_t *element);

/* the code RVAs the image's relocated pointers name, each a routine of the gauge's */
struct gauge_routine_entry { uint32_t rva; void *routine; };
#define GAUGE_ROUTINES(...) \
    static const struct gauge_routine_entry gauge_routines_[] = {__VA_ARGS__}; \
    static void *gauge_routine(uint32_t rva) \
    { \
        unsigned at_; \
        for (at_ = 0; at_ < sizeof gauge_routines_ / sizeof gauge_routines_[0]; ++at_) \
            if (gauge_routines_[at_].rva == rva) return gauge_routines_[at_].routine; \
        return 0; \
    }
#endif
