/* The host's integer-divide repair (host.md: "Integer divides are repaired,
 * not fatal"), for the harnesses that load retail modules: the filter at
 * 0x4119a0 decoded.  Included by panelsdiff.c; g3ddiff.c, windowdiff.c and
 * the others carry their own copies. */
#ifndef FS98_DIVFIX_H
#define FS98_DIVFIX_H

#include <windows.h>
#include <stdint.h>


static DWORD *reg_of(CONTEXT *c, uint32_t r)
{
    switch (r & 7) {
    case 0: return &c->Eax;
    case 1: return &c->Ecx;
    case 2: return &c->Edx;
    case 3: return &c->Ebx;
    case 4: return &c->Esp;
    case 5: return &c->Ebp;
    case 6: return &c->Esi;
    default: return &c->Edi;
    }
}

/* What 0x4119a0 does with a divide fault: decode the F6/F7 (with or without
 * 0x66), its ModRM, SIB and displacement for the length and the divisor;
 * div gives all ones; idiv the largest quotient, or the most negative but
 * one (0x8001, 0x80000001; 0x7f81 in ax for a byte) when the divisor and
 * the dividend's top half differ in sign, and the remainder the largest;
 * then on after the instruction. */
static int div_fixup(CONTEXT *c)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
    uint32_t len = 0, word = 0, byte, mod, reg, rm, v, neg, ea = 0, have_ea = 0;

    if (*p == 0x66) {
        word = 1;
        p++;
        len = 1;
    }
    if ((p[0] & 0xfe) != 0xf6)
        return 0;
    byte = !(p[0] & 1);
    mod = p[1] >> 6;
    reg = (p[1] >> 3) & 7;
    rm = p[1] & 7;
    if (reg != 6 && reg != 7)
        return 0;
    len += 2;
    if (mod != 3) {
        const uint8_t *q = p + 2;

        have_ea = 1;
        if (rm == 4) {
            uint32_t sib = *q++, base = sib & 7, index = (sib >> 3) & 7;

            len++;
            if (base == 5 && mod == 0) {
                ea = *(const uint32_t *)q;
                q += 4;
                len += 4;
            } else {
                ea = *reg_of(c, base);
            }
            if (index != 4)
                ea += *reg_of(c, index) << (sib >> 6);
        } else if (rm == 5 && mod == 0) {
            ea = *(const uint32_t *)q;
            q += 4;
            len += 4;
        } else {
            ea = *reg_of(c, rm);
        }
        if (mod == 1) {
            ea += (uint32_t)(int32_t)(int8_t)*q;
            len += 1;
        } else if (mod == 2) {
            ea += *(const uint32_t *)q;
            len += 4;
        }
    }
    if (byte)
        v = have_ea ? *(const uint8_t *)(uintptr_t)ea : (rm < 4 ? *reg_of(c, rm) : *reg_of(c, rm - 4) >> 8);
    else if (word)
        v = have_ea ? *(const uint16_t *)(uintptr_t)ea : *reg_of(c, rm);
    else
        v = have_ea ? *(const uint32_t *)(uintptr_t)ea : *reg_of(c, rm);
    if (reg == 6) {
        if (byte) {
            c->Eax |= 0xffff;
        } else if (word) {
            c->Eax |= 0xffff;
            c->Edx |= 0xffff;
        } else {
            c->Eax = 0xffffffffu;
            c->Edx = 0xffffffffu;
        }
    } else if (byte) {
        neg = ((int8_t)v < 0) != ((int8_t)(c->Eax >> 8) < 0);
        c->Eax = (c->Eax & 0xffff0000u) | (neg ? 0x7f81u : 0x7f7fu);
    } else if (word) {
        neg = ((int16_t)v < 0) != ((int16_t)c->Edx < 0);
        c->Eax = (c->Eax & 0xffff0000u) | (neg ? 0x8001u : 0x7fffu);
        c->Edx = (c->Edx & 0xffff0000u) | 0x7fffu;
    } else {
        neg = ((int32_t)v < 0) != ((int32_t)c->Edx < 0);
        c->Eax = neg ? 0x80000001u : 0x7fffffffu;
        c->Edx = 0x7fffffffu;
    }
    c->Eip += len;
    return 1;
}

#endif
