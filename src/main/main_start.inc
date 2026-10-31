/* MAIN.DLL slice 29: what +0x28 registers and sets going, MSVC C and assembly.
 *
 * .text+0xae40 (CHAIN list 0x14: message 0x40 redoes the menus) with +0xae70,
 * the whole menu bar's state, and +0xf50 (the kneeboard's pages); +0x16900 and
 * +0x16a20 (the menus greyed for sixteen frames of list 1 after a start);
 * +0x16b70, the date and time from the host's clock; +0x16c83 (CHAIN list 2:
 * the sim's clock at the sim rate), with +0x16cde, its carries; and +0x11330,
 * the minute's logbook hours.
 *
 * Retail details, reproduced:
 *   - +0xae70 pushes several item ids over stale upper halves (a pointer's, a
 *     register's).  The host's +0x254 looks them up as 16 bits, so ours passes
 *     the id alone and the harness logs its low half.
 *   - +0x16c83 leaves its second argument in eax. */

/* .text+0xf50: the kneeboard's items, one per page in .data+0xfda4. */
static void menu_kneeboard(void)
{
    uint32_t i;

    if (M32(V(0x2080fda4)) == 0) {
        MENU_SET(0x1208, 2);
        return;
    }
    MENU_SET(0x1208, 0);
    MENU_SET(0x1625, 0);
    i = 0;
    do {
        MENU_SET(i + 0x300, 0);
        i++;
    } while (i <= M32(V(0x2080fda4)) - 1);
}

