/* 737STROB.GAU's .data image, decompiled: every non-zero value of retail's .data (0x4b4 bytes at RVA 0x3000) at
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
AT(0x0) STR32("737-400.Strob-Nav-Switch"),  /* the gauge name */
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
U8(0x9f),
/* +0x0070  element 1 of 4: type 4 */
AT(0x70) U32(0x00000004),  /* element 1: type */
/* +0x0074  meaning not yet known */
U32(0x00002000), U32(0x00000004), U8(0x01),
AT(0x94) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x009c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x0b),
/* +0x00e0  meaning not yet known */
AT(0xe0) U8(0x02),
/* +0x00f8  element 2 of 4: type 4 */
AT(0xf8) U32(0x00000004),  /* element 2: type */
/* +0x00fc  meaning not yet known */
U32(0x00002000), U32(0x00000044), U8(0x01),
AT(0x11c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x0124  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x0a),
/* +0x0168  meaning not yet known */
AT(0x168) U8(0x02),
/* +0x0180  element 3 of 4: type 4 */
AT(0x180) U32(0x00000004),  /* element 3: type */
/* +0x0184  meaning not yet known */
U32(0x00002000), U32(0x00000084), U8(0x01),
AT(0x1a4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x01ac  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x0d),
/* +0x01f0  meaning not yet known */
AT(0x1f0) U8(0x02),
/* +0x0208  the children of element 4, zero-terminated */
AT(0x208) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0218  element 4 of 4: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x021c  meaning not yet known */
U16(0x1000),
AT(0x23c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x0244  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0268  the element-list slot (the header's +0x08 points here): the first element */
AT(0x268) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x026c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0284  meaning not yet known */
AT(0x284) U32(0xffffffff),
/* +0x029c  meaning not yet known */
AT(0x29c) U32(0x00000001), U32(0x00000001), U32(0x00000002), U32(0x0000001e), U32(0x00000046), U32(0xffffffff),
U16(0xa41d),
/* +0x02c8  meaning not yet known */
AT(0x2c8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001e), U32(0x00000046), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010018), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0308  meaning not yet known */
AT(0x308) U32(0xffffffff),
/* +0x0320  meaning not yet known */
AT(0x320) U32(0x00000001), U32(0x00000040), U32(0x00000002), U32(0x0000001e), U32(0x00000046), U32(0xffffffff),
U16(0xa41e),
/* +0x034c  meaning not yet known */
AT(0x34c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001e), U32(0x00000046), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100d6), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x038c  meaning not yet known */
AT(0x38c) U32(0xffffffff),
/* +0x03a4  meaning not yet known */
AT(0x3a4) U32(0x00000001), U32(0x00000080), U32(0x00000002), U32(0x0000001e), U32(0x00000046), U32(0xffffffff),
U16(0xa43d),
/* +0x03d0  meaning not yet known */
AT(0x3d0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001e), U32(0x00000046), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010142), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0410  meaning not yet known */
AT(0x410) U32(0xffffffff),
/* +0x0428  meaning not yet known */
AT(0x428) U8(0x03),
/* +0x043c  meaning not yet known */
AT(0x43c) U32(0xffffffff),
/* +0x0468  meaning not yet known */
AT(0x468) U32(0xffffffff),
/* +0x0480  ImportTable: one import, PANELS (module id 0xf) */
AT(0x480) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0488  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0490  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x049c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x04ac  meaning not yet known */
U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR32
