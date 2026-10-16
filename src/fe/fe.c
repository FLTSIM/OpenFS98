/* FE.DLL -- module id 18: the scenery file engine.
 *
 * The sim's scenery database.  G3D gets its scenery stream from it, MAIN its
 * navaid and ground lookups, DYNAMIC its traffic programs, HG2D its textures
 * and FACIL its BGL sections; it also owns the Scenery Library and the
 * performance boosters (CD scenery cached to disk).  docs/preflight.md
 * (round 4) has the survey and docs/modules/fe.md the walk through.
 *
 * Retail is two programs in one module: hand-written assembly from .text+0x20
 * to +0x5792, and MSVC C from +0x57a0.  Both are transliterated here; the
 * comments carry retail's addresses.  Before calling into it, know that:
 *
 *   - Everything lives in one .data image laid out exactly as retail's,
 *     ImportTable at +0x00 and Linkage at +0x48, and the code addresses its
 *     state by retail's offsets -- including dwords at odd addresses.
 *   - Memory is read and written through volatile accessors on 32-bit
 *     addresses, one access per instruction, in retail's order: the scenery
 *     files are data nothing checks.
 *   - Shifts are the processor's: counts masked to five bits, and a 16-bit
 *     shift by 16..31 empties the register (see shl16/shr16).
 *   - Many slots take 16-bit arguments, which callers push as
 *     `sub esp,2; push cx`.  A uint16_t parameter reads exactly that half of
 *     its slot, so they are plain stdcall functions.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define FE_IMAGE_SIZE 0x62e0        /* .data's virtual size */
#define FE_DATA_START 0xd4          /* the end of the Linkage */
#define FE_NSLOTS     29            /* +0x18..+0x88 */

typedef void (*fe_fn)(void);

