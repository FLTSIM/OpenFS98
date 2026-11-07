/* SIMSUI's routines, the original PE32 code against C (src/simsui/).
 *
 *   simsuidiff.exe [scenarios] [seed] [first-op] [last-op]
 *
 * Retail SIMSUI.DLL is mapped at its base without resolving its imports, and every import
 * slot is pointed at a generated stub: a push of the import's index and a jump to one
 * dispatcher, which logs the call (ECX for a member, every argument the import pops or
 * its callers remove), answers from the scenario's script and pops what the import pops.
 * Ours calls the same stubs through fs98_simsui_imports, and reads retail's data
 * (vtables, maps, strings, statics) in the same mapped image (fs98_simsui_image).
 *
 * Each operation runs twice from one randomised fixture, retail then ours: the object,
 * blocks of memory every pointer in the fixture leads to, a heap, and SIMSUI's .data.
 * The call logs, the return values and every fixture byte must agree. Values are logged
 * by what they are: a fixture address by its offset, a local by "L", code by "C",
 * retail's image by its RVA, anything else by value. An object's vtable is a fake one
 * built from the class's real slots: each slot a stub that pops what the real routine
 * pops, so a virtual call is logged rather than run. A fault is an outcome to compare;
 * a routine that runs too long is stopped and reported as such. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/simsui/simsui_internal.h"

#define BASE 0x20470000u
#define IMAGE_SIZE 0x2c000u
#define TEXT_RVA 0x1000u
#define TEXT_END 0x147eau
#define DATA_RVA 0x20000u
#define DATA_SIZE 0x13f8u
#define LOG_SIZE 65536
#define BLOCKS 32
#define BLOCK_SIZE 0x400
#define HEAP_SIZE 0x10000
#define MAX_CALLS 4000
#define VIRTUAL_BASE 0x1000u        /* a virtual stub's index: VIRTUAL_BASE + vtable * 128 + slot */
#define HOST_BASE 0x800u            /* a HOST service's: HOST_BASE + offset / 4 */
#define HOST_SLOTS 0x100
#define GIVE_UP() do { fprintf(stderr, "GIVE UP line %d scenario=%u op=%u side=%d\n", __LINE__, s_scenario, s_operation, s_side); exit(2); } while (0)

struct import_info { const char *dll, *name; uint32_t slot, pop, thiscall, arguments; };
static const struct import_info s_imports[] = {
#define SIMSUI_IMPORT(index, ident, dll, name, slot, pop, thiscall, arguments) {dll, name, slot, pop, thiscall, arguments},
#include "../../src/simsui/simsui_tables.inc"
#undef SIMSUI_IMPORT
};
struct function_info { uint32_t rva, pop, convention; const char *name; };
static const struct function_info s_functions[] = {
#define SIMSUI_FUNCTION(rva, pop, convention, name) {rva, pop, convention, name},
#include "../../src/simsui/simsui_tables.inc"
#undef SIMSUI_FUNCTION
};
struct vtable_info { uint32_t rva, slots; const char *name; };
static const struct vtable_info s_vtables[] = {
#define SIMSUI_VTABLE(rva, slots, name) {rva, slots, name},
#include "../../src/simsui/simsui_tables.inc"
#undef SIMSUI_VTABLE
};
#define IMPORT_COUNT (sizeof(s_imports) / sizeof(s_imports[0]))
#define FUNCTION_COUNT (sizeof(s_functions) / sizeof(s_functions[0]))
#define VTABLE_COUNT (sizeof(s_vtables) / sizeof(s_vtables[0]))
#define GENERIC_VTABLE VTABLE_COUNT           /* the blocks' own: CSimsStatic's, a plain CWnd */
#define DC_VTABLE (VTABLE_COUNT + 1)          /* a CDC's */

void *fs98_simsui_imports[SIMSUI_IMPORT_COUNT];
uint8_t *fs98_simsui_image;

