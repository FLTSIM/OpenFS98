/* EXTURN.GAU's .data image, decompiled: every non-zero value of retail's .data (0x508 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR32(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR32("Extra-300.Turn-Coordinator"),  /* the gauge name */
/* +0x0020  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x004c  meaning not yet known */
U8(0x53),
/* +0x0070  meaning not yet known */
AT(0x70) U32(0x00000012), U32(0x00000001), U32(0x00000000), U32(0x00000001), U8(0x02),
/* +0x0098  element 1 of 4: type 3 */
AT(0x98) U32(0x00000003),  /* element 1: type */
/* +0x009c  meaning not yet known */
U32(0x00003000), U32(0x0000008c), U8(0xce),
AT(0xbc) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00c4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x45),
/* +0x00f9  meaning not yet known */
AT(0xf9) U8(0xc0), U16(0x3fe1), U32(0x00000000), U8(0x45),
/* +0x0126  meaning not yet known */
AT(0x126) U16(0x3fb2),
/* +0x0140  meaning not yet known */
AT(0x140) U32(0x00000046), U32(0x00000032), U32(0x00000000), U32(0xc08c2000), U32(0x00000000), U32(0x00000000),
U32(0x00000072), U32(0x00000023), U32(0x00000000), U32(0xc0811000), U32(0x00000000), U32(0x00000000),
U32(0x00000096), U8(0x1e),
/* +0x0188  meaning not yet known */
AT(0x188) U32(0x000000ba), U32(0x00000023), U32(0x00000000), U32(0x40811000), U32(0x00000000), U32(0x00000000),
U32(0x000000e6), U32(0x00000032), U32(0x00000000), U32(0x408c2000), U32(0x00000000), U32(0x00000000),
/* +0x01b8  element 2 of 4: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x01bc  meaning not yet known */
U32(0x00002000), U32(0x00000094), U32(0x000000a0), U32(0x00000000), U32(0x00000000), U32(0x00000010),
U32(0x00000011),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01e8  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x46),
/* +0x0220  meaning not yet known */
AT(0x220) U32(0x0000008f), U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x022c  meaning not yet known */
U32(0x00000005), U8(0x06),
/* +0x0298  the children of element 3, zero-terminated */
AT(0x298) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x02a0  element 3 of 4: type 4 */
U32(0x00000004),  /* element 3: type */
/* +0x02a4  meaning not yet known */
U32(0x00004000), U32(0x000000dc), U8(0x75),
AT(0x2c4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x02cc  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x00000000), U32(0x00000000), U16(0x0117),
/* +0x0310  meaning not yet known */
AT(0x310) U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0328  the children of element 4, zero-terminated */
U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0334  meaning not yet known */
U32(0x00000000),
/* +0x0338  element 4 of 4: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x033c  meaning not yet known */
U16(0x1000),
AT(0x35c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0368  meaning not yet known */
U8(0x01),
/* +0x0388  the element-list slot (the header's +0x08 points here): the first element */
AT(0x388) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x038c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03a4  meaning not yet known */
AT(0x3a4) U32(0xffffffff),
/* +0x03bc  meaning not yet known */
AT(0x3bc) U32(0x00000001), U32(0x00000040), U32(0x00000019), U32(0x000000b4), U32(0x0000009d), U32(0xffffffff),
U16(0xa506),
/* +0x03e8  meaning not yet known */
AT(0x3e8) U8(0x03),
/* +0x03fc  meaning not yet known */
AT(0x3fc) U32(0xffffffff),
/* +0x0414  meaning not yet known */
AT(0x414) U32(0x00000001), U32(0x0000002b), U32(0x000000b4), U32(0x000000d8), U32(0x00000042), U32(0xffffffff),
U16(0xa507),
/* +0x0440  meaning not yet known */
AT(0x440) U8(0x03),
/* +0x0454  meaning not yet known */
AT(0x454) U32(0xffffffff),
/* +0x046c  meaning not yet known */
AT(0x46c) U8(0x03),
/* +0x0480  meaning not yet known */
AT(0x480) U32(0xffffffff),
/* +0x04ac  meaning not yet known */
AT(0x4ac) U32(0xffffffff),
/* +0x04c8  ImportTable: one import, PANELS (module id 0xf) */
AT(0x4c8) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x04d0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x04d8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x04e4  meaning not yet known */
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
#undef STR32
