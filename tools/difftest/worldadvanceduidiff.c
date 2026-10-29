/* Independent Advanced Weather owner. Real SIMSUI/MFC, Alerts, unmasked pixels. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
#include <stdarg.h>
static uint8_t s_owners[2][0x408];
static char s_texts[2][35][32];
static uint32_t s_fields[35],s_field_count,s_step,s_error_field,s_replace,s_owner_counts[5];
static uint64_t s_error_mask;
static uint32_t s_special_error;
static uint8_t *s_parent_pixels[2];
static uint32_t s_parent_lengths[2];
static void *s_advanced_object;
static char s_advanced_snapshots[2][32768];
static uint32_t s_advanced_length;
static const uint32_t s_page_offsets[]={0x263c,0x2640,0x2644,0x2648,0x264c};
static const uint32_t s_tab_kinds[]={0,3,1,2,4};
static void advanced_log(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_log_length,8192-s_log_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=8192-s_log_length)exit(2);
    s_log_length+=(uint32_t)length;
}
static int advanced_owner(void *owner,uint32_t slot)
{
    uint32_t index,tab=U32(owner,0x404),layer,base;
    ++s_callbacks;++s_owner_counts[slot==4?0:slot==0x158?1:slot==0x3fc?2:slot==0x400?3:4];
    advanced_log("owner slot=%x owner=%u tab=%u answer=%x\n",slot,owner==(void *)s_owners[1],tab,U32(owner,0));
    if(slot==4){
        for(index=0;index<s_field_count;++index){U32(owner,s_fields[index])=(s_error_field==index+1||(s_error_mask&((uint64_t)1<<index)))?5:0;advanced_log("field=%u [%s]\n",index,(char *)PTR(owner,s_fields[index]+4));}
        if(s_replace){PTR(s_advanced_object,0x1104)=s_owners[1];s_replace=0;advanced_log("owner.replace\n");}
        return s_special_error?0x100e:s_error_field||s_error_mask?-1:0;
    }
    if(slot==0x400){
        base=tab==0?0x1c:tab==1?0x160:0x2f0;layer=U32(owner,base+4);
        U32(owner,base+(tab==0?layer*0x58+0x5c:tab==1?(layer==3?0x140:layer*0x64+0x68):layer*0x2c+0x30))=0;
    }
    return 0;
}
#define OWNER(name,slot) static int SC name(void *owner){return advanced_owner(owner,slot);}
OWNER(advanced_check_owner,4)
OWNER(advanced_cloud_changed,0x158)
OWNER(advanced_add,0x3fc)
OWNER(advanced_delete,0x400)
OWNER(advanced_convert_1,0x3f4)
OWNER(advanced_convert_2,0x3f8)
static void advanced_fixture(void)
{
    uint32_t row,index,layer,field;static const uint32_t checks[]={0,1,2,0x80000000};
    s_field_count=0;
    for(layer=0;layer<3;++layer)for(field=0;field<3;++field)s_fields[s_field_count++]=0x1c+layer*0x58+8+field*0x14;
    for(layer=0;layer<3;++layer)for(field=0;field<4;++field)s_fields[s_field_count++]=0x160+layer*0x64+8+field*0x14;
    for(field=0;field<3;++field)s_fields[s_field_count++]=0x160+0x144+field*0x14;
    for(layer=0;layer<4;++layer)for(field=0;field<2;++field)s_fields[s_field_count++]=0x2f0+layer*0x2c+8+field*0x14;
    s_fields[s_field_count++]=0x2f0+0xc0;
    s_fields[s_field_count++]=0x2f0+0xdc;
    s_fields[s_field_count++]=0x2f0+0xf0;
    /* 34 strings includes Pressure's second record separately below. */
    memset(s_owners,0,sizeof(s_owners));memset(s_texts,0,sizeof(s_texts));
    for(row=0;row<2;++row){
        void *owner=s_owners[row];U32(owner,0)=0xfeed+row;PTR(owner,4)=advanced_check_owner;PTR(owner,8)="Independent Advanced Weather";
        U32(owner,0x10)=s_case%3;U32(owner,0x14)=s_step==13;U32(owner,0x18)=s_step==14;
        PTR(owner,0x158)=advanced_cloud_changed;PTR(owner,0x3fc)=advanced_add;PTR(owner,0x400)=advanced_delete;PTR(owner,0x3f4)=advanced_convert_1;PTR(owner,0x3f8)=advanced_convert_2;
        for(index=0;index<s_field_count;++index){
            uint32_t off=s_fields[index];snprintf(s_texts[row][index],32,"%u",index<24?(index%4==0?1200:20):15+index);
            PTR(owner,off+4)=s_texts[row][index];U32(owner,off+8)=32;PTR(owner,off+12)="0";PTR(owner,off+16)="9999";
        }
        for(layer=0;layer<3;++layer){U32(owner,0x1c+layer*0x58+0x5c)=1;*(uint16_t *)((uint8_t *)owner+0x1c+layer*0x58+0x44)=1;U32(owner,0x1c+layer*0x58+0x48)=layer?6:10;U32(owner,0x160+layer*0x64+0x68)=1;U32(owner,0x160+layer*0x64+0x5c)=layer%4;}
        for(index=0;index<11;++index){*(uint16_t *)((uint8_t *)owner+0x1c+0x110+index*4)=0;*(uint16_t *)((uint8_t *)owner+0x1c+0x112+index*4)=8;}
        U32(owner,0x1c+0x140)=s_case%21;
        U32(owner,0x160+0x140)=s_case&1;
        for(layer=0;layer<4;++layer)U32(owner,0x2f0+layer*0x2c+0x30)=layer<2+(s_case&1);
        U32(owner,0x2f0+0xbc)=checks[s_case%4];U32(owner,0x2f0+0xd8)=checks[(s_case+1)%4];
    }
    for(row=0;row<2;++row){
        if(s_step==18)U32(s_owners[row],0x1c+0xb4)=0;
        if(s_step==19)U32(s_owners[row],0x1c+0x5c)=0;
        if(s_step==57)for(index=0;index<4;++index)U32(s_owners[row],0x2f0+index*0x2c+0x30)=0;
        if(s_step==58){for(index=0;index<3;++index)U32(s_owners[row],0x160+index*0x64+0x68)=0;U32(s_owners[row],0x160+0x140)=0;}
        if(s_step==59)for(index=0;index<3;++index)U32(s_owners[row],0x1c+index*0x58+0x5c)=0;
    }
    s_callbacks=s_log_length=s_error_field=s_replace=s_special_error=0;s_error_mask=0;memset(s_owner_counts,0,sizeof(s_owner_counts));s_logs[s_side][0]=0;
    s_error_snapshots[s_side][0]=0;s_error_lengths[s_side]=0;
}
static void advanced_owned(void *address,HMODULE module)
{
    HMODULE owner;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)address,&owner)||owner!=module){fprintf(stderr,"ADVANCED UI DIFFER ownership %p\n",address);exit(1);}
}
static void advanced_ownership(void *object,HMODULE module,uint32_t kind)
{
    uint32_t index,*entries;void **vtable=PTR(object,0);void *map;
    static const uint32_t slots[]={4,0x30,0x8c,0xd8,0xdc,0xf0};
    advanced_owned(vtable,module);for(index=0;index<(kind?6u:4u);++index)advanced_owned(vtable[slots[index]/4],module);
    if(!kind){advanced_owned(vtable[0xc4/4],module);advanced_owned(vtable[0xcc/4],module);advanced_owned(vtable[0xd0/4],module);advanced_owned(vtable[0xec/4],module);}
    map=((void *(TC *)(void *))vtable[0x30/4])(object);advanced_owned(map,module);entries=PTR(map,4);advanced_owned(entries,module);
    for(index=0;index<32&&entries[index*6];++index)advanced_owned((void *)entries[index*6+5],module);
    if(index==32)exit(2);
}
struct advanced_probe{void **slot;void *original;uint32_t calls;};
static struct advanced_probe s_probes[7],s_tab_close;
static void advanced_restore(struct advanced_probe *probe)
{DWORD old;if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);*probe->slot=probe->original;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);}
static void *advanced_deleted(void *object,uint32_t flags,uint32_t kind)
{++s_probes[kind].calls;advanced_log("delete kind=%u flags=%u\n",kind,flags);if(kind!=6)advanced_restore(&s_probes[kind]);return ((void *(TC *)(void *,uint32_t))s_probes[kind].original)(object,flags);}
#define ADV_DELETE(name,kind) static void *TC name(void *object,uint32_t flags){return advanced_deleted(object,flags,kind);}
ADV_DELETE(advanced_parent_delete,0)
ADV_DELETE(advanced_cloud_delete,1)
ADV_DELETE(advanced_temperature_delete,2)
ADV_DELETE(advanced_visibility_delete,3)
ADV_DELETE(advanced_wind_delete,4)
ADV_DELETE(advanced_pressure_delete,5)
ADV_DELETE(advanced_notify_delete,6)
static void TC advanced_tab_closed(void *object)
{++s_tab_close.calls;advanced_log("tab.close\n");advanced_restore(&s_tab_close);((void(TC *)(void *))s_tab_close.original)(object);}
static void advanced_probe(void *object,uint32_t kind,void *hook)
{DWORD old;struct advanced_probe *probe=&s_probes[kind];probe->slot=(void **)PTR(object,0)+1;probe->original=*probe->slot;probe->calls=0;if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);*probe->slot=hook;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);}
static void advanced_tab(void *object,uint32_t tab)
{
    void *control=(uint8_t *)object+0x1690;
    int(TC *activate)(void *,int,uint32_t)=(void *)GetProcAddress(GetModuleHandleA("SIMSUI.DLL"),"?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z");
    activate(control,(int)tab,0);world_pump();
}
static BOOL CALLBACK advanced_snapshot(HWND window,LPARAM parent)
{
    char cls[80],text[512];RECT rect;int length;
    GetClassNameA(window,cls,sizeof(cls));GetWindowTextA(window,text,sizeof(text));GetWindowRect(window,&rect);MapWindowPoints(NULL,(HWND)parent,(POINT *)&rect,2);
    length=snprintf(s_advanced_snapshots[s_side]+s_advanced_length,32768-s_advanced_length,"id=%x class=%s text=[%s] xy=%ld/%ld/%ld/%ld enable=%d visible=%d limit=%ld\n",GetDlgCtrlID(window),cls,text,rect.left,rect.top,rect.right,rect.bottom,IsWindowEnabled(window),IsWindowVisible(window),!strcmp(cls,"Edit")?(long)SendMessageA(window,EM_GETLIMITTEXT,0,0):0L);
    if(length<0||(uint32_t)length>=32768-s_advanced_length)exit(2);
    s_advanced_length+=(uint32_t)length;return TRUE;
}
static void advanced_click(HWND control)
{SendMessageA(control,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(5,5));SendMessageA(control,WM_LBUTTONUP,0,MAKELPARAM(5,5));world_pump();}
static HWND s_menu_owner;
static uint32_t s_menu_id;
static UINT_PTR s_menu_timer;
static void CALLBACK advanced_menu_timer(HWND window,UINT message,UINT_PTR timer,DWORD tick)
{(void)window;(void)message;(void)timer;(void)tick;KillTimer(NULL,s_menu_timer);s_menu_timer=0;EndMenu();PostMessageA(s_menu_owner,WM_COMMAND,s_menu_id,0);}
struct advanced_result{uint8_t owners[2][0x408];char texts[2][35][32];uint32_t mode,deletes[7],callbacks[5],tab_close,focus,error_seen;uint32_t lists[5],raw[3],owner_raw[3],limits[9],edit_flags[9],edit_labels[9],error_result;char rows[7][2048];};
static void advanced_controls(void **pages,struct advanced_result *result)
{
    uint32_t index;int row,count,length;char text[512];
    static const uint32_t kinds[]={0,3,1,0,0,3,2},offsets[]={0x11a8,0x1208,0x120c,0x1b20,0x20a0,0x21c8,0x129c};
    static const uint32_t limit_kinds[]={0,0,0,1,1,1,4,4,3},limit_controls[]={0x1920,0x1988,0x19f0,0x175c,0x17c4,0x182c,0x140c,0x1474,0x1ff0};
    for(index=0;index<7;++index){
        HWND control=(HWND)U32(pages[kinds[index]],offsets[index]+0x20);count=(int)SendMessageA(control,index<3?LB_GETCOUNT:CB_GETCOUNT,0,0);length=0;
        if(count<0||count>32)exit(2);
        for(row=0;row<count;++row){
            if(SendMessageA(control,index<3?LB_GETTEXTLEN:CB_GETLBTEXTLEN,row,0)>=512)exit(2);
            SendMessageA(control,index<3?LB_GETTEXT:CB_GETLBTEXT,row,(LPARAM)text);
            length+=snprintf(result->rows[index]+length,2048-(uint32_t)length,"%d [%s] data=%ld\n",row,text,(long)SendMessageA(control,index<3?LB_GETITEMDATA:CB_GETITEMDATA,row,0));if(length>=2048)exit(2);
        }
    }
    for(index=0;index<9;++index){
        void *page=pages[limit_kinds[index]],*label=PTR(page,limit_controls[index]+0x40);
        result->limits[index]=U32(page,limit_controls[index]+0x64);
        result->edit_flags[index]=U32(page,limit_controls[index]+0x60);
        result->edit_labels[index]=label?(uint32_t)((uint8_t *)label-(uint8_t *)page):0;
    }
}
int main(int argc,char **argv)
{
    HMODULE ui,global_module,modules[2];struct world_ui_linkage *linkages[2];char directory[MAX_PATH];uint32_t index,cycle,comparisons=0;
    uint32_t cases=argc>2?strtoul(argv[2],NULL,0):8;struct advanced_result results[2];
    if(!cases||cases>1000)return 2;
    setvbuf(stdout,NULL,_IONBF,0);SetErrorMode(SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(world_fault);
    GetFullPathNameA("orig",MAX_PATH,directory,NULL);SetDllDirectoryA(directory);s_mfc=world_load("cd/mfc42.dll");world_application();world_classes();
    ui=world_load("orig/SIMSUI.DLL");s_fll=world_load("orig/ENGLISH.FLL");global_module=world_load("orig/Modules/GLOBAL.DLL");memcpy(s_global_data,GetProcAddress(global_module,"Linkage"),4764);
    for(index=0;index<sizeof(s_host_services);index+=4)PTR(s_host_services,index)=world_unknown;
    PTR(s_host_services,0x20c)=world_allocate;PTR(s_host_services,0x210)=world_reallocate;PTR(s_host_services,0x214)=world_free;PTR(s_host_services,0x10c)=world_modal;PTR(s_host_services,0x298)=world_register;PTR(s_host_services,0x29c)=world_release;PTR(s_host_services,0x2a0)=world_string;
    {struct world_ui_import *imports=(void *)GetProcAddress(ui,"ImportTable");imports[0].link=s_host_services;imports[1].link=s_global_data;}
    s_auxiliary=1;s_alerts=world_load("orig/English/Alerts.DLL");s_current=s_alerts;
    {struct world_ui_import *imports=(void *)GetProcAddress(s_alerts,"ImportTable");imports[0].link=s_host_services;imports[1].link=s_global_data;s_alert_linkage=(void *)GetProcAddress(s_alerts,"Linkage");s_alert_linkage->init();}s_auxiliary=0;
    for(cycle=0;cycle<2;++cycle){
        modules[0]=world_load("orig/English/World.DLL");modules[1]=world_load(argc>1?argv[1]:"bin/world-diff/WORLD_area.dll");if(modules[0]==modules[1])return 2;
        for(s_side=0;s_side<2;++s_side){struct world_ui_import *imports=(void *)GetProcAddress(modules[s_side],"ImportTable");imports[0].link=s_host_services;imports[1].link=s_global_data;linkages[s_side]=(void *)GetProcAddress(modules[s_side],"Linkage");advanced_owned(world_factory(linkages[s_side],0x13040001),modules[s_side]);}
        world_module_probe(linkages,modules);for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(s_step=0;s_step<60;++s_step){
            if(argc>3&&s_step!=(uint32_t)strtoul(argv[3],NULL,0))continue;
            if(!s_case)printf("Advanced UI cycle=%u step=%u\n",cycle,s_step);
            for(s_side=0;s_side<2;++s_side){
                HWND window,page_window;void *object,*pages[5],*page;void **vtable;uint32_t tab=s_step<5?s_step:(s_step-5)%5;int cancel=s_step<5||s_step==6||s_step==8||s_step==11||s_step==13||s_step==14;
                memset(&results[s_side],0,sizeof(results[s_side]));s_current=modules[s_side];advanced_fixture();SetCursorPos(0,0);world_pump();
                window=(HWND)world_factory(linkages[s_side],0x13040001)(0,s_owners[0]);if(!IsWindow(window))return 2;world_pump();object=world_object(window);s_advanced_object=object;vtable=PTR(object,0);U32(object,0x2654)=0x6bad04;
                advanced_ownership(object,modules[s_side],0);for(index=0;index<5;++index){pages[index]=PTR(object,s_page_offsets[index]);advanced_ownership(pages[index],modules[s_side],index+1);advanced_tab(object,index);}
                advanced_owned(PTR(PTR(object,0x2638),0),modules[s_side]);advanced_owned(PTR(PTR(PTR(object,0x2638),0),4),modules[s_side]);
                advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);
                s_callbacks=s_log_length=0;s_logs[s_side][0]=0;memset(s_owner_counts,0,sizeof(s_owner_counts));
                if(s_step>=15&&s_step<=17){tab=s_step-15;advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);SendMessageA(window,WM_COMMAND,0x1aa1,0);world_pump();if(tab){s_menu_owner=window;s_menu_id=tab==1?0x1e62:0x1e61;s_menu_timer=SetTimer(NULL,0,100,advanced_menu_timer);}SendMessageA(window,WM_COMMAND,0x1a92,0);if(s_menu_timer){KillTimer(NULL,s_menu_timer);s_menu_timer=0;}world_pump();}
                if(s_step==18||s_step==19){tab=0;advanced_tab(object,0);page=pages[0];page_window=(HWND)U32(page,0x20);SendMessageA(GetDlgItem(page_window,0x1abf),CB_SETCURSEL,s_step==18?4:10,0);SendMessageA(page_window,WM_COMMAND,MAKEWPARAM(0x1abf,CBN_SELCHANGE),(LPARAM)GetDlgItem(page_window,0x1abf));world_pump();}
                if(s_step==20||s_step==21){tab=s_step==20?2:4;advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);advanced_click(GetDlgItem(page_window,tab==4?0x1ac9:0x1ac8));}
                if(s_step==22||s_step==23)SendMessageA(page_window,0x5f9,s_step==22?0:0x10001,0);
                if(s_step>=32&&s_step<=34){
                    static const uint32_t ids[]={0x1b0f,0x1b15,0x1b13};tab=s_step-32;advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);SendMessageA(GetDlgItem(page_window,ids[tab]),LB_SETCURSEL,1,0);SendMessageA(page_window,WM_COMMAND,MAKEWPARAM(ids[tab],LBN_SELCHANGE),(LPARAM)GetDlgItem(page_window,ids[tab]));world_pump();
                }
                if(s_step==35||s_step==36){uint32_t id=s_step==35?0x1abc:0x1ac1;tab=s_step==35?0:1;advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);SendMessageA(GetDlgItem(page_window,id),CB_SETCURSEL,2,0);SendMessageA(page_window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),(LPARAM)GetDlgItem(page_window,id));world_pump();}
                if(s_step==37){tab=1;advanced_tab(object,tab);page=pages[3];page_window=(HWND)U32(page,0x20);advanced_click(GetDlgItem(page_window,0x1b22));}
                if(s_step>=38&&s_step<=40){
                    static const uint32_t tabs[]={0,2,4},members[]={0x1b14,0x1950,0x1598},ids[]={0x1acb,0x1ac8,0x1ac9};uint32_t script=s_step-38;tab=tabs[script];advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);U32(page,members[script])=0x80000002+s_case;SendMessageA(page_window,WM_COMMAND,ids[script],(LPARAM)GetDlgItem(page_window,ids[script]));
                }
                if(s_step>=41&&s_step<=44){
                    static const uint32_t tabs[]={0,1,2,4},ids[4][4]={{0x1aeb,0x1aec,0x1aee,0},{0x1b09,0x1b0d,0x1b0e,0x1b0c},{0x1ae1,0x1aed,0x1b0a,0},{0x1b03,0x1b04,0,0}};
                    uint32_t script=s_step-41;tab=tabs[script];advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);for(index=0;index<4&&ids[script][index];++index){char text[32];snprintf(text,32,"%u",21+s_case+index);SetWindowTextA(GetDlgItem(page_window,ids[script][index]),text);}
                    if(script==3){((uint32_t(TC *)(void *,uint32_t))PTR(PTR(page,0),0xd8))(page,0x1b03);((uint32_t(TC *)(void *,uint32_t))PTR(PTR(page,0),0xd8))(page,0x1b04);}
                }
                if(s_step==45){tab=3;advanced_tab(object,tab);page=pages[2];page_window=(HWND)U32(page,0x20);SendMessageA(GetDlgItem(page_window,0x1abd),CB_SETCURSEL,(s_case+7)%21,0);SendMessageA(page_window,WM_COMMAND,MAKEWPARAM(0x1abd,CBN_SELCHANGE),(LPARAM)GetDlgItem(page_window,0x1abd));}
                if(s_step>=57){tab=s_step==57?2:s_step==58?1:0;advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];page_window=(HWND)U32(page,0x20);}
                if(s_step>=15&&s_step<=17&&(!s_owner_counts[2]||!s_owner_counts[3])){fprintf(stderr,"ADVANCED UI DIFFER missing Add/Delete callbacks\n");return s_side?1:2;}
                if((s_step==18&&U32(s_owners[0],0x20)!=1)||(s_step==19&&U32(s_owners[0],0x20)!=0)){fprintf(stderr,"ADVANCED UI DIFFER cloud conversion witness\n");return s_side?1:2;}
                results[s_side].owner_raw[0]=U32(s_owners[0],0x1c+(U32(s_owners[0],0x20)+1)*0x58);results[s_side].owner_raw[1]=U32(s_owners[0],0x2f0+0xbc);results[s_side].owner_raw[2]=U32(s_owners[0],0x2f0+0xd8);
                if(s_step>=38&&s_step<=40&&results[s_side].owner_raw[s_step-38]!=0x80000002+s_case){fprintf(stderr,"ADVANCED UI DIFFER raw checkbox witness\n");return s_side?1:2;}
                advanced_controls(pages,&results[s_side]);
                for(index=0;index<5;++index)results[s_side].lists[index]=(uint32_t)SendMessageA((HWND)U32(pages[index],(index==0?0x11a8:index==1?0x120c:index==2?0x129c:index==3?0x1208:0x140c)+0x20),index==2?CB_GETCURSEL:LB_GETCOUNT,0,0);
                results[s_side].raw[0]=U32(pages[0],0x1b14);results[s_side].raw[1]=U32(pages[1],0x1950);results[s_side].raw[2]=U32(pages[4],0x1598);
                s_advanced_length=0;s_advanced_snapshots[s_side][0]=0;EnumChildWindows(window,advanced_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                s_parent_lengths[s_side]=s_pixel_lengths[s_side];s_parent_pixels[s_side]=realloc(s_parent_pixels[s_side],s_parent_lengths[s_side]);if(!s_parent_pixels[s_side])exit(2);memcpy(s_parent_pixels[s_side],s_pixels[s_side],s_parent_lengths[s_side]);
                if(!cycle&&!s_case&&s_step<5)world_save_pixels("advanced-baseline",cycle,s_step);
                {void *hooks[]={advanced_parent_delete,advanced_cloud_delete,advanced_temperature_delete,advanced_visibility_delete,advanced_wind_delete,advanced_pressure_delete};advanced_probe(object,0,hooks[0]);for(index=0;index<5;++index)advanced_probe(pages[index],index+1,hooks[index+1]);advanced_probe(PTR(object,0x2638),6,advanced_notify_delete);}
                {DWORD old;s_tab_close.slot=(void **)PTR((uint8_t *)object+0x1690,0)+0xc4/4;s_tab_close.original=*s_tab_close.slot;s_tab_close.calls=0;if(!VirtualProtect(s_tab_close.slot,4,PAGE_READWRITE,&old))exit(2);*s_tab_close.slot=advanced_tab_closed;VirtualProtect(s_tab_close.slot,4,old,&old);}
                if((s_step>=24&&s_step<=29)||(s_step>=46&&s_step<=56)){
                    tab=s_step==24?0:s_step==25?1:s_step==26||s_step==29?2:4;advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];s_error_field=s_step==24?1+U32(s_owners[0],0x20)*3:s_step==25?(U32(page,0x111c)<3?10+U32(page,0x111c)*4:22):s_step==26?25+U32(page,0x111c)*2:s_step==27?34:s_step==28?35:33;
                    if(s_step>=46){
                        tab=s_step<=47||s_step==52?0:s_step<=50||s_step==53?1:s_step==51||s_step==54||s_step==56?2:4;advanced_tab(object,tab);page=pages[s_tab_kinds[tab]];s_error_field=0;
                        if(s_step==46||s_step==47)s_error_field=1+U32(s_owners[0],0x20)*3+s_step-45;
                        if(s_step>=48&&s_step<=50){uint32_t layer=U32(page,0x111c);s_error_field=(layer<3?10+layer*4:22)+s_step-47;if(layer==3&&s_step==50)s_error_field=22;}
                        if(s_step==51)s_error_field=26+U32(page,0x111c)*2;
                        if(s_step==52)s_error_mask=(uint64_t)7<<(U32(s_owners[0],0x20)*3);
                        if(s_step==53)s_error_mask=(uint64_t)(U32(page,0x111c)<3?15:7)<<(U32(page,0x111c)<3?9+U32(page,0x111c)*4:21);
                        if(s_step==54)s_error_mask=((uint64_t)3<<(24+U32(page,0x111c)*2))|((uint64_t)1<<32);
                        if(s_step==55)s_error_mask=(uint64_t)3<<33;
                        if(s_step==56)s_special_error=1;
                    }
                    s_error_owner=window;s_error_ticks=s_error_seen=0;s_error_timer=SetTimer(NULL,0,10,world_error_timer);SendMessageA(window,WM_COMMAND,IDOK,0);if(s_error_timer)KillTimer(NULL,s_error_timer);world_pump();
                    results[s_side].error_seen=s_error_seen;results[s_side].focus=GetDlgCtrlID(GetFocus());if(s_error_seen!=1||!IsWindowVisible(window)||U32(s_owners[0],0)!=0xfeed){fprintf(stderr,"ADVANCED UI DIFFER error seen=%u step=%u\n",s_error_seen,s_step);return s_side?1:2;}results[s_side].error_result=U32(page,0x8c);s_error_field=s_special_error=0;s_error_mask=0;
                }
                s_replace=s_step==30;
                if(s_step==5||s_step==6){MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=s_step==5?VK_RETURN:VK_ESCAPE;key.lParam=1;IsDialogMessageA(window,&key);}
                else if(s_step==7||s_step==8||s_step==31)((void(TC *)(void *))vtable[(s_step==7?0xcc:s_step==8?0xd0:0xec)/4])(object);
                else SendMessageA(window,WM_COMMAND,cancel?IDCANCEL:IDOK,0);
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1){fprintf(stderr,"ADVANCED UI DIFFER close step=%u\n",s_step);return s_side?1:2;}
                if(s_tab_close.calls!=1){fprintf(stderr,"ADVANCED UI DIFFER tab close\n");return s_side?1:2;}advanced_restore(&s_tab_close);
                results[s_side].mode=U32(object,0x2654);memcpy(results[s_side].owners,s_owners,sizeof(s_owners));memcpy(results[s_side].texts,s_texts,sizeof(s_texts));memcpy(results[s_side].callbacks,s_owner_counts,sizeof(s_owner_counts));results[s_side].tab_close=s_tab_close.calls;
                if(s_step!=31&&U32(s_owners[s_step==30?1:0],0)!=(uint32_t)!cancel){fprintf(stderr,"ADVANCED UI DIFFER answer step=%u\n",s_step);return s_side?1:2;}
                DestroyWindow(window);world_pump();for(index=0;index<7;++index){advanced_restore(&s_probes[index]);results[s_side].deletes[index]=s_probes[index].calls;if(s_probes[index].calls!=(index==6?3u:1u)){fprintf(stderr,"ADVANCED UI DIFFER deletes kind=%u calls=%u\n",index,s_probes[index].calls);return s_side?1:2;}}
                ++s_checks;
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_advanced_snapshots[0],s_advanced_snapshots[1])||s_parent_lengths[0]!=s_parent_lengths[1]||memcmp(s_parent_pixels[0],s_parent_pixels[1],s_parent_lengths[0])||strcmp(s_error_snapshots[0],s_error_snapshots[1])||s_error_lengths[0]!=s_error_lengths[1]||memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0])){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("advanced-difference",cycle,s_step);
                fprintf(stderr,"ADVANCED UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d pixels=%d\nRETAIL:\n%s%sOURS:\n%s%s",cycle,s_case,s_step,!!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_advanced_snapshots[0],s_advanced_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_parent_lengths[0]!=s_parent_lengths[1]||!!memcmp(s_parent_pixels[0],s_parent_pixels[1],s_parent_lengths[0]),s_advanced_snapshots[0],s_logs[0],s_advanced_snapshots[1],s_logs[1]);return 1;
            }++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5)return 1;if(!FreeLibrary(modules[s_side]))return 2;if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;}
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);printf("Advanced Weather native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
