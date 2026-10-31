/* MAIN.DLL slice 28: the routines MAIN's own G3D programs call.
 *
 * Retail's command blocks (.data+0x2386..+0x34fa) hold G3D opcode 0x8c, which
 * calls an absolute code address out of the program with G3D's registers.
 * Fifteen routines are named that way, 53 times, and G3D keeps what they leave
 * in every register, so they are entered and left with all seven in main_e*:
 *
 *   +0x73d7, +0x73fb   a cloud layer's faces, if +0x741f said it shows
 *   +0x7bc2            the puffs' fade flags at .data+0x1eb4
 *   +0x889c, +0x88e2   the ground's nine tiles, for the first and second layer
 *   +0x8d47, +0x8d5e   two colours into GLOBAL +0xc94 and +0xc9c
 *   +0x8f34..+0x8fb8   GLOBAL +0xe88 and +0xd4c set aside and put back
 *   +0xa04a            a bare ret
 *
 * The hand-rolled stack alignment around each call is left out, as in slice 15.
 *
 * Retail details, reproduced:
 *   - +0x88e2 loads di from .data+0x1aae before reading GLOBAL +0xe76 through
 *     edi, so the read lands at G's upper half, that di, plus 0xe76.
 *   - +0x8d47 writes through the edi it was called with; +0x8d5e loads G first.
 *   - +0x9230 goes on using ecx and edx across G3D +0x40, which leaves them
 *     alone (retail's is `mov eax, ...; ret` between pushes and pops).
 *   - +0x9818 picks its row from .data+0x3474, which +0x9d57 has just set. */

#define native_ebp main_ebp
#define BP  ((uint16_t)native_ebp)
#define SI  ((uint16_t)esi)
#define DI  ((uint16_t)edi)
#define SET_BP(v) SET16(native_ebp, v)
#define SET_SI(v) SET16(esi, v)
#define SET_DI(v) SET16(edi, v)
#define SET_BH(v) SETHI(ebx, v)

/* Entered from G3D's `call eax`: every register in, every register out. */
#define NATIVE(name)                                                         \
    static void name##_body(void) __attribute__((used, noinline));           \
    void name(void);                                                         \
    __asm__(".text\n\t.p2align 4\n"                                          \
            "_" #name ":\n\t"                                                \
            "movl %eax, _main_eax\n\tmovl %ecx, _main_ecx\n\t"               \
            "movl %edx, _main_edx\n\tmovl %ebx, _main_ebx\n\t"               \
            "movl %esi, _main_esi\n\tmovl %edi, _main_edi\n\t"               \
            "movl %ebp, _main_ebp\n\t"                                       \
            "call _" #name "_body\n\t"                                       \
            "movl _main_eax, %eax\n\tmovl _main_ecx, %ecx\n\t"               \
            "movl _main_edx, %edx\n\tmovl _main_ebx, %ebx\n\t"               \
            "movl _main_esi, %esi\n\tmovl _main_edi, %edi\n\t"               \
            "movl _main_ebp, %ebp\n\tret\n");                                \
    static void name##_body(void)

/* The vertex fields a corner of +0x9230's quad is copied to. */
static void face_put(uint32_t off)
{
    M32(esi + off) = eax;
    M32(esi + off + 4) = ecx;
    M32(esi + off + 8) = edx;
}

static void face_add(uint32_t off)
{
    M32(esi + off) += eax;
    M32(esi + off + 4) += ecx;
    M32(esi + off + 8) += edx;
}

/* eax, ecx and edx from the three words at the offset vector ebp names. */
static void face_vector(void)
{
    eax = M32(native_ebp);
    ecx = M32(native_ebp + 4);
    edx = M32(native_ebp + 8);
}

/* A neighbour's texel: the corner pair to move, when it is higher, or the
 * quad's edge flag to clear, when it is not lower.  Nothing at all below the
 * threshold. */
