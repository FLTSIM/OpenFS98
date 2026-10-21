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

/* div r/m16 of dx:ax, with the host's repair: all ones on a fault. */
static void div16(uint16_t d)
{
    uint32_t n = (uint32_t)DX << 16 | AX;

    if (d != 0 && n / d <= 0xffff) {
        SET_AX(n / d);
        SET_DX(n % d);
        return;
    }
    SET_AX(0xffff);
    SET_DX(0xffff);
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

/* A service that keeps none of the caller's registers (the routines other
 * modules call with a record in esi, which retail pushes nothing round):
 * every register comes back as the body leaves it. */
#define SERVICE_RAW(name, bytes)                                              \
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
            "movl _g3d_ebx, %ebx\n\tmovl _g3d_esi, %esi\n\t"                 \
            "movl _g3d_edi, %edi\n\tmovl _g3d_ebp, %ebp\n\t"                 \
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
#define IMP5(m, off, a, b, c, d, e) \
    import_call(M32((m) + (off)), (const uint32_t[]){ a, b, c, d, e }, 5, (m))

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
 * on the nop.  Otherwise the polygon flag 0x206fd6c8 set and on past it.
 * Without an x87, or with G3D_MESH_C, the compiler is g3d_mesh.c's C,
 * given the point table (.data+0x12a) and +0x64 as the instructions use
 * them. */
#if !G3D_FP_X87 || defined(G3D_MESH_C)
uint32_t g3d_mesh_compile(uint32_t program, uint32_t points,
                          uint32_t (FS98_STDCALL *next)(uint32_t));
uint32_t FS98_STDCALL g3d_next_instruction(uint32_t p) __asm__("_next_instruction");
#  define g3d_compile_mesh(p) g3d_mesh_compile((p), M32(V(0x206f912a)), g3d_next_instruction)
#else
uint32_t FS98_STDCALL g3d_compile_mesh(uint32_t program) __asm__("_g3d_compile_mesh@4");
#endif

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

/* The host's string services (docs/modules/host.md). */
#define H_STRCMPI(a, b) (eax = IMP2(HOST, 0x2c0, a, b))
#define H_STRCPY(d, s)  (eax = IMP2(HOST, 0x2a8, d, s))
#define H_STRLEN(s)     (eax = IMP1(HOST, 0x2b8, s))

/* 0x206e6d23 -- 0x19, a texture by name (the string after the opcode):
 * 0x206f91b5 0x100 and GLOBAL +0xcbc 0x40; a name other than the last
 * (0x206fd8d0, _strcmpi) is kept there and handed to HG2D +0x68 with the
 * context, GLOBAL +0xcbe 1 when it answers.  On past the name, its nul
 * and one byte more. */
static void op_texture_name(void)
{
    M16(V(0x206f91b5)) = 0x100;
    edi = G;
    M16(edi + 0xcbc) = 0x40;
    esi += 2;
    H_STRCMPI(V(0x206fd8d0), esi);
    if (eax != 0) {
        H_STRCPY(V(0x206fd8d0), esi);
        eax = IMP2(HG2D, 0x68, V(0x206fd8d0), M32(V(0x206f919c)));
        if (eax != 0) {
            edi = G;
            M16(edi + 0xcbe) = 1;
        }
    }
    H_STRLEN(esi);
    eax += 1;
    esi += eax;
    esi += 1;
    M32(V(0x206f9126)) = esi;
}


/* ---- approach lights: 0x72, .text+0xe03c..+0xe161 and +0x128b0..+0x1331a ---------- */

/* 0x72 writes a program at 0x2070fe96 (in a buffer to 0x20710348) from an
 * 18-byte record, copied to 0x2070fe5a, and runs it with the view turned
 * to the record's heading.  The record's words and bytes:
 *   fe5a  the heading             fe5c  the width
 *   fe5e  the threshold's z, its sign the side the lights run to
 *   fe60  flags: bit 0 run from the header at 0x2070fe82, bit 2 the end
 *         bars, bit 4 the sequenced flashers
 *   fe61  the pattern, 1..10      fe62  a count (the bar's lights)
 *   fe63  the VASI: 0 none, 1 two bars, else three
 *   fe64  its glide angle, fe66 its x, fe68 its z, fe6a the bars' spacing
 * fe6c, fe6e and fe70 are past the record: the flashers' state, which
 * nothing else writes.  0x2070fe30 is the eye's z (GLOBAL +0xce6).  The
 * emitters pass the program pointer in edi and their operands in the
 * registers, as retail's do. */

#define LIGHTS_EYE M16(V(0x2070fe30))

/* 0x206f42fb -- 0x51, colour ax; for 0xf016 (flashing) also 0x30 bx. */
static void light_colour(void)
{
    M16(edi) = 0x51;
    M16(edi + 2) = AX;
    edi += 4;
    if (AX == 0xf016) {
        M16(edi) = 0x30;
        M16(edi + 2) = BX;
        edi += 4;
    }
}

/* 0x206f3c14 -- 0x37, a light at x ax, z bx. */
static void light_point(void)
{
    M16(edi) = 0x37;
    M16(edi + 2) = AX;
    M16(edi + 4) = 2;
    M16(edi + 6) = BX;
    edi += 8;
}

/* 0x206f423f -- 0x35, si lights from (ax, cx) to (bx, dx), unless both
 * ends are past the eye on the lights' side. */
static void light_row(void)
{
    if (DX & 0x8000) {
        if (!((int16_t)DX > (int16_t)LIGHTS_EYE || (int16_t)CX > (int16_t)LIGHTS_EYE))
            return;
    } else {
        if (!((int16_t)DX < (int16_t)LIGHTS_EYE) && (int16_t)CX > (int16_t)LIGHTS_EYE)
            return;
    }
    M16(edi) = 0x35;
    M16(edi + 2) = AX;
    M16(edi + 4) = 2;
    M16(edi + 6) = CX;
    M16(edi + 8) = BX;
    M16(edi + 0xa) = 2;
    M16(edi + 0xc) = DX;
    M16(edi + 0xe) = SI;
    edi += 0x10;
}

/* 0x206f4217 -- a row across, at z dx from x ax to bx, cx lights. */
static void light_row_across(void)
{
    uint16_t c = CX, s = SI;

    SET_SI(CX);
    SET_CX(DX);
    light_row();
    SET_SI(s);
    SET_CX(c);
}

/* 0x206f422b -- a row along, at x ax from z cx to dx, bx lights. */
static void light_row_along(void)
{
    uint16_t b = BX, s = SI;

    SET_SI(BX);
    SET_BX(AX);
    light_row();
    SET_SI(s);
    SET_BX(b);
}

/* 0x206f4295 -- 0x45, a bar of al lights (more than one) from z dx, bp
 * apart, in colour 0x6901, unless it starts past the eye. */
static void light_bar(void)
{
    if (AL <= 1)
        return;
    {
        uint16_t a = AX;

        SET_AX(LIGHTS_EYE);
        if (!(DX & 0x8000) ? !((int16_t)AX > (int16_t)DX) : (int16_t)AX > (int16_t)DX) {
            SET_AX(a);
            return;
        }
        SET_AX(0x6901);
        light_colour();
        SET_AX(a);
    }
    SET_AH(0);
    M16(edi + 0xe) = AX;
    M16(edi) = 0x45;
    M16(edi + 0xc) = DX;
    SET_CX(DX);
    SET_AX(AX - 1);
    imul16(BP);
    SET_AX(AX + CX);
    M16(edi + 6) = AX;
    SET_DX(AX);
    SET_AX(0);
    M16(edi + 2) = AX;
    M16(edi + 8) = AX;
    M16(edi + 4) = 2;
    M16(edi + 0xa) = 2;
    edi += 0x10;
}

/* 0x206f4199 -- the header's words: its colour (0x2070fe84: 0xf010, or
 * 0xf00f once the eye is past z cx, cx made to point the lights' way and
 * moved to the threshold), that z (fe8c, fe92), half the width either side
 * (fe88, fe8e), and the width over bx, rounded up (fe94). */
static void light_header(void)
{
    uint16_t start = M16(V(0x2070fe5e));

    if ((start ^ CX) & 0x8000)
        SET_CX(-CX);
    SET_CX(CX + start);
    SET_DX(0xf010);
    if (CX & 0x8000) {
        if (!((int16_t)CX > (int16_t)LIGHTS_EYE))
            SET_DX(0xf00f);
    } else {
        if (!((int16_t)CX < (int16_t)LIGHTS_EYE))
            SET_DX(0xf00f);
    }
    M16(V(0x2070fe84)) = DX;
    M16(V(0x2070fe8c)) = CX;
    M16(V(0x2070fe92)) = CX;
    SET_AX(AX + AX);
    SET_AX(M16(V(0x2070fe5c)));
    SET_CX(sar16(AX, 1));
    M16(V(0x2070fe88)) = CX;
    SET_CX(-CX);
    M16(V(0x2070fe8e)) = CX;
    SET_AX(AX + BX + BX - 1);
    SET_DX(0);
    div16(BX);
    M16(V(0x2070fe94)) = AX;
}

/* 0x206f39cc -- the end bars (with GLOBAL +0x1204), a light either side
 * just short of the threshold, unless the eye is past it. */
static void light_end_bars(void)
{
    if (M16(G + 0x1204) == 0)
        return;
    SET_BX(M16(V(0x2070fe5e)));
    if (!(BX & 0x8000)) {
        if ((int16_t)BX > (int16_t)LIGHTS_EYE)
            return;
        SET_BX(BX + 8);
    } else {
        if ((int16_t)BX < (int16_t)LIGHTS_EYE)
            return;
        SET_BX(BX - 8);
    }
    SET_AX((M16(V(0x2070fe5c)) >> 1) + 5);
    {
        uint16_t a = AX;

        SET_AX(0xf015);
        light_colour();
        SET_AX(a);
    }
    light_point();
    SET_AX(-AX);
    light_point();
}

/* 0x206f3a2e -- the sequenced flashers, from dx: the step 0x2000 << 32
 * over (fe6e times 0xc910) (fe6e is never set: a divide by zero the host
 * repairs), the angle from 0xc000 (0x2071034e, the top of the dword at
 * 0x2071034c); fe6c moved on by the next z; then 8 * fe70 - 1 steps (fe70
 * is never set either), lights on the steps whose low three bits are 5 or
 * more, by z until fe6c and round a circle of radius fe6e after. */
static void light_sequence(void)
{
    SET_BX(DX);
    eax = (uint32_t)(int32_t)(int16_t)M16(V(0x2070fe6e));
    imul32(0xc910);
    ecx = eax;
    edx = 0x2000;
    eax = 0;
    idiv32(ecx);
    M32(V(0x20710348)) = eax;
    M16(V(0x2071034e)) = 0xc000;
    if (!(BX & 0x8000)) {
        if ((int16_t)BX > (int16_t)LIGHTS_EYE)
            return;
        SET_DX(0x64);
    } else {
        if ((int16_t)BX < (int16_t)LIGHTS_EYE)
            return;
        SET_DX(0xff9c);
    }
    SET_BX(BX + DX);
    M16(V(0x2070fe6c)) += BX;
    SET_AX(0xf015);
    light_colour();
    SET_AX(0);
    ecx = M8(V(0x2070fe70));
    if (CX == 0)
        return;
    ecx <<= 3;
    ecx -= 1;
    do {
        if ((DX & 0x8000) ? (int16_t)BX <= (int16_t)M16(V(0x2070fe6c))
                          : (int16_t)BX >= (int16_t)M16(V(0x2070fe6c)))
            goto circle;
        ebp = ecx;
        SET_BP(BP & 7);
        if (BP >= 5)
            light_point();
        SET_BX(BX + DX);
    } while (--ecx != 0);
    return;
circle:
    do {
        ebp = ecx;
        SET_BP(BP & 7);
        if (BP >= 5) {
            uint32_t keep_ecx = ecx;
            uint16_t keep_dx = DX, keep_di = DI, z, carry;

            SET_BX(M16(V(0x2071034e)));
            UTIL_SINCOS(BX, V(0x206fd1d8), V(0x206fd1da));
            SET_DI(keep_di);
            SET_BX(M16(V(0x2070fe6e)));
            SET_AX(M16(V(0x206fd1d8)));
            imul16(BX);
            carry = AX >> 15;
            SET_AX(AX + AX);
            SET_DX(DX + DX + carry);
            SET_DX(DX + M16(V(0x2070fe6e)));
            z = DX;
            SET_AX(M16(V(0x206fd1da)));
            imul16(BX);
            carry = AX >> 15;
            SET_AX(AX + AX);
            SET_DX(DX + DX + carry);
            SET_DX(DX + M16(V(0x2070fe6c)));
            SET_BX(DX);
            SET_AX(z);
            SET_DX(keep_dx);
            ecx = keep_ecx;
            light_point();
        }
        {
            uint16_t d = DX;

            eax = (uint32_t)(int32_t)(int16_t)DX;
            imul32(M32(V(0x20710348)));
            M32(V(0x2071034c)) += eax;
            SET_DX(d);
        }
        SET_CX(CX - 1);
    } while (CX != 0);
}

/* 0x206f40ba -- a VASI bar's colour for the glide angle ax: the carry
 * (none) when the eye is on the other side of the VASI's z; white
 * (0xf015) when the eye is within GLOBAL +0xce2 of twice its distance;
 * else red (0xf00f) unless the eye's slope (its height +0xce2 over the
 * distance) is above the angle's (its sine over its cosine), all unsigned
 * divides the host repairs. */
static int vasi_colour(void)
{
    uint16_t distance;

    SET_BX(M16(G + 0xce6));
    SET_CX(BX);
    SET_BX(BX - M16(V(0x2070fe68)));
    SET_CX(CX ^ BX);
    if (CX & 0x8000)
        return 1;
    if (BX & 0x8000)
        SET_BX(-BX);
    SET_CX(BX >> 1);
    if (CX < M16(G + 0xce2)) {
        SET_AX(0xf015);
        return 0;
    }
    distance = BX;
    SET_BX(AX);
    {
        uint32_t keep_edi = edi;

        UTIL_SINCOS(BX, V(0x206fd1d8), V(0x206fd1da));
        edi = keep_edi;
    }
    SET_DX(M16(V(0x206fd1d8)));
    SET_AX(0);
    div16(M16(V(0x206fd1da)));
    SET_CX(AX);
    SET_DX(M16(G + 0xce2));
    SET_AX(0);
    SET_BX(distance);
    div16(BX);
    SET_AX(AX > CX ? 0xf015 : 0xf00f);
    return 0;
}

/* 0x206f4171 -- a VASI bar in colour ax: five lights at the VASI's z from
 * its x 0x10 outward. */
static void vasi_bar(void)
{
    light_colour();
    SET_AX(M16(V(0x2070fe66)));
    SET_BX(0x10);
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    SET_BX(BX ^ DX);
    SET_BX(BX + AX);
    SET_CX(5);
    SET_DX(M16(V(0x2070fe68)));
    light_row_across();
}

/* 0x206f4021 -- the VASI: the spacing made to point the VASI's z's way,
 * the far bar half a spacing back (nothing when the eye is on the other
 * side of that), then its bars at the angle plus 0x2e, the angle (three
 * bars only) and the angle less 0x2e, a spacing apart. */
static void vasi(void)
{
    SET_AX(M16(V(0x2070fe6a)) ^ M16(V(0x2070fe68)));
    if (AX & 0x8000)
        M16(V(0x2070fe6a)) = -M16(V(0x2070fe6a));
    SET_AX(sar16(M16(V(0x2070fe6a)), 1));
    M16(V(0x2070fe68)) -= AX;
    SET_AX(AX ^ M16(G + 0xce6));
    if (AX & 0x8000)
        return;
    SET_AX(M16(V(0x2070fe64)) + 0x2e);
    if (!vasi_colour())
        vasi_bar();
    if (M8(V(0x2070fe63)) != 1) {
        M16(V(0x2070fe68)) += M16(V(0x2070fe6a));
        SET_AX(M16(V(0x2070fe64)));
        if (!vasi_colour())
            vasi_bar();
    }
    M16(V(0x2070fe68)) += M16(V(0x2070fe6a));
    SET_AX(M16(V(0x2070fe64)) - 0x2e);
    if (!vasi_colour())
        vasi_bar();
}

/* 0x206f3b8b -- pattern 1: from 0x12c past the threshold, white; one
 * light for a count of 1, else (when the word 0x1204 past the instruction
 * is set -- retail loads GLOBAL into edi and tests [esi+0x1204]) a row of
 * that many, 0x12c apart. */
static void lights_1(void)
{
    SET_AX(0x12c);
    SET_BX(M16(V(0x2070fe5e)));
    if (BX & 0x8000)
        SET_AX(-AX);
    SET_BX(BX + AX);
    SET_CL(M8(V(0x2070fe62)));
    SET_CH(0);
    if (CX == 0)
        return;
    {
        uint16_t a = AX;

        SET_AX(0xf015);
        light_colour();
        SET_AX(a);
    }
    SET_CX(CX - 1);
    if (CX == 0) {
        light_point();
        SET_DX(BX);
        return;
    }
    imul16(CX);
    SET_AX(AX + BX);
    SET_CX(CX + 1);
    {
        uint16_t t = CX;

        SET_CX(BX);
        SET_BX(t);
    }
    SET_DX(AX);
    SET_AX(0);
    if (M16(esi + 0x1204) != 0)
        light_row_along();
}

/* 0x206f3bef -- pattern 8: the bar alone, from 0xc8 past the threshold. */
static void lights_8(void)
{
    SET_BP(0xc8);
    SET_DX(M16(V(0x2070fe5e)));
    if (DX & 0x8000)
        SET_BP(-BP);
    SET_DX(DX + BP);
    SET_AL(M8(V(0x2070fe62)));
    if (AL != 0)
        light_bar();
}

/* 0x206f3c6b, 0x206f3d0a -- the rows of patterns 2 to 5: flashing with bx
 * `flash`, seven rows across 0xc8 apart from the threshold, of five lights
 * `half` either side of the centre, and at the fifth bars from `bar_x` to
 * `bar_x2` either side. */
static void lights_rows(uint16_t flash, uint16_t half, uint16_t bar_x, uint16_t bar_x2)
{
    SET_AX(0xf016);
    SET_BX(flash);
    light_colour();
    SET_DX(M16(V(0x2070fe5e)));
    SET_BP(0xc8);
    if (DX & 0x8000)
        SET_BP(-BP);
    SET_SI(7);
    do {
        SET_DX(DX + BP);
        SET_AX(-half);
        SET_BX(half);
        SET_CX(5);
        light_row_across();
        if (SI == 3) {
            SET_AX(bar_x);
            SET_BX(bar_x2);
            SET_CX(5);
            light_row_across();
            SET_AX(-AX);
            SET_BX(-BX);
            light_row_across();
        }
        SET_SI(SI - 1);
    } while (SI != 0);
}

/* The bar after the rows: one row on (patterns 3 and 5), or back to an
 * eighth of a row short of the last but one (2 and 4). */
static void lights_bar_on(void)
{
    SET_AL(M8(V(0x2070fe62)));
    if (AL != 0) {
        SET_DX(DX + BP);
        light_bar();
    }
}

static void lights_bar_back(void)
{
    SET_AL(M8(V(0x2070fe62)));
    if (AL != 0) {
        SET_BX(sar16(BP, 3));
        SET_DX(DX + BX);
        SET_DX(DX - BP);
        SET_DX(DX - BP);
        light_bar();
    }
}

static void lights_2(void) { lights_rows(0x2000, 5, 0xffdf, 0xffe9); lights_bar_back(); }   /* 0x206f3c42 */
static void lights_3(void) { lights_rows(0x2000, 5, 0xffdf, 0xffe9); lights_bar_on(); }     /* 0x206f3c2b */
static void lights_4(void) { lights_rows(0x4000, 7, 0xffdd, 0xfff1); lights_bar_back(); }   /* 0x206f3cca */
static void lights_5(void) { lights_rows(0x4000, 7, 0xffdd, 0xfff1); lights_bar_on(); }     /* 0x206f3cf3 */

/* 0x206f3d69 -- patterns 6 and 7, 0x64 apart: its own header (0x2d, by
 * 5); red rows near the threshold (pattern 7: nine pairs), then flashing
 * rows of five to the end, with bars at the sixth from the end of the
 * first (pattern 7 the fourth too); the bar from the sixth. */
static void lights_6(void)
{
    uint16_t sixth = 0;

    SET_AX(0x2d);
    SET_BX(5);
    SET_CX(5);
    light_header();
    SET_DX(M16(V(0x2070fe5e)));
    SET_BP(0x64);
    if (DX & 0x8000)
        SET_BP(-BP);
    SET_AX(0xf00f);
    light_colour();
    SET_DX(DX + BP);
    if (M8(V(0x2070fe61)) != 7) {
        SET_AX(0xffce);
        SET_BX(0xffe2);
        SET_CX(5);
        light_row_across();
        SET_AX(-AX);
        SET_BX(-BX);
        light_row_across();
        SET_DX(DX + BP);
        SET_BX(BX + 0xa);
        SET_CX(3);
        light_row_across();
        SET_AX(-AX);
        SET_BX(-BX);
        light_row_across();
        SET_AX(0xfff6);
        SET_BX(0xa);
        SET_CX(5);
        light_row_across();
        SET_SI(0x16);
        SET_DX(DX + BP);
    } else {
        uint16_t d = DX;

        SET_SI(9);
        SET_AX(0xffce);
        SET_BX(0xffd8);
        SET_CX(3);
        do {
            light_row_across();
            SET_AX(-AX);
            SET_BX(-BX);
            light_row_across();
            SET_AX(-AX);
            SET_BX(-BX);
            SET_DX(DX + BP);
            SET_SI(SI - 1);
        } while (SI != 0);
        SET_DX(d);
        SET_SI(0x18);
    }
    SET_AX(0xf016);
    SET_BX(0x4000);
    light_colour();
    do {
        int pair = 0;

        SET_AX(0xfff6);
        SET_BX(0xa);
        SET_CX(5);
        light_row_across();
        if (SI == 0x14 && M8(V(0x2070fe61)) == 7) {
            SET_AX(0xffe0);
            SET_BX(0xffef);
            SET_CX(4);
            pair = 1;
        } else if (SI == 0xf) {
            SET_AX(0xffce);
            SET_BX(0xfff1);
            SET_CX(8);
            sixth = DX;
            pair = 1;
        }
        if (pair) {
            light_row_across();
            SET_AX(-AX);
            SET_BX(-BX);
            light_row_across();
        }
        SET_DX(DX + BP);
        SET_SI(SI - 1);
    } while (SI != 0);
    SET_BX(sixth);
    SET_AL(M8(V(0x2070fe62)));
    if (AL != 0) {
        SET_DX(BX);
        SET_BX(sar16(BP, 3));
        SET_DX(DX + BX);
        light_bar();
    }
}

/* 0x206f3eb1 -- pattern 9, 0x64 apart: white; rows along the centre and
 * either side, then rows across widening to the end.  bx the first row's
 * z. */
static void lights_9(void)
{
    uint16_t first, along;

    SET_DX(M16(V(0x2070fe5e)));
    SET_BP(0x64);
    if (DX & 0x8000)
        SET_BP(-BP);
    SET_AX(0xf015);
    light_colour();
    SET_DX(DX + BP);
    first = DX;
    SET_CX(DX);
    SET_DX(DX + BP + BP + BP);
    SET_AX(0);
    SET_BX(4);
    light_row_along();
    along = DX;
    SET_SI((BP << 2) + BP);
    SET_CX(CX + SI);
    SET_DX(DX + SI);
    light_row_along();
    SET_CX(CX + SI);
    SET_DX(DX + SI);
    SET_AX(0xfffd);
    light_row_along();
    SET_AX(-AX);
    light_row_along();
    SET_CX(CX + SI);
    SET_DX(DX + SI);
    light_row_along();
    SET_AX(-AX);
    light_row_along();
    SET_CX(CX + SI - BP);
    SET_DX(DX + SI + SI + BP);
    SET_AX(0xfffb);
    SET_BX(0xb);
    light_row_along();
    SET_AX(-AX);
    light_row_along();
    SET_AX(0);
    light_row_along();
    SET_DX(along + BP);
    SET_AX(0xffec);
    SET_BX(0x14);
    SET_CX(9);
    light_row_across();
    SET_DX(DX + SI);
    SET_AX(0xffe7);
    SET_BX(0x19);
    SET_CX(0xb);
    light_row_across();
    SET_DX(DX + SI);
    SET_AX(0xffdf);
    SET_BX(0x21);
    SET_CX(0xe);
    light_row_across();
    SET_DX(DX + SI);
    SET_AX(0xffce);
    SET_BX(0xffec);
    SET_CX(7);
    light_row_across();
    SET_AX(-AX);
    SET_BX(-BX);
    light_row_across();
    SET_DX(DX + SI);
    light_row_across();
    SET_AX(-AX);
    SET_BX(-BX);
    light_row_across();
    SET_BX(first);
}

/* 0x206f3fca -- pattern 10: pattern 9 and red rows along either side of
 * its start, eight spacings long. */
static void lights_10(void)
{
    uint16_t d;

    lights_9();
    d = DX;
    SET_DX(BX);
    SET_AX(0xf00f);
    light_colour();
    SET_BP(BP << 3);
    SET_CX(DX);
    SET_DX(DX + BP);
    SET_AX(0xffdd);
    SET_BX(9);
    light_row_along();
    SET_AX(-AX);
    light_row_along();
    SET_AX(AX - 5);
    light_row_along();
    SET_AX(-AX);
    light_row_along();
    SET_AX(AX + 5);
    light_row_along();
    SET_AX(-AX);
    light_row_along();
    SET_DX(d);
}

/* 0x206f38db -- the lights for the record at esi + 2, off the map
 * (GLOBAL +0xb5c) and unless the view's block +0x10 is 1: the header, the
 * end bars, the pattern (through 0x2070fe32), the flashers, the VASI and a
 * return; run from the header or past it, GLOBAL +0x127e 0xffff
 * meanwhile. */
static void approach_lights(void)
{
    edi = G;
    SET_AX(M16(edi + 0xce6));
    LIGHTS_EYE = AX;
    ebx = M32(edi + 0x1198);
    if (M32(ebx + 0x10) == 1)
        return;
    if (M32(edi + 0xb5c) == 0)
        return;
    for (ebx = 0x11; (int32_t)ebx >= 0; ebx--) {
        SET_AL(M8(ebx + esi + 2));
        M8(ebx + V(0x2070fe5a)) = AL;
    }
    edi = V(0x2070fe96);
    SET_AX(0xa);
    SET_BX(0xa);
    SET_CX(5);
    light_header();
    if (M8(V(0x2070fe60)) & 4)
        light_end_bars();
    SET_DX(M16(V(0x2070fe5e)));
    ebx = M8(V(0x2070fe61));
    ebx -= 1;
    if ((int32_t)ebx >= 0 && ebx <= 9)
        routine(M32(V(0x2070fe32) + ebx * 4))();
    if (M8(V(0x2070fe60)) & 0x10)
        light_sequence();
    if (M8(V(0x2070fe63)) != 0)
        vasi();
    M16(edi) = 0x22;
    edi = V(0x20710348) - edi;
    eax = M8(V(0x2070fe60)) & 1 ? V(0x2070fe82) : V(0x2070fe96);
    M32(V(0x206f9126)) = eax;
    {
        uint16_t keep = M16(G + 0x127e);

        M16(G + 0x127e) = 0xffff;
        run_program(M32(V(0x206f9126)));
        eax = G;
        M16(eax + 0x127e) = keep;
    }
}

/* 0x206ef03c -- the view kept (GLOBAL +0xce8's matrix, +0xcdc's eye, the
 * sun at +0xb28.., the angles at +0x5c8..) at 0x206ff860.., then turned by
 * pitch ax, bank bx and heading cx. */
static void view_turn_to(void)
{
    uint32_t keep_eax = eax, keep_ecx = ecx, i;

    eax = G;
    for (i = 0; i < 18; i++)
        M8(V(0x206ff860) + i) = M8(eax + 0xce8 + i);
    for (i = 0; i < 12; i++)
        M8(V(0x206ff872) + i) = M8(eax + 0xcdc + i);
    esi = eax + 0xce8;
    ecx = 0;
    M32(V(0x206ff87e)) = M32(eax + 0xb28);
    M32(V(0x206ff882)) = M32(eax + 0xb2c);
    M32(V(0x206ff886)) = M32(eax + 0xb30);
    M32(V(0x206ff88a)) = M32(eax + 0x5c8);
    M32(V(0x206ff88e)) = M32(eax + 0x5cc);
    M32(V(0x206ff892)) = M32(eax + 0x5d0);
    edx = keep_ecx;
    eax = keep_eax;
    edi = G;
    M16(edi + 0x5ca) = AX;
    M16(edi + 0x5ce) = BX;
    M16(edi + 0x5d2) = DX;
    transform_turn();
    M16(V(0x206f9180)) = 0;
    M16(V(0x206f9196)) += 1;
}

/* 0x206ef0e1 -- the view put back. */
static void view_put_back(void)
{
    uint32_t i;

    eax = G;
    for (i = 0; i < 18; i++)
        M8(eax + 0xce8 + i) = M8(V(0x206ff860) + i);
    for (i = 0; i < 12; i++)
        M8(eax + 0xcdc + i) = M8(V(0x206ff872) + i);
    esi = V(0x206ff87e);
    edi = eax + 0xce8;
    ecx = 0;
    M32(eax + 0xb28) = M32(V(0x206ff87e));
    M32(eax + 0xb2c) = M32(V(0x206ff882));
    M32(eax + 0xb30) = M32(V(0x206ff886));
    M32(eax + 0x5c8) = M32(V(0x206ff88a));
    M32(eax + 0x5cc) = M32(V(0x206ff88e));
    edx = M32(V(0x206ff892));
    M32(eax + 0x5d0) = edx;
    M16(V(0x206f9196)) -= 1;
    M16(V(0x206f9180)) = 0;
}

/* 0x206f38b0 -- +0x48: the lights for the record at esi + 2 with the view
 * turned to its heading; the counter left at esi. */
static void approach_lights_turned(void)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi;

    SET_AX(0);
    SET_BX(0);
    SET_CX(M16(esi + 2));
    view_turn_to();
    esi = keep_esi;
    approach_lights();
    view_put_back();
    M32(V(0x206f9126)) = keep_esi;
    ebx = keep_ebx, esi = keep_esi, edi = keep_edi;
}

