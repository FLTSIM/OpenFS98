#include "sim_internal.h"
#include "sim_tables.inc"

#define D32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))
#define PTR(p, o) (*(void **)((uint8_t *)(p) + (o)))
#define AT(p, o) ((uint8_t *)(p) + (o))
#define TC FS98_SIM_THISCALL

typedef int (FS98_SIM_STDCALL *fuel_convert_fn)(void *helper, uint32_t row);

void fs98_sim_fuel_populate(void *object)
{
    char *caption, *suffix, *heading, *other_heading, *item;
    uint32_t length, row, index, field;
    int metric = D32(PTR(object, 0x1124), 0xc) != 0;
    g_sim_services->host_string(metric ? 0xf7b6 : 0xf7b5, &caption);
    g_sim_services->host_string(metric ? 0xf7bc : 0xf7b7, &suffix);
    g_sim_services->host_string(metric ? 0xf7bc : 0xf7b8, &heading);
    g_sim_services->host_string(metric ? 0xf7ba : 0xf7bf, &other_heading);
    length = g_sim_services->host_length(PTR(PTR(object, 0x1120), 0x14));
    length += g_sim_services->host_length(caption);
    g_sim_services->host_realloc(1, length + 1, (void **)&caption);
    g_sim_services->string_cat(caption, PTR(PTR(object, 0x1120), 0x14));
    g_sim_services->text(AT(object, 0x1b84), caption);
    g_sim_services->text(AT(object, 0x1734), heading);
    g_sim_services->text(AT(object, 0x15c4), other_heading);
    g_sim_services->host_free((void **)&caption);
    g_sim_services->host_free((void **)&heading);
    g_sim_services->host_free((void **)&other_heading);
    g_sim_services->host_string(0xf7be, &item);
    g_sim_services->send(D32(object, 0x2830), 0x143, 0, (uint32_t)item);
    g_sim_services->host_free((void **)&item);
    D32(object, 0x28f8) = 1;
    g_sim_services->host_string(0xf7c9, &item);
    g_sim_services->send(D32(object, 0x2830), 0x143, 0, (uint32_t)item);
    g_sim_services->host_free((void **)&item);
    D32(object, 0x28fc) = 1;
    for (row = 0; row < 5; ++row) {
        g_sim_services->control_text(PTR(object, 0x2858 + row * 4), PTR(PTR(object, 0x1120), 0x80 + row * 0x14));
        g_sim_services->control_text(PTR(object, 0x286c + row * 4), PTR(PTR(object, 0x1120), 0xe4 + row * 0x14));
        g_sim_services->control_text(PTR(object, 0x2880 + row * 4), PTR(PTR(object, 0x1120), 0x1c + row * 0x14));
        g_sim_services->send(D32(PTR(object, 0x2858 + row * 4), 0x20), 0xc5, D32(PTR(object, 0x1120), 0x84 + row * 0x14) - 1, 0);
        g_sim_services->send(D32(PTR(object, 0x286c + row * 4), 0x20), 0xc5, D32(PTR(object, 0x1120), 0xe8 + row * 0x14) - 1, 0);
        g_sim_services->send(D32(PTR(object, 0x2880 + row * 4), 0x20), 0xc5, D32(PTR(object, 0x1120), 0x20 + row * 0x14) - 1, 0);
        field = 0x144 + row * 4;
        length = g_sim_services->host_length(PTR(PTR(object, 0x1120), field));
        length += g_sim_services->host_length(suffix);
        g_sim_services->host_alloc(1, length + 1, (void **)&caption, 0);
        g_sim_services->string_copy(caption, PTR(PTR(object, 0x1120), field));
        g_sim_services->string_cat(caption, suffix);
        g_sim_services->text(PTR(object, 0x2894 + row * 4), caption);
        g_sim_services->host_free((void **)&caption);
        if (!g_sim_services->parse_long(PTR(PTR(object, 0x1120), field))) {
            for (index = 0; index < 5; ++index)
                g_sim_services->enable_control(PTR(object, 0x2858 + index * 0x14 + row * 4), 0);
            D32(object, 0x28f8 + D32(object, 0x2918 + row * 4) * 4) = 0;
        } else {
            D32(object, 0x28f8 + D32(object, 0x2918 + row * 4) * 4) = 1;
        }
    }
    for (index = 2; index <= 6; ++index) {
        if (D32(object, 0x28f8 + index * 4) == 1) {
            g_sim_services->host_string(0xf7c8 + index, &item);
            g_sim_services->send(D32(object, 0x2830), 0x143, 0, (uint32_t)item);
            g_sim_services->host_free((void **)&item);
        }
    }
    g_sim_services->send(D32(object, 0x2830), 0x14e, D32(PTR(object, 0x1120), 0x10), 0);
    g_sim_services->check(AT(object, 0x113c), D32(PTR(object, 0x1120), 8));
    fs98_sim_fuel_manual(object);
    g_sim_services->host_free((void **)&suffix);
}