static void face_neighbour(uint8_t dl, uint8_t dh, uint32_t off, uint32_t flag)
{
    uint32_t saved = ebx;

    SET_BL((uint8_t)(BL + dl));
    SET_BH((uint8_t)((ebx >> 8) + dh));
    ebx = (uint16_t)ebx;
    ebx += M32(V(0x20803490));
    SET_AL(M8(ebx));
    if (AL >= M8(V(0x208033b8))) {
        SET_AL((uint8_t)(AL - M8(V(0x208033b8))));
        SET_AH(0);
        if (AX >= 3)
            SET_AX(3);
        if (AX >= DI) {
            M16(flag) = 0;
        } else {
            native_ebp = M32(V(0x20803402) + (uint32_t)AX * 4);
            face_vector();
            M32(esi + off) += eax;
            M32(esi + off + 4) += ecx;
            M32(esi + off + 8) += edx;
            M32(esi + off + 0x10) += eax;
            M32(esi + off + 0x14) += ecx;
            M32(esi + off + 0x18) += edx;
        }
    }
    ebx = saved;
}

/* .text+0x9230: the column at (bh, bl), when its texel reaches the threshold:
 * a quad from G3D +0x40, moved by the texel's height and by its neighbours',
 * to G3D +0x1c.  It leaves esi on the quad and di on the height. */
static void face_column(void)
{
    uint32_t s_ebp, s_eax, s_ecx, s_edx, s_ebx, twice;
    uint8_t al = M8(V(0x208033b8));

    ebx = (uint16_t)ebx;
    esi = M32(V(0x20803490));
    if (M8(ebx + esi) < al)
        return;
    s_ebp = native_ebp;
    s_eax = eax;
    s_ecx = ecx;
    s_edx = edx;
    s_ebx = ebx;
    SET_AL((uint8_t)(M8(ebx + esi) - al));
    SET_AH(0);
    if (AX >= 3)
        SET_AX(3);
    SET_DI(AX);
    twice = (uint32_t)AX * 2;
    native_ebp = twice * 2;
    eax = G;
    if (M16(eax + 0xe86) != 0)
        native_ebp += 0x10;
    SET_AX(BX & 0x101);
    SET_AL((uint8_t)(AL + AL));
    SET_AL(AL | AH);
    eax = AL;
    native_ebp += eax;
    SET_AL(M8(V(0x2080349c) + native_ebp));
    M8(V(0x20803502)) = AL;
    native_ebp = M32(V(0x20803402) + twice * 2);
    M16(V(0x2080344c)) = 1;
    M16(V(0x2080344e)) = 1;
    M16(V(0x20803450)) = 1;
    M16(V(0x20803452)) = 1;
    M16(V(0x20803454)) = 1;
    esi = CALL(G3D, 0x40, f0)();                   /* ecx and edx as they were */
    eax = s_eax;

    face_put(0);
    face_put(0x70);
    face_put(0x80);
    face_put(0xc0);
    face_put(0x640);
    face_put(0x650);
    face_put(0x660);
    face_put(0x670);
    face_put(0x680);
    eax += M32(V(0x208033ba));
    ecx += M32(V(0x208033be));
    edx += M32(V(0x208033c2));
    face_put(0x10);
    face_put(0x20);
    face_put(0x90);
    face_put(0xd0);
    eax += M32(V(0x208033d2));
    ecx += M32(V(0x208033d6));
    edx += M32(V(0x208033da));
    face_put(0x30);
    face_put(0x40);
    face_put(0xa0);
    face_put(0xe0);
    eax -= M32(V(0x208033ba));
    ecx -= M32(V(0x208033be));
    edx -= M32(V(0x208033c2));
    face_put(0x50);
    face_put(0x60);
    face_put(0xb0);
    face_put(0xf0);
    face_put(0x690);
    face_put(0x6a0);
    face_put(0x6b0);
    face_put(0x6c0);

    face_vector();
    face_add(0x80);
    face_add(0x90);
    face_add(0xa0);
    face_add(0xb0);
    face_add(0x640);
    face_add(0x650);
    face_add(0x660);
    face_add(0x670);
    face_add(0x680);
    face_add(0x690);
    face_add(0x6a0);
    face_add(0x6b0);
    face_add(0x6c0);

    eax = M32(V(0x20803458) + ((uint32_t)DI << 2)) - M32(G + 0xce0);
    if ((int32_t)eax >= 0)
        M16(V(0x20803454)) = 0;

    face_neighbour(0, 0xff, 0, V(0x2080344e));
    face_neighbour(1, 0, 0x20, V(0x20803450));
    face_neighbour(0, 1, 0x40, V(0x2080344c));
    face_neighbour(0xff, 0, 0x60, V(0x20803452));

    main_xcall1(SLOT_OF(G3D, 0x1c), V(0x20803500));
    ebx = s_ebx;
    edx = s_edx;
    ecx = s_ecx;
    eax = s_eax;
    native_ebp = s_ebp;
}

