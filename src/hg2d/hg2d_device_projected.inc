/* +0x44 device target 0x2078d370 and texture-plane helper 0x20784fb4. */
static uint32_t device_projected_coordinate(uint32_t coordinate, uint32_t depth)
{
    uint32_t value = projective_divide((int64_t)(int32_t)coordinate * 32768, depth);
    uint32_t sign = (int32_t)value < 0 ? 0xffffffffu : 0;
    value ^= sign;
    if (value > 0x7fff) value = 0x7fff;
    return value ^ sign;
}

static void device_projected_attributes(uint32_t x, uint32_t y, uint32_t depth, uint32_t *u, uint32_t *v, uint32_t *distance)
{
    uint32_t component, factor, magnitude;
    WORD(0x4168) = device_projected_coordinate(x, depth);
    WORD(0x416c) = device_projected_coordinate(y, depth);
    for (component = 0; component < 3; component++) {
        WORD(0x41c8 + component * 4) = GLOBAL_WORD(0xcdc + component * 4);
        WORD(0x41d4 + component * 4) = GLOBAL_WORD(0xc94 + component * 4);
    }
    WORD(0x41e0) = GLOBAL_WORD(0xca4); WORD(0x41e4) = GLOBAL_WORD(0xca8);
    WORD(0x41e8) = GLOBAL_WORD(0xcac); WORD(0x41ec) = GLOBAL_WORD(0xcb0);
    for (component = 0; component < 3; component++)
        WORD(0x4178 + component * 4) = matrix_attribute(WORD(0x4168), WORD(0x416c), component);
    factor = projective_project(0x4178, 0x4190);
    magnitude = factor ^ ((int32_t)factor < 0 ? 0xffffffffu : 0);
    WORD(0x4361) = magnitude;
    *u = WORD(0x4190);
    if (!((WORD(0x41e0) ^ WORD(0x41e8)) & 0xff000000)) *u &= 0xffffff;
    *v = WORD(0x4194);
    if (!((WORD(0x41e4) ^ WORD(0x41ec)) & 0xff000000)) *v &= 0xffffff;
    *distance = matrix_unsigned_divide((uint64_t)magnitude << 21, GLOBAL_WORD(0x320));
}

static uint32_t device_projected_distance(uint32_t depth, uint32_t *registers)
{
    uint32_t result = GLOBAL_WORD(0xd4c), ecx = depth, edx = WORD(0x2c);
#if defined(__i386__)
    __asm__ volatile("pushl %%ecx; call *0x44(%%edx)" : "+a"(result), "+c"(ecx), "+d"(edx) : : "memory", "cc");
#else
    result = (*(call_one *)((uintptr_t)edx + 0x44))(depth);
#endif
    registers[1] = ADDRESS(GLOBAL); registers[0] = edx;
    return result;
}

void STDCALL __attribute__((used)) device_projected_vertex_body(uint32_t surface, uint32_t x, uint32_t y,
    uint32_t depth, uint32_t *registers)
{
    uint32_t cursor, *vertex, mode, texture, distance, threshold;
    const uint32_t *view = POINTER(((uint32_t *)POINTER(surface))[7]);
    GLOBAL_WORD(0x2f0)++; WORD(0x8854) = GLOBAL_WORD(0x2f0);
    if (!WORD(0x8854)) {
        device_polygon_colour(); hg2d_device_colour();
        texture = GLOBAL_HALF(0xc90) ? GLOBAL_WORD(0xc8c) : 0;
        mode = GLOBAL_HALF(0xc90) ? (GLOBAL_HALF(0xca0) ? 6 : 5) : (WORD(0x5f38) & 0xff000000 ? 4 : 3);
        if (WORD(0x8850) != mode || (GLOBAL_HALF(0xc90) && WORD(0x885c) != texture) || (int32_t)WORD(0x5ee8) < 100)
            hg2d_device_batch(mode, texture);
        cursor = WORD(0x5ee4); WORD(0x5ee0) = cursor;
    } else cursor = WORD(0x8848);
    vertex = device_fan_vertex(cursor);
    vertex[2] = 0x3f800000; vertex[4] = WORD(0x5f38);
    device_project_vertex(vertex, view, x, y, depth);
    if (WORD(0x8840)) {
        uint32_t u, v;
        device_projected_attributes(x, y, depth, &u, &v, &distance);
        device_uv_float(vertex + 6, u); device_uv_float(vertex + 7, v);
        vertex[5] = device_vertex_fog(distance, registers);
        WORD(0x8858) |= u | v; registers[1] = WORD(0x8858); registers[0] = u;
    } else if (GLOBAL_WORD(0xd4c) >= 0xc35000) {
        registers[1] = vertex[4]; registers[0] = ADDRESS(GLOBAL);
        threshold = GLOBAL_HALF(0xe88);
        if (!GLOBAL_HALF(0xdce)) {
            registers[0] = (registers[0] & 0xffff0000u) | threshold;
            if (threshold < 0x7d00) vertex[5] = device_vertex_fog(0x7fffffff, registers);
            else vertex[5] = 0xff000000;
        } else vertex[5] = 0xff000000;
    } else {
        distance = device_projected_distance(depth, registers);
        if (!GLOBAL_HALF(0xdce) && GLOBAL_HALF(0xe88) < 0x7d00) {
            registers[0] = GLOBAL_WORD(0xd4c);
            if (registers[0]) vertex[5] = device_vertex_fog(distance, registers);
            else vertex[5] = 0xff000000;
        } else vertex[5] = 0xff000000;
    }
    WORD(0x8848) += 32; registers[2] = WORD(0x8848);
}

void hg2d_device_projected_vertex(void);
__asm__(".text\n.globl _hg2d_device_projected_vertex\n_hg2d_device_projected_vertex:\n\t"
    "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\tmovl %esp,%eax\n\tpushl %eax\n\t"
    ".rept 4\n\tpushl 32(%esp)\n\t.endr\n\tcall _device_projected_vertex_body@20\n\t"
    "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $16\n");
