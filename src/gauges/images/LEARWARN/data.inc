/* LEARWARN.GAU's .data image, decompiled: every non-zero value of retail's .data (0x67c bytes at RVA 0x3000) at
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
AT(0x0) STR32("Lear_45.Warning_Enunciators.h"),  /* the gauge name */
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
U16(0x017c),
/* +0x0070  element 1 of 9: type 4 */
AT(0x70) U32(0x00000004),  /* element 1: type */
/* +0x0074  meaning not yet known */
U32(0x00000500), U32(0x00000052), U8(0x1c),
AT(0x94) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x009c  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x00e0  meaning not yet known */
AT(0xe0) U8(0x01),
/* +0x00f8  element 2 of 9: type 4 */
AT(0xf8) U32(0x00000004),  /* element 2: type */
/* +0x00fc  meaning not yet known */
U32(0x00000400), U32(0x00000087), U8(0x1c),
AT(0x11c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x0124  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0168  meaning not yet known */
AT(0x168) U8(0x01),
/* +0x0180  element 3 of 9: type 4 */
AT(0x180) U32(0x00000004),  /* element 3: type */
/* +0x0184  meaning not yet known */
U32(0x00000300), U32(0x000000bc), U8(0x1c),
AT(0x1a4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x01ac  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x01f0  meaning not yet known */
AT(0x1f0) U8(0x01),
/* +0x0208  element 4 of 9: type 4 */
AT(0x208) U32(0x00000004),  /* element 4: type */
/* +0x020c  meaning not yet known */
U32(0x00000100), U32(0x000000f1), U8(0x1c),
AT(0x22c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x0234  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0278  meaning not yet known */
AT(0x278) U8(0x01),
/* +0x0290  element 5 of 9: type 4 */
AT(0x290) U32(0x00000004),  /* element 5: type */
/* +0x0294  meaning not yet known */
U32(0x00000200), U32(0x00000156), U8(0x1c),
AT(0x2b4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
/* +0x02bc  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0300  meaning not yet known */
AT(0x300) U8(0x01),
/* +0x0318  element 6 of 9: type 4 */
AT(0x318) U32(0x00000004),  /* element 6: type */
/* +0x031c  meaning not yet known */
U32(0x00000600), U32(0x0000018b), U8(0x1c),
AT(0x33c) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
/* +0x0344  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0388  meaning not yet known */
AT(0x388) U8(0x01),
/* +0x03a0  element 7 of 9: type 4 */
AT(0x3a0) U32(0x00000004),  /* element 7: type */
/* +0x03a4  meaning not yet known */
U32(0x00000700), U32(0x000001c0), U8(0x1c),
AT(0x3c4) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
/* +0x03cc  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0410  meaning not yet known */
AT(0x410) U8(0x01),
/* +0x0428  element 8 of 9: type 4 */
AT(0x428) U32(0x00000004),  /* element 8: type */
/* +0x042c  meaning not yet known */
U32(0x00000800), U32(0x000001f5), U8(0x1c),
AT(0x44c) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
/* +0x0454  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0498  meaning not yet known */
AT(0x498) U8(0x01),
/* +0x04b0  the children of element 9, zero-terminated */
AT(0x4b0) U32(0x00000000),  /* element 9: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: end of the list */
/* +0x04d4  meaning not yet known */
U32(0x00000000),
/* +0x04d8  element 9 of 9: type 0 */
U32(0x00000000),  /* element 9: type */
/* +0x04dc  meaning not yet known */
U16(0x1000),
AT(0x4fc) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: children list (relocated: installed at attach, rebind.inc) */
/* +0x0504  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0528  the element-list slot (the header's +0x08 points here): the first element */
AT(0x528) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x052c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0544  meaning not yet known */
AT(0x544) U32(0xffffffff), U16(0xa1e2),
/* +0x055c  meaning not yet known */
AT(0x55c) U32(0x00000001), U32(0x00000250), U32(0x00000026), U32(0x0000001e), U32(0x0000001e), U32(0xffffffff),
U16(0xa1e3),
/* +0x0588  meaning not yet known */
AT(0x588) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001e), U32(0x0000001e), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05b0  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x05c8  meaning not yet known */
AT(0x5c8) U32(0xffffffff),
/* +0x05e0  meaning not yet known */
AT(0x5e0) U8(0x03),
/* +0x05f4  meaning not yet known */
AT(0x5f4) U32(0xffffffff),
/* +0x0620  meaning not yet known */
AT(0x620) U32(0xffffffff),
/* +0x0638  ImportTable: one import, PANELS (module id 0xf) */
AT(0x638) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0640  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0648  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0654  meaning not yet known */
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
