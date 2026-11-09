/* FLTSIM98.EXE's runtime routines, the original code against C (src/host/runtime/).
 *
 *   hostrtdiff.exe [scenarios] [seed] [first-op] [last-op]
 *
 * As simsuidiff.c, whose structure this keeps. Retail FLTSIM98.EXE is mapped at 0x400000 (this
 * harness is linked elsewhere) without its imports: every import slot points at a generated
 * stub, a push of the import's index and a jump to one dispatcher, which logs the call (ECX for
 * a member, every argument the import pops or its callers remove), answers from the scenario's
 * script, or does what the import does where that matters (the string functions), and pops
 * what the import pops. Each HOST service's first bytes become a jump to such a stub too: the
 * runtime calls the services directly, and they are verified on their own (hostdiff), so here
 * they are logged, not run. Ours calls the same stubs through fs98_host_rt_imports and the
 * service table in the image, and reads retail's data in the same mapped image.
 *
 * Each operation runs twice from one randomised fixture, retail then ours: an object, blocks of
 * memory every pointer in the fixture leads to, a heap, and the EXE's .data. The call logs, the
 * return values and every fixture byte must agree. Values are logged by what they are: a
 * fixture address by its offset, a local by "L", code by "C" and its RVA, retail's image by
 * "I" and its RVA, anything else by value. A fault is an outcome to compare; a routine that runs
 * too long is stopped and reported. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/host/runtime/host_rt.h"

#define BASE 0x400000u
#define TEXT_RVA 0x1000u
#define TEXT_END 0x15060u
#define DATA_RVA 0x17000u
#define DATA_SIZE 0x55ccu
#define LOG_SIZE 65536
#define BLOCKS 32
#define BLOCK_SIZE 0x400
#define HEAP_SIZE 0x10000
#define MAX_CALLS 4000
#define SERVICE_BASE 0x800u          /* a service's stub index: SERVICE_BASE + offset / 4 */
#define SERVICE_SLOTS 0x100
#define COM_BASE 0x1000u             /* a COM method's: COM_BASE + kind * 64 + slot */
#define COM_OBJECTS 24
#define GIVE_UP() do { fprintf(stderr, "GIVE UP line %d scenario=%u op=%u side=%d\n", __LINE__, s_scenario, s_operation, s_side); exit(2); } while (0)

struct import_info { const char *dll, *name; uint32_t slot, pop, thiscall, arguments; };
static const struct import_info s_imports[] = {
#define HOST_RT_IMPORT(index, ident, dll, name, slot, pop, thiscall, arguments) {dll, name, slot, pop, thiscall, arguments},
#include "../../src/host/runtime/host_rt_tables.inc"
#undef HOST_RT_IMPORT
};
struct function_info { uint32_t rva, pop, thiscall; };
static const struct function_info s_functions[] = {
#define HOST_RT_FUNCTION(rva, pop, thiscall) {rva, pop, thiscall},
#include "../../src/host/runtime/host_rt_tables.inc"
#undef HOST_RT_FUNCTION
};
struct service_info { uint32_t rva, offset, pop; };
static const struct service_info s_services[] = {
#define HOST_RT_SERVICE(rva, offset, pop) {rva, offset, pop},
#include "../../src/host/runtime/host_rt_tables.inc"
#undef HOST_RT_SERVICE
};
#define IMPORT_COUNT (sizeof(s_imports) / sizeof(s_imports[0]))
#define FUNCTION_COUNT (sizeof(s_functions) / sizeof(s_functions[0]))
#define SERVICE_COUNT (sizeof(s_services) / sizeof(s_services[0]))

void *fs98_host_rt_imports[HOST_RT_IMPORT_COUNT];
uint8_t *fs98_host_rt_image;

