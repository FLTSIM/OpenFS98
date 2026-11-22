/* The native SIMSUI.DLL's image of retail's .rdata and .data (RVA 0x15000..0x21400), as committed typed C (simsui_image.inc).
 * bind_image() in simsui_native.c makes its pointers ours at attach.  simsui_labels.h (tools/gen_simsui_native.py, from the export
 * table, found through -Ibin/simsui-native) holds the exported data's labels as file-scope asm, assembled in this unit because COFF
 * cannot alias an undefined symbol plus an offset.  Built with -fno-ident (the old assembly emitted no .ident). */
#include <stdint.h>

__attribute__((section(".data"), aligned(4096)))
uint8_t fs98_simsui_block[0xc400] = {
#include "simsui_image.inc"
};

#include "simsui_labels.h"
