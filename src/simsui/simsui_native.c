/* The native SIMSUI.DLL's own: its imports found, its image of retail's data made ours, and
 * the CRT's entry (0x20483490) and DLL initialisation (0x204833c0) as retail runs them,
 * around DllMain (simsui_fonts.c).
 *
 * The image (bin/simsui-native/simsui_image.S, from tools/gen_simsui_native.py) is retail's
 * .rdata and .data from RVA 0x15000; fs98_simsui_image is placed so that IMAGE(rva) lands in
 * it. Each pointer retail's relocations list there is made ours before anything runs:
 * simsui_native_tables.inc says which. */
#include <windows.h>
#include <imm.h>
#include <stdlib.h>
#include "simsui_internal.h"

/* one import from each library retail imports, so the loader has them initialised before
 * this DLL, as retail's import table does (their imports are then found by name) */
void *__stdcall fs98_simsui_mfc42_anchor(void);
void *const volatile fs98_simsui_anchors[] = {
    (void *)fs98_simsui_mfc42_anchor, (void *)GetParent, (void *)CreateCompatibleDC, (void *)ImmAssociateContext,
    (void *)malloc,
};

uint8_t *fs98_simsui_image;
void *fs98_simsui_imports[SIMSUI_IMPORT_COUNT];
extern uint8_t fs98_simsui_block[];
/* the image's extent (the relocations themselves are taken in bind_image) */
#define SIMSUI_RELOC_DATA(site, target)
#define SIMSUI_RELOC_CODE(site, routine)
#define SIMSUI_RELOC_IMPORT(site, index)
#define SIMSUI_RELOC_NONE(site)
#include "simsui_native_tables.inc"
#undef SIMSUI_RELOC_DATA
#undef SIMSUI_RELOC_CODE
#undef SIMSUI_RELOC_IMPORT
#undef SIMSUI_RELOC_NONE

#define PROCESS_ATTACHES 0x213e8                 /* the CRT's count of attaches */
#define ADJUST_FDIV 0x213ec
#define ATEXIT_END_AT 0x213f0
#define ATEXIT_BEGIN_AT 0x213f4
#define INITIALISERS 0x20000                     /* _initterm's table, to 0x2000c */
#define INITIALISERS_END 0x2000c
#define RAW_DLL_MAIN 0x20020

static const struct { const char *dll, *name; } s_import_names[SIMSUI_IMPORT_COUNT] = {
#define SIMSUI_IMPORT(index, ident, dll, name, slot, pop, thiscall, arguments) [index] = {dll, name},
#include "simsui_tables.inc"
#undef SIMSUI_IMPORT
};

static uint32_t ordinal_of(const char *digits)
{
    uint32_t value = 0;
    while (*digits >= '0' && *digits <= '9') value = value * 10 + (uint32_t)(*digits++ - '0');
    return value;
}
/* every import, by name or (MFC42's) ordinal; 0 when one cannot be had */
static int resolve_imports(void)
{
    uint32_t index;
    for (index = 0; index < SIMSUI_IMPORT_COUNT; ++index) {
        HMODULE library = LoadLibraryA(s_import_names[index].dll);
        const char *name = s_import_names[index].name;
        if (!library) return 0;
        fs98_simsui_imports[index] = name[0] == '#'
            ? (void *)GetProcAddress(library, MAKEINTRESOURCEA(ordinal_of(name + 1)))
            : (void *)GetProcAddress(library, name);
        if (!fs98_simsui_imports[index]) return 0;
    }
    return 1;
}

