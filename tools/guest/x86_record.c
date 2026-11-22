/* x86_record: record what a real x86 CPU computes, as ground truth for the
 * fs98guest interpreter's instruction suite (Story 7.1).
 *
 *   gcc -m32 -O1 -static -nostdlib -ffreestanding -fno-builtin -fno-pie -no-pie \
 *       -fno-stack-protector -o bin/guest/x86_record tools/guest/x86_record.c
 *   bin/guest/x86_record > bin/guest/x86_results.txt
 *
 * Freestanding (raw int 0x80 syscalls, no libc or libgcc), so it builds
 * without 32-bit multilib. Inputs come from a fixed-seed generator: two runs
 * give the same bytes on the same CPU. No wine.
 *
 * One line per case: the op, its inputs, '->', its outputs. Flags (EFLAGS
 * CF PF AF ZF SF OF) and x87 status bits are recorded only where the SDM
 * defines them; flags an op leaves unaffected count as defined (output equals
 * input). x87 cases run under each precision (24, 53, 64) and rounding mode,
 * exceptions masked; "cw=007f" is retail's measured 24-bit word.
 *
 * Not covered: string ops, 8- and 16-bit mul/div, 16-bit shifts, BCD and
 * segment ops, SSE. Transcendental results (fsin, fcos, fptan, fpatan, fyl2x,
 * fyl2xp1, f2xm1) are this CPU's; the header names it, because other vendors
 * may differ in the last bit.
 */
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* gcc may emit these even when freestanding */
void *memcpy(void *d, const void *s, unsigned n) { u8 *o = d; const u8 *i = s; while (n--) *o++ = *i++; return d; }
void *memset(void *d, int c, unsigned n) { u8 *o = d; while (n--) *o++ = (u8)c; return d; }

/* ---- output ---- */

static char out_buf[1 << 16];
static u32 out_len;

static void out_flush(void)
{
    u32 done = 0;
    while (done < out_len) {
        int r;
        __asm__ volatile("int $0x80" : "=a"(r) : "a"(4), "b"(1), "c"(out_buf + done), "d"(out_len - done) : "memory");
        if (r <= 0) {
            __asm__ volatile("int $0x80" : : "a"(1), "b"(1));
        }
        done += (u32)r;
    }
    out_len = 0;
}

static void out_char(char c)
{
    if (out_len == sizeof out_buf)
        out_flush();
    out_buf[out_len++] = c;
}

static void out_str(const char *s)
{
    while (*s)
        out_char(*s++);
}

static void out_hex(u32 v, int digits)
{
    while (digits--)
        out_char("0123456789abcdef"[(v >> (digits * 4)) & 15]);
}

/* ---- inputs ---- */

static u32 rng = 0x2545f491u;

static u32 next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static const u32 edges[] = {
    0, 1, 2, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000, 0xffff, 0x10000,
    0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff, 31, 32, 33, 7, 8, 9, 16, 17,
};

static u32 value(void)
{
    u32 r = next();
    if ((r & 3) == 0)
        return edges[(r >> 2) % (sizeof edges / sizeof edges[0])];
    if ((r & 3) == 1)
        return next() >> (next() & 31);  /* small magnitudes */
    return next();
}

/* ---- integer ops ---- */

enum { CF = 0x1, PF = 0x4, AF = 0x10, ZF = 0x40, SF = 0x80, OF = 0x800, ALL = 0x8d5 };

typedef void (*int_fn)(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f);

#define FL_IN  "pushl %1\n\tpopfl\n\t"
#define FL_OUT "\n\tpushfl\n\tpopl %1"

#define ALU(name, insn, w) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 x = a, fl = *f; (void)r2; (void)c; \
      __asm__ volatile(FL_IN insn " %" w "2, %" w "0" FL_OUT : "+q"(x), "+r"(fl) : "q"(b) : "cc"); \
      *r = x; *f = fl; }

#define UNARY(name, insn, w) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 x = a, fl = *f; (void)r2; (void)b; (void)c; \
      __asm__ volatile(FL_IN insn " %" w "0" FL_OUT : "+q"(x), "+r"(fl) : : "cc"); \
      *r = x; *f = fl; }