/* .text+0x9772: a row of columns along bh, walking the position back first. */
static void face_row_down(void)
{
    uint32_t s_eax = eax, s_ecx = ecx, s_edx = edx;
    uint16_t s_bx = BX;

    M16(V(0x20803444)) = 1;
    M16(V(0x20803446)) = 0;
    SET_BP(0x10);
    eax -= M32(V(0x2080341e));
    ecx -= M32(V(0x20803422));
    edx -= M32(V(0x20803426));
    SET_BH((uint8_t)((ebx >> 8) - 0x10));
    do {
        face_column();
        eax += M32(V(0x208033d2));
        ecx += M32(V(0x208033d6));
        edx += M32(V(0x208033da));
        SET_BH((uint8_t)((ebx >> 8) + 1));
        SET_BP(BP - 1);
    } while ((int16_t)BP > 0);
    SET_BX(s_bx);
    edx = s_edx;
    ecx = s_ecx;
    eax = s_eax;
}

/* .text+0x97c5: and forward, one more. */
static void face_row_up(void)
{
    uint32_t s_eax = eax, s_ecx = ecx, s_edx = edx;
    uint16_t s_bx = BX;

    M16(V(0x20803444)) = 0;
    M16(V(0x20803446)) = 1;
    SET_BP(0x10);
    eax += M32(V(0x2080341e));
    ecx += M32(V(0x20803422));
    edx += M32(V(0x20803426));
    SET_BH((uint8_t)((ebx >> 8) + 0x10));
    do {
        face_column();
        eax -= M32(V(0x208033d2));
        ecx -= M32(V(0x208033d6));
        edx -= M32(V(0x208033da));
        SET_BH((uint8_t)((ebx >> 8) - 1));
        SET_BP(BP - 1);
    } while ((int16_t)BP >= 0);
    SET_BX(s_bx);
    edx = s_edx;
    ecx = s_ecx;
    eax = s_eax;
}

/* One step of +0x9818's edge arithmetic: `x * 0x346c`, doubled, less the
 * original, by 0x3468. */
static uint32_t face_scale(uint32_t value)
{
    int64_t p = (int64_t)(int32_t)value * (int32_t)M32(V(0x2080346c));

    eax = (uint32_t)p;
    edx = (uint32_t)((uint64_t)p >> 32);
    edx = edx << 1 | eax >> 31;
    return edx;
}

/* .text+0x9818: layer ax's faces, when +0x9d57 finds its map.  The map stays
 * locked over the walk. */
