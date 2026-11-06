/* 737WARNG.GAU -- 737-400.Warning_Enunciators, module 19.
 * Retail .text: 0x20d21000..0x20d210d0. The element and bitmap are data;
 * PANELS implements all drawing. See docs/modules/gauges.md. */
#include <windows.h>
#include <stdint.h>

union warning_data {
    uint8_t bytes[0xfc];
    uint32_t words[0xfc / 4];
};

union warning_data warning_image = {{
#include "warning_data.inc"
}};

/* Keep the exported tables at their retail offsets within the image. */
__asm__(".globl _ImportTable\n.set _ImportTable, _warning_image + 0xc8\n"
        ".globl _Linkage\n.set _Linkage, _warning_image + 0xd8\n");

typedef uint32_t (__stdcall *panel_one)(uint32_t);
typedef uint32_t (__stdcall *panel_two)(uint32_t, uint32_t);

static uint32_t panel_service(unsigned offset)
{
    const uint32_t *panels = (const uint32_t *)(uintptr_t)warning_image.words[0xcc / 4];
    return panels[offset / 4];
}

/* 0x1020: the element root is read afresh for every callback. */
static uint32_t __stdcall warning_install(uint32_t context)
{
    return ((panel_two)(uintptr_t)panel_service(0x64))(
        warning_image.words[0xc0 / 4], context);
}

/* 0x1040, 0x1050, 0x1060, 0x1080 and 0x1090. */
static uint32_t __stdcall warning_unmark(void)
{
    return ((panel_one)(uintptr_t)panel_service(0x68))(warning_image.words[0xc0 / 4]);
}

static uint32_t __stdcall warning_prepare(void)
{
    return ((panel_one)(uintptr_t)panel_service(0x6c))(warning_image.words[0xc0 / 4]);
}

static uint32_t __stdcall warning_update(uint32_t context)
{
    return ((panel_two)(uintptr_t)panel_service(0x70))(
        warning_image.words[0xc0 / 4], context);
}

static uint32_t __stdcall warning_draw(void)
{
    return ((panel_one)(uintptr_t)panel_service(0x74))(warning_image.words[0xc0 / 4]);
}

static uint32_t __stdcall warning_release(void)
{
    return ((panel_one)(uintptr_t)panel_service(0x7c))(warning_image.words[0xc0 / 4]);
}

/* 0x10a0: Linkage init and deinit share this void no-op. */
static void __stdcall warning_noop(void) { }

/* 0x10b0: use the header's current root-pointer address, then dereference it. */
static uint32_t __stdcall warning_failures(void)
{
    const uint32_t *root = (const uint32_t *)(uintptr_t)warning_image.words[0x28 / 4];
    return ((panel_one)(uintptr_t)panel_service(0x60))(*root);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
#include "warning_pointers.inc"
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
