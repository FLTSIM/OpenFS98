/* 737START.GAU's .data image, decompiled: every non-zero value of retail's .data (0x458 bytes at RVA 0x3000) at
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
AT(0x0) STR24("737-400.Engine-Start"),  /* the gauge name */
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
U8(0xeb),
/* +0x0068  element 1 of 3: type 4 */
AT(0x68) U32(0x00000004),  /* element 1: type */
/* +0x006c  meaning not yet known */
U32(0x00002000), U32(0x00000000), U8(0x09),
AT(0x8c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x0094  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x84),
/* +0x00d8  meaning not yet known */
AT(0xd8) U8(0x03),
/* +0x00f0  element 2 of 3: type 4 */
AT(0xf0) U32(0x00000004),  /* element 2: type */
/* +0x00f4  meaning not yet known */
U32(0x00002000), U32(0x00000083), U8(0x09),
AT(0x114) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x011c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0xa7),
/* +0x0160  meaning not yet known */
AT(0x160) U8(0x03),
/* +0x0178  the children of element 3, zero-terminated */
AT(0x178) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0184  meaning not yet known */
U32(0x00000000),
/* +0x0188  element 3 of 3: type 0 */
U32(0x00000000),  /* element 3: type */
/* +0x018c  meaning not yet known */
U16(0x1000),
AT(0x1ac) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x01b4  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x01d8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x1d8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x01dc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x01f4  meaning not yet known */
AT(0x1f4) U32(0xffffffff),
/* +0x020c  meaning not yet known */
AT(0x20c) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x0000006b), U32(0x0000003b), U32(0xffffffff),
U16(0xa41f),
/* +0x0238  meaning not yet known */
AT(0x238) U32(0x00000002), U32(0x0000002f), U32(0x00000000), U32(0x0000003c), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0260  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x0000002f), U32(0x00000014), U32(0x0000003c), U32(0x00000014),
U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x028c  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x0000002f), U32(0x00000028), U32(0x0000003c), U32(0x00000014),
U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02b8  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x02d0  meaning not yet known */
AT(0x2d0) U32(0xffffffff),
/* +0x02e8  meaning not yet known */
AT(0x2e8) U32(0x00000001), U32(0x00000081), U32(0x00000000), U32(0x0000006b), U32(0x0000003b), U32(0xffffffff),
U16(0xa420),
/* +0x0314  meaning not yet known */
AT(0x314) U32(0x00000002), U32(0x0000002f), U32(0x00000000), U32(0x0000003c), U32(0x00000014), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x033c  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x0000002f), U32(0x00000014), U32(0x0000003c), U32(0x00000014),
U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0368  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x0000002f), U32(0x00000028), U32(0x0000003c), U32(0x00000014),
U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0394  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x03ac  meaning not yet known */
AT(0x3ac) U32(0xffffffff),
/* +0x03c4  meaning not yet known */
AT(0x3c4) U8(0x03),
/* +0x03d8  meaning not yet known */
AT(0x3d8) U32(0xffffffff),
/* +0x0404  meaning not yet known */
AT(0x404) U32(0xffffffff),
/* +0x0420  ImportTable: one import, PANELS (module id 0xf) */
AT(0x420) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0428  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0430  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x043c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x044c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR24