static void faces_draw(void)
{
    uint32_t i;

    edi = G;
    if (M16(edi + 0xdce) != 0)
        return;
    {
        uint16_t which = AX;

        CALL(HG2D, 0x88, f2)(0xa200, V(0x208035de));
        M8(V(0x208033b8)) = M8(V(0x208035df));
        M8(V(0x208033b9)) = M8(V(0x208035df));
        eax = clouds_9d57(which);
    }
    if (AH == 0)
        return;
    M8(V(0x2080343d)) = CL;
    M8(V(0x20803441)) = CH;
    M16(V(0x20803436)) = BX;
    edi = G;
    eax = main_xcall1(SLOT_OF(HG2D, 0xb4), M32(edi + 0xc8c));
    M32(V(0x20803490)) = eax;
    if (M16(V(0x20803474)) == 2) {
        M8(V(0x2080343c)) = M8(V(0x20801ec7));
        M8(V(0x20803440)) = M8(V(0x20801ecb));
        SET_AX(M16(edi + 0xe72));
        SET_BX(M16(edi + 0xe74));
        M16(V(0x2080348e)) = BX;
        SET_BX(BX >> 1);
        SET_AX(AX - BX);
        M16(V(0x2080348a)) = AX;
        M32(V(0x208034cc)) = 0x320;
    } else {
        M8(V(0x2080343c)) = M8(V(0x20801ebf));
        M8(V(0x20803440)) = M8(V(0x20801ec3));
        SET_AX(M16(edi + 0xe6a));
        SET_BX(M16(edi + 0xe6c));
        M16(V(0x2080348e)) = BX;
        SET_BX(BX >> 1);
        SET_AX(AX - BX);
        M16(V(0x2080348a)) = AX;
        M32(V(0x208034cc)) = 0x258;
    }

    ebx = M32(V(0x20803470));
    eax = (uint32_t)((int32_t)(int16_t)M16(V(0x20803438)) * (int32_t)M32(V(0x20803498)));
    eax += M32(ebx + 4);
    M32(V(0x2080347a)) = eax;
    M16(V(0x20803480)) = M16(ebx + 0xa);
    M32(V(0x20803482)) = M32(ebx + 0xc);

    edi = G;
    ebx = edi + 0x5b0;
    main_xcall3(SLOT_OF(UTIL, 0x44), V(0x20803494), ebx, V(0x208033b0));
    SET_AX(M16(V(0x208033b2)));
    SET_CX(M16(V(0x208033b4)));
    SET_DX(M16(V(0x208033b6)));
    SET_BX(M16(V(0x2080343a)));
    if ((int16_t)BX < 0) {
        do {
            uint32_t c = (uint32_t)M16(V(0x20803480)) - AX;

            M16(V(0x20803480)) = (uint16_t)c;
            c = (uint32_t)M16(V(0x20803482)) - CX - ((c >> 16) != 0);
            M16(V(0x20803482)) = (uint16_t)c;
            M16(V(0x20803484)) = (uint16_t)(M16(V(0x20803484)) - DX - ((c >> 16) != 0));
            SET_BX(BX + 1);
        } while (BX != 0);
    } else if (BX != 0) {
        do {
            uint32_t c = (uint32_t)M16(V(0x20803480)) + AX;

            M16(V(0x20803480)) = (uint16_t)c;
            c = (uint32_t)M16(V(0x20803482)) + CX + (c >> 16);
            M16(V(0x20803482)) = (uint16_t)c;
            M16(V(0x20803484)) = (uint16_t)(M16(V(0x20803484)) + DX + (c >> 16));
            SET_BX(BX - 1);
        } while (BX != 0);
    }

    SET_AX(0);
    SET_AL(M8(V(0x2080343d)));
    M16(V(0x208034f0)) = AX;
    SET_AL(M8(V(0x20803441)));
    M16(V(0x208034f4)) = AX;
    main_xcall1(SLOT_OF(G3D, 0x1c), V(0x208034bc));
    edi = G;
    SET_AX(M16(edi + 0xce2));
    M16(V(0x20803456)) = AX;
    main_xcall0(SLOT_OF(G3D, 0x40));

    esi = eax;
    eax = M32(esi);
    ebx = M32(esi + 4);
    ecx = M32(esi + 8);
    edx = M32(esi + 0x10) - eax;
    edi = M32(esi + 0x14) - ebx;
    native_ebp = M32(esi + 0x18) - ecx;
    M32(V(0x208033ba)) = edx;
    M32(V(0x208033be)) = edi;
    M32(V(0x208033c2)) = native_ebp;
    M32(V(0x20803412)) = edx << 4;
    M32(V(0x20803416)) = edi << 4;
    M32(V(0x2080341a)) = native_ebp << 4;
    edx = M32(esi + 0x30) - eax;
    edi = M32(esi + 0x34) - ebx;
    native_ebp = M32(esi + 0x38) - ecx;
    M32(V(0x208033d2)) = edx;
    M32(V(0x208033d6)) = edi;
    M32(V(0x208033da)) = native_ebp;
    M32(V(0x2080341e)) = edx << 4;
    M32(V(0x20803422)) = edi << 4;
    M32(V(0x20803426)) = native_ebp << 4;
    edx = M32(esi + 0x20) - eax;
    edi = M32(esi + 0x24) - ebx;
    native_ebp = M32(esi + 0x28) - ecx;
    M32(V(0x208033c6)) = edx;
    M32(V(0x208033ca)) = edi;
    M32(V(0x208033ce)) = native_ebp;

    SET_AX(M16(V(0x208034cc)));
    SET_BX(M16(V(0x2080348e)));
    {
        uint32_t p = (uint32_t)AX * BX;

        SET_AX(p);
        SET_DX(p >> 16);
        edx = edx << 16;
        SET_DX(AX);
        ebx = edx;
    }
    for (i = 0; i < 3; i++) {
        uint32_t at = 4 * i, v, q;
        int64_t p;

        p = (int64_t)(int32_t)M32(V(0x208033c6) + at) * (int32_t)ebx;
        v = (uint32_t)((uint64_t)p >> 16);
        M32(V(0x208033f6) + at) = v;
        M32(V(0x208033ea) + at) = v;
        M32(V(0x208033de) + at) = v;
        M32(V(0x208033c6) + at) = v;
        edx = face_scale(v);
        eax = v - edx;
        M32(V(0x208033c6) + at) = edx;
        q = idiv32((int64_t)(int32_t)eax, (int32_t)M32(V(0x20803468)));
        M32(V(0x208033ea) + at) -= q;
        M32(V(0x208033de) + at) -= q << 1;
        eax = q << 1;
    }
    M32(V(0x20803464)) = ebx;
    M32(V(0x20803460)) = ebx;
    M32(V(0x2080345c)) = ebx;
    M32(V(0x20803458)) = ebx;
    {
        uint32_t q;

        edx = face_scale(ebx);
        eax = M32(V(0x20803458)) - edx;
        M32(V(0x20803458)) = edx;
        q = idiv32((int64_t)(int32_t)eax, (int32_t)M32(V(0x20803468)));
        M32(V(0x20803460)) -= q;
        M32(V(0x2080345c)) -= q << 1;
        eax = q << 1;
    }

    edx = M32(esi + 0x40);
    edi = M32(esi + 0x44);
    native_ebp = M32(esi + 0x48);
    M32(V(0x2080342a)) = edx;
    M32(V(0x2080342e)) = edi;
    M32(V(0x20803432)) = native_ebp;

    M16(V(0x2080344a)) = 0;
    M16(V(0x20803448)) = 1;
    SET_BP(0x10);
    eax = M32(V(0x2080342a)) - M32(V(0x20803412));
    ecx = M32(V(0x2080342e)) - M32(V(0x20803416));
    edx = M32(V(0x20803432)) - M32(V(0x2080341a));
    SET_BX(M16(V(0x20803436)));
    SET_BL((uint8_t)(BL - 0x10));
    do {
        uint16_t bp = BP;

        face_row_down();
        face_row_up();
        eax += M32(V(0x208033ba));
        ecx += M32(V(0x208033be));
        edx += M32(V(0x208033c2));
        SET_BL((uint8_t)(BL + 1));
        SET_BP(bp - 1);
    } while ((int16_t)BP > 0);

    M16(V(0x2080344a)) = 1;
    M16(V(0x20803448)) = 0;
    SET_BP(0x10);
    eax = M32(V(0x2080342a)) + M32(V(0x20803412));
    ecx = M32(V(0x2080342e)) + M32(V(0x20803416));
    edx = M32(V(0x20803432)) + M32(V(0x2080341a));
    SET_BX(M16(V(0x20803436)));
    SET_BL((uint8_t)(BL + 0x10));
    do {
        uint16_t bp = BP;

        face_row_down();
        face_row_up();
        eax -= M32(V(0x208033ba));
        ecx -= M32(V(0x208033be));
        edx -= M32(V(0x208033c2));
        SET_BL((uint8_t)(BL - 1));
        SET_BP(bp - 1);
    } while ((int16_t)BP >= 0);

    eax = M32(V(0x20803490));
    if (eax != 0) {
        edi = G;
        main_xcall2(SLOT_OF(HG2D, 0xb8), M32(edi + 0xc8c), 0);
        M32(V(0x20803490)) = 0;
    }
}

