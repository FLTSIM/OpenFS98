/* The situation reader and writer, .text+0x11cd2..+0x13104: assembly.
 *
 * A situation (a .STN flight file, or one held in memory by a video) is a
 * run of records, as main_records.inc describes.  The ones MAIN takes into
 * GLOBAL and its own .data itself are listed in the table at +0x4456; any
 * other goes out to its owner on CHAIN list 0x14.  Each table entry is
 * 0x12 bytes:
 *
 *   +0x0  u16  the record's tag; 0xffff ends the table
 *   +0x2  u8   mode: 0, 0xa, 0xb copied in; 0xc, 0xd, 0xe only while GLOBAL
 *              +0x1248, +0x124c, +0x1250 (in turn) is set, 0xe then
 *              clearing +0x4213; 1 and anything else skipped.  Saving
 *              leaves mode 0xa out altogether.
 *   +0x3  u8   where: 0 GLOBAL + offset; 1 the absolute address; 5 through
 *              a pointer (none in retail's table); 2, 3 and 4 the absolute
 *              address with the record's own length (loading)
 *   +0x4  u16  the length, or 0xffff for a list of fields after the entry,
 *              five bytes each {u32 offset, u8 size code}, up to +0xa
 *   +0x6  u32  the offset or address
 *   +0xa  u32  the next entry, after a list of fields
 *   +0xe  u32  a routine called (tag, 1) before a save, which can stop it,
 *              and (tag, 0) after a load
 *
 * Loading copies an entry's own length from the record, not the record's:
 * a record of another length leaves the walk out of step with the file.
 * Retail calls each routine under pusha/popa, and wraps every call it
 * makes in a stack-alignment shim (mov [esp-7], 0x04050607 / and esp, -4 /
 * sub esp, 4 / and [esp], 7 ... add esp, [esp]) that, esp being aligned
 * already, comes to a plain call.
 */

#define ACONTAIN  M32(V(0x20800014))
#define CONTROLS  M32(V(0x2080003c))
#define SOUND     M32(V(0x20800094))
#define UTIL      M32(V(0x208000a4))
#define WEATHERM  M32(V(0x208000ac))

typedef uint32_t (FS98_STDCALL *f8)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t);

#define H_POST        HOST_(0x03c, f2)   /* (command, parameter) */
#define H_LOCAL_TIME  HOST_(0x044, f1)   /* (&six words) */
#define H_FILE_CREATE HOST_(0x148, f3)   /* (&file, path, ext) */
#define H_JOIN        HOST_(0x184, f4)   /* (out, drive, dir, name) */
#define H_MEMSET      HOST_(0x218, f3)   /* (dst, byte, n) */
#define H_LOADSTRING  HOST_(0x2a0, f3)   /* (id, buf, len) */
#define H_STRCPY      HOST_(0x2a8, f2)
#define H_STRNCPY     HOST_(0x2ac, f3)
#define H_STRLEN      HOST_(0x2b8, f1)

#define UTIL_ALERT    CALL(UTIL, 0xa4, f8)

#define S_LOADING     V(0x20803fd0)   /* the flight being loaded: { ext, path } */
#define S_FLIGHT      V(0x208040d8)   /* the current flight: { ext, path } */
#define S_LOAD_FILE   V(0x208041e1)   /* the file a load reads (left dangling) */
#define S_CURSOR      V(0x208041e5)   /* where the walk has got to */
#define S_REC         V(0x208041e9)   /* { tag, length, body length, body } */
#define S_SAVE_FILE   V(0x208041f9)   /* the file a save writes */
#define S_OLD_B20     V(0x20804205)   /* GLOBAL +0xb20 as the save found it */
#define S_CB_C68      V(0x20804209)
#define S_CB_C58      V(0x2080420d)
#define S_TAG_59      V(0x20804211)
#define S_KEEP_E      V(0x20804213)   /* 1 until a mode 0xe record is taken */
#define S_AIRCRAFT    V(0x20804217)   /* 0x104: ACONTAIN's answer */
#define S_STRING_446  V(0x2080431b)   /* 0x104: GLOBAL +0x446's copy */
#define S_TITLE       V(0x2080441f)   /* 0x1e: the saved flight's title */
#define S_TABLE       V(0x20804456)
#define S_TAG2_SIZE   V(0x2080447e)   /* record 2's entry: the description */
#define S_TAG2_AT     V(0x20804480)
#define S_TAG61_SIZE  V(0x20804834)   /* record 0x61's: the aircraft */
#define S_TAG68_SIZE  V(0x208048a6)   /* record 0x68's: +0x446's string */