/* 0x206e4357 -- 0x72, approach lights (0x206f38b0), off the map; on past
 * the record. */
static void op_approach_lights(void)
{
    edi = G;
    if (M32(edi + 0xb5c) != 0)
        approach_lights_turned();
    M32(V(0x206f9126)) += 0x14;
}

SERVICE_RAW(approach_lights_turned_svc, 0);
static void approach_lights_turned_svc_body(const uint32_t *arg)
{
    (void)arg;
    approach_lights_turned();
}

/* +0x4c: 0x206f38db itself. */
SERVICE_RAW(approach_lights_svc, 0);
static void approach_lights_svc_body(const uint32_t *arg)
{
    (void)arg;
    approach_lights();
}

/* ---- buildings: 0x49, .text+0x87d6..+0x89e1 and +0x14200..+0x1694a ----------------- */

/* 0x49 draws a generic building by filling in one of four built-in
 * programs (templates of 0x200 bytes from 0x20710ad0: three boxes and an
 * octagon) and running it.  A template's words, from its base t:
 *   +0, +2       the apparent width and height (the walls' texture limits)
 *   +4..         the program: +8 the sphere (centre, +0xe radius) of its
 *                0x2c, +0x10/+0x14 its detail opcode (0x5f or 0x6d) and
 *                operand, then the corners and the walls' faces. */

static void corner(uint32_t at, uint16_t x, uint16_t y, uint16_t z)
{
    M16(at) = x;
    M16(at + 2) = y;
    M16(at + 4) = z;
}

/* 0x206e97d6 -- the building's colours: with 0x206f931a 0xffff the drawing
 * state only (0x206f9125, GLOBAL +0xc90, +0xca0); else, off the map, the
 * built-in program 0x206fdb7c[min(0x206f931a, 7)] run without a context,
 * every register kept. */
static void building_colours(void)
{
    if (M16(V(0x206f931a)) == 0xffff) {
        M8(V(0x206f9125)) = 0;
        M16(G + 0xc90) = 0;
        M16(G + 0xca0) = 0;
        return;
    }
    if (M16(G + 0xdca) != 0)
        return;
    {
        uint32_t a = eax, b = ebx, c = ecx, d = edx, s = esi, i = edi, p = ebp;
        uint32_t keep_context, keep_pc;

        ebp = M16(V(0x206f931a));
        if (ebp > 7)
            ebp = 7;
        keep_context = M32(V(0x206f919c));
        M32(V(0x206f919c)) = 0;
        keep_pc = M32(V(0x206f9126));
        M32(V(0x206f9126)) = M32(V(0x206fdb7c) + ebp * 4);
        interpret();
        M32(V(0x206f9126)) = keep_pc;
        M32(V(0x206f919c)) = keep_context;
        M8(V(0x206f9125)) = 0;
        edi = G;
        M16(edi + 0xc90) = 0;
        M16(edi + 0xca0) = 0;
        eax = a, ebx = b, ecx = c, edx = d, esi = s, edi = i, ebp = p;
    }
}

/* The opening every kind shares: half the width (si) and depth (di), the
 * height (dx) times 4; the base and top points at base and base + 6; the
 * detail jump (0x5f by the height, or 0x6d by a quarter of the mean width
 * plus one, when that half is the larger); GLOBAL +0x832 0xe with the eye
 * (0x206f9184, 0x206f918c, 0x206f9190) inside, off the map; the sphere,
 * radius the largest of half the height and the half sizes.  dx the top. */
static void building_head(uint32_t t, uint32_t base)
{
    SET_SI(SI >> 1);
    SET_DI(DI >> 1);
    SET_DX(DX + DX);
    SET_DX(DX + DX);
    corner(base, AX, BX, CX);
    corner(base + 6, AX, DX + BX, CX);
    SET_BP(SI + DI);
    SET_BP(BP >> 1);
    if (BP < DX) {
        M16(t + 0x10) = 0x5f;
        M16(t + 0x14) = DX;
    } else {
        SET_BP((BP >> 1) + 1);
        M16(t + 0x10) = 0x6d;
        M16(t + 0x14) = BP;
    }
    if (M16(G + 0xdca) == 0 &&
        (int16_t)M16(V(0x206f918c)) >= (int16_t)BX &&
        (int16_t)M16(V(0x206f918c)) <= (int16_t)(uint16_t)(BX + DX) &&
        (int16_t)M16(V(0x206f9184)) >= (int16_t)(uint16_t)(AX - SI) &&
        (int16_t)M16(V(0x206f9184)) <= (int16_t)(uint16_t)(AX + SI) &&
        (int16_t)M16(V(0x206f9190)) >= (int16_t)(uint16_t)(CX - DI) &&
        (int16_t)M16(V(0x206f9190)) <= (int16_t)(uint16_t)(CX + DI))
        M16(G + 0x832) = 0xe;
    {
        uint16_t height = DX;

        SET_DX(DX >> 1);
        SET_BP(DX);
        if (BP < SI)
            SET_BP(SI);
        if (BP < DI)
            SET_BP(DI);
        SET_DX(DX + BX);
        corner(t + 8, AX, DX, CX);
        M16(t + 0xe) = BP;
        SET_DX(height + BX);
    }
}

/* The apparent width (the mean of the sizes in si and di) and, for
 * 0x206f931a 5 and 6, height (bp), at the view's scale (its block's first
 * and third dwords), doubled, to t +0 and +2; with other colours +2 the
 * height itself.  si, di and bp kept. */
static void building_size(uint32_t t)
{
    uint16_t s = SI, d = DI, p = BP;

    SET_AX(SI + DI);
    SET_AX(AX >> 1);
    ebx = M32(M32(G + 0x1198));
    apparent_size();
    SET_SI(s), SET_DI(d), SET_BP(p);
    SET_AX(AX + AX);
    M16(t) = AX;
    SET_AX(BP);
    if (M16(V(0x206f931a)) >= 5 && M16(V(0x206f931a)) <= 6) {
        ebx = M32(M32(G + 0x1198) + 8);
        apparent_size();
        SET_AX(AX + AX);
        SET_SI(s), SET_DI(d), SET_BP(p);
    }
    M16(t + 2) = AX;
}

/* A wall facing (nx, nz): its shade from the sun (GLOBAL +0xb2a, +0xb32;
 * dark when facing away, through 0x206f91bb's ambient), in 0x20 steps, as
 * u; its texture extents from half its length (si or di, at most 0x1e)
 * and half the height (bp), each cut to the apparent size at t +0/+2 (but
 * not below 4), even less one -- four (u, v) at `at`.  si, di and bp kept;
 * ax, bx, cx and dx as retail leaves them. */
static void building_wall(uint32_t t, int along_si, uint16_t nx, uint16_t nz, uint32_t at)
{
    uint16_t s = SI, d = DI, p = BP, len, limit;
    uint32_t prod;

    len = (along_si ? SI : DI) >> 1;
    SET_BP(BP >> 1);
    if (len >= 0x1e)
        len = 0x1e;
    SET_AX(nx);
    imul16(M16(G + 0xb2a));
    SET_CX(DX);
    SET_AX(nz);
    imul16(M16(G + 0xb32));
    SET_CX(CX + DX);
    SET_AX(CX + CX);
    if (!(AX & 0x8000))
        SET_AX(0);
    SET_AX(-AX);
    SET_BX(0x7fff - M16(V(0x206f91bb)));
    SET_BX(BX + BX);
    SET_AX(AX + AX);
    prod = (uint32_t)AX * BX;
    SET_AX(prod);
    SET_DX(prod >> 16);
    SET_DX(DX + M16(V(0x206f91bb)));
    SET_DX(DX + M16(V(0x206f91bb)));
    SET_DX(~DX);
    SET_AX(DH & 0xe0);
    limit = M16(t);
    if (len > limit) {
        len = limit;
        if (len < 4)
            len = 4;
    }
    limit = M16(t + 2);
    if (BP > limit) {
        SET_BP(limit);
        if (BP < 4)
            SET_BP(4);
    }
    SET_DX(((AX + len) & 0xfffe) - 1);
    SET_BP((BP & 0xfffe) - 1);
    M16(at) = AX;
    M16(at + 2) = 0;
    M16(at + 6) = DX;
    M16(at + 8) = 0;
    M16(at + 0xc) = DX;
    M16(at + 0xe) = BP;
    M16(at + 0x12) = AX;
    M16(at + 0x14) = BP;
    SET_SI(s), SET_DI(d), SET_BP(p);
}

/* Run the filled template, GLOBAL +0x1174 1 meanwhile off the map, the
 * program counter kept. */
static void building_run(uint32_t t)
{
    uint32_t keep_pc;

    if (M16(G + 0xdca) == 0)
        M16(G + 0x1174) = 1;
    keep_pc = M32(V(0x206f9126));
    run_program(t + 4);
    M32(V(0x206f9126)) = keep_pc;
    M16(G + 0x1174) = 0;
}

/* 0x206f5200, 0x206f5a30, 0x206f6290 -- the boxes, kinds 0, 1 and 2, each
 * its template (0x20710ad0 + 0x200 * kind): ax, bx, cx the position (bx
 * the base), si the width, di the depth, dx the height.  Kind 1 raises
 * the roof's ridge and its walls' texture by si; kind 2 widens the base by
 * a quarter and narrows the top by one.
 *
 * Retail's kind 1 (0x206f5b07) pushes edi before setting GLOBAL +0x832 and
 * never pops it, so with the eye inside it returns to edi's value and
 * crashes; this one goes on as the others do (docs/modules/g3d.md). */
static void building_box(uint32_t kind)
{
    uint32_t t = V(0x20710ad0) + 0x200 * kind;
    uint16_t s, d;

    building_head(t, t + 0x1de);
    s = SI, d = DI;
    if (kind == 2) {
        SET_SI(SI + (SI >> 2));
        SET_DI(DI + (DI >> 2));
    }
    corner(t + 0x1e, AX + SI, BX, CX + DI);
    corner(t + 0x24, AX - SI, BX, CX + DI);
    corner(t + 0x2a, AX - SI, BX, CX - DI);
    corner(t + 0x30, AX + SI, BX, CX - DI);
    if (kind == 2) {
        SET_SI(s), SET_DI(d);
        SET_SI(SI - (SI >> 2));
        SET_DI(DI - (DI >> 2));
    }
    corner(t + 0x36, AX + SI, DX, CX + DI);
    corner(t + 0x3c, AX - SI, DX, CX + DI);
    corner(t + 0x42, AX - SI, DX, CX - DI);
    corner(t + 0x48, AX + SI, DX, CX - DI);
    corner(t + 0xf8, AX + SI, DX, CX + DI);
    corner(t + 0x12a, AX - SI, DX, CX + DI);
    corner(t + 0x15c, AX - SI, DX, CX - DI);
    corner(t + 0x18e, AX + SI, DX, CX - DI);
    corner(t + 0x1be, AX + SI, DX, CX + DI);
    corner(t + 0x6c, AX + SI, DX, CX + DI);
    corner(t + 0x8c, AX - SI, DX, CX + DI);
    corner(t + 0xac, AX - SI, DX, CX - DI);
    corner(t + 0xcc, AX + SI, DX, CX - DI);
    if (SI < 0x10)
        SET_SI(SI + SI);
    if (DI < 0x10)
        SET_DI(DI + DI);
    SET_BP(DX & 0xfffe);
    SET_SI(SI & 0xfffe);
    SET_DI(DI & 0xfffe);
    building_size(t);
    building_wall(t, 0, 0x7fff, 0, t + 0x106);
    building_wall(t, 0, 0x8001, 0, t + 0x16a);
    building_wall(t, 1, 0, 0x7fff, t + 0x138);
    building_wall(t, 1, 0, 0x8001, t + 0x19c);
    if (kind == 1) {
        M16(t + 0x44) += SI;
        M16(t + 0x4a) += SI;
        M16(t + 0x11a) += SI >> 1;
        M16(t + 0x178) += SI >> 1;
        M16(t + 0x1aa) += SI >> 1;
        M16(t + 0x1b0) += SI >> 1;
    }
    building_run(t);
}

/* 0x206f6b00 -- the octagon, kind 3 (0x207110d0): its eight corners at
 * the half sizes and half of them again, bottom and top, and eight walls;
 * its sizes halved and four more before the walls. */
static void building_octagon(void)
{
    uint32_t t = V(0x207110d0);
    uint16_t s, d, top;

    building_head(t, t + 0x35e);
    top = DX;
    s = SI, d = DI;
    SET_DX(SI >> 1);
    SET_BP(DI >> 1);
    corner(t + 0x1e, AX + SI, BX, CX + BP);
    corner(t + 0x24, AX + DX, BX, CX + DI);
    corner(t + 0x2a, AX - DX, BX, CX + DI);
    corner(t + 0x30, AX - SI, BX, CX + BP);
    corner(t + 0x36, AX - SI, BX, CX - BP);
    corner(t + 0x3c, AX - DX, BX, CX - DI);
    corner(t + 0x42, AX + DX, BX, CX - DI);
    corner(t + 0x48, AX + SI, BX, CX - BP);
    SET_BX(top);
    corner(t + 0x4e, AX + SI, BX, CX + BP);
    corner(t + 0x54, AX + DX, BX, CX + DI);
    corner(t + 0x5a, AX - DX, BX, CX + DI);
    corner(t + 0x60, AX - SI, BX, CX + BP);
    corner(t + 0x66, AX - SI, BX, CX - BP);
    corner(t + 0x6c, AX - DX, BX, CX - DI);
    corner(t + 0x72, AX + DX, BX, CX - DI);
    corner(t + 0x78, AX + SI, BX, CX - BP);
    corner(t + 0x1a8, AX + SI, BX, CX + BP);
    corner(t + 0x1da, AX + SI, BX, CX + BP);
    corner(t + 0x20c, AX - DX, BX, CX + DI);
    corner(t + 0x23e, AX - DX, BX, CX + DI);
    corner(t + 0x270, AX - SI, BX, CX - BP);
    corner(t + 0x2a2, AX - SI, BX, CX - BP);
    corner(t + 0x2d4, AX + DX, BX, CX - DI);
    corner(t + 0x306, AX + DX, BX, CX - DI);
    corner(t + 0x336, AX + DX, BX, CX - DI);
    corner(t + 0x9c, AX + SI, BX, CX + BP);
    corner(t + 0xbc, AX + SI, BX, CX + BP);
    corner(t + 0xdc, AX - DX, BX, CX + DI);
    corner(t + 0xfc, AX - DX, BX, CX + DI);
    corner(t + 0x11c, AX - SI, BX, CX - BP);
    corner(t + 0x13c, AX - SI, BX, CX - BP);
    corner(t + 0x15c, AX + DX, BX, CX - DI);
    corner(t + 0x17c, AX + DX, BX, CX - DI);
    SET_SI(s), SET_DI(d), SET_DX(top);
    SET_SI((SI >> 1) + 4);
    SET_DI((DI >> 1) + 4);
    SET_BP(DX & 0xfffc);
    SET_SI(SI & 0xfffc);
    SET_DI(DI & 0xfffc);
    building_size(t);
    building_wall(t, 0, 0x7fff, 0, t + 0x1b6);
    building_wall(t, 1, 0x5a82, 0x5a82, t + 0x1e8);
    building_wall(t, 1, 0, 0x7fff, t + 0x21a);
    building_wall(t, 1, 0xa57e, 0x5a82, t + 0x24c);
    building_wall(t, 0, 0x8001, 0, t + 0x27e);
    building_wall(t, 1, 0xa57e, 0xa57e, t + 0x2b0);
    building_wall(t, 1, 0, 0x8001, t + 0x2e2);
    building_wall(t, 1, 0x5a82, 0xa57e, t + 0x314);
    building_run(t);
}

/* 0x206e9888, 0x206e98a2, 0x206e98bc, 0x206e98d6 -- 0x49's four kinds,
 * through 0x206fdb9c: bx, cx, dx from the operand's dx, bx, cx, 0x206f90f6
 * 1 for the boxes, the colours, the building. */
static void building_kind(uint32_t kind)
{
    uint16_t b = BX, c = CX, d = DX;

    SET_BX(d), SET_CX(b), SET_DX(c);
    if (kind != 3)
        M16(V(0x206f90f6)) = 1;
    building_colours();
    if (kind == 3)
        building_octagon();
    else
        building_box(kind);
}

static void building_0(void) { building_kind(0); }
static void building_1(void) { building_kind(1); }
static void building_2(void) { building_kind(2); }
static void building_3(void) { building_kind(3); }

/* 0x206e997d -- 0x49, a building: the eye in the object (0x206e6aa8); the
 * operand's +4 to 0x206f931c; its first word the colours (0x206f931a: the
 * low three bits, or 0xffff when negative) and the kind (bits 4 and 3, or
 * the low two when negative); position, sizes from the next six words. */
