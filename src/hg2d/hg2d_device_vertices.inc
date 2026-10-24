/* Device triangle-fan vertex writers. Projection retains the retail x87 order. */
static void device_project_vertex(uint32_t *vertex, const uint32_t *view, uint32_t x, uint32_t y, uint32_t z)
{
#if defined(__i386__)
    const float fixed_scale = 0x1p-16f, one = 1.0f;
    __asm__ volatile(
        "fildl %[z]; fmuls %[scale]; fildl %[x]; fmuls %[scale]; fdiv %%st(1),%%st; "
        "fildl 0(%[view]); fmulp; fildl 4(%[view]); faddp; fstps 0(%[vertex]); "
        "fildl %[y]; fmuls %[scale]; fdiv %%st(1),%%st; "
        "fildl 8(%[view]); fmulp; fildl 12(%[view]); faddp; fstps 4(%[vertex]); "
        "flds %[one]; fdiv %%st(1),%%st; fstps 12(%[vertex]); fstp %%st(0)"
        : : [vertex]"r"(vertex), [view]"r"(view), [x]"m"(x), [y]"m"(y), [z]"m"(z),
            [scale]"m"(fixed_scale), [one]"m"(one) : "memory", "st", "st(1)", "st(2)");
#else
    float *output = (float *)vertex;
    double depth = (double)(int32_t)z / 65536.0;
    output[0] = (float)((double)(int32_t)x / 65536.0 / depth * (int32_t)view[0] + (int32_t)view[1]);
    output[1] = (float)((double)(int32_t)y / 65536.0 / depth * (int32_t)view[2] + (int32_t)view[3]);
    output[3] = (float)(1.0 / depth);
#endif
}

static uint32_t device_distance(uint32_t depth, const uint32_t *view, uint32_t *registers)
{
    uint32_t result = WORD(0x2c), ecx = ADDRESS(view), edx = depth;
#if defined(__i386__)
    __asm__ volatile("pushl %%edx; call *0x44(%%eax)" : "+a"(result), "+c"(ecx), "+d"(edx) : : "memory", "cc");
#else
    result = (*(call_one *)((uintptr_t)WORD(0x2c) + 0x44))(depth);
#endif
    registers[1] = ecx; registers[0] = edx;
    return result;
}

static uint32_t device_vertex_fog(uint32_t distance, uint32_t *registers)
{
    uint32_t threshold, amount = 0, scale, maximum;
    if (GLOBAL_HALF(0xdce)) return 0xff000000;
    threshold = GLOBAL_HALF(0xe88);
    registers[1] = (registers[1] & 0xffff0000u) | threshold;
    if (threshold >= 0x7d00) return 0xff000000;
    registers[0] = GLOBAL_WORD(0xd4c);
    if (!registers[0]) return 0xff000000;
    registers[1] = (uint32_t)((int32_t)distance >> 8);
    if ((int32_t)registers[1] >= (int32_t)threshold) {
        amount = matrix_signed_divide((uint32_t)((int32_t)distance >> 4), threshold);
        scale = GLOBAL_WORD(0x30c); maximum = GLOBAL_HALF(0xcd4);
        registers[0] = scale; registers[1] = maximum;
        if ((int32_t)scale > (int32_t)maximum) {
            int64_t product = (int64_t)(int32_t)amount * maximum;
            amount = (uint32_t)(product / (int32_t)scale);
            registers[0] = (uint32_t)(product % (int32_t)scale);
        }
        if ((int32_t)amount > 254) amount = 254;
        if (GLOBAL_HALF(0xca0) == 0x98 && threshold == 0x1fff) amount = 0;
    }
    WORD(0x884c) |= amount;
    return (0u - (amount + 1)) << 24;
}

static uint32_t *device_fan_vertex(uint32_t cursor)
{
    if ((int32_t)WORD(0x8854) >= 3) {
        uint32_t *destination = POINTER(cursor), *first = POINTER(WORD(0x5ee0));
        uint32_t index;
        for (index = 0; index < 8; index++) destination[index] = first[index];
        for (index = 0; index < 8; index++) destination[index + 8] = (destination - 8)[index];
        cursor += 64;
    }
    WORD(0x8848) = cursor;
    return POINTER(cursor);
}

void STDCALL __attribute__((used)) device_palette_vertex_body(uint32_t surface, uint32_t x, uint32_t y,
    uint32_t depth, uint32_t colour, uint32_t *registers)
{
    uint32_t cursor, *vertex, index, distance;
    const uint32_t *view = POINTER(((uint32_t *)POINTER(surface))[7]);
    GLOBAL_WORD(0x2f0)++; WORD(0x8854) = GLOBAL_WORD(0x2f0);
    if (!WORD(0x8854)) {
        if (WORD(0x8850) != 8 || (int32_t)WORD(0x5ee8) < 100) hg2d_device_batch(8, 0);
        cursor = WORD(0x5ee4); WORD(0x5ee0) = cursor;
    } else cursor = WORD(0x8848);
    index = (colour & 0xffff) >> 8;
    vertex = device_fan_vertex(cursor);
    vertex[2] = 0x3f800000;
    vertex[4] = (uint32_t)BYTE(0x39c2 + index * 3) << 16 |
                (uint32_t)BYTE(0x39c3 + index * 3) << 8 | BYTE(0x39c4 + index * 3);
    vertex[6] = vertex[7] = 0;
    device_project_vertex(vertex, view, x, y, depth);
    distance = device_distance(depth, view, registers);
    vertex[5] = device_vertex_fog(distance, registers);
    WORD(0x8848) += 32; registers[2] = WORD(0x8848);
}

