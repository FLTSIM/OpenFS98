/* 737ADI.GAU's .data image, decompiled: every non-zero value of retail's .data (0x1078 bytes at RVA 0x3000) at
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
AT(0x0) STR16("737-400.ADI"),  /* the gauge name */
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
AT(0x60) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000007), U32(0x00000001), U32(0x00000000),
U32(0x00000003), U8(0x01),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000006), U32(0x00000001), U32(0x00000000),
U32(0x00000003), U8(0x01),
/* +0x00c0  meaning not yet known */
AT(0xc0) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000008), U8(0x01),
/* +0x00e8  meaning not yet known */
AT(0xe8) U32(0x00000001), U8(0x03),
/* +0x0100  element 1 of 14: type 2 (text, 0xbc bytes) */
AT(0x100) U32(0x00000002),  /* element 1: type */
/* +0x0104  meaning not yet known */
U32(0xffffffff), U32(0x00000022), U8(0x64),
AT(0x124) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0130  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x3d),
AT(0x1b8) U32(0x00000000),  /* element 1: text buffer */
/* +0x01bc  meaning not yet known */
U32(0x00000000), U32(0x0000002e), U32(0x00000012), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x1f8) STR12("Courier New"),
/* +0x0218  meaning not yet known */
AT(0x218) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0238  the children of element 2, zero-terminated */
AT(0x238) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0240  element 2 of 14: type 5 */
U32(0x00000005),  /* element 2: type */
/* +0x0244  meaning not yet known */
U32(0x00004000), U32(0x00000021), U8(0x1c),
AT(0x264) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0270  meaning not yet known */
U8(0x07),
/* +0x02b0  meaning not yet known */
AT(0x2b0) U8(0x3d),
/* +0x02dd  meaning not yet known */
AT(0x2dd) U8(0x40), U16(0x408f),
/* +0x0338  element 3 of 14: type 2 (text, 0xbc bytes) */
AT(0x338) U32(0x00000002),  /* element 3: type */
/* +0x033c  meaning not yet known */
U32(0xffffffff), U32(0x000000c1), U8(0x63),
AT(0x35c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0368  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4a),
AT(0x3f0) U32(0x00000000),  /* element 3: text buffer */
/* +0x03f4  meaning not yet known */
U32(0x00000000), U32(0x00000012), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x430) STR12("Courier New"),
/* +0x0450  meaning not yet known */
AT(0x450) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x02),
/* +0x0470  element 4 of 14: type 2 (text, 0xbc bytes) */
AT(0x470) U32(0x00000002),  /* element 4: type */
/* +0x0474  meaning not yet known */
U32(0xffffffff), U32(0x000000d2), U8(0x64),
AT(0x494) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04a0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4a),
AT(0x528) U32(0x00000000),  /* element 4: text buffer */
/* +0x052c  meaning not yet known */
U32(0x00000000), U32(0x00000021), U32(0x00000010), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x568) STR12("Courier New"),
/* +0x0588  meaning not yet known */
AT(0x588) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x05a8  the children of element 5, zero-terminated */
AT(0x5a8) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x05b0  element 5 of 14: type 5 */
U32(0x00000005),  /* element 5: type */
/* +0x05b4  meaning not yet known */
U32(0x00003000), U32(0x000000d3), U8(0x1c),
AT(0x5d4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05e0  meaning not yet known */
U8(0x07),
/* +0x0620  meaning not yet known */
AT(0x620) U8(0x4a),
/* +0x064d  meaning not yet known */
AT(0x64d) U8(0x40), U16(0x408f),
/* +0x06a8  the children of element 6, zero-terminated */
AT(0x6a8) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x06b4  meaning not yet known */
U32(0x00000000),
/* +0x06b8  element 6 of 14: type 1 */
U32(0x00000001),  /* element 6: type */
/* +0x06bc  meaning not yet known */
U32(0x00006000), U32(0x00000087), U32(0x00000070), U32(0x00000000), U32(0x00000000), U32(0x00000007),
U32(0x00000040),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06e8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x4f),
/* +0x0798  the children of element 7, zero-terminated */
AT(0x798) U32(0x00000000),  /* element 7: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 7: end of the list */
/* +0x07a0  element 7 of 14: type 6 */
U32(0x00000006),  /* element 7: type */
/* +0x07a4  meaning not yet known */
U32(0x00005000), U32(0x00000043), U32(0x00000028), U32(0x00000000), U32(0x00000000), U32(0x00000087),
U32(0x00000071),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x07d0  meaning not yet known */
U8(0x05),
/* +0x0802  meaning not yet known */
AT(0x802) U16(0x3ff0), U8(0x4d),
/* +0x0824  meaning not yet known */
AT(0x824) U32(0xdf3b645a), U32(0x3ff38d4f), U8(0x4f),
/* +0x0852  meaning not yet known */
AT(0x852) U16(0xbff0), U32(0x3ecccccd), U32(0x3ecccccd), U32(0x0000000b), U8(0x2e),
/* +0x08e0  element 8 of 14: type 2 (text, 0xbc bytes) */
AT(0x8e0) U32(0x00000002),  /* element 8: type */
/* +0x08e4  meaning not yet known */
U32(0xffffffff), U32(0x0000005f), U8(0x0e),
AT(0x904) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0910  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x40),
AT(0x998) U32(0x00000000),  /* element 8: text buffer */
/* +0x099c  meaning not yet known */
U32(0x00000000), U32(0x00000021), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x9d8) STR12("Courier New"),
/* +0x09f8  meaning not yet known */
AT(0x9f8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0a18  element 9 of 14: type 2 (text, 0xbc bytes) */
AT(0xa18) U32(0x00000002),  /* element 9: type */
/* +0x0a1c  meaning not yet known */
U32(0xffffffff), U32(0x0000008c), U8(0x0e),
AT(0xa3c) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a48  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4b),
AT(0xad0) U32(0x00000000),  /* element 9: text buffer */
/* +0x0ad4  meaning not yet known */
U32(0x00000000), U32(0x0000002c), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xb10) STR12("Courier New"),
/* +0x0b30  meaning not yet known */
AT(0xb30) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x04),
/* +0x0b50  element 10 of 14: type 2 (text, 0xbc bytes) */
AT(0xb50) U32(0x00000002),  /* element 10: type */
/* +0x0b54  meaning not yet known */
U32(0xffffffff), U32(0x00000078), U8(0xb6),
AT(0xb74) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b80  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4c),
AT(0xc08) U32(0x00000000),  /* element 10: text buffer */
/* +0x0c0c  meaning not yet known */
U32(0x00000000), U32(0x0000001b), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xc48) STR12("Courier New"),
/* +0x0c68  meaning not yet known */
AT(0xc68) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x02),
/* +0x0c88  element 11 of 14: type 2 (text, 0xbc bytes) */
AT(0xc88) U32(0x00000002),  /* element 11: type */
/* +0x0c8c  meaning not yet known */
U32(0xffffffff), U32(0x00000092), U8(0xb6),
AT(0xcac) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0cb8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4c),
AT(0xd40) U32(0x00000000),  /* element 11: text buffer */
/* +0x0d44  meaning not yet known */
U32(0x00000000), U32(0x00000028), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0xd80) STR12("Courier New"),
/* +0x0da0  meaning not yet known */
AT(0xda0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0dc0  element 12 of 14: type 3 */
AT(0xdc0) U32(0x00000003),  /* element 12: type */
/* +0x0dc4  meaning not yet known */
U32(0x00002000), U32(0x00000085), U8(0xd8),
AT(0xde4) U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
/* +0x0dec  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x45),
/* +0x0e22  meaning not yet known */
AT(0xe22) U16(0x3fc8), U32(0x00000000), U8(0x45),
/* +0x0e4e  meaning not yet known */
AT(0xe4e) U16(0x3fa4),
/* +0x0e68  the children of element 13, zero-terminated */
AT(0xe68) U32(0x00000000),  /* element 13: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: end of the list */
/* +0x0e88  element 13 of 14: type 0 */
U32(0x00000000),  /* element 13: type */
/* +0x0e8c  meaning not yet known */
U32(0x00001800), U32(0x0000001e), U8(0x11),
AT(0xeac) U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0eb8  meaning not yet known */
U8(0x14),
/* +0x0ed8  the children of element 14, zero-terminated */
AT(0xed8) U32(0x00000000),  /* element 14: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 14: end of the list */
/* +0x0ee0  element 14 of 14: type 0 */
U32(0x00000000),  /* element 14: type */
/* +0x0ee4  meaning not yet known */
U16(0x1000),
AT(0xf04) U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: children list (relocated: installed at attach, rebind.inc) */
/* +0x0f0c  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0f30  the element-list slot (the header's +0x08 points here): the first element */
AT(0xf30) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0f34  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0f4c  meaning not yet known */
AT(0xf4c) U32(0xffffffff), U16(0xa40a),
/* +0x0f64  meaning not yet known */
AT(0xf64) U32(0x00000001), U32(0x0000003b), U32(0x000000c1), U32(0x0000009a), U32(0x0000003c), U32(0xffffffff),
U16(0xa40b),
/* +0x0f90  meaning not yet known */
AT(0xf90) U8(0x03),
/* +0x0fa4  meaning not yet known */
AT(0xfa4) U32(0xffffffff),
/* +0x0fbc  meaning not yet known */
AT(0xfbc) U8(0x03),
/* +0x0fd0  meaning not yet known */
AT(0xfd0) U32(0xffffffff),
/* +0x0ffc  meaning not yet known */
AT(0xffc) U32(0xffffffff),
AT(0x1014) STR8("%03d"),
STR8("%02d"),
STR8(".%02d"),
STR8("%04d"),
/* +0x1034  meaning not yet known */
U32(0x00000000),
/* +0x1038  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x1040  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x1048  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x1054  meaning not yet known */
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
