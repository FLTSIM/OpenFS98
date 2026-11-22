/* 737WARNG.GAU's .data image, decompiled: every non-zero value of retail's .data (0xfc bytes at RVA 0x3000) at retail's offsets, one typed value per
 * macro.  Included by warning.c as the initialiser of warning_image.  The slots that hold addresses (listed in warning_pointers.inc) are zero here and
 * are installed at attach.  Zero runs of 16 bytes or more are not written.  Regions are named from docs/modules/gauges.md and the C that
 * reads them; the rest is marked "meaning not yet known".
 *
 * Macros: AT(off) is the offset a disassembly shows; U32 emits one little-endian value; STRn(s) is a string padded with NULs to n bytes. */
#define AT(off) [(off)] =
#define U8(x)  (uint8_t)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define U32(x) U16(x), U16((x) >> 16)
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
#define STR32(s) STR24(s), S4(s, 24), S4(s, 28)
/* +0x0000  the gauge name, "737-400.Warning_Enunciators", padded to 0x20 */
AT(0x0) STR32("737-400.Warning_Enunciators"),
/* +0x0020  the header at +0x20: version word 0x262 (meaning not yet known) */
AT(0x20)
           U32(0x00000262),
/* +0x0048  state word at +0x4c, 0x14b; meaning not yet known (the warning-noop break flips it) */
AT(0x4c)
           U32(0x0000014b),
/* +0x0070  the element record at +0x70 and the bitmap record (bitmap id 0x1000 at +0x74); meaning not yet known */
AT(0x74)
           U32(0x00001000),
AT(0xa0)
           U32(0x00000011),
/* +0x00c8  ImportTable (+0xc8): the PANELS import, id 0xf; +0xcc takes PANELS' service table */
AT(0xc8)
           U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x00d8  Linkage (+0xd8): module id 0x13; init +0xdc and deinit +0xe0 (installed at attach); header pointer at +0xf0 (installed at attach) */
AT(0xd8)
           U32(0x00000013),

#undef AT
#undef U8
#undef U16
#undef U32
#undef NUL_PAD
#undef SB
#undef S4
#undef STR24
#undef STR32