/* .text+0x73d7 */
NATIVE(native_73d7)
{
    if (M16(V(0x20801ff5)) != 0) {
        SET_AX(1);
        faces_draw();
    }
    edi = G;
    M16(edi + 0xc90) = 0;
}

/* .text+0x73fb */
NATIVE(native_73fb)
{
    if (M16(V(0x20801ff7)) != 0) {
        SET_AX(2);
        faces_draw();
    }
    edi = G;
    M16(edi + 0xc90) = 0;
}

/* .text+0x7bc2 */
NATIVE(native_7bc2)
{
    edi = G;
    eax = M32(edi + 0xd4c);
    if (eax < M32(V(0x20801e52))) {
        SET_AX(M16(edi + 0xe7a));
        if (AX >= M16(edi + 0x574)) {
            SET_AX(AX - M16(edi + 0xe80));
            if (AX <= M16(edi + 0x574)) {
                M16(V(0x20801eb4)) |= 0x8007;
                return;
            }
        }
    }
    eax = M32(edi + 0xd4c);
    if (eax < M32(V(0x20801e4e))) {
        M16(V(0x20801eb4)) &= 0xfff9;
        M16(V(0x20801eb4)) |= 4;
        return;
    }
    eax = M32(edi + 0xd4c);
    if (eax < M32(V(0x20801e4a))) {
        M16(V(0x20801eb4)) &= 0xfff9;
        M16(V(0x20801eb4)) |= 2;
    }
}

