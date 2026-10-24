/* Device mip filtering: format pairs, signed pitches, padding and aliases. */
static uint32_t device_reduce_entry[2];
static void device_reduce_check(uint32_t destination, uint32_t pitch, uint32_t format,
    uint32_t source, uint32_t source_pitch, uint32_t source_format, uint32_t width, uint32_t height, uint32_t key)
{
    uint32_t arguments[9] = {destination,pitch,format,source,source_pitch,source_format,width,height,key};
    touched = device_output; check_carry = 0;
    draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
    invoke_draw(device_reduce_entry[side], arguments, 9, 0xaabbccdd);
    draw_active = 0; event(draw_fault_code); compare(0);
    CHECK(!draw_fault_code);
    CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
}

static void run_device_reduce(void)
{
    static const uint32_t formats[] = {8,0x22b,0x235,0x1bc,32};
    static const uint32_t widths[] = {0,1,2,3,7,16,31,255,0xffffffff};
    static const uint32_t heights[] = {0,1,2,3,7,0xffffffff};
    static const uint32_t keys[] = {0,1,127,152,255,256,0x12340098,0xffffffff};
    uint32_t destination_format, source_format, variant, index, opaque;
    palette_reset();
    for (index = 0; index < 65536; index++) { device_source[index] = (uint8_t)random_word(); device_output[index] = (uint8_t)(index + scenario); }
    for (index = 0; index < 768; index++) images[side][0x39c2 + index] = (uint8_t)random_word();
    for (destination_format = 0; destination_format < 5; destination_format++)
        for (source_format = 0; source_format < 5; source_format++) for (variant = 0; variant < 10; variant++) {
            int32_t pitch = 1031 + (int32_t)(scenario % 5), source_pitch = 1033 + (int32_t)(scenario % 3);
            if (variant & 1) pitch = -pitch;
            if (variant & 2) source_pitch = -source_pitch;
            device_reduce_check(ADDRESS(device_output + 32768 + (variant & 3)), (uint32_t)pitch, formats[destination_format],
                ADDRESS(device_source + 32768), (uint32_t)source_pitch, formats[source_format],
                widths[(scenario + variant) % 9], heights[(scenario + variant * 3) % 6], keys[(scenario + variant) % 8]);
        }
    images[side][0x39c2] = images[side][0x39c5] = 255;
    images[side][0x39c3] = images[side][0x39c6] = 128;
    images[side][0x39c4] = images[side][0x39c7] = 64;
    memset(device_source, 0, 16);
    device_reduce_check(ADDRESS(device_output), 4, 0x22b, ADDRESS(device_source), 2, 8, 2, 2, 0);
    CHECK(HALF(device_output, 0) == 0xfe08);
    device_reduce_check(ADDRESS(device_output), 4, 0x235, ADDRESS(device_source), 2, 8, 2, 2, 255);
    CHECK(HALF(device_output, 0) == 0xfc08);
    device_reduce_check(ADDRESS(device_output), 4, 0x1bc, ADDRESS(device_source), 2, 8, 2, 2, 0);
    CHECK(HALF(device_output, 0) == 0xff84);
    for (opaque = 0; opaque <= 4; opaque++) {
        for (index = 0; index < 4; index++) { device_source[index] = index < opaque ? 1 : 0; HALF(device_source, 16 + index * 2) = index < opaque ? 0x8000 : 0; }
        device_reduce_check(ADDRESS(device_output), 4, 0x22b, ADDRESS(device_source), 2, 8, 2, 2, 1);
        CHECK(HALF(device_output, 0) == (opaque >= 2 ? 0xfe08 : 0x7e08));
        device_reduce_check(ADDRESS(device_output), 4, 0x22b, ADDRESS(device_source + 16), 4, 0x22b, 2, 2, 0xffffffff);
        CHECK(HALF(device_output, 0) == (opaque == 4 ? 0x8000 : 0));
    }
    for (index = 0; index < 4; index++) HALF(device_source, index * 2) = 0x1234;
    device_reduce_check(ADDRESS(device_output), 4, 0x1bc, ADDRESS(device_source), 4, 0x1bc, 2, 2, 0xffffffff);
    CHECK(HALF(device_output, 0) == 0x1234);
    for (destination_format = 1; destination_format < 4; destination_format++) for (source_format = 0; source_format < 2; source_format++) {
        uint32_t input = source_format ? formats[destination_format] : 8;
        for (index = 0; index < 65536; index++) device_output[index] = (uint8_t)(index * 29 + scenario);
        device_reduce_check(ADDRESS(device_output + 8192), 17, formats[destination_format], ADDRESS(device_output + 8193), 33, input, 13, 5, scenario);
        for (index = 0; index < 768; index++) images[side][0x39c2 + index] = (uint8_t)(index * 37 + scenario);
        memset(device_source, 0, 64);
        device_reduce_check(ADDRESS(images[side] + 0x39c2), 7, formats[destination_format], ADDRESS(device_source), 9, input, 7, 3, 1);
    }
}
