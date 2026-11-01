/* MAIN.DLL: command 0x1301, Set Time and Season.  MSVC C, .text+0x1b20..+0x2e20,
 * with +0x2660 (.text+0x1a90 uses it too): the date as a day of the year and back.
 *
 * The dialog's five field callbacks at .data+0x418 keep local time, zulu time, the
 * date and the season agreeing as each is edited. The time of day buttons pick a
 * time by asking +0xa0 what light it gives. */

#define CD_DLG      V(0x20800308)
#define CD_MINUTE   V(0x20807fd8)
#define CD_DAY      V(0x20807fdc)
#define CD_YEAR     V(0x20807fe0)
#define CD_ZMIN     V(0x20807fe4)
#define CD_ZHOUR    V(0x20807fe8)
#define CD_LIGHT    V(0x20807ff0)
#define CD_MONTH    V(0x20807ff4)
#define CD_BUTTON   V(0x20807ff8)
#define CD_HOUR     V(0x20808004)
#define CD_SEASON   V(0x2080800c)
#define CD_YDAY     V(0x20808010)

/* The month lengths for year y, as retail's address: an index past them reads on
 * through .data, as retail's process holds it (dst_word). */
static uint32_t date_table(uint32_t y)
{
    if ((int32_t)y % 400 == 0 || ((int32_t)y % 100 != 0 && (int32_t)y % 4 == 0))
        return 0x208002e0u;
    return 0x208002c8u;
}

/* .text+0x2660: the day of the year to a month and day. A day past the year's end
 * is brought round first, and stored. */
static uint32_t FS98_STDCALL date_from_yday(uint32_t yday, uint32_t year, uint32_t month, uint32_t day)
{
    uint32_t table = date_table(M16(year)), limit = table == 0x208002e0u ? 0x16e : 0x16d;
    uint16_t cx, ax;
    uint8_t m = 0;

    if (M16(yday) > limit)
        M16(yday) = (uint16_t)(M16(yday) - limit);
    cx = M16(yday);
    ax = dst_word(table);
    while (cx > ax) {
        cx = (uint16_t)(cx - ax);
        m++;
        ax = dst_word(table + m * 2u);
    }
    m++;
    M8(month) = m;
    M16(day) = cx;
    if (m > 0xc || cx > 0x1f || cx < 1)
        return 0xffffffffu;
    return 0;
}

/* .text+0x2730: and back. */
static uint32_t FS98_STDCALL date_to_yday(uint32_t yday, uint32_t year, uint32_t month, uint32_t day)
{
    uint32_t table = date_table(M16(year)), m = M8(month), i;
    uint16_t cx = 0;

    if (m > 1)
        for (i = 0; i < m - 1; i++)
            cx = (uint16_t)(cx + dst_word(table + i * 2));
    cx = (uint16_t)(cx + M16(day));
    if (cx < 1 || cx > 0x16e || (table == 0x208002c8u && cx == 0x16e))
        return 0xffffffffu;
    M16(yday) = cx;
    return 0;
}

/* .text+0x27d0: the light (GLOBAL +0x116e's value) to the button that shows it.
 * Its argument is not used. */
static uint32_t FS98_STDCALL clock_light_button(uint32_t unused)
{
    uint16_t cx = M16(CD_LIGHT);

    (void)unused;
    if (cx == 1) {
        M16(CD_BUTTON) = cx;
        return 0;
    }
    if (cx == 4) {
        M16(CD_BUTTON) = 3;
        return 0;
    }
    if (cx == 2) {
        M16(CD_BUTTON) = M8(CD_HOUR) < 0xc ? 0 : 2;
        return 0;
    }
    return 0xffffffffu;
}

/* .text+0x2d30: the hour and minute with the zone added. */
static void FS98_STDCALL clock_add_zone(uint32_t hour, uint32_t minute, uint32_t out_hour, uint32_t out_minute)
{
    int32_t v = (int16_t)(uint16_t)(M16(G + 0x246) + (uint16_t)(uint8_t)minute);
    int8_t h = (int8_t)(v / 60), m = (int8_t)(v % 60);

    if (m < 0) {
        m = (int8_t)(m + 0x3c);
        h = (int8_t)(h - 1);
    }
    M8(out_minute) = (uint8_t)m;
    h = (int8_t)(h + (int8_t)hour);
    if (h < 0)
        h = (int8_t)(h + 0x18);
    else if (h >= 0x18)
        h = (int8_t)(h - 0x18);
    M8(out_hour) = (uint8_t)h;
}

