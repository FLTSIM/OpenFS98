/* SOUND.DLL -- module id 39: the sim's sounds.
 *
 * Every sound is a 0x44-byte record in a host growable array, built from 29
 * templates when the aircraft loads and configured from the aircraft's
 * sound\SOUND.CFG: the engine (a chain of linked sub-sounds, each with its
 * own rate and volume curves), gear, flaps, the marker beacons, stall, crash,
 * splash, wind, touchdowns, warnings, and a Morse generator that builds a
 * navaid's ident out of dit, dah and gap samples.  The host's voice engine
 * (+0x2cc..+0x2f4) plays them; SOUND decides what plays, how loud and how
 * fast.  MAIN drives it -- +0x18/+0x1c start and stop it, +0x20 runs every
 * frame, +0x24 takes events -- and PANELS reads the marker lights from +0x44.
 *
 * Retail is MSVC-compiled C, transliterated here function by function; the
 * comments carry retail's addresses and docs/modules/sound.md has the walk
 * through.  Before calling into it, know that:
 *
 *   - Everything lives in one .data image laid out exactly as retail's,
 *     ImportTable at +0x00 and Linkage at +0x30.  The code addresses it by
 *     retail's offsets (0x208d7278 is D32(0x1278)), and the image holds
 *     pointers into itself and to five functions, which the generator emits
 *     as such.
 *   - The floating point is retail's, operation for operation, on the x87:
 *     long double where retail keeps a value on the FPU stack, double where it
 *     stores one.  The three transcendental steps (fyl2x twice, fpatan) and
 *     fsqrt are inline x87 so they round as retail's do.
 *   - Retail's bugs are kept: +0x24 code 0x1b passes a voice handle where a
 *     record belongs; the marker-light toggle (CHAIN list 0x12) indexes its
 *     table with an unchecked signed value; ids 0x21 and 0x22 have no
 *     template and land on the shared default record.  See the notes there.
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define SND_DATA_VA    0x208d6000u  /* retail .data */
#define SND_IMAGE_SIZE 0x177c    /* .data's virtual size */

typedef uint8_t  __attribute__((may_alias)) snd_u8;
typedef uint16_t __attribute__((may_alias)) snd_u16;
typedef int16_t  __attribute__((may_alias)) snd_s16;
typedef uint32_t __attribute__((may_alias)) snd_u32;
typedef int32_t  __attribute__((may_alias)) snd_s32;
typedef double   __attribute__((may_alias)) snd_f64;

/* A host growable array header, as +0x2f8..+0x310 use it. */
typedef struct fs98_host_list snd_list;

/* One sound.  0x44 bytes, in the array at 0x208d7760 or in an engine's
 * own array of linked sub-sounds. */
struct snd_rec {
    int32_t   id;               /* +0x00: what snd_find looks up */
    int32_t   chain;            /* +0x04: the id it was linked under */
    int32_t   group;            /* +0x08: which fader, 1..5 */
    snd_list *links;            /* +0x0c: an engine's sub-sounds */
    uint8_t  *params;           /* +0x10: a block whose +0x4 frees it */
    char     *filename;         /* +0x14 */
    int32_t   type;             /* +0x18: the host's load type */
    uint32_t  voice;            /* +0x1c: the host's handle */
    int32_t   playing;          /* +0x20 */
    int32_t   wanted;           /* +0x24: a marker held while sound is off */
    int32_t   volume;           /* +0x28: initial_volume */
    int32_t   minvol;           /* +0x2c: minimum_volume */
    int32_t   maxvol;           /* +0x30: maximum_volume */
    int32_t   fader;            /* +0x34: the group's fader; 0 mutes */
    int32_t   pan;              /* +0x38: panning */
    int32_t   viewpoint;        /* +0x3c */
    int32_t   flags;            /* +0x40: bit 0 disabled, 2/4/8 jet roles */
};

/* An engine sub-sound's frame context, built on the stack by 0x4450. */
struct snd_prop_ctx {
    int32_t flag;               /* +0x00: too slow, or too far */
    double  val;                /* +0x04: rpm x manifold, scaled */
    double  db;                 /* +0x0c: distance attenuation */
    int32_t kind;               /* +0x14: the aircraft's view kind */
} __attribute__((packed));

/* The same for jets, built by 0x4750. */
struct snd_jet_ctx {
    int32_t flag;               /* +0x00 */
    int32_t vol;                /* +0x04 */
    int32_t vol2;               /* +0x08: the whine */
    int32_t kind;               /* +0x0c */
    int32_t whine;              /* +0x10 */
    double  rate;               /* +0x14 */
    double  val;                /* +0x1c */
} __attribute__((packed));

typedef int32_t (FS98_STDCALL *snd_walk_fn)(void *elem, void *ctx);
typedef int32_t (FS98_STDCALL *snd_tmpl_fn)(struct snd_rec *rec, const char *section);
typedef int32_t (FS98_STDCALL *snd_free_fn)(void *pp);

struct snd_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;
    void    *dialogs;                                                   /* +0x14: none */
    int32_t (*start)(void);                                             /* +0x18 */
    int32_t (*stop)(void);                                              /* +0x1c */
    int32_t (*frame)(void);                                             /* +0x20 */
    int32_t (FS98_STDCALL *event)(int32_t code, uint32_t arg);          /* +0x24 */
    void    (FS98_STDCALL *wind)(uint32_t a, uint32_t b, uint32_t c);   /* +0x28 */
    void    (FS98_STDCALL *set_fader)(int32_t group, int32_t value);    /* +0x2c */
    int32_t (FS98_STDCALL *get_fader)(int32_t group);                   /* +0x30 */
    int32_t (FS98_STDCALL *set_faders)(int32_t *f);                     /* +0x34 */
    int32_t (FS98_STDCALL *get_faders)(int32_t *f);                     /* +0x38 */
    int32_t (FS98_STDCALL *save_faders)(const double *v);               /* +0x3c */
    int32_t (FS98_STDCALL *load_faders)(double *v);                     /* +0x40 */
    uint32_t (*markers)(void);                                          /* +0x44 */
};

struct snd_image {
    struct fs98_import import[6];       /* +0x000 ImportTable */
    struct snd_linkage linkage;         /* +0x030 Linkage */
    void *data[(SND_IMAGE_SIZE - 0x78) / 4]; /* +0x078 on */
};

extern struct snd_image snd_image;

typedef char (*snd_layout_check)[sizeof(struct snd_image) == SND_IMAGE_SIZE &&
                                 sizeof(struct snd_linkage) == 0x48 &&
                                 sizeof(struct snd_rec) == 0x44 &&
                                 sizeof(struct snd_prop_ctx) == 0x18 &&
                                 sizeof(struct snd_jet_ctx) == 0x24 ? 1 : -1];

/* ---- the image and its imports ------------------------------------------------- */

#define D           ((uint8_t *)&snd_image)
#define A(va)       ((va) - SND_DATA_VA)            /* a retail address, as an offset */
#define D8(o)       (*(snd_u8  *)(D + (o)))
#define D16(o)      (*(snd_s16 *)(D + (o)))
#define D32(o)      (*(snd_s32 *)(D + (o)))
#define DU32(o)     (*(snd_u32 *)(D + (o)))
#define DF64(o)     (*(snd_f64 *)(D + (o)))
#define DSTR(o)     ((const char *)(D + (o)))

#define HOST        ((void *)snd_image.import[0].link)
#define G           ((uint8_t *)snd_image.import[1].link)
#define G8(o)       (*(snd_u8  *)(G + (o)))
#define G16(o)      (*(snd_u16 *)(G + (o)))
#define GS16(o)     (*(snd_s16 *)(G + (o)))
#define G32(o)      (*(snd_s32 *)(G + (o)))
#define GP(o)     (*(uint8_t **)(G + (o)))

#define P32(p, o)   (*(snd_s32 *)((uint8_t *)(p) + (o)))
#define PF64(p, o)  (*(snd_f64 *)((uint8_t *)(p) + (o)))

#define SLOT(i, off, type) (*(type *)((uint8_t *)snd_image.import[i].link + (off)))

/* The state this file names.  Retail addresses, so a listing greps straight
 * to them. */
#define S_WIND_ARMED   D32(A(0x208d6c14))   /* the wind may retune this frame */
#define S_DEFAULT_NEW  D32(A(0x208d6c18))   /* the default record wants setting up */
#define S_LAST_ON      D32(A(0x208d6c68))   /* GLOBAL +0xb24 last frame */
#define S_MORSE_VOL    D32(A(0x208d7128))
#define S_DEFAULT      ((struct snd_rec *)(D + A(0x208d7230)))
#define S_FLAPS_FRAME  D32(A(0x208d7274))
#define S_OFF          D32(A(0x208d7278))   /* the voice engine said no */
#define S_MARKER_BITS  D32(A(0x208d727c))
#define S_WIND_LAST    DF64(A(0x208d7280))
#define S_FLAPS_STATE  D32(A(0x208d7288))
#define S_FLAPS_A      D32(A(0x208d728c))   /* GLOBAL +0xbe0, last frame */
#define S_FLAPS_B      D32(A(0x208d7290))   /* GLOBAL +0xbe4 */
#define S_CRASH_LAST   D32(A(0x208d7294))
#define S_SPLASH_LAST  D32(A(0x208d7298))
#define S_PAUSE_LAST   D32(A(0x208d72a0))
#define S_KIND_LAST    D32(A(0x208d72a4))
#define S_PATH         ((char *)D + A(0x208d72a8))  /* SOUND.CFG, less .CFG */
#define S_DIR          ((char *)D + A(0x208d73a8))
#define S_SOUNDDIR     ((char *)D + A(0x208d74b0))
#define S_KIND         D32(A(0x208d75b4))
#define S_CRASH_CB     (*(void (**)(void))(D + A(0x208d75b8)))
#define S_CRASHING     D32(A(0x208d75bc))
#define S_SPLASHING    D32(A(0x208d75c0))
#define S_MORSE_OUT    (*(uint32_t *)(D + A(0x208d75c8)))
#define S_MORSE_DIT    (*(uint32_t *)(D + A(0x208d75cc)))
#define S_MORSE_DAH    (*(uint32_t *)(D + A(0x208d75d0)))
#define S_MORSE_GAP    (*(uint32_t *)(D + A(0x208d75d4)))
#define S_WHINE_SCALE  DF64(A(0x208d75d8))
#define S_WHINE        DF64(A(0x208d75e0))
#define S_LISTENER     (D + A(0x208d7600))
#define S_DIST         DU32(A(0x208d7608))  /* the listener's range, 16.16 */
#define S_RATE_AVG     D16(A(0x208d760c))
#define S_EYE          ((double *)(D + A(0x208d7640)))
#define S_POS          ((double *)(D + A(0x208d7660)))
#define S_MARKER       (D + A(0x208d7680))  /* five 0x1c-byte idents */
#define S_FADER_ON(i)  D32(A(0x208d7720) + 4 * (i))
#define S_FADER(i)     D32(A(0x208d7738) + 4 * (i))
#define S_SOUNDS       ((snd_list *)(D + A(0x208d7760)))

/* A marker ident slot, 0x1c bytes from 0x208d7680. */
#define M_ON(i)        D32(A(0x208d7680) + 0x1c * (i))
#define M_VOICE(i)     (*(uint32_t *)(D + A(0x208d7684) + 0x1c * (i)))
#define M_COUNT(i)     D32(A(0x208d7688) + 0x1c * (i))
#define M_RELOAD(i)    D32(A(0x208d768c) + 0x1c * (i))
#define M_INDEX(i)     D32(A(0x208d7690) + 0x1c * (i))
#define M_WORD(i)      D16(A(0x208d7694) + 0x1c * (i))
#define M_STR(i)       (*(char **)(D + A(0x208d7698) + 0x1c * (i)))

