/* CONTROLS.DLL -- module id 16: the keyboard, the joysticks and the mouse.
 *
 * Every key, button and axis ends as a command: an id from 0x10000 with a
 * value, dispatched through a table of 383 handlers (CHAIN list 0xb, or
 * +0x44 directly).  The handlers move GLOBAL: the throttles, mixtures and
 * propellers of four engines, the elevator, ailerons, rudder and their trim,
 * brakes, flaps, the radios digit by digit, the autopilot's holds and
 * targets, the clock, the eye in the active view, and the sim's toggles.
 * Most of what changes is also handed to DEMO +0x24, as a pilot event, while
 * a video records.  CHAIN list 2 steps the brakes back off, list 3 centres
 * the ailerons when "auto-coordination" is on.
 *
 * Retail is two programs.  Hand-written assembly runs from .text+0x20 to
 * +0x78c1, with ATC's stack-alignment wrapper around every import call (no
 * observable effect, so not reproduced).  It keeps the command table among
 * its code at .text+0x15f9 (tools/gen_controls_data.py extracts it).  MSVC C
 * follows from +0x78d0: the key and joystick bindings, read from and written
 * to FLTSIM98.CFG through the host's profile services, the joysticks through
 * the host's winmm services, and force feedback through DirectInput 5,
 * which it loads itself.  That half is controls_input.c.
 *
 * The whole 0xc020-byte .data image is kept at retail's offsets.
 *
 * The assembly runs on a model of the registers (controls.h): its routines
 * pass values in them and leave values in them that other modules read.
 *
 * docs/modules/controls.md has the details.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "controls.h"

struct controls_regs controls_regs;
uint32_t controls_xcall_fn;    /* the pending import target (controls_xcall) */

/* ---- the thunks ------------------------------------------------------------- */

/* controls_xcall(fn, n, args): the model's registers in, the callee's out. */
__asm__(".text\n\t.p2align 4\n"
        ".globl _controls_xcall\n"
        "_controls_xcall:\n\t"
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl 8(%ebp), %eax\n\t"
        "movl %eax, _controls_xcall_fn\n\t"
        "movl 12(%ebp), %ecx\n\t"
        "movl 16(%ebp), %edx\n"
        "1:\n\t"
        "testl %ecx, %ecx\n\t"
        "jz 2f\n\t"
        "pushl -4(%edx,%ecx,4)\n\t"
        "decl %ecx\n\t"
        "jmp 1b\n"
        "2:\n\t"
        "movl _controls_regs, %eax\n\t"
        "movl _controls_regs+4, %ecx\n\t"
        "movl _controls_regs+8, %edx\n\t"
        "movl _controls_regs+12, %ebx\n\t"
        "movl _controls_regs+16, %esi\n\t"
        "movl _controls_regs+20, %edi\n\t"
        "call *_controls_xcall_fn\n\t"
        "movl %eax, _controls_regs\n\t"
        "movl %ecx, _controls_regs+4\n\t"
        "movl %edx, _controls_regs+8\n\t"
        "movl %ebx, _controls_regs+12\n\t"
        "movl %esi, _controls_regs+16\n\t"
        "movl %edi, _controls_regs+20\n\t"
        "leal -12(%ebp), %esp\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "ret\n");

/* An entry into the assembly from outside: the caller's registers into the
 * model, `body` (cdecl, the same arguments), and the model's eax, ecx and
 * edx back out; stdcall.  Every retail entry keeps ebx, esi, edi and ebp,
 * and so does every body (it restores the model's), so the machine's are
 * the C body's to keep. */
#define CT_ENTRY(name, body, n, k) \
    __asm__(".text\n\t.p2align 4\n" \
            ".globl _" #name "\n" \
            "_" #name ":\n\t" \
            "movl %eax, _controls_regs\n\t" \
            "movl %ecx, _controls_regs+4\n\t" \
            "movl %edx, _controls_regs+8\n\t" \
            "movl %ebx, _controls_regs+12\n\t" \
            "movl %esi, _controls_regs+16\n\t" \
            "movl %edi, _controls_regs+20\n\t" \
            ".rept " #n "\n\t" \
            "pushl " #k "(%esp)\n\t" \
            ".endr\n\t" \
            "call _" #body "\n\t" \
            "addl $4*" #n ", %esp\n\t" \
            "movl _controls_regs, %eax\n\t" \
            "movl _controls_regs+4, %ecx\n\t" \
            "movl _controls_regs+8, %edx\n\t" \
            "ret $4*" #n "\n"); \
    extern void name(void)

/* k is 4*n: the last argument, pushed first, and every one after it lands
 * there too */
#define ENTRY0(name, body) CT_ENTRY(name, body, 0, 0)
#define ENTRY1(name, body) CT_ENTRY(name, body, 1, 4)
#define ENTRY2(name, body) CT_ENTRY(name, body, 2, 8)
#define ENTRY3(name, body) CT_ENTRY(name, body, 3, 12)

#define SAVE_REGS    uint32_t saved_ebx = ebx, saved_esi = esi, saved_edi = edi
#define RESTORE_REGS (ebx = saved_ebx, esi = saved_esi, edi = saved_edi)

/* `sub esp,2 / push word` with ATC's wrapper just above it: the dword's top
 * half is two bytes of the wrapper's 0x04050607, written below an aligned
 * esp. */
#define WRAPPED_WORD(v) (0x05060000u | (uint16_t)(v))

/* ---- the state, at retail's offsets ------------------------------------------- */

#define V8(o)  M8(D + (o))
#define V16(o) M16(D + (o))
#define V32(o) M32(D + (o))

#define S_VALUE     0x0e0      /* dword: the command's value */
#define S_SELECTED  0x0e4      /* dword: the next SELECT_n replaces the engines */
#define S_REPEAT    0x0e8      /* byte: frames since the key last repeated */
#define S_SELECT    0x0ea      /* byte: SELECT_n is live while nonzero */
#define S_EYE_MOVED 0x0eb      /* dword: the eye moved; the next view key resets */
#define S_ELEV_AXIS 0x0ef      /* words: +0x18's six, the three axes' curves */
#define S_PAN_RATE  0x10c      /* dword: from FLTSIM98.CFG [CONTROLS] PAN_RATE */
#define S_FRAMES    0x110      /* byte: frames to the next brake step */
#define S_BRAKES    0x111      /* word */
#define S_STEP      0x113      /* word: the throttle step, 0x3fff / value */
#define S_YOKE      0x115      /* dword: the mouse is the yoke */
#define S_NO_YOKE   0x119      /* dword */
#define S_LAST_X    0x11d      /* dwords: the last axis values seen */
#define S_LAST_Y    0x121
#define S_LAST_Z    0x125
#define S_CYCLE     0x129      /* dword: an index into the eight-byte records */
#define S_KEYING    0x185      /* byte: the next command is a key to bind */
#define S_ACCEL     0x18a      /* dword: the accelerator table's handle */
#define S_FLIGHT    0x18e      /* MAIN +0x84's record */
#define S_SECTION   0x192      /* "CONTROLS" */
#define S_PAN_KEY   0x19b      /* "PAN_RATE" */
#define S_HALF      0x1a4      /* word: the last value of 0x10118's axis */

#define GW(o)  M16(edi + (o))      /* GLOBAL, through edi, as retail has it */
#define GB(o)  M8(edi + (o))
#define GD(o)  M32(edi + (o))

static void pilot_event(uint32_t which, uint32_t value);

/* the C half (controls_input.c) */
void controls_input_init(void);                     /* .text+0x92c0 */
void controls_input_deinit(void);                   /* .text+0x9340 */
uint32_t FS98_STDCALL controls_joystick(uint32_t set, uint32_t on);     /* .text+0x9270 */
void FS98_STDCALL controls_accel_menu(uint32_t slew);              /* .text+0x9290 */

/* ======== the assembly: .text+0x20..+0x78c1 ======================================= */

static void brakes_step(void);              /* .text+0x25c */
static void coordinate(void);               /* .text+0x6fdf */
static void command(uint32_t id, uint32_t value);   /* .text+0x3406 */
static void dispatch(void);                 /* .text+0x3480 */
static void set_brakes(void);               /* .text+0x6659 */
static void set_elevator(void);             /* .text+0x60fe */
static void set_aileron(void);              /* .text+0x62a6 */
static void set_rudder(void);               /* .text+0x6335 */

/* .text+0x20 -- Linkage +0x04 */
void controls_init_body(void);
void cb_command(uint32_t id, uint32_t record);
void cb_frame(uint32_t a, uint32_t b);
void cb_frame3(uint32_t a, uint32_t b);
ENTRY2(cb_command_entry, cb_command);
ENTRY2(cb_frame_entry, cb_frame);
ENTRY2(cb_frame3_entry, cb_frame3);

void controls_init_body(void)
{
    SAVE_REGS;

    XCALL(SVC(CHAIN, 0x18), 0xb, ADDR(cb_command_entry));
    XCALL(SVC(CHAIN, 0x18), 0x2, ADDR(cb_frame_entry));
    XCALL(SVC(CHAIN, 0x18), 0x3, ADDR(cb_frame3_entry));
    /* host +0xa8: [CONTROLS] PAN_RATE into the dword at +0x10c */
    XCALL(SVC(HOST, 0xa8), D + S_SECTION, D + S_PAN_KEY, D + S_PAN_RATE);
    controls_input_init();
    RESTORE_REGS;
}

/* .text+0xdb -- Linkage +0x08 */
void controls_deinit_body(void)
{
    SAVE_REGS;

    XCALL(SVC(HOST, 0xac), D + S_SECTION, D + S_PAN_KEY, V32(S_PAN_RATE));
    XCALL(SVC(CHAIN, 0x1c), 0xb, ADDR(cb_command_entry));
    XCALL(SVC(CHAIN, 0x1c), 0x2, ADDR(cb_frame_entry));
    XCALL(SVC(CHAIN, 0x1c), 0x3, ADDR(cb_frame3_entry));
    controls_input_deinit();
    RESTORE_REGS;
}

/* .text+0x197 -- CHAIN list 2, every frame: the repeat count runs down, and
 * every sixth frame the brakes step back. */
void cb_frame(uint32_t a, uint32_t b)
{
    SAVE_REGS;

    eax = a;
    eax = b;
    if (V8(S_REPEAT) != 0)
        V8(S_REPEAT)--;
    if (--V8(S_FRAMES) == 0) {
        V8(S_FRAMES) = 6;
        brakes_step();
    }
    RESTORE_REGS;
}

/* .text+0x1cd -- CHAIN list 3 */
void cb_frame3(uint32_t a, uint32_t b)
{
    SAVE_REGS;

    eax = a;
    eax = b;
    coordinate();
    RESTORE_REGS;
}

/* .text+0x1e5 -- CHAIN list 0xb: a command, (id, record) with the value at
 * record +8. */
void cb_command(uint32_t id, uint32_t record)
{
    SAVE_REGS;

    eax = id;
    if (eax >= 0x10000 && eax <= 0x1dfff) {
        ebx = record;
        command(id, M32(record + 8));
    }
    RESTORE_REGS;
}

/* .text+0x211 -- Linkage +0x18: the three axes' curves, six words. */
static void set_curves(uint32_t p)
{
    SAVE_REGS;

    ebx = p;
    SET_AX(M16(ebx));
    V16(0xf1) = AX;
    SET_AX(M16(ebx + 2));
    V16(0xef) = AX;
    SET_AX(M16(ebx + 4));
    V16(0xf5) = AX;
    SET_AX(M16(ebx + 6));
    V16(0xf3) = AX;
    SET_AX(M16(ebx + 8));
    V16(0xf9) = AX;
    SET_AX(M16(ebx + 0xa));
    V16(0xf7) = AX;
    RESTORE_REGS;
}

/* .text+0x25c -- every sixth frame: the brakes come off by 0x32 a side,
 * the parking brake's left in +0xbc8 if it is set. */
static void brakes_step(void)
{
    uint8_t b;

    edi = G;
    SET_AX(GW(0xc00));
    if (AX != 0) {
        if (GW(0xbc8) != 0)
            SET_AX(GW(0xbc8));
        b = AH - 0x32;
        SET_AH(AH < 0x32 ? 0 : b);
        b = AL - 0x32;
        SET_AL(AL < 0x32 ? 0 : b);
        GW(0xbca) = AX;
        GW(0xc00) = AX;
        SET_CH(AH);
        SET_AX((uint16_t)(AL * V8(0x10b)));
        GW(0xbc6) = AX;
        SET_AL(CH);
        SET_AX((uint16_t)(AL * V8(0x10b)));
        GW(0xbc4) = AX;
    }
    if (GW(0xbc8) == 0) {
        SET_AX(V16(S_BRAKES));
        if ((int16_t)AX > (int16_t)0xfe0c)
            SET_AX(AX - 0x32);
        else if ((int16_t)AX < (int16_t)0xfda8)
            SET_AX(AX + 0x32);
        else
            return;
        set_brakes();
    }
}

/* .text+0x33e5 -- Linkage +0x48: tell DEMO the radios. */
static void tune_com(void);         /* .text+0x3dab */
static void tune_nav1(void);        /* .text+0x3eb5 */
static void tune_nav2(void);        /* .text+0x3f03 */
static void tune_adf(void);         /* .text+0x6805 */

void report_radios(void)
{
    SAVE_REGS;

    edi = G;
    tune_com();
    tune_nav1();
    tune_nav2();
    tune_adf();
    RESTORE_REGS;
}

/* .text+0x3406 -- a command from CHAIN list 0xb: CHAIN list 0xc sees it
 * first and may take it (a nonzero dword); otherwise it is dispatched, and
 * one of kind 0 (mod 4) ends SELECT_n's window early. */
static void command(uint32_t id, uint32_t value)
{
    uint32_t taken = 0, keep_edi = edi;

    edi = G;
    XCALL(SVC(CHAIN, 0x20), 0xc, id, ADDR(&taken));
    if (taken == 0) {
        eax = id;
        ecx = value;
        dispatch();
        if ((eax & 3) == 0)
            V8(S_SELECT) >>= 1;
    }
    edi = keep_edi;
}

/* .text+0x3468 -- Linkage +0x44: dispatch (id, value). */
void dispatch_entry_body(uint32_t id, uint32_t value)
{
    SAVE_REGS;

    eax = id;
    ecx = value;
    dispatch();
    RESTORE_REGS;
}

/* .text+0x7101 -- in key-binding mode three axis commands centre their
 * axis instead; anything else ends the mode.  eax is kept. */
static void keying(void)
{
    uint32_t keep = eax;

    if (eax == 0x12) {
        eax = 0;
        set_elevator();
    } else if (eax == 0x13) {
        eax = 0;
        set_rudder();
    } else if (eax == 0x1e) {
        eax = 0;
        set_aileron();
    } else {
        V8(S_KEYING) = 0;
    }
    eax = keep;
}

/* .text+0x3480 -- dispatch command eax with value ecx.  Ids from 0x10000
 * index the table less 0x10000; below that the id is the index.  After a
 * crash only the eleven listed get through.  The handler is called with
 * edi = GLOBAL, eax the record, ecx the value, and its kind pushed; eax is
 * what comes back off the stack, the kind. */
static void dispatch(void)
{
    uint32_t keep_edi = edi, kind;
    struct controls_command *rec;

    V32(S_VALUE) = ecx;
    if (eax >= 0x10000) {
        edi = G;
        if (GB(0x838) != 0 &&
            eax != 0x100c4 && eax != 0x100c5 && eax != 0x1008a &&
            eax != 0x1008b && eax != 0x1008c && eax != 0x1008d &&
            eax != 0x1008e && eax != 0x1008f && eax != 0x10090 &&
            eax != 0x10091 && eax != 0x10092)
            goto out;
        eax -= 0x10000;
    }
    if (eax >= controls_count)
        goto out;
    edi = G;
    if (V8(S_KEYING) != 0) {
        keying();
        if (V8(S_KEYING) != 0) {
            V8(S_KEYING) = 0;
            goto out;
        }
    }
    rec = &controls_table[eax];
    eax = ADDR(rec);
    kind = rec->kind;
    rec->handler();
    eax = kind;
out:
    edi = keep_edi;
}

/* ---- the handlers: .text+0x3530.. ------------------------------------------------ */

/* .text+0x3530 -- nothing, but eax is the +/- mode */
static void h_3530(void)
{
    eax = GB(0x288);
}

/* .text+0x3539 */
static void h_3539(void)
{
}

/* .text+0x3569 -- SELECT_n (cl '1'..'4') for what +/- is on. */
static void select_com_nav(void)            /* .text+0x35a0 */
{
    if (CL == 0x31)
        GB(0x375) = 1;
    else if (CL == 0x32)
        GB(0x375) = 2;
}

static void select_obs(void)                /* .text+0x35bb */
{
    if (CL == 0x31)
        GB(0x376) = 1;
    else if (CL == 0x32)
        GB(0x376) = 2;
}

/* .text+0x35d6 -- engines: the bit for cl, held to the aircraft's count at
 * +0xaec, added to +0x888 (or replacing it, the first time after ENGINE). */
static void select_engine(void)
{
    uint32_t cf, out;

    SET_CL((CL - 0x31) & 3);
    SET_AX(GW(0xaec));
    if (CL >= AL) {
        SET_CL(AL);
        SET_CL(CL - 1);
    }
    eax = 0;
    cf = 1;
    do {
        out = eax >> 31;
        eax = (eax << 1) | cf;
        cf = out;
        SET_CL(CL - 1);
    } while (!(CL & 0x80));
    cf = V32(S_SELECTED) & 1;
    V32(S_SELECTED) >>= 1;
    if (cf)
        GD(0x888) = 0;
    GD(0x888) |= eax;
    V8(S_SELECT) = 3;
}

static void select_dme(void)                /* .text+0x3614 */
{
    if (CL == 0x31)
        GD(0x378) = 1;
    else if (CL == 0x32)
        GD(0x378) = 2;
}

static void select_apply(void)
{
    SET_AL(GB(0x288));
    if (AL == 6 || AL == 7)
        select_com_nav();
    else if (AL == 0xc)
        select_obs();
    else if (AL == 0xd)
        select_engine();
    else if (AL == 0xe)
        select_dme();
}

static void select_n(void)                  /* .text+0x355a */
{
    if (V8(S_SELECT) != 0)
        select_apply();
}

static void h_353a(void) { SET_CL(0x31); select_n(); }
static void h_3542(void) { SET_CL(0x32); select_n(); }
static void h_354a(void) { SET_CL(0x33); select_n(); }
static void h_3552(void) { SET_CL(0x34); select_n(); }

/* .text+0x3786, +0x37c1 -- the map's zoom at +0x26 of a view, by n or to
 * n, held to 0..0xa00, and recorded */
static void map_zoom_by(uint32_t view, uint32_t n)
{
    uint16_t keep;

    SET_AX(n);
    ecx = view;
    M16(ecx + 0x26) += AX;
    if ((int16_t)M16(ecx + 0x26) > 0xa00)
        M16(ecx + 0x26) = 0xa00;
    if ((int16_t)M16(ecx + 0x26) < 0)
        M16(ecx + 0x26) = 0;
    keep = M16(ecx + 0x26);
    pilot_event(0xc, keep);
}

static void map_zoom_to(uint32_t view, uint32_t n)
{
    uint16_t keep;

    SET_AX(n);
    ecx = view;
    M16(ecx + 0x26) = AX;
    if ((int16_t)M16(ecx + 0x26) > 0xa00)
        M16(ecx + 0x26) = 0xa00;
    if ((int16_t)M16(ecx + 0x26) < 0)
        M16(ecx + 0x26) = 0;
    keep = M16(ecx + 0x26);
    pilot_event(0xc, keep);
}

/* the active view's zoom, for the 3D views (+0x10 not 1) and the map */
static void zoom_halve(void);               /* .text+0x6d39 */
static void zoom_double(void);              /* .text+0x6d5d */
static void zoom_out_fine(void);            /* .text+0x6ce8 */
static void zoom_in_fine(void);             /* .text+0x6d06 */
static void zoom_set(void);                 /* .text+0x6d1a */

static void h_371f(void)                    /* ZOOM_MINUS */
{
    GB(0x370) = 1;
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x10) != 1)
        zoom_halve();
    else
        map_zoom_by(ecx, 0x80);
}

