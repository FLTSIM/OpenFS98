/* LEARPFD.GAU's .data image, decompiled: every non-zero value of retail's .data (0x2b34 bytes at RVA 0x3000) at
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
AT(0x0) STR16("Lear_45.PFD"),  /* the gauge name */
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
U16(0x0111),
/* +0x0060  meaning not yet known */
AT(0x60) U8(0x2c),
/* +0x0080  meaning not yet known */
AT(0x80) U8(0x2b),
/* +0x00a0  meaning not yet known */
AT(0xa0) U8(0x2e),
/* +0x00c0  meaning not yet known */
AT(0xc0) U8(0x37),
/* +0x00dc  meaning not yet known */
AT(0xdc) U32(0x00000001), U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x0000000e), U8(0x01),
/* +0x0108  meaning not yet known */
AT(0x108) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000007), U32(0x00000001), U32(0x00000000),
U32(0x00000003), U8(0x01),
/* +0x0138  meaning not yet known */
AT(0x138) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000006), U32(0x00000001), U32(0x00000000),
U32(0x00000003), U8(0x01),
/* +0x0168  meaning not yet known */
AT(0x168) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000008), U8(0x01),
/* +0x0190  meaning not yet known */
AT(0x190) U32(0x00000001), U8(0x03),
/* +0x01a8  meaning not yet known */
AT(0x1a8) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x0000000b), U8(0x01),
/* +0x01d0  meaning not yet known */
AT(0x1d0) U8(0x12),
/* +0x01e8  meaning not yet known */
AT(0x1e8) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000013), U32(0x00000001), U32(0x00000000),
U32(0x00000003), U8(0x02),
/* +0x0218  meaning not yet known */
AT(0x218) U32(0x00000005), U32(0x00000002), U32(0x00000000), U32(0x00000001), U8(0x01),
/* +0x0240  meaning not yet known */
AT(0x240) U32(0x000000ee), U32(0x00000119), U32(0x00000000), U32(0xc0af4000), U32(0x00000000), U32(0x00000000),
U32(0x000000ea), U32(0x00000112), U32(0x00000000), U32(0xc0a77000), U32(0x00000000), U32(0x00000000),
U32(0x000000e3), U32(0x00000109), U32(0x00000000), U32(0xc09f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000dd), U32(0x000000ff), U32(0x00000000), U32(0xc08f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000d5), U8(0xef),
/* +0x02b8  meaning not yet known */
AT(0x2b8) U32(0x000000dd), U32(0x000000e0), U32(0x00000000), U32(0x408f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000e3), U32(0x000000d7), U32(0x00000000), U32(0x409f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000ea), U32(0x000000ce), U32(0x00000000), U32(0x40a77000), U32(0x00000000), U32(0x00000000),
U32(0x000000ee), U32(0x000000c5), U32(0x00000000), U32(0x40af4000), U32(0x00000000), U32(0x00000000),
/* +0x0318  element 1 of 35: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x031c  meaning not yet known */
U32(0x00001200), U32(0x000000f7), U32(0x000000f0), U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000004),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0348  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x41),
/* +0x0380  meaning not yet known */
AT(0x380) U32(0x00000025), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x038c  meaning not yet known */
U32(0x00000009), U8(0x06),
/* +0x03f8  the children of element 2, zero-terminated */
AT(0x3f8) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0400  element 2 of 35: type 0 */
U32(0x00000000),  /* element 2: type */
/* +0x0404  meaning not yet known */
U32(0x00001100), U32(0x000000cc), U8(0xc3),
AT(0x424) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0430  meaning not yet known */
U8(0x15),
/* +0x0450  element 3 of 35: type 2 (text, 0xbc bytes) */
AT(0x450) U32(0x00000002),  /* element 3: type */
/* +0x0454  meaning not yet known */
U32(0xffffffff), U32(0x00000017), U8(0x5a),
AT(0x474) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0480  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x3d),
AT(0x508) U32(0x00000000),  /* element 3: text buffer */
/* +0x050c  meaning not yet known */
U32(0x00000000), U32(0x00000020), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x548) STR12("Courier New"),
/* +0x0568  meaning not yet known */
AT(0x568) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0588  the children of element 4, zero-terminated */
AT(0x588) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0590  element 4 of 35: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x0594  meaning not yet known */
U32(0x00001450), U32(0x00000015), U8(0x58),
AT(0x5b4) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05c0  meaning not yet known */
U8(0x17),
/* +0x05e0  the children of element 5, zero-terminated */
AT(0x5e0) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x05e8  element 5 of 35: type 5 */
U32(0x00000005),  /* element 5: type */
/* +0x05ec  meaning not yet known */
U32(0x00001400), U32(0x00000015), U8(0x1e),
AT(0x60c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0618  meaning not yet known */
U8(0x07),
/* +0x0658  meaning not yet known */
AT(0x658) U8(0x3d),
/* +0x0685  meaning not yet known */
AT(0x685) U8(0x40), U16(0x408f),
/* +0x06e0  element 6 of 35: type 2 (text, 0xbc bytes) */
AT(0x6e0) U32(0x00000002),  /* element 6: type */
/* +0x06e4  meaning not yet known */
U32(0xffffffff), U32(0x000000af), U8(0xae),
AT(0x704) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0710  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4c),
AT(0x798) U32(0x00000000),  /* element 6: text buffer */
/* +0x079c  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x7d8) STR12("Courier New"),
/* +0x07f8  meaning not yet known */
AT(0x7f8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x08),
/* +0x0818  the children of element 7, zero-terminated */
AT(0x818) U32(0x00000000),  /* element 7: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 7: end of the list */
/* +0x0820  element 7 of 35: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 7: type */
/* +0x0824  meaning not yet known */
U32(0xffffffff), U32(0x000000cc), U8(0x59),
AT(0x844) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0850  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4a),
AT(0x8d8) U32(0x00000000),  /* element 7: text buffer */
/* +0x08dc  meaning not yet known */
U32(0x00000000), U32(0x00000030), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x918) STR12("Courier New"),
/* +0x0938  meaning not yet known */
AT(0x938) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x0958  the children of element 8, zero-terminated */
AT(0x958) U32(0x00000000),  /* element 8: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: end of the list */
/* +0x0960  element 8 of 35: type 5 */
U32(0x00000005),  /* element 8: type */
/* +0x0964  meaning not yet known */
U32(0x00001500), U32(0x000000ca), U8(0x1e),
AT(0x984) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0990  meaning not yet known */
U8(0x07),
/* +0x09d0  meaning not yet known */
AT(0x9d0) U8(0x4a),
/* +0x09fd  meaning not yet known */
AT(0x9fd) U8(0x40), U16(0x408f),
/* +0x0a58  element 9 of 35: type 3 */
AT(0xa58) U32(0x00000003),  /* element 9: type */
/* +0x0a5c  meaning not yet known */
U32(0x00002450), U32(0x000000b5), U8(0x54),
AT(0xa7c) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a88  meaning not yet known */
U8(0x07),
/* +0x0ac0  meaning not yet known */
AT(0xac0) U8(0x29),
/* +0x0ae5  meaning not yet known */
AT(0xae5) U8(0xc0), U16(0x3fd2),
/* +0x0b00  the children of element 10, zero-terminated */
AT(0xb00) U32(0x00000000),  /* element 10: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 10: end of the list */
/* +0x0b08  element 10 of 35: type 0 */
U32(0x00000000),  /* element 10: type */
/* +0x0b0c  meaning not yet known */
U32(0x00002400), U32(0x000000b8), U8(0x4b),
AT(0xb2c) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b38  meaning not yet known */
U8(0x16),
/* +0x0b58  element 11 of 35: type 1 */
AT(0xb58) U32(0x00000001),  /* element 11: type */
/* +0x0b5c  meaning not yet known */
U32(0x00005000), U32(0x00000080), U32(0x00000071), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x0000002e),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b88  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x4f),
/* +0x0c38  the children of element 12, zero-terminated */
AT(0xc38) U32(0x00000000),  /* element 12: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 12: end of the list */
/* +0x0c40  element 12 of 35: type 6 */
U32(0x00000006),  /* element 12: type */
/* +0x0c44  meaning not yet known */
U32(0x00001600), U32(0x00000050), U32(0x00000037), U32(0x00000000), U32(0x00000000), U32(0x00000080),
U32(0x00000073),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c70  meaning not yet known */
U8(0x05),
/* +0x0ca2  meaning not yet known */
AT(0xca2) U16(0x3ff0), U8(0x4d),
/* +0x0cc4  meaning not yet known */
AT(0xcc4) U32(0x8b439581), U32(0x3ff3e76c), U8(0x4f),
/* +0x0cf2  meaning not yet known */
AT(0xcf2) U16(0xbff0), U32(0x3f400000), U32(0x3f400000), U32(0x0000000b), U8(0x2e),
/* +0x0d80  element 13 of 35: type 3 */
AT(0xd80) U32(0x00000003),  /* element 13: type */
/* +0x0d84  meaning not yet known */
U32(0x00001300), U32(0x00000085), U16(0x0150),
AT(0xda4) U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0db0  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x45),
/* +0x0de2  meaning not yet known */
AT(0xde2) U16(0x3fb8), U32(0x00000000), U8(0x45),
/* +0x0e0e  meaning not yet known */
AT(0xe0e) U16(0x3f88),
/* +0x0e28  element 14 of 35: type 0 */
AT(0xe28) U32(0x00000000),  /* element 14: type */
/* +0x0e2c  meaning not yet known */
U32(0x00001700), U32(0x0000007a), U8(0xeb),
AT(0xe4c) U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e58  meaning not yet known */
U8(0x17),
/* +0x0e78  element 15 of 35: type 0 */
AT(0xe78) U32(0x00000000),  /* element 15: type */
/* +0x0e7c  meaning not yet known */
U32(0x00002350), U32(0x0000007c), U8(0xb7),
AT(0xe9c) U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ea8  meaning not yet known */
U8(0x17),
/* +0x0ec8  the children of element 16, zero-terminated */
AT(0xec8) U32(0x00000000),  /* element 16: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: end of the list */
/* +0x0ed4  meaning not yet known */
U32(0x00000000),
/* +0x0ed8  element 16 of 35: type 1 */
U32(0x00000001),  /* element 16: type */
/* +0x0edc  meaning not yet known */
U32(0x00002100), U32(0x00000080), U32(0x000000f2), U32(0x00000000), U32(0x00000000), U32(0x00000023),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0f08  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x0f40  meaning not yet known */
AT(0xf40) U8(0x2e),
/* +0x0fb8  the children of element 17, zero-terminated */
AT(0xfb8) U32(0x00000000),  /* element 17: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: end of the list */
/* +0x0fc0  element 17 of 35: type 6 */
U32(0x00000006),  /* element 17: type */
/* +0x0fc4  meaning not yet known */
U32(0x00001800), U32(0x0000004f), U32(0x000000c1), U32(0x00000000), U32(0x00000000), U32(0x00000080),
U32(0x000000f2),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 17: parent, written at run time */
U32(0x00000000),  /* element 17: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ff0  meaning not yet known */
U8(0x07),
/* +0x1024  meaning not yet known */
AT(0x1024) U8(0x28),
/* +0x1049  meaning not yet known */
AT(0x1049) U8(0x80), U16(0xbfd8), U8(0x2d),
/* +0x1072  meaning not yet known */
AT(0x1072) U16(0x3ff0), U32(0x400ccccd), U32(0x3ff33333), U32(0x0000005a), U8(0x5a),
/* +0x1100  the children of element 18, zero-terminated */
AT(0x1100) U32(0x00000000),  /* element 18: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 18: end of the list */
/* +0x1108  element 18 of 35: type 1 */
U32(0x00000001),  /* element 18: type */
/* +0x110c  meaning not yet known */
U32(0x00005500), U32(0x00000080), U32(0x000000f2), U32(0x00000000), U32(0x00000000), U32(0x00000023),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 18: parent, written at run time */
U32(0x00000000),  /* element 18: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1138  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x3b),
/* +0x1170  meaning not yet known */
AT(0x1170) U32(0x00000064), U8(0x0a),
/* +0x11e8  the children of element 19, zero-terminated */
AT(0x11e8) U32(0x00000000),  /* element 19: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 19: end of the list */
/* +0x11f0  element 19 of 35: type 1 */
U32(0x00000001),  /* element 19: type */
/* +0x11f4  meaning not yet known */
U32(0x00003000), U32(0x00000080), U32(0x000000f2), U32(0x00000000), U32(0x00000000), U32(0x00000023),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 19: parent, written at run time */
U32(0x00000000),  /* element 19: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1220  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x27),
/* +0x1258  meaning not yet known */
AT(0x1258) U8(0x46),
/* +0x12d0  the children of element 20, zero-terminated */
AT(0x12d0) U32(0x00000000),  /* element 20: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 20: end of the list */
/* +0x12d8  element 20 of 35: type 1 */
U32(0x00000001),  /* element 20: type */
/* +0x12dc  meaning not yet known */
U32(0x00001900), U32(0x00000080), U32(0x000000f2), U32(0x00000000), U32(0x00000000), U32(0xfffffff1),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 20: parent, written at run time */
U32(0x00000000),  /* element 20: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1308  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x1340  meaning not yet known */
AT(0x1340) U32(0xffffffec),
/* +0x13b8  the children of element 21, zero-terminated */
AT(0x13b8) U32(0x00000000),  /* element 21: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 21: end of the list */
/* +0x13c0  element 21 of 35: type 1 */
U32(0x00000001),  /* element 21: type */
/* +0x13c4  meaning not yet known */
U32(0x00002000), U32(0x00000080), U32(0x000000f2), U32(0x00000000), U32(0x00000000), U32(0xffffffcd),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 21: parent, written at run time */
U32(0x00000000),  /* element 21: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x13f0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6b),
/* +0x14a0  the children of element 22, zero-terminated */
AT(0x14a0) U32(0x00000000),  /* element 22: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: end of the list */
/* +0x14a8  element 22 of 35: type 1 */
U32(0x00000001),  /* element 22: type */
/* +0x14ac  meaning not yet known */
U32(0x00002250), U32(0x00000080), U32(0x000000f2), U32(0x00000000), U32(0x00000000), U32(0x00000019),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 22: parent, written at run time */
U32(0x00000000),  /* element 22: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x14d8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x1510  meaning not yet known */
AT(0x1510) U8(0xae),
/* +0x1588  the children of element 23, zero-terminated */
AT(0x1588) U32(0x00000000),  /* element 23: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 23: end of the list */
/* +0x1590  element 23 of 35: type 1 */
U32(0x00000001),  /* element 23: type */
/* +0x1594  meaning not yet known */
U32(0x00002200), U32(0x00000080), U32(0x000000f2), U32(0x00000000), U32(0x00000000), U32(0x00000019),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 23: parent, written at run time */
U32(0x00000000),  /* element 23: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x15c0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x15f8  meaning not yet known */
AT(0x15f8) U8(0xae),
/* +0x1670  the children of element 24, zero-terminated */
AT(0x1670) U32(0x00000000),  /* element 24: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 24: end of the list */
/* +0x1678  element 24 of 35: type 6 */
U32(0x00000006),  /* element 24: type */
/* +0x167c  meaning not yet known */
U32(0x00002300), U32(0x0000004a), U32(0x000000bc), U32(0x00000000), U32(0x00000000), U32(0x00000080),
U32(0x000000f2),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 24: parent, written at run time */
U32(0x00000000),  /* element 24: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x16a8  meaning not yet known */
U8(0x07),
/* +0x1704  meaning not yet known */
AT(0x1704) U8(0x51),
/* +0x172a  meaning not yet known */
AT(0x172a) U16(0x3ff0), U32(0x4017b426), U32(0x4017b426), U8(0x6c),
/* +0x17b8  element 25 of 35: type 2 (text, 0xbc bytes) */
AT(0x17b8) U32(0x00000002),  /* element 25: type */
/* +0x17bc  meaning not yet known */
U32(0xffffffff), U32(0x000000b6), U16(0x010e),
AT(0x17dc) U32(0x00000000),  /* element 25: parent, written at run time */
U32(0x00000000),  /* element 25: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x17e8  meaning not yet known */
U8(0x06),
AT(0x1870) U32(0x00000000),  /* element 25: text buffer */
/* +0x1874  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x18b0) STR12("Courier New"),
/* +0x18d0  meaning not yet known */
AT(0x18d0) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x03),
/* +0x18f0  element 26 of 35: type 2 (text, 0xbc bytes) */
AT(0x18f0) U32(0x00000002),  /* element 26: type */
/* +0x18f4  meaning not yet known */
U32(0xffffffff), U32(0x000000b6), U16(0x011d),
AT(0x1914) U32(0x00000000),  /* element 26: parent, written at run time */
U32(0x00000000),  /* element 26: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1920  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x51),
AT(0x19a8) U32(0x00000000),  /* element 26: text buffer */
/* +0x19ac  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x19e8) STR12("Courier New"),
/* +0x1a08  meaning not yet known */
AT(0x1a08) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x03),
/* +0x1a28  element 27 of 35: type 2 (text, 0xbc bytes) */
AT(0x1a28) U32(0x00000002),  /* element 27: type */
/* +0x1a2c  meaning not yet known */
U32(0xffffffff), U32(0x00000012), U8(0xb4),
AT(0x1a4c) U32(0x00000000),  /* element 27: parent, written at run time */
U32(0x00000000),  /* element 27: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1a58  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x1a80  meaning not yet known */
AT(0x1a80) U8(0x20),
/* +0x1a9c  meaning not yet known */
AT(0x1a9c) U8(0x1e),
AT(0x1ae0) U32(0x00000000),  /* element 27: text buffer */
/* +0x1ae4  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x1b20) STR12("Courier New"),
/* +0x1b40  meaning not yet known */
AT(0x1b40) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x04),
/* +0x1b60  the children of element 28, zero-terminated */
AT(0x1b60) U32(0x00000000),  /* element 28: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: end of the list */
/* +0x1b68  element 28 of 35: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 28: type */
/* +0x1b6c  meaning not yet known */
U32(0xffffffff), U32(0x00000012), U8(0xc4),
AT(0x1b8c) U32(0x00000000),  /* element 28: parent, written at run time */
U32(0x00000000),  /* element 28: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1b98  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x1bc0  meaning not yet known */
AT(0x1bc0) U8(0x20),
/* +0x1bdc  meaning not yet known */
AT(0x1bdc) U8(0x1e),
AT(0x1c20) U32(0x00000000),  /* element 28: text buffer */
/* +0x1c24  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x1c60) STR12("Courier New"),
/* +0x1c80  meaning not yet known */
AT(0x1c80) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x03),
/* +0x1ca0  the children of element 29, zero-terminated */
AT(0x1ca0) U32(0x00000000),  /* element 29: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 29: end of the list */
/* +0x1ca8  element 29 of 35: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 29: type */
/* +0x1cac  meaning not yet known */
U32(0xffffffff), U32(0x00000012), U8(0xd6),
AT(0x1ccc) U32(0x00000000),  /* element 29: parent, written at run time */
U32(0x00000000),  /* element 29: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1cd8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x1d00  meaning not yet known */
AT(0x1d00) U8(0x20),
/* +0x1d1c  meaning not yet known */
AT(0x1d1c) U8(0x1e),
AT(0x1d60) U32(0x00000000),  /* element 29: text buffer */
/* +0x1d64  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x1da0) STR12("Courier New"),
/* +0x1dc0  meaning not yet known */
AT(0x1dc0) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x04),
/* +0x1de0  element 30 of 35: type 2 (text, 0xbc bytes) */
AT(0x1de0) U32(0x00000002),  /* element 30: type */
/* +0x1de4  meaning not yet known */
U32(0xffffffff), U32(0x00000012), U8(0xfa),
AT(0x1e04) U32(0x00000000),  /* element 30: parent, written at run time */
U32(0x00000000),  /* element 30: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1e10  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x23),
/* +0x1e38  meaning not yet known */
AT(0x1e38) U8(0x24),
/* +0x1e54  meaning not yet known */
AT(0x1e54) U8(0x22),
AT(0x1e98) U32(0x00000000),  /* element 30: text buffer */
/* +0x1e9c  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x00848484),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x1ed8) STR12("Courier New"),
/* +0x1ef8  meaning not yet known */
AT(0x1ef8) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x04),
/* +0x1f18  the children of element 31, zero-terminated */
AT(0x1f18) U32(0x00000000),  /* element 31: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 31: end of the list */
/* +0x1f20  element 31 of 35: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 31: type */
/* +0x1f24  meaning not yet known */
U32(0xffffffff), U32(0x00000012), U16(0x010a),
AT(0x1f44) U32(0x00000000),  /* element 31: parent, written at run time */
U32(0x00000000),  /* element 31: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1f50  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x23),
/* +0x1f78  meaning not yet known */
AT(0x1f78) U8(0x24),
/* +0x1f94  meaning not yet known */
AT(0x1f94) U8(0x22),
AT(0x1fd8) U32(0x00000000),  /* element 31: text buffer */
/* +0x1fdc  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x00848484),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x2018) STR12("Courier New"),
/* +0x2038  meaning not yet known */
AT(0x2038) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x03),
/* +0x2058  the children of element 32, zero-terminated */
AT(0x2058) U32(0x00000000),  /* element 32: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 32: end of the list */
/* +0x2060  element 32 of 35: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 32: type */
/* +0x2064  meaning not yet known */
U32(0xffffffff), U32(0x00000012), U16(0x011c),
AT(0x2084) U32(0x00000000),  /* element 32: parent, written at run time */
U32(0x00000000),  /* element 32: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2090  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x23),
/* +0x20b8  meaning not yet known */
AT(0x20b8) U8(0x24),
/* +0x20d4  meaning not yet known */
AT(0x20d4) U8(0x22),
AT(0x2118) U32(0x00000000),  /* element 32: text buffer */
/* +0x211c  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x00848484),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x2158) STR12("Courier New"),
/* +0x2178  meaning not yet known */
AT(0x2178) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x04),
/* +0x2198  element 33 of 35: type 4 */
AT(0x2198) U32(0x00000004),  /* element 33: type */
/* +0x219c  meaning not yet known */
U32(0x00004000), U32(0x000000a8), U8(0x1e),
AT(0x21bc) U32(0x00000000),  /* element 33: parent, written at run time */
U32(0x00000000),  /* element 33: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x21c8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0162),
/* +0x2208  meaning not yet known */
AT(0x2208) U8(0x03),
/* +0x2220  element 34 of 35: type 2 (text, 0xbc bytes) */
AT(0x2220) U32(0x00000002),  /* element 34: type */
/* +0x2224  meaning not yet known */
U32(0xffffffff), U32(0x00000053), U8(0x1e),
AT(0x2244) U32(0x00000000),  /* element 34: parent, written at run time */
U32(0x00000000),  /* element 34: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2250  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x40),
AT(0x22d8) U32(0x00000000),  /* element 34: text buffer */
/* +0x22dc  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x2318) STR12("Courier New"),
/* +0x2338  meaning not yet known */
AT(0x2338) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x08),
/* +0x2358  the children of element 35, zero-terminated */
AT(0x2358) U32(0x00000000),  /* element 35: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: child 13 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 35: end of the list */
/* +0x2390  element 35 of 35: type 0 */
U32(0x00000000),  /* element 35: type */
/* +0x2394  meaning not yet known */
U16(0x1000),
AT(0x23b4) U32(0x00000000),  /* element 35: parent, written at run time */
U32(0x00000000),  /* element 35: children list (relocated: installed at attach, rebind.inc) */
/* +0x23bc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x23e0  the element-list slot (the header's +0x08 points here): the first element */
AT(0x23e0) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x23e4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x23fc  meaning not yet known */
AT(0x23fc) U32(0xffffffff),
/* +0x2414  meaning not yet known */
AT(0x2414) U32(0x00000001), U32(0x0000002e), U32(0x00000143), U32(0x00000028), U32(0x0000001e), U32(0xffffffff),
U16(0xa13d),
/* +0x2440  meaning not yet known */
AT(0x2440) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000028), U32(0x0000001e), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x2468  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x2480  meaning not yet known */
AT(0x2480) U32(0xffffffff),
/* +0x2498  meaning not yet known */
AT(0x2498) U32(0x00000001), U32(0x000000bd), U32(0x00000144), U32(0x00000028), U32(0x0000001e), U32(0xffffffff),
U16(0xa1de),
/* +0x24c4  meaning not yet known */
AT(0x24c4) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000028), U32(0x0000001e), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x24ec  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x2504  meaning not yet known */
AT(0x2504) U32(0xffffffff),
/* +0x251c  meaning not yet known */
AT(0x251c) U32(0x00000001), U32(0x000000e6), U32(0x00000152), U32(0x00000026), U32(0x00000026), U32(0xffffffff),
U16(0xa102),
/* +0x2548  meaning not yet known */
AT(0x2548) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000000f), U32(0x0000001e), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001015c), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000000f), U32(0x00000000), U32(0x0000000f), U32(0x0000001e), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001015b), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x25b4  meaning not yet known */
AT(0x25b4) U32(0xffffffff),
/* +0x25cc  meaning not yet known */
AT(0x25cc) U32(0x00000001), U32(0x00000014), U32(0x0000001f), U32(0x0000002d), U32(0x00000090), U32(0xffffffff),
U16(0xa103),
/* +0x25f8  meaning not yet known */
AT(0x25f8) U8(0x03),
/* +0x260c  meaning not yet known */
AT(0x260c) U32(0xffffffff),
/* +0x2624  meaning not yet known */
AT(0x2624) U32(0x00000001), U32(0x00000050), U32(0x0000001e), U32(0x00000037), U32(0x00000015), U32(0xffffffff),
U16(0xa1e1),
/* +0x2650  meaning not yet known */
AT(0x2650) U8(0x03),
/* +0x2664  meaning not yet known */
AT(0x2664) U32(0xffffffff),
/* +0x267c  meaning not yet known */
AT(0x267c) U32(0x00000001), U32(0x0000004f), U32(0x00000037), U32(0x00000061), U32(0x00000076), U32(0xffffffff),
U16(0xa105),
/* +0x26a8  meaning not yet known */
AT(0x26a8) U8(0x03),
/* +0x26bc  meaning not yet known */
AT(0x26bc) U32(0xffffffff),
/* +0x26d4  meaning not yet known */
AT(0x26d4) U32(0x00000001), U32(0x000000a6), U32(0x0000001e), U32(0x00000020), U32(0x00000020), U32(0xffffffff),
U16(0xa41c),
/* +0x2700  meaning not yet known */
AT(0x2700) U8(0x03),
/* +0x2714  meaning not yet known */
AT(0x2714) U32(0xffffffff),
/* +0x272c  meaning not yet known */
AT(0x272c) U32(0x00000001), U32(0x000000b0), U32(0x0000003d), U32(0x00000014), U32(0x00000074), U32(0xffffffff),
U16(0xa13c),
/* +0x2758  meaning not yet known */
AT(0x2758) U8(0x03),
/* +0x276c  meaning not yet known */
AT(0x276c) U32(0xffffffff),
/* +0x2784  meaning not yet known */
AT(0x2784) U32(0x00000001), U32(0x000000c5), U32(0x0000001f), U32(0x00000038), U32(0x00000090), U32(0xffffffff),
U16(0xa101),
/* +0x27b0  meaning not yet known */
AT(0x27b0) U8(0x03),
/* +0x27c4  meaning not yet known */
AT(0x27c4) U32(0xffffffff),
/* +0x27dc  meaning not yet known */
AT(0x27dc) U32(0x00000001), U32(0x00000010), U32(0x000000ba), U32(0x0000002b), U32(0x00000034), U32(0xffffffff),
U16(0xa135),
/* +0x2808  meaning not yet known */
AT(0x2808) U8(0x03),
/* +0x281c  meaning not yet known */
AT(0x281c) U32(0xffffffff),
/* +0x2834  meaning not yet known */
AT(0x2834) U32(0x00000001), U32(0x00000010), U32(0x00000104), U32(0x0000002b), U32(0x00000034), U32(0xffffffff),
U16(0xa136),
/* +0x2860  meaning not yet known */
AT(0x2860) U8(0x03),
/* +0x2874  meaning not yet known */
AT(0x2874) U32(0xffffffff),
/* +0x288c  meaning not yet known */
AT(0x288c) U32(0x00000001), U32(0x00000048), U32(0x000000bc), U32(0x00000070), U32(0x00000077), U32(0xffffffff),
U16(0xa137),
/* +0x28b8  meaning not yet known */
AT(0x28b8) U8(0x03),
/* +0x28cc  meaning not yet known */
AT(0x28cc) U32(0xffffffff),
/* +0x28e4  meaning not yet known */
AT(0x28e4) U32(0x00000001), U32(0x000000b9), U32(0x000000b6), U32(0x0000003e), U32(0x00000010), U32(0xffffffff),
/* +0x2910  meaning not yet known */
AT(0x2910) U8(0x03),
/* +0x2924  meaning not yet known */
AT(0x2924) U32(0xffffffff),
/* +0x293c  meaning not yet known */
AT(0x293c) U32(0x00000001), U32(0x000000b4), U32(0x0000010c), U32(0x00000022), U32(0x00000024), U32(0xffffffff),
U16(0xa118),
/* +0x2968  meaning not yet known */
AT(0x2968) U8(0x03),
/* +0x297c  meaning not yet known */
AT(0x297c) U32(0xffffffff),
/* +0x2994  meaning not yet known */
AT(0x2994) U32(0x00000001), U32(0x000000ca), U32(0x000000c6), U32(0x00000030), U32(0x00000063), U32(0xffffffff),
U16(0xa133),
/* +0x29c0  meaning not yet known */
AT(0x29c0) U8(0x03),
/* +0x29d4  meaning not yet known */
AT(0x29d4) U32(0xffffffff),
/* +0x29ec  meaning not yet known */
AT(0x29ec) U32(0x00000001), U32(0x00000058), U32(0x00000148), U32(0x00000061), U32(0x00000043), U32(0xffffffff),
U16(0xa130),
/* +0x2a18  meaning not yet known */
AT(0x2a18) U8(0x03),
/* +0x2a2c  meaning not yet known */
AT(0x2a2c) U32(0xffffffff),
/* +0x2a44  meaning not yet known */
AT(0x2a44) U8(0x03),
/* +0x2a58  meaning not yet known */
AT(0x2a58) U32(0xffffffff),
/* +0x2a84  meaning not yet known */
AT(0x2a84) U32(0xffffffff),
AT(0x2a9c) STR8("%03d"),
STR16("%02d.%02d in"),
STR8("%05d"),
/* +0x2abc  meaning not yet known */
U32(0x00474448),
STR8("%d.%d"),
STR8("%04d"),
STR8("VOR1"),
STR8("VOR2"),
STR8("M .%02d"),
/* +0x2ae8  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x2af0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x2af8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x2b04  meaning not yet known */
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