/* .text+0xae70 */
void menu_state(void);
void menu_state(void)
{
    static const uint16_t clear_all[] = {
        0x1101, 0x1102, 0x1103, 0x1104, 0x1105, 0x1106, 0x1107, 0x1109, 0x110a,
        0x1201, 0x1208, 0x1202, 0x1203, 0x1204, 0x1205, 0x1206, 0x1207,
        0x1301, 0x1302, 0x1305, 0x1306, 0x1307, 0x1308, 0x1309, 0x130a,
        0x1401, 0x1402, 0x1403, 0x1404, 0x1405, 0x1406, 0x1407, 0x1408, 0x1409, 0x140a,
        0x1501, 0x1502, 0x1503, 0x1504, 0x1505, 0x1506, 0x1507, 0x1508,
        0x1601, 0x1608, 0x1609, 0x160a, 0x1602, 0x1603, 0x1604, 0x1606, 0x1607,
        0x1421, 0x1422, 0x1423, 0x1424, 0x1425, 0x1426, 0x1427, 0x1428, 0x1429, 0x142a,
        0x1661, 0x1662, 0x1663, 0x1664, 0x1665, 0x1666, 0x1667, 0x1668, 0x1669, 0x166a,
        0x1540, 0x1541, 0x1542, 0x1543, 0x1544, 0x1545,
        0x1550, 0x1551, 0x1552, 0x1553, 0x1554, 0x1555,
        0x1625, 0x1520, 0x1180, 0x1181, 0x1304, 0x1303,
    };
    static const uint16_t cover[] = {
        0x1101, 0x1102, 0x1109, 0x1201, 0x1206, 0x1303, 0x1304,
    };
    static const uint16_t busy[] = {
        0x1101, 0x1102, 0x1103, 0x1104, 0x1108, 0x1109, 0x1201, 0x1206, 0x1303,
        0x1304, 0x1105, 0x1106, 0x1107,
    };
    static const uint16_t recording[] = { 0x1105, 0x1106, 0x1107, 0x1201, 0x1204, 0x1302 };
    uint32_t i, p, c, v, n, list, flags;

    for (i = 0; i < sizeof clear_all / sizeof clear_all[0]; i++)
        MENU_SET(clear_all[i], 0);
    MENU_SET(0x1501, M32(G + 0x1240) != 0);

    p = M32(G + 0x11a0);
    if (p != 0) {
        MENU_SET(0x1550, 0);
        MENU_SET(0x1551, 0);
        MENU_SET(0x1552, 0);
        MENU_SET(0x1553, 0);
        MENU_SET(0x1555, 0);
        p = M32(G + 0x11a0);
        c = M32(p + 0x10);
        if (c == 0) {
            switch (M32(p + 0x14)) {
            case 0: MENU_SET(0x1550, 1); break;
            case 1: MENU_SET(0x1551, 1); break;
            case 2: MENU_SET(0x1553, 1); break;
            case 4: MENU_SET(0x1552, 1); break;
            default: break;
            }
        } else if (c == 1) {
            MENU_SET(0x1555, 1);
        }
    } else {
        MENU_SET(0x1550, 2);
        MENU_SET(0x1551, 2);
        MENU_SET(0x1552, 2);
        MENU_SET(0x1553, 2);
        MENU_SET(0x1555, 2);
    }

    p = M32(G + 0x119c);
    if (p != 0 && !(M32(p + 8) & 0x40000000u))
        MENU_SET(0x1506, (M32(p + 8) >> 0x1d) & 1);
    else
        MENU_SET(0x1506, 2);
    p = M32(G + 0x119c);
    if (p != 0) {
        MENU_SET(0x1507, (M32(p + 8) >> 0x1e) & 1);
    } else {
        MENU_SET(0x1507, 2);
        MENU_SET(0x1508, 2);
        MENU_SET(0x1505, 2);
    }

    MENU_SET(0x1408, M32(G + 0xb20) != 0);
    MENU_SET(0x1401, M32(G + 0x268) != 0);
    MENU_SET(0x1207, M16(G + 0x5dc) != 0);
    MENU_SET(0x1404, (M32(G + 0x628) == 1 || M32(G + 0x758) != 0) ? 2 : 0);
    MENU_SET(0x1406, M32(G + 0x5d4) != 0 ? 0 : 2);
    if (M32(G + 0x5d4) != 0)
        MENU_SET(0x1406, M32(G + 0x5d8) != 0);
    MENU_SET(0x1204, M32(G + 0x764) != 0 ? 0 : 2);
    MENU_SET(0x1203, ((uint8_t)~M8(G + 0xb04) >> 2) & 2);
    MENU_SET(0x1202, ((uint8_t)~M8(G + 0xb04) & 1) << 1);
    MENU_SET(0x1206, M32(G + 0x850) != 0 ? 0 : 2);
    if (M32(G + 0x850) != 0)
        MENU_SET(0x1206, M32(G + 0x84c) != 0);

    menu_kneeboard();
    if (view_active())
        MENU_SET((uint16_t)(M16(G + 0x129c) + 0x300), 1);
    else
        MENU_SET(0x1625, 1);

    for (i = 0x1421; i <= 0x142a; i++)
        MENU_SET(i, 0);
    switch (M16(G + 0xc1a)) {
    case 0x40:   MENU_SET(0x1421, 1); break;
    case 0x80:   MENU_SET(0x1422, 1); break;
    case 0x100:  MENU_SET(0x1423, 1); break;
    case 0x200:  MENU_SET(0x1424, 1); break;
    case 0x400:  MENU_SET(0x1425, 1); break;
    case 0x800:  MENU_SET(0x1426, 1); break;
    case 0x1000: MENU_SET(0x1427, 1); break;
    case 0x2000: MENU_SET(0x1428, 1); break;
    case 0x4000: MENU_SET(0x1429, 1); break;
    case 0x8000: MENU_SET(0x142a, 1); break;
    default: break;
    }

    flags = CALL(M32(V(0x2080007c)), 0x30, f0)();
    if (flags & 1)
        MENU_SET(0x1108, 0);
    else
        MENU_SET(0x1108, 2);
    if (flags & 2) {
        HOST_(0x264, f2)(0x1180, 0x1182);
        if (flags & 4) {
            MENU_SET(0x1544, 0);
            MENU_SET(0x1554, 0);
        } else {
            MENU_SET(0x1544, 2);
            MENU_SET(0x1554, 2);
        }
    } else {
        HOST_(0x264, f2)(0x1180, 0x1180);
        MENU_SET(0x1544, 2);
        MENU_SET(0x1554, 2);
    }

    v = 0;
    if (CALL(M32(V(0x2080001c)), 0x18, f1)(ADDR(&v)) != 0) {
        for (i = 0; i < sizeof cover / sizeof cover[0]; i++)
            MENU_SET(cover[i], 2);
        if (v == 0x20) {
            MENU_SET(0x1106, 2);
            MENU_SET(0x1107, 2);
            if (M32(G + 0x758) != 0) {
                MENU_SET(0x1103, 2);
                MENU_SET(0x1104, 2);
                MENU_SET(0x1105, 2);
            }
        } else if (v == 0x1a) {
            MENU_SET(0x1106, 2);
            MENU_SET(0x1105, 2);
        } else if (v == 0x21) {
            MENU_SET(0x1105, 2);
            MENU_SET(0x1107, 2);
        }
    } else if (M32(G + 0x758) != 0 || M32(G + 0x628) != 0) {
        for (i = 0; i < sizeof busy / sizeof busy[0]; i++)
            MENU_SET(busy[i], 2);
    }
    if (M32(G + 0x84c) != 0)
        for (i = 0; i < sizeof recording / sizeof recording[0]; i++)
            MENU_SET(recording[i], 2);
    if (M8(G + 0xb64) != 0 && M8(G + 0xb6f) != 0 && M8(G + 0xb70) != 0)
        MENU_SET(0x1202, 2);
    if (M8(G + 0xb68) != 0)
        MENU_SET(0x1203, 2);

    CALL(M10, 0x80, f3)(ADDR(&list), ADDR(&n), 2);
    for (i = 0; (int32_t)i < (int32_t)n; i++) {
        p = M32(list + i * 4);
        MENU_SET(M16(p + 0xc), M32(p + 8) >> 31);
    }
    H_FREE(ADDR(&list));
    MENU_SET(0x1520, CALL(M32(V(0x2080008c)), 0x38, f0)() != 0);
    CALL(M10, 0x80, f3)(ADDR(&list), ADDR(&n), 3);
    for (i = 0; (int32_t)i < (int32_t)n; i++) {
        p = M32(list + i * 4);
        MENU_SET(M16(p + 0xc), 0);
    }
    H_FREE(ADDR(&list));
    p = M32(G + 0x119c);
    if (p != 0)
        MENU_SET(M16(p + 0xc), 1);
}

