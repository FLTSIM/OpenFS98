/* WINDOW.DLL -- module id 10: the sim's view windows and the eyepoint.
 *
 * A window is a 0xa4-byte record on the host's heap, listed by pointer in a
 * host growable array at +0x110 and given a native window by host +0x64.
 * Type 3 is a 3D view, type 2 the map; each of those carries a 0xc0-byte view
 * block at +0x1c.  WINDOW creates and destroys them, keeps their numbers and
 * the host's "Views" menu entries, writes their titles and the status line
 * (position, heading, speed, the pause and crash messages), maximises them
 * over the panel, and saves and restores their layout with a situation
 * (CHAIN list 0x14: messages 0x21..0x23, and 0x60 for a new aircraft's
 * window geometry).
 *
 * The other half is the eyepoint: per frame, +0x1c paints every visible
 * window through its callback, which for a 3D view sets GLOBAL's current
 * window and view block (+0x1194, +0x1198), places the eye for the view's
 * mode -- cockpit, virtual cockpit, tower, track, spot plane, the map --
 * with the transitions between them, builds the view matrix at GLOBAL +0xce8, and drives MAIN,
 * G3D and HG2D through the frame, the stars included (scenery\stars.fs,
 * loaded at init).
 *
 * Retail is two programs, like DEMO's: MSVC C from .text+0x0 to +0x3490,
 * then 0x74 bytes of constants, then hand-written assembly from +0x3504
 * with ATC's stack-alignment wrapper around every call.  The assembly passes
 * values in registers, so it is transliterated with register variables, as
 * demo.c does, and the comments carry retail's addresses.  The whole
 * 0x8ac-byte .data image is kept at retail's offsets.  docs/modules/window.md
 * has the record and view block layouts.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define WIN_IMAGE_SIZE 0x8ac
#define WIN_IMPORTS    0x000
#define WIN_LINKAGE    0x068

typedef volatile uint8_t  win_v8;
typedef volatile uint16_t __attribute__((may_alias)) win_v16;
typedef volatile uint32_t __attribute__((may_alias)) win_v32;

struct window_image {
    uint8_t bytes[WIN_IMAGE_SIZE];
};

extern struct window_image window_image;

#define D       ((uint32_t)(uintptr_t)window_image.bytes)
#define M8(a)   (*(win_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(win_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(win_v32 *)(uintptr_t)(uint32_t)(a))

#define IMPORT(i)  M32(D + WIN_IMPORTS + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define G          IMPORT(1)
#define ACONTAIN   IMPORT(2)
#define CHAIN      IMPORT(3)
#define CONTROLS   IMPORT(4)
#define HG2D       IMPORT(5)
#define G3D        IMPORT(6)
#define MAIN       IMPORT(7)
#define NUMERIC    IMPORT(8)
#define PANELS     IMPORT(9)
#define REALITY    IMPORT(10)
#define UTIL       IMPORT(11)

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *f8)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_ACTIVE     HOST_(0x020, f0)    /* the sim is drawing */
#define H_WIN_CREATE HOST_(0x064, f1)    /* (rec): its native window */
#define H_WIN_DESTROY HOST_(0x068, f1)   /* (rec) */
#define H_PAINT_BEGIN HOST_(0x06c, f1)   /* (rec) */
#define H_PAINT_END  HOST_(0x070, f1)    /* (rec) */
#define H_WIN_UPDATE HOST_(0x07c, f1)    /* (rec): flags and geometry */
#define H_WIN_TITLE  HOST_(0x080, f1)    /* (rec): SetWindowText */
#define H_WIN_FLUSH  HOST_(0x084, f1)    /* (rec) */
#define H_TEXT       HOST_(0x08c, f5)    /* (rec, where, colour, text, opaque) */
#define H_RAISE      HOST_(0x090, f1)    /* (rec): to the front of its group */
#define H_NTH        HOST_(0x094, f3)    /* (&out, group, n): nth visible */
#define H_PROFILE_INT HOST_(0x0d0, f5)   /* (section, key, &inout, path, ext) */
#define H_CLOSE      HOST_(0x13c, f1)    /* (file) */
#define H_OPEN       HOST_(0x17c, f4)    /* (&file, path, ext, mode) */
#define H_READ       HOST_(0x19c, f4)    /* (file, buf, n, &got) */
#define H_ALLOC      HOST_(0x20c, f4)    /* (flags, size, &out, zero) */
#define H_FREE       HOST_(0x214, f1)    /* (&p) */
#define H_MEMMOVE    HOST_(0x21c, f3)    /* (dst, src, n) */
#define H_MENU_ADD   HOST_(0x244, f4)    /* (command, id, text, 0) */
#define H_MENU_REMOVE HOST_(0x25c, f1)   /* (id) */
#define H_MENU_TEXT  HOST_(0x264, f2)    /* (id, text) */
#define H_STRING     HOST_(0x2a4, f2)    /* (id, &out): a copy of string id */
#define H_STRCPY     HOST_(0x2a8, f2)
#define H_STRCAT     HOST_(0x2b0, f2)
#define H_STRDUP     HOST_(0x2b4, f1)
#define H_STRLEN     HOST_(0x2b8, f1)
#define H_STRCMP     HOST_(0x2bc, f2)
#define H_LIST_ADD   HOST_(0x2f8, f1)    /* (list): a new zeroed element */
#define H_LIST_FREE  HOST_(0x2fc, f1)
#define H_LIST_REMOVE HOST_(0x300, f2)   /* (list, elem) */
#define H_LIST_WALK  HOST_(0x308, f3)    /* (list, ctx, fn) */
#define H_LIST_FIND  HOST_(0x310, f5)    /* (list, offset, key, len, indirect) */

#define ACONTAIN_DONE CALL(ACONTAIN, 0x18, f1)
#define ACONTAIN_PATH CALL(ACONTAIN, 0x30, f4) /* (aircraft, what, out, size) */
#define CHAIN_ADD    CALL(CHAIN, 0x18, f2)   /* (list, callback) */
#define CHAIN_REMOVE CALL(CHAIN, 0x1c, f2)
#define CHAIN_SEND   CALL(CHAIN, 0x20, f3)   /* (list, a, b) */
#define CONTROLS_REDO CALL(CONTROLS, 0x24, f0)
#define CONTROLS_SET CALL(CONTROLS, 0x40, f2)
#define HG2D_LINE_H  CALL(HG2D, 0x2c, f4)
#define HG2D_LINE_V  CALL(HG2D, 0x30, f4)
#define HG2D_PRESENT CALL(HG2D, 0x3c, f5)
#define HG2D_BEGIN   CALL(HG2D, 0x9c, f1)
#define HG2D_END     CALL(HG2D, 0xa0, f1)
#define HG2D_HIT     CALL(HG2D, 0xbc, f2)
#define G3D_STARS    CALL(G3D, 0x1c, f1)
#define G3D_DRAW     CALL(G3D, 0x20, f1)
#define G3D_FRAME_A  CALL(G3D, 0x2c, f0)
#define G3D_FRAME_B  CALL(G3D, 0x30, f0)
#define G3D_LAYER    CALL(G3D, 0x34, f3)
#define G3D_NORMAL   CALL(G3D, 0x3c, f1)
#define G3D_BEGIN    CALL(G3D, 0x5c, f0)
#define MAIN_SAVE_OPEN CALL(MAIN, 0x5c, f2)  /* (tag, size) */
#define MAIN_SAVE_PUT CALL(MAIN, 0x60, f3)   /* (src, size, 0) */
#define MAIN_SAVE_CLOSE CALL(MAIN, 0x64, f0)
#define MAIN_SCENE   CALL(MAIN, 0x88, f0)
#define MAIN_LAYERS  CALL(MAIN, 0x98, f1)
#define MAIN_PREPARE CALL(MAIN, 0x9c, f0)
#define MAIN_AFTER   CALL(MAIN, 0xa8, f0)
#define MAIN_PAINT   CALL(MAIN, 0xc0, f1)
#define NUM_FORMAT   CALL(NUMERIC, 0x18, f3) /* (value, format, out) */
#define PANELS_SHOW  CALL(PANELS, 0x28, f0)
#define PANELS_HIDE  CALL(PANELS, 0x2c, f0)
#define REALITY_FRAME CALL(REALITY, 0x1c, f0)
#define UTIL_ALERT   CALL(UTIL, 0xa4, f8)

#define GLB8(o)  M8(G + (o))
#define GLB16(o) M16(G + (o))
#define GLB32(o) M32(G + (o))

/* ---- the state, at retail's offsets (D + offset) ------------------------- */

#define S_STARS_NAME (D + 0x0f8)    /* "scenery\stars.fs" */
#define S_WINDOWS    (D + 0x110)    /* host list of record pointers */
#define S_LAYOUT     (D + 0x130)    /* host list of 0x18-byte layout entries */
#define S_LAYOUT_FIELDS (D + 0x150) /* kind, offset, size; kind 0 ends */
#define S_KEY_SIZE_Y (D + 0x288)
#define S_KEY_SIZE_X (D + 0x290)
#define S_KEY_Y      (D + 0x298)
#define S_SECTION    (D + 0x29c)    /* "Default View" */
#define S_KEY_X      (D + 0x2ac)
#define S_STARS      (D + 0x39c)    /* the stars file, read whole */
#define S_STARS_ARG  (D + 0x3a0)
#define S_PAINT_WIN  (D + 0x3b4)
#define S_SKY_ARG    (D + 0x3bc)
#define S_NEXT_3D    (D + 0x3f0)    /* the next 3D view's number */
#define S_NEXT_MAP   (D + 0x3f4)
#define S_FPS        (D + 0x3f8)    /* word */
#define S_FPS_TICK   (D + 0x3fc)    /* word */
#define S_OLD_LAYOUT (D + 0x400)    /* three entries, for the old records */
#define S_MAXIMISED  (D + 0x40c)    /* how many windows cover the panel */
#define S_PAINTING   (D + 0x410)
#define S_GEOMETRY   (D + 0x418)    /* x, y, w, h from the aircraft */
#define S_EMPTY      (D + 0x428)    /* a 3D view's first title */
#define S_STRINGS    (D + 0x434)
#define S_TEXT       (D + 0x460)    /* 0x400 */

/* the strings +0x04 loads, by address */
#define STR(a)       M32(D + (a))

/* a window record */
#define R_TITLE   0x00
#define R_TYPE    0x04
#define R_FLAGS   0x08
#define R_ID      0x0c
#define R_PAINT   0x18
#define R_VIEW    0x1c
#define R_RECT    0x20
#define R_GROUP   0x78
#define R_SURFACE 0x80
#define R_WIDTH   0x84
#define R_HEIGHT  0x88

#define F_SHOWN      0x80000000u
#define F_NO_STATUS  0x40000000u
#define F_MAXIMISED  0x20000000u
#define F_PLAIN      0x00010000u

typedef void (FS98_STDCALL *paint_fn)(uint32_t, uint32_t, uint32_t, uint32_t);

static const uint8_t text_constants[0x804] = {
#include "window_text.inc"
};
#define T_MASKS (0x20904490u)       /* retail's addresses for these */
#define CONST_AT(a)  ((uint32_t)(uintptr_t)&text_constants[(a) - T_MASKS])

static uint32_t FS98_STDCALL list_windows(uint32_t out, uint32_t count,
                                          uint32_t type);
static void FS98_STDCALL show(uint32_t rec);
static void FS98_STDCALL hide(uint32_t rec);
static void FS98_STDCALL maximise(uint32_t rec);
static void FS98_STDCALL unmaximise(uint32_t rec);
static void FS98_STDCALL set_mode(uint32_t rec, uint32_t mode);
static void FS98_STDCALL activate(uint32_t rec);
static void FS98_STDCALL set_plain(uint32_t rec);
static void FS98_STDCALL clear_plain(uint32_t rec);
static void FS98_STDCALL paint_3d(uint32_t mode, uint32_t rec, uint32_t arg,
                                  uint32_t out);
static void FS98_STDCALL destroy_3d_all(void);
static uint32_t FS98_STDCALL destroy_record(uint32_t prec);
static void FS98_STDCALL set_orientation(uint32_t view, uint32_t angle);
static void FS98_STDCALL frame_3d(uint32_t rec);
static void FS98_STDCALL frame_map(uint32_t rec);

/* ---- the window list: .text+0x360..+0x86b ----------------------------------- */

/* .text+0x360 -- +0x78: a new record of `type` titled by string id `title`
 * (or a copy of the string, from 0x10000 up), its number and menu entry if
 * it is a view.  0, or -1 with *out null. */
static uint32_t FS98_STDCALL create_record(uint32_t out, uint32_t title,
                                           uint32_t type)
{
    uint32_t slot, rec, count;

    M32(out) = 0;
    slot = H_LIST_ADD(S_WINDOWS);
    if (slot == 0)
        return 0xffffffff;
    H_ALLOC(1, 0xa4, slot, 1);
    if (M32(slot) == 0)
        goto fail;
    if (title < 0x10000)
        H_STRING(title, M32(slot));
    else
        M32(M32(slot) + R_TITLE) = H_STRDUP(title);
    rec = M32(slot);
    M32(rec + R_TYPE) = type;
    if (type == 3 || type == 2) {
        H_ALLOC(1, 0xc0, M32(slot) + R_VIEW, 1);
        if (M32(M32(slot) + R_VIEW) == 0)
            goto fail;
        if (type == 3) {
            list_windows(0, (uint32_t)(uintptr_t)&count, type);
            if (count == 1)
                M32(S_NEXT_3D) = 1;
            rec = M32(slot);
            M32(rec + R_ID) = M32(S_NEXT_3D) + 0x100;
            /* The id goes in dx, over the record pointer's top half. */
            H_MENU_ADD(0x1005, (rec & 0xffff0000u) | M16(rec + R_ID),
                       M32(rec + R_TITLE), 0);
            M32(S_NEXT_3D)++;
        }
        if (type == 2) {
            list_windows(0, (uint32_t)(uintptr_t)&count, type);
            if (count == 1)
                M32(S_NEXT_MAP) = 1;
            rec = M32(slot);
            M32(rec + R_ID) = M32(S_NEXT_MAP) + 0x200;
            /* Here dx goes over the id itself, so the whole id. */
            H_MENU_ADD(0x1503, M32(rec + R_ID), M32(rec + R_TITLE), 0);
            M32(S_NEXT_MAP)++;
        }
    }
    M32(out) = M32(slot);
    return 0;

fail:
    H_FREE(slot);
    H_LIST_REMOVE(S_WINDOWS, slot);
    return 0xffffffff;
}

/* .text+0x500 -- +0x7c: take *prec out of the list and free it, its view
 * block, its title and its menu entry.  -1 if it is not listed. */
static uint32_t FS98_STDCALL destroy_record(uint32_t prec)
{
    uint32_t slot, rec;

    slot = H_LIST_FIND(S_WINDOWS, 0, prec, 4, 0);
    if (slot == 0)
        return 0xffffffff;
    H_LIST_REMOVE(S_WINDOWS, slot);
    if (M32(S_WINDOWS) == 0) {
        H_LIST_FREE(S_WINDOWS);
        M32(S_WINDOWS + 0x00) = 0;
        M32(S_WINDOWS + 0x04) = 0;
        M32(S_WINDOWS + 0x08) = 0x20;
        M32(S_WINDOWS + 0x0c) = 4;
        M32(S_WINDOWS + 0x10) = 0;
        M32(S_WINDOWS + 0x14) = 0;
        M32(S_WINDOWS + 0x18) = 0;
    }
    rec = M32(prec);
    if (M32(rec + R_TYPE) == 3 || M32(rec + R_TYPE) == 2) {
        H_MENU_REMOVE((rec & 0xffff0000u) | M16(rec + R_ID));
        H_FREE(M32(prec) + R_VIEW);
    }
    H_FREE(M32(prec) + R_TITLE);
    H_FREE(prec);
    return 0;
}

struct gather {
    uint32_t list;
    uint32_t count;
    uint32_t type;
};

/* .text+0x6a0 -- the walk's callback: a record of the wanted type (any, for
 * type 0) goes on the list. */
static uint32_t FS98_STDCALL gather_one(uint32_t elem, uint32_t ctx)
{
    struct gather *g = (struct gather *)(uintptr_t)ctx;
    uint32_t rec;

    if (g->type == 0) {
        M32(g->list + 4 * g->count) = M32(elem);
        g->count++;
        return 0;
    }
    rec = M32(elem);
    if (M32(rec + R_TYPE) == g->type) {
        M32(g->list + 4 * g->count) = rec;
        g->count++;
    }
    return 0;
}

/* .text+0x5e0 -- +0x80: the records of `type` (0 for all) in list order, as
 * a host allocation the caller frees through *out, or freed here if out is
 * null; the count through `count`.  -1 if the allocation fails. */
static uint32_t FS98_STDCALL list_windows(uint32_t out, uint32_t count,
                                          uint32_t type)
{
    struct gather g;

    g.list = 0;
    g.count = 0;
    g.type = type;
    if (out != 0)
        M32(out) = 0;
    M32(count) = 0;
    H_ALLOC(1, M32(S_WINDOWS) * 4, (uint32_t)(uintptr_t)&g.list, 1);
    if (g.list == 0)
        return 0xffffffff;
    H_LIST_WALK(S_WINDOWS, (uint32_t)(uintptr_t)&g,
                (uint32_t)(uintptr_t)gather_one);
    if (out != 0) {
        M32(out) = g.list;
        M32(count) = g.count;
        return 0;
    }
    H_FREE((uint32_t)(uintptr_t)&g.list);
    M32(count) = g.count;
    return 0;
}

/* .text+0x6f0 -- +0x84: the record of `type` after `rec`, wrapping; null
 * and -1 if rec is not one.  The list goes in the type argument's slot. */
