/* The window procedures', startup's and the loop's part of the harness (host_rt_main.c,
 * host_rt_frame.c, host_rt_child.c, host_rt_floating.c, host_rt_layout.c, host_rt_mouse.c,
 * host_rt_misc.c): the fixtures they read (the windows' data and bitmaps, hot spots, the
 * lists, the voices, the callbacks the game set), what the imports and services they call
 * do, and what their arguments point to. Included after hostrt_semantics.inc. */

#define DATA_AT_VA(va) (s_fixture.data + (va) - BASE - DATA_RVA)
#define IMAGE32(va) (*(uint32_t *)(uintptr_t)(va))
#define IMAGE32_INITIAL(va) (*(uint32_t *)DATA_AT_VA(va))   /* .data as the scenario begins */
static void set_data(uint32_t va, uint32_t value) { memcpy(DATA_AT_VA(va), &value, 4); }
static void set_word(uint8_t *at, uint32_t value) { memcpy(at, &value, 4); }
static uint32_t stub_address(void *stub) { return (uint32_t)(uintptr_t)stub; }
static uint32_t block_address(uint32_t block, uint32_t offset) { return fixture_address(s_fixture.blocks[block] + offset); }
static int32_t small_signed(uint32_t random, uint32_t range, int32_t low) { return (int32_t)(random % range) + low; }

/* ---- .data the scenario shapes ---------------------------------------------------------- */

static int window_data_fixed(uint32_t rva)
{
    static const uint32_t words[] = {0x18838, 0x1883c, 0x18840, 0x18844, 0x18820, 0x185e8, 0x18798, 0x1a3c8, 0x1a2b8,
                                     0x1a3c4, 0x1a3c0, 0x1a3dc, 0x1888c, 0x1ba28, 0x1b608, 0x17024, 0x17a1c, 0x1bb5c,
                                     0x1881c, 0x1bb38};
    uint32_t at;
    for (at = 0; at < sizeof words / sizeof words[0]; ++at)
        if (rva == words[at]) return 1;
    return (rva >= 0x18278 && rva < 0x18298) || (rva >= 0x17f00 && rva < 0x17f1c) || (rva >= 0x17ae8 && rva < 0x17b04);
}

/* a list header (the growable array) over data in a block */
static void shape_list(uint8_t *header, uint32_t count, uint32_t size, uint32_t data)
{
    uint32_t words[7] = {count, count + 2, 0x10, size, 0, 0, count ? data : 0};
    memcpy(header, words, sizeof words);
}

/* a small rectangle */
static void shape_rect(uint8_t *at, uint32_t random)
{
    static const int32_t sizes[] = {0, 16, 32, 48, 64, 100};
    int32_t rect[4];
    rect[0] = random % 3 ? 0 : small_signed(random, 30, -2);
    rect[1] = random % 4 ? 0 : small_signed(random >> 5, 30, -2);
    rect[2] = rect[0] + (random % 5 ? sizes[(random >> 10) % 6] : small_signed(random >> 10, 50, -3));
    rect[3] = rect[1] + (random % 7 ? sizes[(random >> 16) % 6] : small_signed(random >> 16, 50, -3));
    memcpy(at, rect, 16);
}

/* a mouse description (block 34): areas with their spots, as 0x406fc0 walks them, and the
 * hot-spot elements (block 34 + 0x300) that point at it; rectangles in block 35 */
static void shape_hot_spots(void)
{
    static const uint8_t kinds[2][10] = {{1, 2, 2, 3, 0, 1, 2, 3, 0, 0}, {1, 2, 1, 2, 3, 2, 3, 0, 0, 0}};
    uint32_t shape = next_random() % 2;
    uint32_t at, *entries = (uint32_t *)s_fixture.blocks[34];
    memset(entries, 0, 0x300);
    for (at = 0; at < 10; ++at) {
        uint32_t random = next_random(), *entry = entries + at * 11;
        entry[0] = kinds[shape][at] == 0 && at == 4 && random % 3 ? 2 : kinds[shape][at];
        if (at == 8) entry[0] = 0;
        entry[5] = random % 5 == 0 ? 0xffffffffu : (random >> 3) % 8;
        entry[7] = random % 9 == 0 ? 0 : (random & 0xffff0000u) | ((random >> 4) % 3 == 0 ? 0x200000 : 0);
        entry[8] = random % 4 == 0 ? (random >> 8) % 50 : 0;
        entry[9] = random % 3 ? stub_address(s_callback_stub) : 0;
        entry[10] = block_address(35, (at % 8) * 0x10);
        shape_rect(s_fixture.blocks[35] + (at % 8) * 0x10, next_random());
    }
    for (at = 0; at < 4; ++at) {        /* the elements: a rectangle and the description */
        uint8_t *element = s_fixture.blocks[34] + 0x300 + at * 0x14;
        shape_rect(element, next_random());
        set_word(element + 0x10, block_address(34, 0));
    }
    set_data(0x41888c, next_random() % 4 == 0 ? block_address(34, (next_random() % 7) * 0x2c) : 0);
}

/* a window region's shape (block 32): FS6BOUND rectangles and polygons, or a span per row */
static void shape_region(void)
{
    uint8_t *shape = s_fixture.blocks[32];
    uint32_t random = next_random(), at, count = random % 4, offset = 0x10;
    memset(shape, 0, 0x400);
    if (random & 0x10) {
        uint16_t *spans = (uint16_t *)shape;
        for (at = 0; at < 0x100; ++at) {
            uint32_t r = next_random();
            spans[at * 2] = (uint16_t)(r % 20);
            spans[at * 2 + 1] = (uint16_t)(spans[at * 2] + (r >> 8) % 40);
        }
        return;
    }
    memcpy(shape, "FS6BOUND", 8);
    set_word(shape + 0xc, count);
    for (at = 0; at < count && offset < 0x380; ++at) {
        uint32_t r = next_random();
        if (r % 3 == 0) {
            set_word(shape + offset, 1);
            shape_rect(shape + offset + 4, next_random());
            offset += 0x14;
        } else if (r % 3 == 1) {
            uint32_t points = 3 + (r >> 2) % 3, point;
            set_word(shape + offset, 2);
            set_word(shape + offset + 4, points);
            for (point = 0; point < points * 2; ++point) {   /* now and then far enough out to carry into bit 31 */
                uint32_t p = next_random();
                set_word(shape + offset + 8 + point * 4, p % 5 ? p % 60 : 0x8000 + p % 0x20000);
            }
            offset += 8 + points * 8;
        } else {
            set_word(shape + offset, 5 + (r >> 2) % 3);
            offset += 4;
        }
    }
}

