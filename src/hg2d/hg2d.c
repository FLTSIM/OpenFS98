/* HG2D, module 13. Incomplete: texture, palette and drawing slices are installed.
 * Keep the retail .data layout for whole-image differential testing.
 * See docs/modules/hg2d.md. */
#include <windows.h>
#include <stdint.h>

#define STDCALL __attribute__((stdcall))
#define DATA_SIZE 0x28cd0
#define POOL 0x5f60u
#define POOL_END 0x8800u

uint8_t hg2d_image[DATA_SIZE] __attribute__((aligned(4))) = {
#include "hg2d_data.inc"
};
#define WORD(offset) (*(uint32_t *)(hg2d_image + (offset)))
#define ADDRESS(pointer) ((uint32_t)(uintptr_t)(pointer))
#define POINTER(value) ((void *)(uintptr_t)(value))
typedef uint32_t (STDCALL *call_one)(uint32_t);
typedef uint32_t (STDCALL *call_four)(uint32_t, uint32_t, uint32_t, uint32_t);
#define HOST_FREE (*(call_one *)((uintptr_t)WORD(4) + 0x214))
#define HOST_ALLOC (*(call_four *)((uintptr_t)WORD(4) + 0x20c))

/* .text+0x5ec20. Only the pixels word marks a record occupied. */
static uint32_t *texture_slot(void)
{
    uint32_t offset;
    for (offset = POOL; offset < POOL_END; offset += 0x34)
        if (WORD(offset) == 0)
            return (uint32_t *)(hg2d_image + offset);
    return NULL;
}

/* .text+0x5ebf0, +0x6c: low byte of each palette word, in place. */
static uint32_t STDCALL texture_remap(uint8_t *pixels)
{
    uint32_t index;
    for (index = 0; index < 0x10000; index++)
        pixels[index] = hg2d_image[0x34c0 + pixels[index] * 2];
    return ADDRESS(pixels);
}

/* .text+0x5ec50, +0xac. */
static uint32_t STDCALL texture_create(const uint32_t *descriptor, uint32_t flags)
{
    uint32_t *texture;
    if (descriptor[1] != 0x100 || descriptor[0] != 1 || flags != 0)
        return 0;
    texture = texture_slot();
    if (!texture)
        return 0;
    texture[0] = descriptor[6];
    if (texture[0] == 0)
        HOST_ALLOC(4, 0x10000, ADDRESS(texture), 0);
    return texture[0] ? ADDRESS(texture) : 0;
}

/* .text+0x5ecb0, +0xa8. Third argument controls the record's second byte. */
static uint32_t STDCALL texture_load(const uint32_t *name, uint32_t selector, uint32_t flags)
{
    uint32_t *texture = texture_slot();
    call_four load;
    if (!texture)
        return 0;
    HOST_ALLOC(4, 0x10000, ADDRESS(texture), 0);
    if (!texture[0])
        return 0;
    load = *(call_four *)((uintptr_t)WORD(0x1c) + 0x1c);
    if (!load(texture[0], ADDRESS(name), selector, 0)) {
        HOST_FREE(ADDRESS(texture));
        texture[0] = 0;
        return 0;
    }
    texture_remap(POINTER(texture[0]));
    if (*name == 0x65646973) /* "side" prefix */
        texture[1] |= 0x100000;
    else
        texture[1] &= ~0x100000u;
    texture[1] = (texture[1] & ~0xff00u) | (flags ? 0xff00u : 0);
    return ADDRESS(texture);
}

static int texture_inside(uint32_t handle)
{
    /* Retail checks the range only, including misaligned handles. */
    return handle >= ADDRESS(hg2d_image + POOL) && handle < ADDRESS(hg2d_image + POOL_END);
}

/* .text+0x5d6e0: hardware texture release, also updates shared accounting. */
static uint32_t texture_release(uint32_t *texture)
{
    uint32_t result = texture[3], index;
    if (result) {
        uint32_t *object = POINTER(result);
        uint32_t *vtable = POINTER(object[0]);
        for (index = 0; index < 8; index++)
            WORD(0x8820 + index * 4) = 0;
        ((call_one)(uintptr_t)vtable[2])(result);
        index = texture[7];
        WORD(0x8800 + index * 4)--;
        result = WORD(0x289ec) - (1u << (((8u - index) * 2) & 31));
        WORD(0x289ec) = result;
    }
    texture[3] = texture[2] = texture[7] = 0;
    return result;
}

uint32_t STDCALL __attribute__((used)) texture_destroy_body(uint32_t handle, uint32_t flags, uint32_t entry_eax)
{
    uint32_t *texture = POINTER(handle), result = entry_eax;
    if (!texture_inside(handle))
        return result;
    result = texture[3];
    if (result)
        result = texture_release(texture);
    if (texture[0] && !(flags & 1))
        result = HOST_FREE(ADDRESS(texture));
    texture[0] = texture[1] = texture[2] = texture[3] = 0;
    return result;
}

