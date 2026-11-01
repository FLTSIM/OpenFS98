/* MAIN.DLL: command 0x1202, the radios dialog, with a compass preview for the OBS and ADF
 * cards. MSVC C, .text+0x13870..+0x14aa8 and +0x17ca0, +0x19dbc (assembly). The NAV and
 * ADF fields look their stations up as they are typed. */

#define RD_DLG     V(0x20805368)
#define RD_NAV2    V(0x208091e8)
#define RD_COM     V(0x20805360)
#define RD_COM2    V(0x20805364)

/* .text+0x19dbc: station w1 at w2 (BCD fractions packed in a byte) near pos: its record */
static uint32_t radio_station(uint16_t w1, uint16_t w2, uint32_t pos, uint32_t out)
{
    uint8_t buf[0x190];
    uint8_t cl = (uint8_t)((w2 & 0xf) | (uint8_t)((w2 >> 8) << 4));

    if (CALL(FE_, 0x28, f4)(w1, cl, pos, ADDR(buf)) == 0) {
        H_MEMSET(out, 0, 0x3c);
        return 0xffffffffu;
    }
    {
        static const struct radio_field f[4] = {
            { 3, 3, 6, 0x208072a0 }, { 1, 1, 2, 0x208072b8 }, { 5, 0, 0, 0x208072ba }, { 0x18, 0, 0, 0x208072c0 },
        };

        radio_marshal(ADDR(buf), f, 4);
    }
    H_MEMMOVE(out, V(0x208072a0), 0x3c);
    return 0;
}

/* .text+0x149d0: an ADF or VOR's name, ident and frequency to its fields */
static void rd_station(uint32_t which, uint32_t name, uint32_t ident, uint32_t lat, uint32_t lon, uint32_t pw, uint32_t pb)
{
    uint8_t local[0x58];
    uint32_t r;

    M16(G + 0x37e) = 0;
    r = nav_describe((uint16_t)which, G + 0x560, ADDR(local));
    H_STRCPY(name, ADDR(local) + 0x28);
    H_STRCPY(ident, ADDR(local) + 0x22);
    M16(pw) = *(uint16_t *)(local + 0x18);
    M8(pb) = local[0x1c];
    if (r != 0) {
        H_STRCPY(lat, V(0x20807fd0));
        H_STRCPY(lon, V(0x20807fd0));
        return;
    }
    N_PUT(ADDR(local), 0x15, lat);
    N_PUT(ADDR(local) + 8, 0x14, lon);
}

/* .text+0x14840: both NAV fields' stations. Text NUMERIC refuses leaves `freq` as retail's
 * stack held it; the suite gives it what parses. */
static uint32_t rd_nav_fields(uint32_t b)
{
    uint32_t freq = 0, slot = b - D + MAIN_DATA_VA;

    N_GET(M32(b + 0x2c), 0x32, ADDR(&freq));
    rd_station(freq, b + 0x5a, b + 0x54, M32(b + 0x7c), M32(b + 0x90), b + 0x3c, ADDR(&slot));
    M16(RD_NAV2) = M16(G + 0x37e);
    M8(V(0x208091dc)) = (uint8_t)slot;
    N_GET(M32(b + 0xb4), 0x32, ADDR(&freq));
    rd_station(freq, b + 0xe2, b + 0xdc, M32(b + 0x104), M32(b + 0x118), b + 0xc4, ADDR(&slot));
    return 0;
}

/* .text+0x148f0: the ADF's */
static uint32_t rd_adf_fields(uint32_t b)
{
    uint8_t local[0x3c];
    uint32_t r = radio_station(M16(RD_COM), M16(RD_COM2), G + 0x560, ADDR(local));

    H_STRCPY(b + 0x142, ADDR(local) + 0x20);
    H_STRCPY(b + 0x13c, ADDR(local) + 0x1a);
    M16(b + 0x18a) = *(uint16_t *)(local + 0x18);
    if (r != 0) {
        H_STRCPY(M32(b + 0x190), V(0x20807fd0));
        H_STRCPY(M32(b + 0x1a4), V(0x20807fd0));
        return r;
    }
    N_PUT(ADDR(local), 0x15, M32(b + 0x190));
    N_PUT(ADDR(local) + 8, 0x14, M32(b + 0x1a4));
    return r;
}

