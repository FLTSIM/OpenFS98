/* SIMSUI.DLL's .rdata and .data image, decompiled: every non-zero value of retail's data from RVA 0x15000 to 0x21400 (0xc400 bytes),
 * at retail's RVAs, written one typed value per macro.  Included by simsui_image.c as the initialiser of fs98_simsui_block, which
 * simsui_native.c's bind_image() rewrites at attach; every comment and AT() shows the RVA a disassembly shows (the array index is
 * RVA - 0x15000).
 *
 * SIMSUI is module id 0x20 with image base 0x20470000 (tools/gen_simsui_native.py checks the base, the image bounds and the export
 * count of 540: 448 routines and 92 data).  Its sections: .rdata at RVA 0x15000 with virtual size 0xa768, a gap of zeros to .data at
 * 0x20000 with virtual size 0x13f8 (0xc00 bytes on disk, zero past them), and the image runs to 0x21400.  `tools/ofs data-images`
 * compares the image with retail's unrelocated bytes and checks the 2,129 pointer sites, and what each is bound to, against retail's
 * relocations (tools/check_data_image.py, row SIMSUI, `bound=`); the build checks the image against a committed hash.
 *
 * Retail relocates 2,129 words of it (130 DATA, 486 CODE, 1,234 IMPORT, 279 NONE).  Each stays retail's raw virtual address here, as
 * a U32 commented with what simsui_native_tables.inc binds it to at attach, since bind_image() overwrites it: a pointer into the image
 * becomes the same RVA in our copy, one to a routine our transliteration of it, one to an import thunk the import itself (an MFC base
 * class's virtual), and one to an unwind funclet or exception-table entry zero.
 *
 * Regions are named from the export table (each exported datum under its export name, with the vtables' slot counts from
 * SIMSUI_VTABLE in simsui_tables.inc: vtables, message maps and their entries, runtime classes and the four statics, see
 * docs/modules/simsui.md); everything else is typed values (U32 where aligned, else U16 or U8) marked "meaning not yet known", with
 * the strings as literals.  Zero runs of 16 bytes or more are not written.
 *
 * Macros: AT(rva) designates a retail RVA; U8/U16/U32 emit one little-endian value; STRn(s) is a string padded with NULs to n bytes
 * and S4/SB its 4-byte and 1-byte pieces (a flat byte list cannot hold a string literal, so the padded literal is indexed). */
#define AT(rva) [(rva) - 0x15000] =
#define U8(x)  (uint8_t)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define U32(x) U16(x), U16((x) >> 16)
#define NUL_PAD "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define SB(s, i) U8((s NUL_PAD)[i])
#define S4(s, o) SB(s, o), SB(s, (o) + 1), SB(s, (o) + 2), SB(s, (o) + 3)
#define STR8(s) S4(s, 0), S4(s, 4)
#define STR12(s) S4(s, 0), S4(s, 4), S4(s, 8)
#define STR16(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12)
#define STR20(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16)
#define STR24(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20)
#define STR28(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24)
#define STR32(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28)
#define STR36(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32)
#define STR40(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36)
#define STR44(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40)
#define STR48(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44)
#define STR52(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48)
#define STR56(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52)
#define STR60(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56)
#define STR64(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56), S4(s, 60)
#define STR72(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56), S4(s, 60), S4(s, 64), S4(s, 68)
#define STR76(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56), S4(s, 60), S4(s, 64), S4(s, 68), S4(s, 72)
#define STR104(s) S4(s, 0), S4(s, 4), S4(s, 8), S4(s, 12), S4(s, 16), S4(s, 20), S4(s, 24), S4(s, 28), S4(s, 32), S4(s, 36), S4(s, 40), S4(s, 44), S4(s, 48), S4(s, 52), S4(s, 56), S4(s, 60), S4(s, 64), S4(s, 68), S4(s, 72), S4(s, 76), S4(s, 80), S4(s, 84), S4(s, 88), S4(s, 92), S4(s, 96), S4(s, 100)

/* RVA 0x15000  ??_7CDIBPal@@6B@  (vtable, 5 slots) */
/* RVA 0x15000  .rdata starts */
AT(0x15000) U32(0x20482d2e),  /* retail VA, becomes import 122: #3692 */
            U32(0x204711a0),  /* retail VA, becomes fs98_simsui_pal_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
            U32(0x20482d3a),  /* retail VA, becomes import 124: #3663 */
            U32(0x20471110),  /* retail VA, becomes fs98_simsui_object_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
            U32(0x20482d34),  /* retail VA, becomes import 123: #3626 */
            U32(0x20471130),  /* retail VA, becomes fs98_simsui_gdi_object_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
/* RVA 0x15048  ??_7CSimsSpinButtonCtrl@@6B@  (vtable, 48 slots) */
AT(0x15048) U32(0x20482e36),  /* retail VA, becomes import 166: #3719 */
            U32(0x204713f0),  /* retail VA, becomes fs98_simsui_spin_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20481630),  /* retail VA, becomes fs98_simsui_spin_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x15108  ??_7CSimsScrollBar@@6B@  (vtable, 48 slots) */
AT(0x15108) U32(0x20482e4e),  /* retail VA, becomes import 170: #3711 */
            U32(0x20471470),  /* retail VA, becomes fs98_simsui_scroll_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047afd0),  /* retail VA, becomes fs98_simsui_scroll_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x151c8  ??_7CSimsStatic@@6B@  (vtable, 49 slots) */
AT(0x151c8) U32(0x20482e5a),  /* retail VA, becomes import 172: #3721 */
            U32(0x20471530),  /* retail VA, becomes fs98_simsui_static_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20481620),  /* retail VA, becomes fs98_simsui_static_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20475120),  /* retail VA, becomes fs98_simsui_dialog_base_message_map */
            U32(0x20485290),  /* retail VA, image, RVA 0x15290 */
/* RVA 0x152a8  meaning not yet known */
AT(0x152a8) U32(0x20482e8a),  /* retail VA, becomes import 181: #3619 */
            U32(0x20472320),  /* retail VA, becomes fs98_simsui_font_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
            U32(0x20482f14),  /* retail VA, becomes import 204: #3597 */
            U32(0x20472140),  /* retail VA, becomes fs98_simsui_remap_dialog_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482f0e),  /* retail VA, becomes import 203: #4425 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20472160),  /* retail VA, becomes fs98_simsui_remap_dialog_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x20475110),  /* retail VA, becomes fs98_simsui_dialog_exchange */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482f08),  /* retail VA, becomes import 202: #5280 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482f02),  /* retail VA, becomes import 201: #1775 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482efc),  /* retail VA, becomes import 200: #6052 */
            U32(0x20482ec6),  /* retail VA, becomes import 191: #2514 */
            U32(0x20472170),  /* retail VA, becomes fs98_simsui_remap_dialog_init */
            U32(0x20482ef6),  /* retail VA, becomes import 199: #4998 */
            U32(0x20482ef0),  /* retail VA, becomes import 198: #4853 */
            U32(0x20482eea),  /* retail VA, becomes import 197: #4376 */
            U32(0x20482ee4),  /* retail VA, becomes import 196: #5265 */
/* RVA 0x15398  ?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B */
AT(0x15398) U32(0x20490078),  /* retail VA, image, RVA 0x20078 */
            U32(0x000000fc), U32(0x0000ffff), U32(0x00000000),
            U32(0x20472390),  /* retail VA, becomes fs98_simsui_3piece_base_class */
            U32(0x00000000),
/* RVA 0x153b0  ?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B */
AT(0x153b0) U32(0x20473720),  /* retail VA, becomes fs98_simsui_slider_base_message_map */
            U32(0x204853b8),  /* retail VA, image, RVA 0x153b8 */
/* RVA 0x153b8  ?_messageEntries@CSims3PieceButton@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x153b8) U32(0x00000201), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20472f30),  /* retail VA, becomes fs98_simsui_3piece_button_down */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20472fe0),  /* retail VA, becomes fs98_simsui_3piece_button_up */
            U32(0x00000200), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x204730e0),  /* retail VA, becomes fs98_simsui_3piece_mouse_move */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x20473320),  /* retail VA, becomes fs98_simsui_3piece_kill_focus */
            U32(0x00000105), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x20473370),  /* retail VA, becomes fs98_simsui_default_3 */
            U32(0x00000104), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x20473370),  /* retail VA, becomes fs98_simsui_default_3 */
            U32(0x00000087), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000023),
            U32(0x20473360),  /* retail VA, becomes fs98_simsui_3piece_dialog_code */
/* RVA 0x15478  ??_7CSims3PieceButton@@6B@  (vtable, 50 slots) */
AT(0x15478) U32(0x204723a0),  /* retail VA, becomes fs98_simsui_3piece_runtime_class */
            U32(0x20472400),  /* retail VA, becomes fs98_simsui_3piece_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x204724f0),  /* retail VA, becomes fs98_simsui_3piece_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20473880),  /* retail VA, becomes fs98_simsui_button_draw_item */
            U32(0x20473730),  /* retail VA, becomes fs98_simsui_button_init */
/* RVA 0x15540  ?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B */
AT(0x15540) U32(0x20473720),  /* retail VA, becomes fs98_simsui_slider_base_message_map */
            U32(0x20485548),  /* retail VA, image, RVA 0x15548 */
/* RVA 0x15548  ?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x15548) U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x204735e0),  /* retail VA, becomes fs98_simsui_small_button_set_focus */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20473480),  /* retail VA, becomes fs98_simsui_help_button_up */
            U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20473420),  /* retail VA, becomes fs98_simsui_hit_client */
/* RVA 0x155a8  ?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B */
AT(0x155a8) U32(0x20473720),  /* retail VA, becomes fs98_simsui_slider_base_message_map */
            U32(0x204855b0),  /* retail VA, image, RVA 0x155b0 */
/* RVA 0x155b0  ?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x155b0) U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x204735a0),  /* retail VA, becomes fs98_simsui_exit_button_up */
            U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x204735e0),  /* retail VA, becomes fs98_simsui_small_button_set_focus */
            U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20473420),  /* retail VA, becomes fs98_simsui_hit_client */
/* RVA 0x15610  ?messageMap@CSimsButton@@1UAFX_MSGMAP@@B */
AT(0x15610) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20485618),  /* retail VA, image, RVA 0x15618 */
/* RVA 0x15618  ?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x15618) U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
/* RVA 0x15648  ??_7CSimsHelpButton@@6B@  (vtable, 50 slots) */
AT(0x15648) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x204733a0),  /* retail VA, becomes fs98_simsui_help_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20473410),  /* retail VA, becomes fs98_simsui_help_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20473880),  /* retail VA, becomes fs98_simsui_button_draw_item */
            U32(0x20473730),  /* retail VA, becomes fs98_simsui_button_init */
/* RVA 0x15710  ??_7CSimsExitButton@@6B@  (vtable, 50 slots) */
AT(0x15710) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x204734d0),  /* retail VA, becomes fs98_simsui_exit_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20473540),  /* retail VA, becomes fs98_simsui_exit_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20473880),  /* retail VA, becomes fs98_simsui_button_draw_item */
            U32(0x20473730),  /* retail VA, becomes fs98_simsui_button_init */
/* RVA 0x157d8  ??_7CSimsButton@@6B@  (vtable, 99 slots) */
AT(0x157d8) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x20473660),  /* retail VA, becomes fs98_simsui_button_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20473720),  /* retail VA, becomes fs98_simsui_slider_base_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20473880),  /* retail VA, becomes fs98_simsui_button_draw_item */
            U32(0x20473730),  /* retail VA, becomes fs98_simsui_button_init */
            U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x20473ad0),  /* retail VA, becomes fs98_simsui_cbutton_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20482f80),  /* retail VA, becomes import 224: #3402 */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20482f7a),  /* retail VA, becomes import 223: #2575 */
            U32(0x00000000),
/* RVA 0x15968  ?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B */
AT(0x15968) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20485970),  /* retail VA, image, RVA 0x15970 */
/* RVA 0x15970  ?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x15970) U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x20474470),  /* retail VA, becomes fs98_simsui_erase_nothing */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20474480),  /* retail VA, becomes fs98_simsui_check_button_up */
            U32(0x00000101), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x204745b0),  /* retail VA, becomes fs98_simsui_check_key_up */
            U32(0x00000201), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20474500),  /* retail VA, becomes fs98_simsui_check_button_down */
            U32(0x00000203), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20474560),  /* retail VA, becomes fs98_simsui_check_double_click */
            U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x204745e0),  /* retail VA, becomes fs98_simsui_check_kill_focus */
            U32(0x000005f6), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20474600),  /* retail VA, becomes fs98_simsui_check_checked */
/* RVA 0x15a48  ??_7CSimsCheckButton@@6B@  (vtable, 50 slots) */
AT(0x15a48) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x20473b60),  /* retail VA, becomes fs98_simsui_check_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20473c10),  /* retail VA, becomes fs98_simsui_check_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x204741c0),  /* retail VA, becomes fs98_simsui_check_draw_item */
            U32(0x20473c20),  /* retail VA, becomes fs98_simsui_check_init */
/* RVA 0x15b10  ?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B */
AT(0x15b10) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20485b18),  /* retail VA, image, RVA 0x15b18 */
/* RVA 0x15b18  ?_messageEntries@CSimsComboBox@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x15b18) U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
            U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x20474c10),  /* retail VA, becomes fs98_simsui_combo_erase */
/* RVA 0x15b60  ??_7CSimsComboBox@@6B@  (vtable, 52 slots) */
AT(0x15b60) U32(0x20482fc8),  /* retail VA, becomes import 237: #3582 */
            U32(0x204746e0),  /* retail VA, becomes fs98_simsui_combo_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20474c00),  /* retail VA, becomes fs98_simsui_combo_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482fc2),  /* retail VA, becomes import 236: #4398 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20474910),  /* retail VA, becomes fs98_simsui_combo_draw_item */
            U32(0x20474cc0),  /* retail VA, becomes fs98_simsui_combo_measure */
            U32(0x20474ad0),  /* retail VA, becomes fs98_simsui_combo_compare */
            U32(0x20482fbc),  /* retail VA, becomes import 235: #2411 */
/* RVA 0x15c30  ?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B */
AT(0x15c30) U32(0x2049008c),  /* retail VA, image, RVA 0x2008c */
            U32(0x000010dc), U32(0x0000ffff), U32(0x00000000),
            U32(0x20474d40),  /* retail VA, becomes fs98_simsui_dialog_base_class */
            U32(0x00000000),
/* RVA 0x15c48  ?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B */
AT(0x15c48) U32(0x20475120),  /* retail VA, becomes fs98_simsui_dialog_base_message_map */
            U32(0x20485c50),  /* retail VA, image, RVA 0x15c50 */
/* RVA 0x15c50  ?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x15c50) U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x20475b00),  /* retail VA, becomes fs98_simsui_dialog_erase */
            U32(0x0000002c), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000020),
            U32(0x20475cb0),  /* retail VA, becomes fs98_simsui_dialog_measure */
            U32(0x0000030f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000023),
            U32(0x20475d40),  /* retail VA, becomes fs98_simsui_dialog_query_palette */
            U32(0x00000311), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x20475e00),  /* retail VA, becomes fs98_simsui_palette_changed */
            U32(0x00000006), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000001c),
            U32(0x20476290),  /* retail VA, becomes fs98_simsui_dialog_activate */
            U32(0x00000084), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000022),
            U32(0x20475e20),  /* retail VA, becomes fs98_simsui_dialog_hit_test */
            U32(0x00000210), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000012),
            U32(0x20476850),  /* retail VA, becomes fs98_simsui_dialog_parent_notify */
            U32(0x00000111), U32(0x00000000), U32(0x00002711), U32(0x00002711),
            U32(0x0000000c),
            U32(0x20475cd0),  /* retail VA, becomes fs98_simsui_dialog_sys_exit */
            U32(0x00000111), U32(0x00000000), U32(0x00002710), U32(0x00002710),
            U32(0x0000000c),
            U32(0x20475cf0),  /* retail VA, becomes fs98_simsui_dialog_sys_help */
            U32(0x00000469), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x204751a0),  /* retail VA, becomes fs98_simsui_dialog_parent */
            U32(0x000005f4), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x204767b0),  /* retail VA, becomes fs98_simsui_dialog_edit_kill_focus */
            U32(0x000005f5), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20476790),  /* retail VA, becomes fs98_simsui_dialog_edit_set_focus */
            U32(0x000005fa), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20475190),  /* retail VA, becomes fs98_simsui_safe_focus */
            U32(0x00000400), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20475140),  /* retail VA, becomes fs98_simsui_dialog_default_id */
/* RVA 0x15dbc  meaning not yet known */
AT(0x15dbc) U32(0x0000f823), U32(0x00000001), U32(0x0000f815), U32(0x00000002),
            U32(0x0000f828), U32(0x00000003), U32(0x0000f819), U32(0x00000004),
            U32(0x0000f814), U32(0x00000005), U32(0x0000f82e), U32(0x00000006),
            U32(0x0000f82d), U32(0x00000007), U32(0x0000f80f), U32(0x00000008),
            U32(0x0000f81b), U32(0x00000009), U32(0x0000f827), U32(0x0000000a),
            U32(0x0000f826), U32(0x0000000b), U32(0x0000f81e), U32(0x0000000c),
            U32(0x0000f81f), U32(0x0000000d), U32(0x0000f847), U32(0x0000000e),
            U32(0x0000f821), U32(0x0000000f), U32(0x0000f81c), U32(0x00000010),
            U32(0x0000f818), U32(0x00001000), U32(0x0000f820), U32(0x00001001),
            U32(0x0000f82a), U32(0x00001002), U32(0x0000f810), U32(0x00001003),
            U32(0x0000f812), U32(0x00001004), U32(0x0000f813), U32(0x00001005),
            U32(0x0000f816), U32(0x00001006), U32(0x0000f811), U32(0x00001007),
            U32(0x0000f81a), U32(0x00001008), U32(0x0000f82b), U32(0x00001009),
            U32(0x0000f829), U32(0x0000100a), U32(0x0000f824), U32(0x0000100b),
            U32(0x0000f825), U32(0x0000100c), U32(0x0000f822), U32(0x0000100d),
            U32(0x0000f817), U32(0x0000100e), U32(0x0000f82c), U32(0x0000100f),
            U32(0x0000f81d), U32(0x00001010), U32(0x0000f80e), U32(0x00001011),
            U32(0x0000f846), U32(0x00001012), U32(0x0000f84c), U32(0x00001013),
            U32(0x0000f84d), U32(0xffffffff), U32(0x0000f83c), U32(0xfffffffe),
            U32(0x0000f83c),
/* RVA 0x15ef0  ??_7CSimsDialog@@6B@  (vtable, 73 slots) */
AT(0x15ef0) U32(0x20474d50),  /* retail VA, becomes fs98_simsui_dialog_runtime_class */
            U32(0x20474f30),  /* retail VA, becomes fs98_simsui_dialog_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482f0e),  /* retail VA, becomes import 203: #4425 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20475130),  /* retail VA, becomes fs98_simsui_dialog_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x204764c0),  /* retail VA, becomes fs98_simsui_dialog_command */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x20475110),  /* retail VA, becomes fs98_simsui_dialog_exchange */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482f08),  /* retail VA, becomes import 202: #5280 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482f02),  /* retail VA, becomes import 201: #1775 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482efc),  /* retail VA, becomes import 200: #6052 */
            U32(0x20482ec6),  /* retail VA, becomes import 191: #2514 */
            U32(0x20475870),  /* retail VA, becomes fs98_simsui_dialog_on_init */
            U32(0x20482ef6),  /* retail VA, becomes import 199: #4998 */
            U32(0x20482ef0),  /* retail VA, becomes import 198: #4853 */
            U32(0x20482eea),  /* retail VA, becomes import 197: #4376 */
            U32(0x20482ee4),  /* retail VA, becomes import 196: #5265 */
            U32(0x20471610),  /* retail VA, becomes fs98_simsui_no_error */
            U32(0x20471610),  /* retail VA, becomes fs98_simsui_no_error */
            U32(0x20476820),  /* retail VA, becomes fs98_simsui_dialog_validate */
            U32(0x20475460),  /* retail VA, becomes fs98_simsui_dialog_init */
            U32(0x204752d0),  /* retail VA, becomes fs98_simsui_dialog_init_tab */
            U32(0x204751e0),  /* retail VA, becomes fs98_simsui_dialog_close */
            U32(0x20476ab0),  /* retail VA, becomes fs98_simsui_cdib_runtime_class */
            U32(0x20476b70),  /* retail VA, becomes fs98_simsui_cdib_delete */
            U32(0x20475110),  /* retail VA, becomes fs98_simsui_dialog_exchange */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204771a0),  /* retail VA, becomes fs98_simsui_cdib_load_name */
            U32(0x20476fc0),  /* retail VA, becomes fs98_simsui_cdib_load_resource */
            U32(0x20476d70),  /* retail VA, becomes fs98_simsui_cdib_load_file */
            U32(0x204772c0),  /* retail VA, becomes fs98_simsui_cdib_draw */
            U32(0x20476b40),  /* retail VA, becomes fs98_simsui_cdib_width */
            U32(0x20476b50),  /* retail VA, becomes fs98_simsui_cdib_storage_width */
            U32(0x20476b60),  /* retail VA, becomes fs98_simsui_cdib_height */
            U32(0x20477320),  /* retail VA, becomes fs98_simsui_cdib_map_to_palette */
            U32(0x00000000),
            U32(0x2048306a),  /* retail VA, becomes import 266: #3616 */
            U32(0x20477450),  /* retail VA, becomes fs98_simsui_file_exception_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20483064),  /* retail VA, becomes import 265: #3127 */
            U32(0x2048305e),  /* retail VA, becomes import 256: #5651 */
            U32(0x00000000), U32(0x00000000), U32(0x3fe00000), U32(0x00000000),
            U32(0x40500000), U32(0x00000000), U32(0x40504000), U32(0x00000000),
            U32(0x40620000), U32(0x00000000), U32(0x40622000), U32(0x00000000),
            U32(0x40660000), U32(0x00000000), U32(0x40662000), U32(0x00000000),
            U32(0x406b0000),
/* RVA 0x16078  ??_7CDIBSection@@6B@  (vtable, 5 slots) */
AT(0x16078) U32(0x20482d3a),  /* retail VA, becomes import 124: #3663 */
            U32(0x20477b60),  /* retail VA, becomes fs98_simsui_dib_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
            U32(0x20483088),  /* retail VA, becomes import 263: #3571 */
            U32(0x20478f40),  /* retail VA, becomes fs98_simsui_gdi_object_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
/* RVA 0x160a8  ?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B */
AT(0x160a8) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x204860b0),  /* retail VA, image, RVA 0x160b0 */
/* RVA 0x160b0  ?_messageEntries@CSimsEdit@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x160b0) U32(0x0000bc19), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000005),
            U32(0x20479290),  /* retail VA, becomes fs98_simsui_edit_colour */
            U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
            U32(0x00000102), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x204792e0),  /* retail VA, becomes fs98_simsui_edit_char */
            U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x204794c0),  /* retail VA, becomes fs98_simsui_edit_set_focus */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x20479520),  /* retail VA, becomes fs98_simsui_edit_kill_focus */
/* RVA 0x16140  ??_7CSimsEdit@@6B@  (vtable, 53 slots) */
AT(0x16140) U32(0x2048309a),  /* retail VA, becomes import 211: #3610 */
            U32(0x20478fe0),  /* retail VA, becomes fs98_simsui_edit_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x204790b0),  /* retail VA, becomes fs98_simsui_edit_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x204830a0),  /* retail VA, becomes import 210: #3573 */
            U32(0x20479560),  /* retail VA, becomes fs98_simsui_brush_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
/* RVA 0x16218  ??_7CSimsFont@@6B@  (vtable, 5 slots) */
AT(0x16218) U32(0x20482d3a),  /* retail VA, becomes import 124: #3663 */
            U32(0x20479650),  /* retail VA, becomes fs98_simsui_sims_font_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
/* RVA 0x16230  ?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B */
AT(0x16230) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20486238),  /* retail VA, image, RVA 0x16238 */
/* RVA 0x16238  ?_messageEntries@CSimsGroupbox@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16238) U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x20479fc0),  /* retail VA, becomes fs98_simsui_groupbox_paint */
/* RVA 0x16268  ??_7CSimsGroupbox@@6B@  (vtable, 96 slots) */
AT(0x16268) U32(0x20482e5a),  /* retail VA, becomes import 172: #3721 */
            U32(0x20479e60),  /* retail VA, becomes fs98_simsui_groupbox_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20479f20),  /* retail VA, becomes fs98_simsui_groupbox_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20482e5a),  /* retail VA, becomes import 172: #3721 */
            U32(0x2047a4c0),  /* retail VA, becomes fs98_simsui_cstatic_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20482f80),  /* retail VA, becomes import 224: #3402 */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x163e8  ?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B */
AT(0x163e8) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x204863f0),  /* retail VA, image, RVA 0x163f0 */
/* RVA 0x163f0  ?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x163f0) U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x2047aa60),  /* retail VA, becomes fs98_simsui_check_list_erase */
            U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
/* RVA 0x16438  ??_7CSimsListBox@@6B@  (vtable, 54 slots) */
AT(0x16438) U32(0x20483112),  /* retail VA, becomes import 104: #3639 */
            U32(0x2047a500),  /* retail VA, becomes fs98_simsui_list_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047a580),  /* retail VA, becomes fs98_simsui_list_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x2048310c),  /* retail VA, becomes import 105: #4401 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x2047a740),  /* retail VA, becomes fs98_simsui_list_draw_item */
            U32(0x20483106),  /* retail VA, becomes import 106: #4219 */
            U32(0x2047a910),  /* retail VA, becomes fs98_simsui_list_compare */
            U32(0x20483100),  /* retail VA, becomes import 107: #2413 */
            U32(0x204830fa),  /* retail VA, becomes import 108: #6366 */
            U32(0x204830f4),  /* retail VA, becomes import 109: #1771 */
/* RVA 0x16510  ?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B */
AT(0x16510) U32(0x2047ab00),  /* retail VA, becomes fs98_simsui_check_list_base_message_map */
            U32(0x20486518),  /* retail VA, image, RVA 0x16518 */
/* RVA 0x16518  ?_messageEntries@CSimsCheckListBox@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16518) U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x2047aa60),  /* retail VA, becomes fs98_simsui_check_list_erase */
            U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
/* RVA 0x16560  ??_7CSimsCheckListBox@@6B@  (vtable, 55 slots) */
AT(0x16560) U32(0x20483136),  /* retail VA, becomes import 98: #3577 */
            U32(0x2047aa90),  /* retail VA, becomes fs98_simsui_check_list_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047ab10),  /* retail VA, becomes fs98_simsui_check_list_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20483130),  /* retail VA, becomes import 99: #4397 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x2047aca0),  /* retail VA, becomes fs98_simsui_check_list_draw_item */
            U32(0x2048312a),  /* retail VA, becomes import 100: #4217 */
            U32(0x2047ae80),  /* retail VA, becomes fs98_simsui_check_list_compare */
            U32(0x20483100),  /* retail VA, becomes import 107: #2413 */
            U32(0x204830fa),  /* retail VA, becomes import 108: #6366 */
            U32(0x204830f4),  /* retail VA, becomes import 109: #1771 */
            U32(0x20483124),  /* retail VA, becomes import 101: #4644 */
            U32(0x00000000),
/* RVA 0x16640  ?messageMap@CSimsScrollBar@@1UAFX_MSGMAP@@B */
AT(0x16640) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20486648),  /* retail VA, image, RVA 0x16648 */
/* RVA 0x16648  ?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B */
/* RVA 0x16660  ??_7CNotify@@6B@  (vtable, 48 slots) */
AT(0x16660) U32(0x2048313c),  /* retail VA, becomes import 96: #3742 */
            U32(0x2047b010),  /* retail VA, becomes fs98_simsui_notify_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20482f80),  /* retail VA, becomes import 224: #3402 */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x16720  ?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B */
AT(0x16720) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20486728),  /* retail VA, image, RVA 0x16728 */
/* RVA 0x16728  ?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16728) U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x2047b910),  /* retail VA, becomes fs98_simsui_picture_erase */
            U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x2047b930),  /* retail VA, becomes fs98_simsui_picture_paint */
            U32(0x00000102), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x2047b770),  /* retail VA, becomes fs98_simsui_picture_on_char */
            U32(0x00000100), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x2047b7e0),  /* retail VA, becomes fs98_simsui_picture_key_down */
            U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x2047b850),  /* retail VA, becomes fs98_simsui_picture_set_focus */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x2047b8b0),  /* retail VA, becomes fs98_simsui_picture_kill_focus */
/* RVA 0x167d0  ??_7CSimsPicture@@6B@  (vtable, 48 slots) */
AT(0x167d0) U32(0x20482e5a),  /* retail VA, becomes import 172: #3721 */
            U32(0x2047b1b0),  /* retail VA, becomes fs98_simsui_picture_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047b760),  /* retail VA, becomes fs98_simsui_picture_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x16890  ?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B */
AT(0x16890) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20486898),  /* retail VA, image, RVA 0x16898 */
/* RVA 0x16898  ?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B */
/* RVA 0x168b0  ??_7CSimsPictureView@@6B@  (vtable, 48 slots) */
AT(0x168b0) U32(0x2048313c),  /* retail VA, becomes import 96: #3742 */
            U32(0x2047bb50),  /* retail VA, becomes fs98_simsui_view_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047bbb0),  /* retail VA, becomes fs98_simsui_view_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x16970  ?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B */
AT(0x16970) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20486978),  /* retail VA, image, RVA 0x16978 */
/* RVA 0x16978  ?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16978) U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
            U32(0x00000201), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047c6f0),  /* retail VA, becomes fs98_simsui_radio_button_down */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047c710),  /* retail VA, becomes fs98_simsui_radio_button_up */
            U32(0x00000101), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x2047c7b0),  /* retail VA, becomes fs98_simsui_radio_key_up */
            U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x20474470),  /* retail VA, becomes fs98_simsui_erase_nothing */
            U32(0x00000007), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x2047aa60),  /* retail VA, becomes fs98_simsui_check_list_erase */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x2047c810),  /* retail VA, becomes fs98_simsui_radio_kill_focus */
            U32(0x000005f8), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047c780),  /* retail VA, becomes fs98_simsui_radio_checked */
            U32(0x000005fa), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20475190),  /* retail VA, becomes fs98_simsui_safe_focus */
/* RVA 0x16a68  ??_7CSimsRadioButton@@6B@  (vtable, 50 slots) */
AT(0x16a68) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x2047bc30),  /* retail VA, becomes fs98_simsui_radio_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047bce0),  /* retail VA, becomes fs98_simsui_radio_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x2047c360),  /* retail VA, becomes fs98_simsui_radio_draw_item */
            U32(0x2047bcf0),  /* retail VA, becomes fs98_simsui_radio_init */
/* RVA 0x16b30  ?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B */
AT(0x16b30) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20486b38),  /* retail VA, image, RVA 0x16b38 */
/* RVA 0x16b38  ?_messageEntries@CSimsShadowButton@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16b38) U32(0x00000201), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047d150),  /* retail VA, becomes fs98_simsui_shadow_button_down */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047d200),  /* retail VA, becomes fs98_simsui_shadow_button_up */
            U32(0x00000200), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047d2d0),  /* retail VA, becomes fs98_simsui_shadow_button_move */
/* RVA 0x16b98  ??_7CSimsShadowButton@@6B@  (vtable, 50 slots) */
AT(0x16b98) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x2047c860),  /* retail VA, becomes fs98_simsui_shadow_button_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047c930),  /* retail VA, becomes fs98_simsui_shadow_button_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20473880),  /* retail VA, becomes fs98_simsui_button_draw_item */
            U32(0x20473730),  /* retail VA, becomes fs98_simsui_button_init */
/* RVA 0x16c60  ??_7CSimsShadow@@6B@  (vtable, 5 slots) */
AT(0x16c60) U32(0x20482d3a),  /* retail VA, becomes import 124: #3663 */
            U32(0x2047d420),  /* retail VA, becomes fs98_simsui_shadow_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x00000000),
/* RVA 0x16c78  ?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B */
AT(0x16c78) U32(0x20473720),  /* retail VA, becomes fs98_simsui_slider_base_message_map */
            U32(0x20486c80),  /* retail VA, image, RVA 0x16c80 */
/* RVA 0x16c80  ?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16c80) U32(0x00000201), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047e3b0),  /* retail VA, becomes fs98_simsui_slider_button_down */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047e410),  /* retail VA, becomes fs98_simsui_slider_button_up */
            U32(0x00000200), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047e480),  /* retail VA, becomes fs98_simsui_slider_move */
            U32(0x00000100), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047e5e0),  /* retail VA, becomes fs98_simsui_slider_key_down */
            U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x2047ece0),  /* retail VA, becomes fs98_simsui_slider_erase */
            U32(0x00000087), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000023),
            U32(0x2047ee90),  /* retail VA, becomes fs98_simsui_slider_dialog_code */
            U32(0x00000085), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x2047eea0),  /* retail VA, becomes fs98_simsui_slider_nc_paint */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x2047eed0),  /* retail VA, becomes fs98_simsui_slider_kill_focus */
            U32(0x000005f7), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047eef0),  /* retail VA, becomes fs98_simsui_slider_clicked */
/* RVA 0x16d70  ??_7CSimsSlider@@6B@  (vtable, 50 slots) */
AT(0x16d70) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x2047dba0),  /* retail VA, becomes fs98_simsui_slider_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047dce0),  /* retail VA, becomes fs98_simsui_slider_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x2047e970),  /* retail VA, becomes fs98_simsui_slider_draw_item */
            U32(0x20473730),  /* retail VA, becomes fs98_simsui_button_init */
/* RVA 0x16e38  ?messageMap@CSimsTabCtrlHelper@@1UAFX_MSGMAP@@B */
AT(0x16e38) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20486e40),  /* retail VA, image, RVA 0x16e40 */
/* RVA 0x16e40  ?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16e40) U32(0x0000002b), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000020),
            U32(0x2047f0f0),  /* retail VA, becomes fs98_simsui_helper_draw_item */
            U32(0x00000201), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047f100),  /* retail VA, becomes fs98_simsui_helper_button_down */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x2047f100),  /* retail VA, becomes fs98_simsui_helper_button_down */
            U32(0x00000105), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x2047f110),  /* retail VA, becomes fs98_simsui_helper_sys_key_up */
