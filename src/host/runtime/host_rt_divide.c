/* The integer divide repair (docs/modules/host.md, "Integer divides are repaired, not
 * fatal"): the sim runs under a __try whose filter steps over a faulting div or idiv with
 * a saturated result instead of letting it end the game. Retail modules rely on it.
 *
 * Inherently x86: it decodes the faulting instruction and edits the thread's CONTEXT. */
#include "host_rt.h"

/* the CONTEXT's integer registers, by their number in ModRM and SIB */
static const uint16_t register_at[8] = {0xb0, 0xac, 0xa8, 0xa4, 0xc4, 0xb4, 0xa0, 0x9c};
#define CONTEXT_EAX 0xb0
#define CONTEXT_EDX 0xa8
#define CONTEXT_EIP 0xb8

/* 0x411fe0: the address a SIB byte names: index << scale (none for index 4) plus base
 * (none for base 5 under mod 0, else ebp). */
uint32_t SC fs98_host_rt_sib_address(uint32_t mod, uint32_t scale, uint32_t index, uint32_t base, void **pointers)
{
    uint8_t *context = (uint8_t *)pointers[1];
    uint32_t scaled = 0, added = 0;
    if ((index & 0xff) <= 7 && (index & 0xff) != 4)
        scaled = D32(context, register_at[index & 0xff]);
    scaled <<= (scale & 0xff) & 31;
    if ((base & 0xff) <= 7 && !((base & 0xff) == 5 && (mod & 0xff) == 0))
        added = D32(context, register_at[base & 0xff]);
    return added + scaled;
}

/* 0x4119a0, the filter: for an integer divide fault (0xc0000094, 0xc0000095) at an F6/F7
 * div or idiv, the instruction is decoded for its length, the result saturated in the
 * context and EIP stepped past it: -1, continue. Anything else: 0, search on.
 *
 * div: quotient and remainder all ones (AL/AH, AX/DX, EAX/EDX). idiv: the divisor is read
 * (register or memory, always a whole dword) and compared in sign with the dividend's top
 * half (AH, DX, EDX): differing, the quotient is the most negative but one (0x81, 0x8001,
 * 0x80000001), else the largest (0x7f, 0x7fff, 0x7fffffff); the remainder is the largest
 * either way. */
