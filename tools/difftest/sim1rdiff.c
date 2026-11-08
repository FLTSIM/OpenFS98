/* SIM1's routines one at a time: the retail code against the C of src/sim1/.
 *
 *   sim1rdiff.exe [scenarios] [seed] [first-op] [last-op]
 *
 * Retail orig/Sim/SIM1.sim is mapped at its base without resolving imports. Its MSVCRT imports
 * are the real ones; every module service it calls through its ImportTable (the links at .data
 * +0x4, +0xc, ...) goes to a generated stub: a push of the service's index and a jump to one
 * dispatcher, which logs the call (every argument the service pops), answers from the
 * scenario's script or models the service (HOST's memmove, allocator), and pops what the
 * retail service pops. Ours calls the same stubs.
 *
 * The fixture is a real in-flight state: one of the snapshots of SIM1's .data and GLOBAL that
 * FRAMELOCK took from retail flights (bin/sim1-diff/snapshots.bin, see docs/modules/sim1.md),
 * perturbed a little per scenario (a few doubles nudged, zeroed, negated or scaled, a few
 * flags and words changed), so the floating point runs on values it meets in flight and on
 * edges near them. Each operation runs twice from the same fixture, retail then ours, each
 * with the .data image swapped into the mapped module, on its own stack, with the same
 * registers and x87 state on entry (precision control varied). Compared after: .data,
 * GLOBAL, the heap and blocks, the call log, the registers the routine's retail callers read
 * (sim1_tables.inc), and the whole x87 stack (depth and every value; the condition codes
 * where callers read them). A fault is an outcome compared coarsely; a routine that
 * runs too long is stopped and not compared.
 *
 * Ours is an adapter per operation (sim1r_operations.inc): it takes the entry registers and
 * x87 inputs, calls the routine's C as its callers will, and sets what the C returns into the
 * registers retail leaves it in. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/sim1/sim1_internal.h"
#include "../../src/sim1/sim1_routines.h"

#define BASE 0x22000000u
#define TEXT_RVA 0x1000u
#define TEXT_END 0x14160u
#define DATA_RVA 0x16000u
#define DATA_SIZE 0x43f8u
#define LINK_END 0x94u          /* ImportTable and Linkage: never seeded */
#define GLOBAL_SIZE 0x12a8u
#define SNAPSHOT_SIZE (8 + DATA_SIZE + GLOBAL_SIZE)
#define LOG_SIZE 65536
#define BLOCKS 8
#define BLOCK_SIZE 0x800
#define HEAP_SIZE 0x4000
#define MAX_CALLS 4000
#define SERVICE_BASE 0x100u     /* a service stub's index: SERVICE_BASE + import * 256 + offset / 4 */
#define IMPORTS 10
#define GIVE_UP() do { fprintf(stderr, "GIVE UP line %d scenario=%u op=%u side=%d\n", __LINE__, s_scenario, s_operation, s_side); exit(2); } while (0)

struct routine_info { uint32_t rva; int32_t pop; uint32_t inputs, outputs, flags_out, cc_out; };
static const struct routine_info s_routines[] = {
#define SIM1_ROUTINE(rva, pop, inputs, outputs, flags_out, cc_out) {rva, pop, inputs, outputs, flags_out, cc_out},
#include "../../src/sim1/sim1_tables.inc"
};
struct service_info { uint32_t module_id, offset, pop, reads, flags; };
static const struct service_info s_services[] = {
#define SIM1_SERVICE(index, module_id, offset, pop, reads, flags) {module_id, offset, pop, reads, flags},
#include "../../src/sim1/sim1_tables.inc"
};
struct import_info { const char *dll, *name; uint32_t slot; };
static const struct import_info s_imports[] = {
#define SIM1_IMPORT(index, dll, name, slot) {dll, name, slot},
#include "../../src/sim1/sim1_tables.inc"
};
#define ROUTINE_COUNT (sizeof(s_routines) / sizeof(s_routines[0]))
#define SERVICE_COUNT (sizeof(s_services) / sizeof(s_services[0]))
#define IMPORT_COUNT (sizeof(s_imports) / sizeof(s_imports[0]))

uint8_t *sim1_data, *sim1_base;
void *sim1_crt_cipow, *sim1_crt_ciacos, *sim1_crt_ciasin;
uint32_t sim1_callbacks[3] = {BASE + 0x1020, BASE + 0x1040, BASE + 0x10da0};   /* retail's, so the logs agree */

/* the registers: in on entry, out after; esp is ignored on entry */
struct regs { uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi, eflags; };
struct fixture {
    uint8_t guard_before[64];
    uint8_t global[0x1400];
    uint8_t data[DATA_SIZE];
    uint8_t heap[HEAP_SIZE];
    uint8_t blocks[BLOCKS][BLOCK_SIZE];
    uint8_t guard_after[64];
};
static struct fixture *s_live, s_initial, s_retail_result;
#define s_fixture (*s_live)
#define FIXTURE_REGION 0x30000000u
#define FIXTURE_RESERVE 0x08000000u

/* x87 inputs and the state after: fnsave's 108 bytes */
struct x87 { uint8_t save[108]; };
struct side_result { struct regs regs; struct x87 x87; uint32_t esp_delta; };
static struct side_result s_results[2];

static uint8_t *s_snapshots;
static uint32_t s_snapshot_count;
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_calls, s_log_length, s_heap_used;
static uint32_t s_comparisons, s_faults, s_timeouts, s_max_calls, s_saved_seh, s_stack_low, s_stack_high;
static int s_side, s_running, s_retail_timed_out, s_ours_timed_out;
static uint32_t s_unresolved;
/* a scenario whose perturbation planted a signalling NaN: there, where that NaN meets another on
 * the stack, retail's `fmul QWORD PTR` style memory operand returns the stack's NaN while
 * GCC may have loaded the operand first (quieting it), and the larger significand wins. Memory
 * never holds a signalling NaN in the game (the x87 cannot store one), so in such a scenario a
 * difference confined to doubles that are quiet NaNs on both sides is counted, not failed. */
static uint32_t s_injected_snan, s_nan_collisions;
static char s_logs[2][LOG_SIZE];
static uint8_t *s_stubs;
static void *s_service_tables[IMPORTS][256];
uint32_t s_pop_now;
static uint32_t s_reloc_count;
static uint32_t s_relocs[2048];          /* relocated words in .data (rva) */
uint32_t s_harness_esp, s_operation_stack_top;
static uint8_t *s_operation_stack;
#define OPERATION_STACK 0x100000
static void *s_jump[5];
static volatile LONG s_watch_ticks;
static HANDLE s_main_thread;
/* read by the assembly below */
uint16_t s_control_word;
uint32_t s_x87_in_count;
long double s_x87_in[4];
#define MAX_ARGUMENTS 12
uint32_t s_arguments[MAX_ARGUMENTS], s_argument_count;

