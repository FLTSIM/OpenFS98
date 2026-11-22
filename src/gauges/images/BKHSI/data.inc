/* BKHSI.GAU's .data image, decompiled: every non-zero value of retail's .data (0xc88 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Bendix_King.HSI"),  /* the gauge name */
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
U8(0x53),
/* +0x0068  meaning not yet known */
AT(0x68) U8(0x2b),
/* +0x0088  meaning not yet known */
AT(0x88) U32(0x0000000b), U32(0x00000001), U32(0x00000000), U32(0x00000004), U32(0x00000001), U32(0x00000000),
U32(0x00000002), U8(0x01),
/* +0x00b8  meaning not yet known */
AT(0xb8) U32(0x0000000e), U8(0x01),
/* +0x00d0  element 1 of 15: type 0 */
AT(0xd0) U32(0x00000000),  /* element 1: type */
/* +0x00d4  meaning not yet known */
U32(0x00006100), U32(0x0000001a), U8(0x54),
AT(0xf4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00fc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0120  element 2 of 15: type 0 */
AT(0x120) U32(0x00000000),  /* element 2: type */
/* +0x0124  meaning not yet known */
U32(0x00006200), U32(0x0000001b), U8(0xbf),
AT(0x144) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x014c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0170  the children of element 3, zero-terminated */
AT(0x170) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x017c  meaning not yet known */
U32(0x00000000),
/* +0x0180  element 3 of 15: type 3 */
U32(0x00000003),  /* element 3: type */
/* +0x0184  meaning not yet known */
U32(0x00006000), U32(0x0000001c), U8(0x91),
AT(0x1a4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01b0  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x2c),
/* +0x01e8  meaning not yet known */
AT(0x1e8) U8(0x29),
/* +0x020e  meaning not yet known */
AT(0x20e) U16(0x3fde),
/* +0x0228  element 4 of 15: type 0 */
AT(0x228) U32(0x00000000),  /* element 4: type */
/* +0x022c  meaning not yet known */
U32(0x00006600), U32(0x000000ff), U8(0x3b),
AT(0x24c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x0254  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0278  element 5 of 15: type 0 */
AT(0x278) U32(0x00000000),  /* element 5: type */
/* +0x027c  meaning not yet known */
U32(0x00006700), U32(0x00000103), U8(0xba),
AT(0x29c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
/* +0x02a4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x02c8  the children of element 6, zero-terminated */
AT(0x2c8) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x02d4  meaning not yet known */
U32(0x00000000),
/* +0x02d8  element 6 of 15: type 3 */
U32(0x00000003),  /* element 6: type */
/* +0x02dc  meaning not yet known */
U32(0x00006500), U32(0x000000ff), U8(0x91),
AT(0x2fc) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0308  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x2c),
/* +0x0340  meaning not yet known */
AT(0x340) U8(0x29),
/* +0x0366  meaning not yet known */
AT(0x366) U16(0x3fde),
/* +0x0380  element 7 of 15: type 0 */
AT(0x380) U32(0x00000000),  /* element 7: type */
/* +0x0384  meaning not yet known */
U32(0x00004500), U32(0x00000090), U8(0x1f),
AT(0x3a4) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
/* +0x03ac  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03d0  element 8 of 15: type 0 */
AT(0x3d0) U32(0x00000000),  /* element 8: type */
/* +0x03d4  meaning not yet known */
U32(0x00004000), U32(0x00000080), U8(0x77),
AT(0x3f4) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
/* +0x03fc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0420  element 9 of 15: type 4 */
AT(0x420) U32(0x00000004),  /* element 9: type */
/* +0x0424  meaning not yet known */
U32(0x00005000), U32(0x0000003c), U8(0x1e),
AT(0x444) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
/* +0x044c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x2e),
/* +0x0490  meaning not yet known */
AT(0x490) U8(0x01),
/* +0x04a8  the children of element 10, zero-terminated */
AT(0x4a8) U32(0x00000000),  /* element 10: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 10: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 10: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 10: end of the list */
/* +0x04b8  element 10 of 15: type 6 */
U32(0x00000006),  /* element 10: type */
/* +0x04bc  meaning not yet known */
U32(0x00003000), U32(0x00000048), U32(0x00000049), U32(0x00000000), U32(0x00000000), U32(0x00000095),
U32(0x00000096),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: children list (relocated: installed at attach, rebind.inc) */
/* +0x04e4  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x051c  meaning not yet known */
AT(0x51c) U8(0x28),
/* +0x0541  meaning not yet known */
AT(0x541) U8(0x40), U16(0xbfe4), U8(0x2d),
/* +0x056a  meaning not yet known */
AT(0x56a) U16(0xbff0), U32(0x3fc00000), U32(0x3fc00000), U32(0x0000006c), U8(0x6c),
/* +0x05f8  the children of element 11, zero-terminated */
AT(0x5f8) U32(0x00000000),  /* element 11: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: end of the list */
/* +0x0600  element 11 of 15: type 1 */
U32(0x00000001),  /* element 11: type */
/* +0x0604  meaning not yet known */
U32(0x00002500), U32(0x00000095), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000050),
U32(0x0000002e),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: children list (relocated: installed at attach, rebind.inc) */
/* +0x062c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x0668  meaning not yet known */
AT(0x668) U8(0xae),
/* +0x06e0  the children of element 12, zero-terminated */
AT(0x6e0) U32(0x00000000),  /* element 12: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 12: end of the list */
/* +0x06e8  element 12 of 15: type 1 */
U32(0x00000001),  /* element 12: type */
/* +0x06ec  meaning not yet known */
U32(0x00001800), U32(0x00000095), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x0000000e),
U32(0x0000002d),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0718  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x2d),
/* +0x07c8  the children of element 13, zero-terminated */
AT(0x7c8) U32(0x00000000),  /* element 13: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 13: end of the list */
/* +0x07d0  element 13 of 15: type 1 */
U32(0x00000001),  /* element 13: type */
/* +0x07d4  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0xffffffad),
U32(0x0000000c),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: children list (relocated: installed at attach, rebind.inc) */
/* +0x07fc  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x6b),
/* +0x0838  meaning not yet known */
AT(0x838) U32(0xffffffb7),
/* +0x08b0  the children of element 14, zero-terminated */
AT(0x8b0) U32(0x00000000),  /* element 14: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 14: end of the list */
/* +0x08b8  element 14 of 15: type 6 */
U32(0x00000006),  /* element 14: type */
/* +0x08bc  meaning not yet known */
U32(0x00001500), U32(0x0000002e), U32(0x0000002f), U32(0x00000000), U32(0x00000000), U32(0x00000095),
U32(0x00000096),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: children list (relocated: installed at attach, rebind.inc) */
/* +0x0944  meaning not yet known */
AT(0x944) U8(0x51),
/* +0x096a  meaning not yet known */
AT(0x96a) U16(0x3ff0), U32(0x3fa00000), U32(0x3fa00000), U32(0x00000049), U8(0x49),
/* +0x09f8  the children of element 15, zero-terminated */
AT(0x9f8) U32(0x00000000),  /* element 15: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 15: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 15: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 15: end of the list */
/* +0x0a08  element 15 of 15: type 0 */
U32(0x00000000),  /* element 15: type */
/* +0x0a0c  meaning not yet known */
U16(0x1000),
AT(0xa2c) U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: children list (relocated: installed at attach, rebind.inc) */
/* +0x0a34  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0a58  the element-list slot (the header's +0x08 points here): the first element */
AT(0xa58) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0a5c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0a74  meaning not yet known */
AT(0xa74) U32(0xffffffff), U16(0xa610),
/* +0x0a8c  meaning not yet known */
AT(0xa8c) U32(0x00000001), U32(0x0000000d), U32(0x000000cb), U32(0x00000040), U32(0x00000040), U32(0xffffffff),
U16(0xa01c),
/* +0x0ab8  meaning not yet known */
AT(0xab8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001007e), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000020), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001007f), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0b24  meaning not yet known */
AT(0xb24) U32(0xffffffff),
/* +0x0b3c  meaning not yet known */
AT(0xb3c) U32(0x00000001), U32(0x000000ec), U32(0x000000cb), U32(0x00000040), U32(0x00000040), U32(0xffffffff),
U16(0xa037),
/* +0x0b68  meaning not yet known */
AT(0xb68) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010158), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000020), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010157), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0bd4  meaning not yet known */
AT(0xbd4) U32(0xffffffff),
/* +0x0bec  meaning not yet known */
AT(0xbec) U8(0x03),
/* +0x0c00  meaning not yet known */
AT(0xc00) U32(0xffffffff),
/* +0x0c2c  meaning not yet known */
AT(0xc2c) U32(0xffffffff),
/* +0x0c48  ImportTable: one import, PANELS (module id 0xf) */
AT(0xc48) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0c50  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0c58  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0c64  meaning not yet known */
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