static void h_3761(void)                    /* MAP_ZOOM_FINE_OUT */
{
    GB(0x370) = 1;
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x10) != 1)
        zoom_out_fine();
    else
        map_zoom_by(ecx, 0x20);
}

static void j_3747(void) { GB(0x288) = 2; h_371f(); }
static void j_3754(void) { GB(0x288) = 2; h_3761(); }

/* the radios: the COM's whole and fraction digits by one (.text+0x3bfa,
 * +0x3bdd), its fraction by a step (+0x3b43, +0x3b8e), and the NAVs' by
 * one (+0x3b75, +0x3bc4); ebx the BCD word, cl/ch the bounds */
static void bcd_inc_nav(void);
static void bcd_dec_nav(void);

static void bcd_inc_step(void)              /* .text+0x3b43 */
{
    SET_AL(M8(ebx));
    if (AL == 0x97) {
        SET_AL(0);
    } else {
        SET_AH(AL & 0xf);
        if (AH == 7) {
            SET_AL(AL + 9);
        } else if (AH == 2 || GD(0x358) != 0) {
            SET_AL(AL + 2);
            if (AH == 2)
                SET_AL(AL + 1);
        } else {
            bcd_inc_nav();
            return;
        }
    }
    M8(ebx) = AL;
}

static void bcd_inc_nav(void)               /* .text+0x3b75 */
{
    SET_AL(M8(ebx));
    if (AL == 0x95) {
        SET_AL(0);
    } else {
        SET_AL(AL + 2);
        if (AL & 1)
            SET_AL(AL + 9);
        else
            SET_AL(AL + 3);
    }
    M8(ebx) = AL;
}

static void bcd_dec_step(void)              /* .text+0x3b8e */
{
    SET_AL(M8(ebx));
    SET_AH(AL & 0xf);
    if (AH != 7 && AH != 2 && GD(0x358) == 0) {
        bcd_dec_nav();
        return;
    }
    if (AL == 0)
        SET_AL(0x97);
    else if (AH == 0)
        SET_AL(AL - 9);
    else {
        SET_AL(AL - 2);
        if (AH == 5)
            SET_AL(AL - 1);
    }
    M8(ebx) = AL;
}

static void bcd_dec_nav(void)               /* .text+0x3bc4 */
{
    SET_AL(M8(ebx));
    if (AL == 0) {
        SET_AL(0x95);
    } else {
        SET_AL(AL - 2);
        if (AL & 1)
            SET_AL(AL - 3);
        else
            SET_AL(AL - 9);
    }
    M8(ebx) = AL;
}

static void bcd_whole_inc(void)             /* .text+0x3bdd */
{
    SET_AL(M8(ebx + 1) + 1);
    if (AL > CH) {
        SET_AL(CL);
    } else {
        SET_AH(AL & 0xf);
        if (AH > 9)
            SET_AL(AL + 6);
    }
    M8(ebx + 1) = AL;
}

static void bcd_whole_dec(void)             /* .text+0x3bfa */
{
    SET_AL(M8(ebx + 1));
    if ((AL & 0xf) == 0) {
        SET_AL(AL - 7);
    } else {
        SET_AL(AL - 1);
        if (AL < CL)
            SET_AL(CH);
    }
    M8(ebx + 1) = AL;
}

/* COM (+0x34e), only with a COM radio (+0xb04 bit 3) and not failed */
static void h_37fc(void)                    /* COM_RADIO_WHOLE_DEC */
{
    if ((GW(0xb04) & 8) && GB(0xb68) == 0) {
        ebx = edi + 0x34e;
        SET_CX(0x3618);
        bcd_whole_dec();
        tune_com();
        GB(0x288) = 4;
    }
}

static void h_382c(void)                    /* COM_RADIO_FRACT_DEC */
{
    if ((GW(0xb04) & 8) && GB(0xb68) == 0) {
        ebx = edi + 0x34e;
        bcd_dec_step();
        tune_com();
        GB(0x288) = 5;
    }
}

/* NAV1 (+0x350) and NAV2 (+0x352), not failed (+0xb70) */
static void h_3867(void)                    /* NAV1_RADIO_WHOLE_DEC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x350;
        SET_CX(0x1708);
        bcd_whole_dec();
        tune_nav1();
        GB(0x288) = 6;
        GB(0x375) = 1;
    }
}

static void h_3893(void)                    /* NAV2_RADIO_WHOLE_DEC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x352;
        SET_CX(0x1708);
        bcd_whole_dec();
        tune_nav2();
        GB(0x288) = 6;
        GB(0x375) = 2;
    }
}

static void j_3858(void)
{
    if (GB(0x375) == 2)
        h_3893();
    else
        h_3867();
}

static void h_38d5(void)                    /* NAV1_RADIO_FRACT_DEC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x350;
        bcd_dec_nav();
        tune_nav1();
        GB(0x288) = 7;
        GB(0x375) = 1;
    }
}

static void h_38fd(void)                    /* NAV2_RADIO_FRACT_DEC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x352;
        bcd_dec_nav();
        tune_nav2();
        GB(0x288) = 7;
        GB(0x375) = 2;
    }
}

static void j_38bf(void)
{
    if (GB(0x375) == 2)
        h_38fd();
    else
        h_38d5();
}

/* the transponder, +0x354: a digit by one, each nibble held below 8 */
static void squawk(void)                    /* .text+0x3969 */
{
    if (GB(0xb6f) == 0) {
        SET_AX(AX & 0x7777);
        GW(0x354) = AX;
        pilot_event(0xb, GW(0x354));
    }
}

static void j_3925(void) { SET_AX(GW(0x354) + 0x7000); squawk(); }
static void j_3936(void) { SET_AX(GW(0x354) + 0x700); squawk(); }
static void j_3947(void) { SET_AX(GW(0x354) + 0x70); squawk(); }
static void j_3958(void) { SET_AX(GW(0x354) + 0x7); squawk(); }

/* the OBSs, +0xc4e and +0xc5e in degrees; ten at a time while the key
 * repeats, unless +0x328 bit 3 asks for single steps */
static void obs1_event(void);               /* .text+0x6dce */
static void obs2_event(void);               /* .text+0x6de0 */

static void obs_dec(uint32_t at, uint32_t which, int fast)
{
    GB(0x288) = 0xc;
    GB(0x376) = (uint8_t)which;
    if (!(GD(0x328) & 8)) {
        if (fast) {
            uint8_t was = V8(S_REPEAT);
            V8(S_REPEAT) = 3;
            if (was != 0)
                GW(at) -= 9;
        } else {
            GW(at) -= 9;
        }
    }
    GW(at) -= 1;
    if ((int16_t)GW(at) < 0)
        GW(at) += 0x168;
    if (which == 1)
        obs1_event();
    else
        obs2_event();
}

static void h_39bb(void) { obs_dec(0xc4e, 1, 1); }   /* VOR1_OBI_DEC */
static void j_3a06(void) { obs_dec(0xc4e, 1, 0); }
static void h_3a41(void) { obs_dec(0xc5e, 2, 1); }   /* VOR2_OBI_DEC */
static void j_3a8c(void) { obs_dec(0xc5e, 2, 0); }

static void j_398f(void)
{
    if (GB(0x376) == 2)
        h_3a41();
    else
        h_39bb();
}

static void j_39a5(void)
{
    if (GB(0x376) == 2)
        j_3a8c();
    else
        j_3a06();
}

/* the DMEs: +0x378 which, +0xc41/+0xc42 nautical miles or knots */
static void h_3add(void)                    /* DME1_TOGGLE */
{
    GB(0x288) = 0xe;
    GD(0x378) = 1;
    GB(0xc41) ^= 1;
}

static void h_3af6(void)                    /* DME2_TOGGLE */
{
    GB(0x288) = 0xe;
    GD(0x378) = 2;
    GB(0xc42) ^= 1;
}

static void j_3ac7(void)
{
    if (GD(0x378) == 2)
        h_3af6();
    else
        h_3add();
}

/* +0x8b6 down a step, not below 0 */
static void j_3b0f(void)
{
    GW(0x8b6) -= 0x100;
    if ((int16_t)GW(0x8b6) < 0)
        GW(0x8b6) = 0;
}

/* .text+0x3b24 -- SIM_RATE_DECR: +0xc1a halved, not below 0x40 */
static void h_3b24(void)
{
    GB(0x288) = 0x14;
    SET_AX(GW(0xc1a));
    if (AX > 0x40) {
        SET_AX(AX >> 1);
        GW(0xc1a) = AX;
    }
}

/* MINUS, MINUS_SHIFT, PLUS and PLUS_SHIFT act on what +0x288 says the last
 * radio, view or setting key picked: .text+0x3637, +0x36ab, +0x3c11 and
 * +0x3c85, 21 entries each (eax is the index when they run). */
static void h_3cf9(void);
static void j_3d17(void);
static void j_3d24(void);
static void h_3d4f(void);
static void h_3d7f(void);
static void j_3e09(void);
static void j_3e77(void);
static void h_3f31(void);
static void h_3f49(void);
static void h_3f61(void);
static void h_3f79(void);
static void j_3fa3(void);
static void j_3fb9(void);
static void h_6745(void);
static void h_677f(void);
static void h_6825(void);
static void h_6849(void);
static void j_410d(void);
static void h_4122(void);
static void h_673b(void);
static void h_6898(void);
static void h_6903(void);
static void h_6925(void);
static void h_3761(void);
static void h_3d31(void);

typedef void (*handler)(void);

static const handler minus_by_mode[21] = {
    h_3530, h_3530, h_371f, h_673b, h_37fc, h_382c, j_3858, j_38bf, j_3925,
    j_3936, j_3947, j_3958, j_398f, j_3747, j_3ac7, h_6898, h_6903, h_6925,
    h_3530, j_3b0f, h_3b24,
};
static const handler minus_shift_by_mode[21] = {
    h_3530, h_3530, h_3761, h_673b, h_37fc, h_382c, j_3858, j_38bf, j_3925,
    j_3936, j_3947, j_3958, j_39a5, j_3754, j_3ac7, h_6898, h_6903, h_6925,
    h_3530, j_3b0f, h_3b24,
};
static const handler plus_by_mode[21] = {
    h_3530, h_3530, h_3cf9, h_6745, h_3d4f, h_3d7f, j_3e09, j_3e77, h_3f31,
    h_3f49, h_3f61, h_3f79, j_3fa3, j_3d17, j_3ac7, h_677f, h_6825, h_6849,
    h_3530, j_410d, h_4122,
};
static const handler plus_shift_by_mode[21] = {
    h_3530, h_3530, h_3d31, h_6745, h_3d4f, h_3d7f, j_3e09, j_3e77, h_3f31,
    h_3f49, h_3f61, h_3f79, j_3fb9, j_3d24, j_3ac7, h_677f, h_6825, h_6849,
    h_3530, j_410d, h_4122,
};

static void by_mode(const handler *table)
{
    V32(S_EYE_MOVED) = 0;
    eax = GB(0x288);
    if (eax < 0x15)
        table[eax]();
}

static void h_368b(void) { by_mode(minus_by_mode); }         /* MINUS */
static void h_36ff(void) { by_mode(minus_shift_by_mode); }   /* MINUS_SHIFT */
static void h_3c65(void) { by_mode(plus_by_mode); }          /* PLUS */
static void h_3cd9(void) { by_mode(plus_shift_by_mode); }    /* PLUS_SHIFT */

static void h_3cf9(void)                    /* ZOOM_PLUS */
{
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x10) != 1)
        zoom_double();
    else
        map_zoom_by(ecx, 0xffffff80);
}

static void h_3d31(void)                    /* MAP_ZOOM_FINE_IN */
{
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x10) != 1)
        zoom_in_fine();
    else
        map_zoom_by(ecx, 0xffffffe0);
}

static void j_3d17(void) { GB(0x288) = 2; h_3cf9(); }
static void j_3d24(void) { GB(0x288) = 2; h_3d31(); }

static void h_3d4f(void)                    /* COM_RADIO_WHOLE_INC */
{
    if ((GW(0xb04) & 8) && GB(0xb68) == 0) {
        ebx = edi + 0x34e;
        SET_CX(0x3618);
        bcd_whole_inc();
        tune_com();
        GB(0x288) = 4;
    }
}

static void h_3d7f(void)                    /* COM_RADIO_FRACT_INC */
{
    if ((GW(0xb04) & 8) && GB(0xb68) == 0) {
        ebx = edi + 0x34e;
        bcd_inc_step();
        tune_com();
        GB(0x288) = 5;
    }
}

/* .text+0x3dab -- ATC hears of a COM change; recorded */
static void tune_com(void)
{
    if (GW(0xb04) & 8) {
        XCALL0(SVC(ATC, 0x24));
        GB(0x38a) = 1;
        pilot_event(2, GW(0x34e));
    }
}

/* .text+0x3df1 -- 0x100ab: set the COM to the value */
static void h_3df1(void)
{
    if (GW(0xb04) & 8) {
        GW(0x34e) = CX;
        tune_com();
    }
}

static void h_3e1f(void)                    /* NAV1_RADIO_WHOLE_INC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x350;
        SET_CX(0x1708);
        bcd_whole_inc();
        tune_nav1();
        GB(0x288) = 6;
        GB(0x375) = 1;
    }
}

static void h_3e4b(void)                    /* NAV2_RADIO_WHOLE_INC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x352;
        SET_CX(0x1708);
        bcd_whole_inc();
        tune_nav2();
        GB(0x288) = 6;
        GB(0x375) = 2;
    }
}

static void j_3e09(void)
{
    if (GB(0x375) == 2)
        h_3e4b();
    else
        h_3e1f();
}

static void h_3e8d(void)                    /* NAV1_RADIO_FRACT_INC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x350;
        bcd_inc_nav();
        tune_nav1();
        GB(0x288) = 7;
        GB(0x375) = 1;
    }
}

static void h_3edb(void)                    /* NAV2_RADIO_FRACT_INC */
{
    if (GB(0xb70) == 0) {
        ebx = edi + 0x352;
        bcd_inc_nav();
        tune_nav2();
        GB(0x288) = 7;
        GB(0x375) = 2;
    }
}

static void j_3e77(void)
{
    if (GB(0x375) == 2)
        h_3edb();
    else
        h_3e8d();
}

/* .text+0x3eb5, +0x3f03 -- a NAV changed: +0x388 bit 1, recorded */
static void tune_nav1(void)
{
    GB(0x388) |= 2;
    pilot_event(6, GW(0x350));
}

static void tune_nav2(void)
{
    GB(0x388) |= 2;
    pilot_event(7, GW(0x352));
}

