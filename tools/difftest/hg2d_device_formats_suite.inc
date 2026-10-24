/* Formats are enumerated in both orders, with exact descriptor selection. */
static uint32_t device_formats_entry[2], device_enum_reverse, device_enum_count, device_enum_result;
static uint32_t device_enum_surfaces[16][27], device_enum_calls;
static uint32_t STDCALL device_enumerate_formats(uint32_t instance, uint32_t callback, uint32_t context)
{
    uint32_t *request = (uint32_t *)(uintptr_t)context;
    CHECK(instance == ADDRESS(device_objects[1]));
    event(45); event(request[0]); event(request[1]);
    for (uint32_t index = 2; index < 10; index++) event(request[index]);
    for (uint32_t index = 0; index < device_enum_count; index++) {
        uint32_t at = device_enum_reverse ? device_enum_count - index - 1 : index;
        uint32_t result = ((uint32_t (STDCALL *)(uint32_t *, uint32_t *))(uintptr_t)callback)(device_enum_surfaces[at], request);
        event(result);
        for (uint32_t field = 0; field < 10; field++) event(request[field]);
        if (!result) break;
    }
    device_enum_calls++;
    return device_enum_result;
}

static uint32_t device_formats_check(uint32_t bits, uint32_t alpha)
{
    uint32_t *out = (uint32_t *)(images[side] + 0x28000);
    uint32_t arguments[4] = {ADDRESS(device_objects[1]),bits,alpha,ADDRESS(out)}, result;
    for (uint32_t index = 0; index < 8; index++) out[index] = 0xaabbccdd;
    touched = NULL; check_carry = 0;
    draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
    result = invoke_draw(device_formats_entry[side], arguments, 4, 0xaabbccdd);
    draw_active = 0; event(draw_fault_code); compare(result);
    CHECK(!draw_fault_code);
    CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
    return result;
}

static void run_device_formats(void)
{
    static const uint32_t formats[][8] = {
        {32,0x20,0,8,0,0,0,0}, {32,0x40,0,16,0x7c00,0x3e0,0x1f,0},
        {32,0x41,0,16,0x7c00,0x3e0,0x1f,0x8000},
        {32,0x40,0,16,0xf800,0x7e0,0x1f,0},
        {32,0x40,0,16,0xf00,0xf0,0xf,0},
        {32,0x41,0,16,0xf00,0xf0,0xf,0xf000},
        {32,0x40,0,32,0xff0000,0xff00,0xff,0},
        {32,0x41,0,32,0xff0000,0xff00,0xff,0xff000000},
    };
    static const uint32_t depths[] = {0,8,12,15,16,24,32,33,0x22b,0x15af,0x235,0x1bc,0x115c,0x378,0x22b8,0xffffffff};
    static const uint32_t alphas[] = {0,1,4,8,9,0xffffffff};
    uint32_t variant, index, reverse, result;
    palette_reset(); device_enum_calls = 0;
    memset(device_vtables[1], 0, sizeof device_vtables[1]);
    device_objects[1][0] = ADDRESS(device_vtables[1]); device_vtables[1][9] = ADDRESS(device_enumerate_formats);
    for (index = 0; index < 16; index++) {
        for (uint32_t field = 0; field < 27; field++) device_enum_surfaces[index][field] = random_word();
        memcpy(device_enum_surfaces[index] + 18, formats[index % 8], 32);
        /* Equal candidates keep the earlier descriptor, including fields
         * ignored by the matcher. The serial makes the choice observable. */
        device_enum_surfaces[index][20] = index;
    }
    device_enum_result = 0x88760000 | scenario;
    for (reverse = 0; reverse < 2; reverse++) for (variant = 0; variant < 96; variant++) {
        device_enum_reverse = reverse;
        device_enum_count = 16;
        WORD(images[side], 0x28a84) = (scenario + variant / 6) & 1 ? 8 : 0;
        device_formats_check(depths[variant / 6], alphas[variant % 6]);
    }
    /* Every recognized format alone, then its invalid mask/flag/alpha
     * variants, including the bpp=8 exact-match rule and failed HRESULT. */
    for (variant = 0; variant < 8; variant++) {
        memcpy(device_enum_surfaces[0] + 18, formats[variant], 32);
        device_enum_reverse = 0; device_enum_count = 1;
        WORD(images[side], 0x28a84) = 8;
        result = device_formats_check(variant ? (variant >= 6 ? 24 : variant == 3 ? 16 : variant >= 4 ? 12 : 15) : 8, 0);
        if (!result) break;
        CHECK(!memcmp(images[side] + 0x28000, formats[variant], 32));
        device_enum_surfaces[0][19] = 0;
        CHECK(!device_formats_check(8, 0));
        device_enum_surfaces[0][19] = formats[variant][1];
        device_enum_surfaces[0][21] ^= 1;
        CHECK(!device_formats_check(0, 0));
        memcpy(device_enum_surfaces[0] + 18, formats[variant], 32);
        device_enum_surfaces[0][22] ^= 1;
        CHECK(device_formats_check(0, 0) == (variant == 0 ? 1u : 0u));
    }
    for (variant = 0; variant < 48; variant++) {
        static const uint32_t flags[] = {0,8,0x20,0x28,0x40,0x48,0x60,0x68};
        memcpy(device_enum_surfaces[0] + 18, formats[(scenario + variant / 8) % 8], 32);
        device_enum_surfaces[0][19] = flags[variant % 8];
        device_formats_check(depths[(scenario + variant) % 16], alphas[variant % 6]);
    }
    device_enum_count = 0;
    CHECK(!device_formats_check(16, 1));
    for (index = 0; index < 8; index++) CHECK(!WORD(images[side], 0x28000 + index * 4));
    CHECK(device_enum_calls == 273);
}
