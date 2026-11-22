/* BELLVSPD.GAU's .data image, decompiled: every non-zero value of retail's .data (0x408 bytes at RVA 0x3000) at
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
AT(0x0) STR32("Bell_206B.Vertical_Speed"),  /* the gauge name */
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
AT(0x70) U32(0x00000013), U32(0x00000001), U32(0x00000000), U32(0x00000003), U8(0x02),
/* +0x0098  meaning not yet known */
AT(0x98) U32(0x0000010d), U32(0x0000009c), U32(0x00000000), U32(0xc0b83800), U32(0x00000000), U32(0x00000000),
U32(0x00000109), U32(0x000000ae), U32(0x00000000), U32(0xc0b77000), U32(0x00000000), U32(0x00000000),
U32(0x00000103), U32(0x000000c4), U32(0x00000000), U32(0xc0b38800), U32(0x00000000), U32(0x00000000),
U32(0x000000f0), U32(0x000000e4), U32(0x00000000), U32(0xc0af4000), U32(0x00000000), U32(0x00000000),
U32(0x0000008b), U32(0x0000010e), U32(0x00000000), U32(0xc09f4000), U32(0x00000000), U32(0x00000000),
U32(0x00000043), U32(0x000000e8), U32(0x00000000), U32(0xc08f4000), U32(0x00000000), U32(0x00000000),
U32(0x0000002e), U32(0x000000c6), U32(0x00000000), U32(0xc07f4000), U32(0x00000000), U32(0x00000000),
U32(0x00000026), U8(0x9b),
/* +0x0158  meaning not yet known */
AT(0x158) U32(0x0000002d), U32(0x00000075), U32(0x00000000), U32(0x407f4000), U32(0x00000000), U32(0x00000000),
U32(0x00000042), U32(0x00000052), U32(0x00000000), U32(0x408f4000), U32(0x00000000), U32(0x00000000),
U32(0x0000008b), U32(0x0000002e), U32(0x00000000), U32(0x409f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000c6), U32(0x00000037), U32(0x00000000), U32(0x40a77000), U32(0x00000000), U32(0x00000000),
U32(0x000000f0), U32(0x00000055), U32(0x00000000), U32(0x40af4000), U32(0x00000000), U32(0x00000000),
U32(0x00000109), U32(0x0000008d), U32(0x00000000), U32(0x40b77000), U32(0x00000000), U32(0x00000000),
U32(0x0000010d), U32(0x0000009c), U32(0x00000000), U32(0x40b83800), U32(0x00000000), U32(0x00000000),
/* +0x0200  element 1 of 2: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x0204  meaning not yet known */
U32(0x00002000), U32(0x00000096), U32(0x0000009b), U32(0x00000000), U32(0x00000000), U32(0x0000002f),
U32(0x00000006),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0230  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x41),
/* +0x0268  meaning not yet known */
AT(0x268) U32(0x0000008a), U32(0x00000006),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0274  meaning not yet known */
U32(0x0000000f), U8(0x06),
/* +0x02e0  the children of element 2, zero-terminated */
AT(0x2e0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x02e8  element 2 of 2: type 0 */
U32(0x00000000),  /* element 2: type */
/* +0x02ec  meaning not yet known */
U16(0x1000),
AT(0x30c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0318  meaning not yet known */
U8(0x01),
/* +0x0338  the element-list slot (the header's +0x08 points here): the first element */
AT(0x338) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x033c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0354  meaning not yet known */
AT(0x354) U32(0xffffffff), U16(0xa614),
/* +0x036c  meaning not yet known */
AT(0x36c) U8(0x03),
/* +0x0380  meaning not yet known */
AT(0x380) U32(0xffffffff),
/* +0x03ac  meaning not yet known */
AT(0x3ac) U32(0xffffffff),
/* +0x03c8  ImportTable: one import, PANELS (module id 0xf) */
AT(0x3c8) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x03d0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x03d8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x03e4  meaning not yet known */
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