static void h_3ece(void) { GW(0x350) = CX; tune_nav1(); }    /* 0x100ac */
static void h_3f1c(void) { GW(0x352) = CX; tune_nav2(); }    /* 0x100ad */
static void h_3f29(void) { GW(0x354) = CX; }                 /* 0x100b3 */

static void h_3f31(void)                    /* XPNDR_1000_INC */
{
    GB(0x288) = 8;
    SET_AX(GW(0x354) + 0x1000);
    squawk();
}

static void h_3f49(void)                    /* XPNDR_100_INC */
{
    GB(0x288) = 9;
    SET_AX(GW(0x354) + 0x100);
    squawk();
}

static void h_3f61(void)                    /* XPNDR_10_INC */
{
    GB(0x288) = 0xa;
    SET_AX(GW(0x354) + 0x10);
    squawk();
}

static void h_3f79(void)                    /* XPNDR_1_INC */
{
    GB(0x288) = 0xb;
    SET_AX(GW(0x354) + 1);
    squawk();
}

/* .text+0x3f91, +0x4065 -- 0x100b4/5: set an OBS */
static void h_3f91(void)
{
    eax = V32(S_VALUE);
    GW(0xc4e) = AX;
    obs1_event();
}

static void h_4065(void)
{
    eax = V32(S_VALUE);
    GW(0xc5e) = AX;
    obs2_event();
}

static void obs_inc(uint32_t at, uint32_t which, int fast)
{
    GB(0x288) = 0xc;
    GB(0x376) = (uint8_t)which;
    if (!(GD(0x328) & 8)) {
        if (fast) {
            uint8_t was = V8(S_REPEAT);
            V8(S_REPEAT) = 3;
            if (was != 0)
                GW(at) += 9;
        } else {
            GW(at) += 9;
        }
    }
    GW(at) += 1;
    if ((int16_t)GW(at) >= 0x168)
        GW(at) -= 0x168;
    if (which == 1)
        obs1_event();
    else
        obs2_event();
}

static void h_3fcf(void) { obs_inc(0xc4e, 1, 1); }   /* VOR1_OBI_INC */
static void j_4022(void) { obs_inc(0xc4e, 1, 0); }
static void h_4077(void) { obs_inc(0xc5e, 2, 1); }   /* VOR2_OBI_INC */
static void j_40ca(void) { obs_inc(0xc5e, 2, 0); }

static void j_3fa3(void)
{
    if (GB(0x376) == 2)
        h_4077();
    else
        h_3fcf();
}

static void j_3fb9(void)
{
    if (GB(0x376) == 2)
        j_40ca();
    else
        j_4022();
}

/* +0x8b6 up a step, held at 0x7fff */
static void j_410d(void)
{
    GW(0x8b6) += 0x100;
    if ((int16_t)GW(0x8b6) < 0)
        GW(0x8b6) = 0x7fff;
}

/* .text+0x4122 -- SIM_RATE_INCR: +0xc1a doubled, below 0x8000 */
static void h_4122(void)
{
    GB(0x288) = 0x14;
    SET_AX(GW(0xc1a));
    if (AX < 0x8000) {
        SET_AX(AX << 1);
        GW(0xc1a) = AX;
    }
}

/* ---- the eye: .text+0x4141..+0x43c0 ------------------------------------------- */

/* GLOBAL +0x11a0 is the active view.  Its +0x14 is the kind of eye: 0 and 1
 * pan by their own offsets (+0x50 heading, +0x52 pitch, +0x54 bank) and are
 * re-aimed through .text+0x6bda; 4 is the spot plane, at a heading (+0x28)
 * and offsets (+0x2c, +0x34) from the aircraft. */
static void view_aim(uint32_t view, uint32_t record);   /* .text+0x6bda */

static void pan_heading(void)               /* .text+0x4186 */
{
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x14) == 0 || M32(ecx + 0x14) == 1) {
        M16(ecx + 0x50) += AX;
        view_aim(M32(ecx + 0x18), 0);
    } else if (M32(ecx + 0x14) == 4) {
        SET_AX(-AX);
        M16(ecx + 0x28) += AX;
    }
}

static void pan_left(void)                  /* .text+0x416d */
{
    eax = V32(S_PAN_RATE);
    SET_AX(-AX);
    pan_heading();
}

static void pan_right(void)                 /* .text+0x417b */
{
    eax = V32(S_PAN_RATE);
    pan_heading();
}

/* .text+0x41d3 -- pitch, or for the spot plane the offset turned about */
static void pan_pitch(void)
{
    uint16_t angle, s, c;
    uint32_t square[2];

    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x14) == 0 || M32(ecx + 0x14) == 1) {
        M16(ecx + 0x52) += AX;
        view_aim(M32(ecx + 0x18), 0);
    } else if (M32(ecx + 0x14) == 4) {
        angle = AX;
        ecx = GD(0x11a0);
        XCALL(SVC(UTIL, 0x3c), M32(ecx + 0x2c), M32(ecx + 0x34));
        angle += AX;
        XCALL(SVC(UTIL, 0x2c), angle, ADDR(&s), ADDR(&c));
        ecx = GD(0x11a0);
        eax = M32(ecx + 0x2c);
        eax = eax * eax;
        ebx = M32(ecx + 0x34);
        ebx = ebx * ebx;
        eax += ebx;
        square[0] = eax;
        square[1] = 0;
        XCALL(SVC(UTIL, 0x18), ADDR(square));
        ebx = eax;
        ecx = (uint32_t)(int32_t)(int16_t)s;
        eax = (uint32_t)((int32_t)eax * (int32_t)ecx);
        eax = (uint32_t)((int32_t)eax >> 15);
        ecx = (uint32_t)(int32_t)(int16_t)c;
        ebx = (uint32_t)((int32_t)ebx * (int32_t)ecx);
        ebx = (uint32_t)((int32_t)ebx >> 15);
        ecx = GD(0x11a0);
        M32(ecx + 0x2c) = eax;
        M32(ecx + 0x34) = ebx;
    }
}

static void pan_up(void)                    /* .text+0x41ba */
{
    eax = V32(S_PAN_RATE);
    SET_AX(-AX);
    pan_pitch();
}

static void pan_down(void)                  /* .text+0x41c8 */
{
    eax = V32(S_PAN_RATE);
    pan_pitch();
}

static void h_41ba(void) { pan_up(); }      /* PAN_UP */
static void h_41c8(void) { pan_down(); }    /* PAN_DOWN */
static void h_416d(void) { pan_left(); }    /* PAN_LEFT */
static void h_417b(void) { pan_right(); }   /* PAN_RIGHT */
static void h_4141(void) { pan_left(); pan_up(); }      /* PAN_LEFT_UP */
static void h_414c(void) { pan_left(); pan_down(); }    /* PAN_LEFT_DOWN */
static void h_4157(void) { pan_right(); pan_up(); }     /* PAN_RIGHT_UP */
static void h_4162(void) { pan_right(); pan_down(); }   /* PAN_RIGHT_DOWN */

/* .text+0x439c -- bank */
static void pan_bank(void)
{
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x14) == 0 || M32(ecx + 0x14) == 1) {
        M16(ecx + 0x54) += AX;
        view_aim(M32(ecx + 0x18), 0);
    }
}

static void h_42e0(void)                    /* PAN_TILT_LEFT */
{
    eax = V32(S_PAN_RATE);
    pan_bank();
}

static void h_42eb(void)                    /* PAN_TILT_RIGHT */
{
    eax = V32(S_PAN_RATE);
    eax = -eax;
    pan_bank();
}

/* .text+0x42f8 -- PAN_RESET; the spot plane comes round behind */
static void h_42f8(void)
{
    uint32_t square[2];

    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x14) == 0 || M32(ecx + 0x14) == 1) {
        M16(ecx + 0x52) = 0;
        M16(ecx + 0x54) = 0;
        M16(ecx + 0x50) = 0;
        view_aim(M32(ecx + 0x18), 0);
    } else if (M32(ecx + 0x14) == 4) {
        ecx = GD(0x11a0);
        eax = M32(ecx + 0x2c);
        eax = eax * eax;
        ebx = M32(ecx + 0x34);
        ebx = ebx * ebx;
        eax += ebx;
        square[0] = eax;
        square[1] = 0;
        XCALL(SVC(UTIL, 0x18), ADDR(square));
        ebx = eax;
        ecx = GD(0x11a0);
        M32(ecx + 0x2c) = eax;
        M32(ecx + 0x34) = 0;
        M16(ecx + 0x28) = 0x8000;
    }
}

static void zoom_get(void);                 /* .text+0x6d75 */

/* .text+0x43c1 -- ZOOM_1X */
static void h_43c1(void)
{
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x10) != 1) {
        zoom_get();
        SET_AX(0x40);
        zoom_set();
    } else {
        GW(0x2b2) = 0;
        map_zoom_to(ecx, 0x100);
    }
}

/* ---- toggles and settings: .text+0x43f4..+0x4a07 -------------------------------- */

static void h_43f4(void) { XCALL(SVC(HOST, 0x3c), 0x80011, 0); }   /* PANEL_SELECT_1 */
static void h_441e(void) { XCALL(SVC(HOST, 0x3c), 0x80001, 0); }   /* PANEL_SELECT_2 */

/* .text+0x4448 -- SOUND_TOGGLE: +0xb20, and the menu item 0x1408 */
static void h_4448(void)
{
    GD(0xb20) ^= 1;
    if (GD(0xb20) == 0)
        XCALL(SVC(HOST, 0x254), 0x1408, 0);
    else
        XCALL(SVC(HOST, 0x254), 0x1408, 1);
    edx = GD(0xb20);
    pilot_event(0x12, DX);
}

static void h_44c0(void) { GD(0xb20) = ecx; }      /* 0x1009c: set the sound */

static void h_44c7(void)                    /* FULL_WINDOW_TOGGLE */
{
    ebx = GD(0x119c);
    XCALL(SVC(HOST, 0x3c), 0x2000e, ebx);
}

static void h_44f6(void)                    /* ENGINE: all engines, then SELECT_n */
{
    GD(0x888) = 1;
    V32(S_SELECTED) = 1;
    V8(S_SELECT) = 3;
    GB(0x288) = 0xd;
}

static void h_4519(void) { GB(0x288) = 0x14; }     /* SIM_RATE */

static void h_4521(void)                    /* XPNDR: the next digit while it repeats */
{
    uint8_t was = V8(S_REPEAT);

    V8(S_REPEAT) = 0x12;
    if (was != 0 && GB(0x288) >= 8 && GB(0x288) < 0xb)
        GB(0x288)++;
    else
        GB(0x288) = 8;
}

/* the autopilot's holds: .text+0x4c7b..+0x5068, below */
static void ap_att_on(void);                /* .text+0x4d7d */
static void ap_att_off(void);               /* .text+0x4f7f */
static void ap_loc_on(void);                /* .text+0x4daf */
static void ap_loc_off(void);               /* .text+0x4f93 */
static void ap_apr_on(void);                /* .text+0x4de0 */
static void ap_apr_off(void);               /* .text+0x4fb0 */
static void ap_hdg_on(void);                /* .text+0x4e12 */
static void ap_hdg_off(void);               /* .text+0x4fce */

static void h_4553(void)                    /* AP_ATT_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7d8) == 0)
            ap_att_on();
        else
            ap_att_off();
    }
}

/* .text+0x4581 -- slew on and off: +0x5dc, the accelerators, the menu item */
static void slew_toggle(void)
{
    V32(0x107) ^= 1;
    V32(0x107) &= 1;
    eax = V32(0x107);
    GW(0x5dc) = AX;
    if (V32(0x107) == 0) {
        XCALL(SVC(HOST, 0x1ec), V32(S_ACCEL));
        controls_accel_menu(0);
        XCALL(SVC(HOST, 0x254), 0x1207, 0);
    } else {
        XCALL(SVC(HOST, 0x1e8), V32(S_ACCEL));
        controls_accel_menu(1);
        XCALL(SVC(HOST, 0x254), 0x1207, 1);
    }
}

static void h_4572(void)                    /* SLEW_TOGGLE, not while multiplayer */
{
    if (GD(0x84c) == 0)
        slew_toggle();
}

static void h_465e(void)                    /* SLEW_ON */
{
    if (V32(0x107) == 0)
        slew_toggle();
}

static void h_466d(void)                    /* SLEW_OFF */
{
    if (V32(0x107) != 0)
        slew_toggle();
}

static void h_467c(void) { GB(0x288) = 0x13; }     /* EGT */

static void h_4684(void)                    /* SMOKE_TOGGLE */
{
    if (GD(0x5d8) == 0) {
        if (GD(0x5d4) != 0) {
            GD(0x5d8) = 1;
            XCALL(SVC(MAIN, 0x84), edi + 0x560, 1, D + S_FLIGHT);
        }
    } else {
        GD(0x5d8) = 0;
        XCALL(SVC(MAIN, 0x8c), D + S_FLIGHT);
    }
}

static void h_470a(void)                    /* STROBES_TOGGLE */
{
    GB(0x281) ^= 1;
    GB(0x281) &= 1;
    edx = GB(0x281);
    pilot_event(0xe, DX);
}

static void h_472c(void)                    /* AP_LOC_HOLD */
{
    if (GD(0x7bc) != 0 && GD(0x7fc) == 0) {
        if (GD(0x800) == 0)
            ap_loc_on();
        else
            ap_loc_off();
    }
}

static void pause_on(void);                 /* .text+0x7056 */
static void pause_off(void);                /* .text+0x7061 */

static void h_4754(void)                    /* PAUSE_TOGGLE */
{
    if (GD(0x264) != 0)
        pause_off();
    else
        pause_on();
}

static void h_476c(void) { XCALL0(SVC(PANELS, 0x30)); }    /* PANEL_TOGGLE */

/* .text+0x478f -- ATC: tests, and does nothing either way */
static void h_478f(void)
{
}

static void h_47a4(void)                    /* ADF: the next digit while it repeats */
{
    uint8_t was;

    V8(S_SELECT) = 3;
    SET_AL(GB(0x288));
    was = V8(S_REPEAT);
    V8(S_REPEAT) = 0x12;
    if (was == 0 || (AL != 0xf && AL != 0x10))
        SET_AL(0xf);
    else if (AL == 0xf)
        SET_AL(0x10);
    else
        SET_AL(0x11);
    GB(0x288) = AL;
}

static void h_47da(void)                    /* AP_APR_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7fc) == 0)
            ap_apr_on();
        else
            ap_apr_off();
    }
}

static void h_47f9(void) { XCALL0(SVC(WINDOW, 0x38)); }    /* VIEW_MODE */
static void h_481c(void) { XCALL0(SVC(WINDOW, 0x34)); }    /* VIEW_MODE_REV */
static void h_483f(void) { GW(0xc3e) = 0; }                /* HEADING_GYRO_SET */

static void h_4849(void)                    /* GYRO_DRIFT_INC */
{
    uint8_t was = V8(S_REPEAT);

    V8(S_REPEAT) = 3;
    if (was != 0)
        GW(0xc3e) += 0x666;
    GW(0xc3e) += 0xb6;
}

static void h_4876(void)                    /* GYRO_DRIFT_DEC */
{
    uint8_t was = V8(S_REPEAT);

    V8(S_REPEAT) = 3;
    if (was != 0)
        GW(0xc3e) -= 0x666;
    GW(0xc3e) -= 0xb6;
}

static void h_48a3(void)                    /* DME */
{
    V8(S_SELECT) = 3;
    GB(0x288) = 0xe;
}

static void h_48b2(void)                    /* 0x100a4: the gear to the value */
{
    if (GD(0x60c) == 1) {
        ecx = 0x3fff;
        eax = 0x15;
        if (V32(S_VALUE) == 0) {
            ecx = 0;
            eax = 0x14;
        }
        GD(0xbe8) = ecx;
        XCALL(SVC(SOUND, 0x24), eax, 0);
    }
}

static void h_4901(void)                    /* GEAR_TOGGLE */
{
    if (GD(0x60c) != 1)
        return;
    eax = 0;
    if (GD(0xbe8) == 0)
        eax = 0x3fff;
    GD(0xbe8) = eax;
    if (eax == 0) {
        if (GB(0x366) == 0)
            XCALL(SVC(SOUND, 0x24), 0x14, 0);
    } else {
        if (GB(0x366) == 0)
            XCALL(SVC(SOUND, 0x24), 0x15, 0);
    }
    edx = GD(0xbe8);
    pilot_event(4, DX);
}

static void h_499f(void) { XCALL0(SVC(MAIN, 0xc8)); }      /* CHECKLIST_VIEW */
static void h_49c5(void) { GD(0xbf8) = 0xffff; }           /* GEAR_PUMP */

static void engines_toggle_heat(void);      /* .text+0x5f4a */

static void h_49d0(void)                    /* ANTI_ICE_TOGGLE */
{
    engines_toggle_heat();
    pilot_event(0xf, GW(0x8b2));
}

static void h_49e7(void)                    /* PITOT_HEAT_TOGGLE */
{
    if (GD(0x788) == 0) {
        GD(0x29c) &= 0;
    } else {
        GD(0x29c) &= 1;
        GD(0x29c) ^= 1;
    }
}