/* .rdata's doubles, as K_<offset>, and 2^48 from .data as K_D6FE0. */
#include "sound_consts.inc"

/* fcom/fcomp and the status bits retail tests after them.  C0 is "below or
 * unordered", C3 "equal or unordered". */
#define FC0(a, b)   (!((a) >= (b)))
#define FC3(a, b)   (!((a) < (b)) && !((a) > (b)))
#define FC03(a, b)  (!((a) > (b)))

/* Every prototype read from the callee's `ret N`. */
typedef int32_t  (FS98_STDCALL *chain_reg_fn)(uint32_t list, void *fn);           /* ret 8 */
typedef uint32_t (FS98_STDCALL *chain_dispatch_fn)(uint32_t list, uint32_t a,
                                                   uint32_t b);                   /* ret 0xc */
typedef int32_t  (FS98_STDCALL *acontain_query_fn)(void *aircraft, int32_t key,
                                                   char *buf, uint32_t len);       /* ret 0x10 */
typedef void     (FS98_STDCALL *util_ll2m_fn)(const void *in48, const void *pos48,
                                              void *out48);                        /* ret 0xc */
typedef uint32_t (FS98_STDCALL *util_rand32_fn)(void);                             /* ret */
typedef int32_t  (FS98_STDCALL *util_dtoi_fn)(double v);                           /* ret 8 */
typedef double   (FS98_STDCALL *util_frand_fn)(void);                              /* ret */
typedef double   (*sim_value_fn)(void);                                            /* ret */

#define CHAIN_REGISTER   SLOT(3, 0x18, chain_reg_fn)
#define CHAIN_REMOVE     SLOT(3, 0x1c, chain_reg_fn)
#define CHAIN_DISPATCH   SLOT(3, 0x20, chain_dispatch_fn)
#define ACONTAIN_QUERY   SLOT(2, 0x30, acontain_query_fn)
#define UTIL_LL2M        SLOT(4, 0x48, util_ll2m_fn)
#define UTIL_RAND32      SLOT(4, 0x70, util_rand32_fn)
#define UTIL_DTOI        SLOT(4, 0xb4, util_dtoi_fn)
#define UTIL_FRAND       SLOT(4, 0xb8, util_frand_fn)
/* the flight model's own Linkage, whose +0x38 returns the airspeed the wind
 * sound follows */
#define SIM_VALUE        (*(sim_value_fn *)(GP(0x11d4) + 0x38))

/* The host's services, from FS98_HOST_* where host.h has them. */
#define H_BUSY           FS98_HOST_BUSY(HOST)
#define H_CFG_DOUBLE     FS98_HOST_CFG_DOUBLE(HOST)
#define H_CFG_PUT_DOUBLE FS98_HOST_CFG_PUT_DOUBLE(HOST)
#define H_PROF_INT       FS98_HOST_PROFILE_INT(HOST)
#define H_PROF_DOUBLE    FS98_HOST_PROFILE_DOUBLE(HOST)
#define H_PROF_DOUBLES   FS98_HOST_PROFILE_DOUBLES(HOST)
#define H_PROF_STRING    FS98_HOST_PROFILE_STRING(HOST)
#define H_EXISTS         FS98_HOST_EXISTS(HOST)
#define H_JOIN           FS98_HOST_PATH_JOIN(HOST)
#define H_SPLIT          FS98_HOST_PATH_SPLIT(HOST)
#define H_ALLOC          FS98_HOST_ALLOC(HOST)
#define H_FREE           FS98_HOST_FREE(HOST)
#define H_MEMSET         FS98_HOST_MEMSET(HOST)
#define H_MEMMOVE        FS98_HOST_MEMMOVE(HOST)
#define H_LOADSTRING     FS98_HOST_LOADSTRING(HOST)
#define H_STRCPY         FS98_HOST_STRCPY(HOST)
#define H_STRDUP         FS98_HOST_STRDUP(HOST)
#define H_STRCMP         FS98_HOST_STRCMP(HOST)
#define H_SOUND_OFF      FS98_HOST_SOUND_OFF(HOST)
#define H_SOUND_ENABLE   FS98_HOST_SOUND_ENABLE(HOST)
#define H_VOICE_LOAD     FS98_HOST_VOICE_LOAD(HOST)
#define H_VOICE_APPEND   FS98_HOST_VOICE_APPEND(HOST)
#define H_VOICE_FREE     FS98_HOST_VOICE_FREE(HOST)
#define H_VOICE_PLAY     FS98_HOST_VOICE_PLAY(HOST)
#define H_VOICE_FREQ     FS98_HOST_VOICE_FREQ(HOST)
#define H_VOICE_VOLUME   FS98_HOST_VOICE_VOLUME(HOST)
#define H_VOICE_PAN      FS98_HOST_VOICE_PAN(HOST)
#define H_VOICE_STOP     FS98_HOST_VOICE_STOP(HOST)
#define H_LIST_ADD       FS98_HOST_LIST_ADD(HOST)
#define H_LIST_FREE      FS98_HOST_LIST_FREE(HOST)
#define H_LIST_REMOVE    FS98_HOST_LIST_REMOVE(HOST)
#define H_LIST_WALK      FS98_HOST_LIST_WALK(HOST)
#define H_LIST_FIND      FS98_HOST_LIST_FIND(HOST)

/* ---- the x87 ---------------------------------------------------------------------- */

/* MSVCRT's _ftol: truncation towards zero, 64 bits, the x87's indefinite
 * when out of range.  Callers keep 32 or 16 bits of it, as retail does. */
static int64_t snd_ftol(long double v)
{
    if (!(v >= -9223372036854775808.0L && v < 9223372036854775808.0L))
        return (int64_t)0x8000000000000000LL;
    return (int64_t)v;
}

/* fldlg2 / fyl2x: log10 as retail takes it, lg2 * log2(x) in one step. */
static long double snd_log10(long double x)
{
    long double r;
    __asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
    return r;
}

/* fld1 / fldl2t / fdivp / fyl2x: the other log10, at 0x208d2dc0, which
 * rounds 1/log2(10) to the precision control first. */
static long double snd_log10_l2t(long double x)
{
    long double r;
    /* de f9 is FDIVP st(1),st0: st1 / st0, here 1 / log2(10).  Spelled as
     * bytes because gas reads an operand-less fdivp/fdivrp the other way. */
    __asm__ ("fld1\n\tfldl2t\n\t.byte 0xde, 0xf9\n\tfxch %%st(1)\n\tfyl2x"
             : "=t" (r) : "0" (x));
    return r;
}

/* fpatan: atan2(y, x) with st1 = y, st0 = x. */
static long double snd_atan2(long double y, long double x)
{
    long double r;
    __asm__ ("fpatan" : "=t" (r) : "0" (x), "u" (y) : "st(1)");
    return r;
}

static long double snd_sqrt(long double x)
{
    long double r;
    __asm__ ("fsqrt" : "=t" (r) : "0" (x));
    return r;
}

/* ---- forward ---------------------------------------------------------------------- */

static int32_t FS98_STDCALL snd_event(int32_t code, uint32_t arg);
static int32_t FS98_STDCALL snd_free_block(void *pp);
static int32_t FS98_STDCALL snd_tmpl_basic(struct snd_rec *rec, const char *section);
static int32_t FS98_STDCALL snd_tmpl_extra(struct snd_rec *rec, const char *section);
static int32_t FS98_STDCALL snd_tmpl_rate(struct snd_rec *rec, const char *section);
static int32_t FS98_STDCALL snd_tmpl_engine(struct snd_rec *rec, const char *section);
static void snd_engines_stop(void);
static int32_t FS98_STDCALL snd_markers_off(uint32_t bits);
static void FS98_STDCALL snd_flaps(int32_t kind, int32_t force);
static void snd_morse_free(void);
static int32_t snd_stall_stop(void);

/* ---- records ---------------------------------------------------------------------- */

static void FS98_STDCALL snd_rec_init(struct snd_rec *rec);

/* .text+0x40 -- the record with this id, or the shared default */
static struct snd_rec *FS98_STDCALL snd_find(int32_t id)
{
    struct snd_rec *r = H_LIST_FIND(S_SOUNDS, 0, &id, 4, 0);
    if (r == NULL) {
        r = S_DEFAULT;
        if (S_DEFAULT_NEW == 1) {
            snd_rec_init(r);
            S_DEFAULT_NEW = 0;
        }
    }
    return r;
}

/* .text+0x90 */
static void FS98_STDCALL snd_stop(struct snd_rec *rec)
{
    /* +0x24 code 0x1b hands this a voice handle instead of a record (see
     * snd_horn), so the read goes through a volatile to stay a read. */
    volatile struct snd_rec *r = rec;
    uint32_t v = r->voice;
    if (v != 0) {
        H_VOICE_STOP(v);
        r->playing = 0;
    }
}

static int32_t FS98_STDCALL snd_load(struct snd_rec *rec);
static void FS98_STDCALL snd_ensure(struct snd_rec *rec);

/* .text+0xc0 */
static int32_t FS98_STDCALL snd_play(struct snd_rec *rec, int32_t loop)
{
    int32_t r;
    if (rec->flags & 1)
        return -1;
    if (rec->voice == 0)
        snd_ensure(rec);
    r = H_VOICE_PLAY(rec->voice, loop, 0.0, NULL);
    if (r != -1 && loop == 1)
        rec->playing = loop;
    return r;
}

/* .text+0x110 */
static int32_t FS98_STDCALL snd_volume(struct snd_rec *rec, int32_t v)
{
    int32_t x;
    if (rec->voice == 0)
        return v;
    if (v < rec->minvol && rec->playing != 0) {
        snd_stop(rec);
        return 0;
    }
    x = v + rec->fader - 10000;
    if (x > rec->maxvol)
        x = rec->maxvol;
    if (x < 0)
        x = 0;
    if (x > 10000)
        x = 10000;
    if (x < 0)
        x = 0;
    H_VOICE_VOLUME(rec->voice, x);
    return x;
}

/* .text+0x190 */
static void FS98_STDCALL snd_pan(struct snd_rec *rec, int32_t pan)
{
    if (rec->flags & 1)
        return;
    rec->pan = pan;
    if (rec->voice != 0)
        H_VOICE_PAN(rec->voice, pan);
}

/* .text+0x1c0 -- walked over every sound */
static int32_t FS98_STDCALL snd_fader_cb(void *elem, void *ctx)
{
    struct snd_rec *rec = elem;
    (void)ctx;
    rec->fader = S_FADER(rec->group);
    return 0;
}

/* .text+0x1e0 -- the five faders from FLTSIM98.CFG [PREFS] */
static int32_t snd_read_faders(void)
{
    char key[16];
    double v;
    int32_t i;
    for (i = 1; i < 6; i++) {
        wsprintfA(key, DSTR(A(0x208d6f08)), i);     /* SOUND_FADER%d */
        v = 0.8;
        H_CFG_DOUBLE(DSTR(A(0x208d6f00)), key, &v); /* PREFS */
        if (FC0((long double)v, K_008)) {
            S_FADER(i) = 0;
            S_FADER_ON(i) = 0;
        } else {
            S_FADER(i) = UTIL_DTOI((double)(K_018 - (long double)v * K_010));
            S_FADER_ON(i) = 1;
        }
    }
    return H_LIST_WALK(S_SOUNDS, &S_FADER_ON(0), snd_fader_cb);
}

