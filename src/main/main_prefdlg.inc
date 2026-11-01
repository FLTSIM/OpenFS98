/* MAIN.DLL: command 0x1409, General Preferences. MSVC C, .text+0x14ce0..+0x15c7f, with
 * its Display (0x14090001) and Flight's aircraft (0x14090002) dialogs. The presets at
 * .data+0x5740 are 0x80 bytes each. */

#define PF_DLG    V(0x20809508)
#define PF_PRESET V(0x208095d8)
#define PF_DISP   V(0x20805e08)
#define PF_MODE   V(0x20809318)   /* host +0x31c's block of 0xe8 */
#define PF_FLAGS  V(0x208093e8)
#define PF_DESC   V(0x208093fc)
#define H_DISPLAY HOST_(0x31c, f3)  /* (op, block, size) */

/* The preset's words that the display dialog shows, and where it shows them */
static const struct { uint16_t preset, shown; } pf_shown[] = {
    { 0x5784, 0x5e5c }, { 0x5788, 0x5e64 }, { 0x578c, 0x5e6c }, { 0x5790, 0x5e74 }, { 0x5794, 0x5e7c },
    { 0x5798, 0x5e84 }, { 0x579c, 0x5e8c }, { 0x57a0, 0x5e94 }, { 0x57a4, 0x5e9c }, { 0x57a8, 0x5ea4 },
    { 0x57ac, 0x5e24 }, { 0x57b0, 0x5e2c }, { 0x57b4, 0x5e34 }, { 0x57b8, 0x5e3c },
};

/* .text+0x15960: a list of strings freed, last first */
static void pf_free_list(uint32_t list)
{
    while (M32(list) != 0) {
        M32(list) -= 1;
        H_FREE(M32(list + 4) + M32(list) * 4);
    }
    H_FREE(list + 4);
}

/* The four flags the display block keeps in one word */
static void pf_set_flags(uint32_t b)
{
    uint32_t f = M32(PF_FLAGS);

    f = M32(b + 0x10) != 0 ? f | 4 : f & 0xfffffffbu;
    f = M32(b + 0x34) != 0 ? f | 0x10 : f & 0xffffffefu;
    f = M32(b + 0x38) != 0 ? f | 0x80 : f & 0xffffff7fu;
    f = M32(b + 0x3c) != 0 ? f | 0x400 : f & 0xfffffbffu;
    M32(PF_FLAGS) = f;
}

/* .text+0x15830: the drivers */
static uint32_t FS98_STDCALL pf_list_drivers(uint32_t b)
{
    uint32_t r, i;

    pf_free_list(b + 0x18);
    if (H_ALLOC(5, 0x320, b + 0x1c, 1) != 0)
        return 0xffffffffu;
    M32(b + 0x18) = 0xc8;
    pf_set_flags(b);
    M32(PF_DESC) = M32(b + 0x1c);
    H_DISPLAY(3, PF_MODE, 0xe8);
    M32(b + 0x30) = (M32(PF_FLAGS) & 0x30000000u) != 0;
    r = H_DISPLAY(1, PF_MODE, 0xe8);
    M32(b + 0x20) = 0;
    for (i = 0; M32(M32(b + 0x1c) + i * 4) != 0; i++)
        if (lstrcmpiA((const char *)(uintptr_t)M32(M32(b + 0x1c) + i * 4), (const char *)(uintptr_t)PF_MODE) == 0)
            M32(b + 0x20) = i;
    return r;
}

