/* A model of HG2D (module id 13) for the harnesses of modules that draw
 * through it, PANELS first.  Not a transliteration: HG2D stays retail in
 * the game.  What a module under test sees of it is modelled -- the slots
 * it calls, with what they return and the registers they leave, and the
 * state HG2D's Linkage exposes as pointers -- and what it hands HG2D is
 * logged into the transcript.  Both sides of a differential run see the
 * same model, so it need only be plausible where it is not exact; where it
 * is exact (the texture pool) tools/difftest/hg2dmodeldiff.c checks it
 * against retail HG2D.
 *
 * The slots PANELS uses (impscan over PANELS, every site read by hand):
 *
 *   +0x48  (rec, x, y, w, u, v) the textured polygon's vertex, 16.16; counts
 *          GLOBAL +0x2f0 (with +0xcb6 clear it is +0x44's plain vertex,
 *          which counts too).  ret 0x18.
 *   +0x4c  (surface) the textured polygon's close; ret 4.
 *   +0x5c  () the palette realised: HG2D's colour tables rebuilt from the
 *          palette at +0x8c (0x20731947, through host +0x290).  The model
 *          rebuilds the remap from it as 0x207323f4 does.
 *   +0x64  () 0x20796004 cleared, +0x60's body (0x20731fd3), 0x207956c0
 *          set: a refresh, logged and counted.
 *   +0xac  (desc, flags) a texture: a record from a pool of 200 (0x34
 *          bytes each, 0x20797f60..0x2079a800) for a descriptor of [0] 1,
 *          [1] 0x100 and flags 0; [0] of the record the descriptor's
 *          pixels (+0x18), or 0x10000 bytes from host +0x20c(4, 0x10000,
 *          &rec[0], 0).  0 for anything else, a full pool or a failed
 *          allocation (which leaves the slot free).  ret 8.
 *   +0xb0  (handle, flags) the texture freed, for a handle inside the pool:
 *          its hardware texture released (record +0xc; never made here),
 *          its pixels freed through host +0x214 unless flags bit 0, the
 *          first four dwords cleared.  ret 8.
 *
 * and the four Linkage words that are pointers into HG2D's .data:
 *
 *   +0x8c  0x207950a0  the palette, 256 RGB triplets
 *   +0x90  0x2079294e  a byte a colour (and their inverse at +0x100): the
 *                      colour words' low bytes, or a brightness ramp
 *                      (sqrt(r*r + g*g + b*b) / 14 + 0x207955c0's byte,
 *                      0x74..0x7f kept); its first byte is tested for 0xa
 *   +0x94  0x207954c0  a word a colour, read by byte at index * 2
 *   +0x98  0x20795cc2  the realised palette, 256 RGB triplets
 *
 * Any other slot traps into hg2d_unexpected(), which logs its number.  The includer
 * defines put(), put_ptr(uint32_t) and STDCALL before including (or
 * HG2D_PUT/HG2D_PUT_PTR instead, to log elsewhere or not at all), and sets
 * hg2d.global, hg2d.host_alloc and hg2d.host_free. */

#ifndef HG2D_PUT
#  define HG2D_PUT put
#endif
#ifndef HG2D_PUT_PTR
#  define HG2D_PUT_PTR put_ptr
#endif


#define HG2D_POOL 200

struct hg2d_texture {
    uint32_t pixels;            /* +0x00 */
    uint32_t w04, w08;
    uint32_t hw;                /* +0x0c: the hardware texture (COM); never set here */
    uint32_t rest[9];
};

static struct {
    uint32_t linkage[0xc0 / 4];
    uint8_t palette[0x300];             /* +0x8c */
    uint8_t remap[0x200];               /* +0x90 */
    uint16_t colour[0x400];             /* +0x94 */
    uint8_t realised[0x300];            /* +0x98 */
    uint8_t ramp_base;                  /* 0x207955c0 */
    int ramp;                           /* 0x20796009: the remap by brightness */
    struct hg2d_texture pool[HG2D_POOL];
    uint8_t *global;
    int32_t (STDCALL *host_alloc)(int32_t, uint32_t, void **, int32_t);
    int32_t (STDCALL *host_free)(void **);
    unsigned realised_count, refresh_count;
} hg2d;

/* The registers HG2D leaves: its compiled-C slots clobber ecx and edx, so
 * the wrappers set them to a constant of the slot, as the other harnesses'
 * stubs do (a stub's leftovers are the harness's, not the module's). */
