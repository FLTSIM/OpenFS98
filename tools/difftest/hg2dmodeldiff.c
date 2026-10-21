/* The HG2D model (hg2d_model.inc) against retail HG2D where the model is
 * meant to be exact: the texture pool, +0xac and +0xb0.  Retail HG2D is
 * loaded for real (its DllMain does nothing; these two slots need only the
 * host's allocator) and driven side by side with the model over the same
 * scripts: descriptors right and wrong, flags, handles live, freed, null
 * and out of the pool, enough textures to fill it, and the host's
 * allocation failing now and then.  Compared after every operation: the
 * return (a handle as its pool index; +0xb0's eax, which is the caller's
 * unless the host's free ran), and every record's first four dwords
 * (pixels as a block number), through a transcript per side.
 *
 * Usage: hg2dmodeldiff [N] */
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STDCALL __attribute__((stdcall))

static char transcript[2][1 << 22];     /* a long script dumps the pool after every step */
static unsigned length[2], side;

static void put(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (length[side] >= sizeof transcript[side] - 256) {
        printf("transcript full\n");      /* a comparison past here would be no comparison */
        exit(2);
    }
    length[side] += (unsigned)vsnprintf(transcript[side] + length[side],
                                        sizeof transcript[side] - length[side], fmt, ap);
    va_end(ap);
}

/* host blocks, numbered in allocation order */
#define MAXBLK 512
static void *blocks[MAXBLK];
static unsigned nblocks, fail_every, allocs;
static uint8_t pixels_given[0x100];         /* a descriptor's own pixels */

static void put_ptr(uint32_t v)
{
    unsigned i;

    if (v == 0) {
        put("0");
        return;
    }
    if (v == (uint32_t)(uintptr_t)pixels_given) {
        put("given");
        return;
    }
    for (i = 0; i < nblocks; i++)
        if (blocks[i] == (void *)(uintptr_t)v) {
            put("#%u", i);
            return;
        }
    put("?%08x", v);
}

static int32_t STDCALL h_alloc(int32_t flags, uint32_t size, void **out, int32_t tag)
{
    put(" alloc(%d,%#x,%d)", flags, size, tag);
    allocs++;
    if (fail_every && allocs % fail_every == 0) {
        put("=fail");
        return -1;
    }
    *out = calloc(1, size);
    if (*out == NULL) {
        printf("out of memory\n");
        exit(2);
    }
    blocks[nblocks < MAXBLK ? nblocks : MAXBLK - 1] = *out;
    put("=#%u", nblocks);
    if (nblocks < MAXBLK - 1)
        nblocks++;
    return 0;
}

static int32_t STDCALL h_free(void **p)
{
    put(" free(");
    put_ptr((uint32_t)(uintptr_t)*p);
    put(")");
    *p = NULL;
    return 0x777;
}

/* the model's own logging is not retail's: off here */
static void no_put(const char *fmt, ...) { (void)fmt; }
#define HG2D_PUT no_put
#define HG2D_PUT_PTR(v) ((void)(v))
#include "hg2d_model.inc"

/* +0xb0 is entered with eax the caller's (PANELS's link to HG2D) */
uint32_t call_eax(void *fn, uint32_t a, uint32_t b, uint32_t eax) __asm__("_call_eax");
__asm__(".text\n_call_eax:\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"
        "pushl 28(%esp)\n\tpushl 28(%esp)\n\t"
        "movl 40(%esp), %eax\n\t"
        "call *28(%esp)\n\t"
        "popl %ebp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");

static uint32_t rs;

static uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}

#define RETAIL_POOL 0x20797f60u
#define RETAIL_POOL_END 0x2079a800u

static uint32_t *retail_linkage;

static uint32_t pool_base(void)
{
    return side == 0 ? RETAIL_POOL : (uint32_t)(uintptr_t)hg2d.pool;
}

static void put_handle(uint32_t h)
{
    uint32_t b = pool_base();

    if (h >= b && h < b + 0x34 * HG2D_POOL && (h - b) % 0x34 == 0)
        put("tex%u", (h - b) / 0x34);
    else if (h == 0)
        put("0");
    else if (h == b - 0x34)
        put("below");
    else
        put("%08x", h);
}

static void put_pool(void)
{
    uint32_t i, b = pool_base();

    put(" [");
    for (i = 0; i < HG2D_POOL; i++) {
        const uint32_t *r = (const uint32_t *)(uintptr_t)(b + 0x34 * i);

        if (r[0] || r[1] || r[2] || r[3]) {
            put(" %u:", i);
            put_ptr(r[0]);
            put(",%x,%x,%x", r[1], r[2], r[3]);
        }
    }
    put(" ]");
}

