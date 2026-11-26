/* An MDL's data image, one source for all eleven models: retail's .data at its RVA, built from the model's
 * committed src/mdl/images/<NAME>/ (found through -I: layout.h, and data.inc, the body).  The pointers are
 * address constants, so the linker emits retail's seven relocations: header +0x4 (the body), header +0x10 and
 * Linkage +0x4, +0x8, +0x18, +0x1c, +0x20 (the module's code; the symbols are in mdl_module.c and mdl_crash_entry.S).
 * A normal model's image is in .data; a crash model's (MDL_CRASH) in its own section, .mdlimg. */
#include <stddef.h>
#include "fs98/image_end.h"
#include <stdint.h>

#include "layout.h"

#ifdef MDL_CRASH
#define MDL_NOP "_crash_nop"
#define MDL_UPDATE "_crash_update_entry"
#define MDL_ATTACH "_crash_attach@4"
#define MDL_STATE "_crash_init@4"
#define MDL_DETACH "_crash_detach"
#define MDL_IN __attribute__((section(".mdlimg")))
#else
#define MDL_NOP "_mdl_nop"
#define MDL_UPDATE "_mdl_update@4"
#define MDL_ATTACH "_mdl_attach@4"
#define MDL_STATE "_mdl_state@4"
#define MDL_DETACH "_mdl_detach"
#define MDL_IN
#endif

extern char mdl_nop __asm__(MDL_NOP);
extern char mdl_update __asm__(MDL_UPDATE);
extern char mdl_attach __asm__(MDL_ATTACH);
extern char mdl_state __asm__(MDL_STATE);
extern char mdl_detach __asm__(MDL_DETACH);

#pragma pack(push, 1)
struct mdl_image_layout {
    uint8_t lead[MDL_DATA_RVA];
    uint8_t prefix[0x10];
    struct { uint32_t w0, data, w2, w3, update, w5, w6, w7, w8; } header;
    uint8_t body[MDL_BODY_SIZE];
    struct { uint32_t w0, nop1, nop2, w3, w4, w5, attach, state, detach; } linkage;
    uint8_t tail[0x14];
};
#pragma pack(pop)

_Static_assert(sizeof(struct mdl_image_layout) == MDL_DATA_RVA + MDL_DATA_SIZE, "image size");
_Static_assert(sizeof(((struct mdl_image_layout *)0)->header) == 0x24, "header size");
_Static_assert(sizeof(((struct mdl_image_layout *)0)->linkage) == 0x24, "Linkage size");
_Static_assert(offsetof(struct mdl_image_layout, lead) == 0, "lead");
_Static_assert(offsetof(struct mdl_image_layout, prefix) == MDL_DATA_RVA, "prefix");
_Static_assert(offsetof(struct mdl_image_layout, header) == MDL_HEADER_RVA, "header");
_Static_assert(offsetof(struct mdl_image_layout, header.data) == MDL_HEADER_RVA + 0x4, "header data pointer");
_Static_assert(offsetof(struct mdl_image_layout, header.update) == MDL_HEADER_RVA + 0x10, "header update pointer");
_Static_assert(offsetof(struct mdl_image_layout, body) == MDL_DATA_RVA + 0x34, "body");
_Static_assert(offsetof(struct mdl_image_layout, linkage) == MDL_LINKAGE_RVA, "Linkage");
_Static_assert(offsetof(struct mdl_image_layout, linkage.nop1) == MDL_LINKAGE_RVA + 0x4, "Linkage +0x4");
_Static_assert(offsetof(struct mdl_image_layout, linkage.nop2) == MDL_LINKAGE_RVA + 0x8, "Linkage +0x8");
_Static_assert(offsetof(struct mdl_image_layout, linkage.attach) == MDL_LINKAGE_RVA + 0x18, "Linkage +0x18");
_Static_assert(offsetof(struct mdl_image_layout, linkage.state) == MDL_LINKAGE_RVA + 0x1c, "Linkage +0x1c");
_Static_assert(offsetof(struct mdl_image_layout, linkage.detach) == MDL_LINKAGE_RVA + 0x20, "Linkage +0x20");
_Static_assert(offsetof(struct mdl_image_layout, tail) == MDL_LINKAGE_RVA + 0x24, "tail");
_Static_assert(MDL_IMPORT_RVA >= MDL_DATA_RVA + 0x34 && MDL_IMPORT_RVA < MDL_LINKAGE_RVA, "ImportTable in the body");

#define IMAGE(rva) ((uint32_t)(uintptr_t)&mdl_image + (rva))
#define CODE(symbol) ((uint32_t)(uintptr_t)&(symbol))

__attribute__((aligned(16))) MDL_IN struct mdl_image_layout mdl_image = {
    .lead = {MDL_LEAD},
    .prefix = {MDL_PREFIX},
    .header = {MDL_HEADER_W0, IMAGE(MDL_DATA_POINTER), MDL_HEADER_W2, MDL_HEADER_W3, CODE(mdl_update),
               MDL_HEADER_W5, MDL_HEADER_W6, MDL_HEADER_W7, MDL_HEADER_W8},
    .body = {
#include "data.inc"
    },
    .linkage = {MDL_LINKAGE_W0, CODE(mdl_nop), CODE(mdl_nop), MDL_LINKAGE_W3, MDL_LINKAGE_W4, MDL_LINKAGE_W5,
                CODE(mdl_attach), CODE(mdl_state), CODE(mdl_detach)},
    .tail = {MDL_TAIL},
};
FS98_IMAGE_END(mdl_image)

#define MDL_STRING_(x) #x
#define MDL_STRING(x) MDL_STRING_(x)
/* the exports and the image's .data start, relative to the image */
__asm__(".globl _ImportTable\n.set _ImportTable, _mdl_image + " MDL_STRING(MDL_IMPORT_RVA)
        "\n.globl _Linkage\n.set _Linkage, _mdl_image + " MDL_STRING(MDL_LINKAGE_RVA)
        "\n.globl _mdl_header\n.set _mdl_header, _mdl_image + " MDL_STRING(MDL_HEADER_RVA)
        "\n.globl _mdl_data\n.set _mdl_data, _mdl_image + " MDL_STRING(MDL_DATA_RVA) "\n");