/* RVA 0x16eb8  ??_7CSimsTabCtrlHelper@@6B@  (vtable, 49 slots) */
AT(0x16eb8) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x2047efa0),  /* retail VA, becomes fs98_simsui_helper_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047f0e0),  /* retail VA, becomes fs98_simsui_helper_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20475110),  /* retail VA, becomes fs98_simsui_dialog_exchange */
            U32(0x00000000),
/* RVA 0x16f80  ?classCSimsTabPage@CSimsTabPage@@2UCRuntimeClass@@B */
AT(0x16f80) U32(0x20490af4),  /* retail VA, image, RVA 0x20af4 */
            U32(0x00001118), U32(0x0000ffff), U32(0x00000000),
            U32(0x20474d50),  /* retail VA, becomes fs98_simsui_dialog_runtime_class */
            U32(0x00000000),
/* RVA 0x16f98  ?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B */
AT(0x16f98) U32(0x20475130),  /* retail VA, becomes fs98_simsui_dialog_message_map */
            U32(0x20486fa0),  /* retail VA, image, RVA 0x16fa0 */
/* RVA 0x16fa0  ?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x16fa0) U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x2047f400),  /* retail VA, becomes fs98_simsui_page_paint */
            U32(0x0000030f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000023),
            U32(0x2047f3f0),  /* retail VA, becomes fs98_simsui_page_query_palette */
            U32(0x00000311), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x20475e00),  /* retail VA, becomes fs98_simsui_palette_changed */
            U32(0x00000006), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000001c),
            U32(0x2047f420),  /* retail VA, becomes fs98_simsui_page_activate */
            U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x2047f460),  /* retail VA, becomes fs98_simsui_page_erase */
            U32(0x00000087), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000023),
            U32(0x2047f4e0),  /* retail VA, becomes fs98_simsui_page_dialog_code */
            U32(0x00000010), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x204716a0),  /* retail VA, becomes fs98_simsui_nothing */
/* RVA 0x17060  ??_7CSimsTabPage@@6B@  (vtable, 61 slots) */
AT(0x17060) U32(0x2047f140),  /* retail VA, becomes fs98_simsui_page_runtime_class */
            U32(0x2047f1a0),  /* retail VA, becomes fs98_simsui_page_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482f0e),  /* retail VA, becomes import 203: #4425 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x2047f3d0),  /* retail VA, becomes fs98_simsui_page_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x204764c0),  /* retail VA, becomes fs98_simsui_dialog_command */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x2047f3c0),  /* retail VA, becomes fs98_simsui_page_exchange */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x2047f500),  /* retail VA, becomes fs98_simsui_page_pre_translate */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x2047f4f0),  /* retail VA, becomes fs98_simsui_page_post_destroy */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482f02),  /* retail VA, becomes import 201: #1775 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482efc),  /* retail VA, becomes import 200: #6052 */
            U32(0x20482ec6),  /* retail VA, becomes import 191: #2514 */
            U32(0x2047f3e0),  /* retail VA, becomes fs98_simsui_page_on_init */
            U32(0x20482ef6),  /* retail VA, becomes import 199: #4998 */
            U32(0x20482ef0),  /* retail VA, becomes import 198: #4853 */
            U32(0x20482eea),  /* retail VA, becomes import 197: #4376 */
            U32(0x20482ee4),  /* retail VA, becomes import 196: #5265 */
            U32(0x20471610),  /* retail VA, becomes fs98_simsui_no_error */
            U32(0x20471610),  /* retail VA, becomes fs98_simsui_no_error */
            U32(0x20476820),  /* retail VA, becomes fs98_simsui_dialog_validate */
            U32(0x20475460),  /* retail VA, becomes fs98_simsui_dialog_init */
            U32(0x204752d0),  /* retail VA, becomes fs98_simsui_dialog_init_tab */
            U32(0x204751e0),  /* retail VA, becomes fs98_simsui_dialog_close */
            U32(0x204716a0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x00000000),
/* RVA 0x17158  ?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B */
AT(0x17158) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20487160),  /* retail VA, image, RVA 0x17160 */
/* RVA 0x17160  ?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x17160) U32(0x00000201), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20480de0),  /* retail VA, becomes fs98_simsui_tabs_button_down */
            U32(0x00000202), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000031),
            U32(0x20480e60),  /* retail VA, becomes fs98_simsui_tabs_button_up */
            U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x20480ea0),  /* retail VA, becomes fs98_simsui_tabs_erase */
            U32(0x00000087), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000023),
            U32(0x20481170),  /* retail VA, becomes fs98_simsui_tabs_dialog_code */
            U32(0x00000085), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x20481180),  /* retail VA, becomes fs98_simsui_tabs_nc_paint */
            U32(0x0000002b), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000020),
            U32(0x20480930),  /* retail VA, becomes fs98_simsui_tabs_on_draw_item */
            U32(0x00000100), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x204811a0),  /* retail VA, becomes fs98_simsui_tabs_key_down */
            U32(0x00000105), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x204815c0),  /* retail VA, becomes fs98_simsui_tabs_sys_key_up */
/* RVA 0x17238  ??_7CSimsTabCtrl@@6B@  (vtable, 50 slots) */
AT(0x17238) U32(0x20482f74),  /* retail VA, becomes import 222: #3574 */
            U32(0x2047f670),  /* retail VA, becomes fs98_simsui_tabs_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20480920),  /* retail VA, becomes fs98_simsui_tabs_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482f2c),  /* retail VA, becomes import 83: #4396 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0x20480940),  /* retail VA, becomes fs98_simsui_tabs_draw_item */
            U32(0x20481560),  /* retail VA, becomes fs98_simsui_tabs_close */
/* RVA 0x17300  ?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B */
AT(0x17300) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20487308),  /* retail VA, image, RVA 0x17308 */
/* RVA 0x17308  ?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B */
/* RVA 0x17320  ?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B */
AT(0x17320) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20487328),  /* retail VA, image, RVA 0x17328 */
/* RVA 0x17328  ?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B */
/* RVA 0x17340  ?messageMap@CSimsText@@1UAFX_MSGMAP@@B */
AT(0x17340) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20487348),  /* retail VA, image, RVA 0x17348 */
/* RVA 0x17348  ?_messageEntries@CSimsText@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x17348) U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x20481a30),  /* retail VA, becomes fs98_simsui_text_paint */
            U32(0x0000000a), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000d),
            U32(0x20475e00),  /* retail VA, becomes fs98_simsui_palette_changed */
            U32(0x00000014), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000001),
            U32(0x20474470),  /* retail VA, becomes fs98_simsui_erase_nothing */
            U32(0x00000084), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000022),
            U32(0x20473420),  /* retail VA, becomes fs98_simsui_hit_client */
/* RVA 0x173c0  ??_7CSimsText@@6B@  (vtable, 48 slots) */
AT(0x173c0) U32(0x20482e5a),  /* retail VA, becomes import 172: #3721 */
            U32(0x204816b0),  /* retail VA, becomes fs98_simsui_text_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20481780),  /* retail VA, becomes fs98_simsui_text_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x17480  ?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B */
AT(0x17480) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20487488),  /* retail VA, image, RVA 0x17488 */
/* RVA 0x17488  ?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x17488) U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x204824d0),  /* retail VA, becomes fs98_simsui_text_art_paint */
/* RVA 0x174be  meaning not yet known */
AT(0x174be) U16(0x403d),
/* RVA 0x174c0  ??_7CSimsTextArt@@6B@  (vtable, 48 slots) */
AT(0x174c0) U32(0x20482e5a),  /* retail VA, becomes import 172: #3721 */
            U32(0x20481c60),  /* retail VA, becomes fs98_simsui_text_art_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x204824c0),  /* retail VA, becomes fs98_simsui_text_art_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x17580  ?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B */
AT(0x17580) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20487588),  /* retail VA, image, RVA 0x17588 */
/* RVA 0x17588  ?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x17588) U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x20482690),  /* retail VA, becomes fs98_simsui_hot_key_on_help */
            U32(0x00000100), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x20482860),  /* retail VA, becomes fs98_simsui_hot_key_key_down */
            U32(0x00000104), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x20482940),  /* retail VA, becomes fs98_simsui_hot_key_sys_key_down */
            U32(0x00000101), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x20482960),  /* retail VA, becomes fs98_simsui_hot_key_key_up */
            U32(0x00000105), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000010),
            U32(0x204829a0),  /* retail VA, becomes fs98_simsui_hot_key_sys_key_up */
            U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x20482760),  /* retail VA, becomes fs98_simsui_hot_key_paint */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x204829d0),  /* retail VA, becomes fs98_simsui_hot_joy_kill_focus */
            U32(0x00000087), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000023),
            U32(0x204829c0),  /* retail VA, becomes fs98_simsui_hot_key_get_dlg_code */
/* RVA 0x17660  ??_7CSimsHotKey@@6B@  (vtable, 48 slots) */
AT(0x17660) U32(0x2048313c),  /* retail VA, becomes import 96: #3742 */
            U32(0x204825c0),  /* retail VA, becomes fs98_simsui_hot_key_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20482680),  /* retail VA, becomes fs98_simsui_hot_key_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
/* RVA 0x17720  ?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B */
AT(0x17720) U32(0x2047a570),  /* retail VA, becomes fs98_simsui_button_base_message_map */
            U32(0x20487728),  /* retail VA, image, RVA 0x17728 */
/* RVA 0x17728  ?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B */
AT(0x17728) U32(0x00000053), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000a),
            U32(0x2047a590),  /* retail VA, becomes fs98_simsui_help_to_parent */
            U32(0x0000000f), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x0000000c),
            U32(0x20482c10),  /* retail VA, becomes fs98_simsui_hot_joy_paint */
            U32(0x00000008), U32(0x00000000), U32(0x00000000), U32(0x00000000),
            U32(0x00000017),
            U32(0x204829d0),  /* retail VA, becomes fs98_simsui_hot_joy_kill_focus */
/* RVA 0x17788  ??_7CSimsHotJoy@@6B@  (vtable, 48 slots) */
AT(0x17788) U32(0x2048313c),  /* retail VA, becomes import 96: #3742 */
            U32(0x20482a10),  /* retail VA, becomes fs98_simsui_hot_joy_delete */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204710b0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x20482e30),  /* retail VA, becomes import 165: #4424 */
            U32(0x20482e2a),  /* retail VA, becomes import 164: #4627 */
            U32(0x20482e24),  /* retail VA, becomes import 163: #4080 */
            U32(0x20482e1e),  /* retail VA, becomes import 162: #3079 */
            U32(0x20482e18),  /* retail VA, becomes import 161: #3825 */
            U32(0x20482e12),  /* retail VA, becomes import 160: #3831 */
            U32(0x20482e0c),  /* retail VA, becomes import 159: #3830 */
            U32(0x20482ad0),  /* retail VA, becomes fs98_simsui_hot_joy_message_map */
            U32(0x20482e06),  /* retail VA, becomes import 158: #2976 */
            U32(0x20482e00),  /* retail VA, becomes import 157: #3081 */
            U32(0x20482dfa),  /* retail VA, becomes import 156: #2985 */
            U32(0x20482df4),  /* retail VA, becomes import 155: #3262 */
            U32(0x20482dee),  /* retail VA, becomes import 154: #3136 */
            U32(0x20482de8),  /* retail VA, becomes import 153: #4465 */
            U32(0x20482de2),  /* retail VA, becomes import 152: #3259 */
            U32(0x20482ddc),  /* retail VA, becomes import 151: #3147 */
            U32(0x20482dd6),  /* retail VA, becomes import 150: #2982 */
            U32(0x20482dd0),  /* retail VA, becomes import 149: #5277 */
            U32(0x20482dca),  /* retail VA, becomes import 148: #2124 */
            U32(0x20482dc4),  /* retail VA, becomes import 147: #2446 */
            U32(0x20482dbe),  /* retail VA, becomes import 146: #5261 */
            U32(0x20482db8),  /* retail VA, becomes import 145: #1727 */
            U32(0x20482db2),  /* retail VA, becomes import 144: #5065 */
            U32(0x20482dac),  /* retail VA, becomes import 143: #3749 */
            U32(0x20482da6),  /* retail VA, becomes import 142: #6376 */
            U32(0x20482da0),  /* retail VA, becomes import 141: #2055 */
            U32(0x20482d9a),  /* retail VA, becomes import 140: #2648 */
            U32(0x20482d94),  /* retail VA, becomes import 139: #4441 */
            U32(0x20482d8e),  /* retail VA, becomes import 138: #4837 */
            U32(0x20482d88),  /* retail VA, becomes import 137: #3798 */
            U32(0x204713b0),  /* retail VA, becomes fs98_simsui_nothing_with */
            U32(0x204713c0),  /* retail VA, becomes fs98_simsui_window_disable */
            U32(0x204713d0),  /* retail VA, becomes fs98_simsui_window_enable */
            U32(0x20482d82),  /* retail VA, becomes import 136: #5290 */
            U32(0x20482d7c),  /* retail VA, becomes import 135: #4353 */
            U32(0x20482d76),  /* retail VA, becomes import 134: #6374 */
            U32(0x20482d70),  /* retail VA, becomes import 133: #5163 */
            U32(0x20482d6a),  /* retail VA, becomes import 132: #2385 */
            U32(0x20482d64),  /* retail VA, becomes import 131: #5241 */
            U32(0x20482d5e),  /* retail VA, becomes import 130: #4407 */
            U32(0x20482d58),  /* retail VA, becomes import 129: #1776 */
            U32(0x20482d52),  /* retail VA, becomes import 128: #4078 */
            U32(0x20482d4c),  /* retail VA, becomes import 127: #6055 */
            U32(0xffffffff), U32(0x00000000),
            U32(0x204831f3),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000), U32(0xffffffff),
            U32(0x20483270),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x2048327d),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000), U32(0xffffffff), U32(0x00000000),
            U32(0x2048337f),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000), U32(0x19930520), U32(0x00000001),
            U32(0x20487898),  /* retail VA, image, RVA 0x17898 */
/* RVA 0x17898  meaning not yet known */
AT(0x17898) U32(0xffffffff),
            U32(0x20483560),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204878c0),  /* retail VA, image, RVA 0x178c0 */
/* RVA 0x178c0  meaning not yet known */
AT(0x178c0) U32(0xffffffff),
            U32(0x20483580),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204878e8),  /* retail VA, image, RVA 0x178e8 */
/* RVA 0x178e8  meaning not yet known */
AT(0x178e8) U32(0xffffffff),
            U32(0x204835a0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487910),  /* retail VA, image, RVA 0x17910 */
/* RVA 0x17910  meaning not yet known */
AT(0x17910) U32(0xffffffff),
            U32(0x204835c0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0xffffffff),
            U32(0x204835ca),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487940),  /* retail VA, image, RVA 0x17940 */
/* RVA 0x17940  meaning not yet known */
AT(0x17940) U32(0xffffffff),
            U32(0x204835e0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487968),  /* retail VA, image, RVA 0x17968 */
/* RVA 0x17968  meaning not yet known */
AT(0x17968) U32(0xffffffff),
            U32(0x20483600),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487990),  /* retail VA, image, RVA 0x17990 */
/* RVA 0x17990  meaning not yet known */
AT(0x17990) U32(0xffffffff),
            U32(0x20483620),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000006),
            U32(0x204879b8),  /* retail VA, image, RVA 0x179b8 */
/* RVA 0x179b8  meaning not yet known */
AT(0x179b8) U32(0xffffffff),
            U32(0x20483640),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x2048364b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483656),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x2048365e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20483669),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20483674),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487a08),  /* retail VA, image, RVA 0x17a08 */
/* RVA 0x17a08  meaning not yet known */
AT(0x17a08) U32(0xffffffff),
            U32(0x20483690),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487a30),  /* retail VA, image, RVA 0x17a30 */
/* RVA 0x17a30  meaning not yet known */
AT(0x17a30) U32(0xffffffff),
            U32(0x204836b0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487a58),  /* retail VA, image, RVA 0x17a58 */
/* RVA 0x17a58  meaning not yet known */
AT(0x17a58) U32(0xffffffff),
            U32(0x204836d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204836d8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487a88),  /* retail VA, image, RVA 0x17a88 */
/* RVA 0x17a88  meaning not yet known */
AT(0x17a88) U32(0xffffffff),
            U32(0x204836f0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204836f8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487ab8),  /* retail VA, image, RVA 0x17ab8 */
/* RVA 0x17ab8  meaning not yet known */
AT(0x17ab8) U32(0xffffffff),
            U32(0x20483710),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487ae0),  /* retail VA, image, RVA 0x17ae0 */
/* RVA 0x17ae0  meaning not yet known */
AT(0x17ae0) U32(0xffffffff),
            U32(0x20483730),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000007),
            U32(0x20487b08),  /* retail VA, image, RVA 0x17b08 */
/* RVA 0x17b08  meaning not yet known */
AT(0x17b08) U32(0xffffffff),
            U32(0x20483750),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x2048375b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483766),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x2048376e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20483779),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20483784),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x2048378f),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487b60),  /* retail VA, image, RVA 0x17b60 */
/* RVA 0x17b60  meaning not yet known */
AT(0x17b60) U32(0xffffffff),
            U32(0x204837b0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204837b8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487b90),  /* retail VA, image, RVA 0x17b90 */
/* RVA 0x17b90  meaning not yet known */
AT(0x17b90) U32(0xffffffff),
            U32(0x204837d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487bb8),  /* retail VA, image, RVA 0x17bb8 */
/* RVA 0x17bb8  meaning not yet known */
AT(0x17bb8) U32(0xffffffff),
            U32(0x204837f0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204837f8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487be8),  /* retail VA, image, RVA 0x17be8 */
/* RVA 0x17be8  meaning not yet known */
AT(0x17be8) U32(0xffffffff),
            U32(0x20483810),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483818),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487c18),  /* retail VA, image, RVA 0x17c18 */
/* RVA 0x17c18  meaning not yet known */
AT(0x17c18) U32(0xffffffff),
            U32(0x20483830),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483838),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487c48),  /* retail VA, image, RVA 0x17c48 */
/* RVA 0x17c48  meaning not yet known */
AT(0x17c48) U32(0xffffffff),
            U32(0x20483850),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0xffffffff),
            U32(0x20483858),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487c78),  /* retail VA, image, RVA 0x17c78 */
/* RVA 0x17c78  meaning not yet known */
AT(0x17c78) U32(0xffffffff),
            U32(0x20483870),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000012),
            U32(0x20487ca0),  /* retail VA, image, RVA 0x17ca0 */
/* RVA 0x17ca0  meaning not yet known */
AT(0x17ca0) U32(0xffffffff),
            U32(0x20483890),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483898),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204838a6),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x204838b4),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x204838c2),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x204838d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000005),
            U32(0x204838de),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000006),
            U32(0x204838ec),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000007),
            U32(0x204838fa),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000008),
            U32(0x20483908),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000009),
            U32(0x20483916),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000a),
            U32(0x20483924),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000b),
            U32(0x20483932),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000c),
            U32(0x20483940),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000d),
            U32(0x2048394e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000e),
            U32(0x2048395c),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000f),
            U32(0x2048396a),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000010),
            U32(0x20483978),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000013),
            U32(0x20487d50),  /* retail VA, image, RVA 0x17d50 */
/* RVA 0x17d50  meaning not yet known */
AT(0x17d50) U32(0xffffffff),
            U32(0x20483990),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483998),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204839a6),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x204839b4),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x204839c2),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x204839d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000005),
            U32(0x204839de),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000006),
            U32(0x204839ec),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000007),
            U32(0x204839fa),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000008),
            U32(0x20483a08),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000009),
            U32(0x20483a16),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000a),
            U32(0x20483a24),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000b),
            U32(0x20483a32),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000c),
            U32(0x20483a40),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000d),
            U32(0x20483a4e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000e),
            U32(0x20483a5c),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000f),
            U32(0x20483a6a),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000010),
            U32(0x20483a78),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x0000000f),
            U32(0x20483a86),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487e08),  /* retail VA, image, RVA 0x17e08 */
/* RVA 0x17e08  meaning not yet known */
AT(0x17e08) U32(0xffffffff),
            U32(0x20483aa0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487e30),  /* retail VA, image, RVA 0x17e30 */
/* RVA 0x17e30  meaning not yet known */
AT(0x17e30) U32(0xffffffff),
            U32(0x20483ac0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487e58),  /* retail VA, image, RVA 0x17e58 */
/* RVA 0x17e58  meaning not yet known */
AT(0x17e58) U32(0xffffffff),
            U32(0x20483ae0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487e80),  /* retail VA, image, RVA 0x17e80 */
/* RVA 0x17e80  meaning not yet known */
AT(0x17e80) U32(0xffffffff),
            U32(0x20483b00),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487ea8),  /* retail VA, image, RVA 0x17ea8 */
/* RVA 0x17ea8  meaning not yet known */
AT(0x17ea8) U32(0xffffffff),
            U32(0x20483b20),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000008),
            U32(0x20487ed0),  /* retail VA, image, RVA 0x17ed0 */
/* RVA 0x17ed0  meaning not yet known */
AT(0x17ed0) U32(0xffffffff),
            U32(0x20483b40),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483b4b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483b56),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20483b61),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20483b6c),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20483b77),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000005),
            U32(0x20483b82),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000005),
            U32(0x20483b8d),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20487f30),  /* retail VA, image, RVA 0x17f30 */
/* RVA 0x17f30  meaning not yet known */
AT(0x17f30) U32(0xffffffff),
            U32(0x20483bb0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483bb8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487f60),  /* retail VA, image, RVA 0x17f60 */
/* RVA 0x17f60  meaning not yet known */
AT(0x17f60) U32(0xffffffff),
            U32(0x20483bd0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487f88),  /* retail VA, image, RVA 0x17f88 */
/* RVA 0x17f88  meaning not yet known */
AT(0x17f88) U32(0xffffffff),
            U32(0x20483bf0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20487fb0),  /* retail VA, image, RVA 0x17fb0 */
/* RVA 0x17fb0  meaning not yet known */
AT(0x17fb0) U32(0xffffffff),
            U32(0x20483c10),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000006),
            U32(0x20487fd8),  /* retail VA, image, RVA 0x17fd8 */
/* RVA 0x17fd8  meaning not yet known */
AT(0x17fd8) U32(0xffffffff),
            U32(0x20483c30),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483c38),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483c40),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483c48),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483c50),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483c58),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488028),  /* retail VA, image, RVA 0x18028 */
/* RVA 0x18028  meaning not yet known */
AT(0x18028) U32(0xffffffff),
            U32(0x20483c70),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488050),  /* retail VA, image, RVA 0x18050 */
/* RVA 0x18050  meaning not yet known */
AT(0x18050) U32(0xffffffff),
            U32(0x20483c90),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0xffffffff),
            U32(0x20483c98),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000003),
            U32(0x20488080),  /* retail VA, image, RVA 0x18080 */
/* RVA 0x18080  meaning not yet known */
AT(0x18080) U32(0xffffffff),
            U32(0x20483cb0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483cb8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483cc3),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204880b8),  /* retail VA, image, RVA 0x180b8 */
/* RVA 0x180b8  meaning not yet known */
AT(0x180b8) U32(0xffffffff),
            U32(0x20483ce0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204880e0),  /* retail VA, image, RVA 0x180e0 */
/* RVA 0x180e0  meaning not yet known */
AT(0x180e0) U32(0xffffffff),
            U32(0x20483d00),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488108),  /* retail VA, image, RVA 0x18108 */
/* RVA 0x18108  meaning not yet known */
AT(0x18108) U32(0xffffffff),
            U32(0x20483d20),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488130),  /* retail VA, image, RVA 0x18130 */
/* RVA 0x18130  meaning not yet known */
AT(0x18130) U32(0xffffffff),
            U32(0x20483d40),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488158),  /* retail VA, image, RVA 0x18158 */
/* RVA 0x18158  meaning not yet known */
AT(0x18158) U32(0xffffffff),
            U32(0x20483d60),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0xffffffff),
            U32(0x20483d68),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488188),  /* retail VA, image, RVA 0x18188 */
/* RVA 0x18188  meaning not yet known */
AT(0x18188) U32(0xffffffff),
            U32(0x20483d80),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483d88),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204881b8),  /* retail VA, image, RVA 0x181b8 */
/* RVA 0x181b8  meaning not yet known */
AT(0x181b8) U32(0xffffffff),
            U32(0x20483da0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204881e0),  /* retail VA, image, RVA 0x181e0 */
/* RVA 0x181e0  meaning not yet known */
AT(0x181e0) U32(0xffffffff),
            U32(0x20483dc0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483dc8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488210),  /* retail VA, image, RVA 0x18210 */
/* RVA 0x18210  meaning not yet known */
AT(0x18210) U32(0xffffffff),
            U32(0x20483de0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483de8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488240),  /* retail VA, image, RVA 0x18240 */
/* RVA 0x18240  meaning not yet known */
AT(0x18240) U32(0xffffffff),
            U32(0x20483e00),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483e08),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x0000000b),
            U32(0x20488270),  /* retail VA, image, RVA 0x18270 */
/* RVA 0x18270  meaning not yet known */
AT(0x18270) U32(0xffffffff),
            U32(0x20483e20),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483e28),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483e68),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483e70),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483e30),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483e38),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483e40),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000006),
            U32(0x20483e48),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000007),
            U32(0x20483e50),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000007),
            U32(0x20483e58),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483e60),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204882e8),  /* retail VA, image, RVA 0x182e8 */
/* RVA 0x182e8  meaning not yet known */
AT(0x182e8) U32(0xffffffff),
            U32(0x20483e90),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488310),  /* retail VA, image, RVA 0x18310 */
/* RVA 0x18310  meaning not yet known */
AT(0x18310) U32(0xffffffff),
            U32(0x20483eb0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000012),
            U32(0x20488338),  /* retail VA, image, RVA 0x18338 */
/* RVA 0x18338  meaning not yet known */
AT(0x18338) U32(0xffffffff),
            U32(0x20483ed0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483ed8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483ee3),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20483eee),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20483ef6),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20483efe),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20483f09),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20483f11),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483f19),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483f24),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20483f2f),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20483f37),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483f3f),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483f4a),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20483f55),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20483f5d),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20483f65),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483f70),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204883e8),  /* retail VA, image, RVA 0x183e8 */
/* RVA 0x183e8  meaning not yet known */
AT(0x183e8) U32(0xffffffff),
            U32(0x20483f90),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483f98),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488418),  /* retail VA, image, RVA 0x18418 */
/* RVA 0x18418  meaning not yet known */
AT(0x18418) U32(0xffffffff),
            U32(0x20483fb0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483fb8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488448),  /* retail VA, image, RVA 0x18448 */
/* RVA 0x18448  meaning not yet known */
AT(0x18448) U32(0xffffffff),
            U32(0x20483fd0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483fd8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488478),  /* retail VA, image, RVA 0x18478 */
/* RVA 0x18478  meaning not yet known */
AT(0x18478) U32(0xffffffff),
            U32(0x20483ff0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20483ff8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204884a8),  /* retail VA, image, RVA 0x184a8 */
/* RVA 0x184a8  meaning not yet known */
AT(0x184a8) U32(0xffffffff),
            U32(0x20484010),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484018),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204884d8),  /* retail VA, image, RVA 0x184d8 */
/* RVA 0x184d8  meaning not yet known */
AT(0x184d8) U32(0xffffffff),
            U32(0x20484030),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484038),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488508),  /* retail VA, image, RVA 0x18508 */
/* RVA 0x18508  meaning not yet known */
AT(0x18508) U32(0xffffffff),
            U32(0x20484050),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488530),  /* retail VA, image, RVA 0x18530 */
/* RVA 0x18530  meaning not yet known */
AT(0x18530) U32(0xffffffff),
            U32(0x20484070),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488558),  /* retail VA, image, RVA 0x18558 */
/* RVA 0x18558  meaning not yet known */
AT(0x18558) U32(0xffffffff),
            U32(0x20484090),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000003),
            U32(0x20488580),  /* retail VA, image, RVA 0x18580 */
/* RVA 0x18580  meaning not yet known */
AT(0x18580) U32(0xffffffff),
            U32(0x204840b0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204840bb),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204840c3),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204885b8),  /* retail VA, image, RVA 0x185b8 */
/* RVA 0x185b8  meaning not yet known */
AT(0x185b8) U32(0xffffffff),
            U32(0x204840e0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204840e8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204885e8),  /* retail VA, image, RVA 0x185e8 */
/* RVA 0x185e8  meaning not yet known */
AT(0x185e8) U32(0xffffffff),
            U32(0x20484100),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488610),  /* retail VA, image, RVA 0x18610 */
/* RVA 0x18610  meaning not yet known */
AT(0x18610) U32(0xffffffff),
            U32(0x20484120),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000007),
            U32(0x20488638),  /* retail VA, image, RVA 0x18638 */
/* RVA 0x18638  meaning not yet known */
AT(0x18638) U32(0xffffffff),
            U32(0x20484140),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x2048414b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20484156),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x2048415e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20484169),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20484174),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x2048417f),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488690),  /* retail VA, image, RVA 0x18690 */
/* RVA 0x18690  meaning not yet known */
AT(0x18690) U32(0xffffffff),
            U32(0x204841a0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204841a8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204886c0),  /* retail VA, image, RVA 0x186c0 */
/* RVA 0x186c0  meaning not yet known */
AT(0x186c0) U32(0xffffffff),
            U32(0x204841c0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000006),
            U32(0x204886e8),  /* retail VA, image, RVA 0x186e8 */
/* RVA 0x186e8  meaning not yet known */
AT(0x186e8) U32(0xffffffff),
            U32(0x204841e0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204841eb),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204841f6),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x204841fe),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20484209),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20484214),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488738),  /* retail VA, image, RVA 0x18738 */
/* RVA 0x18738  meaning not yet known */
AT(0x18738) U32(0xffffffff),
            U32(0x20484230),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488760),  /* retail VA, image, RVA 0x18760 */
/* RVA 0x18760  meaning not yet known */
AT(0x18760) U32(0xffffffff),
            U32(0x20484250),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000004),
            U32(0x20488788),  /* retail VA, image, RVA 0x18788 */
/* RVA 0x18788  meaning not yet known */
AT(0x18788) U32(0xffffffff),
            U32(0x20484270),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x2048427b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20484283),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x2048428e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x204887c8),  /* retail VA, image, RVA 0x187c8 */
/* RVA 0x187c8  meaning not yet known */
AT(0x187c8) U32(0xffffffff),
            U32(0x204842b0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000007),
            U32(0x204887f0),  /* retail VA, image, RVA 0x187f0 */
/* RVA 0x187f0  meaning not yet known */
AT(0x187f0) U32(0xffffffff),
            U32(0x204842d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204842d8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204842e6),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x204842f4),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20484302),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20484310),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000005),
            U32(0x2048431e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000008),
            U32(0x20488848),  /* retail VA, image, RVA 0x18848 */
/* RVA 0x18848  meaning not yet known */
AT(0x18848) U32(0xffffffff),
            U32(0x20484340),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484348),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20484356),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20484364),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x20484372),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20484380),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000005),
            U32(0x2048438e),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000006),
            U32(0x2048439c),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000003),
            U32(0x204888a8),  /* retail VA, image, RVA 0x188a8 */
/* RVA 0x188a8  meaning not yet known */
AT(0x188a8) U32(0xffffffff),
            U32(0x204843c0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204843c8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204843d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204888e0),  /* retail VA, image, RVA 0x188e0 */
/* RVA 0x188e0  meaning not yet known */
AT(0x188e0) U32(0xffffffff),
            U32(0x204843f0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204843f8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488910),  /* retail VA, image, RVA 0x18910 */
/* RVA 0x18910  meaning not yet known */
AT(0x18910) U32(0xffffffff),
            U32(0x20484410),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488938),  /* retail VA, image, RVA 0x18938 */
/* RVA 0x18938  meaning not yet known */
AT(0x18938) U32(0xffffffff),
            U32(0x20484430),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000006),
            U32(0x20488960),  /* retail VA, image, RVA 0x18960 */
/* RVA 0x18960  meaning not yet known */
AT(0x18960) U32(0xffffffff),
            U32(0x20484450),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484458),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20484471),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x2048447f),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x2048448d),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x2048449b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204889b0),  /* retail VA, image, RVA 0x189b0 */
/* RVA 0x189b0  meaning not yet known */
AT(0x189b0) U32(0xffffffff),
            U32(0x204844c0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204844cb),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x204889e0),  /* retail VA, image, RVA 0x189e0 */
/* RVA 0x189e0  meaning not yet known */
AT(0x189e0) U32(0xffffffff),
            U32(0x204844e0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204844eb),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000006),
            U32(0x20488a10),  /* retail VA, image, RVA 0x18a10 */
/* RVA 0x18a10  meaning not yet known */
AT(0x18a10) U32(0xffffffff),
            U32(0x20484500),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484508),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x20484521),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x2048452f),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x2048453d),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x2048454b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488a60),  /* retail VA, image, RVA 0x18a60 */
/* RVA 0x18a60  meaning not yet known */
AT(0x18a60) U32(0xffffffff),
            U32(0x20484570),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484578),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000005),
            U32(0x20488a90),  /* retail VA, image, RVA 0x18a90 */
/* RVA 0x18a90  meaning not yet known */
AT(0x18a90) U32(0xffffffff),
            U32(0x20484590),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x2048459b),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204845a3),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x204845ae),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x204845b9),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488ad8),  /* retail VA, image, RVA 0x18ad8 */
/* RVA 0x18ad8  meaning not yet known */
AT(0x18ad8) U32(0xffffffff),
            U32(0x204845d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204845d8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488b08),  /* retail VA, image, RVA 0x18b08 */
/* RVA 0x18b08  meaning not yet known */
AT(0x18b08) U32(0xffffffff),
            U32(0x204845f0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488b30),  /* retail VA, image, RVA 0x18b30 */
/* RVA 0x18b30  meaning not yet known */
AT(0x18b30) U32(0xffffffff),
            U32(0x20484610),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484618),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488b60),  /* retail VA, image, RVA 0x18b60 */
/* RVA 0x18b60  meaning not yet known */
AT(0x18b60) U32(0xffffffff),
            U32(0x20484630),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484638),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488b90),  /* retail VA, image, RVA 0x18b90 */
/* RVA 0x18b90  meaning not yet known */
AT(0x18b90) U32(0xffffffff),
            U32(0x20484650),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x20484658),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488bc0),  /* retail VA, image, RVA 0x18bc0 */
/* RVA 0x18bc0  meaning not yet known */
AT(0x18bc0) U32(0xffffffff),
            U32(0x20484670),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488be8),  /* retail VA, image, RVA 0x18be8 */
/* RVA 0x18be8  meaning not yet known */
AT(0x18be8) U32(0xffffffff),
            U32(0x20484690),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000003),
            U32(0x20488c10),  /* retail VA, image, RVA 0x18c10 */
