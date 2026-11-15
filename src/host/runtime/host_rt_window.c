/* Helpers over the game's windows. */
#include "host_rt.h"

typedef int32_t (SC *window_callback)(uint32_t, uint32_t);
struct window_visit { window_callback callback; uint32_t parameter; };

/* 0x407710: how many entries (0x2c bytes each) a menu description takes, its end (kind 3)
 * included; a submenu (kind 1) is followed by its own description, counted with it. */
int32_t SC fs98_host_rt_menu_length(const uint32_t *entries)
{
    int32_t at = 0;
    while (entries[at * 11] != 3) {
        if (entries[at * 11] == 1)
            at += fs98_host_rt_menu_length(entries + (at + 1) * 11);
        ++at;
    }
    return at + 1;
}

/* 0x407760: a floating window's popup menu tracked at a point: kind 0 the window's own (the
 * menu at 0x41887c for one of type 3, else 0x418884), kind 1 the menu at 0x418880; other kinds
 * none. Not for a window with +8 bit 0x20000. The screen is given to GDI meanwhile, and the
 * hook told the menu began (0x20002) and ended (0x20003) unless a menu was already up. */
void SC fs98_host_rt_popup_menu(uint8_t *window, const int32_t *point, int32_t kind)
{
    uint8_t *bitmap = (uint8_t *)PTR(window, 0xa0);
    int32_t (SC *track)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, const void *) =
        (int32_t (SC *)(uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, const void *))
            IMPORT(TrackPopupMenu);
    if (D32(window, 8) & 0x20000)
        return;
    fs98_host_rt_full_screen_gdi(0);
    if (G32(0x418848) == 0) {
        G32(0x418848) = 2;
        fs98_host_rt_hook(0x20002, 0);
    }
    if (kind == 0) {
        if (D32(window, 4) == 3)
            track(G32(0x41887c), 0, point[0], point[1], 0, D32(bitmap, 0x24), 0);
        else
            track(G32(0x418884), 0, point[0], point[1], 0, D32(bitmap, 0x24), 0);
    } else if (kind == 1)
        track(G32(0x418880), 0, point[0], point[1], 0, D32(bitmap, 0x24), 0);
    if (G32(0x418848) == 2) {
        fs98_host_rt_hook(0x20003, 0);
        G32(0x418848) = 0;
        G32(0x41884c) = 1;
    }
    fs98_host_rt_full_screen_gdi(1);
}
