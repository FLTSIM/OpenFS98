/* Weather requests and snapshots, .text+0x60d0/+0x6140. */
struct multip_weather_field {
    uint32_t source, dest, size;
};
#include "multip_weather_fields.inc"

int32_t multip_message_1013(void)
{ return multip_message_allocated(0, 0x1013); }

static int32_t __attribute__((used, noinline)) multip_message_1014_impl(uint32_t id, uint32_t padding)
{
    uint8_t packet[0x25c];
    uint32_t index;
    const struct multip_weather_field *field;

    A32(packet, 0) = 0x1014;
    A32(packet, 0x258) = padding;
    for (index = 0; index < sizeof(s_weather_fields) / sizeof(s_weather_fields[0]); index++) {
        field = &s_weather_fields[index];
        if (HOST_FN(0x21c, multip_copy_fn)(packet + field->dest,
            (const void *)(uintptr_t)(M32(IMPORT_GLOBAL) + field->source), field->size) != 0)
            return -1;
    }
    A32(packet, 4) = 0x254;
    return multip_send(id, packet, sizeof(packet), 1) != 0 ? -1 : 0;
}

#if defined(__i386__)
/* Four trailing bytes are not initialized in retail's 0x300-byte frame. */
__attribute__((naked)) int32_t FS98_STDCALL multip_message_1014(uint32_t id __attribute__((unused)))
{
    __asm__ volatile("movl -0xa8(%esp), %eax\n\tpushl %eax\n\tpushl 8(%esp)\n\t"
        "call _multip_message_1014_impl\n\taddl $8, %esp\n\tret $4");
}
#else
int32_t FS98_STDCALL multip_message_1014(uint32_t id) { return multip_message_1014_impl(id, 0); }
#endif

/* .text+0x67e0 -- allocate first, then timestamp and read peer state. */
int32_t FS98_STDCALL multip_message_1017(const void *player)
{
    uint32_t *packet;

    if (HOST_FN(0x20c, multip_alloc_fn)(1, 20, (void **)&packet, 1) != 0)
        return -1;
    packet[0] = 0x1017;
    packet[1] = 12;
    packet[4] = HOST_FN(0x38, multip_ticks_fn)();
    packet[3] = A32(player, 0x40);
    packet[2] = A32(player, 0x3c);
    if (multip_send(A32(player, 4), packet, 20, 0) != 0)
        return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;
}

/* .text+0x1270 -- receive the same 28 fields in the same order, then notify
 * MAIN and WEATHERM. Both notification results are ignored. */
typedef uint32_t (FS98_STDCALL *multip_main_weather_fn)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *multip_weather_refresh_fn)(void);
int32_t FS98_STDCALL multip_handle_1014(uint32_t id, const void *packet, uint32_t size)
{
    uint32_t index;
    const struct multip_weather_field *field;
    (void)id; (void)size;
    for (index = 0; index < sizeof(s_weather_fields) / sizeof(s_weather_fields[0]); index++) {
        field = &s_weather_fields[index];
        if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(M32(IMPORT_GLOBAL) + field->source),
            (const uint8_t *)packet + field->dest, field->size) != 0)
            return -1;
    }
    (*(multip_main_weather_fn *)(uintptr_t)(M32(0x34) + 0x74))(
        1, *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x23e));
    (*(multip_weather_refresh_fn *)(uintptr_t)(M32(0x4c) + 0x24))();
    return 0;
}

/* .text+0x1240 -- host replies to a weather request, otherwise succeeds. */
static int32_t __attribute__((used, noinline)) multip_handle_1013_impl(uint32_t id, uint32_t padding)
{
    if (M32(0xaa8) != 0 && multip_message_1014_impl(id, padding) != 0)
        return -1;
    return 0;
}
#if defined(__i386__)
__attribute__((naked)) int32_t FS98_STDCALL multip_handle_1013(
    uint32_t id __attribute__((unused)), const void *packet __attribute__((unused)), uint32_t size __attribute__((unused)))
{
    __asm__ volatile("movl -0xb0(%esp), %eax\n\tpushl %eax\n\tpushl 8(%esp)\n\t"
        "call _multip_handle_1013_impl\n\taddl $8, %esp\n\tret $12");
}
#else
int32_t FS98_STDCALL multip_handle_1013(uint32_t id, const void *packet, uint32_t size)
{ (void)packet; (void)size; return multip_handle_1013_impl(id, 0); }
#endif