/* .text+0x2b0 -- +0x3c: write the faders back, then reread them */
static int32_t FS98_STDCALL snd_save_faders(const double *v)
{
    char key[16];
    int32_t i;
    for (i = 1; i < 6; i++) {
        wsprintfA(key, DSTR(A(0x208d6f08)), i);
        H_CFG_PUT_DOUBLE(DSTR(A(0x208d6f00)), key, *v++);
    }
    return snd_read_faders();
}

/* .text+0x310 -- +0x40 */
static int32_t FS98_STDCALL snd_load_faders(double *v)
{
    char key[16];
    int32_t i, r = 0;
    for (i = 1; i < 6; i++) {
        wsprintfA(key, DSTR(A(0x208d6f08)), i);
        *v = 0.8;
        r = H_CFG_DOUBLE(DSTR(A(0x208d6f00)), key, v);
        v++;
    }
    return r;
}

/* .text+0x370 -- +0x34: { on[6], value[6] }, slot 0 included */
static int32_t FS98_STDCALL snd_set_faders(int32_t *f)
{
    int32_t k;
    uint32_t v;
    for (k = 0; k < 6; k++) {
        if (f[k] != 0) {
            S_FADER_ON(k) = 1;
            v = (uint32_t)f[6 + k];
            if (v > 10000)
                v = 10000;
            S_FADER(k) = (int32_t)v;
        }
    }
    return H_LIST_WALK(S_SOUNDS, f, snd_fader_cb);
}

/* .text+0x3d0 -- +0x38.  Returns its cursor, f + 6, as retail's EAX is. */
static int32_t FS98_STDCALL snd_get_faders(int32_t *f)
{
    int32_t k;
    for (k = 0; k < 6; k++)
        if (f[k] != 0)
            f[6 + k] = S_FADER(k);
    return (int32_t)(uintptr_t)(f + 6);
}

/* .text+0x400 -- +0x2c */
static void FS98_STDCALL snd_set_fader(int32_t group, int32_t value)
{
    int32_t f[12];
    if (group <= 0 || group >= 6)
        return;
    H_MEMSET(f, 0, sizeof f);
    f[group] = 1;
    f[6 + group] = value;
    snd_set_faders(f);
}

/* .text+0x450 -- +0x30 */
static int32_t FS98_STDCALL snd_get_fader(int32_t group)
{
    if (group > 0 && group < 6)
        return S_FADER(group);
    return 0;
}

/* .text+0x470 -- +0x28: the wind.  Retail ignores its three arguments.
 *
 * The rate block (0x208d2750) holds minimum/maximum rate at +0x08/+0x10,
 * minimum/maximum speed at +0x18/+0x20, and the three derived values at
 * +0x28 (volume per decade), +0x30 (log10 of the minimum speed) and +0x38
 * (rate per unit of speed). */
static void FS98_STDCALL snd_wind(uint32_t a, uint32_t b, uint32_t c)
{
    struct snd_rec *rec;
    uint8_t *p;
    double cur, rate = 1.0;
    int32_t vol;
    (void)a; (void)b; (void)c;

    if (S_SOUNDS->data == NULL)
        return;
    rec = snd_find(0x13);
    cur = SIM_VALUE();
    if (G8(0x365) != 0 || S_WIND_ARMED == 0)
        return;
    if (FC3((long double)S_WIND_LAST, cur))
        return;
    if (rec->flags & 1)
        return;
    S_WIND_ARMED = 0;
    p = rec->params;
    if (!((long double)cur > PF64(p, 0x18))) {
        if (!FC3((long double)PF64(p, 0x18), K_008)
            || !FC3((long double)PF64(p, 0x20), K_008)) {
            vol = 0;
            goto set;
        }
    }
    if (FC3((long double)cur, K_008))
        vol = rec->minvol;
    else if ((long double)cur > PF64(p, 0x20))
        vol = rec->maxvol;
    else
        vol = UTIL_DTOI((double)((long double)(int64_t)(uint32_t)rec->minvol
                                 + (snd_log10(cur) - PF64(p, 0x30)) * PF64(p, 0x28)));
    if (FC03((long double)cur, PF64(p, 0x20)))
        rate = (double)(((long double)cur - PF64(p, 0x18)) * PF64(p, 0x38) + PF64(p, 0x08));
    else
        rate = PF64(p, 0x10);
    if ((uint32_t)vol > 10000)
        vol = 10000;
set:
    snd_volume(rec, vol);
    H_VOICE_FREQ(rec->voice, rate);
    if (vol != 0 && rec->playing == 0)
        snd_play(rec, 1);
    S_WIND_LAST = cur;
}

/* .text+0x620 */
static int32_t FS98_STDCALL snd_stop_cb(void *elem, void *ctx)
{
    (void)ctx;
    snd_stop(elem);
    return 0;
}

static void snd_engines_tick(void);

/* .text+0x630 -- everything quiet */
static void snd_silence(void)
{
    if (S_OFF != 0)
        return;
    S_CRASHING = 0;
    S_WIND_ARMED = 0;
    snd_engines_stop();
    snd_markers_off(0x8007);
    snd_flaps(0, 1);
    H_LIST_WALK(S_SOUNDS, NULL, snd_stop_cb);
}

/* .text+0x680 -- +0x44: which marker lights are on */
static uint32_t snd_markers(void)
{
    return (uint32_t)S_MARKER_BITS;
}

/* .text+0x690 -- a file under the sound directory (string 0x9005) */
static char *FS98_STDCALL snd_fullpath(const char *name)
{
    char *p;
    H_ALLOC(1, 0x104, (void **)&p, 1);
    H_LOADSTRING(0x9005, p, 0x104);
    H_JOIN(p, NULL, p, name);
    return p;
}

/* .text+0x6f0 */
static void FS98_STDCALL snd_rec_init(struct snd_rec *rec)
{
    H_MEMSET(rec, 0, sizeof *rec);
    rec->id = 0;
    rec->type = 2;
    rec->volume = 10000;
    rec->maxvol = 10000;
    rec->fader = 10000;
}

/* .text+0x730 */
static int32_t FS98_STDCALL snd_load(struct snd_rec *rec)
{
    int32_t r = H_VOICE_LOAD(rec->filename, rec->type, &rec->voice);
    if ((r == 0 || r == 0x90003) && rec->type != 4) {
        snd_volume(rec, rec->volume);
        H_VOICE_PAN(rec->voice, rec->pan);
    }
    return r == 0;
}

/* .text+0x790 */
static int32_t FS98_STDCALL snd_next_id_cb(void *elem, void *ctx)
{
    int32_t id = *(int32_t *)elem;
    if (*(int32_t *)ctx <= id)
        *(int32_t *)ctx = id + 1;
    return 0;
}

/* .text+0x7b0 -- one more than the largest id in the array, from 0x23 */
static int32_t FS98_STDCALL snd_next_id(snd_list *l, int32_t *out)
{
    *out = 0x23;
    return H_LIST_WALK(l, out, snd_next_id_cb) != 0 ? -1 : 0;
}

/* .text+0x7e0 -- a new record in `l` */
static int32_t FS98_STDCALL snd_add(snd_list *l, struct snd_rec **out, int32_t *id,
                                    int32_t chain)
{
    int32_t r;
    if (out == NULL || id == NULL)
        return -1;
    *out = H_LIST_ADD(l);
    if (*out == NULL)
        return -1;
    snd_rec_init(*out);
    (*out)->chain = chain;
    r = snd_next_id(l, id);
    if (r != 0) {
        H_LIST_REMOVE(l, *out);
        *out = NULL;
        return r;
    }
    (*out)->id = *id;
    return 0;
}

/* .text+0x890 */
static int32_t FS98_STDCALL snd_ensure_cb(void *elem, void *ctx)
{
    struct snd_rec *rec = elem;
    (void)ctx;
    if (!(rec->flags & 1))
        snd_load(rec);
    return 0;
}

/* .text+0x860 -- load the voice, or every sub-sound's */
static void FS98_STDCALL snd_ensure(struct snd_rec *rec)
{
    if (rec->flags & 1)
        return;
    if (rec->links != NULL)
        H_LIST_WALK(rec->links, NULL, snd_ensure_cb);
    else
        snd_load(rec);
}

static void snd_list_init(snd_list *l)
{
    l->count = 0;
    l->capacity = 0;
    l->grow = 0x20;
    l->elem_size = 0x44;
    l->iterating = 0;
    l->pending = 0;
    l->data = NULL;
}

/* .text+0x980 -- an engine's sub-sounds, from a static list in .data, for
 * an aircraft whose SOUND.CFG did not give them */
static void FS98_STDCALL snd_default_links(struct snd_rec *rec, const struct snd_rec *tmpl)
{
    const struct snd_rec *src;
    struct snd_rec *e;
    uint32_t size;

    H_ALLOC(1, 0x1c, (void **)&rec->links, 0);
    snd_list_init(rec->links);
    src = (const struct snd_rec *)tmpl->params;
    if (src->id == 0)
        return;
    do {
        e = H_LIST_ADD(rec->links);
        memcpy(e, src, sizeof *e);
        e->filename = snd_fullpath(e->filename);
        if (src->params != NULL && src->chain == 1) {
            size = *(uint32_t *)src->params;
            H_ALLOC(1, size, (void **)&e->params, 0);
            H_MEMMOVE(e->params, src->params, size);
        }
        src++;
    } while (src->id != 0);
}

/* .text+0x8b0 -- the 29 records from their templates, then the defaults */
static int32_t snd_build(void)
{
    uint8_t *t;
    struct snd_rec *e, *r;

    S_SOUNDS->count = 0;
    S_SOUNDS->capacity = 0;
    S_SOUNDS->grow = 0x20;
    S_SOUNDS->elem_size = 0x44;
    S_SOUNDS->iterating = 0;
    S_SOUNDS->pending = 0;
    S_SOUNDS->data = NULL;
    for (t = D + A(0x208d637c); t < D + A(0x208d6c18); t += 0x4c) {
        const char *name = *(const char **)(t - 4);
        snd_tmpl_fn fn = *(snd_tmpl_fn *)t;
        e = H_LIST_ADD(S_SOUNDS);
        memcpy(e, t + 4, sizeof *e);
        if (name != NULL && fn != NULL)
            fn(e, name);
    }
    r = snd_find(1);
    if (!(r->flags & 1) && r->links == NULL) {
        snd_default_links(r, (const struct snd_rec *)(D + A(0x208d63cc)));
        snd_ensure(r);
    }
    r = snd_find(3);
    if (!(r->flags & 1) && r->links == NULL) {
        snd_default_links(r, (const struct snd_rec *)(D + A(0x208d6464)));
        snd_ensure(r);
    }
    snd_read_faders();
    return 0;
}

/* .text+0xa50 -- the ident slots, staggered by a random first count */
static void snd_markers_init(void)
{
    int32_t k;
    for (k = 0; k < 5; k++) {
        M_ON(k) = 0;
        M_VOICE(k) = 0;
        M_COUNT(k) = 1 - (int32_t)snd_ftol((long double)UTIL_FRAND() * K_028);
        M_RELOAD(k) = 15;
        if (k == 2 || k == 3)
            M_RELOAD(k) = 30;
        M_INDEX(k) = k;
        M_WORD(k) = 0;
        M_STR(k) = NULL;
    }
    if (S_OFF == 0)
        snd_build();
}

