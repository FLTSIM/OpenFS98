/* CONTROLS.DLL -- the compiled-C half, .text+0x78d0..+0xbd16.
 *
 * Ordinary MSVC-compiled C (no register model): the configuration, dialog,
 * joystick and force-feedback side of CONTROLS.  Reached through the Linkage
 * entries the host calls (+0x4c..+0x5c), four entries the hand-written half
 * calls, and the CHAIN lists it registers on (1, 6, 0x14).
 *
 *   +0x4c  controls_keys_load   read [KEYBOARD_MAIN]/[KEYBOARD_SLEW] from
 *                               FLTSIM98.CFG into two host accelerator tables,
 *                               install the flight one, bring force feedback up
 *   +0x50  controls_keys_save   take force feedback down, write them back
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
#define RD(o)   MD(RDATA + (o))         /* an .rdata double */

#define NCMD   controls_count
#define CMD(i) controls_commands[i]

/* string helpers through the host (so the layout and behaviour match retail) */
#define h_strcpy(d, s) HC(0x2a8, f2)((uint32_t)(uintptr_t)(d), (uint32_t)(uintptr_t)(s))
#define h_strcat(d, s) HC(0x2b0, f2)((uint32_t)(uintptr_t)(d), (uint32_t)(uintptr_t)(s))
#define h_strlen(s)    HC(0x2b8, f1)((uint32_t)(uintptr_t)(s))
#define h_stricmp(a, b) HC(0x2c0, f2)((uint32_t)(uintptr_t)(a), (uint32_t)(uintptr_t)(b))
#define P(x) ((uint32_t)(uintptr_t)(x))

static uint32_t ff_begin(void);          /* .text+0xa8d0 */
static uint32_t ff_end(void);            /* .text+0xa9b0 */

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

/* .text+0x7a60 -- the alternate source: [<section> <layout>] -- the
 * section's name and the keyboard layout's handle, "%s %08X" -- in <the sim's
 * directory>\<path>.CFG, each command's key read through +0xd8 into the
 * accelerator table *pAccel.  Returns the section-present word, as
 * section_read does: 0 when the section is absent, so the caller loads the
 * defaults. */
static uint32_t section_read_layout(uint32_t path, const char *section, uint32_t kind,
                                    uint32_t *pAccel)
{
    char dir[0x104], layout[0x104];
    uint32_t got, i, hkl;

    hkl = (uint32_t)(uintptr_t)GetKeyboardLayout(0);
    wsprintfA(layout, (LPCSTR)(uintptr_t)(D + 0x1c8), section, hkl);  /* "%s %08X" */
    HC(0x130, f1)(P(dir));
    HC(0x184, f4)(P(dir), 0, P(dir), path);
    HC(0x104, f4)(P(layout), P(dir), 0x24, P(&got));
    if (got == 0)
        return got;
    if (HC(0x1d8, f1)(P(pAccel)) != 0)
        return got;
    for (i = 0; i < NCMD; i++) {
        uint32_t parsed[2], ent[3];
        if (CMD(i).name == 0)
            continue;
        if ((CMD(i).kind & kind) == 0)
            continue;
        parsed[0] = 0;
        parsed[1] = 0;
        HC(0xd8, f6)(P(layout), P(CMD(i).name), P(parsed), 2, P(dir), 0x24);
        ent[0] = parsed[0];
        ent[1] = parsed[1];
        ent[2] = CMD(i).command;
        if (ent[0] == 0)
            continue;
        HC(0x1f4, f3)(*pAccel, P(ent), 1);
    }
    return got;
}

/* .text+0x78d0 -- Linkage +0x4c */
uint32_t FS98_STDCALL controls_keys_load(void)
{
    const char *main_s = (const char *)(uintptr_t)(D + 0x1a8);  /* "KEYBOARD_MAIN" */
    const char *slew_s = (const char *)(uintptr_t)(D + 0x1b8);  /* "KEYBOARD_SLEW" */
    uint32_t devices = D + 0x408;                               /* the devices file */

    if (section_read(main_s, 4, (uint32_t *)(uintptr_t)(D + 0x186)) == 0 &&
        section_read_layout(devices, main_s, 4, (uint32_t *)(uintptr_t)(D + 0x186)) == 0)
        HC(0x1e4, f2)(D + 0x186, 0x67);
    if (section_read(slew_s, 8, (uint32_t *)(uintptr_t)(D + 0x18a)) == 0 &&
        section_read_layout(devices, slew_s, 8, (uint32_t *)(uintptr_t)(D + 0x18a)) == 0)
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
    return HC(0x1dc, f1)(D + 0x18a);
}

/* .text+0x7d60 -- Linkage +0x54: fill `count` {cmd, keyspec} pairs (at
 * `pairs`) with the flight-accelerator key for each cmd, packed as
 * key + (flags << 8).  Every entry is looked at, so of several keys for one
 * command the last wins; a command with none keeps its keyspec.  Returns
 * +0x1fc's failure, else what +0x214 does. */
uint32_t FS98_STDCALL controls_keys_list(uint32_t count, uint32_t pairs)
{
    uint32_t n, *buf, i, r;

    r = HC(0x1fc, f6)(V32(0x186), 0, 0, 2, 0, P(&n));
    if (r != 0)
        return r;
    HC(0x20c, f4)(1, n * 0xc, P(&buf), 1);
    HC(0x1fc, f6)(V32(0x186), 0, 0, 2, P(buf), P(&n));
    for (i = 0; i < count; i++) {
        uint32_t *pair = (uint32_t *)(uintptr_t)(pairs + i * 8);
        uint32_t k;
        for (k = 0; k < n; k++)
            if (buf[k * 3 + 2] == pair[0])
                pair[1] = buf[k * 3 + 0] + (buf[k * 3 + 1] << 8);
    }
    return HC(0x214, f1)(P(&buf));
}

/* ======== the joysticks and the frame poll: .text+0x8e60.. ======================= */

