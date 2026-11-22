/* MAIN's .data image, decompiled: every non-zero value of retail's 0x2ee00-byte data section (0x8000 bytes on disk, the rest zero)
 * at retail's offsets, written one typed value per macro.  Included by main.c as the initialiser of main_image, a byte array so
 * that the D/V()/M*() accesses stay as they are.  The values come from retail MAIN.DLL (image base 0x207e0000, .text at 0x207e1000
 * for 0x1d360 bytes, .data at 0x20800000 for 0x2ee00 bytes); the build checks the image against a committed hash, and
 * `tools/ofs data-images` compares the whole image with retail with `--mixed` (tools/check_data_image.py, row MAIN).
 *
 * Retail relocates 635 words of it.  The 455 pointers into the image itself (listed in main_pointers.inc, MAIN_DATA_POINTER) and
 * the 47 Linkage slots (+0xc4, +0xc8, +0xd8..+0x188, set at attach; `cset=` in the manifest row) read zero here.  The other 133 are
 * retail's raw addresses into its own .text (92 distinct, MAIN_CODE_TARGET in main_pointers.inc), kept as raw U32 values.  Two kinds:
 * the dialog callbacks in the descriptors, which main_install() overwrites with our own routines at attach, before anything outside
 * MAIN reads them (so retail's value is never used); and the addresses main.c interprets at call time, mapping a retail address to
 * its own routine (table_call(), retail_callbacks[], the routines MAIN's G3D programs call).
 *
 * The tables whose structure is known are named and typed (the ImportTable, the Linkage, the month lengths and season boundaries,
 * the situation records); everything else is typed values (U32 where aligned, else U16 or U8) marked "meaning not yet known", with
 * the strings as literals.  Zero runs of 16 bytes or more are not written.
 *
 * Checks the generator made: the ImportTable is the 23 module ids above and a 0 end mark with all its links zero, the Linkage's
 * module id (+0xc0) is 3, the situation table holds 77 records and its terminator, and the relocations are 455 data pointers,
 * 47 Linkage slots and 133 kept code addresses (180 code pointers in all).
 *
 * Macros: AT(off) is the offset a disassembly shows; U8/U16/U32 emit one little-endian value; STRn(s) is a string padded with NULs
 * to n bytes and S4/SB its 4-byte and 1-byte pieces (a flat byte list cannot hold a string literal, so the padded literal is
 * indexed). */
#define AT(off) [(off)] =
#define U8(x)  (uint8_t)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define U32(x) U16(x), U16((x) >> 16)
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR4(s) S4(s, 0)
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
#define STR20(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
#define STR28(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24)
#define STR32(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28)

/* +0x0000  ImportTable: (module id wanted, link the host fills in) x 23, then a 0 end mark.  Ids 1, 2, 23, 21, 29, 24, 4, 16, 7, 6, 37, 18, 13, 14, 26, 35, 17, 15, 39, 9, 20, 27, 10 (the host, GLOBAL and every module but BOOSTER, WINMSG, REALITY and MPCHAT) */
AT(0x0000) U32(0x00000001), U32(0x00000000), U32(0x00000002), U32(0x00000000), U32(0x00000017), U32(0x00000000),
           U32(0x00000015), U32(0x00000000), U32(0x0000001d), U32(0x00000000), U32(0x00000018), U32(0x00000000),
           U32(0x00000004), U32(0x00000000), U32(0x00000010), U32(0x00000000), U32(0x00000007), U32(0x00000000),
           U32(0x00000006), U32(0x00000000), U32(0x00000025), U32(0x00000000), U32(0x00000012), U32(0x00000000),
           U32(0x0000000d), U32(0x00000000), U32(0x0000000e), U32(0x00000000), U32(0x0000001a), U32(0x00000000),
           U32(0x00000023), U32(0x00000000), U32(0x00000011), U32(0x00000000), U32(0x0000000f), U32(0x00000000),
           U32(0x00000027), U32(0x00000000), U32(0x00000009), U32(0x00000000), U32(0x00000014), U32(0x00000000),
           U32(0x0000001b), U32(0x00000000), U32(0x0000000a), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x00c0  Linkage: module id 3, then 50 slots.  The 47 relocated ones (+0xc4, +0xc8, +0xd8..+0x188: the services of docs/modules/main.md) are zero here and set at attach (`cset=` in the manifest row); retail's values are MAIN_LINKAGE_RETAIL in main_pointers.inc */
           U32(0x00000003), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),
           STR8("118.000"),
           STR8("136.975"),
           U32(0x00000000), U32(0x207e1910),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x7fa8, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x0190, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0198, installed from main_pointers.inc */
/* +0x01e0  meaning not yet known */
AT(0x01e0) U32(0x00000004), U32(0x0000000c), U32(0x00001012), U32(0x00000000),  /* -> +0x7fb8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000004), U32(0x0000000c),
           U32(0x00001012), U32(0x0000034e), U32(0x00000001), U32(0x00000000),
           U32(0x00000020), U32(0x00002002), U32(0x0000027c), U32(0x00000001),
           U32(0x00000000), U32(0x00000028), U32(0x00002002), U32(0x0000038a),
           U32(0x00000001), U32(0x00000001), U32(0x00000030), U32(0x00002005),
           U32(0x00000000),  /* -> +0x7fb4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000002), U32(0x00000038), U32(0x00002009),
           U32(0x00000362), U32(0x00000001),
           STR8("PREFS"),
           STR8("lists"),
           STR12("%d,%d,%d"),
           STR12("CHECKLISTS"),
           STR12("FS6BOUNDS"),
           U32(0x00000030),
           STR8("65535"),
           U32(0x00000031), U32(0x00003231), U32(0x00000031), U32(0x00003832),
           U32(0x00003932), U32(0x00003033), U32(0x00003133), U32(0x00000030),
           U32(0x00003332), U32(0x00000030), U32(0x00003935),
/* +0x02c8  days in each month, a common year (read by main_clockdlg.inc's date_table) */
           U16(0x001f), U16(0x001c), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f),
/* +0x02e0  days in each month, a leap year */
           U16(0x001f), U16(0x001d), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f),
/* +0x02f8  the day of the year each season starts on (80, 172, 264, 355), then 0xffff (main_clockdlg.inc) */
           U16(0x0050), U16(0x00ac), U16(0x0108), U16(0x0163), U16(0xffff),
           U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x207e2df0),  /* retail code address, kept raw */
/* +0x0328  meaning not yet known */
AT(0x0328) U32(0x00000000),  /* -> +0x8014, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x029c, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x02a0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x801c, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x02a4, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x02b4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8024, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x0290, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0294, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x802c, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0x8038, installed from main_pointers.inc */
/* +0x039c  meaning not yet known */
AT(0x039c) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x00000000),  /* -> +0x8048, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x02b8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x02bc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8050, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x02c0, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x02c4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8058, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x02b8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x02bc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8060, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x02c0, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x02c4, installed from main_pointers.inc */
/* +0x0418  meaning not yet known */
AT(0x0418) U32(0x207e2f70),  /* retail code address, kept raw */
           U32(0x207e31a0),  /* retail code address, kept raw */
           U32(0x207e33c0),  /* retail code address, kept raw */
           U32(0x207e3460),  /* retail code address, kept raw */
           U32(0x207e3600),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000001), U32(0x00000014), U32(0x00002007),
           U32(0x00000000),  /* -> +0x7ffc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000044), U32(0x00011000),
           U32(0x00000000),  /* -> +0x7fe0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000058), U32(0x00001000),
           U32(0x00000000),  /* -> +0x8010, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000001), U32(0x00000098), U32(0x00002007),
           U32(0x00000000),  /* -> +0x8000, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000001), U32(0x000000a8), U32(0x00002009),
           U32(0x00000000),  /* -> +0x7ff0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000b0), U32(0x00011007),
           U32(0x00000000),  /* -> +0x8004, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000c4), U32(0x00011007),
           U32(0x00000000),  /* -> +0x7fd8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000d8), U32(0x00011007),
           U32(0x00000000),  /* -> +0x7fe8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000ec), U32(0x00011007),
           U32(0x00000000),  /* -> +0x7fe4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000001), U32(0x0000006c), U32(0x00002009),
           U32(0x00000000),  /* -> +0x800c, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000100), U32(0x00002002),
           U32(0x00000000),  /* -> +0x8008, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000002), U32(0x00000108), U32(0x0000200a),
           U32(0x00000000),  /* -> +0x7fec, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000001c), U32(0x00011007),
           U32(0x00000000),  /* -> +0x7ff4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000030), U32(0x00011000),
           U32(0x00000000),  /* -> +0x7fdc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000001), U32(0x000000a0), U32(0x00002009),
           U32(0x00000000),  /* -> +0x7ff8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000004), U32(0x00000044),
           U32(0x00011000), U32(0x00000240), U32(0x00000001), U32(0x00000004),
           U32(0x00000058), U32(0x00001000), U32(0x0000023e), U32(0x00000001),
           U32(0x00000001), U32(0x0000006c), U32(0x00002006), U32(0x00000248),
           U32(0x00000001), U32(0x00000008), U32(0x00000078), U32(0x00002000),
           U32(0x00000c20), U32(0x00000001), U32(0x00000001), U32(0x000000a8),
           U32(0x00002009), U32(0x0000116e), U32(0x00000001), U32(0x00000004),
           U32(0x000000b0), U32(0x00011007), U32(0x00000238), U32(0x00000001),
           U32(0x00000004), U32(0x000000c4), U32(0x00011007), U32(0x00000239),
           U32(0x00000001), U32(0x00000004), U32(0x000000d8), U32(0x00011007),
           U32(0x0000023b), U32(0x00000001), U32(0x00000004), U32(0x000000ec),
           U32(0x00011007), U32(0x0000023c), U32(0x00000001), U32(0x00000002),
           U32(0x00000108), U32(0x0000200a), U32(0x0000023a), U32(0x00000001),
           U32(0x00000002), U32(0x00000090), U32(0x00002009), U32(0x00000244),
           U32(0x00000001), U32(0x00000000), U32(0x2428221a), U32(0x1d241b15),
           U32(0x171e150f), U32(0x121b1008), U32(0x0d170b00), U32(0x091207f8),
           U32(0x040c02f1), U32(0xfc06fbe7), U32(0xf600f5dd),
           STR32("%2.2d/%2.2d/%2.2d  %2.2d:%2.2d"),
           U32(0x00000000), U32(0x00000000), U32(0x207e3ec0),  /* retail code address, kept raw */
/* +0x06bc  meaning not yet known */
AT(0x06bc) U32(0x00000008), U32(0x00002002), U32(0x00001180), U32(0x00000001),
           U32(0x00000000), U32(0x00000010), U32(0x00002002), U32(0x00001184),
           U32(0x00000001), U32(0x00000000), U32(0x00000018), U32(0x00002002),
           U32(0x00001188), U32(0x00000001), U32(0x00000000), U32(0x00000020),
           U32(0x00002002), U32(0x0000118c), U32(0x00000001), U32(0x00000001),
           U32(0x00000028), U32(0x00002005), U32(0x00001190), U32(0x00000001),
           U32(0x00000000),
           STR4("001"),
           STR4("360"),
           U32(0x00000030), U32(0x00000000),
           STR4("999"),
           U32(0x00000000), U32(0x00000030), U32(0x00000000),
           STR8("1.50"),
           U32(0x00000000), U32(0x76f20000), U32(0x1b58e890), U32(0x0856f9a4),
           U32(0x00000000), U32(0x03de0000), U32(0x00000000), U32(0x00018000),
           U32(0x00000000), U32(0x207e4450),  /* retail code address, kept raw */
/* +0x0788  meaning not yet known */
AT(0x0788) U32(0x00000000),  /* -> +0xdbc0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x0720, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0724, installed from main_pointers.inc */
/* +0x07b4  meaning not yet known */
AT(0x07b4) U32(0x00000000),  /* -> +0xdbd8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x8168, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8170, installed from main_pointers.inc */
/* +0x07e4  meaning not yet known */
AT(0x07e4) U32(0x00000000),  /* -> +0xdbe0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x0728, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0730, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0xdbc8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x0738, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0740, installed from main_pointers.inc */
/* +0x082c  meaning not yet known */
AT(0x082c) U32(0x00000000),  /* -> +0xdbd0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x8178, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8180, installed from main_pointers.inc */
/* +0x0854  meaning not yet known */
AT(0x0854) U32(0x0000000c), U32(0x00002002), U32(0x000007bc), U32(0x00000001),
           U32(0x00000001), U32(0x00000014), U32(0x00002001), U32(0x207e4900),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x0000001c), U32(0x00002001),
           U32(0x207e4880),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000040), U32(0x00002002),
           U32(0x000007d0), U32(0x00000001), U32(0x00000004), U32(0x00000048),
           U32(0x00002001), U32(0x207e47f0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x000000d4), U32(0x00002002),
           U32(0x00000808), U32(0x00000001), U32(0x00000001), U32(0x000000dc),
           U32(0x00002001), U32(0x207e4a50),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000004), U32(0x00000048),
           U32(0x0000103f), U32(0x000007d4), U32(0x00000001), U32(0x00000000),
           U32(0x00000004), U32(0x0000001c), U32(0x0000104b), U32(0x000007cc),
           U32(0x00000001), U32(0x00000000), U32(0x00000004), U32(0x00000048),
           U32(0x0001103f), U32(0x000007d4), U32(0x00000001),
           STR8("%4.2lf"),
           U32(0x00000000), U32(0x4a1034f0), U32(0x00000000), U32(0x00000030),
           U32(0x00000000), U32(0x00003000), U32(0x00000000), U32(0x00300000),
           U32(0x00000000), U32(0x30000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000030), U32(0x00000000), U32(0x00000000),
           STR4("100"),
           U32(0x00000000), U8(0x00),
           STR4("100"),
           U8(0x00), U16(0x0000), U16(0x0000),
           STR4("100"),
           U16(0x0000), U16(0x0000), U8(0x00),
           STR4("100"),
           U8(0x00), U32(0x00000000),
           STR4("100"),
           U32(0x00000000), U32(0x00000000), U32(0x00000030), U32(0x00000000),
           U32(0x00003000), U32(0x00000000), U32(0x00300000), U32(0x00000000),
           U32(0x30000000), U32(0x00000000), U32(0x00000000), U32(0x00000030),
           U32(0x00000000), U32(0x00000000), U32(0x00000030), U32(0x00000000),
           U32(0x00003000), U32(0x00000000), U32(0x00300000), U32(0x00000000),
           U32(0x30000000), U32(0x00000000), U32(0x00000000), U32(0x00000030),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x207e5010),  /* retail code address, kept raw */
/* +0x0a14  meaning not yet known */
AT(0x0a14) U32(0x0000ffff),
/* +0x0a30  meaning not yet known */
AT(0x0a30) U32(0x00000064),
/* +0x0a5c  meaning not yet known */
AT(0x0a5c) U32(0x00000000),  /* -> +0x81b0, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x81c0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8200, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x8280, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8290, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8270, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x82d0, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8350, installed from main_pointers.inc */
           U32(0x207e5a80),  /* retail code address, kept raw */
           U32(0x207e5ac0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000064), U32(0x00000000),
           U32(0x00000064),
/* +0x0ac0  meaning not yet known */
AT(0x0ac0) U32(0x00000001),
/* +0x0aec  meaning not yet known */
AT(0x0aec) U32(0x00000003), U32(0x00000000), U32(0x00000003), U32(0x00000000),
           U32(0x00000003), U32(0x00000000), U32(0x00000003), U32(0x00000001),
/* +0x0b3c  meaning not yet known */
AT(0x0b3c) U32(0x00000001),
/* +0x0b50  meaning not yet known */
AT(0x0b50) U32(0x00000001),
/* +0x0b6c  meaning not yet known */
AT(0x0b6c) U32(0x00000001), U32(0x00000000),  /* -> +0x8340, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x82e0, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x0938, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0968, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x82e9, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x0941, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0971, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8304, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x095c, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x098c, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x82f2, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x094a, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x097a, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x82fb, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x0953, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x0983, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x81d0, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x0998, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x82a0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x81d9, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09a1, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x82a9, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x81f4, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09bc, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x82c4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x81e2, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09aa, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x82b2, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x81eb, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09b3, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x82bb, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8210, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09c8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8310, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8219, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09d1, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8319, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8234, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09ec, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8334, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8222, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09da, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8322, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x822b, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x09e3, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x832b, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8240, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8249, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8264, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x8252, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x825b, installed from main_pointers.inc */
           U32(0x207e5c40),  /* retail code address, kept raw */
           U32(0x207e5ca0),  /* retail code address, kept raw */
           U32(0x207e5df0),  /* retail code address, kept raw */
