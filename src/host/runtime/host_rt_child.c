/* The game's windows (FS98CHILD): the views, gauges and the rest a module makes. Each has its
 * data in GWL_USERDATA (CREATESTRUCT's parameter): +4 its type, +8 its flags, +0x18 the
 * module's handler, +0x1c the view, +0x30 its place docked (x, y, width, height), +0x40 the
 * place it had, +0x50 its place floating (in its frame, bitmap +0x28), +0x60 the picture's
 * size, +0x68 its tracking sizes, +0x7c a key shared pictures are kept under (0x417ae8), +0x80
 * the picture, +0x90 its bits, +0x98 whether it has a bitmap, +0xa0 its bitmap: +0 the display
 * count it was made for (0x41ba2c), +4 the frame count it was placed at (0x41bb2c), +0xc its
 * place on the screen, +0x24 the window, +0x28 the frame, +0x30 the bitmap, +0x34 the window's
 * DC, +0x38 a memory DC, +0x3c its cursor, +0x54 its pieces, +0x70, +0x8c its clipper. */
#include "host_rt.h"
#include "fs98/arena.h"

#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define MAIN_WINDOW G32(0x41880c)
#define SEND(window, message, w, l) \
    WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l)

typedef int32_t (SC *window_handler)(uint32_t kind, uint8_t *data, uint32_t flags, void *rect);
#define HANDLER(data) ((window_handler)PTR(data, 0x18))
/* what a handler is told along with its size: not resizing (0x418860), not moving (0x418888) */
#define SIZE_FLAGS() ((uint32_t)(G32(0x418860) == 0) | (uint32_t)(G32(0x418888) == 0) << 16)

static void copy_rect(uint8_t *to, const uint8_t *from)
{
    D32(to, 0) = D32(from, 0);
    D32(to, 4) = D32(from, 4);
    D32(to, 8) = D32(from, 8);
    D32(to, 0xc) = D32(from, 0xc);
}

/* WM_CREATE: the data taken from CREATESTRUCT; the window's DC and a memory DC with the
 * palette; a shared picture found or added under its key, and the window added to it; the
 * floating windows' menus loaded (0x418878: submenus 0, 1, 2 at 0x41887c..84). */
static void created(uint32_t window, const uint32_t *create)
{
    uint8_t *data = (uint8_t *)(uintptr_t)create[0], *bitmap = (uint8_t *)PTR(data, 0xa0), *shared;
    uint32_t (SC *mode)(uint32_t, int32_t) = (uint32_t (SC *)(uint32_t, int32_t))IMPORT(SetBkMode);
    uint32_t (SC *select)(uint32_t, uint32_t, int32_t) =
        (uint32_t (SC *)(uint32_t, uint32_t, int32_t))IMPORT(SelectPalette);
    uint32_t (SC *submenu)(uint32_t, int32_t) = (uint32_t (SC *)(uint32_t, int32_t))IMPORT(GetSubMenu);
    uint32_t menu;
    D32(bitmap, 0x34) = WIN(GetDC, uint32_t, (uint32_t))(window);
    mode(D32(bitmap, 0x34), 1);
    select(D32(bitmap, 0x34), G32(0x41bb60), 0);
    WIN(SetStretchBltMode, int32_t, (uint32_t, int32_t))(D32(bitmap, 0x34), 3);
    D32(bitmap, 0x38) = WIN(CreateCompatibleDC, uint32_t, (uint32_t))(D32(bitmap, 0x34));
    mode(D32(bitmap, 0x38), 1);
    select(D32(bitmap, 0x38), G32(0x41bb60), 0);
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(window, -21, fs98_handle(data));
    if (D32(data, 0x7c)) {
        uint8_t *entry;
        shared = SERVICE(0x310, uint8_t *, (void *, uint32_t, void *, uint32_t, uint32_t))(VA(0x417ae8), 0, AT(data, 0x7c), 4, 0);
        if (shared == 0) {
            shared = SERVICE(0x2f8, uint8_t *, (void *))(VA(0x417ae8));
            D32(shared, 0) = D32(data, 0x7c);
            fs98_host_rt_copy_words(shared + 4, data + 0x80, 8);
            D32(shared, 0x24) = 0;
            D32(shared, 0x28) = 0;        /* its windows: a list of pointers */
            D32(shared, 0x2c) = 0;
            D32(shared, 0x30) = 0x20;
            D32(shared, 0x34) = 4;
            D32(shared, 0x38) = 0;
            D32(shared, 0x3c) = 0;
            D32(shared, 0x40) = 0;
        }
        if (SERVICE(0x310, void *, (void *, uint32_t, void *, uint32_t, uint32_t))(shared + 0x28, 0, &data, 4, 0) == 0) {
            entry = SERVICE(0x2f8, uint8_t *, (void *))(shared + 0x28);
            PTR(entry, 0) = data;
        }
    }
    if (G32(0x418878))
        WIN(DestroyMenu, int32_t, (uint32_t))(G32(0x418878));
    menu = WIN(LoadMenuA, uint32_t, (uint32_t, uint32_t))(G32(0x41882c), 0x66);
    G32(0x418878) = menu;
    G32(0x41887c) = submenu(menu, 0);
    G32(0x418880) = submenu(G32(0x418878), 1);
    G32(0x418884) = submenu(G32(0x418878), 2);
}