static void FS98_STDCALL snd_cb_messages(uint32_t msg, uint32_t b);
static void FS98_STDCALL snd_cb_marker_light(uint32_t op, int32_t idx);
static void FS98_STDCALL snd_cb_frame(uint32_t a, uint32_t b);

/* .text+0xad0 -- +0x18 */
static int32_t snd_start(void)
{
    int32_t r;
    S_OFF = H_SOUND_OFF() == 0;
    if (S_OFF != 0)
        return S_OFF;
    CHAIN_REGISTER(0x14, snd_cb_messages);
    CHAIN_REGISTER(0x12, snd_cb_marker_light);
    CHAIN_REGISTER(3, snd_cb_frame);
    r = G32(0x1214);
    if (r == 0) {
        G32(0xb20) = 0;
        return 0;
    }
    if (r - 1 != 0)
        return r - 1;
    G32(0xb20) = 1;
    return 0;
}

static void snd_shutdown(void);
static int32_t snd_paths(void);
static void FS98_STDCALL snd_clear_listener(void *p);

/* .text+0xb40 -- CHAIN list 0x14 */
static void FS98_STDCALL snd_cb_messages(uint32_t msg, uint32_t b)
{
    uint32_t i;
    (void)b;
    switch (msg) {
    case 0x21:
        S_SPLASHING = 0;
        S_CRASHING = 0;
        break;
    case 0x40: case 0x42: case 0x44:
        snd_silence();
        break;
    case 0x60:
        H_BUSY(1);
        snd_shutdown();
        snd_paths();
        snd_markers_init();
        snd_clear_listener(S_LISTENER);
        S_CRASHING = 0;
        snd_engines_stop();
        for (i = 0; i < 5; i++)
            CHAIN_DISPATCH(0x12, 0, i);
        H_BUSY(0);
        break;
    }
}

/* .text+0xc30 -- CHAIN list 0x12: a marker light off, on or toggled.
 * `idx` is checked only against 5, signed, so 5 and anything negative
 * reach past the five slots -- into the fader table above them, or below
 * them -- and the shift takes its count mod 32.  Retail's, kept. */
static void FS98_STDCALL snd_cb_marker_light(uint32_t op, int32_t idx)
{
    if (idx > 5)
        return;
    switch (op) {
    case 0:
        S_MARKER_BITS &= ~(1u << (idx & 31));
        M_ON(idx) = 0;
        break;
    case 1:
        M_ON(idx) = 1;
        S_MARKER_BITS |= 1u << (idx & 31);
        break;
    case 2:
        M_ON(idx) = M_ON(idx) == 0;
        S_MARKER_BITS ^= 1u << (idx & 31);
        break;
    }
}

/* .text+0xce0 -- the free function a params block carries at +0x4 */
static int32_t FS98_STDCALL snd_free_block(void *pp)
{
    return H_FREE(pp);
}

/* .text+0xd00 */
static int32_t FS98_STDCALL snd_rec_free(struct snd_rec *rec)
{
    if (rec->voice != 0) {
        if (rec->playing == 1) {
            H_VOICE_STOP(rec->voice);
            rec->playing = 0;
        }
        H_VOICE_FREE(&rec->voice);
    }
    if (rec->filename != NULL)
        H_FREE((void **)&rec->filename);
    if (rec->params != NULL && *(snd_free_fn *)(rec->params + 4) != NULL)
        (*(snd_free_fn *)(rec->params + 4))(&rec->params);
    return 0;
}

/* .text+0xd70 -- walked over a list, removing as it goes.  Returns the
 * record itself, which stops the walk, if the main array has no data. */
static int32_t FS98_STDCALL snd_free_cb(void *elem, void *ctx)
{
    struct snd_rec *rec = elem;
    if (S_SOUNDS->data == NULL)
        return (int32_t)(uintptr_t)elem;
    if (rec->links != NULL) {
        H_LIST_WALK(rec->links, rec->links, snd_free_cb);
        if (rec->links != NULL) {
            H_LIST_FREE(rec->links);
            H_FREE((void **)&rec->links);
        }
    } else {
        snd_rec_free(rec);
    }
    H_LIST_REMOVE(ctx, rec);
    return 0;
}

/* .text+0xdf0 */
static void snd_shutdown(void)
{
    int32_t k;
    S_CRASHING = 0;
    if (S_SOUNDS->data != NULL) {
        for (k = 0; k < 5; k++) {
            M_ON(k) = 0;
            if (M_VOICE(k) != 0)
                H_VOICE_FREE(&M_VOICE(k));
            M_WORD(k) = 0;
            if (M_STR(k) != NULL)
                H_FREE((void **)&M_STR(k));
        }
        H_LIST_WALK(S_SOUNDS, S_SOUNDS, snd_free_cb);
    }
    H_LIST_FREE(S_SOUNDS);
    S_SOUNDS->count = 0;
    S_SOUNDS->capacity = 0;
    S_SOUNDS->iterating = 0;
    S_SOUNDS->pending = 0;
    S_SOUNDS->data = NULL;
    S_SOUNDS->grow = 0x20;
    S_SOUNDS->elem_size = 0x44;
}

/* .text+0xea0 -- +0x1c */
static int32_t snd_stop_all(void)
{
    if (S_OFF != 0)
        return S_OFF;
    snd_shutdown();
    CHAIN_REMOVE(0x14, snd_cb_messages);
    CHAIN_REMOVE(0x12, snd_cb_marker_light);
    return CHAIN_REMOVE(3, snd_cb_frame);
}

/* .text+0xee0 -- CHAIN list 3: the idents repeat */
static void FS98_STDCALL snd_cb_frame(uint32_t a, uint32_t b)
{
    int32_t k;
    (void)a; (void)b;
    for (k = 0; k < 5; k++) {
        if (M_ON(k) == 0 || M_VOICE(k) == 0)
            continue;
        if (--M_COUNT(k) > 0)
            continue;
        M_COUNT(k) = M_RELOAD(k);
        H_VOICE_VOLUME(M_VOICE(k), snd_get_fader(2));
        H_VOICE_PLAY(M_VOICE(k), 0, 0.0, NULL);
    }
}

/* .text+0xf50 */
static void FS98_STDCALL snd_wind_arm(int32_t kind)
{
    struct snd_rec *rec = snd_find(0x13);
    int32_t on = (kind == 0 || kind == 1) && S_CRASHING == 0 && S_SPLASHING == 0;
    S_WIND_ARMED = on;
    if (rec->playing == 1 && on == 0)
        snd_stop(rec);
}

/* .text+0xfa0 -- flaps: 7 starts, 8 runs, 9 stops.  `force` resets.  Note
 * the state-1 test: it stops when *either* flap word matches last frame's. */
static void FS98_STDCALL snd_flaps(int32_t kind, int32_t force)
{
    struct snd_rec *r;
    uint32_t n;
    int32_t v;

    if (force != 0) {
        S_FLAPS_A = G32(0xbe0);
        S_FLAPS_B = G32(0xbe4);
        S_FLAPS_STATE = 0;
        H_VOICE_STOP(snd_find(7)->voice);
        H_VOICE_STOP(snd_find(8)->voice);
        H_VOICE_STOP(snd_find(9)->voice);
        S_FLAPS_FRAME = 0;
        return;
    }
    switch (S_FLAPS_STATE) {
    case 0:
        if (S_FLAPS_A == G32(0xbe0) && S_FLAPS_B == G32(0xbe4))
            goto keep;
        r = snd_find(7);
        if ((kind == 0 || kind == 1) && r->fader != 0) {
            snd_play(r, 0);
            S_FLAPS_FRAME = G16(0x2a8);
            snd_play(snd_find(8), 1);
        }
        S_FLAPS_STATE = 1;
        break;
    case 1:
        r = snd_find(8);
        if ((kind == 0 || kind == 1) && r->fader != 0) {
            if (r->playing == 0)
                snd_play(r, 1);
            n = (uint32_t)G16(0x2a8) - (uint32_t)S_FLAPS_FRAME;
            v = r->volume;
            if (n < 0x12)
                v += D32(A(0x208d6c20) + 4 * n);
            snd_volume(r, v);
        } else {
            snd_volume(r, 0);
        }
        if (S_FLAPS_A != G32(0xbe0) && S_FLAPS_B != G32(0xbe4))
            goto keep;
        snd_stop(snd_find(7));
        snd_stop(snd_find(8));
        if ((kind == 0 || kind == 1) && r->fader != 0)
            snd_play(snd_find(9), 0);
        snd_stop(r);
        S_FLAPS_STATE = 0;
        break;
    default:
        S_FLAPS_STATE = 0;
        break;
    }
keep:
    S_FLAPS_A = G32(0xbe0);
    S_FLAPS_B = G32(0xbe4);
}

static void FS98_STDCALL snd_pos48(double *out, const void *in48);
static void FS98_STDCALL snd_alt48(double *out, const void *in48);
static void FS98_STDCALL snd_listener(double *pos, uint8_t *st);

/* .text+0x1190 -- +0x20, every frame */
static int32_t snd_frame(void)
{
    int32_t kind, a, b;
    struct snd_rec *r;

    if (S_OFF != 0)
        return S_OFF;
    kind = GP(0x11a0) != NULL ? P32(GP(0x11a0), 0x14) : 0;
    if (S_LAST_ON != G32(0xb24)) {
        S_LAST_ON = G32(0xb24);
        if (G32(0xb24) == 0) {
            snd_silence();
            H_SOUND_ENABLE(0);
        } else {
            H_SOUND_ENABLE(1);
            snd_flaps(0, 1);
        }
    }
    if (G32(0xb24) != 1)
        return 0;
    snd_pos48(&S_POS[0], G + 0x560);
    snd_pos48(&S_POS[1], G + 0x568);
    snd_alt48(&S_POS[2], G + 0x570);
    snd_listener(S_POS, S_LISTENER);
    snd_wind_arm(kind);
    snd_flaps(kind, 0);
    a = S_CRASHING;
    b = S_SPLASHING;
    if (S_CRASH_LAST != a || S_SPLASH_LAST != b) {
        S_CRASH_LAST = a;
        S_SPLASH_LAST = b;
        if (a == 1 || b == 1) {
            snd_engines_stop();
            a = S_CRASHING;
            b = S_SPLASHING;
        }
    }
    if (a == 0 && b == 0)
        snd_engines_tick();
    if (G8(0xb6a) == 1 && S_PAUSE_LAST != 1) {
        snd_event(0xf, 0);
        snd_event(0x10, 0);
        snd_event(0x11, 0);
        snd_event(5, 0);
    } else {
        if (snd_find(10)->wanted != 0)
            snd_event(0xf, 0);
        if (snd_find(11)->wanted != 0)
            snd_event(0x10, 0);
        if (snd_find(12)->wanted != 0)
            snd_event(0x11, 0);
    }
    S_PAUSE_LAST = G8(0xb6a);
    r = snd_find(0x1d);
    if (r->playing != 0 && (S_KIND_LAST == 0 || S_KIND_LAST == 1)
        && kind != 0 && kind != 1)
        snd_stop(r);
    S_KIND_LAST = kind;
    if (G8(0x36c) != 0 && (kind == 0 || kind == 1))
        return 0;
    if (snd_find(0xd)->playing != 1)
        return 0;
    return snd_stall_stop();
}

/* .text+0x13b0 -- the [section] keys every sound has.  0 if it is disabled,
 * by its flags or by a missing file. */