static void op_building(void)
{
    uint32_t keep_esi = esi;

    eye_in_object_turned();
    esi = keep_esi;
    SET_AX(M16(esi + 4));
    M16(V(0x206f931c)) = AX;
    SET_AX(M16(esi + 2));
    if (AX & 0x8000) {
        ebp = (uint32_t)(AX & 3) << 1;
        SET_AX(0xffff);
    } else {
        ebp = ((uint32_t)AX >> 2) & 6;
        SET_AX(AX & 7);
    }
    M16(V(0x206f931a)) = AX;
    SET_AX(M16(esi + 6));
    SET_BX(M16(esi + 0xa));
    SET_DX(M16(esi + 8));
    SET_CX(M16(esi + 0xc));
    SET_DI(M16(esi + 0x10));
    SET_SI(M16(esi + 0xe));
    M32(V(0x206f9126)) += 0x12;
    ebp <<= 1;
    routine(M32(V(0x206fdb9c) + ebp))();
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

/* 0x206e960a -- with neither of the cell's triangles drawn (0x206fdc41)
 * and the eye over it, GLOBAL +0x832 2.  The ground tiles' cells end here
 * too. */
static void grid_cell_eye(void)
{
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
    grid_cell_eye();
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

/* ---- runways: 0x42, 0x44, .text+0x1121f..+0x128a3 -------------------------------- */

/* A runway is a record of 0x40 bytes (0x52 with bit 0 of +0x1d) that fills
 * in the built-in program at 0x2070f01c, an object at the record's
 * position; the program calls back (0x8c) into the routines below, which
 * write its markings as point lists (at 0x2070f25c, 0x2070f25a the count)
 * and the surfaces over them, and its lights.  The record's fields, kept
 * from 0x2070ebb0:
 *   +2..+0x13 the position (to 0x2070f02a..)  +0x14 a heading (ebb8)
 *   +0x16 the length (ebba, at most 25000; half at ebcc)
 *   +0x18 the width (ebbc and ebf2, at most 600; half at ebce)
 *   +0x1a markings (ebbe: bit 1 numbers further in, 3 the touchdown
 *         zone from 3000 long, 5 from 28 wide, 6 the fixed distance)
 *   +0x1b the number (ebc1: 1..36, its top two bits L, R, C)
 *   +0x1c lights (ebc3: bits 0-1 the edges, 2-3 the approach)
 *   +0x1d bit 0 the colours (ebc0)       +0x1e the surface (ebc2)
 *   +0x20..+0x26 the two thresholds' displacements and overruns (ebd6..)
 *   +0x28, +0x34 an approach lighting record for each end
 * ebe8 is the eye's z along the runway (the program sets it), ebea..ebf0
 * the detail's distances (0x206f237d). */

#define RW(a) M16(V(0x2070eb00 + (a)))

static uint16_t retail_word(uint32_t at);
static uint8_t retail_byte(uint32_t off);

/* 0x206f3864 -- a fresh point list: its marks' list (0x2070efb4 back to
 * 0x2070efb8), no points, edi at the first. */
static void runway_points(void)
{
    M32(V(0x2070efb4)) = V(0x2070efb8);
    M16(V(0x2070f25a)) = 0;
    edi = V(0x2070f25c);
    M16(edi) = 0x37;
}

/* 0x206f3882 -- a mark al: a polygon of al & 0x7f points (bit 7 not a
 * surface of its own), counted. */
static void runway_mark(void)
{
    uint16_t b = BX;

    ebx = M32(V(0x2070efb4));
    M8(ebx) = AL;
    M32(V(0x2070efb4)) += 1;
    SET_BX(b);                                  /* pop bx: the high half stays */
    M16(V(0x2070f25a)) += AX & 0x7f;
}

/* 0x206f28a6 -- a point (ax, 0, bx). */
static void runway_point(void)
{
    M16(edi) = AX;
    M16(edi + 2) = 0;
    M16(edi + 4) = BX;
    edi += 6;
}

/* The list's points turned end for end (x and z negated): the routines
 * for a runway's far end draw the near end's and turn them. */
static void runway_points_turned(void)
{
    ecx = M16(V(0x2070f25a));
    if (CX == 0)
        return;
    ebx = V(0x2070f25c);
    do {
        M16(ebx) = -M16(ebx);
        M16(ebx + 4) = -M16(ebx + 4);
        ebx += 6;
    } while (--ecx != 0);
}

/* 0x206f2f43 -- a bar from z ax, bx long, x cx, dx wide, unless it is out
 * of the detail's range from the eye: near (within ebec) a quad, else two
 * points (a line: across when narrower than the runway, else along its
 * middle). */
static void runway_bar(void)
{
    uint16_t a = AX, b = BX, d = DX, p = BP;
    int narrower;

    SET_BP(AX - RW(0xe8));
    if ((int16_t)BP > (int16_t)RW(0xea) || (int16_t)BP < (int16_t)RW(0xf0))
        goto out;
    narrower = DX < RW(0xbc);
    SET_BX(BX + AX);
    SET_DX(DX + CX);
    if ((int16_t)BP <= (int16_t)RW(0xec) && (int16_t)BP >= (int16_t)RW(0xee)) {
        M16(edi) = CX;
        M16(edi + 4) = AX;
        M16(edi + 6) = DX;
        M16(edi + 0xa) = AX;
        M16(edi + 0xc) = DX;
        M16(edi + 0x10) = BX;
        M16(edi + 0x12) = CX;
        M16(edi + 0x16) = BX;
        SET_DX(0);
        M16(edi + 2) = 0;
        M16(edi + 8) = 0;
        M16(edi + 0xe) = 0;
        M16(edi + 0x14) = 0;
        edi += 0x18;
        SET_AL(4);
    } else {
        if (!narrower) {
            M16(edi) = CX;
            M16(edi + 4) = AX;
            M16(edi + 6) = DX;
            M16(edi + 0xa) = AX;
        } else {
            SET_DX(sar16(DX + CX, 1));
            M16(edi) = DX;
            M16(edi + 4) = AX;
            M16(edi + 6) = DX;
            M16(edi + 0xa) = BX;
        }
        SET_DX(0);
        M16(edi + 2) = 0;
        M16(edi + 8) = 0;
        edi += 0xc;
        SET_AL(0x82);
    }
    runway_mark();
out:
    SET_BP(p);
    SET_DX(d);
    SET_BX(b);
    SET_AX(a);
}

/* 0x206f2f2e, 0x206f2f1d, 0x206f2f0c, 0x206f2efb -- one, two, three, four
 * pairs of bars mirrored across the middle, si + dx apart. */
static void runway_bar_pair(void)
{
    uint16_t c;

    runway_bar();
    c = CX;
    SET_CX(-CX - DX);
    runway_bar();
    SET_CX(c);
}

static void runway_bar_pairs(int n)
{
    for (;;) {
        runway_bar_pair();
        if (--n == 0)
            break;
        SET_CX(CX + SI + DX);
    }
}

/* 0x206f3857 -- 0x51, colour ax. */
static void runway_colour(void)
{
    M16(edi) = 0x51;
    M16(edi + 2) = AX;
    edi += 4;
}

/* 0x206f32b8 -- with the colours (ebc0 bit 0) 0x52 and 0x51 the word at bx
 * past 0x2070ebb4 -- retail takes the address of the record's pointer
 * (lea) where it means the record, so these are words of its own state. */
static void runway_colours(void)
{
    if (M8(V(0x2070ebc0)) & 1) {
        ebx = BX;
        SET_AX(M16(V(0x2070ebb4) + ebx));
        M16(edi) = 0x52;
        M16(edi + 2) = AX;
        M16(edi + 4) = 0x51;
        M16(edi + 6) = AX;
        edi += 8;
    }
}

/* 0x206f3258 -- the surfaces over the list: for each mark a polygon (0x05,
 * then 0x0f and 0x10s through table points from 0xd3, and 0x08), or for
 * bit 7 the points without a surface of their own. */
static void runway_surfaces(void)
{
    esi = V(0x2070efb8);
    SET_BX(0xd3);
    while (esi != M32(V(0x2070efb4))) {
        uint16_t c = CX;

        SET_AL(M8(esi));
        esi += 1;
        SET_CX(AX);
        ecx &= 0x7f;
        if (AL != 0) {
            int own = !(AL & 0x80);

            if (own) {
                M16(edi) = 5;
                edi += 2;
            }
            SET_AX(0xf);
            do {
                M16(edi) = AX;
                M16(edi + 2) = BX;
                SET_BX(BX + 1);
                edi += 4;
                SET_AX(0x10);
            } while (--ecx != 0);
            if (own) {
                M16(edi) = 8;
                edi += 2;
            }
        }
        SET_CX(c);
    }
}

/* The list's end: the colours (bx), the surfaces and a return. */
static void runway_end(uint16_t colour_at)
{
    SET_BX(colour_at);
    runway_colours();
    runway_surfaces();
    M16(edi) = 0x22;
}

/* 0x206f271d -- an arrow at (ax, bx) of half width ebc4, length ebc8: its
 * point alone (a triangle, mark 0x83) when bp (the last bar's bytes,
 * negated) is at least -12, else with its shaft (ebc6, ebca) as two
 * quads. */
static void runway_arrow(void)
{
    uint16_t a = AX, b = BX;

    SET_AX(AX - RW(0xc4));
    SET_BX(BX - RW(0xc8));
    M16(V(0x2070ec02)) = AX;
    M16(V(0x2070ec04)) = BX;
    SET_AX(AX + RW(0xc4));
    SET_BX(BX + RW(0xc8));
    M16(V(0x2070ec06)) = AX;
    M16(V(0x2070ec08)) = BX;
    SET_AX(AX + RW(0xc4));
    SET_BX(BX - RW(0xc8));
    M16(V(0x2070ec0a)) = AX;
    M16(V(0x2070ec0c)) = BX;
#define PT(at) (SET_AX(M16(V(at))), SET_BX(M16(V((at) + 2))), runway_point())
    if ((int16_t)BP >= (int16_t)0xfff4) {
        PT(0x2070ec02);
        PT(0x2070ec06);
        PT(0x2070ec0a);
        SET_AL(0x83);
        runway_mark();
    } else {
        SET_AX(AX - 2);
        M16(V(0x2070ec0e)) = AX;
        M16(V(0x2070ec10)) = BX;
        SET_AX(AX - RW(0xc6));
        SET_BX(BX + RW(0xca));
        M16(V(0x2070ec12)) = AX;
        M16(V(0x2070ec14)) = BX;
        SET_AX(AX - RW(0xc6));
        SET_BX(BX - RW(0xca));
        M16(V(0x2070ec16)) = AX;
        M16(V(0x2070ec18)) = BX;
        PT(0x2070ec06);
        PT(0x2070ec12);
        PT(0x2070ec16);
        PT(0x2070ec02);
        SET_AL(4);
        runway_mark();
        PT(0x2070ec06);
        PT(0x2070ec12);
        PT(0x2070ec0e);
        PT(0x2070ec0a);
        SET_AL(4);
        runway_mark();
    }
    SET_BX(b);
    SET_AX(a);
}

/* 0x206f253a -- an end's threshold (ax its displacement, ebe4): the bar
 * across; when it was drawn near and the runway is wider than 20, the
 * displaced threshold's arrows (sized by the width, or 12 by 45 from 100
 * wide); then up to seven arrows back along the displacement, each at a
 * bar's end. */
static void runway_threshold(void)
{
    uint32_t keep_edi;

    RW(0xe4) = AX;
    runway_points();
    SET_DX(RW(0xbc));
    SET_CX(-(DX >> 1));
    SET_AX(-(RW(0xcc) - RW(0xe4)));
    SET_BX(0xfff6);
    ebp = edi;
    runway_bar();
    SET_AX(RW(0xe4) + 0x32);
    SET_BX(0xc8);
    RW(0xe0) = BX;
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    div16(BX);
    if (AX > 7) {
        SET_AX(RW(0xe4));
        SET_DX((int16_t)AX < 0 ? 0xffff : 0);
        SET_BX(7);
        div16(BX);
        RW(0xe0) = AX;
        SET_AX(BX);
    }
    SET_SI(AX);
    ebp -= edi;
    if ((int32_t)ebp < -12) {
        SET_AX(RW(0xbc) >> 2);
        if ((int16_t)AX > 5) {
            SET_AX(AX - 1);
            if ((int16_t)AX < 0x18) {
                uint16_t d = AX;
                uint32_t p;

                SET_AX(AX >> 1);
                RW(0xc4) = AX;
                SET_AX(AX - 2);
                RW(0xc6) = AX;
                SET_DX(d);
                SET_AX(0);
                SET_CX(0x18);
                div16(CX);
                SET_CX(0x2d);
                p = (uint32_t)AX * CX;
                SET_AX(p);
                SET_DX(p >> 16);
                if ((int16_t)DX <= 0xa)
                    goto back;
                RW(0xc8) = DX;
                SET_DX(DX - 5);
                RW(0xca) = DX;
            } else {
                RW(0xc4) = 0xc;
                RW(0xc6) = 0xa;
                RW(0xc8) = 0x2d;
                RW(0xca) = 0x28;
            }
            SET_BX(-(RW(0xcc) - RW(0xe4)) - 0xf);
            SET_AX(RW(0xce));
            SET_CX(AX >> 2);
            SET_AX(-AX + CX);
            runway_arrow();
            SET_CX(CX + CX);
            SET_AX(AX + CX);
            runway_arrow();
            SET_AX(-AX);
            runway_arrow();
            SET_AX(AX + CX);
            runway_arrow();
        }
    }
back:
    if (SI != 0) {
        RW(0xc4) = 0xc;
        RW(0xc6) = 0xa;
        RW(0xc8) = 0x2d;
        RW(0xca) = 0x28;
        SET_AX(-(RW(0xcc) - RW(0xe4)) - 0x64);
        do {
            uint16_t z = AX;

            SET_AX(AX - 0x28);
            SET_BX(0xffb0);
            SET_CX(AX + BX + RW(0xcc));
            if (CX & 0x8000)
                SET_BX(BX - CX);
            SET_CX(0xffff);
            SET_DX(2);
            keep_edi = edi;
            runway_bar();
            ebp = keep_edi;
            SET_BX(z);
            ebp -= edi;
            if (ebp != 0) {
                SET_AX(0);
                runway_arrow();
            }
            SET_AX(BX - RW(0xe0));
            SET_SI(SI - 1);
        } while (SI != 0);
    }
    runway_end(0x4e);
}

/* 0x206f24f9 / 0x206f2505 -- the near and far thresholds. */
static void runway_threshold_near(void)
{
    SET_AX(RW(0xd6));
    runway_threshold();
}

static void runway_threshold_far(void)
{
    SET_AX(RW(0xda));
    RW(0xe8) = -RW(0xe8);
    runway_threshold();
    RW(0xe8) = -RW(0xe8);
    runway_points_turned();
}

/* 0x206f2a11 -- a point at (bx, ax) and its mirror (-bx, ax) 0x18 bytes
 * on. */
static void runway_point_mirrored(void)
{
    uint16_t b = BX;

    M16(edi) = BX;
    M16(edi + 4) = AX;
    SET_BX(-BX);
    M16(edi + 0x18) = BX;
    M16(edi + 0x1c) = AX;
    SET_BX(0);
    M16(edi + 2) = 0;
    M16(edi + 0x1a) = 0;
    SET_BX(b);
    edi += 6;
}

/* 0x206f29c2 -- the threshold's stripes either side of a bar drawn near:
 * two quads, mirrored (the mirrors 0x18 bytes on, which a 16-bit add steps
 * over: retail adds to di). */
static void runway_threshold_stripes(void)
{
    uint16_t a = AX;

    SET_AX(AX + BX);
    SET_BX(sar16(CX, 2));
    SET_CX(CX + 5);
    SET_DX(CX - BX);
    SET_BX(BX - 0xa);
    runway_point_mirrored();
    SET_BX(BX + 0xa);
    runway_point_mirrored();
    SET_AX(AX + DX);
    SET_BX(CX);
    runway_point_mirrored();
    SET_AX(AX + 0xa);
    runway_point_mirrored();
    SET_DI(DI + 0x18);
    SET_AL(4);
    runway_mark();
    runway_mark();
    SET_AX(a);
}

/* 0x206f2a36 -- a centreline dash at x ax, z bx, within the detail's
 * range: near two quads (the arrow-shaped dash), else a triangle; cut at
 * the far threshold (ebe6). */
static void runway_dash(void)
{
    uint16_t a = AX, b = BX, d = DX;

    SET_DX(sar16(AX, 1) + BX - RW(0xe8));
    if ((int16_t)DX > (int16_t)RW(0xea) || (int16_t)DX < (int16_t)RW(0xf0))
        goto out;
    SET_DX(DX << 1);
    SET_BX(BX + AX);
    SET_CX(BX - RW(0xe6));
    if (CX & 0x8000) {
        SET_AX(AX - CX);
        SET_BX(BX - CX);
    }
    M16(V(0x2070ec02)) = AX;
    M16(V(0x2070ec04)) = BX;
    {
        uint16_t t = AX;

        SET_BX(BX - AX);
        SET_AX(0);
        M16(V(0x2070ec06)) = AX;
        M16(V(0x2070ec08)) = BX;
        SET_AX(t);
    }
    SET_AX(-AX);
    SET_BX(BX - AX);
    M16(V(0x2070ec0a)) = AX;
    M16(V(0x2070ec0c)) = BX;
    if ((int16_t)DX > (int16_t)RW(0xec) || (int16_t)DX < (int16_t)RW(0xee)) {
        PT(0x2070ec02);
        PT(0x2070ec06);
        PT(0x2070ec0a);
        SET_AL(0x83);
        SET_CL(AL);
        runway_mark();
        goto out;
    }
    SET_BX(BX - 0xa);
    if (CX & 0x8000) {
        SET_BX(BX + 0xa);
        SET_AX(AX - 0xa);
    }
    M16(V(0x2070ec0e)) = AX;
    M16(V(0x2070ec10)) = BX;
    {
        uint16_t t = AX;

        SET_BX(BX + AX);
        SET_AX(0);
        M16(V(0x2070ec12)) = AX;
        M16(V(0x2070ec14)) = BX;
        SET_AX(t);
    }
    SET_AX(-AX);
    SET_BX(BX + AX);
    M16(V(0x2070ec16)) = AX;
    M16(V(0x2070ec18)) = BX;
    PT(0x2070ec06);
    PT(0x2070ec12);
    PT(0x2070ec16);
    PT(0x2070ec02);
    SET_AL(4);
    runway_mark();
    PT(0x2070ec06);
    PT(0x2070ec12);
    PT(0x2070ec0e);
    PT(0x2070ec0a);
    SET_AL(4);
    runway_mark();
    SET_CL(6);
out:
    SET_DX(d);
    SET_BX(b);
    SET_AX(a);
}
#undef PT

/* 0x206f28fa -- an end's threshold bar and stripes, and the centreline
 * back from it (bx its overrun, ebe4): dashes 0xc8 apart (0x64 near the
 * eye), at most ten. */
static void runway_centreline(void)
{
    uint32_t keep_esi;

    RW(0xe4) = BX;
    SET_AX(-RW(0xcc));
    RW(0xe2) = AX;
    SET_AX(AX - BX);
    RW(0xe6) = AX;
    runway_points();
    SET_CX(0xc8);
    SET_AX(AX - RW(0xe8));
    if ((int16_t)AX <= 0x578 && (int16_t)AX >= (int16_t)0xf830)
        SET_CX(sar16(CX, 1));
    SET_AX(BX + 0xa);
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    div16(CX);
    if (AX > 0xa)
        SET_AX(0xa);
    RW(0xe0) = CX;
    SET_BP(AX);
    SET_DX(RW(0xbc));
    SET_CX(-(DX >> 1));
    SET_AX(RW(0xe2));
    SET_BX(0xfff6);
    keep_esi = edi;
    runway_bar();
    esi = keep_esi - edi;
    if (esi != 0)
        runway_threshold_stripes();
    if (BP != 0) {
        SET_BX(AX);
        SET_AX(-(RW(0xce) - 5));
        SET_BX(BX - 0x50);
        do {
            runway_dash();
            SET_BX(BX - RW(0xe0));
            SET_BP(BP - 1);
        } while (BP != 0);
    }
    runway_end(0x50);
}

/* 0x206f28b7 / 0x206f28c4 -- the near and far centrelines. */
static void runway_centreline_near(void)
{
    SET_BX(RW(0xd8));
    runway_centreline();
}

static void runway_centreline_far(void)
{
    SET_BX(RW(0xdc));
    RW(0xe8) = -RW(0xe8);
    runway_centreline();
    RW(0xe8) = -RW(0xe8);
    runway_points_turned();
}

/* 0x206f2c8e -- ax times bx, times eight but for the last carry: dx. */
static void runway_scaled(void)
{
    uint32_t p = (uint32_t)AX * BX;
    uint32_t c;

    SET_AX(p);
    SET_DX(p >> 16);
    c = AX >> 15;
    SET_AX(AX + AX);
    SET_DX(DX + DX + c);
    c = AX >> 15;
    SET_AX(AX + AX);
    SET_DX(DX + DX + c);
    c = AX >> 15;
    SET_AX(AX + AX);
    SET_DX(DX + c);
}

/* 0x206f2bfc -- the touchdown markings at both ends: four pairs of bars
 * 0x82 long, 0x19 in from each threshold, sized by the width over 600 (a
 * divide the host repairs at 600 itself). */
static void runway_touchdown(void)
{
    runway_points();
    SET_DX(RW(0xbc));
    SET_AX(0);
    SET_BX(0x258);
    div16(BX);
    SET_BX(AX);
    SET_AX(7);
    runway_scaled();
    SET_CX(-(RW(0xce) - DX));
    SET_AX(5);
    runway_scaled();
    SET_SI(DX);
    SET_AX(0xa);
    runway_scaled();
    SET_AX(-(RW(0xcc) - RW(0xd6)) + 0x19);
    SET_BX(0x82);
    {
        uint16_t c = CX;

        runway_bar_pairs(4);
        SET_CX(c);
    }
    SET_AX(-(AX - RW(0xd6)) - RW(0xda));
    SET_BX(-BX);
    runway_bar_pairs(4);
    runway_end(0x48);
}

/* 0x206f2ca5 -- the aiming point at both ends, 500 in: three pairs of
 * bars 0x4b long (narrower below 90 wide). */
static void runway_aiming(void)
{
    runway_points();
    SET_AX(-(RW(0xcc) - RW(0xd6)) + 0x1f4);
    SET_BX(0x4b);
    SET_CX(-(RW(0xce) - 9));
    SET_DX(6);
    SET_SI(5);
    if (RW(0xbc) < 0x5a) {
        SET_CX(CX - 3);
        SET_DX(DX - 1);
    }
    {
        uint16_t c = CX;

        runway_bar_pairs(3);
        SET_CX(c);
    }
    SET_AX(-(AX - RW(0xd6)) - RW(0xda));
    SET_BX(-BX);
    runway_bar_pairs(3);
    runway_end(0x4a);
}

/* 0x206f2e1c -- two pairs while the count (ebde) is above 2, else one. */
static void runway_zone_pairs(void)
{
    uint16_t c = CX;

    runway_bar_pairs(M16(V(0x2070ebde)) > 2 ? 2 : 1);
    SET_CX(c);
}

/* 0x206f2d1e -- the fixed-distance marks (ebbe bit 3: a pair 1000 in,
 * narrower below 95 wide) and the touchdown zone (bit 6: from 1500 in,
 * every 500 while 450 are left, at most four, the first two with two
 * pairs each). */
static void runway_zone(void)
{
    runway_points();
    if (M8(V(0x2070ebbe)) & 8) {
        SET_AX(-(RW(0xcc) - RW(0xd6)) + 0x3e8);
        SET_BX(0x96);
        SET_CX(-(RW(0xce) - 9));
        SET_DX(0x1e);
        if (RW(0xbc) < 0x5f)
            SET_DX(0x14);
        runway_bar_pair();
        SET_AX(-(AX - RW(0xd6)) - RW(0xda));
        SET_BX(-BX);
        runway_bar_pair();
    }
    SET_BP(0x40);
    if (M8(V(0x2070ebbe)) & 0x40) {
        SET_AX(RW(0xcc) - 0x5dc);
        SET_BP(AX);
        SET_AX(AX - RW(0xd6));
        SET_BP(BP - RW(0xda));
        SET_AX(-AX);
        SET_BX(0x4b);
        SET_CX((RW(0xbc) >> 3) - RW(0xce));
        SET_DX(6);
        SET_SI(5);
        M16(V(0x2070ebde)) = 4;
        do {
            uint16_t t;

            runway_zone_pairs();
            t = BP, SET_BP(AX), SET_AX(t);
            SET_BX(-BX);
            runway_zone_pairs();
            SET_BX(-BX);
            t = BP, SET_BP(AX), SET_AX(t);
            SET_AX(AX + 0x1f4);
            SET_BP(BP - 0x1f4);
            if ((int16_t)(uint16_t)(BP - AX) < 0x1c2)
                break;
            M16(V(0x2070ebde)) -= 1;
        } while (M16(V(0x2070ebde)) != 0);
    }
    runway_end(0x4c);
}

/* 0x206f2e37 -- the side stripes' count (ebd0: the length less both
 * displacements and 660 over 200) and step (ebd2), from 370 in (ebd4);
 * divides the host repairs. */
static void runway_stripes_setup(void)
{
    uint16_t a;

    SET_AX(RW(0xba) - RW(0xd6) - RW(0xda));
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    SET_AX(AX - 0x294);
    a = AX;
    SET_BX(0xc8);
    div16(BX);
    RW(0xd0) = AX;
    SET_BX(AX);
    SET_AX(a);
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    div16(BX);
    RW(0xd2) = AX;
    SET_AX(-(RW(0xcc) - RW(0xd6) - 0x172));
    RW(0xd4) = AX;
}

/* 0x206f2ea1 -- the side stripes, 0x78 long and 4 wide down the middle,
 * on from where the last frame left them (ebd4, ebd0) while the list has
 * room for 0x50 points. */
static void runway_stripes(void)
{
    runway_points();
    SET_AX(RW(0xd4));
    SET_BX(0x78);
    SET_CX(0xfffe);
    SET_DX(4);
    if (RW(0xd0) != 0) {
        do {
            runway_bar();
            SET_AX(AX + RW(0xd2));
            RW(0xd0) -= 1;
            if (RW(0xd0) == 0)
                break;
        } while (M16(V(0x2070f25a)) < 0x50);
    }
    RW(0xd4) = AX;
    runway_end(0x44);
}

/* 0x206f31d7 -- glyph esi at x bx, z bp: its count of triangles (its
 * first byte), each three points of signed bytes. */
static void runway_glyph(void)
{
    uint32_t i;

    ecx = M8(esi);
    esi += 2;
    do {
        for (i = 0; i < 3; i++) {
            SET_AX((uint16_t)(int16_t)(int8_t)M8(esi) + BX);
            SET_DX(AX);
            SET_AX((uint16_t)(int16_t)(int8_t)M8(esi + 1) + BP);
            M16(edi) = DX;
            M16(edi + 2) = 0;
            M16(edi + 4) = AX;
            esi += 2;
            edi += 6;
        }
        SET_AX(3);
        runway_mark();
    } while (--ecx != 0);
}

/* 0x206f31ba -- glyph al (from 0x2070ec2a) centred. */
static void runway_glyph_centred(void)
{
    SET_AX((int16_t)(int8_t)AL);
    esi = AX;
    esi = M32(V(0x2070ec2a) + esi * 4);
    SET_AL(M8(esi + 1) >> 1);
    SET_AX(-AX);
    SET_BX(AX);
    runway_glyph();
}

/* 0x206f30ad -- the number al with designator ah (1 L, 2 R, 3 C) at the
 * end bp in (further in without ebbe bit 1, and with a designator), in
 * the detail's range: two digits side by side (11 wider) or one (a lone
 * 1 is glyph 0xd), the designator a row before (or after, beyond the
 * runway's middle, L and R swapped); narrow runways (60 and less) have
 * their glyphs halved across. */
static void runway_number(void)
{
    SET_BP(-BP + 0x104);
    if (!(M8(V(0x2070ebbe)) & 2)) {
        SET_BP(BP - 0xf0);
        if (AH != 0)
            SET_BP(BP + 0x50);
    }
    SET_BX(BP - RW(0xe8));
    if ((int16_t)BX > (int16_t)RW(0xec) || (int16_t)BX < (int16_t)RW(0xee))
        return;
    {
        uint16_t number = AX;

        SET_AH(0xff);
        do {
            SET_AH(AH + 1);
            SET_AL(AL - 0xa);
        } while (!(AL & 0x80));
        SET_AL(AL + 0xa);
        if (AH != 0) {
            uint16_t digits = AX, cxw;
            uint32_t units;

            ebx = AL;
            ebx = M32(V(0x2070ec2a) + ebx * 4);
            units = ebx;
            SET_CH(M8(ebx + 1));
            ebx = AH;
            esi = M32(V(0x2070ec2a) + ebx * 4);
            SET_CL(M8(esi + 1));
            SET_CL(CL + CH);
            SET_CL(CL + 0xf);
            if (AX == 0x101)
                SET_CL(CL + 0xd);
            SET_CL(CL >> 1);
            SET_BL(CL);
            SET_BX(-BX);
            SET_CL(CL - CH);
            cxw = CX;
            SET_AL(AH);
            runway_glyph();
            SET_AX(cxw);
            SET_AX((int16_t)(int8_t)AL);
            SET_BX(AX);
            esi = units;
            SET_AX(digits);
            runway_glyph();
        } else {
            if (AL == 1)
                SET_AL(0xd);
            runway_glyph_centred();
        }
        SET_AX(number);
    }
    if (AH != 0) {
        SET_AL(AH + 9);
        SET_BP(BP - 0x50);
        if (!(BP & 0x8000)) {
            SET_BP(BP + 0xa0);
            if (AL == 0xa)
                SET_AL(0xb);
            else if (AL == 0xb)
                SET_AL(0xa);
        }
        runway_glyph_centred();
    }
    runway_end(0x46);
    if (RW(0xbc) <= 0x3c) {
        ecx = M16(V(0x2070f25a));
        ebx = V(0x2070f25c);
        do {
            M16(ebx) = sar16(M16(ebx), 1);
            ebx += 6;
        } while (--ecx != 0);
    }
}

/* 0x206f3016 / 0x206f3044 -- the near end's number, and the far end's (the
 * reciprocal, L and R swapped, turned end for end). */
static void runway_number_near(void)
{
    runway_points();
    SET_AX(RW(0xcc) - RW(0xd6));
    SET_BP(AX);
    SET_AL(M8(V(0x2070ebc1)));
    SET_BL(AL);
    SET_AH(0);
    SET_AX(AX << 2);
    SET_AL(BL);
    SET_AL(AL & 0x3f);
    runway_number();
}

static void runway_number_far(void)
{
    runway_points();
    SET_AX(RW(0xcc) - RW(0xda));
    SET_BP(AX);
    SET_AL(M8(V(0x2070ebc1)));
    SET_BL(AL);
    SET_AH(0);
    SET_AX(AX << 2);
    if (AH != 0 && AH != 3)
        SET_AH(AH ^ 3);
    SET_AL(BL);
    SET_AL(AL & 0x3f);
    SET_AL(AL + 0x12);
    if (AL > 0x24)
        SET_AL(AL - 0x24);
    RW(0xe8) = -RW(0xe8);
    runway_number();
    RW(0xe8) = -RW(0xe8);
    runway_points_turned();
}

/* 0x206f333f -- an end's approach lights: the record's twelve bytes at
 * esi + bx to 0x2070ebf6 after its z (ax, to ebf4), bit 0 of its flags
 * (no header) cleared with a displaced threshold (cx), through 0x72's
 * lights with the record at 0x2070ebf0 (its heading the detail's range,
 * its width the runway's). */
static void runway_end_lights(void)
{
    uint32_t keep_esi = esi;

    esi += ebx;
    RW(0xf4) = AX;
    ebx = 0xb;
    do {
        SET_AL(M8(ebx + esi));
        M8(ebx + V(0x2070ebf6)) = AL;
        SET_BX(BX - 1);
    } while (!(BX & 0x8000));
    if (CX != 0) {
        SET_AL(AL & 0xfe);
        M8(V(0x2070ebf6)) = AL;
    }
    esi = V(0x2070ebee);
    approach_lights();
    esi = keep_esi;
}

/* 0x206f32e7 -- both ends' approach lights (the record's +0x28 and +0x34),
 * unless the view's block +0x10 is 1, every register kept. */
static void runway_approach_lights(void)
{
    edi = G;
    ebx = M32(edi + 0x1198);
    if (M32(ebx + 0x10) == 1)
        return;
    {
        uint32_t a = eax, b = ebx, c = ecx, d = edx, s = esi, i = edi, p = ebp;

        esi = M32(V(0x2070ebb4));
        if (M8(esi + 0x28) != 0) {
            SET_AX(-RW(0xcc));
            ebx = 0x28;
            SET_CX(RW(0xd6));
            runway_end_lights();
        }
        if (M8(esi + 0x34) != 0) {
            SET_AX(RW(0xcc));
            ebx = 0x34;
            SET_CX(RW(0xda));
            runway_end_lights();
        }
        eax = a, ebx = b, ecx = c, edx = d, esi = s, edi = i, ebp = p;
    }
}

/* 0x206f382e -- 0x35, cx lights from (ax, dx) to (bx, dx). */
static void runway_light_row(void)
{
    M16(edi) = 0x35;
    M16(edi + 2) = AX;
    M16(edi + 4) = 0;
    M16(edi + 6) = DX;
    M16(edi + 8) = BX;
    M16(edi + 0xa) = 0;
    M16(edi + 0xc) = DX;
    M16(edi + 0xe) = CX;
    edi += 0x10;
}

/* 0x206f37f5 -- a bar of lights across at z dx, x ax to bx and mirrored, cx
 * each, in colour si when beyond the eye (ebe8), else bp; none for 0. */
static void runway_light_bar(void)
{
    uint16_t a = AX, b;

    SET_AX(SI);
    if (!((int16_t)DX > (int16_t)RW(0xe8)))
        SET_AX(BP);
    if (AX == 0) {
        SET_AX(a);
        return;
    }
    runway_colour();
    SET_AX(a);
    runway_light_row();
    b = BX;
    SET_AX(-AX);
    SET_BX(-BX);
    runway_light_row();
    SET_BX(b);
    SET_AX(a);
}

/* 0x206f37bd -- a row along at x dx from z ax to bx, cx lights (a light
 * for one). */
static void runway_light_line(void)
{
    M16(edi + 2) = DX;
    M16(edi + 4) = 0;
    M16(edi + 6) = AX;
    if (CX == 1) {
        M16(edi) = 0x37;
        edi += 8;
        return;
    }
    M16(edi) = 0x35;
    M16(edi + 8) = DX;
    M16(edi + 0xa) = 0;
    M16(edi + 0xc) = BX;
    M16(edi + 0xe) = CX;
    edi += 0x10;
}

/* 0x206f378d -- the edge rows in colour si (none for 0): along the middle
 * for dx 0, else both edges (the half width + 7). */
static void runway_light_edges(void)
{
    if (SI == 0)
        return;
    {
        uint16_t a = AX;

        SET_AX(SI);
        runway_colour();
        SET_AX(a);
    }
    if (DX != 0) {
        SET_DX(RW(0xce) + 7);
        runway_light_line();
        SET_DX(-DX);
    }
    runway_light_line();
}

/* 0x206f3773 / 0x206f3780 -- in the near and far colours (0x2070fe1c,
 * 0x2070fe1e). */
static void runway_light_edges_near(void)
{
    SET_SI(M16(V(0x2070fe1c)));
    runway_light_edges();
}

static void runway_light_edges_far(void)
{
    SET_SI(M16(V(0x2070fe1e)));
    runway_light_edges();
}

/* 0x206f371a -- a row of lights from z ax to bx at about si apart: the
 * colours (cx, dx), the count (cx, at least two), the step (ebe0, dx), the
 * ends moved in by half the remainder plus a step; bp the eye. */
static void runway_light_spacing(void)
{
    uint16_t a, b;

    M16(V(0x2070fe1c)) = CX;
    M16(V(0x2070fe1e)) = DX;
    a = AX, b = BX;
    SET_AX(AX - BX);
    SET_BX(AX);
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    div16(SI);
    if (AX == 0)
        SET_AX(1);
    SET_CX(AX + 1);
    SET_AX(BX);
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    div16(CX);
    SET_BP(sar16(DX, 1) + AX);
    RW(0xe0) = AX;
    SET_DX(AX);
    SET_BX(b);
    SET_AX(a);
    SET_AX(AX - BP);
    SET_BX(BX + BP);
    SET_CX(CX - 1);
    SET_BP(RW(0xe8));
}

/* 0x206f36d0 -- the row split at the eye (bp): all far, all near, or the
 * steps either side in their colours. */
static void runway_light_split(void)
{
    if (!((int16_t)AX > (int16_t)BP)) {
        runway_light_edges_far();
        return;
    }
    if (!((int16_t)BX < (int16_t)BP)) {
        runway_light_edges_near();
        return;
    }
    {
        uint16_t b = BX, c = CX;

        SET_SI(RW(0xe0));
        do {
            SET_CX(CX - 1);
            SET_BX(BX + SI);
        } while ((int16_t)BX < (int16_t)BP);
        runway_light_edges_near();
        SET_SI(c - CX);
        SET_CX(SI);
        SET_AX(BX - RW(0xe0));
        SET_BX(b);
        runway_light_edges_far();
    }
}

/* 0x206f36c1 */
static void runway_light_run(void)
{
    SET_SI(0xb4);
    runway_light_spacing();
    runway_light_split();
}

/* 0x206f3370 -- the runway's lights (GLOBAL +0x1172 bits 1-2), a program
 * at 0x2070f262: the approach end's centreline (ebc3 bits 2-3) and edges
 * (bits 0-1, their colour from 0x2070fe12), touchdown-zone lights split
 * at the eye, the thresholds' and overruns' bars. */
static void runway_lights(void)
{
    edi = G;
    {
        int on = (M16(edi + 0x1172) & 6) != 0;

        edi = V(0x2070f262);
        if (!on)
            goto end;
    }
    M16(V(0x2070fe20)) = 0;
    if (M8(V(0x2070ebc3)) & 0xc) {
        SET_AX(RW(0xcc) - 0x19);
        SET_BX(-AX);
        SET_CX(0xf014);
        SET_DX(0xf015);
        M16(V(0x2070fe1a)) = CX;
        SET_SI(0x32);
        runway_light_spacing();
        SET_BP(0xf015);
        M16(V(0x2070fe1c)) = BP;
        M16(V(0x2070fe1a)) = BP;
        SET_BP(AX - 0x76c);
        SET_DX(0);
        runway_light_split();
    }
    if (!(M8(V(0x2070ebc3)) & 3))
        goto end;
    ebx = M8(V(0x2070ebc3));
    SET_BX(BX & 3);
    ebx -= 1;
    ebx += ebx;
    SET_DX(M16(V(0x2070fe12) + ebx));
    M16(V(0x2070fe18)) = DX;
    SET_AX(RW(0xcc));
    SET_BX(-AX);
    SET_AX(AX - RW(0xda));
    SET_BX(BX + RW(0xd6));
    SET_CX(0xf014);
    M16(V(0x2070fe1a)) = CX;
    SET_SI(0xb4);
    runway_light_spacing();
    if (M8(V(0x2070ebbe)) & 0x40) {
        if ((int16_t)AX > (int16_t)BP) {
            if (!((int16_t)BX < (int16_t)BP))
                goto edges;
            SET_DX(AX + RW(0xda) - 0x76c);
            if (!((int16_t)DX < (int16_t)BP))
                SET_BP(DX);
            M16(V(0x2070fe1e)) = 0;
            runway_light_split();
            SET_BP(RW(0xe8));
        }
        SET_DX(BX - RW(0xd6) + 0x76c);
        if (!((int16_t)DX > (int16_t)BP))
            SET_BP(DX);
        SET_SI(M16(V(0x2070fe18)));
        M16(V(0x2070fe1c)) = SI;
        M16(V(0x2070fe1e)) = 0xf014;
        runway_light_split();
    } else {
        SET_BP(M16(V(0x2070fe18)));
        M16(V(0x2070fe1c)) = BP;
        M16(V(0x2070fe1a)) = BP;
edges:
        SET_BP(AX - 0x76c);
        runway_light_split();
    }
    if (RW(0xda) != 0) {
        int beyond;

        SET_DX(RW(0xcc) - RW(0xda));
        SET_AX(RW(0xce) + 7);
        SET_BX(AX + 0x1e);
        SET_CX(4);
        SET_SI(0);
        SET_BP(0xf010);
        runway_light_bar();
        SET_AX(AX - 0xa);
        SET_BX(BX - 0xa);
        SET_DX(DX + RW(0xda));
        SET_SI(0xf00f);
        SET_BP(SI);
        runway_light_bar();
        SET_BX(DX);
        SET_AX(BX);
        SET_BX(BX - RW(0xda));
        SET_CX(M16(V(0x2070fe1a)));
        SET_DX(0xf00f);
        beyond = (int16_t)BX < (int16_t)RW(0xe8);
        SET_SI(0xb4);
        runway_light_spacing();
        if (!beyond) {
            SET_BX(BX - DX);
            SET_CX(CX + 1);
        }
        runway_light_split();
    }
    if (RW(0xd6) != 0) {
        int beyond;

        SET_DX(-(RW(0xcc) - RW(0xd6)));
        SET_AX(RW(0xce) + 7);
        SET_BX(AX + 0x1e);
        SET_CX(4);
        SET_SI(0xf010);
        SET_BP(0);
        runway_light_bar();
        SET_AX(AX - 0xa);
        SET_BX(BX - 0xa);
        SET_DX(DX - RW(0xd6));
        SET_SI(0xf00f);
        SET_BP(SI);
        runway_light_bar();
        SET_BX(DX);
        SET_AX(BX);
        SET_AX(AX + RW(0xd6));
        SET_CX(0xf00f);
        SET_DX(M16(V(0x2070fe1a)));
        beyond = (int16_t)AX > (int16_t)RW(0xe8);
        SET_SI(0xb4);
        runway_light_spacing();
        if (!beyond) {
            SET_AX(AX + DX);
            SET_CX(CX + 1);
        }
        runway_light_split();
    }
    if (RW(0xdc) != 0) {
        SET_DX(RW(0xcc) + RW(0xdc));
        SET_AX(RW(0xce) - 3);
        SET_BX(AX + 0x1e);
        SET_CX(4);
        SET_SI(0xf00f);
        SET_BP(SI);
        runway_light_bar();
        SET_AX(DX);
        SET_BX(AX);
        SET_BX(BX - RW(0xdc));
        SET_CX(0xf00f);
        SET_DX(SI);
        runway_light_run();
    }
    if (RW(0xd8) != 0) {
        SET_DX(-(RW(0xcc) + RW(0xd8)));
        SET_AX(RW(0xce) - 3);
        SET_BX(AX + 0x1e);
        SET_CX(4);
        SET_SI(0xf00f);
        SET_BP(SI);
        runway_light_bar();
        SET_BX(DX);
        SET_AX(BX);
        SET_AX(AX + RW(0xd8));
        SET_CX(0xf00f);
        SET_DX(CX);
        runway_light_run();
    }
end:
    M16(edi) = 0x22;
}

/* 0x206f237d -- the detail's distances: 25000 times the view's scale
 * (GLOBAL +0x2b0) at most 40, times the window's +0x84, at most 0x7fff
 * (ebea), an eighth of it (ebec), and both negated (ebf0, ebee). */
static void runway_detail(void)
{
    uint32_t p;

    edi = G;
    p = 0x61a8u * M16(edi + 0x2b0);
    SET_AX(p);
    SET_DX(p >> 16);
    if (DX > 0x28)
        SET_DX(0x28);
    esi = M32(edi + 0x1194);
    eax = M32(esi + 0x84);
    p = (uint32_t)AX * DX;
    SET_AX(p);
    SET_DX(p >> 16);
    if (AX > 0x7fff)
        SET_AX(0x7fff);
    RW(0xea) = AX;
    SET_BX(AX >> 3);
    RW(0xec) = BX;
    SET_AX(-AX);
    SET_BX(-BX);
    RW(0xee) = BX;
    RW(0xf0) = AX;
}

/* 0x206f23d9 -- the template's surface: its texture name's digit and
 * colours by the surface (at most 3; 0xf033 with approach lights and
 * GLOBAL +0x1172 bit 0 clear), its outline's corners, and the
 * surface's word (by the unclamped surface, so past the four words at
 * 0x2070ec1a into the glyph pointers: retail's), width and length. */
static void runway_template(void)
{
    SET_BL(M8(V(0x2070ebc2)));
    if (BL > 3)
        SET_BL(3);
    SET_AL(0x30 + BL);
    M8(V(0x2070f0e1)) = AL;
    ebx = BL;
    ebx += ebx;
    SET_AX(M16(V(0x2070ec22) + ebx));
    M16(V(0x2070f0c6)) = AX;
    M16(V(0x2070f056)) = AX;
    edi = G;
    SET_AX(M16(edi + 0x1172));
    if (!(AX & 1) && (M8(V(0x2070ebc3)) & 3)) {
        SET_AX(0xf033);
        M16(V(0x2070f056)) = AX;
    }
    SET_AX(RW(0xcc));
    M16(V(0x2070f158)) = AX;
    M16(V(0x2070f15e)) = AX;
    SET_AX(AX + RW(0xdc));
    M16(V(0x2070f09a)) = AX;
    M16(V(0x2070f0a0)) = AX;
    SET_AX(sar16(AX, 4));
    M16(V(0x2070f078)) = AX;
    SET_AX(-RW(0xcc));
    M16(V(0x2070f14c)) = AX;
    M16(V(0x2070f152)) = AX;
    SET_AX(AX - RW(0xd8));
    M16(V(0x2070f08e)) = AX;
    M16(V(0x2070f094)) = AX;
    SET_AX(sar16(AX, 4));
    M16(V(0x2070f072)) = AX;
    SET_AX(RW(0xce));
    M16(V(0x2070f08a)) = AX;
    M16(V(0x2070f09c)) = AX;
    M16(V(0x2070f148)) = AX;
    M16(V(0x2070f15a)) = AX;
    SET_AX(-AX);
    M16(V(0x2070f090)) = AX;
    M16(V(0x2070f096)) = AX;
    M16(V(0x2070f14e)) = AX;
    M16(V(0x2070f154)) = AX;
    SET_BL(M8(V(0x2070ebc2)));
    ebx = BL;
    SET_BX(BX + BX);
    SET_AX(retail_word(V(0x2070ec1a) + ebx));
    M16(V(0x2070f0bc)) = AX;
    M16(V(0x2070f0be)) = RW(0xbc);
    M16(V(0x2070f0c0)) = RW(0xba);
}

/* 0x206f224c -- the record into the template and its state (above), the
 * thresholds' presence bits (ebbf), the record's length (ebb0), markings
 * the runway is too short or narrow for dropped; the template's heading
 * jump (0x33 with a heading, else 0x0d) and the template run. */
static void runway_build(void)
{
    uint32_t i;

    for (i = 0; i <= 0x10; i += 2)
        M16(V(0x2070f02a) + i) = M16(i + esi + 2);
    SET_AX(M16(esi + 0x16));
    if (AX > 0x61a8)
        SET_AX(0x61a8);
    RW(0xba) = AX;
    SET_AX(AX >> 1);
    RW(0xcc) = AX;
    SET_AX(M16(esi + 0x14));
    RW(0xb8) = AX;
    SET_AX(M16(esi + 0x18));
    if (AX > 0x258)
        SET_AX(0x258);
    RW(0xbc) = AX;
    RW(0xf2) = AX;
    SET_AX(AX >> 1);
    RW(0xce) = AX;
    M8(V(0x2070ebbe)) = M8(esi + 0x1a);
    M8(V(0x2070ebc0)) = M8(esi + 0x1d);
    M8(V(0x2070ebc1)) = M8(esi + 0x1b);
    M8(V(0x2070ebc2)) = M8(esi + 0x1e);
    M8(V(0x2070ebc3)) = M8(esi + 0x1c);
    SET_DL(0);
    for (ebx = 6; (int32_t)ebx >= 0; ebx -= 2) {
        SET_AX(M16(ebx + esi + 0x20));
        M16(ebx + V(0x2070ebd6)) = AX;
        SET_DL(DL << 1 | (AX != 0));
        SET_AX(-AX);
    }
    M8(V(0x2070ebbf)) = DL;
    eax = M8(V(0x2070ebc0)) & 1 ? 0x52 : 0x40;
    M32(V(0x2070ebb0)) = eax;
    SET_AL(M8(V(0x2070ebbe)));
    if (RW(0xba) < 0xbb8)
        SET_AL(AL & 0xf7);
    if (RW(0xbc) < 0x1c)
        SET_AL(AL & 0xdf);
    M8(V(0x2070ebbe)) = AL;
    runway_template();
    runway_stripes_setup();
    runway_detail();
    SET_AX(0xd);
    SET_BX(RW(0xb8));
    if (BX != 0)
        SET_AX(0x33);
    M16(V(0x2070f03c)) = AX;
    M16(V(0x2070f044)) = BX;
    M16(V(0x2070f064)) = BX;
    M32(V(0x206f9126)) = V(0x2070f01c);
    run_program(M32(V(0x206f9126)));
}

/* 0x206e434b / 0x206e4351 -- 0x42, 0x44, a runway (0x206f2220): the record
 * at the instruction (0x2070ebb4), the context kept; on past it. */
static void op_runway(void)
{
    uint32_t keep_ebx = ebx, keep_esi = esi, keep_edi = edi;
    uint32_t keep_context = M32(V(0x206f91b7));

    M32(V(0x2070ebb4)) = esi;
    runway_build();
    esi = keep_esi;
    M32(V(0x206f91b7)) = keep_context;
    esi += M32(V(0x2070ebb0));
    M32(V(0x206f9126)) = esi;
    edi = keep_edi, esi = keep_esi, ebx = keep_ebx;
}

/* +0x50: 0x206f2220 itself, the record at esi. */
SERVICE_RAW(runway_svc, 0);
static void runway_svc_body(const uint32_t *arg)
{
    (void)arg;
    op_runway();
}

/* 0x206f2140 -- +0x2c, the earth (the built-in model at 0x20708840): its
 * latitude band (the eye's latitude less 10,000,000 in the 48-bit units,
 * inverted, halved, its middle 32 bits over 0x9897 -- a divide the host
 * repairs -- halved), its longitude (the eye's high word plus 0x5500), the
 * eye's position; drawn as +0x28 draws, the surfaces off. */
SERVICE(frame_earth, 0);
static void frame_earth_body(const uint32_t *arg)
{
    uint32_t r, borrow;
    uint64_t v;

    (void)arg;
    edi = G;
    SET_DX(M16(edi + 0x5b6));
    SET_AX(M16(edi + 0x5b4));
    SET_CX(M16(edi + 0x5b2));
    r = (uint32_t)AX - 0x9680;
    SET_AX(r);
    borrow = (r >> 16) & 1;
    SET_DX(DX - 0x98 - borrow);
    SET_DX(~DX);
    SET_CX(~CX);
    SET_AX(~AX);
    v = ((uint64_t)DX << 32 | (uint64_t)AX << 16 | CX) >> 1;
    SET_DX(v >> 32);
    SET_AX(v >> 16);
    SET_CX(v);
    SET_DH(DL);
    SET_DL(AH);
    SET_AH(AL);
    SET_AL(CH);
    SET_BX(0x9897);
    div16(BX);
    SET_AX(AX >> 1);
    M16(V(0x2070882a)) = AX;
    SET_AX(M16(edi + 0x5be) + 0x5500);
    M16(V(0x2070883e)) = AX;
    SET_AX(M16(edi + 0x5b2));
    SET_CX(M16(edi + 0x5b4));
    SET_DX(M16(edi + 0x5b6));
    M16(V(0x20708812)) = AX;
    M16(V(0x20708814)) = CX;
    M16(V(0x20708816)) = DX;
    SET_AX(M16(edi + 0x5ba));
    SET_CX(M16(edi + 0x5bc));
    SET_DX(M16(edi + 0x5be));
    M16(V(0x2070881a)) = AX;
    M16(V(0x2070881c)) = CX;
    M16(V(0x2070881e)) = DX;
    SET_AX(0);
    M16(edi + 0xc90) = AX;
    M16(edi + 0xcb6) = AX;
    M32(V(0x206f9126)) = V(0x20708840);
    draw_model(M32(V(0x206f9126)));
}

/* ---- ground tiles: 0x3d, 0x7f, .text+0x88e7..+0x897a and +0x89e2..+0xa680 ------- */

/* 0x3d lays a tile of ground: a kind (0x206fdc1c, its edge flags at
 * 0x206fdc1d), a mode (0x206fdc1e), its size (0x206fdc1f, 0x206fdc23,
 * each with a high word after), a 32-bit height (0x206fdc39), two
 * texture origins (0x206f91e1, 0x206f91e9) and a seed (0x206fdc27).  Mode
 * 0x22 builds a textured grid with heights from a table; 0x11/0x55, 0x33
 * and 0x44 run one of the built-in programs over the tile's four corners;
 * kinds 0x88 and 0x89 are a small lit grid; kinds 0x81, 0x82 and 0x8e
 * scatter buildings over it after (as 0x7f does), each at a cell picked
 * by a random generator seeded by the cell. */

/* 0x206e8af7 -- the surface colour ax (GLOBAL +0x3c0), and the line
 * colour (+0x2ec) the same, or 0x6900 in the map. */
static void surface_colour(void)
{
    edi = G;
    eax = AX;
    M32(edi + 0x3c0) = eax;
    if (M16(edi + 0xdca) != 0)
        eax = 0x6900;
    M32(edi + 0x2ec) = eax;
}

/* 0x206e1a02 -- a polygon started without counting it. */
static void polygon_restart(void)
{
    edi = G;
    eax = M32(edi + 0xc10);
    M32(edi + 0xc14) = eax;
    M32(edi + 0x2f0) = 0xffffffffu;
}

/* 0x206e98e7 -- the generator (0x206fdc06, a dword) seeded with ax. */
static void tile_seed(void)
{
    M16(V(0x206fdc06)) = AX;
    M16(V(0x206fdc08)) = 0;
}

/* 0x206e98f7 -- the generator's next: times 0x343fd plus 0x269ec3 (the
 * product's low dword by 0x206e993b's three 16-bit multiplies), ax its
 * high word; bx, cx and dx kept. */
static void tile_random(void)
{
    uint16_t b = BX, c = CX, d = DX;
    uint32_t r = ((uint32_t)M16(V(0x206fdc08)) << 16 | M16(V(0x206fdc06))) * 0x343fdu + 0x269ec3u;

    M16(V(0x206fdc06)) = (uint16_t)r;
    M16(V(0x206fdc08)) = (uint16_t)(r >> 16);
    SET_AX(r >> 16);
    SET_BX(b);
    SET_CX(c);
    SET_DX(d);
}

/* 0x206ea4fd -- the height at corner (ax column, dx row): 0 on an edge
 * whose flag (0x206fdc1d: bits 0..3 the left, right, near and far edges,
 * 4..7 the corners) is clear, else the word of the table at 0x206fdcf1 at
 * a byte from the corner's position at the detail's step (0x206fdc55)
 * from the texture origins. */
static void tile_corner_height(void)
{
    uint8_t bit;
    uint32_t p;

    SET_CL(M8(V(0x206fdc1d)));
    if (AX == 0)
        bit = DX == 0 ? 0x10 : DX == M16(V(0x206fd924)) ? 0x40 : 0x01;
    else if (AX == M16(V(0x206fd922)))
        bit = DX == 0 ? 0x20 : DX == M16(V(0x206fd924)) ? 0x80 : 0x02;
    else if (DX == 0)
        bit = 0x04;
    else if (DX == M16(V(0x206fd924)))
        bit = 0x08;
    else
        bit = 0;
    if (bit && !(CL & bit)) {
        SET_AX(0);
        return;
    }
    {
        uint16_t row = DX;

        p = (uint32_t)AX * M16(V(0x206fdc55));
        SET_CX(p);
        p = (uint32_t)row * M16(V(0x206fdc55));
        SET_DX(p);
        SET_AX(CX + M16(V(0x206fdc10)));
        SET_DX(DX + M16(V(0x206fdc0e)));
        p = (uint32_t)AX * DX;
        SET_AX(p);
        SET_DX(p >> 16);
        SET_AL(AL + AH);
        ebx = AL;
        SET_AX(M16(V(0x206fdcf1) + ebx));
    }
}

/* 0x206e99e2 -- the tile's grid: with a texture (0x206fdc31) its steps
 * across (0xff0000 over the columns and rows, divides the host repairs);
 * the box's eight corners (out of view on one side, nothing), the floor
 * with the eye inside, the corners' heights (each through the scale
 * 0x206fdc12 plus 0x206fdc14 as the texture word, and 0x206fdbfa as the
 * height) and positions into the block at 0x206fdc16, then the cells. */
static void tile_grid(void)
{
    uint32_t i, keep, p;

    if (M16(V(0x206fdc31)) != 0) {
        eax = 0xff0000;
        edx = 0;
        ecx = M16(V(0x206fd922));
        div32(ecx);
        M32(V(0x206fd940)) = eax;
        eax = 0xff0000;
        edx = 0;
        ecx = M16(V(0x206fd924));
        div32(ecx);
        M32(V(0x206fd944)) = eax;
    }
    M16(V(0x206fd92a)) = 0x1388;
    M16(V(0x206fd92c)) = 0x1388;
    SET_AX(M16(V(0x206fd922)) + 1);
    M16(V(0x206fd92e)) = AX;
    eax = (uint32_t)(int32_t)(int16_t)AX << 4;
    M32(V(0x206fd932)) = eax;
    p = (uint32_t)M16(V(0x206fd926)) * M16(V(0x206fd922));
    SET_AX(p);
    M16(V(0x206fd936)) = AX;
    p = (uint32_t)M16(V(0x206fd928)) * M16(V(0x206fd924));
    SET_AX(p);
    SET_DX(p >> 16);
    M16(V(0x206fd938)) = AX;
    M8(V(0x206fdc0a)) = 0xff;
    M8(V(0x206fdc0c)) = 0;
    M8(V(0x206fdbfc)) = 0x7f;
    keep = esi;
    edi = M32(V(0x206f912a));
    for (i = 0; i < 8; i++) {
        SET_AX(M16(V(0x206f91ef)) + ((i & 1) ^ ((i >> 1) & 1) ? M16(V(0x206fd936)) : 0));
        SET_BX(M16(V(i < 4 ? 0x206fdbf8 : 0x206fdbfa)));
        SET_DX(M16(V(0x206f91f1)) + (i & 2 ? M16(V(0x206fd938)) : 0));
        point_from_words();
        table_outcode();
        M8(V(0x206fdc0a)) &= AL;
        M8(V(0x206fdc0c)) |= AL;
        SET_AL(M8(V(0x206f9122)));
        if ((int8_t)AL < (int8_t)M8(V(0x206fdbfc)))
            M8(V(0x206fdbfc)) = AL;
        edi += 0x10;
    }
    esi = keep;
    if (M8(V(0x206fdc0a)) != 0)
        return;
    M8(V(0x206f9122)) = M8(V(0x206fdbfc));
    edi = G;
    SET_AX(M16(V(0x206f91ef)));
    if ((int16_t)M16(edi + 0xcde) >= (int16_t)AX &&
        (int16_t)M16(edi + 0xcde) <= (int16_t)(uint16_t)(AX + M16(V(0x206fd936)))) {
        SET_AX(M16(V(0x206f91f1)));
        if ((int16_t)M16(edi + 0xce6) >= (int16_t)AX &&
            (int16_t)M16(edi + 0xce6) <= (int16_t)(uint16_t)(AX + M16(V(0x206fd938))) &&
            (int16_t)M16(edi + 0xce2) <= (int16_t)M16(V(0x206fdbfa))) {
            M32(edi + 0x3c0) = 0x6900;
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
        }
    }
    edi = M32(V(0x206fdc16));
    {
        uint16_t col = 0, row = 0;

        for (;;) {
            SET_AX(col);
            SET_DX(row);
            tile_corner_height();
            SET_BX(AX);
            p = (uint32_t)M16(V(0x206fdc12)) * BX;
            SET_AX(p);
            SET_DX((p >> 16) + M16(V(0x206fdc14)));
            M16(edi + 0xe) = DX;
            p = (uint32_t)M16(V(0x206fdbfa)) * BX;
            SET_AX(p);
            SET_DX(p >> 16);
            M16(edi + 0xc) = DX;
            SET_BX(DX);
            p = (uint32_t)col * M16(V(0x206fd926));
            SET_SI(p);
            p = (uint32_t)row * M16(V(0x206fd928));
            SET_DX(p);
            SET_AX(SI + M16(V(0x206f91ef)));
            SET_DX(DX + M16(V(0x206f91f1)));
            {
                uint32_t d = edi;

                edi = M32(V(0x206f912a));
                point_from_words();
                esi = edi;
                edi = d;
            }
            for (i = 0; i < 12; i++)
                M8(edi + i) = M8(esi + i);
            esi += 12;
            edi += 12;
            ecx = 0;
            edi += 4;
            col++;
            if ((int16_t)col <= (int16_t)M16(V(0x206fd922)))
                continue;
            col = 0;
            row++;
            if ((int16_t)row > (int16_t)M16(V(0x206fd924)))
                break;
        }
    }
    grid_cells(0x206e9e2f);
}

/* The facing test of a tile's triangle: the sign of a 64-bit sum of the
 * products of three height or size differences (a, b, c, each << 16) with
 * the eye's offsets from the corner (x, y, z << 16 less GLOBAL +0xcdc,
 * +0xce0, +0xce4); ebp and ecx the sum, as retail leaves it. */
static int tile_facing(uint16_t a, uint16_t x, uint16_t b, uint16_t y, uint16_t c, uint16_t z)
{
    uint64_t sum;
    uint32_t g = G;

    sum = (uint64_t)((int64_t)(int32_t)((uint32_t)a << 16) *
                     (int32_t)(((uint32_t)x << 16) - M32(g + 0xcdc)));
    sum += (uint64_t)((int64_t)(int32_t)((uint32_t)b << 16) *
                      (int32_t)(((uint32_t)y << 16) - M32(g + 0xce0)));
    sum += (uint64_t)((int64_t)(int32_t)((uint32_t)c << 16) *
                      (int32_t)(((uint32_t)z << 16) - M32(g + 0xce4)));
    ebp = (uint32_t)sum;
    ecx = (uint32_t)(sum >> 32);
    return (int32_t)ecx < 0;
}

/* A textured triangle's vertices go on the machine stack (0x206ea3aa and
 * 0x206ea456 return over them) for the fast triangle; the first pushed is
 * the last of the three. */
static struct {
    struct fast_vertex v[3];
    uint32_t n;
} tile_tri;

/* 0x206ea396 */
static void tile_triangle_start(void)
{
    textured_start();
    tile_tri.n = 0;
}

/* 0x206ea3aa (first) / 0x206ea456 -- the grid's vertex at index (from the
 * block at 0x206fdc16) with texture u, v at uv: its position, outcode and
 * texture word; with a texture onto the stack, its u and v plus the
 * texture's offsets (GLOBAL +0xc94, +0xc9c), its return address kept at
 * 0x206fd968 as retail pops it there (`ret`, retail's); else the shaded
 * polygon's.  The first's u, v (0x206f922f), position (0x206f90e4) and
 * word (0x206f921b) are kept too. */
static void tile_vertex(int first, uint16_t index, uint32_t uv, uint32_t ret)
{
    uint32_t g;

    M32(V(0x206f921f)) = M32(uv);
    M32(V(0x206f9223)) = M32(uv + 4);
    eax = (uint32_t)(int32_t)(int16_t)index << 4;
    eax += M32(V(0x206fdc16));
    esi = eax;
    if (first) {
        M32(V(0x206f922f)) = M32(V(0x206f921f));
        M32(V(0x206f9233)) = M32(V(0x206f9223));
    }
    M32(V(0x206f90c0)) = M32(esi);
    M32(V(0x206f90c4)) = M32(esi + 4);
    M32(V(0x206f90c8)) = M32(esi + 8);
    if (first) {
        M32(V(0x206f90e4)) = M32(esi);
        M32(V(0x206f90e8)) = M32(esi + 4);
        M32(V(0x206f90ec)) = M32(esi + 8);
    }
    eax = M32(esi + 8);
    esi += 12;
    SET_AX(M16(esi));
    esi += 2;
    M8(V(0x206f911c)) = AL;
    if (first)
        M8(V(0x206f9121)) = AH;
    SET_AX(M16(esi));
    esi += 2;
    SET_BX(AX);
    M16(V(0x206f90f0)) = AX;
    if (first)
        M16(V(0x206f921b)) = AX;
    if (M16(V(0x206fdc31)) == 0) {
        if (first)
            shaded_first();
        else
            shaded_next();
        return;
    }
    M32(V(0x206fd968)) = ret;
    g = G;
    edi = g;
    {
        struct fast_vertex *v = &tile_tri.v[2 - tile_tri.n++];

        v->c[4] = M32(V(0x206f90c0));
        v->c[3] = M32(V(0x206f90c4));
        v->c[2] = M32(V(0x206f90c8));
        v->c[1] = M32(V(0x206f921f)) + M32(g + 0xc94);
        v->c[0] = eax = M32(V(0x206f9223)) + M32(g + 0xc9c);
    }
}

/* 0x206ea4d3 -- close it: the fast textured triangle (depth 0), or the
 * shaded polygon's close. */
static void tile_triangle_close(uint32_t ret)
{
    if (M16(V(0x206fdc31)) == 0) {
        shaded_close();
        return;
    }
    M32(V(0x206fd968)) = ret;
    SET_AX(0);
    fast_triangle(0, tile_tri.v);
}

/* 0x206ea1b4, 0x206ea227, 0x206ea2a5, 0x206ea31c -- a cell's triangles:
 * the corners (near left, near right, far left, far right: 0x206fd93a,
 * +1, + a row, both) with their u, v (0x206fd948, 0x206fd950, 0x206fd958,
 * 0x206fd960). */
static void tile_triangle(int corner0, int corner1, int corner2, const uint32_t ret[4])
{
    static const uint32_t uv[4] = { 0x206fd948, 0x206fd950, 0x206fd958, 0x206fd960 };
    const int corner[3] = { corner0, corner1, corner2 };
    uint32_t i;

    tile_triangle_start();
    for (i = 0; i < 3; i++) {
        uint16_t index = M16(V(0x206fd93a));

        if (corner[i] & 1)
            index = (uint16_t)(index + 1);
        if (corner[i] & 2)
            index = (uint16_t)(index + M16(V(0x206fd92e)));
        tile_vertex(i == 0, index, V(uv[corner[i]]), ret[i]);
    }
    tile_triangle_close(ret[3]);
}

/* 0x206e9e2f -- a tile's cell (bx the column, cx the row): its u, v with a
 * texture, its index and origin; split along a diagonal by the parity of
 * the cell and of the texture origins at the detail's shift (0x206fdc57);
 * each triangle drawn facing the eye, then the eye's test (0x206e960a). */
static void tile_cell(void)
{
    uint16_t col = BX, row = CX;
    uint32_t base, p;

    if (M16(V(0x206fdc31)) != 0) {
        uint64_t q;

        ebx = col;
        q = (uint64_t)M32(V(0x206fd940)) * ebx;
        eax = (uint32_t)q;
        M32(V(0x206fd948)) = eax;
        M32(V(0x206fd958)) = eax;
        eax += M32(V(0x206fd940));
        M32(V(0x206fd950)) = eax;
        M32(V(0x206fd960)) = eax;
        ecx = row;
        q = (uint64_t)M32(V(0x206fd944)) * ecx;
        eax = (uint32_t)q;
        edx = (uint32_t)(q >> 32);
        M32(V(0x206fd94c)) = eax;
        M32(V(0x206fd954)) = eax;
        eax += M32(V(0x206fd944));
        M32(V(0x206fd95c)) = eax;
        M32(V(0x206fd964)) = eax;
    }
    p = (uint32_t)row * M16(V(0x206fd92e));
    M16(V(0x206fd93a)) = (uint16_t)(p + col);
    p = (uint32_t)col * M16(V(0x206fd926));
    M16(V(0x206fd93c)) = (uint16_t)(p + M16(V(0x206f91ef)));
    p = (uint32_t)row * M16(V(0x206fd928));
    M16(V(0x206fd93e)) = (uint16_t)(p + M16(V(0x206f91f1)));
    M16(V(0x206fdc41)) = 0;
    SET_AX(col ^ row);
    SET_CL(M8(V(0x206fdc57)));
    SET_SI(sar16(M16(V(0x206fdc0e)), CL));
    SET_DI(sar16(M16(V(0x206fdc10)), CL));
    SET_AX(AX ^ SI ^ DI);
    base = (uint32_t)(int32_t)(int16_t)(uint16_t)(M16(V(0x206fd93a)) << 4) + M32(V(0x206fdc16));
#define H(r) M16((r) + 0xc)
    if (AX & 1) {
        static const uint32_t ra[4] = { 0x206ea1d8, 0x206ea1fb, 0x206ea221, 0x206ea226 };
        static const uint32_t rb[4] = { 0x206ea24f, 0x206ea275, 0x206ea29f, 0x206ea2a4 };

        ebx = esi = edi = base;
        esi += 0x10;
        edi += M32(V(0x206fd932));
        if (!tile_facing(H(esi) - H(ebx), M16(V(0x206fd93c)), -M16(V(0x206fd926)), H(ebx),
                         H(edi) - H(ebx), M16(V(0x206fd93e)))) {
            tile_triangle(0, 1, 2, ra);
            M16(V(0x206fdc41)) += 1;
        }
        ebx = esi = edi = base;
        ebx += 0x10;
        esi += M32(V(0x206fd932)) + 0x10;
        edi += M32(V(0x206fd932));
        if (!tile_facing(H(esi) - H(edi), M16(V(0x206fd93c)) + M16(V(0x206fd926)),
                         -M16(V(0x206fd926)), H(esi), H(esi) - H(ebx),
                         M16(V(0x206fd93e)) + M16(V(0x206fd928)))) {
            tile_triangle(1, 2, 3, rb);
            M16(V(0x206fdc41)) += 1;
        }
    } else {
        static const uint32_t rc[4] = { 0x206ea2c9, 0x206ea2ec, 0x206ea316, 0x206ea31b };
        static const uint32_t rd[4] = { 0x206ea340, 0x206ea366, 0x206ea390, 0x206ea395 };

        ebx = esi = edi = base;
        esi += 0x10;
        edi += M32(V(0x206fd932)) + 0x10;
        if (!tile_facing(H(esi) - H(ebx), M16(V(0x206fd93c)), -M16(V(0x206fd926)), H(ebx),
                         H(edi) - H(esi), M16(V(0x206fd93e)))) {
            tile_triangle(0, 1, 3, rc);
            M16(V(0x206fdc41)) += 1;
        }
        ebx = esi = edi = base;
        esi += M32(V(0x206fd932));
        edi += M32(V(0x206fd932)) + 0x10;
        if (!tile_facing(H(edi) - H(esi), M16(V(0x206fd93c)), -M16(V(0x206fd926)), H(esi),
                         H(esi) - H(ebx), M16(V(0x206fd93e)) + M16(V(0x206fd928)))) {
            tile_triangle(0, 2, 3, rd);
            M16(V(0x206fdc41)) += 1;
        }
    }
#undef H
    M16(V(0x206f90f6)) = 1;
    grid_cell_eye();
}

/* 0x206ea7a1 -- the carry when a textured tile keeps its 0x100 cells: more
 * than one each way, or an edge or corner on its one cell or row that is
 * flagged (0xf0 one cell, 0xf3 one column, 0xfc one row). */
static int tile_whole(void)
{
    SET_AX(M16(V(0x206fd922)) - 1);
    SET_BX(M16(V(0x206fd924)) - 1);
    if (AX != 0) {
        if (BX != 0)
            return 1;
        SET_CL(0xfc);
    } else {
        SET_CL(BX == 0 ? 0xf0 : 0xf3);
    }
    return (M8(V(0x206fdc1d)) & CL) != 0;
}

/* 0x206ea6cb -- the tile flat: colour 0x69 with the texture word's high
 * byte, its four corners as table points 0..3, a polygon. */
static void tile_flat(void)
{
    SET_AX(M16(V(0x206fdc14)));
    SET_AL(AH);
    SET_AH(0x69);
    surface_colour();
    SET_AX(M16(V(0x206f91ef)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)));
    SET_DI(0);
    table_point();
    SET_AX(M16(V(0x206f91ef)) + M16(V(0x206fdc1f)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)));
    SET_DI(1);
    table_point();
    SET_AX(M16(V(0x206f91ef)) + M16(V(0x206fdc1f)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)) + M16(V(0x206fdc23)));
    SET_DI(2);
    table_point();
    SET_AX(M16(V(0x206f91ef)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)) + M16(V(0x206fdc23)));
    SET_DI(3);
    table_point();
    polygon_restart();
    eax = 0;
    table_first();
    eax = 1;
    table_next();
    eax = 2;
    table_next();
    eax = 3;
    table_next();
    polygon_close();
}

