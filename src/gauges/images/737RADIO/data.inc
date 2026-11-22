/* 737RADIO.GAU's .data image, decompiled: every non-zero value of retail's .data (0x1368 bytes at RVA 0x3000) at
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
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("737-400.Radio-Stack"),  /* the gauge name */
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
U8(0xa5),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x09),
/* +0x0090  meaning not yet known */
AT(0x90) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x10),
/* +0x00b8  meaning not yet known */
AT(0xb8) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x05),
/* +0x00e0  meaning not yet known */
AT(0xe0) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x11),
/* +0x0108  meaning not yet known */
AT(0x108) U32(0x00000001), U8(0x03),
/* +0x0120  element 1 of 9: type 4 */
AT(0x120) U32(0x00000004),  /* element 1: type */
/* +0x0124  meaning not yet known */
U32(0x00002000), U32(0x00000049), U8(0x86),
AT(0x144) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0150  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x1a),
/* +0x0190  meaning not yet known */
AT(0x190) U8(0x01),
/* +0x01a8  element 2 of 9: type 4 */
AT(0x1a8) U32(0x00000004),  /* element 2: type */
/* +0x01ac  meaning not yet known */
U32(0x00002000), U32(0x00000049), U8(0xcb),
AT(0x1cc) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01d8  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x1d),
/* +0x0218  meaning not yet known */
AT(0x218) U8(0x01),
/* +0x0230  element 3 of 9: type 4 */
AT(0x230) U32(0x00000004),  /* element 3: type */
/* +0x0234  meaning not yet known */
U32(0x00002000), U32(0x00000049), U16(0x0111),
AT(0x254) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0260  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x15),
/* +0x02a0  meaning not yet known */
AT(0x2a0) U8(0x01),
/* +0x02b8  meaning not yet known */
AT(0x2b8) U32(0x00000004), U32(0x00000000), U32(0x00000002), U32(0x00000005), U32(0x00000004), U8(0x05),
/* +0x02e0  element 4 of 9: type 2 (text, 0xbc bytes) */
AT(0x2e0) U32(0x00000002),  /* element 4: type */
/* +0x02e4  meaning not yet known */
U32(0xffffffff), U32(0x0000001c), U8(0x26),
AT(0x304) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0310  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x17),
AT(0x398) U32(0x00000000),  /* element 4: text buffer */
/* +0x039c  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0xffff),
AT(0x3d8) STR12("Courier New"),
/* +0x03f8  meaning not yet known */
AT(0x3f8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x06),
/* +0x0418  meaning not yet known */
AT(0x418) U32(0x00000006), U32(0x00000000), U32(0x00000002), U32(0x00000007), U32(0x00000004), U8(0x05),
/* +0x0440  element 5 of 9: type 2 (text, 0xbc bytes) */
AT(0x440) U32(0x00000002),  /* element 5: type */
/* +0x0444  meaning not yet known */
U32(0xffffffff), U32(0x0000001c), U8(0x6d),
AT(0x464) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0470  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x18),
AT(0x4f8) U32(0x00000000),  /* element 5: text buffer */
/* +0x04fc  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0xffff),
AT(0x538) STR12("Courier New"),
/* +0x0558  meaning not yet known */
AT(0x558) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x06),
/* +0x0578  meaning not yet known */
AT(0x578) U32(0x00000008), U32(0x00000000), U32(0x00000002), U32(0x00000009), U32(0x00000004), U8(0x05),
/* +0x05a0  element 6 of 9: type 2 (text, 0xbc bytes) */
AT(0x5a0) U32(0x00000002),  /* element 6: type */
/* +0x05a4  meaning not yet known */
U32(0xffffffff), U32(0x0000001c), U8(0xb2),
AT(0x5c4) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05d0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1b),
AT(0x658) U32(0x00000000),  /* element 6: text buffer */
/* +0x065c  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0xffff),
AT(0x698) STR12("Courier New"),
/* +0x06b8  meaning not yet known */
AT(0x6b8) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x06),
/* +0x06d8  meaning not yet known */
AT(0x6d8) U32(0x00000013), U32(0x00000001), U32(0x00000001), U32(0x00000014), U32(0x00000002), U32(0x00000002),
U32(0x00000015), U32(0x00000003), U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0708  element 7 of 9: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 7: type */
/* +0x070c  meaning not yet known */
U32(0xffffffff), U32(0x0000001c), U8(0xfa),
AT(0x72c) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0738  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x10),
/* +0x0760  meaning not yet known */
AT(0x760) U8(0x12),
/* +0x077c  meaning not yet known */
AT(0x77c) U8(0x11),
AT(0x7c0) U32(0x00000000),  /* element 7: text buffer */
/* +0x07c4  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0xffff),
AT(0x800) STR12("Courier New"),
/* +0x0820  meaning not yet known */
AT(0x820) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x07),
/* +0x0840  meaning not yet known */
AT(0x840) U32(0x0000000a), U32(0x00000000), U32(0x00000000), U32(0x0000000b), U32(0x00000001), U32(0x00000001),
U32(0x0000000c), U32(0x00000002), U32(0x00000002), U32(0x0000000d), U32(0x00000003), U8(0x03),
/* +0x0880  element 8 of 9: type 2 (text, 0xbc bytes) */
AT(0x880) U32(0x00000002),  /* element 8: type */
/* +0x0884  meaning not yet known */
U32(0xffffffff), U32(0x0000003a), U16(0x0136),
AT(0x8a4) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x08b0  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x26),
AT(0x938) U32(0x00000000),  /* element 8: text buffer */
/* +0x093c  meaning not yet known */
U32(0x00000000), U32(0x00000037), U32(0x00000013), U32(0x00000000), U32(0x00000000), U32(0x0000ff00),
U32(0x00000000), U16(0xffff),
AT(0x978) STR12("Courier New"),
/* +0x0998  meaning not yet known */
AT(0x998) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x04),
/* +0x09b8  the children of element 9, zero-terminated */
AT(0x9b8) U32(0x00000000),  /* element 9: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: end of the list */
/* +0x09dc  meaning not yet known */
U32(0x00000000),
/* +0x09e0  element 9 of 9: type 0 */
U32(0x00000000),  /* element 9: type */
/* +0x09e4  meaning not yet known */
U16(0x1000),
AT(0xa04) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: children list (relocated: installed at attach, rebind.inc) */
/* +0x0a0c  meaning not yet known */
U32(0x00000000), U8(0x11),
/* +0x0a30  the element-list slot (the header's +0x08 points here): the first element */
AT(0xa30) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0a34  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0a4c  meaning not yet known */
AT(0xa4c) U32(0xffffffff),
/* +0x0a64  meaning not yet known */
AT(0xa64) U32(0x00000001), U32(0x00000006), U32(0x0000001b), U32(0x0000009a), U32(0x00000048), U32(0xffffffff),
U16(0xa421),
/* +0x0a90  meaning not yet known */
AT(0xa90) U32(0x00000001), U32(0x00000016), U32(0x0000000b), U32(0x00000070), U32(0x00000013), U32(0xffffffff),
/* +0x0abc  meaning not yet known */
AT(0xabc) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001b), U32(0x00000013), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010064), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000001b), U32(0x00000000), U32(0x0000001b), U32(0x0000001d), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010065), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000046),
U32(0x00000000), U32(0x00000014), U32(0x00000013), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x00010066), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000057), U32(0x00000000),
U32(0x00000014), U32(0x00000013), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x00010067),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0b80  meaning not yet known */
AT(0xb80) U32(0xffffffff),
/* +0x0b98  meaning not yet known */
AT(0xb98) U8(0x03),
/* +0x0bac  meaning not yet known */
AT(0xbac) U32(0xffffffff),
/* +0x0bc4  meaning not yet known */
AT(0xbc4) U32(0x00000001), U32(0x00000006), U32(0x00000063), U32(0x0000009a), U32(0x00000045), U32(0xffffffff),
U16(0xa422),
/* +0x0bf0  meaning not yet known */
AT(0xbf0) U32(0x00000001), U32(0x00000015), U32(0x0000000a), U32(0x00000070), U32(0x00000013), U32(0xffffffff),
/* +0x0c1c  meaning not yet known */
AT(0xc1c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001b), U32(0x00000013), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010068), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000001b), U32(0x00000000), U32(0x0000001b), U32(0x0000001d), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010069), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000046),
U32(0x00000000), U32(0x00000014), U32(0x00000013), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x0001006a), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000057), U32(0x00000000),
U32(0x00000014), U32(0x00000013), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x0001006b),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0ce0  meaning not yet known */
AT(0xce0) U32(0xffffffff),
/* +0x0cf8  meaning not yet known */
AT(0xcf8) U32(0x00000001), U32(0x00000040), U32(0x00000020), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
/* +0x0d24  meaning not yet known */
AT(0xd24) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010132), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0d64  meaning not yet known */
AT(0xd64) U32(0xffffffff),
/* +0x0d7c  meaning not yet known */
AT(0xd7c) U8(0x03),
/* +0x0d90  meaning not yet known */
AT(0xd90) U32(0xffffffff),
/* +0x0da8  meaning not yet known */
AT(0xda8) U32(0x00000001), U32(0x00000006), U32(0x000000a8), U32(0x00000099), U32(0x00000044), U32(0xffffffff),
U16(0xa423),
/* +0x0dd4  meaning not yet known */
AT(0xdd4) U32(0x00000001), U32(0x00000016), U32(0x0000000a), U32(0x00000070), U32(0x00000013), U32(0xffffffff),
/* +0x0e00  meaning not yet known */
AT(0xe00) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001b), U32(0x00000013), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001006c), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000001b), U32(0x00000000), U32(0x0000001b), U32(0x0000001d), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001006d), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000046),
U32(0x00000000), U32(0x00000014), U32(0x00000013), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x0001006e), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000057), U32(0x00000000),
U32(0x00000014), U32(0x00000013), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x0001006f),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0ec4  meaning not yet known */
AT(0xec4) U32(0xffffffff),
/* +0x0edc  meaning not yet known */
AT(0xedc) U32(0x00000001), U32(0x00000040), U32(0x0000001f), U32(0x0000001c), U32(0x0000001c), U32(0xffffffff),
/* +0x0f08  meaning not yet known */
AT(0xf08) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000001c), U32(0x0000001c), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010133), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0f48  meaning not yet known */
AT(0xf48) U32(0xffffffff),
/* +0x0f60  meaning not yet known */
AT(0xf60) U8(0x03),
/* +0x0f74  meaning not yet known */
AT(0xf74) U32(0xffffffff),
/* +0x0f8c  meaning not yet known */
AT(0xf8c) U32(0x00000001), U32(0x00000006), U32(0x000000ec), U32(0x00000099), U32(0x00000046), U32(0xffffffff),
U16(0xa424),
/* +0x0fb8  meaning not yet known */
AT(0xfb8) U32(0x00000001), U32(0x00000016), U32(0x0000000e), U32(0x00000070), U32(0x00000013), U32(0xffffffff),
/* +0x0fe4  meaning not yet known */
AT(0xfe4) U32(0x00000002), U32(0x00000013), U32(0x00000000), U32(0x00000012), U32(0x00000013), U32(0x00000001),
U32(0x00000000), U32(0x20200000), U32(0x00010070), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000025), U32(0x00000000), U32(0x00000013), U32(0x00000013), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010071), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000003a),
U32(0x00000000), U32(0x00000012), U32(0x00000013), U32(0x00000001), U32(0x00000000), U32(0x20200000),
U32(0x00010072), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x107c  meaning not yet known */
AT(0x107c) U32(0xffffffff),
/* +0x1094  meaning not yet known */
AT(0x1094) U32(0x00000001), U32(0x00000040), U32(0x0000001b), U32(0x0000004f), U32(0x00000041), U32(0xffffffff),
/* +0x10c0  meaning not yet known */
AT(0x10c0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000004f), U32(0x00000041), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010136), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1100  meaning not yet known */
AT(0x1100) U32(0xffffffff),
/* +0x1118  meaning not yet known */
AT(0x1118) U8(0x03),
/* +0x112c  meaning not yet known */
AT(0x112c) U32(0xffffffff),
/* +0x1144  meaning not yet known */
AT(0x1144) U32(0x00000001), U32(0x00000006), U32(0x00000132), U32(0x00000099), U32(0x0000003a), U32(0xffffffff),
U16(0xa425),
/* +0x1170  meaning not yet known */
AT(0x1170) U32(0x00000001), U32(0x00000034), U32(0x00000004), U32(0x00000037), U32(0x00000013), U32(0xffffffff),
/* +0x119c  meaning not yet known */
AT(0x119c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000000d), U32(0x00000013), U32(0x00000001),
U32(0x00000000), U32(0x20000000), U32(0x00010073), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000000d), U32(0x00000000), U32(0x0000000d), U32(0x00000013), U32(0x00000001), U32(0x00000000),
U32(0x20000000), U32(0x00010074), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000001a),
U32(0x00000000), U32(0x0000000d), U32(0x00000013), U32(0x00000001), U32(0x00000000), U32(0x20000000),
U32(0x00010075), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x00000027), U32(0x00000000),
U32(0x0000000e), U32(0x00000013), U32(0x00000001), U32(0x00000000), U32(0x20000000), U32(0x00010076),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x1260  meaning not yet known */
AT(0x1260) U32(0xffffffff),
/* +0x1278  meaning not yet known */
AT(0x1278) U8(0x03),
/* +0x128c  meaning not yet known */
AT(0x128c) U32(0xffffffff),
/* +0x12a4  meaning not yet known */
AT(0x12a4) U8(0x03),
/* +0x12b8  meaning not yet known */
AT(0x12b8) U32(0xffffffff),
/* +0x12e4  meaning not yet known */
AT(0x12e4) U32(0xffffffff),
AT(0x12fc) STR12("1%d%d.%d%d"),
STR16("%c%c%c%c%c%c%c"),
STR12("%d%d%d%d"),
/* +0x1324  meaning not yet known */
U32(0x00000000),
/* +0x1328  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x1330  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x1338  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x1344  meaning not yet known */
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
#undef STR12
#undef STR16
#undef STR24
