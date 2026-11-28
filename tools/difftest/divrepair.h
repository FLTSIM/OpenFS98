/* The host's integer-divide repair for a whole-module harness (Story 1.10b1). A retail module divides without a
 * test and relies on the filter at 0x4119a0 (src/host/runtime/host_rt_divide.c: ours, verified against retail's by
 * dividediff). A harness registers each side's module, calls divrepair_handle first in its vectored handler, and
 * divrepair_report at exit.
 *
 *   - a divide fault at an instruction in a registered module's executable section is repaired by the filter and
 *     counted per VA (image base + RVA), outside the compared transcript;
 *   - a divide fault in an exempt range (DYNAMIC's CHAIN and UTIL stand-ins) is left to the harness's own handling;
 *   - OUR side: a fault is fatal (counted, left to the harness's handling, and the run fails), unless the
 *     environment names DIVREPAIR_OURS=repair, which repairs ours too: the "repaired old" control with the old
 *     asm modules, scoped to each module's section;
 *   - fatal, reported apart from any behavioural mismatch: a divide fault outside every registered section, a fault
 *     the filter does not repair, a repair the harness cannot make safely (the filter reads four bytes for a
 *     16-bit memory divisor, so a word at the end of a readable page would fault inside retail's own repair:
 *     the deferred 1.10a filter-read quirk, not reproduced; guarded here), and any divide fault on our side.
 * The caller links src/host/runtime/host_rt_divide.c (-Iinclude -Isrc/host/runtime). */
#ifndef DIVREPAIR_H
#define DIVREPAIR_H
#pragma GCC diagnostic ignored "-Wunused-function"
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t __attribute__((stdcall)) fs98_host_rt_divide_filter(void **pointers);

#define DIVREPAIR_SIDES 4
#define DIVREPAIR_VAS 128
struct divrepair_side {
    const char *name;
    int ours, exempt;
    uint8_t *lo, *hi, *module;
    uint32_t image_base;
    uint32_t nva, va[DIVREPAIR_VAS];
    unsigned long count[DIVREPAIR_VAS];
    unsigned long first[DIVREPAIR_VAS];     /* the harness's tag (scenario number) at each VA's first repair */
};
static struct divrepair_side divrepair_sides[DIVREPAIR_SIDES];
static int divrepair_nsides, divrepair_ours_repaired = -1;
static unsigned long divrepair_tag;         /* set by the harness: the scenario now running */
static uint32_t divrepair_last_va;          /* the VA of the latest retail repair */
static unsigned long divrepair_repairs[2];   /* retail, ours: every repair so far */
static unsigned long divrepair_outside, divrepair_unrepaired, divrepair_ours_faults, divrepair_guarded,
    divrepair_outside_va[4];

static int divrepair_ours_mode(void)
{
    if (divrepair_ours_repaired < 0) {
        const char *e = getenv("DIVREPAIR_OURS");
        divrepair_ours_repaired = e != NULL && strcmp(e, "repair") == 0;
    }
    return divrepair_ours_repaired;
}

/* a module's executable section: the first section flagged code */
static void divrepair_add(const char *name, int ours, uint8_t *module)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(module + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    struct divrepair_side *s = &divrepair_sides[divrepair_nsides++];
    unsigned i;

    memset(s, 0, sizeof *s);
    s->name = name;
    s->ours = ours;
    s->module = module;
    s->image_base = nt->OptionalHeader.ImageBase;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            s->lo = module + sec[i].VirtualAddress;
            s->hi = s->lo + (sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData);
            break;
        }
}

/* an address range whose divide faults the harness handles itself, as before */
__attribute__((unused)) static void divrepair_exempt(const char *name, uint8_t *lo, uint8_t *hi)
{
    struct divrepair_side *s = &divrepair_sides[divrepair_nsides++];

    memset(s, 0, sizeof *s);
    s->name = name;
    s->exempt = 1;
    s->lo = lo;
    s->hi = hi;
}