#define SEASON_TABLE  0x207f4093u     /* in retail's .text */

#define E_TAG(e)      M16((e) + 0x0)
#define E_MODE(e)     M8((e) + 0x2)
#define E_KIND(e)     M8((e) + 0x3)
#define E_SIZE(e)     M16((e) + 0x4)
#define E_AT(e)       M32((e) + 0x6)
#define E_NEXT(e)     M32((e) + 0xa)
#define E_CALL(e)     M32((e) + 0xe)

/* MAIN routines outside this slice, reached through the Linkage until
 * they are done (the harness sets those slots).  +0xa4 is called with no
 * arguments; +0x74 with (1, the day of the year), the day pushed as a word
 * over two stale bytes, which it reads as a word. */
#define MAIN_74       CALL(D + MAIN_LINKAGE, 0x74, f2)
#define MAIN_A4       CALL(D + MAIN_LINKAGE, 0xa4, f0)

/* Exported (as main_text_tail) for the harness, which patches retail's
 * call sites into its seams and has to show this copy the same bytes;
 * the export goes when +0x74 and +0xa4 are done. */
uint8_t text_tail[0x20800000u - SEASON_TABLE] __asm__("_main_text_tail") = {
#include "main_text_tail.inc"
};

/* ---- the table's routines, .text+0x11cd2..+0x11e3a ------------------------ */

/* .text+0x11cd2 -- record 0x59: which of +0xc54 and +0xc64 it sets */
static uint32_t FS98_STDCALL cb_59(uint32_t tag, uint32_t saving)
{
    (void)tag;
    if (saving == 0) {
        if ((M16(S_TAG_59) & 0xff) == 0)
            M32(G + 0xc54) = 1;
        else
            M32(G + 0xc64) = 1;
    }
    return 0;
}

/* .text+0x11d0e -- record 0x92 */
static uint32_t FS98_STDCALL cb_92(uint32_t tag, uint32_t saving)
{
    (void)tag;
    if (saving != 0)
        M32(S_CB_C68) = M8(G + 0xc68);
    else
        M32(G + 0xc64) = M32(S_CB_C68);
    return 0;
}

/* .text+0x11d45 -- record 0x93 */
static uint32_t FS98_STDCALL cb_93(uint32_t tag, uint32_t saving)
{
    (void)tag;
    if (saving != 0)
        M32(S_CB_C58) = M8(G + 0xc58);
    else
        M32(G + 0xc54) = M32(S_CB_C58);
    return 0;
}

/* .text+0x11d7c -- record 0x35: three words, 0x3fff or 0 */
static uint32_t FS98_STDCALL cb_35(uint32_t tag, uint32_t saving)
{
    uint32_t v;

    (void)tag;
    if (saving == 0) {
        v = M32(G + 0xbd0) & 0xffff;
        if (v != 0)
            v = 0x3fff;
        M32(G + 0xbd0) = v;
        M32(G + 0xbd4) = v;
        M32(G + 0xbd8) = v;
    }
    return 0;
}

/* .text+0x11dbc -- record 0x5e: three, halved */
static uint32_t FS98_STDCALL cb_5e(uint32_t tag, uint32_t saving)
{
    uint32_t v;

    (void)tag;
    if (saving == 0) {
        v = (M32(G + 0xbdc) & 0xffff) >> 1;
        M32(G + 0xbdc) = v;
        M32(G + 0xbe0) = v;
        M32(G + 0xbe4) = v;
    }
    return 0;
}

/* .text+0x11df7 -- record 0x5f: four, 0x3fff or 0 from a byte */
static uint32_t FS98_STDCALL cb_5f(uint32_t tag, uint32_t saving)
{
    uint32_t v;

    (void)tag;
    if (saving == 0) {
        v = M32(G + 0xbe8) & 0xff;
        if (v != 0)
            v = 0x3fff;
        M32(G + 0xbe8) = v;
        M32(G + 0xbec) = v;
        M32(G + 0xbf0) = v;
        M32(G + 0xbf4) = v;
    }
    return 0;
}

