/* 0x2078f9f0: EnumTextureFormats callback. Context holds requested RGB bits,
 * alpha bits, and the best eight-word descriptor found so far. */
static uint32_t device_mask_bits(uint32_t mask)
{
    uint32_t bits = 0;
    for (; mask; mask >>= 1) bits += mask & 1;
    return bits;
}

static uint32_t STDCALL device_format_match(const uint32_t *surface, uint32_t *context)
{
    uint32_t format[8], flags, bits, requested;
    __builtin_memcpy(format, surface + 18, sizeof format);
    flags = format[1];
    if (context[1]) {
        if (flags & 0x28) {
            if (context[1] != 1 || !(BYTE(0x28a84) & 8)) return 1;
        } else if ((int32_t)device_mask_bits(format[7]) < (int32_t)context[1]) return 1;
    }
    if (format[3] <= 8) {
        if (!(flags & 0x28)) return 1;
    } else if (!(flags & 0x40)) return 1;
    requested = context[0];
    if (flags & 0x28) {
        if (format[3] < requested) return 1;
    } else {
        bits = device_mask_bits(format[4]) + device_mask_bits(format[5]) + device_mask_bits(format[6]);
        if ((int32_t)bits < (int32_t)requested) return 1;
    }
    if (requested == 8 && format[3] != requested) return 1;
    if (!hg2d_device_format(format)) return 1;
    if (context[5] && format[3] - requested >= context[5] - requested) return 1;
    __builtin_memcpy(context + 2, format, sizeof format);
    return 1;
}

/* 0x2078fb00: normalize legacy format numbers, enumerate every candidate,
 * and copy the selected descriptor even on failure (then all eight words
 * are zero). The EnumTextureFormats HRESULT is ignored. */
uint32_t STDCALL __attribute__((used)) hg2d_device_formats(uint32_t instance, uint32_t bits, uint32_t alpha, uint32_t *out)
{
    uint32_t context[10] = {0};
    uint32_t *object = POINTER(instance), *vtable = POINTER(object[0]);
    if (bits == 16 && alpha == 1) bits = 15;
    if (bits == 12 && alpha == 1) alpha = 4;
    if (bits == 0x22b) { bits = 15; alpha = 0; }
    if (bits == 0x15af) { bits = 15; alpha = 1; }
    if (bits == 0x235) { bits = 16; alpha = 0; }
    if (bits == 0x1bc) { bits = 12; alpha = 0; }
    if (bits == 0x115c) { bits = 12; alpha = 4; }
    if (bits == 0x378) { bits = 24; alpha = 0; }
    if (bits == 0x22b8) { bits = 24; alpha = 8; }
    context[0] = bits; context[1] = alpha;
    ((call_three)(uintptr_t)vtable[9])(instance, ADDRESS(device_format_match), ADDRESS(context));
    __builtin_memcpy(out, context + 2, 32);
    return context[3] != 0;
}
