/* Differential test: retail CONTROLS.DLL against the OpenFS98 build, the
 * whole module but its key-assignment dialog.
 *
 * CONTROLS turns every key/button/axis into a command (an id and a value)
 * and dispatches it through a 383-entry table of handlers that move GLOBAL.
 * This harness loads both builds side by side and drives them through the
 * directly-callable Linkage entries -- the dispatcher +0x44, the axes
 * +0x28..+0x34, the engines +0x38/+0x3c, the pilot event +0x40, the radio
 * report +0x48, the curve setter +0x18 and the yoke suspend/resume
 * +0x1c/+0x20 -- over random commands and GLOBAL states, comparing GLOBAL,
 * the module's whole .data image and the transcript of every import call.
 * Each scenario runs init (+0x04) and seeds a joystick config, and the
 * per-frame joystick poll init registers on CHAIN list 1 is one of the ops,
 * run for a few frames at a time over scripted sticks whose buttons are
 * held, repeated and released.
 * Commands also go through CHAIN list 0xb, and CHAIN lists 2, 3, 6 and 0x14
 * run; deinit and init cycle; the key bindings (+0x4c/+0x50/+0x54) and force
 * feedback (+0x5c) are driven over the models below: the host's profile
 * services passed through to fixture files, its accelerator tables, the
 * joysticks, and a DirectInput 5 stand-in.  Not driven: the dialog (+0x58),
 * which waits on a stand-in for Options.DLL's dialog 0x140a.
 *
 * GLOBAL and the modules CONTROLS calls fire-and-forget are logging stubs;
 * UTIL and NUMERIC are retail's (the eye and the zoom text use them).  A
 * view record in a harness buffer, reset per side, lets the eye handlers run.
 *
 * Build: make controlsdiff     Run: make controlsdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0xc020u
#define GLOBAL_SIZE  0x1300u
#define VIEW_AT      0x11a0            /* GLOBAL: the active view record ptr */
#define WIN_AT       0x119c            /* GLOBAL: the active window id */
#define TRANSCRIPT_SIZE (16u << 20)
#define MAX_OPS      60

struct import { uint32_t id; void *link; };

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    uint8_t *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    char *transcript;
    uint32_t length;
    uint32_t outside;
};

struct helper {
    HMODULE module;
    uint8_t *link, *lo, *hi;
    uint8_t *saved[8], *at[8];
    uint32_t size[8], n;
};

static struct side retail, ours;
static struct side *cur;
static struct helper util, numeric;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t chain[0x24], main_m[0xd0], window[0x90], sound[0x40];
static uint8_t panels[0x40], hg2d[0x70], acontain[0x40], advdrv[0x20];
static uint8_t atc[0x28], demo[0x30];
static uint8_t view[0x80] __attribute__((aligned(16)));          /* the active view */
static uint32_t rng, scenario_seed, scenario, op_number;
static int counting;

static unsigned long cover_cmd, cover_cmd_crash,  cover_axis, cover_engine,
    cover_event, cover_post, cover_faults;
/* the joystick poll: polls run, and what they posted -- axis commands, and
 * the button commands told apart by the id ranges joy_setup gives them */
static unsigned long cover_jpoll, cover_jaxis, cover_jpress, cover_jrepeat,
    cover_jrelease;
/* the hand-written half's own CHAIN callbacks: list 0xb commands, list 2
 * and list 3 frames */
static unsigned long cover_cmdcb, cover_frame2, cover_frame3, cover_list6, cover_deinit, cover_keysload, cover_force;

#define G8(o)  (*(uint8_t *)(global + (o)))
#define G16(o) (*(uint16_t *)(global + (o)))
#define G32(o) (*(uint32_t *)(global + (o)))
#define I32(o) (*(uint32_t *)(cur->image + (o)))
#define M32(a) (*(uint32_t *)(uintptr_t)(a))
#define I8(o)  (*(uint8_t *)(cur->image + (o)))
#define SET_SLOT(t, o, fn) (*(void **)((t) + (o)) = (void *)(fn))

static uint32_t rnd(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}
static uint32_t rnd_n(uint32_t n) { return n ? rnd() % n : 0; }

static uint32_t digest(const void *p, uint32_t n, uint32_t h)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) h = (h ^ *b++) * 0x01000193u;
    return h;
}

static void put(const char *fmt, ...)
{
    va_list a; int n;
    if (cur == NULL || cur->length >= TRANSCRIPT_SIZE - 4096) return;
    va_start(a, fmt);
    n = vsnprintf(cur->transcript + cur->length, TRANSCRIPT_SIZE - cur->length, fmt, a);
    va_end(a);
    if (n > 0) cur->length += (uint32_t)n;
}

/* ---- faults ----------------------------------------------------------------- */

static void *fault_jb[5];
static volatile int inside, faulted;

static void fault_land(void) { __builtin_longjmp(fault_jb, 1); }

static int in_module(uint32_t v)
{
    return cur && v >= (uint32_t)(uintptr_t)cur->lo && v < (uint32_t)(uintptr_t)cur->hi;
}

/* the host's divide filter (0x4119a0): a faulting div/idiv saturates and the
 * sim carries on.  CONTROLS divides in the altitude-hold rounding. */
static int div_fixup(CONTEXT *c)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
    uint32_t len = 0, word = 0, byte, reg, rm, v, neg;
    DWORD *regs[8];

    regs[0] = &c->Eax; regs[1] = &c->Ecx; regs[2] = &c->Edx; regs[3] = &c->Ebx;
    regs[4] = &c->Esp; regs[5] = &c->Ebp; regs[6] = &c->Esi; regs[7] = &c->Edi;
    if (*p == 0x66) { word = 1; p++; len = 1; }
    if ((p[0] & 0xfe) != 0xf6) return 0;
    byte = !(p[0] & 1);
    reg = (p[1] >> 3) & 7;
    rm = p[1] & 7;
    if (reg != 6 && reg != 7) return 0;
    if ((p[1] >> 6) != 3) {
        printf("scenario %u: divide with a memory operand, not modelled\n", scenario);
        exit(2);
    }
    len += 2;
    if (reg == 6) {                       /* div: all ones */
        if (byte) c->Eax |= 0xffff;
        else if (word) { c->Eax |= 0xffff; c->Edx |= 0xffff; }
        else { c->Eax = 0xffffffffu; c->Edx = 0xffffffffu; }
    } else {                              /* idiv: largest, or most-negative-but-one */
        v = *regs[rm];
        if (byte) {
            neg = ((int8_t)v < 0) != ((int8_t)(c->Eax >> 8) < 0);
            c->Eax = (c->Eax & 0xffff0000u) | (neg ? 0x7f81u : 0x7f7fu);
        } else if (word) {
            neg = ((int16_t)v < 0) != ((int16_t)c->Edx < 0);
            c->Eax = (c->Eax & 0xffff0000u) | (neg ? 0x8001u : 0x7fffu);
            c->Edx = (c->Edx & 0xffff0000u) | 0x7fffu;
        } else {
            neg = ((int32_t)v < 0) != ((int32_t)c->Edx < 0);
            c->Eax = neg ? 0x80000001u : 0x7fffffffu;
            c->Edx = 0x7fffffffu;
        }
    }
    c->Eip += len;
    return 1;
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        (e->ContextRecord->Eip & 0xff000000u) == 0xde000000u) {
        printf("scenario %u op %u %s: wild call to %#lx\n", scenario, op_number,
               cur ? cur->name : "none", (unsigned long)e->ContextRecord->Eip);
        if (cur && cur->length) { uint32_t f = cur->length > 1500 ? cur->length - 1500 : 0;
            printf("--- tail:\n%.*s\n", (int)(cur->length - f), cur->transcript + f); }
        fflush(stdout); ExitProcess(5);
    }
    if (!inside || cur == NULL) return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO && in_module(e->ContextRecord->Eip) &&
        div_fixup(e->ContextRecord))
        return EXCEPTION_CONTINUE_EXECUTION;
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW) put(" FAULT(div)");
    else if (code == EXCEPTION_ACCESS_VIOLATION) put(" FAULT(av)");
    else return EXCEPTION_CONTINUE_SEARCH;
    if (counting) cover_faults++;
    if (getenv("CONTROLS_WHERE"))
        printf("  [fault %s eip=%#lx addr=%#lx]\n", cur ? cur->name : "?",
               (unsigned long)e->ContextRecord->Eip,
               e->ExceptionRecord->NumberParameters > 1 ?
                 (unsigned long)e->ExceptionRecord->ExceptionInformation[1] : 0);
    inside = 0; faulted = 1;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* ---- logging stubs ---------------------------------------------------------- */

static const char *pptr(uint32_t v, char *buf)
{
    if (v == 0) return "null";
    if (v >= (uint32_t)(uintptr_t)global && v < (uint32_t)(uintptr_t)global + GLOBAL_SIZE)
        sprintf(buf, "G+%#x", v - (uint32_t)(uintptr_t)global);
    else if (cur && v >= (uint32_t)(uintptr_t)cur->image && v < (uint32_t)(uintptr_t)cur->image + IMAGE_SIZE)
        sprintf(buf, "img+%#x", v - (uint32_t)(uintptr_t)cur->image);
    else if (v >= (uint32_t)(uintptr_t)view && v < (uint32_t)(uintptr_t)view + sizeof view)
        sprintf(buf, "view+%#x", v - (uint32_t)(uintptr_t)view);
    else if (in_module(v)) sprintf(buf, "<code>");
    else sprintf(buf, "<ptr>");
    return buf;
}

static uint32_t logc(const char *tag, int n, uint32_t a, uint32_t b, uint32_t c,
                     uint32_t d, uint32_t e, uint32_t f)
{
    const uint32_t v[6] = { a, b, c, d, e, f };
    int i;
    put("\n  %s(", tag);
    for (i = 0; i < n; i++) put(i ? ",%#x" : "%#x", v[i]);
    put(")");
    return 0;
}
#define S0(nm, tag) static uint32_t STDCALL nm(void){return logc(tag,0,0,0,0,0,0,0);}
#define S1(nm, tag) static uint32_t STDCALL nm(uint32_t a){return logc(tag,1,a,0,0,0,0,0);}
#define S2(nm, tag) static uint32_t STDCALL nm(uint32_t a,uint32_t b){return logc(tag,2,a,b,0,0,0,0);}
#define S3(nm, tag) static uint32_t STDCALL nm(uint32_t a,uint32_t b,uint32_t c){return logc(tag,3,a,b,c,0,0,0);}

/* the scripted joysticks' buttons, bit n for button n: down at the last
 * read, and at the one before it (so a press posted while the button was
 * already down is a repeat).  Reset per side by joy_setup. */
static uint64_t joy_down[4], joy_was[4];
static int joy_pov[4], in_jpoll;

/* host */
/* +0x3c post WM_COMMAND (cmd, value): logged.  Inside a joystick poll the
 * id says what posted it: joy_setup gives axes 0x10000.., presses
 * 0x10080 + joystick*0x40 + button, releases 0x10100 + the same. */
static uint32_t STDCALL h_post(uint32_t cmd, uint32_t value)
{
    if (counting) {
        cover_post++;
        if (in_jpoll) {
            uint32_t k = (cmd - 0x10080) & 0x7f;
            if (cmd >= 0x10000 && cmd < 0x10080) cover_jaxis++;
            else if (cmd >= 0x10080 && cmd < 0x10100) {
                if ((joy_was[k >> 6] >> (k & 0x3f)) & 1) cover_jrepeat++;
                else cover_jpress++;
            } else if (cmd >= 0x10100 && cmd < 0x10180) cover_jrelease++;
        }
    }
    return logc("host+0x3c", 2, cmd, value, 0, 0, 0, 0);
}
S0(h_quit, "host+0x24")
S1(h_48, "host+0x48")
S1(h_90, "host+0x90")
S2(h_menu, "host+0x254")
static uint32_t STDCALL h_capture(uint32_t cb)
{ char b[24]; put("\n  host+0x274(%s)", pptr(cb, b)); return 0; }
S0(h_release, "host+0x278")
S1(h_accel_on, "host+0x1e8")
S1(h_accel_off, "host+0x1ec")
/* +0x218 memset(dst, byte, n) */
static uint32_t STDCALL h_memset(uint32_t dst, uint32_t byte, uint32_t n)
{ memset((void *)(uintptr_t)dst, (int)byte, n); return 0; }
static uint32_t STDCALL h_memmove(uint32_t dst, uint32_t src, uint32_t n)
{ memmove((void *)(uintptr_t)dst, (void *)(uintptr_t)src, n); return 0; }
/* memset and memmove, logged; pointers through pptr */
static uint32_t STDCALL lh_memset(uint32_t dst, uint32_t byte, uint32_t n)
{ char a[24]; put("\n  host+0x218(%s,%#x,%#x)", pptr(dst, a), byte, n); return h_memset(dst, byte, n); }
static uint32_t STDCALL lh_memmove(uint32_t dst, uint32_t src, uint32_t n)
{
    char a[24], b[24];
    put("\n  host+0x21c(%s,%s,%#x)", pptr(dst, a), pptr(src, b), n);
    return h_memmove(dst, src, n);
}
/* +0x1c4 read joystick `joy` into `block`; return 0 (present).  The axis
 * values (block+0xc, 0x10 dwords) are a scripted reading.  The buttons are
 * written as the host's 0x40b8f0 writes them after joyGetPosEx: one word per
 * button from block+0x4c, exactly 0 or 1 -- buttons 0..31 from the button
 * bits, 32..63 cleared (its rep stosd), then one of 32..39 set by the POV
 * hat's direction.  Each button keeps its state from read to read and
 * toggles now and then, so presses, holds (and so repeats) and releases
 * all happen. */