/* RVA 0x18c10  meaning not yet known */
AT(0x18c10) U32(0xffffffff),
            U32(0x204846b0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204846b8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0xffffffff),
            U32(0x204846c3),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000009),
            U32(0x20488c48),  /* retail VA, image, RVA 0x18c48 */
/* RVA 0x18c48  meaning not yet known */
AT(0x18c48) U32(0xffffffff),
            U32(0x204846e0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204846eb),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000001),
            U32(0x204846f6),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20484701),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000003),
            U32(0x2048470c),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20484717),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000004),
            U32(0x20484722),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000006),
            U32(0x2048472d),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000002),
            U32(0x20484738),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488cb0),  /* retail VA, image, RVA 0x18cb0 */
/* RVA 0x18cb0  meaning not yet known */
AT(0x18cb0) U32(0xffffffff),
            U32(0x20484750),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488cd8),  /* retail VA, image, RVA 0x18cd8 */
/* RVA 0x18cd8  meaning not yet known */
AT(0x18cd8) U32(0xffffffff),
            U32(0x20484770),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0xffffffff),
            U32(0x20484778),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000001),
            U32(0x20488d08),  /* retail VA, image, RVA 0x18d08 */
/* RVA 0x18d08  meaning not yet known */
AT(0x18d08) U32(0xffffffff),
            U32(0x20484790),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488d30),  /* retail VA, image, RVA 0x18d30 */
/* RVA 0x18d30  meaning not yet known */
AT(0x18d30) U32(0xffffffff),
            U32(0x204847b0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0xffffffff),
            U32(0x204847b8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x19930520), U32(0x00000002),
            U32(0x20488d60),  /* retail VA, image, RVA 0x18d60 */
