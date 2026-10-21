/* PANELS.DLL -- the instrument panel and the gauges' runtime.
 *
 * Ordinary MSVC-compiled C, transliterated function by function from the
 * retail module (linked at 0x10000000); each function is headed by its
 * retail address.  Before changing anything, know that:
 *
 *   - Everything lives in one .data image laid out exactly as retail's
 *     (0x16580 bytes from 0x1000f000): ImportTable at +0x0, Linkage at
 *     +0x50, the gauge variables' table at +0x610.  The code addresses it
 *     by retail's addresses through DA()/D32() and friends, so 0x10025020
 *     is the panel windows' list in both.  tools/gen_panels_data.py
 *     extracts the image; its code pointers and its pointers to the GLOBAL
 *     link are installed by panels_install().
 *   - The floating point is retail's, operation for operation, on the x87:
 *     long double where retail keeps a value on the FPU stack, double or
 *     float where it stores one, and pn_ftol for MSVCRT's _ftol.  fsin and
 *     fcos are inline x87.
 *   - The host's lists (+0x2f8 add, +0x2fc free, +0x300 remove, +0x304
 *     reset, +0x308 walk, +0x310 find) hold PANELS's state: 0x10025000 the
 *     loaded gauges (a Linkage pointer each), 0x10025020 the panel windows
 *     (0x50 bytes each), 0x10010ec0 the saved window layouts (0x28 bytes).
 *   - Retail's bugs are kept; each is noted where it happens.
 *
 * docs/modules/panels.md has the module's survey and the harness.
 */
#define __USE_MINGW_ANSI_STDIO 0      /* MSVCRT's own sprintf, as retail's */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs98/linkage.h"
#include "panels.h"

/* ---- the x87 ---------------------------------------------------------------------- */

/* MSVCRT's _ftol: truncation towards zero, 64 bits, the x87's indefinite
 * when out of range.  Callers keep 32 bits of it, as retail does. */
static int64_t pn_ftol(long double v)
{
    if (!(v >= -9223372036854775808.0L && v < 9223372036854775808.0L))
        return (int64_t)0x8000000000000000LL;
    return (int64_t)v;
}
#define FTOL(v) ((uint32_t)pn_ftol(v))

static long double pn_fsin(long double x)
{
    long double r;
    __asm__ ("fsin" : "=t" (r) : "0" (x));
    return r;
}

static long double pn_fcos(long double x)
{
    long double r;
    __asm__ ("fcos" : "=t" (r) : "0" (x));
    return r;
}

/* idiv of edx:eax by d, as retail does it (a quotient that overflows faults
 * into the host's divide repair, as retail's does) */
static int32_t pn_idiv64(int32_t hi, uint32_t lo, int32_t d)
{
    int32_t q, r;
    __asm__ volatile ("idivl %4" : "=a" (q), "=d" (r) : "a" (lo), "d" (hi), "rm" (d));
    (void)r;
    return q;
}

/* imul's 64-bit product shifted down 16 (shrd), its low half */
#define MUL16(a, b) ((uint32_t)(((int64_t)(int32_t)(a) * (int32_t)(b)) >> 16))

/* fild of a dword and of a qword whose high half is zero */
#define FI(v)  ((long double)(int32_t)(v))
#define FIU(v) ((long double)(uint32_t)(v))

/* ---- the host's services, by their confirmed ret N -------------------------------- */

#define h_post(a, b)            HC(0x03c, f2)((a), (b))
#define h_busy(on)              HC(0x050, f1)(on)
#define h_window_create(w)      HC(0x064, f1)(P(w))
#define h_window_destroy(w)     HC(0x068, f1)(P(w))
#define h_window_update(w)      HC(0x07c, f1)(P(w))
#define h_window_invalidate(w, r) HC(0x088, f2)(P(w), P(r))
#define h_cfg_int_get(s, k, p)  HC(0x0a8, f3)(P(s), P(k), P(p))
#define h_cfg_int_put(s, k, v)  HC(0x0ac, f3)(P(s), P(k), (v))
#define h_profile_int(s, k, p, path, e) HC(0x0d0, f5)(P(s), P(k), P(p), P(path), (e))
#define h_profile_num(s, k, p, path, e) HC(0x0e0, f5)(P(s), P(k), P(p), P(path), (e))
#define h_profile_string(s, k, b, n, path, e) \
    HC(0x0f0, f6)(P(s), P(k), P(b), (n), P(path), (e))
#define h_exists(path, e)       HC(0x158, f2)(P(path), (e))
#define h_path_join(o, d, dir, n) HC(0x184, f4)(P(o), P(d), P(dir), P(n))
#define h_surface_new(o, w, h, t) HC(0x1b4, f4)(P(o), (w), (h), (t))
#define h_surface_free(s)       HC(0x1b8, f1)(s)
#define h_alloc(f, n, o, z)     HC(0x20c, f4)((f), (n), P(o), (z))
#define h_free(pp)              HC(0x214, f1)(P(pp))
#define h_memset(d, b, n)       HC(0x218, f3)(P(d), (b), (n))
#define h_memmove(d, s, n)      HC(0x21c, f3)(P(d), P(s), (n))
#define h_module_load(o, path, e) HC(0x26c, f3)(P(o), P(path), (e))
#define h_module_release(o)     HC(0x270, f1)(P(o))
#define h_child_add(a, b, x, y) HC(0x27c, f4)((a), P(b), (x), (y))
#define h_child_remove(a, b)    HC(0x284, f2)((a), P(b))
#define h_module_enter(l, o)    HC(0x298, f2)((l), P(o))
#define h_module_leave(t)       HC(0x29c, f1)(t)
#define h_palette_copy(b)       HC(0x28c, f1)(P(b))
#define h_load_string(id, b, n) HC(0x2a0, f3)((id), P(b), (n))
#define h_strcpy(d, s)          HC(0x2a8, f2)(P(d), P(s))
#define h_strdup(s)             HC(0x2b4, f1)(P(s))
#define h_strlen(s)             HC(0x2b8, f1)(P(s))
#define h_strcmp(a, b)          HC(0x2bc, f2)(P(a), P(b))
#define h_list_add(l)           HC(0x2f8, f1)(P(l))
#define h_list_free(l)          HC(0x2fc, f1)(P(l))
#define h_list_remove(l, e)     HC(0x300, f2)(P(l), P(e))
#define h_list_reset(l)         HC(0x304, f1)(P(l))
#define h_list_walk(l, c, fn)   HC(0x308, f3)(P(l), (c), P(fn))
#define h_list_find(l, o, k, n, x) HC(0x310, f5)(P(l), (o), P(k), (n), (x))

/* PANELS's own lists and tables */
#define GAUGE_LIST   DA(0x10025000)     /* a gauge module's Linkage pointer each */
#define PANEL_LIST   DA(0x10025020)     /* a panel window, 0x50 bytes each */
#define LAYOUT_LIST  DA(0x10010ec0)     /* a saved window layout, 0x28 bytes each */
#define WINDOW_TABLE 0x10020f00u        /* 64 x { panel window, section name[0x100] } */
#define WT_ENTRY(i)  (WINDOW_TABLE + 0x104 * (i))

#define S_IMAGE_QUALITY DA(0x1000f104)
#define S_PANELS        DA(0x1000f0fc)
#define S_GAUGE_FMT     ((const char *)(uintptr_t)DA(0x1000f118))   /* "Gauge%02d" */
#define S_IDENT         DA(0x1000f124)
#define S_VISIBLE       DA(0x1000f12c)
#define S_POSITION      DA(0x1000f134)
#define S_RATIO         DA(0x1000f140)
#define S_SIZE_Y_MM     DA(0x1000f154)
#define S_SIZE_MM       DA(0x1000f160)
#define S_FILE          DA(0x1000f168)
#define S_TITLES        DA(0x1000f170)
#define S_WINDOW_FMT    ((const char *)(uintptr_t)DA(0x1000f180))   /* "Window%02d" */
#define S_COLOURS       DA(0x1000f18c)
#define S_COLOUR_FMT    ((const char *)(uintptr_t)DA(0x1000f19c))   /* "Color%02d" */
#define S_BOUNDS        DA(0x1000f1a8)                               /* "FS6BOUNDS" */
#define S_RB            ((const char *)(uintptr_t)DA(0x1000f114))   /* "rb" */

/* ---- the functions, in retail's order ---------------------------------------------- */

static void panels_unload(void);                                 /* 0x100012a0 */
static void FS98_STDCALL windows_show_all(void);                 /* 0x10001320 */
static void FS98_STDCALL windows_hide_all(void);                 /* 0x100013a0 */
static uint32_t FS98_STDCALL windows_any_shown(void);            /* 0x10001510 */
static uint32_t FS98_STDCALL window_show(uint32_t w);            /* 0x100012e0 */
static uint32_t FS98_STDCALL window_hide(uint32_t w);            /* 0x10001300 */
static uint32_t FS98_STDCALL window_ident(uint32_t w);           /* 0x10001d40 */
static void bitmap_size(const char *path, int32_t *w, int32_t *h);   /* 0x10001d80 */
static uint32_t bitmap_load(const char *path);                   /* 0x10001e20 */
static uint32_t FS98_STDCALL panel_rebuild(uint32_t e, uint32_t w);  /* 0x10001fa0 */
static uint32_t FS98_STDCALL panel_reload_gauges(uint32_t e, uint32_t w);   /* 0x100020e0 */
static uint32_t FS98_STDCALL gauge_unload_for(uint32_t e, uint32_t w);      /* 0x100022b0 */
static uint32_t FS98_STDCALL panel_paint(uint32_t mode, uint32_t w, uint32_t arg,
                                         uint32_t rect);         /* 0x100022f0 */
static void panels_load(uint32_t aircraft);                      /* 0x10002410 */
static void panels_refresh(uint32_t aircraft);                   /* 0x10002fb0 */
static uint32_t panel_window_new(uint32_t title);                /* 0x100032a0 */
static uint32_t bounds_build(uint32_t surface);                  /* 0x100032f0 */
static void gauge_load(uint32_t e, const char *name, int32_t x, int32_t y,
                       int32_t w);                               /* 0x100034c0 */
static uint32_t FS98_STDCALL panel_free(uint32_t e, uint32_t c); /* 0x100035f0 */
static uint32_t FS98_STDCALL gauge_free(uint32_t e, uint32_t c); /* 0x10003670 */
static void remap_reset(uint32_t all);                           /* 0x100036b0 */
static void scale_24(uint32_t info, uint32_t bits, uint32_t dst);    /* 0x10003740 */
static void scale_8(uint32_t info, uint32_t bits, uint32_t dst);     /* 0x10003840 */

static uint32_t surface_from_bitmap(uint32_t info, uint32_t flags, int32_t w, int32_t h,
                                    uint32_t out, uint32_t a6, uint32_t a7,
                                    uint32_t a8);                /* 0x10005660 */
static uint32_t FS98_STDCALL surface_convert(uint32_t e, uint32_t pair, uint32_t at,
                                             uint32_t flags);    /* 0x100060d0 */
static void FS98_STDCALL blit(uint32_t dst, uint32_t src, uint32_t at_dst, uint32_t at_src,
                              uint32_t size, uint32_t flags);    /* 0x10006dc0 */
static void failures_reset(uint32_t list);                       /* 0x10009060 */
static uint32_t failure_state(uint32_t list);                    /* 0x10009090 */
static void trim(uint32_t s);                                    /* 0x100090d0 */
static uint32_t token(uint32_t src, uint32_t dst, uint32_t delim);   /* 0x10009120 */
static uint32_t surfaces_free(uint32_t p);                       /* 0x10009400 */
static void rotate(uint32_t pt, double angle, uint32_t out);     /* 0x10009460 */
static uint32_t panel_children_reset(uint32_t e, uint32_t x);    /* 0x10009570 */
static void dirty_set(uint32_t flags);                           /* 0x10009590 */
static void chain_register(void);                                /* 0x10009c30 */
static void chain_unregister(void);                              /* 0x10009c60 */

/* the CHAIN callbacks */
static uint32_t FS98_STDCALL cb_frame(uint32_t a, uint32_t b);       /* 0x10009860, list 2 */
static uint32_t FS98_STDCALL cb_reset(uint32_t a, uint32_t b);       /* 0x100099d0, list 1 */
static uint32_t FS98_STDCALL cb_situation(uint32_t m, uint32_t p);   /* 0x10009a90, list 0x14 */
static uint32_t FS98_STDCALL cb_engines(uint32_t a, uint32_t b);     /* 0x10009b60, list 3 */
static uint32_t FS98_STDCALL cb_command(uint32_t m, uint32_t p);     /* 0x10009be0, list 0xc */

/* 0x10001020 -- Linkage +0x04 */
static void FS98_STDCALL panels_init(void)
{
    h_load_string(0x9053, DA(0x10025340), 0x100);
    D32(0x10025008) = 0x20;
    D32(0x10025028) = 0x20;
    D32(0x10010ec8) = 0x20;
    D32(0x10025000) = 0;
    D32(0x10025004) = 0;
    D32(0x1002500c) = 4;
    D32(0x10025010) = 0;
    D32(0x10025014) = 0;
    D32(0x10025018) = 0;
    D32(0x10025020) = 0;
    D32(0x10025024) = 0;
    D32(0x1002502c) = 0x50;
    D32(0x10025030) = 0;
    D32(0x10025034) = 0;
    D32(0x10025038) = 0;
    D32(0x10010ec0) = 0;
    D32(0x10010ec4) = 0;
    D32(0x10010ecc) = 0x28;
    D32(0x10010ed0) = 0;
    D32(0x10010ed4) = 0;
    D32(0x10010ed8) = 0;
    D32(0x10010e60) = 0;
    D32(0x10010e64) = 0;
    CH(0x18, f2)(3, ADDR(cb_engines));
    CH(0x18, f2)(0x14, ADDR(cb_situation));
    CH(0x18, f2)(0xc, ADDR(cb_command));
    D32(0x10025568) = 1;
    h_cfg_int_get(S_PANELS, S_IMAGE_QUALITY, DA(0x10025568));
    h_cfg_int_put(S_PANELS, S_IMAGE_QUALITY, D32(0x10025568));
}

/* the element types' handlers, 0x10009e70..0x1000d340: by type (0..7), one
 * table per Linkage operation */
#define ELEMENT_FN1(x) static uint32_t FS98_STDCALL e_##x(uint32_t e);
#define ELEMENT_FN2(x) static uint32_t FS98_STDCALL e_##x(uint32_t e, uint32_t a);
#define ELEMENT_FN0(x) static uint32_t e_##x(void);
ELEMENT_FN2(9e70) ELEMENT_FN2(a170) ELEMENT_FN2(ae40) ELEMENT_FN2(b770)
ELEMENT_FN2(bbf0) ELEMENT_FN2(c130) ELEMENT_FN2(c720) ELEMENT_FN2(d330)
ELEMENT_FN1(9f20) ELEMENT_FN1(a470) ELEMENT_FN1(b1a0) ELEMENT_FN1(b860)
ELEMENT_FN1(bd30) ELEMENT_FN1(c300) ELEMENT_FN1(cbf0)
ELEMENT_FN2(9f30) ELEMENT_FN2(a6f0) ELEMENT_FN2(b250) ELEMENT_FN2(b940)
ELEMENT_FN2(bd80) ELEMENT_FN2(c3e0) ELEMENT_FN2(ccd0)
ELEMENT_FN1(9f80) ELEMENT_FN1(a790) ELEMENT_FN1(b280) ELEMENT_FN1(b9a0)
ELEMENT_FN1(be10) ELEMENT_FN1(c460) ELEMENT_FN1(cda0)
ELEMENT_FN1(a070) ELEMENT_FN1(acf0) ELEMENT_FN1(b580) ELEMENT_FN1(bae0)
ELEMENT_FN1(bff0) ELEMENT_FN1(c5f0) ELEMENT_FN1(d1d0)
ELEMENT_FN1(a110) ELEMENT_FN1(adb0) ELEMENT_FN1(b630) ELEMENT_FN1(bb90)
ELEMENT_FN1(c0a0) ELEMENT_FN1(c6a0) ELEMENT_FN1(d280)
ELEMENT_FN0(a140) ELEMENT_FN0(ae10) ELEMENT_FN0(b740) ELEMENT_FN0(bbc0)
ELEMENT_FN0(c100) ELEMENT_FN0(c6f0) ELEMENT_FN0(d300) ELEMENT_FN0(d340)

/* an element: +0x00 its type, +0x20 its gauge's header, +0x24 its parent,
 * +0x28 its children (a null-ended array), +0x2c its failures, +0x30 its
 * image flags, +0x34 its dirty flags */
#define EL_TYPE(e)     R32(e, 0x00)
#define EL_GAUGE(e)    R32(e, 0x20)
#define EL_PARENT(e)   R32(e, 0x24)
#define EL_CHILDREN(e) R32(e, 0x28)

/* 0x10001140 -- Linkage +0x08 */
static void FS98_STDCALL panels_deinit(void)
{
    CH(0x1c, f2)(3, ADDR(cb_engines));
    CH(0x1c, f2)(0x14, ADDR(cb_situation));
    CH(0x1c, f2)(0xc, ADDR(cb_command));
    h_list_free(GAUGE_LIST);
    h_list_free(PANEL_LIST);
    h_list_free(LAYOUT_LIST);
}

/* 0x100011b0 -- Linkage +0x18: the aircraft `a` is in.  Its panel file
 * (ACONTAIN key 6) is compared with the last one's: the same file only
 * refreshes the windows, another unloads the panel and loads the new. */
static uint32_t FS98_STDCALL panels_aircraft(uint32_t a)
{
    if (AC(0x30, f4)(a, 6, DA(0x10010c50), 0x104) == 0)
        return 0;
    h_busy(1);
    if (h_strcmp(DA(0x10010c50), DA(0x10010d58)) == 0) {
        panels_refresh(a);
    } else {
        h_strcpy(DA(0x10010d58), DA(0x10010c50));
        if (D32(0x10010e94) == 0)
            h_list_reset(LAYOUT_LIST);
        panels_unload();
        G16(0xb02) = 0;
        G16(0xb04) = 0;
        G32(0xb10) = 0;
        G32(0xb14) = 0;
        panels_load(a);
        chain_register();
        CH(0x20, f3)(0x14, 0x70, 0);
    }
    D32(0x10010e94) = 0;
    return h_busy(0);
}

