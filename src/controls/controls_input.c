/* CONTROLS.DLL -- the compiled-C half, .text+0x78d0..+0xbd1x.
 *
 * Ordinary MSVC-compiled C (no register model): the configuration, dialog and
 * joystick side of CONTROLS.  Reached through the Linkage entries the host
 * calls (+0x4c..+0x5c) and four entries the hand-written half calls.
 *
 *   +0x4c  controls_keys_load   read [KEYBOARD_MAIN]/[KEYBOARD_SLEW] from
 *                               FLTSIM98.CFG into two host accelerator tables,
 *                               install the flight one, start the joysticks
 *   +0x50  controls_keys_save   write them back, release the joysticks
 *   +0x54  controls_keys_list   fill a caller block with a command's key
 *   +0x58  controls_dialog      run the keyboard-assignments dialog (0x140a)
 *   +0x5c  controls_force       the force-feedback dispatch (DirectInput 5)
 *   +0x92c0 controls_input_init / +0x9340 controls_input_deinit
 *   +0x9270 controls_joystick   enable/report the joysticks
 *   +0x9290 controls_accel_menu switch the slew/flight accelerator table
 *
 * It reads the command table (controls_commands) as data and reaches the
 * hand-written half only through WM_COMMAND, which the host turns into a
 * CHAIN list-0xb command.  docs/modules/controls.md has the details.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "controls.h"

/* host services, by their confirmed ret N (docs/modules/host.md, controls.md) */
#define HC(off, fn) CALL(HOST, off, fn)
#define U(off, fn)  CALL(UTIL, off, fn)
#define NUMF(off, fn) CALL(NUMERIC, off, fn)
#define ACN(off, fn) CALL(ACONTAIN, off, fn)

#define V8(o)  M8(D + (o))
#define V16(o) M16(D + (o))
#define V32(o) M32(D + (o))
#define RD32(o) M32(RDATA + (o))

#define NCMD   controls_count
#define CMD(i) controls_commands[i]

/* string helpers through the host (so the layout and behaviour match retail) */
#define h_strcpy(d, s) HC(0x2a8, f2)((uint32_t)(uintptr_t)(d), (uint32_t)(uintptr_t)(s))
#define h_strcat(d, s) HC(0x2a4, f2)((uint32_t)(uintptr_t)(d), (uint32_t)(uintptr_t)(s))
#define h_strncat(d, s) HC(0x2b0, f2)((uint32_t)(uintptr_t)(d), (uint32_t)(uintptr_t)(s))
#define h_strlen(s)    HC(0x2b8, f1)((uint32_t)(uintptr_t)(s))
#define h_stricmp(a, b) HC(0x2c0, f2)((uint32_t)(uintptr_t)(a), (uint32_t)(uintptr_t)(b))
#define P(x) ((uint32_t)(uintptr_t)(x))

static uint32_t ff_begin(void);          /* .text+0xa8d0 */
static void ff_end(void);                /* .text+0xa9b0 */

/* ======== reading the key bindings: .text+0x78d0 ================================= */

/* .text+0x7970 -- read one [section] of FLTSIM98.CFG into the accelerator
 * table *pAccel, one entry per command of the matching kind whose key the
 * host's +0xb0 parser returns nonzero.  Returns the host's section-present
 * word: zero when the section is absent (so the caller loads the defaults),
 * nonzero when it built the table from the file. */
static uint32_t section_read(const char *section, uint32_t kind, uint32_t *pAccel)
{
    uint32_t got, i;

    HC(0x100, f2)(P(section), P(&got));     /* get the [section] text */
    if (got == 0)
        return got;
    if (HC(0x1d8, f1)(P(pAccel)) != 0)      /* begin a new table */
        return got;
    for (i = 0; i < NCMD; i++) {
        uint32_t parsed[2], ent[3];         /* parsed = key, flags; ent = key, flags, cmd */
        if (CMD(i).name == 0)
            continue;
        if ((CMD(i).kind & kind) == 0)
            continue;
        parsed[0] = 0;
        parsed[1] = 0;
        HC(0xb0, f4)(P(section), P(CMD(i).name), P(parsed), 2);
        ent[0] = parsed[0];
        ent[1] = parsed[1];
        ent[2] = CMD(i).command;
        if (ent[0] == 0)
            continue;
        HC(0x1f4, f3)(*pAccel, P(ent), 1);
    }
    return got;
}

