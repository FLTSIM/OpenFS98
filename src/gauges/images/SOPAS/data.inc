/* SOPAS.GAU's .data image, decompiled: every non-zero value of retail's .data (0x3b8 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Sopwith Airspeed"),  /* the gauge name */
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
U8(0x70),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000006), U32(0x00000001), U32(0x00000000), U32(0x00000003), U8(0x01),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x0000008e), U8(0x12),
/* +0x00a8  meaning not yet known */
AT(0xa8) U32(0x000000a9), U32(0x00000018), U32(0x00000000), U32(0x40440000), U32(0x00000000), U32(0x00000000),
U32(0x000000d4), U32(0x00000031), U32(0x00000000), U32(0x404e0000), U32(0x00000000), U32(0x00000000),
U32(0x000000ef), U32(0x00000075), U32(0x00000000), U32(0x40540000), U32(0x00000000), U32(0x00000000),
U32(0x000000e1), U32(0x000000a6), U32(0x00000000), U32(0x40568000), U32(0x00000000), U32(0x00000000),
U32(0x000000bd), U32(0x000000c8), U32(0x00000000), U32(0x40590000), U32(0x00000000), U32(0x00000000),
U32(0x0000008f), U32(0x000000d5), U32(0x00000000), U32(0x405b8000), U32(0x00000000), U32(0x00000000),
U32(0x0000005f), U32(0x000000c8), U32(0x00000000), U32(0x405e0000), U32(0x00000000), U32(0x00000000),
U32(0x0000003b), U32(0x000000a6), U32(0x00000000), U32(0x40604000), U32(0x00000000), U32(0x00000000),
U32(0x0000002d), U32(0x00000073), U32(0x00000000), U32(0x40618000), U32(0x00000000), U32(0x00000000),
U32(0x0000003a), U32(0x00000044), U32(0x00000000), U32(0x4062c000), U32(0x00000000), U32(0x00000000),
U32(0x00000062), U32(0x00000028), U32(0x00000000), U32(0x40640000), U32(0x00000000), U32(0x00000000),
/* +0x01b0  element 1 of 2: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x01b4  meaning not yet known */
U32(0x00001100), U32(0x0000008c), U32(0x00000076), U32(0x00000000), U32(0x00000000), U32(0x0000000b),
U32(0x0000000b),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01e0  meaning not yet known */
U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x3d),
/* +0x0218  meaning not yet known */
AT(0x218) U32(0x0000008c), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0224  meaning not yet known */
U32(0x0000000c), U8(0x06),
/* +0x0290  the children of element 2, zero-terminated */
AT(0x290) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0298  element 2 of 2: type 0 */
U32(0x00000000),  /* element 2: type */
/* +0x029c  meaning not yet known */
U16(0x1000),
AT(0x2bc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x02c8  meaning not yet known */
U8(0x01),
/* +0x02e8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x2e8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x02ec  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0304  meaning not yet known */
AT(0x304) U32(0xffffffff), U16(0xa201),
/* +0x031c  meaning not yet known */
AT(0x31c) U8(0x03),
/* +0x0330  meaning not yet known */
AT(0x330) U32(0xffffffff),
/* +0x035c  meaning not yet known */
AT(0x35c) U32(0xffffffff),
/* +0x0378  ImportTable: one import, PANELS (module id 0xf) */
AT(0x378) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0380  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0388  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0394  meaning not yet known */
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
