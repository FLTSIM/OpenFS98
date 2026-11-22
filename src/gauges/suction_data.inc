/* CESSSUC.GAU's .data image, decompiled: every non-zero value of retail's .data (0x298 bytes at RVA 0x3000) at retail's offsets, one typed value per
 * macro.  Included by suction.c as the initialiser of suction_image.  The slots that hold addresses (listed in suction_pointers.inc) are zero here and
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
/* +0x0000  the gauge name, "Cessna_182.Suction", padded to 0x18 */
AT(0x0) STR24("Cessna_182.Suction"),
/* +0x0018  the header at +0x18: version word 0x262 (meaning not yet known) */
AT(0x18)
           U32(0x00000262),
/* +0x0040  the element and bitmap records (bitmap ids 0x1000 and 0x2000 at +0x184 and +0x94); internal pointers and the clamp pointer at +0xf4 are installed at attach; meaning not yet known */
AT(0x44)
           U32(0x0000001e),
AT(0x68)
           U32(0x00000048), U32(0x000000ca), U32(0x00000000), U32(0x40000000),
           U32(0x00000000), U32(0x00000000), U32(0x000000bd), U32(0x000000ca),
           U32(0x00000000), U32(0x40200000), U32(0x00000000), U32(0x00000000),
           U32(0x00000001), U32(0x00002000), U32(0x00000098), U32(0x000000b9),
           U32(0x00000000), U32(0x00000000), U32(0x0000001f), U32(0x00000011),
AT(0xc8)
           U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x00000161),
AT(0x100)
           U32(0x0000005f), U32(0x00000016), U32(0x00000000), U32(0x00000002),
           U32(0x00000006),
AT(0x184)
           U32(0x00001000),
AT(0x1b0)
           U32(0x00000001),
/* +0x01d0  element-root slot at +0x1d0 (installed at attach), the +0x1d8 header and the state words after it; meaning not yet known */
AT(0x1d8)
           U32(0x00000001),
AT(0x1ec)
           U32(0xffffffff), U32(0x0000a036),
AT(0x204)
           U32(0x00000003),
AT(0x218)
           U32(0xffffffff),
AT(0x244)
           U32(0xffffffff),
/* +0x0260  ImportTable (+0x260): the PANELS import, id 0xf; +0x264 takes PANELS' service table */
AT(0x260)
           U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0270  Linkage (+0x270): module id 0x13; init +0x274 and deinit +0x278 (installed at attach); header pointer at +0x288 (installed at attach) */
AT(0x270)
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