/* An entry's routine, by the retail address the table holds. */
static uint32_t table_call(uint32_t retail, uint32_t tag, uint32_t saving)
{
    switch (retail) {
    case 0x207f2cd2: return cb_59(tag, saving);
    case 0x207f2d0e: return cb_92(tag, saving);
    case 0x207f2d45: return cb_93(tag, saving);
    case 0x207f2d7c: return cb_35(tag, saving);
    case 0x207f2dbc: return cb_5e(tag, saving);
    case 0x207f2df7: return cb_5f(tag, saving);
    }
    /* not a routine of retail's table: a corrupted table, as retail would
     * jump to it */
    return ((f2)(uintptr_t)retail)(tag, saving);
}

/* ---- sizes ------------------------------------------------------------------ */

/* .text+0x1248e -- a field's size from its code: 0x80 1, 0x81 2, 0x83 3,
 * 0x82 4, anything else its low seven bits */
static uint32_t field_size(uint32_t field)
{
    uint8_t code = M8(field + 4);

    if (code == 0x80) return 1;
    if (code == 0x81) return 2;
    if (code == 0x83) return 3;
    if (code == 0x82) return 4;
    return code & 0x7f;
}

/* .text+0x1244b -- an entry's length: its own, or its fields' */
static uint32_t entry_size(uint32_t e)
{
    uint32_t end, f, n;

    if (E_SIZE(e) != 0xffff)
        return E_SIZE(e);
    end = E_NEXT(e);
    n = 0;
    for (f = e + 0x12; f < end; f += 5)
        n += field_size(f);
    return n;
}

/* ---- writing ------------------------------------------------------------------ */

/* .text+0x12771 -- what WriteFile wrote */
static uint32_t write_raw(uint32_t src, uint32_t n)
{
    uint32_t wrote;

    H_FILE_WRITE(M32(S_SAVE_FILE), src, n, ADDR(&wrote));
    return wrote;
}

/* .text+0x1273d -- `n` bytes for entry `e`: nothing for a mode 0xa entry,
 * 0x1001 if the disk took less */
static uint32_t write_checked(uint32_t src, uint32_t n, uint32_t e)
{
    if (E_MODE(e) == 0xa)
        return 0;
    return write_raw(src, n) != n ? 0x1001 : 0;
}

/* .text+0x124db -- each field of `e` from `base` */
static uint32_t write_fields(uint32_t e, uint32_t base, uint32_t end)
{
    uint32_t f, r;

    for (f = e + 0x12; f < end; f += 5) {
        r = write_checked(M32(f) + base, field_size(f), e);
        if (r != 0)
            return r;
    }
    return 0;
}

/* .text+0x12350 -- every entry of the table written, then every module's
 * records (list 0x14, message 0x23), then the end */
static uint32_t write_all(void)
{
    struct __attribute__((packed)) { uint16_t tag; uint32_t length; } header;
    uint32_t e = S_TABLE, r, size, at, base, hdr;

    for (;;) {
        if (E_TAG(e) == 0xffff)
            break;
        if (E_CALL(e) != 0) {
            r = table_call(E_CALL(e), E_TAG(e), 1);
            if (r != 0)
                return r;
        }
        header.tag = E_TAG(e);
        hdr = header.tag < 1000 ? 4 : 6;
        header.length = hdr + entry_size(e);
        r = write_checked(ADDR(&header), hdr, e);
        if (r != 0)
            return r;
        size = E_SIZE(e);
        at = E_AT(e);
        base = G;
        if (E_KIND(e) != 0) {
            base = 0;
            if (E_KIND(e) == 5)
                base = M32(at);
        }
        if (E_SIZE(e) == 0xffff) {
            r = write_fields(e, base, E_NEXT(e));
            e = E_NEXT(e);
            if (r != 0)
                return r;
        } else {
            r = write_checked(base + at, size, e);
            if (r != 0)
                return r;
            e += 0x12;
        }
    }
    save_records(M32(S_SAVE_FILE));
    header.tag = 0xffff;
    return write_checked(ADDR(&header), 2, e);
}