/* the windows' data (blocks 5, 13) and bitmaps (6, 14) the procedures and the layout read */
static void shape_window_data(uint8_t *data, uint8_t *bitmap)
{
    uint32_t random = next_random(), r2 = next_random(), at, flags;
    int32_t value;
    memcpy(&flags, data + 8, 4);
    flags |= r2 % 3 == 0 ? (r2 >> 4) & 0x0ff90e00u : 0;
    if (random % 7 == 0) flags |= 0x80400;
    if ((r2 >> 9) % 3 == 0) flags |= 0x8000000;      /* placed by hand: not docked from its units */
    if (random % 5 == 0) flags |= 0x60000000u & (r2 << 3);
    memcpy(data + 8, &flags, 4);
    set_word(data + 0x10, random % 3 == 0 ? block_address(32, 0) : 0);
    set_word(data + 0x18, random % 4 ? stub_address(s_callback4_stub) : 0);
    set_word(data + 0x1c, (random >> 2) % 3 ? block_address(33, 0) : 0);
    for (at = 0x20; at < 0x30; at += 4) set_word(data + at, next_random() % 0x30000);
    for (at = 0x30; at < 0x60; at += 4) {   /* places and sizes, often ones that meet the harness's rectangles */
        static const int32_t usual[] = {0, 16, 32, 48, 64, 100, -10, 0};
        uint32_t r = next_random();
        value = r % 3 ? usual[(r >> 2) % 8] : small_signed(r >> 2, 200, -10);
        memcpy(data + at, &value, 4);
    }
    set_word(data + 0x9c, next_random());
    if (r2 % 2)     /* else the size shape_fixture gave (the floating windows' moves) */
        for (at = 0x60; at < 0x68; at += 4) set_word(data + at, next_random() % 3 ? 16 * (next_random() % 5) : 1 + next_random() % 64);
    {   /* the tracking sizes: none, fixed both ways, one way only, or ranges */
        uint32_t r = next_random(), width = 10 + r % 50, height = 10 + (r >> 8) % 50, kind = (r >> 16) % 8;
        uint32_t across[2] = {0, 0}, down[2] = {0, 0};
        if (kind == 1 || kind == 2 || kind == 5) { across[0] = across[1] = width; }
        if (kind == 1 || kind == 3 || kind == 5) { down[0] = down[1] = height; }
        if (kind == 4) { across[0] = width; across[1] = width + 20; down[0] = height; down[1] = height + 5; }
        if (kind == 6) { across[0] = across[1] = width; down[0] = height; down[1] = height + 5; }    /* fixed across only */
        if (kind == 7) { across[0] = width; across[1] = width + 20; down[0] = down[1] = height; }    /* fixed down only */
        set_word(data + 0x68, across[0]);
        set_word(data + 0x70, across[1]);
        set_word(data + 0x6c, down[0]);
        set_word(data + 0x74, down[1]);
    }
    set_word(data + 0x7c, (random >> 4) % 3 == 0 ? 0x77 + (random >> 6) % 2 : 0);
    set_word(data + 0x80, 1 + (random >> 8) % 6);
    set_word(data + 0x90, (random >> 11) % 3 ? 0 : block_address(25, 0));
    /* the bitmap */
    set_word(bitmap, (r2 >> 1) % 2 ? IMAGE32_INITIAL(0x41ba2c) : next_random());
    set_word(bitmap + 4, (r2 >> 2) % 2 ? IMAGE32_INITIAL(0x41bb2c) : next_random());
    set_word(bitmap + 0x24, (r2 >> 3) % 4 ? 0x4000 + (r2 >> 5) % 8 : 0);
    set_word(bitmap + 0x28, 0x4100 + (r2 >> 8) % 8);
    set_word(bitmap + 0x2c, (r2 >> 11) % 3 ? 0x4200 : 0);
    set_word(bitmap + 0x30, 0x6000 + (r2 >> 13) % 4);
    set_word(bitmap + 0x34, 0x7000 + (r2 >> 15) % 4);
    set_word(bitmap + 0x38, 0x7100 + (r2 >> 17) % 4);
    set_word(bitmap + 0x3c, (r2 >> 19) % 3 ? 0x5000 : 0);
    for (at = 0x44; at < 0x54; at += 4) set_word(bitmap + at, next_random() % 3 ? 0x10000 : 0x8000 + next_random() % 0x18000);
    shape_list(bitmap + 0x54, next_random() % 4, 0x10, block_address(35, 0x200));
    shape_list(bitmap + 0x70, next_random() % 3, 0x14, block_address(34, 0x300));
    set_word(bitmap + 0x8c, (r2 >> 21) % 3 ? com_new(COM_CLIPPER) : 0);
}

static int s_paint_focus;
/* after shape_data's own words: what the windows' scenarios need of them */
static void shape_windows_late(void)
{
    uint32_t random = next_random();
    if (random % 3 == 0) {      /* the primary's clipper a window's */
        uint32_t clipper;
        memcpy(&clipper, s_fixture.blocks[(random >> 2) % 2 ? 6 : 14] + 0x8c, 4);
        set_data(0x41a4f0, clipper);
    }
    if (s_paint_focus) {
        set_data(0x41a4f4, s_paint_focus == 2);
        set_data(0x418858, 1);
        set_data(0x41a250, 0);
        if (s_paint_focus == 2) set_data(0x4183b4, 1 + (random >> 4) % 3);
    }
    {   /* the mouse the game's (0x41881c) or not; for the hit test mostly not, and often inactive (no hot spots) */
        static const uint32_t captured[] = {0, 0, 1, 2, 0x34000000};
        set_data(0x41881c, captured[(random >> 14) % 5]);
        if (!strcmp(CURRENT.name, "window_mouse") && (random >> 17) % 4) {
            set_data(0x41881c, 0);
            if ((random >> 6) % 2) set_data(0x418858, 0);
        }
    }
    set_data(0x41bb38, (random >> 8) % 2 ? 0x1234 : 0);
    if (!strcmp(CURRENT.name, "main_procedure") && (random >> 10) % 3 == 0) {   /* full screen, GDI's or DirectDraw's */
        uint32_t options;
        memcpy(&options, DATA_AT_VA(0x41a4ec), 4);
        set_data(0x41a4ec, options | ((random >> 12) % 2 ? 4 : 0));
        set_data(0x418854, 1);
        set_data(0x41bb58, 0);
    }
}