/* 0x100012a0 */
static void panels_unload(void)
{
    windows_hide_all();
    chain_unregister();
    h_list_walk(GAUGE_LIST, 0, gauge_free);
    h_list_walk(PANEL_LIST, 0, panel_free);
}

/* 0x100012e0 -- Linkage +0x20: show a panel window */
static uint32_t FS98_STDCALL window_show(uint32_t w)
{
    R32(w, 8) |= 0x80000001;
    return h_window_update(w);
}

/* 0x10001300 -- Linkage +0x24: hide one */
static uint32_t FS98_STDCALL window_hide(uint32_t w)
{
    R32(w, 8) &= 0x7ffffffe;
    return h_window_update(w);
}

/* 0x10001320 -- Linkage +0x28: show every panel window that was shown
 * (bit 0) and is not kept off (bit 30) */
static void FS98_STDCALL windows_show_all(void)
{
    uint32_t list, n, i, f;

    WN(0x80, f3)(P(&list), P(&n), 2);
    for (i = 0; i < n; i++) {
        f = R32(R32(list, 4 * i), 8);
        if ((f & 0x40000000) == 0 && (f & 1) != 0) {
            R32(R32(list, 4 * i), 8) = f | 0x80000001;
            h_window_update(R32(list, 4 * i));
        }
    }
    h_free(&list);
}

/* 0x100013a0 -- Linkage +0x2c: hide every shown one, leaving bit 0 */
static void FS98_STDCALL windows_hide_all(void)
{
    uint32_t list, n, i, f;

    WN(0x80, f3)(P(&list), P(&n), 2);
    for (i = 0; i < n; i++) {
        f = R32(R32(list, 4 * i), 8);
        if ((f & 0x40000000) == 0 && (f & 0x80000000) != 0) {
            R32(R32(list, 4 * i), 8) = f & 0x7fffffff;
            h_window_update(R32(list, 4 * i));
        }
    }
    h_free(&list);
}

/* 0x10001420 -- Linkage +0x30: toggle the panel */
static void FS98_STDCALL windows_toggle(void)
{
    if (windows_any_shown())
        windows_hide_all();
    else
        windows_show_all();
}

/* 0x10001440 -- Linkage +0x34: GLOBAL's display options to every panel
 * window's flags */
static void FS98_STDCALL windows_options(void)
{
    uint32_t list, n, i, w;

    WN(0x80, f3)(P(&list), P(&n), 2);
    for (i = 0; i < n; i++) {
        w = R32(list, 4 * i);
        if (G32(0x1224) != 0)
            R32(w, 8) &= 0xfff7ffff;
        else
            R32(w, 8) |= 0x80000;
        w = R32(list, 4 * i);
        if (G32(0x1228) != 0)
            R32(w, 8) |= 0x800;
        else
            R32(w, 8) &= 0xfffff7ff;
        h_window_update(R32(list, 4 * i));
    }
    h_free(&list);
}

/* 0x10001510 -- Linkage +0x38: is any panel window shown? */
static uint32_t FS98_STDCALL windows_any_shown(void)
{
    uint32_t list, n, i, any = 0, f;

    WN(0x80, f3)(P(&list), P(&n), 2);
    for (i = 0; i < n; i++) {
        f = R32(R32(list, 4 * i), 8);
        if ((f & 0x40000000) == 0 && (f & 0x80000000) != 0)
            any = 1;
    }
    h_free(&list);
    return any;
}

/* 0x10001950 -- Linkage +0x3c: an element's failures and dirty flags */
static uint32_t FS98_STDCALL element_failures(uint32_t e)
{
    failures_reset(R32(e, 0x2c));
    dirty_set(R32(e, 0x34));
    return 0;
}

/* 0x10001580 -- Linkage +0x40: install an element (its gauge's header's
 * +0x28 is `a`) */
static uint32_t FS98_STDCALL element_install(uint32_t e, uint32_t a)
{
    if (e == 0)
        return 0;
    R32(EL_GAUGE(e), 0x28) = a;
    switch (EL_TYPE(e)) {
    case 0: return e_9e70(e, a);
    case 1: return e_a170(e, a);
    case 2: return e_ae40(e, a);
    case 3: return e_b770(e, a);
    case 4: return e_bbf0(e, a);
    case 5: return e_c130(e, a);
    case 6: return e_c720(e, a);
    case 7: return e_d330(e, a);
    }
    return e;
}

/* 0x10001610 -- Linkage +0x44: clear an element's image flags' top bit */
static uint32_t FS98_STDCALL element_unmark(uint32_t e)
{
    if (e != 0)
        R32(e, 0x30) &= 0x7fffffff;
    return e;
}

/* 0x10001630 -- Linkage +0x48 */
static uint32_t FS98_STDCALL element_op48(uint32_t e)
{
    switch (EL_TYPE(e)) {
    case 0: return e_9f20(e);
    case 1: return e_a470(e);
    case 2: return e_b1a0(e);
    case 3: return e_b860(e);
    case 4: return e_bd30(e);
    case 5: return e_c300(e);
    case 6: return e_cbf0(e);
    case 7: return e_9f20(e);
    }
    return e;
}

/* 0x100016b0 -- Linkage +0x4c */
static uint32_t FS98_STDCALL element_op4c(uint32_t e, uint32_t a)
{
    switch (EL_TYPE(e)) {
    case 0: return e_9f30(e, a);
    case 1: return e_a6f0(e, a);
    case 2: return e_b250(e, a);
    case 3: return e_b940(e, a);
    case 4: return e_bd80(e, a);
    case 5: return e_c3e0(e, a);
    case 6: return e_ccd0(e, a);
    case 7: return e_d330(e, a);
    }
    return e;
}

/* 0x10001760 -- Linkage +0x50 */
static uint32_t FS98_STDCALL element_op50(uint32_t e)
{
    switch (EL_TYPE(e)) {
    case 0: return e_9f80(e);
    case 1: return e_a790(e);
    case 2: return e_b280(e);
    case 3: return e_b9a0(e);
    case 4: return e_be10(e);
    case 5: return e_c460(e);
    case 6: return e_cda0(e);
    case 7: return e_9f20(e);
    }
    return e;
}

/* 0x100017e0 */
static uint32_t FS98_STDCALL element_op17e0(uint32_t e)
{
    if (e == 0)
        return 0;
    switch (EL_TYPE(e)) {
    case 0: return e_a070(e);
    case 1: return e_acf0(e);
    case 2: return e_b580(e);
    case 3: return e_bae0(e);
    case 4: return e_bff0(e);
    case 5: return e_c5f0(e);
    case 6: return e_d1d0(e);
    case 7: return e_9f20(e);
    }
    return e;
}

/* 0x10001860 -- Linkage +0x58 */
static uint32_t FS98_STDCALL element_op58(uint32_t e)
{
    switch (EL_TYPE(e)) {
    case 0: return e_a110(e);
    case 1: return e_adb0(e);
    case 2: return e_b630(e);
    case 3: return e_bb90(e);
    case 4: return e_c0a0(e);
    case 5: return e_c6a0(e);
    case 6: return e_d280(e);
    case 7: return e_9f20(e);
    }
    return e;
}

/* 0x100018e0 -- Linkage +0x5c: by type alone */
static uint32_t FS98_STDCALL element_op5c(uint32_t type)
{
    switch (type) {
    case 0: return e_a140();
    case 1: return e_ae10();
    case 2: return e_b740();
    case 3: return e_bbc0();
    case 4: return e_c100();
    case 5: return e_c6f0();
    case 6: return e_d300();
    case 7: return e_d340();
    }
    return type;
}

/* 0x10001970 -- Linkage +0x60: a tree's failures, parents set on the way */
static uint32_t FS98_STDCALL tree_failures(uint32_t e)
{
    uint32_t c;

    if (e == 0)
        return 0;
    element_failures(e);
    c = EL_CHILDREN(e);
    if (c == 0)
        return 0;
    while (M32(c) != 0) {
        tree_failures(M32(c));
        R32(M32(c), 0x24) = e;
        c += 4;
    }
    return 0;
}

/* 0x100019b0 -- Linkage +0x64: install a tree */
static uint32_t FS98_STDCALL tree_install(uint32_t e, uint32_t a)
{
    uint32_t c;

    if (e == 0)
        return 0;
    element_install(e, a);
    c = EL_CHILDREN(e);
    if (c == 0)
        return 0;
    while (M32(c) != 0) {
        tree_install(M32(c), a);
        R32(M32(c), 0x24) = e;
        c += 4;
    }
    return 0;
}

/* 0x100019f0 */
static uint32_t FS98_STDCALL tree_unmark(uint32_t e)
{
    uint32_t c;

    if (e == 0)
        return 0;
    R32(e, 0x30) &= 0x7fffffff;
    c = EL_CHILDREN(e);
    if (c == 0)
        return 0;
    while (M32(c) != 0) {
        tree_unmark(M32(c));
        c += 4;
    }
    return 0;
}

/* 0x10001a30 -- Linkage +0x68 */
static uint32_t FS98_STDCALL tree_unmark_entry(uint32_t e)
{
    if (e != 0)
        return tree_unmark(e);
    return e;
}

/* 0x10001a50 -- Linkage +0x6c */
static uint32_t FS98_STDCALL tree_op48(uint32_t e)
{
    uint32_t c;

    if (e == 0)
        return 0;
    element_op48(e);
    c = EL_CHILDREN(e);
    if (c == 0)
        return 0;
    while (M32(c) != 0) {
        tree_op48(M32(c));
        c += 4;
    }
    return 0;
}

/* 0x10001a80 -- Linkage +0x70 */
static uint32_t FS98_STDCALL tree_op4c(uint32_t e, uint32_t a)
{
    uint32_t c;

    if (e == 0)
        return 0;
    element_op4c(e, a);
    c = EL_CHILDREN(e);
    if (c == 0)
        return 0;
    while (M32(c) != 0) {
        tree_op4c(M32(c), a);
        c += 4;
    }
    return 0;
}

/* 0x10001ac0 -- Linkage +0x74: a failed element (3) is not drawn, a
 * blanked one (4) hides its children too */
static uint32_t FS98_STDCALL tree_op50(uint32_t e)
{
    uint32_t c, k;

    if (e == 0)
        return 0;
    k = failure_state(R32(e, 0x2c));
    if (k != 3)
        element_op50(e);
    if (k == 4)
        return k;
    c = EL_CHILDREN(e);
    if (c == 0)
        return 0;
    while (M32(c) != 0) {
        tree_op50(M32(c));
        c += 4;
    }
    return 0;
}

/* 0x10001b10 -- Linkage +0x78 (and +0x54 through 0x10001b50): children
 * first */
static uint32_t FS98_STDCALL tree_op17e0(uint32_t e)
{
    uint32_t c;

    if (e == 0)
        return 0;
    c = EL_CHILDREN(e);
    if (c != 0)
        while (M32(c) != 0) {
            tree_op17e0(M32(c));
            c += 4;
        }
    return element_op17e0(e);
}

/* 0x10001b50 -- Linkage +0x54 */
static uint32_t FS98_STDCALL tree_op17e0_entry(uint32_t e)
{
    return tree_op17e0(e);
}

/* 0x10001b60 -- Linkage +0x7c: children first */
static uint32_t FS98_STDCALL tree_op58(uint32_t e)
{
    uint32_t c;

    if (e == 0)
        return 0;
    c = EL_CHILDREN(e);
    if (c != 0)
        while (M32(c) != 0) {
            tree_op58(M32(c));
            c += 4;
        }
    return element_op58(e);
}

/* 0x10001ba0 -- Linkage +0x80: a gauge's mouse rectangles scaled by its
 * header's +0x34/+0x38 and placed at `pt` */
static uint32_t FS98_STDCALL mouse_place(uint32_t m, uint32_t hdr, uint32_t pt)
{
    float sx = RFL(hdr, 0x34), sy = RFL(hdr, 0x38);
    uint32_t r;

    R32(m, 4) = FTOL(FI(R32(m, 4)) * sx);
    R32(m, 8) = FTOL(FI(R32(m, 8)) * sy);
    R32(m, 0xc) = R32(pt, 0);
    R32(m, 0x10) = R32(pt, 4);
    for (r = m + 0x2c; R32(r, 0) != 0; r += 0x2c) {
        R32(r, 4) = FTOL(FI(R32(r, 4)) * sx);
        R32(r, 8) = FTOL(FI(R32(r, 8)) * sy);
        R32(r, 0xc) = FTOL(FI(R32(r, 0xc)) * sx);
        R32(r, 0x10) = FTOL(FI(R32(r, 0x10)) * sy);
    }
    return R32(r, 0);
}

/* 0x10001c40 -- Linkage +0x84: register a gauge's mouse rectangles with its
 * window, at the gauge's place */
static uint32_t FS98_STDCALL mouse_add(uint32_t m, uint32_t hdr)
{
    return h_child_add(R32(hdr, 0x3c), m, R32(hdr, 0x44) + R32(m, 4),
                       R32(hdr, 0x48) + R32(m, 8));
}

/* 0x10001c70 -- Linkage +0x88 */
static uint32_t FS98_STDCALL mouse_remove(uint32_t m, uint32_t hdr)
{
    return h_child_remove(R32(hdr, 0x3c), m);
}

/* 0x10001c90 -- Linkage +0x8c: toggle the panel window whose ident is `id`;
 * 1 if there was one */
static uint32_t FS98_STDCALL window_toggle(uint32_t id)
{
    uint32_t list, n, i, k, any = 0, w;

    WN(0x80, f3)(P(&list), P(&n), 2);
    for (i = 0; i < n; i++) {
        k = window_ident(R32(list, 4 * i));
        if (k != 0xffffffff && id == k) {
            w = R32(list, 4 * i);
            if (R32(w, 8) & 0x80000000)
                window_hide(w);
            else
                window_show(w);
            any = 1;
        }
    }
    h_free(&list);
    return any;
}

/* 0x10001d20 -- Linkage +0x90: post a command */
static uint32_t FS98_STDCALL post_command(uint32_t a, uint32_t b)
{
    return h_post(a, b);
}

/* 0x10001d40 -- a panel window's ident, or -1 if it is not in the table.
 * Retail reads each entry's window through the entry without checking it:
 * a window past the loaded ones faults on a null entry, as here. */
static uint32_t FS98_STDCALL window_ident(uint32_t w)
{
    uint32_t a, i = 0;

    for (a = WINDOW_TABLE; a < 0x10025000u; a += 0x104, i++)
        if (R32(D32(a), 4) == w)
            return R32(D32(WT_ENTRY(i)), 0);
    return 0xffffffff;
}

/* 0x10001d80 -- a .BMP's width and height, or 1 and 1 */
static void bitmap_size(const char *path, int32_t *w, int32_t *h)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    FILE *f;

    *w = 1;
    *h = 1;
    f = fopen(path, S_RB);
    if (f == NULL)
        return;
    if (fread(&fh, 0xe, 1, f) != 0 && fh.bfType == 0x4d42 &&
        fread(&ih, 0x28, 1, f) != 0 && ih.biSize == 0x28) {
        *w = ih.biWidth;
        *h = ih.biHeight;
    }
    fclose(f);
}

/* 0x10001e20 -- a .BMP into one host block: its BITMAPINFOHEADER, the
 * palette for 8 bits, then the bits.  0 on any failure.  A file that does
 * not open is passed to fclose as retail passes it. */
static uint32_t bitmap_load(const char *path)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    uint32_t blk = 0, bits, size, n;
    FILE *f;

    f = fopen(path, S_RB);
    if (f == NULL)
        goto fail;
    if (fread(&fh, 0xe, 1, f) == 0 || fh.bfType != 0x4d42)
        goto fail;
    if (fread(&ih, 0x28, 1, f) == 0 || ih.biSize != 0x28)
        goto fail;
    size = fh.bfSize - fh.bfOffBits + 0x28;
    if (ih.biBitCount == 8)
        size += 0x400;
    h_alloc(1, size, &blk, 1);
    if (blk == 0)
        goto fail;
    h_memmove(blk, &ih, 0x28);
    bits = R32(blk, 0) + blk;
    if (R16(blk, 0xe) == 8) {
        R32(blk, 0x20) = 0x100;
        bits += 0x400;
        if (R16(blk, 0xe) == 8 && fread((void *)(uintptr_t)(blk + 0x28), 0x400, 1, f) == 0)
            goto fail;
    }
    fseek(f, fh.bfOffBits, 0);
    n = fread((void *)(uintptr_t)bits, 1, fh.bfSize - fh.bfOffBits, f);
    if (n != fh.bfSize - fh.bfOffBits)
        goto fail;
    fclose(f);
    return blk;
fail:
    fclose(f);
    h_free(&blk);
    return 0;
}

/* 0x10001fa0 -- a panel window walk (0x10025020, context the window): its
 * surfaces rebuilt at the window's new size from its bitmap */
static uint32_t FS98_STDCALL panel_rebuild(uint32_t e, uint32_t ctx)
{
    uint32_t w = R32(e, 4), bmp, s, r;
    int32_t width;

    if (ctx != w)
        return 0;
    h_free(w + 0x10);
    R32(w, 0x10) = 0;
    surfaces_free(e + 8);
    r = D32(0x1002503c);
    if (r != 0) {
        R32(w, 0x68) = R32(r, 8) - R32(r, 0);
        R32(w, 0x70) = R32(w, 0x68);
        r = D32(0x1002503c);
        R32(w, 0x6c) = R32(r, 0xc) - R32(r, 4);
        R32(w, 0x74) = R32(w, 0x6c);
    }
    h_window_update(w);
    panel_children_reset(e, 0);
    bmp = bitmap_load((const char *)(uintptr_t)R32(e, 0x4c));
    surface_from_bitmap(bmp, 0, R32(w, 0x84), R32(w, 0x88), e + 0xc, 0, 0, 0);
    s = R32(e, 0xc);
    R32(e, 8) = s;
    width = R32(e, 0x18);
    R32(e, 0x1c) = (uint32_t)width * R32(bmp, 8) / R32(bmp, 4);
    RFL(e, 0x20) = FI(R32(w, 0x84)) / FI(R32(bmp, 4));
    RFL(e, 0x24) = FI(R32(w, 0x88)) / FI(R32(bmp, 8));
    RFL(e, 0x28) = FI(R32(w, 0x84)) / FI(width);
    RFL(e, 0x2c) = FI(R32(w, 0x88)) / FI(R32(e, 0x1c));
    R32(w, 0x10) = bounds_build(s);
    h_window_update(w);
    h_free(&bmp);
    return 0;
}