/* .text+0x120aa -- +0x40: the situation saved as a flight `title`, with
 * `description`, into the flights directory (string 0x9002); the current
 * flight is then that file.  0, or the first failure. */
static uint32_t FS98_STDCALL save_flight(uint32_t title, uint32_t description)
{
    char text[0x100], dir[0x100], path[0x104];
    uint32_t r;

    M16(G + 0x558) = 0;
    M32(S_OLD_B20) = M32(G + 0xb20);
    H_STRNCPY(S_TITLE, title, 0x1e);
    H_STRNCPY(ADDR(text), description, 0x100);
    M16(S_TAG2_SIZE) = 0x100;
    M32(S_TAG2_AT) = ADDR(text);
    CALL(ACONTAIN, 0x30, f4)(G + 0x11e0, 1, S_AIRCRAFT, 0x104);
    M16(S_TAG61_SIZE) = (uint16_t)(H_STRLEN(S_AIRCRAFT) + 1);
    M16(S_TAG68_SIZE) = (uint16_t)(H_STRLEN(G + 0x446) + 1);
    H_STRCPY(S_STRING_446, G + 0x446);
    H_LOADSTRING(0x9002, ADDR(dir), 0x100);
    r = H_JOIN(ADDR(path), 0, ADDR(dir), title);
    if (r != 0)
        return r;
    r = H_FILE_CREATE(S_SAVE_FILE, ADDR(path), 2);
    if (r != 0)
        return r;
    r = write_all();
    H_FILE_CLOSE(M32(S_SAVE_FILE));
    M32(S_SAVE_FILE) = 0;
    if (r != 0)
        return r;
    H_STRNCPY(S_FLIGHT + 4, ADDR(path), 0x103);
    return 0;
}

/* ---- reading ------------------------------------------------------------------ */

/* .text+0x1297c -- before the records: list 0x14 hears 0x20, SOUND event
 * 1, and the fields a situation may leave out are reset */
static void before_load(void)
{
    CHAIN_SEND(0x14, 0x20, 0);
    CALL(SOUND, 0x24, f2)(1, 0);
    M16(G + 0x356) = 0;
    M32(G + 0xd2c) = 0;
    H_MEMSET(S_AIRCRAFT, 0, 0x104);
    H_MEMSET(S_STRING_446, 0, 0x104);
    M32(S_KEEP_E) = 1;
    M16(G + 0xf0a) = 0;
    M16(G + 0x23e) = 0;
    M32(G + 0x5f4) = 0xffffffffu;
    M32(G + 0x7d4) = 0;
    M32(G + 0x8a8) = 0;
    M32(G + 0x940) = 0;
    M32(G + 0x9d8) = 0;
    M32(G + 0xa70) = 0;
}

/* `n` bytes from the walk to `dst`, or not, by the entry's mode */
static void take(uint32_t e, uint32_t dst, uint32_t n)
{
    uint8_t mode = E_MODE(e);
    uint32_t i, src;

    switch (mode) {
    case 0x0: case 0xa: case 0xb:
        break;
    case 0xc:
        if (M32(G + 0x1248) == 0) return;
        break;
    case 0xd:
        if (M32(G + 0x124c) == 0) return;
        break;
    case 0xe:
        if (M32(G + 0x1250) == 0) return;
        M32(S_KEEP_E) = 0;
        break;
    default:
        return;
    }
    src = M32(S_CURSOR);
    for (i = 0; i < n; i++)
        M8(dst + i) = M8(src + i);
}