/* +0x0d48  meaning not yet known */
AT(0x0d48) U32(0x00000001), U32(0x0000002c), U32(0x00002009), U32(0x00000c44),
           U32(0x00000001), U32(0x00000000), U32(0x0000003c), U32(0x00002002),
           U32(0x00000278), U32(0x00000001), U32(0x00000000), U32(0x00000044),
           U32(0x00002003), U32(0x0000033c), U32(0x00000001), U32(0x00000000),
           U32(0x0000004c), U32(0x42200002), U32(0x00000c46), U32(0x00000001),
           U32(0x00000000), U32(0x0000005c), U32(0x42200020), U32(0x00000c46),
           U32(0x00000001), U32(0x00000008), U32(0x00000064), U32(0x00001025),
           U32(0x00000ec6), U32(0x00000001), U32(0x00000008), U32(0x00000068),
           U32(0x00001024), U32(0x00000ec6), U32(0x00000001), U32(0x00000004),
           U32(0x0000006c), U32(0x00001025), U32(0x00000330), U32(0x00000001),
           U32(0x00000004), U32(0x00000080), U32(0x00001024), U32(0x00000330),
           U32(0x00000001), U32(0x00000001), U32(0x000000a0), U32(0x00002009),
           U32(0x00000372), U32(0x00000001), U32(0x00000000), U32(0x000000b0),
           U32(0x42200008), U32(0x00000c46), U32(0x00000001), U32(0x00000000),
           U32(0x000000b8), U32(0x42200004), U32(0x00000c46), U32(0x00000001),
           U32(0x00000001), U32(0x000000c4), U32(0x00002006), U32(0x00000830),
           U32(0x00000001), U32(0x00000000), U32(0x000000cc), U32(0x00002003),
           U32(0x00000338), U32(0x00000001), U32(0x00000000), U32(0x000000d4),
           U32(0x00002003), U32(0x00000846), U32(0x00000001), U32(0x00000000),
           U32(0x000000dc), U32(0x00002003), U32(0x00000848), U32(0x00000001),
           U32(0x00000000), U32(0x000000e4), U32(0x00002003), U32(0x0000084a),
           U32(0x00000001), U32(0x00000001), U32(0x000000f0), U32(0x00002006),
           U32(0x00000892), U32(0x00000001), U32(0x00000001), U32(0x000000f8),
           U32(0x00002006), U32(0x0000092a), U32(0x00000001), U32(0x00000001),
           U32(0x00000100), U32(0x00002006), U32(0x000009c2), U32(0x00000001),
           U32(0x00000001), U32(0x00000108), U32(0x00002006), U32(0x00000a5a),
           U32(0x00000001), U32(0x00000000), U32(0x00000134), U32(0x00002001),
           U32(0x207e5160),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000114), U32(0x44400001),
           U32(0x00000888), U32(0x00000001), U32(0x00000000), U32(0x0000011c),
           U32(0x44400002), U32(0x00000888), U32(0x00000001), U32(0x00000000),
           U32(0x00000124), U32(0x44400004), U32(0x00000888), U32(0x00000001),
           U32(0x00000000), U32(0x0000012c), U32(0x44400008), U32(0x00000888),
           U32(0x00000001), U32(0x00000000), U32(0x0000013c), U32(0x00002003),
           U32(0x00000340), U32(0x00000001), U32(0x00000000), U32(0x00000148),
           U32(0x00002003), U32(0x00000342), U32(0x00000001), U32(0x00000000),
           U32(0x00000150), U32(0x00002003), U32(0x0000033e), U32(0x00000001),
           U32(0x00000001), U32(0x0000015c), U32(0x00002005), U32(0x00000af0),
           U32(0x00000001), U32(0x00000000), U32(0x00000168), U32(0x00002003),
           U32(0x0000033a), U32(0x00000001), U32(0x00000001), U32(0x00000170),
           U32(0x00002005), U32(0x00000af8), U32(0x00000001), U32(0x00000004),
           U32(0x0000017c), U32(0x00002001), U32(0x207e5240),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00000190), U32(0x00002001),
           U32(0x207e5530),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x000001cc), U32(0x00002001),
           U32(0x207e5550),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x000001a4), U32(0x00002001),
           U32(0x207e5570),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x000001b8), U32(0x00002001),
           U32(0x207e5590),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x000001e0), U32(0x00002001),
           U32(0x207e5640),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x000001f4), U32(0x00002001),
           U32(0x207e5750),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00000230), U32(0x00002001),
           U32(0x207e5770),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00000208), U32(0x00002001),
           U32(0x207e5790),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x0000021c), U32(0x00002001),
           U32(0x207e57b0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00000244), U32(0x00002001),
           U32(0x207e5890),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00000258), U32(0x00002001),
           U32(0x207e5a00),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00000294), U32(0x00002001),
           U32(0x207e5a20),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x0000026c), U32(0x00002001),
           U32(0x207e5a40),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00000280), U32(0x00002001),
           U32(0x207e5a60),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x000002cc), U32(0x00002004),
           U32(0x00000b68), U32(0x00000001), U32(0x00000000), U32(0x000002d4),
           U32(0x00002004), U32(0x00000b70), U32(0x00000001), U32(0x00000000),
           U32(0x000002dc), U32(0x00002004), U32(0x00000b64), U32(0x00000001),
           U32(0x00000000), U32(0x000002e4), U32(0x00002004), U32(0x00000b6f),
           U32(0x00000001), U32(0x00000000), U32(0x000002ec), U32(0x00002004),
           U32(0x00000b6c), U32(0x00000001), U32(0x00000000), U32(0x000002f4),
           U32(0x00002004), U32(0x00000b6b), U32(0x00000001), U32(0x00000000),
           U32(0x000002fc), U32(0x00002004), U32(0x00000b73), U32(0x00000001),
           U32(0x00000000), U32(0x00000304), U32(0x00002004), U32(0x00000b6a),
           U32(0x00000001), U32(0x00000000), U32(0x0000030c), U32(0x00002004),
           U32(0x00000b71), U32(0x00000001), U32(0x00000001), U32(0x00000314),
           U32(0x00002007), U32(0x00000b65), U32(0x00000001), U32(0x00000001),
           U32(0x0000031c), U32(0x00002007), U32(0x00000b67), U32(0x00000001),
           U32(0x00000001), U32(0x00000324), U32(0x00002007), U32(0x00000b66),
           U32(0x00000001), U32(0x00000001), U32(0x0000032c), U32(0x00002007),
           U32(0x00000b6d), U32(0x00000001), U32(0x00000001), U32(0x00000334),
           U32(0x00002007), U32(0x00000b72), U32(0x00000001), U32(0x00000001),
           U32(0x0000033c), U32(0x00002007), U32(0x00000b6e), U32(0x00000001),
           U32(0x00000001), U32(0x00000344), U32(0x00002007), U32(0x00000b69),
           U32(0x00000001), U32(0x00000030), U32(0x00000000), U32(0x207e5ff0),  /* retail code address, kept raw */
/* +0x1264  meaning not yet known */
AT(0x1264) U32(0x00000008), U32(0x00002003), U32(0x00000b54), U32(0x00000001),
           U32(0x00000000), U32(0x00000010), U32(0x00002002), U32(0x00000b58),
           U32(0x00000001), U32(0x00000000), U32(0x00000018), U32(0x00002002),
           U32(0x00000b5c), U32(0x00000001), U32(0x00000001), U32(0x00000020),
           U32(0x00002005), U32(0x00000b60), U32(0x00000001), U32(0x00000000),
           U32(0x00000000), U32(0x00000000),  /* -> +0xd960, installed from main_pointers.inc */
           U32(0x0000001d), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdb80, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xd9c0, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdac0, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xda40, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xda80, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1854, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0xda00, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1850, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0xdb40, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdb00, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1850, installed from main_pointers.inc */
/* +0x1378  meaning not yet known */
AT(0x1378) U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0xd97e, installed from main_pointers.inc */
           U32(0x0000001d), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdb95, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xd9d5, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdad5, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xda55, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xda95, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1854, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0xda15, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1850, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0xdb55, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdb15, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1850, installed from main_pointers.inc */
/* +0x1440  meaning not yet known */
AT(0x1440) U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0xd99c, installed from main_pointers.inc */
           U32(0x0000001d), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdbaa, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xd9ea, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdaea, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xda6a, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdaaa, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1854, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0xda2a, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1850, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0xdb6a, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0xdb2a, installed from main_pointers.inc */
           U32(0x00000014), U32(0x00000000),  /* -> +0x1234, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x1850, installed from main_pointers.inc */
/* +0x150c  meaning not yet known */
AT(0x150c) U32(0x207e6e20),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x12b0, installed from main_pointers.inc */
           U32(0x00000001), U32(0x00000001), U32(0x207e7050),  /* retail code address, kept raw */
           U32(0x207e70a0),  /* retail code address, kept raw */
           U32(0x207e7190),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x207e1680),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000001), U32(0x00000000),  /* -> +0x12b0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000001),
/* +0x1590  meaning not yet known */
AT(0x1590) U32(0x00000007), U32(0x0000000c), U32(0x00002005), U32(0x00000f18),
           U32(0x00000001), U32(0x00000000), U32(0x00000004), U32(0x00000004),
           U32(0x0000200e), U32(0x00000000),  /* -> +0x8550, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000090), U32(0x0000100e),
           U32(0x00000000),  /* -> +0x85ac, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x000000b8), U32(0x00002004),
           U32(0x00000f12), U32(0x00000001), U32(0x00000000), U32(0x000000c0),
           U32(0x00002004), U32(0x00000f13), U32(0x00000001), U32(0x00000004),
           U32(0x00000018), U32(0x00001015), U32(0x00000000),  /* -> +0x8360, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000002c), U32(0x00001014),
           U32(0x00000000),  /* -> +0x8368, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000040), U32(0x00001015),
           U32(0x00000000),  /* -> +0x8370, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000054), U32(0x00001014),
           U32(0x00000000),  /* -> +0x8378, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000068), U32(0x00001022),
           U32(0x00000000),  /* -> +0x8380, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000007c), U32(0x00001021),
           U32(0x00000000),  /* -> +0x8384, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000a4), U32(0x00001021),
           U32(0x00000000),  /* -> +0x8388, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000004), U32(0x00000004),
           U32(0x0000200e), U32(0x00000fe2), U32(0x00000001), U32(0x00000004),
           U32(0x00000090), U32(0x0000100e), U32(0x00000fbc), U32(0x00000001),
           U32(0x00000000), U32(0x000000b8), U32(0x00002003), U32(0x00000fc0),
           U32(0x00000001), U32(0x00000000), U32(0x000000c0), U32(0x00002003),
           U32(0x00000fc2), U32(0x00000001), U32(0x00000004), U32(0x00000018),
           U32(0x00001015), U32(0x00000000),  /* -> +0x838c, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000002c), U32(0x00001014),
           U32(0x00000000),  /* -> +0x8394, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000040), U32(0x00001015),
           U32(0x00000000),  /* -> +0x839c, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000054), U32(0x00001014),
           U32(0x00000000),  /* -> +0x83a4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000068), U32(0x00001022),
           U32(0x00000000),  /* -> +0x83ac, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000007c), U32(0x00001021),
           U32(0x00000000),  /* -> +0x83b0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000a4), U32(0x00001021),
           U32(0x00000000),  /* -> +0x83b4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000004), U32(0x00000004),
           U32(0x0000200e), U32(0x000010ca), U32(0x00000001), U32(0x00000004),
           U32(0x00000090), U32(0x0000100e), U32(0x000010a4), U32(0x00000001),
           U32(0x00000000), U32(0x000000b8), U32(0x00002003), U32(0x000010a8),
           U32(0x00000001), U32(0x00000000), U32(0x000000c0), U32(0x00002003),
           U32(0x000010aa), U32(0x00000001), U32(0x00000004), U32(0x00000018),
           U32(0x00001015), U32(0x00000000),  /* -> +0x83b8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000002c), U32(0x00001014),
           U32(0x00000000),  /* -> +0x83c0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000040), U32(0x00001015),
           U32(0x00000000),  /* -> +0x83c8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000054), U32(0x00001014),
           U32(0x00000000),  /* -> +0x83d0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000068), U32(0x00001022),
           U32(0x00000000),  /* -> +0x83d8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000007c), U32(0x00001021),
           U32(0x00000000),  /* -> +0x83dc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000a4), U32(0x00001021),
           U32(0x00000000),  /* -> +0x83e0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x07d00000), U32(0x00630000), U32(0x00630000),
           U32(0x00003939),
           STR8("2000"),
           U32(0x00000000), U32(0x000000b6), U32(0x00000406),
/* +0x192e  meaning not yet known */
AT(0x192e) U16(0x0089), U32(0x00000000),  /* -> +0x1860, installed from main_pointers.inc */
           U32(0x0010004c), U32(0x00010000), U32(0x00000000), U32(0x00080001),
           U32(0x00890022), U32(0x00000000),  /* -> +0x1860, installed from main_pointers.inc */
           U32(0x0016004c), U32(0x00000000), U32(0x00000000), U32(0x00080001),
           U32(0x0008003b), U32(0x00220020), U32(0x00030003), U32(0x000e00c6),
           U32(0x00d6000e), U32(0x001a0046), U32(0x00040096), U32(0x00140019),
           U32(0xffe70000), U32(0x00000014), U32(0xffecffe7), U32(0x00190000),
           U32(0x0000ffec), U32(0xf00f0051), U32(0x0096000f), U32(0x00970010),
           U32(0x00980010), U32(0x00990010), U32(0x00960010), U32(0x001a0022),
           U32(0x000a0096), U32(0x0000000a), U32(0xfff60023), U32(0x00230000),
           U32(0x0000fff6), U32(0x000affdd), U32(0xffdd0000), U32(0x00000000),
           U32(0x00000010), U32(0x00080000), U32(0x00000000), U32(0x00000004),
           U32(0xfffc0000), U32(0x00000000), U32(0x0000fff8), U32(0xfff00000),
           U32(0xf0140052), U32(0xf0000051), U32(0x0004003e), U32(0x7fff0000),
           U32(0x00000000), U32(0x00960000), U32(0x00980097), U32(0x003e0099),
           U32(0x00000004), U32(0x00008001), U32(0x00000000), U32(0x00970096),
           U32(0x00990098), U32(0x009a000f), U32(0x009b0010), U32(0x009c000f),
           U32(0x009d0010), U32(0x009e000f), U32(0x009f0010), U32(0x001a0022),
           U32(0x00040096), U32(0x0005000a), U32(0xfff60000), U32(0x00000005),
           U32(0x00050000), U32(0x00000000), U32(0x0000ffe7), U32(0xf00f0051),
           U32(0x0096000f), U32(0x00970010), U32(0x0098000f), U32(0x00990010),
           U32(0x00000022), U32(0x00000000), U32(0x00000000), U32(0x00010000),
           U32(0x00000002),
/* +0x1af8  meaning not yet known */
AT(0x1af8) U32(0x000069af),
/* +0x1e6e  meaning not yet known */
AT(0x1e6e) U16(0x0001),
/* +0x1ec0  meaning not yet known */
AT(0x1ec0) U32(0x00000014), U32(0x00000000), U32(0x00000094), U32(0x00000080),
           U32(0x00000000), U32(0x00000000), U32(0x00010000), U32(0x00000001),
/* +0x1fe2  meaning not yet known */
AT(0x1fe2) S4("clouds", 0), SB("clouds", 4), SB("clouds", 5), SB("clouds", 6),
           STR8("cloudX"),
           U8(0x00), U16(0xa000), U32(0x00000005), U32(0x00010000), U32(0x01010100),
           U32(0x00000000), U32(0x01010101), U32(0x00000101), U32(0x01010100),
           U32(0x01010100), U32(0x00010000),
