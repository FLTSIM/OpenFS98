/* Views' record editing, validation and preview callbacks. The dependency
 * boundary is shared by the runtime adapter and the differential harness.
 * Object and descriptor offsets are the retail PE32 layout. */
#include "views_internal.h"
#include "views_tables.inc"
#include "views_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))

static const struct fs98_views_services *s_services;
static uint32_t s_resource_module;

void fs98_views_set_services(const struct fs98_views_services *services)
{
    s_services = services;
}

static void *fs98_views_record(void *object)
{
    void *descriptor = PTR(object, 0x2804);
    return (uint8_t *)PTR(descriptor, 0x1c) + D32(descriptor, 0x14) * (uint32_t)0xb4;
}

/* 0x223e34c0/3500/3540: query the control before reloading the descriptor. */
static void fs98_views_combo(void *object, uint32_t control_offset, uint32_t record_offset)
{
    uint32_t selection = (uint32_t)s_services->send_message(D32(object, control_offset), 0x147, 0, 0);
    D32(fs98_views_record(object), record_offset) = selection;
}

void FS98_VIEWS_THISCALL fs98_views_direction(void *object)
{ fs98_views_combo(object, 0x1f54, 0x74); }
void FS98_VIEWS_THISCALL fs98_views_transition(void *object)
{ fs98_views_combo(object, 0x2404, 0xa4); }
void FS98_VIEWS_THISCALL fs98_views_altitude_reference(void *object)
{ fs98_views_combo(object, 0x25cc, 0xac); }

/* 0x223e3580: axis and title flags belong to the descriptor, not a record. */
void FS98_VIEWS_THISCALL fs98_views_axis(void *object)
{
    uint32_t selection = (uint32_t)s_services->send_message(D32(object, 0x26cc), 0x147, 0, 0);
    D32(PTR(object, 0x2804), 0x20) = selection;
}

void FS98_VIEWS_THISCALL fs98_views_fullscreen(void *object)
{
    void *descriptor = PTR(object, 0x2804);
    uint32_t checked = D32(object, 0x1d50);
    D32((uint8_t *)PTR(descriptor, 0x1c) + D32(descriptor, 0x14) * (uint32_t)0xb4, 0x70) = checked;
}

void FS98_VIEWS_THISCALL fs98_views_gradual(void *object)
{
    void *descriptor = PTR(object, 0x2804);
    uint32_t checked = D32(object, 0x24e8);
    D32((uint8_t *)PTR(descriptor, 0x1c) + D32(descriptor, 0x14) * (uint32_t)0xb4, 0xa8) = checked;
}

void FS98_VIEWS_THISCALL fs98_views_titles(void *object)
{ D32(PTR(object, 0x2804), 0x24) = D32(object, 0x27b0); }

int32_t FS98_VIEWS_THISCALL fs98_views_help(void *object, uint32_t unused, void *info)
{
    (void)object; (void)unused;
    return s_services->win_help(D32(info, 0xc), "fs98.hlp", 0xc, s_help_map);
}

int32_t FS98_VIEWS_THISCALL fs98_views_context_help(void *object, uint32_t window, uint32_t unused)
{
    (void)object; (void)unused;
    s_services->win_help(window, "fs98.hlp", 0xa, s_help_map);
    return 1;
}

int32_t FS98_VIEWS_THISCALL fs98_views_tick(void *object, uint32_t w_param, uint32_t l_param)
{
    void *descriptor = PTR(object, 0x2804);
    fs98_views_preview_fn preview = PTR(descriptor, 8);
    (void)w_param; (void)l_param;
    preview(3, descriptor, 0);
    return 0;
}

void FS98_VIEWS_THISCALL fs98_views_close(void *object)
{
    void *descriptor = PTR(object, 0x2804);
    fs98_views_preview_fn preview = PTR(descriptor, 8);
    preview(2, descriptor, 0);
    s_services->close(object);
}

/* 0x223e21a0: record +0x10/+0x14 are a buffer pointer and capacity, not
 * a double. Each mode has its own 0x14-byte numeric exchange descriptor. */
