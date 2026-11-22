/* EXMPFF.GAU's .data image, decompiled: every non-zero value of retail's .data (0x568 bytes at RVA 0x3000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR40(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR40("Extra-300.Manifold-Pressure-Fuel-Flow"),  /* the gauge name */
/* +0x0028  the gauge header (Linkage +0x18), 0x2c bytes */
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
/* +0x0054  meaning not yet known */
U8(0x53),
/* +0x0078  meaning not yet known */
AT(0x78) U32(0x0000010b), U8(0xa4),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x0000010a), U32(0x000000ac), U32(0x00000000), U32(0x40140000), U32(0x00000000), U32(0x00000000),
U32(0x00000103), U32(0x000000c6), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x000000e4), U32(0x000000ee), U32(0x00000000), U32(0x402e0000), U32(0x00000000), U32(0x00000000),
U32(0x000000ae), U32(0x00000108), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x0000005c), U32(0x000000fd), U32(0x00000000), U32(0x40390000), U32(0x00000000), U32(0x00000000),
U32(0x00000021), U32(0x000000a6), U32(0x00000000), U32(0x403e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000038), U32(0x00000070), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x00000000),
U32(0x00000058), U32(0x0000004f), U32(0x00000000), U32(0x402e0000), U32(0x00000000), U32(0x00000000),
U32(0x00000081), U32(0x0000003f), U32(0x00000000), U32(0x40340000), U32(0x00000000), U32(0x00000000),
U32(0x000000aa), U32(0x00000041), U32(0x00000000), U32(0x40390000), U32(0x00000000), U32(0x00000000),
U32(0x000000d2), U32(0x0000004f), U32(0x00000000), U32(0x403e0000), U32(0x00000000), U32(0x00000000),
U32(0x000000f2), U32(0x00000070), U32(0x00000000), U32(0x40418000), U32(0x00000000), U32(0x00000000),
U32(0x00000104), U32(0x00000085), U32(0x00000000), U32(0x40438000), U32(0x00000000), U32(0x00000000),
/* +0x01c8  element 1 of 3: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x01cc  meaning not yet known */
U32(0x00002000), U32(0x00000096), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x0000003a),
U32(0x00000009),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x01f4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x89),
/* +0x0230  meaning not yet known */
AT(0x230) U32(0x000000b0), U32(0x00000009),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x023c  meaning not yet known */
U32(0x00000007), U8(0x06),
/* +0x02a8  the children of element 2, zero-terminated */
AT(0x2a8) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x02b0  element 2 of 3: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x02b4  meaning not yet known */
U32(0x00002001), U32(0x00000096), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000028),
U32(0x00000009),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x02dc  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x98),
/* +0x0318  meaning not yet known */
AT(0x318) U32(0x00000084), U32(0x00000009),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0324  meaning not yet known */
U32(0x00000007), U8(0x06),
/* +0x0390  the children of element 3, zero-terminated */
AT(0x390) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x0398  element 3 of 3: type 0 */
U32(0x00000000),  /* element 3: type */
/* +0x039c  meaning not yet known */
U16(0x1000),
AT(0x3bc) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x03c4  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x03e8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x3e8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x03ec  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0404  meaning not yet known */
AT(0x404) U32(0xffffffff),
/* +0x041c  meaning not yet known */
AT(0x41c) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x0000012c), U32(0x00000096), U32(0xffffffff),
U16(0xa524),
/* +0x0448  meaning not yet known */
AT(0x448) U8(0x03),
/* +0x045c  meaning not yet known */
AT(0x45c) U32(0xffffffff),
/* +0x0474  meaning not yet known */
AT(0x474) U32(0x00000001), U32(0x00000000), U32(0x00000096), U32(0x0000012c), U32(0x00000096), U32(0xffffffff),
U16(0xa516),
/* +0x04a0  meaning not yet known */
AT(0x4a0) U8(0x03),
/* +0x04b4  meaning not yet known */
AT(0x4b4) U32(0xffffffff),
/* +0x04cc  meaning not yet known */
AT(0x4cc) U8(0x03),
/* +0x04e0  meaning not yet known */
AT(0x4e0) U32(0xffffffff),
/* +0x050c  meaning not yet known */
AT(0x50c) U32(0xffffffff),
/* +0x0528  ImportTable: one import, PANELS (module id 0xf) */
AT(0x528) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0530  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0538  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0544  meaning not yet known */
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
#undef STR40