/* retail's pointers in the image made ours */
static void bind_image(void)
{
    fs98_simsui_image = (uint8_t *)((uintptr_t)fs98_simsui_block - SIMSUI_IMAGE_START);
#define SIMSUI_RELOC_DATA(site, target) D32(fs98_simsui_image, site) = (uint32_t)(uintptr_t)IMAGE(target);
#define SIMSUI_RELOC_CODE(site, routine) D32(fs98_simsui_image, site) = (uint32_t)(uintptr_t)(void *)routine;
#define SIMSUI_RELOC_IMPORT(site, index) D32(fs98_simsui_image, site) = (uint32_t)(uintptr_t)fs98_simsui_imports[index];
#define SIMSUI_RELOC_NONE(site) D32(fs98_simsui_image, site) = 0;
#include "simsui_native_tables.inc"
#undef SIMSUI_RELOC_DATA
#undef SIMSUI_RELOC_CODE
#undef SIMSUI_RELOC_IMPORT
#undef SIMSUI_RELOC_NONE
}

/* 0x204833c0, the CRT's DLL initialisation: on attach, _adjust_fdiv kept, the onexit table
 * made (0x80 bytes) and the static initialisers run; on detach, the onexit handlers run last
 * first and the table freed */
static int32_t crt_init(uint32_t instance, uint32_t reason, void *reserved)
{
    (void)instance; (void)reserved;
    if (reason == 0) {
        if (I32(fs98_simsui_image, PROCESS_ATTACHES) <= 0) return 0;
        --I32(fs98_simsui_image, PROCESS_ATTACHES);
    }
    D32(fs98_simsui_image, ADJUST_FDIV) = *(uint32_t *)fs98_simsui_imports[SIMSUI_IMPORT__adjust_fdiv];
    if (reason == 1) {
        void **table = CALL_C(malloc, void *, (uint32_t))(0x80);
        PTR(fs98_simsui_image, ATEXIT_BEGIN_AT) = table;
        if (!table) return 0;
        table[0] = 0;
        PTR(fs98_simsui_image, ATEXIT_END_AT) = PTR(fs98_simsui_image, ATEXIT_BEGIN_AT);
        CALL_C(_initterm, void, (void *, void *))(IMAGE(INITIALISERS), IMAGE(INITIALISERS_END));
        ++I32(fs98_simsui_image, PROCESS_ATTACHES);
        return 1;
    }
    if (reason == 0 && PTR(fs98_simsui_image, ATEXIT_BEGIN_AT)) {
        void (**at)(void) = (void (**)(void))PTR(fs98_simsui_image, ATEXIT_END_AT) - 1;
        while (at >= (void (**)(void))PTR(fs98_simsui_image, ATEXIT_BEGIN_AT)) {
            if (*at) (*at)();
            --at;
        }
        CALL_C(free, void, (void *))(PTR(fs98_simsui_image, ATEXIT_BEGIN_AT));
        D32(fs98_simsui_image, ATEXIT_BEGIN_AT) = 0;
    }
    return 1;
}

/* the entry: before anything of retail's, the imports and the image; then 0x20483490, the
 * CRT's _DllMainCRTStartup: MFC's raw DllMain, the CRT, DllMain, and the reverse on detach */
BOOL WINAPI fs98_simsui_entry(HINSTANCE module, DWORD reason, void *reserved)
{
    uint32_t instance = (uint32_t)(uintptr_t)module;
    int32_t (SC *raw)(uint32_t, uint32_t, void *);
    int32_t result = 1;
    if (reason == DLL_PROCESS_ATTACH) {
        if (!resolve_imports()) return FALSE;
        bind_image();
    }
    if (reason == 0 && !I32(fs98_simsui_image, PROCESS_ATTACHES)) return FALSE;
    raw = (int32_t (SC *)(uint32_t, uint32_t, void *))PTR(fs98_simsui_image, RAW_DLL_MAIN);
    if (reason == 1 || reason == 2) {
        if (raw) result = raw(instance, reason, reserved);
        if (!result || !crt_init(instance, reason, reserved)) return FALSE;
    }
    result = fs98_simsui_dll_main(instance, reason, reserved);
    if (reason == 1 && !result) crt_init(instance, 0, reserved);
    if (reason == 0 || reason == 3) {
        if (!crt_init(instance, reason, reserved)) result = 0;
        if (result && raw) result = raw(instance, reason, reserved);
    }
    return result;
}