static uint32_t FS98_STDCALL next_window(uint32_t out, uint32_t rec,
                                         uint32_t type)
{
    uint32_t count, i, list;

    list_windows((uint32_t)(uintptr_t)&type, (uint32_t)(uintptr_t)&count,
                 type);
    list = type;
    for (i = 0; i < count; i++)
        if (M32(list + 4 * i) == rec)
            break;
    if (i >= count) {
        M32(out) = 0;
        H_FREE((uint32_t)(uintptr_t)&type);
        return 0xffffffff;
    }
    i++;
    if (i >= count)
        i = 0;
    M32(out) = M32(list + 4 * i);
    H_FREE((uint32_t)(uintptr_t)&type);
    return 0;
}

/* .text+0x780 -- +0x88: the one before, wrapping. */
static uint32_t FS98_STDCALL previous_window(uint32_t out, uint32_t rec,
                                             uint32_t type)
{
    uint32_t count, i, list;

    list_windows((uint32_t)(uintptr_t)&type, (uint32_t)(uintptr_t)&count,
                 type);
    list = type;
    for (i = 0; i < count; i++)
        if (M32(list + 4 * i) == rec)
            break;
    if (i >= count) {
        M32(out) = 0;
        H_FREE((uint32_t)(uintptr_t)&type);
        return 0xffffffff;
    }
    if (i == 0)
        i = count;
    M32(out) = M32(list + 4 * (i - 1));
    H_FREE((uint32_t)(uintptr_t)&type);
    return 0;
}

/* .text+0x810 -- +0x8c: the record of `type` numbered `id`, or null.  The
 * count is compared signed. */
static uint32_t FS98_STDCALL find_window(uint32_t id, uint32_t type)
{
    uint32_t count, found = 0, list;
    int32_t i;

    list_windows((uint32_t)(uintptr_t)&type, (uint32_t)(uintptr_t)&count,
                 type);
    list = type;
    for (i = 0; i < (int32_t)count; i++)
        if (M32(M32(list + 4 * i) + R_ID) == id) {
            found = M32(list + 4 * i);
            break;
        }
    H_FREE((uint32_t)(uintptr_t)&type);
    return found;
}

/* ---- showing and hiding: .text+0x250..+0x2b4 ---------------------------------- */

/* .text+0x250 -- +0x6c */
static void FS98_STDCALL hide(uint32_t rec)
{
    if (rec != 0) {
        M32(rec + R_FLAGS) &= 0x7fffffffu;
        H_WIN_UPDATE(rec);
    }
}

/* .text+0x270 -- +0x68 */
static void FS98_STDCALL show(uint32_t rec)
{
    if (rec != 0) {
        M32(rec + R_FLAGS) |= F_SHOWN;
        H_WIN_UPDATE(rec);
    }
}

/* .text+0x290 -- +0x64 */
static void FS98_STDCALL toggle_shown(uint32_t rec)
{
    if (rec != 0) {
        if (M32(rec + R_FLAGS) & F_SHOWN)
            hide(rec);
        else
            show(rec);
    }
}

/* ---- the strings: .text+0x870..+0xcf4 ------------------------------------------ */

static const uint16_t string_ids[] = {
    0x9200, 0x9201, 0x9202, 0x9203, 0x9204, 0x9205, 0x9206, 0x9207, 0x9208,
    0x9209, 0x9220, 0x9221, 0x9222, 0x9240, 0x9241, 0x9242, 0x9243, 0x9245,
    0x9244, 0x9246, 0x9260, 0x9261, 0x9262, 0x9263, 0x9264, 0x9265, 0x9266,
    0x9267, 0x9268, 0x9269
};
static const uint16_t string_slots[] = {
    0x45c, 0x890, 0x458, 0x438, 0x8a4, 0x894, 0x8a8, 0x448, 0x870, 0x43c,
    0x87c, 0x8a0, 0x454, 0x860, 0x884, 0x44c, 0x864, 0x874, 0x868, 0x888,
    0x89c, 0x450, 0x878, 0x880, 0x444, 0x898, 0x88c, 0x86c, 0x440, 0x434
};

/* .text+0x870 -- each string as a host copy, in this order. */
static void load_strings(void)
{
    uint32_t i;

    for (i = 0; i < sizeof string_ids / sizeof string_ids[0]; i++)
        H_STRING(string_ids[i], D + string_slots[i]);
}

/* .text+0xb00 */
static void free_strings(void)
{
    uint32_t i;

    for (i = 0; i < sizeof string_slots / sizeof string_slots[0]; i++)
        H_FREE(D + string_slots[i]);
}

/* ---- the status line: .text+0xd00..+0x14d4 -------------------------------------- */

/* .text+0x1140 -- NORTH, EAST, ALTIT in metres or feet, and the airspeed
 * (the slew's own, or knots), into the text buffer. */
static void position_text(void)
{
    uint32_t value;

    NUM_FORMAT(G + 0x560, 0x15, S_TEXT);
    H_STRCAT(S_TEXT, STR(0x440));
    NUM_FORMAT(G + 0x568, 0x14, S_TEXT + H_STRLEN(S_TEXT));
    H_STRCAT(S_TEXT, STR(0x440));
    H_STRCAT(S_TEXT, STR(0x878));
    NUM_FORMAT(G + 0x574, 0x38, S_TEXT + H_STRLEN(S_TEXT));
    if (GLB16(0xc18) == 2)
        H_STRCAT(S_TEXT, STR(0x880));
    else
        H_STRCAT(S_TEXT, STR(0x444));
    if (GLB16(0x5dc) != 0) {
        H_STRCAT(S_TEXT, STR(0x440));
        NUM_FORMAT(G + 0x5f8, 0x37, S_TEXT + H_STRLEN(S_TEXT));
    } else {
        H_STRCAT(S_TEXT, STR(0x440));
        if (GLB16(0x612) == 0)
            value = GLB32(0x2b8) >> 7;
        else
            value = GLB32(0x2bc) >> 7;
        NUM_FORMAT((uint32_t)(uintptr_t)&value, 0x22,
                   S_TEXT + H_STRLEN(S_TEXT));
    }
    H_STRCAT(S_TEXT, STR(0x898));
}

/* .text+0x1310 -- FRAMES/SEC, worked out every ninth call from the frame
 * time, and the g load: GLOBAL +0x11ba over 39, rounded away from zero
 * and clamped to 158. */
static void rate_text(void)
{
    uint16_t tick = M16(S_FPS_TICK);
    int32_t n, q;
    int64_t p;
    uint32_t local[2];

    M16(S_FPS_TICK) = (uint16_t)(tick + 1);
    if (tick >= 8) {
        M16(S_FPS_TICK) = 0;
        if (GLB16(0x2aa) != 0)
            M16(S_FPS) = (uint16_t)(0x7fff8 / (int32_t)(int16_t)GLB16(0x2aa));
    }
    H_STRCPY(S_TEXT, STR(0x88c));
    NUM_FORMAT(S_FPS, 0x29, S_TEXT + H_STRLEN(S_TEXT));
    if (GLB16(0x5dc) != 0)
        return;
    n = (int16_t)GLB16(0x11ba);
    if (n >= 0)
        n += 0x20;
    else
        n -= 0x20;
    /* imul by 0xd20d20d3, add, sar 5, add the sign: n / 39 */
    p = (int64_t)n * (int32_t)0xd20d20d3u;
    q = (int32_t)(p >> 32) + n;
    q >>= 5;
    q += (int32_t)((uint32_t)q >> 31);
    local[1] = (uint32_t)(int32_t)(int16_t)q;
    local[0] = (uint32_t)q;
    if ((long double)(int32_t)local[1] > 158.4L) {
        q = 0x9e;
        local[0] = 0x9e;
    }
    local[1] = (uint32_t)(int32_t)(int16_t)q;
    if ((long double)(int32_t)local[1] < -158.4L)
        local[0] = 0xffffff62u;
    H_STRCAT(S_TEXT, STR(0x440));
    NUM_FORMAT((uint32_t)(uintptr_t)&local[0], 0x41,
               S_TEXT + H_STRLEN(S_TEXT));
    H_STRCAT(S_TEXT, STR(0x86c));
}

/* .text+0x1490 -- "Replay: " and its time. */
static void pause_text(void)
{
    H_STRCPY(S_TEXT, STR(0x8a4));
    NUM_FORMAT(G + 0x62c, 0x20, S_TEXT + H_STRLEN(S_TEXT));
}

/* .text+0xd00 -- the status line: position (3), rate (4), or both, by
 * GLOBAL's choice for the flight or the slew; lower down when the window
 * shows its title. */
static void FS98_STDCALL status_line(uint32_t rec)
{
    uint32_t first = 3, second = 4, choice;

    if (GLB16(0x552) != 0 && !(M32(rec + R_FLAGS) & F_NO_STATUS)) {
        first = 4;
        second = 5;
    }
    if (GLB16(0x5dc) != 0)
        choice = GLB32(0x5f4);
    else
        choice = GLB32(0x5fc);
    switch (choice + 1) {
    case 2:
        position_text();
        H_TEXT(rec, first, 0x6a, S_TEXT, 0);
        break;
    case 3:
        rate_text();
        H_TEXT(rec, first, 0x6a, S_TEXT, 0);
        break;
    case 4:
        position_text();
        H_TEXT(rec, first, 0x6a, S_TEXT, 0);
        rate_text();
        H_TEXT(rec, second, 0x6a, S_TEXT, 0);
        break;
    }
}

/* .text+0xdf0 -- the messages in the middle of the view (9): paused, the
 * parking brakes or the brakes (while there is a message line, +0x79c),
 * a video playing, a replay and its time, the recording rate; and at the
 * bottom (6) STALL, OVERSPEED or SLEW. */
static void FS98_STDCALL message_line(uint32_t rec)
{
    uint32_t g = G, text;
    uint16_t crashed;

    if (M32(g + 0x268) != 0 && (M8(g + 0x258) & 1)) {
        text = STR(0x45c);
        goto centre;
    }
    crashed = M16(g + 0xbc8);
    if (crashed != 0 && ((M8(g + 0x258) & 2) || M32(g + 0x760) == 0)) {
        if (M32(g + 0x79c) == 0)
            goto bottom;
        text = STR(0x458);
        goto centre;
    }
    if (M16(g + 0xbca) != 0 && crashed == 0) {
        if (M32(g + 0x79c) == 0)
            goto bottom;
        text = STR(0x890);
        goto centre;
    }
    if (M32(g + 0x758) != 0) {
        text = STR(0x438);
        goto centre;
    }
    if (M32(g + 0x628) != 0) {
        pause_text();
        text = S_TEXT;
        goto centre;
    }
    if (M32(g + 0x760) == 0)
        goto bottom;
    switch (M32(g + 0x75c)) {
    case 0: text = STR(0x894); break;
    case 1: text = STR(0x8a8); break;
    case 2: text = STR(0x448); break;
    case 3: text = STR(0x870); break;
    case 4: text = STR(0x43c); break;
    default: goto bottom;
    }
centre:
    H_TEXT(rec, 9, 0x6a, text, 1);
    g = G;
bottom:
    if (M8(g + 0x36c) != 0) {
        H_TEXT(rec, 6, 0x6a, STR(0x87c), 1);
        return;
    }
    if (M8(g + 0x36d) != 0) {
        H_TEXT(rec, 6, 0x6a, STR(0x8a0), 1);
        return;
    }
    if (M16(g + 0x5dc) != 0 && (M8(g + 0x258) & 1))
        H_TEXT(rec, 6, 0x6a, STR(0x454), 1);
}

/* .text+0xfc0 -- the title: the view's kind and number.  A view block whose
 * +0x10 is neither 0 nor 1, or whose mode is past 4, leaves retail's buffer
 * as the stack had it; it starts empty here. */
static void FS98_STDCALL update_title(uint32_t rec)
{
    char buf[0x104];
    uint32_t view, number, kind = 0;

    buf[0] = 0;
    if (M32(rec + R_TITLE) == 0)
        return;
    view = M32(rec + R_VIEW);
    if (M32(view + 0x10) == 0) {
        switch (M32(view + 0x14)) {
        case 0: kind = STR(0x884); break;
        case 1: kind = STR(0x44c); break;
        case 2: kind = STR(0x864); break;
        case 3: kind = STR(0x874); break;
        case 4: kind = STR(0x868); break;
        }
        if (kind != 0)
            H_STRCPY((uint32_t)(uintptr_t)buf, kind);
    } else if (M32(view + 0x10) == 1) {
        H_STRCPY((uint32_t)(uintptr_t)buf, STR(0x888));
    }
    number = M32(rec + R_ID) - 0x100;
    H_STRCAT((uint32_t)(uintptr_t)buf, STR(0x434));
    H_STRCAT((uint32_t)(uintptr_t)buf, STR(0x860));
    NUM_FORMAT((uint32_t)(uintptr_t)&number, 0x20,
               (uint32_t)(uintptr_t)buf + H_STRLEN((uint32_t)(uintptr_t)buf));
    if (H_STRCMP(M32(rec + R_TITLE), (uint32_t)(uintptr_t)buf) != 0) {
        uint32_t title;

        H_FREE(rec + R_TITLE);
        title = H_STRDUP((uint32_t)(uintptr_t)buf);
        M32(rec + R_TITLE) = title;
        H_MENU_TEXT((title & 0xffff0000u) | M16(rec + R_ID), title);
        H_WIN_TITLE(rec);
    }
    if (GLB16(0x552) != 0 && !(M32(rec + R_FLAGS) & F_NO_STATUS))
        H_TEXT(rec, 3, 0x6a, M32(rec + R_TITLE), 0);
}

/* ---- covering the panel, and the view's mode: .text+0x16f0..+0x194c ----------- */

/* .text+0x17c0 -- +0x4c: maximise over the panel, which goes when the first
 * window covers it.  Not for a window that is maximised already or shows no
 * status line. */
static void FS98_STDCALL maximise(uint32_t rec)
{
    if (rec != 0 && !(M32(rec + R_FLAGS) & (F_NO_STATUS | F_MAXIMISED))) {
        if (M32(S_MAXIMISED) == 0)
            PANELS_HIDE();
        M32(S_MAXIMISED)++;
        M32(rec + R_FLAGS) |= F_MAXIMISED;
        H_WIN_UPDATE(rec);
    }
}

/* .text+0x1810 -- +0x50: the panel comes back with the last. */
static void FS98_STDCALL unmaximise(uint32_t rec)
{
    if (rec != 0 && (M32(rec + R_FLAGS) & F_MAXIMISED)) {
        M32(S_MAXIMISED)--;
        M32(rec + R_FLAGS) &= ~F_MAXIMISED;
        H_WIN_UPDATE(rec);
        if (M32(S_MAXIMISED) == 0)
            PANELS_SHOW();
    }
}

/* .text+0x1860 -- +0x54 */
static void FS98_STDCALL toggle_maximised(uint32_t rec)
{
    if (rec != 0) {
        if (M32(rec + R_FLAGS) & F_MAXIMISED)
            unmaximise(rec);
        else
            maximise(rec);
    }
}

/* .text+0x17a0 -- +0x5c: the view's kind, 0 a 3D view and 1 the map. */
static void FS98_STDCALL set_kind(uint32_t rec, uint32_t kind)
{
    if (rec != 0 && M32(rec + R_VIEW) != 0)
        M32(M32(rec + R_VIEW) + 0x10) = kind;
}

/* .text+0x1890 -- +0x58: the 3D view's mode (0 cockpit, 1 virtual
 * cockpit, 2 tower, 3 track, 4 spot), and the controls told.  In full-screen
 * cockpit mode, or for the active window when its view asks, a cockpit view
 * with no panel position uncovers the panel and the rest cover it. */
static void FS98_STDCALL set_mode(uint32_t rec, uint32_t mode)
{
    uint32_t view;

    if (rec == 0 || M32(rec + R_VIEW) == 0)
        return;
    M32(M32(rec + R_VIEW) + 0x14) = mode;
    CONTROLS_SET(0x11, mode);
    CONTROLS_REDO();
    if (GLB32(0x54c) != 1) {
        view = M32(rec + R_VIEW);
        if (M32(view + 0xbc) == 0 || rec != GLB32(0x119c))
            return;
    }
    if (mode == 0 && M32(M32(rec + R_VIEW) + 0x18) == 0) {
        unmaximise(rec);
        return;
    }
    maximise(rec);
}

/* .text+0x1910 -- +0x70 */
static void FS98_STDCALL set_plain(uint32_t rec)
{
    if (rec != 0) {
        M32(rec + R_FLAGS) |= F_PLAIN;
        H_WIN_UPDATE(rec);
    }
}

/* .text+0x1930 -- +0x74 */
static void FS98_STDCALL clear_plain(uint32_t rec)
{
    if (rec != 0) {
        M32(rec + R_FLAGS) &= ~F_PLAIN;
        H_WIN_UPDATE(rec);
    }
}

/* .text+0x1750 -- +0x48: the window the controls and the eyepoint follow. */
static void FS98_STDCALL activate(uint32_t rec)
{
    if (rec != 0) {
        show(rec);
        GLB32(0x119c) = rec;
        GLB32(0x11a0) = M32(rec + R_VIEW);
        GLB8(0x288) = 2;
        CONTROLS_REDO();
    }
}

/* ---- 3D views: .text+0x14e0..+0x1748 -------------------------------------------- */

/* .text+0x1b20 -- a view block's defaults, its kind and its mode. */
static void FS98_STDCALL view_defaults(uint32_t rec, uint32_t kind,
                                       uint32_t mode)
{
    uint32_t view = M32(rec + R_VIEW), v;

    M16(view + 0x1c) = 0x40;
    M16(view + 0x1e) = 0x20;
    M16(view + 0x24) = 0x79;
    M16(view + 0x20) = 0x200;
    M16(view + 0x22) = 0x100;
    M16(view + 0x26) = 0x2c0;
    M32(view + 0x10) = kind;
    set_mode(rec, mode);
    v = M32(rec + R_VIEW);
    set_orientation(v, M32(v + 0x18));
    M16(view + 0x28) = 0x4000;
    M32(view + 0x2c) = 0x1800;
    M32(view + 0x34) = 0x100;
    M8(view + 0x30) = 0;
    M8(view + 0x31) = 0;
}