/* the operations */
enum { ARG_END, ARG_ANY, ARG_GLOBAL, ARG_DATA, ARG_BLOCK, ARG_SMALL, ARG_ANGLE, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_LIST };
struct operation {
    uint32_t rva; void (*ours)(struct regs *, const uint32_t *); const char *name;
    uint32_t x87_in;        /* values on the x87 stack on entry */
    uint32_t extra_outputs; /* registers compared beyond the callers' (mask as sim1_tables.inc) */
    uint32_t narrow;        /* of those, registers whose callers use only the low word */
    uint8_t edi_global;     /* edi holds GLOBAL on entry, as most callers load it */
    uint8_t esi_kind;       /* what esi holds on entry (ARG_ANY: anything) */
    uint8_t ebx_kind;       /* and ebx */
    uint8_t kinds[12];      /* stack arguments */
    void (*shape)(void);    /* steers the fixture and the entry onto the routine's edges */
};
#define OP(rva, name, x87_in, extra, edi_global, esi_kind, ...) {rva, name, #name, x87_in, extra, 0, edi_global, esi_kind, ARG_ANY, {__VA_ARGS__}, 0}
#define OPS(rva, name, shape, x87_in, extra, edi_global, esi_kind, ...) {rva, name, #name, x87_in, extra, 0, edi_global, esi_kind, ARG_ANY, {__VA_ARGS__}, shape}
#define OPN(rva, name, shape, x87_in, extra, narrow, edi_global, esi_kind, ...) {rva, name, #name, x87_in, extra, narrow, edi_global, esi_kind, ARG_ANY, {__VA_ARGS__}, shape}
#define OPB(rva, name, shape, x87_in, extra, edi_global, ebx_kind, ...) {rva, name, #name, x87_in, extra, 0, edi_global, ARG_ANY, ebx_kind, {__VA_ARGS__}, shape}
/* for the operations' shapers: the fixture's .data and GLOBAL, the entry state */
#define FD8(offset) (*(uint8_t *)(s_fixture.data + (offset)))
#define FD16(offset) (*(uint16_t *)(s_fixture.data + (offset)))
#define FD32(offset) (*(uint32_t *)(s_fixture.data + (offset)))
#define FD64(offset) (*(double *)(s_fixture.data + (offset)))
#define FG8(offset) (*(uint8_t *)(s_fixture.global + (offset)))
#define FG16(offset) (*(uint16_t *)(s_fixture.global + (offset)))
#define FG32(offset) (*(uint32_t *)(s_fixture.global + (offset)))
static uint32_t next_random(void);
static struct regs s_entry_regs;
static void *s_util[64];     /* (defined below) */
/* the AIR file the HOST file services stand for: a shaper's blob and its size (0: none) */
static const uint8_t *s_air_blob; static uint32_t s_air_size;
#include "sim1r_operations.inc"
#define OP_COUNT (sizeof(s_operations) / sizeof(s_operations[0]))
#define CURRENT s_operations[s_operation]

