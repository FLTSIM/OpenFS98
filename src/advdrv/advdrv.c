/* ADVDRV.DLL -- module id 21: the adventure and lesson interpreter.
 *
 * An adventure (.ADV, in ADV\) or a lesson (.LES, in LESSONS\) is a program
 * compiled by Microsoft's APL tools: a 0x200-byte header ("FS6X Adventure",
 * a version, a CRC32 of the rest, a string count, the size of the code and
 * variable space, and how far to scan for sounds), then bytecode.  +0x1c
 * loads one; the frame callback on CHAIN list 5 (or 1..3, as the program
 * asks) then runs up to three instructions a frame.  Instructions jump,
 * call, loop, wait (for a time, a key or a message to be acknowledged), show
 * text through WINMSG, play sounds through the host's voice engine, set the
 * aircraft's position and the weather in GLOBAL, load situations through
 * MAIN, play videos through DEMO and write to a debug console.  Expressions
 * are a stack machine over doubles: constants, the program's own variables
 * (doubles in the code buffer) and 176 system variables, each a descriptor
 * with a getter and a setter over GLOBAL.  Events evaluate an expression
 * each frame and call a handler on true, on an edge or on a change.
 *
 * Retail is MSVC C with the x87 throughout; this is a transliteration.  The
 * expressions are evaluated as retail evaluates them: in long double where
 * retail keeps a value on the x87 stack, rounded to double where it stores
 * one, comparisons through the x87's own condition codes (so NaN behaves as
 * retail's branches make it), and MSVCRT's sprintf, _CIfmod, _CIasin and
 * modf called for real.  The complete 0x32a0-byte .data image is kept at
 * retail's offsets, with its tables of code pointers filled at load from the
 * list tools/gen_advdrv_data.py extracts.  Comments carry retail's .text
 * offsets.  docs/modules/advdrv.md describes the format and the variables.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define ADVDRV_IMAGE_SIZE 0x32a0
#define ADVDRV_IMPORTS    0x000
#define ADVDRV_LINKAGE    0x048
#define ADVDRV_DATA_VA    0x2061a000u

typedef long double ld;
typedef uint16_t __attribute__((may_alias)) adv_u16;
typedef uint32_t __attribute__((may_alias)) adv_u32;
typedef int16_t __attribute__((may_alias)) adv_s16;
typedef int32_t __attribute__((may_alias)) adv_s32;
typedef double __attribute__((may_alias)) adv_f64;

struct advdrv_image {
    uint8_t bytes[ADVDRV_IMAGE_SIZE];
} __attribute__((aligned(16)));

extern struct advdrv_image advdrv_image;

#define D             (advdrv_image.bytes)
#define D8(o)         (*(uint8_t *)(D + (o)))
#define D16(o)        (*(adv_u16 *)(D + (o)))
#define D32(o)        (*(adv_u32 *)(D + (o)))
#define DS32(o)       (*(adv_s32 *)(D + (o)))
#define DF64(o)       (*(adv_f64 *)(D + (o)))
#define DA(va)        (D + ((va) - ADVDRV_DATA_VA))   /* a retail .data address */

#define P8(p, o)      (*(uint8_t *)((uint8_t *)(p) + (o)))
#define P16(p, o)     (*(adv_u16 *)((uint8_t *)(p) + (o)))
#define PS16(p, o)    (*(adv_s16 *)((uint8_t *)(p) + (o)))
#define P32(p, o)     (*(adv_u32 *)((uint8_t *)(p) + (o)))
#define PS32(p, o)    (*(adv_s32 *)((uint8_t *)(p) + (o)))
#define PF64(p, o)    (*(adv_f64 *)((uint8_t *)(p) + (o)))
#define PTR(v)        ((uint8_t *)(uintptr_t)(uint32_t)(v))
#define U32(p)        ((uint32_t)(uintptr_t)(p))

#define IMPORT(i)     D32(ADVDRV_IMPORTS + 8 * (i) + 4)
#define HOST          IMPORT(0)
#define G             PTR(IMPORT(1))
#define CHAIN         IMPORT(2)
#define DEMO          IMPORT(3)
#define MAIN          IMPORT(4)
#define SOUND         IMPORT(5)
#define UTIL          IMPORT(6)
#define WINMSG        IMPORT(7)

#define G8(o)         P8(G, o)
#define G16(o)        P16(G, o)
#define GS16(o)       PS16(G, o)
#define G32(o)        P32(G, o)
#define GS32(o)       PS32(G, o)

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *f8)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f9)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *fd)(double);
typedef uint32_t (FS98_STDCALL *fplay)(uint32_t, uint32_t, double, uint32_t);
typedef ld (FS98_STDCALL *fld0)(void);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_TICKS      HOST_(0x038, f0)
#define H_COMMAND    HOST_(0x040, f2)    /* SendMessageA(main, WM_COMMAND, a, b) */
#define H_ROOT       HOST_(0x060, f0)    /* the sim's directory */
#define H_CFG_INT    HOST_(0x0a8, f3)    /* (section, key, &value) */
#define H_CFG_STRING HOST_(0x0c8, f4)    /* (section, key, buf, size) */
#define H_DIALOG     HOST_(0x10c, f2)    /* (id, param) */
#define H_SET_ATTR   HOST_(0x138, f3)    /* (flags, path, ext) */
#define H_CLOSE      HOST_(0x13c, f1)    /* (file) */
#define H_COPY       HOST_(0x144, f4)    /* (to, toext, from, fromext) */
#define H_DELETE     HOST_(0x14c, f2)    /* (path, ext) */
#define H_EXISTS     HOST_(0x158, f2)    /* (path, ext) */
#define H_OPEN       HOST_(0x17c, f4)    /* (&file, path, ext, mode) */
#define H_JOIN       HOST_(0x184, f4)    /* (out, drive, dir, name) */
#define H_SPLIT      HOST_(0x188, f5)    /* (path, drive, dir, name, &ext) */
#define H_READ       HOST_(0x19c, f4)    /* (file, buf, n, &got) */
#define H_SIZE       HOST_(0x1a4, f2)    /* (file, &size) */
#define H_KEYS_NEW   HOST_(0x1d8, f1)    /* (&map) */
#define H_KEYS_FREE  HOST_(0x1dc, f1)    /* (&map) */
#define H_KEYS_ON    HOST_(0x1e8, f1)    /* (map) */
#define H_KEYS_ADD   HOST_(0x1f4, f3)    /* (map, bindings, count) */
#define H_KEYS_DROP  HOST_(0x1f8, f3)    /* (map, bindings, count) */
#define H_ALLOC      HOST_(0x20c, f4)    /* (flags, size, &p, zero) */
#define H_REALLOC    HOST_(0x210, f3)    /* (flags, size, &p) */
#define H_FREE       HOST_(0x214, f1)    /* (&p) */
#define H_MEMSET     HOST_(0x218, f3)
#define H_MEMMOVE    HOST_(0x21c, f3)    /* (dst, src, n) */
#define H_LOADSTRING HOST_(0x2a0, f3)    /* (id, buf, size) */
#define H_STRCPY     HOST_(0x2a8, f2)
#define H_STRCAT     HOST_(0x2b0, f2)
#define H_STRLEN     HOST_(0x2b8, f1)
#define H_STRCMP     HOST_(0x2bc, f2)
#define H_SOUND_ON   HOST_(0x2cc, f0)    /* 1 when the voice engine is on */
#define H_VOICE_LOAD HOST_(0x2d4, f3)    /* (src, type, &h) */
#define H_VOICE_ADD  HOST_(0x2d8, f4)    /* (src, type, dst, final) */
#define H_VOICE_FREE HOST_(0x2dc, f1)    /* (&h) */
#define H_VOICE_PLAY HOST_(0x2e0, fplay) /* (h, loop, freq, done) */
#define H_VOICE_VOL  HOST_(0x2ec, f2)    /* (h, volume) */
#define H_VOICE_STOP HOST_(0x2f4, f1)    /* (h) */

#define CHAIN_ADD    CALL(CHAIN, 0x18, f2)   /* (list, callback) */
#define CHAIN_REMOVE CALL(CHAIN, 0x1c, f2)
#define DEMO_PLAY    CALL(DEMO, 0x28, f2)    /* (name, dialog at the end) */
#define MAIN_SITUATE CALL(MAIN, 0x3c, f1)    /* (&{ext, path}) */
#define MAIN_RANGE   CALL(MAIN, 0x70, f4)    /* (from, to, &bearing, &range) */
#define MAIN_CLOCK   CALL(MAIN, 0x74, f2)    /* (1, word) */
#define MAIN_DME     CALL(MAIN, 0x80, f2)    /* (which, &tenths) */
#define MAIN_TIMEOFDAY CALL(MAIN, 0xa4, f0)
#define SOUND_EVENT  CALL(SOUND, 0x24, f2)   /* (code, arg) */
#define SOUND_VOLUME CALL(SOUND, 0x30, f1)   /* (which) */
#define UTIL_METRES  CALL(UTIL, 0x48, f3)    /* (lon48, at, out48) */
#define UTIL_LIST    CALL(UTIL, 0x98, f9)    /* the catalogue */
#define UTIL_ALERT   CALL(UTIL, 0xa4, f8)
#define UTIL_OPEN    CALL(UTIL, 0xa8, f8)    /* a modeless alert */
#define UTIL_CLOSE   CALL(UTIL, 0xac, f1)
#define UTIL_ROUND   CALL(UTIL, 0xb0, fd)    /* fld/fistp: rounds */
#define UTIL_LOW     CALL(UTIL, 0xb4, fd)    /* the low half of an int64 */
#define UTIL_FRAND   CALL(UTIL, 0xb8, fld0)
#define WINMSG_SHOW  CALL(WINMSG, 0x18, f4)  /* (text, slot, time, mode) */
#define WINMSG_CLEAR CALL(WINMSG, 0x20, f0)

/* ---- the state, at retail's offsets ---------------------------------------- */

#define A_LIST       D32(0x07c)     /* the CHAIN list the frame runs on */
#define A_WAVDIR     ((char *)D + 0x080)    /* "fs98\adv\wav" */
#define A_WAVE       ((char *)D + 0x090)    /* "WAVE" */
#define A_APL        ((char *)D + 0x098)    /* "APL", the FLTSIM98.CFG section */
#define A_MAX_EVENTS ((char *)D + 0x09c)
#define A_MAGIC_OLD  ((char *)D + 0x0a8)    /* "FS6 Adventure\x1a" */
#define A_MAGIC      ((char *)D + 0x0b8)    /* "FS6X Adventure\x1a" */
#define A_STEPS      D32(0x0c8)     /* instructions a frame */
#define A_MSG_TIME   D32(0x0cc)     /* a message's time, in 1/18 s */
#define A_HISTORY    D32(0x0d0)     /* -> the position two frames ago */
#define A_OPCODES    0x0d8          /* 0x2c instruction handlers */
#define A_NEWLINE    ((char *)D + 0x188)
#define A_BACKSLASH  ((char *)D + 0x18c)
#define A_NUMBER     ((char *)D + 0x190)    /* "%0*.*f" */
#define A_OPERATORS  0x198          /* (id, handler) pairs to +0x318 */
#define A_VARS       0x318          /* 0xb0 descriptors of 0x14 bytes */
#define A_ERRORS     0x10d8         /* 8 message string ids */
#define A_WEATHER    0x10f8         /* 7 weather instructions */
#define A_CLOUDS     0x1118         /* 11 cloud types, 18 bytes each */
#define A_VISIBILITY 0x11e0         /* 19 visibilities, to +0x1206 */
#define A_COVER      0x1208         /* cover by octas */
#define A_COVER_10   0x1220         /* cover for type 10 */
#define A_EVENTS_MAX D32(0x1228)    /* MAX_EVENTS, 200 */
#define A_SKIP       0x1230         /* 0x2c instruction skippers */
#define A_CACHE_KEY  ((char *)D + 0x12e0)   /* "CACHE_WAV_FILES" */
#define A_CRC        D32(0x12f0)    /* the header's */
#define A_GOT        D32(0x12f4)    /* bytes read */
#define A_STEPPED    D32(0x12f8)    /* instructions run this frame */
#define A_DIGITS     D32(0x12fc)    /* the number format: integer digits */
#define A_TEXT       ((char *)D + 0x1300)   /* 0x400 */
#define A_MSG        D32(0x1700)    /* the message WINMSG returned */
#define A_PLAIN      D32(0x1704)    /* the text ended in a 1: no rubout */
#define A_WAIT       D32(0x1708)    /* 1 time, 2 key, 3 either, 4 message */
#define A_DECIMALS   D32(0x170c)
#define A_PAD        D32(0x1710)
#define A_WAIT_SAVED D32(0x1714)
#define A_UNTIL      D32(0x1718)    /* ticks */
#define A_TOGGLE6    D32(0x171c)
#define A_WAIT_MSG   D32(0x1720)
#define A_1724       D32(0x1724)
#define A_STACK      0x1728         /* 50 (return pc, from an event) */
#define A_TOGGLE5    D32(0x18b8)
#define A_DEPTH      D32(0x18bc)
#define A_CONSOLE_ON D8(0x18c0)
#define A_UNTIL_SAVED D32(0x18c4)
#define A_POSITIONS  0x18c8         /* now, last frame, the one before */
#define A_RUNNING    D32(0x18f8)
#define A_CODE       D32(0x18fc)    /* the code and variable space */
#define A_PC         D32(0x1900)
#define A_SOUTH      D32(0x1904)    /* latitudes count south */
#define A_EAST       D32(0x1908)    /* longitudes count east */
#define A_STRINGS    D32(0x190c)
#define A_SKIP21     D32(0x1910)    /* the next list 0x14 message 0x21 is ours */
#define A_KEYMAP     D32(0x1914)
#define A_CONSOLE    D32(0x1918)
#define A_DIALOG_EXT D32(0x191c)
#define A_LAYER(o)   D16(o)         /* weather layer ids, +0x1928..+0x1950 */
#define A_NO_SOUND   D32(0x1954)
#define A_VOICE      D32(0x1958)
#define A_VOICE_NEXT D32(0x195c)
#define A_ALERT      D32(0x1960)
#define A_DONE       D32(0x1964)    /* deinitialised */
#define A_EVENTS     D32(0x1968)
#define A_EV_HEAD    D32(0x196c)
#define A_EV_TAIL    D32(0x1970)
#define A_EV_CURSOR  D32(0x1974)
#define A_CACHE      D32(0x1980)    /* 1 copies sounds to the cache, 0 deletes */
#define A_CACHE_DIR  ((char *)D + 0x19a0)   /* the CD's fs98\adv\wav */
#define A_ALERT_TICKS D32(0x1aa0)
#define A_SOUND_DIR1 ((char *)D + 0x1ac0)   /* string 0x900d */
#define A_SOUND_DIR2 ((char *)D + 0x1bc0)   /* [APL] WAVE= */
#define A_SOUND_DIR3 ((char *)D + 0x1cc0)   /* string 0x900e */
#define A_SOUND_DIR0 ((char *)D + 0x1dc0)   /* the program's own */
#define A_KEYS       0x1ec0         /* 0x800 bound keys */
#define A_OPERATE    0x26c0         /* 0x33 operator handlers */
#define A_SP         D32(0x278c)
#define A_OPERANDS   0x27a0         /* doubles */
#define A_START      DF64(0x2ac0)   /* ticks when the program started */
#define A_SOUNDS_ON  D32(0x2ac8)
#define A_ON_KEY     DS32(0x2acc)   /* handler while GLOBAL +0x36c is 1 */
#define A_KEY        D32(0x2ad0)    /* the key pressed */
#define A_BINDINGS   0x2ae0         /* 20 (key, handler) */
#define A_IN_HANDLER D32(0x2b80)
#define A_ON_832     DS32(0x2b84)   /* handler when GLOBAL +0x832 is set */
#define A_LAST_KEY   D32(0x2b88)
#define A_MESSAGES_ON D32(0x2b8c)
#define A_LESSON     D32(0x2b90)
#define A_HANDLER    DS32(0x2b94)   /* the bound key's handler, pending */
#define A_TYPE       D32(0x2b98)    /* the extension: 0x1a, 0x20, 0x21 */
#define A_HEADER     0x2ba0         /* 0x200 */
#define A_VERSION    DS32(0x2cf0)
#define A_REVISION   DS32(0x2cf4)
#define A_STRING_N   D32(0x2cfc)
#define A_CODE_SIZE  D32(0x2d00)
#define A_SCAN_SIZE  D32(0x2d04)
#define A_DIR        ((char *)D + 0x2da0)   /* the program's directory */
#define A_CRC_TABLE  0x2ea0

/* A coverage build (make advdrv-cover) counts what runs, for the harness:
 * instructions at 0, operators at 0x40, variables read at 0x100 and
 * written at 0x200, errors at 0x300. */
#ifdef ADVDRV_COVER
uint32_t advdrv_cover[0x400];
#define COVER(i) (advdrv_cover[(i) & 0x3ff]++)
#else
#define COVER(i) ((void)0)
#endif

static uint32_t stop(void);
static void error(uint32_t which);
static double eval(void);
static int32_t eval_int(void);

/* ---- the CRT, and the x87 as retail uses it ----------------------------------------- */

/* .rdata's constant pool, so that every constant is retail's exact value. */
static const uint8_t advdrv_rdata[] __attribute__((aligned(8))) = {
#define ADVDRV_RDATA
#include "advdrv_data.inc"
#undef ADVDRV_RDATA
};
#define K(va)  (*(const adv_f64 *)(advdrv_rdata + ((va) - 0x20619000u)))

/* Retail calls MSVCRT's sprintf, _CIfmod, _CIasin and modf; so does this, so
 * that formatting and the transcendental functions are the same code. */
typedef int (__cdecl *crt_sprintf_fn)(char *, const char *, ...);
static crt_sprintf_fn crt_sprintf;
void *advdrv_crt_cifmod;
void *advdrv_crt_ciasin;
void *advdrv_crt_modf;

static void bind_crt(void)
{
    HMODULE crt;

    if (crt_sprintf != NULL)
        return;
    crt = LoadLibraryA("MSVCRT.dll");
    crt_sprintf = (crt_sprintf_fn)(void *)GetProcAddress(crt, "sprintf");
    advdrv_crt_cifmod = (void *)GetProcAddress(crt, "_CIfmod");
    advdrv_crt_ciasin = (void *)GetProcAddress(crt, "_CIasin");
    advdrv_crt_modf = (void *)GetProcAddress(crt, "modf");
}

/* fld x; fld y; call _CIfmod -- the remainder of two doubles, on st0. */
ld advdrv_fmod(double x, double y);
/* call _CIasin on an extended value. */
ld advdrv_asin(ld x);
/* modf(x, &whole), its fraction left on st0 unrounded. */
ld advdrv_modf(double x, double *whole);
/* fpatan: atan2(y, x) on extended values. */
ld advdrv_atan2(ld y, ld x);
__asm__(
    ".text\n"
    ".globl _advdrv_fmod\n"
    "_advdrv_fmod:\n\t"
    "fldl 4(%esp)\n\t"
    "fldl 12(%esp)\n\t"
    "call *_advdrv_crt_cifmod\n\t"
    "ret\n"
    ".globl _advdrv_asin\n"
    "_advdrv_asin:\n\t"
    "fldt 4(%esp)\n\t"
    "call *_advdrv_crt_ciasin\n\t"
    "ret\n"
    ".globl _advdrv_modf\n"
    "_advdrv_modf:\n\t"
    "pushl 12(%esp)\n\t"
    "pushl 12(%esp)\n\t"
    "pushl 12(%esp)\n\t"
    "call *_advdrv_crt_modf\n\t"
    "addl $12, %esp\n\t"
    "ret\n"
    ".globl _advdrv_atan2\n"
    "_advdrv_atan2:\n\t"
    "fldt 4(%esp)\n\t"
    "fldt 16(%esp)\n\t"
    "fpatan\n\t"
    "ret\n");

/* MSVCRT's _ftol: truncation towards zero, 64 bits, the x87's indefinite
 * integer out of range.  Callers mostly keep the low half. */
static int64_t ftol(ld value)
{
    if (!(value >= -9223372036854775808.0L && value < 9223372036854775808.0L))
        return (int64_t)0x8000000000000000LL;
    return (int64_t)value;
}

/* fcom: C0 (0x01) below, C3 (0x40) equal, both (with C2) unordered -- the
 * bits retail's `test ah, ...` reads. */
#define C0 0x01
#define C3 0x40
static int fcom(ld a, ld b)
{
    if (a < b)
        return C0;
    if (a == b)
        return C3;
    if (a > b)
        return 0;
    return 0x45;
}