/* +0x202d  meaning not yet known */
AT(0x202d) U8(0x04), U16(0x0010), U32(0x00effc00),
/* +0x2067  meaning not yet known */
AT(0x2067) U8(0x01),
/* +0x2082  meaning not yet known */
AT(0x2082) U16(0x7fff), U32(0x00007fff), U32(0x00000000), U16(0x0000), U32(0x00000000),  /* -> +0x2eba, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3120, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2ed6, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3146, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2ef2, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x316c, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2f0e, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3192, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2f2a, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x31b8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2f46, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x31de, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2f62, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3204, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2f7e, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x322a, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2f9a, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3250, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2fb6, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3276, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2fd2, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x329c, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x2fee, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x32c2, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x300a, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x32e8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3026, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x330e, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3042, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3334, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x305e, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x335a, installed from main_pointers.inc */
           U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x000c003b), U32(0x0048fb98), U32(0x00220000), U32(0x00010011),
           U32(0xac00a6ff), U32(0x54000000), U32(0x00020011), U32(0x5400a6ff),
           U32(0x54000000), U32(0x00030011), U32(0xac00a000), U32(0x54000190),
           U32(0x00040011), U32(0x5400a000), U32(0x54000190), U32(0x0005000e),
           U32(0x0168ac00), U32(0x000eac00), U32(0x54000006), U32(0xac000168),
           U32(0x0007000e), U32(0xfe98ac00), U32(0x000eac00), U32(0x54000008),
           U32(0xac00fe98), U32(0x00090011), U32(0xa000b7ff), U32(0x60000000),
           U32(0x000a0011), U32(0x6000b7ff), U32(0x60000000), U32(0x000b0011),
           U32(0xa000b000), U32(0x6000ffc4), U32(0x000c0011), U32(0x6000b000),
           U32(0x6000ffc4), U32(0x01200039), U32(0x0001f7a6), U32(0x0015000e),
           U32(0x005aa000), U32(0x000e2000), U32(0xa000001f), U32(0x1000005a),
           U32(0x0029000e), U32(0x005aa000), U32(0x000e0b00), U32(0xa0000033),
           U32(0x0900005a), U32(0x003d000e), U32(0x005aa000), U32(0x000e0700),
           U32(0xa0000047), U32(0x0600005a), U32(0x0051000e), U32(0x005aa000),
           U32(0x000efa00), U32(0x60000016), U32(0x2000005a), U32(0x0020000e),
           U32(0x005a6000), U32(0x000e1000), U32(0x6000002a), U32(0x0b00005a),
           U32(0x0034000e), U32(0x005a6000), U32(0x000e0900), U32(0x6000003e),
           U32(0x0700005a), U32(0x0048000e), U32(0x005a6000), U32(0x000e0600),
           U32(0x60000052), U32(0xfa00005a), U32(0x0017000e), U32(0xffa6a000),
           U32(0x000e2000), U32(0xa0000021), U32(0x1000ffa6), U32(0x002b000e),
           U32(0xffa6a000), U32(0x000e0b00), U32(0xa0000035), U32(0x0900ffa6),
           U32(0x003f000e), U32(0xffa6a000), U32(0x000e0700), U32(0xa0000049),
           U32(0x0600ffa6), U32(0x0053000e), U32(0xffa6a000), U32(0x000efa00),
           U32(0x60000018), U32(0x2000ffa6), U32(0x0022000e), U32(0xffa66000),
           U32(0x000e1000), U32(0x6000002c), U32(0x0b00ffa6), U32(0x0036000e),
           U32(0xffa66000), U32(0x000e0900), U32(0x60000040), U32(0x0700ffa6),
           U32(0x004a000e), U32(0xffa66000), U32(0x000e0600), U32(0x60000054),
           U32(0xfa00ffa6), U32(0x00520022), U32(0x000569bf), U32(0x0001000f),
           U32(0x00020010), U32(0x00080010), U32(0x00070010), U32(0x00050008),
           U32(0x0001000f), U32(0x00020010), U32(0x00060010), U32(0x00050010),
           U32(0x00220008), U32(0x008d0076), U32(0x00000000), U32(0x80000018),
           U32(0x00000000), U16(0x0000),
           S4("cloud1  .r8 ", 0), S4("cloud1  .r8 ", 4), S4("cloud1  .r8 ", 8), SB("cloud1  .r8 ", 12),
           U8(0x00), U32(0x00760022), U32(0x0000008d), U32(0x00180000),
           U32(0x00008000), U32(0x00000000),
           S4("cloud2  .r8 ", 0), S4("cloud2  .r8 ", 4), S4("cloud2  .r8 ", 8), SB("cloud2  .r8 ", 12),
           U8(0x00), U16(0x0022), U32(0x00000048), U32(0x037c0026), U32(0x00260000),
           U32(0x0000037e), U32(0x03800026), U32(0x00260000), U32(0x00000382),
           U32(0x03840026), U32(0x00260000), U32(0x00000386), U32(0xfdaa000d),
           U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xff86003a), U32(0x00000000), U32(0xf7fe0000), U32(0xffc00023),
           U32(0x02b20023), U16(0x008c), U32(0x207e83d7),  /* retail code address, kept raw */
           U16(0x0022), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xff62003a), U32(0x00000000), U32(0xf7fe0000), U32(0xff9c0023),
           U32(0x02fe0023), U16(0x008c), U32(0x207e83d7),  /* retail code address, kept raw */
           U16(0x0022), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xff3e003a), U32(0x00000000), U32(0xf8160000), U32(0xff780023),
           U32(0x05540023), U16(0x008c), U32(0x207e83fb),  /* retail code address, kept raw */
           U16(0x0022), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xff1a003a), U32(0x00000000), U32(0xf8160000), U32(0xff540023),
           U32(0x04480023), U16(0x008c), U32(0x207e83fb),  /* retail code address, kept raw */
           U16(0x0022), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x0010004c), U32(0x00000000), U32(0x02580000), U32(0xf7b60000),
           U32(0x00000048), U32(0x02120039), U32(0xffff19b4), U32(0x0000008d),
           U32(0x00180000), U32(0x00008000), U32(0x00000000),
           S4("cloud1  .r8 ", 0), S4("cloud1  .r8 ", 4), S4("cloud1  .r8 ", 8), SB("cloud1  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d47),  /* retail code address, kept raw */
           U32(0x01e60039), U32(0xffff0348), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004d), U32(0xf7ae1ec8), U32(0x0000001e), U32(0x00010081),
           U32(0x00100023), U32(0x00000081), U16(0x008c), U32(0x207e83d7),  /* retail code address, kept raw */
           U16(0x0022), U32(0x003a0039), U32(0x0002f7a6), U32(0x000f0005),
           U32(0x00100001), U32(0x00100002), U32(0x00100006), U32(0x00080005),
           U32(0x00390022), U32(0xf7a600f2), U32(0x000f0002), U32(0x00100001),
           U32(0x00100002), U32(0x00100008), U32(0x00080007), U32(0x008c0022),
           U32(0x207e9f6e),  /* retail code address, kept raw */
           U32(0x00000017), U32(0x00000017), U32(0x69a00052), U32(0x000f0005),
           U32(0x00100001), U32(0x00100002), U32(0x00100016), U32(0x00080015),
           U16(0x008c), U32(0x207e9f89),  /* retail code address, kept raw */
           U16(0x0017), U32(0x00050001), U32(0x0015000f), U32(0x00160010),
           U32(0x00200010), U32(0x001f0010), U32(0x00170008), U32(0x00050001),
           U32(0x001f000f), U32(0x00200010), U32(0x002a0010), U32(0x00290010),
           U32(0x00170008), U32(0x00050001), U32(0x0029000f), U32(0x002a0010),
           U32(0x00340010), U32(0x00330010), U32(0x00170008), U32(0x00050001),
           U32(0x0033000f), U32(0x00340010), U32(0x003e0010), U32(0x003d0010),
           U32(0x00170008), U32(0x00050001), U32(0x003d000f), U32(0x003e0010),
           U32(0x00480010), U32(0x00470010), U32(0x00170008), U32(0x00050001),
           U32(0x0047000f), U32(0x00480010), U32(0x00520010), U32(0x00510010),
           U32(0x00170008), U32(0x00050001), U32(0x0051000f), U32(0x00520010),
           U32(0x00060010), U32(0x00050010), U32(0x00220008), U32(0x00010017),
           U32(0x000f0005), U32(0x00100017), U32(0x00100018), U32(0x00100022),
           U32(0x00080021), U32(0x00010017), U32(0x000f0005), U32(0x00100021),
           U32(0x00100022), U32(0x0010002c), U32(0x0008002b), U32(0x00010017),
           U32(0x000f0005), U32(0x0010002b), U32(0x0010002c), U32(0x00100036),
           U32(0x00080035), U32(0x00010017), U32(0x000f0005), U32(0x00100035),
           U32(0x00100036), U32(0x00100040), U32(0x0008003f), U32(0x00010017),
           U32(0x000f0005), U32(0x0010003f), U32(0x00100040), U32(0x0010004a),
           U32(0x00080049), U32(0x00010017), U32(0x000f0005), U32(0x00100049),
           U32(0x0010004a), U32(0x00100054), U32(0x00080053), U32(0x00010017),
           U32(0x000f0005), U32(0x00100053), U32(0x00100054), U32(0x00100006),
           U32(0x00080005), U32(0x00230022), U32(0x008c0368), U32(0x207e989c),  /* retail code address, kept raw */
           U32(0x00890022), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x0010004c), U32(0x00000000), U32(0x02580000), U32(0xf7b60000),
           U32(0x00000048), U32(0xffde0039), U32(0xffff19b4), U32(0x0000008d),
           U32(0x00180000), U32(0x00008000), U32(0x00000000),
           S4("cloud1  .r8 ", 0), S4("cloud1  .r8 ", 4), S4("cloud1  .r8 ", 8), SB("cloud1  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d47),  /* retail code address, kept raw */
           U32(0xffb20039), U32(0xffff0348), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004d), U32(0xf7b01ec8), U32(0x00a2001e), U16(0x008c), U32(0x207e9f34),  /* retail code address, kept raw */
           U16(0x0023), U32(0x008cfdf4), U32(0x207e9fb8),  /* retail code address, kept raw */
           U32(0x00890022), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x0010004c), U32(0x00000000), U32(0x02580000), U32(0xf7b60000),
           U32(0x00000048), U32(0xff6e0039), U32(0xffff19b4), U32(0x0000008d),
           U32(0x00180000), U32(0x00008000), U32(0x00000000),
           S4("cloud1  .r8 ", 0), S4("cloud1  .r8 ", 4), S4("cloud1  .r8 ", 8), SB("cloud1  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d47),  /* retail code address, kept raw */
           U32(0xff420039), U32(0xffff0348), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004d), U32(0xf7b01ec8), U32(0x00a2001e), U16(0x008c), U32(0x207e9f34),  /* retail code address, kept raw */
           U16(0x0023), U32(0x008cfd66), U32(0x207e9fb8),  /* retail code address, kept raw */
           U32(0x00760022), U16(0x008c), U32(0x207e83d7),  /* retail code address, kept raw */
           U16(0x0048), U32(0x00260000), U32(0x0000037e), U32(0x03820026),
           U32(0x00260000), U32(0x00000386), U32(0x037c0026), U32(0x00260000),
           U32(0x00000380), U32(0x03840026), U32(0x004f0000), U32(0x09e409e0),
           U32(0xf9c60023), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000010), U32(0x00000000), U32(0x00000258),
           U32(0x0048f7b6), U32(0x00390000), U32(0x19b4fec4), U32(0x008dffff),
           U32(0x00000000), U32(0x80000018), U32(0x00000000), U16(0x0000),
           S4("cloud1  .r8 ", 0), S4("cloud1  .r8 ", 4), S4("cloud1  .r8 ", 8), SB("cloud1  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d47),  /* retail code address, kept raw */
           U16(0x0039), U32(0x0348fe98), U32(0x0089ffff), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x1ec8004d), U32(0x001ef7b0), U32(0x002300a2), U32(0x0022fcc2),
           U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x0010004c), U32(0x00000000), U32(0x03200000), U32(0xf7ce0000),
           U32(0x00000048), U32(0x005c0039), U32(0xffff19b4), U32(0x0000008d),
           U32(0x00180000), U32(0x00808000), U32(0x00800000),
           S4("cloud2  .r8 ", 0), S4("cloud2  .r8 ", 4), S4("cloud2  .r8 ", 8), SB("cloud2  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d5e),  /* retail code address, kept raw */
           U32(0x00300039), U32(0xffff0348), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004d), U32(0xf7ae1ec8), U32(0x0000001e), U16(0x008c), U32(0x207e9f51),  /* retail code address, kept raw */
           U16(0x0023), U32(0x008cfc56), U32(0x207e9fb8),  /* retail code address, kept raw */
           U16(0x008c), U32(0x207e83fb),  /* retail code address, kept raw */
           U16(0x0022), U32(0x01660023), U16(0x008c), U32(0x207e98e2),  /* retail code address, kept raw */
           U16(0x0022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000010), U32(0x00000000), U32(0x00000320),
           U32(0x0048f7ce), U32(0x00390000), U32(0x19b4ffde), U32(0x008dffff),
           U32(0x00000000), U32(0x80000018), U32(0x00000080), U16(0x0080),
           S4("cloud2  .r8 ", 0), S4("cloud2  .r8 ", 4), S4("cloud2  .r8 ", 8), SB("cloud2  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d5e),  /* retail code address, kept raw */
           U16(0x0039), U32(0x0348ffb2), U32(0x0089ffff), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x1ec8004d), U32(0x001ef7b0), U32(0x008c00a2), U32(0x207e9f34),  /* retail code address, kept raw */
           U32(0xfbd40023), U16(0x008c), U32(0x207e9fb8),  /* retail code address, kept raw */
           U16(0x0022), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x0010004c), U32(0x00000000), U32(0x03200000), U32(0xf7ce0000),
           U32(0x00000048), U32(0x00500039), U32(0xffff19b4), U32(0x0000008d),
           U32(0x00180000), U32(0x00808000), U32(0x00800000),
           S4("cloud2  .r8 ", 0), S4("cloud2  .r8 ", 4), S4("cloud2  .r8 ", 8), SB("cloud2  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d5e),  /* retail code address, kept raw */
           U32(0x00240039), U32(0xffff0348), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004d), U32(0xf7ae1ec8), U32(0x0000001e), U32(0xfb680023), U16(0x008c),
           U32(0x207e83fb),  /* retail code address, kept raw */
           U16(0x0022), U32(0x007e0023), U16(0x008c), U32(0x207e98e2),  /* retail code address, kept raw */
           U16(0x0022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000010), U32(0x00000000), U32(0x00000320),
           U32(0x0048f7ce), U32(0x00390000), U32(0x19b4fef6), U32(0x008dffff),
           U32(0x00000000), U32(0x80000018), U32(0x00000080), U16(0x0080),
           S4("cloud2  .r8 ", 0), S4("cloud2  .r8 ", 4), S4("cloud2  .r8 ", 8), SB("cloud2  .r8 ", 12),
           U8(0x00), U16(0x008c), U32(0x207e9d5e),  /* retail code address, kept raw */
           U16(0x0039), U32(0x0348feca), U32(0x0089ffff), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x1ec8004d), U32(0x001ef7b0), U32(0x008c00a2), U32(0x207e9f34),  /* retail code address, kept raw */
           U32(0xfb0a0023), U16(0x008c), U32(0x207e9fb8),  /* retail code address, kept raw */
           U16(0x0022), U32(0x008c0076), U32(0x207e9f85),  /* retail code address, kept raw */
           U32(0x003c0039), U32(0xffff19ba), U32(0x00120009), U32(0x00130001),
           U32(0x00130002), U32(0x00130004), U32(0x000c0003), U32(0x69a00014),
           U32(0x000f0005), U32(0x00100003), U32(0x00100004), U32(0x00100006),
           U32(0x00080005), U16(0x008c), U32(0x207e9f89),  /* retail code address, kept raw */
           U16(0x0022), U32(0x69a00014), U32(0x000f0005), U32(0x00100001),
           U32(0x00100002), U32(0x00100006), U32(0x00080005), U16(0x008c),
           U32(0x207e9f89),  /* retail code address, kept raw */
           U16(0x0022), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xf8fe003a), U32(0x00000000), U32(0xfb620000), U32(0x00000048),
           U32(0x037e0026), U32(0x00260000), U32(0x00000382), U32(0x03860026),
           U32(0x00260000), U32(0x0000037c), U32(0x03800026), U32(0x00260000),
           U32(0x00000384), U32(0x09e0004f), U32(0x002309e4), U32(0x0023f6dc),
           U32(0x0022f894), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000039e), U32(0x00000000), U32(0x00000200),
           U32(0x001afd9e), U32(0x00040064), U32(0x00003fff), U32(0xc0013fff),
           U32(0x3fff0000), U32(0x0000c001), U32(0x3fffc001), U32(0xc0010000),
           U32(0xf0040052), U32(0x7fff0030), U32(0x000f0005), U32(0x00100064),
           U32(0x00100065), U32(0x00100066), U32(0x00080067), U32(0x00890022),
           U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x034c004c), U32(0x00000000), U32(0x02000000), U32(0xfd9e0000),
           U32(0x000e0024), U32(0x0000fd94), U32(0x000d0fff), U32(0x001a0052),
           U32(0x00060064), U32(0x00000030), U32(0x00200000), U32(0x00300000),
           U32(0x0000ffe0), U32(0xffd00030), U32(0x00000000), U32(0x0000ffe0),
           U32(0x0020ffd0), U32(0xffd00000), U32(0xf0040052), U32(0x7fff0030),
           U32(0x000f0005), U32(0x00100064), U32(0x00100065), U32(0x00100066),
           U32(0x00100067), U32(0x00100068), U32(0x00080069), U32(0x000e0024),
           U32(0x0000fd94), U32(0x000d1fff), U32(0x001a0052), U32(0x00060064),
           U32(0x000000d0), U32(0x00c00000), U32(0x00300000), U32(0x00000080),
           U32(0x00700030), U32(0x00000000), U32(0x00000080), U32(0x00c0ffd0),
           U32(0xffd00000), U32(0xf0040052), U32(0x3fff0030), U32(0x000f0005),
           U32(0x00100064), U32(0x00100065), U32(0x00100066), U32(0x00100067),
           U32(0x00100068), U32(0x00080069), U32(0x000e0024), U32(0x0000fd94),
           U32(0x000d1fff), U32(0x001a0052), U32(0x00060064), U32(0x000000d0),
           U32(0x00c000a0), U32(0x00d00000), U32(0x00000080), U32(0x007000d0),
           U32(0x00a00000), U32(0x00000080), U32(0x00c00070), U32(0x00700000),
           U32(0xf0040052), U32(0x5fff0030), U32(0x000f0005), U32(0x00100064),
           U32(0x00100065), U32(0x00100066), U32(0x00100067), U32(0x00100068),
           U32(0x00080069), U32(0x000e0024), U32(0x0000fd94), U32(0x000d3fff),
           U32(0x001a0052), U32(0x00060064), U32(0x00000030), U32(0x002000a0),
           U32(0x00d00000), U32(0x0000ffe0), U32(0xffd000d0), U32(0x00a00000),
           U32(0x0000ffe0), U32(0x00200070), U32(0x00700000), U32(0xf0040052),
           U32(0x1fff0030), U32(0x000f0005), U32(0x00100064), U32(0x00100065),
           U32(0x00100066), U32(0x00100067), U32(0x00100068), U32(0x00080069),
           U32(0x000e0024), U32(0x0000fd94), U32(0x000d3fff), U32(0x001a0052),
           U32(0x00060064), U32(0x0000ff90), U32(0xff800000), U32(0x00300000),
           U32(0x0000ff40), U32(0xff300030), U32(0x00000000), U32(0x0000ff40),
           U32(0xff80ffd0), U32(0xffd00000), U32(0xf0040052), U32(0x7fff0030),
           U32(0x000f0005), U32(0x00100064), U32(0x00100065), U32(0x00100066),
           U32(0x00100067), U32(0x00100068), U32(0x00080069), U32(0x000e0024),
           U32(0x0000fd94), U32(0x000d3fff), U32(0x001a0052), U32(0x00060064),
           U32(0x0000ff90), U32(0xff80ff60), U32(0xff900000), U32(0x0000ff40),
           U32(0xff30ff90), U32(0xff600000), U32(0x0000ff40), U32(0xff80ff30),
           U32(0xff300000), U32(0xf0040052), U32(0x3fff0030), U32(0x000f0005),
           U32(0x00100064), U32(0x00100065), U32(0x00100066), U32(0x00100067),
           U32(0x00100068), U32(0x00080069), U32(0x000e0024), U32(0x0000fd94),
           U32(0x000d5fff), U32(0x001a0052), U32(0x00060064), U32(0x00000030),
           U32(0x0020ff60), U32(0xff900000), U32(0x0000ffe0), U32(0xffd0ff90),
           U32(0xff600000), U32(0x0000ffe0), U32(0x0020ff30), U32(0xff300000),
           U32(0xf0040052), U32(0x5fff0030), U32(0x000f0005), U32(0x00100064),
           U32(0x00100065), U32(0x00100066), U32(0x00100067), U32(0x00100068),
           U32(0x00080069), U32(0x000e0024), U32(0x0000fd94), U32(0x000d5fff),
           U32(0x001a0052), U32(0x00060064), U32(0x000000d0), U32(0x00c0ff60),
           U32(0xff900000), U32(0x00000080), U32(0x0070ff90), U32(0xff600000),
           U32(0x00000080), U32(0x00c0ff30), U32(0xff300000), U32(0xf0040052),
           U32(0x1fff0030), U32(0x000f0005), U32(0x00100064), U32(0x00100065),
           U32(0x00100066), U32(0x00100067), U32(0x00100068), U32(0x00080069),
           U32(0x000e0024), U32(0x0000fd94), U32(0x000d5fff), U32(0x001a0052),
           U32(0x00060064), U32(0x0000ff90), U32(0xff8000a0), U32(0x00d00000),
           U32(0x0000ff40), U32(0xff3000d0), U32(0x00a00000), U32(0x0000ff40),
           U32(0xff800070), U32(0x00700000), U32(0xf0040052), U32(0x7fff0030),
           U32(0x000f0005), U32(0x00100064), U32(0x00100065), U32(0x00100066),
           U32(0x00100067), U32(0x00100068), U32(0x00080069), U32(0x00760022),
           U32(0x69b00014), U32(0x000c0039), U32(0xffff0286), U32(0x009e000d),
           U32(0x00680039), U32(0xffff0340), U32(0x00000048), U32(0x005a004c),
           U32(0x00000000), U32(0x02000000), U32(0x226a0000), U32(0x0000008d),
           U32(0x00180000), U32(0x00000000), U32(0x00000000),
           S4("synfarm .r8 ", 0), S4("synfarm .r8 ", 4), S4("synfarm .r8 ", 8), SB("synfarm .r8 ", 12),
           U8(0x00), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0x1ec8004d), U32(0x001ef7ae), U32(0x00810001), U32(0x00050001),
           U32(0x0009000f), U32(0x000a0010), U32(0x00080010), U32(0x00070010),
           U32(0x00810008), U32(0x00220000), U32(0x00320039), U32(0xffff19ba),
           U32(0x00120009), U32(0x00130009), U32(0x0013000a), U32(0x0013000c),
           U32(0x000c000b), U32(0x000f0005), U32(0x0010000b), U32(0x0010000c),
           U32(0x00100008), U32(0x00080007), U32(0x00050022), U32(0x0009000f),
           U32(0x000a0010), U32(0x00080010), U32(0x00070010), U32(0x00220008),
           U16(0x0019),
           S4("fs4     .pal", 0), S4("fs4     .pal", 4), S4("fs4     .pal", 8), SB("fs4     .pal", 12),
           U8(0x00), U32(0x40000022), U32(0x00760000), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000258), U32(0x00000000), U32(0x00004000),
           U32(0x000df862), U32(0x007601a8), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000023c), U32(0x00000000), U32(0x00004000),
           U32(0x000df87a), U32(0x0076018c), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000220), U32(0x00000000), U32(0x00004000),
           U32(0x000df892), U32(0x00760170), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000204), U32(0x00000000), U32(0x00004000),
           U32(0x000df8aa), U32(0x00760154), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x000001e8), U32(0x00000000), U32(0x00004000),
           U32(0x000df8c2), U32(0x00760138), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x000001cc), U32(0x00000000), U32(0x00004000),
           U32(0x000df8da), U32(0x0076011c), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x000001b0), U32(0x00000000), U32(0x00004000),
           U32(0x000df8f2), U32(0x00760100), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000194), U32(0x00000000), U32(0x00004000),
           U32(0x000df90a), U32(0x007600e4), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000178), U32(0x00000000), U32(0x00004000),
           U32(0x000df922), U32(0x007600c8), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000015c), U32(0x00000000), U32(0x00004000),
           U32(0x000df93a), U32(0x007600ac), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000140), U32(0x00000000), U32(0x00004000),
           U32(0x000df952), U32(0x00760090), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000124), U32(0x00000000), U32(0x00004000),
           U32(0x000df96a), U32(0x00760074), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000108), U32(0x00000000), U32(0x00004000),
           U32(0x000df982), U32(0x00760058), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x000000ec), U32(0x00000000), U32(0x00004000),
           U32(0x000df99a), U32(0x0076003c), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x000000d0), U32(0x00000000), U32(0x00004000),
           U32(0x000df9b2), U32(0x00760020), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x000000b4), U32(0x00000000), U32(0x00004000),
           U32(0x000df9ca), U32(0x008c0004), U32(0x207e9f90),  /* retail code address, kept raw */
           U32(0x000e003b), U32(0x008cfb8c), U32(0x207e9fb8),  /* retail code address, kept raw */
           U32(0x001a0022), U32(0x00040000), U32(0x0000ffd8), U32(0x0028ffd8),
           U32(0xffd80000), U32(0x00000028), U32(0xffd80000), U32(0x00000000),
           U32(0x0000008d), U32(0x00180000), U32(0x00008000), U32(0x00000000),
           S4("stormy  .r8 ", 0), S4("stormy  .r8 ", 4), S4("stormy  .r8 ", 8), SB("stormy  .r8 ", 12),
           U8(0x00), U16(0x001e), U32(0x00200001), U32(0x00000004), U32(0x00007fff),
           U32(0x00000000), U32(0x00000000), U32(0x000100c7), U32(0x00c700ff),
           U32(0x00ff0002), U32(0x00030000), U32(0x00000000), U32(0x00040020),
           U32(0x80010000), U32(0x00000000), U32(0x00000000), U32(0x00c70000),
           U32(0x00ff0001), U32(0x000200c7), U32(0x000000ff), U32(0x00000003),
           U32(0x001e0000), U32(0x00220000), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xfff2004c), U32(0x00000000), U32(0x40000000), U32(0xf9e20000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110b6), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000ffcc), U32(0x00000000), U32(0x00004000),
           U32(0x008cf9fa), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10b80026), U32(0x00220001), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xffa6004c), U32(0x00000000), U32(0x40000000), U32(0xfa120000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110ba), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000ff80), U32(0x00000000), U32(0x00004000),
           U32(0x008cfa2a), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10bc0026), U32(0x00220001), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xff5a004c), U32(0x00000000), U32(0x40000000), U32(0xfa420000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110be), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000ff34), U32(0x00000000), U32(0x00004000),
           U32(0x008cfa5a), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10c00026), U32(0x00220001), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xff0e004c), U32(0x00000000), U32(0x40000000), U32(0xfa720000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110c2), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000fee8), U32(0x00000000), U32(0x00004000),
           U32(0x008cfa8a), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10c40026), U32(0x00220001), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xfec2004c), U32(0x00000000), U32(0x40000000), U32(0xfaa20000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110c6), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000fe9c), U32(0x00000000), U32(0x00004000),
           U32(0x008cfaba), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10c80026), U32(0x00220001), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xfe76004c), U32(0x00000000), U32(0x40000000), U32(0xfad20000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110ca), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000fe50), U32(0x00000000), U32(0x00004000),
           U32(0x008cfaea), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10cc0026), U32(0x00220001), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xfe2a004c), U32(0x00000000), U32(0x40000000), U32(0xfb020000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110ce), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000fe04), U32(0x00000000), U32(0x00004000),
           U32(0x008cfb1a), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10d00026), U32(0x00220001), U32(0x00890076), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U32(0xfdde004c), U32(0x00000000), U32(0x40000000), U32(0xfb320000),
           U16(0x008c), U32(0x207e8bc2),  /* retail code address, kept raw */
           U16(0x0026), U32(0x000110d2), U32(0x00760022), U16(0x0089), U32(0x00000000),  /* -> +0x22ca, installed from main_pointers.inc */
           U16(0x004c), U32(0x0000fdb8), U32(0x00000000), U32(0x00004000),
           U32(0x008cfb4a), U32(0x207e8bc2),  /* retail code address, kept raw */
           U32(0x10d40026), U32(0x00220001),
