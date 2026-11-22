/* CESSCTRL.GAU's .data image, decompiled: every non-zero value of retail's .data (0xaf8 bytes at RVA 0x3000) at
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
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Cessna_182.Controls"),  /* the gauge name */
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
U16(0x0135),
/* +0x0068  meaning not yet known */
AT(0x68) U8(0x02),
/* +0x0080  element 1 of 9: type 3 */
AT(0x80) U32(0x00000003),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0x00001100), U32(0x00000017), U8(0x32),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00ac  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x00e8  meaning not yet known */
AT(0xe8) U16(0x013e),
/* +0x010d  meaning not yet known */
AT(0x10d) U8(0xc0), U16(0xbf62),
/* +0x0128  element 2 of 9: type 3 */
AT(0x128) U32(0x00000003),  /* element 2: type */
/* +0x012c  meaning not yet known */
U32(0x00001200), U32(0x00000059), U8(0x0a),
AT(0x14c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x0154  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x0138),
/* +0x0189  meaning not yet known */
AT(0x189) U8(0x40), U16(0x3f62),
/* +0x01d0  element 3 of 9: type 3 */
AT(0x1d0) U32(0x00000003),  /* element 3: type */
/* +0x01d4  meaning not yet known */
U32(0x00001300), U32(0x0000005e), U8(0x31),
AT(0x1f4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x01fc  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0238  meaning not yet known */
AT(0x238) U16(0x0136),
/* +0x025e  meaning not yet known */
AT(0x25e) U16(0xbf5e),
/* +0x0278  element 4 of 9: type 3 */
AT(0x278) U32(0x00000003),  /* element 4: type */
/* +0x027c  meaning not yet known */
U32(0x00001400), U32(0x00000058), U8(0x5a),
AT(0x29c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x02a4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x013b),
/* +0x02da  meaning not yet known */
AT(0x2da) U16(0x3f62),
/* +0x0320  element 5 of 9: type 3 */
AT(0x320) U32(0x00000003),  /* element 5: type */
/* +0x0324  meaning not yet known */
U32(0x00001600), U32(0x00000118), U8(0x3e),
AT(0x344) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0350  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x80),
/* +0x0381  meaning not yet known */
AT(0x381) U8(0x80), U16(0xbf60), U32(0x00000000), U8(0x80),
/* +0x03ad  meaning not yet known */
AT(0x3ad) U8(0x80), U16(0xbf63),
/* +0x03c8  element 6 of 9: type 3 */
AT(0x3c8) U32(0x00000003),  /* element 6: type */
/* +0x03cc  meaning not yet known */
U32(0x00001700), U32(0x00000174), U8(0x3e),
AT(0x3ec) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
/* +0x03f4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x81),
/* +0x0429  meaning not yet known */
AT(0x429) U8(0x80), U16(0xbf60), U32(0x00000000), U8(0x81),
/* +0x0455  meaning not yet known */
AT(0x455) U8(0x80), U16(0xbf63),
/* +0x0470  element 7 of 9: type 3 */
AT(0x470) U32(0x00000003),  /* element 7: type */
/* +0x0474  meaning not yet known */
U32(0x00001800), U32(0x000001cc), U8(0x3e),
AT(0x494) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
/* +0x049c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x82),
/* +0x04d1  meaning not yet known */
AT(0x4d1) U8(0x80), U16(0xbf60), U32(0x00000000), U8(0x82),
/* +0x04fd  meaning not yet known */
AT(0x4fd) U8(0x80), U16(0xbf63),
/* +0x0518  element 8 of 9: type 3 */
AT(0x518) U32(0x00000003),  /* element 8: type */
/* +0x051c  meaning not yet known */
U32(0x00001500), U32(0x00000214), U8(0x13),
AT(0x53c) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
/* +0x0544  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0580  meaning not yet known */
AT(0x580) U16(0x014a),
/* +0x05a5  meaning not yet known */
AT(0x5a5) U8(0x80), U16(0x3f71),
/* +0x05c0  the children of element 9, zero-terminated */
AT(0x5c0) U32(0x00000000),  /* element 9: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: end of the list */
/* +0x05e4  meaning not yet known */
U32(0x00000000),
/* +0x05e8  element 9 of 9: type 0 */
U32(0x00000000),  /* element 9: type */
/* +0x05ec  meaning not yet known */
U16(0x1000),
AT(0x60c) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0618  meaning not yet known */
U8(0x01),
/* +0x0638  the element-list slot (the header's +0x08 points here): the first element */
AT(0x638) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x063c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0654  meaning not yet known */
AT(0x654) U32(0xffffffff),
/* +0x066c  meaning not yet known */
AT(0x66c) U32(0x00000001), U32(0x00000003), U32(0x00000003), U32(0x0000002c), U32(0x0000005e), U32(0xffffffff),
U16(0xa02e),
/* +0x0698  meaning not yet known */
AT(0x698) U8(0x03),
/* +0x06ac  meaning not yet known */
AT(0x6ac) U32(0xffffffff),
/* +0x06c4  meaning not yet known */
AT(0x6c4) U32(0x00000001), U32(0x00000034), U32(0x00000005), U32(0x00000052), U32(0x0000000f), U32(0xffffffff),
U16(0xa12c),
/* +0x06f0  meaning not yet known */
AT(0x6f0) U8(0x03),
/* +0x0704  meaning not yet known */
AT(0x704) U32(0xffffffff),
/* +0x071c  meaning not yet known */
AT(0x71c) U32(0x00000001), U32(0x0000004f), U32(0x00000014), U32(0x00000019), U32(0x0000003e), U32(0xffffffff),
U16(0xa12d),
/* +0x0748  meaning not yet known */
AT(0x748) U8(0x03),
/* +0x075c  meaning not yet known */
AT(0x75c) U32(0xffffffff),
/* +0x0774  meaning not yet known */
AT(0x774) U32(0x00000001), U32(0x00000034), U32(0x00000052), U32(0x00000052), U32(0x0000000f), U32(0xffffffff),
U16(0xa12e),
/* +0x07a0  meaning not yet known */
AT(0x7a0) U8(0x03),
/* +0x07b4  meaning not yet known */
AT(0x7b4) U32(0xffffffff),
/* +0x07cc  meaning not yet known */
AT(0x7cc) U32(0x00000001), U32(0x000000cd), U32(0x00000008), U32(0x00000066), U32(0x0000005d), U32(0xffffffff),
U16(0xa019),
/* +0x07f8  meaning not yet known */
AT(0x7f8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000066), U32(0x0000005d), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0820  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0838  meaning not yet known */
AT(0x838) U32(0xffffffff),
/* +0x0850  meaning not yet known */
AT(0x850) U32(0x00000001), U32(0x00000134), U32(0x00000008), U32(0x0000005a), U32(0x0000005d), U32(0xffffffff),
U16(0xa024),
/* +0x087c  meaning not yet known */
AT(0x87c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000005a), U32(0x0000005d), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x08a4  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x08bc  meaning not yet known */
AT(0x8bc) U32(0xffffffff),
/* +0x08d4  meaning not yet known */
AT(0x8d4) U32(0x00000001), U32(0x0000018e), U32(0x00000008), U32(0x0000005a), U32(0x0000005d), U32(0xffffffff),
U16(0xa025),
/* +0x0900  meaning not yet known */
AT(0x900) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000005a), U32(0x0000005d), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0928  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0940  meaning not yet known */
AT(0x940) U32(0xffffffff),
/* +0x0958  meaning not yet known */
AT(0x958) U32(0x00000001), U32(0x000001f8), U32(0x00000000), U32(0x00000039), U32(0x00000065), U32(0xffffffff),
U16(0xa015),
/* +0x0984  meaning not yet known */
AT(0x984) U32(0x00000002), U32(0x00000004), U32(0x0000000a), U32(0x00000032), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x0001003b), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000004), U32(0x00000023), U32(0x00000032), U32(0x00000014), U32(0x00000005), U32(0x00000000),
U32(0x20000000), U32(0x0001003d), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000004),
U32(0x0000003c), U32(0x00000032), U32(0x00000014), U32(0x00000005), U32(0x00000000), U32(0x20000000),
U32(0x0001003f), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000004), U32(0x00000050),
U32(0x00000032), U32(0x00000014), U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x00010043),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0a48  meaning not yet known */
AT(0xa48) U32(0xffffffff),
/* +0x0a60  meaning not yet known */
AT(0xa60) U8(0x03),
/* +0x0a74  meaning not yet known */
AT(0xa74) U32(0xffffffff),
/* +0x0aa0  meaning not yet known */
AT(0xaa0) U32(0xffffffff),
/* +0x0ab8  ImportTable: one import, PANELS (module id 0xf) */
AT(0xab8) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0ac0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0ac8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0ad4  meaning not yet known */
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
#undef STR24
