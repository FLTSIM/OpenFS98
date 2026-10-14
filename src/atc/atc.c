/* ATC.DLL -- module id 24: the tower.
 *
 * Two jobs.  MAIN hands it an airport's ATIS record (+0x20) and it composes
 * the broadcast -- weather, runway, altimeter, visibility, cloud -- into
 * message templates it keeps in its own .data and puts the result up through
 * WINMSG.  And MAIN starts a takeoff (+0x18) or landing (+0x1c) clearance
 * exchange, which runs as a small state machine on CHAIN list 3, one step a
 * frame, posting tower messages and checking what the pilot did.
 *
 * Retail is mostly hand-written assembly, with the ATIS.XLT translator
 * (.text+0xba0 on) compiled C.  Transliterated from all 3,840 bytes; the
 * comments carry retail's addresses.  docs/modules/atc.md has the walk
 * through.  Before calling into it, know that:
 *
 *   - Everything lives in one .data image laid out exactly as retail's,
 *     ImportTable at +0x00 and Linkage at +0x40 included, and the code
 *     addresses it by retail's offsets.  Pointers *inside* the image stay as
 *     retail's absolute addresses (ATC_DATA_VA) and are translated on use.
 *     Heap pointers stored there are real.  That is a 32-bit layout.
 *   - +0x20 tests GLOBAL through the caller's EDI, which it never loads.
 *     MAIN's only call site has GLOBAL there; this reads it from ImportTable.
 *   - The translator's "copy a word" loop stops at the next space, not at the
 *     end of the string, so a message without a trailing space is copied on
 *     past its terminator.  Reproduced: the bytes it runs into are the same
 *     image bytes retail would run into.
 *   - +0x18/+0x1c register the step callback every time they are called, so
 *     two calls without the exchange finishing step it twice a frame.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

__declspec(dllimport) int __cdecl sscanf(const char *text, const char *format, ...);

#define ATC_DATA_VA   0x20643000u   /* retail .data */
#define ATC_DATA_SIZE 0x2000        /* .data's 0x1a00, to the end of its last page */

typedef uint16_t __attribute__((may_alias)) atc_u16;
typedef uint32_t __attribute__((may_alias)) atc_u32;

typedef void (FS98_STDCALL *atc_tick_fn)(uint32_t a, uint32_t b);

struct atc_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;
    void    *dialogs;                                   /* +0x14: none */
    void     (*takeoff)(void);                          /* +0x18 */
    void     (*landing)(void);                          /* +0x1c */
    void     (FS98_STDCALL *atis)(const uint8_t *record); /* +0x20 */
    void     (*clear)(void);                            /* +0x24 */
    uint32_t (*busy)(void);                             /* +0x28 */
};

struct atc_image {
    struct fs98_import import[8];       /* +0x000 ImportTable */
    struct atc_linkage linkage;         /* +0x040 Linkage */
    uint32_t           zero_6c;
    uint8_t            data[ATC_DATA_SIZE - 0x70]; /* +0x070 on */
};

extern struct atc_image atc_image;

typedef char (*atc_layout_check)[sizeof(struct atc_image) == ATC_DATA_SIZE &&
                                 sizeof(struct atc_linkage) == 0x2c ? 1 : -1];

/* ---- the image and its imports ---------------------------------------------- */

#define D           ((uint8_t *)&atc_image)
#define D8(o)       (*(uint8_t *)(D + (o)))
#define D16(o)      (*(atc_u16 *)(D + (o)))
#define D32(o)      (*(atc_u32 *)(D + (o)))
#define ATC_VA(o)   (ATC_DATA_VA + (uint32_t)(o))           /* offset -> retail address */
#define ATC_PTR(va) (D + ((uint32_t)(va) - ATC_DATA_VA))    /* retail address -> here */

#define HOST        ((void *)atc_image.import[0].link)
#define G           ((uint8_t *)atc_image.import[1].link)
#define G8(o)       (*(uint8_t *)(G + (o)))
#define G16(o)      (*(atc_u16 *)(G + (o)))
#define G32(o)      (*(atc_u32 *)(G + (o)))

#define SLOT(i, off, type) (*(type *)((uint8_t *)atc_image.import[i].link + (off)))