/* .text+0x8c75: one tile to G3D +0x1c. */
static void tile_draw(void)
{
    M16(V(0x2080206c)) = AX;
    M16(V(0x2080206e)) = DX;
    M16(V(0x20802074)) = BX;
    M16(V(0x20802076)) = CX;
    M16(V(0x2080207c)) = SI;
    M16(V(0x2080207e)) = DI;
    edi = G;
    SET_AX(M16(edi + 0xb50));
    M16(V(0x20802a9e)) = AX;
    M32(V(0x20801a72)) = V(0x20802a8e);
    main_xcall1(SLOT_OF(G3D, 0x1c), M32(V(0x20801a72)));
}

/* .text+0x8935: the nine tiles round the point, or, from an angle of 0xe001
 * up, one block. */
static void tiles_draw(void)
{
    static const struct { int8_t x, y; } at[9] = {
        { 0, 0 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 },
        { 1, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 },
    };
    uint32_t i;

    M16(V(0x2080205c)) = DI;
    edi = G;
    if (M16(edi + 0xdce) != 0)
        return;
    SET_CX(CX + 0x80);
    M16(V(0x20802052)) = AX;
    M16(V(0x20802054)) = DX;
    M16(V(0x20802056)) = BX;
    M16(V(0x20802058)) = CX;
    M16(V(0x2080205a)) = SI;
    if (BP >= 0xe001) {
        M16(V(0x2080206c)) = AX;
        M16(V(0x2080206e)) = DX;
        M16(V(0x20802074)) = BX;
        M16(V(0x20802076)) = CX;
        M16(V(0x2080207c)) = SI;
        SET_DI(M16(V(0x2080205c)));
        M16(V(0x2080207e)) = DI;
        M32(V(0x20801a72)) = V(0x20802a3c);
        main_xcall1(SLOT_OF(G3D, 0x1c), M32(V(0x20801a72)));
        return;
    }
    SET_BP(BP >> 1);
    M16(V(0x2080205e)) = BP;
    M32(V(0x2080203d)) = 0x1000000;
    M16(V(0x2080203b)) = 0;
    main_xcall3(SLOT_OF(UTIL, 0x48), V(0x20802039), edi + 0x560, V(0x20802041));
    SET_AX(M16(V(0x20802045)));
    SET_DX(M16(V(0x20802047)));
    M16(V(0x20802060)) = AX;
    M16(V(0x20802062)) = DX;
    {
        uint32_t half = ((uint32_t)DX << 16 | AX) >> 1;
        uint32_t c = (uint32_t)M16(V(0x20802052)) + (uint16_t)half;

        M16(V(0x20802052)) = (uint16_t)c;
        M16(V(0x20802054)) = (uint16_t)(M16(V(0x20802054)) + (half >> 16) + (c >> 16));
    }
    for (i = 0; i < 9; i++) {
        uint32_t x = (uint32_t)M16(V(0x20802054)) << 16 | M16(V(0x20802052));
        uint32_t y = (uint32_t)M16(V(0x20802058)) << 16 | M16(V(0x20802056));
        uint32_t dx = (uint32_t)M16(V(0x20802062)) << 16 | M16(V(0x20802060));
        uint32_t dy = (uint32_t)M16(V(0x20802066)) << 16 | M16(V(0x20802064));

        x += at[i].x * dx;
        y += at[i].y * dy;
        SET_AX(x);
        SET_DX(x >> 16);
        SET_BX(y);
        SET_CX(y >> 16);
        SET_SI(M16(V(0x2080205a)));
        SET_DI(M16(V(0x2080205c)));
        tile_draw();
    }
}