/* .text+0x12513 -- the records, from +0x41e5 up to the end tag */
static void parse_records(void)
{
    uint32_t at, tag, length, hdr, body, e, base, dst, n, f;

    for (;;) {
        at = M32(S_CURSOR);
        tag = M16(at);
        at += 2;
        M32(S_REC) = tag;
        if (tag < 1000) {
            length = M16(at);
            hdr = 4;
        } else {
            length = M32(at);
            hdr = 6;
        }
        M32(S_REC + 4) = length;
        body = length - hdr;
        M32(S_REC + 8) = body;
        at = M32(S_CURSOR) + hdr;
        M32(S_CURSOR) = at;
        M32(S_REC + 0xc) = at;
        if (tag == 0xffff)
            return;
        for (e = S_TABLE; E_TAG(e) != tag; ) {
            if (E_TAG(e) == 0xffff)
                break;
            e = E_SIZE(e) == 0xffff ? E_NEXT(e) : e + 0x12;
        }
        if (E_TAG(e) != tag) {
            dispatch_record(S_REC);
            M32(S_CURSOR) = M32(S_CURSOR) + body;
            continue;
        }
        n = E_SIZE(e);
        dst = E_AT(e);
        switch (E_KIND(e)) {
        case 0:
            base = G;
            break;
        case 1:
            base = 0;
            break;
        case 5:
            base = M32(dst);
            dst = E_NEXT(e);
            break;
        case 2: case 3: case 4:
            base = 0;
            n = M32(S_REC + 8);
            break;
        default:
            base = 0;
            break;
        }
        if (E_SIZE(e) == 0xffff) {
            f = e + 0x12;
            while (f != E_NEXT(e)) {
                n = field_size(f);
                take(e, base + M32(f), n);
                M32(S_CURSOR) = M32(S_CURSOR) + n;
                f += 5;
            }
        } else {
            take(e, base + dst, n);
            M32(S_CURSOR) = M32(S_CURSOR) + n;
        }
        if (E_CALL(e) != 0)
            table_call(E_CALL(e), E_TAG(e), 0);
    }
}

/* The dialogs' callbacks main_install puts in .data, as retail's code addresses. */
static const struct { uint32_t at, to; } retail_callbacks[] = {
    { 0x68c, 0x207e3ec0 },
    { 0x123c, 0x207e5ff0 },
    { 0x1a4, 0x207e1910 },
    { 0x35ec, 0x207ee6b0 },
};

/* A byte as retail's process holds it at `a`: its own code, gap and .rdata
 * below .data, MAIN's image above, with its self-pointers and its Linkage
 * as retail's. */
static uint8_t retail_byte_at(uint32_t a)
{
    uint32_t off, i, v;

    if (a < 0x20800000u)
        return text_tail[a - SEASON_TABLE];
    off = a - MAIN_DATA_VA;
    if (off >= MAIN_LINKAGE && off < MAIN_LINKAGE + 4 * (sizeof retail_linkage / sizeof retail_linkage[0]))
        return (uint8_t)(retail_linkage[(off - MAIN_LINKAGE) / 4] >> 8 * ((off - MAIN_LINKAGE) % 4));
    for (i = 0; i < sizeof retail_callbacks / sizeof retail_callbacks[0]; i++)
        if (off >= retail_callbacks[i].at && off < retail_callbacks[i].at + 4)
            return (uint8_t)(retail_callbacks[i].to >> 8 * (off - retail_callbacks[i].at));
    for (i = 0; i < sizeof data_pointers / sizeof data_pointers[0]; i++) {
        uint32_t at = data_pointers[i].at;
        if (off < at || off >= at + 4)
            continue;
        v = M32(D + at);
        if (v - D < MAIN_IMAGE_SIZE)
            v = v - D + MAIN_DATA_VA;
        return (uint8_t)(v >> 8 * (off - at));
    }
    return M8(D + off);
}

/* .text+0x1309b -- the day of the year from the season, if no record gave
 * it (the season unbounded: past 3 it reads on through retail's code), the
 * year from the clock, then +0x74 */
static void set_season(void)
{
    uint16_t now[6];
    uint32_t a;

    if (M16(G + 0x23e) == 0) {
        a = SEASON_TABLE + 2 * (uint32_t)M16(G + 0x248);
        M16(G + 0x23e) = (uint16_t)(retail_byte_at(a) | retail_byte_at(a + 1) << 8);
        H_LOCAL_TIME(ADDR(now));
        M16(G + 0x240) = now[0];
    }
    MAIN_74(1, M16(G + 0x23e));
}

