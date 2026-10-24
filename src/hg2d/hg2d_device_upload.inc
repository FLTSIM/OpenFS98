/* 0x2078f460: fill a surface and its attached mip chain. Attached surfaces
 * are released before traversing them, matching the chain's owned lifetime. */
void STDCALL __attribute__((used)) hg2d_device_upload(uint32_t surface, uint32_t *texture, uint32_t level)
{
    uint32_t description[27], previous_description[27], previous = 0, format;
    description[0] = 0x6c;
    ((call_two)device_method(surface, 22))(surface, ADDRESS(description));
    format = hg2d_device_format(description + 18);
    while (surface) {
        uint32_t result, next = 0, caps = 0x401000;
        description[0] = 0x6c;
        result = ((call_five)device_method(surface, 25))(surface, 0, ADDRESS(description), 1, 0);
        WORD(0x5f3c) = result;
        if (result) return;
        if (previous && format != 8) {
            previous_description[0] = 0x6c;
            ((call_five)device_method(previous, 25))(previous, 0, ADDRESS(previous_description), 1, 0);
            uint32_t size = 256u >> ((level - 1) & 31);
            hg2d_device_reduce(POINTER(description[9]), description[4], format,
                POINTER(previous_description[9]), previous_description[4], format, size, size, texture[6]);
            ((call_two)device_method(previous, 32))(previous, 0);
        } else if (level == 1 && format != 8) {
            hg2d_device_reduce(POINTER(description[9]), description[4], format,
                POINTER(texture[0]), 256, 8, 256, 256, texture[6]);
        } else {
            uint32_t size = 256u >> (level & 31);
            uint32_t pixels = texture_mip_build(texture, level);
            hg2d_device_convert(POINTER(description[9]), description[4], format,
                POINTER(pixels), 256, 8, size, size, texture[6]);
        }
        ((call_two)device_method(surface, 32))(surface, 0);
        ((call_three)device_method(surface, 12))(surface, ADDRESS(&caps), ADDRESS(&next));
        if (next && next != surface) {
            ((call_one)device_method(next, 2))(next);
            previous = surface; surface = next; level++;
        } else surface = 0;
    }
}
