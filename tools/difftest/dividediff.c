/* include/fs98/divide.h against retail's divide repair filter (0x4119a0) and ours
 * (fs98_host_rt_divide_filter), three ways.
 *
 *   dividediff.exe [N] [seed]
 *
 * Retail FLTSIM98.EXE (sha256 checked) is mapped at 0x400000, this harness being linked elsewhere,
 * without its imports; the filter needs none. Each vector executes a real div or idiv from an
 * executable buffer. A vectored handler, gated on that instruction's exact EIP, calls retail's
 * filter (one argument, __stdcall) with the live EXCEPTION_POINTERS, or ours, and the divide
 * continues past the repaired instruction. Each vector runs once per filter from the same
 * registers, and the registers each leaves, the EIP step, the flags (on a repair) and the hit
 * count must agree, and equal what the helper says (both fields, and with a NULL remainder).
 * The divisor is in a register or in memory (modrm with disp8 >= 0, disp32 and SIB forms), at 8,
 * 16 and 32 bits, for div and idiv. Wine may report every #DE as 0xc0000094, so either live code
 * is accepted; the filters are also called with synthetic records for 94, 95 and another code.
 *
 * Excluded: the filter's decode-time quirks, which are for the guest executing code (AD-18), not
 * for a C helper. Each is a retail bug (docs/retail-bugs.md):
 *   DIVIDE-BYTE-REGISTER      an AH/CH/DH/BH divisor is read as the dword register;
 *   DIVIDE-DISP8-UNSIGNED     a negative disp8 is added zero-extended;
 *   (the four-byte read of a narrow memory divisor: only its low part is used, and the harness
 *    fills the bytes beyond it, so it is exercised and harmless);
 *   DIVIDE-SIB-REGISTER-STEP  r/m=4 with mod=3 decodes a SIB byte and steps EIP one too far
 *                             (esp, sp, ah as a divisor).
 * Registers esp, and eax/edx as the divisor, are not used. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fs98/divide.h"

#define BASE 0x400000u
#define RETAIL_FILTER 0x4119a0u
#define REFERENCE_SHA256 "f47c124a1ad44475a8a3a5ecbce83f5dc5d6e7dad78cf120b20616738bd99f6a"
#define STDCALL __attribute__((stdcall))

int32_t STDCALL fs98_host_rt_divide_filter(void **pointers);
static int32_t (STDCALL *retail_filter)(void **) = (int32_t (STDCALL *)(void **))(uintptr_t)RETAIL_FILTER;

/* registers in the CPU's numbering: eax ecx edx ebx esp ebp esi edi; [8] eflags in, [9] out */
void fs98_run_div(uint32_t *regs, const uint8_t *code);
__asm__(".text\n.globl _fs98_run_div\n_fs98_run_div:\n"
        "pushl %ebp\n pushl %ebx\n pushl %esi\n pushl %edi\n"
        "movl 20(%esp),%eax\n movl 24(%esp),%ecx\n"
        "pushl %eax\n pushl %ecx\n"
        "pushl 32(%eax)\n popfl\n"
        "movl 4(%eax),%ecx\n movl 8(%eax),%edx\n movl 12(%eax),%ebx\n movl 20(%eax),%ebp\n"
        "movl 24(%eax),%esi\n movl 28(%eax),%edi\n movl (%eax),%eax\n"
        "call *(%esp)\n"
        "pushl %eax\n movl 8(%esp),%eax\n"
        "movl %ecx,4(%eax)\n movl %edx,8(%eax)\n movl %ebx,12(%eax)\n movl %ebp,20(%eax)\n"
        "movl %esi,24(%eax)\n movl %edi,28(%eax)\n"
        "popl (%eax)\n"
        "pushfl\n popl 36(%eax)\n"
        "addl $8,%esp\n popl %edi\n popl %esi\n popl %ebx\n popl %ebp\n ret\n");

