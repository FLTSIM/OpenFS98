/* MAIN.DLL slice 25: +0x20, the menu command dispatcher (MSVC C,
 * .text+0xa090, with .text+0xae50).
 *
 * (command, argument).  Commands in 0x80000..0x8ffff first go to .text+0xae50,
 * where 0x80005 reloads the flight (+0x44, slice 2).  The menus' commands then
 * post a host command, call another module's service, or run one of MAIN's own
 * handlers, and answer 1.  0x100..0x1ff are windows (WINDOW +0x8c to find it,
 * +0x48 to bring it up), 0x200..0x2ff panels (PANELS +0x20 or +0x24, by the
 * record's top flag).  Anything else goes out on CHAIN list 0xb with the block
 * { command, handled, argument }, and the answer is whether a listener set
 * handled: that is how the other modules' menus are reached.
 *
 * MAIN's own handlers are done a batch at a time.  One not done yet goes through
 * cmd_pending, which faults on purpose; maindiff's `commands' suite does not
 * issue those commands and docs/modules/main.md lists them. */

#define ADVDRV_  M32(V(0x2080001c))
#define ANALYSIS_ M32(V(0x20800024))
#define CONTROLS_ M32(V(0x2080003c))
#define DEMO_    M7
#define FACIL_   M32(V(0x20800054))
#define MULTIP_  M32(V(0x2080007c))
#define PANELS_  M32(V(0x2080008c))
#define WINDOW_  M10

static void cmd_pending(uint32_t retail_text)
{
    *(volatile uint32_t *)(uintptr_t)0xdead0000u = retail_text;
}

/* .text+0xae50: the 0x80000 range; 0x80001 does nothing, as the others do. */
static void cmd_range8(uint32_t command)
{
    if (command == 0x80005)
        reload_flight();
}

/* .text+0x1890: the kneeboard's view up (WINDOW +0x68) or away (+0x6c). */
static void kneeboard_show(uint32_t up)
{
    if (up != 0)
        CALL(WINDOW_, 0x68, f1)(M32(V(0x20807fc4)));
    else
        CALL(WINDOW_, 0x6c, f1)(M32(V(0x20807fc4)));
}

/* Commands in no range of their own. */
static uint32_t cmd_other(uint32_t command, uint32_t argument)
{
    volatile uint32_t block[3];
    uint32_t r;

    if (command >= 0x100 && command < 0x200) {
        r = CALL(WINDOW_, 0x8c, f2)(command, 3);
        if (r != 0)
            CALL(WINDOW_, 0x48, f1)(r);
        return 1;
    }
    if (command >= 0x300 && command < 0x400) {
        cmd_pending(0x18d0);
        M32(G + 0x129c) = command - 0x300;
        return 1;
    }
    if (command >= 0x200 && command < 0x300) {
        r = CALL(WINDOW_, 0x8c, f2)(command, 2);
        if (r == 0)
            return 1;
        if (M32(r + 8) & 0x80000000u)
            CALL(PANELS_, 0x24, f1)(r);
        else
            CALL(PANELS_, 0x20, f1)(r);
        return 1;
    }
    block[0] = command;
    block[1] = 0;
    block[2] = argument;
    CHAIN_SEND(0xb, command, ADDR(block));
    return block[1] != 0;
}