#define SHIFT(name, insn, w) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 x = a, fl = *f; (void)r2; (void)c; \
      __asm__ volatile(FL_IN insn " %%cl, %" w "0" FL_OUT : "+q"(x), "+r"(fl) : "c"(b) : "cc"); \
      *r = x; *f = fl; }

#define DSHIFT(name, insn) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 x = a, fl = *f; (void)r2; \
      __asm__ volatile(FL_IN insn " %%cl, %2, %0" FL_OUT : "+r"(x), "+r"(fl) : "r"(b), "c"(c) : "cc"); \
      *r = x; *f = fl; }

#define MUL1(name, insn) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 lo = a, hi, fl = *f; (void)c; \
      __asm__ volatile(FL_IN insn " %3" FL_OUT : "+a"(lo), "+r"(fl), "=d"(hi) : "r"(b) : "cc"); \
      *r = lo; *r2 = hi; *f = fl; }

#define DIV(name, insn) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 lo = a, hi = c, fl = *f; \
      __asm__ volatile(FL_IN insn " %3" FL_OUT : "+a"(lo), "+r"(fl), "+d"(hi) : "r"(b) : "cc"); \
      *r = lo; *r2 = hi; *f = fl; }

#define MOVX(name, insn, w) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 x = a, fl = *f; (void)r2; (void)c; \
      __asm__ volatile(FL_IN insn " %" w "2, %0" FL_OUT : "+q"(x), "+r"(fl) : "q"(b) : "cc"); \
      *r = x; *f = fl; }

