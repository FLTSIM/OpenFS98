/* Normal MDL header +0x10 callbacks, transliterated from the reference CD.
 * The DLL Linkage scaffolding and crash models are outside this slice.
 * WINDOW 0x20906a20..0x20906a49 calls these with one state pointer, then
 * replaces eax before calling ACONTAIN. Caller-saved return registers are
 * unused there. Preserve the arithmetic widths and writes into model data.
 */
#include "mdl_normal.h"

/* Explicit little-endian accesses also work for unaligned program operands. */
static uint16_t read16(const uint8_t *p)
{ return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t read32(const uint8_t *p)
{ return read16(p) | (uint32_t)read16(p + 2) << 16; }
static void write16(uint8_t *p, uint16_t value)
{ p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }
static void write32(uint8_t *p, uint32_t value)
{ write16(p, (uint16_t)value); write16(p + 2, (uint16_t)(value >> 16)); }

#define G16(off) read16(global + (off))
#define G32(off) read32(global + (off))
#define S16(off, value) write16(state + (off), (uint16_t)(value))
#define S32(off, value) write32(state + (off), (uint32_t)(value))
#define I16(off, value) write16(image + (off), (uint16_t)(value))

/* C182 helper RVA 0x11c1; RG helper 0x11d3. All operations are 16-bit. */
static void cessna_split(uint16_t value, uint16_t *bx, uint16_t *cx)
{
    uint16_t ax = (uint16_t)(0x7fff - (uint16_t)(value + value));
    *bx = 0;
    *cx = ax;
    if (ax > 0x4000) {
        *cx = 0x4000;
        *bx = (uint16_t)(ax - 0x4000);
    }
}

/* Boeing RVA 0x119e; Lear RVA 0x110e. MUL DX is a 16-bit multiply. */
static void jet_split(uint32_t value, uint16_t *bx, uint16_t *dx)
{
    uint16_t ax = (uint16_t)((0x3fffu - value) << 2);
    *dx = (uint16_t)(((uint32_t)ax * 0x6000u) >> 16);
    *bx = 0;
    if (*dx >= 0x4000) {
        *bx = (uint16_t)((*dx - 0x4000) * 2);
        *dx = 0x4000;
    }
    *dx = (uint16_t)(0u - *dx);
}

/* SAR AX,2, with sign extension expressed without a signed right shift. */
static uint16_t quarter(uint16_t value)
{ return (uint16_t)((value >> 2) | ((value & 0x8000) ? 0xc000 : 0)); }

void fs98_mdl_normal_update(enum fs98_mdl_kind kind, uint8_t *state,
                           const uint8_t *global, uint8_t *image,
                           fs98_host_memmove_fn copy)
{
    uint16_t ax, bx, cx, dx, lights, altitude;
    uint32_t eax, index;

    /* Sopwith RVA 0x1054: a literal string embedded in retail's .text. */
    if (kind == FS98_MDL_SOPWITH)
        S32(0x74, (uintptr_t)(image + 0x1037));
    copy(state + 4, global + 0x560, 0x24);
    copy(state + 0x28, global + 0x5b0, 0x18);
    S32(0x68, G32(0xd48));
    S16(0x60, G16(0x11b6));
    ax = G16(0x120a);
    if (kind != FS98_MDL_BELL)
        S16(0x6c, ax);
    if (kind == FS98_MDL_BOEING) {
        S16(0x70, ax);
        /* MOV AX leaves the upper half from GLOBAL +0xd48 intact. SHR EAX
         * brings its bit 16 into bit 15 of this word. */
        eax = (G32(0xd48) & 0xffff0000u) | ax;
        S16(0x6e, eax | (eax >> 1));
    }
    altitude = (uint16_t)((0u - G32(0x578)) >> 16);
    switch (kind) {
    case FS98_MDL_BOEING: I16(0xea12, altitude); I16(0xe682, altitude); break;
    case FS98_MDL_C182RG:
        I16(0x647e, altitude); I16(0x6a08, altitude); I16(0x6d68, altitude); break;
    case FS98_MDL_EXTRA: I16(0xac94, altitude); I16(0xa5a2, altitude); break;
    case FS98_MDL_SCHWEIZER: I16(0x3ec6, altitude); return;
    case FS98_MDL_SOPWITH:
        I16(0x5daa, altitude); I16(0x5dec, altitude);
        S16(0x72, G16(0x11b8));
        break;
    default: break;
    }
    if (kind == FS98_MDL_SOPWITH || kind == FS98_MDL_EXTRA) {
        if (kind == FS98_MDL_EXTRA)
            S16(0x74, G16(0x11b8));
        ax = G16(0x898);
        S16(0x6e, ax);
        S16(0x70, read16(state + 0x70) - (uint16_t)(ax + ax));
        if (kind == FS98_MDL_SOPWITH)
            return;
    }

    lights = (uint16_t)(((global[0x28c] << 1) & 2) | (global[0x280] & 1));
    if (global[0xb6a]) lights = 0;
    ax = (uint16_t)(global[0x282] | ((global[0x282] & 0x80) ? 0xff00 : 0));
    switch (kind) {
    case FS98_MDL_EXTRA:
        S16(0x76, lights); S16(0x78, ax); return;
    case FS98_MDL_BELL:
        S16(0x76, G16(0x11b8)); S16(0x72, lights); S16(0x74, ax);
        S16(0x6c, G16(0x898));
        S16(0x6e, read16(state + 0x6e) - 0x2cf3);
        S16(0x70, read16(state + 0x70) - 0xa91f);
        S32(0x78, G32(0x290) + G32(0x578) - 0x08000000u);
        S32(0x7c, G32(0x57c)); S32(0x80, G32(0x298) + G32(0x580)); return;
    case FS98_MDL_BOEING:
        S16(0x8a, lights); S16(0x8c, ax);
        for (index = 0; index < 4; index++) {
            S32(0x90 + 12 * index, G32(0x578));
            S32(0x94 + 12 * index, G32(0x57c));
        }
        eax = G32(0x580);
        S32(0x98, eax - 0x038e0000u); S32(0xa4, eax + 0x038e0000u);
        S32(0xb0, eax - 0x38e00000u); S32(0xbc, eax + 0x38e00000u);
        jet_split(G32(0xbec), &bx, &dx);
        S16(0x80, dx); S16(0x82, bx); S16(0x84, 0u - bx);
        S16(0x86, 0x3fffu - G32(0xbf0)); S16(0x88, G32(0xbf4) - 0x3fffu); return;
    case FS98_MDL_LEAR:
        S16(0x88, lights); S16(0x8a, ax);
        jet_split(G32(0xbec), &bx, &dx);
        S16(0x7e, dx); S16(0x80, bx); S16(0x82, 0u - bx);
        S16(0x84, 0x3fffu - G32(0xbf0)); S16(0x86, G32(0xbf4) - 0x3fffu);
        eax = 0xffff8e38u * G32(0xbe4);
        S16(0x74, (0u - eax) >> 16);
        S16(0x76, quarter(G16(0x11c2)));
        S16(0x78, 0u - quarter(G16(0x11c4)));
        break;
    case FS98_MDL_C182RG:
    case FS98_MDL_C182:
        S16(0x8a, G16(0x11b8)); S16(0x86, lights); S16(0x88, ax);
        ax = G16(0x898);
        S16(0x78, ax); S16(0x7a, read16(state + 0x7a) - (uint16_t)(ax + ax));
        S16(0x7c, G32(0xbec) - 0x3fffu);
        cessna_split((uint16_t)G32(0xbf0), &bx, &cx);
        S16(0x7e, bx); S16(0x80, cx);
        cessna_split((uint16_t)G32(0xbf4), &bx, &cx);
        S16(0x82, bx); S16(0x84, 0u - cx);
        S16(0x74, quarter(G16(0x11c2)));
        S16(0x76, 0u - quarter(G16(0x11c4)));
        break;
    default: return;
    }
    /* Low 32 bits of the signed product are identical to unsigned multiply. */
    S16(0x72, (0xffff8e38u * G32(0xbe0)) >> 16);
    ax = quarter(G16(0x11c0));
    S16(0x70, ax); S16(0x6e, 0u - ax);
}
