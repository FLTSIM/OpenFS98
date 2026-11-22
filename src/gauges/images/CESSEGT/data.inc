/* CESSEGT.GAU's .data image, decompiled: every non-zero value of retail's .data (0x6a8 bytes at RVA 0x3000) at
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
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Cessna_182.EGT_CHT"),  /* the gauge name */
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
U8(0x39),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x02),
/* +0x0080  element 1 of 6: type 0 */
AT(0x80) U32(0x00000000),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0x00005000), U32(0x000000de), U8(0x59),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00ac  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x00d0  the children of element 2, zero-terminated */
AT(0xd0) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x00d8  meaning not yet known */
U32(0x000000ce), U8(0xe3),
/* +0x00f0  meaning not yet known */
AT(0xf0) U32(0x000000bb), U32(0x000000da), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
U32(0x000000bc), U32(0x00000055), U32(0x00000000), U32(0x407f4000), U32(0x00000000), U32(0x00000000),
U32(0x000000ca), U32(0x0000004c), U32(0x00000000), U32(0x40813000), U32(0x00000000), U32(0x00000000),
/* +0x0138  element 2 of 6: type 1 */
U32(0x00000001),  /* element 2: type */
/* +0x013c  meaning not yet known */
U32(0x00003000), U32(0x000000e2), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0168  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x92),
/* +0x01a0  meaning not yet known */
AT(0x1a0) U32(0x00000043), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01ac  meaning not yet known */
U32(0x00000004), U8(0x06),
/* +0x0218  meaning not yet known */
AT(0x218) U32(0x00000060), U32(0x0000004a), U32(0x00000000), U32(0x40d00000), U32(0x00000000), U32(0x00000000),
U32(0x0000005f), U8(0xdf),
/* +0x0248  element 3 of 6: type 1 */
AT(0x248) U32(0x00000001),  /* element 3: type */
/* +0x024c  meaning not yet known */
U32(0x00002000), U32(0x00000046), U32(0x00000098), U32(0x00000000), U32(0x00000000), U32(0x00000011),
U32(0x00000010),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x0274  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x91),
/* +0x02b0  meaning not yet known */
AT(0x2b0) U32(0x00000043), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02bc  meaning not yet known */
U32(0x00000002), U8(0x06),
/* +0x0328  the children of element 4, zero-terminated */
AT(0x328) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0330  element 4 of 6: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x0334  meaning not yet known */
U32(0x00004000), U32(0x00000029), U8(0x60),
AT(0x354) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x035c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0380  the children of element 5, zero-terminated */
AT(0x380) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0388  meaning not yet known */
U32(0x00000060), U32(0x0000004a), U32(0x00000000), U32(0x4085e000), U32(0x00000000), U32(0x00000000),
U32(0x0000006d), U32(0x00000056), U32(0x00000000), U32(0x40845000), U32(0x00000000), U32(0x00000000),
U32(0x0000006d), U32(0x000000d9), U32(0x00000000), U32(0x406f4000), U32(0x00000000), U32(0x00000000),
U32(0x0000005c), U32(0x000000e0), U32(0x00000000), U32(0x40690000), U32(0x00000000), U32(0x00000000),
/* +0x03e8  element 5 of 6: type 1 */
U32(0x00000001),  /* element 5: type */
/* +0x03ec  meaning not yet known */
U32(0x00003000), U32(0x00000046), U32(0x00000096), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0418  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x90),
/* +0x0450  meaning not yet known */
AT(0x450) U32(0x00000043), U32(0x00000008),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x045c  meaning not yet known */
U32(0x00000004), U8(0x06),
/* +0x04c8  the children of element 6, zero-terminated */
AT(0x4c8) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x04d4  meaning not yet known */
U32(0x00000000),
/* +0x04d8  element 6 of 6: type 0 */
U32(0x00000000),  /* element 6: type */
/* +0x04dc  meaning not yet known */
U16(0x1000),
AT(0x4fc) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
/* +0x0504  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0528  the element-list slot (the header's +0x08 points here): the first element */
AT(0x528) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x052c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0544  meaning not yet known */
AT(0x544) U32(0xffffffff),
/* +0x055c  meaning not yet known */
AT(0x55c) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000096), U32(0x0000012b), U32(0xffffffff),
U16(0xa018),
/* +0x0588  meaning not yet known */
AT(0x588) U8(0x03),
/* +0x059c  meaning not yet known */
AT(0x59c) U32(0xffffffff),
/* +0x05b4  meaning not yet known */
AT(0x5b4) U32(0x00000001), U32(0x00000096), U32(0x00000000), U32(0x00000096), U32(0x0000012b), U32(0xffffffff),
U16(0xa032),
/* +0x05e0  meaning not yet known */
AT(0x5e0) U8(0x03),
/* +0x05f4  meaning not yet known */
AT(0x5f4) U32(0xffffffff),
/* +0x060c  meaning not yet known */
AT(0x60c) U8(0x03),
/* +0x0620  meaning not yet known */
AT(0x620) U32(0xffffffff),
/* +0x064c  meaning not yet known */
AT(0x64c) U32(0xffffffff),
/* +0x0668  ImportTable: one import, PANELS (module id 0xf) */
AT(0x668) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0670  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0678  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0684  meaning not yet known */
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
#undef STR24
