/* 737CLKSM.GAU's .data image, decompiled: every non-zero value of retail's .data (0xbfc bytes at RVA 0x3000) at
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
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("737-400.Clock-Sim-Rate"),  /* the gauge name */
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
U8(0x91),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0080  meaning not yet known */
AT(0x80) U32(0x00000001), U32(0x00000000),
/* +0x0088  element 1 of 6: type 1 */
U32(0x00000001),  /* element 1: type */
/* +0x008c  meaning not yet known */
U32(0x00003000), U32(0x00000048), U32(0x00000048), U32(0x00000000), U32(0x00000000), U32(0x00000005),
U32(0x00000005),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
/* +0x00b4  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x00f0  meaning not yet known */
AT(0xf0) U8(0x5a),
/* +0x0168  the children of element 2, zero-terminated */
AT(0x168) U32(0x00000000),  /* element 2: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 2: end of the list */
/* +0x0170  element 2 of 6: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 2: type */
/* +0x0174  meaning not yet known */
U32(0xffffffff), U32(0x0000003c), U8(0x28),
AT(0x194) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: children list (relocated: installed at attach, rebind.inc) */
/* +0x019c  meaning not yet known */
U32(0x00000000), U8(0x06),
AT(0x228) U32(0x00000000),  /* element 2: text buffer */
/* +0x022c  meaning not yet known */
U32(0x00000000), U32(0x00000017), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0x5c5c), U8(0x5c),
AT(0x268) STR12("Courier New"),
/* +0x0288  meaning not yet known */
AT(0x288) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x02),
/* +0x02a8  the children of element 3, zero-terminated */
AT(0x2a8) U32(0x00000000),  /* element 3: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 3: end of the list */
/* +0x02b0  meaning not yet known */
U32(0x00000017), U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x02c8  element 3 of 6: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 3: type */
/* +0x02cc  meaning not yet known */
U32(0xffffffff), U32(0x00000032), U8(0x57),
AT(0x2ec) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: children list (relocated: installed at attach, rebind.inc) */
/* +0x02f4  meaning not yet known */
U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x0150),
AT(0x380) U32(0x00000000),  /* element 3: text buffer */
/* +0x0384  meaning not yet known */
U32(0x00000000), U32(0x0000002d), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x3c0) STR12("Courier New"),
/* +0x03e0  meaning not yet known */
AT(0x3e0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x04),
/* +0x0400  meaning not yet known */
AT(0x400) U32(0x00000018), U32(0x00000000), U32(0x00000001), U32(0x00000019), U32(0x00000003), U32(0x00000004),
U32(0x0000001a), U32(0x00000006), U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0430  element 4 of 6: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 4: type */
/* +0x0434  meaning not yet known */
U32(0xffffffff), U32(0x0000001c), U8(0xa1),
AT(0x454) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
/* +0x045c  meaning not yet known */
U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x05),
/* +0x0488  meaning not yet known */
AT(0x488) U8(0x04),
/* +0x04a4  meaning not yet known */
AT(0x4a4) U8(0x03),
AT(0x4e8) U32(0x00000000),  /* element 4: text buffer */
/* +0x04ec  meaning not yet known */
U32(0x00000000), U32(0x0000005a), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x528) STR12("Courier New"),
/* +0x0548  meaning not yet known */
AT(0x548) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x08),
/* +0x0568  meaning not yet known */
AT(0x568) U32(0x00000002), U32(0x00000000), U32(0x00000004), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0580  element 5 of 6: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 5: type */
/* +0x0584  meaning not yet known */
U32(0xffffffff), U32(0x00000024), U8(0xc5),
AT(0x5a4) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
/* +0x05ac  meaning not yet known */
U32(0x00000000), U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x0151),
AT(0x638) U32(0x00000000),  /* element 5: text buffer */
/* +0x063c  meaning not yet known */
U32(0x00000000), U32(0x00000048), U32(0x00000015), U32(0x00000000), U32(0x00000000), U32(0x00ffffff),
U32(0x00000000), U16(0xffff),
AT(0x678) STR12("Courier New"),
/* +0x0698  meaning not yet known */
AT(0x698) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x04),
/* +0x06b8  the children of element 6, zero-terminated */
AT(0x6b8) U32(0x00000000),  /* element 6: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 6: end of the list */
/* +0x06c8  element 6 of 6: type 0 */
U32(0x00000000),  /* element 6: type */
/* +0x06cc  meaning not yet known */
U16(0x1000),
AT(0x6ec) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: children list (relocated: installed at attach, rebind.inc) */
/* +0x06f4  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0718  the element-list slot (the header's +0x08 points here): the first element */
AT(0x718) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x071c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0734  meaning not yet known */
AT(0x734) U32(0xffffffff),
/* +0x074c  meaning not yet known */
AT(0x74c) U32(0x00000001), U32(0x00000004), U32(0x00000004), U32(0x00000087), U32(0x00000088), U32(0xffffffff),
U16(0xa40e),
/* +0x0778  meaning not yet known */
AT(0x778) U32(0x00000002), U32(0x00000028), U32(0x00000020), U32(0x00000035), U32(0x00000019), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x07a0  meaning not yet known */
U32(0x00000000), U32(0x00000001), U32(0x00000029), U32(0x0000004e), U32(0x00000035), U32(0x0000001b),
U32(0xffffffff), U16(0xa40d),
/* +0x07d0  meaning not yet known */
AT(0x7d0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001a), U32(0x0000001b), U32(0x00000002),
U32(0x00000000), U32(0x20000000), U32(0x000100fe), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000001a), U32(0x00000000), U32(0x0000001b), U32(0x0000001b), U32(0x00000001), U32(0x00000000),
U32(0x20000000), U32(0x000100fd), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x083c  meaning not yet known */
AT(0x83c) U32(0xffffffff),
/* +0x0854  meaning not yet known */
AT(0x854) U8(0x03),
/* +0x0868  meaning not yet known */
AT(0x868) U32(0xffffffff),
/* +0x0880  meaning not yet known */
AT(0x880) U32(0x00000001), U32(0x0000000a), U32(0x0000009b), U32(0x0000007c), U32(0x0000001d), U32(0xffffffff),
U16(0xa40c),
/* +0x08ac  meaning not yet known */
AT(0x8ac) U32(0x00000001), U32(0x0000000b), U32(0x00000007), U32(0x00000066), U32(0x0000000f), U32(0xffffffff),
/* +0x08d8  meaning not yet known */
AT(0x8d8) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000000d), U32(0x0000000f), U32(0x00000002),
U32(0x00000000), U32(0x20000000), U32(0x00010079), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000000d), U32(0x00000000), U32(0x0000000d), U32(0x0000000f), U32(0x00000001), U32(0x00000000),
U32(0x20000000), U32(0x0001007a), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000025),
U32(0x00000000), U32(0x0000000d), U32(0x0000000f), U32(0x00000002), U32(0x00000000), U32(0x20000000),
U32(0x0001007b), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000032), U32(0x00000000),
U32(0x0000000d), U32(0x0000000f), U32(0x00000001), U32(0x00000000), U32(0x20000000), U32(0x0001007c),
U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000004c), U32(0x00000000), U32(0x0000001a),
U32(0x0000000f), U32(0x00000005), U32(0x00000000), U32(0x20000000), U32(0x0001007d), U32(0x00000000),
U32(0x00000000), U8(0x03),
/* +0x09c8  meaning not yet known */
AT(0x9c8) U32(0xffffffff),
/* +0x09e0  meaning not yet known */
AT(0x9e0) U8(0x03),
/* +0x09f4  meaning not yet known */
AT(0x9f4) U32(0xffffffff),
/* +0x0a0c  meaning not yet known */
AT(0xa0c) U32(0x00000001), U32(0x00000019), U32(0x000000bd), U32(0x0000005f), U32(0x00000023), U32(0xffffffff),
U16(0xa442),
/* +0x0a38  meaning not yet known */
AT(0xa38) U32(0x00000001), U32(0x0000000b), U32(0x00000007), U32(0x00000048), U32(0x00000015), U32(0xffffffff),
/* +0x0a64  meaning not yet known */
AT(0xa64) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000024), U32(0x00000015), U32(0x00000002),
U32(0x00000000), U32(0x20000000), U32(0x00010077), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000024), U32(0x00000000), U32(0x00000024), U32(0x00000015), U32(0x00000001), U32(0x00000000),
U32(0x20000000), U32(0x00010078), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0ad0  meaning not yet known */
AT(0xad0) U32(0xffffffff),
/* +0x0ae8  meaning not yet known */
AT(0xae8) U8(0x03),
/* +0x0afc  meaning not yet known */
AT(0xafc) U32(0xffffffff),
/* +0x0b14  meaning not yet known */
AT(0xb14) U8(0x03),
/* +0x0b28  meaning not yet known */
AT(0xb28) U32(0xffffffff),
/* +0x0b54  meaning not yet known */
AT(0xb54) U32(0xffffffff),
AT(0xb6c) STR16("%02d:%02d:%02d"),
/* +0x0b7c  meaning not yet known */
U32(0x00643325),
STR8("%2d.%1d"),
STR12("%1d.%02d"),
STR8("%02d"),
STR12("%01d.%02d"),
STR12("%02d.%1d"),
STR8(" %3d"),
/* +0x0bbc  meaning not yet known */
U32(0x00000000),
/* +0x0bc0  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x0bc8  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x0bd0  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x0bdc  meaning not yet known */
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
#undef STR24