static uint32_t STDCALL h_joyread(uint32_t joy, uint32_t block)
{
    uint32_t i, j = joy & 3;
    char pb[24];
    put("\n  host+0x1c4(%#x,%s)", joy, pptr(block, pb));
    for (i = 0; i < 0x10; i++)
        *(uint32_t *)(uintptr_t)(block + 0xc + i * 4) = (int32_t)(rnd() & 0x7fff) - 0x4000;
    joy_was[j] = joy_down[j];
    if (rnd_n(4) == 0) joy_pov[j] = (int)rnd_n(9) - 1;       /* -1 centred, 0..7 */
    joy_down[j] = (uint32_t)(joy_down[j] ^ (rnd() & rnd() & rnd()));
    if (joy_pov[j] >= 0) joy_down[j] |= (uint64_t)1 << (32 + joy_pov[j]);
    for (i = 0; i < 0x40; i++)
        *(uint32_t *)(uintptr_t)(block + 0x4c + i * 4) = (uint32_t)(joy_down[j] >> i) & 1;
    return 0;
}
/* CHAIN: +0x18/+0x1c register, +0x20 dispatch.  Callbacks are captured per
 * list, in order, and removed as CHAIN removes them, so the harness can run
 * a list as CHAIN would (chain_run).  list1 is list 1's: the joystick poll
 * is the first registered there. */
static uint32_t cbs[0x20][4], ncbs[0x20];
#define list1 cbs[1]
#define nlist1 ncbs[1]
static uint32_t STDCALL chain_add(uint32_t list, uint32_t cb)
{
    char b[24];
    if (list < 0x20 && ncbs[list] < 4) cbs[list][ncbs[list]++] = cb;
    put("\n  chain+0x18(%#x,%s)", list, pptr(cb, b));
    return 0;
}
static uint32_t STDCALL chain_remove(uint32_t list, uint32_t cb)
{
    char b[24]; uint32_t i;
    put("\n  chain+0x1c(%#x,%s)", list, pptr(cb, b));
    if (list < 0x20)
        for (i = 0; i < ncbs[list]; i++)
            if (cbs[list][i] == cb) {
                memmove(&cbs[list][i], &cbs[list][i + 1], (ncbs[list] - i - 1) * sizeof cbs[0][0]);
                ncbs[list]--;
                break;
            }
    return 0;
}
/* +0x20 dispatch (list, a, b), logged.  List 0xc is a command passing
 * through (controls .text+0x3406): a is its id and b the address of a
 * local "taken" dword the list's callbacks may set to claim it, so b is
 * logged by name and the scripted answer written through it -- mostly
 * not taken, sometimes 1, now and then any nonzero value. */
static uint32_t STDCALL chain_dispatch(uint32_t list, uint32_t a, uint32_t b)
{
    if (list == 0xc) {
        uint32_t t = rnd_n(5) ? 0 : rnd_n(2) ? 1 : (rnd() | 1);
        put("\n  chain+0x20(0xc,%#x,&taken) taken=%#x", a, t);
        *(uint32_t *)(uintptr_t)b = t;
        return 0;
    }
    return logc("chain+0x20", 3, list, a, b, 0, 0, 0);
}
/* ---- the joystick configs: profiles, the devices file, the sticks ------------
 *
 * The host's profile services are passed through to the real Win32 profile
 * API over fixture files written per scenario (FLTSIM98.CFG and the devices
 * file in controls_fx\), as the host's own are: +0xa8/+0xac/+0xc8/+0xcc/
 * +0xf8/+0x100 on FLTSIM98.CFG, +0xd0/+0xf0/+0x104/+0x158 on a path and an
 * extension id, +0x130/+0x184 for the sim's directory and joining.  The
 * files' contents after each side go into the transcript, so every write is
 * compared.  Strings come from the CD's ENGLISH.FLL (the joystick models are
 * 0xb000 + 4n, their flight and slew defaults +1 and +2), sometimes mutated.
 * The joystick count (+0x1c0) and the sticks' descriptions (+0x1c8) are
 * scripted. */

static char fx_dir[MAX_PATH], fx_cfg[MAX_PATH], fx_dev[MAX_PATH];
static HMODULE fll;
static char ext_text[0x25][8];
static const char *cmd_names[0x200]; static uint32_t n_cmd_names;
static uint32_t cmd_ids[0x200], n_cmd_ids;
static struct { uint32_t id, rid; char name[0x40]; } models[0x40]; static uint32_t n_models;
static uint32_t fx_count, fx_strmut;       /* the scripted joystick count; a string mutation */
static uint32_t fx_aircraft;               /* whether ACONTAIN +0x30 names an aircraft file */
/* the scenario's DirectInput (see the stand-in below) */
enum { DI_ABSENT, DI_NOCREATE, DI_NODEVICE, DI_DEVICE };
static int di_mode, di_entry_missing, di_keyboard_first;
static struct { uint32_t known, model, rudder, caps; char name[0x40]; } fx_stick[4];
static unsigned long cover_cfghit, cover_devhit, cover_strings, cover_saves;

static void put_str(const char *s)
{
    uint32_t i;
    if (!s) { put("null"); return; }
    put("\"");
    for (i = 0; i < 0x80 && s[i]; i++) put("%c", (s[i] >= 0x20 && s[i] < 0x7f) ? s[i] : '?');
    put("\"");
}

/* the host's extension table, 0x417f20, for resolving a path + extension id */
static void load_ext_table(const char *path)
{
    static uint8_t b[0x40000];
    FILE *f = fopen(path, "rb");
    size_t n;
    uint32_t pe, nsec, opt, i, k;
    if (!f) { printf("cannot open %s\n", path); exit(2); }
    n = fread(b, 1, sizeof b, f);
    fclose(f);
    pe = *(uint32_t *)(b + 0x3c);
    nsec = *(uint16_t *)(b + pe + 6);
    opt = *(uint16_t *)(b + pe + 20);
    for (k = 1; k < 0x25; k++) {
        uint32_t va = 0x417f20 + 4 * k, ptr = 0, off = 0;
        for (i = 0; i < nsec; i++) {
            uint8_t *sc = b + pe + 24 + opt + 40 * i;
            uint32_t sva = 0x400000 + *(uint32_t *)(sc + 12), rs = *(uint32_t *)(sc + 16),
                     ro = *(uint32_t *)(sc + 20);
            if (va >= sva && va < sva + rs) ptr = *(uint32_t *)(b + ro + va - sva);
        }
        for (i = 0; i < nsec; i++) {
            uint8_t *sc = b + pe + 24 + opt + 40 * i;
            uint32_t sva = 0x400000 + *(uint32_t *)(sc + 12), rs = *(uint32_t *)(sc + 16),
                     ro = *(uint32_t *)(sc + 20);
            if (ptr >= sva && ptr < sva + rs) off = ro + ptr - sva;
        }
        if (!off || off + 8 > n) { printf("bad extension table\n"); exit(2); }
        strncpy(ext_text[k], (char *)b + off, 7);
    }
    if (strcmp(ext_text[0x24], ".CFG")) { printf("extension table: unexpected contents\n"); exit(2); }
}

/* 0x40b350 */
static int resolve(const char *path, int32_t ext, char *out, size_t len)
{
    const char *t;
    if (ext == -2) t = "";
    else if (ext >= 0 && ext < 0x25) t = ext_text[ext];
    else return -1;
    snprintf(out, len, "%s%s", path, t);
    return 0;
}

/* +0xa8 (0x408890): *inout = GetPrivateProfileIntA(sec, key, *inout, cfg) */
static uint32_t STDCALL h_cfg_int(const char *sec, const char *key, int32_t *inout)
{
    int32_t was = *inout;
    *inout = (int32_t)GetPrivateProfileIntA(sec, key, *inout, fx_cfg);
    if (counting && *inout != was) cover_cfghit++;
    put("\n  host+0xa8("); put_str(sec); put(","); put_str(key); put(")=%d", *inout);
    return 0;
}
/* +0xac (0x4088c0): wsprintf("%d") and WritePrivateProfileStringA */
static uint32_t STDCALL h_cfg_put_int(const char *sec, const char *key, int32_t v)
{
    char b[0x10];
    wsprintfA(b, "%d", v);
    if (counting) cover_saves++;
    put("\n  host+0xac("); put_str(sec); put(","); put_str(key); put(",%d)", v);
    WritePrivateProfileStringA(sec, key, b, fx_cfg);
    return 0;
}
/* +0xc8: GetPrivateProfileStringA(sec, key, buf, buf, size, cfg) -- the
 * buffer is its own default */
static uint32_t STDCALL h_cfg_str(const char *sec, const char *key, char *buf, uint32_t size)
{
    char was[0x40];
    int32_t r;
    lstrcpynA(was, buf, sizeof was);
    r = (int32_t)GetPrivateProfileStringA(sec, key, buf, buf, size, fx_cfg);
    if (counting && strcmp(was, buf)) cover_cfghit++;
    put("\n  host+0xc8("); put_str(sec); put(","); put_str(key); put(",%#x)=%d,", size, r); put_str(buf);
    return (uint32_t)r;
}
/* +0xcc (0x408d50): WritePrivateProfileStringA; a null string deletes */
static uint32_t STDCALL h_cfg_put_str(const char *sec, const char *key, const char *str)
{
    if (counting) cover_saves++;
    put("\n  host+0xcc("); put_str(sec); put(","); put_str(key); put(","); put_str(str); put(")");
    WritePrivateProfileStringA(sec, key, str, fx_cfg);
    return 0;
}
/* +0xf8 (0x408860): WritePrivateProfileSectionA(sec, "", cfg); the string is
 * the word at 0x4188a0, in the host's .bss */
static uint32_t STDCALL h_cfg_clear(const char *sec)
{
    put("\n  host+0xf8("); put_str(sec); put(")");
    WritePrivateProfileSectionA(sec, "\0", fx_cfg);
    return 0;
}
/* +0x100 (0x408810): *out = 1 if [sec] has a key in FLTSIM98.CFG */
static uint32_t STDCALL h_cfg_has(const char *sec, int32_t *out)
{
    char b[4];
    *out = 0;
    b[0] = 0;
    if (GetPrivateProfileStringA(sec, NULL, b, b, 4, fx_cfg) > 0) *out = 1;
    put("\n  host+0x100("); put_str(sec); put(")=%d", *out);
    return 0;
}
/* +0x104 (0x408d70): the same on path + extension; -1 if unresolvable */
static uint32_t STDCALL h_prof_has(const char *sec, const char *path, int32_t ext, int32_t *out)
{
    char full[0x200], b[4];
    *out = 0;
    if (resolve(path, ext, full, sizeof full)) { put("\n  host+0x104=-1"); return (uint32_t)-1; }
    b[0] = 0;
    if (GetPrivateProfileStringA(sec, NULL, b, b, 4, full) > 0) *out = 1;
    put("\n  host+0x104("); put_str(sec); put(","); put_str(full + strlen(fx_dir)); put(")=%d", *out);
    return 0;
}
/* +0xd0: GetPrivateProfileIntA on path + extension, *inout the default */
static uint32_t STDCALL h_prof_int(const char *sec, const char *key, int32_t *inout,
                                   const char *path, int32_t ext)
{
    char full[0x200];
    int32_t was = *inout;
    if (resolve(path, ext, full, sizeof full)) { put("\n  host+0xd0=-1"); return (uint32_t)-1; }
    *inout = (int32_t)GetPrivateProfileIntA(sec, key, *inout, full);
    if (counting && *inout != was) cover_devhit++;
    put("\n  host+0xd0("); put_str(sec); put(","); put_str(key); put(")=%d", *inout);
    return 0;
}
/* +0xf0: GetPrivateProfileStringA on path + extension */
static uint32_t STDCALL h_prof_str(const char *sec, const char *key, char *buf, uint32_t size,
                                   const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;
    if (resolve(path, ext, full, sizeof full)) { put("\n  host+0xf0=-1"); return (uint32_t)-1; }
    r = (int32_t)GetPrivateProfileStringA(sec, key, buf, buf, size, full);
    if (counting && r > 0) cover_devhit++;
    put("\n  host+0xf0("); put_str(sec); put(","); put_str(key); put(")=%d,", r); put_str(buf);
    return (uint32_t)r;
}
/* +0x158: 0 if GetFileAttributesA finds path + extension, else -1 */
static uint32_t STDCALL h_exists(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;
    if (resolve(path, ext, full, sizeof full)) return (uint32_t)-1;
    r = GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES ? -1 : 0;
    put("\n  host+0x158("); put_str(full + strlen(fx_dir)); put(")=%d", r);
    return (uint32_t)r;
}
/* +0x184 (0x40a510): join drive, directory and name */
static uint32_t STDCALL h_join(char *out, const char *drive, const char *dir, const char *name)
{
    char b[0x208];
    size_t n;
    memset(b, 0, sizeof b);
    if (!out) return (uint32_t)-1;
    if (drive && strlen(drive)) {
        strcat(b, drive);
        if (strlen(b) == 1) strcat(b, ":");
        n = strlen(b);
        if (_strcmpi(b + n - 1, "\\") == 0) b[n - 1] = 0;
    }
    if (dir && strlen(dir)) {
        if (dir[0] != '\\' && strlen(b)) strcat(b, "\\");
        strcat(b, dir);
        n = strlen(b);
        if (_strcmpi(b + n - 1, "\\") == 0) b[n - 1] = 0;
    }
    if (name && strlen(name)) {
        if (name[0] != '\\' && strlen(b)) strcat(b, "\\");
        strcat(b, name);
    }
    if (strlen(b) >= 0x104) { out[0] = 0; return (uint32_t)-1; }
    strcpy(out, b);
    return 0;
}
/* +0x130 (0x40a970): the sim's directory */
static uint32_t STDCALL h_simdir(char *out) { strcpy(out, fx_dir); return 0; }
/* +0x2a0: LoadStringA from ENGLISH.FLL, sometimes mutated (blanks after
 * commas or before the end, a sign, a stray character), as the scenario chose */
