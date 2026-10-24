/* Device +0x84, 0x2078ded0: an unclipped 24-edge disc triangle fan. */
static uint32_t device_disc_angle(uint32_t phase)
{
    int64_t converted;
#if defined(__i386__)
    const double step = 15.0, turns = 0x1.6c16c16c16c17p-9, circle = 65536.0;
    uint16_t control, truncate;
    __asm__ volatile("fildl %3; fmull %4; fmull %5; fmull %6; "
        "fnstcw %1; movzwl %1,%%eax; orl $0xc00,%%eax; movw %%ax,%2; "
        "fldcw %2; fistpq %0; fldcw %1"
        : "=m"(converted), "=m"(control), "=m"(truncate)
        : "m"(phase), "m"(step), "m"(turns), "m"(circle) : "eax", "st", "memory");
#else
    converted = (int64_t)((long double)(int32_t)phase * 15.0 * (double)(1.0 / 360.0) * 65536.0);
#endif
    return (uint32_t)converted;
}

static void device_disc_base(uint32_t *vertex, int32_t sine, int32_t cosine, const float *radius)
{
#if defined(__i386__)
    const double reciprocal = 0x1.0002000400080p-15;
    __asm__ volatile("fildl %[sin]; fildl %[cos]; fmull %[recip]; fxch %%st(1); "
        "fmull %[recip]; fxch %%st(1); fmuls %[radius]; fxch %%st(1); "
        "fmuls %[radius]; fxch %%st(1); fstps 0(%[out]); fstps 4(%[out])"
        : : [sin]"m"(sine), [cos]"m"(cosine), [recip]"m"(reciprocal), [radius]"m"(*radius), [out]"r"(vertex)
        : "st", "st(1)", "memory");
#else
    ((float *)vertex)[0] = (float)((long double)cosine * (double)(1.0 / 32767.0) * *radius);
    ((float *)vertex)[1] = (float)((long double)sine * (double)(1.0 / 32767.0) * *radius);
#endif
}

static void device_disc_position(uint32_t *destination, const uint32_t *source, float sign_x, float sign_y,
    const float *centre_x, const float *centre_y, uint32_t swap)
{
    const float half = -0.5f;
    const float *input = (const float *)source;
#if defined(__i386__)
    __asm__ volatile("flds %[sx]; fmuls %[x]; fadds %[cx]; fsubs %[half]; fstps 0(%[out]); "
        "flds %[sy]; fmuls %[y]; fadds %[cy]; fsubs %[half]; fstps 4(%[out])"
        : : [sx]"m"(sign_x), [sy]"m"(sign_y), [x]"m"(input[swap ? 1 : 0]), [y]"m"(input[swap ? 0 : 1]),
            [cx]"m"(*centre_x), [cy]"m"(*centre_y), [half]"m"(half), [out]"r"(destination) : "st", "memory");
#else
    ((float *)destination)[0] = (float)((long double)sign_x * input[swap ? 1 : 0] + *centre_x - half);
    ((float *)destination)[1] = (float)((long double)sign_y * input[swap ? 0 : 1] + *centre_y - half);
#endif
}

uint32_t STDCALL __attribute__((used)) hg2d_device_disc(uint32_t surface, uint32_t radius, uint32_t x, uint32_t y)
{
    uint32_t base[4][8], vertices[26][8], phase, octant, index, count = 1, *view;
    float radius_float, centre_x, centre_y;
    radius &= 0xffff; x &= 0xffff; y &= 0xffff;
    if ((int32_t)(x - radius) < 0) return radius;
    view = POINTER(((uint32_t *)POINTER(surface))[7]);
    if ((int32_t)(x + radius) > (int32_t)(view[0] << 1)) return radius;
    if ((int32_t)(y - radius) < 0) return radius;
    if ((int32_t)(y + radius) > (int32_t)((0u - view[2]) << 1)) return (0u - view[2]) << 1;
    device_polygon_colour(); hg2d_device_colour();
    radius_float = (float)radius; centre_x = (float)x; centre_y = (float)y;
    ((float *)base[0])[0] = radius_float; base[0][1] = 0;
    for (phase = 1; phase < 4; phase++) {
        int16_t sine, cosine;
        uint32_t angle = device_disc_angle(phase);
        (*(call_three *)((uintptr_t)WORD(0x24) + 0x2c))(angle, ADDRESS(&sine), ADDRESS(&cosine));
        device_disc_base(base[phase], sine, cosine, &radius_float);
    }
    for (phase = 0; phase < 4; phase++) {
        base[phase][2] = base[phase][3] = 0x3f800000; base[phase][4] = WORD(0x5f38);
    }
    ((float *)vertices[0])[0] = centre_x + 0.5f; ((float *)vertices[0])[1] = centre_y + 0.5f;
    vertices[0][2] = vertices[0][3] = 0x3f800000; vertices[0][4] = WORD(0x5f38);
    for (octant = 0; octant < 8; octant++) {
        uint32_t swap = ((octant >> 1) & 1) ^ (octant & 1);
        float sign_x = octant >= 2 && octant <= 5 ? -1.0f : 1.0f;
        float sign_y = octant >= 4 ? -1.0f : 1.0f;
        for (index = 0; index < 3; index++, count++) {
            const uint32_t *source = base[octant & 1 ? 3 - index : index];
            device_disc_position(vertices[count], source, sign_x, sign_y, &centre_x, &centre_y, swap);
            vertices[count][2] = source[2]; vertices[count][3] = source[3]; vertices[count][4] = source[4];
        }
    }
    device_disc_position(vertices[count], base[0], 1.0f, 1.0f, &centre_x, &centre_y, 0);
    vertices[count][2] = base[0][2]; vertices[count][3] = base[0][3]; vertices[count][4] = base[0][4];
    hg2d_device_flush();
    hg2d_device_draw_state(9, 1); hg2d_device_draw_state(1, 0); hg2d_device_draw_state(27, 0);
    return device_primitive(6, ADDRESS(vertices), count + 1);
}
