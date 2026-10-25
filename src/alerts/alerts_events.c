/* Alerts' command handlers and modeless tick, transliterated from the retail
 * 1997 reference. alerts_runtime.c supplies the retail-facing DLL adapter.
 * The external harness calls the original routines and these over identical
 * objects, descriptors and dependency scripts. See docs/modules/alerts.md. */
#include "alerts_internal.h"
#include "alerts_tables.inc"
#include "alerts_unwind.h"

static const struct fs98_alerts_services *s_services;
static uint32_t s_resource_module;

struct fs98_alerts_event_unwind {
    struct fs98_alerts_unwind frame;
    void *first, *second;
    uint32_t kind;
    volatile uint32_t stage;
};

static void fs98_alerts_event_cleanup(struct fs98_alerts_unwind *frame)
{
    struct fs98_alerts_event_unwind *state = (void *)frame;
    if (state->stage == 2) {
        state->stage = 1;
        if (state->kind == 1) s_services->dib_destroy(state->second);
        else s_services->dc_destroy(state->second);
    }
    if (state->stage == 1) {
        state->stage = 0;
        if (state->kind == 0) s_services->dib_destroy(state->first);
        else s_services->string_destroy(state->first);
    }
}

static void fs98_alerts_event_scope(struct fs98_alerts_event_unwind *state,
                                    void *first, void *second, uint32_t kind)
{
    state->first = first; state->second = second;
    state->kind = kind; state->stage = 0;
    fs98_alerts_unwind_enter(&state->frame, fs98_alerts_event_cleanup);
}

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))

typedef void (FS98_ALERTS_THISCALL *fs98_alerts_virtual_fn)(void *object);

static void fs98_alerts_virtual(void *object, uint32_t offset)
{
    fs98_alerts_virtual_fn *functions = PTR(object, 0);
    functions[offset / 4](object);
}

void fs98_alerts_set_services(const struct fs98_alerts_services *services)
{
    s_services = services;
}

void fs98_alerts_set_resource_module(uint32_t module)
{
    s_resource_module = module;
}

/* 0x223019c0 / 0x223019e0 / 0x22301a00: the button returns its stored bit,
 * not its control id. In particular an invisible button can carry zero. */
static void fs98_alerts_modal_answer(void *object, uint32_t offset)
{
    void *descriptor = PTR(object, 0x10dc);
    D32(descriptor, 0x0c) = D32(object, offset);
    fs98_alerts_virtual(object, 0xec);
}

void FS98_ALERTS_THISCALL fs98_alerts_modal_first(void *object)
{
    fs98_alerts_modal_answer(object, 0x10e4);
}

void FS98_ALERTS_THISCALL fs98_alerts_modal_second(void *object)
{
    fs98_alerts_modal_answer(object, 0x10e8);
}

void FS98_ALERTS_THISCALL fs98_alerts_modal_third(void *object)
{
    fs98_alerts_modal_answer(object, 0x10ec);
}

/* 0x22302560: retain both stores, even though the second overwrites the first.
 * Descriptor pointers are reloaded as retail does before virtual Close. */
void FS98_ALERTS_THISCALL fs98_alerts_advisory_accept(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    D32(descriptor, 0x10) = D32(object, 0x1550);
    descriptor = PTR(object, 0x10dc);
    *(volatile uint32_t *)descriptor = 1;
    descriptor = PTR(object, 0x10dc);
    *(volatile uint32_t *)descriptor = 0;
    fs98_alerts_virtual(object, 0xec);
}

/* 0x223025d0. */
void FS98_ALERTS_THISCALL fs98_alerts_advisory_quit(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    D32(descriptor, 0x10) = D32(object, 0x1550);
    descriptor = PTR(object, 0x10dc);
    D32(descriptor, 0) = 0;
    descriptor = PTR(object, 0x10dc);
    D32(descriptor, 0x0c) = 1;
    fs98_alerts_virtual(object, 0xec);
}

/* 0x223025b0 / 0x223025a0. */
void FS98_ALERTS_THISCALL fs98_alerts_advisory_help(void *object)
{
    s_services->win_help(D32(object, 0x20), "fs98.hlp", 8, 0xbc2);
}

void FS98_ALERTS_THISCALL fs98_alerts_noop(void *object)
{
    (void)object;
}

/* 0x22301450 / 0x22301b70 / 0x22302260. The control binding order is
 * observable through MFC and is deliberately not sorted by control id. */