void FS98_VIEWS_THISCALL fs98_views_sync(void *object)
{
    void *record = fs98_views_record(object);
    uint32_t mode = D32(record, 8);
    uint32_t offset;
    switch (mode) {
    case 0: offset = 0x10; break;
    case 1: offset = 0x24; break;
    case 2: offset = 0x38; break;
    case 3: offset = 0x4c; break;
    case 4: offset = 0x60; break;
    default: return;
    }
    s_services->get_text((uint8_t *)object + 0x1e14, PTR(record, offset), (int32_t)D32(record, offset + 4));
    if (mode == 4) {
        record = fs98_views_record(object);
        s_services->get_text((uint8_t *)object + 0x2034, PTR(record, 0x80), (int32_t)D32(record, 0x84));
        record = fs98_views_record(object);
        s_services->get_text((uint8_t *)object + 0x220c, PTR(record, 0x94), (int32_t)D32(record, 0x98));
    }
}

/* 0x223e3600: synchronization, caller validation, fresh record, error id. */
int32_t FS98_VIEWS_THISCALL fs98_views_validate(void *object, uint32_t unused)
{
    void *descriptor, *record;
    fs98_views_validate_fn validate;
    uint32_t mode, offset;
    (void)unused;
    fs98_views_sync(object);
    descriptor = PTR(object, 0x2804);
    validate = PTR(descriptor, 4);
    validate(descriptor);
    record = fs98_views_record(object);
    if (D32(record, 4)) return 0;
    mode = D32(record, 8);
    switch (mode) {
    case 0: offset = 0xc; break;
    case 1: offset = 0x20; break;
    case 2: offset = 0x34; break;
    case 3: offset = 0x48; break;
    case 4:
        if (D32(record, 0x7c)) return 0x1889;
        if (D32(record, 0x90)) return 0x188b;
        offset = 0x5c;
        break;
    default: return 0;
    }
    return D32(record, offset) ? 0x1886 : 0;
}

/* 0x223e36f0: report selected error, then dispatch the inherited virtual
 * at +0xe0. A rejected error dialog still receives that virtual call with 0. */
int32_t FS98_VIEWS_THISCALL fs98_views_report_error(void *object, int32_t display)
{
    typedef int32_t (FS98_VIEWS_THISCALL *error_fn)(void *self, uint32_t control);
    uint32_t control = 0, mode, offset = 0;
    void *record;
    if (display) {
        record = fs98_views_record(object);
        mode = D32(record, 8);
        if (!D32(record, 4)) {
            switch (mode) {
            case 0: offset = 0xc; break;
            case 1: offset = 0x20; break;
            case 2: offset = 0x34; break;
            case 3: offset = 0x48; break;
            case 4:
                if (D32(record, 0x7c)) offset = 0x7c;
                else if (D32(record, 0x90)) offset = 0x90;
                else offset = 0x5c;
                break;
            default: break;
            }
            if (offset) {
                uint32_t edit = offset == 0x7c ? 0x2034 : offset == 0x90 ? 0x220c : 0x1e14;
                if (s_services->error_dialog(object, (uint8_t *)object + edit, (uint8_t *)record + offset))
                    control = offset == 0x7c ? 0x1889 : offset == 0x90 ? 0x188b : 0x1886;
            }
        }
    }
    return ((error_fn *)PTR(object, 0))[0xe0 / 4](object, control);
}

void FS98_VIEWS_THISCALL fs98_views_disable(void *object)
{ s_services->enable_window(D32(object, 0x20), 0); }
void FS98_VIEWS_THISCALL fs98_views_enable(void *object)
{ s_services->enable_window(D32(object, 0x20), 1); }
void FS98_VIEWS_THISCALL fs98_views_empty_arg(void *object, uint32_t unused)
{ (void)object; (void)unused; }

/* 0x223e24f0..2f30: the five 3D view modes share a fixed sequence of
 * control enable/show calls. The states are extracted from those listings;
 * text, selection and checkbox calls below preserve their intervening reads. */
