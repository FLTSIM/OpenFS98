/* 737MAIN.GAU's .data image, decompiled: every non-zero value of retail's .data (0x19bc bytes at RVA 0x3000) at
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
AT(0x0) STR24("737-400.Main-Set"),  /* the gauge name */
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
U8(0xd8),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  meaning not yet known */
AT(0x80) U32(0x00000052), U8(0x5b),
/* +0x0098  meaning not yet known */
AT(0x98) U32(0x00000030), U32(0x0000005b), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
U32(0x000000a7), U8(0x5c),
/* +0x00c8  meaning not yet known */
AT(0xc8) U32(0x00000084), U32(0x0000005c), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
U32(0x00000050), U8(0xa5),
/* +0x00f8  meaning not yet known */
AT(0xf8) U32(0x00000032), U32(0x000000a6), U32(0x00000000), U32(0x40d29600), U32(0x00000000), U32(0x00000000),
U32(0x000000a3), U8(0xa6),
/* +0x0128  meaning not yet known */
AT(0x128) U32(0x00000086), U32(0x000000a7), U32(0x00000000), U32(0x40d29600), U32(0x00000000), U32(0x00000000),
U32(0x0000004f), U8(0xf3),
/* +0x0158  meaning not yet known */
AT(0x158) U32(0x00000032), U32(0x000000f2), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
U32(0x000000a3), U8(0xf2),
/* +0x0188  meaning not yet known */
AT(0x188) U32(0x00000086), U32(0x000000f2), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
U32(0x00000052), U16(0x0139),
/* +0x01b8  meaning not yet known */
AT(0x1b8) U32(0x0000002f), U32(0x00000139), U32(0x00000000), U32(0x4092c000), U32(0x00000000), U32(0x00000000),
U32(0x000000a7), U16(0x0139),
/* +0x01e8  meaning not yet known */
AT(0x1e8) U32(0x00000084), U32(0x00000139), U32(0x00000000), U32(0x4092c000), U32(0x00000000), U32(0x00000000),
/* +0x0200  element 1 of 26: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 1: type */
/* +0x0204  meaning not yet known */
U32(0xffffffff), U32(0x00000030), U8(0x67),
AT(0x224) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0230  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0x88),
AT(0x2b8) U32(0x00000000),  /* element 1: text buffer */
/* +0x02bc  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x2f8) STR12("Courier New"),
/* +0x0318  meaning not yet known */
AT(0x318) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0338  element 2 of 26: type 2 (text, 0xbc bytes) */
AT(0x338) U32(0x00000002),  /* element 2: type */
/* +0x033c  meaning not yet known */
U32(0xffffffff), U32(0x00000083), U8(0x67),
AT(0x35c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0368  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0xab),
AT(0x3f0) U32(0x00000000),  /* element 2: text buffer */
/* +0x03f4  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x430) STR12("Courier New"),
/* +0x0450  meaning not yet known */
AT(0x450) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0470  element 3 of 26: type 2 (text, 0xbc bytes) */
AT(0x470) U32(0x00000002),  /* element 3: type */
/* +0x0474  meaning not yet known */
U32(0xffffffff), U32(0x00000030), U8(0xb4),
AT(0x494) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04a0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0x90),
AT(0x528) U32(0x00000000),  /* element 3: text buffer */
/* +0x052c  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x568) STR12("Courier New"),
/* +0x0588  meaning not yet known */
AT(0x588) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x05a8  element 4 of 26: type 2 (text, 0xbc bytes) */
AT(0x5a8) U32(0x00000002),  /* element 4: type */
/* +0x05ac  meaning not yet known */
U32(0xffffffff), U32(0x00000083), U8(0xb4),
AT(0x5cc) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05d8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0xb3),
AT(0x660) U32(0x00000000),  /* element 4: text buffer */
/* +0x0664  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x6a0) STR12("Courier New"),
/* +0x06c0  meaning not yet known */
AT(0x6c0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x06e0  element 5 of 26: type 2 (text, 0xbc bytes) */
AT(0x6e0) U32(0x00000002),  /* element 5: type */
/* +0x06e4  meaning not yet known */
U32(0xffffffff), U32(0x00000030), U8(0xff),
AT(0x704) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0710  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0x87),
AT(0x798) U32(0x00000000),  /* element 5: text buffer */
/* +0x079c  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x7d8) STR12("Courier New"),
/* +0x07f8  meaning not yet known */
AT(0x7f8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0818  element 6 of 26: type 2 (text, 0xbc bytes) */
AT(0x818) U32(0x00000002),  /* element 6: type */
/* +0x081c  meaning not yet known */
U32(0xffffffff), U32(0x00000083), U8(0xff),
AT(0x83c) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0848  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0xaa),
AT(0x8d0) U32(0x00000000),  /* element 6: text buffer */
/* +0x08d4  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x910) STR12("Courier New"),
/* +0x0930  meaning not yet known */
AT(0x930) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0950  element 7 of 26: type 2 (text, 0xbc bytes) */
AT(0x950) U32(0x00000002),  /* element 7: type */
/* +0x0954  meaning not yet known */
U32(0xffffffff), U32(0x00000030), U16(0x0141),
AT(0x974) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0980  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0x8a),
AT(0xa08) U32(0x00000000),  /* element 7: text buffer */
/* +0x0a0c  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xa48) STR12("Courier New"),
/* +0x0a68  meaning not yet known */
AT(0xa68) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0a88  element 8 of 26: type 2 (text, 0xbc bytes) */
AT(0xa88) U32(0x00000002),  /* element 8: type */
/* +0x0a8c  meaning not yet known */
U32(0xffffffff), U32(0x00000083), U16(0x0141),
AT(0xaac) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ab8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0xad),
AT(0xb40) U32(0x00000000),  /* element 8: text buffer */
/* +0x0b44  meaning not yet known */
U32(0x00000000), U32(0x00000024), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xb80) STR12("Courier New"),
/* +0x0ba0  meaning not yet known */
AT(0xba0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0bc0  element 9 of 26: type 2 (text, 0xbc bytes) */
AT(0xbc0) U32(0x00000002),  /* element 9: type */
/* +0x0bc4  meaning not yet known */
U32(0xffffffff), U32(0x0000005a), U8(0x13),
AT(0xbe4) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0bf0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0x7c),
AT(0xc78) U32(0x00000000),  /* element 9: text buffer */
/* +0x0c7c  meaning not yet known */
U32(0x00000000), U32(0x0000001f), U32(0x0000000e), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xcb8) STR12("Courier New"),
/* +0x0cd8  meaning not yet known */
AT(0xcd8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x0cf8  element 10 of 26: type 1 */
AT(0xcf8) U32(0x00000001),  /* element 10: type */
/* +0x0cfc  meaning not yet known */
U32(0x00002000), U32(0x00000041), U32(0x0000006c), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0d28  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x88),
/* +0x0d60  meaning not yet known */
AT(0xd60) U32(0x00000041), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0d6c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0dd8  element 11 of 26: type 1 */
AT(0xdd8) U32(0x00000001),  /* element 11: type */
/* +0x0ddc  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x0000006c), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e08  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0xab),
/* +0x0e40  meaning not yet known */
AT(0xe40) U32(0x00000095), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e4c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0eb8  element 12 of 26: type 1 */
AT(0xeb8) U32(0x00000001),  /* element 12: type */
/* +0x0ebc  meaning not yet known */
U32(0x00002000), U32(0x00000041), U32(0x000000b8), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ee8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x90),
/* +0x0f20  meaning not yet known */
AT(0xf20) U32(0x00000041), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0f2c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0f98  element 13 of 26: type 1 */
AT(0xf98) U32(0x00000001),  /* element 13: type */
/* +0x0f9c  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x000000b8), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0fc8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0xb3),
/* +0x1000  meaning not yet known */
AT(0x1000) U32(0x00000095), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x100c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x1078  element 14 of 26: type 1 */
AT(0x1078) U32(0x00000001),  /* element 14: type */
/* +0x107c  meaning not yet known */
U32(0x00002000), U32(0x00000041), U32(0x00000106), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x10a8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x87),
/* +0x10e0  meaning not yet known */
AT(0x10e0) U32(0x00000041), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x10ec  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x1158  element 15 of 26: type 1 */
AT(0x1158) U32(0x00000001),  /* element 15: type */
/* +0x115c  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x00000106), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1188  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0xaa),
/* +0x11c0  meaning not yet known */
AT(0x11c0) U32(0x00000095), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x11cc  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x1238  element 16 of 26: type 1 */
AT(0x1238) U32(0x00000001),  /* element 16: type */
/* +0x123c  meaning not yet known */
U32(0x00002000), U32(0x00000041), U32(0x00000149), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1268  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x8a),
/* +0x12a0  meaning not yet known */
AT(0x12a0) U32(0x00000041), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x12ac  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x1318  element 17 of 26: type 1 */
AT(0x1318) U32(0x00000001),  /* element 17: type */
/* +0x131c  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x00000149), U32(0x00000000), U32(0x00000000), U32(0xffffffe5),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 17: parent, written at run time */
U32(0x00000000),  /* element 17: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1348  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0xad),
/* +0x1380  meaning not yet known */
AT(0x1380) U32(0x00000095), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x138c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x13f8  element 18 of 26: type 0 */
AT(0x13f8) U32(0x00000000),  /* element 18: type */
/* +0x13fc  meaning not yet known */
U32(0x00003000), U32(0x00000026), U8(0x5b),
AT(0x141c) U32(0x00000000),  /* element 18: parent, written at run time */
U32(0x00000000),  /* element 18: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1428  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1448  element 19 of 26: type 0 */
AT(0x1448) U32(0x00000000),  /* element 19: type */
/* +0x144c  meaning not yet known */
U32(0x00003000), U32(0x00000079), U8(0x5b),
AT(0x146c) U32(0x00000000),  /* element 19: parent, written at run time */
U32(0x00000000),  /* element 19: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1478  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1498  element 20 of 26: type 0 */
AT(0x1498) U32(0x00000000),  /* element 20: type */
/* +0x149c  meaning not yet known */
U32(0x00004000), U32(0x00000026), U8(0xa6),
AT(0x14bc) U32(0x00000000),  /* element 20: parent, written at run time */
U32(0x00000000),  /* element 20: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x14c8  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x14e8  element 21 of 26: type 0 */
AT(0x14e8) U32(0x00000000),  /* element 21: type */
/* +0x14ec  meaning not yet known */
U32(0x00004000), U32(0x00000079), U8(0xa6),
AT(0x150c) U32(0x00000000),  /* element 21: parent, written at run time */
U32(0x00000000),  /* element 21: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1518  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1538  element 22 of 26: type 0 */
AT(0x1538) U32(0x00000000),  /* element 22: type */
/* +0x153c  meaning not yet known */
U32(0x00005000), U32(0x00000026), U8(0xed),
AT(0x155c) U32(0x00000000),  /* element 22: parent, written at run time */
U32(0x00000000),  /* element 22: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1568  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1588  element 23 of 26: type 0 */
AT(0x1588) U32(0x00000000),  /* element 23: type */
/* +0x158c  meaning not yet known */
U32(0x00005000), U32(0x00000079), U8(0xed),
AT(0x15ac) U32(0x00000000),  /* element 23: parent, written at run time */
U32(0x00000000),  /* element 23: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x15b8  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x15d8  element 24 of 26: type 0 */
AT(0x15d8) U32(0x00000000),  /* element 24: type */
/* +0x15dc  meaning not yet known */
U32(0x00006000), U32(0x00000026), U16(0x0134),
AT(0x15fc) U32(0x00000000),  /* element 24: parent, written at run time */
U32(0x00000000),  /* element 24: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1608  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1628  element 25 of 26: type 0 */
AT(0x1628) U32(0x00000000),  /* element 25: type */
/* +0x162c  meaning not yet known */
U32(0x00006000), U32(0x00000079), U16(0x0134),
AT(0x164c) U32(0x00000000),  /* element 25: parent, written at run time */
U32(0x00000000),  /* element 25: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1658  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1678  the children of element 26, zero-terminated */
AT(0x1678) U32(0x00000000),  /* element 26: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 13 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 14 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 15 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 16 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 17 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 18 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 19 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 20 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 21 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 22 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 23 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 24 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: child 25 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 26: end of the list */
/* +0x16e0  element 26 of 26: type 0 */
U32(0x00000000),  /* element 26: type */
/* +0x16e4  meaning not yet known */
U16(0x1000),
AT(0x1704) U32(0x00000000),  /* element 26: parent, written at run time */
U32(0x00000000),  /* element 26: children list (relocated: installed at attach, rebind.inc) */
/* +0x170c  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x1730  the element-list slot (the header's +0x08 points here): the first element */
AT(0x1730) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x1734  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x174c  meaning not yet known */
AT(0x174c) U32(0xffffffff), U16(0xa40f),
/* +0x1764  meaning not yet known */
AT(0x1764) U32(0x00000001), U32(0x0000002d), U32(0x0000000b), U32(0x0000007c), U32(0x0000002d), U32(0xffffffff),
U16(0xa43a),
/* +0x1790  meaning not yet known */
AT(0x1790) U8(0x03),
/* +0x17a4  meaning not yet known */
AT(0x17a4) U32(0xffffffff),
/* +0x17bc  meaning not yet known */
AT(0x17bc) U32(0x00000001), U32(0x00000019), U32(0x0000004a), U32(0x000000a5), U32(0x0000004e), U32(0xffffffff),
U16(0xa410),
/* +0x17e8  meaning not yet known */
AT(0x17e8) U8(0x03),
/* +0x17fc  meaning not yet known */
AT(0x17fc) U32(0xffffffff),
/* +0x1814  meaning not yet known */
AT(0x1814) U32(0x00000001), U32(0x00000019), U32(0x00000099), U32(0x000000a5), U32(0x0000004b), U32(0xffffffff),
U16(0xa411),
/* +0x1840  meaning not yet known */
AT(0x1840) U8(0x03),
/* +0x1854  meaning not yet known */
AT(0x1854) U32(0xffffffff),
/* +0x186c  meaning not yet known */
AT(0x186c) U32(0x00000001), U32(0x00000019), U32(0x000000e4), U32(0x000000a5), U32(0x00000046), U32(0xffffffff),
U16(0xa412),
/* +0x1898  meaning not yet known */
AT(0x1898) U8(0x03),
/* +0x18ac  meaning not yet known */
AT(0x18ac) U32(0xffffffff),
/* +0x18c4  meaning not yet known */
AT(0x18c4) U32(0x00000001), U32(0x00000019), U32(0x0000012a), U32(0x000000a5), U32(0x00000044), U32(0xffffffff),
U16(0xa413),
/* +0x18f0  meaning not yet known */
AT(0x18f0) U8(0x03),
/* +0x1904  meaning not yet known */
AT(0x1904) U32(0xffffffff),
/* +0x191c  meaning not yet known */
AT(0x191c) U8(0x03),
/* +0x1930  meaning not yet known */
AT(0x1930) U32(0xffffffff),
/* +0x195c  meaning not yet known */
AT(0x195c) U32(0xffffffff),
AT(0x1974) STR8("%03d"),
/* +0x197c  meaning not yet known */
U32(0x00004f54),
/* +0x1980  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x1988  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x1990  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x199c  meaning not yet known */
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
