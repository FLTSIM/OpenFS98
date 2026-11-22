/* CRGFUEL.GAU's .data image, decompiled: every non-zero value of retail's .data (0x578 bytes at RVA 0x3000) at
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
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Cessna_182_RG.Fuel"),  /* the gauge name */
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
AT(0x68) U32(0x00000001), U32(0x00000002), U32(0x00000000), U32(0x0000000a), U8(0x01),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x00000014), U8(0x28),
/* +0x00a8  meaning not yet known */
AT(0xa8) U32(0x00000043), U32(0x00000028), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x00000067), U32(0x00000028), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x0000008d), U32(0x00000028), U32(0x00000000), U32(0x403e0000), U32(0x00000000), U32(0x00000000),
U32(0x000000be), U32(0x00000028), U32(0x00000000), U32(0x40468000), U32(0x00000000), U32(0x00000000),
U32(0x000000d8), U8(0x28),
/* +0x0120  meaning not yet known */
AT(0x120) U32(0x00000107), U32(0x00000028), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x0000012a), U32(0x00000028), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x00000150), U32(0x00000028), U32(0x00000000), U32(0x403e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000183), U32(0x00000028), U32(0x00000000), U32(0x40468000), U32(0x00000000), U32(0x00000000),
/* +0x0180  element 1 of 4: type 0 */
U32(0x00000000),  /* element 1: type */
/* +0x0184  meaning not yet known */
U32(0x00002000), U32(0x0000004e), U8(0x70),
AT(0x1a4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x01d0  the children of element 2, zero-terminated */
AT(0x1d0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x01d8  element 2 of 4: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x01dc  meaning not yet known */
U32(0x00003000), U32(0x00000067), U32(0x0000007d), U32(0x00000000), U32(0x00000000), U32(0x00000007),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0208  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x0130),
/* +0x0240  meaning not yet known */
AT(0x240) U32(0x00000063), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x024c  meaning not yet known */
U32(0x00000005), U8(0x06),
/* +0x02b8  the children of element 3, zero-terminated */
AT(0x2b8) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x02c0  element 3 of 4: type 1 */
U32(0x00000001),  /* element 3: type */
/* +0x02c4  meaning not yet known */
U32(0x00003000), U32(0x0000012b), U32(0x0000007d), U32(0x00000000), U32(0x00000000), U32(0x00000007),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02f0  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x012f),
/* +0x0328  meaning not yet known */
AT(0x328) U32(0x00000063), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0334  meaning not yet known */
U32(0x00000005), U8(0x06),
/* +0x03a0  the children of element 4, zero-terminated */
AT(0x3a0) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x03a8  element 4 of 4: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x03ac  meaning not yet known */
U16(0x1000),
AT(0x3cc) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03d8  meaning not yet known */
U8(0x01),
/* +0x03f8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x3f8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x03fc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0414  meaning not yet known */
AT(0x414) U32(0xffffffff),
/* +0x042c  meaning not yet known */
AT(0x42c) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x000000c8), U32(0x000000a3), U32(0xffffffff),
U16(0xa029),
/* +0x0458  meaning not yet known */
AT(0x458) U8(0x03),
/* +0x046c  meaning not yet known */
AT(0x46c) U32(0xffffffff),
/* +0x0484  meaning not yet known */
AT(0x484) U32(0x00000001), U32(0x000000c8), U32(0x00000000), U32(0x000000c8), U32(0x000000a3), U32(0xffffffff),
U16(0xa02a),
/* +0x04b0  meaning not yet known */
AT(0x4b0) U8(0x03),
/* +0x04c4  meaning not yet known */
AT(0x4c4) U32(0xffffffff),
/* +0x04dc  meaning not yet known */
AT(0x4dc) U8(0x03),
/* +0x04f0  meaning not yet known */
AT(0x4f0) U32(0xffffffff),
/* +0x051c  meaning not yet known */
AT(0x51c) U32(0xffffffff),
/* +0x0538  ImportTable: one import, PANELS (module id 0xf) */
AT(0x538) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0540  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0548  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0554  meaning not yet known */
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
#undef STR24
