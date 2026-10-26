/* MAIN.DLL slice 18: the preferences, Linkage +0xb4 (load), +0xb8 (save)
 * and +0xbc (the defaults), MSVC C, .text+0xdd70..+0xf2e0 and the preset
 * routine .text+0x14d20.
 *
 * All three are straight runs of HOST profile calls on the [PREFS] section
 * of FLTSIM98.CFG (.data+0x364c), a key at a time (the keys are .data
 * strings), each read into or written from a GLOBAL field.  The runs were
 * transcribed from the disassembly by script, call for call, and are
 * verified as transcripts by maindiff's `prefs' suite.
 *
 * Word and byte fields go through four small helpers.  Reading, the field
 * (zero-extended) is the default and the low part of what comes back is
 * stored only when HOST +0xa8 answers 0 -- which it always does.  Writing,
 * the value is masked to its width, so whatever was left in the rest of
 * the register retail pushes does not reach the file. */

#define H_CFG_GET_INT     HOST_(0x0a8, f3)   /* (sec, key, &io) */
#define H_CFG_PUT_INT     HOST_(0x0ac, f3)   /* (sec, key, v) */
#define H_CFG_GET_STRING  HOST_(0x0c8, f4)   /* (sec, key, buf, size): buf is the default */
#define H_CFG_PUT_STRING  HOST_(0x0cc, f3)   /* (sec, key, s) */
#define H_288             HOST_(0x288, f1)

/* .text+0xe520 */
static void prefs_get_word(uint32_t sec, uint32_t key, uint32_t field)
{
    uint32_t v = M16(field);

    if (H_CFG_GET_INT(sec, key, ADDR(&v)) == 0)
        M16(field) = (uint16_t)v;
}

/* .text+0xe560 */
static void prefs_get_byte(uint32_t sec, uint32_t key, uint32_t field)
{
    uint32_t v = M8(field);

    if (H_CFG_GET_INT(sec, key, ADDR(&v)) == 0)
        M8(field) = (uint8_t)v;
}

/* .text+0xf290 */
static void prefs_put_word(uint32_t sec, uint32_t key, uint32_t v)
{
    H_CFG_PUT_INT(sec, key, v & 0xffff);
}

/* .text+0xf2c0 */
static void prefs_put_byte(uint32_t sec, uint32_t key, uint32_t v)
{
    H_CFG_PUT_INT(sec, key, v & 0xff);
}

/* .text+0x14d20: apply performance mode `mode`'s preset.  The presets are
 * 0x80-byte entries at .data+0x5740, a 0x40-byte name (string 0x2300 + n,
 * loaded at init) then the settings; mode 0 applies nothing.  The index is
 * not bounded: a mode past the table reads on through .data. */
static void prefs_preset(uint32_t mode)
{
    uint32_t p;

    if (mode == 0)
        return;
    p = V(0x20805780) + (mode << 7);
    M16(G + 0xcb4) = M16(p + 0x00);
    M16(G + 0x11b2) = M16(p + 0x04);
    M16(G + 0x39c) = M16(p + 0x08);
    M16(G + 0x39e) = M16(p + 0x0c);
    M32(G + 0x398) = M32(p + 0x10);
    M16(G + 0xe84) = M16(p + 0x14);
    M16(G + 0xe82) = M16(p + 0x18);
    M32(G + 0x3a4) = M32(p + 0x1c);
    M16(G + 0xdc0) = M16(p + 0x20);
    M16(G + 0xcd0) = M16(p + 0x24);
    M16(G + 0x11bc) = M16(p + 0x28);
    M16(G + 0x11b6) = M16(p + 0x2c);
    M16(G + 0x11b4) = M16(p + 0x30);
    M16(G + 0x556) = M16(p + 0x34);
    M16(G + 0x11b8) = M16(p + 0x38);
    M32(G + 0x1238) = M32(p + 0x3c);
}

/* .text+0xdd70 == Linkage +0xb4: load the preferences into GLOBAL, apply
 * the performance mode's preset (GLOBAL +0x123c), and tell the host whether
 * GLOBAL +0x1220 is clear (HOST +0x288). */
