/* 737HSI.GAU's .data image, decompiled: every non-zero value of retail's .data (0x13f8 bytes at RVA 0x3000) at
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
AT(0x0) STR16("737-400.HSI"),  /* the gauge name */
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
U16(0x0114),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x00000001), U8(0x03),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x0000000b), U8(0x01),
/* +0x00a0  meaning not yet known */
AT(0xa0) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x0000000e), U8(0x01),
/* +0x00c8  meaning not yet known */
AT(0xc8) U8(0x2c),
/* +0x00e8  meaning not yet known */
AT(0xe8) U8(0x2b),
/* +0x0108  meaning not yet known */
AT(0x108) U8(0x2e),
/* +0x0128  element 1 of 21: type 2 (text, 0xbc bytes) */
AT(0x128) U32(0x00000002),  /* element 1: type */
/* +0x012c  meaning not yet known */
U32(0xffffffff), U32(0x00000067), U8(0x29),
AT(0x14c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0158  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x51),
AT(0x1e0) U32(0x00000000),  /* element 1: text buffer */
/* +0x01e4  meaning not yet known */
U32(0x00000000), U32(0x0000002c), U32(0x00000016), U32(0x00000000), U32(0x00000000), U32(0x0000ffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x220) STR12("Courier New"),
/* +0x0240  meaning not yet known */
AT(0x240) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0260  element 2 of 21: type 2 (text, 0xbc bytes) */
AT(0x260) U32(0x00000002),  /* element 2: type */
/* +0x0264  meaning not yet known */
U32(0xffffffff), U32(0x00000037), U16(0x0108),
AT(0x284) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0290  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x18),
AT(0x318) U32(0x00000000),  /* element 2: text buffer */
/* +0x031c  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x0000ffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x358) STR12("Courier New"),
/* +0x0378  meaning not yet known */
AT(0x378) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x06),
/* +0x0398  element 3 of 21: type 2 (text, 0xbc bytes) */
AT(0x398) U32(0x00000002),  /* element 3: type */
/* +0x039c  meaning not yet known */
U32(0xffffffff), U32(0x000000c9), U16(0x0108),
AT(0x3bc) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03c8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x2d),
AT(0x450) U32(0x00000000),  /* element 3: text buffer */
/* +0x0454  meaning not yet known */
U32(0x00000000), U32(0x00000032), U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x0000ffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x490) STR12("Courier New"),
/* +0x04b0  meaning not yet known */
AT(0x4b0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x04d0  element 4 of 21: type 0 */
AT(0x4d0) U32(0x00000000),  /* element 4: type */
/* +0x04d4  meaning not yet known */
U32(0x00001200), U32(0x00000064), U8(0x2c),
AT(0x4f4) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0500  meaning not yet known */
U8(0x15),
/* +0x0520  element 5 of 21: type 4 */
AT(0x520) U32(0x00000004),  /* element 5: type */
/* +0x0524  meaning not yet known */
U32(0x00001300), U32(0x00000017), U16(0x010c),
AT(0x544) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0550  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2e),
/* +0x0590  meaning not yet known */
AT(0x590) U8(0x02),
/* +0x05a8  the children of element 6, zero-terminated */
AT(0x5a8) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x05c0  element 6 of 21: type 0 */
U32(0x00000000),  /* element 6: type */
/* +0x05c4  meaning not yet known */
U32(0x00001400), U32(0x00000093), U16(0x010d),
AT(0x5e4) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05f0  meaning not yet known */
U8(0x15),
/* +0x0610  element 7 of 21: type 0 */
AT(0x610) U32(0x00000000),  /* element 7: type */
/* +0x0614  meaning not yet known */
U32(0x00001500), U32(0x0000005f), U8(0x8c),
AT(0x634) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0640  meaning not yet known */
U8(0x17),
/* +0x0660  the children of element 8, zero-terminated */
AT(0x660) U32(0x00000000),  /* element 8: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: end of the list */
/* +0x0668  element 8 of 21: type 0 */
U32(0x00000000),  /* element 8: type */
/* +0x066c  meaning not yet known */
U32(0x00000700), U32(0x0000006c), U8(0x91),
AT(0x68c) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0698  meaning not yet known */
U8(0x17),
/* +0x06b8  the children of element 9, zero-terminated */
AT(0x6b8) U32(0x00000000),  /* element 9: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: end of the list */
/* +0x06c0  element 9 of 21: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 9: type */
/* +0x06c4  meaning not yet known */
U32(0xffffffff), U32(0x000000cd), U8(0x28),
AT(0x6e4) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06f0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x0718  meaning not yet known */
AT(0x718) U8(0x20),
/* +0x0734  meaning not yet known */
AT(0x734) U8(0x1e),
AT(0x778) U32(0x00000000),  /* element 9: text buffer */
/* +0x077c  meaning not yet known */
U32(0x00000000), U32(0x0000002d), U32(0x00000012), U32(0x00000000), U32(0x00000000), U32(0x00848484),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x7b8) STR12("Courier New"),
/* +0x07d8  meaning not yet known */
AT(0x7d8) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x03),
/* +0x07f8  the children of element 10, zero-terminated */
AT(0x7f8) U32(0x00000000),  /* element 10: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 10: end of the list */
/* +0x0800  element 10 of 21: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 10: type */
/* +0x0804  meaning not yet known */
U32(0xffffffff), U32(0x000000c8), U8(0x12),
AT(0x824) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0830  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x0858  meaning not yet known */
AT(0x858) U8(0x20),
/* +0x0874  meaning not yet known */
AT(0x874) U8(0x1e),
AT(0x8b8) U32(0x00000000),  /* element 10: text buffer */
/* +0x08bc  meaning not yet known */
U32(0x00000000), U32(0x0000002d), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x8f8) STR12("Courier New"),
/* +0x0918  meaning not yet known */
AT(0x918) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x03),
/* +0x0938  the children of element 11, zero-terminated */
AT(0x938) U32(0x00000000),  /* element 11: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: end of the list */
/* +0x0940  element 11 of 21: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 11: type */
/* +0x0944  meaning not yet known */
U32(0xffffffff), U32(0x00000023), U8(0x28),
AT(0x964) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0970  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x0998  meaning not yet known */
AT(0x998) U8(0x20),
/* +0x09b4  meaning not yet known */
AT(0x9b4) U8(0x1e),
AT(0x9f8) U32(0x00000000),  /* element 11: text buffer */
/* +0x09fc  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x00000012), U32(0x00000000), U32(0x00000000), U32(0x00848484),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xa38) STR12("Courier New"),
/* +0x0a58  meaning not yet known */
AT(0xa58) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x02),
/* +0x0a78  the children of element 12, zero-terminated */
AT(0xa78) U32(0x00000000),  /* element 12: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 12: end of the list */
/* +0x0a80  element 12 of 21: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 12: type */
/* +0x0a84  meaning not yet known */
U32(0xffffffff), U32(0x0000001e), U8(0x12),
AT(0xaa4) U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ab0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1f),
/* +0x0ad8  meaning not yet known */
AT(0xad8) U8(0x20),
/* +0x0af4  meaning not yet known */
AT(0xaf4) U8(0x1e),
AT(0xb38) U32(0x00000000),  /* element 12: text buffer */
/* +0x0b3c  meaning not yet known */
U32(0x00000000), U32(0x0000003c), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xb78) STR12("Courier New"),
/* +0x0b98  meaning not yet known */
AT(0xb98) U32(0x00000190), U32(0x00000001), U32(0x00000024), U8(0x04),
/* +0x0bb8  the children of element 13, zero-terminated */
AT(0xbb8) U32(0x00000000),  /* element 13: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: end of the list */
/* +0x0bc0  element 13 of 21: type 6 */
U32(0x00000006),  /* element 13: type */
/* +0x0bc4  meaning not yet known */
U32(0x00001000), U32(0x0000003f), U32(0x0000006c), U32(0x00000000), U32(0x00000000), U32(0x0000007b),
U32(0x000000a8),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0bf0  meaning not yet known */
U8(0x07),
/* +0x0c24  meaning not yet known */
AT(0xc24) U8(0x28),
/* +0x0c49  meaning not yet known */
AT(0xc49) U8(0xc0), U16(0xbfd7), U8(0x2d),
/* +0x0c72  meaning not yet known */
AT(0xc72) U16(0x3ff0), U32(0x3f99999a), U32(0x3f99999a), U32(0x00000080), U8(0x80),
/* +0x0d00  the children of element 14, zero-terminated */
AT(0xd00) U32(0x00000000),  /* element 14: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 14: end of the list */
/* +0x0d08  element 14 of 21: type 1 */
U32(0x00000001),  /* element 14: type */
/* +0x0d0c  meaning not yet known */
U32(0x00000900), U32(0x0000007b), U32(0x000000a8), U32(0x00000000), U32(0x00000000), U32(0xffffffe2),
U32(0x0000000c),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0d38  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x0d70  meaning not yet known */
AT(0xd70) U32(0xffffffe2),
/* +0x0de8  the children of element 15, zero-terminated */
AT(0xde8) U32(0x00000000),  /* element 15: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 15: end of the list */
/* +0x0df0  element 15 of 21: type 1 */
U32(0x00000001),  /* element 15: type */
/* +0x0df4  meaning not yet known */
U32(0x00000800), U32(0x0000007b), U32(0x000000a8), U32(0x00000000), U32(0x00000000), U32(0xffffffb7),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e20  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6b),
/* +0x0e58  meaning not yet known */
AT(0xe58) U32(0xffffffb7),
/* +0x0ed0  the children of element 16, zero-terminated */
AT(0xed0) U32(0x00000000),  /* element 16: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: end of the list */
/* +0x0ed8  element 16 of 21: type 1 */
U32(0x00000001),  /* element 16: type */
/* +0x0edc  meaning not yet known */
U32(0x00000600), U32(0x0000007b), U32(0x000000a8), U32(0x00000000), U32(0x00000000), U32(0x00000041),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0f08  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x0f40  meaning not yet known */
AT(0xf40) U8(0xae),
/* +0x0fb8  the children of element 17, zero-terminated */
AT(0xfb8) U32(0x00000000),  /* element 17: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: end of the list */
/* +0x0fc0  element 17 of 21: type 1 */
U32(0x00000001),  /* element 17: type */
/* +0x0fc4  meaning not yet known */
U32(0x00000500), U32(0x0000007b), U32(0x000000a8), U32(0x00000000), U32(0x00000000), U32(0x0000002d),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 17: parent, written at run time */
U32(0x00000000),  /* element 17: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ff0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x1028  meaning not yet known */
AT(0x1028) U8(0xae),
/* +0x10a0  the children of element 18, zero-terminated */
AT(0x10a0) U32(0x00000000),  /* element 18: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 18: end of the list */
/* +0x10a8  element 18 of 21: type 3 */
U32(0x00000003),  /* element 18: type */
/* +0x10ac  meaning not yet known */
U32(0x00000300), U32(0x000000ea), U8(0x4a),
AT(0x10cc) U32(0x00000000),  /* element 18: parent, written at run time */
U32(0x00000000),  /* element 18: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x10d8  meaning not yet known */
U8(0x07),
/* +0x1110  meaning not yet known */
AT(0x1110) U8(0x29),
/* +0x1135  meaning not yet known */
AT(0x1135) U8(0xa0), U16(0x3fe5),
/* +0x1150  the children of element 19, zero-terminated */
AT(0x1150) U32(0x00000000),  /* element 19: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 19: end of the list */
/* +0x1158  element 19 of 21: type 0 */
U32(0x00000000),  /* element 19: type */
/* +0x115c  meaning not yet known */
U32(0x00000200), U32(0x000000d8), U8(0x38),
AT(0x117c) U32(0x00000000),  /* element 19: parent, written at run time */
U32(0x00000000),  /* element 19: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1188  meaning not yet known */
U8(0x16),
/* +0x11a8  the children of element 20, zero-terminated */
AT(0x11a8) U32(0x00000000),  /* element 20: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 20: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 20: end of the list */
/* +0x11b4  meaning not yet known */
U32(0x00000000),
/* +0x11b8  element 20 of 21: type 1 */
U32(0x00000001),  /* element 20: type */
/* +0x11bc  meaning not yet known */
U32(0x00000400), U32(0x0000007b), U32(0x000000a8), U32(0x00000000), U32(0x00000000), U32(0x0000005a),
U32(0x0000005a),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 20: parent, written at run time */
U32(0x00000000),  /* element 20: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x11e8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x51),
/* +0x1220  meaning not yet known */
AT(0x1220) U8(0xae),
/* +0x1298  the children of element 21, zero-terminated */
AT(0x1298) U32(0x00000000),  /* element 21: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 21: end of the list */
/* +0x12a0  element 21 of 21: type 0 */
U32(0x00000000),  /* element 21: type */
/* +0x12a4  meaning not yet known */
U16(0x0100),
AT(0x12c4) U32(0x00000000),  /* element 21: parent, written at run time */
U32(0x00000000),  /* element 21: children list (relocated: installed at attach, rebind.inc) */
/* +0x12cc  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x12f0  the element-list slot (the header's +0x08 points here): the first element */
AT(0x12f0) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x12f4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x130c  meaning not yet known */
AT(0x130c) U32(0xffffffff), U16(0xa42a),
/* +0x1324  meaning not yet known */
AT(0x1324) U8(0x03),
/* +0x1338  meaning not yet known */
AT(0x1338) U32(0xffffffff),
/* +0x1364  meaning not yet known */
AT(0x1364) U32(0xffffffff),
AT(0x137c) STR8("%03d"),
STR12("1%d%d.%d%d"),
/* +0x1390  meaning not yet known */
U32(0x002d2d2d),
STR8("----"),
STR8("%d.%d"),
STR8("%04d"),
/* +0x13ac  meaning not yet known */
U32(0x0053544b), U32(0x00004d4e), U32(0x00000000),
/* +0x13b8  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x13c0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x13c8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x13d4  meaning not yet known */
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
