/* Aircraft file ownership and initialization, retail 0x2200ae90,
 * 0x22001060 and 0x22010c40. The large record decoder at 0x2200af90 is a
 * generated boundary in the complete build, and a trap in partial tests.
 */
#ifdef SIM1_AIR_TEST
uintptr_t sim1_air_parser_fixture;
#endif

static void sim1_parse_air_boundary(struct sim1_registers *registers, const void *data, uint32_t size)
{
#ifdef SIM1_COMPLETE
    extern void sim1_entry_af90(void);
    uintptr_t arguments[2] = {(uintptr_t)data, size};
    sim1_import(registers, (uintptr_t)sim1_entry_af90, 2, arguments);
    return;
#endif
#ifdef SIM1_AIR_TEST
    if (sim1_air_parser_fixture != 0) {
        uintptr_t arguments[2] = {(uintptr_t)data, size};
        sim1_import(registers, sim1_air_parser_fixture, 2, arguments);
        return;
    }
#endif
    (void)registers; (void)data; (void)size;
    sim1_unsupported();
}

void sim1_load_air_core(struct sim1_registers *registers)
{
    char path[0x100];
    uintptr_t arguments[4], file = 0, data = 0;
    uint32_t size = 0, got = 0, result = 0;
    registers->ecx = (uintptr_t)(sim1_dependency(1) + 0x11e0);
    registers->edx = (uintptr_t)sim1_dependency(2);
    registers->eax = (uintptr_t)path;
    arguments[0] = registers->ecx;
    arguments[1] = 2;
    arguments[2] = (uintptr_t)path;
    arguments[3] = sizeof path;
    sim1_import_call(registers, 2, 0x30, 4, arguments);
    registers->edx = (uintptr_t)sim1_dependency(0);
    registers->eax = (uintptr_t)path;
    registers->ecx = (uintptr_t)&file;
    arguments[0] = (uintptr_t)&file;
    arguments[1] = (uintptr_t)path;
    arguments[2] = 0xf;
    arguments[3] = 0;
    sim1_import_call(registers, 0, 0x17c, 4, arguments);
    if (registers->eax != 0) { registers->eax = 0; return; }
    registers->ecx = file;
    registers->edx = (uintptr_t)sim1_dependency(0);
    registers->eax = (uintptr_t)&size;
    arguments[0] = file;
    arguments[1] = (uintptr_t)&size;
    sim1_import_call(registers, 0, 0x1a4, 2, arguments);
    if (registers->eax != 0) {
        /* RETAIL BUG (kept): SIM1-AIR-SIZE-HANDLE -- the opened file is
         * abandoned on this exit instead of being passed to HOST +0x13c. */
        registers->eax = 0;
        return;
    }
    registers->ecx = size;
    registers->edx = (uintptr_t)sim1_dependency(0);
    registers->eax = (uintptr_t)&data;
    arguments[0] = 1;
    arguments[1] = size;
    arguments[2] = (uintptr_t)&data;
    arguments[3] = 0;
    sim1_import_call(registers, 0, 0x20c, 4, arguments);
    registers->edx = data;
    registers->eax = file;
    registers->ecx = (uintptr_t)sim1_dependency(0);
    arguments[0] = file;
    arguments[1] = data;
    arguments[2] = size;
    arguments[3] = (uintptr_t)&got;
    sim1_import_call(registers, 0, 0x19c, 4, arguments);
    if (registers->eax == 0 && got == size) {
        registers->eax = size;
        registers->ecx = got;
        registers->edx = data;
        sim1_parse_air_boundary(registers, (const void *)data, size);
        result = registers->eax;
    }
    registers->ecx = (uintptr_t)sim1_dependency(0);
    registers->eax = (uintptr_t)&data;
    arguments[0] = (uintptr_t)&data;
    sim1_import_call(registers, 0, 0x214, 1, arguments);
    registers->eax = (uintptr_t)sim1_dependency(0);
    registers->edx = file;
    arguments[0] = file;
    sim1_import_call(registers, 0, 0x13c, 1, arguments);
    registers->eax = result;
}

void sim1_publish_core(struct sim1_registers *registers)
{
    uintptr_t arguments[3];
    uint32_t eax, ecx, edx;
    registers->eax = (uintptr_t)(sim1_dependency(1) + 0x560);
    registers->ecx = (uintptr_t)sim1_dependency(0);
    arguments[0] = (uintptr_t)(D + 0x1730);
    arguments[1] = registers->eax;
    arguments[2] = 0x24;
    sim1_import_call(registers, 0, 0x21c, 3, arguments);
    eax = registers->eax; ecx = registers->ecx; edx = registers->edx;
    /* Retail preloads each next product before storing the previous AX.
     * Capture _ftol's entire volatile return, not just the four WORD stores.
     */
    __asm__ volatile("fldl 0x13b8(%3)\n\tfmull 0x728(%3)\n\tcall __ftol\n\t"
        "fldl 0x13c0(%3)\n\tfmull 0x728(%3)\n\tmovw %%ax, 0x1754(%3)\n\t"
        "call __ftol\n\tfldl 0x13c8(%3)\n\tfmull 0x728(%3)\n\t"
        "movw %%ax, 0x1756(%3)\n\tcall __ftol\n\tfldl 0x13b0(%3)\n\t"
        "fmull 0x728(%3)\n\tmovw %%ax, 0x1758(%3)\n\tcall __ftol\n\t"
        "movw %%ax, 0x175a(%3)"
        : "+a"(eax), "+c"(ecx), "+d"(edx) : "r"(D)
        : "st", "st(1)", "memory", "cc");
    registers->eax = eax; registers->ecx = ecx; registers->edx = edx;
}

void sim1_init_core(struct sim1_registers *registers)
{
    uintptr_t arguments[2];
    sim1_restore_state_core(registers);
    registers->eax = (uintptr_t)sim1_dependency(3);
    arguments[0] = 1; arguments[1] = (uintptr_t)sim1_chain_tick;
    sim1_import_call(registers, 3, 0x18, 2, arguments);
    registers->ecx = (uintptr_t)sim1_dependency(3);
    arguments[0] = 5; arguments[1] = (uintptr_t)sim1_chain_frame;
    sim1_import_call(registers, 3, 0x18, 2, arguments);
    registers->edx = (uintptr_t)sim1_dependency(3);
    arguments[0] = 0x14; arguments[1] = (uintptr_t)sim1_situation_callback;
    sim1_import_call(registers, 3, 0x18, 2, arguments);
    sim1_load_air_core(registers);
    sim1_publish_core(registers); /* retail ignores the aircraft-load result */
}
