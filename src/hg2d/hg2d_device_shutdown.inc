/* 0x2078c370: device selection's teardown path. Texture accounting is
 * cleared before releasing the six device interfaces, in retail order. */
void STDCALL __attribute__((used)) hg2d_device_shutdown(void)
{
    static const uint32_t interfaces[] = {0x28940, 0x28ccc, 0x289f4, 0x289d0, 0x28cc0, 0x289f0};
    uint32_t index, offset;
    for (index = 0; index < 8; index++) WORD(0x8820 + index * 4) = 0;
    for (offset = POOL; offset < POOL_END; offset += 0x34) {
        uint32_t *texture = (uint32_t *)(hg2d_image + offset);
        if (texture[3]) texture_release(texture);
    }
    WORD(0x289ec) = WORD(0x289d4) = WORD(0x5f40) = 0;
    for (index = 0; index < sizeof interfaces / sizeof interfaces[0]; index++) {
        offset = interfaces[index];
        uint32_t instance = WORD(offset);
        if (instance) {
            uint32_t *object = POINTER(instance), *vtable = POINTER(object[0]);
            ((call_one)(uintptr_t)vtable[2])(instance);
            WORD(offset) = 0;
        }
    }
}
