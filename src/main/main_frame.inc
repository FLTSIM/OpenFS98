/* MAIN.DLL slice 22: init and deinit (+0x04, +0x08) and the three CHAIN
 * callbacks init registers.
 *
 * Retail, assembly unless noted:
 *   .text+0x0020 (+0x04, MSVC C) and .text+0x6cf0, the init: the Linkage to
 *     the host (+0x18), MAIN onto the host's module stack (+0x298, its
 *     HMODULE into .data+0x7fc0), the cloud callbacks onto CHAIN lists 1 and
 *     3, the cloud textures (+0x94, slice 20), a random seed into GLOBAL
 *     +0x1162, the situation-record listener onto list 0x14, and the user's
 *     LCID into GLOBAL +0x1288;
 *   .text+0x0070 (+0x08, MSVC C) and .text+0x6d90, the deinit, in reverse;
 *   .text+0x2e10, list 0x14's listener: `ret 8`, nothing else;
 *   .text+0x7095, list 1's: the cloud layers drift with the wind
 *     (+0x8ea9, +0x8d7b, +0x8da5), the puffs move with it (+0x7f73, with
 *     +0x7c40 scattering them afresh when their parameters change, +0x7ec7
 *     and +0x7ee7 placing one), the haze block to G3D +0x1c, slice 12's sun,
 *     and HG2D +0x7c's colour table;
 *   .text+0x85ea, list 3's: the cloud grid follows the aircraft from cell to
 *     cell of FE's level-8 grid (+0x8609, +0x87f0, +0x8838, +0x8850,
 *     +0x8868, +0x8887), and the puffs are re-centred on it (+0x80cd).
 *
 * Retail details, reproduced:
 *   - +0x8da5 picks the wind band whose altitude range holds the layer
 *     (GLOBAL +0xeca/+0xecc, +0xed6/+0xed8, +0xee2/+0xee4, unsigned) and turns
 *     its direction and speed into a step through UTIL +0x2c; the speed's
 *     scale by GLOBAL +0xc1a keeps bits 8..23 of the product.
 *   - +0x80cd builds a 48-bit value from .data+0x1e6e >> 2 with `shld` and
 *     `shr`, leaving its high word in both of the upper words; and divides by
 *     it and by UTIL +0x44's answer into overflow, leaning on the host's
 *     divide repair.
 *   - +0x8609 divides 0x1000000 by UTIL +0x48's answer with a memory operand
 *     (the host's filter repairs it as it does a register's).
 *   - +0x8868 steps a value by a whole cell at a time while its distance to
 *     the target falls, by a signed compare, so a distance of 0x80000000 (its
 *     own negation) counts as falling.
 *   - The callbacks' eax, ecx and edx at return are what their last callee
 *     left; CHAIN ignores the first two and hands edx to the next callback on
 *     the list.  Not reproduced: no retail callback on these lists reads it.
 *
 * The stack-alignment shims are left out, as in slices 15 and 19. */

#define FE      M32(V(0x2080005c))
#define M64ADD(a, v)                                                           \
    do {                                                                       \
        uint64_t t_ = ((uint64_t)M32((a) + 4) << 32 | M32(a)) + (v);           \
        M32(a) = (uint32_t)t_;                                                 \
        M32((a) + 4) = (uint32_t)(t_ >> 32);                                   \
    } while (0)
#define H_PUSH_MODULE  HOST_(0x298, f2)   /* (linkage, &hmodule) */
#define H_POP_MODULE   HOST_(0x29c, f1)

/* .text+0x2e10: list 0x14's listener does nothing, leaving every register as
 * CHAIN entered it with. */
uint32_t FS98_STDCALL main_record_listener(uint32_t a, uint32_t b);
__asm__(".text\n\t.p2align 4\n\t.globl _main_record_listener@8\n_main_record_listener@8:\n\tret $8\n");
#define RECORD_LISTENER ((uint32_t)(uintptr_t)main_record_listener)

/* ---- the wind ------------------------------------------------------------- */

