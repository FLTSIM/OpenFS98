/* 737ALTIM.GAU's .data image, decompiled: every non-zero value of retail's .data (0x7c8 bytes at RVA 0x3000) at
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
AT(0x0) STR24("737-400.Altimeter"),  /* the gauge name */
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
U8(0x93),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x00000007), U32(0x00000001), U32(0x00000000),
U32(0x00000003), U8(0x01),
/* +0x0098  meaning not yet known */
AT(0x98) U32(0x00000001), U8(0x03),
/* +0x00b0  meaning not yet known */
AT(0xb0) U32(0x00000007), U32(0x00000001), U32(0x00000000), U32(0x00000003), U8(0x01),
/* +0x00d8  meaning not yet known */
AT(0xd8) U8(0x07),
/* +0x00f0  element 1 of 5: type 1 */
AT(0xf0) U32(0x00000001),  /* element 1: type */
/* +0x00f4  meaning not yet known */
U32(0x00003000), U32(0x0000004a), U32(0x0000004a), U32(0x00000000), U32(0x00000000), U32(0xffffffd0),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x011c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x6d),
/* +0x0158  meaning not yet known */
AT(0x158) U32(0x0000001b), U32(0x0000001b), U32(0x00000000), U32(0x00000000), U8(0x0a),
/* +0x01d0  the children of element 2, zero-terminated */
AT(0x1d0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x01d8  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x01dc  meaning not yet known */
U32(0x00002000), U32(0x0000004a), U32(0x0000004a), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0208  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x4a),
/* +0x0240  meaning not yet known */
AT(0x240) U32(0x0000004a), U32(0x00000000), U32(0x00000000), U32(0x00000000), U8(0x0a),
/* +0x02b8  the children of element 3, zero-terminated */
AT(0x2b8) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x02c0  element 3 of 5: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 3: type */
/* +0x02c4  meaning not yet known */
U32(0xffffffff), U32(0x00000027), U8(0x2b),
AT(0x2e4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02f0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4a),
AT(0x378) U32(0x00000000),  /* element 3: text buffer */
/* +0x037c  meaning not yet known */
U32(0x00000000), U32(0x00000045), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x3b8) STR12("Courier New"),
/* +0x03d8  meaning not yet known */
AT(0x3d8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x03f8  the children of element 4, zero-terminated */
AT(0x3f8) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0400  element 4 of 5: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 4: type */
/* +0x0404  meaning not yet known */
U32(0xffffffff), U32(0x00000029), U8(0x50),
AT(0x424) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0430  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x4c),
AT(0x4b8) U32(0x00000000),  /* element 4: text buffer */
/* +0x04bc  meaning not yet known */
U32(0x00000000), U32(0x00000042), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x4f8) STR12("Courier New"),
/* +0x0518  meaning not yet known */
AT(0x518) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x05),
/* +0x0538  the children of element 5, zero-terminated */
AT(0x538) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0540  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x0544  meaning not yet known */
U16(0x1000),
AT(0x564) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0570  meaning not yet known */
U8(0x11),
/* +0x0590  the element-list slot (the header's +0x08 points here): the first element */
AT(0x590) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0594  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x05ac  meaning not yet known */
AT(0x5ac) U32(0xffffffff), U16(0xa400),
/* +0x05c4  meaning not yet known */
AT(0x5c4) U32(0x00000001), U32(0x00000008), U32(0x00000070), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
U16(0xa401),
/* +0x05f0  meaning not yet known */
AT(0x5f0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000000e), U32(0x0000001c), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001015c), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000000e), U32(0x00000000), U32(0x0000000e), U32(0x0000001c), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001015b), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x065c  meaning not yet known */
AT(0x65c) U32(0xffffffff),
/* +0x0674  meaning not yet known */
AT(0x674) U32(0x00000001), U32(0x00000070), U32(0x0000006e), U32(0x00000020), U32(0x00000020), U32(0xffffffff),
U16(0xa430),
/* +0x06a0  meaning not yet known */
AT(0x6a0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000010), U32(0x00000020), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010165), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000010), U32(0x00000000), U32(0x00000010), U32(0x00000020), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010164), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x070c  meaning not yet known */
AT(0x70c) U32(0xffffffff),
/* +0x0724  meaning not yet known */
AT(0x724) U8(0x03),
/* +0x0738  meaning not yet known */
AT(0x738) U32(0xffffffff),
/* +0x0764  meaning not yet known */
AT(0x764) U32(0xffffffff),
AT(0x77c) STR8("%05d"),
STR12("%02d.%02d"),
/* +0x0790  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0798  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x07a0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x07ac  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x07bc  meaning not yet known */
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
