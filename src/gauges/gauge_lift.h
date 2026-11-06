/* Shared semantics for the explicit C transliterations emitted by
 * tools/gen_gauges.py. There is no instruction decoder or bytecode runner:
 * generated functions contain C expressions, calls and labelled branches.
 * Keep 32-bit register widths and x87 intermediate precision at each step. */
#ifndef OPENFS98_GAUGE_LIFT_H
#define OPENFS98_GAUGE_LIFT_H
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

struct gauge_frame {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp;
    uint32_t flags, fp_status;
    unsigned depth;
    long double fp[8];
    uint32_t stack[256];
};

static uint8_t gauge_u8(uint32_t address) { return *(uint8_t *)(uintptr_t)address; }
static uint16_t gauge_u16(uint32_t address) { uint16_t value; memcpy(&value, (void *)(uintptr_t)address, 2); return value; }
static uint32_t gauge_u32(uint32_t address) { uint32_t value; memcpy(&value, (void *)(uintptr_t)address, 4); return value; }
static uint64_t gauge_u64(uint32_t address) { uint64_t value; memcpy(&value, (void *)(uintptr_t)address, 8); return value; }
static double gauge_double(uint32_t address) { double value; memcpy(&value, (void *)(uintptr_t)address, 8); return value; }
static void gauge_w8(uint32_t address, uint8_t value) { memcpy((void *)(uintptr_t)address, &value, 1); }
static void gauge_w16(uint32_t address, uint16_t value) { memcpy((void *)(uintptr_t)address, &value, 2); }
static void gauge_w32(uint32_t address, uint32_t value) { memcpy((void *)(uintptr_t)address, &value, 4); }
static void gauge_wdouble(uint32_t address, long double value) { double rounded = (double)value; memcpy((void *)(uintptr_t)address, &rounded, 8); }
static void gauge_push(struct gauge_frame *frame, uint32_t value) { frame->esp -= 4; gauge_w32(frame->esp, value); }
static uint32_t gauge_pop(struct gauge_frame *frame) { uint32_t value = gauge_u32(frame->esp); frame->esp += 4; return value; }

