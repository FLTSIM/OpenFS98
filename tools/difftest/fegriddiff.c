/* Differential test for FE.DLL's grid conversions, +0x60..+0x6c.
 *
 * The four are pure: a 48-bit latitude or longitude to the band it falls in
 * at a level, and back to the band's edge.  They read nothing but their
 * arguments and the band table in .data (+0xf20, 256 rising edges), so retail
 * and the reimplementation are loaded side by side and called directly, with
 * no host and no init.
 *
 * Vectors stay where callers stay: latitudes inside the table and bands whose
 * index lands in it.  Past those, retail's search reads on through .data and
 * beyond, and what lies there differs between two loaded DLLs.  Every edge
 * of the table is tried at, either side of and halfway across, at every level
 * from 0 to 24, then random values; levels past 24 go up to 0xffff for the
 * two that only shift.
 *
 * Usage: fegriddiff.exe [random-vectors]
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STDCALL __attribute__((stdcall))

typedef uint32_t (STDCALL *band_fn)(uint32_t p, uint32_t level);
typedef void     (STDCALL *edge_fn)(uint32_t band, uint32_t level, uint32_t out);

struct side {
    const char *name;
    uint8_t    *linkage;
    uint8_t    *bands;
    band_fn     lat_band, lon_band;
    edge_fn     band_lat, band_lon;
};

static struct side R, O;
static unsigned long long compared, mismatches;
static uint32_t rng = 0x2545f491u;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void load(struct side *s, const char *dll, const char *name)
{
    HMODULE m = LoadLibraryA(dll);
    uint8_t *link;

    if (!m) {
        fprintf(stderr, "cannot load %s (%lu)\n", dll, GetLastError());
        exit(2);
    }
    link = (uint8_t *)GetProcAddress(m, "Linkage");
    if (!link || *(uint32_t *)link != 18) {
        fprintf(stderr, "%s: no FE Linkage\n", dll);
        exit(2);
    }
    s->name = name;
    s->linkage = link;
    s->bands = link - 0x48 + 0xf20;
    s->lat_band = *(band_fn *)(link + 0x60);
    s->band_lat = *(edge_fn *)(link + 0x64);
    s->lon_band = *(band_fn *)(link + 0x68);
    s->band_lon = *(edge_fn *)(link + 0x6c);
}

static void miss(const char *what, uint32_t a, uint32_t b, const void *r, const void *o, size_t n)
{
    size_t k;

    if (++mismatches > 40)
        return;
    printf("MISMATCH %s(0x%08x, 0x%x):\n  retail", what, a, b);
    for (k = 0; k < n; k++)
        printf(" %02x", ((const uint8_t *)r)[k]);
    printf("\n  ours  ");
    for (k = 0; k < n; k++)
        printf(" %02x", ((const uint8_t *)o)[k]);
    printf("\n");
}

/* A position as callers hand it: 48 bits at +2, the integer at +4.  +0x60
 * reads the dword at +0, so the whole 8 bytes are filled. */
static void lat_band(uint32_t lat, uint32_t level)
{
    uint32_t pos[2], r, o;

    pos[0] = lat;
    pos[1] = rnd();
    r = R.lat_band((uint32_t)(uintptr_t)pos, level);
    o = O.lat_band((uint32_t)(uintptr_t)pos, level);
    compared++;
    if (r != o)
        miss("lat_band", lat, level, &r, &o, 4);
}

static void lon_band(uint32_t pos_lo, uint32_t pos_hi, uint32_t level)
{
    uint32_t pos[2] = { pos_lo, pos_hi }, r, o;

    r = R.lon_band((uint32_t)(uintptr_t)pos, level);
    o = O.lon_band((uint32_t)(uintptr_t)pos, level);
    compared++;
    if (r != o)
        miss("lon_band", pos_lo, level, &r, &o, 4);
}

