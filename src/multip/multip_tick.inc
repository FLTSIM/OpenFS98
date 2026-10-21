/* Four CHAIN 0x02 callbacks, .text+0x2460..+0x27b2. */
typedef void (FS98_STDCALL *multip_range48_fn)(const void *, const void *,
    int32_t, int32_t *, int16_t *, uint8_t *);

/* .text+0x2730/+0x2790 -- panel ADF bearing and autopilot heading. */
int32_t FS98_STDCALL multip_guidance_adf(uint32_t bearing)
{
    uint8_t *global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    uint32_t own_heading = A32(global, 0x580) >> 16;

    A32(global, 0xc64) = 1;
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    global[0x389] |= 0x40;
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    *(uint16_t *)(uintptr_t)(A32((void *)(uintptr_t)A32(global, 0x384), 8) + 0x18) = 1;
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    *(uint16_t *)(void *)(global + 0xc6a) = (uint16_t)(bearing - own_heading);
    return 0;
}
int32_t FS98_STDCALL multip_guidance_autopilot(uint32_t heading, uint32_t bearing)
{
    A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x7d4) = heading;
    *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x7cc) = (uint16_t)bearing;
    return 0;
}

/* .text+0x2630 -- GLOBAL+0x84c disables autopilot lock before other guards. */
uint32_t FS98_STDCALL multip_guidance_callback(uint32_t code, uint32_t argument)
{
    uint8_t *global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL), *player, *state;
    int32_t distance;
    int16_t bearing;
    uint8_t octant;
    uint32_t result = ADDR(global);

    (void)code; (void)argument;
    if (A32(global, 0x84c) != 0) M32(0xab8) = 0;
    if (M32(0xab0) == 0 && M32(0xab8) == 0)
        return result;
    if (M32(M_PLAYER_MODE) == 0) {
        global[0x389] = 0;
        global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
        *(uint16_t *)(uintptr_t)(A32((void *)(uintptr_t)A32(global, 0x384), 8) + 0x18) = 0;
        return ADDR(global);
    }
    player = multip_selection_get();
    if (player == NULL) return 0;
    state = *(uint8_t **)(void *)(player + 0x28);
    if (state == NULL) return 0;
    UTIL_FN(0x64, multip_range48_fn)(global + 0x560, state + 4, 3, &distance, &bearing, &octant);
    if (M32(0xab0) != 0) {
        result = (uint32_t)multip_guidance_adf((uint16_t)bearing);
        if (result != 0) return result;
    } else {
        global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
        global[0x389] = 0;
        global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
        *(uint16_t *)(uintptr_t)(A32((void *)(uintptr_t)A32(global, 0x384), 8) + 0x18) = 0;
        result = ADDR(global);
    }
    if (M32(0xab8) != 0) {
        state = *(uint8_t **)(void *)(player + 0x28);
        return (uint32_t)multip_guidance_autopilot(
            ((uint32_t)*(uint16_t *)(void *)(state + 0x16)) + (A32(state, 0x18) << 16),
            (uint16_t)bearing);
    }
    return result;
}

/* .text+0x2550 -- compare the extended sum before discarding it. The raw
 * return contains fnstsw AX and the caller's original upper EAX bits. */
static uint32_t __attribute__((used, noinline)) multip_interval_callback_impl(uint32_t incoming)
{
    uint16_t status;

    __asm__ volatile("fldl %2\n\tfaddl %1\n\tfstl %1\n\tfcompl %3\n\tfnstsw %0"
        : "=a"(status), "+m"(MD(0xa50))
        : "m"(MD(0x158)), "m"(*(double *)(void *)(s_multip_rdata + 0x10)) : "st");
    if ((status & 0x4100) == 0) MD(0xa50) = 100.0;
    return (incoming & 0xffff0000u) | status;
}

/* .text+0x2590 -- sync every ticks-per-sync+1 callbacks, with wrapping count. */
static uint32_t __attribute__((used, noinline)) multip_sync_callback_impl(uint32_t incoming)
{
    uint32_t count, result;
    uint8_t *players;

    if (M32(0x164) == 0) return incoming;
    result = M32(0x944) + 1;
    M32(0x944) = result;
    if (result <= M32(0x164)) return result;
    M32(0x944) = 0;
    count = M32(M_PLAYER_COUNT);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    result = ADDR(players);
    while (count != 0) {
        result = (uint32_t)multip_message_1017(players + --count * 256);
        if (result != 0) break;
    }
    return result;
}

/* .text+0x25f0 -- clear the pending weather notification before sending it. */
static uint32_t __attribute__((used, noinline)) multip_maintenance_callback_impl(uint32_t padding)
{
    if (M32(0xf1c) != 0) M32(0xf1c)--;
    if (M32(0xa68) == 0) return 0;
    M32(0xa68) = 0;
    return (uint32_t)(M32(0xaa8) != 0 ? multip_message_1014_impl(0, padding) : multip_message_1013());
}

#if defined(__i386__)
#define MULTIP_TICK_SHIM(name, expression) \
    __attribute__((naked)) uint32_t FS98_STDCALL name( \
        uint32_t code __attribute__((unused)), uint32_t argument __attribute__((unused))) \
    { __asm__ volatile(expression); }
MULTIP_TICK_SHIM(multip_interval_callback,
    "pushl %eax\n\tcall _multip_interval_callback_impl\n\taddl $4, %esp\n\tret $8")
MULTIP_TICK_SHIM(multip_sync_callback,
    "pushl %eax\n\tcall _multip_sync_callback_impl\n\taddl $4, %esp\n\tret $8")
MULTIP_TICK_SHIM(multip_maintenance_callback,
    "movl -0xb0(%esp), %eax\n\tpushl %eax\n\tcall _multip_maintenance_callback_impl\n\taddl $4, %esp\n\tret $8")
#undef MULTIP_TICK_SHIM
#else
uint32_t FS98_STDCALL multip_interval_callback(uint32_t code, uint32_t argument)
{ (void)code; (void)argument; return multip_interval_callback_impl(0); }
uint32_t FS98_STDCALL multip_sync_callback(uint32_t code, uint32_t argument)
{ (void)code; (void)argument; return multip_sync_callback_impl(0); }
uint32_t FS98_STDCALL multip_maintenance_callback(uint32_t code, uint32_t argument)
{ (void)code; (void)argument; return multip_maintenance_callback_impl(0); }
#endif

/* .text+0x2460/+0x24d0 -- registration sets its flag first; teardown clears
 * it only after all four callbacks have been removed in reverse order. */
int32_t multip_tick_start(void)
{
    M32(0x940) = 1;
    if (CHAIN_FN(0x18, multip_chain_register_fn)(2, multip_sync_callback) != 0) return -1;
    if (CHAIN_FN(0x18, multip_chain_register_fn)(2, multip_interval_callback) != 0) return -1;
    if (CHAIN_FN(0x18, multip_chain_register_fn)(2, multip_maintenance_callback) != 0) return -1;
    return CHAIN_FN(0x18, multip_chain_register_fn)(2, multip_guidance_callback) != 0 ? -1 : 0;
}
int32_t multip_tick_stop(void)
{
    if (M32(0x940) != 0) {
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(2, multip_guidance_callback) != 0) return -1;
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(2, multip_maintenance_callback) != 0) return -1;
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(2, multip_interval_callback) != 0) return -1;
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(2, multip_sync_callback) != 0) return -1;
        M32(0x940) = 0;
    }
    return 0;
}