static void shape_windows(void)
{
    uint32_t at, random;
    /* the callbacks the game set: the timer's, its start, its end, the speed setting, the mouse */
    set_data(0x418838, next_random() % 5 ? stub_address(s_callback0_stub) : 0);
    set_data(0x41883c, next_random() % 6 ? stub_address(s_callback0_stub) : 0);
    set_data(0x418840, next_random() % 6 ? stub_address(s_callback0_stub) : 0);
    set_data(0x418844, next_random() % 6 ? stub_address(s_callback1_stub) : 0);
    set_data(0x418820, next_random() % 6 ? stub_address(s_callback3_stub) : 0);
    /* the main window's CWnd (0x418798) and the application: their vtables logging stubs */
    set_data(0x418798, (uint32_t)(uintptr_t)s_com_vtables[COM_WND]);
    set_data(0x4185e8, next_random() % 8 ? 0x418798 : 0);
    memcpy(s_fixture.object, &s_com_vtables[COM_WND], 4);
    /* DirectSound, its primary buffer and library; the voices (blocks 38, 39) */
    random = next_random();
    set_data(0x41a3c8, random % 2 ? com_new(COM_SOUND) : 0);
    set_data(0x41a2b8, (random >> 1) % 2 ? com_new(COM_BUFFER) : 0);
    set_data(0x41a3c4, (random >> 2) % 3 ? 0x10200000u : 0);
    for (at = 0; at < 2; ++at) {
        uint8_t *voice = s_fixture.blocks[38 + at];
        uint32_t r = next_random();
        set_word(voice + 0x58, r % 3 ? stub_address(s_callback1_stub) : 0);
        set_word(voice + 0x5c, (r >> 2) % 3 ? 1 : 0);
        set_word(voice + 0x60, (r >> 4) % 5 ? com_new(COM_BUFFER) : 0);
        set_word(voice + 0x168, at == 0 && (r >> 6) % 2 ? block_address(39, 0) : 0);
    }
    set_data(0x41a3c0, (random >> 4) % 3 ? block_address(38, 0) : 0);
    /* the frames' groups (block 40) and their nodes (block 41) */
    for (at = 0; at < 2; ++at) {
        uint8_t *group = s_fixture.blocks[40] + at * 0x20, *node = s_fixture.blocks[41] + at * 0x10;
        set_word(group, at);
        set_word(group + 4, block_address(41, at * 0x10));
        set_word(group + 8, at == 0 && next_random() % 2 ? block_address(40, 0x20) : 0);
        set_word(node, block_address(at ? 13 : 5, 0));
        set_word(node + 4, next_random() % 2 ? block_address(41, at * 0x10 + 8) : 0);
        set_word(node + 8, block_address(at ? 5 : 13, 0));
        set_word(node + 0xc, 0);
    }
    set_data(0x41a3dc, (random >> 6) % 3 ? block_address(40, 0) : 0);
    /* the key tables (0x418278; tables and entries in block 37) */
    {
        uint8_t *tables = s_fixture.blocks[37] + 0x200;
        uint32_t count = next_random() % 3;
        shape_list(DATA_AT_VA(0x418278), count, 0x10, block_address(37, 0x200));
        for (at = 0; at < 3; ++at) {
            set_word(tables + at * 0x10, next_random() % 4 ? 1 : 0);
            set_word(tables + at * 0x10 + 4, 0);
            set_word(tables + at * 0x10 + 8, block_address(37, 0x100 + at * 0x30));
            set_word(tables + at * 0x10 + 0xc, next_random() % 4);
        }
        for (at = 0; at < 12; ++at) {
            uint8_t *entry = s_fixture.blocks[37] + 0x100 + at * 0xc;
            uint32_t r = next_random();
            set_word(entry, r % 2 ? 0x41 + (r >> 1) % 4 : 0x10 + (r >> 1) % 4);
            set_word(entry + 4, (r >> 4) & 0x17);
            set_word(entry + 8, 0x10000 + (r >> 8) % 0x100);
        }
        set_word(s_fixture.blocks[37] + 0x80, 0x41 + next_random() % 4);
        set_word(s_fixture.blocks[37] + 0x84, 0x10 + next_random() % 4);
        set_word(s_fixture.blocks[37] + 0x88, next_random() % 8);
        set_word(s_fixture.blocks[37] + 0x8c, next_random() % 2);
    }
    /* the modeless dialogs (0x417f00; {module, window} in block 47) */
    shape_list(DATA_AT_VA(0x417f00), next_random() % 3, 8, block_address(47, 0));
    for (at = 0; at < 3; ++at) {
        uint8_t *module = s_fixture.blocks[46] + 0x300 + at * 0x10;   /* a module entry: its id, its flags */
        set_word(module + 4, 1 + next_random() % 4);
        set_word(module + 0xc, next_random() % 16);
        set_word(s_fixture.blocks[47] + at * 8, block_address(46, 0x300 + at * 0x10));
        set_word(s_fixture.blocks[47] + at * 8 + 4, 0x4300 + at);
    }
    /* the shared pictures (0x417ae8; entries in block 42, their windows at 42 + 0x100) */
    shape_list(DATA_AT_VA(0x417ae8), next_random() % 3, 0x44, block_address(42, 0));
    for (at = 0; at < 2; ++at) {
        uint8_t *shared = s_fixture.blocks[42] + at * 0x44;
        uint32_t r = next_random(), word;
        set_word(shared, 0x77 + at);
        for (word = 4; word < 0x28; word += 4) set_word(shared + word, next_random() % 0x50);
        set_word(shared + 0x24, 0x6100 + at);
        shape_list(shared + 0x28, r % 3, 4, block_address(42, 0x100 + at * 8));
        set_word(s_fixture.blocks[42] + 0x100 + at * 8, block_address(5, 0));
        set_word(s_fixture.blocks[42] + 0x104 + at * 8, block_address(13, 0));
    }
    /* the windows' data and bitmaps, their hot spots, regions and views */
    shape_window_data(s_fixture.blocks[5], s_fixture.blocks[6]);
    shape_window_data(s_fixture.blocks[13], s_fixture.blocks[14]);
    /* a scenario shaped for a game window's paint by DirectDraw (A) or the full screen's (B) */
    s_paint_focus = !strcmp(CURRENT.name, "child_procedure") ? next_random() % 3 : 0;
    if (s_paint_focus) {
        uint32_t surface, at2;
        memcpy(&surface, DATA_AT_VA(0x41a4e0), 4);
        if (!surface) set_data(0x41a4e0, com_new(COM_SURFACE));
        for (at2 = 0; at2 < 2; ++at2) {
            uint8_t *data = s_fixture.blocks[at2 ? 13 : 5];
            uint32_t flags;
            set_word(data + 0x7c, 0x77 + at2);
            memcpy(&flags, data + 8, 4);
            flags &= ~0x10000u;
            if (s_paint_focus == 2) flags |= 0x40000000u;
            memcpy(data + 8, &flags, 4);
        }
    }
    shape_hot_spots();
    shape_region();
    set_word(s_fixture.blocks[33] + 0x10, next_random() % 3 == 0);
    set_word(s_fixture.blocks[33] + 0x14, next_random() % 6);
    {   /* a mouse context (block 36) */
        static const uint32_t messages[] = {0x84, 0xa4, 0x200, 0x201, 0x202, 0x203, 0x204, 0x206, 0x207, 0x209, 0x468, 0x46a, 0x205};
        uint8_t *context = s_fixture.blocks[36];
        uint32_t r = next_random();
        set_word(context, 0x4000 + r % 4);
        set_word(context + 4, messages[(r >> 2) % 13]);
        set_word(context + 8, (r >> 6) & 0x13);
        set_word(context + 0xc, next_random() % 0x400000);
        set_word(context + 0x10, block_address(r & 0x1000 ? 5 : 13, 0));
        set_word(context + 0x14, block_address(r & 0x2000 ? 6 : 14, 0));
        set_word(context + 0x18, next_random() % 64);
        set_word(context + 0x1c, next_random() % 64);
    }
    {   /* a message's parameters (block 43): CREATESTRUCT, COPYDATASTRUCT or a rectangle */
        uint8_t *parameters = s_fixture.blocks[43];
        uint32_t r = next_random();
        shape_rect(parameters, next_random());
        if (r % 3 == 0) set_word(parameters, block_address(r & 8 ? 5 : 13, 0));
        else if (r % 3 == 1) {
            set_word(parameters, 0x10000000);
            set_word(parameters + 4, 0x114);
            set_word(parameters + 8, block_address(44, 0x200));
            set_word(s_fixture.blocks[44] + 0x200, (r >> 4) % 3 ? 1 : 0);
        }
    }
    set_data(0x417a1c, next_random() % 4);
    set_data(0x41bb5c, next_random() % 2 ? next_random() : 0x14cf0000);
    set_word(s_fixture.blocks[44] + 0x28, 0x1c0000 | (next_random() & 0x40000));
    /* the modes repeated and the pause flags */
    set_data(0x41ba28, 0);
    set_data(0x41b608, 0);
    if (next_random() % 2) set_data(0x41c5a0, 0);
}

/* ---- arguments ------------------------------------------------------------------------------ */

