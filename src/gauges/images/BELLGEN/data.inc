/* BELLGEN.GAU's .data image, decompiled: every non-zero value of retail's .data (0x508 bytes at RVA 0x3000) at
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
AT(0x0) STR32("Bell_206B.Generator_Fuel_PSI"),  /* the gauge name */
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
AT(0x70) U32(0x0000003f), U8(0xda),
/* +0x0088  meaning not yet known */
AT(0x88) U32(0x0000002c), U32(0x00000092), U32(0x00000000), U32(0x40140000), U32(0x00000000), U32(0x00000000),
U32(0x00000049), U32(0x0000004e), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x000000e7), U32(0x0000004e), U32(0x00000000), U32(0x40080000), U32(0x00000000), U32(0x00000000),
U32(0x000000fe), U32(0x00000078), U32(0x00000000), U32(0x40000000), U32(0x00000000), U32(0x00000000),
U32(0x00000101), U32(0x000000b1), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U32(0x00000000),
U32(0x000000eb), U8(0xe1),
/* +0x0118  element 1 of 4: type 0 */
AT(0x118) U32(0x00000000),  /* element 1: type */
/* +0x011c  meaning not yet known */
U32(0x00003000), U32(0x00000072), U8(0x77),
AT(0x13c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x0144  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0168  the children of element 2, zero-terminated */
AT(0x168) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0170  element 2 of 4: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x0174  meaning not yet known */
U32(0x00002000), U32(0x00000096), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0xffffffe0),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x019c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x9e),
/* +0x01d8  meaning not yet known */
AT(0x1d8) U32(0x0000003c), U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01e4  meaning not yet known */
U32(0x00000003), U8(0x06),
/* +0x0250  element 3 of 4: type 1 */
AT(0x250) U32(0x00000001),  /* element 3: type */
/* +0x0254  meaning not yet known */
U32(0x00002000), U32(0x00000096), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0xffffffe0),
U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x027c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x9d),
/* +0x02b8  meaning not yet known */
AT(0x2b8) U32(0x0000003c), U32(0x00000007),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02c4  meaning not yet known */
U32(0x00000004), U8(0x06),
/* +0x0330  the children of element 4, zero-terminated */
AT(0x330) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x033c  meaning not yet known */
U32(0x00000000),
/* +0x0340  element 4 of 4: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x0344  meaning not yet known */
U16(0x1000),
AT(0x364) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x036c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0390  the element-list slot (the header's +0x08 points here): the first element */
AT(0x390) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0394  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03ac  meaning not yet known */
AT(0x3ac) U32(0xffffffff),
/* +0x03c4  meaning not yet known */
AT(0x3c4) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000096), U32(0x0000012c), U32(0xffffffff),
U16(0xa605),
/* +0x03f0  meaning not yet known */
AT(0x3f0) U8(0x03),
/* +0x0404  meaning not yet known */
AT(0x404) U32(0xffffffff),
/* +0x041c  meaning not yet known */
AT(0x41c) U32(0x00000001), U32(0x00000096), U32(0x00000000), U32(0x00000096), U32(0x0000012c), U32(0xffffffff),
U16(0xa606),
/* +0x0448  meaning not yet known */
AT(0x448) U8(0x03),
/* +0x045c  meaning not yet known */
AT(0x45c) U32(0xffffffff),
/* +0x0474  meaning not yet known */
AT(0x474) U8(0x03),
/* +0x0488  meaning not yet known */
AT(0x488) U32(0xffffffff),
/* +0x04b4  meaning not yet known */
AT(0x4b4) U32(0xffffffff),
/* +0x04d0  ImportTable: one import, PANELS (module id 0xf) */
AT(0x4d0) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x04d8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x04e0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x04ec  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x04fc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR32