static uint32_t FS98_STDCALL joy_poll(uint32_t a, uint32_t b);   /* .text+0x90c0, list 1 */
static uint32_t FS98_STDCALL joy_watch(uint32_t a, uint32_t b);  /* .text+0x93b0, list 1/6 */
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
 * block, read [CONTROLS] devices (the devices file's name) into D+0x408,
 * register the frame poll and the joystick watcher on CHAIN list 1, and
 * disable the joysticks until the watcher finds one.  Returns the joystick
 * count, which the assembly's init returns in eax. */
uint32_t controls_input_init(void)
{
    if (V32(0xbf90) != 0)
        return V32(0xbf88);
    V32(0xbf90) = 1;
    HC(0x218, f3)(D + 0x4a08, 0, 0x38c0);
    HC(0xc8, f4)(D + 0x192, D + 0x440, D + 0x408, 0x32);
    CALL(CHAIN, 0x18, f2)(1, P(joy_poll));
    CALL(CHAIN, 0x18, f2)(1, P(joy_watch));
    controls_joystick(1, 0);
    return V32(0xbf88);
}

/* .text+0x9340 -- save [PREFS] JOYSTICKS and the joystick configs, and take
 * the poll and the watcher (from list 1 or 6) off CHAIN.  Returns what the
 * last CHAIN call did, or 0 if not initialised. */
uint32_t controls_input_deinit(void)
{
    if (V32(0xbf90) == 0)
        return V32(0xbf90);
    V32(0xbf90) = 0;
    HC(0xac, f3)(D + 0x400, D + 0x448, controls_joystick(0, 0));
    joy_config_save();
    CALL(CHAIN, 0x1c, f2)(1, P(joy_poll));
    CALL(CHAIN, 0x1c, f2)(1, P(joy_watch));
    return CALL(CHAIN, 0x1c, f2)(6, P(joy_watch));
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

/* ======== the joystick configs: .text+0x91d0, +0x93b0..+0xa3ff ==================
 *
 * Each joystick has a flight and a slew config, 0x3cc bytes each, at
 * D+0xd48 / D+0x82c8 + joystick*0x3cc: +0x0 LOCKED, +0x4 TYPE, +0x8
 * AXIS_FLAGS (the present axes), 0x10 each of +0xc axis events, +0x4c scales
 * and +0x8c nulls, and 0x40 each of +0xcc button-down events, +0x1cc repeat
 * intervals and +0x2cc button-up events.  They live in FLTSIM98.CFG as
 * [JOYSTICK_nn] and [JOYSTICK_SLEW_nn]; a stick without one gets its model's
 * defaults, from the devices file or from a string resource. */

static uint32_t joy_read(uint32_t path, uint32_t section, uint32_t table);   /* .text+0x9910 */

/* .text+0x9da0 -- a heap copy of `prefix` with `joy` after it in NUMERIC's
 * two-digit format 0xb ("JOYSTICK_" 1 -> "JOYSTICK_01"); the caller frees
 * it.  If the allocation fails, whatever +0x20c left. */
static uint32_t section_name(uint32_t prefix, uint32_t joy)
{
    uint32_t s = 0;

    if ((int32_t)HC(0x20c, f4)(1, h_strlen(prefix) + 3, P(&s), 1) != -1) {
        h_strcpy(s, prefix);
        NUMF(0x18, f3)(P(&joy), 0xb, s + h_strlen(s));
    }
    return s;
}

/* .text+0x9e90 -- read [section] `key` as a command: its text matched by
 * name against the command table (case-blind), or failing that the key as a
 * number.  From FLTSIM98.CFG when `file` is null, else from `file`.CFG. */
static uint32_t read_cmd(uint32_t file, uint32_t section, uint32_t key, uint32_t out)
{
    char buf[0x34];
    uint32_t i;

    buf[0] = 0;
    if (file == 0)
        HC(0xc8, f4)(section, key, P(buf), 0x32);
    else
        HC(0xf0, f6)(section, key, P(buf), 0x32, file, 0x24);
    for (i = 0; i < NCMD; i++) {
        if (CMD(i).name == 0)
            continue;
        if (h_stricmp(CMD(i).name, buf) == 0) {
            M32(out) = CMD(i).command;
            return out;
        }
    }
    if (file == 0)
        return HC(0xa8, f3)(section, key, out);
    return HC(0xd0, f5)(section, key, out, file, 0x24);
}

/* .text+0x9910 -- read one config into `table`, cleared first, from
 * [section] of FLTSIM98.CFG when `path` is null, else of <the sim's
 * directory>\<path>.CFG, which must exist and hold the section.  Returns 0
 * when it does not, else 1.  The key counter is a byte; NUMERIC's format 0xb
 * reads only that byte of it. */
static uint32_t joy_read(uint32_t path, uint32_t section, uint32_t table)
{
    char k14[0x14], k28[0x18], k40[0x18], file[0x104];
    uint32_t got = 0;
    uint8_t k;

    HC(0x218, f3)(table, 0, 0x3cc);
    if (path == 0) {
        HC(0xa8, f3)(section, D + 0x468, table + 0);        /* LOCKED */
        HC(0xa8, f3)(section, D + 0x460, table + 4);        /* TYPE */
        HC(0xa8, f3)(section, D + 0x454, table + 8);        /* AXIS_FLAGS */
    } else {
        HC(0x130, f1)(P(file));
        HC(0x184, f4)(P(file), 0, P(file), path);
        if (HC(0x158, f2)(P(file), 0x24) == 0)
            HC(0x104, f4)(section, P(file), 0x24, P(&got));
        if (got == 0)
            return 0;
        HC(0xd0, f5)(section, D + 0x468, table + 0, P(file), 0x24);
        HC(0xd0, f5)(section, D + 0x460, table + 4, P(file), 0x24);
        HC(0xd0, f5)(section, D + 0x454, table + 8, P(file), 0x24);
    }
    for (k = 0; k < 0x10; k++) {
        h_strcpy(k28, D + 0x378);                           /* AXIS_EVENT_ */
        NUMF(0x18, f3)(P(&k), 0xb, P(k28) + h_strlen(k28));
        h_strcpy(k14, D + 0x388);                           /* AXIS_SCALE_ */
        NUMF(0x18, f3)(P(&k), 0xb, P(k14) + h_strlen(k14));
        h_strcpy(k40, D + 0x398);                           /* AXIS_NULL_ */
        NUMF(0x18, f3)(P(&k), 0xb, P(k40) + h_strlen(k40));
        if (path == 0) {
            read_cmd(0, section, P(k28), table + 0xc + k * 4);
            HC(0xa8, f3)(section, P(k14), table + 0x4c + k * 4);
            HC(0xa8, f3)(section, P(k40), table + 0x8c + k * 4);
        } else {
            read_cmd(P(file), section, P(k28), table + 0xc + k * 4);
            HC(0xd0, f5)(section, P(k14), table + 0x4c + k * 4, P(file), 0x24);
            HC(0xd0, f5)(section, P(k40), table + 0x8c + k * 4, P(file), 0x24);
        }
    }
    for (k = 0; k < 0x40; k++) {
        h_strcpy(k28, D + 0x3a8);                           /* BUTTON_DOWN_EVENT_ */
        NUMF(0x18, f3)(P(&k), 0xb, P(k28) + h_strlen(k28));
        h_strcpy(k40, D + 0x3c0);                           /* BUTTON_DOWN_REPEAT_ */
        NUMF(0x18, f3)(P(&k), 0xb, P(k40) + h_strlen(k40));
        h_strcpy(k14, D + 0x3d8);                           /* BUTTON_UP_EVENT_ */
        NUMF(0x18, f3)(P(&k), 0xb, P(k14) + h_strlen(k14));
        if (path == 0) {
            read_cmd(0, section, P(k28), table + 0xcc + k * 4);
            HC(0xa8, f3)(section, P(k40), table + 0x1cc + k * 4);
            read_cmd(0, section, P(k14), table + 0x2cc + k * 4);
        } else {
            read_cmd(P(file), section, P(k28), table + 0xcc + k * 4);
            HC(0xd0, f5)(section, P(k40), table + 0x1cc + k * 4, P(file), 0x24);
            read_cmd(P(file), section, P(k14), table + 0x2cc + k * 4);
        }
    }
    return 1;
}

/* .text+0x9890 -- joystick `joy`'s flight and slew configs from
 * [JOYSTICK_nn] and [JOYSTICK_SLEW_nn] of FLTSIM98.CFG. */
static void joy_load(uint32_t joy)
{
    uint32_t s;

    s = section_name(D + 0x358, joy);                       /* JOYSTICK_ */
    joy_read(0, s, D + 0xd48 + joy * 0x3cc);
    HC(0x214, f1)(P(&s));
    s = section_name(D + 0x368, joy);                       /* JOYSTICK_SLEW_ */
    joy_read(0, s, D + 0x82c8 + joy * 0x3cc);
    HC(0x214, f1)(P(&s));
}

/* .text+0xa200 -- one comma-separated integer at *pp into `out`, *pp left
 * past its comma.  Blanks are skipped, a '-' negates when `negative`
 * allows, and any other non-digit makes it 0.  Blanks that run up to the
 * terminator step the cursor past it, as retail does; the buffer is zeroed,
 * so what follows reads as empty. */
static void parse_int(uint32_t *pp, uint32_t out, uint32_t negative)
{
    uint32_t v = 0, neg = 0, bad = 0;

    if (M8(*pp) == 0)
        M32(out) = bad;
    if (M8(*pp) != 0) {
        while (M8(*pp) != ',') {
            while (M8(*pp) == ' ' || M8(*pp) == '\t')
                (*pp)++;
            if (M8(*pp) == ',')
                break;
            if (M8(*pp) == '-') {
                (*pp)++;
                neg = 1;
            }
            if ((int8_t)M8(*pp) >= '0' && (int8_t)M8(*pp) <= '9')
                v = v * 10 + (M8(*pp) & 0xf);
            else
                bad = 1;
            (*pp)++;
            if (M8(*pp) == 0)
                break;
        }
    }
    if (M8(*pp) == ',')
        (*pp)++;
    if (neg && negative)
        v = -v;
    if (bad)
        v = 0;
    M32(out) = v;
}

/* .text+0xa2b0 -- string resource `id`, comma-separated, into `blk`:
 * AXIS_FLAGS, the 16 axis events, scales and nulls (signed), then the 64
 * button-down events, repeats and button-up events.  Returns 0 even when the
 * string is missing -- which leaves `blk` as it was -- and nonzero only if
 * the buffer cannot be had. */
static uint32_t joy_parse(uint32_t id, uint32_t blk)
{
    uint32_t buf, p, r, i;

    r = HC(0x20c, f4)(1, 0x800, P(&buf), 1);
    if (r != 0)
        return r;
    if (HC(0x2a0, f3)(id, buf, 0x800) != 0) {
        HC(0x218, f3)(blk, 0, 0x3cc);
        p = buf;
        parse_int(&p, blk + 8, 0);
        for (i = 0; i < 0x10; i++)
            parse_int(&p, blk + 0xc + i * 4, 1);
        for (i = 0; i < 0x10; i++)
            parse_int(&p, blk + 0x4c + i * 4, 1);
        for (i = 0; i < 0x10; i++)
            parse_int(&p, blk + 0x8c + i * 4, 1);
        for (i = 0; i < 0x40; i++)
            parse_int(&p, blk + 0xcc + i * 4, 0);
        for (i = 0; i < 0x40; i++)
            parse_int(&p, blk + 0x1cc + i * 4, 0);
        for (i = 0; i < 0x40; i++)
            parse_int(&p, blk + 0x2cc + i * 4, 0);
    }
    HC(0x214, f1)(P(&buf));
    return 0;
}

/* the host's description of a stick (+0x1c8) */
struct joy_desc {
    uint32_t id;            /* its model's string id, 0xb000 + 4n */
    uint32_t caps[2];
    uint32_t rudder;
    char name[0x40];        /* the OEM name */
};

/* .text+0xa040 -- the default config of kind 0 (flight) or 1 (slew) for
 * joystick `joy` into `blk`: from the devices file's [<name> RUDDER] for a
 * stick with a rudder, then [<name>] -- each with " SLEW" for slew -- else
 * from the model's string, its id +1 (flight) or +2 (slew); then TYPE is the
 * model's id.  Returns the host's -1 for a stick it does not know. */
static uint32_t joy_default(uint32_t kind, uint32_t joy, uint32_t blk)
{
    struct joy_desc desc;
    char buf[0x100];
    uint32_t r;

    r = HC(0x1c8, f2)(joy, P(&desc));
    if (r != 0)
        return r;
    if (kind == 0) {
        h_strcpy(buf, desc.name);
        if (desc.rudder != 0)
            h_strcat(buf, D + 0x3f0);                       /* " RUDDER" */
        if (joy_read(D + 0x408, P(buf), blk) != 0)
            return 0;
        if (desc.rudder != 0) {
            h_strcpy(buf, desc.name);
            if (joy_read(D + 0x408, P(buf), blk) != 0)
                return 0;
        }
        r = joy_parse(desc.id + 1, blk);
    } else if (kind == 1) {
        h_strcpy(buf, desc.name);
        if (desc.rudder != 0)
            h_strcat(buf, D + 0x3f0);
        h_strcat(buf, D + 0x3f8);                           /* " SLEW" */
        if (joy_read(D + 0x408, P(buf), blk) != 0)
            return 0;
        if (desc.rudder != 0) {
            h_strcpy(buf, desc.name);
            h_strcat(buf, D + 0x3f8);
            if (joy_read(D + 0x408, P(buf), blk) != 0)
                return 0;
        }
        r = joy_parse(desc.id + 2, blk);
    }
    if (r == 0)
        M32(blk + 4) = desc.id;
    return r;
}

/* .text+0x91d0 -- install `blk` as joystick `joy`'s config of kind 0
 * (flight) or 1 (slew), unless that one is locked; reset the axis caches. */
static void joy_install(uint32_t kind, uint32_t joy, uint32_t blk)
{
    uint32_t dst;

    if (kind == 0)
        dst = D + 0xd48 + joy * 0x3cc;
    else if (kind == 1)
        dst = D + 0x82c8 + joy * 0x3cc;
    else {
        axis_reset();
        return;
    }
    if (M32(dst) == 0) {
        HC(0x21c, f3)(dst, blk, 0x3cc);
        M32(dst) = 0;
    }
    axis_reset();
}

/* .text+0x9f90 -- a joystick with no [JOYSTICK_nn] of its own gets its
 * model's defaults, flight and slew -- unless the table D+0x350 points at
 * (flight or slew, whichever is current) has it locked.  When a model's
 * string is missing joy_parse leaves `blk` untouched, so what is installed
 * is whatever the stack held: retail's latent bug, kept (no model the host
 * can name lacks its strings). */
static void joy_defaults(uint32_t joy)
{
    uint8_t blk[0x3cc];
    uint32_t present, s;

    if (M32(V32(0x350) + joy * 0x3cc) != 0)
        return;
    s = section_name(D + 0x358, joy);
    HC(0x100, f2)(s, P(&present));
    HC(0x214, f1)(P(&s));
    if (present != 0)
        return;
    if (joy_default(0, joy, P(blk)) == 0)
        joy_install(0, joy, P(blk));
    if (joy_default(1, joy, P(blk)) == 0)
        joy_install(1, joy, P(blk));
}

/* .text+0x93b0 -- CHAIN list 1, then list 6: watch for joysticks.  Each new
 * one (the host counts them, +0x1c0) gets its configs loaded, or its model's
 * defaults; when the first appear the joysticks are enabled from [PREFS]
 * JOYSTICKS (1 if absent).  Once the count holds, D+0x43c frames later the
 * watcher moves itself to list 6. */
static uint32_t FS98_STDCALL joy_watch(uint32_t a, uint32_t b)
{
    uint32_t n, i, on;

    (void)a;
    (void)b;
    n = HC(0x1c0, f0)();
    i = V32(0xbf88);
    if (i < n) {
        for (; i < n; i++) {
            joy_load(i);
            joy_defaults(i);
        }
        if (V32(0xbf88) == 0) {
            on = 0xffffffff;
            HC(0xa8, f3)(D + 0x400, D + 0x448, P(&on));     /* [PREFS] JOYSTICKS */
            controls_joystick(1, on == 0xffffffff ? 1 : on);
        }
        axis_reset();
        V32(0xbf88) = n;
        return n;
    }
    if (i == n) {
        if (V32(0x43c) != 0) {
            V32(0x43c)--;
            return V32(0x43c);
        }
        if (V32(0xbf94) == 0) {
            CALL(CHAIN, 0x1c, f2)(1, P(joy_watch));
            CALL(CHAIN, 0x18, f2)(6, P(joy_watch));
            V32(0xbf94) = 1;
        }
        return V32(0xbf94);
    }
    /* fewer than before; retail compares again (cmp edi,esi / jae), but here
     * that is always true */
    V32(0xbf88) = n;
    return n;
}

/* .text+0x9e20 -- write [section] `key` as a command: its name when the
 * value is one in the command table -- a nameless command's null name, which
 * deletes the key -- else the number. */
static uint32_t write_cmd(uint32_t section, uint32_t key, uint32_t value)
{
    uint32_t i;

    for (i = 0; i < NCMD; i++)
        if (CMD(i).command == value)
            return HC(0xcc, f3)(section, key, P(CMD(i).name));
    return HC(0xac, f3)(section, key, value);
}

/* .text+0x94d0 -- write every attached joystick's config of `table` back to
 * [<prefix>nn], cleared first: LOCKED, TYPE and AXIS_FLAGS, then each axis
 * and button entry that is set. */
static void joy_save(uint32_t prefix, uint32_t table)
{
    char k1c[0x10], k2c[0x10], k3c[0x10], k4c[0x14], k60[0x18], k78[0x18];
    uint8_t j = 0, k;
    uint32_t t, s, e;

    if (V32(0xbf88) == 0)
        return;
    t = 0;
    do {
        s = section_name(prefix, t);
        HC(0xf8, f1)(s);
        e = table + t * 0x3cc;
        HC(0xac, f3)(s, D + 0x468, M32(e + 0));             /* LOCKED */
        HC(0xac, f3)(s, D + 0x460, M32(e + 4));             /* TYPE */
        HC(0xac, f3)(s, D + 0x454, M32(e + 8));             /* AXIS_FLAGS */
        for (k = 0; k < 0x10; k++) {
            if (M32(e + 0xc + k * 4) == 0)
                continue;
            h_strcpy(k2c, D + 0x378);
            NUMF(0x18, f3)(P(&k), 0xb, P(k2c) + h_strlen(k2c));
            h_strcpy(k3c, D + 0x388);
            NUMF(0x18, f3)(P(&k), 0xb, P(k3c) + h_strlen(k3c));
            h_strcpy(k1c, D + 0x398);
            NUMF(0x18, f3)(P(&k), 0xb, P(k1c) + h_strlen(k1c));
            write_cmd(s, P(k2c), M32(e + 0xc + k * 4));
            HC(0xac, f3)(s, P(k3c), M32(e + 0x4c + k * 4));
            HC(0xac, f3)(s, P(k1c), M32(e + 0x8c + k * 4));
        }
        for (k = 0; k < 0x40; k++) {
            if (M32(e + 0xcc + k * 4) != 0) {
                h_strcpy(k60, D + 0x3a8);
                NUMF(0x18, f3)(P(&k), 0xb, P(k60) + h_strlen(k60));
                write_cmd(s, P(k60), M32(e + 0xcc + k * 4));
            }
            if (M32(e + 0x1cc + k * 4) != 0) {
                h_strcpy(k78, D + 0x3c0);
                NUMF(0x18, f3)(P(&k), 0xb, P(k78) + h_strlen(k78));
                HC(0xac, f3)(s, P(k78), M32(e + 0x1cc + k * 4));
            }
        }
        for (k = 0; k < 0x40; k++) {
            if (M32(e + 0x2cc + k * 4) == 0)
                continue;
            h_strcpy(k4c, D + 0x3d8);
            NUMF(0x18, f3)(P(&k), 0xb, P(k4c) + h_strlen(k4c));
            write_cmd(s, P(k4c), M32(e + 0x2cc + k * 4));
        }
        HC(0x214, f1)(P(&s));
        j++;
        t = j;
    } while (t < V32(0xbf88));
}

/* .text+0x94b0 -- write the flight and slew configs back to FLTSIM98.CFG. */
static void joy_config_save(void)
{
    joy_save(D + 0x358, D + 0xd48);                         /* JOYSTICK_ */
    joy_save(D + 0x368, D + 0x82c8);                        /* JOYSTICK_SLEW_ */
}

/* ---- force feedback: DirectInput 5, loaded by CONTROLS itself ------------
 *
 * The settings, [CONTROLS] in FLTSIM98.CFG: force_master_gain (D+0x488),
 * force_master_enabled (D+0x470), force_stick_force_enabled (D+0x474),
 * force_stick_shaker_enabled (D+0x478), force_gear_bump_enabled (D+0x47c),
 * force_crash_enabled (D+0x480), force_ground_bumps_enabled (D+0x484).  The
 * DirectInput module's handle is at D+0xc014 and DirectInputCreateA at
 * D+0xc010; D+0xbfb8 says it is loaded, D+0xbfbc that a device is up,
 * D+0xbfec that one is there, D+0xbfc0/+0xbfc4 hold the interface and the
 * device. */

/* the device code, after the begin and end that use it */
static void ff_effects_init(void);                                  /* .text+0xaeb0 */
static uint32_t di_create(uint32_t wnd, uint32_t inst, uint32_t pdi, uint32_t pdev,
                          uint32_t level);                          /* .text+0xb4b0 */
static uint32_t di_probe(uint32_t inst);                            /* .text+0xb650 */
static void ff_effects_free(void);                                  /* .text+0xab90 */
static uint32_t di_release(uint32_t pdi, uint32_t pdev);            /* .text+0xb6f0 */
static uint32_t FS98_STDCALL ff_frame(uint32_t a, uint32_t b);      /* .text+0xac70, list 1 */
static uint32_t FS98_STDCALL ff_message(uint32_t a, uint32_t b);    /* .text+0xada0, list 0x14 */

/* .text+0xaa00 -- read the settings */
static void ff_settings_load(void)
{
    HC(0xa8, f3)(D + 0x192, D + 0x868, D + 0x488);          /* force_master_gain */
    HC(0xa8, f3)(D + 0x192, D + 0x850, D + 0x470);          /* force_master_enabled */
    HC(0xa8, f3)(D + 0x192, D + 0x834, D + 0x474);          /* force_stick_force_enabled */
    HC(0xa8, f3)(D + 0x192, D + 0x818, D + 0x478);          /* force_stick_shaker_enabled */
    HC(0xa8, f3)(D + 0x192, D + 0x800, D + 0x47c);          /* force_gear_bump_enabled */
    HC(0xa8, f3)(D + 0x192, D + 0x7ec, D + 0x480);          /* force_crash_enabled */
    HC(0xa8, f3)(D + 0x192, D + 0x7d0, D + 0x484);          /* force_ground_bumps_enabled */
}

/* .text+0xaac0 -- write them back */
static uint32_t ff_settings_save(void)
{
    HC(0xac, f3)(D + 0x192, D + 0x868, V32(0x488));
    HC(0xac, f3)(D + 0x192, D + 0x850, V32(0x470));
    HC(0xac, f3)(D + 0x192, D + 0x834, V32(0x474));
    HC(0xac, f3)(D + 0x192, D + 0x818, V32(0x478));
    HC(0xac, f3)(D + 0x192, D + 0x800, V32(0x47c));
    HC(0xac, f3)(D + 0x192, D + 0x7ec, V32(0x480));
    return HC(0xac, f3)(D + 0x192, D + 0x7d0, V32(0x484));
}

/* .text+0xb460 -- load "dinput" and find DirectInputCreateA, once.  -1 if
 * the module will not load; a missing entry point is only found later. */
static uint32_t di_load(void)
{
    uint32_t h;

    if (V32(0xbfb8) != 0)
        return 0;
    h = (uint32_t)(uintptr_t)LoadLibraryA((LPCSTR)(uintptr_t)(D + 0xaf8));     /* "dinput" */
    V32(0xc014) = h;
    if (h == 0)
        return (uint32_t)-1;
    V32(0xbfb8) = 1;
    V32(0xc010) = (uint32_t)(uintptr_t)GetProcAddress((HMODULE)(uintptr_t)h,
                                                      (LPCSTR)(uintptr_t)(D + 0xae4));
    return 0;
}

/* .text+0xb4a0 */
static uint32_t di_none(void)
{
    return 0;
}

/* .text+0xa8d0 -- tail-called by controls_keys_load: read the settings,
 * load DirectInput and, with force feedback on, bring the device up and put
 * the per-frame update on CHAIN list 1 and the message hook on 0x14; with it
 * off, only see whether a device is there.  Then the effect states clear.
 * Returns what the path left in eax, which +0x4c returns. */
static uint32_t ff_begin(void)
{
    uint32_t wnd, inst, r;

    ff_settings_load();
    r = di_load();
    if (r == 0 && (r = V32(0xbfb8)) != 0) {
        HC(0x54, f2)(P(&wnd), P(&inst));                    /* the main window, the instance */
        r = V32(0x470);
        if (r != 0) {
            r = V32(0xbfbc);
            if (r == 0) {
                ff_effects_init();
                r = di_create(wnd, inst, D + 0xbfc0, D + 0xbfc4, 5);
                if ((int32_t)r >= 0) {
                    V32(0xbfbc) = 1;
                    V32(0xbfec) = 1;
                    CALL(CHAIN, 0x18, f2)(1, P(ff_frame));
                    r = CALL(CHAIN, 0x18, f2)(0x14, P(ff_message));
                }
            }
        } else {
            if ((int32_t)di_probe(inst) >= 0)
                V32(0xbfec) = 1;
            r = di_none();
        }
    }
    V32(0xbfb0) = 0;
    V32(0xbfb4) = 0;
    V32(0xbf98) = 0;
    return r;
}

/* .text+0xa9b0 -- take the device down, if up, and save the settings */
static uint32_t ff_end(void)
{
    if (V32(0xbfbc) == 1) {
        CALL(CHAIN, 0x1c, f2)(1, P(ff_frame));
        CALL(CHAIN, 0x1c, f2)(0x14, P(ff_message));
        ff_effects_free();
        di_release(D + 0xbfc0, D + 0xbfc4);
        di_none();
        V32(0xbfbc) = 0;
    }
    return ff_settings_save();
}

/* ---- the device: DirectInput 5 through COM ------------------------------
 *
 * A COM call is the method at `off` in the object's vtable, the object
 * first.  The device (IDirectInputDevice2A) at D+0xbfc4: +0x8 Release, +0x18
 * SetProperty, +0x1c Acquire, +0x20 Unacquire, +0x2c SetDataFormat, +0x34
 * SetCooperativeLevel, +0x48 CreateEffect; an effect (IDirectInputEffect):
 * +0x8 Release, +0x18 SetParameters, +0x1c Start, +0x20 Stop, +0x24
 * GetEffectStatus, +0x28 Download.  The effects: D+0xbfc8 the stick force,
 * +0xbfcc the stick shaker, +0xbfd0/+0xbfd4/+0xbfd8 the nose, left and right
 * gear bumps, +0xbfdc/+0xbfe0 the two ground bumps, +0xbfe4/+0xbfe8 the
 * crash's two parts.  States: D+0xbfb0 ground bumps, +0xbfb4 the shaker,
 * +0xbf98 the crash (1 first part, 2 second, 3 over). */

#define COM(o, off, fn) CALL(M32(o), off, fn)
#define DI_BADARG 0x80070057u                   /* E_INVALIDARG */

static uint32_t eff_free(uint32_t dev, uint32_t pp);

/* MSVCRT _ftol: truncate whatever the precision control left, 64 bits */
static int64_t ftol(long double v)
{
    uint16_t cw, chop;
    int64_t r;

    __asm__ volatile("fnstcw %0" : "=m"(cw));
    chop = cw | 0x0c00;
    __asm__ volatile("fldcw %1\n\tfistpll %0\n\tfldcw %2"
                     : "=m"(r) : "m"(chop), "m"(cw), "t"(v) : "st");
    return r;
}

/* fpatan: atan2(y, x), st1 = y, st0 = x */
static long double x87_atan2(long double y, long double x)
{
    long double r;
    __asm__ ("fpatan" : "=t" (r) : "0" (x), "u" (y) : "st(1)");
    return r;
}

static long double x87_sqrt(long double x)
{
    long double r;
    __asm__ ("fsqrt" : "=t" (r) : "0" (x));
    return r;
}

/* .text+0xb6b0 -- EnumDevices' callback: the first joystick's
 * DIDEVICEINSTANCE (0x244 bytes) into *ref, and stop */
static uint32_t FS98_STDCALL di_enum(uint32_t ddi, uint32_t ref)
{
    uint32_t i;

    if (ref == 0)
        return 0;
    if (M8(ddi + 0x24) != 4)                            /* dwDevType: DIDEVTYPE_JOYSTICK */
        return 1;
    for (i = 0; i < 0x91; i++)
        M32(ref + i * 4) = M32(ddi + i * 4);
    return 0;
}

/* .text+0xb4b0 -- DirectInputCreateA, the first attached force-feedback
 * joystick, its IDirectInputDevice2A at *pdev with the joystick data format,
 * `level` cooperation with `wnd` and the autocentre off.  A failure releases
 * what was had -- without clearing *pdi and *pdev -- and returns the
 * HRESULT.  If no joystick answers, `ddi` is whatever the stack held. */
static uint32_t di_create(uint32_t wnd, uint32_t inst, uint32_t pdi, uint32_t pdev,
                          uint32_t level)
{
    uint8_t ddi[0x244];
    uint32_t dev1, r, prop[5];

    if (wnd == 0)
        return DI_BADARG;
    r = ((f4)(uintptr_t)V32(0xc010))(inst, 0x500, pdi, 0);
    if ((int32_t)r < 0)
        return r;
    r = COM(M32(pdi), 0x10, f5)(M32(pdi), 4, P(di_enum), P(ddi), 0x101);    /* EnumDevices */
    if ((int32_t)r < 0)
        return r;
    dev1 = 0;
    r = COM(M32(pdi), 0xc, f4)(M32(pdi), P(ddi + 4), P(&dev1), 0);          /* CreateDevice */
    if ((int32_t)r < 0) {
        COM(M32(pdi), 0x8, f1)(M32(pdi));
        return r;
    }
    r = COM(dev1, 0x0, f3)(dev1, RDATA + 0x80, pdev);    /* QueryInterface(IID_IDirectInputDevice2A) */
    if ((int32_t)r < 0) {
        COM(M32(pdi), 0x8, f1)(M32(pdi));
        COM(dev1, 0x8, f1)(dev1);
        return r;
    }
    COM(dev1, 0x8, f1)(dev1);
    r = COM(M32(pdev), 0x2c, f2)(M32(pdev), RDATA + 0x250);              /* c_dfDIJoystick */
    if ((int32_t)r >= 0) {
        r = COM(M32(pdev), 0x34, f3)(M32(pdev), wnd, level);
        if ((int32_t)r >= 0) {
            prop[0] = 0x14;                             /* DIPROPDWORD */
            prop[1] = 0x10;
            prop[2] = 0;
            prop[3] = 0;                                /* DIPH_DEVICE */
            prop[4] = 0;                                /* DIPROPAUTOCENTER_OFF */
            r = COM(M32(pdev), 0x18, f3)(M32(pdev), 9, P(prop));            /* DIPROP_AUTOCENTER */
            if ((int32_t)r >= 0)
                return 0;
        }
    }
    COM(M32(pdev), 0x8, f1)(M32(pdev));
    COM(M32(pdi), 0x8, f1)(M32(pdi));
    return r;
}

/* .text+0xb650 -- is there a force-feedback joystick?  EnumDevices' result */
static uint32_t di_probe(uint32_t inst)
{
    uint8_t ddi[0x244];
    uint32_t di, r;

    r = ((f4)(uintptr_t)V32(0xc010))(inst, 0x500, P(&di), 0);
    if ((int32_t)r < 0)
        return r;
    r = COM(di, 0x10, f5)(di, 4, P(di_enum), P(ddi), 0x101);
    COM(di, 0x8, f1)(di);
    return r;
}

/* .text+0xb6f0 -- unacquire and release the device, release DirectInput */
static uint32_t di_release(uint32_t pdi, uint32_t pdev)
{
    uint32_t r = 0;

    if (M32(pdev) != 0) {
        COM(M32(pdev), 0x20, f1)(M32(pdev));            /* Unacquire */
        r = COM(M32(pdev), 0x8, f1)(M32(pdev));
        M32(pdev) = 0;
    }
    if (M32(pdi) != 0) {
        r = COM(M32(pdi), 0x8, f1)(M32(pdi));
        M32(pdi) = 0;
    }
    return r;
}

/* the effects' DIEFFECT, as dwords: +0 dwSize, +1 dwFlags, +2 dwDuration,
 * +3 dwSamplePeriod, +4 dwGain, +5 dwTriggerButton, +6
 * dwTriggerRepeatInterval, +7 cAxes, +8 rgdwAxes, +9 rglDirection, +0xa
 * lpEnvelope, +0xb cbTypeSpecificParams, +0xc lpvTypeSpecificParams.  Every
 * effect is polar over X and Y (DIEFF_POLAR | DIEFF_OBJECTOFFSETS). */

/* .text+0xb730 -- a constant force of `mag` towards `dir` for `dur`,
 * started */
static uint32_t eff_constant(uint32_t dev, uint32_t pp, uint32_t mag, uint32_t dir, uint32_t dur)
{
    uint32_t axes[2], dirs[2], eff[13], force, e, r;

    if (dev == 0 || pp == 0)
        return DI_BADARG;
    M32(pp) = 0;
    r = COM(dev, 0x1c, f1)(dev);                        /* Acquire */
    if ((int32_t)r < 0)
        return r;
    dirs[0] = dir;
    dirs[1] = 0;
    force = mag;                                        /* DICONSTANTFORCE */
    axes[0] = 0;                                        /* DIJOFS_X, DIJOFS_Y */
    axes[1] = 4;
    eff[0] = 0x34;
    eff[1] = 0x22;
    eff[2] = dur;
    eff[3] = 0x2710;
    eff[4] = V32(0x488);
    eff[5] = 0xffffffff;                                /* DIEB_NOTRIGGER */
    eff[6] = 0;
    eff[7] = 2;
    eff[8] = P(axes);
    eff[9] = P(dirs);
    eff[10] = 0;
    eff[11] = 4;
    eff[12] = P(&force);
    r = COM(dev, 0x48, f5)(dev, RDATA + 0x190, P(eff), P(&e), 0);       /* GUID_ConstantForce */
    if ((int32_t)r < 0)
        return r;
    M32(pp) = e;
    r = COM(e, 0x1c, f3)(e, 1, 0);                      /* Start */
    return (int32_t)r >= 0 ? 0 : r;
}

/* .text+0xb830 -- the constant force again: started anew if the device was
 * not acquired (DI_OK), its magnitude and direction set if it was (S_FALSE) */
static uint32_t eff_constant_set(uint32_t dev, uint32_t pp, uint32_t mag, uint32_t dir)
{
    uint32_t axes[2], dirs[2], eff[13], force, r, i;

    if (pp == 0 || M32(pp) == 0)
        return DI_BADARG;
    r = COM(dev, 0x1c, f1)(dev);
    if (r == 0) {
        COM(M32(pp), 0x28, f1)(M32(pp));                /* Download */
        return COM(M32(pp), 0x1c, f3)(M32(pp), 1, 0);
    }
    if (r != 1)
        return r;
    force = mag;
    axes[0] = 0;
    dirs[0] = dir;
    dirs[1] = 0;
    for (i = 1; i < 13; i++)
        eff[i] = 0;
    eff[0] = 0x34;
    axes[1] = 4;
    eff[8] = P(axes);
    eff[9] = P(dirs);
    eff[11] = 4;
    eff[1] = 0x22;
    eff[7] = 2;
    eff[12] = P(&force);
    return COM(M32(pp), 0x18, f3)(M32(pp), P(eff), 0x140);   /* DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS */
}

/* .text+0xb910 -- a periodic effect, kind 0 square, 1 or 2 sine (2 a
 * quarter-phase on), `mag` towards `dir` every `period` for `dur` with the
 * envelope `env`, started.  For any other kind no effect is made and the
 * device itself is then "started" as one: retail's latent bug, no caller
 * passes such a kind. */
static uint32_t eff_periodic(uint32_t dev, uint32_t pp, int32_t kind, uint32_t mag, uint32_t dir,
                             uint32_t period, uint32_t dur, uint32_t env)
{
    uint32_t axes[2], dirs[2], per[4], eff[13], e, r;

    if (dev == 0 || pp == 0)
        return DI_BADARG;
    M32(pp) = 0;
    r = COM(dev, 0x1c, f1)(dev);
    if ((int32_t)r < 0)
        return r;
    per[0] = mag;                                       /* DIPERIODIC */
    per[1] = 0;
    per[2] = kind == 2 ? 0x2328 : 0;
    per[3] = period;
    axes[0] = 0;
    axes[1] = 4;
    dirs[0] = dir;
    dirs[1] = 0;
    eff[0] = 0x34;
    eff[1] = 0x22;
    eff[2] = dur;
    eff[3] = 0;
    eff[4] = V32(0x488);
    eff[5] = 0xffffffff;
    eff[6] = 0;
    eff[7] = 2;
    eff[8] = P(axes);
    eff[9] = P(dirs);
    eff[10] = env;
    eff[11] = 0x10;
    eff[12] = P(per);
    e = dev;
    if (kind == 0)
        r = COM(dev, 0x48, f5)(dev, RDATA + 0x1b0, P(eff), P(&e), 0);   /* GUID_Square */
    else if (kind > 0 && kind <= 2)
        r = COM(dev, 0x48, f5)(dev, RDATA + 0x1c0, P(eff), P(&e), 0);   /* GUID_Sine */
    if ((int32_t)r < 0)
        return r;
    M32(pp) = e;
    r = COM(e, 0x1c, f3)(e, 1, 0);
    return (int32_t)r >= 0 ? 0 : r;
}

/* .text+0xba60 -- a periodic effect again, as eff_constant_set; its
 * DIPERIODIC is the one at D+0xbfa0 */
static uint32_t eff_periodic_set(uint32_t dev, uint32_t pp, uint32_t mag, uint32_t dir,
                                 uint32_t period)
{
    uint32_t axes[2], dirs[2], eff[13], r, i;

    if (dev == 0 || pp == 0 || M32(pp) == 0)
        return DI_BADARG;
    r = COM(dev, 0x1c, f1)(dev);
    if (r == 0) {
        COM(M32(pp), 0x28, f1)(M32(pp));
        return COM(M32(pp), 0x1c, f3)(M32(pp), 1, 0);
    }
    if (r != 1)
        return r;
    V32(0xbfa0) = mag;
    axes[0] = 0;
    dirs[0] = dir;
    dirs[1] = 0;
    for (i = 1; i < 13; i++)
        eff[i] = 0;
    eff[0] = 0x34;
    V32(0xbfac) = period;
    eff[8] = P(axes);
    axes[1] = 4;
    eff[1] = 0x22;
    eff[7] = 2;
    eff[9] = P(dirs);
    eff[11] = 0x10;
    eff[12] = D + 0xbfa0;
    return COM(M32(pp), 0x18, f3)(M32(pp), P(eff), 0x140);
}

/* .text+0xbb50 -- stop and release the effect at *pp: S_FALSE (1) if there
 * is none.  It is acquired and downloaded again before it is stopped. */
static uint32_t eff_free(uint32_t dev, uint32_t pp)
{
    uint32_t r;

    if (dev == 0 || pp == 0)
        return DI_BADARG;
    if (M32(pp) == 0)
        return 1;
    COM(dev, 0x1c, f1)(dev);
    COM(M32(pp), 0x28, f1)(M32(pp));
    COM(M32(pp), 0x20, f1)(M32(pp));                    /* Stop */
    r = COM(M32(pp), 0x8, f1)(M32(pp));
    M32(pp) = 0;
    return r;
}

/* .text+0xbba0 -- release the effect at *pp once it has stopped playing */
static uint32_t eff_reap(uint32_t dev, uint32_t pp)
{
    uint32_t status, r;

    if (dev == 0 || pp == 0 || M32(pp) == 0)
        return DI_BADARG;
    r = COM(M32(pp), 0x24, f2)(M32(pp), P(&status));    /* GetEffectStatus */
    if (r != 0)
        return r;
    if ((uint8_t)status & 1)                            /* DIEGES_PLAYING */
        return 1;
    return eff_free(dev, pp);
}

/* .text+0xbbf0 -- a bump: a sine, a quarter-phase on, of `mag` towards `dir`
 * lasting `dur` with a period of four times that, started */
static uint32_t eff_bump(uint32_t dev, uint32_t pp, uint32_t mag, uint32_t dir, uint32_t dur)
{
    uint32_t axes[2], dirs[2], per[4], eff[13], e, r;

    if (dev == 0 || pp == 0)
        return DI_BADARG;
    M32(pp) = 0;
    r = COM(dev, 0x1c, f1)(dev);
    if ((int32_t)r < 0)
        return r;
    per[0] = mag;
    per[1] = 0;
    per[2] = 0x2328;
    per[3] = dur * 4;
    dirs[0] = dir;
    dirs[1] = 0;
    axes[0] = 0;
    axes[1] = 4;
    eff[0] = 0x34;
    eff[1] = 0x22;
    eff[2] = dur;
    eff[3] = 0;
    eff[4] = V32(0x488);
    eff[5] = 0xffffffff;
    eff[6] = 0;
    eff[7] = 2;
    eff[8] = P(axes);
    eff[9] = P(dirs);
    eff[10] = 0;
    eff[11] = 0x10;
    eff[12] = P(per);
    r = COM(dev, 0x48, f5)(dev, RDATA + 0x1c0, P(eff), P(&e), 0);       /* GUID_Sine */
    if ((int32_t)r < 0)
        return r;
    M32(pp) = e;
    r = COM(e, 0x1c, f3)(e, 1, 0);
    return (int32_t)r >= 0 ? 0 : r;
}

/* .text+0xb390 -- the stick force from (x, y): direction atan2(y, x) turned
 * to hundredths of a degree from the north, magnitude the length clamped to
 * +-10000, on the x87.  A centred stick (both 0, or unordered) takes x as
 * 0.001; an unordered length clamps to -10000. */
static uint32_t ff_stick(double x, double y)
{
    long double sx, a, r2, m;
    double dir;
    uint32_t d, n;

    if (V32(0xbfbc) == 0 || V32(0xbfc8) == 0)
        return 0;
    sx = x;
    if ((sx == 0.0L || sx != sx) && ((long double)y == 0.0L || y != y))
        sx = RD(0x288);
    a = x87_atan2(y, sx);
    r2 = sx * sx + (long double)y * (long double)y;
    a = a - RD(0x290);
    a = a * RD(0x298);
    m = x87_sqrt(r2);
    dir = (double)a;
    if (!(m < RD(0x2a0) || m == RD(0x2a0) || m != m))
        m = RD(0x2a0);
    if (m < RD(0x2a8) || m != m)
        m = RD(0x2a8);
    d = (uint32_t)ftol(dir);
    n = (uint32_t)ftol(m);
    return eff_constant_set(V32(0xbfc4), D + 0xbfc8, n, d);
}

/* .text+0xabb0 -- release every effect but the stick force */
static uint32_t ff_free_effects(void)
{
    eff_free(V32(0xbfc4), D + 0xbfcc);
    eff_free(V32(0xbfc4), D + 0xbfd0);
    eff_free(V32(0xbfc4), D + 0xbfd4);
    eff_free(V32(0xbfc4), D + 0xbfd8);
    eff_free(V32(0xbfc4), D + 0xbfdc);
    eff_free(V32(0xbfc4), D + 0xbfe0);
    eff_free(V32(0xbfc4), D + 0xbfe4);
    eff_free(V32(0xbfc4), D + 0xbfe8);
    V32(0xbfb0) = 0;
    V32(0xbfb4) = 0;
    V32(0xbf98) = 0;
    return 0;
}

/* .text+0xab90 -- and the stick force */
static void ff_effects_free(void)
{
    eff_free(V32(0xbfc4), D + 0xbfc8);
    ff_free_effects();
}

/* .text+0xac50 -- every effect off, the stick force to nothing */
static void ff_stop_all(void)
{
    ff_free_effects();
    ff_stick(0.0, 0.0);
}

/* .text+0xaeb0 -- the effects' defaults, then the aircraft's own from the
 * [forcefeedback] section of the file ACONTAIN +0x30 names */
static void ff_effects_init(void)
{
    static const uint16_t ints[][2] = {
        { 0xabc, 0x48c }, { 0xaa4, 0xbff8 }, { 0xa90, 0x490 },   /* the stick shaker */
        { 0xa74, 0x494 }, { 0xa58, 0x498 }, { 0xa40, 0x49c },    /* the nose gear */
        { 0xa24, 0x4a0 }, { 0xa08, 0x4a4 }, { 0x9f0, 0x4a8 },    /* the left */
        { 0x9d4, 0x4ac }, { 0x9b8, 0x4b0 }, { 0x99c, 0x4b4 },    /* the right */
        { 0x984, 0x4b8 }, { 0x970, 0x4bc },                      /* ground bumps 1 */
    };
    static const uint16_t more[][2] = {
        { 0x92c, 0x4c8 }, { 0x918, 0x4cc },                      /* ground bumps 2 */
    };
    static const uint16_t crash[][2] = {
        { 0x8d8, 0x4d8 }, { 0x8c4, 0x4dc }, { 0x8b0, 0x4e4 },
        { 0x89c, 0x4e8 }, { 0x88c, 0x4ec }, { 0x87c, 0x4f0 },
    };
    char sect[0x10], path[0x104];
    uint32_t i;

    M32(P(sect) + 0) = V32(0xad4);                      /* "forcefeedback" */
    M32(P(sect) + 4) = V32(0xad8);
    M32(P(sect) + 8) = V32(0xadc);
    M16(P(sect) + 0xc) = V16(0xae0);
    V32(0x494) = 0x1770;
    V32(0x4a0) = 0x1770;
    V32(0x4ac) = 0x1770;
    V32(0x48c) = 0x1388;
    V32(0x49c) = 0x3d090;
    V32(0x4a8) = 0x3d090;
    V32(0x4b4) = 0x3d090;
    V32(0x4b8) = 0x1388;
    V32(0x4c8) = 0x1388;
    V32(0x4d8) = 0x2710;
    V32(0x4e4) = 0x2710;
    V32(0x4ec) = 0x3d090;
    V32(0x4f0) = 0x4c4b40;
    V32(0x508) = 0x4c4b40;
    V32(0xbff8) = 0;
    V32(0x490) = 0x1b207;
    V32(0x498) = 0x4650;
    V32(0x4a4) = 0x2328;
    V32(0x4b0) = 0x6978;
    V32(0x4bc) = 0x1d4c;
    V32(0xc000) = 0;                                    /* ground_bumps_intercept1 0.0 */
    V32(0xc004) = 0;
    V32(0x4c0) = 0;                                     /* ground_bumps_slope1 1.0 */
    V32(0x4c4) = 0x3ff00000;
    V32(0x4cc) = 0x2904;
    V32(0xc008) = 0;                                    /* ground_bumps_intercept2 0.0 */
    V32(0xc00c) = 0;
    V32(0x4d0) = 0;                                     /* ground_bumps_slope2 1.0 */
    V32(0x4d4) = 0x3ff00000;
    V32(0x4dc) = 0x3e8;
    V32(0x4e8) = 0x2328;
    if (CALL(ACONTAIN, 0x30, f4)(G + 0x11e0, 0, P(path), 0x104) == 0)
        return;
    for (i = 0; i < sizeof ints / sizeof ints[0]; i++)
        HC(0xd0, f5)(P(sect), D + ints[i][0], D + ints[i][1], P(path), 0x24);
    HC(0xe0, f5)(P(sect), D + 0x958, D + 0xc000, P(path), 0x24);   /* ground_bumps_intercept1 */
    HC(0xe0, f5)(P(sect), D + 0x944, D + 0x4c0, P(path), 0x24);    /* ground_bumps_slope1 */
    for (i = 0; i < sizeof more / sizeof more[0]; i++)
        HC(0xd0, f5)(P(sect), D + more[i][0], D + more[i][1], P(path), 0x24);
    HC(0xe0, f5)(P(sect), D + 0x900, D + 0xc008, P(path), 0x24);   /* ground_bumps_intercept2 */
    HC(0xe0, f5)(P(sect), D + 0x8ec, D + 0x4d0, P(path), 0x24);    /* ground_bumps_slope2 */
    for (i = 0; i < sizeof crash / sizeof crash[0]; i++)
        HC(0xd0, f5)(P(sect), D + crash[i][0], D + crash[i][1], P(path), 0x24);
    V32(0x508) = V32(0x4f0);
}

/* .text+0xac70 -- CHAIN list 1 while the device is up: the stick force made
 * when it is wanted, every effect off while the sim is paused (GLOBAL
 * +0x5dc) or the joysticks are off, the bumps reaped as they end, and the
 * crash's second part once its first is over */
static uint32_t FS98_STDCALL ff_frame(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    if (V32(0xbfbc) == 0)
        return 0;
    if (V32(0xbfc8) == 0 && V32(0x474) != 0) {
        eff_constant(V32(0xbfc4), D + 0xbfc8, 0, 0, 0xffffffff);
        ff_stick(0.0, 0.0);
    }
    if (M16(G + 0x5dc) == 0 && controls_joystick(0, 0) != 0)
        V32(0xbff4) = 0;
    else {
        ff_stop_all();
        V32(0xbff4) = 1;
    }
    eff_reap(V32(0xbfc4), D + 0xbfd0);
    eff_reap(V32(0xbfc4), D + 0xbfd4);
    eff_reap(V32(0xbfc4), D + 0xbfd8);
    if (V32(0xbf98) == 1) {
        if (eff_reap(V32(0xbfc4), D + 0xbfe4) != 0)
            return 0;
        eff_periodic(V32(0xbfc4), D + 0xbfe8, 0, V32(0x4e4), V32(0x4e8), V32(0x4ec), V32(0x4f0),
                     D + 0x4f8);
        V32(0xbf98) = 2;
    } else if (V32(0xbf98) == 2)
        eff_reap(V32(0xbfc4), D + 0xbfe8);
    return 0;
}

/* .text+0xada0 -- CHAIN list 0x14, the sim's messages: 0x20 stops every
 * effect; 0x38, 0x40, 0x42 and 0x44 the shaker and the ground bumps; 0x60
 * stops them all and reads the aircraft's settings again */
static uint32_t FS98_STDCALL ff_message(uint32_t msg, uint32_t b)
{
    (void)b;
    if (V32(0xbfbc) == 0)
        return 0;
    switch (msg) {
    case 0x20:
        ff_stop_all();
        break;
    case 0x38: case 0x40: case 0x42: case 0x44:
        if (V32(0xbfb4) == 1) {
            eff_free(V32(0xbfc4), D + 0xbfcc);
            V32(0xbfb4) = 2;
        }
        if (V32(0xbfb0) == 1) {
            eff_free(V32(0xbfc4), D + 0xbfdc);
            eff_free(V32(0xbfc4), D + 0xbfe0);
            V32(0xbfb0) = 2;
        }
        break;
    case 0x60:
        ff_stop_all();
        ff_effects_init();
        V32(0xbff0) = 0;
        break;
    }
    return 0;
}

/* ======== the keyboard-assignments dialog: .text+0x7e40..+0x8e5f, +0x9130 ========
 *
 * Dialog 0x140a of English\Options.DLL, run through the host's dialog engine
 * (+0x10c) on the block at D+0x240: +0x0 the answer (1 OK, 0 cancel); +0x4,
 * +0x1c, +0x20, +0x2c the callbacks Options calls (dlg_none, dlg_adjust,
 * dlg_refresh, dlg_reset); +0x10 no joysticks; +0x14 the joysticks-enabled
 * control's record (UTIL's exchange, table D+0x338, variable D+0xb00); +0x24
 * the sensitivity records (G +0xc02..+0xc0c), +0x28 which list the page
 * shows (0 flight, 1 slew); +0x30 and +0x40 the flight and slew lists;
 * +0xc0..+0xf0 the force-feedback settings.
 *
 * A list (0x10 bytes): +0 the commands' count, +4 for each { its display
 * name, a host copy; its command id }, in the order of their display string
 * ids; +8 the devices' count, +0xc for each (0x14 bytes) { 0 keyboard or 1 a
 * joystick, the joystick's index, its name, its bindings, the joystick's
 * extras }.  A binding is { what is bound -- a key and its flags << 8 for the
 * keyboard, 1..0x10 an axis or 0x11..0x50 a button for a joystick -- and a
 * type }; a joystick's extra is { scale, null, repeat }. */

uint32_t FS98_STDCALL controls_force(uint32_t op, uint32_t blk);    /* .text+0xa400 */

/* .text+0x8650 -- the command after *prev in display order: the smallest
 * display string id above *prev among commands whose kind has a bit of
 * `mask`.  0 with its index in *idx and its id in *prev, else -1. */
static uint32_t next_cmd(uint32_t idx, uint32_t prev, uint32_t mask)
{
    uint32_t best = 0xffffffff, at = 0, i;

    for (i = 0; i < NCMD; i++) {
        uint32_t v = CMD(i).sid;
        if (v < best && v > M32(prev) && (CMD(i).kind & mask) != 0) {
            best = v;
            at = i;
        }
    }
    if (best == 0xffffffff)
        return 0xffffffff;
    M32(idx) = at;
    M32(prev) = best;
    return 0;
}

/* .text+0x86c0 -- the list position of command `cmd`, into *at: 1, else 0 */
static uint32_t find_pair(uint32_t list, uint32_t cmd, uint32_t at)
{
    uint32_t i;

    for (i = 0; i < M32(list); i++)
        if (M32(M32(list + 4) + i * 8 + 4) == cmd) {
            M32(at) = i;
            return 1;
        }
    return 0;
}

/* .text+0x8390 -- the keyboard's bindings from the accelerator table
 * `accel`: each listed command's first key, and whether a joystick can take
 * it (its kind has 1 or 2) */
static uint32_t fill_keys(uint32_t bind, uint32_t accel, uint32_t mode)
{
    uint32_t n, buf, prev, idx, r;

    r = HC(0x1fc, f6)(accel, 0, 0, 2, 0, P(&n));
    if (r != 0)
        return r;
    HC(0x20c, f4)(1, n * 0xc, P(&buf), 1);
    HC(0x1fc, f6)(accel, 0, 0, 2, buf, P(&n));
    prev = 0;
    if (next_cmd(P(&idx), P(&prev), mode) == 0) {
        do {
            uint32_t e;
            M32(bind) = 0;
            for (e = 0; e < n; e++)
                if (M32(buf + e * 0xc + 8) == CMD(idx).command) {
                    M32(bind) = (M32(buf + e * 0xc + 4) << 8) + (M32(buf + e * 0xc) & 0xff);
                    break;
                }
            M32(bind + 4) = (CMD(idx).kind & 3) != 0;
            bind += 8;
        } while (next_cmd(P(&idx), P(&prev), mode) == 0);
    }
    return HC(0x214, f1)(P(&buf));
}

/* .text+0x84d0 -- a joystick's bindings and extras from its config `cfg`:
 * each axis's and button's command, found in the list; then every binding's
 * type, 2 if a joystick can take the command, else 3 */
static uint32_t fill_joy(uint32_t list, uint32_t bind, uint32_t ext, uint32_t cfg, uint32_t mode)
{
    uint32_t k, a, at, idx, prev, r;

    for (k = 0; k < M32(list); k++) {
        M32(bind + k * 8) = 0;
        M32(bind + k * 8 + 4) = 0;
        M32(ext + k * 0xc) = 0;
        M32(ext + k * 0xc + 4) = 0;
        M32(ext + k * 0xc + 8) = 0;
    }
    for (a = 0; a < 0x10; a++) {
        if (M32(cfg + 0xc + a * 4) == 0)
            continue;
        if (find_pair(list, M32(cfg + 0xc + a * 4), P(&at)) == 0)
            continue;
        M32(bind + at * 8) = a + 1;
        M32(ext + at * 0xc) = M32(cfg + 0x4c + a * 4);
        M32(ext + at * 0xc + 4) = M32(cfg + 0x8c + a * 4);
    }
    for (a = 0; a < 0x40; a++) {
        if (M32(cfg + 0xcc + a * 4) == 0)
            continue;
        if (find_pair(list, M32(cfg + 0xcc + a * 4), P(&at)) == 0)
            continue;
        M32(bind + at * 8) = a + 0x11;
        M32(ext + at * 0xc + 8) = M32(cfg + 0x1cc + a * 4);
    }
    prev = 0;
    r = next_cmd(P(&idx), P(&prev), mode);
    for (k = 0; r == 0; k++) {
        M32(bind + k * 8 + 4) = (CMD(idx).kind & 3) != 0 ? 2 : 3;
        r = next_cmd(P(&idx), P(&prev), mode);
    }
    return r;
}

/* .text+0x9130 -- joystick `joy`'s config of `kind` (0 flight, 1 slew) into
 * `out`, its present axes those the host reads now */
static void joy_current(uint32_t kind, uint32_t joy, uint32_t out)
{
    uint8_t state[0x14c];
    uint32_t r;

    M32(P(state) + 8) = 0xff;
    r = HC(0x1c4, f2)(joy, P(state));
    if (kind == 0)
        HC(0x21c, f3)(out, D + 0xd48 + joy * 0x3cc, 0x3cc);
    else if (kind == 1)
        HC(0x21c, f3)(out, D + 0x82c8 + joy * 0x3cc, 0x3cc);
    if (r == 0)
        M32(out + 8) = M32(P(state) + 8);
}

/* .text+0x80a0 -- build a list: the commands of `mode` (4 flight or 8 slew
 * keys, with 1 or 2 for the joysticks), the keyboard's bindings from `accel`
 * and, with a joystick bit, each attached joystick's */
static void build_list(uint32_t list, uint32_t accel, uint32_t mode)
{
    uint8_t desc[0x50], cfg[0x3cc];
    uint32_t prev, idx, n, pairs, off, devs, ndev, j;

    prev = 0;
    n = 0;
    if (next_cmd(P(&idx), P(&prev), mode) == 0)
        do
            n++;
        while (next_cmd(P(&idx), P(&prev), mode) == 0);
    HC(0x20c, f4)(1, n * 8, P(&pairs), 1);
    M32(list) = n;
    M32(list + 4) = pairs;
    prev = 0;
    off = 0;
    if (next_cmd(P(&idx), P(&prev), mode) == 0)
        do {
            HC(0x2a4, f2)(CMD(idx).sid, pairs + off);
            off += 8;
            M32(pairs + off - 4) = CMD(idx).command;
        } while (next_cmd(P(&idx), P(&prev), mode) == 0);
    ndev = 1;
    if (mode & 3)
        ndev = HC(0x1c0, f0)() + 1;
    HC(0x20c, f4)(1, ndev * 0x14, P(&devs), 1);
    M32(list + 8) = ndev;
    M32(list + 0xc) = devs;
    HC(0x20c, f4)(1, n * 8, devs + 0xc, 1);
    M32(devs + 0x10) = 0;
    M32(devs + 0) = 0;
    M32(devs + 4) = 0;
    HC(0x20c, f4)(1, h_strlen(D + 0x1d0) + 1, devs + 8, 0);     /* the keyboard's name */
    h_strcpy(M32(devs + 8), D + 0x1d0);
    fill_keys(M32(devs + 0xc), accel, mode);
    if ((mode & 3) == 0 || ndev <= 1)
        return;
    for (j = 0; j + 1 < ndev; j++) {
        uint32_t e = M32(list + 0xc) + (j + 1) * 0x14;
        HC(0x20c, f4)(1, n * 8, e + 0xc, 1);
        HC(0x20c, f4)(1, n * 0xc, M32(list + 0xc) + (j + 1) * 0x14 + 0x10, 1);
        M32(M32(list + 0xc) + (j + 1) * 0x14) = 1;
        M32(M32(list + 0xc) + (j + 1) * 0x14 + 4) = j;
        HC(0x1c8, f2)(j, P(desc));
        HC(0x20c, f4)(1, h_strlen(P(desc) + 0x10) + 1, M32(list + 0xc) + (j + 1) * 0x14 + 8, 0);
        h_strcpy(M32(M32(list + 0xc) + (j + 1) * 0x14 + 8), P(desc) + 0x10);
        if (mode & 1)
            joy_current(0, j, P(cfg));
        else if (mode & 2)
            joy_current(1, j, P(cfg));
        e = M32(list + 0xc) + (j + 1) * 0x14;
        fill_joy(list, M32(e + 0xc), M32(e + 0x10), P(cfg), mode);
    }
}

/* .text+0x8700 -- the sensitivity records: labels from ENGLISH.FLL, values
 * from GLOBAL +0xc02..+0xc0c */
static void records_init(uint32_t rec)
{
    M32(rec + 0x2c) = 2;
    M32(rec + 0x30) = D + 0x208;                            /* the mouse's name */
    HC(0x2a0, f3)(0xf591, D + 0xb08, 0x40);
    M32(rec + 0x34) = D + 0xb08;
    M16(rec + 0x40) = M16(G + 0xc08);
    HC(0x2a0, f3)(0xf590, D + 0xb48, 0x40);
    M32(rec + 0x44) = D + 0xb48;
    M16(rec + 0x50) = M16(G + 0xc0a);
    HC(0x2a0, f3)(0xf592, D + 0xb88, 0x40);
    M32(rec + 0x54) = D + 0xb88;
    M32(rec + 0x64) = 0;
    M16(rec + 0x60) = M16(G + 0xc0c);
    M32(rec + 0x68) = D + 0x1d0;                            /* the keyboard's name */
    HC(0x2a0, f3)(0xf593, D + 0xbc8, 0x40);
    M32(rec + 0x6c) = D + 0xbc8;
    HC(0x2a0, f3)(0xf596, D + 0xc88, 0x40);
    M32(rec + 0x70) = D + 0xc88;
    M16(rec + 0x78) = M16(G + 0xc02);
    HC(0x2a0, f3)(0xf594, D + 0xc08, 0x40);
    M32(rec + 0x7c) = D + 0xc08;
    HC(0x2a0, f3)(0xf597, D + 0xcc8, 0x40);
    M32(rec + 0x80) = D + 0xcc8;
    M16(rec + 0x88) = M16(G + 0xc04);
    HC(0x2a0, f3)(0xf595, D + 0xc48, 0x40);
    M32(rec + 0x8c) = D + 0xc48;
    HC(0x2a0, f3)(0xf598, D + 0xd08, 0x40);
    M32(rec + 0x90) = D + 0xd08;
    M16(rec + 0x98) = M16(G + 0xc06);
}

/* .text+0x88b0 -- and back */
static void records_apply(uint32_t rec)
{
    M16(G + 0xc08) = M16(rec + 0x40);
    M16(G + 0xc0a) = M16(rec + 0x50);
    M16(G + 0xc0c) = M16(rec + 0x60);
    M16(G + 0xc02) = M16(rec + 0x78);
    M16(G + 0xc04) = M16(rec + 0x88);
    M16(G + 0xc06) = M16(rec + 0x98);
}

/* .text+0x8930 -- the keyboard's bindings into the accelerator table
 * `accel`, emptied first */
static uint32_t apply_keys(uint32_t list, uint32_t accel)
{
    uint32_t r, k, bind, ent[3];

    r = HC(0x1f0, f1)(accel);
    if (r != 0)
        return r;
    bind = M32(M32(list + 0xc) + 0xc);
    for (k = 0; k < M32(list); k++) {
        uint32_t spec = M32(bind + k * 8);
        ent[2] = M32(M32(list + 4) + k * 8 + 4);
        ent[0] = spec & 0xff;
        ent[1] = spec >> 8;
        if (ent[0] != 0)
            r = HC(0x1f4, f3)(accel, P(ent), 1);
    }
    return r;
}

/* .text+0x89b0 -- each joystick's bindings into a fresh config of `kind`,
 * installed: AXIS_FLAGS the bound axes, their events, scales and nulls, the
 * buttons' down events and repeats.  LOCKED, TYPE and the up events stay 0. */
static void apply_joys(uint32_t list, uint32_t kind)
{
    uint8_t desc[0x50], cfg[0x3cc];
    uint32_t n = M32(list), pairs = M32(list + 4), j;

    if (M32(list + 8) <= 1)
        return;
    for (j = 0; j + 1 < M32(list + 8); j++) {
        uint32_t e = M32(list + 0xc) + (j + 1) * 0x14;
        uint32_t ext = M32(e + 0x10), bind = M32(e + 0xc), k;
        HC(0x1c8, f2)(j, P(desc));
        HC(0x218, f3)(P(cfg), 0, 0x3cc);
        for (k = 0; k < n; k++) {
            uint32_t v = M32(bind + k * 8);
            if (v == 0)
                continue;
            if (v >= 0x11 && v <= 0x50) {
                M32(P(cfg) + 0x88 + v * 4) = M32(pairs + k * 8 + 4);
                M32(P(cfg) + 0x188 + v * 4) = M32(ext + k * 0xc + 8);
            }
            if (v >= 1 && v <= 0x10) {
                M32(P(cfg) + 8) |= 1u << (v - 1);
                M32(P(cfg) + 8 + v * 4) = M32(pairs + k * 8 + 4);
                M32(P(cfg) + 0x48 + v * 4) = M32(ext + k * 0xc);
                M32(P(cfg) + 0x88 + v * 4) = M32(ext + k * 0xc + 4);
            }
        }
        joy_install(kind, j, P(cfg));
    }
}

/* .text+0x8af0 -- free a list */
static void free_list(uint32_t list)
{
    uint32_t i;

    for (i = 0; i < M32(list); i++)
        HC(0x214, f1)(M32(list + 4) + i * 8);
    HC(0x214, f1)(list + 4);
    M32(list) = 0;
    for (i = 0; i < M32(list + 8); i++) {
        HC(0x214, f1)(M32(list + 0xc) + i * 0x14 + 8);
        HC(0x214, f1)(M32(list + 0xc) + i * 0x14 + 0xc);
        HC(0x214, f1)(M32(list + 0xc) + i * 0x14 + 0x10);
    }
    HC(0x214, f1)(list + 0xc);
    M32(list + 8) = 0;
}

/* .text+0x8090 -- block +0x4 */
static uint32_t FS98_STDCALL dlg_none(uint32_t block)
{
    (void)block;
    return 0;
}

/* .text+0x8ba0 -- block +0x20: after the joystick control panel, whether
 * there are none now */
static uint32_t FS98_STDCALL dlg_refresh(uint32_t block)
{
    (void)block;
    V32(0x250) = HC(0x1c0, f0)() == 0;
    return 0;
}

/* .text+0x8bc0 -- block +0x2c: device `dev` of `list` back to its defaults
 * -- the keyboard's from the layout's section or the resource table, a
 * joystick's from its model's (Options.DLL does not call it) */
static uint32_t FS98_STDCALL dlg_reset(uint32_t block, uint32_t list, uint32_t dev)
{
    uint8_t cfg[0x3cc];
    uint32_t accel, e = M32(list + 0xc) + dev * 0x14;

    if (M32(e) == 0) {
        if (M32(block + 0x28) == 0) {
            if (section_read_layout(D + 0x408, (const char *)(uintptr_t)(D + 0x1a8), 4, &accel) == 0)
                HC(0x1e4, f2)(P(&accel), 0x67);
            fill_keys(M32(M32(list + 0xc) + dev * 0x14 + 0xc), accel, 5);
        } else {
            if (section_read_layout(D + 0x408, (const char *)(uintptr_t)(D + 0x1b8), 8, &accel) == 0)
                HC(0x1e4, f2)(P(&accel), 0x68);
            fill_keys(M32(M32(list + 0xc) + dev * 0x14 + 0xc), accel, 0xa);
        }
        return HC(0x1dc, f1)(P(&accel));
    }
    if (M32(e) == 1) {
        uint32_t kind = M32(block + 0x28) == 0 ? 0 : 1, r;
        r = joy_default(kind, M32(e + 4), P(cfg));
        if (r != 0)
            return r;
        e = M32(list + 0xc) + dev * 0x14;
        return fill_joy(list, M32(e + 0xc), M32(e + 0x10), P(cfg), kind == 0 ? 5 : 0xa);
    }
    return e;
}

/* .text+0x8d40 -- block +0x1c, the page's reset: mode 0 the keyboard's
 * sensitivities, 2 the mouse's, else joystick `joy`'s axes back to scale
 * 100 and null 36 in the list the page shows */
static uint32_t FS98_STDCALL dlg_adjust(uint32_t block, uint32_t mode, uint32_t joy)
{
    uint32_t list, k;

    if (mode == 0) {
        M16(block + 0x9c) = 0x40;
        M16(block + 0xac) = 0x40;
        M16(block + 0xbc) = 0x40;
        return block;
    }
    if (mode == 2) {
        M16(block + 0x64) = 0x23;
        M16(block + 0x74) = 0x19;
        M16(block + 0x84) = 0x40;
        return block;
    }
    list = M32(block + 0x28) == 0 ? block + 0x30 : block + 0x40;
    for (k = 0; k < M32(list); k++) {
        uint32_t e = M32(list + 0xc) + joy * 0x14;
        if (M32(M32(e + 0x20) + k * 8 + 4) == 2 && M32(M32(e + 0x20) + k * 8) != 0) {
            M32(M32(e + 0x24) + k * 0xc) = 0x64;
            M32(M32(M32(list + 0xc) + joy * 0x14 + 0x24) + k * 0xc + 4) = 0x24;
        }
    }
    return block;
}

/* .text+0x7e80 -- set the block up */
static uint32_t dlg_setup(void)
{
    uint32_t ff[7];

    HC(0x2a0, f3)(0x8010, D + 0x1d0, 0x32);                 /* the keyboard's name */
    HC(0x2a0, f3)(0x8011, D + 0x208, 0x32);                 /* the mouse's */
    if (HC(0x1c0, f0)() != 0) {
        V32(0x250) = 0;
        V32(0xb00) = controls_joystick(0, 1);
    } else {
        V32(0x250) = 1;
        V32(0xb00) = 0;
    }
    controls_force(1, P(ff));
    V32(0x310) = ff[2];
    V32(0x308) = ff[1];
    V32(0x328) = ff[5];
    V32(0x300) = ff[0] == 0;
    V32(0x320) = ff[4];
    V32(0x318) = ff[3];
    V32(0x330) = ff[6];
    build_list(D + 0x270, V32(0x186), 5);
    build_list(D + 0x280, V32(0x18a), 0xa);
    CALL(UTIL, 0x90, f3)(D + 0x338, 1, D + 0x240);
    records_init(D + 0x264);
    return 0;
}

/* .text+0x7fa0 */
static uint32_t dlg_cleanup(void)
{
    free_list(D + 0x270);
    free_list(D + 0x280);
    return 0;
}

/* .text+0x7fc0 -- OK: the joysticks' enable, force feedback, the
 * sensitivities, the key tables and the joystick configs */
static uint32_t dlg_apply(void)
{
    uint32_t ff[7], r;

    r = CALL(UTIL, 0x8c, f4)(D + 0x338, 1, D + 0x240, 0);
    controls_joystick(1, V32(0xb00) != 0);
    ff[1] = V32(0x308);
    ff[2] = V32(0x310);
    ff[3] = V32(0x318);
    ff[4] = V32(0x320);
    ff[5] = V32(0x328);
    ff[6] = V32(0x330);
    controls_force(2, P(ff));
    records_apply(D + 0x264);
    apply_keys(D + 0x270, V32(0x186));
    apply_keys(D + 0x280, V32(0x18a));
    apply_joys(D + 0x270, 0);
    apply_joys(D + 0x280, 1);
    return r;
}

/* .text+0x7e40 -- Linkage +0x58: the keyboard-assignments dialog (0x140a) */
uint32_t FS98_STDCALL controls_dialog(void)
{
    uint32_t r = dlg_setup();

    if (r == 0) {
        r = HC(0x10c, f2)(0x140a, D + 0x240);
        if (r == 0 && V32(0x240) == 1)
            dlg_apply();
    }
    dlg_cleanup();
    return r;
}

/* the ground bumps' period at `speed`: 0.5 - (-1e6 / (slope * speed +
 * intercept)), truncated, on the x87 */
static uint32_t ff_bump_period(double speed, uint32_t slope, uint32_t intercept)
{
    long double t;

    t = (long double)MD(D + slope) * (long double)speed;
    t = t + (long double)MD(D + intercept);
    t = (long double)RD(0x268) / t;
    t = (long double)RD(0x270) - t;
    return (uint32_t)ftol(t);
}

/* .text+0xa400 -- Linkage +0x5c: force feedback.  1 reads the settings into
 * blk (whether a device is there, then master, stick force, shaker, crash,
 * ground bumps, gear bumps), 2 sets them, saves them and brings the device
 * up or down.  With the device up and neither a crash nor a pause holding
 * it: 3 the stick force at (x, y) (two doubles at blk), 4 the shaker on (blk
 * nonzero) or off, 5..7 the nose, left and right gear bumps, 8 the crash on
 * (blk 1) or off (0), 9 the ground bumps at blk[2]'s top half's speed (blk[0]
 * zero, or a zero speed, stops them).  Returns what retail leaves in eax. */
uint32_t FS98_STDCALL controls_force(uint32_t op, uint32_t blk)
{
    uint32_t r;
    double speed;

    if (op == 1) {
        M32(blk + 0) = V32(0xbfec);
        M32(blk + 4) = V32(0x470);
        M32(blk + 8) = V32(0x474);
        M32(blk + 0xc) = V32(0x478);
        M32(blk + 0x10) = V32(0x480);
        M32(blk + 0x14) = V32(0x484);
        M32(blk + 0x18) = V32(0x47c);
    } else if (op == 2) {
        V32(0x470) = M32(blk + 4);
        V32(0x474) = M32(blk + 8);
        V32(0x478) = M32(blk + 0xc);
        V32(0x480) = M32(blk + 0x10);
        V32(0x484) = M32(blk + 0x14);
        V32(0x47c) = M32(blk + 0x18);
        ff_settings_save();
        if (V32(0xbfbc) == 0 && V32(0x470) != 0)
            ff_begin();
        else if (V32(0xbfbc) != 0 && V32(0x470) == 0)
            ff_end();
    }
    if ((r = V32(0xbfbc)) == 0 || (r = V32(0xbff0)) != 0 || (r = V32(0xbff4)) != 0)
        return r;
    r = op - 3;
    if (r > 6)
        return r;
    switch (op) {
    case 3:                                             /* the stick force */
        if (V32(0x474) != 0)
            return ff_stick(MD(blk), MD(blk + 8));
        return ff_stick(0.0, 0.0);
    case 4:                                             /* the stick shaker */
        r = V32(0x478);
        if (r != 0 && blk != 0) {
            r = V32(0xbfb4);
            if (r != 1) {
                r = eff_periodic(V32(0xbfc4), D + 0xbfcc, 0, V32(0x48c), V32(0xbff8), V32(0x490),
                                 0xffffffff, 0);
                V32(0xbfb4) = 1;
            }
        }
        if (blk != 0)
            return r;
        if ((r = V32(0xbfb4)) == 2)
            return r;
        r = eff_free(V32(0xbfc4), D + 0xbfcc);
        V32(0xbfb4) = 2;
        return r;
    case 5:                                             /* the gear bumps */
        if ((r = V32(0x47c)) == 0)
            return r;
        return eff_bump(V32(0xbfc4), D + 0xbfd0, V32(0x494), V32(0x498), V32(0x49c));
    case 6:
        if ((r = V32(0x47c)) == 0)
            return r;
        return eff_bump(V32(0xbfc4), D + 0xbfd4, V32(0x4a0), V32(0x4a4), V32(0x4a8));
    case 7:
        if ((r = V32(0x47c)) == 0)
            return r;
        return eff_bump(V32(0xbfc4), D + 0xbfd8, V32(0x4ac), V32(0x4b0), V32(0x4b4));
    case 8:                                             /* the crash */
        r = V32(0x480);
        if (r != 0 && blk == 1) {
            ff_stop_all();
            r = eff_constant(V32(0xbfc4), D + 0xbfe4, V32(0x4d8), V32(0x4dc), V32(0x4e0));
            V32(0xbf98) = 1;
        }
        if (blk != 0)
            return r;
        if ((r = V32(0xbf98)) == 3)
            return r;
        eff_free(V32(0xbfc4), D + 0xbfe4);
        r = eff_free(V32(0xbfc4), D + 0xbfe8);
        V32(0xbf98) = 3;
        V32(0xbff0) = 1;
        return r;
    case 9:                                             /* the ground bumps */
        if ((r = V32(0x484)) == 0)
            return r;
        if (M32(blk) == 0 || (M32(blk + 8) >> 16) == 0) {
            if ((r = V32(0xbfb0)) == 2)
                return r;
            eff_free(V32(0xbfc4), D + 0xbfdc);
            r = eff_free(V32(0xbfc4), D + 0xbfe0);
            V32(0xbfb0) = 2;
            return r;
        }
        speed = (double)(int64_t)(M32(blk + 8) >> 16);
        if (V32(0xbfb0) != 1) {
            eff_periodic(V32(0xbfc4), D + 0xbfdc, 1, V32(0x4b8), V32(0x4bc),
                         ff_bump_period(speed, 0x4c0, 0xc000), 0xffffffff, 0);
            r = eff_periodic(V32(0xbfc4), D + 0xbfe0, 1, V32(0x4c8), V32(0x4cc),
                             ff_bump_period(speed, 0x4d0, 0xc008), 0xffffffff, 0);
            V32(0xbfb0) = 1;
            return r;
        }
        eff_periodic_set(V32(0xbfc4), D + 0xbfdc, V32(0x4b8), V32(0x4bc),
                         ff_bump_period(speed, 0x4c0, 0xc000));
        return eff_periodic_set(V32(0xbfc4), D + 0xbfe0, V32(0x4c8), V32(0x4cc),
                                ff_bump_period(speed, 0x4d0, 0xc008));
    }
    return r;
}

/* install what controls.c's DllMain needs from this half: the code pointers
 * in the dialog's block */
void controls_input_install(void)
{
    V32(0x244) = P(dlg_none);                               /* the dialog's callbacks */
    V32(0x25c) = P(dlg_adjust);
    V32(0x260) = P(dlg_refresh);
    V32(0x26c) = P(dlg_reset);
}