/* .text+0x8da5: the wind step at altitude `alt`: *x (ebx) and *z (eax). */
static void wind_step(uint16_t alt, uint32_t *x, uint32_t *z)
{
    uint32_t gp = G, p;
    uint16_t bx = M16(gp + 0xed0), bp = M16(gp + 0xece), ax, cx, dx;

    if (alt > M16(gp + 0xeca) || alt < M16(gp + 0xecc)) {
        bx = M16(gp + 0xedc);
        bp = M16(gp + 0xeda);
        if (alt > M16(gp + 0xed6) || alt < M16(gp + 0xed8)) {
            bx = M16(gp + 0xee8);
            bp = M16(gp + 0xee6);
            if (alt > M16(gp + 0xee2) || alt < M16(gp + 0xee4)) {
                bp = 0;
                bx = 0;
            }
        }
    }
    p = (uint32_t)bp * M16(gp + 0xc1a);
    bp = (uint16_t)(p >> 8);
    CALL(UTIL, 0x2c, f3)(bx, V(0x2080201d), V(0x2080201f));
    ax = (uint16_t)(0u - M16(V(0x2080201d)));
    cx = (uint16_t)(0u - M16(V(0x2080201f)));

    p = (uint32_t)((int32_t)(int16_t)ax * (int16_t)bp);
    dx = (uint16_t)((p >> 15) & 0xffff);           /* add ax,ax / adc dx,dx: dx */
    p = (uint32_t)((int32_t)(int16_t)dx * (int16_t)M16(gp + 0x2aa));
    *x = p;

    p = (uint32_t)((int32_t)(int16_t)cx * (int16_t)bp);
    dx = (uint16_t)((p >> 15) & 0xffff);
    p = (uint32_t)((int32_t)(int16_t)dx * (int16_t)M16(gp + 0x2aa));
    *z = p;
}

/* .text+0x8d7b: one layer's drift over `dt` (0x258 or 0x320): .data+0x1eba
 * and +0x1eb6 move by the wind's step times dt, a 16.16 product. */
static void layer_drift(uint16_t alt, uint16_t dt)
{
    uint32_t x, z;

    wind_step(alt, &x, &z);
    M32(V(0x20801eba)) = M32(V(0x20801eba)) +
                         (uint32_t)(((int64_t)(int32_t)z * (int32_t)(uint32_t)dt) >> 16);
    M32(V(0x20801eb6)) = M32(V(0x20801eb6)) +
                         (uint32_t)(((int64_t)(int32_t)x * (int32_t)(uint32_t)dt) >> 16);
}

/* .text+0x8ea9: drift both layers' origins (.data+0x1ebe/+0x1ec2 and
 * +0x1ec6/+0x1eca) with the wind at their altitudes. */
static void layers_drift(void)
{
    uint32_t gp = G;

    if (M16(gp + 0xe70) != 0) {
        M32(V(0x20801eb6)) = M32(V(0x20801ebe));
        M32(V(0x20801eba)) = M32(V(0x20801ec2));
        layer_drift(M16(gp + 0xe6a), 0x258);
        M32(V(0x20801ebe)) = M32(V(0x20801eb6));
        M32(V(0x20801ec2)) = M32(V(0x20801eba));
    }
    if (M16(gp + 0xe78) != 0) {
        M32(V(0x20801eb6)) = M32(V(0x20801ec6));
        M32(V(0x20801eba)) = M32(V(0x20801eca));
        layer_drift(M16(gp + 0xe72), 0x320);
        M32(V(0x20801ec6)) = M32(V(0x20801eb6));
        M32(V(0x20801eca)) = M32(V(0x20801eba));
    }
}

/* ---- the puffs -------------------------------------------------------------- */

/* .text+0x7ec7: puff `p`'s latitude from its offset `ax`, a 16.16 product
 * with the cell size .data+0x1e6e, about the field's centre (.data+0x1b18);
 * into both copies of the puff block. */
