/* BKDME.GAU's .data image, decompiled: every non-zero value of retail's .data (0xa5c bytes at RVA 0x3000) at
 * retail's offsets, one typed value per macro.  Included by src/gauges/gauge.h as the initialiser of gauge_data, a byte array.  The
 * slots that hold addresses (rebind.inc) are zero here and are installed at attach.  Zero runs of 16 bytes or more are not
 * written.  Named: the gauge header (Linkage +0x18), the element tree, the ImportTable and the Linkage; the rest is typed values
 * marked "meaning not yet known", with the strings as literals and the doubles the code reads as F64.
 *
 * Macros: AT(off) is the offset a disassembly shows; U8/U16/U32 emit one little-endian value; F64(bits) a double from its bit
 * pattern; STRn(s) is a string padded with NULs to n bytes and S4/SB its 4-byte and 1-byte pieces. */
#define AT(off) [(off)] =
#define U8(x)  (uint8_t)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define U32(x) U16(x), U16((x) >> 16)
#define F64(bits) U32((uint32_t)(bits)), U32((uint32_t)((bits) >> 32))
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Bendix/King DME"),  /* the gauge name */
/* +0x0018  the gauge header (Linkage +0x18), 0x2c bytes */
U32(0x00000262),  /* header +0x00: version word */
U32(0x00000000),  /* header +0x04: the name (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x08: the element-list slot (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x0c: callback 1 of 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x10: callback 2 of 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x14: callback 3 of 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x18: callback 4 of 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x1c: callback 5 of 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x20: callback 6 of 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x24: callback 7 of 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* header +0x28: set at run time */
/* +0x0044  meaning not yet known */
U8(0x5a),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x0d),
/* +0x0090  element 1 of 8: type 2 (text, 0xbc bytes) */
AT(0x90) U32(0x00000002),  /* element 1: type */
/* +0x0094  meaning not yet known */
U32(0xffffffff), U32(0x0000002b), U8(0x12),
AT(0xb4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00c0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x00e8  meaning not yet known */
AT(0xe8) U8(0x23),
/* +0x0104  meaning not yet known */
AT(0x104) U16(0x0163),
AT(0x148) U32(0x00000000),  /* element 1: text buffer */
/* +0x014c  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x0000001b), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x188) STR12("Courier New"),
/* +0x01a8  meaning not yet known */
AT(0x1a8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x04),
/* +0x01c8  element 2 of 8: type 2 (text, 0xbc bytes) */
AT(0x1c8) U32(0x00000002),  /* element 2: type */
/* +0x01cc  meaning not yet known */
U32(0xffffffff), U32(0x0000003a), U8(0x31),
AT(0x1ec) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01f8  meaning not yet known */
U8(0x06),
AT(0x280) U32(0x00000000),  /* element 2: text buffer */
/* +0x0284  meaning not yet known */
U32(0x00000000), U32(0x00000032), U32(0x0000001b), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x2c0) STR12("Courier New"),
/* +0x02e0  meaning not yet known */
AT(0x2e0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x0300  element 3 of 8: type 2 (text, 0xbc bytes) */
AT(0x300) U32(0x00000002),  /* element 3: type */
/* +0x0304  meaning not yet known */
U32(0xffffffff), U32(0x00000083), U8(0x12),
AT(0x324) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0330  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x20),
/* +0x0358  meaning not yet known */
AT(0x358) U8(0x24),
/* +0x0374  meaning not yet known */
AT(0x374) U16(0x0163),
AT(0x3b8) U32(0x00000000),  /* element 3: text buffer */
/* +0x03bc  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x0000001b), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x3f8) STR12("Courier New"),
/* +0x0418  meaning not yet known */
AT(0x418) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x04),
/* +0x0438  element 4 of 8: type 2 (text, 0xbc bytes) */
AT(0x438) U32(0x00000002),  /* element 4: type */
/* +0x043c  meaning not yet known */
U32(0xffffffff), U32(0x00000091), U8(0x31),
AT(0x45c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0468  meaning not yet known */
U8(0x06),
AT(0x4f0) U32(0x00000000),  /* element 4: text buffer */
/* +0x04f4  meaning not yet known */
U32(0x00000000), U32(0x00000032), U32(0x0000001b), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x530) STR12("Courier New"),
/* +0x0550  meaning not yet known */
AT(0x550) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x0570  element 5 of 8: type 2 (text, 0xbc bytes) */
AT(0x570) U32(0x00000002),  /* element 5: type */
/* +0x0574  meaning not yet known */
U32(0xffffffff), U32(0x000000d9), U8(0x12),
AT(0x594) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05a0  meaning not yet known */
U8(0x06),
AT(0x628) U32(0x00000000),  /* element 5: text buffer */
/* +0x062c  meaning not yet known */
U32(0x00000000), U32(0x0000003b), U32(0x0000001b), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x668) STR12("Courier New"),
/* +0x0688  meaning not yet known */
AT(0x688) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x06a8  element 6 of 8: type 2 (text, 0xbc bytes) */
AT(0x6a8) U32(0x00000002),  /* element 6: type */
/* +0x06ac  meaning not yet known */
U32(0xffffffff), U32(0x000000d6), U8(0x31),
AT(0x6cc) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06d8  meaning not yet known */
U8(0x06),
AT(0x760) U32(0x00000000),  /* element 6: text buffer */
/* +0x0764  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x0000001b), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x7a0) STR12("Courier New"),
/* +0x07c0  meaning not yet known */
AT(0x7c0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x07e0  element 7 of 8: type 4 */
AT(0x7e0) U32(0x00000004),  /* element 7: type */
/* +0x07e4  meaning not yet known */
U32(0x00001100), U32(0x0000009d), U8(0x52),
AT(0x804) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0810  meaning not yet known */
U8(0x01),
/* +0x0850  meaning not yet known */
AT(0x850) U8(0x02),
/* +0x0868  the children of element 8, zero-terminated */
AT(0x868) U32(0x00000000),  /* element 8: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: end of the list */
/* +0x0888  element 8 of 8: type 0 */
U32(0x00000000),  /* element 8: type */
/* +0x088c  meaning not yet known */
U16(0x1000),
AT(0x8ac) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: children list (relocated: installed at attach, rebind.inc) */
/* +0x08b4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x08d8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x8d8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x08dc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x08f4  meaning not yet known */
AT(0x8f4) U32(0xffffffff), U16(0xa704),
/* +0x090c  meaning not yet known */
AT(0x90c) U32(0x00000001), U32(0x0000007a), U32(0x0000004b), U32(0x000000c8), U32(0x00000025), U32(0xffffffff),
U16(0xa708),
/* +0x0938  meaning not yet known */
AT(0x938) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x000000c8), U32(0x00000025), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0960  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0978  meaning not yet known */
AT(0x978) U32(0xffffffff),
/* +0x0990  meaning not yet known */
AT(0x990) U8(0x03),
/* +0x09a4  meaning not yet known */
AT(0x9a4) U32(0xffffffff),
/* +0x09d0  meaning not yet known */
AT(0x9d0) U32(0xffffffff),
AT(0x9e8) STR8("----"),
STR8("%d.%d"),
STR8("%04d"),
STR8("%03d"),
STR8("%02d"),
/* +0x0a10  meaning not yet known */
U32(0x00004d4e), U32(0x0000544b), U32(0x004e494d), U32(0x00000000),
/* +0x0a20  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0a28  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0a30  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0a3c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR8
#undef STR12
#undef STR24