/* idiv, so that a zero divisor or INT_MIN / -1 faults as retail's does. */
static int32_t idiv(int32_t a, int32_t b, int32_t *remainder)
{
    int32_t q, r;

    __asm__ volatile ("cltd\n\tidivl %3" : "=a"(q), "=&d"(r) : "a"(a), "r"(b)
                      : "cc");
    *remainder = r;
    return q;
}

/* ---- forward declarations ----------------------------------------------------------- */

static void reset(void);
static void free_code(void);
static uint32_t messages_clear(void);
static void FS98_STDCALL call_handler(uint32_t target);
static void step(void);
static void op_error(void);
static void operators_init(void);
static void keys_clear(void);
static void keys_release(void);
static void voices_idle(void);
static void voices_stop(void);
static void voices_free(void);
static void sound_check(void);
static void sound_dir(void);
static uint32_t events_free(void);
static uint32_t FS98_STDCALL events_next(int32_t *handler);
static void FS98_STDCALL alert_show(uint32_t opening);
static void FS98_STDCALL cache_sounds(uint32_t caching);
static uint32_t FS98_STDCALL on_message(uint32_t code, uint32_t arg);
static uint32_t FS98_STDCALL frame(uint32_t code, uint32_t arg);
static uint32_t FS98_STDCALL track(uint32_t code, uint32_t arg);
static uint32_t FS98_STDCALL on_command(uint32_t command, uint32_t *handled);

/* ---- init, deinit, load: .text+0x20 .. +0x8f0 ---------------------------------------- */

/* .text+0x20, Linkage +0x04 */
static uint32_t adv_init(void)
{
    uint32_t root;

    reset();
    keys_clear();
    /* .text+0x210: the CRC32 table */
    {
        uint32_t n, value, bit;

        for (n = 0; n < 0x100; n++) {
            value = n;
            for (bit = 8; bit != 0; bit--) {
                if (value & 1)
                    value = (value >> 1) ^ 0xedb88320u;
                else
                    value >>= 1;
            }
            D32(A_CRC_TABLE + 4 * n) = value;
        }
    }
    CHAIN_ADD(A_LIST, U32(frame));
    CHAIN_ADD(3, U32(track));
    CHAIN_ADD(0xb, U32(on_command));
    sound_check();
    A_SOUND_DIR2[0] = 0;
    H_CFG_INT(U32(A_APL), U32(A_MAX_EVENTS), U32(&A_EVENTS_MAX));
    H_CFG_STRING(U32(A_APL), U32(A_WAVE), U32(A_SOUND_DIR2), 0x100);
    root = H_ROOT();
    H_JOIN(U32(A_CACHE_DIR), 0, root, U32(A_WAVDIR));
    H_LOADSTRING(0x900d, U32(A_SOUND_DIR1), 0x100);
    return H_LOADSTRING(0x900e, U32(A_SOUND_DIR3), 0x100);
}

/* .text+0x100, Linkage +0x08 */
static uint32_t adv_deinit(void)
{
    A_DONE = 1;
    stop();
    keys_release();
    CHAIN_REMOVE(A_LIST, U32(frame));
    CHAIN_REMOVE(3, U32(track));
    CHAIN_REMOVE(0xb, U32(on_command));
    free_code();
    voices_free();
    return events_free();
}

/* .text+0x160, Linkage +0x18: the type of program running, if one is */
static uint32_t FS98_STDCALL adv_running(uint32_t *type)
{
    if (A_RUNNING == 0 || type == NULL)
        return 0;
    *type = A_TYPE;
    return 1;
}

/* .text+0x190, Linkage +0x24, and the "stop" instruction */
static uint32_t stop(void)
{
    uint32_t result;

    if (A_RUNNING == 1) {
        alert_show(0);
        A_RUNNING = 0;
    }
    messages_clear();
    voices_stop();
    cache_sounds(0);
    CHAIN_REMOVE(0x14, U32(on_message));
    free_code();
    events_free();
    result = A_CONSOLE;
    if (result != 0) {
        result = (uint32_t)FreeConsole();
        A_CONSOLE = 0;
    }
    return result;
}

/* .text+0x1f0, Linkage +0x20, and the "restart" instruction */
static void restart(void)
{
    operators_init();
    keys_release();
    voices_stop();
    reset();
    A_RUNNING = 1;
}

/* .text+0x250 */
static uint32_t FS98_STDCALL crc32(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0xffffffffu;

    while (n != 0) {
        crc = (crc >> 8) ^ D32(A_CRC_TABLE + 4 * ((crc & 0xff) ^ *p++));
        n--;
    }
    return ~crc;
}

/* .text+0x690: 0 for an adventure header this version reads */
static int32_t FS98_STDCALL check_header(const uint8_t *header)
{
    int32_t version;

    if (H_STRCMP(U32(header), U32(A_MAGIC_OLD)) != 0 &&
        H_STRCMP(U32(header), U32(A_MAGIC)) != 0)
        return -1;
    version = PS32(header, 0x150);
    if (!(version == 6 && P32(header, 0x154) == 1) &&
        !(version == 1 && P32(header, 0x154) == 0))
        return -1;
    A_CRC = P32(header, 0x158);
    return 0;
}

/* .text+0x640: read the 0x200-byte header into `buf` and check it */
static int32_t FS98_STDCALL read_header(uint32_t file, uint8_t *buf)
{
    if (H_READ(file, U32(buf), 0x200, U32(&A_GOT)) != 0)
        return -1;
    if (A_GOT != 0x200)
        return -1;
    if (check_header(buf) != 0)
        return -1;
    return 0;
}

#define ALERT(id) UTIL_ALERT(1, 1, (id), 0, 0, 0, 0, 0)

/* .text+0x290, Linkage +0x1c: load and start a program, extension `type`
 * (0x1a an adventure, 0x20 a lesson, 0x21 the other kind of lesson) */
static uint32_t FS98_STDCALL adv_load(const char *path, uint32_t type)
{
    uint32_t file, size, i, n;
    uint8_t *p, *code;

    A_TYPE = type;
    if (type == 0x1a)
        H_LOADSTRING(0x9009, U32(A_DIR), 0x100);
    else if (type == 0x20)
        H_LOADSTRING(0x900b, U32(A_DIR), 0x100);
    else if (type == 0x21)
        H_LOADSTRING(0x900a, U32(A_DIR), 0x100);
    A_RUNNING = 0;
    if (H_STRLEN(U32(path)) > 0x100)
        return ALERT(0xf431);
    if (H_OPEN(U32(&file), U32(path), type, 0) != 0)
        return ALERT(0xf432);
    free_code();
    if (H_SIZE(file, U32(&size)) != 0)
        size = 0x20000;
    if (H_ALLOC(1, size, U32(&A_CODE), 1) != 0) {
        ALERT(0xf430);
        return H_CLOSE(file);
    }
    if (read_header(file, PTR(A_CODE)) != 0) {
        ALERT(0xf434);
        return H_CLOSE(file);
    }
    H_MEMMOVE(U32(D + A_HEADER), A_CODE, 0x200);
    if (H_READ(file, A_CODE, size, U32(&A_GOT)) != 0 || A_GOT == 0) {
        ALERT(0xf433);
        return H_CLOSE(file);
    }
    H_CLOSE(file);
    if (crc32(PTR(A_CODE), A_GOT) != A_CRC)
        return ALERT(0xf435);
    if (A_VERSION > 6 || (A_VERSION == 6 && A_REVISION > 0)) {
        /* 6.1: the strings follow the code and variables, and are moved to
         * a table of their own */
        if (H_ALLOC(1, 4 * A_STRING_N, U32(&A_STRINGS), 1) != 0) {
            ALERT(0xf430);
            return H_FREE(U32(&A_CODE));
        }
        p = PTR(A_CODE + A_CODE_SIZE);
        for (i = 0; i < A_STRING_N; i++) {
            n = H_STRLEN(U32(p));
            H_ALLOC(1, n + 1, A_STRINGS + 4 * i, 0);
            H_STRCPY(P32(PTR(A_STRINGS), 4 * i), U32(p));
            p += n + 1;
        }
        H_REALLOC(1, A_CODE_SIZE, U32(&A_CODE));
    } else {
        /* older: every byte is its offset's low byte less the byte */
        code = PTR(A_CODE);
        if (A_GOT != 0) {
            i = A_GOT - 1;
            for (;;) {
                code[i] = (uint8_t)((uint8_t)i - code[i]);
                if (i-- == 0)
                    break;
            }
        }
    }
    CHAIN_ADD(0x14, U32(on_message));
    alert_show(1);
    G32(0x264) = 0;
    sound_dir();
    cache_sounds(1);
    restart();
    return 0;
}

/* .text+0x700 */
static void reset(void)
{
    uint32_t ticks;

    A_PC = A_CODE;
    A_RUNNING = 0;
    A_ON_832 = -1;
    A_ON_KEY = -1;
    A_HANDLER = -1;
    A_IN_HANDLER = 0;
    A_SOUTH = 0;
    A_EAST = 0;
    A_KEY = 0;
    A_LAST_KEY = 0;
    H_MEMSET(U32(D + A_BINDINGS), 0, 0xa0);
    ticks = H_TICKS();
    A_STEPS = 3;
    A_CONSOLE_ON = 0;
    A_DEPTH = 0;
    A_START = (double)(uint64_t)ticks;
    A_WAIT = 0;
    A_WAIT_MSG = 0;
    A_MSG = 0;
    A_TOGGLE5 = 0;
    A_TOGGLE6 = 0;
    A_1724 = 0;
    A_DIGITS = 1;
    A_PAD = 1;
    A_DECIMALS = 4;
    A_SKIP21 = 0;
    A_SOUNDS_ON = 1;
    A_MESSAGES_ON = 1;
    A_LIST = 5;
    events_free();
}

/* .text+0x7f0 */
static void free_code(void)
{
    uint32_t i, n;

    if (A_CODE != 0)
        H_FREE(U32(&A_CODE));
    if (A_STRINGS != 0) {
        i = A_STRING_N;
        for (;;) {
            n = i;
            i--;
            if (n == 0)
                break;
            if (P32(PTR(A_STRINGS), 4 * i) != 0)
                H_FREE(A_STRINGS + 4 * i);
        }
        H_FREE(U32(&A_STRINGS));
    }
}

/* .text+0x860 */
static uint32_t messages_clear(void)
{
    A_WAIT_MSG = 0;
    A_MSG = 0;
    return WINMSG_CLEAR();
}

/* .text+0x880, Linkage +0x30: the next "stopped" (list 0x14, 0x21) is ours */
static uint32_t FS98_STDCALL adv_set_skip(uint32_t skip)
{
    A_SKIP21 = skip;
    return skip;
}

/* .text+0x890, on CHAIN list 0x14 while a program runs: 0x21 stops it
 * (unless it was ours); 0x50 says WINMSG's message `arg` was acknowledged */
static uint32_t FS98_STDCALL on_message(uint32_t code, uint32_t arg)
{
    if (code == 0x21) {
        if (A_SKIP21 == 0)
            stop();
        else
            A_SKIP21 = 0;
        return 0;
    }
    if (code == 0x50 && A_WAIT_MSG == arg) {
        A_WAIT = 0;
        A_WAIT_MSG = 0;
        A_MSG = 0;
    }
    return 0;
}

/* .text+0x8f0, on CHAIN list A_LIST: one frame of the program */
static uint32_t FS98_STDCALL frame(uint32_t code, uint32_t arg)
{
    int32_t handler;
    uint32_t wait, n;

    (void)code;
    (void)arg;
    if (A_RUNNING == 0 || G32(0x268) == 1 || G32(0x758) == 1)
        return 0;
    A_STEPPED = 0;
    voices_idle();
    if (A_IN_HANDLER == 0) {
        if (A_ON_832 != -1 && G16(0x832) != 0) {
            call_handler((uint32_t)A_ON_832);
            G16(0x832) = 0;
        }
        if (A_ON_KEY != -1 && G8(0x36c) == 1)
            call_handler((uint32_t)A_ON_KEY);
        if (A_HANDLER != -1) {
            call_handler((uint32_t)A_HANDLER);
            A_HANDLER = -1;
        }
    }
    wait = A_WAIT;
    if (wait != 0) {
        if (wait == 4) {
            if (A_KEY != 0) {
                A_LAST_KEY = A_KEY;
                A_KEY = 0;
            }
            return 0;
        }
        if (wait == 1) {
            if (A_KEY != 0) {
                A_LAST_KEY = A_KEY;
                A_KEY = 0;
            }
            if (H_TICKS() < A_UNTIL)
                return 0;
            wait = 0;
            A_WAIT = 0;
        } else if (wait == 2) {
            if (A_KEY == 0)
                return 0;
            A_LAST_KEY = A_KEY;
            A_KEY = 0;
            wait = 0;
            A_WAIT = 0;
        } else if (wait == 3) {
            if (A_KEY != 0) {
                A_LAST_KEY = A_KEY;
                A_KEY = 0;
            } else if (H_TICKS() < A_UNTIL) {
                return 0;
            }
            wait = 0;
            A_WAIT = 0;
        }
    }
    if (A_CODE == 0)
        return 0;
    if (A_IN_HANDLER == 0) {
        while (events_next(&handler)) {
            if (handler != -1)
                call_handler((uint32_t)handler);
        }
        wait = A_WAIT;
    }
    if (A_STEPS == 0)
        return 0;
    n = 0;
    for (;;) {
        if (A_RUNNING == 0 || G32(0x268) == 1 || G32(0x758) == 1 || wait != 0)
            break;
        step();
        if (++n >= A_STEPS)
            break;
        wait = A_WAIT;
    }
    return 0;
}

/* .text+0xae0, on CHAIN list 3: keep the last three positions */
static uint32_t FS98_STDCALL track(uint32_t code, uint32_t arg)
{
    (void)code;
    (void)arg;
    H_MEMMOVE(U32(D + A_POSITIONS + 0x10), U32(D + A_POSITIONS), 0x20);
    D16(A_POSITIONS + 0x2) = G16(0x562);
    D32(A_POSITIONS + 0x4) = G32(0x564);
    D16(A_POSITIONS + 0xa) = G16(0x56a);
    D32(A_POSITIONS + 0xc) = G32(0x56c);
    return U32(G);
}

/* ---- instructions: .text+0xb40 .. +0x1c30 ------------------------------------------- */

#define PC8()   (*PTR(A_PC))
#define PC32()  P32(PTR(A_PC), 0)

static void FS98_STDCALL key_bind(uint32_t key);
static void FS98_STDCALL key_unbind(uint32_t key);
static uint32_t FS98_STDCALL play_sound(const char *name);
static char *text(void);
static void FS98_STDCALL con_write(const char *s);
static void FS98_STDCALL push_call(uint32_t target, uint32_t back, uint32_t from_event);

/* skip the string at the pc */
static void skip_string(void)
{
    uint8_t c = PC8();

    A_PC++;
    if (c != 0) {
        do {
            c = PC8();
            A_PC++;
        } while (c != 0);
    }
}

/* .text+0xb40, 0x25: bind (1) or unbind (2..) a list of keys, or (0) all */
static void op_keys(void)
{
    uint8_t how = PC8(), n;
    void (FS98_STDCALL *fn)(uint32_t);
    uint32_t count;
    int32_t key;

    A_PC++;
    if (how == 0) {
        keys_release();
        return;
    }
    fn = key_bind;
    if (how != 1)
        fn = key_unbind;
    n = PC8();
    A_PC++;
    if (n != 0) {
        count = (uint32_t)(uint8_t)(n - 1) + 1;
        do {
            key = eval_int();
            if (key != 0 && (uint32_t)key <= 0x7ff)
                fn((uint32_t)key);
        } while (--count != 0);
    }
}

/* .text+0xbb0, 0x00 */
static void op_stop(void)
{
    stop();
}

/* .text+0xbc0, 0x0f and every malformed instruction: stop, and say so */
static void op_error(void)
{
    COVER(0x309);
    op_stop();
    UTIL_ALERT(1, 4, 0xf439, 0, 0, 0, 0, 0);
}

/* .text+0xbf0, 0x02: jump if the expression is zero */
static void op_unless(void)
{
    uint32_t target = PC32();

    A_PC += 4;
    if (eval_int() == 0)
        A_PC = A_CODE + target;
}

/* .text+0xc20, 0x0e: play a sound */
static void op_sound(void)
{
    if (A_SOUNDS_ON != 0 && G32(0xb24) != 0)
        play_sound((const char *)PTR(A_PC));
    skip_string();
}

/* .text+0xc70, 0x0d: a message in WINMSG's slot 5 */
static void op_message5(void)
{
    char *t = text();

    if (t != NULL && A_MESSAGES_ON == 1) {
        A_MSG = WINMSG_SHOW(U32(t), 5, A_MSG_TIME, A_TOGGLE5);
        A_TOGGLE5 = A_PLAIN == 0;
    }
}

/* .text+0xcc0, 0x10: the same in slot 6 */
static void op_message6(void)
{
    char *t = text();

    if (t != NULL && A_MESSAGES_ON == 1) {
        A_MSG = WINMSG_SHOW(U32(t), 6, A_MSG_TIME, A_TOGGLE6);
        A_TOGGLE6 = A_PLAIN == 0;
    }
}

/* .text+0xd10, 0x22: text to the console */
static void op_print(void)
{
    char *t = text();

    if (t != NULL) {
        con_write(t);
        if (A_PLAIN != 0)
            con_write(A_NEWLINE);
    }
}

/* .text+0xd40, 0x23: how numbers are written in text */
static void op_format(void)
{
    uint32_t digits = (uint32_t)eval_int();
    uint32_t pad = (uint32_t)eval_int();
    uint32_t decimals = (uint32_t)eval_int();

    if (decimals >= 9 || pad > decimals || digits >= 9) {
        error(2);
        return;
    }
    if (digits == 0 && pad == 0)
        digits = 1;
    A_DIGITS = digits;
    A_PAD = pad;
    A_DECIMALS = decimals;
}

/* .text+0xd90, 0x24: how long messages stay, in seconds */
static void op_message_time(void)
{
    int32_t seconds = eval_int();

    if (seconds < 0) {
        error(3);
        return;
    }
    A_MSG_TIME = (uint32_t)seconds * 18;
}

/* .text+0xdb0, 0x21: open (nonzero) or close (0) the debug console */
static void op_console(void)
{
    char title[0x80];
    uint8_t on = PC8();

    A_PC++;
    A_CONSOLE_ON = on;
    if (A_CONSOLE != 0) {
        if (on == 0) {
            FreeConsole();
            A_CONSOLE = 0;
        }
        return;
    }
    if (on == 0)
        return;
    AllocConsole();
    title[0] = 0;
    H_LOADSTRING(0x2330, U32(title), 0x80);
    SetConsoleTitleA(title[0] != 0 ? title : A_APL);
    A_CONSOLE = (uint32_t)(uintptr_t)GetStdHandle(STD_OUTPUT_HANDLE);
}

/* .text+0xe50, 0x11: wait.  Mode 2 waits for the last message to be
 * acknowledged; otherwise 0 seconds takes the key pressed, -1 (with a
 * nonzero mode) waits for a key, and more waits that long -- or, in mode 1,
 * that long or for a key. */
static void op_wait(void)
{
    double v = eval();
    uint8_t mode = PC8();
    uint32_t key;

    A_PC++;
    if (mode == 2) {
        if (A_MSG != 0 && A_MESSAGES_ON == 1) {
            A_WAIT_MSG = A_MSG;
            A_WAIT = 4;
        }
        return;
    }
    if (fcom(v, K(0x20619000)) & C0) {
        if (mode == 0 || !(fcom(v, K(0x20619008)) & C3)) {
            error(4);
            return;
        }
    }
    if (fcom(v, K(0x20619000)) & C3) {
        key = A_KEY;
        if (key != 0 || mode == 1)
            A_LAST_KEY = key;
        A_KEY = 0;
        return;
    }
    if (fcom(v, K(0x20619008)) & C3) {
        A_WAIT = 2;
        return;
    }
    if (fcom(v, K(0x20619000)) & (C0 | C3))
        return;
#if BREAK==5
    A_UNTIL = (uint32_t)ftol((ld)v);
#else
    A_UNTIL = (uint32_t)ftol((ld)v * K(0x20619010));
#endif
    A_UNTIL = A_UNTIL + H_TICKS();
    A_WAIT = mode == 1 ? 3 : 1;
}

/* .text+0xf60, 0x01: jump */
static void op_goto(void)
{
    A_PC = A_CODE + PC32();
}

/* .text+0xf80, 0x07: jump through a table */
static void op_switch(void)
{
    uint32_t n = PC8();
    int32_t v;

    A_PC++;
    v = eval_int();
    if (v < 0 || v >= (int32_t)n)
        A_PC += 4 * n;
    else
        A_PC = A_CODE + P32(PTR(A_PC), 4 * v);
}