void FS98_ALERTS_THISCALL fs98_alerts_modal_exchange(void *object, void *exchange)
{
    s_services->base_exchange(object, exchange);
    s_services->ddx_control(exchange, 0x7d1, (uint8_t *)object + 0x10fc);
    s_services->ddx_control(exchange, 0x7d5, (uint8_t *)object + 0x13ac);
    s_services->ddx_control(exchange, 0x7d4, (uint8_t *)object + 0x12b0);
    s_services->ddx_control(exchange, 0x7d3, (uint8_t *)object + 0x11b4);
}

void FS98_ALERTS_THISCALL fs98_alerts_modeless_exchange(void *object, void *exchange)
{
    s_services->base_exchange(object, exchange);
    s_services->ddx_control(exchange, 0x7d6, (uint8_t *)object + 0x10e4);
    s_services->ddx_control(exchange, 0x7d2, (uint8_t *)object + 0x1124);
}

void FS98_ALERTS_THISCALL fs98_alerts_advisory_exchange(void *object, void *exchange)
{
    s_services->base_exchange(object, exchange);
    s_services->ddx_control(exchange, 0x7da, (uint8_t *)object + 0x10e8);
    s_services->ddx_control(exchange, 0x7d9, (uint8_t *)object + 0x11e4);
    s_services->ddx_control(exchange, 0x7d1, (uint8_t *)object + 0x12e0);
    s_services->ddx_control(exchange, 0x7d8, (uint8_t *)object + 0x1398);
    s_services->ddx_control(exchange, 0x7d7, (uint8_t *)object + 0x1494);
}

/* 0x22301bc0: initialise the modeless text/icon and progress visibility.
 * CDIBSection's width/height are the dwords at +8/+0xc of its 0x70 bytes. */
FS98_ALERTS_NOINLINE int32_t FS98_ALERTS_THISCALL fs98_alerts_modeless_init(void *object)
{
    typedef int32_t (FS98_ALERTS_THISCALL *fs98_init_fn)(void *self, int32_t id,
                                                     uint32_t module, uint32_t flags);
    uint8_t dib_storage[0x70];
    char buffer[0x200];
    void *descriptor;
    const char *text;
    fs98_init_fn *vtable;
    uint32_t text_value, tag;
    int32_t icon;
    struct fs98_alerts_event_unwind state;
    fs98_alerts_event_scope(&state, dib_storage, 0, 0);
    s_services->dib_construct(dib_storage);
    state.stage = 1;
    s_services->base_init(object);
    vtable = PTR(object, 0);
    if (!vtable[0xe4 / 4](object, 0x14f, s_resource_module, 9)) {
        fs98_alerts_event_cleanup(&state.frame);
        fs98_alerts_unwind_leave(&state.frame);
        return 0;
    }
    descriptor = PTR(object, 0x10dc);
    text_value = D32(descriptor, 0x24);
    text = (const char *)text_value;
    if ((int32_t)text_value <= 0xffff) {
        s_services->load_string(text_value, buffer, 0x200);
        text = buffer;
    }
    s_services->set_text((uint8_t *)object + 0x1124, text);
    s_services->init_text((uint8_t *)object + 0x1124, (uint8_t *)object + 0x220);
    descriptor = PTR(object, 0x10dc);
    tag = D32(descriptor, 8);
    icon = tag == 1 ? 0x86 : tag == 2 ? 0x87 : tag == 4 ? 0x89 : 0x88;
    if (tag)
        s_services->load_dib(dib_storage, icon, D32(object, 0x10e0),
                             (uint8_t *)object + 0x7e8, 0);
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 8))
        s_services->sprite_copy(dib_storage, (uint8_t *)object + 0xff4, 0x1e, 0x22,
                                (int32_t)D32(dib_storage, 8), (int32_t)D32(dib_storage, 0x0c),
                                0, 0, 0xffffffff);
    descriptor = PTR(object, 0x10dc);
    if (!D32(descriptor, 0x10))
        s_services->show_window((uint8_t *)object + 0x10e4, 0);
    fs98_alerts_event_cleanup(&state.frame);
    fs98_alerts_unwind_leave(&state.frame);
    return 1;
}

/* 0x223014c0: modal text, low-eight-bit button selection, button surfaces,
 * and optional icon. The label ids come from the original jump tables. */