static void edge(int lat, uint32_t band, uint32_t level)
{
    uint8_t r[8], o[8];
    uint32_t fill = rnd();

    memset(r, (uint8_t)fill, sizeof r);
    memset(o, (uint8_t)fill, sizeof o);
    if (lat) {
        R.band_lat(band, level, (uint32_t)(uintptr_t)r);
        O.band_lat(band, level, (uint32_t)(uintptr_t)o);
    } else {
        R.band_lon(band, level, (uint32_t)(uintptr_t)r);
        O.band_lon(band, level, (uint32_t)(uintptr_t)o);
    }
    compared++;
    if (memcmp(r, o, sizeof r))
        miss(lat ? "band_lat" : "band_lon", band, level, r, o, 8);
}

/* +0x64 indexes the table by |band| >> (level - 8); keep that inside it. */
static int band_in_table(uint32_t band, uint32_t level)
{
    uint16_t ax = (uint16_t)band, i;
    unsigned cl = (uint8_t)(level - 8) & 31;

    if ((int16_t)ax < 0)
        ax = (uint16_t)(0u - ax);
    i = cl >= 16 ? 0 : (uint16_t)(ax >> cl);
    return i <= 256;
}

static void edge_lat(uint32_t band, uint32_t level)
{
    if (band_in_table(band, level))
        edge(1, band, level);
}

/* A fault in either side ends the run as a difference: retail does not
 * fault inside the table, so one that does is a defect, and wine's debugger
 * would otherwise sit waiting. */
static LONG CALLBACK on_fault(EXCEPTION_POINTERS *x)
{
    if (x->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        x->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        printf("FAULT 0x%08lx at %p after %llu comparisons\n",
               x->ExceptionRecord->ExceptionCode, x->ExceptionRecord->ExceptionAddress,
               compared);
        fflush(stdout);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    long nrand = argc > 1 ? atol(argv[1]) : 2000000;
    uint32_t level, k, top;
    long n;

    AddVectoredExceptionHandler(1, on_fault);
    load(&R, "FE_retail.dll", "retail");
    load(&O, "FE_ours.dll", "ours");
    if (memcmp(R.bands, O.bands, 0x400)) {
        printf("band tables differ\n");
        return 1;
    }
    top = *(uint32_t *)(R.bands + 0x3fc) >> 7;

    /* every table edge, either side, and halfway on, at levels 0..24 */
    for (level = 0; level <= 24; level++) {
        for (k = 0; k < 256; k++) {
            uint32_t e = *(uint32_t *)(R.bands + 4 * k) >> 7;
            uint32_t next = k < 255 ? *(uint32_t *)(R.bands + 4 * k + 4) >> 7 : e;
            int32_t d;
            for (d = -2; d <= 2; d++) {
                lat_band(e + d, level);
                lat_band(0u - (e + d), level);
            }
            lat_band(e + (next - e) / 2, level);
            lat_band(0u - (e + (next - e) / 2), level);
        }
        lat_band(0, level);
        lat_band(0xffffffffu, level);
        for (k = 0; k < 0x10000; k += (k < 0x400 ? 1 : 0x3f)) {
            edge_lat(k, level);
            edge(0, k, level);
            lon_band(rnd(), k << 16, level);
        }
    }

    for (n = 0; n < nrand; n++) {
        uint32_t lat = rnd() % (top + 1);
        uint32_t lv = rnd() % 25;
        if (rnd() & 1)
            lat = 0u - lat;
        if ((n & 63) == 0)
            lv = rnd() & 0xffff;
        lat_band(lat, lv);
        edge_lat(rnd() & 0xffff, rnd() % 25);
        edge(0, rnd() & 0xffff, (n & 15) ? rnd() % 25 : rnd() & 0xffff);
        lon_band(rnd(), rnd(), (n & 15) ? rnd() % 25 : rnd() & 0xffff);
    }

    printf("FE grid: %llu comparisons, %llu mismatches\n", compared, mismatches);
    return mismatches != 0;
}