void hg2d_device_palette_vertex(void);
__asm__(".text\n.globl _hg2d_device_palette_vertex\n_hg2d_device_palette_vertex:\n\t"
    "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\tmovl %esp,%eax\n\tpushl %eax\n\t"
    ".rept 5\n\tpushl 36(%esp)\n\t.endr\n\tcall _device_palette_vertex_body@24\n\t"
    "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $20\n");

static void device_uv_float(uint32_t *destination, uint32_t value)
{
#if defined(__i386__)
    const float scale = 0x1p-24f;
    __asm__ volatile("fildl %1; fmuls %2; fstps %0" : "=m"(*destination) : "m"(value), "m"(scale));
#else
    *(float *)destination = (float)((double)(int32_t)value / 16777216.0);
#endif
}

static void device_polygon_colour(void)
{
    uint16_t colour = (uint16_t)GLOBAL_WORD(0x3c0), pair;
    if (colour >> 8 == 0x68) colour += 0x100;
    colour_translate(colour, GLOBAL_HALF(0xcc8), &pair);
}

void STDCALL __attribute__((used)) device_uv_vertex_body(uint32_t surface, uint32_t x, uint32_t y,
    uint32_t depth, uint32_t u, uint32_t v, uint32_t intensity, uint32_t shaded, uint32_t *registers)
{
    uint32_t cursor, *vertex, texture, mode, distance, shade = (uint32_t)((int32_t)intensity >> 23);
    const uint32_t *view = POINTER(((uint32_t *)POINTER(surface))[7]);
    GLOBAL_WORD(0x2f0)++; WORD(0x8854) = GLOBAL_WORD(0x2f0);
    if (!WORD(0x8854)) {
        texture = GLOBAL_HALF(0xcb6) ? GLOBAL_WORD(0xc8c) : 0;
        mode = shaded ? 7 : GLOBAL_HALF(0xcb6) && GLOBAL_HALF(0xca0) ? 6 : 5;
        if (WORD(0x8850) != mode || WORD(0x885c) != texture || (int32_t)WORD(0x5ee8) < 100)
            hg2d_device_batch(mode, texture);
        cursor = WORD(0x5ee4); WORD(0x5ee0) = cursor;
        if (!shaded) {
            WORD(0x8848) = cursor;
            device_polygon_colour();
            texture = WORD(0x885c);
            if (WORD(0x8840) && !(texture && (((uint32_t *)POINTER(texture))[1] & 0x100000) && GLOBAL_HALF(0xcba) >= 0x4000))
                WORD(0x5f38) = 0xffffffff;
            else {
                uint32_t index = BYTE(0x90c);
                WORD(0x5f38) = (uint32_t)BYTE(0x39c2 + index * 3) << 16 |
                    (uint32_t)BYTE(0x39c3 + index * 3) << 8 | BYTE(0x39c4 + index * 3);
            }
        }
    } else cursor = WORD(0x8848);
    vertex = device_fan_vertex(cursor);
    vertex[4] = shaded ? ((shade << 8 | shade) << 8 | shade) : WORD(0x5f38);
    device_uv_float(vertex + 6, u); device_uv_float(vertex + 7, v);
    WORD(0x8858) |= u | v; vertex[2] = 0x3f800000;
    device_project_vertex(vertex, view, x, y, depth);
    distance = device_distance(depth, view, registers);
    vertex[5] = device_vertex_fog(distance, registers);
    if (shaded) {
        if (GLOBAL_HALF(0xdce) || GLOBAL_HALF(0xe88) >= 0x7d00 || !GLOBAL_WORD(0xd4c)) registers[0] = ADDRESS(vertex);
        else if ((int32_t)distance >> 8 < (int32_t)GLOBAL_HALF(0xe88)) registers[0] = (uint32_t)((int32_t)distance >> 8);
        registers[1] = 1; registers[2] = WORD(0x289e0);
        if (registers[2] == 1 && (int32_t)shade >= 254) {
            uint32_t colour;
            shade = shade == 255 ? 128 : 64;
            WORD(0x8844) = 1;
            vertex[5] |= (shade << 8 | shade) << 8 | shade;
            registers[2] = 255 - shade;
            colour = (registers[2] << 8) | (registers[2] << 16) | registers[2];
            vertex[4] = colour; registers[0] = colour;
        }
    }
    WORD(0x8848) += 32;
    if (!shaded) registers[2] = WORD(0x8848);
}

#define DEVICE_UV_ENTRY(name, shaded, count, bytes, extra) \
    void name(void); \
    __asm__(".text\n.globl _" #name "\n_" #name ":\n\t" \
        "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\tmovl %esp,%eax\n\tpushl %eax\n\tpushl $" #shaded "\n\t" \
        extra ".rept " #count "\n\tpushl 48(%esp)\n\t.endr\n\tcall _device_uv_vertex_body@36\n\t" \
        "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $" #bytes "\n")
DEVICE_UV_ENTRY(hg2d_device_uv_vertex, 0, 6, 24, "pushl $0\n\t");
DEVICE_UV_ENTRY(hg2d_device_intensity_vertex, 1, 7, 28, "");
