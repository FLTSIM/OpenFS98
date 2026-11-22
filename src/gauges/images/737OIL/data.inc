/* 737OIL.GAU's .data image, decompiled: every non-zero value of retail's .data (0x1578 bytes at RVA 0x3000) at
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
AT(0x0) STR16("737-400.Oil"),  /* the gauge name */
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
U8(0xa1),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x00000001), U8(0x03),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x00000041), U8(0x61),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x00000041), U32(0x00000046), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x0000007c), U8(0x61),
/* +0x00c0  meaning not yet known */
AT(0xc0) U32(0x0000007c), U32(0x00000046), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x00000041), U8(0xa2),
/* +0x00f0  meaning not yet known */
AT(0xf0) U32(0x00000041), U32(0x0000008b), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x0000007c), U8(0xa2),
/* +0x0120  meaning not yet known */
AT(0x120) U32(0x0000007c), U32(0x0000008b), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x00000041), U8(0xf6),
/* +0x0150  meaning not yet known */
AT(0x150) U32(0x00000041), U32(0x000000dc), U32(0x00000000), U32(0x40d00000), U32(0x00000000), U32(0x00000000),
U32(0x0000007c), U8(0xf6),
/* +0x0180  meaning not yet known */
AT(0x180) U32(0x0000007c), U32(0x000000dc), U32(0x00000000), U32(0x40d00000), U32(0x00000000), U32(0x00000000),
U32(0x00000041), U16(0x013b),
/* +0x01b0  meaning not yet known */
AT(0x1b0) U32(0x00000041), U32(0x00000124), U32(0x00000000), U32(0x40d00000), U32(0x00000000), U32(0x00000000),
U32(0x0000007c), U16(0x013b),
/* +0x01e0  meaning not yet known */
AT(0x1e0) U32(0x0000007c), U32(0x00000124), U32(0x00000000), U32(0x40d00000), U32(0x00000000), U32(0x00000000),
/* +0x01f8  element 1 of 22: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 1: type */
/* +0x01fc  meaning not yet known */
U32(0xffffffff), U32(0x0000001f), U8(0xb7),
AT(0x21c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0228  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0x95),
AT(0x2b0) U32(0x00000000),  /* element 1: text buffer */
/* +0x02b4  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x2f0) STR12("Courier New"),
/* +0x0310  meaning not yet known */
AT(0x310) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0330  element 2 of 22: type 2 (text, 0xbc bytes) */
AT(0x330) U32(0x00000002),  /* element 2: type */
/* +0x0334  meaning not yet known */
U32(0xffffffff), U32(0x00000061), U8(0xb7),
AT(0x354) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0360  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0xb8),
AT(0x3e8) U32(0x00000000),  /* element 2: text buffer */
/* +0x03ec  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x428) STR12("Courier New"),
/* +0x0448  meaning not yet known */
AT(0x448) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0468  element 3 of 22: type 2 (text, 0xbc bytes) */
AT(0x468) U32(0x00000002),  /* element 3: type */
/* +0x046c  meaning not yet known */
U32(0xffffffff), U32(0x00000022), U16(0x014d),
AT(0x48c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0498  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0x97),
AT(0x520) U32(0x00000000),  /* element 3: text buffer */
/* +0x0524  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x560) STR12("Courier New"),
/* +0x0580  meaning not yet known */
AT(0x580) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x05a0  element 4 of 22: type 2 (text, 0xbc bytes) */
AT(0x5a0) U32(0x00000002),  /* element 4: type */
/* +0x05a4  meaning not yet known */
U32(0xffffffff), U32(0x0000005d), U16(0x014d),
AT(0x5c4) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05d0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U8(0xba),
AT(0x658) U32(0x00000000),  /* element 4: text buffer */
/* +0x065c  meaning not yet known */
U32(0x00000000), U32(0x00000023), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x698) STR12("Courier New"),
/* +0x06b8  meaning not yet known */
AT(0x6b8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x06d8  element 5 of 22: type 2 (text, 0xbc bytes) */
AT(0x6d8) U32(0x00000002),  /* element 5: type */
/* +0x06dc  meaning not yet known */
U32(0xffffffff), U32(0x00000045), U8(0x17),
AT(0x6fc) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0708  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000010), U16(0x015e),
AT(0x790) U32(0x00000000),  /* element 5: text buffer */
/* +0x0794  meaning not yet known */
U32(0x00000000), U32(0x0000001e), U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x7d0) STR12("Courier New"),
/* +0x07f0  meaning not yet known */
AT(0x7f0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0810  element 6 of 22: type 1 */
AT(0x810) U32(0x00000001),  /* element 6: type */
/* +0x0814  meaning not yet known */
U32(0x00002000), U32(0x00000039), U32(0x00000054), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0840  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x94),
/* +0x0878  meaning not yet known */
AT(0x878) U32(0x00000039), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0884  meaning not yet known */
U8(0x02),
/* +0x08f0  element 7 of 22: type 1 */
AT(0x8f0) U32(0x00000001),  /* element 7: type */
/* +0x08f4  meaning not yet known */
U32(0x00002000), U32(0x00000074), U32(0x00000054), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0920  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0xb7),
/* +0x0958  meaning not yet known */
AT(0x958) U32(0x00000074), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0964  meaning not yet known */
U8(0x02),
/* +0x09d0  element 8 of 22: type 1 */
AT(0x9d0) U32(0x00000001),  /* element 8: type */
/* +0x09d4  meaning not yet known */
U32(0x00002000), U32(0x00000039), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a00  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x93),
/* +0x0a38  meaning not yet known */
AT(0xa38) U32(0x00000039), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a44  meaning not yet known */
U8(0x02),
/* +0x0ab0  element 9 of 22: type 1 */
AT(0xab0) U32(0x00000001),  /* element 9: type */
/* +0x0ab4  meaning not yet known */
U32(0x00002000), U32(0x00000074), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ae0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0xb6),
/* +0x0b18  meaning not yet known */
AT(0xb18) U32(0x00000074), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b24  meaning not yet known */
U8(0x02),
/* +0x0b90  element 10 of 22: type 1 */
AT(0xb90) U32(0x00000001),  /* element 10: type */
/* +0x0b94  meaning not yet known */
U32(0x00002000), U32(0x00000039), U32(0x000000e8), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0bc0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x99),
/* +0x0bf8  meaning not yet known */
AT(0xbf8) U32(0x00000039), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c04  meaning not yet known */
U8(0x02),
/* +0x0c70  element 11 of 22: type 1 */
AT(0xc70) U32(0x00000001),  /* element 11: type */
/* +0x0c74  meaning not yet known */
U32(0x00002000), U32(0x00000074), U32(0x000000e8), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ca0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0xbc),
/* +0x0cd8  meaning not yet known */
AT(0xcd8) U32(0x00000074), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ce4  meaning not yet known */
U8(0x02),
/* +0x0d50  element 12 of 22: type 1 */
AT(0xd50) U32(0x00000001),  /* element 12: type */
/* +0x0d54  meaning not yet known */
U32(0x00002000), U32(0x00000039), U32(0x0000012f), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0d80  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x96),
/* +0x0db8  meaning not yet known */
AT(0xdb8) U32(0x00000039), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0dc4  meaning not yet known */
U8(0x02),
/* +0x0e30  element 13 of 22: type 1 */
AT(0xe30) U32(0x00000001),  /* element 13: type */
/* +0x0e34  meaning not yet known */
U32(0x00002000), U32(0x00000074), U32(0x0000012f), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000002),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e60  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000010), U8(0x96),
/* +0x0e98  meaning not yet known */
AT(0xe98) U32(0x00000074), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ea4  meaning not yet known */
U8(0x02),
/* +0x0f10  element 14 of 22: type 0 */
AT(0xf10) U32(0x00000000),  /* element 14: type */
/* +0x0f14  meaning not yet known */
U32(0x00003000), U32(0x00000023), U8(0x40),
AT(0xf34) U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0f40  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x0f60  element 15 of 22: type 0 */
AT(0xf60) U32(0x00000000),  /* element 15: type */
/* +0x0f64  meaning not yet known */
U32(0x00003000), U32(0x0000005d), U8(0x40),
AT(0xf84) U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0f90  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x0fb0  element 16 of 22: type 0 */
AT(0xfb0) U32(0x00000000),  /* element 16: type */
/* +0x0fb4  meaning not yet known */
U32(0x00004000), U32(0x00000023), U8(0x82),
AT(0xfd4) U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0fe0  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1000  element 17 of 22: type 0 */
AT(0x1000) U32(0x00000000),  /* element 17: type */
/* +0x1004  meaning not yet known */
U32(0x00004000), U32(0x0000005d), U8(0x82),
AT(0x1024) U32(0x00000000),  /* element 17: parent, written at run time */
U32(0x00000000),  /* element 17: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1030  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1050  element 18 of 22: type 0 */
AT(0x1050) U32(0x00000000),  /* element 18: type */
/* +0x1054  meaning not yet known */
U32(0x00005000), U32(0x00000023), U8(0xd3),
AT(0x1074) U32(0x00000000),  /* element 18: parent, written at run time */
U32(0x00000000),  /* element 18: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1080  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x10a0  element 19 of 22: type 0 */
AT(0x10a0) U32(0x00000000),  /* element 19: type */
/* +0x10a4  meaning not yet known */
U32(0x00005000), U32(0x0000005d), U8(0xd3),
AT(0x10c4) U32(0x00000000),  /* element 19: parent, written at run time */
U32(0x00000000),  /* element 19: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x10d0  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x10f0  element 20 of 22: type 0 */
AT(0x10f0) U32(0x00000000),  /* element 20: type */
/* +0x10f4  meaning not yet known */
U32(0x00006000), U32(0x00000023), U16(0x0119),
AT(0x1114) U32(0x00000000),  /* element 20: parent, written at run time */
U32(0x00000000),  /* element 20: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1120  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1140  element 21 of 22: type 0 */
AT(0x1140) U32(0x00000000),  /* element 21: type */
/* +0x1144  meaning not yet known */
U32(0x00006000), U32(0x0000005d), U16(0x0119),
AT(0x1164) U32(0x00000000),  /* element 21: parent, written at run time */
U32(0x00000000),  /* element 21: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1170  meaning not yet known */
U32(0x00000015), U32(0x00000000), U8(0x10),
/* +0x1190  the children of element 22, zero-terminated */
AT(0x1190) U32(0x00000000),  /* element 22: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 13 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 14 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 15 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 16 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 17 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 18 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 19 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 20 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: child 21 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 22: end of the list */
/* +0x11e8  element 22 of 22: type 0 */
U32(0x00000000),  /* element 22: type */
/* +0x11ec  meaning not yet known */
U16(0x1000),
AT(0x120c) U32(0x00000000),  /* element 22: parent, written at run time */
U32(0x00000000),  /* element 22: children list (relocated: installed at attach, rebind.inc) */
/* +0x1214  meaning not yet known */
U32(0x00000000), U32(0x00000011), U32(0x00000000), U8(0x08),
/* +0x1238  the element-list slot (the header's +0x08 points here): the first element */
AT(0x1238) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x123c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x1254  meaning not yet known */
AT(0x1254) U32(0xffffffff), U16(0xa417),
/* +0x126c  meaning not yet known */
AT(0x126c) U32(0x00000001), U32(0x00000030), U32(0x00000011), U32(0x00000040), U32(0x00000018), U32(0xffffffff),
U16(0xa431),
/* +0x1298  meaning not yet known */
AT(0x1298) U8(0x03),
/* +0x12ac  meaning not yet known */
AT(0x12ac) U32(0xffffffff),
/* +0x12c4  meaning not yet known */
AT(0x12c4) U32(0x00000001), U32(0x0000001a), U32(0x00000036), U32(0x0000006f), U32(0x00000042), U32(0xffffffff),
U16(0xa418),
/* +0x12f0  meaning not yet known */
AT(0x12f0) U8(0x03),
/* +0x1304  meaning not yet known */
AT(0x1304) U32(0xffffffff),
/* +0x131c  meaning not yet known */
AT(0x131c) U32(0x00000001), U32(0x0000001a), U32(0x00000079), U32(0x0000006f), U32(0x0000003e), U32(0xffffffff),
U16(0xa419),
/* +0x1348  meaning not yet known */
AT(0x1348) U8(0x03),
/* +0x135c  meaning not yet known */
AT(0x135c) U32(0xffffffff),
/* +0x1374  meaning not yet known */
AT(0x1374) U32(0x00000001), U32(0x00000023), U32(0x000000b7), U32(0x0000005e), U32(0x00000014), U32(0xffffffff),
U16(0xa41a),
/* +0x13a0  meaning not yet known */
AT(0x13a0) U8(0x03),
/* +0x13b4  meaning not yet known */
AT(0x13b4) U32(0xffffffff),
/* +0x13cc  meaning not yet known */
AT(0x13cc) U32(0x00000001), U32(0x0000001a), U32(0x000000cc), U32(0x0000006f), U32(0x0000003c), U32(0xffffffff),
U16(0xa433),
/* +0x13f8  meaning not yet known */
AT(0x13f8) U8(0x03),
/* +0x140c  meaning not yet known */
AT(0x140c) U32(0xffffffff),
/* +0x1424  meaning not yet known */
AT(0x1424) U32(0x00000001), U32(0x0000001a), U32(0x00000111), U32(0x0000006f), U32(0x0000003c), U32(0xffffffff),
U16(0xa41b),
/* +0x1450  meaning not yet known */
AT(0x1450) U8(0x03),
/* +0x1464  meaning not yet known */
AT(0x1464) U32(0xffffffff),
/* +0x147c  meaning not yet known */
AT(0x147c) U32(0x00000001), U32(0x00000026), U32(0x0000014d), U32(0x00000055), U32(0x00000016), U32(0xffffffff),
U16(0xa434),
/* +0x14a8  meaning not yet known */
AT(0x14a8) U8(0x03),
/* +0x14bc  meaning not yet known */
AT(0x14bc) U32(0xffffffff),
/* +0x14d4  meaning not yet known */
AT(0x14d4) U8(0x03),
/* +0x14e8  meaning not yet known */
AT(0x14e8) U32(0xffffffff),
/* +0x1514  meaning not yet known */
AT(0x1514) U32(0xffffffff),
AT(0x152c) STR8("%03d"),
STR8("%02d"),
/* +0x153c  meaning not yet known */
U32(0x00000000),
/* +0x1540  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x1548  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x1550  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x155c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x156c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

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