static uint32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    int32_t r = LoadStringA(fll, id, buf, len), n;
    if (r > 0 && fx_strmut) {
        static char t[0x1000];
        char *src = buf, *d = t;
        uint32_t m = fx_strmut, k = 0;
        while (*src && d < t + sizeof t - 8) {
            *d++ = *src;
            if (*src == ',' && (m & 1) && (++k % 5) == 0) *d++ = ' ';
            if (*src == ',' && (m & 2) && (k % 7) == 3) *d++ = '-';
            if (*src == ',' && (m & 4) && (k % 11) == 6) *d++ = 'x';
            src++;
        }
        if (m & 8) { *d++ = ' '; *d++ = ' '; }
        *d = 0;
        lstrcpynA(buf, t, len);
        r = (int32_t)strlen(buf);
    }
    if (counting && r > 0) cover_strings++;
    n = r;
    put("\n  host+0x2a0(%#x,%#x)=%d", id, len, n);
    return (uint32_t)r;
}
/* +0x2a8/+0x2b0/+0x2b8/+0x2c0: the host's string helpers, not logged (their
 * effect shows in what is done with the strings) */
static char *STDCALL h_strcpy(char *d, const char *s) { return strcpy(d, s); }
static char *STDCALL h_strcat(char *d, const char *s) { return strcat(d, s); }
static uint32_t STDCALL h_strlen(const char *s) { return s ? (uint32_t)strlen(s) : 0; }
static int32_t STDCALL h_stricmp(const char *a, const char *b) { return _strcmpi(a, b); }
/* +0x20c/+0x214: the heap, never failing */
static int32_t STDCALL h_alloc(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    (void)flags;
    *out = malloc(size ? size : 1);
    if (zero) memset(*out, 0, size);
    return 0;
}
static int32_t STDCALL h_free(void **pp) { if (*pp) free(*pp); *pp = NULL; return 0; }
/* +0x1c0: how many joysticks answer, scripted -- now and then another */
static uint32_t STDCALL h_joycount(void)
{
    if (rnd_n(16) == 0) fx_count = rnd_n(3);
    put("\n  host+0x1c0()=%u", fx_count);
    return fx_count;
}
/* +0x1c8 (0x40bcc0): describe joystick `joy`: +0 its model's string id
 * (left alone, and -1 returned, when the host cannot match the name), +4 and
 * +8 caps (+8's bit 1 a rudder), +0xc the rudder, +0x10 the OEM name */
static uint32_t STDCALL h_joydesc(uint32_t joy, uint8_t *desc)
{
    uint32_t j = joy & 3;
    put("\n  host+0x1c8(%#x)", joy);
    *(uint32_t *)(desc + 4) = 3 + j;
    *(uint32_t *)(desc + 8) = fx_stick[j].caps | (fx_stick[j].rudder ? 2 : 0);
    *(uint32_t *)(desc + 0xc) = fx_stick[j].rudder != 0;
    lstrcpynA((char *)desc + 0x10, fx_stick[j].name, 0x40);
    /* the host matches "<name>_r" for a stick with a rudder */
    if (!fx_stick[j].known || (fx_stick[j].rudder && !models[fx_stick[j].model].rid)) {
        put("=-1");
        return (uint32_t)-1;
    }
    *(uint32_t *)desc = fx_stick[j].rudder ? models[fx_stick[j].model].rid : models[fx_stick[j].model].id;
    put("=0 %#x", *(uint32_t *)desc);
    return 0;
}

/* a command name for a fixture: the table's, in a random case, or not one */
static void fx_cmd(char *out, uint32_t size)
{
    uint32_t r = rnd_n(8), i;
    if (r < 5 && n_cmd_names) {
        const char *n = cmd_names[rnd_n(n_cmd_names)];
        for (i = 0; n[i] && i + 1 < size; i++)
            out[i] = (char)(rnd_n(4) ? n[i] : (n[i] >= 'A' && n[i] <= 'Z' ? n[i] + 32 : n[i]));
        out[i] = 0;
    } else if (r < 7)
        snprintf(out, size, "%u", rnd_n(4) ? 0x10000 + rnd_n(0x200) : rnd());
    else
        snprintf(out, size, "NOT_A_COMMAND_%u", rnd_n(9));
}

/* one joystick section's keys, at random density */
static void fx_section(FILE *f, const char *head)
{
    uint32_t k, dense = rnd_n(3);
    char c[0x40];
    fprintf(f, "[%s]\r\n", head);
    if (rnd_n(4)) fprintf(f, "LOCKED=%u\r\n", rnd_n(4) == 0);
    if (rnd_n(4)) fprintf(f, "TYPE=%u\r\n", n_models ? models[rnd_n(n_models)].id : 0);
    if (rnd_n(4)) fprintf(f, "AXIS_FLAGS=%u\r\n", rnd() & 0xffff);
    for (k = 0; k < 0x10; k++) {
        if (rnd_n(4) > dense) continue;
        fx_cmd(c, sizeof c); fprintf(f, "AXIS_EVENT_%02u=%s\r\n", k, c);
        if (rnd_n(3)) fprintf(f, "AXIS_SCALE_%02u=%d\r\n", k, (int)rnd_n(200) - (rnd_n(8) ? 0 : 100));
        if (rnd_n(3)) fprintf(f, "AXIS_NULL_%02u=%u\r\n", k, rnd_n(100));
    }
    for (k = 0; k < 0x40; k++) {
        if (rnd_n(6) > dense) continue;
        fx_cmd(c, sizeof c); fprintf(f, "BUTTON_DOWN_EVENT_%02u=%s\r\n", k, c);
        if (rnd_n(2)) fprintf(f, "BUTTON_DOWN_REPEAT_%02u=%u\r\n", k, rnd_n(6));
        if (rnd_n(2)) { fx_cmd(c, sizeof c); fprintf(f, "BUTTON_UP_EVENT_%02u=%s\r\n", k, c); }
    }
}

/* a [section] of key specs: command name = key, flags -- sometimes in hex,
 * sometimes one number, now and then not numbers, and a 0 key (no binding) */
static void fx_keys(FILE *f, const char *head)
{
    uint32_t i, n = 1 + rnd_n(40);
    fprintf(f, "[%s]\r\n", head);
    for (i = 0; i < n && n_cmd_names; i++) {
        const char *c = cmd_names[rnd_n(n_cmd_names)];
        uint32_t r = rnd_n(10), key = rnd_n(8) ? 0x20 + rnd_n(0x60) : 0;
        if (r < 6) fprintf(f, "%s=%u,%u\r\n", c, key, rnd_n(16));
        else if (r < 8) fprintf(f, "%s=0x%x, 0x%x\r\n", c, key, rnd_n(16));
        else if (r < 9) fprintf(f, "%s=%u\r\n", c, key);
        else fprintf(f, "%s=KEY_%u\r\n", c, rnd_n(9));
    }
}

/* write this scenario's FLTSIM98.CFG and devices file (from the scenario's
 * rng, so identically for each side), and choose the sticks */
static void fx_write(void)
{
    FILE *f;
    uint32_t j, k;
    char head[0x80];
    fx_count = rnd_n(3);
    fx_strmut = rnd_n(3) ? 0 : 1 + rnd_n(15);
    {
        uint32_t r = rnd_n(16);
        di_mode = r < 3 ? DI_ABSENT : r < 4 ? DI_NOCREATE : r < 6 ? DI_NODEVICE : DI_DEVICE;
        di_entry_missing = rnd_n(64) == 0;
        di_keyboard_first = rnd_n(3) == 0;
    }
    fx_aircraft = rnd_n(2);
    if (fx_aircraft) {
        static const char *const ints[] = {
            "stick_shaker_magnitude", "stick_shaker_direction", "stick_shaker_period",
            "gear_bump_nose_magnitude", "gear_bump_nose_direction", "gear_bump_nose_duration",
            "gear_bump_left_magnitude", "gear_bump_left_direction", "gear_bump_left_duration",
            "gear_bump_right_magnitude", "gear_bump_right_direction", "gear_bump_right_duration",
            "ground_bumps_magnitude1", "ground_bumps_angle1", "ground_bumps_magnitude2",
            "ground_bumps_angle2", "crash_magnitude1", "crash_direction1", "crash_magnitude2",
            "crash_direction2", "crash_period2", "crash_duration2",
        };
        static const char *const dbls[] = {
            "ground_bumps_intercept1", "ground_bumps_slope1", "ground_bumps_intercept2", "ground_bumps_slope2",
        };
        char path[MAX_PATH];
        uint32_t i;
        snprintf(path, sizeof path, "%s\\aircraft.CFG", fx_dir);
        f = fopen(path, "wb");
        if (!f) { printf("cannot write %s\n", path); exit(2); }
        fprintf(f, "[forcefeedback]\r\n");
        for (i = 0; i < sizeof ints / sizeof ints[0]; i++)
            if (rnd_n(3)) fprintf(f, "%s=%d\r\n", ints[i], (int)rnd_n(20000) - (rnd_n(6) ? 0 : 10000));
        for (i = 0; i < sizeof dbls / sizeof dbls[0]; i++)
            if (rnd_n(3)) {
                static const char *const odd[] = { "0", "-0.5", "1e6", "0.0001", "-3" };
                if (rnd_n(5)) fprintf(f, "%s=%.6f\r\n", dbls[i], (double)(int)rnd_n(4000) / 1000.0 - 1.0);
                else fprintf(f, "%s=%s\r\n", dbls[i], odd[rnd_n(5)]);
            }
        fclose(f);
        WritePrivateProfileStringA(NULL, NULL, NULL, path);
    }
    for (j = 0; j < 4; j++) {
        fx_stick[j].known = rnd_n(6) != 0;
        fx_stick[j].model = n_models ? rnd_n(n_models) : 0;
        fx_stick[j].rudder = rnd_n(3) == 0;
        fx_stick[j].caps = rnd() & 0x1d;
        if (rnd_n(8) == 0) snprintf(fx_stick[j].name, sizeof fx_stick[j].name, "Unknown Stick %u", j);
        else lstrcpynA(fx_stick[j].name, models[fx_stick[j].model].name, sizeof fx_stick[j].name);
    }
    f = fopen(fx_cfg, "wb");
    if (!f) { printf("cannot write %s\n", fx_cfg); exit(2); }
    if (rnd_n(4)) fprintf(f, "[CONTROLS]\r\ndevices=%s\r\n", rnd_n(6) ? "devices" : "nodevices");
    if (rnd_n(2)) fprintf(f, "[PREFS]\r\nJOYSTICKS=%u\r\n", rnd_n(3) != 0);
    for (j = 0; j < 3; j++) {
        if (rnd_n(2)) { snprintf(head, sizeof head, "JOYSTICK_%02u", j); fx_section(f, head); }
        if (rnd_n(2)) { snprintf(head, sizeof head, "JOYSTICK_SLEW_%02u", j); fx_section(f, head); }
    }
    if (rnd_n(3) == 0) fx_keys(f, "KEYBOARD_MAIN");
    if (rnd_n(3) == 0) fx_keys(f, "KEYBOARD_SLEW");
    if (rnd_n(4)) {
        static const char *const en[] = { "force_stick_force_enabled", "force_stick_shaker_enabled",
            "force_gear_bump_enabled", "force_crash_enabled", "force_ground_bumps_enabled" };
        uint32_t i;
        fprintf(f, "[CONTROLS]\r\nforce_master_enabled=%u\r\nforce_master_gain=%u\r\n",
                rnd_n(3) != 0, rnd_n(10001));
        for (i = 0; i < 5; i++) if (rnd_n(4)) fprintf(f, "%s=%u\r\n", en[i], rnd_n(4) != 0);
    }
    fclose(f);
    if (rnd_n(5) == 0) DeleteFileA(fx_dev);
    else {
        f = fopen(fx_dev, "wb");
        if (!f) { printf("cannot write %s\n", fx_dev); exit(2); }
        for (k = 0; k < 4; k++) {
            uint32_t m = rnd_n(n_models ? n_models : 1);
            static const char *const tail[4] = { "", " RUDDER", " SLEW", " RUDDER SLEW" };
            if (rnd_n(2)) m = fx_stick[k].model;
            snprintf(head, sizeof head, "%s%s", models[m].name, tail[rnd_n(4)]);
            fx_section(f, head);
        }
        /* the keyboard layout's sections, "%s %08X" */
        if (rnd_n(3) == 0) {
            snprintf(head, sizeof head, "KEYBOARD_MAIN %08X", (uint32_t)(uintptr_t)GetKeyboardLayout(0));
            fx_keys(f, head);
        }
        if (rnd_n(3) == 0) {
            snprintf(head, sizeof head, "KEYBOARD_SLEW %08X", (uint32_t)(uintptr_t)GetKeyboardLayout(0));
            fx_keys(f, head);
        }
        fclose(f);
    }
    /* drop what the profile API cached of the previous contents */
    WritePrivateProfileStringA(NULL, NULL, NULL, fx_cfg);
    WritePrivateProfileStringA(NULL, NULL, NULL, fx_dev);
}

/* the files as each side left them, into its transcript */
static void fx_dump(void)
{
    static char b[0x40000];
    const char *paths[2] = { fx_cfg, fx_dev };
    uint32_t i;
    WritePrivateProfileStringA(NULL, NULL, NULL, fx_cfg);
    WritePrivateProfileStringA(NULL, NULL, NULL, fx_dev);
    for (i = 0; i < 2; i++) {
        FILE *f = fopen(paths[i], "rb");
        size_t n = 0;
        if (f) { n = fread(b, 1, sizeof b - 1, f); fclose(f); }
        b[n] = 0;
        put("\n#file %u (%u bytes)\n%s", i, (unsigned)n, f ? b : "(none)");
    }
}

/* the command names, from retail's table (.text+0x15f1), and the joystick
 * models, from ENGLISH.FLL */
