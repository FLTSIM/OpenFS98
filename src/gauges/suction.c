/* CESSSUC.GAU -- Cessna_182.Suction, module 19.
 * All 352 bytes of retail .text are lifecycle wrappers and a [2,8] clamp.
 * PANELS draws the element tree and installs its mouse rectangles. */
#include <windows.h>
#include <stdint.h>

union suction_data {
    uint8_t bytes[0x298];
    uint32_t words[0x298 / 4];
};
union suction_data suction_image = {{
#include "suction_data.inc"
}};

__asm__(".globl _ImportTable\n.set _ImportTable, _suction_image + 0x260\n"
        ".globl _Linkage\n.set _Linkage, _suction_image + 0x270\n");

typedef uint32_t (__stdcall *panel_one)(uint32_t);
typedef uint32_t (__stdcall *panel_two)(uint32_t, uint32_t);
typedef uint32_t (__stdcall *panel_three)(uint32_t, uint32_t, uint32_t);

static uint32_t panel_service(unsigned offset)
{
    const uint32_t *panels = (const uint32_t *)(uintptr_t)suction_image.words[0x264 / 4];
    return panels[offset / 4];
}

/* 0x1020: unordered comparisons follow the lower-bound replacement path. */
static double __stdcall suction_clamp(double value)
{
    if (value > 8.0)
        value = 8.0;
    if (!(value >= 2.0))
        value = 2.0;
    return value;
}

/* 0x1060: each service may change the header, root or PANELS import. */
static uint32_t __stdcall suction_install(uint32_t context)
{
    ((panel_two)(uintptr_t)panel_service(0x64))(suction_image.words[0x1d0 / 4], context);
    ((panel_three)(uintptr_t)panel_service(0x80))(
        (uint32_t)(uintptr_t)(suction_image.bytes + 0x1d8),
        (uint32_t)(uintptr_t)(suction_image.bytes + 0x18),
        suction_image.words[0x1bc / 4] + 4);
    return ((panel_two)(uintptr_t)panel_service(0x84))(
        (uint32_t)(uintptr_t)(suction_image.bytes + 0x1d8),
        (uint32_t)(uintptr_t)(suction_image.bytes + 0x18));
}

static uint32_t __stdcall suction_unmark(void)
{
    return ((panel_one)(uintptr_t)panel_service(0x68))(suction_image.words[0x1d0 / 4]);
}

static uint32_t __stdcall suction_prepare(void)
{
    return ((panel_one)(uintptr_t)panel_service(0x6c))(suction_image.words[0x1d0 / 4]);
}

static uint32_t __stdcall suction_update(uint32_t context)
{
    return ((panel_two)(uintptr_t)panel_service(0x70))(suction_image.words[0x1d0 / 4], context);
}

static uint32_t __stdcall suction_draw(void)
{
    return ((panel_one)(uintptr_t)panel_service(0x74))(suction_image.words[0x1d0 / 4]);
}

/* 0x1100: remove the mouse area before reading the current root. */
static uint32_t __stdcall suction_release(void)
{
    ((panel_two)(uintptr_t)panel_service(0x88))(
        (uint32_t)(uintptr_t)(suction_image.bytes + 0x1d8),
        (uint32_t)(uintptr_t)(suction_image.bytes + 0x18));
    return ((panel_one)(uintptr_t)panel_service(0x7c))(suction_image.words[0x1d0 / 4]);
}

static void __stdcall suction_noop(void) { }

static uint32_t __stdcall suction_failures(void)
{
    const uint32_t *root = (const uint32_t *)(uintptr_t)suction_image.words[0x20 / 4];
    return ((panel_one)(uintptr_t)panel_service(0x60))(*root);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
#include "suction_pointers.inc"
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
