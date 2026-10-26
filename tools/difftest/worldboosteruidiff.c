/* Native booster editors over real MFC/SIMSUI controls, alerts and pixels. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
static uint8_t s_booster_descriptor[0x30], s_booster_row[0x80];
static char s_booster_cache_path[288], s_booster_source_path[288], s_booster_name[128], s_booster_description[288];
static uint32_t s_booster_error_kind;
static uint32_t SC booster_check(void *descriptor)
{
    void *row=PTR(descriptor,0xc);
    int length;
    ++s_callbacks;
    U32(row,0)=s_booster_error_kind==1||strstr(PTR(row,4),"invalid")?0x1007:0;
    U32(row,0x14)=s_booster_error_kind==2?0x1007:0;
    U32(row,0x78)=s_booster_error_kind==3?1:0;
    U32(row,0x50)=s_booster_error_kind==4?1:0;
    U32(row,0x58)=U32(row,0x5c)<100?5:U32(row,0x5c)>2500?6:0;
    snprintf(s_booster_name,sizeof(s_booster_name),"%s",U32(row,0)?"Unavailable booster":"Fixture booster");
    snprintf(s_booster_description,sizeof(s_booster_description),"%s",U32(row,0)?"Choose a valid source":"Independent native booster fixture.");
    length=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,
        "check cache_path=[%s] source_path=[%s] cache_size=%u active=%u combo=%u statuses=%u/%u/%u/%u/%u\n",
        (char *)PTR(row,4),(char *)PTR(row,0x18),U32(row,0x5c),U32(row,0x54),U32(row,0x7c),
        U32(row,0),U32(row,0x14),U32(row,0x78),U32(row,0x50),U32(row,0x58));
    if(length<0||(uint32_t)length>=8192-s_log_length)exit(2);
    s_log_length+=(uint32_t)length;
    return 0xffffffff;
}
static void booster_fixture(void)
{
    memset(s_booster_descriptor,0,sizeof(s_booster_descriptor));memset(s_booster_row,0,sizeof(s_booster_row));
    memset(s_booster_cache_path,0,sizeof(s_booster_cache_path));memset(s_booster_source_path,0,sizeof(s_booster_source_path));
    memset(s_booster_name,0,sizeof(s_booster_name));memset(s_booster_description,0,sizeof(s_booster_description));
    PTR(s_booster_descriptor,4)=booster_check;PTR(s_booster_descriptor,8)="Fixture booster editor";PTR(s_booster_descriptor,0xc)=s_booster_row;
    PTR(s_booster_row,4)=s_booster_cache_path;U32(s_booster_row,8)=sizeof(s_booster_cache_path);
    PTR(s_booster_row,0x18)=s_booster_source_path;U32(s_booster_row,0x1c)=sizeof(s_booster_source_path);
    PTR(s_booster_row,0x2c)=s_booster_name;PTR(s_booster_row,0x40)=s_booster_description;
    PTR(s_booster_row,0x6c)="100";PTR(s_booster_row,0x70)="2500";
    U32(s_booster_row,0x5c)=1000+s_case*3;U32(s_booster_row,0x74)=1000;U32(s_booster_row,0x54)=(s_case>>1)&1;U32(s_booster_row,0x7c)=(s_case>>2)&1;
    if(!(s_case&8))strcpy(s_booster_cache_path,"C:\\fixture\\booster");
    strcpy(s_booster_source_path,"C:\\fixture\\cache");strcpy(s_booster_name,"Initial booster");strcpy(s_booster_description,"Initial booster description");
    s_booster_error_kind=s_callbacks=s_log_length=0;s_logs[s_side][0]=0;s_error_snapshots[s_side][0]=0;s_error_lengths[s_side]=0;
}
static void booster_edit(HWND window)
{
    SetWindowTextA(GetDlgItem(window,0x1ae8),"C:\\fixture\\edited");
    SendMessageA(window,WM_COMMAND,MAKEWPARAM(0x1ae8,EN_CHANGE),(LPARAM)GetDlgItem(window,0x1ae8));
    SetWindowTextA(GetDlgItem(window,0x1aea),"C:\\fixture\\edited-cache");
    SetWindowTextA(GetDlgItem(window,0x1ae9),"1200");
    SendMessageA(GetDlgItem(window,0x1ad3),CB_SETCURSEL,(s_case+1)&1,0);
    SendMessageA(window,WM_COMMAND,0x1a94,(LPARAM)GetDlgItem(window,0x1a94));
}
static uint32_t booster_error_probe(HWND window,uint32_t step)
{
    void *object=world_object(window);void **vtable=PTR(object,0);
    uint32_t focus;
    int invalid;
    s_booster_error_kind=step<=6?step-2:0;
    if(step>=7)SetWindowTextA(GetDlgItem(window,0x1ae9),"9999999");
    s_error_owner=window;s_error_ticks=s_error_seen=0;s_error_timer=SetTimer(NULL,0,10,world_error_timer);
    if(step==8){
        invalid=((int (TC *)(void *,uint32_t))vtable[0xd8/4])(object,0);
        if(invalid!=0x1ae9)exit(2);
        ((int (TC *)(void *,uint32_t))vtable[0xdc/4])(object,(uint32_t)invalid);
    }else if(step&1)((void (TC *)(void *))vtable[0xcc/4])(object);
    else SendMessageA(window,WM_COMMAND,IDOK,0);
    if(s_error_timer)KillTimer(NULL,s_error_timer);
    if(s_error_seen!=1||!IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)){
        fprintf(stderr,"booster error witness failed step=%u seen=%u\n",step,s_error_seen);exit(2);
    }
    focus=(uint32_t)GetDlgCtrlID(GetFocus());
    if(!s_side&&step!=8){
        const uint32_t controls[]={0x1ae8,0x1aea,0x1ad3,0x1a94,0x1ae9,0x1ae9};
        HWND expected=GetDlgItem(window,controls[step-3]);
        if(GetFocus()!=expected&&!IsChild(expected,GetFocus())){fprintf(stderr,"booster focus witness failed step=%u focus=%x\n",step,focus);exit(2);}
    }
    s_booster_error_kind=0;SetWindowTextA(GetDlgItem(window,0x1ae9),"1200");
    ++s_checks;return focus;
}
static void **s_delete_slot;
static void *s_delete_original;
static uint32_t s_delete_calls;
static void booster_delete_restore(void)
{
    DWORD old;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) exit(2);
    *s_delete_slot = s_delete_original;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) exit(2);
}
static void *TC booster_delete_hook(void *object, uint32_t flags)
{
    int length;
    ++s_delete_calls;
    length = snprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length, "post.destroy delete flags=%u\n", flags);
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) exit(2);
    s_log_length += (uint32_t)length;
    booster_delete_restore();
    return ((void *(TC *)(void *, uint32_t))s_delete_original)(object, flags);
}
static void booster_delete_probe(void *object)
{
    DWORD old; void **vtable = PTR(object, 0);
    s_delete_slot = &vtable[1]; s_delete_original = *s_delete_slot; s_delete_calls = 0;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) exit(2);
    *s_delete_slot = booster_delete_hook;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) exit(2);
}
struct booster_ui_result {
    uint8_t descriptor[0x30],row[0x80];char cache_path[288],source_path[288],name[128],description[288];
    uint32_t callbacks,focus,deletes;
};
static uint8_t *s_font_destroy_entry, s_font_destroy_saved[5];
static uint32_t s_font_destroys[2];
static HMODULE s_unload_module;
static void booster_font_restore(void)
{
    DWORD old;
    if(!VirtualProtect(s_font_destroy_entry,5,PAGE_EXECUTE_READWRITE,&old))exit(2);
    memcpy(s_font_destroy_entry,s_font_destroy_saved,5);FlushInstructionCache(GetCurrentProcess(),s_font_destroy_entry,5);
    VirtualProtect(s_font_destroy_entry,5,old,&old);
}
static void TC booster_font_destroy_hook(void *font)
{
    HMODULE owner;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)font,&owner)||owner!=s_unload_module)exit(2);
    ++s_font_destroys[s_side];booster_font_restore();
    ((void (TC *)(void *))s_font_destroy_entry)(font);
}
static void booster_font_probe(HMODULE ui,HMODULE module)
{
    DWORD old;
    s_font_destroy_entry=(void *)GetProcAddress(ui,"??1CSimsFont@@UAE@XZ");s_unload_module=module;s_font_destroys[s_side]=0;
    if(!s_font_destroy_entry||!VirtualProtect(s_font_destroy_entry,5,PAGE_EXECUTE_READWRITE,&old))exit(2);
    memcpy(s_font_destroy_saved,s_font_destroy_entry,5);s_font_destroy_entry[0]=0xe9;
    *(uint32_t *)(s_font_destroy_entry+1)=(uint32_t)booster_font_destroy_hook-(uint32_t)s_font_destroy_entry-5;
    FlushInstructionCache(GetCurrentProcess(),s_font_destroy_entry,5);VirtualProtect(s_font_destroy_entry,5,old,&old);
}
static uint32_t s_booster_step, s_booster_stage;
static LONG WINAPI booster_fault(EXCEPTION_POINTERS *exception)
{
    CONTEXT *context=exception->ContextRecord;
    fprintf(stderr,"booster stage=%u step=%u eax=%lx ebx=%lx ecx=%lx edx=%lx esp=%lx ebp=%lx\n",
        s_booster_stage,s_booster_step,context->Eax,context->Ebx,context->Ecx,context->Edx,context->Esp,context->Ebp);
    return world_fault(exception);
}
int main(int argc, char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 16;
    struct booster_ui_result results[2];
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, NULL, _IONBF, 0); SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(booster_fault);
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
        for(s_case=0;s_case<cases;++s_case)for(step=0;step<10;++step){
            for(s_side=0;s_side<2;++s_side){
                HWND window;void *object;uint32_t focus=0;
                s_booster_step=step;s_booster_stage=1;s_current=modules[s_side];booster_fixture();
                window=(HWND)world_factory(linkages[s_side],s_case&1?0x13080008:0x13080007)(0,s_booster_descriptor);
                s_booster_stage=2;if(!IsWindow(window))return 2;
                world_pump();world_ownership(window,modules[s_side]);object=world_object(window);
                s_booster_stage=3;if(step)booster_edit(window);
                if(step>=3&&step<=8)focus=booster_error_probe(window,step);
                if(step==9){
                    SetWindowTextA(GetDlgItem(window,0x1ae8),"invalid source");
                    SendMessageA(window,WM_COMMAND,MAKEWPARAM(0x1ae8,EN_CHANGE),(LPARAM)GetDlgItem(window,0x1ae8));
                    if(IsWindowEnabled(GetDlgItem(window,0x1aea))){fprintf(stderr,"booster disable witness missing\n");return 2;}
                    ++s_checks;booster_edit(window);
                }
                s_booster_stage=4;SendMessageA(window,WM_NCLBUTTONDOWN,HTNOWHERE,0);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&(s_case==0||s_case==9)&&!step)world_save_pixels("booster-baseline",cycle,step);
                s_booster_stage=5;
                if(step==1||step==2){
                    MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;
                    IsDialogMessageA(window,&key);
                }
                if(!GetWindowLongA(window,GWL_USERDATA))SendMessageA(window,WM_COMMAND,step&&(step&1)?IDOK:IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1)return 2;
                memset(&results[s_side],0,sizeof(results[0]));
                memcpy(results[s_side].descriptor,s_booster_descriptor,sizeof(s_booster_descriptor));
                memcpy(results[s_side].row,s_booster_row,sizeof(s_booster_row));PTR(results[s_side].row,0x64)=NULL;
                memcpy(results[s_side].cache_path,s_booster_cache_path,sizeof(s_booster_cache_path));memcpy(results[s_side].source_path,s_booster_source_path,sizeof(s_booster_source_path));
                memcpy(results[s_side].name,s_booster_name,sizeof(s_booster_name));memcpy(results[s_side].description,s_booster_description,sizeof(s_booster_description));
                results[s_side].callbacks=s_callbacks;results[s_side].focus=focus;
                s_booster_stage=6;booster_delete_probe(object);DestroyWindow(window);world_pump();booster_delete_restore();results[s_side].deletes=s_delete_calls;
                if(!s_side&&s_delete_calls!=1){fprintf(stderr,"booster retail deletion witness missing\n");return 2;}
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||
                s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])||
                strcmp(s_error_snapshots[0],s_error_snapshots[1])||s_error_lengths[0]!=s_error_lengths[1]||
                (s_error_lengths[0]&&memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0]))){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("booster-difference",cycle,step);
                fprintf(stderr,"BOOSTER UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",
                    cycle,s_case,step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5)return 2;
            booster_font_probe(ui,modules[s_side]);
            if(!FreeLibrary(modules[s_side]))return 2;
            booster_font_restore();
            if(s_font_destroys[s_side]!=1){fprintf(stderr,"BOOSTER UI DIFFER font cleanup side=%u destroys=%u\n",s_side,s_font_destroys[s_side]);return 1;}
            ++s_checks;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World booster native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