int32_t SC fs98_host_rt_divide_filter(void **pointers)
{
    uint8_t *context;
    const uint8_t *code;
    uint32_t code_value = *(uint32_t *)pointers[0];
    uint32_t length = 0, operand_16 = 0, operand_8 = 0, is_signed;
    uint8_t opcode, modrm, mod, reg, rm, sib_scale = 0, sib_index = 0, sib_base = 0;
    uint32_t displacement_8, displacement_32, divisor;
    if (code_value != 0xc0000094u && code_value != 0xc0000095u)
        return 0;
    context = (uint8_t *)pointers[1];
    code = (const uint8_t *)PTR(context, CONTEXT_EIP);
    if (code[0] == 0x66) {
        operand_16 = 1;
        ++code;
        length = 1;
    }
    opcode = code[0];
    modrm = code[1];
    rm = modrm & 7;
    reg = (modrm >> 3) & 7;
    mod = modrm >> 6;
    if ((opcode & 0xfe) != 0xf6)
        return 0;
    if (rm == 4) {
        ++length;
        sib_scale = code[2] >> 6;
        sib_index = (code[2] >> 3) & 7;
        sib_base = code[2] & 7;
        displacement_8 = code[3];          /* RETAIL BUG (kept): DIVIDE-DISP8-UNSIGNED */
        displacement_32 = *(const uint32_t *)(code + 3);
    } else {
        displacement_8 = code[2];          /* RETAIL BUG (kept): DIVIDE-DISP8-UNSIGNED */
        displacement_32 = *(const uint32_t *)(code + 2);
    }
    if (!(opcode & 1))
        operand_8 = 1;
    if (reg == 6)
        is_signed = 0;
    else if (reg == 7)
        is_signed = 1;
    else
        return 0;
    if (mod == 0) {
        if (rm == 5 || (rm == 4 && sib_base == 5))
            length += 4;
        length += 2;
    } else if (mod == 1)
        length += 3;
    else if (mod == 2)
        length += 6;
    else
        length += 2;
    if (!is_signed) {
        if (operand_8)
            D32(context, CONTEXT_EAX) |= 0xffff;
        else if (operand_16) {
            D32(context, CONTEXT_EAX) |= 0xffff;
            D32(context, CONTEXT_EDX) |= 0xffff;
        } else {
            D32(context, CONTEXT_EAX) = 0xffffffffu;
            D32(context, CONTEXT_EDX) = 0xffffffffu;
        }
        goto stepped;
    }
    /* the divisor; a byte register's number is read as the dword register's
     * (RETAIL BUG (kept): DIVIDE-BYTE-REGISTER) */
    if (mod == 3)
        divisor = D32(context, register_at[rm]);
    else if (mod == 0) {
        if (rm == 4) {
            uint32_t address = fs98_host_rt_sib_address(0, sib_scale, sib_index, sib_base, pointers);
            if (sib_base == 5)
                address += displacement_32;
            divisor = *(const uint32_t *)(uintptr_t)address;
        } else if (rm == 5)
            divisor = *(const uint32_t *)(uintptr_t)displacement_32;
        else
            divisor = *(const uint32_t *)(uintptr_t)D32(context, register_at[rm]);
    } else if (mod == 1) {
        uint32_t address = rm == 4 ? fs98_host_rt_sib_address(1, sib_scale, sib_index, sib_base, pointers)
                                   : D32(context, register_at[rm]);
        divisor = *(const uint32_t *)(uintptr_t)(address + displacement_8);
    } else {
        uint32_t address = rm == 4 ? fs98_host_rt_sib_address(2, sib_scale, sib_index, sib_base, pointers)
                                   : D32(context, register_at[rm]);
        divisor = *(const uint32_t *)(uintptr_t)(address + displacement_32);
    }
    if (operand_8) {
        uint32_t eax = D32(context, CONTEXT_EAX);
        int differ = ((int8_t)divisor < 0) != ((int8_t)(eax >> 8) < 0);
        D32(context, CONTEXT_EAX) = eax & 0xffff0000u;
        D32(context, CONTEXT_EAX) |= differ ? 0x7f81 : 0x7f7f;
    } else if (operand_16) {
        int differ = ((int16_t)divisor < 0) != ((int16_t)D32(context, CONTEXT_EDX) < 0);
        D32(context, CONTEXT_EAX) &= 0xffff0000u;
        D32(context, CONTEXT_EDX) &= 0xffff0000u;
        D32(context, CONTEXT_EAX) |= differ ? 0x8001 : 0x7fff;
        D32(context, CONTEXT_EDX) |= 0x7fff;
    } else {
        int differ = ((int32_t)divisor < 0) != ((int32_t)D32(context, CONTEXT_EDX) < 0);
        D32(context, CONTEXT_EAX) = differ ? 0x80000001u : 0x7fffffffu;
        D32(context, CONTEXT_EDX) = 0x7fffffffu;
    }
stepped:
    D32(context, CONTEXT_EIP) += length;
    return -1;
}

/* How many guarded calls are under way: while there are, a divide fault is the filter's. */
static volatile int32_t s_guarded;

/* 0x412140: a call with the divide repair on: retail's __try { fn(); } __except (filter)
 * with an empty handler, which never runs, since the filter returns only -1 or 0. Returns
 * what fn returns.
 *
 * Retail registers the filter in an SEH frame; here it is on while the call is under way and
 * fs98_host_rt_divide_handler, a vectored handler, applies it. A vectored handler sees a fault
 * before the frames inside the call do, so a module's own __except around a divide would no
 * longer see it first: none is known (docs/open-differences.md). */
int32_t SC fs98_host_rt_guarded(int32_t (*fn)(void))
{
    int32_t result;
    ++s_guarded;
    result = fn();
    --s_guarded;
    return result;
}

/* The vectored handler that stands for retail's SEH frame (see above): install it once at
 * startup with AddVectoredExceptionHandler. */
int32_t SC fs98_host_rt_divide_handler(void **pointers)
{
    return s_guarded > 0 ? fs98_host_rt_divide_filter(pointers) : 0;
}