/* .text+0x2da0: and taken away. */
static void FS98_STDCALL clock_sub_zone(uint32_t hour, uint32_t minute, uint32_t out_hour, uint32_t out_minute)
{
    int32_t v = (int16_t)(uint16_t)((uint16_t)(uint8_t)minute - M16(G + 0x246));
    int8_t h = (int8_t)(v / 60), m = (int8_t)(v % 60);

    if (m < 0) {
        m = (int8_t)(m + 0x3c);
        h = (int8_t)(h - 1);
    }
    M8(out_minute) = (uint8_t)m;
    h = (int8_t)(h + (int8_t)hour);
    if (h < 0)
        h = (int8_t)(h + 0x18);
    else if (h >= 0x18)
        h = (int8_t)(h - 0x18);
    M8(out_hour) = (uint8_t)h;
}

/* .text+0x2820: the light at the zulu time in the dialog. */
static uint32_t clock_light_now(uint32_t arg)
{
    uint8_t h = M8(G + 0x23b), m = M8(G + 0x23c);

    M8(G + 0x23b) = M8(CD_ZHOUR);
    M8(G + 0x23c) = M8(CD_ZMIN);
    nav_sun();
    M8(G + 0x23b) = h;
    M8(G + 0x23c) = m;
    M16(CD_LIGHT) = M16(G + 0x116e);
    return clock_light_button(arg);
}

/* .text+0x24d0: and the buttons shown for it, the date left alone. */
static uint32_t clock_show_light(uint32_t block)
{
    uint8_t h = M8(G + 0x23b), m = M8(G + 0x23c);
    uint16_t year = M16(G + 0x240), day = M16(G + 0x23e);
    uint32_t r;

    M8(G + 0x23b) = M8(CD_ZHOUR);
    M8(G + 0x23c) = M8(CD_ZMIN);
    UTIL_PUT(V(0x20800560), 2, CD_DLG, 0);
    nav_sun();
    M16(CD_LIGHT) = M16(G + 0x116e);
    r = clock_light_button(block + 0x74);
    if (r == 0) {
        UTIL_GET(V(0x20800480), 1, block);
        UTIL_GET(V(0x20800548), 1, block);
    }
    M8(G + 0x23b) = h;
    M8(G + 0x23c) = m;
    M16(G + 0x23e) = day;
    M16(G + 0x240) = year;
    return r;
}

/* .text+0x28a0: the season the day of the year falls in, turned round in the
 * southern hemisphere. */
static uint32_t clock_season_of(uint32_t season, uint32_t yday)
{
    uint16_t cx = M16(yday);
    uint8_t al = 0;

    if (cx >= dst_word(0x208002f8u))
        do {
            al++;
        } while (cx >= dst_word(0x208002f8u + al * 2u));
    al &= 3;
    M16(season) = al;
    if ((int32_t)M32(G + 0x564) < 0)
        M16(season) = (uint8_t)(al - 2) & 3;
    return 0;
}

/* One try of .text+0x2900's search: the light at this local time. */
static uint16_t clock_try(int8_t hour, int8_t minute)
{
    clock_add_zone((uint8_t)hour, (uint8_t)minute, G + 0x23b, G + 0x23c);
    nav_sun();
    return M16(G + 0x116e);
}

/* The minute a step on, or back, carrying into the hour. */
static void clock_up(int8_t *hour, int8_t *minute, int by)
{
    *minute = (int8_t)(*minute + by);
    if (*minute >= 0x3c) {
        *hour = (int8_t)(*hour + 1);
        *minute = 0;
    }
}

static void clock_down(int8_t *hour, int8_t *minute, int by, int8_t wrap)
{
    *minute = (int8_t)(*minute - by);
    if (*minute < 0) {
        *hour = (int8_t)(*hour - 1);
        *minute = wrap;
    }
}

/* .text+0x2900: the time of day buttons: walk from noon by fives until the light
 * changes, then by ones (five at most) to the minute it does. */
