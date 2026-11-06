/* Integer operations used by the crash-model C transliterations. Generated
 * C contains explicit operations and branches; no instruction decoder runs.
 * This boundary retains the crash update's nonstandard register contract.
 */
#ifndef FS98_MDL_CRASH_LIFT_H
#define FS98_MDL_CRASH_LIFT_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

struct mdl_frame {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi, flags;
    uint8_t *image;
    uint32_t budget;
    uint32_t stack_low, stack_high;
    uint8_t *stack;
    uint32_t native_args[3];
};
typedef char mdl_native_args_offset[(offsetof(struct mdl_frame,native_args)==56)?1:-1];
#define MDL_ARITH 0x8d5u
static uint32_t mdl_mask(unsigned width)
{ return width == 32 ? UINT32_MAX : (1u << width) - 1; }
static uint32_t mdl_memory(struct mdl_frame *f, uint32_t address)
{
    if (address >= f->stack_low && address < f->stack_high)
        return (uint32_t)(uintptr_t)(f->stack + address - f->stack_low);
    return address;
}
static uint32_t mdl_read(struct mdl_frame *f, uint32_t address, unsigned width)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)mdl_memory(f,address);
    uint32_t value = p[0];
    if (width >= 16) value |= (uint32_t)p[1] << 8;
    if (width == 32) value |= (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    return value;
}
static void mdl_write(struct mdl_frame *f, uint32_t address, uint32_t value, unsigned width)
{
    uint8_t *p = (uint8_t *)(uintptr_t)mdl_memory(f,address);
    p[0] = (uint8_t)value;
    if (width >= 16) p[1] = (uint8_t)(value >> 8);
    if (width == 32) { p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24); }
}
static void mdl_push(struct mdl_frame *f, uint32_t value, unsigned width)
{ f->esp -= width / 8; mdl_write(f,f->esp, value, width); }
static uint32_t mdl_pop(struct mdl_frame *f, unsigned width)
{ uint32_t value = mdl_read(f,f->esp, width); f->esp += width / 8; return value; }
static uint32_t mdl_zsp(uint32_t value, unsigned width)
{
    uint32_t byte = value & 255, parity;
    value &= mdl_mask(width);
    byte ^= byte >> 4;
    parity = ((0x6996u >> (byte & 15)) & 1) ^ 1;
    return (value == 0 ? 0x40u : 0) | (value & (1u << (width - 1)) ? 0x80u : 0) | parity * 4;
}
static uint32_t mdl_arith(struct mdl_frame *f, uint32_t a, uint32_t b,
                          unsigned width, int subtract, uint32_t carry)
{
    uint32_t mask = mdl_mask(width), result, sign = 1u << (width - 1), flags;
    uint64_t wide;
    a &= mask; b &= mask;
    wide = subtract ? (uint64_t)a - b - carry : (uint64_t)a + b + carry;
    result = (uint32_t)wide & mask;
    flags = mdl_zsp(result, width) | ((a ^ b ^ result) & 16);
    if (subtract) {
        if ((uint64_t)a < (uint64_t)b + carry) flags |= 1;
        if ((a ^ b) & (a ^ result) & sign) flags |= 0x800;
    } else {
        if (wide > mask) flags |= 1;
        if (~(a ^ b) & (a ^ result) & sign) flags |= 0x800;
    }
    f->flags = (f->flags & ~MDL_ARITH) | flags;
    return result;
}
static uint32_t mdl_logic(struct mdl_frame *f, uint32_t value, unsigned width)
{
    value &= mdl_mask(width);
    f->flags = (f->flags & ~MDL_ARITH) | mdl_zsp(value, width);
    return value;
}
/* kind: SHL, SHR, SAR, ROL, RCL, RCR, SHLD, SHRD. Only defined arithmetic
 * flags are relied upon by the generated crash code; AF/OF after multi-bit
 * shifts are not a portable contract. */
