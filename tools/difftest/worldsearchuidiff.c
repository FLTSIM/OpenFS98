/* Native drive-search comparison with independent FE-shaped callbacks. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
#include <stdarg.h>
static uint8_t s_search_descriptor[0x84];
static char s_search_path[288], s_search_title[288], s_drives[16];
static uint32_t s_search_error, s_action_counts[7], s_search_step;
static void search_log(const char *format, ...)
{
    va_list args; int length;
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
    s_log_length += (uint32_t)length;
}
static uint32_t search_callback(void *descriptor, uint32_t slot)
{
    ++s_callbacks; ++s_action_counts[slot / 4];
    search_log("callback %x path=[%s] title=[%s] layer=%u active=%u drive=%u position=%u\n", slot,
        (char *)PTR(descriptor, 0x24), (char *)PTR(descriptor, 0x38), U32(descriptor, 0x4c),
        U32(descriptor, 0x54), U32(descriptor, 0x6c), U32(descriptor, 0x64));
    if (slot == 4) {
        U32(descriptor, 0x20) = s_search_error & 1 ? 0x1007 : 0;
        U32(descriptor, 0x34) = s_search_error & 2 ? 1 : 0;
        U32(descriptor, 0x48) = s_search_error & 4 ? 1 : 0;
    } else if (slot == 8 || slot == 0xc) {
        U32(descriptor, 0x64) += slot == 8 ? 1 : 3;
        strcpy(s_search_title, slot == 8 ? "Added fixture area" : "Added all fixture areas");
    } else {
        snprintf(s_search_path, sizeof(s_search_path), "%c:\\fixture\\%s",
            slot == 0x10 ? 'C' + U32(descriptor, 0x6c) : 'D',
            slot == 0x10 ? "searched" : slot == 0x14 ? "next" : "next-folder");
        snprintf(s_search_title, sizeof(s_search_title), "Fixture action %x", slot);
        U32(descriptor, 0x74) = U32(descriptor, 0x78) = U32(descriptor, 0x7c) = U32(descriptor, 0x80) = 1;
    }
    return 0xffffffff;
}
#define SEARCH_CALLBACK(name, slot) static uint32_t SC name(void *descriptor) { return search_callback(descriptor, slot); }
SEARCH_CALLBACK(search_check, 4)
SEARCH_CALLBACK(search_add, 8)
SEARCH_CALLBACK(search_add_all, 0xc)
SEARCH_CALLBACK(search_scan, 0x10)
SEARCH_CALLBACK(search_next, 0x14)
SEARCH_CALLBACK(search_next_dir, 0x18)
static void search_fixture(void)
{
    memset(s_search_descriptor, 0, sizeof(s_search_descriptor));
    memset(s_search_path, 0, sizeof(s_search_path)); memset(s_search_title, 0, sizeof(s_search_title));
    memset(s_drives, 0, sizeof(s_drives)); memcpy(s_drives, "C:\\\0D:\\\0E:\\\0", 12);
    U32(s_search_descriptor, 0) = 0xabcdef98;
    PTR(s_search_descriptor, 4) = search_check; PTR(s_search_descriptor, 8) = search_add;
    PTR(s_search_descriptor, 0xc) = search_add_all; PTR(s_search_descriptor, 0x10) = search_scan;
    PTR(s_search_descriptor, 0x14) = search_next; PTR(s_search_descriptor, 0x18) = search_next_dir;
    PTR(s_search_descriptor, 0x1c) = "Fixture scenery drive search";
    PTR(s_search_descriptor, 0x24) = s_search_path; U32(s_search_descriptor, 0x28) = sizeof(s_search_path);
    PTR(s_search_descriptor, 0x38) = s_search_title; U32(s_search_descriptor, 0x3c) = 40 + s_case;
    U32(s_search_descriptor, 0x4c) = (s_case % 5 + 1) * 10;
    U32(s_search_descriptor, 0x54) = (s_case / 5) & 1;
    U32(s_search_descriptor, 0x64) = 2;
    U32(s_search_descriptor, 0x68) = 3; U32(s_search_descriptor, 0x6c) = s_case % 3;
    PTR(s_search_descriptor, 0x70) = s_drives;
    U32(s_search_descriptor, 0x74) = U32(s_search_descriptor, 0x78) = !(s_case & 8);
    U32(s_search_descriptor, 0x7c) = !!(s_case & 1); U32(s_search_descriptor, 0x80) = !!(s_case & 2);
    strcpy(s_search_path, "C:\\fixture\\scenery"); strcpy(s_search_title, "Fixture search area");
    if (s_case & 16) { memset(s_search_path + strlen(s_search_path), 'P', 130); s_search_path[148] = 0; }
    memset(s_action_counts, 0, sizeof(s_action_counts));
    s_error_seen = s_search_error = s_callbacks = s_log_length = 0; s_logs[s_side][0] = 0;
    s_error_snapshots[s_side][0] = 0; s_error_lengths[s_side] = 0;
}
static void search_owner_pointer(void *entry, HMODULE module)
{
    HMODULE owner;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (const char *)entry, &owner) || owner != module) { fprintf(stderr, "search dispatch ownership failure\n"); { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); } }
}
static void search_ownership(HWND window, HMODULE module)
{
    void *object = world_object(window); void **vtable = PTR(object, 0);
    const uint32_t slots[] = {4, 0x30, 0x8c, 0xac, 0xc4, 0xd0};
    void *map; uint32_t index;
    for (index = 0; index < sizeof(slots) / sizeof(slots[0]); ++index) search_owner_pointer(vtable[slots[index] / 4], module);
    map = ((void *(TC *)(void *))vtable[0x30 / 4])(object);
    for (index = 0; index < 13; ++index) search_owner_pointer(PTR(PTR(map, 4), index * 24 + 20), module);
    if (U32(PTR(map, 4), index * 24)) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
}
static void search_command(HWND window, uint32_t id)
{ SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(window, id)); }
static void search_combo(HWND window, uint32_t id, uint32_t selected, uint32_t code)
{
    SendMessageA(GetDlgItem(window, id), CB_SETCURSEL, selected, 0);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, code), (LPARAM)GetDlgItem(window, id));
}
static uint32_t search_script(HWND window, uint32_t step, uint32_t *posted, uint32_t *post_target)
{
    uint32_t focus = 0;
    if (step) SetWindowTextA(GetDlgItem(window, 0x1b06), "Edited search title");
    switch (step) {
        case 3: {
            HWND check = GetDlgItem(window, 0x1ab0);
            SendMessageA(check, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(3, 3));
            SendMessageA(check, WM_LBUTTONUP, 0, MAKELPARAM(3, 3)); world_pump();
            if (!s_side && U32(s_search_descriptor, 0x54) != (uint32_t)(!((s_case / 5) & 1))) {
                fprintf(stderr, "search checkbox witness failed\n"); { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
            }
            ++s_checks; break;
        }
        case 4: case 5: search_combo(window, 0x1ada, step - 3, step == 4 ? CBN_SELCHANGE : CBN_CLOSEUP); break;
        case 6: case 7: search_combo(window, 0x1adb, step == 6 ? 2 : 1, step == 6 ? CBN_SELCHANGE : CBN_CLOSEUP); break;
        case 8: search_command(window, 0x1aaf); break;
        case 9: search_command(window, 0x1aae); break;
        case 10: search_command(window, 0x1ab1); break;
        case 11: search_command(window, 0x1ab2); break;
        case 12: {
            MSG message;
            search_combo(window, 0x1ada, 1, CBN_SELCHANGE);
            search_command(window, 0x1ab3);
            *posted = PeekMessageA(&message, window, 0x5fa, 0x5fa, PM_NOREMOVE);
            *post_target = *posted ? (uint32_t)GetDlgCtrlID((HWND)message.wParam) : 0;
            if (!s_side && (!*posted || *post_target != 0x1b06 || message.lParam)) { fprintf(stderr, "search post witness failed\n"); { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); } }
            world_pump(); focus = GetDlgCtrlID(GetFocus());
            if (!s_side && GetFocus() != GetDlgItem(window, 0x1b06)) { fprintf(stderr, "search focus witness failed %x\n", focus); { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); } }
            ++s_checks; break;
        }
        case 13: case 14: case 15: case 16:
            s_search_error = step == 16 ? 7 : 1u << (step - 13);
            s_error_owner = window; s_error_ticks = s_error_seen = 0;
            s_error_timer = SetTimer(NULL, 0, 10, world_error_timer);
            search_command(window, step & 1 ? 0x1aaf : 0x1aae);
            if (s_error_timer) KillTimer(NULL, s_error_timer);
            if (!s_side && (s_error_seen != 1 || !IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA))) { fprintf(stderr, "search error witness failed step=%u seen=%u\n", step, s_error_seen); { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); } }
            focus = GetDlgCtrlID(GetFocus());
            /* Add repopulates even after validation fails. */
            if (!s_side) {
                char title[128]; GetWindowTextA(GetDlgItem(window, 0x1b06), title, sizeof(title));
                if (strcmp(title, step & 1 ? "Added fixture area" : "Added all fixture areas")) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
                if (s_action_counts[1] != 1) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
            }
            ++s_checks; break;
    }
    if (!s_side) {
        if ((step == 4 || step == 5) && U32(s_search_descriptor, 0x6c) != step - 3) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
        if ((step == 6 || step == 7) && (U32(s_search_descriptor, 0x4c) != (step == 6 ? 30u : 20u) ||
                SendMessageA(GetDlgItem(window, 0x1adb), CB_GETCOUNT, 0, 0) != 3)) { fprintf(stderr, "search layer witness failed case=%u step=%u\n", s_case, step); exit(2); }
        if ((step == 8 || step == 9) && (s_action_counts[step == 8 ? 2 : 3] != 1 || s_action_counts[1] != 1)) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
        if ((step == 10 || step == 11) && s_action_counts[step == 10 ? 5 : 6] != 1) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
        if (step == 12 && s_action_counts[4] != 1) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
        ++s_checks;
    }
    return focus;
}
static void **s_delete_slot;
static void *s_delete_original;
static uint32_t s_delete_calls;
static void search_delete_restore(void)
{
    DWORD old;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
    *s_delete_slot = s_delete_original;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
}
static void *TC search_delete_hook(void *object, uint32_t flags)
{
    int length;
    ++s_delete_calls;
    length = snprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length, "post.destroy delete flags=%u\n", flags);
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
    s_log_length += (uint32_t)length;
    search_delete_restore();
    return ((void *(TC *)(void *, uint32_t))s_delete_original)(object, flags);
}
static void search_delete_probe(void *object)
{
    DWORD old; void **vtable = PTR(object, 0);
    s_delete_slot = &vtable[1]; s_delete_original = *s_delete_slot; s_delete_calls = 0;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
    *s_delete_slot = search_delete_hook;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) { fprintf(stderr, "search fixture failed line=%u case=%u step=%u side=%u\n", __LINE__, s_case, s_search_step, s_side); exit(2); }
}
struct search_ui_result {
    uint8_t descriptor[0x84]; char path[288], title[288];
    uint32_t callbacks, focus, deletes, posted, post_target, errors;
};
int main(int argc, char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 20;
    struct search_ui_result results[2];
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

    for(cycle=0;cycle<2;++cycle){
        modules[0]=world_load("orig/English/World.DLL");modules[1]=world_load(argc>1?argv[1]:"bin/world-diff/WORLD_area.dll");
        if(modules[0]==modules[1])return 2;
        for(s_side=0;s_side<2;++s_side){
            struct world_ui_import *imports=(void *)GetProcAddress(modules[s_side],"ImportTable");
            imports[0].link=s_host_services;imports[1].link=s_global_data;linkages[s_side]=(void *)GetProcAddress(modules[s_side],"Linkage");
            if(linkages[s_side]->id!=0x80000006)return 2;
        }
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(step=0;step<17;++step){
            for(s_side=0;s_side<2;++s_side){
                HWND window;void *object;uint32_t focus=0;
                uint32_t posted = 0, post_target = 0;
                s_search_step=step;s_current=modules[s_side];search_fixture();
                search_owner_pointer((void *)world_factory(linkages[s_side], 0x13080002), modules[s_side]);
                window=(HWND)world_factory(linkages[s_side],0x13080002)(0,s_search_descriptor);
                if(!IsWindow(window))return 2;
                world_pump();search_ownership(window,modules[s_side]);object=world_object(window);
                focus=search_script(window,step,&posted,&post_target);

                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&(s_case==0||s_case==8)&&!step)world_save_pixels("search-baseline",cycle,step);

                if(step==1||step==2){
                    MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;
                    IsDialogMessageA(window,&key);
                }
                if(!GetWindowLongA(window,GWL_USERDATA))SendMessageA(window,WM_COMMAND,step&&(step&1)?IDOK:IDCANCEL,0);
                if (!s_side && U32(s_search_descriptor, 0) != 0) { fprintf(stderr, "search answer witness failed\n"); return 2; }
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1)return 2;
                memset(&results[s_side],0,sizeof(results[0]));
                memcpy(results[s_side].descriptor,s_search_descriptor,sizeof(s_search_descriptor));
                memcpy(results[s_side].path,s_search_path,sizeof(s_search_path));memcpy(results[s_side].title,s_search_title,sizeof(s_search_title));
                results[s_side].callbacks=s_callbacks;results[s_side].focus=focus;results[s_side].posted=posted;
                results[s_side].post_target=post_target;results[s_side].errors=s_error_seen;
                search_delete_probe(object);DestroyWindow(window);world_pump();search_delete_restore();results[s_side].deletes=s_delete_calls;
                if(!s_side&&s_delete_calls!=1){fprintf(stderr,"search retail deletion witness missing\n");return 2;}

            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||
                s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])||
                strcmp(s_error_snapshots[0],s_error_snapshots[1])||s_error_lengths[0]!=s_error_lengths[1]||
                (s_error_lengths[0]&&memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0]))){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("search-difference",cycle,step);
                fprintf(stderr,"SEARCH UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",
                    cycle,s_case,step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5)return 2;
            if(!FreeLibrary(modules[s_side]))return 2;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World search native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
