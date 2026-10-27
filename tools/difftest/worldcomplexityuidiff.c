/* Actual MFC/SIMSUI scenery-complexity controls, ownership, pixels and close. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
#include <stdarg.h>
static uint8_t s_complexity_descriptors[2][0x28];
static void *s_complexity_object;
static uint32_t s_step;
static const uint32_t s_check_ids[] = {0x1acd, 0x1acf, 0x1acc};
static const uint32_t s_check_offsets[] = {0x1648, 0x1580, 0x1710};
static void complexity_log(const char *format, ...)
{
    va_list args; int length;
    va_start(args, format);
    length=vsnprintf(s_logs[s_side]+s_log_length,8192-s_log_length,format,args);
    va_end(args);
    if(length<0 || (uint32_t)length>=8192-s_log_length) exit(2);
    s_log_length+=(uint32_t)length;
}
static int SC complexity_check(void *descriptor)
{
    ++s_callbacks;
    complexity_log("check descriptor=%u answer=%x values=%x/%x/%x/%x mode=%x\n",
        descriptor==s_complexity_descriptors[1],U32(descriptor,0),U32(descriptor,0xc),U32(descriptor,0x14),
        U32(descriptor,0x1c),U32(descriptor,0x24),U32(s_complexity_object,0x171c));
    U32(descriptor,8)=s_case&1 ? 0x1003 : 0;
    U32(descriptor,0x10)=s_case&2 ? 0x1005 : 0;
    U32(descriptor,0x18)=s_case&4 ? 0x1007 : 0;
    U32(descriptor,0x20)=s_case&8 ? 0x1009 : 0;
    if(s_case&8) PTR(s_complexity_object,0x10e0)=s_complexity_descriptors[1];
    return s_case&1 ? -1 : 0;
}
static void complexity_fixture(void)
{
    uint32_t row;
    memset(s_complexity_descriptors,0,sizeof(s_complexity_descriptors));
    for(row=0;row<2;++row) {
        PTR(s_complexity_descriptors[row],4)=complexity_check;
        U32(s_complexity_descriptors[row],0)=0xfeed;
        U32(s_complexity_descriptors[row],0xc)=(s_case+row)&1;
        U32(s_complexity_descriptors[row],0x14)=((s_case>>1)+row)&1;
        U32(s_complexity_descriptors[row],0x1c)=((s_case>>2)+row)&1;
        U32(s_complexity_descriptors[row],0x24)=(s_case%7)-1+row;
    }
    s_callbacks=s_log_length=0;s_logs[s_side][0]=0;
}
static void complexity_owned(void *address, HMODULE module)
{
    HMODULE owner;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)address,&owner)||owner!=module) {fprintf(stderr,"complexity dispatch ownership failed\n");exit(2);}
}
static void complexity_ownership(void *object, HMODULE module)
{
    uint32_t index; void **vtable=PTR(object,0); void *map; uint32_t *entry;
    static const uint32_t s_slots[]={4,0x30,0x8c,0xac,0xc4,0xcc,0xd0,0xec};
    complexity_owned(vtable,module);
    for(index=0;index<sizeof(s_slots)/sizeof(s_slots[0]);++index) complexity_owned(vtable[s_slots[index]/4],module);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);
    entry=PTR(map,4);
    for(index=0;index<2;++index) complexity_owned((void *)entry[index*6+5],module);
    if(entry[12]) exit(2);
    complexity_owned(PTR(PTR(object,0x10dc),0),module);
    complexity_owned(PTR(PTR(PTR(object,0x10dc),0),4),module);
}
struct complexity_probe {void **slot;void *original;uint32_t calls;};
static struct complexity_probe s_notify_probe,s_dialog_probe;
static void complexity_restore(struct complexity_probe *probe)
{
    DWORD old;if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);
    *probe->slot=probe->original;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);
}
static void *TC complexity_notify_hook(void *object,uint32_t flags)
{
    ++s_notify_probe.calls;complexity_log("notify.delete flags=%u\n",flags);
    complexity_restore(&s_notify_probe);
    return ((void *(TC *)(void *,uint32_t))s_notify_probe.original)(object,flags);
}
static void *TC complexity_dialog_hook(void *object,uint32_t flags)
{
    ++s_dialog_probe.calls;complexity_log("dialog.delete flags=%u\n",flags);
    complexity_restore(&s_dialog_probe);
    return ((void *(TC *)(void *,uint32_t))s_dialog_probe.original)(object,flags);
}
static void complexity_probe(void *object,struct complexity_probe *probe,void *hook)
{
    DWORD old;probe->slot=(void **)PTR(object,0)+1;probe->original=*probe->slot;probe->calls=0;
    if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);
    *probe->slot=hook;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);
}
static void complexity_click(HWND control)
{
    SendMessageA(control,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(5,5));
    SendMessageA(control,WM_LBUTTONUP,0,MAKELPARAM(5,5));world_pump();
}
struct complexity_result {uint8_t descriptors[2][0x28];uint32_t checks[3],mode,callbacks,notify_deletes,dialog_deletes;int selection,count;uint32_t combo_font,combo_label;char rows[2048];};
static void complexity_list(HWND window,struct complexity_result *result)
{
    HWND combo=GetDlgItem(window,0x1ad4);int row,length=0;
    result->count=(int)SendMessageA(combo,CB_GETCOUNT,0,0);result->selection=(int)SendMessageA(combo,CB_GETCURSEL,0,0);
    if(result->count<1||result->count>16)exit(2);
    for(row=0;row<result->count;++row) {
        char text[128];LRESULT size=SendMessageA(combo,CB_GETLBTEXTLEN,row,0);
        if(size<0||size>=128)exit(2);
        SendMessageA(combo,CB_GETLBTEXT,row,(LPARAM)text);
        length+=snprintf(result->rows+length,sizeof(result->rows)-(uint32_t)length,"row=%d [%s]\n",row,text);
        if(length>1900)exit(2);
    }
}
int main(int argc,char **argv)
{
    HMODULE ui,global_module,modules[2];struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH];uint32_t index,cycle,comparisons=0;
    uint32_t cases=argc>2?strtoul(argv[2],NULL,0):16;
    struct complexity_result results[2];
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

    for(cycle=0;cycle<2;++cycle) {
        modules[0]=world_load("orig/English/World.DLL");modules[1]=world_load(argc>1?argv[1]:"bin/world-diff/WORLD_area.dll");
        if(modules[0]==modules[1])return 2;
        for(s_side=0;s_side<2;++s_side) {
            struct world_ui_import *imports=(void *)GetProcAddress(modules[s_side],"ImportTable");
            imports[0].link=s_host_services;imports[1].link=s_global_data;
            linkages[s_side]=(void *)GetProcAddress(modules[s_side],"Linkage");
            if(linkages[s_side]->id!=0x80000006)return 2;
            complexity_owned(world_factory(linkages[s_side],0x1304),modules[s_side]);
        }
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(s_step=0;s_step<11;++s_step) {
            for(s_side=0;s_side<2;++s_side) {
                HWND window;void *object;void **vtable;int accept=s_step!=0&&s_step!=2&&s_step!=9;
                memset(&results[s_side],0,sizeof(results[s_side]));s_current=modules[s_side];complexity_fixture();
                window=(HWND)world_factory(linkages[s_side],0x1304)(0,s_complexity_descriptors[0]);
                if(!IsWindow(window))return 2;
                world_pump();object=world_object(window);s_complexity_object=object;vtable=PTR(object,0);
                complexity_ownership(object,modules[s_side]);
                for(index=0;index<3;++index) {
                    uint32_t expected=U32(s_complexity_descriptors[0],0xc+index*8);
                    /* Descriptor order follows on-screen check controls. */
                    if(!s_side&&U32(object,s_check_offsets[index])!=expected){fprintf(stderr,"initial checkbox witness failed\n");return 2;}
                }
                if(s_step>=3&&s_step<=5) {
                    uint32_t which=s_step-3,previous=U32(object,s_check_offsets[which]);
                    complexity_click(GetDlgItem(window,s_check_ids[which]));
                    if(!s_side&&U32(object,s_check_offsets[which])==previous){fprintf(stderr,"click witness missing\n");return 2;}
                }
                if(s_step==6||s_step==7) {
                    HWND combo=GetDlgItem(window,0x1ad4);int count=(int)SendMessageA(combo,CB_GETCOUNT,0,0);
                    SendMessageA(combo,CB_SETCURSEL,s_step==6?count-1:-1,0);
                    SendMessageA(window,WM_COMMAND,MAKEWPARAM(0x1ad4,CBN_SELCHANGE),(LPARAM)combo);
                }
                if(s_step==7)for(index=0;index<3;++index)complexity_click(GetDlgItem(window,s_check_ids[index]));
                for(index=0;index<3;++index)results[s_side].checks[index]=U32(object,s_check_offsets[index]);
                if(s_step==10) {SendMessageA(GetDlgItem(window,0x1ad4),CB_SHOWDROPDOWN,TRUE,0);world_pump();}
                /* SIMSUI Init retains label/font pointers at combo +0x40/+0x44.
                 * Normalize to object offsets so native binding state is compared
                 * even when a font substitution happens to render alike. */
                results[s_side].combo_label=(uint32_t)((uint8_t *)PTR(object,0x14bc)-(uint8_t *)object);
                results[s_side].combo_font=(uint32_t)((uint8_t *)PTR(object,0x14c0)-(uint8_t *)object);
                if(!s_side&&(results[s_side].combo_font!=0x3e0||results[s_side].combo_label!=0x13c4))return 2;
                complexity_list(window,&results[s_side]);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&s_case==6&&!s_step)world_save_pixels("complexity-baseline",cycle,s_step);
                if(s_step==10) {SendMessageA(GetDlgItem(window,0x1ad4),CB_SHOWDROPDOWN,FALSE,0);world_pump();}
                complexity_probe(PTR(object,0x10dc),&s_notify_probe,complexity_notify_hook);
                if(s_step==1||s_step==2) {
                    MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=s_step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;
                    IsDialogMessageA(window,&key);
                } else if(s_step==8||s_step==9)((void (TC *)(void *))vtable[(s_step==8?0xcc:0xd0)/4])(object);
                else SendMessageA(window,WM_COMMAND,accept?IDOK:IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1||s_notify_probe.calls!=1) {
                    fprintf(stderr,"COMPLEXITY UI DIFFER close case=%u step=%u side=%u notify=%u\n",s_case,s_step,s_side,s_notify_probe.calls);return 1;
                }
                results[s_side].mode=U32(object,0x171c);results[s_side].notify_deletes=s_notify_probe.calls;results[s_side].callbacks=s_callbacks;
                memcpy(results[s_side].descriptors,s_complexity_descriptors,sizeof(s_complexity_descriptors));
                if(s_callbacks!=(uint32_t)accept||U32(object,0x171c)!=(accept?1u:2u)||
                    U32(s_complexity_descriptors[accept&&(s_case&8)?1:0],0)!=(uint32_t)accept){fprintf(stderr,"COMPLEXITY UI DIFFER answer witness\n");return 1;}
                if(accept) {
                    for(index=0;index<3;++index)if(U32(s_complexity_descriptors[0],0xc+index*8)!=results[s_side].checks[index]){fprintf(stderr,"COMPLEXITY UI DIFFER collected checkbox\n");return s_side?1:2;}
                    if((int)U32(s_complexity_descriptors[0],0x24)!=results[s_side].selection){fprintf(stderr,"COMPLEXITY UI DIFFER collected selection\n");return s_side?1:2;}
                }
                complexity_probe(object,&s_dialog_probe,complexity_dialog_hook);DestroyWindow(window);world_pump();
                complexity_restore(&s_dialog_probe);results[s_side].dialog_deletes=s_dialog_probe.calls;
                if(s_dialog_probe.calls!=1){fprintf(stderr,"COMPLEXITY UI DIFFER dialog deletion\n");return 1;}
                ++s_checks;
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||
                s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])) {
                for(s_side=0;s_side<2;++s_side)world_save_pixels("complexity-difference",cycle,s_step);
                fprintf(stderr,"COMPLEXITY UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",
                    cycle,s_case,s_step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),
                    s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side) {
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5){fprintf(stderr,"COMPLEXITY UI DIFFER module lifetime\n");return 1;}
            if(!FreeLibrary(modules[s_side]))return 2;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World complexity native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);
    return 0;
}
