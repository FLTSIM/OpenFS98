/* BKRADAP.GAU's .data image, decompiled: every non-zero value of retail's .data (0xe7c bytes at RVA 0x3000) at
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
AT(0x0) STR24("Bendix/King Radio AP"),  /* the gauge name */
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
U8(0xaa),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  element 1 of 11: type 4 */
AT(0x80) U32(0x00000004),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0x00001500), U32(0x00000014), U8(0x5b),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00b0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x67),
/* +0x00f0  meaning not yet known */
AT(0xf0) U8(0x01),
/* +0x0108  element 2 of 11: type 4 */
AT(0x108) U32(0x00000004),  /* element 2: type */
/* +0x010c  meaning not yet known */
U32(0x00002000), U32(0x00000057), U8(0x5b),
AT(0x12c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0138  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6a),
/* +0x0178  meaning not yet known */
AT(0x178) U8(0x01),
/* +0x0190  element 3 of 11: type 4 */
AT(0x190) U32(0x00000004),  /* element 3: type */
/* +0x0194  meaning not yet known */
U32(0x00002500), U32(0x000000a3), U8(0x5b),
AT(0x1b4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01c0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x69),
/* +0x0200  meaning not yet known */
AT(0x200) U8(0x01),
/* +0x0218  element 4 of 11: type 4 */
AT(0x218) U32(0x00000004),  /* element 4: type */
/* +0x021c  meaning not yet known */
U32(0x00003000), U32(0x000000e9), U8(0x5b),
AT(0x23c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0248  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x70),
/* +0x0288  meaning not yet known */
AT(0x288) U8(0x01),
/* +0x02a0  element 5 of 11: type 4 */
AT(0x2a0) U32(0x00000004),  /* element 5: type */
/* +0x02a4  meaning not yet known */
U32(0x00003500), U32(0x00000134), U8(0x5b),
AT(0x2c4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02d0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x71),
/* +0x0310  meaning not yet known */
AT(0x310) U8(0x01),
/* +0x0328  element 6 of 11: type 4 */
AT(0x328) U32(0x00000004),  /* element 6: type */
/* +0x032c  meaning not yet known */
U32(0x00004000), U32(0x00000182), U8(0x5b),
AT(0x34c) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0358  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U8(0x6c),
/* +0x0398  meaning not yet known */
AT(0x398) U8(0x01),
/* +0x03b0  element 7 of 11: type 2 (text, 0xbc bytes) */
AT(0x3b0) U32(0x00000002),  /* element 7: type */
/* +0x03b4  meaning not yet known */
U32(0xffffffff), U32(0x00000060), U8(0x0d),
AT(0x3d4) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03e0  meaning not yet known */
U32(0x00000006), U32(0x00000002), U32(0x00000000), U8(0x6a),
AT(0x468) U32(0x00000000),  /* element 7: text buffer */
/* +0x046c  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x0000001c), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x4a8) STR12("Courier New"),
/* +0x04c8  meaning not yet known */
AT(0x4c8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x04e8  element 8 of 11: type 2 (text, 0xbc bytes) */
AT(0x4e8) U32(0x00000002),  /* element 8: type */
/* +0x04ec  meaning not yet known */
U32(0xffffffff), U32(0x00000060), U8(0x29),
AT(0x50c) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0518  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x69),
/* +0x0540  meaning not yet known */
AT(0x540) U8(0x70),
/* +0x055c  meaning not yet known */
AT(0x55c) U8(0x71),
AT(0x5a0) U32(0x00000000),  /* element 8: text buffer */
/* +0x05a4  meaning not yet known */
U32(0x00000000), U32(0x00000050), U32(0x0000001c), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x5e0) STR12("Courier New"),
/* +0x0600  meaning not yet known */
AT(0x600) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0620  element 9 of 11: type 2 (text, 0xbc bytes) */
AT(0x620) U32(0x00000002),  /* element 9: type */
/* +0x0624  meaning not yet known */
U32(0xffffffff), U32(0x000000e6), U8(0x0d),
AT(0x644) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0650  meaning not yet known */
U32(0x00000006), U32(0x00000001), U32(0x00000000), U8(0x6d),
AT(0x6d8) U32(0x00000000),  /* element 9: text buffer */
/* +0x06dc  meaning not yet known */
U32(0x00000000), U32(0x000000c8), U32(0x0000001c), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x718) STR12("Courier New"),
/* +0x0738  meaning not yet known */
AT(0x738) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x09),
/* +0x0758  element 10 of 11: type 2 (text, 0xbc bytes) */
AT(0x758) U32(0x00000002),  /* element 10: type */
/* +0x075c  meaning not yet known */
U32(0xffffffff), U32(0x000000e6), U8(0x29),
AT(0x77c) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0788  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x78),
AT(0x810) U32(0x00000000),  /* element 10: text buffer */
/* +0x0814  meaning not yet known */
U32(0x00000000), U32(0x000000c8), U32(0x0000001c), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x850) STR12("Courier New"),
/* +0x0870  meaning not yet known */
AT(0x870) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x09),
/* +0x0890  the children of element 11, zero-terminated */
AT(0x890) U32(0x00000000),  /* element 11: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 11: end of the list */
/* +0x08bc  meaning not yet known */
U32(0x00000000),
/* +0x08c0  element 11 of 11: type 0 */
U32(0x00000000),  /* element 11: type */
/* +0x08c4  meaning not yet known */
U16(0x1000),
AT(0x8e4) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: children list (relocated: installed at attach, rebind.inc) */
/* +0x08ec  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0910  the element-list slot (the header's +0x08 points here): the first element */
AT(0x910) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0914  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x092c  meaning not yet known */
AT(0x92c) U32(0xffffffff), U16(0xa70c),
/* +0x0944  meaning not yet known */
AT(0x944) U32(0x00000001), U32(0x00000014), U32(0x0000005b), U32(0x00000022), U32(0x00000014), U32(0xffffffff),
U16(0xa121),
/* +0x0970  meaning not yet known */
AT(0x970) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000022), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0998  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x09b0  meaning not yet known */
AT(0x9b0) U32(0xffffffff),
/* +0x09c8  meaning not yet known */
AT(0x9c8) U32(0x00000001), U32(0x00000057), U32(0x0000005b), U32(0x00000022), U32(0x00000014), U32(0xffffffff),
U16(0xa404),
/* +0x09f4  meaning not yet known */
AT(0x9f4) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000022), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a1c  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0a34  meaning not yet known */
AT(0xa34) U32(0xffffffff),
/* +0x0a4c  meaning not yet known */
AT(0xa4c) U32(0x00000001), U32(0x000000a3), U32(0x0000005b), U32(0x00000022), U32(0x00000014), U32(0xffffffff),
U16(0xa406),
/* +0x0a78  meaning not yet known */
AT(0xa78) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000022), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100c1), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0ab8  meaning not yet known */
AT(0xab8) U32(0xffffffff),
/* +0x0ad0  meaning not yet known */
AT(0xad0) U32(0x00000001), U32(0x000000e9), U32(0x0000005b), U32(0x00000022), U32(0x00000014), U32(0xffffffff),
U16(0xa407),
/* +0x0afc  meaning not yet known */
AT(0xafc) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000022), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100bc), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0b3c  meaning not yet known */
AT(0xb3c) U32(0xffffffff),
/* +0x0b54  meaning not yet known */
AT(0xb54) U32(0x00000001), U32(0x00000134), U32(0x0000005b), U32(0x00000022), U32(0x00000014), U32(0xffffffff),
U16(0xa408),
/* +0x0b80  meaning not yet known */
AT(0xb80) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000022), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100c0), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0bc0  meaning not yet known */
AT(0xbc0) U32(0xffffffff),
/* +0x0bd8  meaning not yet known */
AT(0xbd8) U32(0x00000001), U32(0x00000182), U32(0x0000005b), U32(0x00000022), U32(0x00000014), U32(0xffffffff),
U16(0xa405),
/* +0x0c04  meaning not yet known */
AT(0xc04) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000022), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c2c  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0c44  meaning not yet known */
AT(0xc44) U32(0xffffffff),
/* +0x0c5c  meaning not yet known */
AT(0xc5c) U32(0x00000001), U32(0x000000e6), U32(0x0000000d), U32(0x000000c8), U32(0x0000001c), U32(0xffffffff),
U16(0xa430),
/* +0x0c88  meaning not yet known */
AT(0xc88) U32(0x00000002), U32(0x00000059), U32(0x00000000), U32(0x00000037), U32(0x0000001c), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010165), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000091), U32(0x00000000), U32(0x00000037), U32(0x0000001c), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010164), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0cf4  meaning not yet known */
AT(0xcf4) U32(0xffffffff),
/* +0x0d0c  meaning not yet known */
AT(0xd0c) U32(0x00000001), U32(0x000000e6), U32(0x00000029), U32(0x000000c8), U32(0x0000001c), U32(0xffffffff),
U16(0xa441),
/* +0x0d38  meaning not yet known */
AT(0xd38) U32(0x00000002), U32(0x00000059), U32(0x00000000), U32(0x00000037), U32(0x0000001c), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010167), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000091), U32(0x00000000), U32(0x00000037), U32(0x0000001c), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010166), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0da4  meaning not yet known */
AT(0xda4) U32(0xffffffff),
/* +0x0dbc  meaning not yet known */
AT(0xdbc) U8(0x03),
/* +0x0dd0  meaning not yet known */
AT(0xdd0) U32(0xffffffff),
/* +0x0dfc  meaning not yet known */
AT(0xdfc) U32(0xffffffff),
/* +0x0e14  meaning not yet known */
AT(0xe14) U32(0x00474448), U32(0x00004342), U32(0x00525041), U32(0x0056414e),
STR12("%ALT %05d"),
STR12("VS  %05d"),
/* +0x0e3c  meaning not yet known */
U32(0x00000000),
/* +0x0e40  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0e48  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0e50  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0e5c  meaning not yet known */
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
#undef STR12
#undef STR24