static void fs98_views_mode(void *object, uint32_t mode)
{
    uint8_t *record = fs98_views_record(object);
    uint32_t index, text_offset = 0x10 + mode * 0x14;
    void *preview;
    D32(record, 4) = 0;
    D32(record, 8) = mode;
    preview = s_services->get_control(object, 0x188e);
    s_services->enable_control(preview, mode == 4);
    for (index = 0; index < 15; index++) {
        const uint16_t *state = s_mode_controls[mode][index];
        void *control = (uint8_t *)object + state[0];
        s_services->enable_control(control, state[1]);
        s_services->show_control(control, state[2]);
    }
    s_services->set_text((uint8_t *)object + 0x1e14, "");
    record = fs98_views_record(object);
    s_services->set_text((uint8_t *)object + 0x1e14, PTR(record, text_offset));
    if (mode <= 1) {
        record = fs98_views_record(object);
        s_services->send_message(D32(object, 0x1f54), 0x14e, D32(record, 0x74), 0);
    } else if (mode == 4) {
        record = fs98_views_record(object);
        s_services->set_text((uint8_t *)object + 0x2034, PTR(record, 0x80));
        record = fs98_views_record(object);
        s_services->set_text((uint8_t *)object + 0x220c, PTR(record, 0x94));
        record = fs98_views_record(object);
        s_services->send_message(D32(object, 0x2404), 0x14e, D32(record, 0xa4), 0);
        record = fs98_views_record(object);
        s_services->set_checked((uint8_t *)object + 0x242c, (int32_t)D32(record, 0xa8));
    }
}
void FS98_VIEWS_THISCALL fs98_views_cockpit(void *object) { fs98_views_mode(object, 0); }
void FS98_VIEWS_THISCALL fs98_views_virtual_cockpit(void *object) { fs98_views_mode(object, 1); }
void FS98_VIEWS_THISCALL fs98_views_tower(void *object) { fs98_views_mode(object, 2); }
void FS98_VIEWS_THISCALL fs98_views_track(void *object) { fs98_views_mode(object, 3); }
void FS98_VIEWS_THISCALL fs98_views_spot(void *object) { fs98_views_mode(object, 4); }

/* 0x223e1920/1940: accept keeps the entry vtable across validation/error
 * callbacks and field synchronization; cancel does not validate or sync. */
void FS98_VIEWS_THISCALL fs98_views_cancel(void *object)
{
    typedef void (FS98_VIEWS_THISCALL *close_fn)(void *self);
    void **vtable = PTR(object, 0);
    D32(PTR(object, 0x2804), 0) = 0;
    ((close_fn)vtable[0xec / 4])(object);
}

void FS98_VIEWS_THISCALL fs98_views_accept(void *object)
{
    typedef int32_t (FS98_VIEWS_THISCALL *arg_fn)(void *self, uint32_t argument);
    typedef void (FS98_VIEWS_THISCALL *close_fn)(void *self);
    void **vtable = PTR(object, 0);
    uint32_t error = (uint32_t)((arg_fn)vtable[0xd8 / 4])(object, 0);
    if (!error || !((arg_fn)vtable[0xdc / 4])(object, error)) {
        D32(PTR(object, 0x2804), 0) = 1;
        fs98_views_sync(object);
        ((close_fn)vtable[0xec / 4])(object);
    }
}

void FS98_VIEWS_THISCALL fs98_views_post_destroy(void *object)
{
    typedef void *(FS98_VIEWS_THISCALL *delete_fn)(void *self, uint32_t flags);
    if (object) ((delete_fn *)PTR(object, 0))[1](object, 1);
}

/* 0x223e1690: 34 child bindings, without a base DoDataExchange call. */
void FS98_VIEWS_THISCALL fs98_views_exchange(void *object, void *exchange)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_exchange_controls) / sizeof(s_exchange_controls[0]); index++)
        s_services->ddx_control(exchange, s_exchange_controls[index][1],
                               (uint8_t *)object + s_exchange_controls[index][0]);
}

