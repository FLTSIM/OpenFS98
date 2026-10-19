/* CONTROLS.DLL -- the compiled-C half, .text+0x78d0..+0xbd1x.
 *
 * Unlike the hand-written half (controls.c) this is ordinary MSVC-compiled C.
 * It is the configuration, dialog and joystick side of CONTROLS:
 *
 *   +0x4c  controls_keys_load   read the key bindings from FLTSIM98.CFG's
 *                               [KEYBOARD_MAIN]/[KEYBOARD_SLEW] into two host
 *                               accelerator tables, install them, start joysticks
 *   +0x50  controls_keys_save   write them back, release the joysticks
 *   +0x54  controls_keys_list   fill a caller block with a command's key
 *   +0x58  controls_dialog      run the keyboard-assignments dialog (0x140a)
 *   +0x5c  controls_force       the force-feedback dispatch: build and play
 *                               DirectInput 5 effects from the flight model
 *
 * and four entries the assembly half calls:
 *
 *   +0x92c0 controls_input_init    register the joystick frame poll (CHAIN list 1)
 *   +0x9340 controls_input_deinit
 *   +0x9270 controls_joystick      enable/report the joysticks (JOYSTICK_CALIBRATE)
 *   +0x9290 controls_accel_menu    switch the slew/flight accelerator table
 *
 * STATUS: NOT YET TRANSLITERATED.  The assembly half (controls.c) is complete
 * and compiles; this half is the remaining work.  It is 0x78d0..0xbd1x of the
 * module -- the profile read/write, the dialog's key-list model, joystick
 * enumeration and polling, and the DirectInput force-feedback COM code.  It is
 * all compiler-generated C with reused, optimiser-shuffled stack slots, so it
 * must be transliterated instruction-faithfully against the listing and
 * verified in controlsdiff, not paraphrased.  Until then these are honest
 * placeholders so the assembly half links and can be driven on its own: they
 * keep the two state words the assembly reads back (D+0xbf8c the joysticks'
 * reported state, through controls_joystick) and otherwise do nothing.
 *
 * See docs/modules/controls.md for the plan and the host services involved.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "controls.h"

#define V32(o) M32(D + (o))

/* ---- the five Linkage entries (host-called) -- pending transliteration ---------- */

/* .text+0x78d0 -- Linkage +0x4c */
uint32_t FS98_STDCALL controls_keys_load(void)
{
    return 0;
}

/* .text+0x7bd0 -- Linkage +0x50 */
uint32_t FS98_STDCALL controls_keys_save(void)
{
    return 0;
}

/* .text+0x7d60 -- Linkage +0x54 */
uint32_t FS98_STDCALL controls_keys_list(uint32_t command, uint32_t out)
{
    (void)command;
    (void)out;
    return 0;
}

/* .text+0x7e40 -- Linkage +0x58 */
uint32_t FS98_STDCALL controls_dialog(void)
{
    return 0;
}

/* .text+0xa400 -- Linkage +0x5c: the force-feedback dispatch */
uint32_t FS98_STDCALL controls_force(uint32_t op, uint32_t param)
{
    (void)op;
    (void)param;
    return 0;
}

/* ---- the four entries the assembly half calls -- pending transliteration -------- */

/* .text+0x92c0 */
void controls_input_init(void)
{
}

/* .text+0x9340 */
void controls_input_deinit(void)
{
}

/* .text+0x9270 -- JOYSTICK_CALIBRATE's enable/report: with `set` nonzero,
 * store `on` as the joysticks' state (D+0xbf8c); either way return that
 * state.  The assembly reads the return (and, when set, the ecx the real
 * build's reload leaves; see docs/modules/controls.md -- not reproduced by
 * this placeholder). */
uint32_t FS98_STDCALL controls_joystick(uint32_t set, uint32_t on)
{
    if (set)
        V32(0xbf8c) = on;
    return V32(0xbf8c);
}

/* .text+0x9290 -- switch the accelerator table D+0x350 points at between the
 * flight (D+0xd48) and slew (D+0x82c8) key tables. */
void FS98_STDCALL controls_accel_menu(uint32_t slew)
{
    V32(0x350) = slew ? (D + 0x82c8) : (D + 0xd48);
}

/* install anything controls.c's DllMain needs from this half (nothing yet) */
void controls_input_install(void)
{
}