#define IMUL3(name, imm) \
    static void name(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f) \
    { u32 x = a, fl = *f; (void)r2; (void)b; (void)c; \
      __asm__ volatile(FL_IN "imull $" #imm ", %0, %0" FL_OUT : "+r"(x), "+r"(fl) : : "cc"); \
      *r = x; *f = fl; }

ALU(add32, "addl", "") ALU(adc32, "adcl", "") ALU(sub32, "subl", "") ALU(sbb32, "sbbl", "")
ALU(and32, "andl", "") ALU(or32, "orl", "") ALU(xor32, "xorl", "") ALU(cmp32, "cmpl", "") ALU(test32, "testl", "")
ALU(add16, "addw", "w") ALU(adc16, "adcw", "w") ALU(sub16, "subw", "w") ALU(sbb16, "sbbw", "w")
ALU(and16, "andw", "w") ALU(or16, "orw", "w") ALU(xor16, "xorw", "w") ALU(cmp16, "cmpw", "w")
ALU(add8, "addb", "b") ALU(adc8, "adcb", "b") ALU(sub8, "subb", "b") ALU(sbb8, "sbbb", "b")
ALU(and8, "andb", "b") ALU(or8, "orb", "b") ALU(xor8, "xorb", "b") ALU(cmp8, "cmpb", "b") ALU(test8, "testb", "b")
ALU(imul2_32, "imull", "") ALU(bsf32, "bsfl", "") ALU(bsr32, "bsrl", "")
MOVX(movsx8, "movsbl", "b") MOVX(movsx16, "movswl", "w") MOVX(movzx8, "movzbl", "b") MOVX(movzx16, "movzwl", "w")
UNARY(inc32, "incl", "") UNARY(dec32, "decl", "") UNARY(neg32, "negl", "") UNARY(not32, "notl", "")
UNARY(inc8, "incb", "b") UNARY(dec8, "decb", "b") UNARY(neg8, "negb", "b") UNARY(not8, "notb", "b")
UNARY(bswap32, "bswapl", "")
SHIFT(shl32, "shll", "") SHIFT(shr32, "shrl", "") SHIFT(sar32, "sarl", "")
SHIFT(rol32, "roll", "") SHIFT(ror32, "rorl", "") SHIFT(rcl32, "rcll", "") SHIFT(rcr32, "rcrl", "")
SHIFT(shl8, "shlb", "b") SHIFT(shr8, "shrb", "b") SHIFT(sar8, "sarb", "b")
SHIFT(rol8, "rolb", "b") SHIFT(ror8, "rorb", "b") SHIFT(rcl8, "rclb", "b") SHIFT(rcr8, "rcrb", "b")
DSHIFT(shld32, "shldl") DSHIFT(shrd32, "shrdl")
MUL1(mul32, "mull") MUL1(imul1_32, "imull")
DIV(div32, "divl") DIV(idiv32, "idivl")
IMUL3(imul3_3, 3) IMUL3(imul3_m1, -1) IMUL3(imul3_10001, 0x10001) IMUL3(imul3_7fffffff, 0x7fffffff)

static void cdq32(u32 *r, u32 *r2, u32 a, u32 b, u32 c, u32 *f)
{
    u32 lo = a, hi;
    (void)b; (void)c; (void)f;
    __asm__ volatile("cltd" : "+a"(lo), "=d"(hi));
    *r = lo; *r2 = hi;
}

enum kind { ARITH, LOGIC, KEEP, SHIFTK, ROT, MULK, DIVK, IDIVK, BITSCAN, CDQ };

static const struct int_op {
    const char *name;
    int_fn fn;
    enum kind kind;
    u32 width;
} int_ops[] = {
    {"add32", add32, ARITH, 32}, {"adc32", adc32, ARITH, 32}, {"sub32", sub32, ARITH, 32}, {"sbb32", sbb32, ARITH, 32},
    {"cmp32", cmp32, ARITH, 32}, {"neg32", neg32, ARITH, 32}, {"inc32", inc32, ARITH, 32}, {"dec32", dec32, ARITH, 32},
    {"and32", and32, LOGIC, 32}, {"or32", or32, LOGIC, 32}, {"xor32", xor32, LOGIC, 32}, {"test32", test32, LOGIC, 32},
    {"add16", add16, ARITH, 16}, {"adc16", adc16, ARITH, 16}, {"sub16", sub16, ARITH, 16}, {"sbb16", sbb16, ARITH, 16},
    {"cmp16", cmp16, ARITH, 16}, {"and16", and16, LOGIC, 16}, {"or16", or16, LOGIC, 16}, {"xor16", xor16, LOGIC, 16},
    {"add8", add8, ARITH, 8}, {"adc8", adc8, ARITH, 8}, {"sub8", sub8, ARITH, 8}, {"sbb8", sbb8, ARITH, 8},
    {"cmp8", cmp8, ARITH, 8}, {"neg8", neg8, ARITH, 8}, {"inc8", inc8, ARITH, 8}, {"dec8", dec8, ARITH, 8},
    {"and8", and8, LOGIC, 8}, {"or8", or8, LOGIC, 8}, {"xor8", xor8, LOGIC, 8}, {"test8", test8, LOGIC, 8},
    {"not32", not32, KEEP, 32}, {"not8", not8, KEEP, 8}, {"bswap32", bswap32, KEEP, 32},
    {"movsx8", movsx8, KEEP, 32}, {"movsx16", movsx16, KEEP, 32}, {"movzx8", movzx8, KEEP, 32}, {"movzx16", movzx16, KEEP, 32},
    {"shl32", shl32, SHIFTK, 32}, {"shr32", shr32, SHIFTK, 32}, {"sar32", sar32, SHIFTK, 32},
    {"shl8", shl8, SHIFTK, 8}, {"shr8", shr8, SHIFTK, 8}, {"sar8", sar8, SHIFTK, 8},
    {"shld32", shld32, SHIFTK, 32}, {"shrd32", shrd32, SHIFTK, 32},
    {"rol32", rol32, ROT, 32}, {"ror32", ror32, ROT, 32}, {"rcl32", rcl32, ROT, 32}, {"rcr32", rcr32, ROT, 32},
    {"rol8", rol8, ROT, 8}, {"ror8", ror8, ROT, 8}, {"rcl8", rcl8, ROT, 8}, {"rcr8", rcr8, ROT, 8},
    {"mul32", mul32, MULK, 32}, {"imul1_32", imul1_32, MULK, 32}, {"imul2_32", imul2_32, MULK, 32},
    {"imul3_3", imul3_3, MULK, 32}, {"imul3_m1", imul3_m1, MULK, 32},
    {"imul3_10001", imul3_10001, MULK, 32}, {"imul3_7fffffff", imul3_7fffffff, MULK, 32},
    {"div32", div32, DIVK, 32}, {"idiv32", idiv32, IDIVK, 32},
    {"bsf32", bsf32, BITSCAN, 32}, {"bsr32", bsr32, BITSCAN, 32},
    {"cdq", cdq32, CDQ, 32},
};

/* Which result flags the SDM defines, given the masked shift count n. */
static u32 defined_flags(const struct int_op *op, u32 n)
{
    switch (op->kind) {
    case ARITH: case KEEP: return ALL;
    case LOGIC: return ALL & ~AF;
    case SHIFTK:
        if (n == 0) return ALL;
        if (op->width == 8 && n >= 8 && op->name[1] != 'a') return ALL & ~AF & ~OF & ~CF;  /* shl/shr (not sar) past the width */
        return n == 1 ? ALL & ~AF : ALL & ~AF & ~OF;
    case ROT: return n <= 1 ? ALL : ALL & ~OF;
    case MULK: return CF | OF;
    case BITSCAN: return ZF;
    default: return 0;
    }
}

static u32 neg(u32 v) { return 0u - v; }
static u32 mag(u32 v) { return (v & 0x80000000u) ? neg(v) : v; }

static void record_int(const struct int_op *op, u32 count)
{
    for (u32 i = 0; i < count; i++) {
        u32 a = value(), b = value(), c = value(), f = (next() & ALL) | 2, r = 0, r2 = 0, fo;
        u32 n = b & 31;  /* the CPU masks every shift and rotate count to 5 bits */
        if (op->fn == shld32 || op->fn == shrd32)
            n = c & 31;
        if (op->kind == DIVK) {
            if (b == 0) b = 1;
            c = c % b;  /* high half below the divisor: no #DE */
        } else if (op->kind == IDIVK) {
            if (b == 0 || b == 0xffffffffu) b = 3;
            u32 room = mag(b) / 2;
            c = (i & 1) && room > 1 ? next() % (room - 1) : 0;
            if ((i & 1) && (next() & 1)) c = neg(c);
            if (!(i & 1) || room <= 1) c = (a & 0x80000000u) ? 0xffffffffu : 0;  /* the cdq case */
        }
        fo = f;
        op->fn(&r, &r2, a, b, c, &fo);
        out_str(op->name);
        out_str(" a="); out_hex(a, 8);
        out_str(" b="); out_hex(b, 8);
        out_str(" c="); out_hex(c, 8);
        out_str(" f="); out_hex(f, 3);
        out_str(" -> r=");
        if (op->kind == BITSCAN && b == 0)
            out_str("--------");  /* destination undefined for a zero source */
        else
            out_hex(r, 8);
        out_str(" r2="); out_hex(r2, 8);
        out_str(" f="); out_hex(fo & defined_flags(op, n), 3);
        out_char('\n');
    }
}

#define SETCC(cc) \
    static u32 set_##cc(u32 f) \
    { u32 x = 0; __asm__ volatile("pushl %1\n\tpopfl\n\tset" #cc " %b0" : "+q"(x) : "r"(f) : "cc"); return x; }
SETCC(o) SETCC(no) SETCC(b) SETCC(ae) SETCC(e) SETCC(ne) SETCC(be) SETCC(a)
SETCC(s) SETCC(ns) SETCC(p) SETCC(np) SETCC(l) SETCC(ge) SETCC(le) SETCC(g)

static void record_setcc(void)
{
    static u32 (*const fns[16])(u32) = {set_o, set_no, set_b, set_ae, set_e, set_ne, set_be, set_a,
                                        set_s, set_ns, set_p, set_np, set_l, set_ge, set_le, set_g};
    static const char *const names[16] = {"o", "no", "b", "ae", "e", "ne", "be", "a",
                                          "s", "ns", "p", "np", "l", "ge", "le", "g"};
    static const u32 bits[5] = {CF, PF, ZF, SF, OF};
    for (u32 combo = 0; combo < 32; combo++) {
        u32 f = 2;
        for (u32 j = 0; j < 5; j++)
            if (combo & (1u << j)) f |= bits[j];
        for (u32 cc = 0; cc < 16; cc++) {
            out_str("set"); out_str(names[cc]);
            out_str(" f="); out_hex(f, 3);
            out_str(" -> r="); out_hex(fns[cc](f), 2);
            out_char('\n');
        }
    }
}

/* ---- x87 ---- */

typedef struct { u8 b[10]; } f80;

enum { C0 = 0x100, C1 = 0x200, C2 = 0x400, C3 = 0x4000, EXC = 0x7f };  /* EXC: IE..PE and SF */
enum { BIAS = 16383 };

static u32 f80_exp(const f80 *v) { return (u32)(v->b[8] | (v->b[9] << 8)) & 0x7fff; }

static void make_f80(f80 *v)
{
    u32 cls = next() % 16, lo = next(), hi = next() | 0x80000000u, e;
    switch (cls) {
    case 0: lo = 0; hi = 0; e = 0; break;                          /* zero */
    case 1: lo = 0; hi = 0x80000000u; e = 0x7fff; break;           /* infinity */
    case 2: hi |= 0x40000000u; e = 0x7fff; break;                  /* quiet NaN */
    case 3: hi &= 0x7fffffffu; e = 0; break;                       /* denormal */
    case 4: case 5: {                                              /* integers and exact halves */
        u32 k = next() % 40;
        e = BIAS + k;
        if (k + 1 < 32) { hi &= ~(0xffffffffu >> (k + 1)); lo = 0; if (cls == 5) hi |= 0x80000000u >> (k + 1); }
        else if (k + 1 < 64) { lo &= ~(0xffffffffu >> (k + 1 - 32)); }
        break;
    }
    case 6: case 7: case 8: case 9: e = BIAS - 8 + next() % 17; break;
    case 10: case 11: e = BIAS - 70 + next() % 141; break;
    case 12: e = 1 + next() % 0x7ffe; break;
    case 13: e = BIAS - 4 - next() % 4; break;                     /* |v| < 0.25 */
    default: {                                                     /* raw bytes, for the load ops */
        for (int i = 0; i < 10; i++) v->b[i] = (u8)next();
        return;
    }
    }
    if (next() & 1) e |= 0x8000;
    for (int i = 0; i < 4; i++) { v->b[i] = (u8)(lo >> (8 * i)); v->b[4 + i] = (u8)(hi >> (8 * i)); }
    v->b[8] = (u8)e;
    v->b[9] = (u8)(e >> 8);
}

/* Each runner: fninit, the control word, b then a (st0 = a, st1 = b), the op. */
#define X87_PRE "fninit\n\tfldcw %[cw]\n\tfldt %[b]\n\tfldt %[a]\n\t"
#define X87(name, op) \
    static void name(u16 cw, const f80 *a, const f80 *b, f80 *r0, f80 *r1, u16 *sw) \
    { __asm__ volatile(X87_PRE op "\n\tfnstsw %[sw]\n\tfstpt %[r0]\n\tfstpt %[r1]\n\tfninit" \
        : [r0] "=m"(*r0), [r1] "=m"(*r1), [sw] "=m"(*sw) : [cw] "m"(cw), [a] "m"(*a), [b] "m"(*b)); }
#define X87_STORE(name, op) \
    static void name(u16 cw, const f80 *a, const f80 *b, f80 *r0, f80 *r1, u16 *sw) \
    { (void)r1; __asm__ volatile(X87_PRE op " %[r0]\n\tfnstsw %[sw]\n\tfninit" \
        : [r0] "=m"(*r0), [sw] "=m"(*sw) : [cw] "m"(cw), [a] "m"(*a), [b] "m"(*b)); }
#define X87_LOAD(name, op) \
    static void name(u16 cw, const f80 *a, const f80 *b, f80 *r0, f80 *r1, u16 *sw) \
    { (void)b; (void)r1; __asm__ volatile("fninit\n\tfldcw %[cw]\n\t" op " %[a]\n\tfnstsw %[sw]\n\tfstpt %[r0]\n\tfninit" \
        : [r0] "=m"(*r0), [sw] "=m"(*sw) : [cw] "m"(cw), [a] "m"(*a)); }

/* AT&T "fsub %st(1), %st" is D8 E1: st0 = st0 - st1 (the AT&T operand swap only affects st(i) destinations). */
X87(fadd_, "fadd %%st(1), %%st") X87(fsub_, "fsub %%st(1), %%st") X87(fsubr_, "fsubr %%st(1), %%st")
X87(fmul_, "fmul %%st(1), %%st") X87(fdiv_, "fdiv %%st(1), %%st") X87(fdivr_, "fdivr %%st(1), %%st")
X87(fsqrt_, "fsqrt") X87(fabs_, "fabs") X87(fchs_, "fchs") X87(frndint_, "frndint")
X87(fsin_, "fsin") X87(fcos_, "fcos") X87(fptan_, "fptan") X87(fxtract_, "fxtract") X87(f2xm1_, "f2xm1")
X87(fprem_, "fprem") X87(fprem1_, "fprem1") X87(fscale_, "fscale")
X87(fpatan_, "fpatan") X87(fyl2x_, "fyl2x") X87(fyl2xp1_, "fyl2xp1")
X87(fcom_, "fcom %%st(1)") X87(fucom_, "fucom %%st(1)") X87(ftst_, "ftst") X87(fxam_, "fxam")
X87_STORE(fst32_, "fstps") X87_STORE(fst64_, "fstpl")
X87_STORE(fist16_, "fistps") X87_STORE(fist32_, "fistpl") X87_STORE(fist64_, "fistpll")
X87_LOAD(fld32_, "flds") X87_LOAD(fld64_, "fldl") X87_LOAD(fild16_, "filds") X87_LOAD(fild32_, "fildl") X87_LOAD(fild64_, "fildll")

typedef void (*x87_fn)(u16, const f80 *, const f80 *, f80 *, f80 *, u16 *);

enum { ANY, BELOW_1, BELOW_QUARTER };

static const struct x87_op {
    const char *name;   /* Intel syntax, with the opcode bytes */
    x87_fn fn;
    u16 c_bits;         /* condition-code bits the SDM defines for this op */
    u8 outputs;         /* 0 status only, 1 st0, 2 st0 and st1, 100 + n: n bytes stored to memory */
    u8 domain;          /* inputs outside it give undefined results, so are skipped */
} x87_ops[] = {
    {"fadd_st0_st1(d8c1)", fadd_, C1, 1, ANY}, {"fsub_st0_st1(d8e1)", fsub_, C1, 1, ANY},
    {"fsubr_st0_st1(d8e9)", fsubr_, C1, 1, ANY}, {"fmul_st0_st1(d8c9)", fmul_, C1, 1, ANY},
    {"fdiv_st0_st1(d8f1)", fdiv_, C1, 1, ANY}, {"fdivr_st0_st1(d8f9)", fdivr_, C1, 1, ANY},
    {"fsqrt(d9fa)", fsqrt_, C1, 1, ANY}, {"fabs(d9e1)", fabs_, C1, 1, ANY}, {"fchs(d9e0)", fchs_, C1, 1, ANY},
    {"frndint(d9fc)", frndint_, C1, 1, ANY},
    {"fsin(d9fe)", fsin_, C1 | C2, 1, ANY}, {"fcos(d9ff)", fcos_, C1 | C2, 1, ANY},
    {"fptan(d9f2)", fptan_, C1 | C2, 2, ANY}, {"fxtract(d9f4)", fxtract_, C1, 2, ANY},
    {"f2xm1(d9f0)", f2xm1_, C1, 1, BELOW_1},
    {"fprem(d9f8)", fprem_, C0 | C1 | C2 | C3, 1, ANY}, {"fprem1(d9f5)", fprem1_, C0 | C1 | C2 | C3, 1, ANY},
    {"fscale(d9fd)", fscale_, C1, 1, ANY},
    {"fpatan(d9f3)", fpatan_, C1, 1, ANY}, {"fyl2x(d9f1)", fyl2x_, C1, 1, ANY},
    {"fyl2xp1(d9f9)", fyl2xp1_, C1, 1, BELOW_QUARTER},
    {"fcom_st1(d8d1)", fcom_, C0 | C1 | C2 | C3, 0, ANY}, {"fucom_st1(dde1)", fucom_, C0 | C1 | C2 | C3, 0, ANY},
    {"ftst(d9e4)", ftst_, C0 | C1 | C2 | C3, 0, ANY}, {"fxam(d9e5)", fxam_, C0 | C1 | C2 | C3, 0, ANY},
    {"fstp_m32(d9/3)", fst32_, C1, 104, ANY}, {"fstp_m64(dd/3)", fst64_, C1, 108, ANY},
    {"fistp_m16(df/3)", fist16_, C1, 102, ANY}, {"fistp_m32(db/3)", fist32_, C1, 104, ANY},
    {"fistp_m64(df/7)", fist64_, C1, 108, ANY},
    {"fld_m32(d9/0)", fld32_, C1, 1, ANY}, {"fld_m64(dd/0)", fld64_, C1, 1, ANY},
    {"fild_m16(df/0)", fild16_, C1, 1, ANY}, {"fild_m32(db/0)", fild32_, C1, 1, ANY},
    {"fild_m64(df/5)", fild64_, C1, 1, ANY},
};

static void out_f80(const f80 *v)
{
    for (int i = 9; i >= 0; i--) {
        out_hex(v->b[i], 2);
        if (i == 8) out_char('.');
    }
}

static void out_bytes(const f80 *v, int n)
{
    for (int i = n - 1; i >= 0; i--)
        out_hex(v->b[i], 2);
}

static void record_x87(const struct x87_op *op, u32 count)
{
    static const u16 pcs[3] = {0, 2, 3}, rcs[4] = {0, 1, 2, 3};
    for (int p = 0; p < 3; p++) {
        for (int m = 0; m < 4; m++) {
            u16 cw = (u16)(0x7f | (pcs[p] << 8) | (rcs[m] << 10));
            for (u32 i = 0; i < count; i++) {
                f80 a, b, r0 = {{0}}, r1 = {{0}};
                u16 sw = 0;
                make_f80(&a);
                make_f80(&b);
                if (op->domain == BELOW_1 && f80_exp(&a) >= BIAS) continue;
                if (op->domain == BELOW_QUARTER && f80_exp(&a) >= BIAS - 2) continue;
                op->fn(cw, &a, &b, &r0, &r1, &sw);
                out_str(op->name);
                out_str(" cw="); out_hex(cw, 4);
                out_str(" a="); out_f80(&a);
                out_str(" b="); out_f80(&b);
                out_str(" -> sw="); out_hex(sw & (EXC | op->c_bits), 4);
                if (op->outputs >= 100) {
                    out_str(" m="); out_bytes(&r0, op->outputs - 100);
                } else if (op->outputs > 0) {
                    out_str(" st0="); out_f80(&r0);
                    if (op->outputs == 2) { out_str(" st1="); out_f80(&r1); }
                }
                out_char('\n');
            }
        }
    }
}

/* ---- main ---- */

static void cpu_header(void)
{
    u32 r[12], a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    out_str("# x86_record v1 vendor=");
    u32 v[3] = {b, d, c};
    for (int i = 0; i < 12; i++) out_char((char)(v[i / 4] >> (8 * (i % 4))));
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    out_str(" signature="); out_hex(a, 8);
    out_str(" brand=");
    for (u32 leaf = 0; leaf < 3; leaf++) {
        __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(0x80000002u + leaf));
        for (int i = 0; i < 16; i++) {
            char ch = (char)(r[i / 4] >> (8 * (i % 4)));
            if (ch) out_char(ch);
        }
    }
    out_char('\n');
}

__attribute__((noreturn)) void _start(void)
{
    cpu_header();
    for (u32 i = 0; i < sizeof int_ops / sizeof int_ops[0]; i++)
        record_int(&int_ops[i], 1500);
    record_setcc();
    for (u32 i = 0; i < sizeof x87_ops / sizeof x87_ops[0]; i++)
        record_x87(&x87_ops[i], 200);
    out_flush();
    __asm__ volatile("int $0x80" : : "a"(1), "b"(0));
    for (;;) {}
}
