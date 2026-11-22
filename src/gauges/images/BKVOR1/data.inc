/* BKVOR1.GAU's .data image, decompiled: every non-zero value of retail's .data (0x728 bytes at RVA 0x3000) at
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
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Bendix_King.VOR1"),  /* the gauge name */
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
AT(0x68) U32(0x00000001), U32(0x00000002), U32(0x00000000), U32(0x0000000e), U8(0x02),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x000000d7), U32(0x0000005d), U32(0x00000000), U32(0xc0600000), U32(0x00000000), U32(0x00000000),
U32(0x000000dc), U8(0x97),
/* +0x00c0  meaning not yet known */
AT(0xc0) U32(0x000000d5), U32(0x000000cd), U32(0x00000000), U32(0x405fc000), U32(0x00000000), U32(0x00000000),
/* +0x00d8  element 1 of 6: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x00dc  meaning not yet known */
U32(0x00005000), U32(0x00000037), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0xfffffff6),
U32(0x00000003),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0108  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x29),
/* +0x0140  meaning not yet known */
AT(0x140) U32(0x0000008f), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x014c  meaning not yet known */
U32(0x00000003), U8(0x06),
/* +0x01b8  the children of element 2, zero-terminated */
AT(0x1b8) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x01c0  meaning not yet known */
U32(0x000000e8), U32(0x000000be), U32(0x00000000), U32(0x405fc000), U32(0x00000000), U32(0x00000000),
U32(0x00000093), U8(0xc8),
/* +0x01f0  meaning not yet known */
AT(0x1f0) U32(0x00000039), U32(0x000000be), U32(0x00000000), U32(0xc0600000), U32(0x00000000), U32(0x00000000),
/* +0x0208  element 2 of 6: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x020c  meaning not yet known */
U32(0x00005000), U32(0x00000094), U32(0x00000032), U32(0x00000000), U32(0x00000000), U32(0xffffffe7),
U32(0x00000003),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0238  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x28),
/* +0x0270  meaning not yet known */
AT(0x270) U32(0x0000008f), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x027c  meaning not yet known */
U32(0x00000003), U8(0x06),
/* +0x02e8  the children of element 3, zero-terminated */
AT(0x2e8) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x02f0  element 3 of 6: type 4 */
U32(0x00000004),  /* element 3: type */
/* +0x02f4  meaning not yet known */
U32(0x00004000), U32(0x000000be), U8(0x7c),
AT(0x314) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x032c  meaning not yet known */
AT(0x32c) U8(0x2c),
/* +0x0360  meaning not yet known */
AT(0x360) U8(0x02),
/* +0x0378  the children of element 4, zero-terminated */
AT(0x378) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0380  element 4 of 6: type 4 */
U32(0x00000004),  /* element 4: type */
/* +0x0384  meaning not yet known */
U32(0x00003000), U32(0x00000088), U8(0xc5),
AT(0x3a4) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x03bc  meaning not yet known */
AT(0x3bc) U8(0x2b),
/* +0x03f0  meaning not yet known */
AT(0x3f0) U8(0x03),
/* +0x0408  the children of element 5, zero-terminated */
AT(0x408) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0410  element 5 of 6: type 6 */
U32(0x00000006),  /* element 5: type */
/* +0x0414  meaning not yet known */
U32(0x00002000), U32(0x00000010), U32(0x00000010), U32(0x00000000), U32(0x00000000), U32(0x00000096),
U32(0x00000096),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x049c  meaning not yet known */
AT(0x49c) U8(0x2d),
/* +0x04c2  meaning not yet known */
AT(0x4c2) U16(0x3ff0),
STR12("33s\?33s\?4"),
/* +0x04d0  meaning not yet known */
U8(0x34),
/* +0x0550  the children of element 6, zero-terminated */
AT(0x550) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x0558  element 6 of 6: type 0 */
U32(0x00000000),  /* element 6: type */
/* +0x055c  meaning not yet known */
U16(0x1000),
AT(0x57c) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
/* +0x0584  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x05a8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x5a8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x05ac  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x05c4  meaning not yet known */
AT(0x5c4) U32(0xffffffff), U16(0xa01b),
/* +0x05dc  meaning not yet known */
AT(0x5dc) U32(0x00000001), U32(0x00000002), U32(0x000000e8), U32(0x00000040), U32(0x00000040), U32(0xffffffff),
U16(0xa01c),
/* +0x0608  meaning not yet known */
AT(0x608) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001007e), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000020), U32(0x00000000), U32(0x00000020), U32(0x00000040), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001007f), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0674  meaning not yet known */
AT(0x674) U32(0xffffffff),
/* +0x068c  meaning not yet known */
AT(0x68c) U8(0x03),
/* +0x06a0  meaning not yet known */
AT(0x6a0) U32(0xffffffff),
/* +0x06cc  meaning not yet known */
AT(0x6cc) U32(0xffffffff),
/* +0x06e8  ImportTable: one import, PANELS (module id 0xf) */
AT(0x6e8) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x06f0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x06f8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0704  meaning not yet known */
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
#undef STR12
#undef STR24
