/* The rest of the runtime: the modeless dialogs and the keys a message is offered to, the
 * menu bar's redraw, the paused look (the grey palette and the dithered 216-colour cube the
 * 16-bit GDI path draws through), sound's start, tick and end, the frames' stacking, and the
 * lists let go at the end. */
#include "host_rt.h"
#include "fs98/arena.h"

#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define CFG ((const char *)VA(0x41c120))
#define MAIN_WINDOW G32(0x41880c)

/* ---- the lists let go at the end -------------------------------------------------------- */

/* 0x40cc10: the key tables (0x418278) */
void fs98_host_rt_40cc10(void)
{
    SERVICE(0x2fc, int32_t, (void *))(VA(0x418278));
}

/* 0x40ee40: the libraries list (0x4182e0) */
void fs98_host_rt_40ee40(void)
{
    SERVICE(0x2fc, int32_t, (void *))(VA(0x4182e0));
}

/* 0x410400: the removals queued (0x418380) */
void fs98_host_rt_410400(void)
{
    SERVICE(0x2fc, int32_t, (void *))(VA(0x418380));
}

/* ---- the modeless dialogs (0x417f00: {module, window}) ---------------------------------- */

/* ---- the game's keys (0x418278: tables of {key, flags, event}) --------------------------- */

struct key_press { uint32_t key, scan, modifiers, extended; };

/* 0x40cc70: +0x30c's callback over the key tables: an enabled table's entry for the key (or,
 * flags bit 4, its scan code) with these shift, control and alt keys has its event told the
 * game. The table, or 0. */
uint32_t SC fs98_host_rt_key_table(uint32_t *table, const struct key_press *press)
{
    uint32_t count, at;
    const uint32_t *entry;
    if (table[0] == 0)
        return 0;
    count = table[3];
    entry = (const uint32_t *)(uintptr_t)table[2];
    for (at = 0; at < count; ++at, entry += 3) {
        uint32_t flags = entry[1];
        if ((flags & 0x10) ? entry[0] == press->scan : entry[0] == press->key)
            if ((flags & 7) == press->modifiers) {
                fs98_host_rt_hook(((const uint32_t *)(uintptr_t)table[2])[at * 3 + 2], 0);
                return fs98_handle(table);
            }
    }
    return 0;
}

/* 0x40cc20: whether the game's keys took a key, the newest table first */
int32_t SC fs98_host_rt_keys(uint32_t key, uint32_t scan, uint32_t modifiers, uint32_t extended)
{
    struct key_press press;
    press.key = key;
    press.scan = scan;
    press.modifiers = modifiers;
    press.extended = extended;
    return SERVICE(0x30c, int32_t, (void *, void *, void *))(VA(0x418278), &press, (void *)fs98_host_rt_key_table) != 0;
}

/* ---- the menu bar ---------------------------------------------------------------------- */

/* 0x40d660: its redraw held (0x4182c0) */
void fs98_host_rt_hold_menu_bar(void)
{
    G32(0x4182c0) = 0;
}

/* 0x40d670: and let go, the bar drawn */
void fs98_host_rt_draw_menu_bar(void)
{
    G32(0x4182c0) = 1;
    WIN(DrawMenuBar, int32_t, (uint32_t))(MAIN_WINDOW);
}

/* ---- the paused look ------------------------------------------------------------------- */

/* 0x40e670: a window's memory DC given the colours for this display count: the palette set
 * (0x41a254: 0x41a9c0), the tinted one for a window that is tinted (+8 bit 9), else the
 * palette's (0x41adc0). */
void SC fs98_host_rt_colour_table(uint8_t *data)
{
    uint8_t *bitmap = (uint8_t *)PTR(data, 0xa0);
    uint32_t (SC *table)(uint32_t, uint32_t, uint32_t, const void *) =
        (uint32_t (SC *)(uint32_t, uint32_t, uint32_t, const void *))IMPORT(SetDIBColorTable);
    if (D32(bitmap, 0) == G32(0x41ba2c))
        return;
    D32(bitmap, 0) = G32(0x41ba2c);
    if (G32(0x41a254))
        table(D32(bitmap, 0x38), 0, 0x100, VA(0x41a9c0));
    else if (D32(data, 8) & 0x200)
        table(D32(bitmap, 0x38), 0, 0x100, VA(0x41b620));
    else
        table(D32(bitmap, 0x38), 0, 0x100, VA(0x41adc0));
}

/* 0x40e990: 8-bit pixels dithered onto the cube through the table (0x41ba28), the pattern's
 * place from the pixel's (its row and column swapped) */
