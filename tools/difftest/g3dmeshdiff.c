/* G3D's mesh compiler (0x38) three ways over the same polygons: retail's
 * instructions (src/g3d/g3d_mesh.S), the C (src/g3d/g3d_mesh.c) on the
 * x87, and the C on its software float (FS98_PORTABLE_FP), each under
 * the same precision control.  The whole program buffer is compared
 * after each, and the control word.
 *
 * Polygons are drawn in a plane (star-shaped, so some are concave), now
 * and then bent out of it, with repeated points, shuffled orders and
 * index spreads round the 0xff limit, at scales from a few units to
 * 2^26, as facets (0x3e), faces (0x1d) and surfaces (0x05/0x0f/0x10/0x08,
 * or after a 0x18), behind 0x1e, 0x7d and 0x81 or straight after the
 * 0x38.
 *
 * Usage: g3dmeshdiff [N [first]] */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STDCALL __attribute__((stdcall))
#define PROGRAM 0x800
#define NPOINTS 0x400

struct { uint8_t bytes[0x200]; } g3d_image;

uint32_t STDCALL g3d_compile_mesh(uint32_t program) __asm__("_g3d_compile_mesh@4");
uint32_t g3d_mesh_hw(uint32_t program, uint32_t points, uint32_t (STDCALL *next)(uint32_t));
uint32_t g3d_mesh_soft(uint32_t program, uint32_t points, uint32_t (STDCALL *next)(uint32_t));
extern int g3d_mesh_precision;

static int32_t points[NPOINTS][4];
static uint8_t base[PROGRAM], side[3][PROGRAM];

/* +0x64 for the three opcodes the compiler steps over, lengths of the
 * harness's own (each side calls the same one). */
uint32_t STDCALL next_instruction(uint32_t p) __asm__("_next_instruction");
uint32_t STDCALL next_instruction(uint32_t p)
{
    uint16_t op;

    memcpy(&op, (void *)(uintptr_t)p, 2);
    return op == 0x1e ? p + 4 : op == 0x7d ? p + 6 : op == 0x81 ? p + 8 : 0;
}

static uint32_t seed;

static uint32_t rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static double frand(void)
{
    return (rnd() & 0xffffff) / 16777216.0;
}

static void put16(uint8_t **p, uint32_t v)
{
    (*p)[0] = (uint8_t)v;
    (*p)[1] = (uint8_t)(v >> 8);
    *p += 2;
}

static uint16_t control_word(void)
{
    uint16_t cw;

    __asm__ __volatile__ ("fnstcw %0" : "=m" (cw));
    return cw;
}

static void set_control_word(uint16_t cw)
{
    __asm__ __volatile__ ("fldcw %0" : : "m" (cw));
}

/* n vertices into the point table from `first`, their indices to idx. */
static void make_polygon(uint32_t n, uint16_t *idx)
{
    double ang[150], u[3], v[3], o[3], scale, t;
    uint32_t i, k, first, step;
    int bent = rnd() % 6 == 0;

    for (i = 0; i < n; i++)
        ang[i] = frand() * 6.283185307179586;
    if (rnd() % 8)                        /* in order, so simple; else not */
        for (i = 1; i < n; i++)
            for (k = i; k > 0 && ang[k - 1] > ang[k]; k--) {
                t = ang[k];
                ang[k] = ang[k - 1];
                ang[k - 1] = t;
            }
    for (k = 0; k < 3; k++) {
        u[k] = frand() * 2 - 1;
        v[k] = frand() * 2 - 1;
    }
    if (rnd() % 3 == 0) {                 /* or along the axes */
        memset(u, 0, sizeof u);
        memset(v, 0, sizeof v);
        k = rnd() % 3;
        u[k] = 1;
        v[(k + 1 + rnd() % 2) % 3] = 1;
    }
    scale = ldexp(1.0, (int)(rnd() % 26)) * (1 + frand());
    for (k = 0; k < 3; k++)
        o[k] = (frand() * 2 - 1) * scale * (rnd() % 2 ? 4 : 0.25);
    first = rnd() % (NPOINTS - 0x200);
    step = rnd() % 4 ? 1 : 1 + rnd() % 3;     /* spreads past 0xff now and then */
    for (i = 0; i < n; i++) {
        double r = scale * (rnd() % 3 ? 0.5 + frand() : 1.0), x = r * cos(ang[i]), y = r * sin(ang[i]);
        uint32_t at = (first + i * step) % NPOINTS;

        for (k = 0; k < 3; k++) {
            double c = o[k] + x * u[k] + y * v[k];

            if (bent)
                c += (frand() * 2 - 1) * scale * 0.05;
            if (rnd() % 64 == 0)
                c = 0;
            if (c > 2147483000.0)
                c = 2147483000.0;
            if (c < -2147483000.0)
                c = -2147483000.0;
            points[at][k] = (int32_t)c;
        }
        idx[i] = (uint16_t)at;
        if (i > 0 && rnd() % 40 == 0)     /* a repeated point */
            idx[i] = idx[i - 1];
    }
    if (rnd() % 30 == 0 && n > 4)         /* three in a row on a line */
        for (k = 0; k < 3; k++)
            points[idx[2]][k] = (points[idx[1]][k] - points[idx[0]][k]) * 2 + points[idx[0]][k];
}