static uint32_t next_random(void)
{
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static void put(const char *format, ...)
{
    va_list arguments;
    int written;
    if (s_log_length >= LOG_SIZE - 256) return;
    va_start(arguments, format);
    written = vsnprintf(s_logs[s_side] + s_log_length, LOG_SIZE - s_log_length, format, arguments);
    va_end(arguments);
    if (written > 0) s_log_length += (uint32_t)written;
    if (s_log_length > LOG_SIZE - 256) s_log_length = LOG_SIZE - 256;
}
static int in_fixture(uint32_t value)
{
    return value >= (uint32_t)(uintptr_t)&s_fixture && value < (uint32_t)(uintptr_t)(&s_fixture + 1);
}
static int in_stack(uint32_t value) { return value >= s_stack_low && value < s_stack_high; }
static int in_image(uint32_t value) { return value >= BASE && value < BASE + 0x20000; }
static void describe(uint32_t value)
{
    if (in_fixture(value)) put(" F%x", value - (uint32_t)(uintptr_t)&s_fixture);
    else if (in_stack(value)) put(" L");
    else if (in_image(value)) put(" I%x", value - BASE);
    else put(" %x", value);
}
static uint32_t script(void)
{
    uint32_t value = s_seed * 0x9e3779b1u + s_calls * 0x85ebca6bu;
    value ^= value >> 15; value *= 0x2c1b3c6du; value ^= value >> 12;
    return value;
}
static int writable(uint32_t address, uint32_t size)
{
    uint32_t fixture = (uint32_t)(uintptr_t)&s_fixture, data = BASE + DATA_RVA;
    if (address >= fixture && address + size <= fixture + sizeof(struct fixture) && address + size >= address) return 1;
    if (address >= data && address + size <= data + DATA_SIZE && address + size >= address) return 1;
    return in_stack(address) && in_stack(address + size - 1);
}
uint32_t s_answer_ecx, s_answer_edx;
uint32_t s_push_st0; double s_st0_value;     /* a service's st0 result (UTIL's generator), loaded by the stub */
static void *heap_allocate(uint32_t size);
static void *s_util[64];     /* OpenFS98's UTIL's Linkage, for its pure arithmetic services */

/* the dispatcher every service stub jumps to: index, the caller's registers, its arguments */
uint64_t __cdecl sim1r_dispatch(uint32_t index, uint32_t ecx, uint32_t edx, uint32_t *arguments);
uint64_t __cdecl sim1r_dispatch(uint32_t index, uint32_t ecx, uint32_t edx, uint32_t *arguments)
{
    uint32_t import = (index - SERVICE_BASE) / 256, offset = (index - SERVICE_BASE) % 256 * 4, at, service;
    uint32_t module_id = *(uint32_t *)(uintptr_t)(BASE + DATA_RVA + import * 8), result;
    (void)ecx; (void)edx;
    if (++s_calls > MAX_CALLS) { put("too many calls\n"); GIVE_UP(); }
    for (service = 0; service < SERVICE_COUNT; ++service)
        if (s_services[service].module_id == module_id && s_services[service].offset == offset) break;
    if (service == SERVICE_COUNT) { put("unknown service %u+%x\n", module_id, offset); GIVE_UP(); }
    s_pop_now = s_services[service].pop;
    put("svc %u+%x", module_id, offset);
    for (at = 0; at < s_pop_now / 4; ++at) {
        /* UTIL's word arguments (sincos's angle, angle-from-sine's pair): callers push a word
         * over stack garbage, and UTIL reads the word */
        int word = (module_id == 20 && ((offset == 0x2c && at == 0) || offset == 0x38))
                   || (module_id == 10 && offset == 0x2c && at < 3)      /* WINDOW's matrix: three angle words */
                   || (module_id == 39 && offset == 0x28 && at > 0);     /* SOUND's wind: two words pushed over garbage (it ignores all three) */
        describe(word ? arguments[at] & 0xffff : arguments[at]);
    }
    s_answer_ecx = script() ^ 0x5a5a; s_answer_edx = script() ^ 0xa5a5;
    s_push_st0 = 0;
    if (module_id == 20 && offset == 0xb8) {     /* UTIL's generator: [0, 1) in st0, scripted */
        s_st0_value = (double)(script() >> 8) / 16777216.0;
        s_push_st0 = 1;
        put(" => %a\n", s_st0_value);
        return 0;
    }
    if (module_id == 1 && offset == 0x21c) {           /* memmove(dst, src, size), returns 0 */
        uint32_t size = arguments[2];
        if (size < 0x10000 && writable(arguments[0], size) && writable(arguments[1], size))
            memmove((void *)(uintptr_t)arguments[0], (const void *)(uintptr_t)arguments[1], size);
        else put(" (refused)");
        put(" => 0\n");
        return 0;
    }
    if (module_id == 1 && offset == 0x20c) {     /* HOST allocate(count, size, &pointer, flags): from the fixture's heap */
        void *block = writable(arguments[2], 4) ? heap_allocate(arguments[0] * arguments[1]) : 0;
        if (block) { memset(block, 0, arguments[0] * arguments[1]); memcpy((void *)(uintptr_t)arguments[2], &block, 4); }
        put(" => %s\n", block ? "0" : "-1");
        return block ? 0 : 0xffffffffu;
    }
    if (module_id == 1 && offset == 0x17c) {     /* HOST open(&file, path, mode, flags): a handle, or a failure */
        uint32_t answer = script(), handle = 0x4d1e;
        if (answer % 6 && writable(arguments[0], 4)) { memcpy((void *)(uintptr_t)arguments[0], &handle, 4); put(" => 0\n"); return 0; }
        put(" => 1\n");
        return 1;
    }
    if (module_id == 1 && offset == 0x1a4) {     /* HOST size(file, &size): the blob's */
        uint32_t answer = script();
        if (answer % 6 && writable(arguments[1], 4)) { memcpy((void *)(uintptr_t)arguments[1], &s_air_size, 4); put(" => 0 %x\n", s_air_size); return 0; }
        put(" => 1\n");
        return 1;
    }
    if (module_id == 1 && offset == 0x19c) {     /* HOST read(file, buffer, size, &got): the blob, whole or short */
        uint32_t answer = script(), size = arguments[2], got = size;
        if (answer % 8 == 0) got = size ? size - 1 : 0;
        if (answer % 6 && s_air_blob && size <= s_air_size && writable(arguments[1], size) && writable(arguments[3], 4)) {
            memcpy((void *)(uintptr_t)arguments[1], s_air_blob, got);
            memcpy((void *)(uintptr_t)arguments[3], &got, 4);
            put(" => 0 %x\n", got);
            return 0;
        }
        put(" => 1\n");
        return 1;
    }
    if (module_id == 1 && offset == 0x214) {     /* HOST free(&pointer): cleared */
        if (writable(arguments[0], 4)) memset((void *)(uintptr_t)arguments[0], 0, 4);
        put(" => 0\n");
        return 0;
    }
    if (module_id == 3 && offset == 0x60) {      /* MAIN's record write (data, size, written): the bytes */
        uint32_t at, hash = 2166136261u;
        if (arguments[1] <= 0x1000 && writable(arguments[0], arguments[1]))
            for (at = 0; at < arguments[1]; ++at) hash = (hash ^ ((uint8_t *)(uintptr_t)arguments[0])[at]) * 16777619u;
        put(" data %08x", hash);
    }
    if (module_id == 16 && offset == 0x5c && (arguments[0] == 1 || arguments[0] == 3)) {
        /* CONTROLS' force feedback (which, block): 1 answers three words (each 0 or not), 3
         * takes two doubles */
        uint32_t at;
        if (arguments[0] == 1 && writable(arguments[1], 12)) {
            for (at = 0; at < 3; ++at) { uint32_t word = script() % 5 ? 1 + (s_calls + at) % 3 : 0; ((uint32_t *)(uintptr_t)arguments[1])[at] = word; ++s_calls; }
            put(" => %x %x %x\n", ((uint32_t *)(uintptr_t)arguments[1])[0], ((uint32_t *)(uintptr_t)arguments[1])[1], ((uint32_t *)(uintptr_t)arguments[1])[2]);
            return 0;
        }
        if (arguments[0] == 3 && writable(arguments[1], 16)) {
            put(" =");
            for (at = 0; at < 16; ++at) put("%02x", ((uint8_t *)(uintptr_t)arguments[1])[at]);
            put("\n");
            return 0;
        }
        put(" (refused)\n");
        return 0;
    }
    if (module_id == 3 && offset == 0x6c) {      /* MAIN's record read (tag, &pointer, &size): a fixture block, or none */
        uint32_t answer = script();
        static const uint32_t sizes[4] = {0xc0, 0x58, 0xbf, 0x100};
        if (answer % 4 && writable(arguments[1], 4) && writable(arguments[2], 4)) {
            uint32_t pointer = (uint32_t)(uintptr_t)s_fixture.blocks[(answer >> 4) % BLOCKS] + (answer >> 8) % 0x40 * 4;
            memcpy((void *)(uintptr_t)arguments[1], &pointer, 4);
            memcpy((void *)(uintptr_t)arguments[2], &sizes[(answer >> 2) % 4], 4);
            put(" =>");
            describe(pointer);
            put(" %x\n", sizes[(answer >> 2) % 4]);
            return 0;
        }
        put(" => -1\n");
        return 0xffffffffu;
    }
    if (module_id == 3 && offset == 0x78) {      /* MAIN's nav read-out (which, &word): 1 and 2 write a word */
        if ((arguments[0] == 1 || arguments[0] == 2) && writable(arguments[1], 2)) {
            uint16_t word = script() % 2 ? (uint16_t)(script() >> 4) : *(uint16_t *)s_fixture.blocks[0];   /* or one a shaper set */
            memcpy((void *)(uintptr_t)arguments[1], &word, 2);
            put(" => %04x\n", word);
            return word;
        }
        put(" => -1\n");
        return 0xffffffffu;
    }
    if (module_id == 20 && s_util[offset / 4]) {     /* UTIL's pure arithmetic: OpenFS98's verified own */
        uint32_t size = offset == 0x48 ? 6 : 2;
        switch (offset) {
        case 0x2c:
            if (!writable(arguments[1], 2) || !writable(arguments[2], 2)) { put(" (refused)\n"); return 0; }
            ((void (__stdcall *)(int16_t, int16_t *, int16_t *))s_util[offset / 4])((int16_t)arguments[0],
                (int16_t *)(uintptr_t)arguments[1], (int16_t *)(uintptr_t)arguments[2]);
            put(" => %04x %04x\n", *(uint16_t *)(uintptr_t)arguments[1], *(uint16_t *)(uintptr_t)arguments[2]);
            return 0;
        case 0x38:
            result = (uint32_t)((int32_t (__stdcall *)(int16_t, int16_t))s_util[offset / 4])((int16_t)arguments[0], (int16_t)arguments[1]);
            break;
        case 0x3c:
            result = (uint32_t)((int32_t (__stdcall *)(int32_t, int32_t))s_util[offset / 4])((int32_t)arguments[0], (int32_t)arguments[1]);
            break;
        case 0x48:
            if (!writable(arguments[0], 6) || !writable(arguments[1], 0x18) || !writable(arguments[2], 8)) { put(" (refused)\n"); return 0; }
            ((void (__stdcall *)(const void *, const void *, void *))s_util[offset / 4])((const void *)(uintptr_t)arguments[0],
                (const void *)(uintptr_t)arguments[1], (void *)(uintptr_t)arguments[2]);
            put(" =>");     /* bytes 2..7: UTIL writes those only (and reads the input's 2..7), so 0 and 1 are the caller's stack */
            for (at = 2; at < 8; ++at) put(" %02x", ((uint8_t *)(uintptr_t)arguments[2])[at]);
            put("\n");
            (void)size;
            return 0;
        case 0x70:      /* the generator's sign, as the AIR decoder asks it: scripted */
            result = script();
            break;
        default:
            result = 0;
            break;
        }
        put(" => %x\n", result);
        return result;
    }
    result = script();
    switch (result % 8) {
    case 0: case 1: result = 0; break;
    case 2: result = 1; break;
    case 3: result &= 0xff; break;
    case 4: result &= 0xffff; break;
    default: break;
    }
    put(" -> %x\n", result);
    return result;
}

/* index, then the shared tail: call the dispatcher, then pop what the service pops; ecx/edx
 * come back as the script says, the rest as the caller left them */
__asm__(
    ".text\n"
    ".globl _sim1r_stub_common\n"
    "_sim1r_stub_common:\n"
    "    pushl %ebx\n"
    "    leal 12(%esp), %ebx\n"           /* the arguments, past the index and the return */
    "    pushl %ebx\n"
    "    pushl %edx\n"
    "    pushl %ecx\n"
    "    pushl 16(%esp)\n"                /* the index */
    "    call _sim1r_dispatch\n"
    "    addl $16, %esp\n"
    "    popl %ebx\n"
    "    cmpl $0, _s_push_st0\n"
    "    je 1f\n"
    "    fldl _s_st0_value\n"
    "1:\n"
    "    movl _s_answer_ecx, %ecx\n"
    "    movl _s_answer_edx, %edx\n"
    "    pushl %esi\n"
    "    movl _s_pop_now, %esi\n"
    "    pushl %eax\n"
    "    movl 12(%esp), %eax\n"           /* the return address, moved up over the arguments */
    "    movl %eax, 12(%esp,%esi)\n"
    "    popl %eax\n"
    "    leal 8(%esp,%esi), %esi\n"       /* where esp ends: past index, arguments, at the return */
    "    xchgl %esi, (%esp)\n"
    "    popl %esp\n"
    "    ret\n"
    /* invoke(entry, regs in, regs out, x87 out): on the operations' own stack, below a gap a
     * retail overrun can write into; the x87 is set from s_control_word and s_x87_in */
    ".globl _sim1r_invoke\n"
    "_sim1r_invoke:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    movl %esp, _s_harness_esp\n"
    "    movl _s_operation_stack_top, %esp\n"
    "    movl %esp, %edi\n"
    "    subl $0x10000, %edi\n"
    "    movl $0x4000, %ecx\n"
    "    xorl %eax, %eax\n"
    "    cld\n"
    "    rep stosl\n"
    "    movl _s_argument_count, %ecx\n"
    "1:  testl %ecx, %ecx\n"
    "    jz 2f\n"
    "    pushl _s_arguments-4(,%ecx,4)\n"
    "    decl %ecx\n"
    "    jmp 1b\n"
    "2:  fninit\n"
    "    fldcw _s_control_word\n"
    "    movl _s_x87_in_count, %ecx\n"
    "3:  testl %ecx, %ecx\n"
    "    jz 4f\n"
    "    decl %ecx\n"
    "    imull $12, %ecx, %eax\n"
    "    fldt _s_x87_in(%eax)\n"
    "    jmp 3b\n"
    "4:  movl 8(%ebp), %eax\n"
    "    movl %eax, _s_entry\n"
    "    movl 12(%ebp), %eax\n"
    "    movl 4(%eax), %ecx\n"
    "    movl 8(%eax), %edx\n"
    "    movl 12(%eax), %ebx\n"
    "    movl 24(%eax), %esi\n"
    "    movl 28(%eax), %edi\n"
    "    pushl 20(%eax)\n"
    "    movl (%eax), %eax\n"
    "    popl %ebp\n"
    "    movl %esp, _s_entry_esp\n"
    "    call *_s_entry\n"
    "    pushfl\n"
    "    popl _s_exit_flags\n"
    "    movl %esp, _s_exit_esp\n"
    "    pushl %eax\n"
    "    movl _s_out_regs, %eax\n"
    "    movl %ecx, 4(%eax)\n"
    "    movl %edx, 8(%eax)\n"
    "    movl %ebx, 12(%eax)\n"
    "    movl %ebp, 20(%eax)\n"
    "    movl %esi, 24(%eax)\n"
    "    movl %edi, 28(%eax)\n"
    "    popl %ecx\n"
    "    movl %ecx, (%eax)\n"
    "    movl _s_exit_flags, %ecx\n"
    "    movl %ecx, 32(%eax)\n"
    "    movl _s_out_x87, %eax\n"
    "    fnsave (%eax)\n"
    "    movl _s_harness_esp, %esp\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    popl %ebx\n"
    "    popl %ebp\n"
    "    ret\n"
    /* ours: the adapter with the registers as they came, then the registers it set */
    ".globl _sim1r_ours_entry\n"
    "_sim1r_ours_entry:\n"
    "    pushl %eax\n"
    "    movl _s_ours_regs, %eax\n"
    "    pushfl\n"
    "    popl 32(%eax)\n"
    "    movl %ecx, 4(%eax)\n"
    "    movl %edx, 8(%eax)\n"
    "    movl %ebx, 12(%eax)\n"
    "    movl %ebp, 20(%eax)\n"
    "    movl %esi, 24(%eax)\n"
    "    movl %edi, 28(%eax)\n"
    "    popl %ecx\n"
    "    movl %ecx, (%eax)\n"
    "    leal 4(%esp), %ecx\n"            /* the stack arguments */
    "    pushl %ecx\n"
    "    pushl %eax\n"
    "    call *_s_ours_adapter\n"
    "    addl $8, %esp\n"
    "    movl _s_ours_regs, %eax\n"
    "    movl 4(%eax), %ecx\n"
    "    movl 8(%eax), %edx\n"
    "    movl 12(%eax), %ebx\n"
    "    movl 20(%eax), %ebp\n"
    "    movl 24(%eax), %esi\n"
    "    movl 28(%eax), %edi\n"
    "    movl (%eax), %eax\n"
    "    pushl %eax\n"                    /* pop what retail pops */
    "    movl _s_ours_pop, %eax\n"
    "    pushl %ecx\n"
    "    movl 8(%esp), %ecx\n"
    "    movl %ecx, 8(%esp,%eax)\n"
    "    movl _s_ours_regs, %ecx\n"
    "    pushl 32(%ecx)\n"
    "    popfl\n"                         /* the flags the adapter says, untouched from here */
    "    popl %ecx\n"
    "    popl %eax\n"
    "    pushl %ecx\n"
    "    movl _s_ours_pop, %ecx\n"
    "    leal 4(%esp,%ecx), %ecx\n"
    "    xchgl %ecx, (%esp)\n"
    "    popl %esp\n"
    "    ret\n");
void sim1r_stub_common(void);
void sim1r_invoke(void *entry, const struct regs *in);
void sim1r_ours_entry(void);
void *s_entry, *s_ours_adapter;
uint32_t s_entry_esp, s_exit_esp, s_exit_flags, s_ours_pop;
struct regs *s_out_regs, *s_ours_regs;
struct x87 *s_out_x87;
static struct regs s_ours_scratch;

static void *make_stub(uint32_t index)
{
    static uint32_t used;
    uint8_t *stub = s_stubs + used;
    int32_t relative;
    used += 10;
    stub[0] = 0x68; memcpy(stub + 1, &index, 4);
    relative = (int32_t)((uint8_t *)sim1r_stub_common - (stub + 10));
    stub[5] = 0xe9; memcpy(stub + 6, &relative, 4);
    return stub;
}
static const struct routine_info *routine(uint32_t rva)
{
    uint32_t at;
    for (at = 0; at < ROUTINE_COUNT; ++at) if (s_routines[at].rva == rva) return &s_routines[at];
    return 0;
}
static int is_reloc(uint32_t rva)
{
    uint32_t low = 0, high = s_reloc_count;
    while (low < high) {
        uint32_t middle = (low + high) / 2;
        if (s_relocs[middle] == rva) return 1;
        if (s_relocs[middle] < rva) low = middle + 1; else high = middle;
    }
    return 0;
}
/* a double nudged near itself or to an edge */
static void perturb_double(uint8_t *at)
{
    double value;
    uint32_t random = next_random();
    memcpy(&value, at, 8);
    if (random % 23 == 0) {     /* now and then a special value */
        static const uint64_t specials[6] = {0x7ff0000000000001ull, 0xfff8000000000000ull, 0x7ff0000000000000ull,
                                             0x0000000000000001ull, 0x8000000000000000ull, 0x000fffffffffffffull};
        memcpy(at, &specials[(random >> 8) % 6], 8);
        if ((random >> 8) % 6 == 0) ++s_injected_snan;
        return;
    }
    switch (random % 10) {
    case 0: value = 0; break;
    case 1: value = -value; break;
    case 2: value *= 2; break;
    case 3: value *= 0.5; break;
    case 4: { uint64_t bits; memcpy(&bits, &value, 8); bits ^= (uint64_t)1 << (random >> 8) % 20; memcpy(&value, &bits, 8); break; }
    case 5: value += 1; break;
    case 6: value = (double)(int32_t)(random >> 4) / 65536.0; break;
    default: { uint64_t bits; memcpy(&bits, &value, 8); bits += (random >> 8) % 5 - 2; memcpy(&value, &bits, 8); break; }
    }
    memcpy(at, &value, 8);
}
static uint32_t pick(uint32_t kind, uint32_t random);
/* the entry state, the same both sides: registers, x87, stack arguments (chosen with the
 * fixture, so an operation's shaper can steer them onto an edge with it) */
static void choose_entry(void)
{
    struct regs in;
    uint32_t at;
    s_rng = s_seed ^ 0x5bd1e995u ^ s_operation;
    in.eax = pick(ARG_ANY, next_random()); in.ecx = pick(ARG_ANY, next_random());
    in.edx = pick(ARG_ANY, next_random()); in.ebx = pick(CURRENT.ebx_kind, next_random());
    in.ebp = pick(ARG_ANY, next_random()); in.esi = pick(CURRENT.esi_kind, next_random());
    in.edi = CURRENT.edi_global && next_random() % 8 ? (uint32_t)(uintptr_t)s_fixture.global : pick(ARG_ANY, next_random());
    in.esp = 0;
    { static const uint16_t words[4] = {0x027f, 0x027f, 0x037f, 0x007f}; s_control_word = words[next_random() % 4]; }
    s_x87_in_count = CURRENT.x87_in;
    for (at = 0; at < s_x87_in_count; ++at) {
        double value;
        memcpy(&value, s_fixture.data + 0x1000 + (next_random() % 0x3000 & ~7u), 8);
        if (next_random() % 6 == 0) value = (double)(int32_t)next_random() / 1024.0;
        s_x87_in[at] = value;
    }
    for (s_argument_count = 0; s_argument_count < MAX_ARGUMENTS && CURRENT.kinds[s_argument_count]; ++s_argument_count)
        if (CURRENT.kinds[s_argument_count] == ARG_DOUBLE) {    /* a double from the flight, or near one */
            uint8_t bits[8];
            memcpy(bits, s_fixture.data + 0x1000 + (next_random() % 0x3000 & ~7u), 8);
            if (next_random() % 4 == 0) perturb_double(bits);
            memcpy(&s_arguments[s_argument_count], bits, 4);
            if (s_argument_count + 1 < MAX_ARGUMENTS) memcpy(&s_arguments[s_argument_count + 1], bits + 4, 4);
        } else if (CURRENT.kinds[s_argument_count] != ARG_DOUBLE_HIGH)
            s_arguments[s_argument_count] = pick(CURRENT.kinds[s_argument_count], next_random());
    s_entry_regs = in;
}
static void *heap_allocate(uint32_t size)
{
    uint32_t at = (s_heap_used + 15) & ~15u;
    if (!size || size > HEAP_SIZE || at + size > HEAP_SIZE) return 0;
    s_heap_used = at + size;
    return s_fixture.heap + at;
}
static void initialize(void)
{
    uint32_t at, count, snapshot;
    const uint8_t *record;
    memset(&s_fixture, 0, sizeof(s_fixture));
    s_rng = s_seed ^ (s_operation * 0x9e3779b9u);
    if (!s_rng) s_rng = 1;
    snapshot = next_random() % s_snapshot_count;
    record = s_snapshots + snapshot * SNAPSHOT_SIZE;
    memcpy(s_fixture.data, record + 8, DATA_SIZE);
    memcpy(s_fixture.global, record + 8 + DATA_SIZE, GLOBAL_SIZE);
    /* nudge a few values: doubles at 8-byte alignment, words and bytes elsewhere */
    s_injected_snan = 0;
    count = next_random() % 12;
    for (at = 0; at < count; ++at) {
        uint32_t random = next_random(), where;
        int in_global = random % 3 == 0;
        uint8_t *base = in_global ? s_fixture.global : s_fixture.data;
        uint32_t size = in_global ? GLOBAL_SIZE : DATA_SIZE;
        where = (random >> 4) % size & ~7u;
        if (!in_global && (where < LINK_END + 8 || is_reloc(DATA_RVA + where) || is_reloc(DATA_RVA + where + 4))) continue;
        switch ((random >> 2) % 4) {
        case 0: case 1: perturb_double(base + where); break;
        case 2: base[where + (random >> 24) % 8] ^= (uint8_t)(1u << (random >> 20) % 8); break;
        default: base[where + (random >> 24) % 8] = (uint8_t)(random >> 16) % 3; break;
        }
    }
    /* one scenario in four, many words scribbled over too (fields a flight leaves zero) */
    if (next_random() % 4 == 0) {
        count = 32 + next_random() % 256;
        for (at = 0; at < count; ++at) {
            uint32_t random = next_random(), where = (LINK_END + 8 + (random >> 4) % (DATA_SIZE - LINK_END - 16)) & ~3u;
            if (is_reloc(DATA_RVA + where)) continue;
            if (random % 3 == 0) { uint32_t word = next_random(); memcpy(s_fixture.data + where, &word, 4); }
            else if (random % 3 == 1) perturb_double(s_fixture.data + (where & ~7u));
            else { double value = (double)(int32_t)next_random() / 4096.0; memcpy(s_fixture.data + (where & ~7u), &value, 8); }
        }
    }
    /* the links: HOST and the modules to the stubs, GLOBAL to the fixture's */
    for (at = 0; at < IMPORTS; ++at) {
        uint32_t link = (uint32_t)(uintptr_t)s_service_tables[at];
        memcpy(s_fixture.data + at * 8 + 4, &link, 4);
    }
    { uint32_t global = (uint32_t)(uintptr_t)s_fixture.global; memcpy(s_fixture.data + 0xc, &global, 4); }
    for (at = 0; at < HEAP_SIZE; ++at) s_fixture.heap[at] = (uint8_t)(0xa5 ^ at ^ (at >> 7));
    for (at = 0; at < BLOCKS * BLOCK_SIZE; at += 8)
        memcpy(&s_fixture.blocks[0][0] + at, record + 8 + 0x1000 + (at % 0x3000), 8);   /* plausible numbers */
    choose_entry();
    if (CURRENT.shape) CURRENT.shape();
    s_initial = s_fixture;
}
static uint32_t pick(uint32_t kind, uint32_t random)
{
    switch (kind) {
    case ARG_GLOBAL: return (uint32_t)(uintptr_t)s_fixture.global;
    case ARG_DATA: return BASE + DATA_RVA + LINK_END + (random % (DATA_SIZE - LINK_END - 0x100) & ~7u);
    case ARG_BLOCK: return (uint32_t)(uintptr_t)s_fixture.blocks[random % BLOCKS] + (random >> 8) % 0x100 * 4;
    case ARG_SMALL: return random % 8;
    case ARG_LIST: {    /* 8-byte records {kind, value, weight} to a 0: kinds 1, -1, 2 or any */
        uint8_t *list = s_fixture.blocks[random % BLOCKS];
        uint32_t count = (random >> 4) % 8, at;
        static const int32_t kinds[6] = {1, 1, 1, -1, 2, 7};
        for (at = 0; at < count; ++at) {
            uint32_t value = next_random();
            int32_t kind = kinds[value % 6];
            memcpy(list + at * 8, &kind, 4);
            if (value % 9 == 0) value = 0x7fff7fffu;
            memcpy(list + at * 8 + 4, &value, 4);
        }
        memset(list + at * 8, 0, 4);
        return (uint32_t)(uintptr_t)list;
    }
    case ARG_ANGLE: return random & 0xffff;
    default:
        switch (random % 8) {
        case 0: return 0;
        case 1: return 1;
        case 2: return (random >> 8) & 0xff;
        case 3: return (random >> 8) & 0xffff;
        case 4: return (uint32_t)(uintptr_t)s_fixture.global;
        case 5: return (uint32_t)(uintptr_t)s_fixture.blocks[(random >> 8) % BLOCKS];
        default: return random | 0x80000000u;   /* wild: never the harness's own memory, which neither side resets */
        }
    }
}
static LONG WINAPI fatal(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT scenario=%u op=%u (%s) side=%d code=%lx address=%p\n", s_scenario,
        s_operation, CURRENT.name, s_side, exception->ExceptionRecord->ExceptionCode,
        exception->ExceptionRecord->ExceptionAddress);
    fflush(stderr); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static void land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_jump, 1);
}
/* the host's divide repair (0x4119a0) for retail's div and idiv, as in the game: F7 /6 or /7
 * (0x66 for 16 bits), a register or a ds:disp32 divisor */