struct fixture {
    uint8_t guard_before[32];
    uint8_t object[0x2000];
    uint8_t blocks[BLOCKS][BLOCK_SIZE];
    uint8_t heap[HEAP_SIZE];
    int32_t ints[12];
    uint8_t com[COM_OBJECTS][0x40];  /* COM objects: a fake vtable, then their own words */
    uint8_t data[DATA_SIZE];        /* the EXE's .data, in the image while a side runs */
    uint8_t guard_after[32];
};
/* the live fixture sits alone in a reserved region: a read run past it faults */
static struct fixture *s_live, s_initial, s_retail_result;
#define s_fixture (*s_live)
#define FIXTURE_REGION 0x30000000u
#define FIXTURE_RESERVE 0x08000000u
static uint8_t s_loaded_data[DATA_SIZE];
static uint32_t s_image_size;
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_calls, s_log_length, s_heap_used;
static uint32_t s_comparisons, s_faults, s_timeouts, s_max_calls, s_saved_seh, s_stack_low, s_stack_high;
static int s_side, s_running, s_retail_timed_out, s_ours_timed_out;
static uint32_t s_unresolved;
static char s_logs[2][LOG_SIZE];
static uint8_t *s_stubs;
static uint32_t s_service_pop[SERVICE_SLOTS];
static uint32_t s_pop_now, s_com_used;
uint32_t s_harness_esp, s_operation_stack_top;
static uint8_t *s_operation_stack;
#define OPERATION_STACK 0x100000
static void *s_jump[5];
static void *s_callback_stub;
static volatile LONG s_watch_ticks;
static HANDLE s_main_thread;

/* the operations, and how each is called */
enum { ARG_UNSET, ARG_ANY, ARG_SMALL, ARG_FLAGS, ARG_POINTER, ARG_TEXT, ARG_DRIVER, ARG_MAYBE, ARG_MAYBE_TEXT,
       ARG_OUT, ARG_SELF, ARG_DEVICE_NAME, ARG_MAYBE_DEVICE_NAME, ARG_MAYBE_OUT, ARG_INTS, ARG_SEARCH, ARG_DESC,
       ARG_CALLBACK, ARG_VISIT, ARG_MESSAGE, ARG_WINDOW, ARG_SURFACE, ARG_OUT2, ARG_COORD, ARG_PITCH, ARG_COUNT, ARG_WINDOW_DATA, ARG_PICTURE, ARG_SOURCE_PICTURE, ARG_RECT, ARG_POINT };
