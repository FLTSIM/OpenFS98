/* The executable built from the decompiled runtime (src/host/runtime/) and the services
 * (src/host/host.c, built with HOST_IMAGE_SHARED): no lifted code and none of retail's.
 *
 * Retail's .rdata, .data and .idata are laid out as retail's (0x416000..0x41f000) in one block,
 * wherever it can be had (in this process Wine keeps the main thread's stack at 0x400000), and
 * shared by the runtime (fs98_host_rt_image) and the services (fs98_host_data, its .data). Every
 * word of it that held one of retail's addresses is rebased, as the lifted build's loader does:
 * a data address into the block; a code address to our routine, or to the import its thunk
 * jumps to (the application's vtable, the static constructors). The import slots are filled as
 * the loader would, and the service table is host.c's. Then, as retail's C runtime does, the
 * static constructors run and MFC's AfxWinMain takes over. The frame-locked test driver
 * (tools/drive/framelock.c) finds OpenFS98Host here as in the lifted build. */
#include <windows.h>
#include <stdio.h>
#include "runtime/host_rt.h"
#include "fs98/host_runtime.h"

#define IMAGE_START 0x16000u     /* .rdata */
#define IMAGE_END 0x1f000u       /* to .idata's end */
#define IMAGE_AT(rva) (fs98_host_rt_image + (rva))   /* VA(va) is IMAGE_AT(va - 0x400000) */

void *fs98_host_rt_imports[HOST_RT_IMPORT_COUNT];
uint8_t *fs98_host_rt_image;
uint8_t *fs98_host_data;
extern uint8_t host_image[];            /* host.c's .data, retail's as it starts */
extern void *HostTable[0xc8];           /* host.c's service table */

static const uint8_t s_rdata[] = {
#include "runtime/host_rt_rdata.inc"
};

static const struct { const char *dll, *name; uint32_t slot; } s_import_names[HOST_RT_IMPORT_COUNT] = {
#define HOST_RT_IMPORT(index, ident, dll, name, slot, pop, thiscall, arguments) [index] = {dll, name, slot},
#include "runtime/host_rt_tables.inc"
#undef HOST_RT_IMPORT
};

static void *patch_import(const char *name, void *replacement);
__declspec(dllexport) struct fs98_host_runtime OpenFS98Host;

/* our routines by retail's address, as the harness has them */
static const struct { uint32_t rva; void *ours; } s_routines[] = {
#define OP(rva, name, ...) {rva, (void *)fs98_host_rt_##name}
#define OPV(rva, name, ...) {rva, (void *)fs98_host_rt_##name}
#include "../../tools/difftest/hostrt_operations.inc"
#undef OP
#undef OPV
};

static uint32_t ordinal_of(const char *digits)
{
    uint32_t value = 0;
    while (*digits >= '0' && *digits <= '9') value = value * 10 + (uint32_t)(*digits++ - '0');
    return value;
}

static void fail(const char *what, const char *detail)
{
    fprintf(stderr, "host runtime: %s %s (error %lu)\n", what, detail ? detail : "", (unsigned long)GetLastError());
    fflush(stderr);
}

/* every import, by name or (MFC42's) ordinal, into the table and the image's slots */
static int resolve_imports(void)
{
    uint32_t index;
    for (index = 0; index < HOST_RT_IMPORT_COUNT; ++index) {
        HMODULE library = LoadLibraryA(s_import_names[index].dll);
        const char *name = s_import_names[index].name;
        if (!library) { fail("cannot load", s_import_names[index].dll); return 0; }
        fs98_host_rt_imports[index] = name[0] == '#' ? (void *)GetProcAddress(library, MAKEINTRESOURCEA(ordinal_of(name + 1)))
                                                     : (void *)GetProcAddress(library, name);
        if (!fs98_host_rt_imports[index]) { fail("cannot resolve", name); return 0; }
        *(void **)IMAGE_AT(s_import_names[index].slot) = fs98_host_rt_imports[index];
    }
    return 1;
}