/* .text+0x14e0 -- +0x3c: a new 3D view, cascaded from the last, or the
 * aircraft's own geometry when it is the first and the aircraft has one.
 * Past 32 an alert, and null. */
static uint32_t FS98_STDCALL create_3d(uint32_t kind, uint32_t mode)
{
    uint32_t rec, count;

    list_windows(0, (uint32_t)(uintptr_t)&count, 3);
    if (count >= 0x20) {
        UTIL_ALERT(1, 2, 0xf580, 0, 0, 0, 0, 0);
        return 0;
    }
    create_record((uint32_t)(uintptr_t)&rec, S_EMPTY, 3);
    if (count == 0 && M32(S_GEOMETRY + 8) != 0 && M32(S_GEOMETRY + 12) != 0) {
        M32(rec + 0x20) = M32(S_GEOMETRY + 0);
        M32(rec + 0x24) = M32(S_GEOMETRY + 4);
        M32(rec + 0x28) = M32(S_GEOMETRY + 8);
        M32(rec + 0x2c) = M32(S_GEOMETRY + 12);
    } else {
        M32(rec + 0x20) = (M32(rec + R_ID) - 0x101) << 7;
        M32(rec + 0x24) = (M32(rec + R_ID) - 0x101) * 3 << 5;
        M32(rec + 0x28) = 0xaaa;
        M32(rec + 0x2c) = 0x800;
    }
    M32(rec + 0x68) = 0x32;
    M32(rec + 0x6c) = 0x1e;
    M32(rec + R_PAINT) = (uint32_t)(uintptr_t)paint_3d;
    M32(rec + R_GROUP) = 0x2000;
    M32(rec + 0x7c) = 1;
    view_defaults(rec, kind, mode);
    H_WIN_CREATE(rec);
    if (GLB32(0x1238) != 0)
        set_plain(rec);
    else
        clear_plain(rec);
    activate(rec);
    return rec;
}

/* .text+0x1670 -- +0x40: the front window of the group becomes the active
 * one. */
static void FS98_STDCALL destroy_window(uint32_t rec)
{
    uint32_t g;

    if (rec == 0)
        return;
    hide(rec);
    unmaximise(rec);
    H_WIN_DESTROY(rec);
    destroy_record((uint32_t)(uintptr_t)&rec);
    H_NTH(G + 0x119c, 0x2000, 0);
    g = G;
    if (M32(g + 0x119c) != 0) {
        M32(g + 0x11a0) = M32(M32(g + 0x119c) + R_VIEW);
        CONTROLS_REDO();
        return;
    }
    M32(g + 0x11a0) = 0;
}

/* .text+0x1700 -- +0x44 */
static void FS98_STDCALL destroy_3d_all(void)
{
    uint32_t list, count, i;

    list_windows((uint32_t)(uintptr_t)&list, (uint32_t)(uintptr_t)&count, 3);
    for (i = 0; i < count; i++)
        destroy_window(M32(list + 4 * i));
    H_FREE((uint32_t)(uintptr_t)&list);
}

/* .text+0x110 -- +0x18: a jump to +0x44. */
static void FS98_STDCALL destroy_3d_all_jump(void)
{
    destroy_3d_all();
}

/* .text+0x1b90 -- the view's half-extents from the window's size: even
 * widths are one short on the left, and two pixels or less leave the
 * horizontal pair as it was. */
static void FS98_STDCALL view_extent(uint32_t rec)
{
    uint32_t view = M32(rec + R_VIEW);
    int32_t w = (int32_t)M32(rec + R_WIDTH), h;

    if (w & 1) {
        M32(view + 0) = (uint32_t)(w / 2);
        M32(view + 4) = (uint32_t)((int32_t)M32(rec + R_WIDTH) / 2);
    } else if (w > 2) {
        M32(view + 0) = (uint32_t)(w / 2 - 1);
        M32(view + 4) = (uint32_t)((int32_t)M32(rec + R_WIDTH) / 2);
    } else {
        M32(view + 8) = 0;
        M32(view + 0xc) = 0;
    }
    h = (int32_t)M32(rec + R_HEIGHT);
    if (h > 2) {
        M32(view + 8) = (uint32_t)(1 - h / 2);
        M32(view + 0xc) = (uint32_t)((int32_t)M32(rec + R_HEIGHT) / 2);
        return;
    }
    M32(view + 8) = 0;
    M32(view + 0xc) = 0;
}

/* .text+0x1a60 -- a 3D frame, with GLOBAL +0x832 (what the frame wants
 * redrawn) cleared for it.  What the frame asked for stays for the next
 * unless it is nothing, or one of the screen-sized redraws while the view
 * is the cockpit's with no transition running. */
static void FS98_STDCALL render_3d(uint32_t rec)
{
    uint16_t keep = GLB16(0x832), asked;
    uint32_t view;

    GLB16(0x832) = 0;
    frame_3d(rec);
    asked = GLB16(0x832);
    GLB16(0x832) = keep;
    switch (asked) {
    case 0:
        return;
    case 2: case 6: case 8: case 10: case 12: case 20:
        view = M32(rec + R_VIEW);
        if (view != 0 && M32(view + 0x14) == 0 && M16(view + 0x56) == 0)
            GLB16(0x832) = asked;
        return;
    default:
        GLB16(0x832) = asked;
        return;
    }
}

/* .text+0x1950 -- a 3D view's paint callback.  Mode 0 paints, once at a
 * time; mode 5 hit-tests through HG2D. */
static void FS98_STDCALL paint_3d(uint32_t mode, uint32_t rec, uint32_t arg,
                                  uint32_t out)
{
    uint32_t view, r;

    if (rec == 0 || M32(rec + R_VIEW) == 0)
        return;
    if (mode != 0) {
        if (mode == 5) {
            r = HG2D_HIT(rec, arg);
            if (out != 0)
                M32(out) = r;
        }
        return;
    }
    if (M32(S_PAINTING) != 0)
        return;
    M32(S_PAINTING) = 1;
    view_extent(rec);
    view = M32(rec + R_VIEW);
    if (M32(view + 0x10) == 1) {
        GLB16(0x2b2) = M16(view + 0x26);
        HG2D_BEGIN(rec);
        frame_map(rec);
        HG2D_END(rec);
    } else {
        GLB16(0x1172) = GLB16(0x116e);
        HG2D_BEGIN(rec);
        render_3d(rec);
        MAIN_PAINT(rec);
        HG2D_END(rec);
    }
    update_title(rec);
    if (rec == GLB32(0x119c)) {
        status_line(rec);
        message_line(rec);
    }
    M32(S_PAINTING) = 0;
}

/* .text+0x120 -- +0x1c: paint every window that shows, the callbacks between
 * CHAIN's frame messages (0x1b, then 0x17..0x1a for each, then 0x1c). */
static void FS98_STDCALL paint_all(void)
{
    uint32_t list, count, i, rec;

    G3D_BEGIN();
    if (H_ACTIVE() == 0)
        return;
    CHAIN_SEND(0x1b, 0, 0);
    list_windows((uint32_t)(uintptr_t)&list, (uint32_t)(uintptr_t)&count, 0);
    for (i = 0; i < count; i++) {
        rec = M32(list + 4 * i);
        if (!(M32(rec + R_FLAGS) & F_SHOWN))
            continue;
        if (M32(rec + R_PAINT) != 0) {
            H_PAINT_BEGIN(rec);
            CHAIN_SEND(0x17, 0, M32(list + 4 * i));
            rec = M32(list + 4 * i);
            ((paint_fn)(uintptr_t)M32(rec + R_PAINT))(0, rec, 0, 0);
            CHAIN_SEND(0x18, 0, M32(list + 4 * i));
            H_PAINT_END(M32(list + 4 * i));
        }
        CHAIN_SEND(0x19, 0, M32(list + 4 * i));
        H_WIN_FLUSH(M32(list + 4 * i));
        CHAIN_SEND(0x1a, 0, M32(list + 4 * i));
    }
    H_FREE((uint32_t)(uintptr_t)&list);
    CHAIN_SEND(0x1c, 0, 0);
}

/* ---- the layout in a situation: .text+0x1c10..+0x3468 ------------------------------ */

/* A layout entry, 0x18 bytes in the host list at +0x130: */
#define L_Z       0x00      /* place from the front, 0x100 if not shown */
#define L_ACTIVE  0x04
#define L_REC     0x08      /* the record and view as saved */
#define L_VIEW    0x0c
#define L_NEW     0x10      /* and as restored */
#define L_NEW_VIEW 0x14

/* .text+0x3130 -- an old record's mode, from before the virtual cockpit. */
static void FS98_STDCALL old_mode(uint32_t p)
{
    switch (M32(p)) {
    case 1: M32(p) = 2; break;
    case 2: M32(p) = 3; break;
    case 3: M32(p) = 4; break;
    }
}

/* .text+0x3100 -- the bytes the field list describes. */
static uint32_t FS98_STDCALL fields_size(uint32_t fields)
{
    uint32_t n = 0;

    for (; M32(fields) != 0; fields += 0xc)
        n += M32(fields + 8);
    return n;
}

/* .text+0x3070 -- unpack `src` into an entry's record (1), view (2), the
 * entry itself (3) or GLOBAL (4); other kinds are skipped. */
static void FS98_STDCALL load_fields(uint32_t src, uint32_t fields,
                                     uint32_t entry)
{
    uint32_t base;

    for (; M32(fields) != 0; fields += 0xc) {
        switch (M32(fields)) {
        case 1: base = M32(entry + L_REC); break;
        case 2: base = M32(entry + L_VIEW); break;
        case 3: base = entry; break;
        case 4: base = G; break;
        default: base = 0; break;
        }
        if (M32(fields) >= 1 && M32(fields) <= 4)
            H_MEMMOVE(base + M32(fields + 4), src, M32(fields + 8));
        src += M32(fields + 8);
    }
}

/* .text+0x2fd0 -- and pack them through MAIN's situation writer. */
static void FS98_STDCALL save_fields(uint32_t tag, uint32_t fields,
                                     uint32_t entry)
{
    uint32_t base;

    MAIN_SAVE_OPEN(tag, fields_size(fields));
    for (; M32(fields) != 0; fields += 0xc) {
        switch (M32(fields)) {
        case 1: base = M32(entry + L_REC); break;
        case 2: base = M32(entry + L_VIEW); break;
        case 3: base = entry; break;
        case 4: base = G; break;
        default: continue;
        }
        MAIN_SAVE_PUT(base + M32(fields + 4), M32(fields + 8), 0);
    }
    MAIN_SAVE_CLOSE();
}

/* .text+0x3170 -- a window's place from the front of the 3D views, by asking
 * the host for each in turn; 0x100 if it is not there.  The answer goes in
 * the argument's own slot. */
static uint32_t FS98_STDCALL z_index(uint32_t rec)
{
    uint32_t want = rec, i = 0;

    for (;;) {
        H_NTH((uint32_t)(uintptr_t)&rec, 0x2000, i);
        if (rec == 0)
            i = 0xff;
        if (rec == want)
            break;
        i++;
        if (rec == want || rec == 0)
            break;
    }
    return i;
}

/* .text+0x31b0 -- raise the restored windows back to front, then make the
 * first that was active the active one again. */
static void FS98_STDCALL restore_order(uint32_t entries, int32_t n)
{
    uint32_t deepest = 0, z, k;
    int32_t i;

    for (i = 0; i < n; i++) {
        z = M32(entries + 0x18 * i + L_Z);
        if (z < 0xff && z > deepest)
            deepest = z;
    }
    for (k = 1; k <= deepest; k++)
        for (i = 0; i < n; i++)
            if (M32(entries + 0x18 * i + L_Z) == deepest - k)
                H_RAISE(M32(entries + 0x18 * i + L_NEW));
    for (i = 0; i < n; i++)
        if (M32(entries + 0x18 * i + L_ACTIVE) != 0) {
            GLB32(0x119c) = M32(entries + 0x18 * i + L_NEW);
            GLB32(0x11a0) = M32(entries + 0x18 * i + L_NEW_VIEW);
            return;
        }
}

/* .text+0x2c40 -- message 0x23: save every 3D view as a 0x3de record. */
static void FS98_STDCALL save_layout(void)
{
    uint32_t list, count, i, entry[6];

    list_windows((uint32_t)(uintptr_t)&list, (uint32_t)(uintptr_t)&count, 3);
    for (i = 0; i < count; i++) {
        entry[2] = M32(list + 4 * i);
        entry[3] = M32(entry[2] + R_VIEW);
        entry[0] = z_index(entry[2]);
        entry[1] = entry[2] == GLB32(0x119c);
        save_fields(0x3de, S_LAYOUT_FIELDS, (uint32_t)(uintptr_t)entry);
    }
    H_FREE((uint32_t)(uintptr_t)&list);
}

/* A new layout entry with a blank record and view block. */
static uint32_t new_entry(uint32_t kind)
{
    uint32_t e = H_LIST_ADD(S_LAYOUT);

    H_ALLOC(1, 0xa4, e + L_REC, 1);
    H_ALLOC(1, 0xc0, e + L_VIEW, 1);
    M32(M32(e + L_REC) + R_VIEW) = M32(e + L_VIEW);
    view_defaults(M32(e + L_REC), kind, 0);
    M32(e + L_Z) = 0;
    M32(e + L_ACTIVE) = 0;
    return e;
}

/* A field of an old record. */
#define TAKE(dst, at, n) H_MEMMOVE((dst), src + (at), (n))

/* The view fields records 5 and 8 carry. */
static void old_view(uint32_t v, uint32_t src)
{
    TAKE(v + 0x14, 0, 1);
    old_mode(v + 0x14);
    TAKE(v + 0x18, 1, 1);
    TAKE(v + 0x1c, 2, 2);
    TAKE(v + 0x20, 4, 2);
    TAKE(v + 0x22, 6, 2);
    TAKE(v + 0x24, 8, 2);
    TAKE(v + 0x28, 0xa, 2);
    TAKE(v + 0x2c, 0xc, 4);
    TAKE(v + 0x30, 0x10, 1);
    TAKE(v + 0x31, 0x11, 1);
    TAKE(v + 0x34, 0x12, 4);
    TAKE(v + 0x38, 0x16, 4);
}

/* And the eyepoint's, records 0x66 and 0x67. */
static void old_eye(uint32_t v, uint32_t src)
{
    static const uint8_t at[27][2] = {
        { 0x56, 2 }, { 0x5a, 2 }, { 0x5c, 4 }, { 0x62, 2 }, { 0x64, 4 },
        { 0x6a, 2 }, { 0x6c, 4 }, { 0x70, 4 }, { 0x74, 4 }, { 0x78, 4 },
        { 0x7e, 2 }, { 0x80, 4 }, { 0x86, 2 }, { 0x88, 4 }, { 0x8e, 2 },
        { 0x90, 4 }, { 0x96, 2 }, { 0x98, 4 }, { 0x9e, 2 }, { 0xa0, 4 },
        { 0xa6, 2 }, { 0xa8, 4 }, { 0xac, 4 }, { 0xb0, 4 }, { 0xb4, 4 },
        { 0xb8, 2 }, { 0xba, 2 }
    };
    uint32_t i, n = 0;

    for (i = 0; i < 27; i++) {
        TAKE(v + at[i][0], n, at[i][1]);
        n += at[i][1];
    }
}

/* The rectangle records 4, 7 and 10 carry: four words into the low halves. */
static void old_rect(uint32_t rec, uint32_t src)
{
    TAKE(rec + 0x20, 0, 2);
    TAKE(rec + 0x24, 2, 2);
    TAKE(rec + 0x28, 4, 2);
    TAKE(rec + 0x2c, 6, 2);
}

/* A record's shown bit, records 12..14. */
static void old_shown(uint32_t rec, uint32_t src)
{
    if (M8(src) != 0)
        M32(rec + R_FLAGS) |= F_SHOWN;
    else
        M32(rec + R_FLAGS) &= 0x7fffffffu;
}

/* .text+0x1c10 -- message 0x22: one window record of a situation being
 * loaded.  The three views an old situation describes field by field get
 * their entries on the first record of any kind; a 0x3de record is a whole
 * view of the current kind. */
static void FS98_STDCALL load_record(uint32_t record)
{
    uint32_t src = M32(record + 0xc), e0, e1, e2, e, i;

    if (M32(S_OLD_LAYOUT + 0) == 0) {
        M32(S_OLD_LAYOUT + 0) = new_entry(0);
    }
    if (M32(S_OLD_LAYOUT + 4) == 0) {
        M32(S_OLD_LAYOUT + 4) = new_entry(0);
    }
    if (M32(S_OLD_LAYOUT + 8) == 0) {
        M32(S_OLD_LAYOUT + 8) = new_entry(1);
    }
    e0 = M32(S_OLD_LAYOUT + 0);
    e1 = M32(S_OLD_LAYOUT + 4);
    e2 = M32(S_OLD_LAYOUT + 8);

    switch (M32(record)) {
    case 4:  old_rect(M32(e0 + L_REC), src); break;
    case 5:  old_view(M32(e0 + L_VIEW), src); break;
    case 6:
        TAKE(M32(e0 + L_VIEW) + 0x50, 0, 2);
        TAKE(M32(e0 + L_VIEW) + 0x52, 2, 2);
        break;
    case 7:  old_rect(M32(e1 + L_REC), src); break;
    case 8:  old_view(M32(e1 + L_VIEW), src); break;
    case 9:
        TAKE(M32(e1 + L_VIEW) + 0x50, 0, 2);
        TAKE(M32(e1 + L_VIEW) + 0x52, 2, 2);
        break;
    case 10: old_rect(M32(e2 + L_REC), src); break;
    case 11:
        /* The byte goes over the bottom of the argument's own slot. */
        H_MEMMOVE((uint32_t)(uintptr_t)&record, src, 1);
        switch (record & 0xff) {
        case 0: M32(M32(S_OLD_LAYOUT + 0) + L_ACTIVE) = 1; break;
        case 1: M32(M32(S_OLD_LAYOUT + 4) + L_ACTIVE) = 1; break;
        case 2: M32(M32(S_OLD_LAYOUT + 8) + L_ACTIVE) = 1; break;
        }
        break;
    case 12: old_shown(M32(e0 + L_REC), src); break;
    case 13: old_shown(M32(e1 + L_REC), src); break;
    case 14: old_shown(M32(e2 + L_REC), src); break;
    case 0x1e:
        for (i = 0; i < 3; i++) {
            e = M32(S_OLD_LAYOUT + 4 * i);
            TAKE(e + L_Z, i, 1);
            if (M32(e + L_Z) == 2)
                M32(e + L_Z) = 1;
            if (M32(e + L_Z) == 4)
                M32(e + L_Z) = 2;
        }
        break;
    case 0x4d:
        TAKE(M32(e2 + L_VIEW) + 0x26, 0, 2);
        break;
    case 0x66: old_eye(M32(e0 + L_VIEW), src); break;
    case 0x67: old_eye(M32(e1 + L_VIEW), src); break;
    case 0x73:
        TAKE(G + 0xb58, 0, 2);
        TAKE(G + 0xb5c, 2, 2);
        TAKE(G + 0xb60, 8, 2);
        break;
    case 0x3de:
        e = H_LIST_ADD(S_LAYOUT);
        H_ALLOC(1, 0xa4, e + L_REC, 1);
        H_ALLOC(1, 0xc0, e + L_VIEW, 1);
        M32(M32(e + L_REC) + R_VIEW) = M32(e + L_VIEW);
        M32(e + L_Z) = 0;
        M32(e + L_ACTIVE) = 0;
        view_defaults(M32(e + L_REC), 0, 0);
        load_fields(src, S_LAYOUT_FIELDS, e);
        fields_size(S_LAYOUT_FIELDS);
        break;
    }
}

