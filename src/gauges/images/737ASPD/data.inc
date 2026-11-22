/* 737ASPD.GAU's .data image, decompiled: every non-zero value of retail's .data (0xbb8 bytes at RVA 0x3000) at
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
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("737-400.Airspeed"),  /* the gauge name */
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
U8(0xb5),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000006), U32(0x00000001), U32(0x00000000), U32(0x00000003), U8(0x01),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x00000006), U32(0x00000001), U32(0x00000000), U32(0x00000003), U8(0x01),
/* +0x00b8  meaning not yet known */
AT(0xb8) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000006), U32(0x00000001), U32(0x00000000),
U32(0x00000003), U8(0x01),
/* +0x00e8  meaning not yet known */
AT(0xe8) U8(0x06),
/* +0x0100  meaning not yet known */
AT(0x100) U32(0x0000005b), U32(0x0000000f), U32(0x00000000), U32(0x404e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000080), U32(0x0000001a), U32(0x00000000), U32(0x40540000), U32(0x00000000), U32(0x00000000),
U32(0x0000009b), U32(0x00000033), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x000000a3), U32(0x00000059), U32(0x00000000), U32(0x405e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000099), U32(0x0000007c), U32(0x00000000), U32(0x40618000), U32(0x00000000), U32(0x00000000),
U32(0x00000081), U32(0x00000098), U32(0x00000000), U32(0x40640000), U32(0x00000000), U32(0x00000000),
U32(0x0000006c), U32(0x000000a1), U32(0x00000000), U32(0x40668000), U32(0x00000000), U32(0x00000000),
U32(0x00000055), U32(0x000000a4), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x00000024), U32(0x00000089), U32(0x00000000), U32(0x406f4000), U32(0x00000000), U32(0x00000000),
U32(0x00000016), U32(0x0000006b), U32(0x00000000), U32(0x4072c000), U32(0x00000000), U32(0x00000000),
U32(0x00000015), U32(0x00000049), U32(0x00000000), U32(0x4075e000), U32(0x00000000), U32(0x00000000),
U32(0x00000024), U32(0x0000002d), U32(0x00000000), U32(0x40790000), U32(0x00000000), U32(0x00000000),
U32(0x0000003d), U32(0x00000019), U32(0x00000000), U32(0x407c2000), U32(0x00000000), U32(0x00000000),
U32(0x0000005b), U32(0x0000000f), U32(0x00000000), U32(0x404e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000080), U32(0x0000001a), U32(0x00000000), U32(0x40540000), U32(0x00000000), U32(0x00000000),
U32(0x0000009b), U32(0x00000033), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x000000a3), U32(0x00000059), U32(0x00000000), U32(0x405e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000099), U32(0x0000007c), U32(0x00000000), U32(0x40618000), U32(0x00000000), U32(0x00000000),
U32(0x00000081), U32(0x00000098), U32(0x00000000), U32(0x40640000), U32(0x00000000), U32(0x00000000),
U32(0x0000006c), U32(0x000000a1), U32(0x00000000), U32(0x40668000), U32(0x00000000), U32(0x00000000),
U32(0x00000055), U32(0x000000a4), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x00000024), U32(0x00000089), U32(0x00000000), U32(0x406f4000), U32(0x00000000), U32(0x00000000),
U32(0x00000016), U32(0x0000006b), U32(0x00000000), U32(0x4072c000), U32(0x00000000), U32(0x00000000),
U32(0x00000015), U32(0x00000049), U32(0x00000000), U32(0x4075e000), U32(0x00000000), U32(0x00000000),
U32(0x00000024), U32(0x0000002d), U32(0x00000000), U32(0x40790000), U32(0x00000000), U32(0x00000000),
U32(0x0000003d), U32(0x00000019), U32(0x00000000), U32(0x407c2000), U32(0x00000000), U32(0x00000000),
U32(0x0000005b), U32(0x0000000f), U32(0x00000000), U32(0x404e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000080), U32(0x0000001a), U32(0x00000000), U32(0x40540000), U32(0x00000000), U32(0x00000000),
U32(0x0000009b), U32(0x00000033), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x000000a3), U32(0x00000059), U32(0x00000000), U32(0x405e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000099), U32(0x0000007c), U32(0x00000000), U32(0x40618000), U32(0x00000000), U32(0x00000000),
U32(0x00000081), U32(0x00000098), U32(0x00000000), U32(0x40640000), U32(0x00000000), U32(0x00000000),
U32(0x0000006c), U32(0x000000a1), U32(0x00000000), U32(0x40668000), U32(0x00000000), U32(0x00000000),
U32(0x00000055), U32(0x000000a4), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x00000024), U32(0x00000089), U32(0x00000000), U32(0x406f4000), U32(0x00000000), U32(0x00000000),
U32(0x00000016), U32(0x0000006b), U32(0x00000000), U32(0x4072c000), U32(0x00000000), U32(0x00000000),
U32(0x00000015), U32(0x00000049), U32(0x00000000), U32(0x4075e000), U32(0x00000000), U32(0x00000000),
U32(0x00000024), U32(0x0000002d), U32(0x00000000), U32(0x40790000), U32(0x00000000), U32(0x00000000),
U32(0x0000003d), U32(0x00000019), U32(0x00000000), U32(0x407c2000), U32(0x00000000), U32(0x00000000),
/* +0x04a8  element 1 of 6: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x04ac  meaning not yet known */
U32(0x00004000), U32(0x0000005a), U32(0x00000059), U32(0x00000000), U32(0x00000000), U32(0xffffffbb),
U32(0x00000006),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x04d4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x74),
/* +0x0510  meaning not yet known */
AT(0x510) U32(0x0000001e), U32(0x0000001e),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x051c  meaning not yet known */
U32(0x0000000d), U8(0x06),
/* +0x0588  the children of element 2, zero-terminated */
AT(0x588) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0590  element 2 of 6: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x0594  meaning not yet known */
U32(0x00002000), U32(0x0000005a), U32(0x00000059), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05c0  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x3d),
/* +0x05f8  meaning not yet known */
AT(0x5f8) U32(0x0000005a), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0604  meaning not yet known */
U32(0x0000000d), U8(0x06),
/* +0x0670  the children of element 3, zero-terminated */
AT(0x670) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0678  element 3 of 6: type 1 */
U32(0x00000001),  /* element 3: type */
/* +0x067c  meaning not yet known */
U32(0x00003000), U32(0x0000005a), U32(0x00000059), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06a8  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x3f),
/* +0x06e0  meaning not yet known */
AT(0x6e0) U32(0x0000005a), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06ec  meaning not yet known */
U32(0x0000000d), U8(0x06),
/* +0x0758  the children of element 4, zero-terminated */
AT(0x758) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0760  element 4 of 6: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 4: type */
/* +0x0764  meaning not yet known */
U32(0xffffffff), U32(0x00000042), U8(0x6b),
AT(0x784) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0790  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x3d),
AT(0x818) U32(0x00000000),  /* element 4: text buffer */
/* +0x081c  meaning not yet known */
U32(0x00000000), U32(0x0000002f), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x858) STR12("Courier New"),
/* +0x0878  meaning not yet known */
AT(0x878) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x0898  the children of element 5, zero-terminated */
AT(0x898) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x08a0  element 5 of 6: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 5: type */
/* +0x08a4  meaning not yet known */
U32(0xffffffff), U32(0x0000004b), U8(0x36),
AT(0x8c4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x08d0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x40),
AT(0x958) U32(0x00000000),  /* element 5: text buffer */
/* +0x095c  meaning not yet known */
U32(0x00000000), U32(0x0000001f), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x998) STR12("Courier New"),
/* +0x09b8  meaning not yet known */
AT(0x9b8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x02),
/* +0x09d8  the children of element 6, zero-terminated */
AT(0x9d8) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x09e0  element 6 of 6: type 0 */
U32(0x00000000),  /* element 6: type */
/* +0x09e4  meaning not yet known */
U16(0x1000),
AT(0xa04) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a10  meaning not yet known */
U8(0x11),
/* +0x0a30  the element-list slot (the header's +0x08 points here): the first element */
AT(0xa30) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0a34  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0a4c  meaning not yet known */
AT(0xa4c) U32(0xffffffff), U16(0xa409),
/* +0x0a64  meaning not yet known */
AT(0xa64) U32(0x00000001), U32(0x00000008), U32(0x0000008f), U32(0x00000020), U32(0x00000020), U32(0xffffffff),
U16(0xa440),
/* +0x0a90  meaning not yet known */
AT(0xa90) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000010), U32(0x00000020), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010169), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000010), U32(0x00000000), U32(0x00000010), U32(0x00000020), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010168), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0afc  meaning not yet known */
AT(0xafc) U32(0xffffffff),
/* +0x0b14  meaning not yet known */
AT(0xb14) U8(0x03),
/* +0x0b28  meaning not yet known */
AT(0xb28) U32(0xffffffff),
/* +0x0b54  meaning not yet known */
AT(0xb54) U32(0xffffffff),
AT(0xb6c) STR8("%03d"),
STR8("%02d"),
/* +0x0b7c  meaning not yet known */
U32(0x00000000),
/* +0x0b80  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0b88  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0b90  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0b9c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x0bac  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR8
#undef STR12
#undef STR24
