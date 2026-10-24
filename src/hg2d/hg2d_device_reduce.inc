/* 0x2078ebd0: 2x2 texture reduction. Retail only implements indexed-to-
 * 16-bit and same-format 16-bit paths here; 8/32-bit outputs are no-ops. */
void STDCALL __attribute__((used)) hg2d_device_reduce(uint8_t *destination, uint32_t destination_pitch,
    uint32_t format, const uint8_t *source, uint32_t source_pitch, uint32_t source_format,
    uint32_t width, uint32_t height, uint32_t key)
{
    uint32_t x, y, rows, destination_stride, source_stride;
    if (format != 0x22b && format != 0x235 && format != 0x1bc) return;
    if (source_format != 8 && source_format != format) return;
    if ((int32_t)height <= 0) return;
    rows = (height + 1) >> 1;
    destination_stride = (uint32_t)(((int32_t)destination_pitch / 2) * 2);
    source_stride = source_format == 8 ? source_pitch : (uint32_t)(((int32_t)source_pitch / 2) * 2);
    for (y = 0; y < rows; y++) {
        for (x = 0; (int32_t)x < (int32_t)width; x += 2) {
            uint32_t red = 0, green = 0, blue = 0, alpha = 0, sample;
            uint16_t packed;
            for (sample = 0; sample < 4; sample++) {
                uint32_t offset = (sample >> 1) * source_stride;
                uint32_t column = x + (sample & 1);
                if (source_format == 8) {
                    uint32_t indexed = *(const uint8_t *)POINTER(ADDRESS(source) + offset + column);
                    red += BYTE(0x39c2 + indexed * 3); green += BYTE(0x39c3 + indexed * 3); blue += BYTE(0x39c4 + indexed * 3);
                    if (indexed >= (key & 255)) alpha++;
                } else {
                    uint16_t value;
                    __builtin_memcpy(&value, POINTER(ADDRESS(source) + offset + column * 2), 2);
                    if (format == 0x22b) {
                        red += (value >> 7) & 0xf8; green += (value >> 2) & 0xf8; blue += (value & 31) << 3;
                        alpha += (value >> 8) & 0x80;
                    } else if (format == 0x235) {
                        red += (value >> 8) & 0xf8; green += (value >> 3) & 0xfc; blue += (value & 31) << 3;
                    } else {
                        red += (value >> 4) & 0xf0; green += value & 0xf0; blue += (value & 15) << 4;
                        alpha += (value >> 8) & 0xf0;
                    }
                }
            }
            if (source_format == 8) { red /= 4; green /= 4; blue /= 4; }
            else { red = (red + 2) / 4; green = (green + 2) / 4; blue = (blue + 2) / 4; }
            if (format == 0x22b) {
                packed = (uint16_t)(((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3));
                if (source_format == 8 ? alpha >= 2 : (((alpha + 2) / 4) & 0x80) != 0) packed |= 0x8000;
            } else if (format == 0x235) {
                packed = (uint16_t)(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
            } else {
                packed = (uint16_t)(((red >> 4) << 8) | ((green >> 4) << 4) | (blue >> 4));
                if (source_format == 8) { if (alpha >= 2) packed |= 0xf000; }
                else packed |= (uint16_t)((((alpha + 2) / 4) & 0xf0) << 8);
            }
            __builtin_memcpy(destination + x, &packed, 2);
        }
        destination = POINTER(ADDRESS(destination) + destination_stride);
        source = POINTER(ADDRESS(source) + source_stride * 2);
    }
}
