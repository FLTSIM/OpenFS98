/* Locked mip chains and exact pixel buffers, with release/lock order. */
#define UPLOAD_BYTES 0x50000
static uint32_t device_upload_entry[2], upload_objects[4], upload_vtables[4][33];
static uint8_t upload_pixels[4][UPLOAD_BYTES], upload_saved[24][4][UPLOAD_BYTES];
static uint32_t upload_format, upload_start, upload_count, upload_fail, upload_self, upload_negative;
static uint32_t upload_locks[4], upload_describes, upload_releases[4], upload_levels[4], upload_caps[4];

static uint32_t upload_tag(uint32_t instance)
{
    for (uint32_t index = 0; index < 4; index++) if (instance == ADDRESS(upload_objects + index)) return index;
    CHECK(0); return 0;
}

static void upload_description(uint32_t *description, uint32_t tag)
{
    static const uint32_t formats[][8] = {
        {32,0x20,0,8,0,0,0,0}, {32,0x41,0,16,0x7c00,0x3e0,0x1f,0x8000},
        {32,0x40,0,16,0xf800,0x7e0,0x1f,0}, {32,0x41,0,16,0xf00,0xf0,0xf,0xf000},
        {32,0x41,0,32,0xff0000,0xff00,0xff,0xff000000},
    };
    uint32_t width = 256 >> upload_levels[tag], bytes = upload_format == 0 ? 1 : upload_format == 4 ? 4 : 2;
    int32_t pitch = (int32_t)(width * bytes + (width == 256 ? 0 : 3));
    memset(description, 0, 108); description[0] = 108;
    description[2] = description[3] = width;
    if (upload_negative) pitch = -pitch;
    description[4] = (uint32_t)pitch;
    description[9] = ADDRESS(upload_pixels[tag] + (upload_negative ? 0x41000 - (uint32_t)-pitch : 0x1000));
    memcpy(description + 18, formats[upload_format], 32); description[26] = upload_caps[tag];
}

static uint32_t STDCALL upload_describe(uint32_t instance, uint32_t *description)
{
    uint32_t tag = upload_tag(instance);
    event(52); event(tag); event(description[0]); upload_describes++;
    upload_description(description, tag); return 0x8876aaaa;
}

static uint32_t STDCALL upload_lock(uint32_t instance, uint32_t rectangle, uint32_t *description, uint32_t flags, uint32_t handle)
{
    uint32_t tag = upload_tag(instance);
    event(53); event(tag); event(rectangle); event(flags); event(handle); event(description[0]);
    upload_locks[tag]++; event(upload_locks[tag]);
    upload_description(description, tag);
    /* Previous-level lock HRESULT is deliberately ignored by retail. */
    if (upload_locks[tag] > 1) return 0x8876bbbb;
    return upload_fail == tag + 1 ? 0x88760001 + tag : 0;
}

static uint32_t STDCALL upload_unlock(uint32_t instance, uint32_t rectangle)
{
    event(54); event(upload_tag(instance)); event(rectangle); return 0x8876cccc;
}

static uint32_t STDCALL upload_attached(uint32_t instance, const uint32_t *caps, uint32_t *out)
{
    uint32_t tag = upload_tag(instance);
    event(55); event(tag); event(*caps); event(*out);
    if (tag + 1 < upload_count) *out = ADDRESS(upload_objects + tag + 1);
    else if (upload_self) *out = instance;
    return 0x8876dddd;
}

static uint32_t STDCALL upload_release(uint32_t instance)
{
    uint32_t tag = upload_tag(instance);
    event(56); event(tag); event(upload_locks[tag]); upload_releases[tag]++;
    return 17;
}

static void upload_compare_pixels(uint32_t variant)
{
    for (uint32_t index = 0; index < 4; index++) {
        if (!side) memcpy(upload_saved[variant][index], upload_pixels[index], UPLOAD_BYTES);
        else if (memcmp(upload_saved[variant][index], upload_pixels[index], UPLOAD_BYTES)) {
            uint32_t at = 0;
            while (upload_saved[variant][index][at] == upload_pixels[index][at]) at++;
            printf("device_upload differs: scenario %u operation %u surface %u pixel +%x, %02x != %02x\n",
                scenario, variant, index, at, upload_saved[variant][index][at], upload_pixels[index][at]);
            exit(1);
        }
    }
}

