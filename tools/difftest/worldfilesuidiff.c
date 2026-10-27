/* Native world-files acceptance, including nested World dialogs and owner data. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
#include <stdarg.h>
static uint8_t s_list_descriptor[0x30], s_list_rows[8][0x40], s_candidate[0x80];
static char s_list_titles[8][160], s_list_paths[8][288], s_candidate_title[160], s_candidate_path[288];
static uint8_t s_child_descriptor[0x84];
static char s_list_names[8][128], s_list_descriptions[8][288], s_candidate_name[128], s_candidate_description[288];
static struct world_ui_linkage *s_list_linkage;
static HWND s_list_window;
static uint32_t s_list_step, s_list_cycle, s_action_counts[7];
static uint8_t *s_nested_pixels[2];
static uint32_t s_nested_lengths[2], s_nested_count;
static char s_nested_controls[2][8192];
static void list_log(const char *format, ...)
{
    va_list args; int length;
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    s_log_length += (uint32_t)length;
}
static void list_owner_pointer(void *entry)
{
    HMODULE owner;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (const char *)entry, &owner) || owner != s_current) { fprintf(stderr, "list dispatch ownership failure\n"); { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); } }
}
static HWND list_open(uint32_t id, void *descriptor)
{
    void *factory = (void *)world_factory(s_list_linkage, id), *object, *map;
    void **vtable;
    uint32_t index;
    HWND window;
    const uint32_t slots[] = {4, 0x30, 0x8c, 0xc4, 0xd0};
    list_owner_pointer(factory);
    window = (HWND)((uint32_t (SC *)(uint32_t, void *))factory)(0, descriptor);
    if (!IsWindow(window)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    world_pump(); object = world_object(window); vtable = PTR(object, 0);
    for (index = 0; index < sizeof(slots) / sizeof(slots[0]); ++index) list_owner_pointer(vtable[slots[index] / 4]);
    if (id != 0x13080002) list_owner_pointer(vtable[0xcc / 4]);
    if (id == 0x1306) list_owner_pointer(vtable[0xac / 4]);
    map = ((void *(TC *)(void *))vtable[0x30 / 4])(object);
    for (index = 0; index < 64 && U32(PTR(map, 4), index * 24); ++index) list_owner_pointer(PTR(PTR(map, 4), index * 24 + 20));
    if (index == 64 || (id == 0x1306 && index != 6)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    return window;
}
static void list_select(HWND window, uint32_t selected)
{
    HWND list = GetDlgItem(window, 0x1b11);
    SendMessageA(list, LB_SETCURSEL, selected, 0);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x1b11, LBN_SELCHANGE), (LPARAM)list);
}
static void list_command(HWND window, uint32_t id)
{ SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(window, id)); }
static void list_capture_child(HWND window)
{
    uint32_t previous = s_nested_lengths[s_side], length;
    s_length = 0; s_snapshots[s_side][0] = 0;
    EnumChildWindows(window, world_snapshot, (LPARAM)window);
    if (strlen(s_nested_controls[s_side]) + s_length >= sizeof(s_nested_controls[0])) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    strcat(s_nested_controls[s_side], s_snapshots[s_side]);
    BringWindowToTop(window); SetForegroundWindow(window); SetFocus(NULL);
    RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW); world_pump(); world_pixels(window);
    length = s_pixel_lengths[s_side];
    s_nested_pixels[s_side] = realloc(s_nested_pixels[s_side], previous + 8 + length);
    if (!s_nested_pixels[s_side]) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    U32(s_nested_pixels[s_side], previous) = s_pixel_widths[s_side];
    U32(s_nested_pixels[s_side], previous + 4) = s_pixel_heights[s_side];
    memcpy(s_nested_pixels[s_side] + previous + 8, s_pixels[s_side], length);
    s_nested_lengths[s_side] = previous + 8 + length; ++s_nested_count;
    if (!s_side) ++s_checks;
}
static void list_close_child(HWND window, uint32_t command)
{
    SendMessageA(window, WM_COMMAND, command, 0);
    if (!IsWindow(window) || IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA) != 1) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    DestroyWindow(window); world_pump();
    EnableWindow(s_list_window, TRUE); SetForegroundWindow(s_list_window); SetActiveWindow(s_list_window);
}
static uint8_t s_nested_rows[8][0x80];
static char s_nested_titles[8][160], s_nested_paths[8][288], s_nested_names[8][128], s_nested_descriptions[8][288];
static uint32_t s_child_kind;
static void list_delete_probe(void *object);
static uint32_t list_child_delete_count(void);
static uint32_t SC list_child_noop(void *descriptor)
{ (void)descriptor; list_log("owner check\n"); return s_case & 1 ? 0 : 0xffffffff; }
static uint32_t child_action(void *descriptor, uint32_t slot)
{
    list_log("nested %x action=%x selection=%u\n", s_child_kind, slot, U32(descriptor, s_child_kind == 1 ? 0x28 : 0x20));
    if (s_child_kind == 1 && slot == 8) U32(descriptor, 0x20) = 4;
    if (s_child_kind == 6 && slot == 0x14) {
        uint32_t index;
        for (index = 0; index < 3; ++index) U32(s_nested_rows[index], 0x54) = 0;
    }
    return 0xffffffff;
}
#define CHILD_CALLBACK(name, slot) static uint32_t SC name(void *descriptor) { return child_action(descriptor, slot); }
CHILD_CALLBACK(child_add, 8)
CHILD_CALLBACK(child_edit, 0xc)
CHILD_CALLBACK(child_delete, 0x10)
CHILD_CALLBACK(child_flush, 0x14)
CHILD_CALLBACK(child_priority, 0x18)
static void files_nested(uint32_t kind)
{
    uint32_t row; HWND window;
    s_child_kind = kind;
    memset(s_child_descriptor, 0, sizeof(s_child_descriptor)); memset(s_nested_rows, 0, sizeof(s_nested_rows));
    memset(s_nested_titles, 0, sizeof(s_nested_titles)); memset(s_nested_paths, 0, sizeof(s_nested_paths));
    memset(s_nested_names, 0, sizeof(s_nested_names)); memset(s_nested_descriptions, 0, sizeof(s_nested_descriptions));
    PTR(s_child_descriptor, 4) = list_child_noop; PTR(s_child_descriptor, 8) = child_add;
    PTR(s_child_descriptor, 0xc) = child_edit; PTR(s_child_descriptor, 0x10) = child_delete;
    PTR(s_child_descriptor, 0x14) = child_flush;
    if (kind == 1) {
        PTR(s_child_descriptor, 0x18) = child_priority; PTR(s_child_descriptor, 0x1c) = "Nested scenery areas";
        U32(s_child_descriptor, 0x20) = 3; U32(s_child_descriptor, 0x28) = 2;
    } else {
        PTR(s_child_descriptor, 0x18) = "Nested performance boosters";
        U32(s_child_descriptor, 0x1c) = 3; U32(s_child_descriptor, 0x20) = 2;
    }
    PTR(s_child_descriptor, 0x24) = s_nested_rows;
    for (row = 0; row < 8; ++row) {
        uint8_t *data = (uint8_t *)s_nested_rows + row * (kind == 1 ? 0x40 : 0x80);
        snprintf(s_nested_titles[row], 160, "Nested area %u", row);
        snprintf(s_nested_paths[row], 288, "C:\\fixture\\nested%u", row);
        snprintf(s_nested_names[row], 128, "Nested booster %u", row);
        snprintf(s_nested_descriptions[row], 288, "Nested description %u", row);
        PTR(data, 4) = s_nested_paths[row]; PTR(data, 0x18) = s_nested_titles[row];
        if (kind == 1) { U32(data, 0x2c) = (row % 5 + 1) * 10; U32(data, 0x34) = row & 1; }
        else { PTR(data, 0x2c) = s_nested_names[row]; PTR(data, 0x40) = s_nested_descriptions[row]; U32(data, 0x54) = row & 1; }
    }
    EnableWindow(s_list_window, FALSE); window = list_open(0x13080000 | kind, s_child_descriptor);
    if (kind == 1) {
        HWND list = GetDlgItem(window, 0x1b16);
        SendMessageA(list, LB_SETCURSEL, 2, 0);
        SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x1b16, LBN_SELCHANGE), (LPARAM)list);
        list_command(window, 0x1aa3);
    } else list_command(window, 0x1aa0);
    list_capture_child(window); list_delete_probe(world_object(window)); list_close_child(window, IDOK);
    list_log("nested delete count=%u\n", list_child_delete_count());
    if (!s_side && (U32(s_child_descriptor, 0) != 1 || list_child_delete_count() != 1)) exit(2);
}
static uint32_t list_action(void *descriptor, uint32_t slot)
{
    ++s_callbacks; ++s_action_counts[slot / 4];
    list_log("owner %x count=%u selection=%u\n", slot, U32(descriptor, 0x10), U32(descriptor, 0x18));
    files_nested(slot == 8 ? 1 : 6);
    /* World must not implicitly refresh its list or details after this. */
    strcpy(s_list_titles[1], "Owner changed world title");
    strcpy(s_list_descriptions[1], "Owner changed world description");
    return 0xffffffff;
}
#define LIST_CALLBACK(name, slot) static uint32_t SC name(void *descriptor) { return list_action(descriptor, slot); }
LIST_CALLBACK(list_areas, 8)
LIST_CALLBACK(list_caches, 0xc)
static void list_fixture(void)
{
    uint32_t row, count = s_case % 7;
    memset(s_list_descriptor, 0, sizeof(s_list_descriptor)); memset(s_list_rows, 0, sizeof(s_list_rows));
    memset(s_list_titles, 0, sizeof(s_list_titles)); memset(s_list_paths, 0, sizeof(s_list_paths));
    memset(s_list_names, 0, sizeof(s_list_names)); memset(s_list_descriptions, 0, sizeof(s_list_descriptions));
    memset(s_candidate, 0, sizeof(s_candidate)); memset(s_candidate_title, 0, sizeof(s_candidate_title)); memset(s_candidate_path, 0, sizeof(s_candidate_path));
    memset(s_candidate_name, 0, sizeof(s_candidate_name)); memset(s_candidate_description, 0, sizeof(s_candidate_description));
    memset(s_child_descriptor, 0, sizeof(s_child_descriptor)); memset(s_nested_rows, 0, sizeof(s_nested_rows));
    memset(s_nested_titles, 0, sizeof(s_nested_titles)); memset(s_nested_paths, 0, sizeof(s_nested_paths));
    memset(s_nested_names, 0, sizeof(s_nested_names)); memset(s_nested_descriptions, 0, sizeof(s_nested_descriptions));
    U32(s_list_descriptor, 0) = 0x12345678;
    PTR(s_list_descriptor, 4) = list_child_noop; PTR(s_list_descriptor, 8) = list_areas; PTR(s_list_descriptor, 0xc) = list_caches;
    U32(s_list_descriptor, 0x10) = count; PTR(s_list_descriptor, 0x14) = s_list_rows[1];
    U32(s_list_descriptor, 0x18) = count ? count - 1 : 0xffffffff;
    for (row = 0; row < 8; ++row) {
        snprintf(s_list_titles[row], sizeof(s_list_titles[row]), "Fixture world %u", row);
        snprintf(s_list_paths[row], sizeof(s_list_paths[row]), "C:\\fixture\\world%u", row);
        snprintf(s_list_descriptions[row], sizeof(s_list_descriptions[row]), "World description %u", row);
        PTR(s_list_rows[row], 8) = s_list_paths[row]; PTR(s_list_rows[row], 0x1c) = s_list_titles[row];
        PTR(s_list_rows[row], 0x30) = s_list_descriptions[row]; U32(s_list_rows[row], 0) = (row + s_case / 7) % 4;
    }
    memset(s_action_counts, 0, sizeof(s_action_counts)); s_callbacks = s_log_length = s_nested_count = 0;
    s_logs[s_side][0] = s_nested_controls[s_side][0] = 0; s_nested_lengths[s_side] = 0;
    s_error_snapshots[s_side][0] = 0; s_error_lengths[s_side] = 0;
}
static void list_snapshot(HWND window)
{
    HWND list = GetDlgItem(window, 0x1b11); char text[512]; uint32_t row;
    int count = (int)SendMessageA(list, LB_GETCOUNT, 0, 0), length;
    if (count < 0 || count > 7) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    for (row = 0; row < (uint32_t)count; ++row) {
        if (SendMessageA(list, LB_GETTEXTLEN, row, 0) >= (LRESULT)sizeof(text)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
        SendMessageA(list, LB_GETTEXT, row, (LPARAM)text);
        length = snprintf(s_snapshots[s_side] + s_length, 8192 - s_length, "row %u [%s]\n", row, text);
        if (length < 0 || (uint32_t)length >= 8192 - s_length) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
        s_length += length;
    }
    length = snprintf(s_snapshots[s_side] + s_length, 8192 - s_length, "selection=%ld\n", (long)SendMessageA(list, LB_GETCURSEL, 0, 0));
    if (length < 0 || (uint32_t)length >= 8192 - s_length) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
        s_length += length;
    if (!s_side && count != (int)U32(s_list_descriptor, 0x10)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    ++s_checks;
}
static void list_script(HWND window, uint32_t step)
{
    uint32_t count = U32(s_list_descriptor, 0x10), selected = count ? count - 1 : 0xffffffff;
    HWND list = GetDlgItem(window, 0x1b11);
    if (count && step) list_select(window, selected);
    if (step == 3 && count) { list_select(window, 0); selected = 0; }
    if (step == 11 && count > 1) SendMessageA(list, LB_SETCURSEL, 0, 0);
    if (!s_side) {
        int enabled = U32(s_list_rows[selected + 1], 0) != 0;
        if (!!IsWindowEnabled(GetDlgItem(window, 0x1aab)) != enabled ||
            !!IsWindowEnabled(GetDlgItem(window, 0x1aac)) != enabled) exit(2);
        ++s_checks;
    }
    if (step == 4 && IsWindowEnabled(GetDlgItem(window, 0x1aab))) list_command(window, 0x1aab);
    if (step == 5 && IsWindowEnabled(GetDlgItem(window, 0x1aac))) list_command(window, 0x1aac);
    if (step == 6) { HDC dc = GetDC(window); SendMessageA(window, WM_ERASEBKGND, (WPARAM)dc, 0); ReleaseDC(window, dc); world_pump(); }
    if (step == 7 && count) SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x1b11, LBN_DBLCLK), (LPARAM)list);
    if ((step == 8 || step == 9) && count) {
        U32(s_list_descriptor, 0x18) = 0;
        list_command(window, step == 8 ? 0x1aab : 0x1aac);
    }
    if (step == 10 && count) {
        SendMessageA(list, LB_SETCURSEL, 0, 0);
        SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x1b11, LBN_SELCHANGE), (LPARAM)list);
    }
    if (!s_side) {
        if (step == 7 && s_callbacks) exit(2);
        if ((step == 8 || step == 9) && count && (s_callbacks != 1 || U32(s_list_descriptor, 0x18) != 0)) exit(2);
        ++s_checks;
    }
}
static void **s_delete_slot;
static void *s_delete_original;
static uint32_t s_delete_calls;
static uint32_t list_child_delete_count(void) { return s_delete_calls; }
static void list_delete_restore(void)
{
    DWORD old;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    *s_delete_slot = s_delete_original;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
}
static void *TC list_delete_hook(void *object, uint32_t flags)
{
    int length;
    ++s_delete_calls;
    length = snprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length, "post.destroy delete flags=%u\n", flags);
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    s_log_length += (uint32_t)length;
    list_delete_restore();
    return ((void *(TC *)(void *, uint32_t))s_delete_original)(object, flags);
}
static void list_delete_probe(void *object)
{
    DWORD old; void **vtable = PTR(object, 0);
    s_delete_slot = &vtable[1]; s_delete_original = *s_delete_slot; s_delete_calls = 0;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
    *s_delete_slot = list_delete_hook;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
}
struct list_ui_result {
    uint8_t descriptor[0x30], rows[8][0x40], candidate[0x80], child[0x84];
    char titles[8][160], paths[8][288], candidate_title[160], candidate_path[288], names[8][128], descriptions[8][288], candidate_name[128], candidate_description[288];
    uint8_t nested_rows[8][0x80];
    char nested_titles[8][160], nested_paths[8][288], nested_names[8][128], nested_descriptions[8][288];
    uint32_t callbacks, deletes, nested, object_result;
};
int main(int argc, char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 21;
    struct list_ui_result results[2];
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, NULL, _IONBF, 0); SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(world_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, NULL); SetDllDirectoryA(directory);
    s_mfc = world_load("cd/mfc42.dll"); world_application(); world_classes();
    ui = world_load("orig/SIMSUI.DLL"); s_fll = world_load("orig/ENGLISH.FLL");
    global_module = world_load("orig/Modules/GLOBAL.DLL"); memcpy(s_global_data, GetProcAddress(global_module, "Linkage"), 4764);
    for (index = 0; index < sizeof(s_host_services); index += 4) PTR(s_host_services, index) = world_unknown;
    PTR(s_host_services, 0x20c) = world_allocate; PTR(s_host_services, 0x210) = world_reallocate; PTR(s_host_services, 0x214) = world_free;
    PTR(s_host_services, 0x10c) = world_modal; PTR(s_host_services, 0x298) = world_register; PTR(s_host_services, 0x29c) = world_release; PTR(s_host_services, 0x2a0) = world_string;
    {
        struct world_ui_import *imports = (void *)GetProcAddress(ui, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global_data;
    }
    s_auxiliary = 1; s_alerts = world_load("orig/English/Alerts.DLL"); s_current = s_alerts;
    {
        struct world_ui_import *imports = (void *)GetProcAddress(s_alerts, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global_data;
        s_alert_linkage = (void *)GetProcAddress(s_alerts, "Linkage"); s_alert_linkage->init();
    }
    s_auxiliary = 0;

    for (cycle = 0; cycle < 2; ++cycle) {
        modules[0] = world_load("orig/English/World.DLL"); modules[1] = world_load(argc > 1 ? argv[1] : "bin/world-diff/WORLD_area.dll");
        if (modules[0] == modules[1]) return 2;
        for (s_side = 0; s_side < 2; ++s_side) {
            struct world_ui_import *imports = (void *)GetProcAddress(modules[s_side], "ImportTable");
            imports[0].link = s_host_services; imports[1].link = s_global_data;
            linkages[s_side] = (void *)GetProcAddress(modules[s_side], "Linkage");
            if (linkages[s_side]->id != 0x80000006) return 2;
        }
        world_module_probe(linkages, modules);
        for (s_side = 0; s_side < 2; ++s_side) { s_current = modules[s_side]; linkages[s_side]->init(); }
        for (s_case = 0; s_case < cases; ++s_case) for (step = 0; step < 12; ++step) {
            for (s_side = 0; s_side < 2; ++s_side) {
                HWND window; uint32_t count; void *object;
                s_list_step = step; s_list_cycle = cycle; s_current = modules[s_side]; s_list_linkage = linkages[s_side];
                list_fixture(); count = U32(s_list_descriptor, 0x10);
                window = list_open(0x1306, s_list_descriptor); s_list_window = window; object = world_object(window);
                if (!s_side && count && U32(s_list_descriptor, 0x18) != count - 1) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
                list_script(window, step);
                s_length = 0; s_snapshots[s_side][0] = 0; EnumChildWindows(window, world_snapshot, (LPARAM)window); list_snapshot(window);
                BringWindowToTop(window); SetForegroundWindow(window); SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW); world_pump(); world_pixels(window);
                if (!cycle && s_case == 6 && !step) world_save_pixels("files-baseline", cycle, step);
                if (step == 1 || step == 2) {
                    MSG key; memset(&key, 0, sizeof(key)); key.hwnd = window; key.message = WM_KEYDOWN;
                    key.wParam = step == 1 ? VK_RETURN : VK_ESCAPE; key.lParam = 1; IsDialogMessageA(window, &key);
                }
                if (!GetWindowLongA(window, GWL_USERDATA)) SendMessageA(window, WM_COMMAND, step == 4 || step == 8 || step == 11 ? IDOK : IDCANCEL, 0);
                if (!IsWindow(window) || IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA) != 1) return 2;
                if (!s_side && U32(s_list_descriptor, 0) != (step == 1 || step == 4 || step == 8 || step == 11 ? 1u : 0x12345678u)) { fprintf(stderr, "list fixture failed line=%u cycle=%u case=%u step=%u side=%u\n", __LINE__, s_list_cycle, s_case, s_list_step, s_side); exit(2); }
                memset(&results[s_side], 0, sizeof(results[0]));
                memcpy(results[s_side].descriptor, s_list_descriptor, sizeof(s_list_descriptor));
                memcpy(results[s_side].rows, s_list_rows, sizeof(s_list_rows)); memcpy(results[s_side].candidate, s_candidate, sizeof(s_candidate));
                memcpy(results[s_side].child, s_child_descriptor, sizeof(s_child_descriptor));
                memcpy(results[s_side].titles, s_list_titles, sizeof(s_list_titles)); memcpy(results[s_side].paths, s_list_paths, sizeof(s_list_paths));
                memcpy(results[s_side].candidate_title, s_candidate_title, sizeof(s_candidate_title)); memcpy(results[s_side].candidate_path, s_candidate_path, sizeof(s_candidate_path));
                memcpy(results[s_side].names, s_list_names, sizeof(s_list_names)); memcpy(results[s_side].descriptions, s_list_descriptions, sizeof(s_list_descriptions));
                memcpy(results[s_side].candidate_name, s_candidate_name, sizeof(s_candidate_name)); memcpy(results[s_side].candidate_description, s_candidate_description, sizeof(s_candidate_description));
                memcpy(results[s_side].nested_rows, s_nested_rows, sizeof(s_nested_rows));
                memcpy(results[s_side].nested_titles, s_nested_titles, sizeof(s_nested_titles)); memcpy(results[s_side].nested_paths, s_nested_paths, sizeof(s_nested_paths));
                memcpy(results[s_side].nested_names, s_nested_names, sizeof(s_nested_names)); memcpy(results[s_side].nested_descriptions, s_nested_descriptions, sizeof(s_nested_descriptions));
                results[s_side].object_result = U32(object, 0x1824);
                if (!s_side && step == 11 && count > 1 && U32(s_list_descriptor, 0x18) != count - 1) exit(2);
                results[s_side].callbacks = s_callbacks; results[s_side].nested = s_nested_count;
                list_delete_probe(object); DestroyWindow(window); world_pump(); list_delete_restore(); results[s_side].deletes = s_delete_calls;
                if (!s_side && s_delete_calls != 1) { fprintf(stderr, "world-files deletion witness missing\n"); return 2; }

            }
            if (memcmp(&results[0],&results[1],sizeof(results[0])) || strcmp(s_logs[0],s_logs[1]) || strcmp(s_snapshots[0],s_snapshots[1]) ||
                s_pixel_lengths[0]!=s_pixel_lengths[1] || memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0]) ||
                strcmp(s_nested_controls[0], s_nested_controls[1]) || s_nested_lengths[0] != s_nested_lengths[1] ||
                (s_nested_lengths[0] && memcmp(s_nested_pixels[0], s_nested_pixels[1], s_nested_lengths[0]))) {
                for(s_side=0;s_side<2;++s_side)world_save_pixels("files-difference",cycle,step);
                fprintf(stderr,"FILES UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",
                    cycle,s_case,step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),
                    s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5 || s_releases[s_side]!=(cycle+1)*5){fprintf(stderr,"LIST INIT DIFFER lifetime\n");return 1;}
            if(!FreeLibrary(modules[s_side]))return 2;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World world-files native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
