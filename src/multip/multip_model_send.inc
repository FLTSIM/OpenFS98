/* Multipart aircraft sender, .text+0x6a90..+0x6e62. */
uint32_t FS98_STDCALL multip_model_send_callback(uint32_t, uint32_t);
int32_t multip_model_send_stop(void)
{
    if (M32(0xe20) == 0) return 0;
    if (CHAIN_FN(0x1c, multip_chain_register_fn)(2, multip_model_send_callback) != 0) return -1;
    M32(0xe20) = 0;
    return HOST_FN(0x214, multip_free_fn)((void **)(uintptr_t)(D + 0xe14)) != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_model_send_add(uint32_t id)
{
    void *recipient;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 12, &recipient, 1) != 0) return -1;
    A32(recipient, 0) = id; A32(recipient, 4) = 0;
    if (multip_message_101a(id, M32(0xe18), M32(0xe1c)) != 0) return -1;
    A32(recipient, 8) = M32(0xe10); M32(0xe10) = ADDR(recipient);
    return 0;
}
int32_t FS98_STDCALL multip_model_send(uint32_t id)
{
    if (M32(0xe20) == 0) {
        uint64_t rate = M32(0x170);
        void *local, *aircraft;
        MD(0xe30) = 0;
        __asm__ volatile("fildq %1\n\tfmull %2\n\tfstpl %0" : "=m"(MD(0xe28))
            : "m"(rate), "m"(*(double *)(void *)(s_multip_rdata + 0x178)) : "st");
        local = (void *)(uintptr_t)M32(0xa3c);
        M32(0xe18) = A32(local, 0x24) + A32(local, 0x2c);
        if (HOST_FN(0x20c, multip_alloc_fn)(1, M32(0xe18), (void **)(uintptr_t)(D + 0xe14), 1) != 0) return -1;
        local = (void *)(uintptr_t)M32(0xa3c);
        aircraft = (void *)(uintptr_t)A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x11e0);
        if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)M32(0xe14),
            (const void *)(uintptr_t)A32(aircraft, 4), A32(local, 0x24)) != 0) return -1;
        local = (void *)(uintptr_t)M32(0xa3c);
        aircraft = (void *)(uintptr_t)A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x11e0);
        if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(M32(0xe14) + A32(aircraft, 8)),
            (const void *)(uintptr_t)A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x11e4), A32(local, 0x2c)) != 0) return -1;
        M32(0xe1c) = M32(0xe18) / M32(0x16c);
        if (M32(0xe18) % M32(0x16c) != 0) M32(0xe1c)++;
        if (CHAIN_FN(0x18, multip_chain_register_fn)(2, multip_model_send_callback) != 0) return -1;
        M32(0xe20) = 1;
    }
    return multip_model_send_add(id) != 0 ? -1 : 0;
}
int32_t multip_model_send_clear(void)
{
    void *recipient;
    while ((recipient = (void *)(uintptr_t)M32(0xe10)) != NULL) {
        M32(0xe10) = A32(recipient, 8);
        if (HOST_FN(0x214, multip_free_fn)(&recipient) != 0) return -1;
    }
    return 0;
}
int32_t multip_model_send_cancel(void)
{
    void *recipient = (void *)(uintptr_t)M32(0xe10);
    if (M32(0xe20) == 0) return 0;
    while (recipient != NULL) {
        if (multip_message_101c(A32(recipient, 0)) != 0) return -1;
        recipient = (void *)(uintptr_t)A32(recipient, 8);
    }
    if (multip_model_send_stop() != 0) return -1;
    return multip_model_send_clear() != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_model_send_remove(uint32_t id)
{
    void *recipient = (void *)(uintptr_t)M32(0xe10), *previous = NULL;
    while (recipient != NULL && A32(recipient, 0) != id) {
        previous = recipient; recipient = (void *)(uintptr_t)A32(recipient, 8);
    }
    if (recipient == NULL) return 0;
    if (previous == NULL) M32(0xe10) = A32(recipient, 8);
    else A32(previous, 8) = A32(recipient, 8);
    if (HOST_FN(0x214, multip_free_fn)(&recipient) != 0) return -1;
    if (M32(0xe10) == 0 && multip_model_send_stop() != 0) return -1;
    return 0;
}
static uint32_t __attribute__((used, noinline)) multip_model_send_callback_impl(uint32_t incoming)
{
    uint16_t status;
    uint32_t result;
    void *recipient;
    __asm__ volatile("fldl %1\n\tfaddl %2\n\tfstl %1\n\tfcompl %3\n\tfnstsw %0"
        : "=a"(status), "+m"(MD(0xe30)) : "m"(MD(0xe28)),
          "m"(*(double *)(void *)(s_multip_rdata + 0x188)) : "st");
    result = (incoming & 0xffff0000u) | status;
    if (status & 0x100) return result;
    recipient = (void *)(uintptr_t)M32(0xe10);
    MD(0xe30) = -1.0;
    while (recipient != NULL) {
        uint32_t offset = A32(recipient, 4), size = M32(0xe18) - offset;
        void *next;
        if (size >= M32(0x16c)) size = M32(0x16c);
        result = (uint32_t)multip_message_101b(A32(recipient, 0),
            (const void *)(uintptr_t)(M32(0xe14) + offset), offset, size);
        if (result != 0) return result;
        A32(recipient, 4) += size;
        next = (void *)(uintptr_t)A32(recipient, 8);
        result = ADDR(recipient);
        if (A32(recipient, 4) == M32(0xe18)) {
            result = (uint32_t)multip_model_send_remove(A32(recipient, 0));
            if (result != 0) return result;
        }
        recipient = next;
    }
    return result;
}
#if defined(__i386__)
__attribute__((naked)) uint32_t FS98_STDCALL multip_model_send_callback(
    uint32_t code __attribute__((unused)), uint32_t argument __attribute__((unused)))
{ __asm__ volatile("pushl %eax\n\tcall _multip_model_send_callback_impl\n\taddl $4, %esp\n\tret $8"); }
#else
uint32_t FS98_STDCALL multip_model_send_callback(uint32_t code, uint32_t argument)
{ (void)code; (void)argument; return multip_model_send_callback_impl(0); }
#endif

/* .text+0x10b0 -- cancel the previous transfer before serving a request. */
int32_t FS98_STDCALL multip_handle_1010(uint32_t id, const void *packet, uint32_t size)
{
    (void)packet; (void)size;
    if (M32(0xacc) != 0) {
        if (multip_model_send_cancel() != 0) return -1;
        if (multip_model_send(id) != 0) return -1;
    }
    return 0;
}