struct fixture {
    uint8_t guard_before[32];
    uint8_t before[16];             /* an array's count lives before its first element */
    uint8_t object[0x2000];
    uint8_t blocks[BLOCKS][BLOCK_SIZE];
    uint8_t heap[HEAP_SIZE];
    uint8_t dibs[4][0x70];          /* CDIBSections: small, their bits in pixels */
    uint8_t pixels[4][0x800];
    int32_t ints[12];               /* what int & arguments refer to, overlapping */
    uint8_t resource[0x1400];       /* a bitmap resource LockResource gives */
    uint8_t data[DATA_SIZE];        /* SIMSUI's .data, in the image while a side runs */
    uint8_t guard_after[32];
};
/* the live fixture sits alone in a reserved region: a read run past it faults, rather than
 * landing in the harness's own memory, which differs between the sides */
static struct fixture *s_live, s_initial, s_retail_result;
#define s_fixture (*s_live)
#define FIXTURE_REGION 0x30000000u
#define FIXTURE_RESERVE 0x08000000u
static uint8_t s_loaded_data[DATA_SIZE];
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_calls, s_log_length, s_heap_used;
static uint32_t s_comparisons, s_faults, s_timeouts, s_max_calls, s_saved_seh, s_stack_low, s_stack_high;
static int s_side, s_running, s_retail_timed_out;
static uint32_t s_unresolved;
static char s_logs[2][LOG_SIZE];
static uint8_t *s_stubs;
static void **s_fake_vtables[VTABLE_COUNT + 2];
static uint8_t s_virtual_pop[VTABLE_COUNT + 2][128];
static uint32_t s_pop_now;
uint32_t s_harness_esp, s_operation_stack_top;
static uint8_t *s_operation_stack;
#define OPERATION_STACK 0x100000
static void *s_host_table[HOST_SLOTS];
static void *s_jump[5];
static volatile LONG s_watch_ticks;
static HANDLE s_main_thread;

/* the operations, and how each is called */
enum { ARG_UNSET, ARG_ANY, ARG_SMALL, ARG_FLAGS, ARG_DC, ARG_POINTER, ARG_TEXT, ARG_SEPARATOR, ARG_CODE, ARG_INFO, ARG_DIB, ARG_INTS, ARG_KEY };
struct operation {
    uint32_t rva; void *ours; const char *name;
    uint32_t arguments; uint8_t kinds[16];
    uint32_t returns;   /* RET_VALUE, RET_VOID */
};
enum { RET_VALUE, RET_VOID };
#define OP(rva, name, ...) {rva, (void *)fs98_simsui_##name, #name, ~0u, {__VA_ARGS__}, RET_VALUE}
#define OPV(rva, name, ...) {rva, (void *)fs98_simsui_##name, #name, ~0u, {__VA_ARGS__}, RET_VOID}
static struct operation s_operations[] = {
#include "simsui_operations.inc"
};
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
static int in_image(uint32_t value) { return value >= BASE && value < BASE + IMAGE_SIZE; }
static uint32_t s_our_code, s_our_code_end;
static int in_code(uint32_t value)
{
    return (value >= BASE + TEXT_RVA && value < BASE + TEXT_END) || (value >= s_our_code && value < s_our_code_end) ||
           (s_stubs && value >= (uint32_t)(uintptr_t)s_stubs && value < (uint32_t)(uintptr_t)s_stubs + 0x10000);
}
/* a value as both sides can agree on it */
static void describe(uint32_t value)
{
    if (in_fixture(value)) put(" F%x", value - (uint32_t)(uintptr_t)&s_fixture);
    else if (in_stack(value)) put(" L");
    else if (in_code(value)) put(" C");
    else if (in_image(value)) put(" I%x", value - BASE);
    else put(" %x", value);
}
static uint32_t fixture_address(const void *pointer) { return (uint32_t)(uintptr_t)pointer; }

/* the script: what the next call answers */
static uint32_t script(void)
{
    uint32_t value = s_seed * 0x9e3779b1u + s_calls * 0x85ebca6bu;
    value ^= value >> 15; value *= 0x2c1b3c6du; value ^= value >> 12;
    return value;
}
static uint32_t block_pointer(uint32_t random)
{
    return fixture_address(s_fixture.blocks[random % BLOCKS]);
}
static uint32_t answer(void)
{
    uint32_t value = script();
    switch (value % 8) {
    case 0: case 1: return 0;
    case 2: return 1;
    case 3: return (value >> 8) & 0xff;
    case 4: return (value >> 8) & 0xffff;
    default: return block_pointer(value >> 8);
    }
}
static uint32_t mixed(uint32_t random)
{
    switch (random % 8) {
    case 0: case 1: return 0;
    case 2: return 1;
    case 3: return (random >> 8) & 0xff;
    case 4: return (random >> 8) & 0xffff;
    default: return block_pointer(random >> 8);
    }
}
static void *heap_allocate(uint32_t size)
{
    uint32_t at = (s_heap_used + 15) & ~15u;
    if (size > HEAP_SIZE || at + size > HEAP_SIZE) return 0;
    s_heap_used = at + size;
    return s_fixture.heap + at;
}

