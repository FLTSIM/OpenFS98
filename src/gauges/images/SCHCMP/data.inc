/* SCHCMP.GAU's .data image, decompiled: every non-zero value of retail's .data (0x508 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Schweizer Compass"),  /* the gauge name */
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
U8(0x67),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x0000000c), U8(0x01),
/* +0x0080  element 1 of 3: type 2 (text, 0xbc bytes) */
AT(0x80) U32(0x00000002),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0xffffffff), U32(0x00000025), U8(0x2e),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00b0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x51),
AT(0x138) U32(0x00000000),  /* element 1: text buffer */
/* +0x013c  meaning not yet known */
U32(0x00000000), U32(0x00000051), U32(0x00000015), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x178) STR12("Courier New"),
/* +0x0198  meaning not yet known */
AT(0x198) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x01b8  element 2 of 3: type 2 (text, 0xbc bytes) */
AT(0x1b8) U32(0x00000002),  /* element 2: type */
/* +0x01bc  meaning not yet known */
U32(0xffffffff), U32(0x0000009b), U8(0x2e),
AT(0x1dc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x01e4  meaning not yet known */
U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0145),
AT(0x270) U32(0x00000000),  /* element 2: text buffer */
/* +0x0274  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x00000015), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x2b0) STR12("Courier New"),
/* +0x02d0  meaning not yet known */
AT(0x2d0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x02f0  the children of element 3, zero-terminated */
AT(0x2f0) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x02fc  meaning not yet known */
U32(0x00000000),
/* +0x0300  element 3 of 3: type 0 */
U32(0x00000000),  /* element 3: type */
/* +0x0304  meaning not yet known */
U16(0x1000),
AT(0x324) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x032c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0350  the element-list slot (the header's +0x08 points here): the first element */
AT(0x350) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0354  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x036c  meaning not yet known */
AT(0x36c) U32(0xffffffff),
/* +0x0384  meaning not yet known */
AT(0x384) U32(0x00000001), U32(0x0000001b), U32(0x00000009), U32(0x00000063), U32(0x0000003e), U32(0xffffffff),
U16(0xa303),
/* +0x03b0  meaning not yet known */
AT(0x3b0) U8(0x03),
/* +0x03c4  meaning not yet known */
AT(0x3c4) U32(0xffffffff),
/* +0x03dc  meaning not yet known */
AT(0x3dc) U32(0x00000001), U32(0x00000085), U32(0x00000009), U32(0x0000006f), U32(0x0000003f), U32(0xffffffff),
U16(0xa304),
/* +0x0408  meaning not yet known */
AT(0x408) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000006f), U32(0x0000003f), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010035), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0448  meaning not yet known */
AT(0x448) U32(0xffffffff),
/* +0x0460  meaning not yet known */
AT(0x460) U8(0x03),
/* +0x0474  meaning not yet known */
AT(0x474) U32(0xffffffff),
/* +0x04a0  meaning not yet known */
AT(0x4a0) U32(0xffffffff),
AT(0x4b8) STR8("%03d"),
/* +0x04c0  meaning not yet known */
U32(0x0046464f), U32(0x00004e4f),
/* +0x04c8  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
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
#undef STR8
#undef STR12
#undef STR24
