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

static uint64_t gauge_block_hash(const struct blk *block, int scale_fields);

static uint64_t gauge_heap_hash(const struct blk *block)
{
    const struct host_list *panels = (const struct host_list *)((const uint8_t *)cur->imports + 0x16020);
    const uint8_t *bytes = arena + block->off;
    if (block->kind != K_HEAP)
        return hash_words(bytes, block->size, 0);
    /* Panel-window records contain four float scale factors at +20..2c.
     * Their raw bits can equal an address in another rebuilt gauge. */
    return gauge_block_hash(block, bytes == panels->data);
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

/* value as a pointer into one gauge instance's logical data window or paired callbacks: its offset in the
 * retail layout (the window at 0x3000, a callback at its retail RVA); 0 when it is neither. */
static int gauge_instance_decode(const struct gauge *gauge, uint32_t value, uint32_t *offset)
{
    const struct gauge_spec *spec = &gauge_specs[gauge->gauge_kind];
    unsigned callback;
    if (value >= gauge->wlo && value < gauge->whi) {
        *offset = value - gauge->wlo + 0x3000;
        return 1;
    }
    if (value < gauge->gauge_text_lo || value >= gauge->gauge_text_hi)
        return 0;
    for (callback = 0; callback < spec->ncode; callback++)
        if (value == gauge->gauge_code[callback]) {
            *offset = spec->retail_code[callback];
            return 1;
        }
    return 0;
}

static int gauge_pointer(uint32_t value, uint32_t *offset)
{
    unsigned index;
    /* gp_live_only: only loaded instances claim an address (norm); otherwise retained pointers after unload
     * pair too, the latest instance first when the loader reuses an address (put_ptr). */
    for (index = gauge_count; index > 0; index--) {
        const struct gauge *gauge = gauge_instances[index - 1];
        if (gp_live_only && !gauge->loaded)
            continue;
        if (gauge_instance_decode(gauge, value, offset))
            return gauge->region;
    }
    return -1;
}


/* ---- heap words: who owns the address they hold --------------------------------------------------
 * A heap word is compared by what it points at, in this order, each step keeping what it found as a
 * class of its own so that a raw word, a live pointer, a retained one and a DIB row never share a value:
 *   1 a live rebuilt-gauge instance (data window, paired callbacks)       (kind, region, logical offset)
 *   2 a live region (PANELS's data and text, a live gauge image)           (region, byte offset)
 *   3 a live DIB, exact bounds                                             (DIB id, byte offset)
 *   4 the latest unloaded gauge image holding the address: a rebuilt gauge decoded as in 1; any other gauge
 *     only in the proven place (the pointer list at PANELS's .data +0x16000; elsewhere a deleted DIB or exact)
 *                                                                          (load ordinal, gauge name, offset)
 *   5 a deleted DIB, exact bounds                                          (DIB id, byte offset)
 *   6 anything else, exactly.
 * Wine puts a rebuilt gauge at the base a retail one prefers, so the same load sits at different addresses
 * on the two sides, and only 1-5 pair them; a numeric word keeps its bits. */
struct gauge_image {
    uint32_t lo, hi;
    unsigned region;
    int live;
    uint32_t name_hash;         /* the logical gauge (the requested path's file name, upper case) */
};
static struct gauge_image gauge_images[MAXGAUGE];
static unsigned gauge_image_count;

static void gauge_images_reset(void)
{
    gauge_image_count = 0;
}

static void gauge_image_loaded(uint32_t lo, uint32_t hi, unsigned region, const char *path)
{
    const char *name = strrchr(path, '\\');
    uint32_t h = 0x811c9dc5u;
    if (gauge_image_count >= MAXGAUGE)
        return;
    for (name = name ? name + 1 : path; *name; name++)
        h = (h ^ (uint32_t)(unsigned char)(*name >= 'a' && *name <= 'z' ? *name - 32 : *name)) * 16777619u;
    gauge_images[gauge_image_count].lo = lo;
    gauge_images[gauge_image_count].hi = hi;
    gauge_images[gauge_image_count].region = region;
    gauge_images[gauge_image_count].live = 1;
    gauge_images[gauge_image_count++].name_hash = h;
}

static void gauge_image_unloaded(unsigned region)
{
    unsigned i;
    for (i = 0; i < gauge_image_count; i++)
        if (gauge_images[i].region == region)
            gauge_images[i].live = 0;
}

enum { KEY_EXACT, KEY_LIVE_GAUGE, KEY_REGION, KEY_TEXT, KEY_DIB, KEY_STALE_GAUGE, KEY_STALE_IMAGE, KEY_DEAD_DIB, KEY_SCALE };

static uint64_t gauge_key(unsigned class, uint32_t a, uint32_t b, uint32_t c)
{
    uint64_t h = 0xcbf29ce484222325ull;
    h = fnv(h, class);
    h = fnv(h, a);
    h = fnv(h, b);
    return fnv(h, c);
}

/* v as the DIB bits it points into, by exact bounds: its index, and whether it is still live */
static int gauge_dib_owner(uint32_t v, uint32_t *offset, int *live)
{
    unsigned k;
    if ((v & 3) || !dib_page)
        return -1;
    k = dib_page[v >> 12];
    if (!k || v < dibs[k - 1].lo || v >= dibs[k - 1].hi)
        return -1;
    *offset = v - dibs[k - 1].lo;
    *live = dibs[k - 1].live;
    return (int)k - 1;
}

/* the proven place: the data of the host list at PANELS's .data +0x16000, an array of pointers to the loaded
 * gauges' data (elem size 4), where an entry for a gauge since unloaded is kept (docs/implementation/
 * spec-1-4z-*.md: scenarios 6, 20 and 28 hold it at +0x4c of its 0x80-byte data) */
static int gauge_stale_field(const struct blk *block, uint32_t offset)
{
    (void)offset;
    return block->origin && block->origin - (uint32_t)(uintptr_t)cur->imports == 0x16000;
}

static uint64_t gauge_word_key(const struct blk *block, uint32_t offset, uint32_t v)
{
    unsigned index;
    uint32_t off;
    int r, d, live;

    if (v < 0x100000u)
        return gauge_key(KEY_EXACT, v, 0, 0);
    for (index = gauge_count; index > 0; index--) {
        const struct gauge *gauge = gauge_instances[index - 1];
        if (gauge->loaded && gauge_instance_decode(gauge, v, &off))
            return gauge_key(KEY_LIVE_GAUGE, gauge->gauge_kind, gauge->region, off);
    }
    r = region_of(v);
    if (!(v & 3) || r == 1 || r == 3) {
        if (r == 1 || r == 3)
            return gauge_key(KEY_TEXT, 0, 0, 0);
        if (r >= 0)
            return gauge_key(KEY_REGION, r == 2 ? 0 : (uint32_t)r, v - regions[r].lo, 0);
        if ((d = gauge_dib_owner(v, &off, &live)) >= 0 && live)
            return gauge_key(KEY_DIB, (uint32_t)d, off, 0);
    }
    for (index = gauge_image_count; index > 0; index--) {
        const struct gauge_image *image = &gauge_images[index - 1];
        unsigned instance;
        if (image->live || v < image->lo || v >= image->hi)
            continue;
        /* the latest load owning the address, chosen before its offset is read; a word outside the proven place
         * does not take its meaning from an older load either: it falls to a deleted DIB, or stays exact */
        if (!gauge_stale_field(block, offset))
            break;
        for (instance = gauge_count; instance > 0; instance--) {
            const struct gauge *gauge = gauge_instances[instance - 1];
            if (gauge->region == image->region) {
                if (gauge_instance_decode(gauge, v, &off))
                    return gauge_key(KEY_STALE_GAUGE, gauge->gauge_kind, gauge->region, off);
                return gauge_key(KEY_EXACT, v, 0, 0);
            }
        }
        return gauge_key(KEY_STALE_IMAGE, index - 1, image->name_hash, v - image->lo);
    }
    if ((d = gauge_dib_owner(v, &off, &live)) >= 0)
        return gauge_key(KEY_DEAD_DIB, (uint32_t)d, off, 0);
    return gauge_key(KEY_EXACT, v, 0, 0);
}

/* A heap block as it compares: each word's key (class and identity), then the tail bytes. scale_fields: the
 * panel-window records' four float scale factors at +0x20..0x2c, which compare as they are. */
static uint64_t gauge_block_hash(const struct blk *block, int scale_fields)
{
    const uint8_t *bytes = arena + block->off;
    uint64_t hash = 0xcbf29ce484222325ull;
    uint32_t offset;
    for (offset = 0; offset + 4 <= block->size; offset += 4) {
        uint32_t value, field = offset % 0x50;
        uint64_t key;
        memcpy(&value, bytes + offset, 4);
        key = scale_fields && field >= 0x20 && field <= 0x2c ? gauge_key(KEY_SCALE, value, 0, 0)
                                                              : gauge_word_key(block, offset, value);
        hash = fnv(hash, (uint32_t)key);
        hash = fnv(hash, (uint32_t)(key >> 32));
    }
    for (; offset < block->size; offset++)
        hash = fnv(hash, bytes[offset]);
    return hash;
}

/* ---- the rule's checks, run at every start ----------------------------------------------------------
 * Each case builds two sides' states through the harness's own registration paths (gauge_image_loaded and
 * _unloaded as h_module_load and _release call them, the DIB hooks, arena_get, regions[]) and compares the
 * production heap-block hash of the record at the proven site. Pairs marked "same" must compare equal (so the
 * checks are not vacuous); every other pair is a real difference and must compare different. The harness
 * stops with exit 2 on the first that does not. */
static unsigned st_nregions;

static void st_side(void)
{
    gdi_reset();
    arena_reset();
    gauge_count = 0;
    nregions = st_nregions;
}

static void st_load(uint32_t lo, uint32_t hi, const char *path, int unload)
{
    unsigned region = nregions++;
    regions[region].lo = lo;
    regions[region].hi = hi;
    regions[region].live = 1;
    gauge_image_loaded(lo, hi, region, path);
    if (unload) {
        regions[region].live = 0;
        gauge_image_unloaded(region);
    }
}

static uint32_t st_dib(void)
{
    BITMAPINFO bmi;
    void *bits = NULL;
    memset(&bmi, 0, sizeof bmi);
    bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
    bmi.bmiHeader.biWidth = 64;
    bmi.bmiHeader.biHeight = 8;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    hk_CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    return (uint32_t)(uintptr_t)bits;
}

/* the hash of a 0x80-byte block whose word at +0x4c is word, allocated as the proven list's data (gated) or not */
static uint64_t st_hash(uint32_t word, int gated)
{
    uint8_t *p;
    cur_origin = gated ? (uint32_t)(uintptr_t)cur->imports + 0x16000 : 0;
    p = arena_get(0x80, K_HEAP);
    memcpy(p + 0x4c, &word, 4);
    return gauge_block_hash(&blks[nblks - 1], 0);
}


/* ---- the same rule through the harness's own paths ----------------------------------------------------
 * A gauge is loaded by h_module_load and released by h_module_release (the hooks the PANELS imports reach), a
 * pointer into it is kept in the data h_list_add allocates for PANELS's list at .data +0x16000, and what two
 * sides compare is the "blocks=" hash put_state writes. The second side has the gauge's preferred base reserved
 * so that Wine relocates it, as it does a retail gauge when a rebuilt one sits on its base. */
static int32_t STDCALL h_module_load(uint32_t *slot, const char *path, uint32_t ext);
static int32_t STDCALL h_module_release(uint32_t *slot);
static void put_state(void);
static void select_side(struct side *s);
static void world_reset(const struct scenario *s);
static void free_gauges(void);

static struct scenario st_scenario;

/* what a side compares of one heap block holding a pointer into a gauge it has unloaded: in the proven list's
 * data (in_list) or in a block of its own; reserve: the gauge's preferred base is taken first; base_out: where
 * it loaded; offset: the word's offset in the image. Returns the blocks= token. */
static const char *st_real(struct side *side, struct side *load_side, const char *path, int in_list, uint32_t offset,
                           uint32_t reserve, uint32_t *base_out, int data_window)
{
    static char token[64];
    struct host_list *list;
    struct host_list saved;
    uint32_t slot = 0, word;
    void *block = NULL, *held = NULL;
    const char *at;

    select_side(side);
    memcpy(side->imports, side->pristine, IMAGE_SIZE);
    free_gauges();
    gdi_reset();
    nregions = 4;
    tlen = 0;
    world_reset(&st_scenario);
    if (reserve)
        held = VirtualAlloc((void *)(uintptr_t)reserve, 0x200000, MEM_RESERVE, PAGE_NOACCESS);
    cur = load_side;
    if (h_module_load(&slot, path, 0x10) < 0) {
        fprintf(stderr, "stale-pointer self-test: cannot load %s\n", path);
        ExitProcess(2);
    }
    cur = side;
    *base_out = (uint32_t)(uintptr_t)gauges[ngauges - 1].h;
    word = data_window ? gauges[ngauges - 1].wlo + offset : *base_out + offset;
    h_module_release(&slot);
    if (held)
        VirtualFree(held, 0, MEM_RELEASE);
    if (in_list) {
        list = (struct host_list *)((uint8_t *)cur->imports + 0x16000);
        saved = *list;
        list->count = list->capacity = 0;
        list->grow = 32;
        list->elem_size = 4;
        list->data = NULL;
        h_list_add(list);
        memcpy(list->data + 0x4c, &word, 4);
        tlen = 0;
        put_state();
        *list = saved;
    } else {
        h_alloc(0, 0x80, &block, 1);
        memcpy((uint8_t *)block + 0x4c, &word, 4);
        tlen = 0;
        put_state();
    }
    at = tlog ? strstr(tlog, "blocks=") : NULL;
    snprintf(token, sizeof token, "%.40s", at ? at : "none");
    if (strchr(token, ' '))
        *strchr(token, ' ') = 0;
    free_gauges();
    return token;
}

static void gauge_stale_selftest_real(int *bad)
{
    char retail[300], rebuilt[300];
    char one[64], two[64];
    uint32_t base0, base1;
    const char *name = gauge_specs[0].name;
    size_t n = strlen(name);
#define RCASE(what, same) do { if ((strcmp(one, two) == 0) != (same)) { fprintf(stderr, "stale-pointer self-test failed: %s\n", what); *bad = 1; } } while (0)
    snprintf(retail, sizeof retail, "..\\..\\game\\Gauges\\737-400.Oil");
    snprintf(rebuilt, sizeof rebuilt, "..\\..\\game\\Gauges\\%.*s", (int)(n > 4 ? n - 4 : n), name);

    /* (all-gauges-panels has no retail-only gauge: every one is recognised) */
    if (gauge_kind("737-400.Oil.GAU") < 0) {
        /* a retail-only gauge unloaded, its pointer kept in the proven list: the same gauge and offset pair across
         * a relocation, another offset does not */
        snprintf(one, sizeof one, "%s", st_real(&sides[0], &sides[0], retail, 1, 0x430, 0, &base0, 0));
        snprintf(two, sizeof two, "%s", st_real(&sides[1], &sides[0], retail, 1, 0x430, base0, &base1, 0));
        if (base0 == base1) { fprintf(stderr, "stale-pointer self-test: the gauge was not relocated\n"); *bad = 1; }
        RCASE("retained pointer through the list, hooks and put_state (same)", 1);
        snprintf(two, sizeof two, "%s", st_real(&sides[1], &sides[0], retail, 1, 0x434, base0, &base1, 0));
        RCASE("retained pointer through the list, changed offset", 0);
        /* the same words outside the list are numbers */
        snprintf(one, sizeof one, "%s", st_real(&sides[0], &sides[0], retail, 0, 0x430, 0, &base0, 0));
        snprintf(two, sizeof two, "%s", st_real(&sides[1], &sides[0], retail, 0, 0x430, base0, &base1, 0));
        RCASE("numeric word in a recorded image, outside the list", 0);
    }
    /* a rebuilt gauge (a recognised one): in the list its data pointer pairs, outside it it is a number */
    snprintf(one, sizeof one, "%s", st_real(&sides[1], &sides[1], rebuilt, 1, 0x10, 0, &base0, 1));
    snprintf(two, sizeof two, "%s", st_real(&sides[1], &sides[1], rebuilt, 1, 0x10, base0, &base1, 1));
    RCASE("rebuilt gauge's data pointer in the list (same)", 1);
    snprintf(one, sizeof one, "%s", st_real(&sides[1], &sides[1], rebuilt, 0, 0x10, 0, &base0, 1));
    snprintf(two, sizeof two, "%s", st_real(&sides[1], &sides[1], rebuilt, 0, 0x10, base0, &base1, 1));
    RCASE("numeric word in a recognised gauge's data, outside the list", 0);
#undef RCASE
}

static void gauge_stale_selftest(void)
{
    int bad = 0;
    uint64_t a, b;
    uint32_t dib;
    struct side *saved = cur;
#define CASE(what, same) do { if (((a == b) != (same))) { fprintf(stderr, "stale-pointer self-test failed: %s\n", what); bad = 1; } } while (0)
    cur = &sides[0];
    st_nregions = nregions;

    /* a retained pointer into the same gauge at the same offset, the gauge at another address */
    st_side(); st_load(0x32000000, 0x3200e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); a = st_hash(0x32002430, 1);
    st_side(); st_load(0x13b80000, 0x13b8e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); b = st_hash(0x13b82430, 1);
    CASE("retained pointer, same gauge and offset (same)", 1);
    st_side(); st_load(0x13b80000, 0x13b8e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); b = st_hash(0x13b82434, 1);
    CASE("retained pointer, changed offset", 0);
    /* a changed resource: the same address, another gauge */
    st_side(); st_load(0x13b80000, 0x13b8e000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1); b = st_hash(0x13b82430, 1);
    CASE("retained pointer, changed gauge", 0);
    /* a numeric field whose bits fall in a recorded image: not the proven field */
    st_side(); st_load(0x32000000, 0x3200e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); a = st_hash(0x32002430, 0);
    st_side(); st_load(0x13b80000, 0x13b8e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); b = st_hash(0x13b82430, 0);
    CASE("numeric field inside a recorded image, outside the proven place", 0);
    /* a raw word that equals what a mapped pointer could encode is not one */
    st_side(); a = st_hash(0xa0002430, 1);
    st_side(); st_load(0x13b80000, 0x13b8e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); b = st_hash(0x13b82430, 1);
    CASE("raw word against a mapped pointer", 0);
    /* a changed live owner: loaded on one side, unloaded on the other */
    st_side(); st_load(0x32000000, 0x3200e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); a = st_hash(0x32002430, 1);
    st_side(); st_load(0x32000000, 0x3200e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 0); b = st_hash(0x32002430, 1);
    CASE("changed live owner", 0);
    /* address reuse: the later load of the address owns it */
    st_side(); st_load(0x32000000, 0x3200e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1); a = st_hash(0x32000100, 1);
    st_side(); st_load(0x32000000, 0x3200e000, "..\\..\\game\\Gauges\\737-400.Engine-Start.GAU", 1);
    st_load(0x32000000, 0x32020000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1); b = st_hash(0x32000100, 1);
    CASE("address reuse", 0);
    /* DIB rows: the same DIB and row on both sides pair, another row does not, and a live DIB over an
     * unloaded gauge's image owns the address */
    st_side(); dib = st_dib(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1);
    a = st_hash(dib + 0x40, 1);
    st_side(); dib = st_dib(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1);
    b = st_hash(dib + 0x40, 1);
    CASE("same DIB row (same)", 1);
    st_side(); dib = st_dib(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1);
    b = st_hash(dib + 0x44, 1);
    CASE("changed DIB row offset", 0);
    st_side(); dib = st_dib(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1);
    b = st_hash(dib + 0x40, 0);
    CASE("live DIB row compares by DIB and offset in any block (same)", 1);
    st_side(); st_load(0x30000000, 0x30040000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1); b = st_hash(0x30000040, 1);
    st_side(); dib = st_dib(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1);
    a = st_hash(dib + 0x40, 1);
    CASE("live DIB over an unloaded gauge, against the gauge alone", 0);
    /* a deleted DIB does not claim an address an unloaded gauge's image holds: the gauge pairs as usual */
    st_side(); dib = st_dib(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1);
    hk_DeleteObject((HGDIOBJ)(uintptr_t)dibs[0].handle); a = st_hash(dib + 0x40, 1);
    st_side(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1); b = st_hash(dib + 0x40, 1);
    CASE("deleted DIB under an unloaded gauge (same)", 1);
    /* a deleted DIB row outside the proven place: an unloaded image over its address on one side only must not
     * turn it into a raw address there */
    st_side(); dib = st_dib(); st_load(dib & ~0xfffu, (dib & ~0xfffu) + 0x4000, "..\\..\\game\\Gauges\\737-400.Oil.GAU", 1);
    hk_DeleteObject((HGDIOBJ)(uintptr_t)dibs[0].handle); a = st_hash(dib + 0x40, 0);
    st_side(); dib = st_dib(); hk_DeleteObject((HGDIOBJ)(uintptr_t)dibs[0].handle); b = st_hash(dib + 0x40, 0);
    CASE("deleted DIB row outside the proven place, image over one side (same)", 1);
#undef CASE
    st_side();
    nregions = st_nregions;
    tlen = 0;
    cur = saved;
    gauge_stale_selftest_real(&bad);
    memset(gauge_loads, 0, sizeof gauge_loads);        /* the self-test's loads are not the run's coverage */
    if (bad)
        ExitProcess(2);
}
