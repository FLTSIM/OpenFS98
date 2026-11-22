/* CRGOIL.GAU's .data image, decompiled: every non-zero value of retail's .data (0x4e8 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Cessna_182_RG.Oil"),  /* the gauge name */
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
AT(0x68) U32(0x00000001), U8(0x02),
/* +0x0080  meaning not yet known */
AT(0x80) U32(0x0000000a), U32(0x0000001c), U32(0x00000000), U32(0x40490000), U32(0x00000000), U32(0x00000000),
U32(0x000000b4), U32(0x0000001c), U32(0x00000000), U32(0x406f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000dc), U8(0x1c),
/* +0x00c8  meaning not yet known */
AT(0xc8) U32(0x0000012c), U32(0x0000001c), U32(0x00000000), U32(0x404e0000), U32(0x00000000), U32(0x00000000),
U32(0x0000017c), U32(0x0000001c), U32(0x00000000), U32(0x405e0000), U32(0x00000000), U32(0x00000000),
/* +0x00f8  element 1 of 4: type 0 */
U32(0x00000000),  /* element 1: type */
/* +0x00fc  meaning not yet known */
U32(0x00002000), U32(0x00000035), U8(0x70),
AT(0x11c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x0148  the children of element 2, zero-terminated */
AT(0x148) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0150  element 2 of 4: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x0154  meaning not yet known */
U32(0x00003000), U32(0x00000067), U32(0x0000007d), U32(0x00000000), U32(0x00000000), U32(0x00000007),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0180  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x93),
/* +0x01b8  meaning not yet known */
AT(0x1b8) U32(0x00000063), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01c4  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0230  the children of element 3, zero-terminated */
AT(0x230) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0238  element 3 of 4: type 1 */
U32(0x00000001),  /* element 3: type */
/* +0x023c  meaning not yet known */
U32(0x00003000), U32(0x0000012b), U32(0x0000007d), U32(0x00000000), U32(0x00000000), U32(0x00000007),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0268  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x94),
/* +0x02a0  meaning not yet known */
AT(0x2a0) U32(0x00000063), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02ac  meaning not yet known */
U32(0x00000003), U8(0x06),
/* +0x0318  the children of element 4, zero-terminated */
AT(0x318) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0320  element 4 of 4: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x0324  meaning not yet known */
U16(0x1000),
AT(0x344) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x034c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0370  the element-list slot (the header's +0x08 points here): the first element */
AT(0x370) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0374  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x038c  meaning not yet known */
AT(0x38c) U32(0xffffffff),
/* +0x03a4  meaning not yet known */
AT(0x3a4) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x000000c8), U32(0x000000a3), U32(0xffffffff),
U16(0xa022),
/* +0x03d0  meaning not yet known */
AT(0x3d0) U8(0x03),
/* +0x03e4  meaning not yet known */
AT(0x3e4) U32(0xffffffff),
/* +0x03fc  meaning not yet known */
AT(0x3fc) U32(0x00000001), U32(0x000000c8), U32(0x00000000), U32(0x000000c8), U32(0x000000a3), U32(0xffffffff),
U16(0xa023),
/* +0x0428  meaning not yet known */
AT(0x428) U8(0x03),
/* +0x043c  meaning not yet known */
AT(0x43c) U32(0xffffffff),
/* +0x0454  meaning not yet known */
AT(0x454) U8(0x03),
/* +0x0468  meaning not yet known */
AT(0x468) U32(0xffffffff),
/* +0x0494  meaning not yet known */
AT(0x494) U32(0xffffffff),
/* +0x04b0  ImportTable: one import, PANELS (module id 0xf) */
AT(0x4b0) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x04b8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x04c0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x04cc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x04dc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR24