#include "simsui_semantics.inc"
static uint32_t class_vtable(uint32_t rva);
static void fill(uint8_t *memory, uint32_t size);
#include "simsui_fixture.inc"

/* the dispatcher every stub jumps to: index, the caller's ECX, its arguments */
uint64_t __cdecl simsui_dispatch(uint32_t index, uint32_t ecx, uint32_t *arguments);
uint64_t __cdecl simsui_dispatch(uint32_t index, uint32_t ecx, uint32_t *arguments)
{
    uint32_t count, at, result;
    if (++s_calls > MAX_CALLS) { put("too many calls\n"); GIVE_UP(); }
    if (index >= HOST_BASE && index < VIRTUAL_BASE) {
        uint32_t offset = (index - HOST_BASE) * 4;
        uint64_t special = 0;
        s_pop_now = host_pop(offset);
        put("host %x", offset);
        for (at = 0; at < s_pop_now / 4; ++at) describe(arguments[at]);
        if (host_semantics(offset, arguments, &special)) { put(" =>"); describe((uint32_t)special); put("\n"); return special; }
        result = answer();
        put(" -> %x\n", result);
        return result;
    }
    if (index >= VIRTUAL_BASE) {
        uint32_t vtable = (index - VIRTUAL_BASE) / 128, slot = (index - VIRTUAL_BASE) % 128;
        s_pop_now = s_virtual_pop[vtable][slot];
        put("virtual %u.%x this", vtable, slot * 4); describe(ecx);
        for (at = 0; at < s_pop_now / 4; ++at) describe(arguments[at]);
        result = answer();
        put(" -> %x\n", result);
        return result;
    }
    s_pop_now = s_imports[index].pop;
    count = import_arguments(index);
    put("%s", s_imports[index].name);
    if (s_imports[index].thiscall) { put(" this"); describe(ecx); }
    for (at = 0; at < count; ++at) describe(arguments[at]);
    {
        uint64_t special;
        if (semantics(index, ecx, arguments, &special)) {
            put(" =>"); describe((uint32_t)special); put("\n");
            return special;
        }
    }
    result = answer();
    put(" -> %x\n", result);
    return result;
}

/* index, then the shared tail: call the dispatcher, then pop what the callee pops */
__asm__(
    ".text\n"
    ".globl _simsui_stub_common\n"
    "_simsui_stub_common:\n"
    "    leal 8(%esp), %eax\n"
    "    pushl %eax\n"
    "    pushl %ecx\n"
    "    pushl 8(%esp)\n"
    "    call _simsui_dispatch\n"
    "    addl $16, %esp\n"
    "    movl _s_pop_now, %ecx\n"
    "    pushl %ebx\n"
    "    movl 4(%esp), %ebx\n"
    "    movl %ebx, 4(%esp,%ecx)\n"
    "    popl %ebx\n"
    "    addl %ecx, %esp\n"
    "    ret\n"
    /* _ftol: st(0) truncated to edx:eax, popped */
    ".globl _simsui_ftol\n"
    "_simsui_ftol:\n"
    "    subl $12, %esp\n"
    "    fnstcw 8(%esp)\n"
    "    movw 8(%esp), %ax\n"
    "    orw $0x0c00, %ax\n"
    "    movw %ax, 10(%esp)\n"
    "    fldcw 10(%esp)\n"
    "    fistpll (%esp)\n"
    "    fldcw 8(%esp)\n"
    "    movl (%esp), %eax\n"
    "    movl 4(%esp), %edx\n"
    "    addl $12, %esp\n"
    "    ret\n"
    /* invoke(entry, this, arguments, count): on the operations' own stack, below a gap a
     * retail overrun can write into without reaching the harness's frames */
    ".globl _simsui_invoke\n"
    "_simsui_invoke:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    movl %esp, _s_harness_esp\n"
    "    movl _s_operation_stack_top, %esp\n"
    "    movl %esp, %edi\n"            /* the stack below is cleared: no side reads another's garbage */
    "    subl $0x10000, %edi\n"
    "    movl $0x4000, %ecx\n"
    "    xorl %eax, %eax\n"
    "    cld\n"
    "    rep stosl\n"
    "    movl 20(%ebp), %ecx\n"
    "    movl 16(%ebp), %esi\n"
    "1:  testl %ecx, %ecx\n"
    "    jz 2f\n"
    "    pushl -4(%esi,%ecx,4)\n"
    "    decl %ecx\n"
    "    jmp 1b\n"
    "2:  movl 12(%ebp), %ecx\n"
    "    call *8(%ebp)\n"
    "    movl _s_harness_esp, %esp\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    popl %ebx\n"
    "    popl %ebp\n"
    "    ret\n");