/* .text+0x889c */
NATIVE(native_889c)
{
    edi = G;
    SET_AX(M16(edi + 0xb3c));
    SET_DX(M16(edi + 0xb3e));
    SET_BX(M16(edi + 0xb44));
    SET_CX(M16(edi + 0xb46));
    SET_BP(M16(edi + 0xe6e));
    SET_SI(M16(V(0x20801a94)));
    SET_DI(M16(V(0x20801a96)));
    M16(V(0x20802a76)) = 0x4000;
    tiles_draw();
}

/* .text+0x88e2 */
NATIVE(native_88e2)
{
    uint32_t c;

    edi = G;
    SET_AX(M16(edi + 0xb3c));
    SET_DX(M16(edi + 0xb3e));
    SET_BX(M16(edi + 0xb44));
    SET_CX(M16(edi + 0xb46));
    SET_SI(M16(V(0x20801aac)));
    SET_DI(M16(V(0x20801aae)));
    SET_BP(M16(edi + 0xe76));
    SET_CX(CX + 0xc0);
    c = (uint32_t)AX + 0x8000;
    SET_AX(c);
    SET_DX(DX + (c >> 16));
    M16(V(0x20802a76)) = 0x7fff;
    tiles_draw();
}

/* .text+0x8d47: through the caller's edi. */
NATIVE(native_8d47)
{
    eax = M32(V(0x20801ebe));
    M32(edi + 0xc94) = eax;
    eax = M32(V(0x20801ec2));
    M32(edi + 0xc9c) = eax;
}

/* .text+0x8d5e */
NATIVE(native_8d5e)
{
    edi = G;
    eax = M32(V(0x20801ec6));
    M32(edi + 0xc94) = eax;
    eax = M32(V(0x20801eca));
    M32(edi + 0xc9c) = eax;
}

/* .text+0x8f34 */
NATIVE(native_8f34)
{
    edi = G;
    SET_AX(M16(edi + 0xe88));
    M16(V(0x20802017)) = AX;
    M16(edi + 0xe88) = 0x1fff;
}

