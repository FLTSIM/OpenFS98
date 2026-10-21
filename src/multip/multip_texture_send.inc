/* Timed multipart texture sender, .text+0x70d0..+0x76f4. Cached file data
 * survives node removal in retail; recipient failures retain partial lists. */
uint32_t FS98_STDCALL multip_texture_send_callback(uint32_t, uint32_t);
void *FS98_STDCALL multip_texture_send_find(const void *name)
{
    void *file = (void *)(uintptr_t)M32(0xe3c);
    while (file != NULL) {
        if (HOST_FN(0x228, multip_compare_fn)(name, file, 15) == 0) break;
        file = (void *)(uintptr_t)A32(file, 0x24);
    }
    return file;
}
int32_t FS98_STDCALL multip_texture_send_cache(const void *name)
{
    void *file = multip_texture_send_find(name), *data;
    uint32_t size;
    if (file != NULL) return 0;
    if (multip_texture_read(name, &data, &size) != 0) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 40, &file, 1) != 0) return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(file, name, 15) != 0) return -1;
    A32(file, 0x1c) = ADDR(data); A32(file, 0x18) = size;
    A32(file, 0x14) = size / M32(0x16c);
    if (size % M32(0x16c) != 0) A32(file, 0x14)++;
    A32(file, 0x10) = M32(0xe40); M32(0xe40)++;
    A32(file, 0x20) = 0; A32(file, 0x24) = M32(0xe3c);
    M32(0xe3c) = ADDR(file);
    return 0;
}
int32_t FS98_STDCALL multip_texture_send_add(const void *name, uint32_t id)
{
    void *file = multip_texture_send_find(name), *recipient;
    if (file == NULL) return -1;
    recipient = (void *)(uintptr_t)A32(file, 0x20);
    while (recipient != NULL && A32(recipient, 0) != id)
        recipient = (void *)(uintptr_t)A32(recipient, 8);
    if (recipient != NULL) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 12, &recipient, 1) != 0) return -1;
    A32(recipient, 0) = id; A32(recipient, 4) = 0;
    A32(recipient, 8) = A32(file, 0x20); A32(file, 0x20) = ADDR(recipient);
    return multip_message_101d(id, A32(file, 0x10), file,
        A32(file, 0x18), A32(file, 0x14)) != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_texture_send_remove(void *file, uint32_t id)
{
    void *recipient = (void *)(uintptr_t)A32(file, 0x20), *previous = NULL;
    while (recipient != NULL && A32(recipient, 0) != id) {
        previous = recipient; recipient = (void *)(uintptr_t)A32(recipient, 8);
    }
    if (recipient == NULL) return -1;
    if (previous == NULL) A32(file, 0x20) = A32(recipient, 8);
    else A32(previous, 8) = A32(recipient, 8);
    return HOST_FN(0x214, multip_free_fn)(&recipient) != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_texture_send_uncache(const void *name)
{
    void *file = (void *)(uintptr_t)M32(0xe3c), *previous = NULL;
    while (file != NULL) {
        if (HOST_FN(0x228, multip_compare_fn)(name, file, 15) == 0) break;
        previous = file; file = (void *)(uintptr_t)A32(file, 0x24);
    }
    if (file == NULL) return -1;
    if (previous == NULL) M32(0xe3c) = A32(file, 0x24);
    else A32(previous, 0x24) = A32(file, 0x24);
    return HOST_FN(0x214, multip_free_fn)(&file) != 0 ? -1 : 0;
}
int32_t multip_texture_send_start(void)
{
    if (M32(0xe38) == 0) {
        uint64_t rate = M32(0x170);
        MD(0xe50) = 0;
        __asm__ volatile("fildq %1\n\tfmull %2\n\tfstpl %0" : "=m"(MD(0xe48))
            : "m"(rate), "m"(*(double *)(void *)(s_multip_rdata + 0x198)) : "st");
        if (CHAIN_FN(0x18, multip_chain_register_fn)(2, multip_texture_send_callback) != 0) return -1;
        M32(0xe38) = 1;
    }
    return 0;
}
int32_t multip_texture_send_stop(void)
{
    if (M32(0xe38) != 0) {
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(2, multip_texture_send_callback) != 0) return -1;
        M32(0xe38) = 0;
    }
    return 0;
}
int32_t FS98_STDCALL multip_texture_send(uint32_t id, const void *name)
{
    if (multip_texture_send_cache(name) != 0) return -1;
    if (multip_texture_send_add(name, id) != 0) return -1;
    if (M32(0xe38) == 0 && multip_texture_send_start() != 0) return -1;
    return 0;
}
static uint32_t __attribute__((used, noinline)) multip_texture_send_callback_impl(uint32_t incoming)
{
    uint16_t status;
    uint32_t result;
    void *file;
    __asm__ volatile("fldl %1\n\tfaddl %2\n\tfstl %1\n\tfcompl %3\n\tfnstsw %0"
        : "=a"(status), "+m"(MD(0xe50)) : "m"(MD(0xe48)),
          "m"(*(double *)(void *)(s_multip_rdata + 0x1a8)) : "st");
    result = (incoming & 0xffff0000u) | status;
    if (status & 0x100) return result;
    MD(0xe50) -= *(double *)(void *)(s_multip_rdata + 0x1a8);
    file = (void *)(uintptr_t)M32(0xe3c);
    while (file != NULL) {
        void *recipient = (void *)(uintptr_t)A32(file, 0x20), *next_file;
        while (recipient != NULL) {
            uint32_t offset = A32(recipient, 4), size = A32(file, 0x18) - offset;
            void *next;
            if (size > M32(0x16c)) size = M32(0x16c);
            result = (uint32_t)multip_message_101e(A32(recipient, 0), A32(file, 0x10),
                (const void *)(uintptr_t)(A32(file, 0x1c) + offset), offset, size);
            if (result != 0) return result;
            A32(recipient, 4) += size;
            next = (void *)(uintptr_t)A32(recipient, 8);
            result = ADDR(recipient);
            if (A32(recipient, 4) == A32(file, 0x18)) {
                result = (uint32_t)multip_texture_send_remove(file, A32(recipient, 0));
                if (result != 0) return result;
            }
            recipient = next;
        }
        result = ADDR(file);
        next_file = (void *)(uintptr_t)A32(file, 0x24);
        if (A32(file, 0x20) == 0) {
            result = (uint32_t)multip_texture_send_uncache(file);
            if (result != 0) return result;
            result = M32(0xe3c);
            if (result == 0) {
                result = (uint32_t)multip_texture_send_stop();
                if (result != 0) return result;
            }
        }
        file = next_file;
    }
    return result;
}
#if defined(__i386__)
__attribute__((naked)) uint32_t FS98_STDCALL multip_texture_send_callback(
    uint32_t code __attribute__((unused)), uint32_t argument __attribute__((unused)))
{ __asm__ volatile("pushl %eax\n\tcall _multip_texture_send_callback_impl\n\taddl $4, %esp\n\tret $8"); }
#else
uint32_t FS98_STDCALL multip_texture_send_callback(uint32_t code, uint32_t argument)
{ (void)code; (void)argument; return multip_texture_send_callback_impl(0); }
#endif

/* .text+0x1780 -- consume the wire count backwards, including on failure. */
int32_t FS98_STDCALL multip_handle_1015(uint32_t id, void *packet, uint32_t size)
{
    (void)size;
    if (M32(0xac4) != 0) {
        while (A32(packet, 8) != 0) {
            A32(packet, 8)--;
            if (multip_texture_send(id, (const uint8_t *)packet + 12 + A32(packet, 8) * 15) != 0)
                return -1;
        }
    }
    return 0;
}