/* .text+0x2cd0 -- message 0x21: the situation is in.  If GLOBAL +0x1250 says
 * to, every 3D view is replaced by one per shown entry, with the saved
 * geometry and view, maximised if it was, then stacked and activated as
 * saved.  The entries go either way.  The first entry's view takes GLOBAL
 * +0x550 first, so an empty list faults reading through a null array, in
 * retail too. */
static void FS98_STDCALL restore_layout(void)
{
    uint32_t data = M32(S_LAYOUT + 0x18), count, e, rec, old, i, n;
    uint32_t nv, ov;

    M32(M32(data + L_VIEW) + 0xbc) = GLB16(0x550);
    count = M32(S_LAYOUT);
    if (GLB32(0x1250) != 0) {
        destroy_3d_all();
        for (i = 0, n = count; n != 0; i++, n--) {
            e = data + 0x18 * i;
            if (!(M32(M32(e + L_REC) + R_FLAGS) & F_SHOWN))
                continue;
            ov = M32(e + L_VIEW);
            M32(e + L_NEW) = create_3d(M32(ov + 0x10), M32(ov + 0x14));
            rec = M32(e + L_NEW);
            M32(e + L_NEW_VIEW) = M32(rec + R_VIEW);
            nv = M32(e + L_NEW_VIEW);
            old = M32(e + L_REC);
            M32(rec + R_FLAGS) |= M32(old + R_FLAGS) & 0xdffeffffu;
            M32(rec + 0x20) = M32(old + 0x20);
            M32(rec + 0x24) = M32(old + 0x24);
            M32(rec + 0x28) = M32(old + 0x28);
            M32(rec + 0x2c) = M32(old + 0x2c);
            M32(rec + 0x50) = M32(old + 0x50);
            M32(rec + 0x54) = M32(old + 0x54);
            M32(rec + 0x58) = M32(old + 0x58);
            M32(rec + 0x5c) = M32(old + 0x5c);
            ov = M32(e + L_VIEW);
            M16(nv + 0x1c) = M16(ov + 0x1c);
            M16(nv + 0x1e) = M16(ov + 0x1e);
            M16(nv + 0x20) = M16(ov + 0x20);
            M16(nv + 0x22) = M16(ov + 0x22);
            M16(nv + 0x24) = M16(ov + 0x24);
            M16(nv + 0x26) = M16(ov + 0x26);
            M16(nv + 0x28) = M16(ov + 0x28);
            M32(nv + 0x2c) = M32(ov + 0x2c);
            M8(nv + 0x30) = M8(ov + 0x30);
            M8(nv + 0x31) = M8(ov + 0x31);
            M32(nv + 0x34) = M32(ov + 0x34);
            M32(nv + 0x38) = M32(ov + 0x38);
            M16(nv + 0x50) = M16(ov + 0x50);
            M16(nv + 0x52) = M16(ov + 0x52);
            M16(nv + 0x54) = M16(ov + 0x54);
            M16(nv + 0x56) = M16(ov + 0x56);
            M32(nv + 0xbc) = M32(ov + 0xbc);
            set_mode(rec, M32(M32(rec + R_VIEW) + 0x14));
            nv = M32(rec + R_VIEW);
            set_orientation(nv, M32(nv + 0x18));
            H_WIN_UPDATE(rec);
            if (M32(M32(e + L_REC) + R_FLAGS) & F_MAXIMISED)
                maximise(rec);
        }
        restore_order(data, (int32_t)count);
    }
    for (i = 0; i < count; i++) {
        H_FREE(data + 0x18 * i + L_REC);
        H_FREE(data + 0x18 * i + L_VIEW);
    }
    H_FREE((uint32_t)(uintptr_t)&data);
    M32(S_LAYOUT + 0x00) = 0;
    M32(S_LAYOUT + 0x04) = 0;
    M32(S_LAYOUT + 0x10) = 0;
    M32(S_LAYOUT + 0x14) = 0;
    M32(S_LAYOUT + 0x18) = 0;
    M32(S_OLD_LAYOUT + 0) = 0;
    M32(S_OLD_LAYOUT + 4) = 0;
    M32(S_OLD_LAYOUT + 8) = 0;
    M32(S_LAYOUT + 0x08) = 0x20;
    M32(S_LAYOUT + 0x0c) = 0x18;
}

/* .text+0x3270 -- message 0x60: a new aircraft, and its window geometry from
 * its configuration ("Default View": X, Y, SIZE_X, SIZE_Y).  A 3D view within
 * 25 of the last aircraft's geometry on every side moves to the new; with a
 * window over the panel the panel only goes.  The new geometry is kept
 * either way. */
static void FS98_STDCALL aircraft_geometry(void)
{
    char path[0x104];
    uint32_t list, count, i, rec, x = 0, y = 0, w = 0, h = 0;
    int32_t gx, gy, gw, gh;

    ACONTAIN_PATH(G + 0x11e0, 6, (uint32_t)(uintptr_t)path, 0x104);
    H_PROFILE_INT(S_SECTION, S_KEY_X, (uint32_t)(uintptr_t)&x,
                  (uint32_t)(uintptr_t)path, 0x24);
    H_PROFILE_INT(S_SECTION, S_KEY_Y, (uint32_t)(uintptr_t)&y,
                  (uint32_t)(uintptr_t)path, 0x24);
    H_PROFILE_INT(S_SECTION, S_KEY_SIZE_X, (uint32_t)(uintptr_t)&w,
                  (uint32_t)(uintptr_t)path, 0x24);
    H_PROFILE_INT(S_SECTION, S_KEY_SIZE_Y, (uint32_t)(uintptr_t)&h,
                  (uint32_t)(uintptr_t)path, 0x24);
    if (M32(S_MAXIMISED) != 0) {
        PANELS_HIDE();
    } else if (w != 0 && h != 0) {
        list_windows((uint32_t)(uintptr_t)&list, (uint32_t)(uintptr_t)&count,
                     3);
        gx = (int32_t)M32(S_GEOMETRY + 0);
        gy = (int32_t)M32(S_GEOMETRY + 4);
        gw = (int32_t)M32(S_GEOMETRY + 8);
        gh = (int32_t)M32(S_GEOMETRY + 12);
        for (i = 0; i < count; i++) {
            rec = M32(list + 4 * i);
            if ((int32_t)M32(rec + 0x20) < gx - 0x19 ||
                (int32_t)M32(rec + 0x20) > gx + 0x19 ||
                (int32_t)M32(rec + 0x24) < gy - 0x19 ||
                (int32_t)M32(rec + 0x24) > gy + 0x19 ||
                (int32_t)M32(rec + 0x28) < gw - 0x19 ||
                (int32_t)M32(rec + 0x28) > gw + 0x19 ||
                (int32_t)M32(rec + 0x2c) < gh - 0x19 ||
                (int32_t)M32(rec + 0x2c) > gh + 0x19)
                continue;
            M32(rec + 0x20) = x;
            M32(rec + 0x24) = y;
            M32(rec + 0x28) = w;
            M32(rec + 0x2c) = h;
            H_WIN_UPDATE(rec);
            break;
        }
        H_FREE((uint32_t)(uintptr_t)&list);
    }
    M32(S_GEOMETRY + 0) = x;
    M32(S_GEOMETRY + 4) = y;
    M32(S_GEOMETRY + 8) = w;
    M32(S_GEOMETRY + 12) = h;
}

/* .text+0x2c0 -- CHAIN list 0x14, the situation's messages. */
static void FS98_STDCALL situation_message(uint32_t message, uint32_t data)
{
    switch (message) {
    case 0x21: restore_layout(); break;
    case 0x22: load_record(data); break;
    case 0x23: save_layout(); break;
    case 0x60: aircraft_geometry(); break;
    }
}

/* ---- init: .text+0x20..+0x10f -------------------------------------------------------- */

/* .text+0x20 -- the stars, a word's length then the rest; and the strings. */
static void window_init(void)
{
    uint32_t length = 0, file = 0, got = 0;

    if (H_OPEN((uint32_t)(uintptr_t)&file, S_STARS_NAME, 0, 0) == 0) {
        H_READ(file, (uint32_t)(uintptr_t)&length, 2, (uint32_t)(uintptr_t)&got);
        length -= 2;
        if (H_ALLOC(2, length, S_STARS, 1) == 0)
            H_READ(file, M32(S_STARS), length, (uint32_t)(uintptr_t)&got);
        H_CLOSE(file);
    }
    load_strings();
    CHAIN_ADD(0x14, (uint32_t)(uintptr_t)situation_message);
}

/* .text+0xe0 */
static void window_deinit(void)
{
    CHAIN_REMOVE(0x14, (uint32_t)(uintptr_t)situation_message);
    free_strings();
    if (M32(S_STARS) != 0)
        H_FREE(S_STARS);
}

/* ======== the assembly: .text+0x3504..+0x6191 ======================================== */

/* ---- registers ------------------------------------------------------------------ */

static uint32_t eax, ebx, ecx, edx, esi, edi, ebp;

#define AX        ((uint16_t)eax)
#define BX        ((uint16_t)ebx)
#define CX        ((uint16_t)ecx)
#define DX        ((uint16_t)edx)
#define SI        ((uint16_t)esi)
#define DI        ((uint16_t)edi)
#define BP        ((uint16_t)ebp)
#define AH        ((uint8_t)(eax >> 8))
#define CH        ((uint8_t)(ecx >> 8))
#define SET_LO(r, v)  ((r) = ((r) & 0xffff0000u) | (uint16_t)(v))
#define SET_AX(v) SET_LO(eax, v)
#define SET_BX(v) SET_LO(ebx, v)
#define SET_CX(v) SET_LO(ecx, v)
#define SET_DX(v) SET_LO(edx, v)
#define SET_SI(v) SET_LO(esi, v)
#define SET_DI(v) SET_LO(edi, v)
#define SET_BP(v) SET_LO(ebp, v)
#define SET_AL(v) (eax = (eax & 0xffffff00u) | (uint8_t)(v))
#define SET_AH(v) (eax = (eax & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define SET_DL(v) (edx = (edx & 0xffffff00u) | (uint8_t)(v))

#define SAVE_REGS    uint32_t saved_ebx = ebx, saved_esi = esi, saved_edi = edi
#define RESTORE_REGS (ebx = saved_ebx, esi = saved_esi, edi = saved_edi)

/* imul r16 then add ax,ax / adc dx,dx: dx:ax = 2 * ax * b */
static void imul2(uint16_t b)
{
    uint32_t p = (uint32_t)((int32_t)(int16_t)AX * (int16_t)b) << 1;

    SET_AX(p);
    SET_DX(p >> 16);
}

/* imul r16 alone: dx:ax = ax * b */
static void imul16(uint16_t b)
{
    uint32_t p = (uint32_t)((int32_t)(int16_t)AX * (int16_t)b);

    SET_AX(p);
    SET_DX(p >> 16);
}

/* sar dx,1 / rcr ax,1 */
static void sar_dxax(void)
{
    uint16_t c = DX & 1;

    SET_DX((uint16_t)((int16_t)DX >> 1));
    SET_AX((AX >> 1) | (c << 15));
}

/* div and idiv by a 16-bit register.  Retail's overflow (or divides by
 * zero) and the host's filter (0x4119a0, around the sim's main loop)
 * repairs the fault: div gives ax = dx = 0xffff; idiv ax = 0x7fff, or 0x8001
 * when the divisor and dx differ in sign, and dx = 0x7fff.  Here that is
 * done without the fault. */
static void div16(uint16_t d)
{
    uint32_t n = ((uint32_t)DX << 16) | AX;

    if (d == 0 || n / d > 0xffff) {
        SET_AX(0xffff);
        SET_DX(0xffff);
        return;
    }
    SET_AX(n / d);
    SET_DX(n % d);
}

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

/* The 48-bit values are a word and a dword: (edx:ax) against [p], [p+2]. */
static void add48(uint32_t p)
{
    uint32_t s = (uint32_t)AX + M16(p);

    SET_AX(s);
    edx = edx + M32(p + 2) + (s >> 16);
}

static void sub48(uint32_t p)
{
    uint32_t b = AX < M16(p);

    SET_AX(AX - M16(p));
    edx = edx - M32(p + 2) - b;
}

/* add [p],ax / adc [p+2],edx */
static void add48_to(uint32_t p)
{
    uint32_t s = (uint32_t)M16(p) + AX;

    M16(p) = (uint16_t)s;
    M32(p + 2) = M32(p + 2) + edx + (s >> 16);
}

/* neg ax / not edx / sbb edx,-1 */
static void neg48(void)
{
    uint32_t c = AX != 0;

    SET_AX(-AX);
    edx = ~edx + 1 - c;
}

/* shl edx,8 / mov dl,ah: the 48-bit value as 24.8 */
static void to_24_8(void)
{
    edx = (edx << 8) | AH;
}

/* sar eax,8 / cdq / shld edx,eax,16: a 16.16 offset as (edx:ax) */
static void offset48(void)
{
    eax = (uint32_t)((int32_t)eax >> 8);
    edx = (uint32_t)((int32_t)eax >> 16);
}

/* UTIL, with 16-bit arguments in the low halves of dwords whose top halves
 * retail leaves as the stack had them; UTIL reads only the words.  Retail
 * calls through eax = UTIL's Linkage, and +0x2c, +0x40..+0x4c and +0x5c (a
 * bare ret) keep every register, so eax comes back the Linkage.  +0x28
 * leaves edx 0, the top of a square of a word. */
#define U_SQRT    CALL(UTIL, 0x1c, f1)
#define U_HYPOT   CALL(UTIL, 0x28, f2)
#define U_SINCOS  CALL(UTIL, 0x2c, f3)
#define U_ANGLE   CALL(UTIL, 0x38, f2)   /* from a sine, the cosine's sign the side */
#define U_40      CALL(UTIL, 0x40, f3)
#define U_44      CALL(UTIL, 0x44, f3)
#define U_48      CALL(UTIL, 0x48, f3)
#define U_4C      CALL(UTIL, 0x4c, f3)
#define U_5C      CALL(UTIL, 0x5c, f0)

static void eye_for_mode(void);
static void eye_transition(void);
static void eye_follow(void);
static void eye_save(void);
static void eye_matrix(void);
static void FS98_STDCALL aspect(void);
static void eye_orient(void);
static void eye_zoom(void);
static void eye_look(void);
static void look_vector(void);
static void eye_angles(void);
static void spot_target(void);
static void FS98_STDCALL matrix_angles(uint32_t m, uint32_t out);
static void eye_map(void);
static void eye_cockpit(void);
static void eye_virtual(void);
static void eye_tower(void);
static void eye_track(void);
static void eye_spot(void);
static void spot_spring(void);

/* ---- matrices: .text+0x387b..+0x3ab7 -------------------------------------------- */

/* .text+0x392a -- the view matrix at esi from the sines and cosines at
 * +0x2d2..+0x2dc, through temporaries at +0x2bc..+0x2d0. */
static void matrix_build(void)
{
    SET_AX(M16(D + 0x2d6));
    SET_CX(M16(D + 0x2dc));
    M16(D + 0x2bc) = AX;
    M16(D + 0x2c2) = CX;
    SET_AX(-AX);
    M16(esi + 0xa) = AX;
    SET_AX(M16(D + 0x2d2));
    SET_CX(M16(D + 0x2d8));
    M16(D + 0x2be) = AX;
    M16(D + 0x2c4) = CX;
    imul2(M16(D + 0x2c2));
    M16(esi + 6) = DX;
    SET_AX(M16(D + 0x2c4));
    imul2(M16(D + 0x2c2));
    M16(esi + 8) = DX;
    SET_AX(M16(D + 0x2d4));
    SET_CX(M16(D + 0x2da));
    M16(D + 0x2c0) = AX;
    M16(D + 0x2d0) = CX;
    imul2(M16(D + 0x2c2));
    M16(esi + 4) = DX;
    SET_AX(M16(D + 0x2c2));
    imul2(M16(D + 0x2d0));
    M16(esi + 0x10) = DX;
    SET_AX(M16(D + 0x2d0));
    imul2(M16(D + 0x2be));
    M16(D + 0x2ca) = DX;
    SET_AX(M16(D + 0x2d0));
    imul2(M16(D + 0x2c4));
    M16(D + 0x2c6) = DX;
    SET_AX(DX);
    imul2(M16(D + 0x2bc));
    M16(D + 0x2ce) = DX;
    SET_AX(M16(D + 0x2c0));
    imul2(M16(D + 0x2be));
    M16(D + 0x2c8) = DX;
    SET_AX(DX);
    imul2(M16(D + 0x2bc));
    SET_DX(DX + M16(D + 0x2c6));
    M16(esi) = DX;
    SET_AX(M16(D + 0x2c8));
    SET_AX(AX + M16(D + 0x2ce));
    M16(esi + 0xe) = AX;
    SET_AX(M16(D + 0x2c0));
    imul2(M16(D + 0x2c4));
    M16(D + 0x2cc) = DX;
    SET_AX(M16(D + 0x2bc));
    imul2(DX);
    SET_DX(DX - M16(D + 0x2ca));
    M16(esi + 2) = DX;
    SET_AX(M16(D + 0x2ca));
    imul2(M16(D + 0x2bc));
    SET_DX(DX - M16(D + 0x2cc));
    M16(esi + 0xc) = DX;
}

