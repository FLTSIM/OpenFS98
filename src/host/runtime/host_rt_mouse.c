/* The mouse in a game window: its edges for sizing, its hot spots (the bitmap's +0x70 list:
 * each a rectangle and a mouse description, entries of 11 words as host_rt_window.c's menus:
 * 0 an item, 1 an area (its rectangle at +0x28, followed by its spots, 2, to its end, 3), with
 * +0x14 the cursor, +0x1c the events it takes, +0x20 an event to send (+0x3c) or +0x24 a
 * callback), its tool tips, the help and context menus, and auto-repeat (0x41888c the spot,
 * 0x41c5a0 the message to repeat, 0x41bfe4 the delay and 0x41c224 the rate, which the timer
 * uses). */
#include "host_rt.h"
#include "fs98/arena.h"

#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define MAIN_WINDOW G32(0x41880c)

/* what a hot spot is given: the message as the window had it, and the point in the picture */
struct mouse_context {
    uint32_t window, message, w, l;
    uint8_t *data, *bitmap;
    int32_t x, y;
};

typedef void (SC *spot_callback)(int32_t *offset, uint32_t event);

#define IN_RECT(rect, x, y) ((int32_t (SC *)(const void *, int32_t, int32_t))IMPORT(PtInRect))(rect, x, y)

/* auto-repeat stopped */
static void stop_repeat(void)
{
    G32(0x41888c) = 0;
    SERVICE(0x218, int32_t, (void *, uint32_t, uint32_t))(VA(0x41c5a0), 0, 0x20);
    G32(0x41bfe4) = 0;
    G32(0x41c224) = 0;
}

/* a spot's event: sent (+0x3c) if it has one, else its callback told; whether it had either */
static void send_event(const uint8_t *spot, int32_t *offset, uint32_t event)
{
    if (D32(spot, 0x20))
        SERVICE(0x3c, int32_t, (uint32_t, uint32_t))(D32(spot, 0x20), 0);
    else if (PTR(spot, 0x24))
        ((spot_callback)PTR(spot, 0x24))(offset, event);
}

/* 0x406fc0: +0x308's callback over a window's hot spots: the one under the point (or the one
 * holding the mouse) given the message. The spot list's element when it took the message,
 * else 0. */