static int divrepair_readable(const uint8_t *p, unsigned n)
{
    MEMORY_BASIC_INFORMATION m;
    const uint8_t *end = p + n;

    while (p < end) {
        if (!VirtualQuery(p, &m, sizeof m) || m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return 0;
        p = (const uint8_t *)m.BaseAddress + m.RegionSize;
    }
    return 1;
}

/* the bytes the filter reads, in the stages it decodes them (host_rt_divide.c): the first byte (a 0x66 prefix or the opcode); then the opcode and
 * ModR/M after the prefix; then, by rm, the SIB and displacement bytes it fetches unconditionally (a dword at +2, or at +3 when rm is 4) */
static int divrepair_filter_bytes_ok(const uint8_t *ip)
{
    const uint8_t *c;

    if (!divrepair_readable(ip, 1))
        return 0;
    c = ip + (ip[0] == 0x66);
    if (!divrepair_readable(c, 2))
        return 0;
    return divrepair_readable(c, (c[1] & 7) == 4 ? 7 : 6);
}

/* the filter's four-byte read of a 16-bit (0x66) memory divisor, addressed by one register and a displacement */
static int divrepair_filter_read_ok(EXCEPTION_POINTERS *e)
{
    const uint8_t *p = (const uint8_t *)e->ContextRecord->Eip;
    uint8_t mod, rm;
    const DWORD *reg[8];
    CONTEXT *c = e->ContextRecord;
    uint32_t at;

    /* the divisor is read only on the signed path (idiv, /7): an unsigned divide never reads it (the host's quirk) */
    if (p[0] != 0x66 || p[1] != 0xf7 || ((p[2] >> 3) & 7) != 7 || (p[2] >> 6) == 3)
        return 1;
    mod = p[2] >> 6;
    rm = p[2] & 7;
    if (rm == 4)
        return 1;                       /* SIB: no such site in the four modules */
    reg[0] = &c->Eax; reg[1] = &c->Ecx; reg[2] = &c->Edx; reg[3] = &c->Ebx;
    reg[4] = &c->Esp; reg[5] = &c->Ebp; reg[6] = &c->Esi; reg[7] = &c->Edi;
    if (mod == 0 && rm == 5)
        at = *(const uint32_t *)(p + 3);
    else
        at = *reg[rm] + (mod == 1 ? p[3] : mod == 2 ? *(const uint32_t *)(p + 3) : 0);   /* disp8 unsigned, as the filter */
    return divrepair_readable((const uint8_t *)(uintptr_t)at, 4);
}

static void divrepair_count(struct divrepair_side *s, uint8_t *ip)
{
    uint32_t va = s->image_base + (uint32_t)(ip - s->module), i;

    divrepair_last_va = va;
    divrepair_repairs[s->ours]++;
    for (i = 0; i < s->nva; i++)
        if (s->va[i] == va) {
            s->count[i]++;
            return;
        }
    if (s->nva < DIVREPAIR_VAS) {
        s->va[s->nva] = va;
        s->first[s->nva] = divrepair_tag;
        s->count[s->nva++] = 1;
    }
}

/* 1: repaired, resume. 0: not repaired (not a divide fault, an exempt range, or a failure that is now recorded): the
 * harness goes on as it always did. */
static int divrepair_is_off(void)
{
    const char *e = getenv("DIVREPAIR");
    return e != NULL && strcmp(e, "off") == 0;     /* the raw control: today's handling */
}

static int divrepair_handle(EXCEPTION_POINTERS *e)
{
    static int off = -1;
    DWORD code = e->ExceptionRecord->ExceptionCode;
    uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
    struct divrepair_side *s = NULL;
    int i;

    if (off < 0)
        off = divrepair_is_off();
    if (off || (code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_INT_OVERFLOW))
        return 0;
    for (i = 0; i < divrepair_nsides; i++)
        if (ip >= divrepair_sides[i].lo && ip < divrepair_sides[i].hi) {
            s = &divrepair_sides[i];
            break;
        }
    if (s != NULL && s->exempt)
        return 0;
    if (s == NULL) {
        if (divrepair_outside < 4)
            divrepair_outside_va[divrepair_outside] = (uint32_t)(uintptr_t)ip;
        divrepair_outside++;
        return 0;
    }
    if (s->ours && !divrepair_ours_mode()) {
        divrepair_ours_faults++;
        return 0;
    }
    /* the filter reads the instruction's bytes, then (signed 16-bit memory forms only) the divisor */
    if (!divrepair_filter_bytes_ok(ip) || !divrepair_filter_read_ok(e)) {
        divrepair_guarded++;
        return 0;
    }
    if (fs98_host_rt_divide_filter((void **)e) != -1) {
        divrepair_unrepaired++;
        return 0;
    }
    divrepair_count(s, ip);
    return 1;
}

/* the retail side's per-VA counts, before a directed vector; divrepair_since prints the VAs it repaired */
struct divrepair_mark { uint32_t nva, va[DIVREPAIR_VAS]; unsigned long count[DIVREPAIR_VAS]; };
static struct divrepair_side *divrepair_retail_side(void)
{
    int i;

    for (i = 0; i < divrepair_nsides; i++)
        if (!divrepair_sides[i].ours && !divrepair_sides[i].exempt)
            return &divrepair_sides[i];
    return NULL;
}

static void divrepair_mark_now(struct divrepair_mark *m)
{
    struct divrepair_side *s = divrepair_retail_side();

    m->nva = s->nva;
    memcpy(m->va, s->va, sizeof m->va);
    memcpy(m->count, s->count, sizeof m->count);
}

/* "VA x n ..." for what the retail side repaired since the mark, "none" if nothing; returns the repairs */
static unsigned long divrepair_since(const struct divrepair_mark *m, char *out, size_t size)
{
    struct divrepair_side *s = divrepair_retail_side();
    uint32_t i, j;
    unsigned long total = 0;
    size_t n = 0;

    out[0] = 0;
    for (i = 0; i < s->nva; i++) {
        unsigned long was = 0, d;

        for (j = 0; j < m->nva; j++)
            if (m->va[j] == s->va[i])
                was = m->count[j];
        d = s->count[i] - was;
        if (d && n < size - 32)
            n += (size_t)snprintf(out + n, size - n, "%s%08x x%lu", n ? ", " : "", (unsigned)s->va[i], d);
        total += d;
    }
    if (!total)
        snprintf(out, size, "none");
    return total;
}

/* DIVREPAIR_SCENLOG=file: one line a scenario (fault and hang counts, whether state was compared, transcript length and crc),
 * so two controls over the same stream can be compared scenario by scenario */
static FILE *divrepair_scenlog;
static uint32_t divrepair_crc(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint32_t c = 0xffffffffu, i, b;

    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (b = 0; b < 8; b++)
            c = c & 1 ? (c >> 1) ^ 0xedb88320u : c >> 1;
    }
    return ~c;
}
static void divrepair_scenario(unsigned long number, unsigned long faults, unsigned long hangs, int compared, const void *transcript, size_t n)
{
    if (divrepair_scenlog == NULL) {
        const char *f = getenv("DIVREPAIR_SCENLOG");
        if (f == NULL)
            return;
        divrepair_scenlog = fopen(f, "w");
        if (divrepair_scenlog == NULL)
            return;
    }
    fprintf(divrepair_scenlog, "%lu fault=%lu hang=%lu compared=%d len=%lu crc=%08x\n", number, faults, hangs, compared, (unsigned long)n,
            (unsigned)divrepair_crc(transcript, n));
    fflush(divrepair_scenlog);
}

