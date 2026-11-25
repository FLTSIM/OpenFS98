/* The frames a game window floats in (FS98FLOAT) and the 3D dialog windows (FS98DLG3D). */
#include "host_rt.h"
#include "fs98/arena.h"

#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define MAIN_WINDOW G32(0x41880c)
#define SEND(window, message, w, l) \
    WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l)
#define DEFAULT(window, message, w, l) \
    WIN(DefWindowProcA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l)

/* the hit test's answer for a frame whose window has fixed sizes (+0x68 == +0x70 the width,
 * +0x6c == +0x74 the height): a corner becomes an edge that still sizes, or the border */
static int32_t corner(const uint8_t *data, int32_t hit, int32_t vertical, int32_t horizontal)
{
    uint32_t width = D32(data, 0x68), height;
    if (width == D32(data, 0x70)) {
        height = D32(data, 0x6c);
        if (height == D32(data, 0x74) && width && height)
            return 0x12;
        if (width)
            return vertical;
    }
    height = D32(data, 0x6c);
    if (height == D32(data, 0x74) && height)
        return horizontal;
    return hit;
}

/* 0x405270: FS98FLOAT's procedure: the frame of a floating game window (its data is the
 * window's, in GWL_USERDATA; the window is its bitmap's +0x24). */
int32_t SC fs98_host_rt_floating_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l)
{
    uint32_t (SC *long_of)(uint32_t, int32_t) = (uint32_t (SC *)(uint32_t, int32_t))IMPORT(GetWindowLongA);
    uint8_t *data = (uint8_t *)(uintptr_t)long_of(window, -21), *bitmap = 0;
    uint32_t style = long_of(window, -16), extended = long_of(window, -20);
    int32_t rect[4];
    if (data)
        bitmap = (uint8_t *)PTR(data, 0xa0);
    switch (message) {
    case 1:             /* WM_CREATE */
        WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(window, -21, *(const uint32_t *)(uintptr_t)l);
        break;
    case 3:             /* WM_MOVE: the window's floating place kept, and it told */
        if (WIN(GetParent, uint32_t, (uint32_t))(D32(bitmap, 0x24)) != window)
            break;
        WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
        I32(data, 0x50) = rect[0];
        I32(data, 0x54) = rect[1];
        SEND(D32(bitmap, 0x24), 3, 0, 0);
        break;
    case 5:             /* WM_SIZE: the window made the frame's client size, its floating size kept */
        if (WIN(GetParent, uint32_t, (uint32_t))(D32(bitmap, 0x24)) != window)
            break;
        WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(window, rect);
        WIN(MoveWindow, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t))(D32(bitmap, 0x24), 0, 0, rect[2], rect[3], 1);
        WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
        D32(data, 0x58) = (uint32_t)rect[2] - (uint32_t)rect[0];
        D32(data, 0x5c) = (uint32_t)rect[3] - (uint32_t)rect[1];
        SEND(D32(bitmap, 0x24), 5, 0, 0);
        break;
    case 6:             /* WM_ACTIVATE: the main window told (0x46e) and the game (0x20007) */
        WIN(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(MAIN_WINDOW, 0x46e, w & 0xffff, window);
        fs98_host_rt_hook(0x20007, fs98_handle(data));
        return 0;
    case 0x10:          /* WM_CLOSE: the game's (0x20010) */
        fs98_host_rt_hook(0x20010, fs98_handle(data));
        return 0;
    case 0x14:          /* WM_ERASEBKGND */
        return 1;
    case 0x21:          /* WM_MOUSEACTIVATE: never activated; its caption drawn active */
        DEFAULT(window, 0x86, 1, 0);
        return 3;
    case 0x84: {        /* WM_NCHITTEST: no sizing where the window's size is fixed; the client is the caption */
        int32_t hit = DEFAULT(window, 0x84, w, l);
        switch (hit) {
        case 1:
            return 2;
        case 0xa: case 0xb:
            if (D32(data, 8) & 0x80400)
                return hit;
            return D32(data, 0x68) == D32(data, 0x70) && D32(data, 0x68) ? 0x12 : hit;
        case 0xc: case 0xf:
            if (D32(data, 8) & 0x80400)
                return hit;
            return D32(data, 0x6c) == D32(data, 0x74) && D32(data, 0x6c) ? 0x12 : hit;
        case 0xd:
            return D32(data, 8) & 0x80400 ? hit : corner(data, hit, 0xc, 0xa);
        case 0xe:
            return D32(data, 8) & 0x80400 ? hit : corner(data, hit, 0xc, 0xb);
        case 0x10:
            return D32(data, 8) & 0x80400 ? hit : corner(data, hit, 0xf, 0xa);
        case 0x11:
            return D32(data, 8) & 0x80400 ? hit : corner(data, hit, 0xf, 0xb);
        default:
            return hit;
        }
    }
    case 0x86:          /* WM_NCACTIVATE: drawn as the game is */
        DEFAULT(window, 0x86, G32(0x418858), 0);
        return 1;
    case 0xa4:          /* WM_NCRBUTTONDOWN on the caption: the window's menu */
        if (w == 2) {
            int32_t (SC *track)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, const void *) =
                (int32_t (SC *)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, const void *))
                    IMPORT(TrackPopupMenu);
            if (D32(data, 4) == 3)
                track(G32(0x41887c), 0, (int16_t)l, (int16_t)(l >> 16), 0, D32(bitmap, 0x24), 0);
            else
                track(G32(0x418884), 0, (int16_t)l, (int16_t)(l >> 16), 0, D32(bitmap, 0x24), 0);
        }
        /* fall through */
    case 0x311:         /* WM_PALETTECHANGED */
        if (G32(0x41a250))
            WIN(RedrawWindow, int32_t, (uint32_t, const void *, uint32_t, uint32_t))(window, 0, 0, 0x81);
        break;
    case 0x231:         /* WM_ENTERSIZEMOVE, WM_EXITSIZEMOVE, 0x46c, 0x46d: to the window */
    case 0x232:
    case 0x46c:
    case 0x46d:
        SEND(D32(bitmap, 0x24), message, w, l);
        break;
    case 0x467:         /* the frame's styles for a fixed or sizable window; shown as the window is */
        if (D32(data, 0x68) == D32(data, 0x70) && D32(data, 0x6c) == D32(data, 0x74) && D32(data, 0x68) && D32(data, 0x6c)) {
            uint32_t fixed = D32(data, 8) & 0x80400;
            uint32_t (SC *set)(uint32_t, int32_t, uint32_t) = (uint32_t (SC *)(uint32_t, int32_t, uint32_t))IMPORT(SetWindowLongA);
            if (!fixed && !(extended & 0x200)) {
                set(window, -16, style & 0xfffbffffu);
                set(window, -20, extended | 0x200);
            } else if (fixed && (extended & 0x200)) {
                set(window, -16, style | 0x40000);
                set(window, -20, extended & 0xfffffdffu);
            }
        }
        WIN(ShowWindow, int32_t, (uint32_t, int32_t))(window, (D32(data, 8) & 0x80000000) ? 5 : 0);
        WIN(UpdateWindow, int32_t, (uint32_t))(window);
        break;
    default:
        break;
    }
    return DEFAULT(window, message, w, l);
}

