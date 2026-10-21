/* Public multiplayer options dialog, .text+0x39a0..+0x3d57. */
static int32_t multip_options_init(void)
{
    M32(0x1e0) = 0; M32(0xc08) = M32(0xac0);
    UTIL_FN(0x90, multip_exchange_read_fn)((void *)(uintptr_t)(D + 0x288), 8,
        (void *)(uintptr_t)(D + 0x1e0));
    M32(0x210) = A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x764) != 0 &&
        A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x84c) == 0;
    return 0;
}
/* +0x1c (.text+0x39a0 tail-jumps here). */
static int32_t multip_options_dialog(void)
{
    if (multip_options_init() != 0) return 0;
    if (HOST_FN(0x10c, multip_dialog_fn)(0x110b, (void *)(uintptr_t)(D + 0x1e0)) != 0) return 0;
    return (int32_t)M32(0x1e0);
}
static int32_t FS98_STDCALL multip_options_accept(void *dialog)
{
    (void)dialog;
    if (UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x288), 8,
        (void *)(uintptr_t)(D + 0x1e0), 0) != 0) return -1;
    if (M32(M_CONNECTED) != 0) {
        if (multip_player_publish_name() != 0) return -1;
        if (M32(0xc08) != M32(0xac0)) {
            if (M32(0xac0) == 0) { if (multip_session_set_observer() != 0) return -1; }
            else if (multip_session_set_player() != 0) return -1;
        }
    }
    return 0;
}
static int32_t FS98_STDCALL multip_options_weather(void *dialog)
{ (void)dialog; return multip_message_1013() != 0 ? -1 : 0; }
#if defined(__i386__)
/* Keep the weather packet's caller-stack tail at the retail nesting depth. */
__attribute__((naked)) static int32_t FS98_STDCALL multip_options_sync(void *dialog __attribute__((unused)))
{
    __asm__ volatile("pushl $0\n\tcall _multip_message_1014@4\n\tnegl %eax\n\tsbbl %eax, %eax\n\tret $4");
}
#else
static int32_t FS98_STDCALL multip_options_sync(void *dialog)
{ (void)dialog; return multip_message_1014(0) != 0 ? -1 : 0; }
#endif
static int32_t FS98_STDCALL multip_options_read(void *dialog)
{
    (void)dialog;
    return UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x288), 8,
        (void *)(uintptr_t)(D + 0x1e0), 1);
}
static int32_t FS98_STDCALL multip_options_details(void *dialog)
{
    void *description = NULL;
    uint8_t *record = (uint8_t *)dialog + 0x54;
    if (multip_session_get_description(&description) != 0) return -1;
    if (multip_session_record_init(description, record) != 0) return -1;
    M32(0x334) = A32(record, 0); M32(0x338) = ADDR(dialog);
    if (multip_details_dialog((void *)(uintptr_t)(D + 0x330)) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)(&description) != 0 ? -1 : 0;
}
static int32_t FS98_STDCALL multip_options_player_copy(void **players_out,
    uint32_t *count_out, void *counts_out, void *dialog)
{
    uint8_t *record = (uint8_t *)dialog + 0x54, *players;
    void *name = NULL;
    uint32_t size = 0, count;
    int32_t result;
    if (multip_session_enumerate_players(record, NULL, 0x10) != 0) return -1;
    do {
        uint32_t id = A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4);
        result = DP_FN(0x54, multip_dp_get_player_name_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY), id, name, &size);
        if ((uint32_t)result == 0x8877001e) {
            if (name == NULL) {
                if (HOST_FN(0x20c, multip_alloc_fn)(5, size, &name, 0) != 0) return -1;
            } else if (HOST_FN(0x210, multip_realloc_fn)(5, size, &name) != 0) return -1;
        } else if (result != 0) return -1;
    } while (result != 0);
    if (multip_player_names_callback(A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4),
        1, name, 0, record) == 0) return -1;
    if (HOST_FN(0x214, multip_free_fn)(&name) != 0) return -1;
    if (A32(record, 0x18) == 0) return -1;
    A32(record, 0x30) = A32(record, 0x38); A32(record, 0x34) = A32(record, 0x50);
    *count_out = A32(record, 0x30);
    if (HOST_FN(0x21c, multip_copy_fn)(counts_out, record + 0x20, 16) != 0) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, A32(record, 0x30) * 16, (void **)&players, 0) != 0) return -1;
    count = A32(record, 0x30);
    while (count != 0) {
        uint32_t offset = --count * 16;
        if (multip_player_name_record_copy(players + offset,
            (const uint8_t *)(uintptr_t)A32(record, 0x34) + offset) != 0) return -1;
    }
    *players_out = players;
    return 0;
}