/* .text+0xfd0, 0x05: call */
static void op_call(void)
{
    push_call(PC32(), A_PC + 4, 0);
}

/* .text+0xff0, 0x08: call through a table */
static void op_switch_call(void)
{
    uint32_t n = PC8();
    int32_t v;

    A_PC++;
    v = eval_int();
    if (v >= 0 && v < (int32_t)n) {
        push_call(P32(PTR(A_PC), 4 * v), A_PC + 4 * n, 0);
        return;
    }
    A_PC += 4 * n;
}

/* .text+0x1040: run a handler to its return, up to 10000 instructions a
 * frame */
static void FS98_STDCALL call_handler(uint32_t target)
{
    uint32_t depth;

    if (A_IN_HANDLER == 1)
        return;
    depth = A_DEPTH;
    push_call(target, A_PC, 1);
    A_UNTIL_SAVED = A_UNTIL;
    A_IN_HANDLER = 1;
    A_WAIT_SAVED = A_WAIT;
    A_WAIT = 0;
    if (A_DEPTH != depth) {
        while (A_STEPPED < 10000) {
            if (A_RUNNING == 0 || G32(0x268) == 1 || G32(0x758) == 1 ||
                A_WAIT != 0)
                break;
            step();
            if (A_DEPTH == depth)
                break;
        }
    }
    if (A_STEPPED == 10000) {
        error(7);
        stop();
    }
}

/* .text+0x1100 */
static void FS98_STDCALL push_call(uint32_t target, uint32_t back, uint32_t from_event)
{
    uint32_t depth = A_DEPTH;

    if (depth >= 0x32) {
        error(0);
        return;
    }
    D32(A_STACK + 8 * depth) = back;
    D32(A_STACK + 8 * depth + 4) = from_event;
    A_DEPTH = depth + 1;
    A_PC = A_CODE + target;
}

/* .text+0x1150, 0x0a: the handler for GLOBAL +0x832 */
static void op_on_832(void)
{
    A_ON_832 = (int32_t)PC32();
    A_PC += 4;
}

/* .text+0x1170, 0x0b: the handler for GLOBAL +0x36c */
static void op_on_key(void)
{
    A_ON_KEY = (int32_t)PC32();
    A_PC += 4;
}

/* .text+0x1190, 0x06: return */
static void op_return(void)
{
    uint32_t depth = A_DEPTH;

    if (depth == 0) {
        error(1);
        return;
    }
    A_PC = D32(A_STACK + 8 * depth - 8);
    depth--;
    A_DEPTH = depth;
    if (D32(A_STACK + 8 * depth + 4) != 0) {
        A_IN_HANDLER = 0;
        A_WAIT = A_WAIT_SAVED;
        A_UNTIL = A_UNTIL_SAVED;
    }
}

/* A variable reference: kind 0 is the program's own double at `where`,
 * kind 1 system variable `where`. */
struct adv_ref {
    uint16_t kind;
    uint16_t unused;
    uint32_t where;
    uint32_t read_only;
};

static void FS98_STDCALL parse_ref(struct adv_ref *ref);
static void FS98_STDCALL assign(struct adv_ref *ref, double v);

/* .text+0x11e0, 0x03: start a loop: var = from - step, and the limit and
 * step written into the 0x04 that follows */
static void op_for(void)
{
    struct adv_ref ref;
    double from, limit, by;

    parse_ref(&ref);
    from = eval();
    limit = eval();
    by = eval();
    PF64(PTR(ref.where), 0) = (double)((ld)from - (ld)by);
    PF64(PTR(A_PC), 1) = limit;
    PF64(PTR(A_PC), 9) = by;
}

/* .text+0x1240, 0x04: step the loop; jump out once past the limit */
static void op_next(void)
{
    struct adv_ref ref;
    double limit, by;
    uint32_t target;
    int out;

    limit = PF64(PTR(A_PC), 0);
    A_PC += 8;
    by = PF64(PTR(A_PC), 0);
    A_PC += 8;
    target = PC32();
    A_PC += 4;
    parse_ref(&ref);
    PF64(PTR(ref.where), 0) = (double)((ld)PF64(PTR(ref.where), 0) + (ld)by);
    if (!(fcom(by, K(0x20619000)) & (C0 | C3)))
        out = !(fcom(limit, PF64(PTR(ref.where), 0)) & C0) ? 0 : 1;
    else
        out = (fcom(limit, PF64(PTR(ref.where), 0)) & (C0 | C3)) ? 0 : 1;
    if (out)
        A_PC = A_CODE + target;
}

/* .text+0x12f0, 0x09: assign */
static void op_assign(void)
{
    struct adv_ref ref;
    double v;

    parse_ref(&ref);
    v = eval();
    assign(&ref, v);
}

/* .text+0x1320 .. +0x1350, 0x12 .. 0x15: which way latitude and longitude
 * count */
static void op_north(void) { A_SOUTH = 0; }
static void op_south(void) { A_SOUTH = 1; }
static void op_east(void)  { A_EAST = 1; }
static void op_west(void)  { A_EAST = 0; }

static void op_weather(void);

/* .text+0x1360, 0x1a .. 0x20 */
static void op_weather_thunk(void)
{
    op_weather();
}

/* .text+0x1370, 0x17: end the flight; the "stopped" that follows is ours */
static void op_end_flight(void)
{
    A_SKIP21 = 1;
    H_COMMAND(0x10037, 0);
}

static void FS98_STDCALL to_lat(double v, uint8_t *out);
static void FS98_STDCALL to_lon(double v, uint8_t *out);
static void FS98_STDCALL to_48(double v, uint8_t *out);

/* .text+0x1390, 0x16: put the aircraft at (latitude, longitude, feet) */
static void op_position(void)
{
    double v;

    v = eval();
    to_lat(v, G + 0x560);
    v = eval();
    to_lon(v, G + 0x568);
    v = (double)((ld)eval() * K(0x20619018));
    to_48(v, G + 0x570);
#if BREAK==6
    G8(0x365) = 0xe;
#else
    G8(0x365) = 0xf;
#endif
}

/* .text+0x1410, 0x26: play a video, from the program's VIDEOS, the sim's,
 * or as named */
static void op_video(void)
{
    char path[0x104], dir[0x104];
    char *name = path;

    H_LOADSTRING(0x900f, U32(path), 0x104);
    H_STRCPY(U32(dir), U32(A_DIR));
    H_STRCAT(U32(dir), U32(A_BACKSLASH));
    H_STRCAT(U32(dir), U32(path));
    H_JOIN(U32(path), 0, U32(dir), A_PC);
    if (H_EXISTS(U32(path), 0x1d) == (uint32_t)-1) {
        H_LOADSTRING(0x9007, U32(dir), 0x104);
        H_JOIN(U32(path), 0, U32(dir), A_PC);
        if (H_EXISTS(U32(path), 0x1d) == (uint32_t)-1) {
            name = (char *)PTR(A_PC);
            if (H_EXISTS(A_PC, 0x1d) == (uint32_t)-1) {
                skip_string();
                return;
            }
        }
    }
    DEMO_PLAY(U32(name), 0);
    skip_string();
}

/* .text+0x1590, 0x18: load a situation, from the program's directory,
 * string 0x9004's or 0x9002's */
static void op_situation(void)
{
    uint8_t ext[4];
    struct {
        int32_t ext;
        char path[0x104];
    } situation;
    char dir[0x100], name[0x100], drive[0x100];
    uint32_t a, b, c, d;

    H_SPLIT(A_PC, U32(drive), 0, U32(name), U32(ext));
    situation.ext = 2;
    H_JOIN(U32(situation.path), 0, U32(A_DIR), U32(name));
    if (H_EXISTS(U32(situation.path), situation.ext) == (uint32_t)-1) {
        H_LOADSTRING(0x9004, U32(dir), 0x100);
        H_JOIN(U32(situation.path), 0, U32(dir), U32(name));
        if (H_EXISTS(U32(situation.path), situation.ext) == (uint32_t)-1) {
            H_LOADSTRING(0x9002, U32(dir), 0x100);
            H_JOIN(U32(situation.path), 0, U32(dir), U32(name));
            if (H_EXISTS(U32(situation.path), situation.ext) == (uint32_t)-1) {
                UTIL_ALERT(1, 1, 0xf438, A_PC, 0, 0, 0, 1);
                stop();
                return;
            }
        }
    }
    a = G32(0x1244);
    b = G32(0x1248);
    c = G32(0x124c);
    d = G32(0x1250);
    G32(0x1244) = 1;
    G32(0x1248) = 1;
    G32(0x124c) = 1;
    G32(0x1250) = 1;
    A_SKIP21 = 1;
    MAIN_SITUATE(U32(&situation));
    G32(0x1244) = a;
    G32(0x1248) = b;
    G32(0x124c) = c;
    G32(0x1250) = d;
    skip_string();
}

/* .text+0x17b0, 0x19 */
static void op_restart(void)
{
    restart();
}

/* .text+0x17c0, 0x0c: bind a key code to a handler (-1 unbinds it) */
static void op_bind(void)
{
    uint32_t key = (uint32_t)eval_int() & 0xffff;
    uint32_t target = PC32();
    uint32_t i;

    A_PC += 4;
    if (target == 0xffffffffu) {
        for (i = 0; i < 20; i++) {
            if (D32(A_BINDINGS + 8 * i) == key) {
                D32(A_BINDINGS + 8 * i) = 0;
                D32(A_BINDINGS + 8 * i + 4) = 0;
                return;
            }
        }
        return;
    }
    for (i = 0; i < 20; i++) {
        if (D32(A_BINDINGS + 8 * i) == key)
            goto found;
    }
    for (i = 0; i < 20; i++) {
        if (D32(A_BINDINGS + 8 * i) == 0)
            goto found;
    }
    error(5);
    return;
found:
    D32(A_BINDINGS + 8 * i) = key;
    D32(A_BINDINGS + 8 * i + 4) = target;
}

/* .text+0x1870, 0x27: which CHAIN list the frame runs on, and how many
 * instructions a frame (100: three on list 5) */
static void op_rate(void)
{
    uint32_t which, list;

    CHAIN_REMOVE(A_LIST, U32(frame));
    which = PC8();
    A_PC++;
    switch (which) {
    case 1: list = 3; break;
    case 3: list = 2; break;
    case 4: list = 1; break;
    default: list = 5; break;
    }
    A_LIST = list;
    CHAIN_ADD(list, U32(frame));
    if (which == 100)
        A_STEPS = 3;
    else
        A_STEPS = (uint32_t)eval_int();
}

static uint8_t *FS98_STDCALL skip_expr(uint8_t *p);
static void FS98_STDCALL event_true(uint32_t at);
static void FS98_STDCALL event_none(uint32_t at);
static void FS98_STDCALL event_edge(uint32_t at);
static void FS98_STDCALL event_none1(uint32_t at);
static void FS98_STDCALL event_change(uint32_t at);
static void FS98_STDCALL event_none2(uint32_t at);
static uint32_t events_free(void);
static void FS98_STDCALL events_drop(uint32_t target);

/* .text+0x1980, 0x28: an event that calls a handler while true */
static void op_when(void)
{
    uint32_t at = A_PC, target;

    A_PC = U32(skip_expr(PTR(A_PC)));
    target = PC32();
    A_PC += 8;
    if (target == 0xffffffffu)
        event_none(at);
    else
        event_true(at);
}

/* .text+0x19c0, 0x29: an event on the expression's edges */
static void op_edge(void)
{
    uint32_t at = A_PC, target;
    double v = eval();

    target = PC32();
    A_PC += 0x10;
    if (target != 0xffffffffu) {
        PF64(PTR(A_PC), -8) = v;
        event_edge(at);
    } else {
        event_none1(at);
    }
}

/* .text+0x1a00, 0x2a: an event on the expression's changes */
static void op_change(void)
{
    uint32_t at = A_PC, target;
    double v = eval();

    target = PC32();
    A_PC += 0xc;
    if (target != 0xffffffffu) {
        PF64(PTR(A_PC), -8) = v;
        event_change(at);
    } else {
        event_none2(at);
    }
}

/* .text+0x1a40, 0x2b: drop the events calling a handler (-1: all) */
static void op_forget(void)
{
    uint32_t target = PC32();

    A_PC += 4;
    if (target == 0xffffffffu)
        events_free();
    else
        events_drop(target);
}

/* .text+0x1a70: build text.  Pieces: 0 ends, 1 marks it plain, 2 a literal,
 * 3 a number, 4 a string from the table.  Null for a malformed one. */
static char *text(void)
{
    char *out = A_TEXT, *s;
    uint32_t op, width, index;
    uint8_t c;
    double v;

    A_TEXT[0] = 0;
    op = PC8();
    A_PC++;
    if (op > 4)
        return NULL;
    for (;;) {
        switch (op) {
        case 0:
            return A_TEXT;
        case 1:
            A_PLAIN = 1;
            break;
        case 2:
            A_PLAIN = 0;
            do {
                c = PC8();
                *out++ = (char)c;
                A_PC++;
            } while (c != 0);
            out--;
            break;
        case 3:
            A_PLAIN = 0;
            if (A_DECIMALS != 0)
                width = A_PAD + A_DIGITS + 1;
            else
                width = A_PAD + A_DIGITS;
            v = eval();
            out += crt_sprintf(out, A_NUMBER, width, A_DECIMALS, v);
            break;
        case 4:
            A_PLAIN = 0;
            c = PC8();
            A_PC++;
            if (c != 0x32)
                op_error();
            index = PC32();
            A_PC += 4;
            s = (char *)PTR(P32(PTR(A_STRINGS), 4 * index));
            do {
                c = (uint8_t)*s++;
                *out++ = (char)c;
            } while (c != 0);
            out--;
            break;
        }
        op = PC8();
        A_PC++;
        if (op > 4)
            return NULL;
    }
}

/* .text+0x1bd0: one instruction */
static void step(void)
{
    uint32_t op = PC8();

    A_PC++;
    if (op > 0x2b) {
        op_error();
        return;
    }
    COVER(op);
    ((void (*)(void))(uintptr_t)D32(A_OPCODES + 4 * op))();
    A_STEPPED++;
}

static void op_bad_operator(void);

/* .text+0x1c00: the operator table, from the (id, handler) pairs */
static void operators_init(void)
{
    uint32_t i;

    for (i = 0; i < 0x33; i++)
        D32(A_OPERATE + 4 * i) = U32(op_bad_operator);
    for (i = A_OPERATORS; i < A_VARS; i += 8)
        D32(A_OPERATE + 4 * D32(i)) = D32(i + 4);
}

/* ---- expressions: .text+0x1c30 .. +0x2a80 ------------------------------------------- */

static uint32_t FS98_STDCALL var_read_only(uint32_t index);
static uint32_t FS98_STDCALL var_set(uint32_t index, double v);
static ld FS98_STDCALL var_get(uint32_t index);

/* .text+0x1c30 */
static void FS98_STDCALL parse_ref(struct adv_ref *ref)
{
    uint32_t tag = PC8();

    A_PC++;
    if (tag == 7) {
        ref->kind = 0;
        ref->where = A_CODE + PC32();
        A_PC += 4;
    } else if (tag == 8) {
        ref->kind = 1;
        ref->where = P16(PTR(A_PC), 0);
        A_PC += 2;
        ref->read_only = var_read_only(ref->where);
    } else {
        op_error();
    }
}

/* .text+0x1cc0 */
static void FS98_STDCALL assign(struct adv_ref *ref, double v)
{
    if (ref->kind == 0) {
        PF64(PTR(ref->where), 0) = v;
        return;
    }
    var_set(ref->where, v);
}

static double pop(void);
static int stack_empty(void);

/* .text+0x1d00: an expression -- one operator, or a 1 and a run of them to
 * a 0 -- and the one value it leaves */
static double eval(void)
{
    uint32_t op = PC8();
    double v;

    A_PC++;
    if (op == 1) {
        op = PC8();
        A_PC++;
        while (op != 0) {
            COVER(0x40 + (op < 0x3f ? op : 0x3f));
            ((void (*)(void))(uintptr_t)D32(A_OPERATE + 4 * op))();
            op = PC8();
            A_PC++;
        }
    } else {
        COVER(0x40 + (op < 0x3f ? op : 0x3f));
        ((void (*)(void))(uintptr_t)D32(A_OPERATE + 4 * op))();
    }
    v = pop();
    if (!stack_empty())
        op_error();
    return v;
}

/* .text+0x1d70 */
static int32_t eval_int(void)
{
    return (int32_t)ftol(eval());
}

#define OPERAND(n) PF64(PTR(U32(D) + A_OPERANDS + 8 * (n)), 0)

/* .text+0x1d80 */
static void FS98_STDCALL push(double v)
{
    uint32_t sp;

    if (A_SP + 1 == 100)
        op_error();
    sp = A_SP;
    OPERAND(sp) = v;
    A_SP = sp + 1;
}

/* .text+0x1db0 */
static double pop(void)
{
    if (A_SP == 0)
        op_error();
    A_SP = A_SP - 1;
    return OPERAND(A_SP);
}

/* .text+0x1de0 */
static int stack_empty(void)
{
    return A_SP == 0;
}

/* .text+0x1df0: an operator with no handler */
static void op_bad_operator(void)
{
    op_error();
}

/* .text+0x1e00 .. +0x1f20: constants and variables */
static void x_int8(void)
{
    int32_t v = (int8_t)PC8();

    A_PC++;
    push((double)v);
}

static void x_int16(void)
{
    int32_t v = PS16(PTR(A_PC), 0);

    A_PC += 2;
    push((double)v);
}

static void x_int32(void)
{
    int32_t v = PS32(PTR(A_PC), 0);

    A_PC += 4;
    push((double)v);
}

/* .text+0x1ea0: operator 6 stops the program, and evaluation goes on */
static void x_stop(void)
{
    stop();
}

static void x_double(void)
{
    double v = PF64(PTR(A_PC), 0);

    A_PC += 8;
    push(v);
}

static void x_variable(void)
{
    double v = PF64(PTR(A_CODE + PC32()), 0);

    A_PC += 4;
    push(v);
}

static void x_system(void)
{
    uint32_t index = P16(PTR(A_PC), 0);
    double v;

    A_PC += 2;
    v = (double)var_get(index);
    push(v);
}

/* .text+0x1f50 .. +0x2000: arithmetic.  b is the top of the stack. */
static void x_add(void) { double b = pop(); double a = pop(); push((double)((ld)a + (ld)b)); }
static void x_sub(void) { double b = pop(); double a = pop(); push((double)((ld)a - (ld)b)); }
static void x_mul(void) { double b = pop(); double a = pop(); push((double)((ld)a * (ld)b)); }
static void x_div(void) { double b = pop(); double a = pop(); push((double)((ld)a / (ld)b)); }

/* .text+0x2010 .. +0x2120: integers, through _ftol's low half */
static void x_idiv(void)
{
    int32_t b = (int32_t)ftol(pop());
    int32_t a = (int32_t)ftol(pop());
    int32_t r;

    push((double)idiv(a, b, &r));
}

static void x_mod(void)
{
    int32_t b = (int32_t)ftol(pop());
    int32_t a = (int32_t)ftol(pop());
    int32_t r;

    idiv(a, b, &r);
    push((double)r);
}

static void x_and(void) { int32_t b = (int32_t)ftol(pop()); int32_t a = (int32_t)ftol(pop()); push((double)(a & b)); }
static void x_or(void)  { int32_t b = (int32_t)ftol(pop()); int32_t a = (int32_t)ftol(pop()); push((double)(a | b)); }
static void x_xor(void) { int32_t b = (int32_t)ftol(pop()); int32_t a = (int32_t)ftol(pop()); push((double)(a ^ b)); }

/* .text+0x2120 .. +0x22a0: comparisons, by the x87's flags -- so NaN is
 * equal to everything and below everything */
static void x_eq(void) { double b = pop(); double a = pop(); push((fcom(a, b) & C3) ? 1.0 : 0.0); }
static void x_ne(void) { double b = pop(); double a = pop(); push((fcom(a, b) & C3) ? 0.0 : 1.0); }
static void x_lt(void) { double b = pop(); double a = pop(); push((fcom(a, b) & C0) ? 1.0 : 0.0); }
static void x_le(void) { double b = pop(); double a = pop(); push((fcom(a, b) & (C0 | C3)) ? 1.0 : 0.0); }
static void x_gt(void) { double b = pop(); double a = pop(); push((fcom(a, b) & (C0 | C3)) ? 0.0 : 1.0); }
static void x_ge(void) { double b = pop(); double a = pop(); push((fcom(a, b) & C0) ? 0.0 : 1.0); }