static uint32_t FS98_STDCALL prefs_load(void)
{
    H_CFG_GET_INT(V(0x2080364c), V(0x208039ac), G + 0x35c);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803998), G + 0x1234);
    prefs_get_word(V(0x2080364c), V(0x20803984), G + 0x11b6);
    prefs_get_word(V(0x2080364c), V(0x20803970), G + 0x11b4);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803968), G + 0xda8);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803960), G + 0xdac);
    prefs_get_word(V(0x2080364c), V(0x20803950), G + 0xe82);
    prefs_get_word(V(0x2080364c), V(0x20803940), G + 0xe84);
    prefs_get_word(V(0x2080364c), V(0x20803934), G + 0x362);
    H_CFG_GET_INT(V(0x2080364c), V(0x2080392c), G + 0x1210);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803920), G + 0x54c);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803914), G + 0x1240);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803900), G + 0x3a4);
    prefs_get_word(V(0x2080364c), V(0x208038f0), G + 0x11b2);
    prefs_get_word(V(0x2080364c), V(0x208038e8), G + 0xb06);
    H_CFG_GET_INT(V(0x2080364c), V(0x208038dc), G + 0xcd8);
    prefs_get_word(V(0x2080364c), V(0x208038cc), G + 0xcb4);
    prefs_get_word(V(0x2080364c), V(0x208038bc), G + 0xcd0);
    prefs_get_word(V(0x2080364c), V(0x208038b4), G + 0x612);
    prefs_get_word(V(0x2080364c), V(0x208038ac), G + 0xc02);
    prefs_get_word(V(0x2080364c), V(0x208038a4), G + 0xc04);
    prefs_get_word(V(0x2080364c), V(0x2080389c), G + 0xc06);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803890), G + 0x1248);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803888), G + 0x1244);
    H_CFG_GET_INT(V(0x2080364c), V(0x2080387c), G + 0x124c);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803870), G + 0x1250);
    H_CFG_GET_STRING(V(0x2080364c), V(0x20803864), G + 0x12c, 0x104);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803858), G + 0x128);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803844), G + 0x1238);
    prefs_get_word(V(0x2080364c), V(0x20803830), G + 0x3ae);
    prefs_get_word(V(0x2080364c), V(0x20803820), G + 0xdc6);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803810), G + 0x328);
    H_CFG_GET_INT(V(0x2080364c), V(0x208037f8), G + 0x32c);
    prefs_get_word(V(0x2080364c), V(0x208037f0), G + 0xc18);
    prefs_get_word(V(0x2080364c), V(0x208037e8), G + 0xc0a);
    prefs_get_word(V(0x2080364c), V(0x208037dc), G + 0xc08);
    prefs_get_word(V(0x2080364c), V(0x208037d4), G + 0xc0c);
    H_CFG_GET_INT(V(0x2080364c), V(0x208037c0), G + 0x1230);
    H_CFG_GET_INT(V(0x2080364c), V(0x208037ac), G + 0x1228);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803794), G + 0x1224);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803780), G + 0x123c);
    prefs_get_word(V(0x2080364c), V(0x2080376c), G + 0x11b8);
    H_CFG_GET_INT(V(0x2080364c), V(0x2080375c), G + 0x1220);
    H_CFG_GET_INT(V(0x2080364c), V(0x2080374c), G + 0x358);
    prefs_get_word(V(0x2080364c), V(0x20803740), G + 0x556);
    H_CFG_GET_STRING(V(0x2080364c), V(0x20803734), G + 0x24, 0x104);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803728), G + 0x398);
    prefs_get_word(V(0x2080364c), V(0x2080371c), G + 0xdc0);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803714), G + 0x1214);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803700), G + 0x121c);
    H_CFG_GET_INT(V(0x2080364c), V(0x208036f0), G + 0x122c);
    H_CFG_GET_STRING(V(0x2080364c), V(0x208036e0), G + 0x638, 0x104);
    prefs_get_word(V(0x2080364c), V(0x208036d4), G + 0x242);
    prefs_get_word(V(0x2080364c), V(0x208036c4), G + 0x39e);
    prefs_get_word(V(0x2080364c), V(0x208036b8), G + 0x39c);
    prefs_get_word(V(0x2080364c), V(0x208036a8), G + 0x11bc);
    H_CFG_GET_INT(V(0x2080364c), V(0x2080369c), G + 0x1254);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803694), G + 0x1218);
    prefs_get_byte(V(0x2080364c), V(0x20803680), G + 0x3aa);
    prefs_get_byte(V(0x2080364c), V(0x2080366c), G + 0x3ab);
    H_CFG_GET_INT(V(0x2080364c), V(0x20803658), G + 0x128c);
    prefs_preset(M32(G + 0x123c));
    H_288(M32(G + 0x1220) == 0);
    return 0;
}

