/* Optional GAUGES_DIFF mode: same PANELS, retail/rebuilt gauges. Only the
 * logical data images and explicitly paired code pointers are normalized. */
struct gauge_spec {
    const char *name;
    uint32_t data_size, linkage;
    const uint32_t *code_offsets, *retail_code;
    unsigned ncode;
#ifdef GAUGES_ALL_DIFF
    const uint8_t *pointer_mask;
#endif
};
#ifdef GAUGES_ALL_DIFF
#include "all_panels.inc"
#else
static const uint32_t warning_offsets[] = {0x2c,0x30,0x34,0x38,0x3c,0x40,0x44,0xdc,0xe0};
static const uint32_t warning_rvas[] = {0x10b0,0x1020,0x1040,0x1050,0x1060,0x1080,0x1090,0x10a0,0x10a0};
static const uint32_t suction_offsets[] = {0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,0xf4,0x274,0x278};
static const uint32_t suction_rvas[] = {0x1140,0x1060,0x10b0,0x10c0,0x10d0,0x10f0,0x1100,0x1020,0x1130,0x1130};
static const struct gauge_spec gauge_specs[] = {
    {"737-400.WARNING_ENUNCIATORS.GAU",0xfc,0xd8,warning_offsets,warning_rvas,9},
    {"CESSNA_182.SUCTION.GAU",0x298,0x270,suction_offsets,suction_rvas,10}
};
#endif
static unsigned gauge_loads[sizeof gauge_specs / sizeof gauge_specs[0]][2];
static struct gauge *gauge_instances[MAXGAUGE];
static unsigned gauge_count;
#ifdef GAUGES_ALL_DIFF
static uint32_t retail_gauge_data[sizeof gauge_specs / sizeof gauge_specs[0]];
static uint32_t (__attribute__((stdcall)) *real_zoom[2])(uint32_t, uint32_t);
static unsigned canonical_zoom_calls[2];

static void gauge_text_slots(struct gauge *gauge)
{
    uint32_t pending[1024], seen[1024], header = gauge->linkage[6];
    unsigned count = 1, visited = 0;
    pending[0] = *(uint32_t *)(uintptr_t)*(uint32_t *)(uintptr_t)(header + 8);
    while (count) {
        uint32_t element = pending[--count], children;
        unsigned index;
        if (!element) continue;
        for (index = 0; index < visited; index++)
            if (seen[index] == element) break;
        if (index < visited) continue;
        if (visited == 1024 || element < gauge->wlo || element + 0x2c > gauge->whi)
            ExitProcess(2);
        seen[visited++] = element;
        if (*(uint32_t *)(uintptr_t)element == 2) {
            if (gauge->ntext == 128 || element + 0xbc > gauge->whi)
                ExitProcess(2);
            gauge->text_slots[gauge->ntext++] = element + 0xb8;
        }
        children = *(uint32_t *)(uintptr_t)(element + 0x28);
        if (children)
            for (index = 0; ; index++) {
                uint32_t child;
                if (children + 4 * index < gauge->wlo || children + 4 * index + 4 > gauge->whi)
                    ExitProcess(2);
                child = *(uint32_t *)(uintptr_t)(children + 4 * index);
                if (!child) break;
                if (count == 1024) ExitProcess(2);
                pending[count++] = child;
            }
    }
}

static int gauge_text_allocation(const void *slot)
{
    unsigned instance, index;
    for (instance = 0; instance < gauge_count; instance++) {
        const struct gauge *gauge = gauge_instances[instance];
        if (gauge->loaded)
            for (index = 0; index < gauge->ntext; index++)
                if ((uint32_t)(uintptr_t)slot == gauge->text_slots[index])
                    return 1;
    }
    return 0;
}

static uint32_t gauge_data_word(const struct gauge *gauge, uint32_t offset)
{
    uint32_t value = *(const uint32_t *)(uintptr_t)(gauge->wlo + offset);
    /* Numeric bits can coincide with another gauge's image base. Only
     * declared relocations, the module handle/import and element parents
     * are pointers; compare every other byte literally. */
    return gauge_specs[gauge->gauge_kind].pointer_mask[offset / 4] ? norm(value) : value;
}

static uint64_t gauge_data_hash(const struct gauge *gauge)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    uint32_t offset;
    for (offset = 0; offset < gauge->whi - gauge->wlo; offset += 4)
        hash = fnv(hash, gauge_data_word(gauge, offset));
    return hash;
}

static uint64_t gauge_heap_hash(const struct blk *block)
{
    const struct host_list *panels = (const struct host_list *)((const uint8_t *)cur->imports + 0x16020);
    const uint8_t *bytes = arena + block->off;
    uint64_t hash = 0xcbf29ce484222325ull;
    uint32_t offset;
    if (block->kind != K_HEAP || bytes != panels->data)
        return hash_words(bytes, block->size, block->kind == K_HEAP);
    /* Panel-window records contain four float scale factors at +20..2c.
     * Their raw bits can equal an address in another rebuilt gauge. */
    for (offset = 0; offset + 4 <= block->size; offset += 4) {
        uint32_t value, field = offset % 0x50;
        memcpy(&value, bytes + offset, 4);
        hash = fnv(hash, field >= 0x20 && field <= 0x2c ? value : norm(value));
    }
    return hash;
}