static void fx_init(HMODULE retail_module)
{
    uint8_t *text = (uint8_t *)retail_module + 0x1000;
    uint32_t table = *(uint32_t *)(text + 0x15f1), count = *(uint32_t *)(text + 0x15f5), i;
    char b[0x100];
    for (i = 0; i < count && n_cmd_names < 0x200; i++) {
        const char *n = *(const char **)(uintptr_t)(table + i * 20 + 0x10);
        if (n) cmd_names[n_cmd_names++] = n;
        if (n_cmd_ids < 0x200) cmd_ids[n_cmd_ids++] = *(uint32_t *)(uintptr_t)(table + i * 20 + 8);
    }
    fll = LoadLibraryExA("ENGLISH.FLL", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!fll) { printf("cannot load ENGLISH.FLL\n"); exit(2); }
    for (i = 0xb000; i < 0xb100 && n_models < 0x40; i += 4) {
        size_t n;
        if (LoadStringA(fll, i, b, sizeof b) <= 0) continue;
        n = strlen(b);
        if (n > 2 && !strcmp(b + n - 2, "_r")) continue;
        models[n_models].id = i;
        lstrcpynA(models[n_models].name, b, sizeof models[0].name);
        n_models++;
    }
    for (i = 0xb000; i < 0xb100; i += 4) {
        uint32_t m;
        size_t n;
        if (LoadStringA(fll, i, b, sizeof b) <= 0) continue;
        n = strlen(b);
        if (n <= 2 || strcmp(b + n - 2, "_r")) continue;
        b[n - 2] = 0;
        for (m = 0; m < n_models; m++)
            if (!strcmp(models[m].name, b)) models[m].rid = i;
    }
    load_ext_table("FLTSIM98_host.exe");
    CreateDirectoryA("controls_fx", NULL);
    GetFullPathNameA("controls_fx", sizeof fx_dir, fx_dir, NULL);
    snprintf(fx_cfg, sizeof fx_cfg, "%s\\FLTSIM98.CFG", fx_dir);
    snprintf(fx_dev, sizeof fx_dev, "%s\\devices.CFG", fx_dir);
    printf("fixtures in %s: %u command names, %u joystick models\n", fx_dir, n_cmd_names, n_models);
}

/* ---- the key bindings: the host's accelerator tables and key specs ----------
 *
 * The host keeps accelerator tables in a list (0x418278) of { active, id,
 * entries, count }, ids from a counter at 0x418294 that starts at 1 and skips
 * 0; entries are 12 bytes, { key, flags, command }.  Modelled here, reset per
 * side: +0x1d8 new, +0x1dc destroy, +0x1e4 load a resource table (from
 * ENGLISH.FLL, as the host's LoadAcceleratorsA does, each ACCEL converted by
 * 0x40cd50), +0x1e8/+0x1ec activate, +0x1f0 empty, +0x1f4 append, +0x1fc
 * copy out (mode 2).  A key spec in a profile is n numbers, written "%d"
 * joined by ","; +0xb0 and +0xd8 read them back with strtoul(p, &p, 0) and
 * strspn(", ") between. */

struct acc { uint32_t active, id, *ent, count, used; };
static struct acc accs[0x40];
static uint32_t acc_next;
static unsigned long cover_accload, cover_accadd, cover_keyspec;

static void acc_reset(void)
{
    uint32_t i;
    for (i = 0; i < 0x40; i++) { free(accs[i].ent); memset(&accs[i], 0, sizeof accs[i]); }
    acc_next = 1;
}
/* 0x410240 on 0x418278: the table with this id */
static struct acc *acc_find(uint32_t id)
{
    uint32_t i;
    for (i = 0; i < 0x40; i++) if (accs[i].used && accs[i].id == id) return &accs[i];
    return NULL;
}
/* 0x40cd10: a new zeroed table with the next id */
static struct acc *acc_new(void)
{
    uint32_t i;
    for (i = 0; i < 0x40; i++)
        if (!accs[i].used) {
            memset(&accs[i], 0, sizeof accs[i]);
            accs[i].used = 1;
            accs[i].id = acc_next;
            if (++acc_next == 0) acc_next = 1;
            return &accs[i];
        }
    printf("accelerator tables: out of room\n"); exit(2);
}
static uint32_t STDCALL h_acc_new(uint32_t *h)
{
    struct acc *a = acc_new();
    *h = a->id;
    put("\n  host+0x1d8()=%u", a->id);
    return 0;
}
static uint32_t STDCALL h_acc_free(uint32_t *h)
{
    struct acc *a = acc_find(*h);
    put("\n  host+0x1dc(%u)", *h);
    if (!a) { put("=-1"); return (uint32_t)-1; }
    free(a->ent);
    memset(a, 0, sizeof *a);
    *h = 0;
    return 0;
}
/* +0x1e4 (0x40c200) */
static uint32_t STDCALL h_acc_load(uint32_t *h, uint32_t res)
{
    HACCEL hacc = LoadAcceleratorsA(fll, MAKEINTRESOURCEA(res & 0xffff));
    ACCEL *a;
    struct acc *t;
    int n, i;
    put("\n  host+0x1e4(%#x)", res);
    if (!hacc) { *h = 0; put("=-1"); return (uint32_t)-1; }
    n = CopyAcceleratorTableA(hacc, NULL, 0);
    a = malloc(sizeof *a * (n ? n : 1));
    CopyAcceleratorTableA(hacc, a, n);
    DestroyAcceleratorTable(hacc);
    t = acc_new();
    *h = t->id;
    t->count = (uint32_t)n;
    t->ent = calloc((size_t)n * 3 + 1, 4);
    for (i = 0; i < n; i++) {
        uint32_t f = 0;
        if (a[i].fVirt & 4) f = 1;
        if (a[i].fVirt & 8) f |= 2;
        if (a[i].fVirt & 0x10) f |= 4;
        if (a[i].fVirt & 1) f |= 8;
        t->ent[i * 3 + 0] = a[i].key;
        t->ent[i * 3 + 1] = f;
        t->ent[i * 3 + 2] = (uint32_t)a[i].cmd + 0x10000;
    }
    free(a);
    if (counting) cover_accload++;
    put("=%u (%d entries)", t->id, n);
    return 0;
}
static uint32_t STDCALL h_acc_on(uint32_t h)
{
    struct acc *a;
    put("\n  host+0x1e8(%u)", h);
    if (!h || !(a = acc_find(h))) { put("=-1"); return (uint32_t)-1; }
    a->active = 1;
    return 0;
}
static uint32_t STDCALL h_acc_off(uint32_t h)
{
    struct acc *a;
    put("\n  host+0x1ec(%u)", h);
    if (!h || !(a = acc_find(h))) { put("=-1"); return (uint32_t)-1; }
    a->active = 0;
    return 0;
}
static uint32_t STDCALL h_acc_empty(uint32_t h)
{
    struct acc *a = acc_find(h);
    put("\n  host+0x1f0(%u)", h);
    if (!a) { put("=-1"); return (uint32_t)-1; }
    free(a->ent); a->ent = NULL; a->count = 0;
    return 0;
}
/* +0x1f4 (0x40c3d0) */
static uint32_t STDCALL h_acc_add(uint32_t h, const uint32_t *e, uint32_t n)
{
    struct acc *a = acc_find(h);
    uint32_t i;
    put("\n  host+0x1f4(%u,%u:", h, n);
    for (i = 0; i < n && i < 8; i++) put(" %#x/%#x/%#x", e[i * 3], e[i * 3 + 1], e[i * 3 + 2]);
    put(")");
    if (!a) { put("=-1"); return (uint32_t)-1; }
    a->ent = realloc(a->ent, (a->count + n) * 12 + 4);
    memcpy(a->ent + a->count * 3, e, n * 12);
    a->count += n;
    if (counting) cover_accadd += n;
    return 0;
}
/* +0x1fc (0x40c5b0): mode 2 copies from `start`; other modes do nothing */
static uint32_t STDCALL h_acc_get(uint32_t h, uint32_t a2, uint32_t start, uint32_t mode,
                                  uint32_t *buf, uint32_t *pcount)
{
    struct acc *a = acc_find(h);
    (void)a2;
    put("\n  host+0x1fc(%u,%u,%u,%s)", h, start, mode, buf ? "buf" : "null");
    if (!a) { put("=-1"); return (uint32_t)-1; }
    if (mode != 2) return 0;
    if (start > a->count) { *pcount = 0; return 0; }
    *pcount = a->count - start;
    if (buf) memcpy(buf, a->ent + start * 3, (a->count - start) * 12);
    put("=%u", *pcount);
    return 0;
}
/* the key specs, 0x408900 / 0x4089c0 / 0x408f00 */
static void keyspec_parse(char *b, uint32_t *out, uint32_t n)
{
    char *q = b;
    uint32_t i;
    for (i = 0; i < n; i++) {
        out[i] = (uint32_t)strtoul(q, &q, 0);
        q += strspn(q, ", ");
    }
}
static uint32_t STDCALL h_keyspec(const char *sec, const char *key, uint32_t *out, uint32_t n)
{
    char *b = malloc(n * 15 + 1);
    b[0] = 0;
    GetPrivateProfileStringA(sec, key, b, b, n * 15, fx_cfg);
    put("\n  host+0xb0("); put_str(sec); put(","); put_str(key); put(",%u)=", n); put_str(b);
    if (b[0]) { keyspec_parse(b, out, n); if (counting) cover_keyspec++; }
    free(b);
    return 0;
}
static uint32_t STDCALL h_keyspec_put(const char *sec, const char *key, const uint32_t *in, uint32_t n)
{
    char *b = malloc(n * 15 + 1), t[0x10];
    uint32_t i;
    b[0] = 0;
    for (i = 0; i < n; i++) {
        if (i) strcat(b, ",");
        sprintf(t, "%d", (int32_t)in[i]);
        strcat(b, t);
    }
    put("\n  host+0xb4("); put_str(sec); put(","); put_str(key); put(","); put_str(b); put(")");
    if (counting) cover_saves++;
    WritePrivateProfileStringA(sec, key, b, fx_cfg);
    free(b);
    return 0;
}
static uint32_t STDCALL h_keyspec_file(const char *sec, const char *key, uint32_t *out, uint32_t n,
                                       const char *path, int32_t ext)
{
    char full[0x200], *b;
    if (resolve(path, ext, full, sizeof full)) { put("\n  host+0xd8=-1"); return (uint32_t)-1; }
    b = malloc(n * 15 + 1);
    b[0] = 0;
    GetPrivateProfileStringA(sec, key, b, b, n * 15, full);
    put("\n  host+0xd8("); put_str(sec); put(","); put_str(key); put(",%u)=", n); put_str(b);
    if (b[0]) { keyspec_parse(b, out, n); if (counting) cover_keyspec++; }
    free(b);
    return 0;
}
/* +0x54 (0x4087c0): the main window and the instance */
static uint32_t STDCALL h_wndinst(uint32_t *wnd, uint32_t *inst)
{
    *wnd = 0x10020;
    *inst = 0x400000;
    put("\n  host+0x54()");
    return 0;
}

/* ---- DirectInput 5: a stand-in ----------------------------------------------
 *
 * CONTROLS loads it itself (LoadLibraryA "dinput", GetProcAddress
 * "DirectInputCreateA"), so both builds' import-table entries for those two
 * are pointed here.  The scenario chooses what the sim has: no DirectInput,
 * one that will not create, one with no force-feedback joystick, or one with
 * such a joystick (after a keyboard in the enumeration, sometimes).  The
 * interfaces are fakes with every method's arity, logging every call by
 * content -- GUIDs by name, a DIEFFECT and its arrays, envelope and
 * type-specific block whole -- and answering from the scenario: HRESULTs with
 * now and then a failure, Acquire DI_OK and then S_FALSE while acquired, an
 * effect's status playing or not.  Objects come from an arena reset per
 * side, so the pointers CONTROLS keeps in .data compare. */

static unsigned long cover_dicreate, cover_effects, cover_setparams, cover_effstart, cover_dinput,
    cover_stickset, cover_stickclamp;

struct fake { void *vtbl; uint32_t kind, id, refs, acquired; };
static struct fake fakes[0x40];
static uint32_t nfakes;
static void *vt_di[8], *vt_dev[27], *vt_eff[13];
static const GUID guid_stick = { 0x0badf00d, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } };
static const GUID iid_dev2 = { 0x5944e682, 0xc92e, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0, 0 } };
static const GUID guid_constant = { 0x13541c20, 0x8e33, 0x11d0, { 0x9a, 0xd0, 0, 0xa0, 0xc9, 0xa0, 0x6e, 0x35 } };
static const GUID guid_square = { 0x13541c22, 0x8e33, 0x11d0, { 0x9a, 0xd0, 0, 0xa0, 0xc9, 0xa0, 0x6e, 0x35 } };
static const GUID guid_sine = { 0x13541c23, 0x8e33, 0x11d0, { 0x9a, 0xd0, 0, 0xa0, 0xc9, 0xa0, 0x6e, 0x35 } };

