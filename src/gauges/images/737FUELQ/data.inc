/* 737FUELQ.GAU's .data image, decompiled: every non-zero value of retail's .data (0x748 bytes at RVA 0x3000) at
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
AT(0x0) STR24("737-400.Fuel-Quantity"),  /* the gauge name */
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
U8(0x9b),
/* +0x0068  meaning not yet known */
AT(0x68) U16(0x0124),
/* +0x0088  meaning not yet known */
AT(0x88) U16(0x0126),
/* +0x00a8  meaning not yet known */
AT(0xa8) U16(0x012a),
/* +0x00c8  meaning not yet known */
AT(0xc8) U16(0x012c),
/* +0x00e8  meaning not yet known */
AT(0xe8) U16(0x0132),
/* +0x0108  meaning not yet known */
AT(0x108) U32(0x00000001), U32(0x00000003), U32(0x00000000), U32(0x0000000a), U8(0x01),
/* +0x0130  element 1 of 5: type 2 (text, 0xbc bytes) */
AT(0x130) U32(0x00000002),  /* element 1: type */
/* +0x0134  meaning not yet known */
U32(0xffffffff), U32(0x0000005f), U8(0x14),
AT(0x154) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0160  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x0129),
/* +0x0188  meaning not yet known */
AT(0x188) U16(0x012b),
AT(0x1e8) U32(0x00000000),  /* element 1: text buffer */
/* +0x01ec  meaning not yet known */
U32(0x00000000), U32(0x00000014), U32(0x00000016), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x228) STR12("Courier New"),
/* +0x0248  meaning not yet known */
AT(0x248) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x0268  element 2 of 5: type 2 (text, 0xbc bytes) */
AT(0x268) U32(0x00000002),  /* element 2: type */
/* +0x026c  meaning not yet known */
U32(0xffffffff), U32(0x00000071), U8(0x1e),
AT(0x28c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0298  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x0129),
/* +0x02c0  meaning not yet known */
AT(0x2c0) U16(0x012b),
AT(0x320) U32(0x00000000),  /* element 2: text buffer */
/* +0x0324  meaning not yet known */
U32(0x00000000), U32(0x0000000f), U32(0x0000000e), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x360) STR12("Courier New"),
/* +0x0380  meaning not yet known */
AT(0x380) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x01),
/* +0x03a0  element 3 of 5: type 2 (text, 0xbc bytes) */
AT(0x3a0) U32(0x00000002),  /* element 3: type */
/* +0x03a4  meaning not yet known */
U32(0xffffffff), U32(0x00000014), U8(0x14),
AT(0x3c4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03d0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x0123),
/* +0x03f8  meaning not yet known */
AT(0x3f8) U16(0x0125),
AT(0x458) U32(0x00000000),  /* element 3: text buffer */
/* +0x045c  meaning not yet known */
U32(0x00000000), U32(0x00000014), U32(0x00000016), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x498) STR12("Courier New"),
/* +0x04b8  meaning not yet known */
AT(0x4b8) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x02),
/* +0x04d8  element 4 of 5: type 2 (text, 0xbc bytes) */
AT(0x4d8) U32(0x00000002),  /* element 4: type */
/* +0x04dc  meaning not yet known */
U32(0xffffffff), U32(0x00000028), U8(0x1e),
AT(0x4fc) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0508  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x0123),
/* +0x0530  meaning not yet known */
AT(0x530) U16(0x0125),
AT(0x590) U32(0x00000000),  /* element 4: text buffer */
/* +0x0594  meaning not yet known */
U32(0x00000000), U32(0x0000000f), U32(0x0000000e), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x5d0) STR12("Courier New"),
/* +0x05f0  meaning not yet known */
AT(0x5f0) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x01),
/* +0x0610  the children of element 5, zero-terminated */
AT(0x610) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0624  meaning not yet known */
U32(0x00000000),
/* +0x0628  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x062c  meaning not yet known */
U16(0x1000),
AT(0x64c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x0654  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0678  the element-list slot (the header's +0x08 points here): the first element */
AT(0x678) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x067c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0694  meaning not yet known */
AT(0x694) U32(0xffffffff), U16(0xa415),
/* +0x06ac  meaning not yet known */
AT(0x6ac) U8(0x03),
/* +0x06c0  meaning not yet known */
AT(0x6c0) U32(0xffffffff),
/* +0x06ec  meaning not yet known */
AT(0x6ec) U32(0xffffffff),
/* +0x0704  meaning not yet known */
AT(0x704) U32(0x00006425),
/* +0x0708  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0710  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0718  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0724  meaning not yet known */
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