/* .text+0x147c0: the three markers' positions */
static void rd_markers(uint32_t src, uint32_t dst)
{
    static const uint8_t at[3] = { 2, 0x1a, 0x32 };
    uint32_t i;

    for (i = 0; i < 3; i++) {
        uint32_t s = M32(src + i * 4), d = dst + at[i];

        M16(d) = M16(s + 2);
        M32(d + 2) = M32(s + 4);
        M16(d + 8) = M16(s + 0xa);
        M32(d + 0xa) = M32(s + 0xc);
    }
}

/* .text+0x14390: the ADF frequency as text, from BCD words */
static uint32_t rd_adf_text(uint32_t rec, uint16_t w1, uint16_t w2)
{
    uint32_t e = 0, at = 1, mask = 0xf00, shift = 8, v, n;

    if (M32(G + 0x35c) == 1) {
        v = (uint32_t)(0x30 | ((w2 >> 8) & 0xf));
        if ((int32_t)v < 0x30) {
            e = 0xffffffffu;
            M32(rec) = 4;
            M8(M32(rec + 4)) = (uint8_t)v;
        } else {
            if ((int32_t)v > 0x31) {
                e = 0xffffffffu;
                M32(rec) = 6;
            }
            M8(M32(rec + 4)) = (uint8_t)v;
        }
    } else {
        at = 0;
    }
    if (e == 0) {
        n = at + 3;
        for (; at < n; at++) {
            v = ((uint32_t)w1 & mask) >> shift;
            v = (v & 0xffffff00u) | ((v & 0xff) | 0x30);
            if ((int32_t)v < 0x30 || (int32_t)v > 0x39) {
                e = 0xffffffffu;
                M32(rec) = 4;
                break;
            }
            M8(M32(rec + 4) + at) = (uint8_t)v;
            shift -= 4;
            mask >>= 4;
        }
    }
    if (e == 0) {
        if (M32(G + 0x35c) == 1) {
            M8(M32(rec + 4) + 4) = 0x2e;
            v = (uint32_t)((w2 & 0xf) | 0x30);
            if ((int32_t)v > 0x39) {
                e = 0xffffffffu;
                M32(rec) = 4;
            }
            M8(M32(rec + 4) + 5) = (uint8_t)v;
            M8(M32(rec + 4) + 6) = 0;
        } else {
            M8(M32(rec + 4) + 3) = 0;
        }
        if (e == 0)
            M32(rec) = 0;
    }
    if (M32(G + 0x35c) == 1 && M8(M32(rec + 4)) == 0x30)
        H_STRCPY(M32(rec + 4), M32(rec + 4) + 1);
    return e;
}

