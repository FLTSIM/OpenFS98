/* Independent MAIN-shaped weather-area owner; native controls and unmasked pixels. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
static uint32_t s_step;
static void weather_owned(void *address,HMODULE module)
{
    HMODULE owner;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)address,&owner)||owner!=module){fprintf(stderr,"WEATHER UI DIFFER ownership\n");exit(1);}
}
struct weather_probe {void **slot;void *original;uint32_t calls;};
static struct weather_probe s_dialog_probe;
static void weather_restore(struct weather_probe *probe)
{
    DWORD old;if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);
    *probe->slot=probe->original;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);
}
static void *TC weather_dialog_hook(void *object,uint32_t flags)
{
    ++s_dialog_probe.calls;
    s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"dialog.delete flags=%u\n",flags);
    weather_restore(&s_dialog_probe);
    return ((void *(TC *)(void *,uint32_t))s_dialog_probe.original)(object,flags);
}
static void weather_probe(void *object,struct weather_probe *probe,void *hook)
{
    DWORD old;probe->slot=(void **)PTR(object,0)+1;probe->original=*probe->slot;probe->calls=0;
    if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);
    *probe->slot=hook;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);
}

static struct weather_probe s_notify_probe;
static void *TC wareas_notify_hook(void *object,uint32_t flags)
{
    ++s_notify_probe.calls;
    s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"notify.delete flags=%u\n",flags);
    weather_restore(&s_notify_probe);
    return ((void *(TC *)(void *,uint32_t))s_notify_probe.original)(object,flags);
}
static uint8_t *s_font_destroy_entry, s_font_destroy_saved[5];
static uint32_t s_font_destroys[2];
static HMODULE s_unload_module;
static void weather_font_restore(void)
{
    DWORD old;
    if(!VirtualProtect(s_font_destroy_entry,5,PAGE_EXECUTE_READWRITE,&old))exit(2);
    memcpy(s_font_destroy_entry,s_font_destroy_saved,5);FlushInstructionCache(GetCurrentProcess(),s_font_destroy_entry,5);
    VirtualProtect(s_font_destroy_entry,5,old,&old);
}
static void TC weather_font_destroy_hook(void *font)
{
    HMODULE owner;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)font,&owner)||owner!=s_unload_module)exit(2);
    ++s_font_destroys[s_side];weather_font_restore();
    ((void (TC *)(void *))s_font_destroy_entry)(font);
}
static void weather_font_probe(HMODULE ui,HMODULE module)
{
    DWORD old;
    s_font_destroy_entry=(void *)GetProcAddress(ui,"??1CSimsFont@@UAE@XZ");s_unload_module=module;s_font_destroys[s_side]=0;
    if(!s_font_destroy_entry||!VirtualProtect(s_font_destroy_entry,5,PAGE_EXECUTE_READWRITE,&old))exit(2);
    memcpy(s_font_destroy_saved,s_font_destroy_entry,5);s_font_destroy_entry[0]=0xe9;
    *(uint32_t *)(s_font_destroy_entry+1)=(uint32_t)weather_font_destroy_hook-(uint32_t)s_font_destroy_entry-5;
    FlushInstructionCache(GetCurrentProcess(),s_font_destroy_entry,5);VirtualProtect(s_font_destroy_entry,5,old,&old);
}
static uint8_t s_wareas_owner[2][0x2c], s_wareas_rows[3][0xc8];
static char s_wareas_text[3][9][80];
static const uint32_t s_wareas_offsets[] = {8,0x1c,0x30,0x44,0x58,0x6c,0x80,0x94,0xa8};
static const uint32_t s_wareas_ids[] = {0x1b0b,0x1ae6,0x1ae7,0x1af0,0x1af1,0x1ae5,0x1ae4,0x1ae2,0x1ae3};
static void *s_wareas_object;
static uint32_t s_wareas_error;
static int s_wareas_replace;
static int SC wareas_validate(void *owner)
{
    uint32_t selection=U32(owner,0xc),field;
    ++s_callbacks;
    s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"validate selection=%u answer=%x\n",selection,U32(owner,0));
    if(selection>=3)exit(2);
    for(field=0;field<9;++field){
        U32(s_wareas_rows[selection],s_wareas_offsets[field]-4)=s_wareas_error==field+1||(s_wareas_error==10&&(field==7||field==8))?0x1007:0;
        s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"field=%u [%s]\n",field,s_wareas_text[selection][field]);
    }
    if(s_wareas_replace){
        PTR(s_wareas_object,0x10f8)=s_wareas_owner[1];s_wareas_replace=0;
        s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"accept.owner.replace\n");
    }
    return s_wareas_error!=0;
}
static void SC wareas_add(void *owner)
{
    ++s_callbacks; U32(owner,0x10)=3; U32(owner,0xc)=2; U32(owner,0x18)=0;
    s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"add\n");
}
static void SC wareas_delete(void *owner)
{
    ++s_callbacks; U32(owner,0x10)=1; U32(owner,0xc)=0; U32(owner,0x1c)=0;
    s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"delete\n");
}
/* MAIN +0x7190 owns Copy Weather; no simulator callback runs here. */
static void SC wareas_copy(void *owner)
{
    ++s_callbacks; strcpy(s_wareas_text[U32(owner,0xc)][5],"42");
    s_log_length+=snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,"advanced enabled=%u\n",!!IsWindowEnabled((HWND)U32(s_wareas_object,0x20)));
}
static void wareas_fixture(void)
{
    uint32_t row,field;
    memset(s_wareas_owner,0,sizeof(s_wareas_owner));memset(s_wareas_rows,0,sizeof(s_wareas_rows));
    for(row=0;row<3;++row){
        U32(s_wareas_rows[row],0)=row||!(s_case&4);U32(s_wareas_rows[row],0xbc)=s_case&1;U32(s_wareas_rows[row],0xc4)=s_case&2;
        for(field=0;field<9;++field){
            snprintf(s_wareas_text[row][field],80,field?"%u":"Independent area %u",field?row*10+field:row);
            PTR(s_wareas_rows[row],s_wareas_offsets[field])=s_wareas_text[row][field];
            U32(s_wareas_rows[row],s_wareas_offsets[field]+4)=80;
            PTR(s_wareas_rows[row],s_wareas_offsets[field]+8)="0";PTR(s_wareas_rows[row],s_wareas_offsets[field]+12)="999";
        }
    }
    for(row=0;row<2;++row){
        void *owner=s_wareas_owner[row];
        U32(owner,0)=0xfeed+row;PTR(owner,4)=wareas_validate;U32(owner,8)=s_case&1;
        U32(owner,0x10)=1+s_case%3;U32(owner,0xc)=(s_case/3)%U32(owner,0x10);
        PTR(owner,0x14)=s_wareas_rows;U32(owner,0x18)=U32(owner,0x10)<3;U32(owner,0x1c)=U32(owner,0x10)>1;
        PTR(owner,0x20)=wareas_add;PTR(owner,0x24)=wareas_delete;PTR(owner,0x28)=wareas_copy;
    }
    s_callbacks=s_log_length=s_wareas_error=0;s_wareas_replace=0;s_logs[s_side][0]=0;
    s_error_snapshots[s_side][0]=0;s_error_lengths[s_side]=0;
}
static void wareas_ownership(void *object,HMODULE module)
{
    static const uint32_t slots[]={4,0x30,0x8c,0xac,0xc4,0xcc,0xd0,0xd8,0xdc,0xec};
    uint32_t index,*entries;void **vtable=PTR(object,0);void *map;
    weather_owned(vtable,module);
    for(index=0;index<sizeof(slots)/sizeof(slots[0]);++index)weather_owned(vtable[slots[index]/4],module);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);entries=PTR(map,4);weather_owned(map,module);weather_owned(entries,module);
    for(index=0;index<8;++index)weather_owned((void *)entries[index*6+5],module);
    if(entries[48])exit(2);
    weather_owned(PTR(PTR(object,0x10dc),0),module);
    weather_owned(((void **)PTR(PTR(object,0x10dc),0))[1],module);
}
struct wareas_result {
    uint8_t owners[2][0x2c],rows[3][0xc8];char text[3][9][80];
    uint32_t callbacks,selection,limits[9],checks[2],member_limits[9],edit_labels[9],edit_flags[9],heading_style,deletes,notify_deletes,error_seen,error_focus,error_result;
};
int main(int argc,char **argv)
{
    HMODULE ui,global_module,modules[2];struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH];uint32_t index,cycle,comparisons=0;
    uint32_t cases=argc>2?strtoul(argv[2],NULL,0):12;
    struct wareas_result results[2];
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
            weather_owned(world_factory(linkages[s_side],0x1303),modules[s_side]);
        }
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=argc>3?strtoul(argv[3],NULL,0):0;s_case<cases+(argc>3?strtoul(argv[3],NULL,0):0);++s_case)for(s_step=0;s_step<31;++s_step){
            for(s_side=0;s_side<2;++s_side){
                HWND window;void *object;void **vtable;uint32_t row,field;
                memset(&results[s_side],0,sizeof(results[s_side]));s_current=modules[s_side];wareas_fixture();
                SetCursorPos(0,0);world_pump();
                window=(HWND)world_factory(linkages[s_side],0x1303)(0,s_wareas_owner[0]);
                if(!IsWindow(window))return 2;
                world_pump();object=world_object(window);s_wareas_object=object;vtable=PTR(object,0);wareas_ownership(object,modules[s_side]);
                if((uint32_t)SendDlgItemMessageA(window,0x1b14,LB_GETCOUNT,0,0)!=U32(s_wareas_owner[0],0x10))exit(2);
                if(s_step==3){
                    SendDlgItemMessageA(window,0x1b14,LB_SETCURSEL,U32(s_wareas_owner[0],0x10)-1,0);
                    SendMessageA(window,WM_COMMAND,MAKEWPARAM(0x1b14,LBN_SELCHANGE),(LPARAM)GetDlgItem(window,0x1b14));
                }
                if(s_step>=4&&s_step<=6)SendMessageA(window,WM_COMMAND,s_step==4?0x1a93:s_step==5?0x1aa2:0x1a95,0);
                if(s_step==7||s_step==8)SendMessageA(window,WM_COMMAND,s_step==7?0x1ac6:0x1ac7,0);
                if(s_step>=9&&s_step<=17){
                    HWND edit=GetDlgItem(window,s_wareas_ids[s_step-9]);SetWindowTextA(edit,s_step==9?"Renamed independent area":"57");
                    ((int(TC *)(void *,uint32_t))vtable[0xd8/4])(object,s_wareas_ids[s_step-9]);
                }
                if((s_step>=20&&s_step<=28)||s_step==30){
                    HWND edit=GetDlgItem(window,s_wareas_ids[s_step==30?7:s_step-20]);
                    if(U32(PTR(object,0x10fc),0)&&IsWindowEnabled(edit)){
                        s_wareas_error=s_step==30?10:s_step-19;s_error_owner=window;s_error_ticks=s_error_seen=0;
                        s_error_timer=SetTimer(NULL,0,10,world_error_timer);
                        {
                            uint32_t error=((int(TC *)(void *,uint32_t))vtable[0xd8/4])(object,s_wareas_ids[s_step==30?7:s_step-20]);
                            ((int(TC *)(void *,uint32_t))vtable[0xdc/4])(object,error);
                        }
                        if(s_error_timer)KillTimer(NULL,s_error_timer);
                        if(s_error_seen!=1||!IsWindowVisible(window)){fprintf(stderr,"WAREAS UI DIFFER error witness side=%u step=%u seen=%u\n",s_side,s_step,s_error_seen);return 1;}
                        results[s_side].error_seen=s_error_seen;results[s_side].error_focus=GetDlgCtrlID(GetFocus());results[s_side].error_result=U32(object,0x8c);
                        s_wareas_error=0;
                        ((int(TC *)(void *,uint32_t))vtable[0xd8/4])(object,s_wareas_ids[s_step==30?7:s_step-20]);
                    }
                }
                world_pump();
                results[s_side].selection=U32(object,0x10f4);
                for(index=0;index<9;++index)results[s_side].limits[index]=(uint32_t)SendDlgItemMessageA(window,s_wareas_ids[index],EM_GETLIMITTEXT,0,0);
                {
                    static const uint32_t offsets[]={0x17b4,0x23ec,0x2384,0x231c,0x22b4,0x2454,0x24bc,0x258c,0x2524};
                    for(index=0;index<9;++index)results[s_side].member_limits[index]=U32(object,offsets[index]);
                }
                {
                    static const uint32_t edits[]={0x1750,0x2388,0x2320,0x22b8,0x2250,0x23f0,0x2458,0x2528,0x24c0};
                    static const uint32_t labels[]={0,0x1bd8,0x1b20,0x1a68,0x19b0,0x1d48,0x1eb8,0x2198,0x2028};
                    for(index=0;index<9;++index){
                        void *label=PTR(object,edits[index]+0x40);
                        results[s_side].edit_labels[index]=label?(uint32_t)((uint8_t *)label-(uint8_t *)object):0;
                        results[s_side].edit_flags[index]=U32(object,edits[index]+0x60);
                        if(results[s_side].edit_labels[index]!=labels[index]){fprintf(stderr,"WAREAS UI DIFFER edit label binding\n");return s_side?1:2;}
                    }
                    results[s_side].heading_style=*(uint16_t *)((uint8_t *)object+0x11bc+0x98);
                    if(results[s_side].heading_style!=0x110){fprintf(stderr,"WAREAS UI DIFFER heading style\n");return s_side?1:2;}
                }
                results[s_side].checks[0]=U32(object,0x2714);results[s_side].checks[1]=U32(object,0x264c);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&!s_case&&!s_step)world_save_pixels("wareas-baseline",cycle,s_step);
                s_wareas_replace=s_step==19&&U32(PTR(object,0x10fc),0);
                weather_probe(PTR(object,0x10dc),&s_notify_probe,wareas_notify_hook);
                if(s_step==1||s_step==2){MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=s_step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;IsDialogMessageA(window,&key);}
                else if(s_step==18)((void(TC *)(void *))vtable[0xcc/4])(object);
                else if(s_step==29)((void(TC *)(void *))vtable[0xd0/4])(object);
                else SendMessageA(window,WM_COMMAND,s_step?IDOK:IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1){fprintf(stderr,"WAREAS UI DIFFER close witness side=%u step=%u\n",s_side,s_step);return 1;}
                if(s_step==19&&U32(PTR(object,0x10fc),0)&&
                    (s_wareas_replace||U32(s_wareas_owner[1],0)!=1)){
                    fprintf(stderr,"WAREAS UI DIFFER owner replacement at acceptance\n");return s_side?1:2;
                }
                results[s_side].notify_deletes=s_notify_probe.calls;
                weather_restore(&s_notify_probe);
                if(s_notify_probe.calls!=1){fprintf(stderr,"WAREAS UI DIFFER notify deletion\n");return s_side?1:2;}
                memcpy(results[s_side].owners,s_wareas_owner,sizeof(s_wareas_owner));
                memcpy(results[s_side].rows,s_wareas_rows,sizeof(s_wareas_rows));memcpy(results[s_side].text,s_wareas_text,sizeof(s_wareas_text));results[s_side].callbacks=s_callbacks;
                for(row=0;row<3;++row)for(field=0;field<9;++field){
                    /* Alerts may install a temporary message pointer in status +8. */
                    PTR(results[s_side].rows[row],s_wareas_offsets[field]+8)=NULL;
                }
                weather_probe(object,&s_dialog_probe,weather_dialog_hook);DestroyWindow(window);world_pump();weather_restore(&s_dialog_probe);
                results[s_side].deletes=s_dialog_probe.calls;
                if(s_dialog_probe.calls!=1){fprintf(stderr,"WAREAS UI DIFFER scalar deletion\n");return 1;}
                ++s_checks;
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])||strcmp(s_error_snapshots[0],s_error_snapshots[1])||s_error_lengths[0]!=s_error_lengths[1]||(s_error_lengths[0]&&memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0]))){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("wareas-difference",cycle,s_step);
                fprintf(stderr,"WAREAS UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d logs=%d pixels=%d\nRETAIL:\n%s%sOURS:\n%s%s",cycle,s_case,s_step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            weather_font_probe(ui,modules[s_side]);if(!FreeLibrary(modules[s_side]))return 2;weather_font_restore();
            if(s_font_destroys[s_side]!=1){fprintf(stderr,"WAREAS UI DIFFER font cleanup\n");return 1;}
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World weather areas native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