/* Every prototype read from the callee's `ret N`. */
typedef int32_t  (FS98_STDCALL *chain_add_fn)(uint32_t list, atc_tick_fn cb);         /* ret 8 */
typedef char    *(FS98_STDCALL *numeric_format_fn)(const void *value, uint32_t id,
                                                   char *out);                         /* ret 0xc */
typedef void     (FS98_STDCALL *util_unpack_fn)(const void *packed, void *vector);     /* ret 8 */
typedef void     (FS98_STDCALL *weather_at_fn)(void *position);                        /* ret 4 */
typedef void     (*weather_here_fn)(void);                                             /* ret */
typedef void     (FS98_STDCALL *winmsg_show_fn)(const char *text, uint32_t slot,
                                                uint32_t duration, uint32_t mode);     /* ret 0x10 */
typedef uint32_t (FS98_STDCALL *winmsg_slot_fn)(uint32_t slot);                        /* ret 4 */

#define CHAIN_ADD        SLOT(2, 0x18, chain_add_fn)
#define CHAIN_REMOVE     SLOT(2, 0x1c, chain_add_fn)
#define NUMERIC_FORMAT   SLOT(3, 0x18, numeric_format_fn)
#define UTIL_UNPACK      SLOT(4, 0x60, util_unpack_fn)
#define WEATHER_AT       SLOT(5, 0x18, weather_at_fn)
#define WEATHER_HERE     SLOT(5, 0x1c, weather_here_fn)
#define WINMSG_SHOW      SLOT(6, 0x18, winmsg_show_fn)
#define WINMSG_CLEAR     SLOT(6, 0x1c, winmsg_slot_fn)
#define WINMSG_ACTIVE    SLOT(6, 0x24, winmsg_slot_fn)

/* State, at retail's offsets. */
#define ATC_DELAY       D8(0x491)    /* frames left to wait; 0xff = not waiting */
#define ATC_STEP        D32(0x492)   /* state-machine step; 0 = idle */
#define ATC_MODE        D32(0x496)   /* 1 takeoff, 2 landing */
#define ATC_RETRIES     D32(0x49a)
#define ATC_WAITED      D32(0x49e)
#define ATC_REGISTERED  D32(0x4a2)   /* set on the first start, never cleared */
#define ATC_KEY_LENGTH  D32(0x96c)   /* translator: length of the key just matched */
#define ATC_LOADED      D32(0x970)   /* translator: sections read */
#define ATC_SECTION(i)  (*(char **)(D + 0x980 + (i) * 4))   /* 'A'..'Z', heap */
#define ATC_CURSOR      (*(const uint8_t **)(D + 0x9e8))    /* translator, heap */

/* ---- strings ------------------------------------------------------------------ */

/* .text+0x6a7 -- strcat(dst, src), returning the count copied; nothing reads it. */
static void atc_append(uint8_t *dst, const uint8_t *src)
{
    while (*dst != 0)
        dst++;
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
}

/* .text+0x696 -- ", " before anything but the first item.  ESI is the buffer. */
static void atc_separate(uint8_t *dst)
{
    if (*dst != 0)
        atc_append(dst, D + 0x72e);
}

/* .text+0x6d2 -- format `value` through NUMERIC and copy at most six
 * characters into a template.  With `blank`, leading zeroes become spaces, and
 * an all-zero field keeps a single '0' in its last place -- or, for an empty
 * field, in the byte before it.
 *
 * Retail formats into six bytes at [ebp-6], so a seven-byte result would write
 * its terminator over the saved EBP.  Every id ATC uses is fixed-width and at
 * most five characters, so it never does. */
static void atc_digits(const void *value, uint32_t id, uint8_t *dst, uint32_t blank)
{
    char text[8];
    uint32_t i = 0;
    uint8_t c;

    NUMERIC_FORMAT(value, id, text);
    do {
        c = (uint8_t)text[i];
        if (c == 0)
            break;
        if (c == '0' && blank != 0)
            c = ' ';
        else
            blank = 0;
        dst[i] = c;
        i++;
    } while (i <= 5);
    if (blank != 0)
        dst[i - 1] = '0';
}

