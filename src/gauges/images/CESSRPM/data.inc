/* CESSRPM.GAU's .data image, decompiled: every non-zero value of retail's .data (0x348 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR16("Cessna_182.RMP"),  /* the gauge name */
/* +0x0010  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x003c  meaning not yet known */
U8(0x53),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x00000034), U8(0xd9),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x00000020), U32(0x000000a4), U32(0x00000000), U32(0x407f4000), U32(0x00000000), U32(0x00000000),
U32(0x0000002d), U32(0x0000005d), U32(0x00000000), U32(0x408f4000), U32(0x00000000), U32(0x00000000),
U32(0x0000006b), U32(0x00000028), U32(0x00000000), U32(0x40977000), U32(0x00000000), U32(0x00000000),
U32(0x000000ba), U32(0x00000025), U32(0x00000000), U32(0x409f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000fa), U32(0x00000056), U32(0x00000000), U32(0x40a38800), U32(0x00000000), U32(0x00000000),
U32(0x0000010e), U32(0x0000009d), U32(0x00000000), U32(0x40a77000), U32(0x00000000), U32(0x00000000),
U32(0x000000f3), U32(0x000000e2), U32(0x00000000), U32(0x40ab5800), U32(0x00000000), U32(0x00000000),
U8(0x9a),
/* +0x0140  element 1 of 2: type 1 */
AT(0x140) U32(0x00000001),  /* element 1: type */
/* +0x0144  meaning not yet known */
U32(0x00002000), U32(0x00000096), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x0000000a),
U32(0x0000000a),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x016c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x87),
/* +0x01a8  meaning not yet known */
AT(0x1a8) U32(0x00000081), U32(0x00000010),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01b4  meaning not yet known */
U32(0x00000008), U8(0x06),
/* +0x0220  the children of element 2, zero-terminated */
AT(0x220) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0228  element 2 of 2: type 0 */
U32(0x00000000),  /* element 2: type */
/* +0x022c  meaning not yet known */
U16(0x1000),
AT(0x24c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x0254  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0278  the element-list slot (the header's +0x08 points here): the first element */
AT(0x278) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x027c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0294  meaning not yet known */
AT(0x294) U32(0xffffffff), U16(0xa020),
/* +0x02ac  meaning not yet known */
AT(0x2ac) U8(0x03),
/* +0x02c0  meaning not yet known */
AT(0x2c0) U32(0xffffffff),
/* +0x02ec  meaning not yet known */
AT(0x2ec) U32(0xffffffff),
/* +0x0308  ImportTable: one import, PANELS (module id 0xf) */
AT(0x308) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0310  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0318  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0324  meaning not yet known */
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
#undef STR16
