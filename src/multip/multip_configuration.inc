/* Configuration dialog 0x110a and provider selection. */
static int32_t multip_options_dialog(void);
static int32_t FS98_STDCALL multip_details_dialog(void *description);
typedef uint32_t (FS98_STDCALL *multip_confirm_fn)(uint32_t, uint32_t, uint32_t,
    uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* .text+0x3f70 -- the selected provider GUID starts after its name pointer. */
static int32_t FS98_STDCALL multip_configuration_provider(void *dialog)
{
    uint8_t *provider = (uint8_t *)(uintptr_t)A32(dialog, 0x84) + A32(dialog, 0x90) * 28;
    if (multip_directplay_create((const GUID *)(provider + 4)) != 0) {
        UTIL_FN(0xa4, multip_confirm_fn)(2, 1, 0xf53d, 0, 0, 0, 0, 0);
        return -1;
    }
    return 0;
}
/* .text+0x3fc0 -- release only after successful browser cleanup. */
static int32_t FS98_STDCALL multip_configuration_reset(void *dialog)
{
    void *directplay;
    if (multip_browser_clear() != 0) return -1;
    directplay = (void *)(uintptr_t)M32(M_DIRECTPLAY);
    if (directplay != NULL) {
        (*(multip_dp_release_fn *)(void *)(*(uint8_t **)directplay + 8))(directplay);
        M32(M_DIRECTPLAY) = 0;
    }
    A32(dialog, 0xb0) = 0;
    return 0;
}
/* .text+0x3e80 -- validation marks individual controls and still parses both
 * capacities, even when the host/session names were already rejected. */
static int32_t FS98_STDCALL multip_configuration_validate(void *dialog)
{
    uint32_t scratch;
    int32_t result = 0, status;
    if (UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x468), 9,
        (void *)(uintptr_t)(D + 0x3a8), 1) != 0) return -1;
    if (A32(dialog, 0x24) != 2 && HOST_FN(0x2b8, multip_strlen_fn)(
        (const char *)(uintptr_t)A32(dialog, 0x2c)) == 0) {
        A32(dialog, 0x28) = 0x1003; result = -1;
    }
    if (A32(dialog, 0x24) == 1) {
        M32(0x390) = A32(dialog, 0x40) != 1;
        if (HOST_FN(0x2b8, multip_strlen_fn)((const char *)(uintptr_t)A32(dialog, 0x4c)) == 0) {
            A32(dialog, 0x48) = 0x1003; result = -1;
        }
        status = NUMERIC_FN(0x20, multip_numeric_parse_fn)((const void *)(uintptr_t)A32(dialog, 0x60),
            0x1b, &scratch, (void *)(uintptr_t)(D + 0x388));
        A32(dialog, 0x5c) = status; if (status != 0) result = -1;
        status = NUMERIC_FN(0x20, multip_numeric_parse_fn)((const void *)(uintptr_t)A32(dialog, 0x74),
            0x1b, &scratch, (void *)(uintptr_t)(D + 0x390));
        A32(dialog, 0x70) = status; if (status != 0) result = -1;
        NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x390),
            0x1001b, (void *)(uintptr_t)(D + 0xdd0));
    }
    return result;
}
/* .text+0x4000 -- build join selection or create the host's interface. */
static int32_t FS98_STDCALL multip_configuration_accept(void *dialog)
{
    uint32_t mode, index;
    uint8_t *record;
    const char *name;
    if (UTIL_FN(0x8c, multip_exchange_write_fn)((void *)(uintptr_t)(D + 0x468), 9,
        (void *)(uintptr_t)(D + 0x3a8), 0) != 0) return -1;
    if (HOST_FN(0x218, multip_memset_fn)((void *)(uintptr_t)(D + 0x9c0), 0, 80) != 0) return -1;
    mode = A32(dialog, 0x24);
    if (mode == 2) { M32(0x1a4) = 3; return 0; }
    if (mode == 1) {
        M32(0xaa8) = 1;
        return multip_configuration_provider(dialog);
    }
    if (mode != 0) return 0;
    M32(0xaa8) = 0;
    if (A32(dialog, 0xb0) == 0) { M32(0x1a4) = 0; return 0; }
    M32(0x9c0) = 80;
    record = (uint8_t *)(uintptr_t)A32(dialog, 0xac) + A32(dialog, 0xb8) * 84;
    for (index = 0; index < 4; index++) M32(0x9c8 + index * 4) = A32(record, 4 + index * 4);
    name = *(const char **)record;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, HOST_FN(0x2b8, multip_strlen_fn)(name) + 1,
        (void **)(uintptr_t)(D + 0x9f0), 1) != 0) return -1;
    record = (uint8_t *)(uintptr_t)A32(dialog, 0xac) + A32(dialog, 0xb8) * 84;
    HOST_FN(0x2a8, multip_strcpy_fn)((char *)(uintptr_t)M32(0x9f0), *(const char **)record);
    return 0;
}
static int32_t FS98_STDCALL multip_configuration_options(void *dialog)
{ (void)dialog; return multip_options_dialog() != 0 ? -1 : 0; }
static int32_t FS98_STDCALL multip_browser_details(void *dialog)
{
    uint8_t *record = (uint8_t *)(uintptr_t)A32(dialog, 0xac) + A32(dialog, 0xb8) * 84;
    M32(0x528) = ADDR(dialog); M32(0x524) = A32(record, 0);
    return multip_details_dialog((void *)(uintptr_t)(D + 0x520)) != 0 ? -1 : 0;
}
/* .text+0x3dc0 -- exchange errors are ignored, enumeration/cleanup errors stop
 * setup before publishing the four formatted count strings. */
int32_t multip_configuration_init(void)
{
    UTIL_FN(0x90, multip_exchange_read_fn)((void *)(uintptr_t)(D + 0x468), 9,
        (void *)(uintptr_t)(D + 0x3a8));
    if (multip_provider_enumerate() != 0) return -1;
    M32(0x42c) = M32(0x360); M32(0x430) = M32(0x348); M32(0x458) = 0;
    if (multip_browser_clear() != 0) return -1;
    NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x388), 0x1001b,
        (void *)(uintptr_t)(D + 0xdc0));
    NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x38c), 0x1001b,
        (void *)(uintptr_t)(D + 0xdb8));
    NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x390), 0x1001b,
        (void *)(uintptr_t)(D + 0xdd0));
    NUMERIC_FN(0x18, multip_numeric_format_fn)((void *)(uintptr_t)(D + 0x394), 0x1001b,
        (void *)(uintptr_t)(D + 0xdc8));
    return 0;
}
/* .text+0x41c0 -- ShellExecuteA's result is ignored. Tests hook this import. */
int32_t multip_configuration_launch(void)
{
    if (M32(0x3cc) == 2)
        ShellExecuteA(NULL, NULL, (const char *)(uintptr_t)(D + 0xb00), NULL, NULL, 1);
    return 0;
}
/* .text+0x3d60 -- setup/dialog failures are cancellation, without cleanup. */
int32_t multip_configuration_dialog(void)
{
    if (multip_configuration_init() != 0) return 0;
    if (HOST_FN(0x118, multip_dialog_fn)(0x110a, (void *)(uintptr_t)(D + 0x3a8)) != 0) return 0;
    if (M32(0x3a8) != 0) {
        if (multip_configuration_launch() != 0) return 0;
    } else { M32(0xaa8) = 0; M32(0x1a4) = 0; }
    if (multip_configuration_clear() != 0) return -1;
    return (int32_t)M32(0x3a8);
}