/* .text+0x387b -- pitch cx, bank bx, heading dx, into the matrix at esi.
 * UTIL's sincos keeps every register. */
static void angles_matrix(void)
{
    uint32_t keep = ecx;

    ecx = ebx;
    U_SINCOS(CX, D + 0x2d2, D + 0x2d8);
    ecx = keep;
    U_SINCOS(CX, D + 0x2d6, D + 0x2dc);
    U_SINCOS(DX, D + 0x2d4, D + 0x2da);
    eax = UTIL;
    matrix_build();
}

/* .text+0x3831 -- GLOBAL's view matrix from its eye angles. */
static void eye_matrix(void)
{
    uint32_t keep = edi;

    edi = G;
    esi = edi + 0xce8;
    SET_CX(M16(edi + 0x2e6));
    SET_BX(M16(edi + 0x2e8));
    SET_DX(M16(edi + 0x2ea));
    angles_matrix();
    edi = keep;
}

/* .text+0x385a -- +0x2c: the matrix for (pitch, bank, heading). */
static void FS98_STDCALL angles_to_matrix(uint32_t pitch, uint32_t bank,
                                          uint32_t heading, uint32_t out)
{
    SAVE_REGS;

    SET_CX(pitch);
    SET_BX(bank);
    SET_DX(heading);
    esi = out;
    angles_matrix();
    RESTORE_REGS;
}

/* .text+0x3918 -- GLOBAL's view matrix from the sines and cosines. */
static void matrix_from_trig(void)
{
    esi = G + 0xce8;
    matrix_build();
}

/* .text+0x3ab8 -- +0x30: stretch the matrix to the window's shape, by the
 * longer side over the shorter.  The divide is 16 bits of a 32-bit width,
 * so a wide window (or a factor that wraps) overflows it: saturated, as the
 * host's filter leaves retail's. */
static void FS98_STDCALL aspect(void)
{
    uint64_t p;
    SAVE_REGS;

    edi = G;
    esi = M32(edi + 0x1194);
    eax = M32(esi + R_WIDTH);
    ebx = M32(esi + R_HEIGHT);
    p = (uint64_t)eax * M32(edi + 0xdb8);
    eax = (uint32_t)p;
    edx = (uint32_t)(p >> 32);
    eax >>= 0xd;
    esi = edi + 0xce8;
    M16(edi + 0xdbc) = 0x7fff;
    M16(edi + 0xdbe) = 0x7fff;
    if (eax == ebx)
        goto out;
    if ((int32_t)eax > (int32_t)ebx) {
        SET_CX(AX);
        SET_DX(BX);
        SET_AX(0);
        sar_dxax();
        div16(CX);
        M16(edi + 0xdbc) = AX;
        SET_CX(AX);
        imul2(M16(esi));
        M16(esi) = DX;
        SET_AX(CX);
        imul2(M16(esi + 6));
        M16(esi + 6) = DX;
        SET_AX(CX);
        imul2(M16(esi + 0xc));
        M16(esi + 0xc) = DX;
    } else {
        SET_CX(BX);
        SET_DX(AX);
        SET_AX(0);
        sar_dxax();
        div16(CX);
        M16(edi + 0xdbe) = AX;
        SET_CX(AX);
        imul2(M16(esi + 2));
        M16(esi + 2) = DX;
        SET_AX(CX);
        imul2(M16(esi + 8));
        M16(esi + 8) = DX;
        SET_AX(CX);
        imul2(M16(esi + 0xe));
        M16(esi + 0xe) = DX;
    }
out:
    RESTORE_REGS;
}

/* ---- the eye: .text+0x3504..+0x3830 ----------------------------------------------- */

/* Follow `target` with a lag of 4 units either way. */
static void deadband(uint32_t target, uint32_t slot)
{
    eax = target - M32(slot);
    if ((int32_t)eax >= 0) {
        if ((int32_t)eax < 0x40000)
            return;
        eax -= 0x40000;
    } else {
        if ((int32_t)eax > -0x40000)
            return;
        eax += 0x40000;
    }
    M32(slot) += eax;
}

/* .text+0x35f3 -- CHAIN list 0x13 may place the eye itself; if nobody does,
 * the view's mode does. */
static void eye_choose(void)
{
    M32(D + 0x2de) = 0;
    eax = CHAIN_SEND(0x13, 0, D + 0x2de);
    if (M32(D + 0x2de) == 0)
        eye_for_mode();
}

/* .text+0x3504 -- the aircraft's attitude, followed with a lag, then the
 * eye; and the eye's angles as dwords at +0x5c8. */
static void eye_update(void)
{
    uint32_t keep_ebp, keep_edi;
    SAVE_REGS;

    edi = G;
    deadband(M32(edi + 0x578), D + 0x2b0);
    eax = M32(D + 0x2b0) >> 0x10;
    M16(edi + 0x2e0) = AX;
    deadband(M32(edi + 0x57c), D + 0x2b4);
    eax = M32(D + 0x2b4) >> 0x10;
    M16(edi + 0x2e2) = AX;
    eax = M32(edi + 0x580) >> 0x10;
    M16(edi + 0x2e4) = AX;
    deadband(M32(edi + 0x580), D + 0x2b8);
    eax = M32(D + 0x2b8) >> 0x10;
    M16(edi + 0x2e4) = AX;
    keep_ebp = ebp;
    keep_edi = edi;
    eye_choose();
    edi = keep_edi;
    ebp = keep_ebp;
    eax = (uint32_t)M16(edi + 0x2e6) << 0x10;
    M32(edi + 0x5c8) = eax;
    eax = (uint32_t)M16(edi + 0x2e8) << 0x10;
    M32(edi + 0x5cc) = eax;
    eax = (uint32_t)M16(edi + 0x2ea) << 0x10;
    M32(edi + 0x5d0) = eax;
    RESTORE_REGS;
}

/* .text+0x36a5 -- the eye's offset from the aircraft (GLOBAL +0xdb0), turned
 * by the aircraft's attitude and added to its position; the view's eye
 * position at +0x58 follows. */
static void eye_offset(void)
{
    edi = G;
    eax = M32(edi + 0x578) >> 0x10;
    M16(D + 0x2e2) = AX;
    eax = M32(edi + 0x57c) >> 0x10;
    M16(D + 0x2e4) = AX;
    eax = M32(edi + 0x580) >> 0x10;
    M16(D + 0x2e6) = AX;
    U_4C(D + 0x2e2, edi + 0xdb0, D + 0x2e8);
    eax = UTIL;
    edi = G;
    eax = M32(D + 0x2ec);
    ecx = (uint32_t)((int32_t)eax >> 0x10);
    M32(edi + 0x2d8) += ecx;
    offset48();
    add48(edi + 0x572);
    M16(edi + 0x5c2) = AX;
    M32(edi + 0x5c4) = edx;
    eax = M32(D + 0x2f0);
    ecx = (uint32_t)((int32_t)eax >> 0x10);
    M32(edi + 0x2dc) += ecx;
    offset48();
    add48(edi + 0x562);
    M16(edi + 0x5b2) = AX;
    M32(edi + 0x5b4) = edx;
    eax = M32(D + 0x2e8);
    ecx = (uint32_t)((int32_t)eax >> 0x10);
    M32(edi + 0x2d4) += ecx;
    offset48();
    M16(D + 0x2f6) = AX;
    M32(D + 0x2f8) = edx;
    U_44(D + 0x2f4, edi + 0x560, D + 0x2f4);
    eax = UTIL;
    edi = G;
    SET_AX(M16(D + 0x2f6));
    edx = M32(D + 0x2f8);
    add48(edi + 0x56a);
    M16(edi + 0x5ba) = AX;
    M32(edi + 0x5bc) = edx;
    edx = M32(edi + 0x1198);
    eax = H_MEMMOVE(edx + 0x58, edi + 0x5b0, 0x18);
}

/* .text+0x3637 -- by the view's mode, or through a transition while +0x56
 * says the last frame's was another. */
static void eye_for_mode(void)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    if (M32(ebx + 0x10) == 1) {
        eye_map();
        return;
    }
    eax = M32(ebx + 0x14);
    SET_CX(M16(ebx + 0x56));
    if (AX != CX) {
        eye_transition();
        return;
    }
    switch (eax) {
    case 0: eye_cockpit(); break;
    case 1: eye_virtual(); break;
    case 2: eye_tower(); break;
    case 3: eye_track(); break;
    case 4: eye_spot(); break;
    }
}

/* .text+0x3b9c and +0x3c82 -- modes 0 and 1: the eye in the aircraft, with
 * the zoom of view +0x1c or +0x1e; the panel's own matrix if GLOBAL +0xd2c
 * says so. */
static void eye_inside(uint32_t zoom)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(ebx + zoom));
    M16(edi + 0x2b0) = AX;
    eax = H_MEMMOVE(edi + 0x2e6, edi + 0x2e0, 6);
    eax = H_MEMMOVE(edi + 0x2d4, edi + 0xd98, 0xc);
    eye_offset();
    edi = G;
    if (M32(edi + 0xd2c) != 1)
        eye_matrix();
    else
        eax = H_MEMMOVE(edi + 0xce8, edi + 0xd18, 0x12);
    eye_orient();
    eye_zoom();
    aspect();
    eye_save();
}

static void eye_cockpit(void)
{
    eye_inside(0x1c);
}

static void eye_virtual(void)
{
    eye_inside(0x1e);
}

/* .text+0x3d68 -- mode 2, tower: from the tower, GLOBAL +0xd50. */
static void eye_tower(void)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(ebx + 0x20));
    M16(edi + 0x2b0) = AX;
    edi = G;
    eax = H_MEMMOVE(D + 0x30e, edi + 0xd50, 0x18);
    eye_follow();
    eye_save();
}

/* .text+0x3dc3 -- the eye at the target position +0x30e, kept two units
 * above the ground (GLOBAL +0x20), looking at the aircraft.  edi is GLOBAL
 * on the way in. */
static void eye_follow(void)
{
    eax = M32(D + 0x322);
    SET_DX(M16(D + 0x320));
    eax <<= 8;
    SET_AL(DX >> 8);
    eax -= M32(edi + 0x20);
    eax -= 0x200;
    if ((int32_t)eax < 0) {
        eax = M32(edi + 0x20);
        eax += 0x200;
        SET_DX(AX);
        SET_DX(DX << 8);
        eax = (uint32_t)((int32_t)eax >> 8);
        M16(D + 0x320) = DX;
        M32(D + 0x322) = eax;
    }
    edi = G;
    eax = H_MEMMOVE(edi + 0x5b0, D + 0x30e, 0x18);
    U_5C();
    eax = UTIL;
    edi = G;
    edi += 0x560;
    eye_look();
    edi = G;
    M32(edi + 0xd48) &= 0xfffffffeu;
    M32(edi + 0xd48) |= 2;
    aspect();
}

/* .text+0x3e82 -- the eye's position and angles into the view block. */
static void eye_save(void)
{
    edi = G;
    edx = M32(edi + 0x1198);
    if (edx == 0)
        return;
    eax = H_MEMMOVE(edx + 0x58, edi + 0x5b0, 0x18);
    edx = M32(edi + 0x1198);
    eax = H_MEMMOVE(edx + 0x70, edi + 0x5c8, 0xc);
}

/* .text+0x3efe -- mode 3, track: at the aircraft if GLOBAL +0xdc2 says
 * (looking from +0x588), else from the track point +0xd74. */
static void eye_track(void)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(ebx + 0x22));
    M16(edi + 0x2b0) = AX;
    if (M16(edi + 0xdc2) != 0) {
        edi = G;
        eax = H_MEMMOVE(D + 0x30e, edi + 0x560, 0x18);
        edi = G;
        eax = H_MEMMOVE(edi + 0x5b0, D + 0x30e, 0x18);
        U_5C();
    eax = UTIL;
        edi = G;
        edi += 0x588;
        eye_look();
        edi = G;
        M32(edi + 0xd48) &= 0xffffffedu;
        M32(edi + 0xd48) |= 1;
        aspect();
    } else {
        edi = G;
        eax = H_MEMMOVE(D + 0x30e, edi + 0xd74, 0x18);
        eye_follow();
    }
    eye_save();
}

/* .text+0x4027 -- mode 4, spot: the spot plane, chasing the aircraft. */
static void eye_spot(void)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(ebx + 0x24));
    M16(edi + 0x2b0) = AX;
    spot_spring();
    eye_follow();
}

/* .text+0x41ba -- the map's height for zoom bx: between two of the masks
 * by bx's low seven bits, as (edx:ax) in 24.8 with 0xff below.  It leaves
 * the masks in di and si. */
static void zoom_height(void)
{
    uint16_t keep = BX, lo, hi;
    uint64_t p;
    int32_t f;

    ebx &= 0xff80;
    ebx >>= 5;
    SET_AX(M16(CONST_AT(T_MASKS) + ebx));
    SET_DX(M16(CONST_AT(T_MASKS) + ebx + 2));
    SET_DI(M16(CONST_AT(T_MASKS) + ebx + 4));
    SET_SI(M16(CONST_AT(T_MASKS) + ebx + 6));
    SET_CX(keep);
    lo = AX;
    hi = DX;
    {
        uint32_t b = DI < AX;

        SET_DI(DI - AX);
        SET_SI(SI - DX - b);
    }
    SET_CX(CX & 0x7f);
    SET_CX((uint16_t)(CX << 8));
    SET_DX(SI);
    SET_AX(DI);
    SET_AX(SI);
    eax <<= 0x10;
    SET_AX(DI);
    ecx = (uint32_t)(int32_t)(int16_t)CX;
    ecx += ecx;
    f = (int32_t)ecx;
    p = (uint64_t)((int64_t)(int32_t)eax * f);
    eax = (uint32_t)p;
    edx = (uint32_t)(p >> 32);
    SET_DI(lo);
    SET_SI(hi);
    eax >>= 0x10;
    {
        uint32_t s = (uint32_t)AX + DI;

        SET_AX(s);
        SET_DX(DX + SI + (s >> 16));
    }
    edx = (uint32_t)(int32_t)(int16_t)DX;
    to_24_8();
    SET_AX((uint16_t)(AX << 8));
    SET_AL(0xff);
}

/* .text+0x4049 -- the map: straight down from a height the zoom sets
 * (GLOBAL +0x2b2, in steps of 0x80 up to 0x780), north up unless
 * +0xdc4/+0xdc6 ask for the aircraft's heading; or, with GLOBAL +0x390 2,
 * from the zoom's double over the ground. */
static void eye_map(void)
{
    edi = G;
    SET_AX(M16(CONST_AT(0x20904500)));
    M16(edi + 0x2e6) = AX;
    SET_AX(0);
    M16(edi + 0x2e8) = AX;
    M16(edi + 0x2ea) = AX;
    if (M16(edi + 0xdc4) != 0 && M16(edi + 0xdc6) >= 1) {
        if (M16(edi + 0xdc6) == 1 ||
            (SET_AX(M16(edi + 0x2b2)), AX < M16(CONST_AT(0x20904502)))) {
            SET_AX(M16(edi + 0x2e4));
            M16(edi + 0x2ea) = AX;
        }
    }
    if (M32(edi + 0x390) != 2) {
        SET_BX(M16(edi + 0x2b2));
        SET_BX(BX & 0xff80);
        if ((int16_t)BX > 0x780)
            SET_BX(0x780);
        M16(edi + 0x2b2) = BX;
        M16(edi + 0xd44) = 0;
        zoom_height();
    } else {
        SET_BX(M16(edi + 0x2b2));
        M16(edi + 0xd44) = 1;
        zoom_height();
        edi = G;
        {
            uint32_t s = (uint32_t)AX + AX;

            SET_AX(s);
            edx = edx + edx + (s >> 16);
        }
        ebx = M32(edi + 0x20);
        ecx = ebx;
        ecx = (uint32_t)((int32_t)ecx >> 0x10);
        {
            uint32_t s = (uint32_t)AX + BX;

            SET_AX(s);
            edx = edx + ecx + (s >> 16);
        }
    }
    edi = G;
    ecx = edx;
    ecx <<= 0x10;
    SET_CX(AX);
    M32(edi + 0x2d8) = ecx;
    to_24_8();
    SET_AX((uint16_t)(AX << 8));
    SET_AL(0xff);
    M16(edi + 0x5c2) = AX;
    M32(edi + 0x5c4) = edx;
    eax = H_MEMMOVE(edi + 0x2d4, edi + 0xd98, 8);
    edi = G;
    eax = H_MEMMOVE(edi + 0x5b0, edi + 0x560, 0x10);
    eye_matrix();
    edi = G;
    M16(edi + 0xd46) = 0x7fff;
    aspect();
}

/* .text+0x57b4 -- (edx:ax) times cx/0x8000, as 48 bits. */
static void scale(void)
{
    uint32_t keep_esi = esi, keep_edi = edi;
    int64_t p;

    esi = edx;
    eax = AX;
    ecx = (uint32_t)(int32_t)(int16_t)CX;
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    p >>= 0xf;
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    edi = edx;
    {
        uint32_t t = esi;

        esi = eax;
        eax = t;
    }
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    p = (int64_t)((uint64_t)p << 1);
    p += (int64_t)(((uint64_t)edi << 32) | esi);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    edx = (edx << 0x10) | (eax >> 0x10);
    esi = keep_esi;
    edi = keep_edi;
}

