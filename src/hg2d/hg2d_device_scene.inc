/* Device selection, scene begin/end, and lazy Linkage cache switching. */
uint32_t STDCALL __attribute__((used)) hg2d_device_dispatch(uint32_t table)
{
    uint32_t result = WORD(0x28c00), index;
    if (!result) {
        call_three move = *(call_three *)((uintptr_t)WORD(4) + 0x21c);
        move(ADDRESS(hg2d_image + 0x28c00), ADDRESS(hg2d_image + 0x38), 0xc0);
        result = move(ADDRESS(hg2d_image + 0x28880), ADDRESS(hg2d_image + 0x38), 0xc0);
        WORD(0x28880 + 0x1c) = ADDRESS(hg2d_device_point);
        WORD(0x28880 + 0x20) = ADDRESS(hg2d_device_nop);
        WORD(0x28880 + 0x24) = ADDRESS(hg2d_device_depth_point);
        WORD(0x28880 + 0x28) = ADDRESS(hg2d_device_text);
        WORD(0x28880 + 0x2c) = ADDRESS(hg2d_device_horizontal);
        WORD(0x28880 + 0x30) = ADDRESS(hg2d_device_vertical);
        WORD(0x28880 + 0x34) = ADDRESS(hg2d_device_line);
        WORD(0x28880 + 0x38) = ADDRESS(hg2d_device_fixed_line);
        WORD(0x28880 + 0x3c) = ADDRESS(hg2d_device_frame);
        WORD(0x28880 + 0x40) = ADDRESS(hg2d_device_commit);
        WORD(0x28880 + 0x44) = ADDRESS(hg2d_device_projected_vertex);
        WORD(0x28880 + 0x48) = ADDRESS(hg2d_device_uv_vertex);
        WORD(0x28880 + 0x4c) = ADDRESS(hg2d_device_commit);
        WORD(0x28880 + 0x50) = ADDRESS(hg2d_device_intensity_vertex);
        WORD(0x28880 + 0x54) = ADDRESS(hg2d_device_commit);
        WORD(0x28880 + 0x74) = ADDRESS(hg2d_device_palette_vertex);
        WORD(0x28880 + 0x78) = ADDRESS(hg2d_device_commit);
        WORD(0x28880 + 0x84) = ADDRESS(hg2d_device_disc);
    }
    if (WORD(0x38 + 0x40) != ((uint32_t *)POINTER(table))[0x40 / 4])
        for (index = 0; index < 0xc0; index += 4) WORD(0x38 + index) = *(uint32_t *)POINTER(table + index);
    return result;
}

static uint32_t device_half(uint32_t value)
{
    return (uint32_t)((int32_t)(value - ((int32_t)value < 0 ? 0xffffffffu : 0)) >> 1);
}

uint32_t STDCALL __attribute__((used)) hg2d_device_begin(uint32_t *surface)
{
    if ((surface[2] & 0x4000) && WORD(0x28940) && surface[0x9c / 4] == WORD(0x28940)) {
        uint32_t instance = WORD(0x289d0), *object = POINTER(instance), *vtable = POINTER(object[0]), *view;
        WORD(0x5f20) = ((call_one)(uintptr_t)vtable[10])(instance);
        if (WORD(0x5f20)) return 0;
        GLOBAL_WORD(0xd48) |= 0x4000;
        GLOBAL_WORD(0x12a4) = (GLOBAL_WORD(0x12a4) & ~2u) | 0x19;
        hg2d_device_dispatch(ADDRESS(hg2d_image + 0x28880));
        view = POINTER(surface[7]);
        view[0] = device_half(surface[0x60 / 4]); view[2] = 0u - device_half(surface[0x64 / 4]);
        view[1] = device_half(surface[0x60 / 4]); view[3] = device_half(surface[0x64 / 4]);
        if ((GLOBAL_WORD(0x12a4) & 0x100) && !(surface[2] & 0x20000000) &&
            (int32_t)surface[0x30 / 4] >= 0 && (int32_t)surface[0x34 / 4] >= 0) {
            view[1] += surface[0x30 / 4]; view[3] += surface[0x34 / 4];
        }
        if (WORD(0x28860)) device_fog_colour();
    } else {
        GLOBAL_WORD(0xd48) &= ~0x4000u;
        GLOBAL_WORD(0x12a4) = (GLOBAL_WORD(0x12a4) & ~0x19u) | 2;
        hg2d_device_dispatch(ADDRESS(hg2d_image + 0x28c00));
    }
    return 0;
}

uint32_t STDCALL __attribute__((used)) hg2d_device_end(uint32_t *surface)
{
    if ((surface[2] & 0x4000) && surface[0x9c / 4] == WORD(0x28940) && (GLOBAL_WORD(0x12a4) & 1)) {
        uint32_t instance, *object, *vtable;
        hg2d_device_flush();
        instance = WORD(0x289d0); object = POINTER(instance); vtable = POINTER(object[0]);
        WORD(0x5f20) = ((call_one)(uintptr_t)vtable[11])(instance);
        GLOBAL_WORD(0xd48) &= ~0x4000u;
        GLOBAL_WORD(0x12a4) = (GLOBAL_WORD(0x12a4) & ~0x19u) | 2;
        hg2d_device_dispatch(ADDRESS(hg2d_image + 0x28c00));
    }
    return 0;
}

uint32_t STDCALL __attribute__((used)) hg2d_device_select(uint32_t *surface, const uint32_t *descriptor)
{
    uint32_t previous = GLOBAL_WORD(0x12a4), result = 0;
    GLOBAL_WORD(0x12a4) &= ~0x1fu; surface[2] &= ~0xc000u;
    if (!descriptor) { hg2d_device_shutdown(); return 1; }
    GLOBAL_WORD(0x12a4) = 0;
    if (descriptor[7]) {
        uint32_t description[27], *object = POINTER(descriptor[7]), *vtable = POINTER(object[0]);
        description[0] = 0x6c;
        ((call_two)(uintptr_t)vtable[22])(descriptor[7], ADDRESS(description));
        if (hg2d_device_create(descriptor[7])) {
            surface[2] |= 0x4000;
            if (description[21] > 8) surface[2] |= 0x8000;
            if (description[26] & 0x10) GLOBAL_WORD(0x12a4) |= 0x100;
            GLOBAL_WORD(0x12a4) |= 0x80;
            GLOBAL_WORD(0x12a4) = (GLOBAL_WORD(0x12a4) & ~2u) | 4;
            result = 2;
        } else if (description[21] == 8) result = 1;
    } else if (descriptor[0] == 1) result = 1;
    if (result == 1) {
        hg2d_device_dispatch(ADDRESS(hg2d_image + 0x28c00));
        GLOBAL_WORD(0x12a4) = (GLOBAL_WORD(0x12a4) & ~4u) | 2;
    }
    if (result && ((GLOBAL_WORD(0x12a4) ^ previous) & 0x80)) GLOBAL[0x444] |= 4;
    return result;
}