static int32_t FS98_STDCALL snd_section(struct snd_rec *rec, const char *section)
{
    char file[0x104], name[0x104];

    H_PROF_INT(section, DSTR(A(0x208d6f6c)), &rec->flags, S_PATH, FS98_HOST_EXT_CFG);
    if (rec->flags & 1) {
        rec->filename = NULL;
        return 0;
    }
    file[0] = 0;
    H_PROF_STRING(section, DSTR(A(0x208d6f60)), file, 0x104, S_PATH, FS98_HOST_EXT_CFG);
    if (file[0] != 0) {
        H_SPLIT(file, NULL, NULL, name, NULL);
        H_JOIN(file, NULL, S_DIR, name);
        if (H_EXISTS(file, FS98_HOST_EXT_WAV) != 0) {
            H_JOIN(file, NULL, S_SOUNDDIR, name);
            if (H_EXISTS(file, FS98_HOST_EXT_WAV) != 0) {
                rec->filename = NULL;
                rec->flags |= 1;
                return 0;
            }
        }
        rec->filename = H_STRDUP(file);
    } else if (rec->filename != NULL) {
        rec->filename = snd_fullpath(rec->filename);
    } else {
        rec->flags |= 1;
    }
    H_PROF_INT(section, DSTR(A(0x208d6f50)), &rec->volume, S_PATH, FS98_HOST_EXT_CFG);
    H_PROF_INT(section, DSTR(A(0x208d6f40)), &rec->maxvol, S_PATH, FS98_HOST_EXT_CFG);
    H_PROF_INT(section, DSTR(A(0x208d6f30)), &rec->minvol, S_PATH, FS98_HOST_EXT_CFG);
    H_PROF_INT(section, DSTR(A(0x208d6f28)), &rec->pan, S_PATH, FS98_HOST_EXT_CFG);
    H_PROF_INT(section, DSTR(A(0x208d6f1c)), &rec->viewpoint, S_PATH, FS98_HOST_EXT_CFG);
    if ((uint32_t)rec->volume > 10000)
        rec->volume = 10000;
    if ((uint32_t)rec->maxvol > 10000)
        rec->maxvol = 10000;
    if ((uint32_t)rec->minvol > 10000)
        rec->minvol = 10000;
    if (rec->pan > 10000)
        rec->pan = 10000;
    if (rec->pan < -10000)
        rec->pan = -10000;
    return 1;
}

/* .text+0x1600 */
static int32_t FS98_STDCALL snd_tmpl_basic(struct snd_rec *rec, const char *section)
{
    return snd_section(rec, section);
}

/* .text+0x1620 -- the extra block's free function */
static int32_t FS98_STDCALL snd_free_extra(void *pp)
{
    uint8_t **p = pp;
    uint32_t i;
    for (i = 0; i < *(uint32_t *)(*p + 8); i++)
        if (*(char **)(*p + 0xc + 4 * i) != NULL)
            H_FREE((void **)(*p + 0xc + 4 * i));
    return H_FREE(pp);
}

/* .text+0x1670 -- crash and splash: up to eight alternative files */
static int32_t FS98_STDCALL snd_tmpl_extra(struct snd_rec *rec, const char *section)
{
    char buf[0x820];
    uint8_t *blk;
    char *tok;
    uint32_t n = 0;

    if (snd_section(rec, section) == 0)
        return 0;
    H_ALLOC(1, 0x2c, (void **)&rec->params, 1);
    blk = rec->params;
    *(uint32_t *)blk = 0x2c;
    *(snd_free_fn *)(blk + 4) = snd_free_extra;
    buf[0] = 0;
    H_PROF_STRING(section, DSTR(A(0x208d6f74)), buf, sizeof buf, S_PATH, FS98_HOST_EXT_CFG);
    if (buf[0] != 0) {
        tok = strtok(buf, DSTR(A(0x208d6f18)));
        while (tok != NULL && n < 8) {
            *(char **)(blk + 0xc + 4 * n) = H_STRDUP(tok);
            n++;
            tok = strtok(NULL, DSTR(A(0x208d6f18)));
        }
    }
    *(uint32_t *)(blk + 8) = n;
    return 1;
}

/* .text+0x1750 -- the wind's rate and volume curves */
static int32_t FS98_STDCALL snd_tmpl_rate(struct snd_rec *rec, const char *section)
{
    uint8_t *p;
    long double t;

    if (snd_section(rec, section) == 0)
        return 0;
    H_ALLOC(1, 0x40, (void **)&rec->params, 1);
    p = rec->params;
    PF64(p, 0x10) = 1.0;
    PF64(p, 0x08) = 1.0;
    PF64(p, 0x18) = 0.0;
    PF64(p, 0x20) = 0.0;
    *(uint32_t *)p = 0x40;
    *(snd_free_fn *)(p + 4) = snd_free_block;
    H_PROF_DOUBLE(section, DSTR(A(0x208d6fac)), (double *)(p + 0x08), S_PATH, FS98_HOST_EXT_CFG);
    H_PROF_DOUBLE(section, DSTR(A(0x208d6f9c)), (double *)(p + 0x10), S_PATH, FS98_HOST_EXT_CFG);
    H_PROF_DOUBLE(section, DSTR(A(0x208d6f8c)), (double *)(p + 0x18), S_PATH, FS98_HOST_EXT_CFG);
    H_PROF_DOUBLE(section, DSTR(A(0x208d6f7c)), (double *)(p + 0x20), S_PATH, FS98_HOST_EXT_CFG);
    if (FC0((long double)PF64(p, 0x18), K_030))
        PF64(p, 0x18) = 0.0;
    if (FC0((long double)PF64(p, 0x20), K_030))
        PF64(p, 0x20) = 0.0;
    if (!FC3((long double)PF64(p, 0x18), K_030))
        t = (long double)(int64_t)(uint32_t)(rec->maxvol - rec->minvol)
            / snd_log10((long double)PF64(p, 0x20) / PF64(p, 0x18));
    else
        t = K_030;
    PF64(p, 0x28) = (double)t;
    if (!FC3((long double)PF64(p, 0x18), K_030))
        t = snd_log10(PF64(p, 0x18));
    else
        t = K_030;
    PF64(p, 0x30) = (double)t;
    t = (long double)PF64(p, 0x20) - PF64(p, 0x18);
    if (!FC3(t, K_030))
        PF64(p, 0x38) = (double)(((long double)PF64(p, 0x10) - PF64(p, 0x08)) / t);
    else
        PF64(p, 0x38) = K_030;
    return 1;
}

/* .text+0x1900 -- a prop sub-sound's curves: rparams (two points, rate) and
 * vparams (eight, volume).  The volume points must rise; from the first
 * that does not, the last good one is repeated to the end. */
static void FS98_STDCALL snd_prop_params(struct snd_rec *rec, const char *section)
{
    uint8_t *p;
    double x, y;
    int32_t i, k;

    H_ALLOC(1, 0xac, (void **)&p, 1);
    rec->params = p;
    *(uint32_t *)p = 0xac;
    *(snd_free_fn *)(p + 4) = snd_free_block;
    H_PROF_DOUBLES(section, DSTR(A(0x208d6fc4)), (double *)(p + 0xc), 4, S_PATH,
                   FS98_HOST_EXT_CFG);
    H_PROF_DOUBLES(section, DSTR(A(0x208d6fbc)), (double *)(p + 0x2c), 0x10, S_PATH,
                   FS98_HOST_EXT_CFG);
    x = PF64(p, 0x2c);
    y = PF64(p, 0x34);
    for (i = 1; i < 8; i++) {
        if (!FC0((long double)x, PF64(p, 0x2c + 0x10 * i)))
            break;
        x = PF64(p, 0x2c + 0x10 * i);
        y = PF64(p, 0x34 + 0x10 * i);
    }
    for (k = i; k < 8; k++) {
        PF64(p, 0x2c + 0x10 * k) = x;
        PF64(p, 0x34 + 0x10 * k) = y;
    }
}

/* .text+0x1a00 -- one link of an engine's chain: a new sub-sound from
 * [name], whose `link` key names the next */
static int32_t FS98_STDCALL snd_link(struct snd_rec *rec, struct snd_rec **out, char *name,
                                     void (FS98_STDCALL *cb)(struct snd_rec *, const char *))
{
    struct { int32_t id; char sec[0x104]; } l;

    if (snd_add(rec->links, out, &l.id, rec->chain) != 0)
        return 0;
    if (snd_section(*out, name) == 0)
        return 0;
    if ((*out)->flags & 1)
        return 0;
    if (cb != NULL)
        cb(*out, name);
    H_STRCPY(l.sec, name);
    name[0] = 0;
    H_PROF_STRING(l.sec, DSTR(A(0x208d6fcc)), name, 0x104, S_PATH, FS98_HOST_EXT_CFG);
    return 1;
}

/* .text+0x1ab0 -- SOUND_ENGINE and JET_ENGINE.  Only the one matching the
 * aircraft's view kind, and its `type` key, survives. */
static int32_t FS98_STDCALL snd_tmpl_engine(struct snd_rec *rec, const char *section)
{
    char name[0x104];
    struct snd_rec *sub;
    void (FS98_STDCALL *cb)(struct snd_rec *, const char *);
    int32_t kind = G8(0x609);
    uint32_t n;

    H_PROF_INT(section, DSTR(A(0x208d6fd4)), &kind, S_PATH, FS98_HOST_EXT_CFG);
    if (kind != G8(0x609)
        || (kind == 0 && rec->id != 1)
        || ((kind == 1 || kind == 3) && rec->id != 3)
        || kind == 2) {
        rec->flags |= 1;
        rec->filename = NULL;
        rec->params = NULL;
        return 0;
    }
    cb = kind == 0 ? snd_prop_params : NULL;
    H_ALLOC(1, 0x1c, (void **)&rec->links, 0);
    snd_list_init(rec->links);
    H_STRCPY(name, section);
    n = 1;
    if (snd_link(rec, &sub, name, cb) != 0) {
        while (++n <= 0x10 && name[0] != 0)
            if (snd_link(rec, &sub, name, cb) == 0)
                break;
    }
    return 1;
}

/* .text+0x1c10 -- the aircraft's sound folder and SOUND.CFG, from ACONTAIN */
static int32_t snd_paths(void)
{
    char dir[0x100], cfg[0x104];
    if (ACONTAIN_QUERY(G + 0x11e0, 8, cfg, 0x104) == 0)
        return -1;
    if (ACONTAIN_QUERY(G + 0x11e0, 7, dir, 0x100) == 0)
        return -1;
    H_LOADSTRING(0x9005, S_SOUNDDIR, 0x104);
    H_STRCPY(S_PATH, cfg);
    return (int32_t)(uintptr_t)H_STRCPY(S_DIR, dir);
}

/* .text+0x1cc0 -- double to 48-bit, the integer part signed going back */
static void FS98_STDCALL snd_to48(const double *in, void *out48)
{
    long double x = *in;
    int32_t hi;
    if (FC0(x, K_038))
        x = (K_D6FE0 - x * K_040) * K_048;
    hi = (int32_t)snd_ftol(x);
    *(int32_t *)((uint8_t *)out48 + 4) = hi;
    *(uint16_t *)((uint8_t *)out48 + 2) =
        (uint16_t)snd_ftol((x - (long double)hi) * K_050);
}

/* .text+0x1d20 -- 48-bit to double, metres; the high half taken unsigned
 * and a negative one folded back */
static void FS98_STDCALL snd_pos48(double *out, const void *in48)
{
    uint32_t lo = *(const uint16_t *)((const uint8_t *)in48 + 2);
    uint32_t hi = *(const uint32_t *)((const uint8_t *)in48 + 4);
    long double v = (long double)(int32_t)lo - (long double)(int64_t)hi * K_040;
    *out = (double)v;
    if ((int32_t)hi < 0)
        *out = (double)((K_D6FE0 - v) * K_058);
    *out = (double)((long double)*out * K_048);
}

