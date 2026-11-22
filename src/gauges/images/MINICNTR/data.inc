/* MINICNTR.GAU's .data image, decompiled: every non-zero value of retail's .data (0x648 bytes at RVA 0x3000) at
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
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR16("Minicontrols"),  /* the gauge name */
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
U8(0x64),
/* +0x0060  element 1 of 8: type 2 (text, 0xbc bytes) */
AT(0x60) U32(0x00000002),  /* element 1: type */
/* +0x0064  meaning not yet known */
U32(0xffffffff), U32(0x0000000c), U8(0x7c),
AT(0x84) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x008c  meaning not yet known */
U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x3d),
AT(0x118) U32(0x00000000),  /* element 1: text buffer */
/* +0x011c  meaning not yet known */
U32(0x00000000), U32(0x00000034), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00fefefe), U16(0x5c5c), U8(0x5c),
AT(0x158) STR12("Courier New"),
/* +0x0178  meaning not yet known */
AT(0x178) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x04),
/* +0x0198  element 2 of 8: type 3 */
AT(0x198) U32(0x00000003),  /* element 2: type */
/* +0x019c  meaning not yet known */
U32(0x00002000), U32(0x00000030), U8(0x2f),
AT(0x1bc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x01c4  meaning not yet known */
U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0137),
/* +0x01fa  meaning not yet known */
AT(0x1fa) U16(0x3f64), U32(0x00000000), U16(0x0135),
/* +0x0220  meaning not yet known */
AT(0x220) U32(0x47ae147b), U32(0xbf647ae1),
/* +0x0240  element 3 of 8: type 3 */
AT(0x240) U32(0x00000003),  /* element 3: type */
/* +0x0244  meaning not yet known */
U32(0x00002000), U32(0x0000002f), U8(0x6c),
AT(0x264) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x026c  meaning not yet known */
U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x013b),
/* +0x029c  meaning not yet known */
AT(0x29c) U32(0x8d4fdf3b), U32(0x3f626e97),
/* +0x02e8  element 4 of 8: type 3 */
AT(0x2e8) U32(0x00000003),  /* element 4: type */
/* +0x02ec  meaning not yet known */
U32(0x00003000), U32(0x00000064), U8(0x6d),
AT(0x30c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x0314  meaning not yet known */
U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x010b),
/* +0x0350  meaning not yet known */
AT(0x350) U8(0x80),
/* +0x0375  meaning not yet known */
AT(0x375) U8(0x40), U16(0xbf79),
/* +0x0390  element 5 of 8: type 3 */
AT(0x390) U32(0x00000003),  /* element 5: type */
/* +0x0394  meaning not yet known */
U32(0x00003000), U32(0x0000006a), U8(0x6d),
AT(0x3b4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
/* +0x03bc  meaning not yet known */
U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x010b),
/* +0x03f8  meaning not yet known */
AT(0x3f8) U8(0xa3),
/* +0x041d  meaning not yet known */
AT(0x41d) U8(0x40), U16(0xbf79),
/* +0x0438  element 6 of 8: type 3 */
AT(0x438) U32(0x00000003),  /* element 6: type */
/* +0x043c  meaning not yet known */
U32(0x00003000), U32(0x00000070), U8(0x6d),
AT(0x45c) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
/* +0x0464  meaning not yet known */
U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x010b),
/* +0x04a0  meaning not yet known */
AT(0x4a0) U8(0xc6),
/* +0x04c5  meaning not yet known */
AT(0x4c5) U8(0x40), U16(0xbf79),
/* +0x04e0  element 7 of 8: type 3 */
AT(0x4e0) U32(0x00000003),  /* element 7: type */
/* +0x04e4  meaning not yet known */
U32(0x00003000), U32(0x00000076), U8(0x6d),
AT(0x504) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
/* +0x050c  meaning not yet known */
U32(0x00000000), U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x010b),
/* +0x0548  meaning not yet known */
AT(0x548) U8(0xe9),
/* +0x056d  meaning not yet known */
AT(0x56d) U8(0x40), U16(0xbf79),
/* +0x0588  the children of element 8, zero-terminated */
AT(0x588) U32(0x00000000),  /* element 8: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 8: end of the list */
/* +0x05a8  element 8 of 8: type 0 */
U32(0x00000000),  /* element 8: type */
/* +0x05ac  meaning not yet known */
U16(0x1000),
AT(0x5cc) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: children list (relocated: installed at attach, rebind.inc) */
/* +0x05d4  meaning not yet known */
U32(0x00000000), U8(0x05),
/* +0x05f8  the element-list slot (the header's +0x08 points here): the first element */
AT(0x5f8) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
STR8("%03d"),
/* +0x0604  meaning not yet known */
U32(0x00000000),
/* +0x0608  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0610  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0618  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0624  meaning not yet known */
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
#undef STR8
#undef STR12
#undef STR16