static void x_land(void)
{
    int32_t b = (int32_t)ftol(pop());
    int32_t a = (int32_t)ftol(pop());

    push(a != 0 && b != 0 ? 1.0 : 0.0);
}

static void x_lor(void)
{
    int32_t b = (int32_t)ftol(pop());
    int32_t a = (int32_t)ftol(pop());

    push(a != 0 || b != 0 ? 1.0 : 0.0);
}

static void x_not(void) { int32_t a = (int32_t)ftol(pop()); push(a == 0 ? 1.0 : 0.0); }
static void x_neg(void) { double a = pop(); push(-a); }
static void x_trunc(void) { int32_t a = (int32_t)ftol(pop()); push((double)a); }

static void x_frac(void)
{
    double a = pop();

    push((double)((ld)a - (ld)(int32_t)ftol(a)));
}

/* .text+0x23d0: 1 if lo <= x <= hi */
static void x_between(void)
{
    double hi = pop();
    double lo = pop();
    double x = pop();

    if (fcom(x, lo) & C0) {
        push(0.0);
        return;
    }
    push((fcom(x, hi) & (C0 | C3)) ? 1.0 : 0.0);
}

/* .text+0x2440 .. +0x2560: unit conversions */
static void x_ft_m(void)   { push((double)((ld)pop() * K(0x20619020))); }
static void x_m_ft(void)   { push((double)((ld)pop() * K(0x20619028))); }
static void x_f_c(void)    { push((double)(((ld)pop() - K(0x20619030)) * K(0x20619038))); }
static void x_c_f(void)    { push((double)((ld)pop() * K(0x20619040) - K(0x20619048))); }
static void x_mb_in(void)  { push((double)((ld)pop() * K(0x20619050))); }
static void x_in_mb(void)  { push((double)((ld)pop() * K(0x20619058))); }
static void x_km_nm(void)  { push((double)((ld)pop() * K(0x20619060))); }
static void x_nm_km(void)  { push((double)((ld)pop() * K(0x20619068))); }
static void x_nm_mi(void)  { push((double)((ld)pop() * K(0x20619070))); }
static void x_mi_nm(void)  { push((double)((ld)pop() * K(0x20619078))); }

static ld FS98_STDCALL metres(uint32_t low, int32_t high);

/* .text+0x2580, 0x29: nautical miles from the aircraft to (lat, lon), at
 * the aircraft's altitude */
static void x_range(void)
{
    uint8_t to[0x18];
    uint16_t bearing;
    int32_t range;
    double lon = pop(), lat = pop();

    to_lat(lat, to);
    to_lon(lon, to + 8);
    P32(to, 0x10) = G32(0x570);
    P32(to, 0x14) = G32(0x574);
    MAIN_RANGE(U32(to), U32(G + 0x560), U32(&bearing), U32(&range));
    push((double)((ld)range * K(0x20619080)));
}

/* .text+0x2610, 0x2a: the same to (lat, lon) on the ground */
static void x_range_ground(void)
{
    uint8_t to[0x18];
    uint16_t bearing;
    int32_t range;
    double lon = pop(), lat = pop();

    to_lat(lat, to);
    to_lon(lon, to + 8);
    P16(to, 0x12) = 0;
    PS32(to, 0x14) = GS32(0x20) >> 24;
    MAIN_RANGE(U32(to), U32(G + 0x560), U32(&bearing), U32(&range));
    push((double)((ld)range * K(0x20619080)));
}

/* .text+0x26a0, 0x30: nautical miles between two points */
static void x_range_between(void)
{
    uint8_t from[0x18], to[0x18];
    uint16_t bearing;
    int32_t range;
    double lon1 = pop(), lat1 = pop(), lon2 = pop(), lat2 = pop();

    to_lat(lat1, from);
    to_lon(lon1, from + 8);
    P32(from, 0x10) = G32(0x570);
    P32(from, 0x14) = G32(0x574);
    to_lat(lat2, to);
    to_lon(lon2, to + 8);
    P32(to, 0x10) = G32(0x570);
    P32(to, 0x14) = G32(0x574);
    MAIN_RANGE(U32(from), U32(to), U32(&bearing), U32(&range));
    push((double)((ld)range * K(0x20619080)));
}

/* .text+0x2780, 0x2b: the bearing to (lat, lon), relative to the heading */
static void x_bearing(void)
{
    uint8_t to[0x18];
    uint16_t bearing;
    int32_t range;
    double lon = pop(), lat = pop();

    to_lat(lat, to);
    to_lon(lon, to + 8);
    P16(to, 0x12) = 0;
    P32(to, 0x14) = 0;
    MAIN_RANGE(U32(to), U32(G + 0x560), U32(&bearing), U32(&range));
    bearing = (uint16_t)(bearing - G16(0x2a0));
    push((double)((ld)(int32_t)bearing * K(0x20619088)));
}

/* .text+0x2830, 0x2c: the angle from (lat, lon) up to the aircraft */
static void x_elevation(void)
{
    uint8_t to[0x18];
    uint16_t bearing;
    int32_t range;
    double lon = pop(), lat = pop();
    double r, dz, angle;

    to_lat(lat, to);
    to_lon(lon, to + 8);
    P32(to, 0x10) = G32(0x570);
    P32(to, 0x14) = G32(0x574);
    MAIN_RANGE(U32(to), U32(G + 0x560), U32(&bearing), U32(&range));
    r = (double)range;
    dz = (double)(metres(G32(0x570), GS32(0x574)) -
                  (ld)GS32(0x20) * K(0x20619090));
    angle = (double)advdrv_atan2(dz, r);
    push((double)((ld)angle * K(0x20619098)));
}

/* .text+0x28f0, 0x2e: the minutes of a value in degrees */
static void x_minutes(void)
{
    double a = pop();
    int32_t whole = (int32_t)ftol(a);

    push((double)(int32_t)ftol(((ld)a - (ld)whole) * K(0x206190a0)));
}

/* .text+0x2930, 0x2f: the seconds of a value in minutes */
static void x_seconds(void)
{
    ld m = (ld)pop() * K(0x206190a0);
    int32_t whole = (int32_t)ftol(m);

    push((double)((m - (ld)whole) * K(0x206190a0)));
}

/* .text+0x2970, 0x31: 1 if heading x is in from..to, which may wrap */
static void x_heading_in(void)
{
    double to = pop();
    double from = pop();
    double x = pop();
    ld b = from, c = to;
    double cd = to;
    int f1, f2, in;

    if (!(fcom(b, K(0x206190a8)) & C0))
        b = b - K(0x206190a8);
    else if (fcom(b, K(0x206190b0)) & C0)
        b = b - K(0x206190b8);
    if (!(fcom(c, K(0x206190a8)) & C0)) {
        c = c - K(0x206190a8);
        cd = (double)c;
    } else if (fcom(c, K(0x206190b0)) & C0) {
        c = c - K(0x206190b8);
        cd = (double)c;
    }
    f1 = fcom(c, b);
    f2 = fcom(x, b);
    if (f1 & (C0 | C3)) {
        if (!(f2 & C0) && (fcom(x, K(0x206190a8)) & (C0 | C3)))
            in = 1;
        else
            in = (fcom(x, cd) & (C0 | C3)) ? 1 : 0;
    } else if (f2 & C0) {
        in = 0;
    } else {
        in = (fcom(x, cd) & (C0 | C3)) ? 1 : 0;
    }
    push((double)in);
}

/* ---- system variables: .text+0x2a80 .. +0x2b40 ------------------------------------- */

typedef ld (FS98_STDCALL *var_getter)(uint8_t *descriptor);
typedef uint32_t (FS98_STDCALL *var_setter)(uint8_t *descriptor, double v);

#define VAR(i)  (D + A_VARS + 0x14 * (i))

/* .text+0x2a80 */
static ld FS98_STDCALL var_get(uint32_t index)
{
    var_getter fn;

    if ((int32_t)index >= 0 && (int32_t)index <= 0xaf) {
        fn = (var_getter)(uintptr_t)P32(VAR(index), 0);
        if (fn != NULL) {
            COVER(0x100 + index);
            return fn(VAR(index));
        }
    }
    op_error();
    return K(0x206190c0);
}

/* .text+0x2ac0 */
static uint32_t FS98_STDCALL var_set(uint32_t index, double v)
{
    var_setter fn;

    if ((int32_t)index >= 0 && (int32_t)index <= 0xaf) {
        fn = (var_setter)(uintptr_t)P32(VAR(index), 4);
        if (fn != NULL) {
            COVER(0x200 + index);
            return fn(VAR(index), v);
        }
    }
    op_error();
    return 0;
}

/* .text+0x2b00 */
static uint32_t FS98_STDCALL var_read_only(uint32_t index)
{
    if ((int32_t)index >= 0 && (int32_t)index <= 0xaf)
        return P32(VAR(index), 4) == 0;
    op_error();
    return 1;
}

/* ---- the getters: .text+0x2b40 .. +0x3600 ------------------------------------------ *
 *
 * A descriptor is { getter, setter, where, a, b }: `where` is a GLOBAL
 * offset (or a pointer into this image), a and b are per-getter -- a
 * divisor, a mask, a limit, a GLOBAL offset of a flag, a command. */

#define V_WHERE(e) P32(e, 8)
#define V_A(e)     P32(e, 0xc)
#define V_B(e)     P32(e, 0x10)
#define GV(e, t)   (*(t __attribute__((may_alias)) *)(G + V_WHERE(e)))

static ld FS98_STDCALL lat_degrees(uint32_t low, int32_t high);
static ld FS98_STDCALL lon_degrees(uint32_t low, uint32_t high);

static ld FS98_STDCALL g_u8(uint8_t *e)     { return (ld)(int32_t)GV(e, uint8_t); }
static ld FS98_STDCALL g_u16(uint8_t *e)    { return (ld)(int32_t)GV(e, uint16_t); }
static ld FS98_STDCALL g_u32(uint8_t *e)    { return (ld)(uint64_t)GV(e, uint32_t); }
static ld FS98_STDCALL g_bool8(uint8_t *e)  { return (ld)(int32_t)(GV(e, uint8_t) != 0); }
static ld FS98_STDCALL g_bool16(uint8_t *e) { return (ld)(int32_t)(GV(e, uint16_t) != 0); }
static ld FS98_STDCALL g_not16(uint8_t *e)  { return (ld)(int32_t)(GV(e, uint16_t) == 0); }
static ld FS98_STDCALL g_bool32(uint8_t *e) { return (ld)(int32_t)(GV(e, uint32_t) != 0); }

static ld FS98_STDCALL g_frac16(uint8_t *e)
{
    return (ld)(int32_t)GV(e, uint16_t) * K(0x206190c8);
}

static ld FS98_STDCALL g_s8_over(uint8_t *e)
{
    ld divisor = (ld)(int32_t)V_A(e);

    return (ld)(int32_t)GV(e, int8_t) / divisor;
}

static ld FS98_STDCALL g_s16_over(uint8_t *e)
{
    ld divisor = (ld)(int32_t)V_A(e);

    return (ld)(int32_t)GV(e, int16_t) / divisor;
}

static ld FS98_STDCALL g_u32_over(uint8_t *e)
{
    ld divisor = (ld)(int32_t)V_A(e);

    return (ld)(uint64_t)GV(e, uint32_t) / divisor;
}

static ld FS98_STDCALL g_mask8(uint8_t *e)
{
    return (ld)(int32_t)((GV(e, uint8_t) & (uint8_t)V_A(e)) != 0);
}

static ld FS98_STDCALL g_mask16(uint8_t *e)
{
    return (ld)(int32_t)(((uint32_t)GV(e, uint16_t) & V_A(e)) != 0);
}

static ld FS98_STDCALL g_lat(uint8_t *e)
{
    return lat_degrees(P32(G + V_WHERE(e), 0), PS32(G + V_WHERE(e), 4));
}

static ld FS98_STDCALL g_lon(uint8_t *e)
{
    return lon_degrees(P32(G + V_WHERE(e), 0), P32(G + V_WHERE(e), 4));
}

/* metres as feet; -9999, "none", is 0 */
static ld FS98_STDCALL g_feet16(uint8_t *e)
{
    uint32_t v = GV(e, uint16_t);

#if BREAK==1
    return (ld)(int32_t)v * K(0x206190d8);
#else
    if (v == 0xffffd8f1u)
        return K(0x206190d0);
    return (ld)(int32_t)v * K(0x206190d8);
#endif
}

static ld FS98_STDCALL g_u32_scaled(uint8_t *e)
{
    return (ld)(uint64_t)GV(e, uint32_t) * K(0x206190e0);
}

static ld FS98_STDCALL g_feet48(uint8_t *e)
{
    return metres(P32(G + V_WHERE(e), 0), PS32(G + V_WHERE(e), 4)) *
           K(0x206190d8);
}

static ld FS98_STDCALL g_u32_scaled2(uint8_t *e)
{
    return (ld)(uint64_t)GV(e, uint32_t) * K(0x206190e8);
}

static ld FS98_STDCALL g_angle32(uint8_t *e)
{
    return (ld)GV(e, int32_t) * K(0x206190f0);
}

static ld FS98_STDCALL g_angle32_neg(uint8_t *e)
{
    return (ld)GV(e, int32_t) * K(0x206190f8);
}

/* a 32-bit angle less the heading's */
static ld FS98_STDCALL g_angle32_rel(uint8_t *e)
{
    uint32_t v = GV(e, uint32_t) - ((uint32_t)G16(0x2a0) << 16);

    return (ld)(uint64_t)v * K(0x206190f0);
}

static ld FS98_STDCALL g_angle16_rel(uint8_t *e)
{
    uint16_t v = (uint16_t)(GV(e, uint16_t) - G16(0x2a0));

    return (ld)(int32_t)v * K(0x20619100);
}

static ld FS98_STDCALL g_angle16s(uint8_t *e)
{
    return (ld)(int32_t)GV(e, int16_t) * K(0x20619100);
}

static ld FS98_STDCALL g_angle16(uint8_t *e)
{
    return (ld)(int32_t)GV(e, uint16_t) * K(0x20619100);
}

/* through the module at GLOBAL +0x11d4: its +0x28 writes an angle.  The
 * word comes back in the low half of ecx, which retail used as the slot. */
static ld FS98_STDCALL g_through_11d4(uint8_t *e)
{
    uint32_t slot = U32(e);

    CALL(G32(0x11d4), 0x28, f1)(U32(&slot));
    return (ld)(int32_t)(int16_t)slot * K(0x20619100);
}

/* a BCD radio frequency: 1xx.xx, with the 25 kHz step */
static ld FS98_STDCALL g_frequency(uint8_t *e)
{
    uint32_t v = GV(e, uint16_t);
    uint32_t low = v & 0xf;
    ld step, sum;

    if (low == 2 || low == 7)
        step = K(0x20619108);
    else
        step = K(0x206190d0);
    sum = (ld)(uint64_t)low * K(0x20619110) +
          (ld)(uint64_t)((v >> 12) & 0xf) * K(0x20619118);
    sum = sum + (ld)(uint64_t)((v >> 4) & 0xf) * K(0x20619120);
    return step + ((ld)(uint64_t)((v >> 8) & 0xf) - sum) - K(0x20619128);
}

/* the ADF: BCD at GLOBAL +0x34c, and +0x357/+0x356 when GLOBAL +0x35c
 * says the extended range is on */
static ld FS98_STDCALL g_adf(uint8_t *e)
{
    uint32_t extended = G32(0x35c), v;
    ld thousands, sum, result;

    (void)e;
    if (extended != 0)
        thousands = (ld)(int32_t)(G8(0x357) & 0xf) * K(0x20619130);
    else
        thousands = K(0x206190d0);
    v = G16(0x34c);
    sum = (ld)(uint64_t)((v >> 4) & 0xf) * K(0x20619118) +
          (ld)(uint64_t)((v >> 8) & 0xf) * K(0x20619128);
    result = thousands + ((ld)(uint64_t)(v & 0xf) - sum);
    if (extended != 0)
        result = result - (ld)(int32_t)(G8(0x356) & 0xf) * K(0x20619120);
    return result;
}

static ld FS98_STDCALL g_u16_scaled(uint8_t *e)
{
    return (ld)(int32_t)GV(e, uint16_t) * K(0x20619138);
}

/* 1/256ths; -9999 is 0 */
static ld FS98_STDCALL g_256ths(uint8_t *e)
{
    uint16_t v = GV(e, uint16_t);

#if BREAK==2
    return (ld)(int32_t)(int16_t)v * K(0x20619140);
#else
    if (v == 0xd8f1)
        v = 0;
    return (ld)(int32_t)(int16_t)v * K(0x20619140);
#endif
}

/* the two halves of a 64-bit value, multiplied */
static ld FS98_STDCALL g_product(uint8_t *e)
{
    uint8_t *p = G + V_WHERE(e);
    ld low = (ld)(uint64_t)P32(p, 0) * K(0x20619148);

    return low * (ld)(uint64_t)P32(p, 4);
}

/* A navaid record, through GLOBAL +0x384's table; zero unless the flag
 * byte at GLOBAL + a has its top bit. */
#define NAV(e)  PTR(P32(PTR(G32(0x384)), V_WHERE(e)))

static ld FS98_STDCALL g_nav_lat(uint8_t *e)
{
    uint8_t *r;

    if (!(G8(V_A(e)) & 0x80))
        return K(0x206190d0);
    r = NAV(e);
    return lat_degrees(P32(r, 0), PS32(r, 4));
}

static ld FS98_STDCALL g_nav_lon(uint8_t *e)
{
    uint8_t *r;

    if (!(G8(V_A(e)) & 0x80))
        return K(0x206190d0);
    r = NAV(e);
    return lon_degrees(P32(r, 8), P32(r, 0xc));
}

static ld FS98_STDCALL g_nav_feet(uint8_t *e)
{
    uint8_t *r;

    if (!(G8(V_A(e)) & 0x80))
        return K(0x206190d0);
    r = NAV(e);
    return metres(P32(r, 0x10), PS32(r, 0x14)) * K(0x206190d8);
}

/* the radial from the navaid: MAIN's bearing plus the record's variation */
static ld FS98_STDCALL g_nav_radial(uint8_t *e)
{
    uint8_t *r;
    uint32_t bearing = U32(e), range = 0;

    if (!(G8(V_A(e)) & 0x80))
        return K(0x206190d0);
    r = NAV(e);
    MAIN_RANGE(U32(r), U32(G + 0x560), U32(&bearing), U32(&range));
    bearing = (uint16_t)(P16(r, 0x1a) + (uint16_t)bearing);
    return (ld)(int32_t)bearing * K(0x20619100);
}

static ld FS98_STDCALL g_nav_offset(uint8_t *e)
{
    uint8_t *r = NAV(e);

    if (!(P8(r, 0x1c) & 0x80))
        return K(0x206190d0);
    return (ld)(int32_t)(uint16_t)(P16(r, 0x1e) - P16(r, 0x1a)) * K(0x20619100);
}

static ld FS98_STDCALL g_nav_glide(uint8_t *e)
{
    uint8_t *r = NAV(e);

    if (!(P8(r, 0x1c) & 0x40))
        return K(0x206190d0);
    return advdrv_asin((ld)(int32_t)P16(r, 0x4a) * K(0x20619150)) * K(0x20619158);
}

/* 0 is 360 */
static ld FS98_STDCALL g_course(uint8_t *e)
{
    uint16_t v = GV(e, uint16_t);

    if (v == 0)
        v = 0x168;
    return (ld)(int32_t)v;
}

static ld FS98_STDCALL g_nav_flag(uint8_t *e)
{
    return (ld)(int32_t)((P8(NAV(e), 0x1c) & (uint8_t)V_A(e)) != 0);
}

/* the DME, through MAIN, if the record says there is one */
static ld FS98_STDCALL g_dme(uint8_t *e)
{
    uint32_t tenths = U32(e);

    if (!(P8(NAV(e), 0x1c) & 1))
        return K(0x206190d0);
    if (MAIN_DME(V_A(e) != 1 ? 3 : 1, U32(&tenths)) != 0)
        return K(0x206190d0);
    return (ld)(uint64_t)tenths * K(0x20619160);
}

/* the marker or NDB record at the table's third entry */
#define NDB()  PTR(P32(PTR(G32(0x384)), 8))

static ld FS98_STDCALL g_ndb_on(uint8_t *e)
{
    (void)e;
    return (ld)(int32_t)(P16(NDB(), 0x18) != 0);
}

static ld FS98_STDCALL g_ndb_lat(uint8_t *e)
{
    uint8_t *r = NDB();

    (void)e;
    if (P16(r, 0x18) == 0)
        return K(0x206190d0);
    return lat_degrees(P32(r, 0), PS32(r, 4));
}