/* .text+0x42ce -- the spot plane's target: behind the aircraft at view
 * +0x2c along heading +0x28 (+0x8001 an eighth as far), the aircraft's or
 * the view's own (+0x30), raised by +0x34, never under two units above the
 * ground.  At +0x30e, and the distance at +0x362 for UTIL's longitude. */
static void spot_target(void)
{
    uint16_t d_lo, d_hi;

    edi = G;
    SET_BX(M16(edi + 0x2e4));
    esi = M32(edi + 0x1198);
    if (M8(esi + 0x30) != 0) {
        if (M8(esi + 0x30) == 1) {
            SET_AX(M16(edi + 0x2e2));
            SET_AX(AX + 0x4000);
            if ((int16_t)AX < 0)
                SET_BX(BX + 0x8000);
        } else {
            SET_BX(0);
        }
    }
    SET_BX(BX + M16(esi + 0x28));
    edx = M32(esi + 0x2c);
    SET_AX(DX);
    edx >>= 0x10;
    d_hi = DX;
    d_lo = AX;
    M16(D + 0x35c) = BX;
    U_SINCOS(M16(D + 0x35c), D + 0x2d2, D + 0x2d8);
    eax = UTIL;
    SET_CX(M16(D + 0x2d8));
    SET_AX(d_lo);
    SET_DX(d_hi);
    if (M16(esi + 0x28) == 0x8001) {
        sar_dxax();
        sar_dxax();
        sar_dxax();
    }
    d_hi = DX;
    d_lo = AX;
    SET_AX(DX);
    eax <<= 0x10;
    SET_AX(d_lo);
    ecx = (uint32_t)((int32_t)(int16_t)CX * 2);
    {
        int64_t p = (int64_t)(int32_t)eax * (int32_t)ecx;

        eax = (uint32_t)p;
        edx = (uint32_t)((uint64_t)p >> 32);
    }
    edx = (edx << 8) | (eax >> 24);
    eax = (uint32_t)((int32_t)eax >> 8);
    edi = G;
    add48(edi + 0x562);
    M16(D + 0x310) = AX;
    M32(D + 0x312) = edx;
    /* the pops swap the halves, and the shift swaps them back */
    SET_DX(d_lo);
    SET_AX(d_hi);
    eax <<= 0x10;
    SET_AX(DX);
    ecx = (uint32_t)((int32_t)(int16_t)M16(D + 0x2d2) * 2);
    {
        int64_t p = (int64_t)(int32_t)eax * (int32_t)ecx;

        eax = (uint32_t)p;
        edx = (uint32_t)((uint64_t)p >> 32);
    }
    eax = (eax >> 8) | (edx << 24);
    M32(D + 0x362) = eax;
    U_40(D + 0x362, edi + 0x560, D + 0x340);
    eax = UTIL;
    edi = G;
    SET_AX(M16(D + 0x342));
    edx = M32(D + 0x344);
    add48(edi + 0x56a);
    M16(D + 0x318) = AX;
    M32(D + 0x31a) = edx;
    esi = M32(edi + 0x1198);
    SET_CX(M16(edi + 0x572));
    edi = M32(edi + 0x574);
    SET_CX((CX >> 8) | (uint16_t)(DI << 8));
    edi >>= 8;
    edx = M32(esi + 0x34);
    SET_AX(DX);
    edx >>= 0x10;
    if (M16(esi + 0x28) == 0x8001) {
        edx = M32(esi + 0x2c);
        SET_AX(DX);
        edx >>= 0x10;
    }
    {
        uint32_t s = (uint32_t)CX + AX;

        SET_CX(s);
        SET_DI(DI + DX + (s >> 16));
    }
    ebx = G;
    edx = M32(ebx + 0x20);
    SET_AX(DX);
    edx >>= 0x10;
    {
        uint32_t s = (uint32_t)AX + 0x200;

        SET_AX(s);
        SET_DX(DX + (s >> 16));
    }
    if ((int16_t)DI > (int16_t)DX)
        goto keep;
    if ((int16_t)DI < (int16_t)DX || CX <= AX) {
        SET_CX(AX);
        SET_DI(DX);
    }
keep:
    SET_AL(0);
    SET_AH(CX);
    M16(D + 0x320) = AX;
    eax = (uint32_t)(int32_t)(int16_t)DI;
    eax <<= 8;
    SET_AL(CH);
    M32(D + 0x322) = eax;
}

/* .text+0x44ad -- one axis (esi 0, 8 or 0x10) of the spot plane's spring:
 * the velocity at view +0x96 takes the target's move since the last frame
 * (+0x7e), less its own decay by GLOBAL +0x274; the position (+0x5a) moves
 * by it, and by half the gap that is left -- or jumps to the target when
 * the gap is past 0x20 units. */
static void spring_axis(void)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(esi + ebx + 0x96));
    edx = M32(esi + ebx + 0x98);
    ecx = M32(edi + 0x274);
    scale();
    neg48();
    add48(esi + D + 0x310);
    sub48(esi + ebx + 0x7e);
    add48_to(esi + ebx + 0x96);
    SET_AX(M16(esi + D + 0x310));
    edx = M32(esi + D + 0x312);
    M16(esi + ebx + 0x7e) = AX;
    M32(esi + ebx + 0x80) = edx;
    SET_AX(M16(esi + ebx + 0x96));
    edx = M32(esi + ebx + 0x98);
    scale();
    add48_to(esi + ebx + 0x5a);
    SET_AX(M16(esi + D + 0x310));
    edx = M32(esi + D + 0x312);
    sub48(esi + ebx + 0x5a);
    if ((int32_t)edx > 0x200000 || (int32_t)edx < (int32_t)0xffe00000u) {
        M16(esi + ebx + 0x96) = 0;
        M32(esi + ebx + 0x98) = 0;
    } else {
        scale();
        {
            uint32_t c = edx & 1;

            edx = (uint32_t)((int32_t)edx >> 1);
            SET_AX((AX >> 1) | (c << 15));
        }
    }
    add48_to(esi + ebx + 0x5a);
}

/* .text+0x4228 -- the spot plane: its spring, unless +0x31 pins it to the
 * target. */
static void spot_spring(void)
{
    spot_target();
    edi = G;
    ebx = M32(edi + 0x1198);
    if (M8(ebx + 0x31) == 0) {
        esi = 0;
        spring_axis();
        esi = 8;
        spring_axis();
        esi = 0x10;
        spring_axis();
        edi = G;
        ecx = M32(edi + 0x1198);
        eax = H_MEMMOVE(D + 0x30e, ecx + 0x58, 0x18);
    } else {
        esi = ebx;
        eax = H_MEMMOVE(esi + 0x58, D + 0x30e, 0x18);
    }
}

/* ---- transitions between modes: .text+0x458a..+0x4f82 -------------------------- */

/* .text+0x4ca5 -- one axis (esi) of a glide from position edi to ebp: the
 * view's position (+0x5a) starts at edi, the distance left (+0x7e) is the
 * whole way, and the rate (+0x96) its negative. */
static void glide_axis(void)
{
    ebx = G;
    ebx = M32(ebx + 0x1198);
    SET_AX(M16(esi + edi + 2));
    edx = M32(esi + edi + 4);
    M16(esi + ebx + 0x5a) = AX;
    M32(esi + ebx + 0x5c) = edx;
    SET_AX(M16(esi + ebp + 2));
    edx = M32(esi + ebp + 4);
    sub48(esi + edi + 2);
    M16(esi + ebx + 0x7e) = AX;
    M32(esi + ebx + 0x80) = edx;
    neg48();
    M16(esi + ebx + 0x96) = AX;
    M32(esi + ebx + 0x98) = edx;
}

/* .text+0x4c84 -- a glide from edi to esi. */
static void glide_start(void)
{
    ebp = esi;
    esi = 0;
    glide_axis();
    esi = 8;
    glide_axis();
    esi = 0x10;
    glide_axis();
}

/* .text+0x4d18 -- one angle (esi 0, 4, 8): the view's (+0x70) from edi,
 * and how far it has to turn to ebx's (+0xac). */
static void glide_angle(void)
{
    edx = G;
    edx = M32(edx + 0x1198);
    eax = M32(esi + edi);
    M32(esi + edx + 0x70) = eax;
    eax = M32(esi + ebx);
    eax -= M32(esi + edi);
    if ((int32_t)eax < 0)
        eax = -eax;
    M32(esi + edx + 0xac) = eax;
}

/* .text+0x4cf9 */
static void glide_angles(void)
{
    esi = 0;
    glide_angle();
    esi = 4;
    glide_angle();
    esi = 8;
    glide_angle();
}

/* .text+0x4d3d -- the view's position as the eye target (+0x30e), and its
 * angles as GLOBAL's eye angles.  Leaves esi at view +0x70, edi at +0x326. */
static void glide_publish(void)
{
    uint32_t i;

    edx = G;
    edx = M32(edx + 0x1198);
    esi = edx + 0x58;
    edi = D + 0x30e;
    for (i = 0; i < 0x18; i++)
        M8(edi + i) = M8(esi + i);
    esi += 0x18;
    edi += 0x18;
    ecx = 0;
    esi = edx + 0x70;
    {
        uint32_t keep = edi;

        edi = G;
        eax = (uint32_t)((int32_t)M32(esi) >> 0x10);
        M16(edi + 0x2e6) = AX;
        eax = (uint32_t)((int32_t)M32(esi + 4) >> 0x10);
        M16(edi + 0x2e8) = AX;
        eax = (uint32_t)((int32_t)M32(esi + 8) >> 0x10);
        M16(edi + 0x2ea) = AX;
        edi = keep;
    }
}

/* .text+0x4d9a -- the eye target from esi. */
static void glide_target(void)
{
    uint32_t i;

    edi = D + 0x30e;
    for (i = 0; i < 0x18; i++)
        M8(edi + i) = M8(esi + i);
    esi += 0x18;
    edi += 0x18;
    ecx = 0;
}

/* .text+0x4dd3 -- one axis of a glide's step: the distance left closes by
 * the rate times the frame time (GLOBAL +0x2aa); past zero it is zero and
 * bp says so.  The position is the target less what is left. */
static void glide_step_axis(void)
{
    uint16_t keep = CX;

    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(esi + ebx + 0x96));
    edx = M32(esi + ebx + 0x98);
    SET_CX(M16(edi + 0x2aa));
    scale();
    SET_BP(0);
    ecx = edx;
    add48(esi + ebx + 0x7e);
    ecx ^= edx;
    if (!((int32_t)ecx < 0)) {
        SET_AX(0);
        edx = 0;
        SET_BP(BP + 1);
    }
    M16(esi + ebx + 0x7e) = AX;
    M32(esi + ebx + 0x80) = edx;
    neg48();
    add48(esi + D + 0x310);
    M16(esi + ebx + 0x5a) = AX;
    M32(esi + ebx + 0x5c) = edx;
    SET_CX(keep);
}

/* .text+0x4da8 -- a step on every axis; bp ends 1 when all three are
 * there. */
static void glide_step(void)
{
    esi = 0;
    glide_step_axis();
    SET_CX(BP);
    esi = 8;
    glide_step_axis();
    SET_CX(CX + BP);
    esi = 0x10;
    glide_step_axis();
    SET_CX((uint16_t)((int16_t)CX >> 1));
    SET_BP(BP & CX);
}

/* .text+0x4e66 -- one angle's turn toward the aim (+0x2fc), by its rate
 * times the frame time, not past it. */
static void turn_axis(void)
{
    int64_t p;
    uint32_t delta;

    edi = G;
    edx = M32(edi + 0x1198);
    eax = M32(esi + D + 0x2fc);
    eax -= M32(esi + edx + 0x70);
    delta = eax;
    eax = M32(esi + edx + 0xac);
    SET_CX(M16(edi + 0x2aa));
    ecx = (uint32_t)(int32_t)(int16_t)CX;
    ecx += ecx;
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    eax = (eax >> 0x10) | (edx << 0x10);
    edi = eax;
    if ((int32_t)delta >= 0) {
        eax = delta - edi;
        if ((int32_t)eax < 0)
            edi += eax;
    } else {
        edi = -edi;
        eax = delta - edi;
        if (!((int32_t)eax < 0))
            edi += eax;
    }
    edx = G;
    edx = M32(edx + 0x1198);
    M32(esi + edx + 0x70) += edi;
}

/* .text+0x4e45 */
static void turn(void)
{
    uint32_t keep = edi;

    esi = 0;
    turn_axis();
    esi = 4;
    turn_axis();
    esi = 8;
    turn_axis();
    edi = keep;
}

/* .text+0x574f -- UTIL's angle from a sine ax, on cx's side; and +0x577e,
 * the same from a cosine (a quarter round the other way). */
static void angle_(void)
{
    uint32_t keep = esi;

    eax = U_ANGLE(AX, CX);
    esi = keep;
}

static void angle_turned(void)
{
    angle_();
    SET_AX(-AX);
    SET_AX(AX + 0x4000);
}

/* .text+0x4ec8 -- the angles from edi to esi, as dwords at +0x2fc..+0x304
 * (bank 0). */
static void aim(void)
{
    look_vector();
    SET_AX(M16(D + 0x2d6));
    SET_CX(M16(D + 0x2dc));
    angle_();
    eax <<= 0x10;
    M32(D + 0x2fc) = eax;
    M32(D + 0x300) = 0;
    SET_AX(M16(D + 0x2d4));
    SET_CX(M16(D + 0x2da));
    angle_();
    eax <<= 0x10;
    M32(D + 0x304) = eax;
}

/* .text+0x4d8b */
static void aim_turn(void)
{
    uint16_t keep = BP;

    aim();
    turn();
    SET_BP(keep);
}

/* .text+0x4f0c -- end any transition: the spot plane's spring starts at its
 * target with no speed. */
static void view_snap(void)
{
    uint32_t keep;

    spot_target();
    edi = G;
    esi = M32(edi + 0x1198);
    keep = esi;
    eax = H_MEMMOVE(esi + 0x7c, D + 0x30e, 0x18);
    esi = keep;
    eax = H_MEMMOVE(esi + 0x94, CONST_AT(0x209044e8), 0x18);
}

/* The tails the transitions share. */
static void tail_inside(void)       /* .text+0x4763, +0x4961 */
{
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(ebx + 0x1c));
    M16(edi + 0x2b0) = AX;
    M32(edi + 0xd48) &= 0xfffffffcu;
    /* .text+0x4c19 */
    edi = G;
    eax = H_MEMMOVE(edi + 0x5b0, D + 0x30e, 0x18);
    U_5C();
    eax = UTIL;
    eye_matrix();
    eye_zoom();
    aspect();
}

static void tail_follow(uint32_t zoom)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(ebx + zoom));
    M16(edi + 0x2b0) = AX;
    eye_follow();
}

static void tail_spot(void)        /* .text+0x47e9, +0x4880 */
{
    edi = G;
    ebx = M32(edi + 0x1198);
    if (ebx != 0) {
        SET_AX(M16(ebx + 0x24));
        M16(edi + 0x2b0) = AX;
    }
    eye_follow();
}

#define SET_STATE(v) (ebx = G, ebx = M32(ebx + 0x1198), M16(ebx + 0x56) = (v))

/* .text+0x458a -- view +0x56 is the last frame's mode, or a transition in
 * progress: from the cockpit to the tower (8), the track to the tower
 * (0x1e), the spot to the tower (0x5a), the tower to the cockpit (0x14),
 * the spot to the cockpit (0x46), the tower to the track (0x28), the spot
 * to the track (0x50), the cockpit to the spot (0x32), the tower or track
 * to the spot (0x3c).  Each glides the eye, then lands on the new mode.
 * Anything else, or transitions off (GLOBAL +0xdc0), snaps. */