/* WM_DESTROY: the clipper released; the bitmap and bits freed, or the window taken off its
 * shared picture, and the picture freed with its last window (and the game told, 0x46d); the
 * DCs let go. */
static void destroyed(uint8_t *data, uint8_t *bitmap, uint32_t window)
{
    fs98_host_rt_release_clipper(data);
    if (D32(data, 0x7c) == 0) {
        if (D32(data, 0x98)) {
            WIN(DeleteObject, int32_t, (uint32_t))(D32(bitmap, 0x30));
            D32(data, 0x98) = 0;
        }
        if (D32(data, 0x90))
            SERVICE(0x214, int32_t, (void *))(AT(data, 0x90));
    } else {
        uint8_t *shared = SERVICE(0x310, uint8_t *, (void *, uint32_t, void *, uint32_t, uint32_t))(VA(0x417ae8), 0, AT(data, 0x7c), 4, 0);
        if (shared) {
            uint8_t *windows = shared + 0x28;
            void *entry = SERVICE(0x310, void *, (void *, uint32_t, void *, uint32_t, uint32_t))(windows, 0, &data, 4, 0);
            if (entry)
                SERVICE(0x300, int32_t, (void *, void *))(windows, entry);
            if (D32(windows, 0) == 0) {
                if (D32(shared, 0x1c))
                    WIN(DeleteObject, int32_t, (uint32_t))(D32(bitmap, 0x30));
                if (D32(shared, 0x14))
                    SERVICE(0x214, int32_t, (void *))(AT(shared, 0x14));
                SERVICE(0x2fc, int32_t, (void *))(windows);
                SERVICE(0x300, int32_t, (void *, void *))(VA(0x417ae8), shared);
                SEND(window, 0x46d, 0, 0);
            }
        }
    }
    WIN(DeleteDC, int32_t, (uint32_t))(D32(bitmap, 0x38));
    WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(D32(bitmap, 0x24), D32(bitmap, 0x34));
}

/* WM_PAINT (with the palette realised and the module's handler drawing, kind 0, between
 * +0x6c and +0x70, for a shared picture) and 0x466: the picture to the window -- through the
 * full screen's own path (0x414100) when there is one and the window is not a floating one
 * on a second display; else from the memory DC: piece by piece (0x405f40 over +0x54) when
 * only those changed (0x466 at the same display count), else whole, by DirectDraw onto the
 * primary (with the window's clipper), through 0x40ea20 for the 16-bit GDI path (0x41a250)
 * or stretched. */
