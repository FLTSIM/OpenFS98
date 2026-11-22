/* FLTSIM98.EXE's .data image, decompiled: every non-zero value of the host's data section at retail's offsets, written one typed value
 * per macro.  Included by host.c as the initialiser of host_image, a byte array so that the D/V()/M*() accesses stay as they are.
 * HOST is module id 1, the host itself: FLTSIM98.EXE, image base 0x400000, relocations stripped, .text at 0x401000 for 0x14060 bytes,
 * .rdata at 0x416000 and .data at 0x417000, virtual 0x55cc bytes (0x1600 on disk, then zero; this file stops at +0x15c4, the rest
 * is zero).  The reference executable is FLTSIM98.EXE with sha256
 * f47c124a1ad44475a8a3a5ecbce83f5dc5d6e7dad78cf120b20616738bd99f6a (tools/ofs host-rt-tables-check stops on any other);
 * the build checks the image against a committed hash, and `tools/ofs data-images` compares it with retail (tools/check_data_image.py,
 * row HOST).
 *
 * The executable has no relocations, so the image keeps retail's absolute addresses raw: every word whose value lies in .text or in
 * 0x416000..0x41f000 (244 + 51 over .rdata and .data together, HOST_RT_CODE_POINTER/HOST_RT_DATA_POINTER in
 * runtime/host_rt_tables.inc, `rawptrs=` in the manifest row) is a raw U32, and host_rt_exe.c rebases them at start-up.  The service
 * table at 0x417ba0 (0xc8 dwords: a 1, then 199 words) is the largest: 194 raw code addresses and 5 reserved zeros.
 *
 * Named regions: the module records, dialog registry and modeless-dialog list headers (+0x0b30, +0x0ee0, +0x0f00), the service table
 * (+0x0ba0), the 37-entry extension table (+0x0f20) with its strings, the palette (+0x01a4, zero) and the path buffer (+0x4ca0,
 * empty).  The rest is typed values (U32 where aligned, else U16 or U8) marked "meaning not yet known", with the strings as literals.
 * Zero runs of 16 bytes or more are not written.
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
#define STR44(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40)
#define STR56(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52)
#define STR60(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56)
#define STR80(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56), S4(s, 60), S4(s, 64), S4(s, 68), S4(s, 72), S4(s, 76)
#define STR88(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56), S4(s, 60), S4(s, 64), S4(s, 68), S4(s, 72), S4(s, 76), S4(s, 80), S4(s, 84)

/* +0x0000  meaning not yet known */
AT(0x0000) U32(0x00000000), U32(0x0041504a), U32(0x00401000), U32(0x004010b0),  /* retail .text address, kept raw */
           U32(0x00413220), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* retail .text address, kept raw */
           U32(0x00000001), U32(0x00417028),  /* retail data address, kept raw */
           STR12("fs98.hlp"),
           STR20("HideMenuFullscreen"),
           STR16("HideMenuNormal"),
           STR12("Maximized"),
           U32(0x00000031),
           STR12("%d,%d,%d,%d"),
           STR8("MAIN"),
           STR12("Location"),
           STR4("CFG"),
           STR12("help\\fs98"),
           STR8(".hlp"),
           STR8(".CFG"),
           STR12("LANGUAGE"),
           STR4("DLL"),
           STR8("PREFS"),
           STR12("FULL_640"),
           STR16("NoWarnNot256"),
           STR12("ShowLogo"),
           STR16("InstalledFrom"),
           STR12("InstalledTo"),
           STR56("SOFTWARE\\Microsoft\\Microsoft Games\\Flight Simulator\\6.1"),
           STR8("DEBUG"),
           STR12("mslogo.avi"),
           STR4("STN"),
           U32(0x00000000),
           STR12("FS98MAIN"),
           U32(0x00000000),
           STR12("FS98CHILD"),
           U32(0x00000000),
           STR12("FS98FLOAT"),
           U32(0x00000000),
           STR12("FS98DLG3D"),
           U32(0x00000001), U32(0x00000001), U32(0x00000001),
/* +0x01a0  the master palette's header, a LOGPALETTE at 0x4171a0 (entries from +0x01a4) */
           U16(0x0300) /* version */, U16(0x0100) /* entries */,
