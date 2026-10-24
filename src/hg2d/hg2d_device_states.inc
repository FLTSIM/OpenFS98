/* 0x2078c680 / 0x2078cc80: cached render-state setters. The initialization
 * and drawing paths retain their separate HRESULT scratch words. */
static void device_state_set(uint32_t state, uint32_t value, uint32_t result_offset)
{
    if (WORD(0x28b00 + state * 4) != value) {
        uint32_t instance = WORD(0x289d0), *object = POINTER(instance), *vtable = POINTER(object[0]);
        WORD(0x28b00 + state * 4) = value;
        WORD(result_offset) = ((call_three)(uintptr_t)vtable[23])(instance, state, value);
    }
}

void STDCALL __attribute__((used)) hg2d_device_state(uint32_t state, uint32_t value)
{
    device_state_set(state, value, 0x5f20);
}

void STDCALL __attribute__((used)) hg2d_device_draw_state(uint32_t state, uint32_t value)
{
    device_state_set(state, value, 0x5f34);
}

/* 0x2078c400: query all states before setting the device defaults. Failed
 * GetRenderState calls retain whatever was already in the output slot. */
void STDCALL __attribute__((used)) hg2d_device_states(uint32_t flags)
{
    uint32_t state;
    for (state = 1; state < 64; state++) {
        uint32_t instance = WORD(0x289d0), *object = POINTER(instance), *vtable = POINTER(object[0]);
        ((call_three)(uintptr_t)vtable[22])(instance, state, ADDRESS(hg2d_image + 0x28b00 + state * 4));
    }
    hg2d_device_state(7, 0);
    hg2d_device_state(14, 0);
    hg2d_device_state(9, 1);
    hg2d_device_state(22, 1);
    hg2d_device_state(29, 0);
    hg2d_device_state(28, 0);
    hg2d_device_state(26, (flags & 0x20) != 0);
    hg2d_device_state(4, 1);
    hg2d_device_state(1, 0);
    hg2d_device_state(21, 2);
    hg2d_device_state(17, WORD(0x289e4));
    hg2d_device_state(18, WORD(0x289e8));
    hg2d_device_state(3, 1);
    hg2d_device_state(27, 0);
    hg2d_device_state(41, 1);
    hg2d_device_state(29, 0);
    hg2d_device_state(39, 0);
    hg2d_device_state(19, 5);
    hg2d_device_state(20, 6);
}