/* +0x3402  meaning not yet known */
AT(0x3402) U32(0x00000000),  /* -> +0x33c6, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x33de, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x33ea, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x33f6, installed from main_pointers.inc */
/* +0x3468  meaning not yet known */
AT(0x3468) U32(0x00000003),
           STR8("UUUU"),
/* +0x349c  meaning not yet known */
AT(0x349c) U32(0xa2a2a2a2), U32(0xa3a3a3a3), U32(0xa4a4a4a4), U32(0xa5a5a5a5),
           U32(0xa3a4a4a3), U32(0xa4a5a5a4), U32(0xa5a6a6a5), U32(0xa6a7a7a6),
           U16(0x0089), U32(0x00000000),  /* -> +0x33b0, installed from main_pointers.inc */
           U16(0x004c), U32(0x00000036), U32(0x00000000), U32(0x00000000),
           U32(0x001a00c6), U32(0x00050000), U32(0x00000000), U32(0x00010000),
           U32(0x00000000), U32(0x00010000), U32(0x00000000), U32(0x00010000),
           U32(0x00000000), U32(0x00220000), U16(0x008c), U32(0x207eb04a),  /* retail code address, kept raw */
           U16(0x0022), U32(0x69000052), U32(0x00240039), U32(0x00010094),
           U32(0x001c0039), U32(0x0001009c), U32(0x000f0005), U32(0x00100004),
           U32(0x00100005), U32(0x0010000b), U32(0x0008000a), U32(0x00240039),
           U32(0x0001009a), U32(0x001c0039), U32(0x000100a2), U32(0x000f0005),
           U32(0x00100006), U32(0x00100007), U32(0x00100008), U32(0x0008000b),
           U32(0x00240039), U32(0x00010096), U32(0x001c0039), U32(0x0001009e),
           U32(0x000f0005), U32(0x00100000), U32(0x00100001), U32(0x00100009),
           U32(0x00080008), U32(0x00240039), U32(0x00010098), U32(0x001c0039),
           U32(0x000100a0), U32(0x000f0005), U32(0x00100002), U32(0x00100003),
           U32(0x0010000a), U32(0x00080009), U32(0x001e0039), U32(0x800000a6),
           U32(0x000f0005), U32(0x0010000c), U32(0x0010000d), U32(0x0010000e),
           U32(0x0008000f), U32(0x00390022), U32(0x00a4001c), U32(0x00050001),
           U32(0x0008000f), U32(0x00090010), U32(0x000a0010), U32(0x000b0010),
           U32(0x00220008),
/* +0x35ec  meaning not yet known */
AT(0x35ec) U32(0x207ee6b0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000),  /* -> +0x85c0, installed from main_pointers.inc */
           U32(0x000000ff), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x86c8, installed from main_pointers.inc */
           U32(0x000000ff), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x0000005c),
           STR20("Dialog Temporary"),
           STR20("Temporary Flight"),
           U32(0x00000020),
           STR8("PREFS"),
           STR4("ADV"),
           STR20("FOREIGN_TRANSLATOR"),
           STR20("SOUTH_SEASONAL_LAT"),
           STR20("NORTH_SEASONAL_LAT"),
           STR8("WEATHER"),
           STR12("TEXT_SCROLL"),
           STR16("TEXTURE_WATER"),
           STR12("TEXTURE_GND"),
           STR16("TEXTURE_BLDG"),
           STR12("SYSCLOCK"),
           STR16("STARTUP_DEMO"),
           STR16("PROMPT_ON_EXIT"),
           STR20("SPLASH_SCREEN_OFF"),
           STR8("SOUND"),
           STR12("SMOOTH_VIEW"),
           STR12("SKY_TYPE"),
           STR12("SITUATION"),
           STR12("SEE_SELF"),
           STR16("RADIOS_25_KHZ"),
           STR16("QUICKTIPS_OFF"),
           STR20("PROPELLER_VISIBLE"),
           STR20("PERFORMANCE_MODE"),
           STR24("PANEL_STRETCHING_OFF"),
           STR20("PANEL_MASKING_OFF"),
           STR20("PAUSE_ON_LOST_FOCUS"),
           STR8("MOU_NUL"),
           STR12("MOU_ELEV"),
           STR8("MOU_AIL"),
           STR8("MEASURE"),
           STR24("LINEAR_CONTROLS_SCALE"),
           STR16("LINEAR_CONTROLS"),
           STR16("MAP_ORIENTATION"),
           STR20("MAP_GROUND_TEXTURE"),
           STR20("LOW_RES_RENDERING"),
           STR12("LOG_TIME"),
           STR12("LOG_FILE"),
           STR12("LOADWINDOW"),
           STR12("LOADWEATHER"),
           STR8("LOADSIM"),
           STR12("LOADDYNSCN"),
           STR8("KBDRUD"),
           STR8("KBDELEV"),
           STR8("KBDAIL"),
           STR8("INDASPD"),
           STR16("IMAGE_SMOOTHING"),
           STR16("IMAGE_QUALITY"),
           STR12("HAZE_FLAG"),
           STR8("GUGSPD"),
           STR16("GROUND_SHADOWS"),
           STR20("GRADIENT_HORIZON"),
           STR12("FULL_640"),
           STR12("FULL_SCREEN"),
           STR8("DEMO"),
           STR12("COM_RATE"),
           STR16("CLOUD_WHISPING"),
           STR16("CLOUD_THICKNESS"),
           STR8("ASLON"),
           STR8("ASLAT"),
           STR20("AIRCRAFT_SHADOWS"),
           STR20("AIRCRAFT_TEXTURE"),
           STR20("ADVANCED_WEATHER"),
           STR12("ADF_500_HZ"),
           STR4("APL"),
           STR16("CACHE_WAV_FILES"),
           STR8("SIM6"),
           STR16("Boeing 737-400"),
           STR8("SIM5"),
           STR12("Extra 300S"),
           STR8("SIM4"),
           STR16("Sopwith Camel"),
           STR8("SIM3"),
           STR28("Schweizer 2-32 Sailplane"),
           STR8("SIM2"),
           STR12("Learjet 45"),
           STR16("AIRtoCONTAINER"),
           STR8("SIM1"),
           STR24("Cessna Skylane 182R RG"),
           STR16("DEF_PERF_MODE"),
           STR16("HEAR_NAVIGATION"),
           STR12("HEAR_ENGINE"),
           STR16("HEAR_COCKPIT"),
           U32(0x00000000),
           STR4("0.0"),
           STR8("99.9"),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000020),
           U32(0x00000124),
