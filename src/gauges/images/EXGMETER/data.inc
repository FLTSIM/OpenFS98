/* EXGMETER.GAU's .data image, decompiled: every non-zero value of retail's .data (0x12e0 bytes at RVA 0x3000) at
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
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
/* +0x0000  the gauge name, padded to the header */
AT(0x0) STR24("Extra-300.G-Meter"),  /* the gauge name */
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
U8(0x53),
/* +0x0068  meaning not yet known */
AT(0x68) U32(0x00000001), U8(0x03),
/* +0x0086  meaning not yet known */
AT(0x86) U16(0xc028), U32(0x00000000), U32(0xc0260000), U32(0x00000000), U32(0xc0240000), U32(0x00000000),
U32(0xc0220000), U32(0x00000000), U32(0xc0200000), U32(0x00000000), U32(0xc01c0000), U32(0x00000000),
U32(0xc0180000), U32(0x00000000), U32(0xc0140000), U32(0x00000000), U32(0xc0100000), U32(0x00000000),
U32(0xc0080000), U32(0x00000000), U32(0xc0000000), U32(0x00000000), U32(0xbff00000), U32(0x00000000),
U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U32(0x40000000), U32(0x00000000),
U32(0x40080000), U32(0x00000000), U32(0x40100000), U32(0x00000000), U32(0x40140000), U32(0x00000000),
U32(0x40180000), U32(0x00000000), U32(0x401c0000), U32(0x00000000), U32(0x40200000), U32(0x00000000),
U32(0x40220000), U32(0x00000000), U32(0x40240000), U32(0x00000000), U32(0x40260000), U32(0x00000000),
U32(0x40280000),
/* +0x0148  element 1 of 28: type 4 */
U32(0x00000004),  /* element 1: type */
/* +0x014c  meaning not yet known */
U32(0x00004000), U32(0x000000b4), U8(0xff),
AT(0x16c) U32(0x00000000),  /* element 1: parent, written at run time */
U32(0x00000000),  /* element 1: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0178  meaning not yet known */
U8(0x07),
/* +0x01a4  meaning not yet known */
AT(0x1a4) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x01cc  element 2 of 28: type 4 */
AT(0x1cc) U32(0x00000004),  /* element 2: type */
/* +0x01d0  meaning not yet known */
U32(0x00004000), U32(0x000000a2), U16(0x010d),
AT(0x1f0) U32(0x00000000),  /* element 2: parent, written at run time */
U32(0x00000000),  /* element 2: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x01fc  meaning not yet known */
U8(0x07),
/* +0x0228  meaning not yet known */
AT(0x228) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0250  element 3 of 28: type 4 */
AT(0x250) U32(0x00000004),  /* element 3: type */
/* +0x0254  meaning not yet known */
U32(0x00004000), U32(0x00000091), U16(0x0104),
AT(0x274) U32(0x00000000),  /* element 3: parent, written at run time */
U32(0x00000000),  /* element 3: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0280  meaning not yet known */
U8(0x07),
/* +0x02ac  meaning not yet known */
AT(0x2ac) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x02d4  element 4 of 28: type 4 */
AT(0x2d4) U32(0x00000004),  /* element 4: type */
/* +0x02d8  meaning not yet known */
U32(0x00003000), U32(0x00000080), U16(0x0103),
AT(0x2f8) U32(0x00000000),  /* element 4: parent, written at run time */
U32(0x00000000),  /* element 4: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0304  meaning not yet known */
U8(0x07),
/* +0x0330  meaning not yet known */
AT(0x330) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0358  element 5 of 28: type 4 */
AT(0x358) U32(0x00000004),  /* element 5: type */
/* +0x035c  meaning not yet known */
U32(0x00003000), U32(0x0000006e), U8(0xff),
AT(0x37c) U32(0x00000000),  /* element 5: parent, written at run time */
U32(0x00000000),  /* element 5: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0388  meaning not yet known */
U8(0x07),
/* +0x03b4  meaning not yet known */
AT(0x3b4) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x03dc  element 6 of 28: type 4 */
AT(0x3dc) U32(0x00000004),  /* element 6: type */
/* +0x03e0  meaning not yet known */
U32(0x00002000), U32(0x0000005e), U8(0xf8),
AT(0x400) U32(0x00000000),  /* element 6: parent, written at run time */
U32(0x00000000),  /* element 6: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x040c  meaning not yet known */
U8(0x07),
/* +0x0438  meaning not yet known */
AT(0x438) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0460  element 7 of 28: type 4 */
AT(0x460) U32(0x00000004),  /* element 7: type */
/* +0x0464  meaning not yet known */
U32(0x00002000), U32(0x0000004e), U8(0xef),
AT(0x484) U32(0x00000000),  /* element 7: parent, written at run time */
U32(0x00000000),  /* element 7: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0490  meaning not yet known */
U8(0x07),
/* +0x04bc  meaning not yet known */
AT(0x4bc) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x04e4  element 8 of 28: type 4 */
AT(0x4e4) U32(0x00000004),  /* element 8: type */
/* +0x04e8  meaning not yet known */
U32(0x00002000), U32(0x00000041), U8(0xe3),
AT(0x508) U32(0x00000000),  /* element 8: parent, written at run time */
U32(0x00000000),  /* element 8: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0514  meaning not yet known */
U8(0x07),
/* +0x0540  meaning not yet known */
AT(0x540) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0568  element 9 of 28: type 4 */
AT(0x568) U32(0x00000004),  /* element 9: type */
/* +0x056c  meaning not yet known */
U32(0x00002000), U32(0x00000035), U8(0xd6),
AT(0x58c) U32(0x00000000),  /* element 9: parent, written at run time */
U32(0x00000000),  /* element 9: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0598  meaning not yet known */
U8(0x07),
/* +0x05c4  meaning not yet known */
AT(0x5c4) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x05ec  element 10 of 28: type 4 */
AT(0x5ec) U32(0x00000004),  /* element 10: type */
/* +0x05f0  meaning not yet known */
U32(0x00002000), U32(0x0000002b), U8(0xc7),
AT(0x610) U32(0x00000000),  /* element 10: parent, written at run time */
U32(0x00000000),  /* element 10: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x061c  meaning not yet known */
U8(0x07),
/* +0x0648  meaning not yet known */
AT(0x648) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0670  element 11 of 28: type 4 */
AT(0x670) U32(0x00000004),  /* element 11: type */
/* +0x0674  meaning not yet known */
U32(0x00002000), U32(0x00000024), U8(0xb6),
AT(0x694) U32(0x00000000),  /* element 11: parent, written at run time */
U32(0x00000000),  /* element 11: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x06a0  meaning not yet known */
U8(0x07),
/* +0x06cc  meaning not yet known */
AT(0x6cc) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x06f4  element 12 of 28: type 4 */
AT(0x6f4) U32(0x00000004),  /* element 12: type */
/* +0x06f8  meaning not yet known */
U32(0x00002000), U32(0x00000020), U8(0xa4),
AT(0x718) U32(0x00000000),  /* element 12: parent, written at run time */
U32(0x00000000),  /* element 12: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0724  meaning not yet known */
U8(0x07),
/* +0x0750  meaning not yet known */
AT(0x750) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0778  element 13 of 28: type 4 */
AT(0x778) U32(0x00000004),  /* element 13: type */
/* +0x077c  meaning not yet known */
U32(0x00002000), U32(0x0000001e), U8(0x93),
AT(0x79c) U32(0x00000000),  /* element 13: parent, written at run time */
U32(0x00000000),  /* element 13: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x07a8  meaning not yet known */
U8(0x07),
/* +0x07d4  meaning not yet known */
AT(0x7d4) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x07fc  element 14 of 28: type 4 */
AT(0x7fc) U32(0x00000004),  /* element 14: type */
/* +0x0800  meaning not yet known */
U32(0x00002000), U32(0x0000001f), U8(0x80),
AT(0x820) U32(0x00000000),  /* element 14: parent, written at run time */
U32(0x00000000),  /* element 14: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x082c  meaning not yet known */
U8(0x07),
/* +0x0858  meaning not yet known */
AT(0x858) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0880  element 15 of 28: type 4 */
AT(0x880) U32(0x00000004),  /* element 15: type */
/* +0x0884  meaning not yet known */
U32(0x00002000), U32(0x00000024), U8(0x6f),
AT(0x8a4) U32(0x00000000),  /* element 15: parent, written at run time */
U32(0x00000000),  /* element 15: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x08b0  meaning not yet known */
U8(0x07),
/* +0x08dc  meaning not yet known */
AT(0x8dc) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0904  element 16 of 28: type 4 */
AT(0x904) U32(0x00000004),  /* element 16: type */
/* +0x0908  meaning not yet known */
U32(0x00002000), U32(0x0000002a), U8(0x5e),
AT(0x928) U32(0x00000000),  /* element 16: parent, written at run time */
U32(0x00000000),  /* element 16: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0934  meaning not yet known */
U8(0x07),
/* +0x0960  meaning not yet known */
AT(0x960) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0988  element 17 of 28: type 4 */
AT(0x988) U32(0x00000004),  /* element 17: type */
/* +0x098c  meaning not yet known */
U32(0x00002000), U32(0x00000033), U8(0x4d),
AT(0x9ac) U32(0x00000000),  /* element 17: parent, written at run time */
U32(0x00000000),  /* element 17: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x09b8  meaning not yet known */
U8(0x07),
/* +0x09e4  meaning not yet known */
AT(0x9e4) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0a0c  element 18 of 28: type 4 */
AT(0xa0c) U32(0x00000004),  /* element 18: type */
/* +0x0a10  meaning not yet known */
U32(0x00002000), U32(0x00000040), U8(0x40),
AT(0xa30) U32(0x00000000),  /* element 18: parent, written at run time */
U32(0x00000000),  /* element 18: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0a3c  meaning not yet known */
U8(0x07),
/* +0x0a68  meaning not yet known */
AT(0xa68) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0a90  element 19 of 28: type 4 */
AT(0xa90) U32(0x00000004),  /* element 19: type */
/* +0x0a94  meaning not yet known */
U32(0x00002000), U32(0x0000004e), U8(0x33),
AT(0xab4) U32(0x00000000),  /* element 19: parent, written at run time */
U32(0x00000000),  /* element 19: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0ac0  meaning not yet known */
U8(0x07),
/* +0x0aec  meaning not yet known */
AT(0xaec) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0b14  element 20 of 28: type 4 */
AT(0xb14) U32(0x00000004),  /* element 20: type */
/* +0x0b18  meaning not yet known */
U32(0x00002000), U32(0x0000005d), U8(0x2a),
AT(0xb38) U32(0x00000000),  /* element 20: parent, written at run time */
U32(0x00000000),  /* element 20: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0b44  meaning not yet known */
U8(0x07),
/* +0x0b70  meaning not yet known */
AT(0xb70) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0b98  element 21 of 28: type 4 */
AT(0xb98) U32(0x00000004),  /* element 21: type */
/* +0x0b9c  meaning not yet known */
U32(0x00003000), U32(0x0000006f), U8(0x22),
AT(0xbbc) U32(0x00000000),  /* element 21: parent, written at run time */
U32(0x00000000),  /* element 21: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0bc8  meaning not yet known */
U8(0x07),
/* +0x0bf4  meaning not yet known */
AT(0xbf4) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0c1c  element 22 of 28: type 4 */
AT(0xc1c) U32(0x00000004),  /* element 22: type */
/* +0x0c20  meaning not yet known */
U32(0x00003000), U32(0x00000080), U8(0x1e),
AT(0xc40) U32(0x00000000),  /* element 22: parent, written at run time */
U32(0x00000000),  /* element 22: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0c4c  meaning not yet known */
U8(0x07),
/* +0x0c78  meaning not yet known */
AT(0xc78) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0ca0  element 23 of 28: type 4 */
AT(0xca0) U32(0x00000004),  /* element 23: type */
/* +0x0ca4  meaning not yet known */
U32(0x00004000), U32(0x00000092), U8(0x1c),
AT(0xcc4) U32(0x00000000),  /* element 23: parent, written at run time */
U32(0x00000000),  /* element 23: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0cd0  meaning not yet known */
U8(0x07),
/* +0x0cfc  meaning not yet known */
AT(0xcfc) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0d24  element 24 of 28: type 4 */
AT(0xd24) U32(0x00000004),  /* element 24: type */
/* +0x0d28  meaning not yet known */
U32(0x00004000), U32(0x000000a5), U8(0x1e),
AT(0xd48) U32(0x00000000),  /* element 24: parent, written at run time */
U32(0x00000000),  /* element 24: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0d54  meaning not yet known */
U8(0x07),
/* +0x0d80  meaning not yet known */
AT(0xd80) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0da8  element 25 of 28: type 4 */
AT(0xda8) U32(0x00000004),  /* element 25: type */
/* +0x0dac  meaning not yet known */
U32(0x00004000), U32(0x000000b6), U8(0x22),
AT(0xdcc) U32(0x00000000),  /* element 25: parent, written at run time */
U32(0x00000000),  /* element 25: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0dd8  meaning not yet known */
U8(0x07),
/* +0x0e04  meaning not yet known */
AT(0xe04) U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000), U8(0x01),
/* +0x0e30  element 26 of 28: type 2 (text, 0xbc bytes) */
AT(0xe30) U32(0x00000002),  /* element 26: type */
/* +0x0e34  meaning not yet known */
U32(0xffffffff), U32(0x00000064), U8(0x82),
AT(0xe54) U32(0x00000000),  /* element 26: parent, written at run time */
U32(0x00000000),  /* element 26: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0e60  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x015f),
AT(0xee8) U32(0x00000000),  /* element 26: text buffer */
/* +0x0eec  meaning not yet known */
U32(0x00000000), U32(0x00000078), U32(0x00000018), U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00185d15), U16(0x5c5c), U8(0x5c),
AT(0xf28) STR12("Courier New"),
/* +0x0f48  meaning not yet known */
AT(0xf48) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x06),
/* +0x0f68  element 27 of 28: type 2 (text, 0xbc bytes) */
AT(0xf68) U32(0x00000002),  /* element 27: type */
/* +0x0f6c  meaning not yet known */
U32(0xffffffff), U32(0x00000064), U8(0x98),
AT(0xf8c) U32(0x00000000),  /* element 27: parent, written at run time */
U32(0x00000000),  /* element 27: no children */
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x0f98  meaning not yet known */
U32(0x00000006), U32(0x00000000), U32(0x00000000), U16(0x015f),
AT(0x1020) U32(0x00000000),  /* element 27: text buffer */
/* +0x1024  meaning not yet known */
U32(0x00000000), U32(0x00000078), U32(0x00000018), U32(0x00000000), U32(0x00000000), U32(0x00000000),
U32(0x00185d15), U16(0x5c5c), U8(0x5c),
AT(0x1060) STR12("Courier New"),
/* +0x1080  meaning not yet known */
AT(0x1080) U32(0x00000190), U32(0x00000001), U32(0x00000026), U8(0x06),
/* +0x10a0  the children of element 28, zero-terminated */
AT(0x10a0) U32(0x00000000),  /* element 28: child 1 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 2 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 3 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 4 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 5 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 6 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 7 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 8 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 9 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 10 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 11 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 12 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 13 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 14 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 15 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 16 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 17 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 18 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 19 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 20 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 21 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 22 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 23 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 24 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 25 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 26 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: child 27 (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* element 28: end of the list */
/* +0x1110  element 28 of 28: type 0 */
U32(0x00000000),  /* element 28: type */
/* +0x1114  meaning not yet known */
U16(0x1000),
AT(0x1134) U32(0x00000000),  /* element 28: parent, written at run time */
U32(0x00000000),  /* element 28: children list (relocated: installed at attach, rebind.inc) */
/* +0x113c  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x1160  the element-list slot (the header's +0x08 points here): the first element */
AT(0x1160) U32(0x00000000),  /* the first element (relocated: installed at attach, rebind.inc) */
/* +0x1164  meaning not yet known */
U32(0x00000000), U8(0x01),
/* +0x117c  meaning not yet known */
AT(0x117c) U32(0xffffffff), U16(0xa51c),
/* +0x1194  meaning not yet known */
AT(0x1194) U32(0x00000001), U32(0x000000ba), U32(0x00000029), U32(0x00000032), U32(0x00000032), U32(0xffffffff),
U16(0xa527),
/* +0x11c0  meaning not yet known */
AT(0x11c0) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000032), U32(0x00000032), U32(0x00000005),
U32(0x00000000), U32(0x20000000), U32(0x00000000),
U32(0x00000000),  /* relocated: installed at attach (rebind.inc) */
/* +0x11e8  meaning not yet known */
U32(0x00000000), U8(0x03),
/* +0x1200  meaning not yet known */
AT(0x1200) U32(0xffffffff),
/* +0x1218  meaning not yet known */
AT(0x1218) U8(0x03),
/* +0x122c  meaning not yet known */
AT(0x122c) U32(0xffffffff),
/* +0x1258  meaning not yet known */
AT(0x1258) U32(0xffffffff),
AT(0x1270) STR12(" %02d.%02d"),
STR12("-%02d.%02d"),
/* +0x1288  ImportTable: one import, PANELS (module id 0xf) */
U32(0x0000000f),  /* import: module id (PANELS) */
U32(0x00000000),  /* import: PANELS' service table, installed at load */
/* +0x1290  meaning not yet known */
U32(0x00000000), U32(0x00000000),
/* +0x1298  Linkage */
U32(0x00000013),  /* linkage: module id */
U32(0x00000000),  /* linkage +0x04: attach callback (relocated: installed at attach, rebind.inc) */
U32(0x00000000),  /* linkage +0x08: detach callback (relocated: installed at attach, rebind.inc) */
/* +0x12a4  meaning not yet known */
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
#undef STR24