/* Every generated source/pixel pattern has period 256. Grow the exact
 * first period by copies instead of emitting millions of scalar stores. */
static void upload_seed(uint8_t *pixels, uint32_t size, uint32_t multiplier, uint32_t seed)
{
    uint32_t filled, index;
    for (index = 0; index < 256 && index < size; index++) pixels[index] = (uint8_t)(index * multiplier + seed);
    for (filled = index; filled < size;) {
        uint32_t amount = size - filled < filled ? size - filled : filled;
        memcpy(pixels + filled, pixels, amount); filled += amount;
    }
}

static void upload_objects_init(void)
{
    static uint32_t seed_checked;
    static uint8_t seed_probe[UPLOAD_BYTES];
    if (!seed_checked) {
        for (uint32_t multiplier = 1; multiplier <= 17; multiplier += 16)
            for (uint32_t seed = 0; seed <= 255; seed += 255) {
                upload_seed(seed_probe, sizeof seed_probe, multiplier, seed);
                for (uint32_t index = 0; index < sizeof seed_probe; index++)
                    CHECK(seed_probe[index] == (uint8_t)(index * multiplier + seed));
            }
        seed_checked = 1;
    }
    for (uint32_t index = 0; index < 4; index++) {
        memset(upload_vtables[index], 0, sizeof upload_vtables[index]);
        upload_objects[index] = ADDRESS(upload_vtables[index]);
        upload_vtables[index][2] = ADDRESS(upload_release); upload_vtables[index][22] = ADDRESS(upload_describe);
        upload_vtables[index][25] = ADDRESS(upload_lock); upload_vtables[index][32] = ADDRESS(upload_unlock);
        upload_vtables[index][12] = ADDRESS(upload_attached);
    }
}

static void run_device_upload(void)
{
    static const uint32_t keys[] = {0,1,25,26,152,255,256,0xffffffff};
    uint32_t variant, index, surface, pool = POOL + (scenario % 200) * 0x34;
    palette_reset();
    upload_objects_init();
    for (index = 0; index < 768; index++) images[side][0x39c2 + index] = (uint8_t)random_word();
    for (variant = 0; variant < 24; variant++) {
        upload_format = (scenario + variant) % 5; upload_start = (scenario + variant / 5) % 4;
        upload_count = (scenario + variant / 4) % 4 + 1;
        for (surface = 0; surface < 4; surface++) { upload_levels[surface] = upload_start + surface; upload_caps[surface] = 0; }
        upload_fail = variant < 16 ? 0 : (variant - 16) % 5;
        upload_negative = (scenario + variant) & 1; upload_self = variant & 1;
        upload_describes = 0; memset(upload_locks, 0, sizeof upload_locks); memset(upload_releases, 0, sizeof upload_releases);
        upload_seed(mip_build_heap, sizeof mip_build_heap, 17, scenario + variant);
        for (surface = 0; surface < 4; surface++) upload_seed(upload_pixels[surface], UPLOAD_BYTES, 1, surface + variant);
        memset(images[side] + pool, 0, 0x34);
        WORD(images[side], pool) = ADDRESS(mip_build_heap); WORD(images[side], pool + 4) = 0x40000;
        WORD(images[side], pool + 24) = keys[(scenario + variant) % 8];
        {
            uint32_t arguments[3] = {ADDRESS(upload_objects), ADDRESS(images[side] + pool), upload_start};
            touched = NULL; check_carry = 0;
            draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
            invoke_draw(device_upload_entry[side], arguments, 3, 0xaabbccdd);
            draw_active = 0; event(draw_fault_code); compare(0);
            CHECK(!draw_fault_code);
            CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
        }
        upload_compare_pixels(variant);
        CHECK(upload_describes == 1);
        CHECK(upload_locks[0] >= 1);
    }
}