/* +0x3af0  meaning not yet known */
AT(0x3af0) U32(0x00000020), U32(0x0000004f),
/* +0x3b0c  meaning not yet known */
AT(0x3b0c) U32(0x207e1680),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8f90, installed from main_pointers.inc */
           U32(0x0000001f),
/* +0x3b34  meaning not yet known */
AT(0x3b34) U32(0x207f1a80),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8fb0, installed from main_pointers.inc */
           U32(0x00000101),
/* +0x3b60  meaning not yet known */
AT(0x3b60) U32(0x207f1bc0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8f28, installed from main_pointers.inc */
           U32(0x00000009), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x8f38, installed from main_pointers.inc */
           U32(0x00000011), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x8f50, installed from main_pointers.inc */
           U32(0x00000019), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x8f70, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x3ab8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3abc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8f78, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x3ab8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3abc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8f80, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x3ab8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3abc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x8f88, installed from main_pointers.inc */
           U32(0x00000007),
/* +0x3c08  meaning not yet known */
AT(0x3c08) U32(0x00000000),  /* -> +0x90b0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x3ab8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3abc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x90b8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x3ab8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3abc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x90c0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x3ab8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x3abc, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x90c8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x207f0800),  /* retail code address, kept raw */
           STR12("%d-%d-%d"),
           U32(0x00000000), U32(0x00000000), U32(0x00404040), U32(0x00000000),
           U32(0x00000000), U32(0x00000000),
           STR8(" 123.4"),
/* +0x3fbb  meaning not yet known */
AT(0x3fbb) U8(0x20), U32(0x20202020), U32(0x00000020),
/* +0x41e0  meaning not yet known */
AT(0x41e0) U32(0x00000002),
/* +0x41fd  meaning not yet known */
AT(0x41fd) U8(0x06), U16(0x0a00), U32(0x0001a200),
/* +0x4456  the situation records MAIN takes itself: 0x12-byte entries { u16 tag (0xffff ends the table), u8 mode, u8 where, u16 length (0xffff: a list of five-byte {u32 offset, u8 size code} fields follows), u32 offset or address, u32 next entry, u32 routine }; docs/modules/main.md */
AT(0x4456) U16(0x0000), U8(0x00), U8(0x01), U16(0x001f), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x441f, installed from main_pointers.inc */
           U16(0x0001), U8(0x01), U8(0x01), U16(0x0001), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x41e0, installed from main_pointers.inc */
           U16(0x0002), U8(0x01), U8(0x02), U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U16(0x001d), U8(0x01), U8(0x01), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x44ad, installed from main_pointers.inc */
           U32(0x00000000), U8(0x81),  /* -> +0x41fd, installed from main_pointers.inc */
           U32(0x00000000), U8(0x81),  /* -> +0x41ff, installed from main_pointers.inc */
           U32(0x00000000), U8(0x82),  /* -> +0x4201, installed from main_pointers.inc */
           U16(0x0003), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x44ec, installed from main_pointers.inc */
           U32(0x00000562), U8(0x81),
           U32(0x00000564), U8(0x82),
           U32(0x0000056a), U8(0x81),
           U32(0x0000056c), U8(0x82),
           U32(0x00000572), U8(0x81),
           U32(0x00000574), U8(0x82),
           U32(0x00000578), U8(0x82),
           U32(0x0000057c), U8(0x82),
           U32(0x00000580), U8(0x82),
           U16(0x0014), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4512, installed from main_pointers.inc */
           U32(0x00000550), U8(0x81),
           U32(0x00000552), U8(0x81),
           U32(0x00000558), U8(0x81),
           U32(0x00000554), U8(0x81),
           U16(0x0016), U8(0x00), U8(0x00), U16(0x0001), U32(0x00000830), U32(0x00000000), U32(0x00000000),
           U16(0x0017), U8(0x00), U8(0x01), U16(0x0001), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4205, installed from main_pointers.inc */
           U16(0x0018), U8(0x00), U8(0x00), U16(0x0001), U32(0x00000264), U32(0x00000000), U32(0x00000000),
           U16(0x0019), U8(0x00), U8(0x00), U16(0x0001), U32(0x00000c46), U32(0x00000000), U32(0x00000000),
           U16(0x001a), U8(0x00), U8(0x00), U16(0x0001), U32(0x00000600), U32(0x00000000), U32(0x00000000),
           U16(0x001b), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x458d, installed from main_pointers.inc */
           U32(0x00000278), U8(0x81),
           U32(0x000005d8), U8(0x81),
           U32(0x0000027c), U8(0x81),
           U16(0x0082), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000f0a), U32(0x00000000), U32(0x00000000),
           U16(0x001c), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x45d4, installed from main_pointers.inc */
           U32(0x000005dc), U8(0x81),
           U32(0x000005e4), U8(0x81),
           U32(0x000005e6), U8(0x81),
           U32(0x000005e8), U8(0x81),
           U32(0x000005ea), U8(0x81),
           U32(0x000005ec), U8(0x81),
           U32(0x000005ee), U8(0x81),
           U16(0x001f), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x45f0, installed from main_pointers.inc */
           U32(0x00000238), U8(0x80),
           U32(0x00000239), U8(0x80),
           U16(0x0020), U8(0x00), U8(0x00), U16(0x0001), U32(0x0000023a), U32(0x00000000), U32(0x00000000),
           U16(0x007b), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4664, installed from main_pointers.inc */
           U32(0x00000b65), U8(0x80),
           U32(0x00000b72), U8(0x80),
           U32(0x00000b6d), U8(0x80),
           U32(0x00000b66), U8(0x80),
           U32(0x00000b67), U8(0x80),
           U32(0x00000b6e), U8(0x80),
           U32(0x00000b68), U8(0x80),
           U32(0x00000b70), U8(0x80),
           U32(0x00000b6f), U8(0x80),
           U32(0x00000b64), U8(0x80),
           U32(0x00000b69), U8(0x80),
           U32(0x00000b71), U8(0x80),
           U32(0x00000b73), U8(0x80),
           U32(0x00000b6c), U8(0x80),
           U32(0x00000b6b), U8(0x80),
           U32(0x00000b6a), U8(0x80),
           U16(0x0035), U8(0x0a), U8(0x00), U16(0x0002), U32(0x00000bd0), U32(0x00000000), U32(0x207f2d7c),  /* retail code address, kept raw */
           U16(0x0036), U8(0x00), U8(0x00), U16(0x0002), U32(0x0000084c), U32(0x00000000), U32(0x00000000),
           U16(0x0037), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x46b8, installed from main_pointers.inc */
           U32(0x00000338), U8(0x81),
           U32(0x0000033a), U8(0x81),
           U32(0x0000033c), U8(0x81),
           U32(0x0000033e), U8(0x81),
           U32(0x00000340), U8(0x81),
           U32(0x00000342), U8(0x81),
           U16(0x0038), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000372), U32(0x00000000), U32(0x00000000),
           U16(0x0039), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000c44), U32(0x00000000), U32(0x00000000),
           U16(0x004a), U8(0x0a), U8(0x00), U16(0x0001), U32(0x00000b7e), U32(0x00000000), U32(0x00000000),
           U16(0x004b), U8(0x0a), U8(0x00), U16(0x0001), U32(0x00000b96), U32(0x00000000), U32(0x00000000),
           U16(0x004e), U8(0x00), U8(0x00), U16(0x0001), U32(0x00000248), U32(0x00000000), U32(0x00000000),
           U16(0x0052), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e64), U32(0x00000000), U32(0x00000000),
           U16(0x0053), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e5a), U32(0x00000000), U32(0x00000000),
           U16(0x0054), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e68), U32(0x00000000), U32(0x00000000),
           U16(0x0055), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e5e), U32(0x00000000), U32(0x00000000),
           U16(0x0056), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e60), U32(0x00000000), U32(0x00000000),
           U16(0x0057), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e66), U32(0x00000000), U32(0x00000000),
           U16(0x0058), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e5c), U32(0x00000000), U32(0x00000000),
           U16(0x0059), U8(0x0a), U8(0x01), U16(0x0002), U32(0x00000000), U32(0x00000000), U32(0x207f2cd2),  /* -> +0x4211, installed from main_pointers.inc; retail code address, kept raw */
           U16(0x005b), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x47e6, installed from main_pointers.inc */
           U32(0x00000bb2), U8(0x81),
           U32(0x00000bb6), U8(0x81),
           U32(0x00000bba), U8(0x81),
           U32(0x00000558), U8(0x81),
           U32(0x00000bc0), U8(0x81),
           U32(0x00000bc4), U8(0x81),
           U32(0x00000bc6), U8(0x81),
           U32(0x00000bc8), U8(0x81),
           U32(0x00000558), U8(0x81),
           U32(0x00000558), U8(0x81),
           U16(0x005e), U8(0x0a), U8(0x00), U16(0x0002), U32(0x00000bdc), U32(0x00000000), U32(0x207f2dbc),  /* retail code address, kept raw */
           U16(0x005f), U8(0x0a), U8(0x00), U16(0x0001), U32(0x00000be8), U32(0x00000000), U32(0x207f2df7),  /* retail code address, kept raw */
           U16(0x0060), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4830, installed from main_pointers.inc */
           U32(0x00000280), U8(0x80),
           U32(0x00000558), U8(0x81),
           U32(0x00000558), U8(0x80),
           U32(0x00000281), U8(0x80),
           U16(0x0061), U8(0x0b), U8(0x03), U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4217, installed from main_pointers.inc */
           U16(0x0062), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4872, installed from main_pointers.inc */
           U32(0x00000d52), U8(0x81),
           U32(0x00000d54), U8(0x82),
           U32(0x00000d5a), U8(0x81),
           U32(0x00000d5c), U8(0x82),
           U32(0x00000d62), U8(0x81),
           U32(0x00000d64), U8(0x82),
           U16(0x0063), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x48a2, installed from main_pointers.inc */
           U32(0x00000d76), U8(0x81),
           U32(0x00000d78), U8(0x82),
           U32(0x00000d7e), U8(0x81),
           U32(0x00000d80), U8(0x82),
           U32(0x00000d86), U8(0x81),
           U32(0x00000d88), U8(0x82),
           U16(0x0068), U8(0x00), U8(0x04), U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x431b, installed from main_pointers.inc */
           U16(0x006c), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x48df, installed from main_pointers.inc */
           U32(0x0000034c), U8(0x81),
           U32(0x0000034e), U8(0x81),
           U32(0x00000350), U8(0x81),
           U32(0x00000352), U8(0x81),
           U32(0x00000354), U8(0x81),
           U16(0x0025), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000356), U32(0x00000000), U32(0x00000000),
           U16(0x006d), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x490d, installed from main_pointers.inc */
           U32(0x00000c4e), U8(0x81),
           U32(0x00000c5e), U8(0x81),
           U16(0x0076), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4992, installed from main_pointers.inc */
           U32(0x0000088c), U8(0x81),
           U32(0x0000088e), U8(0x81),
           U32(0x00000890), U8(0x81),
           U32(0x00000892), U8(0x81),
           U32(0x00000894), U8(0x81),
           U32(0x00000896), U8(0x81),
           U32(0x00000898), U8(0x81),
           U32(0x0000089a), U8(0x81),
           U32(0x000008a0), U8(0x81),
           U32(0x000008a2), U8(0x81),
           U32(0x000008a4), U8(0x81),
           U32(0x000008a8), U8(0x81),
           U32(0x000008ac), U8(0x81),
           U32(0x000008ae), U8(0x81),
           U32(0x000008b0), U8(0x81),
           U32(0x000008b2), U8(0x81),
           U32(0x000008b4), U8(0x81),
           U32(0x000008b6), U8(0x81),
           U32(0x000008b8), U8(0x81),
           U32(0x000008ba), U8(0x81),
           U32(0x000008bc), U8(0x81),
           U32(0x000008be), U8(0x81),
           U32(0x000008c0), U8(0x81),
           U16(0x0077), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4a17, installed from main_pointers.inc */
           U32(0x00000924), U8(0x81),
           U32(0x00000926), U8(0x81),
           U32(0x00000928), U8(0x81),
           U32(0x0000092a), U8(0x81),
           U32(0x0000092c), U8(0x81),
           U32(0x0000092e), U8(0x81),
           U32(0x00000930), U8(0x81),
           U32(0x00000932), U8(0x81),
           U32(0x00000938), U8(0x81),
           U32(0x0000093a), U8(0x81),
           U32(0x0000093c), U8(0x81),
           U32(0x00000940), U8(0x81),
           U32(0x00000944), U8(0x81),
           U32(0x00000946), U8(0x81),
           U32(0x00000948), U8(0x81),
           U32(0x0000094a), U8(0x81),
           U32(0x0000094c), U8(0x81),
           U32(0x0000094e), U8(0x81),
           U32(0x00000950), U8(0x81),
           U32(0x00000952), U8(0x81),
           U32(0x00000954), U8(0x81),
           U32(0x00000956), U8(0x81),
           U32(0x00000958), U8(0x81),
           U16(0x0078), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4a9c, installed from main_pointers.inc */
           U32(0x000009bc), U8(0x81),
           U32(0x000009be), U8(0x81),
           U32(0x000009c0), U8(0x81),
           U32(0x000009c2), U8(0x81),
           U32(0x000009c4), U8(0x81),
           U32(0x000009c6), U8(0x81),
           U32(0x000009c8), U8(0x81),
           U32(0x000009ca), U8(0x81),
           U32(0x000009d0), U8(0x81),
           U32(0x000009d2), U8(0x81),
           U32(0x000009d4), U8(0x81),
           U32(0x000009d8), U8(0x81),
           U32(0x000009dc), U8(0x81),
           U32(0x000009de), U8(0x81),
           U32(0x000009e0), U8(0x81),
           U32(0x000009e2), U8(0x81),
           U32(0x000009e4), U8(0x81),
           U32(0x000009e6), U8(0x81),
           U32(0x000009e8), U8(0x81),
           U32(0x000009ea), U8(0x81),
           U32(0x000009ec), U8(0x81),
           U32(0x000009ee), U8(0x81),
           U32(0x000009f0), U8(0x81),
           U16(0x0079), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4b21, installed from main_pointers.inc */
           U32(0x00000a54), U8(0x81),
           U32(0x00000a56), U8(0x81),
           U32(0x00000a58), U8(0x81),
           U32(0x00000a5a), U8(0x81),
           U32(0x00000a5c), U8(0x81),
           U32(0x00000a5e), U8(0x81),
           U32(0x00000a60), U8(0x81),
           U32(0x00000a62), U8(0x81),
           U32(0x00000a68), U8(0x81),
           U32(0x00000a6a), U8(0x81),
           U32(0x00000a6c), U8(0x81),
           U32(0x00000a70), U8(0x81),
           U32(0x00000a74), U8(0x81),
           U32(0x00000a76), U8(0x81),
           U32(0x00000a78), U8(0x81),
           U32(0x00000a7a), U8(0x81),
           U32(0x00000a7c), U8(0x81),
           U32(0x00000a7e), U8(0x81),
           U32(0x00000a80), U8(0x81),
           U32(0x00000a82), U8(0x81),
           U32(0x00000a84), U8(0x81),
           U32(0x00000a86), U8(0x81),
           U32(0x00000a88), U8(0x81),
           U16(0x0072), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000af0), U32(0x00000000), U32(0x00000000),
           U16(0x0034), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000b54), U32(0x00000000), U32(0x00000000),
           U16(0x0075), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4b66, installed from main_pointers.inc */
           U32(0x00000846), U8(0x81),
           U32(0x00000848), U8(0x81),
           U32(0x0000084a), U8(0x81),
           U16(0x007e), U8(0x0d), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4f29, installed from main_pointers.inc */
           U32(0x00000f12), U8(0x80),
           U32(0x00000f13), U8(0x80),
           U32(0x00000f14), U8(0x80),
           U32(0x00000f15), U8(0x80),
           U32(0x00000f90), U8(0x81),
           U32(0x00000f98), U8(0x82),
           U32(0x00000fa0), U8(0x82),
           U32(0x00000fa8), U8(0x82),
           U32(0x00000fb0), U8(0x82),
           U32(0x00000fb4), U8(0x82),
           U32(0x00000fb8), U8(0x82),
           U32(0x00000fbc), U8(0x81),
           U32(0x00000fbe), U8(0x81),
           U32(0x00000fc0), U8(0x80),
           U32(0x00000fc2), U8(0x80),
           U32(0x00000fc4), U8(0x80),
           U32(0x00000fc6), U8(0x80),
           U32(0x00000fca), U8(0x81),
           U32(0x00000fcc), U8(0x82),
           U32(0x00000fd2), U8(0x81),
           U32(0x00000fd4), U8(0x82),
           U32(0x00000fd8), U8(0x81),
           U32(0x00000fdc), U8(0x82),
           U32(0x00000fe0), U8(0x81),
           U32(0x00000fe2), U8(0x20),
           U32(0x00001078), U8(0x81),
           U32(0x00001080), U8(0x82),
           U32(0x00001088), U8(0x82),
           U32(0x00001090), U8(0x82),
           U32(0x00001098), U8(0x82),
           U32(0x0000109c), U8(0x82),
           U32(0x000010a0), U8(0x82),
           U32(0x000010a4), U8(0x81),
           U32(0x000010a6), U8(0x81),
           U32(0x000010a8), U8(0x80),
           U32(0x000010aa), U8(0x80),
           U32(0x000010ac), U8(0x80),
           U32(0x000010ae), U8(0x80),
           U32(0x000010b2), U8(0x81),
           U32(0x000010b4), U8(0x82),
           U32(0x000010ba), U8(0x81),
           U32(0x000010bc), U8(0x82),
           U32(0x000010c0), U8(0x81),
           U32(0x000010c4), U8(0x82),
           U32(0x000010c8), U8(0x81),
           U32(0x000010ca), U8(0x20),
           U32(0x00000f1c), U8(0x81),
           U32(0x00000f1e), U8(0x81),
           U32(0x00000f20), U8(0x81),
           U32(0x00000f22), U8(0x81),
           U32(0x00000f24), U8(0x81),
           U32(0x00000f26), U8(0x81),
           U32(0x00000f28), U8(0x81),
           U32(0x00000f2a), U8(0x81),
           U32(0x00000f2c), U8(0x81),
           U32(0x00000f2e), U8(0x81),
           U32(0x00000f30), U8(0x81),
           U32(0x00000f32), U8(0x81),
           U32(0x00000f34), U8(0x81),
           U32(0x00000f36), U8(0x81),
           U32(0x00000f38), U8(0x81),
           U32(0x00000f3a), U8(0x81),
           U32(0x00000f3c), U8(0x81),
           U32(0x00000f3e), U8(0x81),
           U32(0x00000f40), U8(0x81),
           U32(0x00000f42), U8(0x81),
           U32(0x00000f44), U8(0x81),
           U32(0x00000f46), U8(0x81),
           U32(0x00000f48), U8(0x81),
           U32(0x00000f4a), U8(0x81),
           U32(0x00000f4c), U8(0x81),
           U32(0x00000f4e), U8(0x81),
           U32(0x00000f50), U8(0x81),
           U32(0x00000f52), U8(0x81),
           U32(0x00000f54), U8(0x81),
           U32(0x00000f56), U8(0x81),
           U32(0x00000f58), U8(0x81),
           U32(0x00000f5a), U8(0x81),
           U32(0x00000f5c), U8(0x81),
           U32(0x00000f5e), U8(0x81),
           U32(0x00000f60), U8(0x81),
           U32(0x00000f62), U8(0x81),
           U32(0x00000f64), U8(0x81),
           U32(0x00000f66), U8(0x81),
           U32(0x00000f68), U8(0x81),
           U32(0x00000f6a), U8(0x81),
           U32(0x00000f6c), U8(0x81),
           U32(0x00000f6e), U8(0x81),
           U32(0x00000f70), U8(0x81),
           U32(0x00000f72), U8(0x81),
           U32(0x00000f74), U8(0x81),
           U32(0x00000f76), U8(0x81),
           U32(0x00000f78), U8(0x81),
           U32(0x00001004), U8(0x81),
           U32(0x00001006), U8(0x81),
           U32(0x00001008), U8(0x81),
           U32(0x0000100a), U8(0x81),
           U32(0x0000100c), U8(0x81),
           U32(0x0000100e), U8(0x81),
           U32(0x00001010), U8(0x81),
           U32(0x00001012), U8(0x81),
           U32(0x00001014), U8(0x81),
           U32(0x00001016), U8(0x81),
           U32(0x00001018), U8(0x81),
           U32(0x0000101a), U8(0x81),
           U32(0x0000101c), U8(0x81),
           U32(0x0000101e), U8(0x81),
           U32(0x00001020), U8(0x81),
           U32(0x00001022), U8(0x81),
           U32(0x00001024), U8(0x81),
           U32(0x00001026), U8(0x81),
           U32(0x00001028), U8(0x81),
           U32(0x0000102a), U8(0x81),
           U32(0x0000102c), U8(0x81),
           U32(0x0000102e), U8(0x81),
           U32(0x00001030), U8(0x81),
           U32(0x00001032), U8(0x81),
           U32(0x00001034), U8(0x81),
           U32(0x00001036), U8(0x81),
           U32(0x00001038), U8(0x81),
           U32(0x0000103a), U8(0x81),
           U32(0x0000103c), U8(0x81),
           U32(0x0000103e), U8(0x81),
           U32(0x00001040), U8(0x81),
           U32(0x00001042), U8(0x81),
           U32(0x00001044), U8(0x81),
           U32(0x00001046), U8(0x81),
           U32(0x00001048), U8(0x81),
           U32(0x0000104a), U8(0x81),
           U32(0x0000104c), U8(0x81),
           U32(0x0000104e), U8(0x81),
           U32(0x00001050), U8(0x81),
           U32(0x00001052), U8(0x81),
           U32(0x00001054), U8(0x81),
           U32(0x00001056), U8(0x81),
           U32(0x00001058), U8(0x81),
           U32(0x0000105a), U8(0x81),
           U32(0x0000105c), U8(0x81),
           U32(0x0000105e), U8(0x81),
           U32(0x00001060), U8(0x81),
           U32(0x000010ec), U8(0x81),
           U32(0x000010ee), U8(0x81),
           U32(0x000010f0), U8(0x81),
           U32(0x000010f2), U8(0x81),
           U32(0x000010f4), U8(0x81),
           U32(0x000010f6), U8(0x81),
           U32(0x000010f8), U8(0x81),
           U32(0x000010fa), U8(0x81),
           U32(0x000010fc), U8(0x81),
           U32(0x000010fe), U8(0x81),
           U32(0x00001100), U8(0x81),
           U32(0x00001102), U8(0x81),
           U32(0x00001104), U8(0x81),
           U32(0x00001106), U8(0x81),
           U32(0x00001108), U8(0x81),
           U32(0x0000110a), U8(0x81),
           U32(0x0000110c), U8(0x81),
           U32(0x0000110e), U8(0x81),
           U32(0x00001110), U8(0x81),
           U32(0x00001112), U8(0x81),
           U32(0x00001114), U8(0x81),
           U32(0x00001116), U8(0x81),
           U32(0x00001118), U8(0x81),
           U32(0x0000111a), U8(0x81),
           U32(0x0000111c), U8(0x81),
           U32(0x0000111e), U8(0x81),
           U32(0x00001120), U8(0x81),
           U32(0x00001122), U8(0x81),
           U32(0x00001124), U8(0x81),
           U32(0x00001126), U8(0x81),
           U32(0x00001128), U8(0x81),
           U32(0x0000112a), U8(0x81),
           U32(0x0000112c), U8(0x81),
           U32(0x0000112e), U8(0x81),
           U32(0x00001130), U8(0x81),
           U32(0x00001132), U8(0x81),
           U32(0x00001134), U8(0x81),
           U32(0x00001136), U8(0x81),
           U32(0x00001138), U8(0x81),
           U32(0x0000113a), U8(0x81),
           U32(0x0000113c), U8(0x81),
           U32(0x0000113e), U8(0x81),
           U32(0x00001140), U8(0x81),
           U32(0x00001142), U8(0x81),
           U32(0x00001144), U8(0x81),
           U32(0x00001146), U8(0x81),
           U32(0x00001148), U8(0x81),
           U32(0x0000115e), U8(0x81),
           U32(0x00001160), U8(0x81),
           U16(0x0084), U8(0x0a), U8(0x00), U16(0x0002), U32(0x00000550), U32(0x00000000), U32(0x00000000),
           U16(0x007a), U8(0x0d), U8(0x00), U16(0x0002), U32(0x00000330), U32(0x00000000), U32(0x00000000),
           U16(0x007d), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000c1a), U32(0x00000000), U32(0x00000000),
           U16(0x007f), U8(0x0c), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4fa3, installed from main_pointers.inc */
           U32(0x00001180), U8(0x81),
           U32(0x00001184), U8(0x81),
           U32(0x00001188), U8(0x81),
           U32(0x0000118c), U8(0x81),
           U32(0x00001190), U8(0x81),
           U32(0x00000558), U8(0x81),
           U32(0x00000558), U8(0x81),
           U32(0x00000558), U8(0x81),
           U32(0x00000558), U8(0x81),
           U32(0x00000558), U8(0x81),
           U16(0x0026), U8(0x0a), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x4fec, installed from main_pointers.inc */
           U32(0x000007bc), U8(0x81),
           U32(0x000007c0), U8(0x81),
           U32(0x000007c4), U8(0x81),
           U32(0x000007c8), U8(0x81),
           U32(0x000007cc), U8(0x81),
           U32(0x000007d0), U8(0x81),
           U32(0x000007d6), U8(0x81),
           U32(0x000007d8), U8(0x81),
           U32(0x000007fc), U8(0x81),
           U32(0x00000800), U8(0x81),
           U32(0x00000804), U8(0x81),
           U16(0x0083), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x5008, installed from main_pointers.inc */
           U32(0x0000023e), U8(0x81),
           U32(0x00000240), U8(0x81),
           U16(0x0085), U8(0x0d), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x50b0, installed from main_pointers.inc */
           U32(0x00000f7a), U8(0x81),
           U32(0x00000f7c), U8(0x81),
           U32(0x00000f7e), U8(0x81),
           U32(0x00000f80), U8(0x81),
           U32(0x00000f82), U8(0x81),
           U32(0x00000f84), U8(0x81),
           U32(0x00000f86), U8(0x81),
           U32(0x00000f88), U8(0x81),
           U32(0x00000f8a), U8(0x81),
           U32(0x00000f8c), U8(0x81),
           U32(0x00001062), U8(0x81),
           U32(0x00001064), U8(0x81),
           U32(0x00001066), U8(0x81),
           U32(0x00001068), U8(0x81),
           U32(0x0000106a), U8(0x81),
           U32(0x0000106c), U8(0x81),
           U32(0x0000106e), U8(0x81),
           U32(0x00001070), U8(0x81),
           U32(0x00001072), U8(0x81),
           U32(0x00001074), U8(0x81),
           U32(0x0000114a), U8(0x81),
           U32(0x0000114c), U8(0x81),
           U32(0x0000114e), U8(0x81),
           U32(0x00001150), U8(0x81),
           U32(0x00001152), U8(0x81),
           U32(0x00001154), U8(0x81),
           U32(0x00001156), U8(0x81),
           U32(0x00001158), U8(0x81),
           U32(0x0000115a), U8(0x81),
           U32(0x0000115c), U8(0x81),
           U16(0x0086), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x50c7, installed from main_pointers.inc */
           U32(0x000005fc), U8(0x80),
           U16(0x0087), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x50de, installed from main_pointers.inc */
           U32(0x000005f4), U8(0x80),
           U16(0x0088), U8(0x0a), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x50fa, installed from main_pointers.inc */
           U32(0x00000b84), U8(0x83),
           U32(0x00000b9c), U8(0x83),
           U16(0x0089), U8(0x00), U8(0x00), U16(0x0002), U32(0x0000028c), U32(0x00000000), U32(0x00000000),
           U16(0x008a), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000380), U32(0x00000000), U32(0x00000000),
           U16(0x008b), U8(0x0a), U8(0x00), U16(0x0004), U32(0x000007d4), U32(0x00000000), U32(0x00000000),
           U16(0x008c), U8(0x0a), U8(0x00), U16(0x0004), U32(0x00000808), U32(0x00000000), U32(0x00000000),
           U16(0x008d), U8(0x00), U8(0x00), U16(0x0002), U32(0x00000e62), U32(0x00000000), U32(0x00000000),
           U16(0x0091), U8(0x0a), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x5184, installed from main_pointers.inc */
           U32(0x00000b7c), U8(0x82),
           U32(0x00000b94), U8(0x82),
           U32(0x00000b84), U8(0x82),
           U32(0x00000b9c), U8(0x82),
           U32(0x00000b8c), U8(0x82),
           U32(0x00000ba4), U8(0x82),
           U16(0x0092), U8(0x00), U8(0x01), U16(0x0004), U32(0x00000000), U32(0x00000000), U32(0x207f2d0e),  /* -> +0x4209, installed from main_pointers.inc; retail code address, kept raw */
           U16(0x0093), U8(0x00), U8(0x01), U16(0x0004), U32(0x00000000), U32(0x00000000), U32(0x207f2d45),  /* -> +0x420d, installed from main_pointers.inc; retail code address, kept raw */
           U16(0x0094), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x51ce, installed from main_pointers.inc */
           U32(0x000008a8), U8(0x82),
           U32(0x00000940), U8(0x82),
           U32(0x000009d8), U8(0x82),
           U32(0x00000a70), U8(0x82),
           U16(0x0095), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x5212, installed from main_pointers.inc */
           U32(0x00000bd0), U8(0x82),
           U32(0x00000bd4), U8(0x82),
           U32(0x00000bd8), U8(0x82),
           U32(0x00000bdc), U8(0x82),
           U32(0x00000be0), U8(0x82),
           U32(0x00000be4), U8(0x82),
           U32(0x00000be8), U8(0x82),
           U32(0x00000bec), U8(0x82),
           U32(0x00000bf0), U8(0x82),
           U32(0x00000bf4), U8(0x82),
           U16(0x0096), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x5247, installed from main_pointers.inc */
           U32(0x00000b74), U8(0x82),
           U32(0x00000b7c), U8(0x82),
           U32(0x00000b94), U8(0x82),
           U32(0x00000b84), U8(0x82),
           U32(0x00000b9c), U8(0x82),
           U32(0x00000b8c), U8(0x82),
           U32(0x00000ba4), U8(0x82),
           U16(0x0097), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x5268, installed from main_pointers.inc */
           U32(0x00000b58), U8(0x82),
           U32(0x00000b5c), U8(0x82),
           U32(0x00000b60), U8(0x82),
           U16(0x0098), U8(0x00), U8(0x00), U16(0xffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x52e8, installed from main_pointers.inc */
           U32(0x000007bc), U8(0x82),
           U32(0x000007c0), U8(0x82),
           U32(0x000007c4), U8(0x82),
           U32(0x000007c8), U8(0x82),
           U32(0x000007cc), U8(0x81),
           U32(0x000007d0), U8(0x82),
           U32(0x000007d4), U8(0x82),
           U32(0x000007d8), U8(0x82),
           U32(0x000007dc), U8(0x82),
           U32(0x000007e0), U8(0x82),
           U32(0x000007e4), U8(0x82),
           U32(0x000007e8), U8(0x82),
           U32(0x000007ec), U8(0x82),
           U32(0x000007f0), U8(0x82),
           U32(0x000007f4), U8(0x82),
           U32(0x000007f8), U8(0x82),
           U32(0x000007fc), U8(0x82),
           U32(0x00000800), U8(0x82),
           U32(0x00000804), U8(0x82),
           U32(0x00000808), U8(0x82),
           U32(0x0000080c), U8(0x82),
           U32(0x00000810), U8(0x82),
           U16(0x0099), U8(0x00), U8(0x00), U16(0x0004), U32(0x0000029c), U32(0x00000000), U32(0x00000000),
           U16(0x009a), U8(0x00), U8(0x00), U16(0x0004), U32(0x00000af8), U32(0x00000000), U32(0x00000000),
           U16(0xffff), U8(0x00), U8(0x00), U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U16(0x0000),
           STR8("00000"),
           STR8("108.00"),
           STR8("117.95"),
           U32(0x00000031),
           STR4("360"),
           STR4("200"),
           STR4("999"),
           STR8("1699.9"),
           STR8("0000"),
           STR8("7777"),
           U32(0x00008645), U32(0x00000c86), U32(0x00000000), U32(0x207f5050),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000001), U32(0x00000000), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000000), U32(0x00000000),  /* -> +0x91f0, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x5328, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x5330, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x9200, installed from main_pointers.inc */
           U32(0x00000004), U32(0x00000000),  /* -> +0x5338, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x533c, installed from main_pointers.inc */