/* 0x4058a0: FS98DLG3D's procedure: a dialog drawn by the game, its data (as a game window's)
 * given with 0x467. Focused (0x418894), it is framed three deep; drawn through a palette of
 * its own (0x418898, from the one at 0x4175a8 with the flags cleared). */
int32_t SC fs98_host_rt_dialog_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l)
{
    uint8_t *data = (uint8_t *)(uintptr_t)WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -21), *bitmap;
    int32_t point[2];
    /* RETAIL BUG (kept): DIALOG-UNSET-BITMAP -- without data retail's bitmap is a local never
     * set; zero here (what the harness's fresh stack gives retail) */
    bitmap = data ? (uint8_t *)PTR(data, 0xa0) : 0;
    switch (message) {
    case 2:             /* WM_DESTROY */
        if (data) {
            if (D32(data, 0x98)) {
                WIN(DeleteObject, int32_t, (uint32_t))(D32(bitmap, 0x30));
                WIN(DeleteDC, int32_t, (uint32_t))(D32(bitmap, 0x38));
                D32(data, 0x98) = 0;
            }
            if (D32(data, 0x90))
                SERVICE(0x214, int32_t, (void *))(AT(data, 0x90));
        }
        if (G32(0x418898)) {
            WIN(DeleteObject, int32_t, (uint32_t))(G32(0x418898));
            G32(0x418898) = 0;
        }
        break;
    case 7:             /* WM_SETFOCUS */
        if (D32(bitmap, 0x70))
            G32(0x418894) = 1;
        WIN(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(window, 0);
        break;
    case 8:             /* WM_KILLFOCUS */
        G32(0x418894) = 0;
        break;
    case 0xf:           /* WM_PAINT, 0x466 */
    case 0x466: {
        uint32_t old, palette, frame[4], at;
        uint8_t table[0x400];
        int32_t (SC *frame_rect)(uint32_t, const void *, uint32_t) =
            (int32_t (SC *)(uint32_t, const void *, uint32_t))IMPORT(FrameRect);
        uint32_t (SC *select_palette)(uint32_t, uint32_t, int32_t) =
            (uint32_t (SC *)(uint32_t, uint32_t, int32_t))IMPORT(SelectPalette);
        old = WIN(SelectObject, uint32_t, (uint32_t, uint32_t))(D32(bitmap, 0x38), D32(bitmap, 0x30));
        if (G32(0x418894)) {
            uint32_t round;
            WIN(SetRect, int32_t, (void *, int32_t, int32_t, int32_t, int32_t))(frame, 0, 0, I32(data, 0x60), I32(data, 0x64));
            frame_rect(D32(bitmap, 0x38), frame, WIN(GetStockObject, uint32_t, (int32_t))(4));
            for (round = 0; round < 2; ++round) {
                ++frame[1];
                --frame[3];
                ++frame[0];
                --frame[2];
                frame_rect(D32(bitmap, 0x38), frame, WIN(GetStockObject, uint32_t, (int32_t))(4));
            }
        }
        for (at = 0; at < 0x400; at += 4) {
            G8(0x4175af + at) = 0;
            table[at + 2] = G8(0x4175ac + at);
            table[at + 1] = G8(0x4175ad + at);
            table[at] = G8(0x4175ae + at);
            table[at + 3] = 0;
        }
        WIN(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, const void *))(D32(bitmap, 0x38), 0, 0x100, table);
        if (G32(0x418898) == 0)
            G32(0x418898) = WIN(CreatePalette, uint32_t, (const void *))(VA(0x4175a8));
        D32(bitmap, 0x34) = WIN(GetDC, uint32_t, (uint32_t))(window);
        palette = select_palette(D32(bitmap, 0x34), G32(0x418898), 1);
        WIN(RealizePalette, uint32_t, (uint32_t))(D32(bitmap, 0x34));
        WIN(GetClientRect, int32_t, (uint32_t, void *))(D32(bitmap, 0x24), frame);
        WIN(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(bitmap, 0x34), 0, 0, (int32_t)frame[2], (int32_t)frame[3], D32(bitmap, 0x38), 0, 0, 0xcc0020);
        select_palette(D32(bitmap, 0x34), palette, 1);
        WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(window, D32(bitmap, 0x34));
        WIN(SelectObject, uint32_t, (uint32_t, uint32_t))(D32(bitmap, 0x38), old);
        break;
    }
    case 0x87:          /* WM_GETDLGCODE: every key and the arrows */
        return 0x81;
    case 0x100:         /* WM_KEYDOWN: to the game's dialog as 0x468 at (key, 0x7ffe) */
        point[0] = (int32_t)w;
        point[1] = 0x7ffe;
        fs98_host_rt_window_mouse(window, 0x468, 0, 0, point, data, bitmap);
        return 0;
    case 0x102:         /* WM_CHAR */
        return 0;
    case 0x201:         /* a button down, unfocused: the focus taken first */
    case 0x204:
    case 0x207:
        if (G32(0x418894) == 0 && D32(bitmap, 0x70)) {
            WIN(SetFocus, uint32_t, (uint32_t))(window);
            break;
        }
        /* fall through */
    case 0x200: case 0x202: case 0x203: case 0x205: case 0x206: case 0x208: case 0x209:
        point[0] = l & 0xffff;
        point[1] = l >> 16;
        return fs98_host_rt_window_mouse(window, message, w, l, point, data, bitmap);
    case 0x467:         /* its data: a picture with no tracking sizes, a DC, drawn */
        if (data == 0) {
            data = (uint8_t *)(uintptr_t)l;
            bitmap = (uint8_t *)PTR(data, 0xa0);
            WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(window, -21, l);
            {
                uint32_t flags = D32(data, 8);
                D32(data, 0x7c) = 0;
                D32(data, 8) = flags | 0x88020000u;
                D32(data, 0x68) = 0;
                D32(data, 0x70) = 0;
                D32(data, 0x6c) = 0;
                D32(data, 0x74) = 0;
            }
            D32(bitmap, 0x34) = WIN(GetDC, uint32_t, (uint32_t))(window);
            D32(bitmap, 0x38) = WIN(CreateCompatibleDC, uint32_t, (uint32_t))(D32(bitmap, 0x34));
            fs98_host_rt_make_picture(data, 0);
            WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(window, D32(bitmap, 0x34));
            WIN(SetBkMode, uint32_t, (uint32_t, int32_t))(D32(bitmap, 0x38), 1);
        }
        break;
    default:
        break;
    }
    return DEFAULT(window, message, w, l);
}