/* 0x100020e0 -- a panel window walk (0x10025020, context the window): its
 * gauges loaded again from the panel file.  The section is the window's
 * title unless the table names the window. */
static uint32_t FS98_STDCALL panel_reload_gauges(uint32_t e, uint32_t ctx)
{
    char cfg[0x104], val[0x100], name[0x100], tok[0x100], key[0x28];
    uint32_t w = R32(e, 4), sect, i, n, p;
    int32_t x, y, wd;

    if (ctx != w)
        return 0;
    if (AC(0x30, f4)(D32(0x10020ee0), 6, P(cfg), 0x104) == 0)
        return 0;
    n = D32(0x10010e6c);
    sect = R32(w, 0);
    for (i = 0; i < n; i++)
        if (R32(D32(WT_ENTRY(i)), 4) == w) {
            sect = DA(WT_ENTRY(i) + 4);
            break;
        }
    i = 0;
    sprintf(key, S_GAUGE_FMT, i);
    val[0] = 0;
    if (h_profile_string(sect, key, val, 0x100, cfg, 0x24) == 0)
        return 0;
    do {
        p = token(P(val), P(name), 0x2c);
        p = token(p, P(tok), 0x2c);
        x = atoi(tok);
        p = token(p, P(tok), 0x2c);
        y = atoi(tok);
        if (p != 0) {
            token(p, P(tok), 0x2c);
            wd = atoi(tok);
        } else {
            wd = 0;
        }
        gauge_load(e, name, x, y, wd);
        i++;
        sprintf(key, S_GAUGE_FMT, i);
        val[0] = 0;
    } while (h_profile_string(sect, key, val, 0x100, cfg, 0x24) != 0);
    return 0;
}

/* 0x100022b0 -- a gauge walk (0x10025000, context a panel window): the
 * window's gauges unloaded */
static uint32_t FS98_STDCALL gauge_unload_for(uint32_t e, uint32_t ctx)
{
    uint32_t hdr = R32(M32(e), 0x18);

    if (ctx == R32(R32(hdr, 0x40), 4)) {
        CALL(hdr, 0x24, f0)();
        h_module_release(e);
        h_list_remove(GAUGE_LIST, e);
    }
    return 0;
}

/* 0x100022f0 -- a panel window's paint callback (its record's +0x18).
 * Mode 1 with a nonzero low word, or 3, on a shown window whose size has
 * changed, or 3 before its bounds exist, rebuilds the window: its gauges
 * unloaded, its surfaces remade at the new size, the gauges loaded again.
 * Returns what retail leaves in eax: the nesting count when nested, else
 * the rectangle, or the busy cursor's result after a rebuild. */
static uint32_t FS98_STDCALL panel_paint(uint32_t mode, uint32_t w, uint32_t arg,
                                         uint32_t rect)
{
    uint32_t first, resized;

    if (D32(0x10010e70) != 0)
        return D32(0x10010e70);
    first = R32(w, 0x10) == 0 && mode == 3;
    resized = 0;
    if (((mode == 1 && (arg & 0xffff) != 0) || mode == 3) && (R32(w, 8) & 0x80000000) &&
        (R32(rect, 8) - R32(rect, 0) != R32(w, 0x60) ||
         R32(rect, 0xc) - R32(rect, 4) != R32(w, 0x64)))
        resized = 1;
    if (!first && !resized && D32(0x10010e68) == 0)
        return rect;
    D32(0x1002503c) = rect;
    h_busy(1);
    D32(0x10010e70)++;
    h_list_walk(GAUGE_LIST, w, gauge_unload_for);
    h_list_walk(PANEL_LIST, w, panel_rebuild);
    h_list_walk(PANEL_LIST, w, panel_reload_gauges);
    CH(0x20, f3)(0x14, 0x70, 0);
    D32(0x10010e70)--;
    return h_busy(0);
}

/* 0x10002410 -- load the panel of aircraft `a`: the 8-bit colours into
 * HG2D's palette, each [WindowNN] of [Window Titles] as a panel window (its
 * layout from a saved situation when its ident has one, else from the
 * file), then every window's gauges.
 *
 * Kept from retail: a window with no layout of its own takes its size
 * relative to the first window's SIZE_MM; a window the table does not
 * name looks for gauges in the section named by its title and, should it
 * find any, loads them into the last panel window seen (`e`, which retail
 * leaves in ebp; before any it is whatever ebp held, here 0). */
static void panels_load(uint32_t a)
{
    char cfg[0x104], prof[0x100], val[0x100], title[0x100], file[0x100], path[0x104];
    char tok[0x100], name[0x100], key[0x28];
    uint32_t i, n, w, e = 0, f, s, layout, bmp = 0, list, nlist, k, sect, p, dst;
    int32_t width, height, pos, visible, ident, x, y, wd;
    double size, size0 = 0, size_y, ratio;

    if (AC(0x30, f4)(a, 5, P(prof), 0x100) == 0)
        return;
    if (AC(0x30, f4)(a, 6, P(cfg), 0x104) == 0)
        return;
    D32(0x10020ee0) = a;

    /* [8 Bit Colors]: up to 32 into the palette's 0x80.. */
    for (i = 0; i < 0x20; i++) {
        sprintf(key, S_COLOUR_FMT, i);
        val[0] = 0;
        if (h_profile_string(S_COLOURS, key, val, 0x100, cfg, 0x24) == 0)
            break;
        p = token(P(val), P(tok), 0x2c);
        D8(0x10025040 + 3 * i) = (uint8_t)atoi(tok);
        p = token(p, P(tok), 0x2c);
        D8(0x10025041 + 3 * i) = (uint8_t)atoi(tok);
        token(p, P(tok), 0x2c);
        D8(0x10025042 + 3 * i) = (uint8_t)atoi(tok);
    }
    n = i;
    dst = R32(HG2D, 0x8c) + (0xa0 - n) * 3;
    for (k = 0; k < n; k++) {
        M8(dst + 3 * k) = D8(0x10025040 + 3 * k);
        M8(dst + 3 * k + 1) = D8(0x10025041 + 3 * k);
        M8(dst + 3 * k + 2) = D8(0x10025042 + 3 * k);
    }
    HG(0x64, f0)();
    HG(0x5c, f0)();
    HG(0x64, f0)();
    remap_reset(1);

    /* the windows */
    i = 0;
    sprintf(key, S_WINDOW_FMT, i);
    title[0] = 0;
    if (h_profile_string(S_TITLES, key, title, 0x100, cfg, 0x24) != 0) {
        for (;;) {
            if (i >= 0x40)
                break;
            file[0] = 0;
            h_profile_string(key, S_FILE, file, 0x100, cfg, 0x24);
            h_path_join(path, 0, prof, file);
            size = 100.0;
            h_profile_num(key, S_SIZE_MM, &size, cfg, 0x24);
            size_y = -1.0;
            h_profile_num(key, S_SIZE_Y_MM, &size_y, cfg, 0x24);
            if (i == 0)
                size0 = size;
            ratio = 1.0;
            h_profile_num(key, S_RATIO, &ratio, cfg, 0x24);
            pos = 7;
            h_profile_int(key, S_POSITION, &pos, cfg, 0x24);
            visible = 1;
            h_profile_int(key, S_VISIBLE, &visible, cfg, 0x24);
            ident = -1;
            h_profile_int(key, S_IDENT, &ident, cfg, 0x24);
            w = panel_window_new(P(title));
            if (w == 0)
                return;
            e = h_list_add(PANEL_LIST);
            D32(WT_ENTRY(i)) = e;
            h_strcpy(DA(WT_ENTRY(i) + 4), key);
            R32(e, 4) = w;
            R32(e, 0) = ident;
            R32(R32(w, 0x1c), 0) = 1;
            R32(R32(w, 0x1c), 8) = 1;
            R32(R32(w, 0x1c), 4) = 0;
            R32(R32(w, 0x1c), 0xc) = 0;
            R32(w, 0x80) = 1;
            f = R32(w, 8) | 0x400;
            R32(w, 8) = f;
            if (G32(0x1224) == 0)
                R32(w, 8) = f | 0x80000;
            layout = 0;
            if (ident != -1)
                layout = h_list_find(LAYOUT_LIST, 0x24, &ident, 4, 0);
            if (layout != 0) {
                /* the situation's layout */
                R32(w, 0x20) = R32(layout, 0x00);
                R32(w, 0x24) = R32(layout, 0x04);
                R32(w, 0x28) = R32(layout, 0x08);
                R32(w, 0x2c) = R32(layout, 0x0c);
                R32(w, 0x50) = R32(layout, 0x10);
                R32(w, 0x54) = R32(layout, 0x14);
                R32(w, 0x58) = R32(layout, 0x18);
                f = R32(w, 8);
                R32(w, 0x5c) = R32(layout, 0x1c);
                f &= 0x3fffffff;
                R32(w, 8) = f;
                R32(w, 8) = (R32(layout, 0x20) & 0x7fffffff) | f;
                visible = R32(layout, 0x20) & 0x80000000;
                R32(e, 0x4c) = h_strdup(path);
                if (visible == 0) {
                    bitmap_size(path, &width, &height);
                } else {
                    bmp = bitmap_load(path);
                    if (bmp != 0) {
                        width = R32(bmp, 4);
                        height = R32(bmp, 8);
                    } else {
                        width = 100;
                        height = 50;
                    }
                }
                h_window_create(w);
                if (visible != 0) {
                    window_show(w);
                } else {
                    window_hide(w);
                    R32(w, 8) |= R32(layout, 0x20) & 1;
                }
            } else {
                /* the file's */
                long double v;

                R32(e, 0x4c) = h_strdup(path);
                if (visible == 0) {
                    bitmap_size(path, &width, &height);
                } else {
                    bmp = bitmap_load(path);
                    if (bmp != 0) {
                        width = R32(bmp, 4);
                        height = R32(bmp, 8);
                    } else {
                        width = 100;
                        height = 50;
                    }
                }
                R32(w, 0x28) = 0x2000;
                x = FTOL((long double)size / size0 * RD(0x1000e018));
                R32(w, 0x28) = x;
                if ((long double)ratio > RD(0x1000e020))
                    R32(w, 0x28) = FTOL(FI(x) * ratio);
                if ((long double)size_y == RD(0x1000e008))
                    v = FIU(height) / FIU(width) * FI(R32(w, 0x28));
                else
                    v = (long double)size_y / size * FI(R32(w, 0x28));
                R32(w, 0x2c) = FTOL(v);
                switch ((uint32_t)pos) {
                case 0:
                    R32(w, 0x20) = 0;
                    R32(w, 0x24) = 0;
                    break;
                case 1:
                    R32(w, 0x24) = 0;
                    R32(w, 0x20) = (int32_t)(0x2000 - R32(w, 0x28)) / 2;
                    break;
                case 2:
                    R32(w, 0x24) = 0;
                    R32(w, 0x20) = 0x2000 - R32(w, 0x28);
                    break;
                case 3:
                    R32(w, 0x20) = 0;
                    R32(w, 0x24) = (int32_t)(0x1800 - R32(w, 0x2c)) / 2;
                    break;
                case 4:
                    R32(w, 0x20) = (int32_t)(0x2000 - R32(w, 0x28)) / 2;
                    R32(w, 0x24) = (int32_t)(0x1800 - R32(w, 0x2c)) / 2;
                    break;
                case 5:
                    R32(w, 0x20) = 0x2000 - R32(w, 0x28);
                    R32(w, 0x24) = (int32_t)(0x1800 - R32(w, 0x2c)) / 2;
                    break;
                case 6:
                    R32(w, 0x20) = 0;
                    R32(w, 0x24) = 0x1800 - R32(w, 0x2c);
                    break;
                case 7:
                    R32(w, 0x20) = (int32_t)(0x2000 - R32(w, 0x28)) / 2;
                    R32(w, 0x24) = 0x1800 - R32(w, 0x2c);
                    break;
                case 8:
                    R32(w, 0x20) = 0x2000 - R32(w, 0x28);
                    R32(w, 0x24) = 0x1800 - R32(w, 0x2c);
                    break;
                }
                h_window_create(w);
                if (visible != 0)
                    window_show(w);
                else
                    window_hide(w);
            }
            R32(w, 0x18) = ADDR(panel_paint);
            R32(w, 0x68) = R32(w, 0x60);
            R32(w, 0x70) = R32(w, 0x60);
            R32(w, 0x38) = R32(w, 0x60);
            R32(w, 0x6c) = R32(w, 0x64);
            R32(w, 0x74) = R32(w, 0x64);
            R32(w, 0x3c) = R32(w, 0x64);
            h_window_update(w);
            if (bmp != 0 && visible != 0) {
                surface_from_bitmap(bmp, 0, R32(w, 0x84), R32(w, 0x88), e + 0xc, 0, 0, 0);
            } else {
                h_surface_new(e + 0xc, width, height, 1);
                s = R32(e, 0xc);
                h_memset(R32(s, 0x18), 0xa, R32(s, 0x14));
            }
            s = R32(e, 0xc);
            R32(e, 8) = s;
            R32(e, 0x18) = FTOL(size);
            R32(e, 0x1c) = FTOL(FIU(height) * size / FIU(width));
            RFL(e, 0x20) = FI(R32(w, 0x84)) / FI(width);
            RFL(e, 0x24) = FI(R32(w, 0x88)) / FI(height);
            RFL(e, 0x28) = FI(R32(w, 0x84)) / FI(R32(e, 0x18));
            RFL(e, 0x2c) = FI(R32(w, 0x88)) / FI(R32(e, 0x1c));
            if (visible != 0)
                R32(w, 0x10) = bounds_build(s);
            if (G32(0x1228) != 0)
                R32(w, 8) |= 0x800;
            h_window_update(w);
            i++;
            sprintf(key, S_WINDOW_FMT, i);
            title[0] = 0;
            if (bmp != 0)
                h_free(&bmp);
            if (h_profile_string(S_TITLES, key, title, 0x100, cfg, 0x24) == 0)
                break;
        }
    }
    D32(0x10010e6c) = i;

    /* every window's gauges */
    WN(0x80, f3)(P(&list), P(&nlist), 0);
    for (k = 0; k < nlist; k++) {
        uint32_t g = 0;

        w = R32(list, 4 * k);
        sprintf(key, S_GAUGE_FMT, g);
        sect = R32(w, 0);
        val[0] = 0;
        for (n = 0; n < D32(0x10010e6c); n++)
            if (R32(D32(WT_ENTRY(n)), 4) == w) {
                e = D32(WT_ENTRY(n));
                sect = DA(WT_ENTRY(n) + 4);
                break;
            }
        if (h_profile_string(sect, key, val, 0x100, cfg, 0x24) == 0)
            continue;
        do {
            p = token(P(val), P(name), 0x2c);
            p = token(p, P(tok), 0x2c);
            x = atoi(tok);
            p = token(p, P(tok), 0x2c);
            y = atoi(tok);
            if (p != 0) {
                token(p, P(tok), 0x2c);
                wd = atoi(tok);
            } else {
                wd = 0;
            }
            gauge_load(e, name, x, y, wd);
            g++;
            sprintf(key, S_GAUGE_FMT, g);
            val[0] = 0;
        } while (h_profile_string(sect, key, val, 0x100, cfg, 0x24) != 0);
    }
    h_free(&list);
}

/* 0x10002fb0 -- the same panel file again: every panel window's flags and
 * layout reset, from a saved situation's layout where its ident has one,
 * else its VISIBLE */
static void panels_refresh(uint32_t a)
{
    char cfg[0x104], key[0x28];
    uint32_t list, n, i, w, layout;
    int32_t ident, visible;

    WN(0x80, f3)(P(&list), P(&n), 2);
    AC(0x30, f4)(a, 6, P(cfg), 0x104);
    for (i = 0; i < n; i++) {
        w = R32(list, 4 * i);
        ident = window_ident(w);
        layout = 0;
        if (ident != -1)
            layout = h_list_find(LAYOUT_LIST, 0x24, &ident, 4, 0);
        if (layout != 0) {
            R32(R32(list, 4 * i), 8) = 0x10000400;
            if (G32(0x1224) == 0)
                R32(R32(list, 4 * i), 8) |= 0x80000;
            if (G32(0x1228) != 0)
                R32(R32(list, 4 * i), 8) |= 0x800;
            if (M8(R32(HG2D, 0x90)) != 0xa)
                R32(R32(list, 4 * i), 8) |= 0x200;
            R32(R32(list, 4 * i), 0x20) = R32(layout, 0x00);
            R32(R32(list, 4 * i), 0x24) = R32(layout, 0x04);
            R32(R32(list, 4 * i), 0x28) = R32(layout, 0x08);
            R32(R32(list, 4 * i), 0x2c) = R32(layout, 0x0c);
            R32(R32(list, 4 * i), 0x50) = R32(layout, 0x10);
            R32(R32(list, 4 * i), 0x54) = R32(layout, 0x14);
            R32(R32(list, 4 * i), 0x58) = R32(layout, 0x18);
            R32(R32(list, 4 * i), 0x5c) = R32(layout, 0x1c);
            R32(R32(list, 4 * i), 8) &= 0x3fffffff;
            R32(R32(list, 4 * i), 8) |= R32(layout, 0x20) & 0x7fffffff;
            visible = R32(layout, 0x20) & 0x80000000;
            if (visible != 0) {
                window_show(R32(list, 4 * i));
            } else {
                window_hide(R32(list, 4 * i));
                R32(R32(list, 4 * i), 8) |= R32(layout, 0x20) & 1;
            }
            continue;
        }
        sprintf(key, S_WINDOW_FMT, R32(R32(list, 4 * i), 0xc) - 0x201);
        visible = 1;
        h_profile_int(key, S_VISIBLE, &visible, cfg, 0x24);
        R32(R32(list, 4 * i), 8) = 0x10000400;
        if (G32(0x1224) == 0)
            R32(R32(list, 4 * i), 8) |= 0x80000;
        if (G32(0x1228) != 0)
            R32(R32(list, 4 * i), 8) |= 0x800;
        if (M8(R32(HG2D, 0x90)) != 0xa)
            R32(R32(list, 4 * i), 8) |= 0x200;
        if (visible != 0)
            window_show(R32(list, 4 * i));
        else
            window_hide(R32(list, 4 * i));
    }
    h_free(&list);
}