/* .text+0x7a60 -- the alternate path: read each command's key from a
 * [<layout> <hkl>] section named after the current keyboard layout, in the
 * joystick-prefs profile (D+0x408), through the host's dialog-item reader
 * +0xd8.  Appends to the same accelerator table. */
static uint32_t section_read_layout(const char *section, uint32_t kind,
                                    uint32_t *pAccel, const char *path)
{
    char head[0x24], layout[0x104];
    uint32_t got, i, hkl;

    hkl = (uint32_t)(uintptr_t)GetKeyboardLayout(0);
    wsprintfA(layout, (LPCSTR)(uintptr_t)(D + 0x1c8), section, hkl);  /* "%s %08X" */
    HC(0x130, f1)(P(head));                        /* the layout's own name */
    HC(0x184, f4)(P(layout), 0, P(head), P(layout));
    HC(0x104, f4)(P(layout), P(head), 0x24, P(&got));
    if (got == 0)
        return 0;
    if (HC(0x1d8, f1)(P(pAccel)) != 0)
        return -1;
    for (i = 0; i < NCMD; i++) {
        uint32_t parsed[2], ent[3];
        if (CMD(i).name == 0)
            continue;
        if ((CMD(i).kind & kind) == 0)
            continue;
        parsed[0] = 0;
        parsed[1] = 0;
        HC(0xd8, f6)(P(layout), P(CMD(i).name), 2, P(parsed), P(path), 0x24);
        ent[0] = parsed[0];
        ent[1] = parsed[1];
        ent[2] = CMD(i).command;
        if (ent[0] == 0)
            continue;
        HC(0x1f4, f3)(*pAccel, P(ent), 1);
    }
    (void)path;
    return 0;
}

/* .text+0x78d0 -- Linkage +0x4c */
uint32_t FS98_STDCALL controls_keys_load(void)
{
    const char *main_s = (const char *)(uintptr_t)(D + 0x1a8);  /* "KEYBOARD_MAIN" */
    const char *slew_s = (const char *)(uintptr_t)(D + 0x1b8);  /* "KEYBOARD_SLEW" */
    const char *prefs = (const char *)(uintptr_t)(D + 0x408);   /* the prefs path */

    if (section_read(main_s, 4, (uint32_t *)(uintptr_t)(D + 0x186)) == 0 &&
        section_read_layout(main_s, 4, (uint32_t *)(uintptr_t)(D + 0x186), prefs) == 0)
        HC(0x1e4, f2)(D + 0x186, 0x67);
    if (section_read(slew_s, 8, (uint32_t *)(uintptr_t)(D + 0x18a)) == 0 &&
        section_read_layout(slew_s, 8, (uint32_t *)(uintptr_t)(D + 0x18a), prefs) == 0)
        HC(0x1e4, f2)(D + 0x18a, 0x68);
    HC(0x1e8, f1)(V32(0x186));
    return ff_begin();  /* .text+0xa8d0, tail call */
}

/* ======== writing the key bindings: .text+0x7bd0 ================================= */

/* .text+0x7c20 -- write [section] back from the accelerator table *pAccel:
 * clear it, read the table's entries into a temporary {key, flags, cmd}
 * array, and for each command of this kind write its entry's key and flags
 * under the command's name. */
static void section_write(const char *section, uint32_t kind, uint32_t *pAccel)
{
    uint32_t count, *buf, i;

    HC(0xf8, f1)(P(section));                        /* clear the section */
    if (HC(0x1fc, f6)(*pAccel, 0, 0, 2, 0, P(&count)) != 0)
        return;
    HC(0x20c, f4)(1, count * 0xc, P(&buf), 1);       /* count {key,flags,cmd} */
    HC(0x1fc, f6)(*pAccel, 0, 0, 2, P(buf), P(&count));
    for (i = 0; i < NCMD; i++) {
        uint32_t k;
        if (CMD(i).name == 0)
            continue;
        if ((CMD(i).kind & kind) == 0)
            continue;
        for (k = 0; k < count; k++) {
            if (buf[k * 3 + 2] != CMD(i).command)
                continue;
            {
                uint32_t ent2[2];
                ent2[0] = buf[k * 3 + 0];
                ent2[1] = buf[k * 3 + 1];
                HC(0xb4, f4)(P(section), P(CMD(i).name), P(ent2), 2);
            }
        }
    }
    HC(0x214, f1)(P(&buf));
}