/* .text+0x159a0: and the modes for the driver chosen */
static uint32_t FS98_STDCALL pf_list_modes(uint32_t b)
{
    uint32_t r, i, list;

    pf_free_list(b + 0x24);
    if (H_ALLOC(5, 0x320, b + 0x28, 1) != 0)
        return 0xffffffffu;
    M32(b + 0x24) = 0xc8;
    pf_set_flags(b);
    lstrcpyA((char *)(uintptr_t)PF_MODE, (const char *)(uintptr_t)M32(M32(b + 0x1c) + M32(b + 0x20) * 4));
    M32(PF_DESC) = M32(b + 0x28);
    r = H_DISPLAY(4, PF_MODE, 0xe8);
    M32(b + 0x30) = (M32(PF_FLAGS) & 0x30000000u) != 0;
    M32(b + 0x2c) = 0xffffffffu;
    H_DISPLAY(1, PF_MODE, 0xe8);
    list = M32(b + 0x28);
    for (i = 0; M32(list + i * 4) != 0; i++) {
        if (lstrcmpiA((const char *)(uintptr_t)M32(list + i * 4), (const char *)(uintptr_t)V(0x208093c0)) == 0)
            M32(b + 0x2c) = i;
        list = M32(b + 0x28);
    }
    if (M32(b + 0x2c) == 0xffffffffu)
        for (i = 0; M32(M32(b + 0x28) + i * 4) != 0; i++) {
            if (lstrcmpiA((const char *)(uintptr_t)M32(M32(b + 0x28) + i * 4), (const char *)(uintptr_t)V(0x20806234)) == 0)
                M32(b + 0x2c) = i;
            if (lstrcmpiA((const char *)(uintptr_t)M32(M32(b + 0x28) + i * 4), (const char *)(uintptr_t)V(0x20806228)) == 0)
                M32(b + 0x2c) = i;
        }
    return r;
}

/* .text+0x15b20 */
static uint32_t FS98_STDCALL pf_refresh(uint32_t b)
{
    (void)b;
    H_DISPLAY(6, 0, 0);
    return 0;
}

/* .text+0x15b40 */
static void pf_display_load(uint32_t b)
{
    H_DISPLAY(1, PF_MODE, 0xe8);
    M32(b + 0x10) = M32(PF_FLAGS) & 4;
    M32(b + 0x38) = M32(PF_FLAGS) & 0x80;
    M32(b + 0x3c) = M32(PF_FLAGS) & 0x400;
    M32(b + 0x14) = 0;
    M32(b + 0x34) = M32(PF_FLAGS) & 0x10;
}

/* .text+0x15bd0 */
static void pf_display_apply(uint32_t b)
{
    pf_set_flags(b);
    lstrcpyA((char *)(uintptr_t)PF_MODE, (const char *)(uintptr_t)M32(M32(b + 0x1c) + M32(b + 0x20) * 4));
    lstrcpyA((char *)(uintptr_t)V(0x208093c0), (const char *)(uintptr_t)M32(M32(b + 0x28) + M32(b + 0x2c) * 4));
    H_DISPLAY(5, PF_MODE, 0xe8);
}

/* .text+0x15ba0 */
static void pf_display_close(uint32_t b, uint32_t apply)
{
    if (apply != 0 && M32(b + 0x14) != 0)
        pf_display_apply(b);
    pf_free_list(b + 0x24);
    pf_free_list(b + 0x18);
}

/* .text+0x15810 */
static uint32_t FS98_STDCALL pf_display_check(uint32_t b)
{
    return UTIL_PUT(V(0x208060e0), 0x10, b, 1);
}

/* .text+0x15600 */
static void pf_detail_load(void)
{
    uint32_t i, at;

    UTIL_GET(V(0x208060e0), 0x10, PF_DISP);
    if (M32(PF_PRESET) != 0) {
        at = M32(PF_PRESET) << 7;
        for (i = 0; i < sizeof pf_shown / sizeof pf_shown[0]; i++)
            M32(D + pf_shown[i].shown) = M32(D + pf_shown[i].preset + at);
    }
}

/* .text+0x156e0: anything moved off the preset and it is no longer the preset. */
static void pf_detail_apply(void)
{
    uint32_t i, at;

    UTIL_PUT(V(0x208060e0), 0x10, PF_DISP, 0);
    if (M32(PF_PRESET) != 0) {
        at = M32(PF_PRESET) << 7;
        for (i = 0; i < sizeof pf_shown / sizeof pf_shown[0]; i++)
            if (M32(D + pf_shown[i].shown) != M32(D + pf_shown[i].preset + at)) {
                M32(PF_PRESET) = 0;
                return;
            }
    }
}