/* .text+0x12ecb -- six cloud fields: 0x7f0000 means 0x7fffff */
static void open_clouds(void)
{
    static const uint16_t fields[] = { 0xb7c, 0xb94, 0xb84, 0xb9c, 0xb8c, 0xba4 };
    uint32_t i;

    for (i = 0; i < 6; i++)
        if (M32(G + fields[i]) == 0x7f0000)
            M32(G + fields[i]) = 0x7fffff;
}

/* .text+0x12a9e -- after the records: what they leave set up is checked
 * and made consistent, the modules are told (list 0x14 hears 0x70 and
 * then 0x21) */
static uint32_t after_load(void)
{
    uint16_t w;

    M8(G + 0x332) = 2;
    w = M16(G + 0xbc8);
    if (w != 0)
        M16(G + 0xc00) = w;
    if (M32(G + 0x5f4) == 0xffffffffu)
        M32(G + 0x5f4) = 0;
    else if (M32(G + 0x5f4) >= 4)
        M32(G + 0x5f4) = 1;
    if (M16(G + 0xf8c) > 0x7fff || M16(G + 0xf8c) == 0)
        M16(G + 0xf8c) = 0x7fff;
    if (M16(G + 0x1074) > 0x7fff || M16(G + 0x1074) == 0)
        M16(G + 0x1074) = 0x7fff;
    if (M16(G + 0x115c) > 0x7fff || M16(G + 0x115c) == 0)
        M16(G + 0x115c) = 0x7fff;
    CHAIN_SEND(0x14, 0x70, 0);
    MAIN_A4();
    M16(G + 0xe62) = 1;
    CALL(CONTROLS, 0x28, f1)(0);
    CALL(CONTROLS, 0x2c, f1)(0);
    CALL(CONTROLS, 0x30, f1)(0);
    CALL(CONTROLS, 0x34, f1)(0);
    CALL(CONTROLS, 0x38, f2)(0, 0);
    CALL(CONTROLS, 0x38, f2)(0, 1);
    CALL(CONTROLS, 0x38, f2)(0, 2);
    if (M16(G + 0x372) > 100)
        M16(G + 0x372) = 100;
    if (M16(G + 0xc44) > 100)
        M16(G + 0xc44) = 100;
    CALL(CONTROLS, 0x48, f0)();
    if (M32(G + 0x1244) == 0)
        CALL(ACONTAIN, 0x30, f4)(G + 0x11e0, 1, S_AIRCRAFT, 0x104);
    if (H_STRLEN(S_AIRCRAFT) != 0)
        CALL(ACONTAIN, 0x28, f1)(S_AIRCRAFT);
    M32(G + 0x814) = 0;
    M32(G + 0x820) = 1;
    M32(G + 0x818) = 0;
    M32(G + 0x81c) = 0;
    M8(G + 0x364) = 0;
    CALL(CONTROLS, 0x24, f0)();
    if (M32(G + 0x850) == 0)
        M32(G + 0x84c) = 0;
    set_season();
    if (M32(G + 0x73c) == 2)
        M32(G + 0xb20) = 0;
    if (M16(G + 0x354) & 0x7777)
        M16(G + 0x354) = 0x1200;
    M16(G + 0x116c) = 1;
    if (M16(G + 0xe64) != 2 && M16(G + 0xe64) != 4)
        M16(G + 0xe64) = 2;
    if (M16(G + 0x5dc) != 0)
        H_POST(0x100c3, 0);
    else
        H_POST(0x100c2, 0);
    open_clouds();
    if (M32(G + 0x1214) == 2)
        M32(G + 0xb20) = M32(S_OLD_B20);
    M16(G + 0x11fa) = M16(G + 0x572);
    M32(G + 0x11fc) = M32(G + 0x574);
    M32(G + 0x11f0) = 1;
    M8(G + 0x365) = 0xf;
    return CHAIN_SEND(0x14, 0x21, 0);
}

/* .text+0x1295e -- the situation at +0x41e5 taken in.  Returns what the
 * last dispatch left in eax. */
static uint32_t load_records(void)
{
    M8(G + 0x366) = 0;
    before_load();
    parse_records();
    return after_load();
}

/* .text+0x13012 -- the flight `f` ({ ext, path }) as the one being loaded */
static void set_loading(uint32_t f)
{
    H_STRCPY(S_LOADING + 4, f + 4);
    M32(S_LOADING) = M32(f);
}

