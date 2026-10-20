/* G3D.DLL -- module id 14: the 3D pipeline, a virtual machine for scenery.
 *
 * G3D runs FS5/FS98 scenery bytecode -- opcodes 0x00..0x93, the language of
 * BGL files and of FE's scene stream -- through an interpreter whose
 * handlers transform points by the view matrix, clip, and set up spans for
 * HG2D.  It keeps a depth-sorted queue of models for the frame, and carries
 * built-in scenery of its own in .data: the world's land classes and coasts,
 * runways, the sky.  docs/modules/g3d.md has the layout and the plan.
 *
 * Retail is hand-written assembly from .text+0x20 to the end, with ATC's
 * stack-alignment wrapper around every import call (no observable effect,
 * not reproduced).  It passes values in registers and in flags, so it is
 * transliterated with register variables, as window.c and demo.c are, and
 * the comments carry retail's addresses.  Addresses in .data are written as
 * retail's, through V(), so a number in a listing can be grepped for here.
 *
 * The whole 0x18450-byte .data image is kept at retail's offsets.  Its
 * pointers into itself are installed at load from a generated table.  Its
 * code pointers -- the opcode table, the dispatch tables, the addresses
 * embedded in the built-in bytecode -- keep retail's values: only G3D calls
 * through them, and routine() maps a retail address to the routine here, so
 * the image stays byte for byte retail's.  Code G3D hands to other modules
 * (CHAIN callbacks) is passed as itself.
 *
 * Every Linkage service is entered through a thunk (SERVICE below) that
 * loads the caller's eax, ecx and edx into the register variables and hands
 * back eax, ecx and edx as the transliteration leaves them, so what a caller
 * reads in a register after a call is retail's by construction.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define G3D_IMAGE_SIZE 0x18450u
#define G3D_DATA_VA    0x206f9000u    /* retail's .data */
#define G3D_LINKAGE    0x058u

typedef volatile uint8_t  g3d_v8;
typedef volatile uint16_t __attribute__((may_alias)) g3d_v16;
typedef volatile uint32_t __attribute__((may_alias)) g3d_v32;

struct g3d_image {
    uint8_t bytes[G3D_IMAGE_SIZE];
};

extern struct g3d_image g3d_image;

#define D       ((uint32_t)(uintptr_t)g3d_image.bytes)
#define V(a)    (D + ((uint32_t)(a) - G3D_DATA_VA))
#define M8(a)   (*(g3d_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(g3d_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(g3d_v32 *)(uintptr_t)(uint32_t)(a))

/* The ImportTable's links, as retail reads them (ds:0x206f9004 and on). */
#define HOST     M32(V(0x206f9004))
#define G        M32(V(0x206f900c))
#define CHAIN    M32(V(0x206f9014))
#define HG2D     M32(V(0x206f901c))
#define UTIL     M32(V(0x206f9024))
#define FE       M32(V(0x206f902c))
#define WINDOW   M32(V(0x206f9034))
#define WEATHERM M32(V(0x206f903c))

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))

/* ---- registers -------------------------------------------------------------- */

/* Not static: the service thunks load and read them by name. */
uint32_t g3d_eax, g3d_ebx, g3d_ecx, g3d_edx, g3d_esi, g3d_edi, g3d_ebp;
#define eax g3d_eax
#define ebx g3d_ebx
#define ecx g3d_ecx
#define edx g3d_edx
#define esi g3d_esi
#define edi g3d_edi
#define ebp g3d_ebp

/* A trace build (-DG3D_TRACE) logs the registers at points named by their
 * retail addresses, to compare with the harness's retail tracer. */
#ifdef G3D_TRACE
static void trace(uint32_t at)
{
    static HANDLE f = INVALID_HANDLE_VALUE;
    static const char hex[] = "0123456789abcdef";
    const uint32_t v[8] = { at, eax, ebx, ecx, edx, esi, edi, ebp };
    static const char *const name[8] = { "", " eax ", " ebx ", " ecx ", " edx ", " esi ",
                                         " edi ", " ebp " };
    char line[128];
    DWORD n;
    int i, j, k = 0;

    if (f == INVALID_HANDLE_VALUE)
        f = CreateFileA("g3d_ours_trace.txt", GENERIC_WRITE, FILE_SHARE_READ, 0,
                        CREATE_ALWAYS, 0, 0);
    for (i = 0; i < 8; i++) {
        for (j = 0; name[i][j]; j++)
            line[k++] = name[i][j];
        for (j = 28; j >= 0; j -= 4)
            line[k++] = hex[(v[i] >> j) & 15];
    }
    line[k++] = '\n';
    WriteFile(f, line, (DWORD)k, &n, 0);
}
#define TRACE(at) trace(at)
#else
#define TRACE(at) ((void)0)
#endif

/* The carry and sign flags, where a routine returns status in them. */
static uint32_t cf, sf;

/* A routine of retail's, by address: see routine() at the end. */
typedef void (*routine_fn)(void);
static routine_fn routine(uint32_t a);

#define AX        ((uint16_t)eax)
#define BX        ((uint16_t)ebx)
#define CX        ((uint16_t)ecx)
#define DX        ((uint16_t)edx)
#define SI        ((uint16_t)esi)
#define DI        ((uint16_t)edi)
#define BP        ((uint16_t)ebp)
#define AL        ((uint8_t)eax)
#define CL        ((uint8_t)ecx)
#define SET_LO(r, v)  ((r) = ((r) & 0xffff0000u) | (uint16_t)(v))
#define SET_AX(v) SET_LO(eax, v)
#define SET_BX(v) SET_LO(ebx, v)
#define SET_CX(v) SET_LO(ecx, v)
#define SET_DX(v) SET_LO(edx, v)
#define SET_SI(v) SET_LO(esi, v)
#define SET_DI(v) SET_LO(edi, v)
#define SET_BP(v) SET_LO(ebp, v)
#define SET_AL(v) (eax = (eax & 0xffffff00u) | (uint8_t)(v))
#define SET_CL(v) (ecx = (ecx & 0xffffff00u) | (uint8_t)(v))
#define SET_BL(v) (ebx = (ebx & 0xffffff00u) | (uint8_t)(v))
#define SET_DL(v) (edx = (edx & 0xffffff00u) | (uint8_t)(v))
#define AH        ((uint8_t)(eax >> 8))
#define BH        ((uint8_t)(ebx >> 8))
#define CH        ((uint8_t)(ecx >> 8))
#define DH        ((uint8_t)(edx >> 8))
#define BL        ((uint8_t)ebx)
#define DL        ((uint8_t)edx)
#define SET_AH(v) (eax = (eax & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define SET_BH(v) (ebx = (ebx & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define SET_CH(v) (ecx = (ecx & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define SET_DH(v) (edx = (edx & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))

/* Shifts by cl: the count is masked to five bits, as the instruction does. */
static uint32_t sar32(uint32_t v, uint32_t n)
{
    n &= 31;
    return (uint32_t)((int32_t)v >> n);
}

static uint32_t shl32(uint32_t v, uint32_t n)
{
    return v << (n & 31);
}

/* imul r/m16: dx:ax = ax * b, signed */
static void imul16(uint16_t b)
{
    uint32_t p = (uint32_t)((int32_t)(int16_t)AX * (int16_t)b);

    SET_AX(p);
    SET_DX(p >> 16);
}

/* imul r/m32: edx:eax = eax * b, signed */
static void imul32(uint32_t b)
{
    int64_t p = (int64_t)(int32_t)eax * (int32_t)b;

    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
}

/* idiv r/m32 of edx:eax.  Retail divides by zero and into overflow, and the
 * host's filter (0x4119a0, around the sim's main loop) repairs the fault:
 * the quotient becomes 0x7fffffff, or 0x80000001 when the divisor and edx
 * differ in sign, and the remainder 0x7fffffff.  Here that is done without
 * the fault. */
static void idiv32(uint32_t d)
{
    int64_t n = (int64_t)(((uint64_t)edx << 32) | eax), q;

    if (d != 0) {
        q = n / (int32_t)d;
        if (q >= INT32_MIN && q <= INT32_MAX) {
            eax = (uint32_t)q;
            edx = (uint32_t)(n % (int32_t)d);
            return;
        }
    }
    eax = ((int32_t)d < 0) != ((int32_t)edx < 0) ? 0x80000001u : 0x7fffffffu;
    edx = 0x7fffffffu;
}

/* idiv r/m16 of dx:ax, with the host's repair: 0x7fff (0x8001 when the
 * divisor and dx differ in sign) and a remainder of 0x7fff on a fault. */
static void idiv16(uint16_t d)
{
    int32_t n = (int32_t)((uint32_t)DX << 16 | AX), q;

    if (d != 0) {
        q = n / (int16_t)d;
        if (q >= -0x8000 && q <= 0x7fff && !(n == INT32_MIN && (int16_t)d == -1)) {
            SET_AX(q);
            SET_DX(n % (int16_t)d);
            return;
        }
    }
    SET_AX(((int16_t)d < 0) != ((int16_t)DX < 0) ? 0x8001 : 0x7fff);
    SET_DX(0x7fff);
}

/* cdq */
static void cdq(void)
{
    edx = (uint32_t)((int32_t)eax >> 31);
}

/* A service's entry.  The caller's registers go into the register variables,
 * the body runs with a pointer to its stack arguments, and eax, ecx and edx
 * come back as the body leaves them; ebx, esi, edi and ebp are the caller's,
 * as every retail service here pushes and pops them.  `bytes` is the
 * service's ret N. */
#define SERVICE(name, bytes)                                                  \
    static void name##_body(const uint32_t *arg)                             \
        __attribute__((used, noinline));                                     \
    __asm__(".text\n\t.p2align 4\n"                                          \
            "_" #name ":\n\t"                                                \
            "movl %eax, _g3d_eax\n\tmovl %ecx, _g3d_ecx\n\t"                 \
            "movl %edx, _g3d_edx\n\tmovl %ebx, _g3d_ebx\n\t"                 \
            "movl %esi, _g3d_esi\n\tmovl %edi, _g3d_edi\n\t"                 \
            "movl %ebp, _g3d_ebp\n\t"                                        \
            "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"       \
            "leal 20(%esp), %eax\n\tpushl %eax\n\t"                          \
            "call _" #name "_body\n\t"                                       \
            "addl $4, %esp\n\t"                                              \
            "popl %ebp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\t"           \
            "movl _g3d_eax, %eax\n\tmovl _g3d_ecx, %ecx\n\t"                 \
            "movl _g3d_edx, %edx\n\t"                                        \
            "ret $" #bytes "\n");                                            \
    void name(void)

/* ---- the transform core: .text+0x20..+0x535 ---------------------------------- */

static void point_from_stream(void);
static void rotate_view(void);
static void outcode_a(void);
static void outcode_b(void);
static void normalise(void);

/* 0x206e1020 -- the instruction's point (at ebx) into the first vertex,
 * 0x206f90c0, and its outcode. */
static void vertex_a(void)
{
    edi = V(0x206f90c0);
    point_from_stream();
    outcode_a();
}

/* 0x206e1030 -- into the second vertex, 0x206f90cc, copied on to 0x206f90d8,
 * and its outcode. */
static void vertex_b(void)
{
    edi = V(0x206f90cc);
    point_from_stream();
    eax = M32(V(0x206f90cc));
    M32(V(0x206f90d8)) = eax;
    eax = M32(V(0x206f90d0));
    M32(V(0x206f90dc)) = eax;
    eax = M32(V(0x206f90d4));
    M32(V(0x206f90e0)) = eax;
    outcode_b();
}

/* 0x206e105e -- into the first vertex, no outcode. */
static void vertex_a_only(void)
{
    edi = V(0x206f90c0);
    point_from_stream();
}

/* 0x206e1069 -- the vector in the words at 0x206f912e..0x206f9132 by the view
 * matrix (GLOBAL +0xce8..+0xcf8, 2.14), into the three dwords at edi as
 * 2 * the 16x16 sums, in 16-bit halves; z then down by GLOBAL +0xd42. */
static void rotate_row(uint32_t m0, uint32_t m1, uint32_t m2, uint32_t out)
{
    uint32_t s;

    SET_AX(M16(V(0x206f912e)));
    imul16(M16(esi + m0));
    SET_BX(AX);
    SET_CX(DX);
    SET_AX(M16(V(0x206f9130)));
    imul16(M16(esi + m1));
    s = (uint32_t)BX + AX;
    SET_BX(s);
    SET_CX(CX + DX + (s >> 16));
    SET_AX(M16(V(0x206f9132)));
    imul16(M16(esi + m2));
    s = (uint32_t)AX + BX;
    SET_AX(s);
    SET_DX(DX + CX + (s >> 16));
    s = (uint32_t)AX + AX;
    SET_AX(s);
    SET_DX(DX + DX + (s >> 16));
    M16(out) = AX;
    M16(out + 2) = DX;
}

static void rotate_view(void)
{
    uint32_t keep_esi = esi;

    esi = G;
    rotate_row(0xce8, 0xcee, 0xcf4, edi);
    rotate_row(0xcea, 0xcf0, 0xcf6, edi + 4);
    rotate_row(0xcec, 0xcf2, 0xcf8, edi + 8);
    SET_CX(M16(esi + 0xd42));
    M32(edi + 8) = sar32(M32(edi + 8), CL);
    esi = keep_esi;
}

/* 0x206e1150 / 0x206e118c -- the outcode of the vertex at 0x206f90c0 (into
 * 0x206f911c) or 0x206f90cc (into 0x206f911e): four bits, x left and right
 * of the cone |x| <= z, then y below and above. */
static void outcode(uint32_t vertex, uint32_t out)
{
    ebx = M32(vertex);
    ecx = M32(vertex + 4);
    edx = M32(vertex + 8);
    esi = (uint32_t)-(int32_t)edx;
    SET_AL(0);
    if ((int32_t)ebx < (int32_t)esi)
        SET_AL(AL + 1);
    SET_AL(AL + AL);
    if ((int32_t)ebx > (int32_t)edx)
        SET_AL(AL + 1);
    SET_AL(AL + AL);
    if ((int32_t)ecx < (int32_t)esi)
        SET_AL(AL + 1);
    SET_AL(AL + AL);
    if ((int32_t)ecx > (int32_t)edx)
        SET_AL(AL + 1);
    M8(out) = AL;
}

static void outcode_a(void) { outcode(V(0x206f90c0), V(0x206f911c)); }
static void outcode_b(void) { outcode(V(0x206f90cc), V(0x206f911e)); }

/* 0x206e11c8 (a lods/stos string copy) and 0x206e11d8 (each axis's step
 * between two points, divided by cx) are not reached from anywhere -- no
 * call, no address taken in code or .data -- and are left out.  So are the
 * call/ret stubs at 0x206e155f..0x206e1576, and 0x206e1404, which only one
 * of them calls (it loads cx where 0x206e1411 reads dx). */

/* 0x206e121e -- the point of the instruction at ebx: three words from +1
 * (bit 0 of 0x206f9156 clear), or x and z from +1 and +3 with y the word at
 * 0x206f9157 (bit set; the byte is shifted down either way).  The program
 * counter goes past it.  Then relative to the eye, rotated, into edi. */
static void point_relative_into(uint32_t out);

static void point_from_stream(void)
{
    uint32_t keep_edi = edi, bit;

    SET_BP(M16(ebx + 1));
    ebp <<= 16;
    bit = M8(V(0x206f9156)) & 1;
    M8(V(0x206f9156)) = M8(V(0x206f9156)) >> 1;
    if (!bit) {
        SET_DI(M16(ebx + 3));
        edi <<= 16;
        SET_DX(M16(ebx + 5));
        edx <<= 16;
        ebx += 7;
        M32(V(0x206f9126)) = ebx;
    } else {
        SET_DI(M16(V(0x206f9157)));
        edi <<= 16;
        SET_DX(M16(ebx + 3));
        edx <<= 16;
        ebx += 5;
        M32(V(0x206f9126)) = ebx;
    }
    point_relative_into(keep_edi);
}

/* 0x206e1263 -- the same from three words at esi, which moves past them. */
static void point_from_esi(void)
{
    uint32_t keep_edi = edi;

    SET_BP(M16(esi));
    ebp <<= 16;
    SET_DI(M16(esi + 2));
    edi <<= 16;
    SET_DX(M16(esi + 4));
    edx <<= 16;
    esi += 6;
    point_relative_into(keep_edi);
}

/* sub r32,[m] then jo: the difference, and whether it overflowed; the carry
 * is the borrow. */
static uint32_t sub_ov(uint32_t a, uint32_t b, int *ov)
{
    uint32_t r = a - b;

    *ov = (int)((((a ^ b) & (a ^ r)) >> 31) & 1);
    cf = a < b;
    return r;
}

/* cmc / rcr r,1 */
static uint32_t cmc_rcr(uint32_t r)
{
    return (r >> 1) | ((cf ^ 1) << 31);
}

/* 0x206e127b..0x206e1320 -- (ebp, edi, edx), 16.16 world units, down by the
 * word at 0x206f9159, less the eye (GLOBAL +0xcdc..+0xce4), normalised, and
 * their top halves to 0x206f912e..0x206f9132 for the rotation into `out`.
 * A difference that overflows is halved through the carry and the other two
 * scaled to match, and the normalising is skipped: the shift recorded at
 * 0x206f9122 is then 0xfd. */
static void point_relative_into(uint32_t out)
{
    int ov;

    if (M16(V(0x206f9159)) != 0) {
        SET_CX(M16(V(0x206f9159)));
        ebp = sar32(ebp, CL);
        edi = sar32(edi, CL);
        edx = sar32(edx, CL);
    }
    ebx = G;
    ebp = sub_ov(ebp, M32(ebx + 0xcdc), &ov);
    if (ov) {                                   /* 0x206e12dc */
        ebp = cmc_rcr(ebp);
        ebp = sar32(ebp, 2);
        eax = M32(ebx + 0xce0);
        edi = sar32(edi, 1);
        eax = sar32(eax, 1);
        edi -= eax;
        edi = sar32(edi, 2);
        goto z_halved;
    }
    edi = sub_ov(edi, M32(ebx + 0xce0), &ov);
    if (ov) {                                   /* 0x206e12f3 */
        edi = cmc_rcr(edi);
        edi = sar32(edi, 2);
        ebp = sar32(ebp, 3);
z_halved:                                       /* 0x206e12fc */
        eax = M32(ebx + 0xce4);
        edx = sar32(edx, 1);
        eax = sar32(eax, 1);
        edx -= eax;
        edx = sar32(edx, 2);
        goto overflowed;
    }
    edx = sub_ov(edx, M32(ebx + 0xce4), &ov);
    if (ov) {                                   /* 0x206e130d */
        edx = cmc_rcr(edx);
        edx = sar32(edx, 2);
        ebp = sar32(ebp, 3);
        edi = sar32(edi, 3);
        goto overflowed;
    }
    normalise();
    goto halves;
overflowed:                                     /* 0x206e1319 */
    M8(V(0x206f9122)) = 0xfd;
halves:                                         /* 0x206e12a8 */
    edx >>= 16;
    M16(V(0x206f9132)) = DX;
    edi >>= 16;
    M16(V(0x206f9130)) = DI;
    ebp >>= 16;
    M16(V(0x206f912e)) = BP;
    edi = out;
    rotate_view();
}

/* 0x206e13a2 -- normalise (ebp, edi, edx) so the largest magnitude has its
 * top bit at bit 28: shift left by 28 - (its bit), but by 25 when that bit
 * is below 4; right by 1 or 2 when it is at 29 or above.  The shift goes to
 * 0x206f9122 as a signed byte (0, 25..1, -1, -2).  All three zero: nothing
 * changes, 0x206f9122 included.  The jump table at 0x206e1322 is the switch. */
static void normalise(void)
{
    uint32_t bit;

    ecx = edx;
    eax = ecx;
    cdq();
    eax ^= edx;
    ebx = eax;
    eax = edi;
    cdq();
    eax ^= edx;
    ebx |= eax;
    eax = ebp;
    cdq();
    eax ^= edx;
    ebx |= eax;
    edx = ecx;
    if (ebx == 0)
        return;
    bit = 31 - (uint32_t)__builtin_clz(ebx);
    ecx = bit;
    if (bit <= 3) {                             /* 0x206e13c7 */
        SET_CL(0x19);
    } else if (bit <= 27) {                     /* 0x206e13cb */
        SET_CL(0x1c - CL);
    } else if (bit == 28) {                     /* 0x206e13dd */
        M8(V(0x206f9122)) = 0;
        return;
    } else if (bit == 29) {                     /* 0x206e13e5 */
        ebp = sar32(ebp, 1);
        edi = sar32(edi, 1);
        edx = sar32(edx, 1);
        M8(V(0x206f9122)) = 0xff;
        return;
    } else {                                    /* 0x206e13f3 */
        ebp = sar32(ebp, 2);
        edi = sar32(edi, 2);
        edx = sar32(edx, 2);
        M8(V(0x206f9122)) = 0xfe;
        return;
    }
    ebp = shl32(ebp, CL);                       /* 0x206e13d0 */
    edi = shl32(edi, CL);
    edx = shl32(edx, CL);
    M8(V(0x206f9122)) = CL;
}

/* 0x206e1411 -- a point from the words in ax, bx and dx, as 16.16 values
 * with no fraction (shrd and the xor leave each as word << 16), in
 * 0x206f9159's scale, less the eye with each difference halved (through the
 * carry when it overflows), to 0x206fd210..0x206fd218, and through the view
 * matrix, 2.15 this time, into edi's three dwords.  The shift recorded at
 * 0x206f9122 is 1. */
static uint32_t sub_half(uint32_t a, uint32_t b)
{
    int ov;
    uint32_t r = sub_ov(a, b, &ov);

    return ov ? cmc_rcr(r) : sar32(r, 1);
}

static void rotate_row32(uint32_t m0, uint32_t m1, uint32_t m2)
{
    int64_t s;

    eax = (uint32_t)(int32_t)(int16_t)M16(esi + m0);
    imul32(M32(V(0x206fd210)));
    ebx = eax;
    ecx = edx;
    s = (int64_t)(((uint64_t)ecx << 32) | ebx);
    eax = (uint32_t)(int32_t)(int16_t)M16(esi + m1);
    imul32(M32(V(0x206fd214)));
    s += (int64_t)(((uint64_t)edx << 32) | eax);
    ebx = (uint32_t)s;
    ecx = (uint32_t)((uint64_t)s >> 32);
    eax = (uint32_t)(int32_t)(int16_t)M16(esi + m2);
    imul32(M32(V(0x206fd218)));
    s += (int64_t)(((uint64_t)edx << 32) | eax);
    eax = (uint32_t)s;
    edx = (uint32_t)((uint64_t)s >> 32);
    eax = (eax >> 15) | (edx << 17);            /* shrd eax,edx,15 */
}

static void point_from_words(void)
{
    uint32_t keep_esi = esi;

    M8(V(0x206f9122)) = 1;
    ebp = (ebp >> 16) | (eax << 16);            /* shrd ebp,eax,16 */
    SET_BP(0);
    esi = (esi >> 16) | (ebx << 16);            /* shrd esi,ebx,16 */
    SET_SI(0);
    edx <<= 16;
    SET_CL(M8(V(0x206f9159)));
    ebp = sar32(ebp, CL);
    esi = sar32(esi, CL);
    edx = sar32(edx, CL);
    ebx = G;
    ebp = sub_half(ebp, M32(ebx + 0xcdc));
    esi = sub_half(esi, M32(ebx + 0xce0));
    edx = sub_half(edx, M32(ebx + 0xce4));
    eax = ebp;
    M32(V(0x206fd210)) = eax;
    M32(V(0x206fd214)) = esi;
    M32(V(0x206fd218)) = edx;
    esi = ebx;
    rotate_row32(0xce8, 0xcee, 0xcf4);
    M32(edi) = eax;
    rotate_row32(0xcea, 0xcf0, 0xcf6);
    M32(edi + 4) = eax;
    rotate_row32(0xcec, 0xcf2, 0xcf8);
    SET_CX(M16(esi + 0xd42));
    eax = sar32(eax, CL);
    M32(edi + 8) = eax;
    esi = keep_esi;
}

/* ---- imports -------------------------------------------------------------------- */

/* Calls to other modules and the host go through import_call(), which
 * pushes the arguments, calls with eax the import's table (retail's mov
 * eax,[import]; call [eax+n]) and ecx and edx as they are, and leaves in
 * ecx and edx what the callee left there: a callee that keeps them (HG2D's
 * vertex calls) hands retail's on, and 0x27 loops on the ecx a vertex
 * leaves, and a service whose last act is an import call hands them on to
 * its own caller.  Arguments retail pushes as words (push ax after sub
 * esp,2) are passed with their high halves zero; every callee reads only
 * the word. */
uint32_t import_call(uint32_t fn, const uint32_t *args, uint32_t n, uint32_t base);
__asm__(".text\n\t.p2align 4\n_import_call:\n\t"
        "pushl %ebp\n\tmovl %esp, %ebp\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 16(%ebp), %ecx\n\tmovl 12(%ebp), %esi\n"
        "1:\n\ttestl %ecx, %ecx\n\tjz 2f\n\t"
        "pushl -4(%esi,%ecx,4)\n\tdecl %ecx\n\tjmp 1b\n"
        "2:\n\tmovl 20(%ebp), %eax\n\t"
        "movl _g3d_ecx, %ecx\n\tmovl _g3d_edx, %edx\n\t"
        "call *8(%ebp)\n\t"
        "movl %ecx, _g3d_ecx\n\tmovl %edx, _g3d_edx\n\t"
        "leal -12(%ebp), %esp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");

#define IMP0(m, off)            import_call(M32((m) + (off)), 0, 0, (m))
#define IMP1(m, off, a)         import_call(M32((m) + (off)), (const uint32_t[]){ a }, 1, (m))
#define IMP2(m, off, a, b)      import_call(M32((m) + (off)), (const uint32_t[]){ a, b }, 2, (m))
#define IMP3(m, off, a, b, c)   import_call(M32((m) + (off)), (const uint32_t[]){ a, b, c }, 3, (m))
#define IMP4(m, off, a, b, c, d) import_call(M32((m) + (off)), (const uint32_t[]){ a, b, c, d }, 4, (m))

/* The host's memory and list services (include/fs98/host.h). */
#define H_ALLOC(fl, size, out, zero) (eax = IMP4(HOST, 0x20c, fl, size, out, zero))
#define H_REALLOC(fl, size, pp)      (eax = IMP3(HOST, 0x210, fl, size, pp))
#define H_FREE(pp)                   (eax = IMP1(HOST, 0x214, pp))
#define H_LIST_ADD(list)             (eax = IMP1(HOST, 0x2f8, list))
#define H_LIST_FREE(list)            (eax = IMP1(HOST, 0x2fc, list))
#define H_LIST_REMOVE(list, elem)    (eax = IMP2(HOST, 0x300, list, elem))
#define H_LIST_CLEAR(list)           (eax = IMP1(HOST, 0x304, list))

#define CHAIN_ADD(list, cb)    (eax = IMP2(CHAIN, 0x18, list, (uint32_t)(uintptr_t)(cb)))
#define CHAIN_REMOVE(list, cb) (eax = IMP2(CHAIN, 0x1c, list, (uint32_t)(uintptr_t)(cb)))

#define HG2D_UNLOAD(tex)       (eax = IMP2(HG2D, 0xb0, tex, 0))

/* ---- the interpreter: .text+0x1550..+0x1844 ---------------------------------------- */

/* 0x206e25c0 -- run the program at the counter (0x206f9126) until an
 * opcode past 0x93 or a handler sets the stop flag (0x206fd204, cleared on
 * the way out).  Each handler runs between pusha and popa with esi the
 * counter and eax its own address -- retail's, as the table keeps it -- so
 * it talks to the next only through memory. */
static void interpret(void)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi;

    eax = M32(V(0x206f9126));
    M32(V(0x206fd204)) = 0;
    SET_AX(M16(eax));
    if (AX <= 0x93) {
        for (;;) {
            uint32_t a, c, d, b, p, s, i;

            eax &= 0xffff;
            eax = M32(V(0x206fd220) + eax * 8);
            a = eax, c = ecx, d = edx, b = ebx, p = ebp, s = esi, i = edi;
            esi = M32(V(0x206f9126));
            TRACE(0x206e25ee);
            routine(eax)();
            eax = a, ecx = c, edx = d, ebx = b, ebp = p, esi = s, edi = i;
            eax = M32(V(0x206fd204));
            if (eax != 0) {
                M32(V(0x206fd204)) = 0;
                break;
            }
            ecx = M32(V(0x206f9126));
            SET_AX(M16(ecx));
            if (AX > 0x93)
                break;
        }
    }
    ebx = keep_ebx;
    esi = keep_esi;
    edi = keep_edi;
}

/* 0x206e2550 -- +0x1c's body: run (program) from a clean drawing state. */
static void run_program(uint32_t program)
{
    ecx = G;
    edx = program;
    eax = 0;
    M8(V(0x206f9125)) = AL;
    M16(ecx + 0xc90) = AX;
    M16(V(0x206fd6c8)) = AX;
    M32(V(0x206f9126)) = edx;
    interpret();
}

SERVICE(run, 4);
static void run_body(const uint32_t *arg)
{
    run_program(arg[0]);
}

/* 0x206e2580 -- +0x18: the same with the scale (0x206f9159) cleared and a
 * variable context (0x206f91b7) given. */
SERVICE(run_in_context, 8);
static void run_in_context_body(const uint32_t *arg)
{
    ecx = G;
    edx = arg[1];
    eax = 0;
    M8(V(0x206f9125)) = AL;
    M16(ecx + 0xc90) = AX;
    M16(V(0x206fd6c8)) = AX;
    M16(V(0x206f9159)) = AX;
    eax = arg[0];
    M32(V(0x206f91b7)) = edx;
    M32(V(0x206f9126)) = eax;
    interpret();
}

/* 0x206e2620 -- opcodes 0x22 and 0x7c, return: stop this program. */
static void op_return(void)
{
    M32(V(0x206fd204)) = 1;
}

/* 0x206e2630 -- 0x02, 0x43, 0x64, 0x85: two bytes, nothing done. */
static void op_skip2(void)
{
    M32(V(0x206f9126)) += 2;
}

/* 0x206e2640 -- 0x00, end: past it, and stop. */
static void op_end(void)
{
    eax = M32(V(0x206f9126));
    M32(V(0x206fd204)) = 1;
    eax += 2;
    M32(V(0x206f9126)) = eax;
}

/* 0x206e2660 -- 0x8f: the dword operand to GLOBAL +0x1298. */
static void op_8f(void)
{
    eax = M32(V(0x206f9126));
    ecx = G;
    eax += 2;
    M32(V(0x206f9126)) = eax;
    eax = M32(eax);
    M32(ecx + 0x1298) = eax;
    eax = M32(V(0x206f9126));
    eax += 4;
    M32(V(0x206f9126)) = eax;
}

/* 0x206e27f0 / 0x206e2820 -- 0x92, 0x93: the word operand to GLOBAL +0x324
 * or +0x326. */
static void op_word_to_global(uint32_t field)
{
    eax = M32(V(0x206f9126));
    edx = G;
    SET_CX(M16(eax + 2));
    M16(edx + field) = CX;
    eax = M32(V(0x206f9126));
    eax += 4;
    M32(V(0x206f9126)) = eax;
}

static void op_92(void) { op_word_to_global(0x324); }
static void op_93(void) { op_word_to_global(0x326); }

/* ---- models: .text+0x590..+0x689 ------------------------------------------------ */

/* 0x206e15ee -- +0x28's body: a model is 'v' (0x76) at its first byte,
 * then the program from the next even address.  The drawing state is reset
 * first and the eye's position kept in GLOBAL +0xe06..+0xe0a (latitude's
 * high bits, longitude's, the altitude's low word). */
static void draw_model(uint32_t model)
{
    M8(V(0x206f915f)) = 0;
    M8(V(0x206f9160)) = 0;
    M32(V(0x206f9161)) = V(0x206f9166);
    M32(V(0x206f91b7)) = 0;
    M16(V(0x206f9159)) = 0;
    edi = G;
    eax = M32(edi + 0x5b4);
    eax >>= 9;
    M16(edi + 0xe06) = AX;
    eax = M32(edi + 0x5bc);
    eax = sar32(eax, 16);
    M16(edi + 0xe08) = AX;
    eax = M32(edi + 0x5c4);
    M16(edi + 0xe0a) = AX;
    esi = model;
    if (M8(esi) != 0x76)
        return;
    model += 2;
    model &= 0xfffffffeu;
    edi = G;
    M16(edi + 0x3a8) = 0;
    run_program(model);
}

SERVICE(draw, 4);
static void draw_body(const uint32_t *arg)
{
    draw_model(arg[0]);
}

/* 0x206e1590 -- +0x20: the same, with the model kept at 0x206fd1fa and the
 * countdown's reload and value (0x206f915c, 0x206f915b) at 0x206f915d and
 * 0x206f915e. */
SERVICE(draw_counted, 4);
static void draw_counted_body(const uint32_t *arg)
{
    eax = arg[0];
    M32(V(0x206fd1fa)) = eax;
    SET_AL(M8(V(0x206f915c)));
    M8(V(0x206f915d)) = AL;
    SET_AL(M8(V(0x206f915b)));
    M8(V(0x206f915e)) = AL;
    draw_model(arg[0]);
}

/* 0x206e15c3 -- +0x24: with those two cleared. */
SERVICE(draw_uncounted, 4);
static void draw_uncounted_body(const uint32_t *arg)
{
    eax = arg[0];
    M32(V(0x206fd1fa)) = eax;
    M8(V(0x206f915d)) = 0;
    M8(V(0x206f915e)) = 0;
    draw_model(arg[0]);
}

/* ---- init and deinit: .text+0x26c0..+0x26e6, +0x2df0..+0x2e4a ------------------ */

static void init_callbacks(void);
static void deinit_callbacks(void);
static void init_lighting(void);
static void init_textures(void);
static void deinit_textures(void);
static void init_8c80(void);
static void deinit_8c80(void);
static void init_libraries(void);
static void deinit_libraries(void);
static void init_library_ageing(void);
static void deinit_library_ageing(void);
static void init_variables(void);
static void queue_alloc(void);
static void queue_free(void);

/* 0x206e3df0 */
static void init_parts(void)
{
    M16(V(0x206f90f6)) = 1;
    init_variables();
    init_callbacks();
    init_lighting();
    init_textures();
    init_8c80();
    init_libraries();
    init_library_ageing();
    /* 0x206f4810: a bare ret */
    queue_alloc();
}

/* 0x206e3e27 */
static void deinit_parts(void)
{
    deinit_callbacks();
    /* 0x206e6c6a: a bare ret */
    deinit_textures();
    deinit_8c80();
    deinit_libraries();
    deinit_library_ageing();
    /* 0x206f4811: a bare ret */
}

/* 0x206e36c0 / 0x206e36d1 -- the Linkage's init and deinit.  The bare
 * rets at 0x206f431a/0x206f431b they call are left out. */
SERVICE(g3d_init, 0);
static void g3d_init_body(const uint32_t *arg)
{
    (void)arg;
    init_parts();
}

SERVICE(g3d_deinit, 0);
static void g3d_deinit_body(const uint32_t *arg)
{
    (void)arg;
    deinit_parts();
    queue_free();
}

/* ---- the simple opcodes: .text+0x2727..+0x27af ------------------------------------ */

/* 0x206e3727, 0x206f1b5f, 0x206f4813 -- 0x01, 0x59, 0x4b: a bare ret.  The
 * counter does not move, so the interpreter runs the same opcode again,
 * forever: retail hangs on any of the three. */
static void bare_ret(void)
{
}

/* 0x206e3728 -- 0x76, the 'v' a model starts with, read as an opcode. */
static void op_76(void)
{
    M32(V(0x206f9126)) += 2;
}

/* 0x206e3730 -- 0x86: three bytes. */
static void op_86(void)
{
    M32(V(0x206f9126)) += 3;
}

/* 0x206e3738 -- 0x04: past it, and the counter at 0x206fd6d0 down,
 * reloaded with 100 at zero. */
static void op_04(void)
{
    M32(V(0x206f9126)) += 2;
    ebx = M32(V(0x206f9126));
    M32(V(0x206fd6d0)) -= 1;
    if (M32(V(0x206fd6d0)) == 0)
        M32(V(0x206fd6d0)) = 0x64;
}

/* 0x206e3758 -- 0x16, jump: by the signed word operand, from the opcode. */
static void op_jump(void)
{
    eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    esi += eax;
    M32(V(0x206f9126)) = esi;
}

/* 0x206e3765 -- 0x17: GLOBAL +0xc90 from +0xcb6 when the operand is set,
 * else 0; drawing off (0x206f9125). */
static void op_17(void)
{
    SET_AX(0);
    edi = G;
    M8(V(0x206f9125)) = AL;
    if (M16(esi + 2) != AX)
        SET_AX(M16(edi + 0xcb6));
    M16(edi + 0xc90) = AX;
    M32(V(0x206f9126)) += 4;
}

/* 0x206e378f -- 0x54: six bytes. */
static void op_54(void)
{
    M32(V(0x206f9126)) += 6;
}

/* 0x206e3797 -- 0x8c: call the code at the operand's absolute address,
 * the counter kept around it.  Retail's built-in programs carry its own
 * addresses here; routine() finds the one they name. */
static void op_native(void)
{
    uint32_t keep = M32(V(0x206f9126));

    eax = M32(esi + 2);
    routine(eax)();
    M32(V(0x206f9126)) = keep;
    M32(V(0x206f9126)) += 6;
}

/* 0x206e3a00 -- 0x0d, jump: as 0x16 but onto the counter. */
static void op_0d(void)
{
    eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    M32(V(0x206f9126)) += eax;
}

/* ---- opcodes: .text+0x3184..+0x3422 ------------------------------------------------ */

/* lods ax: the word at esi, which moves on */
static void lods16(void)
{
    SET_AX(M16(esi));
    esi += 2;
}

static void lods32(void)
{
    eax = M32(esi);
    esi += 4;
}

/* 0x206e4184 -- 0x1f: the word operand to GLOBAL +0x394. */
static void op_1f(void)
{
    esi += 2;
    lods16();
    eax = AX;
    edi = G;
    M32(edi + 0x394) = eax;
    M32(V(0x206f9126)) = esi;
}

/* 0x206e419f -- 0x23, call: the program at the signed word operand from the
 * opcode, the counter kept around it; on past four bytes. */
static void op_call(void)
{
    uint32_t keep;

    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    keep = M32(V(0x206f9126));
    M32(V(0x206f9126)) = eax;
    interpret();
    M32(V(0x206f9126)) = keep;
    M32(V(0x206f9126)) += 4;
}

/* 0x206e41c9 -- 0x8a: 0x23 with a dword offset; six bytes. */
static void op_call32(void)
{
    uint32_t keep;

    esi += 2;
    lods32();
    eax += M32(V(0x206f9126));
    keep = M32(V(0x206f9126));
    M32(V(0x206f9126)) = eax;
    interpret();
    M32(V(0x206f9126)) = keep;
    M32(V(0x206f9126)) += 6;
}

/* 0x206e41f1 -- 0x5c: call the program at the absolute dword operand. */
static void op_call_absolute(void)
{
    uint32_t keep = esi;

    eax = M32(esi + 2);
    M32(V(0x206f9126)) = eax;
    interpret();
    eax = keep;
    eax += 6;
    M32(V(0x206f9126)) = eax;
}

/* ---- variables ---------------------------------------------------------------------- */

/* 0x206f1c5c -- a variable the table does not map: the scratch word at
 * 0x207087e0. */
static void variable_unmapped(void)
{
    ebx = V(0x207087e0);
    eax = 0;
}

/* The lookup every variable operand goes through (inline in retail): ebx
 * the signed variable number in, its address out.  With a context
 * (0x206f91b7) it is context + number; otherwise the table at 0x206ff950
 * gives the address, and the table's entries for unmapped variables, which
 * init left equal to GLOBAL, go to the scratch word.  eax is left as each
 * path leaves it. */
static void variable(void)
{
    eax = M32(V(0x206f91b7));
    if (eax != 0) {
        ebx += eax;
        return;
    }
    eax = ebx;
    ebx = M32(V(0x206ff950) + ebx * 4);
    if (ebx != G)
        return;
    variable_unmapped();
    ebx += eax;
}

/* The word operand at p as a variable's address in ebx. */
static void variable_at(uint32_t p)
{
    SET_AX(M16(p));
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
}

/* 0x206e43e5 -- 0x5b: call the program whose address is in the variable. */
static void op_5b(void)
{
    uint32_t keep;

    esi += 2;
    lods16();
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
    eax = M32(ebx);
    keep = esi;
    M32(V(0x206f9126)) = eax;
    interpret();
    M32(V(0x206f9126)) = keep;
}

/* 0x206e4422 -- 0x61: six bytes. */
static void op_61(void)
{
    esi += 6;
    M32(V(0x206f9126)) = esi;
}

/* 0x206e4470 -- 0x81: the operand under the mask GLOBAL +0xcd0 to +0xcce. */
static void op_81(void)
{
    SET_AX(M16(esi + 2));
    esi += 4;
    M32(V(0x206f9126)) = esi;
    edi = G;
    SET_AX(AX & M16(edi + 0xcd0));
    M16(edi + 0xcce) = AX;
}

/* 0x206e54cb -- 0x71: a level, the highest seen at or below GLOBAL +0x574
 * kept at 0x206fd720, with 0x206fd724 set when it rises. */
static void op_71(void)
{
    SET_AX(M16(esi + 2));
    esi += 4;
    M32(V(0x206f9126)) = esi;
    edi = G;
    if (AX > M16(edi + 0x574))
        return;
    if (AX < M16(V(0x206fd720)))
        return;
    M16(V(0x206fd720)) = AX;
    M16(V(0x206fd724)) = 1;
}

/* 0x206e5cda -- 0x88: four bytes. */
static void op_88(void)
{
    M32(V(0x206f9126)) += 4;
}

/* 0x206e5ce2 -- 0x8d: the dword operand to 0x206f919c. */
static void op_8d(void)
{
    esi += 2;
    lods32();
    M32(V(0x206f919c)) = eax;
    M32(V(0x206f9126)) = esi;
}

/* 0x206e5cf2 -- 0x8e: the variable's dword to 0x206f919c. */
static void op_8e(void)
{
    esi += 2;
    lods16();
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
    eax = M32(ebx);
    M32(V(0x206f919c)) = eax;
    M32(V(0x206f9126)) += 4;
}

/* 0x206e6796 -- 0x47: the eye's scale.  GLOBAL +0xd42 from +0xd44; the
 * operand's low byte to +0x31c and 1 << (32 - it) to +0x320 (the count
 * masked); the eye's position (+0x300, +0x30c, +0x318) into +0xcdc..+0xce4,
 * shifted up by 32 - the operand when the operand is below 32 (signed, as
 * a word), unshifted otherwise; the scale word 0x206f9159 cleared.  The
 * operand reaches cx through the stack (pushed as bx, popped as cx). */
static void op_47(void)
{
    uint16_t operand;

    edi = G;
    SET_AX(M16(edi + 0xd44));
    M16(edi + 0xd42) = AX;
    SET_BX(M16(esi + 2));
    M8(edi + 0x31c) = BL;
    eax = 1;
    SET_CL(0x20 - BL);
    eax = shl32(eax, CL);
    M32(edi + 0x320) = eax;
    M32(V(0x206f9126)) += 4;
    SET_CX(BX);
    operand = CX;
    ebp = M32(edi + 0x300);
    esi = M32(edi + 0x318);
    edi = M32(edi + 0x30c);
    SET_CX(CX - 0x20);
    if ((int16_t)operand < 0x20) {              /* not jg, not je */
        SET_CX(-CX);
        ebp = shl32(ebp, CL);
        edi = shl32(edi, CL);
        esi = shl32(esi, CL);
    }
    eax = G;                                    /* 0x206e67f3 */
    M32(eax + 0xcdc) = ebp;
    M32(eax + 0xce0) = edi;
    M32(eax + 0xce4) = esi;
    M16(V(0x206f9159)) = 0;
}

/* 0x206e8a2b -- 0x14: the word operand to GLOBAL +0x3c0 (the surface
 * colour). */
static void op_14(void)
{
    edi = G;
    esi += 2;
    lods16();
    eax = AX;
    M32(edi + 0x3c0) = eax;
    M32(V(0x206f9126)) += 4;
}

/* 0x206e8ab5 / 0x206e8ad6 -- 0x51, 0x52: the signed operand to GLOBAL
 * +0x2ec (the line colour) with +0xcc2 all ones, or to +0x3c0 with +0xcc8. */
static void op_51(void)
{
    edi = G;
    eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    M32(edi + 0x2ec) = eax;
    M16(edi + 0xcc2) = 0xffff;
    M32(V(0x206f9126)) += 4;
}

static void op_52(void)
{
    edi = G;
    eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    M32(edi + 0x3c0) = eax;
    M16(edi + 0xcc8) = 0xffff;
    M32(V(0x206f9126)) += 4;
}

/* 0x206e8b58 / 0x206e8b99 -- 0x65, 0x67: a variable's word to GLOBAL +0x3c0
 * or +0x2ec. */
static void op_65(void)
{
    variable_at(esi + 2);
    eax = M16(ebx);
    esi += 4;
    M32(V(0x206f9126)) = esi;
    edi = G;
    M32(edi + 0x3c0) = eax;
}

static void op_67(void)
{
    variable_at(esi + 2);
    eax = M16(ebx);
    esi += 4;
    M32(V(0x206f9126)) = esi;
    edi = G;
    M32(edi + 0x2ec) = eax;
}

/* 0x206e8c4b -- 0x87: the operand to GLOBAL +0xcbc, and +0xcbe from 1 to 2. */
static void op_87(void)
{
    esi += 2;
    lods16();
    M32(V(0x206f9126)) = esi;
    edi = G;
    M16(edi + 0xcbc) = AX;
    if (M16(edi + 0xcbe) == 1)
        M16(edi + 0xcbe) = M16(edi + 0xcbe) + 1;
}

/* 0x206f075e -- 0x58: four dwords to GLOBAL +0xca4..+0xcb0. */
static void op_58(void)
{
    esi += 2;
    lods32();
    edi = G;
    M32(edi + 0xca4) = eax;
    lods32();
    M32(edi + 0xca8) = eax;
    lods32();
    M32(edi + 0xcac) = eax;
    lods32();
    M32(edi + 0xcb0) = eax;
    M32(V(0x206f9126)) = esi;
}

/* 0x206f06b4 -- 0x5e: with the operand's low bits 2, the eye's x and z and
 * the cofactors' signs flipped (a mirror). */
static void op_5e(void)
{
    SET_AX(M16(esi + 2));
    SET_AX(AX & 3);
    if (AX == 2) {
        edi = G;
        M32(edi + 0xcdc) = (uint32_t)-(int32_t)M32(edi + 0xcdc);
        M32(edi + 0xce4) = (uint32_t)-(int32_t)M32(edi + 0xce4);
        M16(edi + 0xcfa) = (uint16_t)-(int16_t)M16(edi + 0xcfa);
        M16(edi + 0xcfe) = (uint16_t)-(int16_t)M16(edi + 0xcfe);
        M16(edi + 0xd00) = (uint16_t)-(int16_t)M16(edi + 0xd00);
        M16(edi + 0xd04) = (uint16_t)-(int16_t)M16(edi + 0xd04);
        M16(edi + 0xd06) = (uint16_t)-(int16_t)M16(edi + 0xd06);
        M32(edi + 0xd0c) = (uint32_t)-(int32_t)M32(edi + 0xd0c);
        M16(edi + 0xd0a) = (uint16_t)-(int16_t)M16(edi + 0xd0a);
        M32(edi + 0xd14) = (uint32_t)-(int32_t)M32(edi + 0xd14);
    }
    M32(V(0x206f9126)) += 4;
}

/* ---- variables and branches: .text+0x10a70..+0x10e72 ----------------------------- */

/* 0x206f1a70 -- 0x7d: 0x206f90f4 set. */
static void op_7d(void)
{
    M16(V(0x206f90f4)) = 1;
    esi += 2;
    M32(V(0x206f9126)) = esi;
}

/* 0x206f1a90 -- the variable table: GLOBAL's address added to every entry,
 * so an entry that was an offset into GLOBAL is that field's address, and
 * one that was 0 is GLOBAL itself, which the lookup sends to the scratch
 * word.  Run again (a second init), it adds again. */
static void init_variables(void)
{
    esi = V(0x206ff950);
    edi = G;
    do {
        M32(esi) += edi;
        esi += 4;
    } while (esi != V(0x207087e0));
}

/* 0x206f1aaa -- 0x26: a word to a variable. */
static void op_26(void)
{
    esi += 2;
    lods16();
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
    lods16();
    M16(ebx) = AX;
    M32(V(0x206f9126)) = esi;
}

/* 0x206f1adf -- 0x7e: the same, always through the table (no context). */
static void op_7e(void)
{
    uint32_t keep = M32(V(0x206f91b7));

    M32(V(0x206f91b7)) = 0;
    variable_at(esi + 2);
    SET_AX(M16(esi + 4));
    M16(ebx) = AX;
    M32(V(0x206f91b7)) = keep;
    M32(V(0x206f9126)) += 6;
}

/* 0x206f1b2c -- 0x48: a zero operand ends the context. */
static void op_48(void)
{
    SET_AX(M16(esi + 2));
    esi += 4;
    M32(V(0x206f9126)) = esi;
    if (AX == 0)
        M32(V(0x206f91b7)) = 0;
}

/* The table's address for the variable in ebx, for the moves below, which
 * do not look at the context: the entry, or GLOBAL + the number through the
 * scratch hook (0x206f1c5c leaves ebx the scratch word and eax 0, and the
 * moves then use ebx). */
static void table_address(void)
{
    edi = M32(V(0x206ff950) + ebx * 4);
    if (edi == G) {
        eax = ebx;
        variable_unmapped();
        edi = ebx;
    }
}

/* 0x206f1b60 -- 0x4d: variable to variable, the first operand the target
 * (through the table), the second the source. */
static void op_4d(void)
{
    ebx = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    table_address();
    variable_at(esi + 4);
    SET_AX(M16(ebx));
    M16(edi) = AX;
    M32(V(0x206f9126)) += 6;
}

/* 0x206f1bb2 -- 0x4e: the other way round. */
static void op_4e(void)
{
    ebx = (uint32_t)(int32_t)(int16_t)M16(esi + 4);
    table_address();
    variable_at(esi + 2);
    SET_AX(M16(edi));
    M16(ebx) = AX;
    M32(V(0x206f9126)) += 6;
}

/* 0x206f1c04 -- 0x4f: both through the table, the variable numbers
 * unsigned. */
static void op_4f(void)
{
    ebx = M16(esi + 4);
    table_address();
    SET_CX(M16(edi));
    ebx = M16(esi + 2);
    table_address();
    M16(edi) = CX;
    M32(V(0x206f9126)) += 6;
}

/* 0x206f1c4a -- 0x89: the dword operand as the variable context. */
static void op_89(void)
{
    eax = M32(esi + 2);
    esi += 6;
    M32(V(0x206f9126)) = esi;
    M32(V(0x206f91b7)) = eax;
}

/* 0x206f1c70 -- 0x24, if in range: on by the first operand unless the
 * variable lies within [lo, hi], else past the instruction (ten bytes). */
static int in_range(uint32_t at)
{
    variable_at(at);
    SET_BX(M16(ebx));
    return !((int16_t)BX < (int16_t)M16(at + 2) || (int16_t)BX > (int16_t)M16(at + 4));
}

static void op_24(void)
{
    ecx = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    if (in_range(esi + 4))
        ecx = 0xa;
    M32(V(0x206f9126)) += ecx;
}

/* 0x206f1cb7 -- 0x1c: two variables in range (sixteen bytes). */
static void op_1c(void)
{
    ecx = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    if (in_range(esi + 4) && in_range(esi + 0xa))
        ecx = 0x10;
    M32(V(0x206f9126)) += ecx;
}

/* 0x206f1d35 -- 0x21: three (twenty-two bytes). */
static void op_21(void)
{
    ecx = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    if (in_range(esi + 4) && in_range(esi + 0xa) && in_range(esi + 0x10))
        ecx = 0x16;
    M32(V(0x206f9126)) += ecx;
}

/* 0x206f1dea -- 0x39, if masked: on by the first operand unless the
 * variable has a bit of the mask. */
static void op_39(void)
{
    esi += 2;
    lods16();
    edx = (uint32_t)(int32_t)(int16_t)AX;
    lods16();
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
    lods16();
    SET_AX(AX & M16(ebx));
    if (AX != 0)
        edx = 8;
    M32(V(0x206f9126)) += edx;
}

/* 0x206f1e2b -- 0x03, case: the variable's value (unsigned) indexes the
 * table of signed offsets after the count.  A value at or past the count
 * was meant to take the default before the table (bx 0xffff, index -1 in
 * 16-bit code), but bx is doubled in ebx, zero-extended, so retail reads
 * its offset 0x20006 bytes on from the instruction instead.  Reproduced. */
static void op_03(void)
{
    SET_CX(M16(esi + 2));
    variable_at(esi + 4);
    ebx = M16(ebx);
    if (BX >= CX)
        SET_BX(0xffff);
    ebx += ebx;
    ebx = ebx + esi + 8;
    eax = (uint32_t)(int32_t)(int16_t)M16(ebx);
    M32(V(0x206f9126)) += eax;
}

/* ---- drawing: .text+0x68a..+0x1545 -------------------------------------------------- */

/* The drawing calls into HG2D, on the current window (GLOBAL +0x1194):
 * the record itself for +0x44, its surface at +0x80 for the rest. */
#define HG2D_POINT(s, x, y, z)        (eax = IMP4(HG2D, 0x24, s, x, y, z))
#define HG2D_LINE(s, x1, y1, x2, y2)  (eax = import_call(M32(HG2D + 0x38), \
                                       (const uint32_t[]){ s, x1, y1, x2, y2 }, 5, HG2D))
#define HG2D_POLYGON(s)               (eax = IMP1(HG2D, 0x40, s))
#define HG2D_VERTEX(rec, x, y, w)     (eax = IMP4(HG2D, 0x44, rec, x, y, w))
#define HG2D_COLOUR(colour, out)      (eax = IMP2(HG2D, 0x88, colour, out))

/* div r/m32 of edx:eax, with the host's repair: all ones on a fault. */
static void div32(uint32_t d)
{
    uint64_t n = ((uint64_t)edx << 32) | eax;

    if (d != 0 && n / d <= 0xffffffffu) {
        eax = (uint32_t)(n / d);
        edx = (uint32_t)(n % d);
        return;
    }
    eax = 0xffffffffu;
    edx = 0xffffffffu;
}

static void clip_line_a(void);
static void clip_line_b(void);
static void line_cached(void);
static void line_project(void);

/* 0x206e168a -- a line from the vertex at 0x206f90c0 to the one at
 * 0x206f90cc, by their outcodes: dropped when both are off one side,
 * clipped end by end (five or ten tries at 0x206f9167), and drawn from the
 * cached first end (0x206f9120 clear) or projected afresh.  With
 * 0x206f90f6 2 it is drawn unclipped. */
static void line(void)
{
    if (M16(V(0x206f90f6)) == 2)
        goto draw;
    SET_CL(M8(V(0x206f911c)));
    SET_CH(M8(V(0x206f911e)));
    SET_CH(CH & CL);
    if (CH != 0)
        goto done;
    if (CL != 0)
        goto clip_a;
    M8(V(0x206f9167)) = 5;
clip_b:                                         /* 0x206e16b3 */
    SET_CH(M8(V(0x206f911e)));
    if (CH != 0) {
        clip_line_b();                          /* 0x206e1705 */
        M8(V(0x206f9167)) = (uint8_t)(M8(V(0x206f9167)) - 1);
        if (M8(V(0x206f9167)) != 0)
            goto clip_b;
        goto done;
    }
    ebx = M32(V(0x206f9161));
    M8(ebx) = M8(ebx) & 0x7f;
draw:                                           /* 0x206e16c6 */
    if (M8(V(0x206f9120)) & 1)
        line_project();
    else
        line_cached();
    return;
clip_a:                                         /* 0x206e16db */
    M8(V(0x206f9167)) = 0xa;
    for (;;) {
        clip_line_a();                          /* 0x206e16e2 */
        SET_CL(M8(V(0x206f911c)));
        SET_CH(M8(V(0x206f911e)));
        if (CL == 0)
            break;
        SET_CH(CH & CL);
        if (CH != 0)
            goto done;
        M8(V(0x206f9167)) = (uint8_t)(M8(V(0x206f9167)) - 1);
        if (M8(V(0x206f9167)) == 0)
            goto done;
    }
    for (;;) {                                  /* 0x206e1714 */
        SET_CH(M8(V(0x206f911e)));
        if (CH == 0) {
            ebx = M32(V(0x206f9161));
            M8(ebx) = M8(ebx) & 0x7f;
            line_project();
            return;
        }
        clip_line_b();                          /* 0x206e172d */
        M8(V(0x206f9167)) = (uint8_t)(M8(V(0x206f9167)) - 1);
        if (M8(V(0x206f9167)) == 0)
            break;
    }
done:                                           /* 0x206e173a */
    M8(V(0x206f9120)) = 0;
}

/* 0x206e1742..0x206e1773: call/ret wrappers nothing reaches, left out. */

/* 0x206e18c0 -- the crossing of an edge with a clip plane: q = edx * 2^31 /
 * ebx (the host's repair saturating a degenerate edge), then the other
 * coordinate (ecx - esi) and z ([0x206f90d4] - [0x206f90c8]) moved by q,
 * 2 * the 64-bit product's high half added to esi and to z. */
static void crossing(void)
{
    int64_t p;

    TRACE(0x206e18c0);
    eax = 0;
    eax = (edx & 1) << 31;                      /* sar edx,1 / rcr eax,1 */
    edx = sar32(edx, 1);
    idiv32(ebx);
    ebp = eax;
    ecx -= esi;
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    p = (int64_t)((uint64_t)p << 1);            /* add eax,eax / adc edx,edx */
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    esi += edx;
    ecx = eax;
    eax = M32(V(0x206f90d4)) - M32(V(0x206f90c8));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    edx += M32(V(0x206f90c8));
}

/* 0x206e18ee, 0x206e1934, 0x206e1978, 0x206e19be -- the vertex at
 * 0x206f90c0 moved along its edge to 0x206f90cc onto the plane y = -z,
 * y = z, x = -z or x = z. */
static void onto_y_low(void)
{
    ebx = M32(V(0x206f90c4));
    edx = M32(V(0x206f90c8));
    ebx += edx;
    ebx -= M32(V(0x206f90d0));
    ebx -= M32(V(0x206f90d4));
    edx += M32(V(0x206f90c4));
    ecx = M32(V(0x206f90cc));
    esi = M32(V(0x206f90c0));
    crossing();
    M32(V(0x206f90c8)) = edx;
    M32(V(0x206f90c0)) = esi;
    edx = (uint32_t)-(int32_t)edx;
    M32(V(0x206f90c4)) = edx;
}

static void onto_y_high(void)
{
    ebx = M32(V(0x206f90d0));
    edx = M32(V(0x206f90c8));
    ebx += edx;
    ebx -= M32(V(0x206f90c4));
    ebx -= M32(V(0x206f90d4));
    edx -= M32(V(0x206f90c4));
    ecx = M32(V(0x206f90cc));
    esi = M32(V(0x206f90c0));
    crossing();
    M32(V(0x206f90c8)) = edx;
    M32(V(0x206f90c0)) = esi;
    M32(V(0x206f90c4)) = edx;
}

static void onto_x_low(void)
{
    ebx = M32(V(0x206f90c0));
    edx = M32(V(0x206f90c8));
    ebx += edx;
    ebx -= M32(V(0x206f90cc));
    ebx -= M32(V(0x206f90d4));
    edx += M32(V(0x206f90c0));
    ecx = M32(V(0x206f90d0));
    esi = M32(V(0x206f90c4));
    crossing();
    M32(V(0x206f90c8)) = edx;
    M32(V(0x206f90c4)) = esi;
    edx = (uint32_t)-(int32_t)edx;
    M32(V(0x206f90c0)) = edx;
}

static void onto_x_high(void)
{
    ebx = M32(V(0x206f90cc));
    edx = M32(V(0x206f90c8));
    ebx += edx;
    ebx -= M32(V(0x206f90c0));
    ebx -= M32(V(0x206f90d4));
    edx -= M32(V(0x206f90c0));
    ecx = M32(V(0x206f90d0));
    esi = M32(V(0x206f90c4));
    crossing();
    M32(V(0x206f90c8)) = edx;
    M32(V(0x206f90c4)) = esi;
    M32(V(0x206f90c0)) = edx;
}

/* The polygon clipper, 0x206e1a1f..0x206e1ed2: four stages, one per plane
 * (x <= z, x >= -z, y <= z, y >= -z, in that order), each keeping its
 * first vertex, its previous one and whether that was outside, and passing
 * what survives to the next; the last emits to HG2D +0x44.  A vertex comes
 * in as (edx, ecx, eax) = (x, y, z), esi = -z.  The stages jump and call
 * into each other's middles; each entry point is a routine here, a jump a
 * tail call.  The bytes at 0x206f9237..0x206f9239 say a stage has not had
 * its first vertex yet (sar shifts the bit into the carry, and clears it). */

static void stage1(void);
static void stage2(void);
static void stage3(void);
static void emit(void);
static void emit_raw(void);

#define LOAD_VERTEX(at)                                                       \
    (edx = M32(V(at)), ecx = M32(V((at) + 4)), eax = M32(V((at) + 8)),        \
     esi = (uint32_t)-(int32_t)eax)
#define STORE_VERTEX(at)                                                      \
    (M32(V(at)) = edx, M32(V((at) + 4)) = ecx, M32(V((at) + 8)) = eax)

/* sar byte [m],1: the bit out in the carry */
static uint32_t first_bit(uint32_t m)
{
    uint32_t c = M8(m) & 1;

    M8(m) = (uint8_t)((int8_t)M8(m) >> 1);
    return c;
}

/* 0x206e1a77 / 0x206e1ab3 / 0x206e1aef -- a stage's first vertex: kept as
 * the stage's first and the next stage's previous, the next stage's
 * outside flag set by the next plane's test.  Entered at +7 (0x206e1a7e,
 * ...) from a stage whose bit was set, with the clear already done. */
static void first0(void)                        /* 0x206e1a7e */
{
    STORE_VERTEX(0x206f926a);
    STORE_VERTEX(0x206f9282);
    M8(V(0x206f92e3)) = 0;
    if ((int32_t)edx < (int32_t)esi) {
        M8(V(0x206f92e3)) = 1;
        return;
    }
    M8(V(0x206f9238)) = 0;
    /* 0x206e1aba */
    STORE_VERTEX(0x206f92b2);
    STORE_VERTEX(0x206f92ca);
    M8(V(0x206f92e5)) = 0;
    if ((int32_t)ecx > (int32_t)eax) {
        M8(V(0x206f92e5)) = 1;
        return;
    }
    M8(V(0x206f9239)) = 0;
    /* 0x206e1af6 */
    STORE_VERTEX(0x206f923a);
    STORE_VERTEX(0x206f9252);
    M8(V(0x206f92e2)) = 0;
    if ((int32_t)ecx < (int32_t)esi) {
        M8(V(0x206f92e2)) = 1;
        return;
    }
    emit();
}

static void first1(void)                        /* 0x206e1aba */
{
    STORE_VERTEX(0x206f92b2);
    STORE_VERTEX(0x206f92ca);
    M8(V(0x206f92e5)) = 0;
    if ((int32_t)ecx > (int32_t)eax) {
        M8(V(0x206f92e5)) = 1;
        return;
    }
    M8(V(0x206f9239)) = 0;
    STORE_VERTEX(0x206f923a);
    STORE_VERTEX(0x206f9252);
    M8(V(0x206f92e2)) = 0;
    if ((int32_t)ecx < (int32_t)esi) {
        M8(V(0x206f92e2)) = 1;
        return;
    }
    emit();
}

static void first2(void)                        /* 0x206e1af6 */
{
    STORE_VERTEX(0x206f923a);
    STORE_VERTEX(0x206f9252);
    M8(V(0x206f92e2)) = 0;
    if ((int32_t)ecx < (int32_t)esi) {
        M8(V(0x206f92e2)) = 1;
        return;
    }
    emit();
}

/* 0x206e1a1f -- the polygon's first vertex (0x206f90c0). */
static void polygon_first(void)
{
    SET_AL(1);
    M8(V(0x206f9237)) = AL;
    M8(V(0x206f9238)) = AL;
    M8(V(0x206f9239)) = AL;
    LOAD_VERTEX(0x206f90c0);
    if (M16(V(0x206f90f6)) == 2) {
        emit_raw();
        return;
    }
    STORE_VERTEX(0x206f929a);
    M8(V(0x206f92e4)) = 0;
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        return;
    }
    M8(V(0x206f9237)) = 0;
    first0();
}

/* 0x206e1ed4, 0x206e1f13, 0x206e1f52, 0x206e1f91 -- the crossing of the
 * edge from a stage's previous vertex to (edx, ecx, eax) with its plane,
 * left at 0x206f90c0; the registers come back as they went in. */
static void cross_with(uint32_t prev, void (*onto)(void))
{
    uint32_t d = edx, c = ecx, a = eax, s = esi, b = ebx;

    STORE_VERTEX(0x206f90c0);
    M32(V(0x206f90cc)) = M32(V(prev));
    M32(V(0x206f90d0)) = M32(V(prev + 4));
    M32(V(0x206f90d4)) = M32(V(prev + 8));
    onto();
    ebx = b, esi = s, eax = a, ecx = c, edx = d;
}

/* One stage: test the vertex against the plane (`out` when outside);
 * an edge that crosses is cut, the crossing passed on before the vertex
 * when coming in, alone when going out. */
#define STAGE(name, flag_first, first, flag_out, prev, out_test, onto, next)  \
static void name(void)                                                        \
{                                                                             \
    TRACE(name##_at);                                                         \
    if (first_bit(V(flag_first))) {                                           \
        first();                                                              \
        return;                                                               \
    }                                                                         \
    if (M8(V(flag_out)) == 1) {                                               \
        if (out_test) {                                                       \
            STORE_VERTEX(prev);                                               \
            return;                                                           \
        }                                                                     \
        M8(V(flag_out)) = 0;                                                  \
        cross_with(prev, onto);                                               \
        STORE_VERTEX(prev);                                                   \
        LOAD_VERTEX(0x206f90c0);                                              \
        next();                                                               \
        LOAD_VERTEX(prev);                                                    \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    if (out_test) {                                                           \
        M8(V(flag_out)) = 1;                                                  \
        cross_with(prev, onto);                                               \
        STORE_VERTEX(prev);                                                   \
        LOAD_VERTEX(0x206f90c0);                                              \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    STORE_VERTEX(prev);                                                       \
    next();                                                                   \
}

/* 0x206e1c0c, 0x206e1cd2, 0x206e1d98 */
#define stage1_at 0x206e1c0c
#define stage2_at 0x206e1cd2
#define stage3_at 0x206e1d98
STAGE(stage1, 0x206f9237, first0, 0x206f92e3, 0x206f9282,
      (int32_t)edx < (int32_t)esi, onto_x_low, stage2)
STAGE(stage2, 0x206f9238, first1, 0x206f92e5, 0x206f92ca,
      (int32_t)ecx > (int32_t)eax, onto_y_high, stage3)
STAGE(stage3, 0x206f9239, first2, 0x206f92e2, 0x206f9252,
      (int32_t)ecx < (int32_t)esi, onto_y_low, emit)

/* 0x206e1b30 -- the polygon's next vertex (0x206f90c0), into stage 0
 * (x <= z), whose previous vertex is at 0x206f929a. */
static void polygon_next(void)
{
    TRACE(0x206e1b30);
    LOAD_VERTEX(0x206f90c0);
    if (M16(V(0x206f90f6)) == 2) {
        emit_raw();
        return;
    }
    if (M8(V(0x206f92e4)) == 1) {
        if ((int32_t)edx > (int32_t)eax) {
            STORE_VERTEX(0x206f929a);
            return;
        }
        M8(V(0x206f92e4)) = 0;
        cross_with(0x206f929a, onto_x_high);
        STORE_VERTEX(0x206f929a);
        LOAD_VERTEX(0x206f90c0);
        stage1();
        LOAD_VERTEX(0x206f929a);
        stage1();
        return;
    }
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        cross_with(0x206f929a, onto_x_high);
        STORE_VERTEX(0x206f929a);
        LOAD_VERTEX(0x206f90c0);
        stage1();
        return;
    }
    STORE_VERTEX(0x206f929a);
    stage1();
}

/* 0x206e1e60 -- a surviving vertex to HG2D +0x44 (x, y, w), x and y
 * clamped to the w they must lie within (a w of 0 is 0x10000 at 0, 0). */
static void emit(void)
{
    TRACE(0x206e1e60);
    if (eax == 0) {
        eax = 0x10000;                          /* 0x206e1eb1 */
        edx = 0;
        ecx = 0;
        emit_raw();
        return;
    }
    ebx = edx;
    if ((int32_t)ebx < 0)
        ebx = (uint32_t)-(int32_t)ebx;
    if ((int32_t)ebx > (int32_t)eax) {          /* 0x206e1ebc */
        uint32_t neg = (int32_t)edx < 0;

        edx = eax;
        if (neg)
            edx = (uint32_t)-(int32_t)edx;
    }
    ebx = ecx;                                  /* 0x206e1e70 */
    if ((int32_t)ebx < 0)
        ebx = (uint32_t)-(int32_t)ebx;
    if ((int32_t)ebx > (int32_t)eax) {          /* 0x206e1ec8 */
        uint32_t neg = (int32_t)ecx < 0;

        ecx = eax;
        if (neg)
            ecx = (uint32_t)-(int32_t)ecx;
    }
    emit_raw();
}

static void emit_raw(void)                      /* 0x206e1e7c */
{
    ebx = eax;
    edi = G;
    edi = M32(edi + 0x1194);
    HG2D_VERTEX(edi, edx, ecx, ebx);
}

/* 0x206e20ff, 0x206e2131, 0x206e2163, 0x206e2195 -- closing a stage: the
 * edge back to its first vertex cut when it crosses, the crossing passed
 * on. */
#define CLOSE(name, flag_out, prev, out_test, onto, next)                     \
static void name(void)                                                        \
{                                                                             \
    if (M8(V(flag_out)) == 1 ? (out_test) : !(out_test))                      \
        return;                                                               \
    cross_with(prev, onto);                                                   \
    LOAD_VERTEX(0x206f90c0);                                                  \
    next();                                                               \
}

CLOSE(close0, 0x206f92e4, 0x206f929a, (int32_t)edx > (int32_t)eax, onto_x_high, stage1)
CLOSE(close1, 0x206f92e3, 0x206f9282, (int32_t)edx < (int32_t)esi, onto_x_low, stage2)
CLOSE(close2, 0x206f92e5, 0x206f92ca, (int32_t)ecx > (int32_t)eax, onto_y_high, stage3)
CLOSE(close3, 0x206f92e2, 0x206f9252, (int32_t)ecx < (int32_t)esi, onto_y_low, emit)

/* 0x206e1fd0 -- close the polygon: each stage that had a vertex closes
 * back to its first; then, unless GLOBAL +0x2f0 counts down past zero,
 * HG2D +0x40 draws it.  The clamp of GLOBAL +0xd4c >> 16 to 0x13 under
 * the test is computed into eax and lost to the import's own eax. */
static void polygon_close(void)
{
    if (M16(V(0x206f90f6)) != 2) {
        LOAD_VERTEX(0x206f90e4);
        M8(V(0x206f9125)) = 0;
        close0();
        if (first_bit(V(0x206f9237)))
            goto skip;
        LOAD_VERTEX(0x206f926a);
        close1();
        if (first_bit(V(0x206f9238)))
            goto skip;
        LOAD_VERTEX(0x206f92b2);
        close2();
        if (first_bit(V(0x206f9239)))
            goto skip;
        LOAD_VERTEX(0x206f923a);
        close3();
    }
    edi = G;                                    /* 0x206e2071 */
    M32(edi + 0x2f0) -= 1;
    if ((int32_t)M32(edi + 0x2f0) < 0)
        goto skip;
    ebx = M32(V(0x206f9161));
    M8(ebx) = M8(ebx) & 0x7f;
    edi = G;
    if (M16(edi + 0x11bc) == 0 && M32(edi + 0x3c0) == 0xf00e && M16(edi + 0xc90) == 0) {
        eax = M32(edi + 0xd4c);
        eax >>= 16;
        if (eax >= 0x13)
            eax = 0x13;
    }
    edi = M32(edi + 0x1194);
    edi += 0x80;
    HG2D_POLYGON(edi);
skip:                                           /* 0x206e20f0 */
    eax = G;
    M16(eax + 0x2f4) = 0;
}

/* 0x206e21c7 -- clip the line's first end onto the plane its outcode says
 * it is beyond (y first), and its outcode again. */
static void clip_line_a(void)
{
    SET_AL(M8(V(0x206f911c)));
    if ((int8_t)AL <= 3) {
        if (AL & 1)
            onto_y_high();
        else
            onto_y_low();
    } else {
        if (AL & 4)
            onto_x_high();
        else
            onto_x_low();
    }
    outcode_a();
}

/* 0x206e2206 -- the same for the second end, by swapping the two. */
static void swap_ends(void)
{
    uint32_t i, t;

    for (i = 0; i < 12; i += 4) {
        t = M32(V(0x206f90c0) + i);
        M32(V(0x206f90c0) + i) = M32(V(0x206f90cc) + i);
        M32(V(0x206f90cc) + i) = t;
    }
    eax = M32(V(0x206f90d4));
}

static void clip_line_b(void)
{
    swap_ends();
    SET_AL(M8(V(0x206f911e)));
    if ((int8_t)AL <= 3) {
        if (AL & 1)
            onto_y_high();
        else
            onto_y_low();
    } else {
        if (AL & 4)
            onto_x_high();
        else
            onto_x_low();
    }
    swap_ends();
    outcode_b();
}

/* 0x206e2347 / 0x206e2398 -- the line projected: x = vx << 16 * x / (z + 1)
 * + cx << 16, y likewise (the view block at GLOBAL +0x1198: +0, +4 x's
 * scale and centre, +8, +0xc y's), both ends, the second's cached at
 * 0x206f92e6/0x206f92ea for the next line to start from; to HG2D +0x38. */
static uint32_t project(uint32_t coord, uint32_t z, uint32_t scale_at)
{
    uint32_t view = M32(G + 0x1198);

    ebx = M32(view + scale_at);
    ebp = M32(view + scale_at + 4);
    eax = coord;
    ecx = z;
    ebx <<= 16;
    imul32(ebx);
    ecx++;
    idiv32(ecx);
    ebp <<= 16;
    eax += ebp;
    return eax;
}

static void line_second(void)                   /* 0x206e2398 */
{
    uint32_t x2;

    M8(V(0x206f9120)) = 0;
    x2 = project(M32(V(0x206f90cc)), M32(V(0x206f90d4)), 0);
    edx = project(M32(V(0x206f90d0)), M32(V(0x206f90d4)), 8);
    ecx = x2;
    M32(V(0x206f92e6)) = ecx;
    M32(V(0x206f92ea)) = edx;
    eax = G;
    ebx = M32(eax + 0x1194);
    ebx += 0x80;
    HG2D_LINE(ebx, ecx, edx, esi, edi);
}

static void line_project(void)
{
    edi = G;
    esi = project(M32(V(0x206f90c0)), M32(V(0x206f90c8)), 0);
    edi = G;
    edi = project(M32(V(0x206f90c4)), M32(V(0x206f90c8)), 8);
    line_second();
}

/* 0x206e2335 */
static void line_cached(void)
{
    esi = M32(V(0x206f92e6));
    edi = M32(V(0x206f92ea));
    line_second();
}

/* 0x206f4359 -- with GLOBAL +0x1286 set, the line colour (+0x2ec) 0xf015
 * for the highlighted index (0x206fd1fe), else 0xf000; +0x1206 0xff. */
static void highlight_colour(void)
{
    SET_SI(M16(V(0x206fd200)));
    eax = 0xf000;
    if (SI == M16(V(0x206fd1fe)))
        eax = 0xf015;
    M32(edi + 0x2ec) = eax;
    M16(edi + 0x1206) = 0xff;
}

/* 0x206e2439 -- the point at 0x206f90c0, when |x| and |y| (one's
 * complement) are within z: projected, its depth z << GLOBAL +0xd42 /
 * GLOBAL +0x320 (unsigned, the host's repair on a fault), to HG2D +0x24. */
static void point(void)
{
    ebx = M32(V(0x206f90c0));
    ecx = M32(V(0x206f90c4));
    ebp = M32(V(0x206f90c8));
    eax = ebx;
    cdq();
    eax ^= edx;
    if ((int32_t)eax >= (int32_t)ebp)
        return;
    eax = ecx;
    cdq();
    eax ^= edx;
    if ((int32_t)eax >= (int32_t)ebp)
        return;
    esi = M32(V(0x206f9161));
    M8(esi) = M8(esi) & 0x7f;
    { uint32_t t = ebp; ebp = ebx; ebx = t; }
    edi = G;
    eax = M32(edi + 0x1198);
    esi = M32(eax + 8);
    M32(V(0x206f92f6)) = esi;
    esi = M32(eax + 0xc);
    M32(V(0x206f92f2)) = esi;
    eax = M32(V(0x206f92f6));
    eax <<= 16;
    imul32(ecx);
    idiv32(ebx);
    esi = M32(V(0x206f92f2));
    esi <<= 16;
    eax += esi;
    ecx = eax;
    eax = M32(edi + 0x1198);
    esi = M32(eax);
    M32(V(0x206f92f6)) = esi;
    esi = M32(eax + 4);
    M32(V(0x206f92f2)) = esi;
    eax = M32(V(0x206f92f6));
    eax <<= 16;
    imul32(ebp);
    idiv32(ebx);
    esi = M32(V(0x206f92f2));
    esi <<= 16;
    eax += esi;
    ebx = eax;
    if (M16(edi + 0x1286) & 0xff) {
        uint32_t b = ebx, c = ecx;

        ebx >>= 16;
        ecx >>= 16;
        highlight_colour();
        ecx = c, ebx = b;
    }
    {
        uint32_t c = ecx;

        eax = M32(V(0x206f90c8));
        SET_CX(M16(edi + 0xd42));
        eax = shl32(eax, CL);
        ecx = M32(edi + 0x320);
        edx = 0;
        div32(ecx);
        ecx = c;
    }
    edi = M32(edi + 0x1194);
    edi += 0x80;
    ebx >>= 16;
    ecx >>= 16;
    esi = eax;
    HG2D_POINT(edi, ebx, ecx, esi);
}

/* ---- the drawing opcodes: .text+0x27b0..+0x2db4 ------------------------------------- */

/* 0x206e37b0 -- 0x05, surface: drawing on (0x206f9125 up), the polygon's
 * flag (0x206fd6c8, taken and cleared) plus one to GLOBAL +0x2f4, its
 * colour count reset. */
static void op_surface(void)
{
    edi = G;
    M8(V(0x206f9125)) = (uint8_t)(M8(V(0x206f9125)) + 1);
    SET_AX(0);
    { uint16_t t = M16(V(0x206fd6c8)); M16(V(0x206fd6c8)) = AX; SET_AX(t); }
    SET_AX(AX + 1);
    M16(edi + 0x2f4) = AX;
    eax = M32(edi + 0xc10);
    M32(edi + 0xc14) = eax;
    M32(edi + 0x2f0) = 0xffffffffu;
    M32(V(0x206f9126)) += 2;
}

/* 0x206e37ed -- 0x06, start point: in a surface, the polygon's first
 * vertex (kept at 0x206f90e4 for the close); otherwise a line's first end,
 * kept as point 0x3e7 of the table. */
static void op_start(void)
{
    ebx = esi;
    ebx++;
    M32(V(0x206f9126)) = ebx;
    if (M8(V(0x206f9125)) != 0) {
        vertex_a();
        eax = M32(V(0x206f90c0));
        M32(V(0x206f90e4)) = eax;
        eax = M32(V(0x206f90c4));
        M32(V(0x206f90e8)) = eax;
        eax = M32(V(0x206f90c8));
        M32(V(0x206f90ec)) = eax;
        polygon_first();
        return;
    }
    vertex_a();                                 /* 0x206e3828 */
    SET_AL(M8(V(0x206f911c)));
    M8(V(0x206f911f)) = AL;
    eax = M32(V(0x206f90c0));
    M32(V(0x206f90e4)) = eax;
    eax = M32(V(0x206f90c4));
    M32(V(0x206f90e8)) = eax;
    eax = M32(V(0x206f90c8));
    M32(V(0x206f90ec)) = eax;
    M8(V(0x206f9120)) = 1;
    M16(V(0x206fd6ca)) = 0x3e7;
    esi = M32(V(0x206f912a));
    esi += 0x3e70;
    eax = M32(V(0x206f90c0));
    M32(esi) = eax;
    eax = M32(V(0x206f90c4));
    M32(esi + 4) = eax;
    eax = M32(V(0x206f90c8));
    M32(esi + 8) = eax;
}

/* 0x206e3889 -- 0x07, continue point: the polygon's next vertex, or a line
 * from the last end to this one.  A line is drawn only with the line
 * colour (GLOBAL +0x2ec) 0x6900 or GLOBAL +0xdca clear; after a drawn one
 * (0x206f9120 clear) the last end comes back from 0x206f90d8. */
static void op_continue(void)
{
    ebx = esi;
    ebx++;
    M32(V(0x206f9126)) = ebx;
    if (M8(V(0x206f9125)) != 0) {
        vertex_a_only();
        polygon_next();
        return;
    }
    edi = G;
    if (M32(edi + 0x2ec) != 0x6900 && M16(edi + 0xdca) != 0)
        return;
    if (M8(V(0x206f9120)) == 0) {
        eax = M32(V(0x206f90d8));
        M32(V(0x206f90c0)) = eax;
        eax = M32(V(0x206f90dc));
        M32(V(0x206f90c4)) = eax;
        eax = M32(V(0x206f90e0));
        M32(V(0x206f90c8)) = eax;
        SET_AL(M8(V(0x206f911d)));
        M8(V(0x206f911c)) = AL;
    }
    vertex_b();
    M8(V(0x206f911d)) = AL;
    line();
}

/* 0x206e3904 -- 0x08, closure: the polygon closed and drawn (in the map,
 * GLOBAL +0x3c0 0x6800 first); drawing off. */
static void op_close(void)
{
    edi = G;
    if (M16(edi + 0xdca) != 0)
        M32(edi + 0x3c0) = 0x6800;
    polygon_close();
    eax = 0;
    edi = G;
    M16(edi + 0x2f4) = AX;
    M8(V(0x206f9125)) = AL;
    M16(edi + 0xc90) = AX;
    M16(V(0x206fd6c8)) = AX;
    M32(V(0x206f9126)) += 2;
}

/* 0x206e395a -- the polygon count up, its colour count reset. */
static void polygon_count(void)
{
    edi = G;
    M16(edi + 0x2f4) = M16(edi + 0x2f4) + 1;
    eax = M32(edi + 0xc10);
    M32(edi + 0xc14) = eax;
    M32(edi + 0x2f0) = 0xffffffffu;
}

/* 0x206e394d -- 0x09 */
static void op_09(void)
{
    M32(V(0x206f9126)) += 2;
    polygon_count();
}

/* 0x206e3a0b -- 0x0e, define point: the point at the table index (signed,
 * x16) from its three words; the outcode byte (+0xc) whatever 0x206f911c
 * last held, the normaliser's shift (+0xd). */
static void op_define_point(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax <<= 4;
    eax += M32(V(0x206f912a));
    edi = eax;
    SET_AX(M16(esi));
    SET_BX(M16(esi + 2));
    SET_DX(M16(esi + 4));
    point_from_words();
    esi += 6;
    SET_AL(M8(V(0x206f911c)));
    M8(edi + 0xc) = AL;
    SET_AL(M8(V(0x206f9122)));
    M8(edi + 0xd) = AL;
    M32(V(0x206f9126)) += 0xa;
}

/* 0x206e3a6e -- table point eax as the polygon's first vertex. */
static void table_first(void)
{
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    lods32();
    M32(V(0x206f90c0)) = eax;
    M32(V(0x206f90e4)) = eax;
    lods32();
    M32(V(0x206f90c4)) = eax;
    M32(V(0x206f90e8)) = eax;
    lods32();
    M32(V(0x206f90c8)) = eax;
    M32(V(0x206f90ec)) = eax;
    lods16();
    M8(V(0x206f911c)) = AL;
    polygon_first();
}

/* 0x206e3aef -- table point eax as the polygon's next vertex. */
static void table_next(void)
{
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    lods32();
    M32(V(0x206f90c0)) = eax;
    lods32();
    M32(V(0x206f90c4)) = eax;
    lods32();
    M32(V(0x206f90c8)) = eax;
    lods16();
    M8(V(0x206f911c)) = AL;
    polygon_next();
}

/* 0x206e3a4c -- 0x0f, start at a table point: in a surface its first
 * vertex, else the line's next start (0x206fd6ca). */
static void op_start_index(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f9126)) = esi;
    if (M8(V(0x206f9125)) != 0) {
        table_first();
        return;
    }
    M16(V(0x206fd6ca)) = AX;
}

/* 0x206e3b19 -- a line between table points eax (whose high half is the
 * new index's sign, as the exchange left it) and ebx. */
static void table_line(void)
{
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    lods32();
    M32(V(0x206f90c0)) = eax;
    lods32();
    M32(V(0x206f90c4)) = eax;
    lods32();
    M32(V(0x206f90c8)) = eax;
    ebx <<= 4;
    ebx += M32(V(0x206f912a));
    esi = ebx;
    lods32();
    M32(V(0x206f90cc)) = eax;
    lods32();
    M32(V(0x206f90d0)) = eax;
    lods32();
    M32(V(0x206f90d4)) = eax;
    outcode_a();
    outcode_b();
    SET_AL(M8(V(0x206f911c)));
    SET_AL(AL & M8(V(0x206f911e)));
    if (AL != 0)
        return;
    M8(V(0x206f9120)) = 1;
    line();
}

/* 0x206e3aa7 -- 0x10, continue at a table point: the polygon's next vertex,
 * or a line from the last index to this one (which becomes the last). */
static void op_continue_index(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f9126)) = esi;
    if (M8(V(0x206f9125)) != 0) {
        table_next();
        return;
    }
    edi = G;
    if (M32(edi + 0x2ec) != 0x6900 && M16(edi + 0xdca) != 0)
        return;
    ebx = AX;
    { uint16_t t = M16(V(0x206fd6ca)); M16(V(0x206fd6ca)) = AX; SET_AX(t); }
    table_line();
}

/* 0x206e3b81 -- 0x11, a coloured point into the table: HG2D +0x88 turns the
 * colour word into 0x206fd6c4 (kept at 0x206f90f0, and at +0xe); the point
 * from its three words. */
static void op_colour_point(void)
{
    uint16_t index;

    esi += 2;
    lods16();
    index = AX;                                 /* push ax */
    lods16();
    HG2D_COLOUR(AX, V(0x206fd6c4));
    eax = M32(V(0x206fd6c4));
    M32(V(0x206f90f0)) = eax;
    edi = V(0x206f90c0);
    SET_AX(M16(esi));
    SET_BX(M16(esi + 2));
    SET_DX(M16(esi + 4));
    point_from_words();
    esi += 6;
    M32(V(0x206f9126)) += 0xc;
    SET_BX(index);                              /* pop bx */
    ebx = BX;
    ebx <<= 4;
    ebx += M32(V(0x206f912a));
    eax = M32(V(0x206f90c0));
    M32(ebx) = eax;
    eax = M32(V(0x206f90c4));
    M32(ebx + 4) = eax;
    eax = M32(V(0x206f90c8));
    M32(ebx + 8) = eax;
    SET_AL(M8(V(0x206f911c)));
    M8(ebx + 0xc) = AL;
    SET_AL(M8(V(0x206f9122)));
    M8(ebx + 0xd) = AL;
    SET_AX(M16(V(0x206f90f0)));
    M16(ebx + 0xe) = AX;
}

/* 0x206e3d21 -- the bounding sphere at (ax, bx, dx) of radius (the word
 * pushed as dx) against the view: point 0 of the table, the radius times
 * 0xb504 (sqrt 2, 1.15) doubled, and the result in ax: 0 out of view, 1
 * partly in, 2 wholly in. */
static void sphere_in_view(uint16_t radius)
{
    uint32_t p;

    M8(V(0x206f9122)) = 0;
    edi = M32(V(0x206f912a));
    SET_DX(CX);
    point_from_words();
    SET_AX(radius);                             /* pop ax */
    SET_SI(AX);
    SET_DX(0xb504);
    p = (uint32_t)AX * DX;
    SET_AX(p);
    SET_DX(p >> 16);
    p = (uint32_t)AX + AX;
    SET_AX(p);
    SET_DX(DX + DX + (p >> 16));
    SET_AX(M16(edi + 0xa));                     /* 0x206e3d4a */
    SET_SI(-SI);
    if ((int16_t)AX <= (int16_t)SI)
        goto out;
    SET_BP(DX);
    SET_DX(-DX);
    SET_SI(DX);
    SET_DX(M16(edi + 2));
    SET_CX(M16(edi + 6));
    SET_BX(DX - AX);
    if ((int16_t)BX >= (int16_t)BP)
        goto out;
    if ((int16_t)BX >= (int16_t)SI)
        goto part;
    SET_BX(DX + AX);
    if ((int16_t)BX <= (int16_t)SI)
        goto out;
    if ((int16_t)BX <= (int16_t)BP)
        goto part;
    SET_BX(CX - AX);
    if ((int16_t)BX >= (int16_t)BP)
        goto out;
    if ((int16_t)BX >= (int16_t)SI)
        goto part;
    SET_BX(CX + AX);
    if ((int16_t)BX <= (int16_t)SI)
        goto out;
    if ((int16_t)BX <= (int16_t)BP)
        goto part;
    SET_AX(2);
    return;
part:
    SET_AX(1);
    return;
out:
    SET_AX(0);
}

/* 0x206e4224 -- 0x2c, reject: on past the instruction (twelve bytes) when
 * the sphere is at least partly in view, else by the first operand. */
static void op_reject(void)
{
    uint16_t skip, x;

    esi += 2;
    lods16();
    skip = AX;
    lods16();
    x = AX;
    lods16();
    SET_BX(AX);
    lods16();
    SET_CX(AX);
    lods16();
    SET_DX(AX);
    SET_AX(x);
    sphere_in_view(DX);
    {
        int in = AX != 0;

        SET_AX(skip);
        eax = (uint32_t)(int32_t)(int16_t)AX;
        if (in)
            eax = 0xc;
    }
    M32(V(0x206f9126)) += eax;
}

/* 0x206e3e95 -- 0x37, point: unless in the map (GLOBAL +0xdca), the point
 * from its three words, drawn. */
static void op_point(void)
{
    edi = G;
    if (M16(edi + 0xdca) == 0) {
        esi += 2;
        edi = V(0x206f90c0);
        SET_AX(M16(esi));
        SET_BX(M16(esi + 2));
        SET_DX(M16(esi + 4));
        point_from_words();
        esi += 6;
        point();
    }
    M32(V(0x206f9126)) += 8;
}

/* 0x206e442c -- 0x80, a table point drawn. */
static void op_point_index(void)
{
    edi = G;
    if (M16(edi + 0xdca) == 0) {
        ebx = M16(esi + 2);
        ebx <<= 4;
        ebx += M32(V(0x206f912a));
        eax = M32(ebx);
        ecx = M32(ebx + 4);
        edx = M32(ebx + 8);
        M32(V(0x206f90c0)) = eax;
        M32(V(0x206f90c4)) = ecx;
        M32(V(0x206f90c8)) = edx;
        point();
    }
    M32(V(0x206f9126)) += 4;
}


/* ---- faces: .text+0x30a4..+0x3350, +0x5c6b..+0x5cda, +0x640b..+0x6dcd ---------- */

/* sar r16 by cl: the count masked to five bits; 16 and over give the sign */
static uint16_t sar16(uint16_t v, uint32_t n)
{
    n &= 31;
    if (n > 15)
        n = 15;
    return (uint16_t)((int16_t)v >> n);
}

/* 0x206e6c6b -- the light on the normal at esi: minus twice the high words
 * of its products with GLOBAL +0xb2a..+0xb32 (the sun's direction), summed
 * as words, when that is negative; else 0. */
static void shade(void)
{
    uint32_t keep = edi;

    edi = G;
    SET_AX(M16(esi));
    imul16(M16(edi + 0xb2a));
    SET_CX(DX);
    SET_AX(M16(esi + 2));
    imul16(M16(edi + 0xb2e));
    SET_CX(CX + DX);
    SET_AX(M16(esi + 4));
    imul16(M16(edi + 0xb32));
    SET_CX(CX + DX);
    SET_AX(CX);
    SET_AX(AX + AX);
    if (!(AX & 0x8000))
        SET_AX(0);
    SET_AX(-AX);
    edi = keep;
}

/* 0x206e6cab -- the light in ax to the face's shade, GLOBAL +0xcc8: the
 * ambient level (0x206f91bb) plus the light over the rest of the range. */
static void shade_colour(void)
{
    uint32_t p;

    SET_BX(0x7fff - M16(V(0x206f91bb)));
    SET_BX(BX + BX);
    SET_AX(AX + AX);
    p = (uint32_t)AX * BX;
    SET_AX(p);
    SET_DX(p >> 16);
    SET_DX(DX + M16(V(0x206f91bb)));
    SET_DX(DX + M16(V(0x206f91bb)));
    edi = G;
    M16(edi + 0xcc8) = DX;
}

/* 0x206e740b -- in the map: the normal at esi against GLOBAL +0xb2a..+0xb32,
 * or against the words at 0x206fd901..0x206fd909 with 0x206f91bd set; the
 * 16x16 products summed in dx:ax, the sign of dx the answer. */
static void map_facing(void)
{
    uint32_t s1, a0, a1, a2;

    if (M16(V(0x206f91bd)) != 0) {
        a0 = V(0x206fd901), a1 = V(0x206fd905), a2 = V(0x206fd909);
    } else {
        edi = G;
        a0 = edi + 0xb2a, a1 = edi + 0xb2e, a2 = edi + 0xb32;
    }
    SET_AX(M16(a0));
    imul16(M16(esi));
    SET_BX(AX);
    SET_CX(DX);
    SET_AX(M16(a1));
    imul16(M16(esi + 2));
    s1 = (uint32_t)BX + AX;
    SET_BX(s1);
    SET_CX(CX + DX + (s1 >> 16));
    SET_AX(M16(a2));
    imul16(M16(esi + 4));
    s1 = (uint32_t)AX + BX;
    SET_AX(s1);
    SET_DX(DX + CX + (s1 >> 16));
    sf = DX >> 15;
}

/* 0x206e7c4c -- which side of the plane at esi (normal words, distance
 * dword at +6) the eye is on: the sign flag set when it is behind.  The
 * normal's words as 16.16 times the eye (GLOBAL +0xcdc..+0xce4), 64-bit,
 * less the distance from the high half; with the scale word 0x206f9159 set,
 * the distance scaled down by it.  In the map, map_facing() answers. */
static void plane_facing(void)
{
    int64_t s1;

    edi = G;
    if (M16(edi + 0xdca) != 0) {
        map_facing();
        return;
    }
    SET_AX(M16(esi));
    eax <<= 16;
    imul32(M32(edi + 0xcdc));
    s1 = (int64_t)(((uint64_t)edx << 32) | eax);
    SET_AX(M16(esi + 2));
    eax <<= 16;
    imul32(M32(edi + 0xce0));
    s1 += (int64_t)(((uint64_t)edx << 32) | eax);
    SET_AX(M16(esi + 4));
    eax <<= 16;
    imul32(M32(edi + 0xce4));
    s1 += (int64_t)(((uint64_t)edx << 32) | eax);
    if (M16(V(0x206f9159)) == 0) {
        ebx = (uint32_t)(s1 - (int64_t)(((uint64_t)edx << 32) | eax));
        ecx = (uint32_t)((uint64_t)(s1 - (int64_t)(((uint64_t)edx << 32) | eax)) >> 32);
        eax = (uint32_t)s1;
        edx = (uint32_t)((uint64_t)s1 >> 32);
        edx -= M32(esi + 6);
        sf = edx >> 31;
        return;
    }
    ecx = (uint32_t)s1;                         /* 0x206e7ca4 */
    ebx = (uint32_t)((uint64_t)s1 >> 32);
    eax = M32(esi + 6);
    SET_CX(M16(V(0x206f9159)));
    eax = sar32(eax, CL);
    ebx -= eax;
    sf = ebx >> 31;
}

/* 0x206e7ce5 -- whether the face whose point is at esi and normal at
 * esi+6 faces the eye: the sign flag set when it does.  (point << 16 - eye)
 * . (normal << 16), 64-bit, by the high half; with the scale word set the
 * point's words are scaled down first.  In the map, the negated sum of
 * map_facing() on the normal. */
static void face_facing(void)
{
    int64_t s1;
    uint32_t n, c;

    edi = G;
    if (M16(edi + 0xdca) != 0) {
        uint32_t keep = esi;

        esi += 6;
        map_facing();
        c = AX != 0;
        SET_AX(-AX);
        SET_DX(~DX);
        SET_DX(DX + 1 - c);                     /* sbb dx,0xffff */
        sf = DX >> 15;
        esi = keep;
        return;
    }
    if (M16(V(0x206f9159)) == 0) {
        SET_AX(M16(esi));
        eax <<= 16;
        eax -= M32(edi + 0xcdc);
        SET_DX(M16(esi + 6));
        edx <<= 16;
        imul32(edx);
        s1 = (int64_t)(((uint64_t)edx << 32) | eax);
        SET_AX(M16(esi + 2));
        eax <<= 16;
        eax -= M32(edi + 0xce0);
        SET_DX(M16(esi + 8));
        edx <<= 16;
        imul32(edx);
        s1 += (int64_t)(((uint64_t)edx << 32) | eax);
        SET_AX(M16(esi + 4));
        eax <<= 16;
        eax -= M32(edi + 0xce4);
        SET_DX(M16(esi + 0xa));
        edx <<= 16;
        imul32(edx);
        s1 += (int64_t)(((uint64_t)edx << 32) | eax);
        ebx = (uint32_t)s1;
        ecx = (uint32_t)((uint64_t)s1 >> 32);
        sf = ecx >> 31;
        return;
    }
    SET_CX(M16(V(0x206f9159)));                 /* 0x206e7d64 */
    n = CL;
    SET_AX(sar16(M16(esi), n));
    eax <<= 16;
    eax -= M32(edi + 0xcdc);
    SET_DX(M16(esi + 6));
    edx <<= 16;
    imul32(edx);
    s1 = (int64_t)(((uint64_t)edx << 32) | eax);
    SET_AX(sar16(M16(esi + 2), n));
    eax <<= 16;
    edx = G;
    eax -= M32(edx + 0xce0);
    SET_DX(M16(esi + 8));
    edx <<= 16;
    imul32(edx);
    s1 += (int64_t)(((uint64_t)edx << 32) | eax);
    SET_AX(sar16(M16(esi + 4), n));
    eax <<= 16;
    edx = G;
    eax -= M32(edx + 0xce4);
    SET_DX(M16(esi + 0xa));
    edx <<= 16;
    imul32(edx);
    s1 += (int64_t)(((uint64_t)edx << 32) | eax);
    ebx = (uint32_t)s1;
    edi = (uint32_t)((uint64_t)s1 >> 32);
    sf = edi >> 31;
}

/* The vertices of a face, table indices from esi: the first, then the
 * rest.  dec ecx then loop: a face of one vertex (or none) wraps ecx and
 * walks some four billion. */
static void face_vertices(void)
{
    uint32_t keep_ecx, keep_esi;

    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    keep_ecx = ecx, keep_esi = esi;
    table_first();
    esi = keep_esi, ecx = keep_ecx;
    ecx--;
    do {
        lods16();
        eax = (uint32_t)(int32_t)(int16_t)AX;
        keep_ecx = ecx, keep_esi = esi;
        table_next();
        esi = keep_esi, ecx = keep_ecx;
    } while (--ecx != 0);
}

/* 0x206e40a4 -- 0x1d, face: when it faces the eye, a surface of the table
 * points listed, shaded (not in the map) and closed; else skipped.  It runs
 * the surface opcode's handler as a subroutine, counter step and all. */
static void op_face(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    {
        uint32_t count = eax;

        face_facing();
        ecx = count;
    }
    if (!sf) {
        ecx += ecx;                             /* 0x206e411d */
        ecx += 0x10;
        M32(V(0x206f9126)) += ecx;
        return;
    }
    esi += 6;
    edi = G;
    if (M16(edi + 0xdca) == 0) {
        uint32_t keep = ecx;

        shade();
        shade_colour();
        ecx = keep;
    }
    esi += 6;
    op_surface();
    face_vertices();
    M32(V(0x206f9126)) = esi;
    edi = G;
    if (M16(edi + 0xdca) != 0)
        M32(edi + 0x3c0) = 0x6800;
    polygon_close();
}

/* 0x206e4297 -- 0x3e, facet: the same with a plane (normal, distance)
 * instead of a point and normal; in the map not closed at all while
 * GLOBAL +0x1298 is set.  Drawing off after, either way. */
static void op_facet(void)
{
    uint32_t count;

    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    count = eax;                                /* push eax */
    plane_facing();
    if (sf) {
        esi += 0xa;                             /* 0x206e433c */
        eax = count;
        eax += eax;
        eax += esi;
        M32(V(0x206f9126)) = eax;
        goto off;
    }
    edi = G;
    if (M16(edi + 0xdca) == 0) {
        shade();
        shade_colour();
    }
    esi += 0xa;
    ecx = count;
    op_surface();
    face_vertices();
    M32(V(0x206f9126)) = esi;
    edi = G;
    if (M16(edi + 0xdca) != 0) {
        if (M32(edi + 0x1298) != 0)
            goto off;
        M32(edi + 0x3c0) = 0x6800;
    }
    polygon_close();
off:                                            /* 0x206e431a */
    edi = G;
    SET_AX(0);
    M16(edi + 0xc90) = AX;
    M8(V(0x206f9125)) = AL;
    M16(V(0x206f90f4)) = AX;
    M16(V(0x206fd6c8)) = AX;
}

/* 0x206e4209 -- 0x25, separation plane: on by the first operand when the
 * eye is behind the plane, else past the instruction (fourteen bytes). */
static void op_separation(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    {
        uint32_t skip = eax;

        plane_facing();
        eax = skip;
    }
    if (!sf)
        eax = 0xe;
    M32(V(0x206f9126)) += eax;
}


/* ---- the shaded clipper: .text+0xa900..+0xb2ff -------------------------------------- */

/* The same four stages as the flat clipper, each vertex carrying a shade
 * in ebx (a word << 16), kept 0x14 past each stage's vertex slot and
 * interpolated at every crossing (0x206ff7d0 towards 0x206ff7d4).  Unlike
 * the flat one there is no unclipped path and no write through
 * 0x206f9161.  Survivors go to HG2D +0x74 with the shade's word; the close
 * draws through +0x78. */

#define HG2D_SHADED_VERTEX(rec, x, y, w, shade) \
    (eax = import_call(M32(HG2D + 0x74), (const uint32_t[]){ rec, x, y, w, shade }, 5, HG2D))
#define HG2D_SHADED_POLYGON(s) (eax = IMP1(HG2D, 0x78, s))

#define LOAD_SHADED(at)                                                       \
    (LOAD_VERTEX(at), ebx = M32(V((at) + 0x14)))
#define STORE_SHADED(at)                                                      \
    (STORE_VERTEX(at), M32(V((at) + 0x14)) = ebx)
#define LOAD_CROSSING                                                         \
    (LOAD_VERTEX(0x206f90c0), ebx = M32(V(0x206ff7d0)))

/* 0x206eb900 -- crossing() with the shade moved along by the same q. */
static void crossing_shaded(void)
{
    int64_t p;
    uint32_t keep;

    eax = (edx & 1) << 31;                      /* sar edx,1 / rcr eax,1 */
    edx = sar32(edx, 1);
    idiv32(ebx);
    ebp = eax;
    keep = ecx;
    ecx = M32(V(0x206ff7d4)) - M32(V(0x206ff7d0));
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    M32(V(0x206ff7d0)) += edx;
    eax = ebp;
    ecx = keep;
    ecx -= esi;
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    esi += edx;
    ecx = eax;
    eax = M32(V(0x206f90d4)) - M32(V(0x206f90c8));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    edx += M32(V(0x206f90c8));
}

/* 0x206eb94a, 0x206eb990, 0x206eb9d4, 0x206eba1a -- onto the planes, as
 * onto_y_low..onto_x_high but through crossing_shaded(). */
#define ONTO_SHADED(name, a, b, adj, op, e, f, neg, store_e)                  \
static void name(void)                                                        \
{                                                                             \
    ebx = M32(V(a));                                                          \
    edx = M32(V(0x206f90c8));                                                 \
    ebx += edx;                                                               \
    ebx -= M32(V(b));                                                         \
    ebx -= M32(V(0x206f90d4));                                                \
    edx op M32(V(adj));                                                       \
    ecx = M32(V(e));                                                          \
    esi = M32(V(f));                                                          \
    crossing_shaded();                                                        \
    M32(V(0x206f90c8)) = edx;                                                 \
    M32(V(f)) = esi;                                                          \
    if (neg)                                                                  \
        edx = (uint32_t)-(int32_t)edx;                                        \
    M32(V(store_e)) = edx;                                                    \
}

/* (ebx from, ebx less, edx adjusted by, how, ecx, esi and its store, negated,
 * edx's store) */
ONTO_SHADED(onto_y_low_s, 0x206f90c4, 0x206f90d0, 0x206f90c4, +=, 0x206f90cc, 0x206f90c0, 1, 0x206f90c4)
ONTO_SHADED(onto_y_high_s, 0x206f90d0, 0x206f90c4, 0x206f90c4, -=, 0x206f90cc, 0x206f90c0, 0, 0x206f90c4)
ONTO_SHADED(onto_x_low_s, 0x206f90c0, 0x206f90cc, 0x206f90c0, +=, 0x206f90d0, 0x206f90c4, 1, 0x206f90c0)
ONTO_SHADED(onto_x_high_s, 0x206f90cc, 0x206f90c0, 0x206f90c0, -=, 0x206f90d0, 0x206f90c4, 0, 0x206f90c0)

static void stage1_s(void);
static void stage2_s(void);
static void stage3_s(void);
static void emit_s(void);

/* 0x206ebabe / 0x206ebb06 / 0x206ebb4e: a stage's first vertex */
static void first2_s(void)
{
    STORE_SHADED(0x206f923a);
    STORE_SHADED(0x206f9252);
    M8(V(0x206f92e2)) = 0;
    if ((int32_t)ecx < (int32_t)esi) {
        M8(V(0x206f92e2)) = 1;
        return;
    }
    emit_s();                                   /* 0x206ebb8f */
}

static void first1_s(void)
{
    STORE_SHADED(0x206f92b2);
    STORE_SHADED(0x206f92ca);
    M8(V(0x206f92e5)) = 0;
    if ((int32_t)ecx > (int32_t)eax) {
        M8(V(0x206f92e5)) = 1;
        return;
    }
    M8(V(0x206f9239)) = 0;
    first2_s();
}

static void first0_s(void)
{
    STORE_SHADED(0x206f926a);
    STORE_SHADED(0x206f9282);
    M8(V(0x206f92e3)) = 0;
    if ((int32_t)edx < (int32_t)esi) {
        M8(V(0x206f92e3)) = 1;
        return;
    }
    M8(V(0x206f9238)) = 0;
    first1_s();
}

/* 0x206eba5e -- the shaded polygon's first vertex (0x206f90c0, its shade
 * the word at 0x206f90f0). */
static void shaded_first(void)
{
    SET_AL(1);
    M8(V(0x206f9237)) = AL;
    M8(V(0x206f9238)) = AL;
    M8(V(0x206f9239)) = AL;
    LOAD_VERTEX(0x206f90c0);
    ebx = M32(V(0x206f90f0));
    ebx <<= 16;
    STORE_SHADED(0x206f929a);
    M8(V(0x206f92e4)) = 0;
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        return;
    }
    M8(V(0x206f9237)) = 0;
    first0_s();
}

/* 0x206ebff8, 0x206ec047, 0x206ec096, 0x206ec0e5 */
static void cross_with_s(uint32_t prev, void (*onto)(void))
{
    uint32_t d = edx, c = ecx, a = eax, s = esi, b = ebx;

    STORE_VERTEX(0x206f90c0);
    M32(V(0x206ff7d0)) = ebx;
    M32(V(0x206f90cc)) = M32(V(prev));
    M32(V(0x206f90d0)) = M32(V(prev + 4));
    M32(V(0x206f90d4)) = M32(V(prev + 8));
    M32(V(0x206ff7d4)) = M32(V(prev + 0x14));
    onto();
    ebx = b, esi = s, eax = a, ecx = c, edx = d;
}

#define STAGE_S(name, flag_first, first, flag_out, prev, out_test, onto, next) \
static void name(void)                                                        \
{                                                                             \
    if (first_bit(V(flag_first))) {                                           \
        first();                                                              \
        return;                                                               \
    }                                                                         \
    if (M8(V(flag_out)) == 1) {                                               \
        if (out_test) {                                                       \
            STORE_SHADED(prev);                                               \
            return;                                                           \
        }                                                                     \
        M8(V(flag_out)) = 0;                                                  \
        cross_with_s(prev, onto);                                             \
        STORE_SHADED(prev);                                                   \
        LOAD_CROSSING;                                                        \
        next();                                                               \
        LOAD_SHADED(prev);                                                    \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    if (out_test) {                                                           \
        M8(V(flag_out)) = 1;                                                  \
        cross_with_s(prev, onto);                                             \
        STORE_SHADED(prev);                                                   \
        LOAD_CROSSING;                                                        \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    STORE_SHADED(prev);                                                       \
    next();                                                                   \
}

/* 0x206ebc9a, 0x206ebd8e, 0x206ebe82 */
STAGE_S(stage1_s, 0x206f9237, first0_s, 0x206f92e3, 0x206f9282,
        (int32_t)edx < (int32_t)esi, onto_x_low_s, stage2_s)
STAGE_S(stage2_s, 0x206f9238, first1_s, 0x206f92e5, 0x206f92ca,
        (int32_t)ecx > (int32_t)eax, onto_y_high_s, stage3_s)
STAGE_S(stage3_s, 0x206f9239, first2_s, 0x206f92e2, 0x206f9252,
        (int32_t)ecx < (int32_t)esi, onto_y_low_s, emit_s)

/* 0x206ebb95 -- the shaded polygon's next vertex. */
static void shaded_next(void)
{
    LOAD_VERTEX(0x206f90c0);
    ebx = M32(V(0x206f90f0));
    ebx <<= 16;
    if (M8(V(0x206f92e4)) == 1) {
        if ((int32_t)edx > (int32_t)eax) {
            STORE_SHADED(0x206f929a);
            return;
        }
        M8(V(0x206f92e4)) = 0;
        cross_with_s(0x206f929a, onto_x_high_s);
        STORE_SHADED(0x206f929a);
        LOAD_CROSSING;
        stage1_s();
        LOAD_SHADED(0x206f929a);
        stage1_s();
        return;
    }
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        cross_with_s(0x206f929a, onto_x_high_s);
        STORE_SHADED(0x206f929a);
        LOAD_CROSSING;
        stage1_s();
        return;
    }
    STORE_SHADED(0x206f929a);
    stage1_s();
}

/* 0x206ebf7c -- a survivor, clamped as emit() does, to HG2D +0x74 with
 * the shade's high word. */
static void emit_s(void)
{
    esi = ebx;
    if (eax == 0) {
        eax = 0x10000;                          /* 0x206ebfd5 */
        edx = 0;
        ecx = 0;
    } else {
        ebx = edx;
        if ((int32_t)ebx < 0)
            ebx = (uint32_t)-(int32_t)ebx;
        if ((int32_t)ebx > (int32_t)eax) {
            uint32_t neg = (int32_t)edx < 0;

            edx = eax;
            if (neg)
                edx = (uint32_t)-(int32_t)edx;
        }
        ebx = ecx;
        if ((int32_t)ebx < 0)
            ebx = (uint32_t)-(int32_t)ebx;
        if ((int32_t)ebx > (int32_t)eax) {
            uint32_t neg = (int32_t)ecx < 0;

            ecx = eax;
            if (neg)
                ecx = (uint32_t)-(int32_t)ecx;
        }
    }
    esi = sar32(esi, 16);                       /* 0x206ebf9a */
    edi = G;
    edi = M32(edi + 0x1194);
    HG2D_SHADED_VERTEX(edi, edx, ecx, eax, SI);
}

/* 0x206ec223, 0x206ec25b, 0x206ec293, 0x206ec2cb */
#define CLOSE_S(name, flag_out, prev, out_test, onto, next)                   \
static void name(void)                                                        \
{                                                                             \
    if (M8(V(flag_out)) == 1 ? (out_test) : !(out_test))                      \
        return;                                                               \
    cross_with_s(prev, onto);                                                 \
    LOAD_CROSSING;                                                            \
    next();                                                                   \
}

CLOSE_S(close0_s, 0x206f92e4, 0x206f929a, (int32_t)edx > (int32_t)eax, onto_x_high_s, stage1_s)
CLOSE_S(close1_s, 0x206f92e3, 0x206f9282, (int32_t)edx < (int32_t)esi, onto_x_low_s, stage2_s)
CLOSE_S(close2_s, 0x206f92e5, 0x206f92ca, (int32_t)ecx > (int32_t)eax, onto_y_high_s, stage3_s)
CLOSE_S(close3_s, 0x206f92e2, 0x206f9252, (int32_t)ecx < (int32_t)esi, onto_y_low_s, emit_s)

/* 0x206ec134 -- close the shaded polygon (its first vertex's shade the
 * word at 0x206f921b) and draw it unless GLOBAL +0x2f0 counts past zero. */
static void shaded_close(void)
{
    LOAD_VERTEX(0x206f90e4);
    ebx = M32(V(0x206f921b));
    ebx <<= 16;
    close0_s();
    if (first_bit(V(0x206f9237)))
        goto done;
    LOAD_SHADED(0x206f926a);
    close1_s();
    if (first_bit(V(0x206f9238)))
        goto done;
    LOAD_SHADED(0x206f92b2);
    close2_s();
    if (first_bit(V(0x206f9239)))
        goto done;
    LOAD_SHADED(0x206f923a);
    close3_s();
    edi = G;
    M32(edi + 0x2f0) -= 1;
    if ((int32_t)M32(edi + 0x2f0) < 0)
        goto done;
    edi = M32(edi + 0x1194);
    edi += 0x80;
    HG2D_SHADED_POLYGON(edi);
done:
    eax = G;
    M16(eax + 0x2f4) = 0;
}

/* 0x206e397e -- 0x0a, a shaded polygon's first vertex: past the opcode
 * and its shade word by hand (three bytes, then the point's own +1), the
 * shade to 0x206f90f0 and, as the first's, to 0x206f921b. */
static void op_shaded_start(void)
{
    M32(V(0x206f9126)) += 1;
    M32(V(0x206f9126)) += 2;
    ebx = M32(V(0x206f9126));
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f90f0)) = eax;
    M32(V(0x206f921b)) = eax;
    vertex_a_only();
    eax = M32(V(0x206f90c0));
    M32(V(0x206f90e4)) = eax;
    eax = M32(V(0x206f90c4));
    M32(V(0x206f90e8)) = eax;
    eax = M32(V(0x206f90c8));
    M32(V(0x206f90ec)) = eax;
    shaded_first();
}

/* 0x206e39ca -- 0x0b, its next vertex. */
static void op_shaded_next(void)
{
    M32(V(0x206f9126)) += 1;
    M32(V(0x206f9126)) += 2;
    ebx = M32(V(0x206f9126));
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f90f0)) = eax;
    vertex_a_only();
    shaded_next();
}

/* 0x206e39f3 -- 0x0c, the close. */
static void op_shaded_close(void)
{
    M32(V(0x206f9126)) += 2;
    shaded_close();
}

/* 0x206e3c22 -- 0x12, a shaded first vertex from a table point: its
 * outcode and scale bytes to 0x206f911c/0x206f9121, the shade stored with
 * it (+0xe). */
static void op_shaded_start_index(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f9126)) = esi;
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    lods32();
    M32(V(0x206f90c0)) = eax;
    M32(V(0x206f90e4)) = eax;
    lods32();
    M32(V(0x206f90c4)) = eax;
    M32(V(0x206f90e8)) = eax;
    lods32();
    M32(V(0x206f90c8)) = eax;
    M32(V(0x206f90ec)) = eax;
    lods16();
    M8(V(0x206f911c)) = AL;
    M8(V(0x206f9121)) = AH;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f921b)) = eax;
    M32(V(0x206f90f0)) = eax;
    SET_BX(AX);
    shaded_first();
}

/* 0x206e3c82 -- 0x13, a shaded next vertex from a table point.  The two
 * loops after it (0x206e3cc8, 0x206e3ce6) are reached only from
 * themselves, and 0x206e3db5 from nothing: left out. */
static void op_shaded_next_index(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f9126)) = esi;
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    lods32();
    M32(V(0x206f90c0)) = eax;
    lods32();
    M32(V(0x206f90c4)) = eax;
    lods32();
    M32(V(0x206f90c8)) = eax;
    lods16();
    M8(V(0x206f911c)) = AL;
    lods16();
    SET_BX(AX);
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f90f0)) = eax;
    shaded_next();
}


/* ---- shaded facets and colour ramps: .text+0x5e7d..+0x6046, +0x79b4..+0x7b98 ------- */

#define HG2D_RAMP(colour, flags, out) \
    (eax = IMP3(HG2D, 0x70, colour, flags, out))

/* 0x206e8a0e -- the colour ramp from bx: the low end bl in the high byte
 * (0x206fd8eb), the high end bh - 1 with 0xff below (0x206fd8ed). */
static void ramp_from_bx(void)
{
    SET_AH(BL);
    SET_AL(0);
    SET_BH(BH - 1);
    SET_BL(0xff);
    M16(V(0x206fd8eb)) = AX;
    M16(V(0x206fd8ed)) = BX;
    M32(V(0x206f9126)) += 4;
}

/* 0x206e89be -- eax the surface colour (GLOBAL +0x3c0); HG2D +0x70 gives
 * its ramp, whose high word becomes bx. */
static void ramp_from_colour(void)
{
    edi = G;
    M32(edi + 0x3c0) = eax;
    SET_BX(0xffff);
    HG2D_RAMP(AX, BX, V(0x206fd8fb));
    eax = M32(V(0x206fd8fb));
    ebx = (ebx << 16) | (eax >> 16);            /* shld ebx,eax,16 */
    ramp_from_bx();
}

/* 0x206e89b4 / 0x206e8b1d / 0x206e8b27 -- 0x50, 0x53, 0x66: the ramp from a
 * colour operand, from a ramp operand, from a variable's colour. */
static void op_colour_ramp(void)
{
    SET_AX(M16(esi + 2));
    ramp_from_colour();
}

static void op_ramp(void)
{
    SET_BX(M16(esi + 2));
    ramp_from_bx();
}

static void op_variable_ramp(void)
{
    variable_at(esi + 2);
    eax = M16(ebx);
    ramp_from_colour();
}

/* A shaded facet's vertex: the table point at the index in ax, its x, y
 * and z copied a word at a time (the first also kept for the close), and
 * its stored colour word (+0xe), doubled, as a fraction of the ramp: the
 * shade at 0x206f90f0 (a word), bumped to the next step when it lands on
 * the ramp's own high byte. */
static void facet_vertex(int first)
{
    uint32_t p, i;

    eax = AX;
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    for (i = 0; i < 6; i++) {
        lods16();
        M16(V(0x206f90c0) + 2 * i) = AX;
        if (first)
            M16(V(0x206f90e4) + 2 * i) = AX;
    }
    esi += 2;
    lods16();
    SET_AX(AX + AX);
    SET_DX(AX);
    SET_AX(M16(V(0x206fd8ed)) - M16(V(0x206fd8eb)));
    p = (uint32_t)AX * DX;
    SET_AX(p);
    SET_DX(p >> 16);
    SET_BX(M16(V(0x206fd8eb)));
    SET_DX(DX + BX);
    if (DH == BH) {
        SET_DH(DH + 1);
        SET_DL(0);
    }
    SET_BX(DX);
    M16(V(0x206f90f0)) = BX;
    if (first)
        M16(V(0x206f921b)) = BX;
}

/* 0x206e6e7d -- 0x2a, a shaded facet: as 0x3e, its vertices shaded by
 * their colours along the ramp and drawn through the shaded clipper (the
 * flat one in the map).  The count is a word counted down with dec / jne,
 * so a facet of one vertex walks 65,536. */
static void op_shaded_facet(void)
{
    uint16_t count;

    esi += 2;
    lods16();
    count = AX;                                 /* push ax */
    plane_facing();
    if (sf) {
        SET_AX(count);                          /* pop ax */
        SET_AX(AX + AX);
        SET_AX(AX + 0xe);
        eax = (uint32_t)(int32_t)(int16_t)AX;
        M32(V(0x206f9126)) += eax;
        return;
    }
    esi += 0xa;
    ecx = count;
    polygon_count();
    SET_CX(CX - 1);
    lods16();
    {
        uint16_t c = CX;
        uint32_t keep = esi;

        facet_vertex(1);
        edi = G;
        if (M16(edi + 0xdca) == 0)
            shaded_first();
        else
            polygon_first();
        esi = keep;
        SET_CX(c);
    }
    do {
        lods16();
        {
            uint16_t c = CX;
            uint32_t keep = esi;

            facet_vertex(0);
            edi = G;
            if (M16(edi + 0xdca) == 0)
                shaded_next();
            else
                polygon_next();
            esi = keep;
            SET_CX(c);
        }
        SET_CX(CX - 1);
    } while (CX != 0);
    M32(V(0x206f9126)) = esi;
    edi = G;
    if (M16(edi + 0xdca) == 0) {
        shaded_close();
        return;
    }
    edi = G;
    M32(edi + 0x3c0) = 0x6800;
    polygon_close();
}


/* ---- the textured clipper: .text+0xc710..+0xd29f ------------------------------------ */

/* The four stages once more, each vertex carrying texture coordinates u
 * in ebx and v in ebp (16.16), kept at +0xc/+0x10 of each slot and moved
 * along at every crossing (0x206f921f/0x206f9223 towards 0x206f9227/
 * 0x206f922b).  As the flat clipper, 0x206f90f6 2 skips the clipping.
 * Survivors go to HG2D +0x48; the close draws through +0x4c. */

#define HG2D_TEXTURED_VERTEX(rec, x, y, w, u, v) \
    (eax = import_call(M32(HG2D + 0x48), (const uint32_t[]){ rec, x, y, w, u, v }, 6, HG2D))
#define HG2D_TEXTURED_POLYGON(s) (eax = IMP1(HG2D, 0x4c, s))

#define LOAD_TEXTURED(at)                                                     \
    (LOAD_VERTEX(at), ebx = M32(V((at) + 0xc)), ebp = M32(V((at) + 0x10)))
#define STORE_TEXTURED(at)                                                    \
    (STORE_VERTEX(at), M32(V((at) + 0xc)) = ebx, M32(V((at) + 0x10)) = ebp)
#define LOAD_TEXTURED_CROSSING                                                \
    (LOAD_VERTEX(0x206f90c0), ebx = M32(V(0x206f921f)), ebp = M32(V(0x206f9223)))

/* 0x206ed710 */
static void crossing_textured(void)
{
    int64_t p;

    eax = (edx & 1) << 31;                      /* sar edx,1 / rcr eax,1 */
    edx = sar32(edx, 1);
    idiv32(ebx);
    ebp = eax;
    eax = M32(V(0x206f9227)) - M32(V(0x206f921f));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    M32(V(0x206f921f)) += edx;
    eax = M32(V(0x206f922b)) - M32(V(0x206f9223));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    M32(V(0x206f9223)) += edx;
    eax = ebp;
    ecx -= esi;
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    esi += edx;
    ecx = eax;
    eax = M32(V(0x206f90d4)) - M32(V(0x206f90c8));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    edx += M32(V(0x206f90c8));
}

/* 0x206ed76e, 0x206ed7b4, 0x206ed7f8, 0x206ed83e */
#define ONTO_TEXTURED(name, a, b, adj, op, e, f, neg, store_e)                \
static void name(void)                                                        \
{                                                                             \
    ebx = M32(V(a));                                                          \
    edx = M32(V(0x206f90c8));                                                 \
    ebx += edx;                                                               \
    ebx -= M32(V(b));                                                         \
    ebx -= M32(V(0x206f90d4));                                                \
    edx op M32(V(adj));                                                       \
    ecx = M32(V(e));                                                          \
    esi = M32(V(f));                                                          \
    crossing_textured();                                                      \
    M32(V(0x206f90c8)) = edx;                                                 \
    M32(V(f)) = esi;                                                          \
    if (neg)                                                                  \
        edx = (uint32_t)-(int32_t)edx;                                        \
    M32(V(store_e)) = edx;                                                    \
}

ONTO_TEXTURED(onto_y_low_t, 0x206f90c4, 0x206f90d0, 0x206f90c4, +=, 0x206f90cc, 0x206f90c0, 1, 0x206f90c4)
ONTO_TEXTURED(onto_y_high_t, 0x206f90d0, 0x206f90c4, 0x206f90c4, -=, 0x206f90cc, 0x206f90c0, 0, 0x206f90c4)
ONTO_TEXTURED(onto_x_low_t, 0x206f90c0, 0x206f90cc, 0x206f90c0, +=, 0x206f90d0, 0x206f90c4, 1, 0x206f90c0)
ONTO_TEXTURED(onto_x_high_t, 0x206f90cc, 0x206f90c0, 0x206f90c0, -=, 0x206f90d0, 0x206f90c4, 0, 0x206f90c0)

static void stage1_t(void);
static void stage2_t(void);
static void stage3_t(void);
static void emit_t(void);

/* 0x206edee1 -- a vertex to HG2D +0x48 as it stands. */
static void emit_raw_t(void)
{
    edi = G;
    edi = M32(edi + 0x1194);
    HG2D_TEXTURED_VERTEX(edi, edx, ecx, eax, ebx, ebp);
}

static void first2_t(void)                      /* 0x206ed9a3 */
{
    STORE_TEXTURED(0x206f923a);
    STORE_TEXTURED(0x206f9252);
    M8(V(0x206f92e2)) = 0;
    if ((int32_t)ecx < (int32_t)esi) {
        M8(V(0x206f92e2)) = 1;
        return;
    }
    emit_t();
}

static void first1_t(void)                      /* 0x206ed94f */
{
    STORE_TEXTURED(0x206f92b2);
    STORE_TEXTURED(0x206f92ca);
    M8(V(0x206f92e5)) = 0;
    if ((int32_t)ecx > (int32_t)eax) {
        M8(V(0x206f92e5)) = 1;
        return;
    }
    M8(V(0x206f9239)) = 0;
    first2_t();
}

static void first0_t(void)                      /* 0x206ed8fb */
{
    STORE_TEXTURED(0x206f926a);
    STORE_TEXTURED(0x206f9282);
    M8(V(0x206f92e3)) = 0;
    if ((int32_t)edx < (int32_t)esi) {
        M8(V(0x206f92e3)) = 1;
        return;
    }
    M8(V(0x206f9238)) = 0;
    first1_t();
}

/* 0x206ed882 -- the textured polygon's first vertex (0x206f90c0, its u and
 * v at 0x206f921f/0x206f9223). */
static void textured_first(void)
{
    SET_AL(1);
    M8(V(0x206f9237)) = AL;
    M8(V(0x206f9238)) = AL;
    M8(V(0x206f9239)) = AL;
    LOAD_VERTEX(0x206f90c0);
    ebx = M32(V(0x206f921f));
    ebp = M32(V(0x206f9223));
    if (M16(V(0x206f90f6)) == 2) {
        emit_raw_t();
        return;
    }
    STORE_TEXTURED(0x206f929a);
    M8(V(0x206f92e4)) = 0;
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        return;
    }
    M8(V(0x206f9237)) = 0;
    first0_t();
}

/* 0x206edf16, 0x206edf77, 0x206edfd8, 0x206ee039 */
static void cross_with_t(uint32_t prev, void (*onto)(void))
{
    uint32_t d = edx, c = ecx, a = eax, s = esi, b = ebx, p = ebp;

    STORE_VERTEX(0x206f90c0);
    M32(V(0x206f921f)) = ebx;
    M32(V(0x206f9223)) = ebp;
    M32(V(0x206f90cc)) = M32(V(prev));
    M32(V(0x206f90d0)) = M32(V(prev + 4));
    M32(V(0x206f90d4)) = M32(V(prev + 8));
    M32(V(0x206f9227)) = M32(V(prev + 0xc));
    M32(V(0x206f922b)) = M32(V(prev + 0x10));
    onto();
    ebp = p, ebx = b, esi = s, eax = a, ecx = c, edx = d;
}

#define STAGE_T(name, flag_first, first, flag_out, prev, out_test, onto, next) \
static void name(void)                                                        \
{                                                                             \
    if (first_bit(V(flag_first))) {                                           \
        first();                                                              \
        return;                                                               \
    }                                                                         \
    if (M8(V(flag_out)) == 1) {                                               \
        if (out_test) {                                                       \
            STORE_TEXTURED(prev);                                             \
            return;                                                           \
        }                                                                     \
        M8(V(flag_out)) = 0;                                                  \
        cross_with_t(prev, onto);                                             \
        STORE_TEXTURED(prev);                                                 \
        LOAD_TEXTURED_CROSSING;                                               \
        next();                                                               \
        LOAD_TEXTURED(prev);                                                  \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    if (out_test) {                                                           \
        M8(V(flag_out)) = 1;                                                  \
        cross_with_t(prev, onto);                                             \
        STORE_TEXTURED(prev);                                                 \
        LOAD_TEXTURED_CROSSING;                                               \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    STORE_TEXTURED(prev);                                                     \
    next();                                                                   \
}

/* 0x206edb38, 0x206edc56, 0x206edd74 */
STAGE_T(stage1_t, 0x206f9237, first0_t, 0x206f92e3, 0x206f9282,
        (int32_t)edx < (int32_t)esi, onto_x_low_t, stage2_t)
STAGE_T(stage2_t, 0x206f9238, first1_t, 0x206f92e5, 0x206f92ca,
        (int32_t)ecx > (int32_t)eax, onto_y_high_t, stage3_t)
STAGE_T(stage3_t, 0x206f9239, first2_t, 0x206f92e2, 0x206f9252,
        (int32_t)ecx < (int32_t)esi, onto_y_low_t, emit_t)

/* 0x206ed9f6 -- the textured polygon's next vertex. */
static void textured_next(void)
{
    LOAD_VERTEX(0x206f90c0);
    ebx = M32(V(0x206f921f));
    ebp = M32(V(0x206f9223));
    if (M16(V(0x206f90f6)) == 2) {
        emit_raw_t();
        return;
    }
    if (M8(V(0x206f92e4)) == 1) {
        if ((int32_t)edx > (int32_t)eax) {
            STORE_TEXTURED(0x206f929a);
            return;
        }
        M8(V(0x206f92e4)) = 0;
        cross_with_t(0x206f929a, onto_x_high_t);
        STORE_TEXTURED(0x206f929a);
        LOAD_TEXTURED_CROSSING;
        stage1_t();
        LOAD_TEXTURED(0x206f929a);
        stage1_t();
        return;
    }
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        cross_with_t(0x206f929a, onto_x_high_t);
        STORE_TEXTURED(0x206f929a);
        LOAD_TEXTURED_CROSSING;
        stage1_t();
        return;
    }
    STORE_TEXTURED(0x206f929a);
    stage1_t();
}

/* 0x206ede98 -- a survivor clamped as emit() does, u kept through it. */
static void emit_t(void)
{
    esi = ebx;
    if (eax == 0) {
        eax = 0x10000;                          /* 0x206edebe */
        edx = 0;
        ecx = 0;
        emit_raw_t();
        return;
    }
    ebx = edx;
    if ((int32_t)ebx < 0)
        ebx = (uint32_t)-(int32_t)ebx;
    if ((int32_t)ebx > (int32_t)eax) {
        uint32_t neg = (int32_t)edx < 0;

        edx = eax;
        if (neg)
            edx = (uint32_t)-(int32_t)edx;
    }
    ebx = ecx;
    if ((int32_t)ebx < 0)
        ebx = (uint32_t)-(int32_t)ebx;
    if ((int32_t)ebx > (int32_t)eax) {
        uint32_t neg = (int32_t)ecx < 0;

        ecx = eax;
        if (neg)
            ecx = (uint32_t)-(int32_t)ecx;
    }
    ebx = esi;
    emit_raw_t();
}

/* 0x206ee1ac, 0x206ee1ea, 0x206ee228, 0x206ee266 */
#define CLOSE_T(name, flag_out, prev, out_test, onto, next)                   \
static void name(void)                                                        \
{                                                                             \
    if (M8(V(flag_out)) == 1 ? (out_test) : !(out_test))                      \
        return;                                                               \
    cross_with_t(prev, onto);                                                 \
    LOAD_TEXTURED_CROSSING;                                                   \
    next();                                                                   \
}

CLOSE_T(close0_t, 0x206f92e4, 0x206f929a, (int32_t)edx > (int32_t)eax, onto_x_high_t, stage1_t)
CLOSE_T(close1_t, 0x206f92e3, 0x206f9282, (int32_t)edx < (int32_t)esi, onto_x_low_t, stage2_t)
CLOSE_T(close2_t, 0x206f92e5, 0x206f92ca, (int32_t)ecx > (int32_t)eax, onto_y_high_t, stage3_t)
CLOSE_T(close3_t, 0x206f92e2, 0x206f9252, (int32_t)ecx < (int32_t)esi, onto_y_low_t, emit_t)

/* 0x206ee09a -- close the textured polygon (its first vertex's u and v at
 * 0x206f922f/0x206f9233) and draw it unless GLOBAL +0x2f0 counts past
 * zero. */
static void textured_close(void)
{
    if (M16(V(0x206f90f6)) != 2) {
        LOAD_VERTEX(0x206f90e4);
        ebx = M32(V(0x206f922f));
        ebp = M32(V(0x206f9233));
        close0_t();
        if (first_bit(V(0x206f9237)))
            goto done;
        LOAD_TEXTURED(0x206f926a);
        close1_t();
        if (first_bit(V(0x206f9238)))
            goto done;
        LOAD_TEXTURED(0x206f92b2);
        close2_t();
        if (first_bit(V(0x206f9239)))
            goto done;
        LOAD_TEXTURED(0x206f923a);
        close3_t();
    }
    edi = G;                                    /* 0x206ee160 */
    M32(edi + 0x2f0) -= 1;
    if ((int32_t)M32(edi + 0x2f0) < 0)
        goto done;
    edi = M32(edi + 0x1194);
    edi += 0x80;
    HG2D_TEXTURED_POLYGON(edi);
done:
    eax = G;
    M16(eax + 0x2f4) = 0;
}

/* ---- textured facets: .text+0xf850..+0xfd6d ----------------------------------------- */

/* 0x206f09c6 -- a textured facet's start: GLOBAL +0xc90 from +0xcb6 (its
 * low byte rotated in) out of the map, else 0; the polygon counted. */
static void textured_start(void)
{
    uint32_t keep_esi = esi, keep_ecx = ecx;

    edi = G;
    M16(edi + 0xc90) = 0;
    if (M16(edi + 0xdca) == 0) {
        SET_AX(M16(edi + 0xcb6));
        SET_AL((uint8_t)((AL >> 1) | (AL << 7)));
        SET_AX(AX | M16(edi + 0xcb6));
        M16(edi + 0xc90) = AX;
    }
    polygon_count();
    ecx = keep_ecx, esi = keep_esi;
}

/* A textured facet's vertex (index, u, v at esi): u and v plus the texture
 * offsets GLOBAL +0xc96/+0xc9e, as 16.16, to 0x206f921f/0x206f9223 (and
 * the first's to 0x206f922f/0x206f9233 for the close); the table point's
 * x, y and z a word at a time. */
static void textured_uv(int first)
{
    SET_BX(M16(esi + 2));
    SET_BP(M16(esi + 4));
    edi = G;
    SET_BX(BX + M16(edi + 0xc96));
    SET_BP(BP + M16(edi + 0xc9e));
    M16(V(0x206f921f)) = 0;
    M16(V(0x206f9221)) = BX;
    M16(V(0x206f9223)) = 0;
    M16(V(0x206f9225)) = BP;
    if (first) {
        M16(V(0x206f922f)) = 0;
        M16(V(0x206f9231)) = BX;
        M16(V(0x206f9233)) = 0;
        M16(V(0x206f9235)) = BP;
    }
}

/* 0x206f0a01 -- the first vertex; the index times 16 after zero-extending. */
static void textured_vertex_first(void)
{
    uint32_t keep_esi = esi, keep_ecx = ecx, i;

    textured_uv(1);
    lods16();
    eax = AX;
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    for (i = 0; i < 6; i++) {
        lods16();
        M16(V(0x206f90c0) + 2 * i) = AX;
        M16(V(0x206f90e4) + 2 * i) = AX;
    }
    if (M16(edi + 0xdca) != 0)
        polygon_first();
    else
        textured_first();
    esi = keep_esi, ecx = keep_ecx;
}

/* 0x206f0ade -- a next vertex; the index times 16 in a word (shl ax,4),
 * so an index from 0x1000 wraps where the first's does not. */
static void textured_vertex_next(void)
{
    uint32_t keep_esi = esi, keep_ecx = ecx, i;

    textured_uv(0);
    lods16();
    SET_AX(AX << 4);
    eax = AX;
    eax += M32(V(0x206f912a));
    esi = eax;
    for (i = 0; i < 6; i++) {
        lods16();
        M16(V(0x206f90c0) + 2 * i) = AX;
    }
    if (M16(edi + 0xdca) != 0)
        polygon_next();
    else
        textured_next();
    esi = keep_esi, ecx = keep_ecx;
}

/* 0x206f0b78 -- the close: the flat one in the map. */
static void textured_end(void)
{
    edi = G;
    if (M16(edi + 0xdca) != 0) {
        M16(edi + 0xc90) = 0;
        M32(edi + 0x3c0) = 0x6800;
        polygon_close();
        return;
    }
    textured_close();
    edi = G;
    M16(edi + 0xc90) = 0;
}

/* The fast triangle and quad, 0x206f0d6e and 0x206f1277: a textured
 * shape split recursively for its perspective.  Retail passes the shape on
 * the stack -- a depth word, then five dwords a vertex, (x, y, z, u, v)
 * from the top down -- and the routines read it through their frames.
 * Here a shape is an array of vertices, slot 0 at the lowest address: a
 * caller pushes its vertices in turn, so slot 0 is the last pushed.
 *
 * A shape is dropped when every vertex's outcode shares a bit or both
 * ends of its z are behind; drawn through the textured clipper (slots in
 * order) at depth 5, or when its nearest z over its farthest is above
 * the threshold at 0x206f9178 (0.75 from init); otherwise split at its
 * edges' midpoints, kept in .data at 0x206ff8e4.., into four, all made
 * before the first is drawn and drawn last first. */
struct fast_vertex { uint32_t c[5]; };          /* c[4] x .. c[0] v */

/* The fast routines' outcode: five bits by adc, from x, y and z biased by
 * 2^31 and compared unsigned. */
static uint8_t fast_outcode(uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t b = x, c = y, s = z, d, carry;
    uint8_t al = 0;

    d = 0x80000000u - s;
    b += 0x80000000u;
    c += 0x80000000u;
    s -= 1;
    carry = s >= 0x80000000u;                   /* add esi,0x80000000 */
    s += 0x80000000u;
    al = (uint8_t)(al * 2 + carry);
    al = (uint8_t)(al * 2 + (b < d));
    al = (uint8_t)(al * 2 + (s < b));
    al = (uint8_t)(al * 2 + (c < d));
    al = (uint8_t)(al * 2 + (s < c));
    return al;
}

/* (a + b) >> 1 as cdq / add / adc / shrd 1 do it, 64-bit. */
static uint32_t midpoint(uint32_t a, uint32_t b)
{
    int64_t s = (int64_t)(int32_t)a + (int32_t)b;

    return (uint32_t)((uint64_t)s >> 1);
}

static struct fast_vertex mid_vertex(const struct fast_vertex *a, const struct fast_vertex *b,
                                     uint32_t at)
{
    struct fast_vertex m;
    uint32_t i;

    for (i = 0; i < 5; i++) {
        m.c[i] = midpoint(a->c[i], b->c[i]);
        M32(V(at) + 4 * i) = m.c[i];
    }
    return m;
}

/* The draw (0x206f1189 / 0x206f1857): GLOBAL +0xc90 as textured_start()
 * sets it, the polygon counted, and the slots in order through the
 * textured clipper. */
static void fast_draw(const struct fast_vertex *v, uint32_t n)
{
    uint32_t i;

    edi = G;
    SET_AX(M16(edi + 0xcb6));
    SET_AL((uint8_t)((AL >> 1) | (AL << 7)));
    SET_AX(AX | M16(edi + 0xcb6));
    M16(edi + 0xc90) = AX;
    polygon_count();
    for (i = 0; i < n; i++) {
        edx = v[i].c[4];
        ecx = v[i].c[3];
        eax = v[i].c[2];
        esi = v[i].c[1];
        edi = v[i].c[0];
        M32(V(0x206f90c0)) = edx;
        M32(V(0x206f90c4)) = ecx;
        M32(V(0x206f90c8)) = eax;
        M32(V(0x206f921f)) = esi;
        M32(V(0x206f9223)) = edi;
        if (i == 0) {
            M32(V(0x206f90e4)) = edx;
            M32(V(0x206f90e8)) = ecx;
            M32(V(0x206f90ec)) = eax;
            M32(V(0x206f922f)) = esi;
            M32(V(0x206f9233)) = edi;
            textured_first();
        } else {
            textured_next();
        }
    }
    textured_close();
    edi = G;
    M16(edi + 0xc90) = 0;
}

/* 1 when the shape is to be drawn as it is, -1 when dropped, 0 when split. */
static int fast_test(uint16_t depth, const struct fast_vertex *v, uint32_t n)
{
    uint32_t lo, hi, i;

    M16(V(0x206ff948)) = depth;
    if (M16(V(0x206ff948)) > 4)
        return 1;
    M8(V(0x206ff8e0)) = 0xff;
    for (i = 0; i < n; i++)
        M8(V(0x206ff8e0)) &= fast_outcode(v[i].c[4], v[i].c[3], v[i].c[2]);
    if (M8(V(0x206ff8e0)) != 0)
        return -1;
    lo = hi = v[0].c[2];
    for (i = 1; i < n; i++) {
        if ((int32_t)v[i].c[2] < (int32_t)lo)
            lo = v[i].c[2];
        if ((int32_t)v[i].c[2] > (int32_t)hi)
            hi = v[i].c[2];
    }
    if ((int32_t)(hi & lo) < 0)
        return -1;
    if ((int32_t)(hi ^ lo) < 0)
        return 0;
    if (lo == hi)
        return 1;
    if ((uint32_t)(((uint64_t)lo << 32) / hi) > M32(V(0x206f9178)))
        return 1;
    return 0;
}

/* 0x206f0d6e */
static void fast_triangle(uint16_t depth, const struct fast_vertex *v)
{
    struct fast_vertex m02, m01, m12, t[4][3];
    int k = fast_test(depth, v, 3), i;

    if (k < 0)
        return;
    if (k > 0) {
        fast_draw(v, 3);
        return;
    }
    m02 = mid_vertex(&v[0], &v[2], 0x206ff8e4);
    m01 = mid_vertex(&v[0], &v[1], 0x206ff8f8);
    m12 = mid_vertex(&v[1], &v[2], 0x206ff90c);
    /* pushed (v0, m02, m01), (m02, m01, m12), (v2, m02, m12), (v1, m01, m12) */
    t[0][0] = m01, t[0][1] = m02, t[0][2] = v[0];
    t[1][0] = m12, t[1][1] = m01, t[1][2] = m02;
    t[2][0] = m12, t[2][1] = m02, t[2][2] = v[2];
    t[3][0] = m12, t[3][1] = m01, t[3][2] = v[1];
    depth = (uint16_t)(M16(V(0x206ff948)) + 1);
    for (i = 3; i >= 0; i--)
        fast_triangle(depth, t[i]);
}

/* 0x206f1277 */
static void fast_quad(uint16_t depth, const struct fast_vertex *v)
{
    struct fast_vertex m01, m12, m23, m30, c, q[4][4];
    int k = fast_test(depth, v, 4), i;

    if (k < 0)
        return;
    if (k > 0) {
        fast_draw(v, 4);
        return;
    }
    m01 = mid_vertex(&v[0], &v[1], 0x206ff8e4);
    m12 = mid_vertex(&v[2], &v[1], 0x206ff8f8);
    m23 = mid_vertex(&v[3], &v[2], 0x206ff90c);
    m30 = mid_vertex(&v[3], &v[0], 0x206ff920);
    c = mid_vertex(&m30, &m12, 0x206ff934);
    /* pushed (v0, m01, c, m30), (m01, v1, m12, c), (c, m12, v2, m23),
     * (m30, c, m23, v3) */
    q[0][0] = m30, q[0][1] = c, q[0][2] = m01, q[0][3] = v[0];
    q[1][0] = c, q[1][1] = m12, q[1][2] = v[1], q[1][3] = m01;
    q[2][0] = m23, q[2][1] = v[2], q[2][2] = m12, q[2][3] = c;
    q[3][0] = v[3], q[3][1] = m23, q[3][2] = c, q[3][3] = m30;
    depth = (uint16_t)(M16(V(0x206ff948)) + 1);
    for (i = 3; i >= 0; i--)
        fast_quad(depth, q[i]);
}

/* 0x206f0970 / 0x206f091a (and 0x206f0cec / 0x206f0c96): a facet's
 * vertices pushed for the fast routine, each (x, y, z) from its table
 * point and (u, v) as for the general path; the first pushed is the last
 * slot.
 *
 * Retail's 0x60 triangle loop (0x206f0cec) pushes edi once a vertex and
 * never pops it, so the triangle reads a skewed frame, and the 12 bytes
 * left on the stack make its handler return to the first vertex's y as a
 * code address: retail crashes there, every time.  Here that path does
 * what the other three do (0x206f0970, 0x206f091a, 0x206f0c96), a
 * deliberate fix of a crash; docs/modules/g3d.md has the details, and the
 * harness keeps clear of it. */
static void textured_fast_shape(uint32_t n)
{
    struct fast_vertex v[4];
    uint32_t i;

    TRACE(n == 3 ? 0x206f0970 : 0x206f091a);
    for (i = 0; i < n; i++) {
        uint32_t slot = n - 1 - i;

        eax = M16(esi);
        eax <<= 4;
        eax += M32(V(0x206f912a));
        edi = eax;
        eax = M32(edi);
        ebx = M32(edi + 4);
        edx = M32(edi + 8);
        v[slot].c[4] = eax;
        v[slot].c[3] = ebx;
        v[slot].c[2] = edx;
        SET_BX(M16(esi + 2));
        SET_BP(M16(esi + 4));
        SET_BX(BX + M16(G + 0xc96));
        SET_BP(BP + M16(G + 0xc9e));
        ebx <<= 16;
        ebp <<= 16;
        v[slot].c[1] = ebx;
        v[slot].c[0] = ebp;
        esi += 6;
    }
    if (n == 3)
        fast_triangle(0, v);
    else
        fast_quad(0, v);
}

/* The general path's vertices: dec ecx then loop again, so a facet of one
 * vertex walks four billion. */
static void textured_vertices(void)
{
    TRACE(0x206f08bf);
    textured_start();
    textured_vertex_first();
    esi += 6;
    ecx--;
    do {
        textured_vertex_next();
        esi += 6;
    } while (--ecx != 0);
    textured_end();
}

/* The fast path's test (0x206f0886 / 0x206f0c02): with 0x206f90f4 set
 * outside the map GLOBAL +0x12a2 goes to 1, and with +0x12a4 bit 3 clear a
 * triangle or quad takes the fast routine. */
static int textured_fast(uint32_t count)
{
    TRACE(0x206f0886);
    edi = G;
    if (M16(V(0x206f90f4)) == 0 || M16(edi + 0xdca) != 0)
        return 0;
    M16(edi + 0x12a2) = 1;
    if (M32(edi + 0x12a4) & 8)
        return 0;
    if (count == 3 || count == 4) {
        textured_fast_shape(count);
        return 1;
    }
    return 0;
}

static void textured_off(void)
{
    edi = G;
    M16(edi + 0xc90) = 0;
    M16(edi + 0x12a2) = 0;
    M8(V(0x206f9125)) = 0;
    M16(V(0x206f90f4)) = 0;
}

/* 0x206f0850 -- 0x20, a textured facet (plane, then index, u, v per
 * vertex). */
static void op_textured_facet(void)
{
    uint32_t count;

    edi = G;
    esi += 2;
    lods16();
    eax = AX;
    count = eax;
    plane_facing();
    if (!sf) {
        edi = G;
        if (M16(edi + 0xcb6) == 0) {
            shade();
            shade_colour();
        }
        esi += 0xa;
        ecx = count;
        if (!textured_fast(ecx))
            textured_vertices();
    }
    {
        uint32_t p = (uint32_t)(uint16_t)count * 6;   /* 0x206f08dc */

        SET_BX(6);
        SET_AX(p);
        SET_DX(p >> 16);
        SET_AX(AX + 0xe);
        eax = AX;
        M32(V(0x206f9126)) += eax;
    }
    textured_off();
}

/* 0x206f0bb7 -- 0x60, a textured face (point and normal). */
static void op_textured_face(void)
{
    uint32_t count;

    edi = G;
    esi += 2;
    lods16();
    eax = AX;
    count = eax;
    face_facing();
    if (sf) {
        edi = G;
        if (M16(edi + 0xcb6) == 0 && M16(edi + 0xdca) == 0) {
            esi += 6;
            shade();
            shade_colour();
            esi -= 6;
        }
        esi += 0xc;
        ecx = count;
        if (!textured_fast(ecx))
            textured_vertices();
    }
    {
        uint32_t p = (uint32_t)(uint16_t)count * 6;   /* 0x206f0c58 */

        SET_BX(6);
        SET_AX(p);
        SET_DX(p >> 16);
        SET_AX(AX + 0x10);
        eax = AX;
        M32(V(0x206f9126)) += eax;
    }
    textured_off();
}


/* ---- the shaded textured clipper: .text+0xd2b0..+0xdfd7 ----------------------------- */

/* The textured clipper once more, each vertex carrying a shade in edi as
 * well (16.16, at +0x14 of each slot; 0x206f9213 towards 0x206f9217, the
 * first's at 0x206f921b).  Survivors go to HG2D +0x50 with GLOBAL +0xc94
 * and +0xc9c added to u and v as dwords; the close draws through +0x54. */

#define HG2D_SHADED_TEXTURED_VERTEX(rec, x, y, w, u, v, shade) \
    (eax = import_call(M32(HG2D + 0x50), (const uint32_t[]){ rec, x, y, w, u, v, shade }, 7, HG2D))
#define HG2D_SHADED_TEXTURED_POLYGON(s) (eax = IMP1(HG2D, 0x54, s))

#define LOAD_ST(at)                                                           \
    (LOAD_TEXTURED(at), edi = M32(V((at) + 0x14)))
#define STORE_ST(at)                                                          \
    (STORE_TEXTURED(at), M32(V((at) + 0x14)) = edi)
#define LOAD_ST_CROSSING                                                      \
    (LOAD_TEXTURED_CROSSING, edi = M32(V(0x206f9213)))

/* 0x206ee2b0 */
static void crossing_st(void)
{
    int64_t p;

    eax = (edx & 1) << 31;                      /* sar edx,1 / rcr eax,1 */
    edx = sar32(edx, 1);
    idiv32(ebx);
    ebp = eax;
    eax = M32(V(0x206f9227)) - M32(V(0x206f921f));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    M32(V(0x206f921f)) += edx;
    eax = M32(V(0x206f922b)) - M32(V(0x206f9223));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    M32(V(0x206f9223)) += edx;
    eax = M32(V(0x206f9217)) - M32(V(0x206f9213));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    M32(V(0x206f9213)) += edx;
    eax = ebp;
    ecx -= esi;
    p = (int64_t)(int32_t)eax * (int32_t)ecx;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    esi += edx;
    ecx = eax;
    eax = M32(V(0x206f90d4)) - M32(V(0x206f90c8));
    p = (int64_t)(int32_t)eax * (int32_t)ebp;
    p = (int64_t)((uint64_t)p << 1);
    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    edx += M32(V(0x206f90c8));
}

/* 0x206ee325, 0x206ee36b, 0x206ee3af, 0x206ee3f5 */
#define ONTO_ST(name, a, b, adj, op, e, f, neg, store_e)                      \
static void name(void)                                                        \
{                                                                             \
    ebx = M32(V(a));                                                          \
    edx = M32(V(0x206f90c8));                                                 \
    ebx += edx;                                                               \
    ebx -= M32(V(b));                                                         \
    ebx -= M32(V(0x206f90d4));                                                \
    edx op M32(V(adj));                                                       \
    ecx = M32(V(e));                                                          \
    esi = M32(V(f));                                                          \
    crossing_st();                                                            \
    M32(V(0x206f90c8)) = edx;                                                 \
    M32(V(f)) = esi;                                                          \
    if (neg)                                                                  \
        edx = (uint32_t)-(int32_t)edx;                                        \
    M32(V(store_e)) = edx;                                                    \
}

ONTO_ST(onto_y_low_st, 0x206f90c4, 0x206f90d0, 0x206f90c4, +=, 0x206f90cc, 0x206f90c0, 1, 0x206f90c4)
ONTO_ST(onto_y_high_st, 0x206f90d0, 0x206f90c4, 0x206f90c4, -=, 0x206f90cc, 0x206f90c0, 0, 0x206f90c4)
ONTO_ST(onto_x_low_st, 0x206f90c0, 0x206f90cc, 0x206f90c0, +=, 0x206f90d0, 0x206f90c4, 1, 0x206f90c0)
ONTO_ST(onto_x_high_st, 0x206f90cc, 0x206f90c0, 0x206f90c0, -=, 0x206f90d0, 0x206f90c4, 0, 0x206f90c0)

static void stage1_st(void);
static void stage2_st(void);
static void stage3_st(void);
static void emit_st(void);

/* 0x206eeb8f -- a vertex to HG2D +0x50, u and v moved by GLOBAL +0xc94 and
 * +0xc9c (left so in ebx and ebp), esi the window record. */
static void emit_raw_st(void)
{
    esi = G;
    ebx += M32(esi + 0xc94);
    ebp += M32(esi + 0xc9c);
    esi = M32(esi + 0x1194);
    HG2D_SHADED_TEXTURED_VERTEX(esi, edx, ecx, eax, ebx, ebp, edi);
}

static void first2_st(void)                     /* 0x206ee590 */
{
    STORE_ST(0x206f923a);
    STORE_ST(0x206f9252);
    M8(V(0x206f92e2)) = 0;
    if ((int32_t)ecx < (int32_t)esi) {
        M8(V(0x206f92e2)) = 1;
        return;
    }
    emit_st();
}

static void first1_st(void)                     /* 0x206ee52a */
{
    STORE_ST(0x206f92b2);
    STORE_ST(0x206f92ca);
    M8(V(0x206f92e5)) = 0;
    if ((int32_t)ecx > (int32_t)eax) {
        M8(V(0x206f92e5)) = 1;
        return;
    }
    M8(V(0x206f9239)) = 0;
    first2_st();
}

static void first0_st(void)                     /* 0x206ee4c4 */
{
    STORE_ST(0x206f926a);
    STORE_ST(0x206f9282);
    M8(V(0x206f92e3)) = 0;
    if ((int32_t)edx < (int32_t)esi) {
        M8(V(0x206f92e3)) = 1;
        return;
    }
    M8(V(0x206f9238)) = 0;
    first1_st();
}

/* 0x206ee439 -- the polygon's first vertex (0x206f90c0, its u, v and shade
 * at 0x206f921f, 0x206f9223, 0x206f9213). */
static void st_first(void)
{
    SET_AL(1);
    M8(V(0x206f9237)) = AL;
    M8(V(0x206f9238)) = AL;
    M8(V(0x206f9239)) = AL;
    LOAD_VERTEX(0x206f90c0);
    ebx = M32(V(0x206f921f));
    ebp = M32(V(0x206f9223));
    edi = M32(V(0x206f9213));
    if (M16(V(0x206f90f6)) == 2) {
        emit_raw_st();
        return;
    }
    STORE_ST(0x206f929a);
    M8(V(0x206f92e4)) = 0;
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        return;
    }
    M8(V(0x206f9237)) = 0;
    first0_st();
}

/* 0x206eebd1, 0x206eec44, 0x206eecb7, 0x206eed2a */
static void cross_with_st(uint32_t prev, void (*onto)(void))
{
    uint32_t d = edx, c = ecx, a = eax, s = esi, b = ebx, p = ebp, i = edi;

    STORE_VERTEX(0x206f90c0);
    M32(V(0x206f921f)) = ebx;
    M32(V(0x206f9223)) = ebp;
    M32(V(0x206f9213)) = edi;
    M32(V(0x206f90cc)) = M32(V(prev));
    M32(V(0x206f90d0)) = M32(V(prev + 4));
    M32(V(0x206f90d4)) = M32(V(prev + 8));
    M32(V(0x206f9227)) = M32(V(prev + 0xc));
    M32(V(0x206f922b)) = M32(V(prev + 0x10));
    M32(V(0x206f9217)) = M32(V(prev + 0x14));
    onto();
    edi = i, ebp = p, ebx = b, esi = s, eax = a, ecx = c, edx = d;
}

#define STAGE_ST(name, flag_first, first, flag_out, prev, out_test, onto, next) \
static void name(void)                                                        \
{                                                                             \
    if (first_bit(V(flag_first))) {                                           \
        first();                                                              \
        return;                                                               \
    }                                                                         \
    if (M8(V(flag_out)) == 1) {                                               \
        if (out_test) {                                                       \
            STORE_ST(prev);                                                   \
            return;                                                           \
        }                                                                     \
        M8(V(flag_out)) = 0;                                                  \
        cross_with_st(prev, onto);                                            \
        STORE_ST(prev);                                                       \
        LOAD_ST_CROSSING;                                                     \
        next();                                                               \
        LOAD_ST(prev);                                                        \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    if (out_test) {                                                           \
        M8(V(flag_out)) = 1;                                                  \
        cross_with_st(prev, onto);                                            \
        STORE_ST(prev);                                                       \
        LOAD_ST_CROSSING;                                                     \
        next();                                                               \
        return;                                                               \
    }                                                                         \
    STORE_ST(prev);                                                           \
    next();                                                                   \
}

/* 0x206ee761, 0x206ee8aa, 0x206ee9f3 */
STAGE_ST(stage1_st, 0x206f9237, first0_st, 0x206f92e3, 0x206f9282,
         (int32_t)edx < (int32_t)esi, onto_x_low_st, stage2_st)
STAGE_ST(stage2_st, 0x206f9238, first1_st, 0x206f92e5, 0x206f92ca,
         (int32_t)ecx > (int32_t)eax, onto_y_high_st, stage3_st)
STAGE_ST(stage3_st, 0x206f9239, first2_st, 0x206f92e2, 0x206f9252,
         (int32_t)ecx < (int32_t)esi, onto_y_low_st, emit_st)

/* 0x206ee5ef -- the polygon's next vertex. */
static void st_next(void)
{
    LOAD_VERTEX(0x206f90c0);
    ebx = M32(V(0x206f921f));
    ebp = M32(V(0x206f9223));
    edi = M32(V(0x206f9213));
    if (M16(V(0x206f90f6)) == 2) {
        emit_raw_st();
        return;
    }
    if (M8(V(0x206f92e4)) == 1) {
        if ((int32_t)edx > (int32_t)eax) {
            STORE_ST(0x206f929a);
            return;
        }
        M8(V(0x206f92e4)) = 0;
        cross_with_st(0x206f929a, onto_x_high_st);
        STORE_ST(0x206f929a);
        LOAD_ST_CROSSING;
        stage1_st();
        LOAD_ST(0x206f929a);
        stage1_st();
        return;
    }
    if ((int32_t)edx > (int32_t)eax) {
        M8(V(0x206f92e4)) = 1;
        cross_with_st(0x206f929a, onto_x_high_st);
        STORE_ST(0x206f929a);
        LOAD_ST_CROSSING;
        stage1_st();
        return;
    }
    STORE_ST(0x206f929a);
    stage1_st();
}

/* 0x206eeb42 -- a survivor clamped as emit() does, u kept through it. */
static void emit_st(void)
{
    esi = ebx;
    if (eax == 0) {
        eax = 0x10000;                          /* 0x206eeb68 */
        edx = 0;
        ecx = 0;
        emit_raw_st();
        return;
    }
    ebx = edx;
    if ((int32_t)ebx < 0)
        ebx = (uint32_t)-(int32_t)ebx;
    if ((int32_t)ebx > (int32_t)eax) {
        uint32_t neg = (int32_t)edx < 0;

        edx = eax;
        if (neg)
            edx = (uint32_t)-(int32_t)edx;
    }
    ebx = ecx;
    if ((int32_t)ebx < 0)
        ebx = (uint32_t)-(int32_t)ebx;
    if ((int32_t)ebx > (int32_t)eax) {
        uint32_t neg = (int32_t)ecx < 0;

        ecx = eax;
        if (neg)
            ecx = (uint32_t)-(int32_t)ecx;
    }
    ebx = esi;
    emit_raw_st();
}

/* 0x206eeec7, 0x206eef0b, 0x206eef4f, 0x206eef93 */
#define CLOSE_ST(name, flag_out, prev, out_test, onto, next)                  \
static void name(void)                                                        \
{                                                                             \
    if (M8(V(flag_out)) == 1 ? (out_test) : !(out_test))                      \
        return;                                                               \
    cross_with_st(prev, onto);                                                \
    LOAD_ST_CROSSING;                                                         \
    next();                                                                   \
}

CLOSE_ST(close0_st, 0x206f92e4, 0x206f929a, (int32_t)edx > (int32_t)eax, onto_x_high_st, stage1_st)
CLOSE_ST(close1_st, 0x206f92e3, 0x206f9282, (int32_t)edx < (int32_t)esi, onto_x_low_st, stage2_st)
CLOSE_ST(close2_st, 0x206f92e5, 0x206f92ca, (int32_t)ecx > (int32_t)eax, onto_y_high_st, stage3_st)
CLOSE_ST(close3_st, 0x206f92e2, 0x206f9252, (int32_t)ecx < (int32_t)esi, onto_y_low_st, emit_st)

/* 0x206eed9d -- close the polygon (its first vertex's u, v and shade at
 * 0x206f922f, 0x206f9233, 0x206f921b) and draw it unless GLOBAL +0x2f0
 * counts past zero. */
static void st_close(void)
{
    if (M16(V(0x206f90f6)) != 2) {
        LOAD_VERTEX(0x206f90e4);
        ebx = M32(V(0x206f922f));
        ebp = M32(V(0x206f9233));
        edi = M32(V(0x206f921b));
        close0_st();
        if (first_bit(V(0x206f9237)))
            goto done;
        LOAD_ST(0x206f926a);
        close1_st();
        if (first_bit(V(0x206f9238)))
            goto done;
        LOAD_ST(0x206f92b2);
        close2_st();
        if (first_bit(V(0x206f9239)))
            goto done;
        LOAD_ST(0x206f923a);
        close3_st();
    }
    edi = G;                                    /* 0x206eee7b */
    M32(edi + 0x2f0) -= 1;
    if ((int32_t)M32(edi + 0x2f0) < 0)
        goto done;
    edi = M32(edi + 0x1194);
    edi += 0x80;
    HG2D_SHADED_TEXTURED_POLYGON(edi);
done:
    eax = G;
    M16(eax + 0x2f4) = 0;
}

/* ---- shaded textured facets: .text+0xb310..+0xbf0c ---------------------------------- */

/* A vertex (index, u, v at esi) with its table point's shade, the high
 * half of the point's +0xc, to 0x206f9213.  Unlike 0x20's, u and v are
 * kept as they are, 16.16 (the emit adds GLOBAL +0xc94/+0xc9c); in the map
 * the flat clipper draws it. */
static void st_uv(int first)
{
    SET_BX(M16(esi + 2));
    SET_BP(M16(esi + 4));
    M16(V(0x206f921f)) = 0;
    M16(V(0x206f9221)) = BX;
    M16(V(0x206f9223)) = 0;
    M16(V(0x206f9225)) = BP;
    if (first) {
        M16(V(0x206f922f)) = 0;
        M16(V(0x206f9231)) = BX;
        M16(V(0x206f9233)) = 0;
        M16(V(0x206f9235)) = BP;
    }
}

/* 0x206ec7a8 -- the first vertex; the index times 16 after zero-extending. */
static void st_vertex_first(void)
{
    uint32_t keep_esi = esi, keep_ecx = ecx, i;

    st_uv(1);
    lods16();
    eax = AX;
    eax <<= 4;
    eax += M32(V(0x206f912a));
    esi = eax;
    for (i = 0; i < 6; i++) {
        lods16();
        M16(V(0x206f90c0) + 2 * i) = AX;
        M16(V(0x206f90e4) + 2 * i) = AX;
    }
    lods32();
    SET_AX(0);
    M32(V(0x206f9213)) = eax;
    M32(V(0x206f921b)) = eax;
    edi = G;
    if (M16(edi + 0xdca) != 0)
        polygon_first();
    else
        st_first();
    esi = keep_esi, ecx = keep_ecx;
}

/* 0x206ec885 -- a next vertex; the index times 16 in a word (shl ax,4), as
 * 0x20's. */
static void st_vertex_next(void)
{
    uint32_t keep_esi = esi, keep_ecx = ecx, i;

    st_uv(0);
    lods16();
    SET_AX(AX << 4);
    eax = AX;
    eax += M32(V(0x206f912a));
    esi = eax;
    for (i = 0; i < 6; i++) {
        lods16();
        M16(V(0x206f90c0) + 2 * i) = AX;
    }
    lods32();
    SET_AX(0);
    M32(V(0x206f9213)) = eax;
    edi = G;
    if (M16(edi + 0xdca) != 0)
        polygon_next();
    else
        st_next();
    esi = keep_esi, ecx = keep_ecx;
}

/* 0x206ec91a -- the close: the flat one in the map. */
static void st_end(void)
{
    edi = G;
    if (M16(edi + 0xdca) != 0) {
        M16(edi + 0xc90) = 0;
        M32(edi + 0x3c0) = 0x6800;
        polygon_close();
        return;
    }
    st_close();
    edi = G;
    M16(edi + 0xc90) = 0;
}

/* The fast triangle and quad, 0x206ec959 and 0x206ecf0d, as 0x20's
 * (0x206f0d6e, 0x206f1277) with six dwords a vertex, (x, y, z, u, v,
 * shade) from the top down, their own depth and outcode (0x206ff858,
 * 0x206ff85a) and midpoints (0x206ff7e0..), and the draw through the
 * shaded textured clipper.  The draw reads GLOBAL +0xcb6 through the edi it
 * came in with, before loading GLOBAL's address: G from both handlers, and
 * G after any draw. */
struct st_vertex { uint32_t c[6]; };            /* c[5] x .. c[0] shade */

static struct st_vertex st_mid(const struct st_vertex *a, const struct st_vertex *b, uint32_t at)
{
    struct st_vertex m;
    uint32_t i;

    for (i = 0; i < 6; i++) {
        m.c[i] = midpoint(a->c[i], b->c[i]);
        M32(V(at) + 4 * i) = m.c[i];
    }
    return m;
}

/* 0x206ecdfe / 0x206ed5c2 */
static void st_fast_draw(const struct st_vertex *v, uint32_t n)
{
    uint32_t i;

    SET_AX(M16(edi + 0xcb6));
    SET_AL((uint8_t)((AL >> 1) | (AL << 7)));
    SET_AX(AX | M16(edi + 0xcb6));
    edi = G;
    M16(edi + 0xc90) = AX;
    polygon_count();
    for (i = 0; i < n; i++) {
        edx = v[i].c[5];
        ecx = v[i].c[4];
        eax = v[i].c[3];
        esi = v[i].c[2];
        edi = v[i].c[1];
        ebx = v[i].c[0];
        M32(V(0x206f90c0)) = edx;
        M32(V(0x206f90c4)) = ecx;
        M32(V(0x206f90c8)) = eax;
        M32(V(0x206f921f)) = esi;
        M32(V(0x206f9223)) = edi;
        M32(V(0x206f9213)) = ebx;
        if (i == 0) {
            M32(V(0x206f90e4)) = edx;
            M32(V(0x206f90e8)) = ecx;
            M32(V(0x206f90ec)) = eax;
            M32(V(0x206f922f)) = esi;
            M32(V(0x206f9233)) = edi;
            M32(V(0x206f921b)) = ebx;
            st_first();
        } else {
            st_next();
        }
    }
    st_close();
    edi = G;
    M16(edi + 0xc90) = 0;
}

/* 1 when the shape is to be drawn as it is, -1 when dropped, 0 when split. */
static int st_fast_test(uint16_t depth, const struct st_vertex *v, uint32_t n)
{
    uint32_t lo, hi, i;

    M16(V(0x206ff858)) = depth;
    if (M16(V(0x206ff858)) > 4)
        return 1;
    M8(V(0x206ff85a)) = 0xff;
    for (i = 0; i < n; i++)
        M8(V(0x206ff85a)) &= fast_outcode(v[i].c[5], v[i].c[4], v[i].c[3]);
    if (M8(V(0x206ff85a)) != 0)
        return -1;
    lo = hi = v[0].c[3];
    for (i = 1; i < n; i++) {
        if ((int32_t)v[i].c[3] < (int32_t)lo)
            lo = v[i].c[3];
        if ((int32_t)v[i].c[3] > (int32_t)hi)
            hi = v[i].c[3];
    }
    if ((int32_t)(hi & lo) < 0)
        return -1;
    if ((int32_t)(hi ^ lo) < 0)
        return 0;
    if (lo == hi)
        return 1;
    if ((uint32_t)(((uint64_t)lo << 32) / hi) > M32(V(0x206f9178)))
        return 1;
    return 0;
}

/* 0x206ec959 */
static void st_fast_triangle(uint16_t depth, const struct st_vertex *v)
{
    struct st_vertex m02, m01, m12, t[4][3];
    int k = st_fast_test(depth, v, 3), i;

    if (k < 0)
        return;
    if (k > 0) {
        st_fast_draw(v, 3);
        return;
    }
    m02 = st_mid(&v[0], &v[2], 0x206ff7e0);
    m01 = st_mid(&v[0], &v[1], 0x206ff7f8);
    m12 = st_mid(&v[1], &v[2], 0x206ff810);
    /* pushed (v0, m02, m01), (m02, m01, m12), (v2, m02, m12), (v1, m01, m12) */
    t[0][0] = m01, t[0][1] = m02, t[0][2] = v[0];
    t[1][0] = m12, t[1][1] = m01, t[1][2] = m02;
    t[2][0] = m12, t[2][1] = m02, t[2][2] = v[2];
    t[3][0] = m12, t[3][1] = m01, t[3][2] = v[1];
    depth = (uint16_t)(M16(V(0x206ff858)) + 1);
    for (i = 3; i >= 0; i--)
        st_fast_triangle(depth, t[i]);
}

/* 0x206ecf0d */
static void st_fast_quad(uint16_t depth, const struct st_vertex *v)
{
    struct st_vertex m01, m12, m23, m30, c, q[4][4];
    int k = st_fast_test(depth, v, 4), i;

    if (k < 0)
        return;
    if (k > 0) {
        st_fast_draw(v, 4);
        return;
    }
    m01 = st_mid(&v[0], &v[1], 0x206ff7e0);
    m12 = st_mid(&v[2], &v[1], 0x206ff7f8);
    m23 = st_mid(&v[3], &v[2], 0x206ff810);
    m30 = st_mid(&v[3], &v[0], 0x206ff828);
    c = st_mid(&m30, &m12, 0x206ff840);
    /* pushed (v0, m01, c, m30), (m01, v1, m12, c), (c, m12, v2, m23),
     * (m30, c, m23, v3) */
    q[0][0] = m30, q[0][1] = c, q[0][2] = m01, q[0][3] = v[0];
    q[1][0] = c, q[1][1] = m12, q[1][2] = v[1], q[1][3] = m01;
    q[2][0] = m23, q[2][1] = v[2], q[2][2] = m12, q[2][3] = c;
    q[3][0] = v[3], q[3][1] = m23, q[3][2] = c, q[3][3] = m30;
    depth = (uint16_t)(M16(V(0x206ff858)) + 1);
    for (i = 3; i >= 0; i--)
        st_fast_quad(depth, q[i]);
}

/* 0x206ec3e1 / 0x206ec47c (and 0x206ec60b / 0x206ec6a6): the vertices
 * pushed for the fast routine, each (x, y, z) from its table point, u and
 * v as words to 16.16, and the shade from the point's +0xc; the first
 * pushed is the last slot.
 *
 * Retail's 0x7b quad (0x206ec75e) pushes its depth as a dword (push 0)
 * where the other three push a word, and frees 0x62 bytes after: the quad
 * reads every vertex two bytes off, and the two bytes left on the stack
 * make its handler pop the count from the wrong place and return into the
 * middle of an address: retail crashes there, every time.  Here it pushes
 * a word as the rest do, a deliberate fix of a crash; docs/modules/g3d.md
 * has the details, and the harness keeps clear of it. */
static void st_fast_shape(uint32_t n)
{
    struct st_vertex v[4];
    uint32_t i;

    for (i = 0; i < n; i++) {
        uint32_t slot = n - 1 - i;

        ebx = M16(esi);
        ebx <<= 4;
        ebx += M32(V(0x206f912a));
        v[slot].c[5] = M32(ebx);
        v[slot].c[4] = M32(ebx + 4);
        v[slot].c[3] = M32(ebx + 8);
        SET_AX(M16(esi + 2));
        SET_DX(M16(esi + 4));
        eax <<= 16;
        edx <<= 16;
        v[slot].c[2] = eax;
        v[slot].c[1] = edx;
        eax = M32(ebx + 0xc);
        SET_AX(0);
        v[slot].c[0] = eax;
        esi += 6;
    }
    if (n == 3)
        st_fast_triangle(0, v);
    else
        st_fast_quad(0, v);
}

/* The general path's vertices: dec ecx then loop again, so a facet of one
 * vertex walks four billion. */
static void st_vertices(void)
{
    textured_start();                           /* 0x206ec76d, the same */
    st_vertex_first();
    esi += 6;
    ecx--;
    do {
        st_vertex_next();
        esi += 6;
    } while (--ecx != 0);
    st_end();
}

/* The fast path's test, as 0x20's but through the edi the handler has:
 * 0x7a loads GLOBAL's address for it, 0x7b keeps the one it loaded before
 * the shading (which leaves it so). */
static int st_fast(uint32_t count)
{
    if (M16(V(0x206f90f4)) == 0 || M16(edi + 0xdca) != 0)
        return 0;
    M16(edi + 0x12a2) = 1;
    if (M32(edi + 0x12a4) & 8)
        return 0;
    if (count == 3 || count == 4) {
        st_fast_shape(count);
        return 1;
    }
    return 0;
}

/* 0x206ec310 -- 0x7a, a shaded textured facet (plane, then index, u, v per
 * vertex, the shade from each table point). */
static void op_st_facet(void)
{
    uint32_t count;

    edi = G;
    esi += 2;
    lods16();
    eax = AX;
    count = eax;
    plane_facing();
    if (!sf) {
        edi = G;
        if (M16(edi + 0xcb6) == 0) {
            shade();
            shade_colour();
        }
        esi += 0xa;
        ecx = count;
        edi = G;
        if (!st_fast(ecx))
            st_vertices();
    }
    {
        uint32_t p = (uint32_t)(uint16_t)count * 6;   /* 0x206ec3a4 */

        SET_BX(6);
        SET_AX(p);
        SET_DX(p >> 16);
        SET_AX(AX + 0xe);
        eax = AX;
        M32(V(0x206f9126)) += eax;
    }
    textured_off();
}

/* 0x206ec545 -- 0x7b, a shaded textured face (point and normal). */
static void op_st_face(void)
{
    uint32_t count;

    esi += 2;
    lods16();
    eax = AX;
    count = eax;
    face_facing();
    if (sf) {
        esi += 6;
        edi = G;
        if (M16(edi + 0xcb6) == 0) {
            shade();
            shade_colour();
        }
        esi += 6;
        ecx = count;
        if (!st_fast(ecx))
            st_vertices();
    }
    {
        uint32_t p = (uint32_t)(uint16_t)count * 6;   /* 0x206ec5ce */

        SET_BX(6);
        SET_AX(p);
        SET_DX(p >> 16);
        SET_AX(AX + 0x10);
        eax = AX;
        M32(V(0x206f9126)) += eax;
    }
    textured_off();
}


/* 0x206e8a47 -- 0x2d, a line colour as three bytes: GLOBAL +0x3c0 0xe000,
 * the bytes to +0xcca..+0xccc, and the shade for no light. */
static void op_line_rgb(void)
{
    edi = G;
    M32(edi + 0x3c0) = 0xe000;
    SET_AL(M8(esi + 2));
    M8(edi + 0xcca) = AL;
    SET_AL(M8(esi + 3));
    M8(edi + 0xccb) = AL;
    SET_AL(M8(esi + 4));
    M8(edi + 0xccc) = AL;
    SET_AX(0);
    shade_colour();
    M32(V(0x206f9126)) += 6;
}

/* 0x206e8a82 -- 0x2e, the same for GLOBAL +0x2ec, but each byte read into
 * al and ah stored: the handler's own address's second byte (0x8a), three
 * times. */
static void op_surface_rgb(void)
{
    edi = G;
    M32(edi + 0x2ec) = 0xe000;
    SET_AL(M8(esi + 2));
    M8(edi + 0xcc4) = AH;
    SET_AL(M8(esi + 3));
    M8(edi + 0xcc5) = AH;
    SET_AL(M8(esi + 4));
    M8(edi + 0xcc6) = AH;
    M32(V(0x206f9126)) += 6;
}

/* 0x206e8c22 -- 0x84, a normal's shade to GLOBAL +0xcc8 and +0xcc2. */
static void op_shade_normal(void)
{
    esi += 2;
    shade();
    shade_colour();
    edi = G;
    M16(edi + 0xcc8) = DX;
    M16(edi + 0xcc2) = DX;
    M32(V(0x206f9126)) += 8;
}

/* 0x206f0d41 -- 0x68, a normal's shade, inverted, as a texture row: its
 * high byte's top three bits to GLOBAL +0xc96. */
static void op_shade_texture(void)
{
    esi += 2;
    shade();
    shade_colour();
    SET_DX(~DX);
    SET_AL(DH);
    SET_AH(0);
    SET_AX(AX & 0xe0);
    edi = G;
    M16(edi + 0xc96) = AX;
    M32(V(0x206f9126)) += 8;
}


/* 0x206e4129 -- 0x1e, a colour word to GLOBAL +0xca0: its low byte in the
 * high one through HG2D +0x88 and back (0 as it is). */
static void op_colour_word(void)
{
    edi = G;
    esi += 2;
    lods16();
    if (AX != 0) {
        SET_AX(AX << 8);
        HG2D_COLOUR(AX, V(0x206fd6c4));
        eax = M32(V(0x206fd6c4));
        SET_AX(AX >> 8);
    }
    M16(edi + 0xca0) = AX;
    M32(V(0x206f9126)) = esi;
}

/* 0x206e4373 -- 0x56, a level of detail: unless the view block's +0x10 is
 * 1, its +0x14 less two (one from 0), modulo 4, plus 1; kept with the
 * counter past the operand in GLOBAL +0x1169/+0x1164 when not below the
 * one there (signed bytes).  On by the operand either way. */
static void op_detail(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    edx = eax;
    edi = G;
    eax = M32(edi + 0x1198);
    if (M32(eax + 0x10) != 1) {
        eax = M32(eax + 0x14);
        eax--;
        if ((int32_t)eax >= 0)
            eax--;
        eax &= 3;
        eax++;
        if ((int8_t)AL >= (int8_t)M8(edi + 0x1169)) {
            M32(edi + 0x1164) = esi;
            M8(edi + 0x1169) = AL;
        }
    }
    M32(V(0x206f9126)) += edx;
}

/* 0x206f1ee8 -- the view's scale word bx (its one's complement magnitude)
 * times the operand ax, times GLOBAL +0x2b0, times 4, over GLOBAL +0xd4c
 * (a dword idiv, which the host repairs); dx the quotient's high half. */
static void apparent_size(void)
{
    uint32_t p;

    {
        uint16_t t = BX;

        SET_BX(AX);
        SET_AX(t);
    }
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);       /* xchg bx,ax / cwd */
    SET_AX(AX ^ DX);
    p = (uint32_t)AX * BX;
    SET_AX(p);
    SET_DX(p >> 16);
    {
        uint16_t high = DX;

        edi = G;
        p = (uint32_t)AX * M16(edi + 0x2b0);
        SET_AX(p);
        SET_DX(p >> 16);
        SET_BX(AX);
        SET_CX(DX);
        SET_AX(high);
    }
    p = (uint32_t)AX * M16(edi + 0x2b0);
    SET_AX(p);
    SET_DX(p >> 16);
    SET_DX(AX + CX);
    SET_AX(BX);
    p = ((uint32_t)DX << 16 | AX) << 2;
    SET_AX(p);
    SET_DX(p >> 16);
    edx <<= 16;
    SET_DX(AX);
    eax = edx;
    edx = (int32_t)eax < 0 ? 0xffffffffu : 0;
    idiv32(M32(edi + 0xd4c));
    edx = (edx << 16) | (eax >> 16);            /* shld edx,eax,0x10 */
}

/* 0x206f1e73 / 0x206f1eae -- 0x5f and 0x6d, on to the target when the
 * operand at the view's scale (+8, y's, for 0x5f; +0, x's, for 0x6d) is
 * below the threshold word; past the instruction otherwise. */
static void size_jump(uint32_t scale)
{
    ebp = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    ebp += esi;
    SET_AX(M16(esi + 4));
    edi = G;
    ebx = M32(edi + 0x1198);
    SET_BX(M16(ebx + scale));
    apparent_size();
    if (DX == 0 && AX < M16(esi + 6)) {
        M32(V(0x206f9126)) = ebp;
        return;
    }
    esi += 8;
    M32(V(0x206f9126)) = esi;
}

static void op_size_jump_y(void) { size_jump(8); }
static void op_size_jump_x(void) { size_jump(0); }


/* 0x206e5384 -- 0x55: with the view's +0x14 0 and its word +0x56 0, or
 * both 1, 0x206fd724 clear and GLOBAL +0x366 set, the first operand's
 * bytes to GLOBAL +0x11ec/+0x11ed when the eye's z (GLOBAL +0xce6) is
 * within half the third operand.  x was to be within half the second, but
 * ebx is loaded with the view block's address over it: x is tested
 * against half that address's low word. */
static void op_area_colour(void)
{
    esi += 2;
    lods16();
    SET_CX(AX);
    lods16();
    SET_BX(AX);
    lods16();
    esi += 2;
    M32(V(0x206f9126)) = esi;
    edi = G;
    ebx = M32(edi + 0x1198);
    if (M32(ebx + 0x14) != 0) {
        if (M32(ebx + 0x14) != 1 || M16(ebx + 0x56) != 1)
            return;
    } else if (M16(ebx + 0x56) != 0) {
        return;
    }
    if (M16(V(0x206fd724)) != 0)
        return;
    if (M8(edi + 0x366) == 0)
        return;
    if (M32(ebx + 0x14) != 0 && M32(ebx + 0x14) != 1)
        return;
    SET_AX(AX >> 1);
    SET_BX(BX >> 1);
    if ((int16_t)M16(edi + 0xce6) > (int16_t)AX)
        return;
    SET_AX(-AX);
    if ((int16_t)M16(edi + 0xce6) < (int16_t)AX)
        return;
    if ((int16_t)M16(edi + 0xcde) > (int16_t)BX)
        return;
    SET_BX(-BX);
    if ((int16_t)M16(edi + 0xcde) < (int16_t)BX)
        return;
    M8(edi + 0x11ec) = CL;
    M8(edi + 0x11ed) = CH;
}

#define UTIL_ATAN2(x, y) (eax = IMP2(UTIL, 0x3c, x, y))

/* 0x206e5422 -- 0x70, the eye inside an area: the angle (UTIL +0x3c) from
 * the eye's x and z (GLOBAL +0xcde, +0xce6, the integer words) to each
 * corner, words to 0x206fd7b1.., the first again after the last; on past
 * the corners when every step to the next angle is below a half turn, to
 * the target otherwise.  Both loops are loop on ecx: a count of 0 walks
 * four billion. */
static void op_inside(void)
{
    uint32_t count;

    eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    eax += esi;
    M32(V(0x206fd7ad)) = eax;
    esi += 4;
    lods16();
    ecx = AX;
    count = ecx;                                /* push ecx */
    ebx = 0;
    do {
        uint32_t b = ebx, c = ecx, s = esi;

        SET_AX(M16(esi));
        SET_CX(M16(esi + 2));
        edi = G;
        SET_AX(AX - M16(edi + 0xcde));
        SET_CX(CX - M16(edi + 0xce6));
        eax <<= 16;
        ecx <<= 16;
        UTIL_ATAN2(eax, ecx);
        esi = s, ecx = c, ebx = b;
        M16(V(0x206fd7b1) + ebx) = AX;
        esi += 4;
        ebx += 2;
    } while (--ecx != 0);
    SET_AX(M16(V(0x206fd7b1)));
    M16(V(0x206fd7b1) + ebx) = AX;
    ecx = count;
    ebx = 0;
    do {
        SET_AX(M16(V(0x206fd7b3) + ebx) - M16(V(0x206fd7b1) + ebx));
        if (AX & 0x8000) {                      /* add ax,ax / jb */
            SET_AX(AX + AX);
            goto out;
        }
        SET_AX(AX + AX);
        ebx += 2;
    } while (--ecx != 0);
    {
        uint32_t t = M32(V(0x206fd7ad));

        M32(V(0x206fd7ad)) = esi;
        esi = t;
    }
out:
    eax = M32(V(0x206fd7ad));
    M32(V(0x206f9126)) = eax;
}


/* 0x206e43b3 -- 0x57, a weather effect: the eight bytes after the opcode
 * to WEATHERM +0x28. */
#define WEATHERM_EFFECT(rec) (eax = IMP1(WEATHERM, 0x28, rec))

static void op_weather(void)
{
    uint32_t keep = esi;

    ebx = esi + 2;
    WEATHERM_EFFECT(ebx);
    esi = keep;
    esi += 0xa;
    M32(V(0x206f9126)) = esi;
}

/* 0x206e6cdb -- 0x30, a shade from the sun's height: the operand times
 * GLOBAL +0xb2e doubled, its high word (one's complement when negative)
 * through shade_colour() to GLOBAL +0xcc8 and +0xcc2. */
static void op_sun_shade(void)
{
    uint32_t p;

    edi = G;
    esi += 2;
    lods16();
    SET_BX(M16(edi + 0xb2e));
    imul16(M16(edi + 0xb2e));
    p = ((uint32_t)DX << 16 | AX) << 1;
    SET_AX(p);
    SET_DX(p >> 16);
    if (DX & 0x8000)
        SET_DX(~DX);
    SET_AX(DX);
    shade_colour();
    edi = G;
    M16(edi + 0xcc8) = DX;
    M16(edi + 0xcc2) = DX;
    M32(V(0x206f9126)) += 4;
}

/* 0x206e71d5 -- 0x3f: with GLOBAL +0x11b2 and +0xb34 set, the program at
 * the signed word operand from the opcode, run as in the map (GLOBAL
 * +0xdca one more while it runs, +0x832 kept round it); on past four bytes
 * either way. */
static void op_map_call(void)
{
    edi = G;
    if (M16(edi + 0x11b2) != 0 && M16(edi + 0xb34) != 0) {
        uint32_t keep;
        uint16_t keep_832;

        esi += 2;
        lods16();
        eax = (uint32_t)(int32_t)(int16_t)AX;
        eax += M32(V(0x206f9126));
        keep = M32(V(0x206f9126));
        M32(V(0x206f9126)) = eax;
        keep_832 = M16(edi + 0x832);
        M16(edi + 0xdca) += 1;
        interpret();
        edi = G;
        M16(edi + 0xdca) -= 1;
        M16(edi + 0x832) = keep_832;
        M32(V(0x206f9126)) = keep;
    }
    M32(V(0x206f9126)) += 4;
}

/* 0x206f1f3f -- 0x62, a reject by table points: to the target when the
 * outcodes of all the points listed share a bit (0x207087f0; their union
 * goes to 0x207087f1), past the list otherwise.  The index times 16 in a
 * word; loop on the count, so 0 walks four billion. */
static void op_reject_points(void)
{
    uint32_t target;

    eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    eax += esi;
    target = eax;
    ecx = M16(esi + 4);
    esi += 6;
    M8(V(0x207087f0)) = 0xff;
    M8(V(0x207087f1)) = 0;
    do {
        uint32_t c = ecx, s = esi;

        eax = M16(esi);
        SET_CX(4);
        SET_AX(AX << CL);
        esi = M32(V(0x206f912a));
        esi += eax;
        M32(V(0x206f90c0)) = M32(esi);
        M32(V(0x206f90c4)) = M32(esi + 4);
        M32(V(0x206f90c8)) = M32(esi + 8);
        outcode_a();
        M8(V(0x207087f0)) &= AL;
        M8(V(0x207087f1)) |= AL;
        esi = s, ecx = c;
        esi += 2;
    } while (--ecx != 0);
    eax = target;
    if (M8(V(0x207087f0)) == 0)
        eax = esi;
    M32(V(0x206f9126)) = eax;
}

/* 0x206f4820 -- 0x69 and 0x6b, a light: its first word's magnitude to
 * 0x20710aa4 and sign bit to 0x20710ab2, the rest to 0x20710abe..
 * 0x20710ac2; 0x20710aa8 cleared, and the shade for the sun's height
 * (GLOBAL +0xb2e, one's complement magnitude). */
static void op_light(void)
{
    SET_AX(M16(esi + 2));
    SET_BX(M16(esi + 4));
    SET_CX(M16(esi + 6));
    SET_DX(M16(esi + 8));
    M16(V(0x20710aa4)) = AX;
    SET_AX(AX & 0x8000);
    if (AX != 0)
        M16(V(0x20710aa4)) = (uint16_t)-M16(V(0x20710aa4));
    M16(V(0x20710ab2)) = AX;
    M16(V(0x20710abe)) = BX;
    M16(V(0x20710ac0)) = CX;
    M16(V(0x20710ac2)) = DX;
    esi += 0xa;
    M32(V(0x206f9126)) = esi;
    M16(V(0x20710aa8)) = 0;
    edi = G;
    SET_AX(M16(edi + 0xb2e));
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    SET_AX(AX ^ DX);
    shade_colour();
}

/* 0x206f4ed8 -- 0x6e, the same. */
static void op_light_6e(void)
{
    op_light();
}


/* ---- rows of points: .text+0x2ef0..+0x30a3 -------------------------------------------- */

/* 0x206e3ffe -- a row's step: from the point at ebx to the one at edi in cx
 * points, the first to 0x206f90c0 and the step per point to 0x206f90f8..
 * 0x206f9100 (each axis over cx - 1 by idiv; nothing when cx is 1). */
static void row_steps(void)
{
    uint16_t keep = CX;

    eax = M32(edi);
    edx = M32(ebx);
    M32(V(0x206f90c0)) = edx;
    eax -= edx;
    cdq();
    SET_CX(CX - 1);
    if (CX == 0) {
        eax = 0;
        M32(V(0x206f90f8)) = eax;
        M32(V(0x206f90fc)) = eax;
        M32(V(0x206f9100)) = eax;
        edx = M32(ebx + 4);
        eax = M32(ebx + 8);
        M32(V(0x206f90c4)) = edx;
        M32(V(0x206f90c8)) = eax;
        SET_CX(keep);
        return;
    }
    ecx = CX;
    idiv32(ecx);
    M32(V(0x206f90f8)) = eax;
    eax = M32(edi + 4);
    edx = M32(ebx + 4);
    M32(V(0x206f90c4)) = edx;
    eax -= edx;
    cdq();
    idiv32(ecx);
    M32(V(0x206f90fc)) = eax;
    eax = M32(edi + 8);
    edx = M32(ebx + 8);
    M32(V(0x206f90c8)) = edx;
    eax -= edx;
    cdq();
    idiv32(ecx);
    M32(V(0x206f9100)) = eax;
    SET_CX(keep);
}

/* 0x206e3fc8 -- a row's two ends, three words each at esi, as points into
 * ebx and edi; then its steps. */
static void row_ends(void)
{
    uint32_t keep_ecx = ecx, keep_edi = edi;

    edi = ebx;
    SET_AX(M16(esi));
    SET_BX(M16(esi + 2));
    SET_DX(M16(esi + 4));
    point_from_words();
    esi += 6;
    ebx = keep_edi;
    keep_edi = edi;
    edi = ebx;
    SET_AX(M16(esi));
    SET_BX(M16(esi + 2));
    SET_DX(M16(esi + 4));
    point_from_words();
    esi += 6;
    ebx = keep_edi;
    ecx = keep_ecx;
    row_steps();
}

/* 0x206e406f -- the next point along the row (x, y, z left in eax, ebx,
 * edx). */
static void row_next(void)
{
    eax = M32(V(0x206f90c0));
    eax += M32(V(0x206f90f8));
    M32(V(0x206f90c0)) = eax;
    ebx = M32(V(0x206f90c4));
    ebx += M32(V(0x206f90fc));
    M32(V(0x206f90c4)) = ebx;
    edx = M32(V(0x206f90c8));
    edx += M32(V(0x206f9100));
    M32(V(0x206f90c8)) = edx;
}

/* 0x206e3ef0 -- 0x35, a row of points: two ends and a count (the word at
 * +0xe), each point drawn, counted in 0x206fd200.  loop on the count: 0
 * walks four billion. */
static void op_row(void)
{
    M16(V(0x206fd200)) = 0;
    ecx = M16(esi + 0xe);
    esi += 2;
    ebx = V(0x206f9104);
    edi = V(0x206f9110);
    row_ends();
    for (;;) {
        uint32_t keep = ecx;

        point();
        M16(V(0x206fd200)) += 1;
        ecx = keep;
        if (--ecx == 0)
            break;
        row_next();
    }
    M32(V(0x206f9126)) += 0x10;
}

/* 0x206e3ece -- 0x45, the same with GLOBAL +0x1286 one less while it draws,
 * and 0 after. */
static void op_row_1286(void)
{
    edi = G;
    M16(edi + 0x1286) -= 1;
    op_row();
    edi = G;
    M16(edi + 0x1286) = 0;
}

/* 0x206e3f2e -- 0x36, a row of points with a bit each from +0x1e: bit al
 * of the byte at esi, al rotated left a point -- and esi moved on by adc
 * esi,1, a byte a point and one more each time al comes round, not a byte
 * each eight.  The counter goes to (ebx + 15) >> 4 past +0x1e, ebx as the
 * last point left it: its screen x when drawn, its eye-space x when out of
 * view, the step's y when its bit is clear (the first end's address with
 * one point). */
static void op_row_mask(void)
{
    uint32_t keep_esi;

    ecx = M16(esi + 0xe);
    esi += 2;
    ebx = V(0x206f9104);
    edi = V(0x206f9110);
    row_ends();
    keep_esi = esi;
    esi += 0x10;
    SET_AL(1);
    for (;;) {
        if (M8(esi) & AL) {
            uint32_t a = eax, c = ecx, s = esi;

            point();
            esi = s, ecx = c, eax = a;
        }
        if (--ecx == 0)
            break;
        {
            uint32_t carry = AL >> 7;

            SET_AL((uint8_t)(AL << 1 | carry));
            esi += 1 + carry;
        }
        {
            uint32_t a = eax;

            row_next();
            eax = a;
        }
    }
    esi = keep_esi;
    SET_AX(M16(esi + 0xe));
    ebx += 0xf;
    ebx >>= 4;
    eax = ebx + esi + 0x10;
    M32(V(0x206f9126)) = eax;
}

/* 0x206e3f7e -- 0x31, a row into the point table: from the index for the
 * count (the end at index + count - 1, a word's arithmetic), the ends from
 * the two points, the rest stepped between them. */
static void op_row_table(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    ebx = M32(V(0x206f912a));
    eax <<= 4;
    ebx += eax;
    edi = ebx;
    lods16();
    ecx = AX;
    SET_AX(AX - 1);
    SET_AX(AX << 4);
    eax = (uint32_t)(int32_t)(int16_t)AX;
    edi += eax;
    row_ends();
    {
        int16_t was = (int16_t)CX;              /* sub cx,2 / jle: signed */

        SET_CX(CX - 2);
        if (was <= 2)
            goto out;
    }
    {
        esi = ebx;
        do {
            esi += 0x10;
            row_next();
            M32(esi) = eax;
            M32(esi + 4) = ebx;
            M32(esi + 8) = edx;
            SET_CX(CX - 1);
        } while (CX != 0);
    }
out:
    M32(V(0x206f9126)) += 0x12;
}


/* 0x206f4412 -- a program kept for later in list ebx (of the 64 at
 * 0x20710360, 0x1c bytes each; 0x40 and over are dropped): the target
 * ebp + esi, with 0x206f919c and the context 0x206f91b7. */
static void defer_program(void)
{
    ebp += esi;
    if (ebx >= 0x40)
        return;
    ebx = ebx * 0x1c + V(0x20710360);
    H_LIST_ADD(ebx);
    M32(eax) = ebp;
    edx = M32(V(0x206f919c));
    M32(eax + 4) = edx;
    edx = M32(V(0x206f91b7));
    M32(eax + 8) = edx;
}

/* 0x206f43e9 -- 0x74, a program for later: a signed word offset and the
 * list's number. */
static void op_defer(void)
{
    ebp = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    ebx = M16(esi + 4);
    M32(V(0x206f9126)) += 6;
    defer_program();
}

/* 0x206f43fe -- 0x8b, the same with a dword offset. */
static void op_defer_far(void)
{
    ebp = M32(esi + 2);
    ebx = M16(esi + 6);
    M32(V(0x206f9126)) += 8;
    defer_program();
}

#define HG2D_VALUE_POINT(s, x, y, z, var, word) \
    (eax = import_call(M32(HG2D + 0x28), (const uint32_t[]){ s, x, y, z, var, word }, 6, HG2D))

/* 0x206e4613 -- the point at 0x206f90c0 projected as point() does (without
 * its GLOBAL +0x1286 highlight and 0x206f9161 write), to HG2D +0x28 with
 * the variable's address (0x206fd6d4) and the word at 0x206fd6d8. */
static void value_point(void)
{
    ebx = M32(V(0x206f90c0));
    ecx = M32(V(0x206f90c4));
    ebp = M32(V(0x206f90c8));
    eax = ebx;
    cdq();
    eax ^= edx;
    if ((int32_t)eax >= (int32_t)ebp)
        return;
    eax = ecx;
    cdq();
    eax ^= edx;
    if ((int32_t)eax >= (int32_t)ebp)
        return;
    { uint32_t t = ebp; ebp = ebx; ebx = t; }
    edi = G;
    eax = M32(edi + 0x1198);
    esi = M32(eax + 8);
    M32(V(0x206f92f6)) = esi;
    esi = M32(eax + 0xc);
    M32(V(0x206f92f2)) = esi;
    eax = M32(V(0x206f92f6));
    eax <<= 16;
    imul32(ecx);
    idiv32(ebx);
    esi = M32(V(0x206f92f2));
    esi <<= 16;
    eax += esi;
    ecx = eax;
    eax = M32(edi + 0x1198);
    esi = M32(eax);
    M32(V(0x206f92f6)) = esi;
    esi = M32(eax + 4);
    M32(V(0x206f92f2)) = esi;
    eax = M32(V(0x206f92f6));
    eax <<= 16;
    imul32(ebp);
    idiv32(ebx);
    esi = M32(V(0x206f92f2));
    esi <<= 16;
    eax += esi;
    ebx = eax;
    {
        uint32_t c = ecx;

        eax = M32(V(0x206f90c8));
        SET_CX(M16(edi + 0xd42));
        eax = shl32(eax, CL);
        ecx = M32(edi + 0x320);
        edx = 0;
        div32(ecx);
        ecx = c;
    }
    edi = M32(edi + 0x1194);
    edi += 0x80;
    ebx >>= 16;
    ecx >>= 16;
    esi = eax;
    HG2D_VALUE_POINT(edi, ebx, ecx, esi, M32(V(0x206fd6d4)), M16(V(0x206fd6d8)));
}

/* 0x206e459d -- 0x91, a value at a point: outside the map, with the
 * variable (+8) nonzero, the point (+2) through value_point(), the word at
 * +0xa with it.  Twelve bytes. */
static void op_value_point(void)
{
    edi = G;
    if (M16(edi + 0xdca) == 0) {
        variable_at(esi + 8);
        M32(V(0x206fd6d4)) = ebx;
        SET_AX(M16(esi + 0xa));
        M16(V(0x206fd6d8)) = AX;
        if (M32(ebx) != 0) {
            esi += 2;
            edi = V(0x206f90c0);
            SET_AX(M16(esi));
            SET_BX(M16(esi + 2));
            SET_DX(M16(esi + 4));
            point_from_words();
            esi += 6;
            value_point();
        }
    }
    M32(V(0x206f9126)) += 0xc;
}


/* ---- positions in the world: .text+0x586c..+0x5bde -------------------------------- */

/* Positions are records of three 48-bit values, a word and a dword each:
 * latitude at +2, longitude at +0xa, altitude at +0x12.  GLOBAL +0x5b0 is
 * the eye's, +0x560 the reference's. */

#define UTIL_SINCOS(angle, sin, cos)  (eax = IMP3(UTIL, 0x2c, angle, sin, cos))
#define UTIL_M32_TO_LON(m, lat, out)  (eax = IMP3(UTIL, 0x40, m, lat, out))
#define UTIL_LON_TO_M(lon, lat, out)  (eax = IMP3(UTIL, 0x48, lon, lat, out))
#define UTIL_ROTATE(angles, v, out)   (eax = IMP3(UTIL, 0x4c, angles, v, out))

/* 0x206e686c -- the object whose record is at eax relative to the eye, into
 * GLOBAL +0x2fc (the longitude difference in metres, UTIL +0x48, at the
 * eye's latitude), +0x30a and +0x316 (altitude and latitude, 48-bit);
 * 0x206f917e and 0x206f9180 cleared. */
static void object_relative(void)
{
    uint32_t keep_esi = esi, r;
    uint16_t borrow;

    esi = eax;
    M16(V(0x206f917e)) = 0;
    M16(V(0x206f9180)) = 0;
    edi = G;
    SET_AX(M16(edi + 0x5ba));
    ecx = M32(edi + 0x5bc);
    edx = sar32(ecx, 16);
    r = (uint32_t)AX - M16(esi + 0xa);
    SET_AX(r);
    borrow = (r >> 16) & 1;
    r = (uint32_t)CX - M16(esi + 0xc) - borrow;
    SET_CX(r);
    borrow = (r >> 16) & 1;
    SET_DX(DX - M16(esi + 0xe) - borrow);
    ebx = edi + 0x5b0;
    M16(V(0x206fd892)) = AX;
    M16(V(0x206fd894)) = CX;
    M16(V(0x206fd896)) = DX;
    ecx = edi + 0x2fc;
    {
        uint32_t s = esi;

        UTIL_LON_TO_M(V(0x206fd890), ebx, ecx);
        esi = s;
    }
    {
        uint64_t v, w;

        eax = M32(edi + 0x5c4);
        edx = (edx << 16) | (eax >> 16);        /* shld edx,eax,0x10 */
        eax <<= 16;
        SET_AX(M16(edi + 0x5c2));
        v = (uint64_t)DX << 32 | eax;
        w = (uint64_t)M16(esi + 0x16) << 32 | M32(esi + 0x12);
        v -= w;
        eax = (uint32_t)v;
        SET_DX((uint16_t)(v >> 32));
        M16(edi + 0x30a) = AX;
        eax = (eax >> 16) | (edx << 16);        /* shrd eax,edx,0x10 */
        M32(edi + 0x30c) = eax;
        eax = M32(edi + 0x5b4);
        edx = (edx << 16) | (eax >> 16);
        eax <<= 16;
        SET_AX(M16(edi + 0x5b2));
        v = (uint64_t)DX << 32 | eax;
        w = (uint64_t)M16(esi + 6) << 32 | M32(esi + 2);
        v -= w;
        eax = (uint32_t)v;
        SET_DX((uint16_t)(v >> 32));
        M16(edi + 0x316) = AX;
        eax = (eax >> 16) | (edx << 16);
        M32(edi + 0x318) = eax;
    }
    esi = keep_esi;
}

/* 0x206e69cd -- once (0x206f917e), the eye relative to the reference and
 * to the object: x (0x206f9182, 0x206f9186) from the longitude difference
 * in metres less GLOBAL +0x2fe, y (0x206f918a) and z (0x206f918e,
 * 0x206f9192) from altitude and latitude, each negated. */
static void eye_in_object(void)
{
    uint32_t r;

    if (M16(V(0x206f917e)) != 0)
        return;
    edi = G;
    SET_AX(M16(edi + 0x5ba));
    ecx = M32(edi + 0x5bc);
    r = (uint32_t)AX - M16(edi + 0x56a);
    SET_AX(r);
    ecx = ecx - M32(edi + 0x56c) - ((r >> 16) & 1);
    M32(V(0x206fd894)) = ecx;
    M16(V(0x206fd892)) = AX;
    ecx = edi + 0x5b0;
    UTIL_LON_TO_M(V(0x206fd890), ecx, V(0x206fd898));
    SET_AX(M16(V(0x206fd89c)));
    eax <<= 16;
    SET_AX(M16(V(0x206fd89a)));
    eax -= M32(edi + 0x2fe);
    eax = (uint32_t)-(int32_t)eax;
    M32(V(0x206f9182)) = eax;
    M32(V(0x206f9186)) = eax;
    edi = G;
    eax = M32(edi + 0x5c2);
    eax -= M32(edi + 0x572);
    eax -= M32(edi + 0x30a);
    eax = (uint32_t)-(int32_t)eax;
    M32(V(0x206f918a)) = eax;
    edi = G;
    eax = M32(edi + 0x5b2);
    eax -= M32(edi + 0x562);
    eax -= M32(edi + 0x316);
    eax = (uint32_t)-(int32_t)eax;
    M32(V(0x206f918e)) = eax;
    M32(V(0x206f9192)) = eax;
    M16(V(0x206f917e)) = 1;
}

/* p << 17 of a 64-bit product, as shld edx,eax,0x11 / shl eax,0x11 */
static uint64_t times_heading(uint16_t a, uint32_t b)
{
    int64_t p = (int64_t)(int16_t)a * (int32_t)b;

    return (uint64_t)p << 17;
}

/* 0x206e6aa8 -- once (0x206f9180), x and z turned by the heading at
 * 0x206f9198 (UTIL +0x2c's sine and cosine at 0x206f91f5/0x206f91fb) when
 * 0x206f9196 is set, as they are otherwise; a heading of 0 with
 * 0x206f9196 set leaves them and the flag as they were. */
static void eye_in_object_turned(void)
{
    uint64_t a, b, c;

    eye_in_object();
    if (M16(V(0x206f9180)) != 0)
        return;
    if (M16(V(0x206f9196)) == 0) {
        eax = M32(V(0x206f9186));
        M32(V(0x206f9182)) = eax;
        eax = M32(V(0x206f9192));
        M32(V(0x206f918e)) = eax;
        M16(V(0x206f9180)) = 1;
        return;
    }
    SET_BX(M16(V(0x206f9198)));
    if (BX == 0)
        return;
    UTIL_SINCOS(BX, V(0x206f91f5), V(0x206f91fb));
    a = times_heading(M16(V(0x206f91fb)), M32(V(0x206f9186)));
    M32(V(0x206fd1c0)) = (uint32_t)a;
    M32(V(0x206fd1c4)) = (uint32_t)(a >> 32);
    b = times_heading(M16(V(0x206f91f5)), M32(V(0x206f9186)));
    M32(V(0x206fd1d0)) = (uint32_t)b;
    M32(V(0x206fd1d4)) = (uint32_t)(b >> 32);
    c = times_heading(M16(V(0x206f91f5)), M32(V(0x206f9192)));
    M32(V(0x206fd1c8)) = (uint32_t)c;
    M32(V(0x206fd1cc)) = (uint32_t)(c >> 32);
    a -= c;
    eax = (uint32_t)a;
    edx = (uint32_t)(a >> 32);
    M32(V(0x206f9182)) = edx;
    c = times_heading(M16(V(0x206f91fb)), M32(V(0x206f9192)));
    M32(V(0x206fd1c8)) = (uint32_t)c;
    M32(V(0x206fd1cc)) = (uint32_t)(c >> 32);
    c += b;
    eax = (uint32_t)c;
    edx = (uint32_t)(c >> 32);
    M32(V(0x206f918e)) = edx;
    M16(V(0x206f9180)) = 1;
}

/* 0x206f1fb0 -- 0x73, the eye in a box: on past the instruction when the
 * turned position's integer words (0x206f9184, 0x206f918c, 0x206f9190) are
 * each within their signed ranges, to the target otherwise. */
static void op_eye_in_box(void)
{
    uint32_t i;
    static const uint32_t at[3] = { 0x206f9184, 0x206f918c, 0x206f9190 };

    {
        uint32_t keep = esi;

        eye_in_object_turned();
        esi = keep;
    }
    ebp = esi;
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    ebp += eax;
    for (i = 0; i < 3; i++) {
        SET_AX(M16(V(at[i])));
        if ((int16_t)AX < (int16_t)M16(esi) || (int16_t)AX > (int16_t)M16(esi + 2)) {
            M32(V(0x206f9126)) = ebp;
            return;
        }
        esi += 4;
    }
    M32(V(0x206f9126)) = esi;
}

/* x + sign(x) + the top bit of the fraction: cwd / add lo,lo / adc hi,dx */
static uint16_t round_word(uint16_t hi, uint16_t lo)
{
    return (uint16_t)(hi + ((int16_t)hi < 0 ? 0xffff : 0) + (lo >> 15));
}

/* 0x206f200d -- 0x1b, the eye in a turned box: the eye less the box's
 * centre (words +4..+8 off 0x206f9188, 0x206f918c, 0x206f9194), turned by
 * the negated angles at +0x10.. (the second and third exchanged) through
 * UTIL +0x4c, rounded to words; on past the instruction when each is
 * inside the half extents at +0xa.. (half of each plus it less it borrows),
 * to the target otherwise. */
static void op_eye_in_turned_box(void)
{
    uint16_t bp_, cx_, di_;
    uint32_t keep;

    keep = esi;
    eye_in_object();
    esi = keep;
    SET_DX(M16(V(0x206f9188)) - M16(esi + 4));
    SET_CX(M16(V(0x206f918c)) - M16(esi + 6));
    SET_DI(M16(V(0x206f9194)) - M16(esi + 8));
    keep = esi;
    SET_AX(M16(esi + 0x10));
    SET_BX(M16(esi + 0x12));
    SET_SI(M16(esi + 0x14));
    {
        uint16_t t = CX;

        SET_CX(SI);
        SET_SI(t);
    }
    SET_AX(-AX);
    SET_BX(-BX);
    SET_CX(-CX);
    M16(V(0x207087f2)) = AX;
    M16(V(0x207087f4)) = BX;
    M16(V(0x207087f6)) = CX;
    M16(V(0x207087f8)) = DX;
    M16(V(0x207087fa)) = SI;
    M16(V(0x207087fc)) = DI;
    UTIL_ROTATE(V(0x207087f2), V(0x207087f8), V(0x207087fe));
    SET_AX(M16(V(0x207087fe)));
    SET_DX(M16(V(0x20708800)));
    SET_BX(M16(V(0x20708802)));
    SET_CX(M16(V(0x20708804)));
    SET_SI(M16(V(0x20708806)));
    SET_DI(M16(V(0x20708808)));
    bp_ = round_word(DX, AX);
    cx_ = round_word(CX, BX);
    di_ = round_word(DI, SI);
    SET_AX(DI);                                 /* mov ax,di / cwd, before adc */
    SET_DX((int16_t)DI < 0 ? 0xffff : 0);
    SET_BX(BX + BX);
    SET_SI(SI + SI);
    SET_BP(bp_);
    SET_CX(cx_);
    SET_DI(di_);
    esi = keep;
    {
        static const uint32_t ext[3] = { 0xa, 0xc, 0xe };
        uint16_t v[3] = { bp_, cx_, di_ };
        uint32_t i;

        for (i = 0; i < 3; i++) {
            uint16_t e = M16(esi + ext[i]);
            uint16_t a = (uint16_t)((e >> 1) + v[i]);

            SET_AX(a - e);
            if (a >= e) {
                eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
                esi += eax;
                M32(V(0x206f9126)) = esi;
                return;
            }
        }
    }
    M32(V(0x206f9126)) += 0x16;
}

/* 0x206e723f -- 0x40, an object's shadow: with 0x206fd8dd set, or GLOBAL
 * +0x11b4 and +0xb34, the object (the variable at +2's record) moved down
 * the sun (GLOBAL +0xb2a/+0xb2e/+0xb32) by its height over the ground
 * (0x206f916a), latitude directly and longitude through UTIL +0x40 at the
 * reference's latitude, into the record at 0x206f91bf, and that made the
 * object (object_relative()).  With 0x206fd8dd set, the height's distance
 * (its bits 3..18 plus 0x100) past 0x1fff, or the height's top word
 * (capped at 0x7fff) within 0x2400 of it, stops the program; otherwise the
 * last goes negated to GLOBAL +0xb34. */
static void op_shadow(void)
{
    uint64_t v;

    if (M16(V(0x206fd8dd)) == 0) {
        edi = G;
        if (M16(edi + 0x11b4) == 0 || M16(edi + 0xb34) == 0)
            goto on;
    }
    variable_at(esi + 2);
    SET_AX(M16(ebx + 2));
    edx = M32(ebx + 4);
    M16(V(0x206f91c1)) = AX;
    M32(V(0x206f91c3)) = edx;
    SET_AX(M16(ebx + 0xa));
    edx = M32(ebx + 0xc);
    M16(V(0x206f91c9)) = AX;
    M32(V(0x206f91cb)) = edx;
    SET_AX(M16(V(0x206f916a)));
    edx = M32(V(0x206f916c));
    M16(V(0x206f91d1)) = AX;
    M32(V(0x206f91d3)) = edx;
    v = ((uint64_t)M32(ebx + 0x14) << 16 | M16(ebx + 0x12)) -
        ((uint64_t)M32(V(0x206f91d3)) << 16 | M16(V(0x206f91d1)));
    SET_AX((uint16_t)v);
    edx = (uint32_t)(v >> 16);
    M16(V(0x206fd8e1)) = AX;
    M32(V(0x206fd8e3)) = edx;
    edi = G;
    eax = (uint32_t)(int32_t)(int16_t)M16(edi + 0xb32);
    imul32(M32(V(0x206fd8e1)));
    ebx = (uint32_t)(int32_t)(int16_t)M16(edi + 0xb2e);
    idiv32(ebx);
    cdq();
    v = ((uint64_t)M16(V(0x206f91c5)) << 32 | M32(V(0x206f91c1))) -
        ((uint64_t)(uint16_t)edx << 32 | eax);
    M32(V(0x206f91c1)) = (uint32_t)v;
    M16(V(0x206f91c5)) = (uint16_t)(v >> 32);
    eax = (uint32_t)(int32_t)(int16_t)M16(edi + 0xb2a);
    imul32(M32(V(0x206fd8e1)));
    ebx = (uint32_t)(int32_t)(int16_t)M16(edi + 0xb2e);
    idiv32(ebx);
    M32(V(0x206fd8ef)) = eax;
    eax = edi + 0x560;
    UTIL_M32_TO_LON(V(0x206fd8ef), eax, V(0x206fd8f3));
    SET_AX(M16(V(0x206fd8f5)));
    edx = M32(V(0x206fd8f7));
    v = ((uint64_t)M32(V(0x206f91cb)) << 16 | M16(V(0x206f91c9))) -
        ((uint64_t)edx << 16 | AX);
    M16(V(0x206f91c9)) = (uint16_t)v;
    M32(V(0x206f91cb)) = (uint32_t)(v >> 16);
    eax = V(0x206f91bf);
    object_relative();
    if (M16(V(0x206fd8dd)) == 0)
        goto on;
    {
        uint32_t d = ((uint32_t)M16(V(0x206fd8e4)) << 16 | M16(V(0x206fd8e2))) >> 3;

        d += 0x100;
        SET_DX(d >> 16);
        SET_AX(d);
    }
    if (AX > 0x1fff)
        goto stop;
    M16(V(0x206fd8e7)) = AX;
    SET_AX(M16(V(0x206fd8e2)));
    if (AX >= 0x7fff)
        SET_AX(0x7fff);
    SET_AX(AX ^ 0x7fff);
    if (AX < 0x2400)
        goto stop;
    SET_AX(-AX);
    edi = G;
    M16(edi + 0xb34) = AX;
on:
    M32(V(0x206f9126)) += 4;
    return;
stop:
    M32(V(0x206fd204)) = 1;
}

/* 0x206e8bda -- 0x82, a shadow of the object in the instruction itself:
 * 0x40 run on an instruction at 0x206fd8e7 whose variable (0x206fd8e9) is
 * 0 in the context of the bytes after the opcode; the counter and context
 * kept round it, on past 0x14 bytes. */
static void op_own_shadow(void)
{
    uint32_t keep_ctx = M32(V(0x206f91b7)), keep_pc = M32(V(0x206f9126));

    eax = V(0x206fd8e7);
    M32(V(0x206f9126)) = eax;
    esi += 2;
    M32(V(0x206f91b7)) = esi;
    M16(V(0x206fd8e9)) = 0;
    esi = M32(V(0x206f9126));
    op_shadow();
    M32(V(0x206f9126)) = keep_pc;
    M32(V(0x206f91b7)) = keep_ctx;
    M32(V(0x206f9126)) += 0x14;
}


/* Retail keeps the distance on the x87; on x86 the same instructions are
 * used (as UTIL does), elsewhere long double, which FS98_PORTABLE_FP
 * forces for checking. */
#if (defined(__i386__) || defined(__x86_64__)) && !defined(FS98_PORTABLE_FP)
#  define G3D_FP_X87 1
#else
#  define G3D_FP_X87 0
#  include <math.h>
#endif

/* 0x206e694a -- the object's distance, GLOBAL +0xd4c: the square root of
 * the sum of the squares of the three 48-bit offsets (GLOBAL +0x2fe, +0x30a,
 * +0x316, each as a qword with 16 zero bits below), each times the double
 * at 0x206fd8c0, rounded by fistp (the low dword kept). */
static void object_distance(void)
{
    int64_t q[3], r;
    uint32_t i;
    static const uint32_t at[3] = { 0x2fe, 0x30a, 0x316 };

    edi = G;
    for (i = 0; i < 3; i++)
        q[i] = (int64_t)((uint64_t)M32(edi + at[i] + 2) << 32 |
                         (uint32_t)M16(edi + at[i]) << 16);
#if G3D_FP_X87
    __asm__ __volatile__ ("fildll %1\n\tfmull %4\n\tfmul %%st(0), %%st\n\t"
                          "fildll %2\n\tfmull %4\n\tfmul %%st(0), %%st\n\tfaddp\n\t"
                          "fildll %3\n\tfmull %4\n\tfmul %%st(0), %%st\n\tfaddp\n\t"
                          "fsqrt\n\tfistpll %0"
                          : "=m" (r)
                          : "m" (q[0]), "m" (q[1]), "m" (q[2]),
                            "m" (*(const double *)(uintptr_t)V(0x206fd8c0))
                          : "st");
#else
    {
        long double k = *(const double *)(uintptr_t)V(0x206fd8c0), sum = 0, d;

        for (i = 0; i < 3; i++) {
            d = (long double)q[i] * k;
            sum += d * d;
        }
        d = sqrtl(sum);
        r = d >= 9223372036854775808.0L || d != d ? (int64_t)0x8000000000000000LL
                                                  : (int64_t)nearbyintl(d);
    }
#endif
    eax = (uint32_t)r;
    M32(edi + 0xd4c) = eax;
}

/* 0x206e6606 -- 0x34, an object's scale: GLOBAL +0xd42 from +0xd44, the
 * shift at +8 to GLOBAL +0x31c and as 1 << (16 - shift) (cl's arithmetic)
 * to +0x320; the eye's offsets (GLOBAL +0x2ff, +0x30b, +0x317, the middle
 * dwords of the 48-bit values) shifted up into the eye (GLOBAL +0xcdc..)
 * by what fits beside the shift, the rest into the scale word 0x206f9159
 * -- the shift is shld of a register with itself, a rotate -- and four
 * more with the view's +0x14 set and +0xd42 at 8 or more.  Then on to the
 * target when the distance (+4 nonzero) is past it or GLOBAL +0xd4f is set,
 * or the radius (+6 nonzero) is out of view; past ten bytes otherwise. */
static void op_object_scale(void)
{
    uint16_t shift, n;

    edi = G;
    SET_AX(M16(edi + 0xd44));
    M16(edi + 0xd42) = AX;
    SET_BX(M16(esi + 8));
    M8(edi + 0x31c) = BL;
    eax = 0x10000;
    SET_CL(0x10 - BL);
    if (CL & 0x80) {
        SET_CL(-CL);
        eax >>= CL & 31;
    } else {
        eax <<= CL & 31;
    }
    M32(edi + 0x320) = eax;
    shift = BX;
    ebp = M32(edi + 0x2ff);
    esi = M32(edi + 0x317);
    edi = M32(edi + 0x30b);
    eax = ebp;
    cdq();
    eax ^= edx;
    ebx = eax;
    eax = edi;
    cdq();
    eax ^= edx;
    ebx |= eax;
    eax = esi;
    cdq();
    eax ^= edx;
    ebx |= eax;
    ecx = 0;
    if (ebx != 0)                               /* bsr leaves ecx when 0 */
        ecx = 31 - __builtin_clz(ebx);
    SET_BX(shift);
    SET_AX(0x18 - BX);
    SET_BX(AX);
    SET_BX(BX + CX);
    if ((int16_t)BX >= 0x1e) {
        SET_BX(BX - 0x1d);
        M16(V(0x206f9159)) = BX;
        SET_CX(AX - BX);
    } else {
        M16(V(0x206f9159)) = 0;
        SET_CX(AX);
    }
    {
        uint32_t g = G;

        eax = M32(g + 0x2ff);
        ebx = M32(g + 0x30b);
        edx = M32(g + 0x317);
    }
    n = CL & 31;
    if (n) {                                    /* shld r,r,cl */
        ebp = (ebp << n) | (eax >> (32 - n));
        edi = (edi << n) | (ebx >> (32 - n));
        esi = (esi << n) | (edx >> (32 - n));
    }
    eax = G;
    eax = M32(eax + 0x1198);
    if (M32(eax + 0x14) != 0) {
        eax = G;
        if (M16(eax + 0xd42) >= 8) {
            M16(V(0x206f9159)) += 4;
            ebp = sar32(ebp, 8);
            edi = sar32(edi, 8);
            esi = sar32(esi, 8);
            M16(eax + 0xd42) -= 4;
        }
    }
    eax = G;
    M32(eax + 0xcdc) = ebp;
    M32(eax + 0xce0) = edi;
    M32(eax + 0xce4) = esi;
    ecx = M16(V(0x206f9159));
    M32(eax + 0x320) >>= CL & 31;
    esi = M32(V(0x206f9126));
    SET_AX(M16(esi + 4));
    if (AX != 0) {
        object_distance();
        esi = M32(V(0x206f9126));
        edi = G;
        if (M8(edi + 0xd4f) != 0)
            goto target;
        SET_BX(M16(esi + 4));
        if (BX < M16(edi + 0xd4d))
            goto target;
    }
    ebx = M32(V(0x206f9126));
    SET_DX(M16(ebx + 6));
    eax = 0xa;
    if (DX != 0) {
        SET_AX(0);
        SET_BX(0);
        SET_CX(0);
        sphere_in_view(DX);
        {
            uint16_t in = AX;

            eax = 0xa;
            if (in == 0)
                goto target;
        }
    }
    M32(V(0x206f9126)) += eax;
    return;
target:
    ebx = M32(V(0x206f9126));
    SET_AX(M16(ebx + 2));
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f9126)) += eax;
}


/* ---- objects: .text+0x5189..+0x5c59 -------------------------------------------------- */

/* 0x206e6189 -- an object's place from its queue entry (offset eax in the
 * queue at 0x206fd87c): the three 48-bit offsets to GLOBAL +0x2fe, +0x30a,
 * +0x316 and its distance to +0xd4c. */
static void object_from_queue(void)
{
    edi = M32(V(0x206fd87c));
    edi += eax;
    ebx = G;
    eax = M32(edi + 0x10);
    M32(ebx + 0x300) = eax;
    SET_AX(M16(edi + 0xe));
    M16(ebx + 0x2fe) = AX;
    eax = M32(edi + 0x18);
    M32(ebx + 0x30c) = eax;
    SET_AX(M16(edi + 0x16));
    M16(ebx + 0x30a) = AX;
    eax = M32(edi + 0x20);
    M32(ebx + 0x318) = eax;
    SET_AX(M16(edi + 0x1e));
    M16(ebx + 0x316) = AX;
    eax = M32(edi + 4);
    M32(ebx + 0xd4c) = eax;
}

/* 0x206e6821 -- a position packed in 18 bytes at eax into a record at
 * 0x206fd8a6 (eax then points at it), unless bit 0 of 0x206fd8be, shifted
 * out, says eax is a record already. */
static void unpack_position(void)
{
    uint16_t w = M16(V(0x206fd8be));

    M16(V(0x206fd8be)) = w >> 1;
    if (w & 1)
        return;
    SET_CX(M16(eax));
    M16(V(0x206fd8a8)) = CX;
    ecx = M32(eax + 2);
    M32(V(0x206fd8aa)) = ecx;
    SET_CX(M16(eax + 6));
    M16(V(0x206fd8b0)) = CX;
    ecx = M32(eax + 8);
    M32(V(0x206fd8b2)) = ecx;
    SET_CX(M16(eax + 0xc));
    M16(V(0x206fd8b8)) = CX;
    ecx = M32(eax + 0xe);
    M32(V(0x206fd8ba)) = ecx;
    eax = V(0x206fd8a6);
}

/* one row of the view matrix (words at m0, m1, m2 off GLOBAL) by the
 * negated offset, 64-bit, shrd 15 */
static uint32_t eye_row(uint32_t m0, uint32_t m1, uint32_t m2, uint32_t x, uint32_t y)
{
    int64_t s;
    uint32_t g = esi;

    eax = (uint32_t)(int32_t)(int16_t)M16(g + m0);
    imul32(x);
    s = (int64_t)(((uint64_t)edx << 32) | eax);
    eax = (uint32_t)(int32_t)(int16_t)M16(g + m1);
    imul32(y);
    s += (int64_t)(((uint64_t)edx << 32) | eax);
    eax = (uint32_t)(int32_t)(int16_t)M16(g + m2);
    imul32(M32(V(0x206fd21c)));
    s += (int64_t)(((uint64_t)edx << 32) | eax);
    eax = (uint32_t)s;
    edx = (uint32_t)((uint64_t)s >> 32);
    return (eax >> 15) | (edx << 17);
}

/* 0x206e1774 -- the object's bounding sphere, radius ax (times 0xb504f334,
 * the high dword: over sqrt 2), against the view in eye space: the
 * negated offsets' integer dwords (GLOBAL +0x300, +0x30c, +0x318) through
 * the view matrix.  ax 0 out of view, 1 partly in, 2 wholly in. */
static void sphere_in_eye(void)
{
    uint32_t r, x, y, z, b;

    eax = (uint32_t)(int32_t)(int16_t)AX;
    {
        uint64_t p = (uint64_t)eax * 0xb504f334u;

        eax = (uint32_t)p;
        edx = (uint32_t)(p >> 32);
    }
    r = edx;
    esi = G;
    ebp = (uint32_t)-(int32_t)M32(esi + 0x300);
    edi = (uint32_t)-(int32_t)M32(esi + 0x30c);
    esi = (uint32_t)-(int32_t)M32(esi + 0x318);
    M32(V(0x206fd21c)) = esi;
    esi = G;
    x = eye_row(0xce8, 0xcee, 0xcf4, ebp, edi);
    y = eye_row(0xcea, 0xcf0, 0xcf6, ebp, edi);
    z = eye_row(0xcec, 0xcf2, 0xcf8, ebp, edi);
    eax = z;
    ecx = y;
    edx = x;
    ebp = r;
    esi = (uint32_t)-(int32_t)ebp;
#define OUT_ { SET_AX(0); return; }
#define PART_ { SET_AX(1); return; }
    if ((int32_t)eax <= (int32_t)esi) OUT_
    b = edx - eax;
    ebx = b;
    if ((int32_t)b >= (int32_t)ebp) OUT_
    if ((int32_t)b >= (int32_t)esi) PART_
    b = edx + eax;
    ebx = b;
    if ((int32_t)b <= (int32_t)esi) OUT_
    if ((int32_t)b <= (int32_t)ebp) PART_
    b = ecx - eax;
    ebx = b;
    if ((int32_t)b >= (int32_t)ebp) OUT_
    if ((int32_t)b >= (int32_t)esi) PART_
    b = ecx + eax;
    ebx = b;
    if ((int32_t)b <= (int32_t)esi) OUT_
    if ((int32_t)b <= (int32_t)ebp) PART_
#undef OUT_
#undef PART_
    SET_AX(2);
}

/* 0x206e63ee -- the eye (GLOBAL +0xcdc..) from the three 48-bit offsets
 * times the scale GLOBAL +0x320 (the fraction word unsigned, the integer
 * dword signed), cx counting the axes whose doubled product leaves the
 * dword. */
static void eye_from_offsets(void)
{
    static const uint32_t at[3] = { 0x2fe, 0x30a, 0x316 };
    static const uint32_t to[3] = { 0xcdc, 0xce0, 0xce4 };
    uint16_t n = 0;
    uint32_t i;

    edi = G;
    ebx = M32(edi + 0x320);
    for (i = 0; i < 3; i++) {
        uint64_t u;
        int64_t p;

        eax = (uint32_t)M16(edi + at[i]) << 16;
        u = (uint64_t)eax * ebx;
        ecx = (uint32_t)(u >> 32);
        p = (int64_t)(int32_t)M32(edi + at[i] + 2) * (int32_t)ebx;
        u = (uint64_t)p + ecx;
        eax = (uint32_t)u;
        edx = (uint32_t)(u >> 32);
        M32(edi + to[i]) = eax;
        u <<= 1;
        eax = (uint32_t)u;
        edx = (uint32_t)(u >> 32);
        if (edx != 0) {
            edx++;
            if (edx != 0)
                n++;
        }
    }
    SET_CX(n);
}

/* 0x206e6334 -- GLOBAL +0xd42 from +0xd44, the eye from the offsets, and 1
 * when it overflowed (any axis, or the sum of the squares of its integer
 * words), which goes to GLOBAL +0x1c; the eye's words to +0x1280.. and the
 * scale word 0x206f9159 cleared.  The queue's path enters here. */
static int object_eye(void)
{
    int ov = 0;
    int32_t t;

    ebx = G;
    SET_BX(M16(ebx + 0xd44));
    ecx = G;
    M16(ecx + 0xd42) = BX;
    eye_from_offsets();
    edi = G;
    SET_BP(M16(edi + 0xcde));
    SET_DX(M16(edi + 0xce6));
    SET_DI(M16(edi + 0xce2));
    t = (int32_t)(int16_t)DX * (int16_t)DX;     /* imul ax: z */
    SET_AX(t);
    SET_DX(t >> 16);
    SET_BX(DX);
    t = (int16_t)BX + (int16_t)BX;
    SET_BX(t);
    if (t != (int16_t)t)
        goto over;
    t = (int32_t)(int16_t)BP * (int16_t)BP;     /* x */
    SET_AX(t);
    SET_DX(t >> 16);
    SET_BP(DX);
    t = (int16_t)BP + (int16_t)BP;
    SET_BP(t);
    if (t != (int16_t)t)
        goto over;
    t = (int32_t)(int16_t)DI * (int16_t)DI;     /* y */
    SET_AX(t);
    SET_DX(t >> 16);
    t = (int16_t)DX + (int16_t)DX;
    SET_DX(t);
    if (t != (int16_t)t)
        goto over;
    t = (int16_t)DX + (int16_t)BP;
    SET_DX(t);
    if (t != (int16_t)t)
        goto over;
    t = (int16_t)DX + (int16_t)BX;
    SET_DX(t);
    if (t == (int16_t)t)
        goto done;
over:
    SET_CL(1);
done:
    ov = CX != 0;                               /* add cx,0xffff / adc ax,ax */
    SET_AX(ov);
    eax = (uint32_t)(int32_t)(int16_t)AX;
    edi = G;
    M32(edi + 0x1c) = eax;
    SET_AX(M16(edi + 0xcde));
    M16(edi + 0x1280) = AX;
    SET_AX(M16(edi + 0xce2));
    M16(edi + 0x1282) = AX;
    SET_AX(M16(edi + 0xce6));
    M16(edi + 0x1284) = AX;
    M16(V(0x206f9159)) = 0;
    return ov;
}

/* 0x206e62f4 -- an object's tests and its eye: past the range (+4,
 * against the distance's top bytes GLOBAL +0xd4d/+0xd4f), GLOBAL +0xe04
 * counts down and the answer is 1; out of view by its radius (+6), 1
 * without the count.  Otherwise object_eye(). */
static int object_tests(void)
{
    SET_BX(M16(esi + 4));
    if (BX != 0) {
        edi = G;
        if (M8(edi + 0xd4f) != 0)
            goto reject;
        SET_AX(M16(edi + 0xd4d));
        if (AX > BX)
            goto reject;
    }
    SET_AX(M16(esi + 6));
    if (AX != 0) {
        sphere_in_eye();
        if (AX == 0)
            return 1;                           /* 0x206e631f, not counted */
    }
    return object_eye();
reject:
    edi = G;
    M16(edi + 0xe04) -= 1;
    return 1;
}

/* 0x206e6286 -- place the object whose position is at eax (packed, or a
 * record with 0x206fd8be's bit), at the shift bl and scale ebx: GLOBAL
 * +0xe04 cleared, +0x31c and +0x320 set; from the queue instead when
 * 0x206f917c is set and the one-shot word at +8 (taken and cleared) names
 * an entry, its tests skipped then.  1 for not drawn. */
static int place_object(void)
{
    edi = G;
    M16(edi + 0xe04) = 0;
    M8(edi + 0x31c) = BL;
    M32(edi + 0x320) = ebx;
    if (M16(V(0x206f917c)) != 0) {
        uint16_t w = M16(esi + 8);

        M16(esi + 8) = 0;
        ebx = w;
        if (ebx != 0) {
            M16(V(0x206fd8be)) = 0;
            eax = ebx;
            object_from_queue();
            M16(V(0x206f917e)) = 0;
            M16(V(0x206f9180)) = 0;
            return object_eye();
        }
    }
    unpack_position();
    object_relative();
    object_distance();
    M16(esi + 8) = 0;
    return object_tests();
}

/* The target, the signed word at +2 from the counter. */
static void object_target(void)
{
    ebx = M32(V(0x206f9126));
    SET_AX(M16(ebx + 2));
    eax = (uint32_t)(int32_t)(int16_t)AX;
    ebx += eax;
    M32(V(0x206f9126)) = ebx;
}

/* 0x206e6260 -- 0x2f, an object at a packed position (+0xe) at the scale
 * dword +0xa; on to the target when not drawn, past 0x20 bytes otherwise. */
static void op_object(void)
{
    ebx = M32(esi + 0xa);
    eax = esi + 0xe;
    if (place_object()) {
        object_target();
        return;
    }
    M32(V(0x206f9126)) += 0x20;
}

/* 0x206e6bdf -- 0x77, the same with the altitude's words first replaced,
 * in the program, by those at 0x206f91d9..0x206f91dd. */
static void op_object_ground(void)
{
    SET_AX(M16(V(0x206f91d9)));
    SET_CX(M16(V(0x206f91db)));
    SET_DX(M16(V(0x206f91dd)));
    M16(esi + 0x1a) = AX;
    M16(esi + 0x1c) = CX;
    M16(esi + 0x1e) = DX;
    op_object();
}

/* 0x206e6494 -- 0x4c, an object at the record a variable (+0xe) holds. */
static void op_object_var(void)
{
    variable_at(esi + 0xe);
    eax = ebx;
    ebx = M32(esi + 0xa);
    M16(V(0x206fd8be)) = 1;
    if (place_object()) {
        object_target();
        return;
    }
    M32(V(0x206f9126)) += 0x10;
}

/* 0x206e6c28 -- 0x83, the tests and eye again for the object last placed,
 * at the scale dword +8. */
static void op_object_again(void)
{
    edi = G;
    eax = M32(esi + 8);
    M32(edi + 0x320) = eax;
    if (object_tests()) {
        object_target();
        return;
    }
    M32(V(0x206f9126)) += 0xc;
}

/* 0x206e64ed / 0x206e658a -- 0x3a and 0x3c, an object's range and sphere
 * without placing its eye: the one-shot word at +8 (taken) names a queue
 * entry, or the object at the record a variable (+0xa) holds (0x3a) or at
 * a packed position (+0xa, 0x3c) is made relative; then past the range
 * (+4) or out of view by the radius (+6), to the target; past 0xc bytes
 * (0x3a) or 0x1c (0x3c) otherwise. */
static void object_check(int packed)
{
    uint32_t len = packed ? 0x1c : 0xc;

    SET_AX(M16(esi + 8));                       /* xchg [esi+8],ax / cwde */
    M16(esi + 8) = 0;
    eax = (uint32_t)(int32_t)(int16_t)AX;
    if (eax != 0) {
        object_from_queue();
        M32(V(0x206f9126)) += len;
        return;
    }
    if (packed) {
        eax = esi + 0xa;
        unpack_position();
    } else {
        variable_at(esi + 0xa);
        eax = ebx;
    }
    object_relative();
    if (M16(esi + 4) != 0) {
        object_distance();
        esi = M32(V(0x206f9126));
        edi = G;
        if (M8(edi + 0xd4f) != 0)
            goto target;
        SET_AX(M16(esi + 4));
        if (AX < M16(edi + 0xd4d))
            goto target;
    }
    SET_AX(M16(esi + 6));
    if (AX != 0) {
        sphere_in_eye();
        if (AX == 0)
            goto target;
    }
    M32(V(0x206f9126)) += len;
    return;
target:
    esi = M32(V(0x206f9126));
    SET_AX(M16(esi + 2));
    eax = (uint32_t)(int32_t)(int16_t)AX;
    M32(V(0x206f9126)) += eax;
}

static void op_object_check_var(void) { object_check(0); }
static void op_object_check_packed(void) { object_check(1); }


/* ---- the depth queue: .text+0x4d60..+0x5253 ------------------------------------------ */

/* The queue (0x206fd87c, made by queue_alloc()): +0 its capacity, +4 its
 * count, +8 the sorted pointers (at +0x14), +0xc the entries after them,
 * +0x10 the next free entry.  An entry, 0x2c bytes: +0 the model, +4 its
 * distance (the sort key), +8 the context it runs in, +0xe/+0x16/+0x1e
 * the object's 48-bit offsets, +0x24 0x206f919c, +0x28 the address of the
 * one-shot word its first instruction keeps. */

/* 0x206e5e1d -- where distance eax goes among the sorted pointers: a
 * binary search, esi the slot (an equal one, or where it would go). */
static void queue_search(void)
{
    edi = M32(V(0x206fd87c));
    esi = M32(edi + 8);
    ecx = M32(edi + 4);
    ecx <<= 2;
    ecx += esi;
    while (esi < ecx) {
        ebx = ecx;
        ebx -= esi;
        ebx >>= 3;
        ebx <<= 2;
        ebx += esi;
        edi = M32(ebx);
        if (eax == M32(edi + 4)) {
            esi = ebx;
            return;
        }
        if (eax > M32(edi + 4))
            esi = ebx + 4;
        else
            ecx = ebx;
    }
    esi = ecx;
}

/* 0x206e5e56 -- an entry (edi) for the model at esi: its first instruction,
 * after a context (0x89, which replaces the entry's), must be an object
 * (0x3a, 0x4c, 0x3c, 0x2f, 0x77; 0x77's altitude dword written first).  The
 * object made relative, its offsets and distance kept, then its range and
 * sphere tested; 1 when it is not to be queued.  0x206f91b7 is kept round
 * it.
 *
 * Retail's answer to any other first instruction (0x206e5eae) leaves the
 * instruction's address pushed: it pops that into 0x206f91b7 and returns
 * to the old context as a code address.  Here it pops both and answers 1,
 * a deliberate fix of a crash (docs/modules/g3d.md). */
static int queue_prepare(void)
{
    uint32_t keep_ctx, keep_esi;

    esi += 2;
    SET_BX(M16(esi));
    keep_ctx = M32(V(0x206f91b7));
    if (BX == 0x89) {
        eax = M32(esi + 2);
        M32(V(0x206f91b7)) = eax;
        esi += 6;
        SET_BX(M16(esi));
    } else {
        eax = M32(edi + 8);
        M32(V(0x206f91b7)) = eax;
    }
    keep_esi = esi;
    SET_AX(M16(esi + 0xa));
    if (BX == 0x3a)
        goto variable;
    SET_AX(M16(esi + 0xe));
    if (BX == 0x4c)
        goto variable;
    eax = esi + 0xa;
    if (BX == 0x3c)
        goto packed;
    eax = esi + 0xe;
    if (BX == 0x2f)
        goto packed;
    if (BX != 0x77) {
        M32(V(0x206f91b7)) = keep_ctx;
        return 1;
    }
    M32(esi + 0x1a) = M32(V(0x206f91d9));
packed:
    unpack_position();
    goto relative;
variable:
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
    eax = ebx;
relative:
    {
        uint32_t e = edi;

        object_relative();
        edi = e;
    }
    esi = G;
    SET_AX(M16(esi + 0x2fe));
    M16(edi + 0xe) = AX;
    eax = M32(esi + 0x300);
    M32(edi + 0x10) = eax;
    SET_AX(M16(esi + 0x30a));
    M16(edi + 0x16) = AX;
    eax = M32(esi + 0x30c);
    M32(edi + 0x18) = eax;
    SET_AX(M16(esi + 0x316));
    M16(edi + 0x1e) = AX;
    eax = M32(esi + 0x318);
    M32(edi + 0x20) = eax;
    {
        uint32_t e = edi;

        object_distance();
        edi = e;
    }
    M32(edi + 4) = eax;
    esi = keep_esi;
    SET_BX(M16(esi + 4));
    if (BX != 0) {
        if (eax > 0xffffff)
            goto fail;
        edx = (edx << 24) | (eax >> 8);         /* shld edx,eax,0x18 */
        SET_CX(DX);
        if (BX < CX)
            goto fail;
    }
    SET_BX(M16(esi + 6));
    if (BX != 0) {
        uint32_t a = eax, d = edx, si = esi, di = edi, bp = ebp;
        uint16_t in;

        SET_AX(BX);
        sphere_in_eye();
        in = AX;
        ebp = bp, edi = di, esi = si, edx = d, eax = a;
        if (in == 0)
            goto fail;
    }
    ebx = esi + 8;
    M32(edi + 0x28) = ebx;
    M32(V(0x206f91b7)) = keep_ctx;
    return 0;
fail:
    M32(V(0x206f91b7)) = keep_ctx;
    return 1;
}

/* 0x206e5d60 -- +0x38: queue the 'v' model at `model` (its key the
 * context it runs in), sorted by distance.  The queue holds up to the
 * table at 0x206fd864's entry for GLOBAL +0xb60 (unclamped here), at most
 * its capacity; full, the farthest entry is dropped and its place reused.
 * An entry that is not to be queued gives back the newest place -- the
 * next free one, even when it reused the farthest's. */
static void queue_model(uint32_t model, uint32_t key)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi, keep_ebp = ebp;
    uint32_t queue, entry;

    if (M32(V(0x206fd87c)) == 0)
        goto out;
    eax = model;
    if (M16(eax) != 0x76)
        goto out;
    edi = G;
    ebx = M32(edi + 0xb60);
    edi = M32(V(0x206fd87c));
    edx = M32(V(0x206fd864) + ebx * 4);
    if (edx > M32(edi))
        edx = M32(edi);
    M32(V(0x206fd878)) = edx;
    ebx = M32(edi + 0x10);
    if (edx <= M32(edi + 4)) {
        edx--;
        M32(edi + 4) -= 1;
        ebx = M32(edi + 8);
        ebx = M32(ebx + edx * 4);
    } else {
        M32(edi + 0x10) += 0x2c;
    }
    eax = model;
    M32(ebx) = eax;
    eax = key;
    M32(ebx + 8) = eax;
    eax = M32(V(0x206f919c));
    M32(ebx + 0x24) = eax;
    queue = edi, entry = ebx;
    esi = model;
    edi = ebx;
    if (queue_prepare()) {
        ebx = entry, edi = queue;
        M32(edi + 0x10) -= 0x2c;
        goto out;
    }
    ebx = entry, edi = queue;
    queue_search();
    ebx = entry, edi = queue;
    eax = M32(edi + 4);
    ecx = M32(edi + 8);
    eax <<= 2;
    ecx += eax;
    edi = ecx;
    ecx -= esi;
    ecx = sar32(ecx, 2);
    if (ecx != 0) {                             /* std / rep movsd */
        esi = edi - 4;
        do {
            M32(edi) = M32(esi);
            edi -= 4;
            esi -= 4;
        } while (--ecx != 0);
    }
    M32(edi) = ebx;
    edi = queue;
    M32(edi + 4) += 1;
out:
    ebp = keep_ebp, edi = keep_edi, esi = keep_esi, ebx = keep_ebx;
}

SERVICE(queue_model_svc, 8);
static void queue_model_svc_body(const uint32_t *arg)
{
    queue_model(arg[0], arg[1]);
}

/* 0x206e5f91 -- the first ecx entries, farthest first, each whose altitude
 * offset's integer dword (+0x18) is above 0x206f91a3 and not above
 * 0x206f91a7: its offset in the queue written to its one-shot word, its
 * 0x206f919c and context set, the scale word cleared, and the model run
 * (+0x1c), counted in 0x206fd884. */
static void queue_drain(void)
{
    esi = ecx;
    esi <<= 2;
    esi += M32(edi + 8);
    edi = ecx;
    do {
        esi -= 4;
        edx = M32(esi);
        eax = M32(edx + 0x18);
        if ((int32_t)eax > (int32_t)M32(V(0x206f91a3)) &&
            (int32_t)eax <= (int32_t)M32(V(0x206f91a7))) {
            uint32_t b = ebx, si = esi, di = edi, bp = ebp;

            eax = edx;
            eax -= M32(V(0x206fd87c));
            ebx = M32(edx + 0x28);
            M16(ebx) = AX;
            eax = M32(edx + 0x24);
            M32(V(0x206f919c)) = eax;
            ebx = M32(edx + 8);
            eax = M32(edx);
            M32(V(0x206fd884)) += 1;
            M16(V(0x206f9159)) = 0;
            M32(V(0x206f91b7)) = ebx;
            run_program(eax);
            ebp = bp, edi = di, esi = si, ebx = b;
        }
    } while (--edi != 0);
    M32(V(0x206f919c)) = 0;
}

/* 0x206e600f -- +0x34: draw the queued entries whose altitude offsets are
 * in (nearest, farthest] -- all but the farthest when the queue is at its limit --
 * with 0x206f917c set, so their objects come from the queue; emptied after
 * when `reset`.  Returns how many were drawn. */
static uint32_t queue_draw(uint32_t farthest, uint32_t nearest, uint32_t reset)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi, keep_ebp = ebp;

    M32(V(0x206fd884)) = 0;
    M32(V(0x206f91a7)) = farthest;
    M32(V(0x206f91a3)) = nearest;
    M16(V(0x206f917c)) = 1;
    edi = G;
    ebx = M32(edi + 0xb60);
    ecx = M32(V(0x206fd864) + ebx * 4);
    edi = M32(V(0x206fd87c));
    if (ecx > M32(edi))
        ecx = M32(edi);
    M32(V(0x206fd878)) = ecx;
    ecx = M32(edi + 4);
    if (ecx >= M32(V(0x206fd878)))
        ecx--;
    if (ecx != 0)
        queue_drain();
    ecx = reset;
    if (ecx != 0)
        queue_alloc();
    M16(V(0x206f917c)) = 0;
    eax = M32(V(0x206fd884));
    ebp = keep_ebp, edi = keep_edi, esi = keep_esi, ebx = keep_ebx;
    return eax;
}

SERVICE(queue_draw_svc, 12);
static void queue_draw_svc_body(const uint32_t *arg)
{
    queue_draw(arg[0], arg[1], arg[2]);
}

/* 0x206e5ff7 -- +0x30: the whole queue, emptied after. */
SERVICE(queue_draw_all, 0);
static void queue_draw_all_body(const uint32_t *arg)
{
    (void)arg;
    queue_draw(0x7fffffff, 0x80000001, 1);
}

/* 0x206e61ef -- queue the model at eax in the second queue (0x206fd880,
 * made on first use), swapped in for it. */
static void queue_second(void)
{
    uint32_t keep = M32(V(0x206fd87c));

    edx = M32(V(0x206fd880));
    M32(V(0x206fd87c)) = edx;
    if (edx == 0) {
        uint32_t a = eax;

        queue_alloc();
        eax = a;
    }
    queue_model(eax, 0);
    eax = M32(V(0x206fd87c));
    M32(V(0x206fd880)) = eax;
    M32(V(0x206fd87c)) = keep;
}

/* 0x206e4259 / 0x206e4279 -- 0x32 and 0x2b, queue the model at a signed
 * word (0x32) or dword (0x2b) offset from the opcode, in the current
 * context; past four or six bytes. */
static void op_queue(void)
{
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    queue_model(eax, M32(V(0x206f91b7)));
    M32(V(0x206f9126)) += 4;
}

static void op_queue_far(void)
{
    esi += 2;
    lods32();
    eax += M32(V(0x206f9126));
    queue_model(eax, M32(V(0x206f91b7)));
    M32(V(0x206f9126)) += 6;
}

/* 0x206f1a5b -- 0x75, queue the model at a signed word offset in the
 * second queue; past four bytes. */
static void op_queue_second(void)
{
    eax = (uint32_t)(int32_t)(int16_t)M16(esi + 2);
    eax += esi;
    esi += 4;
    M32(V(0x206f9126)) = esi;
    queue_second();
}

/* 0x206ea861 -- 0x5a: the program at the signed word offset from +2 run
 * nested; when it set 0x206fdc37 the model at the offset from the opcode
 * is queued.  Past four bytes either way. */
static void op_run_and_queue(void)
{
    uint32_t keep;

    M16(V(0x206fdc37)) = 0;
    eax = 0;
    keep = esi;
    esi += 2;
    lods16();
    eax += esi;
    eax -= 2;
    M32(V(0x206f9126)) = eax;
    interpret();
    esi = keep;
    if (M16(V(0x206fdc37)) == 0) {
        esi += 4;                               /* 0x206ea8a8 */
        M32(V(0x206f9126)) = esi;
        return;
    }
    eax = 0;
    esi += 2;
    lods16();
    eax += esi;
    eax -= 4;
    M32(V(0x206f9126)) = esi;
    queue_model(eax, 0);
}


/* ---- shadows: .text+0x6047..+0x61d4, +0x77d1..+0x79b3 -------------------------------- */

/* 0x206e70f5 -- the point (ax, bx, dx) through the sun's matrix (GLOBAL
 * +0xcfa.., 1.15 words), each row a 32-bit sum of 16x16 products doubled,
 * its high word: back in ax, bx, dx (kept at 0x206f912e..). */
static void sun_rotate(void)
{
    static const uint32_t row[3] = { 0xcfa, 0xd00, 0xd06 };
    uint16_t out[3];
    uint32_t i;

    M16(V(0x206f912e)) = AX;
    M16(V(0x206f9130)) = BX;
    M16(V(0x206f9132)) = DX;
    edi = G;
    for (i = 0; i < 3; i++) {
        int32_t t;
        uint32_t sum;

        t = (int32_t)(int16_t)M16(V(0x206f912e)) * (int16_t)M16(edi + row[i]);
        SET_BX(t);
        SET_CX((uint32_t)t >> 16);
        t = (int32_t)(int16_t)M16(V(0x206f9130)) * (int16_t)M16(edi + row[i] + 2);
        sum = ((uint32_t)CX << 16 | BX) + (uint32_t)t;
        SET_BX(sum);
        SET_CX(sum >> 16);
        t = (int32_t)(int16_t)M16(V(0x206f9132)) * (int16_t)M16(edi + row[i] + 4);
        sum = ((uint32_t)CX << 16 | BX) + (uint32_t)t;
        sum <<= 1;
        SET_AX(sum);
        SET_DX(sum >> 16);
        out[i] = DX;
    }
    SET_BX(out[1]);
    SET_AX(out[0]);
}

/* 0x206e897f -- (ax, bx, dx) times the word at 0x206fd8e7, each the middle
 * word of its product ((dx:ax) >> 8). */
static void shadow_scale(void)
{
    uint16_t a = AX;
    int32_t t;

    t = (int32_t)(int16_t)DX * (int16_t)M16(V(0x206fd8e7));
    SET_AX((uint32_t)t >> 8);
    SET_DX((uint32_t)t >> 16);
    SET_BP(AX);
    t = (int32_t)(int16_t)BX * (int16_t)M16(V(0x206fd8e7));
    SET_AX((uint32_t)t >> 8);
    SET_DX((uint32_t)t >> 16);
    SET_BX(AX);
    t = (int32_t)(int16_t)a * (int16_t)M16(V(0x206fd8e7));
    SET_AX((uint32_t)t >> 8);
    SET_DX(BP);
}

/* x - ebx * sun / height, clamped to +-0x7fff, an idiv the host repairs;
 * ebx left the height */
static uint32_t along_sun(uint32_t x, uint32_t sun)
{
    eax = (uint32_t)(int32_t)(int16_t)M16(edi + sun);
    imul32(ebx);
    ebx = (uint32_t)(int32_t)(int16_t)M16(edi + 0xb2e);
    idiv32(ebx);
    x -= eax;
    if ((int32_t)x > 0x7fff)
        x = 0x7fff;
    if ((int32_t)x < -0x7fff)
        x = (uint32_t)-0x7fff;
    return x;
}

/* 0x206e7047 -- in the map, the point (ax, bx, dx) down the sun onto the
 * ground (y 0), through the sun's matrix first when 0x206f91bd is set and
 * shadow_scale() after when 0x206fd8dd is. */
static void onto_ground(void)
{
    uint32_t keep = edi, x, z;

    edi = G;
    if (M16(edi + 0xdca) == 0)
        goto out;
    if (M16(V(0x206f91bd)) != 0)
        sun_rotate();
    ecx = (uint32_t)(int32_t)(int16_t)AX;
    ebp = (uint32_t)(int32_t)(int16_t)DX;
    ebx = (uint32_t)(int32_t)(int16_t)BX;
    {
        uint32_t y = ebx;                       /* push ebx / pop ebx */

        edi = G;
        x = along_sun(ecx, 0xb2a);
        ecx = x;
        ebx = y;
        z = along_sun(ebp, 0xb32);
        ebp = z;
    }
    SET_AX(CX);
    SET_BX(0);
    SET_DX(BP);
    if (M16(V(0x206fd8dd)) != 0)
        shadow_scale();
out:
    edi = keep;
}

/* 0x206e3e4b -- 0x1a, points into the table from an index for a count,
 * onto the ground in the map; each point's shift byte at +0xd. */
static void op_ground_points(void)
{
    esi += 2;
    eax = 0;
    lods16();
    eax <<= 4;
    eax += M32(V(0x206f912a));
    edi = eax;
    lods16();
    SET_CX(AX);
    ecx = CX;
    do {
        uint32_t c = ecx, s = esi;

        SET_AX(M16(esi));
        SET_BX(M16(esi + 2));
        SET_DX(M16(esi + 4));
        onto_ground();
        point_from_words();
        SET_AL(M8(V(0x206f9122)));
        M8(edi + 0xd) = AL;
        esi = s, ecx = c;
        esi += 6;
        edi += 0x10;
    } while (--ecx != 0);
    M32(V(0x206f9126)) = esi;
}

/* 0x206e6e0b -- 0x29, the same with a normal after each point: its shade
 * (the ambient level plus the light over the rest) to the point's +0xe.
 * dec ecx / jne: a count of 0 walks four billion. */
static void op_shaded_points(void)
{
    esi += 2;
    lods16();
    eax = AX;
    eax <<= 4;
    eax += M32(V(0x206f912a));
    edi = eax;
    lods16();
    SET_CX(AX);
    ecx = CX;
    do {
        uint32_t c = ecx, s = esi;
        int32_t t;
        uint32_t sum;

        M8(V(0x206f9122)) = 0;
        SET_AX(M16(esi));
        SET_BX(M16(esi + 2));
        SET_DX(M16(esi + 4));
        onto_ground();
        point_from_words();
        esi = s;
        esi += 6;
        shade();
        SET_BX(0x7fff - M16(V(0x206f91bb)));
        t = (int32_t)(int16_t)AX * (int16_t)BX;
        sum = (uint32_t)t << 1;
        SET_AX(sum);
        SET_DX(sum >> 16);
        SET_DX(DX + M16(V(0x206f91bb)));
        M16(edi + 0xe) = DX;
        ecx = c;
        esi += 6;
        edi += 0x10;
    } while (--ecx != 0);
    M32(V(0x206f9126)) = esi;
}

#define WINDOW_ANGLES_MATRIX(p, b, h, out) (eax = IMP4(WINDOW, 0x2c, p, b, h, out))

/* 0x206e87d1 -- 0x4a, a shadow: with GLOBAL +0x390 at 2 or more, +0x28c
 * set and +0x116e's low bits clear, the sun's matrix (GLOBAL +0xcfa) from
 * the angles in the variable's record (+2 pitch, +6 bank, +0xa heading,
 * also to GLOBAL +0x5ca/+0x5ce/+0x5d2), the sun's direction (+6..) through
 * it to GLOBAL +0xb2a..; then, the sun's y below -0x400, the program at
 * the target run as in the map with 0x206f91bd and 0x206fd8dd up; all of
 * it put back after, GLOBAL +0x3a8 cleared.  Past 0xc bytes. */
static void op_shadow_run(void)
{
    uint16_t keep[7], i;
    static const uint32_t at[7] = { 0x5ca, 0x5ce, 0x5d2, 0xb2a, 0xb2e, 0xb32, 0xb34 };
    uint32_t target, keep_pc;

    edi = G;
    if (M32(edi + 0x390) < 2 || M32(edi + 0x28c) == 0 || (M16(edi + 0x116e) & 3))
        goto on;
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    for (i = 0; i < 7; i++)
        keep[i] = M16(edi + at[i]);
    target = eax;
    lods16();
    variable_at(esi - 2);
    SET_CX(M16(ebx + 2));
    SET_DX(M16(ebx + 0xa));
    SET_BX(M16(ebx + 6));
    M16(edi + 0x5ca) = CX;
    M16(edi + 0x5ce) = BX;
    M16(edi + 0x5d2) = DX;
    {
        uint32_t s = esi;

        eax = edi + 0xcfa;
        WINDOW_ANGLES_MATRIX(CX, BX, DX, eax);
        esi = s;
    }
    SET_AX(M16(esi));
    SET_BX(M16(esi + 2));
    SET_DX(M16(esi + 4));
    sun_rotate();
    edi = G;
    M16(edi + 0xb2a) = AX;
    M16(edi + 0xb2e) = BX;
    M16(edi + 0xb32) = DX;
    eax = target;
    M16(V(0x206f91bd)) += 1;
    M16(edi + 0xdca) += 1;
    M16(V(0x206fd8dd)) += 1;
    if ((int16_t)BX < (int16_t)0xfc00) {
        keep_pc = M32(V(0x206f9126));
        M32(V(0x206f9126)) = eax;
        interpret();
        M32(V(0x206f9126)) = keep_pc;
    }
    edi = G;
    for (i = 7; i-- > 0; )
        M16(edi + at[i]) = keep[i];
    M16(V(0x206fd8dd)) -= 1;
    M16(V(0x206f91bd)) -= 1;
    M16(edi + 0xdca) -= 1;
    M16(edi + 0x3a8) = 0;
on:
    M32(V(0x206f9126)) += 0xc;
}


/* ---- meshes: .text+0x16d0..+0x17ee, +0x3492..+0x359c (compiled C) -------------- */

/* 0x206e4492 -- a mesh's plane (normal, distance at p): all zero draws it
 * as it is; otherwise not when the eye is behind it, and shaded outside
 * the map.  In the map a mesh is drawn only with GLOBAL +0x1298 clear,
 * GLOBAL +0x3c0 0x6800.  1 to draw. */
static int mesh_facing(uint32_t p)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi;
    int draw = 1;

    esi = p;
    eax = 0;
    if (M32(esi) == 0 && M32(esi + 4) == 0 && M16(esi + 8) == 0) {
        edi = G;
        if (M16(edi + 0xdca) != 0)
            goto map;
        goto out;
    }
    plane_facing();
    eax = 0;
    if (sf) {
        draw = 0;
        goto out;
    }
    edi = G;
    if (M16(edi + 0xdca) != 0)
        goto map;
    shade();
    shade_colour();
    goto out;
map:
    if (M32(edi + 0x1298) != 0) {
        draw = 0;
        goto out;
    }
    M32(edi + 0x3c0) = 0x6800;
out:
    eax = draw;
    edi = keep_edi, esi = keep_esi, ebx = keep_ebx;
    return draw;
}

/* 0x206e450a -- a triangle of table points, a surface of its own (the
 * surface handler's counter step taken back). */
static void mesh_triangle(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t keep_ebx = ebx, keep_ebp = ebp, keep_esi = esi, keep_edi = edi;

    op_surface();
    M32(V(0x206f9126)) -= 2;
    eax = a;
    table_first();
    eax = b;
    table_next();
    eax = c;
    table_next();
    polygon_close();
    edi = keep_edi, esi = keep_esi, ebp = keep_ebp, ebx = keep_ebx;
}

/* 0x206e4541 -- a new surface; the next vertex is its first (0x206fd6cc). */
static void mesh_surface(void)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi;

    M32(V(0x206fd6cc)) = 1;
    op_surface();
    M32(V(0x206f9126)) -= 2;
    edi = keep_edi, esi = keep_esi, ebx = keep_ebx;
}

/* 0x206e455e -- a table point as the surface's first vertex or its next. */
static void mesh_vertex(uint32_t index)
{
    uint32_t keep_ebx = ebx, keep_ebp = ebp, keep_esi = esi, keep_edi = edi;

    eax = index;
    if (M32(V(0x206fd6cc)) == 1) {
        table_first();
        M32(V(0x206fd6cc)) = 0;
    } else {
        table_next();
    }
    edi = keep_edi, esi = keep_esi, ebp = keep_ebp, ebx = keep_ebx;
}

/* 0x206e458f */
static void mesh_close(void)
{
    uint32_t keep_ebx = ebx, keep_ebp = ebp, keep_esi = esi, keep_edi = edi;

    polygon_close();
    edi = keep_edi, esi = keep_esi, ebp = keep_ebp, ebx = keep_ebx;
}

/* 0x206e26d0 -- 0x90, a mesh: a byte count (+2), a plane (+4), a base
 * index (+0xe), then index bytes, each plus the base.  With GLOBAL +0x12a4
 * bit 4, a fan of triangles: a byte with its top bit starts a new centre
 * (its low bits) and edge (the byte after); each byte after makes a
 * triangle of the centre, the last vertex and its own, the last vertex
 * then.  Otherwise polygons: a byte with its top bit closes the last and
 * starts another with it and the byte after; each byte after adds a vertex.
 * The counter goes past the bytes, to an even address.
 *
 * Retail keeps the fan's centre in a local the call to the plane's test
 * moves out from under it (its ret 4 shifts the frame by the argument it
 * pops): the centre starts as whatever was in the stack slot, the count's
 * slot is read for the count.  So a fan whose first byte lacks its top bit
 * draws from a centre nothing set -- not reproducible (it starts 0 here),
 * and g3ddiff never generates one. */
static void op_mesh(void)
{
    uint32_t pc = M32(V(0x206f9126)), at = pc + 0xe, count, base, i;
    uint32_t centre = 0, last = 0;

    count = M16(pc + 2);
    if (mesh_facing(pc + 4)) {
        base = M8(at);
        at++;
        if (M8(G + 0x12a4) & 0x10) {
            for (i = 0; (int32_t)i < (int32_t)count; ) {
                uint32_t b = M8(at + i), v;

                if (b & 0x80) {
                    centre = (b & 0x7f) + base;
                    last = M8(at + i + 1) + base;
                    i += 2;
                }
                v = M8(at + i) + base;
                i++;
                mesh_triangle(centre, last, v);
                last = v;
            }
        } else {
            M16(V(0x206fd6c8)) = 0xffff;
            mesh_surface();
            for (i = 0; (int32_t)i < (int32_t)count; ) {
                if (M8(at + i) & 0x80) {
                    if (i != 0) {
                        mesh_close();
                        M16(V(0x206fd6c8)) = 0xffff;
                        mesh_surface();
                    }
                    mesh_vertex((M8(at + i) & 0x7f) + base);
                    i++;
                    mesh_vertex(M8(at + i) + base);
                    i++;
                }
                mesh_vertex(M8(at + i) + base);
                i++;
            }
            mesh_close();
        }
    }
    M8(V(0x206f9125)) = 0;
    M16(G + 0xc90) = 0;
    eax = (count + M32(V(0x206f9126)) + 0x10) & ~1u;
    M32(V(0x206f9126)) = eax;
}


/* ---- lights: .text+0x13ff0..+0x141fa ---------------------------------------------- */

#define HG2D_LIGHT(rec, size, x, y) (eax = IMP4(HG2D, 0x84, rec, size, x, y))

/* 0x206f514f -- the light's distance from the eye, roughly: each axis's
 * 16.16 difference from the eye made positive, the larger of x and z four
 * times over, the three summed and quartered; the integer word in bp. */
static void light_distance(void)
{
    uint16_t keep_bx = BX, keep_cx = CX;
    uint32_t a, b, sum;

    edi = G;
    a = ((uint32_t)M16(esi + 4) << 16) - M32(edi + 0xcdc);
    if ((int32_t)a < 0)
        a = (uint32_t)-(int32_t)a;
    b = ((uint32_t)M16(esi + 8) << 16) - M32(edi + 0xce4);
    if ((int32_t)b < 0)
        b = (uint32_t)-(int32_t)b;
    if ((b >> 16) > (a >> 16))                  /* cmp bp,ax / ja */
        b <<= 2;
    else
        a <<= 2;
    sum = a + b;
    b = ((uint32_t)M16(esi + 6) << 16) - M32(edi + 0xce0);
    if ((int32_t)b < 0)
        b = (uint32_t)-(int32_t)b;
    sum += b;
    sum = (uint32_t)((int32_t)sum >> 2);
    SET_AX(sum >> 16);
    SET_CX(sum);
    SET_BP(AX);
    SET_BX(b);
    SET_CX(keep_cx);
    SET_BX(keep_bx);
}

/* 0x206f4ff0 -- 0x28, a light of a size (the operand's high byte; none
 * below 1): size 1 a point; larger, projected (inside twice its z) and
 * drawn through HG2D +0x84 at 0xc80 over its distance times the size word
 * (the size byte and the byte after it) >> 8, at most 0x60.  The counter
 * kept round it and on past ten bytes. */
static void op_light_point(void)
{
    uint32_t keep_pc = M32(V(0x206f9126));

    SET_AX(M16(esi + 2));
    if (AH < 1)
        goto done;
    M8(V(0x206fd19e)) = AH;
    if (AH == 1) {
        esi += 4;
        edi = V(0x206f90c0);
        point_from_esi();
        point();
        goto done;
    }
    {
        uint32_t keep_esi = esi;

        esi += 4;
        edi = V(0x206f90c0);
        point_from_esi();
        esi = G;
        esi = M32(esi + 0x1198);
        ebx = M32(esi);
        M16(V(0x206fd1bc)) = BX;
        ebx = M32(esi + 4);
        M16(V(0x206fd1b8)) = BX;
        ebx = M32(esi + 8);
        M16(V(0x206fd1be)) = BX;
        ebx = M32(esi + 0xc);
        M16(V(0x206fd1ba)) = BX;
        esi = keep_esi;
    }
    SET_BX(M16(V(0x206f90c2)));
    SET_CX(M16(V(0x206f90c6)));
    SET_DX(M16(V(0x206f90ca)));
    SET_BP(DX << 1);
    SET_AX(BX);
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    SET_AX(AX ^ DX);
    if ((int16_t)AX >= (int16_t)BP)
        goto done;
    SET_AX(CX);
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    SET_AX(AX ^ DX);
    if ((int16_t)AX >= (int16_t)BP)
        goto done;
    SET_BP(BP >> 1);
    SET_BP(BP + 1);
    {
        int32_t t = (int32_t)(int16_t)BX * (int16_t)M16(V(0x206fd1bc));

        SET_AX(t);
        SET_DX((uint32_t)t >> 16);
        idiv16(BP);
        SET_BX(AX + M16(V(0x206fd1b8)));
        t = (int32_t)(int16_t)CX * (int16_t)M16(V(0x206fd1be));
        SET_AX(t);
        SET_DX((uint32_t)t >> 16);
        idiv16(BP);
        SET_CX(AX + M16(V(0x206fd1ba)));
    }
    light_distance();
    {
        uint32_t p;

        SET_AX(0xc80);
        SET_DX(0);
        if (BP < 1)                             /* cmp bp,1 / adc bp,dx */
            SET_BP(BP + 1);
        p = 0xc80 / BP;
        SET_DX(0xc80 % BP);
        SET_AX(p);
        p = (uint32_t)AX * M16(V(0x206fd19e));
        SET_AX(p >> 8);
        SET_DX(p >> 16);
        if ((int16_t)AX > 0x60)
            SET_AX(0x60);
    }
    edi = G;
    edi = M32(edi + 0x1194);
    HG2D_LIGHT(edi, AX, BX, CX);
done:
    M32(V(0x206f9126)) = keep_pc;
    M32(V(0x206f9126)) += 0xa;
}


/* 0x206e2690 -- 0x38: with GLOBAL +0x12a4 bit 4 (meshes), the program
 * after it compiled in place (g3d_mesh.S, 0x206e35b0): the 0x38 becomes a
 * 0x02, and the next instruction it can take -- a surface of table points
 * (0x05, 0x0f, 0x10.., 0x08), a facet (0x3e) or face (0x1d), past any
 * 0x1e, 0x7d or 0x81 -- becomes a 0x90 mesh and nops.  The counter stays,
 * on the nop.  Otherwise the polygon flag 0x206fd6c8 set and on past it. */
uint32_t FS98_STDCALL g3d_compile_mesh(uint32_t program) __asm__("_g3d_compile_mesh@4");

static void op_compile_mesh(void)
{
    eax = G;
    if (M8(eax + 0x12a4) & 0x10) {
        ecx = M32(V(0x206f9126));
        g3d_compile_mesh(ecx);
        M16(V(0x206fd6c8)) = 0;
        return;
    }
    eax = M32(V(0x206f9126));
    M16(V(0x206fd6c8)) = 0xffff;
    eax += 2;
    M32(V(0x206f9126)) = eax;
}


/* ---- transforms: .text+0xdfe0..+0xedf3 ------------------------------------------ */

/* The state a transformed subroutine runs in and gets back: the view
 * matrix, 0x206f9198 (the heading), the eye, the sun, GLOBAL +0x5c8..+0x5d3
 * (the angles).  Retail pushes it on the machine stack (0x206ef162) and
 * pops it after (0x206ef1e0); both clear GLOBAL +0x3a8 and 0x206f9180 and
 * count 0x206f9196 up and down. */
struct transform_state {
    uint32_t matrix[4];
    uint16_t m8, heading;
    uint32_t eye[3], sun[3], angles[3];
};

static void transform_push(struct transform_state *t)
{
    uint32_t i;

    edi = G;
    for (i = 0; i < 4; i++)
        t->matrix[i] = M32(edi + 0xce8 + 4 * i);
    t->m8 = M16(edi + 0xcf8);
    t->heading = M16(V(0x206f9198));
    for (i = 0; i < 3; i++) {
        t->eye[i] = M32(edi + 0xcdc + 4 * i);
        t->sun[i] = M32(edi + 0xb28 + 4 * i);
        t->angles[i] = M32(edi + 0x5c8 + 4 * i);
    }
    M16(edi + 0x3a8) = 0;
    M16(V(0x206f9180)) = 0;
    M16(V(0x206f9196)) += 1;
}

static void transform_pop(const struct transform_state *t)
{
    uint32_t i;

    edi = G;
    for (i = 0; i < 3; i++) {
        M32(edi + 0x5c8 + 4 * i) = t->angles[i];
        M32(edi + 0xb28 + 4 * i) = t->sun[i];
        M32(edi + 0xcdc + 4 * i) = t->eye[i];
    }
    M16(V(0x206f9198)) = t->heading;
    M16(edi + 0xcf8) = t->m8;
    for (i = 0; i < 4; i++)
        M32(edi + 0xce8 + 4 * i) = t->matrix[i];
    M16(edi + 0x3a8) = 0;
    M16(V(0x206f9196)) -= 1;
    M16(V(0x206f9180)) = 0;
}

/* (word) * (dword) << 17, kept as a qword at `at` */
static uint64_t turn_term(uint32_t word_at, uint32_t dword_at, uint32_t at)
{
    int64_t p = (int64_t)(int16_t)M16(V(word_at)) * (int32_t)M32(dword_at);
    uint64_t q = (uint64_t)p << 17;

    eax = (uint32_t)q;
    edx = (uint32_t)(q >> 32);
    M32(V(at)) = eax;
    M32(V(at) + 4) = edx;
    return q;
}

/* A pair of dwords (a, b) turned by cos/sin words: a = hi(c*a -/+ s*b),
 * b = hi(c*b +/- s*a), the qwords in 0x206fd1c0/c8/d0 as retail leaves
 * them.  minus: a gets the minus. */
static void turn_pair(uint32_t a, uint32_t b, uint32_t cos_at, uint32_t sin_at, int minus)
{
    uint64_t c_a, s_a, s_b, c_b;

    c_a = turn_term(cos_at, a, 0x206fd1c0);
    s_a = turn_term(sin_at, a, 0x206fd1d0);
    s_b = turn_term(sin_at, b, 0x206fd1c8);
    c_a = minus ? c_a - s_b : c_a + s_b;
    eax = (uint32_t)c_a;
    edx = (uint32_t)(c_a >> 32);
    M32(a) = edx;
    c_b = turn_term(cos_at, b, 0x206fd1c8);
    c_b = minus ? c_b + s_a : c_b - s_a;
    eax = (uint32_t)c_b;
    edx = (uint32_t)(c_b >> 32);
    M32(b) = edx;
}

/* the high word of (a*b + c*d) * 2, 16x16 products summed in 32 bits;
 * when the doubling overflows, 0x7fff if it came out negative, else
 * 0x8000 */
static uint16_t turn_word(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    uint32_t sum = (uint32_t)((int32_t)(int16_t)a * (int16_t)b) +
                   (uint32_t)((int32_t)(int16_t)c * (int16_t)d);
    uint32_t twice = sum << 1;

    SET_AX(twice);
    SET_DX(twice >> 16);
    if ((sum ^ twice) & 0x80000000u)
        return (int32_t)twice < 0 ? 0x7fff : 0x8000;
    return (uint16_t)(twice >> 16);
}

/* Two rows of the view matrix (three words each, at r and q) turned:
 * r' = c*r + s1*q, q' = s2*r + c*q, word by word, r' kept at
 * 0x206f920d..0x206f9211 until q' is written. */
static void turn_rows(uint32_t r, uint32_t q, uint16_t c, uint16_t s1, uint16_t s2)
{
    uint32_t i;

    for (i = 0; i < 3; i++)
        M16(V(0x206f920d) + 2 * i) = turn_word(c, M16(edi + r + 2 * i), s1, M16(edi + q + 2 * i));
    for (i = 0; i < 3; i++) {
        M16(edi + q + 2 * i) = turn_word(s2, M16(edi + r + 2 * i), c, M16(edi + q + 2 * i));
        M16(edi + r + 2 * i) = M16(V(0x206f920d) + 2 * i);
    }
}

#define UTIL_SINCOS_AT(angle, sin, cos) (eax = IMP3(UTIL, 0x2c, angle, sin, cos))

/* 0x206ef25e -- the eye, the sun (its words at GLOBAL +0xb2a.. as 16.16)
 * and the view matrix turned by GLOBAL +0x5ca (pitch), +0x5ce (bank) and
 * +0x5d2 (heading, also added to 0x206f9198): each angle's sine and cosine
 * through UTIL +0x2c, then heading, pitch and bank in turn for each, each
 * skipped when its angle tests 0 -- as a dword, so the word after it
 * counts too. */
static void transform_turn(void)
{
    edi = G;
    M16(edi + 0xb28) = 0;
    M16(edi + 0xb2c) = 0;
    M16(edi + 0xb30) = 0;
    SET_BX(M16(edi + 0x5ca));
    UTIL_SINCOS_AT(BX, V(0x206f91f7), V(0x206f91fd));
    SET_BX(M16(edi + 0x5ce));
    UTIL_SINCOS_AT(BX, V(0x206f91f3), V(0x206f91f9));
    SET_BX(M16(edi + 0x5d2));
    M16(V(0x206f9198)) += BX;
    UTIL_SINCOS_AT(BX, V(0x206f91f5), V(0x206f91fb));
    if (M32(edi + 0x5d2) != 0)
        turn_pair(edi + 0xcdc, edi + 0xce4, 0x206f91fb, 0x206f91f5, 1);
    if (M32(edi + 0x5ca) != 0)
        turn_pair(edi + 0xce0, edi + 0xce4, 0x206f91fd, 0x206f91f7, 0);
    if (M32(edi + 0x5ce) != 0)
        turn_pair(edi + 0xcdc, edi + 0xce0, 0x206f91f9, 0x206f91f3, 0);
    if (M32(edi + 0x5d2) != 0)
        turn_pair(edi + 0xb28, edi + 0xb30, 0x206f91fb, 0x206f91f5, 1);
    if (M32(edi + 0x5ca) != 0)
        turn_pair(edi + 0xb2c, edi + 0xb30, 0x206f91fd, 0x206f91f7, 0);
    if (M32(edi + 0x5ce) != 0)
        turn_pair(edi + 0xb28, edi + 0xb2c, 0x206f91f9, 0x206f91f3, 0);
    if (M32(edi + 0x5d2) != 0) {
        M16(V(0x206f920b)) = (uint16_t)-M16(V(0x206f91f5));
        turn_rows(0xce8, 0xcf4, M16(V(0x206f91fb)), M16(V(0x206f920b)), M16(V(0x206f91f5)));
    }
    if (M32(edi + 0x5ca) != 0) {
        M16(V(0x206f920b)) = (uint16_t)-M16(V(0x206f91f7));
        turn_rows(0xcee, 0xcf4, M16(V(0x206f91fd)), M16(V(0x206f91f7)), M16(V(0x206f920b)));
    }
    if (M32(edi + 0x5ce) != 0) {
        M16(V(0x206f920b)) = (uint16_t)-M16(V(0x206f91f3));
        turn_rows(0xce8, 0xcee, M16(V(0x206f91f9)), M16(V(0x206f91f3)), M16(V(0x206f920b)));
    }
}

/* A transformed subroutine: the state kept, the counter round it, on past
 * `len` bytes after. */
static void transform_run(uint32_t target, uint32_t len, struct transform_state *t)
{
    uint32_t keep = M32(V(0x206f9126));

    M32(V(0x206f9126)) = target;
    interpret();
    M32(V(0x206f9126)) = keep;
    M32(V(0x206f9126)) += len;
    transform_pop(t);
}

/* 0x206eefe0 -- 0x33, a subroutine (signed word offset) turned by three
 * angles (pitch, bank, heading). */
static void op_turned_call(void)
{
    struct transform_state t;
    uint32_t target;

    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    target = eax;
    transform_push(&t);
    edi = G;
    lods16();
    M16(edi + 0x5ca) = AX;
    lods16();
    M16(edi + 0x5ce) = AX;
    lods16();
    M16(edi + 0x5d2) = AX;
    transform_turn();
    transform_run(target, 0xa, &t);
}

/* 0x206efdf4 -- 0x3b, the same with the angles from the record a variable
 * holds (+2, +6, +0xa); in the map only the heading (pitch and bank cleared
 * as dwords). */
static void op_turned_call_var(void)
{
    struct transform_state t;
    uint32_t target;

    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    target = eax;
    transform_push(&t);
    lods16();
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
    edi = G;
    SET_AX(M16(ebx + 2));
    M16(edi + 0x5ca) = AX;
    SET_AX(M16(ebx + 6));
    M16(edi + 0x5ce) = AX;
    SET_AX(M16(ebx + 0xa));
    M16(edi + 0x5d2) = AX;
    if (M16(edi + 0xdca) != 0) {
        M32(edi + 0x5ce) = 0;
        M32(edi + 0x5ca) = 0;
    }
    transform_turn();
    transform_run(target, 6, &t);
}

static void shadow_object_46(void);

/* an angle word plus, when the variable word after it is not 0, the
 * variable's word */
static void angle_plus(uint32_t at)
{
    lods16();
    M16(edi + at) = AX;
    lods16();
    if (AX != 0) {
        ebx = (uint32_t)(int32_t)(int16_t)AX;
        variable();
        SET_AX(M16(ebx));
        M16(edi + at) += AX;
    }
}

/* 0x206efe9b -- 0x46, a subroutine moved and turned: the eye less three
 * words << (16 - the scale word), then each angle plus a variable's word;
 * turned only when an angle is not 0.  Past 0x16 bytes.  While 0x206f91bd
 * is set (a shadow), 0x206e7dce instead. */
static void op_moved_call(void)
{
    struct transform_state t;
    uint32_t target, i;

    if (M16(V(0x206f91bd)) != 0) {
        shadow_object_46();
        return;
    }
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    target = eax;
    transform_push(&t);
    edi = G;
    SET_CX(M16(V(0x206f9159)));
    SET_CX(-CX);
    SET_CX(CX + 0x10);
    for (i = 0; i < 3; i++) {
        lods16();
        eax = (uint32_t)(int32_t)(int16_t)AX;
        eax = shl32(eax, CL);
        M32(edi + 0xcdc + 4 * i) -= eax;
    }
    angle_plus(0x5ca);
    angle_plus(0x5ce);
    angle_plus(0x5d2);
    SET_AX(M16(edi + 0x5ca) | M16(edi + 0x5ce) | M16(edi + 0x5d2));
    if (AX != 0)
        transform_turn();
    transform_run(target, 0x16, &t);
}


/* ---- shadows of objects: .text+0x647f..+0x6c4b, +0x6dce..+0x76f6 --------------- */

/* the dword at v (sign and magnitude) times the word w, the top 32 of the
 * 48-bit product, signed, doubled (shl ax,1 / rcl dx,1) */
static uint32_t times_signed(uint32_t v, uint16_t w)
{
    uint32_t m = (int32_t)v < 0 ? (uint32_t)-(int32_t)v : v;
    uint16_t wm = (int16_t)w < 0 ? (uint16_t)-w : w;
    uint32_t r = (uint32_t)(((uint64_t)m * wm) >> 16);

    if ((int16_t)((uint16_t)(v >> 16) ^ w) < 0)
        r = (uint32_t)-(int32_t)r;
    return r << 1;
}

/* a pair of the shadow vector's dwords turned: a = c*a -/+ s*b,
 * b = c*b +/- s*a, through 0x206f91ff/9203/9207 as retail keeps them */
static void shadow_pair(uint32_t a, uint32_t b, uint32_t cos_at, uint32_t sin_at, int minus)
{
    uint32_t ca, sa, sb, cb;

    ca = times_signed(M32(V(a)), M16(V(cos_at)));
    M32(V(0x206f91ff)) = ca;
    sa = times_signed(M32(V(a)), M16(V(sin_at)));
    M32(V(0x206f9207)) = sa;
    sb = times_signed(M32(V(b)), M16(V(sin_at)));
    M32(V(0x206f9203)) = sb;
    M32(V(a)) = minus ? ca - sb : ca + sb;
    cb = times_signed(M32(V(b)), M16(V(cos_at)));
    M32(V(0x206f9203)) = cb;
    M32(V(b)) = minus ? cb + sa : cb - sa;
}

/* 0x206e7693 -- the shadow's sun vector (0x206fd8ff, 0x206fd903,
 * 0x206fd907) turned by heading, pitch and bank. */
static void shadow_vector_turn(void)
{
    shadow_pair(0x206fd8ff, 0x206fd907, 0x206f91fb, 0x206f91f5, 1);
    shadow_pair(0x206fd903, 0x206fd907, 0x206f91fd, 0x206f91f7, 0);
    shadow_pair(0x206fd8ff, 0x206fd903, 0x206f91f9, 0x206f91f3, 0);
}

/* the three angles' sines and cosines (pitch, bank, heading) */
static void shadow_angles(void)
{
    edi = G;
    SET_BX(M16(edi + 0x5ca));
    UTIL_SINCOS_AT(BX, V(0x206f91f7), V(0x206f91fd));
    edi = G;
    SET_BX(M16(edi + 0x5ce));
    UTIL_SINCOS_AT(BX, V(0x206f91f3), V(0x206f91f9));
    edi = G;
    SET_BX(M16(edi + 0x5d2));
    UTIL_SINCOS_AT(BX, V(0x206f91f5), V(0x206f91fb));
}

/* 0x206e75d3 -- the angles' sines and cosines, then the vector turned. */
static void shadow_turn(void)
{
    shadow_angles();
    shadow_vector_turn();
}

/* columns a and b of each row of the sun's matrix (GLOBAL +0xcfa, three
 * rows of three words) turned, as turn_rows() turns rows */
static void turn_columns(uint32_t a, uint32_t b, uint16_t c, uint16_t s1, uint16_t s2)
{
    static const uint32_t row[3] = { 0xcfa, 0xd00, 0xd06 };
    uint32_t i;

    for (i = 0; i < 3; i++)
        M16(V(0x206f920d) + 2 * i) = turn_word(c, M16(edi + row[i] + a), s1, M16(edi + row[i] + b));
    for (i = 0; i < 3; i++) {
        M16(edi + row[i] + b) = turn_word(s2, M16(edi + row[i] + a), c, M16(edi + row[i] + b));
        M16(edi + row[i] + a) = M16(V(0x206f920d) + 2 * i);
    }
}

/* 0x206e8030 -- the sun's matrix turned by the angles (each, 0 or not). */
static void shadow_matrix_turn(void)
{
    shadow_angles();
    edi = G;
    M16(V(0x206f920b)) = (uint16_t)-M16(V(0x206f91f5));
    turn_columns(0, 4, M16(V(0x206f91fb)), M16(V(0x206f920b)), M16(V(0x206f91f5)));
    M16(V(0x206f920b)) = (uint16_t)-M16(V(0x206f91f7));
    turn_columns(2, 4, M16(V(0x206f91fd)), M16(V(0x206f91f7)), M16(V(0x206f920b)));
    M16(V(0x206f920b)) = (uint16_t)-M16(V(0x206f91f3));
    turn_columns(0, 2, M16(V(0x206f91f9)), M16(V(0x206f91f3)), M16(V(0x206f920b)));
}

/* 0x206e747f -- 0x41, an object's shadow: with GLOBAL +0x11b4 and +0xb34
 * set and +0xe8a above 0xfa (signed), the sun's matrix from the angles in
 * the variable's record (through WINDOW +0x2c), the shadow's vector turned
 * by them, and the subroutine at the signed word offset run as in the map
 * with 0x206f91bd up; the angles and GLOBAL +0x832 put back, +0x3a8
 * cleared.  Past six bytes. */
static void op_object_shadow(void)
{
    uint16_t keep_5ca, keep_5ce, keep_5d2, keep_832;
    uint32_t target, keep_pc;

    edi = G;
    if (M16(edi + 0x11b4) == 0 || M16(edi + 0xb34) == 0 || (int16_t)M16(edi + 0xe8a) <= 0xfa)
        goto on;
    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    keep_5ca = M16(edi + 0x5ca);
    keep_5ce = M16(edi + 0x5ce);
    keep_5d2 = M16(edi + 0x5d2);
    target = eax;
    lods16();
    ebx = (uint32_t)(int32_t)(int16_t)AX;
    variable();
    SET_CX(M16(ebx + 2));
    SET_DX(M16(ebx + 0xa));
    SET_BX(M16(ebx + 6));
    M16(edi + 0x5ca) = CX;
    M16(edi + 0x5ce) = BX;
    M16(edi + 0x5d2) = DX;
    eax = edi + 0xcfa;
    WINDOW_ANGLES_MATRIX(CX, BX, DX, eax);
    shadow_turn();
    M16(V(0x206f91bd)) += 1;
    edi = G;
    M16(edi + 0xdca) += 1;
    keep_832 = M16(edi + 0x832);
    keep_pc = M32(V(0x206f9126));
    M32(V(0x206f9126)) = target;
    interpret();
    edi = G;
    M32(V(0x206f9126)) = keep_pc;
    M16(edi + 0x832) = keep_832;
    M16(edi + 0x5d2) = keep_5d2;
    M16(edi + 0x5ce) = keep_5ce;
    M16(edi + 0x5ca) = keep_5ca;
    M16(V(0x206f91bd)) -= 1;
    edi = G;
    M16(edi + 0xdca) -= 1;
    M16(edi + 0x3a8) = 0;
on:
    M32(V(0x206f9126)) += 6;
}

/* 0x206e7dce -- 0x46 while 0x206f91bd is set, a shadow's moved and turned
 * subroutine: the move (three words shifted down by the scale word) carried
 * onto the ground (onto_ground()) and taken off the eye's integer words,
 * the angles plus variables' words, the sun's matrix and the shadow's
 * vector turned by them, and the subroutine run with 0x206f91bd one more;
 * the eye, the angles and the sun's matrix put back.  Past 0x16 bytes.
 * (Its branch for 0x206f91bd clear, which would build the sun's matrix
 * through WINDOW +0x2c from whatever cx, bx and dx hold, is not reached --
 * 0x46 comes here only with it set -- and is left out.) */
static void shadow_object_46(void)
{
    uint32_t keep_eye[3], target, keep_pc, i;
    uint16_t keep_angle[3], keep_sun[9];

    esi += 2;
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax += M32(V(0x206f9126));
    edi = G;
    for (i = 0; i < 3; i++) {
        keep_eye[i] = M32(edi + 0xcdc + 4 * i);
        keep_angle[i] = M16(edi + 0x5ca + 4 * i);
    }
    for (i = 0; i < 9; i++)
        keep_sun[i] = M16(edi + 0xcfa + 2 * i);
    target = eax;
    SET_CX(M16(V(0x206f9159)));
    SET_AX(sar16(M16(esi), CL));
    SET_BX(sar16(M16(esi + 2), CL));
    SET_DX(sar16(M16(esi + 4), CL));
    {
        uint32_t s = esi;

        onto_ground();
        esi = s;
    }
    esi += 6;
    edi = G;
    M16(edi + 0xcde) -= AX;
    M16(edi + 0xce2) -= BX;
    M16(edi + 0xce6) -= DX;
    angle_plus(0x5ca);
    angle_plus(0x5ce);
    angle_plus(0x5d2);
    shadow_matrix_turn();
    shadow_vector_turn();
    M16(V(0x206f91bd)) += 1;
    keep_pc = M32(V(0x206f9126));
    M32(V(0x206f9126)) = target;
    interpret();
    M32(V(0x206f9126)) = keep_pc;
    M16(V(0x206f91bd)) -= 1;
    M32(V(0x206f9126)) += 0x16;
    edi = G;
    for (i = 9; i-- > 0; )
        M16(edi + 0xcfa + 2 * i) = keep_sun[i];
    for (i = 3; i-- > 0; ) {
        M16(edi + 0x5ca + 4 * i) = keep_angle[i];
        M32(edi + 0xcdc + 4 * i) = keep_eye[i];
    }
}


/* ---- strips: .text+0x13888..+0x13fed ----------------------------------------------- */

/* A strip -- a taxiway, a road -- is drawn a segment at a time, from the
 * last point (0x20710abe..) to the next (0x20710ac4..), at the half width
 * 0x20710aa4 (0x20710aa6 the next end's), its sign at 0x20710ab2; the
 * light record 0x69 starts it. */

#define UTIL_SQRT32(v) (eax = IMP1(UTIL, 0x1c, v))

/* 0x206e3d04 -- the point (ax, bx, cx) as table point di. */
static void table_point(void)
{
    SET_DX(CX);
    edi = DI;
    edi <<= 4;
    edi += M32(V(0x206f912a));
    point_from_words();
    SET_AL(M8(V(0x206f9122)));
    M8(edi + 0xd) = AL;
}

/* 0x206e187f -- the outcode of the point at edi, five bits by adc as the
 * fast routines' (x, y and z biased by 2^31 and compared unsigned). */
static void table_outcode(void)
{
    SET_AX(fast_outcode(M32(edi), M32(edi + 4), M32(edi + 8)));
    ebx = M32(edi) + 0x80000000u;
    ecx = M32(edi + 4) + 0x80000000u;
    edx = 0x80000000u - M32(edi + 8);
    ebp = M32(edi + 8) - 1 + 0x80000000u;
}

/* 0x206f48c4 -- a rough length of (eax, ebx, ecx): the largest of the
 * magnitudes, half the next, a quarter the least. */
static void rough_length(void)
{
    uint32_t t;

    if ((int32_t)eax < 0) eax = (uint32_t)-(int32_t)eax;
    if ((int32_t)ebx < 0) ebx = (uint32_t)-(int32_t)ebx;
    if ((int32_t)ecx < 0) ecx = (uint32_t)-(int32_t)ecx;
    if (eax < ecx) t = eax, eax = ecx, ecx = t;
    if (eax <= ebx) t = eax, eax = ebx, ebx = t;
    if (ecx <= ebx) t = ecx, ecx = ebx, ebx = t;
    ecx = sar32(ecx, 1);
    ebx = sar32(ebx, 2);
    eax += ecx;
    eax += ebx;
}

/* the square of the rough distance (its integer word) from the eye to the
 * point at words p */
static uint32_t eye_distance_sq(uint32_t p)
{
    edi = G;
    eax = ((uint32_t)(int32_t)(int16_t)M16(V(p)) << 16) - M32(edi + 0xcdc);
    ebx = ((uint32_t)(int32_t)(int16_t)M16(V(p + 2)) << 16) - M32(edi + 0xce0);
    ecx = ((uint32_t)(int32_t)(int16_t)M16(V(p + 4)) << 16) - M32(edi + 0xce4);
    rough_length();
    eax >>= 16;
    {
        uint64_t q = (uint64_t)eax * eax;

        eax = (uint32_t)q;
        edx = (uint32_t)(q >> 32);
    }
    return eax;
}

/* 0x206f4f06 -- a row of lights from table point ax to bx in cx steps
 * (each axis's difference over cx by idiv), the line colour 0xf011: cx - 1
 * points, the ends left out. */
static void edge_lights(void)
{
    edi = G;
    M32(edi + 0x2ec) = 0xf011;
    eax = AX;
    SET_AX(AX << 4);
    eax += M32(V(0x206f912a));
    esi = eax;
    ebx = BX;
    ebx <<= 4;
    ebx += M32(V(0x206f912a));
    ecx = CX;
    eax = M32(ebx) - M32(esi);
    cdq();
    idiv32(ecx);
    M32(V(0x206f90f8)) = eax;
    eax = M32(ebx + 4) - M32(esi + 4);
    cdq();
    idiv32(ecx);
    M32(V(0x206f90fc)) = eax;
    eax = M32(ebx + 8) - M32(esi + 8);
    cdq();
    idiv32(ecx);
    M32(V(0x206f9100)) = eax;
    eax = M32(esi);
    ebx = M32(esi + 4);
    edx = M32(esi + 8);
    M32(V(0x206f90c0)) = eax;
    M32(V(0x206f90c4)) = ebx;
    M32(V(0x206f90c8)) = edx;
    ecx--;
    do {                                        /* dec ecx, then loop: one step */
        uint32_t c;                             /* walks four billion; callers */
                                                /* pass three or more */
        row_next();
        c = ecx;
        point();
        ecx = c;
    } while (--ecx != 0);
}

/* 0x206f48f0 -- a segment of the strip.  Outside the edge-lights mode
 * (0x20710aa2) the line colour from GLOBAL +0x3c0 and the shade from
 * +0xcc8.  Far off (the distance's top word 0x3a98 or more), a line;
 * both ends out of view on one side, nothing; the eye beside the segment
 * (within four half widths of its x and z ranges), near either end, or the
 * width over the nearer end's distance squared at 6 or more, a quad
 * (corners off each end at right angles to it, UTIL +0x3c and +0x2c), with
 * the joint to the last segment's far corners; at 2 to 6 a line; below 2
 * nothing.  Then, unless GLOBAL +0x1172 bit 0, a negative width or GLOBAL
 * +0x3c0 0xf00e: the dashes (the program at 0x20710ab8, one each 32 half
 * widths), or in the edge-lights mode a row of lights along each edge (one
 * each 4 half widths, more than two).  The next point becomes the last. */
static void strip_segment(void)
{
    edi = G;
    if (M16(V(0x20710aa2)) == 0) {
        M32(edi + 0x2ec) = M32(edi + 0x3c0);
        M16(edi + 0xcc2) = M16(edi + 0xcc8);
    }
    eax = 0;
    ebx = 0;
    ecx = M16(edi + 0xd4d);
    if (ecx >= 0x3a98)
        goto line;
    M8(V(0x20710aa0)) = 0xff;
    SET_AX(M16(V(0x20710abe)));
    SET_BX(M16(V(0x20710ac0)));
    SET_CX(M16(V(0x20710ac2)));
    SET_DI(0);
    table_point();
    table_outcode();
    M8(V(0x20710aa0)) &= AL;
    SET_AX(M16(V(0x20710ac4)));
    SET_BX(M16(V(0x20710ac6)));
    SET_CX(M16(V(0x20710ac8)));
    SET_DI(1);
    table_point();
    table_outcode();
    M8(V(0x20710aa0)) &= AL;
    if (M8(V(0x20710aa0)) != 0)
        goto next;
    edi = G;
    {
        int32_t w = (int32_t)(int16_t)M16(V(0x20710aa4)) << 18;
        int32_t lo, hi, t;

        lo = (int32_t)(int16_t)M16(V(0x20710abe)) << 16;
        hi = (int32_t)(int16_t)M16(V(0x20710ac4)) << 16;
        if (lo >= hi) t = lo, lo = hi, hi = t;
        lo -= w, hi += w;
        if ((int32_t)M32(edi + 0xcdc) >= lo && (int32_t)M32(edi + 0xcdc) <= hi) {
            lo = (int32_t)(int16_t)M16(V(0x20710ac2)) << 16;
            hi = (int32_t)(int16_t)M16(V(0x20710ac8)) << 16;
            if (lo >= hi) t = lo, lo = hi, hi = t;
            lo -= w, hi += w;
            if ((int32_t)M32(edi + 0xce4) >= lo && (int32_t)M32(edi + 0xce4) <= hi)
                goto quad;
        }
    }
    if (eye_distance_sq(0x20710abe) < 0x200)
        goto quad;
    M32(V(0x20710ab4)) = eax;
    if (eye_distance_sq(0x20710ac4) < 0x200)
        goto quad;
    if (eax <= M32(V(0x20710ab4)))
        M32(V(0x20710ab4)) = eax;
    {
        uint64_t p, n;

        ebx = (uint32_t)(int32_t)(int16_t)M16(edi + 0xce2);
        eax = M16(V(0x20710aa4));
        p = (uint64_t)eax * ebx;
        eax = (uint32_t)p;
        n = (uint64_t)eax << 16;
        eax = (uint32_t)n;
        edx = (uint32_t)(n >> 32);
        ecx = eax;
        ebx = edx;
        div32(M32(V(0x20710ab4)));
    }
    if (eax >= 6)
        goto quad;
    if (eax >= 2)
        goto line;
    goto next;
line:
    if (M16(V(0x20710aa2)) != 0)
        goto next;
    SET_AX(M16(V(0x20710abe)));
    SET_BX(M16(V(0x20710ac0)));
    SET_CX(M16(V(0x20710ac2)));
    SET_DI(0);
    table_point();
    SET_AX(M16(V(0x20710ac4)));
    SET_BX(M16(V(0x20710ac6)));
    SET_CX(M16(V(0x20710ac8)));
    edi = 1;
    table_point();
    eax = 0;
    ebx = 1;
    table_line();
    goto marks;
quad:
    {
        uint32_t i;

        SET_AX(M16(V(0x20710ac4)) - M16(V(0x20710abe)));
        SET_CX(M16(V(0x20710ac8)) - M16(V(0x20710ac2)));
        eax <<= 16;
        ecx <<= 16;
        UTIL_ATAN2(eax, ecx);
        SET_AX(AX + 0x4000);
        SET_BX(AX);
        UTIL_SINCOS(BX, V(0x206fd1d8), V(0x206fd1da));
        SET_AX(M16(V(0x206fd1d8)));
        SET_CX(M16(V(0x206fd1da)));
        SET_BX(AX);
        M16(V(0x20710aaa)) = (uint16_t)((((int32_t)(int16_t)BX * (int16_t)M16(V(0x20710aa4))) << 1) >> 16);
        M16(V(0x20710aac)) = (uint16_t)((((int32_t)(int16_t)CX * (int16_t)M16(V(0x20710aa4))) << 1) >> 16);
        M16(V(0x20710aae)) = (uint16_t)((((int32_t)(int16_t)BX * (int16_t)M16(V(0x20710aa6))) << 1) >> 16);
        M16(V(0x20710ab0)) = (uint16_t)((((int32_t)(int16_t)CX * (int16_t)M16(V(0x20710aa6))) << 1) >> 16);
        for (i = 0; i < 4; i++) {
            uint32_t at = i < 2 ? 0x20710abe : 0x20710ac4;
            uint32_t off = i < 2 ? 0x20710aaa : 0x20710aae;
            int plus = i == 1 || i == 2;

            SET_AX(M16(V(at)));
            SET_BX(M16(V(at + 2)));
            SET_CX(M16(V(at + 4)));
            SET_AX(plus ? AX + M16(V(off)) : AX - M16(V(off)));
            SET_CX(plus ? CX + M16(V(off + 2)) : CX - M16(V(off + 2)));
            SET_DI(i);
            table_point();
        }
        polygon_count();
        eax = 0;
        table_first();
        eax = 1;
        table_next();
        eax = 2;
        table_next();
        eax = 3;
        table_next();
        polygon_close();
        if (M16(V(0x20710aa8)) != 0) {
            polygon_count();
            eax = 4;
            table_first();
            eax = 5;
            table_next();
            eax = 0;
            table_next();
            eax = 1;
            table_next();
            polygon_close();
        }
        M16(V(0x20710aa8)) = 1;
        esi = M32(V(0x206f912a));
        edi = esi;
        esi += 0x20;
        edi += 0x40;
        for (i = 0; i < 8; i++)
            M32(edi + 4 * i) = M32(esi + 4 * i);
        esi += 0x20;
        edi += 0x20;
        ecx = 0;
    }
marks:
    edi = G;
    if ((M16(edi + 0x1172) & 1) || M16(V(0x20710ab2)) != 0 || M32(edi + 0x3c0) == 0xf00e)
        goto next;
    {
        int32_t dx_ = (int16_t)(M16(V(0x20710abe)) - M16(V(0x20710ac4)));
        int32_t dz_ = (int16_t)(M16(V(0x20710ac2)) - M16(V(0x20710ac8)));
        uint32_t sum = (uint32_t)(dx_ * dx_) + (uint32_t)(dz_ * dz_);

        edx = sum;
        UTIL_SQRT32(edx);
        SET_DX(0);
        if (M16(V(0x20710aa2)) == 0) {
            uint32_t q;

            SET_CX(5);
            SET_BX(M16(V(0x20710aa4)) << CL);
            q = (uint32_t)AX;
            if (BX == 0) {
                SET_AX(0xffff);
                SET_DX(0xffff);
            } else {
                SET_DX(q % BX);
                SET_AX(q / BX);
            }
            SET_AX(AX + 1);
            M16(V(0x20710aca)) = AX;
            {
                uint32_t keep = M32(V(0x206f9126));

                M32(V(0x206f9126)) = V(0x20710ab8);
                interpret();
                M32(V(0x206f9126)) = keep;
            }
        } else {
            uint32_t q;

            SET_CX(2);
            SET_BX(M16(V(0x20710aa4)) << CL);
            q = (uint32_t)AX;
            if (BX == 0) {
                SET_AX(0xffff);
                SET_DX(0xffff);
            } else {
                SET_DX(q % BX);
                SET_AX(q / BX);
            }
            SET_AX(AX + 1);
            SET_CX(AX);
            if (CX > 2) {
                uint16_t c = CX;

                SET_AX(1);
                SET_BX(2);
                edge_lights();
                SET_CX(c);
                SET_AX(0);
                SET_BX(3);
                edge_lights();
            }
        }
    }
next:
    M16(V(0x20710abe)) = M16(V(0x20710ac4));
    M16(V(0x20710ac0)) = M16(V(0x20710ac6));
    M16(V(0x20710ac2)) = M16(V(0x20710ac8));
}

/* 0x206f4888 -- 0x6a and 0x6c, the strip's next point at the same width. */
static void op_strip_next(void)
{
    SET_AX(M16(V(0x20710aa4)));
    M16(V(0x20710aa6)) = AX;
    M16(V(0x20710ac4)) = M16(esi + 2);
    M16(V(0x20710ac6)) = M16(esi + 4);
    M16(V(0x20710ac8)) = M16(esi + 6);
    esi += 8;
    M32(V(0x206f9126)) = esi;
    strip_segment();
}

/* 0x206f4ede -- 0x6f, the same as edge lights (the line colour 0xf011). */
static void op_strip_lights(void)
{
    edi = G;
    M32(edi + 0x2ec) = 0xf011;
    M16(V(0x20710aa2)) = 1;
    op_strip_next();
    M16(V(0x20710aa2)) = 0;
}

/* 0x206f4fb4 -- 0x78 and 0x79, the next point with its own width. */
static void op_strip_width(void)
{
    M16(V(0x20710aa6)) = M16(esi + 2);
    M16(V(0x20710ac4)) = M16(esi + 4);
    M16(V(0x20710ac6)) = M16(esi + 6);
    M16(V(0x20710ac8)) = M16(esi + 8);
    esi += 0xa;
    M32(V(0x206f9126)) = esi;
    strip_segment();
}


/* ---- textures by name: .text+0xf343..+0xf75d ------------------------------------- */

#define HG2D_LOAD_TEXTURE(name, context, kind) (eax = IMP3(HG2D, 0xa8, name, context, kind))
#define WINDOW_COFACTORS() (eax = IMP0(WINDOW, 0x20))

/* repz cmpsb over 13 bytes: the count left (0 also when only the last
 * byte differs), and whether all were equal */
static uint32_t compare13(uint32_t a, uint32_t b, int *equal)
{
    uint32_t n = 0xd;

    *equal = 1;
    while (n != 0) {
        n--;
        if (M8(a) != M8(b)) {
            *equal = 0;
            break;
        }
        a++, b++;
    }
    return n;
}

/* 0x206f0712 -- the texture's offsets (the words at esi+4.., << 16) to
 * GLOBAL +0xc94/+0xc98/+0xc9c, and its window (+0xca4..+0xcb0) opened
 * wide. */
static void texture_offsets(void)
{
    edi = G;
    SET_AX(M16(esi + 4));
    eax <<= 16;
    M32(edi + 0xc94) = eax;
    SET_AX(M16(esi + 6));
    eax <<= 16;
    M32(edi + 0xc98) = eax;
    SET_AX(M16(esi + 8));
    eax <<= 16;
    M32(edi + 0xc9c) = eax;
    eax = 0x80000000u;
    M32(edi + 0xca4) = eax;
    M32(edi + 0xca8) = eax;
    eax--;
    M32(edi + 0xcac) = eax;
    M32(edi + 0xcb0) = eax;
}

/* 0x206f05cb -- the slot at ebx loaded afresh: its old texture unloaded,
 * HG2D +0xa8 given its name, context and kind (+0x18); a failure flags it
 * (+0xd bit 1).  1 on failure. */
static int texture_reload(void)
{
    uint32_t keep_eax = eax, keep_ebx = ebx;
    int fail;

    esi = ebx;
    M32(V(0x206ff8ba)) = esi;
    eax = M32(ebx + 0xe);
    M32(V(0x206ff8be)) = eax;
    M16(V(0x206ff8c2)) = 0xffff;
    eax = M32(ebx + 0x14);
    if (eax != 0)
        HG2D_UNLOAD(eax);
    eax = M32(ebx + 0x18);
    M32(V(0x206ff8a0)) = eax;
    HG2D_LOAD_TEXTURE(M32(V(0x206ff8ba)), M32(V(0x206ff8be)), M32(V(0x206ff8a0)));
    M32(ebx + 0x14) = eax;
    if (eax == 0) {
        ebx = keep_ebx;
        M8(ebx + 0xd) |= 2;
        eax = 0;
        { uint32_t t = M32(ebx + 0x14); M32(ebx + 0x14) = eax; eax = t; }
        HG2D_UNLOAD(eax);                       /* 0x206f0820 */
        fail = 1;
    } else {
        fail = 0;
    }
    ebx = keep_ebx, eax = keep_eax;
    return fail;
}

/* 0x206f0502 -- the slot for the name at esi+0xa in this context
 * (0x206f919c): one that has it, or a free one, or the one aged most --
 * unless even that is younger than this one's age (the operand's top two
 * bits, or the distance >> 8 when they are 0), which fails.  A slot taken
 * is named, aged 0xffff and reloaded when it held a texture (a fresh slot
 * is not loaded here).  ebx the slot's offset in the table; 1 on failure. */
static int texture_slot(void)
{
    uint32_t keep_esi = esi;
    int equal, fail = 0;

    SET_BP(M16(esi + 2) >> 14);
    if (BP == 0) {
        edi = G;
        ebp = M32(edi + 0xd4c) >> 8;
    }
    M16(V(0x206ff8b8)) = BP;
    edi = esi + 0xa;
    ebx = M32(V(0x206f919c));
    ebp = M32(V(0x206ff8b4));
    esi = ebp + 6;
    SET_DX(M16(ebp));
    do {
        compare13(esi, edi, &equal);
        if (equal && ebx == M32(esi + 0xe)) {
            ebx = esi;
            goto done;
        }
        esi += M32(V(0x206ff8b0));
        SET_DX(DX - 1);
    } while (DX != 0);
    ebx = ebp + 6;
    SET_CX(M16(ebp));
    SET_AX(0);
    SET_DX(0);
    edx &= 0xffff0000u;
    do {
        if (M8(ebx) == 0)
            goto take;
        if (AX <= M16(ebx + 0x12)) {
            SET_AX(M16(ebx + 0x12));
            edx = ebx;
        }
        ebx += M32(V(0x206ff8b0));
        SET_CX(CX - 1);
    } while (CX != 0);
    if (AX < M16(V(0x206ff8b8))) {
        fail = 1;
        goto done;
    }
    ebx = edx;
take:
    eax = M32(V(0x206f919c));
    M32(ebx + 0xe) = eax;
    M16(ebx + 0x12) = 0xffff;
    M8(ebx + 0xd) = 0;
    esi = ebx;
    {
        int32_t i;

        for (i = 0xc; i >= 0; i--)
            M8(esi + i) = M8(edi + i);
    }
    ebx = esi;
    eax = M32(ebx + 0x14);
    if (eax != 0)
        fail = texture_reload();
done:
    esi = keep_esi;
    ebx -= M32(V(0x206ff8b4));
    return fail;
}

int FS98_STDCALL CharLowerA_ptr(uint32_t) __asm__("_CharLowerA@4");

/* 0x206f0343 -- 0x18, a textured surface by texture name (+0xa, lower-cased
 * in the program): with GLOBAL +0x12a4 bit 0 "red " and "side" read as
 * "side2" while it looks.  Not in the map, nor (GLOBAL +0x11bc clear) for
 * the name at 0x206ff8c6 -- 13 bytes compared, a mismatch in the last
 * counting as a match.  The slot cached in the operand (its low 14 bits)
 * is used when it still has the name and context, else texture_slot();
 * the slot's kind (+0x18) and age (+0x12, the lower) from the operand's top
 * bits or the distance; with a texture loaded, GLOBAL +0xc8c from it, +0xc92
 * from +0xcb4, WINDOW +0x20 once a frame (GLOBAL +0x3a8), the offsets.
 * GLOBAL +0xc90 and +0xcb6 1 then, 0 otherwise; on into a surface past
 * 0x16 bytes. */
static void op_texture(void)
{
    uint32_t name0, name1;
    int equal;

    CharLowerA_ptr(esi + 0xa);
    name0 = M32(esi + 0xa);
    name1 = M32(esi + 0xe);
    edi = G;
    if (M32(edi + 0x12a4) & 1) {
        if (M32(esi + 0xa) == 0x20646572 || M32(esi + 0xa) == 0x65646973) {
            M32(esi + 0xa) = 0x65646973;
            M32(esi + 0xe) = 0x20202032;
        }
    }
    edi = G;
    if (M16(edi + 0xdca) != 0)
        goto none;
    if (M16(edi + 0x11bc) == 0) {
        if (compare13(V(0x206ff8c6), esi + 0xa, &equal) == 0)
            goto none;
    }
    ebx = G;
    M16(ebx + 0xca0) = 0;
    SET_BX(M16(esi + 2));
    ebx &= 0x3fff;
    if (BX >= 6) {
        ebp = M32(V(0x206ff8b4));
        if (BX < M16(ebp + 4)) {
            compare13(ebp + ebx, esi + 0xa, &equal);
            if (equal && M32(V(0x206f919c)) == M32(ebp + ebx + 0xe))
                goto cached;
        }
    }
    if (texture_slot())
        goto none;
cached:
    SET_AX(M16(esi + 2) & 0xc000);
    SET_DX(AX);
    SET_AX(AX | BX);
    M16(esi + 2) = AX;
    SET_DX(DX >> 14);
    if (DX == 0) {
        edi = G;
        edx = M32(edi + 0xd4c) >> 8;
    }
    edi = M32(V(0x206ff8b4));
    eax = AX;
    eax >>= 14;
    M32(ebx + edi + 0x18) = eax;
    if (DX < M16(ebx + edi + 0x12))
        M16(ebx + edi + 0x12) = DX;
    eax = M32(ebx + edi + 0x14);
    if (eax == 0)
        goto none;
    edi = G;
    M32(edi + 0xc8c) = eax;
    M16(edi + 0xc92) = M16(edi + 0xcb4);
    if (M16(edi + 0x3a8) == 0) {
        uint32_t s = esi;

        WINDOW_COFACTORS();
        esi = s;
        edi = G;
        M16(edi + 0x3a8) += 1;
    }
    texture_offsets();
    SET_AX(1);
    goto end;
none:
    SET_AX(0);
end:
    edi = G;
    M16(edi + 0xc90) = AX;
    M16(edi + 0xcb6) = AX;
    M32(esi + 0xe) = name1;
    M32(esi + 0xa) = name0;
    esi += 0x16;
    M32(V(0x206f9126)) = esi;
    op_surface();
}

/* 0x206f0672 -- 0x5d, a textured surface with the texture there is: GLOBAL
 * +0xc92 and +0xc90 from +0xcb4 and +0xcb6, the offsets from its three
 * words; on into a surface past six bytes. */
static void op_texture_again(void)
{
    edi = G;
    M16(edi + 0xc92) = M16(edi + 0xcb4);
    M16(edi + 0xc90) = M16(edi + 0xcb6);
    esi -= 2;
    texture_offsets();
    esi += 8;
    M32(V(0x206f9126)) = esi;
    op_surface();
}


/* ---- grids: .text+0x7d73..+0x87d5 ---------------------------------------------------- */

static void fast_triangle(uint16_t depth, const struct fast_vertex *v);

/* 0x206e8d73 -- every cell of the grid (0x206fd922 columns, 0x206fd924
 * rows) within 0x206fd92a/0x206fd92c cells of the eye's, the far rows first
 * and each row's far cells first, the eye's row and column last, through
 * the routine at 0x206fd910 (bx the column, cx the row).  Its variables are
 * kept round it. */
static void grid_cells(uint32_t routine_at)
{
    uint32_t keep_cb = M32(V(0x206fd910));
    uint16_t keep[11];
    static const uint32_t at[11] = { 0x206fd914, 0x206fd916, 0x206fd918, 0x206fd91a, 0x206fd91c,
                                     0x206fd91e, 0x206fd920, 0x206fdbfe, 0x206fdc00, 0x206fdc02,
                                     0x206fdc04 };
    uint32_t i;

    for (i = 0; i < 11; i++)
        keep[i] = M16(V(at[i]));
    M32(V(0x206fd910)) = routine_at;
    M16(V(0x206fd916)) = M16(V(0x206fd922)) - 1;
    M16(V(0x206fd918)) = M16(V(0x206fd924)) - 1;
    edi = G;
    SET_AX(M16(edi + 0xcde) - M16(V(0x206f91ef)));
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    idiv16(M16(V(0x206fd926)));
    M16(V(0x206fd91e)) = AX;
    SET_AX(M16(edi + 0xce6) - M16(V(0x206f91f1)));
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    idiv16(M16(V(0x206fd928)));
    M16(V(0x206fd920)) = AX;
#define BOUND(to, a, b, plus, top)                                            \
    M16(V(to)) = 0;                                                           \
    SET_AX(plus ? M16(V(a)) + M16(V(b)) : M16(V(a)) - M16(V(b)));            \
    if (!(AX & 0x8000)) {                                                     \
        if ((int16_t)AX > (int16_t)M16(V(top)))                               \
            SET_AX(M16(V(top)));                                              \
        M16(V(to)) = AX;                                                      \
    }
    BOUND(0x206fdc00, 0x206fd91e, 0x206fd92a, 1, 0x206fd916)
    BOUND(0x206fdbfe, 0x206fd91e, 0x206fd92a, 0, 0x206fd916)
    BOUND(0x206fdc04, 0x206fd920, 0x206fd92c, 1, 0x206fd918)
    BOUND(0x206fdc02, 0x206fd920, 0x206fd92c, 0, 0x206fd918)
#undef BOUND
#define EYE_CELL(to, v, top, count)                                           \
    M16(V(to)) = 0;                                                           \
    SET_AX(M16(V(v)));                                                        \
    if ((int16_t)AX >= 0) {                                                   \
        M16(V(to)) = M16(V(top));                                             \
        if ((int16_t)AX < (int16_t)M16(V(count)))                             \
            M16(V(to)) = AX;                                                  \
    }
    EYE_CELL(0x206fd91a, 0x206fd91e, 0x206fd916, 0x206fd922)
    EYE_CELL(0x206fd91c, 0x206fd920, 0x206fd918, 0x206fd924)
#undef EYE_CELL
#define CELL(col, row)                                                        \
    { uint16_t b_ = (col), c_ = (row);                                        \
      SET_BX(b_); SET_CX(c_); routine(M32(V(0x206fd910)))();                  \
      SET_BX(b_); SET_CX(c_); }
#define ROW(row)                                                              \
    {                                                                         \
        int16_t c2;                                                           \
                                                                              \
        for (c2 = (int16_t)M16(V(0x206fdbfe)); c2 < (int16_t)M16(V(0x206fd91a)); c2++) \
            CELL((uint16_t)c2, (row))                                         \
        for (c2 = (int16_t)M16(V(0x206fdc00)); c2 > (int16_t)M16(V(0x206fd91a)); c2--) \
            CELL((uint16_t)c2, (row))                                         \
        CELL(M16(V(0x206fd91a)), (row))                                       \
    }
    {
        int16_t r;

        for (r = (int16_t)M16(V(0x206fdc04)); r > (int16_t)M16(V(0x206fd91c)); r--)
            ROW((uint16_t)r)
        for (r = (int16_t)M16(V(0x206fdc02)); r < (int16_t)M16(V(0x206fd91c)); r++)
            ROW((uint16_t)r)
        ROW(M16(V(0x206fd91c)))
    }
#undef ROW
#undef CELL
    for (i = 11; i-- > 0; )
        M16(V(at[i])) = keep[i];
    M32(V(0x206fd910)) = keep_cb;
}

/* 0x206e9706 -- which side of the triangle (corner esi, ebx, edi, heights
 * at +0xc, the cell's sizes 0x206fd926/0x206fd928 and origin
 * 0x206fd93c/0x206fd93e) the eye is on: the sign of a 64-bit sum of the
 * products of the eye's offsets with the normal's 32-bit terms. */
static int grid_facing(void)
{
    int64_t sum;
    int32_t t;

    t = (int32_t)(int16_t)(M16(esi + 0xc) - M16(ebx + 0xc)) * (int16_t)M16(V(0x206fd928));
    edx = (uint32_t)t;
    eax = ((uint32_t)M16(V(0x206fd93c)) << 16) - M32(G + 0xcdc);
    sum = (int64_t)(int32_t)eax * (int32_t)edx;
    t = (int32_t)(int16_t)(uint16_t)-M16(V(0x206fd926)) * (int16_t)M16(V(0x206fd928));
    edx = (uint32_t)t;
    eax = ((uint32_t)M16(ebx + 0xc) << 16) - M32(G + 0xce0);
    sum += (int64_t)(int32_t)eax * (int32_t)edx;
    t = (int32_t)(int16_t)M16(V(0x206fd926)) * (int16_t)(M16(edi + 0xc) - M16(ebx + 0xc));
    edx = (uint32_t)t;
    eax = ((uint32_t)M16(V(0x206fd93e)) << 16) - M32(G + 0xce4);
    sum += (int64_t)(int32_t)eax * (int32_t)edx;
    ebp = (uint32_t)sum;
    ecx = (uint32_t)((uint64_t)sum >> 32);
    return (int32_t)ecx < 0;
}

/* 0x206e9796 -- the triangle esi, edi, ebx through the textured fast
 * triangle: each corner's x, y, z, and its texture word's low and high
 * bytes as u and v; 0x206fdc41 counts it. */
static void grid_triangle(void)
{
    struct fast_vertex v[3];
    uint32_t corner[3] = { esi, edi, ebx }, i;

    for (i = 0; i < 3; i++) {
        uint32_t p = corner[i];
        uint16_t uv = M16(p + 0xe);

        v[2 - i].c[4] = M32(p);
        v[2 - i].c[3] = M32(p + 4);
        v[2 - i].c[2] = M32(p + 8);
        v[2 - i].c[1] = (uint32_t)(uv & 0xff) << 16;
        v[2 - i].c[0] = (uint32_t)(uv >> 8) << 16;
    }
    {
        uint32_t s = esi, d = edi;

        fast_triangle(0, v);
        edi = d, esi = s;
    }
    M16(V(0x206fdc41)) += 1;
}

/* 0x206e9508 -- a cell (bx the column, cx the row): its two triangles,
 * split along alternate diagonals, each drawn when it faces the eye; when
 * neither is drawn and the eye is over the cell, GLOBAL +0x832 2. */
static void grid_cell(void)
{
    uint16_t col = BX, row = CX, k0, k1, k2, k3;
    uint32_t corner;

    M16(V(0x206fd93a)) = (uint16_t)(row * M16(V(0x206fd92e)) + col);
    M16(V(0x206fd93c)) = (uint16_t)(col * M16(V(0x206fd926)) + M16(V(0x206f91ef)));
    M16(V(0x206fd93e)) = (uint16_t)(row * M16(V(0x206fd928)) + M16(V(0x206f91f1)));
    M16(V(0x206fdc41)) = 0;
    SET_AX(M16(V(0x206fd93a)) << 4);
    corner = (uint32_t)(int32_t)(int16_t)AX + M32(V(0x206f912a));
    if ((col ^ row) & 1) {
        ebx = esi = edi = corner;
        esi += 0x10;
        edi += M32(V(0x206fd932));
        if (!grid_facing())
            grid_triangle();
        k0 = M16(V(0x206fd93c)), k1 = M16(V(0x206fd93e));
        M16(V(0x206fd93c)) += M16(V(0x206fd926));
        M16(V(0x206fd93e)) += M16(V(0x206fd928));
        k2 = M16(V(0x206fd926)), k3 = M16(V(0x206fd928));
        M16(V(0x206fd926)) = (uint16_t)-k2;
        M16(V(0x206fd928)) = (uint16_t)-k3;
        { uint32_t t = edi; edi = esi; esi = t; }
        ebx = esi + 0x10;
        {
            int back = grid_facing();

            M16(V(0x206fd928)) = k3, M16(V(0x206fd926)) = k2;
            M16(V(0x206fd93e)) = k1, M16(V(0x206fd93c)) = k0;
            if (!back)
                grid_triangle();
        }
    } else {
        esi = corner;
        ebx = corner + 0x10;
        edi = ebx + M32(V(0x206fd932));
        k0 = M16(V(0x206fd93c));
        k2 = M16(V(0x206fd926));
        M16(V(0x206fd93c)) += k2;
        M16(V(0x206fd926)) = (uint16_t)-k2;
        {
            int back = grid_facing();

            M16(V(0x206fd926)) = k2, M16(V(0x206fd93c)) = k0;
            if (back)
                grid_triangle();
        }
        k1 = M16(V(0x206fd93e));
        k3 = M16(V(0x206fd928));
        M16(V(0x206fd93e)) += k3;
        M16(V(0x206fd928)) = (uint16_t)-k3;
        { uint32_t t = edi; edi = esi; esi = t; }
        ebx = esi - 0x10;
        {
            int back = grid_facing();

            M16(V(0x206fd928)) = k3, M16(V(0x206fd93e)) = k1;
            if (back)
                grid_triangle();
        }
    }
    M16(V(0x206f90f6)) = 1;
    edi = G;
    if ((int16_t)M16(V(0x206fdc41)) < 1) {
        SET_AX(M16(V(0x206fd93c)));
        SET_BX(M16(V(0x206fd93e)));
        if ((int16_t)M16(edi + 0xcde) >= (int16_t)AX && (int16_t)M16(edi + 0xce6) >= (int16_t)BX) {
            SET_AX(AX + M16(V(0x206fd926)));
            SET_BX(BX + M16(V(0x206fd928)));
            if ((int16_t)M16(edi + 0xcde) <= (int16_t)AX && (int16_t)M16(edi + 0xce6) <= (int16_t)BX)
                M16(edi + 0x832) = 2;
        }
    }
}

/* 0x206e90c1 -- 0x15, a grid of ground: columns, rows, the cells' x and z
 * sizes, the origin's x and z, the lowest and highest heights, then a
 * texture word and a height for each vertex ((columns + 1) x (rows + 1)).
 * Its box's eight corners first (as table points 0..7): all out of view
 * on one side, the vertices are skipped; the eye inside the box below its
 * top draws its floor (the four lower corners, line colour 0x6900).  Then
 * the vertices into the point table and the cells drawn from the far ones
 * in. */
static void op_grid(void)
{
    uint32_t i, keep;

    M16(V(0x206fd92a)) = 0x1388;
    M16(V(0x206fd92c)) = 0x1388;
    esi += 2;
    lods16();
    M16(V(0x206fd922)) = AX;
    SET_AX(AX + 1);
    M16(V(0x206fd92e)) = AX;
    eax = (uint32_t)(int32_t)(int16_t)AX;
    eax <<= 4;
    M32(V(0x206fd932)) = eax;
    lods16();
    M16(V(0x206fd924)) = AX;
    SET_AX(AX + 1);
    M16(V(0x206fd930)) = AX;
    lods16();
    M16(V(0x206fd926)) = AX;
    SET_AX((uint16_t)((uint32_t)AX * M16(V(0x206fd922))));
    M16(V(0x206fd936)) = AX;
    lods16();
    M16(V(0x206fd928)) = AX;
    SET_AX((uint16_t)((uint32_t)AX * M16(V(0x206fd924))));
    M16(V(0x206fd938)) = AX;
    lods16();
    M16(V(0x206f91ef)) = AX;
    lods16();
    M16(V(0x206f91f1)) = AX;
    lods16();
    M16(V(0x206fdbf8)) = AX;
    lods16();
    M16(V(0x206fdbfa)) = AX;
    M8(V(0x206fdc0a)) = 0xff;
    M8(V(0x206fdc0c)) = 0;
    M8(V(0x206fdbfc)) = 0x7f;
    keep = esi;
    edi = M32(V(0x206f912a));
    for (i = 0; i < 8; i++) {
        uint32_t s = esi;

        SET_AX(M16(V(0x206f91ef)) + ((i & 1) ^ ((i >> 1) & 1) ? M16(V(0x206fd936)) : 0));
        SET_BX(M16(V(i < 4 ? 0x206fdbf8 : 0x206fdbfa)));
        SET_DX(M16(V(0x206f91f1)) + (i & 2 ? M16(V(0x206fd938)) : 0));
        point_from_words();
        esi = s;
        table_outcode();
        M8(V(0x206fdc0a)) &= AL;
        M8(V(0x206fdc0c)) |= AL;
        SET_AL(M8(V(0x206f9122)));
        if ((int8_t)AL < (int8_t)M8(V(0x206fdbfc)))
            M8(V(0x206fdbfc)) = AL;
        edi += 0x10;
    }
    esi = keep;
    if (M8(V(0x206fdc0a)) != 0) {
        SET_AX((uint16_t)((uint32_t)M16(V(0x206fd92e)) * M16(V(0x206fd930))));
        SET_AX(AX << 2);
        eax = (uint32_t)(int32_t)(int16_t)AX;
        esi += eax;
        M32(V(0x206f9126)) = esi;
        return;
    }
    M8(V(0x206f9122)) = M8(V(0x206fdbfc));
    edi = G;
    SET_AX(M16(V(0x206fdbfa)));
    if ((int16_t)M16(edi + 0xce2) <= (int16_t)AX) {
        SET_AX(M16(V(0x206f91ef)));
        if ((int16_t)M16(edi + 0xcde) >= (int16_t)AX &&
            (int16_t)M16(edi + 0xcde) <= (int16_t)(uint16_t)(AX + M16(V(0x206fd936)))) {
            SET_AX(M16(V(0x206f91f1)));
            if ((int16_t)M16(edi + 0xce6) >= (int16_t)AX &&
                (int16_t)M16(edi + 0xce6) <= (int16_t)(uint16_t)(AX + M16(V(0x206fd938)))) {
                uint32_t s = esi;

                M32(edi + 0x3c0) = 0x6900;
                M16(edi + 0xc90) = 0;
                polygon_count();
                eax = 0;
                table_first();
                eax = 1;
                table_next();
                eax = 2;
                table_next();
                eax = 3;
                table_next();
                polygon_close();
                esi = s;
            }
        }
    }
    edi = M32(V(0x206f912a));
    {
        uint16_t x = 0, z = 0;

        for (;;) {
            M16(edi + 0xe) = M16(esi);
            SET_BX(M16(esi + 2));
            M16(edi + 0xc) = BX;
            esi += 4;
            {
                uint32_t s = esi;

                SET_AX(x + M16(V(0x206f91ef)));
                SET_DX(z + M16(V(0x206f91f1)));
                point_from_words();
                esi = s;
            }
            edi += 0x10;
            x = (uint16_t)(x + M16(V(0x206fd926)));
            if ((int16_t)x <= (int16_t)M16(V(0x206fd936)))
                continue;
            x = 0;
            z = (uint16_t)(z + M16(V(0x206fd928)));
            if ((int16_t)z <= (int16_t)M16(V(0x206fd938)))
                continue;
            break;
        }
    }
    M32(V(0x206f9126)) = esi;
    grid_cells(0x206e9508);
    M8(V(0x206f9125)) = 0;
    edi = G;
    M16(edi + 0xc90) = 0;
}


/* 0x206f1978 -- 0x27, a facet that keeps its plane (plane_facing leaves
 * esi on it; its five words go to 0x206f91ab..0x206f91b3): shaded unless GLOBAL +0xcb6 is set, GLOBAL
 * +0xc90 twice +0xcb6 while it draws, the flat clipper always.  Its count
 * is a word, dec then loop: one vertex walks four billion. */
static void op_plane_facet(void)
{
    uint16_t count;
    uint32_t i;

    esi += 2;
    lods16();
    count = AX;                                 /* push ax */
    plane_facing();
    SET_CX(count);                              /* pop cx */
    if (sf) {
        SET_CX(CX + CX);                        /* 0x206f1a34 */
        SET_CX(CX + 0xe);
        ecx = CX;
        M32(V(0x206f9126)) += ecx;
        goto off;
    }
    edi = G;
    if (M16(edi + 0xcb6) == 0) {
        uint16_t c = CX;

        shade();
        shade_colour();
        SET_CX(c);
    }
    for (i = 0; i < 5; i++) {
        lods16();
        M16(V(0x206f91ab) + 2 * i) = AX;
    }
    edi = G;
    SET_AX(M16(edi + 0xcb6));
    SET_AX(AX + AX);
    M16(edi + 0xc90) = AX;
    polygon_count();
    lods16();
    eax = (uint32_t)(int32_t)(int16_t)AX;
    {
        uint16_t c = CX;
        uint32_t keep = esi;

        table_first();
        esi = keep;
        SET_CX(c);
    }
    SET_CX(CX - 1);
    ecx = CX;
    do {
        lods16();
        eax = (uint32_t)(int32_t)(int16_t)AX;
        {
            uint16_t c = CX;
            uint32_t keep = esi;

            table_next();
            esi = keep;
            SET_CX(c);
        }
    } while (--ecx != 0);
    M32(V(0x206f9126)) = esi;
    polygon_close();
off:
    edi = G;
    M16(edi + 0xc90) = 0;
    M8(V(0x206f9125)) = 0;
}

/* ---- CHAIN's callbacks ------------------------------------------------------------- */

/* A CHAIN callback is entered as a service is, (a, b) and ret 8.  Those not
 * transliterated yet stop if they are called; init only registers them. */
#define CALLBACK_PENDING(name)                                                \
    static void FS98_STDCALL name(uint32_t a, uint32_t b)                    \
    {                                                                         \
        (void)a, (void)b;                                                     \
        __builtin_trap();                                                     \
    }

CALLBACK_PENDING(cb_206e48de)       /* list 0x10 */
CALLBACK_PENDING(cb_206e4b99)       /* list 1 */
CALLBACK_PENDING(cb_206e5500)       /* list 5 */
CALLBACK_PENDING(cb_206f01dc)       /* list 1 */
CALLBACK_PENDING(cb_206f01b0)       /* list 9 */
CALLBACK_PENDING(cb_206f0322)       /* list 0x14 */
CALLBACK_PENDING(cb_206eb55a)       /* list 3 */
CALLBACK_PENDING(cb_206f4505)       /* list 6 */

/* ---- init's parts ------------------------------------------------------------------ */

/* 0x206e4710 */
static void init_callbacks(void)
{
    CHAIN_ADD(0x10, cb_206e48de);
    CHAIN_ADD(1, cb_206e4b99);
    CHAIN_ADD(5, cb_206e5500);
}

/* 0x206e478c -- the three blocks at 0x206fd72a, 0x206fd829 and 0x206fd821
 * freed, and the callbacks off. */
static void free_if_set(uint32_t p)
{
    if (M32(p) != 0) {
        H_FREE(p);
        M32(p) = 0;
    }
}

static void deinit_callbacks(void)
{
    free_if_set(V(0x206fd72a));
    free_if_set(V(0x206fd829));
    free_if_set(V(0x206fd821));
    CHAIN_REMOVE(0x10, cb_206e48de);
    CHAIN_REMOVE(1, cb_206e4b99);
    CHAIN_REMOVE(5, cb_206e5500);
}

/* 0x206e6c60 */
static void init_lighting(void)
{
    M16(V(0x206f91bb)) = 0x3fff;
}

/* 0x206e61dd -- the queue's size for eax entries: a 0x14-byte header, a
 * pointer and an entry (0x206fd860 bytes) each. */
static void queue_size(void)
{
    uint64_t p;

    ebx = eax;
    p = (uint64_t)eax * M32(V(0x206fd860));
    eax = (uint32_t)p;
    edx = (uint32_t)(p >> 32);
    { uint32_t t = ebx; ebx = eax; eax = t; }
    eax <<= 2;
    eax += ebx;
    eax += 0x14;
}

/* 0x206e6096 -- the depth-sorted queue (0x206fd87c), sized from the table
 * at 0x206fd864 by GLOBAL +0xb60 (clamped to 4): allocated, or reallocated
 * when the size changed; then emptied, its pointers at +0x14 and its
 * entries after them. */
static void queue_alloc(void)
{
    uint32_t keep;

    edi = G;
    ebx = M32(edi + 0xb60);
    if (ebx > 4)
        ebx = 4;
    ebx = M32(V(0x206fd864) + ebx * 4);
    eax = M32(V(0x206fd87c));
    if (eax != 0) {
        ecx = M32(eax);
        if (ebx == ecx)
            goto reset;
        keep = ebx;
        eax = ebx;
        queue_size();
        ebx = eax;
        H_REALLOC(2, ebx, V(0x206fd87c));
        ebx = keep;
    } else {
        keep = ebx;
        eax = ebx;
        queue_size();
        ebx = eax;
        H_ALLOC(2, ebx, V(0x206fd87c), 1);
        ebx = keep;
    }
    eax = M32(V(0x206fd87c));
    M32(eax) = ebx;
reset:                                          /* 0x206e613e */
    { uint32_t t = ebx; ebx = eax; eax = t; }
    ecx = 0x14;
    ecx += ebx;
    M32(ebx + 8) = ecx;
    eax <<= 2;
    eax += ecx;
    M32(ebx + 0xc) = eax;
    M32(ecx) = eax;
    M32(ebx + 0x10) = eax;
    M32(ebx + 4) = 0;
}

/* 0x206e615e */
static void queue_free(void)
{
    H_FREE(V(0x206fd87c));
}

/* 0x206e8c80 -- two words from GLOBAL +0x3aa/+0x3ab scaled by 0x1b2 (the
 * second multiply's al is the first product's low byte), a callback on
 * list 3, and a 0x4000-byte block at 0x206fdc16. */
static void init_8c80(void)
{
    uint32_t p;

    edi = G;
    SET_BX(0x1b2);
    SET_AL(0);
    SET_AH(M8(edi + 0x3aa));
    p = (uint32_t)AX * BX;
    SET_AX(p);
    SET_DX(p >> 16);
    M16(V(0x206fdc3d)) = DX;
    SET_AH(M8(edi + 0x3ab));
    p = (uint32_t)AX * BX;
    SET_AX(p);
    SET_DX(p >> 16);
    SET_DX(-DX);
    M16(V(0x206fdc3f)) = DX;
    CHAIN_ADD(3, cb_206eb55a);
    H_ALLOC(2, 0x4000, V(0x206fdc16), 1);
    M16(V(0x206fdc1a)) = 0x400;
    M32(V(0x206fddf1)) = 0;
}

/* 0x206e8d1f */
static void deinit_8c80(void)
{
    CHAIN_REMOVE(3, cb_206eb55a);
    H_FREE(V(0x206fdc16));
}

/* ---- textures: .text+0xeff0..+0xf849 ------------------------------------------------ */

static void release_textures(void);

/* 0x206efff0 -- the texture table at 0x206ff8b4: 0x80 slots of 0x1c bytes
 * after a 6-byte header (count, then the size word at +4).  The clamps
 * from 0x87 are retail's arithmetic on constants, kept. */
static void init_textures(void)
{
    uint32_t p, size;

    M32(V(0x206f9178)) = 0xc0000000u;
    SET_BX(0x87);
    SET_CX(0x87);
    M32(V(0x206ff8b0)) = 0x1c;
    if (BX > CX)
        SET_BX(CX);
    SET_BX(BX - 5);
    if ((int16_t)BX < 0)
        SET_BX(5);
    if (BX >= 0x80)
        SET_BX(0x80);
    M16(V(0x206ff8a4)) = BX;
    eax = 0x1c;
    p = (uint32_t)AX * BX;
    SET_AX(p);
    SET_DX(p >> 16);
    SET_AX(AX + 6);
    size = AX;                                  /* push ax */
    H_ALLOC(2, eax, V(0x206ff8b4), 1);
    edi = M32(V(0x206ff8b4));
    SET_AX(M16(V(0x206ff8a4)));
    M16(edi) = AX;
    M16(edi + 4) = (uint16_t)size;              /* pop word [edi+4] */
    CHAIN_ADD(1, cb_206f01dc);
    CHAIN_ADD(9, cb_206f01b0);
    CHAIN_ADD(0x14, cb_206f0322);
}

/* 0x206f00fc */
static void deinit_textures(void)
{
    release_textures();
    CHAIN_REMOVE(1, cb_206f01dc);
    CHAIN_REMOVE(9, cb_206f01b0);
    CHAIN_REMOVE(0x14, cb_206f0322);
    if (M32(V(0x206ff8b4)) != 0)
        H_FREE(V(0x206ff8b4));
}

/* 0x206f079c -- release ax textures, oldest first: each pass takes the
 * loaded slot with the highest age word (+0x12; the last of equals),
 * clears its flags and hands its texture (+0x14) to HG2D.  Carry set when
 * there was nothing to release; ecx counts the slots, and a table whose
 * count is 0 would be walked 2^32 times. */
static void release_oldest(void)
{
    edi = M32(V(0x206ff8b4));
    if (AX == 0) {
        cf = 1;
        return;
    }
    SET_DX(AX);
    do {
        ebx = edi + 6;
        SET_AX(0);
        esi = 0;
        ecx = M16(edi);
        do {
            if (M32(ebx + 0x14) != 0 && !(AX > M16(ebx + 0x12))) {
                SET_AX(M16(ebx + 0x12));
                esi = ebx;
            }
            ebx += M32(V(0x206ff8b0));
        } while (--ecx != 0);
        if (esi == 0) {
            cf = 1;
            return;
        }
        eax = 0;
        M8(esi) = AL;
        M8(esi + 0xd) = AL;
        { uint32_t t = M32(esi + 0x14); M32(esi + 0x14) = eax; eax = t; }
        {
            uint16_t keep = DX;

            HG2D_UNLOAD(eax);                   /* 0x206f0820 */
            SET_DX(keep);
        }
        SET_DX(DX - 1);
    } while (DX != 0);
    cf = 0;
}

/* 0x206f078a -- +0x54: release every texture (the table's count). */
static void release_textures(void)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi;

    SET_AX(M16(V(0x206ff8a4)));
    release_oldest();
    ebx = keep_ebx;
    esi = keep_esi;
    edi = keep_edi;
}

SERVICE(release_all_textures, 0);
static void release_all_textures_body(const uint32_t *arg)
{
    (void)arg;
    release_textures();
}

/* 0x206f07ef -- +0x58: clear the flags of every slot that has them but no
 * texture. */
SERVICE(clear_unloaded, 0);
static void clear_unloaded_body(const uint32_t *arg)
{
    (void)arg;
    ebx = M32(V(0x206ff8b4));
    if (ebx == 0)
        return;
    ecx = M16(ebx);
    ebx += 6;
    do {
        if (M8(ebx) != 0 && M32(ebx + 0x14) == 0) {
            M8(ebx) = 0;
            M8(ebx + 0xd) = 0;
        }
        ebx += M32(V(0x206ff8b0));
    } while (--ecx != 0);
}

/* ---- libraries: .text+0x13390..+0x136c2 ------------------------------------------- */

/* 0x206f4390 / 0x206f43ba */
static void release_lists(void);

static void init_libraries(void)
{
    CHAIN_ADD(6, cb_206f4505);
}

static void deinit_libraries(void)
{
    release_lists();
    CHAIN_REMOVE(6, cb_206f4505);
}

/* 0x206f44be -- the 64 lists of 0x1c bytes at 0x20710360: each one with
 * data (+0x18) emptied. */
static void release_lists(void)
{
    ecx = 0x40;
    esi = 0;
    do {
        eax = M32(V(0x20710378) + esi);
        if (eax != 0) {
            uint32_t keep_ecx = ecx, keep_esi = esi;

            esi = V(0x20710360) + esi;
            H_LIST_CLEAR(esi);
            esi = keep_esi;
            ecx = keep_ecx;
        }
        esi += 0x1c;
    } while (--ecx != 0);
}

/* 0x206f462b -- CHAIN list 3, once a frame: age the library list at
 * 0x20710a70 (0x18-byte entries): an entry whose count (+0x14, a word)
 * goes negative is freed (its block at +0x10) and removed; with
 * 0x20710a90 set, every entry is.  The arguments are read and ignored. */
static void age_libraries(uint32_t a, uint32_t b)
{
    eax = a;
    eax = b;
    ecx = M32(V(0x20710a70));
    if (ecx == 0)
        return;
    ebx = M32(V(0x20710a88));
    do {
        if (M32(V(0x20710a90)) == 0) {
            M16(ebx + 0x14) = M16(ebx + 0x14) - 1;
            if (!(M16(ebx + 0x14) & 0x8000)) {
                ebx += 0x18;
                continue;
            }
        }
        {
            uint32_t keep_ecx = ecx;

            if (M32(ebx + 0x10) != 0) {
                uint32_t keep_ebx = ebx;

                H_FREE(ebx + 0x10);
                ebx = keep_ebx;
            }
            H_LIST_REMOVE(V(0x20710a70), ebx);
            ecx = keep_ecx;
        }
    } while (--ecx != 0);
}

SERVICE(cb_206f462b, 8);
static void cb_206f462b_body(const uint32_t *arg)
{
    age_libraries(arg[0], arg[1]);
}

/* 0x206f4570 / 0x206f459a -- the ageing on list 3; at deinit, every entry
 * removed and the list freed. */
static void init_library_ageing(void)
{
    CHAIN_ADD(3, cb_206f462b);
}

static void deinit_library_ageing(void)
{
    CHAIN_REMOVE(3, cb_206f462b);
    M32(V(0x20710a90)) = 1;
    age_libraries(0, 0);
    H_LIST_CLEAR(V(0x20710a70));
    H_LIST_FREE(V(0x20710a70));
}

/* 0x206f431c -- +0x60: when GLOBAL +0x2a8 changed since the last call, step
 * the counter at 0x206fd1fe round 0..0x11. */
SERVICE(step_2a8, 0);
static void step_2a8_body(const uint32_t *arg)
{
    int same;

    (void)arg;
    edi = G;
    SET_SI(M16(edi + 0x2a8));
    same = SI == M16(V(0x20710350));
    M16(V(0x20710350)) = SI;
    if (same)
        return;
    SET_SI(M16(V(0x206fd1fe)));
    SET_SI(SI + 1);
    if (!(SI < 0x12))
        SET_SI(0);
    M16(V(0x206fd1fe)) = SI;
}

/* ---- the Linkage's small services ---------------------------------------- */

/* 0x206e1536 -- +0x3c: normalise the three dwords at (v) in place. */
SERVICE(normalise_vector, 4);
static void normalise_vector_body(const uint32_t *arg)
{
    ebx = arg[0];
    ebp = M32(ebx);
    edi = M32(ebx + 4);
    edx = M32(ebx + 8);
    normalise();
    ebx = arg[0];
    M32(ebx) = ebp;
    M32(ebx + 4) = edi;
    M32(ebx + 8) = edx;
}

/* 0x206e36e7 -- +0x40: the dword at 0x206f912a. */
SERVICE(get_912a, 0);
static void get_912a_body(const uint32_t *arg)
{
    (void)arg;
    eax = M32(V(0x206f912a));
}

/* 0x206e36f3 -- +0x44: (x * 2 << (GLOBAL +0xd42 + 8)) / GLOBAL +0x320, the
 * shift 64-bit (shld) with its count masked to five bits, the divide the
 * host's repair saturates. */
SERVICE(scale_to_screen, 4);
static void scale_to_screen_body(const uint32_t *arg)
{
    uint32_t n;

    eax = arg[0];
    cdq();
    edx = (edx << 1) | (eax >> 31);
    eax <<= 1;
    edi = G;
    ecx = M16(edi + 0xd42);
    ecx += 8;
    n = CL & 31;
    if (n != 0) {
        edx = (edx << n) | (eax >> (32 - n));
        eax <<= n;
    }
    ebx = M32(edi + 0x320);
    idiv32(ebx);
}

/* 0x206e1577 -- +0x5c: once a frame, count 0x206f915b down, reloading it
 * from 0x206f915c (through al) when it goes negative. */
SERVICE(frame_tick, 0);
static void frame_tick_body(const uint32_t *arg)
{
    (void)arg;
    M8(V(0x206f915b)) = (uint8_t)(M8(V(0x206f915b)) - 1);
    if (M8(V(0x206f915b)) & 0x80) {
        SET_AL(M8(V(0x206f915c)));
        M8(V(0x206f915b)) = AL;
    }
}

/* 0x206e2850 -- +0x64: the address after the instruction at (p), by the
 * length word beside each handler in the opcode table (0x206fd224); 0 for
 * an opcode past 0x93 or a length that is not positive. */
SERVICE(next_instruction, 4);
static void next_instruction_body(const uint32_t *arg)
{
    edi = arg[0];
    ecx = 0;
    SET_SI(M16(edi));
    if (SI <= 0x93) {
        eax = esi & 0xffff;
        eax = M32(V(0x206fd224) + eax * 8);
        ecx = eax;
        edx = eax;
        ecx <<= 16;
        eax <<= 8;
        edx = sar32(edx, 24);
        ecx = sar32(ecx, 16);
        eax = sar32(eax, 24);
        if (edx != 0) {
            ebx = 0;
            SET_BX(M16(eax + edi));
            ebx = (uint32_t)((int32_t)ebx * (int32_t)edx);
            ebx++;
            ebx &= 0xfffffffeu;
            ecx += ebx;
        }
        if (SI == 0x15) {
            edx = 0;
            eax = 0;
            SET_DX(M16(edi + 2));
            SET_AX(M16(edi + 4));
            edx++;
            eax++;
            edx = (uint32_t)((int32_t)edx * (int32_t)eax);
            ecx += edx * 4;
        }
    }
    if ((int32_t)ecx > 0)
        eax = ecx + edi;
    else
        eax = 0;
}

/* ---- routines by retail address -------------------------------------------- */

/* The routine at retail address `a`, for the code pointers kept in the
 * image.  An address not mapped yet is a stop: the harness says where. */
uint32_t g3d_unmapped;

static void unmapped(void)
{
    __builtin_trap();
}

static routine_fn routine(uint32_t a)
{
    switch (a) {
    case 0x206e2620: return op_return;          /* 0x22, 0x7c */
    case 0x206e2630: return op_skip2;           /* 0x02, 0x43, 0x64, 0x85 */
    case 0x206e2640: return op_end;             /* 0x00 */
    case 0x206e2660: return op_8f;
    case 0x206e27f0: return op_92;
    case 0x206e2820: return op_93;
    case 0x206e3727: return bare_ret;           /* 0x01 */
    case 0x206e3728: return op_76;
    case 0x206e3730: return op_86;
    case 0x206e3738: return op_04;
    case 0x206e3758: return op_jump;            /* 0x16 */
    case 0x206e3765: return op_17;
    case 0x206e378f: return op_54;
    case 0x206e3797: return op_native;          /* 0x8c */
    case 0x206e3a00: return op_0d;
    case 0x206e4184: return op_1f;
    case 0x206e419f: return op_call;            /* 0x23 */
    case 0x206e41c9: return op_call32;          /* 0x8a */
    case 0x206e41f1: return op_call_absolute;   /* 0x5c */
    case 0x206e43e5: return op_5b;
    case 0x206e4422: return op_61;
    case 0x206e4470: return op_81;
    case 0x206e54cb: return op_71;
    case 0x206e5cda: return op_88;
    case 0x206e5ce2: return op_8d;
    case 0x206e5cf2: return op_8e;
    case 0x206e6796: return op_47;
    case 0x206e8a2b: return op_14;
    case 0x206e8ab5: return op_51;
    case 0x206e8ad6: return op_52;
    case 0x206e8b58: return op_65;
    case 0x206e8b99: return op_67;
    case 0x206e8c4b: return op_87;
    case 0x206f06b4: return op_5e;
    case 0x206f075e: return op_58;
    case 0x206f1a70: return op_7d;
    case 0x206f1aaa: return op_26;
    case 0x206f1adf: return op_7e;
    case 0x206f1b2c: return op_48;
    case 0x206f1b5f: return bare_ret;           /* 0x59 */
    case 0x206f1b60: return op_4d;
    case 0x206f1bb2: return op_4e;
    case 0x206f1c04: return op_4f;
    case 0x206f1c4a: return op_89;
    case 0x206f1c70: return op_24;
    case 0x206f1cb7: return op_1c;
    case 0x206f1d35: return op_21;
    case 0x206f1dea: return op_39;
    case 0x206f1e2b: return op_03;
    case 0x206f4813: return bare_ret;           /* 0x4b */
    case 0x206e37b0: return op_surface;
    case 0x206e37ed: return op_start;
    case 0x206e3889: return op_continue;
    case 0x206e3904: return op_close;
    case 0x206e394d: return op_09;
    case 0x206e3a0b: return op_define_point;
    case 0x206e3a4c: return op_start_index;
    case 0x206e3aa7: return op_continue_index;
    case 0x206e3b81: return op_colour_point;
    case 0x206e3e95: return op_point;
    case 0x206e4224: return op_reject;
    case 0x206e442c: return op_point_index;
    case 0x206e40a4: return op_face;
    case 0x206e4297: return op_facet;
    case 0x206e4209: return op_separation;
    case 0x206e397e: return op_shaded_start;
    case 0x206e39ca: return op_shaded_next;
    case 0x206e39f3: return op_shaded_close;
    case 0x206e3c22: return op_shaded_start_index;
    case 0x206e3c82: return op_shaded_next_index;
    case 0x206e6e7d: return op_shaded_facet;
    case 0x206e89b4: return op_colour_ramp;
    case 0x206e8b1d: return op_ramp;
    case 0x206e8b27: return op_variable_ramp;
    case 0x206f0850: return op_textured_facet;
    case 0x206f0bb7: return op_textured_face;
    case 0x206f1978: return op_plane_facet;
    case 0x206e90c1: return op_grid;
    case 0x206e9508: return grid_cell;
    case 0x206f0343: return op_texture;
    case 0x206f0672: return op_texture_again;
    case 0x206f4888: return op_strip_next;
    case 0x206f4ede: return op_strip_lights;
    case 0x206f4fb4: return op_strip_width;
    case 0x206e747f: return op_object_shadow;
    case 0x206eefe0: return op_turned_call;
    case 0x206efdf4: return op_turned_call_var;
    case 0x206efe9b: return op_moved_call;
    case 0x206e2690: return op_compile_mesh;
    case 0x206f4ff0: return op_light_point;
    case 0x206e26d0: return op_mesh;
    case 0x206e3e4b: return op_ground_points;
    case 0x206e6e0b: return op_shaded_points;
    case 0x206e87d1: return op_shadow_run;
    case 0x206e4259: return op_queue;
    case 0x206e4279: return op_queue_far;
    case 0x206f1a5b: return op_queue_second;
    case 0x206ea861: return op_run_and_queue;
    case 0x206e6260: return op_object;
    case 0x206e6bdf: return op_object_ground;
    case 0x206e6494: return op_object_var;
    case 0x206e6c28: return op_object_again;
    case 0x206e64ed: return op_object_check_var;
    case 0x206e658a: return op_object_check_packed;
    case 0x206e6606: return op_object_scale;
    case 0x206f1fb0: return op_eye_in_box;
    case 0x206f200d: return op_eye_in_turned_box;
    case 0x206e723f: return op_shadow;
    case 0x206e8bda: return op_own_shadow;
    case 0x206f43e9: return op_defer;
    case 0x206f43fe: return op_defer_far;
    case 0x206e459d: return op_value_point;
    case 0x206e3ef0: return op_row;
    case 0x206e3ece: return op_row_1286;
    case 0x206e3f2e: return op_row_mask;
    case 0x206e3f7e: return op_row_table;
    case 0x206e43b3: return op_weather;
    case 0x206e6cdb: return op_sun_shade;
    case 0x206e71d5: return op_map_call;
    case 0x206f1f3f: return op_reject_points;
    case 0x206f4820: return op_light;
    case 0x206f4ed8: return op_light_6e;
    case 0x206e5384: return op_area_colour;
    case 0x206e5422: return op_inside;
    case 0x206e4129: return op_colour_word;
    case 0x206e4373: return op_detail;
    case 0x206f1e73: return op_size_jump_y;
    case 0x206f1eae: return op_size_jump_x;
    case 0x206e8a47: return op_line_rgb;
    case 0x206e8a82: return op_surface_rgb;
    case 0x206e8c22: return op_shade_normal;
    case 0x206f0d41: return op_shade_texture;
    case 0x206ec310: return op_st_facet;
    case 0x206ec545: return op_st_face;
    default:
        g3d_unmapped = a;
        return unmapped;
    }
}

/* ---- the image ---------------------------------------------------------------- */

struct g3d_image g3d_image = {{
#include "g3d_data.inc"
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _g3d_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _g3d_image + 0x58");

static const struct { uint32_t at, to; } data_pointers[] = {
#define G3D_DATA_POINTER(at, to) { at, to },
#define G3D_CODE_TARGET(a)
#include "g3d_pointers.inc"
#undef G3D_DATA_POINTER
#undef G3D_CODE_TARGET
};

static const uint32_t code_targets[] = {
#define G3D_DATA_POINTER(at, to)
#define G3D_CODE_TARGET(a) a,
#include "g3d_pointers.inc"
#undef G3D_DATA_POINTER
#undef G3D_CODE_TARGET
};

/* How many of the image's code addresses routine() does not map yet; the
 * harness reports it.  Zero when the module is complete. */
uint32_t g3d_unmapped_targets;

static void g3d_install(void)
{
    static void *const services[20] = {
        (void *)run_in_context,     /* +0x18 */
        (void *)run,                /* +0x1c */
        (void *)draw_counted,       /* +0x20 */
        (void *)draw_uncounted,     /* +0x24 */
        (void *)draw,               /* +0x28 */
        0,                          /* +0x2c */
        (void *)queue_draw_all,     /* +0x30 */
        (void *)queue_draw_svc,     /* +0x34 */
        (void *)queue_model_svc,    /* +0x38 */
        (void *)normalise_vector,   /* +0x3c */
        (void *)get_912a,           /* +0x40 */
        (void *)scale_to_screen,    /* +0x44 */
        0,                          /* +0x48 */
        0,                          /* +0x4c */
        0,                          /* +0x50 */
        (void *)release_all_textures, /* +0x54 */
        (void *)clear_unloaded,     /* +0x58 */
        (void *)frame_tick,         /* +0x5c */
        (void *)step_2a8,           /* +0x60 */
        (void *)next_instruction,   /* +0x64 */
    };
    uint32_t i;

    for (i = 0; i < sizeof data_pointers / sizeof data_pointers[0]; i++)
        M32(D + data_pointers[i].at) = D + data_pointers[i].to;
    for (i = 0; i < sizeof code_targets / sizeof code_targets[0]; i++)
        if (routine(code_targets[i]) == unmapped)
            g3d_unmapped_targets++;
    g3d_unmapped = 0;
    M32(D + G3D_LINKAGE + 0x04) = (uint32_t)(uintptr_t)g3d_init;
    M32(D + G3D_LINKAGE + 0x08) = (uint32_t)(uintptr_t)g3d_deinit;
    for (i = 0; i < 20; i++)
        M32(D + G3D_LINKAGE + 0x18 + 4 * i) = (uint32_t)(uintptr_t)services[i];
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g3d_install();
    }
    return TRUE;
}