/* a procedure's message, and its parameters to suit it (the message is drawn first) */
static uint32_t s_last_message, s_procedure;
static void log_words_described(uint32_t address, uint32_t count);
static uint32_t pick(const uint32_t *values, uint32_t count, uint32_t random) { return values[random % count]; }
#define PICK(array, random) pick(array, sizeof array / sizeof array[0], random)
static uint32_t coordinate(uint32_t random)
{
    static const uint32_t places[] = {0, 5, 9, 10, 11, 15, 16, 32, 48, 50, 85, 89, 90, 91, 95, 100, 0xfffb, 0xffff};
    return random % 5 == 0 ? (random >> 3) % 300 : PICK(places, random >> 3);
}
static uint32_t message_w(uint32_t message, uint32_t random)
{
    static const uint32_t activate[] = {0, 1, 2, 0x10000, 0x10001, 0x20000};
    static const uint32_t keys[] = {0x12, 0x41, 0x42, 0xd, 0x10, 0x11};
    static const uint32_t commands[] = {0x1103, 0x110a, 0x1601, 0x1602, 0x1603, 0x1604, 0x1608, 0x1609, 0x160a, 0x1661, 0x1666,
        0x1668, 0x1669, 0x166a, 0x1500, 0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x200, 0x201, 0x202, 0x300, 0x400, 0x500};
    static const uint32_t system[] = {0xf060, 0xf061, 0xf140, 0xf141, 0xf150, 0xf170, 0xf172, 0xf100};
    static const uint32_t hits[] = {0, 1, 2, 3, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12};
    switch (message) {
    case 6: return PICK(activate, random);
    case 0x1c: case 0x48: case 0x30f: case 0x46e: return random % 3;
    case 0x20: return random % 2 ? 2 : 0x4000 + random % 4;
    case 0x100: case 0x104: return PICK(keys, random);
    case 0x111:
        if (s_procedure == ARG_CHILD_MESSAGE) {     /* the floating windows' menu */
            static const uint32_t own[] = {0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x200, 0x201, 0x202, 0x300, 0x400, 0x500,
                                           0x500, 0x500};
            return PICK(own, random);
        }
        return PICK(commands, random) | (random % 4 == 0 ? ((random >> 8) % 4) << 16 : 0);
    case 0x112: return PICK(system, random);
    case 0x84: case 0xa0: case 0xa1: case 0xa4: return PICK(hits, random);
    case 0x200: case 0x201: case 0x202: case 0x203: case 0x204: case 0x205: case 0x206: case 0x207: case 0x208: case 0x209:
        return (random >> 2) & 0x13;
    default: return random % 6 == 0 ? mixed(random >> 3) : random % 3;
    }
}
static uint32_t message_l(uint32_t message, uint32_t random)
{
    uint8_t *parameters = s_fixture.blocks[43];
    switch (message) {
    case 1:         /* CREATESTRUCT: the window's data */
        set_word(parameters, block_address(random & 1 ? 5 : 13, 0));
        return block_address(43, 0);
    case 0x4a:      /* COPYDATASTRUCT */
        set_word(parameters, random % 4 ? 0x10000000 : 0x10000001);
        set_word(parameters + 4, 0x114);
        set_word(parameters + 8, block_address(44, 0x200));
        set_word(s_fixture.blocks[44] + 0x200, (random >> 4) % 3 ? 1 : 0);
        return block_address(43, 0);
    case 0x24: case 0x214: case 0x216:      /* MINMAXINFO, a rectangle being dragged */
        shape_rect(parameters, random >> 2);
        return block_address(43, 0);
    case 0x467:     /* the dialog's data */
        return block_address(random & 1 ? 5 : 13, 0);
    case 0x46e:     /* the window gone: the active one, half the time */
        return random % 2 ? IMAGE32_INITIAL(0x41880c) : 0x4000;
    case 0x21: return (random % 4) | ((random >> 4) % 0x210) << 16;
    case 0x30f: return random % 3;
    case 0x100: case 0x104: {
        static const uint32_t keys[] = {0x1c0001, 0x41000001, 0x01e0001, 0x40100001, 0x110001, 0x01c80001, 0x40c80001};
        return PICK(keys, random);
    }
    default:
        return random % 7 == 0 ? mixed(random >> 3) : coordinate(random) | coordinate(random >> 11) << 16;
    }
}
static uint32_t procedure_message(uint32_t kind, uint32_t random)
{
    static const uint32_t main[] = {1, 2, 3, 5, 5, 6, 0xf, 0x10, 0x15, 0x1c, 0x1c, 0x1c, 0x20, 0x21, 0x48, 0x4a, 0x7e, 0x85,
        0x100, 0x104, 0x111, 0x111, 0x112, 0x116, 0x200, 0x201, 0x205, 0x211, 0x212, 0x231, 0x232, 0x30f, 0x311, 0x46b,
        0x46e, 0x46f, 0x9};
    static const uint32_t child[] = {1, 2, 3, 5, 0xf, 0xf, 0xf, 0x14, 0x20, 0x22, 0x24, 0x84, 0xa0, 0xa1, 0xa4, 0xa9, 0x111,
        0x111, 0x111, 0x116, 0x200, 0x201, 0x202, 0x204, 0x206, 0x214, 0x216, 0x216, 0x231, 0x232, 0x232, 0x232, 0x466,
        0x466, 0x467, 0x467, 0x467, 0x46c, 0x46d, 0x9};
    static const uint32_t floating[] = {1, 3, 5, 6, 0x10, 0x14, 0x21, 0x84, 0x84, 0x84, 0x86, 0xa4, 0x231, 0x232, 0x311, 0x467,
        0x467, 0x46c, 0x46d, 0x9};
    static const uint32_t dialog[] = {2, 7, 8, 0xf, 0x466, 0x87, 0x100, 0x102, 0x200, 0x201, 0x204, 0x207, 0x202, 0x467,
        0x467, 0x9};
    switch (kind) {
    case ARG_MAIN_MESSAGE: return PICK(main, random);
    case ARG_CHILD_MESSAGE: return PICK(child, random);
    case ARG_FLOAT_MESSAGE: return PICK(floating, random);
    default: return PICK(dialog, random);
    }
}

static uint32_t extra_argument(uint32_t kind, uint32_t random)
{
    switch (kind) {
    case ARG_MAIN_MESSAGE: case ARG_CHILD_MESSAGE: case ARG_FLOAT_MESSAGE: case ARG_DIALOG_MESSAGE:
        s_procedure = kind;
        s_last_message = random % 20 == 0 ? mixed(random >> 5) : procedure_message(kind, random >> 5);
        return s_last_message;
    case ARG_PROC_MESSAGE:
        s_last_message = procedure_message(ARG_MAIN_MESSAGE, random);
        return s_last_message;
    case ARG_MOUSE_MESSAGE: {
        static const uint32_t messages[] = {0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0xa1, 0xa4, 0x200,
                                            0x201, 0x202, 0x203, 0x204, 0x205, 0x206, 0x207, 0x208, 0x209, 0x46a, 0x468, 0xa0};
        s_procedure = 0;
        s_last_message = PICK(messages, random);
        return s_last_message;
    }
    case ARG_WPARAM: return message_w(s_last_message, random);
    case ARG_LPARAM: return message_l(s_last_message, random);
    case ARG_COORD_LPARAM:
        return random % 6 == 0 ? mixed(random) : coordinate(random) | coordinate(random >> 13) << 16;
    case ARG_COMMAND: {
        static const uint32_t ids[] = {0x1103, 0x110a, 0x1601, 0x1602, 0x1603, 0x1604, 0x1605, 0x1608, 0x1609, 0x160a, 0x1661,
            0x1662, 0x1663, 0x1664, 0x1665, 0x1666, 0x1667, 0x1668, 0x1669, 0x166a, 0x166b, 0x1100};
        return ids[random % (sizeof ids / sizeof ids[0])];
    }
    case ARG_MODE: return random % 4 ? block_address(44, 0) : 0;
    case ARG_COPYDATA: return block_address(43, 0);
    case ARG_PIECE: shape_rect(s_fixture.blocks[45], random); return block_address(45, 0);
    case ARG_ENTRY: return block_address(42, 0x100 + (random % 4) * 4);
    case ARG_SHARED: return block_address(42, (random % 2) * 0x44);
    case ARG_SPOT: return block_address(34, 0x300 + (random % 4) * 0x14);
    case ARG_CONTEXT: return block_address(36, 0);
    case ARG_BITMAP: return block_address(random % 2 ? 6 : 14, 0);
    case ARG_MODELESS: return block_address(47, (random % 3) * 8);
    case ARG_KEY_TABLE: return block_address(37, 0x200 + (random % 3) * 0x10);
    case ARG_KEY_PRESS: return block_address(37, 0x80);
    case ARG_HELP_INFO: return block_address(45, 0x40);
    case ARG_PIXELS: return block_address(20 + random % 3, (random >> 2) % 0x80);
    default: return mixed(random);
    }
}

/* ---- the CPU speed's time-stamp counter -------------------------------------------------- */

/* the rdtsc reads, retail's and ours, made breakpoints answered by the scenario: the count
 * between them a speed (0..300 MHz) over the performance counter's microseconds */