/* +0x53e4  meaning not yet known */
AT(0x53e4) U32(0x00000000),  /* -> +0x9208, installed from main_pointers.inc */
           U32(0x00000015), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x9220, installed from main_pointers.inc */
           U32(0x00000015),
/* +0x541c  meaning not yet known */
AT(0x541c) U32(0x00000000),  /* -> +0x9238, installed from main_pointers.inc */
           U32(0x00000008), U32(0x00000000),  /* -> +0x5328, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x5330, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* -> +0x9248, installed from main_pointers.inc */
           U32(0x00000004), U32(0x00000000),  /* -> +0x5338, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x533c, installed from main_pointers.inc */
/* +0x546c  meaning not yet known */
AT(0x546c) U32(0x00000000),  /* -> +0x9250, installed from main_pointers.inc */
           U32(0x00000015), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x9268, installed from main_pointers.inc */
           U32(0x00000015), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000001),
/* +0x54cc  meaning not yet known */
AT(0x54cc) U32(0x00000000),  /* -> +0x9280, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000),  /* -> +0x5350, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x5358, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000),  /* -> +0x9288, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000),  /* -> +0x5340, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x5344, installed from main_pointers.inc */
           U32(0x00000003), U32(0x00000000), U32(0x00000000),  /* -> +0x9290, installed from main_pointers.inc */
           U32(0x00000015), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x92a8, installed from main_pointers.inc */
           U32(0x00000015),