/* .text+0x5edc0, +0xb4: mark accessed and return pixels. */
static uint32_t STDCALL texture_pixels(uint32_t handle)
{
    uint32_t *texture = POINTER(handle);
    if (!texture_inside(handle))
        return 0;
    texture[1] |= 0x10000;
    return texture[0];
}

uint32_t STDCALL __attribute__((used)) texture_update_body(uint32_t handle, uint32_t flags, uint32_t entry_eax)
{
    uint32_t *texture = POINTER(handle), result = entry_eax;
    if (!texture_inside(handle))
        return result;
    texture[1] &= ~0x10000u;
    result = (result & ~0xffu) | (flags & 0xff);
    if (flags & 3) {
        if (flags & 2)
            result = texture_remap(POINTER(texture[0]));
        texture[1] = (texture[1] & ~0x40000u) | 0x20000;
    }
    return result;
}

/* The two services preserve incoming eax on their range-check early exit. */
#define EAX_ENTRY(name, body) \
    void name(void); \
    __asm__(".text\n_" #name ":\n\tpushl %eax\n\t" \
            "pushl 12(%esp)\n\tpushl 12(%esp)\n\t" \
            "call _" #body "@12\n\tret $8\n")
EAX_ENTRY(texture_destroy, texture_destroy_body);
EAX_ENTRY(texture_update, texture_update_body);

#include "hg2d_palette.inc"
#include "hg2d_draw.inc"
#include "hg2d_projective.inc"
#include "hg2d_matrix.inc"
#include "hg2d_polygon.inc"
#include "hg2d_fill.inc"
#include "hg2d_textured.inc"
#include "hg2d_perspective.inc"
#include "hg2d_affine.inc"
#include "hg2d_perspective_fill.inc"
#include "hg2d_gouraud.inc"
#include "hg2d_blit.inc"
#include "hg2d_disc.inc"

__asm__(".globl _ImportTable\n.set _ImportTable, _hg2d_image\n"
        ".globl _Linkage\n.set _Linkage, _hg2d_image + 0x38\n");

/* +0x08, 0x2073104a: retail's teardown is a register-preserving no-op. */
void module_deinit(void);
__asm__(".text\n_module_deinit:\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret\n");

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
#define HG2D_DATA_POINTER(at, target) WORD(at) = ADDRESS(hg2d_image + (target));
#include "hg2d_pointers.inc"
#undef HG2D_DATA_POINTER
        WORD(0x38 + 0x08) = ADDRESS(module_deinit);
        WORD(0x38 + 0x18) = ADDRESS(draw_flush);
        WORD(0x38 + 0x1c) = ADDRESS(draw_point);
        WORD(0x38 + 0x20) = ADDRESS(draw_nop);
        WORD(0x38 + 0x24) = ADDRESS(draw_depth_point);
        WORD(0x38 + 0x28) = ADDRESS(draw_text);
        WORD(0x38 + 0x2c) = ADDRESS(draw_horizontal);
        WORD(0x38 + 0x30) = ADDRESS(draw_vertical);
        WORD(0x38 + 0x34) = ADDRESS(draw_line);
        WORD(0x38 + 0x38) = ADDRESS(draw_fixed_line);
        WORD(0x38 + 0x3c) = ADDRESS(draw_frame);
        WORD(0x38 + 0x40) = ADDRESS(polygon_close);
        WORD(0x38 + 0x44) = ADDRESS(polygon_vertex);
        WORD(0x38 + 0x48) = ADDRESS(textured_vertex);
        WORD(0x38 + 0x4c) = ADDRESS(affine_close);
        WORD(0x38 + 0x50) = ADDRESS(perspective_vertex);
        WORD(0x38 + 0x54) = ADDRESS(perspective_close);
        WORD(0x38 + 0x5c) = ADDRESS(palette_realize);
        WORD(0x38 + 0x60) = ADDRESS(palette_refresh_entry);
        WORD(0x38 + 0x64) = ADDRESS(palette_force);
        WORD(0x38 + 0x70) = ADDRESS(colour_convert);
        WORD(0x38 + 0x74) = ADDRESS(gouraud_vertex);
        WORD(0x38 + 0x78) = ADDRESS(gouraud_close);
        WORD(0x38 + 0x7c) = ADDRESS(palette_select);
        WORD(0x38 + 0x80) = ADDRESS(blit_span);
        WORD(0x38 + 0x84) = ADDRESS(disc);
        WORD(0x38 + 0x88) = ADDRESS(colour_remap);
        WORD(0x38 + 0x6c) = ADDRESS(texture_remap);
        WORD(0x38 + 0xa8) = ADDRESS(texture_load);
        WORD(0x38 + 0xac) = ADDRESS(texture_create);
        WORD(0x38 + 0xb0) = ADDRESS(texture_destroy);
        WORD(0x38 + 0xb4) = ADDRESS(texture_pixels);
        WORD(0x38 + 0xb8) = ADDRESS(texture_update);
    }
    return TRUE;
}