/* .text+0x7bd0 -- Linkage +0x50 */
uint32_t FS98_STDCALL controls_keys_save(void)
{
    ff_end();
    section_write((const char *)(uintptr_t)(D + 0x1a8), 4, (uint32_t *)(uintptr_t)(D + 0x186));
    section_write((const char *)(uintptr_t)(D + 0x1b8), 8, (uint32_t *)(uintptr_t)(D + 0x18a));
    HC(0x1dc, f1)(D + 0x186);
    HC(0x1dc, f1)(D + 0x18a);
    return 0;
}

/* .text+0x7d60 -- Linkage +0x54: fill `count` {cmd, keyspec} pairs (at
 * `pairs`) with the current flight-accelerator key for each cmd, packed as
 * key | flags<<8. */
uint32_t FS98_STDCALL controls_keys_list(uint32_t count, uint32_t pairs)
{
    uint32_t n, *buf, i;

    if (HC(0x1fc, f6)(V32(0x186), 0, 0, 2, 0, P(&n)) != 0)
        return 0;
    HC(0x20c, f4)(1, n * 0xc, P(&buf), 1);
    HC(0x1fc, f6)(V32(0x186), 0, 0, 2, P(buf), P(&n));
    for (i = 0; i < count; i++) {
        uint32_t *pair = (uint32_t *)(uintptr_t)(pairs + i * 8);
        uint32_t k;
        for (k = 0; k < n; k++)
            if (buf[k * 3 + 2] == pair[0]) {
                pair[1] = buf[k * 3 + 0] + (buf[k * 3 + 1] << 8);
                break;
            }
    }
    HC(0x214, f1)(P(&buf));
    return 0;
}

/* ======== the joysticks and the frame poll: .text+0x8e60.. ======================= */

static uint32_t FS98_STDCALL joy_poll(uint32_t a, uint32_t b);   /* .text+0x90c0, list 1 */
static uint32_t FS98_STDCALL ff_frame(uint32_t a, uint32_t b);   /* .text+0x93b0, list 1/6 */
static void joy_config_save(void);                               /* .text+0x94b0 */

/* .text+0x8e60 -- reset every joystick's 0x10 cached axis values to
 * 0xffff8000, so the next poll re-sends them.  16 slots of 0x38c bytes from
 * D+0x4b54 up to D+0x8414. */
static void axis_reset(void)
{
    uint32_t p;

    for (p = D + 0x4b54; p < D + 0x8414; p += 0x38c) {
        uint32_t i;
        for (i = 0; i < 0x10; i++)
            M32(p + i * 4) = 0xffff8000u;
    }
    ecx = 0;    /* the rep stosd leaves ecx 0; +0x44's caller (DEMO) reads it */
}

/* .text+0x9270 -- JOYSTICK_CALIBRATE's enable/report: with `set`, store `on`
 * as the joysticks' state (D+0xbf8c) and reset the axis caches; return the
 * state.  (The axis_reset leaves ecx 0, which the assembly does not read.) */
uint32_t FS98_STDCALL controls_joystick(uint32_t set, uint32_t on)
{
    if (set) {
        V32(0xbf8c) = on;
        axis_reset();
    }
    return V32(0xbf8c);
}

/* .text+0x9290 -- switch the key table D+0x350 points at: 0 the flight table
 * (D+0xd48), 1 the slew table (D+0x82c8); then reset the axis caches. */
void FS98_STDCALL controls_accel_menu(uint32_t which)
{
    if (which == 0)
        V32(0x350) = D + 0xd48;
    else if (which == 1)
        V32(0x350) = D + 0x82c8;
    axis_reset();
}

/* .text+0x92c0 -- Linkage via the assembly's init: clear the joystick state
 * block, read [CONTROLS] "devices" (the prefs path) into D+0x408, register
 * the frame poll and the force-feedback update on CHAIN list 1, and enable
 * the joysticks. */