void fs98_sim_fuel_init(void *object)
{
    static const uint16_t s_labels[] = {0x1b84, 0x2088, 0x1acc, 0x1a14, 0x195c, 0x18a4, 0x17ec, 0x1734, 0x167c, 0x15c4};
    static const uint16_t s_edit_labels[] = {0x2140, 0x1fd0, 0x1f18, 0x1e60, 0x1da8};
    static const uint16_t s_pointer_groups[8][5] = {
        {0x27a8, 0x2740, 0x26d8, 0x2670, 0x2608},
        {0x2398, 0x2330, 0x22c8, 0x2260, 0x21f8},
        {0x25a0, 0x2538, 0x24d0, 0x2468, 0x2400},
        {0x1acc, 0x1a14, 0x195c, 0x18a4, 0x17ec},
        {0x2140, 0x1fd0, 0x1f18, 0x1e60, 0x1da8},
        {0x1584, 0x1544, 0x1484, 0x1504, 0x14c4},
        {0x1444, 0x1404, 0x13c4, 0x1384, 0x1344},
        {0x1304, 0x12c4, 0x1284, 0x1244, 0x1204}
    };
    static const uint32_t s_tank_indices[] = {4, 2, 3, 5, 6};
    uint32_t index, group, offset;
    void *font;
    g_sim_services->init_group(AT(object, 0x1cf4), AT(object, 0x760));
    for (index = 0; index < 10; ++index)
        g_sim_services->init_text(AT(object, s_labels[index]), AT(object, 0x220));
    g_sim_services->init_text(AT(object, 0x1c3c), AT(object, 0x370));
    g_sim_services->init_combo(AT(object, 0x2810), AT(object, 0x3e0), AT(object, 0x1c3c));
    fs98_sim_init_page_button(object, 0x113c, 0x4c0, 0x139);
    for (index = 0; index < 5; ++index) {
        g_sim_services->init_text(AT(object, s_edit_labels[index]), AT(object, 0x220));
        font = g_sim_services->base_font(2);
        offset = 0x25a0 - index * 0x68;
        /* The last edit deliberately shares the preceding edit's label. */
        g_sim_services->init_edit(AT(object, offset), font, 0xffffff, 0, 0xff0000,
            AT(object, s_edit_labels[index == 4 ? 3 : index]), 0x11);
        D32(object, offset + 0x64) = 3;
    }
    for (index = 0; index < 5; ++index) {
        font = g_sim_services->base_font(2);
        g_sim_services->init_edit(AT(object, 0x27a8 - index * 0x68), font, 0xffffff, 0, 0xff0000, 0, 0x11);
    }
    for (index = 0; index < 5; ++index)
        D32(object, 0x280c - index * 0x68) = g_sim_services->string_length(PTR(PTR(object, 0x1120), 0x144 + index * 4));
    for (index = 0; index < 5; ++index) {
        font = g_sim_services->base_font(2);
        g_sim_services->init_edit(AT(object, 0x2398 - index * 0x68), font, 0xffffff, 0, 0xff0000, 0, 1);
    }
    for (index = 0; index < 5; ++index)
        D32(object, 0x23fc - index * 0x68) = D32(PTR(object, 0x1120), 0xe8 + index * 0x14) - 1;
    for (group = 0; group < 8; ++group)
        for (index = 0; index < 5; ++index)
            PTR(object, 0x2858 + group * 0x14 + index * 4) = AT(object, s_pointer_groups[group][index]);
    for (index = 0; index < 5; ++index)
        D32(object, 0x2918 + index * 4) = s_tank_indices[index];
    PTR(object, 0x1130) = fs98_sim_make_notify();
    fs98_sim_fuel_populate(object);
}