/* .text+0xae40: CHAIN list 0x14.  Any other message leaves every register. */
void menu_listener(void);
__asm__(".text\n\t.p2align 4\n_menu_listener:\n\t"
        "cmpl $0x40, 4(%esp)\n\tjne 1f\n\t"
        "call _menu_state\n"
        "1:\n\tret $8\n");

/* .text+0x16a20: CHAIN list 1, sixteen frames after +0x16900. */
static uint32_t FS98_STDCALL start_countdown(uint32_t a, uint32_t b)
{
    uint32_t n = M32(V(0x2080cff8)) + 1;

    (void)a;
    (void)b;
    M32(V(0x2080cff8)) = n;
    if ((int32_t)n <= 0xf)
        return n;
    menu_enable();
    HOST_(0x50, f1)(0);
    M32(V(0x2080cff8)) = 0;
    return CHAIN_REMOVE(1, ADDR(start_countdown));
}

/* .text+0x16900 */
static void start_menus(void)
{
    menu_grey();
    HOST_(0x50, f1)(1);
    CHAIN_ADD(1, ADDR(start_countdown));
}

/* The season by the day of the year, from the table at .data+0x6e10. */
static void clock_season(uint32_t gp)
{
    uint32_t i = 0;

    while (M16(gp + 0x23e) >= M16(V(0x20806e10) + i))
        i += 2;
    M16(gp + 0x248) = (uint16_t)((i >> 1) & 3);
}

/* .text+0x16c6c: the two digits of `value` at GLOBAL +0xc20 + at. */
static void date_digits(uint8_t value, uint32_t at)
{
    M16(G + 0xc20 + at) = (uint16_t)(((uint32_t)(value % 10) << 8 | value / 10) + 0x3030);
}

/* .text+0x16b70 */
static void clock_from_host(void)
{
    uint32_t gp = G, table = V(0x20806de0), n;
    uint16_t year, day = 0;

    if (M16(gp + 0x242) == 0)
        return;
    HOST_(0x44, f1)(V(0x20806dd4));
    gp = G;
    year = M16(V(0x20806dd4));
    M16(gp + 0x240) = year;
    if ((year & 3) == 0)
        table = V(0x20806df8);
    n = (uint16_t)(M8(V(0x20806dd6)) - 1);
    while (n != 0) {
        day = (uint16_t)(day + M16(table));
        table += 2;
        n--;
    }
    M16(gp + 0x23e) = (uint16_t)(day + M8(V(0x20806dd8)));
    clock_season(gp);
    M8(gp + 0x238) = M8(V(0x20806dda));
    date_digits(M8(V(0x20806dda)), 0);
    M8(gp + 0x239) = M8(V(0x20806ddc));
    date_digits(M8(V(0x20806ddc)), 3);
    M8(gp + 0x23a) = M8(V(0x20806dde));
    date_digits(M8(V(0x20806dde)), 6);
}

