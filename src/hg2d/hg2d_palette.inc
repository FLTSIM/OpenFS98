/* Palette and colour services. Addresses are retail .text addresses. */
static uint32_t device_palette_realize(void);
#define HALF(offset) (*(uint16_t *)(hg2d_image + (offset)))
#define BYTE(offset) (hg2d_image[(offset)])
#define GLOBAL ((uint8_t *)POINTER(WORD(0x0c)))
#define GLOBAL_HALF(offset) (*(uint16_t *)(GLOBAL + (offset)))
#define GLOBAL_WORD(offset) (*(uint32_t *)(GLOBAL + (offset)))
typedef uint32_t (STDCALL *call_two)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *call_three)(uint32_t, uint32_t, uint32_t);

/* 0x20732344. The caller supplies AX and BX; only their low words matter. */
static uint32_t colour_translate(uint16_t colour, uint16_t pair, uint16_t *mapped_pair)
{
    uint32_t mode = colour >> 8, index, product;
    uint8_t value = (uint8_t)colour;
    if (mode == 0xf0) {
        index = value * 3;
        if (BYTE(0x3402 + index) == 0 && pair <= 0x1800)
            index = 0;
        product = (uint8_t)(BYTE(0x3401 + index) - BYTE(0x3400 + index)) * (pair >> 8);
        /* add al,al / adc ah,0: partial doubling, not product << 1. */
        value = (uint8_t)(BYTE(0x3400 + index) + (product >> 8) + ((product >> 7) & 1));
        pair = HALF(0x3400 + index);
    } else if (mode != 0x69 && mode != 0xe0) {
        index = colour & 15;
        value = BYTE((mode == 0x67 ? 0x34af : 0x349f) + index);
    }
    *mapped_pair = (uint16_t)(BYTE(0x34c0 + (uint8_t)pair * 2) |
                            (BYTE(0x34c0 + (pair >> 8) * 2) << 8));
    value = BYTE(0x34c0 + value * 2);
    BYTE(0x90c) = value;
    return value;
}

/* +0x70, 0x20732320. Output is two word stores, in that order. */
static uint32_t STDCALL colour_convert(uint32_t colour, uint32_t pair, uint16_t *out)
{
    uint16_t mapped_pair;
    uint32_t result = colour_translate((uint16_t)colour, (uint16_t)pair, &mapped_pair);
    out[0] = (uint16_t)result;
    out[1] = mapped_pair;
    return result;
}

/* +0x88, 0x2073263b. Upper eax survives the word load and byte remap. */
uint32_t STDCALL __attribute__((used)) colour_remap_body(uint32_t colour, uint16_t *out, uint32_t entry_eax)
{
    uint16_t result = (uint16_t)((colour & 0xff) | (BYTE(0x34c0 + ((colour >> 8) & 0xff) * 2) << 8));
    *out = result;
    return (entry_eax & 0xffff0000u) | result;
}
EAX_ENTRY(colour_remap, colour_remap_body);