static void painted(uint32_t window, uint32_t message, uint8_t *data, uint8_t *bitmap, int32_t realise)
{
    uint32_t pieces, old;
    int32_t rect[4], width, height;
    if (realise) {
        WIN(RealizePalette, uint32_t, (uint32_t))(D32(bitmap, 0x34));
        if (D32(data, 0x7c) && D32(data, 0x18)) {
            SERVICE(0x6c, int32_t, (void *))(data);
            HANDLER(data)(0, data, 0, 0);
            SERVICE(0x70, int32_t, (void *))(data);
        }
    }
    pieces = D32(bitmap, 0) == G32(0x41ba2c) ? message == 0x466 : 0;
    if (D32(bitmap, 4) != G32(0x41bb2c))
        fs98_host_rt_place_bitmap(data);
    if (G32(0x41a4f4) && G32(0x418858) == 1 && (GI32(0x4183b4) <= 1 || !(D32(data, 8) & 0x40000000))) {
        fs98_host_rt_redraw_window(window, (int32_t)pieces);
        return;
    }
    old = WIN(SelectObject, uint32_t, (uint32_t, uint32_t))(D32(bitmap, 0x38), D32(bitmap, 0x30));
    if (D32(bitmap, 0) != G32(0x41ba2c))
        fs98_host_rt_colour_table(data);
    if ((D32(data, 8) & 0x10000000) && pieces)
        SERVICE(0x308, int32_t, (void *, void *, void *))(bitmap + 0x54, data, (void *)fs98_host_rt_draw_piece_gdi);
    else {
        uint32_t gdi16;
        WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(bitmap, 0x24), rect);
        if (!(D32(data, 8) & 0x90000)) {
            rect[2] = I32(data, 0x60);
            rect[3] = I32(data, 0x64);
        }
        width = rect[2];
        height = rect[3];
        gdi16 = G32(0x41a250);
        if (D32(data, 0x7c) && G32(0x41a4e0) && G32(0x418858) == 1 && !gdi16) {
            uint32_t flags = D32(data, 8);
            if (!(flags & 0x10000) && (GI32(0x4183b4) <= 1 || !(flags & 0x40000000))) {
                if (G32(0x41a4f0) != D32(bitmap, 0x8c)) {
                    void *primary = G_PTR(0x41a4f8);
                    ((int32_t (SC *)(void *, uint32_t))PTR(PTR(primary, 0), 0x70))(primary, D32(bitmap, 0x8c));
                    G32(0x41a4f0) = D32(bitmap, 0x8c);
                }
                fs98_host_rt_blit(G_PTR(0x41a4f8), I32(bitmap, 0xc), I32(bitmap, 0x10), width, height, G_PTR(0x41a4e0), 0, 0,
                                  I32(data, 0x60), I32(data, 0x64), 0);
                goto drawn;
            }
        }
        if (gdi16)
            fs98_host_rt_gdi_blit(D32(bitmap, 0x34), 0, 0, width, height, D32(bitmap, 0x38), 0, 0, I32(data, 0x60),
                                I32(data, 0x64), D32(data, 8));
        else
            WIN(StretchBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t,
                                      int32_t, uint32_t))(D32(bitmap, 0x34), 0, 0, width, height, D32(bitmap, 0x38), 0, 0,
                                                          I32(data, 0x60), I32(data, 0x64), 0xcc0020);
    }
drawn:
    if (D32(data, 8) & 0x10000000)
        SERVICE(0x304, int32_t, (void *))(bitmap + 0x54);
    WIN(SelectObject, uint32_t, (uint32_t, uint32_t))(D32(bitmap, 0x38), old);
}

/* 0x467, the main window resized: the window docked into the main window or floated in its
 * frame as its flags say (+8 bit 30), shown or hidden (bit 31, the handler told: 3 shown, 4
 * hidden), its styles put back and its frame redrawn, then placed and told. */