/* +0x01a4  the master palette's 256 entries of four bytes, zero until the host fills them */
/* +0x05a8  the second palette, the master's copy: a LOGPALETTE header at 0x4175a8, its 256 entries (four bytes each, zero until the host fills them) from +0x05ac */
AT(0x05a8) U16(0x0300) /* version */, U16(0x0100) /* entries */,
/* +0x09ac  meaning not yet known */
AT(0x09ac) STR24("ERROR - FS5WTEST.EXE"),
           STR88("Couldn't Load Language Resource DLL\nError%s\nError Code: 0x%X (%d)\nProgram is aborting!"),
           U32(0x00000002),
           U32(0x00000030),
           STR4("%d"),
           STR8("MSPage"),
           STR12("WebTutorial"),
           STR12("Internet"),
           STR12("SearchWeb"),
           STR12("Search Page"),
           STR44("Software\\Microsoft\\Internet Explorer\\Main"),
           STR16("WebDirectory"),
           STR12("Feedback"),
           STR8("FS6Page"),
           STR16("OnlineSupport"),
           STR4("FAQ"),
           STR12("ProductNews"),
           STR12("FreeStuff"),
           STR12(">Concept"),
           U32(0x00000000), U32(0x00000000),
           U32(0x00000020), U32(0x00000044), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),
           STR16("help\\FS98.HLP"),
           STR12("FS6BOUND"),
           STR16("help\\fs98.hlp"),
/* +0x0b30  the host's module records: a struct fs98_host_list (count, capacity, grow 0x20, element size 0x14, for-each depth, removal-queued flag, data); empty at start */
           U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           STR12("SIMSUI.DLL"),
           STR8("*.DLL"),
           STR8("Linkage"),
           STR20("FS6API - Internal"),
           STR12("ImportTable"),
           STR8("MODULE="),
           STR16("AutoInitModules"),
