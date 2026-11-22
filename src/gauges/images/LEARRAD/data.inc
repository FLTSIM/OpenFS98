/* LEARRAD.GAU's .data image, decompiled: every non-zero value of retail's .data (0x11a8 bytes at RVA 0x3000) at
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
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR16("Lear-45.Radio"),  /* the gauge name */
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
U8(0xb2),
/* +0x0060  meaning not yet known */
AT(0x60) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x09),
/* +0x0088  meaning not yet known */
AT(0x88) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x10),
/* +0x00b0  meaning not yet known */
AT(0xb0) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x05),
/* +0x00d8  meaning not yet known */
AT(0xd8) U32(0x00000001), U32(0x00000003), U32(0x00000000), U8(0x11),
/* +0x0100  meaning not yet known */
AT(0x100) U32(0x00000001), U8(0x03),
/* +0x0118  meaning not yet known */
AT(0x118) U32(0x00000004), U32(0x00000000), U32(0x00000002), U32(0x00000005), U32(0x00000004), U8(0x05),
/* +0x0140  element 1 of 9: type 4 */
AT(0x140) U32(0x00000004),  /* element 1: type */
/* +0x0144  meaning not yet known */
U32(0x00002000), U32(0x00000006), U8(0x40),
AT(0x164) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0170  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x1a),
/* +0x01b0  meaning not yet known */
AT(0x1b0) U8(0x01),
/* +0x01c8  element 2 of 9: type 4 */
AT(0x1c8) U32(0x00000004),  /* element 2: type */
/* +0x01cc  meaning not yet known */
U32(0x00002000), U32(0x00000006), U8(0x59),
AT(0x1ec) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01f8  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x1d),
/* +0x0238  meaning not yet known */
AT(0x238) U8(0x01),
/* +0x0250  element 3 of 9: type 4 */
AT(0x250) U32(0x00000004),  /* element 3: type */
/* +0x0254  meaning not yet known */
U32(0x00002000), U32(0x00000006), U8(0x72),
AT(0x274) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0280  meaning not yet known */
U32(0x00000002), U32(0x00000000), U32(0x00000000), U8(0x15),
/* +0x02c0  meaning not yet known */
AT(0x2c0) U8(0x01),
/* +0x02d8  element 4 of 9: type 2 (text, 0xbc bytes) */
AT(0x2d8) U32(0x00000002),  /* element 4: type */
/* +0x02dc  meaning not yet known */
U32(0xffffffff), U32(0x00000023), U8(0x19),
AT(0x2fc) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0308  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x17),
AT(0x390) U32(0x00000000),  /* element 4: text buffer */
/* +0x0394  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x00008000),
U32(0x00000000), U16(0xffff),
AT(0x3d0) STR12("Courier New"),
/* +0x03f0  meaning not yet known */
AT(0x3f0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x07),
/* +0x0410  meaning not yet known */
AT(0x410) U32(0x00000006), U32(0x00000000), U32(0x00000002), U32(0x00000007), U32(0x00000004), U8(0x05),
/* +0x0438  element 5 of 9: type 2 (text, 0xbc bytes) */
AT(0x438) U32(0x00000002),  /* element 5: type */
/* +0x043c  meaning not yet known */
U32(0xffffffff), U32(0x00000023), U8(0x32),
AT(0x45c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0468  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x18),
AT(0x4f0) U32(0x00000000),  /* element 5: text buffer */
/* +0x04f4  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x00008000),
U32(0x00000000), U16(0xffff),
AT(0x530) STR12("Courier New"),
/* +0x0550  meaning not yet known */
AT(0x550) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x07),
/* +0x0570  meaning not yet known */
AT(0x570) U32(0x00000008), U32(0x00000000), U32(0x00000002), U32(0x00000009), U32(0x00000004), U8(0x05),
/* +0x0598  element 6 of 9: type 2 (text, 0xbc bytes) */
AT(0x598) U32(0x00000002),  /* element 6: type */
/* +0x059c  meaning not yet known */
U32(0xffffffff), U32(0x00000023), U8(0x4b),
AT(0x5bc) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x05c8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x1b),
AT(0x650) U32(0x00000000),  /* element 6: text buffer */
/* +0x0654  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x00008000),
U32(0x00000000), U16(0xffff),
AT(0x690) STR12("Courier New"),
/* +0x06b0  meaning not yet known */
AT(0x6b0) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x07),
/* +0x06d0  meaning not yet known */
AT(0x6d0) U32(0x00000013), U32(0x00000001), U32(0x00000001), U32(0x00000014), U32(0x00000002), U32(0x00000002),
U32(0x00000015), U32(0x00000003), U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0700  element 7 of 9: type 2 (text, 0xbc bytes) */
U32(0x00000002),  /* element 7: type */
/* +0x0704  meaning not yet known */
U32(0xffffffff), U32(0x00000023), U8(0x64),
AT(0x724) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0730  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x10),
/* +0x0758  meaning not yet known */
AT(0x758) U8(0x12),
/* +0x0774  meaning not yet known */
AT(0x774) U8(0x11),
AT(0x7b8) U32(0x00000000),  /* element 7: text buffer */
/* +0x07bc  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x00008000),
U32(0x00000000), U16(0xffff),
AT(0x7f8) STR12("Courier New"),
/* +0x0818  meaning not yet known */
AT(0x818) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x07),
/* +0x0838  meaning not yet known */
AT(0x838) U32(0x0000000a), U32(0x00000000), U32(0x00000000), U32(0x0000000b), U32(0x00000001), U32(0x00000001),
U32(0x0000000c), U32(0x00000002), U32(0x00000002), U32(0x0000000d), U32(0x00000003), U8(0x03),
/* +0x0878  element 8 of 9: type 2 (text, 0xbc bytes) */
AT(0x878) U32(0x00000002),  /* element 8: type */
/* +0x087c  meaning not yet known */
U32(0xffffffff), U32(0x00000023), U8(0x7d),
AT(0x89c) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x08a8  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U8(0x26),
AT(0x930) U32(0x00000000),  /* element 8: text buffer */
/* +0x0934  meaning not yet known */
U32(0x00000000), U32(0x00000070), U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x00008000),
U32(0x00000000), U16(0xffff),
AT(0x970) STR12("Courier New"),
/* +0x0990  meaning not yet known */
AT(0x990) U32(0x00000190), U32(0x00000001), U32(0x00000025), U8(0x07),
/* +0x09b0  the children of element 9, zero-terminated */
AT(0x9b0) U32(0x00000000),  /* element 9: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 9: end of the list */
/* +0x09d4  meaning not yet known */
U32(0x00000000),
/* +0x09d8  element 9 of 9: type 0 */
U32(0x00000000),  /* element 9: type */
/* +0x09dc  meaning not yet known */
U16(0x1000),
AT(0x9fc) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: children list (relocated: installed at attach, rebind.inc) */
/* +0x0a04  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0a28  the element-list slot (the header's +0x08 points here): the first element */
AT(0xa28) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x0a2c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x0a44  meaning not yet known */
AT(0xa44) U32(0xffffffff),
/* +0x0a5c  meaning not yet known */
AT(0xa5c) U32(0x00000001), U32(0x00000027), U32(0x00000019), U32(0x00000068), U32(0x00000019), U32(0xffffffff),
U16(0xa127),
/* +0x0a88  meaning not yet known */
AT(0xa88) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000017), U32(0x00000019), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010064), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000017), U32(0x00000000), U32(0x00000017), U32(0x00000019), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010065), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000003b),
U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x00010066), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000004a), U32(0x00000000),
U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x00010067),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0b4c  meaning not yet known */
AT(0xb4c) U32(0xffffffff),
/* +0x0b64  meaning not yet known */
AT(0xb64) U32(0x00000001), U32(0x00000027), U32(0x00000032), U32(0x00000068), U32(0x00000019), U32(0xffffffff),
U16(0xa128),
/* +0x0b90  meaning not yet known */
AT(0xb90) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000017), U32(0x00000019), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x00010068), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000017), U32(0x00000000), U32(0x00000017), U32(0x00000019), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010069), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000003b),
U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x0001006a), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000004a), U32(0x00000000),
U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x0001006b),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0c54  meaning not yet known */
AT(0xc54) U32(0xffffffff),
/* +0x0c6c  meaning not yet known */
AT(0xc6c) U32(0x00000001), U32(0x00000005), U32(0x0000003e), U32(0x00000013), U32(0x0000000e), U32(0xffffffff),
/* +0x0c98  meaning not yet known */
AT(0xc98) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000013), U32(0x0000000e), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010132), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0cd8  meaning not yet known */
AT(0xcd8) U32(0xffffffff),
/* +0x0cf0  meaning not yet known */
AT(0xcf0) U32(0x00000001), U32(0x00000027), U32(0x0000004b), U32(0x00000068), U32(0x00000019), U32(0xffffffff),
U16(0xa129),
/* +0x0d1c  meaning not yet known */
AT(0xd1c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000017), U32(0x00000019), U32(0x00000002),
U32(0x00000000), U32(0x20200000), U32(0x0001006c), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x00000017), U32(0x00000000), U32(0x00000017), U32(0x00000019), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x0001006d), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000003b),
U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000002), U32(0x00000000), U32(0x20200000),
U32(0x0001006e), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000004a), U32(0x00000000),
U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000), U32(0x20200000), U32(0x0001006f),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0de0  meaning not yet known */
AT(0xde0) U32(0xffffffff),
/* +0x0df8  meaning not yet known */
AT(0xdf8) U32(0x00000001), U32(0x00000005), U32(0x00000057), U32(0x00000013), U32(0x0000000e), U32(0xffffffff),
/* +0x0e24  meaning not yet known */
AT(0xe24) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000013), U32(0x0000000e), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010133), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0e64  meaning not yet known */
AT(0xe64) U32(0xffffffff),
/* +0x0e7c  meaning not yet known */
AT(0xe7c) U32(0x00000001), U32(0x00000027), U32(0x00000064), U32(0x00000068), U32(0x00000019), U32(0xffffffff),
U16(0xa12b),
/* +0x0ea8  meaning not yet known */
AT(0xea8) U32(0x00000002), U32(0x0000000f), U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000001),
U32(0x00000000), U32(0x20200000), U32(0x00010070), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000001e), U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000),
U32(0x20200000), U32(0x00010071), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000002d),
U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000), U32(0x20200000),
U32(0x00010072), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0f40  meaning not yet known */
AT(0xf40) U32(0xffffffff),
/* +0x0f58  meaning not yet known */
AT(0xf58) U32(0x00000001), U32(0x00000005), U32(0x00000070), U32(0x00000013), U32(0x0000000e), U32(0xffffffff),
/* +0x0f84  meaning not yet known */
AT(0xf84) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000004f), U32(0x00000041), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00010136), U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x0fc4  meaning not yet known */
AT(0xfc4) U32(0xffffffff),
/* +0x0fdc  meaning not yet known */
AT(0xfdc) U32(0x00000001), U32(0x00000027), U32(0x0000007d), U32(0x00000068), U32(0x00000019), U32(0xffffffff),
U16(0xa12a),
/* +0x1008  meaning not yet known */
AT(0x1008) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000001),
U32(0x00000000), U32(0x20000000), U32(0x00010073), U32(0x00000000), U32(0x00000000), U32(0x00000002),
U32(0x0000000f), U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000),
U32(0x20000000), U32(0x00010074), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000001e),
U32(0x00000000), U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000), U32(0x20000000),
U32(0x00010075), U32(0x00000000), U32(0x00000000), U32(0x00000002), U32(0x0000002d), U32(0x00000000),
U32(0x0000000f), U32(0x00000019), U32(0x00000001), U32(0x00000000), U32(0x20000000), U32(0x00010076),
U32(0x00000000), U32(0x00000000), U8(0x03),
/* +0x10cc  meaning not yet known */
AT(0x10cc) U32(0xffffffff),
/* +0x10e4  meaning not yet known */
AT(0x10e4) U8(0x03),
/* +0x10f8  meaning not yet known */
AT(0x10f8) U32(0xffffffff),
/* +0x1124  meaning not yet known */
AT(0x1124) U32(0xffffffff),
AT(0x113c) STR12("1%d%d.%d%d"),
STR16("%c%c%c%c%c%c%c"),
STR12("%d%d%d%d"),
/* +0x1164  meaning not yet known */
U32(0x00000000),
/* +0x1168  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x1170  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x1178  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x1184  meaning not yet known */
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
