/* Multipart model/texture packet builders. Retail frees the initial packet
 * only after enqueue succeeds; all earlier failures retain its allocation. */
/* .text+0x6e70 -- the second argument is unused. The total comes from the
 * current GLOBAL aircraft, read after the title callback and allocation. */
int32_t FS98_STDCALL multip_message_101a(uint32_t id, uint32_t total, uint32_t segments)
{
    char title[260];
    uint8_t *packet;
    uint32_t payload, size;
    void *aircraft;
    (void)total;
    AC_FN(0x30, multip_aircraft_title_fn)((void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11e0),
        1, title, sizeof(title));
    payload = HOST_FN(0x2b8, multip_strlen_fn)(title) + 0x39;
    size = payload + 8;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, (void **)&packet, 1) != 0) return -1;
    A32(packet, 0) = 0x101a; A32(packet, 4) = payload;
    aircraft = (void *)(uintptr_t)A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x11e0);
    A32(packet, 8) = A32(aircraft, 8) + A32(aircraft, 0x18);
    if (HOST_FN(0x21c, multip_copy_fn)(packet + 0x10,
        (const uint8_t *)(uintptr_t)M32(0xa3c) + 0x10, 44) != 0) return -1;
    A32(packet, 0x3c) = HOST_FN(0x2b8, multip_strlen_fn)(title) + 1;
    HOST_FN(0x2a8, multip_strcpy_fn)((char *)packet + 0x40, title);
    A32(packet, 0xc) = segments;
    if (multip_send(id, packet, size, 1) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;
}
/* .text+0x6fb0 */
int32_t FS98_STDCALL multip_message_101c(uint32_t id)
{ return multip_message_allocated(id, 0x101c); }

/* .text+0x7020 */
int32_t FS98_STDCALL multip_message_101b(uint32_t id, const void *source,
    uint32_t offset, uint32_t size)
{
    uint8_t *packet;
    uint32_t payload = size + 8, bytes = payload + 8;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, bytes, (void **)&packet, 1) != 0) return -1;
    A32(packet, 0) = 0x101b; A32(packet, 4) = payload;
    A32(packet, 8) = size; A32(packet, 0xc) = offset;
    if (HOST_FN(0x21c, multip_copy_fn)(packet + 0x10, source, size) != 0) return -1;
    if (multip_send(id, packet, bytes, 1) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;
}
/* .text+0x7270 */
int32_t FS98_STDCALL multip_message_101e(uint32_t id, uint32_t texture,
    const void *source, uint32_t offset, uint32_t size)
{
    uint8_t *packet;
    uint32_t payload = size + 12, bytes = payload + 8;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, bytes, (void **)&packet, 1) != 0) return -1;
    A32(packet, 0) = 0x101e; A32(packet, 4) = payload;
    A32(packet, 8) = texture; A32(packet, 0xc) = size; A32(packet, 0x10) = offset;
    if (HOST_FN(0x21c, multip_copy_fn)(packet + 0x14, source, size) != 0) return -1;
    if (multip_send(id, packet, bytes, 1) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;
}
/* .text+0x7530 -- all fifteen name bytes are copied, including past NUL. */
int32_t FS98_STDCALL multip_message_101d(uint32_t id, uint32_t texture,
    const void *name, uint32_t size, uint32_t segments)
{
    uint8_t *packet;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 36, (void **)&packet, 1) != 0) return -1;
    A32(packet, 0) = 0x101d; A32(packet, 4) = 28; A32(packet, 0x18) = size;
    if (HOST_FN(0x21c, multip_copy_fn)(packet + 8, name, 15) != 0) return -1;
    A32(packet, 0x1c) = texture; A32(packet, 0x20) = segments;
    if (multip_send(id, packet, 36, 1) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;
}
