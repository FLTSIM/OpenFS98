/* CESSTIMR.GAU's .data image, decompiled: every non-zero value of retail's .data (0x668 bytes at RVA 0x3000) at
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
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Cessna_182.Timer"),  /* the gauge name */
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
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  meaning not yet known */
AT(0x80) U32(0x00000018), U32(0x00000000), U32(0x00000001), U32(0x00000019), U32(0x00000003), U32(0x00000004),
U32(0x0000001a), U32(0x00000006), U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x00b0  element 1 of 3: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 1: type */
/* +0x00b4  meaning not yet known */
U32(0xffffffff), U32(0x00000026), U8(0x81),
AT(0xd4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00e0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x05),
/* +0x0108  meaning not yet known */
AT(0x108) U8(0x04),
/* +0x0124  meaning not yet known */
AT(0x124) U8(0x03),
AT(0x168) U32(0x00000000),  /* element 1: text buffer */
/* +0x016c  meaning not yet known */
U32(0x00000000), U32(0x000000e6), U32(0x00000049), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x1a8) STR12("Courier New"),
/* +0x01c8  meaning not yet known */
AT(0x1c8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x08),
/* +0x01e8  meaning not yet known */
AT(0x1e8) U32(0x00000017), U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0200  element 2 of 3: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 2: type */
/* +0x0204  meaning not yet known */
U32(0xffffffff), U32(0x0000004b), U8(0x4a),
AT(0x224) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0230  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0150),
AT(0x2b8) U32(0x00000000),  /* element 2: text buffer */
/* +0x02bc  meaning not yet known */
U32(0x00000000), U32(0x000000a0), U32(0x00000032), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x2f8) STR12("Courier New"),
/* +0x0318  meaning not yet known */
AT(0x318) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x05),
/* +0x0338  the children of element 3, zero-terminated */
AT(0x338) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0344  meaning not yet known */
U32(0x00000000),
/* +0x0348  element 3 of 3: type 0 */
U32(0x00000000),  /* element 3: type */
/* +0x034c  meaning not yet known */
U16(0x1000),
AT(0x36c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x0374  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0398  the element-list slot (the header's +0x08 points here): the first element */
AT(0x398) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x039c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03b4  meaning not yet known */
AT(0x3b4) U32(0xffffffff), U16(0xa017),
/* +0x03cc  meaning not yet known */
AT(0x3cc) U32(0x00000001), U32(0x0000004b), U32(0x0000004a), U32(0x000000a0), U32(0x00000032), U32(0xffffffff),
U16(0xa107),
/* +0x03f8  meaning not yet known */
AT(0x3f8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000050), U32(0x00000032), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x000100fe), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000050), U32(0x00000000), U32(0x00000050), U32(0x00000032), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x000100fd), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0464  meaning not yet known */
AT(0x464) U32(0xffffffff),
/* +0x047c  meaning not yet known */
AT(0x47c) U32(0x00000001), U32(0x00000026), U32(0x00000081), U32(0x000000e6), U32(0x00000049), U32(0xffffffff),
U16(0xa017),
/* +0x04a8  meaning not yet known */
AT(0x4a8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001e), U32(0x00000049), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010079), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000001e), U32(0x00000000), U32(0x0000001e), U32(0x00000049), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001007a), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000055),
U32(0x00000000), U32(0x0000001e), U32(0x00000049), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x0001007b), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000073), U32(0x00000000),
U32(0x0000001e), U32(0x00000049), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x0001007c),
U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x000000af), U32(0x00000000), U32(0x0000003c),
U32(0x00000049), U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x0001007d), U32(0x00000000),
U32(0x00000000), U8(0x03),
/* +0x0598  meaning not yet known */
AT(0x598) U32(0xffffffff),
/* +0x05b0  meaning not yet known */
AT(0x5b0) U8(0x03),
/* +0x05c4  meaning not yet known */
AT(0x5c4) U32(0xffffffff),
/* +0x05f0  meaning not yet known */
AT(0x5f0) U32(0xffffffff),
AT(0x608) STR16("%02d:%02d:%02d"),
STR12("%02d.%02d"),
/* +0x0624  meaning not yet known */
U32(0x00000000),
/* +0x0628  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0630  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0638  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0644  meaning not yet known */
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
#undef STR12
#undef STR16
#undef STR24
