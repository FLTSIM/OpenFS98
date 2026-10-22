/* Matching 1000 model lengths below 0x6c wrap the copy count. Guard pages
 * bound actual reads/writes while preserving the requested count and prefix. */
struct bounds_results {
    uint32_t calls, size, result, fault, source, destination;
    uint8_t player[256], model[128];
};
static struct bounds_results s_bounds;
static uint8_t *s_bounds_source, *s_bounds_destination;
static int32_t STDCALL bounds_copy(void *destination, const void *source, uint32_t size)
{
    uint32_t index;
    volatile uint8_t *out = destination;
    const volatile uint8_t *in = source;
    s_bounds.calls++; s_bounds.size = size;
    s_bounds.source = (const uint8_t *)source - s_bounds_source;
    s_bounds.destination = (uint8_t *)destination - s_bounds_destination;
    s_dialog_host_access = 1;
    for (index = 0; index < size; index++) out[index] = in[index];
    s_dialog_host_access = 0;
    return s_dialog_number & 1 ? 0x12345678 : 0;
}
static void run_bounds(const struct multip_module *module, uint32_t number, struct bounds_results *results)
{
    uint32_t length = number % 112, source_room = number & 1 ? 16 : 64;
    uint32_t destination_room = number & 1 ? 64 : 16;
    uint8_t *packet = s_bounds_source + 4096 - 0x14 - source_room;
    uint8_t *model = s_bounds_destination + 4096 - 0x6c - destination_room;
    typedef int32_t (STDCALL *bounds_handler_fn)(uint32_t, const void *, uint32_t);
    dialog_setup(module, number); memset(&s_bounds, 0, sizeof(s_bounds));
    memset(s_bounds_source, 0x35, 4096); memset(s_bounds_destination, 0xa7, 4096);
    if (length == 110) length = 0xffffffffu;
    if (length == 111) length = 0x80000000u;
    put_u32(packet, 0, 0x1000); put_u32(packet, 0xc, number / 112 % 3);
    put_u32(packet, 0x10, length);
    memset(s_dialog_local, 0, 256); put_u32(s_dialog_local, 4, 0x1234);
    put_u32(s_dialog_local, 0x28, number / 2016 % 2 ? 0 : (uint32_t)(uintptr_t)model);
    put_u32(s_dialog_local, 0x2c, number / 336 % 2 ? length + 1 : length);
    put_u32(s_dialog_local, 0x50, 1);
    put_u32(module->data, M_LOCAL_PLAYER, 0); put_u32(module->data, M_PLAYER_COUNT, 1);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_dialog_local);
    put_u32(s_dialog_host, 0x21c, (uint32_t)(uintptr_t)bounds_copy);
    if (__builtin_setjmp(s_dialog_jump) == 0) {
        uint32_t id = number / 672 % 3 ? 0x1234 : 0xdead;
        s_dialog_active = 1;
        s_bounds.result = number & 2 ? dispatch_painted(send_internal(module, 217), id, packet, number % 0x80, 0) :
            ((bounds_handler_fn)send_internal(module, 136))(id, packet, number % 0x80);
    } else s_bounds.fault = s_dialog_outcome;
    s_dialog_active = 0; s_dialog_host_access = 0;
    memcpy(s_bounds.player, s_dialog_local, 256);
    memcpy(s_bounds.model, model, 0x6c + (destination_room < 20 ? destination_room : 20));
    memcpy(results, &s_bounds, sizeof(*results));
}
static uint32_t test_bounds(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    struct bounds_results retail_results, our_results;
    uint32_t number, bad = 0, faults = 0, wrapped = 0;
    DWORD protection;
    void *handler = AddVectoredExceptionHandler(1, dialog_fault);
    s_bounds_source = VirtualAlloc(NULL, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    s_bounds_destination = VirtualAlloc(NULL, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (handler == NULL || s_bounds_source == NULL || s_bounds_destination == NULL ||
        !VirtualProtect(s_bounds_source + 4096, 4096, PAGE_NOACCESS, &protection) ||
        !VirtualProtect(s_bounds_destination + 4096, 4096, PAGE_NOACCESS, &protection)) exit(2);
    for (number = 0; number < scenarios; number++) {
        multip_test_context("bounds", "retail", number); run_bounds(retail, number, &retail_results);
        multip_test_context("bounds", "ours", number); run_bounds(ours, number, &our_results);
        faults += retail_results.fault != 0;
        wrapped += retail_results.calls != 0 && retail_results.size > 0x80000000u;
        if (memcmp(&retail_results, &our_results, sizeof(our_results)) != 0) {
            if (bad < 4) printf("bounds scenario %lu differs\n", (unsigned long)number);
            bad++;
        }
    }
    RemoveVectoredExceptionHandler(handler);
    VirtualFree(s_bounds_source, 0, MEM_RELEASE); VirtualFree(s_bounds_destination, 0, MEM_RELEASE);
    if (scenarios >= 20000 && (faults == 0 || wrapped == 0)) exit(2);
    printf("bounds coverage: %lu wrapped copies, %lu guard faults\n", (unsigned long)wrapped, (unsigned long)faults);
    printf("MULTIP guarded packet boundaries: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
