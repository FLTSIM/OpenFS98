/* mpchat.DLL -- module id 0xffffffff: the multiplayer chat window.
 *
 * A window of its own (class FS98MPCHAT): a RichEdit transcript on the left
 * three quarters, the players in a ListBox on the right, and under them an
 * Edit line and a Send button.  Nothing imports MPCHAT -- its id is -1 --
 * so it is reached only through CHAIN and the host's dialog registry:
 *
 *   list 0x1d  MULTIP's session news: 1 a session began (the window is made
 *              through dialog 0xff00, a "Views" menu item 0x1183 added and
 *              lists 0x14, 0xb and 0xc joined), 2 it ended, 3 and 4 a new
 *              player list, 5 an application message (type 0x1020 is a
 *              line of chat);
 *   list 0x14  message 0x40: the menu item's check mark kept with the
 *              window's visibility;
 *   list 0xb   command 0x1183: the window shown or hidden;
 *   list 0xc   commands 0x10170 (toggle) and 0x10171 (show and focus the
 *              line), answered through the second argument.
 *
 * A line typed and sent goes to MULTIP +0x20 as {0x1020, size, text} and
 * into the transcript under the local player's name; one received goes into
 * the transcript under its sender's (or string 0x9402), and, if the window
 * is hidden, up on WINMSG's bar in slot 6.
 *
 * Retail is MSVC C, transliterated here.  The whole 0x50c-byte .data image
 * is kept at retail's offsets, with the dialog table at +0x30 that Linkage
 * +0x14 points to; the harness compares the image.  docs/modules/mpchat.md
 * has the layout and what retail does that is worth knowing.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define MC_IMAGE_SIZE 0x50c
#define MC_IMPORTS    0x000
#define MC_DIALOGS    0x030
#define MC_LINKAGE    0x048

typedef volatile uint8_t  mc_v8;
typedef volatile uint32_t __attribute__((may_alias)) mc_v32;

struct mpchat_image {
    uint8_t bytes[MC_IMAGE_SIZE];
};

extern struct mpchat_image mpchat_image;

#define D       ((uint32_t)(uintptr_t)mpchat_image.bytes)
#define M8(a)   (*(mc_v8  *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(mc_v32 *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))
#define PTR(a)  ((void *)(uintptr_t)(uint32_t)(a))
#define STR(a)  ((const char *)(uintptr_t)(uint32_t)(a))
#define WND(a)  ((HWND)(uintptr_t)(uint32_t)(a))

#define IMPORT(i)  M32(D + MC_IMPORTS + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define CHAIN      IMPORT(2)
#define MULTIP     IMPORT(3)
#define WINMSG     IMPORT(4)

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_WINDOW_INST HOST_(0x054, f2)   /* (&wnd, &instance) */
#define H_POST        HOST_(0x03c, f2)   /* (command, parameter) */
#define H_DIALOG      HOST_(0x110, f2)   /* (id, param) */
#define H_FREE        HOST_(0x214, f1)   /* (&p) */
#define H_MENU_ADD    HOST_(0x244, f4)   /* (command, id, text, 0) */
#define H_MENU_STATE  HOST_(0x254, f2)   /* (id, flags): 1 checked */
#define H_MENU_REMOVE HOST_(0x25c, f1)   /* (id) */
#define H_MODULE_IN   HOST_(0x298, f2)   /* (linkage, &instance) */
#define H_MODULE_OUT  HOST_(0x29c, f1)   /* (instance) */
#define H_STRING      HOST_(0x2a4, f2)   /* (id, &out): a copy of string id */
#define H_STRCPY      HOST_(0x2a8, f2)
#define H_STRCAT      HOST_(0x2b0, f2)
#define H_STRLEN      HOST_(0x2b8, f1)

#define CHAIN_ADD     CALL(CHAIN, 0x18, f2)    /* (list, callback) */
#define CHAIN_REMOVE  CALL(CHAIN, 0x1c, f2)
#define MULTIP_SEND   CALL(MULTIP, 0x20, f4)   /* (to, payload, size, flags) */
#define MULTIP_LIST   CALL(MULTIP, 0x24, f0)   /* {count, records} */
#define MULTIP_PLAYER CALL(MULTIP, 0x28, f1)   /* (id): a record, or 0 */
#define MULTIP_LOCAL  CALL(MULTIP, 0x2c, f0)   /* the local record, or 0 */
#define WINMSG_SHOW   CALL(WINMSG, 0x18, f4)   /* (text, slot, time, append) */