static ld FS98_STDCALL g_ndb_lon(uint8_t *e)
{
    uint8_t *r = NDB();

    (void)e;
    if (P16(r, 0x18) == 0)
        return K(0x206190d0);
    return lon_degrees(P32(r, 8), P32(r, 0xc));
}

static ld FS98_STDCALL g_ndb_feet(uint8_t *e)
{
    uint8_t *r = NDB();

    (void)e;
    if (P16(r, 0x18) == 0)
        return K(0x206190d0);
    return metres(P32(r, 0x10), PS32(r, 0x14)) * K(0x206190d8);
}

/* the height above the ground, as a 32-bit angle's scale */
static ld FS98_STDCALL g_ground(uint8_t *e)
{
    uint32_t v;

    (void)e;
    v = G32(0x580) - ((uint32_t)G16(0x2a0) << 16) + ((uint32_t)GS16(0xc6a) << 16);
    return (ld)(uint64_t)v * K(0x206190f0);
}

/* this image's own flags */
static ld FS98_STDCALL g_own(uint8_t *e)
{
    return (ld)PS32(PTR(V_WHERE(e)), 0);
}

static uint32_t FS98_STDCALL s_own(uint8_t *e, double v)
{
    if (fcom(v, K(0x206190d0)) & C3) {
        P32(PTR(V_WHERE(e)), 0) = 0;
        return 0;
    }
    P32(PTR(V_WHERE(e)), 0) = 1;
    return 1;
}

static ld FS98_STDCALL g_random(uint8_t *e)
{
    (void)e;
    return UTIL_FRAND();
}

/* seconds since the program started */
static ld FS98_STDCALL g_seconds(uint8_t *e)
{
    (void)e;
    return ((ld)(uint64_t)H_TICKS() - (ld)A_START) * K(0x20619168);
}

static ld FS98_STDCALL g_s32_scaled(uint8_t *e)
{
    return (ld)GV(e, int32_t) * K(0x20619170);
}

static ld FS98_STDCALL g_is1(uint8_t *e)
{
    return (ld)(int32_t)(GV(e, uint8_t) == 1);
}

/* a BCD transponder code */
static ld FS98_STDCALL g_squawk(uint8_t *e)
{
    uint32_t v = GV(e, uint16_t);
    ld result;

    result = (ld)(uint64_t)(v & 0xf) -
             ((ld)(uint64_t)((v >> 8) & 0xf) * K(0x20619128) +
              (ld)(uint64_t)((v >> 12) & 0xf) * K(0x20619178));
    return result - (ld)(uint64_t)((v >> 4) & 0xf) * K(0x20619118);
}

/* the track over the ground since two frames ago, relative to the heading */
static ld FS98_STDCALL g_track(uint8_t *e)
{
    uint8_t *old = PTR(A_HISTORY);
    uint8_t delta[8], out[8];
    uint16_t word;
    int32_t high;
    double east;
    ld north, angle;

    (void)e;
    if (P16(old, 2) == G16(0x562) && P32(old, 4) == G32(0x564) &&
        P16(old, 0xa) == G16(0x56a) && P32(old, 0xc) == G32(0x56c))
        return (ld)(uint64_t)(G32(0x580) - ((uint32_t)G16(0x2a0) << 16)) *
               K(0x206190f0);
    word = G16(0x56a);
    high = GS32(0x56c);
    if (word < P16(old, 0xa))
        high--;
    P16(delta, 2) = (uint16_t)(word - P16(old, 0xa));
    PS32(delta, 4) = high - PS32(old, 0xc);
    UTIL_METRES(U32(delta), U32(G + 0x560), U32(out));
    east = (double)metres(P32(out, 0), PS32(out, 4));
    old = PTR(A_HISTORY);
    word = G16(0x562);
    high = GS32(0x564);
    if (word < P16(old, 2))
        high--;
    P16(delta, 2) = (uint16_t)(word - P16(old, 2));
    north = metres(P32(delta, 0), high - PS32(old, 4));
    angle = advdrv_atan2(east, north) * K(0x20619158);
    if (fcom(angle, K(0x206190d0)) & C0)
        angle = angle - K(0x20619180);
    angle = angle - (ld)(int32_t)G16(0x2a0) * K(0x20619100);
    if (fcom(angle, K(0x206190d0)) & C0)
        angle = angle - K(0x20619180);
    return angle;
}

/* a control: positive over 16383, negative over GLOBAL +0xb00 */
static ld FS98_STDCALL g_control(uint8_t *e)
{
    int16_t v = GV(e, int16_t);

    if (v >= 0)
        return (ld)(int32_t)v * K(0x20619188);
    return (ld)(int32_t)v / (ld)(int32_t)-(int32_t)GS16(0xb00);
}

/* ---- the setters: .text+0x38a0 .. +0x41a0 ------------------------------------------ *
 * Error 6 for a value out of range. */

/* a and b bound an integer value */
static uint32_t FS98_STDCALL s_u8(uint8_t *e, double v)
{
    uint32_t n = (uint32_t)ftol(v);

    if (n < V_A(e) || n > V_B(e)) {
        error(6);
        return 0;
    }
    GV(e, uint8_t) = (uint8_t)n;
    return n;
}

static uint32_t FS98_STDCALL s_u16(uint8_t *e, double v)
{
    uint32_t n = (uint32_t)ftol(v);

    if (n < V_A(e) || n > V_B(e)) {
        error(6);
        return 0;
    }
    GV(e, uint16_t) = (uint16_t)n;
    return n;
}

static uint32_t FS98_STDCALL s_u32(uint8_t *e, double v)
{
    uint32_t n = (uint32_t)ftol(v);

    if (n < V_A(e) || n > V_B(e)) {
        error(6);
        return 0;
    }
    GV(e, uint32_t) = n;
    return n;
}

/* a for true, b for false */
static uint32_t FS98_STDCALL s_flag8(uint8_t *e, double v)
{
    GV(e, uint8_t) = (uint8_t)((fcom(v, K(0x206190d0)) & C3) ? V_B(e) : V_A(e));
    return V_WHERE(e);
}

static uint32_t FS98_STDCALL s_flag16(uint8_t *e, double v)
{
    GV(e, uint16_t) = (uint16_t)((fcom(v, K(0x206190d0)) & C3) ? V_B(e) : V_A(e));
    return V_WHERE(e);
}

static uint32_t FS98_STDCALL s_flag32(uint8_t *e, double v)
{
    GV(e, uint32_t) = (fcom(v, K(0x206190d0)) & C3) ? V_B(e) : V_A(e);
    return V_WHERE(e);
}

/* degrees into a 16-bit angle */
static uint32_t FS98_STDCALL s_angle16(uint8_t *e, double v)
{
    ld a = v;
    uint32_t n;

    if (!(fcom(a, K(0x20619190)) & C0)) {
        a = advdrv_fmod(v, K(0x20619190));
    } else if (fcom(a, K(0x206190d0)) & (C0 | C3)) {
        if (fcom(a, K(0x20619180)) & (C0 | C3))
            a = advdrv_fmod(v, K(0x20619190));
        a = a - K(0x20619180);
    }
    n = UTIL_LOW((double)(a * K(0x20619198)));
    GV(e, uint16_t) = (uint16_t)n;
    return n;
}

/* 0..1: command 0x100a2 */
static uint32_t FS98_STDCALL s_100a2(uint8_t *e, double v)
{
    (void)e;
    if ((fcom(v, K(0x206190d0)) & C0) || !(fcom(v, K(0x206191a0)) & (C0 | C3))) {
        error(6);
        return 0;
    }
    return H_COMMAND(0x100a2, (uint32_t)ftol((ld)v * K(0x206191a8)));
}

/* 0 or 1: command 0x100a4 */
static uint32_t FS98_STDCALL s_100a4(uint8_t *e, double v)
{
    (void)e;
    if (!(fcom(v, K(0x206190d0)) & C3) && !(fcom(v, K(0x206191a0)) & C3)) {
        error(6);
        return 0;
    }
    return H_COMMAND(0x100a4, (uint32_t)ftol(v));
}

/* .text+0x3cd0: binary to BCD, four digits */
static uint32_t FS98_STDCALL bcd(uint32_t n)
{
    uint32_t r;

    r = (n % 10) & 0xf;
    r |= ((n / 10 % 10) & 0xf) << 4;
    r |= ((n / 100 % 10) & 0xf) << 8;
    r |= (n / 1000 % 10) << 12;
    return r;
}

/* the COM frequency, 118..136.95 in 25 kHz steps where GLOBAL +0x358
 * allows them: command 0x100ab */
