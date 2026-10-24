/* 0x2078f600: allocate/reuse a texture surface, upload it, and obtain its
 * device handle. Failure paths retain retail's partial state and releases. */
void STDCALL __attribute__((used)) hg2d_device_prepare(uint32_t *texture, uint32_t level)
{
    uint32_t surface, texture_interface, stage, stage_interface, description[27], result, key[2];
    WORD(0x5f44)++;
    if (!texture[0]) return;
    if (texture[3] && texture[7] != level) texture_release(texture);
    surface = texture[3];
    if (!surface) {
        hg2d_device_allocate(texture, level);
        if (!texture[3]) hg2d_device_reuse(texture, level);
        if (texture[7] != level) {
            if ((int32_t)texture[7] > (int32_t)level) level = texture[7];
            texture_release(texture);
            hg2d_device_allocate(texture, level);
        }
        surface = texture[3];
        if (!surface) return;
    }
    texture[2] = 0;
    if (((call_one)device_method(surface, 27))(surface)) return;
    ((call_three)device_method(surface, 0))(surface, ADDRESS(device_guids[5]), ADDRESS(&texture_interface));
    description[0] = 0x6c;
    ((call_two)device_method(surface, 22))(surface, ADDRESS(description));
    if (description[26] & 0x4000) {
        result = hg2d_device_surface(256, 256, level, 0x800, &stage);
        WORD(0x5f3c) = result;
        if (result) return;
        hg2d_device_upload(stage, texture, level);
        ((call_three)device_method(stage, 0))(stage, ADDRESS(device_guids[5]), ADDRESS(&stage_interface));
        WORD(0x5f3c) = ((call_two)device_method(texture_interface, 5))(texture_interface, stage_interface);
        ((call_one)device_method(stage_interface, 2))(stage_interface);
        ((call_one)device_method(stage, 2))(stage);
    } else hg2d_device_upload(surface, texture, level);
    if (description[21] == 8 && WORD(0x289f4)) {
        if (texture[6]) {
            key[0] = key[1] = texture[6] == 0x98 ? 0x96 : texture[6] - 1;
            ((call_three)device_method(surface, 29))(surface, 8, ADDRESS(key));
        } else ((call_three)device_method(surface, 29))(surface, 8, 0);
    }
    WORD(0x5f3c) = ((call_three)device_method(texture_interface, 3))(texture_interface, WORD(0x289d0), ADDRESS(texture + 2));
    ((call_one)device_method(texture_interface, 2))(texture_interface);
}