/* 0x206ea5a6 -- the textured tile: cells of 0x100 (its 32-bit sizes over
 * that, divides the host repairs), a quarter of that and four times as
 * many unless tile_whole(); centred, the texture origins; a grid while its
 * vertices fit the block (0x206fdc1a), else flat. */
static void tile_texture_grid(void)
{
    uint32_t p;

    SET_AX(0x100);
    M16(V(0x206fd926)) = AX;
    M16(V(0x206fd928)) = AX;
    SET_AX(M16(V(0x206fdc1f)));
    SET_DX(M16(V(0x206fdc21)));
    div16(M16(V(0x206fd926)));
    M16(V(0x206fd922)) = AX;
    SET_AX(M16(V(0x206fdc23)));
    SET_DX(M16(V(0x206fdc25)));
    div16(M16(V(0x206fd928)));
    M16(V(0x206fd924)) = AX;
    if (!tile_whole()) {
        p = 4u * M16(V(0x206fd922));
        M16(V(0x206fd922)) = (uint16_t)p;
        p = 4u * M16(V(0x206fd924));
        SET_AX(p);
        SET_DX(p >> 16);
        M16(V(0x206fd924)) = AX;
        M16(V(0x206fd926)) = 0x40;
        M16(V(0x206fd928)) = 0x40;
        M8(V(0x206fdc1d)) = 0;
    }
    M16(V(0x206fdbf8)) = 0;
    M16(V(0x206f91ef)) = (uint16_t)-(M16(V(0x206fdc1f)) >> 1);
    M16(V(0x206f91f1)) = (uint16_t)-(M16(V(0x206fdc23)) >> 1);
    M16(V(0x206fdc0e)) = M16(V(0x206f91e1));
    M16(V(0x206fdc10)) = M16(V(0x206f91e9));
    SET_AX(M16(V(0x206fd922)) + 1);
    SET_BX(M16(V(0x206fd924)) + 1);
    p = (uint32_t)AX * BX;
    SET_AX(p);
    SET_DX(p >> 16);
    if (AX < M16(V(0x206fdc1a)))
        tile_grid();
    else
        tile_flat();
}