uint32_t SC fs98_host_rt_hot_spot(uint8_t *element, struct mouse_context *context)
{
    uint8_t *spot;
    const int32_t *area;
    int32_t offset[2];
    uint32_t message;
    if (!IN_RECT(element, context->x, context->y))
        return 0;
    if (G32(0x41888c) == 0) {
        uint32_t *list = (uint32_t *)PTR(element, 0x10);
        uint32_t best = 0, at = 0, entry;
        if (list[0] != 0)
            do {
                if (list[at * 11] == 1 && IN_RECT((const void *)(uintptr_t)list[at * 11 + 10], context->x, context->y))
                    best = at;
                list = (uint32_t *)PTR(element, 0x10);
                ++at;
            } while (list[at * 11] != 0);
        list = (uint32_t *)PTR(element, 0x10);
        area = (const int32_t *)(uintptr_t)list[best * 11 + 10];
        offset[0] = (int32_t)((uint32_t)context->x - (uint32_t)area[0]);
        offset[1] = (int32_t)((uint32_t)context->y - (uint32_t)area[1]);
        entry = best + 1;
        while (list[entry * 11] != 3) {
            if (list[entry * 11] == 2) {
                if (IN_RECT((const void *)(uintptr_t)list[entry * 11 + 10], context->x, context->y))
                    best = entry;
            } else if (list[entry * 11] == 1)
                entry += (uint32_t)fs98_host_rt_menu_length(list + (entry + 1) * 11);
            ++entry;
            list = (uint32_t *)PTR(element, 0x10);
        }
        spot = (uint8_t *)((uint32_t *)PTR(element, 0x10) + best * 11);
    } else {
        spot = (uint8_t *)G_PTR(0x41888c);
        area = (const int32_t *)PTR(spot, 0x28);
        offset[0] = (int32_t)((uint32_t)context->x - (uint32_t)area[0]);
        offset[1] = (int32_t)((uint32_t)context->y - (uint32_t)area[1]);
    }
    if (context->message != 0x84) {
        D32(context->bitmap, 0x3c) = 0;
        if (D32(spot, 0x14) != 0xffffffffu) {
            uint32_t (SC *load)(uint32_t, uint32_t) = (uint32_t (SC *)(uint32_t, uint32_t))IMPORT(LoadCursorA);
            switch (D32(spot, 0x14)) {
            case 0: D32(context->bitmap, 0x3c) = load(0, 0x7f00); break;
            case 1: case 4: D32(context->bitmap, 0x3c) = load(G32(0x418828), 0x67); break;
            case 2: case 3: D32(context->bitmap, 0x3c) = load(G32(0x418828), 0x68); break;
            case 5: case 6: D32(context->bitmap, 0x3c) = load(G32(0x418828), 0x66); break;
            default: break;
            }
            WIN(SetCursor, uint32_t, (uint32_t))(D32(context->bitmap, 0x3c));
        }
    }
    message = context->message;
    switch (message) {
    case 0x84:
        return D32(spot, 0x1c) ? fs98_handle(element) : 0;
    case 0x204:         /* the right button down: its event (bit 31), else its tool tip's menu */
        if (D32(spot, 0x1c) & 0x2000000)
            G_PTR(0x41888c) = spot;
        if (D32(spot, 0x1c) & 0x80000000) {
            send_event(spot, offset, 0x80000000);
            if ((D32(spot, 0x1c) & 0x200000) && G32(0x41c5a0) == 0)
                goto repeat;
            return fs98_handle(element);
        }
        if (D32(spot, 0x1c) & 0x12000000)
            return 0;
        /* fall through */
    case 0xa4: {        /* the help menu for the spot's tool, at the point (0x41bb30) */
        struct { uint32_t window; int32_t x, y; uint32_t tool[11]; } hit;
        uint8_t *bitmap = context->bitmap;
        context->x = (int32_t)(D32(bitmap, 0x44) * (uint32_t)context->x) >> 16;
        context->y = (int32_t)(D32(bitmap, 0x48) * (uint32_t)context->y) >> 16;
        hit.tool[0] = 0x28;
        hit.window = context->window;
        hit.x = context->x;
        hit.y = context->y;
        if (WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(bitmap, 0x2c), 0x40a, 0, &hit) == 0)
            return 0;
        GI32(0x41bb30) = context->x;
        GI32(0x41bb34) = context->y;
        G32(0x41bb38) = hit.tool[9];
        WIN(ClientToScreen, int32_t, (uint32_t, void *))(context->window, &context->x);
        fs98_host_rt_popup_menu(context->data, &context->x, 1);
        return fs98_handle(element);
    }
    case 0x206:         /* the right button double-clicked */
        if (!(D32(spot, 0x1c) & 0x10000000))
            return 0;
        send_event(spot, offset, 0x10000000);
        return fs98_handle(element);
    case 0x201:         /* the left button down */
        if (D32(spot, 0x1c) & 0x800000)
            G_PTR(0x41888c) = spot;
        if (!(D32(spot, 0x1c) & 0x20000000))
            return 0;
        send_event(spot, offset, 0x20000000);
        if ((D32(spot, 0x1c) & 0x200000) && G32(0x41c5a0) == 0)
            goto repeat;
        return fs98_handle(element);
    case 0x203:         /* the left button double-clicked */
        if (!(D32(spot, 0x1c) & 0x4000000))
            return 0;
        send_event(spot, offset, 0x4000000);
        return fs98_handle(element);
    case 0x207:         /* the middle button down */
        if (D32(spot, 0x1c) & 0x1000000)
            G_PTR(0x41888c) = spot;
        if (!(D32(spot, 0x1c) & 0x40000000))
            return 0;
        send_event(spot, offset, 0x40000000);
        if ((D32(spot, 0x1c) & 0x200000) && G32(0x41c5a0) == 0)
            goto repeat;
        return fs98_handle(element);
    case 0x209:         /* the middle button double-clicked */
        if (!(D32(spot, 0x1c) & 0x8000000))
            return 0;
        send_event(spot, offset, 0x8000000);
        return fs98_handle(element);
    case 0x202:         /* a button up: the repeat stopped */
    case 0x205:
    case 0x208:
        stop_repeat();
        return 0;
    case 0x200:         /* the mouse moved: dragging with a button (left, middle, right), or over the spot */
        if (G32(0x41888c)) {
            uint32_t w;
            spot_callback callback;
            if (!IN_RECT(area, context->x, context->y) && G32(0x41c5a0)) {
                stop_repeat();
                return fs98_handle(element);
            }
            w = context->w;
            callback = (spot_callback)PTR(spot, 0x24);
            if ((w & 1) && (D32(spot, 0x1c) & 0x800000)) {
                if (callback)
                    callback(offset, 0x800000);
            } else if ((w & 0x10) && (D32(spot, 0x1c) & 0x1000000)) {
                if (callback)
                    callback(offset, 0x1000000);
            } else if ((w & 2) && (D32(spot, 0x1c) & 0x2000000)) {
                if (callback)
                    callback(offset, 0x2000000);
            }
            return fs98_handle(element);
        }
        if (!(D32(spot, 0x1c) & 0x400000) || PTR(spot, 0x24) == 0)
            return 0;
        ((spot_callback)PTR(spot, 0x24))(offset, 0x400000);
        return fs98_handle(element);
    case 0x468:         /* a key, from a 3D dialog */
        if (!(D32(spot, 0x1c) & 0x100000) || PTR(spot, 0x24) == 0)
            return 0;
        ((spot_callback)PTR(spot, 0x24))(offset, 0x100000);
        return fs98_handle(element);
    case 0x46a: {       /* help on the spot the menu was for (0x41bb38) */
        char *path = (char *)VA(0x41c480);
        uint32_t at = 0, end, topic = G32(0x41bb38);
        do
            path[at] = STR(0x41bca0)[at];
        while (STR(0x41bca0)[at++]);
        for (end = 0; path[end]; ++end)
            ;
        at = 0;
        do
            path[end + at] = STR(0x417b20)[at];
        while (STR(0x417b20)[at++]);
        fs98_host_rt_help(MAIN_WINDOW, path, 8, topic);
        G32(0x41bb38) = 0;
        G32(0x41bb34) = 0;
        G32(0x41bb30) = 0;
        return fs98_handle(element);
    }
    default:
        return 0;
    }