static void relaid(uint32_t window, uint8_t *data, uint8_t *bitmap)
{
    uint32_t style, extended, visible, redraw, flags;
    int32_t rect[4];
    uint32_t (SC *long_of)(uint32_t, int32_t) = (uint32_t (SC *)(uint32_t, int32_t))IMPORT(GetWindowLongA);
    uint32_t (SC *parent_of)(uint32_t) = (uint32_t (SC *)(uint32_t))IMPORT(GetParent);
    int32_t (SC *move)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t) =
        (int32_t (SC *)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t))IMPORT(MoveWindow);
    if (D32(data, 0x38) == 0 || D32(data, 0x3c) == 0)
        copy_rect(data + 0x30, data + 0x40);
    ++G32(0x418890);
    style = long_of(window, -16);
    extended = long_of(window, -20);
    visible = style & 0x10000000;
    if (!visible && (D32(data, 8) & 0x80000000) && D32(data, 0x18)) {
        WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(window, rect);
        HANDLER(data)(3, data, 0, rect);
    } else if (visible && !(D32(data, 8) & 0x80000000) && D32(data, 0x18)) {
        WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(window, rect);
        HANDLER(data)(4, data, 0, rect);
    }
    redraw = visible && !(D32(data, 8) & 0x80000000);
    if (D32(data, 8) & 0x40000000) {
        if (parent_of(window) == MAIN_WINDOW) {
            copy_rect(data + 0x40, data + 0x30);
            WIN(SetParent, uint32_t, (uint32_t, uint32_t))(window, D32(bitmap, 0x28));
            if (D32(data, 0x58) == 0 || D32(data, 0x5c) == 0) {
                uint32_t frame_extended, frame_style;
                WIN(GetClientRect, int32_t, (uint32_t, void *))(window, AT(data, 0x50));
                frame_extended = long_of(D32(bitmap, 0x28), -20);
                frame_style = long_of(D32(bitmap, 0x28), -16);
                WIN(AdjustWindowRectEx, int32_t, (void *, uint32_t, int32_t, uint32_t))(AT(data, 0x50), frame_style, 0, frame_extended);
                if (I32(data, 0x50) < 0) {
                    D32(data, 0x58) -= D32(data, 0x50);
                    D32(data, 0x50) = 0;
                }
                if (I32(data, 0x54) < 0) {
                    D32(data, 0x5c) -= D32(data, 0x54);
                    D32(data, 0x54) = 0;
                }
            }
            move(D32(bitmap, 0x28), I32(data, 0x50), I32(data, 0x54), I32(data, 0x58), I32(data, 0x5c), 1);
            if (D32(data, 8) & 0x80000000) {
                WIN(ShowWindow, int32_t, (uint32_t, int32_t))(D32(bitmap, 0x28), 5);
                WIN(UpdateWindow, int32_t, (uint32_t))(D32(bitmap, 0x28));
            }
            redraw = 1;
        }
    } else if (parent_of(window) != MAIN_WINDOW) {
        WIN(ShowWindow, int32_t, (uint32_t, int32_t))(D32(bitmap, 0x28), 0);
        WIN(UpdateWindow, int32_t, (uint32_t))(D32(bitmap, 0x28));
        move(window, I32(data, 0x30), I32(data, 0x34), I32(data, 0x38), I32(data, 0x3c), 1);
        WIN(SetParent, uint32_t, (uint32_t, uint32_t))(window, MAIN_WINDOW);
        redraw = 1;
        copy_rect(data + 0x30, data + 0x40);
    }
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(window, -16, style);
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(window, -20, extended);
    WIN(SetWindowPos, int32_t, (uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))(
        window, 0, 0, 0, 0, 0, (D32(data, 8) & 0x80000000) ? 0x77 : 0xb7);
    if (redraw)
        WIN(RedrawWindow, int32_t, (uint32_t, const void *, uint32_t, uint32_t))(MAIN_WINDOW, 0, 0, 0x181);
    if (!(D32(data, 8) & 0x8000000))
        fs98_host_rt_dock_place(data);
    flags = D32(data, 8);
    if (flags & 0x100000)
        D32(data, 8) = flags & 0xffefffffu;
    else if ((flags & 0x7e00000) && !(flags & 0x80000))
        fs98_host_rt_align(data);
    if (D32(data, 8) & 0x60000000) {
        WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(parent_of(window), rect);
        move(window, rect[0], rect[1], (int32_t)((uint32_t)rect[2] - (uint32_t)rect[0]),
             (int32_t)((uint32_t)rect[3] - (uint32_t)rect[1]), 1);
    } else
        move(window, I32(data, 0x30), I32(data, 0x34), I32(data, 0x38), I32(data, 0x3c), 1);
    fs98_host_rt_make_picture(data, 0);
    if (redraw)
        SEND(window, 0x232, 0, 0);
    if (parent_of(window) != MAIN_WINDOW)
        SEND(parent_of(window), 0x467, 0, 0);
    fs98_host_rt_restack();
    --G32(0x418890);
}