void controls_input_init(void)
{
    if (V32(0xbf90) != 0)
        return;
    V32(0xbf90) = 1;
    HC(0x218, f3)(D + 0x4a08, 0, 0x38c0);
    HC(0xc8, f4)(D + 0x192, D + 0x440, D + 0x408, 0x32);
    CALL(CHAIN, 0x18, f2)(1, P(joy_poll));
    CALL(CHAIN, 0x18, f2)(1, P(ff_frame));
    controls_joystick(1, 0);
}

/* .text+0x9340 */
void controls_input_deinit(void)
{
    if (V32(0xbf90) == 0)
        return;
    V32(0xbf90) = 0;
    HC(0xac, f3)(D + 0x400, D + 0x448, controls_joystick(0, 0));
    joy_config_save();
    CALL(CHAIN, 0x1c, f2)(1, P(joy_poll));
    CALL(CHAIN, 0x1c, f2)(1, P(ff_frame));
    CALL(CHAIN, 0x1c, f2)(6, P(ff_frame));
}

/* .text+0x8e90 -- the axis response curve for joystick `joy` axis `axis`:
 * take the live reading, subtract a dead zone (config +0x8c, <<7), scale
 * (config +0x4c, >>7), clamp and negate.  The very-negative case returns
 * +0x4000 without the final negate -- a retail quirk, reproduced. */
static uint32_t FS98_STDCALL axis_curve(uint32_t joy, uint32_t axis)
{
    uint32_t cfg = V32(0x350) + (axis + 0xf3 * joy) * 4;
    int32_t live = (int32_t)M32(D + 0x4a14 + (axis + 0xe3 * joy) * 4);
    int32_t dz = (int32_t)(M32(cfg + 0x8c) << 7);
    int32_t v, r;

    if (live > dz)
        v = live - dz;
    else if (live < -dz)
        v = live + dz;
    else
        v = 0;
    r = v ? (((int32_t)M32(cfg + 0x4c) * v) >> 7) : 0;
    if (r < -0x3fff)
        return 0x4000;
    if (r > 0x3fff)
        r = 0x3fff;
    return (uint32_t)(-r);
}

/* .text+0x8f20 -- turn joystick `joy`'s freshly-read state into commands: for
 * each of its 0x10 present axes whose value changed, post the axis command
 * with the curved value; for each of its 0x40 buttons (one word each, 0 or
 * 1), post the press command on a press and again every repeat interval
 * (config +0x1cc, in polls) while it is held, and the release command when
 * it comes up.  All go out as
 * WM_COMMAND (host +0x3c), which returns as a CHAIN list-0xb command. */
static void FS98_STDCALL joy_map(uint32_t joy)
{
    uint32_t base = 0x38c * joy;        /* the live-state block's offset */
    uint32_t cfg = V32(0x350);          /* the per-joystick config table */
    uint32_t a, b;

    for (a = 0; a < 0x10; a++) {
        uint32_t idx, cmd, cur, last;
        if ((M32(D + 0x4a10 + base) & (1u << a)) == 0)
            continue;
        idx = a + 0xf3 * joy;
        cmd = M32(cfg + idx * 4 + 0xc);
        if (cmd == 0)
            continue;
        cur = axis_curve(joy, a);
        last = base + (a + 0xe3 * joy) * 4;     /* D+0x4b54 cache slot */
        if (cur != M32(D + 0x4b54 + (a + 0xe3 * joy) * 4)) {
            M32(D + 0x4b54 + (a + 0xe3 * joy) * 4) = cur;
            HC(0x3c, f2)(M32(V32(0x350) + idx * 4 + 0xc), cur);
        }
        (void)last;
    }
    for (b = 0; b < 0x40; b++) {
        uint32_t idx = b + 0xf3 * joy;
        uint32_t slot = base + D + 0x4c94 + b * 4;   /* the repeat counter */
        uint32_t n = M32(slot);
        if (n == 0)
            ;
        else if (n == 1) {
            M32(slot) = M32(cfg + idx * 4 + 0x1cc);  /* the repeat interval */
            M32(slot - 0x100) = 0;                   /* so it presses again */
        } else {
            M32(slot) = n - 1;
        }
        /* press: down this frame (D+0x4a54), not held (D+0x4b94 copy 0) */
        if (M32(slot - 0x240) == 1 && M32(slot - 0x100) == 0) {
            uint32_t cmd = M32(cfg + idx * 4 + 0xcc);
            if (cmd != 0) {
                HC(0x3c, f2)(cmd, 0);
                M32(slot) = M32(V32(0x350) + idx * 4 + 0x1cc);
            }
        }
        /* up this frame: a release if it was held, and either way the
         * repeat stops -- .text+0x9078 is reached only from this branch, so
         * a held button keeps its counter */
        if (M32(slot - 0x240) == 0) {
            if (M32(slot - 0x100) == 1) {
                uint32_t cmd = M32(cfg + idx * 4 + 0x2cc);
                if (cmd != 0)
                    HC(0x3c, f2)(cmd, 0);
                cfg = V32(0x350);
            }
            M32(slot) = 0;
        }
    }
    /* carry this frame's 0x40 button words to next frame's "held" */
    HC(0x21c, f3)(base + D + 0x4b94, base + D + 0x4a54, 0x100);
}

