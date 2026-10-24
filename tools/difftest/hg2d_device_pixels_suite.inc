/* Internal device helpers: descriptors, packed pixels, pitches and aliases. */
static uint32_t device_format_entry[2], device_convert_entry[2];
static uint8_t device_source[0x10000], device_output[0x10000];

static uint32_t device_format_check(uint32_t *format)
{
    uint32_t result = invoke(device_format_entry[side], ADDRESS(format), 0, 0, 0xaabbccdd, 1);
    touched = NULL; check_carry = 0;
    compare(result);
    return result;
}

static void device_convert_check(uint32_t destination, uint32_t pitch, uint32_t format, uint32_t source,
                                  uint32_t source_pitch, uint32_t width, uint32_t height, uint32_t key)
{
    uint32_t arguments[9] = {destination,pitch,format,source,source_pitch,0x12345678,width,height,key};
    draw_fault_code = draw_fault_address = draw_fault_access = 0;
    touched = device_output; check_carry = 0; draw_active = 1;
    invoke_draw(device_convert_entry[side], arguments, 9, 0xaabbccdd);
    draw_active = 0;
    event(draw_fault_code); event(draw_fault_access); event(draw_fault_address);
    compare(0); /* The converter has no scalar return contract. */
    CHECK(!draw_fault_code);
    CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
}

static void run_device_pixels(void)
{
    static const uint32_t formats[] = {8,0x22b,0x235,0x1bc,32};
    static const uint32_t descriptors[][8] = {
        {32,0x20,0,8,0,0,0,0}, {32,0,0,16,0x7c00,0x3e0,0x1f,0},
        {32,0,0,16,0xf800,0x7e0,0x1f,0}, {32,0,0,16,0xf00,0xf0,0xf,0},
        {32,0,0,32,0xff0000,0xff00,0xff,0},
    };
    static const uint32_t widths[] = {0,1,2,3,7,16,31,255,256};
    static const uint32_t heights[] = {0,1,2,7,16};
    static const uint32_t keys[] = {0,1,151,152,153,255,256,0x12340098,0x80000098,0xffffffff};
    uint32_t index, format, variant, descriptor[8];
    palette_reset();
    for (format = 0; format < 5; format++) {
        memcpy(descriptor, descriptors[format], sizeof descriptor);
        CHECK(device_format_check(descriptor) == formats[format]);
        for (index = 1; index < 8; index++) {
            memcpy(descriptor, descriptors[format], sizeof descriptor);
            descriptor[index] ^= 1u << ((scenario + index) & 31);
            device_format_check(descriptor);
        }
    }
    for (variant = 0; variant < 24; variant++) {
        for (index = 0; index < 8; index++) descriptor[index] = random_word();
        if (variant & 1) descriptor[3] = 8u << (variant % 3);
        device_format_check(descriptor);
    }
    for (index = 0; index < sizeof device_source; index++) {
        device_source[index] = (uint8_t)(index + scenario); device_output[index] = (uint8_t)(index * 19 + scenario);
    }
    for (index = 0; index < 0x300; index++) images[side][0x39c2 + index] = (uint8_t)random_word();
    for (format = 0; format < 5; format++) for (variant = 0; variant < 20; variant++) {
        int32_t destination_pitch = 1031 + (int32_t)(scenario % 5), source_pitch = 263;
        uint32_t width = widths[(scenario + variant) % 9], height = heights[(scenario + variant * 3) % 5];
        if (variant & 1) destination_pitch = -destination_pitch;
        if (variant & 2) source_pitch = -source_pitch;
        device_convert_check(ADDRESS(device_output + 32768 + (variant & 3)), (uint32_t)destination_pitch,
            formats[format], ADDRESS(device_source + 32768), (uint32_t)source_pitch, width, height, keys[(scenario + variant) % 10]);
    }
    /* Directed channel packing and the 0x98 threshold exception. */
    images[side][0x39c2] = 255; images[side][0x39c3] = 128; images[side][0x39c4] = 64;
    device_source[0] = 0;
    device_convert_check(ADDRESS(device_output), 4, 0x22b, ADDRESS(device_source), 1, 1, 1, 1);
    CHECK(HALF(device_output, 0) == 0x7e08);
    device_convert_check(ADDRESS(device_output), 4, 0x22b, ADDRESS(device_source), 1, 1, 1, 0);
    CHECK(HALF(device_output, 0) == 0xfe08);
    device_convert_check(ADDRESS(device_output), 4, 0x235, ADDRESS(device_source), 1, 1, 1, 255);
    CHECK(HALF(device_output, 0) == 0xfc08);
    device_convert_check(ADDRESS(device_output), 4, 0x1bc, ADDRESS(device_source), 1, 1, 1, 0);
    CHECK(HALF(device_output, 0) == 0xff84);
    device_convert_check(ADDRESS(device_output), 4, 32, ADDRESS(device_source), 1, 1, 1, 0);
    CHECK(WORD(device_output, 0) == 0xffff8040);
    device_source[0] = 0; device_source[1] = 151; device_source[2] = 152; device_source[3] = 255;
    device_convert_check(ADDRESS(device_output), 4, 8, ADDRESS(device_source), 4, 4, 1, 152);
    CHECK(!memcmp(device_output, "\x96\x96\x98\xff", 4));
    device_convert_check(ADDRESS(device_output), 4, 8, ADDRESS(device_source), 4, 4, 1, 0x12340098);
    CHECK(!memcmp(device_output, "\x96\x96\x96\x96", 4));
    device_convert_check(ADDRESS(device_output), 4, 8, ADDRESS(device_source), 4, 4, 1, 0x80000098);
    CHECK(!memcmp(device_output, "\x96\x96\x96\x96", 4));
    for (format = 0; format < 5; format++) {
        for (index = 0; index < sizeof device_output; index++) device_output[index] = (uint8_t)(index * 29 + scenario);
        device_convert_check(ADDRESS(device_output + 8192), 80, formats[format], ADDRESS(device_output + 8193), 40, 13, 4, scenario);
        /* Output aliases the palette: the 32-bit path stores blue before
         * reading green/red; the 16-bit paths fetch all three before store. */
        for (index = 0; index < 0x300; index++) images[side][0x39c2 + index] = (uint8_t)(index * 37 + scenario);
        memset(device_source, 0, 32);
        device_convert_check(ADDRESS(images[side] + 0x39c2), 7, formats[format], ADDRESS(device_source), 8, 8, 2, 1);
    }
    device_convert_check(ADDRESS(device_output), 4, 0, ADDRESS(device_source), 4, 4, 1, 0);
    device_convert_check(ADDRESS(device_output), 4, 8, ADDRESS(device_source), 4, 0xffffffff, 1, 0);
    device_convert_check(ADDRESS(device_output), 4, 32, ADDRESS(device_source), 4, 1, 0xffffffff, 0);
}