struct fs98_views_strings {
    struct fs98_views_unwind frame;
    uint8_t strings[5][4];
    volatile uint32_t count, temporary;
};
static void fs98_views_strings_cleanup(struct fs98_views_unwind *frame)
{
    struct fs98_views_strings *state = (void *)frame;
    if (state->temporary) {
        state->temporary = 0;
        s_services->string_destroy(state->strings[4]);
    }
    while (state->count) {
        state->count--;
        s_services->string_destroy(state->strings[state->count]);
    }
}
static void fs98_views_strings_begin(struct fs98_views_strings *state, uint32_t count)
{
    state->count = state->temporary = 0;
    fs98_views_unwind_enter(&state->frame, fs98_views_strings_cleanup);
    while (state->count < count) {
        s_services->string_construct(state->strings[state->count]);
        state->count++;
    }
}
static void fs98_views_strings_end(struct fs98_views_strings *state)
{
    fs98_views_strings_cleanup(&state->frame);
    fs98_views_unwind_leave(&state->frame);
}
static void fs98_views_assign_temporary(struct fs98_views_strings *state, uint32_t index)
{
    state->temporary = 1;
    s_services->string_assign(state->strings[index], state->strings[4]);
    state->temporary = 0;
    s_services->string_destroy(state->strings[4]);
}

/* 0x223e2f30: map altitude choices combine two resource lists and a suffix.
 * Invalid unit selectors leave retail's local metric flag uninitialized;
 * the legitimate caller supplies 0, 1 or 2. */
FS98_VIEWS_NOINLINE void fs98_views_map_body(void *object, uint32_t legacy_bits)
{
    struct fs98_views_strings state;
    void *preview, *record, *descriptor;
    uint32_t index, selector;
    int32_t count, comma;
    volatile int32_t metric = (int32_t)legacy_bits;
    char *buffer;
    fs98_views_strings_begin(&state, 4);
    record = fs98_views_record(object);
    D32(record, 4) = 1;
    preview = s_services->get_control(object, 0x188e);
    s_services->enable_control(preview, 0);
    for (index = 0; index < 15; index++) {
        const uint16_t *control = s_mode_controls[0][index];
        uint32_t visible = index >= 13;
        s_services->enable_control((uint8_t *)object + control[0], (int32_t)visible);
        s_services->show_control((uint8_t *)object + control[0], visible ? 5 : 0);
    }
    s_services->send_message(D32(object, 0x25cc), 0x14b, 0, 0);
    descriptor = PTR(object, 0x2804);
    selector = D32(descriptor, 0x10);
    if ((int32_t)selector >= 0 && selector <= 1) metric = 0;
    else if (selector == 2) metric = 1;
    buffer = s_services->string_buffer(state.strings[0], 0x400);
    s_services->load_string(D32(object, 0x80), metric ? 0xf87d : 0xf870, buffer, 0x400);
    s_services->string_release(state.strings[0], -1);
    buffer = s_services->string_buffer(state.strings[1], 0x400);
    s_services->load_string(D32(object, 0x80), metric ? 0xf883 : 0xf881, buffer, 0x400);
    s_services->string_release(state.strings[1], -1);
    buffer = s_services->string_buffer(state.strings[2], 0x400);
    s_services->load_string(D32(object, 0x80), metric ? 0xf882 : 0xf884, buffer, 0x400);
    s_services->string_release(state.strings[2], -1);
    count = metric ? 12 : 15;
    while (count-- > 0) {
        comma = s_services->string_find_char(state.strings[0], ',');
        s_services->string_mid(state.strings[0], state.strings[4], 0, comma);
        fs98_views_assign_temporary(&state, 3);
        s_services->string_append_text(state.strings[3], "  ");
        s_services->string_append(state.strings[3], state.strings[1]);
        s_services->string_tail(state.strings[0], state.strings[4], (int32_t)((uint32_t)comma + 1));
        fs98_views_assign_temporary(&state, 0);
        s_services->send_message(D32(object, 0x25cc), 0x143, 0, D32(state.strings[3], 0));
    }
    comma = s_services->string_find_char(state.strings[0], ',');
    while (comma != -1) {
        s_services->string_mid(state.strings[0], state.strings[4], 0, comma);
        fs98_views_assign_temporary(&state, 3);
        s_services->string_append_text(state.strings[3], "  ");
        s_services->string_append(state.strings[3], state.strings[2]);
        s_services->string_tail(state.strings[0], state.strings[4], (int32_t)((uint32_t)comma + 1));
        fs98_views_assign_temporary(&state, 0);
        s_services->send_message(D32(object, 0x25cc), 0x143, 0, D32(state.strings[3], 0));
        comma = s_services->string_find_char(state.strings[0], ',');
    }
    s_services->string_assign(state.strings[3], state.strings[0]);
    s_services->string_append_text(state.strings[3], "  ");
    s_services->string_append(state.strings[3], state.strings[2]);
    s_services->send_message(D32(object, 0x25cc), 0x143, 0, D32(state.strings[3], 0));
    record = fs98_views_record(object);
    s_services->send_message(D32(object, 0x25cc), 0x14e, D32(record, 0xac), 0);
    fs98_views_strings_end(&state);
}