static uint32_t s_tsc_at[4];
static uint32_t s_qpc_reads, s_qpc_start, s_qpc_ticks, s_qpc_frequency;
static uint64_t s_tsc_delta;
static void patch_tsc_reads(void)
{
    const uint8_t *ours = (const uint8_t *)(uintptr_t)fs98_host_rt_cpu_speed;
    uint32_t at, found = 0;
    DWORD old;
    s_tsc_at[0] = BASE + 0x1ffb;
    s_tsc_at[1] = BASE + 0x2007;
    for (at = 0; at < 2; ++at)
        if (((uint8_t *)(uintptr_t)s_tsc_at[at])[0] != 0x0f || ((uint8_t *)(uintptr_t)s_tsc_at[at])[1] != 0x31) GIVE_UP();
    for (at = 0; at < 0x400 && found < 3; ++at)
        if (ours[at] == 0x0f && ours[at + 1] == 0x31) {
            if (found >= 2) GIVE_UP();
            s_tsc_at[2 + found++] = (uint32_t)(uintptr_t)(ours + at);
        }
    if (found != 2) GIVE_UP();
    for (at = 0; at < 4; ++at) {
        uint8_t *code = (uint8_t *)(uintptr_t)s_tsc_at[at];
        if (!VirtualProtect(code, 2, PAGE_EXECUTE_READWRITE, &old)) GIVE_UP();
        code[0] = 0xcc;
        code[1] = 0x90;
    }
}
static int is_tsc_read(uint32_t address)
{
    return address == s_tsc_at[0] || address == s_tsc_at[1] || address == s_tsc_at[2] || address == s_tsc_at[3];
}
static uint64_t tsc_answer(void)
{
    uint64_t start = 0x123400000000ull + s_seed;
    return s_tsc_reads++ % 2 ? start + s_tsc_delta : start;
}
/* the performance counter's frequency, start and ticks, and the speed they make */
static void choose_timing(void)
{
    static const uint32_t frequencies[] = {1193182, 3579545, 1000000, 10000000};
    static const uint32_t speeds[] = {0, 50, 79, 80, 100, 109, 110, 150, 189, 190, 300};
    uint32_t value = script(), microseconds;
    s_qpc_frequency = frequencies[value % 4];
    s_qpc_ticks = 1000 + (value >> 2) % 100000;
    s_qpc_start = value ^ 0x5a5a;
    microseconds = (uint32_t)MulDiv((int)s_qpc_ticks, 1000000, (int)s_qpc_frequency);
    s_tsc_delta = (uint64_t)microseconds * speeds[(value >> 20) % 11] + (value >> 8) % (microseconds + 1);
    s_qpc_reads = 0;
}

/* ---- the imports ------------------------------------------------------------------------- */