repeat:                 /* the message repeated by the timer: 18 ticks first, then every tick */
    G_PTR(0x41888c) = spot;
    SERVICE(0x21c, int32_t, (void *, const void *, uint32_t))(VA(0x41c5a0), context, 0x20);
    G32(0x41bfe4) = 0x12;
    G32(0x41c224) = 1;
    return fs98_handle(element);
}

/* the hit test's answer for an edge of a fixed size (+0x68 == +0x70 across, +0x6c == +0x74
 * down) on a window that may be sized */
static int32_t fixed_across(const uint8_t *data)
{
    return !(D32(data, 8) & 0x80400) && D32(data, 0x68) == D32(data, 0x70) && D32(data, 0x68);
}
static int32_t fixed_down(const uint8_t *data)
{
    return !(D32(data, 8) & 0x80400) && D32(data, 0x6c) == D32(data, 0x74) && D32(data, 0x6c);
}

/* 0x406a70: the mouse in a game window (FS98CHILD, FS98DLG3D): to the main window while the
 * game has the mouse (0x41881c); auto-repeat stopped by a move with no button; while the game
 * is active, its tool tips told and its hot spots offered it (the point scaled to the
 * picture); then the hit test (sizing within 10 of its edges, unless fixed there; the client
 * a caption, for a docked window), activation, the context menu and help. */
