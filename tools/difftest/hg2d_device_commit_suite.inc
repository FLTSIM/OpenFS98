/* Writer close and the hardware's three-argument no-op. */
static uint32_t device_commit_entry[2], device_nop_entry[2];
static void run_device_commit(void)
{
    static const uint32_t deltas[] = {0,1,31,32,33,63,64,0xffffffff,0xffffffe1,0xffffffe0,0x7fffffff,0x80000000};
    uint32_t variant, index, next, previous, remaining, result;
    palette_reset(); device_points_compare = 1;
    for (variant = 0; variant < 96; variant++) {
        uint32_t arguments[3] = {0xdeadbeef,0x12345678,0x87654321}, no_op = variant & 1;
        previous = ADDRESS(images[side] + 0x8860 + (scenario % 31));
        next = previous + deltas[(scenario + variant) % 12];
        remaining = random_word();
        WORD(images[side], 0x8848) = next; WORD(images[side], 0x5ee4) = previous; WORD(images[side], 0x5ee8) = remaining;
        touched = NULL; check_carry = 0;
        draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
        result = invoke_draw(no_op ? device_nop_entry[side] : device_commit_entry[side], arguments, no_op ? 3 : 1, 0xaabbccdd);
        draw_active = 0; event(draw_fault_code);
        for (index = 0; index < 7; index++) {
            uint32_t value = index == 0 && !no_op ? draw_registers[index] - ADDRESS(images[side]) : draw_registers[index];
            event(value);
        }
        /* The cursor may wrap far outside the image; compare it relative to
         * the image unconditionally, rather than guessing a pointer range. */
        event(WORD(images[side], 0x8848) - ADDRESS(images[side]));
        WORD(images[side], 0x8848) -= ADDRESS(images[side]);
        if (!no_op) WORD(images[side], 0x5ee4) -= next - previous;
        device_points_compare = 0;
        compare(no_op ? result : result - ADDRESS(images[side]));
        device_points_compare = 1;
        CHECK(!draw_fault_code);
        CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
        if (no_op) CHECK(result == 0xaabbccdd && draw_registers[2] == 0xc0c0c0c0 && draw_registers[3] == 0xd0d0d0d0);
        else CHECK(result == next && draw_registers[2] == (uint32_t)((int32_t)(next - previous) >> 5) && draw_registers[3] == remaining - draw_registers[2]);
    }
    device_points_compare = 0;
}
