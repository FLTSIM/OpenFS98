/* MAIN.DLL: command 0x1101, Create a Flight. MSVC C, .text+0xcaa0, +0xcef0, +0xd7a0..
 * +0xda60, +0xdb60, +0x1a90. It saves the situation aside, lets the other dialogs change
 * it through its buttons, and either saves the result as a flight or restores what it
 * saved. */

#define CR_DESC V(0x208086c8)

/* .text+0x1a90: the date and time as text */
static uint32_t cr_when(void)
{
    uint16_t year = M16(G + 0x240), yday = M16(G + 0x23e), day = 0;
    uint8_t month = 0;

    date_from_yday(ADDR(&yday), ADDR(&year), ADDR(&month), ADDR(&day));
    wsprintfA((char *)(uintptr_t)V(0x20808068), (const char *)(uintptr_t)V(0x20800664), (int)month, (int)day, (int)year, (int)M8(G + 0x238), (int)M8(G + 0x239));
    return V(0x20808068);
}

/* .text+0xdb60: the weather as text */
static void cr_weather_text(void)
{
    char *out = (char *)(uintptr_t)V(0x2080d160);
    uint16_t cover = 0;
    uint32_t fmt = 0, at, v, q, rem, tenth = 0;
    char a[0x10], b[0x10];

    M8(V(0x2080d160)) = 0;
    cover = M16(G + 0xf72);
    if (cover == 0) {
        H_LOADSTRING(0x2030, V(0x2080d160), 0x800 - H_STRLEN(V(0x2080d160)));
    } else if (cover <= 5) {
        H_LOADSTRING(0x2031, V(0x2080d160), 0x800 - H_STRLEN(V(0x2080d160)));
    } else {
        N_PUT(G + 0xf74, 0x4c, ADDR(a));
        N_PUT(G + 0xf72, 0x21, ADDR(b));
        HOST_(0x2a4, f2)(0x2032, ADDR(&fmt));
        wsprintfA(out, (const char *)(uintptr_t)fmt, a, b);
        H_FREE(ADDR(&fmt));
    }
    at = V(0x2080d160) + H_STRLEN(V(0x2080d160));
    v = M16(G + 0xf8c);
    q = v / 100;
    rem = v % 100;
    if (q > 0x28) {
        H_LOADSTRING(0x2040, at, 0x800 - H_STRLEN(V(0x2080d160)));
        return;
    }
    if (q > 3) {
        HOST_(0x2a4, f2)(0x2041, ADDR(&fmt));
        wsprintfA((char *)(uintptr_t)at, (const char *)(uintptr_t)fmt, (int)q);
        return;
    }
    if (rem >= 0xd) {
        if (rem < 0x26)
            tenth = 0x19;
        else if (rem < 0x3f)
            tenth = 0x32;
        else if (rem < 0x58)
            tenth = 0x4b;
        else
            q++;
    }
    HOST_(0x2a4, f2)(0x2042, ADDR(&fmt));
    wsprintfA((char *)(uintptr_t)at, (const char *)(uintptr_t)fmt, (int)q, (int)tenth);
}

/* .text+0xcef0: the flight's description */
static void cr_description(uint32_t b, uint32_t out)
{
    static const uint16_t ids[5] = { 0x2050, 0x2051, 0x2052, 0x2053, 0x2054 };
    char t[0x100];
    uint32_t i;

    H_LOADSTRING(0x2000, out, 0x100);
    H_LOADSTRING(0x2001, ADDR(t), 0x100);
    H_STRCAT(out, ADDR(t));
    H_STRCAT(out, M32(b + 0x1c));
    H_LOADSTRING(0x2002, ADDR(t), 0x100);
    H_STRCAT(out, ADDR(t));
    for (i = 0; i < 5; i++)
        if (M32(V(0x20808ec8) + i * 4) != 0) {
            H_LOADSTRING(ids[i], ADDR(t), 0x100);
            H_STRCAT(out, ADDR(t));
            H_STRCAT(out, M32(V(0x20808ec8) + i * 4));
        }
    H_LOADSTRING(0x2003, ADDR(t), 0x100);
    H_STRCAT(out, ADDR(t));
    H_STRCAT(out, M32(b + 0x24));
    H_LOADSTRING(0x2004, ADDR(t), 0x100);
    H_STRCAT(out, ADDR(t));
    H_STRCAT(out, M32(b + 0x28));
}

/* .text+0xd7a0: Aircraft */
static uint32_t FS98_STDCALL cr_aircraft(uint32_t b)
{
    char path[0x104];

    if (command_1201() != 0) {
        M32(b + 0x30) = 1;
        M32(b + 0x2c) = M32(b + 0x34) != 0;
        CALL(ACONTAIN, 0x30, f4)(G + 0x11e0, 1, ADDR(path), 0x104);
        H_FREE(b + 0x1c);
        M32(b + 0x1c) = HOST_(0x2b4, f1)(ADDR(path));
    }
    return 0;
}

