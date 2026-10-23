/* Software palettes and colour encoding, included by hg2ddiff.c. */
#define GLOBAL_HALF(offset) (*(uint16_t *)(global + (offset)))
#define HALF(image, offset) (*(uint16_t *)((image) + (offset)))
static uint32_t chain[0x24 / 4];

static uint32_t STDCALL chain_send(uint32_t list, uint32_t message, uint32_t argument)
{
    CHECK(list == 0x14 && message == 0x70 && argument == 0);
    event(5); event(list); event(message); event(argument);
    notifications++;
    return 0x12345678;
}

static uint32_t STDCALL host_realize(uint8_t *current, uint8_t *full)
{
    uint32_t index;
    CHECK(ADDRESS(current) == ADDRESS(images[side] + 0x39c2));
    CHECK(ADDRESS(full) == ADDRESS(images[side] + 0x3cc2));
    event(6);
    for (index = 0; index < 0x300; index++) {
        event(current[index]);
        event(full[index]);
    }
    palette_calls++;
    return 0x76543210;
}

static void palette_reset(void)
{
    uint32_t index;
    memcpy(images[side], initial[side], DATA_SIZE);
    memset(global, 0, sizeof global);
    random_state = scenario * 2654435761u + 0x12345;
    if (!random_state) random_state = 1;
    polygon_compare = 0;
    step = check_carry = 0;
    touched = NULL;
    event_hash = 2166136261u;
    WORD(images[side], 4) = ADDRESS(host);
    WORD(images[side], 0x0c) = ADDRESS(global);
    WORD(images[side], 0x14) = ADDRESS(chain);
    WORD(images[side], 0x24) = util_linkage;
    chain[0x20 / 4] = ADDRESS(chain_send);
    for (index = 0x30a0; index < 0x3fc2; index++)
        images[side][index] = (uint8_t)random_word();
    for (index = 0; index < 16; index++)
        images[side][0x3fee + index] = (uint8_t)random_word();
    /* Output guards and unaligned destinations live in unused image space. */
    memset(images[side] + 0x28000, 0x5a, 32);
}

static void layout(uint16_t level)
{
    uint32_t result;
    GLOBAL_HALF(0xcc0) = level;
    touched = NULL;
    check_carry = 0;
    result = invoke(layout_entry[side], 0, 0, 0, 0xaabbccdd, 0);
    compare(result);
    CHECK(HALF(images[side], 0x34c0) != HALF(images[side], 0x34c2));
}

static void run_palette(void)
{
    static const uint8_t brightnesses[] = {0, 1, 0x7f, 0x80, 0xfe, 0xff};
    static const uint16_t counts[] = {0, 1, 63, 64, 65, 0xffff};
    uint32_t index, result, colour, pair, output = ADDRESS(images[side] + 0x28003);
    uint16_t level;
    palette_reset();
    level = (uint16_t)(scenario % 42);
    layout(level);
    /* Includes early return, repeated brightness, forced zero, changed mode. */
    global[0x374] = (uint8_t)(scenario % 3);
    global[0x280] = (uint8_t)random_word();
    GLOBAL_HALF(0xc46) = scenario & 1 ? 0x20 : 0;
    WORD(global, 0x398) = random_word();
    WORD(global, 0x390) = scenario % 4;
    GLOBAL_HALF(0xcbc) = counts[scenario % 6];
    GLOBAL_HALF(0x12a0) = (uint16_t)random_word();
    images[side][0x400d] = (uint8_t)random_word();
    images[side][0x400c] = (uint8_t)(scenario % 3);
    WORD(images[side], 0x4011) = random_word();
    for (index = 0; index < sizeof brightnesses; index++) {
        uint8_t brightness = brightnesses[index];
        GLOBAL_HALF(0xcba) = (uint16_t)((brightness << 8) | (random_word() & 0xff));
        global[0xb6a] = (uint8_t)(index % 3);
        result = call(0x60, 0, 0, 0);
        (void)result;
        call(0x60, 0, 0, 0);
        CHECK(!(returned_flags & 1));
        CHECK(images[side][0x4004] == brightness);
        call(0x64, 0, 0, 0);
        CHECK(HALF(images[side], 0x36c0) == 1);
    }
    /* Packed realization must preserve untouched bytes in both output palettes. */
    GLOBAL_HALF(0xcbc) = (uint16_t)(scenario % 65);
    GLOBAL_HALF(0xcbe) = (uint16_t)(scenario % 3);
    call(0x5c, 0, 0, 0);
    CHECK(GLOBAL_HALF(0xcbe) == 0 && HALF(images[side], 0x36c0) == 0);
    call(0x7c, 0x12340000 | (scenario & 1), 0, 0);
    call(0x7c, 0xffff0000 | (scenario & 1), 0, 0);
    call(0x5c, 0, 0, 0);
    /* Arbitrary colour words are legal in the conversion API; they also
     * prove +0x88 uses byte lookup rather than the whole palette word. */
    for (index = 0; index < 256; index++)
        HALF(images[side], 0x34c0 + index * 2) = (uint16_t)random_word();
    /* High palette bytes index the inverse maps; keep these writes below
     * the unrelated pointer tables while exercising values above 255. */
    GLOBAL_HALF(0xcba) = 0x7f00;
    global[0x374] = 0;
    global[0xb6a] = 0;
    GLOBAL_HALF(0xc46) = 0;
    images[side][0x400d] = 0;
    for (index = 0; index < 256; index++)
        HALF(images[side], 0x34c0 + index * 2) = (uint16_t)(random_word() & 0x7ff);
    call(0x64, 0, 0, 0);
    for (index = 0; index < 256; index++)
        HALF(images[side], 0x34c0 + index * 2) = (uint16_t)random_word();
    for (index = 0; index < 16; index++) {
        colour = random_word();
        pair = random_word();
        call(0x70, colour, pair, output);
        result = call(0x88, colour, output, 0);
        CHECK((result & 0xffff0000u) == 0xaabb0000u);
    }
}