/* .text+0xe5a0 == Linkage +0xb8: save the preferences from GLOBAL. */
static uint32_t FS98_STDCALL prefs_save(void)
{
    H_CFG_PUT_INT(V(0x2080364c), V(0x208039ac), M32(G + 0x35c));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803998), M32(G + 0x1234));
    prefs_put_word(V(0x2080364c), V(0x20803984), M16(G + 0x11b6));
    prefs_put_word(V(0x2080364c), V(0x20803970), M16(G + 0x11b4));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803968), M32(G + 0xda8));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803960), M32(G + 0xdac));
    prefs_put_word(V(0x2080364c), V(0x20803950), M16(G + 0xe82));
    prefs_put_word(V(0x2080364c), V(0x20803940), M16(G + 0xe84));
    prefs_put_word(V(0x2080364c), V(0x20803934), M16(G + 0x362));
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080392c), M32(G + 0x1210));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803920), M32(G + 0x54c));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803914), M32(G + 0x1240));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803900), M32(G + 0x3a4));
    prefs_put_word(V(0x2080364c), V(0x208038f0), M16(G + 0x11b2));
    prefs_put_word(V(0x2080364c), V(0x208038e8), M16(G + 0xb06));
    H_CFG_PUT_INT(V(0x2080364c), V(0x208038dc), M32(G + 0xcd8));
    prefs_put_word(V(0x2080364c), V(0x208038cc), M16(G + 0xcb4));
    prefs_put_word(V(0x2080364c), V(0x208038bc), M16(G + 0xcd0));
    prefs_put_word(V(0x2080364c), V(0x208038b4), M16(G + 0x612));
    prefs_put_word(V(0x2080364c), V(0x208038ac), M16(G + 0xc02));
    prefs_put_word(V(0x2080364c), V(0x208038a4), M16(G + 0xc04));
    prefs_put_word(V(0x2080364c), V(0x2080389c), M16(G + 0xc06));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803890), M32(G + 0x1248));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803888), M32(G + 0x1244));
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080387c), M32(G + 0x124c));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803870), M32(G + 0x1250));
    H_CFG_PUT_STRING(V(0x2080364c), V(0x20803864), G + 0x12c);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803858), M32(G + 0x128));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803844), M32(G + 0x1238));
    prefs_put_word(V(0x2080364c), V(0x20803830), M16(G + 0x3ae));
    prefs_put_word(V(0x2080364c), V(0x20803820), M16(G + 0xdc6));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803810), M32(G + 0x328));
    prefs_put_word(V(0x2080364c), V(0x208037f0), M16(G + 0xc18));
    prefs_put_word(V(0x2080364c), V(0x208037e8), M16(G + 0xc0a));
    prefs_put_word(V(0x2080364c), V(0x208037dc), M16(G + 0xc08));
    prefs_put_word(V(0x2080364c), V(0x208037d4), M16(G + 0xc0c));
    H_CFG_PUT_INT(V(0x2080364c), V(0x208037c0), M32(G + 0x1230));
    H_CFG_PUT_INT(V(0x2080364c), V(0x208037ac), M32(G + 0x1228));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803794), M32(G + 0x1224));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803780), M32(G + 0x123c));
    prefs_put_word(V(0x2080364c), V(0x2080376c), M16(G + 0x11b8));
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080375c), M32(G + 0x1220));
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080374c), M32(G + 0x358));
    prefs_put_word(V(0x2080364c), V(0x20803740), M16(G + 0x556));
    H_CFG_PUT_STRING(V(0x2080364c), V(0x20803734), G + 0x24);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803728), M32(G + 0x398));
    prefs_put_word(V(0x2080364c), V(0x2080371c), M16(G + 0xdc0));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803714), M32(G + 0x1214));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803700), M32(G + 0x121c));
    H_CFG_PUT_INT(V(0x2080364c), V(0x208036f0), M32(G + 0x122c));
    H_CFG_PUT_STRING(V(0x2080364c), V(0x208036e0), G + 0x638);
    prefs_put_word(V(0x2080364c), V(0x208036d4), M16(G + 0x242));
    prefs_put_word(V(0x2080364c), V(0x208036c4), M16(G + 0x39e));
    prefs_put_word(V(0x2080364c), V(0x208036b8), M16(G + 0x39c));
    prefs_put_word(V(0x2080364c), V(0x208036a8), M16(G + 0x11bc));
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080369c), M32(G + 0x1254));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803694), M32(G + 0x1218));
    prefs_put_byte(V(0x2080364c), V(0x20803680), M8(G + 0x3aa));
    prefs_put_byte(V(0x2080364c), V(0x2080366c), M8(G + 0x3ab));
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803658), M32(G + 0x128c));
    return 0;
}

