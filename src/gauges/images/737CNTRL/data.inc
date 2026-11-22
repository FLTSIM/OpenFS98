/* 737CNTRL.GAU's .data image, decompiled: every non-zero value of retail's .data (0x618 bytes at RVA 0x3000) at
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
AT(0x0) STR24("737-400.Controls"),  /* the gauge name */
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
U8(0x71),
/* +0x0068  meaning not yet known */
AT(0x68) U8(0x02),
/* +0x0080  element 1 of 5: type 3 */
AT(0x80) U32(0x00000003),  /* element 1: type */
/* +0x0084  meaning not yet known */
U32(0x00002000), U32(0x00000000), U8(0x05),
AT(0xa4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00ac  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x00e8  meaning not yet known */
AT(0xe8) U16(0x0145),
/* +0x010e  meaning not yet known */
AT(0x10e) U16(0x3f84),
/* +0x0128  element 2 of 5: type 3 */
AT(0x128) U32(0x00000003),  /* element 2: type */
/* +0x012c  meaning not yet known */
U32(0x00003000), U32(0x0000002e), U8(0x8b),
AT(0x14c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0158  meaning not yet known */
U8(0x03),
/* +0x0190  meaning not yet known */
AT(0x190) U8(0x80),
/* +0x01b5  meaning not yet known */
AT(0x1b5) U8(0x80), U16(0xbf81),
/* +0x01d0  element 3 of 5: type 3 */
AT(0x1d0) U32(0x00000003),  /* element 3: type */
/* +0x01d4  meaning not yet known */
U32(0x00004000), U32(0x00000057), U8(0x8b),
AT(0x1f4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0200  meaning not yet known */
U8(0x03),
/* +0x0238  meaning not yet known */
AT(0x238) U8(0xa3),
/* +0x025d  meaning not yet known */
AT(0x25d) U8(0x80), U16(0xbf81),
/* +0x0278  element 4 of 5: type 4 */
AT(0x278) U32(0x00000004),  /* element 4: type */
/* +0x027c  meaning not yet known */
U32(0x00005000), U32(0x00000047), U8(0xb3),
AT(0x29c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x02a4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x7c),
/* +0x02e8  meaning not yet known */
AT(0x2e8) U8(0x01),
/* +0x0300  the children of element 5, zero-terminated */
AT(0x300) U32(0x00000000),  /* element 5: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 5: end of the list */
/* +0x0314  meaning not yet known */
U32(0x00000000),
/* +0x0318  element 5 of 5: type 0 */
U32(0x00000000),  /* element 5: type */
/* +0x031c  meaning not yet known */
U16(0x1000),
AT(0x33c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: children list (relocated: installed at attach, rebind.inc) */
/* +0x0344  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0368  the element-list slot (the header's +0x08 points here): the first element */
AT(0x368) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x036c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0384  meaning not yet known */
AT(0x384) U32(0xffffffff), U16(0xa427),
/* +0x039c  meaning not yet known */
AT(0x39c) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000028), U32(0x000000b3), U32(0xffffffff),
U16(0xa427),
/* +0x03c8  meaning not yet known */
AT(0x3c8) U32(0x00000002), U32(0x00000000), U32(0x00000005), U32(0x00000028), U32(0x000000b3), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03f0  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x0408  meaning not yet known */
AT(0x408) U32(0xffffffff),
/* +0x0420  meaning not yet known */
AT(0x420) U32(0x00000001), U32(0x0000002e), U32(0x00000000), U32(0x00000051), U32(0x000000b3), U32(0xffffffff),
U16(0xa428),
/* +0x044c  meaning not yet known */
AT(0x44c) U32(0x00000002), U32(0x00000000), U32(0x00000008), U32(0x00000019), U32(0x000000b3), U32(0x00000005),
U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0474  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x0000001a), U32(0x00000008), U32(0x0000001c), U32(0x000000b3),
U32(0x00000005), U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04a0  meaning not yet known */
U32(0x00000000), U32(0x00000002), U32(0x00000037), U32(0x00000008), U32(0x00000019), U32(0x000000b3),
U32(0x00000005), U32(0x00000000), U32(0x20800000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x04cc  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x04e4  meaning not yet known */
AT(0x4e4) U32(0xffffffff),
/* +0x04fc  meaning not yet known */
AT(0x4fc) U32(0x00000001), U32(0x0000004c), U32(0x000000b5), U32(0x0000001a), U32(0x00000018), U32(0xffffffff),
U16(0xa444),
/* +0x0528  meaning not yet known */
AT(0x528) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001a), U32(0x00000018), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010145), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0568  meaning not yet known */
AT(0x568) U32(0xffffffff),
/* +0x0580  meaning not yet known */
AT(0x580) U8(0x03),
/* +0x0594  meaning not yet known */
AT(0x594) U32(0xffffffff),
/* +0x05c0  meaning not yet known */
AT(0x5c0) U32(0xffffffff),
/* +0x05d8  ImportTable: one import, PANELS (module id 0xf) */
AT(0x5d8) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x05e0  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x05e8  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x05f4  meaning not yet known */
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
