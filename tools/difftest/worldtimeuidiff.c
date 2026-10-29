/* Native time/season tabs with independent MAIN-shaped descriptors. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
#include <stdarg.h>
static uint8_t s_time_descriptors[2][0x124];
static char s_time_texts[2][7][32];
static char s_time_display[]="14:35:59";
static uint32_t s_step,s_errors,s_kind,s_replace_owner,s_owner_calls[6];
static void *s_time_object;
static const uint32_t s_fields[]={0x20,0x34,0x48,0xb4,0xc8,0xdc,0xf0};
static const uint32_t s_ids[]={0x1afb,0x1afa,0x1afc,0x1afd,0x1afe,0x1af8,0x1af9};
static void time_ui_log(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_log_length,8192-s_log_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=8192-s_log_length){exit(2);}s_log_length+=(uint32_t)length;
}
static int time_owner(void *descriptor,uint32_t slot)
{
    uint32_t index;++s_callbacks;++s_owner_calls[slot==4?0:(slot-0x110)/4+1];
    time_ui_log("owner slot=%x descriptor=%u action=%x",slot,descriptor==(void *)s_time_descriptors[1],U32(descriptor,0xc));
    for(index=0;index<7;++index)time_ui_log(" [%s]",(char *)PTR(descriptor,s_fields[index]));
    time_ui_log("\n");
    if(slot==4){
        for(index=0;index<7;++index)U32(descriptor,s_fields[index]-4)=s_errors&(1u<<index)?5:0;
        if(s_replace_owner)PTR(s_time_object,0x10dc)=s_time_descriptors[1];
        return s_errors?-1:0;
    }
    /* Conversion callbacks deliberately change both data and selection. */
    if(slot==0x110||slot==0x114){
        for(index=0;index<3;++index)snprintf(PTR(descriptor,s_fields[index]),32,"%u",index==2?2024+s_case:1+(s_case+index)%12);
        U32(descriptor,0x70)=(s_case+1)%4;
    }else{
        for(index=3;index<7;++index)snprintf(PTR(descriptor,s_fields[index]),32,"%u",(s_case+index)%24);
        U32(descriptor,0xa4)=(s_case+2)%4;
    }
    return 0;
}
#define TIME_OWNER(name,slot) static int SC name(void *descriptor){return time_owner(descriptor,slot);}
TIME_OWNER(time_check,4)
TIME_OWNER(time_season,0x110)
TIME_OWNER(time_date,0x114)
TIME_OWNER(time_day,0x118)
TIME_OWNER(time_local,0x11c)
TIME_OWNER(time_gmt,0x120)
static void time_fixture(void)
{
    static const uint32_t checks[]={0,1,2,0xffffffff,0x80000000,0x1234ffff,65536,65535};
    uint32_t row,index;static const char *values[]={"10","28","2026","14","35","21","35"};
    memset(s_time_descriptors,0,sizeof(s_time_descriptors));memset(s_time_texts,0,sizeof(s_time_texts));
    for(row=0;row<2;++row){
        U32(s_time_descriptors[row],0)=0xfeed+row;PTR(s_time_descriptors[row],4)=time_check;
        PTR(s_time_descriptors[row],0x110)=time_season;PTR(s_time_descriptors[row],0x114)=time_date;PTR(s_time_descriptors[row],0x118)=time_day;
        PTR(s_time_descriptors[row],0x11c)=time_local;PTR(s_time_descriptors[row],0x120)=time_gmt;
        for(index=0;index<7;++index){
            strcpy(s_time_texts[row][index],values[index]);PTR(s_time_descriptors[row],s_fields[index])=s_time_texts[row][index];
            U32(s_time_descriptors[row],s_fields[index]+4)=32;PTR(s_time_descriptors[row],s_fields[index]+8)="0";PTR(s_time_descriptors[row],s_fields[index]+12)="9999";
        }
        U32(s_time_descriptors[row],0x18)=s_case&1;U32(s_time_descriptors[row],0x9c)=(s_case>>1)&1;
        U32(s_time_descriptors[row],0x70)=s_case%4;U32(s_time_descriptors[row],0xa4)=(s_case+2)%4;
        U32(s_time_descriptors[row],0x104)=checks[s_case%8];
        PTR(s_time_descriptors[row],0x78)=s_time_display;strcpy((char *)s_time_descriptors[row]+0x7c,"21:35:59");strcpy((char *)s_time_descriptors[row]+0x85,"No");
    }
    s_replace_owner=0;s_errors=s_callbacks=s_log_length=0;memset(s_owner_calls,0,sizeof(s_owner_calls));s_logs[s_side][0]=0;
    s_error_snapshots[s_side][0]=0;s_error_lengths[s_side]=0;
}
static void time_owned(void *address,HMODULE module)
{
    HMODULE owner;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)address,&owner)||owner!=module){fprintf(stderr,"TIME UI DIFFER ownership\n");exit(1);}
}
static void time_ownership(void *object,HMODULE module,uint32_t kind)
{
    void **vtable=PTR(object,0);uint32_t index,*entries;void *map;
    static const uint32_t parent_slots[]={4,0x30,0x8c,0xac,0xc4,0xcc,0xd0,0xd8,0xec};
    static const uint32_t page_slots[]={4,0x30,0x8c,0xd8,0xdc,0xf0};
    const uint32_t *slots=kind?page_slots:parent_slots;uint32_t count=kind?6:9;
    time_owned(vtable,module);for(index=0;index<count;++index)time_owned(vtable[slots[index]/4],module);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);entries=PTR(map,4);count=kind==1?8:kind==2?7:2;
    for(index=0;index<count;++index)time_owned((void *)entries[index*6+5],module);
    if(entries[count*6]){exit(2);}
}
struct time_probe{void **slot;void *original;uint32_t calls;};
static struct time_probe s_probes[4];
static struct time_probe s_tab_close;
static void time_restore(struct time_probe *probe)
{DWORD old;if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old)){exit(2);}*probe->slot=probe->original;if(!VirtualProtect(probe->slot,4,old,&old)){exit(2);}}
static void *time_deleted(void *object,uint32_t flags,uint32_t kind)
{
    ++s_probes[kind].calls;time_ui_log("delete kind=%u flags=%u\n",kind,flags);time_restore(&s_probes[kind]);
    return ((void *(TC *)(void *,uint32_t))s_probes[kind].original)(object,flags);
}
static void *TC time_parent_delete(void *object,uint32_t flags){return time_deleted(object,flags,0);}
static void *TC time_season_delete(void *object,uint32_t flags){return time_deleted(object,flags,1);}
static void *TC time_clock_delete(void *object,uint32_t flags){return time_deleted(object,flags,2);}
static void *TC time_notify_delete(void *object,uint32_t flags){return time_deleted(object,flags,3);}
static void TC time_tab_closed(void *object)
{
    ++s_tab_close.calls;time_ui_log("tab close\n");time_restore(&s_tab_close);
    ((void(TC *)(void *))s_tab_close.original)(object);
}
static void time_watch_tab(void *object)
{
    DWORD old;s_tab_close.slot=(void **)PTR(object,0)+0xc4/4;
    s_tab_close.original=*s_tab_close.slot;s_tab_close.calls=0;
    if(!VirtualProtect(s_tab_close.slot,4,PAGE_READWRITE,&old)){exit(2);}
    *s_tab_close.slot=time_tab_closed;
    if(!VirtualProtect(s_tab_close.slot,4,old,&old)){exit(2);}
}
static void time_probe(void *object,uint32_t kind,void *hook)
{DWORD old;struct time_probe *probe=&s_probes[kind];probe->slot=(void **)PTR(object,0)+1;probe->original=*probe->slot;probe->calls=0;
 if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old)){exit(2);}*probe->slot=hook;if(!VirtualProtect(probe->slot,4,old,&old)){exit(2);}}
