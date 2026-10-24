/* Initialization and the CHAIN haze transition callback. */
uint32_t STDCALL __attribute__((used)) resource_haze_tick_body(uint32_t first, uint32_t second, uint32_t entry_eax)
{
    uint16_t target = GLOBAL_HALF(0xcd2) && (int16_t)GLOBAL_HALF(0xe8a) < 0x12f3 ? GLOBAL_HALF(0xe8a) : 0x12f3;
    uint16_t previous = HALF(0xd50), delta, movement_integer, movement_fraction;
    uint32_t index;
    (void)first; (void)second;
    if (target != previous) { HALF(0xd56) = previous; HALF(0xd50) = target; }
    if (!(GLOBAL[0x365] & 8)) {
        delta = (uint16_t)(target - HALF(0xd54));
        if (!delta) return (entry_eax & 0xffff0000u) | previous;
        uint32_t elapsed = 300u * GLOBAL_HALF(0x2aa);
        int64_t product = (int64_t)(int16_t)HALF(0xd54) * elapsed * 2;
        uint32_t movement = (uint32_t)((uint64_t)product >> 8) & 0x7fffffffu;
        uint16_t change = (uint16_t)(target - HALF(0xd56));
        if ((int16_t)change < 0) change = (uint16_t)(0u - change);
        change = (uint16_t)((change >> 4) + 1);
        movement_integer = (uint16_t)(movement >> 16);
        movement_fraction = (uint16_t)movement;
        if (movement_integer > change) movement_integer = change;
        uint16_t distance = (int16_t)delta < 0 ? (uint16_t)(0u - delta) : delta;
        distance--;
        if ((int16_t)movement_integer <= (int16_t)distance) {
            movement = ((uint32_t)movement_integer << 16) | movement_fraction;
            WORD(0xd52) += (int16_t)delta < 0 ? 0u - movement : movement;
        } else HALF(0xd54) = target;
    } else HALF(0xd54) = target;
    uint16_t current = HALF(0xd54), accumulated = 0;
    for (index = 0x1d84;; index -= 4) {
        uint16_t difference = (uint16_t)(current - HALF(index));
        if ((int16_t)current > (int16_t)HALF(index)) {
            accumulated = (uint16_t)(accumulated + (uint32_t)difference * HALF(index + 2));
            current = HALF(index);
        }
        if (index == 0x1d6c) break;
    }
    GLOBAL_HALF(0xe88) = accumulated >> 1;
    return resource_haze_refresh();
}
EAX_ENTRY(resource_haze_tick, resource_haze_tick_body);

static uint32_t STDCALL module_init(void)
{
    uint32_t index, result;
    SYSTEM_INFO information;
    call_two register_tick = *(call_two *)((uintptr_t)WORD(0x14) + 0x18);
    palette_layout_body(0);
    resource_palette_default();
    GLOBAL_WORD(0xc10) = GLOBAL_WORD(0xc14) = ADDRESS(hg2d_image + 0x104);
    for (index = 0; index < 33; index++) BYTE(0x4135 + index) = BYTE(0x34c0 + BYTE(0x4135 + index) * 2);
    resource_remap_rows(0x4e00);
    for (index = 0x29a0; index < 0x29a4; index++) BYTE(index) = BYTE(0x34c0 + BYTE(index) * 2);
    result = register_tick(1, ADDRESS(resource_haze_tick));
    resource_haze_tick_body(0, 0, result);
    GetSystemInfo(&information);
    result = information.dwProcessorType;
    if (!information.wProcessorArchitecture && result >= 586) {
        result = WORD(0x5e00);
        if (!result) {
            DWORD previous_protection;
            /* The native renderer has no patched instructions. Retain the
             * retail capability probe and its observable result/selector. */
            result = VirtualProtect(POINTER(ADDRESS(affine_close) & 0xfffff000u), 0x1000, PAGE_EXECUTE_READWRITE, &previous_protection);
            WORD(0x5e00) = result;
        }
        if (result) WORD(0x4678) = 16;
    }
    return result;
}