static void puff_lat(uint32_t p, uint16_t ax)
{
    uint32_t v = (uint32_t)(((int64_t)(int16_t)ax * (int32_t)M32(V(0x20801e6e))) >> 16) +
                 M32(V(0x20801b18));

    M32(V(0x20801b30) + p) = v;
    M32(V(0x20801cb0) + p) = v;
}

/* .text+0x7ee7: and its longitude: the same product as 48-bit metres, turned
 * into longitude at the field's centre by UTIL +0x44. */
static void puff_lon(uint32_t p, uint16_t ax)
{
    uint64_t q = (uint64_t)((int64_t)(int16_t)ax * (int32_t)M32(V(0x20801e6e)));
    uint32_t v;

    M16(V(0x20802043)) = (uint16_t)q;
    M16(V(0x20802045)) = (uint16_t)((uint32_t)q >> 16);
    M16(V(0x20802047)) = (uint16_t)(q >> 32);
    CALL(UTIL, 0x44, f3)(V(0x20802041), V(0x20801b14), V(0x20802039));
    v = ((uint32_t)M16(V(0x2080203f)) << 16 | M16(V(0x2080203d))) + M32(V(0x20801b20));
    M32(V(0x20801b38) + p) = v;
    M32(V(0x20801cb8) + p) = v;
}

/* .text+0x7c40: scatter the sixteen puffs afresh when the puff parameters
 * (GLOBAL +0xe7a + +0xe80) have changed since the last (.data+0x1e48),
 * about the cell GLOBAL +0xb3a..+0xb46 names; while .data+0x1e46 counts
 * down the puffs keep their offsets, otherwise each draws two from UTIL's
 * generator. */
static void puffs_scatter(void)
{
    uint32_t gp = G, p, o, n, t;
    uint16_t ax, cx;

    if (M16(gp + 0xe7e) == 0 || M16(gp + 0xe7a) == 0 || M16(V(0x20801e72)) == 0)
        return;
    ax = (uint16_t)(M16(gp + 0xe7a) + M16(gp + 0xe80));
    if (ax != M16(V(0x20801e48))) {
        M16(V(0x20801e48)) = ax;
        M16(V(0x20801e44)) = 1;
    }
    if (M16(V(0x20801e44)) == 0)
        return;
    M16(V(0x20801e44)) = 0;
    M16(V(0x20801afe)) = M16(gp + 0xb3a);
    M16(V(0x20801b00)) = M16(gp + 0xb3c);
    M16(V(0x20801b02)) = M16(gp + 0xb3e);
    if (M16(V(0x20801e46)) == 0) {
        M16(V(0x20801b16)) = M16(gp + 0xb3a);
        M16(V(0x20801b18)) = M16(gp + 0xb3c);
        M16(V(0x20801b1a)) = M16(gp + 0xb3e);
    }
    M16(V(0x20801b06)) = M16(gp + 0xb42);
    M16(V(0x20801b08)) = M16(gp + 0xb44);
    M16(V(0x20801b0a)) = M16(gp + 0xb46);
    if (M16(V(0x20801e46)) == 0) {
        M16(V(0x20801b1e)) = M16(gp + 0xb42);
        M16(V(0x20801b20)) = M16(gp + 0xb44);
        M16(V(0x20801b22)) = M16(gp + 0xb46);
    }
    /* 2^32 / the grid's span: past 2^32 - 1 the host's repair gives all ones */
    t = M32(V(0x20802eb6));
    M32(V(0x20801e6e)) = t <= 1 ? 0xffffffffu : (uint32_t)(0x100000000ull / t);

    ax = (uint16_t)(0u - (uint16_t)(M16(gp + 0xe80) >> 2));
    M16(V(0x20803098)) = ax;
    M16(V(0x2080309e)) = ax;
    cx = (uint16_t)(((uint32_t)M16(gp + 0xe80) * 0xcccdu) >> 16);
    ax = (uint16_t)(cx >> 2);
    M16(V(0x2080309a)) = ax;
    M16(V(0x208030a0)) = ax;
    ax = (uint16_t)(0u - ax);
    M16(V(0x20803094)) = ax;
    M16(V(0x208030a6)) = ax;
    M32(V(0x20801e52)) = (0xc000u * cx) >> 8;
    M32(V(0x20801e4e)) = (0xd99au * cx) >> 8;
    M32(V(0x20801e4a)) = (0xffffu * cx) >> 8;

    for (n = 0, p = 0, o = 0; n < 16; n++, p += 0x18, o += 4) {
        if (M16(V(0x20801e46)) != 0) {
            puff_lat(p, M16(V(0x20801e74) + o));
            puff_lon(p, M16(V(0x20801e76) + o));
        } else {
            M16(V(0x20801e74) + o) = (uint16_t)CALL(UTIL, 0x6c, f0)();
            puff_lat(p, M16(V(0x20801e74) + o));
            M16(V(0x20801e76) + o) = (uint16_t)CALL(UTIL, 0x6c, f0)();
            puff_lon(p, M16(V(0x20801e76) + o));
        }
        gp = G;
        M16(V(0x20801b40) + p) = M16(gp + 0xe7a);
        M16(V(0x20801cbe) + p) = M16(gp + 0x572);
        M16(V(0x20801cc0) + p) = M16(gp + 0x574);
        M16(V(0x20801cc2) + p) = M16(gp + 0x576);
    }
}

