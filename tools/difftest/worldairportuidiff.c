/* Native go-to-airport comparison over independent FACIL-shaped callbacks. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
#include <stdarg.h>
static uint8_t s_airport_descriptor[0xc0];
static uint32_t s_airport_rows[8][5][3],s_airport_calls[10],s_airport_allocated,s_airport_freed,s_nested;
static char s_airport_names[8][5][64],s_airport_selected[128],s_airport_prefix[128],s_open_placeholder[128];
static struct world_ui_linkage *s_airport_linkages[2];
static const uint32_t s_airport_lists[]={0x44,0x50,0x5c,0x68,0x74,0x94,0xa0,0xac};
static const uint32_t s_airport_ids[]={0x1be1,0x1add,0x1be5,0x1ade,0x1beb,0x1bef,0x1be6,0x1bf0};
static void airport_log(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_log_length,sizeof(s_logs[0])-s_log_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=sizeof(s_logs[0])-s_log_length)exit(2);
    s_log_length+=(uint32_t)length;
}
static int SC airport_allocate(int flags,uint32_t size,void **out,int zero)
{++s_airport_allocated;airport_log("allocate %x %x %d\n",flags,size,zero);return world_allocate(flags,size,out,zero);}
static int SC airport_free(void **out)
{++s_airport_freed;airport_log("free live=%u\n",!!*out);return world_free(out);}
static uint32_t SC airport_resource(uint32_t id,char **out)
{
    char text[512];uint32_t length=(uint32_t)world_string(id,text,sizeof(text))+1;
    *out=malloc(length);if(!*out)exit(2);memcpy(*out,text,length);++s_airport_allocated;
    airport_log("resource %x [%s]\n",id,*out);return length;
}
static char *SC airport_copy(char *out,const char *text){airport_log("copy [%s]\n",text);strcpy(out,text);return out;}
static uint32_t SC nested_noop(void *descriptor){(void)descriptor;return 0;}
static uint32_t SC nested_owner(void *descriptor)
{airport_log("nested.owner flags=%x places=%u\n",U32(descriptor,0x20),U32(descriptor,0x10));U32(descriptor,0x24)=U32(descriptor,0x2c)=0;return 0;}
static void airport_nested(void)
{
    uint8_t descriptor[0x34]={0};HWND window;
    PTR(descriptor,4)=PTR(descriptor,8)=nested_noop;PTR(descriptor,12)=nested_owner;
    window=(HWND)world_factory(s_airport_linkages[s_side],0x1307)(0,descriptor);if(!IsWindow(window))exit(2);
    world_pump();SendMessageA(window,WM_COMMAND,IDCANCEL,0);DestroyWindow(window);world_pump();++s_nested;
}
static uint32_t airport_callback(void *descriptor,uint32_t slot)
{
    uint32_t i;++s_airport_calls[slot/4];++s_callbacks;
    airport_log("callback %x mode=%x text=[%s] prefix=[%s]",slot,U32(descriptor,0x28),(char *)PTR(descriptor,0x34),(char *)PTR(descriptor,0x84));
    for(i=0;i<8;++i)airport_log(" %x:%u",s_airport_lists[i],U32(descriptor,s_airport_lists[i]+8));
    airport_log("\n");
    if(slot==0x14)airport_nested();
    if(slot==0x10){U32(descriptor,0x28)=0x1000+(s_case+1)%3;strcpy(s_airport_selected,"refreshed");}
    /* Rows are owned by this independent fixture. World only borrows them. */
    return 0xffffffff;
}
#define AIRPORT_OWNER(n) static uint32_t SC airport_owner_##n(void *descriptor){return airport_callback(descriptor,0x##n);}
AIRPORT_OWNER(4) AIRPORT_OWNER(8) AIRPORT_OWNER(c) AIRPORT_OWNER(10) AIRPORT_OWNER(14) AIRPORT_OWNER(18) AIRPORT_OWNER(1c) AIRPORT_OWNER(20) AIRPORT_OWNER(24)
static void airport_fixture(void)
{
    uint32_t i,r;memset(s_airport_descriptor,0,sizeof(s_airport_descriptor));memset(s_airport_calls,0,sizeof(s_airport_calls));
    s_log_length=s_callbacks=s_airport_allocated=s_airport_freed=s_nested=0;s_logs[s_side][0]=0;s_open_placeholder[0]=0;
    U32(s_airport_descriptor,0)=0x12345678;U32(s_airport_descriptor,0x28)=0x1000+s_case%3;
    PTR(s_airport_descriptor,4)=airport_owner_4;PTR(s_airport_descriptor,8)=airport_owner_8;PTR(s_airport_descriptor,0xc)=airport_owner_c;PTR(s_airport_descriptor,0x10)=airport_owner_10;PTR(s_airport_descriptor,0x14)=airport_owner_14;
    PTR(s_airport_descriptor,0x18)=airport_owner_18;PTR(s_airport_descriptor,0x1c)=airport_owner_1c;PTR(s_airport_descriptor,0x20)=airport_owner_20;PTR(s_airport_descriptor,0x24)=airport_owner_24;
    PTR(s_airport_descriptor,0x34)=s_airport_selected;PTR(s_airport_descriptor,0x84)=s_airport_prefix;strcpy(s_airport_selected,"Initial airport");strcpy(s_airport_prefix,"Initial name");
    for(i=0;i<8;++i){U32(s_airport_descriptor,s_airport_lists[i])=i<4?(s_case+i)%4:1+s_case%4;PTR(s_airport_descriptor,s_airport_lists[i]+4)=s_airport_rows[i];U32(s_airport_descriptor,s_airport_lists[i]+8)=0;
        for(r=0;r<5;++r){snprintf(s_airport_names[i][r],64,"%c fixture %u %u",'B'+r*3,i,r);s_airport_rows[i][r][0]=(uint32_t)s_airport_names[i][r];s_airport_rows[i][r][1]=i*100+r;s_airport_rows[i][r][2]=0x42;}}
}
static void airport_owner_of(void *entry)
{HMODULE owner;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(char *)entry,&owner)||owner!=s_current)exit(2);}
static void airport_ownership(void *object)
{
    static const uint32_t slots[]={4,0x30,0x8c,0xac,0xc4,0xcc,0xd0,0xd8,0xdc};uint32_t i;void **vtable=PTR(object,0);void *map;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i)airport_owner_of(vtable[slots[i]/4]);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);for(i=0;i<26;++i)airport_owner_of(PTR(PTR(map,4),i*24+20));
    if(U32(PTR(map,4),26*24))exit(2);
}
static void airport_notify(HWND window,uint32_t id,uint32_t code){SendMessageA(window,WM_COMMAND,MAKEWPARAM(id,code),(LPARAM)GetDlgItem(window,id));world_pump();}
static void airport_mode(HWND window,uint32_t mode){SendMessageA(GetDlgItem(window,0x1bdd+mode),BM_CLICK,0,0);world_pump();}
static void airport_script(HWND window,uint32_t step)
{
    uint32_t i;void *object=world_object(window);
    if(step>=3&&step<=5)airport_mode(window,step-3);
    if(step>=6&&step<=9){i=step-6;airport_mode(window,0);SendDlgItemMessageA(window,s_airport_ids[i],CB_SETCURSEL,1,0);airport_notify(window,s_airport_ids[i],CBN_SELCHANGE);}
    if(step>=10&&step<=13){i=step-10;airport_mode(window,0);airport_notify(window,s_airport_ids[i],CBN_DROPDOWN);
        if(SendDlgItemMessageA(window,s_airport_ids[i],CB_GETLBTEXTLEN,0,0)>=128)exit(2);
        SendDlgItemMessageA(window,s_airport_ids[i],CB_GETLBTEXT,0,(LPARAM)s_open_placeholder);
        if(!s_side){char expected[128];world_string(0x8001,expected,128);if(strcmp(expected,s_open_placeholder))exit(2);}
        SendDlgItemMessageA(window,s_airport_ids[i],CB_SETCURSEL,0,0);airport_notify(window,s_airport_ids[i],CBN_CLOSEUP);}
    if(step==14){airport_mode(window,1);SetWindowTextA(GetDlgItem(window,0x1bee),s_airport_names[5][0]);}
    if(step==15){airport_mode(window,1);SetWindowTextA(GetDlgItem(window,0x1bee),"C");}
    if(step==16){airport_mode(window,1);SendDlgItemMessageA(window,0x1bef,LB_SETCURSEL,0,0);airport_notify(window,0x1bef,LBN_SELCHANGE);}
    if(step==17){airport_mode(window,0);SendDlgItemMessageA(window,0x1beb,LB_SETCURSEL,0,0);airport_notify(window,0x1beb,LBN_SELCHANGE);}
    if(step==18){airport_mode(window,0);SetWindowTextA(GetDlgItem(window,0x1bea),"Z");}
    if(step==19)SendMessageA(window,WM_COMMAND,0x1be8,0);
    if(step==20)SendMessageA(window,WM_COMMAND,0x1bd9,0);
    if(step==21){airport_mode(window,2);SendDlgItemMessageA(window,0x1bf0,LB_SETCURSEL,0,0);airport_notify(window,0x1bf0,LBN_SELCHANGE);}
    if(!s_side){
        if(U32(object,0x23d8))exit(2);
        if(step==20&&s_nested!=1)exit(2);
        if(step>=3&&step<=5&&U32(s_airport_descriptor,0x28)!=0x1000+step-3)exit(2);
        if(s_airport_allocated<10)exit(2);
        ++s_checks;
    }
}
static void **s_delete_slot;static void *s_delete_original;static uint32_t s_delete_calls;
static void airport_delete_restore(void){DWORD old;if(!VirtualProtect(s_delete_slot,4,PAGE_READWRITE,&old))exit(2);*s_delete_slot=s_delete_original;if(!VirtualProtect(s_delete_slot,4,old,&old))exit(2);}
static void *TC airport_delete_hook(void *object,uint32_t flags){++s_delete_calls;airport_log("post.destroy flags=%u\n",flags);airport_delete_restore();return ((void *(TC *)(void *,uint32_t))s_delete_original)(object,flags);}
static void airport_delete_probe(void *object){DWORD old;void **vtable=PTR(object,0);s_delete_slot=&vtable[1];s_delete_original=*s_delete_slot;s_delete_calls=0;if(!VirtualProtect(s_delete_slot,4,PAGE_READWRITE,&old))exit(2);*s_delete_slot=airport_delete_hook;if(!VirtualProtect(s_delete_slot,4,old,&old))exit(2);}
struct airport_result {uint8_t descriptor[0xc0],tail[0x54];char selected[128],prefix[128],lists[8][1024],open_placeholder[128];uint32_t calls[10],allocated,freed,nested,deletes,radio[3];};
static void airport_lists(HWND window,struct airport_result *result)
{
    uint32_t i,r;for(i=0;i<8;++i){int combo=i<4||i==6;HWND control=GetDlgItem(window,s_airport_ids[i]);LRESULT count=SendMessageA(control,combo?CB_GETCOUNT:LB_GETCOUNT,0,0);int length=snprintf(result->lists[i],1024,"count=%ld selection=%ld",(long)count,(long)SendMessageA(control,combo?CB_GETCURSEL:LB_GETCURSEL,0,0));
        if(count<0||count>20)exit(2);
        for(r=0;r<(uint32_t)count;++r){char text[256];LRESULT size=SendMessageA(control,combo?CB_GETLBTEXTLEN:LB_GETTEXTLEN,r,0);if(size<0||size>=256)exit(2);SendMessageA(control,combo?CB_GETLBTEXT:LB_GETTEXT,r,(LPARAM)text);length+=snprintf(result->lists[i]+length,1024-(uint32_t)length," [%s]",text);if(length>=1024)exit(2);}
    }
}
int main(int argc,char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 12;
    struct airport_result results[2];
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
    PTR(s_host_services,0x20c)=airport_allocate;PTR(s_host_services,0x214)=airport_free;PTR(s_host_services,0x2a4)=airport_resource;PTR(s_host_services,0x2a8)=airport_copy;
    s_auxiliary = 1; s_alerts = world_load("orig/English/Alerts.DLL"); s_current = s_alerts;
    {
        struct world_ui_import *imports = (void *)GetProcAddress(s_alerts, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global_data;
        s_alert_linkage = (void *)GetProcAddress(s_alerts, "Linkage"); s_alert_linkage->init();
    }
    s_auxiliary = 0;


    for(cycle=0;cycle<2;++cycle){
        modules[0]=world_load("orig/English/World.DLL");modules[1]=world_load(argc>1?argv[1]:"bin/world-diff/WORLD_area.dll");if(modules[0]==modules[1])return 2;
        for(s_side=0;s_side<2;++s_side){struct world_ui_import *imports=(void *)GetProcAddress(modules[s_side],"ImportTable");imports[0].link=s_host_services;imports[1].link=s_global_data;linkages[s_side]=(void *)GetProcAddress(modules[s_side],"Linkage");s_airport_linkages[s_side]=linkages[s_side];if(linkages[s_side]->id!=0x80000006)return 2;}
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(step=0;step<24;++step){
            for(s_side=0;s_side<2;++s_side){
                HWND window;void *object,*factory;uint32_t field;
                s_current=modules[s_side];airport_fixture();factory=(void *)world_factory(linkages[s_side],0x1309);airport_owner_of(factory);
                window=(HWND)((uint32_t (SC *)(uint32_t,void *))factory)(0,s_airport_descriptor);if(!IsWindow(window))return 2;
                world_pump();object=world_object(window);airport_ownership(object);airport_script(window,step);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&!s_case&&!step)world_save_pixels("airport-baseline",cycle,step);
                memset(&results[s_side],0,sizeof(results[0]));memcpy(results[s_side].open_placeholder,s_open_placeholder,128);airport_lists(window,&results[s_side]);
                for(field=0;field<3;++field)results[s_side].radio[field]=SendDlgItemMessageA(window,0x1bdd+field,BM_GETCHECK,0,0);
                if(step==1||step==2){MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;IsDialogMessageA(window,&key);}
                if(step==22){SendDlgItemMessageA(window,0x1beb,LB_SETCURSEL,0,0);airport_notify(window,0x1beb,LBN_DBLCLK);}
                if(!GetWindowLongA(window,GWL_USERDATA))SendMessageA(window,WM_COMMAND,step==23?IDOK:IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1)return 2;
                if(!s_side&&U32(s_airport_descriptor,0)!=(step==1||step==22||step==23?1u:0u))exit(2);
                memcpy(results[s_side].descriptor,s_airport_descriptor,sizeof(s_airport_descriptor));memcpy(results[s_side].selected,s_airport_selected,128);memcpy(results[s_side].prefix,s_airport_prefix,128);memcpy(results[s_side].tail,(uint8_t *)object+0x238c,0x54);
                memcpy(results[s_side].calls,s_airport_calls,sizeof(s_airport_calls));results[s_side].allocated=s_airport_allocated;results[s_side].freed=s_airport_freed;results[s_side].nested=s_nested;
                if(!s_side&&s_airport_freed<10)exit(2);
                airport_delete_probe(object);DestroyWindow(window);world_pump();airport_delete_restore();results[s_side].deletes=s_delete_calls;if(!s_side&&s_delete_calls!=1)exit(2);
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("airport-difference",cycle,step);
                fprintf(stderr,"AIRPORT UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d pixels=%d\nRETAIL:\n%s%sOURS:\n%s%s",cycle,s_case,step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_pixel_lengths[0]!=s_pixel_lengths[1]||!!memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5)return 1;if(!FreeLibrary(modules[s_side]))return 2;if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(char *)modules[s_side],&s_current))return 2;}
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World airport native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