/* .text+0x136c0 -- the current flight is `f` */
static void set_flight(uint32_t f)
{
    H_STRCPY(S_FLIGHT + 4, f + 4);
    M32(S_FLIGHT) = M32(f);
}

/* .text+0x126ff -- the current flight is `path`, extension 2 */
static void set_flight_path(uint32_t path)
{
    H_STRCPY(S_FLIGHT + 4, path);
    M32(S_FLIGHT) = 2;
}

/* .text+0x12843 -- the current flight loaded from its file: 0, or the
 * open's failure */
static uint32_t load_current(void)
{
    uint32_t size, buf, got, r;

    set_loading(S_FLIGHT);
    r = H_FILE_OPEN(S_LOAD_FILE, S_LOADING + 4, M32(S_LOADING), 0);
    if (r != 0)
        return r;
    H_FILE_SIZE(M32(S_LOAD_FILE), ADDR(&size));
    H_ALLOC(1, size, ADDR(&buf), 1);
    H_FILE_READ(M32(S_LOAD_FILE), buf, size, ADDR(&got));
    H_FILE_CLOSE(M32(S_LOAD_FILE));
    M32(S_CURSOR) = buf;
    load_records();
    H_FREE(ADDR(&buf));
    return 0;
}

static uint32_t FS98_STDCALL load_default(void);

/* .text+0x127b3 -- +0x44: the current flight again; if it will not open,
 * an alert (string 0xf462) and the default flight */
static uint32_t FS98_STDCALL reload_flight(void)
{
    char text[0x100];

    if (load_current() == 0)
        return 0;
    H_LOADSTRING(0xf462, ADDR(text), 0x100);
    UTIL_ALERT(1, 1, ADDR(text), 0, 0, 0, 0, 0);
    return load_default();
}

/* .text+0x11eeb -- +0x38: the default flight, GLOBAL +0x24's, or the one
 * named by strings 0x9004 (the directory) and 0x9016.  With neither there
 * an alert (string 0xf461) and then nothing: retail spins at .text+0x1207c
 * for good, and so does this. */
static uint32_t FS98_STDCALL load_default(void)
{
    char dir[0x104], name[0x100], text[0x100];
    struct { uint32_t ext; char path[0x104]; } f;

    if (H_EXISTS(G + 0x24, 2) == 0) {
        set_flight_path(G + 0x24);
        return load_current();
    }
    H_LOADSTRING(0x9004, ADDR(dir), 0x104);
    H_LOADSTRING(0x9016, ADDR(name), 0x100);
    H_JOIN(ADDR(f.path), 0, ADDR(dir), ADDR(name));
    if (H_EXISTS(ADDR(f.path), 2) != 0) {
        H_LOADSTRING(0xf461, ADDR(text), 0x100);
        UTIL_ALERT(8, 1, ADDR(text), 0, 0, 0, 0, 0);
        for (;;)
            __asm__ volatile("");
    }
    f.ext = 2;
    set_flight(ADDR(&f));
    return load_current();
}

/* .text+0x11e3d -- +0x3c: the flight `f` ({ ext, path }), or the default
 * if it is not there; WEATHERM then starts afresh */
static uint32_t FS98_STDCALL load_flight(uint32_t f)
{
    set_loading(f);
    if (H_EXISTS(f + 4, M32(f)) == 0) {
        set_flight(f);
        reload_flight();
    } else {
        load_default();
    }
    return CALL(WEATHERM, 0x24, f0)();
}

/* .text+0x12f50 -- +0x48: a situation in memory */
static uint32_t FS98_STDCALL load_memory(uint32_t situation)
{
    M32(S_CURSOR) = situation;
    return load_records();
}

/* .text+0x12f6a -- +0x4c: `length` bytes of an open file */
static uint32_t FS98_STDCALL load_file(uint32_t file, uint32_t length)
{
    uint32_t buf, got;

    H_ALLOC(1, length, ADDR(&buf), 1);
    H_FILE_READ(file, buf, length, ADDR(&got));
    M32(S_CURSOR) = buf;
    load_records();
    return H_FREE(ADDR(&buf));
}