/* WM_INITMENUPOPUP's work on the floating window's menu (WM_INITMENU, 0x116): the view
 * commands (0x100..0x105) and float (0x200) checked and enabled for a view (type 3) as its
 * view (+0x1c: +0x10 set, else its kind +0x14) and flags are; dock (0x201) for a floating
 * one; the menu (0x300) as it is shown; both only under DirectDraw's full screen. */
static void menu_shown(uint32_t menu, uint8_t *data)
{
    static const uint32_t items[8] = {0x100, 0x101, 0x102, 0x103, 0x105, 0x200, 0x201, 0x300};
    uint32_t (SC *check)(uint32_t, uint32_t, uint32_t) = (uint32_t (SC *)(uint32_t, uint32_t, uint32_t))IMPORT(CheckMenuItem);
    int32_t (SC *enable)(uint32_t, uint32_t, uint32_t) = (int32_t (SC *)(uint32_t, uint32_t, uint32_t))IMPORT(EnableMenuItem);
    uint32_t at;
    for (at = 0; at < 8; ++at)
        check(menu, items[at], 0);
    for (at = 0; at < 6; ++at)
        enable(menu, items[at], 1);
    if (D32(data, 4) == 3) {
        uint8_t *view;
        for (at = 0; at < 6; ++at)
            enable(menu, items[at], 0);
        view = (uint8_t *)PTR(data, 0x1c);
        if (D32(view, 0x10))
            check(menu, 0x105, 8);
        else
            switch (D32(view, 0x14)) {
            case 0: check(menu, 0x100, 8); break;
            case 1: check(menu, 0x101, 8); break;
            case 2: check(menu, 0x102, 8); break;
            case 4: check(menu, 0x103, 8); break;
            default: break;
            }
        if (D32(data, 8) & 0x20000000)
            check(menu, 0x200, 8);
    }
    if (D32(data, 8) & 0x40000000) {
        enable(menu, 0x200, 1);
        check(menu, 0x201, 8);
    }
    if (WIN(GetMenu, uint32_t, (uint32_t))(MAIN_WINDOW) == 0)
        check(menu, 0x300, 8);
    enable(menu, 0x300, G32(0x41a4f4) != 0);
    enable(menu, 0x201, G32(0x41a4f4) != 0);
}

/* WM_SIZING, WM_MOVING: a shared picture's window kept inside its parent's client area; the
 * full screen's floating window moved with it */
static void dragged(uint32_t window, uint8_t *data, int32_t *drag)
{
    if (D32(data, 0x7c)) {
        int32_t area[4];
        uint32_t parent = WIN(GetParent, uint32_t, (uint32_t))(window);
        int32_t (SC *to_screen)(uint32_t, void *) = (int32_t (SC *)(uint32_t, void *))IMPORT(ClientToScreen);
        int32_t (SC *offset)(void *, int32_t, int32_t) = (int32_t (SC *)(void *, int32_t, int32_t))IMPORT(OffsetRect);
        WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(parent, area);
        to_screen(parent, &area[0]);
        to_screen(parent, &area[2]);
        if (drag[0] < area[0])
            offset(drag, (int32_t)((uint32_t)area[0] - (uint32_t)drag[0]), 0);
        if (drag[2] > area[2])
            offset(drag, (int32_t)((uint32_t)area[2] - (uint32_t)drag[2]), 0);
        if (drag[1] < area[1])
            offset(drag, 0, (int32_t)((uint32_t)area[1] - (uint32_t)drag[1]));
        if (drag[3] > area[3])
            offset(drag, 0, (int32_t)((uint32_t)area[3] - (uint32_t)drag[3]));
        WIN(IntersectRect, int32_t, (void *, const void *, const void *))(drag, drag, area);
    }
    if (G32(0x41a4f4))
        fs98_host_rt_move_window(window, drag);
}

