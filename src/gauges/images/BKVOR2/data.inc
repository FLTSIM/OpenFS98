/* BKVOR2.GAU's .data image, decompiled: every non-zero value of retail's .data (0x5b8 bytes at RVA 0x3000) at
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
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Bendix_King.VOR2"),  /* the gauge name */
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
AT(0x68) U32(0x00000001), U32(0x00000002), U32(0x00000000), U32(0x0000000f), U8(0x02),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x000000d2), U32(0x000000c0), U32(0x00000000), U32(0x405fc000), U32(0x00000000), U32(0x00000000),
U32(0x00000096), U8(0xc8),
/* +0x00c0  meaning not yet known */
AT(0xc0) U32(0x0000004e), U32(0x000000c0), U32(0x00000000), U32(0xc0600000), U32(0x00000000), U32(0x00000000),
/* +0x00d8  element 1 of 5: type 0 */
U32(0x00000000),  /* element 1: type */
/* +0x00dc  meaning not yet known */
U32(0x00005000), U32(0x0000008d), U8(0x37),
AT(0xfc) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x0104  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0128  the children of element 2, zero-terminated */
AT(0x128) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0130  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x0134  meaning not yet known */
U32(0x00003000), U32(0x00000095), U32(0x00000032), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000003),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0160  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x32),
/* +0x0198  meaning not yet known */
AT(0x198) U32(0x0000008f), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01a4  meaning not yet known */
U32(0x00000003), U8(0x06),
/* +0x0210  the children of element 3, zero-terminated */
AT(0x210) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0218  element 3 of 5: type 4 */
U32(0x00000004),  /* element 3: type */
/* +0x021c  meaning not yet known */
U32(0x00004000), U32(0x000000b3), U8(0x8b),
AT(0x23c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x0254  meaning not yet known */
AT(0x254) U8(0x34),
/* +0x0288  meaning not yet known */
AT(0x288) U8(0x03),
/* +0x02a0  the children of element 4, zero-terminated */
AT(0x2a0) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x02a8  element 4 of 5: type 6 */
U32(0x00000006),  /* element 4: type */
/* +0x02ac  meaning not yet known */
U32(0x00002000), U32(0x00000012), U32(0x00000010), U32(0x00000000), U32(0x00000000), U32(0x00000096),
U32(0x00000096),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x0334  meaning not yet known */
AT(0x334) U8(0x36),
/* +0x035a  meaning not yet known */
AT(0x35a) U16(0x3ff0),
STR12("33s\?33s\?7"),
/* +0x0368  meaning not yet known */
U8(0x37),
/* +0x03e8  the children of element 5, zero-terminated */
AT(0x3e8) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x03f0  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x03f4  meaning not yet known */
U16(0x1000),
AT(0x414) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x041c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0440  the element-list slot (the header's +0x08 points here): the first element */
AT(0x440) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0444  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x045c  meaning not yet known */
AT(0x45c) U32(0xffffffff), U16(0xa01d),
/* +0x0474  meaning not yet known */
AT(0x474) U32(0x00000001), U32(0x00000002), U32(0x000000e8), U32(0x00000040), U32(0x00000040), U32(0xffffffff),
U16(0xa01e),
/* +0x04a0  meaning not yet known */
AT(0x4a0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010080), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000020), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010081), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x050c  meaning not yet known */
AT(0x50c) U32(0xffffffff),
/* +0x0524  meaning not yet known */
AT(0x524) U8(0x03),
/* +0x0538  meaning not yet known */
AT(0x538) U32(0xffffffff),
/* +0x0564  meaning not yet known */
AT(0x564) U32(0xffffffff),
/* +0x0580  ImportTable: one import, PANELS (module id 0xf) */
AT(0x580) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0588  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0590  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x059c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x05ac  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR12
#undef STR24