int fs98_sim_fuel_validate(void *object, uint32_t control_id)
{
    static const uint32_t s_controls[] = {0x2880, 0x2858, 0x286c};
    static const uint32_t s_buffers[] = {0x1c, 0x80, 0xe4};
    void *control, *descriptor, *helper;
    uint32_t row, group, mode;
    int result;

    if (!D32(object, 0x1100) || !control_id)
        return 0;
    control = g_sim_services->get_control(object, control_id);
    if (control && g_sim_services->is_edit(control)) {
        if (!g_sim_services->send(D32(control, 0x20), 0xb8, 0, 0))
            return 0;
    }
    result = g_sim_services->send(D32(object, 0x2830), 0x147, 0, 0);
    D32(PTR(object, 0x1120), 0x10) = result;
    for (group = 0; group < 3; ++group) {
        for (row = 0; row < 5; ++row)
            if (s_fuel_edits[group][row] == control_id)
                goto edit_found;
    }
    D32(PTR(object, 0x1124), 0x20) = 0;
    return 0;

edit_found:
    D32(PTR(object, 0x1124), 0x20) = group + 1;
    D32(PTR(object, 0x1124), 0x24) = row;
    /* Retail updates this cache after GetWindowText only for these two edits. */
    if (group != 1 || row < 3)
        D32(object, 0x111c) = row;
    descriptor = PTR(object, 0x1120);
    g_sim_services->get_text(PTR(object, s_controls[group] + row * 4),
        PTR(descriptor, s_buffers[group] + row * 0x14),
        D32(descriptor, s_buffers[group] + row * 0x14 + 4));
    if (group == 1 && row >= 3)
        D32(object, 0x111c) = row;
    helper = PTR(object, 0x1124);
    result = ((fs98_sim_settings_fn)PTR(helper, 4))(helper);
    if (result) {
        for (row = 0; row < 5; ++row) {
            if (!g_sim_services->is_enabled(PTR(object, 0x2858 + row * 4)))
                continue;
            descriptor = AT(PTR(object, 0x1120), row * 0x14);
            if (D32(descriptor, 0x7c))
                group = 1;
            else if (D32(descriptor, 0x18))
                group = 0;
            else if (D32(descriptor, 0xe0))
                group = 2;
            else
                continue;
            return g_sim_services->window_control_id(D32(PTR(object, s_controls[group] + row * 4), 0x20));
        }
        return control_id;
    }
    helper = PTR(object, 0x1124);
    if (!D32(helper, 0x20))
        return 0;
    if (control && g_sim_services->is_edit(control))
        g_sim_services->send(D32(control, 0x20), 0xb9, 0, 0);
    helper = PTR(object, 0x1124);
    row = D32(helper, 0x24);
    if (row > 4) {
        D32(helper, 0x20) = 0;
        return control_id;
    }
    D32(object, 0x111c) = row;
    mode = D32(helper, 0x20);
    descriptor = PTR(object, 0x1120);
    ((fuel_convert_fn)PTR(descriptor, mode == 2 ? 0x15c : mode == 3 ? 0x160 : 0x158))
        (helper, D32(object, 0x111c));
    /* The converter can replace both descriptors and the selected row. */
    row = D32(PTR(object, 0x1124), 0x24);
    group = mode == 3 ? 1 : 2;
    g_sim_services->control_text(PTR(object, s_controls[group] + row * 4),
        PTR(PTR(object, 0x1120), s_buffers[group] + row * 0x14));
    row = D32(PTR(object, 0x1124), 0x24);
    group = mode == 2 || mode == 3 ? 0 : 1;
    g_sim_services->control_text(PTR(object, s_controls[group] + row * 4),
        PTR(PTR(object, 0x1120), s_buffers[group] + row * 0x14));
    D32(PTR(object, 0x1124), 0x20) = 0;
    return 0;
}

int fs98_sim_fuel_selection(void *object, int selection)
{
    int index, available = 0;
    for (index = 0; index < 8; ++index) {
        if (D32(object, 0x28f8 + index * 4) == 1) {
            if (selection == available)
                return index;
            ++available;
        }
    }
    return 8;
}

void fs98_sim_fuel_selector_changed(void *object)
{
    int selection = g_sim_services->send(D32(object, 0x2830), 0x147, 0, 0);
    int mapped = fs98_sim_fuel_selection(object, selection);
    D32(PTR(object, 0x1120), 0x10) = mapped;
}

void fs98_sim_fuel_manual(void *object)
{
    D32(PTR(object, 0x1120), 8) = D32(object, 0x11f8);
    g_sim_services->enable_control(AT(object, 0x2810), D32(PTR(object, 0x1120), 8));
    g_sim_services->enable_control(AT(object, 0x1c3c), D32(PTR(object, 0x1120), 8));
}

int fs98_sim_fuel_error(void *object, int display)
{
    int row, result;
    void *descriptor, *control, *error;
    if (!display)
        return 0;
    for (row = 0; row < 5; ++row) {
        descriptor = AT(PTR(object, 0x1120), row * 0x14);
        if (D32(descriptor, 0x7c)) {
            control = PTR(object, 0x28d0 + row * 4);
            error = AT(descriptor, 0x7c);
        } else if (D32(descriptor, 0x18)) {
            control = PTR(object, 0x28bc + row * 4);
            error = AT(descriptor, 0x18);
        } else if (D32(descriptor, 0xe0)) {
            control = PTR(object, 0x28e4 + row * 4);
            error = AT(descriptor, 0xe0);
        } else {
            continue;
        }
        result = g_sim_services->error_dialog(object, control, error);
        if (result)
            return ((int (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, display);
    }
    return ((int (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, 0);
}
