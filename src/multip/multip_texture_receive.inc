/* Multipart texture receivers, .text+0x5290..+0x5694. Each owner has a
 * linked list of files. Unlink/free failures retain retail's partial state. */
void *FS98_STDCALL multip_texture_receive_owner(uint32_t id)
{
    void *owner = (void *)(uintptr_t)M32(0xdec);
    while (owner != NULL && A32(owner, 0) != id) owner = (void *)(uintptr_t)A32(owner, 8);
    return owner;
}
void *FS98_STDCALL multip_texture_receive_file(const void *owner, uint32_t id)
{
    void *file;
    if (owner == NULL) return NULL;
    file = (void *)(uintptr_t)A32(owner, 4);
    while (file != NULL && A32(file, 0) != id) file = (void *)(uintptr_t)A32(file, 0x20);
    return file;
}
int32_t FS98_STDCALL multip_texture_receive_ensure(uint32_t id)
{
    void *owner = multip_texture_receive_owner(id);
    if (owner == NULL) {
        if (HOST_FN(0x20c, multip_alloc_fn)(1, 12, &owner, 1) != 0) return -1;
        A32(owner, 0) = id; A32(owner, 4) = 0; A32(owner, 8) = M32(0xdec);
        M32(0xdec) = ADDR(owner);
    }
    return 0;
}
int32_t FS98_STDCALL multip_texture_receive_add(uint32_t owner_id, const void *name,
    uint32_t size, uint32_t id, uint32_t segments)
{
    void *owner = multip_texture_receive_owner(owner_id), *file;
    if (owner == NULL || multip_texture_receive_file(owner, id) != NULL) return 0;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 36, &file, 1) != 0) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, (void **)((uint8_t *)file + 0x18), 1) != 0) return -1;
    if (HOST_FN(0x21c, multip_copy_fn)((uint8_t *)file + 4, name, 15) != 0) return -1;
    A32(file, 0) = id; A32(file, 0x14) = size; A32(file, 0x1c) = segments;
    A32(file, 0x20) = A32(owner, 4); A32(owner, 4) = ADDR(file);
    return 0;
}
int32_t FS98_STDCALL multip_texture_receive_clear(void *owner)
{
    void *file;
    while ((file = (void *)(uintptr_t)A32(owner, 4)) != NULL) {
        A32(owner, 4) = A32(file, 0x20);
        if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)file + 0x18)) != 0) return -1;
        if (HOST_FN(0x214, multip_free_fn)(&file) != 0) return -1;
    }
    return 0;
}

/* .text+0x55e0 -- even for a non-head match, retail replaces the head with
 * that match's next pointer. Preceding owners are detached without freeing. */
int32_t FS98_STDCALL multip_texture_receive_remove_owner(uint32_t id)
{
    void *owner = multip_texture_receive_owner(id);
    if (owner == NULL) return 0;
    M32(0xdec) = A32(owner, 8);
    if (multip_texture_receive_clear(owner) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)(&owner) != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_texture_receive_remove(void *owner, uint32_t id)
{
    void *file = (void *)(uintptr_t)A32(owner, 4), *previous = NULL;
    while (file != NULL && A32(file, 0) != id) {
        previous = file; file = (void *)(uintptr_t)A32(file, 0x20);
    }
    if (file == NULL) return 0;
    if (previous == NULL) A32(owner, 4) = A32(file, 0x20);
    else A32(previous, 0x20) = A32(file, 0x20);
    if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)file + 0x18)) != 0) return -1;
    if (HOST_FN(0x214, multip_free_fn)(&file) != 0) return -1;
    if (A32(owner, 4) == 0 && multip_texture_receive_remove_owner(A32(owner, 0)) != 0)
        return -1;
    return 0;
}

/* .text+0x5290/+0x52d0/+0x5370 -- sizes are ignored; offsets/lengths are
 * passed through to host memory/file services without bounds checks. */
int32_t FS98_STDCALL multip_handle_101d(uint32_t id, const void *packet, uint32_t size)
{
    (void)size;
    if (multip_texture_receive_ensure(id) != 0) return -1;
    return multip_texture_receive_add(id, (const uint8_t *)packet + 8,
        A32(packet, 0x18), A32(packet, 0x1c), A32(packet, 0x20)) != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_handle_101e(uint32_t id, const void *packet, uint32_t size)
{
    void *owner = multip_texture_receive_owner(id);
    uint8_t *file = multip_texture_receive_file(owner, A32(packet, 8));
    (void)size;
    if (file == NULL) return -1;
    if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(A32(file, 0x18) + A32(packet, 0x10)),
        (const uint8_t *)packet + 0x14, A32(packet, 0xc)) != 0) return -1;
    A32(file, 0x1c)--;
    if (A32(file, 0x1c) == 0) {
        if (multip_texture_write((const char *)file + 4,
            (const void *)(uintptr_t)A32(file, 0x18), A32(file, 0x14)) != 0) return -1;
        if (multip_texture_receive_remove(owner, A32(packet, 8)) != 0) return -1;
    }
    return 0;
}
int32_t FS98_STDCALL multip_handle_101f(uint32_t id, const void *packet, uint32_t size)
{
    void *owner = multip_texture_receive_owner(id);
    (void)size;
    if (owner == NULL) return -1;
    return multip_texture_receive_remove(owner, A32(packet, 8)) != 0 ? -1 : 0;
}
