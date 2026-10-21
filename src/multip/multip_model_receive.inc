/* Multipart aircraft receive state, .text+0x4fe0..+0x5289. */
void *FS98_STDCALL multip_model_receive_find(uint32_t id)
{
    void *transfer = (void *)(uintptr_t)M32(0xde8);
    while (transfer != NULL && A32(transfer, 0) != id)
        transfer = (void *)(uintptr_t)A32(transfer, 0x140);
    return transfer;
}
int32_t FS98_STDCALL multip_model_receive_remove(uint32_t id)
{
    void *transfer = (void *)(uintptr_t)M32(0xde8), *previous = NULL;
    while (transfer != NULL && A32(transfer, 0) != id) {
        previous = transfer; transfer = (void *)(uintptr_t)A32(transfer, 0x140);
    }
    if (transfer == NULL) return 0;
    if (previous == NULL) M32(0xde8) = A32(transfer, 0x140);
    else A32(previous, 0x140) = A32(transfer, 0x140);
    if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)transfer + 0x34)) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)(&transfer) != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_handle_101a(uint32_t id, const void *packet, uint32_t size)
{
    void *transfer = multip_model_receive_find(id);
    (void)size;
    if (transfer != NULL) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 324, &transfer, 1) != 0) return -1;
    A32(transfer, 0) = id; A32(transfer, 4) = A32(packet, 0xc);
    if (HOST_FN(0x21c, multip_copy_fn)((uint8_t *)transfer + 8,
        (const uint8_t *)packet + 0x10, 44) != 0) return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)((char *)transfer + 0x38, (const char *)packet + 0x40);
    if (HOST_FN(0x20c, multip_alloc_fn)(1, A32(packet, 8),
        (void **)((uint8_t *)transfer + 0x34), 1) != 0) return -1;
    A32(transfer, 0x18) = A32(transfer, 0x34);
    A32(transfer, 0x20) = A32(transfer, 0x34) + A32(transfer, 0x1c);
    A32(transfer, 0x140) = M32(0xde8); M32(0xde8) = ADDR(transfer);
    return 0;
}
int32_t FS98_STDCALL multip_handle_101b(uint32_t id, const void *packet, uint32_t size)
{
    void *transfer = multip_model_receive_find(id), *player;
    (void)size;
    if (transfer == NULL) return -1;
    if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(A32(transfer, 0x34) + A32(packet, 0xc)),
        (const uint8_t *)packet + 0x10, A32(packet, 8)) != 0) return -1;
    A32(transfer, 4)--;
    if (A32(transfer, 4) == 0) {
        player = multip_find_player(id);
        if (player == NULL) {
            multip_model_receive_remove(id);
            return -1;
        }
        if (multip_model_cache_write((const uint8_t *)transfer + 8, (const char *)transfer + 0x38) != 0) return -1;
        if (multip_model_cache_select((uint8_t *)player + 0x10, (const char *)transfer + 0x38) != 0) return -1;
        if (multip_textures_request(id, (const void *)(uintptr_t)A32(player, 0x20), A32(player, 0x24)) != 0) return -1;
        if (multip_model_receive_remove(id) != 0) return -1;
    }
    return 0;
}
int32_t FS98_STDCALL multip_handle_101c(uint32_t id, const void *packet, uint32_t size)
{
    (void)packet; (void)size;
    return multip_model_receive_remove(id) != 0 ? -1 : 0;
}
