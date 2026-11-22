/* SOPRPM.GAU's .data image, decompiled: every non-zero value of retail's .data (0x3a8 bytes at RVA 0x3000) at
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
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR16("Sopwith Rpm"),  /* the gauge name */
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
U8(0x71),
/* +0x0060  meaning not yet known */
AT(0x60) U8(0x88),
/* +0x0080  meaning not yet known */
AT(0x80) U32(0x00000082), U8(0xda),
/* +0x0098  meaning not yet known */
AT(0x98) U32(0x0000005d), U32(0x000000cb), U32(0x00000000), U32(0x4082c000), U32(0x00000000), U32(0x00000000),
U32(0x0000003e), U32(0x000000ad), U32(0x00000000), U32(0x40890000), U32(0x00000000), U32(0x00000000),
U32(0x0000002e), U32(0x0000007b), U32(0x00000000), U32(0x408f4000), U32(0x00000000), U32(0x00000000),
U32(0x0000003c), U32(0x0000003f), U32(0x00000000), U32(0x4092c000), U32(0x00000000), U32(0x00000000),
U32(0x0000006a), U32(0x0000001c), U32(0x00000000), U32(0x4095e000), U32(0x00000000), U32(0x00000000),
U32(0x000000a5), U32(0x00000016), U32(0x00000000), U32(0x40990000), U32(0x00000000), U32(0x00000000),
U32(0x000000d7), U32(0x00000032), U32(0x00000000), U32(0x409c2000), U32(0x00000000), U32(0x00000000),
U32(0x000000f1), U32(0x00000064), U32(0x00000000), U32(0x409f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000e7), U32(0x000000a2), U32(0x00000000), U32(0x40a13000), U32(0x00000000), U32(0x00000000),
U32(0x000000b7), U32(0x000000cf), U32(0x00000000), U32(0x40a45000), U32(0x00000000), U32(0x00000000),
U32(0x0000008e), U32(0x000000dc), U32(0x00000000), U32(0x40a5e000), U32(0x00000000), U32(0x00000000),
/* +0x01a0  element 1 of 2: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x01a4  meaning not yet known */
U32(0x00001100), U32(0x00000090), U32(0x0000006f), U32(0x00000000), U32(0x00000000), U32(0x0000000b),
U32(0x0000000b),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x01cc  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x9a),
/* +0x0208  meaning not yet known */
AT(0x208) U32(0x00000090), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0214  meaning not yet known */
U32(0x0000000c), U8(0x06),
/* +0x0280  the children of element 2, zero-terminated */
AT(0x280) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0288  element 2 of 2: type 0 */
U32(0x00000000),  /* element 2: type */
/* +0x028c  meaning not yet known */
U16(0x1000),
AT(0x2ac) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x02b4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x02d8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x2d8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x02dc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x02f4  meaning not yet known */
AT(0x2f4) U32(0xffffffff), U16(0xa207),
/* +0x030c  meaning not yet known */
AT(0x30c) U8(0x03),
/* +0x0320  meaning not yet known */
AT(0x320) U32(0xffffffff),
/* +0x034c  meaning not yet known */
AT(0x34c) U32(0xffffffff),
/* +0x0368  ImportTable: one import, PANELS (module id 0xf) */
AT(0x368) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0370  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0378  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0384  meaning not yet known */
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
#undef STR16
