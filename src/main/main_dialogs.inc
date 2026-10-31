/* MAIN.DLL: the menu commands' dialogs, a batch at a time (docs/modules/main.md).
 *
 * The usual shape: UTIL +0x90 fills a descriptor in .data from GLOBAL through
 * an exchange table, HOST +0x10c runs the dialog over it (an English\ DLL,
 * which calls back through the descriptor's +4), and when the host answers 0
 * and the descriptor's answer at +0 is 1, UTIL +0x8c writes it back.  The
 * callback validates: mode 1 to UTIL +0x8c.
 *
 * The descriptors' callbacks are installed over retail's code addresses by
 * main_install. */

#define UTIL_PUT    CALL(UTIL, 0x8c, f4)   /* (table, n, records, mode) */
#define UTIL_GET    CALL(UTIL, 0x90, f3)   /* (table, n, records) */
#define M24         M32(V(0x2080002c))

/* ---- slice 26: 0x1309 and 0x1308, two plain settings dialogs ------------------- */

/* .text+0x2ec0: dialog 0x1305's validate */
static uint32_t FS98_STDCALL dialog_1305_check(uint32_t records)
{
    return UTIL_PUT(V(0x208006b8), 5, records, 1);
}

/* .text+0x2e30, command 0x1309, with +0x2e70 and +0x2ea0 */
static void command_1309(void)
{
    M32(V(0x20800688)) = 0;
    UTIL_GET(V(0x208006b8), 5, V(0x20800688));
    if (H_DIALOG(0x1305, V(0x20800688)) == 0 && M32(V(0x20800688)) == 1)
        UTIL_PUT(V(0x208006b8), 5, V(0x20800688), 0);
}

/* .text+0x4ff0: dialog 0x1304's validate */
static uint32_t FS98_STDCALL dialog_1304_check(uint32_t records)
{
    return UTIL_PUT(V(0x20801260), 4, records, 1);
}

/* .text+0x4f50, command 0x1308, with +0x4f90 and +0x4fc0 */
static void command_1308(void)
{
    M32(V(0x20801238)) = 0;
    UTIL_GET(V(0x20801260), 4, V(0x20801238));
    if (H_DIALOG(0x1304, V(0x20801238)) == 0 && M32(V(0x20801238)) == 1)
        UTIL_PUT(V(0x20801260), 4, V(0x20801238), 0);
}

/* ---- 0x1607, a dialog with a child ---------------------------------------------- */

/* .text+0x10e0 */
static uint32_t FS98_STDCALL dialog_1607_nothing(uint32_t a)
{
    (void)a;
    return 0;
}

/* .text+0x10f0: its child dialog */
static uint32_t FS98_STDCALL dialog_1607_child(uint32_t block)
{
    H_DIALOG(0x16070001, block);
    return 0;
}

/* .text+0xa0, command 0x1607.  The block's +8 and +0xc are left as the stack
 * had them; the dialog does not read them. */
static void command_1607(void)
{
    uint32_t block[5];

    block[0] = 0;
    block[1] = ADDR(dialog_1607_nothing);
    block[2] = 0;
    block[3] = 0;
    block[4] = ADDR(dialog_1607_child);
    H_DIALOG(0x1607, ADDR(block));
}

/* ---- 0x1203 ----------------------------------------------------------------------- */

/* .text+0x910: validate.  The settings, then, when +0x2c, +0x24 and +0 are 1 and
 * +0x34 is set, +0x30 and +0x20 raised; then the text field at +0xc: a value
 * below 0x1800 sets its status to 0xc and refuses. */
static uint32_t FS98_STDCALL dialog_1203_check(uint32_t records)
{
    if (UTIL_PUT(V(0x208001f8), 5, records, 1) != 0)
        return 0xffffffffu;
    if (M32(records) == 1 && M32(records + 0x2c) == 1 && M32(records + 0x24) == 1 &&
        M32(records + 0x34) != 0) {
        M32(records + 0x30) = 1;
        M32(records + 0x20) = 1;
    }
    UTIL_PUT(V(0x208001e0), 1, records, 0);
    if (M16(V(0x20807fb8)) < 0x1800) {
        M32(records + 0xc) = 0xc;
        return 0xffffffffu;
    }
    return 0;
}

/* .text+0x8d0, command 0x1203, with +0x990 and +0x9f0.  Applying compares the
 * text field's value with what it held going in, flagging GLOBAL +0x38a, and
 * by the +0x34 value tells module 24 (+0x18 for 1, +0x1c for 2). */
static void command_1203(void)
{
    M32(V(0x208001a0)) = 0;
    UTIL_GET(V(0x208001f8), 5, V(0x208001a0));
    if (UTIL_PUT(V(0x208001e0), 1, V(0x208001a0), 0) != 0)
        return;
    M16(V(0x20807fbc)) = M16(V(0x20807fb8));
    if (H_DIALOG(0x1203, V(0x208001a0)) != 0 || M32(V(0x208001a0)) != 1)
        return;
    UTIL_PUT(V(0x208001f8), 5, V(0x208001a0), 0);
    if (M16(V(0x20807fbc)) != M16(V(0x20807fb8)))
        M8(G + 0x38a) = 1;
    if (M32(V(0x208001d4)) == 1)
        CALL(M24, 0x18, f0)();
    else if (M32(V(0x208001d4)) == 2)
        CALL(M24, 0x1c, f0)();
    M32(V(0x20807fb4)) = 0;
}