/* +0x5554  meaning not yet known */
AT(0x5554) U32(0x00000004), U32(0x00000001), U32(0x00000001), U32(0x00000000),
           U32(0x00000000), U32(0x00000001), U32(0x00000001), U32(0x00000000),
           U32(0x00000000),  /* -> +0x5320, installed from main_pointers.inc */
           U32(0x00000006), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000004), U32(0x00000028), U32(0x00001032), U32(0x00000350),
           U32(0x00000001), U32(0x00000001), U32(0x000000a8), U32(0x00002006),
           U32(0x00000380), U32(0x00000001), U32(0x00000004), U32(0x000000b0),
           U32(0x00001032), U32(0x00000352), U32(0x00000001), U32(0x00000000),
           U32(0x00000128), U32(0x00002004), U32(0x00000c58), U32(0x00000001),
           U32(0x00000004), U32(0x00000160), U32(0x00001019), U32(0x00000354),
           U32(0x00000001), U32(0x00000000), U32(0x000001b4), U32(0x00002004),
           U32(0x00000c68), U32(0x00000001), U32(0x00000000), U32(0x000001c0),
           U32(0x00002003), U32(0x00000e5a), U32(0x00000001), U32(0x00000001),
           U32(0x000001c8), U32(0x00002006), U32(0x00000e62), U32(0x00000001),
           U32(0x00000001), U32(0x000001d0), U32(0x00002006), U32(0x00000e68),
           U32(0x00000001), U32(0x00000001), U32(0x000001d8), U32(0x00002006),
           U32(0x00000e5e), U32(0x00000001), U32(0x00000001), U32(0x000001e0),
           U32(0x00002006), U32(0x00000e60), U32(0x00000001), U32(0x00000001),
           U32(0x000001e8), U32(0x00002006), U32(0x00000e64), U32(0x00000001),
           U32(0x00000001), U32(0x000001f8), U32(0x00002006), U32(0x00000e66),
           U32(0x00000001), U32(0x00000004), U32(0x00000208), U32(0x00011038),
           U32(0x00000e5c), U32(0x00000001), U32(0x00000004), U32(0x00000040),
           U32(0x0000104b), U32(0x00000000),  /* -> +0x91d8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000000c8), U32(0x0000104b),
           U32(0x00000000),  /* -> +0x91e4, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000078), U32(0x00001015),
           U32(0x00000000),  /* -> +0x92c0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000008c), U32(0x00001014),
           U32(0x00000000),  /* -> +0x92c8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000100), U32(0x00001015),
           U32(0x00000000),  /* -> +0x92d8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x00000114), U32(0x00001014),
           U32(0x00000000),  /* -> +0x92e0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x0000018c), U32(0x00001015),
           U32(0x00000000),  /* -> +0x92f0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000004), U32(0x000001a0), U32(0x00001014),
           U32(0x00000000),  /* -> +0x92f8, installed from main_pointers.inc */
/* +0x581c  meaning not yet known */
AT(0x581c) U32(0x00000001), U32(0x00000001),
/* +0x583c  meaning not yet known */
AT(0x583c) U32(0x00000001),
/* +0x589c  meaning not yet known */
AT(0x589c) U32(0x00000001), U32(0x00000001),
/* +0x5900  meaning not yet known */
AT(0x5900) U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000000), U32(0x00000001), U32(0x00000001), U32(0x00000001),
/* +0x5980  meaning not yet known */
AT(0x5980) U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000000), U32(0x00000001), U32(0x00000001),
/* +0x5a00  meaning not yet known */
AT(0x5a00) U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
/* +0x5a80  meaning not yet known */
AT(0x5a80) U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000001),
/* +0x5b00  meaning not yet known */
AT(0x5b00) U32(0x00000002), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
/* +0x5b80  meaning not yet known */
AT(0x5b80) U32(0x00000002), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001),
/* +0x5c00  meaning not yet known */
AT(0x5c00) U32(0x00000002), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
/* +0x5c80  meaning not yet known */
AT(0x5c80) U32(0x00000002), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),
           U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000000),
           U32(0x00000000), U32(0x207f6300),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x207f6320),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x207f6380),  /* retail code address, kept raw */
           U32(0x207f64d0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000000), U32(0x00000001),
/* +0x5d18  meaning not yet known */
AT(0x5d18) U32(0x00000001), U32(0x00000000), U32(0x00000001),
/* +0x5d68  meaning not yet known */
AT(0x5d68) U32(0x00000002), U32(0x00000000), U32(0x00000001),
/* +0x5d84  meaning not yet known */
AT(0x5d84) U32(0x207f6590),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x207f6560),  /* retail code address, kept raw */
/* +0x5dbc  meaning not yet known */
AT(0x5dbc) U32(0x9999999a), U32(0x3fe99999), U32(0x9999999a), U32(0x3fe99999),
           U32(0x9999999a), U32(0x3fe99999), U32(0x9999999a), U32(0x3fe99999),
           U32(0x9999999a), U32(0x3fe99999), U32(0x00000000), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x207f6810),  /* retail code address, kept raw */
/* +0x5e24  meaning not yet known */
AT(0x5e24) U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000001),
/* +0x5e50  meaning not yet known */
AT(0x5e50) U32(0x00000001),
/* +0x5e64  meaning not yet known */
AT(0x5e64) U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x00000001), U32(0x207f6830),  /* retail code address, kept raw */
           U32(0x207f69a0),  /* retail code address, kept raw */
           U32(0x207f6b20),  /* retail code address, kept raw */
/* +0x5ee8  meaning not yet known */
AT(0x5ee8) U32(0x00000001), U32(0x0000001c), U32(0x00002005), U32(0x00001210),
           U32(0x00000001), U32(0x00000001), U32(0x00000024), U32(0x00002003),
           U32(0x00000242), U32(0x00000001), U32(0x00000001), U32(0x0000002c),
           U32(0x00002005), U32(0x00001214), U32(0x00000001), U32(0x00000000),
           U32(0x00000034), U32(0x00002002), U32(0x00001218), U32(0x00000001),
           U32(0x00000000), U32(0x0000003c), U32(0x00002002), U32(0x00000128),
           U32(0x00000001), U32(0x00000000), U32(0x00000044), U32(0x00002002),
           U32(0x0000121c), U32(0x00000001), U32(0x00000000), U32(0x0000004c),
           U32(0x00002002), U32(0x00001220), U32(0x00000001), U32(0x00000000),
           U32(0x00000054), U32(0x00002002), U32(0x0000122c), U32(0x00000001),
           U32(0x00000000), U32(0x0000005c), U32(0x00002002), U32(0x00001230),
           U32(0x00000001), U32(0x00000000), U32(0x00000064), U32(0x00002002),
           U32(0x00001234), U32(0x00000001), U32(0x00000000), U32(0x00000070),
           U32(0x00002006), U32(0x00000612), U32(0x00000001), U32(0x00000000),
           U32(0x00000078), U32(0x00002002), U32(0x00000358), U32(0x00000001),
           U32(0x00000000), U32(0x00000080), U32(0x00002002), U32(0x0000035c),
           U32(0x00000001), U32(0x00000000), U32(0x00000088), U32(0x00002002),
           U32(0x00001224), U32(0x00000001), U32(0x00000000), U32(0x00000090),
           U32(0x00002002), U32(0x00001228), U32(0x00000001), U32(0x00000001),
           U32(0x0000009c), U32(0x00002009), U32(0x00000c18), U32(0x00000001),
           U32(0x00000001), U32(0x000000a4), U32(0x00002005), U32(0x00000da8),
           U32(0x00000001), U32(0x00000001), U32(0x000000ac), U32(0x00002005),
           U32(0x00000dac), U32(0x00000001), U32(0x00000000), U32(0x000000b8),
           U32(0x00002002), U32(0x0000128c), U32(0x00000001), U32(0x00000001),
           U32(0x000000d8), U32(0x00002006), U32(0x00000cb4), U32(0x00000001),
           U32(0x00000000), U32(0x000000f0), U32(0x00002002), U32(0x00001238),
           U32(0x00000001), U32(0x00000000), U32(0x00000128), U32(0x00002002),
           U32(0x00001244), U32(0x00000001), U32(0x00000000), U32(0x00000130),
           U32(0x00002002), U32(0x00001248), U32(0x00000001), U32(0x00000000),
           U32(0x00000138), U32(0x00002002), U32(0x0000124c), U32(0x00000001),
           U32(0x00000000), U32(0x00000140), U32(0x00002002), U32(0x00001250),
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000018),
           U32(0x00002003), U32(0x000011b6), U32(0x00000001), U32(0x00000000),
           U32(0x00000020), U32(0x00002003), U32(0x000011b4), U32(0x00000001),
           U32(0x00000000), U32(0x00000028), U32(0x00002003), U32(0x00000556),
           U32(0x00000001), U32(0x00000000), U32(0x00000030), U32(0x00002003),
           U32(0x000011b8), U32(0x00000001), U32(0x00000001), U32(0x0000003c),
           U32(0x00002006), U32(0x00000dc6), U32(0x00000001), U32(0x00000000),
           U32(0x00000044), U32(0x00002003), U32(0x000003ae), U32(0x00000001),
           U32(0x00000000), U32(0x00000050), U32(0x00002003), U32(0x000011b2),
           U32(0x00000001), U32(0x00000000), U32(0x00000058), U32(0x00002003),
           U32(0x0000039c), U32(0x00000001), U32(0x00000000), U32(0x00000060),
           U32(0x00002003), U32(0x0000039e), U32(0x00000001), U32(0x00000000),
           U32(0x00000068), U32(0x00002002), U32(0x00000398), U32(0x00000001),
           U32(0x00000000), U32(0x00000070), U32(0x00002003), U32(0x00000e84),
           U32(0x00000001), U32(0x00000000), U32(0x00000078), U32(0x00002003),
           U32(0x00000e82), U32(0x00000001), U32(0x00000000), U32(0x00000080),
           U32(0x00002002), U32(0x000003a4), U32(0x00000001), U32(0x00000000),
           U32(0x00000088), U32(0x00002003), U32(0x00000dc0), U32(0x00000001),
           U32(0x00000000), U32(0x00000090), U32(0x00002003), U32(0x00000cd0),
           U32(0x00000001), U32(0x00000000), U32(0x00000098), U32(0x00002003),
           U32(0x000011bc), U32(0x00000001),
           STR8("PREFS"),
           STR12("640x480x16"),
           STR12("640x480x8"),
           U32(0x003a0076), U32(0x0000001c), U32(0x0000000b), U32(0x00340004),
           U32(0x00000010), U32(0x00060000), U32(0x000c003b), U32(0x0048001c),
           U32(0x00220000), U32(0x00180046), U32(0x00000000), U32(0x00003000),
           U32(0x00000000), U32(0x00000000), U32(0x00220000), U32(0x00a5001a),
           U32(0xf6b00004), U32(0x00003000), U32(0xd000f6b0), U32(0x09500000),
           U32(0x0000d000), U32(0x30000950), U32(0x00180000), U32(0x00000000),
           U32(0x00000000),
           S4("smoke2  .r8 ", 0), S4("smoke2  .r8 ", 4), S4("smoke2  .r8 ", 8), SB("smoke2  .r8 ", 12),
           U8(0x00), U16(0x0048), U32(0x00390000), U32(0x03480032), U32(0x001e0001),
           U32(0x00200003), U32(0x00000004), U32(0x7ff40000), U32(0x00000000),
           U32(0x000000a5), U32(0x00a60000), U32(0x01000000), U32(0x010000a7),
           U32(0x00a80100), U32(0x00000100), U32(0x00000022), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6310, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x6300, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x6300, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x6300, installed from main_pointers.inc */
           STR16("smoke_wt.r8 "),
           STR16("smoke_bk.r8 "),
           U32(0x14211402), U32(0x00001421), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x14221402), U32(0x00001422), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x14231402), U32(0x00001423),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x14241402),
           U32(0x00001424), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x14251402), U32(0x00001425), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x14261402), U32(0x00001426), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x14271402), U32(0x00001427),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x14281402),
           U32(0x00001428), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x14291402), U32(0x00001429), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x142a1402), U32(0x0000142a), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0xffff1402),
/* +0x6400  meaning not yet known */
AT(0x6400) U32(0x16611606), U32(0x00001661), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x16621606), U32(0x00001662), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x16631606), U32(0x00001663),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x16641606),
           U32(0x00001664), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00011606),
/* +0x6464  meaning not yet known */
AT(0x6464) U32(0x16651606), U32(0x00001665), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x16661606), U32(0x00001666), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x16671606), U32(0x00001667),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x16681606),
           U32(0x00001668), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x16691606), U32(0x00001669), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00011606),
/* +0x64dc  meaning not yet known */
AT(0x64dc) U32(0x166a1606), U32(0x0000166a), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0xffff1606),
/* +0x6508  meaning not yet known */
AT(0x6508) U32(0x15401504), U32(0x00001540), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x15411504), U32(0x00001541), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x15421504), U32(0x00001542),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x15431504),
           U32(0x00001543), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x15441504), U32(0x00001544), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x15451504), U32(0x00001545), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0xffff1504),
/* +0x6598  meaning not yet known */
AT(0x6598) U32(0x16251208), U32(0x00001565), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0xffff1208),
/* +0x65c0  meaning not yet known */
AT(0x65c0) U32(0x15501505), U32(0x00001550), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x15511505), U32(0x00001551), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x15521505), U32(0x00001552),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x15531505),
           U32(0x00001553), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x15541505), U32(0x00001554), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x15551505), U32(0x00001555), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0xffff1505),
/* +0x6650  meaning not yet known */
AT(0x6650) U32(0x15201503), U32(0x00001520), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00011503),
/* +0x6678  meaning not yet known */
AT(0x6678) U32(0xffff1503),
/* +0x6690  meaning not yet known */
AT(0x6690) U32(0x11801108), U32(0x00001180), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x11811108), U32(0x00001181), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0xffff1108),
/* +0x66d0  meaning not yet known */
AT(0x66d0) U32(0x13041302), U32(0x00001304), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x13031302), U32(0x00001303), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0xffff1302),
/* +0x6710  meaning not yet known */
AT(0x6710) U32(0x11011001), U32(0x00001101), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x11021001), U32(0x00001102), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x11031001), U32(0x00001103),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x11041001),
           U32(0x00001104), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00011001),
/* +0x6774  meaning not yet known */
AT(0x6774) U32(0x11051001), U32(0x00001105), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x11061001), U32(0x00001106), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x11071001), U32(0x00001107),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00011001),
/* +0x67c4  meaning not yet known */
AT(0x67c4) U32(0x11081001), U32(0x00001108), U32(0x00000000), U32(0x00000000),  /* -> +0x6690, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00011001),
/* +0x67ec  meaning not yet known */
AT(0x67ec) U32(0x11091001), U32(0x00001109), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x110a1001), U32(0x0000110a), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0xffff1001),
/* +0x6828  meaning not yet known */
AT(0x6828) U32(0x12011002), U32(0x00001201), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x12081002), U32(0x00001208), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6598, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00011002),
/* +0x6864  meaning not yet known */
AT(0x6864) U32(0x12021002), U32(0x00001202), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x12031002), U32(0x00001203), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x12041002), U32(0x00001204),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x12051002),
           U32(0x00001205), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00011002),