/* 0x20731c42. Build the 256 colour words; signed tests use 16-bit values. */
uint32_t STDCALL __attribute__((used)) palette_layout_body(uint32_t entry_eax)
{
    uint16_t level = GLOBAL_HALF(0xcc0), count, position, stride;
    uint32_t index, group, run, output = 0;
    HALF(0x400a) = level;
    count = (uint16_t)(11 - level);
    if ((int16_t)count >= 0)
        count = 0;
    count = (uint16_t)(count + 32);
    HALF(0x400e) = count;
    stride = (uint16_t)(((count << 8) | (count >> 8)) & 0xffff) / 32;
    position = (uint16_t)(level << 7);
    for (index = 0; index < 32; index++) {
        HALF(0x34c0 + output++ * 2) = position >> 8;
        position = (uint16_t)(position + stride);
    }
    for (group = 1; group <= 8; group++) {
        HALF(0x34c0 + output++ * 2) = position >> 8;
        position = (uint16_t)(position + 0x100);
        if ((int16_t)level >= (int16_t)group) {
            HALF(0x34c0 + output++ * 2) = position >> 8;
            HALF(0x34c0 + output++ * 2) = position >> 8;
            position = (uint16_t)(position + 0x100);
            run = group <= 3 ? 7 : 6;
            for (index = 0; index < run; index++) {
                HALF(0x34c0 + output++ * 2) = position >> 8;
                position = (uint16_t)(position + 0x100);
            }
            if (group <= 3) {
                if ((int16_t)level >= (int16_t)(group + 8)) {
                    HALF(0x34c0 + output++ * 2) = position >> 8;
                    HALF(0x34c0 + output++ * 2) = position >> 8;
                    position = (uint16_t)(position + 0x100);
                } else {
                    for (index = 0; index < 2; index++) {
                        HALF(0x34c0 + output++ * 2) = position >> 8;
                        position = (uint16_t)(position + 0x100);
                    }
                }
            }
        } else {
            run = group <= 3 ? 11 : 8;
            for (index = 0; index < run; index++) {
                HALF(0x34c0 + output++ * 2) = position >> 8;
                position = (uint16_t)(position + 0x100);
            }
        }
    }
    for (index = 0; index < 143; index++) {
        HALF(0x34c0 + output++ * 2) = position >> 8;
        position = (uint16_t)(position + 0x100);
    }
    if (HALF(0x34c0) == HALF(0x34c2))
        HALF(0x34c2)++;
    return (entry_eax & 0xffff0000u) | HALF(0x34c0);
}

void hg2d_palette_layout(void);
__asm__(".text\n.globl _hg2d_palette_layout\n_hg2d_palette_layout:\n\tpushl %eax\n\t"
        "call _palette_layout_body@4\n\tret\n");

/* 0x20731a02: pack duplicated colours and build the initial grey ramp.
 * Reads/writes remain sequential, including when source and destination alias. */
static void palette_copy(uint32_t *source, uint32_t *destination, uint32_t count, int average)
{
    uint32_t index, channel;
    for (index = 0; index < count; index++) {
        for (channel = 0; channel < 3; channel++) {
            uint32_t value = BYTE(*source + channel);
            if (average)
                value = (value + BYTE(*source + channel + 3)) >> 1;
            BYTE(*destination + channel) = (uint8_t)value;
        }
        *source += average ? 6 : 3;
        *destination += 3;
    }
}

static void palette_pack(uint32_t source, uint32_t destination)
{
    uint16_t product = (uint16_t)(HALF(0x400a) * 3);
    uint16_t stride = (uint16_t)(((BYTE(source + 0x5d) << 8) | 0xff) / (uint16_t)(HALF(0x400e) - 1));
    uint16_t value = 0;
    uint32_t index, group;
    int16_t level = (int16_t)HALF(0x400a);
    destination += (uint16_t)((int16_t)product >> 1);
    for (index = 0; index < HALF(0x400e); index++) {
        BYTE(destination) = BYTE(destination + 1) = BYTE(destination + 2) = (uint8_t)(value >> 8);
        destination += 3;
        value = (uint16_t)(value + stride);
    }
    source += 0x60;
    for (group = 1; group <= 8; group++) {
        palette_copy(&source, &destination, 1, 0);
        if (level >= (int16_t)group) {
            palette_copy(&source, &destination, 1, 1);
            palette_copy(&source, &destination, group <= 3 ? 7 : 6, 0);
            if (group <= 3) {
                if (level >= (int16_t)(group + 8))
                    palette_copy(&source, &destination, 1, 1);
                else
                    palette_copy(&source, &destination, 2, 0);
            }
        } else {
            palette_copy(&source, &destination, group <= 3 ? 11 : 8, 0);
        }
    }
    palette_copy(&source, &destination, 143, 0);
}

/* 0x207323f4 / 0x207324e6. UTIL's stateless integer square root is shared. */
static uint32_t palette_brightness(uint32_t index, uint32_t divisor)
{
    uint32_t red = BYTE(0x30a0 + index * 3), green = BYTE(0x30a1 + index * 3);
    uint32_t blue = BYTE(0x30a2 + index * 3);
    call_one square_root = *(call_one *)((uintptr_t)WORD(0x24) + 0x1c);
    return (square_root(red * red + green * green + blue * blue) & 0xffff) / divisor;
}

