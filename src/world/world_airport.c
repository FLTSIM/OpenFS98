/* Go-to-airport list state, owner dispatch and control initialization. */
#include "world_airport.h"
#include "world_init.h"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot, type) ((type)s_services->resolve(slot))
static const struct fs98_world_airport_services *s_services;
void fs98_world_airport_set_services(const struct fs98_world_airport_services *services) { s_services = services; }
static uint32_t send(void *object, uint32_t offset, uint32_t message, uint32_t first, uint32_t second)
{ return API(0x258b0, uint32_t (SC *)(uint32_t, uint32_t, uint32_t, uint32_t))(U32(object, offset + 0x20), message, first, second); }
static void callback(void *object, uint32_t slot)
{ void *descriptor = PTR(object, 0x10dc); ((void (SC *)(void *))PTR(descriptor, slot))(descriptor); }
static void call1(uint32_t slot, void *object, uint32_t value) { API(slot, void (TC *)(void *, uint32_t))(object, value); }
static void edit_text(void *object, const char *text)
{ API(0x255bc, void (TC *)(void *, const char *))(AT(object, 0x1b40), text); }
static void text_copy(char *out, const char *text) { do { *out++ = *text; } while (*text++); }
static const uint32_t s_places[] = {0x142c, 0x152c, 0x162c, 0x172c};
static const uint32_t s_labels[] = {0x1374, 0x1474, 0x1574, 0x1674};
/* 0x224067c0: nine HOST strings plus a twice-maximum-length work buffer. */
void fs98_world_airport_allocate(void *object)
{
    uint32_t index, length;
    U32(object, 0x23d4) = 0;
    for (index = 0; index < 9; ++index) {
        length = ((uint32_t (SC *)(uint32_t, void *))PTR(s_services->host(), 0x2a4))(0x8001 + index, AT(object, 0x23ac + index * 4));
        if (length > U32(object, 0x23d4)) U32(object, 0x23d4) = length;
    }
    ((void (SC *)(uint32_t, uint32_t, void *, uint32_t))PTR(s_services->host(), 0x20c))(5, U32(object, 0x23d4) * 2, AT(object, 0x23d0), 0);
}
/* 0x22406950 */
void fs98_world_airport_release(void *object)
{
    uint32_t index;
    for (index = 0; index < 10; ++index)
        ((void (SC *)(void *))PTR(s_services->host(), 0x214))(AT(object, 0x23ac + index * 4));
}
static void placeholder(void *object, uint32_t index, uint32_t count)
{
    char *out = PTR(object, 0x23d0);
    if (count == 1)
        ((void (SC *)(char *, const char *))PTR(s_services->host(), 0x2a8))(out, PTR(object, 0x23b0 + index * 8));
    else API(0x256b8, int (*)(char *, const char *, ...))(out, PTR(object, 0x23b4 + index * 8), count);
}
static void fill(void *object, uint32_t list, uint32_t control, uint32_t snapshot, int combo)
{
    void *descriptor = PTR(object, 0x10dc);
    int32_t count = (int32_t)U32(descriptor, list);
    uint32_t index;
    U32(object, snapshot) = U32(descriptor, list + 8);
    for (index = 0; count > 0; ++index, --count)
        send(object, control, combo ? 0x143 : 0x180, 0, U32(PTR(PTR(object, 0x10dc), list + 4), index * 12));
    send(object, control, combo ? 0x14e : 0x186, U32(object, snapshot), 0);
}
/* 0x22406720 */
void fs98_world_airport_runways(void *object)
{
    if (U32(PTR(object, 0x10dc), 0x28) == 0x1002) return;
    callback(object, 0x24);
    send(object, 0x1f54, 0x14b, 0, 0);
    fill(object, 0xa0, 0x1f54, 0x23a8, 1);
}
/* 0x22405690 */
void fs98_world_airport_populate(void *object)
{
    uint32_t mode, index;
    callback(object, 0x20);
    mode = U32(PTR(object, 0x10dc), 0x28);
    if (mode == 0x1000) {
        edit_text(object, PTR(PTR(object, 0x10dc), 0x34));
        for (index = 0; index < 4; ++index) {
            void *descriptor = PTR(object, 0x10dc);
            uint32_t count = U32(descriptor, 0x44 + index * 12);
            U32(object, 0x238c + index * 4) = U32(descriptor, 0x4c + index * 12);
            placeholder(object, index, count);
            send(object, s_places[index], 0x143, 0, U32(object, 0x23d0));
            /* Keep the selection captured before formatting the placeholder. */
            {
                uint32_t row;
                for (row = 0; (int32_t)count > 0; ++row, --count)
                    send(object, s_places[index], 0x143, 0, U32(PTR(PTR(object, 0x10dc), 0x48 + index * 12), row * 12));
            }
            send(object, s_places[index], 0x14e, U32(object, 0x238c + index * 4), 0);
        }
    } else if (mode == 0x1001) fill(object, 0x94, 0x1a40, 0x239c, 0);
    else if (mode == 0x1002) fill(object, 0xac, 0x1d9c, 0x23a4, 0);
    fill(object, 0x74, 0x1c60, 0x23a0, 0);
    fs98_world_airport_runways(object);
}
/* 0x22405d10: clear according to the old mode. */
void fs98_world_airport_clear(void *object)
{
    uint32_t mode = U32(PTR(object, 0x10dc), 0x28), index;
    if (mode == 0x1000) for (index = 0; index < 4; ++index) send(object, s_places[index], 0x14b, 0, 0);
    else if (mode == 0x1001) send(object, 0x1a40, 0x184, 0, 0);
    else if (mode == 0x1002) send(object, 0x1d9c, 0x184, 0, 0);
    send(object, 0x1c60, 0x184, 0, 0); send(object, 0x1f54, 0x14b, 0, 0);
    edit_text(object, PTR(PTR(object, 0x10dc), 0x34));
}
static void enable(void *object, uint32_t offset, int value) { call1(0x255c8, AT(object, offset), (uint32_t)value); }
static void show(void *object, uint32_t offset, int value) { call1(0x25500, AT(object, offset), (uint32_t)value); }
/* 0x22406a10 / 0x22406bb0 / 0x22406d50. Ordering is observable. */
void fs98_world_airport_mode(void *object, uint32_t mode)
{
    uint32_t index;
    fs98_world_airport_clear(object);
    U32(PTR(object, 0x10dc), 0x28) = 0x1000 + mode;
    if (mode == 0) {
        for (index = 0; index < 4; ++index) { enable(object, s_labels[index], 1); show(object, s_labels[index], 1); }
        for (index = 0; index < 4; ++index) enable(object, s_places[index], 1);
        for (index = 0; index < 4; ++index) show(object, s_places[index], 1);
    } else if (mode == 1) {
        static const uint32_t s_names[] = {0x1868, 0x1920, 0x1988, 0x1a40};
        for (index = 0; index < 4; ++index) { enable(object, s_names[index], 1); show(object, s_names[index], 1); }
    } else {
        enable(object, 0x1de4, 1); enable(object, 0x1d9c, 1);
        show(object, 0x1de4, 5); show(object, 0x1d9c, 5);
        enable(object, 0x1f9c, 0);
    }
    if (mode != 2) { enable(object, 0x1f54, 1); enable(object, 0x1e9c, 1); enable(object, 0x1f9c, 1); }
    if (mode != 0) {
        static const uint32_t s_hidden[] = {0x142c, 0x1374, 0x152c, 0x1474, 0x162c, 0x1574, 0x172c, 0x1674};
        for (index = 0; index < 8; ++index) { enable(object, s_hidden[index], 0); show(object, s_hidden[index], 0); }
    }
    if (mode != 2) {
        if (mode == 0) {
            static const uint32_t s_names[] = {0x1868, 0x1920, 0x1988, 0x1a40};
            for (index = 0; index < 4; ++index) { show(object, s_names[index], 0); enable(object, s_names[index], 0); }
        }
        enable(object, 0x1de4, 0); enable(object, 0x1d9c, 0);
        show(object, 0x1de4, 0); show(object, 0x1d9c, 0);
    } else {
        static const uint32_t s_names[] = {0x1868, 0x1920, 0x1988, 0x1a40};
        for (index = 0; index < 4; ++index) { show(object, s_names[index], 0); enable(object, s_names[index], 0); }
        enable(object, 0x1f54, 0); enable(object, 0x1e9c, 0);
        callback(object, 0xc);
    }
    fs98_world_airport_populate(object);
}
/* 0x22405a80..0x22405ac0: this cached mode differs from descriptor +0x28. */
void fs98_world_airport_mode_button(void *object, uint32_t mode)
{ if (U32(object, 0x23dc) != mode) { U32(object, 0x23dc) = mode; fs98_world_airport_mode(object, mode); } }
/* 0x224055e0 */
void fs98_world_airport_restore(void *object)
{
    static const uint32_t s_offsets[] = {0x4c, 0x58, 0x64, 0x70, 0x9c, 0x7c, 0xa8};
    uint32_t index, mode; void *descriptor = PTR(object, 0x10dc);
    for (index = 0; index < 7; ++index) U32(object, 0x238c + index * 4 + (index == 6 ? 4 : 0)) = U32(descriptor, s_offsets[index]);
    mode = U32(descriptor, 0x28) - 0x1000;
    if (mode < 3) { static const uint32_t s_radio[] = {0x1280, 0x1774, 0x1ca8}; call1(0x2573c, AT(object, s_radio[mode]), 1); fs98_world_airport_mode(object, mode); }
}
void fs98_world_airport_refresh(void *object) { callback(object, 0x10); fs98_world_airport_restore(object); }
void fs98_world_airport_facilities(void *object) { callback(object, 0x14); }
/* Place and FE-area list selection handlers, 0x22405ae0..0x22405ca0. */
void fs98_world_airport_place(void *object, uint32_t index)
{
    uint32_t selection = send(object, index == 4 ? 0x1d9c : s_places[index], index == 4 ? 0x188 : 0x147, 0, 0);
    U32(PTR(object, 0x10dc), index == 4 ? 0xb4 : 0x4c + index * 12) = selection;
    callback(object, index == 4 ? 0x20 : 0x18);
    text_copy(PTR(PTR(object, 0x10dc), 0x34), "");
    fs98_world_airport_clear(object); fs98_world_airport_populate(object);
}
/* 0x22405de0 */
void fs98_world_airport_airport(void *object)
{
    void *descriptor; uint32_t selection;
    U32(object, 0x23d8) = 1;
    selection = send(object, 0x1c60, 0x188, 0, 0);
    descriptor = PTR(object, 0x10dc); U32(descriptor, 0x7c) = selection;
    descriptor = PTR(object, 0x10dc);
    text_copy(PTR(descriptor, 0x34), PTR(PTR(descriptor, 0x78), U32(descriptor, 0x7c) * 12));
    edit_text(object, PTR(PTR(object, 0x10dc), 0x34)); fs98_world_airport_runways(object);
    U32(object, 0x23d8) = 0;
}
void fs98_world_airport_double_click(void *object)
{
    uint32_t selection = send(object, 0x1c60, 0x188, 0, 0); void **vtable;
    U32(PTR(object, 0x10dc), 0x7c) = selection; vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xcc / 4])(object);
}
static int compare_text(const char *first, const char *second) { return API(0x256a0, int (*)(const char *, const char *))(first, second); }
/* 0x22405fb0: recursive insertion search. Signed midpoint rounds toward zero. */
int fs98_world_airport_search(void *object, int32_t *low, int32_t *high, const char *text, void *rows)
{
    int32_t first = *low, last = *high, middle = (int32_t)((uint32_t)first + (uint32_t)last) / 2;
    int result;
    if (middle == first) {
        if (first < last && compare_text(text, PTR(rows, (uint32_t)first * 12))) return first + 1;
        return first;
    }
    result = compare_text(text, PTR(rows, (uint32_t)middle * 12));
    if (!result) return middle;
    return result < 0 ? fs98_world_airport_search(object, low, &middle, text, rows) : fs98_world_airport_search(object, &middle, high, text, rows);
}
static int find_text(void *object, const char *text, uint32_t list, int allow_null)
{
    int32_t low = 0, high; void *descriptor;
    if (!*text) return 0;
    descriptor = PTR(object, 0x10dc); high = (int32_t)(U32(descriptor, list) - 1);
    if (allow_null && !PTR(descriptor, list + 4)) return 0;
    if (compare_text(text, PTR(PTR(descriptor, list + 4), 0)) < 0) return 0;
    if (compare_text(text, PTR(PTR(PTR(object, 0x10dc), list + 4), (uint32_t)high * 12)) > 0) return high;
    return fs98_world_airport_search(object, &low, &high, text, PTR(PTR(object, 0x10dc), list + 4));
}
/* 0x22405ec0 */
void fs98_world_airport_airport_edit(void *object)
{
    char text[100]; uint32_t selection;
    if (U32(object, 0x23d8)) return;
    API(0x25598, int (TC *)(void *, char *, int))(AT(object, 0x1b40), text, 100);
    selection = (uint32_t)find_text(object, text, 0x74, 1);
    U32(PTR(object, 0x10dc), 0x7c) = selection; U32(object, 0x23a0) = selection;
    send(object, 0x1c60, 0x186, selection, 0); fs98_world_airport_runways(object);
}
/* 0x22406550 */
void fs98_world_airport_name(void *object)
{
    uint32_t selection = send(object, 0x1a40, 0x188, 0, 0); void *descriptor = PTR(object, 0x10dc);
    U32(object, 0x239c) = selection; U32(descriptor, 0x9c) = selection;
    descriptor = PTR(object, 0x10dc);
    text_copy(PTR(descriptor, 0x84), PTR(PTR(descriptor, 0x98), U32(descriptor, 0x9c) * 12));
    callback(object, 0x20); edit_text(object, PTR(PTR(object, 0x10dc), 0x34));
    send(object, 0x1c60, 0x184, 0, 0); send(object, 0x1f54, 0x14b, 0, 0);
    fill(object, 0x74, 0x1c60, 0x23a0, 0);
    descriptor = PTR(object, 0x10dc); edit_text(object, PTR(PTR(descriptor, 0x78), U32(object, 0x23a0) * 12));
    fs98_world_airport_runways(object);
}
/* 0x224063a0: retail writes +0x23a0, rather than +0x239c, here. */
void fs98_world_airport_name_edit(void *object)
{
    char text[100]; uint32_t selection; void *descriptor;
    if (!U32(PTR(object, 0x10dc), 0x94)) return;
    API(0x25598, int (TC *)(void *, char *, int))(AT(object, 0x1920), text, 100);
    selection = (uint32_t)find_text(object, text, 0x94, 0);
    U32(PTR(object, 0x10dc), 0x9c) = selection; U32(object, 0x23a0) = selection;
    send(object, 0x1a40, 0x186, selection, 0);
    descriptor = PTR(object, 0x10dc);
    if (!compare_text(text, PTR(PTR(descriptor, 0x98), selection * 12))) {
        callback(object, 0x20); fs98_world_airport_name(object); return;
    }
    send(object, 0x1c60, 0x184, 0, 0); send(object, 0x1f54, 0x14b, 0, 0);
    edit_text(object, ""); text_copy(PTR(PTR(object, 0x10dc), 0x34), "");
    send(object, 0x1c60, 0x186, 0, 0); U32(PTR(object, 0x10dc), 0x7c) = 0;
}
/* Dropdown open/close, 0x22406070..0x22406350. */
void fs98_world_airport_dropdown(void *object, uint32_t index, int close)
{
    uint32_t control = s_places[index], selection;
    if (!close) {
        send(object, control, 0x14a, 0, U32(object, 0x23ac));
        send(object, control, 0x144, 1, 0);
        send(object, control, 0x14e, U32(object, 0x238c + index * 4), 0);
    } else {
        selection = send(object, control, 0x147, 0, 0);
        U32(object, 0x238c + index * 4) = selection;
        if (index || !selection) placeholder(object, index, U32(PTR(object, 0x10dc), 0x44 + index * 12));
        send(object, control, 0x14a, 0, U32(object, 0x23d0));
        send(object, control, 0x144, 1, 0);
        send(object, control, 0x14e, U32(object, 0x238c + index * 4), 0);
    }
}
/* 0x22406680 keeps the validation vtable for its final Close. */
void fs98_world_airport_accept(void *object)
{
    void **vtable = PTR(object, 0); uint32_t selection;
    int control = ((int (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (((int (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, (uint32_t)control)) return;
    if (API(0x255c4, int (TC *)(void *))(AT(object, 0x1f54))) {
        selection = send(object, 0x1f54, 0x147, 0, 0); U32(PTR(object, 0x10dc), 0xa8) = selection;
    }
    callback(object, 8); U32(PTR(object, 0x10dc), 0) = 1;
    fs98_world_airport_release(object); ((void (TC *)(void *))vtable[0xec / 4])(object);
}
void fs98_world_airport_cancel(void *object)
{
    void **vtable; U32(PTR(object, 0x10dc), 0) = 0;
    fs98_world_airport_release(object); vtable = PTR(object, 0); ((void (TC *)(void *))vtable[0xec / 4])(object);
}
static void text_init(void *object, uint32_t offset, uint32_t font) { API(0x257dc, void (TC *)(void *, void *))(AT(object, offset), AT(object, font)); }
static void radio_init(void *object, uint32_t offset)
{
    void *control = AT(object, offset); void **vtable = PTR(control, 0);
    ((void (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))vtable[0xc4 / 4])(control, AT(object, 0x530), 0x134, AT(object, 0xff4), AT(object, 0x7e8), s_services->resource_module());
}
static void edit_init(void *object, uint32_t offset)
{
    void *font = API(0x257d0, void *(SC *)(uint32_t))(2);
    API(0x25740, void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))(AT(object, offset), font, 0xffffff, 0, 0xff0000, AT(object, 0x1a88), 0);
    send(object, offset, 0xc5, 100, 0);
}
static void list_init(void *object, uint32_t offset, uint32_t label)
{ API(0x25718, void (TC *)(void *, void *, void *))(AT(object, offset), AT(object, 0x450), AT(object, label)); }
static void combo_init(void *object, uint32_t offset, uint32_t label)
{ API(0x25790, void (TC *)(void *, void *, void *))(AT(object, offset), AT(object, 0x3e0), AT(object, label)); }
/* 0x22405190: no local unwind ownership, including HOST allocations. */
int fs98_world_airport_init(void *object)
{
    uint32_t index; void **vtable; void *descriptor; struct fs98_world_rect rect = {0, 0, 0, 0};
    API(0x257c0, void (TC *)(void *))(object); vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(object, 0x14f, s_services->resource_module(), 15)) return 0;
    API(0x257c4, void (TC *)(void *, uint32_t, void *, void *, uint32_t))(AT(object, 0x1198), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    text_init(object, 0x10e0, 0x220); radio_init(object, 0x1280);
    for (index = 0; index < 4; ++index) { text_init(object, s_labels[index], 0x370); combo_init(object, s_places[index], s_labels[index]); }
    radio_init(object, 0x1774); text_init(object, 0x1868, 0x370); edit_init(object, 0x1920);
    text_init(object, 0x1988, 0x370); list_init(object, 0x1a40, 0x1ba8);
    text_init(object, 0x1a88, 0x370); edit_init(object, 0x1b40);
    text_init(object, 0x1ba8, 0x370); list_init(object, 0x1c60, 0x1ba8);
    text_init(object, 0x1e9c, 0x370); combo_init(object, 0x1f54, 0x1e9c);
    radio_init(object, 0x1ca8); text_init(object, 0x1de4, 0x370); list_init(object, 0x1d9c, 0x1de4);
    for (index = 0; index < 4; ++index) {
        static const uint32_t s_buttons[] = {0x1f9c, 0x2098, 0x2194, 0x2290};
        API(0x257c8, void (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, int, uint32_t, int, int, struct fs98_world_rect))(
            AT(object, s_buttons[index]), AT(object, 0x680), AT(object, 0x6f0), index < 2 ? 0x137 : 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_services->resource_module(), 1, 1, rect);
    }
    descriptor = PTR(object, 0x10dc);
    if (!descriptor || (uint32_t)descriptor == 0xcdcdcdcd) {
        API(0x2551c, int (SC *)(const char *, uint32_t, uint32_t))("Bad m_pliveInfo In CGoToLocation InitControls", 0, 0); return 0;
    }
    callback(object, 0x18); callback(object, 0x1c); fs98_world_airport_allocate(object); fs98_world_airport_restore(object);
    U32(object, 0x23dc) = 3; U32(object, 0x23d8) = 0;
    API(0x25590, void (TC *)(void *))(AT(object, 0x1b40)); return 0;
}