/* RVA 0x18d60  meaning not yet known */
AT(0x18d60) U32(0xffffffff),
            U32(0x204847d0),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000),
            U32(0x204847d8),  /* retail VA, becomes 0 (unwind funclet or EH table entry) */
            U32(0x00000000), U32(0x33e95106), U32(0x00000000), U32(0x0001a2b0),
            U32(0x00000001), U32(0x0000021c), U32(0x0000021c), U32(0x00018d98),
            U32(0x00019608), U32(0x00019e78), U32(0x000010a0), U32(0x00007b30),
            U32(0x0000afe0), U32(0x000023b0), U32(0x00003600), U32(0x00003af0),
            U32(0x0000aa70), U32(0x000046c0), U32(0x00004d60), U32(0x00008fb0),
            U32(0x000034b0), U32(0x000096a0), U32(0x000095f0), U32(0x00009e00),
            U32(0x00003380), U32(0x000129e0), U32(0x00012590), U32(0x0000a4e0),
            U32(0x0000b140), U32(0x0000bb30), U32(0x0000bbc0), U32(0x00001440),
            U32(0x0000d3c0), U32(0x0000c830), U32(0x0000dad0), U32(0x00001390),
            U32(0x000014c0), U32(0x0000f590), U32(0x0000ef80), U32(0x0000f150),
            U32(0x00011640), U32(0x00011bd0), U32(0x000010c0), U32(0x00007bb0),
            U32(0x0000b130), U32(0x00002460), U32(0x000036c0), U32(0x00003bc0),
            U32(0x0000aae0), U32(0x00004730), U32(0x00004f90), U32(0x00009030),
            U32(0x00003530), U32(0x00009700), U32(0x00009ec0), U32(0x00003400),
            U32(0x00012a60), U32(0x00012610), U32(0x0000a550), U32(0x0000b210),
            U32(0x0000bba0), U32(0x0000bc90), U32(0x00001460), U32(0x0000d480),
            U32(0x0000c8c0), U32(0x0000dc00), U32(0x000013e0), U32(0x000014e0),
            U32(0x0000f790), U32(0x0000eff0), U32(0x0000f200), U32(0x00011710),
            U32(0x00011cc0), U32(0x000012c0), U32(0x000012c0), U32(0x000012c0),
            U32(0x000012c0), U32(0x000012c0), U32(0x000012c0), U32(0x000012c0),
            U32(0x000012c0), U32(0x000012c0), U32(0x000012c0), U32(0x000012c0),
            U32(0x000012c0), U32(0x000012c0), U32(0x000012c0), U32(0x000012c0),
            U32(0x000012c0), U32(0x000012c0), U32(0x000012c0), U32(0x000012c0),
            U32(0x000012c0), U32(0x000012c0), U32(0x00001310), U32(0x00001310),
            U32(0x00001310), U32(0x00001310), U32(0x00001310), U32(0x00001310),
            U32(0x00001310), U32(0x00001310), U32(0x00001310), U32(0x00001310),
            U32(0x00001310), U32(0x00001310), U32(0x00001310), U32(0x00001310),
            U32(0x00001310), U32(0x00001310), U32(0x00001310), U32(0x00001310),
            U32(0x00001310), U32(0x00001310), U32(0x00001310), U32(0x00001210),
            U32(0x00001200), U32(0x00015000), U32(0x00016078), U32(0x00016660),
            U32(0x00015478), U32(0x000157d8), U32(0x00015a48), U32(0x00016560),
            U32(0x00015b60), U32(0x00015ef0), U32(0x00016140), U32(0x00015710),
            U32(0x00016218), U32(0x00016268), U32(0x00015648), U32(0x00017788),
            U32(0x00017660), U32(0x00016438), U32(0x000167d0), U32(0x000168b0),
            U32(0x00016a68), U32(0x00015108), U32(0x00016c60), U32(0x00016b98),
            U32(0x00016d70), U32(0x00015048), U32(0x000151c8), U32(0x00017238),
            U32(0x00016eb8), U32(0x00017060), U32(0x000173c0), U32(0x000174c0),
            U32(0x000012d0), U32(0x00001770), U32(0x0000fc40), U32(0x00003200),
            U32(0x00008b80), U32(0x00008b10), U32(0x00008bc0), U32(0x00008b50),
            U32(0x00008d90), U32(0x00003280), U32(0x00001810), U32(0x000082d0),
            U32(0x000051e0), U32(0x00011560), U32(0x0000ae80), U32(0x00004ad0),
            U32(0x0000a910), U32(0x00009580), U32(0x000086f0), U32(0x00008750),
            U32(0x00008720), U32(0x00008790), U32(0x000074c0), U32(0x00012ae0),
            U32(0x00012ae0), U32(0x000080b0), U32(0x00008600), U32(0x000031c0),
            U32(0x00009290), U32(0x000032a0), U32(0x00001610), U32(0x00005110),
            U32(0x0000f3c0), U32(0x00005e40), U32(0x000075b0), U32(0x0000d940),
            U32(0x00003880), U32(0x000041c0), U32(0x0000aca0), U32(0x00004910),
            U32(0x0000a740), U32(0x0000c360), U32(0x0000e970), U32(0x00010940),
            U32(0x00005110), U32(0x00005230), U32(0x00009770), U32(0x0000f850),
            U32(0x000081d0), U32(0x00008200), U32(0x000102b0), U32(0x00010630),
            U32(0x00001240), U32(0x00001200), U32(0x00001280), U32(0x00001220),
            U32(0x00001630), U32(0x00001660), U32(0x000012e0), U32(0x00008d40),
            U32(0x00001300), U32(0x00001230), U32(0x00001340), U32(0x00001340),
            U32(0x00001340), U32(0x0000e780), U32(0x00012c00), U32(0x00012750),
            U32(0x000015d0), U32(0x0000b120), U32(0x000024f0), U32(0x00003720),
            U32(0x00003c10), U32(0x0000ab10), U32(0x00004c00), U32(0x00005130),
            U32(0x000090b0), U32(0x00003540), U32(0x00009f20), U32(0x00003410),
            U32(0x00012ad0), U32(0x00012680), U32(0x0000a580), U32(0x0000b760),
            U32(0x0000bbb0), U32(0x0000bce0), U32(0x0000afd0), U32(0x0000c930),
            U32(0x0000dce0), U32(0x00011630), U32(0x00011620), U32(0x00010920),
            U32(0x0000f0e0), U32(0x0000f3d0), U32(0x00011780), U32(0x000124c0),
            U32(0x00001270), U32(0x00007580), U32(0x0000dab0), U32(0x00001620),
            U32(0x00001650), U32(0x000012f0), U32(0x000023a0), U32(0x00004d50),
            U32(0x0000f140), U32(0x0000e6e0), U32(0x00002100), U32(0x000015a0),
            U32(0x00001640), U32(0x00001250), U32(0x00001350), U32(0x00001350),
            U32(0x00001350), U32(0x00001260), U32(0x000119a0), U32(0x00011a00),
            U32(0x00002500), U32(0x00003730), U32(0x00003c20), U32(0x0000ab20),
            U32(0x00004750), U32(0x00005460), U32(0x000090c0), U32(0x00003550),
            U32(0x00009f30), U32(0x00003430), U32(0x00012b10), U32(0x000126a0),
            U32(0x0000a5c0), U32(0x0000b4a0), U32(0x0000bcf0), U32(0x000014f0),
            U32(0x0000d4d0), U32(0x0000c940), U32(0x0000dcf0), U32(0x000014f0),
            U32(0x000014f0), U32(0x0000f8b0), U32(0x0000f000), U32(0x0000f210),
            U32(0x0000f2d0), U32(0x00011790), U32(0x00011d40), U32(0x00007810),
            U32(0x000016a0), U32(0x000052d0), U32(0x00001360), U32(0x00001320),
            U32(0x00001290), U32(0x00007c40), U32(0x00004cc0), U32(0x00011410),
            U32(0x00006290), U32(0x0000f420), U32(0x00004600), U32(0x000092e0),
            U32(0x0000b770), U32(0x000016a0), U32(0x000064c0), U32(0x00010930),
            U32(0x0000f0f0), U32(0x000067b0), U32(0x00006790), U32(0x00005e00),
            U32(0x00004470), U32(0x0000aa60), U32(0x00004c10), U32(0x00005b00),
            U32(0x0000aa60), U32(0x0000b910), U32(0x00004470), U32(0x0000ece0),
            U32(0x00010ea0), U32(0x0000f460), U32(0x00004470), U32(0x00005140),
            U32(0x00003360), U32(0x000129c0), U32(0x0000ee90), U32(0x00011170),
            U32(0x0000f4e0), U32(0x0000a590), U32(0x0000a590), U32(0x0000a590),
            U32(0x0000a590), U32(0x0000a590), U32(0x00003420), U32(0x00003420),
            U32(0x0000a590), U32(0x00012690), U32(0x0000a590), U32(0x0000a590),
            U32(0x00005870), U32(0x0000f3e0), U32(0x00012860), U32(0x0000b7e0),
            U32(0x000111a0), U32(0x000045b0), U32(0x00012960), U32(0x0000c7b0),
            U32(0x00003320), U32(0x000045e0), U32(0x00009520), U32(0x000129d0),
            U32(0x000129d0), U32(0x0000b8b0), U32(0x0000c810), U32(0x0000eed0),
            U32(0x00004560), U32(0x00002f30), U32(0x00004500), U32(0x0000c6f0),
            U32(0x0000d150), U32(0x0000e3b0), U32(0x00010de0), U32(0x0000f100),
            U32(0x00002fe0), U32(0x00004480), U32(0x000035a0), U32(0x00003480),
            U32(0x0000c710), U32(0x0000d200), U32(0x0000e410), U32(0x00010e60),
            U32(0x0000f100), U32(0x000051a0), U32(0x00005cb0), U32(0x000030e0),
            U32(0x0000d2d0), U32(0x0000e480), U32(0x0000e5e0), U32(0x00005e20),
            U32(0x00003420), U32(0x0000eea0), U32(0x00011180), U32(0x00009fc0),
            U32(0x00012c10), U32(0x00012760), U32(0x0000b930), U32(0x0000f400),
            U32(0x00011a30), U32(0x000124d0), U32(0x00005e00), U32(0x00005e00),
            U32(0x00006850), U32(0x00005d40), U32(0x0000f3f0), U32(0x0000c780),
            U32(0x00005190), U32(0x00005190), U32(0x00005190), U32(0x000094c0),
            U32(0x000035e0), U32(0x000035e0), U32(0x0000b850), U32(0x0000aa60),
            U32(0x0000eef0), U32(0x00005cd0), U32(0x00005cf0), U32(0x00012940),
            U32(0x000129a0), U32(0x000115c0), U32(0x0000f110), U32(0x00006a00),
            U32(0x0000f4f0), U32(0x0000f500), U32(0x000114c0), U32(0x00002260),
            U32(0x000022b0), U32(0x000021d0), U32(0x00001850), U32(0x00001940),
            U32(0x000015b0), U32(0x000015b0), U32(0x000015b0), U32(0x0000b070),
            U32(0x0000b100), U32(0x000102d0), U32(0x00010640), U32(0x00001580),
            U32(0x00004440), U32(0x0000c5d0), U32(0x00001000), U32(0x00006960),
            U32(0x00012b90), U32(0x00012720), U32(0x0000b260), U32(0x00010320),
            U32(0x000069b0), U32(0x0000e6f0), U32(0x0000e720), U32(0x00007730),
            U32(0x00001590), U32(0x00009db0), U32(0x0000e840), U32(0x00006910),
            U32(0x000118c0), U32(0x000079a0), U32(0x000084a0), U32(0x00008430),
            U32(0x00008470), U32(0x000084e0), U32(0x00001260), U32(0x00008850),
            U32(0x00001370), U32(0x000032d0), U32(0x0000c670), U32(0x00006820),
            U32(0x00001610), U32(0x00002390), U32(0x00004d40), U32(0x00004d50),
            U32(0x00003720), U32(0x0000a570), U32(0x0000a570), U32(0x0000ab00),
            U32(0x0000a570), U32(0x00005120), U32(0x0000a570), U32(0x00003720),
            U32(0x0000a570), U32(0x00003720), U32(0x0000a570), U32(0x0000a570),
            U32(0x0000a570), U32(0x0000a570), U32(0x0000a570), U32(0x0000a570),
            U32(0x0000a570), U32(0x0000a570), U32(0x00003720), U32(0x0000a570),
            U32(0x0000a570), U32(0x0000a570), U32(0x0000a570), U32(0x00005130),
            U32(0x0000a570), U32(0x0000a570), U32(0x000153b8), U32(0x00015618),
            U32(0x00015970), U32(0x00016518), U32(0x00015b18), U32(0x00015c50),
            U32(0x000160b0), U32(0x000155b0), U32(0x00016238), U32(0x00015548),
            U32(0x00017728), U32(0x00017588), U32(0x000163f0), U32(0x00016728),
            U32(0x00016898), U32(0x00016978), U32(0x00016648), U32(0x00016b38),
            U32(0x00016c80), U32(0x00017328), U32(0x00017308), U32(0x00017160),
            U32(0x00016e40), U32(0x00016fa0), U32(0x00017348), U32(0x00017488),
            U32(0x00015398), U32(0x00015c30), U32(0x00016f80), U32(0x000213d0),
            U32(0x000153b0), U32(0x00015610), U32(0x00015968), U32(0x00016510),
            U32(0x00015b10), U32(0x00015c48), U32(0x000160a8), U32(0x000155a8),
            U32(0x00016230), U32(0x00015540), U32(0x00017720), U32(0x00017580),
            U32(0x000163e8), U32(0x00016720), U32(0x00016890), U32(0x00016970),
            U32(0x00016640), U32(0x00016b30), U32(0x00016c78), U32(0x00017320),
            U32(0x00017300), U32(0x00017158), U32(0x00016e38), U32(0x00016f98),
            U32(0x00017340), U32(0x00017480), U32(0x000213cc), U32(0x000204f0),
            U32(0x000204d8), U32(0x00001b20), U32(0x00001b00), U32(0x00001b00),
            U32(0x00020028), U32(0x00020040), U32(0x0001a2bb), U32(0x0001a2ce),
            U32(0x0001a2e5), U32(0x0001a2fa), U32(0x0001a317), U32(0x0001a32e),
            U32(0x0001a34a), U32(0x0001a367), U32(0x0001a380), U32(0x0001a3a1),
            U32(0x0001a3b6), U32(0x0001a3d1), U32(0x0001a3f0), U32(0x0001a405),
            U32(0x0001a41e), U32(0x0001a439), U32(0x0001a450), U32(0x0001a467),
            U32(0x0001a47f), U32(0x0001a497), U32(0x0001a4b3), U32(0x0001a4cf),
            U32(0x0001a4e9), U32(0x0001a500), U32(0x0001a51d), U32(0x0001a534),
            U32(0x0001a553), U32(0x0001a56a), U32(0x0001a582), U32(0x0001a5a0),
            U32(0x0001a5c2), U32(0x0001a5d7), U32(0x0001a5ef), U32(0x0001a602),
            U32(0x0001a619), U32(0x0001a62c), U32(0x0001a649), U32(0x0001a660),
            U32(0x0001a67c), U32(0x0001a699), U32(0x0001a6b2), U32(0x0001a6c9),
            U32(0x0001a6de), U32(0x0001a6f9), U32(0x0001a70e), U32(0x0001a727),
            U32(0x0001a742), U32(0x0001a759), U32(0x0001a770), U32(0x0001a788),
            U32(0x0001a7a0), U32(0x0001a7bc), U32(0x0001a7d8), U32(0x0001a7f2),
            U32(0x0001a809), U32(0x0001a826), U32(0x0001a83d), U32(0x0001a85c),
            U32(0x0001a873), U32(0x0001a88b), U32(0x0001a8a9), U32(0x0001a8c1),
            U32(0x0001a8d6), U32(0x0001a8ee), U32(0x0001a903), U32(0x0001a91c),
            U32(0x0001a931), U32(0x0001a94a), U32(0x0001a968), U32(0x0001a987),
            U32(0x0001a9a2), U32(0x0001a9bb), U32(0x0001a9d2), U32(0x0001a9e9),
            U32(0x0001aa02), U32(0x0001aa1b), U32(0x0001aa35), U32(0x0001aa4f),
            U32(0x0001aa6d), U32(0x0001aa8b), U32(0x0001aaa4), U32(0x0001aabe),
            U32(0x0001aade), U32(0x0001aaf5), U32(0x0001ab0f), U32(0x0001ab24),
            U32(0x0001ab3d), U32(0x0001ab52), U32(0x0001ab6b), U32(0x0001ab89),
            U32(0x0001aba8), U32(0x0001abc3), U32(0x0001abdc), U32(0x0001abf3),
            U32(0x0001ac0a), U32(0x0001ac23), U32(0x0001ac3c), U32(0x0001ac56),
            U32(0x0001ac70), U32(0x0001ac8e), U32(0x0001acac), U32(0x0001acc5),
            U32(0x0001acdf), U32(0x0001acff), U32(0x0001ad16), U32(0x0001ad30),
            U32(0x0001ad54), U32(0x0001ad76), U32(0x0001ad87), U32(0x0001ad9c),
            U32(0x0001adad), U32(0x0001adc8), U32(0x0001addd), U32(0x0001adf7),
            U32(0x0001ae12), U32(0x0001ae29), U32(0x0001ae3e), U32(0x0001ae51),
            U32(0x0001ae6a), U32(0x0001ae7d), U32(0x0001ae94), U32(0x0001aead),
            U32(0x0001aec2), U32(0x0001aed7), U32(0x0001aeed), U32(0x0001af03),
            U32(0x0001af1d), U32(0x0001af37), U32(0x0001af4f), U32(0x0001af64),
            U32(0x0001af7f), U32(0x0001af94), U32(0x0001afb1), U32(0x0001afc6),
            U32(0x0001afdc), U32(0x0001aff8), U32(0x0001b00e), U32(0x0001b021),
            U32(0x0001b037), U32(0x0001b04b), U32(0x0001b05c), U32(0x0001b08f),
            U32(0x0001b0b9), U32(0x0001b0f3), U32(0x0001b122), U32(0x0001b14d),
            U32(0x0001b176), U32(0x0001b1b0), U32(0x0001b1db), U32(0x0001b1f2),
            U32(0x0001b21a), U32(0x0001b235), U32(0x0001b251), U32(0x0001b291),
            U32(0x0001b2cd), U32(0x0001b308), U32(0x0001b325), U32(0x0001b352),
            U32(0x0001b38a), U32(0x0001b3b1), U32(0x0001b3da), U32(0x0001b404),
            U32(0x0001b437), U32(0x0001b46a), U32(0x0001b4af), U32(0x0001b4dd),
            U32(0x0001b51e), U32(0x0001b54f), U32(0x0001b575), U32(0x0001b598),
            U32(0x0001b5ce), U32(0x0001b605), U32(0x0001b632), U32(0x0001b65b),
            U32(0x0001b67f), U32(0x0001b6b3), U32(0x0001b6ec), U32(0x0001b726),
            U32(0x0001b75c), U32(0x0001b791), U32(0x0001b7ca), U32(0x0001b7fe),
            U32(0x0001b833), U32(0x0001b86e), U32(0x0001b88f), U32(0x0001b8ca),
            U32(0x0001b8fd), U32(0x0001b91d), U32(0x0001b945), U32(0x0001b979),
            U32(0x0001b99c), U32(0x0001b9be), U32(0x0001b9e8), U32(0x0001ba29),
            U32(0x0001ba4f), U32(0x0001ba80), U32(0x0001bab2), U32(0x0001bad0),
            U32(0x0001baf8), U32(0x0001bb17), U32(0x0001bb36), U32(0x0001bb55),
            U32(0x0001bb79), U32(0x0001bb9d), U32(0x0001bbbf), U32(0x0001bbea),
            U32(0x0001bc09), U32(0x0001bc3b), U32(0x0001bc54), U32(0x0001bc8b),
            U32(0x0001bcbc), U32(0x0001bcf2), U32(0x0001bd29), U32(0x0001bd5c),
            U32(0x0001bd8d), U32(0x0001bdbc), U32(0x0001bdf1), U32(0x0001be24),
            U32(0x0001be59), U32(0x0001be8a), U32(0x0001bebb), U32(0x0001beed),
            U32(0x0001bf1f), U32(0x0001bf55), U32(0x0001bf8b), U32(0x0001bfbf),
            U32(0x0001bff6), U32(0x0001c027), U32(0x0001c060), U32(0x0001c091),
            U32(0x0001c0c3), U32(0x0001c0fb), U32(0x0001c12d), U32(0x0001c15c),
            U32(0x0001c18e), U32(0x0001c1b4), U32(0x0001c1d2), U32(0x0001c202),
            U32(0x0001c229), U32(0x0001c251), U32(0x0001c271), U32(0x0001c2ad),
            U32(0x0001c2e3), U32(0x0001c31a), U32(0x0001c33c), U32(0x0001c35e),
            U32(0x0001c38e), U32(0x0001c3bc), U32(0x0001c3da), U32(0x0001c3f8),
            U32(0x0001c41b), U32(0x0001c43e), U32(0x0001c461), U32(0x0001c487),
            U32(0x0001c4b5), U32(0x0001c51d), U32(0x0001c554), U32(0x0001c5ae),
            U32(0x0001c5ea), U32(0x0001c622), U32(0x0001c64e), U32(0x0001c682),
            U32(0x0001c6b5), U32(0x0001c6df), U32(0x0001c712), U32(0x0001c73b),
            U32(0x0001c764), U32(0x0001c79b), U32(0x0001c7e4), U32(0x0001c83e),
            U32(0x0001c85b), U32(0x0001c8a7), U32(0x0001c90f), U32(0x0001c95d),
            U32(0x0001c97f), U32(0x0001c999), U32(0x0001ca02), U32(0x0001ca34),
            U32(0x0001ca50), U32(0x0001ca98), U32(0x0001cabe), U32(0x0001caf7),
            U32(0x0001cb2c), U32(0x0001cb4f), U32(0x0001cb81), U32(0x0001cba5),
            U32(0x0001cbc9), U32(0x0001cbec), U32(0x0001cc27), U32(0x0001cc63),
            U32(0x0001cc81), U32(0x0001ccac), U32(0x0001ccd8), U32(0x0001cd03),
            U32(0x0001cd20), U32(0x0001cd40), U32(0x0001cd5e), U32(0x0001cd7f),
            U32(0x0001cdb7), U32(0x0001cdf5), U32(0x0001ce1c), U32(0x0001ce42),
            U32(0x0001ce5f), U32(0x0001ce8e), U32(0x0001cebe), U32(0x0001ceea),
            U32(0x0001cf14), U32(0x0001cf3f), U32(0x0001cf6a), U32(0x0001cf99),
            U32(0x0001cfc3), U32(0x0001cfee), U32(0x0001d019), U32(0x0001d041),
            U32(0x0001d063), U32(0x0001d08b), U32(0x0001d0ad), U32(0x0001d0cf),
            U32(0x0001d0f2), U32(0x0001d115), U32(0x0001d136), U32(0x0001d15c),
            U32(0x0001d183), U32(0x0001d1a6), U32(0x0001d1c5), U32(0x0001d1ea),
            U32(0x0001d20f), U32(0x0001d230), U32(0x0001d251), U32(0x0001d273),
            U32(0x0001d299), U32(0x0001d2bb), U32(0x0001d2de), U32(0x0001d300),
            U32(0x0001d323), U32(0x0001d346), U32(0x0001d36b), U32(0x0001d38b),
            U32(0x0001d3b0), U32(0x0001d3e0), U32(0x0001d40f), U32(0x0001d437),
            U32(0x0001d461), U32(0x0001d48b), U32(0x0001d4b6), U32(0x0001d4e5),
            U32(0x0001d50f), U32(0x0001d543), U32(0x0001d576), U32(0x0001d5a8),
            U32(0x0001d5da), U32(0x0001d60d), U32(0x0001d63a), U32(0x0001d668),
            U32(0x0001d69c), U32(0x0001d6cd), U32(0x0001d6fd), U32(0x0001d72c),
            U32(0x0001d75b), U32(0x0001d78b), U32(0x0001d7bc), U32(0x0001d7e7),
            U32(0x0001d813), U32(0x0001d845), U32(0x0001d86c), U32(0x0001d8a9),
            U32(0x0001d8da), U32(0x0001d90b), U32(0x0001d936), U32(0x0001d959),
            U32(0x0001d983), U32(0x0001d9ab), U32(0x0001d9ca), U32(0x0001d9ea),
            U32(0x0001da09), U32(0x0001da26), U32(0x0001da43), U32(0x0001da61),
            U32(0x0001da7f), U32(0x0001da9a), U32(0x0001dab8), U32(0x0001dae7),
            U32(0x0001db17), U32(0x0001db3d), U32(0x0001db64), U32(0x0001db8c),
            U32(0x0001dbb7), U32(0x0001dbdd), U32(0x0001dc03), U32(0x0001dc2e),
            U32(0x0001dc55), U32(0x0001dc82), U32(0x0001dcaf), U32(0x0001dcd9),
            U32(0x0001dd07), U32(0x0001dd2e), U32(0x0001dd4d), U32(0x0001dd6c),
            U32(0x0001dd91), U32(0x0001ddb4), U32(0x0001ddd8), U32(0x0001de02),
            U32(0x0001de35), U32(0x0001de59), U32(0x0001de8e), U32(0x0001deac),
            U32(0x0001ded4), U32(0x0001df02), U32(0x0001df2b), U32(0x0001df46),
            U32(0x0001df71), U32(0x0001df9e), U32(0x0001dfcb), U32(0x0001dffd),
            U32(0x0001e017), U32(0x0001e02e), U32(0x0001e063), U32(0x0001e092),
            U32(0x0001e0b3), U32(0x0001e0d9), U32(0x0001e0ff), U32(0x0001e126),
            U32(0x0001e14e), U32(0x0001e182), U32(0x0001e1a2), U32(0x0001e1ef),
            U32(0x0001e222), U32(0x0001e24d), U32(0x0001e270), U32(0x0001e296),
            U32(0x0001e2b7), U32(0x0001e2e8), U32(0x0001e30b), U32(0x0001e32d),
            U32(0x0001e34f), U32(0x0001e374), U32(0x0001e3b5), U32(0x0001e3f0),
            U32(0x0001e420), U32(0x0001e44a), U32(0x0001e476), U32(0x0001e498),
            U32(0x0001e4b7), U32(0x0001e4df), U32(0x0001e504), U32(0x0001e52d),
            U32(0x0001e54c), U32(0x0001e56f), U32(0x0001e5a8), U32(0x0001e5db),
            U32(0x0001e60f), U32(0x0001e64a), U32(0x0001e67f), U32(0x0001e6b9),
            U32(0x0001e6f4), U32(0x0001e72b), U32(0x0001e760), U32(0x0001e793),
            U32(0x0001e7cc), U32(0x0001e803), U32(0x0001e83c), U32(0x0001e871),
            U32(0x0001e8a6), U32(0x0001e8dc), U32(0x0001e912), U32(0x0001e94c),
            U32(0x0001e986), U32(0x0001e9be), U32(0x0001e9f9), U32(0x0001ea2e),
            U32(0x0001ea6b), U32(0x0001eaa0), U32(0x0001ead6), U32(0x0001eb12),
            U32(0x0001eb48), U32(0x0001eb7b), U32(0x0001ebb1), U32(0x0001ebed),
            U32(0x0001ec23), U32(0x0001ec5e), U32(0x0001ec9a), U32(0x0001ecd2),
            U32(0x0001ed08), U32(0x0001ed3c), U32(0x0001ed76), U32(0x0001edae),
            U32(0x0001ede8), U32(0x0001ee1e), U32(0x0001ee54), U32(0x0001ee8b),
            U32(0x0001eec2), U32(0x0001eefd), U32(0x0001ef38), U32(0x0001ef71),
            U32(0x0001efad), U32(0x0001efe3), U32(0x0001f021), U32(0x0001f057),
            U32(0x0001f08e), U32(0x0001f0cb), U32(0x0001f102), U32(0x0001f136),
            U32(0x0001f16d), U32(0x0001f1ab), U32(0x0001f1dd), U32(0x0001f211),
            U32(0x0001f241), U32(0x0001f270), U32(0x0001f299), U32(0x0001f2c7),
            U32(0x0001f2f6), U32(0x0001f321), U32(0x0001f34a), U32(0x0001f371),
            U32(0x0001f39e), U32(0x0001f3c9), U32(0x0001f3f6), U32(0x0001f41f),
            U32(0x0001f448), U32(0x0001f472), U32(0x0001f49c), U32(0x0001f4ca),
            U32(0x0001f4f8), U32(0x0001f524), U32(0x0001f553), U32(0x0001f57c),
            U32(0x0001f5ad), U32(0x0001f5d6), U32(0x0001f600), U32(0x0001f630),
            U32(0x0001f65a), U32(0x0001f681), U32(0x0001f6ab), U32(0x0001f6cc),
            U32(0x0001f6ef), U32(0x0001f714), U32(0x0001f729), U32(0x0001f73b),
            U32(0x0001f754), U32(0x0001f760), U32(0x00010000), U32(0x00030002),
            U32(0x00050004), U32(0x00070006), U32(0x00090008), U32(0x000b000a),
            U32(0x000d000c), U32(0x000f000e), U32(0x00110010), U32(0x00130012),
            U32(0x00150014), U32(0x00170016), U32(0x00190018), U32(0x001b001a),
            U32(0x001d001c), U32(0x001f001e), U32(0x00210020), U32(0x00230022),
            U32(0x00250024), U32(0x00270026), U32(0x00290028), U32(0x002b002a),
            U32(0x002d002c), U32(0x002f002e), U32(0x00310030), U32(0x00330032),
            U32(0x00350034), U32(0x00370036), U32(0x00390038), U32(0x003b003a),
            U32(0x003d003c), U32(0x003f003e), U32(0x00410040), U32(0x00430042),
            U32(0x00450044), U32(0x00470046), U32(0x00490048), U32(0x004b004a),
            U32(0x004d004c), U32(0x004f004e), U32(0x00510050), U32(0x00530052),
            U32(0x00550054), U32(0x00570056), U32(0x00590058), U32(0x005b005a),
            U32(0x005d005c), U32(0x005f005e), U32(0x00610060), U32(0x00630062),
            U32(0x00650064), U32(0x00670066), U32(0x00690068), U32(0x006b006a),
            U32(0x006d006c), U32(0x006f006e), U32(0x00710070), U32(0x00730072),
            U32(0x00750074), U32(0x00770076), U32(0x00790078), U32(0x007b007a),
            U32(0x007d007c), U32(0x007f007e), U32(0x00810080), U32(0x00830082),
            U32(0x00850084), U32(0x00870086), U32(0x00890088), U32(0x008b008a),
            U32(0x008d008c), U32(0x008f008e), U32(0x00910090), U32(0x00930092),
            U32(0x00950094), U32(0x00970096), U32(0x00990098), U32(0x009b009a),
            U32(0x009d009c), U32(0x009f009e), U32(0x00a100a0), U32(0x00a300a2),
            U32(0x00a500a4), U32(0x00a700a6), U32(0x00a900a8), U32(0x00ab00aa),
            U32(0x00ad00ac), U32(0x00af00ae), U32(0x00b100b0), U32(0x00b300b2),
            U32(0x00b500b4), U32(0x00b700b6), U32(0x00b900b8), U32(0x00bb00ba),
            U32(0x00bd00bc), U32(0x00bf00be), U32(0x00c100c0), U32(0x00c300c2),
            U32(0x00c500c4), U32(0x00c700c6), U32(0x00c900c8), U32(0x00cb00ca),
            U32(0x00cd00cc), U32(0x00cf00ce), U32(0x00d100d0), U32(0x00d300d2),
            U32(0x00d500d4), U32(0x00d700d6), U32(0x00d900d8), U32(0x00db00da),
            U32(0x00dd00dc), U32(0x00df00de), U32(0x00e100e0), U32(0x00e300e2),
            U32(0x00e500e4), U32(0x00e700e6), U32(0x00e900e8), U32(0x00eb00ea),
            U32(0x00ed00ec), U32(0x00ef00ee), U32(0x00f100f0), U32(0x00f300f2),
            U32(0x00f500f4), U32(0x00f700f6), U32(0x00f900f8), U32(0x00fb00fa),
            U32(0x00fd00fc), U32(0x00ff00fe), U32(0x01010100), U32(0x01030102),
            U32(0x01050104), U32(0x01070106), U32(0x01090108), U32(0x010b010a),
            U32(0x010d010c), U32(0x010f010e), U32(0x01110110), U32(0x01130112),
            U32(0x01150114), U32(0x01170116), U32(0x01190118), U32(0x011b011a),
            U32(0x011d011c), U32(0x011f011e), U32(0x01210120), U32(0x01230122),
            U32(0x01250124), U32(0x01270126), U32(0x01290128), U32(0x012b012a),
            U32(0x012d012c), U32(0x012f012e), U32(0x01310130), U32(0x01330132),
            U32(0x01350134), U32(0x01370136), U32(0x01390138), U32(0x013b013a),
            U32(0x013d013c), U32(0x013f013e), U32(0x01410140), U32(0x01430142),
            U32(0x01450144), U32(0x01470146), U32(0x01490148), U32(0x014b014a),
            U32(0x014d014c), U32(0x014f014e), U32(0x01510150), U32(0x01530152),
            U32(0x01550154), U32(0x01570156), U32(0x01590158), U32(0x015b015a),
            U32(0x015d015c), U32(0x015f015e), U32(0x01610160), U32(0x01630162),
            U32(0x01650164), U32(0x01670166), U32(0x01690168), U32(0x016b016a),
            U32(0x016d016c), U32(0x016f016e), U32(0x01710170), U32(0x01730172),
            U32(0x01750174), U32(0x01770176), U32(0x01790178), U32(0x017b017a),
            U32(0x017d017c), U32(0x017f017e), U32(0x01810180), U32(0x01830182),
            U32(0x01850184), U32(0x01870186), U32(0x01890188), U32(0x018b018a),
            U32(0x018d018c), U32(0x018f018e), U32(0x01910190), U32(0x01930192),
            U32(0x01950194), U32(0x01970196), U32(0x01990198), U32(0x019b019a),
            U32(0x019d019c), U32(0x019f019e), U32(0x01a101a0), U32(0x01a301a2),
            U32(0x01a501a4), U32(0x01a701a6), U32(0x01a901a8), U32(0x01ab01aa),
            U32(0x01ad01ac), U32(0x01af01ae), U32(0x01b101b0), U32(0x01b301b2),
            U32(0x01b501b4), U32(0x01b701b6), U32(0x01b901b8), U32(0x01bb01ba),
            U32(0x01bd01bc), U32(0x01bf01be), U32(0x01c101c0), U32(0x01c301c2),
            U32(0x01c501c4), U32(0x01c701c6), U32(0x01c901c8), U32(0x01cb01ca),
            U32(0x01cd01cc), U32(0x01cf01ce), U32(0x01d101d0), U32(0x01d301d2),
            U32(0x01d501d4), U32(0x01d701d6), U32(0x01d901d8), U32(0x01db01da),
            U32(0x01dd01dc), U32(0x01df01de), U32(0x01e101e0), U32(0x01e301e2),
            U32(0x01e501e4), U32(0x01e701e6), U32(0x01e901e8), U32(0x01eb01ea),
            U32(0x01ed01ec), U32(0x01ef01ee), U32(0x01f101f0), U32(0x01f301f2),
            U32(0x01f501f4), U32(0x01f701f6), U32(0x01f901f8), U32(0x01fb01fa),
            U32(0x01fd01fc), U32(0x01ff01fe), U32(0x02010200), U32(0x02030202),
            U32(0x02050204), U32(0x02070206), U32(0x02090208), U32(0x020b020a),
            U32(0x020d020c), U32(0x020f020e), U32(0x02110210), U32(0x02130212),
            U32(0x02150214), U32(0x02170216), U32(0x02190218), U32(0x021b021a),
            S4("SIMSUI.DLL", 0), S4("SIMSUI.DLL", 4), SB("SIMSUI.DLL", 8), SB("SIMSUI.DLL", 9), SB("SIMSUI.DLL", 10),
            S4("\?\?0CDIBPal@@QAE@XZ", 0), S4("\?\?0CDIBPal@@QAE@XZ", 4), S4("\?\?0CDIBPal@@QAE@XZ", 8), S4("\?\?0CDIBPal@@QAE@XZ", 12), SB("\?\?0CDIBPal@@QAE@XZ", 16), SB("\?\?0CDIBPal@@QAE@XZ", 17), SB("\?\?0CDIBPal@@QAE@XZ", 18),
            S4("\?\?0CDIBSection@@QAE@XZ", 0), S4("\?\?0CDIBSection@@QAE@XZ", 4), S4("\?\?0CDIBSection@@QAE@XZ", 8), S4("\?\?0CDIBSection@@QAE@XZ", 12), S4("\?\?0CDIBSection@@QAE@XZ", 16), SB("\?\?0CDIBSection@@QAE@XZ", 20), SB("\?\?0CDIBSection@@QAE@XZ", 21), SB("\?\?0CDIBSection@@QAE@XZ", 22),
            S4("\?\?0CNotify@@QAE@II@Z", 0), S4("\?\?0CNotify@@QAE@II@Z", 4), S4("\?\?0CNotify@@QAE@II@Z", 8), S4("\?\?0CNotify@@QAE@II@Z", 12), S4("\?\?0CNotify@@QAE@II@Z", 16), SB("\?\?0CNotify@@QAE@II@Z", 20),
            S4("\?\?0CSims3PieceButton@@QAE@XZ", 0), S4("\?\?0CSims3PieceButton@@QAE@XZ", 4), S4("\?\?0CSims3PieceButton@@QAE@XZ", 8), S4("\?\?0CSims3PieceButton@@QAE@XZ", 12), S4("\?\?0CSims3PieceButton@@QAE@XZ", 16), S4("\?\?0CSims3PieceButton@@QAE@XZ", 20), S4("\?\?0CSims3PieceButton@@QAE@XZ", 24), SB("\?\?0CSims3PieceButton@@QAE@XZ", 28),
            S4("\?\?0CSimsButton@@QAE@XZ", 0), S4("\?\?0CSimsButton@@QAE@XZ", 4), S4("\?\?0CSimsButton@@QAE@XZ", 8), S4("\?\?0CSimsButton@@QAE@XZ", 12), S4("\?\?0CSimsButton@@QAE@XZ", 16), SB("\?\?0CSimsButton@@QAE@XZ", 20), SB("\?\?0CSimsButton@@QAE@XZ", 21), SB("\?\?0CSimsButton@@QAE@XZ", 22),
            STR28("\?\?0CSimsCheckButton@@QAE@XZ"),
            S4("\?\?0CSimsCheckListBox@@QAE@XZ", 0), S4("\?\?0CSimsCheckListBox@@QAE@XZ", 4), S4("\?\?0CSimsCheckListBox@@QAE@XZ", 8), S4("\?\?0CSimsCheckListBox@@QAE@XZ", 12), S4("\?\?0CSimsCheckListBox@@QAE@XZ", 16), S4("\?\?0CSimsCheckListBox@@QAE@XZ", 20), S4("\?\?0CSimsCheckListBox@@QAE@XZ", 24), SB("\?\?0CSimsCheckListBox@@QAE@XZ", 28),
            S4("\?\?0CSimsComboBox@@QAE@XZ", 0), S4("\?\?0CSimsComboBox@@QAE@XZ", 4), S4("\?\?0CSimsComboBox@@QAE@XZ", 8), S4("\?\?0CSimsComboBox@@QAE@XZ", 12), S4("\?\?0CSimsComboBox@@QAE@XZ", 16), S4("\?\?0CSimsComboBox@@QAE@XZ", 20), SB("\?\?0CSimsComboBox@@QAE@XZ", 24),
            S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 0), S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 4), S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 8), S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 12), S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 16), S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 20), S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 24), S4("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 28), SB("\?\?0CSimsDialog@@QAE@HPAVCWnd@@@Z", 32),
            S4("\?\?0CSimsEdit@@QAE@XZ", 0), S4("\?\?0CSimsEdit@@QAE@XZ", 4), S4("\?\?0CSimsEdit@@QAE@XZ", 8), S4("\?\?0CSimsEdit@@QAE@XZ", 12), S4("\?\?0CSimsEdit@@QAE@XZ", 16), SB("\?\?0CSimsEdit@@QAE@XZ", 20),
            S4("\?\?0CSimsExitButton@@QAE@XZ", 0), S4("\?\?0CSimsExitButton@@QAE@XZ", 4), S4("\?\?0CSimsExitButton@@QAE@XZ", 8), S4("\?\?0CSimsExitButton@@QAE@XZ", 12), S4("\?\?0CSimsExitButton@@QAE@XZ", 16), S4("\?\?0CSimsExitButton@@QAE@XZ", 20), SB("\?\?0CSimsExitButton@@QAE@XZ", 24), SB("\?\?0CSimsExitButton@@QAE@XZ", 25), SB("\?\?0CSimsExitButton@@QAE@XZ", 26),
            S4("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 0), S4("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 4), S4("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 8), S4("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 12), S4("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 16), S4("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 20), S4("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 24), SB("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 28), SB("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 29), SB("\?\?0CSimsFont@@QAE@PAVCFont@@@Z", 30),
            S4("\?\?0CSimsFont@@QAE@XZ", 0), S4("\?\?0CSimsFont@@QAE@XZ", 4), S4("\?\?0CSimsFont@@QAE@XZ", 8), S4("\?\?0CSimsFont@@QAE@XZ", 12), S4("\?\?0CSimsFont@@QAE@XZ", 16), SB("\?\?0CSimsFont@@QAE@XZ", 20),
            S4("\?\?0CSimsGroupbox@@QAE@XZ", 0), S4("\?\?0CSimsGroupbox@@QAE@XZ", 4), S4("\?\?0CSimsGroupbox@@QAE@XZ", 8), S4("\?\?0CSimsGroupbox@@QAE@XZ", 12), S4("\?\?0CSimsGroupbox@@QAE@XZ", 16), S4("\?\?0CSimsGroupbox@@QAE@XZ", 20), SB("\?\?0CSimsGroupbox@@QAE@XZ", 24),
            S4("\?\?0CSimsHelpButton@@QAE@XZ", 0), S4("\?\?0CSimsHelpButton@@QAE@XZ", 4), S4("\?\?0CSimsHelpButton@@QAE@XZ", 8), S4("\?\?0CSimsHelpButton@@QAE@XZ", 12), S4("\?\?0CSimsHelpButton@@QAE@XZ", 16), S4("\?\?0CSimsHelpButton@@QAE@XZ", 20), SB("\?\?0CSimsHelpButton@@QAE@XZ", 24), SB("\?\?0CSimsHelpButton@@QAE@XZ", 25), SB("\?\?0CSimsHelpButton@@QAE@XZ", 26),
            S4("\?\?0CSimsHotJoy@@QAE@XZ", 0), S4("\?\?0CSimsHotJoy@@QAE@XZ", 4), S4("\?\?0CSimsHotJoy@@QAE@XZ", 8), S4("\?\?0CSimsHotJoy@@QAE@XZ", 12), S4("\?\?0CSimsHotJoy@@QAE@XZ", 16), SB("\?\?0CSimsHotJoy@@QAE@XZ", 20), SB("\?\?0CSimsHotJoy@@QAE@XZ", 21), SB("\?\?0CSimsHotJoy@@QAE@XZ", 22),
            S4("\?\?0CSimsHotKey@@QAE@XZ", 0), S4("\?\?0CSimsHotKey@@QAE@XZ", 4), S4("\?\?0CSimsHotKey@@QAE@XZ", 8), S4("\?\?0CSimsHotKey@@QAE@XZ", 12), S4("\?\?0CSimsHotKey@@QAE@XZ", 16), SB("\?\?0CSimsHotKey@@QAE@XZ", 20), SB("\?\?0CSimsHotKey@@QAE@XZ", 21), SB("\?\?0CSimsHotKey@@QAE@XZ", 22),
            STR24("\?\?0CSimsListBox@@QAE@XZ"),
            STR24("\?\?0CSimsPicture@@QAE@XZ"),
            STR28("\?\?0CSimsPictureView@@QAE@XZ"),
            STR28("\?\?0CSimsRadioButton@@QAE@XZ"),
            S4("\?\?0CSimsScrollBar@@QAE@XZ", 0), S4("\?\?0CSimsScrollBar@@QAE@XZ", 4), S4("\?\?0CSimsScrollBar@@QAE@XZ", 8), S4("\?\?0CSimsScrollBar@@QAE@XZ", 12), S4("\?\?0CSimsScrollBar@@QAE@XZ", 16), S4("\?\?0CSimsScrollBar@@QAE@XZ", 20), SB("\?\?0CSimsScrollBar@@QAE@XZ", 24), SB("\?\?0CSimsScrollBar@@QAE@XZ", 25),
            S4("\?\?0CSimsShadow@@QAE@XZ", 0), S4("\?\?0CSimsShadow@@QAE@XZ", 4), S4("\?\?0CSimsShadow@@QAE@XZ", 8), S4("\?\?0CSimsShadow@@QAE@XZ", 12), S4("\?\?0CSimsShadow@@QAE@XZ", 16), SB("\?\?0CSimsShadow@@QAE@XZ", 20), SB("\?\?0CSimsShadow@@QAE@XZ", 21), SB("\?\?0CSimsShadow@@QAE@XZ", 22),
            S4("\?\?0CSimsShadowButton@@QAE@XZ", 0), S4("\?\?0CSimsShadowButton@@QAE@XZ", 4), S4("\?\?0CSimsShadowButton@@QAE@XZ", 8), S4("\?\?0CSimsShadowButton@@QAE@XZ", 12), S4("\?\?0CSimsShadowButton@@QAE@XZ", 16), S4("\?\?0CSimsShadowButton@@QAE@XZ", 20), S4("\?\?0CSimsShadowButton@@QAE@XZ", 24), SB("\?\?0CSimsShadowButton@@QAE@XZ", 28),
            S4("\?\?0CSimsSlider@@QAE@XZ", 0), S4("\?\?0CSimsSlider@@QAE@XZ", 4), S4("\?\?0CSimsSlider@@QAE@XZ", 8), S4("\?\?0CSimsSlider@@QAE@XZ", 12), S4("\?\?0CSimsSlider@@QAE@XZ", 16), SB("\?\?0CSimsSlider@@QAE@XZ", 20), SB("\?\?0CSimsSlider@@QAE@XZ", 21), SB("\?\?0CSimsSlider@@QAE@XZ", 22),
            S4("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 0), S4("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 4), S4("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 8), S4("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 12), S4("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 16), S4("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 20), S4("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 24), SB("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 28), SB("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 29), SB("\?\?0CSimsSpinButtonCtrl@@QAE@XZ", 30),
            S4("\?\?0CSimsStatic@@QAE@XZ", 0), S4("\?\?0CSimsStatic@@QAE@XZ", 4), S4("\?\?0CSimsStatic@@QAE@XZ", 8), S4("\?\?0CSimsStatic@@QAE@XZ", 12), S4("\?\?0CSimsStatic@@QAE@XZ", 16), SB("\?\?0CSimsStatic@@QAE@XZ", 20), SB("\?\?0CSimsStatic@@QAE@XZ", 21), SB("\?\?0CSimsStatic@@QAE@XZ", 22),
            STR24("\?\?0CSimsTabCtrl@@QAE@XZ"),
            S4("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 0), S4("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 4), S4("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 8), S4("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 12), S4("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 16), S4("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 20), S4("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 24), SB("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 28), SB("\?\?0CSimsTabCtrlHelper@@QAE@XZ", 29),
            S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 0), S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 4), S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 8), S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 12), S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 16), S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 20), S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 24), S4("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 28), SB("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 32), SB("\?\?0CSimsTabPage@@QAE@HPAVCWnd@@@Z", 33),
            S4("\?\?0CSimsText@@QAE@XZ", 0), S4("\?\?0CSimsText@@QAE@XZ", 4), S4("\?\?0CSimsText@@QAE@XZ", 8), S4("\?\?0CSimsText@@QAE@XZ", 12), S4("\?\?0CSimsText@@QAE@XZ", 16), SB("\?\?0CSimsText@@QAE@XZ", 20),
            STR24("\?\?0CSimsTextArt@@QAE@XZ"),
            S4("\?\?1CDIBPal@@UAE@XZ", 0), S4("\?\?1CDIBPal@@UAE@XZ", 4), S4("\?\?1CDIBPal@@UAE@XZ", 8), S4("\?\?1CDIBPal@@UAE@XZ", 12), SB("\?\?1CDIBPal@@UAE@XZ", 16), SB("\?\?1CDIBPal@@UAE@XZ", 17), SB("\?\?1CDIBPal@@UAE@XZ", 18),
            S4("\?\?1CDIBSection@@UAE@XZ", 0), S4("\?\?1CDIBSection@@UAE@XZ", 4), S4("\?\?1CDIBSection@@UAE@XZ", 8), S4("\?\?1CDIBSection@@UAE@XZ", 12), S4("\?\?1CDIBSection@@UAE@XZ", 16), SB("\?\?1CDIBSection@@UAE@XZ", 20), SB("\?\?1CDIBSection@@UAE@XZ", 21), SB("\?\?1CDIBSection@@UAE@XZ", 22),
            S4("\?\?1CNotify@@UAE@XZ", 0), S4("\?\?1CNotify@@UAE@XZ", 4), S4("\?\?1CNotify@@UAE@XZ", 8), S4("\?\?1CNotify@@UAE@XZ", 12), SB("\?\?1CNotify@@UAE@XZ", 16), SB("\?\?1CNotify@@UAE@XZ", 17), SB("\?\?1CNotify@@UAE@XZ", 18),
            S4("\?\?1CSims3PieceButton@@UAE@XZ", 0), S4("\?\?1CSims3PieceButton@@UAE@XZ", 4), S4("\?\?1CSims3PieceButton@@UAE@XZ", 8), S4("\?\?1CSims3PieceButton@@UAE@XZ", 12), S4("\?\?1CSims3PieceButton@@UAE@XZ", 16), S4("\?\?1CSims3PieceButton@@UAE@XZ", 20), S4("\?\?1CSims3PieceButton@@UAE@XZ", 24), SB("\?\?1CSims3PieceButton@@UAE@XZ", 28),
            S4("\?\?1CSimsButton@@UAE@XZ", 0), S4("\?\?1CSimsButton@@UAE@XZ", 4), S4("\?\?1CSimsButton@@UAE@XZ", 8), S4("\?\?1CSimsButton@@UAE@XZ", 12), S4("\?\?1CSimsButton@@UAE@XZ", 16), SB("\?\?1CSimsButton@@UAE@XZ", 20), SB("\?\?1CSimsButton@@UAE@XZ", 21), SB("\?\?1CSimsButton@@UAE@XZ", 22),
            STR28("\?\?1CSimsCheckButton@@UAE@XZ"),
            S4("\?\?1CSimsCheckListBox@@UAE@XZ", 0), S4("\?\?1CSimsCheckListBox@@UAE@XZ", 4), S4("\?\?1CSimsCheckListBox@@UAE@XZ", 8), S4("\?\?1CSimsCheckListBox@@UAE@XZ", 12), S4("\?\?1CSimsCheckListBox@@UAE@XZ", 16), S4("\?\?1CSimsCheckListBox@@UAE@XZ", 20), S4("\?\?1CSimsCheckListBox@@UAE@XZ", 24), SB("\?\?1CSimsCheckListBox@@UAE@XZ", 28),
            S4("\?\?1CSimsComboBox@@UAE@XZ", 0), S4("\?\?1CSimsComboBox@@UAE@XZ", 4), S4("\?\?1CSimsComboBox@@UAE@XZ", 8), S4("\?\?1CSimsComboBox@@UAE@XZ", 12), S4("\?\?1CSimsComboBox@@UAE@XZ", 16), S4("\?\?1CSimsComboBox@@UAE@XZ", 20), SB("\?\?1CSimsComboBox@@UAE@XZ", 24),
            S4("\?\?1CSimsDialog@@UAE@XZ", 0), S4("\?\?1CSimsDialog@@UAE@XZ", 4), S4("\?\?1CSimsDialog@@UAE@XZ", 8), S4("\?\?1CSimsDialog@@UAE@XZ", 12), S4("\?\?1CSimsDialog@@UAE@XZ", 16), SB("\?\?1CSimsDialog@@UAE@XZ", 20), SB("\?\?1CSimsDialog@@UAE@XZ", 21), SB("\?\?1CSimsDialog@@UAE@XZ", 22),
            S4("\?\?1CSimsEdit@@UAE@XZ", 0), S4("\?\?1CSimsEdit@@UAE@XZ", 4), S4("\?\?1CSimsEdit@@UAE@XZ", 8), S4("\?\?1CSimsEdit@@UAE@XZ", 12), S4("\?\?1CSimsEdit@@UAE@XZ", 16), SB("\?\?1CSimsEdit@@UAE@XZ", 20),
            S4("\?\?1CSimsExitButton@@UAE@XZ", 0), S4("\?\?1CSimsExitButton@@UAE@XZ", 4), S4("\?\?1CSimsExitButton@@UAE@XZ", 8), S4("\?\?1CSimsExitButton@@UAE@XZ", 12), S4("\?\?1CSimsExitButton@@UAE@XZ", 16), S4("\?\?1CSimsExitButton@@UAE@XZ", 20), SB("\?\?1CSimsExitButton@@UAE@XZ", 24), SB("\?\?1CSimsExitButton@@UAE@XZ", 25), SB("\?\?1CSimsExitButton@@UAE@XZ", 26),
            S4("\?\?1CSimsFont@@UAE@XZ", 0), S4("\?\?1CSimsFont@@UAE@XZ", 4), S4("\?\?1CSimsFont@@UAE@XZ", 8), S4("\?\?1CSimsFont@@UAE@XZ", 12), S4("\?\?1CSimsFont@@UAE@XZ", 16), SB("\?\?1CSimsFont@@UAE@XZ", 20),
            S4("\?\?1CSimsGroupbox@@UAE@XZ", 0), S4("\?\?1CSimsGroupbox@@UAE@XZ", 4), S4("\?\?1CSimsGroupbox@@UAE@XZ", 8), S4("\?\?1CSimsGroupbox@@UAE@XZ", 12), S4("\?\?1CSimsGroupbox@@UAE@XZ", 16), S4("\?\?1CSimsGroupbox@@UAE@XZ", 20), SB("\?\?1CSimsGroupbox@@UAE@XZ", 24),
            S4("\?\?1CSimsHelpButton@@UAE@XZ", 0), S4("\?\?1CSimsHelpButton@@UAE@XZ", 4), S4("\?\?1CSimsHelpButton@@UAE@XZ", 8), S4("\?\?1CSimsHelpButton@@UAE@XZ", 12), S4("\?\?1CSimsHelpButton@@UAE@XZ", 16), S4("\?\?1CSimsHelpButton@@UAE@XZ", 20), SB("\?\?1CSimsHelpButton@@UAE@XZ", 24), SB("\?\?1CSimsHelpButton@@UAE@XZ", 25), SB("\?\?1CSimsHelpButton@@UAE@XZ", 26),
            S4("\?\?1CSimsHotJoy@@UAE@XZ", 0), S4("\?\?1CSimsHotJoy@@UAE@XZ", 4), S4("\?\?1CSimsHotJoy@@UAE@XZ", 8), S4("\?\?1CSimsHotJoy@@UAE@XZ", 12), S4("\?\?1CSimsHotJoy@@UAE@XZ", 16), SB("\?\?1CSimsHotJoy@@UAE@XZ", 20), SB("\?\?1CSimsHotJoy@@UAE@XZ", 21), SB("\?\?1CSimsHotJoy@@UAE@XZ", 22),
            S4("\?\?1CSimsHotKey@@UAE@XZ", 0), S4("\?\?1CSimsHotKey@@UAE@XZ", 4), S4("\?\?1CSimsHotKey@@UAE@XZ", 8), S4("\?\?1CSimsHotKey@@UAE@XZ", 12), S4("\?\?1CSimsHotKey@@UAE@XZ", 16), SB("\?\?1CSimsHotKey@@UAE@XZ", 20), SB("\?\?1CSimsHotKey@@UAE@XZ", 21), SB("\?\?1CSimsHotKey@@UAE@XZ", 22),
            STR24("\?\?1CSimsListBox@@UAE@XZ"),
            STR24("\?\?1CSimsPicture@@UAE@XZ"),
            STR28("\?\?1CSimsPictureView@@UAE@XZ"),
            STR28("\?\?1CSimsRadioButton@@UAE@XZ"),
            S4("\?\?1CSimsScrollBar@@UAE@XZ", 0), S4("\?\?1CSimsScrollBar@@UAE@XZ", 4), S4("\?\?1CSimsScrollBar@@UAE@XZ", 8), S4("\?\?1CSimsScrollBar@@UAE@XZ", 12), S4("\?\?1CSimsScrollBar@@UAE@XZ", 16), S4("\?\?1CSimsScrollBar@@UAE@XZ", 20), SB("\?\?1CSimsScrollBar@@UAE@XZ", 24), SB("\?\?1CSimsScrollBar@@UAE@XZ", 25),
            S4("\?\?1CSimsShadow@@UAE@XZ", 0), S4("\?\?1CSimsShadow@@UAE@XZ", 4), S4("\?\?1CSimsShadow@@UAE@XZ", 8), S4("\?\?1CSimsShadow@@UAE@XZ", 12), S4("\?\?1CSimsShadow@@UAE@XZ", 16), SB("\?\?1CSimsShadow@@UAE@XZ", 20), SB("\?\?1CSimsShadow@@UAE@XZ", 21), SB("\?\?1CSimsShadow@@UAE@XZ", 22),
            S4("\?\?1CSimsShadowButton@@UAE@XZ", 0), S4("\?\?1CSimsShadowButton@@UAE@XZ", 4), S4("\?\?1CSimsShadowButton@@UAE@XZ", 8), S4("\?\?1CSimsShadowButton@@UAE@XZ", 12), S4("\?\?1CSimsShadowButton@@UAE@XZ", 16), S4("\?\?1CSimsShadowButton@@UAE@XZ", 20), S4("\?\?1CSimsShadowButton@@UAE@XZ", 24), SB("\?\?1CSimsShadowButton@@UAE@XZ", 28),
            S4("\?\?1CSimsSlider@@UAE@XZ", 0), S4("\?\?1CSimsSlider@@UAE@XZ", 4), S4("\?\?1CSimsSlider@@UAE@XZ", 8), S4("\?\?1CSimsSlider@@UAE@XZ", 12), S4("\?\?1CSimsSlider@@UAE@XZ", 16), SB("\?\?1CSimsSlider@@UAE@XZ", 20), SB("\?\?1CSimsSlider@@UAE@XZ", 21), SB("\?\?1CSimsSlider@@UAE@XZ", 22),
            S4("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 0), S4("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 4), S4("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 8), S4("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 12), S4("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 16), S4("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 20), S4("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 24), SB("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 28), SB("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 29), SB("\?\?1CSimsSpinButtonCtrl@@UAE@XZ", 30),
            S4("\?\?1CSimsStatic@@UAE@XZ", 0), S4("\?\?1CSimsStatic@@UAE@XZ", 4), S4("\?\?1CSimsStatic@@UAE@XZ", 8), S4("\?\?1CSimsStatic@@UAE@XZ", 12), S4("\?\?1CSimsStatic@@UAE@XZ", 16), SB("\?\?1CSimsStatic@@UAE@XZ", 20), SB("\?\?1CSimsStatic@@UAE@XZ", 21), SB("\?\?1CSimsStatic@@UAE@XZ", 22),
            STR24("\?\?1CSimsTabCtrl@@UAE@XZ"),
            S4("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 0), S4("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 4), S4("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 8), S4("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 12), S4("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 16), S4("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 20), S4("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 24), SB("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 28), SB("\?\?1CSimsTabCtrlHelper@@UAE@XZ", 29),
            STR24("\?\?1CSimsTabPage@@UAE@XZ"),
            S4("\?\?1CSimsText@@UAE@XZ", 0), S4("\?\?1CSimsText@@UAE@XZ", 4), S4("\?\?1CSimsText@@UAE@XZ", 8), S4("\?\?1CSimsText@@UAE@XZ", 12), S4("\?\?1CSimsText@@UAE@XZ", 16), SB("\?\?1CSimsText@@UAE@XZ", 20),
            STR24("\?\?1CSimsTextArt@@UAE@XZ"),
            S4("\?\?2CDIBPal@@SGPAXI@Z", 0), S4("\?\?2CDIBPal@@SGPAXI@Z", 4), S4("\?\?2CDIBPal@@SGPAXI@Z", 8), S4("\?\?2CDIBPal@@SGPAXI@Z", 12), S4("\?\?2CDIBPal@@SGPAXI@Z", 16), SB("\?\?2CDIBPal@@SGPAXI@Z", 20),
            S4("\?\?2CDIBSection@@SGPAXI@Z", 0), S4("\?\?2CDIBSection@@SGPAXI@Z", 4), S4("\?\?2CDIBSection@@SGPAXI@Z", 8), S4("\?\?2CDIBSection@@SGPAXI@Z", 12), S4("\?\?2CDIBSection@@SGPAXI@Z", 16), S4("\?\?2CDIBSection@@SGPAXI@Z", 20), SB("\?\?2CDIBSection@@SGPAXI@Z", 24),
            S4("\?\?2CNotify@@SGPAXI@Z", 0), S4("\?\?2CNotify@@SGPAXI@Z", 4), S4("\?\?2CNotify@@SGPAXI@Z", 8), S4("\?\?2CNotify@@SGPAXI@Z", 12), S4("\?\?2CNotify@@SGPAXI@Z", 16), SB("\?\?2CNotify@@SGPAXI@Z", 20),
            S4("\?\?2CSimsButton@@SGPAXI@Z", 0), S4("\?\?2CSimsButton@@SGPAXI@Z", 4), S4("\?\?2CSimsButton@@SGPAXI@Z", 8), S4("\?\?2CSimsButton@@SGPAXI@Z", 12), S4("\?\?2CSimsButton@@SGPAXI@Z", 16), S4("\?\?2CSimsButton@@SGPAXI@Z", 20), SB("\?\?2CSimsButton@@SGPAXI@Z", 24),
            S4("\?\?2CSimsCheckButton@@SGPAXI@Z", 0), S4("\?\?2CSimsCheckButton@@SGPAXI@Z", 4), S4("\?\?2CSimsCheckButton@@SGPAXI@Z", 8), S4("\?\?2CSimsCheckButton@@SGPAXI@Z", 12), S4("\?\?2CSimsCheckButton@@SGPAXI@Z", 16), S4("\?\?2CSimsCheckButton@@SGPAXI@Z", 20), S4("\?\?2CSimsCheckButton@@SGPAXI@Z", 24), SB("\?\?2CSimsCheckButton@@SGPAXI@Z", 28), SB("\?\?2CSimsCheckButton@@SGPAXI@Z", 29),
            S4("\?\?2CSimsCheckListBox@@SGPAXI@Z", 0), S4("\?\?2CSimsCheckListBox@@SGPAXI@Z", 4), S4("\?\?2CSimsCheckListBox@@SGPAXI@Z", 8), S4("\?\?2CSimsCheckListBox@@SGPAXI@Z", 12), S4("\?\?2CSimsCheckListBox@@SGPAXI@Z", 16), S4("\?\?2CSimsCheckListBox@@SGPAXI@Z", 20), S4("\?\?2CSimsCheckListBox@@SGPAXI@Z", 24), SB("\?\?2CSimsCheckListBox@@SGPAXI@Z", 28), SB("\?\?2CSimsCheckListBox@@SGPAXI@Z", 29), SB("\?\?2CSimsCheckListBox@@SGPAXI@Z", 30),
            S4("\?\?2CSimsComboBox@@SGPAXI@Z", 0), S4("\?\?2CSimsComboBox@@SGPAXI@Z", 4), S4("\?\?2CSimsComboBox@@SGPAXI@Z", 8), S4("\?\?2CSimsComboBox@@SGPAXI@Z", 12), S4("\?\?2CSimsComboBox@@SGPAXI@Z", 16), S4("\?\?2CSimsComboBox@@SGPAXI@Z", 20), SB("\?\?2CSimsComboBox@@SGPAXI@Z", 24), SB("\?\?2CSimsComboBox@@SGPAXI@Z", 25), SB("\?\?2CSimsComboBox@@SGPAXI@Z", 26),
            S4("\?\?2CSimsDialog@@SGPAXI@Z", 0), S4("\?\?2CSimsDialog@@SGPAXI@Z", 4), S4("\?\?2CSimsDialog@@SGPAXI@Z", 8), S4("\?\?2CSimsDialog@@SGPAXI@Z", 12), S4("\?\?2CSimsDialog@@SGPAXI@Z", 16), S4("\?\?2CSimsDialog@@SGPAXI@Z", 20), SB("\?\?2CSimsDialog@@SGPAXI@Z", 24),
            S4("\?\?2CSimsEdit@@SGPAXI@Z", 0), S4("\?\?2CSimsEdit@@SGPAXI@Z", 4), S4("\?\?2CSimsEdit@@SGPAXI@Z", 8), S4("\?\?2CSimsEdit@@SGPAXI@Z", 12), S4("\?\?2CSimsEdit@@SGPAXI@Z", 16), SB("\?\?2CSimsEdit@@SGPAXI@Z", 20), SB("\?\?2CSimsEdit@@SGPAXI@Z", 21), SB("\?\?2CSimsEdit@@SGPAXI@Z", 22),
            S4("\?\?2CSimsFont@@SGPAXI@Z", 0), S4("\?\?2CSimsFont@@SGPAXI@Z", 4), S4("\?\?2CSimsFont@@SGPAXI@Z", 8), S4("\?\?2CSimsFont@@SGPAXI@Z", 12), S4("\?\?2CSimsFont@@SGPAXI@Z", 16), SB("\?\?2CSimsFont@@SGPAXI@Z", 20), SB("\?\?2CSimsFont@@SGPAXI@Z", 21), SB("\?\?2CSimsFont@@SGPAXI@Z", 22),
            S4("\?\?2CSimsHotJoy@@SGPAXI@Z", 0), S4("\?\?2CSimsHotJoy@@SGPAXI@Z", 4), S4("\?\?2CSimsHotJoy@@SGPAXI@Z", 8), S4("\?\?2CSimsHotJoy@@SGPAXI@Z", 12), S4("\?\?2CSimsHotJoy@@SGPAXI@Z", 16), S4("\?\?2CSimsHotJoy@@SGPAXI@Z", 20), SB("\?\?2CSimsHotJoy@@SGPAXI@Z", 24),
            S4("\?\?2CSimsHotKey@@SGPAXI@Z", 0), S4("\?\?2CSimsHotKey@@SGPAXI@Z", 4), S4("\?\?2CSimsHotKey@@SGPAXI@Z", 8), S4("\?\?2CSimsHotKey@@SGPAXI@Z", 12), S4("\?\?2CSimsHotKey@@SGPAXI@Z", 16), S4("\?\?2CSimsHotKey@@SGPAXI@Z", 20), SB("\?\?2CSimsHotKey@@SGPAXI@Z", 24),
            S4("\?\?2CSimsListBox@@SGPAXI@Z", 0), S4("\?\?2CSimsListBox@@SGPAXI@Z", 4), S4("\?\?2CSimsListBox@@SGPAXI@Z", 8), S4("\?\?2CSimsListBox@@SGPAXI@Z", 12), S4("\?\?2CSimsListBox@@SGPAXI@Z", 16), S4("\?\?2CSimsListBox@@SGPAXI@Z", 20), SB("\?\?2CSimsListBox@@SGPAXI@Z", 24), SB("\?\?2CSimsListBox@@SGPAXI@Z", 25),
            S4("\?\?2CSimsPicture@@SGPAXI@Z", 0), S4("\?\?2CSimsPicture@@SGPAXI@Z", 4), S4("\?\?2CSimsPicture@@SGPAXI@Z", 8), S4("\?\?2CSimsPicture@@SGPAXI@Z", 12), S4("\?\?2CSimsPicture@@SGPAXI@Z", 16), S4("\?\?2CSimsPicture@@SGPAXI@Z", 20), SB("\?\?2CSimsPicture@@SGPAXI@Z", 24), SB("\?\?2CSimsPicture@@SGPAXI@Z", 25),
            S4("\?\?2CSimsPictureView@@SGPAXI@Z", 0), S4("\?\?2CSimsPictureView@@SGPAXI@Z", 4), S4("\?\?2CSimsPictureView@@SGPAXI@Z", 8), S4("\?\?2CSimsPictureView@@SGPAXI@Z", 12), S4("\?\?2CSimsPictureView@@SGPAXI@Z", 16), S4("\?\?2CSimsPictureView@@SGPAXI@Z", 20), S4("\?\?2CSimsPictureView@@SGPAXI@Z", 24), SB("\?\?2CSimsPictureView@@SGPAXI@Z", 28), SB("\?\?2CSimsPictureView@@SGPAXI@Z", 29),
            S4("\?\?2CSimsRadioButton@@SGPAXI@Z", 0), S4("\?\?2CSimsRadioButton@@SGPAXI@Z", 4), S4("\?\?2CSimsRadioButton@@SGPAXI@Z", 8), S4("\?\?2CSimsRadioButton@@SGPAXI@Z", 12), S4("\?\?2CSimsRadioButton@@SGPAXI@Z", 16), S4("\?\?2CSimsRadioButton@@SGPAXI@Z", 20), S4("\?\?2CSimsRadioButton@@SGPAXI@Z", 24), SB("\?\?2CSimsRadioButton@@SGPAXI@Z", 28), SB("\?\?2CSimsRadioButton@@SGPAXI@Z", 29),
            S4("\?\?2CSimsShadow@@SGPAXI@Z", 0), S4("\?\?2CSimsShadow@@SGPAXI@Z", 4), S4("\?\?2CSimsShadow@@SGPAXI@Z", 8), S4("\?\?2CSimsShadow@@SGPAXI@Z", 12), S4("\?\?2CSimsShadow@@SGPAXI@Z", 16), S4("\?\?2CSimsShadow@@SGPAXI@Z", 20), SB("\?\?2CSimsShadow@@SGPAXI@Z", 24),
            S4("\?\?2CSimsTabCtrl@@SGPAXI@Z", 0), S4("\?\?2CSimsTabCtrl@@SGPAXI@Z", 4), S4("\?\?2CSimsTabCtrl@@SGPAXI@Z", 8), S4("\?\?2CSimsTabCtrl@@SGPAXI@Z", 12), S4("\?\?2CSimsTabCtrl@@SGPAXI@Z", 16), S4("\?\?2CSimsTabCtrl@@SGPAXI@Z", 20), SB("\?\?2CSimsTabCtrl@@SGPAXI@Z", 24), SB("\?\?2CSimsTabCtrl@@SGPAXI@Z", 25),
            STR32("\?\?2CSimsTabCtrlHelper@@SGPAXI@Z"),
            S4("\?\?2CSimsText@@SGPAXI@Z", 0), S4("\?\?2CSimsText@@SGPAXI@Z", 4), S4("\?\?2CSimsText@@SGPAXI@Z", 8), S4("\?\?2CSimsText@@SGPAXI@Z", 12), S4("\?\?2CSimsText@@SGPAXI@Z", 16), SB("\?\?2CSimsText@@SGPAXI@Z", 20), SB("\?\?2CSimsText@@SGPAXI@Z", 21), SB("\?\?2CSimsText@@SGPAXI@Z", 22),
            S4("\?\?2CSimsTextArt@@SGPAXI@Z", 0), S4("\?\?2CSimsTextArt@@SGPAXI@Z", 4), S4("\?\?2CSimsTextArt@@SGPAXI@Z", 8), S4("\?\?2CSimsTextArt@@SGPAXI@Z", 12), S4("\?\?2CSimsTextArt@@SGPAXI@Z", 16), S4("\?\?2CSimsTextArt@@SGPAXI@Z", 20), SB("\?\?2CSimsTextArt@@SGPAXI@Z", 24), SB("\?\?2CSimsTextArt@@SGPAXI@Z", 25),
            S4("\?\?3CDIBPal@@SGXPAX@Z", 0), S4("\?\?3CDIBPal@@SGXPAX@Z", 4), S4("\?\?3CDIBPal@@SGXPAX@Z", 8), S4("\?\?3CDIBPal@@SGXPAX@Z", 12), S4("\?\?3CDIBPal@@SGXPAX@Z", 16), SB("\?\?3CDIBPal@@SGXPAX@Z", 20),
            S4("\?\?3CDIBSection@@SGXPAX@Z", 0), S4("\?\?3CDIBSection@@SGXPAX@Z", 4), S4("\?\?3CDIBSection@@SGXPAX@Z", 8), S4("\?\?3CDIBSection@@SGXPAX@Z", 12), S4("\?\?3CDIBSection@@SGXPAX@Z", 16), S4("\?\?3CDIBSection@@SGXPAX@Z", 20), SB("\?\?3CDIBSection@@SGXPAX@Z", 24),
            S4("\?\?3CNotify@@SGXPAX@Z", 0), S4("\?\?3CNotify@@SGXPAX@Z", 4), S4("\?\?3CNotify@@SGXPAX@Z", 8), S4("\?\?3CNotify@@SGXPAX@Z", 12), S4("\?\?3CNotify@@SGXPAX@Z", 16), SB("\?\?3CNotify@@SGXPAX@Z", 20),
            S4("\?\?3CSimsButton@@SGXPAX@Z", 0), S4("\?\?3CSimsButton@@SGXPAX@Z", 4), S4("\?\?3CSimsButton@@SGXPAX@Z", 8), S4("\?\?3CSimsButton@@SGXPAX@Z", 12), S4("\?\?3CSimsButton@@SGXPAX@Z", 16), S4("\?\?3CSimsButton@@SGXPAX@Z", 20), SB("\?\?3CSimsButton@@SGXPAX@Z", 24),
            S4("\?\?3CSimsCheckButton@@SGXPAX@Z", 0), S4("\?\?3CSimsCheckButton@@SGXPAX@Z", 4), S4("\?\?3CSimsCheckButton@@SGXPAX@Z", 8), S4("\?\?3CSimsCheckButton@@SGXPAX@Z", 12), S4("\?\?3CSimsCheckButton@@SGXPAX@Z", 16), S4("\?\?3CSimsCheckButton@@SGXPAX@Z", 20), S4("\?\?3CSimsCheckButton@@SGXPAX@Z", 24), SB("\?\?3CSimsCheckButton@@SGXPAX@Z", 28), SB("\?\?3CSimsCheckButton@@SGXPAX@Z", 29),
            S4("\?\?3CSimsCheckListBox@@SGXPAX@Z", 0), S4("\?\?3CSimsCheckListBox@@SGXPAX@Z", 4), S4("\?\?3CSimsCheckListBox@@SGXPAX@Z", 8), S4("\?\?3CSimsCheckListBox@@SGXPAX@Z", 12), S4("\?\?3CSimsCheckListBox@@SGXPAX@Z", 16), S4("\?\?3CSimsCheckListBox@@SGXPAX@Z", 20), S4("\?\?3CSimsCheckListBox@@SGXPAX@Z", 24), SB("\?\?3CSimsCheckListBox@@SGXPAX@Z", 28), SB("\?\?3CSimsCheckListBox@@SGXPAX@Z", 29), SB("\?\?3CSimsCheckListBox@@SGXPAX@Z", 30),
            S4("\?\?3CSimsComboBox@@SGXPAX@Z", 0), S4("\?\?3CSimsComboBox@@SGXPAX@Z", 4), S4("\?\?3CSimsComboBox@@SGXPAX@Z", 8), S4("\?\?3CSimsComboBox@@SGXPAX@Z", 12), S4("\?\?3CSimsComboBox@@SGXPAX@Z", 16), S4("\?\?3CSimsComboBox@@SGXPAX@Z", 20), SB("\?\?3CSimsComboBox@@SGXPAX@Z", 24), SB("\?\?3CSimsComboBox@@SGXPAX@Z", 25), SB("\?\?3CSimsComboBox@@SGXPAX@Z", 26),
            S4("\?\?3CSimsDialog@@SGXPAX@Z", 0), S4("\?\?3CSimsDialog@@SGXPAX@Z", 4), S4("\?\?3CSimsDialog@@SGXPAX@Z", 8), S4("\?\?3CSimsDialog@@SGXPAX@Z", 12), S4("\?\?3CSimsDialog@@SGXPAX@Z", 16), S4("\?\?3CSimsDialog@@SGXPAX@Z", 20), SB("\?\?3CSimsDialog@@SGXPAX@Z", 24),
            S4("\?\?3CSimsEdit@@SGXPAX@Z", 0), S4("\?\?3CSimsEdit@@SGXPAX@Z", 4), S4("\?\?3CSimsEdit@@SGXPAX@Z", 8), S4("\?\?3CSimsEdit@@SGXPAX@Z", 12), S4("\?\?3CSimsEdit@@SGXPAX@Z", 16), SB("\?\?3CSimsEdit@@SGXPAX@Z", 20), SB("\?\?3CSimsEdit@@SGXPAX@Z", 21), SB("\?\?3CSimsEdit@@SGXPAX@Z", 22),
            S4("\?\?3CSimsFont@@SGXPAX@Z", 0), S4("\?\?3CSimsFont@@SGXPAX@Z", 4), S4("\?\?3CSimsFont@@SGXPAX@Z", 8), S4("\?\?3CSimsFont@@SGXPAX@Z", 12), S4("\?\?3CSimsFont@@SGXPAX@Z", 16), SB("\?\?3CSimsFont@@SGXPAX@Z", 20), SB("\?\?3CSimsFont@@SGXPAX@Z", 21), SB("\?\?3CSimsFont@@SGXPAX@Z", 22),
            S4("\?\?3CSimsHotJoy@@SGXPAX@Z", 0), S4("\?\?3CSimsHotJoy@@SGXPAX@Z", 4), S4("\?\?3CSimsHotJoy@@SGXPAX@Z", 8), S4("\?\?3CSimsHotJoy@@SGXPAX@Z", 12), S4("\?\?3CSimsHotJoy@@SGXPAX@Z", 16), S4("\?\?3CSimsHotJoy@@SGXPAX@Z", 20), SB("\?\?3CSimsHotJoy@@SGXPAX@Z", 24),
            S4("\?\?3CSimsHotKey@@SGXPAX@Z", 0), S4("\?\?3CSimsHotKey@@SGXPAX@Z", 4), S4("\?\?3CSimsHotKey@@SGXPAX@Z", 8), S4("\?\?3CSimsHotKey@@SGXPAX@Z", 12), S4("\?\?3CSimsHotKey@@SGXPAX@Z", 16), S4("\?\?3CSimsHotKey@@SGXPAX@Z", 20), SB("\?\?3CSimsHotKey@@SGXPAX@Z", 24),
            S4("\?\?3CSimsListBox@@SGXPAX@Z", 0), S4("\?\?3CSimsListBox@@SGXPAX@Z", 4), S4("\?\?3CSimsListBox@@SGXPAX@Z", 8), S4("\?\?3CSimsListBox@@SGXPAX@Z", 12), S4("\?\?3CSimsListBox@@SGXPAX@Z", 16), S4("\?\?3CSimsListBox@@SGXPAX@Z", 20), SB("\?\?3CSimsListBox@@SGXPAX@Z", 24), SB("\?\?3CSimsListBox@@SGXPAX@Z", 25),
            S4("\?\?3CSimsPicture@@SGXPAX@Z", 0), S4("\?\?3CSimsPicture@@SGXPAX@Z", 4), S4("\?\?3CSimsPicture@@SGXPAX@Z", 8), S4("\?\?3CSimsPicture@@SGXPAX@Z", 12), S4("\?\?3CSimsPicture@@SGXPAX@Z", 16), S4("\?\?3CSimsPicture@@SGXPAX@Z", 20), SB("\?\?3CSimsPicture@@SGXPAX@Z", 24), SB("\?\?3CSimsPicture@@SGXPAX@Z", 25),
            S4("\?\?3CSimsPictureView@@SGXPAX@Z", 0), S4("\?\?3CSimsPictureView@@SGXPAX@Z", 4), S4("\?\?3CSimsPictureView@@SGXPAX@Z", 8), S4("\?\?3CSimsPictureView@@SGXPAX@Z", 12), S4("\?\?3CSimsPictureView@@SGXPAX@Z", 16), S4("\?\?3CSimsPictureView@@SGXPAX@Z", 20), S4("\?\?3CSimsPictureView@@SGXPAX@Z", 24), SB("\?\?3CSimsPictureView@@SGXPAX@Z", 28), SB("\?\?3CSimsPictureView@@SGXPAX@Z", 29),
            S4("\?\?3CSimsRadioButton@@SGXPAX@Z", 0), S4("\?\?3CSimsRadioButton@@SGXPAX@Z", 4), S4("\?\?3CSimsRadioButton@@SGXPAX@Z", 8), S4("\?\?3CSimsRadioButton@@SGXPAX@Z", 12), S4("\?\?3CSimsRadioButton@@SGXPAX@Z", 16), S4("\?\?3CSimsRadioButton@@SGXPAX@Z", 20), S4("\?\?3CSimsRadioButton@@SGXPAX@Z", 24), SB("\?\?3CSimsRadioButton@@SGXPAX@Z", 28), SB("\?\?3CSimsRadioButton@@SGXPAX@Z", 29),
            S4("\?\?3CSimsShadow@@SGXPAX@Z", 0), S4("\?\?3CSimsShadow@@SGXPAX@Z", 4), S4("\?\?3CSimsShadow@@SGXPAX@Z", 8), S4("\?\?3CSimsShadow@@SGXPAX@Z", 12), S4("\?\?3CSimsShadow@@SGXPAX@Z", 16), S4("\?\?3CSimsShadow@@SGXPAX@Z", 20), SB("\?\?3CSimsShadow@@SGXPAX@Z", 24),
            S4("\?\?3CSimsTabCtrl@@SGXPAX@Z", 0), S4("\?\?3CSimsTabCtrl@@SGXPAX@Z", 4), S4("\?\?3CSimsTabCtrl@@SGXPAX@Z", 8), S4("\?\?3CSimsTabCtrl@@SGXPAX@Z", 12), S4("\?\?3CSimsTabCtrl@@SGXPAX@Z", 16), S4("\?\?3CSimsTabCtrl@@SGXPAX@Z", 20), SB("\?\?3CSimsTabCtrl@@SGXPAX@Z", 24), SB("\?\?3CSimsTabCtrl@@SGXPAX@Z", 25),
            STR32("\?\?3CSimsTabCtrlHelper@@SGXPAX@Z"),
            S4("\?\?3CSimsText@@SGXPAX@Z", 0), S4("\?\?3CSimsText@@SGXPAX@Z", 4), S4("\?\?3CSimsText@@SGXPAX@Z", 8), S4("\?\?3CSimsText@@SGXPAX@Z", 12), S4("\?\?3CSimsText@@SGXPAX@Z", 16), SB("\?\?3CSimsText@@SGXPAX@Z", 20), SB("\?\?3CSimsText@@SGXPAX@Z", 21), SB("\?\?3CSimsText@@SGXPAX@Z", 22),
            S4("\?\?3CSimsTextArt@@SGXPAX@Z", 0), S4("\?\?3CSimsTextArt@@SGXPAX@Z", 4), S4("\?\?3CSimsTextArt@@SGXPAX@Z", 8), S4("\?\?3CSimsTextArt@@SGXPAX@Z", 12), S4("\?\?3CSimsTextArt@@SGXPAX@Z", 16), S4("\?\?3CSimsTextArt@@SGXPAX@Z", 20), SB("\?\?3CSimsTextArt@@SGXPAX@Z", 24), SB("\?\?3CSimsTextArt@@SGXPAX@Z", 25),
            STR36("\?\?BCDIBSection@@QAEPAUHBITMAP__@@XZ"),
            S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 0), S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 4), S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 8), S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 12), S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 16), S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 20), S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 24), S4("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 28), SB("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 32), SB("\?\?BCDIBSection@@QAEPAVCBitmap@@XZ", 33),
            S4("\?\?_7CDIBPal@@6B@", 0), S4("\?\?_7CDIBPal@@6B@", 4), S4("\?\?_7CDIBPal@@6B@", 8), S4("\?\?_7CDIBPal@@6B@", 12), SB("\?\?_7CDIBPal@@6B@", 16),
            S4("\?\?_7CDIBSection@@6B@", 0), S4("\?\?_7CDIBSection@@6B@", 4), S4("\?\?_7CDIBSection@@6B@", 8), S4("\?\?_7CDIBSection@@6B@", 12), S4("\?\?_7CDIBSection@@6B@", 16), SB("\?\?_7CDIBSection@@6B@", 20),
            S4("\?\?_7CNotify@@6B@", 0), S4("\?\?_7CNotify@@6B@", 4), S4("\?\?_7CNotify@@6B@", 8), S4("\?\?_7CNotify@@6B@", 12), SB("\?\?_7CNotify@@6B@", 16),
            S4("\?\?_7CSims3PieceButton@@6B@", 0), S4("\?\?_7CSims3PieceButton@@6B@", 4), S4("\?\?_7CSims3PieceButton@@6B@", 8), S4("\?\?_7CSims3PieceButton@@6B@", 12), S4("\?\?_7CSims3PieceButton@@6B@", 16), S4("\?\?_7CSims3PieceButton@@6B@", 20), SB("\?\?_7CSims3PieceButton@@6B@", 24), SB("\?\?_7CSims3PieceButton@@6B@", 25), SB("\?\?_7CSims3PieceButton@@6B@", 26),
            S4("\?\?_7CSimsButton@@6B@", 0), S4("\?\?_7CSimsButton@@6B@", 4), S4("\?\?_7CSimsButton@@6B@", 8), S4("\?\?_7CSimsButton@@6B@", 12), S4("\?\?_7CSimsButton@@6B@", 16), SB("\?\?_7CSimsButton@@6B@", 20),
            S4("\?\?_7CSimsCheckButton@@6B@", 0), S4("\?\?_7CSimsCheckButton@@6B@", 4), S4("\?\?_7CSimsCheckButton@@6B@", 8), S4("\?\?_7CSimsCheckButton@@6B@", 12), S4("\?\?_7CSimsCheckButton@@6B@", 16), S4("\?\?_7CSimsCheckButton@@6B@", 20), SB("\?\?_7CSimsCheckButton@@6B@", 24), SB("\?\?_7CSimsCheckButton@@6B@", 25),
            S4("\?\?_7CSimsCheckListBox@@6B@", 0), S4("\?\?_7CSimsCheckListBox@@6B@", 4), S4("\?\?_7CSimsCheckListBox@@6B@", 8), S4("\?\?_7CSimsCheckListBox@@6B@", 12), S4("\?\?_7CSimsCheckListBox@@6B@", 16), S4("\?\?_7CSimsCheckListBox@@6B@", 20), SB("\?\?_7CSimsCheckListBox@@6B@", 24), SB("\?\?_7CSimsCheckListBox@@6B@", 25), SB("\?\?_7CSimsCheckListBox@@6B@", 26),
            S4("\?\?_7CSimsComboBox@@6B@", 0), S4("\?\?_7CSimsComboBox@@6B@", 4), S4("\?\?_7CSimsComboBox@@6B@", 8), S4("\?\?_7CSimsComboBox@@6B@", 12), S4("\?\?_7CSimsComboBox@@6B@", 16), SB("\?\?_7CSimsComboBox@@6B@", 20), SB("\?\?_7CSimsComboBox@@6B@", 21), SB("\?\?_7CSimsComboBox@@6B@", 22),
            S4("\?\?_7CSimsDialog@@6B@", 0), S4("\?\?_7CSimsDialog@@6B@", 4), S4("\?\?_7CSimsDialog@@6B@", 8), S4("\?\?_7CSimsDialog@@6B@", 12), S4("\?\?_7CSimsDialog@@6B@", 16), SB("\?\?_7CSimsDialog@@6B@", 20),
            S4("\?\?_7CSimsEdit@@6B@", 0), S4("\?\?_7CSimsEdit@@6B@", 4), S4("\?\?_7CSimsEdit@@6B@", 8), S4("\?\?_7CSimsEdit@@6B@", 12), SB("\?\?_7CSimsEdit@@6B@", 16), SB("\?\?_7CSimsEdit@@6B@", 17), SB("\?\?_7CSimsEdit@@6B@", 18),
            S4("\?\?_7CSimsExitButton@@6B@", 0), S4("\?\?_7CSimsExitButton@@6B@", 4), S4("\?\?_7CSimsExitButton@@6B@", 8), S4("\?\?_7CSimsExitButton@@6B@", 12), S4("\?\?_7CSimsExitButton@@6B@", 16), S4("\?\?_7CSimsExitButton@@6B@", 20), SB("\?\?_7CSimsExitButton@@6B@", 24),
            S4("\?\?_7CSimsFont@@6B@", 0), S4("\?\?_7CSimsFont@@6B@", 4), S4("\?\?_7CSimsFont@@6B@", 8), S4("\?\?_7CSimsFont@@6B@", 12), SB("\?\?_7CSimsFont@@6B@", 16), SB("\?\?_7CSimsFont@@6B@", 17), SB("\?\?_7CSimsFont@@6B@", 18),
            S4("\?\?_7CSimsGroupbox@@6B@", 0), S4("\?\?_7CSimsGroupbox@@6B@", 4), S4("\?\?_7CSimsGroupbox@@6B@", 8), S4("\?\?_7CSimsGroupbox@@6B@", 12), S4("\?\?_7CSimsGroupbox@@6B@", 16), SB("\?\?_7CSimsGroupbox@@6B@", 20), SB("\?\?_7CSimsGroupbox@@6B@", 21), SB("\?\?_7CSimsGroupbox@@6B@", 22),
            S4("\?\?_7CSimsHelpButton@@6B@", 0), S4("\?\?_7CSimsHelpButton@@6B@", 4), S4("\?\?_7CSimsHelpButton@@6B@", 8), S4("\?\?_7CSimsHelpButton@@6B@", 12), S4("\?\?_7CSimsHelpButton@@6B@", 16), S4("\?\?_7CSimsHelpButton@@6B@", 20), SB("\?\?_7CSimsHelpButton@@6B@", 24),
            S4("\?\?_7CSimsHotJoy@@6B@", 0), S4("\?\?_7CSimsHotJoy@@6B@", 4), S4("\?\?_7CSimsHotJoy@@6B@", 8), S4("\?\?_7CSimsHotJoy@@6B@", 12), S4("\?\?_7CSimsHotJoy@@6B@", 16), SB("\?\?_7CSimsHotJoy@@6B@", 20),
            S4("\?\?_7CSimsHotKey@@6B@", 0), S4("\?\?_7CSimsHotKey@@6B@", 4), S4("\?\?_7CSimsHotKey@@6B@", 8), S4("\?\?_7CSimsHotKey@@6B@", 12), S4("\?\?_7CSimsHotKey@@6B@", 16), SB("\?\?_7CSimsHotKey@@6B@", 20),
            S4("\?\?_7CSimsListBox@@6B@", 0), S4("\?\?_7CSimsListBox@@6B@", 4), S4("\?\?_7CSimsListBox@@6B@", 8), S4("\?\?_7CSimsListBox@@6B@", 12), S4("\?\?_7CSimsListBox@@6B@", 16), SB("\?\?_7CSimsListBox@@6B@", 20), SB("\?\?_7CSimsListBox@@6B@", 21),
            S4("\?\?_7CSimsPicture@@6B@", 0), S4("\?\?_7CSimsPicture@@6B@", 4), S4("\?\?_7CSimsPicture@@6B@", 8), S4("\?\?_7CSimsPicture@@6B@", 12), S4("\?\?_7CSimsPicture@@6B@", 16), SB("\?\?_7CSimsPicture@@6B@", 20), SB("\?\?_7CSimsPicture@@6B@", 21),
            S4("\?\?_7CSimsPictureView@@6B@", 0), S4("\?\?_7CSimsPictureView@@6B@", 4), S4("\?\?_7CSimsPictureView@@6B@", 8), S4("\?\?_7CSimsPictureView@@6B@", 12), S4("\?\?_7CSimsPictureView@@6B@", 16), S4("\?\?_7CSimsPictureView@@6B@", 20), SB("\?\?_7CSimsPictureView@@6B@", 24), SB("\?\?_7CSimsPictureView@@6B@", 25),
            S4("\?\?_7CSimsRadioButton@@6B@", 0), S4("\?\?_7CSimsRadioButton@@6B@", 4), S4("\?\?_7CSimsRadioButton@@6B@", 8), S4("\?\?_7CSimsRadioButton@@6B@", 12), S4("\?\?_7CSimsRadioButton@@6B@", 16), S4("\?\?_7CSimsRadioButton@@6B@", 20), SB("\?\?_7CSimsRadioButton@@6B@", 24), SB("\?\?_7CSimsRadioButton@@6B@", 25),
            STR24("\?\?_7CSimsScrollBar@@6B@"),
            S4("\?\?_7CSimsShadow@@6B@", 0), S4("\?\?_7CSimsShadow@@6B@", 4), S4("\?\?_7CSimsShadow@@6B@", 8), S4("\?\?_7CSimsShadow@@6B@", 12), S4("\?\?_7CSimsShadow@@6B@", 16), SB("\?\?_7CSimsShadow@@6B@", 20),
            S4("\?\?_7CSimsShadowButton@@6B@", 0), S4("\?\?_7CSimsShadowButton@@6B@", 4), S4("\?\?_7CSimsShadowButton@@6B@", 8), S4("\?\?_7CSimsShadowButton@@6B@", 12), S4("\?\?_7CSimsShadowButton@@6B@", 16), S4("\?\?_7CSimsShadowButton@@6B@", 20), SB("\?\?_7CSimsShadowButton@@6B@", 24), SB("\?\?_7CSimsShadowButton@@6B@", 25), SB("\?\?_7CSimsShadowButton@@6B@", 26),
            S4("\?\?_7CSimsSlider@@6B@", 0), S4("\?\?_7CSimsSlider@@6B@", 4), S4("\?\?_7CSimsSlider@@6B@", 8), S4("\?\?_7CSimsSlider@@6B@", 12), S4("\?\?_7CSimsSlider@@6B@", 16), SB("\?\?_7CSimsSlider@@6B@", 20),
            S4("\?\?_7CSimsSpinButtonCtrl@@6B@", 0), S4("\?\?_7CSimsSpinButtonCtrl@@6B@", 4), S4("\?\?_7CSimsSpinButtonCtrl@@6B@", 8), S4("\?\?_7CSimsSpinButtonCtrl@@6B@", 12), S4("\?\?_7CSimsSpinButtonCtrl@@6B@", 16), S4("\?\?_7CSimsSpinButtonCtrl@@6B@", 20), S4("\?\?_7CSimsSpinButtonCtrl@@6B@", 24), SB("\?\?_7CSimsSpinButtonCtrl@@6B@", 28),
            S4("\?\?_7CSimsStatic@@6B@", 0), S4("\?\?_7CSimsStatic@@6B@", 4), S4("\?\?_7CSimsStatic@@6B@", 8), S4("\?\?_7CSimsStatic@@6B@", 12), S4("\?\?_7CSimsStatic@@6B@", 16), SB("\?\?_7CSimsStatic@@6B@", 20),
            S4("\?\?_7CSimsTabCtrl@@6B@", 0), S4("\?\?_7CSimsTabCtrl@@6B@", 4), S4("\?\?_7CSimsTabCtrl@@6B@", 8), S4("\?\?_7CSimsTabCtrl@@6B@", 12), S4("\?\?_7CSimsTabCtrl@@6B@", 16), SB("\?\?_7CSimsTabCtrl@@6B@", 20), SB("\?\?_7CSimsTabCtrl@@6B@", 21),
            STR28("\?\?_7CSimsTabCtrlHelper@@6B@"),
            S4("\?\?_7CSimsTabPage@@6B@", 0), S4("\?\?_7CSimsTabPage@@6B@", 4), S4("\?\?_7CSimsTabPage@@6B@", 8), S4("\?\?_7CSimsTabPage@@6B@", 12), S4("\?\?_7CSimsTabPage@@6B@", 16), SB("\?\?_7CSimsTabPage@@6B@", 20), SB("\?\?_7CSimsTabPage@@6B@", 21),
            S4("\?\?_7CSimsText@@6B@", 0), S4("\?\?_7CSimsText@@6B@", 4), S4("\?\?_7CSimsText@@6B@", 8), S4("\?\?_7CSimsText@@6B@", 12), SB("\?\?_7CSimsText@@6B@", 16), SB("\?\?_7CSimsText@@6B@", 17), SB("\?\?_7CSimsText@@6B@", 18),
            S4("\?\?_7CSimsTextArt@@6B@", 0), S4("\?\?_7CSimsTextArt@@6B@", 4), S4("\?\?_7CSimsTextArt@@6B@", 8), S4("\?\?_7CSimsTextArt@@6B@", 12), S4("\?\?_7CSimsTextArt@@6B@", 16), SB("\?\?_7CSimsTextArt@@6B@", 20), SB("\?\?_7CSimsTextArt@@6B@", 21),
            STR20("\?\?_FCNotify@@QAEXXZ"),
            S4("\?AddFont@@YGHH@Z", 0), S4("\?AddFont@@YGHH@Z", 4), S4("\?AddFont@@YGHH@Z", 8), S4("\?AddFont@@YGHH@Z", 12), SB("\?AddFont@@YGHH@Z", 16),
            S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 0), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 4), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 8), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 12), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 16), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 20), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 24), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 28), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 32), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 36), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 40), S4("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 44), SB("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 48), SB("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 49), SB("\?AddPage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@PADH@Z", 50),
            S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 0), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 4), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 8), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 12), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 16), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 20), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 24), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 28), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 32), S4("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 36), SB("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 40), SB("\?AppendMenuA@CSims3PieceButton@@QAEHFKF@Z", 41),
            S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 0), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 4), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 8), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 12), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 16), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 20), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 24), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 28), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 32), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 36), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 40), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 44), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 48), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 52), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 56), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@AAVCPoint@@K@Z", 57),
            S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 0), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 4), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 8), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 12), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 16), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 20), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 24), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 28), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 32), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 36), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 40), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 44), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 45), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@AAVCRect@@K@Z", 46),
            S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 0), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 4), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 8), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 12), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 16), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 20), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 24), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 28), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 32), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 36), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 40), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 41), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHHHK@Z", 42),
            S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 0), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 4), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 8), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 12), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 16), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 20), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 24), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 28), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 32), S4("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 36), SB("\?BitBlt@CDIBSection@@QAEXPAVCDC@@HHHHK@Z", 40),
            S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 0), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 4), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 8), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 12), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 16), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 20), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 24), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 28), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 32), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 36), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 40), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 44), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 48), S4("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 52), SB("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 56), SB("\?BlurShadowFromMask@CDIBSection@@QAEHAAV1@HPAVCDIBPal@@@Z", 57),
            S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 0), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 4), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 8), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 12), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 16), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 20), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 24), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 28), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 32), S4("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 36), SB("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 40), SB("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 41), SB("\?CheckMenuItem@CSims3PieceButton@@QAEKFF@Z", 42),
            S4("\?ClearSimsFonts@@YGXXZ", 0), S4("\?ClearSimsFonts@@YGXXZ", 4), S4("\?ClearSimsFonts@@YGXXZ", 8), S4("\?ClearSimsFonts@@YGXXZ", 12), S4("\?ClearSimsFonts@@YGXXZ", 16), SB("\?ClearSimsFonts@@YGXXZ", 20), SB("\?ClearSimsFonts@@YGXXZ", 21), SB("\?ClearSimsFonts@@YGXXZ", 22),
            STR40("\?Clip@CDIBSection@@QAEHPAV1@0AAH11111@Z"),
            S4("\?Close@CSimsDialog@@UAEXXZ", 0), S4("\?Close@CSimsDialog@@UAEXXZ", 4), S4("\?Close@CSimsDialog@@UAEXXZ", 8), S4("\?Close@CSimsDialog@@UAEXXZ", 12), S4("\?Close@CSimsDialog@@UAEXXZ", 16), S4("\?Close@CSimsDialog@@UAEXXZ", 20), SB("\?Close@CSimsDialog@@UAEXXZ", 24), SB("\?Close@CSimsDialog@@UAEXXZ", 25), SB("\?Close@CSimsDialog@@UAEXXZ", 26),
            STR28("\?Close@CSimsTabCtrl@@UAEXXZ"),
            STR64("\?CompareItem@CSimsCheckListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z"),
            STR60("\?CompareItem@CSimsComboBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z"),
            S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 0), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 4), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 8), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 12), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 16), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 20), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 24), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 28), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 32), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 36), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 40), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 44), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 48), S4("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 52), SB("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 56), SB("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 57), SB("\?CompareItem@CSimsListBox@@UAEHPAUtagCOMPAREITEMSTRUCT@@@Z", 58),
            S4("\?Construct@CSimsFont@@QAEXXZ", 0), S4("\?Construct@CSimsFont@@QAEXXZ", 4), S4("\?Construct@CSimsFont@@QAEXXZ", 8), S4("\?Construct@CSimsFont@@QAEXXZ", 12), S4("\?Construct@CSimsFont@@QAEXXZ", 16), S4("\?Construct@CSimsFont@@QAEXXZ", 20), S4("\?Construct@CSimsFont@@QAEXXZ", 24), SB("\?Construct@CSimsFont@@QAEXXZ", 28),
            S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 0), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 4), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 8), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 12), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 16), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 20), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 24), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 28), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 32), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 36), S4("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 40), SB("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@@Z", 44),
            STR56("\?CopyBits@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@@Z"),
            S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 0), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 4), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 8), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 12), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 16), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 20), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 24), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 28), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 32), SB("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 36), SB("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 37), SB("\?CopyBits@CDIBSection@@QAEXPAV1@HHHH@Z", 38),
            S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 0), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 4), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 8), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 12), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 16), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 20), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 24), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 28), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 32), S4("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 36), SB("\?CopyBits@CDIBSection@@QAEXPAV1@HHHHHH@Z", 40),
            S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 0), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 4), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 8), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 12), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 16), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 20), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 24), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 28), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 32), S4("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 36), SB("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 40), SB("\?Create@CDIBPal@@QAEHPAUtagBITMAPINFO@@@Z", 41),
            S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 0), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 4), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 8), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 12), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 16), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 20), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 24), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 28), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 32), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 36), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 40), S4("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 44), SB("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 48), SB("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 49), SB("\?Create@CSimsHotJoy@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 50),
            S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 0), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 4), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 8), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 12), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 16), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 20), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 24), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 28), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 32), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 36), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 40), S4("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 44), SB("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 48), SB("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 49), SB("\?Create@CSimsHotKey@@QAEHKABUtagRECT@@PAVCWnd@@I@Z", 50),
            S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 0), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 4), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 8), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 12), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 16), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 20), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 24), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 28), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 32), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 36), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 40), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 44), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 48), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 52), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 56), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 60), S4("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 64), SB("\?CreateDIBSection@CDIBSection@@QAEHPAUtagBITMAPINFO@@PAVCDIBPal@@I@Z", 68),
            S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 0), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 4), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 8), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 12), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 16), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 20), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 24), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 28), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 32), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 36), S4("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 40), SB("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 44), SB("\?CreateMask@CDIBSection@@QAEXPAV1@HHHHHHEHH@Z", 45),
            S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 0), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 4), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 8), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 12), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 16), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 20), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 24), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 28), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 32), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 36), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 40), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 44), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 48), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 52), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 56), S4("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 60), SB("\?CreateMenu@CSims3PieceButton@@QAEPAUHMENU__@@PAUHINSTANCE__@@@Z", 64),
            S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 0), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 4), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 8), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 12), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 16), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 20), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 24), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 28), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 32), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 36), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 40), S4("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 44), SB("\?CtlColor@CSimsEdit@@IAEPAUHBRUSH__@@PAVCDC@@I@Z", 48),
            S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 0), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 4), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 8), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 12), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 16), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 20), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 24), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 28), S4("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 32), SB("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 36), SB("\?DeleteMenu@CSims3PieceButton@@QAEXXZ", 37),
            S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 0), S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 4), S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 8), S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 12), S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 16), S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 20), S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 24), S4("\?DisplayError@CSimsDialog@@UAEHH@Z", 28), SB("\?DisplayError@CSimsDialog@@UAEHH@Z", 32), SB("\?DisplayError@CSimsDialog@@UAEHH@Z", 33), SB("\?DisplayError@CSimsDialog@@UAEHH@Z", 34),
            S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 0), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 4), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 8), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 12), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 16), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 20), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 24), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 28), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 32), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 36), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 40), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 44), S4("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 48), SB("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 52), SB("\?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z", 53),
            S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 0), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 4), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 8), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 12), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 16), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 20), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 24), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 28), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 32), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 36), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 40), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 44), S4("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 48), SB("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 52), SB("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 53), SB("\?DoDataExchange@CSimsTabPage@@MAEXPAVCDataExchange@@@Z", 54),
            S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 0), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 4), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 8), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 12), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 16), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 20), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 24), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 28), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 32), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 36), S4("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 40), SB("\?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z", 44),
            S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 0), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 4), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 8), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 12), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 16), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 20), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 24), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 28), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 32), S4("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 36), SB("\?Draw@CDIBPal@@QAEXPAVCDC@@PAVCRect@@H@Z", 40),
            STR36("\?Draw@CSimsShadow@@QAEHPAVCDC@@HH@Z"),
            STR52("\?DrawItem@CSimsButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z"),
            S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 48), S4("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 52), SB("\?DrawItem@CSimsCheckButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 56),
            S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 48), S4("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 52), SB("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 56), SB("\?DrawItem@CSimsCheckListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 57),
            S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 48), SB("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 52), SB("\?DrawItem@CSimsComboBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 53),
            S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 48), SB("\?DrawItem@CSimsListBox@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 52),
            S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 48), S4("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 52), SB("\?DrawItem@CSimsRadioButton@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 56),
            STR52("\?DrawItem@CSimsSlider@@UAEXPAUtagDRAWITEMSTRUCT@@@Z"),
            S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 48), SB("\?DrawItem@CSimsTabCtrl@@MAEXPAUtagDRAWITEMSTRUCT@@@Z", 52),
            S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 48), S4("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 52), SB("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 56), SB("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 57), SB("\?DrawItem@CSimsTabCtrlHelper@@UAEXPAUtagDRAWITEMSTRUCT@@@Z", 58),
            S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 0), S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 4), S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 8), S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 12), S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 16), S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 20), S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 24), S4("\?DrawPalette@CSimsDialog@@IAEXXZ", 28), SB("\?DrawPalette@CSimsDialog@@IAEXXZ", 32),
            S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 0), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 4), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 8), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 12), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 16), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 20), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 24), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 28), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 32), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 36), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 40), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 44), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 48), S4("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 52), SB("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 56), SB("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 57), SB("\?DrawSimsText@CSimsFont@@QAEXPAVCDC@@VCString@@VCRect@@H@Z", 58),
            S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 0), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 4), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 8), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 12), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 16), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 20), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 24), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 28), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 32), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 36), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 40), S4("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 44), SB("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 48), SB("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 49), SB("\?EnablePage@CSimsTabCtrl@@QAEXPAVCSimsTabPage@@H@Z", 50),
            STR32("\?FillColor@CDIBSection@@QAEXE@Z"),
            STR40("\?FillColor@CDIBSection@@QAEXVCRect@@E@Z"),
            STR52("\?GetActivePage@CSimsTabCtrl@@QBEPAVCSimsTabPage@@XZ"),
            S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 0), S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 4), S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 8), S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 12), S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 16), S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 20), S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 24), S4("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 28), SB("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 32), SB("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 33), SB("\?GetActiveTab@CSimsTabCtrl@@QBEHXZ", 34),
            S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 0), S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 4), S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 8), S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 12), S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 16), S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 20), S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 24), S4("\?GetBMIHeight@CDIBSection@@QAEHXZ", 28), SB("\?GetBMIHeight@CDIBSection@@QAEHXZ", 32), SB("\?GetBMIHeight@CDIBSection@@QAEHXZ", 33),
            S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 0), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 4), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 8), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 12), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 16), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 20), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 24), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 28), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 32), S4("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 36), SB("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 40), SB("\?GetBitmap@CDIBSection@@QAEPAVCBitmap@@XZ", 41),
            S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 0), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 4), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 8), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 12), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 16), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 20), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 24), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 28), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 32), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 36), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 40), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 44), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 48), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 52), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 56), S4("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 60), SB("\?GetBitmapInfoAddress@CDIBSection@@QAEPAUtagBITMAPINFOHEADER@@XZ", 64),
            S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 0), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 4), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 8), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 12), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 16), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 20), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 24), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 28), S4("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 32), SB("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 36), SB("\?GetBitsAddress@CDIBSection@@QAEPAEXZ", 37),
            S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 0), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 4), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 8), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 12), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 16), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 20), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 24), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 28), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 32), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 36), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 40), S4("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 44), SB("\?GetBkgrndDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 48),
            S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 0), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 4), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 8), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 12), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 16), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 20), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 24), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 28), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 32), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 36), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 40), S4("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 44), SB("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 48), SB("\?GetBkgrndDib@CSimsTabPage@@QAEPAVCDIBSection@@XZ", 49),
            S4("\?GetChar@CSimsPicture@@QAEIXZ", 0), S4("\?GetChar@CSimsPicture@@QAEIXZ", 4), S4("\?GetChar@CSimsPicture@@QAEIXZ", 8), S4("\?GetChar@CSimsPicture@@QAEIXZ", 12), S4("\?GetChar@CSimsPicture@@QAEIXZ", 16), S4("\?GetChar@CSimsPicture@@QAEIXZ", 20), S4("\?GetChar@CSimsPicture@@QAEIXZ", 24), SB("\?GetChar@CSimsPicture@@QAEIXZ", 28), SB("\?GetChar@CSimsPicture@@QAEIXZ", 29),
            STR40("\?GetColorIdxNotUsed@CDIBSection@@QAEHXZ"),
            S4("\?GetFlags@CSimsPicture@@QAEIXZ", 0), S4("\?GetFlags@CSimsPicture@@QAEIXZ", 4), S4("\?GetFlags@CSimsPicture@@QAEIXZ", 8), S4("\?GetFlags@CSimsPicture@@QAEIXZ", 12), S4("\?GetFlags@CSimsPicture@@QAEIXZ", 16), S4("\?GetFlags@CSimsPicture@@QAEIXZ", 20), S4("\?GetFlags@CSimsPicture@@QAEIXZ", 24), SB("\?GetFlags@CSimsPicture@@QAEIXZ", 28), SB("\?GetFlags@CSimsPicture@@QAEIXZ", 29), SB("\?GetFlags@CSimsPicture@@QAEIXZ", 30),
            S4("\?GetHeight@CDIBSection@@QAEHXZ", 0), S4("\?GetHeight@CDIBSection@@QAEHXZ", 4), S4("\?GetHeight@CDIBSection@@QAEHXZ", 8), S4("\?GetHeight@CDIBSection@@QAEHXZ", 12), S4("\?GetHeight@CDIBSection@@QAEHXZ", 16), S4("\?GetHeight@CDIBSection@@QAEHXZ", 20), S4("\?GetHeight@CDIBSection@@QAEHXZ", 24), SB("\?GetHeight@CDIBSection@@QAEHXZ", 28), SB("\?GetHeight@CDIBSection@@QAEHXZ", 29), SB("\?GetHeight@CDIBSection@@QAEHXZ", 30),
            S4("\?GetHeight@CSimsButton@@QAEHXZ", 0), S4("\?GetHeight@CSimsButton@@QAEHXZ", 4), S4("\?GetHeight@CSimsButton@@QAEHXZ", 8), S4("\?GetHeight@CSimsButton@@QAEHXZ", 12), S4("\?GetHeight@CSimsButton@@QAEHXZ", 16), S4("\?GetHeight@CSimsButton@@QAEHXZ", 20), S4("\?GetHeight@CSimsButton@@QAEHXZ", 24), SB("\?GetHeight@CSimsButton@@QAEHXZ", 28), SB("\?GetHeight@CSimsButton@@QAEHXZ", 29), SB("\?GetHeight@CSimsButton@@QAEHXZ", 30),
            STR36("\?GetHeight@CSimsCheckButton@@QAEHXZ"),
            STR36("\?GetHeight@CSimsRadioButton@@QAEHXZ"),
            S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 0), S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 4), S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 8), S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 12), S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 16), S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 20), S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 24), S4("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 28), SB("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 32), SB("\?GetHitTest@CSimsSlider@@AAEIHH@Z", 33),
            S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 0), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 4), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 8), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 12), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 16), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 20), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 24), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 28), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 32), S4("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 36), SB("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 40), SB("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 41), SB("\?GetHotJoy@CSimsHotJoy@@QAE\?AW4JOYCODE@@XZ", 42),
            S4("\?GetHotKey@CSimsHotKey@@QAEHXZ", 0), S4("\?GetHotKey@CSimsHotKey@@QAEHXZ", 4), S4("\?GetHotKey@CSimsHotKey@@QAEHXZ", 8), S4("\?GetHotKey@CSimsHotKey@@QAEHXZ", 12), S4("\?GetHotKey@CSimsHotKey@@QAEHXZ", 16), S4("\?GetHotKey@CSimsHotKey@@QAEHXZ", 20), S4("\?GetHotKey@CSimsHotKey@@QAEHXZ", 24), SB("\?GetHotKey@CSimsHotKey@@QAEHXZ", 28), SB("\?GetHotKey@CSimsHotKey@@QAEHXZ", 29), SB("\?GetHotKey@CSimsHotKey@@QAEHXZ", 30),
            S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 0), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 4), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 8), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 12), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 16), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 20), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 24), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 28), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 32), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 36), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 40), S4("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 44), SB("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 48), SB("\?GetLastEditFieldNotValidated@CSimsDialog@@QAEHXZ", 49),
            S4("\?GetMask@CNotify@@QAEIXZ", 0), S4("\?GetMask@CNotify@@QAEIXZ", 4), S4("\?GetMask@CNotify@@QAEIXZ", 8), S4("\?GetMask@CNotify@@QAEIXZ", 12), S4("\?GetMask@CNotify@@QAEIXZ", 16), S4("\?GetMask@CNotify@@QAEIXZ", 20), SB("\?GetMask@CNotify@@QAEIXZ", 24),
            S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 52), SB("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 53), SB("\?GetMessageMap@CSims3PieceButton@@MBEPBUAFX_MSGMAP@@XZ", 54),
            S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsButton@@MBEPBUAFX_MSGMAP@@XZ", 48),
            S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 52), SB("\?GetMessageMap@CSimsCheckButton@@MBEPBUAFX_MSGMAP@@XZ", 53),
            S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 52), SB("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 53), SB("\?GetMessageMap@CSimsCheckListBox@@MBEPBUAFX_MSGMAP@@XZ", 54),
            S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 49), SB("\?GetMessageMap@CSimsComboBox@@MBEPBUAFX_MSGMAP@@XZ", 50),
            S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsDialog@@MBEPBUAFX_MSGMAP@@XZ", 48),
            S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 40), SB("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 45), SB("\?GetMessageMap@CSimsEdit@@MBEPBUAFX_MSGMAP@@XZ", 46),
            S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsExitButton@@MBEPBUAFX_MSGMAP@@XZ", 52),
            S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 49), SB("\?GetMessageMap@CSimsGroupbox@@MBEPBUAFX_MSGMAP@@XZ", 50),
            S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsHelpButton@@MBEPBUAFX_MSGMAP@@XZ", 52),
            S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsHotJoy@@MBEPBUAFX_MSGMAP@@XZ", 48),
            S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsHotKey@@MBEPBUAFX_MSGMAP@@XZ", 48),
            S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsListBox@@MBEPBUAFX_MSGMAP@@XZ", 49),
            S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsPicture@@MBEPBUAFX_MSGMAP@@XZ", 49),
            S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 52), SB("\?GetMessageMap@CSimsPictureView@@MBEPBUAFX_MSGMAP@@XZ", 53),
            S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 52), SB("\?GetMessageMap@CSimsRadioButton@@MBEPBUAFX_MSGMAP@@XZ", 53),
            STR52("\?GetMessageMap@CSimsScrollBar@@MBEPBUAFX_MSGMAP@@XZ"),
            S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 52), SB("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 53), SB("\?GetMessageMap@CSimsShadowButton@@MBEPBUAFX_MSGMAP@@XZ", 54),
            S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsSlider@@MBEPBUAFX_MSGMAP@@XZ", 48),
            S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 44), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 48), S4("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 52), SB("\?GetMessageMap@CSimsSpinButtonCtrl@@MBEPBUAFX_MSGMAP@@XZ", 56),
            S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsStatic@@MBEPBUAFX_MSGMAP@@XZ", 48),
            S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsTabCtrl@@MBEPBUAFX_MSGMAP@@XZ", 49),
            STR56("\?GetMessageMap@CSimsTabCtrlHelper@@MBEPBUAFX_MSGMAP@@XZ"),
            S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsTabPage@@MBEPBUAFX_MSGMAP@@XZ", 49),
            S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 40), SB("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 45), SB("\?GetMessageMap@CSimsText@@MBEPBUAFX_MSGMAP@@XZ", 46),
            S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 0), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 4), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 8), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 12), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 16), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 20), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 24), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 28), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 32), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 36), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 40), S4("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 44), SB("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 48), SB("\?GetMessageMap@CSimsTextArt@@MBEPBUAFX_MSGMAP@@XZ", 49),
            S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 0), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 4), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 8), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 12), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 16), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 20), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 24), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 28), S4("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 32), SB("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 36), SB("\?GetNumClrEntries@CDIBSection@@QAEHXZ", 37),
            S4("\?GetNumColors@CDIBPal@@QAEHXZ", 0), S4("\?GetNumColors@CDIBPal@@QAEHXZ", 4), S4("\?GetNumColors@CDIBPal@@QAEHXZ", 8), S4("\?GetNumColors@CDIBPal@@QAEHXZ", 12), S4("\?GetNumColors@CDIBPal@@QAEHXZ", 16), S4("\?GetNumColors@CDIBPal@@QAEHXZ", 20), S4("\?GetNumColors@CDIBPal@@QAEHXZ", 24), SB("\?GetNumColors@CDIBPal@@QAEHXZ", 28), SB("\?GetNumColors@CDIBPal@@QAEHXZ", 29),
            STR48("\?GetOSBuffer@CSimsShadow@@QAEPAVCDIBSection@@XZ"),
            S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 0), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 4), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 8), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 12), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 16), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 20), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 24), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 28), S4("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 32), SB("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 36), SB("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 37), SB("\?GetPal@CSimsDialog@@QAEPAVCDIBPal@@XZ", 38),
            STR40("\?GetPal@CSimsTabPage@@QAEPAVCDIBPal@@XZ"),
            STR32("\?GetRepCnt@CSimsPicture@@QAEIXZ"),
            STR60("\?GetRuntimeClass@CSims3PieceButton@@UBEPAUCRuntimeClass@@XZ"),
            S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 0), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 4), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 8), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 12), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 16), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 20), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 24), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 28), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 32), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 36), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 40), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 44), S4("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 48), SB("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 52), SB("\?GetRuntimeClass@CSimsDialog@@UBEPAUCRuntimeClass@@XZ", 53),
            S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 0), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 4), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 8), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 12), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 16), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 20), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 24), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 28), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 32), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 36), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 40), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 44), S4("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 48), SB("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 52), SB("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 53), SB("\?GetRuntimeClass@CSimsTabPage@@UBEPAUCRuntimeClass@@XZ", 54),
            S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 0), S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 4), S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 8), S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 12), S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 16), S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 20), S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 24), S4("\?GetScrollPos@CSimsSlider@@QBEHXZ", 28), SB("\?GetScrollPos@CSimsSlider@@QBEHXZ", 32), SB("\?GetScrollPos@CSimsSlider@@QBEHXZ", 33),
            S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 0), S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 4), S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 8), S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 12), S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 16), S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 20), S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 24), S4("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 28), SB("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 32), SB("\?GetSimsBaseFont@@YGPAVCFont@@H@Z", 33),
            STR48("\?GetTabCtrl@CSimsDialog@@QAEPAVCSimsTabCtrl@@XZ"),
            S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 0), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 4), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 8), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 12), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 16), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 20), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 24), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 28), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 32), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 36), S4("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 40), SB("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 44), SB("\?GetTabDib@CSimsDialog@@QAEPAVCDIBSection@@XZ", 45),
            S4("\?GetWidth@CDIBSection@@QAEHXZ", 0), S4("\?GetWidth@CDIBSection@@QAEHXZ", 4), S4("\?GetWidth@CDIBSection@@QAEHXZ", 8), S4("\?GetWidth@CDIBSection@@QAEHXZ", 12), S4("\?GetWidth@CDIBSection@@QAEHXZ", 16), S4("\?GetWidth@CDIBSection@@QAEHXZ", 20), S4("\?GetWidth@CDIBSection@@QAEHXZ", 24), SB("\?GetWidth@CDIBSection@@QAEHXZ", 28), SB("\?GetWidth@CDIBSection@@QAEHXZ", 29),
            S4("\?GetWidth@CSimsButton@@QAEHXZ", 0), S4("\?GetWidth@CSimsButton@@QAEHXZ", 4), S4("\?GetWidth@CSimsButton@@QAEHXZ", 8), S4("\?GetWidth@CSimsButton@@QAEHXZ", 12), S4("\?GetWidth@CSimsButton@@QAEHXZ", 16), S4("\?GetWidth@CSimsButton@@QAEHXZ", 20), S4("\?GetWidth@CSimsButton@@QAEHXZ", 24), SB("\?GetWidth@CSimsButton@@QAEHXZ", 28), SB("\?GetWidth@CSimsButton@@QAEHXZ", 29),
            S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 0), S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 4), S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 8), S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 12), S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 16), S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 20), S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 24), S4("\?GetWidth@CSimsCheckButton@@QAEHXZ", 28), SB("\?GetWidth@CSimsCheckButton@@QAEHXZ", 32), SB("\?GetWidth@CSimsCheckButton@@QAEHXZ", 33), SB("\?GetWidth@CSimsCheckButton@@QAEHXZ", 34),
            S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 0), S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 4), S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 8), S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 12), S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 16), S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 20), S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 24), S4("\?GetWidth@CSimsRadioButton@@QAEHXZ", 28), SB("\?GetWidth@CSimsRadioButton@@QAEHXZ", 32), SB("\?GetWidth@CSimsRadioButton@@QAEHXZ", 33), SB("\?GetWidth@CSimsRadioButton@@QAEHXZ", 34),
            S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 0), S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 4), S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 8), S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 12), S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 16), S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 20), S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 24), S4("\?GetWidthBytes@CDIBSection@@QAEHXZ", 28), SB("\?GetWidthBytes@CDIBSection@@QAEHXZ", 32), SB("\?GetWidthBytes@CDIBSection@@QAEHXZ", 33), SB("\?GetWidthBytes@CDIBSection@@QAEHXZ", 34),
            S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 0), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 4), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 8), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 12), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 16), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 20), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 24), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 28), S4("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 32), SB("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 36), SB("\?GetWindowTextA@CSimsText@@QBEHPADH@Z", 37),
            S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 0), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 4), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 8), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 12), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 16), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 20), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 24), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 28), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 32), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 36), S4("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 40), SB("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 44), SB("\?GetWindowTextA@CSimsText@@QBEXAAVCString@@@Z", 45),
            STR104("\?Init@CSims3PieceButton@@QAEHPAVCSimsFont@@0IPAVCDIBSection@@PAVCDIBPal@@KHPAUHINSTANCE__@@HHVCRect@@@Z"),
            S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 0), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 4), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 8), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 12), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 16), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 20), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 24), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 28), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 32), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 36), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 40), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 44), S4("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 48), SB("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 52), SB("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 53), SB("\?Init@CSimsButton@@UAEHIPAVCDIBPal@@PAUHINSTANCE__@@@Z", 54),
            S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 0), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 4), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 8), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 12), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 16), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 20), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 24), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 28), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 32), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 36), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 40), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 44), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 48), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 52), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 56), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 60), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 64), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 68), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 72), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 76), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 80), S4("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 84), SB("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 88), SB("\?Init@CSimsCheckButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 89),
            STR60("\?Init@CSimsCheckListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z"),
            STR56("\?Init@CSimsComboBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z"),
            STR44("\?Init@CSimsDialog@@UAEHHPAUHINSTANCE__@@K@Z"),
            STR52("\?Init@CSimsEdit@@QAEXPAVCFont@@KKKPAVCSimsText@@K@Z"),
            S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 0), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 4), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 8), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 12), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 16), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 20), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 24), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 28), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 32), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 36), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 40), S4("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 44), SB("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 48), SB("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 49), SB("\?Init@CSimsExitButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 50),
            S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 0), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 4), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 8), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 12), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 16), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 20), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 24), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 28), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 32), S4("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 36), SB("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 40), SB("\?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z", 41),
            S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 0), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 4), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 8), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 12), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 16), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 20), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 24), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 28), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 32), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 36), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 40), S4("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 44), SB("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 48), SB("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 49), SB("\?Init@CSimsHelpButton@@QAEHPAVCDIBPal@@VCPoint@@@Z", 50),
            S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 0), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 4), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 8), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 12), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 16), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 20), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 24), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 28), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 32), S4("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 36), SB("\?Init@CSimsHotJoy@@QAEXPAVCSimsFont@@K@Z", 40),
            S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 0), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 4), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 8), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 12), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 16), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 20), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 24), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 28), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 32), S4("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 36), SB("\?Init@CSimsHotKey@@QAEXPAVCSimsFont@@K@Z", 40),
            S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 0), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 4), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 8), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 12), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 16), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 20), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 24), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 28), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 32), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 36), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 40), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 44), S4("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 48), SB("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 52), SB("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 53), SB("\?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z", 54),
            S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 0), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 4), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 8), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 12), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 16), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 20), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 24), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 28), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 32), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 36), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 40), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 44), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 48), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 52), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 56), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 60), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 64), S4("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 68), SB("\?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 72),
            S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 0), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 4), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 8), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 12), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 16), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 20), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 24), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 28), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 32), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 36), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 40), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 44), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 48), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 52), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 56), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 60), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 64), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 68), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 72), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 76), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 80), S4("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 84), SB("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 88), SB("\?Init@CSimsRadioButton@@UAEHPAVCSimsFont@@IPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z", 89),
            S4("\?Init@CSimsScrollBar@@QAEXXZ", 0), S4("\?Init@CSimsScrollBar@@QAEXXZ", 4), S4("\?Init@CSimsScrollBar@@QAEXXZ", 8), S4("\?Init@CSimsScrollBar@@QAEXXZ", 12), S4("\?Init@CSimsScrollBar@@QAEXXZ", 16), S4("\?Init@CSimsScrollBar@@QAEXXZ", 20), S4("\?Init@CSimsScrollBar@@QAEXXZ", 24), SB("\?Init@CSimsScrollBar@@QAEXXZ", 28),
            STR76("\?Init@CSimsShadow@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@HHKHH@Z"),
            STR104("\?Init@CSimsShadowButton@@QAEHPAVCSimsFont@@0IPAVCDIBSection@@PAVCDIBPal@@KHPAUHINSTANCE__@@HHVCRect@@@Z"),
            S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 0), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 4), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 8), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 12), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 16), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 20), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 24), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 28), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 32), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 36), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 40), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 44), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 48), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 52), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 56), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 60), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 64), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 68), S4("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 72), SB("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 76), SB("\?Init@CSimsSlider@@QAEHIIPAVCDIBSection@@PAVCDIBPal@@IIIIHHPAUHINSTANCE__@@@Z", 77),
            S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 0), S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 4), S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 8), S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 12), S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 16), S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 20), S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 24), S4("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 28), SB("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 32), SB("\?Init@CSimsSpinButtonCtrl@@QAEXXZ", 33),
            S4("\?Init@CSimsStatic@@QAEXXZ", 0), S4("\?Init@CSimsStatic@@QAEXXZ", 4), S4("\?Init@CSimsStatic@@QAEXXZ", 8), S4("\?Init@CSimsStatic@@QAEXXZ", 12), S4("\?Init@CSimsStatic@@QAEXXZ", 16), S4("\?Init@CSimsStatic@@QAEXXZ", 20), SB("\?Init@CSimsStatic@@QAEXXZ", 24), SB("\?Init@CSimsStatic@@QAEXXZ", 25),
            S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 0), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 4), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 8), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 12), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 16), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 20), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 24), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 28), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 32), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 36), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 40), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 44), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 48), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 52), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 56), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 60), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 64), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 68), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 72), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 76), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 80), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 84), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 88), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 92), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 96), S4("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 100), SB("\?Init@CSimsTabCtrl@@QAEHPAVCSimsDialog@@PAVCFont@@IPAVCDIBSection@@PAVCDIBPal@@KKHHHHPAUHINSTANCE__@@K@Z", 104),
            S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 0), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 4), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 8), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 12), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 16), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 20), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 24), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 28), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 32), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 36), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 40), S4("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 44), SB("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 48), SB("\?Init@CSimsTabCtrlHelper@@QAEXPAVCSimsTabCtrl@@@Z", 49),
            STR28("\?Init@CSimsTabPage@@QAEHH@Z"),
            STR72("\?Init@CSimsTabPage@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@@Z"),
            S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 0), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 4), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 8), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 12), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 16), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 20), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 24), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 28), S4("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 32), SB("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 36), SB("\?Init@CSimsText@@QAEXPAVCSimsFont@@@Z", 37),
            S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 0), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 4), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 8), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 12), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 16), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 20), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 24), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 28), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 32), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 36), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 40), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 44), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 48), S4("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 52), SB("\?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z", 56),
            S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 0), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 4), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 8), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 12), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 16), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 20), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 24), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 28), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 32), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 36), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 40), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 44), S4("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 48), SB("\?InitColorTables@CDIBPal@@IAEXPAUtagPALETTEENTRY@@@Z", 52),
            S4("\?InitControls@CSimsTabPage@@UAEXXZ", 0), S4("\?InitControls@CSimsTabPage@@UAEXXZ", 4), S4("\?InitControls@CSimsTabPage@@UAEXXZ", 8), S4("\?InitControls@CSimsTabPage@@UAEXXZ", 12), S4("\?InitControls@CSimsTabPage@@UAEXXZ", 16), S4("\?InitControls@CSimsTabPage@@UAEXXZ", 20), S4("\?InitControls@CSimsTabPage@@UAEXXZ", 24), S4("\?InitControls@CSimsTabPage@@UAEXXZ", 28), SB("\?InitControls@CSimsTabPage@@UAEXXZ", 32), SB("\?InitControls@CSimsTabPage@@UAEXXZ", 33), SB("\?InitControls@CSimsTabPage@@UAEXXZ", 34),
            S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 0), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 4), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 8), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 12), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 16), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 20), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 24), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 28), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 32), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 36), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 40), S4("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 44), SB("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 48), SB("\?InitTab@CSimsDialog@@UAEHHHHHHPAUHINSTANCE__@@@Z", 49),
            STR36("\?IsChecked@CSimsCheckButton@@QAEHXZ"),
            STR36("\?IsChecked@CSimsRadioButton@@QAEHXZ"),
            S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 0), S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 4), S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 8), S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 12), S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 16), S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 20), S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 24), S4("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 28), SB("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 32), SB("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 33), SB("\?IsPtrValid@CDIBSection@@QAEHPAE@Z", 34),
            S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 0), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 4), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 8), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 12), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 16), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 20), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 24), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 28), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 32), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 36), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 40), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 44), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 48), S4("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 52), SB("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 56), SB("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 57), SB("\?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z", 58),
            STR60("\?MeasureItem@CSimsComboBox@@UAEXPAUtagMEASUREITEMSTRUCT@@@Z"),
            S4("\?NextTab@CSimsTabCtrl@@QAEXXZ", 0), S4("\?NextTab@CSimsTabCtrl@@QAEXXZ", 4), S4("\?NextTab@CSimsTabCtrl@@QAEXXZ", 8), S4("\?NextTab@CSimsTabCtrl@@QAEXXZ", 12), S4("\?NextTab@CSimsTabCtrl@@QAEXXZ", 16), S4("\?NextTab@CSimsTabCtrl@@QAEXXZ", 20), S4("\?NextTab@CSimsTabCtrl@@QAEXXZ", 24), SB("\?NextTab@CSimsTabCtrl@@QAEXXZ", 28), SB("\?NextTab@CSimsTabCtrl@@QAEXXZ", 29),
            S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 0), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 4), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 8), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 12), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 16), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 20), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 24), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 28), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 32), S4("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 36), SB("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 40), SB("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 41), SB("\?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z", 42),
            STR44("\?OnActivate@CSimsTabPage@@QAEXIPAVCWnd@@H@Z"),
            S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 0), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 4), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 8), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 12), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 16), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 20), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 24), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 28), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 32), S4("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 36), SB("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 40), SB("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 41), SB("\?OnChBtnChecked@CSimsCheckButton@@IAEJIJ@Z", 42),
            S4("\?OnChar@CSimsEdit@@IAEXIII@Z", 0), S4("\?OnChar@CSimsEdit@@IAEXIII@Z", 4), S4("\?OnChar@CSimsEdit@@IAEXIII@Z", 8), S4("\?OnChar@CSimsEdit@@IAEXIII@Z", 12), S4("\?OnChar@CSimsEdit@@IAEXIII@Z", 16), S4("\?OnChar@CSimsEdit@@IAEXIII@Z", 20), S4("\?OnChar@CSimsEdit@@IAEXIII@Z", 24), SB("\?OnChar@CSimsEdit@@IAEXIII@Z", 28),
            STR32("\?OnChar@CSimsPicture@@IAEXIII@Z"),
            S4("\?OnClose@CSimsTabPage@@QAEXXZ", 0), S4("\?OnClose@CSimsTabPage@@QAEXXZ", 4), S4("\?OnClose@CSimsTabPage@@QAEXXZ", 8), S4("\?OnClose@CSimsTabPage@@QAEXXZ", 12), S4("\?OnClose@CSimsTabPage@@QAEXXZ", 16), S4("\?OnClose@CSimsTabPage@@QAEXXZ", 20), S4("\?OnClose@CSimsTabPage@@QAEXXZ", 24), SB("\?OnClose@CSimsTabPage@@QAEXXZ", 28), SB("\?OnClose@CSimsTabPage@@QAEXXZ", 29),
            S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 0), S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 4), S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 8), S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 12), S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 16), S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 20), S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 24), S4("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 28), SB("\?OnCommand@CSimsDialog@@MAEHIJ@Z", 32),
            STR56("\?OnDrawItem@CSimsTabCtrl@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z"),
            S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 0), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 4), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 8), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 12), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 16), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 20), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 24), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 28), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 32), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 36), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 40), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 44), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 48), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 52), S4("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 56), SB("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 60), SB("\?OnDrawItem@CSimsTabCtrlHelper@@IAEXHPAUtagDRAWITEMSTRUCT@@@Z", 61),
            S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 0), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 4), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 8), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 12), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 16), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 20), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 24), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 28), S4("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 32), SB("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 36), SB("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 37), SB("\?OnEditKillFocus@CSimsDialog@@IAEJIJ@Z", 38),
            S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 0), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 4), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 8), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 12), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 16), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 20), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 24), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 28), S4("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 32), SB("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 36), SB("\?OnEditSetFocus@CSimsDialog@@IAEJIJ@Z", 37),
            S4("\?OnEnable@CSimsText@@IAEXH@Z", 0), S4("\?OnEnable@CSimsText@@IAEXH@Z", 4), S4("\?OnEnable@CSimsText@@IAEXH@Z", 8), S4("\?OnEnable@CSimsText@@IAEXH@Z", 12), S4("\?OnEnable@CSimsText@@IAEXH@Z", 16), S4("\?OnEnable@CSimsText@@IAEXH@Z", 20), S4("\?OnEnable@CSimsText@@IAEXH@Z", 24), SB("\?OnEnable@CSimsText@@IAEXH@Z", 28),
            S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 36), S4("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 44), SB("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 45), SB("\?OnEraseBkgnd@CSimsCheckButton@@IAEHPAVCDC@@@Z", 46),
            STR48("\?OnEraseBkgnd@CSimsCheckListBox@@IAEHPAVCDC@@@Z"),
            STR44("\?OnEraseBkgnd@CSimsComboBox@@IAEHPAVCDC@@@Z"),
            S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 36), SB("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsDialog@@IAEHPAVCDC@@@Z", 41),
            S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 36), SB("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 41), SB("\?OnEraseBkgnd@CSimsListBox@@IAEHPAVCDC@@@Z", 42),
            S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 36), SB("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 41), SB("\?OnEraseBkgnd@CSimsPicture@@IAEHPAVCDC@@@Z", 42),
            S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 36), S4("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 44), SB("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 45), SB("\?OnEraseBkgnd@CSimsRadioButton@@IAEHPAVCDC@@@Z", 46),
            S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 36), SB("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsSlider@@IAEHPAVCDC@@@Z", 41),
            S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 36), SB("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 41), SB("\?OnEraseBkgnd@CSimsTabCtrl@@IAEHPAVCDC@@@Z", 42),
            S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 0), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 4), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 8), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 12), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 16), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 20), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 24), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 28), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 32), S4("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 36), SB("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 40), SB("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 41), SB("\?OnEraseBkgnd@CSimsTabPage@@QAEHPAVCDC@@@Z", 42),
            STR40("\?OnEraseBkgnd@CSimsText@@IAEHPAVCDC@@@Z"),
            S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 0), S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 4), S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 8), S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 12), S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 16), S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 20), S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 24), S4("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 28), SB("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 32), SB("\?OnGetDefID@CSimsDialog@@IAEJIJ@Z", 33),
            STR40("\?OnGetDlgCode@CSims3PieceButton@@IAEIXZ"),
            S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 0), S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 4), S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 8), S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 12), S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 16), S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 20), S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 24), S4("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 28), SB("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 32), SB("\?OnGetDlgCode@CSimsHotKey@@IAEIXZ", 33),
            S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 0), S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 4), S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 8), S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 12), S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 16), S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 20), S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 24), S4("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 28), SB("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 32), SB("\?OnGetDlgCode@CSimsSlider@@IAEIXZ", 33),
            S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 0), S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 4), S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 8), S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 12), S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 16), S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 20), S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 24), S4("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 28), SB("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 32), SB("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 33), SB("\?OnGetDlgCode@CSimsTabCtrl@@IAEIXZ", 34),
            S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 0), S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 4), S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 8), S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 12), S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 16), S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 20), S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 24), S4("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 28), SB("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 32), SB("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 33), SB("\?OnGetDlgCode@CSimsTabPage@@QAEIXZ", 34),
            S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 28), SB("\?OnHelpMsg@CSimsButton@@IAEJIJ@Z", 32),
            S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 28), S4("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 32), SB("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 36), SB("\?OnHelpMsg@CSimsCheckButton@@QAEJIJ@Z", 37),
            S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 28), S4("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 32), SB("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 36), SB("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 37), SB("\?OnHelpMsg@CSimsCheckListBox@@IAEJIJ@Z", 38),
            S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 28), SB("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 32), SB("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 33), SB("\?OnHelpMsg@CSimsComboBox@@IAEJIJ@Z", 34),
            S4("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 24), SB("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 28), SB("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 29), SB("\?OnHelpMsg@CSimsEdit@@IAEJIJ@Z", 30),
            S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 28), S4("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 32), SB("\?OnHelpMsg@CSimsExitButton@@IAEJIJ@Z", 36),
            S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 28), S4("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 32), SB("\?OnHelpMsg@CSimsHelpButton@@IAEJIJ@Z", 36),
            S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 28), SB("\?OnHelpMsg@CSimsHotJoy@@IAEJIJ@Z", 32),
            S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 28), SB("\?OnHelpMsg@CSimsHotKey@@IAEJIJ@Z", 32),
            S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 28), SB("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 32), SB("\?OnHelpMsg@CSimsListBox@@IAEJIJ@Z", 33),
            S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 0), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 4), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 8), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 12), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 16), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 20), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 24), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 28), S4("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 32), SB("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 36), SB("\?OnHelpMsg@CSimsRadioButton@@IAEJIJ@Z", 37),
            S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 0), S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 4), S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 8), S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 12), S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 16), S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 20), S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 24), S4("\?OnInitDialog@CSimsDialog@@MAEHXZ", 28), SB("\?OnInitDialog@CSimsDialog@@MAEHXZ", 32), SB("\?OnInitDialog@CSimsDialog@@MAEHXZ", 33),
            S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 0), S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 4), S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 8), S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 12), S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 16), S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 20), S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 24), S4("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 28), SB("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 32), SB("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 33), SB("\?OnInitDialog@CSimsTabPage@@UAEHXZ", 34),
            S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 0), S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 4), S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 8), S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 12), S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 16), S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 20), S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 24), S4("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 28), SB("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 32), SB("\?OnKeyDown@CSimsHotKey@@IAEXIII@Z", 33),
            S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 0), S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 4), S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 8), S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 12), S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 16), S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 20), S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 24), S4("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 28), SB("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 32), SB("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 33), SB("\?OnKeyDown@CSimsPicture@@IAEXIII@Z", 34),
            S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 0), S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 4), S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 8), S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 12), S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 16), S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 20), S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 24), S4("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 28), SB("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 32), SB("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 33), SB("\?OnKeyDown@CSimsTabCtrl@@IAEXIII@Z", 34),
            S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 0), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 4), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 8), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 12), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 16), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 20), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 24), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 28), S4("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 32), SB("\?OnKeyUp@CSimsCheckButton@@IAEXIII@Z", 36),
            STR32("\?OnKeyUp@CSimsHotKey@@IAEXIII@Z"),
            S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 0), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 4), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 8), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 12), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 16), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 20), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 24), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 28), S4("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 32), SB("\?OnKeyUp@CSimsRadioButton@@IAEXIII@Z", 36),
            STR48("\?OnKillFocus@CSims3PieceButton@@IAEXPAVCWnd@@@Z"),
            S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 0), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 4), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 8), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 12), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 16), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 20), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 24), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 28), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 32), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 36), S4("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 40), SB("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 44), SB("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 45), SB("\?OnKillFocus@CSimsCheckButton@@IAEXPAVCWnd@@@Z", 46),
            STR40("\?OnKillFocus@CSimsEdit@@IAEXPAVCWnd@@@Z"),
            S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 0), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 4), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 8), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 12), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 16), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 20), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 24), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 28), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 32), S4("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 36), SB("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 40), SB("\?OnKillFocus@CSimsHotJoy@@IAEXPAVCWnd@@@Z", 41),
            S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 0), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 4), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 8), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 12), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 16), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 20), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 24), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 28), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 32), S4("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 36), SB("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 40), SB("\?OnKillFocus@CSimsHotKey@@IAEXPAVCWnd@@@Z", 41),
            S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 0), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 4), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 8), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 12), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 16), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 20), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 24), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 28), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 32), S4("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 36), SB("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 40), SB("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 41), SB("\?OnKillFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 42),
            S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 0), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 4), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 8), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 12), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 16), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 20), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 24), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 28), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 32), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 36), S4("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 40), SB("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 44), SB("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 45), SB("\?OnKillFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 46),
            S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 0), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 4), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 8), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 12), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 16), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 20), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 24), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 28), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 32), S4("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 36), SB("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 40), SB("\?OnKillFocus@CSimsSlider@@IAEXPAVCWnd@@@Z", 41),
            STR52("\?OnLButtonDblClk@CSimsCheckButton@@IAEXIVCPoint@@@Z"),
            S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 48), SB("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 49), SB("\?OnLButtonDown@CSims3PieceButton@@IAEXIVCPoint@@@Z", 50),
            S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 48), SB("\?OnLButtonDown@CSimsCheckButton@@IAEXIVCPoint@@@Z", 49),
            S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 48), SB("\?OnLButtonDown@CSimsRadioButton@@IAEXIVCPoint@@@Z", 49),
            S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 48), SB("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 49), SB("\?OnLButtonDown@CSimsShadowButton@@IAEXIVCPoint@@@Z", 50),
            S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 40), SB("\?OnLButtonDown@CSimsSlider@@IAEXIVCPoint@@@Z", 44),
            S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 40), SB("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonDown@CSimsTabCtrl@@IAEXIVCPoint@@@Z", 45),
            STR52("\?OnLButtonDown@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z"),
            S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonUp@CSims3PieceButton@@IAEXIVCPoint@@@Z", 48),
            STR48("\?OnLButtonUp@CSimsCheckButton@@IAEXIVCPoint@@@Z"),
            S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 40), SB("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 45), SB("\?OnLButtonUp@CSimsExitButton@@IAEXIVCPoint@@@Z", 46),
            S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 40), SB("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 45), SB("\?OnLButtonUp@CSimsHelpButton@@IAEXIVCPoint@@@Z", 46),
            STR48("\?OnLButtonUp@CSimsRadioButton@@IAEXIVCPoint@@@Z"),
            S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonUp@CSimsShadowButton@@IAEXIVCPoint@@@Z", 48),
            S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 36), SB("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 40), SB("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 41), SB("\?OnLButtonUp@CSimsSlider@@IAEXIVCPoint@@@Z", 42),
            STR44("\?OnLButtonUp@CSimsTabCtrl@@IAEXIVCPoint@@@Z"),
            S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 0), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 4), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 8), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 12), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 16), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 20), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 24), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 28), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 32), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 36), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 40), S4("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 44), SB("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 48), SB("\?OnLButtonUp@CSimsTabCtrlHelper@@IAEXIVCPoint@@@Z", 49),
            S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 0), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 4), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 8), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 12), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 16), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 20), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 24), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 28), S4("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 32), SB("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 36), SB("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 37), SB("\?OnLDialogParent@CSimsDialog@@IAEJIJ@Z", 38),
            S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 0), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 4), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 8), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 12), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 16), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 20), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 24), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 28), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 32), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 36), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 40), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 44), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 48), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 52), S4("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 56), SB("\?OnMeasureItem@CSimsDialog@@IAEXHPAUtagMEASUREITEMSTRUCT@@@Z", 60),
            S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnMouseMove@CSims3PieceButton@@IAEXIVCPoint@@@Z", 48),
            S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 0), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 4), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 8), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 12), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 16), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 20), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 24), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 28), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 32), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 36), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 40), S4("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 44), SB("\?OnMouseMove@CSimsShadowButton@@IAEXIVCPoint@@@Z", 48),
            S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 0), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 4), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 8), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 12), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 16), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 20), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 24), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 28), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 32), S4("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 36), SB("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 40), SB("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 41), SB("\?OnMouseMove@CSimsSlider@@IAEXIVCPoint@@@Z", 42),
            S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 0), S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 4), S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 8), S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 12), S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 16), S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 20), S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 24), S4("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 28), SB("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 32), SB("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 33), SB("\?OnMyKeyDown@CSimsSlider@@IAEJIJ@Z", 34),
            S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 0), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 4), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 8), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 12), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 16), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 20), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 24), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 28), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 32), S4("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 36), SB("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 40), SB("\?OnNcHitTest@CSimsDialog@@IAEIVCPoint@@@Z", 41),
            STR40("\?OnNcHitTest@CSimsText@@IAEIVCPoint@@@Z"),
            S4("\?OnNcPaint@CSimsSlider@@IAEXXZ", 0), S4("\?OnNcPaint@CSimsSlider@@IAEXXZ", 4), S4("\?OnNcPaint@CSimsSlider@@IAEXXZ", 8), S4("\?OnNcPaint@CSimsSlider@@IAEXXZ", 12), S4("\?OnNcPaint@CSimsSlider@@IAEXXZ", 16), S4("\?OnNcPaint@CSimsSlider@@IAEXXZ", 20), S4("\?OnNcPaint@CSimsSlider@@IAEXXZ", 24), SB("\?OnNcPaint@CSimsSlider@@IAEXXZ", 28), SB("\?OnNcPaint@CSimsSlider@@IAEXXZ", 29), SB("\?OnNcPaint@CSimsSlider@@IAEXXZ", 30),
            STR32("\?OnNcPaint@CSimsTabCtrl@@IAEXXZ"),
            S4("\?OnPaint@CSimsGroupbox@@IAEXXZ", 0), S4("\?OnPaint@CSimsGroupbox@@IAEXXZ", 4), S4("\?OnPaint@CSimsGroupbox@@IAEXXZ", 8), S4("\?OnPaint@CSimsGroupbox@@IAEXXZ", 12), S4("\?OnPaint@CSimsGroupbox@@IAEXXZ", 16), S4("\?OnPaint@CSimsGroupbox@@IAEXXZ", 20), S4("\?OnPaint@CSimsGroupbox@@IAEXXZ", 24), SB("\?OnPaint@CSimsGroupbox@@IAEXXZ", 28), SB("\?OnPaint@CSimsGroupbox@@IAEXXZ", 29), SB("\?OnPaint@CSimsGroupbox@@IAEXXZ", 30),
            S4("\?OnPaint@CSimsHotJoy@@IAEXXZ", 0), S4("\?OnPaint@CSimsHotJoy@@IAEXXZ", 4), S4("\?OnPaint@CSimsHotJoy@@IAEXXZ", 8), S4("\?OnPaint@CSimsHotJoy@@IAEXXZ", 12), S4("\?OnPaint@CSimsHotJoy@@IAEXXZ", 16), S4("\?OnPaint@CSimsHotJoy@@IAEXXZ", 20), S4("\?OnPaint@CSimsHotJoy@@IAEXXZ", 24), SB("\?OnPaint@CSimsHotJoy@@IAEXXZ", 28),
            S4("\?OnPaint@CSimsHotKey@@IAEXXZ", 0), S4("\?OnPaint@CSimsHotKey@@IAEXXZ", 4), S4("\?OnPaint@CSimsHotKey@@IAEXXZ", 8), S4("\?OnPaint@CSimsHotKey@@IAEXXZ", 12), S4("\?OnPaint@CSimsHotKey@@IAEXXZ", 16), S4("\?OnPaint@CSimsHotKey@@IAEXXZ", 20), S4("\?OnPaint@CSimsHotKey@@IAEXXZ", 24), SB("\?OnPaint@CSimsHotKey@@IAEXXZ", 28),
            S4("\?OnPaint@CSimsPicture@@IAEXXZ", 0), S4("\?OnPaint@CSimsPicture@@IAEXXZ", 4), S4("\?OnPaint@CSimsPicture@@IAEXXZ", 8), S4("\?OnPaint@CSimsPicture@@IAEXXZ", 12), S4("\?OnPaint@CSimsPicture@@IAEXXZ", 16), S4("\?OnPaint@CSimsPicture@@IAEXXZ", 20), S4("\?OnPaint@CSimsPicture@@IAEXXZ", 24), SB("\?OnPaint@CSimsPicture@@IAEXXZ", 28), SB("\?OnPaint@CSimsPicture@@IAEXXZ", 29),
            S4("\?OnPaint@CSimsTabPage@@QAEXXZ", 0), S4("\?OnPaint@CSimsTabPage@@QAEXXZ", 4), S4("\?OnPaint@CSimsTabPage@@QAEXXZ", 8), S4("\?OnPaint@CSimsTabPage@@QAEXXZ", 12), S4("\?OnPaint@CSimsTabPage@@QAEXXZ", 16), S4("\?OnPaint@CSimsTabPage@@QAEXXZ", 20), S4("\?OnPaint@CSimsTabPage@@QAEXXZ", 24), SB("\?OnPaint@CSimsTabPage@@QAEXXZ", 28), SB("\?OnPaint@CSimsTabPage@@QAEXXZ", 29),
            S4("\?OnPaint@CSimsText@@IAEXXZ", 0), S4("\?OnPaint@CSimsText@@IAEXXZ", 4), S4("\?OnPaint@CSimsText@@IAEXXZ", 8), S4("\?OnPaint@CSimsText@@IAEXXZ", 12), S4("\?OnPaint@CSimsText@@IAEXXZ", 16), S4("\?OnPaint@CSimsText@@IAEXXZ", 20), SB("\?OnPaint@CSimsText@@IAEXXZ", 24), SB("\?OnPaint@CSimsText@@IAEXXZ", 25), SB("\?OnPaint@CSimsText@@IAEXXZ", 26),
            S4("\?OnPaint@CSimsTextArt@@IAEXXZ", 0), S4("\?OnPaint@CSimsTextArt@@IAEXXZ", 4), S4("\?OnPaint@CSimsTextArt@@IAEXXZ", 8), S4("\?OnPaint@CSimsTextArt@@IAEXXZ", 12), S4("\?OnPaint@CSimsTextArt@@IAEXXZ", 16), S4("\?OnPaint@CSimsTextArt@@IAEXXZ", 20), S4("\?OnPaint@CSimsTextArt@@IAEXXZ", 24), SB("\?OnPaint@CSimsTextArt@@IAEXXZ", 28), SB("\?OnPaint@CSimsTextArt@@IAEXXZ", 29),
            S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 0), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 4), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 8), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 12), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 16), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 20), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 24), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 28), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 32), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 36), S4("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 40), SB("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 44), SB("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 45), SB("\?OnPaletteChanged@CSimsDialog@@IAEXPAVCWnd@@@Z", 46),
            STR48("\?OnPaletteChanged@CSimsTabPage@@QAEXPAVCWnd@@@Z"),
            S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 0), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 4), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 8), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 12), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 16), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 20), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 24), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 28), S4("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 32), SB("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 36), SB("\?OnParentNotify@CSimsDialog@@IAEXIJ@Z", 37),
            S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 0), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 4), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 8), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 12), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 16), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 20), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 24), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 28), S4("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 32), SB("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 36), SB("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 37), SB("\?OnQueryNewPalette@CSimsDialog@@IAEHXZ", 38),
            STR40("\?OnQueryNewPalette@CSimsTabPage@@QAEHXZ"),
            S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 0), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 4), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 8), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 12), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 16), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 20), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 24), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 28), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 32), S4("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 36), SB("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 40), SB("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 41), SB("\?OnRdBtnChecked@CSimsRadioButton@@IAEJIJ@Z", 42),
            S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 0), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 4), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 8), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 12), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 16), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 20), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 24), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 28), S4("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 32), SB("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 36), SB("\?OnSafeSetFocus@CSimsButton@@IAEJIJ@Z", 37),
            S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 0), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 4), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 8), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 12), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 16), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 20), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 24), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 28), S4("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 32), SB("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 36), SB("\?OnSafeSetFocus@CSimsDialog@@IAEJIJ@Z", 37),
            S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 0), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 4), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 8), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 12), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 16), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 20), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 24), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 28), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 32), S4("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 36), SB("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 40), SB("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 41), SB("\?OnSafeSetFocus@CSimsRadioButton@@IAEJIJ@Z", 42),
            S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 0), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 4), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 8), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 12), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 16), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 20), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 24), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 28), S4("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 32), SB("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 36), SB("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 37), SB("\?OnSetFocus@CSimsEdit@@IAEXPAVCWnd@@@Z", 38),
            S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 0), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 4), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 8), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 12), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 16), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 20), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 24), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 28), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 32), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 36), S4("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 40), SB("\?OnSetFocus@CSimsExitButton@@IAEXPAVCWnd@@@Z", 44),
            S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 0), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 4), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 8), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 12), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 16), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 20), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 24), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 28), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 32), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 36), S4("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 40), SB("\?OnSetFocus@CSimsHelpButton@@IAEXPAVCWnd@@@Z", 44),
            S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 0), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 4), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 8), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 12), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 16), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 20), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 24), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 28), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 32), S4("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 36), SB("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 40), SB("\?OnSetFocus@CSimsPicture@@IAEXPAVCWnd@@@Z", 41),
            S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 0), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 4), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 8), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 12), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 16), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 20), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 24), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 28), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 32), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 36), S4("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 40), SB("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 44), SB("\?OnSetFocus@CSimsRadioButton@@IAEXPAVCWnd@@@Z", 45),
            S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 0), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 4), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 8), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 12), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 16), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 20), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 24), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 28), S4("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 32), SB("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 36), SB("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 37), SB("\?OnSliderClicked@CSimsSlider@@IAEJIJ@Z", 38),
            S4("\?OnSysExit@CSimsDialog@@IAEXXZ", 0), S4("\?OnSysExit@CSimsDialog@@IAEXXZ", 4), S4("\?OnSysExit@CSimsDialog@@IAEXXZ", 8), S4("\?OnSysExit@CSimsDialog@@IAEXXZ", 12), S4("\?OnSysExit@CSimsDialog@@IAEXXZ", 16), S4("\?OnSysExit@CSimsDialog@@IAEXXZ", 20), S4("\?OnSysExit@CSimsDialog@@IAEXXZ", 24), SB("\?OnSysExit@CSimsDialog@@IAEXXZ", 28), SB("\?OnSysExit@CSimsDialog@@IAEXXZ", 29), SB("\?OnSysExit@CSimsDialog@@IAEXXZ", 30),
            S4("\?OnSysHelp@CSimsDialog@@IAEXXZ", 0), S4("\?OnSysHelp@CSimsDialog@@IAEXXZ", 4), S4("\?OnSysHelp@CSimsDialog@@IAEXXZ", 8), S4("\?OnSysHelp@CSimsDialog@@IAEXXZ", 12), S4("\?OnSysHelp@CSimsDialog@@IAEXXZ", 16), S4("\?OnSysHelp@CSimsDialog@@IAEXXZ", 20), S4("\?OnSysHelp@CSimsDialog@@IAEXXZ", 24), SB("\?OnSysHelp@CSimsDialog@@IAEXXZ", 28), SB("\?OnSysHelp@CSimsDialog@@IAEXXZ", 29), SB("\?OnSysHelp@CSimsDialog@@IAEXXZ", 30),
            S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 0), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 4), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 8), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 12), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 16), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 20), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 24), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 28), S4("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 32), SB("\?OnSysKeyDown@CSimsHotKey@@IAEXIII@Z", 36),
            S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 0), S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 4), S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 8), S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 12), S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 16), S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 20), S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 24), S4("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 28), SB("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 32), SB("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 33), SB("\?OnSysKeyUp@CSimsHotKey@@IAEXIII@Z", 34),
            STR36("\?OnSysKeyUp@CSimsTabCtrl@@IAEXIII@Z"),
            S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 0), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 4), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 8), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 12), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 16), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 20), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 24), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 28), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 32), S4("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 36), SB("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 40), SB("\?OnSysKeyUp@CSimsTabCtrlHelper@@IAEXIII@Z", 41),
            S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 0), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 4), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 8), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 12), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 16), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 20), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 24), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 28), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 32), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 36), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 40), S4("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 44), SB("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 48), SB("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 49), SB("\?PostMessageSimsEditKillFocus@CSimsDialog@@IAEXH@Z", 50),
            STR36("\?PostNcDestroy@CSimsTabPage@@MAEXXZ"),
            S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 0), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 4), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 8), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 12), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 16), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 20), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 24), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 28), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 32), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 36), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 40), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 44), S4("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 48), SB("\?PreTranslateMessage@CSimsTabPage@@UAEHPAUtagMSG@@@Z", 52),
            S4("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 0), S4("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 4), S4("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 8), S4("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 12), S4("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 16), S4("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 20), S4("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 24), SB("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 28), SB("\?PrevTab@CSimsTabCtrl@@QAEXXZ", 29),
            STR40("\?RemapCtrlPt@@YGXAAVCPoint@@PAVCWnd@@@Z"),
            S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 0), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 4), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 8), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 12), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 16), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 20), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 24), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 28), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 32), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 36), S4("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 40), SB("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 44), SB("\?RemapCtrlPtWth@@YGXAAVCPoint@@AAHPAVCWnd@@@Z", 45),
            S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 0), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 4), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 8), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 12), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 16), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 20), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 24), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 28), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 32), S4("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 36), SB("\?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z", 40),
            S4("\?SIMSUI_ExitCleanly@@YAXXZ", 0), S4("\?SIMSUI_ExitCleanly@@YAXXZ", 4), S4("\?SIMSUI_ExitCleanly@@YAXXZ", 8), S4("\?SIMSUI_ExitCleanly@@YAXXZ", 12), S4("\?SIMSUI_ExitCleanly@@YAXXZ", 16), S4("\?SIMSUI_ExitCleanly@@YAXXZ", 20), SB("\?SIMSUI_ExitCleanly@@YAXXZ", 24), SB("\?SIMSUI_ExitCleanly@@YAXXZ", 25), SB("\?SIMSUI_ExitCleanly@@YAXXZ", 26),
            S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 0), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 4), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 8), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 12), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 16), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 20), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 24), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 28), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 32), S4("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 36), SB("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 40), SB("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 41), SB("\?SIMSUI_ExitCleanlyE@@YAXPAVCException@@@Z", 42),
            S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 0), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 4), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 8), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 12), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 16), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 20), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 24), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 28), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 32), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 36), S4("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 40), SB("\?SafeSetFocus@CSimsButton@@QAEXPAUHWND__@@@Z", 44),
            S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 0), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 4), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 8), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 12), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 16), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 20), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 24), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 28), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 32), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 36), S4("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 40), SB("\?SafeSetFocus@CSimsDialog@@QAEXPAUHWND__@@@Z", 44),
            S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 0), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 4), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 8), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 12), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 16), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 20), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 24), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 28), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 32), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 36), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 40), S4("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 44), SB("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 48), SB("\?SafeSetFocus@CSimsRadioButton@@QAEXPAUHWND__@@@Z", 49),
            S4("\?Send@CNotify@@QAAHPAD0ZZ", 0), S4("\?Send@CNotify@@QAAHPAD0ZZ", 4), S4("\?Send@CNotify@@QAAHPAD0ZZ", 8), S4("\?Send@CNotify@@QAAHPAD0ZZ", 12), S4("\?Send@CNotify@@QAAHPAD0ZZ", 16), S4("\?Send@CNotify@@QAAHPAD0ZZ", 20), SB("\?Send@CNotify@@QAAHPAD0ZZ", 24), SB("\?Send@CNotify@@QAAHPAD0ZZ", 25),
            S4("\?Set@CNotify@@QAEXII@Z", 0), S4("\?Set@CNotify@@QAEXII@Z", 4), S4("\?Set@CNotify@@QAEXII@Z", 8), S4("\?Set@CNotify@@QAEXII@Z", 12), S4("\?Set@CNotify@@QAEXII@Z", 16), SB("\?Set@CNotify@@QAEXII@Z", 20), SB("\?Set@CNotify@@QAEXII@Z", 21), SB("\?Set@CNotify@@QAEXII@Z", 22),
            S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 0), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 4), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 8), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 12), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 16), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 20), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 24), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 28), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 32), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 36), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 40), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 44), S4("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 48), SB("\?SetActivePage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 52),
            S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 0), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 4), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 8), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 12), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 16), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 20), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 24), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 28), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 32), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 36), S4("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 40), SB("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 44), SB("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 45), SB("\?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z", 46),
            S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 0), S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 4), S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 8), S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 12), S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 16), S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 20), S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 24), S4("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 28), SB("\?SetCharLimit@CSimsEdit@@QAEXH@Z", 32),
            S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 0), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 4), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 8), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 12), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 16), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 20), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 24), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 28), S4("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 32), SB("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 36), SB("\?SetChecked@CSimsCheckButton@@QAEHH@Z", 37),
            S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 0), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 4), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 8), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 12), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 16), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 20), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 24), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 28), S4("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 32), SB("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 36), SB("\?SetChecked@CSimsRadioButton@@QAEHH@Z", 37),
            S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 0), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 4), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 8), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 12), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 16), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 20), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 24), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 28), S4("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 32), SB("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 36), SB("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 37), SB("\?SetColorTable@CDIBPal@@QAEXPAVCDC@@@Z", 38),
            STR40("\?SetEditValidateID@CSimsDialog@@IAEXH@Z"),
            STR52("\?SetHotJoy@CSimsHotJoy@@QAEXW4JOYID@@W4JOYSENSE@@@Z"),
            STR32("\?SetHotKey@CSimsHotKey@@QAEXH@Z"),
            S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 0), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 4), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 8), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 12), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 16), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 20), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 24), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 28), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 32), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 36), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 40), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 44), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 48), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 52), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 56), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 60), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 64), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 68), S4("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 72), SB("\?SetImage@CSimsPicture@@QAEXIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z", 76),
            S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 0), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 4), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 8), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 12), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 16), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 20), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 24), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 28), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 32), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 36), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 40), S4("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 44), SB("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 48), SB("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 49), SB("\?SetLastPage@CSimsTabCtrl@@QAEHPAVCSimsTabPage@@@Z", 50),
            S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 0), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 4), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 8), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 12), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 16), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 20), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 24), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 28), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 32), S4("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 36), SB("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 40), SB("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 41), SB("\?SetOkOrCancelClicked@CSimsDialog@@IAEXH@Z", 42),
            S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 0), S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 4), S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 8), S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 12), S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 16), S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 20), S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 24), S4("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 28), SB("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 32), SB("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 33), SB("\?SetScrollPos@CSimsSlider@@QAEHH@Z", 34),
            S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 0), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 4), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 8), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 12), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 16), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 20), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 24), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 28), S4("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 32), SB("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 36), SB("\?SetScrollRange@CSimsSlider@@QAEXHH@Z", 37),
            S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 0), S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 4), S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 8), S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 12), S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 16), S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 20), S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 24), S4("\?SetStaticColors@CDIBPal@@QAEHXZ", 28), SB("\?SetStaticColors@CDIBPal@@QAEHXZ", 32),
            S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 0), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 4), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 8), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 12), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 16), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 20), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 24), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 28), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 32), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 36), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 40), S4("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 44), SB("\?SetTabCtrl@CSimsDialog@@QAEXPAVCSimsTabCtrl@@@Z", 48),
            S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 0), S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 4), S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 8), S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 12), S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 16), S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 20), S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 24), S4("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 28), SB("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 32), SB("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 33), SB("\?SetTabStops@CSimsFont@@QAEHHPAH@Z", 34),
            S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 0), S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 4), S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 8), S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 12), S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 16), S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 20), S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 24), S4("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 28), SB("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 32), SB("\?SetThumbPos@CSimsSlider@@AAEXH@Z", 33),
            S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 0), S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 4), S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 8), S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 12), S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 16), S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 20), S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 24), S4("\?SetValidate@CSimsDialog@@IAEXH@Z", 28), SB("\?SetValidate@CSimsDialog@@IAEXH@Z", 32), SB("\?SetValidate@CSimsDialog@@IAEXH@Z", 33),
            S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 0), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 4), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 8), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 12), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 16), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 20), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 24), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 28), S4("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 32), SB("\?SetWindowTextA@CSimsText@@QAEXPBD@Z", 36),
            S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 0), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 4), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 8), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 12), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 16), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 20), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 24), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 28), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 32), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 36), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 40), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 44), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 48), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 52), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 56), S4("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 60), SB("\?SimsGetNearestPaletteIndex@CDIBPal@@QAEHKPAUtagPALETTEENTRY@@@Z", 64),
            S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 0), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 4), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 8), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 12), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 16), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 20), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 24), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 28), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 32), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 36), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 40), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 44), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 48), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 52), SB("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 56), SB("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 57), SB("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@AAVCPoint@@H@Z", 58),
            STR48("\?SpriteCopy@CDIBSection@@QAEXPAV1@AAVCRect@@H@Z"),
            S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 0), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 4), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 8), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 12), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 16), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 20), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 24), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 28), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 32), S4("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 36), SB("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 40), SB("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHH@Z", 41),
            STR44("\?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHHHH@Z"),
            S4("\?StorageWidth@CDIBSection@@QAEHXZ", 0), S4("\?StorageWidth@CDIBSection@@QAEHXZ", 4), S4("\?StorageWidth@CDIBSection@@QAEHXZ", 8), S4("\?StorageWidth@CDIBSection@@QAEHXZ", 12), S4("\?StorageWidth@CDIBSection@@QAEHXZ", 16), S4("\?StorageWidth@CDIBSection@@QAEHXZ", 20), S4("\?StorageWidth@CDIBSection@@QAEHXZ", 24), S4("\?StorageWidth@CDIBSection@@QAEHXZ", 28), SB("\?StorageWidth@CDIBSection@@QAEHXZ", 32), SB("\?StorageWidth@CDIBSection@@QAEHXZ", 33),
            S4("\?Tile@CDIBSection@@QAEXPAV1@@Z", 0), S4("\?Tile@CDIBSection@@QAEXPAV1@@Z", 4), S4("\?Tile@CDIBSection@@QAEXPAV1@@Z", 8), S4("\?Tile@CDIBSection@@QAEXPAV1@@Z", 12), S4("\?Tile@CDIBSection@@QAEXPAV1@@Z", 16), S4("\?Tile@CDIBSection@@QAEXPAV1@@Z", 20), S4("\?Tile@CDIBSection@@QAEXPAV1@@Z", 24), SB("\?Tile@CDIBSection@@QAEXPAV1@@Z", 28), SB("\?Tile@CDIBSection@@QAEXPAV1@@Z", 29), SB("\?Tile@CDIBSection@@QAEXPAV1@@Z", 30),
            STR40("\?ToggleChecked@CSimsCheckButton@@QAEXXZ"),
            S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 0), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 4), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 8), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 12), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 16), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 20), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 24), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 28), S4("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 32), SB("\?TrackMenu@CSims3PieceButton@@QAEXXZ", 36),
            S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 0), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 4), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 8), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 12), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 16), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 20), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 24), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 28), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 32), S4("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 36), SB("\?UndoSetChecked@CSimsRadioButton@@QAEXXZ", 40),
            S4("\?Validate@CSimsDialog@@UAEHH@Z", 0), S4("\?Validate@CSimsDialog@@UAEHH@Z", 4), S4("\?Validate@CSimsDialog@@UAEHH@Z", 8), S4("\?Validate@CSimsDialog@@UAEHH@Z", 12), S4("\?Validate@CSimsDialog@@UAEHH@Z", 16), S4("\?Validate@CSimsDialog@@UAEHH@Z", 20), S4("\?Validate@CSimsDialog@@UAEHH@Z", 24), SB("\?Validate@CSimsDialog@@UAEHH@Z", 28), SB("\?Validate@CSimsDialog@@UAEHH@Z", 29), SB("\?Validate@CSimsDialog@@UAEHH@Z", 30),
            S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 0), S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 4), S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 8), S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 12), S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 16), S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 20), S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 24), S4("\?ValidateData@CSimsDialog@@UAEHH@Z", 28), SB("\?ValidateData@CSimsDialog@@UAEHH@Z", 32), SB("\?ValidateData@CSimsDialog@@UAEHH@Z", 33), SB("\?ValidateData@CSimsDialog@@UAEHH@Z", 34),
            S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 0), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 4), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 8), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 12), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 16), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 20), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 24), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 28), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 32), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 36), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 40), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 44), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 48), S4("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 52), SB("\?_GetBaseClass@CSims3PieceButton@@KGPAUCRuntimeClass@@XZ", 56),
            S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 0), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 4), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 8), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 12), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 16), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 20), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 24), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 28), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 32), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 36), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 40), S4("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 44), SB("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 48), SB("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 49), SB("\?_GetBaseClass@CSimsDialog@@KGPAUCRuntimeClass@@XZ", 50),
            STR52("\?_GetBaseClass@CSimsTabPage@@KGPAUCRuntimeClass@@XZ"),
            S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 56), SB("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 57), SB("\?_GetBaseMessageMap@CSims3PieceButton@@KGPBUAFX_MSGMAP@@XZ", 58),
            S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsButton@@KGPBUAFX_MSGMAP@@XZ", 52),
            S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 56), SB("\?_GetBaseMessageMap@CSimsCheckButton@@KGPBUAFX_MSGMAP@@XZ", 57),
            S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 56), SB("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 57), SB("\?_GetBaseMessageMap@CSimsCheckListBox@@KGPBUAFX_MSGMAP@@XZ", 58),
            S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 53), SB("\?_GetBaseMessageMap@CSimsComboBox@@KGPBUAFX_MSGMAP@@XZ", 54),
            S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsDialog@@KGPBUAFX_MSGMAP@@XZ", 52),
            S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 44), SB("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 49), SB("\?_GetBaseMessageMap@CSimsEdit@@KGPBUAFX_MSGMAP@@XZ", 50),
            S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsExitButton@@KGPBUAFX_MSGMAP@@XZ", 56),
            S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 53), SB("\?_GetBaseMessageMap@CSimsGroupbox@@KGPBUAFX_MSGMAP@@XZ", 54),
            S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsHelpButton@@KGPBUAFX_MSGMAP@@XZ", 56),
            S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsHotJoy@@KGPBUAFX_MSGMAP@@XZ", 52),
            S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsHotKey@@KGPBUAFX_MSGMAP@@XZ", 52),
            S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsListBox@@KGPBUAFX_MSGMAP@@XZ", 53),
            S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsPicture@@KGPBUAFX_MSGMAP@@XZ", 53),
            S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 56), SB("\?_GetBaseMessageMap@CSimsPictureView@@KGPBUAFX_MSGMAP@@XZ", 57),
            S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 56), SB("\?_GetBaseMessageMap@CSimsRadioButton@@KGPBUAFX_MSGMAP@@XZ", 57),
            STR56("\?_GetBaseMessageMap@CSimsScrollBar@@KGPBUAFX_MSGMAP@@XZ"),
            S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 56), SB("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 57), SB("\?_GetBaseMessageMap@CSimsShadowButton@@KGPBUAFX_MSGMAP@@XZ", 58),
            S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsSlider@@KGPBUAFX_MSGMAP@@XZ", 52),
            S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 48), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 52), S4("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 56), SB("\?_GetBaseMessageMap@CSimsSpinButtonCtrl@@KGPBUAFX_MSGMAP@@XZ", 60),
            S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsStatic@@KGPBUAFX_MSGMAP@@XZ", 52),
            S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsTabCtrl@@KGPBUAFX_MSGMAP@@XZ", 53),
            STR60("\?_GetBaseMessageMap@CSimsTabCtrlHelper@@KGPBUAFX_MSGMAP@@XZ"),
            S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsTabPage@@KGPBUAFX_MSGMAP@@XZ", 53),
            S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 44), SB("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 49), SB("\?_GetBaseMessageMap@CSimsText@@KGPBUAFX_MSGMAP@@XZ", 50),
            S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 0), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 4), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 8), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 12), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 16), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 20), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 24), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 28), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 32), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 36), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 40), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 44), S4("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 48), SB("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 52), SB("\?_GetBaseMessageMap@CSimsTextArt@@KGPBUAFX_MSGMAP@@XZ", 53),
            STR60("\?_messageEntries@CSims3PieceButton@@0PBUAFX_MSGMAP_ENTRY@@B"),
            S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsButton@@0PBUAFX_MSGMAP_ENTRY@@B", 53),
            S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 56), SB("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 57), SB("\?_messageEntries@CSimsCheckButton@@0PBUAFX_MSGMAP_ENTRY@@B", 58),
            STR60("\?_messageEntries@CSimsCheckListBox@@0PBUAFX_MSGMAP_ENTRY@@B"),
            STR56("\?_messageEntries@CSimsComboBox@@0PBUAFX_MSGMAP_ENTRY@@B"),
            S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsDialog@@0PBUAFX_MSGMAP_ENTRY@@B", 53),
            STR52("\?_messageEntries@CSimsEdit@@0PBUAFX_MSGMAP_ENTRY@@B"),
            S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 56), SB("\?_messageEntries@CSimsExitButton@@0PBUAFX_MSGMAP_ENTRY@@B", 57),
            STR56("\?_messageEntries@CSimsGroupbox@@0PBUAFX_MSGMAP_ENTRY@@B"),
            S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 56), SB("\?_messageEntries@CSimsHelpButton@@0PBUAFX_MSGMAP_ENTRY@@B", 57),
            S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsHotJoy@@0PBUAFX_MSGMAP_ENTRY@@B", 53),
            S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsHotKey@@0PBUAFX_MSGMAP_ENTRY@@B", 53),
            S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 53), SB("\?_messageEntries@CSimsListBox@@0PBUAFX_MSGMAP_ENTRY@@B", 54),
            S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 53), SB("\?_messageEntries@CSimsPicture@@0PBUAFX_MSGMAP_ENTRY@@B", 54),
            S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 56), SB("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 57), SB("\?_messageEntries@CSimsPictureView@@0PBUAFX_MSGMAP_ENTRY@@B", 58),
            S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 56), SB("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 57), SB("\?_messageEntries@CSimsRadioButton@@0PBUAFX_MSGMAP_ENTRY@@B", 58),
            S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsScrollBar@@0PBUAFX_MSGMAP_ENTRY@@B", 56),
            STR60("\?_messageEntries@CSimsShadowButton@@0PBUAFX_MSGMAP_ENTRY@@B"),
            S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsSlider@@0PBUAFX_MSGMAP_ENTRY@@B", 53),
            S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 52), S4("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 56), SB("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 60), SB("\?_messageEntries@CSimsSpinButtonCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 61),
            S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsStatic@@0PBUAFX_MSGMAP_ENTRY@@B", 53),
            S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 53), SB("\?_messageEntries@CSimsTabCtrl@@0PBUAFX_MSGMAP_ENTRY@@B", 54),
            S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 48), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 52), S4("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 56), SB("\?_messageEntries@CSimsTabCtrlHelper@@0PBUAFX_MSGMAP_ENTRY@@B", 60),
            S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 53), SB("\?_messageEntries@CSimsTabPage@@0PBUAFX_MSGMAP_ENTRY@@B", 54),
            STR52("\?_messageEntries@CSimsText@@0PBUAFX_MSGMAP_ENTRY@@B"),
            S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 0), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 4), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 8), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 12), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 16), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 20), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 24), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 28), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 32), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 36), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 40), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 44), S4("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 48), SB("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 52), SB("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 53), SB("\?_messageEntries@CSimsTextArt@@0PBUAFX_MSGMAP_ENTRY@@B", 54),
            S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 0), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 4), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 8), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 12), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 16), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 20), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 24), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 28), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 32), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 36), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 40), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 44), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 48), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 52), S4("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 56), SB("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 60), SB("\?classCSims3PieceButton@CSims3PieceButton@@2UCRuntimeClass@@B", 61),
            S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 0), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 4), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 8), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 12), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 16), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 20), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 24), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 28), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 32), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 36), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 40), S4("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 44), SB("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 48), SB("\?classCSimsDialog@CSimsDialog@@2UCRuntimeClass@@B", 49),
            STR52("\?classCSimsTabPage@CSimsTabPage@@2UCRuntimeClass@@B"),
            STR48("\?m_hInstMainDLL@CSimsDialog@@2PAUHINSTANCE__@@A"),
            S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 44), SB("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 45), SB("\?messageMap@CSims3PieceButton@@1UAFX_MSGMAP@@B", 46),
            S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsButton@@1UAFX_MSGMAP@@B", 40),
            S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 44), SB("\?messageMap@CSimsCheckButton@@1UAFX_MSGMAP@@B", 45),
            S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 44), SB("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 45), SB("\?messageMap@CSimsCheckListBox@@1UAFX_MSGMAP@@B", 46),
            S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 41), SB("\?messageMap@CSimsComboBox@@1UAFX_MSGMAP@@B", 42),
            S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B", 40),
            S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 32), SB("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 37), SB("\?messageMap@CSimsEdit@@1UAFX_MSGMAP@@B", 38),
            S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsExitButton@@1UAFX_MSGMAP@@B", 44),
            S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 41), SB("\?messageMap@CSimsGroupbox@@1UAFX_MSGMAP@@B", 42),
            S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsHelpButton@@1UAFX_MSGMAP@@B", 44),
            S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsHotJoy@@1UAFX_MSGMAP@@B", 40),
            S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsHotKey@@1UAFX_MSGMAP@@B", 40),
            S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsListBox@@1UAFX_MSGMAP@@B", 41),
            S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsPicture@@1UAFX_MSGMAP@@B", 41),
            S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 44), SB("\?messageMap@CSimsPictureView@@1UAFX_MSGMAP@@B", 45),
            S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 44), SB("\?messageMap@CSimsRadioButton@@1UAFX_MSGMAP@@B", 45),
            STR44("\?messageMap@CSimsScrollBar@@1UAFX_MSGMAP@@B"),
            S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 44), SB("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 45), SB("\?messageMap@CSimsShadowButton@@1UAFX_MSGMAP@@B", 46),
            S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsSlider@@1UAFX_MSGMAP@@B", 40),
            S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 36), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 40), S4("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 44), SB("\?messageMap@CSimsSpinButtonCtrl@@1UAFX_MSGMAP@@B", 48),
            S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsStatic@@1UAFX_MSGMAP@@B", 40),
            S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsTabCtrl@@1UAFX_MSGMAP@@B", 41),
            STR48("\?messageMap@CSimsTabCtrlHelper@@1UAFX_MSGMAP@@B"),
            S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsTabPage@@1UAFX_MSGMAP@@B", 41),
            S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 32), SB("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 37), SB("\?messageMap@CSimsText@@1UAFX_MSGMAP@@B", 38),
            S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 0), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 4), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 8), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 12), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 16), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 20), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 24), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 28), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 32), S4("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 36), SB("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 40), SB("\?messageMap@CSimsTextArt@@1UAFX_MSGMAP@@B", 41),
            S4("\?ms_ObjectCount@CSimsButton@@1HA", 0), S4("\?ms_ObjectCount@CSimsButton@@1HA", 4), S4("\?ms_ObjectCount@CSimsButton@@1HA", 8), S4("\?ms_ObjectCount@CSimsButton@@1HA", 12), S4("\?ms_ObjectCount@CSimsButton@@1HA", 16), S4("\?ms_ObjectCount@CSimsButton@@1HA", 20), S4("\?ms_ObjectCount@CSimsButton@@1HA", 24), S4("\?ms_ObjectCount@CSimsButton@@1HA", 28), SB("\?ms_ObjectCount@CSimsButton@@1HA", 32),
            S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 0), S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 4), S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 8), S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 12), S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 16), S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 20), S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 24), S4("\?ms_cbShadow@CDIBSection@@2PAY05EA", 28), SB("\?ms_cbShadow@CDIBSection@@2PAY05EA", 32), SB("\?ms_cbShadow@CDIBSection@@2PAY05EA", 33), SB("\?ms_cbShadow@CDIBSection@@2PAY05EA", 34),
            S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 0), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 4), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 8), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 12), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 16), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 20), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 24), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 28), S4("\?ms_nShadowFactor@CDIBSection@@2PAHA", 32), SB("\?ms_nShadowFactor@CDIBSection@@2PAHA", 36),
            S4("\?ui_delete@@YAXPAX@Z", 0), S4("\?ui_delete@@YAXPAX@Z", 4), S4("\?ui_delete@@YAXPAX@Z", 8), S4("\?ui_delete@@YAXPAX@Z", 12), S4("\?ui_delete@@YAXPAX@Z", 16), SB("\?ui_delete@@YAXPAX@Z", 20),
            S4("\?ui_new@@YAPAXI@Z", 0), S4("\?ui_new@@YAPAXI@Z", 4), S4("\?ui_new@@YAPAXI@Z", 8), S4("\?ui_new@@YAPAXI@Z", 12), SB("\?ui_new@@YAPAXI@Z", 16), SB("\?ui_new@@YAPAXI@Z", 17),
            S4("\?ui_new_db@@YAPAXIPBDH@Z", 0), S4("\?ui_new_db@@YAPAXIPBDH@Z", 4), S4("\?ui_new_db@@YAPAXIPBDH@Z", 8), S4("\?ui_new_db@@YAPAXIPBDH@Z", 12), S4("\?ui_new_db@@YAPAXIPBDH@Z", 16), S4("\?ui_new_db@@YAPAXIPBDH@Z", 20), SB("\?ui_new_db@@YAPAXIPBDH@Z", 24),
            STR12("ImportTable"),
            STR8("Linkage"),
