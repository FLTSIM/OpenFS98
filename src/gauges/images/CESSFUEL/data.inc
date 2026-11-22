/* CESSFUEL.GAU's .data image, decompiled: every non-zero value of retail's .data (0x608 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR32(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR32("Cessna_182.Fuel_Quantity"),  /* the gauge name */
/* +0x0020  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x004c  meaning not yet known */
U8(0x39),
/* +0x0070  meaning not yet known */
AT(0x70) U32(0x00000001), U32(0x00000002), U32(0x00000000), U32(0x0000000a), U8(0x01),
/* +0x0098  element 1 of 5: type 0 */
AT(0x98) U32(0x00000000),  /* element 1: type */
/* +0x009c  meaning not yet known */
U32(0x00003000), U32(0x00000032), U8(0x55),
AT(0xbc) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00e8  the children of element 2, zero-terminated */
AT(0xe8) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x00f0  meaning not yet known */
U32(0x0000006c), U32(0x00000055), U32(0x00000000), U32(0x40450000), U32(0x00000000), U32(0x00000000),
U32(0x00000087), U32(0x0000006e), U32(0x00000000), U32(0x403e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000093), U32(0x00000091), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x00000090), U32(0x000000b5), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x00000086), U32(0x000000c9), U32(0x00000000), U32(0x40140000), U32(0x00000000), U32(0x00000000),
U32(0x00000072), U8(0xdd),
/* +0x0180  element 2 of 5: type 1 */
AT(0x180) U32(0x00000001),  /* element 2: type */
/* +0x0184  meaning not yet known */
U32(0x00002000), U32(0x00000046), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01b0  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x0130),
/* +0x01e8  meaning not yet known */
AT(0x1e8) U32(0x00000045), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01f4  meaning not yet known */
U32(0x00000006), U8(0x06),
/* +0x0260  element 3 of 5: type 0 */
AT(0x260) U32(0x00000000),  /* element 3: type */
/* +0x0264  meaning not yet known */
U32(0x00004000), U32(0x000000d9), U8(0x4d),
AT(0x284) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x02b0  the children of element 4, zero-terminated */
AT(0x2b0) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x02b8  meaning not yet known */
U32(0x000000bc), U8(0xdb),
/* +0x02d0  meaning not yet known */
AT(0x2d0) U32(0x000000a9), U32(0x000000c8), U32(0x00000000), U32(0x40140000), U32(0x00000000), U32(0x00000000),
U32(0x0000009e), U32(0x000000b4), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x0000009a), U32(0x00000091), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x000000a5), U32(0x0000006c), U32(0x00000000), U32(0x403e0000), U32(0x00000000), U32(0x00000000),
U32(0x000000c0), U32(0x00000052), U32(0x00000000), U32(0x40450000), U32(0x00000000), U32(0x00000000),
/* +0x0348  element 4 of 5: type 1 */
U32(0x00000001),  /* element 4: type */
/* +0x034c  meaning not yet known */
U32(0x00002000), U32(0x000000df), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0378  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x012f),
/* +0x03b0  meaning not yet known */
AT(0x3b0) U32(0x00000045), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03bc  meaning not yet known */
U32(0x00000006), U8(0x06),
/* +0x0428  the children of element 5, zero-terminated */
AT(0x428) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0434  meaning not yet known */
U32(0x00000000),
/* +0x0438  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x043c  meaning not yet known */
U16(0x1000),
AT(0x45c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0468  meaning not yet known */
U8(0x01),
/* +0x0488  the element-list slot (the header's +0x08 points here): the first element */
AT(0x488) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x048c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x04a4  meaning not yet known */
AT(0x4a4) U32(0xffffffff),
/* +0x04bc  meaning not yet known */
AT(0x4bc) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000096), U32(0x0000012c), U32(0xffffffff),
U16(0xa029),
/* +0x04e8  meaning not yet known */
AT(0x4e8) U8(0x03),
/* +0x04fc  meaning not yet known */
AT(0x4fc) U32(0xffffffff),
/* +0x0514  meaning not yet known */
AT(0x514) U32(0x00000001), U32(0x00000096), U32(0x00000000), U32(0x00000096), U32(0x0000012c), U32(0xffffffff),
U16(0xa02a),
/* +0x0540  meaning not yet known */
AT(0x540) U8(0x03),
/* +0x0554  meaning not yet known */
AT(0x554) U32(0xffffffff),
/* +0x056c  meaning not yet known */
AT(0x56c) U8(0x03),
/* +0x0580  meaning not yet known */
AT(0x580) U32(0xffffffff),
/* +0x05ac  meaning not yet known */
AT(0x5ac) U32(0xffffffff),
/* +0x05c8  ImportTable: one import, PANELS (module id 0xf) */
AT(0x5c8) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x05d0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x05d8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x05e4  meaning not yet known */
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
#undef STR32