#define HG2D_WRAP(name, fn, n, bytes, slot)                                  \
    void name(void);                                                         \
    __asm__(".text\n_" #name ":\n\t"                                         \
            ".rept " #n "\n\tpushl " #bytes "(%esp)\n\t.endr\n\t"            \
            "call _" #fn "@" #bytes "\n\t"                                   \
            "movl $0x6dc00000+" #slot ", %ecx\n\t"                           \
            "movl $0x6dd00000+" #slot ", %edx\n\t"                           \
            "ret $" #bytes "\n")

static int32_t STDCALL __attribute__((used)) hg2d_m_vertex(uint32_t rec, uint32_t x, uint32_t y,
                                                           uint32_t w, uint32_t u, uint32_t v)
{
    if (hg2d.global)
        (*(uint32_t *)(hg2d.global + 0x2f0))++;
    HG2D_PUT(" tv(");
    HG2D_PUT_PTR(rec);
    HG2D_PUT(",%#x,%#x,%#x,%#x,%#x)", x, y, w, u, v);
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_m_close(uint32_t surface)
{
    HG2D_PUT(" tpolygon(");
    HG2D_PUT_PTR(surface);
    if (hg2d.global)
        HG2D_PUT(",tex %#x,mode %x", *(uint32_t *)(hg2d.global + 0xc8c),
            *(uint16_t *)(hg2d.global + 0xc90));
    HG2D_PUT(")");
    return 0;
}

static uint32_t hg2d_isqrt(uint32_t v)
{
    uint32_t r = 0;

    while ((r + 1) * (r + 1) <= v)
        r++;
    return r;
}

/* +0x5c: the remap rebuilt from the palette, as 0x207323f4 does it --
 * the colour words' low bytes, or with the ramp on each colour's
 * brightness, 0x74..0x7f keeping theirs -- and the realised palette
 * copied from the palette. */
static int32_t STDCALL __attribute__((used)) hg2d_m_realise(void)
{
    uint32_t i, h = 0;

    for (i = 0; i < 0x100; i++) {
        uint8_t c = (uint8_t)hg2d.colour[i];
        uint8_t m;

        if (!hg2d.ramp || (i >= 0x74 && i < 0x80)) {
            m = c;
        } else {
            uint32_t r = hg2d.palette[3 * i], g = hg2d.palette[3 * i + 1],
                     b = hg2d.palette[3 * i + 2];

            m = (uint8_t)(hg2d_isqrt(r * r + g * g + b * b) / 0xe + hg2d.ramp_base);
        }
        hg2d.remap[i] = m;
        hg2d.remap[0x100 + c] = m;
    }
    memcpy(hg2d.realised, hg2d.palette, sizeof hg2d.realised);
    for (i = 0; i < sizeof hg2d.palette; i++)
        h = h * 31 + hg2d.palette[i];
    hg2d.realised_count++;
    HG2D_PUT(" realise(palette %08x)", h);
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_m_refresh(void)
{
    hg2d.refresh_count++;
    HG2D_PUT(" refresh");
    return 0;
}

static int32_t STDCALL __attribute__((used)) hg2d_m_texture(uint32_t *desc, uint32_t flags)
{
    uint32_t i;
    struct hg2d_texture *t;

    HG2D_PUT(" texture(");
    HG2D_PUT_PTR((uint32_t)(uintptr_t)desc);
    HG2D_PUT(",%u)", flags);
    if (desc[1] != 0x100 || desc[0] != 1 || flags != 0)
        return 0;
    for (i = 0; i < HG2D_POOL && hg2d.pool[i].pixels != 0; i++)
        ;
    if (i == HG2D_POOL)
        return 0;
    t = &hg2d.pool[i];
    t->pixels = desc[6];
    if (t->pixels == 0)
        hg2d.host_alloc(4, 0x10000, (void **)&t->pixels, 0);
    if (t->pixels == 0)
        return 0;
    HG2D_PUT("=tex%u", i);
    return (int32_t)(uintptr_t)t;
}

/* eax as +0xb0 is entered: it returns that for a handle outside the pool;
 * inside, the record's hardware texture word (tested through eax), or the
 * host free's result when it frees */
uint32_t hg2d_entry_eax;            /* read by name from the asm */

static int32_t STDCALL __attribute__((used)) hg2d_m_untexture(struct hg2d_texture *t, uint32_t flags)
{
    int32_t r = (int32_t)hg2d_entry_eax;

    HG2D_PUT(" untexture(");
    if (t >= hg2d.pool && t < hg2d.pool + HG2D_POOL &&
        ((uintptr_t)t - (uintptr_t)hg2d.pool) % sizeof *t == 0)
        HG2D_PUT("tex%u", (unsigned)(t - hg2d.pool));
    else
        HG2D_PUT_PTR((uint32_t)(uintptr_t)t);
    HG2D_PUT(",%u)", flags);
    if ((uintptr_t)t < (uintptr_t)hg2d.pool || (uintptr_t)t >= (uintptr_t)(hg2d.pool + HG2D_POOL))
        return r;
    r = (int32_t)t->hw;                 /* tested through eax */
    if (t->pixels != 0 && !(flags & 1))
        r = hg2d.host_free((void **)&t->pixels);
    t->pixels = t->w04 = t->w08 = t->hw = 0;
    return r;
}

/* every other slot */
uint32_t hg2d_unexpected_slot;      /* written by name from the asm */

static int32_t STDCALL __attribute__((used)) hg2d_unexpected(void)
{
    HG2D_PUT(" HG2D+%#x?", hg2d_unexpected_slot);
    return 0;
}

HG2D_WRAP(hg2d_w_vertex, hg2d_m_vertex, 6, 24, 0x48);
HG2D_WRAP(hg2d_w_close, hg2d_m_close, 1, 4, 0x4c);
HG2D_WRAP(hg2d_w_realise, hg2d_m_realise, 0, 0, 0x5c);
HG2D_WRAP(hg2d_w_refresh, hg2d_m_refresh, 0, 0, 0x64);
HG2D_WRAP(hg2d_w_texture, hg2d_m_texture, 2, 8, 0xac);
void hg2d_w_untexture(void);
__asm__(".text\n_hg2d_w_untexture:\n\t"
        "movl %eax, _hg2d_entry_eax\n\t"
        "pushl 8(%esp)\n\tpushl 8(%esp)\n\t"
        "call _hg2d_m_untexture@8\n\t"
        "movl $0x6dc000b0, %ecx\n\tmovl $0x6dd000b0, %edx\n\t"
        "ret $8\n");
HG2D_WRAP(hg2d_w_unexpected, hg2d_unexpected, 0, 0, 0xfff);

/* A thunk a slot for the rest, each noting its slot: 15 bytes apiece (a
 * movl to memory and a long jmp, spelled so neither shrinks). */
void hg2d_traps(void);
__asm__(".text\n_hg2d_traps:\n\t"
        ".set hg2d_slot, 0\n\t"
        ".rept 0x30\n\t"
        ".byte 0xc7, 0x05\n\t.long _hg2d_unexpected_slot\n\t.long hg2d_slot\n\t"
        ".byte 0xe9\n\t.long _hg2d_w_unexpected - . - 4\n\t"
        ".set hg2d_slot, hg2d_slot + 4\n\t"
        ".endr\n");

/* The model reset for a scenario: an empty pool, and tables of one of
 * three kinds -- an identity colour map with a grey palette, the same
 * with the brightness ramp, or random -- through the includer's rnd(). */
static void hg2d_model_reset(uint32_t (*random)(void))
{
    uint32_t i, kind = random() % 3;

    memset(hg2d.pool, 0, sizeof hg2d.pool);
    for (i = 0; i < sizeof hg2d.linkage / 4; i++)
        hg2d.linkage[i] = (uint32_t)(uintptr_t)hg2d_traps + 15 * i;
    hg2d.linkage[0] = 13;
    hg2d.linkage[0x48 / 4] = (uint32_t)(uintptr_t)hg2d_w_vertex;
    hg2d.linkage[0x4c / 4] = (uint32_t)(uintptr_t)hg2d_w_close;
    hg2d.linkage[0x5c / 4] = (uint32_t)(uintptr_t)hg2d_w_realise;
    hg2d.linkage[0x64 / 4] = (uint32_t)(uintptr_t)hg2d_w_refresh;
    hg2d.linkage[0x8c / 4] = (uint32_t)(uintptr_t)hg2d.palette;
    hg2d.linkage[0x90 / 4] = (uint32_t)(uintptr_t)hg2d.remap;
    hg2d.linkage[0x94 / 4] = (uint32_t)(uintptr_t)hg2d.colour;
    hg2d.linkage[0x98 / 4] = (uint32_t)(uintptr_t)hg2d.realised;
    hg2d.linkage[0xac / 4] = (uint32_t)(uintptr_t)hg2d_w_texture;
    hg2d.linkage[0xb0 / 4] = (uint32_t)(uintptr_t)hg2d_w_untexture;
    hg2d.ramp = kind == 1;
    hg2d.ramp_base = (uint8_t)(kind == 2 ? random() : 0x10);
    for (i = 0; i < 0x100; i++) {
        uint8_t grey = (uint8_t)i;

        hg2d.colour[i] = (uint16_t)(kind == 2 ? random() : i);
        hg2d.palette[3 * i] = kind == 2 ? (uint8_t)random() : grey;
        hg2d.palette[3 * i + 1] = kind == 2 ? (uint8_t)random() : grey;
        hg2d.palette[3 * i + 2] = kind == 2 ? (uint8_t)random() : grey;
    }
    for (i = 0x100; i < 0x400; i++)
        hg2d.colour[i] = (uint16_t)random();
    if (kind != 2 && random() % 4 == 0)
        hg2d.colour[0] = 0xa;               /* what PANELS tests the remap's first byte for */
    hg2d.realised_count = hg2d.refresh_count = 0;
    hg2d_m_realise();
}

/* The model's state into the transcript, for comparing. */
static void hg2d_model_put_state(void)
{
    uint32_t i, h = 0;

    for (i = 0; i < sizeof hg2d.palette; i++)
        h = h * 31 + hg2d.palette[i];
    for (i = 0; i < sizeof hg2d.remap; i++)
        h = h * 31 + hg2d.remap[i];
    HG2D_PUT(" HG2D(palette+remap %08x, realised %u, refreshed %u, textures", h,
        hg2d.realised_count, hg2d.refresh_count);
    for (i = 0; i < HG2D_POOL; i++)
        if (hg2d.pool[i].pixels) {
            HG2D_PUT(" %u:", i);
            HG2D_PUT_PTR(hg2d.pool[i].pixels);
        }
    HG2D_PUT(")");
}