/* .text+0x144e0: and back */
static uint32_t rd_adf_parse(uint32_t rec, uint32_t pw1, uint32_t pw2)
{
    uint32_t text = M32(rec + 4), n = 0, skip, len = 0, end, i, from;
    int32_t dot = -1;
    int8_t c;

    while ((c = (int8_t)M8(text + n)) != 0) {
        if (c != 0x20 && c != 0x2e && c != 9 && (c < 0x30 || c > 0x39)) {
            M32(rec) = 4;
            return 0xffffffffu;
        }
        n++;
    }
    if (n == 0) {
        M32(rec) = 0x1003;
        return 0xffffffffu;
    }
    skip = 0;
    while (M8(text + skip) == 0x20 || M8(text + skip) == 9)
        skip++;
    while (M8(text + skip + len) != 0) {
        if (M8(text + skip + len) == 0x2e) {
            if (M32(G + 0x35c) != 1) {
                M32(rec) = 4;
                return 0xffffffffu;
            }
            dot = (int32_t)len;
        }
        len++;
    }
    if ((int32_t)len < 3) {
        M32(rec) = 0x1009;
        return 0xffffffffu;
    }
    if ((int32_t)len > 6) {
        M32(rec) = 0x1001;
        return 0xffffffffu;
    }
    if (dot != -1) {
        if (dot > 4 || dot < 3 || (dot == 3 && len == 6)) {
            M32(rec) = 8;
            return 0xffffffffu;
        }
    } else if ((int32_t)len > 4) {
        M32(rec) = 0x1001;
        return 0xffffffffu;
    }
    M16(pw1) = 0;
    M16(pw2) = 0;
    from = skip;
    if (M32(G + 0x35c) == 1 && (dot == 4 || (dot == -1 && len == 4))) {
        uint16_t top = (uint16_t)(int16_t)(int8_t)(M8(text + skip) & 0xf);

        M16(pw2) = top;
        from = skip + 1;
        if (top > 1) {
            M32(rec) = 6;
            return 0xffffffffu;
        }
        M16(pw2) = (uint16_t)((uint32_t)(uint8_t)top << 8);
    }
    end = dot == -1 ? skip + len : skip + (uint32_t)dot;
    for (i = from; (int32_t)i < (int32_t)end; i++) {
        c = (int8_t)M8(text + i);
        if (c < 0x30 || c > 0x39) {
            M32(rec) = 4;
            return 0xffffffffu;
        }
        M16(pw1) = (uint16_t)((M16(pw1) << 4) + (c & 0xf));
    }
    if (M16(pw2) == 0x100 && M16(pw1) > 0x699) {
        M32(rec) = 0xb;
        return 0xffffffffu;
    }
    if (M16(pw2) == 0) {
        if (M16(pw1) < 0x200) {
            M32(rec) = 0xc;
            return 0xffffffffu;
        }
        if (M16(pw1) > 0x999) {
            M32(rec) = 0xb;
            return 0xffffffffu;
        }
    }
    if (end == skip + (uint32_t)dot)
        for (i = end + 1; (int32_t)i < (int32_t)(skip + len); i++) {
            c = (int8_t)M8(text + i);
            if (c < 0x30 || c > 0x39) {
                M32(rec) = 4;
                return 0xffffffffu;
            }
            M16(pw2) = (uint16_t)(M16(pw2) + (c & 0xf));
        }
    M32(rec) = 0;
    return 0;
}

