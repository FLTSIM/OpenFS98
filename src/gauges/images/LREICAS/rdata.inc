/* LREICAS.GAU's .rdata image, decompiled: every non-zero value of retail's .rdata (0xc2 bytes at RVA 0x2000) at
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
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
AT(0x0) F64(0x3f79000000000000ull),  /* double 0x1.9000000000000p-8 */
F64(0x3faae00000000000ull),  /* double 0x1.ae00000000000p-5 */
F64(0x408f380000000000ull),  /* double 0x1.f380000000000p+9 */
F64(0x0000000000000000ull),  /* double 0x0.0p+0 */
F64(0x3f6b800000000000ull),  /* double 0x1.b800000000000p-9 */
F64(0x3f81800000000000ull),  /* double 0x1.1800000000000p-7 */
F64(0x40c3878000000000ull),  /* double 0x1.3878000000000p+13 */
F64(0x3e80000000000000ull),  /* double 0x1.0000000000000p-23 */
F64(0x4070000000000000ull),  /* double 0x1.0000000000000p+8 */
F64(0x40b5540000000000ull),  /* double 0x1.5540000000000p+12 */
F64(0x40b5560000000000ull),  /* double 0x1.5560000000000p+12 */
F64(0x40c5558000000000ull),  /* double 0x1.5558000000000p+13 */
/* +0x0060  meaning not yet known */
U32(0x00000000), U32(0x33e94e70), U32(0x00000000), U32(0x0000209c), U32(0x00000001), U32(0x00000002),
U32(0x00000002), U32(0x00002088), U32(0x00002090), U32(0x00002098), U32(0x00005ef8), U32(0x00005f08),
U32(0x000020ae), U32(0x000020ba), U32(0x00010000),
S4("Lear_45.EICAS.gau", 0), S4("Lear_45.EICAS.gau", 4), S4("Lear_45.EICAS.gau", 8), S4("Lear_45.EICAS.gau", 12), SB("Lear_45.EICAS.gau", 16), SB("Lear_45.EICAS.gau", 17),
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