/* 0x403f80: FS98CHILD's procedure. */
int32_t SC fs98_host_rt_child_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l)
{
    uint8_t *data = (uint8_t *)(uintptr_t)WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -21), *bitmap = 0;
    int32_t rect[4], point[2];
    if (data)
        bitmap = (uint8_t *)PTR(data, 0xa0);
    if (bitmap && D32(bitmap, 0x24) == 0)
        D32(bitmap, 0x24) = window;
    switch (message) {
    case 1:             /* WM_CREATE */
        created(window, (const uint32_t *)(uintptr_t)l);
        break;
    case 2:             /* WM_DESTROY */
        destroyed(data, bitmap, window);
        break;
    case 3:             /* WM_MOVE: its docked place kept (a floating one's not), the module told (2) */
        ++G32(0x41bb2c);
        WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
        point[0] = rect[0];
        point[1] = rect[1];
        WIN(ScreenToClient, int32_t, (uint32_t, int32_t *))(MAIN_WINDOW, point);
        if (!(D32(data, 8) & 0x60000000)) {
            I32(data, 0x30) = point[0];
            I32(data, 0x40) = point[0];
            I32(data, 0x34) = point[1];
            I32(data, 0x44) = point[1];
        }
        if (!(D32(data, 8) & 0x8000000) && G32(0x418890) == 0)
            fs98_host_rt_dock_units(data);
        fs98_host_rt_restack();
        if (D32(data, 0x18)) {
            WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(window, rect);
            HANDLER(data)(2, data, SIZE_FLAGS(), rect);
        }
        break;
    case 5:             /* WM_SIZE: its docked size kept, the picture remade, the module told (1) */
        ++G32(0x41bb2c);
        WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
        if (!(D32(data, 8) & 0x60000000)) {
            D32(data, 0x38) = (uint32_t)rect[2] - (uint32_t)rect[0] + 1;
            D32(data, 0x48) = D32(data, 0x38);
            D32(data, 0x3c) = (uint32_t)rect[3] - (uint32_t)rect[1] + 1;
            D32(data, 0x4c) = D32(data, 0x3c);
        }
        if (!(D32(data, 8) & 0x8000000) && G32(0x418890) == 0)
            fs98_host_rt_dock_units(data);
        if ((D32(data, 0x38) && D32(data, 0x3c)) || (D32(data, 8) & 0x60000000))
            fs98_host_rt_make_picture(data, 0);
        fs98_host_rt_restack();
        if (D32(data, 0x18)) {
            WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(window, rect);
            HANDLER(data)(1, data, SIZE_FLAGS(), rect);
        }
        break;
    case 0xf:           /* WM_PAINT */
        painted(window, message, data, bitmap, 1);
        break;
    case 0x14:          /* WM_ERASEBKGND: never */
        return 1;
    case 0x20:          /* WM_SETCURSOR: the window's own cursor */
        if (G32(0x41885c) == 0 && w == 2 && bitmap && D32(bitmap, 0x3c)) {
            WIN(SetCursor, uint32_t, (uint32_t))(D32(bitmap, 0x3c));
            return 1;
        }
        break;
    case 0x22:          /* WM_CHILDACTIVATE */
        fs98_host_rt_restack();
        break;
    case 0x24: {        /* WM_GETMINMAXINFO: the window's tracking sizes, or 40x40 at least */
        uint8_t *info = (uint8_t *)(uintptr_t)l;
        if (D32(data, 8) & 0x80400) {
            D32(info, 0x18) = 0x28;
            D32(info, 0x1c) = 0x28;
            break;
        }
        if (D32(data, 0x68))
            D32(info, 0x18) = D32(data, 0x68);
        if (D32(data, 0x6c))
            D32(info, 0x1c) = D32(data, 0x6c);
        if (D32(data, 0x70))
            D32(info, 0x20) = D32(data, 0x70);
        if (D32(data, 0x74))
            D32(info, 0x24) = D32(data, 0x74);
        break;
    }
    case 0x84:          /* WM_NCHITTEST and the non-client mouse: at the point in the window */
    case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7: case 0xa8: case 0xa9:
        point[0] = l & 0xffff;
        point[1] = l >> 16;
        WIN(ScreenToClient, int32_t, (uint32_t, int32_t *))(window, point);
        return fs98_host_rt_window_mouse(window, message, w, l, point, data, bitmap);
    case 0x111:         /* WM_COMMAND: the floating window's menu (whole wParam) */
        switch (w) {
        case 0x100: fs98_host_rt_hook(0x20008, fs98_handle(data)); return 0;
        case 0x101: fs98_host_rt_hook(0x20009, fs98_handle(data)); return 0;
        case 0x102: fs98_host_rt_hook(0x2000a, fs98_handle(data)); return 0;
        case 0x103: fs98_host_rt_hook(0x2000b, fs98_handle(data)); return 0;
        case 0x105: fs98_host_rt_hook(0x2000d, fs98_handle(data)); return 0;
        case 0x200: fs98_host_rt_hook(0x2000e, fs98_handle(data)); return 0;
        case 0x201: fs98_host_rt_hook(0x2000f, fs98_handle(data)); return 0;
        case 0x202: fs98_host_rt_hook(0x20010, fs98_handle(data)); return 0;
        case 0x300:     /* the main window's menu shown or hidden */
            if (G32(0x418854)) {
                G32(0x417198) = G32(0x417198) == 0;
                WIN(SetMenu, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, G32(0x417198) ? 0 : G32(0x418810));
            } else {
                G32(0x418824) = G32(0x418824) == 0;
                WIN(SetMenu, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, G32(0x418824) ? 0 : G32(0x418810));
            }
            return 0;
        case 0x400:
            fs98_host_rt_hook(0x10053, 0);
            return 0;
        case 0x500: {   /* help: the window's own (0x46a to 0x406a70, unless it answers -1), else its topic */
            char *path = (char *)VA(0x41c480);
            uint32_t at = 0, end;
            if (D32(bitmap, 0x70) && G32(0x41bb38) &&
                fs98_host_rt_window_mouse(window, 0x46a, 0, 0, (int32_t *)VA(0x41bb30), data, bitmap) != -1)
                return 0;
            do
                path[at] = STR(0x41bca0)[at];
            while (STR(0x41bca0)[at++]);
            for (end = 0; path[end]; ++end)
                ;
            at = 0;
            do
                path[end + at] = STR(0x417b04)[at];
            while (STR(0x417b04)[at++]);
            fs98_host_rt_help(MAIN_WINDOW, path, 8, D32(data, 4) + 0xaf00);
            return 0;
        }
        default:
            break;
        }
        break;
    case 0x116:         /* WM_INITMENU */
        menu_shown(w, data);
        break;
    case 0x200: case 0x201: case 0x202: case 0x203: case 0x204:
    case 0x205: case 0x206: case 0x207: case 0x208: case 0x209:
        point[0] = l & 0xffff;
        point[1] = l >> 16;
        return fs98_host_rt_window_mouse(window, message, w, l, point, data, bitmap);
    case 0x214:         /* WM_SIZING, WM_MOVING */
    case 0x216:
        dragged(window, data, (int32_t *)(uintptr_t)l);
        break;
    case 0x231:         /* WM_ENTERSIZEMOVE, unless laying out */
        if (G32(0x418890) == 0) {
            G32(0x418860) = 1;
            G32(0x418888) = 1;
        }
        break;
    case 0x232:         /* WM_EXITSIZEMOVE: moved, and resized if the size changed */
        G32(0x418860) = 0;
        G32(0x418888) = 0;
        WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(window, rect);
        SEND(window, 3, 0, 0);
        if (I32(data, 0x60) != rect[2] || I32(data, 0x64) != rect[3])
            SEND(window, 5, 0, 0);
        break;
    case 0x466:         /* drawn again (only its pieces, if that is all) */
        painted(window, message, data, bitmap, 0);
        break;
    case 0x467:
        relaid(window, data, bitmap);
        break;
    case 0x46c:         /* the picture remade */
        ++G32(0x41bb2c);
        if (data && D32(data, 0x18))
            fs98_host_rt_make_picture(data, 1);
        break;
    case 0x46d:         /* the shared picture gone: the module told (5), the clipper released */
        ++G32(0x41bb2c);
        if (data) {
            if (D32(data, 0x18))
                HANDLER(data)(5, data, 0, 0);
            if (data)
                fs98_host_rt_release_clipper(data);
        }
        break;
    default:
        break;
    }
    return WIN(DefWindowProcA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l);
}