/* +0x68c8  meaning not yet known */
AT(0x68c8) U32(0x12061002), U32(0x00001206), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x12071002), U32(0x00001207), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0xffff1002),
/* +0x6908  meaning not yet known */
AT(0x6908) U32(0x13011003), U32(0x00001301), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00011003),
/* +0x6930  meaning not yet known */
AT(0x6930) U32(0x13021003), U32(0x00001302), U32(0x00000000), U32(0x00000000),  /* -> +0x66d0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x13051003), U32(0x00001305), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00011003),
/* +0x696c  meaning not yet known */
AT(0x696c) U32(0x13061003), U32(0x00001306), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x13071003), U32(0x00001307), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00011003),
/* +0x69a8  meaning not yet known */
AT(0x69a8) U32(0x13081003), U32(0x00001308), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x13091003), U32(0x00001309), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x130a1003), U32(0x0000130a),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0xffff1003),
/* +0x69f8  meaning not yet known */
AT(0x69f8) U32(0x14011004), U32(0x00001401), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x14021004), U32(0x00001402), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6320, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00011004),
/* +0x6a34  meaning not yet known */
AT(0x6a34) U32(0x14031004), U32(0x00001403), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x14041004), U32(0x00001404), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00011004),
/* +0x6a70  meaning not yet known */
AT(0x6a70) U32(0x14051004), U32(0x00001405), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x14061004), U32(0x00001406), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00011004),
/* +0x6aac  meaning not yet known */
AT(0x6aac) U32(0x14071004), U32(0x00001407), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00011004),
/* +0x6ad4  meaning not yet known */
AT(0x6ad4) U32(0x14081004), U32(0x00001408), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x14091004), U32(0x00001409), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x140a1004), U32(0x0000140a),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0xffff1004),
/* +0x6b28  meaning not yet known */
AT(0x6b28) U32(0x15011005), U32(0x00001501), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x15021005), U32(0x00001502), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00011005),
/* +0x6b64  meaning not yet known */
AT(0x6b64) U32(0x15031005), U32(0x00001503), U32(0x00000000), U32(0x00000000),  /* -> +0x6650, installed from main_pointers.inc */
           U32(0x00000000), U32(0x15041005), U32(0x00001504), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6508, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00011005),
/* +0x6ba0  meaning not yet known */
AT(0x6ba0) U32(0x15051005), U32(0x00001505), U32(0x00000000), U32(0x00000000),  /* -> +0x65c0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x15061005), U32(0x00001506), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x15071005), U32(0x00001507),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x15081005),
           U32(0x00001508), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00011005),
/* +0x6c04  meaning not yet known */
AT(0x6c04) U32(0xffff1005),
/* +0x6c18  meaning not yet known */
AT(0x6c18) U32(0x16011006), U32(0x00001601), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x16081006), U32(0x00001608), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x16091006), U32(0x00001609),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x160a1006),
           U32(0x0000160a), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00011006),
/* +0x6c7c  meaning not yet known */
AT(0x6c7c) U32(0x16021006), U32(0x00001602), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x16031006), U32(0x00001603), U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00011006),
/* +0x6cb8  meaning not yet known */
AT(0x6cb8) U32(0x16041006), U32(0x00001604), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0x00011006),
/* +0x6ce0  meaning not yet known */
AT(0x6ce0) U32(0x16061006), U32(0x00001606), U32(0x00000000), U32(0x00000000),  /* -> +0x6400, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00011006),
/* +0x6d08  meaning not yet known */
AT(0x6d08) U32(0x16071006), U32(0x00001607), U32(0x00000000), U32(0x00000000),
           U32(0x00000000), U32(0xffff1006),
/* +0x6d32  meaning not yet known */
AT(0x6d32) U16(0x1001), U32(0x00001001), U32(0x00000000), U32(0x00000000),  /* -> +0x6710, installed from main_pointers.inc */
           U32(0x00000000), U32(0x10020000), U32(0x00001002), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6828, installed from main_pointers.inc */
           U32(0x00000000), U32(0x10030000), U32(0x00001003), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6908, installed from main_pointers.inc */
           U32(0x00000000), U32(0x10040000), U32(0x00001004), U32(0x00000000),
           U32(0x00000000),  /* -> +0x69f8, installed from main_pointers.inc */
           U32(0x00000000), U32(0x10050000), U32(0x00001005), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6b28, installed from main_pointers.inc */
           U32(0x00000000), U32(0x10060000), U32(0x00001006), U32(0x00000000),
           U32(0x00000000),  /* -> +0x6c18, installed from main_pointers.inc */
           U32(0x00000000), U32(0xffff0000),
/* +0x6dbc  meaning not yet known */
AT(0x6dbc) STR4("STN"),
           U32(0x00000003), U32(0x00000006), U32(0x00000012), U32(0x00000000),
           U32(0x0e390000), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x6de0  days in each month, a common year (clock_from_host in main_start.inc) */
           U16(0x001f), U16(0x001c), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f),
/* +0x6df8  days in each month, a leap year */
           U16(0x001f), U16(0x001d), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001f), U16(0x001e), U16(0x001f), U16(0x001e), U16(0x001f),
/* +0x6e10  the day of the year each season starts on, then 0xffff (clock_season in main_start.inc) */
           U16(0x0050), U16(0x00ac), U16(0x0108), U16(0x0163), U16(0xffff),
           U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x004e0c00),
           STR4("Foo"),
           U32(0x00000000), U32(0x00000000), U32(0x00000000),
           STR4("Bar"),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0xffb1f400),
           U32(0x004e0c00),
           STR4("Foo"),
           U32(0x00000000), U32(0x00000000), U32(0x00000000),
           STR4("Bar"),
           U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x00000010),
           U32(0x00007fff),
           STR4("Foo"),
           U32(0x00000000), U32(0x00000000), U32(0x00000000),
           STR4("Bar"),
/* +0x6e9c  meaning not yet known */
AT(0x6e9c) U32(0x207f8670),  /* retail code address, kept raw */
           U32(0x207f8800),  /* retail code address, kept raw */
/* +0x6ec0  meaning not yet known */
AT(0x6ec0) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000400),
           U32(0x00007fff), U32(0xffffffff),
/* +0x6eec  meaning not yet known */
AT(0x6eec) U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000400),
           U32(0x00000400), U32(0xffffffff),
/* +0x6f18  meaning not yet known */
AT(0x6f18) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000400),
           U32(0x00000400), U32(0x00000006), U32(0x00000000), U32(0x20800000),
           U32(0x00000000), U32(0x207f8ca0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000003),
/* +0x6f58  meaning not yet known */
AT(0x6f58) U32(0xffffffff),
/* +0x6f70  meaning not yet known */
AT(0x6f70) U32(0x00000001), U32(0x00000000), U32(0x00007ffe), U32(0x000000ff),
           U32(0x00007fff), U32(0xffffffff),
/* +0x6f9c  meaning not yet known */
AT(0x6f9c) U32(0x00000002), U32(0x00000000), U32(0x00000000), U32(0x00000400),
           U32(0x00000001), U32(0xffffffff), U32(0x00000000), U32(0x00100000),
           U32(0x00000000), U32(0x207f8ca0),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000003),
/* +0x6fdc  meaning not yet known */
AT(0x6fdc) U32(0xffffffff),
/* +0x6ff4  meaning not yet known */
AT(0x6ff4) U32(0x00000003),
/* +0x7008  meaning not yet known */
AT(0x7008) U32(0xffffffff),
/* +0x7034  meaning not yet known */
AT(0x7034) U32(0xffffffff),
/* +0x7335  meaning not yet known */
AT(0x7335) U32(0x00000000),  /* -> +0x7341, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x7399, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x73f1, installed from main_pointers.inc */
/* +0x7435  meaning not yet known */
AT(0x7435) U32(0x00000000),  /* -> +0x7363, installed from main_pointers.inc */
           U8(0x01), U16(0x0000), U32(0x00000000), U8(0x00), U32(0x00000000),  /* -> +0x73bb, installed from main_pointers.inc */
           U8(0x02), U16(0x0000), U32(0x00000000), U8(0x00), U32(0x00000000),  /* -> +0x7363, installed from main_pointers.inc */
           U8(0x03), U16(0x0000), U32(0x00000000), U8(0x00), U32(0x00000000),  /* -> +0x73bb, installed from main_pointers.inc */
           U8(0x04), U16(0x0000), U32(0x00000000), U8(0x00), U32(0x00000000),  /* -> +0x740b, installed from main_pointers.inc */
           U8(0x00), U16(0x0a00), U32(0x12000168), U32(0xf8a44800), U32(0x0101000a),
           U32(0x00000001),
/* +0x7496  meaning not yet known */
AT(0x7496) U16(0x2328), U32(0x012c0000), U16(0x0000), U32(0x207f93c4),  /* retail code address, kept raw */
           U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x17700000),
           U32(0x03e80000), U16(0x0000), U32(0x207f959f),  /* retail code address, kept raw */
           U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x27100000),
           U32(0x05dc0000), U16(0x0000), U32(0x207f9a17),  /* retail code address, kept raw */
           U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x1f400000),
           U32(0x4e200000), U16(0x0000), U32(0x207f99c6),  /* retail code address, kept raw */
           U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x4e200000),
           U32(0x75300000), U16(0x0000), U32(0x207f9989),  /* retail code address, kept raw */
           U16(0x0000), U32(0x00000000), U32(0x00000000), U32(0x13880000),
           U32(0x4e200000), U16(0x0000), U32(0x207f9679),  /* retail code address, kept raw */
/* +0x7878  meaning not yet known */
AT(0x7878) U32(0x36ac0064), U32(0x27100003), U32(0x00080000), U32(0x000000c8),
           U32(0x00000007), U32(0x12c00000), U32(0x000336ac), U32(0x00000064),
           U32(0x00000002), U32(0x00010000), U32(0x00000000), U32(0x36ac12c0),
           U32(0x00050003), U32(0x00080007), U32(0x00000000), U32(0x00000001),
           U32(0x12c00000), U32(0x000536ac), U32(0x0002012c), U32(0x00000006),
           U32(0x00030001), U32(0x00040001), U32(0x1b560708), U32(0x012c000f),
           U32(0x00080007), U32(0x00000000), U32(0x00010001), U32(0x07080000),
           U32(0x001e1b56), U32(0x000203e8), U32(0x00000006), U32(0x00030001),
           U32(0x00040000), U32(0x05dc0064), U32(0x03e80064), U32(0x00080002),
           U32(0x00010064), U32(0x00000003), U32(0x00640004), U32(0x012c05dc),
           U32(0x000604b0), U32(0x00140008), U32(0x00010000), U32(0x00000001),
           U32(0x05dc0064), U32(0x0258001e), U32(0x00080004), U32(0x00000014),
           U32(0x00010001), U32(0x00640000), U32(0x006405dc), U32(0x00021388),
           U32(0x00c80008), U32(0x00030001), U32(0x00040000), U32(0x176e0064),
           U32(0x232805dc), U32(0x00020000), U32(0x000400c8), U32(0x00010007),
           U32(0x00000008), U32(0x00000000), U32(0x00000009), U32(0x00000000),
           U32(0x000d0006), U32(0x00250019), U32(0x004b0032), U32(0x00960064),
           U32(0x00fa00c8), U32(0x0190012c), U32(0x02bc01f4), U32(0x05dc03e8),
           U32(0x09c407d0), U32(0x0fa00bb8), U32(0x00007d00), U32(0x00000000),
           U32(0x00080000), U32(0x00230014), U32(0x00000000), U32(0x207e1680),  /* retail code address, kept raw */
/* +0x79c8  meaning not yet known */
AT(0x79c8) U32(0x00000014), U32(0x00000000),
           STR8("01000"),
           STR8("01100"),
           STR8("06000"),
           STR8("06100"),
           STR8("10000"),
           STR8("10100"),
           U32(0x00000030), U32(0x00000000), U32(0x00000030), U32(0x00000000),
           U32(0x00000030), U32(0x00000000),
           STR8("03000"),
           STR8("04000"),
           U32(0x00000030), U32(0x00000000),
           STR4("360"),
           U32(0x00000000),
           STR8("09000"),
           STR8("10000"),
           U32(0x00003035), U32(0x00000000),
           STR4("270"),
           U32(0x00000000),
           STR8("11000"),
           STR8("12000"),
           STR4("100"),
           U32(0x00000000), U32(0x00003039), U32(0x00000000), U32(0x00003031),
           U32(0x00000000), U32(0x00000030), U32(0x00000000),
           STR4("360"),
           U32(0x00000000),
           STR4("500"),
           U32(0x00000000), U32(0x00003038), U32(0x00000000),
           STR8("5000"),
           U32(0x00003536), U32(0x00000000),
           STR8("10000"),
           U32(0x00003432), U32(0x00000000),
           STR8("30000"),
           STR4("-68"),
           U32(0x00000000), U32(0x00003031), U32(0x00000000),
           STR8("29.92"),
           STR8("1013"),
           STR8("32767"),
           U32(0x00000030), U32(0x00000000), U32(0x00000031), U32(0x00000000),
           STR4("360"),
           U32(0x00000000), U32(0x00000030), U32(0x00000000), U32(0x00003032),
           U32(0x00000000), U32(0x00003633), U32(0x00000000), U32(0x00000000),
           U32(0x05f40000), U32(0x00000725), U32(0x072505f4), U32(0x0be80000),
           U32(0x003d0e4a), U32(0x1dc40be8), U32(0x00000098), U32(0x00000000),
           U32(0x207fcd50),  /* retail code address, kept raw */
/* +0x7b60  meaning not yet known */
AT(0x7b60) U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0x79d0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x79d8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a00, installed from main_pointers.inc */
           U32(0x00000007),
/* +0x7bc0  meaning not yet known */
AT(0x7bc0) U32(0x00000000),  /* -> +0x79e0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x79e8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a08, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000001),
/* +0x7c10  meaning not yet known */
AT(0x7c10) U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0x79f0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x79f8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a10, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000002),
/* +0x7c6e  meaning not yet known */
AT(0x7c6e) U16(0x0008), U32(0x00020000), U32(0x00080007), U32(0x00060002),
           U32(0x00080007), U32(0x00060002), U32(0x00080002), U32(0x00080006),
           U32(0x00080004), U32(0x00080002), U32(0x00030001), U32(0x207fe180),  /* retail code address, kept raw */
           U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a18, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a20, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a28, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a30, installed from main_pointers.inc */
           U32(0x00000007),
/* +0x7d08  meaning not yet known */
AT(0x7d08) U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0x7a38, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a40, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a48, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a50, installed from main_pointers.inc */
           U32(0x00000007),
/* +0x7d6c  meaning not yet known */
AT(0x7d6c) U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0x7a58, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a60, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a68, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a70, installed from main_pointers.inc */
           U32(0x00000007),
/* +0x7dd0  meaning not yet known */
AT(0x7dd0) U32(0x00000001), U32(0x00000000), U32(0x00000001), U32(0x00000000),
           U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0x7a78, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a80, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a88, installed from main_pointers.inc */
           U32(0x00000007),
/* +0x7e3c  meaning not yet known */
AT(0x7e3c) U32(0x00000000),  /* -> +0x7a90, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7a98, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000000), U32(0x00000000),  /* -> +0x7aa0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7aa8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000000), U32(0x00000000),  /* -> +0x7ab0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7ab8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000000), U32(0x00000000),  /* -> +0x7ac0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7ac8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000001),
           U32(0x00000000), U32(0x00000001), U32(0x00000000), U32(0x00000000),  /* -> +0x7ad0, installed from main_pointers.inc */
           U32(0x00000007),
/* +0x7f10  meaning not yet known */
AT(0x7f10) U32(0x00000000),  /* -> +0x7ad8, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),  /* -> +0x7ae0, installed from main_pointers.inc */
           U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x207fe100),  /* retail code address, kept raw */
           U32(0x207fe140),  /* retail code address, kept raw */
           U32(0x207fdab0),  /* retail code address, kept raw */
           U32(0x207fdd80),  /* retail code address, kept raw */
           U32(0x00000000), U32(0x00000000),  /* -> +0x7b5c, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x7ca0, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x7e30, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x7b64, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x7ca8, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0x7e38, installed from main_pointers.inc */
           U32(0x00000000),  /* -> +0xd0c0, installed from main_pointers.inc */
           U32(0x00000000), U32(0x00000000), U32(0x00070002), U32(0x00000002),
           STR8("30488"),
           STR8("0000"),

#undef AT
#undef U8
#undef U16
#undef U32
#undef NUL_PAD
#undef SB
#undef S4
#undef STR4
#undef STR8
#undef STR12
#undef STR16
#undef STR20
#undef STR24
#undef STR28
#undef STR32