/* ---- the translator (.text+0xba0 on, compiled C) --------------------------------
 *
 * ATIS.XLT is an INI file with a section per initial letter, A to Z, of
 * `phrase=replacement` lines.  When GLOBAL+0x128c asks for it and the file's
 * [TRANSLATOR] LocaleID is installed, every message goes through it on the way
 * out. */

/* .text+0xd20 -- also Linkage init. */
static void atc_load_translations(void)
{
    char section[2];
    char path[0x104];
    char locale[0xc];
    uint32_t lcid = 0;              /* retail: uninitialised if sscanf fails */
    uint32_t letter;

    *(atc_u16 *)section = D16(0x968);       /* "A" */
    FS98_HOST_LOADSTRING(HOST)(0x9052, path, 0x104);
    FS98_HOST_PATH_JOIN(HOST)(path, 0, path, (const char *)D + 0x95c);
    GetPrivateProfileStringA((const char *)D + 0x938, (const char *)D + 0x944,
                             (const char *)D + 0x950, locale, 9, path);
    sscanf(locale, (const char *)D + 0x934, &lcid);
    if (IsValidLocale(lcid, LCID_INSTALLED))
        G32(0x1290) = 1;
    else
        G32(0x1290) = 0;

    if (G32(0x128c) == 0) {
        for (letter = 0; letter < 26; letter++)     /* rep stosd; ATC_LOADED stays */
            ATC_SECTION(letter) = 0;
        return;
    }

    for (letter = 0; letter < 26; letter++) {
        uint32_t size = 0;
        int32_t got, i;

        section[0] = (char)('A' + letter);
        ATC_SECTION(letter) = 0;
        for (;;) {                  /* grow until the section fits */
            size += 0x400;
            FS98_HOST_ALLOC(HOST)(1, size, (void **)&ATC_SECTION(letter), 1);
            got = (int32_t)GetPrivateProfileSectionA(section, ATC_SECTION(letter),
                                                     size, path);
            if ((uint32_t)got != size - 2)
                break;
            FS98_HOST_FREE(HOST)((void **)&ATC_SECTION(letter));
        }
        /* "key=value" -> "key\0value" */
        for (i = 0; i < got; i++) {
            char *p = ATC_SECTION(letter) + i;
            if (*p == '=') {
                i++;
                *p = 0;
                i += (int32_t)FS98_HOST_STRLEN(HOST)(ATC_SECTION(letter) + i);
            }
        }
    }
    ATC_LOADED = 1;
}

/* .text+0xec0 -- Linkage deinit. */
static void atc_unload_translations(void)
{
    uint32_t letter;

    if (G32(0x128c) == 0)
        return;
    for (letter = 0; letter < 26; letter++)
        if (ATC_SECTION(letter) != 0)
            FS98_HOST_FREE(HOST)((void **)&ATC_SECTION(letter));
}

/* .text+0xc20 -- is there a key in s's letter section that `s` starts with?
 * Keys are compared against `s` upper-cased, not as words: "AB" matches
 * "ABC".  The first full match wins, and an empty key ends the search as if
 * the section had.  On a match, ATC_CURSOR is the replacement and ATC_KEY_LENGTH the
 * length of the key. */
static uint32_t atc_match(const uint8_t *s)
{
    const uint8_t *e, *found = 0;
    uint32_t pos = 0, run, best;
    uint8_t c = s[0], k;

    if (c >= 'a' && c <= 'z')
        c -= 0x20;
    if (c < 'A' || c > 'Z')
        return 0;
    e = (const uint8_t *)(uintptr_t)D32(0x87c + (uint32_t)c * 4);  /* ATC_SECTION(c - 'A') */
    ATC_CURSOR = e;
    if (*e == 0)
        return 0;
    best = 0;
    ATC_KEY_LENGTH = 0;
    k = *e;
outer:
    if (k == 0 || c == 0)
        goto done;
    run = 0;
    if (k != 0) {
        do {
            c = s[pos];
            if (c >= 'a' && c <= 'z')
                c -= 0x20;
            if (k != c) {
                /* past the rest of this key and its value */
                e++;
                ATC_CURSOR = e;
                while (*e != 0) {
                    e++;
                    ATC_CURSOR = e;
                }
                e++;
                ATC_CURSOR = e;
                while (*e != 0) {
                    e++;
                    ATC_CURSOR = e;
                }
                run = (uint32_t)-1;
                pos = (uint32_t)-1;
            }
            e++;
            pos++;
            ATC_CURSOR = e;
            run++;
            k = *e;
        } while (k != 0);
    }
    if (run != 0) {
        best = run;
        found = e;
        ATC_KEY_LENGTH = best;
    }
    k = *e;
    if (k != 0)
        goto outer;
done:
    if (best == 0)
        return 0;
    ATC_CURSOR = found + 1;
    return 1;
}