/* the exit status of a run whose repairs failed: apart from 1 (a behavioural difference), so a break whose run
 * only failed its repairs is never counted as caught */
#define DIVREPAIR_EXIT 5

/* prints the counters (outside any transcript); nonzero when the run must fail */
static int divrepair_report(void)
{
    int i, fail;
    uint32_t j;

    for (i = 0; i < divrepair_nsides; i++) {
        struct divrepair_side *s = &divrepair_sides[i];
        unsigned long total = 0;

        if (s->exempt)
            continue;
        for (j = 0; j < s->nva; j++)
            total += s->count[j];
        printf("divrepair: %s %s: %lu repairs", s->name, s->ours ? "ours" : "retail", total);
        for (j = 0; j < s->nva; j++)
            printf(" %08x:%lu@%lu", (unsigned)s->va[j], s->count[j], s->first[j]);
        printf("\n");
    }
    fail = divrepair_outside || divrepair_unrepaired || divrepair_ours_faults || divrepair_guarded;
    printf("divrepair: outside %lu, unrepaired %lu, ours faults %lu%s, filter-read guard %lu%s\n", divrepair_outside,
           divrepair_unrepaired, divrepair_ours_faults, divrepair_ours_mode() ? " (ours repaired: control)" : "",
           divrepair_guarded, fail ? "  DIVREPAIR FAIL" : "");
    fflush(stdout);
    return fail;
}
#endif