/* ---- the state, at retail's offsets (D + offset) ------------------------- */

#define S_CLASS     (D + 0x060)  /* "FS98MPCHAT" */
#define S_RICHED    (D + 0x06c)  /* "RICHED32.DLL" */
#define S_SPLIT     (D + 0x080)  /* 0xc000: the transcript's share, 16.16 (+0x7c, the same, is unread) */
#define S_SPACE     (D + 0x084)  /* " " */
#define S_COLON     (D + 0x088)  /* ":" */
#define S_BUTTON    (D + 0x08c)
#define S_EDIT      (D + 0x094)
#define S_LISTBOX   (D + 0x09c)
#define S_RICHEDIT  (D + 0x0a4)
#define S_NEWLINE   (D + 0x0b0)  /* "\n" */
#define S_INSTANCE  (D + 0x0b4)  /* MPCHAT's own, from host +0x298 */
#define S_PLAYERS   (D + 0x0b8)  /* {count, records}: MULTIP's list, or list 0x1d's 3 */
#define S_OTHERS    (D + 0x0bc)  /* the same, from list 0x1d's 4 */
#define S_LOCAL     (D + 0x0c0)  /* the local record, as last sent */
#define S_MAIN_WND  (D + 0x0c4)  /* host +0x54's */
#define S_MAIN_INST (D + 0x0c8)
#define S_TITLE     (D + 0x0cc)  /* host strings 0x9400, 0x9401, 0x9402 */
#define S_SEND_TEXT (D + 0x0d0)
#define S_UNKNOWN   (D + 0x0d4)  /* a sender MULTIP does not know */
#define S_WND       (D + 0x0d8)
#define S_CHAT      (D + 0x0dc)  /* RichEdit, id 0x100 */
#define S_ENTRY     (D + 0x0e0)  /* Edit, id 0x101 */
#define S_LIST      (D + 0x0e4)  /* ListBox, id 0x102 */
#define S_SEND      (D + 0x0ec)  /* Button, id 0x103 */
#define S_EMPTY     (D + 0x0f0)  /* "" */
#define S_MSG       (D + 0x100)  /* {0x1020, size, text[0x400]} */
#define S_RICHED_MOD (D + 0x508)

#define MSG_CHAT    0x1020
#define CMD_CHAT    0x1183      /* the menu item and its command */
#define RECORD_SIZE 0x100       /* a MULTIP player record; +0 its name */

/* ---- the window ------------------------------------------------------------ */

/* .text+0x1540 */
static uint32_t is_visible(void)
{
    return (uint32_t)GetWindowLongA(WND(M32(S_WND)), GWL_STYLE) & WS_VISIBLE;
}

/* .text+0x1560 */
static void show(void)
{
    ShowWindow(WND(M32(S_WND)), SW_SHOWNOACTIVATE);
    UpdateWindow(WND(M32(S_WND)));
}

/* .text+0x1580 */
static void hide(void)
{
    ShowWindow(WND(M32(S_WND)), SW_HIDE);
    UpdateWindow(WND(M32(S_WND)));
}

/* .text+0x15a0 */
static void focus_entry(void)
{
    SetFocus(WND(M32(S_ENTRY)));
}

/* .text+0x14a0 */
static void destroy(void)
{
    DestroyWindow(WND(M32(S_WND)));
}

/* .text+0x14b0 -- the ListBox refilled: list 0x1d's 4 first, then the
 * players.  Both loops reread the list's count and records every time
 * round.  Returns the players' list, as retail leaves it in eax. */
static uint32_t fill_list(void)
{
    uint32_t i, at;

    SendMessageA(WND(M32(S_LIST)), LB_RESETCONTENT, 0, 0);
    if (M32(S_OTHERS) != 0 && M32(M32(S_OTHERS)) != 0)
        for (i = 0, at = 0; i < M32(M32(S_OTHERS)); i++, at += RECORD_SIZE)
            SendMessageA(WND(M32(S_LIST)), LB_ADDSTRING, 0,
                         (LPARAM)M32(M32(M32(S_OTHERS) + 4) + at));
    if (M32(S_PLAYERS) != 0 && M32(M32(S_PLAYERS)) != 0)
        for (i = 0, at = 0; i < M32(M32(S_PLAYERS)); i++, at += RECORD_SIZE)
            SendMessageA(WND(M32(S_LIST)), LB_ADDSTRING, 0,
                         (LPARAM)M32(M32(M32(S_PLAYERS) + 4) + at));
    return M32(S_PLAYERS);
}