static uint32_t clock_pick_time(uint32_t out_hour, uint32_t out_minute, uint32_t out_zhour, uint32_t out_zmin, uint32_t button)
{
    uint8_t zh = M8(G + 0x23b), zm = M8(G + 0x23c);
    uint16_t day = M16(G + 0x23e), year = M16(G + 0x240), mode;
    int8_t hour = 0xc, minute = 0;
    uint32_t r = UTIL_PUT(V(0x20800560), 2, CD_DLG, 0), i;

    mode = M16(button);

    if (mode == 0) {                                 /* dawn: back to night, on to light */
        for (;;) {
            clock_down(&hour, &minute, 5, 0x37);
            if (clock_try(hour, minute) == 4)
                break;
            if ((uint8_t)hour == 1)
                goto done;
        }
        for (i = 0; i < 5; i++) {
            clock_up(&hour, &minute, 1);
            if (clock_try(hour, minute) == 2)
                break;
        }
    } else if (mode == 1) {                          /* day */
        for (;;) {
            clock_down(&hour, &minute, 5, 0x37);
            if (clock_try(hour, minute) == 2)
                break;
            if ((uint8_t)hour == 2)
                goto done;
        }
        for (i = 0; i < 5; i++) {
            clock_up(&hour, &minute, 1);
            if (clock_try(hour, minute) == 1)
                break;
        }
    } else if (mode == 2) {                          /* dusk */
        for (;;) {
            clock_up(&hour, &minute, 5);
            if (clock_try(hour, minute) == 2)
                break;
            if ((uint8_t)hour == 0x16)
                goto done;
        }
        for (i = 0; i < 5; i++) {
            clock_down(&hour, &minute, 1, 0x3b);
            if (clock_try(hour, minute) == 1) {
                clock_up(&hour, &minute, 1);
                break;
            }
        }
    } else if (mode == 3) {                          /* night */
        for (;;) {
            clock_up(&hour, &minute, 5);
            if (clock_try(hour, minute) == 4)
                break;
            if ((uint8_t)hour == 0x17)
                goto done;
        }
        for (i = 0; i < 5; i++) {
            clock_down(&hour, &minute, 1, 0x3b);
            if (clock_try(hour, minute) == 2) {
                clock_up(&hour, &minute, 1);
                break;
            }
        }
    } else {
        r = 0xffffffffu;
    }
done:
    M8(G + 0x23b) = zh;
    M8(G + 0x23c) = zm;
    M16(G + 0x23e) = day;
    M16(G + 0x240) = year;
    M8(out_hour) = (uint8_t)hour;
    M8(out_minute) = (uint8_t)minute;
    clock_add_zone((uint8_t)hour, (uint8_t)minute, out_zhour, out_zmin);
    return r;
}

/* The tail .text+0x1f70 and +0x21a0 share: zulu and the season pushed through +0x74,
 * and everything shown again. `a` to `d` are the fields to put back. */
static void clock_after_date(uint32_t block, uint8_t hour_was, const uint32_t at[4], const uint8_t was[4], uint16_t day, uint16_t light)
{
    uint32_t i;

    M8(G + 0x23b) = M8(CD_ZHOUR);
    M8(G + 0x23c) = M8(CD_ZMIN);
    nav_season(0, M16(CD_YDAY));
    UTIL_GET(V(0x20800628), 1, block);
    if (hour_was != M8(G + 0x238)) {
        M8(CD_ZHOUR) = M8(G + 0x23b);
        M8(CD_ZMIN) = M8(G + 0x23c);
        M8(CD_HOUR) = M8(G + 0x238);
        M8(CD_MINUTE) = M8(G + 0x239);
        UTIL_GET(V(0x20800494), 4, block);
    }
    for (i = 0; i < 4; i++)
        M8(G + at[i]) = was[i];
    M16(G + 0x23e) = M16(CD_YDAY);
    clock_light_now(block + 0x74);
    M16(G + 0x23e) = day;
    if (M16(CD_LIGHT) != light) {
        UTIL_GET(V(0x20800480), 1, block);
        UTIL_GET(V(0x20800548), 1, block);
    }
}

