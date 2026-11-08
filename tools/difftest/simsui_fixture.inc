/* How simsuidiff.c shapes parts of its random fixture so the routines meet what they
 * expect often enough to take every path. Included after the fixture and helpers. */

/* a BITMAPINFOHEADER, or now and then a BITMAPCOREHEADER: size, bit count, colours used */
static void shape_info(uint8_t *info)
{
    static const uint16_t depths[] = {1, 4, 8, 16, 24, 32, 2, 0};
    uint32_t random = next_random();
    uint32_t size = random % 5 == 0 ? 0xc : random % 9 == 1 ? 0x6c : 0x28;    /* now and then a V4 header */
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
        if (next_random() % 4 == 0)        /* nearly every colour, for UnusedColor's last ones */
            for (at = 0; at < 0x800; ++at) s_fixture.pixels[index][at] = (uint8_t)(at % (0xfd + index % 3));
    }
    for (index = 0; index < 12; ++index) s_fixture.ints[index] = (int32_t)(next_random() % 50) - 8;
    shape_info(s_fixture.resource);
    {
        uint32_t random = next_random();
        int32_t width = (int32_t)(random % 30) + 1, height = (int32_t)((random >> 8) % 64) - 4;
        static const uint16_t depths[] = {8, 8, 4, 1, 24, 16, 32, 8};
        memcpy(s_fixture.resource + 4, &width, 4);
        memcpy(s_fixture.resource + 8, &height, 4);
        memcpy(s_fixture.resource + 0xe, &depths[(random >> 16) % 8], 2);
        for (index = 0x28; index < sizeof(s_fixture.resource); ++index)
            s_fixture.resource[index] = (uint8_t)next_random();
    }
}
/* the fake vtable of the class named */
static void **fake_vtable_named(const char *name)
{
    uint32_t at;
    for (at = 0; at < VTABLE_COUNT; ++at) if (!strcmp(s_vtables[at].name, name)) return s_fake_vtables[at];
    return s_fake_vtables[GENERIC_VTABLE];
}
/* a CSimsTabPage where one can sit: its fake vtable, its window, shown or not, its owner */
static void shape_page(uint8_t *page, uint32_t random)
{
    void **vtable = fake_vtable_named("??_7CSimsTabPage@@6B@");
    uint32_t window = 0x6000 + (random % 4) * 0x10, shown = (random >> 2) % 2, edit = (random >> 3) % 3;
    uint32_t owner = fixture_address(s_fixture.blocks[0]);
    memcpy(page, &vtable, 4);
    memcpy(page + 0x20, &window, 4);
    memcpy(page + 0x7d0, &edit, 4);
    memcpy(page + 0x1100, &shown, 4);
    memcpy(page + 0x10f4, &owner, 4);
}
/* the object under test: a CDIBSection's header at +0x1c, or the whole of one */
/* the .BMP behind ARG_FILE: a Windows or OS/2 header, 1, 4, 8 or 24 bits, its colours and
 * rows; now and then the wrong magic, a header of neither size, offsets past the end, or
 * cut short */