/* ---- sha256, to pin the reference executable ---- */
static void sha256(const uint8_t *data, size_t length, char *hex)
{
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_t total = (length + 9 + 63) & ~(size_t)63, at;
    uint8_t *buffer = calloc(total, 1);
    int i;
    memcpy(buffer, data, length);
    buffer[length] = 0x80;
    for (i = 0; i < 8; ++i) buffer[total - 1 - i] = (uint8_t)(((uint64_t)length * 8) >> (8 * i));
    for (at = 0; at < total; at += 64) {
        uint32_t w[64], v[8], t1, t2;
        for (i = 0; i < 16; ++i)
            w[i] = (uint32_t)buffer[at + 4 * i] << 24 | buffer[at + 4 * i + 1] << 16 | buffer[at + 4 * i + 2] << 8 | buffer[at + 4 * i + 3];
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
        for (i = 16; i < 64; ++i)
            w[i] = w[i - 16] + (ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
                   (ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10));
        memcpy(v, h, sizeof v);
        for (i = 0; i < 64; ++i) {
            t1 = v[7] + (ROR(v[4], 6) ^ ROR(v[4], 11) ^ ROR(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + k[i] + w[i];
            t2 = (ROR(v[0], 2) ^ ROR(v[0], 13) ^ ROR(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
            memmove(v + 1, v, 7 * sizeof v[0]);
            v[4] += t1;
            v[0] = t1 + t2;
        }
        for (i = 0; i < 8; ++i) h[i] += v[i];
    }
    free(buffer);
    for (i = 0; i < 8; ++i) sprintf(hex + 8 * i, "%08x", (unsigned)h[i]);
}

static void map_retail(void)
{
    static uint8_t file[0x30000];
    FILE *stream = fopen("orig/FLTSIM98.EXE", "rb");
    uint32_t pe, sections, at, headers, image_size;
    size_t length;
    char hex[65];
    if (!stream) { fprintf(stderr, "cannot open orig/FLTSIM98.EXE\n"); exit(2); }
    length = fread(file, 1, sizeof file, stream);
    fclose(stream);
    sha256(file, length, hex);
    if (strcmp(hex, REFERENCE_SHA256)) { fprintf(stderr, "orig/FLTSIM98.EXE is not the reference executable (%s)\n", hex); exit(2); }
    pe = *(uint32_t *)(file + 0x3c);
    image_size = *(uint32_t *)(file + pe + 0x50);
    headers = *(uint32_t *)(file + pe + 0x54);
    sections = *(uint16_t *)(file + pe + 6);
    {   /* under wine the region is already reserved: made usable */
        DWORD old;
        if (!VirtualAlloc((void *)(uintptr_t)BASE, image_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE) &&
            !VirtualAlloc((void *)(uintptr_t)BASE, image_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE) &&
            !VirtualProtect((void *)(uintptr_t)BASE, image_size, PAGE_EXECUTE_READWRITE, &old)) {
            fprintf(stderr, "cannot map 0x400000 (%lu)\n", GetLastError());
            exit(2);
        }
    }
    memcpy((void *)(uintptr_t)BASE, file, headers);
    for (at = 0; at < sections; ++at) {
        uint8_t *section = file + pe + 24 + *(uint16_t *)(file + pe + 20) + at * 40;
        memcpy((uint8_t *)(uintptr_t)(BASE + *(uint32_t *)(section + 12)), file + *(uint32_t *)(section + 20), *(uint32_t *)(section + 16));
    }
}

/* ---- the executed instruction and the handler ---- */
static uint8_t *s_code;                 /* executable buffer, zero padded */
static uint8_t s_cell_buffer[64] __attribute__((aligned(16)));
#define CELL (s_cell_buffer + 16)
static volatile int s_side, s_hits, s_bad_code, s_unrepaired, s_flags_changed;
static volatile uint32_t s_step, s_len;

static LONG CALLBACK handler(EXCEPTION_POINTERS *p)
{
    CONTEXT *context = p->ContextRecord;
    uint32_t before, flags;
    int32_t result;
    if (context->Eip != (uint32_t)(uintptr_t)s_code) return EXCEPTION_CONTINUE_SEARCH;
    ++s_hits;
    if (p->ExceptionRecord->ExceptionCode != 0xc0000094u && p->ExceptionRecord->ExceptionCode != 0xc0000095u) s_bad_code = 1;
    before = context->Eip;
    flags = context->EFlags;
    result = s_side ? fs98_host_rt_divide_filter((void **)p) : retail_filter((void **)p);
    if (result != -1) s_unrepaired = 1;
    if (context->EFlags != flags) s_flags_changed = 1;
    s_step = context->Eip - before;
    context->Eip = before + s_len;   /* a wrong step is counted, not followed */
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* ---- randomness and bookkeeping ---- */
static uint32_t s_rng = 0x2545f491u;
static uint32_t rnd(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static unsigned long s_comparisons, s_differ, s_faults, s_printed;
#define REPORT(...) do { ++s_differ; if (s_printed++ < 200) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* ---- encodings ---- */
struct enc { const char *name; int mod, rm, sib, base, index, scale, dsize; uint32_t disp, absolute; };
/* sib: 1 if a SIB byte; base/index -1 for none (base none: disp32); absolute: no registers, disp32 = the cell */
static const struct enc s_encodings[] = {
    {"reg:ecx", 3, 1, 0, -1, -1, 0, 0, 0, 0}, {"reg:ebx", 3, 3, 0, -1, -1, 0, 0, 0, 0},
    {"reg:ebp", 3, 5, 0, -1, -1, 0, 0, 0, 0}, {"reg:esi", 3, 6, 0, -1, -1, 0, 0, 0, 0},
    {"reg:edi", 3, 7, 0, -1, -1, 0, 0, 0, 0}, {"reg:edx", 3, 2, 0, -1, -1, 0, 0, 0, 0},
    {"[ebx]", 0, 3, 0, 3, -1, 0, 0, 0, 0}, {"[esi]", 0, 6, 0, 6, -1, 0, 0, 0, 0},
    {"[abs]", 0, 5, 0, -1, -1, 0, 4, 0, 1},
    {"[edi+0x10]", 1, 7, 0, 7, -1, 0, 1, 0x10, 0}, {"[ecx+0]", 1, 1, 0, 1, -1, 0, 1, 0, 0},
    {"[esi+0x7f]", 1, 6, 0, 6, -1, 0, 1, 0x7f, 0},
    {"[ebp+0x1234]", 2, 5, 0, 5, -1, 0, 4, 0x1234, 0}, {"[ecx-0x40]", 2, 1, 0, 1, -1, 0, 4, 0xffffffc0u, 0},
    {"[ebx+esi*4]", 0, 4, 1, 3, 6, 2, 0, 0, 0}, {"[edi*2+disp32]", 0, 4, 1, -1, 7, 1, 4, 0, 0},
    {"[ebx+ecx*8+0x20]", 1, 4, 1, 3, 1, 3, 1, 0x20, 0}, {"[ebp+edi+0x100]", 2, 4, 1, 5, 7, 0, 4, 0x100, 0},
    {"[ebx](sib)", 0, 4, 1, 3, -1, 0, 0, 0, 0},
};
#define ENCODINGS (sizeof s_encodings / sizeof s_encodings[0])
#define REG_FORMS 6        /* the first six are registers; edx is not valid at 16/32 bits, ch/dh/bh/eax/ah never */

static int encoding_valid(unsigned e, int width)
{
    const struct enc *encoding = &s_encodings[e];
    if (encoding->mod != 3) return 1;
    if (width == 8) return encoding->rm == 1 || encoding->rm == 2 || encoding->rm == 3;   /* cl dl bl */
    return encoding->rm != 2;
}

struct vector { int width, sgn; uint32_t hi, lo, divisor; };

struct result { uint32_t regs[10]; int hits, step, bad_code, unrepaired, flags_changed; };

static void run_side(int side, const uint32_t *in, const uint8_t *insn, uint32_t length, struct result *out)
{
    uint32_t regs[10];
    memcpy(regs, in, sizeof regs);
    memset(s_code, 0, 64);
    memcpy(s_code, insn, length);
    s_code[length] = 0xc3;
    s_side = side; s_hits = 0; s_bad_code = 0; s_unrepaired = 0; s_flags_changed = 0; s_step = 0; s_len = length;
    fs98_run_div(regs, s_code);
    memcpy(out->regs, regs, sizeof regs);
    out->hits = s_hits; out->step = (int)s_step; out->bad_code = s_bad_code; out->unrepaired = s_unrepaired; out->flags_changed = s_flags_changed;
}

static uint32_t helper(const struct vector *v, uint32_t eax, uint32_t edx, uint32_t *eax_out, uint32_t *edx_out)
{
    uint32_t q = 0, r = 0xa5a5a5a5u, q_null = 0, mask;
    if (v->width == 32) {
        q = v->sgn ? fs98_idiv32(v->hi, v->lo, v->divisor, &r) : fs98_div32(v->hi, v->lo, v->divisor, &r);
        q_null = v->sgn ? fs98_idiv32(v->hi, v->lo, v->divisor, 0) : fs98_div32(v->hi, v->lo, v->divisor, 0);
        *eax_out = q; *edx_out = r;
    } else if (v->width == 16) {
        uint16_t r16 = 0xa5a5;
        q = v->sgn ? fs98_idiv16(v->hi, v->lo, v->divisor, &r16) : fs98_div16(v->hi, v->lo, v->divisor, &r16);
        q_null = v->sgn ? fs98_idiv16(v->hi, v->lo, v->divisor, 0) : fs98_div16(v->hi, v->lo, v->divisor, 0);
        *eax_out = (eax & 0xffff0000u) | q; *edx_out = (edx & 0xffff0000u) | r16;
    } else {
        uint8_t r8 = 0xa5, ax_q;
        uint16_t ax = (uint16_t)(v->hi << 8 | v->lo);
        ax_q = v->sgn ? fs98_idiv8(ax, v->divisor, &r8) : fs98_div8(ax, v->divisor, &r8);
        q = ax_q;
        q_null = v->sgn ? fs98_idiv8(ax, v->divisor, 0) : fs98_div8(ax, v->divisor, 0);
        *eax_out = (eax & 0xffff0000u) | (uint32_t)r8 << 8 | ax_q; *edx_out = edx;
    }
    mask = v->width == 32 ? 0xffffffffu : (1u << v->width) - 1;
    return (q_null & mask) != (q & mask);
}

/* whether the operands fault: a zero divisor, or a quotient out of range (independent of the helper) */
static int will_fault(const struct vector *v)
{
    int w = v->width;
    uint64_t mask = w == 32 ? 0xffffffffull : (1ull << w) - 1, sign = 1ull << (w - 1);
    uint64_t d = v->divisor & mask, n = ((uint64_t)v->hi << w | v->lo) & (w == 32 ? ~0ull : (1ull << 2 * w) - 1);
    if (d == 0) return 1;
    if (!v->sgn) return n / d > mask;
    {
        int nneg = (v->hi & sign) != 0, dneg = (d & sign) != 0;
        uint64_t nmag = nneg ? (w == 32 ? 0 - n : (1ull << 2 * w) - n) : n, dmag = dneg ? (1ull << w) - d : d;
        uint64_t q = nmag / dmag;
        return nneg != dneg ? q > sign : q > sign - 1;
    }
}

/* one vector, one encoding, upper = how the divisor's bits above its width are filled (0 zeros,
 * 1 the opposite of its sign, 2 random) */
static void run_vector(const struct vector *v, unsigned e, int upper)
{
    const struct enc *encoding = &s_encodings[e];
    uint32_t in[10], i, wmask = v->width == 32 ? 0xffffffffu : (1u << v->width) - 1, dword, expected_eax, expected_edx, length = 0;
    uint8_t insn[16];
    struct result a, b;
    int differs = 0;
    uint32_t cell = (uint32_t)(uintptr_t)CELL, disp = encoding->disp;
    for (i = 0; i < 8; ++i) in[i] = rnd();
    in[8] = 0x202 | (rnd() & 0x8d5);
    in[9] = 0;
    if (v->width == 32) { in[0] = v->lo; in[2] = v->hi; }
    else if (v->width == 16) { in[0] = (in[0] & 0xffff0000u) | v->lo; in[2] = (in[2] & 0xffff0000u) | v->hi; }
    else in[0] = (in[0] & 0xffff0000u) | v->hi << 8 | v->lo;
    dword = v->divisor & wmask;
    if (v->width != 32) {
        if (upper == 1) dword |= ((v->divisor >> (v->width - 1)) & 1) ? 0 : ~wmask;
        else if (upper == 2) dword |= rnd() & ~wmask;
    }
    for (i = 0; i < sizeof s_cell_buffer; ++i) s_cell_buffer[i] = (uint8_t)rnd();
    memcpy(CELL, &dword, 4);
    /* the instruction */
    if (v->width == 16) insn[length++] = 0x66;
    insn[length++] = v->width == 8 ? 0xf6 : 0xf7;
    insn[length++] = (uint8_t)(encoding->mod << 6 | (v->sgn ? 7 : 6) << 3 | (encoding->sib ? 4 : encoding->rm));
    if (encoding->mod == 3) in[encoding->rm] = dword;
    else {
        uint32_t index_part = encoding->index >= 0 ? 2u << encoding->scale : 0;
        if (encoding->index >= 0) in[encoding->index] = 2;
        if (encoding->absolute || encoding->base < 0) disp = cell - index_part;
        else in[encoding->base] = cell - index_part - (encoding->dsize == 1 ? (uint32_t)(uint8_t)disp : disp);
        if (encoding->sib)
            insn[length++] = (uint8_t)(encoding->scale << 6 | (encoding->index >= 0 ? encoding->index : 4) << 3 | (encoding->base >= 0 ? encoding->base : 5));
        if (encoding->dsize == 1) insn[length++] = (uint8_t)disp;
        else if (encoding->dsize == 4) { memcpy(insn + length, &disp, 4); length += 4; }
    }
    helper(v, in[0], in[2], &expected_eax, &expected_edx);
    run_side(0, in, insn, length, &a);
    run_side(1, in, insn, length, &b);
    ++s_comparisons;
    if (a.hits) ++s_faults;
    {
        uint32_t q_mismatch = helper(v, in[0], in[2], &expected_eax, &expected_edx);
        uint32_t expected[8];
        memcpy(expected, in, sizeof expected);
        expected[0] = expected_eax; expected[2] = expected_edx;
        if (q_mismatch) { REPORT("NULL remainder changes the quotient"); differs = 1; }
        if (a.hits != will_fault(v) || b.hits != will_fault(v)) { REPORT("hits retail %d ours %d, expected %d", a.hits, b.hits, will_fault(v)); differs = 1; }
        if (a.flags_changed || b.flags_changed) { REPORT("a filter changed EFLAGS: retail %d ours %d", a.flags_changed, b.flags_changed); differs = 1; }
        if (a.bad_code || b.bad_code || a.unrepaired || b.unrepaired) { REPORT("bad code %d/%d or unrepaired %d/%d", a.bad_code, b.bad_code, a.unrepaired, b.unrepaired); differs = 1; }
        if (a.hits && (a.step != (int)length || b.step != (int)length)) { REPORT("EIP step retail %d ours %d, length %u", a.step, b.step, length); differs = 1; }
        if (a.hits && (((a.regs[9] ^ in[8]) | (b.regs[9] ^ in[8])) & 0x8d5)) { REPORT("flags changed on a repair: in %#x retail %#x ours %#x", in[8], a.regs[9], b.regs[9]); differs = 1; }
        for (i = 0; i < 8; ++i) {
            if (i == 4) continue;
            if (a.regs[i] != b.regs[i]) { REPORT("reg %u retail %#x ours %#x", i, a.regs[i], b.regs[i]); differs = 1; }
            if (a.regs[i] != expected[i]) { REPORT("reg %u retail %#x helper %#x", i, a.regs[i], expected[i]); differs = 1; }
        }
    }
    if (differs)
        fprintf(stderr, "  ^ %s%s %d-bit %s %#x:%#x / %#x (upper %d) hits %d\n", v->sgn ? "idiv" : "div", "", v->width, encoding->name,
                (unsigned)v->hi, (unsigned)v->lo, (unsigned)v->divisor, upper, a.hits);
}

static void run_all_encodings(const struct vector *v, int uppers)
{
    unsigned e;
    int upper;
    for (e = 0; e < ENCODINGS; ++e) {
        if (!encoding_valid(e, v->width)) continue;
        for (upper = 0; upper < (v->width == 32 ? 1 : uppers); ++upper) run_vector(v, e, upper);
    }
}

static void add(int width, int sgn, uint64_t hi, uint64_t lo, uint64_t divisor)
{
    uint64_t mask = width == 32 ? 0xffffffffu : (1u << width) - 1;
    struct vector v = {width, sgn, (uint32_t)(hi & mask), (uint32_t)(lo & mask), (uint32_t)(divisor & mask)};
    run_all_encodings(&v, 2);
}

static void add_n(int width, int sgn, uint64_t n, uint64_t divisor)
{
    add(width, sgn, n >> width, n, divisor);
}

static void fixed_unsigned(int width)
{
    uint64_t m = width == 32 ? 0xffffffffu : (1u << width) - 1, s = 1ull << (width - 1);
    uint64_t divisors[] = {1, 2, 3, 5, 0x10, s, s + 1, m}, top = width == 32 ? ~0ull : (1ull << (2 * width)) - 1;
    unsigned i;
    add(width, 0, 0, 5, 0); add(width, 0, m, m, 0); add(width, 0, 0, 0, 0); add(width, 0, s, 0, 0);
    for (i = 0; i < sizeof divisors / sizeof divisors[0]; ++i) {
        uint64_t d = divisors[i], ok = d * m + (d - 1), over = d * (m + 1), n[] = {0, 1, d - 1, d, d + 1, 8 * d - 1, ok, ok - 1, over, over + 1, top};
        unsigned j;
        for (j = 0; j < sizeof n / sizeof n[0]; ++j) add_n(width, 0, n[j] & top, d);
    }
}

static void fixed_signed(int width)
{
    int64_t qmax = (1ll << (width - 1)) - 1, qmin = -(1ll << (width - 1)), m = width == 32 ? 0xffffffffll : (1ll << width) - 1;
    int64_t divisors[] = {1, -1, 2, -2, 3, -3, 7, -7, 0x10, -0x10, qmax, qmin, qmax - 1, qmin + 1};
    int64_t quotients[] = {qmax, qmin, qmax + 1, qmin - 1, 0, 1, -1, 5, -5};
    int64_t his[] = {0, m, 1ll << (width - 1), qmax}, los[] = {5, 0, m, 1ll << (width - 1)};
    unsigned i, j, k;
    for (i = 0; i < sizeof divisors / sizeof divisors[0]; ++i) {
        int64_t d = divisors[i], remainders[] = {0, (d < 0 ? -d : d) - 1, -((d < 0 ? -d : d) - 1)};
        for (j = 0; j < sizeof quotients / sizeof quotients[0]; ++j)
            for (k = 0; k < 3; ++k) add_n(width, 1, (uint64_t)(quotients[j] * d + remainders[k]), (uint64_t)d);
    }
    add(width, 1, 0, 5, 0); add(width, 1, m, 5, 0); add(width, 1, 0, 5, m); add(width, 1, m, 5, m);
    add(width, 1, 0, 5, 1ull << (width - 1)); add(width, 1, m, 5, 1ull << (width - 1)); add(width, 1, 0, 5, 2); add(width, 1, m, 5, 2);
    add(width, 1, 1ull << (width - 1), 0, m);      /* the full most negative dividend by -1 */
    add(width, 1, m, 1ull << (width - 1), m);      /* the sign-extended minimum by -1 */
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            for (k = 0; k < 5; ++k) {
                uint64_t d[] = {1, m, 1ull << (width - 1), 2, 0};
                add(width, 1, his[i], los[j], d[k]);
            }
}

static void fixed_matrix(void)
{
    add(32, 0, 0, 5, 0); add(32, 0, 1, 0, 1);
    add(32, 1, 0xffffffffu, 0x80000000u, 0xffffffffu); add(32, 1, 0x80000000u, 0, 0xffffffffu); add(32, 1, 0xffffffffu, 5, 0);
    add(16, 1, 0xffff, 0xfff9, 2);
    add(8, 1, 0x7f, 0x00, 1);
    add(8, 0, 0x00, 0xfe, 0x10);
}

static void random_vector(int width, int sgn)
{
    int64_t m = width == 32 ? 0xffffffffll : (1ll << width) - 1, s = 1ll << (width - 1);
    uint64_t hi, lo, d;
    int kind = rnd() & 3;
    struct vector v;
    lo = rnd() & m; hi = rnd() & m; d = rnd() & m;
    if (kind == 1) { uint32_t pick = rnd() % 6; d = pick == 0 ? 0 : pick == 1 ? 1 : pick == 2 ? (uint64_t)m : pick == 3 ? 2 : pick == 4 ? (uint64_t)s : rnd() % 16; }
    if (kind >= 2 && !sgn) {            /* force overflow: top half >= divisor */
        if (!d) d = 1 + rnd() % 100;
        d &= m; if (!d) d = 1;
        hi = d + rnd() % (uint64_t)(m + 1 - d);
    } else if (kind >= 2) {             /* a quotient one or a few past the limit, either side */
        int64_t dd = (int64_t)(rnd() & m); int64_t q = (rnd() & 1) ? s + (rnd() & 3) : -s - 1 - (rnd() & 3);
        uint64_t n;
        if (dd >= s) dd -= m + 1;
        if (!dd || (rnd() & 7) == 0) dd = (rnd() & 1) ? -1 : 0;
        {
            int64_t base = q * (dd ? dd : 1), extra = dd ? (dd < 0 ? -dd : dd) - 1 : 0;
            n = (uint64_t)(base < 0 ? base - extra : base + extra);   /* the remainder has the dividend's sign */
        }
        if (dd == -1 && (rnd() & 1)) n = (uint64_t)-s;
        hi = (n >> width) & m; lo = n & m; d = (uint64_t)dd & m;
    }
    v.width = width; v.sgn = sgn; v.hi = (uint32_t)hi; v.lo = (uint32_t)lo; v.divisor = (uint32_t)d;
    for (;;) { unsigned e = rnd() % ENCODINGS; if (encoding_valid(e, width)) { run_vector(&v, e, width == 32 ? 0 : (int)(rnd() % 3)); break; } }
}

/* ---- both filters with synthetic records ---- */
static void synthetic(void)
{
    static const uint32_t codes[] = {0xc0000094u, 0xc0000095u, 0xc0000005u};
    static const uint8_t forms[][4] = {{0xf7, 0xfb}, {0xf7, 0xf3}, {0x66, 0xf7, 0xfb}, {0xf6, 0xfb}, {0xf6, 0xf1}, {0xf7, 0xc3},
                                       {0xf7, 0xfc}, {0x8f, 0xf3}};
    static const uint32_t lengths[] = {2, 2, 3, 2, 2, 2, 2, 2};
    /* the EIP step of a repair: the length, one more for DIVIDE-SIB-REGISTER-STEP (idiv esp); 0 and -1 returns for no repair */
    static const uint32_t steps[] = {2, 2, 3, 2, 2, 0, 3, 0};
    static const int32_t returns[] = {-1, -1, -1, -1, -1, 0, -1, 0};
    unsigned c, f, side;
    for (c = 0; c < 3; ++c)
        for (f = 0; f < 8; ++f) {
            CONTEXT contexts[2];
            int32_t results[2];
            for (side = 0; side < 2; ++side) {
                EXCEPTION_RECORD record;
                void *pointers[2];
                uint32_t at;
                memset(s_code, 0, 64);
                memcpy(s_code, forms[f], lengths[f]);
                memset(&record, 0, sizeof record);
                record.ExceptionCode = codes[c];
                memset(&contexts[side], 0, sizeof contexts[side]);
                contexts[side].Eax = 0x12345678; contexts[side].Edx = 0x9abcdef0; contexts[side].Ebx = 0xfffffffe;
                contexts[side].Ecx = 0x00000081; contexts[side].Eip = (uint32_t)(uintptr_t)s_code;
                contexts[side].EFlags = 0x246;
                pointers[0] = &record; pointers[1] = &contexts[side];
                results[side] = side ? fs98_host_rt_divide_filter(pointers) : retail_filter(pointers);
                at = contexts[side].Eip - (uint32_t)(uintptr_t)s_code;
                if (results[side] != (c < 2 ? returns[f] : 0) || at != (c < 2 ? steps[f] : 0))
                    REPORT("synthetic code %#x form %u side %u: returned %d, EIP step %u", codes[c], f, side, (int)results[side], at);
            }
            ++s_comparisons;
            if (results[0] != results[1] || memcmp(&contexts[0], &contexts[1], sizeof contexts[0]))
                REPORT("synthetic code %#x form %u: retail and ours differ", codes[c], f);
        }
}

int main(int argc, char **argv)
{
    unsigned n = 2000, i, width, sgn, helpers = 0;
    char *end;
    if (argc > 1) { n = (unsigned)strtoul(argv[1], &end, 0); if (!*argv[1] || *end) { fprintf(stderr, "bad N: %s\n", argv[1]); return 2; } }
    if (argc > 2) { s_rng = (uint32_t)strtoul(argv[2], &end, 0) | 1; if (!*argv[2] || *end) { fprintf(stderr, "bad seed: %s\n", argv[2]); return 2; } }
    map_retail();
    s_code = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_code) { fprintf(stderr, "no code buffer\n"); return 2; }
    AddVectoredExceptionHandler(1, handler);
    synthetic();
    fixed_matrix();
    for (width = 8; width <= 32; width *= 2)
        for (sgn = 0; sgn < 2; ++sgn) {
            if (sgn) fixed_signed(width); else fixed_unsigned(width);
            for (i = 0; i < n; ++i) random_vector(width, sgn);
            ++helpers;
        }
    fprintf(stderr, "(N=%u; %lu of them faulted)\n", n, s_faults);
    printf("%u helpers, %lu comparisons, %lu differ\n", helpers, s_comparisons, s_differ);
    return s_differ ? 1 : 0;
}
