/* GLOBAL.DLL -- module id 2.
 *
 * The first module reimplemented, because it is the smallest meaningful one:
 * the retail DLL has 54 bytes of .text, all of it boilerplate (a minimal
 * DllMain plus one no-op reached through both Linkage service slots).  Its
 * actual content is data -- a 4776 byte block that every other module reads
 * and writes as byte offsets from the Linkage pointer it was handed at init.
 *
 * So GLOBAL has no behaviour to reverse: get the ABI and the power-on defaults
 * right and it is finished.  Everything still to be learned about it is the
 * *meaning* of those offsets, which gets filled in from the other modules as
 * they are worked through -- each one that reads [global + N] names a field.
 *
 * Known offsets so far:
 *   +0x365  byte, read by SLEW service slot 4 as an early-out flag
 */

#include <windows.h>
#include "fs98/image_end.h"

#include "fs98/global.h"
#include "fs98/linkage.h"

#define FS98_GLOBAL_HEAD 0xc            /* the Linkage header: id, init, deinit */

/* Byte-emitting initialiser macros, one typed value each, little endian (slice 12's style).  AT(off) is the
 * block offset a disassembly shows; STRn(s) is a string padded with NULs to n bytes (a flat byte list cannot
 * hold a string literal, so the padded literal is indexed). */
#define AT(off) [(off) - FS98_GLOBAL_HEAD] =
#define U8(x)  (unsigned char)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR4(s)  S4(s, 0)
#define STR8(s)  S4(s, 0), S4(s, 4)
#define STR20(s) STR8(s), S4(s, 8), S4(s, 12), S4(s, 16)

/* Declared before the no-op and before Linkage so it lands at .text+0, where
 * the retail module's entry point is. */
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}

static void global_noop(void) { }

/* Laid out to match the retail .data exactly: the Linkage header sits at the
 * base of the block and the shared state follows it. */
struct global_module {
    uint32_t id;
    void   (*init)(void);
    void   (*deinit)(void);
    unsigned char state[FS98_GLOBAL_SIZE - FS98_GLOBAL_HEAD];
};

