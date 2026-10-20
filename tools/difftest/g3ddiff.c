/* Differential test: retail G3D.DLL against the OpenFS98 build.
 *
 * Both DLLs are loaded into one process -- retail first, so it sits at its
 * own base and the code addresses its image keeps are the file's -- and
 * driven through the Linkage and the CHAIN callbacks they register, one
 * side after the other over the same script.  The harness grows with the
 * transliteration (docs/modules/g3d.md); what it covers now:
 *
 *   - init and deinit around every scenario;
 *   - the self-contained services +0x3c, +0x40, +0x44, +0x5c, +0x64, and
 *     +0x54, +0x58, +0x60 over a texture table seeded with slots;
 *   - the interpreter, through +0x18, +0x1c, +0x20, +0x24 and +0x28, on
 *     programs from a generator: the control-flow opcodes (end, return,
 *     jumps, calls relative, 32-bit and absolute, through a variable, case,
 *     the ifs in range and masked), variables (set, move, the context) and
 *     the opcodes that set GLOBAL's drawing state.
 *
 * What G3D imports:
 *
 *   host     a stub: memory is a bump arena reset before each side, so heap
 *            pointers come out the same; the growable lists are the
 *            host's (+0x2f8..+0x304).
 *   CHAIN    a logging stub; a callback is named by the order it was first
 *            registered in, so retail's and ours pair up.
 *   HG2D     logging stubs; +0xb0 releases a texture.
 *   the rest are not called yet.
 *
 * Every service is called through call_regs(), which sets all seven
 * registers to scripted values first and records all seven after, and the
 * stack's balance: G3D's assembly passes and returns values in registers,
 * and callers read what a service leaves.
 *
 * Compared: the transcript of every import call and every register after
 * every service call; and after every operation, pairwise, G3D's whole
 * image, GLOBAL and the arena, with a dword allowed to differ only when
 * each side's points at the same offset in its own image (G3D keeps
 * pointers into itself, some made at run time); and the programs and the
 * context buffer by hash.  A fault is an outcome: logged, and the scenario
 * ends on both sides.  The host repairs integer divide faults (0x4119a0,
 * see docs/modules/host.md), and retail G3D relies on it, so a vectored
 * handler here does what the filter does.
 *
 * Kept clear of, because retail hangs or faults on them by construction:
 *   - opcodes 0x01, 0x4b and 0x59, a bare ret that never moves the counter;
 *   - backward jumps (a program that loops forever);
 *   - variable numbers outside the table, which read pointers out of the
 *     image's other data;
 *   - 0x8c (native call), which needs a routine's address.
 *
 * Build: make g3ddiff     Run: make g3ddiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE      0x18450u
#define LINKAGE         0x58u
#define LINKAGE_END     (LINKAGE + 0x68u)
#define GLOBAL_SIZE     0x1300u
#define TRANSCRIPT_SIZE (8u << 20)
#define MAX_OPS         40
#define ARENA_SIZE      (4u << 20)
#define PROGRAM_SIZE    0x40000u     /* the case opcode reads 0x20006 on */
#define CONTEXT_SIZE    0x20000u
#define VAR_TABLE       0x6950u      /* the variable table in the image */
#define VAR_ENTRIES     9124u

struct import { uint32_t id; void *link; };

struct snapshot {
    uint8_t image[IMAGE_SIZE];
    uint8_t global[GLOBAL_SIZE];
    uint32_t arena_used;
    uint8_t *arena;
    uint32_t programs_hash, context_hash;
};

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    uint8_t *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    char *transcript;
    uint32_t length;
    struct snapshot *snap;
    uint32_t nsnap;
    uint32_t callbacks[32];          /* by first registration */
    uint32_t ncallbacks;
};

static struct side retail, ours;
static struct side *cur;
static uint8_t global[GLOBAL_SIZE];
static uint8_t host[0x320], chain[0x40], hg2d[0x100], fe[0x80],
               weatherm[0x40], atc[0x40], numeric[0x40];
static uint8_t buffer[0x400];        /* instructions for +0x64 */
static uint8_t *arena, *programs, *context;
static uint8_t window_rec[0x100], view_block[0x60];  /* GLOBAL +0x1194, +0x1198 */
static uint32_t arena_used;

static uint32_t scenario, op_number, scenario_seed, rng;
static int counting, faulted;
static unsigned long cover_ops[32], cover_divfix, cover_faults, cover_shift_masked,
                     cover_opcodes[0x100], cover_norm[34], cover_emitted[0x100],
                     cover_programs, cover_released,
                     cover_context, cover_mapped, cover_unmapped, cover_steered,
                     cover_drawn[10];

/* ---- the transcript ------------------------------------------------------------ */

static void put(const char *fmt, ...)
{
    va_list ap;
    int n;

    if (cur == NULL)
        return;
    va_start(ap, fmt);
    n = vsnprintf(cur->transcript + cur->length, TRANSCRIPT_SIZE - cur->length, fmt, ap);
    va_end(ap);
    if (n > 0 && cur->length + (uint32_t)n < TRANSCRIPT_SIZE)
        cur->length += (uint32_t)n;
}

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/* A value of a random bit length (0..32) and sign: magnitudes spread over
 * every range, not clustered at the top. */
static uint32_t spread(void)
{
    uint32_t bits = rnd() % 34, v;

    if (bits == 0)
        return 0;
    if (bits >= 33)
        return rnd() & 1 ? 0x80000000u : 0x7fffffffu;
    v = rnd() & (bits == 32 ? 0xffffffffu : (1u << bits) - 1);
    v |= 1u << (bits - 1);
    return rnd() & 1 ? (uint32_t)-(int32_t)v : v;
}

