/* Native facilities browser over independent FACIL-shaped owner callbacks. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
#include <stdarg.h>
static uint8_t s_facilities_descriptor[0x34];
static uint32_t s_rows_created,s_rows_freed,s_owner_calls,s_free_calls;
static void facilities_log(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_log_length,sizeof(s_logs[0])-s_log_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=sizeof(s_logs[0])-s_log_length)exit(2);
    s_log_length+=(uint32_t)length;
}
static char *facilities_string(const char *text) {char *result=malloc(strlen(text)+1);if(!result)exit(2);strcpy(result,text);return result;}
static int SC facilities_free(void **pointer)
{++s_free_calls;if(*pointer)++s_rows_freed;free(*pointer);*pointer=NULL;return 0;}
static uint32_t SC facilities_noop(void *descriptor) {(void)descriptor;facilities_log("noop\n");return 0;}
static uint32_t SC facilities_owner(void *descriptor)
{
    uint32_t index,field,flags=U32(descriptor,0x20),count=s_case%3;
    uint8_t *rows;
    ++s_owner_calls;facilities_log("owner flags=%x places=%u",flags,U32(descriptor,0x10));
    for(index=0;index<U32(descriptor,0x10);++index)for(field=0;field<3;++field){
        const char *text=PTR(PTR(descriptor,0x14+field*4),index*4);
        facilities_log(" [%s]",text?text:"");
    }
    facilities_log("\n");
    rows=calloc(count?count:1,flags==1?0x64:0x60);if(!rows)exit(2);++s_rows_created;
    U32(descriptor,flags==1?0x24:0x2c)=count;PTR(descriptor,flags==1?0x28:0x30)=rows;
    /* A zero-count callback still returns an allocation, matching a caller
     * allowed to retain capacity; World does not release that branch. */
    for(index=0;index<count;++index){
        uint8_t *row=rows+index*(flags==1?0x64:0x60);
        static const char *names[]={"Fixture region","Fixture country","Fixture state","Fixture city","Fixture airport"};
        for(field=0;field<5;++field){PTR(row,field*4)=facilities_string(names[field]);++s_rows_created;}
        snprintf((char *)row+0x14,11,"%u.250",110+index);snprintf((char *)row+0x1f,21,"N47 %u",20+index);
        snprintf((char *)row+0x34,21,"W122 %u",10+index);snprintf((char *)row+0x49,8,"%u",500+index);snprintf((char *)row+0x51,11,"NAV%u",index);
        if(s_case&1){((char *)row)[0x14]=0;((char *)row)[0x49]=0;}
        if(flags==1){
            uint32_t end,count_ends=1+s_case%4;uint8_t *ends=calloc(count_ends,0x2c);if(!ends)exit(2);++s_rows_created;
            PTR(row,0x60)=ends;U32(row,0x5c)=count_ends;
            for(end=0;end<count_ends;++end){uint8_t *item=ends+end*0x2c;U32(item,0)=(s_case*3+end)%43;U32(item,4)=(s_case+end)%6;U32(item,8)=(s_case+end)%12;
                snprintf((char *)item+0xc,8,"%u",2500+end);snprintf((char *)item+0x14,11,"%u.500",109+end);snprintf((char *)item+0x1f,11,"%u",end);
                if(s_case&1)((char *)item)[0xc]=0;
            }
        }else U32(row,0x5c)=s_case%16;
    }
    return 0xffffffff;
}
static void facilities_fixture(void)
{
    memset(s_facilities_descriptor,0,sizeof(s_facilities_descriptor));U32(s_facilities_descriptor,0)=0x12345678;
    PTR(s_facilities_descriptor,4)=facilities_noop;PTR(s_facilities_descriptor,8)=facilities_noop;PTR(s_facilities_descriptor,12)=facilities_owner;
    s_log_length=s_owner_calls=s_rows_created=s_rows_freed=s_free_calls=0;s_logs[s_side][0]=0;
}
static void facilities_owner_of(void *entry)
{HMODULE owner;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(char *)entry,&owner)||owner!=s_current)exit(2);}
static void facilities_ownership(void *object)
{
    static const uint32_t slots[]={4,0x30,0x80,0x8c,0xac,0xc4,0xcc,0xd0,0xec};uint32_t index;void **vtable=PTR(object,0);void *map;
    for(index=0;index<sizeof(slots)/sizeof(slots[0]);++index)facilities_owner_of(vtable[slots[index]/4]);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);for(index=0;index<10;++index)facilities_owner_of(PTR(PTR(map,4),index*24+20));
    if(U32(PTR(map,4),10*24))exit(2);
    facilities_owner_of(PTR(PTR(PTR(object,0x1428),0),4));facilities_owner_of(PTR(PTR((uint8_t *)object+0x1724,0),4));
}
static void facilities_enter(HWND window,uint32_t id,uint32_t current)
{
    void *object=world_object(window);U32(object,0x10e8)=id;U32(object,0x10e4)=current;
    SetFocus(GetDlgItem(window,0x1bf0));SendMessageA(window,WM_COMMAND,IDOK,0);world_pump();
}
static void facilities_map_point(HWND window, uint32_t id, int click)
{
    void *object=world_object(window);DIBSECTION bitmap;int x,y,found=0;POINT point;HWND picture=GetDlgItem(window,0x1bf0);
    if(!GetObjectA((HBITMAP)U32(object,0x10e0),sizeof(bitmap),&bitmap)||!bitmap.dsBm.bmBits||bitmap.dsBm.bmBitsPixel!=8)exit(2);
    for(y=0;y<bitmap.dsBm.bmHeight&&!found;++y)for(x=0;x<bitmap.dsBm.bmWidth;++x)if(((uint8_t *)bitmap.dsBm.bmBits)[(bitmap.dsBm.bmHeight-y-1)*bitmap.dsBm.bmWidthBytes+x]==id){found=1;break;}
    if(!found)exit(2);
    point.x=x;point.y=y-1;ClientToScreen(picture,&point);SetForegroundWindow(window);SetActiveWindow(window);
    SendMessageA(window,WM_NCMOUSEMOVE,HTCLIENT,MAKELPARAM(point.x,point.y));
    if(!s_side&&U32(object,0x10e4)!=id){fprintf(stderr,"mouse witness got=%x wanted=%x\n",U32(object,0x10e4),id);exit(2);}
    if(click)SendMessageA(window,WM_NCLBUTTONDOWN,HTCLIENT,MAKELPARAM(point.x,point.y));
}
static void **s_delete_slots[2];static void *s_delete_originals[2];static uint32_t s_delete_counts[2];
static void facilities_restore(uint32_t index)
{DWORD old;if(!VirtualProtect(s_delete_slots[index],4,PAGE_READWRITE,&old))exit(2);*s_delete_slots[index]=s_delete_originals[index];if(!VirtualProtect(s_delete_slots[index],4,old,&old))exit(2);}
static void *facilities_delete(void *object,uint32_t flags,uint32_t index)
{++s_delete_counts[index];facilities_log("delete %u flags=%u\n",index,flags);facilities_restore(index);return ((void *(TC *)(void *,uint32_t))s_delete_originals[index])(object,flags);}
static void *TC facilities_class_delete(void *object,uint32_t flags){return facilities_delete(object,flags,0);}
static void *TC facilities_output_delete(void *object,uint32_t flags){return facilities_delete(object,flags,1);}
static void facilities_delete_probe(void *object)
{uint32_t index;void *objects[]={object,PTR(object,0x1428)};void *hooks[]={facilities_class_delete,facilities_output_delete};
 for(index=0;index<2;++index){DWORD old;s_delete_slots[index]=&((void **)PTR(objects[index],0))[1];s_delete_originals[index]=*s_delete_slots[index];s_delete_counts[index]=0;if(!VirtualProtect(s_delete_slots[index],4,PAGE_READWRITE,&old))exit(2);*s_delete_slots[index]=hooks[index];if(!VirtualProtect(s_delete_slots[index],4,old,&old))exit(2);}}