/* +0x0ba0  the service table at 0x417ba0: 0xc8 dwords, a 1 (the host's module id) then 199 words, of which 194 are retail's code addresses in 0x4xxxxx (kept raw: host_rt_exe.c rebases them at start-up) and 5 are reserved zeros */
           U32(0x00000001), U32(0x00000000), U32(0x00000000), U32(0x00000000),  /* slots +0x000..+0x00c: the id 1, then code addresses (kept raw) */
           U32(0x00000000), U32(0x00000000), U32(0x004084e0), U32(0x00408530),  /* slots +0x010..+0x01c: 2 reserved zeros, the rest code addresses */
           U32(0x00408540), U32(0x00408550), U32(0x00408580), U32(0x00401a00),  /* slots +0x020..+0x02c: code addresses, kept raw */
           U32(0x00401a10), U32(0x004085b0), U32(0x004085c0), U32(0x00408620),  /* slots +0x030..+0x03c: code addresses, kept raw */
           U32(0x00408650), U32(0x004085d0), U32(0x00408680), U32(0x004086b0),  /* slots +0x040..+0x04c: code addresses, kept raw */
           U32(0x00408760), U32(0x004087c0), U32(0x004087e0), U32(0x004087f0),  /* slots +0x050..+0x05c: code addresses, kept raw */
           U32(0x00408800), U32(0x00410540), U32(0x004106e0), U32(0x00410740),  /* slots +0x060..+0x06c: code addresses, kept raw */
           U32(0x00410860), U32(0x00410aa0), U32(0x00410b80), U32(0x00410c00),  /* slots +0x070..+0x07c: code addresses, kept raw */
           U32(0x00410c40), U32(0x00410c70), U32(0x00410ca0), U32(0x00410d70),  /* slots +0x080..+0x08c: code addresses, kept raw */
           U32(0x00411560), U32(0x004115d0), U32(0x004094f0), U32(0x00409530),  /* slots +0x090..+0x09c: code addresses, kept raw */
           U32(0x00409570), U32(0x004095b0), U32(0x00408890), U32(0x004088c0),  /* slots +0x0a0..+0x0ac: code addresses, kept raw */
           U32(0x00408900), U32(0x004089c0), U32(0x00408ac0), U32(0x00408b10),  /* slots +0x0b0..+0x0bc: code addresses, kept raw */
           U32(0x00408b60), U32(0x00408c20), U32(0x00408d20), U32(0x00408d50),  /* slots +0x0c0..+0x0cc: code addresses, kept raw */
           U32(0x00408e40), U32(0x00408e90), U32(0x00408f00), U32(0x00408ff0),  /* slots +0x0d0..+0x0dc: code addresses, kept raw */
           U32(0x00409120), U32(0x004091a0), U32(0x00409210), U32(0x00409300),  /* slots +0x0e0..+0x0ec: code addresses, kept raw */
           U32(0x00409440), U32(0x004094a0), U32(0x00408860), U32(0x00408df0),  /* slots +0x0f0..+0x0fc: code addresses, kept raw */
           U32(0x00408810), U32(0x00408d70), U32(0x0040cf80), U32(0x00409720),  /* slots +0x100..+0x10c: code addresses, kept raw */
           U32(0x00409bd0), U32(0x00409bf0), U32(0x00409bd0), U32(0x00409fb0),  /* slots +0x110..+0x11c: code addresses, kept raw */
           U32(0x00413260), U32(0x00413570), U32(0x00413620), U32(0x0040cdb0),  /* slots +0x120..+0x12c: code addresses, kept raw */
           U32(0x0040a970), U32(0x0040a240), U32(0x0040a2c0), U32(0x0040a140),  /* slots +0x130..+0x13c: code addresses, kept raw */
           U32(0x0040b2d0), U32(0x0040ad00), U32(0x00409f20), U32(0x0040acb0),  /* slots +0x140..+0x14c: code addresses, kept raw */
           U32(0x0040a8b0), U32(0x00414ed0), U32(0x00409ed0), U32(0x0040ac90),  /* slots +0x150..+0x15c: code addresses, kept raw */
           U32(0x0040ab60), U32(0x0040a9a0), U32(0x0040a890), U32(0x0040ad90),  /* slots +0x160..+0x16c: code addresses, kept raw */
           U32(0x0040aff0), U32(0x00409de0), U32(0x0040ae20), U32(0x0040a000),  /* slots +0x170..+0x17c: code addresses, kept raw */
           U32(0x0040b210), U32(0x0040a510), U32(0x0040a6c0), U32(0x0040af10),  /* slots +0x180..+0x18c: code addresses, kept raw */
           U32(0x0040a3d0), U32(0x0040a360), U32(0x0040b290), U32(0x0040a190),  /* slots +0x190..+0x19c: code addresses, kept raw */
           U32(0x0040b240), U32(0x0040a400), U32(0x0040a430), U32(0x0040a480),  /* slots +0x1a0..+0x1ac: code addresses, kept raw */
           U32(0x0040a1f0), U32(0x0040b590), U32(0x0040b680), U32(0x0040b6d0),  /* slots +0x1b0..+0x1bc: code addresses, kept raw */
           U32(0x0040b780), U32(0x0040b8f0), U32(0x0040bcc0), U32(0x0040be90),  /* slots +0x1c0..+0x1cc: code addresses, kept raw */
           U32(0x0040bfc0), U32(0x0040c060), U32(0x0040c140), U32(0x0040c160),  /* slots +0x1d0..+0x1dc: code addresses, kept raw */
           U32(0x0040c1d0), U32(0x0040c200), U32(0x0040c310), U32(0x0040c350),  /* slots +0x1e0..+0x1ec: code addresses, kept raw */
           U32(0x0040c390), U32(0x0040c3d0), U32(0x0040c470), U32(0x0040c5b0),  /* slots +0x1f0..+0x1fc: code addresses, kept raw */
           U32(0x0040c630), U32(0x0040c860), U32(0x0040ca90), U32(0x0040cdc0),  /* slots +0x200..+0x20c: code addresses, kept raw */
           U32(0x0040ce60), U32(0x0040cef0), U32(0x0040cf20), U32(0x0040cf60),  /* slots +0x210..+0x21c: code addresses, kept raw */
           U32(0x0040cf80), U32(0x0040cf80), U32(0x0040cf90), U32(0x0040cfb0),  /* slots +0x220..+0x22c: code addresses, kept raw */
           U32(0x0040d000), U32(0x0040d050), U32(0x0040d080), U32(0x0040d0d0),  /* slots +0x230..+0x23c: code addresses, kept raw */
           U32(0x0040d140), U32(0x0040d190), U32(0x0040d220), U32(0x0040d280),  /* slots +0x240..+0x24c: code addresses, kept raw */
           U32(0x0040d340), U32(0x0040d3c0), U32(0x0040d4c0), U32(0x0040d4f0),  /* slots +0x250..+0x25c: code addresses, kept raw */
           U32(0x0040d5e0), U32(0x0040d610), U32(0x0040d640), U32(0x0040da90),  /* slots +0x260..+0x26c: code addresses, kept raw */
           U32(0x0040db10), U32(0x0040db40), U32(0x0040dbc0), U32(0x0040dc10),  /* slots +0x270..+0x27c: code addresses, kept raw */
           U32(0x0040dcd0), U32(0x0040dd10), U32(0x0040dd80), U32(0x0040e260),  /* slots +0x280..+0x28c: code addresses, kept raw */
           U32(0x0040e290), U32(0x0040ec00), U32(0x0040ece0), U32(0x0040ed40),  /* slots +0x290..+0x29c: code addresses, kept raw */
           U32(0x0040ed70), U32(0x0040edc0), U32(0x0040ee70), U32(0x0040eea0),  /* slots +0x2a0..+0x2ac: code addresses, kept raw */
           U32(0x0040eec0), U32(0x0040ef00), U32(0x0040ef40), U32(0x0040ef60),  /* slots +0x2b0..+0x2bc: code addresses, kept raw */
           U32(0x0040efa0), U32(0x0040f000), U32(0x0040efc0), U32(0x0040f110),  /* slots +0x2c0..+0x2cc: code addresses, kept raw */
           U32(0x0040f120), U32(0x0040f840), U32(0x0040fc00), U32(0x0040fb30),  /* slots +0x2d0..+0x2dc: code addresses, kept raw */
           U32(0x0040fd50), U32(0x0040fed0), U32(0x004100b0), U32(0x0040ff80),  /* slots +0x2e0..+0x2ec: code addresses, kept raw */
           U32(0x00410020), U32(0x00410170), U32(0x004101d0), U32(0x004103e0),  /* slots +0x2f0..+0x2fc: code addresses, kept raw */
           U32(0x00410210), U32(0x00410230), U32(0x004102a0), U32(0x00410340),  /* slots +0x300..+0x30c: code addresses, kept raw */
           U32(0x00410240), U32(0x00401140), U32(0x00411200), U32(0x00414380),  /* slots +0x310..+0x31c: code addresses, kept raw */
           U32(0xcc080a27), U32(0x11d06d4e), U32(0xa000218e), U32(0xef4205c9),
           U32(0x0000202c), U32(0x0000002c), U32(0x00006625), U32(0x00000000),