static void palette_remap(void)
{
    uint32_t index, colour;
    uint8_t value;
    for (index = 0; index < 256; index++) {
        colour = HALF(0x34c0 + index * 2);
        value = (uint8_t)colour;
        if (BYTE(0x4009) && (index < 0x74 || index >= 0x80))
            value = (uint8_t)(palette_brightness(index, 14) + BYTE(0x35c0));
        BYTE(0x94e + index) = value;
        /* Retail only writes the inverse map in brightness mode. Full word
         * indexing is intentional: palette words need not be byte-sized. */
        if (BYTE(0x4009))
            BYTE(0xa4e + colour) = value;
    }
    if (BYTE(0x4009) != BYTE(0x400c)) {
        call_three send = *(call_three *)((uintptr_t)WORD(0x14) + 0x20);
        BYTE(0x400c) = BYTE(0x4009);
        send(0x14, 0x70, 0);
    }
}

static uint32_t palette_shades(void)
{
    uint32_t index, colour, result = 0;
    BYTE(0x4034) ^= 0xa5;
    for (index = 0; index < 256; index++) {
        colour = HALF(0x34c0 + index * 2);
        result = index >= 0x74 && index < 0x80 ? (uint8_t)colour : palette_brightness(index, 16);
        BYTE(0xb4e + colour) = (uint8_t)result;
    }
    return result;
}

/* +0x60, 0x20731fd3. EAX and the carry distinguish the cache's early exit. */
uint32_t hg2d_palette_changed;

static uint32_t palette_refresh(void)
{
    uint32_t index, count, source, result, previous = WORD(0x4011);
    uint8_t brightness = (uint8_t)(GLOBAL_HALF(0xcba) >> 8), ramp, flag = GLOBAL[0xb6a];
    WORD(0x4005) = GLOBAL_WORD(0x398);
    result = (WORD(0x4005) & 0xffff0000u) | (brightness * 0x101);
    if (!(GLOBAL_HALF(0xc46) & 0x20)) {
        uint8_t night = brightness < 0x80;
        if (BYTE(0x400d) != night) {
            BYTE(0x400d) = night;
            GLOBAL[0x280] = night;
        }
    }
    if (GLOBAL[0x374] != BYTE(0x4003)) {
        BYTE(0x4003) = GLOBAL[0x374];
        BYTE(0x4004) = 0;
    }
    ramp = GLOBAL[0x374] == 0 && brightness < 0x80 ? GLOBAL[0x280] & 1 : 0;
    if (flag == 1)
        ramp = 0;
    result = (result & ~0xff00u) | (ramp << 8);
    if ((uint8_t)previous != flag || ramp != BYTE(0x4009)) {
        if ((uint8_t)previous != flag)
            BYTE(0x4004) = 0;
        BYTE(0x4009) = ramp;
        palette_remap();
        /* Remap leaves EBX at zero, or the final palette byte offset (765). */
        WORD(0x4011) = (ramp ? 0x200 : 0) | GLOBAL[0xb6a];
    }
    if (brightness == BYTE(0x4004)) {
        hg2d_palette_changed = 0;
        return result;
    }
    BYTE(0x4004) = brightness;
    source = HALF(0x3fc2) ? 0x33d0 : 0x33a0;
    for (index = 0; index < 48; index++)
        BYTE(0x3280 + index) = BYTE(source + index);
    for (index = 0; index < 0x15c; index++)
        BYTE(0x36c2 + index) = (uint8_t)((BYTE(0x30a0 + index) * brightness) >> 8);
    for (index = 0; index < 0x24; index++)
        BYTE(0x381e + index) = BYTE(0x31fc + index);
    for (index = 0; index < 0x60; index++)
        BYTE(0x3842 + index) = (uint8_t)((BYTE(0x3220 + index) * brightness) >> 8);
    if (GLOBAL_WORD(0x390) >= 2) {
        count = GLOBAL_HALF(0xcbc);
        if (!count || count > 64) count = 64;
        for (index = 0; index < count * 3; index++)
            BYTE(0x3902 + index) = (uint8_t)((BYTE(0x32e0 + index) * brightness) >> 8);
    } else {
        for (index = 0; index < 0xc0; index++)
            BYTE(0x3902 + index) = BYTE(0x32e0 + index);
    }
    for (index = 0; index < 0x60; index++)
        BYTE(0x38a2 + index) = (uint8_t)((BYTE(0x3280 + index) * brightness) >> 8);
    source = 0x3fee;
    for (count = 16, index = 0; count; count--, index += 3) {
        uint8_t addition = (uint8_t)((BYTE(source) * GLOBAL_HALF(0x12a0)) >> 8);
        uint32_t sum = BYTE(0x38a2 + index) + addition;
        BYTE(0x38a2 + index) = (uint8_t)(sum > 255 ? 255 : sum);
        if (!WORD(0x4005) || count > 8)
            source++;
    }
    palette_remap();
    result = palette_shades();
    if (BYTE(0x4009)) {
        uint8_t value = 0;
        for (index = 0; index < 32; index++) {
            value = (uint8_t)(value + 8);
            if (!value) value--;
            BYTE(0x3842 + index * 3) = value;
            BYTE(0x3843 + index * 3) = value >> 2;
            BYTE(0x3844 + index * 3) = 0;
        }
        result = 0x3fff;
    }
    HALF(0x36c0) = 1;
    hg2d_palette_changed = 1;
    return result;
}