static void run_colour_vectors(void)
{
    uint32_t batch, index, value, pair;
    uint32_t output;
    /* Constants independently check the preserved carry in partial doubling,
     * the 0x1800 cutoff, the two output words, and +0x88's return width. */
    active_suite = "colour fixtures";
    scenario = 0x300000;
    for (side = 0; side < 2; side++) {
        palette_reset();
        for (index = 0; index < 256; index++)
            HALF(images[side], 0x34c0 + index * 2) = (uint16_t)index;
        output = ADDRESS(images[side] + 0x28003);
        call(0x70, 0xe03b, 0xa1b2, output);
        CHECK(HALF(images[side], 0x28003) == 0x3b && HALF(images[side], 0x28005) == 0xa1b2);
        images[side][0x3433] = 250;
        images[side][0x3434] = 5;
        images[side][0x3435] = 0;
        images[side][0x3400] = 50;
        images[side][0x3401] = 100;
        call(0x70, 0xf011, 0x8000, output);
        CHECK(HALF(images[side], 0x28003) == 0 && HALF(images[side], 0x28005) == 0x05fa);
        call(0x70, 0xf011, 0x1800, output);
        CHECK(HALF(images[side], 0x28003) == 55);
        call(0x70, 0xf011, 0x1801, output);
        CHECK(HALF(images[side], 0x28003) == 251);
        CHECK(call(0x88, 0x1234abcd, output, 0) == 0xaabbabcdu);
        /* Output aliasing the lookup tables must preserve read-before-write. */
        call(0x70, 0xf011, 0x8000, ADDRESS(images[side] + 0x3433));
        call(0x88, 0x1111, ADDRESS(images[side] + 0x34c0 + 0x11 * 2), 0);
    }
    active_suite = "colours";
    operations = 0;
    /* Every encoded 16-bit colour, then all 256 gradients at all 256
     * interpolation fractions, and the zero-third-byte cutoff boundaries. */
    for (batch = 0; batch < 512 + 6; batch++) {
        scenario = 0x100000 + batch;
        for (side = 0; side < 2; side++) {
            palette_reset();
            for (index = 0; index < 256; index++) {
                if (batch < 256) {
                    value = batch * 256 + index;
                    pair = random_word();
                } else if (batch < 512) {
                    value = 0xf000 | (batch - 256);
                    pair = index << 8;
                } else {
                    static const uint16_t boundaries[] = {0x17ff, 0x1800, 0x1801};
                    value = 0xf000 | index;
                    pair = boundaries[(batch - 512) % 3];
                    images[side][0x3402 + index * 3] = batch < 515 ? 0 : 1;
                }
                call(0x70, value, pair, ADDRESS(images[side] + 0x28003));

            }
        }
    }
    for (batch = 0; batch < 256; batch++) {
        scenario = 0x200000 + batch;
        for (side = 0; side < 2; side++) {
            palette_reset();
            for (index = 0; index < 256; index++)
                call(0x88, batch * 256 + index, ADDRESS(images[side] + 0x28003), 0);
        }
    }
    /* Layout's finite 16-bit domain can be checked in full, independently of
     * palette_pack's smaller valid range (which excludes its divide faults). */
    active_suite = "palette layout";
    for (batch = 0; batch < 256; batch++) {
        scenario = 0x400000 + batch;
        for (side = 0; side < 2; side++) {
            palette_reset();
            for (index = 0; index < 256; index++)
                layout((uint16_t)(batch * 256 + index));
        }
    }
    printf("hg2ddiff colours: %u exhaustive/boundary operations, 0 differ\n", operations / 2);
}