FS98_ALERTS_NOINLINE int32_t FS98_ALERTS_THISCALL fs98_alerts_modal_init(void *object)
{
    typedef int32_t (FS98_ALERTS_THISCALL *fs98_init_fn)(void *self, int32_t id,
                                                     uint32_t module, uint32_t flags);
    uint8_t dib_storage[0x70];
    char text_buffer[0x200], label[0x100];
    struct fs98_alerts_rect zero = {0, 0, 0, 0};
    fs98_init_fn *vtable;
    void *descriptor;
    const char *text;
    uint32_t text_value, mask, count = 0, index, bit, value, tag;
    int32_t icon;
    struct fs98_alerts_event_unwind state;
    fs98_alerts_event_scope(&state, dib_storage, 0, 0);
    s_services->dib_construct(dib_storage);
    state.stage = 1;
    s_services->base_init(object);
    vtable = PTR(object, 0);
    if (!vtable[0xe4 / 4](object, 0x14f, s_resource_module, 9)) {
        fs98_alerts_event_cleanup(&state.frame);
        fs98_alerts_unwind_leave(&state.frame);
        return 0;
    }
    descriptor = PTR(object, 0x10dc);
    text_value = D32(descriptor, 0x14);
    text = (const char *)text_value;
    if ((int32_t)text_value <= 0xffff) {
        s_services->load_string(text_value, text_buffer, 0x200);
        text = text_buffer;
    }
    s_services->set_text((uint8_t *)object + 0x10fc, text);
    s_services->init_text((uint8_t *)object + 0x10fc, (uint8_t *)object + 0x220);
    for (mask = 1; mask < 0x100; mask <<= 1) {
        descriptor = PTR(object, 0x10dc);
        if ((D32(descriptor, 8) & mask) && count < 3) {
            D32(object, 0x10e4 + count * 4) = mask;
            count++;
        }
    }
    if (D32(object, 0x10e8) == 0) {
        D32(object, 0x10e8) = D32(object, 0x10e4);
        D32(object, 0x10e4) = 0;
    }
    if (D32(object, 0x10ec) == 0) {
        D32(object, 0x10ec) = D32(object, 0x10e8);
        D32(object, 0x10e8) = D32(object, 0x10e4);
        D32(object, 0x10e4) = 0;
    }
    /* The listing's focus/default branch is dead: its stack variable starts
     * at -1 and can only be changed behind a test that it is already != -1.
     * No call can observe the omitted test. The two answer indices below
     * are separate live object fields and must still be written. */
    for (index = 0; index < 3; index++) {
        void *button = (uint8_t *)object + 0x11b4 + index * 0xfc;
        value = D32(object, 0x10e4 + index * 4);
        for (bit = 0; bit < 8; bit++)
            if (value == (1u << bit))
                break;
        if (bit == 8) {
            s_services->show_window(button, 0);
        } else {
            s_services->window_load_string(D32(object, 0x10e0), s_button_text[bit],
                                          label, 0x100);
            s_services->button_text(button, label);
            if (value == 1 || value == 2 || value == 0x40)
                D32(object, 0x10f4) = index;
            else if (value == 4 || value == 0x80)
                D32(object, 0x10f8) = index;
        }
    }
    for (index = 0; index < 3; index++)
        s_services->button_init((uint8_t *)object + 0x11b4 + index * 0xfc,
                                 (uint8_t *)object + 0x5a0, (uint8_t *)object + 0x610,
                                 0x132, (uint8_t *)object + 0xff4,
                                 (uint8_t *)object + 0x7e8, 1, 0xffffffff,
                                 s_resource_module, 1, 1, zero);
    descriptor = PTR(object, 0x10dc);
    tag = D32(descriptor, 0x10);
    icon = tag == 1 ? 0x86 : tag == 2 ? 0x87 : tag == 4 ? 0x89 : 0x88;
    if (tag)
        s_services->load_dib(dib_storage, icon, D32(object, 0x10e0),
                             (uint8_t *)object + 0x7e8, 0);
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 0x10))
        s_services->sprite_copy(dib_storage, (uint8_t *)object + 0xff4, 0x1e, 0x22,
                                (int32_t)D32(dib_storage, 8), (int32_t)D32(dib_storage, 0xc),
                                0, 0, 0xffffffff);
    fs98_alerts_event_cleanup(&state.frame);
    fs98_alerts_unwind_leave(&state.frame);
    return 1;
}

