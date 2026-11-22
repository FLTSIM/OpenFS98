/* 737RMI.GAU's .data image, decompiled: every non-zero value of retail's .data (0x728 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR16("737-400.RMI"),  /* the gauge name */
/* +0x0010  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x003c  meaning not yet known */
U8(0x94),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x0000000f), U8(0x02),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x00000005), U32(0x00000002), U32(0x00000000), U32(0x00000001), U8(0x01),
/* +0x00a0  meaning not yet known */
AT(0xa0) U32(0x00000001), U8(0x03),
/* +0x00b8  meaning not yet known */
AT(0xb8) U32(0x0000000b), U32(0x00000001), U32(0x00000000), U32(0x00000001), U8(0x01),
/* +0x00e0  element 1 of 5: type 1 */
AT(0xe0) U32(0x00000001),  /* element 1: type */
/* +0x00e4  meaning not yet known */
U32(0x00003000), U32(0x00000071), U32(0x000000a1), U32(0x00000000), U32(0x00000000), U32(0x00000046),
U32(0x0000000d),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0110  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x3b),
/* +0x0148  meaning not yet known */
AT(0x148) U32(0x0000008b), U8(0x0b),
/* +0x01c0  the children of element 2, zero-terminated */
AT(0x1c0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x01c8  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x01cc  meaning not yet known */
U32(0x00004000), U32(0x00000071), U32(0x000000a1), U32(0x00000000), U32(0x00000000), U32(0x00000046),
U32(0x0000000b),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01f8  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x27),
/* +0x0230  meaning not yet known */
AT(0x230) U32(0x0000008b), U8(0x0e),
/* +0x02a8  the children of element 3, zero-terminated */
AT(0x2a8) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x02b0  element 3 of 5: type 6 */
U32(0x00000006),  /* element 3: type */
/* +0x02b4  meaning not yet known */
U32(0x00002000), U32(0x00000020), U32(0x00000050), U32(0x00000000), U32(0x00000000), U32(0x00000071),
U32(0x000000a1),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x033c  meaning not yet known */
AT(0x33c) U8(0x51),
/* +0x0362  meaning not yet known */
AT(0x362) U16(0x3ff0), U32(0x3fca3d71), U32(0x3fca3d71), U32(0x00000071), U8(0x4c),
/* +0x03f8  meaning not yet known */
AT(0x3f8) U32(0x00000012), U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0410  element 4 of 5: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 4: type */
/* +0x0414  meaning not yet known */
U32(0xffffffff), U32(0x0000002a), U8(0x23),
AT(0x434) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0440  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x23),
/* +0x0468  meaning not yet known */
AT(0x468) U8(0x24),
/* +0x0484  meaning not yet known */
AT(0x484) U8(0x22),
AT(0x4c8) U32(0x00000000),  /* element 4: text buffer */
/* +0x04cc  meaning not yet known */
U32(0x00000000), U32(0x0000008c), U32(0x00000023), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x508) STR12("Courier New"),
/* +0x0528  meaning not yet known */
AT(0x528) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x07),
/* +0x0548  the children of element 5, zero-terminated */
AT(0x548) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0554  meaning not yet known */
U32(0x00000000),
/* +0x0558  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x055c  meaning not yet known */
U16(0x1000),
AT(0x57c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x0584  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x05a8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x5a8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x05ac  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x05c4  meaning not yet known */
AT(0x5c4) U32(0xffffffff), U16(0xa426),
/* +0x05dc  meaning not yet known */
AT(0x5dc) U32(0x00000001), U32(0x00000013), U32(0x0000001d), U32(0x000000bc), U32(0x00000032), U32(0xffffffff),
U16(0xa436),
/* +0x0608  meaning not yet known */
AT(0x608) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x000000bc), U32(0x00000032), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100fc), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0648  meaning not yet known */
AT(0x648) U32(0xffffffff),
/* +0x0660  meaning not yet known */
AT(0x660) U8(0x03),
/* +0x0674  meaning not yet known */
AT(0x674) U32(0xffffffff),
/* +0x06a0  meaning not yet known */
AT(0x6a0) U32(0xffffffff),
AT(0x6b8) STR8("---- NM"),
STR12("%d.%d NM"),
STR8("%04d NM"),
STR8("--- KT"),
STR8("%03d KT"),
/* +0x06e4  meaning not yet known */
U32(0x00000000),
/* +0x06e8  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x06f0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x06f8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0704  meaning not yet known */
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
#undef STR16