/* .text+0xed10 == Linkage +0xbc: write the default preferences for
 * performance mode `mode`, which also decides five of them (signed: above
 * 4 the second set), the default aircraft and APL's CACHE_WAV_FILES. */
static uint32_t FS98_STDCALL prefs_defaults(uint32_t mode)
{
    H_CFG_PUT_INT(V(0x2080364c), V(0x208039ac), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803998), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803968), 0x2);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803960), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803934), 0x7);
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080392c), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803920), 0x2);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803914), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208038dc), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803aa4), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803a98), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803a88), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208038b4), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208038ac), 0x40);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208038a4), 0x40);
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080389c), 0x40);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803890), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803888), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080387c), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803870), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803858), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803820), 0x2);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803810), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208037f0), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208037e8), 0x19);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208037dc), 0x23);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208037d4), 0x40);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208037c0), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080375c), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080374c), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080371c), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803714), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803700), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208036f0), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x208036d4), 0x1);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803694), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803680), 0x20);
    H_CFG_PUT_INT(V(0x2080364c), V(0x2080366c), 0x20);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803658), 0x0);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803780), mode);
    H_CFG_PUT_INT(V(0x2080364c), V(0x20803a78), mode);
    if ((int32_t)mode <= 4) {
        H_CFG_PUT_INT(V(0x2080364c), V(0x208038e8), 0x0);
        H_CFG_PUT_INT(V(0x2080364c), V(0x20803830), 0x0);
        H_CFG_PUT_INT(V(0x2080364c), V(0x208037ac), 0x1);
        H_CFG_PUT_INT(V(0x2080364c), V(0x20803794), 0x1);
        H_CFG_PUT_INT(V(0x2080364c), V(0x2080369c), 0x0);
    } else {
        H_CFG_PUT_INT(V(0x2080364c), V(0x208038e8), 0x2);
        H_CFG_PUT_INT(V(0x2080364c), V(0x20803830), 0x1);
        H_CFG_PUT_INT(V(0x2080364c), V(0x208037ac), 0x0);
        H_CFG_PUT_INT(V(0x2080364c), V(0x20803794), 0x0);
        H_CFG_PUT_INT(V(0x2080364c), V(0x2080369c), 0x1);
    }
    H_CFG_PUT_STRING(V(0x20803a48), V(0x20803a58), V(0x20803a60));
    H_CFG_PUT_STRING(V(0x20803a48), V(0x20803a34), V(0x20803a3c));
    H_CFG_PUT_STRING(V(0x20803a48), V(0x20803a10), V(0x20803a18));
    H_CFG_PUT_STRING(V(0x20803a48), V(0x208039f8), V(0x20803a00));
    H_CFG_PUT_STRING(V(0x20803a48), V(0x208039e4), V(0x208039ec));
    H_CFG_PUT_STRING(V(0x20803a48), V(0x208039cc), V(0x208039d4));
    H_CFG_PUT_INT(V(0x208039b8), V(0x208039bc), 0x1);
    return 0;
}