/* .text+0x15590: the Display button */
static uint32_t FS98_STDCALL pf_display(uint32_t b)
{
    uint32_t r;

    (void)b;
    pf_detail_load();
    pf_display_load(V(0x20805ea8));
    r = H_DIALOG(0x14090001, PF_DISP);
    if (r == 0 && M32(PF_DISP) == 1) {
        pf_detail_apply();
        pf_display_close(V(0x20805ea8), 1);
        return r;
    }
    pf_display_close(V(0x20805ea8), 0);
    return r;
}

/* .text+0x15560 */
static uint32_t FS98_STDCALL pf_preset_show(uint32_t b)
{
    uint32_t i = M32(b + 0xd0);

    if (i != 0) {
        M32(b + 0xdc) = M32(V(0x20805780) + (i << 7));
        M32(b + 0xf4) = M32(V(0x208057bc) + (i << 7));
    }
    return 0;
}

/* .text+0x15300 */
static uint32_t FS98_STDCALL pf_check(uint32_t b)
{
    return UTIL_PUT(V(0x20805ee8), 0x19, b, 1);
}

/* .text+0x15320: Defaults */
static uint32_t FS98_STDCALL pf_defaults(uint32_t b)
{
    H_MEMMOVE(b, V(0x20805cc0), 0x148);
    M32(b + 0xc8) = V(0x20805740);
    M32(b + 0xcc) = 0xb;
    H_CFG_GET_INT(V(0x20806220), V(0x20803a78), b + 0xd0);
    pf_preset_show(b);
    return 0;
}

/* .text+0x15380: the default aircraft. Its block runs on into the count and list on
 * retail's stack; the scan of the list decides nothing. */
static uint32_t FS98_STDCALL pf_aircraft(uint32_t b)
{
    volatile uint32_t block[7];
    uint32_t i, item;

    (void)b;
    CALL(M7, 0x38, f2)(ADDR(&block[5]), ADDR(&block[6]));
    if (M8(G + 0x638) != 0)
        for (i = 0; (int32_t)i < (int32_t)block[5]; i++)
            if (H_STRCMP(G + 0x638, block[6] + i * 0xa08 + 0x900) == 0)
                break;
    block[0] = 0;
    block[1] = ADDR(main_nothing);
    block[2] = ADDR(main_nothing);
    block[3] = ADDR(main_nothing);
    block[4] = 0;
    H_DIALOG(0x14090002, ADDR(block));
    if (block[0] != 0) {
        item = block[6] + block[4] * 0xa08 + 0x900;
        if (H_EXISTS(item, 0x1d) != 0)
            CALL(UTIL, 0xa4, f8)(1, 1, 0xf419, 0, 0, 0, 0, 0);
        else
            H_STRCPY(G + 0x638, item);
    }
    H_FREE(ADDR(&block[6]));
    return 0;
}

/* .text+0x154d0: the default flight */
static uint32_t FS98_STDCALL pf_flight(uint32_t b)
{
    char path[0x104];

    (void)b;
    if (flight_dialog(ADDR(path), G + 0x24) != 0) {
        if (H_EXISTS(ADDR(path), 2) != 0) {
            CALL(UTIL, 0xa4, f8)(1, 1, 0xf460, 0, 0, 0, 0, 0);
            return 0;
        }
        H_STRCPY(G + 0x24, ADDR(path));
    }
    return 0;
}

