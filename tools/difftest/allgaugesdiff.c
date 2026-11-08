/* Every gauge callback, complete writable image, formatted output, bitmap
 * payload and PANELS transcript. Metadata comes from the checked PE survey;
 * the test loads separate retail and rebuilt modules and never executes a
 * retail byte through the replacement. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct function_spec { uint32_t rva, bytes, fp; int32_t pointer; uint32_t is_void, arg_pointer; };
struct module_spec {
    const char *name, *installed;
    uint32_t base, size, imports, linkage, header;
    const struct function_spec *functions; unsigned nfunctions;
    const uint32_t *relocs; unsigned nrelocs;
    const uint32_t *floats; unsigned nfloats;
    const uint32_t *prints; unsigned nprints;
    const uint32_t *bitmaps; unsigned nbitmaps;
    const uint32_t *indirect; unsigned nindirect;
    const uint32_t *ints; unsigned nints;      /* (offset, width) pairs: integers the code reads */
};
#include "all_gauges.inc"

struct side {
    HMODULE module;
    uint8_t *data, *pristine;
    uint32_t functions[64];
};
static struct side sides[2], *current;
static const struct module_spec *spec;
static unsigned current_function, current_case;
static uint32_t panel[0xac / 4];
static uint8_t argument[4096], text_buffers[64][512], scratch[64][512];
static uint64_t transcript;
static unsigned panel_calls;
static uint32_t random_state = 0x98371997;
static uint32_t word(const void *address) { uint32_t value; memcpy(&value, address, 4); return value; }
static void put_word(void *address, uint32_t value) { memcpy(address, &value, 4); }
static uint32_t random_word(void) { random_state ^= random_state << 13; random_state ^= random_state >> 17; random_state ^= random_state << 5; return random_state; }
static uint64_t mix(uint64_t hash, uint32_t value) { return (hash ^ value) * 0x100000001b3ull; }

static uint32_t normalize(const struct side *side, uint32_t value)
{
    unsigned index;
    uint32_t base = (uint32_t)(uintptr_t)side->data;
    if (value >= base && value < base + spec->size)
        return 0xe0000000u + value - base;
    for (index = 0; index < spec->nfunctions; index++)
        if (value && value == side->functions[index])
            return 0xf0000000u + spec->functions[index].rva;
    return value;
}

static uint32_t record(uint32_t slot, uint32_t a, uint32_t b, uint32_t c)
{
    transcript = mix(transcript, slot);
    transcript = mix(transcript, normalize(current, a));
    transcript = mix(transcript, normalize(current, b));
    transcript = mix(transcript, normalize(current, c));
    panel_calls++;
    return slot ^ 0x98370000u;
}
#define P1(name, slot) static uint32_t __stdcall name(uint32_t a) { return record(slot, a, 0, 0); }
#define P2(name, slot) static uint32_t __stdcall name(uint32_t a, uint32_t b) { return record(slot, a, b, 0); }
#define P3(name, slot) static uint32_t __stdcall name(uint32_t a, uint32_t b, uint32_t c) { return record(slot, a, b, c); }
P2(p40, 0x40) P1(p44, 0x44) P1(p48, 0x48) P2(p4c, 0x4c) P1(p50, 0x50) P1(p58, 0x58)
P1(p60, 0x60) P2(p64, 0x64) P1(p68, 0x68) P1(p6c, 0x6c) P2(p70, 0x70) P1(p74, 0x74)
P1(p7c, 0x7c) P3(p80, 0x80) P2(p84, 0x84) P2(p88, 0x88) P1(p8c, 0x8c) P2(p90, 0x90)
P1(p98, 0x98) P1(pa0, 0xa0)

