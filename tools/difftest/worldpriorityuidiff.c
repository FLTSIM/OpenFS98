/* Reuse the native application's host/modal/pixel boundary, not its scripts. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
static uint8_t s_priority_descriptor[0x40], s_priority_rows[6][0x40];
static const char *s_priority_titles[] = {"Zebra", "Alpha", "Delta", "Charlie", "Bravo", "Echo"};
static void priority_log(uint32_t callback, void *descriptor)
{
    int length = snprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length,
        "priority callback=%x count=%u selection=%u position=%u status=%u\n", callback,
        U32(descriptor, 0x1c), U32(descriptor, 0x20), U32(descriptor, 0x2c), U32(descriptor, 0x28));
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) exit(2);
    s_log_length += (uint32_t)length; ++s_callbacks;
}
static uint32_t SC priority_check(void *descriptor)
{
    U32(descriptor, 0x28) = U32(descriptor, 0x2c) < 1 ? 0xc :
        U32(descriptor, 0x2c) > U32(descriptor, 0x1c) ? 0xb : 0;
    priority_log(4, descriptor); return U32(descriptor, 0x28) ? 0xffffffff : 0;
}
static void priority_swap(uint8_t *left, uint8_t *right)
{ uint8_t row[0x40]; memcpy(row, left, 0x40); memcpy(left, right, 0x40); memcpy(right, row, 0x40); }
static uint32_t SC priority_up(void *descriptor)
{
    uint32_t selected = U32(descriptor, 0x20), count = U32(descriptor, 0x1c);
    if (selected < count && selected) { priority_swap(s_priority_rows[selected], s_priority_rows[selected - 1]); --U32(descriptor, 0x20); }
    priority_log(8, descriptor); return 0;
}
static uint32_t SC priority_down(void *descriptor)
{
    uint32_t selected = U32(descriptor, 0x20), count = U32(descriptor, 0x1c);
    if (selected < count && selected + 1 < count) { priority_swap(s_priority_rows[selected], s_priority_rows[selected + 1]); ++U32(descriptor, 0x20); }
    priority_log(0xc, descriptor); return 0;
}
static int priority_compare(const void *left, const void *right)
{
    uint32_t a = U32(left, 0x2c), b = U32(right, 0x2c);
    if (a != b) return a < b ? 1 : -1;
    return strcmp(PTR(left, 0x18), PTR(right, 0x18));
}
static uint32_t SC priority_sort(void *descriptor)
{ qsort(s_priority_rows, U32(descriptor, 0x1c), 0x40, priority_compare); priority_log(0x10, descriptor); return 0; }
static uint32_t SC priority_move(void *descriptor)
{
    uint32_t selected = U32(descriptor, 0x20), target = U32(descriptor, 0x2c) - 1, count = U32(descriptor, 0x1c);
    uint8_t row[0x40];
    if (selected < count && target < count) {
        memcpy(row, s_priority_rows[selected], 0x40);
        if (selected < target) memmove(s_priority_rows[selected], s_priority_rows[selected + 1], (target - selected) * 0x40);
        else if (target < selected) memmove(s_priority_rows[target + 1], s_priority_rows[target], (selected - target) * 0x40);
        memcpy(s_priority_rows[target], row, 0x40); U32(descriptor, 0x20) = target;
    }
    priority_log(0x14, descriptor); return 0;
}
static void priority_fixture(void)
{
    uint32_t row, count = s_case % 7;
    memset(s_priority_descriptor, 0, sizeof(s_priority_descriptor)); memset(s_priority_rows, 0, sizeof(s_priority_rows));
    PTR(s_priority_descriptor, 4) = priority_check; PTR(s_priority_descriptor, 8) = priority_up;
    PTR(s_priority_descriptor, 0xc) = priority_down; PTR(s_priority_descriptor, 0x10) = priority_sort; PTR(s_priority_descriptor, 0x14) = priority_move;
    PTR(s_priority_descriptor, 0x18) = "Priority fixture world"; U32(s_priority_descriptor, 0x1c) = count;
    U32(s_priority_descriptor, 0x20) = count ? s_case % count : 0;
    PTR(s_priority_descriptor, 0x24) = s_priority_rows;
    for (row = 0; row < count; ++row) {
        PTR(s_priority_rows[row], 0x18) = (void *)s_priority_titles[row];
        U32(s_priority_rows[row], 0x2c) = ((s_case + row) % 5 + 1) * 10;
        U32(s_priority_rows[row], 0x34) = (s_case + row) & 1;
        U32(s_priority_rows[row], 0x3c) = row == 0;
    }
    s_callbacks = s_log_length = 0; s_logs[s_side][0] = 0;
    s_error_snapshots[s_side][0] = 0; s_error_lengths[s_side] = 0;
}
static void priority_select(HWND window, uint32_t selected)
{
    HWND list = GetDlgItem(window, 0x1b12);
    SendMessageA(list, LB_SETCURSEL, selected, 0);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x1b12, LBN_SELCHANGE), (LPARAM)list);
}
static void priority_list_snapshot(HWND window)
{
    HWND list = GetDlgItem(window, 0x1b12);
    int count = (int)SendMessageA(list, LB_GETCOUNT, 0, 0), index, length;
    if (count < 0 || count > 6) exit(2);
    for (index = 0; index < count; ++index) {
        char text[512];
        if (SendMessageA(list, LB_GETTEXTLEN, index, 0) >= (LRESULT)sizeof(text)) exit(2);
        SendMessageA(list, LB_GETTEXT, index, (LPARAM)text);
        length = snprintf(s_snapshots[s_side] + s_length, 8192 - s_length, "row %d [%s]\n", index, text);
        if (length < 0 || (uint32_t)length >= 8192 - s_length) exit(2);
        s_length += (uint32_t)length;
    }
    if (count != (int)U32(s_priority_descriptor, 0x1c)) { fprintf(stderr, "priority list count witness failed\n"); exit(2); }
    ++s_checks;
}
static void **s_delete_slot;
static void *s_delete_original;
static uint32_t s_delete_calls;
static void priority_delete_restore(void)
{
    DWORD old;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) exit(2);
    *s_delete_slot = s_delete_original;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) exit(2);
}
static void *TC priority_delete_hook(void *object, uint32_t flags)
{
    int length;
    ++s_delete_calls;
    length = snprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length, "post.destroy delete flags=%u\n", flags);
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) exit(2);
    s_log_length += (uint32_t)length;
    priority_delete_restore();
    return ((void *(TC *)(void *, uint32_t))s_delete_original)(object, flags);
}
static void priority_delete_probe(void *object)
{
    DWORD old; void **vtable = PTR(object, 0);
    s_delete_slot = &vtable[1]; s_delete_original = *s_delete_slot; s_delete_calls = 0;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) exit(2);
    *s_delete_slot = priority_delete_hook;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) exit(2);
}
struct priority_ui_result { uint8_t descriptor[0x40], rows[6][0x40]; uint32_t callbacks, focus, deletes; };

int main(int argc, char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 21;
    struct priority_ui_result results[2];
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
        for (s_case = 0; s_case < cases; ++s_case) for (step = 0; step < 5; ++step) {
            for (s_side = 0; s_side < 2; ++s_side) {
                HWND window; uint32_t count, focus = 0; void *object; void **vtable;
                s_current = modules[s_side]; priority_fixture(); count = U32(s_priority_descriptor, 0x1c);
                window = (HWND)world_factory(linkages[s_side], 0x13080005)(0, s_priority_descriptor);
                if (!IsWindow(window)) return 2;
                world_pump(); world_ownership(window, modules[s_side]); object = world_object(window); vtable = PTR(object, 0);
                if (count && step >= 1) {
                    priority_select(window, count - 1);
                    SendMessageA(window, WM_COMMAND, 0x1aaa, (LPARAM)GetDlgItem(window, 0x1aaa));
                    if (step == 1) SendMessageA(window, WM_COMMAND, 0x1aa9, (LPARAM)GetDlgItem(window, 0x1aa9));
                    if (step >= 2) SendMessageA(window, WM_COMMAND, 0x1aa8, (LPARAM)GetDlgItem(window, 0x1aa8));
                    SetWindowTextA(GetDlgItem(window, 0x1b08), "001");
                    if (step >= 3) {
                        uint32_t callbacks = s_callbacks;
                        if (((int (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0) != 0 || s_callbacks < callbacks + 2) {
                            fprintf(stderr, "priority validate/move witness failed\n"); return 2;
                        }
                        ++s_checks;
                    }
                }
                if (step == 4) {
                    int invalid;
                    SetWindowTextA(GetDlgItem(window, 0x1b08), "999");
                    invalid = ((int (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
                    if (invalid != 0x1b08) { fprintf(stderr,"priority invalid witness failed\n"); return 2; }
                    s_error_owner = window; s_error_ticks = s_error_seen = 0; s_error_timer = SetTimer(NULL, 0, 10, world_error_timer);
                    ((int (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, (uint32_t)invalid);
                    if (s_error_timer) KillTimer(NULL, s_error_timer);
                    if (s_error_seen != 1 || !IsWindowVisible(window) || GetWindowLongA(window,GWL_USERDATA)) return 2;
                    focus = (uint32_t)GetDlgCtrlID(GetFocus());
                    SetWindowTextA(GetDlgItem(window, 0x1b08), "001"); ++s_checks;
                }
                s_length = 0; s_snapshots[s_side][0] = 0; EnumChildWindows(window, world_snapshot, (LPARAM)window); priority_list_snapshot(window);
                BringWindowToTop(window); SetForegroundWindow(window); SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW); world_pump(); world_pixels(window);
                if (!cycle && s_case == 6 && !step) world_save_pixels("priority-baseline", cycle, step);
                /* Empty lists and error-only scenarios close by Cancel. */
                SendMessageA(window, WM_COMMAND, count && (step == 2 || step == 3) ? IDOK : IDCANCEL, 0);
                if (!IsWindow(window) || IsWindowVisible(window) || GetWindowLongA(window,GWL_USERDATA)!=1) return 2;
                memset(&results[s_side],0,sizeof(results[0]));
                memcpy(results[s_side].descriptor,s_priority_descriptor,sizeof(s_priority_descriptor));
                PTR(results[s_side].descriptor,0x34)=NULL; /* expired modal-error stack address */
                memcpy(results[s_side].rows,s_priority_rows,sizeof(s_priority_rows)); results[s_side].callbacks=s_callbacks;results[s_side].focus=focus;
                priority_delete_probe(object);
                DestroyWindow(window);world_pump(); /* PostNcDestroy owns the object delete. */
                priority_delete_restore(); results[s_side].deletes = s_delete_calls;
                if (!s_side && s_delete_calls != 1) { fprintf(stderr, "priority retail destruction witness missing\n"); return 2; }
            }
            if (memcmp(&results[0],&results[1],sizeof(results[0])) || strcmp(s_logs[0],s_logs[1]) || strcmp(s_snapshots[0],s_snapshots[1]) ||
                s_pixel_lengths[0]!=s_pixel_lengths[1] || memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0]) ||
                strcmp(s_error_snapshots[0],s_error_snapshots[1]) || s_error_lengths[0]!=s_error_lengths[1] ||
                (s_error_lengths[0] && memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0]))) {
                for(s_side=0;s_side<2;++s_side)world_save_pixels("priority-difference",cycle,step);
                fprintf(stderr,"PRIORITY UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",
                    cycle,s_case,step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),
                    s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5 || s_releases[s_side]!=(cycle+1)*5){fprintf(stderr,"PRIORITY INIT DIFFER lifetime\n");return 1;}
            if(!FreeLibrary(modules[s_side]))return 2;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World priority native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