/* .text+0x11330: a minute of the logbook: hours by day, night or instruments. */
static void clock_logbook(void)
{
    uint32_t gp = G, i, n, a, list, p;
    uint16_t alt;
    int tally = 0;

    CALL(M10, 0x80, f3)(V(0x20803c73), V(0x20803c77), 3);
    if (M32(gp + 0x128) != 0) {
        if (M16(gp + 0x116e) & 4) {
            M8(V(0x20803c72)) = (uint8_t)(M8(V(0x20803c72)) + 0x20);
            if (M8(V(0x20803c72)) == 0) {
                M16(gp + 0x234) = (uint16_t)(M16(gp + 0x234) + 1);
                M8(V(0x20803c72)) = 0x40;
            }
        } else {
            M8(V(0x20803c70)) = (uint8_t)(M8(V(0x20803c70)) + 0x20);
            if (M8(V(0x20803c70)) == 0) {
                M8(V(0x20803c70)) = 0x40;
                M16(gp + 0x230) = (uint16_t)(M16(gp + 0x230) + 1);
            }
        }
        n = M32(V(0x20803c77));
        if (n == 0) {
            tally = 1;
        } else {
            a = 0;
            list = M32(V(0x20803c73));
            i = 0;
            do {
                p = M32(list);
                if (M32(M32(p + 0x1c) + 0x10) == 0)
                    a |= M32(p + 8);
                list += 4;
                i++;
            } while (i != M32(V(0x20803c77)));
            if (!(a & 0x80000000u)) {
                tally = 1;
            } else {
                alt = (uint16_t)M32(gp + 0x574);
                if ((M16(gp + 0xea4) | M16(gp + 0xea2)) != 0 &&
                    (int16_t)alt <= (int16_t)M16(gp + 0xea2) &&
                    (int16_t)alt > (int16_t)M16(gp + 0xea4)) {
                    tally = 1;
                } else if ((M16(gp + 0xe9c) | M16(gp + 0xe9a)) != 0 &&
                           (int16_t)alt <= (int16_t)M16(gp + 0xe9a) &&
                           (int16_t)alt > (int16_t)M16(gp + 0xe9c)) {
                    tally = 1;
                }
            }
        }
        if (tally) {
            M8(V(0x20803c71)) = (uint8_t)(M8(V(0x20803c71)) + 0x20);
            if (M8(V(0x20803c71)) == 0) {
                M8(V(0x20803c71)) = 0x40;
                M16(gp + 0x232) = (uint16_t)(M16(gp + 0x232) + 1);
            }
        }
    }
    H_FREE(V(0x20803c73));
}

/* .text+0x16cde */
static void clock_carry(void)
{
    uint32_t gp = G;

    if (M8(gp + 0x23a) >= 0x3c) {
        M8(gp + 0x23a) = (uint8_t)(M8(gp + 0x23a) - 0x3c);
        if (M32(gp + 0x128) != 0)
            clock_logbook();
        M8(gp + 0x239) = (uint8_t)(M8(gp + 0x239) + 1);
        M8(gp + 0x23c) = (uint8_t)(M8(gp + 0x23c) + 1);
    }
    if (M8(gp + 0x239) >= 0x3c) {
        M8(gp + 0x239) = 0;
        M8(gp + 0x238) = (uint8_t)(M8(gp + 0x238) + 1);
    }
    if (M8(gp + 0x23c) >= 0x3c) {
        M8(gp + 0x23c) = 0;
        M8(gp + 0x23b) = (uint8_t)(M8(gp + 0x23b) + 1);
    }
    if (M8(gp + 0x238) >= 0x18)
        M8(gp + 0x238) = 0;
    if (M8(gp + 0x23b) >= 0x18) {
        M8(gp + 0x23b) = 0;
        M16(gp + 0x23e) = (uint16_t)(M16(gp + 0x23e) + 1);
        if (M16(gp + 0x23e) >= ((M16(gp + 0x240) & 3) ? 0x16e : 0x16f)) {
            M16(gp + 0x23e) = 1;
            M16(gp + 0x240) = (uint16_t)(M16(gp + 0x240) + 1);
        }
        nav_season(0, M16(gp + 0x23e));
        clock_season(gp);
    }
    nav_clock_ascii();
}