/* 0x223e22b0: map keeps the 3D mode word, and sets a separate record flag. */
void FS98_VIEWS_THISCALL fs98_views_select(void *object)
{
    uint32_t selected, index, mode, radio;
    void *record;
    selected = (uint32_t)s_services->send_message(D32(object, 0x1494), 0x147, 0, 0);
    D32(PTR(object, 0x2804), 0x14) = selected;
    record = fs98_views_record(object);
    s_services->set_checked((uint8_t *)object + 0x1c94, (int32_t)D32(record, 0x70));
    mode = D32(record, 4) == 1 ? 5 : D32(record, 8);
    if (D32(record, 4) != 1 && mode > 4) return;
    radio = mode == 3 ? 4 : mode == 4 ? 3 : mode;
    if (mode <= 5) {
        for (index = 0; index < 6; index++)
            s_services->set_radio((uint8_t *)object + 0x1570 + index * 0xf4, index == radio);
        switch (mode) {
        case 0: fs98_views_cockpit(object); break;
        case 1: fs98_views_virtual_cockpit(object); break;
        case 2: fs98_views_tower(object); break;
        case 3: fs98_views_track(object); break;
        case 4: fs98_views_spot(object); break;
        case 5: fs98_views_map(object); break;
        }
    }
}

/* 0x223e3830: sscanf's result is deliberately ignored as retail does.
 * Malformed text therefore leaves an uninitialized single-precision value. */
FS98_VIEWS_NOINLINE void fs98_views_zoom_body(void *object, uint32_t legacy_bits)
{
    struct fs98_views_strings state;
    union { uint32_t bits; float value; } parsed;
    uint32_t position;
    parsed.bits = legacy_bits;
    fs98_views_strings_begin(&state, 1);
    s_services->get_text_string((uint8_t *)object + 0x1e14, state.strings[0]);
    s_services->scan_float(PTR(state.strings[0], 0), &parsed.value);
    position = (uint32_t)s_services->float_to_integer(((long double)parsed.value - s_zoom_offset) * s_zoom_scale);
    s_services->send_message(D32(object, 0x27dc), 0x467, 0, position & 0xffff);
    fs98_views_strings_end(&state);
}

/* 0x223e38e0: keep two fractional characters beyond the first decimal place.
 * The position is zero-extended to 64 bits before conversion to double. */
FS98_VIEWS_NOINLINE void FS98_VIEWS_THISCALL fs98_views_spin(void *object, uint32_t code,
                                                         uint32_t position, void *scrollbar)
{
    typedef int32_t (FS98_VIEWS_THISCALL *validate_fn)(void *self, uint32_t argument);
    struct fs98_views_strings state;
    int32_t dot;
    (void)code; (void)scrollbar;
    fs98_views_strings_begin(&state, 2);
    s_services->get_text_string((uint8_t *)object + 0x1e14, state.strings[0]);
    if (s_services->string_find_text(state.strings[0], ".") > 0) {
        dot = s_services->string_find_text(state.strings[0], ".");
        s_services->string_mid(state.strings[0], state.strings[4], (int32_t)((uint32_t)dot + 2), 2);
        fs98_views_assign_temporary(&state, 1);
    }
    if (s_services->compare_text(PTR(state.strings[1], 0), "") == 0)
        s_services->string_assign_text(state.strings[1], "0");
    s_services->string_format(state.strings[0], "%05.1f", (double)((long double)(uint64_t)position * s_position_scale));
    s_services->string_append(state.strings[0], state.strings[1]);
    s_services->set_text((uint8_t *)object + 0x1e14, PTR(state.strings[0], 0));
    ((validate_fn *)PTR(object, 0))[0xd8 / 4](object, 0);
    fs98_views_strings_end(&state);
}

/* 0x223e1f40: an empty list disables controls without querying selection.
 * A negative (nonzero) count skips only the string-add loop in retail. */
