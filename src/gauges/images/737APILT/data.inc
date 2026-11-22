/* 737APILT.GAU's .data image, decompiled: every non-zero value of retail's .data (0x170c bytes at RVA 0x3000) at
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
AT(0x0) STR24("737-400.Autopilot"),  /* the gauge name */
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
U16(0x0173),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  element 1 of 17: type 2 (text, 0xbc bytes) */
AT(0x80) U32(0x00000002),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0xffffffff), U32(0x00000035), U8(0x35),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00b0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x2d),
AT(0x138) U32(0x00000000),  /* element 1: text buffer */
/* +0x013c  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x178) STR12("Courier New"),
/* +0x0198  meaning not yet known */
AT(0x198) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x01b8  element 2 of 17: type 2 (text, 0xbc bytes) */
AT(0x1b8) U32(0x00000002),  /* element 2: type */
/* +0x01bc  meaning not yet known */
U32(0xffffffff), U32(0x00000065), U8(0x35),
AT(0x1dc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01e8  meaning not yet known */
U32(0x00000006), U32(0x00000002), U32(0x00000000), U8(0x6b),
AT(0x270) U32(0x00000000),  /* element 2: text buffer */
/* +0x0274  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x2b0) STR12("Courier New"),
/* +0x02d0  meaning not yet known */
AT(0x2d0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x02f0  element 3 of 17: type 2 (text, 0xbc bytes) */
AT(0x2f0) U32(0x00000002),  /* element 3: type */
/* +0x02f4  meaning not yet known */
U32(0xffffffff), U32(0x00000095), U8(0x35),
AT(0x314) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0320  meaning not yet known */
U32(0x00000006), U32(0x00000001), U32(0x00000000), U8(0x6d),
AT(0x3a8) U32(0x00000000),  /* element 3: text buffer */
/* +0x03ac  meaning not yet known */
U32(0x00000000), U32(0x0000004d), U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x3e8) STR12("Courier New"),
/* +0x0408  meaning not yet known */
AT(0x408) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x0428  element 4 of 17: type 2 (text, 0xbc bytes) */
AT(0x428) U32(0x00000002),  /* element 4: type */
/* +0x042c  meaning not yet known */
U32(0xffffffff), U32(0x000000e9), U8(0x35),
AT(0x44c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0458  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x74),
/* +0x0480  meaning not yet known */
AT(0x480) U8(0x76),
AT(0x4e0) U32(0x00000000),  /* element 4: text buffer */
/* +0x04e4  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x520) STR12("Courier New"),
/* +0x0540  meaning not yet known */
AT(0x540) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0560  element 5 of 17: type 2 (text, 0xbc bytes) */
AT(0x560) U32(0x00000002),  /* element 5: type */
/* +0x0564  meaning not yet known */
U32(0xffffffff), U32(0x00000119), U8(0x35),
AT(0x584) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0590  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x78),
AT(0x618) U32(0x00000000),  /* element 5: text buffer */
/* +0x061c  meaning not yet known */
U32(0x00000000), U32(0x0000004d), U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x658) STR12("Courier New"),
/* +0x0678  meaning not yet known */
AT(0x678) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x0698  element 6 of 17: type 4 */
AT(0x698) U32(0x00000004),  /* element 6: type */
/* +0x069c  meaning not yet known */
U32(0x00002000), U32(0x00000033), U8(0x14),
AT(0x6bc) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06c8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x67),
/* +0x0708  meaning not yet known */
AT(0x708) U8(0x01),
/* +0x0720  element 7 of 17: type 4 */
AT(0x720) U32(0x00000004),  /* element 7: type */
/* +0x0724  meaning not yet known */
U32(0x00002000), U32(0x00000052), U8(0x14),
AT(0x744) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0750  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6a),
/* +0x0790  meaning not yet known */
AT(0x790) U8(0x01),
/* +0x07a8  element 8 of 17: type 4 */
AT(0x7a8) U32(0x00000004),  /* element 8: type */
/* +0x07ac  meaning not yet known */
U32(0x00002000), U32(0x00000071), U8(0x14),
AT(0x7cc) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x07d8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6c),
/* +0x0818  meaning not yet known */
AT(0x818) U8(0x01),
/* +0x0830  element 9 of 17: type 4 */
AT(0x830) U32(0x00000004),  /* element 9: type */
/* +0x0834  meaning not yet known */
U32(0x00002000), U32(0x0000008f), U8(0x14),
AT(0x854) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0860  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x73),
/* +0x08a0  meaning not yet known */
AT(0x8a0) U8(0x01),
/* +0x08b8  element 10 of 17: type 4 */
AT(0x8b8) U32(0x00000004),  /* element 10: type */
/* +0x08bc  meaning not yet known */
U32(0x00002000), U32(0x000000ae), U8(0x14),
AT(0x8dc) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x08e8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x75),
/* +0x0928  meaning not yet known */
AT(0x928) U8(0x01),
/* +0x0940  element 11 of 17: type 4 */
AT(0x940) U32(0x00000004),  /* element 11: type */
/* +0x0944  meaning not yet known */
U32(0x00002000), U32(0x000000cc), U8(0x14),
AT(0x964) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0970  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x69),
/* +0x09b0  meaning not yet known */
AT(0x9b0) U8(0x01),
/* +0x09c8  element 12 of 17: type 4 */
AT(0x9c8) U32(0x00000004),  /* element 12: type */
/* +0x09cc  meaning not yet known */
U32(0x00002000), U32(0x000000eb), U8(0x14),
AT(0x9ec) U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x09f8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x70),
/* +0x0a38  meaning not yet known */
AT(0xa38) U8(0x01),
/* +0x0a50  element 13 of 17: type 4 */
AT(0xa50) U32(0x00000004),  /* element 13: type */
/* +0x0a54  meaning not yet known */
U32(0x00002000), U32(0x00000109), U8(0x14),
AT(0xa74) U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a80  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x71),
/* +0x0ac0  meaning not yet known */
AT(0xac0) U8(0x01),
/* +0x0ad8  element 14 of 17: type 4 */
AT(0xad8) U32(0x00000004),  /* element 14: type */
/* +0x0adc  meaning not yet known */
U32(0x00002000), U32(0x00000128), U8(0x14),
AT(0xafc) U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b08  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x68),
/* +0x0b48  meaning not yet known */
AT(0xb48) U8(0x01),
/* +0x0b60  element 15 of 17: type 4 */
AT(0xb60) U32(0x00000004),  /* element 15: type */
/* +0x0b64  meaning not yet known */
U32(0x00002000), U32(0x00000146), U8(0x14),
AT(0xb84) U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b90  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x72),
/* +0x0bd0  meaning not yet known */
AT(0xbd0) U8(0x01),
/* +0x0be8  element 16 of 17: type 4 */
AT(0xbe8) U32(0x00000004),  /* element 16: type */
/* +0x0bec  meaning not yet known */
U32(0x00003000), U32(0x00000010), U8(0x1e),
AT(0xc0c) U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c18  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x7b),
/* +0x0c58  meaning not yet known */
AT(0xc58) U8(0x01),
/* +0x0c70  the children of element 17, zero-terminated */
AT(0xc70) U32(0x00000000),  /* element 17: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 13 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 14 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 15 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: child 16 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 17: end of the list */
/* +0x0cb4  meaning not yet known */
U32(0x00000000),
/* +0x0cb8  element 17 of 17: type 0 */
U32(0x00000000),  /* element 17: type */
/* +0x0cbc  meaning not yet known */
U16(0x1000),
AT(0xcdc) U32(0x00000000),  /* element 17: parent, written at run time */
U32(0x00000000),  /* element 17: children list (relocated: installed at attach, rebind.inc) */
/* +0x0ce4  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0d08  the element-list slot (the header's +0x08 points here): the first element */
AT(0xd08) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0d0c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0d24  meaning not yet known */
AT(0xd24) U32(0xffffffff), U16(0xa402),
/* +0x0d3c  meaning not yet known */
AT(0xd3c) U32(0x00000001), U32(0x00000005), U32(0x0000000a), U32(0x00000028), U32(0x00000028), U32(0xffffffff),
U16(0xa443),
/* +0x0d68  meaning not yet known */
AT(0xd68) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000028), U32(0x00000028), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010144), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0da8  meaning not yet known */
AT(0xda8) U32(0xffffffff),
/* +0x0dc0  meaning not yet known */
AT(0xdc0) U32(0x00000001), U32(0x00000033), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa403),
/* +0x0dec  meaning not yet known */
AT(0xdec) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x0001002c), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0e2c  meaning not yet known */
AT(0xe2c) U32(0xffffffff),
/* +0x0e44  meaning not yet known */
AT(0xe44) U32(0x00000001), U32(0x00000052), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa404),
/* +0x0e70  meaning not yet known */
AT(0xe70) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010106), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0eb0  meaning not yet known */
AT(0xeb0) U32(0xffffffff),
/* +0x0ec8  meaning not yet known */
AT(0xec8) U32(0x00000001), U32(0x00000071), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa405),
/* +0x0ef4  meaning not yet known */
AT(0xef4) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010107), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0f34  meaning not yet known */
AT(0xf34) U32(0xffffffff),
/* +0x0f4c  meaning not yet known */
AT(0xf4c) U32(0x00000001), U32(0x0000008f), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa1df),
/* +0x0f78  meaning not yet known */
AT(0xf78) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0fa0  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0fb8  meaning not yet known */
AT(0xfb8) U32(0xffffffff),
/* +0x0fd0  meaning not yet known */
AT(0xfd0) U32(0x00000001), U32(0x000000ae), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa138),
/* +0x0ffc  meaning not yet known */
AT(0xffc) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1024  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x103c  meaning not yet known */
AT(0x103c) U32(0xffffffff),
/* +0x1054  meaning not yet known */
AT(0x1054) U32(0x00000001), U32(0x000000cc), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa406),
/* +0x1080  meaning not yet known */
AT(0x1080) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100c1), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x10c0  meaning not yet known */
AT(0x10c0) U32(0xffffffff),
/* +0x10d8  meaning not yet known */
AT(0x10d8) U32(0x00000001), U32(0x000000eb), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa407),
/* +0x1104  meaning not yet known */
AT(0x1104) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100bc), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1144  meaning not yet known */
AT(0x1144) U32(0xffffffff),
/* +0x115c  meaning not yet known */
AT(0x115c) U32(0x00000001), U32(0x00000109), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa408),
/* +0x1188  meaning not yet known */
AT(0x1188) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100c0), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x11c8  meaning not yet known */
AT(0x11c8) U32(0xffffffff),
/* +0x11e0  meaning not yet known */
AT(0x11e0) U32(0x00000001), U32(0x00000128), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa42c),
/* +0x120c  meaning not yet known */
AT(0x120c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100bf), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x124c  meaning not yet known */
AT(0x124c) U32(0xffffffff),
/* +0x1264  meaning not yet known */
AT(0x1264) U32(0x00000001), U32(0x00000146), U32(0x00000009), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa42d),
/* +0x1290  meaning not yet known */
AT(0x1290) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010101), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x12d0  meaning not yet known */
AT(0x12d0) U32(0xffffffff),
/* +0x12e8  meaning not yet known */
AT(0x12e8) U32(0x00000001), U32(0x00000035), U32(0x00000035), U32(0x00000028), U32(0x0000000f), U32(0xffffffff),
U16(0xa42e),
/* +0x1314  meaning not yet known */
AT(0x1314) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000014), U32(0x0000000f), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001007e), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000014), U32(0x00000000), U32(0x00000014), U32(0x0000000f), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001007f), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1380  meaning not yet known */
AT(0x1380) U32(0xffffffff),
/* +0x1398  meaning not yet known */
AT(0x1398) U32(0x00000001), U32(0x00000065), U32(0x00000035), U32(0x00000028), U32(0x0000000f), U32(0xffffffff),
U16(0xa42f),
/* +0x13c4  meaning not yet known */
AT(0x13c4) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000014), U32(0x0000000f), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010158), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000014), U32(0x00000000), U32(0x00000014), U32(0x0000000f), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010157), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1430  meaning not yet known */
AT(0x1430) U32(0xffffffff),
/* +0x1448  meaning not yet known */
AT(0x1448) U32(0x00000001), U32(0x00000095), U32(0x00000035), U32(0x0000004d), U32(0x0000000f), U32(0xffffffff),
U16(0xa430),
/* +0x1474  meaning not yet known */
AT(0x1474) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000026), U32(0x0000000f), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010165), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000026), U32(0x00000000), U32(0x00000026), U32(0x0000000f), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010164), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x14e0  meaning not yet known */
AT(0x14e0) U32(0xffffffff),
/* +0x14f8  meaning not yet known */
AT(0x14f8) U32(0x00000001), U32(0x000000e9), U32(0x00000035), U32(0x00000028), U32(0x0000000f), U32(0xffffffff),
U16(0xa1e0),
/* +0x1524  meaning not yet known */
AT(0x1524) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000014), U32(0x00000012), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x154c  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x00000014), U32(0x00000000), U32(0x00000014), U32(0x00000012),
U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x1578  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x1590  meaning not yet known */
AT(0x1590) U32(0xffffffff),
/* +0x15a8  meaning not yet known */
AT(0x15a8) U32(0x00000001), U32(0x00000119), U32(0x00000035), U32(0x0000004d), U32(0x0000000f), U32(0xffffffff),
U16(0xa441),
/* +0x15d4  meaning not yet known */
AT(0x15d4) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000026), U32(0x0000000f), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010167), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000026), U32(0x00000000), U32(0x00000026), U32(0x0000000f), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010166), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1640  meaning not yet known */
AT(0x1640) U32(0xffffffff),
/* +0x1658  meaning not yet known */
AT(0x1658) U8(0x03),
/* +0x166c  meaning not yet known */
AT(0x166c) U32(0xffffffff),
/* +0x1698  meaning not yet known */
AT(0x1698) U32(0xffffffff),
AT(0x16b0) STR8("%03d"),
STR8("%05d"),
STR8(".%02d"),
/* +0x16c8  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x16d0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x16d8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x16e4  meaning not yet known */
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
