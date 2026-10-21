/* Shared player-details dialog 0x110c, .text+0x7b10..+0x805d. */
typedef int32_t (FS98_STDCALL *multip_load_string_fn)(uint32_t, char **);
typedef int32_t (FS98_STDCALL *multip_details_get_fn)(void **, uint32_t *, void *, void *);
static int32_t FS98_STDCALL multip_details_read(void *dialog)
{
    void *description;
    UTIL_FN(0x90, multip_exchange_read_fn)((void *)(uintptr_t)(D + 0x5e8), 2, dialog);
    description = (void *)(uintptr_t)M32(0xe88);
    if (A32(description, 0) == 1) {
        if (M32(0xe90) != 0)
            UTIL_FN(0x90, multip_exchange_read_fn)((void *)(uintptr_t)(D + 0x638), 2, dialog);
    } else UTIL_FN(0x90, multip_exchange_read_fn)((void *)(uintptr_t)(D + 0x610), 2, dialog);
    M32(0xe90) = 0;
    return 0;
}
static int32_t FS98_STDCALL multip_details_write(void *dialog)
{
    uint32_t scratch;
    int32_t result = 0, status;
    if (UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x5e8), 2, dialog, 1) != 0) result = -1;
    if (A32((void *)(uintptr_t)M32(0xe88), 0) == 1) {
        if (M32(0xe90) != 0 && UTIL_FN(0x8c, multip_exchange_write_fn)(
            (void *)(uintptr_t)(D + 0x638), 2, dialog, 1) != 0) return -1;
        M32(0x578) = M32(0xe80); if (M32(0x578) < 2) M32(0x578) = 2;
        M32(0x580) = M32(0xe84);
        NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x578), 0x1001b, (void *)(uintptr_t)(D + 0xed8));
        NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x580), 0x1001b, (void *)(uintptr_t)(D + 0xee8));
        status = NUMERIC_FN(0x20, multip_numeric_parse_fn)((void *)(uintptr_t)A32(dialog, 0x20), 0x1b,
            &scratch, (void *)(uintptr_t)(D + 0x578));
        A32(dialog, 0x1c) = status; if (status != 0) result = -1;
        status = NUMERIC_FN(0x20, multip_numeric_parse_fn)((void *)(uintptr_t)A32(dialog, 0x34), 0x1b,
            &scratch, (void *)(uintptr_t)(D + 0x580));
        A32(dialog, 0x30) = status; if (status != 0) result = -1;
    } else if (UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x610), 2, dialog, 1) != 0) result = -1;
    return result;
}
static int32_t FS98_STDCALL multip_details_clear(void *dialog)
{
    while (A32(dialog, 0x50) != 0) {
        uint32_t index = A32(dialog, 0x50) - 1;
        uint8_t *players = (uint8_t *)(uintptr_t)A32(dialog, 0x4c);
        A32(dialog, 0x50) = index;
        if (HOST_FN(0x214, multip_free_fn)((void **)(players + index * 16 + 12)) != 0) return -1;
    }
    return HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)dialog + 0x4c)) != 0 ? -1 : 0;
}
static int32_t FS98_STDCALL multip_details_accept(void *dialog)
{
    int32_t result = 0;
    if (UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x5e8), 2,
        (void *)(uintptr_t)(D + 0x588), 0) != 0) result = -1;
    if (A32((void *)(uintptr_t)M32(0xe88), 0) == 1) {
        if (UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x638), 2, dialog, 0) != 0) result = -1;
        if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0) return -1;
    }
    return result;
}
static int32_t FS98_STDCALL multip_details_kick(void *dialog)
{
    char *format, *message;
    uint32_t size, answer;
    HOST_FN(0x2a4, multip_load_string_fn)(0xf53b, &format);
    size = HOST_FN(0x2b8, multip_strlen_fn)((const char *)(uintptr_t)A32(
        (void *)(uintptr_t)(A32(dialog, 0x4c) + A32(dialog, 0x58) * 16), 12));
    size += HOST_FN(0x2b8, multip_strlen_fn)(format);
    if (HOST_FN(0x20c, multip_alloc_fn)(5, size * 2, (void **)&message, 0) != 0) return -1;
    wsprintfA(message, format, (const char *)(uintptr_t)A32(
        (void *)(uintptr_t)(A32(dialog, 0x4c) + A32(dialog, 0x58) * 16), 12));
    answer = UTIL_FN(0xa4, multip_confirm_fn)(0xc0, 1, ADDR(message), 0, 0, 0, 0, 0);
    if ((answer & 0x40) != 0 && multip_message_1016(A32(
        (void *)(uintptr_t)(A32(dialog, 0x4c) + A32(dialog, 0x58) * 16), 0)) != 0) return -1;
    if (HOST_FN(0x214, multip_free_fn)((void **)&format) != 0) return -1;
    if (HOST_FN(0x214, multip_free_fn)((void **)&message) != 0) return -1;
    M32(0xe8c) = HOST_FN(0x38, multip_ticks_fn)() + 2000;
    return 0;
}
static int32_t FS98_STDCALL multip_details_refresh(void *dialog)
{
    void *description = (void *)(uintptr_t)M32(0xe88);
    int32_t result;
    if (A32(description, 0x14) == 0) return 0;
    if (HOST_FN(0x38, multip_ticks_fn)() - M32(0xe8c) <= M32(0x178) * 1000) return 0;
    M32(0xe8c) = HOST_FN(0x38, multip_ticks_fn)();
    if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)dialog + 0x4c)) != 0) return -1;
    description = (void *)(uintptr_t)M32(0xe88);
    result = (*(multip_details_get_fn *)((uint8_t *)description + 0x14))(
        (void **)((uint8_t *)dialog + 0x4c), (uint32_t *)((uint8_t *)dialog + 0x50),
        (void *)(uintptr_t)(D + 0xe78), (void *)(uintptr_t)A32(description, 8));
    if (result != 0) return 0;
    return multip_details_read(dialog) != 0 ? -1 : 1;
}
static int32_t multip_details_init(void)
{
    void *description = (void *)(uintptr_t)M32(0xe88);
    M32(0x588) = 0; M32(0x59c) = A32(description, 0); M32(0x5a0) = A32(description, 4);
    M32(0xe90) = 1; M32(0x644) = A32(description, 0xc); M32(0x658) = A32(description, 0x10);
    if ((*(multip_details_get_fn *)((uint8_t *)description + 0x14))(
        (void **)(uintptr_t)(D + 0x5d4), (uint32_t *)(uintptr_t)(D + 0x5d8),
        (void *)(uintptr_t)(D + 0xe78), (void *)(uintptr_t)A32(description, 8)) == 0) {
        if (multip_details_read((void *)(uintptr_t)(D + 0x588)) != 0) return -1;
        NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x57c), 0x1001b, (void *)(uintptr_t)(D + 0xee0));
        NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x584), 0x1001b, (void *)(uintptr_t)(D + 0xef0));
        multip_details_write((void *)(uintptr_t)(D + 0x588));
    }
    return 0;
}
static int32_t FS98_STDCALL multip_details_dialog(void *description)
{
    M32(0xe88) = ADDR(description);
    if (multip_details_init() != 0) return -1;
    if (HOST_FN(0x10c, multip_dialog_fn)(0x110c, (void *)(uintptr_t)(D + 0x588)) != 0) return -1;
    return multip_details_clear((void *)(uintptr_t)(D + 0x588)) != 0 ? -1 : 0;
}