/* ---- the autopilot and the rest: .text+0x4a08..+0x59c2 ----------------------------
 *
 * GLOBAL +0x7bc is the autopilot's master; the holds: +0x7c0 wing leveller,
 * +0x7c4 NAV1, +0x7c8 heading, +0x7d0 altitude, +0x7d8 attitude, +0x7fc
 * approach, +0x800 localiser, +0x804 back course, +0x7dc airspeed, +0x7e4
 * mach, +0x7ec vertical speed, +0x808 yaw damper, +0x80c/+0x810 the
 * autothrottle.  Targets: +0x7cc heading, +0x7d4 altitude, +0x7e0 speed,
 * +0x7e8 mach, +0x7f0 vertical speed.  +0x764 says the aircraft has one,
 * +0x84c is LANDME flying. */

/* the C runtime's divide, with the host's repair of an overflow */
static void div32(uint32_t d)
{
    uint64_t n = ((uint64_t)edx << 32) | eax;

    if (d == 0 || n / d > 0xffffffffu) {
        eax = 0xffffffffu;
        edx = 0xffffffffu;
        return;
    }
    eax = (uint32_t)(n / d);
    edx = (uint32_t)(n % d);
}

static void h_4a08(void)                    /* AP_HDG_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7c8) == 0)
            ap_hdg_on();
        else
            ap_hdg_off();
    }
}

static void h_4a27(void)                    /* AP_PANEL_HEADING_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7c8) == 0) {
            GD(0x7c0) = 0;
            GD(0x7d8) = 0;
            GD(0x7c8) = 1;
        } else {
            GD(0x7c8) = 0;
        }
    }
}

static void h_4a64(void)                    /* AP_PANEL_VS_HOLD */
{
    if (GD(0x7bc) != 0)
        GD(0x7ec) = GD(0x7ec) == 0 ? 1 : 0;
}

static void h_4a8d(void)                    /* AP_PANEL_SPEED_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7dc) == 0) {
            GD(0x7dc) = 1;
            if (GD(0x7e4) != 0)
                GD(0x7e4) = 0;
        } else {
            GD(0x7dc) = 0;
        }
    }
}

static void h_4ac9(void)                    /* AP_PANEL_MACH_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7e4) == 0) {
            GD(0x7e4) = 1;
            if (GD(0x7dc) != 0)
                GD(0x7dc) = 0;
        } else {
            GD(0x7e4) = 0;
        }
    }
}

static void h_4b05(void)                    /* JET_STARTER */
{
    if (GB(0x609) == 1)
        GB(0x288) = 3;
}

/* .text+0x4b16 -- JOYSTICK_CALIBRATE: the joysticks off and on */
static void h_4b16(void)
{
    eax = controls_joystick(0, 0);
    eax ^= 1;
    eax = controls_joystick(1, eax);
}

static void h_4b2b(void)                    /* ALL_LIGHTS_TOGGLE, PANEL_LIGHTS_TOGGLE */
{
    GB(0x280) ^= 1;
    GB(0x280) &= 1;
    SET_DX(GB(0x280));
    pilot_event(5, DX);
    XCALL0(SVC(HG2D, 0x64));
}

static void h_4b70(void)                    /* LANDING_LIGHTS_TOGGLE */
{
    GD(0x28c) ^= 1;
    GD(0x28c) &= 1;
    edx = GD(0x28c);
    pilot_event(0xd, DX);
}

static void h_4b91(void)                    /* LANDING_LIGHT_UP */
{
    eax = GD(0x290) + 0xff000000u;
    if ((int32_t)eax <= (int32_t)0xfc000000u)
        eax = 0xfc000000u;
    GD(0x290) = eax;
}

static void h_4baf(void)                    /* LANDING_LIGHT_DOWN */
{
    eax = GD(0x290) + 0x1000000u;
    if ((int32_t)eax >= 0x10000000)
        eax = 0x10000000u;
    GD(0x290) = eax;
}

static void h_4bcd(void) { GD(0x298) += 0xff000000u; }    /* LANDING_LIGHT_LEFT */
static void h_4bd8(void) { GD(0x298) += 0x1000000u; }     /* LANDING_LIGHT_RIGHT */

static void h_4be3(void)                    /* LANDING_LIGHT_HOME */
{
    GD(0x290) = 0;
    GD(0x294) = 0;
    GD(0x298) = 0;
}

static void h_4c02(void) { XCALL0(SVC(MAIN, 0xac)); }              /* SITUATION_SAVE */
static void h_4c28(void) { XCALL(SVC(HOST, 0x90), GD(0x119c)); }   /* VIEW_WINDOW_TO_FRONT */
static void h_4c54(void) { }                                       /* ANALYSIS_MANEUVER_STOP */

static void cycle4(void)                    /* .text+0x4c61: [ebx] round 0..3 */
{
    M32(ebx)++;
    if (M32(ebx) >= 4)
        M32(ebx) = 0;
}

static void h_4c55(void) { ebx = edi + 0x5f4; cycle4(); }   /* READOUTS_SLEW */
static void h_4c6f(void) { ebx = edi + 0x5fc; cycle4(); }   /* READOUTS_FLIGHT */

static void h_4c91(void)                    /* AUTOPILOT_OFF */
{
    if (GD(0x764) == 0) {
        GD(0x7bc) = 0;
        return;
    }
    if (GD(0x84c) != 0)
        return;
    GD(0x7bc) = 0;
    GD(0x7c0) = 0;
    GD(0x7d8) = 0;
    GD(0x7d0) = 0;
    GD(0x7fc) = 0;
    GD(0x7c4) = 0;
    GD(0x7c8) = 0;
    GD(0x800) = 0;
    GD(0x804) = 0;
    GD(0x7dc) = 0;
    GD(0x7e4) = 0;
    if (GD(0xb10) == 0)
        GD(0x7d4) = 0;
    if (GD(0xb14) == 0)
        GW(0x7cc) = 0;
}

static void h_4d4a(void)                    /* AUTOPILOT_ON */
{
    if (GD(0x764) == 0) {
        GD(0x7bc) = 0;
        GD(0x808) = 0;
    } else if (GD(0x84c) == 0) {
        GD(0x7bc) = 1;
    }
}

static void h_4c7b(void)                    /* AP_MASTER */
{
    if (GD(0x7bc) == 0)
        h_4d4a();
    else
        h_4c91();
}

static void ap_att_on(void)                 /* AP_ATT_HOLD_ON */
{
    if (GD(0x7bc) != 0) {
        GD(0x7c0) = 0;
        GD(0x7d8) = 1;
        GD(0x7d0) = 0;
        GD(0x7c8) = 0;
    }
}

static void ap_loc_on(void)                 /* AP_LOC_HOLD_ON */
{
    if (GD(0x7bc) != 0 && GD(0x7fc) == 0) {
        GD(0x7c4) = 0;
        GD(0x800) = 1;
        GD(0x804) = 0;
    }
}

static void ap_apr_on(void)                 /* AP_APR_HOLD_ON */
{
    if (GD(0x7bc) != 0) {
        GD(0x7fc) = 1;
        GD(0x7c4) = 0;
        GD(0x800) = 1;
        GD(0x804) = 0;
    }
}

static void ap_hdg_on(void)                 /* AP_HDG_HOLD_ON: on the present heading */
{
    if (GD(0x7bc) != 0) {
        GD(0x7c0) = 0;
        GD(0x7d8) = 0;
        GD(0x7c8) = 1;
        eax = GD(0x580);
        eax >>= 16;
        SET_AX(AX - GW(0x2a0));
        GW(0x7cc) = AX;
    }
}

static void h_4e51(void)                    /* HEADING_BUG_INC */
{
    uint8_t was = V8(S_REPEAT);

    V8(S_REPEAT) = 3;
    if (was != 0)
        GW(0x7cc) += 0x666;
    GW(0x7cc) += 0xb6;
}

static void h_4e74(void)                    /* HEADING_BUG_DEC */
{
    uint8_t was = V8(S_REPEAT);

    V8(S_REPEAT) = 3;
    if (was != 0)
        GW(0x7cc) -= 0x666;
    GW(0x7cc) -= 0xb6;
}

/* .text+0x4e97 -- AP_ALT_HOLD_ON: on the present altitude, rounded to 100
 * ft (or 100 m in metric, +0xc18 2) when +0xb10 asks.  The rounding divides
 * after cdq, which overflows below the sea; the host repairs it. */
static void ap_alt_on(void)
{
    uint32_t keep_ebx, keep_edx;

    if (GD(0x7bc) == 0)
        return;
    GD(0x7d0) = 1;
    eax = GD(0x574);
    eax <<= 16;
    SET_AX(GW(0x572));
    GD(0x7d4) = eax;
    if (GD(0xb10) == 0)
        return;
    keep_ebx = ebx;
    keep_edx = edx;
    ebx = GW(0xc18) == 2 ? 0x320000u : 0xf3d70u;
    eax += ebx;
    edx = (int32_t)eax < 0 ? 0xffffffffu : 0;
    ebx += ebx;
    div32(ebx);
    {
        uint64_t p = (uint64_t)eax * ebx;
        eax = (uint32_t)p;
        edx = (uint32_t)(p >> 32);
    }
    GD(0x7d4) = eax;
    edx = keep_edx;
    ebx = keep_ebx;
}

static void ap_wing_on(void)                /* AP_WING_LEVELER_ON */
{
    if (GD(0x7bc) != 0) {
        GD(0x7c0) = 1;
        GD(0x7d8) = 0;
        GD(0x7c8) = 0;
    }
}

static void ap_bc_on(void)                  /* AP_BC_HOLD_ON */
{
    if (GD(0x7bc) != 0) {
        GD(0x7fc) = 0;
        GD(0x7c4) = 0;
        GD(0x800) = 0;
        GD(0x804) = 1;
    }
}

static void ap_nav1_on(void)                /* AP_NAV1_HOLD_ON */
{
    if (GD(0x7bc) != 0) {
        GD(0x7fc) = 0;
        GD(0x7c4) = 1;
        GD(0x800) = 0;
        GD(0x804) = 0;
    }
}

static void ap_att_off(void) { if (GD(0x7bc) != 0) GD(0x7d8) = 0; }
static void ap_loc_off(void) { if (GD(0x7bc) != 0 && GD(0x7fc) == 0) GD(0x800) = 0; }
static void ap_apr_off(void) { if (GD(0x7bc) != 0) { GD(0x7fc) = 0; GD(0x800) = 0; } }
static void ap_hdg_off(void) { if (GD(0x7bc) != 0) GD(0x7c8) = 0; }
static void ap_alt_off(void) { if (GD(0x7bc) != 0) GD(0x7d0) = 0; }
static void ap_wing_off(void) { if (GD(0x7bc) != 0) GD(0x7c0) = 0; }
static void ap_bc_off(void) { if (GD(0x7bc) != 0) GD(0x804) = 0; }
static void ap_nav1_off(void) { if (GD(0x7bc) != 0) GD(0x7c4) = 0; }

static void h_4d7d(void) { ap_att_on(); }
static void h_4daf(void) { ap_loc_on(); }
static void h_4de0(void) { ap_apr_on(); }
static void h_4e12(void) { ap_hdg_on(); }
static void h_4e97(void) { ap_alt_on(); }
static void h_4ef3(void) { ap_wing_on(); }
static void h_4f1b(void) { ap_bc_on(); }
static void h_4f4d(void) { ap_nav1_on(); }
static void h_4f7f(void) { ap_att_off(); }
static void h_4f93(void) { ap_loc_off(); }
static void h_4fb0(void) { ap_apr_off(); }
static void h_4fce(void) { ap_hdg_off(); }
static void h_4fe2(void) { ap_alt_off(); }
static void h_4ff6(void) { ap_wing_off(); }
static void h_500a(void) { ap_bc_off(); }
static void h_501e(void) { ap_nav1_off(); }

static void h_5032(void)                    /* YAW_DAMPER_TOGGLE */
{
    if (GD(0x764) == 0) {
        GD(0x7bc) = 0;
        GD(0x808) = 0;
    } else if (GD(0x84c) == 0) {
        GD(0x808) ^= 1;
        GD(0x808) &= 1;
    }
}

static void h_5069(void)                    /* AP_ALT_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7d0) == 0)
            ap_alt_on();
        else
            ap_alt_off();
    }
}

static void h_5088(void)                    /* AP_PANEL_ALTITUDE_HOLD */
{
    if (GD(0x7bc) != 0)
        GD(0x7d0) = GD(0x7d0) == 0 ? 1 : 0;
}

/* the altitude target, in 32.16 metres: 100 ft (500 ft fast), or 100 m
 * (500 m) in metric */
static int fast(void)                       /* the repeat test, and its reload */
{
    uint8_t was = V8(S_REPEAT);

    V8(S_REPEAT) = 3;
    return was != 0;
}

static void h_50b1(void)                    /* AP_ALT_VAR_INC */
{
    if (GW(0xc18) == 2) {
        if (fast())
            GD(0x7d4) += 0x1900000u;
        GD(0x7d4) += 0x640000u;
        if (GD(0x7d4) > 0x76f20000u)
            GD(0x7d4) = 0x76f20000u;
    } else {
        if (fast())
            GD(0x7d4) += 0x79eb94u;
        GD(0x7d4) += 0x1e7ae5u;
        if (GD(0x7d4) > 0x76f193a3u)
            GD(0x7d4) = 0x76f193a3u;
    }
}

static void h_513e(void)                    /* AP_ALT_VAR_DEC */
{
    if (GW(0xc18) == 2) {
        if (fast())
            GD(0x7d4) -= 0x1900000u;
        if (GD(0x7d4) == 0x76f20000u) {
            GD(0x7d4) -= 0x320000u;
        } else {
            GD(0x7d4) -= 0x640000u;
            if ((int32_t)GD(0x7d4) < 0)
                GD(0x7d4) = 0;
        }
    } else {
        if (fast())
            GD(0x7d4) -= 0x79eb94u;
        GD(0x7d4) -= 0x1e7ae5u;
        if ((int32_t)GD(0x7d4) < 0)
            GD(0x7d4) = 0;
    }
}

/* the vertical speed target: steps of 100 (or 200 past 1000) ft/min, or
 * the metric equivalents, with two values stepped over */
#define VS ((int32_t)GD(0x7f0))

static void h_51cf(void)                    /* AP_VS_VAR_INC */
{
    if (GW(0xc18) != 2) {
        if (fast()) {
            if (VS < 0x3e80000 && VS >= (int32_t)0xfc180000u) {
                GD(0x7f0) += 0xc80000u;
                if (GD(0x7f0) == 0x41a0000u || GD(0x7f0) == 0x47e0000u)
                    GD(0x7f0) -= 0x320000u;
                return;
            }
            GD(0x7f0) += 0x1900000u;
        } else if (VS < 0x3e80000 && VS >= (int32_t)0xfc180000u) {
            GD(0x7f0) += 0x320000u;
        } else {
            GD(0x7f0) += 0x640000u;
        }
        if (VS > 0x1b580000)
            GD(0x7f0) = 0x1b580000u;
    } else {
        if (fast()) {
            if (VS < 0xcd0d8e0 && VS >= (int32_t)0xf32f2720u) {
                GD(0x7f0) += 0x14815b0u;
                if (GD(0x7f0) == 0xd74e3b8u || GD(0x7f0) == 0xebcf968u)
                    GD(0x7f0) -= 0xa40ad8u;
                return;
            }
            GD(0x7f0) += 0x2902b60u;
        } else if (VS < 0xcd0d8e0 && VS >= (int32_t)0xf32f2720u) {
            GD(0x7f0) += 0xa40ad8u;
        } else {
            GD(0x7f0) += 0x14815b0u;
        }
        if (VS > 0x1ae9c770)
            GD(0x7f0) = 0x1ae9c770u;
    }
}

static void h_5338(void)                    /* AP_VS_VAR_DEC */
{
    if (GW(0xc18) != 2) {
        if (fast()) {
            if (VS <= 0x3e80000 && VS > (int32_t)0xfc180000u) {
                GD(0x7f0) -= 0xc80000u;
                if (GD(0x7f0) == 0xfbe60000u || GD(0x7f0) == 0xfb820000u)
                    GD(0x7f0) += 0x320000u;
                return;
            }
            GD(0x7f0) -= 0x1900000u;
        } else if (VS <= 0x3e80000 && VS > (int32_t)0xfc180000u) {
            GD(0x7f0) -= 0x320000u;
        } else {
            GD(0x7f0) -= 0x640000u;
        }
        if (VS < (int32_t)0xe8900000u)
            GD(0x7f0) = 0xe8900000u;
    } else {
        if (fast()) {
            if (VS <= 0xcd0d8e0 && VS > (int32_t)0xf32f2720u) {
                GD(0x7f0) -= 0x14815b0u;
                if (GD(0x7f0) == 0xf28b1c48u || GD(0x7f0) == 0xf1430698u)
                    GD(0x7f0) += 0xa40ad8u;
                return;
            }
            GD(0x7f0) -= 0x2902b60u;
        } else if (VS <= 0xcd0d8e0 && VS > (int32_t)0xf32f2720u) {
            GD(0x7f0) -= 0xa40ad8u;
        } else {
            GD(0x7f0) -= 0x14815b0u;
        }
        if (VS < (int32_t)0xeb7ea500u)
            GD(0x7f0) = 0xeb7ea500u;
    }
}

static void h_54a1(void)                    /* AP_SPD_VAR_INC */
{
    if (fast())
        GD(0x7e0) += 0x280000u;
    GD(0x7e0) += 0xa0000u;
    if (GD(0x7e0) > 0x3de0000u)
        GD(0x7e0) = 0x3de0000u;
}