/* .text+0x1f70: the season field. */
static uint32_t FS98_STDCALL clock_field_season(uint32_t block)
{
    static const uint32_t at[4] = { 0x238, 0x239, 0x23b, 0x23c };
    uint8_t was[4], hour = M8(CD_HOUR);
    uint16_t day = M16(G + 0x23e), light = M16(CD_LIGHT);
    uint32_t r, i;

    for (i = 0; i < 4; i++)
        was[i] = M8(G + at[i]);
    r = UTIL_PUT(V(0x208004e4), 1, block, 0);
    if (r != 0)
        return r;
    if ((int32_t)M32(G + 0x564) < 0)
        M16(CD_SEASON) = (uint8_t)(M8(CD_SEASON) - 2) & 3;
    M16(CD_YDAY) = (uint16_t)(dst_word(0x208002f8u + M16(CD_SEASON) * 2u) - 0x2f);
    r = date_from_yday(CD_YDAY, CD_YEAR, CD_MONTH, CD_DAY);
    if (r != 0)
        return r;
    UTIL_GET(V(0x20800458), 1, block);
    UTIL_GET(V(0x20800520), 2, block);
    clock_after_date(block, hour, at, was, day, light);
    return 0;
}

/* .text+0x21a0: the date fields. */
static uint32_t FS98_STDCALL clock_field_date(uint32_t block)
{
    static const uint32_t at[4] = { 0x23b, 0x23c, 0x238, 0x239 };
    uint8_t was[4], hour = M8(CD_HOUR);
    uint16_t day = M16(G + 0x23e), light = M16(CD_LIGHT);
    uint32_t r, i;

    for (i = 0; i < 4; i++)
        was[i] = M8(G + at[i]);
    r = UTIL_PUT(V(0x20800444), 1, block, 0);
    if (r != 0)
        return r;
    r = UTIL_PUT(V(0x20800520), 2, block, 0);
    if (r != 0)
        return r;
    r = date_to_yday(CD_YDAY, CD_YEAR, CD_MONTH, CD_DAY);
    if (r != 0)
        return r;
    UTIL_GET(V(0x20800458), 1, block);
    r = clock_season_of(CD_SEASON, CD_YDAY);
    if (r != 0)
        return r;
    UTIL_GET(V(0x208004e4), 1, block);
    clock_after_date(block, hour, at, was, day, light);
    return 0;
}

/* .text+0x23c0: the light buttons. Any but 1 and 3 ask for 2. */
static uint32_t FS98_STDCALL clock_field_button(uint32_t block)
{
    uint32_t r = UTIL_PUT(V(0x20800548), 1, block, 0);
    uint16_t ax;

    if (r != 0)
        return r;
    ax = M16(CD_BUTTON);
    if (ax == 1)
        M16(CD_LIGHT) = ax;
    else
        M16(CD_LIGHT) = ax == 3 ? 4 : 2;
    UTIL_GET(V(0x20800480), 1, block);
    r = clock_pick_time(CD_HOUR, CD_MINUTE, CD_ZHOUR, CD_ZMIN, CD_BUTTON);
    if (r != 0)
        return r;
    UTIL_GET(V(0x20800494), 4, block);
    return 0;
}

/* .text+0x2460: local time edited. */
static uint32_t FS98_STDCALL clock_field_local(uint32_t block)
{
    uint32_t r = UTIL_PUT(V(0x20800494), 2, block, 0);

    if (r != 0)
        return r;
    clock_add_zone(M8(CD_HOUR), M8(CD_MINUTE), CD_ZHOUR, CD_ZMIN);
    UTIL_GET(V(0x208004bc), 2, block);
    UTIL_GET(V(0x20800494), 1, block);
    clock_show_light(block);
    return 0;
}

/* .text+0x2600: zulu time edited. */
static uint32_t FS98_STDCALL clock_field_zulu(uint32_t block)
{
    uint32_t r = UTIL_PUT(V(0x208004bc), 2, block, 0);

    if (r != 0)
        return r;
    clock_sub_zone(M8(CD_ZHOUR), M8(CD_ZMIN), CD_HOUR, CD_MINUTE);
    UTIL_GET(V(0x20800494), 2, block);
    clock_show_light(block);
    return 0;
}

