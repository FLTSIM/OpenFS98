/* Vertex ARGB: precedence, word gates, full-width overrides and live palette. */
static uint32_t device_colour_entry[2], device_colour_texture[13];
static void run_device_colour(void)
{
    static const uint32_t overrides[] = {0,1,3,27,28,29,0x80000000,0xffffffff};
    uint32_t variant, index;
    palette_reset();
    for (index = 0; index < 28; index++) WORD(images[side], 0x28960 + index * 4) = random_word();
    for (index = 0; index < 768; index++) images[side][0x39c2 + index] = (uint8_t)random_word();
    WORD(global, 0xc8c) = ADDRESS(device_colour_texture);
    for (variant = 0; variant < 384; variant++) {
        uint32_t gate = variant % 16, expected, colour_index;
        GLOBAL_HALF(0xf10) = 0; GLOBAL_HALF(0xdce) = 0; WORD(global, 0x398) = 0; GLOBAL_HALF(0x1172) = 0;
        GLOBAL_HALF(0xc90) = 0; GLOBAL_HALF(0xca0) = 0; GLOBAL_HALF(0xe82) = 0; device_colour_texture[2] = 0;
        GLOBAL_HALF(0xdca) = 0; GLOBAL_HALF(0xb34) = (scenario + variant) & 1 ? 0x7fff : 0x8000;
        WORD(global, 0x1298) = gate < 6 ? 0 : overrides[(scenario + variant / 16) % 8];
        WORD(global, 0x3c0) = gate & 1 ? 0xaabb68cc : 0x11226744;
        if (gate == 0 || gate >= 9) {
            GLOBAL_HALF(0xf10) = 0x100; WORD(global, 0x398) = 0x80000000; GLOBAL_HALF(0x1172) = 1;
            if (gate == 9) GLOBAL_HALF(0xf10) = 0;
            if (gate == 10) GLOBAL_HALF(0xdce) = 0x100;
            if (gate == 11) GLOBAL_HALF(0x1172) = 0x101;
            if (gate == 12) WORD(global, 0x398) = 0;
            if (gate == 13) GLOBAL_HALF(0x1172) = 0;
            if (gate >= 14) GLOBAL_HALF(0xf10) = 0;
        }
        if (gate == 1 || gate == 2 || gate >= 9) {
            GLOBAL_HALF(0xc90) = 0x100; GLOBAL_HALF(0xca0) = 0x98;
            if (gate == 2) GLOBAL_HALF(0xe82) = 0x100;
            device_colour_texture[2] = gate == 2 || (gate >= 9 && gate < 15) ? 0x80000000 : 0;
            if (gate == 14) GLOBAL_HALF(0xca0) = 0x198;
            if (gate == 15) GLOBAL_HALF(0xe82) = 0x100;
        }
        if (gate == 3) GLOBAL_HALF(0xdca) = 0x100;
        if (gate == 5) GLOBAL_HALF(0xdce) = variant & 16 ? 0x100 : 0;
        colour_index = (scenario + variant * 17) & 255; images[side][0x90c] = (uint8_t)colour_index;
        /* Independently stated constants verify the early precedence paths. */
        device_states_check(device_colour_entry[side], 0, 0, 0);
        expected = WORD(images[side], 0x5f38);
        if (gate == 0) CHECK(expected == 0xeeffffff);
        if (gate == 1 || (gate >= 9 && gate < 14)) CHECK(expected == 0xccffffff);
        if (gate == 2) CHECK(expected == 0xffffffff);
        if (gate == 3) CHECK(expected == (GLOBAL_HALF(0xb34) < 0x8000 ? 0x80000000u : 0x80ffffffu));
        if (gate == 4) CHECK(expected == ((uint32_t)images[side][0x39c2 + colour_index * 3] << 16 |
            (uint32_t)images[side][0x39c3 + colour_index * 3] << 8 | images[side][0x39c4 + colour_index * 3]));
    }
}