static uint32_t call_function, call_args[3], call_bytes, call_fp, call_eax, call_saved_sp, call_bad_sp;
static uint16_t call_status;
static uint8_t call_float[10];
static DWORD WINAPI hang_watchdog(void *unused)
{
    (void)unused;
    Sleep(2000);
    fprintf(stderr, "HANG %s %s fn %#x\n", spec->name,
            current == &sides[0] ? "retail" : "ours", spec->functions[current_function].rva);
    ExitProcess(77);
    return 0;
}
static void invoke(uint32_t function, uint32_t bytes, uint32_t fp, uint64_t bits)
{
    call_function = function; call_bytes = bytes; call_fp = fp;
    call_args[0] = bytes == 8 && !spec->functions[current_function].arg_pointer ? (uint32_t)bits : (uint32_t)(uintptr_t)argument;
    call_args[1] = (uint32_t)(bits >> 32);
    call_args[2] = (uint32_t)bits ^ 0x9837;
    memset(call_float, 0, sizeof call_float);
    call_bad_sp = 0;
    __asm__ volatile(
        "pushl %%ebx; pushl %%esi; pushl %%edi; movl %%esp,%[saved]; movl %[fn],%%esi;"
        "cmpl $12,%[bytes]; jne 1f; pushl %[arg2];"
        "1: cmpl $8,%[bytes]; jb 2f; pushl %[arg1];"
        "2: cmpl $4,%[bytes]; jb 3f; pushl %[arg0];"
        "3: xorl %%eax,%%eax; fnclex; call *%%esi; movl %%eax,%[eax];"
        "cmpl $0,%[fp]; je 4f; fstpt %[value];"
        "4: fnstsw %[status]; cmpl %[saved],%%esp; je 5f; movl $1,%[bad]; movl %[saved],%%esp;"
        "5: popl %%edi; popl %%esi; popl %%ebx;"
        : [saved] "=m"(call_saved_sp), [eax] "=m"(call_eax), [status] "=m"(call_status),
          [value] "=m"(call_float), [bad] "+m"(call_bad_sp)
        : [fn] "m"(call_function), [bytes] "m"(call_bytes), [fp] "m"(call_fp),
          [arg0] "m"(call_args[0]), [arg1] "m"(call_args[1]), [arg2] "m"(call_args[2])
        : "eax", "ecx", "edx", "cc", "memory", "st");
}

static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "%s fn %#x case %u %s: fault %#lx at %p\n", spec ? spec->name : "load",
            spec ? spec->functions[current_function].rva : 0, current_case,
            current == &sides[0] ? "retail" : "ours", exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(2);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int load_side(struct side *side, const char *path, int retail)
{
    unsigned index;
    uint8_t *imports;
    side->module = LoadLibraryA(path);
    if (!side->module) { fprintf(stderr, "%s: cannot load %s (%lu)\n", spec->name, path, GetLastError()); return 0; }
    imports = (uint8_t *)GetProcAddress(side->module, "ImportTable");
    if (!imports) return 0;
    side->data = imports - spec->imports;
    if (GetProcAddress(side->module, "Linkage") != (FARPROC)(side->data + spec->linkage)) return 0;
    if (word(imports) != 15 || word(side->data + spec->linkage) != 19) return 0;
    side->pristine = malloc(spec->size);
    if (!side->pristine) return 0;
    memcpy(side->pristine, side->data, spec->size);
    for (index = 0; index < spec->nfunctions; index++) {
        const struct function_spec *function = &spec->functions[index];
        side->functions[index] = function->pointer < 0 ? 0 : word(side->data + function->pointer);
        if (retail && side->functions[index] && side->functions[index] !=
            (uint32_t)(uintptr_t)side->module + function->rva) return 0;
    }
    return 1;
}

static int compare_images(void)
{
    unsigned offset;
    for (offset = 0; offset + 4 <= spec->size; offset += 4) {
        uint32_t a = word(sides[0].data + offset), b = word(sides[1].data + offset);
        if (a != b && normalize(&sides[0], a) != normalize(&sides[1], b)) {
            fprintf(stderr, "%s fn %#x case %u data+%#x: %#x != %#x\n", spec->name,
                    spec->functions[current_function].rva, current_case, offset, a, b);
            return 0;
        }
    }
    return 1;
}