void SC fs98_host_rt_dither(uint8_t *to, int32_t to_pitch, const uint8_t *from, int32_t from_pitch, int32_t left,
                            int32_t right, int32_t top, int32_t bottom)
{
    int32_t y, x;
    for (y = top; y < bottom; ++y) {
        for (x = left; x < right; ++x) {
            const uint8_t *table = (const uint8_t *)G_PTR(0x41ba28) + (((uint32_t)(y & 7) + (uint32_t)(x & 7) * 8) << 8);
            *to++ = table[*from++];
        }
        /* on by the pitch less the width, even for an empty row (backwards, then) */
        from += (uint32_t)from_pitch + ((uint32_t)left - (uint32_t)right);
        to += (uint32_t)to_pitch + ((uint32_t)left - (uint32_t)right);
    }
}

/* 0x40ea20: the 16-bit GDI path's blit: a memory DC's picture dithered onto the cube and
 * stretched out with the halftone palette, unless it has no bits, the cube is not made, or the
 * window is tinted (+8 bit 9): then stretched as it is. */
void SC fs98_host_rt_gdi_blit(uint32_t dc, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t from,
                              int32_t from_x, int32_t from_y, int32_t from_width, int32_t from_height, uint32_t flags)
{
    uint32_t (SC *current)(uint32_t, uint32_t) = (uint32_t (SC *)(uint32_t, uint32_t))IMPORT(GetCurrentObject);
    uint32_t (SC *select)(uint32_t, uint32_t, int32_t) =
        (uint32_t (SC *)(uint32_t, uint32_t, int32_t))IMPORT(SelectPalette);
    uint32_t bitmap, palette, old;
    int32_t object[6];
    bitmap = current(from, 7);
    current(dc, 5);
    if (G32(0x41b1c0) != G32(0x41ba2c))
        fs98_host_rt_cube(1);
    WIN(GetObjectA, int32_t, (uint32_t, int32_t, void *))(bitmap, 0x18, object);
    if (G32(0x41b608) == 0 || GI32(0x41ba24) < (int32_t)((((uint32_t)from_width + 3) & ~3u) * (uint32_t)from_height)) {
        uint32_t pitch, size;
        if (G32(0x41b608))
            SERVICE(0x214, int32_t, (void *))(VA(0x41b608));
        pitch = ((uint32_t)from_width + 3) & ~3u;
        size = (uint32_t)from_height * pitch;
        G32(0x41ba24) = size;
        SERVICE(0x20c, int32_t, (uint32_t, uint32_t, void *, uint32_t))(4, size, VA(0x41b608), 0);
    }
    palette = G32(0x41ba20);
    if (palette == 0) {
        palette = WIN(CreateHalftonePalette, uint32_t, (uint32_t))(dc);
        G32(0x41ba20) = palette;
    }
    old = select(dc, palette, 1);
    WIN(RealizePalette, uint32_t, (uint32_t))(dc);
    if (G32(0x41b608) && G32(0x41ba28) && object[5] && !(flags & 0x200)) {
        int32_t rows = (int32_t)((uint32_t)object[2] - (uint32_t)from_y), columns = (int32_t)((uint32_t)object[1] - (uint32_t)from_x);
        if (from_height < rows)
            rows = from_height;
        if (from_width < columns)
            columns = from_width;
        fs98_host_rt_dither((uint8_t *)G_PTR(0x41b608), (int32_t)(((uint32_t)from_width + 3) & ~3u),
                            (const uint8_t *)(uintptr_t)((uint32_t)object[3] * (uint32_t)from_y + (uint32_t)object[5] + (uint32_t)from_x),
                            object[3], x, (int32_t)((uint32_t)columns + (uint32_t)x), y, (int32_t)((uint32_t)rows + (uint32_t)y));
        GI32(0x41b1e8) = (int32_t)-(uint32_t)from_height;
        GI32(0x41b1e4) = from_width;
        WIN(StretchDIBits, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                                     const void *, const void *, uint32_t, uint32_t))(
            dc, x, y, width, height, 0, 0, from_width, from_height, G_PTR(0x41b608), VA(0x41b1e0), 0, 0xcc0020);
    } else
        WIN(StretchBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t,
                                  int32_t, uint32_t))(dc, x, y, width, height, from, from_x, from_y, from_width, from_height,
                                                      0xcc0020);
    select(dc, old, 1);
}

/* ---- sound ----------------------------------------------------------------------------- */

/* 0x40f320: sound set up: no voices yet (0x41a3c0), none at all without a wave device
 * (0x41a3cc); the format from [SOUND] Channels, SamplesPerSec and BitsPerSample, kept to 1..2,
 * 11025 or 22050 and 8 or 16 unless AllowIdiotProofing=0; DSOUND.DLL loaded and started
 * (0x418308 3 if it will not load). */