/* 0x100032a0 -- a new panel window (WINDOW type 2) */
static uint32_t panel_window_new(uint32_t title)
{
    uint32_t w = 0;

    if (WN(0x78, f3)(P(&w), title, 2) == 0) {
        R32(w, 8) = 0x10000000;
        R32(w, 0x78) = 0x4000;
        R32(w, 0x7c) = 0;
    }
    return w;
}

/* 0x100032f0 -- a surface's bounds: "FS6BOUNDS", then for each row each run
 * of non-black pixels as { 1, x0, y0, x1, y1 }.  Built in a block of
 * 0x14 + 200 bytes a row (ten runs a row; retail does not check, nor does
 * this) and copied into one of the size used. */
static uint32_t bounds_build(uint32_t surf)
{
    uint32_t blk, copy, out, row, n, x, start, type, size;
    uint32_t px1, px3;

    if (h_alloc(1, R32(surf, 8) * 200 + 0x14, &blk, 0) != 0)
        return 0;
    h_memmove(blk, S_BOUNDS, 8);
    R32(blk, 8) = 1;
    R32(blk, 0xc) = 0;
    out = blk + 0x10;
    for (row = 0; (int32_t)row < (int32_t)R32(surf, 8); row++) {
        px1 = px3 = R32(R32(surf, 0x10), 4 * row);
        n = R32(surf, 4);
        x = 0;
        if (n == 0)
            continue;
        do {
            type = R32(surf, 0);
            if (type == 1) {
                while (n != 0 && M8(px1) == 0) {
                    n--;
                    x++;
                    px1++;
                }
            } else {
                while (n != 0 && M8(px3 + 2) == 0 && M8(px3 + 1) == 0 && M8(px3) == 0) {
                    n--;
                    x++;
                    px3 += 3;
                }
            }
            start = x;
            if (type == 1) {
                while (n != 0 && M8(px1) != 0) {
                    n--;
                    x++;
                    px1++;
                }
            } else {
                while (n != 0 && (M8(px3 + 2) != 0 || M8(px3 + 1) != 0 || M8(px3) != 0)) {
                    n--;
                    x++;
                    px3 += 3;
                }
            }
            if (start != x) {
                R32(out, 0) = 1;
                R32(out, 4) = start;
                R32(out, 8) = row;
                R32(out, 0xc) = x;
                R32(out, 0x10) = row + 1;
                out += 0x14;
                R32(blk, 0xc)++;
            }
        } while (n != 0);
    }
    size = (R32(blk, 0xc) + 1) * 0x14;
    if (h_alloc(1, size, &copy, 0) != 0)
        return blk;
    h_memmove(copy, blk, size);
    h_free(&blk);
    return copy;
}

/* 0x100034c0 -- load gauge `name` (Gauges\name.GAU) into panel window `e`,
 * at x, y in the window's millimetres, `w` its width if positive.  A gauge
 * of a window that is not shown is initialised and unloaded again. */
static void gauge_load(uint32_t e, const char *name, int32_t x, int32_t y, int32_t w)
{
    uint32_t slot, hdr, tag;

    h_path_join(DA(0x10025460), 0, DA(0x10025340), name);
    if (h_exists(DA(0x10025460), 0x10) != 0)
        return;
    slot = h_list_add(GAUGE_LIST);
    if (h_module_load(slot, DA(0x10025460), 0x10) != 0) {
        h_list_remove(GAUGE_LIST, slot);
        return;
    }
    hdr = R32(M32(slot), 0x18);
    R32(hdr, 0x40) = e;
    R32(hdr, 0x3c) = R32(e, 4);
    R32(hdr, 0x44) = FTOL(FI(x) * RFL(e, 0x28));
    R32(hdr, 0x48) = FTOL(FI(y) * RFL(e, 0x2c));
    if (w > 0)
        R32(hdr, 0x2c) = w;
    h_module_enter(M32(slot), &tag);
    if ((R32(R32(e, 4), 8) & 0x80000000) == 0) {
        CALL(hdr, 0xc, f0)();
        h_module_release(slot);
        h_module_leave(tag);
        h_list_remove(GAUGE_LIST, slot);
        return;
    }
    CALL(hdr, 0x10, f1)(tag);
    h_module_leave(tag);
}

/* 0x100035f0 -- a panel window walk: free it.  Retail destroys the window
 * (host +0x68, which frees its record) and then frees the bounds block
 * whose pointer it reads from the freed record, as here. */
static uint32_t FS98_STDCALL panel_free(uint32_t e, uint32_t c)
{
    uint32_t w = R32(e, 4);

    (void)c;
    if (w != 0) {
        h_window_destroy(w);
        h_free(e + 0x4c);
        h_free(w + 0x10);
        WN(0x7c, f1)(P(&w));
    }
    surfaces_free(e + 8);
    panel_children_reset(e, 0);
    h_list_remove(PANEL_LIST, e);
    return 0;
}

/* 0x10003670 -- a gauge walk: unload it */
static uint32_t FS98_STDCALL gauge_free(uint32_t e, uint32_t c)
{
    (void)c;
    CALL(R32(M32(e), 0x18), 0x24, f0)();
    h_module_release(e);
    h_list_remove(GAUGE_LIST, e);
    return 0;
}

/* 0x100036b0 -- the two 0x8000-byte colour remaps: all 0xff, or only their
 * entries that are panel colours (0x80..0x9f) */
static void remap_reset(uint32_t all)
{
    int32_t i;

    if (all != 0) {
        h_memset(DA(0x10010ee0), 0xff, 0x8000);
        h_memset(DA(0x10018ee0), 0xff, 0x8000);
        D32(0x1000f5b8) = 0;
        return;
    }
    for (i = 0; i > -0x8000; i--) {
        uint8_t c = D8(0x10018edf + i);

        if (c >= 0x80 && c <= 0x9f)
            D8(0x10018edf + i) = 0xff;
        c = D8(0x10020edf + i);
        if (c >= 0x80 && c <= 0x9f)
            D8(0x10020edf + i) = 0xff;
    }
}

/* 0x10003740 -- a 24-bit DIB's bits (`info` its header) scaled into a
 * surface by nearest pixels, the rows turned the right way up */
static void scale_24(uint32_t info, uint32_t bits, uint32_t dst)
{
    int32_t dw = R32(dst, 4), dh = R32(dst, 8);
    int32_t sw = R32(info, 4), sh = R32(info, 8);
    int32_t xs = sw / dw, ys = sh / dh, yr = sh % dh, xr = sw % dw;
    uint32_t stride = ((sw + 1) * 3) & ~3u, step, src, row = bits, d;
    int32_t yacc = 0, xacc, k, r = dh - 1;

    if (dh == 0)
        return;
    step = stride * ys;
    for (;;) {
        d = R32(R32(dst, 0x10), 4 * r);
        xacc = 0;
        src = row;
        for (k = dw; k != 0; k--) {
            M8(d + 2) = M8(src + 2);
            M8(d) = M8(src);
            M8(d + 1) = M8(src + 1);
            xacc += xr;
            if (xacc >= dw) {
                xacc -= dw;
                src += 3;
            }
            d += 3;
            src += xs * 3;
        }
        yacc += yr;
        if (yacc >= dh) {
            yacc -= dh;
            row += stride;
        }
        row += step;
        if (r-- == 0)
            break;
    }
}

/* 0x10003840 -- the same from an 8-bit DIB through its palette */
static void scale_8(uint32_t info, uint32_t bits, uint32_t dst)
{
    uint32_t pal = info + R32(info, 0);
    int32_t sw = R32(info, 4), sh = R32(info, 8);
    int32_t dw = R32(dst, 4), dh = R32(dst, 8);
    int32_t xs = sw / dw, ys = sh / dh, yr = sh % dh, xr = sw % dw;
    uint32_t stride = (sw + 3) & ~3u, step, src, row = bits, d;
    int32_t yacc = 0, xacc, k, r = dh - 1;

    if (dh == 0)
        return;
    step = stride * ys;
    for (;;) {
        d = R32(R32(dst, 0x10), 4 * r);
        xacc = 0;
        src = row;
        for (k = dw; k != 0; k--) {
            M8(d + 2) = M8(pal + 4 * M8(src) + 2);
            M8(d) = M8(pal + 4 * M8(src));
            M8(d + 1) = M8(pal + 4 * M8(src) + 1);
            xacc += xr;
            if (xacc >= dw) {
                xacc -= dw;
                src++;
            }
            d += 3;
            src += xs;
        }
        yacc += yr;
        if (yacc >= dh) {
            yacc -= dh;
            row += stride;
        }
        row += step;
        if (r-- == 0)
            break;
    }
}

/* 0x10003960 -- an 8-bit DIB scaled into a 24-bit surface with filtering
 * (IMAGE_QUALITY).  Kept from retail: the weights are the output column
 * and row counters, not the source position's fractions; the plain average
 * at (0x8000, 0x8000) counts the right-hand pixel twice and the pixel
 * itself not at all for green and blue; and the rows are stored one late
 * -- the first two go to the last row and row 0 is never written.  Black
 * and (1,1,1) pixels, the first row and the first column are copied as
 * they are, and a result of black or (1,1,1) becomes (0x10,0x10,0x10). */
static void smooth_8(uint32_t info, uint32_t bits, uint32_t dst)
{
    uint32_t pal = info + R32(info, 0), rows = R32(dst, 0x10);
    int32_t sw = R32(info, 4), sh = R32(info, 8);
    int32_t dw = R16(dst, 4), dh = R16(dst, 8);
    uint32_t stride = (sw + 3) & ~3u, out = R32(rows, 4 * (dh - 1));
    int32_t xstep, ystep, x0, ypos, j, i;

    if (sw == 2 * dw && sh == 2 * dh) {
        xstep = 0x20000;
        ystep = 0x20000;
        x0 = 0x8000;
    } else {
        xstep = ((sw << 16) - 0x10000) / (dw - 1);
        ystep = ((sh << 16) - 0x10000) / (dh - 1);
        x0 = 0;
    }
    ypos = x0;
    for (j = 0; j < dh; j++) {
        uint32_t srow = ((uint32_t)ypos >> 16) * stride + bits, xpos = x0;

        for (i = 0; i < dw; i++, xpos += xstep) {
            uint32_t p = srow + (xpos >> 16);
            uint32_t a = M8(p), b = M8(p + 1), c = M8(p + stride), d = M8(p + stride + 1);
            uint32_t R = M8(pal + 4 * a + 2), G = M8(pal + 4 * a + 1), B = M8(pal + 4 * a);
            uint32_t ui = i, uj = j, nx = 0x10000 - ui, ny = 0x10000 - uj;

            if ((R == 0 && G == 0 && B == 0) || (R == 1 && G == 1 && B == 1) || i == 0 || j == 0)
                goto store;
            if (i == 0x8000 && j == 0x8000) {
                R = (M8(pal + 4 * c + 2) + M8(pal + 4 * b + 2) + M8(pal + 4 * a + 2) +
                     M8(pal + 4 * d + 2)) >> 2;
                G = (M8(pal + 4 * c + 1) + M8(pal + 4 * d + 1) + 2 * M8(pal + 4 * b + 1)) >> 2;
                B = (M8(pal + 4 * d) + 2 * M8(pal + 4 * b) + M8(pal + 4 * c)) >> 2;
            } else {
                uint32_t Ra = R, Ga = G, Ba = B;

                R = ((M8(pal + 4 * c + 2) * nx) >> 16) * uj;
                R += ((M8(pal + 4 * b + 2) * ui) >> 16) * ny;
                R += ((M8(pal + 4 * d + 2) * ui) >> 16) * uj;
                R += ((nx * Ra) >> 16) * ny;
                R >>= 16;
                G = (((M8(pal + 4 * b + 1) * ui) >> 16) + ((nx * Ga) >> 16)) * ny;
                G += (((M8(pal + 4 * d + 1) * ui) >> 16) + ((M8(pal + 4 * c + 1) * nx) >> 16)) * uj;
                G >>= 16;
                B = ((nx * M8(pal + 4 * c)) >> 16) * uj;
                B += ((ui * M8(pal + 4 * b)) >> 16) * ny;
                B += ((nx * Ba) >> 16) * ny;
                B += ((ui * M8(pal + 4 * d)) >> 16) * uj;
                B >>= 16;
            }
            if ((B | G | R) == 0 || (R == 1 && G == 1 && B == 1)) {
                B = 0x10;
                R = 0x10;
                G = 0x10;
            }
        store:
            M8(out) = (uint8_t)B;
            M8(out + 1) = (uint8_t)G;
            M8(out + 2) = (uint8_t)R;
            out += 3;
        }
        out = R32(rows, 4 * (dh - j - 1));
        ypos += ystep;
    }
}

/* 0x10003d40 -- the same from a 24-bit DIB.  Its pixels are read a dword
 * at a time; any channel at 1 is copied as it is, as is the last source
 * row; the average at (0x8000, 0x8000) is of all four; a black or
 * (1,1,1) result becomes (8,8,8).  The same counters for weights and the
 * same late rows as smooth_8. */
static void smooth_24(uint32_t info, uint32_t bits, uint32_t dst)
{
    uint32_t rows = R32(dst, 0x10);
    int32_t sw = R32(info, 4), sh = R32(info, 8);
    int32_t dw = R16(dst, 4), dh = R16(dst, 8);
    uint32_t stride = ((sw + 1) * 3) & ~3u, out = R32(rows, 4 * (dh - 1));
    int32_t xstep, ystep, x0, ypos, j, i;

    if (sw == 2 * dw && sh == 2 * dh) {
        xstep = 0x20000;
        ystep = 0x20000;
        x0 = 0x8000;
    } else {
        xstep = ((sw << 16) - 0x10000) / (dw - 1);
        ystep = ((sh << 16) - 0x10000) / (dh - 1);
        x0 = 0;
    }
    ypos = x0;
    for (j = 0; j < dh; j++) {
        uint32_t srow = ((uint32_t)ypos >> 16) * stride + bits, xpos = x0;

        for (i = 0; i < dw; i++, xpos += xstep) {
            uint32_t x = xpos >> 16, p = srow + 3 * x, v = M32(p);
            uint32_t R = (v >> 16) & 0xff, G = (v >> 8) & 0xff, B = v & 0xff;
            uint32_t q, below, right, diag, ui = i, uj = j, nx = 0x10000 - ui, ny = 0x10000 - uj;

            if ((B | G | R) == 0 || R == 1 || G == 1 || B == 1 || i == 0 || j == 0 ||
                j >= dh - 1)
                goto store;
            q = srow + 3 * (x + 1);
            below = M32(p + stride);
            right = M32(q);
            diag = M32(q + stride);
            if (i == 0x8000 && j == 0x8000) {
                uint32_t Ra = R, Ga = G, Ba = B;

                R = (((below >> 16) & 0xff) + Ra + ((right >> 16) & 0xff) + ((diag >> 16) & 0xff)) >> 2;
                G = (((diag >> 8) & 0xff) + Ga + ((below >> 8) & 0xff) + ((right >> 8) & 0xff)) >> 2;
                B = ((diag & 0xff) + Ba + (below & 0xff) + (right & 0xff)) >> 2;
            } else {
                uint32_t Ra = R, Ga = G, Ba = B;

                R = ((((diag >> 16) & 0xff) * ui) >> 16) * uj;
                R += ((nx * Ra) >> 16) * ny;
                R += ((((below >> 16) & 0xff) * nx) >> 16) * uj;
                R += ((((right >> 16) & 0xff) * ui) >> 16) * ny;
                R >>= 16;
                G = (((((right >> 8) & 0xff) * ui) >> 16) + ((nx * Ga) >> 16)) * ny;
                G += (((((below >> 8) & 0xff) * nx) >> 16) + ((((diag >> 8) & 0xff) * ui) >> 16)) * uj;
                G >>= 16;
                B = ((((right & 0xff) * ui) >> 16) + ((nx * Ba) >> 16)) * ny;
                B += ((((below & 0xff) * nx) >> 16) + (((diag & 0xff) * ui) >> 16)) * uj;
                B >>= 16;
            }
            if ((B | G | R) == 0 || (R == 1 && G == 1 && B == 1)) {
                R = 8;
                G = 8;
                B = 8;
            }
        store:
            M8(out) = (uint8_t)B;
            M8(out + 1) = (uint8_t)G;
            M8(out + 2) = (uint8_t)R;
            out += 3;
        }
        out = R32(rows, 4 * (dh - j - 1));
        ypos += ystep;
    }
}

/* 0x10004120 -- a 24-bit DIB shrunk into a surface by area averaging, in
 * 16.16: each output pixel the weighted sum of the source pixels its box
 * covers times the inverse of the box's area.  A box whose plain average
 * is (1,1,1) or black stays so; a weighted result of (1,1,1) or black
 * becomes (8,8,8).  Retail first copies the host's palette into a local
 * it does not use; the call is kept. */