static int compare_resources(void)
{
    unsigned index, side;
    for (index = 0; index < spec->nbitmaps; index++) {
        const uint32_t *bitmap = spec->bitmaps + index * 3;
        const void *data[2];
        for (side = 0; side < 2; side++) {
            HRSRC resource = FindResourceExA(sides[side].module, RT_BITMAP,
                                             MAKEINTRESOURCEA(bitmap[0]), (WORD)bitmap[1]);
            if (!resource || SizeofResource(sides[side].module, resource) != bitmap[2]) return 0;
            data[side] = LockResource(LoadResource(sides[side].module, resource));
            if (!data[side]) return 0;
        }
        if (memcmp(data[0], data[1], bitmap[2])) return 0;
    }
    return 1;
}

static void setup(struct side *side, uint64_t bits)
{
    unsigned index;
    double number;
    memcpy(side->data, side->pristine, spec->size);
    put_word(side->data + spec->imports + 4, (uint32_t)(uintptr_t)panel);
    memset(argument, 0, 512);
    put_word(argument + 4, current_case % 1024);
    memcpy(argument + 0xc, &bits, 8);
    memcpy(&number, &bits, 8);
    /* Keep the persistent variable fixtures finite and bounded. Directed
     * values exercise each selector, zero gate and integer/text conversion. */
    number = (double)(int32_t)(current_case % 1024) - 100;
    for (index = 0; index < spec->nfloats; index++)
        memcpy(side->data + spec->floats[index], &number, 8);
    /* Every other case gives each slot its own value, so branches that need two slots to
     * differ, or one to equal one of the gauge's constants, are reached too: a different
     * integer, a half, slot 0's value again, or a finite constant from the gauge's .rdata
     * (taken from the retail side, so both sides get the same). */
    if (((uint32_t)(current_case + 7) * 2654435761u) >> 31) {
        const uint8_t *rdata = 0;
        uint32_t rdata_size = 0;
        {
            const uint8_t *base = (const uint8_t *)sides[0].module;
            const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
            const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
            unsigned at;
            for (at = 0; at < nt->FileHeader.NumberOfSections; at++)
                if (!memcmp(section[at].Name, ".rdata", 6)) { rdata = base + section[at].VirtualAddress; rdata_size = section[at].Misc.VirtualSize; }
        }
        for (index = 0; index < spec->nfloats; index++) {
            uint32_t mix = (uint32_t)(current_case + 1) * 2654435761u ^ (uint32_t)(index + 1) * 0x9e3779b9u;
            double value = (double)(int32_t)((mix >> 8) % 1024) - 100;
            mix ^= mix >> 15;
            if ((mix >> 20) % 4 == 0 && number == number && number >= -100000.0 && number <= 100000.0) {
                /* the variable's own value: states compared with the element's variable */
                memcpy(&value, &bits, 8);
                if (value >= -100000.0 && value <= 100000.0 && (value == 0.0 || fabs(value) >= 2.2250738585072014e-308)) {
                    memcpy(side->data + spec->floats[index], &value, 8);
                    continue;
                }
                value = (double)(int32_t)((mix >> 8) % 1024) - 100;
            }
            switch (mix % 6) {
            case 1: value += 0.5; break;
            case 4: value = value * 97.0 + 0.25; break;                  /* beyond the small range: caps */
            case 2: if (index) memcpy(&value, side->data + spec->floats[0], 8); break;
            case 3:
                if (rdata_size >= 8) {
                    double constant;
                    memcpy(&constant, rdata + ((mix >> 4) % (rdata_size / 8)) * 8, 8);
                    /* normal numbers only: .rdata's other bytes read as doubles can be denormal, and
                     * with a NaN on the stack a denormal memory operand raises nothing more where
                     * loading it first raises DE; a gauge's data holds no denormals in the game */
                    if (constant >= -100000.0 && constant <= 100000.0 && (constant == 0.0 || fabs(constant) >= 2.2250738585072014e-308)) value = constant;
                }
                break;
            case 5:                                                       /* next to a constant */
                if (rdata_size >= 8) {
                    double constant;
                    memcpy(&constant, rdata + ((mix >> 4) % (rdata_size / 8)) * 8, 8);
                    if (constant >= -100000.0 && constant <= 100000.0 && (constant == 0.0 || fabs(constant) >= 2.2250738585072014e-308))
                        value = constant + ((mix >> 9) & 1 ? 1.0 : -1.0);
                }
                break;
            }
            memcpy(side->data + spec->floats[index], &value, 8);
        }
    }
    /* The integers the code reads (states, flags, BCD frequencies) start where the image has
     * them in a quarter of the cases, otherwise 0 to 3, a 16-bit word or any word. */
    for (index = 0; index < spec->nints; index++) {
        uint32_t mix = (uint32_t)(current_case + 3) * 0x9e3779b9u ^ (uint32_t)(index + 1) * 2654435761u, value;
        uint32_t offset = spec->ints[2 * index], width = spec->ints[2 * index + 1];
        mix ^= mix >> 13; mix *= 0x85ebca6bu; mix ^= mix >> 16;
        switch (mix % 4) {
        case 0: continue;
        case 1: value = (mix >> 4) % 8; break;
        case 2: value = (mix >> 8) & 0xffff; break;
        default: value = mix * 0x27d4eb2du | 0x80000000u; break;   /* above any user address: a word
                                                                     * passed on is never normalised as one */
        }
        if (width == 8) side->data[offset] = (uint8_t)value;
        else if (width == 16) memcpy(side->data + offset, &value, 2);
        else memcpy(side->data + offset, &value, 4);
    }
    memset(text_buffers, 0, spec->nprints * sizeof text_buffers[0]);
    for (index = 0; index < spec->nprints; index++)
        put_word(side->data + spec->prints[index], (uint32_t)(uintptr_t)text_buffers[index]);
    memset(scratch, 0, spec->nindirect * sizeof scratch[0]);
    for (index = 0; index < spec->nindirect; index++) {
        put_word(side->data + spec->indirect[index], (uint32_t)(uintptr_t)scratch[index]);
        put_word(scratch[index] + 4, 320);
        put_word(scratch[index] + 8, 240);
        /* now and then another height, so a lever's arithmetic lands on its band's edges */
        if (((uint32_t)(current_case + 11) * 0x9e3779b9u) >> 30 == 0)
            put_word(scratch[index] + 8, 1 + ((uint32_t)(current_case + index) * 2654435761u >> 8) % 1000);
    }
    current = side;
    transcript = 0xcbf29ce484222325ull;
    panel_calls = 0;
}