static void write_rect(uint32_t address, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    int32_t rect[4] = {left, top, right, bottom};
    if (writable(address, 16)) memcpy((void *)(uintptr_t)address, rect, 16);
}
static int window_semantics(uint32_t index, uint32_t ecx, uint32_t *arguments, uint64_t *result)
{
    uint32_t value = script();
    (void)ecx;
    switch (index) {
    case HOST_RT_IMPORT_PeekMessageA: {   /* (msg, window, first, last, remove): a message, WM_QUIT in time */
        static const uint32_t messages[] = {0x12, 0x104, 0x104, 0x100, 0x200, 0x111, 0x113, 0xf};
        uint32_t message[7] = {0x4000 + value % 4, messages[(value >> 2) % 8], value % 3 ? 0xd : 0x41,
                               (value >> 5) % 2 ? 0x40000000u : 0x1c0001u, value, 0, 0};
        if (s_calls > 300) message[1] = 0x12;
        else if (value % 4 == 0) { *result = 0; return 1; }
        if (!writable(arguments[0], 0x1c)) return 0;
        memcpy((void *)(uintptr_t)arguments[0], message, 0x1c);
        put(" now %x %x %x", message[1], message[2], message[3]);
        *result = 1;
        return 1;
    }
    case HOST_RT_IMPORT_GlobalMemoryStatus: {
        /* mostly enough: 64MB; now and then too little, or short of 32MB */
        static const uint32_t totals[] = {0x4000000, 0x4000000, 0x4000000, 0x2000000, 0x1800000, 0x1000000, 0x300000, 0x400000};
        static const uint32_t free[] = {0x1000000, 0x800000, 0x1c00000, 0x10000, 0x1bfffff, 0x400000};
        uint32_t status[8] = {0x20, 50, totals[value % 8], free[(value >> 4) % 6], totals[(value >> 8) % 8],
                              free[(value >> 12) % 6], 0x7ffe0000, 0x7000000};
        if (!writable(arguments[0], 0x20)) return 0;
        memcpy((void *)(uintptr_t)arguments[0], status, 0x20);
        *result = 0;
        return 1;
    }
    case HOST_RT_IMPORT_GetSystemInfo: {   /* an x86 386, 486 or Pentium, now and then another */
        static const uint32_t types[] = {0x24a, 0x24a, 0x1e6, 0x182};
        uint32_t info[9] = {value % 9 == 0 ? 1 : 0, 0x1000, 0x10000, 0x7ffeffff, 1, 1, types[(value >> 4) % 4], 0x10000, 0x50002};
        if (!writable(arguments[0], 0x24)) return 0;
        memcpy((void *)(uintptr_t)arguments[0], info, 0x24);
        choose_timing();
        *result = 0;
        return 1;
    }
    case HOST_RT_IMPORT_QueryPerformanceFrequency:
        if (!writable(arguments[0], 8)) return 0;
        { uint32_t words[2] = {s_qpc_frequency, 0}; memcpy((void *)(uintptr_t)arguments[0], words, 8); }
        *result = 1;
        return 1;
    case HOST_RT_IMPORT_QueryPerformanceCounter:
        if (!writable(arguments[0], 8)) return 0;
        { uint32_t words[2] = {s_qpc_start + (s_qpc_reads++ ? s_qpc_ticks : 0), 0}; memcpy((void *)(uintptr_t)arguments[0], words, 8); }
        *result = 1;
        return 1;
    case HOST_RT_IMPORT_MulDiv:
        *result = (uint32_t)MulDiv((int)arguments[0], (int)arguments[1], (int)arguments[2]);
        return 1;
    case HOST_RT_IMPORT_GetWindowPlacement: {
        uint32_t placement[11] = {0x2c, value % 3 == 0 ? 2 : 0, 1 + (value >> 2) % 3, 0, 0, 0xffffffffu, 0xffffffffu,
                                  (value >> 4) % 40, (value >> 8) % 40, 100 + (value >> 12) % 200, 100 + (value >> 16) % 200};
        if (!writable(arguments[1], 0x2c)) return 0;
        memcpy((void *)(uintptr_t)arguments[1], placement, 0x2c);
        *result = value % 7 != 0;
        return 1;
    }
    case HOST_RT_IMPORT_GetWindowRect: case HOST_RT_IMPORT_GetClientRect: {
        /* sizes mostly from the few the fixtures use too, so that they meet */
        static const int32_t sizes[] = {0, 16, 32, 48, 64, 100, 100, -1};
        int32_t left = index == HOST_RT_IMPORT_GetClientRect || value % 3 == 0 ? 0 : small_signed(value, 200, -20);
        int32_t top = index == HOST_RT_IMPORT_GetClientRect || value % 3 == 0 ? 0 : small_signed(value >> 8, 200, -20);
        int32_t width = value % 4 ? sizes[(value >> 16) % 8] : small_signed(value >> 16, 66, -2);
        int32_t height = value % 5 ? sizes[(value >> 23) % 8] : small_signed(value >> 23, 66, -2);
        if (!strcmp(CURRENT.name, "window_mouse") && value % 4) left = top = 0, width = height = 100;   /* the edges the points are near */
        if (index == HOST_RT_IMPORT_GetClientRect && !strcmp(CURRENT.name, "child_procedure") && s_last_window_data &&
            value % 3 == 0) {   /* the window's picture size, the height now and then not */
            memcpy(&width, (uint8_t *)(uintptr_t)s_last_window_data + 0x60, 4);
            memcpy(&height, (uint8_t *)(uintptr_t)s_last_window_data + 0x64, 4);
            if ((value >> 3) % 2) height += 16;
        }
        write_rect(arguments[1], left, top, left + width, top + height);
        *result = value % 9 != 0;
        return 1;
    }
    case HOST_RT_IMPORT_SystemParametersInfoA:
        if (arguments[0] != 0x30) return 0;
        write_rect(arguments[2], (int32_t)(value % 3) * 10, (int32_t)((value >> 2) % 3) * 10, 640, 480);
        *result = 1;
        return 1;
    case HOST_RT_IMPORT_timeGetDevCaps: {
        static const uint32_t lows[] = {1, 10, 50, 60};
        static const uint32_t highs[] = {50, 1000, 40, 1000000};
        uint32_t caps[2] = {lows[value % 4], highs[(value >> 2) % 4]};
        if (!writable(arguments[0], 8)) return 0;
        memcpy((void *)(uintptr_t)arguments[0], caps, 8);
        *result = value % 6 == 0;
        return 1;
    }
    case HOST_RT_IMPORT_RegOpenKeyExA:
        if (value % 3 == 0) { *result = 2; return 1; }
        if (!writable(arguments[4], 4)) return 0;
        { uint32_t key = 0x9000 + value % 4; memcpy((void *)(uintptr_t)arguments[4], &key, 4); }
        *result = 0;
        return 1;
    case HOST_RT_IMPORT_RegQueryValueExA: {   /* (key, name, reserved, type, data, size) */
        const char *text = value % 2 ? "C:\\FS98" : "http://x";
        uint32_t size = 0;
        if (writable(arguments[5], 4)) memcpy(&size, (void *)(uintptr_t)arguments[5], 4);
        if (value % 3 == 0 || !arguments[4] || size < 16 || !writable(arguments[4], 16)) { *result = 2; return 1; }
        strcpy((char *)(uintptr_t)arguments[4], text);
        *result = 0;
        return 1;
    }
    case HOST_RT_IMPORT_GetObjectA: {   /* (object, size, out): a BITMAP, its bits in block 46 or none */
        uint32_t bitmap[6] = {0, 8 + value % 40, 8 + (value >> 6) % 40, 0, 0x80001, (value >> 12) % 3 ? block_address(46, 0) : 0};
        bitmap[3] = (bitmap[1] + 3) & ~3u;
        if (arguments[1] < 0x18 || !writable(arguments[2], 0x18)) return 0;
        memcpy((void *)(uintptr_t)arguments[2], bitmap, 0x18);
        *result = 0x18;
        return 1;
    }
    case HOST_RT_IMPORT_GetRgnBox:
        if (value % 2) write_rect(arguments[1], 0, 0, 16 * (int32_t)((value >> 4) % 5), 16 * (int32_t)((value >> 12) % 5));
        else write_rect(arguments[1], (int32_t)(value >> 4) % 3, 0, 20, 20);
        *result = (value >> 2) % 4 ? 2 : (value >> 4) % 2;
        return 1;
    case HOST_RT_IMPORT_SendMessageA:
        if (arguments[1] == 0x40a && writable(arguments[3], 0x34)) {   /* TTM_HITTEST: a tool's words */
            uint32_t at;
            for (at = 0xc; at < 0x34; at += 4) { uint32_t word = mixed(value + at); memcpy((void *)(uintptr_t)(arguments[3] + at), &word, 4); }
            *result = value % 3 != 0;
            return 1;
        }
        return 0;
    case HOST_RT_IMPORT_OffsetRect:
        if (!writable(arguments[0], 16)) return 0;
        *result = (uint32_t)OffsetRect((RECT *)(uintptr_t)arguments[0], (int)arguments[1], (int)arguments[2]);
        return 1;
    case HOST_RT_IMPORT_SetRect:
        write_rect(arguments[0], (int32_t)arguments[1], (int32_t)arguments[2], (int32_t)arguments[3], (int32_t)arguments[4]);
        *result = 1;
        return 1;
    case HOST_RT_IMPORT_CreateDIBSection: {   /* (dc, info, usage, bits out, section, offset) */
        uint8_t *bits = heap_allocate(0x2000);
        uint32_t pointer = value % 5 && bits ? fixture_address(bits) : 0;
        if (writable(arguments[3], 4)) memcpy((void *)(uintptr_t)arguments[3], &pointer, 4);
        *result = pointer ? 0x6200 + value % 4 : 0;
        return 1;
    }
    case HOST_RT_IMPORT_CreateThread:
        if (writable(arguments[5], 4)) { uint32_t id = 0x300 + value % 8; memcpy((void *)(uintptr_t)arguments[5], &id, 4); }
        *result = value % 5 ? 0x700 : 0;
        return 1;
    case HOST_RT_IMPORT_GetExitCodeThread:
        if (value % 4 == 0) { *result = 0; return 1; }
        if (!writable(arguments[1], 4)) return 0;
        { uint32_t code = (value >> 2) % 3 - 1; memcpy((void *)(uintptr_t)arguments[1], &code, 4); }
        *result = 1;
        return 1;
    case HOST_RT_IMPORT_GetDeviceCaps: {   /* a display of 640..1024 wide, mostly 8-bit or more, palette or not */
        static const uint32_t widths[] = {640, 800, 1024, 320, 641}, heights[] = {480, 600, 768, 200, 481}, bits[] = {8, 16, 8, 4, 32, 1};
        switch (arguments[1]) {
        case 8: *result = widths[value % 5]; return 1;
        case 10: *result = heights[value % 5]; return 1;
        case 12: *result = bits[value % 6]; return 1;
        case 14: *result = value % 7 ? 1 : value % 3; return 1;
        case 0x26: *result = value % 2 ? 0x7e99 : 0x7f99 & ~0x100u; return 1;
        default: return 0;
        }
    }
    case HOST_RT_IMPORT_GetFileAttributesA:
        *result = value % 3 ? 0x20 : 0xffffffffu;
        return 1;
    case HOST_RT_IMPORT_DefWindowProcA:   /* the hit test's answer: the client, the caption or an edge */
        if (arguments[1] != 0x84) return 0;
        {
            static const uint32_t hits[] = {1, 1, 1, 2, 0, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12};
            *result = !strcmp(CURRENT.name, "window_mouse") && value % 2 ? 1 : PICK(hits, value);
        }
        return 1;
    case HOST_RT_IMPORT_GetWindowLongA:  /* styles: visible or not, the client edge; GWL_USERDATA as before */
        if (arguments[1] == (uint32_t)-16 || arguments[1] == (uint32_t)-20) {
            static const uint32_t styles[] = {0, 0x200, 0x80, 0x10000000, 0x10040000, 0x14cf0000, 0x40000, 0x280};
            *result = PICK(styles, value);
            return 1;
        }
        return 0;
    case HOST_RT_IMPORT_GetPrivateProfileIntA:   /* the default, or a value near the bounds the code tests */
        { static const uint32_t values[] = {0, 1, 2, 3, 8, 16, 7, 0x2b10, 0x2b11, 0x2b12, 0x5622, 0xffffffffu};
          *result = value % 3 == 0 ? arguments[2] : PICK(values, value >> 2); }
        return 1;
    case HOST_RT_IMPORT_MessageBoxA:
        { static const uint32_t answers[] = {1, 2, 6, 7, 7, 0}; *result = PICK(answers, value); }
        return 1;
    case HOST_RT_IMPORT_GetSystemMetrics:
        if (arguments[0] > 1) return 0;
        { static const uint32_t sizes[] = {640, 800, 1024, 639, 641, 480}; *result = PICK(sizes, value); }
        return 1;
    case HOST_RT_IMPORT_FindWindowA:   /* another copy running, now and then */
        *result = value % 4 == 0 ? 0x4800 : 0;
        return 1;
    case HOST_RT_IMPORT_MFC_1168: {    /* AfxGetModuleState: block 26, the instance at +8 */
        uint32_t instance = 0x400000;
        memcpy(s_fixture.blocks[26] + 8, &instance, 4);
        *result = block_address(26, 0);
        return 1;
    }
    case HOST_RT_IMPORT_RegisterClassA:  /* an atom; now and then none, or a word of 0 */
        *result = value % 12 ? 0xc000 + value % 0x100 : value % 24 ? 0 : 0x10000;
        return 1;
    case HOST_RT_IMPORT_GetForegroundWindow: case HOST_RT_IMPORT_GetLastActivePopup: case HOST_RT_IMPORT_GetActiveWindow:
        /* the main window half the time */
        *result = value % 2 ? IMAGE32(0x41880c) : 0x4000 + value % 8;
        return 1;
    case HOST_RT_IMPORT_GetParent:     /* the main window, the window's frame, or another */
        *result = value % 3 == 0 ? IMAGE32(0x41880c) : value % 3 == 1 ? 0x4100 + (value >> 2) % 8 : 0x4400;
        return 1;
    case HOST_RT_IMPORT__splitpath: {
        char drive[8], directory[0x100], name[0x100], extension[0x100];
        const char *path = (const char *)(uintptr_t)arguments[0];
        if (!readable_text(arguments[0]) || strlen(path) > 0xf0) return 0;
        _splitpath(path, drive, directory, name, extension);
        if (arguments[1] && writable(arguments[1], 3)) strcpy((char *)(uintptr_t)arguments[1], drive);
        if (arguments[2] && writable(arguments[2], (uint32_t)strlen(directory) + 1)) strcpy((char *)(uintptr_t)arguments[2], directory);
        if (arguments[3] && writable(arguments[3], (uint32_t)strlen(name) + 1)) strcpy((char *)(uintptr_t)arguments[3], name);
        if (arguments[4] && writable(arguments[4], (uint32_t)strlen(extension) + 1)) strcpy((char *)(uintptr_t)arguments[4], extension);
        *result = 0;
        return 1;
    }
    case HOST_RT_IMPORT__makepath: {
        char path[0x400];
        const char *parts[4];
        uint32_t at;
        for (at = 0; at < 4; ++at) {
            if (arguments[1 + at] && !readable_text(arguments[1 + at])) return 0;
            parts[at] = arguments[1 + at] ? (const char *)(uintptr_t)arguments[1 + at] : 0;
            if (parts[at] && strlen(parts[at]) > 0xf0) return 0;
        }
        _makepath(path, parts[0], parts[1], parts[2], parts[3]);
        if (!writable(arguments[0], (uint32_t)strlen(path) + 1) || strlen(path) >= 0x104) return 0;
        strcpy((char *)(uintptr_t)arguments[0], path);
        put(" now \"%.80s\"", path);
        *result = 0;
        return 1;
    }
    case HOST_RT_IMPORT_sscanf: {
        int numbers[4] = {0, 0, 0, 0}, count, at;
        if (!readable_text(arguments[0]) || !readable_text(arguments[1]) ||
            strcmp((const char *)(uintptr_t)arguments[1], "%d,%d,%d,%d")) return 0;
        count = sscanf((const char *)(uintptr_t)arguments[0], "%d,%d,%d,%d", &numbers[0], &numbers[1], &numbers[2], &numbers[3]);
        for (at = 0; at < count && at < 4; ++at)
            if (writable(arguments[2 + at], 4)) memcpy((void *)(uintptr_t)arguments[2 + at], &numbers[at], 4);
        *result = (uint32_t)count;
        return 1;
    }
    case HOST_RT_IMPORT_sprintf: {
        char out[0x400];
        int length;
        if (!readable_text(arguments[1]) || !readable_text(arguments[2])) return 0;
        length = snprintf(out, sizeof out, (const char *)(uintptr_t)arguments[1], (const char *)(uintptr_t)arguments[2],
                          arguments[3], arguments[4]);
        if (length < 0 || !writable(arguments[0], (uint32_t)length + 1)) return 0;
        memcpy((void *)(uintptr_t)arguments[0], out, (size_t)length + 1);
        put(" now \"%.80s\"", out);
        *result = (uint32_t)length;
        return 1;
    }
    default:
        return 0;
    }
}