/* .text+0x7f73: move the puffs with the wind at the puffs' altitude. */
static void puffs_move(void)
{
    uint32_t gp = G, x, z, p, n;
    uint64_t a;

    if (M16(gp + 0xe7e) == 0 || M16(gp + 0xe7a) == 0)
        return;
    puffs_scatter();
    gp = G;
    M16(V(0x20801e60)) = M16(gp + 0x2ea);
    M16(V(0x20801e58)) = 0xc000;
    wind_step(M16(gp + 0xe7a), &x, &z);
    M32(V(0x20801ece)) = x;
    M32(V(0x20801ed6)) = z;

    x = M32(V(0x20801ece));
    M16(V(0x20802043)) = (uint16_t)x;
    M16(V(0x20802045)) = (uint16_t)(x >> 16);
    M16(V(0x20802047)) = (uint16_t)((int32_t)x >> 31);
    CALL(UTIL, 0x44, f3)(V(0x20802041), V(0x20801b14), V(0x20802039));
    M16(V(0x20801ece)) = M16(V(0x2080203b));
    M16(V(0x20801ed0)) = M16(V(0x2080203d));
    M32(V(0x20801ed2)) = (uint32_t)(int32_t)(int16_t)M16(V(0x2080203f));

    for (n = 0, p = 0; n < 16; n++, p += 0x18) {
        gp = G;
        M16(V(0x20801cbe) + p) = M16(gp + 0x572);
        M16(V(0x20801cc0) + p) = M16(gp + 0x574);
        M16(V(0x20801cc2) + p) = M16(gp + 0x576);
        a = (uint64_t)M32(V(0x20801ed2)) << 32 | M32(V(0x20801ece));
        M64ADD(V(0x20801b36) + p, a);
        M64ADD(V(0x20801cb6) + p, a);
        a = (uint64_t)(int64_t)(int32_t)M32(V(0x20801ed6));
        M64ADD(V(0x20801b2e) + p, a);
        M64ADD(V(0x20801cae) + p, a);
    }
}

/* .text+0x7095: list 1's callback. */
static uint32_t FS98_STDCALL frame_clouds(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    layers_drift();
    puffs_move();
    if (M32(G + 0x390) < 2) {
        M32(V(0x20801a72)) = V(0x20802ea4);
        CALL(G3D, 0x1c, f1)(V(0x20802ea4));
    }
    M16(G + 0xe44) = 0;
    nav_sun();
    return CALL(HG2D, 0x7c, f1)(M16(V(0x20801a7e)));
}

/* ---- the grid ------------------------------------------------------------- */

/* .text+0x8887: |v - target| against the last (.data+0x1ac4), by a signed
 * compare; answers whether it fell, and keeps it. */