int32_t SC fs98_host_rt_window_mouse(uint32_t window, uint32_t message, uint32_t w, uint32_t l, const int32_t *point,
                                     uint8_t *data, uint8_t *bitmap)
{
    if (G32(0x41881c))
        return WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(MAIN_WINDOW, message, w, l);
    if (G32(0x41888c) && message == 0x200 && !(w & 0x13))
        stop_repeat();
    if (G32(0x418858)) {
        struct mouse_context context;
        if (D32(bitmap, 0x2c) && G32(0x417194)) {
            uint32_t relay[7], position;
            relay[0] = window;
            relay[1] = message;
            relay[2] = w;
            relay[3] = l;
            relay[4] = WIN(GetMessageTime, uint32_t, (void))();
            position = WIN(GetMessagePos, uint32_t, (void))();
            relay[5] = (uint32_t)(int32_t)(int16_t)position;
            relay[6] = (uint32_t)(int32_t)(int16_t)(position >> 16);
            WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(bitmap, 0x2c), 0x407, 0, relay);
        }
        context.l = l;
        context.w = w;
        context.window = window;
        context.message = message;
        context.data = data;
        context.bitmap = bitmap;
        context.x = point[0];
        context.y = point[1];
        if (D32(data, 8) & 0x90400) {
            context.x = (int32_t)(D32(bitmap, 0x4c) * (uint32_t)point[0]) >> 16;
            context.y = (int32_t)(D32(bitmap, 0x50) * (uint32_t)point[1]) >> 16;
        }
        if (SERVICE(0x308, int32_t, (void *, void *, void *))(bitmap + 0x70, &context, (void *)fs98_host_rt_hot_spot))
            return message == 0x84;
    }
    if (G32(0x41888c) && G32(0x41c5a0))
        stop_repeat();
    switch (message) {
    case 0x84: {
        int32_t hit, rect[4], x, y, left, top, right, bottom;
        if (D32(data, 8) & 0x40000000)
            break;
        hit = WIN(DefWindowProcA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x84, w, l);
        if (hit != 1)
            return hit;
        if (D32(data, 0x68) == D32(data, 0x70) && D32(data, 0x6c) == D32(data, 0x74) && (D32(data, 0x68) || D32(data, 0x6c)) &&
            !(D32(data, 8) & 0x80400))
            return 2;
        if (D32(data, 8) & 0x60000000)
            return 1;
        WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
        right = (int32_t)((uint32_t)rect[2] - 0xa);
        bottom = (int32_t)((uint32_t)rect[3] - 0xa);
        left = (int32_t)((uint32_t)rect[0] + 0xa);
        top = (int32_t)((uint32_t)rect[1] + 0xa);
        x = (int16_t)l;
        if (x < left) {
            y = (int16_t)(l >> 16);
            if (y < top)
                return fixed_across(data) ? 0xc : fixed_down(data) ? 0xa : 0xd;
            if (y > bottom)
                return fixed_across(data) ? 0xf : fixed_down(data) ? 0xa : 0x10;
        }
        if (x > right) {
            y = (int16_t)(l >> 16);
            if (y < top)
                return fixed_across(data) ? 0xc : fixed_down(data) ? 0xb : 0xe;
            if (y > bottom)
                return fixed_across(data) ? 0xf : fixed_down(data) ? 0xb : 0x11;
        }
        if (x < left)
            return fixed_across(data) ? 2 : 0xa;
        if (x > right)
            return fixed_across(data) ? 2 : 0xb;
        y = (int16_t)(l >> 16);
        if (y < top)
            return fixed_down(data) ? 2 : 0xc;
        if (y > bottom)
            return fixed_down(data) ? 2 : 0xf;
        return 2;
    }
    case 0xa1:          /* a left button down: a floating window's frame activated, else the game told */
    case 0x201:
        if (w == 1 || w == 2) {
            if (D32(data, 8) & 0x40000000)
                WIN(SetActiveWindow, uint32_t, (uint32_t))(D32(bitmap, 0x28));
            else
                fs98_host_rt_hook(0x20007, fs98_handle(data));
        }
        break;
    case 0xa4:          /* a right button down: the window's menu */
    case 0x204:
        WIN(ClientToScreen, int32_t, (uint32_t, const void *))(window, point);
        fs98_host_rt_popup_menu(data, point, 0);
        break;
    case 0x46a:
        return -1;
    default:
        break;
    }
    return WIN(DefWindowProcA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l);
}
