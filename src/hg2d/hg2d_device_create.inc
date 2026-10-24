/* 0x2078c030: bind a surface, create the selected device and viewport, and
 * initialize caps, formats, render states and palette. */
#include "hg2d_device_guids.inc"

static uintptr_t device_method(uint32_t instance, uint32_t method)
{
    uint32_t *object = POINTER(instance), *vtable = POINTER(object[0]);
    return vtable[method];
}

uint32_t STDCALL __attribute__((used)) hg2d_device_create(uint32_t surface)
{
    uint32_t temporary, result, instance, flags, index;
    uint32_t host_info[58], description[27] = {0}, software[51] = {0};
    uint32_t viewport[11] = {0x2c, 0, 0, 0x800, 0x800, 0xbf800000, 0x40000000,
                            0x40000000, 0x3f800000, 0, 0x3f800000};
    if (surface == WORD(0x28940)) {
        device_textures_clear();
        return 1;
    }
    result = ((call_three)device_method(surface, 0))(surface, ADDRESS(device_guids[4]), ADDRESS(&temporary));
    if (result) goto failed;
    result = ((call_two)device_method(temporary, 36))(temporary, ADDRESS(hg2d_image + 0x289f0));
    if (temporary) ((call_one)device_method(temporary, 2))(temporary);
    if (result) goto failed;
    instance = WORD(0x289f0);
    result = ((call_three)device_method(instance, 0))(instance, ADDRESS(device_guids[3]), ADDRESS(hg2d_image + 0x28cc0));
    if (result) goto failed;
    WORD(0x28940) = surface;
    ((call_one)device_method(surface, 1))(surface);
    (*(call_three *)((uintptr_t)WORD(4) + 0x31c))(1, ADDRESS(host_info), 0xe8);
    /* Each comparison is independent; no matching name leaves the existing
     * device pointer and the successful QueryInterface result untouched. */
    for (index = 0; index < 3; index++) {
        static const uint32_t names[] = {0x5ed0, 0x5ec0, 0x5eb4};
        if (!lstrcmpA((const char *)host_info + 0x80, (const char *)hg2d_image + names[index])) {
            instance = WORD(0x28cc0);
            result = ((call_four)device_method(instance, 8))(instance, ADDRESS(device_guids[2 - index]),
                WORD(0x28940), ADDRESS(hg2d_image + 0x289d0));
        }
    }
    if (result || !WORD(0x289d0)) goto failed;
    instance = WORD(0x28cc0);
    result = ((call_three)device_method(instance, 6))(instance, ADDRESS(hg2d_image + 0x28ccc), 0);
    if (result) goto failed;
    instance = WORD(0x289d0);
    result = ((call_two)device_method(instance, 6))(instance, WORD(0x28ccc));
    if (result) goto failed;
    description[0] = 0x6c;
    instance = WORD(0x28940);
    ((call_two)device_method(instance, 22))(instance, ADDRESS(description));
    viewport[4] = description[2]; viewport[3] = description[3];
    instance = WORD(0x28ccc);
    result = ((call_two)device_method(instance, 17))(instance, ADDRESS(viewport));
    if (result) goto failed;
    instance = WORD(0x289d0);
    result = ((call_two)device_method(instance, 13))(instance, WORD(0x28ccc));
    if (result) goto failed;
    __builtin_memset(hg2d_image + 0x28a00, 0, 0xcc);
    WORD(0x28a00) = software[0] = 0xcc;
    WORD(0x28cc8) = 0;
    instance = WORD(0x289d0);
    ((call_three)device_method(instance, 3))(instance, ADDRESS(hg2d_image + 0x28a00), ADDRESS(software));
    if (!WORD(0x28a04)) {
        WORD(0x28cc8) = 1;
        __builtin_memcpy(hg2d_image + 0x28a00, software, 0xcc);
        if (!WORD(0x28a04)) goto failed;
    }
    flags = host_info[0xd0 / 4];
    if (!hg2d_device_select_format(flags)) goto failed;
    hg2d_device_mipmap(flags);
    hg2d_device_filter(flags);
    hg2d_device_states(flags);
    hg2d_device_blend();
    hg2d_device_fog(flags);
    device_palette_realize();
    return 1;
failed:
    hg2d_device_shutdown();
    return 0;
}