void FS98_VIEWS_THISCALL fs98_views_populate(void *object)
{
    void *descriptor = PTR(object, 0x2804);
    uint32_t index;
    if (D32(descriptor, 0x18) == 0) {
        void *preview;
        for (index = 0; index < sizeof(s_empty_controls) / sizeof(s_empty_controls[0]); index++) {
            const uint16_t *state = s_empty_controls[index];
            s_services->enable_control((uint8_t *)object + state[0], state[1]);
            if (state[2] != 0xffff) s_services->show_control((uint8_t *)object + state[0], state[2]);
        }
        preview = s_services->get_control(object, 0x188e);
        s_services->enable_control(preview, 0);
        s_services->show_control(preview, 0);
        s_services->show_control((uint8_t *)object + 0x1bdc, 0);
    } else {
        s_services->send_message(D32(object, 0x1494), 0x14b, 0, 0);
        index = 0;
        descriptor = PTR(object, 0x2804);
        while ((int32_t)index < (int32_t)D32(descriptor, 0x18)) {
            void *record = (uint8_t *)PTR(descriptor, 0x1c) + index * (uint32_t)0xb4;
            s_services->send_message(D32(object, 0x1494), 0x143, 0, D32(record, 0));
            index++;
            descriptor = PTR(object, 0x2804);
        }
        descriptor = PTR(object, 0x2804);
        s_services->send_message(D32(object, 0x1494), 0x14e, D32(descriptor, 0x14), 0);
        fs98_views_select(object);
    }
    descriptor = PTR(object, 0x2804);
    s_services->send_message(D32(object, 0x26cc), 0x14e, D32(descriptor, 0x20), 0);
    descriptor = PTR(object, 0x2804);
    s_services->set_checked((uint8_t *)object + 0x26f4, (int32_t)D32(descriptor, 0x24));
}

void fs98_views_set_resource_module(uint32_t module) { s_resource_module = module; }

static void fs98_views_init_check(void *object, uint32_t offset, uint32_t font_offset, uint32_t bitmap)
{
    typedef int32_t (FS98_VIEWS_THISCALL *init_fn)(void *self, void *font, uint32_t bitmap,
                                                 void *dib, void *palette, uint32_t module);
    uint8_t *control = (uint8_t *)object + offset;
    ((init_fn *)PTR(control, 0))[0xc4 / 4](control, (uint8_t *)object + font_offset, bitmap,
                                        (uint8_t *)object + 0xff4, (uint8_t *)object + 0x7e8, s_resource_module);
}
static void fs98_views_move_rect(void *control, struct fs98_views_rect rect)
{
    int32_t width = (int32_t)((uint32_t)rect.right - (uint32_t)rect.left);
    int32_t height = (int32_t)((uint32_t)rect.bottom - (uint32_t)rect.top);
    s_services->move_control(control, rect.left, rect.top, width, height, 1);
}

/* 0x223e1990: the SIMSUI skin, preview lifecycle, edits and spin control.
 * The preview callback is fetched before GetDlgItem; its descriptor argument
 * is reloaded afterwards, preserving possible reentry at that boundary. */