static void eye_transition(void)
{
    edi = G;
    if (M16(edi + 0xdc0) == 0)
        goto snap;
    edi = G;
    ebx = M32(edi + 0x1198);
    ecx = M16(ebx + 0x56);
    eax = M32(ebx + 0x14);
    if (eax == 0 || eax == 1) {
        switch (ecx) {
        case 2:                                     /* .text+0x4984 */
            ebx = G;
            edi = ebx + 0xd50;
            esi = ebx + 0x560;
            glide_start();
            glide_publish();
            SET_STATE(0x14);
            break;
        case 0x14:                                  /* .text+0x49b4 */
            edi = G;
            esi = edi + 0x560;
            glide_target();
            SET_BP(0);
            glide_step();
            glide_publish();
            if (BP != 0) {
                ebx = G;
                ebx = M32(ebx + 0x1198);
                eax = M32(ebx + 0x14);
                M16(ebx + 0x56) = AX;
            }
            break;
        case 4:                                     /* .text+0x4a07 */
            edx = G;
            ebx = M32(edx + 0x1198);
            edi = ebx + 0x58;
            esi = edx + 0x560;
            glide_start();
            glide_publish();
            SET_STATE(0x46);
            tail_follow(0x24);
            return;
        case 0x46:                                  /* .text+0x4a3a */
            edi = G;
            esi = edi + 0x560;
            glide_target();
            SET_BP(0);
            glide_step();
            glide_publish();
            if (BP != 0) {
                ebx = G;
                ebx = M32(ebx + 0x1198);
                eax = M32(ebx + 0x14);
                M16(ebx + 0x56) = AX;
            }
            tail_follow(0x24);
            return;
        default:
            goto snap;
        }
        tail_follow(0x20);
        return;
    }
    if (eax == 2) {
        switch (ecx) {
        case 0: case 1:                             /* .text+0x46b6 */
            ebx = G;
            edi = ebx + 0x560;
            esi = ebx + 0xd50;
            glide_start();
            ebx = G;
            esi = ebx + 0x560;
            edi = ebx + 0xd50;
            aim();
            edi = G;
            ebx = D + 0x2fc;
            edi = edi + 0x578;
            glide_angles();
            glide_publish();
            SET_STATE(8);
            tail_inside();
            return;
        case 8:                                     /* .text+0x4714 */
        case 0x1e:                                  /* .text+0x4912 */
            edi = G;
            esi = edi + 0xd50;
            glide_target();
            SET_BP(0);
            glide_step();
            edi = G;
            ebx = M32(edi + 0x1198);
            esi = ebx + 0x58;
            edi = edi + 0xd50;
            aim_turn();
            glide_publish();
            if (BP != 0)
                SET_STATE(2);
            tail_inside();
            return;
        case 3:                                     /* .text+0x48a1 */
            ebx = G;
            edi = ebx + 0xd74;
            esi = ebx + 0xd50;
            glide_start();
            edi = G;
            esi = edi + 0xd74;
            edi = edi + 0x560;
            aim();
            /* The aim from the aircraft is replaced at once by the aim
             * from the tower, and both ends of the turn are that. */
            {
                uint32_t keep = D + 0x2fc;

                edi = G;
                esi = edi + 0xd74;
                edi = edi + 0xd50;
                aim();
                ebx = D + 0x2fc;
                edi = keep;
            }
            glide_angles();
            glide_publish();
            SET_STATE(0x1e);
            tail_inside();
            return;
        case 4:                                     /* .text+0x4b94 */
            edi = G;
            ebx = M32(edi + 0x1198);
            esi = edi + 0xd50;
            edi = ebx + 0x58;
            glide_start();
            glide_publish();
            SET_STATE(0x5a);
            tail_follow(0x24);
            return;
        case 0x5a:                                  /* .text+0x4bc7 */
            edi = G;
            esi = edi + 0xd50;
            glide_target();
            SET_BP(0);
            glide_step();
            glide_publish();
            if (BP != 0)
                SET_STATE(2);
            tail_follow(0x24);
            return;
        default:
            goto snap;
        }
    }
    if (eax == 3) {
        switch (ecx) {
        case 2:                                     /* .text+0x4a8d */
            ebx = G;
            edi = ebx + 0xd50;
            esi = ebx + 0xd74;
            glide_start();
            glide_publish();
            SET_STATE(0x28);
            tail_follow(0x20);
            return;
        case 0x28:                                  /* .text+0x4abd */
            edi = G;
            esi = edi + 0xd74;
            glide_target();
            SET_BP(0);
            glide_step();
            glide_publish();
            if (BP != 0)
                SET_STATE(3);
            tail_follow(0x20);
            return;
        case 4:                                     /* .text+0x4b0f */
            edx = G;
            ebx = M32(edx + 0x1198);
            edi = ebx + 0x58;
            esi = edx + 0xd74;
            glide_start();
            glide_publish();
            SET_STATE(0x50);
            tail_follow(0x24);
            return;
        case 0x50:                                  /* .text+0x4b42 */
            edi = G;
            esi = edi + 0xd74;
            glide_target();
            SET_BP(0);
            glide_step();
            glide_publish();
            if (BP != 0)
                SET_STATE(3);
            tail_follow(0x24);
            return;
        default:
            goto snap;
        }
    }
    if (eax == 4) {
        switch (ecx) {
        case 0: case 1:                             /* .text+0x4786 */
            spot_target();
            edi = G;
            edi = edi + 0x560;
            esi = D + 0x30e;
            glide_start();
            glide_publish();
            SET_STATE(0x32);
            tail_spot();
            return;
        case 0x32:                                  /* .text+0x47bb */
        case 0x3c:                                  /* .text+0x4852 */
            spot_target();
            SET_BP(0);
            glide_step();
            glide_publish();
            if (BP != 0) {
                SET_STATE(4);
                view_snap();
            }
            tail_spot();
            return;
        case 3:                                     /* .text+0x481d */
        case 2:                                     /* .text+0x480a */
        {
            uint32_t from = ecx == 3 ? 0xd74 : 0xd50;

            spot_target();
            edi = G;
            edi = edi + from;
        }
            esi = D + 0x30e;
            glide_start();
            glide_publish();
            SET_STATE(0x3c);
            tail_spot();
            return;
        default:
            goto snap;
        }
    }
snap:                                               /* .text+0x459a */
    view_snap();
    edi = G;
    ebx = M32(edi + 0x1198);
    eax = M32(ebx + 0x14);
    M16(ebx + 0x56) = AX;
    eye_for_mode();
}

/* ---- orientation and zoom: .text+0x4f83..+0x5302 -------------------------------- */

/* .text+0x531c -- the 3x3 word matrix at esi times the one +0x326 points
 * at, into edi, doubled. */
static void matrix_multiply(void)
{
    M16(D + 0x33c) = 3;
    do {
        ebx = M32(D + 0x326);
        M16(D + 0x33e) = 3;
        do {
            uint32_t s, b;

            SET_AX(M16(esi));
            imul16(M16(ebx));
            SET_CX(AX);
            SET_BP(DX);
            SET_AX(M16(esi + 2));
            imul16(M16(ebx + 6));
            s = (uint32_t)CX + AX;
            SET_CX(s);
            SET_BP(BP + DX + (s >> 16));
            SET_AX(M16(esi + 4));
            imul16(M16(ebx + 0xc));
            s = (uint32_t)CX + AX;
            SET_CX(s);
            SET_BP(BP + DX + (s >> 16));
            b = CX >> 15;
            SET_CX((uint16_t)(CX << 1));
            SET_BP((uint16_t)(BP << 1) | b);
            M16(edi) = BP;
            edi += 2;
            ebx += 2;
            M16(D + 0x33e) = (uint16_t)(M16(D + 0x33e) - 1);
        } while (M16(D + 0x33e) != 0);
        esi += 6;
        M16(D + 0x33c) = (uint16_t)(M16(D + 0x33c) - 1);
    } while (M16(D + 0x33c) != 0);
}

/* .text+0x5302 -- GLOBAL's eye angles back from its view matrix. */
static void eye_angles(void)
{
    edi = G;
    matrix_angles(edi + 0xce8, edi + 0x2e6);
}

/* .text+0x4f83 -- the view's own turn (its matrix at +0x3e) on top of the
 * eye's, and GLOBAL +0xd48's clipping bits for it. */
static void eye_orient(void)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    M32(edi + 0xd48) &= 0xfffffff5u;
    M32(edi + 0xd48) |= 0x11;
    if (M32(ebx + 0x14) == 1 || M32(ebx + 0x18) != 0) {
        if (M32(ebx + 0x18) == 8)
            M32(edi + 0xd48) |= 8;
        M32(edi + 0xd48) &= 0xffffffefu;
    }
    esi = edi + 0xce8;
    edi = ebx + 0x3e;
    M32(D + 0x326) = edi;
    edi = D + 0x32a;
    matrix_multiply();
    edi = G;
    eax = H_MEMMOVE(edi + 0xce8, D + 0x32a, 0x12);
    eye_angles();
}

/* .text+0x5092 -- the zoom ax (at least 4) as a shift (+0xd44, the power of
 * two below it, less 2) and a factor (+0xd46, that power over the zoom,
 * halved). */
static void zoom_factor(void)
{
    SET_BP(AX);
    SET_CX(AX);
    SET_BX(0xffff);
    SET_DI(0x8000);
    do {
        SET_BX(BX + 1);
        SET_DI((uint16_t)((DI << 1) | (DI >> 15)));
        SET_CX(CX >> 1);
    } while (CX != 0);
    SET_BX(BX - 4);
    if ((int16_t)BX < 0)
        SET_BX(0);
    M16(esi + 0xd44) = BX;
    SET_DX(DI);
    SET_AX(0);
    if (DX < BP) {
        div16(BP);
        SET_CX(AX);
    } else {
        SET_CX(0xffff);
    }
    SET_CX(CX >> 1);
    M16(esi + 0xd46) = CX;
}

/* .text+0x5018 -- the zoom on the matrix's forward column.  edi is GLOBAL
 * on the way in. */
static void eye_zoom(void)
{
    esi = G;
    ebx = M32(edi + 0x1198);
    SET_AX(M16(edi + 0x2b0));
    if ((int16_t)AX < 4)
        SET_AX(4);
    zoom_factor();
    SET_AX(M16(esi + 0xcec));
    M16(D + 0x308) = AX;
    imul2(CX);
    M16(esi + 0xcec) = DX;
    SET_AX(M16(esi + 0xcf2));
    M16(D + 0x30a) = AX;
    imul2(CX);
    M16(esi + 0xcf2) = DX;
    SET_AX(M16(esi + 0xcf8));
    M16(D + 0x30c) = AX;
    imul2(CX);
    M16(esi + 0xcf8) = DX;
}

/* .text+0x50fa -- the direction from esi to edi (positions: a word and a
 * dword per axis, 8 apart) as sines and cosines at +0x2d2..+0x2dc, through
 * UTIL's longitude (+0x48) and G3D's normalising (+0x3c).  Straight up or
 * down leaves pitch and heading at 0. */
static void look_vector(void)
{
    uint32_t keep_ebx = ebx, keep_esi, keep_edi;
    uint16_t first;

    SET_AX(M16(edi + 2));
    edx = M32(edi + 4);
    sub48(esi + 2);
    to_24_8();
    M32(D + 0x358) = edx;
    SET_AX(M16(edi + 0xa));
    edx = M32(edi + 0xc);
    sub48(esi + 0xa);
    ebx = esi;
    keep_edi = edi;
    keep_esi = esi;
    M16(D + 0x342) = AX;
    M32(D + 0x344) = edx;
    M32(D + 0x35e) = ebx;
    U_48(D + 0x340, M32(D + 0x35e), D + 0x348);
    eax = UTIL;
    SET_AX(M16(D + 0x34a));
    edx = M32(D + 0x34c);
    to_24_8();
    M32(D + 0x350) = edx;
    esi = keep_esi;
    edi = keep_edi;
    SET_AX(M16(edi + 0x12));
    edx = M32(edi + 0x14);
    sub48(esi + 0x12);
    to_24_8();
    M32(D + 0x354) = edx;
    eax = G3D_NORMAL(D + 0x350);
    eax = M32(D + 0x350) >> 0x10;
    ecx = M32(D + 0x358) >> 0x10;
    eax = U_HYPOT(AX, CX);
    edx = 0;
    SET_CX(AX);
    if (AX == 0) {
        M16(D + 0x2d4) = 0;
        M16(D + 0x2da) = 0x7fff;
    } else {
        uint16_t t;

        edx = M32(D + 0x350);
        SET_AX(DX);
        edx >>= 0x10;
        sar_dxax();
        ebx = M32(D + 0x358);
        ebx >>= 0x10;
        t = CX; SET_CX(BX); SET_BX(t);
        idiv16(BX);
        t = CX; SET_CX(BX); SET_BX(t);
        M16(D + 0x2d4) = AX;
        SET_DX(BX);
        SET_AX(0);
        sar_dxax();
        SET_BX(CX);
        idiv16(BX);
        M16(D + 0x2da) = AX;
    }
    eax = M32(D + 0x354) >> 0x10;
    first = CX;
    eax = U_HYPOT(AX, CX);
    edx = 0;
    SET_BX(first);
    SET_CX(AX);
    if (CX == 0) {
        M16(D + 0x2d6) = 0;
        M16(D + 0x2dc) = 0x7fff;
    } else {
        uint16_t t;

        edx = M32(D + 0x354);
        SET_AX(DX);
        edx >>= 0x10;
        sar_dxax();
        t = CX; SET_CX(BX); SET_BX(t);
        idiv16(BX);
        t = CX; SET_CX(BX); SET_BX(t);
        SET_AX(-AX);
        M16(D + 0x2d6) = AX;
        SET_DX(BX);
        SET_AX(0);
        sar_dxax();
        SET_BX(CX);
        idiv16(BX);
        M16(D + 0x2dc) = AX;
    }
    M16(D + 0x2d2) = 0;
    M16(D + 0x2d8) = 0x7fff;
    ebx = keep_ebx;
}

/* .text+0x50df -- look from edi at the target +0x30e. */
static void eye_look(void)
{
    esi = D + 0x30e;
    look_vector();
    matrix_from_trig();
    eye_angles();
    eye_zoom();
}

/* ---- the Linkage's assembly: .text+0x5381..+0x5761 -------------------------------- */

/* .text+0x5381 -- +0x24: the view's turn (+0x18: the low three bits eighths
 * of a circle round, bit 3 straight down) on its own angles (+0x50..+0x54),
 * into its matrix at +0x3e. */
static void FS98_STDCALL set_orientation(uint32_t view, uint32_t turn_code)
{
    uint32_t c;
    SAVE_REGS;
    uint32_t keep_ebp = ebp;

    esi = view;
    ebx = turn_code;
    M32(esi + 0x18) = ebx;
    SET_CX(0);
    ebx &= 0xffff00ffu;
    SET_BX((uint16_t)(BX << 0xc));
    c = BX >> 15;
    SET_BX((uint16_t)(BX << 1));
    if (c)
        SET_CX(0x4000);
    SET_DX(BX);
    SET_DX(DX + M16(esi + 0x50));
    SET_CX(CX + M16(esi + 0x52));
    SET_BX(M16(esi + 0x54));
    esi += 0x3e;
    angles_matrix();
    ebp = keep_ebp;
    RESTORE_REGS;
}

/* .text+0x53c0 -- +0x20: the cofactors of GLOBAL's view matrix, words at
 * +0xcfa..+0xd0a and three of them as dwords at +0xd0c..+0xd14. */
static void FS98_STDCALL cofactors(void)
{
    SAVE_REGS;
    uint32_t g = G;

#define COF(a, b, c, d, out) do {                                         \
        uint32_t p1 = (uint32_t)((int32_t)(int16_t)M16(g + (a)) *         \
                                 (int16_t)M16(g + (b)));                  \
        uint32_t p2 = (uint32_t)((int32_t)(int16_t)M16(g + (c)) *         \
                                 (int16_t)M16(g + (d)));                  \
        uint32_t r = p2 - p1;                                             \
        SET_AX(r);                                                        \
        SET_DX(r >> 16);                                                  \
        SET_BX(p1);                                                       \
        SET_CX(p1 >> 16);                                                 \
        M16(g + (out)) = DX;                                              \
    } while (0)

    edi = g;
    COF(0xcf2, 0xcf6, 0xcf0, 0xcf8, 0xcfa);
    COF(0xcea, 0xcf8, 0xcec, 0xcf6, 0xcfc);
    COF(0xcec, 0xcf0, 0xcea, 0xcf2, 0xcfe);
    COF(0xcee, 0xcf8, 0xcf2, 0xcf4, 0xd00);
    COF(0xcec, 0xcf4, 0xce8, 0xcf8, 0xd02);
    COF(0xce8, 0xcf2, 0xcec, 0xcee, 0xd04);
    COF(0xcf0, 0xcf4, 0xcee, 0xcf6, 0xd06);
    edx = (edx << 0x10) | AX;
    edx = (uint32_t)((int32_t)edx >> 1);
    M32(g + 0xd0c) = edx;
    COF(0xce8, 0xcf6, 0xcea, 0xcf4, 0xd08);
    edx = (edx << 0x10) | AX;
    edx = (uint32_t)((int32_t)edx >> 1);
    M32(g + 0xd10) = edx;
    COF(0xcea, 0xcee, 0xce8, 0xcf0, 0xd0a);
    edx = (edx << 0x10) | AX;
    edx = (uint32_t)((int32_t)edx >> 1);
    M32(g + 0xd14) = edx;
#undef COF
    RESTORE_REGS;
}

/* .text+0x55c0 -- step the active 3D view's mode by eax, round 0..4,
 * passing over the track (3) when GLOBAL +0x11b0 has none. */
static void step_mode(void)
{
    edi = G;
    ebx = M32(edi + 0x11a0);
    if (ebx == 0 || M32(ebx + 0x10) == 1)
        return;
    ecx = M32(ebx + 0x14);
    ecx += eax;
    if ((int32_t)ecx < 0)
        ecx = 4;
    if ((int32_t)ecx >= 5)
        ecx = 0;
    if (ecx == 3 && M16(edi + 0x11b0) == 0)
        ecx += eax;
    set_mode(M32(edi + 0x119c), ecx);
}

/* .text+0x559e -- +0x38 */
static void FS98_STDCALL next_mode(void)
{
    SAVE_REGS;

    eax = 1;
    step_mode();
    RESTORE_REGS;
}

/* .text+0x55af -- +0x34 */
static void FS98_STDCALL previous_mode(void)
{
    SAVE_REGS;

    eax = 0xffffffffu;
    step_mode();
    RESTORE_REGS;
}

/* .text+0x560d -- +0x60: the active 3D view's mode. */
static void FS98_STDCALL set_active_mode(uint32_t mode)
{
    uint32_t g = G, view = M32(g + 0x11a0);
    SAVE_REGS;

    edi = g;
    ebx = view;
    if (view != 0 && M32(view + 0x10) != 1)
        set_mode(M32(g + 0x119c), mode);
    RESTORE_REGS;
}

/* .text+0x563f -- +0x28: pitch, bank and heading from a view matrix: the
 * pitch from its +0xa as a sine, its cosine by UTIL's square root, then
 * each of the others from its two terms over that cosine: from the sine
 * while it is within 0x5a82 (45 degrees), else from the cosine. */
