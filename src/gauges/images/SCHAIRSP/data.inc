/* SCHAIRSP.GAU's .data image, decompiled: every non-zero value of retail's .data (0x4e8 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Schweizer Airspeed"),  /* the gauge name */
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
U8(0x59),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x0000007c), U8(0x15),
/* +0x0080  meaning not yet known */
AT(0x80) U32(0x0000009f), U32(0x0000001f), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x000000cb), U32(0x00000048), U32(0x00000000), U32(0x40440000), U32(0x00000000), U32(0x00000000),
U32(0x000000d1), U32(0x0000008e), U32(0x00000000), U32(0x404e0000), U32(0x00000000), U32(0x00000000),
U32(0x0000009e), U32(0x000000ca), U32(0x00000000), U32(0x40540000), U32(0x00000000), U32(0x00000000),
U32(0x0000005a), U32(0x000000c9), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x0000003d), U32(0x000000b8), U32(0x00000000), U32(0x405b8000), U32(0x00000000), U32(0x00000000),
U32(0x00000027), U32(0x00000099), U32(0x00000000), U32(0x405e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000025), U32(0x00000051), U32(0x00000000), U32(0x40618000), U32(0x00000000), U32(0x00000000),
U32(0x00000057), U32(0x0000001f), U32(0x00000000), U32(0x40640000), U32(0x00000000), U32(0x00000000),
U32(0x00000068), U32(0x00000018), U32(0x00000000), U32(0x4064a000), U32(0x00000000), U32(0x00000000),
U32(0x00000006), U32(0x00000001), U32(0x00000000), U32(0x00000003), U8(0x01),
/* +0x0198  element 1 of 3: type 1 */
AT(0x198) U32(0x00000001),  /* element 1: type */
/* +0x019c  meaning not yet known */
U32(0x00001100), U32(0x0000007c), U32(0x00000075), U32(0x00000000), U32(0x00000000), U32(0x00000027),
U32(0x00000006),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01c8  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x3d),
/* +0x0200  meaning not yet known */
AT(0x200) U32(0x0000007c), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x020c  meaning not yet known */
U32(0x0000000b), U8(0x05),
/* +0x0278  the children of element 2, zero-terminated */
AT(0x278) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0280  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0288  element 2 of 3: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 2: type */
/* +0x028c  meaning not yet known */
U32(0xffffffff), U32(0x00000064), U8(0x51),
AT(0x2ac) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02b8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x3d),
AT(0x340) U32(0x00000000),  /* element 2: text buffer */
/* +0x0344  meaning not yet known */
U32(0x00000000), U32(0x00000037), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x380) STR12("Courier New"),
/* +0x03a0  meaning not yet known */
AT(0x3a0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x03),
/* +0x03c0  the children of element 3, zero-terminated */
AT(0x3c0) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x03c8  element 3 of 3: type 0 */
U32(0x00000000),  /* element 3: type */
/* +0x03cc  meaning not yet known */
U16(0x1000),
AT(0x3ec) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03f8  meaning not yet known */
U8(0x01),
/* +0x0418  the element-list slot (the header's +0x08 points here): the first element */
AT(0x418) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x041c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0434  meaning not yet known */
AT(0x434) U32(0xffffffff), U16(0xa302),
/* +0x044c  meaning not yet known */
AT(0x44c) U8(0x03),
/* +0x0460  meaning not yet known */
AT(0x460) U32(0xffffffff),
/* +0x048c  meaning not yet known */
AT(0x48c) U32(0xffffffff),
AT(0x4a4) STR8("%03d"),
/* +0x04ac  meaning not yet known */
U32(0x00000000),
/* +0x04b0  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x04b8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x04c0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x04cc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x04dc  meaning not yet known */
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