struct global_module Linkage = {
    FS98_MOD_GLOBAL,
    global_noop,
    global_noop,
    {
    /* The power-on defaults: the 194 non-zero bytes of retail's 4776 byte block (203 with the header), at retail's
     * offsets from the Linkage pointer; the rest is zero.  Only +0x365 (SLEW's early-out flag, zero here) has a
     * known meaning; the others are named as their readers are worked through.  The build checks the block against a committed hash,
     * and `tools/ofs data-images` compares it with retail (tools/check_data_image.py, row GLOBAL, at=_Linkage).  Values are grouped by width as they look in the
     * dump (byte, word or text); the grouping is a guess until each reader is known. */
    AT(0x0238) U8(0x0c),
    AT(0x0242) U8(0x01),
    AT(0x0278) U8(0x01),
    AT(0x0284) U8(0x01),
    AT(0x0288) U8(0x02),
    AT(0x02aa) U16(0x1fff),
    AT(0x0330) U16(0x3f50),
    AT(0x034e) U16(0x2485), U16(0x1050), U16(0x1390),
    AT(0x0362) U8(0x07),
    AT(0x0366) U8(0x01),
    AT(0x0368) U16(0x0fff),
    AT(0x0372) U8(0x64),
    AT(0x0375) U8(0x01), U8(0x01),
    AT(0x0378) U8(0x01),
    AT(0x0388) U8(0x01),
    AT(0x0390) U8(0x02),
    AT(0x03aa) U16(0x2020),
    AT(0x03b8) U16(0x7777),
    AT(0x03bc) U16(0xbbbb),
    AT(0x03dc) U8(0x0a),
    AT(0x0446) STR20("scenery\\world.vis"),
    AT(0x054c) U8(0x02),
    AT(0x0556) U8(0x01),
    AT(0x05f4) U8(0x01),
    AT(0x0610) U8(0x01),
    AT(0x0634) U8(0x64),
    AT(0x075c) U8(0x02),
    AT(0x0884) U8(0x50),
    AT(0x0887) U16(0x0150),
    AT(0x0af8) U8(0x01),
    AT(0x0c02) U16(0x0040),
    AT(0x0c04) U16(0x0040),
    AT(0x0c06) U16(0x0040),
    AT(0x0c08) U16(0x0023),
    AT(0x0c0a) U16(0x0019),
    AT(0x0c0c) U16(0x0040),
    AT(0x0c1b) U8(0x01),
    AT(0x0c1c) STR4("1.0"),
    AT(0x0c20) STR8("12:00:00"),
    AT(0x0c29) STR4("00.0"),
    AT(0x0c2e) STR4("111."),
    AT(0x0c33) STR4("12.3"),
    AT(0x0c38) STR4("321."),
    AT(0x0cba) U16(0xffff),
    AT(0x0cc0) U16(0x0014),
    AT(0x0cc2) U16(0xffff),
    AT(0x0cc8) U16(0xffff),
    AT(0x0da8) U8(0x02),
    AT(0x0dac) U8(0x01),
    AT(0x0db9) U8(0x20),
    AT(0x0dc0) U8(0x01),
    AT(0x0dc4) U8(0x01),
    AT(0x0dc6) U8(0x02),
    AT(0x0e0e) U16(0x0001), U16(0x0007), U16(0x0001), U16(0x000a), U16(0x000a), U16(0x000f), U16(0x000f), U16(0x0006), U16(0x000f), U16(0x000f), U16(0x0002), U16(0x000f), U16(0x000f), U16(0x000a), U16(0x000a), U16(0x0008), U16(0x000f),
    AT(0x0e58) U8(0x01),
    AT(0x0e62) U8(0x01),
    AT(0x0e64) U8(0x02),
    AT(0x0e80) U16(0x03e8),
    AT(0x0e88) U16(0x07dc), U16(0x07dc),
    AT(0x0e8d) U8(0x0f),
    AT(0x0e8e) U16(0x3f50),
    AT(0x0eb2) U16(0xd8f1),
    AT(0x0eb6) U16(0xd8f1),
    AT(0x0eba) U16(0xd8f1),
    AT(0x0ec1) U8(0x14),
    AT(0x0ec6) U16(0x3f50),
    AT(0x0f34) U16(0xd8f1),
    AT(0x0f38) U16(0xd8f1),
    AT(0x0f3c) U16(0xd8f1),
    AT(0x0f43) U8(0x14),
    AT(0x0f48) U16(0x3f50),
    AT(0x0f8c) U16(0x07dc),
    AT(0x101c) U16(0xd8f1),
    AT(0x1020) U16(0xd8f1),
    AT(0x1024) U16(0xd8f1),
    AT(0x102b) U8(0x14),
    AT(0x1030) U16(0x3f50),
    AT(0x1074) U16(0x07dc),
    AT(0x1104) U16(0xd8f1),
    AT(0x1108) U16(0xd8f1),
    AT(0x110c) U16(0xd8f1),
    AT(0x1113) U8(0x14),
    AT(0x1118) U16(0x3f50),
    AT(0x115c) U16(0x07dc),
    AT(0x1176) U8(0x01),
    AT(0x11a4) U16(0x691f),
    AT(0x11a8) U16(0x6910),
    AT(0x11ad) U8(0x69),
    AT(0x11b6) U8(0x01),
    AT(0x11b8) U8(0x01),
    AT(0x11bc) U8(0x01),
    AT(0x11dc) U8(0x01),
    AT(0x120c) U16(0xca20),
    AT(0x1214) U8(0x02),
    AT(0x122c) U8(0x01),
    AT(0x1230) U8(0x01),
    AT(0x123c) U8(0x0a),
    AT(0x1259) U8(0x06),
    }
};
FS98_IMAGE_END(Linkage)
_Static_assert(sizeof(struct global_module) == FS98_GLOBAL_SIZE, "GLOBAL is 0x12a8 bytes");
_Static_assert(__builtin_offsetof(struct global_module, state) == FS98_GLOBAL_HEAD, "state follows the Linkage header");

/* GLOBAL imports nothing, so unlike every other module it exports no
 * ImportTable -- matching the retail DLL, which has a single export. */