static int repair_divide(CONTEXT *context)
{
    const uint8_t *code = (const uint8_t *)(uintptr_t)context->Eip;
    uint32_t length = 0, wide = 1, modrm, divisor, *registers[8];
    registers[0] = (uint32_t *)&context->Eax; registers[1] = (uint32_t *)&context->Ecx;
    registers[2] = (uint32_t *)&context->Edx; registers[3] = (uint32_t *)&context->Ebx;
    registers[4] = (uint32_t *)&context->Esp; registers[5] = (uint32_t *)&context->Ebp;
    registers[6] = (uint32_t *)&context->Esi; registers[7] = (uint32_t *)&context->Edi;
    if (code[0] == 0x66) { wide = 0; ++length; }
    if (code[length] != 0xf7) return 0;
    modrm = code[length + 1];
    if ((modrm >> 6) == 3) { divisor = *registers[modrm & 7]; length += 2; }
    else if ((modrm & 0xc7) == 5) { divisor = *(uint32_t *)(uintptr_t)*(uint32_t *)(code + length + 2); length += 6; }
    else return 0;
    if (((modrm >> 3) & 7) == 6) {          /* div: all ones */
        if (wide) { context->Eax = 0xffffffffu; context->Edx = 0xffffffffu; }
        else { context->Eax |= 0xffff; context->Edx |= 0xffff; }
    } else if (((modrm >> 3) & 7) == 7) {   /* idiv */
        if (wide) {
            context->Eax = ((int32_t)divisor < 0) != ((int32_t)context->Edx < 0) ? 0x80000001u : 0x7fffffffu;
            context->Edx = 0x7fffffffu;
        } else {
            context->Eax = (context->Eax & 0xffff0000u) |
                           (((int16_t)divisor < 0) != ((int16_t)context->Edx < 0) ? 0x8001u : 0x7fffu);
            context->Edx = (context->Edx & 0xffff0000u) | 0x7fff;
        }
    } else return 0;
    context->Eip += length;
    return 1;
}
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    uint32_t address;
    if (s_running && !s_side && (record->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO ||
                                 record->ExceptionCode == EXCEPTION_INT_OVERFLOW) &&
        repair_divide(exception->ContextRecord))
        return EXCEPTION_CONTINUE_EXECUTION;
    if (!s_running || (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
                       record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
                       record->ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO &&
                       record->ExceptionCode != EXCEPTION_INT_OVERFLOW &&
                       record->ExceptionCode != EXCEPTION_FLT_INVALID_OPERATION &&
                       record->ExceptionCode != EXCEPTION_FLT_STACK_CHECK))
        return EXCEPTION_CONTINUE_SEARCH;
    address = record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? (uint32_t)record->ExceptionInformation[1] : 0;
    /* coarsely: which byte of a bad pointer is touched first, and whether a read-modify-write
     * instruction faults as a read or a write, are the compiler's */
    put("fault code=%lx address=%s\n", record->ExceptionCode,
        address < 0x10000 ? "low" : in_fixture(address) ? "fixture" : "wild");
    if (getenv("SIM1R_EIP"))     /* where faults are (ebx and [esp+0x10]: a table lookup's table and caller) */
        fprintf(stderr, "eip %08lx address %08x edi %08lx ebx %08lx esp+10 %08x\n", exception->ContextRecord->Eip, address,
                exception->ContextRecord->Edi, exception->ContextRecord->Ebx, ((uint32_t *)(uintptr_t)exception->ContextRecord->Esp)[4]);
    if (!s_side) ++s_faults;
    s_running = 0;
    exception->ContextRecord->Eip = (DWORD)(uintptr_t)land;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static void timed_out(void)
{
    if (!s_side) s_retail_timed_out = 1;
    else s_ours_timed_out = 1;
    put("timeout\n");
    if (!s_side) ++s_timeouts;
    s_running = 0;
    land();
}
static DWORD WINAPI watchdog(void *unused)
{
    (void)unused;
    for (;;) {
        Sleep(250);
        if (!s_running) { s_watch_ticks = 0; continue; }
        if (++s_watch_ticks < 8) continue;
        SuspendThread(s_main_thread);
        if (s_running) {
            CONTEXT context;
            context.ContextFlags = CONTEXT_CONTROL;
            GetThreadContext(s_main_thread, &context);
            context.Eip = (DWORD)(uintptr_t)timed_out;
            context.Esp = (context.Esp - 64) & ~15u;
            SetThreadContext(s_main_thread, &context);
        }
        s_watch_ticks = 0;
        ResumeThread(s_main_thread);
    }
    return 0;
}
static __attribute__((noinline)) void run(void)
{
    const struct routine_info *info = routine(CURRENT.rva);
    struct regs in;
    uint32_t saved_rng = s_rng;
    s_fixture = s_initial;
    memcpy((uint8_t *)(uintptr_t)(BASE + DATA_RVA), s_fixture.data, DATA_SIZE);
    in = s_entry_regs;
    s_rng = saved_rng;
    s_log_length = s_calls = s_heap_used = 0; s_logs[s_side][0] = 0;
    s_stack_low = (uint32_t)(uintptr_t)s_operation_stack;
    s_stack_high = s_stack_low + OPERATION_STACK;
    memset(&s_results[s_side], 0, sizeof(s_results[s_side]));
    s_out_regs = &s_results[s_side].regs;
    s_out_x87 = &s_results[s_side].x87;
    s_ours_regs = &s_ours_scratch;
    s_ours_adapter = (void *)CURRENT.ours;
    s_ours_pop = info ? (uint32_t)info->pop : 0;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        s_running = 1;
        sim1r_invoke(s_side ? (void *)sim1r_ours_entry : (void *)(uintptr_t)(BASE + CURRENT.rva), &in);
        s_running = 0;
        s_results[s_side].esp_delta = s_exit_esp - s_entry_esp;
        put("returned esp+%x\n", s_results[s_side].esp_delta);
    }
    s_running = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __asm__ volatile("fninit");
    memcpy(s_fixture.data, (uint8_t *)(uintptr_t)(BASE + DATA_RVA), DATA_SIZE);
    if (s_calls > s_max_calls) s_max_calls = s_calls;
}
/* the x87 after: TOP, condition codes and exception flags (not precision's), each register
 * that is not empty, by its stack position */