static uint32_t mdl_shift(struct mdl_frame *f, uint32_t a, uint32_t b,
                          unsigned width, unsigned count, unsigned kind)
{
    uint32_t mask = mdl_mask(width), sign = 1u << (width - 1), old, carry, next;
    unsigned i;
    count &= 31;
    a &= mask; b &= mask; old = a;
    if (kind == 4 || kind == 5) count %= width + 1;
    if (!count) return a;
    carry = f->flags & 1;
    for (i = 0; i < count; i++) {
        if (kind == 0 || kind == 3 || kind == 4 || kind == 6) {
            next = !!(a & sign);
            a = ((a << 1) | (kind == 3 ? next : kind == 4 ? carry : kind == 6 ? !!(b & sign) : 0)) & mask;
            b = (b << 1) & mask;
        } else {
            next = a & 1;
            a = (a >> 1) | (kind == 2 ? (a & sign) : kind == 5 ? carry * sign : kind == 7 ? (b & 1) * sign : 0);
            b >>= 1;
        }
        carry = next;
    }
    if (kind < 3 || kind >= 6)
        f->flags = (f->flags & ~MDL_ARITH) | mdl_zsp(a, width) | carry;
    else
        f->flags = (f->flags & ~0x801u) | carry;
    if (count == 1) {
        uint32_t overflow = kind == 1 ? !!(old & sign) : kind == 2 ? 0
            : kind == 5 ? !!((a ^ (a << 1)) & sign)
            : kind == 7 ? !!((a ^ old) & sign) : (!!(a & sign) ^ carry);
        f->flags = (f->flags & ~0x800u) | (overflow << 11);
    }
    return a;
}
static int64_t mdl_signed(uint32_t value, unsigned width)
{
    uint32_t sign = 1u << (width - 1);
    return (int64_t)(value & mdl_mask(width)) - ((value & sign) ? (1ll << width) : 0);
}
static void mdl_multiply(struct mdl_frame *f, uint32_t right, unsigned width, int is_signed)
{
    uint32_t mask = mdl_mask(width), low, high, overflow;
    uint64_t product = is_signed ? (uint64_t)(mdl_signed(f->eax, width) * mdl_signed(right, width))
                                 : (uint64_t)(f->eax & mask) * (right & mask);
    low = (uint32_t)product & mask;
    high = (uint32_t)(product >> width) & mask;
    overflow = is_signed ? (high != ((low & (1u << (width - 1))) ? mask : 0)) : high != 0;
    f->eax = (f->eax & ~mask) | low;
    f->edx = (f->edx & ~mask) | high;
    f->flags = (f->flags & ~MDL_ARITH) | (overflow ? 0x801u : 0);
}
/* Native call shims transfer every general register and flags in both
 * directions. Their argument widths come from the verified callees. */
void mdl_native1(struct mdl_frame *f, uint32_t fn);
void mdl_native2(struct mdl_frame *f, uint32_t fn);
void mdl_native3(struct mdl_frame *f, uint32_t fn);
static void mdl_native(struct mdl_frame *f, uint32_t fn, unsigned argc, unsigned pointers)
{
    uint32_t i, *registers[8]={&f->eax,&f->ecx,&f->edx,&f->ebx,&f->esp,&f->ebp,&f->esi,&f->edi};
    for(i=0;i<argc;i++) {
        uint32_t value=mdl_read(f,f->esp+i*4,32);
        f->native_args[i]=(pointers&(1u<<i))?mdl_memory(f,value):value;
    }
    if (argc == 1) mdl_native1(f, fn);
    else if (argc == 2) mdl_native2(f, fn);
    else if (argc == 3) mdl_native3(f, fn);
    else RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION, 0, 0, NULL);
    for(i=0;i<8;i++) if(i!=4) {
        uint32_t origin=(uint32_t)(uintptr_t)f->stack;
        if(*registers[i]>=origin && *registers[i]<origin+f->stack_high-f->stack_low)
            *registers[i]=f->stack_low+*registers[i]-origin;
    }
    f->esp += argc * 4;
}
#endif