/* .text+0x16c83: CHAIN list 2.  The clock runs at the sim rate, GLOBAL +0xc1a. */
static uint32_t FS98_STDCALL clock_tick(uint32_t a, uint32_t b)
{
    uint32_t gp = G, p, sum;

    (void)a;
    M16(gp + 0x2a8) = (uint16_t)(M16(gp + 0x2a8) + 1);
    if (M32(gp + 0x268) == 0 && M8(gp + 0x365) == 0) {
        p = (uint32_t)M16(gp + 0xc1a) * M16(V(0x20806dd2));
        sum = (uint32_t)M16(V(0x20806dd0)) + (uint16_t)(p >> 8);
        M16(V(0x20806dd0)) = (uint16_t)sum;
        p = (p >> 24) + (sum >> 16);
        if ((uint8_t)p != 0) {
            M8(gp + 0x23a) = (uint8_t)(M8(gp + 0x23a) + (uint8_t)p);
            clock_carry();
        }
    }
    return b;
}

/* --------------------------------- slice 33: start and shutdown ---------------- */

/* The welcome dialog's buttons.  +0xdaa0 (0x1101, Create a flight) is still
 * pending. */
static uint32_t FS98_STDCALL welcome_lesson(uint32_t a)
{
    (void)a;
    return CALL(M32(V(0x2080001c)), 0x2c, f0)();
}
static uint32_t FS98_STDCALL welcome_quit(uint32_t a)
{
    (void)a;
    HOST_(0x3c, f2)(0x1601, 0);
    return 0;
}
static uint32_t FS98_STDCALL welcome_about(uint32_t a)
{
    (void)a;
    HOST_(0x3c, f2)(0x160a, 0);
    return 0;
}

/* .text+0x14bb0 */
static uint32_t welcome_more_1(void)
{
    uint32_t b[5];

    b[0] = 0;
    b[1] = ADDR(main_nothing);
    b[2] = ADDR(welcome_lesson);
    b[3] = ADDR(welcome_quit);
    b[4] = ADDR(welcome_about);
    HOST_(0x10c, f2)(0x11090001, ADDR(b));
    return b[0];
}

static uint32_t FS98_STDCALL welcome_dual(uint32_t a)
{
    (void)a;
    return flight_open();
}
static uint32_t FS98_STDCALL welcome_review(uint32_t a)
{
    (void)a;
    return command_1106();
}
static uint32_t FS98_STDCALL welcome_video(uint32_t a)
{
    (void)a;
    return CALL(M32(V(0x2080001c)), 0x28, f0)();
}

/* .text+0x14c40 */
static uint32_t welcome_more_2(void)
{
    uint32_t b[6];

    b[0] = 0;
    b[1] = ADDR(main_nothing);
    b[2] = ADDR(welcome_dual);
    b[3] = ADDR(welcome_lesson);
    b[4] = ADDR(welcome_review);
    b[5] = ADDR(welcome_video);
    HOST_(0x10c, f2)(0x11090002, ADDR(b));
    return b[0];
}

static uint32_t FS98_STDCALL welcome_session_more(uint32_t a)
{
    (void)a;
    return welcome_more_1();
}
static uint32_t FS98_STDCALL welcome_lessons(uint32_t a)
{
    (void)a;
    return welcome_more_2();
}
static uint32_t FS98_STDCALL welcome_create(uint32_t a)
{
    (void)a;
    cmd_pending(0xdaa0);
    return 0;
}
static uint32_t FS98_STDCALL welcome_multip(uint32_t a)
{
    (void)a;
    return CALL(M32(V(0x2080007c)), 0x18, f0)();
}

/* .text+0x14ab0: the dialog at start, and command 0x1109. */
static uint32_t welcome(uint32_t show)
{
    uint32_t b[9];

    if (show == 0)
        return 0;
    b[0] = 0;
    b[1] = ADDR(main_nothing);
    b[2] = ADDR(welcome_session_more);
    b[3] = ADDR(welcome_lessons);
    b[4] = ADDR(welcome_create);
    b[5] = ADDR(welcome_multip);
    b[7] = 0;
    b[8] = M32(G + 0x121c) == 0;
    b[6] = CALL(M32(V(0x2080007c)), 0x30, f0)() & 0xff & 1;
    HOST_(0x10c, f2)(0x1109, ADDR(b));
    M32(G + 0x121c) = b[8] == 0;
    if (b[7] != 0)
        HOST_(0x28, f0)();
    return 1;
}

