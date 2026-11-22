/* BELLCLK.GAU's .data image, decompiled: every non-zero value of retail's .data (0x6f8 bytes at RVA 0x3000) at
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
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Bell_206B.Clock"),  /* the gauge name */
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
U8(0x39),
/* +0x0068  element 1 of 5: type 1 */
AT(0x68) U32(0x00000001),  /* element 1: type */
/* +0x006c  meaning not yet known */
U32(0x00002000), U32(0x0000009a), U32(0x00000094), U32(0x00000000), U32(0x00000000), U32(0x00000034),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x0094  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x05),
/* +0x00d0  meaning not yet known */
AT(0xd0) U32(0x0000009a), U8(0x09),
/* +0x0148  the children of element 2, zero-terminated */
AT(0x148) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0150  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x0154  meaning not yet known */
U32(0x00003000), U32(0x0000009a), U32(0x00000094), U32(0x00000000), U32(0x00000000), U32(0x0000000b),
U32(0x0000000b),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x017c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x04),
/* +0x01b8  meaning not yet known */
AT(0x1b8) U32(0x00000073), U8(0x0b),
/* +0x0230  the children of element 3, zero-terminated */
AT(0x230) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0238  element 3 of 5: type 1 */
U32(0x00000001),  /* element 3: type */
/* +0x023c  meaning not yet known */
U32(0x00004000), U32(0x0000009a), U32(0x00000094), U32(0x00000000), U32(0x00000000), U32(0x0000000b),
U32(0x0000000b),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x0264  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x02a0  meaning not yet known */
AT(0x2a0) U32(0x00000057), U8(0x0b),
/* +0x0318  the children of element 4, zero-terminated */
AT(0x318) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0320  meaning not yet known */
U32(0x00000017), U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0338  element 4 of 5: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 4: type */
/* +0x033c  meaning not yet known */
U32(0xffffffff), U32(0x00000057), U8(0xa3),
AT(0x35c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x0364  meaning not yet known */
U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x0150),
AT(0x3f0) U32(0x00000000),  /* element 4: text buffer */
/* +0x03f4  meaning not yet known */
U32(0x00000000), U32(0x0000008c), U32(0x00000030), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x430) STR12("Courier New"),
/* +0x0450  meaning not yet known */
AT(0x450) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x0470  the children of element 5, zero-terminated */
AT(0x470) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0478  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x047c  meaning not yet known */
U16(0x1000),
AT(0x49c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x04a4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x04c8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x4c8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x04cc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x04e4  meaning not yet known */
AT(0x4e4) U32(0xffffffff), U16(0xa60a),
/* +0x04fc  meaning not yet known */
AT(0x4fc) U32(0x00000001), U32(0x00000057), U32(0x000000a3), U32(0x0000008c), U32(0x00000030), U32(0xffffffff),
U16(0xa107),
/* +0x0528  meaning not yet known */
AT(0x528) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000046), U32(0x00000030), U32(0x00000002),
U32(0x00000000), U32(0x20000000), U32(0x000100fe), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000046), U32(0x00000000), U32(0x00000046), U32(0x00000030), U32(0x00000001), U32(0x00000000),
U32(0x20000000), U32(0x000100fd), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0594  meaning not yet known */
AT(0x594) U32(0xffffffff),
/* +0x05ac  meaning not yet known */
AT(0x5ac) U32(0x00000001), U32(0x00000000), U32(0x000000e4), U32(0x00000048), U32(0x00000048), U32(0xffffffff),
/* +0x05d8  meaning not yet known */
AT(0x5d8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000024), U32(0x00000048), U32(0x00000002),
U32(0x00000000), U32(0x20000000), U32(0x00010079), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000024), U32(0x00000000), U32(0x00000024), U32(0x00000048), U32(0x00000001), U32(0x00000000),
U32(0x20000000), U32(0x0001007a), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0644  meaning not yet known */
AT(0x644) U32(0xffffffff),
/* +0x065c  meaning not yet known */
AT(0x65c) U8(0x03),
/* +0x0670  meaning not yet known */
AT(0x670) U32(0xffffffff),
/* +0x069c  meaning not yet known */
AT(0x69c) U32(0xffffffff),
AT(0x6b4) STR12("%02d.%02d"),
/* +0x06c0  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x06c8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x06d0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x06dc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x06ec  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR12
#undef STR24