/* .text+0xba0 -- translate a message into the buffer at .data+0xa00. */
static char *atc_translate(const char *text)
{
    const uint8_t *s = (const uint8_t *)text;
    uint8_t *out = D + 0xa00;
    uint8_t c;

    if (ATC_LOADED == 0)
        atc_load_translations();
    while (*s != 0) {
        if (atc_match(s) == 1) {
            const uint8_t *v = ATC_CURSOR;
            while ((c = *v) != 0) {
                *out++ = c;
                v++;
                ATC_CURSOR = v;
            }
            s += ATC_KEY_LENGTH;
        } else {
            /* a word and the space after it -- to the next space, even past
             * the terminator */
            c = *s;
            if (c != ' ') {
                do {
                    *out++ = c;
                    c = s[1];
                    s++;
                } while (c != ' ');
            }
            *out++ = *s;
            s++;
        }
    }
    *out = 0;
    return (char *)(D + 0xa00);
}

/* ---- messages ---------------------------------------------------------------- */

/* .text+0x107 -- Linkage +0x24.  pusha/popa around it: preserves everything. */
static void atc_clear(void)
{
    WINMSG_CLEAR(2);
    WINMSG_CLEAR(1);
    WINMSG_CLEAR(3);
}

/* .text+0xb41 -- Linkage +0x28: is a tower message (slot 2) or an intro
 * (slot 1) still up? */
static uint32_t atc_busy(void)
{
    uint32_t tower = WINMSG_ACTIVE(2);
    return WINMSG_ACTIVE(1) | tower;
}

/* .text+0x17c -- expand a template into `dst` and show it.  A byte with the
 * top bit set is a macro: the string the table at .data+0x4a6 points to,
 * copied in raw. */
static void atc_expand(const uint8_t *src, uint8_t *dst, uint32_t slot)
{
    uint8_t *out = dst;
    const char *text;
    uint8_t c;

    *out = 0;
    for (;;) {
        c = *src;
        *out++ = c;
        src++;
        if (c & 0x80) {
            const uint8_t *m = ATC_PTR(D32(0x4a6 + (c & 0x7f) * 4));
            out--;
            while ((c = *m) != 0) {
                *out++ = c;
                m++;
            }
            continue;
        }
        if (c == 0)
            break;
    }
    text = (const char *)dst;
    if (G32(0x1290) != 0 && G32(0x128c) != 0)
        text = atc_translate(text);
    WINMSG_SHOW(text, slot, 600u / ((uint32_t)G16(0x362) + 1u), 1);
}

/* .text+0xadc -- the cloud layer at or above `*base` and at least `*cover`
 * thick, from GLOBAL's two layers; zero for none.  The first layer is taken
 * only below its top (`<`), the second also at it (`<=`).  Register
 * convention: AX, BX in and out, CX from .data+0x48d. */
static void atc_cloud_layer(uint16_t *base, uint16_t *cover)
{
    uint16_t cx = (uint16_t)D32(0x48d);

    if (cx < G16(0xea2) && *base <= G16(0xea4) && *cover <= G16(0xea6)) {
        *base = G16(0xea4);
        *cover = G16(0xea6);
        return;
    }
    if (cx <= G16(0xe9a) && *base <= G16(0xe9c) && *cover <= G16(0xe9e)) {
        *base = G16(0xe9c);
        *cover = G16(0xe9e);
        return;
    }
    *base = 0;
    *cover = 0;
}

/* .text+0x5db -- a ceiling height above the field, into the five characters
 * at .data+0x6ee, then appended to `dst`.  Under 31 units it is " <10". */
