/* EXVERT.GAU's .rdata image, decompiled: every non-zero value of retail's .rdata (0x8d bytes at RVA 0x2000) at
 * retail's offsets, one typed value per macro.  Included by src/gauges/gauge.h as the initialiser of gauge_rdata.  The constants the
 * gauge's code reads are F64 (doubles) or a U32 with a float comment; the rest is "meaning not yet known".  Zero runs of 16 bytes
 * or more are not written.
 *
 * Macros: AT(off) is the offset a disassembly shows; U8/U16/U32 emit one little-endian value; F64(bits) a double from its bit
 * pattern; STRn(s) is a string padded with NULs to n bytes and S4/SB its 4-byte and 1-byte pieces. */
#define AT(off) [(off)] =
#define U8(x)  (uint8_t)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define U32(x) U16(x), U16((x) >> 16)
#define F64(bits) U32((uint32_t)(bits)), U32((uint32_t)((bits) >> 32))
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
AT(0x0) F64(0x3fe89b367a0f9097ull),  /* double 0x1.89b367a0f9097p-1 */
F64(0x40b7700000000000ull),  /* double 0x1.7700000000000p+12 */
F64(0xc0b7700000000000ull),  /* double -0x1.7700000000000p+12 */
/* +0x0018  meaning not yet known */
U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x33e94e05), U32(0x00000000), U32(0x0000205c),
U32(0x00000001), U32(0x00000002), U32(0x00000002), U32(0x00002048), U32(0x00002050), U32(0x00002058),
U32(0x00003338), U32(0x00003348), U32(0x00002079), U32(0x00002085), U32(0x00010000),
S4("Extra-300.Vertical-Speed.gau", 0), S4("Extra-300.Vertical-Speed.gau", 4), S4("Extra-300.Vertical-Speed.gau", 8), S4("Extra-300.Vertical-Speed.gau", 12), S4("Extra-300.Vertical-Speed.gau", 16), S4("Extra-300.Vertical-Speed.gau", 20), S4("Extra-300.Vertical-Speed.gau", 24), SB("Extra-300.Vertical-Speed.gau", 28),
STR12("ImportTable"),
STR8("Linkage"),

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
