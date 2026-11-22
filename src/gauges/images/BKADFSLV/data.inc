/* BKADFSLV.GAU's .data image, decompiled: every non-zero value of retail's .data (0x568 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Bendix_King.ADF.Slaved"),  /* the gauge name */
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
AT(0x68) U8(0x51),
/* +0x0088  meaning not yet known */
AT(0x88) U32(0x00000005), U8(0x01),
/* +0x00a0  meaning not yet known */
AT(0xa0) U8(0x05),
/* +0x00b8  element 1 of 5: type 0 */
AT(0xb8) U32(0x00000000),  /* element 1: type */
/* +0x00bc  meaning not yet known */
U32(0x00004000), U32(0x00000090), U8(0x0f),
AT(0xdc) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00e8  meaning not yet known */
U8(0x01),
/* +0x0108  element 2 of 5: type 0 */
AT(0x108) U32(0x00000000),  /* element 2: type */
/* +0x010c  meaning not yet known */
U32(0x00005000), U32(0x00000081), U8(0x87),
AT(0x12c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0138  meaning not yet known */
U8(0x01),
/* +0x0158  the children of element 3, zero-terminated */
AT(0x158) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0164  meaning not yet known */
U32(0x00000000),
/* +0x0168  element 3 of 5: type 1 */
U32(0x00000001),  /* element 3: type */
/* +0x016c  meaning not yet known */
U32(0x00003000), U32(0x00000096), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000059),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0198  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x27),
/* +0x01d0  meaning not yet known */
AT(0x1d0) U32(0x000000b2), U8(0x07),
/* +0x0248  the children of element 4, zero-terminated */
AT(0x248) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0250  element 4 of 5: type 6 */
U32(0x00000006),  /* element 4: type */
/* +0x0254  meaning not yet known */
U32(0x00002000), U32(0x0000000f), U32(0x0000000e), U32(0x00000000), U32(0x00000000), U32(0x00000096),
U32(0x00000096),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x02dc  meaning not yet known */
AT(0x2dc) U8(0x16),
/* +0x0302  meaning not yet known */
AT(0x302) U16(0x3ff0), U32(0x3f7ae148), U32(0x3f7ae148), U32(0x00000035), U8(0x35),
/* +0x0390  the children of element 5, zero-terminated */
AT(0x390) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0398  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x039c  meaning not yet known */
U16(0x1000),
AT(0x3bc) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03c8  meaning not yet known */
U8(0x01),
/* +0x03e8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x3e8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x03ec  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0404  meaning not yet known */
AT(0x404) U32(0xffffffff), U16(0xa000),
/* +0x041c  meaning not yet known */
AT(0x41c) U32(0x00000001), U32(0x00000003), U32(0x000000e9), U32(0x00000040), U32(0x00000040), U32(0xffffffff),
U16(0xa501),
/* +0x0448  meaning not yet known */
AT(0x448) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001015a), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000020), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010159), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x04b4  meaning not yet known */
AT(0x4b4) U32(0xffffffff),
/* +0x04cc  meaning not yet known */
AT(0x4cc) U8(0x03),
/* +0x04e0  meaning not yet known */
AT(0x4e0) U32(0xffffffff),
/* +0x050c  meaning not yet known */
AT(0x50c) U32(0xffffffff),
/* +0x0528  ImportTable: one import, PANELS (module id 0xf) */
AT(0x528) U32(0x0000000f),  /* import: module id (PANELS) */
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
#undef STR24