/* PANELS's documented 0x151 bug scales the MODULE_VAR's own address for
 * invalid view modes. Verify that raw result first, then compare it at the
 * original gauge's address, just as pointer fields are compared by offsets.
 * This fixture adapter does not alter either runtime DLL. */
static uint32_t __attribute__((stdcall)) gauge_canonical_zoom(uint32_t variable, uint32_t operation)
{
    unsigned side = cur == &sides[1], index;
    uint32_t result = real_zoom[side](variable, operation);
    uint32_t view = *(uint32_t *)(global_blob + 0x11a0);
    if (operation != 1 || !view || *(uint32_t *)(uintptr_t)(view + 0x10) == 1 ||
        *(uint32_t *)(uintptr_t)(view + 0x14) <= 4)
        return result;
    for (index = 0; index < gauge_count; index++) {
        const struct gauge *gauge = gauge_instances[index];
        if (gauge->loaded && variable >= gauge->wlo && variable < gauge->whi) {
            double actual, expected = (double)((long double)variable / 64.0L);
            uint32_t canonical = retail_gauge_data[gauge->gauge_kind] + variable - gauge->wlo;
            memcpy(&actual, (void *)(uintptr_t)(variable + 0xc), 8);
            if (actual != expected) {
                fprintf(stderr, "PANELS zoom address quirk changed: %.17g != %.17g\n", actual, expected);
                ExitProcess(2);
            }
            put(" zoom-address("); put_ptr(variable); put(")");
            actual = (double)((long double)canonical / 64.0L);
            memcpy((void *)(uintptr_t)(variable + 0xc), &actual, 8);
            canonical_zoom_calls[side]++;
            break;
        }
    }
    return result;
}

static void gauge_canonical_install(struct side *side, unsigned index)
{
    uint32_t *slot = (uint32_t *)((uint8_t *)side->imports + 0x608 + 16 * 0x151 + 0xc);
    real_zoom[index] = (uint32_t (__attribute__((stdcall)) *)(uint32_t, uint32_t))(uintptr_t)*slot;
    *slot = (uint32_t)(uintptr_t)gauge_canonical_zoom;
}
#endif

static int gauge_kind(const char *path)
{
    const char *name = strrchr(path, '\\');
    unsigned index;
    for (index = 0; index < sizeof gauge_specs / sizeof gauge_specs[0]; index++)
        if (stricmp(name ? name + 1 : path, gauge_specs[index].name) == 0)
            return (int)index;
    return -1;
}

static const char *gauge_replacement(const char *path)
{
    int kind = gauge_kind(path);
    const char *only = getenv("GAUGES_ONLY");
    if (kind >= 0 && only && !strstr(only, gauge_specs[kind].name))
        return NULL;
#ifdef GAUGES_ALL_DIFF
    static char replacement[300];
    if (kind < 0) return NULL;
    snprintf(replacement, sizeof replacement, "Gauges\\%s", gauge_specs[kind].name);
    return replacement;
#else
    return kind < 0 ? NULL : gauge_specs[kind].name;
#endif
}

static void gauge_loaded(struct gauge *gauge, const char *path)
{
    int kind = gauge_kind(path);
    const struct gauge_spec *spec;
    const uint8_t *data;
    unsigned index;
    if (kind < 0)
        return;
    spec = &gauge_specs[kind];
    data = (const uint8_t *)gauge->linkage - spec->linkage;
    gauge->gauge_kind = (unsigned)kind;
    gauge_instances[gauge_count++] = gauge;
    for (index = 0; index < spec->ncode; index++)
        gauge->gauge_code[index] = *(const uint32_t *)(data + spec->code_offsets[index]);
    find_sections(gauge->h, &gauge->gauge_text_lo, &gauge->gauge_text_hi, NULL, NULL, NULL, NULL);
    /* Exclude toolchain-specific IAT and section padding. */
    gauge->wlo = (uint32_t)(uintptr_t)data;
    gauge->whi = gauge->wlo + spec->data_size;
    gauge->wlo2 = gauge->whi2 = 0;
    gauge_loads[kind][cur == &sides[1]]++;
#ifdef GAUGES_ALL_DIFF
    gauge_text_slots(gauge);
    if (cur == &sides[0]) retail_gauge_data[kind] = gauge->wlo;
#endif
}

static int gauge_pointer(uint32_t value, uint32_t *offset)
{
    unsigned index, callback;
    /* Pair retained pointers after unload without reading unmapped memory;
     * prefer the latest instance when the loader reuses an address. */
    for (index = gauge_count; index > 0; index--) {
        const struct gauge *gauge = gauge_instances[index - 1];
        const struct gauge_spec *spec = &gauge_specs[gauge->gauge_kind];
        if (value >= gauge->wlo && value < gauge->whi) {
            *offset = value - gauge->wlo + 0x3000;
            return gauge->region;
        }
        if (value < gauge->gauge_text_lo || value >= gauge->gauge_text_hi)
            continue;
        for (callback = 0; callback < spec->ncode; callback++)
            if (value == gauge->gauge_code[callback]) {
                *offset = spec->retail_code[callback];
                return gauge->region;
            }
    }
    return -1;
}