/* .text+0x14e70 */
static uint32_t pf_load(void)
{
    static const struct { uint16_t at, g; uint8_t wide; } same[] = {
        { 0x5788, 0x39c, 0 }, { 0x578c, 0x39e, 0 }, { 0x5790, 0x398, 1 }, { 0x5794, 0xe84, 0 }, { 0x5798, 0xe82, 0 },
        { 0x579c, 0x3a4, 1 }, { 0x57a0, 0xdc0, 0 }, { 0x57a4, 0xcd0, 0 }, { 0x57a8, 0x11bc, 0 }, { 0x57ac, 0x11b6, 0 },
        { 0x57b0, 0x11b4, 0 }, { 0x57b4, 0x556, 0 }, { 0x57b8, 0x11b8, 0 },
    };
    uint32_t i, gp, at;

    H_MEMMOVE(PF_DLG, V(0x20805cc0), 0x148);
    for (i = 0; i < 12; i++)              /* the last overwrites the head of the descriptor it was copied from */
        H_LOADSTRING(0x2300 + i, V(0x20805740) + i * 0x80, 0x40);
    UTIL_GET(V(0x20805ee8), 0x19, PF_DLG);
    gp = G;
    M32(V(0x208095ec)) = M32(gp + 0x1254) == 0;
    H_STRCPY(V(0x20809400), gp + 0x24);
    H_STRCPY(V(0x20809650), G + 0x638);
    gp = G;
    /* the first word is tested the wrong way up: a preset still in force is forgotten
     * only if GLOBAL +0x11b2 differs from it and all the others agree */
    if (M32(gp + 0x123c) != 0) {
        at = M32(PF_PRESET) << 7;
        if (M32(V(0x20805784) + at) != M16(gp + 0x11b2)) {
            for (i = 0; i < sizeof same / sizeof same[0]; i++)
                if (M32(D + same[i].at + at) != (same[i].wide ? M32(gp + same[i].g) : M16(gp + same[i].g)))
                    break;
            if (i == sizeof same / sizeof same[0])
                M32(gp + 0x123c) = 0;
        }
    }
    M32(V(0x208095d0)) = V(0x20805740);
    M32(V(0x208095d4)) = 0xb;
    M32(PF_PRESET) = M32(gp + 0x123c);
    M32(V(0x208095bc)) = M32(gp + 0x1290);
    CALL(SOUND, 0x40, f1)(V(0x20809604));
    return 0;
}

/* .text+0x150b0 */
static uint32_t pf_apply(void)
{
    uint32_t gp = G, was_1238 = M32(gp + 0x1238), was_35c = M32(gp + 0x35c), a = M32(gp + 0x1210), b = M32(gp + 0x1214);
    uint32_t c = M32(gp + 0x1218), d = M32(gp + 0x121c), r, list = 0, count = 0, i;
    uint16_t e = M16(gp + 0x242);

    r = UTIL_PUT(V(0x20805ee8), 0x19, PF_DLG, 0);
    H_288(M32(G + 0x1220) == 0);
    if (r == 0)
        M32(G + 0x1254) = M32(V(0x208095ec)) == 0;
    M32(G + 0x123c) = M32(PF_PRESET);
    prefs_preset(M32(PF_PRESET));
    if (H_STRICMP(V(0x20809400), G + 0x24) != 0 || a != M32(G + 0x1210) || e != M16(G + 0x242) || b != M32(G + 0x1214) ||
        c != M32(G + 0x1218) || d != M32(G + 0x121c) || H_STRICMP(V(0x20809650), G + 0x638) != 0) {
        if (CALL(UTIL, 0xa4, f8)(0xc0, 3, 0xf450, 0xf451, 0xf452, 0xf453, 0xf454, 0) == 0x80)
            HOST_(0x28, f0)();
    }
    if (M32(G + 0x1238) != was_1238) {
        CALL(WINDOW_, 0x80, f3)(ADDR(&list), ADDR(&count), 3);
        for (i = 0; i < count; i++) {
            if (M32(G + 0x1238) != 0)
                CALL(WINDOW_, 0x70, f1)(M32(list + i * 4));
            else
                CALL(WINDOW_, 0x74, f1)(M32(list + i * 4));
        }
    }
    CHAIN_SEND(0x14, 0x70, 0);
    gp = G;
    if (M32(gp + 0x35c) == 0 && was_35c != 0 && M16(gp + 0x34c) < 0x200) {
        M16(gp + 0x34c) = 0x200;
        M16(gp + 0x356) = 0;
    }
    CALL(SOUND, 0x3c, f1)(V(0x20809604));
    CALL(PANELS_, 0x34, f0)();
    return r;
}

/* .text+0x14ce0, command 0x1409 */
static uint32_t command_1409(void)
{
    uint32_t r = pf_load();

    if (r != 0)
        return r;
    r = H_DIALOG(0x1409, PF_DLG);
    if (r == 0 && M32(PF_DLG) == 1)
        pf_apply();
    return r;
}