static void atc_ceiling(uint8_t *dst, uint16_t height)
{
    uint32_t value, id = 6, offset = 1;
    const char *t;
    uint8_t lo, hi;

    value = (uint32_t)height - (G32(0x20) >> 8);
    if ((int32_t)value < 0)
        value = 0;
    if (G16(0xc18) == 2) {
        id = 0;
        offset = 0;
    }
    if (value <= 0x1e) {
        D32(0x6ee) = 0x30313c20;            /* " <10" */
    } else {
        t = NUMERIC_FORMAT(&value, id, 0) + offset;   /* NUMERIC's own buffer */
        lo = (uint8_t)t[0];
        hi = (uint8_t)t[1];
        if (lo == '0') {
            lo = ' ';
            if (hi == '0')
                hi = ' ';
        }
        D8(0x6ee) = lo;
        D8(0x6ef) = hi;
        lo = (uint8_t)t[2];
        hi = (uint8_t)t[3];
        if (offset != 0)
            hi = '0';
        D8(0x6f0) = lo;
        D8(0x6f1) = hi;
    }
    atc_append(dst, D + 0x6ee);
}

/* .text+0x21b -- fill the ATIS templates from the weather at the airport.
 * WEATHERM computes for .data+0x479 and is put back on the aircraft after. */
static void atc_compose_atis(void)
{
    uint32_t value, ceiling_said, layers, id, k;
    uint16_t layer_base, base, cover;
    uint8_t *sky = D + 0x5e9;
    uint8_t unit, runway;
    int32_t metres;

    WEATHER_AT(D + 0x479);

    id = 0x27;
    unit = 'F';
    if (G16(0xc18) != 0) {
        id = 0x26;
        unit = 'C';
    }
    D8(0x533) = unit;
    atc_digits(G + 0xe8c, id, D + 0x52f, 0);       /* temperature */
    atc_digits(G + 0xef2, 0x4c, D + 0x5b1, 0);     /* wind direction */
    value = G16(0xef0);
    k = ATC_VA(0x5bf);                              /* " Winds calm -" */
    if (value != 0) {
        k = ATC_VA(0x5cd);                          /* " Winds light and variable -" */
        if (value > 5)
            k = ATC_VA(0x5aa);                      /* " Winds 330 at  27 - " */
    }
    D32(0x4ce) = k;                                 /* macro 0x8a */
    atc_digits(&value, 0x21, D + 0x5b8, 1);        /* wind speed */
    value = G8(0x23b);
    atc_digits(&value, 0x20, D + 0x515, 0);        /* the hour */

    /* cloud, into .data+0x5e9 (macro 0x8b) */
    D8(0x5e9) = 0;
    layer_base = 0;
    ceiling_said = 0;
    layers = 0;
    for (;;) {
        base = (uint16_t)(layer_base + 1);
        cover = 0x100;
        atc_cloud_layer(&base, &cover);
        if (base == 0)
            break;
        layers++;
        layer_base = base;
        if (cover > 0xe001) {
            if (ceiling_said == 0) {
                ceiling_said = 1;
                atc_separate(sky);
                atc_append(sky, D + 0x6db);         /* " Measured ceiling " */
                atc_ceiling(sky, layer_base);
                atc_append(sky, D + 0x6fc);         /* "overcast" */
            } else {
                atc_separate(sky);
                atc_append(sky, D + 0x718);         /* "Overcast " */
                atc_ceiling(sky, layer_base);
            }
            goto storms;                            /* nothing shows above overcast */
        }
        if (cover > 0x9000) {
            if (ceiling_said == 0) {
                ceiling_said = 1;
                atc_separate(sky);
                atc_append(sky, D + 0x6db);
                atc_ceiling(sky, layer_base);
                atc_append(sky, D + 0x6f5);         /* "broken" */
            } else {
                atc_separate(sky);
                atc_append(sky, D + 0x710);         /* "Broken " */
                atc_ceiling(sky, layer_base);
            }
            continue;
        }
        atc_separate(sky);
        atc_append(sky, D + 0x705);                 /* "Scattered " */
        atc_ceiling(sky, layer_base);
    }
    if (layers == 0)
        atc_append(sky, D + 0x722);                 /* "Skies clear" */
storms:
    if (G16(0xeaa) != 0)
        atc_append(sky, D + 0x6b5);                 /* ", Thunderstorms observed..." */
    atc_append(sky, D + 0x731);                     /* " - " */

    /* runway: the record's byte for the wind's quadrant -- number in the low
     * six bits, " LRC" suffix in the top two.  `rol bx,2 / and bx,3`. */
    runway = D8(0x471 + (G16(0xef2) >> 14));
    D8(0x566) = D8(0x735 + (runway >> 6));
    value = runway & 0x3f;
    atc_digits(&value, 0x20, D + 0x564, 0);
    D8(0x58e) = ' ';
    atc_digits(G + 0xec6, 0x3b, D + 0x58a, 0);     /* altimeter */

    /* visibility */
    value = G16(0xe8a);
    if (G16(0xea4) == 0)
        value = 0xffff;
    if (G16(0xc18) == 0) {
        k = 0;
        while ((uint16_t)value > D16(0x8b3 + k))    /* ends at 0xffff */
            k += 6;
        D32(0x59f) = D32(0x8b5 + k);
        D8(0x5a3) = 'm';
        D8(0x5a4) = 'i';
    } else {
        metres = (int32_t)((int64_t)(int32_t)value * (int32_t)D32(0x92b) /
                           (int32_t)D32(0x92f));    /* imul / idiv */
        if ((uint32_t)metres >= 10000) {
            D8(0x59f) = ' '; D8(0x5a0) = '>'; D8(0x5a1) = '1'; D8(0x5a2) = '0';
            D8(0x5a3) = 'k'; D8(0x5a4) = 'm'; D8(0x5a5) = ' '; D8(0x5a6) = ' ';
        } else {
            uint8_t *dst;
            if ((uint32_t)metres >= 5000) {
                value = ((uint32_t)metres + 500) / 1000;
                D32(0x5a3) = 0x20206d6b;            /* "km  " */
                id = 0x1f;
                dst = D + 0x5a1;
            } else {
                value = ((uint32_t)metres + 50) / 100 * 100;
                D32(0x5a3) = 0x2020206d;            /* "m   " */
                id = 0x22;
                dst = D + 0x59f;
            }
            D32(0x59f) = 0x20202020;
            atc_digits(&value, id, dst, 1);
        }
    }
    WEATHER_HERE();
}