/* RichEdit's structures, at the sizes retail gives them. */
struct mc_charrange { int32_t min, max; };
struct mc_paraformat {                 /* PARAFORMAT2, 0x9c */
    uint32_t size, mask;
    uint16_t numbering, reserved;
    int32_t  start_indent;
    uint8_t  rest[0x9c - 0x10];
};
struct mc_charformat {                 /* CHARFORMATA, 0x3c */
    uint32_t size, mask, effects;
    int32_t  height, offset;
    uint32_t colour;
    uint8_t  rest[0x3c - 0x18];
};

/* .text+0x1a50 -- `text` onto the end of RichEdit `hwnd`, after `newlines`
 * line breaks (none into an empty one), indented 400 twips or 40, bold or
 * not, in `colour`.  The selection is put back if it was not already at the
 * end, and the transcript scrolled to the bottom if it was at the bottom
 * before (or that could not be told) and now is not.  Retail leaves the
 * unmasked fields of both formats as the stack had them; they are zero
 * here, which RichEdit does not read. */
static void append(uint32_t hwnd, uint32_t text, uint32_t colour, uint32_t newlines,
                   uint32_t indented, uint32_t bold)
{
    struct mc_charrange sel, end;
    SCROLLINFO before, after;
    struct mc_paraformat pf = { 0 };
    struct mc_charformat cf = { 0 };
    uint32_t len, got;

    SendMessageA(WND(hwnd), 0x434, 0, (LPARAM)&sel);              /* EM_EXGETSEL */
    len = (uint32_t)SendMessageA(WND(hwnd), WM_GETTEXTLENGTH, 0, 0);
    before.cbSize = 0x1c;
    before.fMask = SIF_ALL;
    got = (uint32_t)GetScrollInfo(WND(hwnd), SB_VERT, &before);
    if ((uint32_t)sel.min != (uint32_t)sel.max || (uint32_t)sel.min != len) {
        end.max = (int32_t)len;
        end.min = (int32_t)len;
        SendMessageA(WND(hwnd), 0x437, 0, (LPARAM)&end);          /* EM_EXSETSEL */
    }
    if (len != 0 && newlines != 0)
        do
            SendMessageA(WND(hwnd), EM_REPLACESEL, 0, (LPARAM)S_NEWLINE);
        while (--newlines);
    pf.size = 0x9c;
    pf.mask = 1;                                                  /* PFM_STARTINDENT */
    pf.start_indent = (int32_t)((-(uint32_t)(indented != 0) & 0x168) + 0x28);
    SendMessageA(WND(hwnd), 0x447, 0, (LPARAM)&pf);              /* EM_SETPARAFORMAT */
    cf.size = 0x3c;
    cf.mask = 0x40000001u;                                        /* CFM_COLOR | CFM_BOLD */
    cf.effects = bold != 0;
    cf.colour = colour;
    SendMessageA(WND(hwnd), 0x444, 1, (LPARAM)&cf);              /* EM_SETCHARFORMAT, selection */
    SendMessageA(WND(hwnd), EM_REPLACESEL, 0, (LPARAM)text);
    if ((uint32_t)sel.min != (uint32_t)sel.max || (uint32_t)sel.min != len)
        SendMessageA(WND(hwnd), 0x437, 0, (LPARAM)&sel);
    after.cbSize = 0x1c;
    after.fMask = SIF_ALL;
    if (GetScrollInfo(WND(hwnd), SB_VERT, &after) == 0)
        return;
    if (after.nMax <= (int32_t)(after.nPage + (uint32_t)after.nPos))
        return;
    if (got != 0 && before.nPos <= (int32_t)((uint32_t)before.nMax - before.nPage))
        return;
    SendMessageA(WND(hwnd), WM_VSCROLL,
                 ((((uint32_t)after.nPage << 16) - after.nPage + (uint32_t)after.nMax) << 16) |
                     SB_THUMBPOSITION, 0);
}

/* .text+0x15b0 -- a line of chat into the transcript, "name:" in blue and
 * the text under it indented; and, when the window is hidden and the line
 * is not the local player's, the three onto WINMSG's bar in slot 6. */