static void h_54dc(void)                    /* AP_SPD_VAR_DEC */
{
    if (fast())
        GD(0x7e0) -= 0x280000u;
    GD(0x7e0) -= 0xa0000u;
    if ((int32_t)GD(0x7e0) < 0)
        GD(0x7e0) = 0;
}

static void h_550d(void)                    /* AP_MACH_VAR_INC */
{
    if (fast())
        GD(0x7e8) += 0xa3c;
    GD(0x7e8) += 0x28f;
    if (GD(0x7e8) > 0xfd71)
        GD(0x7e8) = 0xfd71;
}

static void h_5548(void)                    /* AP_MACH_VAR_DEC */
{
    if (fast())
        GD(0x7e8) -= 0xa3c;
    GD(0x7e8) -= 0x28f;
    if ((int32_t)GD(0x7e8) < 0)
        GD(0x7e8) = 0;
}

static void h_5579(void) { }                /* AP_N1_REF_INC */
static void h_557a(void) { }                /* AP_N1_REF_DEC */

/* .text+0x557b -- LANDME_TOGGLE: not in multiplayer, a crash, slew, or
 * while a lesson says no (ADVDRV +0x18's state 0x20) */
static void h_557b(void)
{
    uint32_t state = 0;

    if (GD(0x850) == 0 || GD(0x628) != 0 || GD(0x758) != 0 || GW(0x5dc) != 0)
        return;
    XCALL(SVC(ADVDRV, 0x18), ADDR(&state));
    if (eax != 0 && state == 0x20)
        return;
    GD(0x84c) ^= 1;
    GD(0x84c) &= 1;
}

static void h_55e7(void)                    /* COM_RADIO */
{
    uint8_t was;

    if (!(GW(0xb04) & 8))
        return;
    GB(0x38a) = 1;
    was = V8(S_REPEAT);
    V8(S_REPEAT) = 0x12;
    if (was != 0 && GB(0x288) == 4)
        GB(0x288) = 5;
    else
        GB(0x288) = 4;
}

/* .text+0x5623 -- EXIT: asks first in a multiplayer session (+0x122c) */
static void quit(void)
{
    XCALL0(SVC(MAIN, 0x90));
    XCALL(SVC(HOST, 0x48), 0);
    XCALL0(SVC(HOST, 0x24));
}

static void h_5623(void)
{
    if (GD(0x122c) == 1) {
        XCALL(SVC(UTIL, 0xa4), 0xc0, 3, 0xf402, 0, 0, 0, 0, 0);
        if (eax != 0x40)
            return;
    }
    quit();
}

static void h_56d8(void) { quit(); }        /* ABORT */

static void h_5744(void)                    /* VOR_OBS */
{
    GB(0x288) = 0xc;
    V8(S_SELECT) = 3;
}

static void h_5753(void)                    /* AP_WING_LEVELER */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7c0) == 0)
            ap_wing_on();
        else
            ap_wing_off();
    }
}

static void h_5772(void)                    /* BAROMETRIC */
{
    SET_AX(GW(0xec6));
    GW(0x330) = AX;
}

static void h_5781(void)                    /* KOHLSMAN_INC */
{
    if (fast())
        GW(0x330) += 0x2d;
    GW(0x330) += 5;
    if (GW(0x330) > 0x43c0)
        GW(0x330) = 0x43c0;
}

static void h_57b6(void)                    /* KOHLSMAN_DEC */
{
    if (fast())
        GW(0x330) -= 0x2d;
    GW(0x330) -= 5;
    if (GW(0x330) < 0x3b40)
        GW(0x330) = 0x3b40;
}

static void h_57eb(void)                    /* TRUE_AIRSPEED_CAL_INC */
{
    if (fast())
        GD(0x2c0) += 0x2d8;
    GD(0x2c0) += 0xb6;
    if (GD(0x2c0) > 0x4000)
        GD(0x2c0) = 0x4000;
}

static void h_5826(void)                    /* TRUE_AIRSPEED_CAL_DEC */
{
    if (fast())
        GD(0x2c0) -= 0x2d8;
    GD(0x2c0) -= 0xb6;
    if ((int32_t)GD(0x2c0) < 0)
        GD(0x2c0) = 0;
}

static void h_5857(void) { GD(0xafc) = 0; }       /* CROSS_FEED_OFF */
static void h_5862(void) { GD(0xafc) = 1; }       /* CROSS_FEED_LEFT_TO_RIGHT */
static void h_586d(void) { GD(0xafc) = 2; }       /* CROSS_FEED_RIGHT_TO_LEFT */

static void h_5878(void)                    /* AP_BC_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x804) == 0)
            ap_bc_on();
        else
            ap_bc_off();
    }
}

static void h_5897(void)                    /* NAV_RADIO */
{
    uint8_t was;

    V8(S_SELECT) = 3;
    was = V8(S_REPEAT);
    V8(S_REPEAT) = 0x12;
    if (was != 0 && GB(0x288) == 6)
        GB(0x288) = 7;
    else
        GB(0x288) = 6;
}

static void h_58c8(void)                    /* AP_NAV1_HOLD */
{
    if (GD(0x7bc) != 0) {
        if (GD(0x7c4) == 0)
            ap_nav1_on();
        else
            ap_nav1_off();
    }
}

static void h_58e7(void)                    /* MAGNETO, not for a jet */
{
    if (GB(0x609) == 0)
        GB(0x288) = 3;
}

/* .text+0x591b -- the brakes, each byte held to 0xc8 */
static void brakes_hold(void)
{
    SET_CL(0xc8);
    if (AH >= CL)
        SET_AH(CL);
    if (AL >= CL)
        SET_AL(CL);
    GW(0xc00) = AX;
}

static void h_58f8(void)                    /* BRAKES */
{
    GW(0xbc8) = 0;
    SET_AX(GW(0xc00));
    if (AX == 0)
        SET_AX(AX + 0x3232);
    SET_AX(AX + 0x2828);
    brakes_hold();
}

static void h_5931(void)                    /* PARKING_BRAKES */
{
    SET_AX(0xc8c8);
    GW(0xbc8) = AX;
    GW(0xc00) = AX;
}

static void h_5944(void)                    /* SPOILERS_TOGGLE */
{
    if (GD(0xbd0) != 0x3fff) {
        GD(0xbd0) = 0x3fff;
        GD(0xbcc) &= 0;
    } else {
        GD(0xbd0) = 0;
        GD(0xbcc) &= 0;
    }
}

static void h_5975(void)                    /* SPOILERS_SET */
{
    eax = V32(S_VALUE);
    GD(0xbd0) = eax;
}

static void h_5981(void)                    /* SPOILERS_ARM_TOGGLE */
{
    GD(0xbcc) &= 1;
    GD(0xbcc) ^= 1;
}

static void h_5990(void)                    /* SITUATION_RESET, not after a crash */
{
    if (GB(0x838) == 0)
        XCALL(SVC(HOST, 0x3c), 0x80005, 0);
}

/* ---- flaps: .text+0x59c3..+0x5b51 ------------------------------------------------- */

/* .text+0x5a9a -- the flaps to w, if the aircraft has them; a sound when
 * they move, and recorded */
static void flaps_to(uint32_t w)
{
    if (GD(0x778) == 0)
        return;
    eax = (uint16_t)w;
    if (GD(0xbdc) != eax)
        XCALL(SVC(SOUND, 0x24), 0x16, 0);
    eax = (uint16_t)w;
    GD(0xbdc) = eax;
    ecx = GD(0xbdc);
    pilot_event(0x13, CX);
}

/* .text+0x5af8 -- by w, to an end if it passes one, or if half of w more
 * would */
static void flaps_by(uint32_t w)
{
    uint16_t bx, cx;

    eax = (uint32_t)(int32_t)(int16_t)w + GD(0xbdc);
    if (eax > 0x3fff) {
        SET_AX(0);
        if ((int16_t)w >= 0)
            SET_AX(0x3fff);
    }
    cx = (uint16_t)((int16_t)w >> 1);
    bx = AX + cx;
    if (bx > 0x3fff) {
        SET_AX(0);
        if ((int16_t)w >= 0)
            SET_AX(0x3fff);
    }
    flaps_to(AX);
}

static void h_59c3(void) { flaps_to(0); }                  /* FLAPS_UP */
static void h_59cb(void) { flaps_to(0); flaps_by(V16(S_STEP)); }
static void h_59e2(void)
{
    flaps_to(0);
    flaps_by(V16(S_STEP));
    flaps_by(V16(S_STEP));
}
static void h_5a08(void)
{
    flaps_to(0);
    flaps_by(V16(S_STEP));
    flaps_by(V16(S_STEP));
    flaps_by(V16(S_STEP));
}
static void h_5a3d(void) { flaps_to(0x3fff); }             /* FLAPS_DOWN */

static void h_5a48(void)                    /* 0x100a2: the flaps to the value */
{
    ecx = V32(S_VALUE);
    flaps_to(CX);
}

static void h_5a59(void) { flaps_by(V16(S_STEP)); }        /* FLAPS_INCR */

static void h_5a69(void)                    /* FLAPS_DECR */
{
    SET_AX(V16(S_STEP));
    SET_AX(-AX);
    flaps_by(AX);
}

static void h_5a7d(void)                    /* FLAPS_DETENTS_SET: the step for n */
{
    ecx = V32(S_VALUE);
    SET_CX(CX - 1);
    if ((int16_t)CX <= 0)
        return;
    eax = 0x3fff;
    SET_DX(0);
    SET_DX(0x3fff % CX);
    SET_AX(0x3fff / CX);
    V16(S_STEP) = AX;
}

/* ---- the engines: .text+0x5b54..+0x5f6c --------------------------------------------
 *
 * GLOBAL +0x888 has a bit for each engine selected; their records are at
 * +0x88c, +0x924, +0x9bc and +0xa54, with the throttle at +0, the propeller
 * at +2, the mixture at +4, the magnetos at +6 and the carburettor heat at
 * +0x26. */

/* .text+0x5e81 -- engine `mask` (one bit) if it is selected, or 0 */
static void engine_selected(uint32_t mask)
{
    eax = 0;
    ebx = (uint16_t)mask & GD(0x888);
    if (ebx == 0)
        return;
    if (ebx & 1)
        eax = edi + 0x88c;
    else if (ebx & 2)
        eax = edi + 0x924;
    else if (ebx & 4)
        eax = edi + 0x9bc;
    else if (ebx & 8)
        eax = edi + 0xa54;
    else
        eax = 0;
}

/* .text+0x5e3f -- ebx to field `which` (0 throttle, 1 propeller, 2 mixture)
 * of engine `mask`; carry if it is not selected or which is past 2.  eax is
 * kept. */
static int engine_field(uint32_t mask, uint32_t which)
{
    uint32_t keep = eax;

    engine_selected(mask);
    if (eax == 0) {
        eax = keep;
        return 1;
    }
    ebx = 0;
    if (which != 0) {
        ebx = 2;
        if (which != 1) {
            ebx = 4;
            if (which != 2) {
                eax = keep;
                return 1;
            }
        }
    }
    ebx += eax;
    eax = keep;
    return 0;
}

static void clamp(void)                     /* .text+0x6c88 */
{
    if ((int16_t)AX < (int16_t)0xc001)
        SET_AX(0xc001);
    if ((int16_t)AX > 0x3fff)
        SET_AX(0x3fff);
}

/* the throttle has a floor: 0 in the air, and the reverser's +0xb00 on the
 * ground; the others stop at 0 */
static void engine_store(uint32_t which)
{
    if (which == 0) {
        if (GB(0x366) == 0) {
            if ((int16_t)AX < 0)
                SET_AX(0);
        } else if ((int16_t)AX < (int16_t)GW(0xb00)) {
            SET_AX(GW(0xb00));
        }
    } else {
        SET_DX((int16_t)AX < 0 ? 0xffff : 0);
        SET_DX(~DX);
        SET_AX(AX & DX);
    }
    M16(ebx) = AX;
}

/* .text+0x5d64 -- Linkage +0x38: field `which` of every selected engine by w */
static void engines_by(uint32_t w, uint32_t which)
{
    int carry;
    SAVE_REGS;

    edi = G;
    SET_CX(8);
    do {
        if (!engine_field(CX, which)) {
            SET_AX((uint16_t)w + M16(ebx));
            clamp();
            engine_store(which);
        }
        carry = CX & 1;
        SET_CX(CX >> 1);
    } while (!carry);
    RESTORE_REGS;
}

/* .text+0x5dd3 -- Linkage +0x3c: ... to w */
static void engines_to(uint32_t w, uint32_t which)
{
    int carry;
    SAVE_REGS;

    edi = G;
    SET_CX(8);
    do {
        if (!engine_field(CX, which)) {
            SET_AX(w);
            clamp();
            engine_store(which);
        }
        carry = CX & 1;
        SET_CX(CX >> 1);
    } while (!carry);
    RESTORE_REGS;
}

/* a key's step: 0x80 (0xff80), shifted by +0x32c when +0x328 asks for it
 * (bit `mask`), else 0x200 while the key repeats */
static void step_or_fast(uint32_t base, uint32_t extra, uint32_t mask)
{
    uint8_t was;

    eax = base;
    if (GD(0x328) & mask) {
        ecx = GD(0x32c);
        eax <<= CL & 31;
    } else {
        was = V8(S_REPEAT);
        V8(S_REPEAT) = 8;
        if (was != 0)
            eax += extra;
    }
}

static void h_5b54(void) { eax = V32(S_VALUE); engines_to(AX, 0); }  /* 0x100a1 */
static void h_5b66(void) { engines_to(0x3fff, 0); }        /* THROTTLE_FULL */
static void h_5b73(void) { engines_by(0x80, 0); }          /* THROTTLE_INCR */
static void h_5b80(void)                    /* THROTTLE_INCR_SMALL */
{
    step_or_fast(0x80, 0x180, 1);
    engines_by(AX, 0);
}
static void h_5bbd(void)                    /* THROTTLE_DECR */
{
    step_or_fast(0xffffff80u, 0xfffffe80u, 1);
    engines_by(AX, 0);
}
static void h_5bfa(void) { engines_to(0, 0); }             /* THROTTLE_CUT */
static void h_5c04(void) { eax = V32(S_VALUE); engines_to(AX, 1); }  /* PROP_PITCH_SET */
static void h_5c16(void) { engines_to(0x3fff, 1); }        /* PROP_PITCH_LO */
static void h_5c23(void) { engines_by(0x80, 1); }          /* PROP_PITCH_INCR */
static void h_5c30(void)                    /* PROP_PITCH_INCR_SMALL */
{
    step_or_fast(0x80, 0x180, 0x10);
    engines_by(AX, 1);
}
static void h_5c6d(void)                    /* PROP_PITCH_DECR */
{
    step_or_fast(0xffffff80u, 0xfffffe80u, 0x10);
    engines_by(AX, 1);
}
static void h_5caa(void) { engines_to(0, 1); }             /* PROP_PITCH_HI */
static void h_5cb4(void) { eax = V32(S_VALUE); engines_to(AX, 2); }  /* MIXTURE_SET */
static void h_5cc6(void) { engines_to(0x3fff, 2); }        /* MIXTURE_RICH */
static void h_5cd3(void) { engines_by(0x80, 2); }          /* MIXTURE_INCR */
static void h_5ce0(void)                    /* MIXTURE_INCR_SMALL */
{
    step_or_fast(0x80, 0x180, 0x20);
    engines_by(AX, 2);
}
static void h_5d1d(void)                    /* MIXTURE_DECR */
{
    step_or_fast(0xffffff80u, 0xfffffe80u, 0x20);
    engines_by(AX, 2);
}
static void h_5d5a(void) { engines_to(0, 2); }             /* MIXTURE_LEAN */

/* .text+0x5ed8 -- the magnetos of every selected engine by w, to 0..limit;
 * for a jet (+0x609 1) the starter, a three-way switch that rolls over */
static void magnetos_by(uint32_t w, uint32_t limit)
{
    int carry;
    uint16_t bx;

    SET_CX(8);
    do {
        engine_selected(CX);
        if (eax != 0) {
            if (GB(0x609) == 1) {
                M16(eax + 6) += 1;
                if (M16(eax + 6) == 3)
                    M16(eax + 6) += 0xfffd;
            }
            bx = (uint16_t)w + M16(eax + 6);
            if ((int16_t)bx < 0)
                bx = 0;
            else if ((int16_t)bx > (int16_t)limit)
                bx = (uint16_t)limit;
            if (GB(0x609) == 1) {
                bx -= 1;
                if (bx == 0xffff)
                    bx += 3;
            }
            SET_BX(bx);
            M16(eax + 6) = bx;
        }
        carry = CX & 1;
        SET_CX(CX >> 1);
    } while (!carry);
}

/* .text+0x5f4a */
static void engines_toggle_heat(void)
{
    int carry;

    SET_CX(8);
    do {
        engine_selected(CX);
        if (eax != 0) {
            M16(eax + 0x26) &= 1;
            M16(eax + 0x26) ^= 1;
        }
        carry = CX & 1;
        SET_CX(CX >> 1);
    } while (!carry);
}

/* ---- the axes: .text+0x5f6d..+0x6658 -----------------------------------------------
 *
 * GLOBAL +0xbb2 is the elevator, +0xbb6 the ailerons, +0xbba the rudder,
 * +0xbc0/+0xbc2 the elevator trim, all -0x3fff..0x3fff.  A key or the mouse
 * moves them through a curve (.text+0x6edf) that softens the step with the
 * airspeed; the curve's knees are +0x18's six words.  The keypad's keys
 * double as the views while a view key last moved the eye (+0xeb). */