/* .text+0x138f0 */
static uint32_t rd_load(void)
{
    uint32_t gp = G, r;
    uint8_t al, cl;

    M32(RD_DLG) = 0;
    M32(V(0x2080537c)) = M32(gp + 0x7a0);
    M32(V(0x20805380)) = M32(gp + 0x7a0) != 0 ? M32(gp + 0x7ac) : 0;
    M32(V(0x20805384)) = M32(gp + 0x7a4);
    M32(V(0x20805388)) = M32(gp + 0x7b0);
    M32(V(0x2080538c)) = M32(gp + 0x7b4);
    if (M32(gp + 0x7a4) == 0) {
        M32(V(0x20805388)) = 0;
        M32(V(0x2080538c)) = 0;
    }
    al = M8(gp + 0xc68);
    cl = M8(gp + 0xc58);
    M8(V(0x208091e0)) = al;
    M8(V(0x208091d0)) = cl;
    if (al == 1 && cl == 1)
        M32(V(0x20805374)) = 2;
    else if (al == 0 && cl == 0)
        M32(V(0x20805374)) = 1;
    else
        M32(V(0x20805374)) = 0;
    /* degrees from the binary angle: v * 360 / 0x10000, by Microsoft's unsigned division */
    {
        uint32_t v = (uint32_t)M16(gp + 0xc4e) << 0x10;
        uint32_t h = (uint32_t)(((uint64_t)v * 0x6c16c16du) >> 32);

        M16(V(0x208091d8)) = (uint16_t)((((v - h) >> 1) + h) >> 8);
        v = (uint32_t)M16(gp + 0xc5e) << 0x10;
        h = (uint32_t)(((uint64_t)v * 0x6c16c16du) >> 32);
        M16(V(0x208091e4)) = (uint16_t)((((v - h) >> 1) + h) >> 8);
    }
    UTIL_GET(V(0x208056a0), 2, RD_DLG);
    UTIL_GET(V(0x20805588), 0xe, RD_DLG);
    gp = G;
    if (M32(gp + 0x35c) == 0) {
        M8(V(0x208054f0)) = 3;
        if (M16(gp + 0x34c) < 0x200) {
            M16(gp + 0x34c) = 0x200;
            M16(G + 0x356) = 0;
        }
    } else {
        M8(V(0x208054f0)) = 6;
    }
    gp = G;
    r = rd_adf_text(V(0x208054dc), M16(gp + 0x34c), M16(gp + 0x356));
    if (r != 0)
        return r;
    gp = G;
    M16(RD_COM) = M16(gp + 0x34c);
    M16(RD_COM2) = M16(gp + 0x356);
    M32(V(0x20805408)) = M8(M32(M32(gp + 0x384)) + 0x1c) & 0x80;
    M16(V(0x208053a4)) = M16(M32(M32(gp + 0x384)) + 0x18);
    M16(V(0x2080542c)) = M16(M32(M32(gp + 0x384) + 4) + 0x18);
    M16(V(0x2080540c)) = M16(gp + 0x37e);
    M16(RD_NAV2) = M16(gp + 0x37e);
    M16(V(0x208054f2)) = M16(M32(M32(gp + 0x384) + 8) + 0x18);
    rd_markers(M32(gp + 0x384), V(0x208092c0));
    H_MEMSET(V(0x208053bc), 0, 7);
    H_MEMSET(V(0x208053c2), 0, 0x1d);
    H_MEMSET(M32(V(0x208053e4)), 0, 0x16);
    H_MEMSET(M32(V(0x208053f8)), 0, 0x16);
    H_MEMSET(V(0x20805444), 0, 6);
    H_MEMSET(V(0x2080544a), 0, 0x1c);
    H_MEMSET(M32(V(0x2080546c)), 0, 0x16);
    H_MEMSET(M32(V(0x20805480)), 0, 0x16);
    H_MEMSET(V(0x208054a4), 0, 6);
    H_MEMSET(V(0x208054aa), 0, 0x1c);
    H_MEMSET(M32(V(0x208054f8)), 0, 0x16);
    H_MEMSET(M32(V(0x2080550c)), 0, 0x16);
    if (M16(V(0x208053a4)) > 0) {
        H_STRCPY(V(0x208053bc), M32(M32(G + 0x384)) + 0x22);
        H_STRCPY(V(0x208053c2), M32(M32(G + 0x384)) + 0x28);
        UTIL_GET(V(0x208056c8), 2, RD_DLG);
    }
    if (M16(V(0x2080542c)) > 0) {
        H_STRCPY(V(0x20805444), M32(M32(G + 0x384) + 4) + 0x22);
        H_STRCPY(V(0x2080544a), M32(M32(G + 0x384) + 4) + 0x28);
        UTIL_GET(V(0x208056f0), 2, RD_DLG);
    }
    if (M16(V(0x208054f2)) > 0) {
        H_STRCPY(V(0x208054a4), M32(M32(G + 0x384) + 8) + 0x1a);
        H_STRCPY(V(0x208054aa), M32(M32(G + 0x384) + 8) + 0x20);
        UTIL_GET(V(0x20805718), 2, RD_DLG);
    }
    gp = G;
    M32(V(0x208054ec)) = M32(gp + 0x35c) == 0 ? V(0x20805344) : V(0x20805348);
    M32(V(0x20805498)) = M8(gp + 0xb64) != 0 && M8(gp + 0xb6f) != 0;
    M32(V(0x208054a0)) = M8(gp + 0xb64) != 0;
    M32(V(0x2080549c)) = M8(gp + 0xb6f) != 0;
    M32(V(0x20805378)) = M8(gp + 0xb70) != 0;
    M32(V(0x20805524)) = M8(gp + 0xb70) != 0;
    if (M32(V(0x20805564)) == 0)
        M32(V(0x20805554)) = M32(V(0x20805408)) != 0 ? 4 : 2;
    if (M8(gp + 0xb70) == 0) {
        uint16_t ax = M16(V(0x208053a4)), dx = M16(V(0x2080542c));

        M32(V(0x2080555c)) = M32(V(0x20805408)) != 0;
        M32(V(0x20805558)) = ax > 0 || dx > 0;
        M32(V(0x20805568)) = ax > 0;
        M32(V(0x2080556c)) = dx > 0;
    }
    return 0;
}