/* RVA 0x20000  the CRT's initialiser table, INITIALISERS in simsui_native.c (_initterm runs it from 0x20000 to INITIALISERS_END 0x2000c; the first word is zero here, so nothing is written for it) */
AT(0x20004) U32(0x204716b0),  /* retail VA, becomes fs98_simsui_fonts_static */
            U32(0x20476ac0),  /* retail VA, becomes fs98_simsui_cdib_register */
/* RVA 0x20020  RAW_DLL_MAIN in simsui_native.c: MFC's raw DllMain, run by the CRT's DLL initialisation */
AT(0x20020) U32(0x20471670),  /* retail VA, becomes fs98_simsui_raw_dll_main */
            U32(0x00000000),
/* RVA 0x20028  ImportTable */
AT(0x20028) U32(0x00000001), U32(0x00000000), U32(0x00000002), U32(0x00000000),
            U32(0x00000000), U32(0x00000000),
/* RVA 0x20040  Linkage */
AT(0x20040) U32(0x80000000),
            U32(0x204716a0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x204716a0),  /* retail VA, becomes fs98_simsui_nothing */
            U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x0000000a),
            STR28("Error Loading Font String"),
            STR20("CSims3PieceButton"),
            STR12("CSimsDialog"),
            U32(0x00002711), U32(0x00000002), U32(0x00000001), U32(0x000a202e),
            U32(0x00000026),
            STR12("simsui.dll"),
            U32(0x204900d0),  /* retail VA, image, RVA 0x200d0 */
            U32(0x00000010), U32(0x00000000),
            U32(0x20476a40),  /* retail VA, becomes fs98_simsui_cdib_create_object */
            U32(0x20476aa0),  /* retail VA, becomes fs98_simsui_cdib_base_class */
            U32(0x00000000),
            STR8("CDIB"),
            U32(0xfc04fe01), U32(0xf810fa09), U32(0xf424f619), U32(0xf040f231),
            U32(0xec64ee51), U32(0xe890ea79), U32(0xe4c4e6a9), U32(0xe100e2e1),
            U32(0xdd44df21), U32(0xd990db69), U32(0xd5e4d7b9), U32(0xd240d411),
            U32(0xcea4d071), U32(0xcb10ccd9), U32(0xc784c949), U32(0xc400c5c1),
            U32(0xc084c241), U32(0xbd10bec9), U32(0xb9a4bb59), U32(0xb640b7f1),
            U32(0xb2e4b491), U32(0xaf90b139), U32(0xac44ade9), U32(0xa900aaa1),
            U32(0xa5c4a761), U32(0xa290a429), U32(0x9f64a0f9), U32(0x9c409dd1),
            U32(0x99249ab1), U32(0x96109799), U32(0x93049489), U32(0x90009181),
            U32(0x8d048e81), U32(0x8a108b89), U32(0x87248899), U32(0x844085b1),
            U32(0x816482d1), U32(0x7e907ff9), U32(0x7bc47d29), U32(0x79007a61),
            U32(0x764477a1), U32(0x739074e9), U32(0x70e47239), U32(0x6e406f91),
            U32(0x6ba46cf1), U32(0x69106a59), U32(0x668467c9), U32(0x64006541),
            U32(0x618462c1), U32(0x5f106049), U32(0x5ca45dd9), U32(0x5a405b71),
            U32(0x57e45911), U16(0x56b9), U8(0x90),
            U32(0x44546955), U32(0x00522153),
            U8(0x51), U32(0x4ec44fe1), U32(0x4c904da9), U32(0x4a644b79),
            U32(0x48404951), U32(0x46244731), U32(0x44104519), U32(0x42044309),
            U32(0x40004101), U32(0x3e043f01), U32(0x3c103d09), U32(0x3a243b19),
            U32(0x38403931), U32(0x36643751), U32(0x34903579), U32(0x32c433a9),
            U32(0x310031e1), U32(0x2f443021), U32(0x2d902e69), U32(0x2be42cb9),
            U32(0x2a402b11), U32(0x28a42971), U32(0x271027d9), U32(0x25842649),
            U32(0x240024c1), U32(0x22842341), U32(0x211021c9), U32(0x1fa42059),
            U32(0x1e401ef1), U32(0x1ce41d91), U32(0x1b901c39), U32(0x1a441ae9),
            U32(0x190019a1), U32(0x17c41861), U32(0x16901729), U32(0x156415f9),
            U32(0x144014d1), U32(0x132413b1), U32(0x12101299), U32(0x11041189),
            U32(0x10001081), U32(0x0f040f81), U32(0x0e100e89), U32(0x0d240d99),
            U32(0x0c400cb1), U32(0x0b640bd1), U32(0x0a900af9), U32(0x09c40a29),
            U32(0x09000961), U32(0x084408a1), U32(0x079007e9), U32(0x06e40739),
            U32(0x06400691), U32(0x05a405f1), U32(0x05100559), U32(0x048404c9),
            U32(0x04000441), U32(0x038403c1), U32(0x03100349), U32(0x02a402d9),
            U32(0x02400271), U32(0x01e40211), U32(0x019001b9), U32(0x01440169),
            U32(0x01000121), U32(0x00c400e1), U32(0x009000a9), U32(0x00640079),
            U32(0x00400051), U32(0x00240031), U32(0x00100019), U32(0x00040009),
            U32(0x00000001), U32(0x00040001), U32(0x00100009), U32(0x00240019),
            U32(0x00400031), U32(0x00640051), U32(0x00900079), U32(0x00c400a9),
            U32(0x010000e1), U32(0x01440121), U32(0x01900169), U32(0x01e401b9),
            U32(0x02400211), U32(0x02a40271), U32(0x031002d9), U32(0x03840349),
            U32(0x040003c1), U32(0x04840441), U32(0x051004c9), U32(0x05a40559),
            U32(0x064005f1), U32(0x06e40691), U32(0x07900739), U32(0x084407e9),
            U32(0x090008a1), U32(0x09c40961), U32(0x0a900a29), U32(0x0b640af9),
            U32(0x0c400bd1), U32(0x0d240cb1), U32(0x0e100d99), U32(0x0f040e89),
            U32(0x10000f81), U32(0x11041081), U32(0x12101189), U32(0x13241299),
            U32(0x144013b1), U32(0x156414d1), U32(0x169015f9), U32(0x17c41729),
            U32(0x19001861), U32(0x1a4419a1), U32(0x1b901ae9), U32(0x1ce41c39),
            U32(0x1e401d91), U32(0x1fa41ef1), U32(0x21102059), U32(0x228421c9),
            U32(0x24002341), U32(0x258424c1), U32(0x27102649), U32(0x28a427d9),
            U32(0x2a402971), U32(0x2be42b11), U16(0x2cb9), U8(0x90),
            U32(0x442e692d), U32(0x0030212f),
            U8(0x31), U32(0x32c431e1), U32(0x349033a9), U32(0x36643579),
            U32(0x38403751), U32(0x3a243931), U32(0x3c103b19), U32(0x3e043d09),
            U32(0x40003f01), U32(0x42044101), U32(0x44104309), U32(0x46244519),
            U32(0x48404731), U32(0x4a644951), U32(0x4c904b79), U32(0x4ec44da9),
            U32(0x51004fe1), U32(0x53445221), U32(0x55905469), U32(0x57e456b9),
            U32(0x5a405911), U32(0x5ca45b71), U32(0x5f105dd9), U32(0x61846049),
            U32(0x640062c1), U32(0x66846541), U32(0x691067c9), U32(0x6ba46a59),
            U32(0x6e406cf1), U32(0x70e46f91), U32(0x73907239), U32(0x764474e9),
            U32(0x790077a1), U32(0x7bc47a61), U32(0x7e907d29), U32(0x81647ff9),
            U32(0x844082d1), U32(0x872485b1), U32(0x8a108899), U32(0x8d048b89),
            U32(0x90008e81), U32(0x93049181), U32(0x96109489), U32(0x99249799),
            U32(0x9c409ab1), U32(0x9f649dd1), U32(0xa290a0f9), U32(0xa5c4a429),
            U32(0xa900a761), U32(0xac44aaa1), U32(0xaf90ade9), U32(0xb2e4b139),
            U32(0xb640b491), U32(0xb9a4b7f1), U32(0xbd10bb59), U32(0xc084bec9),
            U32(0xc400c241), U32(0xc784c5c1), U32(0xcb10c949), U32(0xcea4ccd9),
            U32(0xd240d071), U32(0xd5e4d411), U32(0xd990d7b9), U32(0xdd44db69),
            U32(0xe100df21), U32(0xe4c4e2e1), U32(0xe890e6a9), U32(0xec64ea79),
            U32(0xf040ee51), U32(0xf424f231), U32(0xf810f619), U32(0xfc04fa09),
            U32(0x0000fe01),