static void say(uint32_t name, uint32_t text, uint32_t local)
{
    char line[0x400];

    line[0] = 0;
    H_STRCAT(ADDR(line), name);
    H_STRCAT(ADDR(line), S_COLON);
    append(M32(S_CHAT), ADDR(line), 0xff0000, 2, 0, 0);
    append(M32(S_CHAT), text, 0, 1, 1, 0);
    if (is_visible() || local != 0)
        return;
    WINMSG_SHOW(ADDR(line), 6, 0x5a, 0);
    WINMSG_SHOW(S_SPACE, 6, 0x5a, 1);
    WINMSG_SHOW(text, 6, 0x5a, 1);
}

/* .text+0x1680 -- `text` sent to everyone, then shown as the local
 * player's (or nameless, with no local record). */
static void send_line(uint32_t text)
{
    uint32_t n, local;

    M32(S_MSG) = MSG_CHAT;
    H_STRCPY(S_MSG + 8, text);
    n = H_STRLEN(S_MSG + 8) + 1;
    M32(S_MSG + 4) = n;
    MULTIP_SEND(0, S_MSG, n + 8, 0);
    local = MULTIP_LOCAL();
    M32(S_LOCAL) = local;
    say(local != 0 ? M32(local) : S_EMPTY, text, 1);
}

/* .text+0x1700 -- the Send button: the line, if there is one, sent and
 * cleared. */
static void on_send(void)
{
    char buf[0x400];

    GetWindowTextA(WND(M32(S_ENTRY)), buf, 0x3ff);
    if (H_STRLEN(ADDR(buf)) == 0)
        return;
    send_line(ADDR(buf));
    SetWindowTextA(WND(M32(S_ENTRY)), STR(S_EMPTY));
}

#define SPLIT(right) (((uint32_t)(right) * M32(S_SPLIT)) >> 16)

/* .text+0x1760 -- the window procedure.  The four controls are laid out
 * from the client rectangle on WM_CREATE and again on WM_SIZE. */
static LRESULT CALLBACK chat_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    RECT rc;
    uint32_t right, bottom, split, h;

    GetClientRect(hwnd, &rc);
    if (msg > 5) {
        if (msg == WM_SETFOCUS) {
            SetFocus(WND(M32(S_ENTRY)));
        } else if (msg == WM_CLOSE) {
            H_POST(CMD_CHAT, 0);
            return 0;
        } else if (msg == WM_COMMAND) {
            if ((wparam & 0xffff) == 0x100) {
                if ((uint32_t)wparam >> 16 == 0x100)              /* EN_SETFOCUS */
                    SendMessageA((HWND)lparam, EM_SETSEL, (WPARAM)-1, 0);
            } else if ((wparam & 0xffff) == 0x103 && (uint32_t)wparam >> 16 == 0) {
                /* sent, and the keyboard handed back to the sim */
                on_send();
                SetFocus(WND(M32(S_MAIN_WND)));
            }
        }
    } else if (msg == WM_SIZE) {
        right = (uint32_t)rc.right;
        bottom = (uint32_t)rc.bottom;
        split = SPLIT(right);
        SetWindowPos(WND(M32(S_CHAT)), 0, 2, 2, (int)(split - 3), (int)(bottom - 0x23),
                     SWP_NOZORDER);
        split = SPLIT(right);
        SetWindowPos(WND(M32(S_LIST)), 0, (int)(split + 1), 2, (int)(right - split - 1),
                     (int)(bottom - 0x23), SWP_NOZORDER);
        SetWindowPos(WND(M32(S_ENTRY)), 0, 2, (int)(bottom - 0x1e), (int)(right - 0x50), 0x19,
                     SWP_NOZORDER);
        SetWindowPos(WND(M32(S_SEND)), 0, (int)(right - 0x4b), (int)(bottom - 0x1e), 0x46, 0x19,
                     SWP_NOZORDER);
    } else if (msg == WM_CREATE) {
        right = (uint32_t)rc.right;
        bottom = (uint32_t)rc.bottom;
        split = SPLIT(right);
        h = (uint32_t)(uintptr_t)CreateWindowExA(
            WS_EX_CLIENTEDGE, STR(S_RICHEDIT), STR(S_EMPTY), 0x50a30904, 2, 2,
            (int)(split - 3), (int)(bottom - 0x23), hwnd, (HMENU)0x100,
            (HINSTANCE)(uintptr_t)M32(S_MAIN_INST), 0);
        M32(S_CHAT) = h;
        ShowWindow(WND(h), SW_SHOWNOACTIVATE);
        UpdateWindow(WND(M32(S_CHAT)));
        split = SPLIT(right);
        h = (uint32_t)(uintptr_t)CreateWindowExA(
            WS_EX_CLIENTEDGE, STR(S_LISTBOX), STR(S_EMPTY), 0x50b30908, (int)(split + 1), 2,
            (int)(right - split - 1), (int)(bottom - 0x23), hwnd, (HMENU)0x102,
            (HINSTANCE)(uintptr_t)M32(S_MAIN_INST), 0);
        M32(S_LIST) = h;
        ShowWindow(WND(h), SW_SHOWNOACTIVATE);
        UpdateWindow(WND(M32(S_LIST)));
        h = (uint32_t)(uintptr_t)CreateWindowExA(
            WS_EX_CLIENTEDGE, STR(S_EDIT), STR(S_EMPTY), 0x50830080, 2, (int)(bottom - 0x1e),
            (int)(right - 0x50), 0x19, hwnd, (HMENU)0x101,
            (HINSTANCE)(uintptr_t)M32(S_MAIN_INST), 0);
        M32(S_ENTRY) = h;
        ShowWindow(WND(h), SW_SHOWNOACTIVATE);
        UpdateWindow(WND(M32(S_ENTRY)));
        h = (uint32_t)(uintptr_t)CreateWindowExA(
            0, STR(S_BUTTON), STR(M32(S_SEND_TEXT)), 0x50030000, (int)(right - 0x4b),
            (int)(bottom - 0x1e), 0x46, 0x19, hwnd, (HMENU)0x103,
            (HINSTANCE)(uintptr_t)M32(S_MAIN_INST), 0);
        M32(S_SEND) = h;
        ShowWindow(WND(h), SW_SHOWNOACTIVATE);
        UpdateWindow(WND(M32(S_SEND)));
        /* Send as the default button: told to the dialog procedure
         * directly, not sent through the window */
        DefDlgProcA(hwnd, DM_SETDEFID, 0x103, 0);
    }
    return DefDlgProcA(hwnd, msg, wparam, lparam);
}