/* .text+0x8f51 */
NATIVE(native_8f51)
{
    edi = G;
    SET_AX(M16(edi + 0xe88));
    M16(V(0x20802017)) = AX;
    M16(edi + 0xe88) = 0x7fff;
}

/* .text+0x8f73: GLOBAL +0xd4c for eax; the old value into .data+0x2019. */
static void timer_swap(void)
{
    uint32_t old;

    edi = G;
    old = M32(edi + 0xd4c);
    M32(edi + 0xd4c) = eax;
    eax = old;
    M32(V(0x20802019)) = eax;
}

/* .text+0x8f6e */
NATIVE(native_8f6e)
{
    eax = 0xffffffffu;
    timer_swap();
}

/* .text+0x8f85 */
NATIVE(native_8f85)
{
    eax = 0;
    timer_swap();
}

/* .text+0x8f89 */
NATIVE(native_8f89)
{
    eax = M32(V(0x20802019));
    timer_swap();
}

/* .text+0x8f90 */
NATIVE(native_8f90)
{
    edi = G;
    SET_AX(M16(edi + 0xe88));
    M16(V(0x20802017)) = AX;
    if (M16(edi + 0xe86) != 0)
        M16(edi + 0xe88) = 0x7fff;
}

/* .text+0x8fb8 */
NATIVE(native_8fb8)
{
    edi = G;
    SET_AX(M16(V(0x20802017)));
    M16(edi + 0xe88) = AX;
}

/* .text+0xa04a */
NATIVE(native_a04a)
{
}

/* The blocks' pointers, by the .data offset of each (retail's values are in
 * retail_callbacks). */
static const struct { uint32_t at; void (*fn)(void); } native_pointers[] = {
    { 0x2386, native_73d7 },
    { 0x23aa, native_73d7 },
    { 0x2466, native_73d7 },
    { 0x2716, native_73d7 },
    { 0x23ce, native_73fb },
    { 0x23f2, native_73fb },
    { 0x2822, native_73fb },
    { 0x290a, native_73fb },
    { 0x313a, native_7bc2 },
    { 0x3160, native_7bc2 },
    { 0x3186, native_7bc2 },
    { 0x31ac, native_7bc2 },
    { 0x31d2, native_7bc2 },
    { 0x31f8, native_7bc2 },
    { 0x321e, native_7bc2 },
    { 0x3244, native_7bc2 },
    { 0x326a, native_7bc2 },
    { 0x3290, native_7bc2 },
    { 0x32b6, native_7bc2 },
    { 0x32dc, native_7bc2 },
    { 0x3302, native_7bc2 },
    { 0x3328, native_7bc2 },
    { 0x334e, native_7bc2 },
    { 0x3374, native_7bc2 },
    { 0x262c, native_889c },
    { 0x282e, native_88e2 },
    { 0x2916, native_88e2 },
    { 0x243c, native_8d47 },
    { 0x2674, native_8d47 },
    { 0x26e4, native_8d47 },
    { 0x278e, native_8d47 },
    { 0x27f4, native_8d5e },
    { 0x2876, native_8d5e },
    { 0x28e8, native_8d5e },
    { 0x295e, native_8d5e },
    { 0x2692, native_8f34 },
    { 0x2702, native_8f34 },
    { 0x2894, native_8f34 },
    { 0x297c, native_8f34 },
    { 0x2812, native_8f51 },
    { 0x24a8, native_8f6e },
    { 0x2990, native_8f85 },
    { 0x24ce, native_8f89 },
    { 0x29ca, native_8f89 },
    { 0x29ea, native_8f89 },
    { 0x307c, native_8f90 },
    { 0x269c, native_8fb8 },
    { 0x270c, native_8fb8 },
    { 0x281c, native_8fb8 },
    { 0x289e, native_8fb8 },
    { 0x2986, native_8fb8 },
    { 0x3088, native_8fb8 },
    { 0x34fa, native_a04a },
};

#undef BP
#undef SI
#undef DI