/* .text+0x0c6 -- clear what is up, refresh the ATIS pieces, and show
 * `template` in `slot` -- unless GLOBAL+0xb6a or +0xb68 is exactly 1. */
static void atc_say(const uint8_t *template, uint32_t slot)
{
    if (G8(0xb6a) == 1 || G8(0xb68) == 1)
        return;
    atc_clear();
    atc_compose_atis();
    atc_expand(template, D + 0x70, slot);
}

/* .text+0x038 -- Linkage +0x20: an airport's ATIS record.  +0x03 is its packed
 * position, +0x0c four runway bytes, +0x10 the broadcast template.
 *
 * Retail reads +0xb04 and +0xb68 through the caller's EDI, never loaded here;
 * MAIN, the only caller, has GLOBAL in it. */
static void FS98_STDCALL atc_atis(const uint8_t *record)
{
    uint32_t ground;

    if ((G16(0xb04) & 8) == 0)
        return;
    if (G8(0xb68) != 0)
        return;
    D32(0x471) = *(const atc_u32 *)(record + 0xc);
    UTIL_UNPACK(record + 3, D + 0x479);
    ground = G32(0x20);                 /* over the vector's last six bytes */
    D16(0x48b) = (uint16_t)((ground & 0xff) << 8);
    D32(0x48d) = ground >> 8;
    atc_say(record + 0x10, 3);
}

/* ---- the clearance exchange ------------------------------------------------------ */

static void FS98_STDCALL atc_tick(uint32_t a, uint32_t b);

/* .text+0xa79 -- tower message n in slot 2, if nothing is up.  Returns what
 * atc_busy said, so 0 means it went out. */
static uint32_t atc_post(uint32_t n)
{
    uint32_t busy = atc_busy();

    if (busy == 0) {
        atc_clear();
        atc_say(ATC_PTR(D32(0x7c4 + n * 4)), 2);
    }
    return busy;
}