static void shrink_24(uint32_t info, uint32_t bits, uint32_t dst)
{
    uint8_t palette[0x36c];
    uint32_t dw = R16(dst, 4), dh = R16(dst, 8), stride;
    int32_t xs, ys, inv;
    uint32_t r, c;

    h_palette_copy(palette);
    xs = ((int32_t)R32(info, 4) << 16) / (int32_t)dw;
    ys = ((int32_t)R32(info, 8) << 16) / (int32_t)dh;
    inv = MUL16(xs, ys);
    inv = pn_idiv64(1, 0, inv);
    stride = ((R32(info, 4) + 1) * 3) & ~3u;
    for (r = 0; (uint16_t)r < dh; r++) {
        uint32_t y0 = (uint16_t)r * (uint32_t)ys, rowoff = (y0 >> 16) * stride;
        uint32_t outrow = R32(R32(dst, 0x10), 4 * (dh - 1 - (uint16_t)r));

        for (c = 0; (uint16_t)c < dw; c++) {
            uint32_t x0 = (uint16_t)c * (uint32_t)xs, src = bits + (x0 >> 16) * 3 + rowoff;
            uint32_t out = outrow + (uint16_t)c * 3, y = y0, yrem = ys, n = 0;
            uint32_t sb = 0, sg = 0, sr = 0, pb = 0, pg = 0, pr = 0;
            int32_t R, G, B;

            if ((uint32_t)ys > 0x147) {
                do {
                    uint32_t ny = (y & 0xffff0000) + 0x10000, hy, x = x0, xrem = xs, p = src;

                    if (ny > yrem + y)
                        ny = yrem + y;
                    hy = ny - y;
                    if ((uint32_t)xs > 0x147) {
                        do {
                            uint32_t nx = (x & 0xffff0000) + 0x10000, wx, w;

                            if (nx > xrem + x)
                                nx = xrem + x;
                            wx = nx - x;
                            n++;
                            pg += M8(p + 1);
                            pr += M8(p + 2);
                            pb += M8(p);
                            w = MUL16(wx, hy);
                            sb += M8(p) * w;
                            sg += M8(p + 1) * w;
                            sr += M8(p + 2) * w;
                            xrem -= wx;
                            p += 3;
                            x = nx;
                        } while (xrem > 0x147);
                    }
                    yrem -= hy;
                    src += stride;
                    y = ny;
                } while (yrem > 0x147);
            }
            R = (int32_t)pr / (int32_t)n;
            G = (int32_t)pg / (int32_t)n;
            B = (int32_t)pb / (int32_t)n;
            if (R == 1 && G == 1 && B == 1) {
                M8(out + 2) = 1;
                M8(out) = 1;
                M8(out + 1) = 1;
                continue;
            }
            if ((B | G | R) == 0) {
                M8(out + 2) = 0;
                M8(out) = 0;
                M8(out + 1) = 0;
                continue;
            }
            M8(out) = (uint8_t)((int32_t)MUL16(sb, inv) >> 16);
            M8(out + 1) = (uint8_t)((int32_t)MUL16(sg, inv) >> 16);
            M8(out + 2) = (uint8_t)((int32_t)MUL16(sr, inv) >> 16);
            if ((M8(out + 2) == 1 && M8(out + 1) == 1 && M8(out) == 1) ||
                (M8(out + 2) == 0 && M8(out + 1) == 0 && M8(out) == 0)) {
                M8(out + 2) = 8;
                M8(out) = 8;
                M8(out + 1) = 8;
            }
        }
    }
}

/* 0x10004450 -- the same from an 8-bit DIB through its palette; a
 * weighted result of (1,1,1) or black becomes (0x10,0x10,0x10) */
static void shrink_8(uint32_t info, uint32_t bits, uint32_t dst)
{
    uint32_t pal = R32(info, 0) + info;
    uint32_t dw = R16(dst, 4), dh = R16(dst, 8), stride;
    int32_t xs, ys, inv;
    uint32_t r, c;

    xs = ((int32_t)R32(info, 4) << 16) / (int32_t)dw;
    ys = ((int32_t)R32(info, 8) << 16) / (int32_t)dh;
    inv = MUL16(xs, ys);
    inv = pn_idiv64(1, 0, inv);
    stride = (R32(info, 4) + 3) & ~3u;
    for (r = 0; (uint16_t)r < dh; r++) {
        uint32_t y0 = (uint16_t)r * (uint32_t)ys, rowoff = (y0 >> 16) * stride;
        uint32_t outrow = R32(R32(dst, 0x10), 4 * (dh - 1 - (uint16_t)r));

        for (c = 0; (uint16_t)c < dw; c++) {
            uint32_t x0 = (uint16_t)c * (uint32_t)xs, src = (x0 >> 16) + bits + rowoff;
            uint32_t out = outrow + (uint16_t)c * 3, y = y0, yrem = ys, n = 0;
            uint32_t sb = 0, sg = 0, sr = 0, pb = 0, pg = 0, pr = 0;
            int32_t R, G, B;

            if ((uint32_t)ys > 0x147) {
                do {
                    uint32_t ny = (y & 0xffff0000) + 0x10000, hy, x = x0, xrem = xs, p = src;

                    if (ny > yrem + y)
                        ny = yrem + y;
                    hy = ny - y;
                    if ((uint32_t)xs > 0x147) {
                        do {
                            uint32_t nx = (x & 0xffff0000) + 0x10000, wx, w, ent;

                            if (nx > xrem + x)
                                nx = xrem + x;
                            wx = nx - x;
                            n++;
                            ent = pal + 4 * M8(p);
                            pr += M8(ent + 2);
                            pg += M8(ent + 1);
                            pb += M8(ent);
                            w = MUL16(wx, hy);
                            sb += M8(ent) * w;
                            sg += M8(ent + 1) * w;
                            sr += M8(ent + 2) * w;
                            xrem -= wx;
                            p++;
                            x = nx;
                        } while (xrem > 0x147);
                    }
                    yrem -= hy;
                    src += stride;
                    y = ny;
                } while (yrem > 0x147);
            }
            R = (int32_t)pr / (int32_t)n;
            G = (int32_t)pg / (int32_t)n;
            B = (int32_t)pb / (int32_t)n;
            if (R == 1 && G == 1 && B == 1) {
                M8(out + 2) = 1;
                M8(out) = 1;
                M8(out + 1) = 1;
                continue;
            }
            if ((B | G | R) == 0) {
                M8(out + 2) = 0;
                M8(out) = 0;
                M8(out + 1) = 0;
                continue;
            }
            M8(out) = (uint8_t)((int32_t)MUL16(sb, inv) >> 16);
            M8(out + 1) = (uint8_t)((int32_t)MUL16(sg, inv) >> 16);
            M8(out + 2) = (uint8_t)((int32_t)MUL16(sr, inv) >> 16);
            if ((M8(out + 2) == 1 && M8(out + 1) == 1 && M8(out) == 1) ||
                (M8(out + 2) == 0 && M8(out + 1) == 0 && M8(out) == 0)) {
                M8(out + 2) = 0x10;
                M8(out) = 0x10;
                M8(out + 1) = 0x10;
            }
        }
    }
}

/* the tail 0x10004770 and 0x10004f50 share: black and (1,1,1) are
 * themselves; any other colour is its 15-bit cache entry, worked out on a
 * miss as the nearest (by the squares at 0x1000f3b6) in the palette's
 * ranges, as a colour word's low byte unless `quick` */
static uint8_t colour_8(uint8_t R, uint8_t G, uint8_t B, uint32_t cache, uint32_t ranges,
                        uint32_t nranges, uint32_t pal, uint32_t quick)
{
    uint32_t slot;

    if (R == 0 && G == 0 && B == 0)
        return 0;
    if (R == 1 && G == 1 && B == 1)
        return 1;
    slot = cache + ((((uint32_t)(R & 0xf8) << 5 | (G & 0xf8)) << 2 | (B >> 3)) & 0xffff);
    if (M8(slot) == 0xff) {
        int32_t best = 0x8000001;
        uint32_t bi = 2, k, rg = ranges, left;

        for (left = nranges; (int32_t)left > 0; left--, rg += 8) {
            for (k = R32(rg, 0); (int32_t)k <= (int32_t)R32(rg, 4); k++) {
                uint32_t pe = pal + 3 * k;
                int32_t d = R16(DA(0x1000f3b6) + 2 * (int32_t)(G - M8(pe + 1)), 0) +
                            R16(DA(0x1000f3b6) + 2 * (int32_t)(B - M8(pe + 2)), 0) +
                            R16(DA(0x1000f3b6) + 2 * (int32_t)(R - M8(pe)), 0);

                if (d < best) {
                    best = d;
                    bi = k;
                }
            }
        }
        M8(slot) = quick != 0 ? (uint8_t)bi : M8(R32(HG2D, 0x94) + 2 * bi);
    }
    return M8(slot);
}

/* 0x10004770 -- an 8-bit DIB shrunk into an 8-bit surface: each box's
 * colour averaged as shrink_8's, then matched to the nearest entry of the
 * palette's ranges (0x1000f5d8, two of HG2D's palette; with flags bit 2
 * 0x1000f5e8, four of the host's) through a 15-bit cache (0x10010ee0, or
 * 0x10018ee0).  A box mostly black (0xff in `black`) is 0, mostly (1,1,1)
 * is 1; where a box is part black and `back` is given, the background's
 * pixel at `at` shows through in proportion.  Kept from retail: a box with
 * whole pixels in its middle counts its first pixel's (1,1,1) weight twice. */
static void shrink_8_to_8(uint32_t info, uint32_t bits, uint32_t dst, uint32_t back,
                          uint32_t at, uint32_t flags)
{
    uint8_t hostpal[0x300], black[0x100], grey[0x100];
    uint32_t spal, cache, ranges, pal, nranges, quick = flags & 4;
    uint32_t dw, dh, stride, r, c, i;
    int32_t xs, ys, inv;

    if (D32(0x1000f5b8) != 0)
        remap_reset(1);
    if (quick != 0) {
        cache = DA(0x10018ee0);
        nranges = 4;
        ranges = DA(0x1000f5e8);
        h_palette_copy(hostpal);
        pal = P(hostpal);
    } else {
        cache = DA(0x10010ee0);
        nranges = 2;
        ranges = DA(0x1000f5d8);
        pal = R32(HG2D, 0x8c);
    }
    spal = info + R32(info, 0);
    memset(black, 0, sizeof black);
    memset(grey, 0, sizeof grey);
    for (i = 0; i < 0x100; i++) {
        if (M8(spal + 4 * i + 2) == 0 && M8(spal + 4 * i + 1) == 0 && M8(spal + 4 * i) == 0)
            black[i] = 0xff;
        else if (M8(spal + 4 * i + 2) == 1 && M8(spal + 4 * i + 1) == 1 && M8(spal + 4 * i) == 1)
            grey[i] = 0xff;
    }
    dw = R16(dst, 4);
    dh = R16(dst, 8);
    xs = ((int32_t)R32(info, 4) << 16) / (int32_t)dw;
    ys = ((int32_t)R32(info, 8) << 16) / (int32_t)dh;
    if (xs == 0 || ys == 0)
        return;
    inv = pn_idiv64(1, 0, MUL16(xs, ys));
    stride = (R32(info, 4) + 3) & ~3u;
    for (r = 0; (uint16_t)r < dh; r++) {
        uint32_t y0 = (uint16_t)r * (uint32_t)ys, rowoff = (y0 >> 16) * stride;
        uint32_t out = R32(R32(dst, 0x10), 4 * (dh - (uint16_t)r - 1));

        for (c = 0; (uint16_t)c < dw; c++, out++) {
            uint32_t x0 = (uint16_t)c * (uint32_t)xs, src = (x0 >> 16) + bits + rowoff;
            uint32_t sb = 0, sg = 0, sr = 0, sbk = 0, n = 0, nbk = 0, ngr = 0;
            uint32_t y = y0, yrem = ys;
            uint8_t B, G, R;

            if ((uint32_t)ys > 0x147) {
                uint32_t first = (x0 & 0xffff0000) - x0 + 0x10000, half = first >> 1;
                uint32_t edge = (x0 & 0xffff0000) + 0x10000, xend = xs + x0, mid = src + 1;

                do {
                    uint32_t ny = (y & 0xffff0000) + 0x10000, hy, w, used, rem, p, idx;
                    uint32_t mb = 0, mg = 0, mr = 0, mbk = 0, mgr;

                    if (ny > yrem + y)
                        ny = yrem + y;
                    hy = ny - y;
                    used = first;
                    w = (hy * half) >> 15;
                    if (edge > xend) {
                        used = xend - x0;
                        w = ((used >> 1) * hy) >> 15;
                    }
                    /* the first pixel, partly */
                    idx = M8(src);
                    sb += w * M8(spal + 4 * idx);
                    sg += w * M8(spal + 4 * idx + 1);
                    sr += w * M8(spal + 4 * idx + 2);
                    mgr = grey[idx];
                    sbk += w * black[idx];
                    n++;
                    nbk += black[idx];
                    ngr += grey[idx];
                    /* the whole ones */
                    rem = xs - used;
                    p = mid;
                    if (rem > 0x10000) {
                        uint32_t k = (rem - 1) >> 16;

                        do {
                            idx = M8(p);
                            rem -= 0x10000;
                            mb += M8(spal + 4 * idx);
                            mg += M8(spal + 4 * idx + 1);
                            mr += M8(spal + 4 * idx + 2);
                            mbk += black[idx];
                            mgr += grey[idx];
                            p++;
                        } while (--k != 0);
                    }
                    if (p != mid) {
                        n += p - mid;
                        sb += mb * hy;
                        sg += mg * hy;
                        sr += mr * hy;
                        sbk += mbk * hy;
                        nbk += mbk;
                        ngr += mgr;
                    }
                    /* the last, partly */
                    if (rem > 0x147) {
                        idx = M8(p);
                        w = ((rem >> 1) * hy) >> 15;
                        sb += w * M8(spal + 4 * idx);
                        sg += w * M8(spal + 4 * idx + 1);
                        sr += w * M8(spal + 4 * idx + 2);
                        sbk += w * black[idx];
                        n++;
                        nbk += black[idx];
                        ngr += grey[idx];
                    }
                    yrem -= hy;
                    src += stride;
                    mid += stride;
                    y = ny;
                } while (yrem > 0x147);
            }
            if ((int32_t)(nbk / n) > 0xe8) {
                B = G = R = 0;
            } else if ((int32_t)(ngr / n) > 0xe8) {
                B = G = R = 1;
            } else {
                if (back != 0 && sbk != 0) {
                    int32_t by = R32(at, 4) + dh - (uint16_t)r - 1;
                    int32_t bx = R32(at, 0) + (uint16_t)c;

                    if (by > 0 && by < (int32_t)R32(back, 8) && bx > 0 &&
                        bx < (int32_t)R32(back, 4)) {
                        uint32_t e = R32(HG2D, 0x98) +
                                     3 * M8(R32(R32(back, 0x10), 4 * by) + bx);
                        uint32_t wb = sbk >> 8;

                        sb += M8(e + 2) * wb;
                        sg += M8(e + 1) * wb;
                        sr += M8(e) * wb;
                    }
                }
                R = (uint8_t)(((sr >> 16) * (uint32_t)inv) >> 16);
                B = (uint8_t)(((sb >> 16) * (uint32_t)inv) >> 16);
                G = (uint8_t)(((sg >> 16) * (uint32_t)inv) >> 16);
                if ((R == 1 && G == 1 && B == 1) || (R == 0 && G == 0 && B == 0))
                    R = G = B = 0x10;
            }
            M8(out) = colour_8(R, G, B, cache, ranges, nranges, pal, quick);
        }
    }
}

/* 0x10004f50 -- the same without a background, mostly meaning more than
 * half (0x80): whole pixels in a box's middle are its width's whole part,
 * the last its fraction.  The first pixel's (1,1,1) count is doubled as in
 * shrink_8_to_8. */
static void shrink_8_to_8_plain(uint32_t info, uint32_t bits, uint32_t dst, uint32_t flags)
{
    uint8_t hostpal[0x300], black[0x100], grey[0x100];
    uint32_t spal, cache, ranges, pal, nranges, quick = flags & 4;
    uint32_t dw, dh, stride, r, c, i;
    int32_t xs, ys, inv;

    if (D32(0x1000f5b8) != 0)
        remap_reset(1);
    if (quick != 0) {
        cache = DA(0x10018ee0);
        nranges = 4;
        ranges = DA(0x1000f5e8);
        h_palette_copy(hostpal);
        pal = P(hostpal);
    } else {
        cache = DA(0x10010ee0);
        nranges = 2;
        ranges = DA(0x1000f5d8);
        pal = R32(HG2D, 0x8c);
    }
    spal = info + R32(info, 0);
    memset(black, 0, sizeof black);
    memset(grey, 0, sizeof grey);
    for (i = 0; i < 0x100; i++) {
        if (M8(spal + 4 * i + 2) == 0 && M8(spal + 4 * i + 1) == 0 && M8(spal + 4 * i) == 0)
            black[i] = 0xff;
        else if (M8(spal + 4 * i + 2) == 1 && M8(spal + 4 * i + 1) == 1 && M8(spal + 4 * i) == 1)
            grey[i] = 0xff;
    }
    dw = R16(dst, 4);
    dh = R16(dst, 8);
    xs = ((int32_t)R32(info, 4) << 16) / (int32_t)dw;
    ys = ((int32_t)R32(info, 8) << 16) / (int32_t)dh;
    if (xs == 0 || ys == 0)
        return;
    inv = pn_idiv64(1, 0, MUL16(xs, ys));
    stride = (R32(info, 4) + 3) & ~3u;
    for (r = 0; (uint16_t)r < dh; r++) {
        uint32_t y0 = (uint16_t)r * (uint32_t)ys, rowoff = (y0 >> 16) * stride;
        uint32_t out = R32(R32(dst, 0x10), 4 * (dh - (uint16_t)r - 1));

        for (c = 0; (uint16_t)c < dw; c++, out++) {
            uint32_t x0 = (uint16_t)c * (uint32_t)xs, src = (x0 >> 16) + bits + rowoff;
            uint32_t sb = 0, sg = 0, sr = 0, n = 0, nbk = 0, ngr = 0;
            uint32_t y = y0, yrem = ys;
            uint8_t B, G, R;

            if ((uint32_t)ys > 0x147) {
                uint32_t first = (x0 & 0xffff0000) - x0 + 0x10000, half = first >> 1;
                uint32_t edge = (x0 & 0xffff0000) + 0x10000, xend = xs + x0, mid = src + 1;

                do {
                    uint32_t ny = (y & 0xffff0000) + 0x10000, hy, w, used, rem, p, end, idx;
                    uint32_t mb = 0, mg = 0, mr = 0, mbk = 0, mgr;

                    if (ny > yrem + y)
                        ny = yrem + y;
                    hy = ny - y;
                    used = first;
                    w = (hy * half) >> 15;
                    if (edge > xend) {
                        used = xend - x0;
                        w = ((used >> 1) * hy) >> 15;
                    }
                    idx = M8(src);
                    sb += w * M8(spal + 4 * idx);
                    sg += w * M8(spal + 4 * idx + 1);
                    sr += w * M8(spal + 4 * idx + 2);
                    mgr = grey[idx];
                    n++;
                    nbk += black[idx];
                    ngr += grey[idx];
                    rem = xs - used;
                    end = (rem >> 16) + mid;
                    rem &= 0xffff;
                    for (p = mid; p != end; p++) {
                        idx = M8(p);
                        mb += M8(spal + 4 * idx);
                        mg += M8(spal + 4 * idx + 1);
                        mr += M8(spal + 4 * idx + 2);
                        mbk += black[idx];
                        mgr += grey[idx];
                    }
                    if (p != mid) {
                        n += end - mid;
                        sb += mb * hy;
                        sg += mg * hy;
                        sr += mr * hy;
                        nbk += mbk;
                        ngr += mgr;
                    }
                    if (rem > 0x147) {
                        idx = M8(p);
                        w = ((rem >> 1) * hy) >> 15;
                        sb += w * M8(spal + 4 * idx);
                        sg += w * M8(spal + 4 * idx + 1);
                        sr += w * M8(spal + 4 * idx + 2);
                        n++;
                        nbk += black[idx];
                        ngr += grey[idx];
                    }
                    yrem -= hy;
                    src += stride;
                    mid += stride;
                    y = ny;
                } while (yrem > 0x147);
            }
            if ((int32_t)(nbk / n) > 0x80) {
                B = G = R = 0;
            } else if ((int32_t)(ngr / n) > 0x80) {
                B = G = R = 1;
            } else {
                R = (uint8_t)(((sr >> 16) * (uint32_t)inv) >> 16);
                B = (uint8_t)(((sb >> 16) * (uint32_t)inv) >> 16);
                G = (uint8_t)(((sg >> 16) * (uint32_t)inv) >> 16);
                if ((R == 1 && G == 1 && B == 1) || (R == 0 && G == 0 && B == 0))
                    R = G = B = 0x10;
            }
            M8(out) = colour_8(R, G, B, cache, ranges, nranges, pal, quick);
        }
    }
}