static struct fake *fake_new(uint32_t kind)
{
    struct fake *f;
    if (nfakes >= 0x40) { printf("DirectInput stand-in: out of objects\n"); exit(2); }
    f = &fakes[nfakes];
    memset(f, 0, sizeof *f);
    f->vtbl = kind == 0 ? (void *)vt_di : kind == 1 ? (void *)vt_dev : (void *)vt_eff;
    f->kind = kind;
    f->id = nfakes++;
    f->refs = 1;
    return f;
}
static void di_reset(void) { nfakes = 0; memset(fakes, 0, sizeof fakes); }
#define FK(o) ((struct fake *)(o))
/* an HRESULT for a call: S_OK mostly, now and then a failure */
static uint32_t di_hr(void) { return rnd_n(24) ? 0 : 0x80004005; }
static const char *guid_name(const GUID *g)
{
    if (!g) return "null";
    if (!memcmp(g, &guid_constant, 16)) return "ConstantForce";
    if (!memcmp(g, &guid_square, 16)) return "Square";
    if (!memcmp(g, &guid_sine, 16)) return "Sine";
    if (!memcmp(g, &iid_dev2, 16)) return "IID_IDirectInputDevice2A";
    if (!memcmp(g, &guid_stick, 16)) return "the stick";
    return "other";
}
static void put_effect(const uint32_t *e)
{
    uint32_t i, n;
    if (!e) { put(" null"); return; }
    put(" {size %#x flags %#x dur %#x period %#x gain %#x trigger %#x repeat %#x axes %u:",
        e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
    n = e[7] < 8 ? e[7] : 8;
    for (i = 0; i < n; i++) put(" %#x", e[8] ? ((uint32_t *)(uintptr_t)e[8])[i] : 0);
    put(" dir");
    for (i = 0; i < n; i++) put(" %#x", e[9] ? ((uint32_t *)(uintptr_t)e[9])[i] : 0);
    if (e[10]) {
        const uint32_t *v = (const uint32_t *)(uintptr_t)e[10];
        put(" env %#x %#x %#x %#x %#x", v[0], v[1], v[2], v[3], v[4]);
    }
    put(" specific %u:", e[11]);
    for (i = 0; i < e[11] / 4 && i < 8 && e[12]; i++) put(" %#x", ((uint32_t *)(uintptr_t)e[12])[i]);
    put("}");
}

/* DirectInputCreateA(hinst, version, out, outer) */
static uint32_t STDCALL fk_create(uint32_t inst, uint32_t ver, uint32_t *out, uint32_t outer)
{
    put("\n  DirectInputCreateA(%#x,%#x,%#x)", inst, ver, outer);
    if (di_mode == DI_NOCREATE) { put("=E_FAIL"); return 0x80004005; }
    *out = (uint32_t)(uintptr_t)fake_new(0);
    if (counting) cover_dicreate++;
    put("=#%u", FK(*out)->id);
    return 0;
}
/* shared: AddRef/Release */
static uint32_t STDCALL fk_addref(void *o) { put("\n  di.AddRef(#%u)", FK(o)->id); return ++FK(o)->refs; }
static uint32_t STDCALL fk_release(void *o)
{
    put("\n  di.Release(#%u)", FK(o)->id);
    if (FK(o)->refs) FK(o)->refs--;
    return FK(o)->refs;
}
static uint32_t STDCALL fk_qi_none(void *o, const GUID *g, void **pp)
{
    put("\n  di.QueryInterface(#%u,%s)=E_NOINTERFACE", FK(o)->id, guid_name(g));
    *pp = NULL;
    return 0x80004002;
}
/* IDirectInputA */
static uint32_t STDCALL fk_di_createdevice(void *o, const GUID *g, uint32_t *out, uint32_t outer)
{
    put("\n  di.CreateDevice(#%u,%s,%#x)", FK(o)->id, di_mode == DI_DEVICE ? guid_name(g) : "-", outer);
    if (di_mode != DI_DEVICE || memcmp(g, &guid_stick, 16)) { put("=DIERR_DEVICENOTREG"); return 0x80040154; }
    if (rnd_n(24) == 0) { put("=E_FAIL"); return 0x80004005; }
    *out = (uint32_t)(uintptr_t)fake_new(1);
    put("=#%u", FK(*out)->id);
    return 0;
}
static uint32_t STDCALL fk_di_enum(void *o, uint32_t type, uint32_t cb, uint32_t ref, uint32_t flags)
{
    static uint8_t ddi[0x244];
    uint32_t r;
    put("\n  di.EnumDevices(#%u,%#x,%s,%#x)", FK(o)->id, type, ref ? "ref" : "null", flags);
    if (rnd_n(32) == 0) { put("=E_FAIL"); return 0x80004005; }
    memset(ddi, 0, sizeof ddi);
    *(uint32_t *)ddi = 0x244;
    if (di_keyboard_first || di_mode == DI_NODEVICE) {
        ddi[0x24] = 3;                                  /* DIDEVTYPE_KEYBOARD */
        r = ((uint32_t (STDCALL *)(uint32_t, uint32_t))(uintptr_t)cb)((uint32_t)(uintptr_t)ddi, ref);
        put(" cb(keyboard)=%u", r);
        if (r == 0 || di_mode == DI_NODEVICE) return 0;
    }
    memcpy(ddi + 4, &guid_stick, 16);
    *(uint32_t *)(ddi + 0x24) = 0x10204;                /* a joystick */
    strcpy((char *)ddi + 0x28, "FF Stick");
    r = ((uint32_t (STDCALL *)(uint32_t, uint32_t))(uintptr_t)cb)((uint32_t)(uintptr_t)ddi, ref);
    put(" cb(stick)=%u", r);
    return 0;
}
static uint32_t STDCALL fk_di_status(void *o, const GUID *g) { (void)g; put("\n  di.GetDeviceStatus(#%u)", FK(o)->id); return 0; }
static uint32_t STDCALL fk_di_panel(void *o, uint32_t w, uint32_t f) { (void)w; (void)f; put("\n  di.RunControlPanel(#%u)", FK(o)->id); return 0; }
static uint32_t STDCALL fk_di_init(void *o, uint32_t i, uint32_t v) { (void)i; (void)v; put("\n  di.Initialize(#%u)", FK(o)->id); return 0; }
/* IDirectInputDevice2A */
static uint32_t STDCALL fk_dev_qi(void *o, const GUID *g, uint32_t *pp)
{
    put("\n  di.QueryInterface(#%u,%s)", FK(o)->id, guid_name(g));
    if (memcmp(g, &iid_dev2, 16)) { *pp = 0; put("=E_NOINTERFACE"); return 0x80004002; }
    if (rnd_n(24) == 0) { *pp = 0; put("=E_FAIL"); return 0x80004005; }
    FK(o)->refs++;
    *pp = (uint32_t)(uintptr_t)o;
    return 0;
}
static uint32_t STDCALL fk_dev_unexpected1(void *o, uint32_t a) { (void)a; put("\n  di.unexpected(#%u)", FK(o)->id); return 0x80004001; }
static uint32_t STDCALL fk_dev_unexpected2(void *o, uint32_t a, uint32_t b) { (void)a; (void)b; put("\n  di.unexpected(#%u)", FK(o)->id); return 0x80004001; }
static uint32_t STDCALL fk_dev_unexpected3(void *o, uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; put("\n  di.unexpected(#%u)", FK(o)->id); return 0x80004001; }
static uint32_t STDCALL fk_dev_unexpected4(void *o, uint32_t a, uint32_t b, uint32_t c, uint32_t d) { (void)a; (void)b; (void)c; (void)d; put("\n  di.unexpected(#%u)", FK(o)->id); return 0x80004001; }
static uint32_t STDCALL fk_dev_unexpected0(void *o) { put("\n  di.unexpected(#%u)", FK(o)->id); return 0x80004001; }
static uint32_t STDCALL fk_dev_setprop(void *o, uint32_t prop, const uint32_t *ph)
{
    uint32_t r = di_hr();
    put("\n  di.SetProperty(#%u,%#x,{%#x %#x %#x %#x %#x})=%#x", FK(o)->id, prop, ph[0], ph[1], ph[2], ph[3], ph[4], r);
    return r;
}
static uint32_t STDCALL fk_dev_acquire(void *o)
{
    uint32_t r = FK(o)->acquired ? 1 : 0;
    if (rnd_n(20) == 0) r = 0x8007001e;                 /* DIERR_INPUTLOST */
    else FK(o)->acquired = 1;
    put("\n  di.Acquire(#%u)=%#x", FK(o)->id, r);
    return r;
}
static uint32_t STDCALL fk_dev_unacquire(void *o)
{
    uint32_t r = FK(o)->acquired ? 0 : 1;
    FK(o)->acquired = 0;
    put("\n  di.Unacquire(#%u)=%#x", FK(o)->id, r);
    return r;
}
static uint32_t STDCALL fk_dev_format(void *o, const uint32_t *df)
{
    uint32_t r = di_hr(), i, n = df[4] < 64 ? df[4] : 64, d = 0x811c9dc5u;
    const uint32_t *odf = (const uint32_t *)(uintptr_t)df[5];
    for (i = 0; i < n && odf; i++) {
        const uint8_t *g = (const uint8_t *)(uintptr_t)odf[i * 4];
        uint32_t k;
        for (k = 0; k < 16 && g; k++) d = (d ^ g[k]) * 0x01000193u;
        for (k = 1; k < 4; k++) d = (d ^ odf[i * 4 + k]) * 0x01000193u;
    }
    put("\n  di.SetDataFormat(#%u,{%#x %#x %#x %#x %u objects %08x})=%#x", FK(o)->id,
        df[0], df[1], df[2], df[3], df[4], d, r);
    return r;
}
static uint32_t STDCALL fk_dev_coop(void *o, uint32_t wnd, uint32_t flags)
{
    uint32_t r = di_hr();
    put("\n  di.SetCooperativeLevel(#%u,%#x,%#x)=%#x", FK(o)->id, wnd, flags, r);
    return r;
}
static uint32_t STDCALL fk_dev_createeffect(void *o, const GUID *g, const uint32_t *eff, uint32_t *out, uint32_t outer)
{
    uint32_t r = rnd_n(24) ? 0 : 0x80004005;
    put("\n  di.CreateEffect(#%u,%s,%#x)", FK(o)->id, guid_name(g), outer);
    put_effect(eff);
    if (r) { put("=E_FAIL"); return r; }
    *out = (uint32_t)(uintptr_t)fake_new(2);
    if (counting) cover_effects++;
    put("=#%u", FK(*out)->id);
    return 0;
}
/* IDirectInputEffect */
static uint32_t STDCALL fk_eff_setparams(void *o, const uint32_t *eff, uint32_t flags)
{
    uint32_t r = di_hr();
    put("\n  di.SetParameters(#%u,%#x)", FK(o)->id, flags);
    put_effect(eff);
    if (counting) {
        cover_setparams++;
        if (eff && eff[11] == 4 && eff[12]) {           /* the stick force's DICONSTANTFORCE */
            int32_t m = *(int32_t *)(uintptr_t)eff[12];
            cover_stickset++;
            if (m == 10000 || m == -10000) cover_stickclamp++;
        }
    }
    put("=%#x", r);
    return r;
}
static uint32_t STDCALL fk_eff_start(void *o, uint32_t n, uint32_t flags)
{
    uint32_t r = di_hr();
    if (counting) cover_effstart++;
    put("\n  di.Start(#%u,%u,%#x)=%#x", FK(o)->id, n, flags, r);
    return r;
}
static uint32_t STDCALL fk_eff_stop(void *o) { uint32_t r = di_hr(); put("\n  di.Stop(#%u)=%#x", FK(o)->id, r); return r; }
static uint32_t STDCALL fk_eff_download(void *o) { uint32_t r = di_hr(); put("\n  di.Download(#%u)=%#x", FK(o)->id, r); return r; }
static uint32_t STDCALL fk_eff_status(void *o, uint32_t *st)
{
    uint32_t r = rnd_n(24) ? 0 : 0x80004005;
    *st = rnd_n(3) ? 1 : 0;                             /* DIEGES_PLAYING */
    put("\n  di.GetEffectStatus(#%u)=%#x,%u", FK(o)->id, r, *st);
    return r;
}

static void di_vtables(void)
{
    uint32_t i;
    void *dev[27] = {
        fk_dev_qi, fk_addref, fk_release, fk_dev_unexpected1 /*GetCapabilities*/,
        fk_dev_unexpected3 /*EnumObjects*/, fk_dev_unexpected2 /*GetProperty*/, fk_dev_setprop,
        fk_dev_acquire, fk_dev_unacquire, fk_dev_unexpected2 /*GetDeviceState*/,
        fk_dev_unexpected4 /*GetDeviceData*/, fk_dev_format, fk_dev_unexpected1 /*SetEventNotification*/,
        fk_dev_coop, fk_dev_unexpected3 /*GetObjectInfo*/, fk_dev_unexpected1 /*GetDeviceInfo*/,
        fk_dev_unexpected2 /*RunControlPanel*/, fk_dev_unexpected3 /*Initialize*/, fk_dev_createeffect,
        fk_dev_unexpected4 /*EnumEffects*/, fk_dev_unexpected2 /*GetEffectInfo*/,
        fk_dev_unexpected1 /*GetForceFeedbackState*/, fk_dev_unexpected1 /*SendForceFeedbackCommand*/,
        fk_dev_unexpected3 /*EnumCreatedEffectObjects*/, fk_dev_unexpected1 /*Escape*/,
        fk_dev_unexpected0 /*Poll*/, fk_dev_unexpected4 /*SendDeviceData*/,
    };
    void *eff[13] = {
        fk_qi_none, fk_addref, fk_release, fk_dev_unexpected3 /*Initialize*/, fk_dev_unexpected1 /*GetEffectGuid*/,
        fk_dev_unexpected2 /*GetParameters*/, fk_eff_setparams, fk_eff_start, fk_eff_stop, fk_eff_status,
        fk_eff_download, fk_dev_unexpected0 /*Unload*/, fk_dev_unexpected1 /*Escape*/,
    };
    void *di[8] = {
        fk_qi_none, fk_addref, fk_release, fk_di_createdevice, fk_di_enum, fk_di_status, fk_di_panel, fk_di_init,
    };
    for (i = 0; i < 27; i++) vt_dev[i] = dev[i];
    for (i = 0; i < 13; i++) vt_eff[i] = eff[i];
    for (i = 0; i < 8; i++) vt_di[i] = di[i];
}

#define FAKE_DINPUT ((HMODULE)(uintptr_t)0x7ffd0000)
static HMODULE WINAPI h_LoadLibraryA(LPCSTR name)
{
    put("\n  LoadLibraryA("); put_str(name); put(")");
    if (name && !_strcmpi(name, "dinput")) {
        if (counting) cover_dinput++;
        if (di_mode == DI_ABSENT) { put("=null"); return NULL; }
        put("=dinput");
        return FAKE_DINPUT;
    }
    return LoadLibraryA(name);
}
static FARPROC WINAPI h_GetProcAddress(HMODULE m, LPCSTR name)
{
    put("\n  GetProcAddress("); put_str(name); put(")");
    if (m == FAKE_DINPUT) {
        if (!strcmp(name, "DirectInputCreateA") && !di_entry_missing) return (FARPROC)(void *)fk_create;
        put("=null");
        return NULL;
    }
    return GetProcAddress(m, name);
}

/* ACONTAIN +0x30 (.text+0xc50): key 0, the aircraft's own "folder\aircraft"
 * (.CFG), when there is one -- 1 and the path, else 0 with buf untouched */
static uint32_t STDCALL acontain_30(uint32_t a, uint32_t key, char *buf, uint32_t len)
{
    (void)a;
    put("\n  acontain+0x30(%#x,%#x)", key, len);
    if (fx_aircraft && key == 0) {
        snprintf(buf, len, "%s\\aircraft", fx_dir);
        put("=1");
        return 1;
    }
    put("=0");
    return 0;
}
/* +0xe0: a double from path + extension, *inout the default */
static uint32_t STDCALL h_prof_double(const char *sec, const char *key, double *inout,
                                      const char *path, int32_t ext)
{
    char full[0x200], b[0x20];
    if (resolve(path, ext, full, sizeof full)) { put("\n  host+0xe0=-1"); return (uint32_t)-1; }
    b[0] = 0;
    GetPrivateProfileStringA(sec, key, b, b, 0x1e, full);
    if (b[0]) *inout = strtod(b, NULL);
    put("\n  host+0xe0("); put_str(sec); put(","); put_str(key); put(")=%a", *inout);
    return 0;
}

/* point `mod`'s import of dll!fn at `repl` */
static void patch_iat(HMODULE mod, const char *dll, const char *fn, void *repl)
{
    uint8_t *b = (uint8_t *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(b +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; d->Name; d++) {
        IMAGE_THUNK_DATA *names, *iat;
        if (_strcmpi((char *)b + d->Name, dll)) continue;
        names = (IMAGE_THUNK_DATA *)(b + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        iat = (IMAGE_THUNK_DATA *)(b + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            DWORD old;
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            if (strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(b + names->u1.AddressOfData))->Name, fn)) continue;
            VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old);
            iat->u1.Function = (DWORD)(uintptr_t)repl;
            VirtualProtect(&iat->u1.Function, 4, old, &old);
            return;
        }
    }
    printf("cannot find the import %s!%s\n", dll, fn); exit(2);
}