static int grid_closer(uint32_t v, uint32_t target)
{
    uint32_t d = v - target;
    int fell;

    if ((int32_t)d < 0)
        d = 0u - d;
    fell = (int32_t)d < (int32_t)M32(V(0x20801ac4));
    M32(V(0x20801ac4)) = d;
    return fell;
}

/* .text+0x8868: step `v` by whole `cell`s toward `target`, up while that
 * helps, then down while that helps, and back the one step too far. */
static uint32_t grid_step(uint32_t v, uint32_t target, uint32_t cell)
{
    M32(V(0x20801ac4)) = 0x7fffffffu;
    do
        v += cell;
    while (grid_closer(v, target));
    do
        v -= cell;
    while (grid_closer(v, target));
    return v + cell;
}

/* .text+0x87f0: the cell for a layer of extent `span` (0x258 or 0x320):
 * 0x1000000 / span metres (.data+0x1ab8), and as longitude at the aircraft
 * (UTIL +0x44, .data+0x1ac0). */
static void grid_cell(uint32_t span)
{
    M32(V(0x20801ab8)) = 0x1000000u / span;
    CALL(UTIL, 0x44, f3)(V(0x20801ab4), G + 0x560, V(0x20801abc));
}

/* .text+0x8609: follow the aircraft to its level-8 cell of FE's grid
 * (GLOBAL +0xb38 latitude, +0xb40 longitude), its size per unit in GLOBAL
 * +0xb50, and when the cell changes move both layers' origins (.data+0x1a84,
 * +0x1a8c; +0x1a9c, +0x1aa4) onto it, a cell at a time. */
static void grid_follow(void)
{
    uint32_t gp = G, q, d;
    uint16_t bx, ax, dx;

    bx = (uint16_t)CALL(FE, 0x60, f2)(gp + 0x564, 8);
    CALL(FE, 0x64, f3)(bx, 8, G + 0xb38);
    bx = (uint16_t)CALL(FE, 0x68, f2)(G + 0x56c, 8);
    CALL(FE, 0x6c, f3)(bx, 8, G + 0xb40);
    M32(V(0x2080203d)) = 0x1000000;
    CALL(UTIL, 0x48, f3)(V(0x20802039), G + 0xb38, V(0x20802041));
    d = M32(V(0x20802045));
    q = idiv32(0x1000000, (int32_t)d);             /* by zero, the repair's largest */
    gp = G;
    M32(gp + 0xb50) = q;
    M32(gp + 0xb4b) = M32(gp + 0x20);
    M16(V(0x20801e72)) = 1;
    ax = M16(gp + 0x116c);
    M16(gp + 0x116c) = (uint16_t)(ax >> 1);
    if (ax & 1) {
        M32(V(0x20801a84)) = M32(gp + 0xb3c);
        M32(V(0x20801a9c)) = M32(gp + 0xb3c);
        M32(V(0x20801a8c)) = M32(gp + 0xb44);
        M32(V(0x20801aa4)) = M32(gp + 0xb44);
    }
    dx = M16(gp + 0xb46);
    ax = M16(V(0x20801ab2));
    M16(V(0x20801ab2)) = dx;
    if (ax == dx) {
        dx = M16(gp + 0xb3d);
        ax = M16(V(0x20801ab0));
        M16(V(0x20801ab0)) = dx;
        if (ax == dx)
            return;
    }
    grid_cell(0x258);
    M32(V(0x20801a8c)) = grid_step(M32(V(0x20801a8c)), M32(G + 0xb44), M32(V(0x20801ac0)));
    M32(V(0x20801a84)) = grid_step(M32(V(0x20801a84)), M32(G + 0xb3c), M32(V(0x20801ab8)));
    grid_cell(0x320);
    M32(V(0x20801aa4)) = grid_step(M32(V(0x20801aa4)), M32(G + 0xb44), M32(V(0x20801ac0)));
    M32(V(0x20801a9c)) = grid_step(M32(V(0x20801a9c)), M32(G + 0xb3c), M32(V(0x20801ab8)));
}

