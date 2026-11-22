/* CESSOIL.GAU's .data image, decompiled: every non-zero value of retail's .data (0x598 bytes at RVA 0x3000) at
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
AT(0x0) STR16("Cessna_182.Oil"),  /* the gauge name */
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
U8(0x39),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x00000001), U8(0x02),
/* +0x0078  element 1 of 5: type 0 */
AT(0x78) U32(0x00000000),  /* element 1: type */
/* +0x007c  meaning not yet known */
U32(0x00003000), U32(0x0000002d), U8(0x57),
AT(0x9c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00c8  the children of element 2, zero-terminated */
AT(0xc8) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x00d0  meaning not yet known */
U32(0x00000071), U32(0x00000050), U32(0x00000000), U32(0x406f4000), U32(0x00000000), U32(0x00000000),
U32(0x00000076), U32(0x00000052), U32(0x00000000), U32(0x406ea000), U32(0x00000000), U32(0x00000000),
U32(0x0000008a), U32(0x00000075), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x00000090), U32(0x000000a8), U32(0x00000000), U32(0x4062c000), U32(0x00000000), U32(0x00000000),
U32(0x0000007c), U32(0x000000cf), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x00000071), U32(0x000000d8), U32(0x00000000), U32(0x4052c000), U32(0x00000000), U32(0x00000000),
U32(0x00000068), U32(0x000000dd), U32(0x00000000), U32(0x40490000), U32(0x00000000), U32(0x00000000),
/* +0x0178  element 2 of 5: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x017c  meaning not yet known */
U32(0x00002000), U32(0x00000041), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01a8  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x93),
/* +0x01e0  meaning not yet known */
AT(0x1e0) U32(0x00000043), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01ec  meaning not yet known */
U32(0x00000007), U8(0x06),
/* +0x0258  element 3 of 5: type 0 */
AT(0x258) U32(0x00000000),  /* element 3: type */
/* +0x025c  meaning not yet known */
U32(0x00004000), U32(0x000000de), U8(0x4e),
AT(0x27c) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x02a8  the children of element 4, zero-terminated */
AT(0x2a8) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x02b0  meaning not yet known */
U32(0x000000ba), U8(0xda),
/* +0x02c8  meaning not yet known */
AT(0x2c8) U32(0x000000b8), U32(0x00000052), U32(0x00000000), U32(0x405cc000), U32(0x00000000), U32(0x00000000),
/* +0x02e0  element 4 of 5: type 1 */
U32(0x00000001),  /* element 4: type */
/* +0x02e4  meaning not yet known */
U32(0x00002000), U32(0x000000e6), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0310  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x94),
/* +0x0348  meaning not yet known */
AT(0x348) U32(0x00000043), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0354  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x03c0  the children of element 5, zero-terminated */
AT(0x3c0) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x03cc  meaning not yet known */
U32(0x00000000),
/* +0x03d0  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x03d4  meaning not yet known */
U16(0x1000),
AT(0x3f4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x03fc  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0420  the element-list slot (the header's +0x08 points here): the first element */
AT(0x420) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0424  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x043c  meaning not yet known */
AT(0x43c) U32(0xffffffff),
/* +0x0454  meaning not yet known */
AT(0x454) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000096), U32(0x0000012c), U32(0xffffffff),
U16(0xa022),
/* +0x0480  meaning not yet known */
AT(0x480) U8(0x03),
/* +0x0494  meaning not yet known */
AT(0x494) U32(0xffffffff),
/* +0x04ac  meaning not yet known */
AT(0x4ac) U32(0x00000001), U32(0x00000096), U32(0x00000000), U32(0x0000012c), U32(0x0000012c), U32(0xffffffff),
U16(0xa023),
/* +0x04d8  meaning not yet known */
AT(0x4d8) U8(0x03),
/* +0x04ec  meaning not yet known */
AT(0x4ec) U32(0xffffffff),
/* +0x0504  meaning not yet known */
AT(0x504) U8(0x03),
/* +0x0518  meaning not yet known */
AT(0x518) U32(0xffffffff),
/* +0x0544  meaning not yet known */
AT(0x544) U32(0xffffffff),
/* +0x0560  ImportTable: one import, PANELS (module id 0xf) */
AT(0x560) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0568  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0570  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x057c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x058c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR16