struct fe_linkage {
    uint32_t id;
    void   (*init)(void);
    void   (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;           /* 0xd0000000 in retail */
    void    *dialogs;               /* +0x14: none */
    fe_fn    slots[FE_NSLOTS];      /* +0x18..+0x88 */
};

struct fe_image {
    struct fs98_import import[9];       /* +0x000 ImportTable */
    struct fe_linkage linkage;          /* +0x048 Linkage */
    uint8_t data[FE_IMAGE_SIZE - FE_DATA_START];
};

extern struct fe_image fe_image;

typedef char (*fe_layout_check)[sizeof(struct fe_image) == FE_IMAGE_SIZE &&
                                sizeof(struct fe_linkage) == 0x8c ? 1 : -1];

/* ---- memory, as the registers address it ------------------------------------ */

typedef volatile uint8_t  __attribute__((may_alias)) fe_v8;
typedef volatile uint16_t __attribute__((may_alias)) fe_v16;
typedef volatile uint32_t __attribute__((may_alias)) fe_v32;

#define M8(a)   (*(fe_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(fe_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(fe_v32 *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

/* The image, by retail's offsets (0x206cexxx is D + 0xxxx). */
#define D           ADDR(&fe_image)
#define T_BANDS     (D + 0xf20)     /* latitude band edges, 32-bit, rising */

/* ---- arithmetic as the processor does it -------------------------------------- */

/* shl/shr r16, cl: the count is masked to five bits, and 16..31 shift every
 * bit out. */
static uint16_t shl16(uint16_t v, unsigned cl)
{
    cl &= 31;
    return cl >= 16 ? 0 : (uint16_t)(v << cl);
}

static uint16_t shr16(uint16_t v, unsigned cl)
{
    cl &= 31;
    return cl >= 16 ? 0 : (uint16_t)(v >> cl);
}

/* sar r32, cl and sar r16, cl: the count masked to five bits; past 15 a
 * 16-bit one leaves only copies of the sign. */
static uint32_t sar32(uint32_t v, unsigned cl)
{
    return (uint32_t)((int32_t)v >> (cl & 31));
}

static uint16_t sar16(uint16_t v, unsigned cl)
{
    cl &= 31;
    return (uint16_t)((int16_t)v >> (cl >= 16 ? 15 : cl));
}

/* div r/m32 and div r/m16: the processor's own, so a zero divisor or a
 * quotient too wide faults where retail's does. */
static uint32_t udiv32(uint32_t hi, uint32_t lo, uint32_t d)
{
    uint32_t q, r;
    __asm__ volatile("divl %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    return q;
}

/* idiv r/m32 */
static uint32_t sdiv32(uint32_t hi, uint32_t lo, uint32_t d)
{
    uint32_t q, r;
    __asm__ volatile("idivl %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    return q;
}

/* imul r/m32, folded as retail folds it: the middle 32 bits of the 64-bit
 * product (shl edx,16 / shr eax,16 / mov dx,ax). */
static uint32_t imul_mid(uint32_t a, uint32_t b)
{
    uint64_t p = (uint64_t)((int64_t)(int32_t)a * (int32_t)b);
    return (uint32_t)(p >> 16);
}

static uint16_t udiv16(uint16_t hi, uint16_t lo, uint16_t d, uint16_t *rem)
{
    uint16_t q, r;
    __asm__ volatile("divw %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    if (rem)
        *rem = r;
    return q;
}

/* ---- the grid --------------------------------------------------------------------- */

/* .text+0x20eb -- +0x60: the band a latitude falls in at `level`.  The
 * latitude is the 32-bit integer part of a 48-bit position (*p), scaled by
 * 2^7 and found in the table of band edges; each level past 8 halves the band
 * once more.  A southern latitude is found by its magnitude and the band
 * complemented. */
static uint32_t FS98_STDCALL fe_lat_band(uint32_t p, uint16_t level)
{
    uint32_t ecx, ebx, esi, eax, half;
    int16_t di;
    int south;

    ecx = M32(p);
    south = (int32_t)ecx < 0;                       /* or ecx,ecx / pushfw */
    if (south)
        ecx = 0u - ecx;
    ecx <<= 7;
    ebx = 0;
    esi = T_BANDS;
    while ((int32_t)(M32(esi) - ecx) < 0) {         /* sub eax,ecx / jns */
        ebx++;
        esi += 4;
    }
    di = (int16_t)(level - 8);
    esi -= 4;
    half = M32(esi + 4) - M32(esi);
    eax = M32(esi);
    while (--di >= 0) {                             /* dec di / js */
        ebx += ebx;
        half >>= 1;
        eax += half;
        if ((int32_t)(ecx - eax) < 0)
            eax -= half;
        else
            ebx++;
    }
    if (south)
        ebx = ~ebx;
    return ebx;
}

/* .text+0x2157 -- +0x64: the other way, the latitude at the south edge of
 * `band` at `level`, into the 48-bit position at `out` (+2 the fraction word,
 * +4 the integer; +0 untouched).  Interpolates between the table's edges at
 * 1/2^(23-level) of a band, then scales by 2^9.  The `sbb` that takes the
 * band's height reads the carry the `xor ecx,ecx` before it cleared. */
static void FS98_STDCALL fe_band_lat(uint16_t band, uint16_t level, uint32_t out)
{
    uint16_t ax, dx16;
    uint32_t ebx, edx, ecx, eax, cf;
    uint64_t m;
    unsigned cl;

    ax = band;
    if ((int16_t)ax < 0)
        ax = (uint16_t)(0u - ax);
    cl = (uint8_t)(level - 8);
    ebx = shr16(ax, cl);
    edx = ebx;
    ebx = ebx * 4;
    edx = (edx & 0xffff0000u) | shl16((uint16_t)edx, cl);
    ax = (uint16_t)(ax - (uint16_t)edx);
    cl = (uint8_t)(0x17 - level);
    ax = shl16(ax, cl);
    ecx = ax;
    eax = M32(T_BANDS + ebx) - M32(T_BANDS + ebx - 4);
    m = (uint64_t)eax * ecx;
    m <<= 1;                                        /* add eax,eax / adc edx,edx */
    eax = (uint32_t)m;
    edx = (uint32_t)(m >> 32);
    ecx = (uint32_t)M16(T_BANDS + ebx - 4) << 16;
    cf = (uint32_t)(eax + ecx) < eax;
    eax += ecx;
    dx16 = (uint16_t)((uint16_t)edx + M16(T_BANDS + ebx - 2) + cf);   /* adc dx,... */
    edx = (edx & 0xffff0000u) | dx16;
    edx = (edx << 9) | (eax >> 23);                 /* shld edx,eax,9 */
    eax <<= 9;
    if ((int16_t)band < 0) {                        /* neg eax / not edx / sbb edx,-1 */
        cf = eax != 0;
        eax = 0u - eax;
        edx = ~edx + 1 - cf;
    }
    M32(out + 4) = edx;
    M16(out + 2) = (uint16_t)(eax >> 16);
}

/* .text+0x21e9 -- +0x68: the band a longitude falls in at `level`: the top
 * `level` bits of the 48-bit position's fraction-and-low word at p+2. */
static uint32_t FS98_STDCALL fe_lon_band(uint32_t p, uint16_t level)
{
    return shr16(M16(p + 2), (uint8_t)(0x10 - level));
}

/* .text+0x220a -- +0x6c: the other way: the band's west edge, into the
 * 48-bit position at `out` (+2 cleared, +4 the integer; +0 untouched). */
static void FS98_STDCALL fe_band_lon(uint16_t band, uint16_t level, uint32_t out)
{
    M32(out + 4) = (uint32_t)shl16(band, (uint8_t)(0x10 - level)) << 16;
    M16(out + 2) = 0;
}

/* ---- imports ---------------------------------------------------------------------- */

#define G ADDR(fe_image.import[1].link)

/* Every service is called with 32-bit arguments, pointers among them, so one
 * shape per arity serves; the prototype each was read from is beside its use. */
typedef uint32_t (FS98_STDCALL *fe_f0)(void);
typedef uint32_t (FS98_STDCALL *fe_f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *fe_f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *fe_f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *fe_f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *fe_f5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *fe_f8)(uint32_t, uint32_t, uint32_t, uint32_t,
                                       uint32_t, uint32_t, uint32_t, uint32_t);

#define CALL(i, off, fn) (*(fn *)((uint8_t *)fe_image.import[i].link + (off)))
#define HOST(off, fn)    CALL(0, off, fn)

/* The host.  The "fatal" four show a message box and exit the process when the
 * operation fails; the others return -1. */
#define H_DIALOG     HOST(0x10c, fe_f2)  /* (id, param) */
#define H_BASEDIR    HOST(0x130, fe_f1)  /* (out): the host's directory */
#define H_CLOSE      HOST(0x13c, fe_f1)  /* (file), by value */
#define H_CLOSE_F    HOST(0x140, fe_f1)  /* (file), fatal */
#define H_FIND_CLOSE HOST(0x15c, fe_f1)  /* (&find) */
#define H_FIND_NEXT  HOST(0x160, fe_f2)  /* (find, entry) */
#define H_FIND_FIRST HOST(0x164, fe_f4)  /* (&find, dir, pattern, ext) */
#define H_OPEN       HOST(0x17c, fe_f4)  /* (&file, path, ext, mode) */
#define H_OPEN_F     HOST(0x180, fe_f4)  /* the same, fatal */
#define H_JOIN       HOST(0x184, fe_f4)  /* (out, drive, dir, name) */
#define H_SPLIT      HOST(0x188, fe_f5)  /* (path, drive, dir, name, &ext) */
#define H_TELL       HOST(0x190, fe_f2)  /* (file, &pos) */
#define H_SEEK       HOST(0x194, fe_f3)  /* (file, offset, whence) */
#define H_SEEK_F     HOST(0x198, fe_f3)  /* the same, fatal */
#define H_READ       HOST(0x19c, fe_f4)  /* (file, buf, n, &got) */
#define H_READ_F     HOST(0x1a0, fe_f4)  /* the same, fatal */
#define H_SIZE       HOST(0x1a4, fe_f2)  /* (file, &size) */
#define H_ALLOC      HOST(0x20c, fe_f4)  /* (flags, size, &p, zero) */
#define H_REALLOC    HOST(0x210, fe_f3)  /* (flags, size, &p) */
#define H_FREE       HOST(0x214, fe_f1)  /* (&p) */
#define H_MEMSET     HOST(0x218, fe_f3)  /* (dst, byte, n) */
#define H_MEMMOVE    HOST(0x21c, fe_f3)  /* (dst, src, n) */
#define H_LOADSTRING HOST(0x2a0, fe_f3)  /* (id, buf, size) */
#define H_STRCPY     HOST(0x2a8, fe_f2)  /* (dst, src) */
#define H_STRLEN     HOST(0x2b8, fe_f1)
#define H_STRCMP     HOST(0x2bc, fe_f2)
#define H_STRICMP    HOST(0x2c0, fe_f2)
#define H_FIT        HOST(0x2c4, fe_f3)  /* (dst, src, n): a string fitted to n */

#define BOOSTER_ADD  CALL(2, 0x18, fe_f2)   /* (area record, index) */
#define BOOSTER_DROP CALL(2, 0x1c, fe_f1)   /* (area record) */
#define CHAIN_ADD    CALL(3, 0x18, fe_f2)   /* (list, callback) */
#define CHAIN_REMOVE CALL(3, 0x1c, fe_f2)
#define FACIL_INDEX  CALL(4, 0x2c, fe_f0)
#define UTIL_SQRT64  CALL(7, 0x18, fe_f1)   /* (&v64) */
#define UTIL_HYPOT   CALL(7, 0x28, fe_f2)   /* (int16 a, int16 b) */
#define UTIL_TO_LON  CALL(7, 0x44, fe_f3)   /* (in48, pos48, out48) */
#define UTIL_TO_M    CALL(7, 0x48, fe_f3)   /* (in48, pos48, out48) */
#define UTIL_UNPACK  CALL(7, 0x60, fe_f2)   /* (src, dst) */
#define UTIL_ALERT   CALL(7, 0xa4, fe_f8)   /* (flags, tag, p1..p5, lines) */

#define ALLOC(size, pp, zero) H_ALLOC(1, (size), (pp), (zero))
#define REALLOC(size, pp)     H_REALLOC(1, (size), (pp))
#define FREE(pp)              H_FREE(pp)

/* The critical section every asm slot holds, because BOOSTER calls +0x70 and
 * +0x74 from its own thread. */
#define CS ((LPCRITICAL_SECTION)(uintptr_t)(D + 0x1734))

/* ---- the asm half's state ------------------------------------------------------- */

/* The image, by retail's offsets (0x206cexxx is D + 0xxxx).  These are the
 * ones read in more than one routine; the rest appear as D + offset. */
#define S_FILE      (D + 0x119)     /* the open BGL */
#define S_GOT       (D + 0x11d)     /* the fatal read's count */
#define S_AREA      (D + 0x2b5)     /* the iteration: area index */
#define S_INDEX     (D + 0x2b9)     /*   and file index within it */
#define S_AREAS     (D + 0x2d1)     /* 0x10 per area: files, bytes, count, path */
#define S_AREAS_CAP (D + 0x2d5)
#define S_NAREAS    (D + 0x2d9)
#define S_REC       (D + 0x2dd)     /* the current file's 0x1ae-byte record */
#define S_ENTRY     (D + 0x2e1)     /* the current area */
#define S_SEARCH    (D + 0x2e5)     /* the lookups' state, a word */
#define S_WORLD     (D + 0x2e7)     /* the world file */
#define S_BUF       (D + 0xace)     /* where records are read to */
#define S_BASE      (D + 0xa33)     /* the current section's file offset */

/* The registers the assembly keeps across routines.  A routine that sets one
 * for its caller, or reads one its caller set, does it through these. */
static uint32_t eax, ebx, ecx, edx, esi, edi;

/* Retail leaves a saved-register push on the stack when +0x18 passes through
 * state 4.  The public entry shim uses these to reproduce the resulting
 * EBX/ESI/EDI rotation without corrupting the C implementation's frame. */
uint32_t fe_scene_in_ebx;
uint32_t fe_scene_in_esi;
uint32_t fe_scene_in_edi;
uint32_t fe_scene_out_edi;
uint32_t fe_scene_shifted;

/* A slot's prologue and epilogue push and pop these three, so a slot called
 * from inside the module leaves its caller's. */
#define SAVE_REGS    uint32_t saved_ebx = ebx, saved_esi = esi, saved_edi = edi
#define RESTORE_REGS (ebx = saved_ebx, esi = saved_esi, edi = saved_edi)

/* +0x1a0 and +0x198 on the open BGL. */
#define SEEK_FILE(off)       H_SEEK_F(M32(S_FILE), (off), 0)
#define READ_FILE(buf, n)    H_READ_F(M32(S_FILE), (buf), (n), S_GOT)

/* ---- the world file and the areas ------------------------------------------------- */

/* .text+0x183 -- close the open BGL, fatally. */
static void close_file(void)
{
    eax = M32(S_FILE);
    if (eax != 0) {
        eax = H_CLOSE_F(M32(S_FILE));
        M32(S_FILE) = 0;
    }
}

/* .text+0x1c2 -- free the search grid. */
static void free_grid(void)
{
    eax = M32(D + 0xa5d);
    if (eax != 0) {
        eax = FREE(D + 0xa5d);
        M32(D + 0xa5d) = 0;
    }
}

/* .text+0x714 -- one byte of the world file.  Retail reads into a stack byte
 * and returns it whether or not the read got one; past the end of the file
 * that is what the slot last held, which was the previous byte read at the
 * same depth.  A static keeps that. */
static uint8_t world_byte_last;
static uint32_t world_byte(void)
{
    H_READ_F(M32(S_WORLD), ADDR(&world_byte_last), 1, S_GOT);
    return world_byte_last;
}

/* .text+0x6d5 -- seek the world file. */
static void world_seek(uint16_t off)
{
    H_SEEK_F(M32(S_WORLD), (uint32_t)(int32_t)(int16_t)off, 0);
}

/* .text+0x75d -- the world file's word at edx, into cx. */
static uint16_t world_word_last;
static void world_word(void)
{
    H_SEEK_F(M32(S_WORLD), edx, 0);
    H_READ_F(M32(S_WORLD), ADDR(&world_word_last), 2, S_GOT);
    ecx = (ecx & 0xffff0000u) | world_word_last;
}

/* .text+0x7cd -- insert the string at esi into the one at +0xb96, at di,
 * a character at a time, each pushing the rest along. */
static void insert_string(void)
{
    uint8_t al;

    for (;;) {
        al = M8(esi);
        if (al == 0)
            return;
        ebx = 0;
        do {
            uint8_t old = M8(D + 0xb96 + ebx + edi);
            M8(D + 0xb96 + ebx + edi) = al;
            al = old;
            ebx = (ebx & 0xffff0000u) | (uint16_t)(ebx + 1);
        } while (al != 0);
        M8(D + 0xb96 + ebx + edi) = al;
        edi = (edi & 0xffff0000u) | (uint16_t)(edi + 1);
        esi++;
    }
}

/* .text+0x7ef -- drop the first character of the string at edi. */
static void drop_char(void)
{
    uint8_t al;

    esi = edi;
    do {
        al = M8(esi + 1);
        M8(esi) = al;
        esi++;
    } while (al != 0);
}

static uint32_t FS98_STDCALL fe_register(uint32_t area, uint32_t name);

/* .text+0x628 -- register every BGL in the directory at +0xb96 matching the
 * name at +0x61e, through +0x70, as area S_AREA's. */
static void find_files(void)
{
    if (H_FIND_FIRST(D + 0x1a1, D + 0xb96, D + 0x61e, 1) != 0)
        return;
    while (H_FIND_NEXT(M32(D + 0x1a1), D + 0x1a5) == 0)
        fe_register(M32(S_AREA), D + 0x1a5);
    H_FIND_CLOSE(D + 0x1a1);
}

/* .text+0x200 -- read the world file's area list from offset cx: for each
 * area, a 1 byte, a flag, a path (an optional "=" prefix dropped; "\=\" means
 * the host's directory), and a skipped string.  Each path is split, its
 * SCENERY directory found, its BGLs registered.  With no list, the host's own
 * SCENERY directory is the one area.  Stops at a byte that is not 1. */
static void read_areas(void)
{
    uint32_t al;

    if ((uint16_t)ecx == 0) {
        M8(D + 0xb96) = 0;
        H_BASEDIR(D + 0x71e);
        H_LOADSTRING(0x9003, D + 0x81e, 0x100);
        H_JOIN(D + 0x71e, 0, D + 0x71e, D + 0x81e);
        M32(D + 0x91e) = 1;
        M8(D + 0x61e) = '*';
        M8(D + 0x61f) = 0;
        edi = 0;
        esi = D + 0x71e;
        insert_string();
        goto add_path;
    }
    edx = (uint16_t)ecx;
    H_SEEK_F(M32(S_WORLD), edx, 0);
    for (;;) {
        if (world_byte() != 1)
            return;
        world_byte();
        M8(D + 0x309) = (uint8_t)world_byte();
        edi = 0;
        al = world_byte();
        if (al == 0x3d) {
            world_byte();
            al = world_byte();
        }
        for (;;) {
            M8(D + 0xb96 + edi) = (uint8_t)al;
            edi++;
            if ((uint8_t)al == 0)
                break;
            al = world_byte();
        }
        while ((uint8_t)world_byte() != 0)
            ;
        edi = D + 0xb96;
        if (M16(edi) == 0x3d5c) {                  /* "\=": the host's directory */
            drop_char();
            drop_char();
            drop_char();
            H_BASEDIR(D + 0x71e);
            H_JOIN(D + 0x71e, 0, D + 0x71e, D + 0xb96);
            H_JOIN(D + 0xb96, 0, 0, D + 0x71e);
        }
        H_SPLIT(D + 0xb96, D + 0x41e, D + 0x51e, D + 0x61e, D + 0x91e);
        H_JOIN(D + 0x71e, D + 0x41e, D + 0x51e, D + 0x81e);
        H_JOIN(D + 0x31a, D + 0x41e, D + 0x51e, 0);
        H_STRCPY(D + 0xb96, D + 0x71e);
        M32(S_NAREAS)++;
        if (M8(D + 0x309) == 0)
            goto next;
add_path:
        esi = M32(S_AREAS) + 0x10 * M32(S_AREA) + 0xc;
        eax = H_STRLEN(D + 0x31a) + 1;
        ALLOC(eax, esi, 0);
        H_STRCPY(M32(esi), D + 0x31a);
        find_files();
        if (M32(G + 0x390) != 2)
            return;
next:
        M32(S_AREA)++;
        if (0x10 * M32(S_AREA) >= M32(S_AREAS_CAP)) {
            M32(S_AREAS_CAP) += 0xa0;
            REALLOC(M32(S_AREAS_CAP), S_AREAS);
            H_MEMSET(M32(S_AREAS) + M32(S_AREAS_CAP) - 0xa0, 0, 0xa0);
        }
    }
}

static void build_boosters(uint16_t n);
static void rebuild_boosters(uint16_t n);

/* .text+0x895 -- open the world file if it is not, and its word at +0xc (the
 * booster list's offset) into cx.  Its name is GLOBAL +0x446, in the host's
 * directory; edi is GLOBAL on entry. */
static void open_world(void)
{
    if (M32(S_WORLD) == 0) {
        H_BASEDIR(D + 0x71e);
        H_LOADSTRING(0x9003, D + 0x81e, 0x100);
        H_JOIN(D + 0x71e, 0, D + 0x71e, edi + 0x446);
        H_OPEN_F(S_WORLD, D + 0x71e, 0, 0);
    }
    edx = 0xc;
    world_word();
}

/* .text+0x80f -- GLOBAL +0x444 bit 0x10: the boosters, read again. */
static void reload_boosters(void)
{
    edi = G;
    if (M32(edi + 0x390) >= 2) {
        if (M32(D + 0x1750) != 0) {
            open_world();
            rebuild_boosters((uint16_t)ecx);
        } else {
            open_world();
            if ((uint16_t)ecx != 0) {
                build_boosters((uint16_t)ecx);
                M32(D + 0x1750) = eax;
            }
        }
        H_CLOSE_F(M32(S_WORLD));
        M32(S_WORLD) = 0;
    }
    edi = G;
    M16(edi + 0x444) &= 0xffef;
}

/* .text+0xbae -- free the areas and everything registered in them. */
static void free_areas(void)
{
    edi = M32(S_AREAS);
    if (edi != 0) {
        esi = M32(S_NAREAS);
        if (esi != 0) {
            do {
                if (M32(edi) != 0)
                    FREE(edi);
                if (M32(edi + 0xc) != 0)
                    FREE(edi + 0xc);
                edi += 0x10;
            } while (--esi != 0);
        }
        FREE(S_AREAS);
        M32(S_AREAS) = 0;
        M32(S_AREAS_CAP) = 0;
        M32(S_NAREAS) = 0;
    }
    if (M32(D + 0x9fb) != 0) {
        FREE(D + 0x9fb);
        M32(D + 0x9fb) = 0;
        M16(D + 0x9ff) = 0;
    }
    M32(D + 0x5ca0) = 0;
}

/* .text+0x976 -- read the world file afresh: its header, the accepted BGL
 * versions into GLOBAL +0x424, the areas; then FACIL rebuilds its index. */
static void rebuild_areas(void)
{
    free_areas();
    edi = G;
    M16(edi + 0x444) &= 0xffde;
    ALLOC(0x1e0, S_AREAS, 1);
    M32(S_AREA) = 0;
    M32(S_INDEX) = 0;
    M32(S_AREAS_CAP) = 0x1e0;
    H_BASEDIR(D + 0x71e);
    H_LOADSTRING(0x9003, D + 0x81e, 0x100);
    H_JOIN(D + 0x71e, 0, D + 0x71e, edi + 0x446);
    H_OPEN_F(S_WORLD, D + 0x71e, 0, 0);
    H_READ_F(M32(S_WORLD), D + 0x2eb, 0x1e, 0);
    ebx = M16(D + 0x2f3);
    H_SEEK_F(M32(S_WORLD), ebx, 0);
    edi = G;
    H_READ_F(M32(S_WORLD), edi + 0x424, 0x20, 0);
    ecx = (ecx & 0xffff0000u) | M16(D + 0x2f5);
    read_areas();
    H_CLOSE_F(M32(S_WORLD));
    M32(S_WORLD) = 0;
    eax = FACIL_INDEX();
}

/* .text+0xcc1 -- every lookup starts here: the BGL closed, the boosters and
 * areas brought up to date with GLOBAL +0x444, and the iteration rewound. */
static void begin(void)
{
    close_file();
    edi = G;
    if (M16(edi + 0x444) & 0x10)
        reload_boosters();
    if (M32(S_NAREAS) == 0 || (edi = G, M16(edi + 0x444) & 0x21))
        rebuild_areas();
    M32(S_INDEX) = 0xffffffffu;
    M32(S_AREA) = 0;
    M16(S_SEARCH) = 0;
    M32(D + 0x2cd) = 1;
}

/* .text+0x32b3 -- step to the next registered file: edi its area, esi its
 * record.  Carry when there are no more. */
static int next_file(void)
{
    eax = M32(S_AREA);
    if (eax >= M32(S_NAREAS))
        return 1;
    edi = 0x10 * eax + M32(S_AREAS);
    M32(S_INDEX)++;
    for (;;) {
        esi = M32(edi + 8);
        if (M32(S_INDEX) < esi) {
            esi = M32(edi);
            break;
        }
        esi = M32(S_NAREAS);
        M32(S_AREA)++;
        if (M32(S_AREA) == esi)
            return 1;
        M32(S_INDEX) = 0;
        edi += 0x10;
    }
    esi += 0x1ae * M32(S_INDEX);
    M32(S_ENTRY) = edi;
    M32(S_REC) = esi;
    M32(D + 0x2bd) = M32(edi + 0xc);
    return 0;
}

/* .text+0x330b -- the same without stepping. */
static int cur_file(void)
{
    eax = M32(S_AREA);
    if (eax >= M32(S_NAREAS))
        return 1;
    edi = 0x10 * eax + M32(S_AREAS);
    eax = M32(edi + 8);
    esi = M32(edi);
    if (M32(S_INDEX) >= eax)
        return 1;
    esi += 0x1ae * M32(S_INDEX);
    M32(S_ENTRY) = edi;
    M32(S_REC) = esi;
    M32(D + 0x2bd) = M32(edi + 0xc);
    return 0;
}

/* .text+0xd23 -- open the current file: <area>\SCENERY\<name>.  Retail does
 * not look at cur_file's carry, so past the end it opens whatever edi and esi
 * already named. */
static void open_file(void)
{
    cur_file();
    H_JOIN(D + 0x71e, 0, M32(edi + 0xc), D + 0x81e);
    H_JOIN(D + 0x71e, 0, D + 0x71e, esi);
    H_OPEN_F(S_FILE, D + 0x71e, 1, 0);
    esi = M32(S_REC);
}

/* ---- registering files: +0x70, +0x74 -------------------------------------------- */

/* .text+0x2359 -- +0x70: register the BGL `name` in area `area`.  Its 0xa8-
 * byte header is read; a version not in GLOBAL +0x424 is refused, as are
 * 1000..1999 unless +0xf1a is set.  The record is the name, the header from
 * +2 (0x74 bytes), 0x28 more when the header carries 0x87654321, and what the
 * file's extra sections at header +0x72 hold: a type-3 one is copied to
 * +0x19c, and type-4 ones are listed at +0x9fb -- or would be: the list is
 * only allocated while its pointer is negative, and each entry is written at
 * 13 * count with the list's base never added, so a type-4 section faults.
 * BOOSTER calls this from its thread; the file it had open is put back. */
static uint32_t FS98_STDCALL fe_register(uint32_t area, uint32_t name)
{
    uint32_t saved, entry, rec;
    uint16_t ax;
    SAVE_REGS;

    EnterCriticalSection(CS);
    saved = M32(S_FILE);
    ebx = area;
    if (ebx >= M32(S_NAREAS)) {
        eax = 0;
        goto out;
    }
    entry = 0x10 * ebx + M32(S_AREAS);
    H_JOIN(D + 0x71e, 0, M32(entry + 0xc), D + 0x81e);
    H_JOIN(D + 0x71e, 0, D + 0x71e, name);
    H_OPEN_F(S_FILE, D + 0x71e, 1, 0);
    READ_FILE(S_BUF, 0xa8);
    ax = M16(S_BUF);
    edi = G;
    ebx = edi + 0x424;
    for (;;) {
        uint16_t cx = M16(ebx);
        if (cx == 0)
            goto refuse;
        if (ax == cx)
            break;
        ebx = (ebx & 0xffff0000u) | (uint16_t)(ebx + 2);
    }
    if (M16(D + 0xf1a) == 0 && ax >= 0x3e8 && ax < 0x7d0)
        goto refuse;
    edi = entry;
    if (M32(edi) == 0) {
        ALLOC(0x866, edi, 1);
        M32(edi + 4) = 0x866;
    }
    esi = 0x1ae * M32(edi + 8);
    if (esi >= M32(edi + 4)) {
        ebx = M32(edi + 4) + 0x866;
        M32(edi + 4) = ebx;
        REALLOC(ebx, edi);
        H_MEMSET(M32(edi) + M32(edi + 4) - 0x866, 0, 0x866);
    }
    esi += M32(edi);
    M32(edi + 8)++;
    rec = esi;
    H_STRCPY(rec, name);
    H_MEMMOVE(rec + 0x100, D + 0xad0, 0x74);
    if (M32(D + 0xb44) == 0x87654321u) {
        H_MEMMOVE(rec + 0x174, D + 0xb4e, 0x28);
        M32(D + 0x5ca0)++;
    }
    esi = rec;
    M16(esi + 0x19c) = 0;
    edx = M32(esi + 0x170);
    if (edx != 0) {
        SEEK_FILE(edx);
        READ_FILE(S_BUF, 1);
        if (M8(S_BUF) == 3) {
            READ_FILE(S_BUF, 0x12);
            for (ebx = 0x11; (int32_t)ebx >= 0; ebx--)
                M8(ebx + esi + 0x19c) = M8(S_BUF + ebx);
            READ_FILE(S_BUF, 1);
        }
        while (M8(S_BUF) == 4) {
            READ_FILE(S_BUF, 0xc);
            if ((int32_t)M32(D + 0x9fb) < 0) {
                ALLOC(0xd0, D + 0x9fb, 0);
                M16(D + 0x9ff) = 0;
                M32(D + 0xa01) = 0xd0;
            }
            esi = (uint16_t)(0xd * M16(D + 0x9ff));
            if ((int32_t)esi >= (int32_t)M32(D + 0xa01)) {
                eax = 0xd0 + M32(D + 0xa01);
                REALLOC(eax, D + 0x9fb);
                M32(D + 0xa01) = eax;
            }
            M8(esi) = 4;
            for (ebx = 0xc; (int16_t)ebx >= 0; ebx = (ebx & 0xffff0000u) | (uint16_t)(ebx - 1))
                M8(ebx + esi + 1) = M8(S_BUF + ebx);
            M16(D + 0x9ff)++;
            READ_FILE(S_BUF, 1);
        }
    }
    H_CLOSE_F(M32(S_FILE));
    M32(S_FILE) = 0;
    eax = 1;
    goto out;
refuse:
    H_CLOSE_F(M32(S_FILE));
    M32(S_FILE) = 0;
    eax = 0;
out:
    M32(S_FILE) = saved;
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x292c -- +0x74: unregister `name` from area `area` (unchecked),
 * closing the gap.  A file before the saved iteration point moves it back
 * one; the file at it marks the iteration to restart.  Returns what eax
 * held: the area's address, or strcmp's last answer when not found. */
static uint32_t FS98_STDCALL fe_unregister(uint32_t area, uint32_t name)
{
    uint32_t entry, count;
    SAVE_REGS;

    EnterCriticalSection(CS);
    entry = 0x10 * area + M32(S_AREAS);
    eax = entry;
    esi = 0;
    ebx = M32(entry + 8);
    edi = M32(entry);
    count = ebx;
    for (;;) {
        if (esi == count)
            goto out;
        eax = H_STRCMP(edi, name);
        if (eax == 0)
            break;
        esi++;
        edi += 0x1ae;
    }
    if (area == M32(D + 0x2c1) && esi <= M32(D + 0x2c5)) {
        if (esi == M32(D + 0x2c5)) {
            M32(D + 0x2c9) = 0;
            M32(D + 0x2cd) = 1;
        } else {
            M32(D + 0x2c5)--;
        }
    }
    esi = count - esi - 1;
    if (esi != 0) {
        eax = 0x1ae * esi;
        esi = edi;
        edi += eax;
        ebx = esi + 0x1ae;
        H_MEMMOVE(esi, ebx, eax);
    }
    H_MEMSET(edi, 0, 0x1ae);
    eax = entry;
    M32(entry + 8)--;
out:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x2a8b -- the index of the area whose path is `name`, or -1. */
static uint32_t FS98_STDCALL area_by_path(uint32_t name)
{
    uint32_t i = 0, e = M32(S_AREAS);

    for (;; i++, e += 0x10) {
        if (i == M32(S_NAREAS))
            return 0xffffffffu;
        if (M32(e + 0xc) != 0 && H_STRICMP(M32(e + 0xc), name) == 0)
            return i;
    }
}

/* ---- boxes ------------------------------------------------------------------------ */

/* .text+0x1db1 -- is the point at edi (latitude, longitude) inside the box at
 * esi (north, south, east, west)?  Carry when it is not.  Longitude is taken
 * as an unsigned distance east of the west edge, so a box across 180 works. */
static int point_out(void)
{
    if ((int32_t)(M32(esi) - M32(edi)) < 0)
        return 1;
    if ((int32_t)(M32(edi) - M32(esi + 4)) < 0)
        return 1;
    ecx = M32(edi + 4);
    eax = M32(esi + 8) - M32(esi + 0xc);
    ecx -= M32(esi + 0xc);
    return eax < ecx;
}

/* .text+0x1d8b -- +0x5c: 1 if the point is in the box, else 0. */
static uint32_t FS98_STDCALL fe_in_box(uint32_t box, uint32_t point)
{
    uint32_t b = ebx, si = esi, di = edi, r;

    esi = box;
    edi = point;
    r = point_out() ? 0 : 1;
    ebx = b;
    esi = si;
    edi = di;
    return r;
}

/* .text+0x1ce3 -- do the boxes at ebx and edi overlap?  Carry when they do not
 * on latitude; eax is what the last subtraction left, which +0x58 returns. */
static int boxes_apart(void)
{
    eax = M32(ebx) - M32(edi + 4);
    if ((int32_t)eax < 0)
        return 1;
    eax = M32(ebx + 4) - M32(edi);
    if ((int32_t)eax >= 0)
        return 1;
    ecx = M32(edi + 0xc);
    esi = M32(ebx + 0xc) - ecx;
    edx = M32(ebx + 8) - ecx;
    if (edx < esi)
        return 0;
    edx -= esi;
    eax = M32(edi + 8) - ecx;
    {
        int cf = eax < esi;
        eax -= esi;
        return cf;
    }
}

/* .text+0x1ccb -- +0x58. */
static uint32_t FS98_STDCALL fe_overlap(uint32_t a, uint32_t b)
{
    uint32_t bx = ebx, si = esi, di = edi;

    ebx = a;
    edi = b;
    boxes_apart();
    ebx = bx;
    esi = si;
    edi = di;
    return eax;
}

/* .text+0x1d71 -- does the registered file esi (its bounds at +0x100) lie
 * outside the search box at GLOBAL +0x3e4?  Carry when it does. */
static int file_out(void)
{
    uint32_t keep = esi;
    int cf;

    edi = G;
    ebx = edi + 0x3e4;
    edi = esi + 0x100;
    cf = boxes_apart();
    esi = keep;
    return cf;
}

/* ---- the boosters ------------------------------------------------------------------ */

/* The world file's second list: performance boosters, CD scenery copied to a
 * cache.  0x220 bytes each at +0x1750 (count +0x1760):
 *   +0x000  the box, from the booster's bstinfo file (16 bytes)
 *   +0x010  handed to BOOSTER, a word
 *   +0x012  the source directory, +0x116 the cache directory
 *   +0x21a, +0x21c  two words from the world file
 *   +0x21e  off for this session; +0x21f  turned off when next seen */
#define S_BST       (D + 0x1750)
#define S_BST_CAP   (D + 0x1754)
#define S_BST_I     (D + 0x175c)
#define S_BST_N     (D + 0x1760)
#define S_BST_FILE  (D + 0x1a64)

/* .text+0x53cf -- the next booster into esi; carry past the last. */
static int next_booster(void)
{
    M32(S_BST_I)++;
    if (M32(S_BST_I) >= M32(S_BST_N))
        return 1;
    esi = M32(S_BST) + 0x220 * M32(S_BST_I);
    M32(D + 0x1758) = esi;
    return 0;
}

/* .text+0x53e2 -- the booster S_BST_I into esi. */
static void this_booster(void)
{
    esi = M32(S_BST) + 0x220 * M32(S_BST_I);
    M32(D + 0x1758) = esi;
}

/* .text+0x53fe -- if BOOSTER has booster esi, take it back. */
static void drop_booster(void)
{
    uint16_t had = M16(esi + 0x10);

    M16(esi + 0x10) = 0;
    if (had != 0)
        BOOSTER_DROP(esi);
}

/* .text+0x542e -- a little-endian word of the world file. */
static uint32_t world_le16(void)
{
    uint32_t lo = world_byte();
    return (world_byte() << 8) | lo;
}

/* .text+0x5445 -- skip the world file to the next byte equal to cl. */
static void world_skip_to(uint8_t cl)
{
    while ((uint8_t)world_byte() != cl)
        ;
}

/* .text+0x5453 -- one byte of the bstinfo file. */
static uint8_t bst_byte(void)
{
    H_READ(M32(S_BST_FILE), D + 0x1ac0, 1, D + 0x1ac1);
    return M8(D + 0x1ac0);
}

/* .text+0x51c4 -- read the booster list from the world file, as it stands:
 * each a 1 byte and a flag (0: skipped), two words, the cache and source
 * directories (a trailing backslash dropped), and the 16-byte box from the
 * source's bstinfo.  A missing bstinfo puts up "cache directory not found"
 * and turns the booster off.  Not re-entered while that alert is up. */
static void read_boosters(void)
{
    if (M8(D + 0x1ac5) != 0)
        return;
    for (;;) {
        if (world_byte() != 1) {
            eax = M32(S_BST);
            return;
        }
        if ((uint8_t)world_byte() == 0) {
            world_le16();
            world_le16();
            world_skip_to(0);
            world_skip_to(0);
            continue;
        }
        this_booster();
        if ((int32_t)(0x220 * M32(S_BST_I)) >= (int32_t)M32(S_BST_CAP)) {
            M32(S_BST_CAP) += 0xaa0;
            REALLOC(M32(S_BST_CAP), S_BST);
            this_booster();
        }
        M16(esi + 0x21c) = (uint16_t)world_le16();
        M16(esi + 0x21a) = (uint16_t)world_le16();
        ebx = 0;
        do {
            eax = world_byte();
            M8(ebx + esi + 0x116) = (uint8_t)eax;
            ebx++;
        } while ((uint8_t)eax != 0);
        if (M8(ebx + esi + 0x114) == '\\')
            M8(ebx + esi + 0x114) = 0;
        ebx = 0;
        do {
            eax = world_byte();
            M8(ebx + esi + 0x12) = (uint8_t)eax;
            M8(ebx + D + 0x1864) = (uint8_t)eax;
            ebx++;
        } while ((uint8_t)eax != 0);
        if (M8(ebx + esi + 0x10) == '\\')
            M8(ebx + esi + 0x10) = 0;
        H_JOIN(D + 0x1864, 0, D + 0x1864, D + 0x1a68);
        if (H_OPEN(S_BST_FILE, D + 0x1864, 0, 0) != 0) {
            M8(D + 0x1ac5) = 1;
            UTIL_ALERT(1, 2, esi + 0x12, 0xf427, 0, 0, 0, 1);
            M8(D + 0x1ac5) = 0;
            M8(esi + 0x21e) = 1;
        } else {
            for (ebx = 0; ebx != 0x10; ebx++)
                M8(ebx + esi) = bst_byte();
            H_CLOSE(M32(S_BST_FILE));
            M8(esi + 0x21f) = 0;
            M8(esi + 0x21e) = 0;
        }
        M16(esi + 0x10) = 0;
        M32(S_BST_I)++;
    }
}

/* .text+0x50d4 -- build the booster list from the world file's offset n. */
static void build_boosters(uint16_t n)
{
    world_seek(n);
    ALLOC(0xaa0, S_BST, 1);
    M32(S_BST_CAP) = 0xaa0;
    M32(S_BST_N) = 0xffffffffu;
    M32(S_BST_I) = 0;
    read_boosters();
    M32(S_BST_N) = M32(S_BST_I);
    eax = M32(S_BST);
}

/* .text+0x5152 -- take every booster back from BOOSTER, free the list, and
 * build it again (unless n is 0). */
static void rebuild_boosters(uint16_t n)
{
    M32(S_BST_I) = 0xffffffffu;
    while (!next_booster())
        drop_booster();
    FREE(S_BST);
    if (n == 0) {
        eax = (eax & 0xffff0000u) | 0;
        return;
    }
    build_boosters(n);
}

static uint32_t FS98_STDCALL area_by_path(uint32_t name);

/* .text+0x4ce0 -- CHAIN list 3, every frame: each booster whose box holds the
 * aircraft and BOOSTER has not got is handed over, once its cache has a
 * bstinfo; until then "insert the CD and press RETRY" (0x10 retries), naming
 * the source's description from its bstinfo, or its path when that will not
 * open.  Boosters the aircraft has left are taken back.  The counter is a
 * word, and starts from the low half of the count. */
static void FS98_STDCALL fe_frame(uint32_t a, uint32_t b)
{
    uint32_t local, text;
    uint16_t cx;

    (void)a;
    (void)b;
    M32(S_BST_I) = 0xffffffffu;
    if (M32(S_BST) == 0)
        return;
    ecx = M32(S_BST_N);
    if ((int32_t)ecx < 0)
        return;
    edi = G;
    M32(D + 0x1a78) = M32(edi + 0x564);
    M32(D + 0x1a7c) = M32(edi + 0x56c);
    cx = (uint16_t)ecx;
    do {
        if (next_booster())
            goto next;
        if (M8(esi + 0x21e) == 1)
            goto next;
        if (M8(esi + 0x21f) == 1) {
            M8(esi + 0x21e) = 1;
            drop_booster();
            goto next;
        }
        if (fe_in_box(esi, D + 0x1a78) != 1) {
            drop_booster();
            goto next;
        }
        if (M16(esi + 0x10) != 0)
            goto next;
        for (;;) {
            H_JOIN(D + 0x1864, 0, esi + 0x116, D + 0x1a68);
            if (H_OPEN(ADDR(&local), D + 0x1864, 0, 0) == 0) {
                H_CLOSE(local);
                H_JOIN(D + 0x1764, 0, esi + 0x12, D + 0x1a70);
                eax = area_by_path(esi + 0x12);
                if (eax != 0xffffffffu) {
                    BOOSTER_ADD(esi, eax);
                    M16(esi + 0x10) = 1;
                }
                break;
            }
            H_JOIN(D + 0x1864, 0, esi + 0x12, D + 0x1a68);
            if (H_OPEN(S_BST_FILE, D + 0x1864, 0, 0) == 0) {
                H_SEEK(M32(S_BST_FILE), 0x10, 0);
                H_READ(M32(S_BST_FILE), D + 0x1a80, 2, D + 0x1ac1);
                H_SEEK(M32(S_BST_FILE), M32(D + 0x1a80), 0);
                H_READ(M32(S_BST_FILE), D + 0x1a84, 0x3b, D + 0x1ac1);
                M8(D + 0x1ac0) = 0;
                H_CLOSE(M32(S_BST_FILE));
                text = D + 0x1a84;
            } else {
                H_FIT(D + 0x1964, esi + 0x116, 0x23);
                text = D + 0x1964;
            }
            if (UTIL_ALERT(0x12, 2, text, 0xf424, 0, 0, 0, 1) != 0x10) {
                M8(esi + 0x21e) = 1;
                break;
            }
        }
next:   ;
    } while (--cx != 0);
}

/* ---- textures: the decompressor ------------------------------------------------- */

/* A file of 9-bit codes, high bit first, two bytes read at a time, ending at
 * 0x1ff.  Codes 0..0xff are bytes; each pair of codes after that defines the
 * next (0x100 on), the two strings joined, while the result is under 7
 * bytes and there are fewer than 0x1fe.  The string a code names is itself a
 * run-length stream: a byte with the top bit set is n literal bytes to
 * follow, without it the next byte n times.
 *   +0x1e10  the dictionary, 8 bytes a code: length, then the bytes
 *   +0x2e10  the last code defined; +0x2e12 the first of a pair, or -1
 *   +0x2e20  the run state, +0x2e21 its count
 *   +0x1d0e  the input buffer, +0x1d02 the next byte, +0x1d0a bytes left,
 *            +0x1d06 bits of the current byte unused */
#define S_DICT (D + 0x1e10)

/* .text+0x56a3 */
static void dict_init(void)
{
    uint32_t i;

    for (i = 0; i < 0x100; i++) {
        M8(S_DICT + 8 * i) = 1;
        M8(S_DICT + 8 * i + 1) = (uint8_t)i;
    }
    M16(D + 0x2e10) = 0xff;
    M32(D + 0x2e12) = 0xffffffffu;
    M8(D + 0x2e16) = 0;
    M8(D + 0x2e1f) = 0;
}

/* .text+0x55d1 -- the next code into eax; carry and -1 at the end.  The
 * buffer is topped up 0x100 bytes at a time when under two are left; jcxz
 * tests only cx before the leftovers are moved down. */
static int next_code(void)
{
    uint32_t cl;

    if (M32(D + 0x1d0a) < 2) {
        uint32_t n;
        edi = D + 0x1d0e;
        esi = M32(D + 0x1d02);
        M32(D + 0x1d02) = edi;
        ecx = M32(D + 0x1d0a);
        if ((uint16_t)ecx != 0)
            for (n = ecx; n != 0; n--)
                M8(edi++) = M8(esi++);
        H_READ(M32(D + 0x1cf0), edi, 0x100, D + 0x1cfa);
        M32(D + 0x1d0a) += M32(D + 0x1cfa);
    }
    ebx = M32(D + 0x1d02);
    eax = M16(ebx);
    eax = ((eax & 0xff) << 8) | (eax >> 8);
    ecx = M32(D + 0x1d06) + 8 - 9;
    if (ecx == 0) {
        ebx += 2;
        M32(D + 0x1d0a) -= 2;
        M32(D + 0x1d06) = 8;
    } else {
        M32(D + 0x1d06) = ecx;
        ebx++;
        M32(D + 0x1d0a)--;
        cl = ecx & 31;
        eax >>= cl;
    }
    M32(D + 0x1d02) = ebx;
    eax &= 0x1ff;
    if (eax < 0x1ff)
        return 0;
    if (M32(D + 0x1d06) != 8)
        M32(D + 0x1d0a)--;
    edi = M32(D + 0x1cf4);
    eax = 0xffffffffu;
    return 1;
}

/* .text+0x56e1 -- the next code's dictionary entry into eax, defining a new
 * entry from each pair of codes; -1 at the end. */
static void code_entry(void)
{
    if (next_code())
        return;
    ebx = M32(D + 0x2e12);
    if (ebx == 0xffffffffu) {
        M32(D + 0x2e12) = eax;
    } else {
        uint32_t code = eax;
        if (M16(D + 0x2e10) >= 0x1fe) {
            M16(D + 0x2e10) = 0xff;
        } else {
            esi = S_DICT + 8 * eax;
            edi = S_DICT + 8 * ebx;
            if ((uint8_t)(M8(esi) + M8(edi)) < 7) {
                M16(D + 0x2e10)++;
                ebx = S_DICT + 8 * M16(D + 0x2e10);
                M8(ebx) = (uint8_t)(M8(esi) + M8(edi));
                ebx++;
                ecx = M8(edi);
                edi++;
                do {
                    M8(ebx) = M8(edi);
                    edi++;
                    ebx++;
                } while (--ecx != 0);
                ecx = M8(esi);
                esi++;
                do {
                    M8(ebx) = M8(esi);
                    esi++;
                    ebx++;
                } while (--ecx != 0);
            }
        }
        M32(D + 0x2e12) = 0xffffffffu;
        eax = code;
    }
    eax = S_DICT + 8 * eax;
}

/* .text+0x555b -- expand every code's string into the output. */
static void expand(void)
{
    uint8_t al, st;

    for (;;) {
        code_entry();
        if (eax == 0xffffffffu)
            return;
        edi = M32(D + 0x1cf4);
        esi = eax;
        ecx = M8(esi++);
        do {
            al = M8(esi++);
            st = M8(D + 0x2e20);
            if (st == 1) {
                M8(edi++) = al;
                if (--M8(D + 0x2e21) == 0)
                    M8(D + 0x2e20) = 0;
            } else if (st > 1) {
                uint32_t n;
                for (n = M8(D + 0x2e21); n != 0; n--)
                    M8(edi++) = al;
                M8(D + 0x2e20) = 0;
            } else {
                uint8_t ah = al & 0x7f;
                M8(D + 0x2e21) = ah;
                M8(D + 0x2e20) = 1;
                if (al == ah)
                    M8(D + 0x2e20) = 2;
            }
        } while (--ecx != 0);
        M32(D + 0x1cf4) = edi;
    }
}

/* .text+0x54a0 -- decompress the open file into out; the file is left just
 * past what was used.  Returns where the output ended. */
static uint32_t FS98_STDCALL decompress(uint32_t out, uint32_t file)
{
    M32(D + 0x1cf4) = out;
    M32(D + 0x1cf0) = file;
    M32(D + 0x1d0a) = 0;
    M32(D + 0x1d06) = 8;
    dict_init();
    M8(D + 0x2e20) = 0;
    expand();
    ecx = M32(D + 0x1d0a);
    if (ecx != 0) {
        H_TELL(M32(D + 0x1cf0), D + 0x1cfe);
        edx = M32(D + 0x1cfe) - M32(D + 0x1d0a);
        H_SEEK(M32(D + 0x1cf0), edx, 0);
    }
    return M32(D + 0x1cf4);
}

/* ---- records ------------------------------------------------------------------------ */

/* .text+0x109f -- read ecx bytes of the open BGL at the offset the dword at
 * esi holds, into +0xace.  A record of type 0x14 is a jump: its dword at +1,
 * from the section base, replaces the offset and the read is made again.
 * Returns the type in eax (0 ends a list); ebx is left at esi. */
static uint8_t read_rec(void)
{
    uint8_t al;

    for (;;) {
        SEEK_FILE(M32(esi));
        READ_FILE(S_BUF, ecx);
        ebx = esi;
        al = M8(S_BUF);
        if (al != 0x14)
            break;
        edx = M32(D + 0xacf) + M32(S_BASE);
        M32(ebx) = edx;
        SEEK_FILE(edx);
    }
    eax = (eax & 0xffffff00u) | al;
    return al;
}

/* .text+0x115e, +0x116d -- a 9-byte index entry at +0x92a or +0x936. */
static uint8_t read_index_92a(void)
{
    ecx = 9;
    esi = D + 0x92a;
    return read_rec();
}

static uint8_t read_index_936(void)
{
    ecx = 9;
    esi = D + 0x936;
    return read_rec();
}

/* .text+0x2b40 -- is the index entry just read (its south and north edges,
 * words at +0xacf and +0xad1, in units of 2^9) clear of the search box's
 * latitudes?  Carry, and +0x92a stepped past it, when it is. */
static int index_out(void)
{
    edi = G;
    eax = (uint32_t)(int32_t)(int16_t)M16(D + 0xacf) << 9;
    if ((int32_t)(eax - M32(edi + 0x3e4)) >= 0)
        goto out;
    eax = (uint32_t)(int32_t)(int16_t)M16(D + 0xad1) << 9;
    if ((int32_t)(eax - M32(edi + 0x3e8)) < 0)
        goto out;
    return 0;
out:
    M32(D + 0x92a) += 9;
    return 1;
}

/* .text+0x35be -- a section's base: +0xa33, +0x936 and +0xa37. */
static void set_base(void)
{
    M32(S_BASE) = eax;
    M32(D + 0x936) = eax;
    M32(D + 0xa37) = eax;
}

/* .text+0x1c87 -- the size of the record at +0xace into eax, its header's
 * size into cx.  Type 0xd keeps its size at +0xad9; the others at +0xad8, a
 * byte for types that fold to under 5 (10.. less 6, 7.. less 3). */
static void rec_size(void)
{
    uint8_t bl;

    ecx = (ecx & 0xffff0000u) | 0xe;
    bl = M8(S_BUF);
    ebx = (ebx & 0xffffff00u) | bl;
    if (bl == 0xd) {
        eax = M16(D + 0xad9);
        ecx = (ecx & 0xffff0000u) | 0xd;
        return;
    }
    if ((int8_t)bl >= 0xa)
        bl -= 6;
    if ((int8_t)bl >= 7)
        bl -= 3;
    ebx = (ebx & 0xffffff00u) | bl;
    eax = M32(D + 0xad8);
    if ((int8_t)bl > 5)
        return;
    if (bl != 5) {
        eax &= 0xffff00ffu;
        ecx = (ecx & 0xffff0000u) | 0xd;
    }
    eax &= 0xffff;
    ecx = (ecx & 0xffff0000u) | (uint16_t)(ecx - 2);
}

/* .text+0x35ce -- step +0x92e past the record. */
static void skip_rec(void)
{
    rec_size();
    M32(D + 0x92e) += eax;
}

/* .text+0x1f94 -- how far the record's point (+0xacf, +0xad3) is from the
 * middle of the search box, as a level: under 100 km it is the distance in
 * km, past that 100 + a tenth of the rest, at most 255.  The distance is
 * found in metres through UTIL; past 65,535 km the divide faults. */
static uint8_t dist_level(void)
{
    uint32_t lo, hi, sum[2];
    uint64_t a, b, c;
    uint16_t ax, dx;
    int carry;

    edi = G;
    eax = sar32(M32(edi + 0x3e4) + M32(edi + 0x3e8), 1);
    M16(D + 0x93c) = 0;
    M32(D + 0x93e) = eax;
    eax = M32(edi + 0x3ec);
    dx = eax < M32(edi + 0x3f0);
    carry = (uint32_t)(eax + M32(edi + 0x3f0)) < eax;
    eax += M32(edi + 0x3f0);
    dx = (uint16_t)(dx + carry);
    eax = (eax >> 1) | ((uint32_t)(dx & 1) << 31);   /* sar dx,1 / rcr eax,1 */
    M32(D + 0x946) = eax;
    M16(D + 0x944) = 0;
    M32(D + 0x0e4) = M32(D + 0xacf) - M32(D + 0x93e);
    ecx = M32(D + 0xad3) - M32(D + 0x946);
    M32(D + 0xa2f) = ecx;
    M16(D + 0xa2d) = 0;
    UTIL_TO_M(D + 0xa2b, D + 0x93a, D + 0xa23);
    M32(D + 0x0ec) = M32(D + 0xa27);
    a = (uint64_t)((int64_t)(int32_t)M32(D + 0x0e4) * (int32_t)M32(D + 0x0e4));
    M32(D + 0x0f4) = (uint32_t)a;
    M32(D + 0x0f8) = (uint32_t)(a >> 32);
    b = (uint64_t)((int64_t)(int32_t)M32(D + 0x0ec) * (int32_t)M32(D + 0x0ec));
    M32(D + 0x104) = (uint32_t)b;
    M32(D + 0x108) = (uint32_t)(b >> 32);
    lo = (uint32_t)b + M32(D + 0x0f4);
    hi = (uint32_t)(b >> 32) + M32(D + 0x0f8) + (lo < (uint32_t)b);
    c = ((uint64_t)hi << 32) | lo;
    sum[0] = (uint32_t)c;
    sum[1] = (uint32_t)(c >> 32);
    eax = UTIL_SQRT64(ADDR(sum));
    ax = udiv16((uint16_t)(eax >> 16), (uint16_t)eax, 0x3e8, NULL);
    if (ax > 0x64) {
        ax = (uint16_t)(ax - 0x64);
        ax = udiv16(0, ax, 0xa, NULL);
        if (ax > 0x9b)
            ax = 0x9b;
        ax = (uint16_t)((ax & 0xff00) | (uint8_t)(ax + 0x64));
    }
    eax = (eax & 0xffff0000u) | ax;
    return (uint8_t)ax;
}

/* .text+0x430e / +0x4316 -- is the point at edi (+0x430e: dwords at +0 and
 * +4; +0x4316: at +2 and +8) inside the exclusion box of any earlier area's
 * file whose mask (+0x19c) shares a bit with ax?  Carry when it is.  The
 * walk starts below the current area's first file, so it never looks at the
 * current area. */
static int excluded(void)
{
    uint32_t keep = esi, e;

    M32(D + 0xa07) = ecx;
    M32(D + 0xa0b) = edx;
    M16(D + 0xa05) = (uint16_t)eax;
    e = M32(S_ENTRY);
    ebx = M32(e);
    for (;;) {
        ebx -= 0x1ae;
        if (ebx < M32(e)) {
            do {
                e -= 0x10;
                if (e < M32(S_AREAS)) {
                    esi = keep;
                    ecx = e;
                    return 0;
                }
                eax = 0x1ae;
            } while (M32(e + 8) == 0);
            ebx = 0x1ae * M32(e + 8) - 0x1ae + M32(e);
        }
        eax = (eax & 0xffff0000u) | M16(ebx + 0x19c);
        if ((uint16_t)eax == 0)
            continue;
        eax = (eax & 0xffff0000u) | (uint16_t)(eax & M16(D + 0xa05));
        if ((uint16_t)eax == 0)
            continue;
        M32(D + 0xa0f) = M32(ebx + 0x19e);
        M32(D + 0xa13) = M32(ebx + 0x1a2);
        M32(D + 0xa17) = M32(ebx + 0x1a6);
        M32(D + 0xa1b) = M32(ebx + 0x1aa);
        esi = D + 0xa0f;
        edi = D + 0xa07;
        {
            uint32_t b = ebx, c = e;
            int out = point_out();
            ebx = b;
            ecx = c;
            e = c;
            if (out)
                continue;
        }
        esi = keep;
        return 1;
    }
}

static int excluded_at(void)            /* .text+0x430e */
{
    ecx = M32(edi);
    edx = M32(edi + 4);
    return excluded();
}

static int excluded_mid(void)           /* .text+0x4316 */
{
    ecx = M32(edi + 2);
    edx = M32(edi + 8);
    return excluded();
}

/* .text+0x43c2 -- is the record at +0xace (type bl) on the type-4 list at
 * +0x9fb, by its two dwords at +0xacf and +0xad3 and, for its kind, its size?
 * Carry when it is.  rec_size leaves the folded type in bl for the entries
 * after. */
static int listed(void)
{
    uint8_t bl, flags;

    esi = M32(D + 0x9fb);
    ecx = M16(D + 0x9ff);
    if (ecx == 0)
        return 0;
    do {
        uint32_t keep = ecx;
        if (M32(D + 0xacf) != M32(esi + 1) || M32(D + 0xad3) != M32(esi + 5))
            goto next;
        bl = (uint8_t)ebx;
        flags = M8(esi + 9);
        if ((int8_t)bl >= 0xa && (int8_t)bl <= 0xc && (flags & 4)) {
            rec_size();
            if ((uint8_t)eax == M8(esi + 0xc))
                return 1;
            goto next;
        }
        if ((int8_t)bl >= 4 && (int8_t)bl <= 6 && (flags & 1)) {
            rec_size();
            if ((uint8_t)eax == M8(esi + 0xa))
                return 1;
            goto next;
        }
        if ((int8_t)bl >= 7 && (int8_t)bl <= 9 && (flags & 2)) {
            rec_size();
            if ((uint8_t)eax == M8(esi + 0xb))
                return 1;
        }
next:
        esi += 0xd;
        ecx = keep;
    } while (--ecx != 0);
    return 0;
}

/* ---- the search grid -------------------------------------------------------------- */

/* The scene is gathered into a grid of bands at level +0xa47 over the search
 * box at GLOBAL +0x3e4 (north, south, east, west), 8 bytes a cell at +0xa5d,
 * at most 0x7e9 cells:
 *   +0xa9e/+0xaa2  the north and south bands (south even, north odd)
 *   +0xaa6/+0xaaa  the east and west bands (west even, east odd)
 *   +0xa55, +0xa59 columns and rows; +0xa6f the bands round the world
 * A cell holds the best texture reference found for it: three bytes of
 * reference, a flag byte (0x20 seen this area, 0x40 set this area, 0x80 drawn
 * or empty), a 24-bit offset and the level it came from. */
#define S_COLS (D + 0xa55)
#define S_ROWS (D + 0xa59)
#define S_GRID (D + 0xa5d)
#define S_LVL  (D + 0xa47)

/* .text+0x1e29 -- the cell at row esi, column edi, into ebx. */
static void cell(void)
{
    uint64_t m = (uint64_t)M32(S_COLS) * esi;
    eax = (uint32_t)m + edi;
    m = (uint64_t)eax * 8;
    edx = (uint32_t)(m >> 32);
    eax = (uint32_t)m;
    ebx = M32(S_GRID) + eax;
}

/* .text+0xf05 -- the grid's bounds at the current level.  A grid that would
 * be over 0x7e8 cells is cut to 43 x 43 round the box's middle. */
static void grid_bounds(void)
{
    uint64_t m;

    edi = G;
    M32(D + 0xa9e) = fe_lat_band(edi + 0x3e4, M16(S_LVL));
    M32(D + 0xaa2) = fe_lat_band(edi + 0x3e8, M16(S_LVL));
    M32(D + 0xaa6) = fe_lon_band(edi + 0x3ec, M16(S_LVL));
    M32(D + 0xaaa) = fe_lon_band(edi + 0x3f0, M16(S_LVL));
    edi = M16(S_LVL);
    M16(D + 0xa61) = 0;
    ebx = 1u << (edi & 31);
    M32(D + 0xa6f) = ebx;
    for (;;) {
        M32(D + 0xaa2) &= 0xfffffffeu;
        M32(D + 0xaaa) &= 0xfffffffeu;
        M32(D + 0xa9e) |= 1;
        M32(D + 0xaa6) |= 1;
        eax = M32(D + 0xaa6) - M32(D + 0xaaa);
        if ((int32_t)eax < 0)
            eax += M32(D + 0xa6f);
        eax++;
        M32(S_COLS) = eax;
        ebx = M32(D + 0xa9e) - M32(D + 0xaa2) + 1;
        M32(S_ROWS) = ebx;
        m = (uint64_t)eax * ebx;
        eax = (uint32_t)m;
        edx = (uint32_t)(m >> 32);
        if (edx == 0 && eax < 0x7e9)
            break;
        M16(D + 0xa61) = 1;
        edi = G;
        M32(D + 0xa67) = sar32(M32(edi + 0x3e4) + M32(edi + 0x3e8), 1);
        M32(D + 0xa6b) = ((M32(edi + 0x3ec) - M32(edi + 0x3f0)) >> 1) + M32(edi + 0x3f0);
        eax = fe_lat_band(D + 0xa67, M16(S_LVL)) + 0x15;
        M32(D + 0xa9e) = eax;
        M32(D + 0xaa2) = eax - 0x2a;
        eax = fe_lon_band(D + 0xa6b, M16(S_LVL)) + 0x15;
        ebx = eax;
        if (eax >= M32(D + 0xa6f))
            eax -= M32(D + 0xa6f);
        M32(D + 0xaa6) = eax;
        ebx -= 0x2a;
        if ((int32_t)ebx < 0)
            ebx += M32(D + 0xa6f);
        M32(D + 0xaaa) = ebx;
    }
    M16(D + 0xa61) = (uint16_t)(M16(D + 0xa61) - 1);
}

/* .text+0xdca -- +0x48: the level G3D wants the scene at. */
static uint32_t FS98_STDCALL fe_level(uint16_t level)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    M16(D + 0x922) = 0;
    M16(S_LVL) = level;
    grid_bounds();
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0xe37 -- start a scene: the areas brought up to date and, unless
 * GLOBAL +0x3e0 bit 3 says not to, the grid laid out empty -- each cell
 * reference 9 (polar) or 0, flags 0x11, level 8 with the top bit set. */
static void grid_build(void)
{
    begin();
    M32(D + 0x2cd) = 0;
    edi = G;
    if (M16(edi + 0x3e0) & 8)
        goto out;
    grid_bounds();
    if (M32(S_GRID) == 0)
        ALLOC(0x3f48, S_GRID, 1);
    edi = M32(S_GRID);
    ebx = M32(D + 0xaa2);
    ecx = 0xd;
    ecx = (ecx & 0xffff0000u) | (uint16_t)(ecx - M16(S_LVL));
    edx = M32(S_ROWS);
    do {
        esi = M32(S_COLS);
        do {
            uint32_t polar = (ebx << (ecx & 31)) + 0xa38;
            M16(edi) = polar >= 0x1470 ? 9 : 0;
            M16(edi + 2) = 0x11;
            M16(edi + 4) = 0;
            M16(edi + 6) = 0x800;
            edi += 8;
            eax = 0x800;
        } while (--esi != 0);
        ebx++;
    } while (--edx != 0);
out:
    M32(D + 0x924) = 0;
}

/* .text+0x217c -- the grid's bounds at the section's level: shifted right by
 * +0xa51, into +0xabe..+0xaca. */
static void level_bounds(void)
{
    M32(D + 0x92e) = 0xffffffffu;
    ecx = M32(D + 0xa51);
    M32(D + 0xabe) = sar32(M32(D + 0xa9e), ecx);
    M32(D + 0xac2) = sar32(M32(D + 0xaa2), ecx);
    M32(D + 0xac6) = sar32(M32(D + 0xaa6), ecx);
    M32(D + 0xaca) = sar32(M32(D + 0xaaa), ecx);
}

/* .text+0x21bd -- step +0x92e past a row of 7-byte entries from eax to edx. */
static void skip_row(void)
{
    uint64_t m;

    edx = edx - eax + 1;
    m = (uint64_t)7 * edx;
    eax = (uint32_t)m + 5;
    edx = (uint32_t)(m >> 32);
    M32(D + 0x92e) += eax;
}

/* .text+0x2381 -- clip the columns eax..edx to the grid's ecx (east) and ebx
 * (west); carry when they miss.  A grid across the date line is taken in two
 * passes, +0xa65 saying the second is due.  The clipped range is left in esi
 * and edx and as words at +0xace and +0xad0. */
static int clip(void)
{
    if ((int32_t)ecx < (int32_t)ebx) {
        if (M16(D + 0xa65) == 1) {
            M16(D + 0xa65) = 0;
            goto from_west;
        }
        if ((int32_t)eax <= (int32_t)ecx) {
            if ((int32_t)edx >= (int32_t)ebx)
                M16(D + 0xa65) = 1;
            esi = eax;
            goto cap_east;
        }
        if ((int32_t)edx < (int32_t)ebx)
            return 1;
from_west:
        esi = eax;
        if ((int32_t)esi < (int32_t)ebx)
            esi = ebx;
        goto store;
    }
    if ((int32_t)eax > (int32_t)ecx || (int32_t)edx < (int32_t)ebx)
        return 1;
    esi = eax;
    if ((int32_t)esi < (int32_t)ebx)
        esi = ebx;
cap_east:
    if ((int32_t)edx > (int32_t)ecx)
        edx = ecx;
store:
    M16(S_BUF) = (uint16_t)esi;
    M16(D + 0xad0) = (uint16_t)edx;
    return 0;
}

/* .text+0x21d1 -- the next row of the section's level index that crosses the
 * grid.  Returns 0 with the row's entries read to +0xad2 (or a single entry,
 * type 4, at +0xab6), 1 at the end, 2 when 15 rows were read without one. */
static int next_row(void)
{
    M16(D + 0x928) = 0;
    if (M32(D + 0x92e) == 0xffffffffu) {
        for (;;) {
            ecx = 7;
            esi = D + 0x936;
            if (read_rec() == 0) {
                eax = 0;
                return 1;
            }
            eax = (uint32_t)(int32_t)(int16_t)M16(D + 0xacf);
            if ((int32_t)eax >= (int32_t)M32(D + 0xac2) &&
                (int32_t)eax <= (int32_t)M32(D + 0xabe))
                break;
            M32(D + 0x936) += 7;
        }
        M32(D + 0xaae) = eax;
        eax = M32(D + 0xad1) + M32(S_BASE);
        M32(D + 0x92e) = eax;
    }
    for (;;) {
        uint8_t al;

        M16(D + 0x928)++;
        if ((int16_t)M16(D + 0x928) > 0xf) {
            eax = (eax & 0xffff0000u) | 1;
            return 2;
        }
        ecx = 0xa;
        esi = D + 0x92e;
        al = read_rec();
        if (al == 0) {
            M32(D + 0x936) += 7;
            for (;;) {
                ecx = 7;
                esi = D + 0x936;
                if (read_rec() == 0) {
                    eax = 0;
                    return 1;
                }
                eax = (uint32_t)(int32_t)(int16_t)M16(D + 0xacf);
                if ((int32_t)eax >= (int32_t)M32(D + 0xac2) &&
                    (int32_t)eax <= (int32_t)M32(D + 0xabe))
                    break;
                M32(D + 0x936) += 7;
            }
            M32(D + 0xaae) = eax;
            eax = M32(D + 0xad1) + M32(S_BASE);
            M32(D + 0x92e) = eax;
            continue;
        }
        if (al == 4) {
            M32(D + 0x92e) += 0xa;
            eax = M16(D + 0xacf);
            ebx = M32(D + 0xaca);
            ecx = M32(D + 0xac6);
            if ((int32_t)ecx >= (int32_t)ebx) {
                if ((int32_t)eax > (int32_t)ecx || (int32_t)eax < (int32_t)ebx)
                    continue;
            } else if ((int32_t)eax > (int32_t)ecx && (int32_t)eax < (int32_t)ebx) {
                continue;
            }
            M32(D + 0xab6) = eax;
            return 0;
        }
        if (al != 5) {
            eax = 0;
            return 1;
        }
        {
            uint32_t from = M16(D + 0xacf), to = M16(D + 0xad1), n;
            uint64_t m;

            eax = from;
            edx = to;
            ebx = M32(D + 0xaca);
            ecx = M32(D + 0xac6);
            if (clip()) {
                eax = from;
                edx = to;
                skip_row();
                continue;
            }
            edx = edx - esi + 1;
            edi = edx;
            esi -= eax;
            m = (uint64_t)7 * esi;
            eax = (uint32_t)m + 5 + M32(D + 0x92e);
            M32(D + 0xa3b) = eax;
            m = (uint64_t)7 * edi;
            eax = (uint32_t)m;
            edx = (uint32_t)(m >> 32);
            n = eax;
            SEEK_FILE(M32(D + 0xa3b));
            READ_FILE(D + 0xad2, n);
            eax = from;
            edx = to;
            if (M16(D + 0xa65) != 1)
                skip_row();
            M32(D + 0xab6) = 0xffffffffu;
            return 0;
        }
    }
}

/* .text+0x2547 -- offer the reference at +0xa73 (+0xa76 its flags, +0xa77
 * the offset, +0xa51 the level) to the cell at ebx.  On a new area every
 * cell's "set this area" bit is cleared first.  It wins over a cell not
 * already set this area when both are or are not 0x80-flagged and it is the
 * coarser level, or the same level and a later offset... as retail compares
 * them below. */
static void offer(void)
{
    uint8_t al, ah;

    eax = M32(S_AREA);
    if (eax != M32(D + 0x924)) {
        uint32_t keep = esi;
        uint64_t m;
        M32(D + 0x924) = eax;
        m = (uint64_t)M32(S_COLS) * M32(S_ROWS);
        m = (uint64_t)(uint32_t)m * 8;
        edx = (uint32_t)(m >> 32);
        esi = (uint32_t)m + M32(S_GRID);
        do {
            M8(esi - 5) &= 0xbf;
            esi -= 8;
        } while (esi != M32(S_GRID));
        esi = keep;
    }
    al = M8(D + 0xa76) & 0x8f;
    ah = M8(ebx + 3);
    eax = (eax & 0xffff0000u) | ((uint32_t)ah << 8) | al;
    if ((ah & 0x20) && !(ah & 0x40))
        return;
    ah &= 0x8f;
    if (al >= 0x80) {
        if (ah >= 0x80 && al < ah)
            return;
    } else {
        if (ah >= 0x80)
            return;
        al = M8(ebx + 7);
        if ((int8_t)al < (int8_t)M8(D + 0xa51))
            return;
        if (al == M8(D + 0xa51)) {
            uint16_t ax = M16(ebx + 4);
            uint16_t sub = M16(D + 0xa77);
            uint8_t dl = (uint8_t)(M8(ebx + 6) - M8(D + 0xa79) - (ax < sub));
            if ((int8_t)dl >= 0)
                return;
        }
    }
    M32(ebx) = M32(D + 0xa73);
    M16(ebx + 4) = M16(D + 0xa77);
    M8(ebx + 6) = M8(D + 0xa79);
    M8(ebx + 7) = (uint8_t)M32(D + 0xa51);
    M8(ebx + 3) |= 0x60;
}

/* .text+0x2479 -- offer the entry at +0xa73 to every cell its band (+0xab6
 * row, +0xaae column, at level +0xa51) covers; esi is the band's width less
 * one, in grid bands. */
static void fill(void)
{
    int32_t lon0, lon1, lat0, lat1;
    unsigned cl = M32(D + 0xa51) & 31;

    ecx = M32(D + 0xa51);
    edi = M32(D + 0xaa6);
    eax = M32(D + 0xab6) << cl;
    edx = eax;
    if (edi <= M32(D + 0xaaa)) {
        eax = 1u << (M8(S_LVL) & 31);
        edi += eax;
        eax >>= 1;
        if (edx <= eax) {
            edx += eax;
            edx += eax;
        }
    }
    eax = edx;
    ebx = M32(D + 0xaaa);
    if (eax <= ebx)
        eax = ebx;
    lon0 = (int32_t)eax;
    eax = edx + esi;
    if (eax >= edi)
        eax = edi;
    lon1 = (int32_t)eax;
    eax = M32(D + 0xaae) << cl;
    ebx = M32(D + 0xaa2);
    if ((int32_t)eax <= (int32_t)ebx)
        eax = ebx;
    lat0 = (int32_t)eax;
    eax = (M32(D + 0xaae) << cl) + esi;
    ebx = M32(D + 0xa9e);
    if ((int32_t)eax >= (int32_t)ebx)
        eax = ebx;
    lat1 = (int32_t)eax;
    lat0 -= (int32_t)M32(D + 0xaa2);
    lat1 -= (int32_t)M32(D + 0xaa2);
    lon0 -= (int32_t)M32(D + 0xaaa);
    lon1 -= (int32_t)M32(D + 0xaaa);
    esi = (uint32_t)lat0;
    do {
        edi = (uint32_t)lon0;
        do {
            cell();
            offer();
            edi++;
        } while ((int32_t)edi <= lon1);
        esi++;
    } while ((int32_t)esi <= lat1);
}

/* .text+0x23e5 -- offer the row just read: one entry at +0xab6, or each of
 * the row's 7-byte entries from column +0xace. */
static void offer_row(void)
{
    uint32_t n, off;

    ecx = M32(D + 0xa51);
    esi = (1u << (ecx & 31)) - 1;
    eax = M32(D + 0xab6);
    if ((uint16_t)eax != 0xffff) {
        M32(D + 0xa73) = M32(D + 0xad1);
        M16(D + 0xa77) = M16(D + 0xad5);
        M8(D + 0xa79) = M8(D + 0xad7);
        fill();
        return;
    }
    eax = M16(S_BUF);
    M32(D + 0xab6) = eax;
    ecx = M16(D + 0xad0) - eax;
    n = ecx;
    off = 4;
    {
        uint32_t mask = esi;
        do {
            M32(D + 0xa73) = M32(S_BUF + off);
            M16(D + 0xa77) = M16(D + 0xad2 + off);
            M8(D + 0xa79) = M8(D + 0xad4 + off);
            fill();
            esi = mask;
            ebx = off;
            M32(D + 0xab6)++;
            off += 7;
        } while ((int32_t)--n >= 0);
        ecx = n;
    }
}

/* ---- emitting the grid to G3D ---------------------------------------------------- */

/* .text+0x29cf -- does the cell at row esi, column edi hold the reference at
 * +0xa7a?  Carry when it does not.  ebx is left at the cell; edx is kept. */
static int cell_differs(void)
{
    uint32_t dx = edx;

    cell();
    edx = dx;
    eax = (eax & 0xffff0000u) | M16(D + 0xa7a);
    if (M16(ebx) != (uint16_t)eax)
        return 1;
    eax = (eax & 0xffffff00u) | M8(D + 0xa7c);
    if (M8(ebx + 2) != (uint8_t)eax)
        return 1;
    eax = (eax & 0xffff0000u) | M16(D + 0xa7e);
    if (M16(ebx + 4) != (uint16_t)eax)
        return 1;
    eax = (eax & 0xffffff00u) | M8(D + 0xa80);
    if (M8(ebx + 6) != (uint8_t)eax)
        return 1;
    return 0;
}

/* .text+0x27c5 -- is row esi in the same coarse block as the run's first row
 * (by the mask +0xa81)?  0 when it is. */
static uint16_t row_block(void)
{
    uint16_t ax;

    eax = esi + M32(D + 0xaa2);
    eax = sar32(eax, (uint8_t)(0xfff8 + M16(S_LVL)));
    ax = (uint16_t)(eax ^ M16(D + 0xa92));
    eax = (eax & 0xffff0000u) | ax;
    return (uint16_t)(M16(D + 0xa81) & ax);
}

/* .text+0x2952 -- for a reference of type 0x22, which of the rows above and
 * below hold it too (4 above, 8 below), within the block. */
static uint8_t rows_beside(void)
{
    uint32_t bx = ebx;
    uint8_t cl = 0;

    if (M8(D + 0xa7c) == 0x22) {
        esi--;
        if ((int32_t)esi >= 0 && row_block() == 0 && !cell_differs())
            cl |= 4;
        esi += 2;
        if (esi != M32(S_ROWS) && row_block() == 0 && !cell_differs())
            cl |= 8;
        esi--;
    }
    ebx = bx;
    ecx = (ecx & 0xffffff00u) | cl;
    return cl;
}

/* .text+0x2991, +0x29b3 -- for type 0x22: is the column after (2) or before
 * (1) the edge of the grid or the same reference? */
static uint8_t col_after(void)
{
    uint32_t bx = ebx;
    uint8_t cl = 0;

    if (M8(D + 0xa7c) == 0x22) {
        edi++;
        if (edi == M32(S_COLS) || !cell_differs())
            cl |= 2;
        edi--;
    }
    ebx = bx;
    ecx = (ecx & 0xffffff00u) | cl;
    return cl;
}

static uint8_t col_before(void)
{
    uint32_t bx = ebx;
    uint8_t cl = 0;

    if (M8(D + 0xa7c) == 0x22) {
        edi--;
        if ((int32_t)edi < 0 || !cell_differs())
            cl |= 1;
        edi++;
    }
    ebx = bx;
    ecx = (ecx & 0xffffff00u) | cl;
    return cl;
}

/* .text+0x2a05 -- for a type-0x22 cell, do its rows above and below differ
 * from the run's first?  Carry when they do. */
static int rows_differ(void)
{
    uint32_t cx = ecx;

    eax = (eax & 0xffffff00u) | M8(D + 0xa7c);
    if (M8(ebx + 2) == 0x22 && rows_beside() != M8(D + 0xa85)) {
        ecx = cx;
        return 1;
    }
    ecx = cx;
    return 0;
}

/* .text+0x28d5 -- from column edi, how many cells east carry the same
 * reference in the same coarse block (into ecx), stopping at +0xa8a or
 * +0xa96 cells, the grid's edge, a drawn cell, or a change in the rows
 * beside; and the edge flags of the run (into al) when there is one. */
static void run_east(void)
{
    uint32_t di = edi;
    uint8_t cl;

    cl = col_before();
    M8(D + 0xa89) = cl;
    ecx = 0;
    for (;;) {
        uint16_t ax;
        eax = edi + M32(D + 0xaaa);
        ax = sar16((uint16_t)eax, (uint8_t)(0xfff8 + M16(S_LVL)));
        ax ^= M16(D + 0xa94);
        eax = (eax & 0xffff0000u) | ax;
        if (M16(D + 0xa81) & ax)
            break;
        if (ecx == M32(D + 0xa8a) || ecx == M32(D + 0xa96) || edi == M32(S_COLS))
            break;
        if (cell_differs())
            break;
        if (M8(ebx + 7) == 0x80)
            break;
        if (rows_differ())
            break;
        ecx++;
        edi++;
    }
    if (ecx != 0) {
        uint32_t n = ecx;
        edi--;
        cl = col_after();
        eax = (eax & 0xffffff00u) | (uint8_t)(cl | M8(D + 0xa89));
        ecx = n;
    }
    edi = di;
}

/* .text+0x26d2 -- the largest block of cells from the cell at (+0xac2, +0xaca)
 * holding its reference: +0xa96 columns by +0xa9a rows, at most +0xa8a
 * across and +0xa8e down, within a coarse block whose size (+0xa81) grows
 * toward the poles; +0xa88 its edge flags. */
static void span(void)
{
    uint16_t ax;
    uint8_t cl, al;

    esi = M32(D + 0xac2);
    edi = M32(D + 0xaca);
    M32(D + 0xa9a) = 1;
    M32(D + 0xa96) = 0x2d;
    eax = esi + M32(D + 0xaa2);
    ebx = edi + M32(D + 0xaaa);
    cl = (uint8_t)(0xfff8 + M16(S_LVL));
    eax = sar32(eax, cl);
    ebx = sar32(ebx, cl);
    M16(D + 0xa92) = (uint16_t)eax;
    M16(D + 0xa94) = (uint16_t)ebx;
    if ((int16_t)eax < 0)
        eax = ~eax;
    ax = (uint16_t)eax;
    ebx = 0;
    ecx = 1;
    while ((int16_t)ax >= (int16_t)M16(D + 0x1724 + ebx)) {
        ecx += ecx;
        ebx += 2;
    }
    M16(D + 0xa81) = (uint16_t)ecx;
    cl = rows_beside();
    M8(D + 0xa85) = cl;
    M8(D + 0xa88) = cl;
    run_east();
    al = (uint8_t)eax;
    M8(D + 0xa86) = al;
    M8(D + 0xa88) |= al;
    M32(D + 0xa96) = ecx;
    for (;;) {
        esi++;
        if (esi == M32(S_ROWS))
            return;
        if (row_block() != 0)
            return;
        cl = rows_beside();
        M8(D + 0xa85) = cl;
        run_east();
        if (ecx != M32(D + 0xa96))
            return;
        if ((uint8_t)eax != M8(D + 0xa86))
            return;
        eax = M32(D + 0xa8e);
        if (M32(D + 0xa9a) == eax)
            return;
        M32(D + 0xa9a)++;
        al = M8(D + 0xa85);
        M8(D + 0xa88) &= 0xf7;
        al &= 8;
        M8(D + 0xa88) |= al;
    }
}

/* .text+0x27ec -- for type 0x22, the block's corner flags in +0xa87: each
 * corner (0x80 south-east .. 0x10 north-west) that is the grid's edge or the
 * same reference, where both its sides are. */
static void corners(void)
{
    uint8_t dl;

    if (M8(D + 0xa7c) != 0x22)
        return;
    dl = M8(D + 0xa88) & 0xf;
    if ((dl & 8) && (dl & 2)) {
        esi = M32(D + 0xac2) + M32(D + 0xa9a);
        edi = M32(D + 0xaca);
        if (esi == M32(S_ROWS))
            goto c80;
        edi += M32(D + 0xa96);
        if (edi == M32(S_COLS))
            goto c80;
        edx = (edx & 0xffffff00u) | dl;
        if (cell_differs())
            goto c40;
c80:    dl |= 0x80;
    }
c40:
    if ((dl & 8) && (dl & 1)) {
        esi = M32(D + 0xac2) + M32(D + 0xa9a);
        edi = M32(D + 0xaca);
        if (esi == M32(S_ROWS))
            goto s40;
        if ((int32_t)--edi < 0)
            goto s40;
        edx = (edx & 0xffffff00u) | dl;
        if (cell_differs())
            goto c20;
s40:    dl |= 0x40;
    }
c20:
    if ((dl & 4) && (dl & 2)) {
        esi = M32(D + 0xac2);
        edi = M32(D + 0xaca);
        if ((int32_t)--esi < 0)
            goto s20;
        edi += M32(D + 0xa96);
        if (edi == M32(S_COLS))
            goto s20;
        edx = (edx & 0xffffff00u) | dl;
        if (cell_differs())
            goto c10;
s20:    dl |= 0x20;
    }
c10:
    if ((dl & 4) && (dl & 1)) {
        esi = M32(D + 0xac2);
        edi = M32(D + 0xaca);
        if ((int32_t)--esi < 0)
            goto s10;
        if ((int32_t)--edi < 0)
            goto s10;
        edx = (edx & 0xffffff00u) | dl;
        if (cell_differs())
            goto done;
s10:    dl |= 0x10;
    }
done:
    edx = (edx & 0xffffff00u) | dl;
    M8(D + 0xa87) = dl;
}

/* .text+0x25ff -- take the cell at ebx (row esi, column edi) as a block's
 * first, find the block, and mark its cells drawn.  Type 0x22 blocks are
 * single cells; otherwise blocks run to 2 across, or 1 when GLOBAL +0x12a4
 * bit 7 is set, or 0x2e when GLOBAL +0xcb4 is under 2 -- and when the block
 * found is exactly that square, the limit is raised until it is not. */
static void take_block(void)
{
    M16(D + 0xa7a) = M16(ebx);
    M8(D + 0xa7c) = M8(ebx + 2);
    M8(D + 0xa7d) = M8(ebx + 3);
    M16(D + 0xa7e) = M16(ebx + 4);
    M8(D + 0xa80) = M8(ebx + 6);
    M32(D + 0xac2) = esi;
    M32(D + 0xaca) = edi;
    ecx = 2;
    if (M8(D + 0xa7c) == 0x22)
        goto fixed;
    edi = G;
    ecx = 1;
    if (M32(edi + 0x12a4) & 0x80)
        goto fixed;
    ecx = 2;
    if ((int16_t)M16(edi + 0xcb4) < 2) {
        ecx = 0x2e;
        goto fixed;
    }
    for (;;) {
        uint32_t n = ecx;
        M32(D + 0xa8a) = ecx;
        M32(D + 0xa8e) = ecx;
        span();
        ecx = n;
        if (ecx != M32(D + 0xa96) || ecx != M32(D + 0xa9a))
            break;
        ecx++;
    }
    ecx--;
fixed:
    M32(D + 0xa8a) = ecx;
    M32(D + 0xa8e) = ecx;
    span();
    corners();
    esi = M32(D + 0xac2);
    edi = M32(D + 0xaca);
    ecx = M32(D + 0xa9a);
    do {
        uint16_t n;
        cell();
        eax = M32(D + 0xa96);
        n = (uint16_t)eax;
        do {
            M8(ebx + 7) = 0x80;
            ebx += 8;
        } while (--n != 0);
        eax = (eax & 0xffff0000u) | n;
        esi++;
    } while (--ecx != 0);
}

/* .text+0x2c22 -- the block's texture scale: 0x1fffc000 halved once for each
 * step of the band table at +0x1724 the block's latitude passes, divided by
 * the entry of +0x1320 for that latitude; +0xa83 the level it leaves. */
static uint32_t block_scale(void)
{
    uint32_t keep = edi, idx;

    eax = M32(D + 0xaa2) + M32(D + 0xac2);
    if ((int32_t)eax < 0) {
        eax += M32(D + 0xa9a);
        eax--;
        eax = ~eax;
    }
    ecx = (ecx & 0xffff0000u) | (uint16_t)(M16(S_LVL) - 8);
    eax >>= (ecx & 31);
    idx = eax;
    M16(D + 0xa83) = 0xd;
    ecx = 0x1fffc000;
    edi = D + 0x1724;
    do {
        if ((int16_t)eax < (int16_t)M16(edi))
            break;
        edi += 2;
        ecx >>= 1;
        M16(D + 0xa83)--;
    } while (edi != D + 0x1730);
    edx = 0;
    ebx = idx * 4;
    eax = udiv32(0, ecx, M32(D + 0x1320 + ebx));
    ecx = M32(D + 0x1320 + ebx);
    edi = keep;
    return eax;
}

/* .text+0x2a24 -- write the block as a 0x44-byte record into buf: a fixed
 * header, the scale, the reference and flags, the block's size in texture
 * units, its south-west band, its middle as a latitude and longitude, and
 * its offset as a 48-bit altitude.  +0x20 is not written. */
static void emit_block(uint32_t buf)
{
    uint32_t keep_a25, keep_a27, mid_lon;
    uint16_t ax, dx, bx, cx;
    unsigned cl;
    int cf;

    edi = buf;
    M16(edi) = 0x5a;
    M16(edi + 0x2) = 0x8;
    M16(edi + 0x4) = 0xd;
    M16(edi + 0x6) = 0x40;
    M16(edi + 0x8) = 0x76;
    M16(edi + 0xa) = 0x2f;
    M16(edi + 0xc) = 0x38;
    M16(edi + 0x2a) = 0x3d;
    M16(edi + 0x42) = 0x22;
    M16(edi + 0xe) = 0;
    M16(edi + 0x10) = 0;
    eax = block_scale();
    M32(edi + 0x14) = eax;
    ax = M16(D + 0xa7a);
    dx = (uint16_t)(M8(D + 0xa7c) | (M8(D + 0xa7d) << 8));
    if ((uint8_t)dx == 0x22)
        ax = (uint16_t)((ax & 0xff) | (M8(D + 0xa87) << 8));
    M16(edi + 0x2c) = ax;
    M16(edi + 0x2e) = dx;
    cl = (uint8_t)(M16(S_LVL) - M16(D + 0xa83));
    eax = 0x100u >> (cl & 31);
    M32(edi + 0x30) = eax * M32(D + 0xa96);
    M32(edi + 0x34) = eax * M32(D + 0xa9a);
    cl = (uint8_t)(0xd - M8(S_LVL));
    eax = (M32(D + 0xaa2) + M32(D + 0xac2)) << (cl & 31);
    M16(edi + 0x3c) = (uint16_t)eax;
    eax = (M32(D + 0xaaa) + M32(D + 0xaca)) << (cl & 31);
    M16(edi + 0x3e) = (uint16_t)eax;

    edx = M32(D + 0xaa2) + M32(D + 0xac2);
    fe_band_lat((uint16_t)edx, M16(S_LVL), D + 0xa23);
    keep_a27 = M32(D + 0xa27);
    keep_a25 = M16(D + 0xa25);
    edx = M32(D + 0xaa2) + M32(D + 0xac2) + M32(D + 0xa9a);
    fe_band_lat((uint16_t)edx, M16(S_LVL), D + 0xa23);
    bx = (uint16_t)keep_a25;
    cf = (uint16_t)(bx + M16(D + 0xa25)) < bx;
    bx = (uint16_t)(bx + M16(D + 0xa25));
    eax = keep_a27 + M32(D + 0xa27) + cf;
    cf = eax & 1;                                       /* sar eax,1 / rcr bx,1 */
    eax = sar32(eax, 1);
    bx = (uint16_t)((bx >> 1) | (cf << 15));
    edi = buf;
    M16(edi + 0x18) = bx;
    M32(edi + 0x1a) = eax;

    edx = M32(D + 0xaaa) + M32(D + 0xaca);
    fe_band_lon((uint16_t)edx, M16(S_LVL), D + 0xa2b);
    mid_lon = M32(D + 0xa2f) >> 16;
    edx = M32(D + 0xaaa) + M32(D + 0xaca) + M32(D + 0xa96);
    fe_band_lon((uint16_t)edx, M16(S_LVL), D + 0xa2b);
    edx = M32(D + 0xa2f) >> 16;
    ebx = mid_lon;
    dx = (uint16_t)((uint16_t)edx - (uint16_t)ebx);
    cf = dx & 1;                                        /* shr dx,1 / rcr ax,1 */
    dx >>= 1;
    ax = (uint16_t)(cf << 15);
    dx = (uint16_t)(dx + (uint16_t)ebx);
    edx = (edx & 0xffff0000u) | dx;
    edi = buf;
    M16(edi + 0x1e) = 0;
    M16(edi + 0x1e) = ax;
    M16(edi + 0x22) = dx;

    cx = (uint16_t)(M8(D + 0xa7e) << 8);
    ax = M16(D + 0xa7f);
    dx = (int16_t)ax < 0 ? 0xffff : 0;
    M16(edi + 0x38) = cx;
    M16(edi + 0x3a) = ax;
    M16(edi + 0x24) = cx;
    M16(edi + 0x26) = ax;
    M16(edi + 0x28) = dx;
    M16(edi + 0x40) = 0;
    ecx = 0x44;
}

/* .text+0x2aee -- carry on where the last call left the iteration: when it
 * was marked to restart, the saved point is put back and the file opened
 * again -- or, if the saved file has gone, the index stepped back and 0
 * returned.  Otherwise the section's base is restored; returns 1. */
static uint32_t resume(void)
{
    if (M32(D + 0x2cd) != 0) {
        M32(S_INDEX) = M32(D + 0x2c5);
        M32(S_AREA) = M32(D + 0x2c1);
        M32(D + 0x2cd) = 0;
        if (M32(D + 0x2c9) == 0) {
            M32(S_INDEX)--;
            eax = 0;
            return 0;
        }
        open_file();
    }
    M32(S_BASE) = M32(D + 0xa37);
    eax = 1;
    return 1;
}

/* ---- +0x18: the scene stream ---------------------------------------------------- */

/* .text+0x2b7b -- +0x18: the next piece of the scene into buf (room bytes).
 * Returns 1 with *len bytes written (0 for a call that only advanced) and
 * *area the file's area path; -1 with *len the bytes the next record needs;
 * 0 at the end.  A state machine across calls, in +0x922:
 *   0     lay out the grid, then 1
 *   1     the next file in the box with a scene section (+0x138): 2;
 *         none left: 4 (or the end, with GLOBAL +0x3e0 bit 3)
 *   2, 3  the section's index (9-byte entries, by latitude) and its records:
 *         each in reach (by type, box, distance level, exclusions) is copied
 *         out; at 15 entries a call returns to yield; at the end, 7
 *   4     the grid's four edge cells' references into GLOBAL +0x414..+0x420
 *   5     each undrawn cell's block, as a 0x44-byte record
 *   6     free the grid; the end
 *   7, 8  the file's texture sections, levels 8.. at header +0x118.., the
 *         ones at or below the scene's level
 *   9     their rows offered to the grid; then the next section (8)
 * Any other state ends the stream (0x63). */
uint32_t FS98_STDCALL fe_scene(uint32_t buf, int32_t room, uint32_t len, uint32_t area)
{
    uint8_t al;
    int r;
    SAVE_REGS;

    EnterCriticalSection(CS);
    for (;;) {
        uint16_t st = M16(D + 0x922);
        if (st == 0) {
            grid_build();
            goto next_file;
        }
        switch ((uint8_t)st) {
        case 2:
            if (!resume())
                goto next_file;
            M16(D + 0x928) = 0;
            for (;;) {
                M16(D + 0x928)++;
                if ((int16_t)M16(D + 0x928) > 0xf)
                    goto yield;
                if (read_index_936() == 0)
                    goto textures;
                if (!index_out())
                    break;
                M32(D + 0x936) += 9;
            }
            M32(D + 0x92e) = M32(D + 0xad3) + M32(S_BASE);
            M16(D + 0x922) = 3;
            goto records;
        case 3:
            if (!resume())
                goto next_file;
            M16(D + 0x928) = 0;
            goto records;
        case 4:
            esi = 0;
            edi = M32(S_COLS) >> 1;
            cell();
            M32(G + 0x418) = M32(ebx);
            fe_scene_out_edi = edi;
            fe_scene_shifted = 1;
            esi = M32(S_ROWS) - 1;
            cell();
            M32(G + 0x414) = M32(ebx);
            esi = M32(S_ROWS) >> 1;
            edi = 0;
            cell();
            M32(G + 0x420) = M32(ebx);
            edi = M32(S_COLS) - 1;
            cell();
            edi = G;
            M32(edi + 0x41c) = M32(ebx);
            M16(D + 0x922) = 5;
            continue;
        case 5:
            if (room < 0x44 && (M16(G + 0x3e0) & 1))
                goto too_small_44;
            edi = 0;
            esi = 0;
            ebx = M32(S_GRID);
            for (;;) {
                if (M8(ebx + 7) != 0x80) {
                    take_block();
                    if (room < 0x44)
                        goto too_small_44;
                    emit_block(buf);
                    M32(len) = ecx;
                    eax = 1;
                    goto done;
                }
                ebx += 8;
                edi++;
                if ((int32_t)edi < (int32_t)M32(S_COLS))
                    continue;
                edi = 0;
                esi++;
                if ((int32_t)esi < (int32_t)M32(S_ROWS))
                    continue;
                break;
            }
            M16(D + 0x922) = 6;
            continue;
        case 6:
            free_grid();
            goto end;
        case 7:
            edi = G;
            if (M16(edi + 0x3e0) & 8)
                goto close_next;
            esi = M32(S_REC);
            M32(D + 0xa49) = 0x114;
            M32(D + 0xa4d) = 7;
            M16(D + 0x922) = 8;
            /* fall through */
        case 8:
            for (;;) {
                M32(D + 0xa4d)++;
                M32(D + 0xa49) += 4;
                ebx = M32(D + 0xa49);
                if ((int32_t)ebx > 0x134)
                    goto close_next;
                esi = M32(S_REC);
                eax = M32(ebx + esi);
                if (eax != 0)
                    break;
            }
            ecx = M16(S_LVL) - M32(D + 0xa4d);
            if ((int32_t)ecx < 0)
                goto close_next;
            M32(D + 0xa51) = ecx;
            set_base();
            if (M32(S_FILE) == 0)
                open_file();
            level_bounds();
            M16(D + 0x922) = 9;
            continue;
        case 9:
            if (!resume())
                goto next_file;
            r = next_row();
            if (r == 0) {
                offer_row();
            } else if (r == 1) {
                M16(D + 0x922) = 8;
                continue;
            }
            M32(len) = 0;
            eax = 1;
            goto done;
        default:
            goto end;
        }

close_next:
        close_file();
next_file:
        r = next_file();
        M32(D + 0x2c5) = M32(S_INDEX);
        M32(D + 0x2c1) = M32(S_AREA);
        M32(D + 0x2c9) = 1;
        if (r) {
            edi = G;
            if (M16(edi + 0x3e0) & 8)
                goto end;
            M16(D + 0x922) = 4;
            continue;
        }
        if (file_out())
            goto next_file;
        eax = M32(esi + 0x138);
        set_base();
        if (eax == 0)
            goto textures;
        edi = G;
        if (M16(edi + 0x3e0) & 4)
            goto textures;
        open_file();
        M16(D + 0x922) = 2;
        continue;
textures:
        M16(D + 0x922) = 7;
        continue;

records:
        for (;;) {
            M16(D + 0x928)++;
            if ((int16_t)M16(D + 0x928) > 0xf)
                goto yield;
            ecx = 0xe;
            esi = D + 0x92e;
            al = read_rec();
            if (al == 0) {
                M32(D + 0x936) += 9;
                M16(D + 0x922) = 2;
                break;
            }
            edi = G;
            esi = edi + 0x3f4;
            edi = D + 0xacf;
            if (!point_out()) {
                al = M8(S_BUF);
                if ((int8_t)al >= 0xa)
                    goto big;
                if ((int8_t)al >= 7)
                    goto skip;
                goto small;
            }
            al = M8(S_BUF);
            edi = G;
            esi = edi + 0x3e4;
            edi = D + 0xacf;
            {
                uint16_t ax = (uint16_t)((uint16_t)(eax & 0xff00) | al);
                int out = point_out();
                eax = (eax & 0xffff0000u) | ax;
                if (out)
                    goto skip;
            }
            if ((int8_t)al >= 0xa)
                goto big;
            if ((int8_t)al < 7)
                goto skip;
            goto small;
big:
            if (al != 0xd)
                goto small;
            al = dist_level();
            if (al < M8(D + 0xad7) || al > M8(D + 0xad8))
                goto skip;
            goto copy;
small:
            al = dist_level();
            if (al > M8(D + 0xad7))
                goto skip;
            edi = D + 0xacf;
            eax = (eax & 0xffff0000u) | 1;
            if (excluded_at())
                goto skip;
            ebx = (ebx & 0xffffff00u) | M8(S_BUF);
            if (listed())
                goto skip;
copy:
            rec_size();
            ebx = (uint32_t)room;
            if ((int32_t)(eax - ebx) >= 0) {
                edi = G;
                if (!(M16(edi + 0x3e0) & 1))
                    skip_rec();
                rec_size();
                M32(len) = eax;
                eax = 0xffffffffu;
                goto done;
            }
            ecx = 0;
            rec_size();
            M32(D + 0x92e) += ecx;
            {
                uint16_t n = (uint16_t)(eax - ecx);
                SEEK_FILE(M32(D + 0x92e));
                ecx = n;
                READ_FILE(buf, ecx);
                M32(D + 0x92e) += ecx;
            }
            goto out_len;
skip:
            skip_rec();
        }
        continue;

yield:
        ecx = 0;
out_len:
        M32(len) = ecx;
        M32(area) = M32(D + 0x2bd);
        eax = 1;
        goto done;
too_small_44:
        M32(len) = 0x44;
        eax = 0xffffffffu;
        goto done;
end:
        M16(D + 0x922) = 0x63;
        eax = 0;
        goto done;
    }
done:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* +0x18's public entry.  In state 4 retail fails to pop its second saved EDI,
 * so its epilogue returns EBX=old ESI, ESI=old EDI and EDI=columns/2. */
__asm__(".text\n"
        "_fe_scene_entry:\n\t"
        "movl %ebx, _fe_scene_in_ebx\n\t"
        "movl %esi, _fe_scene_in_esi\n\t"
        "movl %edi, _fe_scene_in_edi\n\t"
        "movl $0, _fe_scene_shifted\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "call _fe_scene@16\n\t"
        "cmpl $0, _fe_scene_shifted\n\t"
        "je 1f\n\t"
        "movl _fe_scene_in_esi, %ebx\n\t"
        "movl _fe_scene_in_edi, %esi\n\t"
        "movl _fe_scene_out_edi, %edi\n"
        "1:\n\t"
        "ret $16\n");
void fe_scene_entry(void);

/* ---- lookups: the shared machinery ---------------------------------------------- */

/* .text+0x2238 -- +0x54: the search box `radius` metres round pos (a 48-bit
 * position pair: latitude at +4, longitude at +0xc), into box (north, south,
 * east, west).  The longitude half-width comes from UTIL at pos's latitude. */
static uint32_t FS98_STDCALL fe_search(uint32_t pos, uint32_t radius, uint32_t box)
{
    uint8_t in[8];
    SAVE_REGS;

    edi = box;
    esi = pos;
    eax = M32(esi + 4);
    M32(edi) = eax + radius;
    M32(edi + 4) = eax - radius;
    edx = esi;
    M16(ADDR(in) + 2) = 0;
    M32(ADDR(in) + 4) = radius;
    UTIL_TO_LON(ADDR(in), edx, D + 0xa2b);
    eax = M32(esi + 0xc);
    M32(edi + 8) = eax + M32(D + 0xa2f);
    eax -= M32(D + 0xa2f);
    M32(edi + 0xc) = eax;
    RESTORE_REGS;
    return eax;
}

/* .text+0x32e3 -- unpack the 8-byte position at esi (bl its extra byte, kept
 * at +0x9ba) to three 48-bit values at +0x9a8; esi is left there. */
static void unpack_point(void)
{
    uint8_t v[0x18];

    ebx &= 0xffff00ffu;
    M16(D + 0x9ba) = (uint16_t)ebx;
    UTIL_UNPACK(esi, ADDR(v));
    M16(D + 0x9a8) = M16(ADDR(v) + 0x2);
    M32(D + 0x9aa) = M32(ADDR(v) + 0x4);
    M16(D + 0x9ae) = M16(ADDR(v) + 0xa);
    M32(D + 0x9b0) = M32(ADDR(v) + 0xc);
    M16(D + 0x9b4) = M16(ADDR(v) + 0x12);
    M32(D + 0x9b6) = M32(ADDR(v) + 0x14);
    esi = D + 0x9a8;
}

/* .text+0x1e42 -- a navaid's strength at the aircraft (+0x94a): 0 when an
 * earlier area excludes it (mask ax) or the east-west distance alone is past
 * its range (+0x12, doubled when `wide`, times +0x115); otherwise the range
 * over the distance (hypot of the two, in 256 m), capped at 0x7fff.  The
 * distance and the height difference are kept at +0x30a/+0x30e (wide) or
 * +0x312/+0x316. */
static uint32_t strength(uint32_t wide)
{
    uint8_t in[8];
    uint32_t keep, range;
    uint16_t ax, cx;
    uint32_t r;
    SAVE_REGS;

    edi = esi;
    if (excluded_mid()) {
        eax = 0;
        goto out;
    }
    ebx = M32(D + 0x94a);
    ax = (uint16_t)(M16(ebx + 0xa) - M16(esi + 6));
    eax = M32(ebx + 0xc) - M32(esi + 8) - (M16(ebx + 0xa) < M16(esi + 6));
    M16(ADDR(in) + 2) = ax;
    M32(ADDR(in) + 4) = eax;
    keep = esi;
    UTIL_TO_M(ADDR(in), M32(D + 0x94a), D + 0xa23);
    esi = keep;
    ecx = M32(D + 0xa27);
    M32(D + 0x9bc) = ecx;
    if ((int32_t)ecx < 0)
        ecx = 0u - ecx;
    ax = M16(esi + 0x12);
    if (wide)
        ax = (uint16_t)(ax << 1);
    range = (uint32_t)ax * M16(D + 0x115);
    edx = range;
    if (ecx >= edx) {
        eax = 0;
        goto out;
    }
    edi = M32(D + 0x94a);
    eax = M32(edi + 4) >> 8;
    ax = (uint16_t)((uint16_t)eax - M16(esi + 3));
    cx = M16(D + 0x9bd);
    eax = UTIL_HYPOT((uint32_t)(int32_t)(int16_t)ax, (uint32_t)(int32_t)(int16_t)cx);
    ebx = (uint16_t)eax;
    if (wide) {
        M32(D + 0x30a) = ebx << 8;
        M32(D + 0x30e) = M32(edi + 0x14) - M32(esi + 0xe);
    } else {
        M32(D + 0x312) = ebx << 8;
        M32(D + 0x316) = M32(edi + 0x14) - M32(esi + 0xe);
    }
    range = (uint32_t)M16(esi + 0x12) * M16(D + 0x115);
    r = sdiv32(0, range, ebx);
    if (r > 0x7fff)
        r = 0x7fff;
    eax = r;
out:
    RESTORE_REGS;
    return eax;
}

/* .text+0x1dcf -- is the 48-bit point at edi (+0xad2: latitude bytes 0..2,
 * longitude 3..5) inside the box at esi (48-bit north, south, east, west)?
 * Carry when it is not. */
static int point48_out(void)
{
    uint16_t ax, dx, bx, cx;
    uint8_t dl;
    int cf;

    ax = M16(esi);
    dx = M16(esi + 2);
    ax = (uint16_t)((ax >> 1) | ((dx & 1) << 15));
    dx = (uint16_t)((int16_t)dx >> 1);
    cf = ax < M16(edi);
    ax = (uint16_t)(ax - M16(edi));
    dl = (uint8_t)((uint8_t)dx - M8(edi + 2) - cf);
    if ((int8_t)dl < 0)
        return 1;
    dx = (uint16_t)(int16_t)(int8_t)M8(edi + 2);
    ax = M16(edi);
    cf = ax >> 15;
    ax = (uint16_t)(ax << 1);
    dx = (uint16_t)((dx << 1) | cf);
    cf = ax < M16(esi + 4);
    ax = (uint16_t)(ax - M16(esi + 4));
    dx = (uint16_t)(dx - M16(esi + 6) - cf);
    if ((int16_t)dx < 0)
        return 1;
    cx = (uint16_t)(M8(edi + 3) << 8);
    bx = M16(edi + 4);
    {
        uint32_t east = ((uint32_t)M16(esi + 0xa) << 16) | M16(esi + 8);
        uint32_t west = ((uint32_t)M16(esi + 0xe) << 16) | M16(esi + 0xc);
        uint32_t pt = ((uint32_t)bx << 16) | cx;
        uint32_t a = east - west, c = pt - west;
        ax = (uint16_t)a;
        dx = (uint16_t)(a >> 16);
        return a < c;
    }
}

/* .text+0x1d0e -- is the 24-bit point at ebx (latitude at +0..2 south and
 * +3..5 north... as compared below) inside GLOBAL's search box?  Carry when
 * it is not.  Latitudes against the box's halved, longitudes with the low
 * byte dropped. */
static int point24_out(void)
{
    edi = G;
    edx = sar32(M32(edi + 0x3e4), 1);
    eax = ((uint32_t)(int32_t)(int8_t)M8(ebx + 5) << 16) | M16(ebx + 3);
    if ((int32_t)(edx - eax) < 0)
        return 1;
    edx = sar32(M32(edi + 0x3e8), 1);
    eax = ((uint32_t)(int32_t)(int8_t)M8(ebx + 2) << 16) | M16(ebx);
    if ((int32_t)(edx - eax) >= 0)
        return 1;
    ecx = M32(ebx + 8) & 0xffffff00u;
    edx = M32(edi + 0x3f0) - ecx;
    eax = M32(edi + 0x3ec) - ecx;
    if (eax < edx)
        return 0;
    eax -= edx;
    eax = (M32(ebx + 5) & 0xffffff00u) - ecx;
    {
        int cf = eax < edx;
        eax -= edx;
        return cf;
    }
}

/* .text+0x4083 -- a BCD frequency in bx (four digits, the leading 1 implied)
 * to a channel: (digits - 800) / 5, into +0x951 and ax. */
static uint16_t channel(void)
{
    uint16_t bx = (uint16_t)ebx, dx, ax, cx;
    uint8_t al, ah;

    dx = (uint16_t)((bx << 4) | (bx >> 12));
    al = (uint8_t)bx & 0xf;
    ah = (uint8_t)(dx >> 8) & 0xf;
    cx = (uint8_t)(ah * 10 + al);                       /* aad */
    al = (uint8_t)(bx >> 8) & 0xf;
    ah = (uint8_t)dx & 0xf;
    al = (uint8_t)(ah * 10 + al);
    ax = (uint16_t)(al * 100);
    ax = (uint16_t)(ax + cx - 0x320);
    ebx = (ebx & 0xffff0000u) | 5;
    ax = udiv16(0, ax, 5, NULL);
    ax &= 0xff;
    M8(D + 0x951) = (uint8_t)ax;
    eax = ax;
    ecx = cx;
    return ax;
}

/* .text+0x3b0b -- a marker's or localiser's coverage gate: keep eax when the
 * aircraft (+0x30a its distance, +0x30e its height, from strength(1)) is
 * inside the envelope for the navaid's range class w, else 0.  Piecewise
 * straight lines, as retail has them. */
static uint32_t gate(uint16_t w)
{
    int32_t h = (int32_t)M32(D + 0x30e), d = (int32_t)M32(D + 0x30a);
    uint32_t e;

    if (h > 0x5f40)
        return 0;
    if (h <= 0x98) {
        if (d <= 0x15b4)
            return eax;
        if (h <= 0xf)
            return 0;
        e = udiv32((uint32_t)h, 0, (uint32_t)d);
        return e < 0x6bdbc0 ? 0 : eax;
    }
    if (w == 0)
        return eax;
    if (w <= 0x19) {
        if (d > 0xfd34)
            return 0;
        if (h > 0x7be) {
            if (h > 0x17d0)
                return 0;
            e = imul_mid((uint32_t)h - 0x7be, 0xfff0cf49u) + 0xfd34;
        } else {
            if (d <= 0xb496)
                return eax;
            e = imul_mid((uint32_t)h - 0x98, 0xf30b7) + 0xb496;
        }
        return (int32_t)e < d ? 0 : eax;
    }
    if (w < 0x32) {
        if (d > 0x169b8)
            return 0;
        if (h > 0xb50) {
            if (h > 0x23b8)
                return 0;
            e = imul_mid((uint32_t)h - 0xb50, 0xfff27f5eu) + 0x169b8;
        } else {
            if (d <= 0x12160)
                return eax;
            e = imul_mid((uint32_t)h - 0x98, 0xd80a2) + 0x12160;
        }
        return (int32_t)e < d ? 0 : eax;
    }
    if (d > 0x51630)
        return 0;
    if (h <= 0x1144) {
        if (d <= 0x169b8)
            return eax;
        e = imul_mid((uint32_t)h - 0x98, 0x293b14) + 0x169b8;
    } else if (h <= 0x2581) {
        if (d <= 0x2d370)
            return eax;
        e = imul_mid((uint32_t)h - 0x1144, 0xe4bf8) + 0x3f4d0;
    } else {
        if (h > 0x5f40)
            return 0;
        if (d <= 0x2d370)
            return eax;
        e = imul_mid((uint32_t)h - 0x2581, 0xfff55715u) + 0x51630;
    }
    return (int32_t)e < d ? 0 : eax;
}

/* .text+0x3dc7 -- a glide slope's gate: keep eax while the height above it
 * (+0x316, from strength(0)) is within 0..0xbe8 and the slope's band at the
 * distance +0x312. */
static uint32_t slope_gate(void)
{
    uint32_t keep = eax;
    uint64_t m;

    if (M32(D + 0x316) > 0xbe8)
        return 0;
    m = (uint64_t)M32(D + 0x312) * 0xd6a98a2u;
    edx = (uint32_t)(m >> 32) + 0x55b;
    if ((int32_t)M32(D + 0x316) > (int32_t)edx)
        return 0;
    edx -= 0x8ed;
    if ((int32_t)edx >= 0 && (int32_t)M32(D + 0x316) < (int32_t)edx)
        return 0;
    return keep;
}

/* .text+0x403d -- a navaid record may be followed by a 0x14 record marked
 * 0x1234: its 10 bytes go to +0x99e (else +0x99e is 0). */
static void navaid_extra(void)
{
    uint32_t keep = esi;

    M16(D + 0x99e) = 0;
    esi = (M8(D + 0xad2) & 0x80) ? 0x36 : 0x2c;
    if (M8(S_BUF + esi) == 0x14 && M16(D + 0xad3 + esi) == 0x1234) {
        uint32_t k;
        esi += D + 0xad5;
        for (k = 0; k < 0xa; k++)
            M8(D + 0x99e + k) = M8(esi + k);
    }
    esi = keep;
}

/* The x87 sequence at .text+0x3f39: fild the two 48-bit differences as
 * 64-bit values (low word zeroed), fpatan, fmul by the double at +0x10c,
 * fistp -- the bearing, rounded under the current control word. */
static int32_t bearing(uint32_t y_lo, uint32_t y_hi, uint32_t x_lo, uint32_t x_hi)
{
    uint64_t y = ((uint64_t)y_hi << 32) | y_lo, x = ((uint64_t)x_hi << 32) | x_lo;
    int32_t r;

    __asm__ volatile("fildq %1\n\tfildq %2\n\tfpatan\n\tfmull %3\n\tfistpl %0\n\tfwait"
                     : "=m"(r)
                     : "m"(y), "m"(x), "m"(*(volatile double *)(uintptr_t)(D + 0x10c))
                     : "st", "st(1)");
    return r;
}

/* .text+0x3e21 -- consider the localiser or marker record at +0xace for
 * +0x2c: unpack it, then keep it in +0x952 (0x4c bytes) if it is the best
 * so far by strength through its gate -- a localiser (+0xad2 bit 7) also by
 * how close its course is to the bearing from it to the best so far's. */
static void consider_beacon(void)
{
    uint16_t ax;

    esi = D + 0xad3;
    ebx = (ebx & 0xffffff00u) | M8(D + 0xacf);
    if (M8(D + 0xad2) & 0x80) {
        uint8_t bl = (uint8_t)(M8(D + 0xacf) * 2);
        uint16_t p = (uint16_t)(0xe1 * bl);
        p = (uint16_t)(p + p);
        ebx = (ebx & 0xffffff00u) | (uint8_t)(p >> 8);
        eax = (eax & 0xffff0000u) | p;
    }
    unpack_point();
    navaid_extra();
    eax = (eax & 0xffff0000u) | 2;
    if (M8(D + 0xad2) & 0x80)
        eax = strength(0);
    else
        eax = strength(1);
    if (!(M8(D + 0xad2) & 0x80)) {
        if (eax < 0x8c)
            goto next;
        eax = gate(M16(esi + 0x12));
        ax = (uint16_t)eax;
        if (ax <= M16(D + 0x964))
            goto next;
        goto take;
    }
    if (eax < 0x82)
        goto next;
    eax = slope_gate();
    ax = (uint16_t)eax;
    if (M16(D + 0x964) < 0x82)
        goto take;
    if (ax == 0) {
        ax = M16(D + 0x964);
        eax = (eax & 0xffff0000u) | ax;
        goto next;
    }
    edi = G;
    M16(edi + 0x37e) = 1;
    if (M16(edi + 0x380) == 1)
        goto next;
    if (M16(edi + 0x380) > 1)
        goto take;
    {
        uint8_t in[8], pos[8], out[8];
        uint16_t dx, bx, cx;
        uint32_t ylo, yhi, xlo, xhi;
        int32_t b;
        int cf;

        dx = (uint16_t)(M16(D + 0x958) - M16(D + 0x9ae));
        cf = M16(D + 0x958) < M16(D + 0x9ae);
        M16(ADDR(in) + 2) = dx;
        M32(ADDR(in) + 4) = M32(D + 0x95a) - M32(D + 0x9b0) - cf;
        M16(ADDR(pos) + 2) = M16(D + 0x9a8);
        M32(ADDR(pos) + 4) = M32(D + 0x9aa);
        UTIL_TO_M(ADDR(in), ADDR(pos), ADDR(out));
        M16(ADDR(out)) = 0;
        ylo = M32(ADDR(out));
        yhi = M32(ADDR(out) + 4);
        dx = (uint16_t)(M16(D + 0x952) - M16(D + 0x9a8));
        cf = M16(D + 0x952) < M16(D + 0x9a8);
        xhi = M32(D + 0x954) - M32(D + 0x9aa) - cf;
        xlo = (uint32_t)dx << 16;
        b = bearing(ylo, yhi, xlo, xhi);
        eax = (uint32_t)b;
        if ((int32_t)eax < 0)
            eax += 0xfff0;
        edx = M16(D + 0xadb);
        eax -= edx;
        edx = (int32_t)eax < 0 ? 0xffffffffu : 0;
        eax = (eax ^ edx) - edx;
        bx = ax;
        cx = M16(D + 0x964);
        if (eax < 0x3ffc) {
            uint16_t t = cx;
            cx = bx;
            bx = t;
        }
        if (bx > cx)
            goto next;
        eax = (eax & 0xffff0000u) | ax;
    }
take:
    {
        uint32_t k;
        M16(D + 0x964) = ax;
        for (k = 0; k < 0x12; k += 2)
            M16(D + 0x952 + k) = M16(D + 0x9a8 + k);
        M16(D + 0x966) = M16(D + 0xad0);
        M8(D + 0x968) = M8(D + 0xad2);
        M16(D + 0x969) = M8(D + 0xacf);
        for (k = 0; k < 0x29; k++)
            M8(D + 0x96b + k) = M8(D + 0xadb + k);
        for (k = 0; k < 0xa; k++)
            M8(D + 0x994 + k) = M8(D + 0x99e + k);
    }
next:
    M32(D + 0x932) += (M8(D + 0xad2) & 0x80) ? 0x36 : 0x2c;
}

/* ---- lookups: the slots ------------------------------------------------------------ */

/* The lookups share one iteration (+0x2e5): 0 the next file in the box with
 * the section the lookup wants; 2 that section's latitude index; 3 its
 * records.  Each slot opens with begin() and the box 312,368 m (0x4c830)
 * round the position. */

/* .text+0x40dd -- +0x28: the strongest ILS with this frequency and channel:
 * 0x2f bytes into out; returns its strength. */
static uint32_t FS98_STDCALL fe_ils(uint16_t freq, uint8_t ch, uint32_t pos, uint32_t out)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    M16(D + 0x94e) = freq;
    M8(D + 0x950) = ch;
    M32(D + 0x94a) = pos;
    begin();
    edi = G;
    fe_search(M32(D + 0x94a), 0x4c830, edi + 0x3e4);
    M16(D + 0x9d2) = 0;
    for (;;) {
        uint16_t st = M16(S_SEARCH);
        if (st == 0) {
            if (next_file()) {
                uint32_t k;
                for (k = 0; k < 0x2f; k++)
                    M8(out + k) = M8(D + 0x9c0 + k);
                esi = D + 0x9c0;
                eax = (uint32_t)(int32_t)(int16_t)M16(D + 0x9d2);
                goto done;
            }
            if (file_out())
                continue;
            eax = M32(esi + 0x14c);
            M32(S_BASE) = eax;
            M32(D + 0x92a) = eax;
            if (eax == 0)
                continue;
            open_file();
            M16(S_SEARCH) = 2;
        } else if ((uint8_t)st == 2) {
            for (;;) {
                if (read_index_92a() == 0) {
                    close_file();
                    M16(S_SEARCH) = 0;
                    goto again;
                }
                if (!index_out())
                    break;
            }
            M32(D + 0x932) = M32(D + 0xad3) + M32(S_BASE);
            M16(S_SEARCH) = 3;
        } else if ((uint8_t)st == 3) {
            for (;;) {
                ecx = 0x2a;
                esi = D + 0x932;
                if (read_rec() == 0) {
                    M32(D + 0x92a) += 9;
                    M16(S_SEARCH) = 2;
                    break;
                }
                M32(D + 0x932) += 0x2a;
                if (M16(D + 0x94e) != M16(D + 0xacf) || M8(D + 0x950) != M8(D + 0xad1))
                    continue;
                edi = G;
                esi = edi + 0x3e4;
                edi = D + 0xad2;
                if (point48_out())
                    continue;
                esi = D + 0xad2;
                ebx = (ebx & 0xffffff00u) | M8(D + 0xada);
                unpack_point();
                eax = (eax & 0xffff0000u) | 4;
                eax = strength(0);
                if (eax >= 0x11a && (int16_t)eax > (int16_t)M16(D + 0x9d2)) {
                    uint32_t k;
                    M16(D + 0x9d2) = (uint16_t)eax;
                    for (k = 0; k < 0x12; k += 2)
                        M16(D + 0x9c0 + k) = M16(D + 0x9a8 + k);
                    for (k = 0; k < 0x1d; k++)
                        M8(D + 0x9d4 + k) = M8(D + 0xadb + k);
                }
            }
        } else {
            eax = 0;
            goto done;
        }
again:  ;
    }
done:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x4356 -- +0x24: the strongest VOR with this frequency: its record
 * (to +0xb26) into *out; returns its strength. */
static uint32_t FS98_STDCALL fe_vor(uint16_t freq, uint32_t pos, uint32_t out)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    M16(D + 0x94e) = freq;
    M32(D + 0x94a) = pos;
    begin();
    edi = G;
    fe_search(M32(D + 0x94a), 0x4c830, edi + 0x3e4);
    M32(D + 0x9f1) = 0;
    for (;;) {
        uint16_t st = M16(S_SEARCH);
        if (st == 0) {
            if (next_file()) {
                M32(out) = D + 0xb26;
                eax = M32(D + 0x9f1);
                goto done;
            }
            if (file_out())
                continue;
            eax = M32(esi + 0x148);
            M32(S_BASE) = eax;
            M32(D + 0x92a) = eax;
            if (eax == 0)
                continue;
            open_file();
            M16(S_SEARCH) = 2;
        } else if ((uint8_t)st == 2) {
            for (;;) {
                if (read_index_92a() == 0) {
                    close_file();
                    M16(S_SEARCH) = 0;
                    goto again;
                }
                if (!index_out())
                    break;
            }
            M32(D + 0x932) = M32(D + 0xad3) + M32(S_BASE);
            M16(S_SEARCH) = 3;
        } else if ((uint8_t)st == 3) {
            for (;;) {
                ecx = 0xc;
                esi = D + 0x932;
                if (read_rec() == 0) {
                    M32(D + 0x92a) += 9;
                    M16(S_SEARCH) = 2;
                    break;
                }
                M32(D + 0xa3b) = M32(D + 0x932);
                M32(D + 0x932) += M16(D + 0xad8);
                if (M16(D + 0xacf) != M16(D + 0x94e))
                    continue;
                edi = G;
                edx = ((uint32_t)M16(D + 0xad5) << 16) | ((uint32_t)M8(D + 0xad4) << 8);
                if ((int32_t)(M32(edi + 0x3ec) - edx) < 0)
                    continue;
                if ((int32_t)(M32(edi + 0x3f0) - edx) >= 0)
                    continue;
                esi = D + 0xad1;
                ebx = (ebx & 0xffffff00u) | M8(D + 0xad7);
                unpack_point();
                eax = (eax & 0xffff0000u) | 8;
                eax = strength(0);
                if (eax < 0x11a || eax <= M32(D + 0x9f1))
                    continue;
                M32(D + 0x9f1) = eax;
                SEEK_FILE(M32(D + 0xa3b));
                ecx = M16(D + 0xad8);
                READ_FILE(D + 0xb26, ecx);
            }
        } else {
            eax = 0;
            goto done;
        }
again:  ;
    }
done:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x48cb -- +0x2c: the best localiser or marker on this frequency:
 * 0x4c bytes into out; returns its strength.  Files carry a table by
 * channel (+0x110, channels +0x114..+0x116) pointing at each channel's
 * latitude index.  A best that is a localiser but weaker than 0x9b clears
 * GLOBAL +0x37e and is reported as 0. */
static uint32_t FS98_STDCALL fe_beacon(uint16_t freq, uint32_t pos, uint32_t out)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    M16(D + 0x94e) = freq;
    M32(D + 0x94a) = pos;
    begin();
    edi = G;
    fe_search(M32(D + 0x94a), 0x4c830, edi + 0x3e4);
    M16(D + 0x964) = 0;
    for (;;) {
        uint16_t ax;

        if (next_file()) {
            uint32_t k;
            ecx = 0x4c;
            if ((M8(D + 0x968) & 0x80) && (int16_t)M16(D + 0x964) < 0x9b) {
                edi = G;
                M16(D + 0x964) = 0;
                M16(edi + 0x37e) = 0;
            }
            for (k = 0; k < 0x4c; k++)
                M8(out + k) = M8(D + 0x952 + k);
            eax = M16(D + 0x964);
            break;
        }
        if (file_out())
            continue;
        ebx = M32(esi + 0x110);
        if (ebx == 0)
            continue;
        M32(S_BASE) = ebx;
        ebx = (ebx & 0xffff0000u) | M16(D + 0x94e);
        ax = channel();
        if (ax < M16(esi + 0x114) || ax > M16(esi + 0x116))
            continue;
        open_file();
        ebx = M32(S_BASE);
        eax = (uint16_t)((uint8_t)(M8(D + 0x951) - M8(esi + 0x114)) * M8(D + 0x114));
        eax = (uint16_t)(eax + 1);
        ebx += eax;
        M32(D + 0xa3f) = ebx;
        SEEK_FILE(M32(D + 0xa3f));
        READ_FILE(D + 0x92a, 4);
        eax = M32(D + 0x92a);
        if (eax == 0) {
            close_file();
            continue;
        }
        M32(D + 0x92a) = eax + M32(S_BASE);
        for (;;) {
            if (read_index_92a() == 0) {
                close_file();
                break;
            }
            edi = G;
            eax = M32(edi + 0x3e4) >> 9;
            if ((int16_t)eax > (int16_t)M16(D + 0xacf)) {
                eax = M32(edi + 0x3e8) >> 9;
                if ((int16_t)eax <= (int16_t)M16(D + 0xad1)) {
                    M32(D + 0x932) = M32(D + 0xad3) + M32(S_BASE);
                    for (;;) {
                        M8(D + 0xafa) = 0;
                        M8(D + 0xb04) = 0;
                        ecx = 0x47;
                        esi = D + 0x932;
                        if (read_rec() == 0)
                            break;
                        consider_beacon();
                    }
                }
            }
            M32(D + 0x92a) += 9;
        }
    }
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x41f2 -- the ground's height at +0x94a from the elevation grid just
 * read (+0xace rows, +0xad0 columns): the nearest sample, by rounded
 * fractions of its latitude (over +0x117) and longitude. */
static uint16_t height_sample(void)
{
    uint32_t v;
    uint16_t ax, dx, di;
    uint32_t p;

    esi = M32(D + 0x94a);
    v = (((uint32_t)M16(esi + 6) << 16) | M16(esi + 4)) + 0x989d56u;
    v <<= 7;
    ax = udiv16((uint16_t)(v >> 16), (uint16_t)v, M16(D + 0x117), NULL);
    p = (uint32_t)ax * (uint16_t)(M16(D + 0xad0) - 1) + 0x8000;
    di = (uint16_t)(p >> 16);
    p = (uint32_t)M16(esi + 0xe) * M16(S_BUF) + 0x8000;
    dx = (uint16_t)(p >> 16);
    if ((int16_t)dx >= (int16_t)M16(S_BUF))
        dx = 0;
    {
        uint16_t lo = (uint16_t)((uint32_t)M16(D + 0xad0) * dx);
        uint16_t hi = (uint16_t)(((uint32_t)M16(D + 0xad0) * dx) >> 16);
        int cf = (uint16_t)(lo + di) < lo;
        lo = (uint16_t)(lo + di);
        hi = (uint16_t)(hi + cf);
        cf = lo >> 15;
        lo = (uint16_t)(lo << 1);
        hi = (uint16_t)((hi << 1) | cf);
        cf = (uint16_t)(lo + 8) < lo;
        lo = (uint16_t)(lo + 8);
        hi = (uint16_t)(hi + cf);
        cf = (uint16_t)(M16(D + 0xa3b) + lo) < lo;
        M16(D + 0xa3b) = (uint16_t)(M16(D + 0xa3b) + lo);
        M16(D + 0xa3d) = (uint16_t)(M16(D + 0xa3d) + hi + cf);
    }
    SEEK_FILE(M32(D + 0xa3b));
    READ_FILE(S_BUF, 2);
    eax = M16(S_BUF);
    return (uint16_t)eax;
}

/* .text+0x40d0 -- +0x30: the ground's height at pos, from the elevation
 * section (+0x16c) of the file whose section has the highest priority
 * (+0xad2); every file with one is opened, in the box or not. */
static uint32_t FS98_STDCALL fe_ground(uint32_t pos)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    M32(D + 0x94a) = pos;
    M32(D + 0x9f5) = 0;
    M16(D + 0x9f9) = 0;
    begin();
    while (!next_file()) {
        eax = M32(esi + 0x16c);
        M32(D + 0xa3b) = eax;
        if (eax == 0)
            continue;
        open_file();
        SEEK_FILE(M32(D + 0xa3b));
        READ_FILE(S_BUF, 8);
        eax = M32(D + 0xad2);
        if (!((int32_t)M32(D + 0x9f5) > (int32_t)eax)) {
            M32(D + 0x9f5) = eax;
            M16(D + 0x9f9) = height_sample();
        }
        close_file();
    }
    eax = (uint32_t)(int32_t)(int16_t)M16(D + 0x9f9);
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x4656 -- +0x20: the next record of the last search (kinds 0 and 1;
 * 0 starts over), from each file's +0x164 section: *out the record (read to
 * +0xace), *len its size; 0 at the end.  Kinds 2 and 3 set +0xf1a (1 and
 * 0), which lets +0x70 accept version 1000..1999 files. */
static uint32_t FS98_STDCALL fe_next(uint16_t kind, uint32_t out, uint32_t len)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    if (kind == 2 || kind == 3) {
        M16(D + 0xf1a) = kind == 2 ? 1 : 0;
        eax = 1;
        goto done;
    }
    if (kind > 3) {
        eax = 0;
        goto done;
    }
    if (kind == 0)
        begin();
    for (;;) {
        uint16_t st = M16(S_SEARCH);
        if (st == 0) {
            if (next_file())
                goto end;
            if (file_out())
                continue;
            eax = M32(esi + 0x164);
            M32(S_BASE) = eax;
            M32(D + 0x92a) = eax;
            if (eax == 0)
                continue;
            open_file();
            M16(S_SEARCH) = 2;
        } else if ((uint8_t)st == 2) {
            for (;;) {
                if (read_index_92a() == 0) {
                    close_file();
                    M16(S_SEARCH) = 0;
                    goto again;
                }
                if (!index_out())
                    break;
            }
            M32(D + 0x932) = M32(D + 0xad3) + M32(S_BASE);
            M16(S_SEARCH) = 3;
        } else if ((uint8_t)st == 3) {
            for (;;) {
                ecx = 0xe;
                esi = D + 0x932;
                if (read_rec() == 0) {
                    M32(D + 0x92a) += 9;
                    M16(S_SEARCH) = 2;
                    goto again;
                }
                M32(D + 0xa3b) = M32(D + 0x932);
                eax = M8(D + 0xacf);
                M32(D + 0x932) += eax;
                if (M8(S_BUF) < 0x64) {
                    uint16_t ax, dx;
                    int cf;
                    edi = G;
                    ax = M16(edi + 0x3ec);
                    dx = M16(edi + 0x3ee);
                    cf = (uint8_t)(ax >> 8) < M8(D + 0xad3);
                    dx = (uint16_t)(dx - M16(D + 0xad4) - cf);
                    if ((int16_t)dx < 0)
                        continue;
                    ax = M16(edi + 0x3f0);
                    dx = M16(edi + 0x3f2);
                    cf = (uint8_t)(ax >> 8) < M8(D + 0xad3);
                    dx = (uint16_t)(dx - M16(D + 0xad4) - cf);
                    if ((int16_t)dx >= 0)
                        continue;
                    eax = (eax & 0xffff0000u) | (uint16_t)(int16_t)(int8_t)M8(D + 0xad2);
                    cf = (eax >> 16) & 1;                   /* shl eax,16 */
                    eax = (eax << 16) | M16(D + 0xad0);
                    eax = eax + eax + cf;                   /* adc eax,eax */
                    if ((int32_t)(M32(edi + 0x3e4) - eax) < 0)
                        continue;
                    if ((int32_t)(M32(edi + 0x3e8) - eax) >= 0)
                        continue;
                } else {
                    ebx = D + 0xad0;
                    if (point24_out())
                        continue;
                }
                SEEK_FILE(M32(D + 0xa3b));
                ecx = M8(D + 0xacf);
                READ_FILE(S_BUF, ecx);
                M32(out) = S_BUF;
                M32(len) = M32(S_GOT);
                eax = 1;
                goto done;
            }
        } else {
            goto end;
        }
again:  ;
    }
end:
    M16(S_SEARCH) = 0x63;
    eax = 0;
done:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x4452 -- +0x4c: start a listing (FACIL's). */
static uint32_t FS98_STDCALL fe_list_begin(void)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    begin();
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x4631 -- +0x50: start at a given point: +0x932, then the area and
 * file index. */
static uint32_t FS98_STDCALL fe_list_at(uint32_t at, uint32_t area, uint32_t index)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    M32(D + 0x932) = at;
    begin();
    M32(S_AREA) = area;
    M32(S_INDEX) = index;
    eax = index;
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x44a6 -- +0x34: the next record of every file's +0x140 section, in
 * the box or not: *out the record at +0xace with its file's section offset,
 * area and index after it; *len what was read. */
static uint32_t FS98_STDCALL fe_list_next(uint32_t out, uint32_t len)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    for (;;) {
        uint16_t st = M16(S_SEARCH);
        if (st == 0) {
            if (next_file())
                goto end;
            eax = M32(esi + 0x140);
            M32(S_BASE) = eax;
            M32(D + 0xa43) = eax;
            if (eax == 0)
                goto close;
            open_file();
            M16(S_SEARCH) = 2;
        } else if ((uint8_t)st == 2) {
            uint32_t n;
            ecx = 7;
            esi = D + 0xa43;
            if (read_rec() == 0)
                goto close;
            M32(D + 0xa43) += 7;
            SEEK_FILE(M32(D + 0xa43));
            ecx = M16(D + 0xacf) - 7;
            n = ecx;
            READ_FILE(D + 0xada, ecx);
            M32(D + 0xad6) = M32(S_BASE) + M32(D + 0xad1);
            M32(S_BUF) = M32(S_AREA);
            M32(D + 0xad2) = M32(S_INDEX);
            M32(D + 0xa43) += n;
            esi = S_BUF;
            M32(out) = esi;
            M32(len) = M32(S_GOT);
            eax = 1;
            goto done;
        } else {
            goto end;
        }
        continue;
close:
        close_file();
        M16(S_SEARCH) = 0;
    }
end:
    M16(S_SEARCH) = 0x63;
    eax = 0;
done:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x46a3 -- +0x38: the next record from +0x932 in the file +0x50 set:
 * *out the record at +0xace, *len what was read. */
static uint32_t FS98_STDCALL fe_list_read(uint32_t out, uint32_t len)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    for (;;) {
        uint16_t st = M16(S_SEARCH);
        if (st == 0) {
            open_file();
            M16(S_SEARCH) = 1;
        } else if ((uint8_t)st == 1) {
            uint32_t n;
            ecx = 0x1b;
            esi = D + 0x932;
            if (read_rec() == 0) {
                close_file();
                M16(S_SEARCH) = 0x63;
                continue;
            }
            M32(D + 0x932) += 3;
            SEEK_FILE(M32(D + 0x932));
            ecx = M16(D + 0xacf) - 3;
            n = ecx;
            READ_FILE(S_BUF, ecx);
            M32(D + 0x932) += n;
            esi = S_BUF;
            M32(out) = esi;
            M32(len) = M32(S_GOT);
            eax = 1;
            break;
        } else {
            M16(S_SEARCH) = 0x63;
            eax = 0;
            break;
        }
    }
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x47e9 -- +0x3c: the object whose 16-byte id is at id, from the
 * object section (+0x13c, its id range at +0x154..+0x163) of the first file
 * that may hold it: up to size bytes into buf, *got what was read; -1 with
 * *got the size needed when it will not fit.  Without `more` the search
 * starts over.  Only the id's last dword decides a match. */
static uint32_t FS98_STDCALL fe_object(uint32_t id, uint32_t buf, uint16_t more,
                                       uint32_t size, uint32_t got)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    if (more == 0)
        begin();
    for (;;) {
        uint16_t st = M16(S_SEARCH);
        if (st == 0) {
            uint32_t lo, hi;
            int cf;
            if (next_file()) {
                eax = 0;
                goto done;
            }
            ebx = id;
            edi = esi + 0x154;
            lo = M32(ebx);
            cf = lo < M32(edi);
            hi = M32(ebx + 4) - M32(edi + 4) - cf;
            if ((int32_t)hi < 0)
                continue;
            edi = esi + 0x15c;
            lo = M32(edi);
            cf = lo < M32(ebx);
            hi = M32(edi + 4) - M32(ebx + 4) - cf;
            if ((int32_t)hi < 0)
                continue;
            eax = M32(esi + 0x13c);
            M32(S_BASE) = eax;
            M32(D + 0x932) = eax;
            open_file();
            M16(S_SEARCH) = 2;
        } else if ((uint8_t)st == 2) {
            uint32_t a, b, c, d;
            int cf;
            SEEK_FILE(M32(D + 0x932));
            READ_FILE(S_BUF, 0x14);
            M32(D + 0x932) += 0x14;
            ecx = M32(S_BUF);
            if (ecx == 0) {
                close_file();
                M16(S_SEARCH) = 0;
                continue;
            }
            ebx = id;
            a = M32(D + 0xad2);
            cf = a < M32(ebx);
            b = M32(D + 0xad6);
            d = b - M32(ebx + 4) - cf;
            cf = (uint64_t)b < (uint64_t)M32(ebx + 4) + cf;
            c = M32(D + 0xada);
            d = c - M32(ebx + 8) - cf;
            cf = (uint64_t)c < (uint64_t)M32(ebx + 8) + cf;
            d = M32(D + 0xade) - M32(ebx + 0xc) - cf;
            if (d != 0)
                continue;
            ecx += M32(S_BASE);
            M32(D + 0x932) = ecx;
            M16(S_SEARCH) = 3;
        } else if ((uint8_t)st == 3) {
            SEEK_FILE(M32(D + 0x932));
            READ_FILE(S_BUF, 0x13);
            eax = M16(D + 0xadf) - 0x13;
            if (eax > size) {
                if (got != 0)
                    M32(got) = eax;
                edi = G;
                if (!(M16(edi + 0x3e0) & 1))
                    close_file();
                eax = 0xffffffffu;
                goto done;
            }
            ecx = (uint16_t)eax;
            READ_FILE(buf, ecx);
            close_file();
            M32(got) = M32(S_GOT);
            eax = 1;
            goto done;
        } else {
            eax = 0;
            goto done;
        }
    }
done:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* .text+0x4ad4 -- is the dynamic block's box (+0xacf south, +0xad1 north,
 * +0xad3 west, +0xad5 east, words) clear of the search box?  Carry when it
 * is. */
static int block_out(void)
{
    edi = G;
    eax = (uint32_t)(int32_t)(int16_t)M16(D + 0xad1) << 9;
    if ((int32_t)(eax - M32(edi + 0x3e4)) >= 0)
        return 1;
    eax = (uint32_t)(int32_t)(int16_t)M16(D + 0xacf) << 9;
    if ((int32_t)(eax - M32(edi + 0x3e8)) < 0)
        return 1;
    eax = (uint32_t)M16(D + 0xad5) << 16;
    if ((int32_t)(eax - M32(edi + 0x3ec)) >= 0)
        return 1;
    eax = (uint32_t)M16(D + 0xad3) << 16;
    if ((int32_t)(eax - M32(edi + 0x3f0)) < 0)
        return 1;
    return 0;
}

/* .text+0x4ac3 -- step +0x932 past the block (its size at +0xad7, signed). */
static void skip_block(void)
{
    M32(D + 0x932) += (uint32_t)(int32_t)(int16_t)M16(D + 0xad7) + 0xb;
}

/* .text+0x4a81 -- read ecx bytes of the block, from where the file is, into
 * buf. */
static void read_block(uint32_t buf)
{
    M32(D + 0xa3b) += 0xb;
    READ_FILE(buf, ecx);
}

/* The caller's registers at +0x40's entry, kept by fe_dynamic_entry (below):
 * the block-too-big path needs them. */
uint32_t fe_in_ebx, fe_in_esi, fe_in_edi, fe_in_ebp, fe_shifted;

/* .text+0x4b24 -- +0x40: the next dynamic-scenery block in the box from each
 * file's +0x150 section -- of type 4 for kinds 0 and 1, else 5 (kind 0 starts
 * over): up to size bytes into buf.  One that fits returns 1 and leaves *out
 * alone.  One bigger than size returns -1 with *out its size -- and breaks
 * the calling convention: retail calls .text+0x4a81 there without pushing
 * the buffer, so the read goes to the address in the caller's edi, and the
 * `ret 4` that pops the missing argument leaves the slot's epilogue a dword
 * off -- it returns with the caller's esi in edi, ebx in esi and ebp in ebx.
 * DYNAMIC probes with a 1-byte buffer first, so it meets this on every load.
 * fe_dynamic_entry reproduces the registers; the read is done here. */
uint32_t FS98_STDCALL fe_dynamic(uint16_t kind, uint32_t buf, uint32_t size, uint32_t out)
{
    SAVE_REGS;

    EnterCriticalSection(CS);
    M8(D + 0xa21) = kind <= 1 ? 4 : 5;
    if (kind == 0)
        begin();
    for (;;) {
        uint16_t st = M16(S_SEARCH);
        if (st == 0) {
            if (next_file())
                goto end;
            if (file_out())
                continue;
            eax = M32(esi + 0x150);
            M32(S_BASE) = eax;
            M32(D + 0x92a) = eax;
            if (eax == 0)
                continue;
            open_file();
            M16(S_SEARCH) = 2;
        } else if ((uint8_t)st == 2) {
            for (;;) {
                if (read_index_92a() == 0) {
                    close_file();
                    M16(S_SEARCH) = 0;
                    goto again;
                }
                if (!index_out())
                    break;
            }
            M32(D + 0x932) = M32(D + 0xad3) + M32(S_BASE);
            M16(S_SEARCH) = 3;
        } else if ((uint8_t)st == 3) {
            for (;;) {
                uint8_t al;
                ecx = 0xb;
                esi = D + 0x932;
                al = read_rec();
                if (al == 0) {
                    M32(D + 0x92a) += 9;
                    M16(S_SEARCH) = 2;
                    goto again;
                }
                M32(D + 0xa3b) = M32(D + 0x932);
                if (al != M8(D + 0xa21) || block_out()) {
                    skip_block();
                    continue;
                }
                ebx = M16(D + 0xad7);
                eax = size;
                ecx = eax;
                if (eax < ebx) {
                    read_block(fe_in_edi);
                    fe_shifted = 1;
                    edi = G;
                    if (!(M16(edi + 0x3e0) & 1))
                        skip_block();
                    eax = M16(D + 0xad7);
                    M32(out) = eax;
                    close_file();
                    eax = 0xffffffffu;
                    goto done;
                }
                skip_block();
                ecx = (uint32_t)(int32_t)(int16_t)M16(D + 0xad7);
                read_block(buf);
                close_file();
                eax = 1;
                goto done;
            }
        } else {
            goto end;
        }
again:  ;
    }
end:
    M16(S_SEARCH) = 0x63;
    eax = 0;
done:
    LeaveCriticalSection(CS);
    RESTORE_REGS;
    return eax;
}

/* +0x40's entry: keeps the caller's ebx, esi, edi and ebp for fe_dynamic,
 * and when fe_dynamic took the too-big path, returns with them shifted as
 * retail's misaligned epilogue leaves them. */
__asm__(".text\n"
        "_fe_dynamic_entry:\n\t"
        "movl %ebx, _fe_in_ebx\n\t"
        "movl %esi, _fe_in_esi\n\t"
        "movl %edi, _fe_in_edi\n\t"
        "movl %ebp, _fe_in_ebp\n\t"
        "movl $0, _fe_shifted\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "call _fe_dynamic@16\n\t"
        "cmpl $0, _fe_shifted\n\t"
        "je 1f\n\t"
        "movl _fe_in_esi, %edi\n\t"
        "movl _fe_in_ebx, %esi\n\t"
        "movl _fe_in_ebp, %ebx\n"
        "1:\n\t"
        "ret $16\n");
void fe_dynamic_entry(void);

/* ---- the entries ------------------------------------------------------------------ */

/* .text+0x35da -- +0x44: abandon the scene stream (0) or a lookup (else),
 * closing the BGL. */
static uint32_t FS98_STDCALL fe_reset(uint16_t which)
{
    EnterCriticalSection(CS);
    if (which == 0) {
        M16(D + 0x922) = 0x63;
        free_grid();
    } else {
        M16(S_SEARCH) = 0x63;
    }
    close_file();
    LeaveCriticalSection(CS);
    return eax;
}

/* .text+0xb470 -- the C half's init: nothing. */
static void lib_init(void)
{
}

/* .text+0xb480 -- the C half's deinit: the data of the two lists +0x3208 and
 * +0x320c point at, through host +0x2fc. */
static void lib_deinit(void)
{
    HOST(0x2fc, fe_f1)(M32(D + 0x3208));
    HOST(0x2fc, fe_f1)(M32(D + 0x320c));
}

/* .text+0x20 -- +0x04 */
static void fe_init(void)
{
    CHAIN_ADD(3, ADDR(fe_frame));
    H_LOADSTRING(0x9003, D + 0x81e, 0x100);
    H_LOADSTRING(0x9006, D + 0x5cc0, 0x100);
    edi = G;
    M16(edi + 0x444) |= 0x32;
    InitializeCriticalSection(CS);
    lib_init();
}

/* .text+0xed -- +0x08 */
static void fe_deinit(void)
{
    CHAIN_REMOVE(3, ADDR(fe_frame));
    fe_reset(0);
    free_areas();
    DeleteCriticalSection(CS);
    if (M32(S_BST) != 0)
        FREE(S_BST);
    lib_deinit();
}

/* ---- the image ------------------------------------------------------------------- */

#define SLOT(fn) ((fe_fn)(fn))

struct fe_image fe_image = {
    {
        { FS98_MOD_HOST,    0 },
        { FS98_MOD_GLOBAL,  0 },
        { FS98_MOD_BOOSTER, 0 },
        { FS98_MOD_CHAIN,   0 },
        { FS98_MOD_FACIL,   0 },
        { FS98_MOD_G3D,     0 },
        { FS98_MOD_NUMERIC, 0 },
        { FS98_MOD_UTIL,    0 },
        { 0, 0 }
    },
    {
        FS98_MOD_FE, fe_init, fe_deinit, 0, 0xd0000000u, 0,
        {
            /* +0x18 */ SLOT(fe_scene_entry), 0, SLOT(fe_next), SLOT(fe_vor),
            /* +0x28 */ SLOT(fe_ils), SLOT(fe_beacon), SLOT(fe_ground), SLOT(fe_list_next),
            /* +0x38 */ SLOT(fe_list_read), SLOT(fe_object), SLOT(fe_dynamic_entry), SLOT(fe_reset),
            /* +0x48 */ SLOT(fe_level), SLOT(fe_list_begin), SLOT(fe_list_at), SLOT(fe_search),
            /* +0x58 */ SLOT(fe_overlap), SLOT(fe_in_box), SLOT(fe_lat_band), SLOT(fe_band_lat),
            /* +0x68 */ SLOT(fe_lon_band), SLOT(fe_band_lon), SLOT(fe_register), SLOT(fe_unregister),
            /* +0x78 */ 0, 0, 0, 0, 0,
        }
    },
    {
#include "fe_data.inc"
    }
};

/* The pointers retail's loader relocates inside the image. */
static void fe_relocate(void)
{
#define FE_REL_DATA(off, target) M32(D + (off)) = D + (target);
#define FE_REL_TEXT(off, rva)
#define FE_RELOCS
#include "fe_data.inc"
#undef FE_RELOCS
#undef FE_REL_TEXT
#undef FE_REL_DATA
}

/* The two exports are the image's first 0x48 bytes and the Linkage at +0x48,
 * where retail has them. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _fe_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _fe_image + 0x48");

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        fe_relocate();
    }
    return TRUE;
}