/* ---- the services ------------------------------------------------------------------------ */

static int window_service_semantics(uint32_t offset, uint32_t *arguments, uint64_t *result)
{
    uint32_t value = script();
    switch (offset) {
    case 0x98: {    /* (name, buffer, size): the command line's /name:value */
        static const char *const values[] = {"C:\\FLIGHT\\MINE.CFG", "", "abc", "D:test", "\\X\\A.CFG", "x\\b"};
        const char *text = values[(value >> 2) % 6];
        if (value % 3 == 0) { *result = 0xffffffffu; return 1; }
        if (arguments[2] <= strlen(text) || !writable(arguments[1], (uint32_t)strlen(text) + 1)) return 0;
        strcpy((char *)(uintptr_t)arguments[1], text);
        put(" now \"%s\"", text);
        *result = 0;
        return 1;
    }
    case 0x9c:      /* (name, out): its number */
        if (value % 2) { *result = 0xffffffffu; return 1; }
        if (!writable(arguments[1], 4)) return 0;
        { uint32_t number = (value >> 1) % 2; memcpy((void *)(uintptr_t)arguments[1], &number, 4); }
        *result = 0;
        return 1;
    case 0x120: {   /* (block, size, flat): the lobby's connection, or none */
        uint8_t *block = value % 3 ? 0 : heap_allocate(0x40);
        uint32_t pointer = fixture_address(block), size = block ? 0x40 : 0;
        if (!writable(arguments[0], 4) || !writable(arguments[1], 4)) return 0;
        if (block) memset(block, 0x5c, 0x40);
        memcpy((void *)(uintptr_t)arguments[0], &pointer, 4);
        memcpy((void *)(uintptr_t)arguments[1], &size, 4);
        *result = 0;
        return 1;
    }
    case 0x21c:     /* memmove(to, from, count) */
        if (arguments[2] > 0x10000 || !writable(arguments[0], arguments[2]) || !readable(arguments[1], arguments[2])) return 0;
        memmove((void *)(uintptr_t)arguments[0], (void *)(uintptr_t)arguments[1], arguments[2]);
        *result = 0;
        return 1;
    case 0x2a8: case 0x2b0: {   /* strcpy, strcat */
        size_t have;
        if (!readable_text(arguments[1]) || (offset == 0x2b0 && !readable_text(arguments[0]))) return 0;
        have = offset == 0x2b0 ? strlen((char *)(uintptr_t)arguments[0]) : 0;
        if (!writable(arguments[0], (uint32_t)(have + strlen((char *)(uintptr_t)arguments[1]) + 1))) return 0;
        memmove((char *)(uintptr_t)arguments[0] + have, (char *)(uintptr_t)arguments[1], strlen((char *)(uintptr_t)arguments[1]) + 1);
        put(" now \"%.80s\"", (char *)(uintptr_t)arguments[0]);
        *result = arguments[0];
        return 1;
    }
    case 0x218:     /* memset(to, value, count) */
        if (arguments[2] > 0x10000 || !writable(arguments[0], arguments[2])) return 0;
        memset((void *)(uintptr_t)arguments[0], (int)arguments[1], arguments[2]);
        *result = 0;
        return 1;
    case 0x228: {   /* memcmp: -1, 0, 1 */
        int compared;
        if (arguments[2] > 0x10000 || !readable(arguments[0], arguments[2]) || !readable(arguments[1], arguments[2])) return 0;
        compared = memcmp((void *)(uintptr_t)arguments[0], (void *)(uintptr_t)arguments[1], arguments[2]);
        *result = compared < 0 ? 0xffffffffu : compared > 0;
        return 1;
    }
    case 0x30c: {   /* (list, context, fn): fn(element, context) from the last back, until one is not 0 */
        uint32_t *header = (uint32_t *)(uintptr_t)arguments[0], at, back = 0;
        if (!list_sane(arguments[0])) return 0;
        ++header[4];
        put("\n");
        for (at = 0; at < header[0] && at < 64; ++at) {
            back = (uint32_t)call_back2(arguments[2], header[6] + (header[0] - at - 1) * header[3], arguments[1]);
            put("\n");
            if (back) break;
        }
        --header[4];
        *result = back;
        return 1;
    }
    case 0x300: {   /* (list, element): removed */
        uint32_t *header = (uint32_t *)(uintptr_t)arguments[0], at;
        if (!list_sane(arguments[0]) || !header[3] || arguments[1] < header[6] || (arguments[1] - header[6]) % header[3]) return 0;
        at = (arguments[1] - header[6]) / header[3];
        if (at >= header[0]) return 0;
        memmove((uint8_t *)(uintptr_t)header[6] + at * header[3], (uint8_t *)(uintptr_t)header[6] + (at + 1) * header[3],
                (header[0] - at - 1) * header[3]);
        --header[0];
        *result = 0;
        return 1;
    }
    case 0x208: {   /* (key, scan, modifiers, extended): what they hold, logged; not modelled */
        uint32_t at;
        for (at = 0; at < 4; ++at) log_words_described(arguments[at], 1);
        return 0;
    }
    case 0x304:     /* (pointer): the word cleared */
        if (!writable(arguments[0], 4)) return 0;
        memset((void *)(uintptr_t)arguments[0], 0, 4);
        *result = arguments[0];
        return 1;
    default:
        return 0;
    }
}

