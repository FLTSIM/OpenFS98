/* EXCSURF.GAU's .data image, decompiled: every non-zero value of retail's .data (0x59c bytes at RVA 0x3000) at
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
AT(0x0) STR32("Extra-300.Control_Surfaces"),  /* the gauge name */
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
U8(0x30),
/* +0x0070  element 1 of 5: type 3 */
AT(0x70) U32(0x00000003),  /* element 1: type */
/* +0x0074  meaning not yet known */
U32(0x00001100), U32(0x00000003), U8(0x27),
AT(0x94) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x009c  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x00d8  meaning not yet known */
AT(0xd8) U16(0x013e),
/* +0x00fd  meaning not yet known */
AT(0xfd) U8(0xc0), U16(0xbf62),
/* +0x0118  element 2 of 5: type 3 */
AT(0x118) U32(0x00000003),  /* element 2: type */
/* +0x011c  meaning not yet known */
U32(0x00001200), U8(0x3b),
AT(0x13c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x0144  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x0138),
/* +0x0179  meaning not yet known */
AT(0x179) U8(0x40), U16(0x3f62),
/* +0x01c0  element 3 of 5: type 3 */
AT(0x1c0) U32(0x00000003),  /* element 3: type */
/* +0x01c4  meaning not yet known */
U32(0x00001300), U32(0x00000042), U8(0x27),
AT(0x1e4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x01ec  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0228  meaning not yet known */
AT(0x228) U16(0x0136),
/* +0x024e  meaning not yet known */
AT(0x24e) U16(0xbf5e),
/* +0x0268  element 4 of 5: type 3 */
AT(0x268) U32(0x00000003),  /* element 4: type */
/* +0x026c  meaning not yet known */
U32(0x00001400), U32(0x0000003c), U8(0x4f),
AT(0x28c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x0294  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U16(0x013b),
/* +0x02ca  meaning not yet known */
AT(0x2ca) U16(0x3f62),
/* +0x0310  the children of element 5, zero-terminated */
AT(0x310) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0324  meaning not yet known */
U32(0x00000000),
/* +0x0328  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x032c  meaning not yet known */
U16(0x1000),
AT(0x34c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x0354  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0378  the element-list slot (the header's +0x08 points here): the first element */
AT(0x378) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x037c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0394  meaning not yet known */
AT(0x394) U32(0xffffffff),
/* +0x03ac  meaning not yet known */
AT(0x3ac) U32(0x00000001), U32(0x00000000), U32(0x00000002), U32(0x00000017), U32(0x00000051), U32(0xffffffff),
U16(0xa02e),
/* +0x03d8  meaning not yet known */
AT(0x3d8) U8(0x03),
/* +0x03ec  meaning not yet known */
AT(0x3ec) U32(0xffffffff),
/* +0x0404  meaning not yet known */
AT(0x404) U32(0x00000001), U32(0x00000019), U32(0x00000000), U32(0x0000004f), U32(0x0000000d), U32(0xffffffff),
U16(0xa12c),
/* +0x0430  meaning not yet known */
AT(0x430) U8(0x03),
/* +0x0444  meaning not yet known */
AT(0x444) U32(0xffffffff),
/* +0x045c  meaning not yet known */
AT(0x45c) U32(0x00000001), U32(0x00000035), U32(0x0000000d), U32(0x00000018), U32(0x0000003e), U32(0xffffffff),
U16(0xa12d),
/* +0x0488  meaning not yet known */
AT(0x488) U8(0x03),
/* +0x049c  meaning not yet known */
AT(0x49c) U32(0xffffffff),
/* +0x04b4  meaning not yet known */
AT(0x4b4) U32(0x00000001), U32(0x00000019), U32(0x0000004b), U32(0x0000004f), U32(0x0000000c), U32(0xffffffff),
U16(0xa12e),
/* +0x04e0  meaning not yet known */
AT(0x4e0) U8(0x03),
/* +0x04f4  meaning not yet known */
AT(0x4f4) U32(0xffffffff),
/* +0x050c  meaning not yet known */
AT(0x50c) U8(0x03),
/* +0x0520  meaning not yet known */
AT(0x520) U32(0xffffffff),
/* +0x054c  meaning not yet known */
AT(0x54c) U32(0xffffffff),
/* +0x0568  ImportTable: one import, PANELS (module id 0xf) */
AT(0x568) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0570  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0578  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0584  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x0594  meaning not yet known */
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