static void run(unsigned scenario)
{
    uint32_t handles[64], nh = 0, i, n, full;
    uint32_t desc[8][8];
    void *texture = side == 0 ? (void *)(uintptr_t)retail_linkage[0xac / 4] : (void *)hg2d_w_texture;
    void *untexture = side == 0 ? (void *)(uintptr_t)retail_linkage[0xb0 / 4] : (void *)hg2d_w_untexture;

    rs = scenario * 2654435761u + 0x12345;
    if (rs == 0)
        rs = 1;
    length[side] = 0;
    for (i = 0; i < nblocks; i++)
        free(blocks[i]);
    nblocks = allocs = 0;
    fail_every = rnd() % 3 == 0 ? 2 + rnd() % 6 : 0;
    if (side == 0)
        memset((void *)(uintptr_t)RETAIL_POOL, 0, RETAIL_POOL_END - RETAIL_POOL);
    else
        memset(hg2d.pool, 0, sizeof hg2d.pool);
    for (i = 0; i < 8; i++) {
        memset(desc[i], 0, sizeof desc[i]);
        desc[i][0] = rnd() % 6 ? 1 : rnd() % 3;
        desc[i][1] = rnd() % 6 ? 0x100 : 0x80;
        desc[i][6] = rnd() % 3 ? 0 : (uint32_t)(uintptr_t)pixels_given;
    }
    /* now and then long and mostly making textures, past a full pool */
    full = rnd() % 4 == 0;
    n = full ? 400 + rnd() % 150 : 1 + rnd() % 40;
    if (full)
        fail_every = 0;
    put("scenario %u\n", scenario);
    for (i = 0; i < n; i++) {
        uint32_t r;

        if ((full ? rnd() % 12 : rnd() % 3) || nh == 0) {
            uint32_t d = rnd() % 8, flags = rnd() % 8 ? 0 : 1 + rnd() % 2;

            put("+ac(d%u,%u)", d, flags);
            r = call_eax(texture, (uint32_t)(uintptr_t)desc[d], flags, 0x1111);
            put("=");
            put_handle(r);
            if (r && nh < 64)
                handles[nh++] = r;
        } else {
            uint32_t k = rnd() % 8, h, flags = rnd() % 4 ? 0 : 1;

            h = k < 6 ? handles[rnd() % nh] : k == 6 ? 0 : pool_base() - 0x34;
            put("+b0(");
            put_handle(h);
            put(",%u)", flags);
            r = call_eax(untexture, h, flags, 0x2222);
            put("=%x", r);
        }
        put_pool();
        put("\n");
    }
}

int main(int argc, char **argv)
{
    static uint8_t host[0x400];
    unsigned n = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : 20000, s, differ = 0;
    HMODULE h = LoadLibraryA("HG2D_retail.dll");
    uint32_t *imports;

    if (!h || (uint32_t)(uintptr_t)h != 0x20730000u) {
        printf("retail HG2D did not load at its base (%p)\n", (void *)h);
        return 2;
    }
    imports = (uint32_t *)GetProcAddress(h, "ImportTable");
    retail_linkage = (uint32_t *)GetProcAddress(h, "Linkage");
    *(void **)(host + 0x20c) = (void *)h_alloc;
    *(void **)(host + 0x214) = (void *)h_free;
    imports[1] = (uint32_t)(uintptr_t)host;
    hg2d.host_alloc = h_alloc;
    hg2d.host_free = h_free;
    hg2d_model_reset(rnd);
    (void)hg2d_model_put_state;
    for (s = 0; s < n; s++) {
        side = 0;
        run(s);
        side = 1;
        run(s);
        if (length[0] != length[1] || memcmp(transcript[0], transcript[1], length[0]) != 0) {
            if (differ++ < 3) {
                unsigned at = 0;

                while (at < length[0] && transcript[0][at] == transcript[1][at])
                    at++;
                while (at > 0 && transcript[0][at - 1] != '\n')
                    at--;
                printf("scenario %u differs:\n  retail %.300s\n  model  %.300s\n", s,
                       transcript[0] + at, transcript[1] + at);
            }
        }
    }
    printf("hg2dmodeldiff: %u scenarios, %u differ\n", n, differ);
    return differ != 0;
}