/* .text+0xa5d -- post, and move on if it went out. */
static void atc_post_and_step(uint32_t n)
{
    if (atc_post(n) == 0)
        ATC_STEP++;
}

/* .text+0x85b -- step: done.  Unregisters whenever it has ever registered,
 * so a second finish removes a callback that is no longer there. */
static void atc_finish(void)
{
    ATC_STEP = 0;
    if (ATC_REGISTERED != 0)
        CHAIN_REMOVE(3, atc_tick);
}

/* .text+0x89f -- step: wait for the screen to clear. */
static void atc_wait_quiet(void)
{
    if (atc_busy() == 0)
        ATC_STEP++;
}

/* .text+0x8b2 and .text+0x8dc -- step: wait `reload - 1` frames. */
static void atc_delay(uint8_t reload)
{
    uint8_t v = ATC_DELAY;

    if (v == 0)
        ATC_STEP++;
    else if (v == 0xff)
        v = reload;
    ATC_DELAY = (uint8_t)(v - 1);
}

static void atc_delay_short(void) { atc_delay(2); }
static void atc_delay_long(void)  { atc_delay(10); }

/* .text+0xaa5 -- pick a squawk: "02" and two octal digits from GLOBAL. */
static void atc_pick_squawk(void)
{
    uint16_t ax;

    D16(0x6af) = 0x3230;
    ax = (uint16_t)((uint16_t)G32(0x2ac) | G16(0x2aa));
    ax &= 0x707;
    ax |= 0x3030;
    D16(0x6b1) = ax;
}

/* .text+0x906 -- takeoff step 4: taxi instructions and the squawk.
 * Posted with atc_post, not atc_post_and_step: it steps regardless. */
static void atc_taxi(void)
{
    atc_pick_squawk();
    atc_post(1);
    ATC_STEP++;
    ATC_WAITED = 0;
    ATC_RETRIES = 0;
}

/* .text+0x92d -- takeoff step 5: wait for GLOBAL+0x2b8, for 30 quiet frames
 * at most. */
static void atc_wait_ready(void)
{
    if (G32(0x2b8) != 0)
        ATC_STEP++;
    else if (ATC_WAITED >= 0x1e)
        ATC_STEP += 2;
    else if (atc_busy() == 0)
        ATC_WAITED++;
}

/* .text+0x970 -- takeoff step 6. */
static void atc_check_ready(void)
{
    if (G8(0x366) == 0 || G32(0x2b8) == 0)
        ATC_STEP++;
}

/* .text+0x991 -- takeoff step 7: is the transponder (BCD at GLOBAL+0x354) on
 * the squawk?  Yes skips the reminder; no posts it. */
static void atc_check_squawk(void)
{
    uint16_t ax = D16(0x6af), bx = D16(0x6b1);
    uint8_t ah = (uint8_t)(ax >> 8), bl = (uint8_t)bx;

    ax = (uint16_t)((ax & 0x00ff) | (bl << 8));     /* xchg bl,ah */
    bx = (uint16_t)((bx & 0xff00) | ah);
    ax &= 0xf0f;
    bx &= 0xf0f;
    ax = (uint16_t)(ax << 4);
    ax |= bx;
    ax = (uint16_t)((ax >> 8) | (ax << 8));         /* xchg ah,al */
    if ((uint16_t)(ax ^ G16(0x354)) == 0) {
        ATC_STEP += 4;
        return;
    }
    atc_post(5);
    ATC_STEP++;
}

/* .text+0x9ec -- takeoff step 10: back to step 7, three times. */
static void atc_retry(void)
{
    ATC_RETRIES++;
    if (ATC_RETRIES >= 3)
        ATC_STEP++;
    else
        ATC_STEP -= 3;
}

/* .text+0xa55 -- takeoff step 12. */
static void atc_cleared_takeoff(void) { atc_post_and_step(2); }

/* .text+0xa31 -- takeoff step 3: already airborne? */
static void atc_check_airborne(void)
{
    ATC_STEP++;
    if (G8(0x366) == 0) {
        atc_post_and_step(6);
        atc_finish();
    }
}

