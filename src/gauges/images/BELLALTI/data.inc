/* BELLALTI.GAU's .data image, decompiled: every non-zero value of retail's .data (0x678 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Bell_206B.Altimeter"),  /* the gauge name */
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
U8(0x53),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000007), U32(0x00000001), U32(0x00000000), U32(0x00000003), U8(0x01),
/* +0x0090  meaning not yet known */
AT(0x90) U8(0x07),
/* +0x00a8  element 1 of 5: type 1 */
AT(0xa8) U32(0x00000001),  /* element 1: type */
/* +0x00ac  meaning not yet known */
U32(0x00002000), U32(0x00000095), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000032),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00d8  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x4a),
/* +0x0110  meaning not yet known */
AT(0x110) U32(0x000000ac), U8(0x08),
/* +0x0188  the children of element 2, zero-terminated */
AT(0x188) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0190  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x0194  meaning not yet known */
U32(0x00003000), U32(0x00000095), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x0000000f),
U32(0x0000000c),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01c0  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x4a),
/* +0x01f8  meaning not yet known */
AT(0x1f8) U32(0x00000067), U8(0x0d),
/* +0x0270  the children of element 3, zero-terminated */
AT(0x270) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0278  element 3 of 5: type 1 */
U32(0x00000001),  /* element 3: type */
/* +0x027c  meaning not yet known */
U32(0x00004000), U32(0x00000095), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000043),
U32(0x0000003c),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02a8  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x4a),
/* +0x02e0  meaning not yet known */
AT(0x2e0) U32(0x000000ba), U8(0x5b),
/* +0x0358  the children of element 4, zero-terminated */
AT(0x358) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0360  element 4 of 5: type 6 */
U32(0x00000006),  /* element 4: type */
/* +0x0364  meaning not yet known */
U32(0x00005000), U32(0x000000df), U32(0x0000007f), U32(0x00000000), U32(0x00000000), U32(0x00000095),
U32(0x00000096),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x03ec  meaning not yet known */
AT(0x3ec) U8(0x4c),
/* +0x0412  meaning not yet known */
AT(0x412) U16(0x3ff0), U32(0x3f8ccccd), U32(0x3f8ccccd), U32(0x00000100), U8(0x7e),
/* +0x04a0  the children of element 5, zero-terminated */
AT(0x4a0) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x04a8  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x04ac  meaning not yet known */
U16(0x1000),
AT(0x4cc) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04d8  meaning not yet known */
U8(0x01),
/* +0x04f8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x4f8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x04fc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0514  meaning not yet known */
AT(0x514) U32(0xffffffff), U16(0xa612),
/* +0x052c  meaning not yet known */
AT(0x52c) U32(0x00000001), U32(0x00000006), U32(0x000000f1), U32(0x00000040), U32(0x00000040), U32(0xffffffff),
U16(0xa034),
/* +0x0558  meaning not yet known */
AT(0x558) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001015c), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000020), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001015b), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x05c4  meaning not yet known */
AT(0x5c4) U32(0xffffffff),
/* +0x05dc  meaning not yet known */
AT(0x5dc) U8(0x03),
/* +0x05f0  meaning not yet known */
AT(0x5f0) U32(0xffffffff),
/* +0x061c  meaning not yet known */
AT(0x61c) U32(0xffffffff),
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
#undef STR24