static uint32_t fnv(const uint8_t *p, uint32_t n)
{
    uint32_t h = 2166136261u;

    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

/* A value that points into side s's image, as 0xd0000000 | offset. */
static uint32_t as_offset(const struct side *s, uint32_t v)
{
    if (v >= (uint32_t)(uintptr_t)s->image && v < (uint32_t)(uintptr_t)s->image + IMAGE_SIZE)
        return 0xd0000000u | (v - (uint32_t)(uintptr_t)s->image);
    return v;
}

/* A pointer the import stubs log: into the arena or a harness buffer by
 * name and offset (the same on both sides), into the image as an offset,
 * else tagged for pairing. */
static void put_ptr(uint32_t v)
{
    if (v >= (uint32_t)(uintptr_t)arena && v < (uint32_t)(uintptr_t)arena + ARENA_SIZE)
        put("arena+%x", v - (uint32_t)(uintptr_t)arena);
    else if (v >= (uint32_t)(uintptr_t)programs && v < (uint32_t)(uintptr_t)programs + PROGRAM_SIZE)
        put("prog+%x", v - (uint32_t)(uintptr_t)programs);
    else if (v >= (uint32_t)(uintptr_t)global && v < (uint32_t)(uintptr_t)global + GLOBAL_SIZE)
        put("G+%x", v - (uint32_t)(uintptr_t)global);
    else if (v >= (uint32_t)(uintptr_t)window_rec && v < (uint32_t)(uintptr_t)window_rec + sizeof window_rec)
        put("win+%x", v - (uint32_t)(uintptr_t)window_rec);
    else
        put("R%08x", v);
}

/* ---- calling with every register set ------------------------------------------- */

uint32_t regs_in[7], regs_out[7];   /* eax ebx ecx edx esi edi ebp */
uint32_t call_target, call_args[8], call_nargs, saved_esp, esp_after;

void call_regs(void);
__asm__(".text\n_call_regs:\n\t"
        "pushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl %esp, _saved_esp\n\t"
        "movl _call_nargs, %ecx\n"
        "1:\n\ttestl %ecx, %ecx\n\tjz 2f\n\t"
        "pushl _call_args-4(,%ecx,4)\n\tdecl %ecx\n\tjmp 1b\n"
        "2:\n\t"
        "movl _regs_in+4, %ebx\n\tmovl _regs_in+8, %ecx\n\t"
        "movl _regs_in+12, %edx\n\tmovl _regs_in+16, %esi\n\t"
        "movl _regs_in+20, %edi\n\tmovl _regs_in+24, %ebp\n\t"
        "movl _regs_in, %eax\n\t"
        "call *_call_target\n\t"
        "movl %eax, _regs_out\n\tmovl %ebx, _regs_out+4\n\t"
        "movl %ecx, _regs_out+8\n\tmovl %edx, _regs_out+12\n\t"
        "movl %esi, _regs_out+16\n\tmovl %edi, _regs_out+20\n\t"
        "movl %ebp, _regs_out+24\n\t"
        "movl %esp, _esp_after\n\t"
        "movl _saved_esp, %esp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");

/* ---- faults, and the host's divide repair ---------------------------------------- */

static void *fault_jb[5];
static int in_g3d;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

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

/* A tracer for retail, for finding where the sides part: G3D_BREAK lists
 * retail addresses; each gets an int3, and when it is hit the registers go
 * to g3d_retail_trace.txt, the byte is put back for one single step, and
 * the int3 goes back after it.  Ours logs the same points from a build
 * with -DG3D_TRACE (g3d_ours_trace.txt).  Never break on the mov
 * [esp-7],0x4050607 that starts an import call's alignment: the single
 * step's exception frame overwrites the bytes it wrote below esp, and the
 * call goes out misaligned (retail then differs for the tracer's sake). */
static uint32_t break_at[32], nbreaks, stepping_from;
static uint8_t break_byte[32];
static FILE *trace_file;

static void arm_breaks(void)
{
    const char *list = getenv("G3D_BREAK");
    DWORD old;
    char *end;

    if (list == NULL)
        return;
    while (*list && nbreaks < 32) {
        uint32_t a = (uint32_t)strtoul(list, &end, 16);

        if (end == list)
            break;
        break_at[nbreaks] = a;
        VirtualProtect((void *)(uintptr_t)a, 1, PAGE_EXECUTE_READWRITE, &old);
        break_byte[nbreaks] = *(uint8_t *)(uintptr_t)a;
        *(uint8_t *)(uintptr_t)a = 0xcc;
        nbreaks++;
        list = *end ? end + 1 : end;
    }
    trace_file = fopen("g3d_retail_trace.txt", "w");
}

static int on_break(EXCEPTION_POINTERS *e)
{
    CONTEXT *c = e->ContextRecord;
    DWORD code = e->ExceptionRecord->ExceptionCode;
    uint32_t i;

    if (code == EXCEPTION_BREAKPOINT) {
        for (i = 0; i < nbreaks; i++)
            if (break_at[i] == (uint32_t)(uintptr_t)e->ExceptionRecord->ExceptionAddress)
                break;
        if (i == nbreaks)
            return 0;
        if (trace_file && cur == &retail)
            fprintf(trace_file, "%08x eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x ebp %08x\n",
                    break_at[i], (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx,
                    (unsigned)c->Edx, (unsigned)c->Esi, (unsigned)c->Edi, (unsigned)c->Ebp);
        /* G3D_BREAKFRAME: the 0x60 bytes from ebp too */
        if (trace_file && cur == &retail && getenv("G3D_BREAKFRAME")) {
            uint32_t k;

            fprintf(trace_file, "  frame:");
            for (k = 0; k < 0x60; k += 2)
                fprintf(trace_file, "%s%04x", k % 16 ? " " : "\n   +", *(uint16_t *)(uintptr_t)(c->Ebp + k));
            fprintf(trace_file, "\n");
        }
        *(uint8_t *)(uintptr_t)break_at[i] = break_byte[i];
        c->Eip = break_at[i];
        c->EFlags |= 0x100;
        stepping_from = break_at[i];
        return 1;
    }
    if (code == EXCEPTION_SINGLE_STEP && stepping_from) {
        *(uint8_t *)(uintptr_t)stepping_from = 0xcc;
        stepping_from = 0;
        c->EFlags &= ~0x100u;
        return 1;
    }
    return 0;
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;

    if (nbreaks && on_break(e))
        return EXCEPTION_CONTINUE_EXECUTION;
    if (!in_g3d)
        return EXCEPTION_CONTINUE_SEARCH;
    if ((code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW) &&
        div_fixup(e->ContextRecord)) {
        if (counting)
            cover_divfix++;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_INT_OVERFLOW || code == EXCEPTION_STACK_OVERFLOW) {
        uint8_t *at = (uint8_t *)e->ExceptionRecord->ExceptionAddress;
        const char *where = "elsewhere";

        if (at >= retail.lo && at < retail.hi)
            where = "retail";
        else if (at >= ours.lo && at < ours.hi)
            where = "ours";
        /* Where it faulted differs between the builds; that it did, and
         * the kind, does not. */
        put(" FAULT(%#lx)", code);
        if (getenv("G3D_VERBOSE"))
            printf("scenario %u op %u: %s faulted (%#lx) %s at %p\n", scenario, op_number,
                   cur ? cur->name : "?", code, where, (void *)at);
        if (cur == &retail && counting)
            cover_faults++;
        faulted = 1;
        e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
        e->ContextRecord->Esp = saved_esp - 64;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---- the host ------------------------------------------------------------------- */

#define SET_SLOT(base, off, fn) (*(void **)((base) + (off)) = (void *)(fn))

struct host_list {
    int32_t count, capacity, grow, elem_size, iterating, pending;
    uint8_t *data;
};

/* A block's size sits in the 16 bytes before it. */
static void *arena_get(uint32_t size)
{
    uint8_t *p;

    arena_used = (arena_used + 15) & ~15u;
    if (arena_used + size + 16 > ARENA_SIZE) {
        printf("scenario %u: arena exhausted\n", scenario);
        exit(2);
    }
    p = arena + arena_used;
    *(uint32_t *)p = size;
    arena_used += size + 16;
    return p + 16;
}

static __attribute__((used)) int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p;

    if (size > 0x400000) {
        put(" alloc(%d,%#x)=-1", fl, size);
        return -1;
    }
    p = arena_get(size);
    memset(p, zero ? 0 : 0xa5, size);
    put(" alloc(%d,%#x,%d,", fl, size, zero);
    put_ptr((uint32_t)(uintptr_t)out);
    put(")=");
    put_ptr((uint32_t)(uintptr_t)p);
    *out = p;
    return 0;
}

static __attribute__((used)) int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    uint8_t *old = *pp, *p;
    uint32_t n;

    if (size > 0x400000) {
        put(" realloc(%d,%#x)=-1", fl, size);
        return -1;
    }
    p = arena_get(size);
    memset(p, 0xa5, size);
    if (old) {
        n = *(uint32_t *)(old - 16);
        memcpy(p, old, n < size ? n : size);
        memset(old, 0xdd, n);
    }
    put(" realloc(%d,%#x)=", fl, size);
    put_ptr((uint32_t)(uintptr_t)p);
    *pp = p;
    return 0;
}

static __attribute__((used)) int32_t STDCALL h_free(void **pp)
{
    put(" free(");
    put_ptr((uint32_t)(uintptr_t)pp);
    put(":");
    put_ptr((uint32_t)(uintptr_t)*pp);
    put(")");
    if (*pp != NULL) {
        uint8_t *p = *pp;
        uint32_t size = *(uint32_t *)(p - 16);

        /* a block the arena did not give, or whose size before it has been
         * written over, is reported, not poisoned: poisoning it once
         * wiped a transcript */
        if (p < arena + 16 || p >= arena + ARENA_SIZE || size > (uint32_t)(arena + ARENA_SIZE - p))
            put(" BADFREE");
        else
            memset(p, 0xdd, size);
    }
    *pp = NULL;
    return 0;
}

static __attribute__((used)) void *STDCALL h_list_add(struct host_list *l)
{
    uint8_t *e;

    if (l->count >= l->capacity) {
        uint8_t *d;
        int32_t cap = l->capacity + (l->grow > 0 ? l->grow : 1);

        d = arena_get((uint32_t)(cap * l->elem_size));
        if (l->data) {
            memcpy(d, l->data, (size_t)(l->count * l->elem_size));
            memset(l->data, 0xdd, (size_t)(l->capacity * l->elem_size));
        }
        l->data = d;
        l->capacity = cap;
    }
    e = l->data + l->count * l->elem_size;
    memset(e, 0, (size_t)l->elem_size);
    l->count++;
    put(" listadd(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(")=");
    put_ptr((uint32_t)(uintptr_t)e);
    return e;
}

static __attribute__((used)) int32_t STDCALL h_list_free(struct host_list *l)
{
    put(" listfree(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(")");
    l->data = NULL;
    return 0;
}

static __attribute__((used)) void STDCALL h_list_remove(struct host_list *l, void *elem)
{
    uint32_t i = (uint32_t)((uint8_t *)elem - l->data) / (uint32_t)l->elem_size;

    put(" listremove(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(",%u)", i);
    if ((int32_t)i < l->count) {
        memmove(l->data + i * l->elem_size, l->data + (i + 1) * l->elem_size,
                (size_t)((l->count - (int32_t)i - 1) * l->elem_size));
        l->count--;
    }
}

static __attribute__((used)) int32_t STDCALL h_list_clear(struct host_list *l)
{
    put(" listclear(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(")");
    l->count = 0;
    return 0;
}

static int32_t STDCALL h_unexpected0(void)
{
    put(" HOST?");
    return 0;
}

/* ---- CHAIN and HG2D ---------------------------------------------------------------- */

static uint32_t callback_name(uint32_t cb)
{
    uint32_t i;

    for (i = 0; i < cur->ncallbacks; i++)
        if (cur->callbacks[i] == cb)
            return i;
    if (cur->ncallbacks < 32)
        cur->callbacks[cur->ncallbacks++] = cb;
    return i;
}

static __attribute__((used)) int32_t STDCALL chain_add(uint32_t list, uint32_t cb)
{
    put(" chainadd(%#x,cb%u)", list, callback_name(cb));
    return 0;
}

static __attribute__((used)) int32_t STDCALL chain_remove(uint32_t list, uint32_t cb)
{
    put(" chainremove(%#x,cb%u)", list, callback_name(cb));
    return 0;
}

static __attribute__((used)) int32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    put(" chainsend(%#x,%#x,%#x)", list, a, b);
    return 0;
}

static __attribute__((used)) int32_t STDCALL hg2d_unload(uint32_t tex, uint32_t zero)
{
    put(" unload(%#x,%u)", tex, zero);
    return 0;
}

/* The drawing calls, logged with every argument; a surface or record as
 * its place in the harness's window record. */
static int32_t STDCALL __attribute__((used)) hg2d_point(uint32_t surf, uint32_t x, uint32_t y,
                                                        uint32_t z)
{
    put(" point(");
    put_ptr(surf);
    put(",%#x,%#x,%#x)", x, y, z);
    if (counting) cover_drawn[0]++;
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_line(uint32_t surf, uint32_t x1, uint32_t y1,
                                                       uint32_t x2, uint32_t y2)
{
    put(" line(");
    put_ptr(surf);
    put(",%#x,%#x,%#x,%#x)", x1, y1, x2, y2);
    if (counting) cover_drawn[1]++;
    return 0;
}

/* The closes log GLOBAL +0xc90, the drawing mode HG2D reads as it fills. */
static int32_t STDCALL __attribute__((used)) hg2d_polygon(uint32_t surf)
{
    put(" polygon(");
    put_ptr(surf);
    put(",mode %x", *(uint16_t *)(global + 0xc90));
    put(")");
    if (counting) cover_drawn[2]++;
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_vertex(uint32_t rec, uint32_t x, uint32_t y,
                                                         uint32_t w)
{
    /* HG2D counts the polygon's vertices in GLOBAL +0x2f0 (0x20732b09);
     * G3D's close draws only when the count survives its decrement */
    (*(uint32_t *)(global + 0x2f0))++;
    put(" v(");
    put_ptr(rec);
    put(",%#x,%#x,%#x)", x, y, w);
    if (counting) cover_drawn[3]++;
    return 0;
}

/* +0x74 and +0x78: the shaded polygon's vertex (its shade a word: retail
 * pushes it as one) and its close.  +0x74 counts the vertices in GLOBAL
 * +0x2f0 as +0x44 does (0x207853da); neither keeps eax, ecx, edx. */
static int32_t STDCALL __attribute__((used)) hg2d_shaded_vertex(uint32_t rec, uint32_t x,
                                                                uint32_t y, uint32_t w,
                                                                uint32_t shade)
{
    (*(uint32_t *)(global + 0x2f0))++;
    put(" sv(");
    put_ptr(rec);
    put(",%#x,%#x,%#x,%#x)", x, y, w, shade & 0xffff);
    if (counting) cover_drawn[4]++;
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_shaded_polygon(uint32_t surf)
{
    put(" spolygon(");
    put_ptr(surf);
    put(",mode %x", *(uint16_t *)(global + 0xc90));
    put(")");
    if (counting) cover_drawn[5]++;
    return 0;
}

/* +0x48 and +0x4c: the textured polygon's vertex (u, v as 16.16) and its
 * close; +0x48 counts the vertices in GLOBAL +0x2f0 (0x20787ac0). */
static int32_t STDCALL __attribute__((used)) hg2d_textured_vertex(uint32_t rec, uint32_t x,
                                                                  uint32_t y, uint32_t w,
                                                                  uint32_t u, uint32_t v)
{
    (*(uint32_t *)(global + 0x2f0))++;
    put(" tv(");
    put_ptr(rec);
    put(",%#x,%#x,%#x,%#x,%#x)", x, y, w, u, v);
    if (counting) cover_drawn[6]++;
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_textured_polygon(uint32_t surf)
{
    put(" tpolygon(");
    put_ptr(surf);
    put(",mode %x", *(uint16_t *)(global + 0xc90));
    put(")");
    if (counting) cover_drawn[7]++;
    return 0;
}

/* +0x50 and +0x54: the shaded textured polygon's vertex (u, v and the
 * shade as 16.16) and its close; +0x50 counts the vertices in GLOBAL
 * +0x2f0 (0x20788717, or through +0x44 with GLOBAL +0xcb6 clear). */
static int32_t STDCALL __attribute__((used)) hg2d_st_vertex(uint32_t rec, uint32_t x,
                                                            uint32_t y, uint32_t w,
                                                            uint32_t u, uint32_t v,
                                                            uint32_t shade)
{
    (*(uint32_t *)(global + 0x2f0))++;
    put(" stv(");
    put_ptr(rec);
    put(",%#x,%#x,%#x,%#x,%#x,%#x)", x, y, w, u, v, shade);
    if (counting) cover_drawn[8]++;
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_st_polygon(uint32_t surf)
{
    put(" stpolygon(");
    put_ptr(surf);
    put(",mode %x", *(uint16_t *)(global + 0xc90));
    put(")");
    if (counting) cover_drawn[9]++;
    return 0;
}

/* +0x28: a point with a value -- the variable's address and a word (pushed
 * as one). */
static int32_t STDCALL __attribute__((used)) hg2d_value_point(uint32_t surf, uint32_t x,
                                                              uint32_t y, uint32_t z,
                                                              uint32_t var, uint32_t word)
{
    put(" vpoint(");
    put_ptr(surf);
    put(",%#x,%#x,%#x,", x, y, z);
    put_ptr(var);
    put(",%#x)", word & 0xffff);
    return 0;
}

/* +0xa8: a texture loaded by name (13 bytes), context and kind: a handle,
 * or 0 for one that will not load. */
static int32_t STDCALL __attribute__((used)) hg2d_load_texture(const char *name, uint32_t context,
                                                               uint32_t kind)
{
    uint32_t h = 0x811c9dc5u, i;

    put(" load(%.13s,", name);
    put_ptr(context);
    put(",%#x)", kind);
    for (i = 0; i < 13; i++)
        h = (h ^ (uint8_t)name[i]) * 0x01000193u;
    return (h & 7) == 0 ? 0 : (int32_t)(0x2000 + (h & 0xfff));
}

/* +0x84: a light, a size at x, y (each pushed as a word). */
static int32_t STDCALL __attribute__((used)) hg2d_light(uint32_t rec, uint32_t size, uint32_t x,
                                                        uint32_t y)
{
    put(" light(");
    put_ptr(rec);
    put(",%#x,%#x,%#x)", size & 0xffff, x & 0xffff, y & 0xffff);
    return 0;
}

/* WEATHERM +0x28: a weather effect record of eight bytes from scenery. */
static int32_t STDCALL __attribute__((used)) weather_effect(const uint8_t *rec)
{
    uint32_t i;

    put(" weather(");
    put_ptr((uint32_t)(uintptr_t)rec);
    for (i = 0; i < 8; i++)
        put("%s%02x", i ? "" : ",", rec[i]);
    put(")");
    return 0;
}

/* +0x70: a colour (a word, with a word of flags) to its ramp, a dword
 * through `out`. */
static int32_t STDCALL __attribute__((used)) hg2d_ramp(uint32_t colour, uint32_t flags,
                                                       uint32_t *out)
{
    uint16_t c = (uint16_t)colour;

    put(" ramp(%#x,%#x,", c, flags & 0xffff);
    put_ptr((uint32_t)(uintptr_t)out);
    put(")");
    *out = ((uint32_t)(uint16_t)(c * 0x9e37u + 0x101) << 16) | (uint16_t)(c ^ 0x5a5a);
    return 0;
}

/* +0x88: a colour word to the one HG2D draws with, through `out`: the low
 * byte kept, the high byte through a palette table (0x2073263b).  Only a
 * word is written, and only the word of the argument read (retail pushes
 * it as one). */
static int32_t STDCALL __attribute__((used)) hg2d_colour(uint32_t colour, uint16_t *out)
{
    uint16_t c = (uint16_t)colour;

    put(" colour(%#x,", c);
    put_ptr((uint32_t)(uintptr_t)out);
    put(")");
    *out = (uint16_t)((c & 0xff) | (((c >> 8) * 7 + 3) & 0xff) << 8);
    return 0;
}

/* Every HG2D slot not modelled yet logs which one it was. */
#define HG2D_SLOTS 0x30
static uint32_t hg2d_slot_called;
#define HG2D_UNMODELLED(n) \
    static int32_t STDCALL hg2d_##n(void) { put(" HG2D+%#x?", n * 4); return 0; }
HG2D_UNMODELLED(0)

/* Every stub returns through a wrapper that leaves ecx and edx as constants
 * of the slot's own: G3D passes on what an import leaves in them, and what
 * a C stub leaves there depends on the harness, not on anything G3D does.
 * The wrapper re-pushes the stub's n arguments and calls it (stdcall). */
#define WRAP(name, fn, n, bytes, tag)                                        \
    void name(void);                                                         \
    __asm__(".text\n_" #name ":\n\t"                                         \
            ".rept " #n "\n\tpushl " #bytes "(%esp)\n\t.endr\n\t"            \
            "call _" #fn "@" #bytes "\n\t"                                   \
            "movl $0x5ec00000+" #tag ", %ecx\n\t"                            \
            "movl $0x5ed00000+" #tag ", %edx\n\t"                            \
            "ret $" #bytes "\n")

WRAP(w_alloc, h_alloc, 4, 16, 0x20c);
WRAP(w_realloc, h_realloc, 3, 12, 0x210);
WRAP(w_free, h_free, 1, 4, 0x214);
WRAP(w_list_add, h_list_add, 1, 4, 0x2f8);
WRAP(w_list_free, h_list_free, 1, 4, 0x2fc);
WRAP(w_list_remove, h_list_remove, 2, 8, 0x300);
WRAP(w_list_clear, h_list_clear, 1, 4, 0x304);
WRAP(w_chain_add, chain_add, 2, 8, 0x418);
WRAP(w_chain_remove, chain_remove, 2, 8, 0x41c);
WRAP(w_chain_send, chain_send, 3, 12, 0x420);
WRAP(w_unload, hg2d_unload, 2, 8, 0xdb0);
/* HG2D's line, polygon and vertex calls push and pop eax, ecx and edx
 * (0x20732f2e, 0x2073269d, 0x20732aab): their wrappers keep them too. */
#define WRAP_KEEP(name, fn, n, bytes)                                         \
    void name(void);                                                         \
    __asm__(".text\n_" #name ":\n\t"                                         \
            "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"                     \
            ".rept " #n "\n\tpushl " #bytes "+12(%esp)\n\t.endr\n\t"         \
            "call _" #fn "@" #bytes "\n\t"                                   \
            "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\t"                        \
            "ret $" #bytes "\n")

WRAP(w_point, hg2d_point, 4, 16, 0xd24);
WRAP_KEEP(w_line, hg2d_line, 5, 20);
WRAP_KEEP(w_polygon, hg2d_polygon, 1, 4);
WRAP_KEEP(w_vertex, hg2d_vertex, 4, 16);
WRAP(w_colour, hg2d_colour, 2, 8, 0xd88);
WRAP(w_shaded_vertex, hg2d_shaded_vertex, 5, 20, 0xd74);
WRAP(w_ramp, hg2d_ramp, 3, 12, 0xd70);
WRAP(w_textured_vertex, hg2d_textured_vertex, 6, 24, 0xd48);
WRAP(w_textured_polygon, hg2d_textured_polygon, 1, 4, 0xd4c);
WRAP(w_shaded_polygon, hg2d_shaded_polygon, 1, 4, 0xd78);
WRAP(w_st_vertex, hg2d_st_vertex, 7, 28, 0xd50);
WRAP(w_st_polygon, hg2d_st_polygon, 1, 4, 0xd54);
WRAP(w_weather_effect, weather_effect, 1, 4, 0x1b28);
WRAP(w_value_point, hg2d_value_point, 6, 24, 0xd28);
WRAP(w_light, hg2d_light, 4, 16, 0xd84);
WRAP(w_load_texture, hg2d_load_texture, 3, 12, 0xda8);

static void build_stubs(void)
{
    uint32_t i;

    for (i = 0; i < sizeof host; i += 4)
        SET_SLOT(host, i, h_unexpected0);
    SET_SLOT(host, 0x20c, w_alloc);
    SET_SLOT(host, 0x210, w_realloc);
    SET_SLOT(host, 0x214, w_free);
    SET_SLOT(host, 0x2f8, w_list_add);
    SET_SLOT(host, 0x2fc, w_list_free);
    SET_SLOT(host, 0x300, w_list_remove);
    SET_SLOT(host, 0x304, w_list_clear);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, w_chain_add);
    SET_SLOT(chain, 0x1c, w_chain_remove);
    SET_SLOT(chain, 0x20, w_chain_send);
    *(uint32_t *)hg2d = 13;
    for (i = 4; i < sizeof hg2d; i += 4)
        SET_SLOT(hg2d, i, hg2d_0);
    SET_SLOT(hg2d, 0xb0, w_unload);
    SET_SLOT(hg2d, 0x24, w_point);
    SET_SLOT(hg2d, 0x38, w_line);
    SET_SLOT(hg2d, 0x40, w_polygon);
    SET_SLOT(hg2d, 0x44, w_vertex);
    SET_SLOT(hg2d, 0x88, w_colour);
    SET_SLOT(hg2d, 0x74, w_shaded_vertex);
    SET_SLOT(hg2d, 0x70, w_ramp);
    SET_SLOT(hg2d, 0x48, w_textured_vertex);
    SET_SLOT(hg2d, 0x4c, w_textured_polygon);
    SET_SLOT(hg2d, 0x78, w_shaded_polygon);
    SET_SLOT(hg2d, 0x50, w_st_vertex);
    SET_SLOT(hg2d, 0x54, w_st_polygon);
    SET_SLOT(hg2d, 0x28, w_value_point);
    SET_SLOT(hg2d, 0x84, w_light);
    SET_SLOT(hg2d, 0xa8, w_load_texture);
    *(uint32_t *)weatherm = 27;
    SET_SLOT(weatherm, 0x28, w_weather_effect);
    (void)hg2d_slot_called;
}

/* Retail UTIL, shared by the sides: G3D asks it only for arithmetic
 * (+0x3c, atan2), which reads nothing but its own tables. */
static uint8_t *util_link;

static void load_util(void)
{
    HMODULE module = LoadLibraryA("UTIL_retail.dll");
    struct import *im;

    if (module == NULL) {
        printf("cannot load UTIL_retail.dll (%lu)\n", GetLastError());
        exit(2);
    }
    util_link = (uint8_t *)GetProcAddress(module, "Linkage");
    im = (struct import *)GetProcAddress(module, "ImportTable");
    if (util_link == NULL || *(uint32_t *)util_link != 20 || im == NULL) {
        printf("UTIL_retail.dll: wrong module\n");
        exit(2);
    }
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host; break;
        case 2:  im->link = global; break;
        case 17: im->link = numeric; break;
        default: printf("UTIL_retail.dll: unexpected import %u\n", im->id); exit(2);
        }
    }
}

/* Retail WINDOW, shared too: G3D asks it only for +0x2c, the matrix for
 * three angles, which reads UTIL and its own scratch.  Its other imports
 * get an empty table. */
static uint8_t *window_link;
static uint8_t empty_link[0x400];

static void load_window(void)
{
    HMODULE module = LoadLibraryA("WINDOW_retail.dll");
    struct import *im;

    if (module == NULL) {
        printf("cannot load WINDOW_retail.dll (%lu)\n", GetLastError());
        exit(2);
    }
    window_link = (uint8_t *)GetProcAddress(module, "Linkage");
    im = (struct import *)GetProcAddress(module, "ImportTable");
    if (window_link == NULL || *(uint32_t *)window_link != 10 || im == NULL) {
        printf("WINDOW_retail.dll: wrong module\n");
        exit(2);
    }
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host; break;
        case 2:  im->link = global; break;
        case 20: im->link = util_link; break;
        default: im->link = empty_link; break;
        }
    }
}

/* ---- the sides ------------------------------------------------------------------- */

static void load_side(struct side *s, const char *path, const char *name)
{
    struct import *im;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    uint32_t i;

    memset(s, 0, sizeof *s);
    s->name = name;
    s->transcript = malloc(TRANSCRIPT_SIZE);
    s->snap = malloc(sizeof(struct snapshot) * (MAX_OPS + 8));
    for (i = 0; i < MAX_OPS + 8; i++)
        s->snap[i].arena = malloc(ARENA_SIZE);
    s->module = LoadLibraryA(path);
    if (s->module == NULL) { printf("cannot load %s (%lu)\n", path, GetLastError()); exit(2); }
    s->imports = (struct import *)GetProcAddress(s->module, "ImportTable");
    s->link = (uint8_t *)GetProcAddress(s->module, "Linkage");
    if (s->imports == NULL || s->link == NULL || *(uint32_t *)s->link != 14) {
        printf("%s: not G3D\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if (s->link != s->image + LINKAGE) {
        printf("%s: Linkage is not ImportTable+0x58\n", path);
        exit(2);
    }
    dos = (IMAGE_DOS_HEADER *)s->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)s->module + dos->e_lfanew);
    s->lo = (uint8_t *)s->module;
    s->hi = s->lo + nt->OptionalHeader.SizeOfImage;
    for (im = s->imports; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host; break;
        case 2:  im->link = global; break;
        case 4:  im->link = chain; break;
        case 10: im->link = window_link; break;
        case 13: im->link = hg2d; break;
        case 17: im->link = numeric; break;
        case 18: im->link = fe; break;
        case 20: im->link = util_link; break;
        case 24: im->link = atc; break;
        case 27: im->link = weatherm; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* ---- state ------------------------------------------------------------------------ */

static void snapshot(void)
{
    struct snapshot *t;

    if (cur->nsnap >= MAX_OPS + 8)
        return;
    t = &cur->snap[cur->nsnap++];
    memcpy(t->image, cur->image, IMAGE_SIZE);
    memcpy(t->global, global, GLOBAL_SIZE);
    t->arena_used = arena_used;
    memcpy(t->arena, arena, arena_used);
    t->programs_hash = fnv(programs, PROGRAM_SIZE);
    t->context_hash = fnv(context, CONTEXT_SIZE);
}

/* The first offset at which a and b differ, a dword whose two values point
 * at the same offset in their own sides' images counting as equal; n if
 * none. */
static uint32_t pair_differs(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    uint32_t i = 0, j;

    while (i < n) {
        if (a[i] == b[i]) {
            i++;
            continue;
        }
        for (j = i >= 3 ? i - 3 : 0; j <= i; j++) {
            uint32_t va, vb;

            if (j + 4 > n)
                break;
            memcpy(&va, a + j, 4);
            memcpy(&vb, b + j, 4);
            if (as_offset(&retail, va) != va && as_offset(&ours, vb) != vb &&
                as_offset(&retail, va) == as_offset(&ours, vb))
                break;
        }
        if (j <= i && j + 4 <= n) {
            i = j + 4;
            continue;
        }
        return i;
    }
    return n;
}

/* The snapshots compared; prints the first difference.  Returns 0 when
 * they agree. */
static int compare_snapshots(int show)
{
    uint32_t k, at;
    char what[96];

    if (retail.nsnap != ours.nsnap) {
        if (show)
            printf("scenario %u: %u snapshots against %u\n", scenario, retail.nsnap, ours.nsnap);
        return 1;
    }
    for (k = 0; k < retail.nsnap; k++) {
        struct snapshot *a = &retail.snap[k], *b = &ours.snap[k];
        uint8_t ia[IMAGE_SIZE], ib[IMAGE_SIZE];

        memcpy(ia, a->image, IMAGE_SIZE);
        memcpy(ib, b->image, IMAGE_SIZE);
        memset(ia + LINKAGE + 4, 0, LINKAGE_END - LINKAGE - 4);
        memset(ib + LINKAGE + 4, 0, LINKAGE_END - LINKAGE - 4);
        what[0] = 0;
        if ((at = pair_differs(ia, ib, IMAGE_SIZE)) < IMAGE_SIZE)
            snprintf(what, sizeof what, "image +%#x: retail %02x ours %02x", at, ia[at], ib[at]);
        else if ((at = pair_differs(a->global, b->global, GLOBAL_SIZE)) < GLOBAL_SIZE)
            snprintf(what, sizeof what, "GLOBAL +%#x: retail %02x ours %02x", at,
                     a->global[at], b->global[at]);
        else if (a->arena_used != b->arena_used)
            snprintf(what, sizeof what, "arena used %#x against %#x", a->arena_used, b->arena_used);
        else if ((at = pair_differs(a->arena, b->arena, a->arena_used)) < a->arena_used)
            snprintf(what, sizeof what, "arena +%#x: retail %02x ours %02x", at,
                     a->arena[at], b->arena[at]);
        else if (a->programs_hash != b->programs_hash)
            snprintf(what, sizeof what, "programs");
        else if (a->context_hash != b->context_hash)
            snprintf(what, sizeof what, "the context buffer");
        if (what[0]) {
            if (show)
                printf("scenario %u: after op %u, %s\n", scenario, k, what);
            return 1;
        }
    }
    return 0;
}

static void put_regs(void)
{
    /* Raw and tagged: matching_length() pairs them across the sides. */
    put(" -> eax R%08x ebx R%08x ecx R%08x edx R%08x esi R%08x edi R%08x ebp R%08x pop %d",
        regs_out[0], regs_out[1], regs_out[2], regs_out[3], regs_out[4], regs_out[5],
        regs_out[6], (int)(esp_after - (saved_esp - 4 * call_nargs)));
}

static void call_service(uint32_t slot, uint32_t nargs, ...)
{
    va_list ap;
    uint32_t i;

    va_start(ap, nargs);
    for (i = 0; i < nargs; i++)
        call_args[i] = va_arg(ap, uint32_t);
    va_end(ap);
    call_nargs = nargs;
    call_target = *(uint32_t *)(cur->link + slot);
    /* Never inside either image: a register that points into one after the
     * call was put there by G3D, and compares as an offset. */
    for (i = 0; i < 7; i++)
        do
            regs_in[i] = rnd();
        while (as_offset(&retail, regs_in[i]) != regs_in[i] ||
               as_offset(&ours, regs_in[i]) != regs_in[i]);
    in_g3d = 1;
    if (__builtin_setjmp(fault_jb) == 0) {
        call_regs();
        put_regs();
    }
    in_g3d = 0;
}

/* ---- programs ------------------------------------------------------------------------- */

/* The variable numbers the table maps to GLOBAL (nonzero on disk) and a few
 * it does not; chosen once from retail's pristine image.  A few mapped ones
 * whose GLOBAL fields G3D does not touch are reserved for 0x5b's program
 * addresses, which nothing else may write. */
static uint16_t mapped_vars[VAR_ENTRIES], unmapped_vars[64], reserved_vars[16];
static uint32_t nmapped, nunmapped, nreserved;

static void find_variables(void)
{
    uint32_t i, v;

    for (i = 0; i < VAR_ENTRIES; i++) {
        memcpy(&v, retail.pristine + VAR_TABLE + 4 * i, 4);
        if (v != 0 && v < GLOBAL_SIZE - 8) {
            /* nothing else may map into the reserved fields: two numbers
             * can name the same field */
            if (v >= 0x400 && v < 0x5b0) {
                uint32_t k, field;

                /* one name per field: two reserved numbers on one field
                 * would share its address */
                for (k = 0; k < nreserved; k++) {
                    memcpy(&field, retail.pristine + VAR_TABLE + 4 * reserved_vars[k], 4);
                    if (field + 4 > v && v + 4 > field)
                        break;
                }
                if ((v & 3) == 0 && nreserved < 16 && k == nreserved)
                    reserved_vars[nreserved++] = (uint16_t)i;
            } else if (!(v + 2 > 0x1194 && v < 0x119c) && !(v + 2 > 0xdca && v < 0xdcc)) {
                /* not the window and view pointers the drawing reads, nor
                 * the map flag (lines depend on it, below) */
                mapped_vars[nmapped++] = (uint16_t)i;
            }
        } else if (v == 0 && nunmapped < 64 && i % 37 == 1) {
            unmapped_vars[nunmapped++] = (uint16_t)i;
        }
    }
    if (nmapped == 0 || nunmapped == 0 || nreserved < 4) {
        printf("variable table: %u mapped, %u unmapped, %u reserved\n",
               nmapped, nunmapped, nreserved);
        exit(2);
    }
}

struct gen {
    uint8_t *p;              /* where the next instruction goes */
    uint8_t *end;
    uint32_t depth;          /* nesting of calls */
    int context;             /* a context is set on this path */
};

static void w16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v) { w16(p, v); w16(p + 2, v >> 16); }

/* A variable operand: through the table, mapped or not.  Whether a
 * context is set depends on the path taken (a 0x89 in a skipped block never
 * runs), so with one possibly set the number is always one the table maps:
 * valid either way, and with the context an offset into its buffer. */
static uint16_t variable(const struct gen *g)
{
    if (g->context && rnd() % 2) {
        if (counting) cover_context++;
        return mapped_vars[rnd() % nmapped];
    }
    if (rnd() % 6 == 0 && nunmapped) {
        if (counting) cover_unmapped++;
        return unmapped_vars[rnd() % nunmapped];
    }
    if (counting) cover_mapped++;
    return mapped_vars[rnd() % nmapped];
}

/* Put `value` where variable v will be read: its GLOBAL field (or the
 * scratch word an unmapped one goes to) and, as a context may be set on
 * the path taken, the context buffer too.  Instructions run before it may
 * change it again; that is fine. */
static void steer(uint16_t v, uint16_t value)
{
    uint32_t field;

    memcpy(&field, retail.pristine + VAR_TABLE + 4 * v, 4);
    if (field != 0)
        w16(global + field, value);
    else
        w16(cur->image + 0xf7e0, value);
    w16(context + CONTEXT_SIZE / 2 + v, value);
    if (counting)
        cover_steered++;
}

static uint16_t small_word(void)
{
    switch (rnd() % 4) {
    case 0: return 0;
    case 1: return (uint16_t)(rnd() % 4);
    case 2: return (uint16_t)rnd();
    default: return (uint16_t)(0x8000 - rnd() % 4);
    }
}

static void emit_block(struct gen *g, uint32_t n);
/* Texture names as FS98 writes them, 8.3 in 13 bytes; 0x18 lower-cases
 * its own (CharLowerA), so some come upper. */
static const char *const texture_names[] = {
    "water   .r8", "runway  .r8", "side    .r8", "red     .r8", "side2   .r8",
    "grass   .r8", "GRASS   .R8", "Runway  .R8",
};
#define NTEXTURE_NAMES (sizeof texture_names / sizeof texture_names[0])

static int scene_is_map;           /* no lines: GLOBAL +0xdca set and the line
                                    * colour (+0x2ec) not 0x6900, or maybe not */
static int scene_dca;              /* GLOBAL +0xdca set for this program */
static int scene_plain;            /* identity matrix, x centre 0x140, +0xd42 small */
static int scene_steady;           /* identity matrix, the eye where set_scene put it */

/* A point's three words: mostly ahead of the eye (z positive), x and y
 * within the view's cone or a little past it, so some are clipped. */
static void emit_point(uint8_t *p)
{
    int32_t z = rnd() % 8 ? (int32_t)(0x10 + rnd() % 0x400) : (int16_t)rnd();
    int32_t span = z < 0 ? -z : z;
    int32_t x = rnd() % 6 ? (int32_t)(rnd() % (2 * span + 1)) - span : (int16_t)rnd();
    int32_t y = rnd() % 6 ? (int32_t)(rnd() % (2 * span + 1)) - span : (int16_t)rnd();

    if (rnd() % 4 == 0) x = x * 3 / 2;
    if (rnd() % 4 == 0) y = y * 3 / 2;
    w16(p, (uint16_t)x);
    w16(p + 2, (uint16_t)y);
    w16(p + 4, (uint16_t)z);
}

/* A normal: three 1.15 words, a unit vector more or less, or anything. */
static void emit_normal(uint8_t *p)
{
    uint32_t i, axis = rnd() % 3;

    for (i = 0; i < 3; i++)
        w16(p + 2 * i, rnd() % 4 == 0 ? (uint16_t)rnd() :
                       i == axis ? (rnd() % 2 ? 0x7fff : 0x8001) : (uint16_t)(rnd() % 0x2000 - 0x1000));
}

/* A plane's distance, around what the eye's dot product gives; a quarter
 * of the time outside the map in a steady scene, exactly at it give or
 * take one (0x206e7c4c: the high dword of normal << 16 . eye, less the
 * distance -- shifted down by the scale word 0x206f9159 when it is set). */
static uint32_t plane_distance(const uint8_t *normal)
{
    if (scene_steady && !scene_dca && rnd() % 4 == 0) {
        int64_t sum = 0;
        int32_t eye, hi, d = (int32_t)(rnd() % 3) - 1;
        uint16_t scale;
        uint32_t i;

        for (i = 0; i < 3; i++) {
            int16_t w;

            memcpy(&w, normal + 2 * i, 2);
            memcpy(&eye, global + 0xcdc + 4 * i, 4);
            sum += (int64_t)(int32_t)((uint32_t)(uint16_t)w << 16) * eye;
        }
        hi = (int32_t)((uint64_t)sum >> 32);
        memcpy(&scale, cur->image + 0x159, 2);
        if (scale == 0)
            return (uint32_t)(hi + d);
        scale &= 31;
        return (uint32_t)(((uint32_t)hi << scale) + (uint32_t)(d << scale) +
                          (rnd() % 2 ? 0 : (1u << scale) - 1));
    }
    return rnd() % 4 ? (uint32_t)((int32_t)(rnd() % 0x20000) - 0x10000) : spread();
}

/* In the map a face's side is the sun's dot product with its normal
 * (0x206e740b), negated in 32 bits (0x206e7ce5): its sign turns on a borrow
 * only when the sum is in 1..0xffff.  A quarter of the time there, the
 * normal made at right angles to the sun but for its z, set to put the sum
 * at the sun's z word. */
static void steer_map_normal(uint8_t *normal)
{
    int16_t sun[3];
    uint16_t on;
    uint32_t i;

    if (!scene_dca || rnd() % 4)
        return;
    memcpy(&on, cur->image + 0x1bd, 2);
    for (i = 0; i < 3; i++)
        memcpy(&sun[i], on ? cur->image + 0x4901 + 4 * i : global + 0xb2a + 4 * i, 2);
    if (sun[2] == 0)
        return;
    w16(normal, (uint16_t)sun[1]);
    w16(normal + 2, (uint16_t)-sun[0]);
    w16(normal + 4, sun[2] > 0 ? 1 : 0xffff);
}

/* Points 0x30..0x33 are 3:4 deep for the fast routines' ratio and 0x38..0x3f
 * are 0x27's (scenario_run seeds them all), kept from the
 * rest. */
static uint16_t point_index(void)
{
    uint16_t k = (uint16_t)(rnd() % 8 ? rnd() % 0x20 : rnd() % 0x3e8);

    return k >= 0x30 && k < 0x40 ? k + 0x10 : k;
}

/* A position packed as three (word, dword) 48-bit values -- latitude,
 * longitude, altitude, as GLOBAL +0x5b2, +0x5ba, +0x5c2 hold the eye's --
 * mostly the eye's own with the integer parts a little off, so objects
 * fall in, across and out of view; now and then anything. */
static void packed_position(uint8_t *p)
{
    static const uint32_t eye[3] = { 0x5b2, 0x5ba, 0x5c2 };
    uint32_t i, d;

    for (i = 0; i < 3; i++) {
        if (rnd() % 4 == 0) {
            w16(p + 6 * i, (uint16_t)rnd());
            w32(p + 6 * i + 2, rnd());
            continue;
        }
        memcpy(p + 6 * i, global + eye[i], 6);
        memcpy(&d, p + 6 * i + 2, 4);
        w32(p + 6 * i + 2, d + (uint32_t)((int32_t)(rnd() % 0x81) - 0x40));
    }
}

/* An object instruction (0x2f, 0x77, 0x4c, 0x83, 0x3a, 0x3c) at p, its
 * target left for the caller; its length.  Range and radius 0 for none, the
 * one-shot word 0 (the queue fills it when it draws), a scale, and a
 * packed position or a variable holding a record. */
static uint32_t emit_object(struct gen *g, uint8_t *p, uint32_t op)
{
    uint32_t len = op == 0x83 || op == 0x3a ? 0xc : op == 0x4c ? 0x10 :
                   op == 0x3c ? 0x1c : 0x20;
    uint32_t scale = rnd() % 2 ? 1u << (rnd() % 24) : rnd();

    w16(p, op);
    w16(p + 4, rnd() % 3 ? 0 : (uint16_t)rnd());
    w16(p + 6, rnd() % 3 ? 0 : rnd() % 2 ? (uint16_t)(rnd() % 0x100) : (uint16_t)rnd());
    if (op == 0x83) {
        w32(p + 8, scale);
    } else {
        w16(p + 8, 0);
        if (op == 0x3a) {
            w16(p + 0xa, variable(g));
        } else if (op == 0x3c) {
            packed_position(p + 0xa);
        } else {
            w32(p + 0xa, scale);
            if (op == 0x4c)
                w16(p + 0xe, variable(g));
            else
                packed_position(p + 0xe);
        }
    }
    return len;
}

/* A point's word on one axis as the sphere test sees it, the integer word
 * of its eye-space coordinate, worked as point_from_words() works it in a
 * steady scene: the word << 16 shifted by .data+0x159, less the eye,
 * halved, times 0x7fff >> 15, z shifted by GLOBAL +0xd42.  0x7fffffff
 * when the difference overflows (another path). */
static int32_t eye_word(uint16_t w, int axis)
{
    int32_t eye, a, rel, v;
    int64_t d;
    uint16_t d42;

    memcpy(&eye, global + 0xcdc + 4 * axis, 4);
    a = (int32_t)((uint32_t)w << 16) >> (cur->image[0x159] & 31);
    d = (int64_t)a - eye;
    if (d != (int32_t)d)
        return 0x7fffffff;
    rel = (int32_t)d >> 1;
    v = (int32_t)(((int64_t)rel * 0x7fff) >> 15);
    if (axis == 2) {
        memcpy(&d42, global + 0xd42, 2);
        v >>= d42 & 31;
    }
    return (int16_t)((uint32_t)v >> 16);
}

/* a word whose eye_word() on the axis is t, or 0x7fffffff */
static int32_t word_for(int32_t t, int axis)
{
    int32_t eye, i;

    memcpy(&eye, global + 0xcdc + 4 * axis, 4);
    for (i = -6; i <= 6; i++) {
        int32_t w = 2 * t + (eye >> 16) + i;

        if (w >= -0x8000 && w <= 0x7fff && eye_word((uint16_t)w, axis) == t)
            return w;
    }
    return 0x7fffffff;
}

/* 0x2c's point and radius (at p) put on a boundary of the sphere test --
 * x - z, z, y + z or x + z at the radius times sqrt 2 (r', as the test
 * works it) give or take one -- where its sqrt 2 and its comparisons
 * show.  0 when no word lands there. */
static int16_t radius_root2(uint16_t r, uint32_t k)
{
    uint32_t q = (uint32_t)r * k;

    return (int16_t)(((q >> 16) * 2 + ((q & 0xffff) >> 15)) & 0xffff);
}

static int steer_sphere(uint8_t *p)
{
    uint16_t r = (uint16_t)(1 + rnd() % 0x100);
    int16_t rp;
    int32_t d = (int32_t)(rnd() % 3) - 1, z, x = 0, y = 0, wx, wy, wz;
    uint32_t k = rnd() % 4, n;

    /* half the time a radius where sqrt 2's last bit shows */
    if (rnd() % 2)
        for (n = 0; n < 16; n++) {
            r = (uint16_t)(0x1000 + rnd() % 0x1a00);
            if (radius_root2(r, 0xb504) != radius_root2(r, 0xb505))
                break;
        }
    rp = radius_root2(r, 0xb504);
    /* the first test is z against the radius itself, the rest against r' */
    z = k == 1 ? -(int32_t)r + d : 0x40 + (int32_t)(rnd() % 0x1c0);
    /* y's tests come only after x's are passed, which wants z past r' */
    if (k == 2)
        z = rp + 0x40 + (int32_t)(rnd() % 0x100);
    if (k == 0)
        x = z + rp + d;
    else if (k == 2)
        y = -rp - z + d;
    else if (k == 3)
        x = -rp - z + d;
    wx = word_for(x, 0);
    wy = word_for(y, 1);
    wz = word_for(z, 2);
    if (wx == 0x7fffffff || wy == 0x7fffffff || wz == 0x7fffffff)
        return 0;
    w16(p, (uint16_t)wx);
    w16(p + 2, (uint16_t)wy);
    w16(p + 4, (uint16_t)wz);
    w16(p + 6, r);
    if (getenv("G3D_STEERLOG") && cur == &retail)
        fprintf(stderr, "steer %u: x %d y %d z %d r' %d\n", k, x, y, z, rp);
    return 1;
}

/* A drawing: a polygon (surface, start, continues, close), a line, or a
 * table point, a coloured one, a point, a reject.  The bytes after p,
 * returning the length used. */
static uint32_t emit_drawing(struct gen *g, uint8_t *p)
{
    uint32_t k, n, len = 0;

    uint32_t pick = rnd() % 18;

    /* in a steady scene a third of them rejects, steered onto the sphere's
     * boundaries */
    if (scene_steady && rnd() % 3 == 0)
        pick = 7;
    switch (pick) {
    case 17: {                                  /* a facet that keeps its plane */
        uint32_t i;

        /* At least two vertices, as 0x2a.  Its loop counts in all of ecx,
         * whose high half is what a vertex leaves there: the vertex's y,
         * which at 0x10000 or more, or below 0, walks on past the count
         * through words that are not indices to points outside the table
         * (docs/modules/g3d.md).  Its points are 0x38..0x3f, whose y stays
         * in 0..0xffff. */
        n = 2 + rnd() % 5;
        w16(p, 0x27);
        w16(p + 2, n);
        emit_normal(p + 4);
        w32(p + 10, plane_distance(p + 4));
        for (i = 0; i < n; i++)
            w16(p + 14 + 2 * i, 0x38 + rnd() % 8);
        len = 14 + 2 * n;
        if (counting) cover_emitted[0x27]++;
        break;
    }
    case 15: case 16: {                         /* a textured facet or face */
        uint32_t i, at, deep;

        n = 2 + rnd() % 5;
        if (rnd() % 2) {
            /* 0x7a, shaded, has 0x20's shape */
            w16(p, rnd() % 2 ? 0x20 : 0x7a);
            w16(p + 2, n);
            emit_normal(p + 4);
            w32(p + 10, plane_distance(p + 4));
            at = 14;
        } else {
            /* not three: retail's 0x60 triangle fast path leaves the stack
             * unbalanced and crashes (docs/modules/g3d.md); ours is fixed */
            /* 0x7b, shaded, has 0x60's shape; not four: retail's 0x7b quad
             * fast path pushes its depth as a dword and crashes */
            uint16_t op = rnd() % 2 ? 0x60 : 0x7b;

            if (op == 0x60 && n == 3)
                n = 4;
            if (op == 0x7b && n == 4)
                n = 3;
            w16(p, op);
            w16(p + 2, n);
            emit_point(p + 4);
            emit_normal(p + 10);
            steer_map_normal(p + 10);
            at = 16;
        }
        deep = rnd() % 4 == 0;
        for (i = 0; i < n; i++) {
            /* a next vertex's index is times 16 in a word: from 0x1000 it
             * wraps (the unwrapped place, past 0x1031e, holds no pointers) */
            w16(p + at + 6 * i, i > 0 && rnd() % 16 == 0 ? (uint16_t)(0x1000 + rnd() % 0x20)
                                                         : point_index());
            /* a quarter of the time from the 3:4 points, both depths in */
            if (deep)
                w16(p + at + 6 * i, (uint16_t)(0x30 + (i == 0 ? 0 : i == 1 ? 2 : rnd() % 4)));
            w16(p + at + 6 * i + 2, (uint16_t)rnd());
            w16(p + at + 6 * i + 4, (uint16_t)rnd());
        }
        len = at + 6 * n;
        if (counting) cover_emitted[p[0]]++;
        break;
    }
    case 13:                                    /* a colour ramp */
        k = rnd() % 3;
        w16(p, k == 0 ? 0x50 : k == 1 ? 0x53 : 0x66);
        w16(p + 2, k == 2 ? variable(g) : (uint16_t)rnd());
        len = 4;
        if (counting) cover_emitted[p[0]]++;
        break;
    case 14: {                                  /* a shaded facet */
        uint32_t i;

        /* at least two vertices: its word count walks 65,536 for one */
        n = 2 + rnd() % 5;
        w16(p, 0x2a);
        w16(p + 2, n);
        emit_normal(p + 4);
        w32(p + 10, plane_distance(p + 4));
        for (i = 0; i < n; i++)
            w16(p + 14 + 2 * i, point_index());
        len = 14 + 2 * n;
        if (counting) cover_emitted[0x2a]++;
        break;
    }
    case 11: case 12:                           /* a shaded polygon */
        n = 1 + rnd() % 6;
        for (k = 0; k <= n; k++) {
            if (rnd() % 4 == 0) {
                w16(p + len, k == 0 ? 0x12 : 0x13);
                w16(p + len + 2, point_index());
                len += 4;
            } else {
                w16(p + len, k == 0 ? 0x0a : 0x0b);
                w16(p + len + 2, (uint16_t)rnd());
                emit_point(p + len + 4);
                len += 10;
            }
        }
        if (rnd() % 8)
            w16(p + len, 0x0c), len += 2;
        if (counting) cover_emitted[0x0a]++;
        break;
    case 8: case 9: case 10: {                  /* a face, a facet, a separation plane */
        uint32_t kind = rnd() % 3, i;

        /* at least two vertices: retail's dec-then-loop walks four billion
         * for one */
        n = 2 + rnd() % 5;
        w16(p, kind == 0 ? 0x1d : kind == 1 ? 0x3e : 0x25);
        if (kind == 0) {
            w16(p + 2, n);
            emit_point(p + 4);
            emit_normal(p + 10);
            steer_map_normal(p + 10);
            for (i = 0; i < n; i++)
                w16(p + 16 + 2 * i, point_index());
            len = 16 + 2 * n;
        } else if (kind == 1) {
            w16(p + 2, n);
            emit_normal(p + 4);
            w32(p + 10, plane_distance(p + 4));
            for (i = 0; i < n; i++)
                w16(p + 14 + 2 * i, point_index());
            len = 14 + 2 * n;
        } else {
            /* behind skips the 0x09 after it */
            w16(p + 2, 16);
            emit_normal(p + 4);
            w32(p + 10, plane_distance(p + 4));
            w16(p + 14, 0x09);
            len = 16;
        }
        if (counting) cover_emitted[p[0]]++;
        break;
    }
    case 0: case 1: case 2:                     /* a polygon */
        w16(p, 0x05);
        len = 2;
        n = 1 + rnd() % 6;
        for (k = 0; k <= n; k++) {
            if (rnd() % 4 == 0 && k > 0) {
                w16(p + len, 0x10);             /* continue at a table point */
                w16(p + len + 2, point_index());
                len += 4;
            } else if (rnd() % 6 == 0 && k == 0) {
                w16(p + len, 0x0f);
                w16(p + len + 2, point_index());
                len += 4;
            } else {
                w16(p + len, k == 0 ? 0x06 : 0x07);
                emit_point(p + len + 2);
                len += 8;
            }
        }
        if (rnd() % 8)
            w16(p + len, 0x08), len += 2;
        if (counting) cover_emitted[0x05]++;
        break;
    case 3:                                     /* a line */
        /* In the map (GLOBAL +0xdca) with a line colour other than 0x6900,
         * 0x07 outside a surface returns with the counter one byte on, and
         * the program goes on from inside the instruction: no lines there. */
        if (scene_is_map) {
            w16(p, 0x09);
            len = 2;
            break;
        }
        w16(p, rnd() % 4 ? 0x06 : 0x0f);
        if (p[0] == 0x06)
            emit_point(p + 2), len = 8;
        else
            w16(p + 2, point_index()), len = 4;
        n = 1 + rnd() % 4;
        for (k = 0; k < n; k++) {
            if (rnd() % 3 == 0) {
                w16(p + len, 0x10);
                w16(p + len + 2, point_index());
                len += 4;
            } else {
                w16(p + len, 0x07);
                emit_point(p + len + 2);
                len += 8;
            }
        }
        if (counting) cover_emitted[0x07]++;
        break;
    case 4:                                     /* points into the table */
        if (rnd() % 2) {
            w16(p, 0x0e);
            w16(p + 2, point_index());
            emit_point(p + 4);
            len = 10;
        } else {
            w16(p, 0x11);
            w16(p + 2, point_index());
            w16(p + 4, (uint16_t)rnd());
            emit_point(p + 6);
            len = 12;
        }
        if (counting) cover_emitted[p[0]]++;
        break;
    case 5:
        w16(p, 0x37);
        emit_point(p + 2);
        len = 8;
        if (counting) cover_emitted[0x37]++;
        break;
    case 6:
        w16(p, 0x80);
        w16(p + 2, point_index());
        len = 4;
        if (counting) cover_emitted[0x80]++;
        break;
    default:                                    /* reject, forward */
        /* out of view skips the instruction after it: a 0x09, which counts
         * in GLOBAL +0x2f4 (often reset before anything sees it), or a
         * 0x87, whose operand stays in GLOBAL +0xcbc.  In a steady scene,
         * on one of the sphere's boundaries */
        w16(p, 0x2c);
        if (!(scene_steady && steer_sphere(p + 4))) {
            emit_point(p + 4);
            w16(p + 10, (uint16_t)(rnd() % 4 ? rnd() % 0x200 : rnd()));
        }
        if (rnd() % 2) {
            w16(p + 2, 14);
            w16(p + 12, 0x09);
            len = 14;
        } else {
            w16(p + 2, 16);
            w16(p + 12, 0x87);
            w16(p + 14, (uint16_t)rnd());
            len = 16;
        }
        if (counting) cover_emitted[0x2c]++;
        break;
    }
    (void)g;
    return len;
}

/* The scene a program draws into: the window record and view block GLOBAL
 * points at, an eye near the origin and the identity matrix, mostly; now
 * and then anything. */
static void set_scene(void)
{
    uint32_t i;
    uint16_t m[9] = { 0x7fff, 0, 0, 0, 0x7fff, 0, 0, 0, 0x7fff };

    w32(global + 0x1194, (uint32_t)(uintptr_t)window_rec);
    w32(global + 0x1198, (uint32_t)(uintptr_t)view_block);
    w32(view_block + 0, rnd() % 8 ? 0x80 + rnd() % 0x200 : spread());
    w32(view_block + 4, rnd() % 8 ? 0x140 : spread());
    w32(view_block + 8, rnd() % 8 ? 0x80 + rnd() % 0x200 : spread());
    w32(view_block + 0xc, rnd() % 8 ? 0xf0 : spread());
    /* 0x56's level of detail: +0x10 1 keeps it, else from +0x14 */
    w32(view_block + 0x10, rnd() % 4 ? 0 : rnd() % 2 ? 1 : rnd());
    w32(view_block + 0x14, rnd() % 4 ? rnd() % 8 : rnd());
    /* 0x55 wants +0x14 and the word +0x56 alike, 0 or 1 */
    w16(view_block + 0x56, rnd() % 3 ? (uint16_t)(view_block[0x14] & 1) : (uint16_t)rnd());
    for (i = 0; i < 3; i++)
        w32(global + 0xcdc + 4 * i, rnd() % 4 ? (uint32_t)((int32_t)(rnd() % 0x40000) - 0x20000)
                                              : spread());
    if (rnd() % 4 == 0)
        for (i = 0; i < 9; i++)
            m[i] = (uint16_t)rnd();
    for (i = 0; i < 9; i++)
        w16(global + 0xce8 + 2 * i, m[i]);
    w16(global + 0xd42, rnd() % 4 ? rnd() % 3 : (uint16_t)rnd());
    w32(global + 0x320, rnd() % 8 ? 1 + rnd() % 0x10000 : (rnd() % 2 ? 0 : spread()));
    w16(global + 0xdca, rnd() % 6 ? 0 : 1);
    scene_dca = global[0xdca] != 0;
    {
        uint16_t d42;

        memcpy(&d42, global + 0xd42, 2);
        scene_steady = m[0] == 0x7fff && m[1] == 0 && m[2] == 0 && m[3] == 0 && m[4] == 0x7fff &&
                       m[5] == 0 && m[6] == 0 && m[7] == 0 && m[8] == 0x7fff;
        scene_plain = m[0] == 0x7fff && m[1] == 0 && m[4] == 0x7fff && m[8] == 0x7fff &&
                      view_block[4] == 0x40 && view_block[5] == 1 && view_block[6] == 0 &&
                      view_block[7] == 0 && d42 <= 2;
    }
    w32(global + 0x2ec, rnd() % 4 ? 0x6900 : rnd());
    w16(global + 0x1286, rnd() % 3 ? 0 : (uint16_t)rnd());
    /* bit 3 of GLOBAL +0x12a4 keeps textured triangles and quads off the
     * fast path; with 0x206f90f4 set (0x7d) they take it */
    if (rnd() % 2)
        global[0x12a4] |= 8;
    else
        global[0x12a4] &= ~8;
    if (rnd() % 2)
        w16(cur->image + 0xf4, 1);
    /* the distance (strips draw near by it, lines far) */
    w32(global + 0xd4c, rnd() % 2 ? rnd() % 0x400000 : rnd());
    /* in the map, facets and meshes close only with +0x1298 clear */
    w32(global + 0x1298, rnd() % 2 ? 0 : rnd());
    /* 0x87 steps +0xcbe from 1 to 2 */
    w16(global + 0xcbe, rnd() % 4 ? (uint16_t)(rnd() % 3) : (uint16_t)rnd());
    /* 0x41 runs its shadow with +0xe8a above 0xfa */
    w16(global + 0xe8a, rnd() % 2 ? (uint16_t)(0xf8 + rnd() % 5) : (uint16_t)rnd());
    /* 0x4a runs its shadow with +0x390 at 2 or more, +0x28c set and
     * +0x116e's low bits clear */
    w32(global + 0x390, rnd() % 4);
    w32(global + 0x28c, rnd() % 4 ? rnd() : 0);
    w16(global + 0x116e, rnd() % 2 ? 0 : (uint16_t)rnd());
    /* 0x3f runs its subroutine only with both set */
    w16(global + 0x11b2, rnd() % 4 ? 1 : 0);
    w16(global + 0xb34, rnd() % 4 ? (uint16_t)rnd() : 0);
    w16(global + 0xc96, (uint16_t)rnd());       /* texture offsets */
    w16(global + 0xc9e, (uint16_t)rnd());
    for (i = 0; i < 3; i++)                     /* the sun's direction */
        w16(global + 0xb2a + 4 * i, (uint16_t)rnd());
    cur->image[0x159] = 0;
    cur->image[0x15a] = 0;
    if (rnd() % 8 == 0)
        cur->image[0x159] = (uint8_t)(rnd() % 4);
    /* In the map a line is drawn only with the line colour 0x6900; a point
     * drawn with GLOBAL +0x1286 set may change the colour */
    {
        uint32_t colour;
        uint16_t highlight;

        memcpy(&colour, global + 0x2ec, 4);
        memcpy(&highlight, global + 0x1286, 2);
        scene_is_map = scene_dca && !(colour == 0x6900 && highlight == 0);
    }
}

/* A subroutine in a 0x100-byte slot of its own after the main program,
 * ending in return (or end), returned as its address. */
/* The models +0x38 and the queueing opcodes queue: a slot of 0x100 bytes
 * each at programs+0x10000, made at the start of a scenario and kept
 * through it (programs are remade over the rest), as the queue keeps
 * pointers to them until it draws them. */
#define NMODELS 16
#define models (programs + 0x10000)

static void seed_textures(void);
static uint8_t *sub_next;
static uint32_t reserved_next;
static uint32_t draw_weight;         /* in ten, how often emit_one draws */
static int context_possible;

static uint8_t *subroutine(struct gen *g)
{
    struct gen s;
    uint8_t *at;

    if (sub_next + 0x100 > programs + 0x10000)
        return NULL;
    at = s.p = sub_next;
    sub_next += 0x100;
    s.end = s.p + 0xc0;
    s.depth = g->depth + 1;
    s.context = g->context;
    emit_block(&s, 1 + rnd() % 6);
    w16(s.p, rnd() % 3 ? 0x22 : (rnd() % 2 ? 0x7c : 0x00));
    return at;
}

/* One instruction at g->p; forward branches land on a later instruction
 * boundary, which emit_block arranges by emitting the skipped part first. */
static void emit_one(struct gen *g)
{
    static const uint8_t simple[] = {
        0x02, 0x43, 0x64, 0x85, 0x04, 0x14, 0x17, 0x1f, 0x47, 0x51, 0x52, 0x54,
        0x58, 0x5e, 0x61, 0x71, 0x76, 0x7d, 0x81, 0x86, 0x87, 0x88, 0x8d, 0x8f,
        0x92, 0x93, 0x26, 0x7e, 0x4d, 0x4e, 0x4f, 0x65, 0x67, 0x8e, 0x89, 0x48,
        0x2d, 0x2e, 0x84, 0x68, 0x1e, 0x56, 0x5f, 0x6d, 0x55, 0x70,
        0x57, 0x30, 0x62, 0x69, 0x6b, 0x6e, 0x35, 0x45, 0x31, 0x36,
        0x91, 0x73, 0x1b, 0x40, 0x82, 0x34, 0x2f, 0x77, 0x4c, 0x83, 0x3a, 0x3c,
        0x32, 0x2b, 0x75, 0x5a, 0x1a, 0x29, 0x90, 0x28, 0x38, 0x6a, 0x18, 0x5d, 0x15,
    };
    uint8_t *p = g->p, *q;
    uint32_t op, n, i;

    if (g->end - p < 0x60) {
        w16(p, 0x02);
        g->p += 2;
        return;
    }
    if (draw_weight && rnd() % 10 < draw_weight && g->end - p >= 0x80) {
        g->p += emit_drawing(g, p);
        return;
    }
    switch (rnd() % 10) {
    case 0: case 1: case 2: case 3: case 4:
        op = simple[rnd() % sizeof simple];
        break;
    case 5: op = (uint8_t[]){ 0x24, 0x1c, 0x21, 0x39 }[rnd() % 4]; break;
    case 6: op = (uint8_t[]){ 0x0d, 0x16 }[rnd() % 2]; break;
    case 7: op = 0x03; break;
    default: op = (uint8_t[]){ 0x23, 0x8a, 0x5c, 0x5b, 0x3f, 0x74, 0x8b, 0x4a, 0x33, 0x3b, 0x46, 0x41 }[rnd() % 12]; break;
    }
    if (g->depth >= 3 && (op == 0x23 || op == 0x8a || op == 0x5c || op == 0x5b || op == 0x3f ||
                          op == 0x74 || op == 0x8b || op == 0x5a || op == 0x4a || op == 0x33 ||
                          op == 0x3b || op == 0x46 || op == 0x41))
        op = 0x02;
    /* Generation order is execution order (branches go forward, and each
     * subroutine is called from where it was made), so once a context may
     * be set it may be set for everything generated after. */
    if (op == 0x5b && context_possible)
        op = 0x02;
    if (op == 0x89)
        context_possible = 1;
    if (counting)
        cover_emitted[op]++;
    if (scene_dca && (op == 0x51 || op == 0x67 || op == 0x2e))
        scene_is_map = 1;                       /* the line colour changes */
    w16(p, op);
    switch (op) {
    case 0x02: case 0x43: case 0x64: case 0x85: case 0x04: case 0x76: case 0x7d:
        g->p += 2;
        break;
    case 0x86:
        g->p += 3;
        break;
    case 0x14: case 0x17: case 0x1f: case 0x51: case 0x52: case 0x5e: case 0x71:
    case 0x81: case 0x87: case 0x92: case 0x93: case 0x88:
        w16(p + 2, small_word());
        g->p += 4;
        break;
    case 0x47:
        scene_plain = scene_steady = 0;         /* it moves the eye */
        w16(p + 2, rnd() % 4 ? rnd() % 0x30 : (uint16_t)rnd());
        g->p += 4;
        break;
    case 0x48:
        w16(p + 2, rnd() % 2 ? 0 : 1);
        if (p[2] == 0 && p[3] == 0)
            g->context = 0;
        g->p += 4;
        break;
    case 0x54: case 0x61:
        g->p += 6;
        break;
    case 0x2d: case 0x2e:                       /* three colour bytes */
        p[2] = (uint8_t)rnd(), p[3] = (uint8_t)rnd(), p[4] = (uint8_t)rnd(), p[5] = 0;
        g->p += 6;
        break;
    case 0x84: case 0x68:                       /* a normal to shade */
        emit_normal(p + 2);
        g->p += 8;
        break;
    case 0x55:                                  /* a colour by the eye's place */
        w16(p + 2, (uint16_t)rnd());
        w16(p + 4, (uint16_t)rnd());
        w16(p + 6, rnd() % 4 ? (uint16_t)(rnd() % 0x10) : (uint16_t)rnd());
        w16(p + 8, 0);
        g->p += 10;
        break;
    case 0x70:                                  /* the eye inside an area */
        n = 1 + rnd() % 8;
        w16(p + 4, n);
        for (i = 0; i < n; i++) {
            w16(p + 6 + 4 * i, rnd() % 4 ? (uint16_t)(rnd() % 0x11 - 8) : (uint16_t)rnd());
            w16(p + 8 + 4 * i, rnd() % 4 ? (uint16_t)(rnd() % 0x11 - 8) : (uint16_t)rnd());
        }
        g->p += 6 + 4 * n;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x35: case 0x45:                       /* a row of points */
        emit_point(p + 2);
        emit_point(p + 8);
        w16(p + 0xe, 1 + rnd() % 8);
        g->p += 0x10;
        break;
    case 0x31:                                  /* a row into the point table */
        n = 1 + rnd() % 8;
        /* clear of the kept points, 0x30..0x3f */
        w16(p + 2, rnd() % 2 ? rnd() % (0x30 - n) : 0x40 + rnd() % 0x100);
        w16(p + 4, n);
        emit_point(p + 6);
        emit_point(p + 0xc);
        g->p += 0x12;
        break;
    case 0x36: {                                /* a row of points by a mask */
        /* The counter goes on (ebx + 15) >> 4 past +0x1e, ebx as the last
         * point left it: its screen x only when it is drawn.  So only where
         * the scene puts a point at the eye's x and y on screen at x 0x140
         * (an identity matrix, the centre 0x140, +0xd42 small), the last
         * point's bit set and both ends there, the mask bytes with their top
         * bit set and nops after: landing in them goes on, or stops at the
         * 0x0200 an odd landing reads. */
        int32_t eye[3];
        uint32_t k;

        if (!scene_plain || g->end - p < 0x90) {
            w16(p, 0x02);
            g->p += 2;
            break;
        }
        memcpy(eye, global + 0xcdc, 12);
        n = 1 + rnd() % 7;
        for (k = 0; k < 2; k++) {
            w16(p + 2 + 6 * k, (uint16_t)(eye[0] >> 16));
            w16(p + 4 + 6 * k, (uint16_t)(eye[1] >> 16));
            w16(p + 6 + 6 * k, (uint16_t)((eye[2] >> 16) + 0x100 + rnd() % 0x100));
        }
        w16(p + 0xe, n);
        for (i = 0x10; i < 0x1e; i++)
            p[i] = 0xff;
        for (i = 0; i < n; i++)
            p[0x1e + i] = (uint8_t)(rnd() | 0x80);
        p[0x1e + n - 1] |= (uint8_t)(1 << (n - 1));
        for (i = 0x1e + n; i < 0x1e + n + 0x60; i += 2)
            w16(p + i, 0x02);
        g->p += 0x1e + n + 0x60;
        break;
    }
    case 0x34:                                  /* an object's scale */
        scene_plain = scene_steady = 0;         /* it moves the eye */
        w16(p + 4, rnd() % 3 ? 0 : (uint16_t)rnd());
        w16(p + 6, rnd() % 3 ? 0 : (uint16_t)rnd());
        w16(p + 8, rnd() % 4 ? rnd() % 0x19 : (uint16_t)rnd());
        g->p += 0xa;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x2f: case 0x77: case 0x4c: case 0x83: case 0x3a: case 0x3c:
        scene_plain = scene_steady = 0;         /* they move the eye */
        g->p += emit_object(g, p, op);
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x15: {                                /* a grid of ground */
        /* small (it fills the point table from 0), near the eye: columns,
         * rows, cell sizes, origin, the height range, then a texture word
         * and a height a vertex */
        uint16_t cols = (uint16_t)(1 + rnd() % 4), rows = (uint16_t)(1 + rnd() % 4);
        uint16_t cx = (uint16_t)(1 + rnd() % 0x20), cz = (uint16_t)(1 + rnd() % 0x20);
        uint32_t nv = (uint32_t)(cols + 1) * (rows + 1);

        if (g->end - p < (int32_t)(0x12 + 4 * nv)) {
            w16(p, 0x02);
            g->p += 2;
            break;
        }
        w16(p + 2, cols);
        w16(p + 4, rows);
        w16(p + 6, cx);
        w16(p + 8, cz);
        w16(p + 0xa, (uint16_t)(-(int32_t)(cols * cx) / 2 + (int32_t)(rnd() % 0x21) - 0x10));
        w16(p + 0xc, (uint16_t)(-(int32_t)(rows * cz) / 2 + (int32_t)(rnd() % 0x21) - 0x10));
        w16(p + 0xe, (uint16_t)(rnd() % 5 - 4));
        w16(p + 0x10, (uint16_t)(rnd() % 5));
        for (i = 0; i < nv; i++) {
            w16(p + 0x12 + 4 * i, (uint16_t)rnd());
            w16(p + 0x14 + 4 * i, (uint16_t)(rnd() % 7 - 3));
        }
        g->p += 0x12 + 4 * nv;
        break;
    }
    case 0x18: case 0x5d: {                     /* a textured surface, then its vertices */
        uint8_t *q2;

        if (g->end - p < 0x60) {
            w16(p, 0x02);
            g->p += 2;
            break;
        }
        if (op == 0x18) {
            /* the cached slot (low 14 bits: an offset into the table, a
             * slot's or not) and the kind/age (top two), the offsets, the
             * name */
            uint16_t slot = rnd() % 2 ? (uint16_t)(6 + 0x1c * (rnd() % 8)) : (uint16_t)rnd();

            w16(p + 2, (uint16_t)((slot & 0x3fff) | (rnd() % 4) << 14));
            for (i = 4; i < 0xa; i += 2)
                w16(p + i, (uint16_t)rnd());
            memset(p + 0xa, 0, 12);
            memcpy(p + 0xa, texture_names[rnd() % NTEXTURE_NAMES], 11);
            w16(p + 0x16, 0x05);                /* the surface's word, stepped over */
            q2 = p + 0x18;
        } else {
            for (i = 2; i < 8; i += 2)
                w16(p + i, (uint16_t)rnd());
            q2 = p + 8;
        }
        n = 2 + rnd() % 4;
        w16(q2, 0x06);
        emit_point(q2 + 2);
        q2 += 8;
        for (i = 0; i < n; i++) {
            w16(q2, 0x07);
            emit_point(q2 + 2);
            q2 += 8;
        }
        w16(q2, 0x08);
        g->p = q2 + 2;
        break;
    }
    case 0x6a: {                                /* a strip: a start and next points */
        /* near the eye mostly, on the ground: quads, and their markings,
         * need the eye beside the segment or close to an end */
#define STRIP_POINT(q) (rnd() % 3 ? (w16((q), (uint16_t)(rnd() % 0x61 - 0x30)),                \
                                     w16((q) + 2, (uint16_t)(rnd() % 5 - 2)),                  \
                                     w16((q) + 4, (uint16_t)(rnd() % 0x61 - 0x30)))            \
                                  : emit_point(q))
        static const uint8_t next[5] = { 0x6a, 0x6c, 0x6f, 0x78, 0x79 };
        uint8_t *q2 = p;

        if (g->end - p < 0x50) {
            w16(p, 0x02);
            g->p += 2;
            break;
        }
        w16(q2, rnd() % 2 ? 0x69 : 0x6b);
        w16(q2 + 2, rnd() % 4 ? (uint16_t)(1 + rnd() % (rnd() % 2 ? 8 : 0x40)) : (uint16_t)rnd());
        STRIP_POINT(q2 + 4);
        q2 += 10;
        n = 1 + rnd() % 4;
        uint32_t lights = rnd() % 2;            /* half of them edge lights */
        for (i = 0; i < n; i++) {
            uint8_t o = lights ? 0x6f : next[rnd() % 5];

            w16(q2, o);
            if (o == 0x78 || o == 0x79) {
                w16(q2 + 2, rnd() % 4 ? (uint16_t)(1 + rnd() % (rnd() % 2 ? 8 : 0x40)) : (uint16_t)rnd());
                STRIP_POINT(q2 + 4);
                q2 += 10;
            } else {
                STRIP_POINT(q2 + 2);
                q2 += 8;
            }
        }
        g->p = q2;
        break;
    }
    case 0x38: {                                /* a mesh compiled from what follows */
        /* with GLOBAL +0x12a4 bit 4, the next instruction it takes --
         * past a 0x1e, 0x7d or 0x81 -- is triangulated into a 0x90: a
         * surface of table points, a facet, a face, four to ten points */
        uint8_t *q2 = p + 2;
        uint32_t kind = rnd() % 4;

        if (g->end - p < 0x80) {
            w16(p, 0x02);
            g->p += 2;
            break;
        }
        if (rnd() % 4 == 0) {
            static const uint16_t skip[3] = { 0x1e, 0x7d, 0x81 };
            uint16_t sop = skip[rnd() % 3];

            w16(q2, sop);
            if (sop == 0x7d) {
                q2 += 2;
            } else {
                w16(q2 + 2, sop == 0x1e ? 0 : (uint16_t)rnd());
                q2 += 4;
            }
        }
        n = 4 + rnd() % 7;
        if (kind <= 1) {
            w16(q2, 0x05);
            w16(q2 + 2, 0x0f);
            w16(q2 + 4, point_index());
            for (i = 1; i < n; i++) {
                w16(q2 + 2 + 4 * i, 0x10);
                w16(q2 + 4 + 4 * i, point_index());
            }
            w16(q2 + 2 + 4 * n, 0x08);
            q2 += 4 + 4 * n;
        } else if (kind == 2) {
            w16(q2, 0x3e);
            w16(q2 + 2, n);
            emit_normal(q2 + 4);
            w32(q2 + 10, plane_distance(q2 + 4));
            for (i = 0; i < n; i++)
                w16(q2 + 14 + 2 * i, point_index());
            q2 += 14 + 2 * n;
        } else {
            w16(q2, 0x1d);
            w16(q2 + 2, n);
            emit_point(q2 + 4);
            emit_normal(q2 + 10);
            for (i = 0; i < n; i++)
                w16(q2 + 16 + 2 * i, point_index());
            q2 += 16 + 2 * n;
        }
        g->p = q2;
        break;
    }
    case 0x28:                                  /* a light of a size */
        p[2] = (uint8_t)rnd();
        p[3] = (uint8_t)(rnd() % 4 ? rnd() % 5 : rnd());
        emit_point(p + 4);
        g->p += 0xa;
        break;
    case 0x90: {                                /* a mesh of table points */
        /* index bytes from a base, a top bit starting a new centre or
         * polygon; the first always has it -- a fan's centre is otherwise
         * a stack slot nothing set (docs/modules/g3d.md) */
        n = 1 + rnd() % 12;
        w16(p + 2, n);
        if (rnd() % 4 == 0) {
            memset(p + 4, 0, 10);
        } else {
            emit_normal(p + 4);
            w32(p + 10, plane_distance(p + 4));
        }
        p[0xe] = (uint8_t)(rnd() % 4 ? rnd() % 0x20 : rnd());
        for (i = 0; i < n + 2; i++)
            p[0xf + i] = (uint8_t)(rnd() % 4 == 0 ? 0x80 | rnd() % 0x20 : rnd() % 0x20);
        p[0xf] |= 0x80;
        g->p += (n + 0x10) & ~1u;
        break;
    }
    case 0x1a: case 0x29: {                     /* points into the table */
        uint32_t per = op == 0x29 ? 12 : 6;

        n = 1 + rnd() % 6;
        /* clear of the kept points, 0x30..0x3f */
        w16(p + 2, rnd() % 2 ? rnd() % (0x30 - n) : 0x40 + rnd() % 0x100);
        w16(p + 4, n);
        for (i = 0; i < n; i++) {
            emit_point(p + 6 + per * i);
            if (op == 0x29)
                emit_normal(p + 12 + per * i);
        }
        g->p += 6 + per * n;
        break;
    }
    case 0x32: case 0x2b: case 0x75: {          /* queue a model */
        uint8_t *m = models + 0x100 * (rnd() % NMODELS);
        int32_t off = (int32_t)(m - p);

        if (op == 0x2b) {
            w32(p + 2, (uint32_t)off);
            g->p += 6;
        } else if (off >= -0x8000 && off <= 0x7fff) {
            w16(p + 2, (uint16_t)off);
            g->p += 4;
        } else {
            w16(p, 0x02);
            g->p += 2;
        }
        break;
    }
    case 0x5a: {                                /* a subroutine run nested */
        /* its offset from +2; nothing sets 0x206fdc37 yet, so nothing is
         * queued and the program goes on past the instruction */
        uint8_t *sub = subroutine(g);

        if (sub == NULL) {
            w16(p, 0x02);
            g->p += 2;
            break;
        }
        w16(p + 2, (uint32_t)(sub - (p + 2)));
        g->p += 4;
        break;
    }
    case 0x73:                                  /* the eye in a box */
        for (i = 0; i < 3; i++) {
            int16_t lo = (int16_t)rnd(), hi = (int16_t)rnd();

            if (rnd() % 3 == 0)
                lo = -0x8000, hi = 0x7fff;
            if (lo > hi) {
                int16_t t = lo;

                lo = hi, hi = t;
            }
            w16(p + 4 + 4 * i, (uint16_t)lo);
            w16(p + 6 + 4 * i, (uint16_t)hi);
        }
        g->p += 0x10;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x1b:                                  /* the eye in a turned box */
        for (i = 4; i < 0x16; i += 2)
            w16(p + i, (uint16_t)rnd());
        for (i = 0xa; i < 0x10; i += 2)
            if (rnd() % 2)
                w16(p + i, 0xfffe);
        g->p += 0x16;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x40:                                  /* an object's shadow */
        w16(p + 2, variable(g));
        g->p += 4;
        break;
    case 0x82:                                  /* a shadow of its own object */
        for (i = 2; i < 0x14; i += 2)
            w16(p + i, (uint16_t)rnd());
        g->p += 0x14;
        break;
    case 0x91:                                  /* a value at a point */
        emit_point(p + 2);
        w16(p + 8, variable(g));
        w16(p + 0xa, (uint16_t)rnd());
        g->p += 0xc;
        break;
    case 0x57:                                  /* a weather effect */
        for (i = 0; i < 8; i++)
            p[2 + i] = (uint8_t)rnd();
        g->p += 10;
        break;
    case 0x30:                                  /* a shade from the sun */
        w16(p + 2, (uint16_t)rnd());
        g->p += 4;
        break;
    case 0x69: case 0x6b: case 0x6e:            /* a light */
        for (i = 0; i < 4; i++)
            w16(p + 2 + 2 * i, (uint16_t)rnd());
        g->p += 10;
        break;
    case 0x62:                                  /* a reject by table points */
        n = 1 + rnd() % 6;
        w16(p + 4, n);
        for (i = 0; i < n; i++)
            w16(p + 6 + 2 * i, point_index());
        g->p += 6 + 2 * n;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x1e:                                  /* a colour word, or 0 */
        w16(p + 2, rnd() % 4 ? (uint16_t)rnd() : 0);
        g->p += 4;
        break;
    case 0x56:                                  /* a level of detail, then on */
        g->p += 4;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x5f: case 0x6d:                       /* on when small enough */
        w16(p + 4, rnd() % 4 ? (uint16_t)(rnd() % 0x400) : (uint16_t)rnd());
        w16(p + 6, rnd() % 4 ? (uint16_t)(rnd() % 0x100) : (uint16_t)rnd());
        g->p += 8;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x58:
        for (i = 0; i < 4; i++)
            w32(p + 2 + 4 * i, spread());
        g->p += 18;
        break;
    case 0x8d: case 0x8f:
        w32(p + 2, rnd());
        g->p += 6;
        break;
    case 0x89:
        w32(p + 2, (uint32_t)(uintptr_t)context + CONTEXT_SIZE / 2);
        g->context = 1;
        g->p += 6;
        break;
    case 0x26: case 0x7e:
        w16(p + 2, variable(g));
        w16(p + 4, small_word());
        g->p += 6;
        break;
    case 0x4d: case 0x4e:
        w16(p + 2, variable(&(struct gen){ .context = 0 }));
        w16(p + 4, variable(g));
        g->p += 6;
        break;
    case 0x4f:
        w16(p + 2, variable(&(struct gen){ .context = 0 }));
        w16(p + 4, variable(&(struct gen){ .context = 0 }));
        g->p += 6;
        break;
    case 0x65: case 0x67: case 0x8e:
        w16(p + 2, variable(g));
        g->p += 4;
        break;
    case 0x24: case 0x1c: case 0x21: case 0x39: {
        uint32_t len = op == 0x24 ? 10 : op == 0x1c ? 16 : op == 0x21 ? 22 : 8, k;

        if (op == 0x39) {
            uint16_t v = variable(g), mask = small_word();

            w16(p + 4, v);
            w16(p + 6, mask);
            if (rnd() % 2)
                steer(v, rnd() % 2 ? (uint16_t)(mask & rnd()) : (uint16_t)~mask);
        } else {
            for (k = 0; 4 + 6 * k < len; k++) {
                int16_t lo = (int16_t)(rnd() % 3 ? -(int)(rnd() % 4) : (int16_t)rnd());
                int16_t hi = (int16_t)(rnd() % 3 ? (int)(rnd() % 4) : (int16_t)rnd());
                uint16_t v = variable(g);

                w16(p + 4 + 6 * k, v);
                w16(p + 6 + 6 * k, (uint16_t)lo);
                w16(p + 8 + 6 * k, (uint16_t)hi);
                /* onto the bounds: each side of each */
                if (rnd() % 4)
                    steer(v, (uint16_t)((int16_t[]){ lo - 1, lo, hi, hi + 1,
                                                     (lo + hi) / 2 }[rnd() % 5]));
            }
        }
        g->p += len;
        /* the skipped part, then the landing point */
        q = g->p;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        (void)q;
        break;
    }
    case 0x0d: case 0x16:
        g->p += 4;
        emit_block(g, rnd() % 3);
        w16(p + 2, (uint32_t)(g->p - p));
        break;
    case 0x03:
        n = rnd() % 5;
        w16(p + 2, n);
        {
            uint16_t v = variable(g);

            w16(p + 4, v);
            /* each index, the count itself, one past it, the top */
            if (rnd() % 4)
                steer(v, rnd() % 2 ? (uint16_t)(rnd() % (n + 1)) :
                         (uint16_t[]){ (uint16_t)(n + 1), 0xffff, 0x7fff }[rnd() % 3]);
        }
        g->p += 8 + 2 * n;
        {
            /* every target the same landing point, after a few skipped
             * instructions.  +6 is the default 16-bit code would take; retail
             * reads its default 0x20006 on, so the landing goes there too. */
            uint8_t *land;

            emit_block(g, rnd() % 2);
            land = g->p;
            w16(p + 6, (uint32_t)(land - p));
            for (i = 0; i < n; i++)
                w16(p + 8 + 2 * i, (uint32_t)(land - p));
            w16(p + 0x20006, (uint32_t)(land - p));
        }
        break;
    case 0x23: case 0x8a: case 0x5c: case 0x5b: case 0x3f: case 0x74: case 0x8b: case 0x4a:
    case 0x33: case 0x3b: case 0x46: case 0x41: {
        /* 0x3f runs its subroutine as in the map: no lines there either */
        int map = scene_is_map;
        uint8_t *sub;

        if (op == 0x3f || op == 0x4a || op == 0x41)
            scene_is_map = 1;
        sub = subroutine(g);
        scene_is_map = map;

        if (sub == NULL) {
            w16(p, 0x02);
            g->p += 2;
            break;
        }
        if (op == 0x74 || op == 0x8b) {
            /* a program for later, in one of the 64 lists (or past them) */
            uint16_t list = rnd() % 8 ? (uint16_t)(rnd() % 0x40) : (uint16_t)rnd();

            if (op == 0x74) {
                w16(p + 2, (uint32_t)(sub - p));
                w16(p + 4, list);
                g->p += 6;
            } else {
                w32(p + 2, (uint32_t)(sub - p));
                w16(p + 6, list);
                g->p += 8;
            }
        } else if (op == 0x33 || op == 0x3b || op == 0x46) {
            /* a turned subroutine: angles, a variable's record of them, or
             * a move and angles with variables added (a shadow's, with
             * 0x206f91bd set) */
            scene_plain = scene_steady = 0;     /* the eye moves inside */
            w16(p + 2, (uint32_t)(sub - p));
            if (op == 0x33) {
                for (i = 4; i < 0xa; i += 2)
                    w16(p + i, rnd() % 3 ? (uint16_t)rnd() : 0);
                g->p += 0xa;
            } else if (op == 0x3b) {
                w16(p + 4, variable(g));
                g->p += 6;
            } else {
                for (i = 4; i < 0xa; i += 2)
                    w16(p + i, rnd() % 2 ? (uint16_t)(rnd() % 0x40 - 0x20) : (uint16_t)rnd());
                for (i = 0xa; i < 0x16; i += 4) {
                    w16(p + i, rnd() % 3 ? (uint16_t)rnd() : 0);
                    w16(p + i + 2, rnd() % 2 ? 0 : variable(g));
                }
                g->p += 0x16;
            }
        } else if (op == 0x41) {
            /* an object's shadow: the target, a variable holding the
             * angles' record */
            w16(p + 2, (uint32_t)(sub - p));
            w16(p + 4, variable(g));
            g->p += 6;
        } else if (op == 0x4a) {
            /* a shadow: the target, a variable holding the angles' record,
             * the sun's direction */
            w16(p + 2, (uint32_t)(sub - p));
            w16(p + 4, variable(g));
            for (i = 6; i < 0xc; i += 2)
                w16(p + i, (uint16_t)rnd());
            g->p += 0xc;
        } else if (op == 0x23 || op == 0x3f) {
            w16(p + 2, (uint32_t)(sub - p));
            g->p += 4;
        } else if (op == 0x8a) {
            w32(p + 2, (uint32_t)(sub - p));
            g->p += 6;
        } else if (op == 0x5c) {
            w32(p + 2, (uint32_t)(uintptr_t)sub);
            g->p += 6;
        } else {
            /* through a reserved GLOBAL variable set to the address.  Not
             * where a context may be set: the variable would read the
             * context buffer instead (context_possible) */
            uint16_t v;
            uint32_t at;

            /* one variable per 0x5b: two sharing one would overwrite the
             * first's address, and a subroutine could call itself */
            if (reserved_next >= nreserved) {
                w16(p, 0x02);
                g->p += 2;
                break;
            }
            v = reserved_vars[reserved_next++];
            memcpy(&at, retail.pristine + VAR_TABLE + 4 * v, 4);
            w32(global + at, (uint32_t)(uintptr_t)sub);
            w16(p + 2, v);
            g->p += 4;
        }
        break;
    }
    default:
        w16(p, 0x02);
        g->p += 2;
        break;
    }
}

static void emit_block(struct gen *g, uint32_t n)
{
    while (n--)
        emit_one(g);
}

/* The models: 'v', a context now and then (0x89), an object instruction
 * (one of the five the queue takes), a few instructions it skips to the end of when the object is
 * not drawn, and a return.  Made as map code: the map's state when they
 * run is not known. */
static void make_models(void)
{
    static const uint8_t first[] = { 0x3a, 0x4c, 0x3c, 0x2f, 0x77 };
    uint32_t m;
    int map = scene_is_map, plain = scene_plain;

    scene_is_map = 1;
    scene_plain = 0;
    scene_steady = 0;                           /* no scene yet: what the */
    scene_dca = 0;                              /* other side left */
    context_possible = 1;
    draw_weight = 5;
    for (m = 0; m < NMODELS; m++) {
        struct gen g;
        uint8_t *p = models + 0x100 * m, *obj;

        memset(p, 0, 0x100);
        w16(p, 0x76);
        g.p = p + 2;
        g.end = p + 0xc0;
        g.depth = 3;
        g.context = 0;
        if (rnd() % 4 == 0) {
            w16(g.p, 0x89);
            w32(g.p + 2, (uint32_t)(uintptr_t)context + CONTEXT_SIZE / 2);
            g.context = 1;
            g.p += 6;
        }
        /* only the five: any other first instruction is retail's crash
         * (docs/modules/g3d.md) */
        obj = g.p;
        g.p += emit_object(&g, g.p, first[rnd() % sizeof first]);
        emit_block(&g, rnd() % 4);
        w16(obj + 2, (uint32_t)(g.p - obj));
        w16(g.p, 0x22);
    }
    scene_is_map = map;
    scene_plain = plain;
}

/* A program at programs+0: a few instructions and an end.  `contextual`
 * when it will run with a context already set. */
static uint32_t make_program(int contextual)
{
    struct gen g;
    uint32_t i;

    for (i = 0; i < PROGRAM_SIZE; i += 4)
        if (i < 0x10000 || i >= 0x10000 + 0x100 * NMODELS)
            w32(programs + i, rnd());
    g.p = programs + 0x10;
    g.end = programs + 0x8000;
    sub_next = programs + 0x8000;
    reserved_next = 0;
    context_possible = contextual;
    g.depth = 0;
    g.context = 0;
    programs[0x0e] = 0x76;            /* a model's 'v' before it, for +0x28 */
    /* G3D_SMALL: one drawing and nothing else, to find a minimal case */
    if (getenv("G3D_SMALL"))
        g.p += emit_drawing(&g, g.p);
    else
        emit_block(&g, 2 + rnd() % 24);
    w16(g.p, 0x00);
    if (counting)
        cover_programs++;
    return (uint32_t)(uintptr_t)(programs + 0x10);
}

/* ---- the operations ---------------------------------------------------------------- */

static void op_normalise(void)
{
    uint32_t *v = (uint32_t *)(global + 0x1200), i;

    for (i = 0; i < 3; i++)
        v[i] = rnd() % 8 == 0 ? 0 : spread();
    if (rnd() % 8 == 0)
        v[0] = v[1] = v[2] = 0;
    /* now and then tiny, where the shift clamps at bit 3 */
    if (rnd() % 6 == 0)
        for (i = 0; i < 3; i++)
            v[i] = (uint32_t)((int32_t)(rnd() % 31) - 15);
    put("+3c(%08x %08x %08x)", v[0], v[1], v[2]);
    if (counting) {
        uint32_t m = 0;

        for (i = 0; i < 3; i++)
            m |= v[i] ^ (uint32_t)((int32_t)v[i] >> 31);
        cover_norm[m ? 32 - __builtin_clz(m) : 0]++;
    }
    call_service(0x3c, 1, (uint32_t)(uintptr_t)v);
    put(" = %08x %08x %08x", v[0], v[1], v[2]);
}

/* +0x12a is the point table's pointer, into the image: usually left as it
 * is, sometimes anything for the call and put back after, as the drawing
 * reads the table through it. */
static void op_get(void)
{
    uint32_t v = rnd(), keep;

    memcpy(&keep, cur->image + 0x12a, 4);
    if (rnd() % 4 == 0)
        memcpy(cur->image + 0x12a, &v, 4);
    else
        v = 0;
    put("+40(%08x)", v);
    call_service(0x40, 0);
    memcpy(cur->image + 0x12a, &keep, 4);
}

static void op_scale(void)
{
    uint32_t x = spread();
    uint16_t shift = (uint16_t)(rnd() % 4 == 0 ? rnd() : rnd() % 30);
    uint32_t d;

    switch (rnd() % 6) {
    case 0: d = 0; break;
    case 1: d = rnd() & 1 ? 1 : 0xffffffffu; break;
    default: d = spread(); break;
    }
    memcpy(global + 0xd42, &shift, 2);
    memcpy(global + 0x320, &d, 4);
    if (counting && ((shift + 8) & 0xffff) >= 32)
        cover_shift_masked++;
    put("+44(%08x) d42 %04x 320 %08x", x, shift, d);
    call_service(0x44, 1, x);
}

static void op_tick(void)
{
    uint8_t a = (uint8_t)rnd(), b = (uint8_t)rnd();

    if (rnd() % 2) a = (uint8_t)(rnd() % 3);
    cur->image[0x15b] = a;
    cur->image[0x15c] = b;
    put("+5c(%02x %02x)", a, b);
    call_service(0x5c, 0);
}

static void op_next(void)
{
    uint32_t i, at = 2 * (rnd() % 0x40);
    uint16_t opcode = (uint16_t)(rnd() % 8 == 0 ? rnd() : rnd() % 0xa0);

    if (rnd() % 8 == 0)                         /* the top of the table */
        opcode = (uint16_t)(0x92 + rnd() % 3);

    for (i = 0; i < sizeof buffer; i++)
        buffer[i] = (uint8_t)rnd();
    for (i = 2; i < 0x40; i += 2)
        if (rnd() % 2)
            buffer[at + i] = (uint8_t)(rnd() % 8), buffer[at + i + 1] = 0;
    memcpy(buffer + at, &opcode, 2);
    if (counting && opcode < 0x100)
        cover_opcodes[opcode]++;
    put("+64(+%x op %04x)", at, opcode);
    call_service(0x64, 1, (uint32_t)(uintptr_t)(buffer + at));
    if (regs_out[0] >= (uint32_t)(uintptr_t)buffer &&
        regs_out[0] < (uint32_t)(uintptr_t)buffer + 0x10000)
        put(" = +%x", regs_out[0] - (uint32_t)(uintptr_t)buffer);
}

/* The texture table (0x206ff8b4): slots of 0x1c bytes after a 6-byte
 * header, with flags at +0, +0xd, an age word at +0x12 and a texture at
 * +0x14.  Seeded before +0x54, +0x58: some loaded, some flagged with none,
 * ages that tie. */

static void seed_textures(void)
{
    uint32_t table, i, n;
    uint8_t *t;

    memcpy(&table, cur->image + 0x68b4, 4);
    if (table == 0)
        return;
    t = (uint8_t *)(uintptr_t)table;
    n = *(uint16_t *)t;
    for (i = 0; i < n; i++) {
        uint8_t *slot = t + 6 + 0x1c * i;

        if (rnd() % 3 == 0)
            continue;
        slot[0] = (uint8_t)(rnd() % 2 ? 0 : rnd());
        slot[0xd] = (uint8_t)rnd();
        w16(slot + 0x12, rnd() % 4 ? rnd() % 8 : rnd());
        w32(slot + 0x14, rnd() % 3 ? 0x1000 + rnd() % 0x1000 : 0);
        /* named as 0x18's programs name them, in use with a texture loaded,
         * so they find it */
        if (slot[0] && rnd() % 2) {
            memset(slot, 0, 13);
            memcpy(slot, texture_names[rnd() % 6], 11);
            w32(slot + 0xe, rnd() % 2 ? 0 : (uint32_t)(uintptr_t)context + CONTEXT_SIZE / 2);
            w32(slot + 0x14, 0x1000 + rnd() % 0x1000);
        }
    }
}

static void op_textures(void)
{
    uint32_t k = rnd() % 3;

    if (rnd() % 2)
        seed_textures();
    if (k == 0) {
        put("+54");
        call_service(0x54, 0);
        if (counting) cover_released++;
    } else if (k == 1) {
        put("+58");
        call_service(0x58, 0);
    } else {
        uint16_t v = (uint16_t)(rnd() % 3);

        memcpy(global + 0x2a8, &v, 2);
        put("+60(%u)", v);
        call_service(0x60, 0);
    }
}

/* +0x38: a model queued, in a context now and then; sometimes two bytes
 * in, which is not a model ('v') at all. */
static void op_queue_model(void)
{
    uint32_t n = 1 + rnd() % 4, i;

    for (i = 0; i < n; i++) {
        uint32_t m = (uint32_t)(uintptr_t)(models + 0x100 * (rnd() % NMODELS));
        uint32_t key = rnd() % 3 ? 0 : (uint32_t)(uintptr_t)context + CONTEXT_SIZE / 2;

        if (rnd() % 8 == 0)
            m += 2;
        set_scene();
        put("+38(");
        put_ptr(m);
        put(",");
        put_ptr(key);
        put(")");
        call_service(0x38, 2, m, key);
    }
}

/* +0x34 between two altitudes, emptied after or not; or +0x30, all of it. */
static void op_queue_draw(void)
{
    set_scene();
    if (rnd() % 3 == 0) {
        put("+30()");
        call_service(0x30, 0);
        return;
    }
    {
        uint32_t a = rnd() % 4 ? (uint32_t)((int32_t)(rnd() % 0x200) - 0x100) : spread();
        uint32_t b = rnd() % 4 ? (uint32_t)((int32_t)(rnd() % 0x200) - 0x100) : spread();
        uint32_t reset = rnd() % 2;

        if (rnd() % 4 == 0)
            a = 0x7fffffff, b = 0x80000001;
        put("+34(%08x,%08x,%u)", a, b, reset);
        call_service(0x34, 3, a, b, reset);
    }
}

static void op_run(void)
{
    uint32_t k = rnd() % 5, ctx = 0, prog;

    if (k == 1 && rnd() % 2)
        ctx = (uint32_t)(uintptr_t)context + CONTEXT_SIZE / 2;
    set_scene();
    draw_weight = rnd() % 4 ? 5 + rnd() % 4 : 0;
    /* +0x1c leaves the context (0x206f91b7) as an earlier program left it;
     * +0x18 sets it, and the models clear it */
    {
        uint32_t left;

        memcpy(&left, cur->image + 0x1b7, 4);
        prog = make_program(ctx != 0 || (k == 0 && left != 0));
    }
    if (getenv("G3D_DUMPPROG") && (uint32_t)atoi(getenv("G3D_DUMPPROG")) == op_number &&
        cur == &retail) {
        FILE *f = fopen("g3d_program.bin", "wb");

        fwrite(programs, 1, PROGRAM_SIZE, f);
        fclose(f);
        f = fopen("g3d_global.bin", "wb");
        fwrite(global, 1, GLOBAL_SIZE, f);
        fclose(f);
        f = fopen("g3d_image.bin", "wb");
        fwrite(retail.pristine, 1, IMAGE_SIZE, f);
        fclose(f);
        fprintf(stderr, "programs at %p, global at %p\n", (void *)programs, (void *)global);
    }
    switch (k) {
    case 0:
        put("+1c");
        call_service(0x1c, 1, prog);
        break;
    case 1: {
        put("+18(ctx %s)", ctx ? "set" : "0");
        call_service(0x18, 2, prog, ctx);
        break;
    }
    default: {
        uint32_t slot = k == 2 ? 0x28 : k == 3 ? 0x20 : 0x24;

        /* the model's 'v' at an even address or an odd one: the program
         * starts at the next even address either way */
        uint32_t model = prog - 2 + (rnd() % 2);

        programs[model - (uint32_t)(uintptr_t)programs] = rnd() % 8 ? 0x76 : 0x75;
        put("+%x(model%s%s)", slot, model & 1 ? " odd" : "",
            programs[model - (uint32_t)(uintptr_t)programs] == 0x76 ? "" : " not v");
        call_service(slot, 1, model);
        break;
    }
    }
}

static void op(void)
{
    uint32_t k = rnd() % 12;

    if (k >= 7 && k <= 9)
        k = 7;              /* programs, three times in twelve */
    if (counting)
        cover_ops[k]++;
    put("#%u ", op_number);
    if (getenv("G3D_TRACE")) {
        fprintf(stderr, "scenario %u %s op %u\n", scenario, cur->name, op_number);
        fflush(stderr);
    }
    switch (k) {
    case 0: op_normalise(); break;
    case 1: op_get(); break;
    case 2: op_scale(); break;
    case 3: op_tick(); break;
    case 4: op_next(); break;
    case 5: case 6: op_textures(); break;
    case 10: op_queue_model(); break;
    case 11: op_queue_draw(); break;
    default: op_run(); break;
    }
    put("\n");
    if (getenv("G3D_TRACE")) {
        uint32_t from = cur->length;

        while (from > 0 && cur->transcript[from - 1 - (from == cur->length)] != '\n')
            from--;
        fprintf(stderr, "  %.*s", (int)(cur->length - from), cur->transcript + from);
    }
    snapshot();
}

static void run_side(struct side *s)
{
    uint32_t i, nops, k;

    cur = s;
    s->length = 0;
    s->nsnap = 0;
    s->ncallbacks = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    memset(arena, 0, ARENA_SIZE);
    memset(context, 0, CONTEXT_SIZE);
    memset(programs, 0, PROGRAM_SIZE);
    rng = scenario_seed;
    for (k = 0; k < GLOBAL_SIZE; k++)
        global[k] = (uint8_t)rnd();
    for (k = 0; k < CONTEXT_SIZE; k++)
        context[k] = (uint8_t)rnd();
    /* the queue's size by GLOBAL +0xb60, a detail level 0..4: +0x38 and
     * +0x34 read the table at 0x206fd864 with it unclamped, and 5 reads its
     * limit from 0x206fd878, 0 at first, which empties a queue below zero
     * and has it write before itself */
    w32(global + 0xb60, rnd() % 5);
    make_models();
    faulted = 0;
    put("scenario %u\ninit", scenario);
    call_service(0x04, 0);
    put("\n");
    /* The point table (0x206f931e, 16 bytes a point, eye-space x, y, z
     * as 16.16): its first 0x40 points seeded, mostly ahead of the eye and
     * within the view's cone, some outside it, a few behind, so the
     * facets that index it have something to draw. */
    for (k = 0; k < 0x40; k++) {
        uint8_t *pt = s->image + 0x31e + 16 * k;
        int32_t z = rnd() % 8 ? (int32_t)(0x10000 + rnd() % 0x4000000) : -(int32_t)(rnd() % 0x100000);
        int32_t span = z < 0 ? -z : z;

        w32(pt, (uint32_t)((int32_t)(rnd() % (2u * (uint32_t)span + 1)) - span) * (rnd() % 4 ? 1 : 2));
        w32(pt + 4, (uint32_t)((int32_t)(rnd() % (2u * (uint32_t)span + 1)) - span) * (rnd() % 4 ? 1 : 2));
        w32(pt + 8, (uint32_t)z);
        /* now and then 0x30000 or 0x40000 deep: three over four is the fast
         * routines' threshold (0x206f9178, 0.75) exactly */
        if (rnd() % 8 == 0) {
            z = rnd() % 2 ? 0x30000 : 0x40000;
            w32(pt, (uint32_t)((int32_t)(rnd() % 0x20001) - 0x10000));
            w32(pt + 4, (uint32_t)((int32_t)(rnd() % 0x20001) - 0x10000));
            w32(pt + 8, (uint32_t)z);
        }
        /* now and then on the cone: |x| = z one's complement, x = z or
         * -z - 1, where a point is just out of view */
        if (z > 0 && rnd() % 8 == 0)
            w32(pt, rnd() % 2 ? (uint32_t)z : (uint32_t)(-z - 1));
        w16(pt + 0xc, (uint16_t)(rnd() & 0x0f0f));
        w16(pt + 0xe, (uint16_t)rnd());
    }
    /* 0x30..0x33: two at 0x30000 deep, two at 0x40000, within the cone */
    for (k = 0x30; k < 0x34; k++) {
        uint8_t *pt = s->image + 0x31e + 16 * k;

        w32(pt, (uint32_t)((int32_t)(rnd() % 0x20001) - 0x10000));
        w32(pt + 4, (uint32_t)((int32_t)(rnd() % 0x20001) - 0x10000));
        w32(pt + 8, k < 0x32 ? 0x30000 : 0x40000);
    }
    /* 0x27's points: z below 0x10000 and y within 0..z, so every vertex,
     * crossing and clamp it draws leaves ecx's high half 0 */
    for (k = 0x38; k < 0x40; k++) {
        uint8_t *pt = s->image + 0x31e + 16 * k;
        int32_t z = (int32_t)(0x200 + rnd() % 0xfe00);

        w32(pt, (uint32_t)((int32_t)(rnd() % (4u * (uint32_t)z + 1)) - 2 * z));
        w32(pt + 4, 0x100 + rnd() % (uint32_t)(z - 0xff));
        w32(pt + 8, (uint32_t)z);
    }
    /* The positions' state: computed or not (0x206f917e, 0x206f9180), a
     * heading to turn by (0x206f9196, 0x206f9198), and 0x40's shadow mode
     * (0x206fd8dd, which 0x4a counts) */
    w16(s->image + 0x17e, rnd() % 4 ? 0 : 1);
    w16(s->image + 0x180, rnd() % 4 ? 0 : 1);
    w16(s->image + 0x196, rnd() % 2);
    w16(s->image + 0x198, rnd() % 8 ? (uint16_t)rnd() : 0);
    w16(s->image + 0x48dd, rnd() % 2);
    w16(s->image + 0x1bd, rnd() % 2);           /* the sun's matrix on (0x4a) */
    for (k = 0x4901; k < 0x490d; k++)           /* and its words, 0x41's */
        s->image[k] = (uint8_t)rnd();
    /* counters near their turns: 0x04's at 0x206fd6d0 (reloads at 0),
     * +0x60's at 0x206fd1fe (wraps at 0x12) */
    w32(s->image + 0x46d0, rnd() % 2 ? 1 + rnd() % 4 : rnd());
    w16(s->image + 0x41fe, rnd() % 2 ? (uint16_t)(0x0f + rnd() % 4) : (uint16_t)rnd());
    w16(s->image + 0x48be, rnd() % 4);          /* a record, not packed */
    w16(s->image + 0x17c, rnd() % 2);           /* the queue's one-shot words */
    seed_textures();                            /* named slots for 0x18 to find */
    /* In the game a model has always been drawn by the time the sky goes
     * through +0x1c, so 0x206f9161 points at 0x206f9166 as draw_model left
     * it; programs write through it.  Now and then it is still null. */
    if (rnd() % 16)
        w32(s->image + 0x161, (uint32_t)(uintptr_t)(s->image + 0x166));
    snapshot();
    nops = 4 + rnd() % MAX_OPS;
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        op();
    }
    if (!faulted) {
        put("deinit");
        call_service(0x08, 0);
        put("\n");
        snapshot();
    }
    cur = NULL;
}