/* .text+0x13ea0 */
static uint32_t rd_apply(void)
{
    uint32_t gp, r;
    uint8_t dl, bl, cl;

    UTIL_PUT(V(0x208056a0), 2, RD_DLG, 0);
    gp = G;
    M16(gp + 0xc4e) = (uint16_t)((((uint32_t)M16(V(0x208091d8)) + 0x5b) * 360) >> 0x10);
    M16(G + 0xc5e) = (uint16_t)(((uint32_t)M16(V(0x208091e4)) + 0x5b) * 360 >> 0x10);
    r = UTIL_PUT(V(0x20805588), 0xe, RD_DLG, 0);
    gp = G;
    M16(gp + 0x37e) = M16(RD_NAV2);
    M16(gp + 0x34c) = M16(RD_COM);
    M16(gp + 0x356) = M16(RD_COM2);
    M8(gp + 0x388) |= 2;
    M8(gp + 0x389) |= 2;
    if (M16(gp + 0xe5a) != 0)
        M8(gp + 0xe62) |= 1;
    dl = M8(gp + 0xc68);
    bl = M8(V(0x208091e0));
    if (dl == bl && M8(V(0x208091d0)) == M8(gp + 0xc58))
        return r;
    if (M32(V(0x20805374)) == 0) {
        H_POST(0x80001, 0);
        return r;
    }
    if (M32(V(0x20805374)) != 1)
        return r;
    cl = M8(V(0x208091d0));
    /* each is posted once, twice when the other radio's switch went the same way */
    if (bl == 0) {
        if (cl == 0)
            dl = M8(gp + 0xc58);
    } else {
        if (cl != 0)
            return r;
        if (M8(gp + 0xc58) != 0) {
            H_POST(0x80001, 0);
            H_POST(0x80001, 0);
        } else {
            H_POST(0x80001, 0);
        }
        return r;
    }
    H_POST(0x80001, 0);
    if (dl == 0)
        H_POST(0x80001, 0);
    return r;
}

/* .text+0x14300: both radios off is refused; switching one on takes the other off. */
static uint32_t rd_switches(uint32_t b)
{
    uint32_t c = M32(b + 0xc), x = M32(b + 0x12c), y = M32(b + 0x1b8);

    if (c == 0) {
        if ((x != 0 && y != 0) || (x == 0 && y == 0)) {
            M32(b + 0x128) = 0x1000;
            M32(b + 0x1b4) = 0x1000;
            return 0xffffffffu;
        }
    } else if (c == 1 && x != 0 && y != 0) {
        if (M8(V(0x208091d0)) != 0)
            M32(b + 0x12c) = 0;
        else
            M32(b + 0x1b8) = 0;
    }
    return 0;
}

