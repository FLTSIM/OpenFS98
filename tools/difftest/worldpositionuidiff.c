/* Native exact-position controls over independent FACIL-shaped callbacks. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
static uint8_t s_position_descriptor[0xe0];
static char s_position_texts[8][128];
static uint32_t s_position_errors, s_position_step, s_position_calls[6];
static const uint32_t s_field_status[]={0x64,0x7c,0x94,0xac,0xc0,0x20,0x38,0x50};
static const uint32_t s_field_ids[]={0x119c,0x11a3,0x11a4,0x119f,0x119e,0x11a1,0x11a2,0x119d};
static const uint32_t s_field_snapshots[]={0x1145,0x11aa,0x10e0,0x120f,0x1274,0x133e,0x13a3,0x12d9};
static uint32_t position_callback(void *descriptor,uint32_t slot)
{
    uint32_t index;int length;
    ++s_callbacks;++s_position_calls[slot/4];
    length=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"callback %x",slot);
    if(length<0||(uint32_t)length>=8192-s_log_length)exit(2);
        s_log_length+=(uint32_t)length;
    for(index=0;index<8;++index){
        length=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length," [%s:%u]",(char *)PTR(descriptor,s_field_status[index]+4),U32(descriptor,s_field_status[index]+0x14));
        if(length<0||(uint32_t)length>=8192-s_log_length)exit(2);
        s_log_length+=(uint32_t)length;
    }
    if(s_log_length+2>=8192)exit(2);
    strcpy(s_logs[s_side]+s_log_length,"\n");++s_log_length;
    if(slot==4)for(index=0;index<8;++index)U32(descriptor,s_field_status[index])=(s_position_errors&(1u<<index))?5:0;
    if(slot==0x10)for(index=0;index<8;++index)snprintf(PTR(descriptor,s_field_status[index]+4),128,"%u",100+s_case+index);
    if(slot==0xc)for(index=0;index<3;++index)strcpy(PTR(descriptor,s_field_status[index+5]+4),PTR(descriptor,s_field_status[index]+4));
    return 0xffffffff;
}
#define POSITION_CALLBACK(name,slot) static uint32_t SC name(void *descriptor){return position_callback(descriptor,slot);}
POSITION_CALLBACK(position_check,4)
POSITION_CALLBACK(position_apply,8)
POSITION_CALLBACK(position_revert,0xc)
POSITION_CALLBACK(position_reload,0x10)
POSITION_CALLBACK(position_facilities,0x14)
static void position_fixture(void)
{
    uint32_t index;
    memset(s_position_descriptor,0,sizeof(s_position_descriptor));memset(s_position_texts,0,sizeof(s_position_texts));
    U32(s_position_descriptor,0)=0x12345678;
    PTR(s_position_descriptor,4)=position_check;PTR(s_position_descriptor,8)=position_apply;
    PTR(s_position_descriptor,0xc)=position_revert;PTR(s_position_descriptor,0x10)=position_reload;PTR(s_position_descriptor,0x14)=position_facilities;
    U32(s_position_descriptor,0x18)=(s_case/4)%3;U32(s_position_descriptor,0x1c)=s_case&1;
    for(index=0;index<8;++index){
        snprintf(s_position_texts[index],128,"%u",10+s_case+index);
        PTR(s_position_descriptor,s_field_status[index]+4)=s_position_texts[index];U32(s_position_descriptor,s_field_status[index]+8)=s_case&1?101:16;
        PTR(s_position_descriptor,s_field_status[index]+0xc)="0";PTR(s_position_descriptor,s_field_status[index]+0x10)="100";
    }
    memset(s_position_calls,0,sizeof(s_position_calls));s_position_errors=s_callbacks=s_log_length=0;s_logs[s_side][0]=0;
    s_error_snapshots[s_side][0]=0;s_error_lengths[s_side]=0;
}
static void position_owner(void *entry)
{
    HMODULE module;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)entry,&module)||module!=s_current)exit(2);
}
static void position_ownership(void *object)
{
    void **vtable=PTR(object,0);void *map;uint32_t index;
    const uint32_t slots[]={4,0x30,0x8c,0xac,0xc4,0xcc,0xd0,0xd8,0xdc,0xf0,0xf4};
    for(index=0;index<sizeof(slots)/sizeof(slots[0]);++index)position_owner(vtable[slots[index]/4]);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);
    for(index=0;index<10;++index)position_owner(PTR(PTR(map,4),index*24+20));
    if(U32(PTR(map,4),10*24))exit(2);
}
static void position_edit(HWND window)
{
    uint32_t index;
    for(index=0;index<8;++index){char text[128];snprintf(text,128,"%u",200+s_case+index);SetWindowTextA(GetDlgItem(window,s_field_ids[index]),text);}
}
static uint32_t position_script(HWND window,uint32_t step)
{
    uint32_t index,focus=0;
    if(step>=3&&step<=6)position_edit(window);
    if(step==4&&IsWindowEnabled(GetDlgItem(window,0x1bd2)))SendMessageA(window,WM_COMMAND,0x1bd2,0);
    if(step==5)SendMessageA(window,WM_COMMAND,0x1bd4,0);
    if(step==6)SendMessageA(window,WM_COMMAND,0x1bd9,0);
    if(step==7){
        void *object=world_object(window);void **vtable=PTR(object,0);
        int error=((int (TC *)(void *,uint32_t))vtable[0xd8/4])(object,0);
        ((int (TC *)(void *,uint32_t))vtable[0xdc/4])(object,(uint32_t)error);
        if(!s_side&&error)exit(2);
    }
    if(step>=8){
        s_position_errors=1u<<(step-8);
        s_error_owner=window;s_error_ticks=s_error_seen=0;s_error_timer=SetTimer(NULL,0,10,world_error_timer);
        SendMessageA(window,WM_COMMAND,IDOK,0);
        if(s_error_timer)KillTimer(NULL,s_error_timer);
        if(s_error_seen!=1||!IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA))exit(2);
        world_pump(); focus=GetDlgCtrlID(GetFocus());
        if(!s_side&&focus!=s_field_ids[step-8]){fprintf(stderr,"position focus witness case=%u step=%u got=%x enabled=%d visible=%d\n",s_case,step,focus,IsWindowEnabled(GetDlgItem(window,s_field_ids[step-8])),IsWindowVisible(GetDlgItem(window,s_field_ids[step-8])));exit(2);}
        s_position_errors=0;
    }
    if(!s_side){
        if(step==6&&s_position_calls[5]!=1)exit(2);
        if(step==5&&s_position_calls[3]!=1)exit(2);
        for(index=0;index<8;++index)if(U32(s_position_descriptor,s_field_status[index]+8)!=(s_case&1?101u:16u))exit(2);
        ++s_checks;
    }
    return focus;
}
static void **s_delete_slot;
static void *s_delete_original;
static uint32_t s_delete_calls;
static void position_delete_restore(void)
{
    DWORD old;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) exit(2);
    *s_delete_slot = s_delete_original;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) exit(2);
}
static void *TC position_delete_hook(void *object, uint32_t flags)
{
    int length;
    ++s_delete_calls;
    length = snprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length, "post.destroy delete flags=%u\n", flags);
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) exit(2);
    s_log_length += (uint32_t)length;
    position_delete_restore();
    return ((void *(TC *)(void *, uint32_t))s_delete_original)(object, flags);
}
static void position_delete_probe(void *object)
{
    DWORD old; void **vtable = PTR(object, 0);
    s_delete_slot = &vtable[1]; s_delete_original = *s_delete_slot; s_delete_calls = 0;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), PAGE_READWRITE, &old)) exit(2);
    *s_delete_slot = position_delete_hook;
    if (!VirtualProtect(s_delete_slot, sizeof(void *), old, &old)) exit(2);
}

struct position_ui_result {uint8_t descriptor[0xe0];char texts[8][128],snapshots[8][101];uint32_t callbacks,focus,deletes,flag,ranges[2];};
int main(int argc,char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 16;
    struct position_ui_result results[2];
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
            struct world_ui_import *imports=(void *)GetProcAddress(modules[s_side],"ImportTable");imports[0].link=s_host_services;imports[1].link=s_global_data;
            linkages[s_side]=(void *)GetProcAddress(modules[s_side],"Linkage");if(linkages[s_side]->id!=0x80000006)return 2;
        }
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(step=0;step<16;++step){
            for(s_side=0;s_side<2;++s_side){
                HWND window;void *object;void *factory;uint32_t focus,field;
                s_position_step=step;s_current=modules[s_side];position_fixture();factory=(void *)world_factory(linkages[s_side],0x1308);position_owner(factory);
                window=(HWND)((uint32_t (SC *)(uint32_t,void *))factory)(0,s_position_descriptor);
                if(!IsWindow(window))return 2;
                world_pump();object=world_object(window);position_ownership(object);
                if(!s_side&&U32(object,0x26e8)!=1)exit(2);
                if(!s_side&&!!IsWindowEnabled(GetDlgItem(window,0x1bd2))!=(U32(s_position_descriptor,0x18)==0))exit(2);
                focus=position_script(window,step);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&!s_case&&!step)world_save_pixels("position-baseline",cycle,step);
                if(step==1||step==2){MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;IsDialogMessageA(window,&key);}
                if(!GetWindowLongA(window,GWL_USERDATA))SendMessageA(window,WM_COMMAND,step==3?IDOK:IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1)return 2;
                if(!s_side&&U32(s_position_descriptor,0)!=(step==1||step==3?1u:0u))exit(2);
                memset(&results[s_side],0,sizeof(results[0]));memcpy(results[s_side].descriptor,s_position_descriptor,sizeof(s_position_descriptor));
                memcpy(results[s_side].texts,s_position_texts,sizeof(s_position_texts));
                for(field=0;field<8;++field)snprintf(results[s_side].snapshots[field],101,"%s",(char *)object+s_field_snapshots[field]);
                results[s_side].ranges[0]=SendMessageA(GetDlgItem(window,0x11b7),0x466,0,0);
                results[s_side].ranges[1]=SendMessageA(GetDlgItem(window,0x11b8),0x466,0,0);
                if(!s_side&&(results[s_side].ranges[0]!=0x10168||results[s_side].ranges[1]!=0x1f4))exit(2);
                results[s_side].callbacks=s_callbacks;results[s_side].focus=focus;results[s_side].flag=U32(object,0x26e8);
                position_delete_probe(object);DestroyWindow(window);world_pump();position_delete_restore();results[s_side].deletes=s_delete_calls;
                if(!s_side&&s_delete_calls!=1)exit(2);
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||
                s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])||
                strcmp(s_error_snapshots[0],s_error_snapshots[1])||s_error_lengths[0]!=s_error_lengths[1]||
                (s_error_lengths[0]&&memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0]))){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("position-difference",cycle,step);
                fprintf(stderr,"POSITION UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",cycle,s_case,step,
                    !!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5)return 1;
            if(!FreeLibrary(modules[s_side]))return 2;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World position native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