static void time_click(HWND control)
{SendMessageA(control,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(5,5));SendMessageA(control,WM_LBUTTONUP,0,MAKELPARAM(5,5));world_pump();}
static void time_tab(void *object,uint32_t kind)
{
    HMODULE ui=GetModuleHandleA("SIMSUI.DLL");void *tab=(uint8_t *)object+0x1450;
    int (TC *activate)(void *,int,uint32_t)=(void *)GetProcAddress(ui,"?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z");
    activate(tab,kind==1?1:0,0);world_pump();
}
struct time_result{
    uint8_t descriptors[2][0x124];char texts[2][7][32];uint32_t mode,deletes[4],callbacks[6],focus,error_seen,error_answer;
    int selections[2],counts[2];uint32_t enabled[7],check,radio[4],fonts[2],labels[2];char rows[2][512];
};
static void time_controls(void *object,struct time_result *result)
{
    uint32_t index,page_index;void *pages[]={PTR(object,0x2118),PTR(object,0x211c)};
    for(page_index=0;page_index<2;++page_index){
        void *page=pages[page_index];uint32_t off=page_index?0x1ca0:0x1b80;HWND combo=(HWND)U32(page,off+0x20);int row,length=0;
        result->counts[page_index]=(int)SendMessageA(combo,CB_GETCOUNT,0,0);result->selections[page_index]=(int)SendMessageA(combo,CB_GETCURSEL,0,0);
        if(result->counts[page_index]!=4){fprintf(stderr,"TIME UI DIFFER combo count kind=%u count=%d\n",page_index,result->counts[page_index]);exit(s_side?1:2);}
        for(row=0;row<4;++row){char text[128];LRESULT size=SendMessageA(combo,CB_GETLBTEXTLEN,row,0);if(size<0||size>=128){exit(2);}
            SendMessageA(combo,CB_GETLBTEXT,row,(LPARAM)text);length+=snprintf(result->rows[page_index]+length,512-(uint32_t)length,"row=%d [%s]\n",row,text);}
        result->fonts[page_index]=(uint32_t)((uint8_t *)PTR(page,off+0x44)-(uint8_t *)page);
        result->labels[page_index]=(uint32_t)((uint8_t *)PTR(page,off+0x40)-(uint8_t *)page);
        if(result->fonts[page_index]!=0x3e0||result->labels[page_index]!=0x1130){fprintf(stderr,"TIME UI DIFFER combo binding\n");exit(s_side?1:2);}
    }
    for(index=0;index<7;++index)result->enabled[index]=!!IsWindowEnabled(GetDlgItem((HWND)U32(pages[index>=3],0x20),s_ids[index]));
    result->check=U32(pages[1],0x1da4);
    for(index=0;index<4;++index){void *page=pages[index/2];uint32_t off=index<2?(index&1?0x1954:0x1860):(index&1?0x1a0c:0x1918);HWND control=(HWND)U32(page,off+0x20);
        result->radio[index]=GetWindowLongA(control,GWL_USERDATA)==(LONG)control;}
}
int main(int argc,char **argv)
{
    HMODULE ui,global_module,modules[2];struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH];uint32_t index,cycle,comparisons=0;
    uint32_t cases=argc>2?strtoul(argv[2],NULL,0):16;
    struct time_result results[2];
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
        if(modules[0]==modules[1]){return 2;}
        for(s_side=0;s_side<2;++s_side){
            struct world_ui_import *imports=(void *)GetProcAddress(modules[s_side],"ImportTable");
            imports[0].link=s_host_services;imports[1].link=s_global_data;linkages[s_side]=(void *)GetProcAddress(modules[s_side],"Linkage");
            if(linkages[s_side]->id!=0x80000006){return 2;}
            time_owned(world_factory(linkages[s_side],0x1301),modules[s_side]);
        }
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(s_step=0;s_step<28;++s_step){
            for(s_side=0;s_side<2;++s_side){
                HWND window,page_window;void *object,*pages[2],*page;void **vtable;uint32_t initial_mode,owner_index=s_step==26;int accept=s_step!=0&&s_step!=2&&s_step!=4&&s_step!=26&&s_step!=27;
                memset(&results[s_side],0,sizeof(results[s_side]));s_current=modules[s_side];time_fixture();
                if(!SetCursorPos(0,0)){return 2;}world_pump();
                window=(HWND)world_factory(linkages[s_side],0x1301)(0,s_time_descriptors[owner_index]);
                if(!IsWindow(window)){return 2;}world_pump();object=world_object(window);s_time_object=object;vtable=PTR(object,0);
                pages[0]=PTR(object,0x2118);pages[1]=PTR(object,0x211c);
                if(U32(pages[1],0x1da4)!=U32(s_time_descriptors[owner_index],0x104)){fprintf(stderr,"TIME UI DIFFER initial raw check\n");return s_side?1:2;}
                U32(object,0x2124)=0x6bad04;
                time_ownership(object,modules[s_side],0);
                for(index=0;index<2;++index)time_ownership(pages[index],modules[s_side],index+1);
                time_owned(PTR(PTR(object,0x2128),0),modules[s_side]);time_owned(PTR(PTR(PTR(object,0x2128),0),4),modules[s_side]);
                time_tab(object,1);time_tab(object,2);s_callbacks=s_log_length=0;s_logs[s_side][0]=0;memset(s_owner_calls,0,sizeof(s_owner_calls));
                s_kind=(s_step==5||s_step==6||s_step==7||s_step==11||s_step==13||s_step==23||(s_step>=16&&s_step<=18))?1:2;
                time_tab(object,s_kind);page=pages[s_kind-1];page_window=(HWND)U32(page,0x20);initial_mode=U32(object,0x2124);
                if(s_step==6||s_step==8||(s_step>=16&&s_step<=24)){
                    time_click(GetDlgItem(page_window,s_kind==1?0x1b18:0x1b1c));
                    for(index=s_kind==1?0:3;index<(s_kind==1?3u:7u);++index){char text[32];snprintf(text,32,"%u",index==2?2030+s_case:2+s_case+index);SetWindowTextA(GetDlgItem(page_window,s_ids[index]),text);}
                }
                if(s_step==7||s_step==9){
                    uint32_t id=s_kind==1?0x1adc:0x1add;HWND combo=GetDlgItem(page_window,id);
                    SendMessageA(combo,CB_SETCURSEL,(s_case+1)%4,0);SendMessageA(page_window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),(LPARAM)combo);world_pump();
                }
                if(s_step==10)time_click(GetDlgItem(page_window,0x1ace));
                if(s_step==13||s_step==14)SendMessageA(page_window,0x5f9,0,0);
                if(s_step==15){time_tab(object,1);time_tab(object,2);}
                if(s_step==11||s_step==12){SendMessageA(GetDlgItem(page_window,s_kind==1?0x1adc:0x1add),CB_SHOWDROPDOWN,TRUE,0);world_pump();}
                time_controls(object,&results[s_side]);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&!s_case&&!s_step)world_save_pixels("time-baseline",cycle,s_step);
                if(s_step==11||s_step==12){SendMessageA(GetDlgItem(page_window,s_kind==1?0x1adc:0x1add),CB_SHOWDROPDOWN,FALSE,0);world_pump();}
                time_probe(object,0,time_parent_delete);time_probe(pages[0],1,time_season_delete);time_probe(pages[1],2,time_clock_delete);time_probe(PTR(object,0x2128),3,time_notify_delete);
                time_watch_tab((uint8_t *)object+0x1450);
                if((s_step>=16&&s_step<=24)){
                    s_errors=s_step<=22?1u<<(s_step-16):s_step==23?7:0x78;
                    s_error_owner=window;s_error_ticks=s_error_seen=0;s_error_timer=SetTimer(NULL,0,10,world_error_timer);
                    SendMessageA(window,WM_COMMAND,IDOK,0);if(s_error_timer)KillTimer(NULL,s_error_timer);world_pump();
                    results[s_side].error_seen=s_error_seen;results[s_side].focus=GetDlgCtrlID(GetFocus());results[s_side].error_answer=U32(s_time_descriptors[owner_index],0);
                    if(s_error_seen!=1||!IsWindowVisible(window)||results[s_side].error_answer!=0xfeed+owner_index||s_probes[1].calls||s_probes[2].calls||s_probes[3].calls){fprintf(stderr,"TIME UI DIFFER rejection seen=%u focus=%x\n",s_error_seen,results[s_side].focus);return s_side?1:2;}
                    if(!s_side&&results[s_side].focus!=(s_step<=22?s_ids[s_step-16]:s_step==23?0x1afc:0x1af8)){fprintf(stderr,"TIME rejection focus witness step=%u focus=%x\n",s_step,results[s_side].focus);return 2;}
                    s_errors=0;
                }
                s_replace_owner=s_step==25;
                if(s_step==1||s_step==2){MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=s_step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;IsDialogMessageA(window,&key);}
                else if(s_step==3||s_step==4||s_step==27)((void(TC *)(void *))vtable[(s_step==3?0xcc:s_step==4?0xd0:0xec)/4])(object);
                else SendMessageA(window,WM_COMMAND,accept?IDOK:IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1){fprintf(stderr,"TIME UI DIFFER close step=%u\n",s_step);return 1;}
                time_restore(&s_tab_close);
                if(s_tab_close.calls!=1){fprintf(stderr,"TIME UI DIFFER tab close count=%u\n",s_tab_close.calls);return s_side?1:2;}
                results[s_side].mode=U32(object,0x2124);memcpy(results[s_side].descriptors,s_time_descriptors,sizeof(s_time_descriptors));memcpy(results[s_side].texts,s_time_texts,sizeof(s_time_texts));memcpy(results[s_side].callbacks,s_owner_calls,sizeof(s_owner_calls));
                if(s_step!=27&&(U32(s_time_descriptors[s_step==25?1:owner_index],0)!=(uint32_t)accept||results[s_side].mode!=(accept?initial_mode:2u))){fprintf(stderr,"TIME UI DIFFER answer/mode\n");return s_side?1:2;}
                if(accept&&!s_owner_calls[0]){fprintf(stderr,"TIME UI DIFFER missing owner validation\n");return 1;}
                DestroyWindow(window);world_pump();for(index=0;index<4;++index){time_restore(&s_probes[index]);results[s_side].deletes[index]=s_probes[index].calls;
                    if(s_probes[index].calls!=1){fprintf(stderr,"TIME UI DIFFER deletion kind=%u count=%u\n",index,s_probes[index].calls);return 1;}}
                ++s_checks;
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||
                strcmp(s_error_snapshots[0],s_error_snapshots[1])||s_error_lengths[0]!=s_error_lengths[1]||memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0])||
                s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("time-difference",cycle,s_step);
                fprintf(stderr,"TIME UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",cycle,s_case,s_step,
                    !!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5){fprintf(stderr,"TIME UI DIFFER module lifetime\n");return 1;}
            if(!FreeLibrary(modules[s_side])){return 2;}
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current)){return 2;}
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World time native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