void simsui_stub_common(void);
void simsui_ftol(void);
uint64_t simsui_invoke(void *entry, uint32_t self, const uint32_t *arguments, uint32_t count);

static void *make_stub(uint32_t index)
{
    static uint32_t used;
    uint8_t *stub = s_stubs + used;
    int32_t relative;
    used += 10;
    stub[0] = 0x68; memcpy(stub + 1, &index, 4);
    relative = (int32_t)((uint8_t *)simsui_stub_common - (stub + 10));
    stub[5] = 0xe9; memcpy(stub + 6, &relative, 4);
    return stub;
}
static uint32_t routine_pop(uint32_t address)
{
    uint32_t at;
    uint8_t *code = (uint8_t *)(uintptr_t)address;
    if (code[0] == 0xff && code[1] == 0x25) {          /* a thunk: the import's */
        uint32_t slot = *(uint32_t *)(code + 2) - BASE;
        for (at = 0; at < IMPORT_COUNT; ++at) if (s_imports[at].slot == slot) return s_imports[at].pop;
    }
    if (code[0] == 0xe9) return routine_pop(address + 5 + *(int32_t *)(code + 1));
    for (at = 0; at < FUNCTION_COUNT; ++at) if (BASE + s_functions[at].rva == address) return s_functions[at].pop;
    return 0;
}
static void build_vtables(void)
{
    uint32_t vtable, slot;
    for (vtable = 0; vtable < VTABLE_COUNT + 2; ++vtable) {
        uint32_t source = vtable < VTABLE_COUNT ? vtable : 3;      /* CSimsStatic's */
        s_fake_vtables[vtable] = calloc(128, sizeof(void *));
        for (slot = 0; slot < 128; ++slot) {
            s_fake_vtables[vtable][slot] = make_stub(VIRTUAL_BASE + vtable * 128 + slot);
            if (vtable == DC_VTABLE) s_virtual_pop[vtable][slot] = dc_slot_pop(slot);
            else if (slot < s_vtables[source].slots)
                s_virtual_pop[vtable][slot] = (uint8_t)routine_pop(*(uint32_t *)(uintptr_t)(BASE + s_vtables[source].rva + slot * 4));
        }
    }
}
/* which vtable an exported routine's class has, or the generic one */
static uint32_t class_vtable(uint32_t rva)
{
    uint32_t at, vtable;
    const char *name = 0, *start, *end;
    for (at = 0; at < FUNCTION_COUNT; ++at) if (s_functions[at].rva == rva) name = s_functions[at].name;
    if (!name || name[0] != '?') return GENERIC_VTABLE;
    start = strchr(name + 2, '@');
    if (!start) return GENERIC_VTABLE;
    ++start;
    end = strchr(start, '@');
    if (!end || end == start) return GENERIC_VTABLE;
    for (vtable = 0; vtable < VTABLE_COUNT; ++vtable) {
        const char *class_name = s_vtables[vtable].name + 4;
        if (!strncmp(class_name, start, (size_t)(end - start)) && class_name[end - start] == '@') return vtable;
    }
    return GENERIC_VTABLE;
}