/* .text+0xd820: Location (FACIL's dialog); its five strings are replaced by it */
static uint32_t FS98_STDCALL cr_location(uint32_t b)
{
    uint32_t held[6], p = 0, n, i;

    for (i = 0; i < 6; i++)
        held[i] = M32(V(0x20808ec8) + i * 4);
    if (CALL(FACIL_, 0x24, f1)(V(0x20808ec8)) != 0) {
        for (i = 0; i < 6; i++)
            if (held[i] != 0)
                H_FREE(ADDR(&held[i]));
        M32(b + 0x34) = 1;
        M32(b + 0x2c) = M32(b + 0x30) != 0;
        n = H_STRLEN(M32(V(0x20808ed8)));
        n += H_STRLEN(M32(V(0x20808edc)));
        if (n != 0) {
            H_ALLOC(1, n + 6, ADDR(&p), 1);
            if (M32(V(0x20808ed8)) != 0) {
                H_STRCAT(p, M32(V(0x20808ed8)));
                H_STRCAT(p, V(0x20803648));
            }
            if (M32(V(0x20808edc)) != 0)
                H_STRCAT(p, M32(V(0x20808edc)));
        } else {
            HOST_(0x2a4, f2)(0x2020, ADDR(&p));
        }
        H_FREE(b + 0x20);
        M32(b + 0x20) = p;
        nav_season(0, M16(G + 0x23e));
    }
    return 0;
}

/* .text+0xda00: Weather */
static uint32_t FS98_STDCALL cr_weather(uint32_t b)
{
    (void)b;
    if (command_1306() != 0)
        cr_weather_text();
    return 0;
}

/* .text+0xda20: Time */
static uint32_t FS98_STDCALL cr_time(uint32_t b)
{
    if (command_1301() != 0) {
        H_FREE(b + 0x28);
        M32(b + 0x28) = HOST_(0x2b4, f1)(cr_when());
    }
    return 0;
}

/* .text+0xda60: Save */
static uint32_t FS98_STDCALL cr_save(uint32_t b)
{
    cr_description(b, CR_DESC);
    flight_save_dialog();
    return 0;
}

/* .text+0xcaa0, command 0x1101 */
static uint32_t command_1101(void)
{
    uint8_t aside[0x108], title[0x14], dir[0x104];
    char text[0x100];
    struct { uint32_t ext; char path[0x200]; } flight;
    volatile uint32_t b[14];
    uint32_t i, r;

    memcpy(title, (const void *)(uintptr_t)V(0x20803620), 0x11);
    H_LOADSTRING(0x9002, ADDR(dir), 0x104);
    H_JOIN(ADDR(flight.path), 0, ADDR(dir), ADDR(title));
    flight.ext = 2;
    memcpy(aside, (const void *)(uintptr_t)V(0x208040d8), 0x108);
    save_flight(ADDR(title), V(0x20807fd0));
    memcpy((void *)(uintptr_t)V(0x208040d8), aside, 0x108);
    b[0] = 0;
    b[1] = ADDR(main_nothing);
    b[2] = ADDR(cr_save);
    b[3] = ADDR(cr_aircraft);
    b[4] = ADDR(cr_location);
    b[5] = ADDR(cr_weather);
    b[6] = ADDR(cr_time);
    HOST_(0x2a4, f2)(0x2010, ADDR(&b[7]));
    HOST_(0x2a4, f2)(0x2011, ADDR(&b[8]));
    b[9] = V(0x2080d160);
    cr_weather_text();
    b[10] = HOST_(0x2b4, f1)(cr_when());
    memcpy(aside, (const void *)(uintptr_t)V(0x208040d8), 0x108);
    b[11] = 0;
    b[12] = 0;
    b[13] = 0;
    H_DIALOG(0x1101, ADDR(b));
    memcpy((void *)(uintptr_t)V(0x208040d8), aside, 0x108);
    if (b[0] != 0) {
        cr_description(ADDR(b), CR_DESC);
        H_LOADSTRING(0x2200, ADDR(text), 0x100);
        save_flight(ADDR(text), CR_DESC);
    } else {
        load_flight(ADDR(&flight));
        M32(G + 0x11f0) = 0;
        M32(G + 0x11f4) = 0;
        M32(G + 0x1200) = 0;
        M16(G + 0x11fa) = 0;
        M32(G + 0x11fc) = 0;
        memcpy((void *)(uintptr_t)V(0x208040d8), aside, 0x108);
    }
    H_DELETE(ADDR(flight.path), flight.ext);
    H_FREE(ADDR(&b[7]));
    M8(CR_DESC) = 0;
    H_FREE(ADDR(&b[8]));
    H_FREE(ADDR(&b[10]));
    for (i = 0; i < 6; i++)
        if (M32(V(0x20808ec8) + i * 4) != 0)
            H_FREE(V(0x20808ec8) + i * 4);
    r = b[0];
    return r;
}