/* 0x206ea7d0 -- called from built-in programs (0x8c): the texture's
 * origin and steps (GLOBAL +0xca4..+0xcb2) for a tile of 0x206fdc1f by
 * 0x206fdc23, its corner (+0xc96, +0xc9e) at minus half each. */
static void tile_texture_origin(void)
{
    uint32_t v;

    edi = G;
    v = ((uint32_t)M16(V(0x206fdc1f)) << 16) - 0x8000;
    SET_AX(v);
    SET_DX(v >> 16);
    M16(edi + 0xcac) = AX;
    M16(edi + 0xcae) = DX;
    M16(edi + 0xca4) = 0x8000;
    M16(edi + 0xca6) = 0;
    v = ((uint32_t)M16(V(0x206fdc23)) << 16) - 0x8000;
    SET_AX(v);
    SET_DX(v >> 16);
    M16(edi + 0xcb0) = AX;
    M16(edi + 0xcb2) = DX;
    M16(edi + 0xca8) = 0x8000;
    M16(edi + 0xcaa) = 0;
    SET_AX(-(M16(V(0x206fdc1f)) >> 1));
    M16(edi + 0xc96) = AX;
    SET_AX(-(M16(V(0x206fdc23)) >> 1));
    M16(edi + 0xc9e) = AX;
}

/* 0x206eae34 -- the carry when the tile's four corners (table points 0..3)
 * are all out of view on one side. */
static int tile_out(void)
{
    uint32_t i;

    M8(V(0x206fdc0a)) = 0xff;
    edi = M32(V(0x206f912a));
    for (i = 0; i < 4; i++) {
        if (i)
            edi += 0x10;
        table_outcode();
        M8(V(0x206fdc0a)) &= AL;
        if (M8(V(0x206fdc0a)) == 0)
            return 0;
    }
    return 1;
}

/* 0x206eaeea -- al as two hex digits, the high one first. */
static void hex_byte(void)
{
    SET_AH(AL);
    SET_AL(AL & 0xf0);
    SET_CL(4);
    SET_AL(AL >> CL);
    SET_AL(AL + 0x30);
    if (AL > 0x39)
        SET_AL(AL + 7);
    { uint8_t t = AH; SET_AH(AL); SET_AL(t); }
    SET_AL(AL & 0xf);
    SET_AL(AL + 0x30);
    if (AL > 0x39)
        SET_AL(AL + 7);
    { uint8_t t = AH; SET_AH(AL); SET_AL(t); }
}

/* 0x206eae9a -- the texture name for kind al: kinds 0..0xe and 0x80..0x8f
 * from tables (0x206fdb00, 0x206fdb3c), the rest written in hex into one
 * of two names (0x206fe4e8 below 0x80, else 0x206fe7c6). */
static void tile_texture(void)
{
    SET_AH(0);
    if (AX <= 0xe) {
        ebx = AX;
        eax = M32(V(0x206fdb00) + ebx * 4);
    } else if (AX >= 0x80 && AX <= 0x8f) {
        SET_AX(AX & 0x7f);
        ebx = AX;
        eax = M32(V(0x206fdb3c) + ebx * 4);
    } else if (AX < 0x80) {
        hex_byte();
        M16(V(0x206fe508)) = AX;
        eax = V(0x206fe4e8);
    } else {
        hex_byte();
        M16(V(0x206fe7e6)) = AX;
        eax = V(0x206fe7c6);
    }
}

/* 0x206eaf09 -- the twelve bytes of the kind's texture names (by the
 * season, GLOBAL +0x248, unless the table has one) to 0x206fe4d2. */
static void tile_kind_name(uint8_t kind)
{
    uint32_t i;

    ebx = kind & 0xf;
    if (ebx >= 0xc)
        ebx = 0;
    esi = M32(V(0x206fd96c) + ebx * 4);
    if (M32(esi) == 1) {
        esi += 4;
    } else {
        eax = (uint32_t)M16(G + 0x248) * 0xc;
        esi = eax + esi + 4;
    }
    edi = V(0x206fe4d2);
    for (i = 0; i < 12; i++)
        M8(edi + i) = M8(esi + i);
    esi += 12;
    edi += 12;
    ecx = 0;
}

static void tile_scatter(void);

/* 0x206eae82 -- after the tile, kinds 0x81, 0x82 and 0x8e scatter. */
static void tile_after(void)
{
    if (AL == 0x8e || (AL >= 0x81 && AL <= 0x82))
        tile_scatter();
}

/* 0x206eb552 -- the eye in the object, every register kept. */
static void tile_eye(void)
{
    uint32_t a = eax, b = ebx, c = ecx, d = edx, s = esi, i = edi, p = ebp;

    eye_in_object_turned();
    eax = a, ebx = b, ecx = c, edx = d, esi = s, edi = i, ebp = p;
}

/* 0x206eb385 -- after a scattered building (si, di its sizes): GLOBAL
 * +0x832 cleared, and 6 with the eye inside it (not for kind 0x8e). */