/* .text+0xa090 == Linkage +0x20. */
static uint32_t FS98_STDCALL main_command(uint32_t command, uint32_t argument)
{
    uint32_t gp;

    if (command >= 0x80000 && command <= 0x8ffff)
        cmd_range8(command);
    switch (command) {
    case 0x1101: cmd_pending(0xdaa0); break;
    case 0x1102: flight_open(); break;
    case 0x1103: H_POST(0x10037, 0); break;
    case 0x1104: H_POST(0x10027, 0); break;
    case 0x1105: CALL(ADVDRV_, 0x2c, f0)(); break;
    case 0x1106: command_1106(); break;
    case 0x1107: CALL(ADVDRV_, 0x28, f0)(); break;
    case 0x1109: cmd_pending(0x14ab0); break;
    case 0x1180: CALL(MULTIP_, 0x18, f0)(); break;
    case 0x1181: CALL(MULTIP_, 0x1c, f0)(); break;
    case 0x1201: cmd_pending(0x110); break;
    case 0x1202: cmd_pending(0x13870); break;
    case 0x1203: command_1203(); break;
    case 0x1204: cmd_pending(0x2ee0); break;
    case 0x1205: cmd_pending(0x3bf0); break;
    case 0x1206: H_POST(0x1002d, 0); break;
    case 0x1207: H_POST(0x10015, 0); break;
    case 0x1301: cmd_pending(0x1b20); break;
    case 0x1303: CALL(FACIL_, 0x28, f0)(); break;
    case 0x1304: CALL(FACIL_, 0x24, f1)(0); break;
    case 0x1305: CALL(FACIL_, 0x30, f0)(); break;
    case 0x1306: cmd_pending(0x19fb0); break;
    case 0x1307: cmd_pending(0x5010); break;
    case 0x1308: command_1308(); break;
    case 0x1309: command_1309(); break;
    case 0x130a: CALL(FE, 0x78, f0)(); break;
    case 0x1401: H_POST(0x10019, 0); break;
    case 0x1403: CALL(DEMO_, 0x30, f0)(); break;
    case 0x1404:
        if (M32(G + 0x760) == 0)
            CALL(DEMO_, 0x2c, f0)();
        else
            CALL(DEMO_, 0x34, f0)();
        break;
    case 0x1405: CALL(ANALYSIS_, 0x34, f0)(); break;
    case 0x1406: H_POST(0x10017, 0); break;
    case 0x1407: cmd_pending(0xf3d0); break;
    case 0x1408: H_POST(0x10010, 0); break;
    case 0x1409: cmd_pending(0x14ce0); break;
    case 0x140a: CALL(CONTROLS_, 0x58, f0)(); break;
    case 0x1421: case 0x1422: case 0x1423: case 0x1424: case 0x1425:
    case 0x1426: case 0x1427: case 0x1428: case 0x1429: case 0x142a:
        M16(G + 0xc1a) = (uint16_t)(0x40u << (command - 0x1421));
        break;
    case 0x1501:
        gp = G;
        M32(gp + 0x1240) = M32(gp + 0x1240) == 0;
        HOST_(0x48, f1)(M32(G + 0x1240));
        break;
    case 0x1502: cmd_pending(0x16e40); break;
    case 0x1506: H_POST(0x10011, 0); break;
    case 0x1507: H_POST(0x2000f, M32(G + 0x119c)); break;
    case 0x1508: H_POST(0x10120, 0); break;
    case 0x1520: H_POST(0x100d4, 0); break;
    case 0x1540: H_POST(0x10121, 0); break;
    case 0x1541: H_POST(0x10121, 1); break;
    case 0x1542: H_POST(0x10121, 4); break;
    case 0x1543: H_POST(0x10121, 2); break;
    case 0x1544: H_POST(0x10121, 3); break;
    case 0x1545: H_POST(0x10122, 0); break;
    case 0x1550: H_POST(0x20008, M32(G + 0x119c)); break;
    case 0x1551: H_POST(0x20009, M32(G + 0x119c)); break;
    case 0x1552: H_POST(0x2000b, M32(G + 0x119c)); break;
    case 0x1553: H_POST(0x2000a, M32(G + 0x119c)); break;
    case 0x1554: H_POST(0x2000c, M32(G + 0x119c)); break;
    case 0x1555: H_POST(0x2000d, M32(G + 0x119c)); break;
    case 0x1607: command_1607(); break;
    case 0x1625:
        kneeboard_show(0);
        M32(G + 0x129c) = 0x1325;
        break;
    case 0x20000: case 0x20001: case 0x20002:
    case 0x20003: case 0x20004: case 0x20005:
        CHAIN_SEND(0x14, 0x40 + (command - 0x20000), 0);
        break;
    case 0x20006: CHAIN_SEND(9, 0, 0); break;
    case 0x20007:
        if (M32(argument + 4) == 3)
            CALL(WINDOW_, 0x48, f1)(argument);
        break;
    case 0x20008:
        CALL(WINDOW_, 0x5c, f2)(argument, 0);
        CALL(WINDOW_, 0x58, f2)(argument, 0);
        break;
    case 0x20009:
        CALL(WINDOW_, 0x5c, f2)(argument, 0);
        CALL(WINDOW_, 0x58, f2)(argument, 1);
        break;
    case 0x2000a:
        CALL(WINDOW_, 0x5c, f2)(argument, 0);
        CALL(WINDOW_, 0x58, f2)(argument, 2);
        break;
    case 0x2000b:
        CALL(WINDOW_, 0x5c, f2)(argument, 0);
        CALL(WINDOW_, 0x58, f2)(argument, 4);
        break;
    case 0x2000c:
        CALL(WINDOW_, 0x5c, f2)(argument, 0);
        CALL(WINDOW_, 0x58, f2)(argument, 3);
        break;
    case 0x2000d: CALL(WINDOW_, 0x5c, f2)(argument, 1); break;
    case 0x2000e: CALL(WINDOW_, 0x54, f1)(argument); break;
    case 0x2000f:
        if (argument != 0) {
            CALL(WINDOW_, 0x50, f1)(argument);
            M32(argument + 8) = M32(argument + 8) ^ 0x40000000u;
            HOST_(0x7c, f1)(argument);
        }
        break;
    case 0x20010:
        if (M32(argument + 4) == 3)
            CALL(WINDOW_, 0x40, f1)(argument);
        else if (M32(argument + 4) == 2)
            CALL(PANELS_, 0x24, f1)(argument);
        else
            CALL(WINDOW_, 0x6c, f1)(argument);
        break;
    case 0x20011:
        gp = G;
        if (argument == 0 && M32(gp + 0x2ac) != 0 && M32(gp + 0x1230) != 0)
            H_POST(0x10102, argument);
        break;
    case 0x20012: CHAIN_SEND(0x14, 0x46, 0); break;
    case 0x20013:
        InterlockedExchange((LONG *)(uintptr_t)(G + 0x254), 0);
        InterlockedExchange((LONG *)(uintptr_t)(G + 0x250), 0);
        InterlockedExchange((LONG *)(uintptr_t)(G + 0x24c), 0);
        break;
    case 0x20014: CHAIN_SEND(0x14, 0x3a, argument); break;
    default:
        return cmd_other(command, argument);
    }
    return 1;
}