/* .text+0x1d90 -- altitude: signed high half, fraction */
static void FS98_STDCALL snd_alt48(double *out, const void *in48)
{
    int32_t hi = *(const int32_t *)((const uint8_t *)in48 + 4);
    uint32_t lo = *(const uint16_t *)((const uint8_t *)in48 + 2);
    *out = (double)((long double)hi + (long double)(int32_t)lo * K_048);
}

/* .text+0x1dc0 -- log10 by 1/log2(10), with x <= 0 taken as 1 */
static double FS98_STDCALL snd_log10b(double x)
{
    if (FC03((long double)x, K_038))
        x = 1.0;
    return (double)snd_log10_l2t(x);
}

/* .text+0x1e10 */
static void FS98_STDCALL snd_clear_listener(void *p)
{
    H_MEMSET(p, 0, 0x28);
}

/* .text+0x1e30 -- where the listener is: the tower view's eyepoint if the
 * view has one, else the aircraft */
static void FS98_STDCALL snd_eyepoint(double *out, const double *src)
{
    uint8_t *v = GP(0x11a0), *w = GP(0x119c);
    if (v != NULL && w != NULL && (P32(w, 8) & 0x80000000) && P32(v, 0x10) != 1) {
        snd_pos48(&out[0], v + 0x58);
        snd_pos48(&out[1], v + 0x60);
        snd_alt48(&out[2], v + 0x68);
        return;
    }
    out[0] = src[0];
    out[1] = src[1];
    out[2] = src[2];
}

/* .text+0x1ed0 -- the listener: squared range at +0x00, range in 16.16 at
 * +0x08, its rate averaged over four frames at +0x0c, bearing in degrees at
 * +0x10, horizontal range at +0x18, the four rates from +0x20 */
static void FS98_STDCALL snd_listener(double *pos, uint8_t *st)
{
    uint8_t d48[8], m48[8];
    double dx, dy, dz, e, sum;
    long double ldx, dzc, h2, s3, h;
    int32_t old, rate, n;
    int16_t h0, h1, h2w;

    snd_eyepoint(S_EYE, pos);
    ldx = (long double)pos[0] - S_EYE[0];
    dx = (double)ldx;
    dz = (double)((long double)pos[2] - S_EYE[2]);
    dy = (double)((long double)pos[1] - S_EYE[1]);
    if (ldx > K_068)        /* compared before it was rounded, as retail's is */
        dx = K_068;
    snd_to48(&dy, d48);
    UTIL_LL2M(d48, G + 0x560, m48);
    snd_pos48(&e, m48);
    if ((long double)e > K_068)
        e = K_068;
    dzc = dz;
    if (dzc > K_068)
        dzc = K_068;
    h2 = (long double)dx * dx + (long double)e * e;
    s3 = dzc * dzc + h2;
    h = snd_sqrt(h2);
    sum = (double)s3;
    old = P32(st, 8);
    PF64(st, 0) = sum;
    PF64(st, 0x18) = (double)h;
    P32(st, 8) = (int32_t)snd_ftol(snd_sqrt(s3) * K_070);
    n = GS16(0x2aa);
    rate = n != 0 ? (P32(st, 8) - old) / n : 0;
    h0 = *(int16_t *)(st + 0x20);
    h1 = *(int16_t *)(st + 0x22);
    h2w = *(int16_t *)(st + 0x24);
    *(int16_t *)(st + 0x26) = h2w;
    *(int16_t *)(st + 0x24) = h1;
    *(int16_t *)(st + 0x20) = (int16_t)rate;
    *(int16_t *)(st + 0x22) = h0;
    *(int16_t *)(st + 0xc) = (int16_t)(((int32_t)(int16_t)rate + h1 + h2w + h0) / 4);
    PF64(st, 0x10) = (double)(snd_atan2(e, dx) * K_078);
}

/* .text+0x2070 */
static int32_t snd_crash_reset(void)
{
    S_CRASHING = 0;
    return 0;
}

/* .text+0x2080 -- the crash or splash sample finished */
static void FS98_STDCALL snd_crash_done(uint32_t voice)
{
    void (*cb)(void);
    (void)voice;
    snd_find(0x11)->playing = 0;
    cb = S_CRASH_CB;
    S_CRASHING = 0;
    if (cb != NULL) {
        cb();
        S_CRASH_CB = NULL;
    }
}

static int32_t snd_gearwarn_stop(void);

/* 10000 less the listener's range in units of 2 metres, 0 past 10 km */
static int32_t snd_range_volume(void)
{
    return S_DIST > 0x13880000u ? 0 : 10000 - (int32_t)(S_DIST >> 15);
}

/* .text+0x20b0 -- crash (0) or splash (1), one of up to eight files */
static int32_t FS98_STDCALL snd_crash(int32_t splash)
{
    struct snd_rec *r;
    uint8_t *ex;
    int32_t vol, idx;

    snd_stall_stop();
    snd_gearwarn_stop();
    snd_markers_off(7);
    vol = snd_range_volume();
    r = snd_find(splash != 0 ? 0x12 : 0x11);
    ex = r->params;
    if (r->fader != 0 && !(r->flags & 1)) {
        if (ex != NULL && *(uint32_t *)(ex + 8) > 0) {
            long double n = (long double)(int64_t)(uint64_t)UTIL_RAND32();
            idx = (int32_t)snd_ftol(n * (long double)(int64_t)(uint64_t)*(uint32_t *)(ex + 8)
                                    * K_080);
            if (r->filename != NULL)
                H_FREE((void **)&r->filename);
            r->filename = snd_fullpath(*(char **)(ex + 0xc + 4 * idx));
        }
        snd_volume(r, vol);
        S_CRASHING = 1;
        if (r->voice == 0)
            snd_load(r);
        if (r->voice != 0)
            r->playing = H_VOICE_PLAY(r->voice, 0, 0.0, snd_crash_done) == 0;
    } else {
        snd_crash_done((uint32_t)(uintptr_t)r);
    }
    snd_stop(snd_find(0x13));
    return 0;
}

/* .text+0x21f0 -- +0x24 code 0xe: { flags, sub, -, done callback, splash } */
static intptr_t FS98_STDCALL snd_crash_event(uint32_t *ev)
{
    if (ev[0] & 0x80000000)
        return -1;
    if (ev[0] & 8)
        S_SPLASHING = (int32_t)ev[4];
    if (ev[0] & 4)
        S_CRASH_CB = (void (*)(void))(uintptr_t)ev[3];
    else
        S_CRASH_CB = NULL;
    if (G32(0xb24) == 1 && (ev[0] & 0x10) && (ev[0] & 1))
        return snd_crash((int32_t)((ev[1] >> 3) & 1));
    /* retail returns whatever EAX held: `ev` itself, the caller's copy on
     * the stack.  REALITY tests it against zero. */
    return (intptr_t)ev;
}

/* .text+0x2260 -- a marker beacon: plays now, or is held for when sound
 * comes back */
static int32_t FS98_STDCALL snd_marker(int32_t id, uint32_t bits)
{
    struct snd_rec *r = snd_find(id);
    if (r->fader == 0 || G8(0xb6a) != 0)
        return 0;
    if (G32(0xb24) == 1) {
        snd_play(r, 1);
        r->wanted = 0;
        snd_markers_off(bits);
        return 0;
    }
    r->wanted = 1;
    r->playing = 0;
    snd_markers_off(bits);
    return 0;
}

/* .text+0x22d0 */
static int32_t FS98_STDCALL snd_marker_off(int32_t id, uint32_t bits)
{
    struct snd_rec *r = snd_find(id);
    H_VOICE_STOP(r->voice);
    if (bits & 0x8000) {
        r->wanted |= r->playing;
        r->playing = 0;
        return 0;
    }
    r->wanted = 0;
    r->playing = 0;
    return 0;
}

/* .text+0x2320 -- bits 1, 2, 4: inner, middle, outer */
static int32_t FS98_STDCALL snd_markers_off(uint32_t bits)
{
    if (S_OFF != 0)
        return 0;
    if (bits & 1)
        snd_marker_off(0xc, bits);
    if (bits & 2)
        snd_marker_off(0xb, bits);
    if (bits & 4)
        snd_marker_off(0xa, bits);
    return 0;
}

/* .text+0x2360 -- a wheel touches down; `side` <0 left, 0 centre, >0 right */
static int32_t FS98_STDCALL snd_touchdown(int32_t kind, int32_t side)
{
    int32_t vol = snd_range_volume();
    struct snd_rec *r = snd_find(side > 0 ? 0x10 : side >= 0 ? 0xf : 0xe);
    if (r->fader != 0) {
        snd_volume(r, vol);
        snd_pan(r, kind == 0 || kind == 1 ? r->pan : 0);
        snd_play(r, 0);
    }
    return 0;
}

/* .text+0x23e0 -- id 0x22, which no template has: the default record */
static int32_t FS98_STDCALL snd_range_sound(uint32_t dist)
{
    struct snd_rec *r = snd_find(0x22);
    snd_volume(r, dist > 0x13880000u ? 0 : 10000 - (int32_t)(dist >> 15));
    snd_play(r, 0);
    return 0;
}

/* .text+0x2420 -- id 0x21, likewise.  Turning it off hands snd_stop the
 * *voice handle* as if it were a record: retail reads the handle's +0x1c
 * and stops whatever is there.  With no voice that is a read of address
 * 0x1c.  Kept. */
static int32_t FS98_STDCALL snd_horn(int32_t on)
{
    struct snd_rec *r = snd_find(0x21);
    if (on == 1 && G32(0xb24) != 0) {
        snd_volume(r, r->maxvol);
        snd_play(r, 1);
        return 0;
    }
    snd_stop((struct snd_rec *)(uintptr_t)r->voice);
    return 0;
}

/* .text+0x2470 */
static int32_t snd_gear_up(void)
{
    struct snd_rec *r = snd_find(5);
    if (r->fader != 0)
        snd_play(r, 0);
    return 0;
}

/* .text+0x2490 */
static int32_t snd_gear_down(void)
{
    struct snd_rec *r = snd_find(6);
    if (r->fader != 0) {
        snd_volume(r, r->maxvol);
        snd_play(r, 0);
    }
    return 0;
}

/* .text+0x24c0 */
static int32_t FS98_STDCALL snd_overspeed(int32_t on)
{
    struct snd_rec *r = snd_find(0x1d);
    if (!(r->flags & 1) && on == 1 && r->fader != 0) {
        if (r->playing == 0) {
            snd_volume(r, r->maxvol);
            snd_play(r, 1);
        }
        return 0;
    }
    if (r->playing != 0)
        snd_stop(r);
    return 0;
}

/* .text+0x2510 */
static int32_t snd_ap_disconnect(void)
{
    struct snd_rec *r = snd_find(0x1f);
    if (r->fader != 0)
        snd_play(r, 0);
    return 0;
}

/* .text+0x2530 */
static int32_t snd_glideslope(void)
{
    struct snd_rec *r = snd_find(0x20);
    if (r->fader != 0 && G8(0xb6a) == 0)
        snd_play(r, 0);
    return 0;
}

/* .text+0x2560 */
static int32_t snd_stall_start(void)
{
    struct snd_rec *r = snd_find(0xd);
    int32_t ret = 0;
    if (r->fader != 0) {
        ret = snd_play(r, 1);
        r->playing = 1;
    }
    return ret;
}