/* ---- what the calls are given ------------------------------------------------------------ */

/* a word as retail's: our routine's address read as retail's */
static uint32_t retail_word(uint32_t value)
{
    uint32_t op;
    if (value < s_our_code || value >= s_our_code_end) return value;
    for (op = 0; op < OP_COUNT; ++op)
        if ((uint32_t)(uintptr_t)s_operations[op].ours == value) return BASE + s_operations[op].rva;
    return value;
}
static void log_words_described(uint32_t address, uint32_t count)
{
    uint32_t at;
    if (!address) { put(" {null}"); return; }
    if (!readable(address, count * 4)) { put(" {?}"); return; }
    put(" {");
    for (at = 0; at < count; ++at) describe(((uint32_t *)(uintptr_t)address)[at]);
    put(" }");
}
static void log_maybe_text(uint32_t address)
{
    if (address) log_text(address);
    else put(" null");
}
static void window_log_contents(uint32_t index, const uint32_t *arguments)
{
    switch (index) {
    case HOST_RT_IMPORT_RegisterClassA: log_words_described(arguments[0], 10); log_text(((uint32_t *)(uintptr_t)arguments[0])[9]); break;
    case HOST_RT_IMPORT_CreateWindowExA: log_text(arguments[1]); log_text(arguments[2]); break;
    case HOST_RT_IMPORT__splitpath: case HOST_RT_IMPORT_SetCurrentDirectoryA: case HOST_RT_IMPORT_DeleteFileA:
    case HOST_RT_IMPORT_GetFileAttributesA:
        log_text(arguments[0]); break;
    case HOST_RT_IMPORT__makepath:
        log_maybe_text(arguments[1]); log_maybe_text(arguments[2]); log_maybe_text(arguments[3]); log_maybe_text(arguments[4]); break;
    case HOST_RT_IMPORT_WritePrivateProfileStringA:
        log_text(arguments[0]); log_text(arguments[1]); log_text(arguments[2]); log_text(arguments[3]); break;
    case HOST_RT_IMPORT_sscanf: case HOST_RT_IMPORT_FindWindowA: log_text(arguments[0]); log_maybe_text(arguments[1]); break;
    case HOST_RT_IMPORT_sprintf: log_text(arguments[1]); log_text(arguments[2]); break;
    case HOST_RT_IMPORT_MessageBoxA: log_text(arguments[1]); log_maybe_text(arguments[2]); break;
    case HOST_RT_IMPORT_MessageBoxIndirectA: log_words_described(arguments[0], 10); break;
    case HOST_RT_IMPORT_RegOpenKeyExA: case HOST_RT_IMPORT_RegQueryValueExA: case HOST_RT_IMPORT_SetWindowTextA:
    case HOST_RT_IMPORT_WinHelpA:
        log_text(arguments[1]); break;
    case HOST_RT_IMPORT_MCIWndCreateA: log_text(arguments[3]); break;
    case HOST_RT_IMPORT_ShellExecuteA: log_text(arguments[2]); break;
    case HOST_RT_IMPORT_SetWindowPlacement: log_words_described(arguments[1], 11); break;
    case HOST_RT_IMPORT_CreateDIBSection:       /* the header (not its pels per metre, never set) and three colours */
        if (readable(arguments[1], 0x34)) {
            const uint32_t *info = (const uint32_t *)(uintptr_t)arguments[1];
            put(" {%x %x %x %x %x %x %x %x %x %x %x}", info[0], info[1], info[2], info[3], info[4], info[5], info[8], info[9],
                info[10], info[11], info[12]);
        }
        break;
    case HOST_RT_IMPORT_SendMessageA:
        if (arguments[1] == 0x4a) {
            log_words_described(arguments[3], 3);
            if (readable(arguments[3], 12)) log_words_described(((uint32_t *)(uintptr_t)arguments[3])[2], 4);
        } else if (arguments[1] == 0x407) log_words_described(arguments[3], 7);
        else if (arguments[1] == 0x40a) log_words_described(arguments[3], 4);
        {   /* a message sent runs a window procedure at once: what of .data it would see */
            uint32_t sum = 0, at;
            for (at = 0; at < DATA_SIZE; at += 4) sum = sum * 31 + retail_word(*(uint32_t *)(uintptr_t)(BASE + DATA_RVA + at));
            put(" data %x", sum);
        }
        break;
    case HOST_RT_IMPORT_SetDIBColorTable: log_words_described(arguments[3], 4); break;
    case HOST_RT_IMPORT_CreateThread:     /* the display mode it is given: the fields set */
        if (arguments[3] && readable(arguments[3], 0x74)) {
            const uint8_t *mode = (const uint8_t *)(uintptr_t)arguments[3];
            uint32_t fields[4];
            memcpy(&fields[0], mode + 0x28, 4); memcpy(&fields[1], mode + 0x68, 4);
            memcpy(&fields[2], mode + 0x6c, 4); memcpy(&fields[3], mode + 0x70, 4);
            put(" {%x %x %x %x %x}", *(const uint16_t *)(mode + 0x24), fields[0], fields[1], fields[2], fields[3]);
        }
        break;
    case HOST_RT_IMPORT_CreateRectRgnIndirect: log_words_described(arguments[0], 4); break;
    case HOST_RT_IMPORT_FrameRect: case HOST_RT_IMPORT_CreatePolygonRgn: case HOST_RT_IMPORT_EqualRect:
    case HOST_RT_IMPORT_AdjustWindowRectEx: case HOST_RT_IMPORT_AdjustWindowRect:
        log_words_described(arguments[index == HOST_RT_IMPORT_FrameRect ? 1 : 0], 4); break;
    case HOST_RT_IMPORT_IntersectRect: log_words_described(arguments[1], 4); log_words_described(arguments[2], 4); break;
    case HOST_RT_IMPORT_RectVisible: case HOST_RT_IMPORT_IsRectEmpty:
        log_words_described(arguments[index == HOST_RT_IMPORT_RectVisible ? 1 : 0], 4); break;
    case HOST_RT_IMPORT_StretchDIBits: log_words_described(arguments[10], 6); break;
    case HOST_RT_IMPORT_ClientToScreen: case HOST_RT_IMPORT_ScreenToClient: log_words_described(arguments[1], 2); break;
    case HOST_RT_IMPORT_ChangeDisplaySettingsA:
        if (arguments[0] && readable(arguments[0] + 0x24, 0x50)) {
            const uint8_t *mode = (const uint8_t *)(uintptr_t)arguments[0];
            uint32_t fields[4];
            memcpy(&fields[0], mode + 0x28, 4); memcpy(&fields[1], mode + 0x68, 4);
            memcpy(&fields[2], mode + 0x6c, 4); memcpy(&fields[3], mode + 0x70, 4);
            put(" {%x %x %x %x}", fields[0], fields[1], fields[2], fields[3]);
        }
        break;
    case HOST_RT_IMPORT_TranslateMessage: case HOST_RT_IMPORT_DispatchMessageA: case HOST_RT_IMPORT_IsDialogMessageA:
        log_words_described(arguments[index == HOST_RT_IMPORT_IsDialogMessageA ? 1 : 0], 4); break;
    default: break;
    }
}

/* a callback's arguments' contents, and what a window handler answers through its fourth */
static void callback_contents(uint32_t index, uint32_t *arguments)
{
    if (index == CALLBACK4_INDEX && arguments[3] && writable(arguments[3], 16)) {
        uint32_t keep = script() % 2;
        log_words_described(arguments[3], arguments[0] == 5 ? 1 : 4);   /* kind 5's is its answer, else a rectangle */
        if (arguments[0] == 5 && arguments[2]) log_words_described(arguments[2] + 0x1c, 1);   /* the picture's surface */
        memcpy((void *)(uintptr_t)arguments[3], &keep, 4);
    } else if (index == CALLBACK_INDEX && in_stack(arguments[0]))
        log_words_described(arguments[0], 2);
}
