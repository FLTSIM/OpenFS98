/* Native basic-weather acceptance with an independent MAIN-shaped owner.
 * No real weather application runs. Retail executes its complete class. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
static uint8_t s_weather_descriptors[2][0x20];
static char s_weather_title[] = "Independent weather fixture";
static uint32_t s_step;
static void *s_weather_object;
static const uint32_t s_combo_ids[] = {0x1ac2, 0x1ac0, 0x1abe};
static const uint32_t s_combo_offsets[] = {0x15a4, 0x15ec, 0x1634};
static const uint32_t s_fields[] = {0x1c, 0x18, 0x14};
static int SC weather_validate(void *descriptor)
{
    ++s_callbacks;
    s_log_length += snprintf(s_logs[s_side]+s_log_length,8192-s_log_length,
        "validate descriptor=%u answer=%x values=%x/%x/%x mode=%x\n",
        descriptor==(void *)s_weather_descriptors[1],U32(descriptor,0),
        U32(descriptor,0x14),U32(descriptor,0x18),U32(descriptor,0x1c),U32(s_weather_object,0x195c));
    if(s_step==17)PTR(s_weather_object,0x10e0)=s_weather_descriptors[1];
    return s_case&1 ? (int)0x80000001 : 0;
}
static void weather_fixture(void)
{
    uint32_t row;
    static const uint32_t values[]={0,1,2,3,4,0xffffffff,0x80000000,65536};
    memset(s_weather_descriptors,0,sizeof(s_weather_descriptors));
    for(row=0;row<2;++row){
        U32(s_weather_descriptors[row],0)=0xfeed+row;
        PTR(s_weather_descriptors[row],4)=weather_validate;PTR(s_weather_descriptors[row],8)=s_weather_title;
        U32(s_weather_descriptors[row],0xc)=s_case&1;U32(s_weather_descriptors[row],0x10)=s_case&2;
        U32(s_weather_descriptors[row],0x14)=values[s_case%8];
        U32(s_weather_descriptors[row],0x18)=values[(s_case+2)%8];
        U32(s_weather_descriptors[row],0x1c)=values[(s_case+4)%8];
    }
    s_callbacks=s_log_length=0;s_logs[s_side][0]=0;
}
static void weather_owned(void *address,HMODULE module)
{
    HMODULE owner;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)address,&owner)||owner!=module){fprintf(stderr,"WEATHER UI DIFFER ownership\n");exit(1);}
}
static void weather_ownership(void *object,HMODULE module)
{
    static const uint32_t slots[]={4,0x30,0x8c,0xac,0xc4,0xcc,0xd0};
    void **vtable=PTR(object,0);uint32_t *entries,index;void *map;
    weather_owned(vtable,module);
    for(index=0;index<sizeof(slots)/sizeof(slots[0]);++index)weather_owned(vtable[slots[index]/4],module);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);entries=PTR(map,4);
    for(index=0;index<4;++index)weather_owned((void *)entries[index*6+5],module);
    if(entries[24])exit(2);
}
struct weather_result {
    uint8_t descriptors[2][0x20];uint32_t mode,dialog_deletes,callbacks;
    uint32_t fonts[3],labels[3],enabled[3],heading_font[6];
    int selection[3],count[3];char rows[3][512];
};
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

static void weather_controls(HWND window,void *object,struct weather_result *result)
{
    uint32_t index;
    for(index=0;index<3;++index){
        HWND combo=GetDlgItem(window,s_combo_ids[index]);uint32_t offset=s_combo_offsets[index];int row,length=0;
        result->count[index]=(int)SendMessageA(combo,CB_GETCOUNT,0,0);
        result->selection[index]=(int)SendMessageA(combo,CB_GETCURSEL,0,0);
        result->enabled[index]=!!IsWindowEnabled(combo);
        if(result->count[index]!=(index==0?4:5)){fprintf(stderr,"WEATHER UI DIFFER count side=%u index=%u count=%d\n",s_side,index,result->count[index]);exit(s_side?1:2);}
        for(row=0;row<result->count[index];++row){char text[128];LRESULT size=SendMessageA(combo,CB_GETLBTEXTLEN,row,0);
            if(size<0||size>=128)exit(2);
            SendMessageA(combo,CB_GETLBTEXT,row,(LPARAM)text);
            length+=snprintf(result->rows[index]+length,512-(uint32_t)length,"row=%d [%s]\n",row,text);
        }
        result->labels[index]=(uint32_t)((uint8_t *)PTR(object,offset+0x40)-(uint8_t *)object);
        result->fonts[index]=(uint32_t)((uint8_t *)PTR(object,offset+0x44)-(uint8_t *)object);
        if(result->fonts[index]!=0x3e0||result->labels[index]!=(index==0?0x137cu:index==1?0x1434u:0x14ecu)){
            fprintf(stderr,"WEATHER UI DIFFER binding\n");exit(s_side?1:2);
        }
    }
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
int main(int argc,char **argv)
{
    HMODULE ui,global_module,modules[2];struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH];uint32_t index,cycle,comparisons=0;
    uint32_t cases=argc>2?strtoul(argv[2],NULL,0):20;
    struct weather_result results[2];
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
            weather_owned(world_factory(linkages[s_side],0x1302),modules[s_side]);
        }
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(s_step=0;s_step<20;++s_step){
            for(s_side=0;s_side<2;++s_side){
                HWND window;void *object;void **vtable;int accept=s_step!=0&&s_step!=2&&s_step!=12;
                uint32_t descriptor_index=s_step==19;int initial_cloud,expected_enabled;
                memset(&results[s_side],0,sizeof(results[s_side]));s_current=modules[s_side];weather_fixture();
                window=(HWND)world_factory(linkages[s_side],0x1302)(0,s_weather_descriptors[descriptor_index]);
                if(!IsWindow(window))return 2;
                world_pump();object=world_object(window);s_weather_object=object;vtable=PTR(object,0);weather_ownership(object,modules[s_side]);
                initial_cloud=(int)SendMessageA(GetDlgItem(window,0x1abe),CB_GETCURSEL,0,0);
                expected_enabled=initial_cloud!=0&&initial_cloud!=4;
                if(!!IsWindowEnabled(GetDlgItem(window,0x1ac0))!=expected_enabled||
                    !!IsWindowEnabled(GetDlgItem(window,0x1abe))!=!(s_case&1)||
                    !!IsWindowEnabled(GetDlgItem(window,0x1ac2))!=!(s_case&2)||
                    !!IsWindowVisible(GetDlgItem(window,0x1b2c))!=!!(s_case&1)||
                    !!IsWindowVisible(GetDlgItem(window,0x1b2e))!=!!(s_case&2)){
                    fprintf(stderr,"WEATHER UI DIFFER initial enable witness side=%u case=%u\n",s_side,s_case);return s_side?1:2;
                }
                if(s_step>=3&&s_step<=7){
                    SendMessageA(GetDlgItem(window,0x1abe),CB_SETCURSEL,s_step-3,0);
                    SendMessageA(window,WM_COMMAND,MAKEWPARAM(0x1abe,CBN_SELCHANGE),(LPARAM)GetDlgItem(window,0x1abe));world_pump();
                    if(!!IsWindowEnabled(GetDlgItem(window,0x1ac0))!=(s_step!=3&&s_step!=7)){
                        fprintf(stderr,"WEATHER UI DIFFER selection enable\n");return s_side?1:2;
                    }
                }
                if(s_step==8||s_step==9)SendMessageA(GetDlgItem(window,s_combo_ids[s_step-8]),CB_SETCURSEL,3,0);
                if(s_step==10)for(index=0;index<3;++index)SendMessageA(GetDlgItem(window,s_combo_ids[index]),CB_SETCURSEL,-1,0);
                if(s_step==18){
                    SendMessageA(GetDlgItem(window,0x1abe),CB_SETCURSEL,-1,0);
                    SendMessageA(window,WM_COMMAND,MAKEWPARAM(0x1abe,CBN_SELCHANGE),(LPARAM)GetDlgItem(window,0x1abe));
                }
                if(s_step>=13&&s_step<=15){SendMessageA(GetDlgItem(window,s_combo_ids[s_step-13]),CB_SHOWDROPDOWN,TRUE,0);world_pump();}
                weather_controls(window,object,&results[s_side]);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&s_case==0&&!s_step)world_save_pixels("weather-baseline",cycle,s_step);
                if(s_step>=13&&s_step<=15){SendMessageA(GetDlgItem(window,s_combo_ids[s_step-13]),CB_SHOWDROPDOWN,FALSE,0);world_pump();}
                if(s_step==1||s_step==2){
                    MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=s_step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;IsDialogMessageA(window,&key);
                }else if(s_step==11||s_step==12)((void(TC *)(void *))vtable[(s_step==11?0xcc:0xd0)/4])(object);
                else SendMessageA(window,WM_COMMAND,accept?IDOK:IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1){fprintf(stderr,"WEATHER UI DIFFER close\n");return 1;}
                results[s_side].mode=U32(object,0x195c);results[s_side].callbacks=s_callbacks;
                memcpy(results[s_side].descriptors,s_weather_descriptors,sizeof(s_weather_descriptors));
                if(s_callbacks!=(uint32_t)accept||U32(object,0x195c)!=(accept?1u:2u)||U32(s_weather_descriptors[s_step==17?1:descriptor_index],0)!=(uint32_t)accept){
                    fprintf(stderr,"WEATHER UI DIFFER answer/mode/callback\n");return s_side?1:2;
                }
                if(accept)for(index=0;index<3;++index){
                    int selection=results[s_side].selection[index];uint32_t expected=(uint32_t)selection;
                    if(selection<0||selection>(index==2?4:3))expected=index==1?4:0;
                    if(U32(s_weather_descriptors[descriptor_index],s_fields[index])!=expected){fprintf(stderr,"WEATHER UI DIFFER collect\n");return s_side?1:2;}
                }
                weather_probe(object,&s_dialog_probe,weather_dialog_hook);DestroyWindow(window);world_pump();weather_restore(&s_dialog_probe);
                results[s_side].dialog_deletes=s_dialog_probe.calls;
                if(s_dialog_probe.calls!=1){fprintf(stderr,"WEATHER UI DIFFER dialog deletion\n");return 1;}
                ++s_checks;
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||
                s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("weather-difference",cycle,s_step);
                fprintf(stderr,"WEATHER UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",cycle,s_case,s_step,
                    !!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5){fprintf(stderr,"WEATHER UI DIFFER module lifetime\n");return 1;}
            weather_font_probe(ui,modules[s_side]);
            if(!FreeLibrary(modules[s_side]))return 2;
            weather_font_restore();
            if(s_font_destroys[s_side]!=1){fprintf(stderr,"WEATHER UI DIFFER font cleanup side=%u destroys=%u\n",s_side,s_font_destroys[s_side]);return 1;}
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World weather native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