/* MAIN */
static uint32_t STDCALL main_84(uint32_t rec, uint32_t flags, uint32_t block)
{ char b1[24], b2[24]; put("\n  main+0x84(%s,%#x,%s)", pptr(rec, b1), flags, pptr(block, b2)); return 0; }
static uint32_t STDCALL main_8c(uint32_t block)
{ char b[24]; put("\n  main+0x8c(%s)", pptr(block, b)); return 0; }
S0(main_90, "main+0x90")
S0(main_a4, "main+0xa4")
S0(main_ac, "main+0xac")
S0(main_c8, "main+0xc8")
/* WINDOW: most fire-and-forget; +0x84/+0x88 write the next/prev view id */
S2(win_24, "window+0x24")
S0(win_34, "window+0x34")
S0(win_38, "window+0x38")
S2(win_3c, "window+0x3c")
S1(win_48, "window+0x48")
S2(win_58, "window+0x58")
S2(win_5c, "window+0x5c")
S1(win_60, "window+0x60")
static uint32_t STDCALL win_84(uint32_t out, uint32_t w, uint32_t type)
{ *(uint32_t *)(uintptr_t)out = 0x5100 + (w & 0xff); put("\n  window+0x84(%#x,%#x)=%#x", w, type, *(uint32_t *)(uintptr_t)out); return 0; }
static uint32_t STDCALL win_88(uint32_t out, uint32_t w, uint32_t type)
{ *(uint32_t *)(uintptr_t)out = 0x5200 + (w & 0xff); put("\n  window+0x88(%#x,%#x)=%#x", w, type, *(uint32_t *)(uintptr_t)out); return 0; }
/* others */
S2(sound_24, "sound+0x24")
S0(panels_30, "panels+0x30")
S0(hg2d_64, "hg2d+0x64")
S0(atc_24, "atc+0x24")
static uint32_t STDCALL demo_24(uint32_t which, uint32_t value)
{ return logc("demo+0x24", 2, which & 0xffff, value & 0xffff, 0, 0, 0, 0); }
static uint32_t adv_ctr;
static uint32_t STDCALL advdrv_18(uint32_t pstate)
{ uint32_t v = (adv_ctr++ & 1) ? 0x20 : 0; *(uint32_t *)(uintptr_t)pstate = v; put("\n  advdrv+0x18()=%#x", v); return v != 0; }

/* ---- module loading --------------------------------------------------------- */

static void helper_load(struct helper *h, const char *path, uint32_t id)
{
    IMAGE_DOS_HEADER *dos; IMAGE_NT_HEADERS *nt; IMAGE_SECTION_HEADER *sec;
    struct import *im; uint32_t i;
    h->module = LoadLibraryA(path);
    if (!h->module) { printf("cannot load %s\n", path); exit(2); }
    h->link = (uint8_t *)GetProcAddress(h->module, "Linkage");
    im = (struct import *)GetProcAddress(h->module, "ImportTable");
    if (!h->link || !im || *(uint32_t *)h->link != id) { printf("%s: not module %u\n", path, id); exit(2); }
    for (; im->id; im++) {
        if (im->id == 1) im->link = host;
        else if (im->id == 2) im->link = global;
        else if (im->id == 17) im->link = numeric.link;
        else if (im->id == 20) im->link = util.link;
        else { printf("%s: unexpected import %u\n", path, im->id); exit(2); }
    }
    dos = (IMAGE_DOS_HEADER *)h->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)h->module + dos->e_lfanew);
    h->lo = (uint8_t *)h->module; h->hi = h->lo + nt->OptionalHeader.SizeOfImage;
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections && h->n < 8; i++)
        if (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            h->at[h->n] = h->lo + sec[i].VirtualAddress;
            h->size[h->n] = sec[i].Misc.VirtualSize;
            h->saved[h->n] = malloc(h->size[h->n]); h->n++;
        }
}
static void helper_save(struct helper *h) { uint32_t i; for (i = 0; i < h->n; i++) memcpy(h->saved[i], h->at[i], h->size[i]); }
static void helper_restore(struct helper *h) { uint32_t i; for (i = 0; i < h->n; i++) memcpy(h->at[i], h->saved[i], h->size[i]); }
static uint32_t helper_digest(struct helper *h)
{ uint32_t i, d = 0x811c9dc5u; for (i = 0; i < h->n; i++) d = digest(h->at[i], h->size[i], d); return d; }

static void build_stubs(void)
{
    uint32_t i;
#define TRAPS(t, id) for (i = 4; i < sizeof t; i += 4) *(uint32_t *)((t) + i) = 0xde000000u | ((id) << 16) | i
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(main_m, 3); TRAPS(window, 10); TRAPS(sound, 39);
    TRAPS(panels, 15); TRAPS(hg2d, 13); TRAPS(acontain, 23); TRAPS(advdrv, 21); TRAPS(atc, 24);
    TRAPS(demo, 7);
#undef TRAPS
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x3c, h_post); SET_SLOT(host, 0x24, h_quit); SET_SLOT(host, 0x48, h_48);
    SET_SLOT(host, 0x90, h_90); SET_SLOT(host, 0x254, h_menu); SET_SLOT(host, 0x274, h_capture);
    SET_SLOT(host, 0x278, h_release); SET_SLOT(host, 0x1e8, h_accel_on); SET_SLOT(host, 0x1ec, h_accel_off);
    SET_SLOT(host, 0x218, lh_memset); SET_SLOT(host, 0x1c4, h_joyread); SET_SLOT(host, 0x21c, lh_memmove);
    SET_SLOT(host, 0xa8, h_cfg_int); SET_SLOT(host, 0xac, h_cfg_put_int); SET_SLOT(host, 0xc8, h_cfg_str);
    SET_SLOT(host, 0xcc, h_cfg_put_str); SET_SLOT(host, 0xf8, h_cfg_clear); SET_SLOT(host, 0x100, h_cfg_has);
    SET_SLOT(host, 0x104, h_prof_has); SET_SLOT(host, 0xd0, h_prof_int); SET_SLOT(host, 0xf0, h_prof_str);
    SET_SLOT(host, 0x158, h_exists); SET_SLOT(host, 0x184, h_join); SET_SLOT(host, 0x130, h_simdir);
    SET_SLOT(host, 0x2a0, h_loadstring); SET_SLOT(host, 0x2a8, h_strcpy); SET_SLOT(host, 0x2b0, h_strcat);
    SET_SLOT(host, 0x2b8, h_strlen); SET_SLOT(host, 0x2c0, h_stricmp); SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x214, h_free); SET_SLOT(host, 0x1c0, h_joycount); SET_SLOT(host, 0x1c8, h_joydesc);
    SET_SLOT(host, 0x1d8, h_acc_new); SET_SLOT(host, 0x1dc, h_acc_free); SET_SLOT(host, 0x1e4, h_acc_load);
    SET_SLOT(host, 0x1e8, h_acc_on); SET_SLOT(host, 0x1ec, h_acc_off); SET_SLOT(host, 0x1f0, h_acc_empty);
    SET_SLOT(host, 0x1f4, h_acc_add); SET_SLOT(host, 0x1fc, h_acc_get); SET_SLOT(host, 0xb0, h_keyspec);
    SET_SLOT(host, 0xb4, h_keyspec_put); SET_SLOT(host, 0xd8, h_keyspec_file); SET_SLOT(host, 0x54, h_wndinst);
    SET_SLOT(host, 0xe0, h_prof_double); SET_SLOT(acontain, 0x30, acontain_30);
    di_vtables();
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add); SET_SLOT(chain, 0x1c, chain_remove); SET_SLOT(chain, 0x20, chain_dispatch);
    *(uint32_t *)main_m = 3;
    SET_SLOT(main_m, 0x84, main_84); SET_SLOT(main_m, 0x8c, main_8c); SET_SLOT(main_m, 0x90, main_90);
    SET_SLOT(main_m, 0xa4, main_a4); SET_SLOT(main_m, 0xac, main_ac); SET_SLOT(main_m, 0xc8, main_c8);
    *(uint32_t *)window = 10;
    SET_SLOT(window, 0x24, win_24); SET_SLOT(window, 0x34, win_34); SET_SLOT(window, 0x38, win_38);
    SET_SLOT(window, 0x3c, win_3c); SET_SLOT(window, 0x40, win_48 /*+0x40 close; log*/); SET_SLOT(window, 0x48, win_48);
    SET_SLOT(window, 0x58, win_58); SET_SLOT(window, 0x5c, win_5c); SET_SLOT(window, 0x60, win_60);
    SET_SLOT(window, 0x84, win_84); SET_SLOT(window, 0x88, win_88);
    *(uint32_t *)sound = 39; SET_SLOT(sound, 0x24, sound_24);
    *(uint32_t *)panels = 15; SET_SLOT(panels, 0x30, panels_30);
    *(uint32_t *)hg2d = 13; SET_SLOT(hg2d, 0x64, hg2d_64);
    *(uint32_t *)acontain = 23;
    *(uint32_t *)advdrv = 21; SET_SLOT(advdrv, 0x18, advdrv_18);
    *(uint32_t *)atc = 24; SET_SLOT(atc, 0x24, atc_24);
    *(uint32_t *)demo = 7; SET_SLOT(demo, 0x24, demo_24);
}

static uint32_t outside_digest(struct side *s)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)s->module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((uint8_t *)s->module + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    uint32_t i, d = 0x811c9dc5u;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        uint8_t *a = s->lo + sec[i].VirtualAddress, *b = a + sec[i].Misc.VirtualSize;
        if (s->image >= a && s->image < b) {
            d = digest(a, (uint32_t)(s->image - a), d);
            d = digest(s->image + IMAGE_SIZE, (uint32_t)(b - s->image - IMAGE_SIZE), d);
        } else d = digest(a, (uint32_t)(b - a), d);
    }
    return d;
}

