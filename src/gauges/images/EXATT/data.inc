/* EXATT.GAU's .data image, decompiled: every non-zero value of retail's .data (0x428 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Extra-300.Attitude"),  /* the gauge name */
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
U8(0x59),
/* +0x0068  meaning not yet known */
AT(0x68) U8(0x08),
/* +0x0080  element 1 of 3: type 6 */
AT(0x80) U32(0x00000006),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0x00003000), U32(0x00000010), U32(0x0000000d), U32(0x00000000), U32(0x00000000), U32(0x00000089),
U32(0x00000086),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00b0  meaning not yet known */
U8(0x01),
/* +0x010c  meaning not yet known */
AT(0x10c) U8(0x50),
/* +0x0132  meaning not yet known */
AT(0x132) U16(0xbff0), U32(0x3f866666), U32(0x3f866666), U32(0x00000030), U8(0x30),
/* +0x01c0  the children of element 2, zero-terminated */
AT(0x1c0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x01c8  element 2 of 3: type 6 */
U32(0x00000006),  /* element 2: type */
/* +0x01cc  meaning not yet known */
U32(0x00002000), U32(0x00000037), U32(0x00000035), U32(0x00000000), U32(0x00000000), U32(0x00000089),
U32(0x00000086),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01f8  meaning not yet known */
U8(0x01),
/* +0x022c  meaning not yet known */
AT(0x22c) U8(0x4e),
/* +0x0253  meaning not yet known */
AT(0x253) U8(0x40), U8(0x50),
/* +0x027a  meaning not yet known */
AT(0x27a) U16(0xbff0), U32(0x3f0ccccd), U32(0x3f0ccccd), U32(0x0000004c), U8(0x4c),
/* +0x0308  the children of element 3, zero-terminated */
AT(0x308) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0310  element 3 of 3: type 0 */
U32(0x00000000),  /* element 3: type */
/* +0x0314  meaning not yet known */
U16(0x1000),
AT(0x334) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0340  meaning not yet known */
U8(0x01),
/* +0x0360  the element-list slot (the header's +0x08 points here): the first element */
AT(0x360) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0364  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x037c  meaning not yet known */
AT(0x37c) U32(0xffffffff), U16(0xa505),
/* +0x0394  meaning not yet known */
AT(0x394) U8(0x03),
/* +0x03a8  meaning not yet known */
AT(0x3a8) U32(0xffffffff),
/* +0x03d4  meaning not yet known */
AT(0x3d4) U32(0xffffffff),
/* +0x03f0  ImportTable: one import, PANELS (module id 0xf) */
AT(0x3f0) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x03f8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0400  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x040c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x041c  meaning not yet known */
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
