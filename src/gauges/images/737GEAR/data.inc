/* 737GEAR.GAU's .data image, decompiled: every non-zero value of retail's .data (0x448 bytes at RVA 0x3000) at
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
AT(0x0) STR24("737-400.Landing-Gear"),  /* the gauge name */
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
U8(0x5a),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  meaning not yet known */
AT(0x80) U16(0x014b),
/* +0x00a0  element 1 of 5: type 3 */
AT(0xa0) U32(0x00000003),  /* element 1: type */
/* +0x00a4  meaning not yet known */
U32(0x00002000), U32(0x00000025), U8(0x23),
AT(0xc4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00cc  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0108  meaning not yet known */
AT(0x108) U16(0x014b),
/* +0x012e  meaning not yet known */
AT(0x12e) U16(0x3f7d),
/* +0x0148  the children of element 2, zero-terminated */
AT(0x148) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0150  element 2 of 5: type 4 */
U32(0x00000004),  /* element 2: type */
/* +0x0154  meaning not yet known */
U32(0x00004000), U32(0x00000000), U8(0x17),
AT(0x174) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0180  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x014d),
/* +0x01c0  meaning not yet known */
AT(0x1c0) U8(0x02),
/* +0x01d8  the children of element 3, zero-terminated */
AT(0x1d8) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x01e0  element 3 of 5: type 4 */
U32(0x00000004),  /* element 3: type */
/* +0x01e4  meaning not yet known */
U32(0x00003000), U32(0x0000002b), U8(0x17),
AT(0x204) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0210  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x014e),
/* +0x0250  meaning not yet known */
AT(0x250) U8(0x02),
/* +0x0268  the children of element 4, zero-terminated */
AT(0x268) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0270  element 4 of 5: type 4 */
U32(0x00000004),  /* element 4: type */
/* +0x0274  meaning not yet known */
U32(0x00005000), U8(0x15),
AT(0x294) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02a0  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x014c),
/* +0x02e0  meaning not yet known */
AT(0x2e0) U8(0x02),
/* +0x02f8  the children of element 5, zero-terminated */
AT(0x2f8) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0300  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x0304  meaning not yet known */
U16(0x1000),
AT(0x324) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x032c  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0350  the element-list slot (the header's +0x08 points here): the first element */
AT(0x350) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0354  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x036c  meaning not yet known */
AT(0x36c) U32(0xffffffff), U16(0xa432),
/* +0x0384  meaning not yet known */
AT(0x384) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000056), U32(0x000000f4), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010022), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x03c4  meaning not yet known */
AT(0x3c4) U32(0xffffffff),
/* +0x03f0  meaning not yet known */
AT(0x3f0) U32(0xffffffff),
/* +0x0408  ImportTable: one import, PANELS (module id 0xf) */
AT(0x408) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0410  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0418  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0424  meaning not yet known */
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