static void FS98_STDCALL matrix_angles(uint32_t m, uint32_t out)
{
    uint32_t keep_esi, keep_ebp = ebp;
    SAVE_REGS;

    esi = m;
    edi = out;
    SET_AX(M16(esi + 0xa));
    SET_CX(0);
    angle_();
    SET_AX(-AX);
    M16(edi) = AX;
    SET_AX(M16(esi + 0xa));
    imul16(AX);
    {
        uint32_t c = AX != 0;

        SET_AX(-AX);
        SET_DX((uint16_t)(~DX + 1 - c));
    }
    SET_DX(DX + 0x4000);
    keep_esi = esi;
    edx <<= 0x10;
    SET_DX(AX);
    eax = U_SQRT(edx);
    esi = keep_esi;
    if ((int16_t)AX < 0)
        SET_AX(0x7fff);
    M16(D + 0x2be) = AX;
    SET_CX(AX);
    SET_BX(AX);
    SET_DX(M16(esi + 4));
    SET_AX(0);
    sar_dxax();
    idiv16(BX);
    if ((int16_t)AX <= 0x5a82 && (int16_t)AX >= (int16_t)0xa57e) {
        SET_CX(M16(esi + 0x10));
        angle_();
    } else {
        SET_DX(M16(esi + 0x10));
        SET_AX(0);
        sar_dxax();
        idiv16(BX);
        SET_CX(M16(esi + 4));
        angle_turned();
    }
    M16(edi + 4) = AX;
    SET_BX(M16(D + 0x2be));
    SET_DX(M16(esi + 6));
    SET_AX(0);
    sar_dxax();
    idiv16(BX);
    if ((int16_t)AX <= 0x5a82 && (int16_t)AX >= (int16_t)0xa57e) {
        SET_CX(M16(esi + 8));
        angle_();
    } else {
        SET_DX(M16(esi + 8));
        SET_AX(0);
        sar_dxax();
        idiv16(BX);
        SET_CX(M16(esi + 6));
        angle_turned();
    }
    M16(edi + 2) = AX;
    RESTORE_REGS;
    ebp = keep_ebp;
}

/* ---- the frame: .text+0x57e0..+0x6191 ------------------------------------------------ */

/* .text+0x5e46 -- the cloud layers' bands from GLOBAL (+0xe6a/+0xe6c and
 * +0xe72/+0xe74: middle and depth), each band's bottom raised two units
 * above the ground when the eye is above its top. */
static void layer_bounds(void)
{
    SET_AX(M16(edi + 0xe6a));
    SET_BX(M16(edi + 0xe6c));
    SET_BX(BX >> 1);
    SET_AX(AX - BX);
    M16(D + 0x380) = AX;
    SET_AX(AX + M16(edi + 0xe6c));
    M16(D + 0x384) = AX;
    SET_AX(M16(edi + 0xe72));
    SET_BX(M16(edi + 0xe74));
    SET_BX(BX >> 1);
    SET_AX(AX - BX);
    M16(D + 0x388) = AX;
    SET_AX(AX + M16(edi + 0xe74));
    M16(D + 0x38c) = AX;
    eax = M32(edi + 0x5c4);
    if ((int32_t)eax >= (int32_t)M32(D + 0x384)) {
        eax = M32(edi + 0x20);
        eax >>= 8;
        eax += 2;
        if ((int32_t)eax >= (int32_t)M32(D + 0x380))
            M32(D + 0x380) = eax;
    }
    eax = M32(edi + 0x5c4);
    if ((int32_t)eax >= (int32_t)M32(D + 0x38c)) {
        eax = M32(edi + 0x20);
        eax >>= 8;
        eax += 2;
        if ((int32_t)eax >= (int32_t)M32(D + 0x388))
            M32(D + 0x388) = eax;
    }
}

/* .text+0x5ee7 -- one boundary for G3D: the distance from the eye's height
 * to a band's edge, as the new near (lower) limit, with the old one, and
 * what G3D gives back added up at +0x390.  The flags of that sum are what
 * the callers test. */
static int layer_near(uint32_t edge)
{
    eax = M32(edi + 0x5c4) - M32(edge);
    ebx = M32(D + 0x394);
    M32(D + 0x394) = eax;
    eax = G3D_LAYER(eax, ebx, 0);
    M32(D + 0x390) += eax;
    return M32(D + 0x390) == 0;
}

/* .text+0x5f49 -- and the far (upper) one */
static int layer_far(uint32_t edge)
{
    ebx = M32(edi + 0x5c4) - M32(edge);
    eax = M32(D + 0x398);
    M32(D + 0x398) = ebx;
    eax = G3D_LAYER(eax, ebx, 0);
    M32(D + 0x390) += eax;
    return M32(D + 0x390) == 0;
}

/* .text+0x5c64 -- the bands below the eye, top first; the far side of a
 * band MAIN draws as a layer (3, 4) when G3D found it in view.  Layer 4
 * needs GLOBAL +0xe8a at 0x5db. */
static void layers_below(void)
{
    M32(D + 0x390) = 0;
    if (M16(edi + 0xe78) != 0) {
        SET_AX(M16(D + 0x38c));
        if ((int16_t)M16(edi + 0x5c4) > (int16_t)AX)
            return;
        layer_near(D + 0x38c);
        SET_AX(M16(D + 0x388));
        if ((int16_t)M16(edi + 0x5c4) > (int16_t)AX)
            return;
        if (!layer_near(D + 0x388))
            eax = MAIN_LAYERS(3);
    }
    if (M16(edi + 0xe70) != 0) {
        SET_AX(M16(D + 0x384));
        if ((int16_t)M16(edi + 0x5c4) > (int16_t)AX)
            return;
        layer_near(D + 0x384);
        SET_AX(M16(D + 0x380));
        if ((int16_t)M16(edi + 0x5c4) > (int16_t)AX)
            return;
        if (layer_near(D + 0x380))
            return;
        edi = G;
        if (M16(edi + 0xe8a) < 0x5db)
            return;
        eax = MAIN_LAYERS(4);
    }
}

/* .text+0x5d40 -- and above it, bottom first (layers 1 and 2 when the
 * ground is under the band). */
static void layers_above(void)
{
    if (M16(edi + 0xe70) == 0)
        return;
    SET_AX(M16(D + 0x380));
    if ((int16_t)M16(edi + 0x5c4) < (int16_t)AX)
        return;
    layer_far(D + 0x380);
    SET_AX(M16(D + 0x380));
    if (!((int16_t)M16(edi + 0x21) > (int16_t)AX))
        eax = MAIN_LAYERS(1);
    SET_AX(M16(D + 0x384));
    if ((int16_t)M16(edi + 0x5c4) < (int16_t)AX)
        return;
    layer_far(D + 0x384);
    if (M16(edi + 0xe78) == 0)
        return;
    SET_AX(M16(D + 0x388));
    if ((int16_t)M16(edi + 0x5c4) < (int16_t)AX)
        return;
    layer_far(D + 0x388);
    SET_AX(M16(D + 0x388));
    if (!((int16_t)M16(edi + 0x21) > (int16_t)AX))
        eax = MAIN_LAYERS(2);
    SET_AX(M16(D + 0x38c));
    if ((int16_t)M16(edi + 0x5c4) < (int16_t)AX)
        return;
    layer_far(D + 0x38c);
}

/* .text+0x5be0 -- the scene between the cloud layers; in the map, G3D's
 * own. */
static void draw_layers(void)
{
    edi = G;
    if (M16(edi + 0xdce) != 0) {
        eax = G3D_FRAME_B();
        return;
    }
    layer_bounds();
    M32(D + 0x398) = 0x7fffffffu;
    M32(D + 0x394) = 0x80000001u;
    eax = MAIN_LAYERS(0);
    layers_below();
    layers_above();
    eax = G3D_LAYER(M32(D + 0x398), M32(D + 0x394), 1);
}

/* .text+0x5840 -- a 3D window's frame: GLOBAL's current window and view,
 * the eye, MAIN's preparation; the stars when the sky is dark enough
 * (GLOBAL +0xe6e/+0xe76, or +0xe86 4) and something wants them; then
 * either G3D's own frame from high up in the map, or the aircraft's model,
 * CHAIN list 0x10, MAIN's scene, the layers, list 0xe, MAIN's and
 * REALITY's after-frames; and the frame presented. */
static void FS98_STDCALL frame_3d(uint32_t rec)
{
    uint32_t keep_ebp = ebp;
    SAVE_REGS;

    edi = G;
    esi = rec;
    ebx = M32(esi + R_VIEW);
    M32(edi + 0x1194) = esi;
    M32(edi + 0x1198) = ebx;
    eax = M32(edi + 0xb60);
    M16(edi + 0xdcc) = AX;
    eye_update();
    edi = G;
    M16(edi + 0xdca) = 0;
    eax = MAIN_PREPARE();
    edi = G;
    if ((M16(edi + 0x1172) & 6) && M16(edi + 0xcba) < 0xb000) {
        ebx = M32(edi + 0x1198);
        if (AX != 0 || M16(edi + 0xb54) != 0) {
            int dark;

            if (M16(edi + 0xe86) == 4) {
                dark = 1;
            } else {
                uint32_t p;

                SET_AX(~M16(edi + 0xe6e));
                SET_DX(~M16(edi + 0xe76));
                p = (uint32_t)AX * DX;
                SET_AX(p);
                SET_DX(p >> 16);
                SET_DX(~DX);
                dark = DX < 0x8000;
            }
            if (dark && M32(S_STARS) != 0) {
                eax = G3D_DRAW(S_STARS_ARG);
                edi = G;
                M32(edi + 0xcdc) = 0;
                M32(edi + 0xce0) = 0;
                M32(edi + 0xce4) = 0;
                eax = G3D_DRAW(M32(S_STARS));
            }
        }
    }
    edi = G;
    if (M16(edi + 0xdce) != 0 && (int32_t)M32(edi + 0x390) >= 2) {
        uint8_t hi;
        uint32_t b, s;

        SET_BX(M16(edi + 0x5c2));
        SET_DX(M16(edi + 0x5c4));
        SET_CX(M16(edi + 0x5c6));
        SET_AL(M8(edi + 0x23));
        SET_AX((uint16_t)(int16_t)(int8_t)AX);
        hi = (uint8_t)(BX >> 8);
        b = hi < M8(edi + 0x20);
        ebx = (ebx & 0xffff00ffu) | ((uint32_t)(uint8_t)(hi - M8(edi + 0x20)) << 8);
        s = (uint32_t)DX - M16(edi + 0x21) - b;
        b = (uint32_t)DX < (uint32_t)M16(edi + 0x21) + b;
        SET_DX(s);
        SET_CX(CX - AX - b);
        SET_AX((uint16_t)(int16_t)(int8_t)AX);
        if ((int16_t)CX >= 4) {
            eax = G3D_FRAME_A();
            goto present;
        }
    }
    edi = G;
    if (!(M16(edi + 0x556) & 0xff))
        M32(edi + 0xd48) |= 0x11;
    edi = G;
    if (!(M16(edi + 0xdce) & 1)) {
        ebx = M32(edi + 0x11e0);
        if (ebx != 0 && (eax = M32(ebx + 0x10)) != 0 &&
            (ebx = M32(edi + 0x11dc)) != 0 &&
            (ebx = M32(edi + 0x11e4)) != 0) {
            ((uint32_t (FS98_STDCALL *)(uint32_t))(uintptr_t)eax)(ebx);
            eax = ACONTAIN_DONE(edi + 0x11e0);
        }
    }
    eax = CHAIN_SEND(0x10, 0, 0);
    eax = MAIN_SCENE();
    draw_layers();
    eax = CHAIN_SEND(0xe, 0, 0);
    edi = G;
    if (M16(edi + 0xe5a) != 0)
        eax = MAIN_AFTER();
    edi = G;
    if (M8(edi + 0x840) != 0)
        eax = REALITY_FRAME();
present:
    edi = G;
    eax = M32(edi + 0x11ac);
    ecx = M32(edi + 0x119c);
    if (ecx != 0) {
        if (ecx == M32(edi + 0x1194)) {
            eax = M32(edi + 0x11a4);
            if (M16(edi + 0xcba) < 0x8000)
                eax = M32(edi + 0x11a8);
        }
        M32(edi + 0x2ec) = eax;
        ecx = M32(edi + 0x1194);
        eax = HG2D_PRESENT(ecx + R_SURFACE, 0, 0, M32(ecx + 0x60),
                           M32(ecx + 0x64));
    }
    edi = G;
    M32(edi + 0xd4c) = 0;
    RESTORE_REGS;
    ebp = keep_ebp;
}

/* .text+0x60a9 -- the map's sky: a flat identity view, the sky colour, and
 * G3D's sky through GLOBAL +0xb34 0x9000. */
static void map_sky(void)
{
    uint16_t keep;

    edi = G;
    if (M16(edi + 0x116e) & 1)
        return;
    if (M8(edi + 0x280) == 0)
        return;
    if (M16(edi + 0xcba) >= 0x8000)
        return;
    M32(edi + 0xcdc) = 0;
    M32(edi + 0xce0) = 0;
    M32(edi + 0xce4) = 0;
    M16(edi + 0xce8) = 0x7fff;
    M16(edi + 0xcea) = 0;
    M16(edi + 0xcec) = 0;
    M16(edi + 0xcee) = 0;
    M16(edi + 0xcf0) = 0x7fff;
    M16(edi + 0xcf2) = 0;
    M16(edi + 0xcf4) = 0;
    M16(edi + 0xcf6) = 0;
    M16(edi + 0xcf8) = 0x7fff;
    M32(edi + 0x3c0) = 0x6800;
    keep = M16(edi + 0xb34);
    M16(edi + 0xb34) = 0x9000;
    eax = G3D_STARS(S_SKY_ARG);
    M16(edi + 0xb34) = keep;
}

/* .text+0x5f8c -- the map's frame: G3D's map view (GLOBAL +0xdce), a 3D frame
 * with GLOBAL +0x39c from +0x3ae, the sky, and the cross at the middle. */
static void map_draw(void)
{
    uint16_t keep_832, keep_39c;

    edi = G;
    eax = M32(edi + 0x3d4);
    M32(edi + 0x3c8) = eax;
    eax = M32(edi + 0x3d8);
    M32(edi + 0x3bc) = eax;
    M16(edi + 0x1172) = 1;
    M16(edi + 0xdce) = 1;
    keep_832 = M16(edi + 0x832);
    keep_39c = M16(edi + 0x39c);
    SET_AX(M16(edi + 0x3ae));
    M16(edi + 0x39c) = AX;
    frame_3d(M32(S_PAINT_WIN));
    edi = G;
    M16(edi + 0x39c) = keep_39c;
    map_sky();
    edi = G;
    M16(edi + 0x832) = keep_832;
    M16(edi + 0xdce) = 0;
    M32(edi + 0x2ec) = 0xf00f;
    esi = M32(edi + 0x1198);
    eax = M32(esi);
    eax >>= 4;
    ebx = M32(esi + 4);
    ecx = ebx;
    ebx -= eax;
    ecx += eax;
    edx = M32(esi + 0xc);
    esi = M32(edi + 0x1194);
    esi += R_SURFACE;
    eax = HG2D_LINE_H(esi, ebx, ecx, edx);
    esi = M32(edi + 0x1198);
    eax = M32(esi + 8);
    eax = (uint32_t)((int32_t)eax >> 4);
    ebx = M32(esi + 0xc);
    ecx = ebx;
    ecx -= eax;
    ebx += eax;
    edx = M32(esi + 4);
    esi = M32(edi + 0x1194);
    esi += R_SURFACE;
    eax = HG2D_LINE_V(esi, edx, ebx, ecx);
}

/* .text+0x57e0 -- the map window's frame, with GLOBAL +0xe88 at 0x7f00 and
 * +0x394 kept round it. */
static void FS98_STDCALL frame_map(uint32_t rec)
{
    uint32_t keep_ebp = ebp, keep_394;
    uint16_t keep_e88;
    SAVE_REGS;

    eax = rec;
    M32(S_PAINT_WIN) = eax;
    edi = G;
    keep_e88 = M16(edi + 0xe88);
    M16(edi + 0xe88) = 0x7f00;
    SET_AX(keep_e88);
    keep_394 = M32(edi + 0x394);
    {
        uint32_t keep_edi = edi;

        map_draw();
        edi = keep_edi;
    }
    SET_AH(M8(edi + 0x11ed));
    SET_AL(M8(edi + 0x11ec));
    M32(edi + 0x394) = keep_394;
    M16(edi + 0xe88) = keep_e88;
    RESTORE_REGS;
    ebp = keep_ebp;
}

/* ---- the image ---------------------------------------------------------------------- */

struct window_image window_image = {{
#include "window_data.inc"
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _window_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _window_image + 0x68");

static void window_install(void)
{
    static void *const services[30] = {
        (void *)destroy_3d_all_jump, /* +0x18 */
        (void *)paint_all,          /* +0x1c */
        (void *)cofactors,          /* +0x20 */
        (void *)set_orientation,    /* +0x24 */
        (void *)matrix_angles,      /* +0x28 */
        (void *)angles_to_matrix,   /* +0x2c */
        (void *)aspect,             /* +0x30 */
        (void *)previous_mode,      /* +0x34 */
        (void *)next_mode,          /* +0x38 */
        (void *)create_3d,          /* +0x3c */
        (void *)destroy_window,     /* +0x40 */
        (void *)destroy_3d_all,     /* +0x44 */
        (void *)activate,           /* +0x48 */
        (void *)maximise,           /* +0x4c */
        (void *)unmaximise,         /* +0x50 */
        (void *)toggle_maximised,   /* +0x54 */
        (void *)set_mode,           /* +0x58 */
        (void *)set_kind,           /* +0x5c */
        (void *)set_active_mode,    /* +0x60 */
        (void *)toggle_shown,       /* +0x64 */
        (void *)show,               /* +0x68 */
        (void *)hide,               /* +0x6c */
        (void *)set_plain,          /* +0x70 */
        (void *)clear_plain,        /* +0x74 */
        (void *)create_record,      /* +0x78 */
        (void *)destroy_record,     /* +0x7c */
        (void *)list_windows,       /* +0x80 */
        (void *)next_window,        /* +0x84 */
        (void *)previous_window,    /* +0x88 */
        (void *)find_window,        /* +0x8c */
    };
    uint32_t i;

    M32(D + WIN_LINKAGE + 0x04) = (uint32_t)(uintptr_t)window_init;
    M32(D + WIN_LINKAGE + 0x08) = (uint32_t)(uintptr_t)window_deinit;
    for (i = 0; i < 30; i++)
        M32(D + WIN_LINKAGE + 0x18 + 4 * i) = (uint32_t)(uintptr_t)services[i];
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        window_install();
    }
    return TRUE;
}