/* The listing uses ZF/SF/CF/OF. Logical operations clear CF and OF. */
static uint32_t gauge_flags(uint32_t value, unsigned width)
{
    uint32_t mask = width == 32 ? 0xffffffffu : (1u << width) - 1;
    value &= mask;
    return (value == 0 ? 0x40u : 0) | (value & (1u << (width - 1)) ? 0x80u : 0);
}
static uint32_t gauge_add(struct gauge_frame *frame, uint32_t left, uint32_t right, unsigned width)
{
    uint64_t mask = width == 32 ? 0xffffffffull : (1ull << width) - 1;
    uint64_t sum = (left & mask) + (right & mask);
    uint32_t result = (uint32_t)(sum & mask), sign = 1u << (width - 1);
    frame->flags = gauge_flags(result, width) | (sum > mask ? 1u : 0) |
        ((~(left ^ right) & (left ^ result) & sign) ? 0x800u : 0);
    return result;
}
static uint32_t gauge_sub(struct gauge_frame *frame, uint32_t left, uint32_t right, unsigned width)
{
    uint32_t mask = width == 32 ? 0xffffffffu : (1u << width) - 1;
    uint32_t result = (left - right) & mask, sign = 1u << (width - 1);
    frame->flags = gauge_flags(result, width) | ((left & mask) < (right & mask) ? 1u : 0) |
        (((left ^ right) & (left ^ result) & sign) ? 0x800u : 0);
    return result;
}
static void gauge_fp_push(struct gauge_frame *frame, long double value) { frame->fp[frame->depth++] = value; }
static long double gauge_fp_pop(struct gauge_frame *frame) { return frame->fp[--frame->depth]; }
static uint16_t gauge_compare(long double left, long double right)
{
#if defined(__i386__) || defined(__x86_64__)
    uint16_t status;
    __asm__ volatile("fldt %2; fldt %1; fcom %%st(1); fnstsw %0; fstp %%st(0); fstp %%st(0)"
                     : "=m"(status) : "m"(left), "m"(right) : "st", "st(1)");
    return status;
#else
    if (isnan(left) || isnan(right)) return 0x4500;
    return left < right ? 0x100 : left == right ? 0x4000 : 0;
#endif
}
static int64_t gauge_ftol(long double value)
{
#if defined(__i386__) || defined(__x86_64__)
    uint16_t saved, control;
    int64_t result;
    __asm__ volatile("fnstcw %0" : "=m"(saved));
    control = saved | 0xc00;
    __asm__ volatile("fldcw %1; fldt %2; fistpll %0; fldcw %3"
                     : "=m"(result) : "m"(control), "m"(value), "m"(saved) : "st");
    return result;
#else
    if (!(value >= -9223372036854775808.0L && value < 9223372036854775808.0L))
        return INT64_MIN;
    return (int64_t)value;
#endif
}
static long double gauge_cos(long double value)
{
#if defined(__i386__) || defined(__x86_64__)
    long double result;
    __asm__ volatile("fcos" : "=t"(result) : "0"(value));
    return result;
#else
    return cosl(value);
#endif
}
static void gauge_divide(struct gauge_frame *frame, uint32_t divisor, int is_signed)
{
#if defined(__i386__) || defined(__x86_64__)
    /* Keep a real #DE boundary so FS98's own divide-repair filter can
     * recognize the instruction and resume it, including quotient overflow. */
    uint32_t low = frame->eax, high = frame->edx;
    if (is_signed)
        __asm__ volatile("idivl %2" : "+a"(low), "+d"(high) : "r"(divisor) : "cc");
    else
        __asm__ volatile("divl %2" : "+a"(low), "+d"(high) : "r"(divisor) : "cc");
    frame->eax = low; frame->edx = high;
#else
    uint64_t dividend = ((uint64_t)frame->edx << 32) | frame->eax;
    if (!divisor) { RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, 0, 0, NULL); return; }
    if (is_signed) {
        int64_t numerator = (int64_t)dividend, denominator = (int32_t)divisor, quotient;
        if (numerator == INT64_MIN && denominator == -1) {
            RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, 0, 0, NULL); return;
        }
        quotient = numerator / denominator;
        if (quotient < INT32_MIN || quotient > INT32_MAX) {
            RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, 0, 0, NULL); return;
        }
        frame->eax = (uint32_t)quotient;
        frame->edx = (uint32_t)(numerator % denominator);
    } else {
        uint64_t quotient = dividend / divisor;
        if (quotient > UINT32_MAX) { RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, 0, 0, NULL); return; }
        frame->eax = (uint32_t)quotient;
        frame->edx = (uint32_t)(dividend % divisor);
    }
#endif
}

typedef uint32_t (__stdcall *gauge_f0)(void);
typedef uint32_t (__stdcall *gauge_f1)(uint32_t);
typedef uint32_t (__stdcall *gauge_f2)(uint32_t, uint32_t);
typedef uint32_t (__stdcall *gauge_f3)(uint32_t, uint32_t, uint32_t);
static void gauge_panel_call(struct gauge_frame *frame, uint32_t function, unsigned count)
{
    uint32_t *args = (uint32_t *)(uintptr_t)frame->esp;
    switch (count) {
    case 0: frame->eax = ((gauge_f0)(uintptr_t)function)(); break;
    case 1: frame->eax = ((gauge_f1)(uintptr_t)function)(args[0]); break;
    case 2: frame->eax = ((gauge_f2)(uintptr_t)function)(args[0], args[1]); break;
    case 3: frame->eax = ((gauge_f3)(uintptr_t)function)(args[0], args[1], args[2]); break;
    }
    frame->esp += count * 4;
}
static void gauge_print(struct gauge_frame *frame)
{
    uint32_t *args = (uint32_t *)(uintptr_t)frame->esp;
    /* wsprintf is cdecl. Unused variadic words are ignored; the translated
     * caller, not this boundary, removes the actual argument block. */
    frame->eax = wsprintfA((char *)(uintptr_t)args[0], (char *)(uintptr_t)args[1],
        args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
        args[10], args[11], args[12], args[13], args[14], args[15]);
}
#endif
