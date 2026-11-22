/* LEARAUTO.GAU's .data image, decompiled: every non-zero value of retail's .data (0x15fc bytes at RVA 0x3000) at
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
AT(0x0) STR24("Lear_45.Autopilot.h"),  /* the gauge name */
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
U16(0x017c),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  element 1 of 16: type 2 (text, 0xbc bytes) */
AT(0x80) U32(0x00000002),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0xffffffff), U32(0x00000021), U8(0x58),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00b0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x2d),
AT(0x138) U32(0x00000000),  /* element 1: text buffer */
/* +0x013c  meaning not yet known */
U32(0x00000000), U32(0x0000004b), U32(0x0000001f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x178) STR12("Courier New"),
/* +0x0198  meaning not yet known */
AT(0x198) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x01b8  element 2 of 16: type 2 (text, 0xbc bytes) */
AT(0x1b8) U32(0x00000002),  /* element 2: type */
/* +0x01bc  meaning not yet known */
U32(0xffffffff), U32(0x0000007a), U8(0x58),
AT(0x1dc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01e8  meaning not yet known */
U32(0x00000006), U32(0x00000002), U32(0x00000000), U8(0x6b),
AT(0x270) U32(0x00000000),  /* element 2: text buffer */
/* +0x0274  meaning not yet known */
U32(0x00000000), U32(0x0000004b), U32(0x0000001f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x2b0) STR12("Courier New"),
/* +0x02d0  meaning not yet known */
AT(0x2d0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x02f0  element 3 of 16: type 2 (text, 0xbc bytes) */
AT(0x2f0) U32(0x00000002),  /* element 3: type */
/* +0x02f4  meaning not yet known */
U32(0xffffffff), U32(0x000000f0), U8(0x58),
AT(0x314) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0320  meaning not yet known */
U32(0x00000006), U32(0x00000001), U32(0x00000000), U8(0x6d),
AT(0x3a8) U32(0x00000000),  /* element 3: text buffer */
/* +0x03ac  meaning not yet known */
U32(0x00000000), U32(0x0000007d), U32(0x0000001f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x3e8) STR12("Courier New"),
/* +0x0408  meaning not yet known */
AT(0x408) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x0428  element 4 of 16: type 2 (text, 0xbc bytes) */
AT(0x428) U32(0x00000002),  /* element 4: type */
/* +0x042c  meaning not yet known */
U32(0xffffffff), U32(0x00000179), U8(0x58),
AT(0x44c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0458  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x78),
AT(0x4e0) U32(0x00000000),  /* element 4: text buffer */
/* +0x04e4  meaning not yet known */
U32(0x00000000), U32(0x0000007d), U32(0x0000001f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x520) STR12("Courier New"),
/* +0x0540  meaning not yet known */
AT(0x540) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x0560  element 5 of 16: type 2 (text, 0xbc bytes) */
AT(0x560) U32(0x00000002),  /* element 5: type */
/* +0x0564  meaning not yet known */
U32(0xffffffff), U32(0x0000020a), U8(0x58),
AT(0x584) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0590  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x74),
/* +0x05b8  meaning not yet known */
AT(0x5b8) U8(0x76),
AT(0x618) U32(0x00000000),  /* element 5: text buffer */
/* +0x061c  meaning not yet known */
U32(0x00000000), U32(0x0000004b), U32(0x0000001f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x658) STR12("Courier New"),
/* +0x0678  meaning not yet known */
AT(0x678) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0698  element 6 of 16: type 4 */
AT(0x698) U32(0x00000004),  /* element 6: type */
/* +0x069c  meaning not yet known */
U32(0x00001500), U32(0x0000001f), U8(0x16),
AT(0x6bc) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06c8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x67),
/* +0x0708  meaning not yet known */
AT(0x708) U8(0x01),
/* +0x0720  element 7 of 16: type 4 */
AT(0x720) U32(0x00000004),  /* element 7: type */
/* +0x0724  meaning not yet known */
U32(0x00002000), U32(0x0000005a), U8(0x16),
AT(0x744) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0750  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6a),
/* +0x0790  meaning not yet known */
AT(0x790) U8(0x01),
/* +0x07a8  element 8 of 16: type 4 */
AT(0x7a8) U32(0x00000004),  /* element 8: type */
/* +0x07ac  meaning not yet known */
U32(0x00002500), U32(0x00000095), U8(0x16),
AT(0x7cc) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x07d8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6c),
/* +0x0818  meaning not yet known */
AT(0x818) U8(0x01),
/* +0x0830  element 9 of 16: type 4 */
AT(0x830) U32(0x00000004),  /* element 9: type */
/* +0x0834  meaning not yet known */
U32(0x00003000), U32(0x000000d0), U8(0x16),
AT(0x854) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0860  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x73),
/* +0x08a0  meaning not yet known */
AT(0x8a0) U8(0x01),
/* +0x08b8  element 10 of 16: type 4 */
AT(0x8b8) U32(0x00000004),  /* element 10: type */
/* +0x08bc  meaning not yet known */
U32(0x00003500), U32(0x0000010b), U8(0x16),
AT(0x8dc) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x08e8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x75),
/* +0x0928  meaning not yet known */
AT(0x928) U8(0x01),
/* +0x0940  element 11 of 16: type 4 */
AT(0x940) U32(0x00000004),  /* element 11: type */
/* +0x0944  meaning not yet known */
U32(0x00004000), U32(0x00000146), U8(0x16),
AT(0x964) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0970  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x69),
/* +0x09b0  meaning not yet known */
AT(0x9b0) U8(0x01),
/* +0x09c8  element 12 of 16: type 4 */
AT(0x9c8) U32(0x00000004),  /* element 12: type */
/* +0x09cc  meaning not yet known */
U32(0x00004500), U32(0x00000181), U8(0x16),
AT(0x9ec) U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x09f8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x70),
/* +0x0a38  meaning not yet known */
AT(0xa38) U8(0x01),
/* +0x0a50  element 13 of 16: type 4 */
AT(0xa50) U32(0x00000004),  /* element 13: type */
/* +0x0a54  meaning not yet known */
U32(0x00005000), U32(0x000001bc), U8(0x16),
AT(0xa74) U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a80  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x71),
/* +0x0ac0  meaning not yet known */
AT(0xac0) U8(0x01),
/* +0x0ad8  element 14 of 16: type 4 */
AT(0xad8) U32(0x00000004),  /* element 14: type */
/* +0x0adc  meaning not yet known */
U32(0x00005500), U32(0x000001f7), U8(0x16),
AT(0xafc) U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b08  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x68),
/* +0x0b48  meaning not yet known */
AT(0xb48) U8(0x01),
/* +0x0b60  element 15 of 16: type 4 */
AT(0xb60) U32(0x00000004),  /* element 15: type */
/* +0x0b64  meaning not yet known */
U32(0x00006000), U32(0x00000232), U8(0x16),
AT(0xb84) U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b90  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x72),
/* +0x0bd0  meaning not yet known */
AT(0xbd0) U8(0x01),
/* +0x0be8  the children of element 16, zero-terminated */
AT(0xbe8) U32(0x00000000),  /* element 16: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 13 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 14 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: child 15 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 16: end of the list */
/* +0x0c28  element 16 of 16: type 0 */
U32(0x00000000),  /* element 16: type */
/* +0x0c2c  meaning not yet known */
U16(0x1000),
AT(0xc4c) U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: children list (relocated: installed at attach, rebind.inc) */
/* +0x0c54  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0c78  the element-list slot (the header's +0x08 points here): the first element */
AT(0xc78) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0c7c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0c94  meaning not yet known */
AT(0xc94) U32(0xffffffff),
/* +0x0cac  meaning not yet known */
AT(0xcac) U32(0x00000001), U32(0x00000017), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa121),
/* +0x0cd8  meaning not yet known */
AT(0xcd8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x0001002c), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0d18  meaning not yet known */
AT(0xd18) U32(0xffffffff),
/* +0x0d30  meaning not yet known */
AT(0xd30) U32(0x00000001), U32(0x00000052), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa404),
/* +0x0d5c  meaning not yet known */
AT(0xd5c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010106), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0d9c  meaning not yet known */
AT(0xd9c) U32(0xffffffff),
/* +0x0db4  meaning not yet known */
AT(0xdb4) U32(0x00000001), U32(0x0000008d), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa405),
/* +0x0de0  meaning not yet known */
AT(0xde0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010107), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0e20  meaning not yet known */
AT(0xe20) U32(0xffffffff),
/* +0x0e38  meaning not yet known */
AT(0xe38) U32(0x00000001), U32(0x000000c8), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa1df),
/* +0x0e64  meaning not yet known */
AT(0xe64) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e8c  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0ea4  meaning not yet known */
AT(0xea4) U32(0xffffffff),
/* +0x0ebc  meaning not yet known */
AT(0xebc) U32(0x00000001), U32(0x00000103), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa138),
/* +0x0ee8  meaning not yet known */
AT(0xee8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0f10  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0f28  meaning not yet known */
AT(0xf28) U32(0xffffffff),
/* +0x0f40  meaning not yet known */
AT(0xf40) U32(0x00000001), U32(0x0000013e), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa406),
/* +0x0f6c  meaning not yet known */
AT(0xf6c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100c1), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0fac  meaning not yet known */
AT(0xfac) U32(0xffffffff),
/* +0x0fc4  meaning not yet known */
AT(0xfc4) U32(0x00000001), U32(0x00000179), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa407),
/* +0x0ff0  meaning not yet known */
AT(0xff0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100bc), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1030  meaning not yet known */
AT(0x1030) U32(0xffffffff),
/* +0x1048  meaning not yet known */
AT(0x1048) U32(0x00000001), U32(0x000001b4), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa408),
/* +0x1074  meaning not yet known */
AT(0x1074) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100c0), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x10b4  meaning not yet known */
AT(0x10b4) U32(0xffffffff),
/* +0x10cc  meaning not yet known */
AT(0x10cc) U32(0x00000001), U32(0x000001ef), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa42c),
/* +0x10f8  meaning not yet known */
AT(0x10f8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100bf), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1138  meaning not yet known */
AT(0x1138) U32(0xffffffff),
/* +0x1150  meaning not yet known */
AT(0x1150) U32(0x00000001), U32(0x0000022a), U32(0x00000010), U32(0x0000002f), U32(0x00000021), U32(0xffffffff),
U16(0xa42d),
/* +0x117c  meaning not yet known */
AT(0x117c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000002f), U32(0x00000021), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010101), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x11bc  meaning not yet known */
AT(0x11bc) U32(0xffffffff),
/* +0x11d4  meaning not yet known */
AT(0x11d4) U32(0x00000001), U32(0x00000015), U32(0x00000040), U32(0x0000006a), U32(0x0000003c), U32(0xffffffff),
U16(0xa42e),
/* +0x1200  meaning not yet known */
AT(0x1200) U32(0x00000002), U32(0x00000005), U32(0x00000015), U32(0x00000025), U32(0x00000021), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001007e), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000002a), U32(0x00000015), U32(0x00000025), U32(0x00000021), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001007f), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x126c  meaning not yet known */
AT(0x126c) U32(0xffffffff),
/* +0x1284  meaning not yet known */
AT(0x1284) U32(0x00000001), U32(0x0000006e), U32(0x00000040), U32(0x0000006a), U32(0x0000003c), U32(0xffffffff),
U16(0xa42f),
/* +0x12b0  meaning not yet known */
AT(0x12b0) U32(0x00000002), U32(0x00000005), U32(0x00000015), U32(0x00000025), U32(0x00000021), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010158), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000002a), U32(0x00000015), U32(0x00000025), U32(0x00000021), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010157), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x131c  meaning not yet known */
AT(0x131c) U32(0xffffffff),
/* +0x1334  meaning not yet known */
AT(0x1334) U32(0x00000001), U32(0x000000e4), U32(0x00000040), U32(0x00000087), U32(0x0000003c), U32(0xffffffff),
U16(0xa430),
/* +0x1360  meaning not yet known */
AT(0x1360) U32(0x00000002), U32(0x00000005), U32(0x00000015), U32(0x0000003e), U32(0x00000021), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010165), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000043), U32(0x00000015), U32(0x0000003e), U32(0x00000021), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010164), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x13cc  meaning not yet known */
AT(0x13cc) U32(0xffffffff),
/* +0x13e4  meaning not yet known */
AT(0x13e4) U32(0x00000001), U32(0x0000016d), U32(0x00000040), U32(0x00000087), U32(0x0000003c), U32(0xffffffff),
U16(0xa441),
/* +0x1410  meaning not yet known */
AT(0x1410) U32(0x00000002), U32(0x00000005), U32(0x00000015), U32(0x0000003e), U32(0x00000021), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010167), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000043), U32(0x00000015), U32(0x0000003e), U32(0x00000021), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010166), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x147c  meaning not yet known */
AT(0x147c) U32(0xffffffff),
/* +0x1494  meaning not yet known */
AT(0x1494) U32(0x00000001), U32(0x000001fe), U32(0x00000040), U32(0x0000006a), U32(0x0000003c), U32(0xffffffff),
U16(0xa1e0),
/* +0x14c0  meaning not yet known */
AT(0x14c0) U32(0x00000002), U32(0x00000005), U32(0x00000015), U32(0x00000025), U32(0x00000021), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x14e8  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x0000002a), U32(0x00000015), U32(0x00000025), U32(0x00000021),
U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1514  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x152c  meaning not yet known */
AT(0x152c) U32(0xffffffff),
/* +0x1544  meaning not yet known */
AT(0x1544) U8(0x03),
/* +0x1558  meaning not yet known */
AT(0x1558) U32(0xffffffff),
/* +0x1584  meaning not yet known */
AT(0x1584) U32(0xffffffff),
AT(0x159c) STR8("%03d"),
STR8("%05d"),
STR8(".%02d"),
/* +0x15b4  meaning not yet known */
U32(0x00000000),
/* +0x15b8  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x15c0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x15c8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x15d4  meaning not yet known */
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
