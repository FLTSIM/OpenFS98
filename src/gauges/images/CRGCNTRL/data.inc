/* CRGCNTRL.GAU's .data image, decompiled: every non-zero value of retail's .data (0xe68 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Cessna_182_RG.Controls"),  /* the gauge name */
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
/* +0x0080  meaning not yet known */
AT(0x80) U32(0x00000001), U8(0x03),
/* +0x0098  meaning not yet known */
AT(0x98) U16(0x014b),
/* +0x00b8  element 1 of 13: type 3 */
AT(0xb8) U32(0x00000003),  /* element 1: type */
/* +0x00bc  meaning not yet known */
U32(0x00002000), U32(0x00000017), U8(0x32),
AT(0xdc) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00e4  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0120  meaning not yet known */
AT(0x120) U16(0x013e),
/* +0x0145  meaning not yet known */
AT(0x145) U8(0xc0), U16(0xbf62),
/* +0x0160  element 2 of 13: type 3 */
AT(0x160) U32(0x00000003),  /* element 2: type */
/* +0x0164  meaning not yet known */
U32(0x00002100), U32(0x00000059), U8(0x0d),
AT(0x184) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x018c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x0138),
/* +0x01c1  meaning not yet known */
AT(0x1c1) U8(0x40), U16(0x3f62),
/* +0x0208  element 3 of 13: type 3 */
AT(0x208) U32(0x00000003),  /* element 3: type */
/* +0x020c  meaning not yet known */
U32(0x00002200), U32(0x0000005e), U8(0x34),
AT(0x22c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x0234  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0270  meaning not yet known */
AT(0x270) U16(0x0136),
/* +0x0296  meaning not yet known */
AT(0x296) U16(0x3f5e),
/* +0x02b0  element 4 of 13: type 3 */
AT(0x2b0) U32(0x00000003),  /* element 4: type */
/* +0x02b4  meaning not yet known */
U32(0x00002300), U32(0x00000058), U8(0x5d),
AT(0x2d4) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x02dc  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x013b),
/* +0x0312  meaning not yet known */
AT(0x312) U16(0x3f62),
/* +0x0358  element 5 of 13: type 4 */
AT(0x358) U32(0x00000004),  /* element 5: type */
/* +0x035c  meaning not yet known */
U32(0x00002400), U32(0x000000a0), U8(0x11),
AT(0x37c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
/* +0x0384  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x8d),
/* +0x03b4  meaning not yet known */
AT(0x3b4) U8(0x03),
/* +0x03c8  meaning not yet known */
AT(0x3c8) U8(0x02),
/* +0x03e0  element 6 of 13: type 3 */
AT(0x3e0) U32(0x00000003),  /* element 6: type */
/* +0x03e4  meaning not yet known */
U32(0x00003000), U32(0x00000118), U8(0x3e),
AT(0x404) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0410  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x80),
/* +0x0441  meaning not yet known */
AT(0x441) U8(0x80), U16(0xbf60), U32(0x00000000), U8(0x80),
/* +0x046d  meaning not yet known */
AT(0x46d) U8(0x80), U16(0xbf63),
/* +0x0488  element 7 of 13: type 3 */
AT(0x488) U32(0x00000003),  /* element 7: type */
/* +0x048c  meaning not yet known */
U32(0x00004000), U32(0x00000174), U8(0x3e),
AT(0x4ac) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
/* +0x04b4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x81),
/* +0x04e9  meaning not yet known */
AT(0x4e9) U8(0x80), U16(0xbf60), U32(0x00000000), U8(0x81),
/* +0x0515  meaning not yet known */
AT(0x515) U8(0x80), U16(0xbf63),
/* +0x0530  element 8 of 13: type 3 */
AT(0x530) U32(0x00000003),  /* element 8: type */
/* +0x0534  meaning not yet known */
U32(0x00005000), U32(0x000001cc), U8(0x3e),
AT(0x554) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
/* +0x055c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x82),
/* +0x0591  meaning not yet known */
AT(0x591) U8(0x80), U16(0xbf60), U32(0x00000000), U8(0x82),
/* +0x05bd  meaning not yet known */
AT(0x5bd) U8(0x80), U16(0xbf63),
/* +0x05d8  element 9 of 13: type 3 */
AT(0x5d8) U32(0x00000003),  /* element 9: type */
/* +0x05dc  meaning not yet known */
U32(0x00005100), U32(0x00000214), U8(0x13),
AT(0x5fc) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
/* +0x0604  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0640  meaning not yet known */
AT(0x640) U16(0x0148),
/* +0x0665  meaning not yet known */
AT(0x665) U8(0x80), U16(0x3f71),
/* +0x0680  element 10 of 13: type 4 */
AT(0x680) U32(0x00000004),  /* element 10: type */
/* +0x0684  meaning not yet known */
U32(0x00006000), U32(0x0000023c), U8(0x3a),
AT(0x6a4) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06b0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x014d),
/* +0x06dc  meaning not yet known */
AT(0x6dc) U8(0x03),
/* +0x06f0  meaning not yet known */
AT(0x6f0) U8(0x02),
/* +0x0708  element 11 of 13: type 4 */
AT(0x708) U32(0x00000004),  /* element 11: type */
/* +0x070c  meaning not yet known */
U32(0x00006000), U32(0x00000264), U8(0x3a),
AT(0x72c) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0738  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x014e),
/* +0x0764  meaning not yet known */
AT(0x764) U8(0x03),
/* +0x0778  meaning not yet known */
AT(0x778) U8(0x02),
/* +0x0790  element 12 of 13: type 4 */
AT(0x790) U32(0x00000004),  /* element 12: type */
/* +0x0794  meaning not yet known */
U32(0x00006000), U32(0x00000250), U8(0x13),
AT(0x7b4) U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x07c0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x014c),
/* +0x07ec  meaning not yet known */
AT(0x7ec) U8(0x03),
/* +0x0800  meaning not yet known */
AT(0x800) U8(0x02),
/* +0x0818  the children of element 13, zero-terminated */
AT(0x818) U32(0x00000000),  /* element 13: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: end of the list */
/* +0x084c  meaning not yet known */
U32(0x00000000),
/* +0x0850  element 13 of 13: type 0 */
U32(0x00000000),  /* element 13: type */
/* +0x0854  meaning not yet known */
U16(0x1000),
AT(0x874) U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: children list (relocated: installed at attach, rebind.inc) */
/* +0x087c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x08a0  the element-list slot (the header's +0x08 points here): the first element */
AT(0x8a0) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x08a4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x08bc  meaning not yet known */
AT(0x8bc) U32(0xffffffff),
/* +0x08d4  meaning not yet known */
AT(0x8d4) U32(0x00000001), U32(0x00000003), U32(0x00000003), U32(0x0000002c), U32(0x0000005e), U32(0xffffffff),
U16(0xa02e),
/* +0x0900  meaning not yet known */
AT(0x900) U8(0x03),
/* +0x0914  meaning not yet known */
AT(0x914) U32(0xffffffff),
/* +0x092c  meaning not yet known */
AT(0x92c) U32(0x00000001), U32(0x00000034), U32(0x00000005), U32(0x00000052), U32(0x0000000f), U32(0xffffffff),
U16(0xa12c),
/* +0x0958  meaning not yet known */
AT(0x958) U8(0x03),
/* +0x096c  meaning not yet known */
AT(0x96c) U32(0xffffffff),
/* +0x0984  meaning not yet known */
AT(0x984) U32(0x00000001), U32(0x0000004f), U32(0x00000014), U32(0x00000019), U32(0x0000003e), U32(0xffffffff),
U16(0xa12d),
/* +0x09b0  meaning not yet known */
AT(0x9b0) U8(0x03),
/* +0x09c4  meaning not yet known */
AT(0x9c4) U32(0xffffffff),
/* +0x09dc  meaning not yet known */
AT(0x9dc) U32(0x00000001), U32(0x00000034), U32(0x00000052), U32(0x00000052), U32(0x0000000f), U32(0xffffffff),
U16(0xa12e),
/* +0x0a08  meaning not yet known */
AT(0xa08) U8(0x03),
/* +0x0a1c  meaning not yet known */
AT(0xa1c) U32(0xffffffff),
/* +0x0a34  meaning not yet known */
AT(0xa34) U32(0x00000001), U32(0x0000008b), U32(0x0000000d), U32(0x0000003c), U32(0x00000049), U32(0xffffffff),
U16(0xa00f),
/* +0x0a60  meaning not yet known */
AT(0xa60) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000003c), U32(0x00000049), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010023), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0aa0  meaning not yet known */
AT(0xaa0) U32(0xffffffff),
/* +0x0ab8  meaning not yet known */
AT(0xab8) U32(0x00000001), U32(0x000000cd), U32(0x00000008), U32(0x00000066), U32(0x0000005d), U32(0xffffffff),
U16(0xa019),
/* +0x0ae4  meaning not yet known */
AT(0xae4) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000066), U32(0x0000005d), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b0c  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0b24  meaning not yet known */
AT(0xb24) U32(0xffffffff),
/* +0x0b3c  meaning not yet known */
AT(0xb3c) U32(0x00000001), U32(0x00000134), U32(0x00000008), U32(0x0000005a), U32(0x0000005d), U32(0xffffffff),
U16(0xa024),
/* +0x0b68  meaning not yet known */
AT(0xb68) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000005a), U32(0x0000005d), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b90  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0ba8  meaning not yet known */
AT(0xba8) U32(0xffffffff),
/* +0x0bc0  meaning not yet known */
AT(0xbc0) U32(0x00000001), U32(0x0000018e), U32(0x00000008), U32(0x0000005a), U32(0x0000005d), U32(0xffffffff),
U16(0xa025),
/* +0x0bec  meaning not yet known */
AT(0xbec) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000005a), U32(0x0000005d), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c14  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0c2c  meaning not yet known */
AT(0xc2c) U32(0xffffffff),
/* +0x0c44  meaning not yet known */
AT(0xc44) U32(0x00000001), U32(0x000001f8), U32(0x00000000), U32(0x00000039), U32(0x00000065), U32(0xffffffff),
U16(0xa015),
/* +0x0c70  meaning not yet known */
AT(0xc70) U32(0x00000002), U32(0x00000004), U32(0x0000000a), U32(0x00000032), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x0001003b), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000004), U32(0x00000023), U32(0x00000032), U32(0x00000014), U32(0x00000005), U32(0x00000000),
U32(0x20000000), U32(0x0001003d), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000004),
U32(0x0000003c), U32(0x00000032), U32(0x00000014), U32(0x00000005), U32(0x00000000), U32(0x20000000),
U32(0x0001003f), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000004), U32(0x00000050),
U32(0x00000032), U32(0x00000014), U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x00010043),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0d34  meaning not yet known */
AT(0xd34) U32(0xffffffff),
/* +0x0d4c  meaning not yet known */
AT(0xd4c) U32(0x00000001), U32(0x0000023a), U32(0x0000000c), U32(0x00000053), U32(0x00000059), U32(0xffffffff),
U16(0xa014),
/* +0x0d78  meaning not yet known */
AT(0xd78) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000053), U32(0x00000059), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010022), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0db8  meaning not yet known */
AT(0xdb8) U32(0xffffffff),
/* +0x0dd0  meaning not yet known */
AT(0xdd0) U8(0x03),
/* +0x0de4  meaning not yet known */
AT(0xde4) U32(0xffffffff),
/* +0x0e10  meaning not yet known */
AT(0xe10) U32(0xffffffff),
/* +0x0e28  ImportTable: one import, PANELS (module id 0xf) */
AT(0xe28) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0e30  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0e38  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0e44  meaning not yet known */
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
