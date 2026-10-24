/* Host growth, the entire 96 KB allocation, cached levels and fallback. */
static uint32_t mip_grow_fail, mip_grow_calls;

static uint32_t STDCALL mip_grow(uint32_t kind, uint32_t size, uint32_t *out)
{
    CHECK(kind == 4 && size == 0x18000);
    event(37); event(kind); event(size); event(ADDRESS(out) - ADDRESS(images[side]));
    mip_grow_calls++;
    if (mip_grow_fail) return 1;
    memcpy(mip_build_heap, (void *)(uintptr_t)*out, 0x10000);
    *out = ADDRESS(mip_build_heap);
    return 0;
}

static void run_projective_mip_build(void)
{
    uint32_t variant, index, pass, selector, *texture, expected;
    matrix_reset();
    mip_build_compare = 1;
    host[0x210 / 4] = ADDRESS(mip_grow);
    for (variant = 0; variant < 16; variant++) {
        uint32_t mode = variant & 3;
        mip_grow_fail = variant >= 4 && variant < 8;
        mip_grow_calls = 0;
        projective_fog_mode = mode & 1; projective_dither_mode = (mode >> 1) & 1;
        texture = (uint32_t *)(images[side] + POOL + (variant ? scenario % 200 : 199) * 0x34 + (scenario & 1));
        if (variant >= 12) texture = matrix_texture;
        memset(texture, 0, 13 * 4);
        texture[0] = ADDRESS(matrix_pixels);
        texture[1] = variant >= 8 && variant < 12 ? 0x60000 : variant ? 0x20000 : 0xa0000;
        for (index = 0; index < sizeof matrix_pixels; index++) matrix_pixels[index] = (uint8_t)(index * 13 + (index >> 8) * 17 + scenario);
        matrix_pixels[0] = 0; matrix_pixels[1] = 0; matrix_pixels[256] = 8; matrix_pixels[257] = 8;
        matrix_pixels[2] = 0; matrix_pixels[3] = 4; matrix_pixels[258] = 4; matrix_pixels[259] = 8;
        memset(mip_build_heap, 0xa5, sizeof mip_build_heap);
        if (variant >= 8 && variant < 12) {
            memcpy(mip_build_heap, matrix_pixels, sizeof matrix_pixels);
            texture[0] = ADDRESS(mip_build_heap);
        }
        for (index = 0; index < 256; index++) {
            images[side][0x39c2 + index * 3] = (uint8_t)index;
            images[side][0x39c3 + index * 3] = (uint8_t)(variant ? index * 37 + scenario : index);
            images[side][0x39c4 + index * 3] = (uint8_t)(variant ? index * 73 + variant * 11 : index);
        }
        for (pass = 0; pass < 2; pass++) {
            /* matrix_case uses the external record; save it for invalid handles. */
            uint32_t saved_texture[13];
            memcpy(saved_texture, texture, sizeof saved_texture);
            projective_case((scenario + variant) & 3, 0);
            memcpy(texture, saved_texture, sizeof saved_texture);
            GLOBAL_HALF(0xcb4) = 2; GLOBAL_HALF(0x1172) = GLOBAL_HALF(0x324) = 1;
            GLOBAL_HALF(0xcce) = projective_dither_mode;
            WORD(global, 0xc8c) = ADDRESS(texture);
            projective_point(4, 4); projective_point(84, 4); projective_point(84, 7); projective_point(4, 7);
            selector = ((scenario + variant) & 3) | (mode & 1 ? 4 : 0) | (mode & 2 ? 32 : 0);
            if (!mip_grow_fail && variant < 12) selector |= 16;
            expected = WORD(images[side], 0x4204 + selector * 4);
            polygon_compare = 1;
            drawing(0x40, draw_surface(), 0, 0, 0, 0, 0);
            CHECK(!draw_fault_code && WORD(images[side], 0x4200) == expected);
            CHECK(mip_grow_calls == (variant >= 8 ? 0u : mip_grow_fail ? pass + 1 : 1));
            if (variant < 4) CHECK(texture[1] & 0x40000);
            if (!variant) CHECK(mip_build_heap[0x10000] == 0 && mip_build_heap[0x10001] == 4);
        }
    }
    mip_build_compare = projective_fog_mode = projective_dither_mode = polygon_compare = 0;
}
