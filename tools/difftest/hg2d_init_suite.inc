/* Initialization probes and the callback registered with CHAIN +0x18. */
static uint32_t init_architecture, init_processor, init_protect_result, init_callback[2];

static void WINAPI init_system_info(SYSTEM_INFO *information)
{
    memset(information, 0, sizeof *information);
    information->wProcessorArchitecture = (uint16_t)init_architecture;
    information->dwProcessorType = init_processor;
    event(34); event(init_architecture); event(init_processor);
}

static BOOL WINAPI init_protect(void *address, SIZE_T size, DWORD protection, DWORD *previous)
{
    uint32_t expected = side ? WORD(images[side], 0x38 + 0x4c) & 0xfffff000u : 0x2078a000;
    CHECK(ADDRESS(address) == expected && size == 0x1000 && protection == PAGE_EXECUTE_READWRITE);
    event(35); event(size); event(protection);
    *previous = PAGE_EXECUTE_READ;
    return init_protect_result;
}

static uint32_t STDCALL init_register(uint32_t list, uint32_t callback)
{
    CHECK(list == 1 && callback);
    if (!side) CHECK(callback == 0x20788238);
    init_callback[side] = callback;
    event(36); event(list);
    return 0x55667788;
}

static void init_imports(HMODULE module)
{
    uint8_t *base = (uint8_t *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *descriptor = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    uint32_t found = 0;
    for (; descriptor->Name; descriptor++) {
        IMAGE_THUNK_DATA *lookup = (IMAGE_THUNK_DATA *)(base + descriptor->OriginalFirstThunk);
        IMAGE_THUNK_DATA *address = (IMAGE_THUNK_DATA *)(base + descriptor->FirstThunk);
        for (; lookup->u1.AddressOfData; lookup++, address++) {
            if (IMAGE_SNAP_BY_ORDINAL(lookup->u1.Ordinal)) continue;
            const char *name = (const char *)((IMAGE_IMPORT_BY_NAME *)(base + lookup->u1.AddressOfData))->Name;
            uint32_t replacement = !strcmp(name, "GetSystemInfo") ? ADDRESS(init_system_info) : !strcmp(name, "VirtualProtect") ? ADDRESS(init_protect) : 0;
            if (replacement) {
                DWORD previous;
                CHECK(VirtualProtect(address, sizeof *address, PAGE_READWRITE, &previous));
                address->u1.Function = replacement;
                CHECK(VirtualProtect(address, sizeof *address, previous, &previous));
                found++;
            }
        }
    }
    CHECK(found == 2);
}

static void run_initialization(void)
{
    static const uint32_t probes[][3] = {{0,585,0},{0,586,0},{0,586,1},{9,8664,1},{0,0,1},{1,586,1},{0,65535,1}};
    static const uint16_t targets[] = {0,1,255,256,257,0x12f2,0x12f3,0x12f4,0x7fff,0x8000,0xffff};
    static const uint16_t elapsed[] = {0,1,2,10,255,256,1000,0x7fff,0x8000,0xffff};
    uint32_t index, result;
    palette_reset();
    resource_compare = 1;
    WORD(images[side], 0x1c) = ADDRESS(fe);
    fe[0x1c / 4] = ADDRESS(resource_fe_load);
    host[0x17c / 4] = ADDRESS(resource_open); host[0x19c / 4] = ADDRESS(resource_read); host[0x13c / 4] = ADDRESS(resource_close);
    chain[0x18 / 4] = ADDRESS(init_register);
    init_architecture = probes[scenario % 7][0]; init_processor = probes[scenario % 7][1]; init_protect_result = probes[scenario % 7][2];
    resource_fe_success = resource_haze_success = 1;
    resource_open_success = scenario & 1; resource_read_size = scenario & 2 ? 0x15c : 137;
    GLOBAL_HALF(0xcc0) = scenario % 24;
    GLOBAL_HALF(0xcba) = 0x7f00;
    GLOBAL_HALF(0xcd2) = scenario & 1;
    GLOBAL_HALF(0xe8a) = targets[scenario % 11];
    GLOBAL_HALF(0x2aa) = elapsed[scenario % 10];
    global[0x365] = scenario & 2 ? 8 : 0;
    result = call(0x04, 0, 0, 0);
    CHECK(result == (!init_architecture && init_processor >= 586 ? init_protect_result : init_processor));
    CHECK(WORD(global, 0xc10) == ADDRESS(images[side] + 0x104) && WORD(global, 0xc14) == WORD(global, 0xc10));
    CHECK(WORD(images[side], 0x4678) == (!init_architecture && init_processor >= 586 && init_protect_result ? 16u : 0u));
    for (index = 0; index < 40; index++) {
        GLOBAL_HALF(0xcd2) = index % 3 != 0;
        GLOBAL_HALF(0xe8a) = targets[(scenario + index) % 11];
        GLOBAL_HALF(0x2aa) = elapsed[(scenario * 3 + index) % 10];
        global[0x365] = index % 4 == 0 ? 8 : 0;
        result = invoke(init_callback[side], 0, 0, 0, 0xaabbccdd, 2);
        compare(result);
        if (index % 9 == 0) {
            result = invoke(init_callback[side], 0, 0, 0, 0xaabbccdd, 2);
            compare(result);
        }
    }
    /* A cached protection result skips the probe on repeated initialization. */
    WORD(images[side], 0x5e00) = 0x12345678;
    init_architecture = 0; init_processor = 586;
    CHECK(call(0x04, 0, 0, 0) == 0x12345678);
    fe[0x1c / 4] = ADDRESS(fe_load);
    resource_compare = 0;
}