static int test_module(unsigned count, const char *replacement)
{
    static const uint64_t edges[] = {
        0, 0x3ff0000000000000ull, 0xbff0000000000000ull, 0x4000000000000000ull,
        0x3fffffffffffffffull, 0x4000000000000001ull, 0x401fffffffffffffull,
        0x4020000000000000ull, 0x4020000000000001ull, 0x8000000000000000ull,
        1, 0x8000000000000001ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
        0x7ff8000000000000ull, 0xfff8000000000000ull, 0x7ff0000000000001ull,
        0x409f400000000000ull, 0xc09f400000000000ull, 0x4076800000000000ull,
        0x405e000000000000ull, 0x40c3880000000000ull, 0x40cfff8000000000ull   /* 16383, a gear light's code */
    };
    char path[512];
    unsigned index, vectors = 0, entries = 0, calls = 0;
    static const uint16_t precisions[] = {0, 0x200, 0x300};
    const unsigned edge_count = sizeof edges / sizeof edges[0];
    uint16_t saved, control;
    void *reservation = NULL;
    uint32_t preferred = 0x30000000u + (uint32_t)(spec - modules) * 0x200000u;
    snprintf(path, sizeof path, "orig/Gauges/%s.GAU", spec->name);
    if (!load_side(&sides[0], path, 1)) return 0;
    if (getenv("GAUGES_RELOCATE"))
        reservation = VirtualAlloc((void *)(uintptr_t)preferred, 0x10000, MEM_RESERVE, PAGE_NOACCESS);
    snprintf(path, sizeof path, "bin/gauges-all/Gauges/%s", spec->installed);
    if (!load_side(&sides[1], replacement ? replacement : path, 0)) return 0;
    if (getenv("GAUGES_RELOCATE") && (uint32_t)(uintptr_t)sides[1].module == preferred) {
        fprintf(stderr, "%s did not relocate\n", spec->name); return 0;
    }
    if (sides[0].module == sides[1].module || !compare_images() || !compare_resources()) {
        fprintf(stderr, "%s initial image/resource mismatch\n", spec->name); return 0;
    }
    __asm__ volatile("fnstcw %0" : "=m"(saved));
    control = (saved & ~0xf00u) | 0x200 | 0x3f;
    __asm__ volatile("fldcw %0" : : "m"(control));
    if (getenv("GAUGES_HANG_SIDE")) {
        uint64_t bits = strtoull(getenv("GAUGES_HANG_BITS"), NULL, 16);
        unsigned side = (unsigned)atoi(getenv("GAUGES_HANG_SIDE"));
        for (current_function = 0; current_function < spec->nfunctions; current_function++)
            if (spec->functions[current_function].rva == 0x1020) break;
        if (current_function == spec->nfunctions || side > 1) return 0;
        setup(&sides[side], bits);
        CreateThread(NULL, 0, hang_watchdog, NULL, 0, NULL);
        invoke(sides[side].functions[current_function], 8, 1, bits);
        fprintf(stderr, "RETURNED instead of hanging\n");
        ExitProcess(3);
    }
    const uint8_t *rdata = 0;
    uint32_t rdata_size = 0;
    {
        const uint8_t *image = (const uint8_t *)sides[0].module;
        const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(image + ((const IMAGE_DOS_HEADER *)image)->e_lfanew);
        const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
        unsigned at;
        for (at = 0; at < nt->FileHeader.NumberOfSections; at++)
            if (!memcmp(section[at].Name, ".rdata", 6)) { rdata = image + section[at].VirtualAddress; rdata_size = section[at].Misc.VirtualSize; }
    }
    for (current_function = 0; current_function < spec->nfunctions; current_function++) {
        const struct function_spec *function = &spec->functions[current_function];
        if (!sides[0].functions[current_function]) continue;
        entries++;
        for (current_case = 0; current_case < count + edge_count * 12; current_case++) {
            uint64_t bits = current_case < edge_count * 12 ? edges[current_case / 12] :
                ((uint64_t)random_word() << 32) | random_word();
            /* a quarter of the random cases take one of the gauge's normal .rdata constants, exactly
             * or half off, so the arguments reach the gauge's own thresholds and bands */
            if (current_case >= edge_count * 12 && (bits >> 62) == 0 && rdata_size >= 8) {
                double constant;
                memcpy(&constant, rdata + (uint32_t)(bits % (rdata_size / 8)) * 8, 8);
                if (constant >= -100000.0 && constant <= 100000.0 && (constant == 0.0 || fabs(constant) >= 2.2250738585072014e-308)) {
                    constant += (bits >> 40) % 3 == 0 ? 0.0 : (bits >> 40) % 3 == 1 ? 0.5 : -0.5;
                    memcpy(&bits, &constant, 8);
                }
            }
            control = (saved & ~0xf00u) | precisions[current_case % 3] | (((current_case / 3) % 4) << 10) | 0x3f;
            __asm__ volatile("fldcw %0" : : "m"(control));
            /* These two retail compass formatters wrap by subtraction. NaN,
             * infinity and huge non-progressing values are tested separately
             * as hangs, not allowed to stall the returning-vector suite. */
            if (function->rva == 0x1020 && (!strcmp(spec->name, "SCHCMP") || !strcmp(spec->name, "SOPCOMP"))) {
                double value;
                memcpy(&value, &bits, 8);
                if (!(value >= -100000.0 && value <= 100000.0)) {
                    value = (double)(int32_t)(bits % 144000) - 72000;
                    memcpy(&bits, &value, 8);
                }
            }
            uint32_t retail_eax, retail_bad, retail_calls;
            uint16_t retail_status;
            uint64_t retail_transcript;
            uint8_t retail_float[10];
            static uint8_t retail_text[sizeof text_buffers], retail_scratch[sizeof scratch], retail_argument[512];
            setup(&sides[0], bits);
            invoke(sides[0].functions[current_function], function->bytes, function->fp, bits);
            retail_eax = normalize(&sides[0], call_eax); retail_bad = call_bad_sp;
            retail_status = call_status & 0x3f; retail_transcript = transcript; retail_calls = panel_calls;
            memcpy(retail_float, call_float, 10);
            memcpy(retail_text, text_buffers, spec->nprints * sizeof text_buffers[0]);
            memcpy(retail_scratch, scratch, spec->nindirect * sizeof scratch[0]);
            memcpy(retail_argument, argument, sizeof retail_argument);
            setup(&sides[1], bits);
            invoke(sides[1].functions[current_function], function->bytes, function->fp, bits);
            if (retail_bad || call_bad_sp || (!function->fp && !function->is_void && retail_eax != normalize(&sides[1], call_eax)) ||
                retail_status != (call_status & 0x3f) ||
                (function->fp && memcmp(retail_float, call_float, 10)) ||
                retail_transcript != transcript || retail_calls != panel_calls ||
                memcmp(retail_text, text_buffers, spec->nprints * sizeof text_buffers[0]) ||
                memcmp(retail_scratch, scratch, spec->nindirect * sizeof scratch[0]) ||
                memcmp(retail_argument, argument, sizeof retail_argument) || !compare_images()) {
                fprintf(stderr, "%s fn %#x case %u mismatch: eax %x/%x fp %016llx:%04x/%016llx:%04x "
                        "status %x/%x calls %u/%u\n", spec->name, function->rva, current_case,
                        retail_eax, normalize(&sides[1], call_eax),
                        (unsigned long long)*(uint64_t *)retail_float, *(uint16_t *)(retail_float + 8),
                        (unsigned long long)*(uint64_t *)call_float, *(uint16_t *)(call_float + 8),
                        retail_status, call_status & 0x3f, retail_calls, panel_calls);
                return 0;
            }
            vectors++; calls += panel_calls;
        }
    }
    __asm__ volatile("fnclex; fldcw %0" : : "m"(saved));
    for (index = 0; index < 2; index++) {
        FreeLibrary(sides[index].module); free(sides[index].pristine); memset(&sides[index], 0, sizeof sides[index]);
    }
    if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    printf("%s: %u entries, %u vectors, %u PANELS calls, image/text/bitmaps/FP flags match\n",
           spec->name, entries, vectors, calls);
    fflush(stdout);
    return 1;
}