/* .text+0x90c0 -- CHAIN list 1, every frame: with the joysticks enabled and
 * not in a lesson that locks them (GLOBAL +0x84c), read each attached
 * joystick's state into its block and map it to commands. */
static uint32_t FS98_STDCALL joy_poll(uint32_t a, uint32_t b)
{
    uint32_t i, n;

    (void)a;
    (void)b;
    if (V32(0xbf8c) == 0)
        return V32(0xbf88);
    if (M32(G + 0x84c) != 0)
        return V32(0xbf88);
    n = V32(0xbf88);
    for (i = 0; (int32_t)i < (int32_t)n; i++) {
        uint32_t blk = D + 0x4a08 + i * 0x38c;
        M32(blk + 8) = M32(V32(0x350) + i * 0x3cc + 8);
        if (HC(0x1c4, f2)(i, blk) == 0)
            joy_map(i);
        n = V32(0xbf88);
    }
    return V32(0xbf88);
}

/* ======== NOT yet transliterated: FF device setup, dialog, force dispatch ========
 *
 * The force-feedback device setup (.text+0xa8d0/+0xa9b0), the per-frame FF
 * update (.text+0x93b0), the joystick-config persistence (.text+0x94b0 and
 * the profile readers it calls), the keyboard-assignments dialog
 * (.text+0x7e40) and the force dispatch (Linkage +0x5c, .text+0xa400) are the
 * remaining compiled-C work.  They are large, profile- and COM-heavy, and
 * must be transliterated instruction-faithfully against the listing and
 * verified in controlsdiff -- not paraphrased.  Until then these are honest
 * placeholders.  The "no force-feedback device" path -- what wine gives and
 * the first harness state -- is the one that runs, and it leaves the joystick
 * count (D+0xbf88) zero, so joy_poll does nothing: a sim with no configured
 * joystick, which is safe.  The keyboard half is unaffected. */

/* .text+0xa8d0 -- force-feedback / joystick-device begin (tail-called by
 * controls_keys_load); returns the joystick count D+0xbf88. */
static uint32_t ff_begin(void)
{
    return V32(0xbf88);
}

/* .text+0xa9b0 -- release the force-feedback device. */
static void ff_end(void)
{
}

/* .text+0x93b0 -- CHAIN list 1/6: the per-frame force-feedback update. */
static uint32_t FS98_STDCALL ff_frame(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    return V32(0xbf88);
}

/* .text+0x94b0 -- write each joystick's config back to the prefs. */
static void joy_config_save(void)
{
}

/* .text+0x7e40 -- Linkage +0x58: the keyboard-assignments dialog (0x140a). */
uint32_t FS98_STDCALL controls_dialog(void)
{
    return 0;
}

/* .text+0xa400 -- Linkage +0x5c: the force-feedback dispatch. */
uint32_t FS98_STDCALL controls_force(uint32_t op, uint32_t param)
{
    (void)op;
    (void)param;
    return 0;
}

/* install anything controls.c's DllMain needs from this half (nothing yet) */
void controls_input_install(void)
{
}
