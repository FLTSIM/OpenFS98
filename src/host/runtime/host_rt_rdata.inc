/* FLTSIM98.EXE's .rdata, decompiled: its 0x200-byte raw size (virtual 0x190, then zero to the file alignment) at 0x416000, written one
 * typed value per macro.  Included by host_rt_exe.c (s_rdata) and host.c (fs98_host_rt_rdata, the standalone build) as the
 * initialiser of a byte array.  The values come from retail FLTSIM98.EXE (the reference, sha256
 * f47c124a1ad44475a8a3a5ecbce83f5dc5d6e7dad78cf120b20616738bd99f6a); the build checks them against a committed hash, and
 * `tools/ofs data-images` compares them with retail (`rdata=0x16000:0x200`, row HOST).  Addresses stay raw (the executable has no
 * relocations): U32 values in .text are retail's code addresses, those in 0x416000..0x41f000 its data addresses.  Everything else is
 * typed values marked "meaning not yet known"; strings are literals.
 *
 * Macros (no AT(): host_rt.h defines one, and the values run from +0 without a gap):
 * U8/U16/U32 emit one little-endian value; STRn(s) is a string padded with NULs to n bytes and S4/SB its 4-byte and 1-byte pieces (a flat byte list cannot hold a string literal, so the padded literal is indexed). */
#define U8(x)  (uint8_t)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define U32(x) U16(x), U16((x) >> 16)
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)

/* +0x0000  meaning not yet known */
           U32(0x00401040), U32(0x00416008), U32(0x00000000), U32(0x00000000),  /* retail .text address, kept raw; retail data address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00414fcc), U32(0x00401080), U32(0x004025c0), U32(0x004025b0),  /* retail .text address, kept raw */
           U32(0x004025c0), U32(0x00414fc6), U32(0x00414fc0), U32(0x00414fba),  /* retail .text address, kept raw */
           U32(0x00414fb4), U32(0x00414fae), U32(0x00414fa8), U32(0x00414fa2),  /* retail .text address, kept raw */
           U32(0x00401050), U32(0x00414f9c), U32(0x00414f96), U32(0x00414f90),  /* retail .text address, kept raw */
           U32(0x00414f8a), U32(0x00414f84), U32(0x00414f7e), U32(0x00414f78),  /* retail .text address, kept raw */
           U32(0x00414f72), U32(0x00414f6c), U32(0x00401110), U32(0x00401120),  /* retail .text address, kept raw */
           U32(0x00414f66), U32(0x00414f60), U32(0x00414f5a), U32(0x00414f54),  /* retail .text address, kept raw */
           U32(0x004010f0), U32(0x00414f4e), U32(0x00414f48), U32(0x00414f42),  /* retail .text address, kept raw */
           U32(0x00414f3c), U32(0x00414f36), U32(0x00414f30), U32(0x00414f2a),  /* retail .text address, kept raw */
           U32(0x00414f24), U32(0x00414f1e), U32(0x00414f18), U32(0x00414f12),  /* retail .text address, kept raw */
           U32(0x00414f0c), U32(0x00000000), U32(0x2f092600), U32(0x320b2802),  /* retail .text address, kept raw */
           U32(0x16230c19), U32(0x18250f1b), U32(0x29032c06), U32(0x2b052f08),
           U32(0x0f1c131f), U32(0x121f1522), U32(0x310b2701), U32(0x300a2700),
           U32(0x17240e1b), U32(0x17230d1a), U32(0x2b042e07), U32(0x2a032d07),
           U32(0x111e1421), U32(0x101d1320), U32(0x00000000), U32(0x00000000),
           U32(0xffffffff), U32(0x00412172), U32(0x0041217c), U32(0x00000000),  /* retail .text address, kept raw */
           STR12("HARDWARE"),
           U32(0x00000000),
/* +0x0130  four 16-byte GUIDs, as host.c names them (host_iid_d3d, host_iid_dplay2a, host_guid_mmx, host_guid_hal) */
           U32(0x6aae1ec1), U32(0x11d0662a), U32(0xaa009d88), U32(0x6ab7bb00),  /* IID_IDirect3D, 0x416130 */
           U32(0x133efe41), U32(0x11d032dc), U32(0xa000fb9c), U32(0xcb430ac9),  /* IID_IDirectPlay2A, 0x416140 */
           U32(0x881949a1), U32(0x11d0d6f3), U32(0xa000ab89), U32(0x294105c9),  /* the Direct3D MMX device GUID, 0x416150 */
           U32(0x84e63de0), U32(0x11cf46aa), U32(0x00006f81), U32(0x6e1520c0),  /* the Direct3D HAL device GUID, 0x416160 */
           U32(0xffffffff), U32(0x00414e42), U32(0x00414e57), U32(0x00000000),  /* retail .text address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x0190  zero: past the virtual size, the file pads .rdata to 0x200 */
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),

#undef U8
#undef U16
#undef U32
#undef NUL_PAD
#undef SB
#undef S4
#undef STR12