static uint8_t *s_font_entries[2],s_font_saved[2][5];
static void *s_font_objects[2][2],*s_font_bases[2];static uint32_t s_font_constructs[2][2],s_font_destroys[2][2],s_font_order_length[2];static char s_font_order[2][3];
static void facilities_font_patch(uint32_t index,int hook);
static void *TC facilities_font_construct(void *object,void *base)
{
    void *returned;HMODULE owner;
    facilities_font_patch(0,0);returned=((void *(TC *)(void *,void *))s_font_entries[0])(object,base);facilities_font_patch(0,1);
    if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)object,&owner)&&owner==s_current){uint32_t index;for(index=0;index<2;++index)if(base==s_font_bases[index]){s_font_objects[s_side][index]=object;++s_font_constructs[s_side][index];}}
    return returned;
}
static void TC facilities_font_destroy(void *object)
{uint32_t index;for(index=0;index<2;++index)if(object==s_font_objects[s_side][index]){++s_font_destroys[s_side][index];if(s_font_order_length[s_side]>=2)exit(2);s_font_order[s_side][s_font_order_length[s_side]++]=index?'B':'F';}facilities_font_patch(1,0);((void (TC *)(void *))s_font_entries[1])(object);facilities_font_patch(1,1);}
static void facilities_font_patch(uint32_t index,int hook)
{
    DWORD old;void *targets[]={facilities_font_construct,facilities_font_destroy};uint8_t *entry=s_font_entries[index];
    if(!VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&old))exit(2);
    if(hook){entry[0]=0xe9;*(uint32_t *)(entry+1)=(uint32_t)targets[index]-(uint32_t)entry-5;}else memcpy(entry,s_font_saved[index],5);
    FlushInstructionCache(GetCurrentProcess(),entry,5);if(!VirtualProtect(entry,5,old,&old))exit(2);
}
static void facilities_font_probes(void)
{
    HMODULE ui=GetModuleHandleA("SIMSUI.DLL");uint32_t index;
    s_font_bases[0]=((void *(SC *)(int))(void *)GetProcAddress(ui,"?GetSimsBaseFont@@YGPAVCFont@@H@Z"))(12);s_font_bases[1]=((void *(SC *)(int))(void *)GetProcAddress(ui,"?GetSimsBaseFont@@YGPAVCFont@@H@Z"))(8);
    memset(s_font_objects,0,sizeof(s_font_objects));memset(s_font_constructs,0,sizeof(s_font_constructs));memset(s_font_destroys,0,sizeof(s_font_destroys));memset(s_font_order,0,sizeof(s_font_order));memset(s_font_order_length,0,sizeof(s_font_order_length));
    s_font_entries[0]=(void *)GetProcAddress(ui,"??0CSimsFont@@QAE@PAVCFont@@@Z");s_font_entries[1]=(void *)GetProcAddress(ui,"??1CSimsFont@@UAE@XZ");
    for(index=0;index<2;++index){if(!s_font_entries[index])exit(2);memcpy(s_font_saved[index],s_font_entries[index],5);facilities_font_patch(index,1);}
}
static void facilities_mixed_font(struct world_ui_linkage *linkage)
{
    uint8_t descriptor[0x30]={0},row[0x80]={0};char cache[288]="C:\\fixture\\booster",source[288]="C:\\fixture\\cache",name[128]="Mixed font fixture",description[288]="Independent booster owner";HWND window;
    PTR(descriptor,4)=facilities_noop;PTR(descriptor,8)="Mixed font fixture";PTR(descriptor,12)=row;
    PTR(row,4)=cache;U32(row,8)=sizeof(cache);PTR(row,0x18)=source;U32(row,0x1c)=sizeof(source);PTR(row,0x2c)=name;PTR(row,0x40)=description;U32(row,0x5c)=1024;U32(row,0x74)=1024;PTR(row,0x6c)="100";PTR(row,0x70)="2500";
    s_log_length=0;s_logs[s_side][0]=0;
    window=(HWND)world_factory(linkage,0x13080008)(0,descriptor);if(!IsWindow(window))exit(2);world_pump();SendMessageA(window,WM_COMMAND,IDCANCEL,0);DestroyWindow(window);world_pump();
}
static void facilities_script(HWND window,uint32_t step)
{
    void *object=world_object(window);
    if(step==1) {SetFocus(GetDlgItem(window,0x1bf3));SendMessageA(window,WM_COMMAND,IDOK,0);}
    if(step>=2&&step<14){facilities_enter(window,step==3?0xbf:step==4?0xd6:step==5?0x30:0xc1,step==3?0xbf:step==4?0xd6:step==5?0x30:0xc1);}
    if(step>=6&&step<14&&step!=12&&step!=13){facilities_enter(window,U32(PTR(object,0x10fc),0),0);}
    if(step==7){HWND button=GetDlgItem(window,0x1bf6);SendMessageA(button,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(10,10));SendMessageA(button,WM_LBUTTONUP,0,MAKELPARAM(10,10));}
    if(step==8)SendMessageA(window,WM_COMMAND,0x1bf5,0);
    if(step==9)SendMessageA(window,WM_COMMAND,0x1bf2,0);
    if(step==10){SendMessageA(window,WM_COMMAND,0x1bf1,0);}
    if(step==11){SendMessageA(window,WM_COMMAND,0x1bf2,0);SendMessageA(window,WM_COMMAND,0x1bf2,0);}
    if(step==12){facilities_map_point(window,U32(PTR(object,0x10fc),(U32(object,0x10f8)-1)*20),0);SendMessageA(window,WM_COMMAND,0x10003,0);if(!s_side&&U32(object,0x10e8)!=U32(PTR(object,0x10fc),0))exit(2);}
    if(step==13){facilities_map_point(window,U32(PTR(object,0x10fc),0),0);SendMessageA(window,WM_COMMAND,0x10004,0);if(!s_side&&U32(object,0x10e8)!=U32(PTR(object,0x10fc),(U32(object,0x10f8)-1)*20))exit(2);}
    if(step==14)facilities_map_point(window,0xc1,0);
    if(step==15)facilities_map_point(window,0xc1,1);
    if(step==16){facilities_map_point(window,0xc1,1);facilities_map_point(window,U32(PTR(object,0x10fc),0),1);}
    if(step==17){facilities_map_point(window,0xc1,0);U32(object,0x1c08)=0x20;SendMessageA(window,WM_COMMAND,0x7e91bf0,0);}
    if(!s_side){
        if(step==7&&U32(s_facilities_descriptor,0x20)!=2){fprintf(stderr,"navaid witness flags=%x\n",U32(s_facilities_descriptor,0x20));exit(2);}
        if(step==1&&(!IsWindowVisible(window)||U32(s_facilities_descriptor,0)!=0x12345678))exit(2);
        if(step>=6&&step<14&&step!=12&&step!=13&&s_owner_calls<1)exit(2);
        if(U32(s_facilities_descriptor,0x24)||U32(s_facilities_descriptor,0x2c))exit(2);
        ++s_checks;
    }
}
struct facilities_result {uint32_t history,mode,selection,count,flags,answer,calls,created,freed,frees,deletes[2],cursor_result;uint8_t formats[3][60],paragraphs[2][156];char text[8192];};
int main(int argc,char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 16;
    struct facilities_result results[2];
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, NULL, _IONBF, 0); SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(world_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, NULL); SetDllDirectoryA(directory);
    s_mfc = world_load("cd/mfc42.dll"); world_application(); world_classes();
    ui = world_load("orig/SIMSUI.DLL"); s_fll = world_load("orig/ENGLISH.FLL");
    global_module = world_load("orig/Modules/GLOBAL.DLL"); memcpy(s_global_data, GetProcAddress(global_module, "Linkage"), 4764);
    for (index = 0; index < sizeof(s_host_services); index += 4) PTR(s_host_services, index) = world_unknown;
    PTR(s_host_services, 0x20c) = world_allocate; PTR(s_host_services, 0x210) = world_reallocate; PTR(s_host_services, 0x214) = facilities_free;
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
        for(s_side=0;s_side<2;++s_side){struct world_ui_import *imports=(void *)GetProcAddress(modules[s_side],"ImportTable");imports[0].link=s_host_services;imports[1].link=s_global_data;linkages[s_side]=(void *)GetProcAddress(modules[s_side],"Linkage");if(linkages[s_side]->id!=0x80000006)return 2;}
        world_module_probe(linkages,modules);
        facilities_font_probes();
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        if(cycle==0)for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];facilities_mixed_font(linkages[s_side]);}
        for(s_case=0;s_case<cases;++s_case)for(step=0;step<18;++step){
            for(s_side=0;s_side<2;++s_side){
                HWND window;void *object,*factory;uint32_t field;
                s_current=modules[s_side];facilities_fixture();factory=(void *)world_factory(linkages[s_side],0x1307);facilities_owner_of(factory);
                window=(HWND)((uint32_t (SC *)(uint32_t,void *))factory)(0,s_facilities_descriptor);
                if(!IsWindow(window))return 2;
                world_pump();object=world_object(window);facilities_ownership(object);facilities_script(window,step);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);if(step<14)SetFocus(NULL);RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();
                if(step==14||step==12||step==13){U32(object,0x1424)=1;facilities_map_point(window,step==14?0xc1:U32(object,0x10e8),0);}
                world_pixels(window);
                if(!cycle&&!s_case)world_save_pixels("facilities-baseline",cycle,step);
                memset(&results[s_side],0,sizeof(results[0]));
                results[s_side].cursor_result=(uint32_t)SendMessageA(window,WM_SETCURSOR,(WPARAM)window,MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
                if(!s_side&&results[s_side].cursor_result!=1)exit(2);
                results[s_side].history=U32(object,0x1420);results[s_side].mode=U32(object,0x10f4);results[s_side].selection=U32(object,0x10e8);results[s_side].count=U32(object,0x10f8);
                for(field=0;field<3;++field)memcpy(results[s_side].formats[field],(uint8_t *)object+0x143c+field*60,60);
                /* Paragraph fields not written by retail initialization are not
                 * defined class state; query the actual rich-edit format too. */
                for(field=0;field<2;++field){uint32_t offset=field?0x158c:0x14f0;memcpy(results[s_side].paragraphs[field],(uint8_t *)object+offset,8);memcpy(results[s_side].paragraphs[field]+12,(uint8_t *)object+offset+12,4);memcpy(results[s_side].paragraphs[field]+24,(uint8_t *)object+offset+24,4+(field?20:16));}
                GetWindowTextA((HWND)U32(PTR(object,0x1428),0x20),results[s_side].text,sizeof(results[s_side].text));
                facilities_delete_probe(object);
                SendMessageA(window,WM_COMMAND,IDCANCEL,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1)return 2;
                results[s_side].answer=U32(s_facilities_descriptor,0);results[s_side].calls=s_owner_calls;results[s_side].flags=U32(s_facilities_descriptor,0x20);results[s_side].created=s_rows_created;results[s_side].freed=s_rows_freed;results[s_side].frees=s_free_calls;
                if(!s_side&&(results[s_side].answer||U32(object,0x1db4)!=2))exit(2);
                DestroyWindow(window);world_pump();
                for(field=0;field<2;++field){facilities_restore(field);results[s_side].deletes[field]=s_delete_counts[field];if(!s_side&&s_delete_counts[field]!=1)exit(2);}
                /* Release the capacity that the zero-count callback retained. */
                if(PTR(s_facilities_descriptor,0x28))free(PTR(s_facilities_descriptor,0x28));
                if(PTR(s_facilities_descriptor,0x30))free(PTR(s_facilities_descriptor,0x30));
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("facilities-difference",cycle,step);
                fprintf(stderr,"FACILITIES UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d pixels=%d\nRETAIL:\n%s%s%s\nOURS:\n%s%s%s\n",cycle,s_case,step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),!!memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0]),s_snapshots[0],s_logs[0],results[0].text,s_snapshots[1],s_logs[1],results[1].text);return 1;
            }
            ++comparisons;
        }
        if(cycle==1)for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];facilities_mixed_font(linkages[s_side]);}
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5)return 1;if(!FreeLibrary(modules[s_side]))return 2;
            if(s_font_constructs[s_side][0]!=1||s_font_destroys[s_side][0]!=1||s_font_constructs[s_side][1]!=1||s_font_destroys[s_side][1]!=1||strcmp(s_font_order[s_side],cycle?"BF":"FB")){fprintf(stderr,"FACILITIES UI DIFFER font cycle=%u side=%u constructs=%u/%u destroys=%u/%u order=%s\n",cycle,s_side,s_font_constructs[s_side][0],s_font_constructs[s_side][1],s_font_destroys[s_side][0],s_font_destroys[s_side][1],s_font_order[s_side]);return 1;}if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;}
        facilities_font_patch(0,0);facilities_font_patch(1,0);
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World facilities native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