/* .text+0x14050: the check */
static uint32_t FS98_STDCALL rd_check(uint32_t b)
{
    static const uint16_t range[5][2] = { { 0xac, 0xa8 }, { 0x1cc, 0x1c8 }, { 0x1d4, 0x1d0 }, { 0x1dc, 0x1d8 }, { 0x1e4, 0x1e0 } };
    uint32_t r = UTIL_PUT(V(0x20805588), 0xe, b, 1), i;

    if (r != 0)
        return r;
    rd_nav_fields(b);
    M16(b + 0xa4) = M16(RD_NAV2);
    M32(b + 0xa0) = M8(V(0x208091dc)) & 0x80;
    r = rd_adf_parse(b + 0x174, RD_COM, RD_COM2);
    if (r != 0)
        return r;
    rd_adf_fields(b);
    if (M32(G + 0x35c) != 0)
        rd_adf_text(b + 0x174, M16(RD_COM), M16(RD_COM2));
    r = rd_switches(b);
    if (r != 0)
        return r;
    r = UTIL_PUT(V(0x208056a0), 2, b, 0);
    if (r != 0)
        return r;
    for (i = 0; i < 5; i++)
        if ((int32_t)M32(b + range[i][0]) < 0 || (int32_t)M32(b + range[i][0]) > 2) {
            M32(b + range[i][1]) = 0x1005;
            return 0xffffffffu;
        }
    if (M32(b + 0x1ec) != 2 && M32(b + 0x1ec) != 4) {
        M32(b + 0x1e8) = 0x1005;
        return 0xffffffffu;
    }
    if ((int32_t)M32(b + 0x1fc) < 0 || (int32_t)M32(b + 0x1fc) > 1) {
        M32(b + 0x1d0) = 0x1005;
        return 0xffffffffu;
    }
    if (M32(V(0x20805564)) == 0)
        M32(V(0x20805554)) = M32(V(0x20805408)) != 0 ? 4 : 2;
    if (M32(b + 0x1bc) == 0) {
        M32(b + 0x1f4) = M32(b + 0xa0) != 0;
        M32(b + 0x1f0) = 1;
        if (M16(b + 0x3c) == 0 && M16(b + 0xc4) == 0) {
            M32(b + 0x1f0) = 0;
            M32(b + 0x200) = 0;
            M32(b + 0x204) = 0;
        } else {
            M32(b + 0x200) = M16(b + 0x3c) > 0;
            M32(b + 0x204) = M16(b + 0xc4) > 0;
        }
    }
    return 0;
}

/* .text+0x13870, command 0x1202. Cancelled, the station lookups are put back. */
static uint32_t command_1202(void)
{
    uint32_t r = rd_load();

    if (r != 0)
        return r;
    r = H_DIALOG(0x1202, RD_DLG);
    if (r != 0)
        return r;
    if (M32(RD_DLG) == 1)
        return rd_apply();
    UTIL_GET(V(0x20805588), 6, RD_DLG);
    rd_nav_fields(RD_DLG);
    M16(G + 0x37e) = M16(RD_NAV2);
    rd_adf_fields(RD_DLG);
    return 0;
}

/* .text+0x17ca0: the preview's click or key turns the card it shows: the arrows by
 * 0x200 toward the nearest side, the numeric pad to the eight points. */
static uint32_t FS98_STDCALL vw_mouse(uint32_t p, uint32_t flags)
{
    uint32_t heading = M32(V(0x2080d0ac)), key;
    uint16_t ax;

    if (flags != 0x100000) {
        M16(heading) = (uint16_t)CALL(UTIL, 0x3c, f2)(M32(p) - (uint32_t)((int32_t)M32(V(0x2080d068)) >> 1),
                                                     (uint32_t)((int32_t)M32(V(0x2080d06c)) >> 1) - M32(p + 4));
        return 1;
    }
    key = M32(p);
    ax = M16(heading);
    switch (key) {
    case 0x25: M16(heading) = (uint16_t)(ax < 0x4000 || ax >= 0xc000 ? ax - 0x200 : ax + 0x200); break;
    case 0x27: M16(heading) = (uint16_t)(ax < 0x4000 || ax >= 0xc000 ? ax + 0x200 : ax - 0x200); break;
    case 0x28: M16(heading) = (uint16_t)(ax < 0x8000 ? ax + 0x200 : ax - 0x200); break;
    case 0x26: M16(heading) = (uint16_t)(ax < 0x8000 ? ax - 0x200 : ax + 0x200); break;
    case 0x61: M16(heading) = 0xa000; break;
    case 0x62: M16(heading) = 0x8000; break;
    case 0x63: M16(heading) = 0x6000; break;
    case 0x64: M16(heading) = 0xc000; break;
    case 0x66: M16(heading) = 0x4000; break;
    case 0x67: M16(heading) = 0xe000; break;
    case 0x68: M16(heading) = 0; break;
    case 0x69: M16(heading) = 0x2000; break;
    }
    return 1;
}