/* A tagged value, R and eight hex digits, compares equal across the sides
 * when it is the same number, or when each points at the same offset in its
 * own side's image.  A number that merely falls inside one image does not
 * pass: the other side's would have to fall at the same offset in its own.
 * Returns the length that matches. */
static uint32_t matching_length(void)
{
    const char *a = retail.transcript, *b = ours.transcript;
    uint32_t n = retail.length < ours.length ? retail.length : ours.length, i = 0;

    while (i < n) {
        if (a[i] == b[i]) {
            if (a[i] == 'R' && i + 9 <= n) {
                char ra[9], rb[9];
                uint32_t va, vb;

                memcpy(ra, a + i + 1, 8);
                memcpy(rb, b + i + 1, 8);
                ra[8] = rb[8] = 0;
                va = (uint32_t)strtoul(ra, NULL, 16);
                vb = (uint32_t)strtoul(rb, NULL, 16);
                if (va != vb &&
                    !(as_offset(&retail, va) != va && as_offset(&ours, vb) != vb &&
                      as_offset(&retail, va) == as_offset(&ours, vb)))
                    return i;
                i += 9;
                continue;
            }
            i++;
            continue;
        }
        return i;
    }
    return retail.length == ours.length ? retail.length : n;
}

static void show_difference(void)
{
    const char *a = retail.transcript, *b = ours.transcript;
    uint32_t i, from;
    const char *ls;

    i = matching_length();
    ls = a + i;
    while (ls > a && ls[-1] != '\n')
        ls--;
    printf("scenario %u differs at byte %u\n", scenario, i);
    from = (uint32_t)(ls - a) > 1200 ? (uint32_t)(ls - a) - 1200 : 0;
    printf("--- before:\n%.*s\n", (int)((uint32_t)(ls - a) - from), a + from);
    printf("--- retail:\n%.700s\n", ls);
    printf("--- ours:\n%.700s\n", b + (ls - a));
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 20000;
    uint32_t first = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0;
    uint32_t i, differ = 0, c;

    AddVectoredExceptionHandler(1, on_vectored);
    arena = VirtualAlloc(NULL, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    programs = VirtualAlloc(NULL, PROGRAM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    context = VirtualAlloc(NULL, CONTEXT_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    build_stubs();
    load_util();
    load_window();
    load_side(&retail, "G3D_retail.dll", "retail");
    if ((uint32_t)(uintptr_t)retail.module != 0x206e0000u) {
        printf("retail G3D did not load at its base (%p)\n", (void *)retail.module);
        return 2;
    }
    load_side(&ours, "G3D_ours.dll", "ours");
    find_variables();
    arm_breaks();

    for (i = first; i < first + n; i++) {
        int bad;

        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        scenario_seed = rnd() | 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("G3D_DUMP") != NULL) {
            FILE *f = fopen("g3d_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("g3d_ours.txt", "wb");
            fwrite(ours.transcript, 1, ours.length, f);
            fclose(f);
        }
        if (getenv("G3D_SHOWAT")) {
            uint32_t at = (uint32_t)strtoul(getenv("G3D_SHOWAT"), NULL, 16), k, j;

            for (k = 0; k < retail.nsnap && k < ours.nsnap; k++) {
                printf("snap %2u retail", k);
                for (j = 0; j < 16; j++) printf(" %02x", retail.snap[k].image[at + j]);
                printf("\n        ours  ");
                for (j = 0; j < 16; j++) printf(" %02x", ours.snap[k].image[at + j]);
                printf("\n");
            }
        }
        bad = retail.length != ours.length || matching_length() != retail.length;
        if (bad) {
            if (differ < 3) {
                show_difference();
                /* state can part before anything shows in the calls */
                compare_snapshots(1);
            }
        } else if (compare_snapshots(differ < 3)) {
            bad = 1;
        }
        differ += bad;
        if ((i + 1 - first) % 5000 == 0) {
            printf("%u scenarios, %u differ\n", i + 1 - first, differ);
            fflush(stdout);
        }
    }
    printf("g3ddiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops (+3c +40 +44 +5c +64 textures programs): %lu %lu %lu %lu %lu %lu %lu\n",
           cover_ops[0], cover_ops[1], cover_ops[2], cover_ops[3], cover_ops[4],
           cover_ops[5] + cover_ops[6], cover_ops[7]);
    printf("  +3c by magnitude bits:");
    for (i = 0; i < 34; i++)
        if (cover_norm[i])
            printf(" %u:%lu", i, cover_norm[i]);
    printf("\n  +44 with the shift count masked %lu; divides repaired %lu; retail faults %lu\n",
           cover_shift_masked, cover_divfix, cover_faults);
    for (i = 0, c = 0; i < 0x94; i++)
        c += cover_opcodes[i] != 0;
    printf("  +64 opcodes covered: %u of 0x94\n", c);
    printf("  programs %lu; variables mapped %lu, unmapped %lu, in a context %lu, "
           "steered %lu; texture releases %lu\n  opcodes emitted:", cover_programs,
           cover_mapped, cover_unmapped, cover_context, cover_steered, cover_released);
    for (i = 0; i < 0x94; i++)
        if (cover_emitted[i])
            printf(" %02x:%lu", i, cover_emitted[i]);
    printf("\n  drawn: points %lu, lines %lu, polygons %lu, vertices %lu; shaded polygons %lu, "
           "vertices %lu; textured polygons %lu, vertices %lu; shaded textured polygons %lu, "
           "vertices %lu\n", cover_drawn[0],
           cover_drawn[1], cover_drawn[2], cover_drawn[3], cover_drawn[5], cover_drawn[4],
           cover_drawn[7], cover_drawn[6], cover_drawn[9], cover_drawn[8]);
    return differ ? 1 : 0;
}