static void load_side(struct side *s, const char *path, const char *name)
{
    struct import *im; IMAGE_DOS_HEADER *dos; IMAGE_NT_HEADERS *nt;
    memset(s, 0, sizeof *s);
    s->name = name; s->transcript = malloc(TRANSCRIPT_SIZE);
    s->module = LoadLibraryA(path);
    if (!s->module) { printf("cannot load %s (%lu)\n", path, GetLastError()); exit(2); }
    s->imports = (struct import *)GetProcAddress(s->module, "ImportTable");
    s->link = (uint8_t *)GetProcAddress(s->module, "Linkage");
    if (!s->imports || !s->link || *(uint32_t *)s->link != 16) { printf("%s: not CONTROLS\n", path); exit(2); }
    s->image = (uint8_t *)s->imports;             /* ImportTable at image+0 */
    if (s->link != s->image + 0x78) { printf("%s: Linkage not ImportTable+0x78\n", path); exit(2); }
    dos = (IMAGE_DOS_HEADER *)s->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)s->module + dos->e_lfanew);
    s->lo = (uint8_t *)s->module; s->hi = s->lo + nt->OptionalHeader.SizeOfImage;
    for (im = s->imports; im->id; im++) {
        switch (im->id) {
        case 1: im->link = host; break;        case 2: im->link = global; break;
        case 3: im->link = main_m; break;      case 4: im->link = chain; break;
        case 7: im->link = demo; break;        case 13: im->link = hg2d; break;
        case 15: im->link = panels; break;     case 17: im->link = numeric.link; break;
        case 20: im->link = util.link; break;  case 21: im->link = advdrv; break;
        case 23: im->link = acontain; break;   case 24: im->link = atc; break;
        case 39: im->link = sound; break;      case 10: im->link = window; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    patch_iat(s->module, "KERNEL32.dll", "LoadLibraryA", (void *)h_LoadLibraryA);
    patch_iat(s->module, "KERNEL32.dll", "GetProcAddress", (void *)h_GetProcAddress);
    memcpy(s->pristine, s->image, IMAGE_SIZE);
    s->outside = outside_digest(s);
}

/* ---- GLOBAL + view ---------------------------------------------------------- */

static uint32_t pick(const uint32_t *v, uint32_t n) { return v[rnd_n(n)]; }

static void g_setup(void)
{
    uint32_t i;
    memset(view, 0, sizeof view);
    for (i = 0; i + 4 <= GLOBAL_SIZE; i += 4) G32(i) = rnd_n(8) == 0 ? rnd() : 0;
    /* the gates the handlers read */
    G8(0x366) = rnd_n(2);                 /* on the ground */
    G8(0x609) = rnd_n(2);                 /* a jet */
    G8(0x838) = rnd_n(5) == 0 ? 1 + rnd_n(4) : 0;   /* a crash */
    G32(0x764) = rnd_n(2);                /* has an autopilot */
    G32(0x76c) = rnd_n(2); G32(0x774) = rnd_n(2); G32(0x778) = rnd_n(2);
    G32(0x7bc) = rnd_n(2);                /* autopilot available */
    G32(0x788) = rnd_n(2); G32(0x84c) = rnd_n(3) == 0; G32(0x850) = rnd_n(2);
    G32(0x60c) = pick((const uint32_t[]){ 0, 1, 2 }, 3);
    G16(0xb04) = rnd_n(2) ? 8 : 0;        /* a COM radio */
    G8(0xb68) = rnd_n(4) == 0; G8(0xb70) = rnd_n(4) == 0;
    G8(0xb64) = rnd_n(4) == 0; G8(0xb6f) = rnd_n(4) == 0;
    G32(0x760) = rnd_n(2);                /* a video records -> DEMO events */
    G32(0x122c) = 0;   /* skip EXIT's multiplayer confirm (UTIL's alert, not driven here) */
    G16(0xc18) = rnd_n(2) ? 2 : 0;        /* metric */
    G32(0x5d4) = rnd_n(2); G32(0x5d8) = 0;
    G16(0x5dc) = rnd_n(2);                /* slew */
    G32(0x628) = rnd_n(2); G32(0x758) = rnd_n(2);
    /* the engines selected */
    G32(0x888) = rnd_n(16); G16(0xaec) = 1 + rnd_n(4);
    /* steer onto the handler boundaries (method.md): exact sim-rate floor,
     * OBS wrap edge, and a throttle resting on the reverser floor */
    if (rnd_n(3) == 0) G16(0xc1a) = (const uint16_t[]){ 0x40, 0x8000, 0x80 }[rnd_n(3)];
    if (rnd_n(3) == 0) { G16(0xc4e) = 0x167 + rnd_n(3); G16(0xc5e) = 0x167 + rnd_n(3); }
    if (rnd_n(3) == 0) {
        uint16_t f = G16(0xb00);
        G16(0x88c) = f; G16(0x924) = f; G16(0x9bc) = f; G16(0xa54) = f;
    }
    /* an active view, half the time */
    if (rnd_n(2)) {
        *(uint32_t *)(view + 0x10) = rnd_n(2);          /* 0 3D, 1 map */
        *(uint32_t *)(view + 0x14) = pick((const uint32_t[]){ 0, 1, 4 }, 3);
        *(uint32_t *)(view + 0x18) = 0x9000 + rnd_n(8);
        *(uint16_t *)(view + 0x1c) = rnd(); *(uint16_t *)(view + 0x1e) = rnd();
        *(uint16_t *)(view + 0x20) = rnd(); *(uint16_t *)(view + 0x22) = rnd();
        *(uint16_t *)(view + 0x24) = rnd(); *(uint16_t *)(view + 0x26) = rnd_n(0xa00);
        *(uint16_t *)(view + 0x28) = rnd();
        *(uint32_t *)(view + 0x2c) = rnd(); *(uint32_t *)(view + 0x34) = rnd();
        G32(VIEW_AT) = (uint32_t)(uintptr_t)view;
        G32(WIN_AT) = 0x9000 + rnd_n(8);
    } else {
        G32(VIEW_AT) = 0; G32(WIN_AT) = 0;
    }
}

/* ---- driving ---------------------------------------------------------------- */

#define SVC(off) (*(void **)(cur->link + (off)))

/* a stdcall entry called with a chosen ecx, returning eax and the ecx left */
/* call a stdcall (fn)(a, b) with a chosen ecx; return eax, and the ecx left
 * in c2_ecx.  An explicit thunk, not inline asm, so the operands are
 * unambiguous: cdecl args fn=[esp+4], ecx=[esp+8], a=[esp+12], b=[esp+16]. */
uint32_t c2_ecx;
uint32_t call2(uint32_t fn, uint32_t ecx, uint32_t a, uint32_t b);
/* run CHAIN list `list` as retail CHAIN's dispatch does (CHAIN .text+0x220):
 * each callback in order, (a, b), with ecx = a */
static void chain_run(uint32_t list, uint32_t a, uint32_t b)
{
    uint32_t i;
    for (i = 0; i < ncbs[list]; i++)
        call2(cbs[list][i], a, a, b);
}
/* send a command as the game does, through CHAIN list 0xb (id, record with
 * the value at +8; what WM_COMMAND becomes), or straight to +0x44 */
static uint32_t record[4];
static void send_cmd(uint32_t via_chain, uint32_t id, uint32_t value)
{
    if (!via_chain) {
        call2((uint32_t)SVC(0x44), 0, id, value);
        return;
    }
    record[0] = rnd(); record[1] = rnd(); record[2] = value; record[3] = rnd();
    if (counting) cover_cmdcb++;
    chain_run(0xb, id, (uint32_t)(uintptr_t)record);
}
__asm__(".text\n\t.p2align 4\n"
        ".globl _call2\n"
        "_call2:\n\t"
        "pushl 16(%esp)\n\t"       /* b */
        "pushl 16(%esp)\n\t"       /* a */
        "movl 16(%esp), %ecx\n\t"  /* ecx */
        "call *12(%esp)\n\t"       /* fn (callee pops a,b) */
        "movl %ecx, _c2_ecx\n\t"
        "ret\n");
#define ret_ecx c2_ecx
static uint32_t call1(uint32_t fn, uint32_t a)
{
    uint32_t r;
    __asm__ volatile("pushl %2\n\tcall *%1" : "=a"(r) : "r"(fn), "r"(a) : "ecx", "edx", "memory", "cc");
    return r;
}
static uint32_t call0(uint32_t fn)
{
    uint32_t r;
    __asm__ volatile("call *%1" : "=a"(r) : "r"(fn) : "ecx", "edx", "memory", "cc");
    return r;
}

static void op(void)
{
    uint32_t k = rnd_n(24);
    uint32_t cmd, val = rnd_n(4) == 0 ? rnd() : rnd_n(0x10000);

    switch (k) {
    case 0: case 1: case 2: case 3: case 4: case 5: {
        /* the dispatcher +0x44 (id, value), ecx chosen */
        uint32_t ecx = rnd_n(4) == 0 ? rnd() : 0;
        cmd = rnd_n(4) == 0 ? 0x10000 + rnd_n(0x180) : rnd_n(0x180);
        put("\n#dispatch %#x val %#x ecx %#x", cmd, val, ecx);
        if (counting) { cover_cmd++; if (G8(0x838)) cover_cmd_crash++; }
        call2((uint32_t)SVC(0x44), ecx, cmd, val);
        put(" ->ecx %#x", ret_ecx);
        break;
    }
    case 6: {           /* an axis/trim step +0x28/+0x2c/+0x30/+0x34 */
        uint32_t off = (const uint32_t[]){ 0x28, 0x2c, 0x30, 0x34 }[rnd_n(4)];
        int32_t w = (int32_t)(rnd() & 0x7fff) - 0x4000;
        put("\n#axis +%#x %d", off, w);
        if (counting) cover_axis++;
        call1((uint32_t)SVC(off), (uint32_t)w);
        break;
    }
    case 7: {           /* engines by/to +0x38/+0x3c */
        uint32_t off = rnd_n(2) ? 0x38 : 0x3c, which = rnd_n(3);
        int32_t w = (int32_t)(rnd() & 0x7fff) - 0x4000;
        put("\n#engine +%#x which %u %d", off, which, w);
        if (counting) cover_engine++;
        call2((uint32_t)SVC(off), 0, (uint32_t)w, which);
        break;
    }
    case 8: {           /* a pilot event +0x40 (which, value) */
        uint32_t which = rnd_n(0x20), v = rnd();
        put("\n#event %u %#x", which, v);
        if (counting) cover_event++;
        call2((uint32_t)SVC(0x40), 0, which, v);
        break;
    }
    case 9:             /* report the radios +0x48 */
        put("\n#radios");
        call0((uint32_t)SVC(0x48));
        break;
    case 10: {          /* set the axis curves +0x18 (six words) */
        uint16_t c[6]; uint32_t i;
        for (i = 0; i < 6; i++) c[i] = rnd();
        put("\n#curves");
        call1((uint32_t)SVC(0x18), (uint32_t)(uintptr_t)c);
        break;
    }
    case 11:            /* yoke suspend/resume +0x1c/+0x20 */
        put("\n#yoke %u", k);
        call0((uint32_t)SVC(rnd_n(2) ? 0x1c : 0x20));
        break;
    case 12: {          /* ENGINE then SELECT_n: arms and applies engine select */
        uint32_t i, n = 1 + rnd_n(5), via = rnd_n(2);
        put("\n#engine-select%s", via ? " via list 0xb" : "");
        send_cmd(via, 0x10012, rnd());                            /* ENGINE */
        for (i = 0; i < n; i++)
            send_cmd(via, 0x10002 + rnd_n(4), rnd());             /* SELECT_1..4 */
        /* a kind-0 command now: through list 0xb it meets the S_SELECT
         * decrement (.text+0x3406), which +0x44 does not reach */
        send_cmd(via, 0x1000c + rnd_n(4), rnd());
        break;
    }
    case 13: {          /* the OBS/transponder steppers at a repeat */
        uint32_t c1 = 0x1007e + rnd_n(4), v1 = rnd(), v2 = rnd();
        put("\n#rep %#x v1=%#x v2=%#x", c1, v1, v2);
        call2((uint32_t)SVC(0x44), 0, c1, v1);   /* VOR OBI dec/inc */
        call2((uint32_t)SVC(0x44), 0, 0x100fe, v2);  /* SIM_RATE_DECR */
        break;
    }
    case 14: {          /* frames: CHAIN list 1 (the joystick poll, then the watcher
                         * while it is there), and list 6 now and then */
        uint32_t i, n = 1 + rnd_n(8);
        for (i = 0; i < n && nlist1; i++) {
            put("\n#jpoll");
            if (counting) cover_jpoll++;
            in_jpoll = 1;
            chain_run(1, 0, 0);
            in_jpoll = 0;
            if (rnd_n(4) == 0) {
                put("\n#list-6");
                if (counting) cover_list6 += ncbs[6] != 0;
                chain_run(6, 0, 0);
            }
        }
        break;
    }
    case 19:            /* +0x4c: load the key bindings (and bring force feedback up) */
        put("\n#keys-load -> %#x", call0((uint32_t)SVC(0x4c)));
        if (counting) cover_keysload++;
        break;
    case 20:            /* +0x50: save them (and take force feedback down) */
        put("\n#keys-save -> %#x", call0((uint32_t)SVC(0x50)));
        break;
    case 21: {          /* +0x54: the keys of some commands */
        static uint32_t pairs[16][2];
        uint32_t i, n = 1 + rnd_n(16);
        for (i = 0; i < n; i++) {
            pairs[i][0] = rnd_n(8) && n_cmd_ids ? cmd_ids[rnd_n(n_cmd_ids)] : rnd();
            pairs[i][1] = rnd();
        }
        put("\n#keys-list %u -> %#x", n, call2((uint32_t)SVC(0x54), 0, n, (uint32_t)(uintptr_t)pairs));
        for (i = 0; i < n; i++) put(" %#x:%#x", pairs[i][0], pairs[i][1]);
        break;
    }
    case 22: {          /* +0x5c, force feedback: op and its block, as MAIN, REALITY
                         * and the dialog call it -- the per-frame ones (3, the stick
                         * force; 9, the ground bumps) a few frames running */
        static uint32_t blk[8];
        /* weighted as the game calls them: the stick force and the ground bumps
         * every frame, a crash -- which holds the effects off until message 0x60 --
         * rarely */
        static const uint8_t ops[] = { 3, 3, 3, 3, 3, 3, 9, 9, 9, 9, 9, 4, 4, 5, 6, 7, 1, 1, 2, 2, 8 };
        uint32_t op = rnd_n(16) ? ops[rnd_n(sizeof ops)] : pick((const uint32_t[]){ 0, 10, 0xffffffff }, 3), i, arg;
        uint32_t n = (op == 3 || op == 9) ? 1 + rnd_n(6) : 1, j, speed = rnd_n(4) ? rnd_n(0x400) : rnd() & 0xffff;
        double x = 0, y = 0;
        for (i = 0; i < 8; i++) blk[i] = rnd_n(3) ? rnd_n(2) : rnd();
        if (op == 3) {
            /* forces in DirectInput's units, now and then exactly at the clamp */
            static const double edge[][2] = { { 10000, 0 }, { 6000, 8000 }, { 0, -10000 }, { -8000, 6000 } };
            uint32_t r = rnd_n(10);
            if (r < 6) { x = (double)((int)rnd_n(30001) - 15000); y = (double)((int)rnd_n(30001) - 15000); }
            else if (r < 7) { x = edge[rnd_n(4)][0]; y = edge[rnd_n(4)][1]; }
            else if (r < 8) { x = (double)((int)rnd_n(4001) - 2000) / 1000.0; y = (double)((int)rnd_n(4001) - 2000) / 1000.0; }
            else if (r < 9) x = y = 0.0;
            else { x = rnd_n(2) ? 0.0 : NAN; y = rnd_n(2) ? 0.0 : NAN; }
        }
        if (op == 9) blk[0] = rnd_n(5) != 0;
        for (j = 0; j < n; j++) {
            arg = (uint32_t)(uintptr_t)blk;
            if (op == 3) {
                memcpy(blk, &x, 8); memcpy(blk + 2, &y, 8);
                x += (double)((int)rnd_n(201) - 100); y += (double)((int)rnd_n(201) - 100);
            } else if (op == 4 || op == 8)
                arg = rnd_n(3) ? rnd_n(2) : rnd_n(4);
            else if (op == 9) {
                blk[2] = speed << 16 | (rnd() & 0xffff);
                speed = (speed + rnd_n(9) - 4) & 0xffff;
            }
            put("\n#force %#x", op);
            if (op == 1 || op == 2 || op == 3 || op == 9) for (i = 0; i < 7; i++) put(" %#x", blk[i]);
            else put(" arg %#x", arg);
            if (counting) cover_force++;
            put(" -> %#x", call2((uint32_t)SVC(0x5c), 0, op, arg));
            if (op == 1) for (i = 0; i < 7; i++) put(" %#x", blk[i]);
        }
        break;
    }
    case 23: {          /* CHAIN list 0x14, the sim's messages (force feedback listens) */
        static const uint32_t msgs[] = { 0x20, 0x38, 0x40, 0x42, 0x44, 0x60, 0x21, 0x1f, 0x61 };
        uint32_t m = rnd_n(3) == 0 ? 0x60 : rnd_n(8) ? msgs[rnd_n(9)] : rnd_n(0x80);
        put("\n#list-14 %#x", m);
        chain_run(0x14, m, 0);
        break;
    }
    case 18:            /* deinit (saving the joystick configs), then init again */
        put("\n#deinit -> %#x", call0((uint32_t)SVC(8)));
        if (counting) cover_deinit++;
        put("\n#init -> %#x", call0((uint32_t)SVC(4)));
        break;
    case 15: {          /* CHAIN list 0xb: a command (id, record), the value at +8;
                         * ids at the edges of the 0x10000..0x1dfff it accepts too */
        static const uint32_t edge[] = { 0xffff, 0x10000, 0x1dfff, 0x1e000 };
        uint32_t r = rnd_n(8);
        cmd = r == 0 ? rnd() : r == 1 ? edge[rnd_n(4)] : r == 2 ? 0x1e000 + rnd_n(0x10)
                     : 0x10000 + rnd_n(0x180);
        put("\n#list-b %#x val %#x", cmd, val);
        send_cmd(1, cmd, val);
        break;
    }
    case 16: {          /* CHAIN list 2, a run of frames: the repeat count, the brakes */
        uint32_t i, n = 1 + rnd_n(14);
        for (i = 0; i < n; i++) {
            put("\n#list-2");
            if (counting) cover_frame2++;
            chain_run(2, 0, 0);
        }
        break;
    }
    case 17:            /* CHAIN list 3: coordination */
        put("\n#list-3");
        if (counting) cover_frame3++;
        chain_run(3, 0, 0);
        break;
    }
}

static void put_state(void)
{
    uint8_t img[IMAGE_SIZE];
    uint32_t i;
    /* The two builds differ only in where their code/constants live, so the
     * cross-build-varying dwords are the fixed pointer fields: the Linkage
     * function slots (0x7c..0xd4), the dialog control procedures
     * (0x244/0x25c/0x260/0x26c), the inner .data pointers (0x344/0x350) and
     * the DirectInput object table (.rdata, 0x510..0x7c0).  Blank them all so
     * the digest covers the state, not the addresses.  (Arbitrary dwords must
     * NOT be paired by address: ours is a smaller module than retail, so a
     * data value can fall inside one module's image and not the other's.) */
    static const uint32_t ptroff[] = {
        0x7c, 0x80, 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4,
        0xb8, 0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0xd0, 0xd4,          /* Linkage fns */
        0x244, 0x25c, 0x260, 0x26c,                               /* dialog procs */
        0x344, 0x350,                                             /* inner .data */
    };
    memcpy(img, cur->image, IMAGE_SIZE);
    for (i = 0; i < sizeof ptroff / 4; i++)
        *(uint32_t *)(img + ptroff[i]) = 0;
    for (i = 0x510; i <= 0x7c0; i += 4)         /* the DirectInput object table */
        *(uint32_t *)(img + i) = 0;
    if (getenv("CONTROLS_AXES"))
        put("\n  elv=%04x ail=%04x rud=%04x trim=%04x bb2a=%04x",
            G16(0xbb2), G16(0xbb6), G16(0xbba), G16(0xbc0), G16(0x2b8));
    put("\n  img=%08x G=%08x view=%08x util=%08x numeric=%08x",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), digest(global, GLOBAL_SIZE, 0x811c9dc5u),
        digest(view, sizeof view, 0x811c9dc5u), helper_digest(&util), helper_digest(&numeric));
    if (getenv("CONTROLS_IMAGE")) {
        put("\n  nz:");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i)) put(" %x:%08x", i, *(uint32_t *)(img + i));
    }
}