int main(int argc, char **argv)
{
    unsigned index, tested = 0, count = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : 100;
    const char *only = argc > 2 ? argv[2] : NULL;
    const char *replacement = argc > 3 ? argv[3] : NULL;
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    SetUnhandledExceptionFilter(fault);
#define SLOT(offset, function) panel[(offset) / 4] = (uint32_t)(uintptr_t)(function)
    SLOT(0x40,p40); SLOT(0x44,p44); SLOT(0x48,p48); SLOT(0x4c,p4c); SLOT(0x50,p50); SLOT(0x58,p58);
    SLOT(0x60,p60); SLOT(0x64,p64); SLOT(0x68,p68); SLOT(0x6c,p6c); SLOT(0x70,p70); SLOT(0x74,p74);
    SLOT(0x7c,p7c); SLOT(0x80,p80); SLOT(0x84,p84); SLOT(0x88,p88); SLOT(0x8c,p8c); SLOT(0x90,p90);
    SLOT(0x98,p98); SLOT(0xa0,pa0);
    for (index = 0; index < sizeof modules / sizeof modules[0]; index++) {
        spec = &modules[index];
        if (only && strcmp(only, spec->name)) continue;
        if (!test_module(count, replacement)) return 1;
        tested++;
    }
    printf("%u gauges checked, 0 differ\n", tested);
    return tested ? 0 : 2;
}