/* .text+0xce30: the temp flight's file removed. */
static void start_clean_temp(void)
{
    char name[0x104], dir[0x104];

    memset(dir, 0, sizeof dir);
    memcpy(dir, (const void *)(uintptr_t)V(0x20803634), 0x11);
    HOST_(0x2a0, f3)(0x9002, ADDR(name), 0x104);
    HOST_(0x2a0, f3)(0x2201, ADDR(dir), 0x104);
    HOST_(0x184, f4)(ADDR(name), 0, ADDR(name), ADDR(dir));
    HOST_(0x14c, f2)(ADDR(name), 2);
}

/* .text+0x16820 */
static void shutdown_save(void)
{
    char a[0x104], b[0x200];

    HOST_(0x2a0, f3)(0x2202, ADDR(a), 0x104);
    HOST_(0x2a0, f3)(0x2203, ADDR(b), 0x200);
    save_flight(ADDR(a), ADDR(b));
}

/* .text+0x165b0 == Linkage +0x28 */
static uint32_t FS98_STDCALL main_start(void)
{
    char line[0x104], ext[0x100], dir[0x100], file[0x100];
    struct { uint32_t kind; char path[0x10c]; } f;
    uint32_t type = 1, loaded = 0, gp;

    prefs_load();
    HOST_(0x23c, f1)(V(0x20806d30));
    weather_load();
    CALL(SOUND_, 0x18, f0)();
    kb_create();
    if (HOST_(0x98, f3)(V(0x20806dbc), ADDR(line), 0x104) == 0) {
        HOST_(0x188, f5)(ADDR(line), ADDR(file), ADDR(dir), ADDR(ext), ADDR(&type));
        if (type == 2 || type == 0) {
            HOST_(0x184, f4)(ADDR(f.path), ADDR(file), ADDR(dir), ADDR(ext));
            f.kind = 2;
            load_flight(ADDR(&f));
            loaded = 1;
        }
    }
    if (loaded == 0)
        load_default();
    gp = G;
    if (M32(gp + 0x11d4) == 0) {
        HOST_(0x2a0, f3)(0x90a0, ADDR(line), 0x104);
        CALL(MODULE23, 0x28, f1)(ADDR(line));
    }
    start_menus();
    CALL(HG2D, 0x18, f0)();
    clock_from_host();
    nav_season(1, M16(G + 0x23e));
    radios_start();
    CHAIN_ADD(0x14, ADDR(menu_listener));
    CHAIN_ADD(2, ADDR(clock_tick));
    gp = G;
    if (loaded == 0) {
        M32(gp + 0x73c) = M32(gp + 0x1210);
        if (M32(G + 0x73c) == 1 || M32(G + 0x73c) == 2) {
            M32(G + 0xb20) = M32(G + 0x73c) == 1;
            M32(G + 0x750) = 1;
            CALL(M7, 0x28, f2)(G + 0x638, 1);
        }
    }
    if (M32(G + 0x1218) != 0) {
        M8(G + 0xf12) = 1;
        M8(G + 0xf13) = 1;
        CALL(WEATHERM_, 0x24, f0)();
    }
    CALL(M32(V(0x2080003c)), 0x4c, f0)();
    HOST_(0x114, f0)();
    HOST_(0x114, f0)();
    CHAIN_SEND(0x14, 0x36, 0);
    M32(G + 0x24c) = 1;
    M32(G + 0x250) = 1;
    M32(G + 0x254) = 1;
    main_step();
    if (loaded == 0)
        welcome(M32(G + 0x121c) == 0);
    return 0;
}

/* .text+0x16880 == Linkage +0x2c */
static uint32_t FS98_STDCALL main_shutdown(void)
{
    shutdown_save();
    CHAIN_SEND(0x14, 0x37, 0);
    CHAIN_REMOVE(0x14, ADDR(menu_listener));
    CALL(MODULE23, 0x2c, f0)();
    H_FREE(G + 0x11d8);
    radios_stop();
    CALL(WINDOW_, 0x18, f0)();
    CALL(M32(V(0x2080003c)), 0x50, f0)();
    HOST_(0x240, f0)();
    CALL(SOUND_, 0x1c, f0)();
    kb_destroy();
    prefs_save();
    start_clean_temp();
    return 0;
}

static void welcome_cmd(void)
{
    welcome(1);
}