/* +0x0ee0  the dialog registry: a struct fs98_host_list, grow 0x20, element size 0x10 ({ id, module, index, flags }) */
           U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000010), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),
/* +0x0f00  the modeless-dialog list: a struct fs98_host_list, grow 0x20, element size 8 */
           U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           U32(0x00000000),
/* +0x0f20  the 37 built-in file extensions, by id: pointers to NUL-terminated strings (id 0 is the empty string at 0x4187f8, past the initialised data); the strings are in this image, from +0x00a0 (.CFG) and +0x0fb4 */
           U32(0x004187f8), U32(0x004180b8), U32(0x004180b0), U32(0x004180a8),  /* extension ids 0..3: pointers to their strings, kept raw */
           U32(0x004180a4), U32(0x0041809c), U32(0x00418094), U32(0x00418090),  /* extension ids 4..7: pointers to their strings, kept raw */
           U32(0x0041808c), U32(0x00418084), U32(0x0041807c), U32(0x00418074),  /* extension ids 8..11: pointers to their strings, kept raw */
           U32(0x0041806c), U32(0x00418064), U32(0x0041805c), U32(0x00418054),  /* extension ids 12..15: pointers to their strings, kept raw */
           U32(0x0041804c), U32(0x00418044), U32(0x0041803c), U32(0x00418034),  /* extension ids 16..19: pointers to their strings, kept raw */
           U32(0x0041802c), U32(0x00418024), U32(0x0041801c), U32(0x00418014),  /* extension ids 20..23: pointers to their strings, kept raw */
           U32(0x0041800c), U32(0x00418004), U32(0x00417ffc), U32(0x00417ff4),  /* extension ids 24..27: pointers to their strings, kept raw */
           U32(0x00417fec), U32(0x00417fe4), U32(0x00417fdc), U32(0x00417fd4),  /* extension ids 28..31: pointers to their strings, kept raw */
           U32(0x00417fcc), U32(0x00417fc4), U32(0x00417fbc), U32(0x00417fb4),  /* extension ids 32..35: pointers to their strings, kept raw */
           U32(0x004170a0),  /* extension ids 36..36: pointers to their strings, kept raw */
           STR8(".SC1"),
           STR8(".DRV"),
           STR8(".CHL"),
           STR8(".LES"),
           STR8(".TXR"),
           STR8(".TMP"),
           STR8(".VID"),
           STR8(".LOG"),
           STR8(".SIM"),
           STR8(".ADV"),
           STR8(".PNL"),
           STR8(".NDL"),
           STR8(".BND"),
           STR8(".FS6"),
           STR8(".DLL"),
           STR8(".MDL"),
           STR8(".WAV"),
           STR8(".BIN"),
           STR8(".ICN"),
           STR8(".GAU"),
           STR8(".AIR"),
           STR8(".VIS"),
           STR8(".PIX"),
           STR8(".BMP"),
           STR8(".PCX"),
           STR8(".HAZ"),
           STR8(".PAL"),
           STR4(".1"),
           STR4(".0"),
           STR8(".R82"),
           STR8(".R81"),
           STR4(".R8"),
           STR8(".MOD"),
           STR8(".STN"),
           STR8(".BGL"),
           STR4(":"),
           STR4("\\"),
           STR4("\\\\"),
           STR4(".*"),
           STR4("*"),
           STR4(".."),
           STR4("."),
           STR4("a.x"),
           U32(0x0000000a), U32(0x00000000), U32(0x00000001), U32(0x00000001),
           U32(0x00000002), U32(0x00000002), U32(0x00000003), U32(0x00000004),
           U32(0x00000001), U32(0x00000001),
           STR44("IDS_CUSTOM_CONTROLS_BUTTON_TEXT goes here! "),
           U32(0x00000000), U32(0x00000000), U32(0x00000000),
           STR44("IDS_CUSTOM_CONTROLS_AXIS_TEXT goes here! "),
           U32(0x00000000),
           U32(0x00000000),
           STR8("OEMName"),
           STR8("%s\\%s"),
           STR80("System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\Joystick\\OEM"),
           STR20("Joystick%dOEMName"),
           STR12("%s\\%s\\%s"),
           STR60("System\\CurrentControlSet\\Control\\MediaResources\\Joystick"),
           STR24("CurrentJoystickSettings"),
           STR12("Generic_%d"),
           STR16("Generic_%d_r"),
           STR8("%s_r"),
           STR8("%s%2.2d"),
           U32(0x00000000),