static void fill(uint8_t *memory, uint32_t size)
{
    uint32_t at;
    for (at = 0; at + 4 <= size; at += 4) {
        uint32_t value = mixed(next_random());
        memcpy(memory + at, &value, 4);
    }
}
static void initialize(void)
{
    uint32_t block, at;
    memset(&s_fixture, 0, sizeof(s_fixture));
    s_rng = s_seed ^ (s_operation * 0x9e3779b9u);
    if (!s_rng) s_rng = 1;
    fill(s_fixture.before, sizeof(s_fixture.before));
    s_fixture.before[12] = (uint8_t)(next_random() % 4); s_fixture.before[13] = 0;
    s_fixture.before[14] = 0; s_fixture.before[15] = 0;
    fill(s_fixture.object, sizeof(s_fixture.object));
    for (at = 0; at < HEAP_SIZE; ++at) s_fixture.heap[at] = (uint8_t)(0xa5 ^ at ^ (at >> 7));  /* new memory is not clean */
    for (block = 0; block < BLOCKS; ++block) {
        fill(s_fixture.blocks[block], BLOCK_SIZE);
        if (block % 4 == 3) {               /* some read as text */
            static const char alphabet[] = "abcXYZ019-#,,,##  ";
            uint32_t length = next_random() % 28;
            for (at = 0; at < length; ++at) {
                uint32_t random = next_random();
                s_fixture.blocks[block][at] = random % 3 ? (uint8_t)alphabet[random % (sizeof(alphabet) - 1)]
                                                         : (uint8_t)(' ' + (random >> 8) % 95);
            }
            s_fixture.blocks[block][at] = 0;
        } else if (block % 4 == 1) {
            shape_info(s_fixture.blocks[block]);
        } else {
            void **vtable = s_fake_vtables[block % 4 == 2 ? DC_VTABLE : GENERIC_VTABLE];
            memcpy(s_fixture.blocks[block], &vtable, 4);
        }
    }
    {
        void **vtable = s_fake_vtables[class_vtable(CURRENT.rva)];
        memcpy(s_fixture.object, &vtable, 4);
    }
    shape_object(s_fixture.object);
    shape_dibs();
    memcpy(s_fixture.data, s_loaded_data, DATA_SIZE);
    if (s_seed & 1)                         /* the statics with values of their own */
        for (at = 0; at + 4 <= DATA_SIZE; at += 4)
            if (!*(uint32_t *)(s_fixture.data + at) && at != 0x2c) {
                uint32_t value = mixed(next_random());
                memcpy(s_fixture.data + at, &value, 4);
            }
    s_initial = s_fixture;
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
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    uint32_t address;
    if (!s_running || (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
                       record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
                       record->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION &&
                       record->ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO &&
                       record->ExceptionCode != EXCEPTION_INT_OVERFLOW))
        return EXCEPTION_CONTINUE_SEARCH;
    address = record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? (uint32_t)record->ExceptionInformation[1] : 0;
    /* which byte of a bad pointer is read first is the compiler's choice: only whether the
     * pointer was null-ish, wild or into the fixture counts */
    put("fault code=%lx access=%u address=%s\n", record->ExceptionCode,
        record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? (uint32_t)record->ExceptionInformation[0] : 0,
        address < 0x10000 ? "low" : in_fixture(address) ? "fixture" : "wild");
    if (!s_side) ++s_faults;
    s_running = 0;
    exception->ContextRecord->Eip = (DWORD)(uintptr_t)land;
    return EXCEPTION_CONTINUE_EXECUTION;
}
/* a routine looping without calls: stopped from outside after about two seconds */
static void timed_out(void)
{
    if (!s_side) s_retail_timed_out = 1;
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

static uint32_t argument(uint32_t kind)
{
    uint32_t random = next_random();
    switch (kind) {
    case ARG_SMALL: return random % 8;
    case ARG_FLAGS: return random % 4;
    case ARG_DC: return fixture_address(s_fixture.blocks[(random % (BLOCKS / 4)) * 4 + 2]);
    case ARG_POINTER: return block_pointer(random);
    case ARG_DIB: return fixture_address(s_fixture.dibs[random % 4]);
    case ARG_INTS: return fixture_address(&s_fixture.ints[random % 6]);
    case ARG_KEY: return random % 3 == 0 ? 0 : random % 3 == 1 ? 0xffffffffu : (random >> 4) % 4;
    case ARG_INFO: return fixture_address(s_fixture.blocks[(random % (BLOCKS / 4)) * 4 + 1]);
    case ARG_TEXT: return fixture_address(s_fixture.blocks[(random % (BLOCKS / 4)) * 4 + 3]);
    case ARG_SEPARATOR: { static const char separators[] = "#,a"; return random % 4 == 3 ? 0 : (uint32_t)separators[random % 4]; }
    case ARG_CODE: return (uint32_t)(uintptr_t)s_fake_vtables[GENERIC_VTABLE][0];
    default: return mixed(random);
    }
}
static __attribute__((noinline)) uint64_t run(void)
{
    void *entry = s_side ? CURRENT.ours : (void *)(uintptr_t)(BASE + CURRENT.rva);
    uint32_t arguments[16], count = CURRENT.arguments, at;
    volatile uint64_t result = 0;
    uint32_t saved_rng = s_rng;
    s_fixture = s_initial;
    memcpy((uint8_t *)(uintptr_t)(BASE + DATA_RVA), s_fixture.data, DATA_SIZE);
    s_rng = s_seed ^ 0x5bd1e995u ^ s_operation;
    for (at = 0; at < count; ++at) arguments[at] = argument(CURRENT.kinds[at]);
    s_rng = saved_rng;
    s_log_length = s_calls = s_heap_used = 0; s_logs[s_side][0] = 0;
    s_stack_low = (uint32_t)(uintptr_t)s_operation_stack;
    s_stack_high = s_stack_low + OPERATION_STACK;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        s_running = 1;
        result = simsui_invoke(entry, fixture_address(s_fixture.object), arguments, count);
        s_running = 0;
        if (CURRENT.returns == RET_VALUE) { put("return"); describe((uint32_t)result); put("\n"); }
    }
    s_running = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    memcpy(s_fixture.data, (uint8_t *)(uintptr_t)(BASE + DATA_RVA), DATA_SIZE);
    if (s_calls > s_max_calls) s_max_calls = s_calls;
    return result;
}
static int compare(void)
{
    uint32_t at;
    s_side = 0; s_retail_timed_out = 0; run(); s_retail_result = s_fixture;
    if (s_retail_timed_out) { ++s_unresolved; return 0; }    /* retail's result is unknown: nothing to compare */
    s_side = 1; run();
    ++s_comparisons;
    if (!memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s, %#x) seed=%x\n", s_scenario, s_operation, CURRENT.name, CURRENT.rva, s_seed);
    if (getenv("SIMSUI_DATA")) {      /* words of .data the initial fixture held */
        char *list = getenv("SIMSUI_DATA"), *end;
        for (;;) {
            uint32_t offset = strtoul(list, &end, 16);
            if (end == list) break;
            fprintf(stderr, "data+%x = %08x\n", offset, *(uint32_t *)(s_initial.data + offset - DATA_RVA));
            list = *end ? end + 1 : end;
        }
    }
    for (at = 0; at < sizeof(s_fixture); ++at)
        if (((uint8_t *)&s_retail_result)[at] != ((uint8_t *)&s_fixture)[at]) {
            uint32_t around;
            fprintf(stderr, "byte offset=%x retail=%02x ours=%02x\nnear: initial/retail/ours", at,
                ((uint8_t *)&s_retail_result)[at], ((uint8_t *)&s_fixture)[at]);
            for (around = at & ~15u; around < (at & ~15u) + 16 && around < sizeof(s_fixture); ++around)
                fprintf(stderr, " %02x/%02x/%02x", ((uint8_t *)&s_initial)[around], ((uint8_t *)&s_retail_result)[around],
                        ((uint8_t *)&s_fixture)[around]);
            fprintf(stderr, "\n");
            break;
        }
    {   /* the logs from a few lines before they part */
        const char *retail = s_logs[0], *ours = s_logs[1], *line = s_logs[0];
        uint32_t lines = 0;
        while (*retail && *retail == *ours) { if (*retail == '\n') { ++lines; line = retail + 1; } ++retail; ++ours; }
        retail = line;
        for (at = 0; at < 4 && retail > s_logs[0]; ) { --retail; if (retail[-1] == '\n' || retail == s_logs[0]) ++at; }
        ours = s_logs[1] + (retail - s_logs[0]);
        fprintf(stderr, "(logs agree for %u lines)\nRETAIL:\n%.3000s\nOURS:\n%.3000s\n", lines, retail, ours);
    }
    return 1;
}
static void setup(void)
{
    uint32_t index, at;
    HMODULE retail = LoadLibraryExA("orig/SIMSUI.DLL", 0, DONT_RESOLVE_DLL_REFERENCES);
    if ((uint32_t)(uintptr_t)retail != BASE) {
        fprintf(stderr, "orig/SIMSUI.DLL not mapped at its base: %p, error %lu\n", (void *)retail, GetLastError());
        exit(2);
    }
    fs98_simsui_image = (uint8_t *)retail;
    {
        uint8_t *self = (uint8_t *)GetModuleHandleA(0);
        IMAGE_NT_HEADERS *headers = (IMAGE_NT_HEADERS *)(self + ((IMAGE_DOS_HEADER *)self)->e_lfanew);
        IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(headers);
        s_our_code = (uint32_t)(uintptr_t)self + section->VirtualAddress;
        s_our_code_end = s_our_code + section->Misc.VirtualSize;
    }
    if (!VirtualAlloc((void *)(uintptr_t)FIXTURE_REGION, FIXTURE_RESERVE, MEM_RESERVE, PAGE_NOACCESS)) GIVE_UP();
    s_live = VirtualAlloc((void *)(uintptr_t)(FIXTURE_REGION + FIXTURE_RESERVE / 2), sizeof(struct fixture),
                          MEM_COMMIT, PAGE_READWRITE);
    if (!s_live) GIVE_UP();
    s_operation_stack = VirtualAlloc(0, OPERATION_STACK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!s_operation_stack) GIVE_UP();
    s_operation_stack_top = (uint32_t)(uintptr_t)s_operation_stack + OPERATION_STACK - 0x20000;
    s_stubs = VirtualAlloc(0, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_stubs) GIVE_UP();
    for (index = 0; index < IMPORT_COUNT; ++index) {
        void **slot = (void **)(uintptr_t)(BASE + s_imports[index].slot);
        void *stub = index == SIMSUI_IMPORT__ftol ? (void *)simsui_ftol : data_import(index);
        DWORD old;
        if (!stub) stub = make_stub(index);
        fs98_simsui_imports[index] = stub;
        if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) GIVE_UP();
        *slot = stub;
        VirtualProtect(slot, 4, old, &old);
    }
    {
        DWORD old;
        VirtualProtect((void *)(uintptr_t)(BASE + DATA_RVA), DATA_SIZE, PAGE_READWRITE, &old);
    }
    memcpy(s_loaded_data, (void *)(uintptr_t)(BASE + DATA_RVA), DATA_SIZE);
    build_vtables();
    for (index = 0; index < HOST_SLOTS; ++index) s_host_table[index] = make_stub(HOST_BASE + index);
    *(void **)(s_loaded_data + 0x2c) = s_host_table;     /* HOST's table, from the import link */
    for (at = 0; at < OP_COUNT; ++at) {
        uint32_t function;
        for (function = 0; function < FUNCTION_COUNT; ++function)
            if (s_functions[function].rva == s_operations[at].rva) break;
        if (function == FUNCTION_COUNT) { fprintf(stderr, "op %s: %#x is no routine\n", s_operations[at].name, s_operations[at].rva); exit(2); }
        if (s_operations[at].arguments == ~0u) {     /* those listed, or all the routine pops */
            uint32_t count = 0;
            while (count < 16 && s_operations[at].kinds[count]) ++count;
            if (count < s_functions[function].pop / 4) count = s_functions[function].pop / 4;
            s_operations[at].arguments = count;
        }
    }
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], 0, 0) : 2000;
    uint32_t initial_seed = argc > 2 ? strtoul(argv[2], 0, 0) : 0x51c5e1u;
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
            if (getenv("SIMSUI_TRACE")) { fprintf(stderr, "scenario %u op %u %s\n", s_scenario, s_operation, CURRENT.name); fflush(stderr); }
            if (compare()) return 1;
        }
    }
    printf("SIMSUI: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x faults=%u timeouts=%u "
           "(retail's, not compared) max_calls=%u\n",
        count, last - first + 1, s_comparisons, initial_seed, s_faults, s_unresolved, s_max_calls);
    return 0;
}