/* 0x223022f0: the advisory's fixed message, three buttons and suppression
 * checkbox. SIMSUI SetChecked writes checkbox +0xbc, i.e. object +0x1550. */
FS98_ALERTS_NOINLINE int32_t FS98_ALERTS_THISCALL fs98_alerts_advisory_init(void *object)
{
    typedef int32_t (FS98_ALERTS_THISCALL *fs98_init_fn)(void *self, int32_t id,
                                                     uint32_t module, uint32_t flags);
    typedef int32_t (FS98_ALERTS_THISCALL *fs98_check_init_fn)(void *self, void *font,
                uint32_t bitmap, void *dib, void *palette, uint32_t module);
    uint8_t string_storage[4], dib_storage[0x70];
    char buffer[0x200];
    struct fs98_alerts_rect zero = {0, 0, 0, 0};
    fs98_init_fn *vtable;
    fs98_check_init_fn *check_vtable;
    void *check = (uint8_t *)object + 0x1494;
    void *descriptor;
    uint32_t index;
    struct fs98_alerts_event_unwind state;
    fs98_alerts_event_scope(&state, string_storage, dib_storage, 1);
    s_services->string_construct(string_storage);
    state.stage = 1;
    s_services->dib_construct(dib_storage);
    state.stage = 2;
    s_services->base_init(object);
    vtable = PTR(object, 0);
    if (!vtable[0xe4 / 4](object, 0x14f, s_resource_module, 9)) {
        fs98_alerts_event_cleanup(&state.frame);
        fs98_alerts_unwind_leave(&state.frame);
        return 0;
    }
    s_services->load_string(0xf6f0, buffer, 0x200);
    s_services->set_text((uint8_t *)object + 0x12e0, buffer);
    s_services->init_text((uint8_t *)object + 0x12e0, (uint8_t *)object + 0x220);
    /* The listing initializes OK, Help, Quit in descending object order. */
    for (index = 0; index < 3; index++) {
        uint32_t offset = index == 0 ? 0x1398 : index == 1 ? 0x11e4 : 0x10e8;
        s_services->button_init((uint8_t *)object + offset,
                                 (uint8_t *)object + 0x5a0, (uint8_t *)object + 0x610,
                                 0x132, (uint8_t *)object + 0xff4,
                                 (uint8_t *)object + 0x7e8, 1, 0xffffffff,
                                 s_resource_module, 1, 1, zero);
    }
    check_vtable = PTR(check, 0);
    check_vtable[0xc4 / 4](check, (uint8_t *)object + 0x4c0, 0x139,
                           (uint8_t *)object + 0xff4, (uint8_t *)object + 0x7e8,
                           s_resource_module);
    s_services->load_dib(dib_storage, 0x87, D32(object, 0x10e0),
                         (uint8_t *)object + 0x7e8, 0);
    s_services->sprite_copy(dib_storage, (uint8_t *)object + 0xff4, 0x1e, 0x22,
                            (int32_t)D32(dib_storage, 8), (int32_t)D32(dib_storage, 0xc),
                            0, 0, 0xffffffff);
    descriptor = PTR(object, 0x10dc);
    s_services->set_checked(check, (int32_t)D32(descriptor, 0x10));
    fs98_alerts_event_cleanup(&state.frame);
    fs98_alerts_unwind_leave(&state.frame);
    return 1;
}

/* Use modulo-2^32 additions/subtractions before interpreting signed results.
 * Retail's drawing arithmetic is ordinary wrapping x86, not C signed UB. */
static int32_t fs98_alerts_add(int32_t left, int32_t right)
{
    return (int32_t)((uint32_t)left + (uint32_t)right);
}

static int32_t fs98_alerts_sub(int32_t left, int32_t right)
{
    return (int32_t)((uint32_t)left - (uint32_t)right);
}

/* 0x22301db0..0x22302090. Current/total comparisons are signed, but the
 * progress product wraps at 32 bits and its division is unsigned. The split
 * uses a separate signed product and truncating divide by 100. */
