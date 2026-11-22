/* BKRADDME.GAU's .data image, decompiled: every non-zero value of retail's .data (0x56c bytes at RVA 0x3000) at
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
AT(0x0) STR24("Bendix/King Radio DME"),  /* the gauge name */
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
U8(0xaa),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x0d),
/* +0x0090  element 1 of 4: type 2 (text, 0xbc bytes) */
AT(0x90) U32(0x00000002),  /* element 1: type */
/* +0x0094  meaning not yet known */
U32(0xffffffff), U32(0x00000014), U8(0x17),
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
U32(0x00000000), U32(0x0000006e), U32(0x0000001e), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x188) STR12("Courier New"),
/* +0x01a8  meaning not yet known */
AT(0x1a8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x07),
/* +0x01c8  element 2 of 4: type 2 (text, 0xbc bytes) */
AT(0x1c8) U32(0x00000002),  /* element 2: type */
/* +0x01cc  meaning not yet known */
U32(0xffffffff), U32(0x00000091), U8(0x17),
AT(0x1ec) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01f8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x20),
/* +0x0220  meaning not yet known */
AT(0x220) U8(0x24),
/* +0x023c  meaning not yet known */
AT(0x23c) U16(0x0163),
AT(0x280) U32(0x00000000),  /* element 2: text buffer */
/* +0x0284  meaning not yet known */
U32(0x00000000), U32(0x0000006e), U32(0x0000001e), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x2c0) STR12("Courier New"),
/* +0x02e0  meaning not yet known */
AT(0x2e0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x07),
/* +0x0300  element 3 of 4: type 4 */
AT(0x300) U32(0x00000004),  /* element 3: type */
/* +0x0304  meaning not yet known */
U32(0x00001100), U32(0x00000185), U8(0x2a),
AT(0x324) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x032c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0370  meaning not yet known */
AT(0x370) U8(0x02),
/* +0x0388  the children of element 4, zero-terminated */
AT(0x388) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0398  element 4 of 4: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x039c  meaning not yet known */
U16(0x1000),
AT(0x3bc) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x03c4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03e8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x3e8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x03ec  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0404  meaning not yet known */
AT(0x404) U32(0xffffffff), U16(0xa704),
/* +0x041c  meaning not yet known */
AT(0x41c) U32(0x00000001), U32(0x0000016e), U32(0x0000001e), U32(0x00000032), U32(0x0000003c), U32(0xffffffff),
U16(0xa708),
/* +0x0448  meaning not yet known */
AT(0x448) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000032), U32(0x0000003c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0470  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0488  meaning not yet known */
AT(0x488) U32(0xffffffff),
/* +0x04a0  meaning not yet known */
AT(0x4a0) U8(0x03),
/* +0x04b4  meaning not yet known */
AT(0x4b4) U32(0xffffffff),
/* +0x04e0  meaning not yet known */
AT(0x4e0) U32(0xffffffff),
AT(0x4f8) STR8("---- NM"),
STR12("%d.%d NM"),
STR8("%04d NM"),
STR8("---- KT"),
STR8("%03d KT"),
/* +0x0524  meaning not yet known */
U32(0x00000000),
/* +0x0528  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0530  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0538  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0544  meaning not yet known */
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