#define CALLBACK_INDEX 0x7ffu        /* a callback the harness gives: (a, b), logged */
struct operation {
    uint32_t rva; void *ours; const char *name;
    uint32_t arguments; uint8_t kinds[16];
    uint32_t returns;   /* RET_VALUE, RET_VOID */
};
enum { RET_VALUE, RET_VOID };
#define OP(rva, name, ...) {rva, (void *)fs98_host_rt_##name, #name, ~0u, {__VA_ARGS__}, RET_VALUE}
#define OPV(rva, name, ...) {rva, (void *)fs98_host_rt_##name, #name, ~0u, {__VA_ARGS__}, RET_VOID}
static struct operation s_operations[] = {
#include "hostrt_operations.inc"
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
static int in_image(uint32_t value) { return value >= BASE && value < BASE + s_image_size; }
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
    else if (in_code(value)) {
        uint32_t at;
        if (value >= BASE + TEXT_RVA && value < BASE + TEXT_END) { put(" C%x", value - BASE); return; }
        for (at = 0; at < OP_COUNT; ++at)
            if ((uint32_t)(uintptr_t)s_operations[at].ours == value) { put(" C%x", s_operations[at].rva); return; }
        put(" C");
    }
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
static uint32_t block_pointer(uint32_t random) { return fixture_address(s_fixture.blocks[random % BLOCKS]); }
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
static __attribute__((unused)) void *heap_allocate(uint32_t size)
{
    uint32_t at = (s_heap_used + 15) & ~15u;
    if (size > HEAP_SIZE || at + size > HEAP_SIZE) return 0;
    s_heap_used = at + size;
    return s_fixture.heap + at;
}
static void fill(uint8_t *memory, uint32_t size)
{
    uint32_t at;
    for (at = 0; at + 4 <= size; at += 4) {
        uint32_t value = mixed(next_random());
        memcpy(memory + at, &value, 4);
    }
}
static uint32_t argument(uint32_t kind);

static void *make_stub(uint32_t index);
#include "hostrt_semantics.inc"

/* the dispatcher every stub jumps to: index, the caller's ECX, its arguments */
uint64_t __cdecl hostrt_dispatch(uint32_t index, uint32_t ecx, uint32_t *arguments);
uint64_t __cdecl hostrt_dispatch(uint32_t index, uint32_t ecx, uint32_t *arguments)
{
    uint32_t count, at, result;
    uint64_t special;
    if (++s_calls > MAX_CALLS) { put("too many calls\n"); GIVE_UP(); }
    if (index >= COM_BASE) {
        uint32_t kind = (index - COM_BASE) / 64, slot = (index - COM_BASE) % 64;
        s_pop_now = com_pop(kind, slot);
        put("com %s.%x", com_names[kind], slot * 4);    /* this is the first argument */
        for (at = 0; at < s_pop_now / 4; ++at) describe(arguments[at]);
        com_log_contents(kind, slot, arguments);
        if (com_semantics(kind, slot, arguments, &special)) { put(" =>"); describe((uint32_t)special); put("\n"); return special; }
        result = answer();
        put(" -> %x\n", result);
        return result;
    }
    if (index == CALLBACK_INDEX) {
        s_pop_now = 8;
        put("callback"); describe(arguments[0]); describe(arguments[1]);
        result = answer();
        put(" -> %x\n", result);
        return result;
    }
    if (index >= SERVICE_BASE) {
        uint32_t offset = (index - SERVICE_BASE) * 4;
        s_pop_now = s_service_pop[offset / 4];
        put("service %x", offset);
        for (at = 0; at < s_pop_now / 4; ++at) describe(arguments[at]);
        if (service_semantics(offset, arguments, &special)) { put(" =>"); describe((uint32_t)special); put("\n"); return special; }
        result = answer();
        put(" -> %x\n", result);
        return result;
    }
    s_pop_now = s_imports[index].pop;
    count = s_imports[index].arguments;
    put("%s", s_imports[index].name);
    if (s_imports[index].thiscall) { put(" this"); describe(ecx); }
    for (at = 0; at < count; ++at) describe(arguments[at]);
    log_contents(index, arguments);
    if (semantics(index, ecx, arguments, &special)) {
        put(" =>"); describe((uint32_t)special); put("\n");
        return special;
    }
    result = answer();
    put(" -> %x\n", result);
    return result;
}

void hostrt_stub_common(void);
uint64_t hostrt_invoke(void *entry, uint32_t self, const uint32_t *arguments, uint32_t count);
/* index, then the shared tail: call the dispatcher, then pop what the callee pops */
__asm__(
    ".text\n"
    ".globl _hostrt_stub_common\n"
    "_hostrt_stub_common:\n"
    "    leal 8(%esp), %eax\n"
    "    pushl %eax\n"
    "    pushl %ecx\n"
    "    pushl 8(%esp)\n"
    "    call _hostrt_dispatch\n"
    "    addl $16, %esp\n"
    "    movl _s_pop_now, %ecx\n"
    "    pushl %ebx\n"
    "    movl 4(%esp), %ebx\n"
    "    movl %ebx, 4(%esp,%ecx)\n"
    "    popl %ebx\n"
    "    addl %ecx, %esp\n"
    "    ret\n"
    /* invoke(entry, this, arguments, count): on the operations' own stack, cleared, below a
     * gap a retail overrun can write into without reaching the harness's frames */
    ".globl _hostrt_invoke\n"
    "_hostrt_invoke:\n"
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

static void *make_stub(uint32_t index)
{
    static uint32_t used;
    uint8_t *stub = s_stubs + used;
    int32_t relative;
    used += 10;
    if (used > 0x10000) GIVE_UP();
    stub[0] = 0x68; memcpy(stub + 1, &index, 4);
    relative = (int32_t)((uint8_t *)hostrt_stub_common - (stub + 10));
    stub[5] = 0xe9; memcpy(stub + 6, &relative, 4);
    return stub;
}

static void initialize(void)
{
    uint32_t block, at;
    memset(&s_fixture, 0, sizeof(s_fixture));
    s_rng = s_seed ^ (s_operation * 0x9e3779b9u);
    if (!s_rng) s_rng = 1;
    fill(s_fixture.object, sizeof(s_fixture.object));
    for (at = 0; at < HEAP_SIZE; ++at) s_fixture.heap[at] = (uint8_t)(0xa5 ^ at ^ (at >> 7));
    for (block = 0; block < BLOCKS; ++block) {
        fill(s_fixture.blocks[block], BLOCK_SIZE);
        if (block % 4 == 3) {               /* some read as text */
            static const char alphabet[] = "abcXYZ019-\\.  ";
            uint32_t length = next_random() % 28;
            for (at = 0; at < length; ++at) {
                uint32_t random = next_random();
                s_fixture.blocks[block][at] = random % 3 ? (uint8_t)alphabet[random % (sizeof(alphabet) - 1)]
                                                         : (uint8_t)(' ' + (random >> 8) % 95);
            }
            s_fixture.blocks[block][at] = 0;
        }
    }
    for (at = 0; at < 12; ++at) s_fixture.ints[at] = (int32_t)mixed(next_random());
    shape_fixture();
    memcpy(s_fixture.data, s_loaded_data, DATA_SIZE);
    shape_data();
    if (s_seed & 1)                         /* the statics with values of their own */
        for (at = 0; at + 4 <= DATA_SIZE; at += 4)
            if (!*(uint32_t *)(s_fixture.data + at) && data_may_vary(DATA_RVA + at)) {
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

static uint32_t argument(uint32_t kind)
{
    uint32_t random = next_random();
    switch (kind) {
    case ARG_SMALL: return random % 8;
    case ARG_FLAGS: return random % 4;
    case ARG_POINTER: return block_pointer(random);
    case ARG_TEXT: return fixture_address(s_fixture.blocks[(random % (BLOCKS / 4)) * 4 + 3]);
    case ARG_MAYBE: return random % 2 ? 0 : mixed(random >> 1);
    case ARG_MAYBE_TEXT: return random % 3 == 0 ? 0 : fixture_address(s_fixture.blocks[((random >> 2) % (BLOCKS / 4)) * 4 + 3]);
    case ARG_DRIVER: {      /* a device's driver name: \\.\DISPLAYn, "display" in some case, or text */
        static const char *const drivers[] = {"\\\\.\\DISPLAY1", "\\\\.\\DISPLAY2", "display", "DISPLAY", "\\\\x",
                                              "\\.", "", "\\\\", "Display"};
        uint8_t *block = s_fixture.blocks[((random >> 4) % (BLOCKS / 4)) * 4 + 3];
        if (random % 5 == 4) return fixture_address(block);
        strcpy((char *)block + 0x300, drivers[(random >> 8) % 9]);
        return fixture_address(block + 0x300);
    }
    case ARG_OUT: return fixture_address(s_fixture.blocks[((random >> 4) % (BLOCKS / 4)) * 4]);
    case ARG_MAYBE_OUT: return random % 4 == 0 ? 0 : fixture_address(s_fixture.blocks[((random >> 4) % (BLOCKS / 4)) * 4]);
    case ARG_INTS: return fixture_address(&s_fixture.ints[random % 9]);
    case ARG_MAYBE_DEVICE_NAME: if (random % 4 == 0) return random % 8 == 0 ? 0 : fixture_address(s_fixture.blocks[27] + 0x3ff);
        /* fall through */
    case ARG_DEVICE_NAME: {     /* a listed device's name, with or without a 3D suffix */
        static const char *const names[] = {"Voodoo (Direct3D)", "Primary Display Driver (Direct3D MMX)", "x(Direct3D RGB)",
            "(Direct3D RAMP)", "Primary Display Driver", "Direct3D)", "display", "\\\\.\\DISPLAY2 (Direct3D)",
            "Voodoo Graphics", "a (Direct3D RAMP) b"};
        uint8_t *block = s_fixture.blocks[((random >> 6) % (BLOCKS / 4)) * 4 + 3];
        strcpy((char *)block + 0x200, names[(random >> 2) % 10]);
        return fixture_address(block + 0x200);
    }
    case ARG_SEARCH: {          /* a device search: the name now and then a driver the enumeration shows */
        static const char *const names[] = {"display", "Voodoo Graphics", "\\\\.\\DISPLAY2", "Nothing"};
        uint8_t *search = s_fixture.blocks[((random >> 4) % (BLOCKS / 4)) * 4 + 1];
        uint32_t object = fixture_address(s_fixture.blocks[((random >> 8) % (BLOCKS / 4)) * 4]);
        strcpy((char *)search + 4, names[random % 4]);
        memcpy(search + 0x84, &object, 4);
        return fixture_address(search);
    }
    case ARG_DESC: {            /* a D3DDEVICEDESC: none, empty, or with flags and caps */
        uint8_t *desc = s_fixture.blocks[((random >> 4) % (BLOCKS / 4)) * 4 + 2] + ((random >> 9) % 2) * 0x100;
        uint32_t flags = random % 3 ? (random >> 12) % 4 : 0;
        if (random % 5 == 0) return 0;
        memcpy(desc + 4, &flags, 4);
        flags = (random >> 16) % 4; memcpy(desc + 8, &flags, 4);
        return fixture_address(desc);
    }
    case ARG_CALLBACK: return (uint32_t)(uintptr_t)s_callback_stub;
    case ARG_VISIT: {
        uint8_t *visit = s_fixture.blocks[((random >> 4) % (BLOCKS / 4)) * 4 + 2] + 0x3f0;
        uint32_t code = (uint32_t)(uintptr_t)s_callback_stub, parameter = mixed(random >> 8);
        memcpy(visit, &code, 4);
        memcpy(visit + 4, &parameter, 4);
        return fixture_address(visit);
    }
    case ARG_MESSAGE: return random % 2 ? 0x46d : random % 3 ? 0x46e + random % 4 : mixed(random >> 4);
    case ARG_WINDOW: return random % 5 == 0 ? 0 : 0x4000 + random % 16;
    case ARG_SELF: return fixture_address(s_fixture.object);
    case ARG_PITCH: return random % 8 == 0 ? (uint32_t)-(int32_t)((random >> 3) % 64) : (random >> 3) % 72;
    case ARG_COUNT: return random % 10 == 0 ? (uint32_t)-(int32_t)((random >> 4) % 4) : (random >> 4) % 20;
    case ARG_WINDOW_DATA: return fixture_address(s_fixture.blocks[random % 2 ? 5 : 13]);
    case ARG_PICTURE: return fixture_address(s_fixture.blocks[7] + 0x200);
    case ARG_SOURCE_PICTURE: return fixture_address(s_fixture.blocks[random % 2 ? 5 : 13] + 0x80);
    case ARG_RECT: return fixture_address(s_fixture.blocks[10] + 0x3c0);
    case ARG_POINT: return fixture_address(s_fixture.blocks[10] + 0x3e0);
    case ARG_OUT2: return fixture_address(s_fixture.blocks[((random >> 4) % (BLOCKS / 4)) * 4] + 0x200);
    case ARG_COORD: return random % 4 == 0 ? (uint32_t)-(int32_t)((random >> 2) % 700) : (random >> 2) % 900;
    case ARG_SURFACE: {         /* the primary, back buffer or overlay (any may be none), or another */
        static const uint32_t surfaces[] = {0x41a4f8, 0x41a4e0, 0x41a4fc};
        uint32_t object;
        if (random % 4 == 3) return fixture_address(s_fixture.com[COM_OBJECTS - 1]);
        memcpy(&object, s_fixture.data + surfaces[random % 4] - BASE - DATA_RVA, 4);
        return object ? object : fixture_address(s_fixture.com[COM_OBJECTS - 1]);
    }
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
    s_com_used = s_com_first;
    s_stack_low = (uint32_t)(uintptr_t)s_operation_stack;
    s_stack_high = s_stack_low + OPERATION_STACK;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        s_running = 1;
        result = hostrt_invoke(entry, fixture_address(s_fixture.object), arguments, count);
        s_running = 0;
        if (CURRENT.returns == RET_VALUE) { put("return"); describe((uint32_t)result); put("\n"); }
    }
    s_running = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    memcpy(s_fixture.data, (uint8_t *)(uintptr_t)(BASE + DATA_RVA), DATA_SIZE);
    if (s_calls > s_max_calls) s_max_calls = s_calls;
    return result;
}
static uint32_t s_cover_runs, s_cover_hits;
/* HOSTRT_COVER=op,text: how many of the op's runs have text in retail's log (or "@changed") */
static void cover(void)
{
    const char *want = getenv("HOSTRT_COVER"), *comma;
    if (!want || !(comma = strchr(want, ','))) return;
    if (strlen(CURRENT.name) != (size_t)(comma - want) || strncmp(CURRENT.name, want, (size_t)(comma - want))) return;
    ++s_cover_runs;
    /* "@changed": retail changed the fixture's memory (outside .data) */
    if (!strcmp(comma + 1, "@changed") ? memcmp(&s_initial, &s_retail_result, offsetof(struct fixture, data)) != 0
                                       : strstr(s_logs[0], comma + 1) != 0) {
        ++s_cover_hits;
        if (getenv("HOSTRT_SHOW") && s_cover_hits <= 3) fprintf(stderr, "--- %s:\n%s", CURRENT.name, s_logs[0]);
    }
}
static int compare(void)
{
    uint32_t at;
    DWORD started = GetTickCount(), retail_took;
    s_side = 0; s_retail_timed_out = 0; run(); s_retail_result = s_fixture;
    retail_took = GetTickCount() - started;
    cover();
    if (s_retail_timed_out) { ++s_unresolved; return 0; }
    s_side = 1; s_ours_timed_out = 0; run();
    if (s_ours_timed_out && retail_took > 300) { ++s_unresolved; return 0; }
    ++s_comparisons;
    if (!memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s, %#x) seed=%x\n", s_scenario, s_operation, CURRENT.name, CURRENT.rva, s_seed);
    for (at = 0; at < sizeof(s_fixture); ++at)
        if (((uint8_t *)&s_retail_result)[at] != ((uint8_t *)&s_fixture)[at]) {
            uint32_t around;
            fprintf(stderr, "byte offset=%x retail=%02x ours=%02x%s\nnear: initial/retail/ours", at,
                ((uint8_t *)&s_retail_result)[at], ((uint8_t *)&s_fixture)[at],
                at >= offsetof(struct fixture, data) && at < offsetof(struct fixture, data) + DATA_SIZE ? " (data)" : "");
            if (at >= offsetof(struct fixture, data) && at < offsetof(struct fixture, data) + DATA_SIZE)
                fprintf(stderr, " va=%#x", (unsigned)(BASE + DATA_RVA + at - offsetof(struct fixture, data)));
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
        if (getenv("HOSTRT_LOGS")) fprintf(stderr, "FULL RETAIL:\n%s\nFULL OURS:\n%s\n", s_logs[0], s_logs[1]);
    }
    return 1;
}

/* the retail EXE at its base, its sections where the loader puts them, nothing resolved */
static void map_retail(void)
{
    static uint8_t file[0x30000];
    FILE *stream = fopen("orig/FLTSIM98.EXE", "rb");
    uint32_t pe, sections, at, headers;
    size_t length;
    if (!stream) { fprintf(stderr, "cannot open orig/FLTSIM98.EXE\n"); exit(2); }
    length = fread(file, 1, sizeof file, stream);
    fclose(stream);
    if (length < 0x1000) GIVE_UP();
    pe = *(uint32_t *)(file + 0x3c);
    s_image_size = *(uint32_t *)(file + pe + 0x50);
    headers = *(uint32_t *)(file + pe + 0x54);
    sections = *(uint16_t *)(file + pe + 6);
    {   /* under wine the region is already there (the preloader's reservation): made usable */
        DWORD old;
        if (!VirtualAlloc((void *)(uintptr_t)BASE, s_image_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE) &&
            !VirtualAlloc((void *)(uintptr_t)BASE, s_image_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE) &&
            !VirtualProtect((void *)(uintptr_t)BASE, s_image_size, PAGE_EXECUTE_READWRITE, &old)) {
            fprintf(stderr, "cannot map 0x400000 (%lu)\n", GetLastError());
            exit(2);
        }
    }
    memcpy((void *)(uintptr_t)BASE, file, headers);
    for (at = 0; at < sections; ++at) {
        uint8_t *section = file + pe + 24 + *(uint16_t *)(file + pe + 20) + at * 40;
        uint32_t rva = *(uint32_t *)(section + 12), raw = *(uint32_t *)(section + 20), size = *(uint32_t *)(section + 16);
        memcpy((uint8_t *)(uintptr_t)(BASE + rva), file + raw, size);
    }
    fs98_host_rt_image = (uint8_t *)(uintptr_t)BASE;
}
static void setup(void)
{
    uint32_t index, at;
    map_retail();
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
        void *stub = data_import(index);
        if (!stub) stub = make_stub(index);
        fs98_host_rt_imports[index] = stub;
        *(void **)(uintptr_t)(BASE + s_imports[index].slot) = stub;
    }
    for (index = 0; index < SERVICE_COUNT; ++index) {   /* each service's entry: jmp to its stub */
        uint8_t *entry = (uint8_t *)(uintptr_t)(BASE + s_services[index].rva), *stub;
        int32_t relative;
        if (s_services[index].offset / 4 >= SERVICE_SLOTS) GIVE_UP();
        s_service_pop[s_services[index].offset / 4] = s_services[index].pop;
        stub = make_stub(SERVICE_BASE + s_services[index].offset / 4);
        relative = (int32_t)(stub - (entry + 5));
        entry[0] = 0xe9; memcpy(entry + 1, &relative, 4);
    }
    memcpy(s_loaded_data, (void *)(uintptr_t)(BASE + DATA_RVA), DATA_SIZE);
    com_setup();
    s_callback_stub = make_stub(CALLBACK_INDEX);
    for (at = 0; at < OP_COUNT; ++at) {
        uint32_t function;
        for (function = 0; function < FUNCTION_COUNT; ++function)
            if (s_functions[function].rva == s_operations[at].rva) break;
        if (function == FUNCTION_COUNT) { fprintf(stderr, "op %s: %#x is no routine\n", s_operations[at].name, s_operations[at].rva); exit(2); }
        if (s_operations[at].arguments == ~0u) {     /* those listed, or all the routine pops */
            uint32_t count = 0;
            while (count < 16 && s_operations[at].kinds[count]) ++count;
            if (s_functions[function].pop != 0xffff && count < s_functions[function].pop / 4) count = s_functions[function].pop / 4;
            s_operations[at].arguments = count;
        }
    }
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], 0, 0) : 2000;
    uint32_t initial_seed = argc > 2 ? strtoul(argv[2], 0, 0) : 0x40c5e1u;
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
            if (getenv("HOSTRT_TRACE")) { fprintf(stderr, "scenario %u op %u %s\n", s_scenario, s_operation, CURRENT.name); fflush(stderr); }
            if (compare()) return 1;
        }
    }
    if (getenv("HOSTRT_COVER")) printf("cover %s: %u of %u\n", getenv("HOSTRT_COVER"), s_cover_hits, s_cover_runs);
    printf("HOST runtime: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x faults=%u timeouts=%u "
           "(retail's, not compared) max_calls=%u\n",
        count, last - first + 1, s_comparisons, initial_seed, s_faults, s_unresolved, s_max_calls);
    return 0;
}