/* .text+0x6edf -- the curve: w scaled by a factor from the airspeed (+0x2b8
 * >> 7) against the knees lo and hi.  Divides as retail, repaired. */
static void idiv16(uint16_t d)
{
    int64_t n = (int32_t)(((uint32_t)DX << 16) | AX), q;

    if (d != 0) {
        q = n / (int16_t)d;
        if (q >= -0x8000 && q <= 0x7fff) {
            SET_AX(q);
            SET_DX(n % (int16_t)d);
            return;
        }
    }
    SET_AX(((int16_t)d < 0) != ((int16_t)DX < 0) ? 0x8001 : 0x7fff);
    SET_DX(0x7fff);
}

static void imul16(uint16_t b)              /* dx:ax = ax * b */
{
    uint32_t p = (uint32_t)((int32_t)(int16_t)AX * (int16_t)b);

    SET_AX(p);
    SET_DX(p >> 16);
}

static void sar_dx_rcr_ax(void)             /* sar dx,1 / rcr ax,1 */
{
    uint16_t c = DX & 1;

    SET_DX((uint16_t)((int16_t)DX >> 1));
    SET_AX((AX >> 1) | (c << 15));
}

static void curve(uint32_t w, uint32_t lo, uint32_t hi)
{
    uint32_t s;

    eax = GD(0x2b8);
    eax >>= 7;
    if ((int16_t)AX < (int16_t)lo) {
        SET_DX(AX);
        SET_AX(0);
        sar_dx_rcr_ax();
        sar_dx_rcr_ax();
        SET_BX(lo);
        idiv16(BX);
        SET_AX(-AX);
        SET_AX(AX + 0x7fff);
    } else if ((int16_t)AX < (int16_t)hi) {
        SET_AX(AX - (uint16_t)lo);
        SET_DX(AX);
        SET_AX(0);
        sar_dx_rcr_ax();
        SET_CX((uint16_t)hi - (uint16_t)lo);
        idiv16(CX);
        SET_CX(0x3000);
        imul16(CX);
        s = (uint32_t)AX + AX;
        SET_AX(s);
        SET_DX(DX + DX + (s >> 16));
        SET_AX(0x4000);
        SET_AX(AX - DX);
    } else {
        SET_AX(0x1000);
    }
    imul16((uint16_t)w);
    s = (uint32_t)AX + AX;
    SET_AX(s);
    SET_DX(DX + DX + (s >> 16));
    s = (uint32_t)AX + AX;
    SET_AX(s);
    SET_DX(DX + (s >> 16));
    SET_AX(DX);
}

static void elevator_store(void) { GW(0xbb2) = AX; }       /* .text+0x6109 */
static void aileron_store(void) { GW(0xbb6) = AX; }        /* .text+0x62b1 */
static void rudder_store(void) { GW(0xbba) = AX; }         /* .text+0x6340 */
static void set_elevator(void) { clamp(); elevator_store(); }
static void set_aileron(void) { clamp(); aileron_store(); }
static void set_rudder(void) { clamp(); rudder_store(); }

/* .text+0x60bf -- Linkage +0x2c: the elevator by w, through the curve */
static void elevator_by(uint32_t w)
{
    SAVE_REGS;

    edi = G;
    curve(w, V16(0xf1), V16(0xef));
    SET_AX(AX + GW(0xbb2));
    set_elevator();
    RESTORE_REGS;
}

/* .text+0x6267 -- Linkage +0x28: the ailerons */
static void aileron_by(uint32_t w)
{
    SAVE_REGS;

    edi = G;
    curve(w, V16(0xf5), V16(0xf3));
    SET_AX(AX + GW(0xbb6));
    set_aileron();
    RESTORE_REGS;
}

/* .text+0x62f6 -- Linkage +0x30: the rudder */
static void rudder_by(uint32_t w)
{
    SAVE_REGS;

    edi = G;
    curve(w, V16(0xf9), V16(0xf7));
    SET_AX(AX + GW(0xbba));
    set_rudder();
    RESTORE_REGS;
}

/* .text+0x6015 -- the elevator trim to ax; the elevator's curve reapplied */
static void trim_set(void)
{
    clamp();
    GW(0xbc0) = AX;
    GW(0xbc2) = AX;
    elevator_by(0);
}

/* .text+0x5ff2 -- Linkage +0x34: the trim by w */
static void trim_by(uint32_t w)
{
    SAVE_REGS;

    edi = G;
    SET_AX((uint16_t)w + GW(0xbc0));
    trim_set();
    RESTORE_REGS;
}

static void elevator_key(void)              /* .text+0x60ad: ax times +0xc04 */
{
    imul16(GW(0xc04));
    elevator_by(AX);
}

static void aileron_key(void)               /* .text+0x6255 */
{
    imul16(GW(0xc02));
    aileron_by(AX);
}

static void rudder_key(void)                /* .text+0x62e4 */
{
    imul16(GW(0xc06));
    rudder_by(AX);
}

/* .text+0x5f6d -- VIEW: the pan's heading and pitch back to the front */
static void h_5f6d(void)
{
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    M16(ecx + 0x52) = 0;
    M16(ecx + 0x50) = 0;
    V32(S_EYE_MOVED) = 1;
}

/* .text+0x6ba6 -- if a view key last moved the eye, look direction ebx/2
 * (eighths, from ahead) and say so: eax 1; else eax 0 */
static void view_look(void)
{
    eax = 0;
    if (V32(S_EYE_MOVED) == 0)
        return;
    V32(S_EYE_MOVED) = 0;
    ebx >>= 1;
    pilot_event(0xa, BX);
    view_aim(ebx, 1);
    eax = 1;
}

static void view_dir(void)                  /* .text+0x6b9b */
{
    h_5f6d();
    view_look();
}

/* the keypad: a view while the eye is a view key's, else the controls */
static void trim_up_key(void);
static void elevator_down_key(void);
static void aileron_left_key(void);
static void centre_key(void);
static void aileron_right_key(void);
static void trim_down_key(void);
static void elevator_up_key(void);

static void h_5f97(void) { ebx = 0xe; view_look(); if (eax == 0) trim_up_key(); }  /* NUMPAD7 */
static void h_5fac(void) { ebx = 0xe; view_dir(); }        /* VIEW_FORWARD_LEFT */
static void h_6059(void) { ebx = 0; view_look(); if (eax == 0) elevator_down_key(); }  /* NUMPAD8 */
static void h_606e(void) { ebx = 0; view_dir(); }          /* VIEW_FORWARD */
static void h_6126(void) { ebx = 2; view_look(); if (eax == 0) h_5b80(); }   /* NUMPAD9 */
static void h_613b(void) { ebx = 2; view_dir(); }          /* VIEW_FORWARD_RIGHT */
static void h_6150(void) { ebx = 0xc; view_look(); if (eax == 0) aileron_left_key(); }  /* NUMPAD4 */
static void h_6165(void) { ebx = 0xc; view_dir(); }        /* VIEW_LEFT */
static void h_6190(void) { ebx = 0x10; view_look(); if (eax == 0) centre_key(); }  /* NUMPAD5 */
static void h_61a5(void) { ebx = 0x10; view_dir(); }       /* VIEW_DOWN */
static void h_622a(void) { ebx = 4; view_look(); if (eax == 0) aileron_right_key(); }  /* NUMPAD6 */
static void h_623f(void) { ebx = 4; view_dir(); }          /* VIEW_RIGHT */
static void h_6348(void) { ebx = 0xa; view_look(); if (eax == 0) trim_down_key(); }  /* NUMPAD1 */
static void h_635d(void) { ebx = 0xa; view_dir(); }        /* VIEW_REAR_LEFT */
static void h_63ad(void) { ebx = 8; view_look(); if (eax == 0) elevator_up_key(); }  /* NUMPAD2 */
static void h_63c2(void) { ebx = 8; view_dir(); }          /* VIEW_REAR */
static void h_6416(void) { ebx = 6; view_look(); if (eax == 0) h_5bbd(); }   /* NUMPAD3 */
static void h_642b(void) { ebx = 6; view_dir(); }          /* VIEW_REAR_RIGHT */

static void trim_up_key(void)               /* .text+0x5fb7 */
{
    step_or_fast(0xffffffe0u, 0xffffff20u, 4);
    trim_by(AX);
}

static void trim_down_key(void)             /* .text+0x6368 */
{
    step_or_fast(0x20, 0xe0, 4);
    trim_by(AX);
}

static void elevator_down_key(void)         /* .text+0x6079 */
{
    step_or_fast(0xfffffff8u, 0xffffffe8u, 2);
    elevator_key();
}

static void elevator_up_key(void)           /* .text+0x63cd */
{
    step_or_fast(8, 0x18, 2);
    elevator_key();
}

static void aileron_left_key(void)          /* .text+0x6170 */
{
    eax = 0xfffffff0u;
    aileron_key();
}

static void aileron_right_key(void)         /* .text+0x624a */
{
    eax = 0x10;
    aileron_key();
}

static void centre_key(void)                /* .text+0x61b0: the auto-centred axes */
{
    if (V32(0xfb) != 0) {
        SET_AX(0);
        set_aileron();
    }
    if (V32(0xff) != 0) {
        SET_AX(0);
        set_rudder();
    }
    if (V32(0x103) != 0) {
        SET_AX(0);
        set_elevator();
    }
}

static void h_5f8c(void) { eax = V32(S_VALUE); trim_set(); }       /* 0x100aa */
static void h_604e(void) { eax = V32(S_VALUE); set_elevator(); }   /* 0x1009e */
static void h_621f(void) { eax = V32(S_VALUE); aileron_store(); }  /* 0x1009f */
static void h_62ce(void) { eax = V32(S_VALUE); rudder_store(); }   /* 0x100a0 */
static void h_61e4(void) { SET_AX(0); set_rudder(); }              /* 0x10099 */
static void h_6079(void) { elevator_down_key(); }                  /* 0x10093 */
static void h_63cd(void) { elevator_up_key(); }                    /* 0x10094 */
static void h_6170(void) { aileron_left_key(); }                   /* 0x10095 */
static void h_61b0(void) { centre_key(); }                         /* 0x10096 */
static void h_624a(void) { aileron_right_key(); }                  /* 0x10097 */
static void h_62d9(void) { eax = 0x10; rudder_key(); }             /* RUDDER_RIGHT */
static void h_6440(void) { eax = 0xfffffff0u; rudder_key(); }      /* RUDDER_LEFT */

/* slew: GLOBAL +0x5e4 bank, +0x5e6 heading, +0x5e8 altitude, +0x5ea ahead,
 * +0x5ec sideways, +0x5ee pitch.  A step that overflows is undone. */
static void slew_step(uint32_t at, uint16_t by)
{
    uint16_t a = GW(at), r = a + by;

    GW(at) = r;
    if ((int16_t)((a ^ r) & (by ^ r)) < 0)
        GW(at) -= by;
}

static void h_6030(void) { GW(0x5e4) += 0x80; }            /* SLEW_BANK_MINUS */
static void h_6146(void) { GW(0x5e4) -= 0x80; }            /* SLEW_BANK_PLUS */
static void h_63a3(void) { GW(0x5e6) += 0x80; }            /* SLEW_HEADING_MINUS */
static void h_6436(void) { GW(0x5e6) -= 0x80; }            /* SLEW_HEADING_PLUS */
static void h_6111(void) { slew_step(0x5ea, 0xff00); }     /* SLEW_AHEAD_PLUS */
static void h_6401(void) { slew_step(0x5ea, 0x100); }      /* SLEW_AHEAD_MINUS */
static void h_617b(void) { slew_step(0x5ec, 0xff00); }     /* SLEW_LEFT */
static void h_62b9(void) { slew_step(0x5ec, 0x100); }      /* SLEW_RIGHT */

static void h_603a(void)                    /* SLEW_RESET */
{
    GW(0x5de) = 1;
    GD(0x5e0) = 1;
}

static void h_61ed(void)                    /* SLEW_FREEZE */
{
    SET_AX(0);
    GW(0x5ea) = AX;
    GW(0x5e8) = 0x4000;
    GW(0x5ee) = 0x4000;
    GW(0x5e4) = AX;
    GW(0x5e6) = AX;
    GW(0x5ec) = AX;
}

/* ---- the mouse as the yoke: .text+0x644b..+0x6658 --------------------------------- */

void mouse_body(uint32_t dx, uint32_t dy, uint32_t flags);
ENTRY3(mouse_entry, mouse_body);

/* .text+0x64d1 -- the host's capture on or off */
static void yoke(uint32_t on)
{
    if (on != 0)
        XCALL(SVC(HOST, 0x274), ADDR(mouse_entry));
    else
        XCALL0(SVC(HOST, 0x278));
}

static void h_644b(void)                    /* MOUSE_AS_YOKE_TOGGLE */
{
    if (V32(S_NO_YOKE) == 0) {
        V32(S_YOKE) ^= 1;
        yoke(V32(S_YOKE));
    }
}

static void h_6467(void)                    /* MOUSE_AS_YOKE_OFF */
{
    if (V32(S_NO_YOKE) == 0) {
        V32(S_YOKE) = 0;
        yoke(V32(S_YOKE));
    }
}

static void h_6486(void)                    /* MOUSE_AS_YOKE_ON */
{
    if (V32(S_NO_YOKE) == 0) {
        V32(S_YOKE) = 1;
        yoke(V32(S_YOKE));
    }
}

/* .text+0x64a5, +0x64b9 -- Linkage +0x1c and +0x20: suspend and resume */
void yoke_suspend(void)
{
    SAVE_REGS;

    edi = G;
    yoke(0);
    RESTORE_REGS;
}

void yoke_resume(void)
{
    SAVE_REGS;

    edi = G;
    yoke(V32(S_YOKE));
    RESTORE_REGS;
}

static void h_64a5(void) { yoke_suspend(); }  /* MOUSE_AS_YOKE_SUSPEND */
static void h_64b9(void) { yoke_resume(); }   /* MOUSE_AS_YOKE_RESUME */

/* .text+0x6659 -- the brakes to ax; below 0 they are off, at 0x3fff full */
static void set_brakes(void)
{
    V16(S_BRAKES) = AX;
    if ((int16_t)AX < 0) {
        SET_AX(0);
        GW(0xc00) = 0;
        GW(0xbc8) = 0;
    }
    GW(0xbc4) = AX;
    GW(0xbc6) = AX;
    GW(0xbca) = AX;
    if (AX == 0x3fff) {
        GW(0xbc8) = 0xc8c8;
        GW(0xc00) = 0xc8c8;
    }
}

/* .text+0x652f -- the host's mouse callback (dx, dy, buttons).  The right
 * button lets go; the left moves the brakes and throttle instead; in slew
 * the motion slews.  The ailerons' step is four times as big outside the
 * band +0xc0c << 5 about the centre. */
void mouse_body(uint32_t dx, uint32_t dy, uint32_t flags)
{
    SAVE_REGS;

    edi = G;
    if (flags & 2) {
        XCALL(SVC(HOST, 0x3c), 0x100f6, 0);
    } else if (flags & 1) {
        if (GW(0x5dc) == 0) {
            eax = dx;
            if (eax != 0) {
                SET_AX(AX << 5);
                SET_AX(AX + V16(S_BRAKES));
                clamp();
                set_brakes();
            }
            eax = dy;
            if (eax != 0) {
                SET_AX(AX << 6);
                engines_by(AX, 0);
            }
        } else {
            h_61ed();
        }
    } else {
        eax = dx;
        if (eax != 0) {
            if (GW(0x5dc) == 0) {
                int64_t p;

                eax = -eax;
                edx = GW(0xc0a);
                p = (int64_t)(int32_t)eax * (int32_t)edx;
                eax = (uint32_t)p;
                edx = (uint32_t)((uint64_t)p >> 32);
                eax = (uint32_t)((int32_t)eax >> 2);
                ecx = eax;
                SET_AX(GW(0xc0c) << 5);
                if ((int16_t)AX < (int16_t)GW(0xbb6)) {
                    ecx <<= 2;
                } else {
                    SET_AX(-AX);
                    if (!((int16_t)AX < (int16_t)GW(0xbb6)))
                        ecx <<= 2;
                }
                aileron_by(CX);
            } else {
                GW(0x5e6) += AX;
            }
        }
        eax = dy;
        if (eax != 0) {
            if (GW(0x5dc) == 0) {
                int64_t p;

                eax = -eax;
                edx = GW(0xc08);
                p = (int64_t)(int32_t)eax * (int32_t)edx;
                eax = (uint32_t)p;
                edx = (uint32_t)((uint64_t)p >> 32);
                eax = (uint32_t)((int32_t)eax >> 2);
                elevator_by(AX);
            } else {
                eax <<= 4;
                GW(0x5ea) -= AX;
            }
        }
    }
    RESTORE_REGS;
}

/* ---- slew altitude and pitch, the magnetos: .text+0x66a7..+0x6771 ----------------- */

static void h_66a7(void) { GW(0x5e8) = 0; }                /* SLEW_ALTIT_UP_FAST */
static void h_66b1(void) { GW(0x5e8) = 0x3e00; }           /* SLEW_ALTIT_UP_SLOW */
static void h_66bb(void) { GW(0x5e8) = 0x4000; }           /* SLEW_ALTIT_FREEZE */
static void h_66c5(void) { GW(0x5e8) = 0x4200; }           /* SLEW_ALTIT_DN_SLOW */
static void h_66cf(void) { GW(0x5e8) = 0x7e00; }           /* SLEW_ALTIT_DN_FAST */