uint32_t STDCALL __attribute__((used)) palette_refresh_body(void)
{
    return palette_refresh();
}

/* +0x64, 0x2073261f. Force refresh, including a dirty flag on a cache hit. */
uint32_t STDCALL __attribute__((used)) palette_force_body(void)
{
    uint32_t result;
    BYTE(0x4004) = 0;
    result = palette_refresh();
    HALF(0x36c0) = 1;
    return result;
}

/* +0x7c, 0x2073256d. The caller receives a pointer into our image. */
uint32_t STDCALL __attribute__((used)) palette_select_body(uint32_t selector)
{
    if (HALF(0x3fc2) != (uint16_t)selector) {
        HALF(0x3fc2) = (uint16_t)selector;
        BYTE(0x4004) = 0;
    }
    palette_refresh();
    return ADDRESS(hg2d_image + 0xb4e);
}

/* +0x5c, 0x20731947: software palette realization. */
static uint32_t STDCALL palette_realize(void)
{
    uint32_t index;
    call_two realize = *(call_two *)((uintptr_t)WORD(4) + 0x290);
    HALF(0x36c0) = 0;
    if (GLOBAL_HALF(0xcbe) == 2) {
        for (index = 0x240 + GLOBAL_HALF(0xcbc) * 3; index != 0x300; index += 3) {
            HALF(0x36c2 + index) = HALF(0x30a0 + index);
            BYTE(0x36c4 + index) = BYTE(0x30a2 + index);
        }
    }
    GLOBAL_HALF(0xcbe) = 0;
    palette_pack(0x36c2, 0x39c2);
    palette_pack(0x30a0, 0x3cc2);
    realize(ADDRESS(hg2d_image + 0x39c2), ADDRESS(hg2d_image + 0x3cc2));
    return device_palette_realize();
}

/* The assembly cache API returns the changed bit in carry as well as eax. */
void palette_refresh_entry(void);
void palette_force(void);
void palette_select(void);
__asm__(".text\n_palette_refresh_entry:\n\tcall _palette_refresh_body@0\n\t"
        "btl $0,_hg2d_palette_changed\n\tret\n"
        "_palette_force:\n\tcall _palette_force_body@0\n\t"
        "btl $0,_hg2d_palette_changed\n\tret\n"
        "_palette_select:\n\tpushl 4(%esp)\n\tcall _palette_select_body@4\n\t"
        "btl $0,_hg2d_palette_changed\n\tret $4\n");