static void tile_scatter_eye(void)
{
    if (M8(V(0x206fdc1c)) == 0x8e)
        return;
    M16(G + 0x832) = 0;
    if ((int16_t)M16(G + 0xce2) < 0)
        return;
    SET_AX(M16(V(0x206fdc5d)) * 4);
    if ((int16_t)M16(G + 0xce2) > (int16_t)AX)
        return;
    SET_SI(SI >> 1);
    SET_DI(DI >> 1);
    SET_AX(M16(V(0x206fdc59)) - SI);
    if ((int16_t)M16(G + 0xcde) < (int16_t)AX)
        return;
    SET_AX(AX + SI + SI);
    if ((int16_t)M16(G + 0xcde) > (int16_t)AX)
        return;
    SET_AX(M16(V(0x206fdc5b)) - DI);
    if ((int16_t)M16(G + 0xce6) < (int16_t)AX)
        return;
    SET_AX(AX + DI + DI);
    if ((int16_t)M16(G + 0xce6) > (int16_t)AX)
        return;
    M16(G + 0x832) = 6;
}

/* 0x206eb448..0x206eb544 -- the scattered buildings: colours, then one of
 * 0x49's kinds (through 0x206fdbac, the big ones, and 0x206fdbd8). */
#define SCATTERED(name, colours, kind)                                        \
    static void name(void)                                                    \
    {                                                                         \
        M16(V(0x206f931a)) = colours;                                         \
        building_kind(kind);                                                  \
    }
SCATTERED(scattered_eb448, 7, 0)
SCATTERED(scattered_eb456, 0, 0)
SCATTERED(scattered_eb464, 1, 0)
SCATTERED(scattered_eb472, 1, 0)
SCATTERED(scattered_eb480, 3, 0)
SCATTERED(scattered_eb48e, 2, 1)
SCATTERED(scattered_eb49c, 2, 3)
SCATTERED(scattered_eb4aa, 3, 0)
SCATTERED(scattered_eb4b8, 1, 0)
SCATTERED(scattered_eb4c6, 2, 3)
SCATTERED(scattered_eb4d4, 4, 3)
SCATTERED(scattered_eb4e2, 0, 0)
SCATTERED(scattered_eb4f0, 1, 0)
SCATTERED(scattered_eb4fe, 2, 0)
SCATTERED(scattered_eb50c, 3, 0)
SCATTERED(scattered_eb51a, 4, 0)
SCATTERED(scattered_eb528, 5, 0)
SCATTERED(scattered_eb536, 2, 3)
SCATTERED(scattered_eb544, 7, 0)
#undef SCATTERED

/* 0x206eb17d -- a cell of the scatter (bx the column, cx the row): the
 * generator seeded by the cell and 0x206fdc27; a building when its first
 * number shares a bit with 0x206fdc6d (the detail's mask), at a quarter
 * cell in (and half a cell more by its bits 1 and 2), the object there
 * (GLOBAL +0x300, +0x318 from 0x206fdc61/0x206fdc69, its distance); its
 * height from the room left to the tile's edges and the next number; big
 * ones (over 0x19) of 0x14 by 0x14 from eleven kinds, else sized by two
 * more numbers from eight. */
static void tile_scatter_cell(void)
{
    uint16_t col = BX, row = CX;
    uint32_t p, v, table;

    SET_AX(col + 1);
    SET_DX(row + 1);
    p = (uint32_t)AX * DX;
    SET_AX(p);
    SET_DX(AX);
    SET_AX((uint16_t)(AX << 8 | AX >> 8) + DX);
    SET_AX(AX + M16(V(0x206fdc27)));
    tile_seed();
    tile_random();
    if (!(M16(V(0x206fdc6d)) & AX))
        return;
    SET_BP(AX);
    SET_AX(M16(V(0x206fd926)));
    SET_SI(AX);
    p = (uint32_t)AX * BX;
    SET_AX(p);
    SET_DX(p >> 16);
    SET_AX(AX + M16(V(0x206f91ef)) + (SI >> 2));
    SET_SI(SI >> 2);
    M16(V(0x206fdc59)) = AX;
    SET_AX(M16(V(0x206fd928)));
    SET_SI(AX);
    p = (uint32_t)AX * CX;
    SET_AX(p);
    SET_DX(p >> 16);
    SET_AX(AX + M16(V(0x206f91f1)) + (SI >> 2));
    SET_SI(SI >> 2);
    M16(V(0x206fdc5b)) = AX;
    if (BP & 2)
        M16(V(0x206fdc59)) += 0x40;
    if (BP & 4)
        M16(V(0x206fdc5b)) += 0x40;
    SET_AX(M16(V(0x206fdc59)));
    v = M32(V(0x206fdc61)) - (uint32_t)(int32_t)(int16_t)M16(V(0x206fdc59));
    SET_BX(v);
    SET_CX(v >> 16);
    SET_DX((int16_t)M16(V(0x206fdc59)) < 0 ? 0xffff : 0);
    edi = G;
    M16(edi + 0x300) = BX;
    M16(edi + 0x302) = CX;
    SET_AX(M16(V(0x206fdc5b)));
    v = M32(V(0x206fdc69)) - (uint32_t)(int32_t)(int16_t)M16(V(0x206fdc5b));
    SET_BX(v);
    SET_CX(v >> 16);
    SET_DX((int16_t)M16(V(0x206fdc5b)) < 0 ? 0xffff : 0);
    M16(edi + 0x318) = BX;
    M16(edi + 0x31a) = CX;
    object_distance();
    SET_AX(M16(V(0x206fdc59)));
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    SET_AX(AX ^ DX);
    SET_BX((M16(V(0x206fdc1f)) >> 1) - AX);
    SET_AX(M16(V(0x206fdc5b)));
    SET_DX((int16_t)AX < 0 ? 0xffff : 0);
    SET_AX(AX ^ DX);
    SET_CX((M16(V(0x206fdc23)) >> 1) - AX);
    p = (uint32_t)BX * CX;
    SET_AX(p);
    SET_DX(p >> 16);
    SET_BX(BX + CX);
    SET_BX(BX + BX);
    SET_BX(BX + BX);
    div16(BX);
    SET_DX(AX);
    tile_random();
    SET_AX(AX | 0x8000);
    p = (uint32_t)AX * DX;
    SET_AX(p);
    SET_DX(p >> 16);
    M16(V(0x206fdc5d)) = DX;
    if (DX > 0x19) {
        SET_SI(0x14);
        SET_DI(0x14);
        tile_random();
        p = (uint32_t)AX * 0xb;
        SET_AX(p);
        SET_DX(p >> 16);
        ebp = DX;
        table = 0x206fdbac;
    } else {
        tile_random();
        SET_DX(M16(V(0x206fd926)) >> 2);
        p = (uint32_t)AX * DX;
        SET_AX(p);
        SET_DX(p >> 16);
        SET_SI(DX + 0xf);
        tile_random();
        SET_DX(M16(V(0x206fd928)) >> 2);
        p = (uint32_t)AX * DX;
        SET_AX(p);
        SET_DX(p >> 16);
        SET_DI(DX + 0xf);
        tile_random();
        SET_DX(8);
        p = (uint32_t)AX * DX;
        SET_AX(p);
        SET_DX(p >> 16);
        ebp = DX;
        table = 0x206fdbd8;
    }
    SET_AX(M16(V(0x206fdc59)));
    SET_DX(0);
    SET_BX(M16(V(0x206fdc5b)));
    SET_CX(M16(V(0x206fdc5d)));
    {
        uint16_t s = SI, d = DI;

        routine(M32(V(table) + ebp * 4))();
        SET_DI(d);
        SET_SI(s);
    }
    tile_scatter_eye();
}

/* 0x206eaffe -- the scatter's grid: 6 by 6 cells of 0x80 for kind 1, else
 * 8 by 8; the eye moved to the scale times 16 (kind 0x8e: 1) and, when it
 * fits, the cells within 0x50 of it through tile_scatter_cell(); GLOBAL's
 * object offsets and the eye put back. */
static void tile_scatter_grid(void)
{
    uint32_t keep[4];

    if (M8(V(0x206fdc1c)) == 1) {
        M16(V(0x206fd922)) = 6;
        M16(V(0x206fd924)) = 6;
        M16(V(0x206f91ef)) = 0xfe80;
        M16(V(0x206f91f1)) = 0xfe80;
        M16(V(0x206fdc1f)) = 0x300;
        M16(V(0x206fdc23)) = 0x300;
    } else {
        M16(V(0x206fd922)) = 8;
        M16(V(0x206fd924)) = 8;
        M16(V(0x206f91ef)) = 0xfe00;
        M16(V(0x206f91f1)) = 0xfe00;
        M16(V(0x206fdc1f)) = 0x400;
        M16(V(0x206fdc23)) = 0x400;
    }
    M16(V(0x206fd926)) = 0x80;
    M16(V(0x206fd928)) = 0x80;
    M16(V(0x206fd92a)) = 0x50;
    M16(V(0x206fd92c)) = 0x50;
    edi = G;
    keep[0] = M32(edi + 0xcdc);
    keep[1] = M32(edi + 0xce0);
    keep[2] = M32(edi + 0xce4);
    keep[3] = M32(edi + 0x320);
    if (M8(V(0x206fdc1c)) != 0x8e) {
        uint32_t v = M32(edi + 0x320) << 4;

        SET_AX(v);
        SET_DX(v >> 16);
        M16(edi + 0x320) = AX;
        M16(edi + 0x322) = DX;
    } else {
        M32(edi + 0x320) = 0x10000;
    }
    eye_from_offsets();
    if (CX == 0) {
        uint32_t keep_pc;

        edi = G;
        M32(V(0x206fdc61)) = M32(edi + 0x300);
        M32(V(0x206fdc69)) = M32(edi + 0x318);
        eax = 0x206eb17d;
        keep_pc = M32(V(0x206f9126));
        grid_cells(0x206eb17d);
        M32(V(0x206f9126)) = keep_pc;
        edi = G;
        M32(edi + 0x300) = M32(V(0x206fdc61));
        M32(edi + 0x318) = M32(V(0x206fdc69));
    }
    edi = G;
    M32(edi + 0x320) = keep[3];
    M32(edi + 0xce4) = keep[2];
    M32(edi + 0xce0) = keep[1];
    M32(edi + 0xcdc) = keep[0];
}

/* 0x206eaf51 -- scatter buildings over the tile (when 0x206f917c is set,
 * else 0x206fdc37 only): the detail's mask (0x206fdc6d by GLOBAL +0xb60),
 * the program at 0x206fdf22 unless GLOBAL +0x1172 bit 0, the scatter in
 * the map first (GLOBAL +0x11b2 and +0xb34 set: +0xdca up, +0x832 kept),
 * then the scatter. */
static void tile_scatter(void)
{
    tile_eye();
    M16(V(0x206f931c)) = 0xffff;
    if (M16(V(0x206f917c)) == 0) {
        M16(V(0x206fdc37)) = 1;
        return;
    }
    edi = G;
    ebx = M32(edi + 0xb60);
    ebx += ebx;
    SET_AX(M16(V(0x206fdc6f) + ebx));
    M16(V(0x206fdc6d)) = AX;
    edi = G;
    if (!(M16(edi + 0x1172) & 1)) {
        uint32_t keep_pc = M32(V(0x206f9126));

        M32(V(0x206f9126)) = V(0x206fdf22);
        interpret();
        M32(V(0x206f9126)) = keep_pc;
    }
    edi = G;
    if (M16(edi + 0x11b2) != 0 && M16(edi + 0xb34) != 0) {
        uint16_t keep = M16(edi + 0x832);

        M16(edi + 0xdca) += 1;
        tile_scatter_grid();
        edi = G;
        M16(edi + 0xdca) -= 1;
        M16(edi + 0x832) = keep;
    }
    tile_scatter_grid();
}

/* 0x206eabfa / 0x206eac94 -- kinds 0x88 and 0x89 (with 0x206f917c): a lit
 * grid of 4 by 4 cells of 0x40, its texture word and scale from the pair
 * at `at`. */
static void tile_lit(uint32_t at)
{
    if (M16(V(0x206f917c)) != 0) {
        M16(V(0x206fd922)) = 4;
        M16(V(0x206fd924)) = 4;
        M16(V(0x206fd926)) = 0x40;
        M16(V(0x206fd928)) = 0x40;
        M16(V(0x206f91ef)) = 0xff80;
        M16(V(0x206f91f1)) = 0xff80;
        M16(V(0x206fdbf8)) = 0;
        M16(V(0x206fdbfa)) = 0x10;
        SET_AX(M16(at));
        M16(V(0x206fdc14)) = AX;
        SET_AX(-AX + M16(at + 2));
        M16(V(0x206fdc12)) = AX;
        M16(V(0x206fdc0e)) = M16(V(0x206f91e1));
        M16(V(0x206fdc10)) = M16(V(0x206f91e9));
        tile_grid();
    }
    M16(V(0x206fdc37)) = 1;
}

/* Run a built-in program, the counter kept. */
static void tile_program_at(uint32_t program)
{
    uint32_t keep_pc = M32(V(0x206f9126));

    M32(V(0x206f9126)) = program;
    interpret();
    M32(V(0x206f9126)) = keep_pc;
}

/* 0x206eaac3 -- modes 0x11 and 0x55: kinds 0x88, 0x89 lit; else, in view
 * and unless 0x206f917c, the program at 0x206fe47a without a context;
 * then tile_after(). */
static void tile_mode_11(void)
{
    if (M8(V(0x206fdc1c)) == 0x88) {
        tile_lit(V(0x206fde49));
        return;
    }
    if (M8(V(0x206fdc1c)) == 0x89) {
        tile_lit(V(0x206fde4d));
        return;
    }
    if (M16(V(0x206f917c)) == 0 && !tile_out()) {
        uint32_t keep_context = M32(V(0x206f919c));

        M32(V(0x206f919c)) = 0;
        tile_program_at(V(0x206fe47a));
        M32(V(0x206f919c)) = keep_context;
    }
    SET_AL(M8(V(0x206fdc1c)));
    tile_after();
}

/* 0x206eab2b -- mode 0x33, in view and unless 0x206f917c: by the edge
 * flags' low four bits (and 0x10 for kinds 6, 7) a word to 0x206fdce9, and
 * by the four a program. */
static void tile_mode_33(void)
{
    if (M16(V(0x206f917c)) != 0 || tile_out())
        return;
    SET_BL(M8(V(0x206fdc1d)));
    SET_BX(BX & 0xf);
    if (M8(V(0x206fdc1c)) >= 6 && M8(V(0x206fdc1c)) <= 7)
        SET_BX(BX | 0x10);
    ebx = BX;
    M32(V(0x206fdce9)) = M32(V(0x206fdea1) + ebx * 4);
    ebx &= 0xf;
    tile_program_at(M32(V(0x206fde61) + ebx * 4));
}

/* 0x206eab8d -- mode 0x44: as 0x33 by the low two bits (and 4), from
 * 0x206ff4e2 to 0x206fdced and the programs at 0x206ff4d2; then
 * tile_after(). */
static void tile_mode_44(void)
{
    if (M16(V(0x206f917c)) == 0 && !tile_out()) {
        SET_BL(M8(V(0x206fdc1d)));
        SET_BX(BX & 3);
        if (M8(V(0x206fdc1c)) >= 6 && M8(V(0x206fdc1c)) <= 7)
            SET_BX(BX | 4);
        ebx = BX;
        M32(V(0x206fdced)) = M32(V(0x206ff4e2) + ebx * 4);
        SET_BX(BX & 3);
        tile_program_at(M32(V(0x206ff4d2) + ebx * 4));
    }
    SET_AL(M8(V(0x206fdc1c)));
    tile_after();
}

/* 0x206ead2e -- mode 0x22 (with 0x206f917c): with a texture, its names
 * (tile_kind_name); the texture word and scale by the kind's low four
 * bits, or for 0xd and 0xe by the season (GLOBAL +0x248, two on in the
 * south, +0x566 negative; 0xe four more); the height scale by its high
 * four; with a texture the program at 0x206fe4ac; the textured tile.
 * GLOBAL +0xcce cleared, 0x206fdc37 set. */
static void tile_mode_22(void)
{
    if (M16(V(0x206f917c)) != 0) {
        uint16_t kind;

        if (M16(V(0x206fdc31)) != 0) {
            SET_AL(M8(V(0x206fdc1c)));
            tile_kind_name(AL);
        }
        SET_BH(0);
        SET_BL(M8(V(0x206fdc1c)));
        kind = BX;
        SET_BX(BX & 0xf);
        edi = G;
        SET_AX(M16(edi + 0x248));
        if ((int16_t)M16(edi + 0x566) < 0) {
            SET_AL(AL + 2);
            SET_AX(AX & 3);
        }
        if (BX == 0xd || BX == 0xe) {
            if (BX == 0xe)
                SET_AX(AX + 4);
            SET_BX(AX + AX);
            ebx = BX;
            SET_AX(retail_word(V(0x206fde29) + ebx));
            M16(V(0x206fdc14)) = AX;
            SET_AX(-AX + retail_word(V(0x206fde39) + ebx));
            M16(V(0x206fdc12)) = AX;
        } else {
            SET_BX(BX + BX);
            ebx = BX;
            SET_AX(M16(V(0x206fddf5) + ebx));
            M16(V(0x206fdc14)) = AX;
            SET_AX(-AX + M16(V(0x206fde0f) + ebx));
            M16(V(0x206fdc12)) = AX;
        }
        SET_BX(kind >> 4);
        SET_BX(BX + BX);
        ebx = BX;
        SET_AX(retail_word(V(0x206fde51) + ebx));   /* kinds 0x80.. read the pointers after */
        M16(V(0x206fdbfa)) = AX;
        if (M16(V(0x206fdc31)) != 0)
            tile_program_at(V(0x206fe4ac));
        tile_texture_grid();
    }
    edi = G;
    M16(edi + 0xcce) = 0;
    M16(V(0x206fdc37)) = 1;
}

/* 0x206ea8b2 -- 0x3d, a ground tile: GLOBAL +0xca0 cleared, its texture,
 * library and detail switches (+0x39c, +0x11bc, +0x12a4) kept; the
 * operand's eleven words; the detail's step and shift from the first
 * texture origin's magnitude against 0x206fdc4b's five words.  With a
 * mode: the height (less 0x206f91d9's) added to GLOBAL +0x30a's 48 bits,
 * the eye moved (0x206e63ee), then mode 0x22 apart, the corners as table
 * points 0..3 and the mode's drawing. */
static void op_tile(void)
{
    uint32_t r, borrow, carry;

    edi = G;
    M16(edi + 0xca0) = 0;
    M16(V(0x206fdc31)) = M16(edi + 0x39c);
    M16(V(0x206fdc33)) = M16(edi + 0x11bc);
    M16(V(0x206fdc35)) = M16(edi + 0x12a4);
    esi += 2;
    lods16();
    M16(V(0x206fdc1c)) = AX;
    lods16();
    M16(V(0x206fdc1e)) = AX;
    SET_BL(AL);
    lods16();
    M16(V(0x206fdc1f)) = AX;
    lods16();
    M16(V(0x206fdc21)) = AX;
    lods16();
    M16(V(0x206fdc23)) = AX;
    lods16();
    M16(V(0x206fdc25)) = AX;
    lods16();
    M16(V(0x206fdc39)) = AX;
    lods16();
    M16(V(0x206fdc3b)) = AX;
    lods16();
    M16(V(0x206f91e1)) = AX;
    SET_DX(1);
    M16(V(0x206fdc57)) = 0;
    SET_CL(5);
    SET_AX(sar16(AX, CL));
    if (AX & 0x8000)
        SET_AX(~AX);
    edi = V(0x206fdc4b);
    do {
        SET_BP(M16(edi));
        if ((int16_t)AX < (int16_t)BP)
            break;
        edi += 2;
        SET_DX(DX + DX);
        M16(V(0x206fdc57)) += 1;
    } while (edi != V(0x206fdc55));
    M16(V(0x206fdc55)) = DX;
    lods16();
    M16(V(0x206f91e9)) = AX;
    lods16();
    M16(V(0x206fdc27)) = AX;
    M32(V(0x206f9126)) = esi;
    if (BL == 0)
        return;
    r = (uint32_t)M16(V(0x206fdc39)) - M16(V(0x206f91d9));
    SET_AX(r);
    borrow = (r >> 16) & 1;
    r = (uint32_t)M16(V(0x206fdc3b)) - M16(V(0x206f91db)) - borrow;
    SET_CX(r);
    borrow = (r >> 16) & 1;
    SET_DX(0 - borrow);
    edi = G;
    r = (uint32_t)M16(edi + 0x30a) + AX;
    M16(edi + 0x30a) = (uint16_t)r;
    carry = r >> 16;
    r = (uint32_t)M16(edi + 0x30c) + CX + carry;
    M16(edi + 0x30c) = (uint16_t)r;
    carry = r >> 16;
    M16(edi + 0x30e) = (uint16_t)(M16(edi + 0x30e) + DX + carry);
    eye_from_offsets();
    if (M8(V(0x206fdc1e)) == 0x22) {
        tile_mode_22();
        return;
    }
    SET_AL(M8(V(0x206fdc1c)));
    tile_texture();
    M32(V(0x206fdc2d)) = eax;
    M16(V(0x206f91ef)) = (uint16_t)-(M16(V(0x206fdc1f)) >> 1);
    M16(V(0x206f91f1)) = (uint16_t)-(M16(V(0x206fdc23)) >> 1);
    SET_AX(M16(V(0x206f91ef)) + M16(V(0x206fdc1f)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)));
    SET_DI(0);
    table_point();
    SET_AX(M16(V(0x206f91ef)) + M16(V(0x206fdc1f)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)) + M16(V(0x206fdc23)));
    SET_DI(1);
    table_point();
    SET_AX(M16(V(0x206f91ef)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)) + M16(V(0x206fdc23)));
    SET_DI(2);
    table_point();
    SET_AX(M16(V(0x206f91ef)));
    SET_BX(0);
    SET_CX(M16(V(0x206f91f1)));
    SET_DI(3);
    table_point();
    switch (M8(V(0x206fdc1e))) {
    case 0x11:
    case 0x55:
        tile_mode_11();
        break;
    case 0x33:
        tile_mode_33();
        break;
    case 0x44:
        tile_mode_44();
        break;
    }
}

/* 0x206eb66b -- 0x7f, scattered buildings alone (as kind 2). */
static void op_scatter(void)
{
    M16(V(0x206fdc1c)) = 2;
    tile_scatter();
    M32(V(0x206f9126)) += 2;
}

/* 0x206eb5dc -- called from built-in programs (0x8c): the view turned by
 * a heading of 0x1536 for WINDOW's cofactors (+0x20), then put back but
 * for the eye, the turn's; GLOBAL +0x3a8 1 (the cofactors are this
 * frame's). */
static void view_turned_cofactors(void)
{
    struct transform_state t;

    transform_push(&t);
    edi = G;
    M32(edi + 0x5ca) = 0;
    M32(edi + 0x5ce) = 0;
    M32(edi + 0x5d2) = 0x1536;
    transform_turn();
    WINDOW_COFACTORS();
    edi = G;
    ebx = M32(edi + 0xcdc);
    ecx = M32(edi + 0xce0);
    edx = M32(edi + 0xce4);
    transform_pop(&t);
    edi = G;
    M32(edi + 0xcdc) = ebx;
    M32(edi + 0xce0) = ecx;
    M32(edi + 0xce4) = edx;
    M16(edi + 0x3a8) = 1;
}

/* ---- CHAIN's callbacks ------------------------------------------------------------- */

/* A CHAIN callback is entered as a service is, (a, b) and ret 8. */

/* ---- the scene: .text+0x38de..+0x4d53, +0x5225..+0x5253, +0xa681..+0xa8a3,
 * +0x1345e..+0x134bd -------------------------------------------------------------- */

/* The scenery round the eye is a program G3D builds from FE's stream
 * (+0x18) into a block from the host (0x206fd821, the one being built;
 * 0x206fd829, the one drawn), a piece a frame (0x206fd77a, the builder's
 * state, through 0x206fd839) or all at once when the eye has moved far.
 * Beside it: the areas program at 0x206fd72a, FE's records of type 0x65
 * (ground heights and their areas) packed into a 'v' model, run to set the
 * ground under the eye. */

#define FE_SCENE(dst, size, got, x) (eax = IMP4(FE, 0x18, dst, size, got, x))
#define FE_NEXT(kind, out, len)     (eax = IMP3(FE, 0x20, kind, out, len))
#define FE_RESET(which)             (eax = IMP1(FE, 0x44, which))
#define FE_LEVEL(level)             (eax = IMP1(FE, 0x48, level))
#define FE_SEARCH(pos, radius, box) (eax = IMP3(FE, 0x54, pos, radius, box))
#define FE_LAT_BAND(p, level)       (eax = IMP2(FE, 0x60, p, level))
#define FE_BAND_LAT(b, level, out)  (eax = IMP3(FE, 0x64, b, level, out))
#define FE_LON_BAND(p, level)       (eax = IMP2(FE, 0x68, p, level))
#define FE_BAND_LON(b, level, out)  (eax = IMP3(FE, 0x6c, b, level, out))
#define H_BUSY(on)                  (eax = IMP1(HOST, 0x50, on))
#define CHAIN_SEND(list, a, b)      (eax = IMP3(CHAIN, 0x20, list, a, b))

static void clear_unloaded_body(const uint32_t *arg);
static void op_texture_name(void);

/* draw_model() as retail's 0x206e15ee is called from inside: ebx, esi,
 * edi and ebp kept. */