FS98_ALERTS_NOINLINE int32_t FS98_ALERTS_THISCALL fs98_alerts_progress_tick(void *object,
                                                   uint32_t w_param, uint32_t l_param)
{
    uint8_t string_storage[4];
    uint8_t dc_storage[0x14];
    struct fs98_alerts_rect remaining, filled;
    struct fs98_alerts_size text_size;
    void *descriptor = PTR(object, 0x10dc);
    void *font;
    const char *text;
    uint32_t child, percent, start, limit, length;
    int32_t current, total, width, height, split, x, y;
    struct fs98_alerts_event_unwind state;
    (void)w_param;
    (void)l_param;

    if (D32(descriptor, 0x0c) == 0) {
        /* Retail keeps the original vtable in edi across the first call. */
        fs98_alerts_virtual_fn *functions = PTR(object, 0);
        functions[0xec / 4](object);
        functions[0x60 / 4](object);
        return 0;
    }
    if (D32(descriptor, 0x10) == 0)
        return 0;

    fs98_alerts_event_scope(&state, string_storage, dc_storage, 2);
    s_services->string_construct(string_storage);
    state.stage = 1;
    descriptor = PTR(object, 0x10dc);
    current = (int32_t)D32(descriptor, 0x14);
    total = (int32_t)D32(descriptor, 0x18);
    s_services->dc_construct(dc_storage, object);
    state.stage = 2;
    font = s_services->font_from_handle(s_services->stock_object(13));
    s_services->select_font(dc_storage, font);

    child = D32(object, 0x1104);
    s_services->client_rect(child, &remaining);
    width = remaining.right;
    height = remaining.bottom;
    s_services->client_to_screen(D32(object, 0x1104),
                                (struct fs98_alerts_point *)&remaining.left);
    s_services->client_to_screen(D32(object, 0x1104),
                                (struct fs98_alerts_point *)&remaining.right);
    s_services->screen_to_client(D32(object, 0x20),
                                (struct fs98_alerts_point *)&remaining.left);
    s_services->screen_to_client(D32(object, 0x20),
                                (struct fs98_alerts_point *)&remaining.right);
    remaining.right = fs98_alerts_add(remaining.left, fs98_alerts_sub(width, 1));
    remaining.bottom = fs98_alerts_add(remaining.top, fs98_alerts_sub(height, 1));

    if (total <= 0)
        total = 1;
    if (current > total)
        current = total;
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 0x20) > 100)
        D32(descriptor, 0x20) = 100;
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 0x20) == 0)
        D32(descriptor, 0x20) = 1;
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 0x1c) >= D32(descriptor, 0x20))
        D32(descriptor, 0x1c) = D32(descriptor, 0x20) - 1;
    filled = remaining;
    descriptor = PTR(object, 0x10dc);
    start = D32(descriptor, 0x1c);
    limit = D32(descriptor, 0x20);
    percent = (uint32_t)((limit - start) * (uint32_t)current) / (uint32_t)total + start;
    split = (int32_t)((uint32_t)fs98_alerts_sub(remaining.right, remaining.left) * percent);
    split = fs98_alerts_add(remaining.left, split / 100);
    remaining.left = split;
    filled.right = split;
    s_services->format(string_storage, "%3d%%", (int32_t)percent);
    text = PTR(string_storage, 0);
    length = D32(text, -8);
    s_services->text_extent(D32(dc_storage, 8), text, (int32_t)length, &text_size);

    s_services->text_color(dc_storage, 0xffffff);
    s_services->background_color(dc_storage, 0x7f0000);
    text = PTR(string_storage, 0);
    length = D32(text, -8);
    y = fs98_alerts_add(filled.top,
                       fs98_alerts_sub(fs98_alerts_sub(filled.bottom, text_size.height), filled.top) / 2);
    x = fs98_alerts_add(filled.left,
                       fs98_alerts_sub(fs98_alerts_sub(remaining.right, text_size.width), filled.left) / 2);
    s_services->text_out(D32(dc_storage, 4), x, y, 6, &filled, text, length, 0);
    s_services->background_color(dc_storage, 0xffffff);
    s_services->text_color(dc_storage, 0x7f0000);
    text = PTR(string_storage, 0);
    length = D32(text, -8);
    /* The second pass uses the same uncut overall rectangle for centering. */
    y = fs98_alerts_add(filled.top,
                       fs98_alerts_sub(fs98_alerts_sub(filled.bottom, text_size.height), filled.top) / 2);
    x = fs98_alerts_add(filled.left,
                       fs98_alerts_sub(fs98_alerts_sub(remaining.right, text_size.width), filled.left) / 2);
    s_services->text_out(D32(dc_storage, 4), x, y, 6, &remaining, text, length, 0);
    fs98_alerts_event_cleanup(&state.frame);
    fs98_alerts_unwind_leave(&state.frame);
    return 0;
}
