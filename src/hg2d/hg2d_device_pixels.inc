/* Pixel-format selection and indexed texture conversion used by device upload.
 * Named exports let the standalone harness check these internal helpers. */
uint32_t STDCALL __attribute__((used)) hg2d_device_format(const uint32_t *format)
{
    if (format[3] == 8 && (format[1] & 0x20)) return 8;
    if (format[3] == 16) {
        if (format[4] == 0x7c00 && format[5] == 0x3e0 && format[6] == 0x1f) return 0x22b;
        if (format[4] == 0xf800 && format[5] == 0x7e0 && format[6] == 0x1f) return 0x235;
        if (format[4] == 0xf00 && format[5] == 0xf0 && format[6] == 0xf) return 0x1bc;
    }
    if (format[3] == 32 && format[4] == 0xff0000 && format[5] == 0xff00 && format[6] == 0xff) return 32;
    return 0;
}

void STDCALL __attribute__((used)) hg2d_device_convert(uint8_t *destination, uint32_t destination_pitch,
    uint32_t format, const uint8_t *source, uint32_t source_pitch, uint32_t unused,
    uint32_t width, uint32_t height, uint32_t key)
{
    uint32_t x, y, stride = destination_pitch;
    (void)unused;
    if (format != 8 && format != 0x22b && format != 0x235 && format != 0x1bc && format != 32) return;
    if (format != 8 && format != 32) stride = (uint32_t)(((int32_t)destination_pitch / 2) * 2);
    for (y = 0; (int32_t)y < (int32_t)height; y++) {
        for (x = 0; (int32_t)x < (int32_t)width; x++) {
            uint32_t indexed = source[x];
            if (format == 8) {
                uint8_t value = (uint8_t)indexed;
                if (indexed < key) value = (uint8_t)key == 0x98 ? 0x96 : (uint8_t)(key - 1);
                destination[x] = value;
            } else if (format == 32) {
                /* Keep byte stores between palette reads: output may alias it. */
                destination[x * 4] = BYTE(0x39c4 + indexed * 3);
                destination[x * 4 + 1] = BYTE(0x39c3 + indexed * 3);
                destination[x * 4 + 2] = BYTE(0x39c2 + indexed * 3);
                destination[x * 4 + 3] = indexed >= (key & 255) ? 255 : 0;
            } else {
                uint16_t packed;
                uint8_t red = BYTE(0x39c2 + indexed * 3), green = BYTE(0x39c3 + indexed * 3), blue = BYTE(0x39c4 + indexed * 3);
                if (format == 0x22b) {
                    packed = (uint16_t)(((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3));
                    if (indexed >= (key & 255)) packed |= 0x8000;
                } else if (format == 0x235) {
                    packed = (uint16_t)(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
                } else {
                    packed = (uint16_t)(((red >> 4) << 8) | ((green >> 4) << 4) | (blue >> 4));
                    if (indexed >= (key & 255)) packed |= 0xf000;
                }
                __builtin_memcpy(destination + x * 2, &packed, 2);
            }
        }
        source = POINTER(ADDRESS(source) + source_pitch);
        destination = POINTER(ADDRESS(destination) + stride);
    }
}
