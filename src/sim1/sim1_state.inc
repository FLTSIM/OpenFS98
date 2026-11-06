/* Situation serialization and teardown. Retail 0x22011050..0x22011282,
 * 0x22010d50, 0x220010a0 and 0x2200a7fe. This is included by sim1.c so all
 * state continues to live in the one retail-offset image.
 */
struct sim1_registers {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

void sim1_import(struct sim1_registers *registers, uintptr_t function,
                 uint32_t count, const uintptr_t *arguments);

static uint32_t sim1_read32(const void *pointer)
{
    return *(const volatile uint32_t *)pointer;
}

static void sim1_write32(void *pointer, uint32_t value)
{
    *(volatile uint32_t *)pointer = value;
}

static uint8_t *sim1_dependency(uint32_t index)
{
    return (uint8_t *)sim1_image.imports[index].link;
}

static void sim1_import_call(struct sim1_registers *registers, uint32_t index,
                            uint32_t slot, uint32_t count, const uintptr_t *arguments)
{
    sim1_import(registers, sim1_read32(sim1_dependency(index) + slot), count, arguments);
}

/* Each pair is loaded before either store. Keeping both reads volatile also
 * preserves the observable order when a test aims the output into the image.
 */
static void sim1_record_pair(uint8_t *out, uint32_t offset, uint32_t source,
                             uint32_t *low, uint32_t *high)
{
    *low = sim1_read32(D + source);
    *high = sim1_read32(D + source + 4);
    sim1_write32(out + offset, *low);
    sim1_write32(out + offset + 4, *high);
}

void sim1_record_write_core(struct sim1_registers *registers, uint8_t *out)
{
    uint32_t low, high;
    __asm__ volatile("fldl %1\n\tfstpl (%0)"
                     : : "r"(out), "m"(*(double *)(D + 0x1538)) : "st", "memory");
    sim1_record_pair(out, 0x08, 0x1540, &low, &high);
    sim1_record_pair(out, 0x10, 0x1548, &low, &high);
    sim1_record_pair(out, 0x18, 0x13e8, &low, &high);
    sim1_record_pair(out, 0x20, 0x13f0, &low, &high);
    sim1_record_pair(out, 0x28, 0x13f8, &low, &high);
    sim1_record_pair(out, 0x30, 0x13a0, &low, &high);
    sim1_record_pair(out, 0x38, 0x13a8, &low, &high);
    sim1_record_pair(out, 0x40, 0x13b0, &low, &high);
    sim1_record_pair(out, 0x48, 0x15a8, &low, &high);
    sim1_write32(out + 0x50, sim1_read32(D + 0x1d18));
    sim1_write32(out + 0x54, *(volatile uint8_t *)(sim1_dependency(1) + 0x366));
    sim1_record_pair(out, 0x58, 0x1358, &low, &high);
    sim1_record_pair(out, 0x60, 0x1360, &low, &high);
    sim1_record_pair(out, 0x68, 0x1368, &low, &high);
    sim1_record_pair(out, 0x70, 0x1328, &low, &high);
    sim1_record_pair(out, 0x78, 0x1330, &low, &high);
    sim1_record_pair(out, 0x80, 0x1338, &low, &high);
    sim1_record_pair(out, 0x88, 0x1418, &low, &high);
    sim1_record_pair(out, 0x90, 0x1420, &low, &high);
    sim1_record_pair(out, 0x98, 0x1428, &low, &high);
    sim1_record_pair(out, 0xa0, 0x13b8, &low, &high);
    sim1_record_pair(out, 0xa8, 0x13c0, &low, &high);
    sim1_record_pair(out, 0xb0, 0x13c8, &low, &high);
    sim1_record_pair(out, 0xb8, 0x1760, &low, &high);
    registers->eax = (uintptr_t)out;
    registers->ecx = high;
    registers->edx = low;
}

void sim1_save_situation_core(struct sim1_registers *registers)
{
    uint8_t record[0xc0];
    uintptr_t arguments[3];
    sim1_record_write_core(registers, record);
    registers->ecx = (uintptr_t)sim1_dependency(5);
    arguments[0] = 0x38e;
    arguments[1] = 0xc0;
    sim1_import_call(registers, 5, 0x5c, 2, arguments);
    if (registers->eax == 0) {
        registers->eax = (uintptr_t)sim1_dependency(5);
        registers->edx = (uintptr_t)record;
        arguments[0] = (uintptr_t)record;
        arguments[1] = 0xc0;
        arguments[2] = 0;
        sim1_import_call(registers, 5, 0x60, 3, arguments);
    }
}

void sim1_retain_state_core(struct sim1_registers *registers)
{
    uint8_t *global = sim1_dependency(1);
    uintptr_t arguments[4];
    registers->eax = (uintptr_t)(global + 0x11d8);
    if (sim1_read32(global + 0x11d8) != 0) {
        registers->ecx = (uintptr_t)sim1_dependency(0);
        arguments[0] = (uintptr_t)(global + 0x11d8);
        sim1_import_call(registers, 0, 0x214, 1, arguments);
    }
    global = sim1_dependency(1);
    registers->edx = (uintptr_t)(global + 0x11d8);
    registers->eax = (uintptr_t)sim1_dependency(0);
    arguments[0] = 1;
    arguments[1] = 0xc0;
    arguments[2] = (uintptr_t)(global + 0x11d8);
    arguments[3] = 1;
    sim1_import_call(registers, 0, 0x20c, 4, arguments);
    registers->ecx = (uintptr_t)sim1_dependency(1);
    registers->edx = sim1_read32(sim1_dependency(1) + 0x11d8);
    sim1_record_write_core(registers, (uint8_t *)(uintptr_t)registers->edx);
}

void sim1_deinit_core(struct sim1_registers *registers)
{
    uintptr_t arguments[2];
    uint8_t *global;
    registers->eax = (uintptr_t)sim1_dependency(3);
    arguments[0] = 1;
    arguments[1] = (uintptr_t)sim1_chain_tick;
    sim1_import_call(registers, 3, 0x1c, 2, arguments);
    registers->ecx = (uintptr_t)sim1_dependency(3);
    arguments[0] = 5;
    arguments[1] = (uintptr_t)sim1_chain_frame;
    sim1_import_call(registers, 3, 0x1c, 2, arguments);
    registers->edx = (uintptr_t)sim1_dependency(3);
    arguments[0] = 0x14;
    arguments[1] = (uintptr_t)sim1_situation_callback;
    sim1_import_call(registers, 3, 0x1c, 2, arguments);
    sim1_retain_state_core(registers);
    global = sim1_dependency(1);
    registers->eax = (uintptr_t)global;
    if (sim1_read32(global + 0x604) == 1) {
        registers->ecx = sim1_read32(D + 0x1c80);
        sim1_write32(global + 0x278, registers->ecx);
    }
    /* 0x2200a7fe preserves all registers. Reset engine selectors only when
     * GLOBAL's aircraft flag is exactly one and no situation was loaded.
     */
    if (global[0x609] == 1 && sim1_read32(D + 0x1778) == 0) {
        static const uint32_t offsets[] = {0x892, 0x92a, 0x9c2, 0xa5a};
        uint32_t i;
        for (i = 0; i < 4; ++i)
            *(volatile uint16_t *)(global + offsets[i]) = 3;
        sim1_write32(D + 0x1778, 0);
    }
}

/* 0x2200a4c0: retain the x87 instruction/store order. Only the low WORD of
 * each four-byte fixed-point slot is written; the padding is live old state.
 */
static void sim1_angles(void)
{
    uint32_t i;
    for (i = 0; i < 3; ++i) {
        __asm__ volatile("fildl %0\n\tfmull %1\n\tfmull %2\n\tfld %%st(0)\n\t"
                         "fsin\n\tfstpl %3\n\tfcos\n\tfstpl %4"
            : : "m"(*(int32_t *)(D + 0x1550 + 4 * i)), "m"(*(double *)(D + 0x680)),
                "m"(*(double *)(D + 0x690)), "m"(*(double *)(D + 0x1568 + 16 * i)),
                "m"(*(double *)(D + 0x1560 + 16 * i)) : "st", "st(1)", "memory");
    }
    for (i = 0; i < 6; ++i) {
        uint32_t source = 0x1560 + (i / 2) * 16 + (i & 1 ? 0 : 8);
        __asm__ volatile("fldl %0\n\tfmull %1\n\tfistps %2"
            : : "m"(*(double *)(D + source)), "m"(*(double *)(D + 0x6e0)),
                "m"(*(int16_t *)(D + 0x1590 + 4 * i)) : "st", "memory");
    }
}

static void sim1_finish_record_read(struct sim1_registers *registers)
{
    int32_t x, y;
    uintptr_t arguments[2];
    uint16_t angle, difference;
    sim1_angles();
    /* 0x22011620 -> 0x2200a599, with UTIL +0x3c's real register contract. */
    registers->ecx = sim1_read32(D + 0x1398);
    registers->edx = sim1_read32(D + 0x138c);
    __asm__ volatile("fldl %2\n\tfmull %4\n\tfistpl %0\n\t"
                     "fldl %3\n\tfmull %4\n\tfistpl %1\n\tfwait"
        : "=m"(x), "=m"(y)
        : "m"(*(double *)(D + 0x1388)), "m"(*(double *)(D + 0x1398)),
          "m"(*(double *)(D + 0x6d8)) : "st", "memory");
    registers->eax = (uintptr_t)sim1_dependency(8);
    arguments[0] = (uint32_t)x; arguments[1] = (uint32_t)y;
    sim1_import_call(registers, 8, 0x3c, 2, arguments);
    angle = (uint16_t)(0x4000 - (uint16_t)registers->eax);
    registers->eax = (registers->eax & 0xffff0000) | angle;
    registers->edx = (registers->edx & 0xffff0000) | angle;
    difference = (uint16_t)(*(volatile uint16_t *)(D + 0x3900) - angle);
    registers->ecx = (registers->ecx & 0xffff0000) | difference;
    *(volatile uint16_t *)(D + 0x1518) = difference;
}

static void sim1_read_pair(const uint8_t *record, uint32_t source, uint32_t destination)
{
    uint32_t low = sim1_read32(record + source);
    uint32_t high = sim1_read32(record + source + 4);
    sim1_write32(D + destination, low);
    sim1_write32(D + destination + 4, high);
}

void sim1_velocity(void);

static void sim1_clear_extended_state(uint32_t clear_velocity)
{
    static const uint32_t offsets[] = {
        0x1358, 0x1360, 0x1368, 0x1328, 0x1330, 0x1338,
        0x1418, 0x1420, 0x1428, 0x1760
    };
    uint32_t i;
    /* Modern fallback clears these before +1760; the legacy reader does not
     * clear them before its transform overwrites them. Keep the store order.
     */
    for (i = 0; i < 9; ++i) {
        sim1_write32(D + offsets[i], 0);
        sim1_write32(D + offsets[i] + 4, 0);
    }
    if (clear_velocity) {
        for (i = 0; i < 3; ++i) {
            sim1_write32(D + 0x13b8 + 8 * i, 0);
            sim1_write32(D + 0x13bc + 8 * i, 0);
        }
    }
    sim1_write32(D + 0x1760, 0);
    sim1_write32(D + 0x1764, 0);
}

static void sim1_reconstruct_velocity(struct sim1_registers *registers)
{
    uintptr_t arguments[9];
    uint32_t i;
    sim1_angles();
    for (i = 0; i < 6; ++i)
        arguments[i] = sim1_read32(D + 0x13a0 + 4 * i);
    arguments[6] = (uintptr_t)(D + 0x13b8);
    arguments[7] = (uintptr_t)(D + 0x13c0);
    arguments[8] = (uintptr_t)(D + 0x13c8);
    sim1_import(registers, (uintptr_t)sim1_velocity, 9, arguments);
}

/* 0x22011290: only size exactly 0xc0 selects the extended fields. Retail
 * reads the base through +0x54 even when the advertised size is smaller.
 */
void sim1_record_read_core(struct sim1_registers *registers, const uint8_t *record, uint32_t size)
{
    sim1_read_pair(record, 0, 0x1538);
    /* MSVCRT _ftol consumes ST0, returns EDX:EAX, and retains its own precise
     * x87 exception behavior. No intermediate double store may be inserted.
     */
    __asm__ volatile("fldl 8(%0)\n\tfldl 0x1538(%1)\n\tfmull 0x6d8(%1)\n\t"
        "fxch %%st(1)\n\tfstpl 0x1540(%1)\n\tfldl 16(%0)\n\tfxch %%st(1)\n\t"
        "fmull 0x678(%1)\n\tfxch %%st(1)\n\tfstpl 0x1548(%1)\n\t"
        "call __ftol\n\tmovl %%eax, 0x1550(%1)\n\tfldl 0x1540(%1)\n\t"
        "fmull 0x6d8(%1)\n\tfmull 0x678(%1)\n\tcall __ftol\n\t"
        "movl %%eax, 0x1554(%1)\n\tfldl 0x1548(%1)\n\tfmull 0x6d8(%1)\n\t"
        "fmull 0x678(%1)\n\tcall __ftol\n\tmovl %%eax, 0x1558(%1)"
        : : "r"(record), "r"(D) : "eax", "ecx", "edx", "st", "st(1)", "memory", "cc");
    sim1_read_pair(record, 0x18, 0x13e8);
    sim1_read_pair(record, 0x20, 0x13f0);
    sim1_read_pair(record, 0x28, 0x13f8);
    sim1_read_pair(record, 0x30, 0x13a0);
    sim1_read_pair(record, 0x38, 0x13a8);
    sim1_read_pair(record, 0x40, 0x13b0);
    sim1_read_pair(record, 0x48, 0x15a8);
    sim1_write32(D + 0x1d18, sim1_read32(record + 0x50));
    *(volatile uint8_t *)(sim1_dependency(1) + 0x366) = record[0x54];
    if (size != 0xc0) {
        sim1_clear_extended_state(1);
        sim1_reconstruct_velocity(registers);
        sim1_finish_record_read(registers);
        return;
    }
    sim1_read_pair(record, 0x58, 0x1358);
    sim1_read_pair(record, 0x60, 0x1360);
    sim1_read_pair(record, 0x68, 0x1368);
    sim1_read_pair(record, 0x70, 0x1328);
    sim1_read_pair(record, 0x78, 0x1330);
    sim1_read_pair(record, 0x80, 0x1338);
    sim1_read_pair(record, 0x88, 0x1418);
    sim1_read_pair(record, 0x90, 0x1420);
    sim1_read_pair(record, 0x98, 0x1428);
    sim1_read_pair(record, 0xa0, 0x13b8);
    sim1_read_pair(record, 0xa8, 0x13c0);
    sim1_read_pair(record, 0xb0, 0x13c8);
    sim1_read_pair(record, 0xb8, 0x1760);
    sim1_finish_record_read(registers);
}

/* 0x22010df0: record 0x5a, split-word orientation and velocities. */
void sim1_legacy_read_core(struct sim1_registers *registers, const uint8_t *record)
{
    static const uint32_t rate_sources[] = {6, 10, 8};
    uint32_t i, packed;
    int32_t value;
    uint64_t wide;
    /* Keep the product of the two scales on the x87 stack across all three
     * unsigned qword conversions. A temporary double would round too soon.
     */
    __asm__ volatile("subl $8, %%esp\n\tmovl $0, 4(%%esp)\n\t"
        "fldl 0x690(%1)\n\tfmull 0x680(%1)\n\t"
        "movzwl (%0), %%eax\n\tshll $16, %%eax\n\tmovzwl 0x32(%0), %%ecx\n\t"
        "addl %%ecx, %%eax\n\tmovl %%eax, (%%esp)\n\tfildq (%%esp)\n\t"
        "movl %%eax, 0x1550(%1)\n\tfmul %%st(1), %%st\n\tfstpl 0x1538(%1)\n\t"
        "movzwl 2(%0), %%eax\n\tshll $16, %%eax\n\tmovzwl 0x30(%0), %%ecx\n\t"
        "addl %%ecx, %%eax\n\tmovl %%eax, (%%esp)\n\tmovl %%eax, 0x1554(%1)\n\t"
        "fildq (%%esp)\n\tfmul %%st(1), %%st\n\tfstpl 0x1540(%1)\n\t"
        "movzwl 4(%0), %%eax\n\tshll $16, %%eax\n\tmovzwl 0x34(%0), %%ecx\n\t"
        "addl %%ecx, %%eax\n\tmovl %%eax, (%%esp)\n\tmovl %%eax, 0x1558(%1)\n\t"
        "fildq (%%esp)\n\tfmul %%st(1), %%st\n\tfstpl 0x1548(%1)\n\t"
        "fstp %%st(0)\n\taddl $8, %%esp"
        : : "r"(record), "r"(D) : "eax", "ecx", "st", "st(1)", "memory", "cc");
    for (i = 0; i < 3; ++i) {
        value = (int16_t)*(const volatile uint16_t *)(record + rate_sources[i]);
        __asm__ volatile("fildl %0\n\tfmull %1\n\tfstpl %2"
            : : "m"(value), "m"(*(double *)(D + 0x680)),
                "m"(*(double *)(D + 0x13e8 + 8 * i)) : "st", "memory");
    }
    for (i = 0; i < 3; ++i) {
        packed = ((uint32_t)*(const volatile uint16_t *)(record + 0xc + 2 * i) << 16) +
                 *(const volatile uint16_t *)(record + 0x12 + 2 * i);
        value = (int32_t)packed;
        __asm__ volatile("fildl %0\n\tfmull %1\n\tfmull %2\n\tfstpl %3"
            : : "m"(value), "m"(*(double *)(D + 0x720)), "m"(*(double *)(D + 0x690)),
                "m"(*(double *)(D + 0x13a0 + 8 * i)) : "st", "memory");
    }
    wide = (uint64_t)sim1_read32(record + 0x3c);
    __asm__ volatile("fildq %0\n\tfmull %1\n\tfstpl %2"
        : : "m"(wide), "m"(*(double *)(D + 0x690)),
            "m"(*(double *)(D + 0x15a8)) : "st", "memory");
    sim1_write32(D + 0x1d18, sim1_read32(record + 0x18));
    sim1_clear_extended_state(0);
    *(volatile uint8_t *)(sim1_dependency(1) + 0x366) = 0;
    sim1_reconstruct_velocity(registers);
    sim1_finish_record_read(registers);
}

/* 0x22010cc0: use retained state first, otherwise prefer a modern situation
 * record over a legacy one. MAIN's returned buffer is owned here and freed;
 * GLOBAL's retained buffer is borrowed and survives the restore.
 */
void sim1_restore_state_core(struct sim1_registers *registers)
{
    uintptr_t arguments[3], record = 0;
    uint32_t size = 0;
    registers->eax = (uintptr_t)sim1_dependency(1);
    registers->eax = sim1_read32(sim1_dependency(1) + 0x11d8);
    if (registers->eax != 0) {
        sim1_record_read_core(registers, (const uint8_t *)(uintptr_t)registers->eax, 0xc0);
        return;
    }
    registers->eax = (uintptr_t)sim1_dependency(5);
    registers->ecx = (uintptr_t)&size;
    registers->edx = (uintptr_t)&record;
    arguments[0] = 0x38e;
    arguments[1] = (uintptr_t)&record;
    arguments[2] = (uintptr_t)&size;
    sim1_import_call(registers, 5, 0x6c, 3, arguments);
    if (registers->eax == 0) {
        sim1_record_read_core(registers, (const uint8_t *)record, size);
    } else {
        registers->ecx = (uintptr_t)sim1_dependency(5);
        registers->edx = (uintptr_t)&size;
        registers->eax = (uintptr_t)&record;
        arguments[0] = 0x5a;
        sim1_import_call(registers, 5, 0x6c, 3, arguments);
        if (registers->eax != 0) return;
        sim1_legacy_read_core(registers, (const uint8_t *)record);
    }
    registers->ecx = (uintptr_t)sim1_dependency(0);
    registers->eax = (uintptr_t)&record;
    arguments[0] = (uintptr_t)&record;
    sim1_import_call(registers, 0, 0x214, 1, arguments);
}

void sim1_situation_core(struct sim1_registers *registers, uint32_t message, uint8_t *record)
{
    registers->eax = message;
    if (message == 0x22) {
        registers->ecx = (uintptr_t)record;
        sim1_write32(D + 0x1778, 1);
        registers->eax = sim1_read32(record);
        if (registers->eax == 0x38e)
            sim1_record_read_core(registers, (const uint8_t *)(uintptr_t)sim1_read32(record + 12),
                                  sim1_read32(record + 8));
        else if (registers->eax == 0x5a)
            sim1_legacy_read_core(registers, (const uint8_t *)(uintptr_t)sim1_read32(record + 12));
    } else if (message == 0x23) {
        sim1_save_situation_core(registers);
    }
}
