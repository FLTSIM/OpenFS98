/* BKRNC1.GAU's .data image, decompiled: every non-zero value of retail's .data (0x828 bytes at RVA 0x3000) at
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
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR32(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR32("Bendix/King Radio Nav/Comm 1"),  /* the gauge name */
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
U8(0xaa),
/* +0x0070  meaning not yet known */
AT(0x70) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x0e),
/* +0x0098  meaning not yet known */
AT(0x98) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x09),
/* +0x00c0  element 1 of 4: type 4 */
AT(0xc0) U32(0x00000004),  /* element 1: type */
/* +0x00c4  meaning not yet known */
U32(0x00002000), U32(0x0000012a), U8(0x76),
AT(0xe4) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00fc  meaning not yet known */
AT(0xfc) U8(0x1a),
/* +0x0130  meaning not yet known */
AT(0x130) U8(0x02),
/* +0x0148  meaning not yet known */
AT(0x148) U32(0x00000004), U32(0x00000000), U32(0x00000002), U32(0x00000005), U32(0x00000004), U8(0x05),
/* +0x0170  element 2 of 4: type 2 (text, 0xbc bytes) */
AT(0x170) U32(0x00000002),  /* element 2: type */
/* +0x0174  meaning not yet known */
U32(0xffffffff), U32(0x00000010), U8(0x16),
AT(0x194) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01a0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x17),
AT(0x228) U32(0x00000000),  /* element 2: text buffer */
/* +0x022c  meaning not yet known */
U32(0x00000000), U32(0x000000e1), U32(0x00000032), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x268) STR12("Courier New"),
/* +0x0288  meaning not yet known */
AT(0x288) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x06),
/* +0x02a8  meaning not yet known */
AT(0x2a8) U32(0x00000006), U32(0x00000000), U32(0x00000002), U32(0x00000007), U32(0x00000004), U8(0x05),
/* +0x02d0  element 3 of 4: type 2 (text, 0xbc bytes) */
AT(0x2d0) U32(0x00000002),  /* element 3: type */
/* +0x02d4  meaning not yet known */
U32(0xffffffff), U32(0x00000104), U8(0x16),
AT(0x2f4) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0300  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x18),
AT(0x388) U32(0x00000000),  /* element 3: text buffer */
/* +0x038c  meaning not yet known */
U32(0x00000000), U32(0x000000e1), U32(0x00000032), U32(0x00000000), U32(0x00000000), U32(0x000000ff),
U32(0x00000000), U16(0xffff),
AT(0x3c8) STR12("Courier New"),
/* +0x03e8  meaning not yet known */
AT(0x3e8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x06),
/* +0x0408  the children of element 4, zero-terminated */
AT(0x408) U32(0x00000000),  /* element 4: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 4: end of the list */
/* +0x0418  element 4 of 4: type 0 */
U32(0x00000000),  /* element 4: type */
/* +0x041c  meaning not yet known */
U16(0x1000),
AT(0x43c) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: children list (relocated: installed at attach, rebind.inc) */
/* +0x0444  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0468  the element-list slot (the header's +0x08 points here): the first element */
AT(0x468) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x046c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0484  meaning not yet known */
AT(0x484) U32(0xffffffff),
/* +0x049c  meaning not yet known */
AT(0x49c) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x000000ff), U32(0x0000009d), U32(0xffffffff),
U16(0xa701),
/* +0x04c8  meaning not yet known */
AT(0x4c8) U32(0x00000001), U32(0x00000010), U32(0x00000016), U32(0x000000e1), U32(0x00000032), U32(0xffffffff),
/* +0x04f4  meaning not yet known */
AT(0x4f4) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000038), U32(0x00000032), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010064), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000038), U32(0x00000000), U32(0x00000038), U32(0x00000032), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010065), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000096),
U32(0x00000000), U32(0x00000025), U32(0x00000032), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x00010066), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x000000bb), U32(0x00000000),
U32(0x00000025), U32(0x00000032), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x00010067),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x05b8  meaning not yet known */
AT(0x5b8) U32(0xffffffff),
/* +0x05d0  meaning not yet known */
AT(0x5d0) U8(0x03),
/* +0x05e4  meaning not yet known */
AT(0x5e4) U32(0xffffffff),
/* +0x05fc  meaning not yet known */
AT(0x5fc) U32(0x00000001), U32(0x00000100), U32(0x00000000), U32(0x000000f2), U32(0x0000009d), U32(0xffffffff),
U16(0xa700),
/* +0x0628  meaning not yet known */
AT(0x628) U32(0x00000001), U32(0x00000004), U32(0x00000016), U32(0x000000e1), U32(0x00000032), U32(0xffffffff),
/* +0x0654  meaning not yet known */
AT(0x654) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000038), U32(0x00000032), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010068), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000038), U32(0x00000000), U32(0x00000038), U32(0x00000032), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010069), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000096),
U32(0x00000000), U32(0x00000025), U32(0x00000032), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x0001006a), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x000000bb), U32(0x00000000),
U32(0x00000025), U32(0x00000032), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x0001006b),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0718  meaning not yet known */
AT(0x718) U32(0xffffffff),
/* +0x0730  meaning not yet known */
AT(0x730) U32(0x00000002), U32(0x00000002), U32(0x00000073), U32(0x00000055), U32(0x00000026), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010132), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0770  meaning not yet known */
AT(0x770) U32(0xffffffff),
/* +0x0788  meaning not yet known */
AT(0x788) U8(0x03),
/* +0x079c  meaning not yet known */
AT(0x79c) U32(0xffffffff),
/* +0x07c8  meaning not yet known */
AT(0x7c8) U32(0xffffffff),
AT(0x7e0) STR12("1%d%d.%d%d"),
/* +0x07ec  meaning not yet known */
U32(0x00000000),
/* +0x07f0  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x07f8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0800  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x080c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00000000),  /* linkage +0x18: the gauge header (relocated: installed at attach, rebind.inc) */
/* +0x081c  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef AT
#undef U8
#undef U16
#undef U32
#undef F64
#undef NUL_PAD
#undef SB
#undef S4
#undef STR12
#undef STR32