/* .text+0x80cd: re-centre the puffs the drawing has released (.data+0x3380's
 * flags, cleared after) on the aircraft's cell; while .data+0x1e46 counts down
 * the centre stays put. */
static void puffs_recentre(void)
{
    uint32_t gp = G, n, p, o, e, x;
    uint32_t q;

    if (M16(gp + 0xe7e) == 0 || M16(gp + 0xe7a) == 0)
        return;
    M16(V(0x20802080)) = 0;
    for (n = 0, p = 0, o = 0; n < 16; n++, p += 0x18, o += 4) {
        if (M16(V(0x20803380) + n * 2) != 0)
            continue;
        if (M16(V(0x20802080)) == 0) {
            M16(V(0x20802080)) = 1;
            e = M32(V(0x20801e6e)) >> 2;
            q = idiv32((int64_t)(int32_t)(M32(G + 0x564) - M32(V(0x20801b00))), (int32_t)e);
            if ((uint16_t)q != M16(V(0x20802082))) {
                M16(V(0x20802082)) = (uint16_t)q;
                if (M16(V(0x20801e46)) == 0)
                    M32(V(0x20801b18)) = q * e + M32(V(0x20801b00));
            }
            /* retail's shld/shr leave e's high word in both upper words */
            M16(V(0x20802043)) = 0;
            M16(V(0x20802045)) = (uint16_t)(e >> 16);
            M16(V(0x20802047)) = (uint16_t)(e >> 16);
            CALL(UTIL, 0x44, f3)(V(0x20802041), G + 0x560, V(0x20802039));
            x = (uint32_t)M16(V(0x2080203f)) << 16 | M16(V(0x2080203d));
            q = idiv32((int64_t)(int32_t)(M32(G + 0x56c) - M32(V(0x20801b08))), (int32_t)x);
            if ((uint16_t)q != M16(V(0x20802084))) {
                M16(V(0x20802084)) = (uint16_t)q;
                if (M16(V(0x20801e46)) == 0)
                    M32(V(0x20801b20)) = q * x + M32(V(0x20801b08));
            }
        }
        puff_lat(p, M16(V(0x20801e74) + o));
        puff_lon(p, M16(V(0x20801e76) + o));
    }
    for (n = 0; n < 16; n++)
        M16(V(0x20803380) + n * 2) = 0;
    if (M16(V(0x20801e46)) != 0)
        M16(V(0x20801e46)) = (uint16_t)(M16(V(0x20801e46)) - 1);
}

/* .text+0x85ea: list 3's callback. */
static uint32_t FS98_STDCALL frame_grid(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    grid_follow();
    puffs_recentre();
    return 0;
}

/* ---- init and deinit -------------------------------------------------------- */

/* .text+0x0020 == Linkage +0x04. */
static uint32_t FS98_STDCALL main_init(void)
{
    HOST_(0x18, f1)(V(0x208000c0));
    H_PUSH_MODULE(V(0x208000c0), V(0x20807fc0));
    /* .text+0x6cf0 */
    CHAIN_ADD(1, ADDR(frame_clouds));
    CHAIN_ADD(3, ADDR(frame_grid));
    clouds_textures();
    M16(G + 0x1162) = (uint16_t)CALL(UTIL, 0x6c, f0)();
    M16(V(0x20801e44)) = 1;
    CHAIN_ADD(0x14, RECORD_LISTENER);
    M32(G + 0x1288) = GetUserDefaultLCID();
    return M32(G + 0x1288);
}

/* .text+0x0070 == Linkage +0x08. */
static uint32_t FS98_STDCALL main_deinit(void)
{
    H_POP_MODULE(M32(V(0x20807fc0)));
    CHAIN_REMOVE(0x14, RECORD_LISTENER);
    HOST_(0x1c, f0)();
    /* .text+0x6d90 */
    CHAIN_REMOVE(1, ADDR(frame_clouds));
    return CHAIN_REMOVE(3, ADDR(frame_grid));
}