/* RVA 0x204d8  ?ms_nShadowFactor@CDIBSection@@2PAHA */
AT(0x204d8) U32(0x00000055), U32(0x00000046), U32(0x0000003c), U32(0x00000032),
            U32(0x00000028), U32(0x00000023),
/* RVA 0x204f0  ?ms_cbShadow@CDIBSection@@2PAY05EA */
AT(0x204f0) U32(0x0a0a0a0a), U32(0x7fcd0a0a), U32(0x0c7f7f7f), U32(0x585959dd),
            U32(0x8f90dada), U32(0x32323333), U32(0xe7e7e8e9), U32(0x72730ce7),
            U32(0x0d3e7272), U32(0x252626a9), U32(0x171a2425), U32(0x11121416),
            U32(0x10111214), U32(0x16180e0f), U32(0x10111214), U32(0x0a0a0a0a),
            U32(0x0b0b0a0a), U32(0x0a0a0a0b), U32(0x0b0b0b0c), U32(0x0c0c0b0b),
            U32(0x0b0b0b0c), U32(0x0c0c0d0d), U32(0x0d0e0b0c), U32(0x0c0c0c0d),
            U32(0x0d0e0e0f), U32(0x0f100c0c), U32(0x0c0d0d0e), U32(0x0e0f1011),
            U32(0x10110d0d), U32(0x0d0e0e0f), U32(0x0f101113), U32(0x12130e0e),
            U32(0x0e0e0f11), U32(0x10111214), U32(0x13150e0f), U32(0x0f0f1112),
            U32(0x11121416), U32(0x15170f10), U32(0x0f101213), U32(0x12141517),
            U32(0x16181010), U32(0x10111314), U32(0x13151619), U32(0x171a1011),
            U32(0x11121416), U32(0x1416181b), U32(0x191c1112), U32(0x11131517),
            U32(0x1517191d), U32(0x1a1e1213), U32(0x12131618), U32(0x16181b1e),
            U32(0x1b1f1214), U32(0x13141619), U32(0x0b0c2424), U32(0x24240b0b),
            U32(0x0b0c2424), U32(0x24242525), U32(0x26260c24), U32(0x24242425),
            U32(0x25262627), U32(0x27282424), U32(0x0e0e2626), U32(0x10272829),
            U32(0x292a0e0f), U32(0x0f101128), U32(0x1228292b), U32(0x2a2b1010),
            U32(0x10112829), U32(0x142a2b2c), U32(0x2b2d1112), U32(0x1113292a),
            U32(0x152b2c2e), U32(0x31311213), U32(0x0b0b0b31), U32(0x3131318c),
            U32(0x32320b0b), U32(0x31313131), U32(0x32323233), U32(0x33343131),
            U32(0x31313233), U32(0x33343435), U32(0x35363232), U32(0x0e0f1034),
            U32(0x34353637), U32(0x36380f0f), U32(0x0f103536), U32(0x12363739),
            U32(0x38391011), U32(0x11123637), U32(0x1437393a), U32(0x393b1112),
            U32(0x12131538), U32(0x0c3e3e3e), U32(0x3e3f0b0c), U32(0x0c0c0d3e),
            U32(0x0e3e3f3f), U32(0x3f400d0d), U32(0x0d0e0e3f), U32(0x3f404041),
            U32(0x41420e0e), U32(0x0e0f1040), U32(0x40414243), U32(0x42440f10),
            U32(0x10101241), U32(0x13424344), U32(0x44451011), U32(0x11121443),
            U32(0x15434446), U32(0x45471113), U32(0x12131517), U32(0x16181a1e),
            U32(0xe7e71214), U32(0x0c0c0ce7), U32(0xe7e74b4b), U32(0x4c4c0c0d),
            U32(0x0d0d0e4b), U32(0x0f4c4c4d), U32(0x4d4e0e0e), U32(0x0e0f104c),
            U32(0x114d4e4f), U32(0x4e500f0f), U32(0x0f10124e), U32(0x4d4e4f50),
            U32(0x50511011), U32(0x1112134f), U32(0x144f5052), U32(0x51531112),
            U32(0x12131550), U32(0x15181a1d), U32(0x1b1e1213), U32(0x12141618),
            U32(0xdadadada), U32(0xda580b0b), U32(0xdadadada), U32(0xda585859),
            U32(0x5a5adada), U32(0xdada5858), U32(0x8c8d5b5b), U32(0x5b5c8c8c),
            U32(0x8c8c8d5b), U32(0x8d5c5c5d), U32(0x5d5e8c8d), U32(0x0e0f105c),
            U32(0x115d5d5f), U32(0x5e600f10), U32(0x1010125d), U32(0x5d5e5f60),
            U32(0x60611011), U32(0x1112145f), U32(0x14166062), U32(0xe7651112),
            U32(0x0c0c0ce7), U32(0x24656565), U32(0x65660c0c), U32(0x0d246565),
            U32(0x65656667), U32(0x67680d0e), U32(0x0e0e6566), U32(0x66676869),
            U32(0x696a0e0f), U32(0x0f101168), U32(0x4d69696b), U32(0x6a6b1011),
            U32(0x10114d69), U32(0x4e696b6c), U32(0x6b6d1112), U32(0x1112296a),
            U32(0x152a2c6e), U32(0x2c2e1213), U32(0x1213162b), U32(0x0de77272),
            U32(0x72720c0c), U32(0x0d0d0e72), U32(0x72727273), U32(0x73740d0e),
            U32(0x0e0e7272), U32(0x72737475), U32(0x75760e0f), U32(0x0f107274),
            U32(0x73747577), U32(0x76781010), U32(0x10111375), U32(0x14767778),
            U32(0x77791112), U32(0x11131576), U32(0x1577787a), U32(0x457a1213),
            U32(0x12131644), U32(0x16181a48), U32(0x7f7f1214), U32(0x0b0b0c7f),
            U32(0x317f7fc0), U32(0xc0c10c0c), U32(0x3131c0c0), U32(0xc0c0c1c2),
            U32(0xc28231c0), U32(0x31c0c0c1), U32(0xc1c28283), U32(0x8384c0c0),
            U32(0x0ec0c182), U32(0x34838485), U32(0x84860f10), U32(0x10103583),
            U32(0x13848586), U32(0x86871011), U32(0x11123685), U32(0x14378688),
            U32(0x393b1112), U32(0x12131538), U32(0x31313131), U32(0x8c320b0b),
            U32(0x31313131), U32(0x31323233), U32(0x338e3131), U32(0x31313232),
            U32(0x32338e8f), U32(0x8f903132), U32(0x3232338e), U32(0x338f8f91),
            U32(0x90923232), U32(0x3233348f), U32(0x34353693), U32(0x36943333),
            U32(0x33343536), U32(0x35363795), U32(0x38953434), U32(0x34353637),
            U32(0x36373896), U32(0x3e993435), U32(0x0c3e3e3e), U32(0x3e3e3e99),
            U32(0x3f9a3e3e), U32(0x3e3e3e3f), U32(0x3f3f409b), U32(0x419c3e3e),
            U32(0x3e3f3f40), U32(0x4041419d), U32(0x429e3f3f), U32(0x3f3f4041),
            U32(0x4142439e), U32(0x439f3f40), U32(0x40404142), U32(0x41434445),
            U32(0x44464040), U32(0x11124243), U32(0x42434546), U32(0x45471213),
            U32(0x12131544), U32(0xda2424a6), U32(0xa6a60bda), U32(0x24242424),
            U32(0x242525a7), U32(0xa7a82424), U32(0x24242525), U32(0x2526a8a9),
            U32(0xa9aa2424), U32(0x25252626), U32(0x2627a9ab), U32(0x28ac2525),
            U32(0x25262727), U32(0x272829ad), U32(0x2aad2626), U32(0x26272829),
            U32(0x28292aae), U32(0x2baf2727), U32(0x2727292a), U32(0x292a2b2d),
            U32(0xe9ea2728), U32(0xe7e7e7e8), U32(0x4b4beab3), U32(0x4cb4e7e7),
            U32(0xe7e74b4c), U32(0x4c4c4db5), U32(0x4eb6e74b), U32(0x4b4b4c4d),
            U32(0x4d4e4eb7), U32(0x4fb74b4c), U32(0x4c4c4d4e), U32(0x4d4f4f51),
            U32(0x50524c4c), U32(0x114d4e4f), U32(0x4e4f5052), U32(0x51531112),
            U32(0x12131550), U32(0x15171a53), U32(0x1a1e1213), U32(0x12141618),
            U32(0x0c317fc0), U32(0xc0c00b0c), U32(0x0c3131c0), U32(0xc0c0c0c1),
            U32(0xc1c23131), U32(0x3132c0c1), U32(0xc0c1c2c3), U32(0xc3c43232),
            U32(0x32c0c1c2), U32(0xc2c2c3c5), U32(0xc4c532c1), U32(0x33c1c2c3),
            U32(0x8283c5c7), U32(0x84c83334), U32(0x34343583), U32(0x838485c8),
            U32(0x86c93434), U32(0x34353637), U32(0x36373896), U32(0x7fcd3435),
            U32(0x0c7f7f7f), U32(0x7f7fcdcd), U32(0xcdce7f7f), U32(0x7f7f7f7f),
            U32(0x7f8081cf), U32(0x82d07f7f), U32(0x7fc08081), U32(0x818282d1),
            U32(0x83d2c0c0), U32(0xc03f8282), U32(0x828384d3), U32(0x84d33f3f),
            U32(0x3f404184), U32(0x41848586), U32(0x85873f40), U32(0x11404142),
            U32(0x42434487), U32(0x44881112), U32(0x11121443), U32(0x0bdadada),
            U32(0xdada0b0b), U32(0x0b0bdada), U32(0xdadada58), U32(0x5859dada),
            U32(0xdadada58), U32(0xda5859dd), U32(0x59dedada), U32(0xdada5859),
            U32(0x5859dddf), U32(0xdedfda58), U32(0xda5859dd), U32(0x59dedfe0),
            U32(0xdfe15858), U32(0x5859ddde), U32(0xdedfe0e2), U32(0xe1e35859),
            U32(0x59dddedf), U32(0xdfe0e2e4), U32(0xe7e759dd), U32(0x0a0b0ce7),
            U32(0x0ce7e7e7), U32(0xe7e80a0c), U32(0x0c0ce7e7), U32(0xe7e7e8e9),
            U32(0xe8ea0ce7), U32(0x0ce7e7e7), U32(0xe7e8e9eb), U32(0xeaece7e7),
            U32(0xe7e76565), U32(0x656566ed), U32(0x67eee7e7), U32(0xe7656566),
            U32(0x656667ee), U32(0x67f06565), U32(0x65656667), U32(0x666768f0),
            U32(0xf0f26565), U32(0x65666768), U32(0x6768f0f2), U32(0xf0f26566),
            U32(0x66666768), U32(0x1517191c), U32(0x15181213), U32(0x10111214),
            U32(0x11121315), U32(0xcfd00f0f), U32(0x7fcdcdce), U32(0xdfe0e1e4),
            U32(0x93c859dd), U32(0x8f909192), U32(0xeab3b3b4), U32(0x7677e8e9),
            U32(0x72737475), U32(0xaaabacf5), U32(0x1c202627), U32(0x13141719),
            U32(0x00002d2b),
            STR16("CSimsTabPage"),
            U32(0x00000000), U32(0x00000023), U32(0x00000023), U32(0x00000023),
            U32(0x00000022), U32(0x00000022), U32(0x00000021), U32(0x00000021),
            U32(0x00000021), U32(0x00000020), U32(0x00000020), U32(0x0000001f),
            U32(0x0000001f), U32(0x0000001e), U32(0x0000001e), U32(0x0000001e),
            U32(0x0000001d), U32(0x0000001d), U32(0x0000001c), U32(0x0000001c),
            U32(0x0000001c), U32(0x0000001b), U32(0x0000001b), U32(0x0000001a),
            U32(0x0000001a), U32(0x00000019), U32(0x00000019), U32(0x00000019),
            U32(0x00000018), U32(0x00000018), U32(0x00000017), U32(0x00000020),
/* RVA 0x213cc  ?ms_ObjectCount@CSimsButton@@1HA (all zero here, so no value is written) */
/* RVA 0x213d0  ?m_hInstMainDLL@CSimsDialog@@2PAUHINSTANCE__@@A (all zero here, so no value is written) */

#undef AT
#undef U8
#undef U16
#undef U32
#undef NUL_PAD
#undef SB
#undef S4
#undef STR8
#undef STR12
#undef STR16
#undef STR20
#undef STR24
#undef STR28
#undef STR32
#undef STR36
#undef STR40
#undef STR44
#undef STR48
#undef STR52
#undef STR56
#undef STR60
#undef STR64
#undef STR72
#undef STR76
#undef STR104