/* .text+0x2590 */
static int32_t snd_stall_stop(void)
{
    snd_stop(snd_find(0xd));
    return 0;
}

/* .text+0x25a0 */
static int32_t snd_gearwarn_start(void)
{
    struct snd_rec *r = snd_find(0x1e);
    int32_t ret = 0;
    if (r->fader != 0 && G8(0xb6a) == 0) {
        ret = snd_play(r, 1);
        r->playing = 1;
    }
    return ret;
}

/* .text+0x25e0 */
static int32_t snd_gearwarn_stop(void)
{
    snd_stop(snd_find(0x1e));
    return 0;
}

/* .text+0x25f0 */
static int32_t FS98_STDCALL snd_tone(int32_t id)
{
    snd_play(snd_find(id), 0);
    return 0;
}

/* .text+0x2610 -- replace a string unless it is already that */
static int32_t FS98_STDCALL snd_set_string(char **slot, const char *s, int32_t *same)
{
    *same = 0;
    if (s == NULL)
        return -1;
    if (*slot != NULL) {
        if (H_STRCMP(*slot, s) == 0) {
            *same = 1;
            return 0;
        }
        H_FREE((void **)slot);
    }
    *slot = H_STRDUP(s);
    return 0;
}

static int32_t FS98_STDCALL snd_morse(const char *s, int32_t high, uint32_t *out);

/* .text+0x2680 -- +0x24 code 0x17: { slot, word, ident } */
static int32_t FS98_STDCALL snd_ident(int32_t *m)
{
    int32_t idx, high, same;
    int16_t w;

    if (m == NULL || m[0] >= 5 || m[0] < 0)
        return -1;
    idx = m[0];
    high = idx == 2 || idx == 3;
    w = *(int16_t *)(m + 1);
    if (w == 0) {
        if (M_VOICE(idx) != 0) {
            H_VOICE_STOP(M_VOICE(idx));
            H_VOICE_FREE(&M_VOICE(m[0]));
        }
        return 0;
    }
    if ((char *)(uintptr_t)m[2] != NULL && *(char *)(uintptr_t)m[2] != 0) {
        M_WORD(idx) = w;
        snd_set_string(&M_STR(idx), (char *)(uintptr_t)m[2], &same);
        if (same != 0)
            return 0;
        if (M_VOICE(idx) != 0) {
            H_VOICE_STOP(M_VOICE(idx));
            H_VOICE_FREE(&M_VOICE(m[0]));
        }
        return snd_morse(M_STR(idx), high, &M_VOICE(idx));
    }
    if (M_VOICE(idx) == 0)
        return 0;
    H_VOICE_STOP(M_VOICE(idx));
    H_VOICE_FREE(&M_VOICE(m[0]));
    if (M_STR(idx) != NULL)
        H_FREE((void **)&M_STR(idx));
    return 0;
}

/* .text+0x2810 -- +0x24 */
static int32_t FS98_STDCALL snd_event(int32_t code, uint32_t arg)
{
    int32_t kind;
    uint32_t ev[5];

    if (S_OFF != 0)
        return 0;
    kind = GP(0x11a0) != NULL ? P32(GP(0x11a0), 0x14) : 0;
    S_KIND = kind;
    switch (code) {
    case 1:
        if (G32(0xb24) == 0)
            return 0;
        return snd_crash_reset();
    case 3:
        if (G32(0xb24) != 0 && (kind == 0 || kind == 1))
            return snd_overspeed((int32_t)arg);
        return 0;
    case 5:
        if (arg == 0)
            return snd_gearwarn_stop();
        if (G32(0xb24) == 0)
            return 0;
        if (kind == 0 || kind == 1)
            snd_gearwarn_start();
        return 0;
    case 7:
        if (G32(0xb24) != 0 && (kind == 0 || kind == 1))
            return snd_glideslope();
        return 0;
    case 8:
        if (arg == 0)
            return snd_stall_stop();
        if (G32(0xb24) == 0)
            return 0;
        if (kind == 0 || kind == 1)
            snd_stall_start();
        return 0;
    case 9:
        if (G32(0xb24) != 0 && (kind == 0 || kind == 1))
            return snd_ap_disconnect();
        return 0;
    case 0xe:
        memcpy(ev, (const void *)(uintptr_t)arg, sizeof ev);
        return (int32_t)snd_crash_event(ev);
    case 0xf:
        if (kind == 0 || kind == 1)
            return snd_marker(0xa, 3);
        return 0;
    case 0x10:
        if (kind == 0 || kind == 1)
            return snd_marker(0xb, 5);
        return 0;
    case 0x11:
        if (kind == 0 || kind == 1)
            return snd_marker(0xc, 6);
        return 0;
    case 0x12:
        return snd_markers_off(7);
    case 0x13:
        if (G32(0xb24) == 0)
            return 0;
        return snd_touchdown(kind, (int32_t)arg);
    case 0x14:
        if (G32(0xb24) != 0 && (kind == 0 || kind == 1))
            return snd_gear_up();
        return 0;
    case 0x15:
        if (G32(0xb24) != 0 && (kind == 0 || kind == 1))
            return snd_gear_down();
        return 0;
    case 0x16:
    case 0x1c:
        return 0;
    case 0x17:
        return snd_ident((int32_t *)(uintptr_t)arg);
    case 0x18:
        return snd_tone(0x14);
    case 0x19:
        return snd_tone(0x15);
    case 0x1a:
        return snd_tone(0x16);
    case 0x1b:
        return snd_horn((int32_t)arg);
    case 0x1d:
        if (G32(0xb24) == 0)
            return 0;
        return snd_range_sound(arg);
    default:
        return 2;
    }
}

static int32_t FS98_STDCALL snd_morse_load(int32_t high);

/* .text+0x2b70 -- an ident in Morse: each letter's dits and dahs and a gap
 * after each, two gaps after the letter, appended into one voice */
static int32_t FS98_STDCALL snd_morse(const char *s, int32_t high, uint32_t *out)
{
    const char *code;
    int32_t r, i;

    *out = 0;
    r = snd_morse_load(high);
    if (r != 0)
        return r;
    for (; *s != 0; s++) {
        for (i = 0; D8(A(0x208d6fe8) + 8 * i) != 0; i++)
            if ((uint8_t)*s == D8(A(0x208d6fe8) + 8 * i))
                break;
        if (D8(A(0x208d6fe8) + 8 * i) == 0)
            continue;
        for (code = *(const char **)(D + A(0x208d6fec) + 8 * i); *code != 0; code++) {
            switch (*code) {
            case '.':
                r = H_VOICE_APPEND(S_MORSE_DIT, 7, S_MORSE_OUT, 0);
                break;
            case '-':
                r = H_VOICE_APPEND(S_MORSE_DAH, 7, S_MORSE_OUT, 0);
                break;
            case ' ':
                r = H_VOICE_APPEND(S_MORSE_GAP, 7, S_MORSE_OUT, 0);
                break;
            default:
                continue;
            }
            if (r != 0)
                goto fail;
        }
        r = H_VOICE_APPEND(S_MORSE_GAP, 7, S_MORSE_OUT, 0);
        if (r != 0)
            goto fail;
        r = H_VOICE_APPEND(S_MORSE_GAP, 7, S_MORSE_OUT, 0);
        if (r != 0)
            goto fail;
    }
    H_VOICE_APPEND(0, 0, S_MORSE_OUT, 1);
    *out = S_MORSE_OUT;
    snd_morse_free();
    return 0;
fail:
    H_VOICE_FREE(&S_MORSE_OUT);
    snd_morse_free();
    return r;
}

/* .text+0x2ee0 -- load sound `id`'s file as a new voice of `type`, and keep
 * the quietest volume seen */
static int32_t FS98_STDCALL snd_load_named(int32_t id, int32_t type, uint32_t *out)
{
    struct snd_rec *r = H_LIST_FIND(S_SOUNDS, 0, &id, 4, 0);
    int32_t ret;
    if (r == NULL)
        return -1;
    ret = H_VOICE_LOAD(r->filename, type, out);
    if ((uint32_t)S_MORSE_VOL > (uint32_t)r->volume)
        S_MORSE_VOL = r->volume;
    return ret;
}

/* .text+0x2da0 -- the output voice and the three samples, high or low */
static int32_t FS98_STDCALL snd_morse_load(int32_t high)
{
    int32_t r;

    S_MORSE_VOL = 10000;
    r = snd_load_named(0x1c, 2, &S_MORSE_OUT);
    if (r != 0)
        return r;
    r = snd_load_named(0x1c, 4, &S_MORSE_GAP);
    if (r != 0) {
        H_VOICE_FREE(&S_MORSE_OUT);
        return r;
    }
    r = snd_load_named(high == 1 ? 0x1a : 0x1b, 4, &S_MORSE_DIT);
    if (r != 0) {
        H_VOICE_FREE(&S_MORSE_OUT);
        H_VOICE_FREE(&S_MORSE_GAP);
        return r;
    }
    r = snd_load_named(high == 1 ? 0x18 : 0x19, 4, &S_MORSE_DAH);
    if (r != 0) {
        H_VOICE_FREE(&S_MORSE_OUT);
        H_VOICE_FREE(&S_MORSE_DIT);
        H_VOICE_FREE(&S_MORSE_GAP);
        return r;
    }
    H_VOICE_VOLUME(S_MORSE_OUT, S_MORSE_VOL);
    return 0;
}

/* .text+0x2f40 */
static void snd_morse_free(void)
{
    if (S_MORSE_DIT != 0)
        H_VOICE_FREE(&S_MORSE_DIT);
    if (S_MORSE_DAH != 0)
        H_VOICE_FREE(&S_MORSE_DAH);
    if (S_MORSE_GAP != 0)
        H_VOICE_FREE(&S_MORSE_GAP);
}

/* .text+0x2f90 -- the fastest engine's rpm, by engine count */
static uint16_t snd_rpm(void)
{
    uint16_t a = G16(0x898), b = G16(0x930), c = G16(0x9c8), d = G16(0xa60), m;
    switch (G16(0xaec)) {
    case 1:
        return a;
    case 2:
        return a > b ? a : b;
    case 3:
        m = b > c ? b : c;
        return a > m ? a : m;
    case 4:
        m = c > d ? c : d;
        if (b > m)
            m = b;
        return a > m ? a : m;
    default:
        return 0;
    }
}

/* .text+0x30b0 -- Doppler, from the listener's averaged range rate */
static long double snd_doppler(void)
{
    long double x = (long double)(int32_t)S_RATE_AVG;
    if (x > K_088)
        x = K_088;
    if (FC0(x, K_090))
        x = K_090;
    return K_0A0 / (x - K_098);
}

/* .text+0x3100 -- attenuation with range, in hundredths */
static long double snd_range_db(void)
{
    long double x = (long double)(int64_t)(uint64_t)S_DIST * K_0A8;
    if (FC0(x, K_0B0))
        x = K_0B0;
    return (long double)snd_log10b((double)(x * K_0B8)) * K_0C0;
}

/* .text+0x3420 -- the line through two points, at x */
static long double snd_interp(double x, double x0, double y0, double x1, double y1)
{
    long double m = ((long double)y1 - y0) / ((long double)x1 - x0);
    return ((long double)y0 - m * x0) + m * x;
}