static void FS98_STDCALL edges_24(uint32_t s, uint32_t back, uint32_t at);  /* 0x100062f0 */

/* 0x10005660 -- a surface for a DIB (`bits` its bits, or after its
 * palette), `w` by `h`, into *out: 8 bits shrunk to an 8-bit surface (over
 * `back` at `at` when given; `flags` to the shrink); else, or when that
 * surface cannot be had, a 24-bit one scaled as IMAGE_QUALITY says (1 area
 * averaging, 2 filtering, else nearest) and, for 24 bits with `back`, its
 * edges blended into it.
 * 1 when made. */
static uint32_t surface_from_bitmap(uint32_t info, uint32_t bits, int32_t w, int32_t h,
                                    uint32_t out, uint32_t back, uint32_t at, uint32_t flags)
{
    uint16_t bpp;
    uint32_t q;

    if (info == 0)
        return 0;
    bpp = R16(info, 0xe);
    if (bpp != 0x18 && bpp != 8)
        return 0;
    if (bits == 0) {
        bits = R32(info, 0) + info;
        if (R32(info, 0x10) == 3) {
            bits += 0xc;
        } else {
            uint32_t n = R32(info, 0x20);

            if (n == 0 && bpp <= 8)
                n = 1u << bpp;
            bits += n * 4;
        }
    }
    if (bpp == 8 && h_surface_new(out, w, h, 1) == 0) {
        if (back != 0)
            shrink_8_to_8(info, bits, M32(out), back, at, flags);
        else
            shrink_8_to_8_plain(info, bits, M32(out), flags);
        return 1;
    }
    if (h_surface_new(out, w, h, 4) != 0)
        return 0;
    q = D32(0x10025568);
    if (R16(info, 0xe) == 8) {
        if (q == 1)
            shrink_8(info, bits, M32(out));
        else if (q == 2)
            smooth_8(info, bits, M32(out));
        else
            scale_8(info, bits, M32(out));
        return 1;
    }
    if (q == 1)
        shrink_24(info, bits, M32(out));
    else if (q == 2)
        smooth_24(info, bits, M32(out));
    else
        scale_24(info, bits, M32(out));
    if (back != 0)
        edges_24(M32(out), back, at);
    return 1;
}

/* 0x100057e0 -- a 24-bit surface into an 8-bit one of HG2D's palette */
static void FS98_STDCALL to_8(uint32_t src, uint32_t dst)
{
    uint32_t pal = R32(HG2D, 0x8c), r, k;

    for (r = R32(src, 8); r-- != 0;) {
        uint32_t p = R32(R32(src, 0x10), 4 * r), d = R32(R32(dst, 0x10), 4 * r);

        for (k = R32(src, 4); k != 0; k--, d++, p += 3)
            M8(d) = colour_8(M8(p + 2), M8(p + 1), M8(p), DA(0x10010ee0), DA(0x1000f5d8), 2,
                             pal, 0);
    }
}

/* 0x100059e0 -- the same into the host's palette, by index */
static void FS98_STDCALL to_8_host(uint32_t src, uint32_t dst)
{
    uint8_t hostpal[0x300];
    uint32_t r, k;

    h_palette_copy(hostpal);
    for (r = R32(src, 8); r-- != 0;) {
        uint32_t p = R32(R32(src, 0x10), 4 * r), d = R32(R32(dst, 0x10), 4 * r);

        for (k = R32(src, 4); k != 0; k--, d++, p += 3)
            M8(d) = colour_8(M8(p + 2), M8(p + 1), M8(p), DA(0x10018ee0), DA(0x1000f5e8), 4,
                             P(hostpal), 1);
    }
}

/* 0x10005bd0 -- a 24-bit surface into a 15-bit one, each pixel scaled by
 * the 8-bit `light` surface's at `at` (255 full).  Black keeps only the top
 * bit; (1,1,1) is 0x421 over the old top bits.  Kept from retail: the
 * test meant for a result of 0 or 0x421 looks at bit 15 alone, so every
 * result without it becomes at least 0x842 in its low bits. */
static void FS98_STDCALL to_555(uint32_t src, uint32_t dst, uint32_t light, uint32_t at)
{
    uint32_t r, k;

    for (r = R32(src, 8); r-- != 0;) {
        uint32_t p = R32(R32(src, 0x10), 4 * r), d = R32(R32(dst, 0x10), 4 * r);
        uint32_t m = R32(R32(light, 0x10), 4 * (r + R32(at, 4))) + R32(at, 0);

        for (k = R32(src, 4); k != 0; k--, d += 2, p += 3, m++) {
            uint32_t v;

            if (M8(p + 2) == 0 && M8(p + 1) == 0 && M8(p) == 0) {
                R16(d, 0) &= 0x8000;
                continue;
            }
            if (M8(p + 2) == 1 && M8(p + 1) == 1 && M8(p) == 1) {
                R16(d, 0) = (R16(d, 0) & 0x8421) | 0x421;
                continue;
            }
            v = ((((uint32_t)M8(m) * M8(p + 2)) >> 11) & 0x1f) << 10 | (R16(d, 0) & 0x83ff);
            R16(d, 0) = (uint16_t)v;
            v = ((((uint32_t)M8(p + 1) * M8(m)) >> 11) & 0x1f) << 5 | (v & 0xfc1f);
            R16(d, 0) = (uint16_t)v;
            v = ((((uint32_t)M8(p) * M8(m)) >> 11) & 0x1f) | (v & 0xffe0);
            R16(d, 0) = (uint16_t)v;
            if ((v & 0x8000) == 0)
                R16(d, 0) = (uint16_t)((v & 0x8842) | 0x842);
        }
    }
}

/* 0x10005d20 -- the same into a 16-bit (565) one: black 0, (1,1,1) 0x821,
 * a result of 0 or 0x841 becomes 0x1042 */
static void FS98_STDCALL to_565(uint32_t src, uint32_t dst, uint32_t light, uint32_t at)
{
    uint32_t r, k;

    for (r = R32(src, 8); r-- != 0;) {
        uint32_t p = R32(R32(src, 0x10), 4 * r), d = R32(R32(dst, 0x10), 4 * r);
        uint32_t m = R32(R32(light, 0x10), 4 * (r + R32(at, 4))) + R32(at, 0);

        for (k = R32(src, 4); k != 0; k--, d += 2, p += 3, m++) {
            uint32_t v;

            if (M8(p + 2) == 0 && M8(p + 1) == 0 && M8(p) == 0) {
                R16(d, 0) = 0;
                continue;
            }
            if (M8(p + 2) == 1 && M8(p + 1) == 1 && M8(p) == 1) {
                R16(d, 0) = 0x821;
                continue;
            }
            v = ((((uint32_t)M8(m) * M8(p + 2)) >> 11) & 0x1f) << 11 | (R16(d, 0) & 0x7ff);
            R16(d, 0) = (uint16_t)v;
            v = ((((uint32_t)M8(p + 1) * M8(m)) >> 10) & 0x3f) << 5 | (v & 0xf81f);
            R16(d, 0) = (uint16_t)v;
            v = ((((uint32_t)M8(m) * M8(p)) >> 11) & 0x1f) | (v & 0xffe0);
            R16(d, 0) = (uint16_t)v;
            if ((uint16_t)v == 0 || (uint16_t)v == 0x841)
                R16(d, 0) = 0x1042;
        }
    }
}

/* 0x10005e50 -- a 24-bit surface into another, scaled by the light */
static void FS98_STDCALL light_24(uint32_t src, uint32_t dst, uint32_t light, uint32_t at)
{
    uint32_t r, k;

    for (r = R32(src, 8); r-- != 0;) {
        uint32_t p = R32(R32(src, 0x10), 4 * r), d = R32(R32(dst, 0x10), 4 * r);
        uint32_t m = R32(R32(light, 0x10), 4 * (r + R32(at, 4))) + R32(at, 0);

        for (k = R32(src, 4); k != 0; k--, d += 3, p += 3, m++) {
            M8(d + 2) = (uint8_t)(((uint32_t)M8(p + 2) * M8(m)) >> 8);
            M8(d + 1) = (uint8_t)(((uint32_t)M8(p + 1) * M8(m)) >> 8);
            M8(d) = (uint8_t)(((uint32_t)M8(m) * M8(p)) >> 8);
        }
    }
}

/* 0x10005f00 -- into a 32-bit one, the fourth byte 0 */
static void FS98_STDCALL light_32(uint32_t src, uint32_t dst, uint32_t light, uint32_t at)
{
    uint32_t r, k;

    for (r = R32(src, 8); r-- != 0;) {
        uint32_t p = R32(R32(src, 0x10), 4 * r), d = R32(R32(dst, 0x10), 4 * r);
        uint32_t m = R32(R32(light, 0x10), 4 * (r + R32(at, 4))) + R32(at, 0);

        for (k = R32(src, 4); k != 0; k--, d += 4, p += 3, m++) {
            M8(d + 2) = (uint8_t)(((uint32_t)M8(p + 2) * M8(m)) >> 8);
            M8(d + 1) = (uint8_t)(((uint32_t)M8(p + 1) * M8(m)) >> 8);
            M8(d + 3) = 0;
            M8(d) = (uint8_t)(((uint32_t)M8(p) * M8(m)) >> 8);
        }
    }
}

/* 0x10005fb0 -- pair[0] made the mask of pair[1]: 0 where it is black,
 * else colour word 3's low byte; a new 8-bit surface when the two are the
 * same */
static uint32_t FS98_STDCALL mask_make(uint32_t pair)
{
    uint32_t r, k;

    if (R32(pair, 4) == 0 || R32(pair, 0) == 0)
        return 0;
    if (R32(pair, 4) == R32(pair, 0))
        h_surface_new(pair, R32(R32(pair, 4), 4), R32(R32(pair, 4), 8), 1);
    for (r = R32(R32(pair, 4), 8); r-- != 0;) {
        uint32_t p = R32(R32(R32(pair, 4), 0x10), 4 * r);
        uint32_t d = R32(R32(R32(pair, 0), 0x10), 4 * r), p8 = p;

        for (k = R32(R32(pair, 4), 4); k != 0; k--, d++) {
            if (R32(R32(pair, 4), 0) == 1) {
                M8(d) = M8(p8) == 0 ? 0 : M8(R32(HG2D, 0x94) + 6);
                p8++;
            } else {
                M8(d) = (M8(p + 2) == 0 && M8(p + 1) == 0 && M8(p) == 0)
                            ? 0 : M8(R32(HG2D, 0x94) + 6);
                p += 3;
            }
        }
    }
    return 0;
}

/* 0x10006080 -- an 8-bit surface's rows copied into another's, the
 * second's width of bytes each */
static void FS98_STDCALL copy_8(uint32_t src, uint32_t dst)
{
    uint32_t r;

    for (r = R32(src, 8); r-- != 0;)
        memcpy((void *)(uintptr_t)R32(R32(dst, 0x10), 4 * r),
               (const void *)(uintptr_t)R32(R32(src, 0x10), 4 * r), R32(dst, 4));
}

/* 0x100060d0 -- pair[1] into pair[0] by the target's type: 1 8 bits (a
 * copy, or matched to HG2D's palette, or with flags bit 2 the host's), 2
 * 15 bits, 3 16 bits, 4 24 bits, 5 32 bits, each lit by `e`'s light
 * surface (+0x10) at `at` */
static uint32_t FS98_STDCALL surface_convert(uint32_t e, uint32_t pair, uint32_t at,
                                             uint32_t flags)
{
    uint32_t src = R32(pair, 4), dst = R32(pair, 0);

    if (src == 0 || dst == 0 || src == dst)
        return 0;
    switch (R32(dst, 0)) {
    case 1:
        if (R32(src, 0) == 1)
            copy_8(src, dst);
        else if (flags & 4)
            to_8_host(src, dst);
        else
            to_8(src, dst);
        break;
    case 2:
        to_555(src, dst, R32(e, 0x10), at);
        break;
    case 3:
        to_565(src, dst, R32(e, 0x10), at);
        break;
    case 4:
        light_24(src, dst, R32(e, 0x10), at);
        break;
    case 5:
        light_32(src, dst, R32(e, 0x10), at);
        break;
    }
    return 0;
}

/* 0x100061b0 -- clip a copy of *w by *h from (*sx, *sy) of a source sw by
 * sh to (*dx, *dy) of a target dw by dh, both ways; 0 if nothing is left,
 * else the height */
static uint32_t FS98_STDCALL clip(uint32_t psx, uint32_t psy, uint32_t pdx, uint32_t pdy,
                                  uint32_t pw, uint32_t ph, int32_t sw, int32_t sh,
                                  int32_t dw, int32_t dh)
{
    int32_t sx = M32(psx), sy = M32(psy), dx = M32(pdx), dy = M32(pdy);
    int32_t w = M32(pw), h = M32(ph), t;

    if (dx >= dw || (t = w + dx) < 0)
        return 0;
    if (dx < 0) {
        sx += w - t;
        w = t;
        dx = 0;
    }
    if (dy >= dh || (t = h + dy) < 0)
        return 0;
    if (dy < 0) {
        sy += h - t;
        dy = 0;
        h = t;
    }
    if (w + dx >= dw)
        w += dw - (w + dx);
    if (h + dy >= dh)
        h += dh - (h + dy);
    if (sx >= sw || (t = sx + w) < 0)
        return 0;
    if (sx < 0) {
        dx += w - t;
        sx = 0;
        w = t;
    }
    if (sy >= sh || (t = h + sy) < 0)
        return 0;
    if (sy < 0) {
        dy += h - t;
        sy = 0;
        h = t;
    }
    if (sx + w >= sw)
        w += sw - (sx + w);
    if (h + sy >= sh)
        h += sh - (h + sy);
    if (w <= 0 || h <= 0)
        return 0;
    M32(psx) = sx;
    M32(psy) = sy;
    M32(pdx) = dx;
    M32(pdy) = dy;
    M32(pw) = w;
    M32(ph) = h;
    return h;
}

/* 0x100062f0 -- a 24-bit gauge surface's black pixels next to colour
 * blended with the panel `back` (24 bits) behind it at `at`: the average
 * of its coloured neighbours moved towards the average of the background
 * where its neighbours are black, by a factor that falls as more of the
 * eight are coloured (0.9 for one .. 0.1 for eight).  Rows are read from
 * copies taken before the row above was changed.  Neighbours of (1,1,1)
 * count as neither; a result of black or (1,1,1) becomes (0x18,0x18,0x18).
 *
 * Kept from retail: the up-left neighbour is taken when the column and row
 * share a bit (i & j), not when both are nonzero; the background is read
 * round the pixel one left of and one row before the gauge's, the row
 * index going to -1 for the first; and a pixel with no background inside
 * the surface's memory divides by zero, into the host's repair. */