/* .text+0x1430 -- dialog 0xff00's create, (main window, param), both
 * unread: the window, shown without activating it and filled with
 * MULTIP's players. */
static uint32_t FS98_STDCALL create(uint32_t parent, uint32_t param)
{
    uint32_t h;

    (void)parent;
    (void)param;
    h = (uint32_t)(uintptr_t)CreateWindowExA(
        WS_EX_TOOLWINDOW, STR(S_CLASS), STR(M32(S_TITLE)), 0x84cc0000, 0, 0, 0x190, 0xc8,
        WND(M32(S_MAIN_WND)), 0, (HINSTANCE)(uintptr_t)M32(S_MAIN_INST), 0);
    M32(S_WND) = h;
    ShowWindow(WND(h), SW_SHOWNOACTIVATE);
    M32(S_PLAYERS) = MULTIP_LIST();
    fill_list();
    UpdateWindow(WND(M32(S_WND)));
    return M32(S_WND);
}

/* ---- the CHAIN callbacks ---------------------------------------------------
 *
 * CHAIN discards what a callback returns.  Retail's leave whatever eax
 * happened to hold -- the entry's eax, for a message they ignore -- and that
 * is not reproduced.
 */

/* .text+0x1200 -- list 0xc: 0x10170 toggles the window, 0x10171 shows it
 * and puts the caret in the line; either is answered with 1 through `out`. */
static void FS98_STDCALL on_command(uint32_t command, uint32_t out)
{
    if (command == 0x10170) {
        if (is_visible())
            hide();
        else
            show();
        M32(out) = 1;
    } else if (command == 0x10171) {
        if (!is_visible())
            show();
        focus_entry();
        M32(out) = 1;
    }
}

/* .text+0x1400 -- list 0xb: the menu item's command toggles the window. */
static void FS98_STDCALL on_menu(uint32_t command, uint32_t arg)
{
    (void)arg;
    if (command != CMD_CHAT)
        return;
    if (is_visible())
        hide();
    else
        show();
}

/* .text+0x13c0 -- list 0x14: on 0x40 the menu item's check is the window's
 * visibility. */
static void FS98_STDCALL on_situation(uint32_t message, uint32_t arg)
{
    (void)arg;
    if (message != 0x40)
        return;
    if (is_visible())
        H_MENU_STATE(CMD_CHAT, 1);
    else
        H_MENU_STATE(CMD_CHAT, 0);
}