int32_t FS98_VIEWS_THISCALL fs98_views_init(void *object)
{
    typedef int32_t (FS98_VIEWS_THISCALL *init_fn)(void *self, int32_t id, uint32_t module, uint32_t flags);
    uint32_t index, window;
    void *preview, *descriptor, *font;
    fs98_views_preview_fn callback;
    struct fs98_views_rect rect, zero = {0, 0, 0, 0};
    s_services->base_init(object);
    if (!((init_fn *)PTR(object, 0))[0xe4 / 4](object, 0x14f, s_resource_module, 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    s_services->init_art((uint8_t *)object + 0x10dc, 0xf831, (uint8_t *)object + 0xff4, (uint8_t *)object + 0x7e8, 2);
    s_services->init_button((uint8_t *)object + 0x11c4, (uint8_t *)object + 0x5a0, (uint8_t *)object + 0x610,
                            0x132, (uint8_t *)object + 0xff4, (uint8_t *)object + 0x7e8, 1, 0xffffffff,
                            s_resource_module, 1, 1, zero);
    s_services->init_button((uint8_t *)object + 0x12c0, (uint8_t *)object + 0x5a0, (uint8_t *)object + 0x610,
                            0x132, (uint8_t *)object + 0xff4, (uint8_t *)object + 0x7e8, 1, 0xffffffff,
                            s_resource_module, 1, 1, zero);
#define VIEWS_TEXT(offset, font_offset) s_services->init_text((uint8_t *)object + (offset), (uint8_t *)object + (font_offset))
#define VIEWS_COMBO(offset, label_offset) s_services->init_combo((uint8_t *)object + (offset), (uint8_t *)object + 0x3e0, (uint8_t *)object + (label_offset))
    VIEWS_TEXT(0x13bc, 0x370);
    VIEWS_COMBO(0x1474, 0x13bc);
    s_services->init_group((uint8_t *)object + 0x14bc, (uint8_t *)object + 0x220);
    for (index = 0; index < 6; index++) {
        fs98_views_init_check(object, 0x1570 + index * 0xf4, 0x530, 0x134);
        if (index == 4 && !D32(PTR(object, 0x2804), 0xc))
            s_services->enable_control((uint8_t *)object + 0x1940, 0);
    }
    s_services->init_group((uint8_t *)object + 0x1b28, (uint8_t *)object + 0x220);
    preview = s_services->get_control(object, 0x188e);
    s_services->remap_rect(&rect, preview);
    fs98_views_move_rect(preview, rect);
    descriptor = PTR(object, 0x2804);
    callback = PTR(descriptor, 8);
    window = s_services->get_dlg_item(D32(object, 0x20), 0x188e);
    callback(1, PTR(object, 0x2804), window);
    VIEWS_TEXT(0x1bdc, 0x220);
    VIEWS_TEXT(0x1d5c, 0x220);
    font = s_services->base_font(2);
    s_services->init_edit((uint8_t *)object + 0x1e14, font, 0xffffff, 0, 0xff0000, (uint8_t *)object + 0x1d5c, 5);
    s_services->send_message(D32(object, 0x1e34), 0xc5, 6, 0);
    s_services->remap_rect(&rect, (uint8_t *)object + 0x27bc);
    fs98_views_move_rect((uint8_t *)object + 0x27bc, rect);
    D32(object, 0x2800) = 1;
    D32(object, 0x27fc) = 0;
    s_services->send_message(D32(object, 0x27dc), 0x46b, 1, (uint32_t)((uint8_t *)object + 0x27fc));
    s_services->send_message(D32(object, 0x27dc), 0x465, 0, 0x313ff);
    fs98_views_init_check(object, 0x1c94, 0x4c0, 0x139);
    VIEWS_TEXT(0x1e7c, 0x370);
    VIEWS_COMBO(0x1f34, 0x1e7c);
    VIEWS_TEXT(0x1f7c, 0x220);
    font = s_services->base_font(2);
    s_services->init_edit((uint8_t *)object + 0x2034, font, 0xffffff, 0, 0xff0000, (uint8_t *)object + 0x1f7c, 1);
    s_services->send_message(D32(object, 0x2054), 0xc5, 5, 0);
    VIEWS_TEXT(0x209c, 0x220);
    VIEWS_TEXT(0x2154, 0x220);
    font = s_services->base_font(2);
    s_services->init_edit((uint8_t *)object + 0x220c, font, 0xffffff, 0, 0xff0000, (uint8_t *)object + 0x2154, 3);
    s_services->send_message(D32(object, 0x222c), 0xc5, 5, 0);
    VIEWS_TEXT(0x2274, 0x220);
    VIEWS_TEXT(0x232c, 0x370);
    VIEWS_COMBO(0x23e4, 0x232c);
    fs98_views_init_check(object, 0x242c, 0x4c0, 0x139);
    VIEWS_TEXT(0x24f4, 0x370);
    VIEWS_COMBO(0x25ac, 0x24f4);
    VIEWS_TEXT(0x25f4, 0x370);
    VIEWS_COMBO(0x26ac, 0x25f4);
    fs98_views_init_check(object, 0x26f4, 0x4c0, 0x139);
#undef VIEWS_COMBO
#undef VIEWS_TEXT
    fs98_views_populate(object);
    return 1;
}