/* +0x1278  the accelerator tables: a struct fs98_host_list, grow 0x20, element size 0x10 ({ enabled, id, entries, count }) */
           U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000010), U32(0x00000000), U32(0x00000000), U32(0x00000000),
/* +0x1294  the id the next accelerator table gets (0x418294; never 0) */
           U32(0x00000001) /* next id */,
/* +0x1298  the menu tree's root: a one-item array of 0x14-byte items { parent id (word), id (word), text, flags, submenu items, HMENU }, the menu bar, id 0, then the item that ends the array (id 0xffff) */
           U16(0x0000) /* parent id */, U16(0x0000) /* id 0, the menu bar */, U32(0x00000000) /* text */, U32(0x00000000) /* flags */, U32(0x00000000) /* submenu items */, U32(0x00000000) /* HMENU */,
           U16(0x0000) /* parent id */, U16(0xffff) /* id 0xffff, ends the array */, U32(0x00000000) /* text */, U32(0x00000000) /* flags */, U32(0x00000000) /* submenu items */, U32(0x00000000) /* HMENU */,
/* +0x12c0  the menu redraw flag (0x4182c0) */
           U32(0x00000001) /* redraw */,
           STR20("tooltips_class32"),
           U32(0x00000001), U32(0x00000000),
/* +0x12e0  the modules strings are looked up in: a struct fs98_host_list of HMODULEs, grow 0x20, element size 4 */
           U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000004), U32(0x00000000), U32(0x00000000), U32(0x00000000),
           STR8(".fll"),
           STR4("..."),
           U32(0xffffffff),
           STR20("DirectSoundCreate"),
           STR12("DSOUND.DLL"),
           STR20("AllowIdiotProofing"),
           STR16("BitsPerSample"),
           STR16("SamplesPerSec"),
           STR8("SOUND"),
           STR12("Channels"),
           STR8(".wav"),
           U32(0x00000000),
           U32(0x00000000), U32(0x00000000), U32(0x00000020), U32(0x00000008),
           U32(0x00000000), U32(0x00000000), U32(0x00000000),
           STR12("Small Fonts"),
           U32(0x00000280), U32(0x000001e0),
           U32(0x00000008), U32(0xffffffff), U32(0x00000020),
           STR8("%s (%s)"),
           STR8("display"),
           STR16("(Direct3D RAMP)"),
           STR16("(Direct3D RGB)"),
           STR16("(Direct3D MMX)"),
           STR12("(Direct3D)"),
           STR24("DirectPlayLobbyCreateA"),
           STR8("dplayx"),
           STR24("DirectPlayEnumerateA"),
           STR20("DirectPlayCreate"),
           U32(0x00000000), U32(0x00000001), U32(0x00418560),  /* retail data address, kept raw */
           U32(0x00000000), U32(0x00000004), U32(0x00418554), U32(0x00000001),  /* retail data address, kept raw */
           U32(0x00000002), U32(0x00418548), U32(0x00000000), U32(0x00000008),  /* retail data address, kept raw */
           U32(0x00418544), U32(0x00000000), U32(0x00000010), U32(0x0041853c),  /* retail data address, kept raw */
           U32(0x00000001), U32(0x00000020), U32(0x00418534), U32(0x00000001),  /* retail data address, kept raw */
           U32(0x00000080), U32(0x0041852c), U32(0x00000001), U32(0x00000040),  /* retail data address, kept raw */
           U32(0x00418520), U32(0x00000000), U32(0x00001000), U32(0x0041851c),  /* retail data address, kept raw */
           U32(0x00000001), U32(0x00002000), U32(0x00418510), U32(0x00000000),  /* retail data address, kept raw */
           U32(0x00000400), U32(0x00418504), U32(0x00000000), U32(0x00000800),  /* retail data address, kept raw */
           U32(0x004184f4),  /* retail data address, kept raw */
/* +0x14f4  meaning not yet known */
AT(0x14f4) STR16("TextureShrink"),
           STR12("Texture8Bit"),
           STR12("VertexFog"),
           STR4("Fog"),
           STR12("PageFlip"),
           STR8("MipMap"),
           STR8("Dither"),
           STR8("Filter"),
           STR4("MMX"),
           STR12("DirectDraw"),
           STR12("Direct3D"),
           STR20("DirectDrawDisable"),
           STR12("640x480x8"),
           STR12("640x480x16"),
           STR8("Mode"),
           STR8("Device"),
           STR8("%s %s"),
           STR12("%dx%dx%d"),
           STR8("640x480"),
           U32(0x00000000), U32(0x00000000),
           U32(0x00000001),
/* +0x4ca0  the host's path buffer: 0x104 bytes, empty at start; the runtime fills it with a 0x104-byte bound (host_rt_main.c) */

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
#undef STR44
#undef STR56
#undef STR60
#undef STR80
#undef STR88
