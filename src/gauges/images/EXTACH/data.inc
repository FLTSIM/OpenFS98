/* EXTACH.GAU's .data image, decompiled: every non-zero value of retail's .data (0x5a8 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Extra-300.Tachometer"),  /* the gauge name */
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
U8(0x53),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  element 1 of 6: type 0 */
AT(0x80) U32(0x00000000),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0x00001500), U32(0x0000000e), U8(0x0e),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00ac  meaning not yet known */
U32(0x00000000), U32(0x40000017),
/* +0x00d0  the children of element 2, zero-terminated */
AT(0xd0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x00d8  element 2 of 6: type 4 */
U32(0x00000004),  /* element 2: type */
/* +0x00dc  meaning not yet known */
U32(0x00002000), U32(0x00000097), U8(0x2a),
AT(0xfc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0108  meaning not yet known */
U8(0x07),
/* +0x0142  meaning not yet known */
AT(0x142) U16(0x3ff0), U32(0x00000000), U8(0x01),
/* +0x0160  the children of element 3, zero-terminated */
AT(0x160) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0168  element 3 of 6: type 4 */
U32(0x00000004),  /* element 3: type */
/* +0x016c  meaning not yet known */
U32(0x00003000), U32(0x000000ae), U8(0x2a),
AT(0x18c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0198  meaning not yet known */
U8(0x07),
/* +0x01d2  meaning not yet known */
AT(0x1d2) U16(0x3ff0), U32(0x00000000), U8(0x01),
/* +0x01f0  the children of element 4, zero-terminated */
AT(0x1f0) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x01f8  element 4 of 6: type 4 */
U32(0x00000004),  /* element 4: type */
/* +0x01fc  meaning not yet known */
U32(0x00004000), U32(0x000000c7), U8(0x2a),
AT(0x21c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0228  meaning not yet known */
U8(0x07),
/* +0x0262  meaning not yet known */
AT(0x262) U16(0x3ff0), U32(0x00000000), U8(0x01),
/* +0x0280  the children of element 5, zero-terminated */
AT(0x280) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0288  element 5 of 6: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 5: type */
/* +0x028c  meaning not yet known */
U32(0xffffffff), U32(0x00000045), U8(0x75),
AT(0x2ac) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02b8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x88),
/* +0x02e0  meaning not yet known */
AT(0x2e0) U8(0x9a),
AT(0x340) U32(0x00000000),  /* element 5: text buffer */
/* +0x0344  meaning not yet known */
U32(0x00000000), U32(0x000000a2), U32(0x00000033), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff), U8(0x80),
AT(0x380) STR12("Courier New"),
/* +0x03a0  meaning not yet known */
AT(0x3a0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x04),
/* +0x03c0  the children of element 6, zero-terminated */
AT(0x3c0) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x03c8  element 6 of 6: type 0 */
U32(0x00000000),  /* element 6: type */
/* +0x03cc  meaning not yet known */
U16(0x1000),
AT(0x3ec) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
/* +0x03f4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0418  the element-list slot (the header's +0x08 points here): the first element */
AT(0x418) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x041c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0434  meaning not yet known */
AT(0x434) U32(0xffffffff), U16(0xa523),
/* +0x044c  meaning not yet known */
AT(0x44c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000012c), U32(0x0000012c), U32(0xffffffff),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0474  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x00000056), U32(0x000000dc), U32(0x00000020), U32(0x00000020),
U32(0xffffffff), U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04a0  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x00000085), U32(0x000000dc), U32(0x00000020), U32(0x00000020),
U32(0xffffffff), U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04cc  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x000000b5), U32(0x000000dc), U32(0x00000020), U32(0x00000020),
U32(0xffffffff), U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04f8  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0510  meaning not yet known */
AT(0x510) U32(0xffffffff),
/* +0x053c  meaning not yet known */
AT(0x53c) U32(0xffffffff),
AT(0x554) STR8("%04d"),
/* +0x055c  meaning not yet known */
U32(0x00000000),
/* +0x0560  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0568  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0570  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x057c  meaning not yet known */
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