/* .text+0x1260 -- message 1: a session began. */
static void session_begin(void)
{
    H_DIALOG(0xff00, 0);
    fill_list();
    H_MENU_ADD(0x1108, CMD_CHAT, CMD_CHAT, 0);
    CHAIN_ADD(0x14, ADDR(on_situation));
    CHAIN_ADD(0xb, ADDR(on_menu));
    CHAIN_ADD(0xc, ADDR(on_command));
}

/* .text+0x12d0 -- message 2: it ended. */
static void session_end(void)
{
    CHAIN_REMOVE(0xc, ADDR(on_command));
    CHAIN_REMOVE(0xb, ADDR(on_menu));
    CHAIN_REMOVE(0x14, ADDR(on_situation));
    H_MENU_REMOVE(CMD_CHAT);
    destroy();
    M32(S_PLAYERS) = 0;
    M32(S_OTHERS) = 0;
    M32(S_LOCAL) = 0;
}

/* .text+0x1370 -- message 5: {sender, payload, size}.  A line of chat is
 * shown under its sender's name. */
static void on_message(uint32_t args)
{
    uint32_t player;

    if (M32(M32(args + 4)) != MSG_CHAT)
        return;
    player = MULTIP_PLAYER(M32(args));
    if (player == 0)
        say(M32(S_UNKNOWN), M32(args + 4) + 8, 0);
    else
        say(M32(player), M32(args + 4) + 8, 0);
}

/* .text+0x11a0 -- list 0x1d, MULTIP's session news. */
static void FS98_STDCALL on_session(uint32_t message, uint32_t arg)
{
    switch (message) {
    case 1:
        session_begin();
        break;
    case 2:
        session_end();
        break;
    case 3:                     /* .text+0x1330 */
        M32(S_PLAYERS) = arg;
        fill_list();
        break;
    case 4:                     /* .text+0x1350 */
        M32(S_OTHERS) = arg;
        fill_list();
        break;
    case 5:
        on_message(arg);
        break;
    }
}

/* ---- the Linkage ------------------------------------------------------------ */

/* .text+0x1020 -- +0x04 */
static uint32_t FS98_STDCALL mpchat_init(void)
{
    WNDCLASSA wc;

    M32(S_RICHED_MOD) = (uint32_t)(uintptr_t)LoadLibraryA(STR(S_RICHED));
    H_MODULE_IN(D + MC_LINKAGE, S_INSTANCE);
    H_STRING(0x9400, S_TITLE);
    H_STRING(0x9401, S_SEND_TEXT);
    H_STRING(0x9402, S_UNKNOWN);
    H_WINDOW_INST(S_MAIN_WND, S_MAIN_INST);
    wc.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = chat_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = DLGWINDOWEXTRA;
    wc.hInstance = (HINSTANCE)(uintptr_t)M32(S_INSTANCE);
    wc.hIcon = 0;
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = STR(S_CLASS);
    RegisterClassA(&wc);
    return CHAIN_ADD(0x1d, ADDR(on_session));
}

/* .text+0x1120 -- +0x08 */
static uint32_t FS98_STDCALL mpchat_deinit(void)
{
    CHAIN_REMOVE(0x1d, ADDR(on_session));
    UnregisterClassA(STR(S_CLASS), (HINSTANCE)(uintptr_t)M32(S_INSTANCE));
    H_MODULE_OUT(M32(S_INSTANCE));
    FreeLibrary((HMODULE)(uintptr_t)M32(S_RICHED_MOD));
    H_FREE(S_TITLE);
    H_FREE(S_SEND_TEXT);
    return H_FREE(S_UNKNOWN);
}

/* ---- the image -------------------------------------------------------------- */

struct mpchat_image mpchat_image = {{
#include "mpchat_data.inc"
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _mpchat_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _mpchat_image + 0x48");

static void mpchat_install(void)
{
    M32(D + MC_DIALOGS + 0x04) = ADDR(create);
    M32(D + MC_LINKAGE + 0x04) = ADDR(mpchat_init);
    M32(D + MC_LINKAGE + 0x08) = ADDR(mpchat_deinit);
    M32(D + MC_LINKAGE + 0x14) = D + MC_DIALOGS;
}

/* .text+0x1000 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        mpchat_install();
    }
    return TRUE;
}