/* .text+0x32a0 -- a prop sub-sound's rate and volume at `x` */
static int32_t FS98_STDCALL snd_curves(struct snd_rec *sub, double x, double *rate, double *vdb)
{
    uint8_t *p = sub->params;
    double c0, c1;
    int32_t i;

    if (p == NULL) {
        *rate = 0.0;
        *vdb = 0.0;
        return -1;
    }
    c0 = PF64(p, 0xc);
    if (!((long double)x > c0)) {
        *rate = PF64(p, 0x14);
    } else {
        c1 = PF64(p, 0x1c);
        if (FC0((long double)x, c1))
            *rate = (double)snd_interp(x, c0, PF64(p, 0x14), c1, PF64(p, 0x24));
        else
            *rate = PF64(p, 0x24);
    }
    if (!((long double)x > PF64(p, 0x2c))) {
        *vdb = PF64(p, 0x34);
        return 0;
    }
    if ((long double)x > PF64(p, 0x9c)) {
        *vdb = PF64(p, 0xa4);
        return 0;
    }
    for (i = 1; i < 8; i++)
        if (FC03((long double)x, PF64(p, 0x2c + 0x10 * i)))
            break;
    if (i == 8)
        return 0;
    *vdb = (double)snd_interp(x, PF64(p, 0x2c + 0x10 * (i - 1)), PF64(p, 0x34 + 0x10 * (i - 1)),
                              PF64(p, 0x2c + 0x10 * i), PF64(p, 0x34 + 0x10 * i));
    return 0;
}

/* Inside-only and outside-only sounds: viewpoint 1 is heard in the cockpit
 * views (kind 0 and 1), viewpoint 2 outside them. */
static int32_t snd_muted(int32_t viewpoint, int32_t kind)
{
    if (viewpoint == 1)
        return kind != 0 && kind != 1;
    if (viewpoint == 2)
        return kind == 0 || kind == 1;
    return 0;
}

/* .text+0x3150 -- one prop sub-sound */
static int32_t FS98_STDCALL snd_prop_cb(void *elem, void *ctx_)
{
    struct snd_rec *sub = elem;
    struct snd_prop_ctx *ctx = ctx_;
    double rate, vdb = 0.0;     /* snd_curves always sets it; gcc cannot tell */
    int32_t a, b;

    if (sub == NULL)
        return -1;
    if (ctx == NULL)
        return (int32_t)(uintptr_t)sub;
    if (ctx->flag != 0 || snd_muted(sub->viewpoint, ctx->kind)) {
        if (sub->playing != 0)
            snd_stop(sub);
        return 0;
    }
    if (sub->playing == 0) {
        snd_volume(sub, 0);
        snd_play(sub, 1);
    }
    snd_curves(sub, ctx->val, &rate, &vdb);
    if (ctx->kind != 0 && ctx->kind != 1)
        rate = (double)(snd_doppler() * rate);
    H_VOICE_FREQ(sub->voice, rate);
    sub->fader = snd_get_fader(1);
    if (FC0((long double)vdb, K_0C8)) {
        snd_volume(sub, 0);
        return 0;
    }
    a = (int32_t)snd_ftol(snd_log10(vdb) * K_0D0);
    b = (int32_t)snd_ftol((long double)ctx->db * K_0D8);
    snd_volume(sub, a - b + 8000);
    return 0;
}

/* .text+0x3450 -- a prop engine's frame */
static void FS98_STDCALL snd_prop(struct snd_rec *eng)
{
    struct snd_prop_ctx ctx;
    long double v;

    ctx.kind = GP(0x11a0) != NULL ? P32(GP(0x11a0), 0x14) : 0;
    v = (long double)(int32_t)snd_rpm() * (long double)(int32_t)G16(0x8c8) * K_0A8;
    if (FC0(v, K_0E0))
        ctx.flag = 1;
    else
        ctx.flag = S_DIST > 0x13880000u;
    ctx.val = (double)v;
    if (ctx.kind == 0 || ctx.kind == 1)
        ctx.db = 5.0;
    else
        ctx.db = (double)snd_range_db();
    H_LIST_WALK(eng->links, &ctx, snd_prop_cb);
}

/* .text+0x3520 -- a jet sub-sound flagged 2: the core */
static void FS98_STDCALL snd_jet_core(struct snd_rec *sub, struct snd_jet_ctx *ctx)
{
    if (ctx->flag == 1) {
        H_VOICE_VOLUME(sub->voice, 0);
        return;
    }
    sub->fader = snd_get_fader(1);
    snd_volume(sub, ctx->vol);
    H_VOICE_FREQ(sub->voice, ctx->rate);
}

/* .text+0x3580 -- flagged 4: the whine */
static void FS98_STDCALL snd_jet_whine(struct snd_rec *sub, struct snd_jet_ctx *ctx)
{
    if (ctx->flag == 1) {
        H_VOICE_VOLUME(sub->voice, 0);
        return;
    }
    sub->fader = snd_get_fader(1);
    if (ctx->whine == 1) {
        if (sub->playing == 0) {
            snd_play(sub, 1);
            sub->playing = 1;
        }
        snd_volume(sub, ctx->vol2);
    } else if (sub->playing == 1) {
        snd_stop(sub);
    }
}

/* .text+0x3600 -- flagged 8: each one a tenth lower in rate than the last */
static void FS98_STDCALL snd_jet_stack(struct snd_rec *sub, struct snd_jet_ctx *ctx)
{
    double r;
    if (ctx->flag == 1) {
        H_VOICE_VOLUME(sub->voice, 0);
        return;
    }
    sub->fader = snd_get_fader(1);
    snd_volume(sub, ctx->vol);
    r = (double)((long double)ctx->rate - K_0F0);
    ctx->rate = r;
    H_VOICE_FREQ(sub->voice, r);
}

/* .text+0x3680 -- one jet sub-sound */
static int32_t FS98_STDCALL snd_jet_cb(void *elem, void *ctx_)
{
    struct snd_rec *sub = elem;
    struct snd_jet_ctx *ctx = ctx_;

    if (sub == NULL)
        return -1;
    if (ctx == NULL)
        return (int32_t)(uintptr_t)sub;
    if (snd_muted(sub->viewpoint, ctx->kind)) {
        if (sub->playing != 0)
            snd_stop(sub);
        return 0;
    }
    if (sub->playing == 0) {
        H_VOICE_VOLUME(sub->voice, 0);
        snd_play(sub, 1);
        sub->playing = 1;
    }
    if (sub->flags & 2)
        snd_jet_core(sub, ctx);
    else if (sub->flags & 4)
        snd_jet_whine(sub, ctx);
    else if (sub->flags & 8)
        snd_jet_stack(sub, ctx);
    return 0;
}

/* .text+0x3750 -- a jet's frame.  The whine builds while any engine's word
 * at +0x88c/+0x924/+0x9bc/+0xa54 is negative, by one a frame up to nine,
 * and fades by one a frame when none is. */
static void FS98_STDCALL snd_jet(struct snd_rec *eng)
{
    struct snd_jet_ctx ctx;
    double val;
    long double x;
    int16_t a, b, c, d, m;
    int32_t vol, w;

    ctx.kind = GP(0x11a0) != NULL ? P32(GP(0x11a0), 0x14) : 0;
    ctx.flag = 0;
    val = (double)((long double)(int32_t)snd_rpm() * K_0F8);
    if (FC0((long double)val, K_100))
        ctx.flag = 1;
    ctx.val = val;
    if (ctx.kind != 0 && ctx.kind != 1)
        val = (double)(snd_doppler() * val);
    x = K_110 - (long double)val * K_108;
    if (x > K_118)
        x = K_118;
    if (FC0(x, K_110))
        x = K_110;
    ctx.rate = (double)x;

    a = GS16(0x9bc);
    b = GS16(0xa54);
    c = GS16(0x924);
    d = GS16(0x88c);
    m = a < b ? a : b;
    if (c < m)
        m = c;
    if (d < m)
        m = d;
    x = S_WHINE;
    if (m < 0) {
        S_WHINE = (double)(x - K_120);
        if ((long double)(x - K_120) > K_128)
            S_WHINE = 9.0;
    } else {
        S_WHINE = (double)(x - K_130);
        if (FC0(x - K_130, K_138))
            S_WHINE = 0.0;
    }

    if (ctx.kind == 0 || ctx.kind == 1)
        x = K_128;
    else
        x = snd_range_db();
    vol = (int16_t)snd_ftol(K_140 - x * K_0D8);
    ctx.vol = vol;
    if (!FC0((long double)val, K_148)) {
        vol += 400;
        ctx.vol = vol;
    } else if ((long double)val > K_110) {
        vol += (int16_t)snd_ftol(snd_log10((long double)val * K_150) * S_WHINE_SCALE);
        ctx.vol = vol;
    }
    if ((long double)S_WHINE > K_138) {
        w = (int16_t)snd_ftol((long double)S_WHINE * K_0D8);
        ctx.whine = 1;
        ctx.vol2 = vol + w;
        ctx.vol = vol - w;
        if (ctx.vol2 > 10000)
            ctx.vol2 = 10000;
    } else {
        ctx.whine = 0;
        ctx.vol2 = 0;
    }
    H_LIST_WALK(eng->links, &ctx, snd_jet_cb);
}

/* .text+0x3a10 */
static int32_t FS98_STDCALL snd_stop_sub_cb(void *elem, void *ctx)
{
    (void)ctx;
    if (elem != NULL)
        snd_stop(elem);
    return 0;
}

/* .text+0x3a30 -- walk sound `id`'s sub-sounds */
static int32_t FS98_STDCALL snd_walk_links(int32_t id, snd_walk_fn fn)
{
    struct snd_rec *r = H_LIST_FIND(S_SOUNDS, 0, &id, 4, 0);
    if (r == NULL || r->links == NULL)
        return -1;
    return H_LIST_WALK(r->links, NULL, fn);
}

/* .text+0x39e0 -- the engine sound for this view kind, stopped */
static void snd_engines_stop(void)
{
    uint8_t v = G8(0x609);
    if (v == 2)
        return;
    snd_walk_links(v != 0 ? 3 : 1, snd_stop_sub_cb);
}

/* .text+0x3a80 */
static void snd_engines_tick(void)
{
    int32_t id;
    struct snd_rec *r;
    switch (G8(0x609)) {
    case 0:
        id = 1;
        r = H_LIST_FIND(S_SOUNDS, 0, &id, 4, 0);
        if (r != NULL && r->links != NULL)
            snd_prop(r);
        break;
    case 1:
    case 3:
        id = 3;
        r = H_LIST_FIND(S_SOUNDS, 0, &id, 4, 0);
        if (r != NULL && r->links != NULL)
            snd_jet(r);
        break;
    }
}

/* ---- the module ------------------------------------------------------------------- */

/* .text+0x3b10 -- +0x04 */
static void snd_init(void)
{
    S_WHINE_SCALE = (double)(K_160 / snd_log10(K_158));
}

/* .text+0x3b30 -- +0x08 */
static void snd_deinit(void)
{
}

struct snd_image snd_image = {
    {
        { FS98_MOD_HOST,     0 },
        { FS98_MOD_GLOBAL,   0 },
        { FS98_MOD_ACONTAIN, 0 },
        { FS98_MOD_CHAIN,    0 },
        { FS98_MOD_UTIL,     0 },
        { 0, 0 }
    },
    {
        FS98_MOD_SOUND, snd_init, snd_deinit, 0, 0, 0,
        snd_start, snd_stop_all, snd_frame, snd_event, snd_wind,
        snd_set_fader, snd_get_fader, snd_set_faders, snd_get_faders,
        snd_save_faders, snd_load_faders, snd_markers
    },
    {
#include "sound_data.inc"
    }
};

/* The two exports are the image's first 0x30 bytes and the 0x48 at +0x30,
 * where retail has them. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _snd_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _snd_image + 0x30");

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
