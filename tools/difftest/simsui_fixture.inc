/* How simsuidiff.c shapes parts of its random fixture so the routines meet what they
 * expect often enough to take every path. Included after the fixture and helpers. */

/* a BITMAPINFOHEADER, or now and then a BITMAPCOREHEADER: size, bit count, colours used */
static void shape_info(uint8_t *info)
{
    static const uint16_t depths[] = {1, 4, 8, 16, 24, 32, 2, 0};
    uint32_t random = next_random();
    uint32_t size = random % 5 == 0 ? 0xc : 0x28;
    memcpy(info, &size, 4);
    memcpy(info + (size == 0xc ? 0xa : 0xe), &depths[(random >> 4) % 8], 2);
    if (size == 0x28) {
        uint32_t used = (random >> 8) % 3 == 0 ? (random >> 12) % 0x120 : 0;
        memcpy(info + 0x20, &used, 4);
    }
}
/* a CDIBSection, small, its bits in one of the fixture's pixel areas */
static void shape_dib(uint8_t *dib, uint8_t *pixels)
{
    uint32_t random = next_random();
    int32_t width = (int32_t)(random % 40) + 1, height = (int32_t)((random >> 8) % 40) + 1;
    uint32_t bits = fixture_address(pixels);
    if (random % 23 == 0) width = 0;
    if (random % 29 == 0) height = -height;
    memcpy(dib + 8, &width, 4);
    memcpy(dib + 0xc, &height, 4);
    memcpy(dib + 0x64, &bits, 4);
}
static void shape_dibs(void)
{
    uint32_t index, at;
    for (index = 0; index < 4; ++index) {
        fill(s_fixture.dibs[index], 0x70);
        shape_dib(s_fixture.dibs[index], s_fixture.pixels[index]);
        for (at = 0; at < 0x800; ++at) {
            uint32_t random = next_random();
            s_fixture.pixels[index][at] = (uint8_t)(random % 3 ? random % 4 : random >> 8);
        }
    }
    for (index = 0; index < 12; ++index) s_fixture.ints[index] = (int32_t)(next_random() % 50) - 8;
    shape_info(s_fixture.resource);
    {
        uint32_t random = next_random();
        int32_t width = (int32_t)(random % 30) + 1, height = (int32_t)((random >> 8) % 30) - 4;
        static const uint16_t depths[] = {8, 8, 4, 1, 24, 16, 32, 8};
        memcpy(s_fixture.resource + 4, &width, 4);
        memcpy(s_fixture.resource + 8, &height, 4);
        memcpy(s_fixture.resource + 0xe, &depths[(random >> 16) % 8], 2);
        for (index = 0x28; index < sizeof(s_fixture.resource); ++index)
            s_fixture.resource[index] = (uint8_t)next_random();
    }
}
/* the object under test: a CDIBSection's header at +0x1c, or the whole of one */
static void shape_object(uint8_t *object)
{
    uint32_t vtable = class_vtable(CURRENT.rva);
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CDIBSection@@6B@") && next_random() % 4) {
        shape_dib(object, s_fixture.pixels[next_random() % 4]);
        return;
    }
    if (next_random() % 2) shape_info(object + 0x1c);
}
