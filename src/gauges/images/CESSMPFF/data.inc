/* CESSMPFF.GAU's .data image, decompiled: every non-zero value of retail's .data (0x5c8 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR40(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR40("Cessna_182.Manifold_Pressure_Fuel_Flow"),  /* the gauge name */
/* +0x0028  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x0054  meaning not yet known */
U8(0x53),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x00000001), U8(0x02),
/* +0x0090  element 1 of 5: type 0 */
AT(0x90) U32(0x00000000),  /* element 1: type */
/* +0x0094  meaning not yet known */
U32(0x00004000), U32(0x0000001e), U8(0x52),
AT(0xb4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00e0  the children of element 2, zero-terminated */
AT(0xe0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x00e8  meaning not yet known */
U32(0x00000061), U32(0x00000042), U32(0x00000000), U32(0x40420000), U32(0x00000000), U32(0x00000000),
U32(0x00000069), U32(0x00000046), U32(0x00000000), U32(0x40418000), U32(0x00000000), U32(0x00000000),
U32(0x00000068), U32(0x000000df), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x0000005a), U32(0x000000ef), U32(0x00000000), U32(0x40200000), U32(0x00000000), U32(0x00000000),
/* +0x0148  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x014c  meaning not yet known */
U32(0x00002000), U32(0x00000039), U32(0x00000098), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x0174  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x98),
/* +0x01b0  meaning not yet known */
AT(0x1b0) U32(0x0000004d), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01bc  meaning not yet known */
U32(0x00000004), U8(0x06),
/* +0x0228  element 3 of 5: type 0 */
AT(0x228) U32(0x00000000),  /* element 3: type */
/* +0x022c  meaning not yet known */
U32(0x00003000), U32(0x000000e5), U8(0x59),
AT(0x24c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x0278  the children of element 4, zero-terminated */
AT(0x278) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0280  meaning not yet known */
U32(0x000000c1), U8(0xe9),
/* +0x0298  meaning not yet known */
AT(0x298) U32(0x000000b7), U32(0x000000e1), U32(0x00000000), U32(0x40140000), U32(0x00000000), U32(0x00000000),
U32(0x000000a4), U32(0x000000ca), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x00000098), U32(0x00000094), U32(0x00000000), U32(0x402e0000), U32(0x00000000), U32(0x00000000),
U32(0x000000b5), U32(0x00000053), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x000000cf), U32(0x00000040), U32(0x00000000), U32(0x40360000), U32(0x00000000), U32(0x00000000),
/* +0x0310  element 4 of 5: type 1 */
U32(0x00000001),  /* element 4: type */
/* +0x0314  meaning not yet known */
U32(0x00002000), U32(0x000000eb), U32(0x00000098), U32(0x00000000), U32(0x00000000), U32(0x00000006),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0340  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x89),
/* +0x0378  meaning not yet known */
AT(0x378) U32(0x0000004d), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0384  meaning not yet known */
U32(0x00000006), U8(0x06),
/* +0x03f0  the children of element 5, zero-terminated */
AT(0x3f0) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x03fc  meaning not yet known */
U32(0x00000000),
/* +0x0400  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x0404  meaning not yet known */
U16(0x1000),
AT(0x424) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x042c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0450  the element-list slot (the header's +0x08 points here): the first element */
AT(0x450) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0454  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x046c  meaning not yet known */
AT(0x46c) U32(0xffffffff),
/* +0x0484  meaning not yet known */
AT(0x484) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000096), U32(0x0000012b), U32(0xffffffff),
U16(0xa021),
/* +0x04b0  meaning not yet known */
AT(0x4b0) U8(0x03),
/* +0x04c4  meaning not yet known */
AT(0x4c4) U32(0xffffffff),
/* +0x04dc  meaning not yet known */
AT(0x4dc) U32(0x00000001), U32(0x00000096), U32(0x00000000), U32(0x00000096), U32(0x0000012b), U32(0xffffffff),
U16(0xa031),
/* +0x0508  meaning not yet known */
AT(0x508) U8(0x03),
/* +0x051c  meaning not yet known */
AT(0x51c) U32(0xffffffff),
/* +0x0534  meaning not yet known */
AT(0x534) U8(0x03),
/* +0x0548  meaning not yet known */
AT(0x548) U32(0xffffffff),
/* +0x0574  meaning not yet known */
AT(0x574) U32(0xffffffff),
/* +0x0590  ImportTable: one import, PANELS (module id 0xf) */
AT(0x590) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0598  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x05a0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x05ac  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x05bc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR40