static void draw_model_kept(uint32_t model)
{
    uint32_t b = ebx, s = esi, i = edi, p = ebp;

    draw_model(model);
    ebx = b, esi = s, edi = i, ebp = p;
}

/* The longitude from the eye (GLOBAL +0x56a's 48 bits) to `at`'s, its
 * magnitude in one's complement, to 0x206fd809.., and in metres at the
 * eye's latitude (UTIL +0x48) to 0x206fd815.. */
static void scene_lon_distance(uint32_t lo_at, uint32_t hi_at, int from_eye)
{
    uint32_t r, borrow;

    edi = G;
    if (from_eye) {
        eax = M32(edi + 0x56c);
        SET_CX(M16(edi + 0x56a));
        r = (uint32_t)CX - M16(lo_at);
        SET_CX(r);
        borrow = (r >> 16) & 1;
        eax = eax - M32(hi_at) - borrow;
    } else {
        SET_CX(M16(lo_at));
        eax = M32(hi_at);
        r = (uint32_t)CX - M16(edi + 0x56a);
        SET_CX(r);
        borrow = (r >> 16) & 1;
        eax = eax - M32(edi + 0x56c) - borrow;
    }
    if ((int32_t)eax < 0) {
        eax = ~eax;
        SET_CX(~CX);
    }
    M16(V(0x206fd80b)) = CX;
    M32(V(0x206fd80d)) = eax;
    ebx = edi + 0x560;
    UTIL_LON_TO_M(V(0x206fd809), ebx, V(0x206fd815));
}

/* 0x206e4bf2 -- the scene's bands at the level (0x206f919a): latitude and
 * longitude of the search box's edges (GLOBAL +0x3e4..+0x3f0) as bands and
 * back as positions (0x206fd6e8..), widened by 1000 m north and south and by
 * 1000 m of longitude (UTIL +0x40) east and west. */
static void scene_bands(void)
{
    uint32_t r, c;

    edi = G;
    FE_LAT_BAND(edi + 0x3e8, M16(V(0x206f919a)));
    M16(V(0x206fd70a)) = AX;
    FE_BAND_LAT(M16(V(0x206fd70a)), M16(V(0x206f919a)), V(0x206fd6f0));
    FE_LAT_BAND(edi + 0x3e4, M16(V(0x206f919a)));
    M16(V(0x206fd708)) = AX;
    FE_BAND_LAT(M16(V(0x206fd708)), M16(V(0x206f919a)), V(0x206fd6e8));
    FE_LON_BAND(edi + 0x3ec, M16(V(0x206f919a)));
    M16(V(0x206fd70c)) = AX;
    FE_BAND_LON(M16(V(0x206fd70c)), M16(V(0x206f919a)), V(0x206fd6f8));
    FE_LON_BAND(edi + 0x3f0, M16(V(0x206f919a)));
    M16(V(0x206fd70e)) = AX;
    FE_BAND_LON(M16(V(0x206fd70e)), M16(V(0x206f919a)), V(0x206fd700));
    M32(V(0x206fd6ec)) -= 0x3e8;
    M32(V(0x206fd6f4)) += 0x3e8;
    M32(V(0x206fd811)) = 0x3e8;
    ebx = edi + 0x560;
    UTIL_M32_TO_LON(V(0x206fd811), ebx, V(0x206fd815));
    SET_AX(M16(V(0x206fd817)));
    r = (uint32_t)M16(V(0x206fd702)) + AX;
    M16(V(0x206fd702)) = (uint16_t)r;
    c = r >> 16;
    eax = M32(V(0x206fd819));
    M32(V(0x206fd704)) = M32(V(0x206fd704)) + eax + c;
    SET_AX(M16(V(0x206fd817)));
    r = (uint32_t)M16(V(0x206fd6fa)) - AX;
    M16(V(0x206fd6fa)) = (uint16_t)r;
    c = (r >> 16) & 1;
    eax = M32(V(0x206fd819));
    M32(V(0x206fd6fc)) = M32(V(0x206fd6fc)) - eax - c;
}

/* 0x206e4e4a -- the box drawn: its corners' positions (0x206f92fc..) and
 * bands (0x206fd6e0..). */
static void scene_box(void)
{
    M16(V(0x206f92fc)) = M16(V(0x206fd6ea));
    M32(V(0x206f92fe)) = M32(V(0x206fd6ec));
    M16(V(0x206f9304)) = M16(V(0x206fd6f2));
    M32(V(0x206f9306)) = M32(V(0x206fd6f4));
    M16(V(0x206f930c)) = M16(V(0x206fd6fa));
    M32(V(0x206f930e)) = M32(V(0x206fd6fc));
    M16(V(0x206f9314)) = M16(V(0x206fd702));
    eax = M32(V(0x206fd704));
    M32(V(0x206f9316)) = eax;
    M16(V(0x206fd6e0)) = M16(V(0x206fd708));
    M16(V(0x206fd6e2)) = M16(V(0x206fd70a));
    M16(V(0x206fd6e4)) = M16(V(0x206fd70c));
    SET_AX(M16(V(0x206fd70e)));
    M16(V(0x206fd6e6)) = AX;
}

/* 0x206e4ed3 -- the areas program (0x206fd72a): the eye's position kept
 * (0x206fd79f..), a search box of 0x206fd760 round it for FE (+0x54, also
 * as the box at GLOBAL +0x3e4), then every record FE has there (+0x20) of
 * type 0x65 appended after its 14-byte header while it fits in 0xff8 bytes;
 * a return, and 'v' first. */
static void scene_areas(void)
{
    if (M32(V(0x206fd72a)) == 0)
        return;
    edi = G;
    M16(V(0x206fd79f)) = M16(edi + 0x562);
    M32(V(0x206fd7a1)) = M32(edi + 0x564);
    M16(V(0x206fd7a7)) = M16(edi + 0x56a);
    M32(V(0x206fd7a9)) = M32(edi + 0x56c);
    ebx = edi + 0x560;
    ecx = edi + 0x3f4;
    FE_SEARCH(ebx, M32(V(0x206fd760)), ecx);
    M32(edi + 0x3e4) = M32(edi + 0x3f4);
    M32(edi + 0x3e8) = M32(edi + 0x3f8);
    M32(edi + 0x3ec) = M32(edi + 0x3fc);
    eax = M32(edi + 0x400);
    M32(edi + 0x3f0) = eax;
    SET_AX(0);
    edi = M32(V(0x206fd72a)) + 2;
    for (;;) {
        uint32_t keep = edi, i;

        FE_NEXT(AX, V(0x206fd851), V(0x206fd855));
        {
            uint32_t found = eax;

            eax = M32(V(0x206fd855));
            esi = M32(V(0x206fd851));
            edi = keep;
            if (found == 0)
                break;
        }
        if (M8(esi) == 0x65) {
            eax -= 0xe;
            esi += 0xe;
            ebx = eax + edi - M32(V(0x206fd72a));
            if (ebx < 0xff8) {
                for (i = 0; i < eax; i++)
                    M8(edi + i) = M8(esi + i);
                edi += eax;
                esi += eax;
                ecx = 0;
            }
        }
        SET_AX(1);
    }
    SET_AX(0x22);
    M16(edi) = AX;
    edi += 2;
    edi = M32(V(0x206fd72a));
    SET_AX(0x76);
    M16(edi) = AX;
    edi += 2;
}

/* 0x206e5596 / 0x206e562e -- the builder's state put back from / kept at
 * 0x206fd7d9..: the boxes (GLOBAL +0x3e4..+0x400), the write pointer, the
 * stream's size, the level, 0x206f91a1 and GLOBAL +0x3e0. */
static void scene_resume(void)
{
    uint32_t i;

    edi = G;
    for (i = 0; i < 8; i++)
        M32(edi + 0x3e4 + 4 * i) = M32(V(0x206fd7d9) + 4 * i);
    M32(V(0x206fd825)) = M32(V(0x206fd7f9));
    eax = M32(V(0x206fd7fd));
    M32(V(0x206fd82d)) = eax;
    M16(V(0x206f919a)) = M16(V(0x206fd801));
    M16(V(0x206f91a1)) = M16(V(0x206fd803));
    SET_AX(M16(V(0x206fd805)));
    M16(edi + 0x3e0) = AX;
}

static void scene_pause(void)
{
    uint32_t i;

    edi = G;
    for (i = 0; i < 8; i++)
        M32(V(0x206fd7d9) + 4 * i) = M32(edi + 0x3e4 + 4 * i);
    M32(V(0x206fd7f9)) = M32(V(0x206fd825));
    eax = M32(V(0x206fd82d));
    M32(V(0x206fd7fd)) = eax;
    M16(V(0x206fd801)) = M16(V(0x206f919a));
    M16(V(0x206fd803)) = M16(V(0x206f91a1));
    SET_AX(M16(edi + 0x3e0));
    M16(V(0x206fd805)) = AX;
}

/* 0x206e56c6 -- the near part: kind 9, search boxes of 0x206fd758 (GLOBAL
 * +0x3f4) and 0x206fd75c (+0x3e4) round the eye, the level (FE +0x48). */
static void scene_near(void)
{
    edi = G;
    M16(edi + 0x3e0) = 9;
    FE_SEARCH(edi + 0x560, M32(V(0x206fd758)), edi + 0x3f4);
    FE_SEARCH(edi + 0x560, M32(V(0x206fd75c)), edi + 0x3e4);
    FE_LEVEL(M16(V(0x206f919a)));
}

/* 0x206e576e -- the far part: the eye's position kept (0x206fd712..), kind
 * 5, a box of 0x206fd754, and the level by the latitude's magnitude past
 * that and 8 against the five edges at 0x206fd764 (13 down to 8). */
static void scene_far(void)
{
    edi = G;
    M32(V(0x206fd714)) = M32(edi + 0x564);
    M16(V(0x206fd712)) = M16(edi + 0x562);
    M32(V(0x206fd71c)) = M32(edi + 0x56c);
    M16(V(0x206fd71a)) = M16(edi + 0x56a);
    FE_SEARCH(edi + 0x560, M32(V(0x206fd754)), edi + 0x3e4);
    M16(edi + 0x3e0) = 5;
    eax = M32(edi + 0x564);
    if ((int32_t)eax < 0)
        eax = (uint32_t)-(int32_t)eax;
    eax -= M32(V(0x206fd754));
    eax -= 8;
    SET_CL(0xd);
    ebx = V(0x206fd764);
    do {
        edx = eax - M32(ebx);
        if ((int32_t)edx < 0)
            break;
        ebx += 4;
        SET_CL(CL - 1);
    } while (CL != 8);
    M8(V(0x206f919a)) = CL;
    FE_LEVEL(M16(V(0x206f919a)));
}

/* 0x206e5842 -- a block for the scene (0x206fd81d bytes) from the host, a
 * 'v' and the far part. */
static void scene_alloc(void)
{
    H_ALLOC(2, M32(V(0x206fd81d)), V(0x206fd821), 1);
    esi = M32(V(0x206fd821));
    M32(V(0x206fd825)) = esi;
    M16(esi) = 0x76;
    M32(V(0x206fd825)) += 2;
    scene_far();
}

/* 0x206e5894 -- the room left in the block, less 16 (negative: full). */
static int32_t scene_room(void)
{
    eax = M32(V(0x206fd821)) + M32(V(0x206fd81d)) - M32(V(0x206fd825)) - 0x10;
    return (int32_t)eax;
}

/* 0x206e58a9 -- FE's stream into the room (eax): positive while it gave
 * a piece (its size at 0x206fd831). */
static int32_t scene_stream(void)
{
    M32(V(0x206fd82d)) = eax;
    FE_SCENE(M32(V(0x206fd825)), M32(V(0x206fd82d)), V(0x206fd831), V(0x206fd835));
    return (int32_t)eax;
}

/* 0x206e58e9 -- the far part closed: 0x48 0 (a level jump to nothing), the
 * bands. */
static void scene_far_end(void)
{
    eax = M32(V(0x206fd825));
    M16(eax) = 0x48;
    M16(eax + 2) = 0;
    M32(V(0x206fd825)) += 4;
    scene_bands();
}

/* 0x206e5906 -- the near part's next piece, behind a 0x8d (its operand
 * filled after, 0x206e5d44). */
static int32_t scene_stream_near(void)
{
    M32(V(0x206fd82d)) = eax;
    esi = M32(V(0x206fd825));
    M16(esi) = 0x8d;
    M32(esi + 2) = 0;
    M32(V(0x206fd825)) += 6;
    FE_SCENE(M32(V(0x206fd825)), M32(V(0x206fd82d)), V(0x206fd831), V(0x206fd835));
    return (int32_t)eax;
}

/* 0x206e594b -- a near piece taken: the 0x8d's operand from FE's other
 * answer (0x206fd835); none, and the 0x8d goes; an odd size padded with a
 * three-byte 0x86. */
static void scene_take_near(void)
{
    esi = M32(V(0x206fd825));
    eax = M32(V(0x206fd835));
    M32(esi - 4) = eax;
    eax = M32(V(0x206fd831));
    M32(V(0x206fd825)) += eax;
    if (eax == 0) {
        M32(V(0x206fd825)) -= 6;
    } else if (eax & 1) {
        eax = M32(V(0x206fd825));
        M32(V(0x206fd825)) += 3;
        M16(eax) = 0x86;
        M8(eax + 2) = 0;
    }
}

/* 0x206e5986 -- the new scene in: the old block freed, GLOBAL +0x1164 and
 * +0x1169 cleared, the new one to 0x206fd829, the box. */
static void scene_swap(void)
{
    if (M32(V(0x206fd829)) != 0)
        H_FREE(V(0x206fd829));
    eax = G;
    M32(eax + 0x1164) = 0;
    M8(eax + 0x1169) = 0;
    eax = M32(V(0x206fd821));
    M32(V(0x206fd829)) = eax;
    M32(V(0x206fd821)) = 0;
    M32(V(0x206fd825)) = 0;
    scene_box();
}

/* 0x206e59f3 -- the whole scene at once: the far part streamed while it
 * comes, its end (0x206fd726), the near part, a return, the areas, in. */
static void scene_build(void)
{
    scene_alloc();
    while (scene_room() >= 0 && scene_stream() > 0)
        M32(V(0x206fd825)) += M32(V(0x206fd831));
    ebx = M32(V(0x206fd825));
    M32(V(0x206fd726)) = ebx;
    scene_far_end();
    scene_near();
    while (scene_room() >= 0 && scene_stream_near() > 0)
        scene_take_near();
    eax = M32(V(0x206fd825));
    M16(eax) = 0x22;
    scene_areas();
    scene_swap();
}

/* 0x206e5c77 -- a scene being built dropped: FE reset, the block freed. */
static void scene_abort(void)
{
    FE_RESET(0);
    M32(V(0x206fd77a)) = 0;
    H_FREE(V(0x206fd821));
    M32(V(0x206fd825)) = 0;
}

/* 0x206e4b12 -- rebuilt at once while the scene is on (GLOBAL +0x390 2),
 * the busy cursor up: the unloaded textures' flags cleared (+0x58), the
 * view's number kept (0x206fd782), the ground's easing reset. */
static void scene_rebuild(void)
{
    edi = G;
    if (M32(edi + 0x390) != 2)
        return;
    H_BUSY(1);
    clear_unloaded_body(0);
    edi = G;
    SET_AX(M16(edi + 0x424));
    M16(V(0x206fd782)) = AX;
    M32(V(0x206fd74c)) = 0xffffffffu;
    scene_build();
    edi = G;
    M8(edi + 0x444) &= 0xf3;
    H_BUSY(0);
}

/* 0x206e4a36 -- whether the scene must change: none yet, rebuild; one
 * being built, dropped and rebuilt; the view changed or no area under the
 * eye, rebuild; else, the eye far enough from where it was built (its
 * latitude difference or'd with the longitude's in metres, against
 * 0x206fd758), built a piece a frame from state 1. */
static void scene_check(void)
{
    edi = G;
    if (M32(V(0x206fd829)) == 0) {
        scene_rebuild();
        return;
    }
    if (M32(V(0x206fd77a)) != 0) {
        scene_abort();
        scene_rebuild();
        return;
    }
    SET_AX(M16(edi + 0x424));
    if (AX != M16(V(0x206fd782)) || M16(V(0x206fd736)) == 0) {
        scene_rebuild();
        return;
    }
    scene_lon_distance(V(0x206fd71a), V(0x206fd71c), 1);
    ebx = M32(edi + 0x564) - M32(V(0x206fd714));
    if ((int32_t)ebx < 0)
        ebx = (uint32_t)-(int32_t)ebx;
    ebx |= M32(V(0x206fd819));
    if (ebx >= M32(V(0x206fd758))) {
        scene_rebuild();
        return;
    }
    M16(V(0x206f9176)) = 0xf;
    M32(V(0x206fd77a)) = 1;
}

/* 0x206e49de -- the areas block (0x1000 bytes) on first use, and the check
 * once (0x206f9174). */
static void scene_once(void)
{
    if (M32(V(0x206fd72a)) == 0) {
        H_ALLOC(2, 0x1000, V(0x206fd72a), 1);
        M8(V(0x206f9174)) = 0;
    }
    if (M8(V(0x206f9174)) == 0) {
        scene_check();
        M8(V(0x206f9174)) += 1;
    }
}

/* 0x206e5b73 .. 0x206e5c58 -- the builder's states: 1 a block and the far
 * part's start; 2 the far part, 0x206f9176 pieces a frame; 3 the near
 * part's start; 4 the near part, as many; 5 in. */
static void scene_state_1(void)
{
    edi = G;
    SET_AX(M16(edi + 0x424));
    M16(V(0x206fd782)) = AX;
    scene_alloc();
    M32(V(0x206fd77a)) = 2;
    scene_pause();
}

static void scene_state_2(void)
{
    int16_t n;

    scene_resume();
    n = (int16_t)M16(V(0x206f9176));
    for (;;) {
        if (scene_room() < 0 || scene_stream() <= 0)
            break;
        M32(V(0x206fd825)) += M32(V(0x206fd831));
        SET_CX(--n);
        if (!(n > 0)) {
            scene_pause();
            return;
        }
    }
    SET_CX(n);
    ebx = M32(V(0x206fd825));
    M32(V(0x206fd77e)) = ebx;
    scene_far_end();
    M32(V(0x206fd77a)) = 3;
    scene_pause();
}

static void scene_state_3(void)
{
    scene_resume();
    scene_near();
    M32(V(0x206fd77a)) = 4;
    scene_pause();
}

static void scene_state_4(void)
{
    int16_t n;

    scene_resume();
    n = (int16_t)M16(V(0x206f9176));
    for (;;) {
        if (scene_room() < 0 || scene_stream_near() <= 0)
            break;
        scene_take_near();
        SET_CX(--n);
        if (!(n > 0)) {
            scene_pause();
            return;
        }
    }
    SET_CX(n);
    eax = M32(V(0x206fd825));
    M16(eax) = 0x22;
    scene_areas();
    M32(V(0x206fd77a)) = 5;
}

static void scene_state_5(void)
{
    scene_resume();
    eax = M32(V(0x206fd77e));
    M32(V(0x206fd726)) = eax;
    M32(V(0x206fd77a)) = 0;
    scene_swap();
}

/* 0x206e5ae7 -- a frame's step: GLOBAL +0x444 bit 2 rebuilds at once (a
 * build in progress dropped); bit 3 (dropping one too) or bit 5 counts
 * 0x206f91a0 down, clears bit 3 and checks; else a build in progress is
 * dropped once the scene is off, or goes on a state (0x206fd839). */
static void scene_step(void)
{
    edi = G;
    if (M16(edi + 0x444) & 4) {
        if (M32(V(0x206fd77a)) != 0)
            scene_abort();
        scene_rebuild();
        return;
    }
    if ((M16(edi + 0x444) & 8) || (M16(edi + 0x444) & 0x20)) {
        if ((M16(edi + 0x444) & 8) && M32(V(0x206fd77a)) != 0)
            scene_abort();
        M8(V(0x206f91a0)) -= 1;
        M16(edi + 0x444) &= 0xfff7;
        scene_check();
        return;
    }
    ebx = M32(V(0x206fd77a));
    if (ebx != 0) {
        edi = G;
        if (M32(edi + 0x390) != 2) {
            scene_abort();
            return;
        }
    }
    if (ebx > 6)
        return;
    routine(M32(V(0x206fd839) + ebx * 4))();
}

/* 0x206e504f -- the ground under the eye, while it is within 0x10000 of
 * where the scene was built (each way; their squares' sum under 0x2ebae40,
 * or 0x4d3f640 with a build in progress -- else the scene checked): the
 * pieces a frame by the distance (15, 25, 50 or 0x7d00); with GLOBAL
 * +0x444 bit 3 none of the areas.  Each area in the scene (0x44-byte
 * records from 0x206fd829 + 2 to 0x206fd726) whose ellipse (scale +0x14,
 * half sizes +0x34, +0x30 about +0x1a, +0x1e) takes in the eye gives the
 * ground height (+0x38, +0x3a: eased in unless 0x206fd74c is negative)
 * and, for kind 0xf0.., clears GLOBAL +0x11ec; the first one ends the
 * search.  Then the areas program run with the eye at the reference and
 * the identity matrix, GLOBAL +0x20 from the area's height (the dword at
 * 0x206fd731, its low byte the word below). */
static void scene_ground(void)
{
    uint32_t far_;

    scene_once();
    edi = G;
    eax = M32(edi + 0x564) - M32(V(0x206fd714));
    cdq();
    eax = (eax ^ edx) - edx;
    if ((int32_t)eax > 0x10000)
        goto check;
    far_ = eax;
    scene_lon_distance(V(0x206fd71a), V(0x206fd71c), 1);
    eax = far_;
    if ((int32_t)M32(V(0x206fd819)) > 0x10000)
        goto check;
    {
        uint64_t a = (uint64_t)eax * eax, b;

        ecx = (uint32_t)a;
        b = (uint64_t)M32(V(0x206fd819)) * M32(V(0x206fd819));
        edx = (uint32_t)(b >> 32);
        eax = (uint32_t)b + ecx;
        if (eax < ecx)
            goto check;
        edx = M32(V(0x206fd77a)) == 0 ? 0x2ebae40 : 0x4d3f640;
        if (eax > edx)
            goto check;
        SET_DX(0x7d00);
        if (eax <= 0x2ebae40) {
            SET_DX(0x32);
            if (eax <= 0x17d7840) {
                SET_DX(0x19);
                if (eax <= 0x895440)
                    SET_DX(0xf);
            }
        }
        M16(V(0x206f9176)) = DX;
    }
    edi = G;
    if (M8(edi + 0x444) & 8) {
        M16(V(0x206fd736)) = 0;
        goto check;
    }
    M16(V(0x206fd736)) = 0;
    M32(V(0x206fd750)) = M32(V(0x206fd829)) + 2;
    for (;;) {
        uint64_t q;

        esi = M32(V(0x206fd750));
        if (esi >= M32(V(0x206fd726)))
            break;
        edi = G;
        eax = M32(edi + 0x564) - M32(esi + 0x1a);
        cdq();
        eax = (eax ^ edx) - edx;
        q = (uint64_t)eax * M32(esi + 0x14);
        edx = (uint32_t)(q >> 32);
        eax = (uint32_t)(q >> 16);
        ebx = M32(esi + 0x34) >> 1;
        eax -= ebx;
        if ((int32_t)eax >= 0)
            goto next;
        {
            uint32_t keep = esi;

            scene_lon_distance(esi + 0x1e, esi + 0x20, 0);
            esi = keep;
        }
        edx = M32(V(0x206fd819));
        q = (uint64_t)M32(esi + 0x14) * edx;
        edx = (uint32_t)(q >> 32);
        eax = (uint32_t)(q >> 16);
        ebx = M32(esi + 0x30) >> 1;
        eax -= ebx;
        if ((int32_t)eax >= 0)
            goto next;
        SET_AX(M16(esi + 0x38));
        M16(V(0x206fd730)) = AX;
        M16(V(0x206fd73a)) = AX;
        eax = (uint32_t)(int32_t)(int16_t)M16(esi + 0x3a);
        M32(V(0x206fd732)) = eax;
        M32(V(0x206fd73c)) = eax;
        if ((int32_t)M32(V(0x206fd74c)) < 0) {
            SET_AX(M16(V(0x206fd73a)));
            M16(V(0x206f91d9)) = AX;
            eax = M32(V(0x206fd73c));
            M32(V(0x206f91db)) = eax;
            M32(V(0x206fd74c)) = 0;
        }
        SET_AX(M16(esi + 0x2e));
        if (AH == 0xf0) {
            edi = G;
            M8(edi + 0x11ec) = 0;
            M16(V(0x206fd724)) = 1;
        }
        M16(V(0x206fd736)) += 1;
        break;
next:
        M32(V(0x206fd750)) += 0x44;
    }
    if (M16(V(0x206fd736)) == 0)
        M16(V(0x206fd736)) += 1;
    M32(V(0x206fd720)) = M32(V(0x206fd732));
    edi = G;
    M16(edi + 0x5b2) = M16(edi + 0x562);
    M32(edi + 0x5b4) = M32(edi + 0x564);
    M16(edi + 0x5ba) = M16(edi + 0x56a);
    M32(edi + 0x5bc) = M32(edi + 0x56c);
    M16(edi + 0x5c2) = M16(edi + 0x572);
    M32(edi + 0x5c4) = M32(edi + 0x574);
    M16(edi + 0xce8) = 0x7fff;
    M16(edi + 0xcea) = 0;
    M16(edi + 0xcec) = 0;
    M16(edi + 0xcee) = 0;
    M16(edi + 0xcf0) = 0x7fff;
    M16(edi + 0xcf2) = 0;
    M16(edi + 0xcf4) = 0;
    M16(edi + 0xcf6) = 0;
    M16(edi + 0xcf8) = 0x7fff;
    draw_model_kept(M32(V(0x206fd72a)));
    eax = M32(V(0x206fd720));
    M32(V(0x206fd732)) = eax;
    eax = M32(V(0x206fd731));
    edx = eax;
    { uint32_t t = M32(edi + 0x20); M32(edi + 0x20) = edx; edx = t; }
    return;
check:
    scene_check();
}