void fs98_host_rt_sound_setup(void)
{
    uint32_t (SC *profile)(const char *, const char *, int32_t, const char *) =
        (uint32_t (SC *)(const char *, const char *, int32_t, const char *))IMPORT(GetPrivateProfileIntA);
    uint16_t channels, bits;
    uint32_t rate;
    G32(0x41a258) = 0x16c;
    G32(0x41a3c0) = 0;
    if (WIN(waveOutGetNumDevs, uint32_t, (void))() == 0) {
        G32(0x41a3cc) = 1;
        G32(0x41a3c8) = 0;
    }
    G16(0x41a264) = 1;
    G16(0x41a274) = 0;
    G16(0x41a266) = (uint16_t)profile(STR(0x418360), STR(0x418368), 2, CFG);
    G32(0x41a268) = profile(STR(0x418360), STR(0x418350), 0x5622, CFG);
    G16(0x41a272) = (uint16_t)profile(STR(0x418360), STR(0x418340), 0x10, CFG);
    if (profile(STR(0x418360), STR(0x41832c), 1, CFG) == 1) {
        channels = G16(0x41a266);
        if (channels < 1) {
            channels = 1;
            G16(0x41a266) = channels;
        } else if (channels > 2) {
            channels = 2;
            G16(0x41a266) = channels;
        }
        rate = G32(0x41a268);
        if (rate < 0x2b11) {
            rate = 0x2b11;
            G32(0x41a268) = rate;
        } else if (rate > 0x2b11) {
            rate = 0x5622;
            G32(0x41a268) = rate;
        }
        bits = G16(0x41a272);
        if (bits < 8) {
            bits = 8;
            G16(0x41a272) = bits;
        } else if (bits > 8) {
            bits = 0x10;
            G16(0x41a272) = bits;
        }
    } else {
        bits = G16(0x41a272);
        rate = G32(0x41a268);
        channels = G16(0x41a266);
    }
    G16(0x41a270) = (uint16_t)((bits >> 3) * channels);
    G32(0x41a26c) = rate * (G32(0x41a270) & 0xffff);
    G32(0x41a3c4) = WIN(LoadLibraryA, uint32_t, (const char *))(STR(0x418320));
    if (G32(0x41a3c4) == 0) {
        void *sound = G_PTR(0x41a3c8);
        G32(0x418308) = 3;
        if (sound) {
            COM_METHOD(sound, 8, int32_t, (void *))(sound);
            G32(0x41a3c8) = 0;
        }
        return;
    }
    fs98_host_rt_start_sound();
}

/* 0x40f4b0: sound ended: the voices let go (+0x2dc, until it stops taking them), DirectSound
 * released and its library freed */
void fs98_host_rt_sound_end(void)
{
    uint32_t voice;
    if (G32(0x41a3cc) == 1)
        return;
    voice = G32(0x41a3c0);
    while (voice) {
        uint32_t taken = voice;
        SERVICE(0x2dc, int32_t, (uint32_t *))(&taken);
        voice = G32(0x41a3c0);
        if (taken == voice)
            break;
    }
    if (G_PTR(0x41a3c8)) {
        COM_METHOD(G_PTR(0x41a3c8), 8, int32_t, (void *))(G_PTR(0x41a3c8));
        G32(0x41a3c8) = 0;
    }
    if (G32(0x41a3c4))
        WIN(FreeLibrary, int32_t, (uint32_t))(G32(0x41a3c4));
}

/* 0x40f510: sound's tick: each voice (0x41a3c0, +0x168 the next) with a callback waiting
 * (+0x5c, +0x58) whose buffer (+0x60) has stopped has it called */
void fs98_host_rt_sound_tick(void)
{
    uint8_t *voice, *next;
    uint32_t status;
    if (G32(0x41a3c8) == 0 || (voice = (uint8_t *)G_PTR(0x41a3c0)) == 0)
        return;
    do {
        next = (uint8_t *)PTR(voice, 0x168);
        if (D32(voice, 0x5c) && D32(voice, 0x58)) {
            void *buffer = PTR(voice, 0x60);
            if (((int32_t (SC *)(void *, uint32_t *))PTR(PTR(buffer, 0), 0x24))(buffer, &status) == 0 && !(status & 5)) {
                D32(voice, 0x5c) = 0;
                ((void (SC *)(void *))PTR(voice, 0x58))(voice);
            }
        }
        voice = next;
    } while (voice);
}

/* ---- the frames' stacking -------------------------------------------------------------- */
