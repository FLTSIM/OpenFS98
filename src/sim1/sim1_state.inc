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

/* 0x22011290: modern 0xc0-byte record. The non-c0 legacy reconstruction path
 * is intentionally unsupported until its velocity-transform helper is done.
 */
void sim1_record_read_core(struct sim1_registers *registers, const uint8_t *record, uint32_t size)
{
    if (size != 0xc0) sim1_unsupported();
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
            sim1_unsupported(); /* legacy reader remains */
    } else if (message == 0x23) {
        sim1_save_situation_core(registers);
    }
}