static void FS98_STDCALL edges_24(uint32_t s, uint32_t back, uint32_t at)
{
    static const double factor[9] = { 1.0, 0.9, 0.8, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1 };
    uint8_t none[3] = { 0, 0, 0 };
    int32_t sx = 0, sy = 0, ax = R32(at, 0), ay = R32(at, 4);
    int32_t w = R32(s, 4), h = R32(s, 8), i, j;
    uint32_t prevb = 0, curb = 0, nextb = 0, stride, lo, hi;

    if (clip(P(&sx), P(&sy), P(&ax), P(&ay), P(&w), P(&h), R32(s, 4), R32(s, 8),
             R32(back, 4), R32(back, 8)) == 0)
        return;
    stride = R32(s, 0xc);
    h_alloc(1, stride, &prevb, 0);
    h_alloc(1, stride, &curb, 0);
    h_alloc(1, stride, &nextb, 0);
    if (prevb == 0 || curb == 0 || nextb == 0)
        goto done;
    lo = R32(R32(back, 0x10), 0);
    hi = R32(R32(back, 0x10), 4 * (R32(back, 8) - 1)) + R32(back, 4) * 3;
    for (j = 0; j < h; j++) {
        uint32_t srow = R32(R32(s, 0x10), 4 * j);
        uint32_t bp = R32(R32(back, 0x10), 4 * (j + ay) - 4) + ax * 3 - 3;
        uint32_t prev, cur, next, out;

        if (j != 0)
            h_memmove(prevb, curb, stride);
        h_memmove(curb, srow, stride);
        if (j < h - 1)
            h_memmove(nextb, srow + stride, stride);
        next = nextb;
        prev = prevb;
        cur = curb;
        out = srow;
        for (i = 0; i < w; i++, out += 3, bp += 3, prev += 3, cur += 3, next += 3) {
            int32_t v[8];
            uint32_t at8[8], bk[8], k, nn = 0, nb = 0;
            int32_t snr = 0, sng = 0, snb = 0, sbr = 0, sbg = 0, sbb = 0;
            uint8_t R, G, B;
            double f;

            if (M32(cur) & 0xffffff)
                continue;
            v[0] = (i & j) != 0 ? (int32_t)(M32(prev - 3) & 0xffffff) : -1;
            v[1] = j != 0 ? (int32_t)(M32(prev) & 0xffffff) : -1;
            v[2] = j != 0 && i < w - 1 ? (int32_t)(M32(prev + 3) & 0xffffff) : -1;
            v[3] = i != 0 ? (int32_t)(M32(cur - 3) & 0xffffff) : -1;
            v[4] = i < w - 1 ? (int32_t)(M32(cur + 3) & 0xffffff) : -1;
            v[5] = i != 0 && j < h - 1 ? (int32_t)(M32(next - 3) & 0xffffff) : -1;
            v[6] = j < h - 1 ? (int32_t)(M32(next) & 0xffffff) : -1;
            v[7] = i < w - 1 && j < h - 1 ? (int32_t)(M32(next + 3) & 0xffffff) : -1;
            at8[0] = v[0] < 0 ? P(none) : prev - 3;
            at8[1] = v[1] < 0 ? P(none) : prev;
            at8[2] = v[2] < 0 ? P(none) : prev + 3;
            at8[3] = v[3] < 0 ? P(none) : cur - 3;
            at8[4] = v[4] < 0 ? P(none) : cur + 3;
            at8[5] = v[5] < 0 ? P(none) : next - 3;
            at8[6] = v[6] < 0 ? P(none) : next;
            at8[7] = v[7] < 0 ? P(none) : next + 3;
            for (k = 0; k < 8; k++)
                if (v[k] < 0)
                    v[k] = 0;
            if ((v[0] | v[1] | v[2] | v[3] | v[4] | v[5] | v[6] | v[7]) == 0)
                continue;
            bk[0] = bp - R32(back, 0xc) - 3;
            bk[1] = bp - R32(back, 0xc);
            bk[2] = bp - R32(back, 0xc) + 3;
            bk[3] = bp - 3;
            bk[4] = bp + 3;
            bk[5] = bp + R32(back, 0xc) - 3;
            bk[6] = bp + R32(back, 0xc);
            bk[7] = bp + R32(back, 0xc) + 3;
            for (k = 0; k < 8; k++) {
                if (bk[k] < lo || bk[k] > hi || v[k] == 0x10101)
                    continue;
                if (v[k] != 0) {
                    nn++;
                    snr += M8(at8[k] + 2);
                    sng += M8(at8[k] + 1);
                    snb += M8(at8[k]);
                } else {
                    nb++;
                    sbr += M8(bk[k] + 2);
                    sbg += M8(bk[k] + 1);
                    sbb += M8(bk[k]);
                }
            }
            snr /= (int32_t)nn;
            snb /= (int32_t)nn;
            sng /= (int32_t)nn;
            f = factor[nn];
            R = (uint8_t)FTOL(FI(sbr / (int32_t)nb - snr) * f) + (uint8_t)snr;
            M8(out + 2) = R;
            B = (uint8_t)FTOL(FI(sbb / (int32_t)nb - snb) * f) + (uint8_t)snb;
            M8(out) = B;
            G = (uint8_t)FTOL(FI(sbg / (int32_t)nb - sng) * f) + (uint8_t)sng;
            M8(out + 1) = G;
            if ((R == 0 && G == 0 && M8(out) == 0) ||
                (M8(out + 2) == 1 && M8(out + 1) == 1 && M8(out) == 1)) {
                M8(out + 2) = 0x18;
                M8(out) = 0x18;
                M8(out + 1) = 0x18;
            }
        }
    }
done:
    if (prevb != 0)
        h_free(&prevb);
    if (curb != 0)
        h_free(&curb);
    if (nextb != 0)
        h_free(&nextb);
}

/* 0x10006dc0 -- a rectangle `size` (w, h) of `src` at `at_src` copied to
 * `dst` at `at_dst`, both of one type, in bytes, clipped to both; with
 * flags bit 0 the bytes equal to the key at 0x10010e74 are left out */
static void FS98_STDCALL blit(uint32_t dst, uint32_t src, uint32_t at_dst, uint32_t at_src,
                              uint32_t size, uint32_t flags)
{
    int32_t dx, dy, sx, sy, w, h;
    uint32_t d, s, k;

    if (src == 0 || dst == 0 || R32(src, 0) != R32(dst, 0))
        return;
    dx = R32(at_dst, 0);
    dy = R32(at_dst, 4);
    sx = R32(at_src, 0);
    sy = R32(at_src, 4);
    w = R32(size, 0);
    h = R32(size, 4);
    if (clip(P(&dx), P(&dy), P(&sx), P(&sy), P(&w), P(&h), R32(dst, 4), R32(dst, 8),
             R32(src, 4), R32(src, 8)) == 0)
        return;
    d = R32(R32(dst, 0x10), 4 * dy) + dx;
    if (flags & 1) {
        s = R32(R32(src, 0x10), 4 * sy) + sx;
        for (; h > 0; h--) {
            for (k = 0; k < (uint32_t)w; k++)
                if (M8(s + k) != D8(0x10010e74))
                    M8(d + k) = M8(s + k);
            s += R32(src, 0xc);
            d += R32(dst, 0xc);
        }
        return;
    }
    s = R32(R32(src, 0x10), 4 * sy) + sx;
    for (; h > 0; h--) {
        h_memmove(d, s, w);
        s += R32(src, 0xc);
        d += R32(dst, 0xc);
    }
}

/* 0x10006f20 -- `dst` made from `base` at `at`, then `over` keyed onto it,
 * then the key colour (1,1,1 in each format) turned black */
static void FS98_STDCALL compose(uint32_t dst, uint32_t over, uint32_t base, uint32_t at)
{
    uint32_t p, k;

    blit(dst, base, DA(0x10010e60), at, dst + 4, 0);
    blit(dst, over, DA(0x10010e60), DA(0x10010e60), dst + 4, 1);
    p = R32(dst, 0x18);
    switch (R32(dst, 0)) {
    case 1:
        for (k = 0; k < R32(dst, 0x14); k++, p++)
            if (M8(p) == 1)
                M8(p) = 0;
        break;
    case 2:
        for (k = 0; k < R32(dst, 0x14); k += 2, p += 2) {
            uint16_t v = R16(p, 0);

            if ((v & 0x7c00) == 0x400 && (v & 0x1f) == 1 && (v & 0x3e0) == 0x20)
                R16(p, 0) = v & 0x8000;
        }
        break;
    case 3:
        for (k = 0; k < R32(dst, 0x14); k += 2, p += 2) {
            uint16_t v = R16(p, 0);

            if ((v & 0xf800) == 0x800 && (v & 0x1f) == 1 && (v & 0x7e0) == 0x20)
                R16(p, 0) = 0;
        }
        break;
    case 4:
        for (k = 0; k < R32(dst, 0x14); k += 3, p += 3)
            if (M8(p + 2) == 1 && M8(p) == 1 && M8(p + 1) == 1) {
                M8(p + 2) = 0;
                M8(p) = 0;
                M8(p + 1) = 0;
            }
        break;
    case 5:
        for (k = 0; k < R32(dst, 0x14); k += 4, p += 4)
            if ((M32(p) & 0xffffff) == 0x10101)
                M32(p) = 0;
        break;
    }
}

/* 0x100070c0 -- a DIB section of surface type `type` (0/1 8 bits with the
 * panel colours' palette: HG2D's through its remap, or with flags bit 2 the
 * host's; 2/3 16; 4 24; 5 32), `w` by `h` top down, its bits to *bits.
 * Retail has no header for a type past 5 and writes through an
 * uninitialised pointer; here a null one. */
static uint32_t FS98_STDCALL dib_new(uint32_t type, int32_t w, int32_t h, uint32_t bits,
                                     uint32_t flags)
{
    uint8_t hostpal[0x300];
    uint32_t info = 0, k, rg, pal;
    HBITMAP bm;

    switch (type) {
    case 0:
    case 1:
        h_alloc(1, 0x428, &info, 1);
        R32(info, 0x10) = 0;
        R16(info, 0xe) = 8;
        for (k = 0; k < 0x100; k++) {
            M8(info + 0x28 + 4 * k + 2) = 0;
            M8(info + 0x28 + 4 * k) = 0;
            M8(info + 0x28 + 4 * k + 1) = 0;
            M8(info + 0x28 + 4 * k + 3) = 0;
        }
        if (flags & 4) {
            h_palette_copy(hostpal);
            for (rg = DA(0x1000f5e8); rg < DA(0x1000f608); rg += 8)
                for (k = R32(rg, 0); (int32_t)k <= (int32_t)R32(rg, 4); k++) {
                    M8(info + 0x28 + 4 * k + 2) = hostpal[3 * k];
                    M8(info + 0x28 + 4 * k) = hostpal[3 * k + 2];
                    M8(info + 0x28 + 4 * k + 1) = hostpal[3 * k + 1];
                }
        } else {
            pal = R32(HG2D, 0x8c);
            for (rg = DA(0x1000f5d8); rg < DA(0x1000f5e8); rg += 8)
                for (k = R32(rg, 0); (int32_t)k <= (int32_t)R32(rg, 4); k++) {
                    M8(info + 0x28 + 4 * M8(R32(HG2D, 0x90) + k) + 2) = M8(pal + 3 * k);
                    M8(info + 0x28 + 4 * M8(R32(HG2D, 0x90) + k)) = M8(pal + 3 * k + 2);
                    M8(info + 0x28 + 4 * M8(R32(HG2D, 0x90) + k) + 1) = M8(pal + 3 * k + 1);
                }
        }
        break;
    case 2:
    case 3:
        h_alloc(1, 0x28, &info, 1);
        R16(info, 0xe) = 0x10;
        break;
    case 4:
        h_alloc(1, 0x28, &info, 1);
        R16(info, 0xe) = 0x18;
        break;
    case 5:
        h_alloc(1, 0x28, &info, 1);
        R16(info, 0xe) = 0x20;
        break;
    }
    R32(info, 0) = 0x28;
    R32(info, 4) = w;
    R32(info, 8) = -h;
    R16(info, 0xc) = 1;
    R32(info, 0x10) = 0;
    bm = CreateDIBSection(NULL, (BITMAPINFO *)(uintptr_t)info, 0, (void **)(uintptr_t)bits,
                          NULL, 0);
    h_free(&info);
    return P(bm);
}

/* 0x10007300 -- a surface record (as host +0x1b4's) over the bits *bits:
 * type, w, h, the row's bytes (by the bytes a pixel at 0x1000f5c0, to a
 * dword), the row table, the size, the bits.  -1 for a type past 5. */
static uint32_t FS98_STDCALL surface_over(uint32_t out, int32_t w, int32_t h, int32_t type,
                                          uint32_t bits)
{
    uint32_t rec, r, p;

    if (type < 0 || type >= 6)
        return 0xffffffff;
    h_alloc(1, 0x20, &rec, 1);
    R32(rec, 0) = type;
    R32(rec, 4) = w;
    R32(rec, 8) = h;
    R32(rec, 0xc) = D32(0x1000f5c0 + 4 * type) * w;
    R32(rec, 0xc) = (R32(rec, 0xc) + 3) & ~3u;
    R32(rec, 0x14) = R32(rec, 0xc) * R32(rec, 8);
    R32(rec, 0x18) = M32(bits);
    h_alloc(1, R32(rec, 8) * 4, rec + 0x10, 1);
    p = R32(rec, 0x18);
    for (r = 0; (int32_t)r < (int32_t)R32(rec, 8); r++) {
        R32(R32(rec, 0x10), 4 * r) = p;
        p += R32(rec, 0xc);
    }
    M32(out) = rec;
    return 0;
}

/* 0x100073e0 -- free a surface_over record (not its bits) */
static uint32_t FS98_STDCALL surface_over_free(uint32_t rec)
{
    if (rec == 0)
        return 0xffffffff;
    if (R32(rec, 0x10) != 0)
        h_free(rec + 0x10);
    h_free(&rec);
    return 0;
}

/* ---- the gauge variables' functions, named in the table at 0x10000610 -------------
 *
 * Each is f(var, op) on a gauge's MODULE_VAR (+0x4 its pointer, +0x8 its
 * type, +0xc its value): op 0 makes it a 64-bit float (type 0x21) with no
 * pointer, op 1 works its value out. */
#define VAR_INIT(v) do { R32(v, 4) = 0; R32(v, 8) = 0x21; } while (0)
#define VAL(v) RDB(v, 0xc)
#define GD(o) MD(G + (o))

/* the angle, in degrees, stepped into (0, 360] as retail steps it: by
 * stores above, in the x87 below */
static void wrap_360(uint32_t v)
{
    long double x;

    if ((long double)VAL(v) > RD(0x1000e0a8))
        do {
            x = (long double)VAL(v) - RD(0x1000e0a8);
            VAL(v) = (double)x;
        } while (x > RD(0x1000e0a8));
    if ((long double)VAL(v) > RD(0x1000e0b0))
        return;
    x = VAL(v);
    do
        x -= RD(0x1000e0b8);
    while (!(x > RD(0x1000e0b0)));
    VAL(v) = (double)x;
}

/* 0x10007420, 0x10007480, 0x100074e0, 0x10007540: a GLOBAL double over a
 * 1/256 scale (GLOBAL +0xaf4) */
static void var_scaled(uint32_t v, uint32_t op, uint32_t field)
{
    long double a;

    if (op == 0) {
        VAR_INIT(v);
        return;
    }
    if (op != 1)
        return;
    a = GD(field);
    VAL(v) = (double)a;
    VAL(v) = (double)(a / (FI((int16_t)G16(0xaf4)) * RD(0x1000e070)));
}
static uint32_t FS98_STDCALL v_7420(uint32_t v, uint32_t op) { var_scaled(v, op, 0x918); return 0; }
static uint32_t FS98_STDCALL v_7480(uint32_t v, uint32_t op) { var_scaled(v, op, 0x9b0); return 0; }
static uint32_t FS98_STDCALL v_74e0(uint32_t v, uint32_t op) { var_scaled(v, op, 0xa48); return 0; }
static uint32_t FS98_STDCALL v_7540(uint32_t v, uint32_t op) { var_scaled(v, op, 0xae0); return 0; }

/* 0x100075a0 -- height above the ground, feet unless metric */
static uint32_t FS98_STDCALL v_75a0(uint32_t v, uint32_t op)
{
    long double x;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    x = FI((uint32_t)G16(0x330) - (uint32_t)G16(0xf48)) * RD(0x1000e078);
    VAL(v) = (double)x;
    x += FI(G32(0x574));
    VAL(v) = (double)x;
    if (G16(0xc18) != 2)
        VAL(v) = (double)(x * RD(0x1000e080));
    return 0;
}

#define VAR_SIMPLE(name, expr) \
    static uint32_t FS98_STDCALL name(uint32_t v, uint32_t op) \
    { \
        if (op == 0) { \
            VAR_INIT(v); \
            return 0; \
        } \
        if (op == 1) \
            VAL(v) = (double)(expr); \
        return 0; \
    }

VAR_SIMPLE(v_7620, FI(G16(0x330)) * RD(0x1000e088))                        /* 0x10007620 */
VAR_SIMPLE(v_7670, FI(G16(0x330)) * RD(0x1000e090))                        /* 0x10007670 */
VAR_SIMPLE(v_76c0, FI(G32(0x578)) * RD(0x1000e098))                        /* 0x100076c0 */
VAR_SIMPLE(v_7750, FIU(G32(0x57c)) * RD(0x1000e098))                       /* 0x10007750 */
VAR_SIMPLE(v_7da0, FIU(G32(0x2c0) * 360u) * RD(0x1000e0e8))                /* 0x10007da0 */

/* 0x10007700, 0x100077a0: a double through GLOBAL +0x11d4's +0x20/+0x24,
 * copied as two dwords */
static uint32_t FS98_STDCALL v_7700(uint32_t v, uint32_t op)
{
    uint32_t p;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    p = R32(G32(0x11d4), 0x20);
    R32(v, 0xc) = R32(p, 0);
    R32(v, 0x10) = R32(p, 4);
    return 0;
}

static uint32_t FS98_STDCALL v_77a0(uint32_t v, uint32_t op)
{
    uint32_t p;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    p = R32(G32(0x11d4), 0x24);
    R32(v, 0xc) = R32(p, 0);
    R32(v, 0x10) = R32(p, 4);
    return 0;
}

/* 0x100077f0 -- the heading less the magnetic variation */
static uint32_t FS98_STDCALL v_77f0(uint32_t v, uint32_t op)
{
    long double x;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    x = FIU(G32(0x580)) * RD(0x1000e098);
    VAL(v) = (double)x;
    VAL(v) = (double)(x - FI(G16(0x2a0)) * RD(0x1000e0a0));
    wrap_360(v);
    return 0;
}

/* 0x100078c0 -- the heading less (GLOBAL +0xc3e less the variation) */
static uint32_t FS98_STDCALL v_78c0(uint32_t v, uint32_t op)
{
    long double x;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    x = FIU(G32(0x580)) * RD(0x1000e098);
    VAL(v) = (double)x;
    VAL(v) = (double)(x - FI((int32_t)(int16_t)G16(0xc3e) - (int32_t)G16(0x2a0)) *
                              RD(0x1000e0c0));
    wrap_360(v);
    return 0;
}

/* 0x100079a0 -- the heading */
static uint32_t FS98_STDCALL v_79a0(uint32_t v, uint32_t op)
{
    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    VAL(v) = (double)(FIU(G32(0x580)) * RD(0x1000e098));
    wrap_360(v);
    return 0;
}

