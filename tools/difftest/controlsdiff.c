/* Differential test: retail CONTROLS.DLL against the OpenFS98 build, the
 * command dispatcher and its handlers.
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
 *
 * It does NOT drive deinit (+0x08), the CHAIN list 0xb/2/3 callbacks, or
 * the config entries (+0x4c/+0x50/+0x58/+0x5c) yet:
 * those need the host's profile, accelerator, joystick and DirectInput
 * services modelled.  That is the next phase; see docs/modules/controls.md.
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
/* +0xc8 GetPrivateProfileString on FLTSIM98.CFG: leave the default buffer */
static uint32_t STDCALL h_cfg(uint32_t section, uint32_t key, uint32_t buf, uint32_t size)
{ (void)section; (void)key; (void)buf; (void)size; return 0; }
static uint32_t STDCALL h_cfg3(uint32_t section, uint32_t key, uint32_t buf)
{ (void)section; (void)key; (void)buf; return 0; }
/* +0x218 memset(dst, byte, n) */
static uint32_t STDCALL h_memset(uint32_t dst, uint32_t byte, uint32_t n)
{ memset((void *)(uintptr_t)dst, (int)byte, n); return 0; }
static uint32_t STDCALL h_memmove(uint32_t dst, uint32_t src, uint32_t n)
{ memmove((void *)(uintptr_t)dst, (void *)(uintptr_t)src, n); return 0; }
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
/* CHAIN: +0x18/+0x1c register, +0x20 dispatch.  List-1 callbacks are captured
 * in order so the harness can drive the joystick poll (first registered). */
static uint32_t list1[4]; static uint32_t nlist1;
static uint32_t STDCALL chain_add(uint32_t list, uint32_t cb)
{
    char b[24];
    if (list == 1 && nlist1 < 4) list1[nlist1++] = cb;
    put("\n  chain+0x18(%#x,%s)", list, pptr(cb, b));
    return 0;
}
static uint32_t STDCALL chain_remove(uint32_t list, uint32_t cb)
{ char b[24]; put("\n  chain+0x1c(%#x,%s)", list, pptr(cb, b)); return 0; }
S3(chain_dispatch, "chain+0x20")
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
    SET_SLOT(host, 0xc8, h_cfg); SET_SLOT(host, 0x218, h_memset); SET_SLOT(host, 0x1c4, h_joyread);
    SET_SLOT(host, 0xa8, h_cfg3); SET_SLOT(host, 0x21c, h_memmove);
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
    uint32_t k = rnd_n(15);
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
        uint32_t i, n = 1 + rnd_n(5);
        put("\n#engine-select");
        call2((uint32_t)SVC(0x44), 0, 0x10012, rnd());        /* ENGINE */
        for (i = 0; i < n; i++)
            call2((uint32_t)SVC(0x44), 0, 0x10002 + rnd_n(4), rnd());  /* SELECT_1..4 */
        /* a kind-0 command now, to hit the S_SELECT decrement */
        call2((uint32_t)SVC(0x44), 0, 0x1000c + rnd_n(4), rnd());
        break;
    }
    case 13: {          /* the OBS/transponder steppers at a repeat */
        uint32_t c1 = 0x1007e + rnd_n(4), v1 = rnd(), v2 = rnd();
        put("\n#rep %#x v1=%#x v2=%#x", c1, v1, v2);
        call2((uint32_t)SVC(0x44), 0, c1, v1);   /* VOR OBI dec/inc */
        call2((uint32_t)SVC(0x44), 0, 0x100fe, v2);  /* SIM_RATE_DECR */
        break;
    }
    case 14: {          /* the per-frame joystick poll (CHAIN list 1), a run of frames */
        uint32_t i, n = 1 + rnd_n(8);
        for (i = 0; i < n && nlist1; i++) {
            put("\n#jpoll");
            if (counting) cover_jpoll++;
            in_jpoll = 1;
            call2(list1[0], 0, 0, 0);
            in_jpoll = 0;
        }
        break;
    }
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
    uint32_t count = 1 + rnd_n(2), j, cfg = I32(0x350), a, b, r;
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
    I32(0xbf88) = count;      /* joystick count */
    I32(0xbf8c) = 1;          /* enabled */
}

static void run_side(struct side *s)
{
    uint32_t i, nops;
    cur = s; s->length = 0;
    adv_ctr = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    helper_restore(&util); helper_restore(&numeric);
    rng = scenario_seed;
    g_setup();                       /* rebuild GLOBAL and the view, identically per side */
    nlist1 = 0;
    inside = 1;
    if (__builtin_setjmp(fault_jb) == 0) call0((uint32_t)SVC(4));   /* module init */
    inside = 0;
    joy_setup();
    put("scenario %u (joysticks %u)\n", scenario, I32(0xbf88));
    nops = 4 + rnd_n(MAX_OPS);
    faulted = 0;
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i; inside = 1;
        if (__builtin_setjmp(fault_jb) == 0) op();
        inside = 0;
        put_state();
    }
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
        if ((i + 1 - first) % 1000 == 0) { printf("%u scenarios, %u differ\n", i + 1 - first, differ); fflush(stdout); }
    }
    printf("controlsdiff: %u scenarios, %u differ\n", n, differ);
    printf("  dispatches %lu (after a crash %lu), axes %lu, engines %lu, events %lu\n",
           cover_cmd, cover_cmd_crash, cover_axis, cover_engine, cover_event);
    printf("  joystick polls %lu: posted axes %lu, presses %lu, repeats %lu, releases %lu\n",
           cover_jpoll, cover_jaxis, cover_jpress, cover_jrepeat, cover_jrelease);
    printf("  posts %lu, faults (both sides alike) %lu\n", cover_post, cover_faults);
    return differ ? 1 : 0;
}