/* .text+0x1cb0: the date's checks; the statuses mark the field, and +0x40 says why. */
static uint32_t FS98_STDCALL clock_check_date(uint32_t day, uint32_t month, uint32_t year, uint32_t block)
{
    uint8_t m = (uint8_t)month;
    uint16_t d = (uint16_t)day;
    uint32_t table;

    if (m < 1) {
        M32(block + 0x1c) = 0xc;
        return 0xffffffffu;
    }
    if (m > 0xc) {
        M32(block + 0x1c) = 0xb;
        return 0xffffffffu;
    }
    table = date_table((uint16_t)year);
    if (d < 1) {
        M32(block + 0x30) = 0xc;
        return 0xffffffffu;
    }
    if (d > dst_word(table - 2 + m * 2u)) {
        M32(block + 0x30) = 0xb;
        switch (m) {
        case 2:
            M32(block + 0x40) = V(table == 0x208002e0u ? 0x208002ac : 0x208002a8);
            break;
        case 4: case 6: case 9: case 11:
            M32(block + 0x40) = V(0x208002b0);
            break;
        default:
            M32(block + 0x40) = V(0x208002b4);
            break;
        }
        return 0xffffffffu;
    }
    return 0;
}

/* .text+0x1df0: the dialog's check. Its +0xc says which field moved last; a date
 * that objects objects again here with the status it set. */
static uint32_t FS98_STDCALL clock_check(uint32_t block)
{
    uint32_t r = 0, which = M32(block + 0xc);

    if (which == 1 || which == 2) {
        r = clock_field_local(block);
    } else if (which == 3 || which == 4) {
        r = clock_field_zulu(block);
    } else if (which >= 5 && which <= 7) {
        r = clock_field_date(block);
        if (r != 0)
            clock_check_date(M16(CD_DAY), M8(CD_MONTH), M16(CD_YEAR), block);
    }
    if (r != 0)
        return r;
    r = UTIL_PUT(V(0x20800430), 0xf, block, 0);
    if (r != 0)
        return r;
    r = clock_check_date(M16(CD_DAY), M8(CD_MONTH), M16(CD_YEAR), block);
    if (r != 0)
        return r;
    if ((int32_t)M32(block + 0x70) < 0 || (int32_t)M32(block + 0x70) > 3) {
        M32(block + 0x6c) = 1;
        return 0xffffffffu;
    }
    if ((int32_t)M32(block + 0xac) <= 0 || ((int32_t)M32(block + 0xac) > 2 && M32(block + 0xac) != 4)) {
        M32(block + 0xa8) = 1;
        return 0xffffffffu;
    }
    if (M8(CD_HOUR) > 0x17) {
        M32(block + 0xb0) = 0xb;
        return 0xffffffffu;
    }
    if (M8(CD_MINUTE) > 0x3b) {
        M32(block + 0xc4) = 0xb;
        return 0xffffffffu;
    }
    if (M8(CD_ZHOUR) > 0x17) {
        M32(block + 0xd8) = 0xb;
        return 0xffffffffu;
    }
    if (M8(CD_ZMIN) > 0x3b) {
        M32(block + 0xec) = 0xb;
        return 0xffffffffu;
    }
    return 0;
}

/* .text+0x1b60: the dialog's fields from GLOBAL. */
static uint32_t clock_load(void)
{
    uint8_t text[4];
    uint32_t r;

    M32(CD_DLG) = 0;
    UTIL_GET(V(0x20800560), 0xb, CD_DLG);
    M32(V(0x2080040c)) = 0;
    r = UTIL_PUT(V(0x20800430), 0xc, CD_DLG, 0);
    if (r != 0)
        return r;
    H_LOADSTRING(M32(V(0x2080039c)) == 0 ? 0xf4c0 : 0xf4c1, V(0x2080038d), 9);
    N_PUT(V(0x20807fe8), 7, ADDR(text));
    H_STRCPY(V(0x20800384), ADDR(text + 1));
    HOST_(0x2b0, f2)(V(0x20800384), M32(V(0x20800380)) + 2);
    r = date_from_yday(CD_YDAY, CD_YEAR, CD_MONTH, CD_DAY);
    if (r != 0)
        return r;
    r = clock_light_button(V(0x2080037c));
    if (r != 0)
        return r;
    UTIL_GET(V(0x20800520), 3, CD_DLG);
    return 0;
}

/* .text+0x1c80 */
static void clock_apply(void)
{
    if (M32(V(0x2080040c)) != 0)
        M32(V(0x20800414)) = 0;
    UTIL_PUT(V(0x20800560), 0xb, CD_DLG, 0);
}

/* .text+0x1b20, command 0x1301. +0x2e20, which it calls last, is a bare ret. */
static uint32_t command_1301(void)
{
    if (clock_load() == 0 && H_DIALOG(0x1301, CD_DLG) == 0 && M32(CD_DLG) == 1)
        clock_apply();
    return M32(CD_DLG);
}
