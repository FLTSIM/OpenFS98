/* CESSOMI.GAU's .data image, decompiled: every non-zero value of retail's .data (0x328 bytes at RVA 0x3000) at
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
AT(0x0) STR16("Cessna_182.OMI"),  /* the gauge name */
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
U8(0x31),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x00000001), U8(0x03),
/* +0x0078  element 1 of 2: type 4 */
AT(0x78) U32(0x00000004),  /* element 1: type */
/* +0x007c  meaning not yet known */
U32(0x00002000), U32(0x00000006), U8(0x06),
AT(0x9c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x00a8  meaning not yet known */
U32(0x00000007), U32(0x00000000), U32(0x00000000), U16(0x0162),
/* +0x00e8  meaning not yet known */
AT(0xe8) U8(0x03),
/* +0x0100  the children of element 2, zero-terminated */
AT(0x100) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0108  element 2 of 2: type 0 */
U32(0x00000000),  /* element 2: type */
/* +0x010c  meaning not yet known */
U16(0x1000),
AT(0x12c) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x0134  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0158  the element-list slot (the header's +0x08 points here): the first element */
AT(0x158) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x015c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0174  meaning not yet known */
AT(0x174) U32(0xffffffff),
/* +0x018c  meaning not yet known */
AT(0x18c) U32(0x00000001), U32(0x00000005), U32(0x00000003), U32(0x0000001f), U32(0x00000025), U32(0xffffffff),
U16(0xa026),
/* +0x01b8  meaning not yet known */
AT(0x1b8) U8(0x03),
/* +0x01cc  meaning not yet known */
AT(0x1cc) U32(0xffffffff),
/* +0x01e4  meaning not yet known */
AT(0x1e4) U32(0x00000001), U32(0x00000024), U32(0x00000003), U32(0x0000001d), U32(0x00000025), U32(0xffffffff),
U16(0xa027),
/* +0x0210  meaning not yet known */
AT(0x210) U8(0x03),
/* +0x0224  meaning not yet known */
AT(0x224) U32(0xffffffff),
/* +0x023c  meaning not yet known */
AT(0x23c) U32(0x00000001), U32(0x00000041), U32(0x00000003), U32(0x0000001d), U32(0x00000025), U32(0xffffffff),
U16(0xa028),
/* +0x0268  meaning not yet known */
AT(0x268) U8(0x03),
/* +0x027c  meaning not yet known */
AT(0x27c) U32(0xffffffff),
/* +0x0294  meaning not yet known */
AT(0x294) U8(0x03),
/* +0x02a8  meaning not yet known */
AT(0x2a8) U32(0xffffffff),
/* +0x02d4  meaning not yet known */
AT(0x2d4) U32(0xffffffff),
/* +0x02f0  ImportTable: one import, PANELS (module id 0xf) */
AT(0x2f0) U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x02f8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0300  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x030c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x031c  meaning not yet known */
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