/* 0x10007a50 -- the ADF needle's bearing, 90 with no station */
static uint32_t FS98_STDCALL v_7a50(uint32_t v, uint32_t op)
{
    uint32_t s;
    long double x;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    s = G32(0x384);
    if (s == 0 || (s = R32(s, 4)) == 0 || R8(s, 0x1c) == 8) {
        R32(v, 0xc) = 0;
        R32(v, 0x10) = 0x40568000;
        return 0;
    }
    x = FI(G16(0xc60)) - (FIU(G32(0x580) >> 16) - FI(G16(0x2a0)));
    x = (x - RD(0x1000e0c8)) * RD(0x1000e0a0);
    VAL(v) = (double)x;
    if (x < RD(0x1000e0b0))
        VAL(v) = (double)(x - RD(0x1000e0b8));
    if (!((long double)VAL(v) < RD(0x1000e0a8)))
        VAL(v) = (double)((long double)VAL(v) - RD(0x1000e0a8));
    return 0;
}

/* 0x10007b70, 0x10007bf0: MAIN +0x80's tenths, -1 for none or failure */
static void var_main_tenths(uint32_t v, uint32_t op, uint32_t what)
{
    int32_t t = op;

    if (op == 0) {
        VAR_INIT(v);
        return;
    }
    if (op != 1)
        return;
    if (MN(0x80, f2)(what, P(&t)) != 0) {
        R32(v, 0xc) = 0;
        R32(v, 0x10) = 0xbff00000;
    } else if (t == -1) {
        VAL(v) = RD(0x1000e0d0);
    } else {
        VAL(v) = (double)(FI(t) * RD(0x1000e0d8));
    }
}
static uint32_t FS98_STDCALL v_7b70(uint32_t v, uint32_t op) { var_main_tenths(v, op, 1); return 0; }
static uint32_t FS98_STDCALL v_7bf0(uint32_t v, uint32_t op) { var_main_tenths(v, op, 3); return 0; }

/* 0x10007c70, 0x10007cd0: MAIN +0x80's whole value, 0 on failure */
static void var_main_whole(uint32_t v, uint32_t op, uint32_t what)
{
    int32_t t = op;

    if (op == 0) {
        VAR_INIT(v);
        return;
    }
    if (op != 1)
        return;
    if (MN(0x80, f2)(what, P(&t)) != 0) {
        R32(v, 0xc) = 0;
        R32(v, 0x10) = 0;
    } else {
        VAL(v) = (double)FI(t);
    }
}
static uint32_t FS98_STDCALL v_7c70(uint32_t v, uint32_t op) { var_main_whole(v, op, 2); return 0; }
static uint32_t FS98_STDCALL v_7cd0(uint32_t v, uint32_t op) { var_main_whole(v, op, 4); return 0; }

/* 0x10007d30 -- by GLOBAL +0x612: +0x2b8 or +0x2bc in 1/128ths; for any
 * other retail scales what is left in the op's slot, 1 */
static uint32_t FS98_STDCALL v_7d30(uint32_t v, uint32_t op)
{
    int32_t t = op;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    if (G16(0x612) == 0)
        t = G32(0x2b8);
    else if (G16(0x612) == 1)
        t = G32(0x2bc);
    VAL(v) = (double)(FI(t) * RD(0x1000e0e0));
    return 0;
}

/* 0x10007e00, 0x10007ec0: three products of GLOBAL dwords, as 2^-23 */
static void var_products(uint32_t v, uint32_t op, uint32_t a, uint32_t b, uint32_t c,
                         uint32_t d, uint32_t e, uint32_t f)
{
    long double x;

    if (op == 0) {
        VAR_INIT(v);
        return;
    }
    if (op != 1)
        return;
    x = FIU(G32(a)) * FIU(G32(b));
    x = x + FIU(G32(c)) * FIU(G32(d));
    x = x + FIU(G32(e)) * FIU(G32(f));
    VAL(v) = (double)(x * RD(0x1000e0f0));
}
static uint32_t FS98_STDCALL v_7e00(uint32_t v, uint32_t op)
{
    var_products(v, op, 0xba0, 0xb9c, 0xba4, 0xba8, 0xb94, 0xb98);
    return 0;
}
static uint32_t FS98_STDCALL v_7ec0(uint32_t v, uint32_t op)
{
    var_products(v, op, 0xb80, 0xb7c, 0xb8c, 0xb90, 0xb84, 0xb88);
    return 0;
}
VAR_SIMPLE(v_7f80, FIU(G32(0xb74)) * FIU(G32(0xb78)) * RD(0x1000e0f0))     /* 0x10007f80 */

/* 0x10007ff0 -- 1, 2 or 3 by which of GLOBAL +0xbb0, +0xbae, +0xbac is
 * set first, else 0 */
static uint32_t FS98_STDCALL v_7ff0(uint32_t v, uint32_t op)
{
    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    R32(v, 0xc) = 0;
    if (G16(0xbb0) != 0)
        R32(v, 0x10) = 0x3ff00000;
    else if (G16(0xbae) != 0)
        R32(v, 0x10) = 0x40000000;
    else if (G16(0xbac) != 0)
        R32(v, 0x10) = 0x40080000;
    else
        R32(v, 0x10) = 0;
    return 0;
}

/* 0x10008070 -- the active view's zoom (GLOBAL +0x11a0's view block: the
 * map's, or the mode's), in 1/64ths; nothing without a view.  For a mode
 * past 4 retail scales the op's slot, 1. */
static uint32_t FS98_STDCALL v_8070(uint32_t v, uint32_t op)
{
    uint32_t w, x = op;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    w = G32(0x11a0);
    if (w == 0)
        return 0;
    if (R32(w, 0x10) == 1)
        x = R16(w, 0x26);
    else if (R32(w, 0x14) <= 4)
        x = R16(w, 0x1c + 2 * R32(w, 0x14));
    VAL(v) = (double)(FIU(x) * RD(0x1000e0f8));
    return 0;
}

/* 0x10008130 -- GLOBAL +0x7d4 in 1/65536ths, in feet unless metric */
static uint32_t FS98_STDCALL v_8130(uint32_t v, uint32_t op)
{
    long double x;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    x = FIU(G32(0x7d4)) * RD(0x1000e0e8);
    VAL(v) = (double)x;
    if (G16(0xc18) != 2)
        VAL(v) = (double)(x * RD(0x1000e100));
    return 0;
}

/* 0x100081a0 -- GLOBAL +0x7f0: scaled when metric, else its whole part */
static uint32_t FS98_STDCALL v_81a0(uint32_t v, uint32_t op)
{
    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    if (G16(0xc18) == 2)
        VAL(v) = (double)(FI(G32(0x7f0)) * RD(0x1000e108));
    else
        VAL(v) = (double)FI((int32_t)G32(0x7f0) / 0x10000);
    return 0;
}

/* 0x10008210.. -- the tuned stations (GLOBAL +0x384's +0 and +4): a byte
 * (type 4), the name (type 0x26), and two words as doubles; zeros
 * without one */
static uint32_t station(uint32_t which)
{
    uint32_t s = G32(0x384);

    return s == 0 ? 0 : R32(s, which);
}

static void var_station_byte(uint32_t v, uint32_t op, uint32_t which)
{
    uint32_t s;

    if (op == 0) {
        R32(v, 4) = 0;
        R32(v, 8) = 4;
        return;
    }
    if (op != 1)
        return;
    s = station(which);
    R32(v, 0xc) = s == 0 ? 0 : R8(s, 0x1c);
}

static void var_station_name(uint32_t v, uint32_t op, uint32_t which)
{
    uint32_t s;

    if (op == 0) {
        R32(v, 4) = 0;
        R32(v, 8) = 0x26;
        return;
    }
    if (op != 1)
        return;
    s = station(which);
    R32(v, 0xc) = s == 0 ? 0 : s + 0x22;
}

static void var_station_word(uint32_t v, uint32_t op, uint32_t which, uint32_t field)
{
    uint32_t s;

    if (op == 0) {
        VAR_INIT(v);
        return;
    }
    if (op != 1)
        return;
    s = station(which);
    if (s == 0) {
        R32(v, 0xc) = 0;
        R32(v, 0x10) = 0;
        return;
    }
    VAL(v) = (double)FI(R16(s, field));
}

static uint32_t FS98_STDCALL v_8210(uint32_t v, uint32_t op) { var_station_byte(v, op, 0); return 0; }
static uint32_t FS98_STDCALL v_8270(uint32_t v, uint32_t op) { var_station_name(v, op, 0); return 0; }
static uint32_t FS98_STDCALL v_82d0(uint32_t v, uint32_t op) { var_station_word(v, op, 0, 0x1a); return 0; }
static uint32_t FS98_STDCALL v_8340(uint32_t v, uint32_t op) { var_station_word(v, op, 0, 0x18); return 0; }
static uint32_t FS98_STDCALL v_83b0(uint32_t v, uint32_t op) { var_station_byte(v, op, 4); return 0; }
static uint32_t FS98_STDCALL v_8410(uint32_t v, uint32_t op) { var_station_name(v, op, 4); return 0; }
static uint32_t FS98_STDCALL v_8470(uint32_t v, uint32_t op) { var_station_word(v, op, 4, 0x1a); return 0; }
static uint32_t FS98_STDCALL v_84e0(uint32_t v, uint32_t op) { var_station_word(v, op, 4, 0x18); return 0; }

/* 0x10008550, 0x100085b0: the third station (+8): its name at +0x1a, its
 * word at +0x18 */
static uint32_t FS98_STDCALL v_8550(uint32_t v, uint32_t op)
{
    uint32_t s;

    if (op == 0) {
        R32(v, 4) = 0;
        R32(v, 8) = 0x26;
        return 0;
    }
    if (op != 1)
        return 0;
    s = station(8);
    R32(v, 0xc) = s == 0 ? 0 : s + 0x1a;
    return 0;
}
static uint32_t FS98_STDCALL v_85b0(uint32_t v, uint32_t op) { var_station_word(v, op, 8, 0x18); return 0; }

/* 0x10008620 -- the third station's bearing (GLOBAL +0xc6a in degrees,
 * whole), 90 without one */
static uint32_t FS98_STDCALL v_8620(uint32_t v, uint32_t op)
{
    uint32_t s;

    if (op == 0) {
        VAR_INIT(v);
        return 0;
    }
    if (op != 1)
        return 0;
    s = station(8);
    if (s == 0 || R16(s, 0x18) == 0) {
        R32(v, 0xc) = 0;
        R32(v, 0x10) = 0x40568000;
        return 0;
    }
    VAL(v) = (double)FI((int32_t)((int16_t)G16(0xc6a) * 360) / 0x10000);
    return 0;
}

/* 0x100086a0.. -- a bit of SOUND +0x44's state as 1 or 0 */
static void var_sound(uint32_t v, uint32_t op, uint32_t mask)
{
    if (op == 0) {
        VAR_INIT(v);
        return;
    }
    if (op != 1)
        return;
    R32(v, 0xc) = 0;
    R32(v, 0x10) = (SN(0x44, f0)() & mask) != 0 ? 0x3ff00000 : 0;
}
static uint32_t FS98_STDCALL v_86a0(uint32_t v, uint32_t op) { var_sound(v, op, 0x01); return 0; }
static uint32_t FS98_STDCALL v_86f0(uint32_t v, uint32_t op) { var_sound(v, op, 0x02); return 0; }
static uint32_t FS98_STDCALL v_8740(uint32_t v, uint32_t op) { var_sound(v, op, 0x10); return 0; }
static uint32_t FS98_STDCALL v_8790(uint32_t v, uint32_t op) { var_sound(v, op, 0x04); return 0; }
static uint32_t FS98_STDCALL v_87e0(uint32_t v, uint32_t op) { var_sound(v, op, 0x08); return 0; }

/* the variables' table: id i at 0x1000f608 + 16 i, { link, offset, type,
 * function } */
#define VT(id, o) D32(0x1000f608 + 16 * (id) + (o))

/* 0x10008830 -- Linkage +0x94: a named variable a gauge publishes, at the
 * head of the list at 0x10010e88: { name, pointer, type, next } and the
 * name */
static uint32_t FS98_STDCALL named_add(uint32_t ptr, uint32_t type, uint32_t name)
{
    uint32_t rec = name;

    h_alloc(1, h_strlen(name) + 0x11, &rec, 0);
    h_strcpy(rec + 0x10, name);
    R32(rec, 0) = rec + 0x10;
    R32(rec, 4) = ptr;
    R32(rec, 8) = type;
    R32(rec, 0xc) = D32(0x10010e88);
    D32(0x10010e88) = rec;
    return rec;
}

/* 0x100088b0 -- Linkage +0x98: a gauge's variable made ready: by its
 * function's op 0, or its pointer and type from the table.  Kept from
 * retail: the test for a GLOBAL entry (which would take one more
 * indirection) compares the entry's link with GLOBAL's address, which the
 * table never holds. */
static uint32_t FS98_STDCALL var_init(uint32_t v)
{
    uint32_t id, p;

    if (v == 0)
        return 0;
    id = R32(v, 0);
    if (id == 0) {
        R32(v, 4) = 0;
        R32(v, 8) = 0;
        return 0;
    }
    if (VT(id, 0xc) != 0)
        return CALL(DA(0x1000f608 + 16 * id), 0xc, f2)(v, 0);
    p = M32(VT(id, 0));
    if (p == 0) {
        R32(v, 4) = 0;
        R32(v, 8) = 0;
        return 0;
    }
    R32(v, 4) = p + VT(id, 4);
    if (VT(id, 0) == G)
        R32(v, 4) = M32(p + VT(id, 4));
    R32(v, 8) = VT(id, 8);
    return VT(id, 8);
}

/* 0x10008920 -- Linkage +0x9c: a variable by name from the list */
static uint32_t FS98_STDCALL named_find(uint32_t v, uint32_t name)
{
    uint32_t rec;

    R32(v, 4) = 0;
    R32(v, 8) = 0;
    for (rec = D32(0x10010e88); rec != 0; rec = R32(rec, 0xc))
        if (h_strcmp(name, R32(rec, 0)) == 0) {
            R32(v, 4) = R32(rec, 4);
            R32(v, 8) = R32(rec, 8);
            return 0;
        }
    return 0;
}

/* 0x10008980 -- Linkage +0xa0: a gauge's variable's value, by its function
 * or as its type reads its pointer */
static uint32_t FS98_STDCALL var_lookup(uint32_t v)
{
    uint32_t p, id = R32(v, 0);

    if (VT(id, 0xc) != 0)
        return CALL(DA(0x1000f608 + 16 * id), 0xc, f2)(v, 1);
    p = R32(v, 4);
    if (p == 0)
        return var_init(v);
    switch (R32(v, 8)) {
    case 0x01: R32(v, 0xc) = 0; R32(v, 0x10) = M8(p) != 0 ? 0x3ff00000 : 0; break;
    case 0x02: VAL(v) = (double)FI(M8(p)); break;
    case 0x03: VAL(v) = (double)FI((int8_t)M8(p)); break;
    case 0x04: case 0x05: R32(v, 0xc) = M8(p); break;
    case 0x06: R32(v, 0xc) = 0; R32(v, 0x10) = M16(p) != 0 ? 0x3ff00000 : 0; break;
    case 0x07: VAL(v) = (double)(FI(M16(p)) * RD(0x1000e0a0)); break;
    case 0x08: case 0x10: VAL(v) = (double)FI(M16(p)); break;
    case 0x09: VAL(v) = (double)FI((int16_t)M16(p)); break;
    case 0x0a: VAL(v) = (double)(FI(M16(p)) * RD(0x1000e070)); break;
    case 0x0b: VAL(v) = (double)(FI((int16_t)M16(p)) * RD(0x1000e070)); break;
    case 0x0c: case 0x0d: case 0x0e: case 0x0f: R32(v, 0xc) = M16(p); break;
    case 0x11: case 0x22: R32(v, 0xc) = 0; R32(v, 0x10) = M32(p) != 0 ? 0x3ff00000 : 0; break;
    case 0x12: case 0x13: case 0x19: VAL(v) = (double)FIU(M32(p)); break;
    case 0x14: VAL(v) = (double)FI(M32(p)); break;
    case 0x15: VAL(v) = (double)(FIU(M32(p)) * RD(0x1000e0e8)); break;
    case 0x16: VAL(v) = (double)(FI(M32(p)) * RD(0x1000e0e8)); break;
    case 0x17: case 0x18: case 0x23: case 0x24: case 0x26: R32(v, 0xc) = M32(p); break;
    case 0x1a: case 0x1b:
        VAL(v) = (double)(FI(M16(p + 2)) - FIU(M32(p + 4)) * RD(0x1000e110));
        break;
    case 0x1c: VAL(v) = (double)(FIU(M32(p + 4)) - FI(M16(p + 2)) * RD(0x1000e118)); break;
    case 0x1d: VAL(v) = (double)(FI(M32(p + 4)) - FI(M16(p + 2)) * RD(0x1000e118)); break;
    case 0x21: R32(v, 0xc) = M32(p); R32(v, 0x10) = M32(p + 4); break;
    case 0x25: R32(v, 0xc) = p; break;
    }
    return 0;
}

/* 0x10008ce0 -- Linkage +0xa4: unpublish a named variable.  The records
 * come from the host's allocator and go back through MSVCRT's free, as
 * retail's do; an empty list faults on its head, as retail's does. */
static uint32_t FS98_STDCALL named_remove(uint32_t name)
{
    uint32_t prev, cur;

    if (h_strcmp(R32(D32(0x10010e88), 0), name) == 0) {
        cur = D32(0x10010e88);
        D32(0x10010e88) = R32(cur, 0xc);
        free((void *)(uintptr_t)cur);
        return 0;
    }
    prev = D32(0x10010e88);
    for (cur = R32(prev, 0xc); cur != 0; prev = cur, cur = R32(cur, 0xc))
        if (h_strcmp(R32(cur, 0), name) == 0) {
            R32(prev, 0xc) = R32(cur, 0xc);
            free((void *)(uintptr_t)cur);
            return 0;
        }
    return 0;
}

/* 0x10008d70 -- Linkage +0xa8: empty the list.  Kept from retail: each
 * record is unlinked and MSVCRT's free is handed the address of the local
 * that holds it, not the record, so nothing is freed. */
static void FS98_STDCALL named_clear(void)
{
    uint32_t p = D32(0x10010e88);
    void *(*volatile launder)(void *) = 0;
    void (*volatile release)(void *) = free;

    (void)launder;
    while (p != 0) {
        D32(0x10010e88) = R32(p, 0xc);
        release(&p);
        p = D32(0x10010e88);
    }
}