static void h_66d9(void)                    /* SLEW_ALTIT_PLUS */
{
    GW(0x5e8) += 0xfe00;
    if ((int16_t)GW(0x5e8) < 0)
        h_66a7();
}

static void h_66e5(void)                    /* SLEW_ALTIT_MINUS */
{
    GW(0x5e8) += 0x200;
    if ((int16_t)GW(0x5e8) < 0)
        h_66cf();
}

static void h_66f1(void) { GW(0x5ee) = 0x7c00; }           /* SLEW_PITCH_DN_FAST */
static void h_66fb(void) { GW(0x5ee) = 0x6000; }           /* SLEW_PITCH_DN_SLOW */
static void h_6705(void) { GW(0x5ee) = 0x4000; }           /* SLEW_PITCH_FREEZE */
static void h_670f(void) { GW(0x5ee) = 0x2000; }           /* SLEW_PITCH_UP_SLOW */
static void h_6719(void) { GW(0x5ee) = 0x200; }            /* SLEW_PITCH_UP_FAST */

static void h_6723(void)                    /* SLEW_PITCH_PLUS */
{
    GW(0x5ee) += 0xfe00;
    if ((int16_t)GW(0x5ee) < 0)
        h_6719();
}

static void h_672f(void)                    /* SLEW_PITCH_MINUS */
{
    GW(0x5ee) += 0x200;
    if ((int16_t)GW(0x5ee) < 0)
        h_66f1();
}

static void magnetos_key(void)              /* .text+0x674f */
{
    if (GB(0x609) == 0)
        SET_DX(4);
    else
        SET_DX(2);
    magnetos_by(AX, DX);
}

static void h_673b(void) { SET_AX(0xffff); magnetos_key(); }       /* MAGNETO_LEFT */
static void h_6745(void) { SET_AX(1); magnetos_key(); }            /* MAGNETO_RIGHT */

/* ---- the ADF: .text+0x6772..+0x69ef ------------------------------------------------
 *
 * GLOBAL +0x34c is the ADF in BCD, hundreds in the top byte; +0x356 its
 * extra digits when the aircraft's ADF has them (+0x35c): ch the thousands,
 * cl the tenths.  Failed (+0xb64), it does not change. */

static void tune_adf(void)                  /* .text+0x6805 */
{
    GB(0x389) &= 0x7f;
    GB(0x389) |= 2;
    pilot_event(1, GW(0x34c));
}

static void adf_store(void)                 /* .text+0x67ef */
{
    if (GB(0xb64) == 0) {
        GW(0x34c) = AX;
        tune_adf();
    }
}

static void adf_store_both(void)            /* .text+0x67d9 */
{
    if (GB(0xb64) == 0) {
        GW(0x356) = CX;
        adf_store();
    }
}

static void adf_tens(void)                  /* .text+0x683d */
{
    if (AL > 0x9f)
        SET_AL(AL & 0xf);
    adf_store();
}

static void h_6772(void) { GW(0x34c) = CX; tune_adf(); }   /* 0x100b2 */

static void h_677f(void)                    /* ADF_100_INC */
{
    GB(0x288) = 0xf;
    if (GD(0x35c) == 0) {
        SET_AX(GW(0x34c));
        SET_AH(AH + 1);
        if (AH > 9)
            SET_AH(2);
        adf_store();
        return;
    }
    SET_CX(GW(0x356));
    if (CH == 0) {
        SET_AX(GW(0x34c));
        SET_AH(AH + 1);
        if (AH <= 9) {
            adf_store();
            return;
        }
        SET_CH(1);
        SET_AH(0);
        adf_store_both();
        return;
    }
    SET_AX(GW(0x34c));
    SET_AH(AH + 1);
    if (AH <= 6) {
        adf_store();
        return;
    }
    SET_CH(0);
    SET_AH(2);
    adf_store_both();
}

static void h_6825(void)                    /* ADF_10_INC */
{
    GB(0x288) = 0x10;
    SET_AX(GW(0x34c) + 0x10);
    adf_tens();
}

static void h_6849(void)                    /* ADF_1_INC */
{
    if (GB(0xb64) != 0)
        return;
    GB(0x288) = 0x11;
    SET_AX(GW(0x34c));
    if (GD(0x35c) != 0) {
        SET_CX(GW(0x356));
        SET_CL(CL + 1);
        if (CL != 0xa)
            goto store;
    }
    SET_CL(0);
    SET_AX(AX + 1);
store:
    GW(0x356) = CX;
    SET_BL(AL & 0xf);
    if (BL <= 9) {
        adf_store();
        return;
    }
    SET_AL(AL & 0xf0);
    adf_tens();
}

static void h_6898(void)                    /* ADF_100_DEC */
{
    GB(0x288) = 0xf;
    if (GD(0x35c) == 0) {
        SET_AX(GW(0x34c));
        SET_AH(AH - 1);
        if (AH < 2)
            SET_AH(9);
        adf_store();
        return;
    }
    SET_CX(GW(0x356));
    if (CH == 0) {
        SET_AX(GW(0x34c));
        SET_AH(AH - 1);
        if (AH >= 2) {
            adf_store();
            return;
        }
        SET_CH(1);
        SET_AH(6);
        adf_store_both();
        return;
    }
    SET_AX(GW(0x34c));
    SET_AH(AH - 1);
    if (!(AH & 0x80)) {
        adf_store();
        return;
    }
    SET_CH(0);
    SET_AH(9);
    adf_store_both();
}

static void h_6903(void)                    /* ADF_10_DEC */
{
    GB(0x288) = 0x10;
    SET_AX(GW(0x34c));
    if ((AX & 0xf0) == 0)
        SET_AX(AX | 0xa0);
    SET_AX(AX - 0x10);
    adf_store();
}

static void h_6925(void)                    /* ADF_1_DEC */
{
    if (GB(0xb64) != 0)
        return;
    GB(0x288) = 0x11;
    SET_AX(GW(0x34c));
    SET_CL(0);
    if (GD(0x35c) != 0) {
        SET_CX(GW(0x356));
        SET_CL(CL - 1);
        if (!(CL & 0x80)) {
            GW(0x356) = CX;
            SET_BL(AL & 0xf);
            if (BL <= 9) {
                adf_store();
                return;
            }
            SET_AL(AL & 0xf0);
            adf_tens();
            return;
        }
        SET_CL(9);
    }
    GW(0x356) = CX;
    if ((AX & 0xf) == 0)
        SET_AX(AX | 0xa);
    SET_AX(AX - 1);
    adf_store();
}

static void h_698b(void)                    /* ADF_CARD_INC */
{
    if (fast())
        GD(0xc6c) += 9;
    GD(0xc6c)++;
    if ((int32_t)GD(0xc6c) > 0x168)
        GD(0xc6c) -= 0x168;
}

static void h_69bf(void)                    /* ADF_CARD_DEC */
{
    if (fast())
        GD(0xc6c) -= 9;
    GD(0xc6c)--;
    if ((int32_t)GD(0xc6c) < 0)
        GD(0xc6c) += 0x168;
}

/* ---- the clock: .text+0x69f0..+0x6b30 ----------------------------------------------
 *
 * GLOBAL +0x238 hours, +0x239 minutes, +0x23a seconds; +0x23b, +0x23c the
 * same for the local clock, which carries into the day of the year (+0x23e)
 * and the year (+0x240). */

static void clock_changed(void) { XCALL0(SVC(MAIN, 0xa4)); }      /* .text+0x6b0b */

static void h_69f0(void)                    /* CLOCK_HOURS_DEC */
{
    GB(0x238)--;
    if (GB(0x238) & 0x80)
        GB(0x238) = 0x17;
    GB(0x23b)--;
    if (GB(0x23b) & 0x80) {
        GB(0x23b) = 0x17;
        GW(0x23e)--;
        if ((int16_t)GW(0x23e) <= 0) {
            GW(0x240)--;
            GW(0x23e) = 0x16d;
            if ((GW(0x240) & 3) == 0)
                GW(0x23e)++;
        }
    }
    clock_changed();
}

static void h_6a47(void)                    /* CLOCK_HOURS_INC */
{
    GB(0x238)++;
    if ((int8_t)GB(0x238) >= 0x18)
        GB(0x238) = 0;
    GB(0x23b)++;
    if ((int8_t)GB(0x23b) >= 0x18) {
        GB(0x23b) = 0;
        GW(0x23e)++;
        if ((int16_t)GW(0x23e) >= 0x16e &&
            ((int16_t)GW(0x23e) > 0x16e || (GW(0x240) & 3) != 0)) {
            GW(0x240)++;
            GW(0x23e) = 1;
        }
    }
    clock_changed();
}

static void h_6aa8(void)                    /* CLOCK_MINUTES_DEC */
{
    GB(0x239)--;
    if (GB(0x239) & 0x80)
        GB(0x239) = 0x3b;
    GB(0x23c)--;
    if (GB(0x23c) & 0x80)
        GB(0x23c) = 0x3b;
    clock_changed();
}

static void h_6acc(void)                    /* CLOCK_MINUTES_INC */
{
    GB(0x239)++;
    if (GB(0x239) >= 0x3c)
        GB(0x239) = 0;
    GB(0x23c)++;
    if ((int8_t)GB(0x23c) >= 0x3c)
        GB(0x23c) = 0;
    clock_changed();
}

static void h_6afe(void)                    /* CLOCK_SECONDS_ZERO */
{
    GB(0x23a) = 0;
    clock_changed();
}

/* ---- views and zoom: .text+0x6b31..+0x6ead ----------------------------------------- */

static void nothing(void) { }               /* .text+0x6eae, +0x6eaf, +0x6eb0 */

static void view_set_mode(void)             /* .text+0x6b47 */
{
    XCALL(SVC(WINDOW, 0x60), V32(S_VALUE));
}

static void h_6b31(void) { nothing(); view_set_mode(); }  /* 0x1009b */
static void h_6b3c(void) { nothing(); view_set_mode(); }  /* 0x100ae */

static void view_dir_value(void)            /* .text+0x6b92 */
{
    SET_BX(BX + BX);
    view_dir();
}

static void h_6b70(void) { nothing(); ebx = V32(S_VALUE); view_dir_value(); }  /* 0x1009d */
static void h_6b81(void) { nothing(); ebx = V32(S_VALUE); view_dir_value(); }  /* 0x100af */

/* .text+0x6bda -- aim the active view in direction `dir` (eighths); the 3D
 * eyes through WINDOW +0x24, and with `record` WINDOW +0x58 hears of the
 * mode; the spot plane is put at heading dir << 13.  edi is left GLOBAL, ecx
 * is kept. */
static void view_aim(uint32_t dir, uint32_t record)
{
    uint32_t keep_ecx = ecx;
    int carry;

    edi = G;
    ecx = GD(0x11a0);
    if (ecx != 0) {
        ebx = dir;
        eax = M32(ecx + 0x14);
        if (eax == 1 || eax == 0) {
            edx = ebx;
            XCALL(SVC(WINDOW, 0x24), ecx, edx);
            GB(0x370) = 1;
            if (record != 0) {
                ecx = GD(0x11a0);
                eax = M32(ecx + 0x14);
                ebx = GD(0x119c);
                XCALL(SVC(WINDOW, 0x58), ebx, eax);
            }
        } else if (eax == 4) {
            carry = (BX >> 3) & 1;
            SET_BX(BX << 13);
            if (carry)
                M16(ecx + 0x28) = 0x8001;
            else
                M16(ecx + 0x28) = BX;
        }
    }
    ecx = keep_ecx;
}

/* .text+0x6d75 -- ebx to the active view's zoom word and ax its value, by
 * its kind (+0x14: 0..4 at +0x1c, +0x1e, +0x20, +0x22, +0x24); ebx 0 when
 * there is none, and the callers that store through it fault, as retail */
static void zoom_get(void)
{
    ebx = 0;
    ecx = GD(0x11a0);
    if (ecx == 0)
        return;
    if (M32(ecx + 0x14) == 0) {
        ebx = ecx + 0x1c;
        SET_AX(M16(ecx + 0x1c));
    } else if (M32(ecx + 0x14) == 1) {
        ebx = ecx + 0x1e;
        SET_AX(M16(ecx + 0x1e));
    } else if (M32(ecx + 0x14) == 4) {
        ebx = ecx + 0x24;
        SET_AX(M16(ecx + 0x24));
    } else if (M32(ecx + 0x14) == 2) {
        ebx = ecx + 0x20;
        SET_AX(M16(ecx + 0x20));
    } else if (M32(ecx + 0x14) == 3) {
        ebx = ecx + 0x22;
        SET_AX(M16(ecx + 0x22));
    }
}

/* .text+0x6df2 -- Linkage +0x24: the zoom recorded, and its text into
 * GLOBAL +0xc1c without leading zeros.  The frame is retail's: the value
 * two bytes above the end of the text. */
void zoom_report(void)
{
    struct {
        char text[10];
        uint16_t value;
    } f;
    SAVE_REGS;

    edi = G;
    zoom_get();
    f.value = AX;
    pilot_event(0xc, f.value);
    XCALL(SVC(NUMERIC, 0x18), ADDR(&f.value), 0x13, ADDR(f.text));
    ebx = ADDR(f.text);
    while (M8(ebx) == 0x30)
        ebx++;
    SET_AL(M8(ebx));
    GB(0xc1c) = AL;
    SET_AL(M8(ebx + 1));
    GB(0xc1d) = AL;
    SET_AL(M8(ebx + 2));
    GB(0xc1e) = AL;
    RESTORE_REGS;
}

static void zoom_set(void)                  /* .text+0x6d1a */
{
    M16(ebx) = AX;
    GB(0x288) = 2;
    zoom_report();
}

static void zoom_step(void)                 /* .text+0x6d2a: 1/128 of it, and 1 */
{
    zoom_get();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax = (uint32_t)((int32_t)eax >> 7);
    eax += 1;
}

static void zoom_out_fine(void)             /* .text+0x6ce8 */
{
    zoom_step();
    SET_CX(AX);
    SET_AX(M16(ebx));
    SET_AX(AX - CX);
    if ((int16_t)AX < 0x10)
        SET_AX(0x10);
    zoom_set();
}

static void zoom_in_fine(void)              /* .text+0x6d06 */
{
    zoom_step();
    SET_AX(AX + M16(ebx));
    if ((int16_t)AX < 0)
        SET_AX(0x7fff);
    zoom_set();
}

static void zoom_halve(void)                /* .text+0x6d39, ZOOM_IN */
{
    zoom_get();
    if (AX >= 0x7fff) {
        SET_AX(0x4000);
    } else {
        SET_AX(AX >> 1);
        if ((int16_t)AX < 0x10)
            SET_AX(0x10);
    }
    zoom_set();
}

/* .text+0x6d5d, ZOOM_OUT -- `rcl ax,1` with the carry the kind tests in
 * zoom_get leave, which is clear on every path that reaches it */
static void zoom_double(void)
{
    zoom_get();
    SET_AX(AX << 1);
    if (AX > 0x7fff)
        SET_AX(0x7fff);
    zoom_set();
}

static void h_6d06(void) { zoom_in_fine(); }        /* ZOOM_IN_FINE */
static void h_6ce8(void) { zoom_out_fine(); }       /* ZOOM_OUT_FINE */
static void h_6d39(void) { zoom_halve(); }          /* ZOOM_IN */
static void h_6d5d(void) { zoom_double(); }         /* ZOOM_OUT */

static void zoom_to(uint32_t v)             /* .text+0x6cbf */
{
    zoom_get();
    eax = v;
    M16(ebx) = AX;
    zoom_report();
}

static void h_6c9d(void) { nothing(); zoom_to(V32(S_VALUE)); }     /* 0x100b0 */
static void h_6cae(void) { nothing(); zoom_to(V32(S_VALUE)); }     /* 0x100a5 */

static void h_6cd6(void)                    /* 0x100b1 */
{
    eax = V32(S_VALUE);
    GW(0x2b2) = AX;
    nothing();
}

static void obs1_event(void) { pilot_event(8, GW(0xc4e)); }        /* .text+0x6dce */
static void obs2_event(void) { pilot_event(9, GW(0xc5e)); }        /* .text+0x6de0 */

static void h_6e6e(void) { eax = V32(S_VALUE); GW(0x600) = AX; }   /* 0x100a6 */
static void h_6e7b(void) { if (V32(S_VALUE) != 0) nothing(); }     /* 0x100a7 */
static void h_6e8c(void) { if (V32(S_VALUE) != 0) nothing(); }     /* 0x100a8 */
static void h_6e9d(void) { if (V32(S_VALUE) != 0) nothing(); }     /* 0x100a9 */

/* ---- brakes, the event, and the rest: .text+0x6f66..+0x78c1 --------------------- */

static void h_6f66(void)                    /* BRAKES_LEFT */
{
    SET_AX(GW(0xc00));
    if (AH == 0)
        SET_AH(AH + 0x32);
    SET_AH(AH + 0x28);
    brakes_hold();
}

static void h_6f7d(void)                    /* BRAKES_RIGHT */
{
    SET_AX(GW(0xc00));
    if (AL == 0)
        SET_AL(AL + 0x32);
    SET_AL(AL + 0x28);
    brakes_hold();
}