static void x87_log(const struct x87 *state, int condition_codes)
{
    uint16_t status, tags, control;
    uint32_t top, slot;
    memcpy(&control, state->save, 2);
    memcpy(&status, state->save + 4, 2);
    memcpy(&tags, state->save + 8, 2);
    top = (status >> 11) & 7;
    /* The condition codes only where a caller reads them (cc_out). The sticky exception flags
     * (bits 0-6) are not compared: every exception is masked, SIM1's status-word tests look
     * only at C0 and C3 (ah), and the C compiler is free to load an operand early (a denormal
     * raises DE) or compare with fcomi. A NaN made where retail makes none still shows in the
     * values. */
    put("x87 control %04x status %04x", control, status & (condition_codes ? 0x4700 : 0));
    for (slot = 0; slot < 8; ++slot) {
        uint32_t physical = (top + slot) & 7;
        if (((tags >> (physical * 2)) & 3) == 3) continue;
        put(" st%u=", slot);
        { uint32_t at; for (at = 10; at-- > 0; ) put("%02x", state->save[28 + slot * 10 + at]); }
    }
    put("\n");
}
static void result_log(void)
{
    const struct routine_info *info = routine(CURRENT.rva);
    uint32_t outputs = (info ? info->outputs : 0xed) | CURRENT.extra_outputs;
    const uint32_t *values = (const uint32_t *)&s_results[s_side].regs;
    static const char *const names[8] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    uint32_t at;
    if (strstr(s_logs[s_side], "fault ") || strstr(s_logs[s_side], "timeout")) return;
    for (at = 0; at < 8; ++at)
        if (at != 4 && outputs & (1u << at)) {
            put("%s", names[at]);
            if (CURRENT.narrow & (1u << at)) put(" low %04x", values[at] & 0xffff);   /* callers use the low word */
            else describe(values[at]);
            put("\n");
        }
    /* narrow bit 8: the callers test only ZF (a jne after the call) */
    if (info && info->flags_out) put("flags %03x\n", s_results[s_side].regs.eflags & (CURRENT.narrow & 0x100 ? 0x40 : 0x8d5));
    x87_log(&s_results[s_side].x87, info ? (int)info->cc_out : 1);
}
static void show(void);
/* every differing byte inside a double that is a quiet NaN on both sides */
static int quiet_nans_only(void)
{
    const uint8_t *retail = (const uint8_t *)&s_retail_result, *ours = (const uint8_t *)&s_fixture;
    uint32_t at, size = sizeof(s_fixture);
    for (at = 0; at < size; ++at) {
        uint32_t window;
        int covered = 0;
        if (retail[at] == ours[at]) continue;
        for (window = at >= 7 ? at - 7 : 0; window <= at && window + 8 <= size && !covered; ++window) {
            uint64_t a, b;
            memcpy(&a, retail + window, 8); memcpy(&b, ours + window, 8);
            covered = ((a >> 51) & 0xfff) == 0xfff && ((b >> 51) & 0xfff) == 0xfff;
        }
        if (!covered) return 0;
    }
    return 1;
}
static int compare(void)
{
    uint32_t at;
    DWORD started = GetTickCount(), retail_took;
    s_side = 0; s_retail_timed_out = 0; run(); result_log(); s_retail_result = s_fixture;
    retail_took = GetTickCount() - started;
    if (s_retail_timed_out) { ++s_unresolved; return 0; }
    s_side = 1; s_ours_timed_out = 0; run(); result_log();
    if (s_ours_timed_out && retail_took > 300) { ++s_unresolved; return 0; }
    ++s_comparisons;
    show();
    if (!memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    if (s_injected_snan && !strcmp(s_logs[0], s_logs[1]) && quiet_nans_only()) { ++s_nan_collisions; return 0; }
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s, %#x) seed=%x\n", s_scenario, s_operation, CURRENT.name, CURRENT.rva, s_seed);
    for (at = 0; at < sizeof(s_fixture); ++at)
        if (((uint8_t *)&s_retail_result)[at] != ((uint8_t *)&s_fixture)[at]) {
            uint32_t data = (uint32_t)offsetof(struct fixture, data), global = (uint32_t)offsetof(struct fixture, global);
            fprintf(stderr, "byte %s+%x retail=%02x ours=%02x initial=%02x\n",
                at >= data && at < data + DATA_SIZE ? "data" : at >= global && at < global + GLOBAL_SIZE ? "global" : "fixture",
                at >= data && at < data + DATA_SIZE ? at - data : at >= global && at < global + GLOBAL_SIZE ? at - global : at,
                ((uint8_t *)&s_retail_result)[at], ((uint8_t *)&s_fixture)[at], ((uint8_t *)&s_initial)[at]);
            if (!getenv("SIM1R_ALL")) break;
        }
    fprintf(stderr, "RETAIL:\n%.4000s\nOURS:\n%.4000s\n", s_logs[0], s_logs[1]);
    return 1;
}
static void show(void)
{   /* SIM1R_SHOW=n: retail's log of the first n comparisons */
    static uint32_t shown;
    const char *want = getenv("SIM1R_SHOW");
    if (want && shown < strtoul(want, 0, 0)) { ++shown; fprintf(stderr, "--- %s scenario %u:\n%s", CURRENT.name, s_scenario, s_logs[0]); }
}
static void load_snapshots(void)
{
    FILE *file = fopen("bin/sim1-diff/snapshots.bin", "rb");
    long size;
    if (!file) { fprintf(stderr, "no bin/sim1-diff/snapshots.bin (see docs/modules/sim1.md)\n"); exit(2); }
    fseek(file, 0, SEEK_END); size = ftell(file); fseek(file, 0, SEEK_SET);
    s_snapshot_count = (uint32_t)(size / SNAPSHOT_SIZE);
    s_snapshots = malloc((size_t)size);
    if (!s_snapshot_count || !s_snapshots || fread(s_snapshots, 1, (size_t)size, file) != (size_t)size) GIVE_UP();
    fclose(file);
}
static void setup(void)
{
    uint32_t index, at;
    uint8_t *image;
    HMODULE retail = LoadLibraryExA("orig/Sim/SIM1.sim", 0, DONT_RESOLVE_DLL_REFERENCES);
    HMODULE msvcrt = LoadLibraryA("msvcrt.dll");
    if ((uint32_t)(uintptr_t)retail != BASE) {
        fprintf(stderr, "orig/Sim/SIM1.sim not mapped at its base: %p, error %lu\n", (void *)retail, GetLastError());
        exit(2);
    }
    image = (uint8_t *)retail;
    sim1_data = image + DATA_RVA;
    sim1_base = image;
    {   /* .data's relocated words, from the base relocations */
        IMAGE_NT_HEADERS *headers = (IMAGE_NT_HEADERS *)(image + ((IMAGE_DOS_HEADER *)image)->e_lfanew);
        IMAGE_DATA_DIRECTORY *directory = &headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        uint8_t *block = image + directory->VirtualAddress, *end = block + directory->Size;
        while (block < end) {
            IMAGE_BASE_RELOCATION *relocation = (IMAGE_BASE_RELOCATION *)block;
            uint16_t *entries = (uint16_t *)(relocation + 1);
            uint32_t count = (relocation->SizeOfBlock - sizeof(*relocation)) / 2;
            if (!relocation->SizeOfBlock) break;
            for (at = 0; at < count; ++at) {
                uint32_t rva = relocation->VirtualAddress + (entries[at] & 0xfff);
                if ((entries[at] >> 12) == 3 && rva >= DATA_RVA && rva < DATA_RVA + DATA_SIZE && s_reloc_count < 2048)
                    s_relocs[s_reloc_count++] = rva;
            }
            block += relocation->SizeOfBlock;
        }
    }
    for (index = 0; index < IMPORT_COUNT; ++index) {
        void **slot = (void **)(uintptr_t)(BASE + s_imports[index].slot);
        void *real = strcmp(s_imports[index].dll, "MSVCRT.dll") ? GetProcAddress(GetModuleHandleA("kernel32.dll"), s_imports[index].name)
                                                                : GetProcAddress(msvcrt, s_imports[index].name);
        DWORD old;
        if (!real) GIVE_UP();
        if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) GIVE_UP();
        *slot = real;
        VirtualProtect(slot, 4, old, &old);
    }
    { DWORD old; VirtualProtect(image + DATA_RVA, DATA_SIZE, PAGE_READWRITE, &old); }
    sim1_crt_cipow = (void *)GetProcAddress(msvcrt, "_CIpow");      /* the same ones retail's imports are */
    sim1_crt_ciacos = (void *)GetProcAddress(msvcrt, "_CIacos");
    sim1_crt_ciasin = (void *)GetProcAddress(msvcrt, "_CIasin");
    if (!VirtualAlloc((void *)(uintptr_t)FIXTURE_REGION, FIXTURE_RESERVE, MEM_RESERVE, PAGE_NOACCESS)) GIVE_UP();
    s_live = VirtualAlloc((void *)(uintptr_t)(FIXTURE_REGION + FIXTURE_RESERVE / 2), sizeof(struct fixture),
                          MEM_COMMIT, PAGE_READWRITE);
    if (!s_live) GIVE_UP();
    s_operation_stack = VirtualAlloc(0, OPERATION_STACK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!s_operation_stack) GIVE_UP();
    s_operation_stack_top = (uint32_t)(uintptr_t)s_operation_stack + OPERATION_STACK - 0x20000;
    s_stubs = VirtualAlloc(0, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_stubs) GIVE_UP();
    for (index = 0; index < IMPORTS; ++index)
        for (at = 0; at < 256; ++at) s_service_tables[index][at] = make_stub(SERVICE_BASE + index * 256 + at);
    for (at = 0; at < OP_COUNT; ++at)
        if (!routine(s_operations[at].rva)) { fprintf(stderr, "op %s: %#x is no routine\n", s_operations[at].name, s_operations[at].rva); exit(2); }
    load_snapshots();
    {   /* OpenFS98's UTIL (bin/UTIL.DLL, verified against retail by utildiff) for sincos, angle
         * from sine, atan2 and longitude to metres: the real arithmetic, the same both sides */
        HMODULE util = LoadLibraryA("bin/UTIL.DLL");
        const uint32_t *linkage = util ? (const uint32_t *)(void *)GetProcAddress(util, "Linkage") : 0;
        static const uint32_t pure[4] = {0x2c, 0x38, 0x3c, 0x48};
        if (!linkage) { fprintf(stderr, "no bin/UTIL.DLL (make)\n"); exit(2); }
        for (at = 0; at < 4; ++at) s_util[pure[at] / 4] = (void *)(uintptr_t)linkage[pure[at] / 4];
    }
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], 0, 0) : 2000;
    uint32_t initial_seed = argc > 2 ? strtoul(argv[2], 0, 0) : 0x51e1u;
    uint32_t first = argc > 3 ? strtoul(argv[3], 0, 0) : 0;
    uint32_t last = argc > 4 ? strtoul(argv[4], 0, 0) : (uint32_t)OP_COUNT - 1;
    uint32_t rng;
    if (!count || !initial_seed) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal);
    AddVectoredExceptionHandler(1, expected_fault);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    CreateThread(0, 0, watchdog, 0, 0, 0);
    setup();
    if (last >= OP_COUNT) last = (uint32_t)OP_COUNT - 1;
    rng = initial_seed;
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        s_seed = rng;
        for (s_operation = first; s_operation <= last; ++s_operation) {
            initialize();
            if (getenv("SIM1R_TRACE")) { fprintf(stderr, "scenario %u op %u %s\n", s_scenario, s_operation, CURRENT.name); fflush(stderr); }
            if (compare()) return 1;
        }
    }
    printf("SIM1: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x snapshots=%u faults=%u timeouts=%u "
           "(retail's, not compared) max_calls=%u nan_collisions=%u\n",
        count, last - first + 1, s_comparisons, initial_seed, s_snapshot_count, s_faults, s_unresolved, s_max_calls, s_nan_collisions);
    return 0;
}