static void make_file(void)
{
    static const uint16_t depths[] = {1, 4, 8, 24, 8, 2};
    uint32_t random = next_random(), core = random % 4 == 0, depth = depths[(random >> 2) % 6];
    int32_t width = 1 + (int32_t)((random >> 5) % 40), height = 1 + (int32_t)((random >> 11) % 30);
    uint32_t header = core ? 0xc : 0x28, entries = depth == 1 ? 2 : depth == 4 ? 16 : depth == 8 ? 256 : 0;
    uint32_t used = 0, table, stride, bits, offset, size, at;
    if (!core && (random >> 16) % 4 == 0) used = (random >> 18) % 300;
    if (!core && used) entries = used;
    table = entries * (core ? 3 : 4);
    stride = (((uint32_t)width * depth + 31) / 32) * 4;
    bits = stride * (uint32_t)height;
    offset = 0xe + header + table;
    size = offset + bits;
    memset(s_file, 0, sizeof(s_file));
    s_file[0] = 'B'; s_file[1] = 'M';
    memcpy(s_file + 2, &size, 4);
    memcpy(s_file + 0xa, &offset, 4);
    memcpy(s_file + 0xe, &header, 4);
    if (core) {
        uint16_t w = (uint16_t)width, h = (uint16_t)height, planes = 1, d = depth;
        memcpy(s_file + 0x12, &w, 2); memcpy(s_file + 0x14, &h, 2);
        memcpy(s_file + 0x16, &planes, 2); memcpy(s_file + 0x18, &d, 2);
    } else {
        uint16_t planes = 1, d = depth;
        if ((random >> 27) % 4 == 0) height = -height;
        memcpy(s_file + 0x12, &width, 4); memcpy(s_file + 0x16, &height, 4);
        memcpy(s_file + 0x1a, &planes, 2); memcpy(s_file + 0x1c, &d, 2);
        memcpy(s_file + 0x2e, &used, 4);
    }
    for (at = 0xe + header; at < size && at < sizeof(s_file); ++at) s_file[at] = (uint8_t)next_random();
    s_file_size = size < sizeof(s_file) ? size : sizeof(s_file);
    {   /* the .BMP where the file stands, after a few bytes of something else */
        uint32_t start = next_random() % 4 ? 0 : 1 + next_random() % 16;
        if (s_file_size + start <= sizeof(s_file)) {
            memmove(s_file + start, s_file, s_file_size);
            memset(s_file, 0x5a, start);
            s_file_size += start;
            s_file_start = start;
        } else s_file_start = 0;
    }
    switch (next_random() % 12) {
    case 0: s_file[s_file_start + 1] = 'X'; break;
    case 1: { uint32_t odd = 0x20; memcpy(s_file + s_file_start + 0xe, &odd, 4); break; }
    case 2: s_file_size = next_random() % (s_file_size + 1); break;
    case 3: { uint32_t past = size + 1 + next_random() % 64; memcpy(s_file + s_file_start + 0xa, &past, 4); break; }
    case 4: { uint32_t short_size = offset - 1 - next_random() % 4; memcpy(s_file + s_file_start + 2, &short_size, 4); break; }
    default: break;
    }
}
static void shape_object(uint8_t *object)
{
    uint32_t vtable = class_vtable(CURRENT.rva);
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CDIBSection@@6B@") && next_random() % 4) {
        shape_dib(object, s_fixture.pixels[next_random() % 4]);
        return;
    }
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CSimsFont@@6B@")) {
        /* its CFont (a block, its handle there or not), its flags and its tab stops */
        uint32_t random = next_random(), base = random % 8 ? fixture_address(s_fixture.blocks[(random >> 3) % BLOCKS]) : 0;
        uint32_t stops = fixture_address(s_fixture.ints), count = (random >> 8) % 4, at;
        uint16_t format = (uint16_t)(next_random() & 0x63f);
        if ((random >> 12) % 6 == 0) count = 4 + (random >> 16) % 8;
        if (base && (random >> 20) % 5 == 0) memset(s_fixture.blocks[(random >> 3) % BLOCKS] + 4, 0, 4);
        memcpy(object + 4, &base, 4);
        for (at = 0x48; at <= 0x54; at += 4) { uint32_t flag = next_random() % 2; memcpy(object + at, &flag, 4); }
        memcpy(object + 0x58, &format, 2);
        memcpy(object + 0x60, &stops, 4);
        memcpy(object + 0x64, &count, 4);
        for (at = 0x38; at <= 0x44; at += 4) { int32_t value = (int32_t)(next_random() % 24) - 4; memcpy(object + at, &value, 4); }
        return;
    }
    if (vtable < VTABLE_COUNT && strstr(s_vtables[vtable].name, "Button")) {
        /* a CSimsButton: its palette a block (the palette state +0x808 there or not), a
         * frame's height and width small, its strip a section */
        uint32_t random = next_random(), pal = fixture_address(s_fixture.blocks[random % BLOCKS]);
        int32_t height = (int32_t)((random >> 8) % 24), width = (int32_t)((random >> 16) % 40);
        shape_dib(object + 0x40, s_fixture.pixels[(random >> 5) % 4]);
        if ((random >> 24) % 8) memcpy(object + 0xb0, &pal, 4);
        memcpy(object + 0xb4, &height, 4);
        memcpy(object + 0xb8, &width, 4);
        if ((random >> 27) % 2) memcpy(object + 0xbc, (uint8_t *)(uintptr_t)pal + 0x808, 4);
        {   /* a 3-piece button's: pressed, its mask (a pixel area) or none; a small button's window */
            uint32_t more = next_random(), pressed = more % 2, mask = more % 3 ? fixture_address(s_fixture.pixels[(more >> 2) % 4]) : 0;
            uint32_t window = (more >> 4) % 3 ? fixture_address(s_fixture.blocks[(more >> 6) % BLOCKS]) : 0;
            memcpy(object + 0xd4, &pressed, 4);
            memcpy(object + 0xe8, &mask, 4);
            if (strstr(s_vtables[vtable].name, "Help") || strstr(s_vtables[vtable].name, "Exit")) memcpy(object + 0xc0, &window, 4);
            if (strstr(s_vtables[vtable].name, "Slider")) {   /* its geometry: small and consistent */
                int32_t values[4], at;
                for (at = 0; at < 4; ++at) values[at] = (int32_t)((more >> (at * 4)) % 12);
                memcpy(object + 0xcc, &pressed, 4);
                { int32_t rect[4] = {0, 0, 40 + values[0] * 4, 20}; memcpy(object + 0xe0, rect, 16); }
                { int32_t thumb[4] = {values[1], 0, values[1] + 12, 0x13}; memcpy(object + 0xf0, thumb, 16); memcpy(object + 0x100, thumb, 16); }
                { int32_t geometry[8] = {4, 2, 4, 8, 2 + values[2] % 5, 6, 3, 6 * (1 + values[2] % 5)};
                  memcpy(object + 0x120, geometry, 32); }
                { int32_t state[6] = {values[3], (int32_t)(more % 2), 1, 0, 0, values[3] % 3};
                  memcpy(object + 0x140, state, 24); }
                memcpy(object + 0x154, values, 4);
                { int32_t range[2] = {0, 1 + values[2] % 5}; memcpy(object + 0x158, range, 8); }
                memcpy(object + 0x160, &mask, 4);
                { int32_t thumb_size[2] = {6 + values[0], 8 + values[1]}; memcpy(object + 0x248, thumb_size, 8); }
                shape_dib(object + 0x168, s_fixture.pixels[0]); shape_dib(object + 0x1d8, s_fixture.pixels[1]);
                shape_dib(object + 0x250, s_fixture.pixels[2]); shape_dib(object + 0x2c0, s_fixture.pixels[3]);
                shape_dib(object + 0x330, s_fixture.pixels[0]);
                shape_dib(object + 0x3a0, s_fixture.pixels[2]); shape_dib(object + 0x410, s_fixture.pixels[3]);
            }
            if (strstr(s_vtables[vtable].name, "Shadow")) {   /* pressed +0xcc, the mask +0xe0 */
                memcpy(object + 0xcc, &pressed, 4);
                memcpy(object + 0xe0, &mask, 4);
            }
            if (strstr(s_vtables[vtable].name, "Radio")) {    /* its group: a few windows, the last checked */
                uint32_t count = (more >> 8) % 4, at, pressed_too = (more >> 10) % 2;
                for (at = 0; at < 10; ++at) { uint32_t member = 0x5000 + at * 0x10; memcpy(object + 0xc4 + at * 4, &member, 4); }
                memcpy(object + 0xf0, &count, 4);
                memcpy(object + 0xc0, &pressed_too, 4);
                if ((more >> 16) % 2) { uint32_t window = 0x5000 + ((more >> 17) % 4) * 0x10; memcpy(object + 0x20, &window, 4); }
            }
        }
        return;
    }
    if (vtable < VTABLE_COUNT && strstr(s_vtables[vtable].name, "CSimsDialog")) {
        /* its palette (state +0x808), background and tab strip, tab control, buttons */
        uint32_t random = next_random(), tabs = random % 3 ? 0 : fixture_address(s_fixture.blocks[(random >> 2) % 28]);
        int32_t small[4];
        uint32_t at;
        shape_dib(object + 0xff4, s_fixture.pixels[(random >> 8) % 4]);
        shape_dib(object + 0x1064, s_fixture.pixels[(random >> 10) % 4]);
        memcpy(object + 0x7c, &tabs, 4);
        if (tabs && (random >> 24) % 2) {
            /* a whole tab control (blocks 20 to 23) of up to four tabs sharing one page (blocks
             * 14 to 18), tab 0 enabled, so its searches end and its pages call as pages */
            uint8_t *control = s_fixture.blocks[20], *page = s_fixture.blocks[14];
            int32_t last = (int32_t)((random >> 25) % 4), active = (int32_t)((random >> 27) % (uint32_t)(last + 1)), tab;
            uint32_t page_address = fixture_address(page), window = 0x7000;
            tabs = fixture_address(control);
            memcpy(object + 0x7c, &tabs, 4);
            memcpy(control + 0x20, &window, 4);
            memcpy(control + 0xc20, &last, 4);
            memcpy(control + 0xc28, &active, 4);
            for (tab = 0; tab < 7; ++tab) {
                uint32_t disabled = tab && next_random() % 3 == 0;
                memcpy(control + 0x40 + tab * 0x160, &page_address, 4);
                memcpy(control + 0x19c + tab * 0x160, &disabled, 4);
            }
            shape_page(page, next_random());
        } else if (tabs) {
            uint8_t *control = (uint8_t *)(uintptr_t)tabs;
            int32_t active = (int32_t)((random >> 12) % 2);
            uint32_t page = fixture_address(s_fixture.blocks[(random >> 13) % BLOCKS]);
            memcpy(control + 0xc28, &active, 4);
            memcpy(control + 0x40 + active * 0x160, &page, 4);
        }
        if ((random >> 16) % 4 == 0) { int32_t none = -1; memcpy(object + 0x88, &none, 4); }
        {   /* the edit being validated: none, or an id commands use */
            static const uint32_t ids[7] = {0, 1, 2, 0x2711, 3, 4, 0x3e8};
            uint32_t edit = ids[next_random() % 7], kept = next_random() % 2 ? 0 : 0x5100;
            memcpy(object + 0x7d0, &edit, 4);
            memcpy(object + 0x7e4, &kept, 4);
            {   /* OK or Cancel clicked, mostly not */
                uint32_t clicked = next_random() % 4 == 0;
                memcpy(object + 0x7e0, &clicked, 4);
            }
        }
        for (at = 0; at < 4; ++at) small[at] = (int32_t)(next_random() % 40);
        memcpy(object + 0x14c, small, 8);
        memcpy(object + 0x210, small + 2, 8);
        if ((random >> 18) % 2) memset(object + 0x7e8 + 0x808, 0, 4);
        return;
    }
    if (vtable < VTABLE_COUNT && strstr(s_vtables[vtable].name, "Groupbox")) {
        uint32_t random = next_random(), font = random % 3 ? fixture_address(object + 0x40) : 0;
        uint32_t base = fixture_address(s_fixture.blocks[(random >> 2) % BLOCKS]);
        memcpy(object + 0xb0, &font, 4);
        memcpy(object + 0x44, &base, 4);
        memset(object + 0x64 + 0x40, 0, 4);
        return;
    }
    if (vtable < VTABLE_COUNT && (!strcmp(s_vtables[vtable].name, "??_7CSimsHotKey@@6B@") ||
                                  !strcmp(s_vtables[vtable].name, "??_7CSimsHotJoy@@6B@"))) {
        /* its font (a CSimsFont, or now and then none), a key and its modifiers */
        uint32_t random = next_random(), font = argument(ARG_FONT), code = random & 0xff, modifiers = (random >> 8) % 16;
        memcpy(object + 0x4c, &font, 4);
        memcpy(object + 0x58, &code, 4);
        memcpy(object + 0x5c, &modifiers, 4);
        return;
    }
    if (!strncmp(CURRENT.name, "cdib_", 5)) {
        /* a CDIB: its info (small, 8 bits, a few colours used) in a block, its bits a pixel
         * buffer, owned or not; its vtable the class's */
        uint32_t random = next_random(), info = fixture_address(s_fixture.blocks[1 + 4 * ((random >> 4) % 6)]);
        uint32_t bits = fixture_address(s_fixture.pixels[(random >> 8) % 4]), owned = random % 2;
        int32_t width = 1 + (int32_t)((random >> 10) % 40), height = 1 + (int32_t)((random >> 16) % 40);
        uint8_t *at = (uint8_t *)(uintptr_t)info;
        uint32_t header = 0x28, used = (random >> 22) % 3 == 0 ? (random >> 24) % 0x100 : 0;
        uint16_t planes = 1, depth = (random >> 30) % 2 ? 8 : 4;
        uint32_t vtable = BASE + 0x15fe0;
        memcpy(at, &header, 4); memcpy(at + 4, &width, 4); memcpy(at + 8, &height, 4);
        memcpy(at + 0xc, &planes, 2); memcpy(at + 0xe, &depth, 2); memcpy(at + 0x20, &used, 4);
        memcpy(object, &vtable, 4);
        memcpy(object + 4, &info, 4);
        memcpy(object + 8, &bits, 4);
        memcpy(object + 0xc, &owned, 4);
        return;
    }
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CSimsTextArt@@6B@")) {
        uint32_t random = next_random(), pal = random % 5 ? fixture_address(s_fixture.blocks[(random >> 3) % BLOCKS]) : 0;
        int32_t place[4] = {2, 3, 2 + (int32_t)((random >> 8) % 40), 3 + (int32_t)((random >> 14) % 20)};
        shape_dib(object + 0x64, s_fixture.pixels[(random >> 20) % 4]);
        memcpy(object + 0x54, place, 16);
        memcpy(object + 0xdc, &pal, 4);
        return;
    }
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CSimsText@@6B@")) {
        /* its font (its own, or none) and its text, a CString the model made */
        uint32_t random = next_random(), font = random % 3 ? fixture_address(object + 0x40) : 0;
        uint32_t base = fixture_address(s_fixture.blocks[(random >> 2) % BLOCKS]), stops = fixture_address(s_fixture.ints), none = 0;
        memcpy(object + 0xb0, &font, 4);
        memcpy(object + 0x44, &base, 4);
        memcpy(object + 0x60 + 0x40, &stops, 4);
        memcpy(object + 0x64 + 0x40, &none, 4);
        { uint32_t text = fixture_address(s_fixture.blocks[((random >> 8) % (BLOCKS / 4)) * 4 + 3] + 0x100);
          static const char words[] = "\x01\0\0\0\x05\0\0\0\x08\0\0\0Hello";
          memcpy(s_fixture.blocks[((random >> 8) % (BLOCKS / 4)) * 4 + 3] + 0xf4, words, sizeof(words));
          memcpy(object + 0xb4, &text, 4); }
        return;
    }
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CSimsTabCtrl@@6B@")) {
        /* a few tabs (pages at +0xd00 or none), their sizes, the strip and sections */
        uint32_t random = next_random(), last = random % 4, at, page = fixture_address(object + 0xd00);
        int32_t sizes[2] = {5 + (int32_t)((random >> 4) % 16), 20 + (int32_t)((random >> 8) % 40)};
        uint32_t pal = fixture_address(s_fixture.blocks[(random >> 12) % BLOCKS]), background = fixture_address(s_fixture.dibs[0]);
        uint32_t mask = fixture_address(s_fixture.pixels[(random >> 14) % 4]);
        int32_t counts[3] = {(int32_t)last, (int32_t)last + 1, (int32_t)((random >> 16) % (last + 1))};
        for (at = 0; at < 7; ++at) {
            uint8_t *entry = object + 0x40 + at * 0x160;
            uint32_t disabled = at && (next_random() % 4) == 0, chosen = next_random() % 5 ? page : 0;  /* tab 0 enabled: a search for one ends */
            memcpy(entry, &chosen, 4);
            shape_dib(entry + 4, s_fixture.pixels[at % 4]);
            shape_dib(entry + 0x74, s_fixture.pixels[(at + 1) % 4]);
            shape_dib(entry + 0xe4, s_fixture.pixels[(at + 2) % 4]);
            entry[0x158] = (uint8_t)('A' + at);
            memcpy(entry + 0x15c, &disabled, 4);
        }
        for (at = 0; at < 5; ++at) shape_dib(object + (at ? 0xa54 + (at - 1) * 0x70 : 0x9e0), s_fixture.pixels[at % 4]);
        {   /* the strip's height: under 0xa0 it holds two frames a tab, else four */
            static const int32_t heights[4] = {0x9f, 0xa0, 0xc8, 0};
            int32_t height = heights[next_random() % 4];
            if (height) memcpy(object + 0x9ec, &height, 4);
        }
        memcpy(object + 0xa50, &background, 4);
        memcpy(object + 0xc14, &mask, 4);
        memcpy(object + 0xc20, counts, 12);
        memcpy(object + 0xc38, &pal, 4);
        memcpy(object + 0xc40, sizes, 8);
        { int32_t offset = (int32_t)((random >> 20) % 4); memcpy(object + 0xc4c, &offset, 4); }
        { int32_t pressed = (random >> 22) % 2 ? -1 : (int32_t)((random >> 23) % 4); memcpy(object + 0xc6c, &pressed, 4); }
        { int32_t rect[4] = {0, counts[2] * sizes[0], sizes[1], (counts[2] + 1) * sizes[0]}; memcpy(object + 0xc90, rect, 16); }
        { uint32_t info = fixture_address(s_fixture.blocks[((random >> 24) % 7) * 4 + 1]), font = fixture_address(s_fixture.blocks[0]);
          memcpy(object + 0xcbc, &info, 4); memcpy(object + 0xc60, &font, 4); }
        shape_page(object + 0xd00, next_random());
        return;
    }
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CSimsTabPage@@6B@")) {
        uint32_t random = next_random(), picture = fixture_address(s_fixture.dibs[random % 4]);
        uint32_t pal = fixture_address(s_fixture.blocks[(random >> 2) % BLOCKS]);
        shape_page(object, random >> 8);
        memcpy(object, &object, 0);
        { void **vtable_page = s_fake_vtables[vtable]; memcpy(object, &vtable_page, 4); }
        memcpy(object + 0x10fc, &picture, 4);
        memcpy(object + 0x10dc, &pal, 4);
        shape_dib(object + 0xff4, s_fixture.pixels[(random >> 6) % 4]);
        return;
    }
    if (vtable < VTABLE_COUNT && strstr(s_vtables[vtable].name, "CSimsEdit")) {
        uint32_t random = next_random(), font = random % 4 ? fixture_address(s_fixture.blocks[(random >> 2) % BLOCKS]) : 0;
        uint32_t flags = (random >> 8) & ((random >> 28) % 8 ? 0xf7 : 0xff), limit = (random >> 16) % 8;
        memcpy(object + 0x50, &font, 4);
        memcpy(object + 0x60, &flags, 4);
        memcpy(object + 0x64, &limit, 4);
        return;
    }
    if (vtable < VTABLE_COUNT && !strcmp(s_vtables[vtable].name, "??_7CSimsShadow@@6B@")) {
        uint32_t random = next_random(), pal = random % 5 ? fixture_address(s_fixture.blocks[(random >> 3) % BLOCKS]) : 0;
        int32_t place[4], at;
        shape_dib(object + 4, s_fixture.pixels[(random >> 8) % 4]);
        for (at = 0; at < 4; ++at) place[at] = (int32_t)(next_random() % 40);
        memcpy(object + 0x74, place, 16);
        memcpy(object + 0x84, &pal, 4);
        return;
    }
    if (vtable < VTABLE_COUNT && strstr(s_vtables[vtable].name, "CSimsPicture@")) {
        /* its palette (a block), image size, section, whether it has an image */
        uint32_t random = next_random(), pal = fixture_address(s_fixture.blocks[random % BLOCKS]), none = (random >> 8) % 4 == 0;
        int32_t height = (int32_t)((random >> 10) % 40), width = (int32_t)((random >> 16) % 40);
        shape_dib(object + 0x4c, s_fixture.pixels[(random >> 24) % 4]);
        memcpy(object + 0x40, &pal, 4);
        memcpy(object + 0x44, &height, 4);
        memcpy(object + 0x48, &width, 4);
        memcpy(object + 0xbc, &none, 4);
        return;
    }
    if (vtable < VTABLE_COUNT && strstr(s_vtables[vtable].name, "ListBox")) {
        /* its CSimsText and CSimsFont, at +0x40 (+0x48 for the check list box) */
        uint32_t at = strstr(s_vtables[vtable].name, "Check") ? 0x48 : 0x40;
        uint32_t random = next_random(), text = random % 4 ? fixture_address(s_fixture.blocks[(random >> 2) % BLOCKS]) : 0;
        uint32_t font = (random >> 8) % 6 ? fixture_address(s_fixture.blocks[((random >> 10) % (BLOCKS / 4)) * 4]) : 0;
        memcpy(object + at, &text, 4);
        memcpy(object + at + 4, &font, 4);
        if (font) {
            uint8_t *base = (uint8_t *)(uintptr_t)font;
            uint32_t cfont = fixture_address(s_fixture.blocks[(random >> 16) % BLOCKS]), stops = fixture_address(s_fixture.ints);
            uint32_t count = (random >> 20) % 3;
            memcpy(base + 4, &cfont, 4);
            memcpy(base + 0x60, &stops, 4);
            memcpy(base + 0x64, &count, 4);
        }
        return;
    }
    if (vtable < VTABLE_COUNT && strstr(s_vtables[vtable].name, "ComboBox")) {
        /* its CSimsText (a block, or none) and its CSimsFont (a block shaped as one, or none) */
        uint32_t random = next_random(), text = random % 4 ? fixture_address(s_fixture.blocks[(random >> 2) % BLOCKS]) : 0;
        uint32_t font = (random >> 8) % 6 ? fixture_address(s_fixture.blocks[((random >> 10) % (BLOCKS / 4)) * 4]) : 0;
        memcpy(object + 0x40, &text, 4);
        memcpy(object + 0x44, &font, 4);
        if (font) {
            uint8_t *at = (uint8_t *)(uintptr_t)font;
            uint32_t base = fixture_address(s_fixture.blocks[(random >> 16) % BLOCKS]), stops = fixture_address(s_fixture.ints);
            uint32_t count = (random >> 20) % 3, flag;
            memcpy(at + 4, &base, 4);
            for (flag = 0x48; flag <= 0x54; flag += 4) { uint32_t on = (random >> (flag / 4)) & 1; memcpy(at + flag, &on, 4); }
            memcpy(at + 0x60, &stops, 4);
            memcpy(at + 0x64, &count, 4);
        }
        return;
    }
    if (next_random() % 2) shape_info(object + 0x1c);
}