/* .text+0x6f93 -- Linkage +0x40: a pilot event for DEMO while it records
 * (+0x760).  Retail passes words, with whatever the stack held above them;
 * DEMO reads the words.  DEMO's +0x24 reads the edi it is called with:
 * GLOBAL. */
static void pilot_event(uint32_t which, uint32_t value)
{
    SAVE_REGS;

    edi = G;
    if (GD(0x760) != 0)
        XCALL(SVC(DEMO, 0x24), (uint16_t)which, (uint16_t)value);
    RESTORE_REGS;
}

void controls_event(uint32_t which, uint32_t value)
{
    pilot_event(which, value);
}

/* .text+0x6fdf -- CHAIN list 3: without a joystick, and with +0xc46 bit 1,
 * the elevator springs back a sixty-fourth (at least one) each time, except
 * while the autopilot holds the altitude */
static void coordinate(void)
{
    uint32_t keep_ebx = ebx;

    edi = G;
    if (GW(0xc46) & 2) {
        eax = controls_joystick(0, 0);
        if (eax == 0 && (GD(0x7bc) == 0 || GD(0x7d0) == 0)) {
            SET_AX(GW(0xbb2));
            SET_AX(-AX);
            SET_BX(AX);
            SET_AX((int16_t)AX >> 6);
            if (AX == 0)
                SET_AX(BX);
            SET_AX(AX + GW(0xbb2));
            set_elevator();
        }
    }
    ebx = keep_ebx;
}

/* which axes centre themselves on NUMPAD5 */
static void h_7035(void) { eax = V32(S_VALUE); V32(0xfb) = eax; }   /* CENTERING_AILERON_SET */
static void h_7040(void) { eax = V32(S_VALUE); V32(0xff) = eax; }   /* CENTERING_RUDDER_SET */
static void h_704b(void) { eax = V32(S_VALUE); V32(0x103) = eax; }  /* CENTERING_ELEVATOR_SET */

static void pause_on(void) { GD(0x264) = 1; }       /* PAUSE_ON */
static void pause_off(void) { GD(0x264) = 0; }      /* PAUSE_OFF */
static void h_7056(void) { pause_on(); }
static void h_7061(void) { pause_off(); }

/* .text+0x706c -- the CH Virtual Pilot Pro's hat: eight-byte records at
 * +0x12d: what +/- adjusts, a value, and where in GLOBAL it goes */
static void hat_apply(void)
{
    edi = G;
    SET_AL(V8(0x12d + ebx));
    SET_AH(V8(0x12e + ebx));
    ecx = V32(0x131 + ebx);
    GB(0x288) = AL;
    if (ecx != 0)
        M8(edi + ecx) = AH;
}

static void h_7093(void)                    /* CHVPP_LEFT_HAT_UP */
{
    uint32_t keep = ebx;

    edi = G;
    ebx = V32(S_CYCLE);
    if (ebx == 0xffffffffu || ebx < 8)
        ebx = 0x50;
    else
        ebx -= 8;
    V32(S_CYCLE) = ebx;
    hat_apply();
    ebx = keep;
}

static void h_70bc(void)                    /* CHVPP_LEFT_HAT_DOWN */
{
    uint32_t keep = ebx;

    edi = G;
    ebx = V32(S_CYCLE);
    if (ebx == 0xffffffffu) {
        ebx = 0;
    } else {
        ebx += 8;
        if (ebx > 0x50)
            ebx = 0;
    }
    V32(S_CYCLE) = ebx;
    hat_apply();
    ebx = keep;
}

static void h_70e8(void)                    /* CHVPP_AP_ALT_WING */
{
    edi = G;
    h_5069();
    h_5753();
}

static void h_70f9(void) { V8(S_KEYING) = 1; }      /* CENTER_NT361_CHECK */

/* .text+0x7169 -- a throttle to the value, under 0x3fff and over +0xb00 */
static void throttle_set(uint32_t p)
{
    ecx = V32(S_VALUE);
    if ((int32_t)ecx > 0x3fff)
        ecx = 0x3fff;
    if ((int16_t)CX < (int16_t)GW(0xb00))
        SET_CX(GW(0xb00));
    eax = p;
    M16(eax) = CX;
}

static void h_7135(void) { eax = edi + 0x88c; throttle_set(eax); }  /* THROTTLE1_SET */
static void h_7142(void) { eax = edi + 0x924; throttle_set(eax); }  /* THROTTLE2_SET */
static void h_714f(void) { eax = edi + 0x9bc; throttle_set(eax); }  /* THROTTLE3_SET */
static void h_715c(void) { eax = edi + 0xa54; throttle_set(eax); }  /* THROTTLE4_SET */

/* the windows: GLOBAL +0x119c is the active window, +0x11a0 its view */
static void h_7199(void) { XCALL(SVC(WINDOW, 0x40), GD(0x119c)); }     /* CLOSE_VIEW */

static void h_71c2(void)                    /* NEW_VIEW */
{
    ecx = V32(S_VALUE);
    if (ecx == 0)
        ecx = 0;
    XCALL(SVC(WINDOW, 0x3c), 0, ecx);
}

static void h_71f7(void) { XCALL(SVC(WINDOW, 0x3c), 1, 0); }           /* NEW_MAP */

static void h_721e(void)                    /* NEXT_VIEW */
{
    uint32_t next = 0;

    eax = GD(0x119c);
    if (eax != 0) {
        XCALL(SVC(WINDOW, 0x84), ADDR(&next), GD(0x119c), 3);
        XCALL(SVC(WINDOW, 0x48), next);
    }
}

static void h_7286(void)                    /* PREV_VIEW */
{
    uint32_t prev = 0;

    eax = GD(0x119c);
    if (eax != 0) {
        XCALL(SVC(WINDOW, 0x88), ADDR(&prev), GD(0x119c), 3);
        XCALL(SVC(WINDOW, 0x48), prev);
    }
}

static void h_72ee(void)                    /* VIEW_TYPE */
{
    eax = GD(0x11a0);
    if (eax == 0)
        return;
    ecx = M32(eax + 0x10) + 1;
    if ((int32_t)ecx >= 2)
        ecx = 0;
    XCALL(SVC(WINDOW, 0x5c), GD(0x119c), ecx);
}

static void h_7330(void)                    /* VIEW_TYPE_REV */
{
    eax = GD(0x11a0);
    if (eax == 0)
        return;
    ecx = M32(eax + 0x10) - 1;
    if ((int32_t)ecx < 0)
        ecx = 1;
    XCALL(SVC(WINDOW, 0x5c), GD(0x119c), ecx);
}

static void h_7372(void)                    /* VIEW_TYPE_SET */
{
    eax = GD(0x11a0);
    if (eax != 0)
        XCALL(SVC(WINDOW, 0x5c), GD(0x119c), V32(S_VALUE));
}

/* the speed and mach holds exclude each other; the autothrottle */
static void h_73ab(void)                    /* AP_AIRSPEED_HOLD */
{
    if (GD(0x76c) == 0) {
        GD(0x7dc) &= 0;
        return;
    }
    GD(0x7dc) &= 1;
    if (GD(0x7dc) == 0 && GD(0x7e4) == 1)
        GD(0x7e4) &= 0;
    GD(0x7dc) ^= 1;
}

static void h_73de(void)                    /* AP_MACH_HOLD */
{
    if (GD(0x76c) == 0) {
        GD(0x7e4) &= 0;
        return;
    }
    GD(0x7e4) &= 1;
    if (GD(0x7e4) == 0 && GD(0x7dc) == 1)
        GD(0x7dc) &= 0;
    GD(0x7e4) ^= 1;
}

static void h_7411(void)                    /* AUTO_THROTTLE_ARM */
{
    if (GD(0x774) == 0) {
        GD(0x810) &= 0;
        GD(0x80c) &= 0;
        return;
    }
    GD(0x810) &= 1;
    GD(0x810) ^= 1;
    if (GD(0x810) == 0) {
        GD(0x7dc) = 0;
        GD(0x7e4) = 0;
        GD(0x80c) = 0;
    }
}

static void h_7459(void)                    /* AUTO_THROTTLE_TO_GA */
{
    if (GD(0x774) == 0) {
        GD(0x810) &= 0;
        GD(0x80c) &= 0;
        return;
    }
    GD(0x80c) &= 1;
    GD(0x80c) ^= 1;
}

/* the radios' Morse idents, through CHAIN list 0x12: (0 off, 1 on, 2
 * toggle; VOR1, VOR2, DME1, DME2, ADF) */
static void ident(uint32_t how, uint32_t which)
{
    XCALL(SVC(CHAIN, 0x20), 0x12, how, which);
}

static void h_7481(void) { ident(0, 0); }
static void h_74aa(void) { ident(0, 1); }
static void h_74d3(void) { ident(0, 2); }
static void h_74fc(void) { ident(0, 3); }
static void h_7525(void) { ident(0, 4); }
static void h_754e(void) { ident(1, 0); }
static void h_7577(void) { ident(1, 1); }
static void h_75a0(void) { ident(1, 2); }
static void h_75c9(void) { ident(1, 3); }
static void h_75f2(void) { ident(1, 4); }
static void h_761b(void) { ident(2, 0); }
static void h_7644(void) { ident(2, 1); }
static void h_766d(void) { ident(2, 2); }
static void h_7696(void) { ident(2, 3); }
static void h_76bf(void) { ident(2, 4); }

static void h_76e8(void) { if (V32(S_VALUE) == 0) h_7481(); else h_754e(); }
static void h_76fe(void) { if (V32(S_VALUE) == 0) h_74aa(); else h_7577(); }
static void h_7714(void) { if (V32(S_VALUE) == 0) h_74d3(); else h_75a0(); }
static void h_772a(void) { if (V32(S_VALUE) == 0) h_74fc(); else h_75c9(); }
static void h_7740(void) { if (V32(S_VALUE) == 0) h_7525(); else h_75f2(); }

/* the axis commands carry the joystick's position negated; with the mouse
 * as the yoke a repeat of the last one is ignored */
static int axis_changed(uint32_t last)
{
    ecx = V32(S_VALUE);
    ecx = -ecx;
    if (V32(S_YOKE) == 0)
        return 1;
    if (ecx == V32(last))
        return 0;
    V32(last) = ecx;
    return 1;
}

static void h_7756(void)                    /* AXIS_AILERONS_SET */
{
    if (axis_changed(S_LAST_Y)) {
        eax = ecx;
        set_aileron();
    }
}

static void h_777d(void)                    /* AXIS_ELEVATOR_SET */
{
    if (axis_changed(S_LAST_X)) {
        eax = ecx;
        set_elevator();
    }
}

static void h_77a4(void)                    /* AXIS_RUDDER_SET */
{
    if (axis_changed(S_LAST_Z)) {
        eax = ecx;
        set_rudder();
    }
}

static void h_77cb(void)                    /* AXIS_THROTTLE_SET: -0x4000..0x4000 to 0..0x4000 */
{
    ecx = V32(S_VALUE);
    ecx = -ecx;
    SET_CX(-CX);
    SET_CX(CX + 0x4000);
    SET_CX(CX >> 1);
    if (V16(S_HALF) != CX) {
        V16(S_HALF) = CX;
        engines_to(CX, 0);
    }
}

static void h_77fb(void)                    /* AXIS_ELEV_TRIM_SET */
{
    eax = V32(S_VALUE);
    eax = -eax;
    trim_set();
}

static void h_7808(void)                    /* AXIS_SLEW_SIDEWAYS_SET */
{
    if (axis_changed(S_LAST_Y)) {
        ecx >>= 1;
        GW(0x5ec) = CX;
    }
}

static void h_7831(void)                    /* AXIS_SLEW_AHEAD_SET */
{
    if (axis_changed(S_LAST_X)) {
        ecx >>= 1;
        GW(0x5ea) = CX;
    }
}

static void h_785a(void)                    /* AXIS_SLEW_HEADING_SET */
{
    if (axis_changed(S_LAST_Z)) {
        ecx = -ecx;
        ecx >>= 2;
        GW(0x5e6) = CX;
    }
}

static void h_7886(void)                    /* AXIS_SLEW_ALT_SET */
{
    ecx = V32(S_VALUE);
    ecx = -ecx;
    ecx += 0x3fff;
    GW(0x5e8) = CX;
}

static void h_789c(void)                    /* AXIS_SLEW_BANK_SET */
{
    ecx = V32(S_VALUE);
    ecx = -ecx;
    GW(0x5e4) = CX;
}

static void h_78ac(void)                    /* AXIS_SLEW_PITCH_SET */
{
    ecx = V32(S_VALUE);
    ecx = -ecx;
    ecx += 0x3fff;
    GW(0x5ee) = CX;
}

/* ======== the shell ================================================================== */

/* .text+0x15f9 -- the command table, and .text+0x15f1/+0x15f5 its address
 * and count */
#define H(x) h_##x
struct controls_command controls_commands[CT_COMMANDS] = {
#include "controls_table.inc"
};
#undef H
struct controls_command *const controls_table = controls_commands;
const uint32_t controls_count = CT_COMMANDS;

void set_curves_body(uint32_t p) { set_curves(p); }
void elevator_body(uint32_t w) { elevator_by(w); }
void aileron_body(uint32_t w) { aileron_by(w); }
void rudder_body(uint32_t w) { rudder_by(w); }
void trim_body(uint32_t w) { trim_by(w); }
void engines_by_body(uint32_t w, uint32_t which) { engines_by(w, which); }
void engines_to_body(uint32_t w, uint32_t which) { engines_to(w, which); }
void event_body(uint32_t which, uint32_t value) { pilot_event(which, value); }

ENTRY0(init_entry, controls_init_body);
ENTRY0(deinit_entry, controls_deinit_body);
ENTRY1(curves_entry, set_curves_body);
ENTRY0(suspend_entry, yoke_suspend);
ENTRY0(resume_entry, yoke_resume);
ENTRY0(zoom_entry, zoom_report);
ENTRY1(aileron_entry, aileron_body);
ENTRY1(elevator_entry, elevator_body);
ENTRY1(rudder_entry, rudder_body);
ENTRY1(trim_entry, trim_body);
ENTRY2(engines_by_entry, engines_by_body);
ENTRY2(engines_to_entry, engines_to_body);
ENTRY2(event_entry, event_body);
ENTRY2(dispatch_entry, dispatch_entry_body);
ENTRY0(radios_entry, report_radios);

/* the C half's entries (controls_input.c) */
uint32_t FS98_STDCALL controls_keys_load(void);               /* .text+0x78d0 */
uint32_t FS98_STDCALL controls_keys_save(void);               /* .text+0x7bd0 */
uint32_t FS98_STDCALL controls_keys_list(uint32_t a, uint32_t b);  /* .text+0x7d60 */
uint32_t FS98_STDCALL controls_dialog(void);                  /* .text+0x7e40 */
uint32_t FS98_STDCALL controls_force(uint32_t op, uint32_t p);     /* .text+0xa400 */
void controls_input_install(void);

struct controls_image controls_image = {{
#include "controls_data.inc"
}};

uint8_t controls_rdata[CT_RDATA_SIZE] __attribute__((aligned(8))) = {
#include "controls_rdata.inc"
};

static const uint16_t rdata_pointers[][2] = {
#include "controls_pointers.inc"
};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _controls_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _controls_image + 0x78");

static void controls_install(void)
{
    uint32_t i;

    M32(D + CT_LINKAGE + 0x04) = ADDR(init_entry);
    M32(D + CT_LINKAGE + 0x08) = ADDR(deinit_entry);
    M32(D + CT_LINKAGE + 0x18) = ADDR(curves_entry);
    M32(D + CT_LINKAGE + 0x1c) = ADDR(suspend_entry);
    M32(D + CT_LINKAGE + 0x20) = ADDR(resume_entry);
    M32(D + CT_LINKAGE + 0x24) = ADDR(zoom_entry);
    M32(D + CT_LINKAGE + 0x28) = ADDR(aileron_entry);
    M32(D + CT_LINKAGE + 0x2c) = ADDR(elevator_entry);
    M32(D + CT_LINKAGE + 0x30) = ADDR(rudder_entry);
    M32(D + CT_LINKAGE + 0x34) = ADDR(trim_entry);
    M32(D + CT_LINKAGE + 0x38) = ADDR(engines_by_entry);
    M32(D + CT_LINKAGE + 0x3c) = ADDR(engines_to_entry);
    M32(D + CT_LINKAGE + 0x40) = ADDR(event_entry);
    M32(D + CT_LINKAGE + 0x44) = ADDR(dispatch_entry);
    M32(D + CT_LINKAGE + 0x48) = ADDR(radios_entry);
    M32(D + CT_LINKAGE + 0x4c) = ADDR(controls_keys_load);
    M32(D + CT_LINKAGE + 0x50) = ADDR(controls_keys_save);
    M32(D + CT_LINKAGE + 0x54) = ADDR(controls_keys_list);
    M32(D + CT_LINKAGE + 0x58) = ADDR(controls_dialog);
    M32(D + CT_LINKAGE + 0x5c) = ADDR(controls_force);
    M32(D + 0x344) = D + 0xb00;
    M32(D + 0x350) = D + 0xd48;
    for (i = 0; i < sizeof rdata_pointers / sizeof *rdata_pointers; i++)
        M32(D + rdata_pointers[i][0]) = RDATA + rdata_pointers[i][1];
    M32(RDATA + 0x264) = D + 0x510;
    controls_input_install();
}

/* .text+0x000 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        controls_install();
    }
    return TRUE;
}
