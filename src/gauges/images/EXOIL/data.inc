/* EXOIL.GAU's .data image, decompiled: every non-zero value of retail's .data (0x518 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR40(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR40("Extra-300.Oil-Pressure-Tempature"),  /* the gauge name */
/* +0x0028  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x0054  meaning not yet known */
U8(0x53),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x00000041), U8(0x85),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x000000ec), U32(0x00000085), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x000000ea), U32(0x000000a6), U32(0x00000000), U32(0x40440000), U32(0x00000000), U32(0x00000000),
U32(0x000000e5), U32(0x000000be), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x000000d8), U32(0x000000d4), U32(0x00000000), U32(0x40618000), U32(0x00000000), U32(0x00000000),
U32(0x000000bb), U32(0x000000e8), U32(0x00000000), U32(0x40668000), U32(0x00000000), U32(0x00000000),
U32(0x000000a6), U32(0x000000f0), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x0000008d), U32(0x000000f0), U32(0x00000000), U32(0x406b8000), U32(0x00000000), U32(0x00000000),
U32(0x0000006f), U32(0x000000e8), U32(0x00000000), U32(0x406e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000053), U32(0x000000d2), U32(0x00000000), U32(0x40704000), U32(0x00000000), U32(0x00000000),
U32(0x00000041), U32(0x000000a9), U32(0x00000000), U32(0x40718000), U32(0x00000000), U32(0x00000000),
/* +0x0180  element 1 of 3: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x0184  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x00000085), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x01ac  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x94),
/* +0x01e8  meaning not yet known */
AT(0x1e8) U32(0x00000059), U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01f4  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0260  element 2 of 3: type 1 */
AT(0x260) U32(0x00000001),  /* element 2: type */
/* +0x0264  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x000000a6), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x028c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x93),
/* +0x02c8  meaning not yet known */
AT(0x2c8) U32(0x00000059), U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02d4  meaning not yet known */
U32(0x00000009), U8(0x06),
/* +0x0340  the children of element 3, zero-terminated */
AT(0x340) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x034c  meaning not yet known */
U32(0x00000000),
/* +0x0350  element 3 of 3: type 0 */
U32(0x00000000),  /* element 3: type */
/* +0x0354  meaning not yet known */
U16(0x1000),
AT(0x374) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x037c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03a0  the element-list slot (the header's +0x08 points here): the first element */
AT(0x3a0) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x03a4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03bc  meaning not yet known */
AT(0x3bc) U32(0xffffffff),
/* +0x03d4  meaning not yet known */
AT(0x3d4) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000126), U32(0x0000009a), U32(0xffffffff),
U16(0xa526),
/* +0x0400  meaning not yet known */
AT(0x400) U8(0x03),
/* +0x0414  meaning not yet known */
AT(0x414) U32(0xffffffff),
/* +0x042c  meaning not yet known */
AT(0x42c) U32(0x00000001), U32(0x00000000), U32(0x0000009a), U32(0x00000126), U32(0x0000008e), U32(0xffffffff),
U16(0xa525),
/* +0x0458  meaning not yet known */
AT(0x458) U8(0x03),
/* +0x046c  meaning not yet known */
AT(0x46c) U32(0xffffffff),
/* +0x0484  meaning not yet known */
AT(0x484) U8(0x03),
/* +0x0498  meaning not yet known */
AT(0x498) U32(0xffffffff),
/* +0x04c4  meaning not yet known */
AT(0x4c4) U32(0xffffffff),
/* +0x04e0  ImportTable: one import, PANELS (module id 0xf) */
AT(0x4e0) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x04e8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x04f0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x04fc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x050c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR40