/* .text+0xa0b -- landing step 3: already down? */
static void atc_check_landed(void)
{
    ATC_STEP++;
    if (G8(0x366) != 0) {
        atc_post_and_step(7);
        atc_finish();
    }
}

/* .text+0xad4 -- landing step 4. */
static void atc_cleared_to_land(void) { atc_post_and_step(3); }

/* .data+0x739 and +0x771, which hold the retail addresses noted; the
 * generator checks them.  Entry 0 is never called. */
static void (*const atc_takeoff_steps[14])(void) = {
    atc_finish,             /* 0x2064185b */
    atc_wait_quiet,         /* 0x2064189f */
    atc_delay_short,        /* 0x206418b2 */
    atc_check_airborne,     /* 0x20641a31 */
    atc_taxi,               /* 0x20641906 */
    atc_wait_ready,         /* 0x2064192d */
    atc_check_ready,        /* 0x20641970 */
    atc_check_squawk,       /* 0x20641991 */
    atc_wait_quiet,         /* 0x2064189f */
    atc_delay_long,         /* 0x206418dc */
    atc_retry,              /* 0x206419ec */
    atc_delay_short,        /* 0x206418b2 */
    atc_cleared_takeoff,    /* 0x20641a55 */
    atc_finish,             /* 0x2064185b */
};

static void (*const atc_landing_steps[6])(void) = {
    atc_finish,             /* 0x2064185b */
    atc_wait_quiet,         /* 0x2064189f */
    atc_delay_short,        /* 0x206418b2 */
    atc_check_landed,       /* 0x20641a0b */
    atc_cleared_to_land,    /* 0x20641ad4 */
    atc_finish,             /* 0x2064185b */
};

/* .text+0x7ff -- the CHAIN list 3 callback, once a frame.  GLOBAL+0x27c
 * going to zero abandons the exchange.  Every step either finishes or moves
 * within its table, so ATC_STEP never indexes past one. */
static void FS98_STDCALL atc_tick(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    if (G32(0x27c) == 0) {
        ATC_MODE = 0;
        ATC_STEP = 0;
        return;
    }
    if ((uint32_t)(ATC_STEP << 2) == 0)
        return;
    if (ATC_MODE < 1)
        return;
    if (ATC_MODE == 1)
        atc_takeoff_steps[ATC_STEP]();
    else
        atc_landing_steps[ATC_STEP]();
}

/* .text+0x79f -- the mode's request ("Microsoft Flight Simulator requesting
 * takeoff clearance... "), then step 1 every frame. */
static void atc_start(void)
{
    uint32_t request;

    if ((uint32_t)(ATC_MODE << 2) == 0)
        return;
    request = D32(0x789 + ATC_MODE * 4);
    ATC_STEP = 1;
    ATC_REGISTERED = 1;
    atc_say(ATC_PTR(request), 1);
    CHAIN_ADD(3, atc_tick);
}

/* .text+0x76f -- Linkage +0x18. */
static void atc_takeoff(void)
{
    G32(0x27c) = 1;
    ATC_MODE = 1;
    ATC_STEP = 0;
    atc_start();
}

/* .text+0x73f -- Linkage +0x1c. */
static void atc_landing(void)
{
    G32(0x27c) = 1;
    ATC_MODE = 2;
    ATC_STEP = 0;
    atc_start();
}

/* ---- the module ---------------------------------------------------------------- */

struct atc_image atc_image = {
    {
        { FS98_MOD_HOST,     0 },
        { FS98_MOD_GLOBAL,   0 },
        { FS98_MOD_CHAIN,    0 },
        { FS98_MOD_NUMERIC,  0 },
        { FS98_MOD_UTIL,     0 },
        { FS98_MOD_WEATHERM, 0 },
        { FS98_MOD_WINMSG,   0 },
        { 0, 0 }
    },
    {
        FS98_MOD_ATC, atc_load_translations, atc_unload_translations, 0, 0, 0,
        atc_takeoff, atc_landing, atc_atis, atc_clear, atc_busy
    },
    0,
    {
#include "atc_data.inc"
    }
};

/* The two exports are the image's first 0x40 bytes and the 0x2c at +0x40,
 * where retail has them. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _atc_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _atc_image + 0x40");

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