/* a code address in retail's image made ours: a routine, an import's thunk, or nothing */
static uint32_t code_for(uint32_t target)
{
    uint32_t at;
    for (at = 0; at < sizeof s_routines / sizeof s_routines[0]; ++at)
        if (s_routines[at].rva == target) return (uint32_t)(uintptr_t)s_routines[at].ours;
#define HOST_RT_THUNK(rva, index) if (target == rva) return (uint32_t)(uintptr_t)fs98_host_rt_imports[index];
#include "runtime/host_rt_tables.inc"
#undef HOST_RT_THUNK
    return 0;           /* the SEH scopes, the CRT's own: never called */
}

/* retail's .rdata, .data and .idata in a block of ours, its pointers rebased, its import slots
 * filled, its code addresses ours */
static int map_image(void)
{
    uint8_t *block = VirtualAlloc(0, IMAGE_END - IMAGE_START, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!block) { fail("cannot allocate", "the image"); return 0; }
    fs98_host_rt_image = block - IMAGE_START;
    fs98_host_data = IMAGE_AT(0x17000);
    memcpy(IMAGE_AT(0x16000), s_rdata, sizeof s_rdata < 0x1000 ? sizeof s_rdata : 0x1000);
    memcpy(IMAGE_AT(0x17000), host_image, 0x55cc);
    if (!resolve_imports()) return 0;
#define HOST_RT_DATA_POINTER(site, target) *(uint32_t *)IMAGE_AT(site) = (uint32_t)(uintptr_t)IMAGE_AT(target);
#define HOST_RT_CODE_POINTER(site, target) *(uint32_t *)IMAGE_AT(site) = code_for(target);
#include "runtime/host_rt_tables.inc"
#undef HOST_RT_DATA_POINTER
#undef HOST_RT_CODE_POINTER
    memcpy(IMAGE_AT(0x17ba0), HostTable, 0x320);
    OpenFS98Host.image = IMAGE_AT(0x17000);
    return 1;
}

/* the test driver's: OpenFS98Host's import patching, onto the runtime's table */
static void *patch_import(const char *name, void *replacement)
{
    uint32_t index;
    void *previous = 0;
    for (index = 0; index < HOST_RT_IMPORT_COUNT; ++index)
        if (s_import_names[index].name[0] != '#' && !strcmp(s_import_names[index].name, name)) {
            previous = fs98_host_rt_imports[index];
            fs98_host_rt_imports[index] = replacement;
            *(void **)IMAGE_AT(s_import_names[index].slot) = replacement;
        }
    return previous;
}
__declspec(dllexport) struct fs98_host_runtime OpenFS98Host = {1, NULL, patch_import, NULL};
static void loop_pass(void)
{
    if (OpenFS98Host.main_loop_pass)
        OpenFS98Host.main_loop_pass();
}

static LONG WINAPI unhandled(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "host runtime: exception %08lx at %p\n", (unsigned long)exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    fflush(stderr);
    ExitProcess(4);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    /* MSVCRT starts a VC5 program at 53-bit precision; mingw's startup leaves 64 (SIM1's
     * trajectories depend on it) */
    const uint16_t control = 0x027f;
    __asm__ volatile("fldcw %0" : : "m"(control));
    SetUnhandledExceptionFilter(unhandled);
    if (!map_image())
        return 2;
    fs98_host_rt_loop_pass = loop_pass;
    AddVectoredExceptionHandler(1, (PVECTORED_EXCEPTION_HANDLER)(void *)fs98_host_rt_divide_handler);
    /* an executable's onexit wrapper hands everything to MSVCRT's list (0x414c70) */
    G32(0x41c5c4) = 0xffffffffu;
    G32(0x41c5c8) = 0xffffffffu;
    /* retail's C runtime runs its static constructors (0x417004..0x417010) before WinMain */
    fs98_host_rt_construct_afx();
    fs98_host_rt_static_418798();
    fs98_host_rt_static_application();
    fs98_host_rt_413220();
    return CALL_STD(MFC_1576, int, (HINSTANCE, HINSTANCE, LPSTR, int))(instance, previous, command, show);
}
