/* EXSWITCH.GAU's .data image, decompiled: every non-zero value of retail's .data (0x614 bytes at RVA 0x3000) at
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
AT(0x0) STR24("Extra-300.Switches"),  /* the gauge name */
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
U8(0xf2),
/* +0x0068  element 1 of 6: type 0 */
AT(0x68) U32(0x00000000),  /* element 1: type */
/* +0x006c  meaning not yet known */
U32(0x00002000), U32(0x0000028f), U32(0xffffff29),
AT(0x8c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x0094  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x00b8  element 2 of 6: type 4 */
AT(0xb8) U32(0x00000004),  /* element 2: type */
/* +0x00bc  meaning not yet known */
U16(0x3000),
AT(0xdc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
/* +0x00e4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x67),
/* +0x0128  meaning not yet known */
AT(0x128) U8(0x02),
/* +0x0140  element 3 of 6: type 4 */
AT(0x140) U32(0x00000004),  /* element 3: type */
/* +0x0144  meaning not yet known */
U32(0x00003000), U8(0xbd),
AT(0x164) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
/* +0x016c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x0a),
/* +0x01b0  meaning not yet known */
AT(0x1b0) U8(0x02),
/* +0x01c8  element 4 of 6: type 4 */
AT(0x1c8) U32(0x00000004),  /* element 4: type */
/* +0x01cc  meaning not yet known */
U32(0x00003000), U8(0xec),
AT(0x1ec) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x01f4  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x0b),
/* +0x0238  meaning not yet known */
AT(0x238) U8(0x02),
/* +0x0250  element 5 of 6: type 4 */
AT(0x250) U32(0x00000004),  /* element 5: type */
/* +0x0254  meaning not yet known */
U32(0x00003000), U16(0x01a9),
AT(0x274) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
/* +0x027c  meaning not yet known */
U32(0x00000000), U32(0x00000003), U32(0x00000000), U32(0x00000000), U8(0x0f),
/* +0x02c0  meaning not yet known */
AT(0x2c0) U8(0x02),
/* +0x02d8  the children of element 6, zero-terminated */
AT(0x2d8) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x02f0  element 6 of 6: type 0 */
U32(0x00000000),  /* element 6: type */
/* +0x02f4  meaning not yet known */
U16(0x1000),
AT(0x314) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
/* +0x031c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0340  the element-list slot (the header's +0x08 points here): the first element */
AT(0x340) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0344  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x035c  meaning not yet known */
AT(0x35c) U32(0xffffffff),
/* +0x0374  meaning not yet known */
AT(0x374) U32(0x00000001), U32(0x0000011c), U32(0x00000000), U32(0x00000028), U32(0x0000003b), U32(0xffffffff),
U16(0xa70b),
/* +0x03a0  meaning not yet known */
AT(0x3a0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000028), U32(0x0000003b), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x03c8  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x03e0  meaning not yet known */
AT(0x3e0) U32(0xffffffff),
/* +0x03f8  meaning not yet known */
AT(0x3f8) U32(0x00000001), U32(0x000000bd), U32(0x00000000), U32(0x00000025), U32(0x0000003b), U32(0xffffffff),
U16(0xa510),
/* +0x0424  meaning not yet known */
AT(0x424) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000025), U32(0x0000003b), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x000100d6), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0464  meaning not yet known */
AT(0x464) U32(0xffffffff),
/* +0x047c  meaning not yet known */
AT(0x47c) U32(0x00000001), U32(0x000000ec), U32(0x00000000), U32(0x00000025), U32(0x0000003b), U32(0xffffffff),
U16(0xa511),
/* +0x04a8  meaning not yet known */
AT(0x4a8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000025), U32(0x0000003b), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010018), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x04e8  meaning not yet known */
AT(0x4e8) U32(0xffffffff),
/* +0x0500  meaning not yet known */
AT(0x500) U32(0x00000001), U32(0x000001a9), U32(0x00000000), U32(0x00000025), U32(0x0000003b), U32(0xffffffff),
U16(0xa513),
/* +0x052c  meaning not yet known */
AT(0x52c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000025), U32(0x0000003b), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010017), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x056c  meaning not yet known */
AT(0x56c) U32(0xffffffff),
/* +0x0584  meaning not yet known */
AT(0x584) U8(0x03),
/* +0x0598  meaning not yet known */
AT(0x598) U32(0xffffffff),
/* +0x05c4  meaning not yet known */
AT(0x5c4) U32(0xffffffff),
/* +0x05e0  ImportTable: one import, PANELS (module id 0xf) */
AT(0x5e0) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x05e8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x05f0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x05fc  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x060c  meaning not yet known */
U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR24
