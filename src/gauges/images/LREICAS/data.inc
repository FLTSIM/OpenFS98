/* LREICAS.GAU's .data image, decompiled: every non-zero value of retail's .data (0x2f38 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR16("Lear_45.EICAS"),  /* the gauge name */
/* +0x0010  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x003c  meaning not yet known */
U16(0x0112),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x00000001), U8(0x03),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x0000000a), U8(0x01),
/* +0x00a0  meaning not yet known */
AT(0xa0) U16(0x0124),
/* +0x00c0  meaning not yet known */
AT(0xc0) U16(0x0126),
/* +0x00e0  meaning not yet known */
AT(0xe0) U16(0x012a),
/* +0x0100  meaning not yet known */
AT(0x100) U16(0x012c),
/* +0x0120  meaning not yet known */
AT(0x120) U16(0x0132),
/* +0x0140  meaning not yet known */
AT(0x140) U32(0x00000068), U8(0x28),
/* +0x0158  meaning not yet known */
AT(0x158) U32(0x00000045), U32(0x0000002c), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
U32(0x000000c1), U8(0x28),
/* +0x0188  meaning not yet known */
AT(0x188) U32(0x0000009e), U32(0x0000002c), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
/* +0x01a0  element 1 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 1: type */
/* +0x01a4  meaning not yet known */
U32(0xffffffff), U32(0x00000078), U8(0x37),
AT(0x1c4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01d0  meaning not yet known */
U8(0x07),
AT(0x258) U32(0x00000000),  /* element 1: text buffer */
/* +0x025c  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x298) STR12("Courier New"),
/* +0x02b8  meaning not yet known */
AT(0x2b8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x02d8  element 2 of 41: type 1 */
AT(0x2d8) U32(0x00000001),  /* element 2: type */
/* +0x02dc  meaning not yet known */
U32(0x00002000), U32(0x00000058), U32(0x00000038), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0308  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x88),
/* +0x0340  meaning not yet known */
AT(0x340) U32(0x00000058), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x034c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x03b8  the children of element 3, zero-terminated */
AT(0x3b8) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x03c0  element 3 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 3: type */
/* +0x03c4  meaning not yet known */
U32(0xffffffff), U32(0x00000041), U8(0x1c),
AT(0x3e4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03f0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x88),
AT(0x478) U32(0x00000000),  /* element 3: text buffer */
/* +0x047c  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x4b8) STR12("Courier New"),
/* +0x04d8  meaning not yet known */
AT(0x4d8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x04f8  the children of element 4, zero-terminated */
AT(0x4f8) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0500  element 4 of 41: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x0504  meaning not yet known */
U32(0x00004000), U32(0x00000039), U8(0x23),
AT(0x524) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0530  meaning not yet known */
U8(0x06),
/* +0x0550  element 5 of 41: type 1 */
AT(0x550) U32(0x00000001),  /* element 5: type */
/* +0x0554  meaning not yet known */
U32(0x00002000), U32(0x000000b2), U32(0x00000038), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0580  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xab),
/* +0x05b8  meaning not yet known */
AT(0x5b8) U32(0x000000b2), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05c4  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0630  the children of element 6, zero-terminated */
AT(0x630) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x0638  element 6 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 6: type */
/* +0x063c  meaning not yet known */
U32(0xffffffff), U32(0x000000a0), U8(0x1c),
AT(0x65c) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0668  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xab),
AT(0x6f0) U32(0x00000000),  /* element 6: text buffer */
/* +0x06f4  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x730) STR12("Courier New"),
/* +0x0750  meaning not yet known */
AT(0x750) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0770  the children of element 7, zero-terminated */
AT(0x770) U32(0x00000000),  /* element 7: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 7: end of the list */
/* +0x0778  element 7 of 41: type 0 */
U32(0x00000000),  /* element 7: type */
/* +0x077c  meaning not yet known */
U32(0x00004000), U32(0x00000098), U8(0x23),
AT(0x79c) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x07a8  meaning not yet known */
U8(0x06),
/* +0x07c8  meaning not yet known */
AT(0x7c8) U32(0x00000068), U8(0x5d),
/* +0x07e0  meaning not yet known */
AT(0x7e0) U32(0x00000042), U32(0x0000006e), U32(0x00000000), U32(0x408f3800), U32(0x00000000), U32(0x00000000),
U32(0x000000c1), U8(0x5d),
/* +0x0810  meaning not yet known */
AT(0x810) U32(0x0000009b), U32(0x0000006e), U32(0x00000000), U32(0x408f3800), U32(0x00000000), U32(0x00000000),
/* +0x0828  element 8 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 8: type */
/* +0x082c  meaning not yet known */
U32(0xffffffff), U32(0x00000071), U8(0x61),
AT(0x84c) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0858  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x90),
AT(0x8e0) U32(0x00000000),  /* element 8: text buffer */
/* +0x08e4  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x920) STR12("Courier New"),
/* +0x0940  meaning not yet known */
AT(0x940) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0960  element 9 of 41: type 1 */
AT(0x960) U32(0x00000001),  /* element 9: type */
/* +0x0964  meaning not yet known */
U32(0x00002000), U32(0x00000058), U32(0x0000006f), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0990  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x90),
/* +0x09c8  meaning not yet known */
AT(0x9c8) U32(0x00000058), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x09d4  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0a40  the children of element 10, zero-terminated */
AT(0xa40) U32(0x00000000),  /* element 10: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 10: end of the list */
/* +0x0a48  element 10 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 10: type */
/* +0x0a4c  meaning not yet known */
U32(0xffffffff), U32(0x00000041), U8(0x54),
AT(0xa6c) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a78  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x90),
AT(0xb00) U32(0x00000000),  /* element 10: text buffer */
/* +0x0b04  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0xb40) STR12("Courier New"),
/* +0x0b60  meaning not yet known */
AT(0xb60) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0b80  the children of element 11, zero-terminated */
AT(0xb80) U32(0x00000000),  /* element 11: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: end of the list */
/* +0x0b88  element 11 of 41: type 0 */
U32(0x00000000),  /* element 11: type */
/* +0x0b8c  meaning not yet known */
U32(0x00004000), U32(0x00000039), U8(0x58),
AT(0xbac) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0bb8  meaning not yet known */
U8(0x06),
/* +0x0bd8  element 12 of 41: type 1 */
AT(0xbd8) U32(0x00000001),  /* element 12: type */
/* +0x0bdc  meaning not yet known */
U32(0x00002000), U32(0x000000b2), U32(0x0000006f), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c08  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xb3),
/* +0x0c40  meaning not yet known */
AT(0xc40) U32(0x000000b2), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c4c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0cb8  the children of element 13, zero-terminated */
AT(0xcb8) U32(0x00000000),  /* element 13: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: end of the list */
/* +0x0cc0  element 13 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 13: type */
/* +0x0cc4  meaning not yet known */
U32(0xffffffff), U32(0x000000a0), U8(0x54),
AT(0xce4) U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0cf0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xb3),
AT(0xd78) U32(0x00000000),  /* element 13: text buffer */
/* +0x0d7c  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0xdb8) STR12("Courier New"),
/* +0x0dd8  meaning not yet known */
AT(0xdd8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x0df8  the children of element 14, zero-terminated */
AT(0xdf8) U32(0x00000000),  /* element 14: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 14: end of the list */
/* +0x0e00  element 14 of 41: type 0 */
U32(0x00000000),  /* element 14: type */
/* +0x0e04  meaning not yet known */
U32(0x00004000), U32(0x00000098), U8(0x58),
AT(0xe24) U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e30  meaning not yet known */
U8(0x06),
/* +0x0e50  meaning not yet known */
AT(0xe50) U32(0x00000068), U8(0xa5),
/* +0x0e68  meaning not yet known */
AT(0xe68) U32(0x00000042), U32(0x00000095), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
U32(0x000000c1), U8(0xa5),
/* +0x0e98  meaning not yet known */
AT(0xe98) U32(0x0000009b), U32(0x00000095), U32(0x00000000), U32(0x40d33440), U32(0x00000000), U32(0x00000000),
/* +0x0eb0  element 15 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 15: type */
/* +0x0eb4  meaning not yet known */
U32(0xffffffff), U32(0x00000078), U8(0x90),
AT(0xed4) U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ee0  meaning not yet known */
U8(0x07),
AT(0xf68) U32(0x00000000),  /* element 15: text buffer */
/* +0x0f6c  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0xfa8) STR12("Courier New"),
/* +0x0fc8  meaning not yet known */
AT(0xfc8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x0fe8  element 16 of 41: type 1 */
AT(0xfe8) U32(0x00000001),  /* element 16: type */
/* +0x0fec  meaning not yet known */
U32(0x00002000), U32(0x00000058), U32(0x000000a6), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1018  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x87),
/* +0x1050  meaning not yet known */
AT(0x1050) U32(0x00000058), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x105c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x10c8  the children of element 17, zero-terminated */
AT(0x10c8) U32(0x00000000),  /* element 17: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: end of the list */
/* +0x10d0  element 17 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 17: type */
/* +0x10d4  meaning not yet known */
U32(0xffffffff), U32(0x00000041), U8(0x8b),
AT(0x10f4) U32(0x00000000),  /* element 17: parent, written at run time */
U32(0x00000000),  /* element 17: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1100  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x87),
AT(0x1188) U32(0x00000000),  /* element 17: text buffer */
/* +0x118c  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x11c8) STR12("Courier New"),
/* +0x11e8  meaning not yet known */
AT(0x11e8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x1208  the children of element 18, zero-terminated */
AT(0x1208) U32(0x00000000),  /* element 18: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 18: end of the list */
/* +0x1210  element 18 of 41: type 0 */
U32(0x00000000),  /* element 18: type */
/* +0x1214  meaning not yet known */
U32(0x00004000), U32(0x00000039), U8(0x90),
AT(0x1234) U32(0x00000000),  /* element 18: parent, written at run time */
U32(0x00000000),  /* element 18: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1240  meaning not yet known */
U8(0x06),
/* +0x1260  element 19 of 41: type 1 */
AT(0x1260) U32(0x00000001),  /* element 19: type */
/* +0x1264  meaning not yet known */
U32(0x00002000), U32(0x000000b2), U32(0x000000a6), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000001),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 19: parent, written at run time */
U32(0x00000000),  /* element 19: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1290  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xaa),
/* +0x12c8  meaning not yet known */
AT(0x12c8) U32(0x000000b2), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x12d4  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x1340  the children of element 20, zero-terminated */
AT(0x1340) U32(0x00000000),  /* element 20: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 20: end of the list */
/* +0x1348  element 20 of 41: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 20: type */
/* +0x134c  meaning not yet known */
U32(0xffffffff), U32(0x000000a0), U8(0x8b),
AT(0x136c) U32(0x00000000),  /* element 20: parent, written at run time */
U32(0x00000000),  /* element 20: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1378  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xaa),
AT(0x1400) U32(0x00000000),  /* element 20: text buffer */
/* +0x1404  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1440) STR12("Courier New"),
/* +0x1460  meaning not yet known */
AT(0x1460) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x1480  the children of element 21, zero-terminated */
AT(0x1480) U32(0x00000000),  /* element 21: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 21: end of the list */
/* +0x1488  element 21 of 41: type 0 */
U32(0x00000000),  /* element 21: type */
/* +0x148c  meaning not yet known */
U32(0x00004000), U32(0x00000098), U8(0x90),
AT(0x14ac) U32(0x00000000),  /* element 21: parent, written at run time */
U32(0x00000000),  /* element 21: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x14b8  meaning not yet known */
U8(0x06),
/* +0x14d8  element 22 of 41: type 0 */
AT(0x14d8) U32(0x00000000),  /* element 22: type */
/* +0x14dc  meaning not yet known */
U32(0x00005000), U32(0x0000001a), U8(0xc0),
AT(0x14fc) U32(0x00000000),  /* element 22: parent, written at run time */
U32(0x00000000),  /* element 22: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1508  meaning not yet known */
U8(0x06),
/* +0x1528  element 23 of 41: type 2 (text, 0xbc bytes) */
AT(0x1528) U32(0x00000002),  /* element 23: type */
/* +0x152c  meaning not yet known */
U32(0xffffffff), U32(0x0000005f), U8(0xc3),
AT(0x154c) U32(0x00000000),  /* element 23: parent, written at run time */
U32(0x00000000),  /* element 23: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1558  meaning not yet known */
U8(0x07),
AT(0x15e0) U32(0x00000000),  /* element 23: text buffer */
/* +0x15e4  meaning not yet known */
U32(0x00000000), U32(0x00000046), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1620) STR12("Courier New"),
/* +0x1640  meaning not yet known */
AT(0x1640) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x07),
/* +0x1660  element 24 of 41: type 2 (text, 0xbc bytes) */
AT(0x1660) U32(0x00000002),  /* element 24: type */
/* +0x1664  meaning not yet known */
U32(0xffffffff), U32(0x00000069), U8(0xd7),
AT(0x1684) U32(0x00000000),  /* element 24: parent, written at run time */
U32(0x00000000),  /* element 24: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1690  meaning not yet known */
U8(0x07),
AT(0x1718) U32(0x00000000),  /* element 24: text buffer */
/* +0x171c  meaning not yet known */
U32(0x00000000), U32(0x00000032), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1758) STR12("Courier New"),
/* +0x1778  meaning not yet known */
AT(0x1778) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x05),
/* +0x1798  element 25 of 41: type 2 (text, 0xbc bytes) */
AT(0x1798) U32(0x00000002),  /* element 25: type */
/* +0x179c  meaning not yet known */
U32(0xffffffff), U32(0x00000064), U8(0xeb),
AT(0x17bc) U32(0x00000000),  /* element 25: parent, written at run time */
U32(0x00000000),  /* element 25: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x17c8  meaning not yet known */
U8(0x07),
AT(0x1850) U32(0x00000000),  /* element 25: text buffer */
/* +0x1854  meaning not yet known */
U32(0x00000000), U32(0x0000003c), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1890) STR12("Courier New"),
/* +0x18b0  meaning not yet known */
AT(0x18b0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x06),
/* +0x18d0  element 26 of 41: type 2 (text, 0xbc bytes) */
AT(0x18d0) U32(0x00000002),  /* element 26: type */
/* +0x18d4  meaning not yet known */
U32(0xffffffff), U32(0x00000028), U8(0xc3),
AT(0x18f4) U32(0x00000000),  /* element 26: parent, written at run time */
U32(0x00000000),  /* element 26: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1900  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x94),
AT(0x1988) U32(0x00000000),  /* element 26: text buffer */
/* +0x198c  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x19c8) STR12("Courier New"),
/* +0x19e8  meaning not yet known */
AT(0x19e8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x1a08  element 27 of 41: type 2 (text, 0xbc bytes) */
AT(0x1a08) U32(0x00000002),  /* element 27: type */
/* +0x1a0c  meaning not yet known */
U32(0xffffffff), U32(0x00000028), U8(0xd7),
AT(0x1a2c) U32(0x00000000),  /* element 27: parent, written at run time */
U32(0x00000000),  /* element 27: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1a38  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x93),
AT(0x1ac0) U32(0x00000000),  /* element 27: text buffer */
/* +0x1ac4  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1b00) STR12("Courier New"),
/* +0x1b20  meaning not yet known */
AT(0x1b20) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x1b40  element 28 of 41: type 2 (text, 0xbc bytes) */
AT(0x1b40) U32(0x00000002),  /* element 28: type */
/* +0x1b44  meaning not yet known */
U32(0xffffffff), U32(0x0000001e), U8(0xeb),
AT(0x1b64) U32(0x00000000),  /* element 28: parent, written at run time */
U32(0x00000000),  /* element 28: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1b70  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x8a),
AT(0x1bf8) U32(0x00000000),  /* element 28: text buffer */
/* +0x1bfc  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1c38) STR12("Courier New"),
/* +0x1c58  meaning not yet known */
AT(0x1c58) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x04),
/* +0x1c78  element 29 of 41: type 2 (text, 0xbc bytes) */
AT(0x1c78) U32(0x00000002),  /* element 29: type */
/* +0x1c7c  meaning not yet known */
U32(0xffffffff), U32(0x000000be), U8(0xc3),
AT(0x1c9c) U32(0x00000000),  /* element 29: parent, written at run time */
U32(0x00000000),  /* element 29: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1ca8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xb7),
AT(0x1d30) U32(0x00000000),  /* element 29: text buffer */
/* +0x1d34  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1d70) STR12("Courier New"),
/* +0x1d90  meaning not yet known */
AT(0x1d90) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x1db0  element 30 of 41: type 2 (text, 0xbc bytes) */
AT(0x1db0) U32(0x00000002),  /* element 30: type */
/* +0x1db4  meaning not yet known */
U32(0xffffffff), U32(0x000000be), U8(0xd7),
AT(0x1dd4) U32(0x00000000),  /* element 30: parent, written at run time */
U32(0x00000000),  /* element 30: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1de0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xb6),
AT(0x1e68) U32(0x00000000),  /* element 30: text buffer */
/* +0x1e6c  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1ea8) STR12("Courier New"),
/* +0x1ec8  meaning not yet known */
AT(0x1ec8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x1ee8  element 31 of 41: type 2 (text, 0xbc bytes) */
AT(0x1ee8) U32(0x00000002),  /* element 31: type */
/* +0x1eec  meaning not yet known */
U32(0xffffffff), U32(0x000000b4), U8(0xeb),
AT(0x1f0c) U32(0x00000000),  /* element 31: parent, written at run time */
U32(0x00000000),  /* element 31: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1f18  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0xad),
AT(0x1fa0) U32(0x00000000),  /* element 31: text buffer */
/* +0x1fa4  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x1fe0) STR12("Courier New"),
/* +0x2000  meaning not yet known */
AT(0x2000) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x04),
/* +0x2020  element 32 of 41: type 0 */
AT(0x2020) U32(0x00000000),  /* element 32: type */
/* +0x2024  meaning not yet known */
U32(0x00005000), U32(0x0000001a), U8(0xfe),
AT(0x2044) U32(0x00000000),  /* element 32: parent, written at run time */
U32(0x00000000),  /* element 32: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2050  meaning not yet known */
U8(0x06),
/* +0x2070  element 33 of 41: type 2 (text, 0xbc bytes) */
AT(0x2070) U32(0x00000002),  /* element 33: type */
/* +0x2074  meaning not yet known */
U32(0xffffffff), U32(0x0000000f), U16(0x0102),
AT(0x2094) U32(0x00000000),  /* element 33: parent, written at run time */
U32(0x00000000),  /* element 33: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x20a0  meaning not yet known */
U8(0x07),
AT(0x2128) U32(0x00000000),  /* element 33: text buffer */
/* +0x212c  meaning not yet known */
U32(0x00000000), U32(0x0000003c), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x2168) STR12("Courier New"),
/* +0x2188  meaning not yet known */
AT(0x2188) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x06),
/* +0x21a8  element 34 of 41: type 2 (text, 0xbc bytes) */
AT(0x21a8) U32(0x00000002),  /* element 34: type */
/* +0x21ac  meaning not yet known */
U32(0xffffffff), U32(0x00000082), U16(0x0102),
AT(0x21cc) U32(0x00000000),  /* element 34: parent, written at run time */
U32(0x00000000),  /* element 34: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x21d8  meaning not yet known */
U8(0x07),
AT(0x2260) U32(0x00000000),  /* element 34: text buffer */
/* +0x2264  meaning not yet known */
U32(0x00000000), U32(0x0000003c), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x22a0) STR12("Courier New"),
/* +0x22c0  meaning not yet known */
AT(0x22c0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x06),
/* +0x22e0  element 35 of 41: type 2 (text, 0xbc bytes) */
AT(0x22e0) U32(0x00000002),  /* element 35: type */
/* +0x22e4  meaning not yet known */
U32(0xffffffff), U32(0x00000014), U16(0x0116),
AT(0x2304) U32(0x00000000),  /* element 35: parent, written at run time */
U32(0x00000000),  /* element 35: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2310  meaning not yet known */
U8(0x07),
AT(0x2398) U32(0x00000000),  /* element 35: text buffer */
/* +0x239c  meaning not yet known */
U32(0x00000000), U32(0x00000032), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x23d8) STR12("Courier New"),
/* +0x23f8  meaning not yet known */
AT(0x23f8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x05),
/* +0x2418  element 36 of 41: type 2 (text, 0xbc bytes) */
AT(0x2418) U32(0x00000002),  /* element 36: type */
/* +0x241c  meaning not yet known */
U32(0xffffffff), U32(0x00000082), U16(0x0116),
AT(0x243c) U32(0x00000000),  /* element 36: parent, written at run time */
U32(0x00000000),  /* element 36: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2448  meaning not yet known */
U8(0x07),
AT(0x24d0) U32(0x00000000),  /* element 36: text buffer */
/* +0x24d4  meaning not yet known */
U32(0x00000000), U32(0x00000032), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00965a32),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x2510) STR12("Courier New"),
/* +0x2530  meaning not yet known */
AT(0x2530) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x05),
/* +0x2550  element 37 of 41: type 2 (text, 0xbc bytes) */
AT(0x2550) U32(0x00000002),  /* element 37: type */
/* +0x2554  meaning not yet known */
U32(0xffffffff), U32(0x00000050), U16(0x0102),
AT(0x2574) U32(0x00000000),  /* element 37: parent, written at run time */
U32(0x00000000),  /* element 37: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2580  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0123),
/* +0x25a8  meaning not yet known */
AT(0x25a8) U16(0x0125),
AT(0x2608) U32(0x00000000),  /* element 37: text buffer */
/* +0x260c  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x2648) STR12("Courier New"),
/* +0x2668  meaning not yet known */
AT(0x2668) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x04),
/* +0x2688  element 38 of 41: type 2 (text, 0xbc bytes) */
AT(0x2688) U32(0x00000002),  /* element 38: type */
/* +0x268c  meaning not yet known */
U32(0xffffffff), U32(0x000000c8), U16(0x0102),
AT(0x26ac) U32(0x00000000),  /* element 38: parent, written at run time */
U32(0x00000000),  /* element 38: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x26b8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0129),
/* +0x26e0  meaning not yet known */
AT(0x26e0) U16(0x012b),
AT(0x2740) U32(0x00000000),  /* element 38: text buffer */
/* +0x2744  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x2780) STR12("Courier New"),
/* +0x27a0  meaning not yet known */
AT(0x27a0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x04),
/* +0x27c0  element 39 of 41: type 2 (text, 0xbc bytes) */
AT(0x27c0) U32(0x00000002),  /* element 39: type */
/* +0x27c4  meaning not yet known */
U32(0xffffffff), U32(0x0000005a), U16(0x0116),
AT(0x27e4) U32(0x00000000),  /* element 39: parent, written at run time */
U32(0x00000000),  /* element 39: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x27f0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x015e),
AT(0x2878) U32(0x00000000),  /* element 39: text buffer */
/* +0x287c  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x28b8) STR12("Courier New"),
/* +0x28d8  meaning not yet known */
AT(0x28d8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x28f8  element 40 of 41: type 2 (text, 0xbc bytes) */
AT(0x28f8) U32(0x00000002),  /* element 40: type */
/* +0x28fc  meaning not yet known */
U32(0xffffffff), U32(0x000000d2), U16(0x0116),
AT(0x291c) U32(0x00000000),  /* element 40: parent, written at run time */
U32(0x00000000),  /* element 40: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2928  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0148),
AT(0x29b0) U32(0x00000000),  /* element 40: text buffer */
/* +0x29b4  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00080808), U16(0x5c5c), U8(0x5c),
AT(0x29f0) STR12("Courier New"),
/* +0x2a10  meaning not yet known */
AT(0x2a10) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x03),
/* +0x2a30  the children of element 41, zero-terminated */
AT(0x2a30) U32(0x00000000),  /* element 41: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 13 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 14 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 15 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 16 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 17 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 18 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 19 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 20 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 21 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 22 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 23 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 24 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 25 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 26 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 27 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: child 28 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 41: end of the list */
/* +0x2aa4  meaning not yet known */
U32(0x00000000),
/* +0x2aa8  element 41 of 41: type 0 */
U32(0x00000000),  /* element 41: type */
/* +0x2aac  meaning not yet known */
U16(0x1000),
AT(0x2acc) U32(0x00000000),  /* element 41: parent, written at run time */
U32(0x00000000),  /* element 41: children list (relocated: installed at attach, rebind.inc) */
/* +0x2ad4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x2af8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x2af8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x2afc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x2b14  meaning not yet known */
AT(0x2b14) U32(0xffffffff),
/* +0x2b2c  meaning not yet known */
AT(0x2b2c) U32(0x00000001), U32(0x0000003a), U32(0x00000024), U32(0x00000091), U32(0x00000036), U32(0xffffffff),
U16(0xa10b),
/* +0x2b58  meaning not yet known */
AT(0x2b58) U8(0x03),
/* +0x2b6c  meaning not yet known */
AT(0x2b6c) U32(0xffffffff),
/* +0x2b84  meaning not yet known */
AT(0x2b84) U32(0x00000001), U32(0x0000003a), U32(0x00000059), U32(0x00000091), U32(0x00000036), U32(0xffffffff),
U16(0xa10a),
/* +0x2bb0  meaning not yet known */
AT(0x2bb0) U8(0x03),
/* +0x2bc4  meaning not yet known */
AT(0x2bc4) U32(0xffffffff),
/* +0x2bdc  meaning not yet known */
AT(0x2bdc) U32(0x00000001), U32(0x0000003a), U32(0x0000007a), U32(0x00000091), U32(0x00000036), U32(0xffffffff),
U16(0xa109),
/* +0x2c08  meaning not yet known */
AT(0x2c08) U8(0x03),
/* +0x2c1c  meaning not yet known */
AT(0x2c1c) U32(0xffffffff),
/* +0x2c34  meaning not yet known */
AT(0x2c34) U32(0x00000001), U32(0x0000001e), U32(0x000000c3), U32(0x000000be), U32(0x00000014), U32(0xffffffff),
U16(0xa10d),
/* +0x2c60  meaning not yet known */
AT(0x2c60) U8(0x03),
/* +0x2c74  meaning not yet known */
AT(0x2c74) U32(0xffffffff),
/* +0x2c8c  meaning not yet known */
AT(0x2c8c) U32(0x00000001), U32(0x0000001e), U32(0x000000d7), U32(0x000000be), U32(0x00000014), U32(0xffffffff),
U16(0xa10c),
/* +0x2cb8  meaning not yet known */
AT(0x2cb8) U8(0x03),
/* +0x2ccc  meaning not yet known */
AT(0x2ccc) U32(0xffffffff),
/* +0x2ce4  meaning not yet known */
AT(0x2ce4) U32(0x00000001), U32(0x0000001e), U32(0x000000eb), U32(0x000000be), U32(0x00000014), U32(0xffffffff),
U16(0xa10e),
/* +0x2d10  meaning not yet known */
AT(0x2d10) U8(0x03),
/* +0x2d24  meaning not yet known */
AT(0x2d24) U32(0xffffffff),
/* +0x2d3c  meaning not yet known */
AT(0x2d3c) U32(0x00000001), U32(0x0000001e), U32(0x000000ff), U32(0x000000be), U32(0x00000014), U32(0xffffffff),
U16(0xa10f),
/* +0x2d68  meaning not yet known */
AT(0x2d68) U8(0x03),
/* +0x2d7c  meaning not yet known */
AT(0x2d7c) U32(0xffffffff),
/* +0x2d94  meaning not yet known */
AT(0x2d94) U32(0x00000001), U32(0x0000001e), U32(0x00000113), U32(0x00000050), U32(0x00000014), U32(0xffffffff),
U16(0xa12f),
/* +0x2dc0  meaning not yet known */
AT(0x2dc0) U8(0x03),
/* +0x2dd4  meaning not yet known */
AT(0x2dd4) U32(0xffffffff),
/* +0x2dec  meaning not yet known */
AT(0x2dec) U32(0x00000001), U32(0x00000082), U32(0x00000113), U32(0x00000064), U32(0x00000014), U32(0xffffffff),
U16(0xa126),
/* +0x2e18  meaning not yet known */
AT(0x2e18) U8(0x03),
/* +0x2e2c  meaning not yet known */
AT(0x2e2c) U32(0xffffffff),
/* +0x2e44  meaning not yet known */
AT(0x2e44) U8(0x03),
/* +0x2e58  meaning not yet known */
AT(0x2e58) U32(0xffffffff),
/* +0x2e84  meaning not yet known */
AT(0x2e84) U32(0xffffffff),
/* +0x2e9c  meaning not yet known */
AT(0x2e9c) U32(0x0000314e),
STR8("%03d"),
/* +0x2ea8  meaning not yet known */
U32(0x00545449), U32(0x0000324e),
STR8("Oil PSI"),
STR8("Oil c"),
STR8("FF pph"),
STR8("%04d"),
STR8("Fuel L"),
STR8("Fuel R"),
STR8("SAT c"),
STR8("Flaps"),
STR8("-%02d"),
/* +0x2ef8  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x2f00  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x2f08  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x2f14  meaning not yet known */
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
#undef STR16