/* 0x206e5008 -- with 0x206fd778 counting down, the ground at once (its
 * height put in without easing); else while the scene is on. */
static void scene_ground_step(void)
{
    if (M16(V(0x206fd778)) != 0) {
        M16(V(0x206fd778)) -= 1;
        scene_ground();
        SET_AX(M16(V(0x206fd73a)));
        M16(V(0x206f91d9)) = AX;
        eax = M32(V(0x206fd73c));
        M32(V(0x206f91db)) = eax;
        M32(V(0x206fd74c)) = 0;
        return;
    }
    edi = G;
    if (M32(edi + 0x390) == 2)
        scene_ground();
}

/* 0x206e6225 -- the second queue (0x206fd880, what 0x75 queued) drawn
 * whole and emptied, in the first's place meanwhile. */
static void draw_second_queue(void)
{
    uint32_t keep = M32(V(0x206fd87c));

    eax = M32(V(0x206fd880));
    M32(V(0x206fd87c)) = eax;
    if (eax != 0) {
        queue_draw(0x7fffffff, 0x80000001, 1);
        eax = M32(V(0x206fd87c));
        M32(V(0x206fd880)) = eax;
        queue_alloc();
    }
    M32(V(0x206fd87c)) = keep;
}

/* 0x206f445e -- the programs 0x74 and 0x8b kept in the 64 lists
 * (0x20710360: each a count, then 12-byte entries of program, context and
 * variable context at +0x18) run and the lists emptied. */
static void drain_lists(void)
{
    int32_t n;

    ecx = 0x40;
    esi = 0;
    for (n = 0x40; n > 0; n--) {
        uint32_t keep_esi = esi;

        ecx = 0;
        { uint32_t t = M32(V(0x20710360) + esi); M32(V(0x20710360) + esi) = ecx; ecx = t; }
        if (ecx != 0) {
            esi = M32(V(0x20710378) + esi);
            do {
                uint32_t c = ecx, s = esi;

                eax = M32(esi + 4);
                M32(V(0x206f919c)) = eax;
                eax = M32(esi);
                M32(V(0x206f9126)) = eax;
                eax = M32(esi + 8);
                M32(V(0x206f91b7)) = eax;
                interpret();
                esi = s, ecx = c;
                esi += 0xc;
            } while (--ecx != 0);
        }
        esi = keep_esi + 0x1c;
    }
    for (esi = 0; esi < 0x40 * 0x1c; esi += 0x1c)
        M32(V(0x20710360) + esi) = 0;
    ecx = 0;
}

/* 0x206e5a57 -- the frame's scene drawn: the scene program (no contexts),
 * then the second queue, CHAIN list 0xf told (0, 0), then the kept
 * programs; the drawing state cleared between. */
static void scene_draw(void)
{
    M32(V(0x206f919c)) = 0;
    M32(V(0x206f91b7)) = 0;
    draw_model_kept(M32(V(0x206fd829)));
    edi = G;
    M16(edi + 0xc90) = 0;
    M8(V(0x206f9125)) = 0;
    M16(V(0x206f90f4)) = 0;
    draw_second_queue();
    CHAIN_SEND(0xf, 0, 0);
    edi = G;
    M16(edi + 0xc90) = 0;
    M8(V(0x206f9125)) = 0;
    M16(V(0x206f90f4)) = 0;
    drain_lists();
}

/* 0x206eb838 -- a ground texture's program by its word (eax; kind 0x22..
 * keeps the low nibble): 0 0x206fe514, 9 0x206fe70e, 7 0x206fe6b2; others
 * by GLOBAL +0xcd2 and the eye's height over the word at GLOBAL +0x21. */
static void tile_texture_program(void)
{
    ebx = eax >> 16;
    if (BL == 0x22)
        SET_AL(AL & 0xf);
    if (AL == 0) {
        eax = V(0x206fe514);
        return;
    }
    if (AL == 9) {
        eax = V(0x206fe70e);
        return;
    }
    if (AL == 7) {
        eax = V(0x206fe6b2);
        return;
    }
    edi = G;
    if (M16(edi + 0xcd2) == 0) {
        eax = V(0x206fea4c);
        return;
    }
    SET_CX(M16(edi + 0x5c4) - M16(edi + 0x21));
    eax = (int16_t)CX > 0x17d4 ? V(0x206fe628) : V(0x206fe7f8);
}

/* 0x206eb681 -- the ground tiles' frame: their switches kept, the four
 * texture programs (GLOBAL +0x414.. through tile_texture_program, or all
 * 0x206fe472 with +0xdce 1), the box's corners and the ground height into
 * the built-in program at 0x206fe108, which is drawn. */
static void tile_frame(void)
{
    uint32_t i;
    static const uint32_t to[4][2] = { { 0x206fdc7b, 0x206fdc93 }, { 0x206fdcab, 0x206fdcc3 },
                                      { 0x206fdc83, 0x206fdccb }, { 0x206fdc9b, 0x206fdcb3 } };

    edi = G;
    M16(V(0x206fdc31)) = M16(edi + 0x39c);
    M16(V(0x206fdc33)) = M16(edi + 0x11bc);
    M16(V(0x206fdc35)) = M16(edi + 0x12a4);
    if (M16(edi + 0xdce) == 1) {
        eax = V(0x206fe472);
        for (i = 0; i < 4; i++)
            M32(V(0x206fdcd9) + 4 * i) = eax;
    } else {
        M16(V(0x206fdc1f)) = 0x7000;
        M16(V(0x206fdc23)) = 0x7000;
        for (i = 0; i < 4; i++) {
            eax = M32(edi + 0x414 + 4 * i);
            tile_texture_program();
            edi = G;
            M32(V(0x206fdcd9) + 4 * i) = eax;
        }
    }
    for (i = 0; i < 4; i++) {
        SET_AX(M16(V(0x206f92fc) + 8 * i));
        M16(V(to[i][0])) = AX;
        eax = M32(V(0x206f92fe) + 8 * i);
        M32(V(to[i][0]) + 2) = eax;
        SET_AX(M16(V(0x206f92fc) + 8 * i));
        M16(V(to[i][1])) = AX;
        eax = M32(V(0x206f92fe) + 8 * i);
        M32(V(to[i][1]) + 2) = eax;
    }
    {
        static const uint32_t ground[4] = { 0x206fdc8b, 0x206fdca3, 0x206fdcbb, 0x206fdcd3 };

        for (i = 0; i < 4; i++) {
            SET_AX(M16(V(0x206f91d9)));
            M16(V(ground[i])) = AX;
            eax = M32(V(0x206f91db));
            M32(V(ground[i]) + 2) = eax;
        }
    }
    draw_model_kept(V(0x206fe108));
}

/* 0x206e48de -- list 0x10, the frame's scene: GLOBAL +0x394 1 when the
 * scene is on (+0x390 2) and not +0xdce; then the tiles' frame, +0x11ec 1
 * with the view's +0x14 0 or 1, the scene kept up, GLOBAL +0x20 to the
 * reference's height (0x206f916a, 48 bits), the scene drawn.  Off, the
 * areas block freed.  0x206fd724 clears +0x11ec. */
SERVICE(cb_206e48de, 8);
static void cb_206e48de_body(const uint32_t *arg)
{
    (void)arg;
    edi = G;
    M32(edi + 0x394) = 0;
    if (M32(edi + 0x390) == 2) {
        if (M16(edi + 0xdce) == 0)
            M32(edi + 0x394) = 1;
        tile_frame();
        edi = G;
        if (M16(edi + 0xdce) == 0) {
            eax = M32(edi + 0x1198);
            if (M32(eax + 0x14) == 0 || M32(eax + 0x14) == 1)
                M8(edi + 0x11ec) = 1;
        }
        scene_once();
        edi = G;
        eax = M32(edi + 0x20);
        cdq();
        edx = (edx << 24) | (eax >> 8);
        eax <<= 8;
        M32(V(0x206f916c)) = edx;
        M16(V(0x206f916a)) = AX;
        scene_draw();
    } else {
        eax = M32(edi + 0x20);
        cdq();
        edx = (edx << 24) | (eax >> 8);
        eax <<= 8;
        M32(V(0x206f916c)) = edx;
        M16(V(0x206f916a)) = AX;
        if (M32(V(0x206fd72a)) != 0) {
            H_FREE(V(0x206fd72a));
            M32(V(0x206fd72a)) = 0;
        }
    }
    edi = G;
    if (M16(V(0x206fd724)) != 0)
        M8(edi + 0x11ec) = 0;
}

/* 0x206e4b99 -- list 1, the frame's step: the builder, the fast routines'
 * threshold by GLOBAL +0xcb4 (0x206fd784), the ground; every other frame
 * (0x206f91b5 halved to 0) the texture by name at 0x206fd78e. */
SERVICE(cb_206e4b99, 8);
static void cb_206e4b99_body(const uint32_t *arg)
{
    (void)arg;
    scene_step();
    edi = G;
    ebx = M16(edi + 0xcb4);
    if (ebx > 2)
        ebx = 0;
    eax = M32(V(0x206fd784) + ebx * 4);
    M32(V(0x206f9178)) = eax;
    M16(V(0x206fd724)) = 0;
    scene_ground_step();
    M16(V(0x206f91b5)) >>= 1;
    if (M16(V(0x206f91b5)) == 0) {
        esi = V(0x206fd78e);
        op_texture_name();
    }
}

/* 0x206e5500 -- list 5, once a frame: the ground's height (0x206f91d9,
 * the dword at 0x206f91db) eased to a new one (0x206fd73a, 0x206fd73c) in
 * 30 steps: a change sets the step ((new - now) / 30) and the count; each
 * frame after adds a step; the last puts the new one in exactly. */
SERVICE(cb_206e5500, 8);
static void cb_206e5500_body(const uint32_t *arg)
{
    eax = arg[0];
    eax = arg[1];
    eax = M32(V(0x206fd73c));
    if (eax != M32(V(0x206fd744))) {
        M32(V(0x206fd744)) = eax;
        eax -= M32(V(0x206f91db));
        cdq();
        ebx = 0x1e;
        idiv32(ebx);
        M32(V(0x206fd748)) = eax;
        M32(V(0x206fd74c)) = 0x1e;
        return;
    }
    if (M32(V(0x206fd74c)) == 0) {
        SET_AX(M16(V(0x206fd73a)));
        M16(V(0x206f91d9)) = AX;
        eax = M32(V(0x206fd73c));
        M32(V(0x206f91db)) = eax;
        M32(V(0x206fd74c)) = 0;
        return;
    }
    if ((int32_t)M32(V(0x206fd74c)) < 0)
        return;
    eax = M32(V(0x206fd748));
    M32(V(0x206f91db)) += eax;
    M32(V(0x206fd74c)) -= 1;
}

/* 0x206f01dc -- list 1, once a frame: the textures aged.  The slot in use,
 * unloaded and not flagged, with the lowest age, loaded, again and again
 * while that succeeds and 0x206ff8aa holds (the count of three at
 * 0x206ff8c4 only calls 0x206f0342, a bare ret, when it runs out, and
 * goes on); a load that fails flags its slot, so the branch that would
 * give it the oldest loaded slot's texture is not reached from here.  Then every slot in use ages by 200
 * (flags bit 1 cleared with 0x206f91a0), and one that passes 0xffff is
 * freed outright -- not while the window is drawn (0x206ff8ac, list 9).
 * GLOBAL +0xcb8 taken: set, 0x206ff8aa is 7; it halves each frame. */
static void age_textures(void)
{
    M16(V(0x206ff8c4)) = 3;
    for (;;) {
        esi = M32(V(0x206ff8b4));
        ebx = esi + 6;
        ecx = M16(esi);
        SET_AX(0xffff);
        edx = 0;
        do {
            if (M8(ebx) != 0 && M32(ebx + 0x14) == 0 && !(M8(ebx + 0xd) & 2) &&
                AX > M16(ebx + 0x12)) {
                SET_AX(M16(ebx + 0x12));
                edx = ebx;
            }
            ebx += M32(V(0x206ff8b0));
        } while (--ecx != 0);
        if (edx == 0)
            break;
        ebx = edx;
        if (!texture_reload()) {
            if (M16(V(0x206ff8aa)) == 0)
                break;
            M16(V(0x206ff8c4)) -= 1;
            continue;
        }
        if (M8(ebx + 0xd) & 2)
            continue;
        edi = M32(V(0x206ff8b4));
        esi = edi + 6;
        ecx = M16(edi);
        SET_AX(M16(ebx + 0x12));
        edx = 0;
        do {
            if (M8(esi) != 0 && M32(esi + 0x14) != 0 && AX < M16(esi + 0x12)) {
                SET_AX(M16(esi + 0x12));
                edx = esi;
            }
            esi += M32(V(0x206ff8b0));
        } while (--ecx != 0);
        if (edx == 0)
            break;
        esi = edx;
        eax = 0;
        M8(esi + 0xd) = AL;
        { uint32_t t = M32(esi + 0x14); M32(esi + 0x14) = eax; eax = t; }
        M32(ebx + 0x14) = eax;
        texture_reload();
        break;
    }
    edi = M32(V(0x206ff8b4));
    ebx = edi + 6;
    ecx = M16(edi);
    do {
        if (M8(ebx) != 0) {
            uint32_t age;

            if (M8(V(0x206f91a0)) != 0)
                M8(ebx + 0xd) &= 0xfd;
            age = (uint32_t)M16(ebx + 0x12) + 0xc8;
            M16(ebx + 0x12) = (uint16_t)age;
            if (age > 0xffff) {
                M16(ebx + 0x12) = 0xffff;
                if (M32(V(0x206ff8ac)) == 0) {
                    uint32_t keep_ebx = ebx, keep_ecx = ecx;

                    eax = 0;
                    M8(ebx) = AL;
                    M8(ebx + 0xd) = AL;
                    { uint32_t t = M32(ebx + 0x14); M32(ebx + 0x14) = eax; eax = t; }
                    HG2D_UNLOAD(eax);
                    ecx = keep_ecx, ebx = keep_ebx;
                }
            }
        }
        ebx += M32(V(0x206ff8b0));
    } while (--ecx != 0);
    eax = 0;
    M8(V(0x206f91a0)) = AL;
    edi = G;
    { uint16_t t = M16(edi + 0xcb8); M16(edi + 0xcb8) = AX; SET_AX(t); }
    if (AX != 0)
        M16(V(0x206ff8aa)) = 7;
    M16(V(0x206ff8aa)) >>= 1;
}

SERVICE(cb_206f01dc, 8);
static void cb_206f01dc_body(const uint32_t *arg)
{
    (void)arg;
    age_textures();
}

/* 0x206f01b0 -- list 9: the same with 0x206ff8ac set, so nothing is freed
 * outright. */
SERVICE(cb_206f01b0, 8);
static void cb_206f01b0_body(const uint32_t *arg)
{
    (void)arg;
    M32(V(0x206ff8ac)) = 1;
    age_textures();
    M32(V(0x206ff8ac)) = 0;
}

static void release_textures(void);

/* 0x206f0322 -- list 0x14: message 0x46 releases every texture. */
SERVICE(cb_206f0322, 8);
static void cb_206f0322_body(const uint32_t *arg)
{
    if (arg[0] == 0x46)
        release_textures();
}

/* 0x206eb8a4 -- the tiles' colour words (0x206fddf5..0x206fde51) through
 * HG2D +0x88, in place. */
static void tile_colours(void)
{
    esi = V(0x206fddf5);
    edi = V(0x206fde51);
    do {
        HG2D_COLOUR(M16(esi), V(0x206f91ff));
        eax = M32(V(0x206f91ff));
        M16(esi) = AX;
        esi += 2;
    } while (esi != edi);
}

/* 0x206eb55a -- list 3: the tiles' colours once (0x206fddf1), then the
 * season's letter (GLOBAL +0x248, two on in the south; 2 when +0x566 lies
 * between 0x206fdc3f and 0x206fdc3d) from 0x206fdc43 into five texture
 * names. */
SERVICE(cb_206eb55a, 8);
static void cb_206eb55a_body(const uint32_t *arg)
{
    eax = arg[0];
    eax = arg[1];
    if (M32(V(0x206fddf1)) == 0) {
        tile_colours();
        M32(V(0x206fddf1)) = 1;
    }
    edi = G;
    SET_BX(M16(edi + 0x248));
    SET_CX(M16(edi + 0x566));
    if ((int16_t)CX < 0) {
        SET_BL(BL + 2);
        SET_BL(BL & 3);
    }
    if ((int16_t)CX <= (int16_t)M16(V(0x206fdc3d)) && (int16_t)CX >= (int16_t)M16(V(0x206fdc3f)))
        SET_BX(2);
    ebx = BX;
    /* unbounded: a season past 3 reads on into the image, pointers and
     * all, as retail's */
    SET_AL(retail_byte(V(0x206fdc43) + ebx - D));
    M8(V(0x206fe563)) = AL;
    M8(V(0x206fe818)) = AL;
    M8(V(0x206fe5ed)) = AL;
    M8(V(0x206fe61c)) = AL;
    M8(V(0x206fe702)) = AL;
}

static void release_lists(void);

/* 0x206f4505 -- list 6: the 64 lists emptied (0x206f4522 does nothing). */
SERVICE(cb_206f4505, 8);
static void cb_206f4505_body(const uint32_t *arg)
{
    eax = arg[0];
    eax = arg[1];
    release_lists();
}

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

/* 0x206f46c3 -- 0x63, a library by its 16-byte id (the operand): found in
 * the list at 0x20710a70, or added there and fetched through FE +0x3c --
 * once for its size (a byte read into the frame), then into a block the
 * host gives (the size read to 0x20710a8c); an id FE has not leaves the
 * entry with no block.  Its count (+0x14) to 3, and with a block it runs
 * from a clean drawing state, as +0x1c, inside this program; on past the
 * id. */
static void op_library(void)
{
    uint32_t keep_esi = esi;
    uint32_t frame[2];                          /* [ebp-8], [ebp-4] */

    ecx = M32(V(0x20710a70));
    edi = M32(V(0x20710a88));
    if (ecx != 0) {
        do {
            if (M32(esi + 2) == M32(edi) && M32(esi + 6) == M32(edi + 4) &&
                M32(esi + 0xa) == M32(edi + 8) && M32(esi + 0xe) == M32(edi + 0xc))
                goto found;
            edi += 0x18;
        } while (--ecx != 0);
    }
    H_LIST_ADD(V(0x20710a70));
    edi = eax;
    M32(edi) = M32(esi + 2);
    M32(edi + 4) = M32(esi + 6);
    M32(edi + 8) = M32(esi + 0xa);
    M32(edi + 0xc) = M32(esi + 0xe);
    M32(edi + 0x10) = 0;
    eax = IMP5(FE, 0x3c, edi, (uint32_t)(uintptr_t)frame + 3, 0, 1,
               (uint32_t)(uintptr_t)&frame[1]);
    if (eax != 0) {
        H_ALLOC(2, frame[1], edi + 0x10, 1);
        eax = IMP5(FE, 0x3c, edi, M32(edi + 0x10), 1, frame[1], V(0x20710a8c));
    }
found:
    M16(edi + 0x14) = 3;
    eax = M32(edi + 0x10);
    if (eax != 0)
        run_program(eax);
    esi = keep_esi;
    M32(V(0x206f9126)) = esi + 0x12;
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

/* Code outside the image that a program's 0x8c names (MAIN's, in retail's
 * built-in programs): called as itself, with the registers the
 * transliteration holds, and what it leaves in them kept. */
uint32_t g3d_native_target;
void native_thunk(void);
__asm__(".text\n\t.p2align 4\n_native_thunk:\n\t"
        "pushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl _g3d_ebx, %ebx\n\tmovl _g3d_esi, %esi\n\tmovl _g3d_edi, %edi\n\t"
        "movl _g3d_ebp, %ebp\n\tmovl _g3d_ecx, %ecx\n\tmovl _g3d_edx, %edx\n\t"
        "movl _g3d_eax, %eax\n\t"
        "call *_g3d_native_target\n\t"
        "movl %eax, _g3d_eax\n\tmovl %ebx, _g3d_ebx\n\tmovl %ecx, _g3d_ecx\n\t"
        "movl %edx, _g3d_edx\n\tmovl %esi, _g3d_esi\n\tmovl %edi, _g3d_edi\n\t"
        "movl %ebp, _g3d_ebp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");

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
    case 0x206e6d23: return op_texture_name;
    case 0x206f46c3: return op_library;
    case 0x206e997d: return op_building;
    case 0x206e4357: return op_approach_lights;
    case 0x206ea8b2: return op_tile;            /* 0x3d */
    case 0x206e434b: return op_runway;          /* 0x42 */
    case 0x206e4351: return op_runway;          /* 0x44 */
    case 0x206f24f9: return runway_threshold_near;
    case 0x206f2505: return runway_threshold_far;
    case 0x206f28b7: return runway_centreline_near;
    case 0x206f28c4: return runway_centreline_far;
    case 0x206f2bfc: return runway_touchdown;
    case 0x206f2ca5: return runway_aiming;
    case 0x206f2d1e: return runway_zone;
    case 0x206f2ea1: return runway_stripes;
    case 0x206f3016: return runway_number_near;
    case 0x206f3044: return runway_number_far;
    case 0x206f3370: return runway_lights;
    case 0x206f32e7: return runway_approach_lights;
    case 0x206eb66b: return op_scatter;         /* 0x7f */
    case 0x206e9e2f: return tile_cell;
    case 0x206eb17d: return tile_scatter_cell;
    case 0x206ea7d0: return tile_texture_origin;
    case 0x206eb5dc: return view_turned_cofactors;
    case 0x206e5b72: return bare_ret;           /* the builder's state 0 */
    case 0x206e5b73: return scene_state_1;
    case 0x206e5b9b: return scene_state_2;
    case 0x206e5bf3: return scene_state_3;
    case 0x206e5c0d: return scene_state_4;
    case 0x206e5c58: return scene_state_5;
    case 0x206eb448: return scattered_eb448;
    case 0x206eb456: return scattered_eb456;
    case 0x206eb464: return scattered_eb464;
    case 0x206eb472: return scattered_eb472;
    case 0x206eb480: return scattered_eb480;
    case 0x206eb48e: return scattered_eb48e;
    case 0x206eb49c: return scattered_eb49c;
    case 0x206eb4aa: return scattered_eb4aa;
    case 0x206eb4b8: return scattered_eb4b8;
    case 0x206eb4c6: return scattered_eb4c6;
    case 0x206eb4d4: return scattered_eb4d4;
    case 0x206eb4e2: return scattered_eb4e2;
    case 0x206eb4f0: return scattered_eb4f0;
    case 0x206eb4fe: return scattered_eb4fe;
    case 0x206eb50c: return scattered_eb50c;
    case 0x206eb51a: return scattered_eb51a;
    case 0x206eb528: return scattered_eb528;
    case 0x206eb536: return scattered_eb536;
    case 0x206eb544: return scattered_eb544;
    case 0x206f3b8b: return lights_1;
    case 0x206f3c42: return lights_2;
    case 0x206f3c2b: return lights_3;
    case 0x206f3cca: return lights_4;
    case 0x206f3cf3: return lights_5;
    case 0x206f3d69: return lights_6;           /* 6 and 7 */
    case 0x206f3bef: return lights_8;
    case 0x206f3eb1: return lights_9;
    case 0x206f3fca: return lights_10;
    case 0x206e9888: return building_0;
    case 0x206e98a2: return building_1;
    case 0x206e98bc: return building_2;
    case 0x206e98d6: return building_3;
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
        if (a - 0x206e0000u >= 0x38000u) {
            g3d_native_target = a;
            return native_thunk;
        }
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

/* A word of the image as retail's holds it: the bytes of a pointer to the
 * image itself (relocated here to this one) are retail's address's.  For
 * tables that retail reads past their end into such pointers. */
static uint8_t retail_byte(uint32_t off)
{
    uint32_t i;

    for (i = 0; i < sizeof data_pointers / sizeof data_pointers[0]; i++) {
        uint32_t at = data_pointers[i].at, v;

        if (off < at || off >= at + 4)
            continue;
        v = M32(D + at);
        if (v - D < G3D_IMAGE_SIZE)
            v = v - D + G3D_DATA_VA;
        return (uint8_t)(v >> 8 * (off - at));
    }
    return M8(D + off);
}

static uint16_t retail_word(uint32_t at)
{
    return (uint16_t)(retail_byte(at - D) | retail_byte(at - D + 1) << 8);
}

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
        (void *)frame_earth,        /* +0x2c */
        (void *)queue_draw_all,     /* +0x30 */
        (void *)queue_draw_svc,     /* +0x34 */
        (void *)queue_model_svc,    /* +0x38 */
        (void *)normalise_vector,   /* +0x3c */
        (void *)get_912a,           /* +0x40 */
        (void *)scale_to_screen,    /* +0x44 */
        (void *)approach_lights_turned_svc, /* +0x48 */
        (void *)approach_lights_svc,        /* +0x4c */
        (void *)runway_svc,                 /* +0x50 */
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