static uint32_t FS98_STDCALL s_com(uint8_t *e, double v)
{
    uint32_t khz, step, tenths, digits;

    (void)e;
    if ((fcom(v, K(0x206191b0)) & C0) || !(fcom(v, K(0x206191b8)) & (C0 | C3))) {
        error(6);
        return 0;
    }
    khz = UTIL_LOW((double)((ld)v * K(0x20619130))) % 100;
    step = 0;
#if BREAK==7
    if (khz == 50) {
        step = 7;
    } else if (G32(0x358) != 0) {
#else
    if (khz == 50) {
        step = 5;
    } else if (G32(0x358) != 0) {
#endif
        if (khz == 25)
            step = 2;
        else if (khz == 75)
            step = 7;
    }
    if ((uint16_t)step == 0 && khz != 0) {
        error(6);
        return 0;
    }
    tenths = (uint32_t)ftol((ld)v * K(0x206191c0));
    digits = step | ((tenths % 10) & 0xf) << 4;
    digits |= ((tenths / 10 % 10) & 0xf) << 8;
    digits = (digits & 0xffff) | (((tenths / 100 % 10) << 12) & 0xffff);
    return H_COMMAND(0x100ab, digits);
}

/* a NAV frequency, 108..117.95 in 50 kHz steps: commands 0x100ac (NAV1),
 * 0x100ad (NAV2) */
static uint32_t FS98_STDCALL s_nav(uint8_t *e, double v)
{
    uint32_t n;

    if ((fcom(v, K(0x206191c8)) & C0) || !(fcom(v, K(0x206191d0)) & (C0 | C3))) {
        error(6);
        return 0;
    }
    n = bcd(UTIL_LOW((double)(((ld)v - K(0x206191d8)) * K(0x206191d8)))) & 0xffff;
    if ((n & 0xf) != 0 && (n & 0xf) != 5) {
        error(6);
        return 0;
    }
    if (V_A(e) == 1)
        return H_COMMAND(0x100ac, n);
    if (V_A(e) == 2)
        return H_COMMAND(0x100ad, n);
    return n;
}

/* the ADF, 200..1699.9 (999 without GLOBAL +0x35c's extended range,
 * whole kilohertz) */
static uint32_t FS98_STDCALL s_adf(uint8_t *e, double v)
{
    uint32_t extended, whole, tenths, digits;

    (void)e;
    if ((fcom(v, K(0x206191e0)) & C0) || !(fcom(v, K(0x206191e8)) & (C0 | C3))) {
        error(6);
        return 0;
    }
    extended = G32(0x35c);
    if (extended == 0 && !(fcom(v, K(0x206191f0)) & (C0 | C3))) {
        error(6);
        return 0;
    }
    whole = (uint32_t)ftol(v);
    tenths = (uint32_t)ftol(((ld)v - (ld)(int32_t)whole) * K(0x206191c0));
    if (extended == 0 && tenths != 0) {
        error(6);
        return 0;
    }
    digits = bcd(whole);
    G16(0x34c) = (uint16_t)(digits & 0xfff);
    if (G32(0x35c) != 0) {
        G16(0x356) = (uint16_t)tenths;
        G16(0x356) |= (uint16_t)((digits >> 4) & 0xf00);
    }
    G8(0x389) |= 2;
    return digits;
}

/* an OBS, 0..360: commands 0x100b4 (NAV1), 0x100b5 (NAV2) */
static uint32_t FS98_STDCALL s_obs(uint8_t *e, double v)
{
    int32_t n = (int32_t)ftol(v);

    if (n > 0x168) {
        error(6);
        return 0;
    }
    if (n == 0x168)
        n = 0;
    if (V_A(e) == 1)
        return H_COMMAND(0x100b4, (uint32_t)n);
    if (V_A(e) == 2)
        return H_COMMAND(0x100b5, (uint32_t)n);
    return (uint32_t)n;
}

/* 0 or 1, unless the byte is 2 (locked) */
static uint32_t FS98_STDCALL s_switch(uint8_t *e, double v)
{
    uint8_t *p = G + V_WHERE(e);

    if (*p == 2)
        return 2;
    if (!(fcom(v, K(0x206190d0)) & C3) && !(fcom(v, K(0x206191a0)) & C3)) {
        error(6);
        return 0;
    }
    *p = (fcom(v, K(0x206190d0)) & C3) ? 0 : 1;
    return *p;
}

/* the transponder, 0..7777 in octal digits: command 0x100b3 */
static uint32_t FS98_STDCALL s_squawk(uint8_t *e, double v)
{
    uint32_t n;

    (void)e;
    if ((fcom(v, K(0x206190d0)) & C0) || !(fcom(v, K(0x206191f8)) & (C0 | C3))) {
        error(6);
        return 0;
    }
    n = bcd((uint32_t)ftol(v));
    if (n & 0x8888) {
        error(6);
        return 0;
    }
    return H_COMMAND(0x100b3, n & 0xffff);
}

/* a command: a for true, b for false */
static uint32_t FS98_STDCALL s_command(uint8_t *e, double v)
{
    if (fcom(v, K(0x206190d0)) & C3)
        return H_COMMAND(V_B(e), 0);
    return H_COMMAND(V_A(e), 0);
}

/* the visibility, in miles */
static uint32_t FS98_STDCALL s_visibility(uint8_t *e, double v)
{
    ld a = v;
    uint32_t n;

    if (!(fcom(a, K(0x20619200)) & (C0 | C3))) {
        error(6);
        a = K(0x20619200);
    }
    n = UTIL_LOW((double)(a * K(0x20619208)));
    GV(e, uint32_t) = n;
    return n;
}

/* the clock: hours, then the time of day is reset through MAIN */
static uint32_t FS98_STDCALL s_hours(uint8_t *e, double v)
{
    int32_t n = (int32_t)ftol(v);

    (void)e;
    if (n < 0 || n > 23) {
        error(6);
        return 0;
    }
    G8(0x238) = (uint8_t)n;
    MAIN_CLOCK(1, ((uint32_t)n & 0xffff0000u) | G16(0x23e));
    return MAIN_TIMEOFDAY();
}

static uint32_t FS98_STDCALL s_minutes(uint8_t *e, double v)
{
    int32_t n = (int32_t)ftol(v);

    (void)e;
    if (n < 0 || n > 59) {
        error(6);
        return 0;
    }
    G8(0x239) = (uint8_t)n;
    MAIN_CLOCK(1, ((uint32_t)n & 0xffff0000u) | G16(0x23e));
    return MAIN_TIMEOFDAY();
}

static uint32_t FS98_STDCALL s_seconds(uint8_t *e, double v)
{
    int32_t n = (int32_t)ftol(v);

    (void)e;
    if (n < 0 || n > 59) {
        error(6);
        return 0;
    }
    G8(0x23a) = (uint8_t)n;
    return (uint32_t)n;
}

/* a control, -1..1: command 0x1011b + a */
static uint32_t FS98_STDCALL s_control(uint8_t *e, double v)
{
    ld scaled;

    if (!(fcom(v, K(0x206191a0)) & (C0 | C3)) || (fcom(v, K(0x20619210)) & C0)) {
        error(6);
        return 0;
    }
    if (!(fcom(v, K(0x206190d0)) & (C0 | C3)))
        scaled = (ld)v * K(0x206191a8);
    else
        scaled = (ld)(int32_t)-(int32_t)GS16(0xb00) * (ld)v;
    return H_COMMAND(V_A(e) + 0x1011b, (uint32_t)ftol(scaled));
}

/* the elevator trim, -1..1: command 0x100aa */
static uint32_t FS98_STDCALL s_trim(uint8_t *e, double v)
{
    (void)e;
    if (!(fcom(v, K(0x206191a0)) & (C0 | C3)) || (fcom(v, K(0x20619210)) & C0)) {
        error(6);
        return 0;
    }
    return H_COMMAND(0x100aa, UTIL_ROUND((double)((ld)v * K(0x20619218))));
}

/* ---- keys: .text+0x41a0 .. +0x4390 ------------------------------------------------- *
 * A program binds keys through a host key map; a bound key arrives as
 * command 0x1e000 + key on CHAIN list 0xb. */

/* .text+0x41a0 */
static void keys_clear(void)
{
    H_MEMSET(U32(D + A_KEYS), 0, 0x800);
}

/* .text+0x41c0: unbind every key, and free the map */
static void keys_release(void)
{
    uint32_t key;

    for (key = 0; key <= 0x7ff; key++) {
        if (D8(A_KEYS + key) != 0)
            key_unbind(key);
    }
    H_KEYS_FREE(U32(&A_KEYMAP));
}

/* .text+0x41f0 */
static void FS98_STDCALL key_bind(uint32_t key)
{
    uint32_t binding[3];

    if (key > 0x7ff)
        return;
    if (A_KEYMAP == 0 && H_KEYS_NEW(U32(&A_KEYMAP)) == 0)
        H_KEYS_ON(A_KEYMAP);
    if (D8(A_KEYS + key) == 0) {
        binding[0] = key & 0xff;
#if BREAK==8
        binding[1] = key >> 8;
#else
        binding[1] = (key | 0x800) >> 8;
#endif
        binding[2] = key + 0x1e000;
        H_KEYS_ADD(A_KEYMAP, U32(binding), 1);
    }
    D8(A_KEYS + key) = 1;
}

/* .text+0x4290 */
static void FS98_STDCALL key_unbind(uint32_t key)
{
    uint32_t binding[3];

    if (key > 0x7ff)
        return;
    if (D8(A_KEYS + key) != 0) {
        binding[0] = key & 0xff;
        binding[1] = (key | 0x800) >> 8;
        binding[2] = key + 0x1e000;
        H_KEYS_DROP(A_KEYMAP, U32(binding), 1);
    }
    D8(A_KEYS + key) = 0;
}

/* .text+0x4300, on CHAIN list 0xb: a bound key is ours, and runs its
 * handler if one is bound to it; 0x10001 stops the program */
static uint32_t FS98_STDCALL on_command(uint32_t command, uint32_t *handled)
{
    uint32_t key, i;

    if (command < 0x1e000 || command > 0x1efff) {
        if (command == 0x10001)
            stop();
        *handled = 0;
        return 0;
    }
    key = command - 0x1e000;
    *handled = 1;
    if (A_IN_HANDLER == 0) {
        if (A_HANDLER != -1)
            return 0;
        for (i = 0; i < 20; i++) {
            if (D32(A_BINDINGS + 8 * i) == key) {
                A_HANDLER = (int32_t)D32(A_BINDINGS + 8 * i + 4);
                break;
            }
        }
    }
    if (A_HANDLER == -1)
        A_KEY = key;
    return 0;
}

/* ---- coordinates: .text+0x4390 .. +0x4630 ------------------------------------------ */

/* .text+0x4390: degrees of latitude into a 48-bit position */
static void FS98_STDCALL to_lat(double v, uint8_t *out)
{
    ld a = v;

    if (!(fcom(a, K(0x20619220)) & C0)) {
        a = advdrv_fmod(v, K(0x20619228));
        if (!(fcom(a, K(0x20619220)) & C0))
            a = a - K(0x20619228);
    } else if (fcom(a, K(0x20619230)) & C0) {
        a = advdrv_fmod(v, K(0x20619228));
        if (fcom(a, K(0x20619230)) & C0)
            a = a - K(0x20619238);
    }
#if BREAK==3
    if (fcom(a, K(0x20619248)) & C0)
        a = K(0x20619248);
#else
    if (!(fcom(a, K(0x20619240)) & (C0 | C3)))
        a = K(0x20619240);
    else if (fcom(a, K(0x20619248)) & C0)
        a = K(0x20619248);
#endif
    if (A_SOUTH == 1)
        a = -a;
    to_48((double)(a * K(0x20619250)), out);
}

/* .text+0x4450 */
static ld FS98_STDCALL lat_degrees(uint32_t low, int32_t high)
{
    ld a = metres(low, high) * K(0x20619258);

    if (A_SOUTH == 1)
        return -a;
    return a;
}

static void FS98_STDCALL lon_48(double v, uint8_t *out);

/* .text+0x4480: degrees of longitude, west unless east is set */
static void FS98_STDCALL to_lon(double v, uint8_t *out)
{
    if (A_EAST == 0)
        v = (double)(K(0x20619228) - (ld)v);
    lon_48(v, out);
}

/* .text+0x44b0 */
static ld FS98_STDCALL lon_degrees(uint32_t low, uint32_t high)
{
    ld a = (ld)(uint64_t)high * K(0x20619260);   /* .text+0x44e0 */

    (void)low;
    if (A_EAST == 0)
        return K(0x20619228) - a;
    return a;
}

/* .text+0x4500 */
static void FS98_STDCALL lon_48(double v, uint8_t *out)
{
    ld a = v;

    if (!(fcom(a, K(0x20619228)) & C0)) {
        a = advdrv_fmod(v, K(0x20619228));
    } else if (fcom(a, K(0x20619268)) & (C0 | C3)) {
        if (fcom(a, K(0x20619238)) & (C0 | C3))
            a = advdrv_fmod(v, K(0x20619228));
        a = a - K(0x20619238);
    }
    to_48((double)(a * K(0x20619270)), out);
}

/* .text+0x4570: a value into 48-bit fixed point, +2..+7 of `out`: the
 * whole part truncated, the fraction's magnitude -- so a negative value is
 * sign and magnitude -- after 2^-17 is added (subtracted from zero). */
static void FS98_STDCALL to_48(double v, uint8_t *out)
{
    double whole;
    ld fraction;
    int32_t high;

    if (fcom(v, K(0x20619268)) & C3)
        v = (double)((ld)v - K(0x20619280));
    else
        v = (double)((ld)v - K(0x20619278));
    fraction = advdrv_modf(v, &whole);
    high = (int32_t)ftol(whole);
#if BREAK==9
    fraction = fraction * K(0x20619288);
#else
    fraction = __builtin_fabsl(fraction * K(0x20619288));
#endif
    PS32(out, 4) = high;
    P16(out, 2) = (uint16_t)ftol(fraction);
}

/* .text+0x45e0: a 48-bit value as a number */
static ld FS98_STDCALL metres(uint32_t low, int32_t high)
{
    ld fraction = (ld)(int32_t)(low >> 16) * K(0x20619290);

    if (high > 0)
        return (ld)high + fraction;
    return (ld)high - fraction;
}

/* .text+0x4630 */
static void FS98_STDCALL con_write(const char *s)
{
    DWORD written;

    WriteFile((HANDLE)(uintptr_t)A_CONSOLE, s, H_STRLEN(U32(s)), &written, NULL);
}

/* .text+0x4660: say what went wrong, on the console */
static void error(uint32_t which)
{
    char buf[0x400];

    COVER(0x300 + (which < 8 ? which : 8));
    if (which < 8) {
        H_LOADSTRING(D32(A_ERRORS + 4 * which), U32(buf), 0x400);
        H_STRCAT(U32(buf), U32(A_NEWLINE));
        con_write(buf);
    }
}

/* ---- the pickers: .text+0x46c0 .. +0x4970 ------------------------------------------ */

static uint32_t FS98_STDCALL title_of(const char *path, char *out);
static uint32_t FS98_STDCALL description_of(const char *path, char *out);

/* The catalogue's block, for the host's dialog. */
struct adv_picker {
    uint32_t ok;            /* +0x00 */
    uint32_t unused;        /* +0x04 */
    uint32_t count;         /* +0x08 */
    int32_t chosen;         /* +0x0c */
    uint32_t list;          /* +0x10: 0xa08-byte entries, the path at +0x900 */
    uint32_t lesson;        /* +0x14 */
};

/* .text+0x47a0 */
static uint32_t FS98_STDCALL picker(uint32_t dialog, uint32_t strings, uint32_t type)
{
    struct adv_picker block;

    A_DIALOG_EXT = type;
    UTIL_LIST(strings, 0, type, U32(&block.count), U32(&block.list),
              U32(title_of), U32(description_of), 0, 1);
    block.chosen = 0;
    H_DIALOG(dialog, U32(&block));
    if (block.ok != 0 && block.chosen != -1) {
        stop();
        A_LESSON = 0;
        adv_load((const char *)PTR(block.list + 0xa08 * (uint32_t)block.chosen + 0x900),
                 type);
    }
    H_FREE(U32(&block.list));
    return block.ok;
}

/* .text+0x46c0, Linkage +0x28: the adventures */
static uint32_t adv_adventures(void)
{
    return picker(0x1108, 0x9009, 0x1a);
}

/* .text+0x46e0, Linkage +0x2c: the lessons, which remember where they were */
static uint32_t adv_lessons(void)
{
    struct adv_picker block;

    A_DIALOG_EXT = 0x20;
    UTIL_LIST(0x900b, 0, 0x20, U32(&block.count), U32(&block.list),
              U32(title_of), U32(description_of), 0, 2);
    block.lesson = A_LESSON;
    H_DIALOG(0x1106, U32(&block));
    if (block.ok != 0 && block.chosen != -1) {
        stop();
        A_LESSON = block.lesson;
        adv_load((const char *)PTR(block.list + 0xa08 * (uint32_t)block.chosen + 0x900),
                 A_DIALOG_EXT);
    }
    H_FREE(U32(&block.list));
    return block.ok;
}

/* .text+0x4850: the catalogue's description of a program: header +0x50 */
static uint32_t FS98_STDCALL description_of(const char *path, char *out)
{
    uint32_t file;
    uint8_t header[0x200];

    *out = 0;
    if (H_OPEN(U32(&file), U32(path), A_DIALOG_EXT, 0) == (uint32_t)-1)
        return 0;
    if (read_header(file, header) == 0)
        H_STRCPY(U32(out), U32(header + 0x50));
    H_CLOSE(file);
    return 1;
}

/* .text+0x48e0: and its title, header +0x10 */
static uint32_t FS98_STDCALL title_of(const char *path, char *out)
{
    uint32_t file;
    uint8_t header[0x200];

    *out = 0;
    if (H_OPEN(U32(&file), U32(path), A_DIALOG_EXT, 0) == (uint32_t)-1)
        return 0;
    if (read_header(file, header) == 0)
        H_STRCPY(U32(out), U32(header + 0x10));
    H_CLOSE(file);
    return 1;
}

/* .text+0x4970, 0x1a .. 0x20: by the instruction's own byte */
static void op_weather(void)
{
    uint8_t which = (uint8_t)(*(PTR(A_PC) - 1) - 0x1a);

    if (which >= 7) {
        op_error();
        return;
    }
    ((void (*)(void))(uintptr_t)D32(A_WEATHER + 4 * which))();
}

/* ---- the weather: .text+0x49a0 .. +0x6470 ------------------------------------------ *
 * Instructions 0x1a..0x20 write a weather area into GLOBAL +0xf90..+0x1074
 * for WEATHERM: its name and corners, then its surface conditions, up to
 * two cloud layers and a thunderstorm layer, three wind layers and a
 * surface wind, four temperature layers, the pressure and the visibility.
 * Layers are kept sorted by altitude, each under the id the program gave
 * it; ids are words. */

/* clamp to [lo, hi] as retail does: a NaN becomes lo */
static ld clamp(ld v, double hi, double lo)
{
    if (!(fcom(v, hi) & (C0 | C3)))
        return hi;
    if (fcom(v, lo) & C0)
        return lo;
    return v;
}

/* .text+0x49a0, 0x1a: a new area, (name, lat, lon, lat, lon) */
static void w_area(void)
{
    uint8_t p[8];

    H_STRCPY(U32(G + 0xfe2), A_PC);
    A_PC = A_PC + H_STRLEN(A_PC) + 1;
    to_lat(eval(), p);
    G32(0xf98) = P32(p, 4);
    G16(0xf96) = P16(p, 2);
    to_lon(eval(), p);
    G32(0xfa0) = P32(p, 4);
    G16(0xf9e) = P16(p, 2);
    to_lat(eval(), p);
    G32(0xfa8) = P32(p, 4);
    G16(0xfa6) = P16(p, 2);
    to_lon(eval(), p);
    G32(0xfb0) = P32(p, 4);
    G16(0xfae) = P16(p, 2);
    G32(0xfb4) = 0;
    G32(0xfb8) = 0;
    G16(0xfbc) = 0;
    G16(0xfbe) = 0;
    G16(0x1004) = 0; G16(0x1006) = 0; G16(0x1008) = 0; G16(0x100a) = 0;
    G16(0x100c) = 0; G16(0x100e) = 0; G16(0x1010) = 0; G16(0x1012) = 0;
    G16(0x1014) = 0; G16(0x1016) = 0; G16(0x1018) = 0; G16(0x101a) = 0;
    G16(0x1034) = 0; G16(0x1036) = 0; G16(0x1038) = 0; G16(0x103a) = 0;
    G16(0x103c) = 0; G16(0x103e) = 0; G16(0x1040) = 0; G16(0x1042) = 0;
    G16(0x1044) = 0; G16(0x1046) = 0; G16(0x1048) = 0; G16(0x104a) = 0;
    G16(0x104c) = 0; G16(0x104e) = 0; G16(0x1050) = 0; G16(0x1052) = 0;
    G16(0x1054) = 0; G16(0x1056) = 0; G16(0x1058) = 0; G16(0x105a) = 0;
    G16(0x105c) = 0; G16(0x105e) = 0; G16(0x1060) = 0;
    G16(0x101c) = 0xd8f1; G16(0x101e) = 0;
    G16(0x1020) = 0xd8f1; G16(0x1022) = 0;
    G16(0x1024) = 0xd8f1; G16(0x1026) = 0;
    G16(0x1028) = 0xd8f1; G16(0x102a) = 0;
    G16(0x1030) = 0x3f53;
    G16(0x1032) = 0;
    G16(0x1062) = 0; G16(0x1064) = 0; G16(0x1066) = 0; G16(0x1068) = 0;
    G16(0x106a) = 0; G16(0x106c) = 0; G16(0x106e) = 0; G16(0x1070) = 0;
    G16(0x1072) = 0;
    G16(0x1074) = 0x7d00;
    A_LAYER(0x1930) = 0;
    A_LAYER(0x1928) = 0;
    A_LAYER(0x192c) = 0;
    A_LAYER(0x1934) = 0;
    A_LAYER(0x1938) = 0;
    A_LAYER(0x193c) = 0;
    A_LAYER(0x1940) = 0;
    A_LAYER(0x1944) = 0;
    A_LAYER(0x1948) = 0;
    A_LAYER(0x194c) = 0;
    A_LAYER(0x1950) = 0;
    G16(0xf90) = 1;
}

/* .text+0x4e40, 0x1b: the area's surface conditions */
static void w_surface(void)
{
    if (G16(0xf90) == 0) {
        eval_int();
        eval_int();
        eval_int();
        eval_int();
        return;
    }
    G32(0xfb4) = UTIL_LOW((double)(clamp(eval(), K(0x20619298), K(0x206192a0)) *
                                   K(0x206192a8)));
    G32(0xfb8) = UTIL_LOW((double)(clamp(eval(), K(0x206192b0), K(0x206192a0)) *
                                   K(0x206192a8)));
    G16(0xfbc) = (uint16_t)UTIL_LOW((double)(clamp(eval(), K(0x206192b8),
                                                   K(0x206192a0)) * K(0x206192c0)));
    G16(0xfbe) = (uint16_t)UTIL_LOW((double)(clamp(eval(), K(0x206192b0),
                                                   K(0x206192a0)) * K(0x206192c8)));
}

/* .text+0x54c0: drop cloud layer `id` */
static uint32_t FS98_STDCALL cloud_remove(uint32_t id)
{
    uint16_t lower;

    if (id == A_LAYER(0x192c)) {
        A_LAYER(0x192c) = 0;
        G16(0x1014) = 0; G16(0x1016) = 0; G16(0x1018) = 0; G16(0x101a) = 0;
        G16(0x106e) = 0; G16(0x1070) = 0; G16(0x1072) = 0;
        return 1;
    }
    lower = A_LAYER(0x1928);
    if (id == lower) {
        A_LAYER(0x1928) = A_LAYER(0x1930);
        G16(0x100c) = G16(0x1004);
        G16(0x100e) = G16(0x1006);
        G16(0x1010) = G16(0x1008);
        G16(0x1012) = G16(0x100a);
        G16(0x1068) = G16(0x1062);
        G16(0x106a) = G16(0x1064);
        G16(0x106c) = G16(0x1066);
        lower = A_LAYER(0x1928);
    }
    if (id != lower && id != A_LAYER(0x1930))
        return 0;
    A_LAYER(0x1930) = 0;
    G16(0x1004) = 0; G16(0x1006) = 0; G16(0x1008) = 0; G16(0x100a) = 0;
    G16(0x1062) = 0; G16(0x1064) = 0; G16(0x1066) = 0;
    return 1;
}

/* .text+0x4fc0, 0x1c: a cloud layer (id, then 0 to drop it or 1 and base,
 * top in feet, type, cover in octas, turbulence, icing, precipitation).
 * Type 10, the thunderstorm, has a layer of its own. */
static void w_cloud(void)
{
    uint32_t id, has, base = 0, top = 0, cover = 0, turbulence = 0, icing = 0;
    uint32_t precip = 0, limit, flag;
    uint16_t type = 0, b, t, cv, tu, ic;
    uint16_t o_id, o_top, o_base, o_cover, o_icing, o_type, o_flag, o_turb;
    uint8_t *row;

    id = (uint32_t)eval_int();
    has = PC8();
    A_PC++;
    if (has != 0) {
        base = UTIL_LOW((double)((ld)eval() * K(0x206192d0)));
        top = UTIL_LOW((double)((ld)eval() * K(0x206192d0)));
        type = PC8();
        A_PC++;
        cover = (uint32_t)eval_int();
        turbulence = (uint32_t)eval_int();
        icing = (uint32_t)eval_int();
        precip = (uint32_t)eval_int() & 0xffff;
    }
    if (G16(0xf90) == 0 || (uint16_t)id > 0x7fff || (uint16_t)id < 1)
        return;
    if (has == 0) {
        cloud_remove(id & 0xffff);
        return;
    }
    if (type > 10)
        type = 0;
    row = D + A_CLOUDS + 18 * type;
    if ((uint16_t)base > P16(row, 2))
        base = P16(row, 2);
    else if ((uint16_t)base < P16(row, 0))
        base = P16(row, 0);
    b = (uint16_t)base;
    t = (uint16_t)top;
    if ((int32_t)t < (int32_t)b + P16(row, 4))
        t = (uint16_t)(b + P16(row, 4));
    else if ((int32_t)t > (int32_t)b + P16(row, 6))
        t = (uint16_t)(b + P16(row, 6));
    limit = type != 10 ? 8 : 2;
    if ((uint16_t)cover > limit)
        cover = limit;
    if (type == 10)
        cv = D16(A_COVER_10 + 2 * (cover & 0xffff));
    else
        cv = D16(A_COVER + 2 * (cover & 0xffff));
    tu = (uint16_t)turbulence;
    if (tu > P16(row, 0xe))
        tu = P16(row, 0xe);
    else if (tu < P16(row, 0xc))
        tu = P16(row, 0xc);
    ic = (uint16_t)icing;
    if (ic > P16(row, 0x10))
        ic = P16(row, 0x10);
    flag = precip != 0;
    if ((uint16_t)id == A_LAYER(0x192c) && (int32_t)t > (int32_t)GS16(0x1016) &&
        (int32_t)b < (int32_t)GS16(0x1014))
        return;
    if ((uint16_t)id == A_LAYER(0x1928) && (int32_t)t > (int32_t)GS16(0x100e) &&
        (int32_t)b < (int32_t)GS16(0x100c))
        return;
    if ((uint16_t)id == A_LAYER(0x1930) && (int32_t)t > (int32_t)GS16(0x1006) &&
        (int32_t)b < (int32_t)GS16(0x1004))
        return;
    if (type == 10) {
        if (G16(0x1014) != 0 && (uint16_t)id != A_LAYER(0x192c))
            return;
    } else {
        if (G16(0x1004) != 0 && (uint16_t)id != A_LAYER(0x1928) &&
            (uint16_t)id != A_LAYER(0x1930))
            return;
    }
    cloud_remove(id & 0xffff);
    if (type == 10) {
        A_LAYER(0x192c) = (uint16_t)id;
        G16(0x1014) = t;
        G16(0x1016) = b;
        G16(0x1018) = cv;
        G16(0x101a) = ic;
        G16(0x106e) = 10;
        G16(0x1070) = (uint16_t)flag;
        G16(0x1072) = tu;
        return;
    }
    o_base = G16(0x100e);
    if (o_base != 0 && (int32_t)b > (int32_t)(int16_t)o_base)
        goto upper;
    o_id = A_LAYER(0x1928);
    o_type = G16(0x1068);
    o_top = G16(0x100c);
    o_flag = G16(0x106a);
    o_cover = G16(0x1010);
    o_icing = G16(0x1012);
    o_turb = G16(0x106c);
    A_LAYER(0x1928) = (uint16_t)id;
    G16(0x100c) = t;
    G16(0x100e) = b;
    G16(0x1010) = cv;
    G16(0x1012) = ic;
    G16(0x1068) = type;
    G16(0x106a) = (uint16_t)flag;
    G16(0x106c) = tu;
    if (o_base == 0)
        return;
    /* the old lower layer moves up */
    id = o_id;
    t = o_top;
    b = o_base;
    cv = o_cover;
    ic = o_icing;
    tu = o_turb;
    type = o_type;
    flag = o_flag;
upper:
    A_LAYER(0x1930) = (uint16_t)id;
    G16(0x1004) = t;
    G16(0x1006) = b;
    G16(0x1008) = cv;
    G16(0x100a) = ic;
    G16(0x1062) = type;
    G16(0x1064) = (uint16_t)flag;
    G16(0x1066) = tu;
}

/* .text+0x5b90: drop wind layer `id`; the surface layer is +0x1950 */
static uint32_t FS98_STDCALL wind_remove(uint32_t id)
{
    uint16_t a, b;

    if (id == A_LAYER(0x1950)) {
        A_LAYER(0x1950) = 0;
        G16(0x1058) = 0; G16(0x105a) = 0; G16(0x105c) = 0; G16(0x105e) = 0;
        G16(0x1060) = 0;
        return 1;
    }
    a = A_LAYER(0x194c);
    if (id == a) {
        A_LAYER(0x194c) = A_LAYER(0x1948);
        G16(0x104c) = G16(0x1040);
        G16(0x104e) = G16(0x1042);
        G16(0x1050) = G16(0x1044);
        G16(0x1052) = G16(0x1046);
        G16(0x1054) = G16(0x1048);
        G16(0x1056) = G16(0x104a);
        a = A_LAYER(0x194c);
    }
    b = A_LAYER(0x1948);
    if (id == a || id == b) {
        A_LAYER(0x1948) = A_LAYER(0x1944);
        G16(0x1040) = G16(0x1034);
        G16(0x1042) = G16(0x1036);
        G16(0x1044) = G16(0x1038);
        G16(0x1046) = G16(0x103a);
        G16(0x1048) = G16(0x103c);
        G16(0x104a) = G16(0x103e);
        a = A_LAYER(0x194c);
        b = A_LAYER(0x1948);
    }
    if (id != a && id != b && id != A_LAYER(0x1944))
        return 0;
    A_LAYER(0x1944) = 0;
    G16(0x1034) = 0; G16(0x1036) = 0; G16(0x1038) = 0; G16(0x103a) = 0;
    G16(0x103c) = 0; G16(0x103e) = 0;
    return 1;
}

/* .text+0x5670, 0x1d: a wind layer (id, then 0 to drop it or 1 and base,
 * top in feet, turbulence, direction, speed, gusts).  Base 0 is the
 * surface wind, whose direction is relative to the heading. */
static void w_wind(void)
{
    uint32_t id, has, base = 0, top = 0, heading_n, speed = 0, gust = 0;
    uint16_t si, di, bp, top16, t20, t18, t1c, t14, o_id = 0, cx, dx, bx;
    uint16_t s28 = 0, s24 = 0, s2c = 0, s30 = 0;
    uint8_t turbulent = 0;
    double heading = 0;

    id = (uint32_t)eval_int();
    has = PC8();
    A_PC++;
    if (has != 0) {
        top = UTIL_LOW((double)((ld)eval() * K(0x206192d0)));
        base = UTIL_LOW((double)((ld)eval() * K(0x206192d0)));
        turbulent = PC8();
        A_PC++;
        heading = eval();
        speed = (uint32_t)eval_int();
        gust = (uint32_t)eval_int();
    }
    si = (uint16_t)id;
    if (G16(0xf90) == 0 || si > 0x7fff || si < 1)
        return;
    if (has == 0) {
        wind_remove(id & 0xffff);
        return;
    }
    bp = (uint16_t)top;
    if (bp > 0x7718)
        bp = 0x7718;
    top16 = bp;
    di = (uint16_t)base;
    if (di > 0x7718)
        di = 0x7718;
    else if (di < 1)
        di = 1;
    t20 = turbulent != 0;
    if (!(fcom(heading, K(0x206192b8)) & (C0 | C3)) ||
        (fcom(heading, K(0x206192a0)) & C0))
        heading = 0;
    heading_n = UTIL_LOW((double)((ld)heading * K(0x206192c0)));
    t1c = (uint16_t)heading_n;
    if (bp == 0)
        t1c = (uint16_t)(t1c + G16(0x2a0));
    t18 = (uint16_t)speed;
    if (t18 > 0xc7)
        t18 = 0xc7;
    t14 = (uint16_t)gust;
    if (t14 > 0xff)
        t14 = 0xff;
    if (si == A_LAYER(0x1950) && di < G16(0x1058))
        return;
    bx = A_LAYER(0x194c);
    if (si == bx && di > G16(0x104e) && bp < G16(0x104c))
        return;
    dx = A_LAYER(0x1948);
    if (si == dx && di > G16(0x1042) && bp < G16(0x1040))
        return;
    cx = A_LAYER(0x1944);
    if (si == cx && di > G16(0x1036) && bp < G16(0x1034))
        return;
    if (bp == 0) {
        if (G16(0x1058) != 0 && si != A_LAYER(0x1950))
            return;
    } else {
        if (G16(0x1034) != 0 && si != bx && si != dx && si != cx)
            return;
    }
    wind_remove(si);
    if (bp == 0) {
        A_LAYER(0x1950) = si;
        G16(0x1058) = di;
        G16(0x105a) = t18;
        G16(0x105c) = t1c;
        G16(0x105e) = t14;
        G16(0x1060) = t20;
        return;
    }
    /* the first of the three layers above the surface */
    cx = 0;
    bp = G16(0x104e);
    if (bp != 0) {
        if (top16 > bp)
            goto second;
        cx = bp;
        s28 = G16(0x1050);
        s24 = G16(0x1052);
        o_id = A_LAYER(0x194c);
        bx = G16(0x104c);
        s2c = G16(0x1054);
        s30 = G16(0x1056);
    } else {
        o_id = 0;
        bx = 0;
    }
    A_LAYER(0x194c) = si;
    G16(0x104c) = di;
    G16(0x104e) = top16;
    G16(0x1050) = t18;
    G16(0x1052) = t1c;
    G16(0x1054) = t14;
    G16(0x1056) = t20;
    if (cx == 0)
        return;
    /* the old first layer moves up -- keeping the new layer's top, which
     * retail never reloads */
    t18 = s28;
    t1c = s24;
    t14 = s2c;
    si = o_id;
    di = bx;
    t20 = s30;
second:
    bp = G16(0x1042);
    cx = 0;
    if (bp != 0) {
        cx = top16;
        if (cx > bp)
            goto third;
        cx = bp;
        s28 = G16(0x1044);
        s24 = G16(0x1046);
        o_id = A_LAYER(0x1948);
        bx = G16(0x1040);
        s2c = G16(0x1048);
        s30 = G16(0x104a);
    }
    A_LAYER(0x1948) = si;
    G16(0x1040) = di;
    G16(0x1042) = top16;
    G16(0x1044) = t18;
    G16(0x1046) = t1c;
    G16(0x1048) = t14;
    G16(0x104a) = t20;
    if (cx == 0)
        return;
    t20 = s30;
    bp = s2c;
    si = o_id;
    dx = s24;
    di = bx;
    bx = s28;
    goto store;
third:
    bp = t14;
    bx = t18;
    dx = t1c;
store:
    A_LAYER(0x1944) = si;
    G16(0x1034) = di;
    G16(0x1036) = cx;
    G16(0x1038) = bx;
    G16(0x103a) = dx;
    G16(0x103c) = bp;
    G16(0x103e) = t20;
}

/* .text+0x6170: drop temperature layer `id` */
static uint32_t FS98_STDCALL temperature_remove(uint32_t id)
{
    uint16_t a, b, c;

    a = A_LAYER(0x1940);
    if (id == a) {
        A_LAYER(0x1940) = A_LAYER(0x193c);
        G16(0x1028) = G16(0x1024);
        G16(0x102a) = G16(0x1026);
        a = A_LAYER(0x1940);
    }
    b = A_LAYER(0x193c);
    if (id == a || id == b) {
        A_LAYER(0x193c) = A_LAYER(0x1938);
        G16(0x1024) = G16(0x1020);
        G16(0x1026) = G16(0x1022);
        a = A_LAYER(0x1940);
        b = A_LAYER(0x193c);
    }
    c = A_LAYER(0x1938);
    if (id == a || id == b || id == c) {
        A_LAYER(0x1938) = A_LAYER(0x1934);
        G16(0x1020) = G16(0x101c);
        G16(0x1022) = G16(0x101e);
        a = A_LAYER(0x1940);
        b = A_LAYER(0x193c);
        c = A_LAYER(0x1938);
    }
    if (id != a && id != b && id != c && id != A_LAYER(0x1934))
        return 0;
    A_LAYER(0x1934) = 0;
    G16(0x101c) = 0xd8f1;
    G16(0x101e) = 0;
    return 1;
}

/* is `alt` within 100 of layer altitude `at` (or the layer empty)? */
static int temperature_near(uint16_t alt, uint16_t at)
{
    int32_t d;

    if (at == 0xd8f1)
        return 1;
    if ((int16_t)alt > (int16_t)at)
        d = (int16_t)alt - (int16_t)at;
    else
        d = (int16_t)at - (int16_t)alt;
    return d < 100;
}

/* .text+0x5dc0, 0x1e: a temperature layer (id, then 0 to drop it or 1 and
 * altitude in feet, degrees, the day's range).  Kept in four slots from
 * the highest; the lowest slot's id is never recorded. */
static void w_temperature(void)
{
    uint32_t id, has, n;
    uint16_t bp, si, temp = 0, range = 0, alt = 0, old, ax, bx, dx, di, edi;
    uint16_t on;

    id = (uint32_t)eval_int();
    has = PC8();
    A_PC++;
    if (has != 0) {
        alt = (uint16_t)UTIL_ROUND((double)((ld)eval() * K(0x206192d0)));
        temp = (uint16_t)UTIL_ROUND((double)((ld)eval() * K(0x206192d8)));
        n = UTIL_LOW((double)((ld)eval() * K(0x206192d8)));
        range = (uint16_t)n;
    }
    bp = (uint16_t)id;
    if (G16(0xf90) == 0 || bp > 0x7fff || bp < 1)
        return;
    if (has == 0) {
        temperature_remove(id & 0xffff);
        return;
    }
    si = alt;
    if ((int16_t)si > 0x7718)
        si = 0x7718;
    else if ((int16_t)si < 0)
        si = 0;
    if ((int16_t)temp > 0x4b00)
        temp = 0x4b00;
    else if ((int16_t)temp < (int16_t)0xa100)
        temp = 0xa100;
    if (range > 0x1400)
        range = 0x1400;
    on = range != 0;
    if (bp == A_LAYER(0x1940) && !temperature_near(si, G16(0x1028)))
        return;
    bx = A_LAYER(0x193c);
    if (bp == bx && !temperature_near(si, G16(0x1024)))
        return;
    di = A_LAYER(0x1938);
    if (bp == di && !temperature_near(si, G16(0x1020)))
        return;
    dx = A_LAYER(0x1934);
    if (bp == dx && !temperature_near(si, G16(0x101c)))
        return;
    if (G16(0x101c) != 0xd8f1 && bp != A_LAYER(0x1940) && bp != bx &&
        bp != di && bp != dx)
        return;
    temperature_remove(bp);
    /* the highest slot */
    ax = 0xd8f1;
    old = G16(0x1028);
    edi = 0;
    dx = 0;
    if (old != 0xd8f1) {
        if ((int16_t)si > (int16_t)old)
            goto second;
        edi = A_LAYER(0x1940);
        ax = old;
        dx = G16(0x102a);
    }
    A_LAYER(0x1940) = bp;
    G16(0x1028) = si;
    G16(0x102a) = temp;
    if (ax == 0xd8f1)
        goto done;
    si = ax;
    bp = edi;
    temp = dx;
    ax = 0xd8f1;
second:
    bx = G16(0x1024);
    if (bx != 0xd8f1) {
        if ((int16_t)si > (int16_t)bx) {
            bx = temp;
            goto third;
        }
        edi = A_LAYER(0x193c);
        dx = G16(0x1026);
        ax = bx;
    }
    A_LAYER(0x193c) = bp;
    G16(0x1024) = si;
    G16(0x1026) = temp;
    if (ax == 0xd8f1)
        goto done;
    si = ax;
    bp = edi;
    bx = dx;
    ax = 0xd8f1;
third:
    old = G16(0x1020);
    if (old != 0xd8f1) {
        if ((int16_t)si > (int16_t)old)
            goto fourth;
        dx = G16(0x1022);
        ax = old;
    }
    A_LAYER(0x1938) = bp;
    G16(0x1020) = si;
    G16(0x1022) = bx;
    if (ax == 0xd8f1)
        goto done;
    si = ax;
    bx = dx;
fourth:
    G16(0x101c) = si;
    G16(0x101e) = bx;
done:
    G16(0x102c) = on;
    G16(0x102e) = range;
}

/* .text+0x6300, 0x1f: the pressure, in inches, and whether it drifts */
static void w_pressure(void)
{
    double inches = eval();
    int32_t drift = eval_int();

    if (G16(0xf90) == 0)
        return;
    G16(0x1030) = (uint16_t)UTIL_LOW((double)(clamp(inches, K(0x206192e0),
                                                    K(0x206192e8)) * K(0x206192f0)));
    G16(0x1032) = drift != 0;
}

/* .text+0x63a0, 0x20: the visibility, in miles, to the nearest table step */
static void w_visibility(void)
{
    uint32_t miles = (uint32_t)eval_int(), i;
    uint16_t want, here, before, mid;

    if (G16(0xf90) == 0)
        return;
    if ((uint16_t)miles > 0x32)
        miles = 0x32;
    want = (uint16_t)(miles * 100);
    for (i = 0; A_VISIBILITY + 2 * i < A_COVER - 2; i++) {
        here = D16(A_VISIBILITY + 2 * i);
        if ((int32_t)want < (int32_t)here - 2) {
            if (i == 0) {
                G16(0x1074) = here;
                return;
            }
            before = D16(A_VISIBILITY + 2 * i - 2);
#if BREAK==4
            mid = before;
#else
            mid = (uint16_t)(before + ((int32_t)here - (int32_t)before) / 2);
#endif
            G16(0x1074) = (int32_t)mid > (int32_t)want ? before : here;
            return;
        }
        if ((int32_t)want < (int32_t)here + 2) {
            G16(0x1074) = here;
            return;
        }
    }
    G16(0x1074) = want;
}

/* ---- sounds: .text+0x6470 .. +0x6860 ----------------------------------------------- *
 * Found in the program's own WAV directory, string 0x900d's, 0x900e's,
 * [APL] WAVE='s, or as named; played through the host's voice engine, one
 * voice playing and one queued behind it. */

/* .text+0x6470: the path of sound `name`, or "" */
static uint32_t FS98_STDCALL find_sound(const char *name, char *out)
{
    uint32_t result;

    H_JOIN(U32(out), 0, U32(A_SOUND_DIR0), U32(name));
    result = H_EXISTS(U32(out), 0x13);
    if (result == 0)
        return 0;
    H_JOIN(U32(out), 0, U32(A_SOUND_DIR1), U32(name));
    result = H_EXISTS(U32(out), 0x13);
    if (result == 0)
        return 0;
    H_JOIN(U32(out), 0, U32(A_SOUND_DIR3), U32(name));
    result = H_EXISTS(U32(out), 0x13);
    if (result == 0)
        return 0;
    if (A_SOUND_DIR2[0] != 0) {
        H_JOIN(U32(out), 0, U32(A_SOUND_DIR2), U32(name));
        result = H_EXISTS(U32(out), 0x13);
        if (result == 0)
            return 0;
    }
    H_STRCPY(U32(out), U32(name));
    result = H_EXISTS(U32(out), 0x13);
    if (result != 0)
        *out = 0;
    return result;
}

static uint32_t FS98_STDCALL queue_sound(const char *path);

/* .text+0x6550: play a sound; "" silences both voices */
static uint32_t FS98_STDCALL play_sound(const char *name)
{
    char path[0x104];

    if (A_NO_SOUND != 0)
        return 0;
    if (name != NULL && *name != 0) {
        find_sound(name, path);
        if (path[0] != 0)
            return queue_sound(path);
        return 0;
    }
    if (A_VOICE != 0)
        H_VOICE_FREE(U32(&A_VOICE));
    if (A_VOICE_NEXT != 0)
        H_VOICE_FREE(U32(&A_VOICE_NEXT));
    return 0;
}

/* .text+0x65e0 */
static void sound_check(void)
{
    A_NO_SOUND = H_SOUND_ON() == 0;
}

/* .text+0x6600 */
static void voices_free(void)
{
    if (A_VOICE != 0)
        H_VOICE_FREE(U32(&A_VOICE));
    if (A_VOICE_NEXT != 0)
        H_VOICE_FREE(U32(&A_VOICE_NEXT));
}

/* .text+0x6640 */
static void voices_stop(void)
{
    if (H_SOUND_ON() != 1)
        return;
    if (A_VOICE != 0) {
        H_VOICE_STOP(A_VOICE);
        H_VOICE_FREE(U32(&A_VOICE));
    }
    if (A_VOICE_NEXT != 0) {
        H_VOICE_STOP(A_VOICE_NEXT);
        H_VOICE_FREE(U32(&A_VOICE_NEXT));
    }
    SOUND_EVENT(0x12, 0);
}

/* .text+0x66b0: the program's own WAV directory */
static void sound_dir(void)
{
    char name[0x100];

    H_LOADSTRING(0x900c, U32(name), 0x100);
    H_JOIN(U32(A_SOUND_DIR0), 0, U32(A_DIR), U32(name));
}

/* .text+0x6700: stop the voices if GLOBAL +0xb24 has switched sounds off */
static void voices_idle(void)
{
    if (A_NO_SOUND == 0 && G32(0xb24) == 0 && A_VOICE != 0)
        voices_stop();
}

static void FS98_STDCALL sound_done(uint32_t voice);

/* .text+0x6730: play `path` now, or queue it behind the voice playing */
static uint32_t FS98_STDCALL queue_sound(const char *path)
{
    if (A_VOICE == 0) {
        if (H_VOICE_LOAD(U32(path), 2, U32(&A_VOICE)) != 0)
            return 0;
        H_VOICE_VOL(A_VOICE, SOUND_VOLUME(4));
        H_VOICE_PLAY(A_VOICE, 0, 0.0, U32(sound_done));
        return 1;
    }
    if (A_VOICE_NEXT == 0)
        return H_VOICE_LOAD(U32(path), 2, U32(&A_VOICE_NEXT)) == 0;
    return H_VOICE_ADD(U32(path), 2, A_VOICE_NEXT, 1) == 0;
}

/* .text+0x67f0: a voice finished; the queued one starts */
static void FS98_STDCALL sound_done(uint32_t voice)
{
    H_VOICE_FREE(U32(&voice));
    A_VOICE = A_VOICE_NEXT;
    if (A_VOICE_NEXT != 0) {
        A_VOICE_NEXT = 0;
        H_VOICE_VOL(A_VOICE, SOUND_VOLUME(4));
        H_VOICE_PLAY(A_VOICE, 0, 0.0, U32(sound_done));
    }
}

/* ---- the "loading" alert: .text+0x6860 .. +0x6950 ---------------------------------- */

static void alert_close(void);
static uint32_t FS98_STDCALL alert_tick(uint32_t code, uint32_t arg);

/* .text+0x6860: show the program's title for a few frames: 0xf436 when it
 * loads (four frames), 0xf437 when it stops (two) */
static void FS98_STDCALL alert_show(uint32_t opening)
{
    if (A_TYPE == 0x21 || A_DONE != 0)
        return;
    alert_close();
    A_ALERT_TICKS = opening != 0 ? 4 : 2;
    UTIL_OPEN(U32(&A_ALERT), 2, 0xf437 - (opening != 0), U32(D + A_HEADER + 0x10),
              0, 0, 0, 0);
    CHAIN_ADD(3, U32(alert_tick));
}

/* .text+0x68d0 */
static void alert_close(void)
{
    if (A_ALERT == 0)
        return;
    UTIL_CLOSE(U32(&A_ALERT));
    CHAIN_REMOVE(3, U32(alert_tick));
    A_ALERT = 0;
    A_ALERT_TICKS = 0;
}

/* .text+0x6910, on CHAIN list 3 while the alert shows */
static uint32_t FS98_STDCALL alert_tick(uint32_t code, uint32_t arg)
{
    (void)code;
    (void)arg;
    if (A_ALERT_TICKS != 0) {
        A_ALERT_TICKS = A_ALERT_TICKS - 1;
        return 0;
    }
    UTIL_CLOSE(U32(&A_ALERT));
    CHAIN_REMOVE(3, U32(alert_tick));
    return 0;
}

/* ---- events: .text+0x6950 .. +0x6d70 ----------------------------------------------- *
 * A 16-byte host block each: { kind, the instruction, where its operands
 * follow the expression, next }.  Kind 0 calls a handler while the
 * expression is true (and another while false), 1 on its rising and
 * falling edges, 2 when it changes; the last value is kept in the
 * instruction. */

/* .text+0x6980: a new event at the end of the list, or 0 (and the program
 * stopped) past MAX_EVENTS */
static uint8_t *event_new(void)
{
    uint32_t block = 0;

    if (A_EVENTS == A_EVENTS_MAX) {
        UTIL_ALERT(1, 1, 0xf43a, 0, 0, 0, 0, 0);
        stop();
        return NULL;
    }
    H_ALLOC(1, 0x10, U32(&block), 0);
    P32(PTR(block), 0xc) = 0;
    if (A_EV_HEAD == 0) {
        A_EV_HEAD = block;
        A_EV_TAIL = block;
        A_EV_CURSOR = block;
    } else {
        P32(PTR(A_EV_TAIL), 0xc) = block;
        A_EV_TAIL = block;
    }
    A_EVENTS = A_EVENTS + 1;
    return PTR(block);
}

static void event_add(uint32_t kind, uint32_t at)
{
    uint8_t *e = event_new();

    if (e == NULL)
        return;
    P32(e, 0) = kind;
    P32(e, 4) = at;
    P32(e, 8) = U32(skip_expr(PTR(at)));
}

/* .text+0x6950, +0x6a50, +0x6a90 */
static void FS98_STDCALL event_true(uint32_t at)   { event_add(0, at); }
static void FS98_STDCALL event_edge(uint32_t at)   { event_add(1, at); }
static void FS98_STDCALL event_change(uint32_t at) { event_add(2, at); }

/* .text+0x6a30, +0x6a80, +0x6ac0: each calls a routine that does nothing */
static void FS98_STDCALL event_nothing(uint32_t kind, uint32_t at)
{
    (void)kind;
    (void)at;
}

static void FS98_STDCALL event_none(uint32_t at)  { event_nothing(0, at); }
static void FS98_STDCALL event_none1(uint32_t at) { event_nothing(1, at); }
static void FS98_STDCALL event_none2(uint32_t at) { event_nothing(2, at); }

/* .text+0x6ad0 */
static uint32_t events_free(void)
{
    uint32_t next;

    while (A_EV_HEAD != 0) {
        next = P32(PTR(A_EV_HEAD), 0xc);
        H_FREE(U32(&A_EV_HEAD));
        A_EV_HEAD = next;
    }
    A_EV_TAIL = 0;
    A_EV_CURSOR = 0;
    A_EVENTS = 0;
    return 0;
}

/* .text+0x6b20: evaluate the event at the cursor and step it on; the
 * handler to call, or -1.  0 at the end of the list (the cursor goes back
 * to the start). */
static uint32_t FS98_STDCALL events_next(int32_t *handler)
{
    uint32_t saved, t1, t2, kind;
    int32_t chosen = -1;
    uint8_t *e;
    double v, last;

    if (A_EV_CURSOR == 0) {
        A_EV_CURSOR = A_EV_HEAD;
        return 0;
    }
    saved = A_PC;
    A_PC = P32(PTR(A_EV_CURSOR), 4);
    v = eval();
    e = PTR(A_EV_CURSOR);
    kind = P32(e, 0);
    if (kind == 0) {
        t1 = PC32();
        A_PC += 4;
        chosen = (int32_t)PC32();
        if (!(fcom(v, K(0x206192f8)) & C3))
            chosen = (int32_t)t1;
    } else if (kind == 1) {
        t1 = PC32();
        A_PC += 4;
        t2 = PC32();
        A_PC += 4;
        last = PF64(PTR(A_PC), 0);
        A_PC += 8;
        if (!(fcom(v, K(0x206192f8)) & C3) && (fcom(last, K(0x206192f8)) & C3)) {
            chosen = (int32_t)t1;
        } else if ((fcom(v, K(0x206192f8)) & C3) &&
                   !(fcom(last, K(0x206192f8)) & C3)) {
            chosen = (int32_t)t2;
        }
        PF64(PTR(P32(e, 8)), 8) = v;
        e = PTR(A_EV_CURSOR);
    } else if (kind == 2) {
        t1 = PC32();
        A_PC += 4;
        last = PF64(PTR(A_PC), 0);
        A_PC += 8;
        if (!(fcom(last, v) & C3))
            chosen = (int32_t)t1;
        PF64(PTR(P32(e, 8)), 4) = v;
        e = PTR(A_EV_CURSOR);
    }
    A_PC = saved;
    A_EV_CURSOR = P32(e, 0xc);
    *handler = chosen;
    return 1;
}

/* .text+0x6cc0: unlink `node` after `before` (0: the head), and free it;
 * returns the node now following */
static uint32_t FS98_STDCALL event_unlink(uint32_t before, uint32_t node)
{
    uint32_t next;

    A_EVENTS = A_EVENTS - 1;
    if (before == 0) {
        next = P32(PTR(A_EV_HEAD), 0xc);
        if (next == 0) {
            A_EV_TAIL = 0;
            A_EV_CURSOR = 0;
        } else if (A_EV_CURSOR == A_EV_HEAD) {
            A_EV_CURSOR = next;
        }
        next = P32(PTR(A_EV_HEAD), 0xc);
        H_FREE(U32(&A_EV_HEAD));
        A_EV_HEAD = next;
        return next;
    }
    if (node == A_EV_TAIL) {
        A_EV_TAIL = before;
        A_EV_CURSOR = A_EV_HEAD;
    } else if (node == A_EV_CURSOR) {
        A_EV_CURSOR = P32(PTR(node), 0xc);
    }
    P32(PTR(before), 0xc) = P32(PTR(node), 0xc);
    H_FREE(U32(&node));
    return P32(PTR(before), 0xc);
}

/* .text+0x6c80: drop the events whose handlers include `target` */
static void FS98_STDCALL events_drop(uint32_t target)
{
    uint32_t node = A_EV_HEAD, before = 0;
    uint8_t *after;

    while (node != 0) {
        after = PTR(P32(PTR(node), 8));
        if (P32(after, 0) == target ||
            (P32(PTR(node), 0) != 2 && P32(after, 4) == target)) {
            node = event_unlink(before, node);
            if (node == 0)
                return;
        }
        before = node;
        node = P32(PTR(node), 0xc);
    }
}

/* ---- skipping instructions: .text+0x6d70 .. +0x70d0 -------------------------------- *
 * To find the sounds a program plays without running it.  Each skipper
 * takes the operands' start and returns the next instruction's. */

/* .text+0x6d70: a string */
static uint8_t *FS98_STDCALL skip_str(uint8_t *p)
{
    uint8_t c = *p++;

    if (c != 0) {
        do {
            c = *p++;
        } while (c != 0);
    }
    return p;
}

static uint8_t *FS98_STDCALL skip_none(uint8_t *p) { return p; }   /* +0x6d90 */
static uint8_t *FS98_STDCALL skip_1(uint8_t *p)    { return p + 1; }  /* +0x6da0 */
static uint8_t *FS98_STDCALL skip_4(uint8_t *p)    { return p + 4; }  /* +0x6db0 */

/* .text+0x6dc0: an expression -- or a variable reference, which reads the
 * same way */
static uint8_t *FS98_STDCALL skip_expr(uint8_t *p)
{
    int done = 1;
    uint32_t op;

    if (*p == 1) {
        done = 0;
        p++;
    }
    do {
        op = *p++;
        switch (op) {
        case 0: done = 1; break;
        case 2: p += 1; break;
        case 3: case 8: p += 2; break;
        case 4: case 7: p += 4; break;
        case 5: p += 8; break;
        default: break;
        }
    } while (!done);
    return p;
}

static uint8_t *FS98_STDCALL skip_4_expr(uint8_t *p)    /* +0x6e30 */
{
    return skip_expr(p + 4);
}

static uint8_t *FS98_STDCALL skip_4x(uint8_t *p)        /* +0x6e40 */
{
    return skip_expr(skip_expr(skip_expr(skip_expr(p))));
}

static uint8_t *FS98_STDCALL skip_20_expr(uint8_t *p)   /* +0x6e60 */
{
    return skip_expr(p + 0x14);
}

/* .text+0x6e70: text.  A string-table piece (4, '2', index) is read as an
 * expression of one operator, so only its '2' is skipped. */
static uint8_t *FS98_STDCALL skip_text(uint8_t *p)
{
    int done = 0;
    uint8_t op;

    do {
        op = *p++;
        switch (op) {
        case 0: done = 1; break;
        case 2: p = skip_str(p); break;
        case 3: case 4: p = skip_expr(p); break;
        default: break;
        }
    } while (!done);
    return p;
}

static uint8_t *FS98_STDCALL skip_table(uint8_t *p)     /* +0x6ed0 */
{
    uint8_t n = *p++;

    return skip_expr(p) + 4 * n;
}

static uint8_t *FS98_STDCALL skip_2x(uint8_t *p)        /* +0x6f00 */
{
    return skip_expr(skip_expr(p));
}

static uint8_t *FS98_STDCALL skip_expr_4(uint8_t *p)    /* +0x6f20 */
{
    return skip_expr(p) + 4;
}

static uint8_t *FS98_STDCALL skip_expr_1(uint8_t *p)    /* +0x6f30 */
{
    return skip_expr(p) + 1;
}

static uint8_t *FS98_STDCALL skip_area(uint8_t *p)      /* +0x6f40 */
{
    return skip_expr(skip_expr(skip_expr(skip_expr(skip_str(p)))));
}

static uint8_t *FS98_STDCALL skip_cloud(uint8_t *p)     /* +0x6f70 */
{
    uint8_t has;

    p = skip_expr(p);
    has = *p++;
    if (has == 0)
        return p;
    p = skip_expr(skip_expr(p));
    p = skip_expr(skip_expr(skip_expr(skip_expr(p + 1))));
    return p;
}

static uint8_t *FS98_STDCALL skip_wind(uint8_t *p)      /* +0x6fb0 */
{
    uint8_t has;

    p = skip_expr(p);
    has = *p++;
    if (has == 0)
        return p;
    p = skip_expr(skip_expr(p));
    return skip_expr(skip_expr(skip_expr(p + 1)));
}

static uint8_t *FS98_STDCALL skip_temperature(uint8_t *p)   /* +0x6ff0 */
{
    uint8_t has;

    p = skip_expr(p);
    has = *p++;
    if (has == 0)
        return p;
    return skip_expr(skip_expr(skip_expr(p)));
}

static uint8_t *FS98_STDCALL skip_3x(uint8_t *p)        /* +0x7020 */
{
    return skip_expr(skip_expr(skip_expr(p)));
}

static uint8_t *FS98_STDCALL skip_keys(uint8_t *p)      /* +0x7040 */
{
    uint8_t n;
    uint32_t count;

    if (*p++ == 0)
        return p;
    n = *p++;
    if (n != 0) {
        count = (uint32_t)(uint8_t)(n - 1) + 1;
        do {
            p = skip_expr(p);
        } while (--count != 0);
    }
    return p;
}

static uint8_t *FS98_STDCALL skip_rate(uint8_t *p)      /* +0x7080 */
{
    if (*p++ == 100)
        return p;
    return skip_expr(p);
}

static uint8_t *FS98_STDCALL skip_expr_8(uint8_t *p)    /* +0x70a0 */
{
    return skip_expr(p) + 8;
}

static uint8_t *FS98_STDCALL skip_expr_16(uint8_t *p)   /* +0x70b0 */
{
    return skip_expr(p) + 0x10;
}

static uint8_t *FS98_STDCALL skip_expr_12(uint8_t *p)   /* +0x70c0 */
{
    return skip_expr(p) + 0xc;
}

/* .text+0x70d0, the 0x0e skipper: a sound found nowhere is copied from the
 * CD's fs98\adv\wav into string 0x900e's directory (caching), and a copy
 * there is deleted (not) */
static uint8_t *FS98_STDCALL skip_sound(uint8_t *name)
{
    char copy[0x100], source[0x100];

    if (A_CACHE != 0) {
        find_sound((const char *)name, copy);
        if (copy[0] == 0) {
            H_JOIN(U32(copy), 0, U32(A_SOUND_DIR3), U32(name));
            H_JOIN(U32(source), 0, U32(A_CACHE_DIR), U32(name));
            H_COPY(U32(copy), 0x13, U32(source), 0x13);
#if BREAK!=10
            H_SET_ATTR(1, U32(copy), 0x13);
#endif
        }
        return skip_str(name);
    }
    H_JOIN(U32(source), 0, U32(A_SOUND_DIR3), U32(name));
    if (H_EXISTS(U32(source), 0x13) == 0)
        H_DELETE(U32(source), 0x13);
    return skip_str(name);
}

/* .text+0x71e0: walk the program to its scan size */
static void scan_sounds(void)
{
    uint8_t *p = PTR(A_CODE);
    uint8_t op;

    while ((uint32_t)(p - PTR(A_CODE)) < A_SCAN_SIZE) {
        op = *p++;
        p = ((uint8_t *(FS98_STDCALL *)(uint8_t *))(uintptr_t)
             D32(A_SKIP + 4 * op))(p);
    }
}

/* .text+0x7230: [APL] CACHE_WAV_FILES (default 1) copies a program's
 * sounds from the CD into the sim's ADV\WAV when it starts, and deletes
 * them when it stops */
static void FS98_STDCALL cache_sounds(uint32_t caching)
{
    uint32_t enabled = 1;

    H_CFG_INT(U32(A_APL), U32(A_CACHE_KEY), U32(&enabled));
    if (A_CODE != 0 && enabled != 0) {
        A_CACHE = caching;
        scan_sounds();
    }
}

/* ---- the image ------------------------------------------------------------------------ */

struct advdrv_image advdrv_image = {{
#define ADVDRV_IMAGE
#include "advdrv_data.inc"
#undef ADVDRV_IMAGE
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _advdrv_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _advdrv_image + 0x48");

/* Every routine retail's .data points at, by its retail address. */
static const struct {
    uint32_t retail;
    void *ours;
} advdrv_code[] = {
    {0x20611020, (void *)adv_init},       {0x20611100, (void *)adv_deinit},
    {0x20611160, (void *)adv_running},    {0x20611190, (void *)stop},
    {0x206111f0, (void *)restart},        {0x20611290, (void *)adv_load},
    {0x20611880, (void *)adv_set_skip},
    {0x20611b40, (void *)op_keys},        {0x20611bb0, (void *)op_stop},
    {0x20611bc0, (void *)op_error},       {0x20611bf0, (void *)op_unless},
    {0x20611c20, (void *)op_sound},       {0x20611c70, (void *)op_message5},
    {0x20611cc0, (void *)op_message6},    {0x20611d10, (void *)op_print},
    {0x20611d40, (void *)op_format},      {0x20611d90, (void *)op_message_time},
    {0x20611db0, (void *)op_console},     {0x20611e50, (void *)op_wait},
    {0x20611f60, (void *)op_goto},        {0x20611f80, (void *)op_switch},
    {0x20611fd0, (void *)op_call},        {0x20611ff0, (void *)op_switch_call},
    {0x20612150, (void *)op_on_832},      {0x20612170, (void *)op_on_key},
    {0x20612190, (void *)op_return},      {0x206121e0, (void *)op_for},
    {0x20612240, (void *)op_next},        {0x206122f0, (void *)op_assign},
    {0x20612320, (void *)op_north},       {0x20612330, (void *)op_south},
    {0x20612340, (void *)op_east},        {0x20612350, (void *)op_west},
    {0x20612360, (void *)op_weather_thunk}, {0x20612370, (void *)op_end_flight},
    {0x20612390, (void *)op_position},    {0x20612410, (void *)op_video},
    {0x20612590, (void *)op_situation},   {0x206127b0, (void *)op_restart},
    {0x206127c0, (void *)op_bind},        {0x20612870, (void *)op_rate},
    {0x20612980, (void *)op_when},        {0x206129c0, (void *)op_edge},
    {0x20612a00, (void *)op_change},      {0x20612a40, (void *)op_forget},
    {0x20612e00, (void *)x_int8},         {0x20612e30, (void *)x_int16},
    {0x20612e70, (void *)x_int32},        {0x20612ea0, (void *)x_stop},
    {0x20612eb0, (void *)x_double},       {0x20612ee0, (void *)x_variable},
    {0x20612f20, (void *)x_system},       {0x20612f50, (void *)x_add},
    {0x20612f80, (void *)x_sub},          {0x20612fb0, (void *)x_mul},
    {0x20612fe0, (void *)x_div},          {0x20613010, (void *)x_idiv},
    {0x20613050, (void *)x_mod},          {0x20613090, (void *)x_and},
    {0x206130c0, (void *)x_or},           {0x206130f0, (void *)x_xor},
    {0x20613120, (void *)x_eq},           {0x20613160, (void *)x_ne},
    {0x206131a0, (void *)x_lt},           {0x206131e0, (void *)x_le},
    {0x20613220, (void *)x_gt},           {0x20613260, (void *)x_ge},
    {0x206132a0, (void *)x_land},         {0x206132f0, (void *)x_lor},
    {0x20613330, (void *)x_not},          {0x20613360, (void *)x_neg},
    {0x20613380, (void *)x_trunc},        {0x206133a0, (void *)x_frac},
    {0x206133d0, (void *)x_between},      {0x20613440, (void *)x_ft_m},
    {0x20613460, (void *)x_m_ft},         {0x20613480, (void *)x_f_c},
    {0x206134a0, (void *)x_c_f},          {0x206134c0, (void *)x_mb_in},
    {0x206134e0, (void *)x_in_mb},        {0x20613500, (void *)x_km_nm},
    {0x20613520, (void *)x_nm_km},        {0x20613540, (void *)x_nm_mi},
    {0x20613560, (void *)x_mi_nm},        {0x20613580, (void *)x_range},
    {0x20613610, (void *)x_range_ground}, {0x206136a0, (void *)x_range_between},
    {0x20613780, (void *)x_bearing},      {0x20613830, (void *)x_elevation},
    {0x206138f0, (void *)x_minutes},      {0x20613930, (void *)x_seconds},
    {0x20613970, (void *)x_heading_in},
    {0x20613b40, (void *)g_u8},           {0x20613b60, (void *)g_u16},
    {0x20613b80, (void *)g_u32},          {0x20613bb0, (void *)g_bool8},
    {0x20613bd0, (void *)g_bool16},       {0x20613bf0, (void *)g_not16},
    {0x20613c10, (void *)g_bool32},       {0x20613c30, (void *)g_frac16},
    {0x20613c60, (void *)g_s8_over},      {0x20613c90, (void *)g_s16_over},
    {0x20613cc0, (void *)g_u32_over},     {0x20613cf0, (void *)g_mask8},
    {0x20613d20, (void *)g_mask16},       {0x20613d50, (void *)g_lat},
    {0x20613d70, (void *)g_lon},          {0x20613d90, (void *)g_feet16},
    {0x20613dd0, (void *)g_u32_scaled},   {0x20613e00, (void *)g_feet48},
    {0x20613e30, (void *)g_u32_scaled2},  {0x20613e60, (void *)g_angle32},
    {0x20613e80, (void *)g_angle32_neg},  {0x20613ea0, (void *)g_angle32_rel},
    {0x20613ee0, (void *)g_angle16_rel},  {0x20613f10, (void *)g_angle16s},
    {0x20613f40, (void *)g_angle16},      {0x20613f70, (void *)g_through_11d4},
    {0x20613fa0, (void *)g_frequency},    {0x20614050, (void *)g_adf},
    {0x20614110, (void *)g_u16_scaled},   {0x20614140, (void *)g_256ths},
    {0x20614170, (void *)g_product},      {0x206141b0, (void *)g_nav_lat},
    {0x206141f0, (void *)g_nav_lon},      {0x20614230, (void *)g_nav_feet},
    {0x20614270, (void *)g_nav_radial},   {0x206142e0, (void *)g_nav_offset},
    {0x20614330, (void *)g_nav_glide},    {0x20614390, (void *)g_course},
    {0x206143c0, (void *)g_nav_flag},     {0x206143f0, (void *)g_dme},
    {0x20614460, (void *)g_ndb_on},       {0x20614490, (void *)g_ndb_lat},
    {0x206144c0, (void *)g_ndb_lon},      {0x206144f0, (void *)g_ndb_feet},
    {0x20614530, (void *)g_ground},       {0x20614580, (void *)g_own},
    {0x20614590, (void *)s_own},          {0x206145c0, (void *)g_random},
    {0x206145d0, (void *)g_seconds},      {0x20614600, (void *)g_s32_scaled},
    {0x20614620, (void *)g_is1},          {0x20614650, (void *)g_squawk},
    {0x206146e0, (void *)g_track},        {0x20614850, (void *)g_control},
    {0x206148a0, (void *)s_u8},           {0x206148d0, (void *)s_u16},
    {0x20614910, (void *)s_u32},          {0x20614940, (void *)s_flag8},
    {0x20614980, (void *)s_flag16},       {0x206149c0, (void *)s_flag32},
    {0x20614a00, (void *)s_angle16},      {0x20614a80, (void *)s_100a2},
    {0x20614ad0, (void *)s_100a4},        {0x20614b20, (void *)s_com},
    {0x20614c30, (void *)s_nav},          {0x20614d40, (void *)s_adf},
    {0x20614e30, (void *)s_obs},          {0x20614e90, (void *)s_switch},
    {0x20614ef0, (void *)s_squawk},       {0x20614f50, (void *)s_command},
    {0x20614f90, (void *)s_visibility},   {0x20614fe0, (void *)s_hours},
    {0x20615030, (void *)s_minutes},      {0x20615080, (void *)s_seconds},
    {0x206150b0, (void *)s_control},      {0x20615140, (void *)s_trim},
    {0x206156c0, (void *)adv_adventures}, {0x206156e0, (void *)adv_lessons},
    {0x206159a0, (void *)w_area},         {0x20615e40, (void *)w_surface},
    {0x20615fc0, (void *)w_cloud},        {0x20616670, (void *)w_wind},
    {0x20616dc0, (void *)w_temperature},  {0x20617300, (void *)w_pressure},
    {0x206173a0, (void *)w_visibility},
    {0x20617d70, (void *)skip_str},       {0x20617d90, (void *)skip_none},
    {0x20617da0, (void *)skip_1},         {0x20617db0, (void *)skip_4},
    {0x20617dc0, (void *)skip_expr},      {0x20617e30, (void *)skip_4_expr},
    {0x20617e40, (void *)skip_4x},        {0x20617e60, (void *)skip_20_expr},
    {0x20617e70, (void *)skip_text},      {0x20617ed0, (void *)skip_table},
    {0x20617f00, (void *)skip_2x},        {0x20617f20, (void *)skip_expr_4},
    {0x20617f30, (void *)skip_expr_1},    {0x20617f40, (void *)skip_area},
    {0x20617f70, (void *)skip_cloud},     {0x20617fb0, (void *)skip_wind},
    {0x20617ff0, (void *)skip_temperature}, {0x20618020, (void *)skip_3x},
    {0x20618040, (void *)skip_keys},      {0x20618080, (void *)skip_rate},
    {0x206180a0, (void *)skip_expr_8},    {0x206180b0, (void *)skip_expr_16},
    {0x206180c0, (void *)skip_expr_12},   {0x206180d0, (void *)skip_sound},
};

static uint32_t code_for(uint32_t retail)
{
    uint32_t i;

    for (i = 0; i < sizeof advdrv_code / sizeof advdrv_code[0]; i++) {
        if (advdrv_code[i].retail == retail)
            return U32(advdrv_code[i].ours);
    }
    return 0;
}

static void advdrv_install(void)
{
#define ADVDRV_CODE(offset, retail) D32(offset) = code_for(retail);
#define ADVDRV_DATA(offset, target) D32(offset) = U32(D + (target));
#define ADVDRV_RELOCS
#include "advdrv_data.inc"
#undef ADVDRV_RELOCS
#undef ADVDRV_CODE
#undef ADVDRV_DATA
    bind_crt();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        advdrv_install();
    }
    return TRUE;
}
