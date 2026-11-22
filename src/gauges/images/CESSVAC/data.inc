/* CESSVAC.GAU's .data image, decompiled: every non-zero value of retail's .data (0x558 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Cessna_182.Vacuum_Amps"),  /* the gauge name */
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
U8(0x39),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x02),
/* +0x0080  element 1 of 5: type 0 */
AT(0x80) U32(0x00000000),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0x00004000), U32(0x000000e1), U8(0x5b),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00ac  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x00d0  the children of element 2, zero-terminated */
AT(0xd0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x00d8  meaning not yet known */
U32(0x000000c0), U32(0x000000e5), U32(0x00000000), U32(0xc0504000), U32(0x00000000), U32(0x00000000),
U32(0x000000c3), U32(0x00000055), U32(0x00000000), U32(0x40504000), U32(0x00000000), U32(0x00000000),
/* +0x0108  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x010c  meaning not yet known */
U32(0x00002000), U32(0x000000e3), U32(0x0000009e), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0138  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x0160),
/* +0x0170  meaning not yet known */
AT(0x170) U32(0x00000047), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x017c  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x01e8  element 3 of 5: type 0 */
AT(0x1e8) U32(0x00000000),  /* element 3: type */
/* +0x01ec  meaning not yet known */
U32(0x00003000), U32(0x0000002d), U8(0x43),
AT(0x20c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x0214  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0238  the children of element 4, zero-terminated */
AT(0x238) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0240  meaning not yet known */
U32(0x00000061), U32(0x0000004d), U32(0x00000000), U32(0x401e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000089), U32(0x00000075), U32(0x00000000), U32(0x40180000), U32(0x00000000), U32(0x00000000),
U32(0x0000007d), U32(0x000000cd), U32(0x00000000), U32(0x40100000), U32(0x00000000), U32(0x00000000),
U32(0x0000005c), U32(0x000000e9), U32(0x00000000), U32(0x40040000), U32(0x00000000), U32(0x00000000),
/* +0x02a0  element 4 of 5: type 1 */
U32(0x00000001),  /* element 4: type */
/* +0x02a4  meaning not yet known */
U32(0x00002000), U32(0x00000041), U32(0x0000009c), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x02cc  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x0161),
/* +0x0308  meaning not yet known */
AT(0x308) U32(0x00000047), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0314  meaning not yet known */
U32(0x00000004), U8(0x06),
/* +0x0380  the children of element 5, zero-terminated */
AT(0x380) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x038c  meaning not yet known */
U32(0x00000000),
/* +0x0390  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x0394  meaning not yet known */
U16(0x1000),
AT(0x3b4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x03bc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03e0  the element-list slot (the header's +0x08 points here): the first element */
AT(0x3e0) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x03e4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03fc  meaning not yet known */
AT(0x3fc) U32(0xffffffff),
/* +0x0414  meaning not yet known */
AT(0x414) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000096), U32(0x0000012e), U32(0xffffffff),
U16(0xa036),
/* +0x0440  meaning not yet known */
AT(0x440) U8(0x03),
/* +0x0454  meaning not yet known */
AT(0x454) U32(0xffffffff),
/* +0x046c  meaning not yet known */
AT(0x46c) U32(0x00000001), U32(0x00000096), U32(0x00000000), U32(0x00000098), U32(0x0000012e), U32(0xffffffff),
U16(0xa518),
/* +0x0498  meaning not yet known */
AT(0x498) U8(0x03),
/* +0x04ac  meaning not yet known */
AT(0x4ac) U32(0xffffffff),
/* +0x04c4  meaning not yet known */
AT(0x4c4) U8(0x03),
/* +0x04d8  meaning not yet known */
AT(0x4d8) U32(0xffffffff),
/* +0x0504  meaning not yet known */
AT(0x504) U32(0xffffffff),
/* +0x0520  ImportTable: one import, PANELS (module id 0xf) */
AT(0x520) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0528  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0530  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x053c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x054c  meaning not yet known */
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