static uint32_t polygon_size(void)
{
    uint32_t k = rnd() % 100;

    return k < 3 ? 3 : k < 70 ? 4 + rnd() % 9 : k < 97 ? 13 + rnd() % 30 : 43 + rnd() % 108;
}

static const char *make_program(void)
{
    uint8_t *p = base;
    uint16_t idx[150];
    uint32_t n = polygon_size(), kind = rnd() % 4, i, skips;
    static const char *names[4] = { "facet", "face", "surface", "0x18 surface" };

    for (i = 0; i < PROGRAM; i++)
        base[i] = (uint8_t)rnd();
    p = base + 0x10;
    put16(&p, 0x38);
    skips = rnd() % 3 ? 0 : 1 + rnd() % 3;
    for (i = 0; i < skips; i++) {
        uint32_t op = rnd() % 3, len = op == 0 ? 4 : op == 1 ? 6 : 8, k;

        put16(&p, op == 0 ? 0x1e : op == 1 ? 0x7d : 0x81);
        for (k = 2; k < len; k += 2)
            put16(&p, rnd() % 4 ? (uint16_t)rnd() : 2);
    }
    make_polygon(n, idx);
    switch (kind) {
    case 0:
        put16(&p, 0x3e);
        put16(&p, n);
        for (i = 0; i < 5; i++)
            put16(&p, (uint16_t)rnd());
        for (i = 0; i < n; i++)
            put16(&p, idx[i]);
        break;
    case 1:
        put16(&p, 0x1d);
        put16(&p, n);
        for (i = 0; i < 6; i++)
            put16(&p, (uint16_t)rnd());
        for (i = 0; i < n; i++)
            put16(&p, idx[i]);
        break;
    default:
        if (kind == 3) {
            put16(&p, 0x18);
            for (i = 2; i < 0x18; i += 2)
                put16(&p, (uint16_t)rnd());
        } else {
            put16(&p, 0x05);
        }
        for (i = 0; i < n; i++) {
            put16(&p, i == 0 ? 0x0f : 0x10);
            put16(&p, idx[i]);
        }
        if (rnd() % 30)
            put16(&p, 0x08);
        break;
    }
    return names[kind];
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 100000;
    uint32_t first = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0, s, k;
    uint32_t differ[2] = { 0, 0 }, meshes = 0, refused = 0, by_pc[3] = { 0, 0, 0 };
    static const uint16_t words[3] = { 0x007f, 0x027f, 0x037f };
    static const int bits[3] = { 24, 53, 64 };
    uint32_t table = (uint32_t)(uintptr_t)points;
    uint16_t saved = control_word();

    memcpy(g3d_image.bytes + 0x12a, &table, 4);
    for (s = first; s < first + n; s++) {
        const char *kind;
        uint32_t pc;
        uint16_t cw[3];

        seed = s * 2654435761u + 0x9e3779b9u;
        if (seed == 0)
            seed = 1;
        rnd();
        pc = rnd() % 3;
        kind = make_program();
        for (k = 0; k < 3; k++)
            memcpy(side[k], base, PROGRAM);
        g3d_mesh_precision = bits[pc];
        set_control_word(words[pc]);
        g3d_compile_mesh((uint32_t)(uintptr_t)(side[0] + 0x10));
        cw[0] = control_word();
        set_control_word(words[pc]);
        g3d_mesh_hw((uint32_t)(uintptr_t)(side[1] + 0x10), table, next_instruction);
        cw[1] = control_word();
        set_control_word(words[pc]);
        g3d_mesh_soft((uint32_t)(uintptr_t)(side[2] + 0x10), table, next_instruction);
        cw[2] = control_word();
        set_control_word(saved);
        by_pc[pc]++;
        if (memchr(side[0], 0x90, PROGRAM) && memcmp(side[0], base, PROGRAM) != 0) {
            uint32_t i;

            for (i = 0x12; i + 1 < PROGRAM; i += 2)
                if (side[0][i] == 0x90 && side[0][i + 1] == 0 && base[i] != 0x90)
                    break;
            if (i + 1 < PROGRAM)
                meshes++;
            else
                refused++;
        } else {
            refused++;
        }
        for (k = 1; k < 3; k++)
            if (memcmp(side[0], side[k], PROGRAM) != 0 || cw[0] != cw[k]) {
                uint32_t i;

                if (differ[k - 1]++ < 5) {
                    for (i = 0; i < PROGRAM && side[0][i] == side[k][i]; i++)
                        ;
                    printf("scenario %u (%s, %d-bit): %s differs at byte %#x (%02x, %02x), cw %04x %04x\n",
                           s, kind, bits[pc], k == 1 ? "x87 C" : "software C", i,
                           i < PROGRAM ? side[0][i] : 0, i < PROGRAM ? side[k][i] : 0, cw[0], cw[k]);
                }
            }
    }
    printf("g3dmeshdiff: %u polygons (24/53/64-bit: %u %u %u), %u meshed, %u not; "
           "x87 C %u differ, software C %u differ\n",
           n, by_pc[0], by_pc[1], by_pc[2], meshes, refused, differ[0], differ[1]);
    return differ[0] || differ[1];
}