/* seed a joystick config (D+0x350's table) and enable the joysticks, so the
 * per-frame poll (joy_poll) has axes and buttons to map.  Identical per side.
 * Layout: per joystick 0x3cc bytes -- +0x08 axis-present mask, +0x0c axis
 * command ids (0x10), +0x4c scales, +0x8c dead zones, then 0x40 each of
 * +0xcc button press commands, +0x1cc repeat intervals (in polls; 0 is no
 * repeat) and +0x2cc release commands.  The command ids fall in disjoint
 * ranges, per button for the buttons, so h_post can count what fired. */
static void joy_setup(void)
{
    uint32_t count = rnd_n(2) ? 1 + rnd_n(2) : 0, j, cfg = I32(0x350), a, b, r;
    memset(joy_down, 0, sizeof joy_down);
    memset(joy_was, 0, sizeof joy_was);
    for (j = 0; j < 4; j++) joy_pov[j] = -1;
    for (j = 0; j < count; j++) {
        uint32_t c = cfg + 0x3cc * j;
        M32(c + 8) = rnd() & 0xffff;                         /* axis-present mask */
        for (a = 0; a < 0x10; a++) {
            M32(c + 0xc + a * 4) = rnd_n(3) ? 0 : (0x10000 + rnd_n(0x80));
            M32(c + 0x4c + a * 4) = rnd() & 0xffff;
            M32(c + 0x8c + a * 4) = rnd_n(0x2000);
        }
        for (b = 0; b < 0x40; b++) {
            M32(c + 0xcc + b * 4) = rnd_n(2) ? 0 : 0x10080 + ((j * 0x40 + b) & 0x7f);
            r = rnd_n(6);       /* mostly short intervals, some none, some huge */
            M32(c + 0x1cc + b * 4) = r < 2 ? 0 : r < 5 ? 1 + rnd_n(4) : rnd();
            M32(c + 0x2cc + b * 4) = rnd_n(2) ? 0 : 0x10100 + ((j * 0x40 + b) & 0x7f);
        }
    }
    I32(0xbf88) = count;      /* joystick count; 0 leaves them to the watcher (list 1) */
    if (rnd_n(2)) I32(0x43c) = rnd_n(4);  /* the watcher's countdown (32 at load) to list 6 */
    I32(0xbf8c) = rnd_n(4) != 0;  /* enabled, mostly (list 3's coordination needs it off) */
}

static void run_side(struct side *s)
{
    uint32_t i, nops;
    cur = s; s->length = 0;
    adv_ctr = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    helper_restore(&util); helper_restore(&numeric);
    rng = scenario_seed;
    acc_reset();                     /* the host's accelerator tables, per side */
    di_reset();                      /* the DirectInput stand-in's objects, per side */
    fx_write();                      /* the profiles and sticks, identically per side */
    g_setup();                       /* rebuild GLOBAL and the view, identically per side */
    memset(ncbs, 0, sizeof ncbs);
    put("scenario %u", scenario);
    inside = 1;
    if (__builtin_setjmp(fault_jb) == 0) put(" init -> %#x", call0((uint32_t)SVC(4)));
    inside = 0;
    joy_setup();
    put(" (joysticks %u)\n", I32(0xbf88));
    nops = 4 + rnd_n(MAX_OPS);
    faulted = 0;
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i; inside = 1;
        if (__builtin_setjmp(fault_jb) == 0) op();
        inside = 0;
        put_state();
    }
    fx_dump();
    put("\n");
    cur = NULL;
}

static void show_difference(void)
{
    const char *a = retail.transcript, *b = ours.transcript;
    uint32_t n = retail.length < ours.length ? retail.length : ours.length, i, from;
    const char *ls;
    for (i = 0; i < n && a[i] == b[i]; i++) ;
    ls = a + i; while (ls > a && ls[-1] != '\n') ls--;
    printf("scenario %u differs at byte %u\n", scenario, i);
    from = (uint32_t)(ls - a) > 1200 ? (uint32_t)(ls - a) - 1200 : 0;
    printf("--- before:\n%.*s\n", (int)((uint32_t)(ls - a) - from), a + from);
    from = i - (uint32_t)(ls - a) > 400 ? i - 400 : (uint32_t)(ls - a);
    printf("--- retail:\n...%.800s\n", a + from);
    printf("--- ours:\n...%.800s\n", b + from);
}

static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    uint8_t *at = (uint8_t *)e->ExceptionRecord->ExceptionAddress;
    struct side *sd[2] = { &retail, &ours }; int i;
    for (i = 0; i < 2; i++)
        if (sd[i]->module && at >= sd[i]->lo && at < sd[i]->hi)
            printf("\nFAULT in %s CONTROLS at .text+%#x\n", sd[i]->name, (uint32_t)(at - sd[i]->lo - 0x1000));
    printf("FAULT %#lx at %p, scenario %u op %u, running %s\n", e->ExceptionRecord->ExceptionCode,
           (void *)at, scenario, op_number, cur ? cur->name : "none");
    if (cur && cur->length) {
        uint32_t from = cur->length > 2500 ? cur->length - 2500 : 0;
        printf("--- transcript tail:\n%.*s\n", (int)(cur->length - from), cur->transcript + from);
    }
    fflush(stdout); ExitProcess(3); return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 20000;
    uint32_t first = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0;
    uint32_t i, differ = 0;

    AddVectoredExceptionHandler(1, on_vectored);
    if (!getenv("CONTROLS_NOFILTER")) SetUnhandledExceptionFilter(on_fault);
    build_stubs();
    helper_load(&util, "UTIL_retail.dll", 20);
    helper_load(&numeric, "NUMERIC_retail.dll", 17);
    { struct import *im = (struct import *)GetProcAddress(util.module, "ImportTable");
      for (; im->id; im++) if (im->id == 17) im->link = numeric.link; }
    helper_save(&util); helper_save(&numeric);
    load_side(&retail, "CONTROLS_retail.dll", "retail");
    fx_init(retail.module);
    load_side(&ours, "CONTROLS_ours.dll", "ours");

    for (i = first; i < first + n; i++) {
        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        scenario_seed = rnd() | 1;
        counting = 1; run_side(&retail);
        counting = 0; run_side(&ours);
        if (getenv("CONTROLS_DUMP")) {
            FILE *f = fopen("controls_retail.txt", "wb"); fwrite(retail.transcript, 1, retail.length, f); fclose(f);
            f = fopen("controls_ours.txt", "wb"); fwrite(ours.transcript, 1, ours.length, f); fclose(f);
        }
        if (retail.length != ours.length || memcmp(retail.transcript, ours.transcript, retail.length)) {
            if (differ++ < 3) show_difference();
        }
        if (differ && getenv("CONTROLS_FIRST")) break;      /* a break run: one is enough */
        if ((i + 1 - first) % 1000 == 0) { printf("%u scenarios, %u differ\n", i + 1 - first, differ); fflush(stdout); }
    }
    printf("controlsdiff: %u scenarios, %u differ\n", n, differ);
    printf("  dispatches %lu (after a crash %lu), axes %lu, engines %lu, events %lu\n",
           cover_cmd, cover_cmd_crash, cover_axis, cover_engine, cover_event);
    printf("  list 0xb commands %lu, list 2 frames %lu, list 3 frames %lu\n",
           cover_cmdcb, cover_frame2, cover_frame3);
    printf("  list 6 runs %lu, deinits %lu; profile hits %lu, devices-file hits %lu, strings %lu, writes %lu\n",
           cover_list6, cover_deinit, cover_cfghit, cover_devhit, cover_strings, cover_saves);
    printf("  key loads %lu: tables loaded %lu, entries added %lu, key specs read %lu; dinput asked %lu\n",
           cover_keysload, cover_accload, cover_accadd, cover_keyspec, cover_dinput);
    printf("  force feedback %lu calls: DirectInput created %lu, effects %lu, started %lu, set %lu"
           " (stick force %lu, at the clamp %lu)\n", cover_force, cover_dicreate, cover_effects,
           cover_effstart, cover_setparams, cover_stickset, cover_stickclamp);
    printf("  joystick polls %lu: posted axes %lu, presses %lu, repeats %lu, releases %lu\n",
           cover_jpoll, cover_jaxis, cover_jpress, cover_jrepeat, cover_jrelease);
    printf("  posts %lu, faults (both sides alike) %lu\n", cover_post, cover_faults);
    return differ ? 1 : 0;
}
