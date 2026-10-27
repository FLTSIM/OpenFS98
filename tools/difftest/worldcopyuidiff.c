/* Actual copy-weather controls; independent MAIN-shaped rows, no live apply. */
#define main world_area_ui_main
#include "worlduidiff.c"
#undef main
static uint8_t s_copy_descriptors[2][0x54], s_copy_rows[4][0xc8];
static char s_copy_names[4][32];
static uint32_t s_step;
static const uint32_t s_check_ids[]={0x1aba,0x1ab9,0x1ab8,0x1ab7,0x1ab6,0x1ab5};
static const uint32_t s_controls[]={0x15c0,0x1688,0x1750,0x1818,0x18e0,0x19a8};
static const uint32_t s_fields[]={0x28,0x38,0x50,0x40,0x30,0x48};
static void copy_fixture(void)
{
    static const uint32_t values[]={0,1,2,0xffffffff,0x80000000,0x1234ffff,65536,65535};
    uint32_t row,index;
    memset(s_copy_descriptors,0,sizeof(s_copy_descriptors));memset(s_copy_rows,0,sizeof(s_copy_rows));
    for(row=0;row<4;++row){snprintf(s_copy_names[row],32,"Area %u fixture",row);PTR(s_copy_rows[row],8)=s_copy_names[row];}
    for(row=0;row<2;++row){
        U32(s_copy_descriptors[row],0)=0xfeed;
        U32(s_copy_descriptors[row],0xc)=4;PTR(s_copy_descriptors[row],0x10)=s_copy_rows;
        U32(s_copy_descriptors[row],0x14)=s_case&1;U32(s_copy_descriptors[row],0x18)=s_case&2;
        U32(s_copy_descriptors[row],0x1c)=s_case%6-1;U32(s_copy_descriptors[row],0x20)=(s_case+2)%6-1;
        for(index=0;index<6;++index)U32(s_copy_descriptors[row],s_fields[index])=values[(s_case+index+row)%8];
    }
    s_callbacks=s_log_length=0;s_logs[s_side][0]=0;
}
static void copy_owned(void *address,HMODULE module)
{
    HMODULE owner;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)address,&owner)||owner!=module){fprintf(stderr,"copy ownership failed\n");exit(2);}
}
static void copy_ownership(void *object,HMODULE module)
{
    static const uint32_t slots[]={4,0x30,0x8c,0xac,0xc4,0xcc,0xd0};
    void **vtable=PTR(object,0);uint32_t *entries,index;void *map;
    copy_owned(vtable,module);
    for(index=0;index<sizeof(slots)/sizeof(slots[0]);++index)copy_owned(vtable[slots[index]/4],module);
    map=((void *(TC *)(void *))vtable[0x30/4])(object);entries=PTR(map,4);
    for(index=0;index<2;++index)copy_owned((void *)entries[index*6+5],module);
    if(entries[12])exit(2);
}
struct copy_probe {void **slot;void *original;uint32_t calls;};
static struct copy_probe s_dialog_probe;
static void copy_restore(struct copy_probe *probe)
{
    DWORD old;if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);
    *probe->slot=probe->original;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);
}
static void *TC copy_dialog_hook(void *object,uint32_t flags)
{
    ++s_dialog_probe.calls;snprintf(s_logs[s_side],8192,"dialog.delete flags=%u\n",flags);
    copy_restore(&s_dialog_probe);
    return ((void *(TC *)(void *,uint32_t))s_dialog_probe.original)(object,flags);
}
static void copy_probe(void *object,struct copy_probe *probe,void *hook)
{
    DWORD old;probe->slot=(void **)PTR(object,0)+1;probe->original=*probe->slot;probe->calls=0;
    if(!VirtualProtect(probe->slot,4,PAGE_READWRITE,&old))exit(2);
    *probe->slot=hook;if(!VirtualProtect(probe->slot,4,old,&old))exit(2);
}

static void copy_click(HWND control)
{
    SendMessageA(control,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(5,5));
    SendMessageA(control,WM_LBUTTONUP,0,MAKELPARAM(5,5));world_pump();
}
struct copy_result {
    uint8_t descriptors[2][0x54];uint32_t checks[6],enabled[6],mode,dialog_deletes,error_focus,error_answer,error_seen;
    uint32_t fonts[2],labels[2],data[2][4];int selection[2],count[2];char rows[2][512];
};
static void copy_lists(HWND window,void *object,struct copy_result *result)
{
    uint32_t side;
    for(side=0;side<2;++side){
        HWND combo=GetDlgItem(window,side?0x1b1f:0x1b1e);uint32_t offset=side?0x1448:0x1490;int row,length=0;
        result->count[side]=(int)SendMessageA(combo,CB_GETCOUNT,0,0);result->selection[side]=(int)SendMessageA(combo,CB_GETCURSEL,0,0);
        if(result->count[side]!=4)exit(2);
        for(row=0;row<4;++row){char text[80];LRESULT size=SendMessageA(combo,CB_GETLBTEXTLEN,row,0);
            if(size<0||size>=80)exit(2);
            SendMessageA(combo,CB_GETLBTEXT,row,(LPARAM)text);
            length+=snprintf(result->rows[side]+length,512-(uint32_t)length,"row=%d [%s]\n",row,text);
            result->data[side][row]=(uint32_t)SendMessageA(combo,CB_GETITEMDATA,row,0);
            if(result->data[side][row]!=(uint32_t)s_copy_rows[row]||strcmp(text,s_copy_names[row])){fprintf(stderr,"COPY UI DIFFER row witness\n");exit(s_side?1:2);}
        }
        result->labels[side]=(uint32_t)((uint8_t *)PTR(object,offset+0x40)-(uint8_t *)object);
        result->fonts[side]=(uint32_t)((uint8_t *)PTR(object,offset+0x44)-(uint8_t *)object);
        if(!s_side&&(result->fonts[side]!=0x3e0||result->labels[side]!=(side?0x12d8u:0x1390u)))exit(2);
    }
}
int main(int argc,char **argv)
{
    HMODULE ui,global_module,modules[2];struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH];uint32_t index,cycle,comparisons=0;
    uint32_t cases=argc>2?strtoul(argv[2],NULL,0):16;
    struct copy_result results[2];
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
            copy_owned(world_factory(linkages[s_side],0x13050001),modules[s_side]);
        }
        world_module_probe(linkages,modules);
        for(s_side=0;s_side<2;++s_side){s_current=modules[s_side];linkages[s_side]->init();}
        for(s_case=0;s_case<cases;++s_case)for(s_step=0;s_step<18;++s_step){
            for(s_side=0;s_side<2;++s_side){
                HWND window,first,second;void *object;void **vtable;int accept=s_step!=0&&s_step!=2&&s_step!=12;
                memset(&results[s_side],0,sizeof(results[s_side]));s_current=modules[s_side];copy_fixture();
                window=(HWND)world_factory(linkages[s_side],0x13050001)(0,s_copy_descriptors[s_step==17]);
                if(!IsWindow(window))return 2;
                world_pump();object=world_object(window);vtable=PTR(object,0);copy_ownership(object,modules[s_side]);
                first=GetDlgItem(window,0x1b1e);second=GetDlgItem(window,0x1b1f);
                for(index=0;index<6;++index){
                    uint32_t expected=U32(s_copy_descriptors[s_step==17],s_fields[index]);
                    uint32_t disabled=((s_case&1)&&(index==2||index==3||index==4||index==5))||((s_case&2)&&(index==1||index==2||index==3||index==5));
                    if(!s_side&&(U32(object,s_controls[index]+0xbc)!=expected||!!IsWindowEnabled(GetDlgItem(window,s_check_ids[index]))==!!disabled)){
                        fprintf(stderr,"initial checkbox witness failed\n");return 2;
                    }
                }
                if(s_step>=3&&s_step<=8){
                    uint32_t which=s_step-3,previous=U32(object,s_controls[which]+0xbc);HWND control=GetDlgItem(window,s_check_ids[which]);
                    if(IsWindowEnabled(control)){
                        copy_click(control);
                        if(!s_side&&U32(object,s_controls[which]+0xbc)==previous){fprintf(stderr,"click witness failed\n");return 2;}
                    }
                }
                if(s_step==10)for(index=0;index<6;++index)if(IsWindowEnabled(GetDlgItem(window,s_check_ids[index])))copy_click(GetDlgItem(window,s_check_ids[index]));
                /* Normal accepts are distinct. Three rejection scripts cover a
                 * first/last duplicate and two missing selections. */
                if(accept){
                    SendMessageA(first,CB_SETCURSEL,s_step==9?-1:0,0);
                    SendMessageA(second,CB_SETCURSEL,s_step>=14&&s_step<=16?(s_step==14?0:s_step==16?3:-1):3,0);
                    if(s_step==15||s_step==16)SendMessageA(first,CB_SETCURSEL,s_step==15?-1:3,0);
                }
                if(s_step==13){SendMessageA(first,CB_SHOWDROPDOWN,TRUE,0);world_pump();}
                for(index=0;index<6;++index){results[s_side].checks[index]=U32(object,s_controls[index]+0xbc);results[s_side].enabled[index]=!!IsWindowEnabled(GetDlgItem(window,s_check_ids[index]));}
                copy_lists(window,object,&results[s_side]);
                s_length=0;s_snapshots[s_side][0]=0;EnumChildWindows(window,world_snapshot,(LPARAM)window);
                BringWindowToTop(window);SetForegroundWindow(window);SetFocus(NULL);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);world_pump();world_pixels(window);
                if(!cycle&&s_case==0&&!s_step)world_save_pixels("copy-baseline",cycle,s_step);
                if(s_step==13){SendMessageA(first,CB_SHOWDROPDOWN,FALSE,0);world_pump();}
                s_error_snapshots[s_side][0]=0;s_error_lengths[s_side]=0;
                if(s_step>=14&&s_step<=16){s_error_owner=window;s_error_ticks=s_error_seen=0;s_error_timer=SetTimer(NULL,0,10,world_error_timer);}
                if(s_step==1||s_step==2){
                    MSG key;memset(&key,0,sizeof(key));key.hwnd=window;key.message=WM_KEYDOWN;key.wParam=s_step==1?VK_RETURN:VK_ESCAPE;key.lParam=1;IsDialogMessageA(window,&key);
                }else if(s_step==11||s_step==12)((void(TC *)(void *))vtable[(s_step==11?0xcc:0xd0)/4])(object);
                else SendMessageA(window,WM_COMMAND,accept?IDOK:IDCANCEL,0);
                if(s_step>=14&&s_step<=16){
                    if(s_error_timer)KillTimer(NULL,s_error_timer);
                    world_pump();
                    results[s_side].error_seen=s_error_seen;results[s_side].error_focus=(uint32_t)GetDlgCtrlID(GetFocus());results[s_side].error_answer=U32(s_copy_descriptors[0],0);
                    if(s_error_seen!=1||!IsWindowVisible(window)||results[s_side].error_answer!=0xfeed||U32(object,0x1a70)!=1){fprintf(stderr,"COPY UI DIFFER rejection side=%u case=%u step=%u seen=%u visible=%d answer=%x mode=%x focus=%x\n",s_side,s_case,s_step,s_error_seen,IsWindowVisible(window),results[s_side].error_answer,U32(object,0x1a70),results[s_side].error_focus);return s_side?1:2;}
                    if(!s_side&&results[s_side].error_focus!=0x1b1f){fprintf(stderr,"focus witness %x\n",results[s_side].error_focus);return 2;}
                    SendMessageA(first,CB_SETCURSEL,0,0);SendMessageA(second,CB_SETCURSEL,3,0);SendMessageA(window,WM_COMMAND,IDOK,0);
                }
                if(!IsWindow(window)||IsWindowVisible(window)||GetWindowLongA(window,GWL_USERDATA)!=1){fprintf(stderr,"COPY UI DIFFER close\n");return 1;}
                results[s_side].mode=U32(object,0x1a70);memcpy(results[s_side].descriptors,s_copy_descriptors,sizeof(s_copy_descriptors));
                if(s_callbacks||U32(object,0x1a70)!=(accept?1u:2u)||U32(s_copy_descriptors[s_step==17],0)!=(uint32_t)accept){fprintf(stderr,"COPY UI DIFFER answer/mode\n");return s_side?1:2;}
                if(accept){
                    for(index=0;index<6;++index)if(U32(s_copy_descriptors[s_step==17],s_fields[index])!=results[s_side].checks[index]){fprintf(stderr,"COPY UI DIFFER collect check\n");return s_side?1:2;}
                    for(index=0;index<2;++index)if((int)U32(s_copy_descriptors[s_step==17],0x1c+4*index)!=(s_step>=14&&s_step<=16?(index?3:0):results[s_side].selection[index])){fprintf(stderr,"COPY UI DIFFER collect selection\n");return s_side?1:2;}
                }
                copy_probe(object,&s_dialog_probe,copy_dialog_hook);DestroyWindow(window);world_pump();copy_restore(&s_dialog_probe);
                results[s_side].dialog_deletes=s_dialog_probe.calls;
                if(s_dialog_probe.calls!=1){fprintf(stderr,"COPY UI DIFFER dialog deletion\n");return 1;}
                ++s_checks;
            }
            if(memcmp(&results[0],&results[1],sizeof(results[0]))||strcmp(s_logs[0],s_logs[1])||strcmp(s_snapshots[0],s_snapshots[1])||
                strcmp(s_error_snapshots[0],s_error_snapshots[1])||s_error_lengths[0]!=s_error_lengths[1]||memcmp(s_error_pixels[0],s_error_pixels[1],s_error_lengths[0])||
                s_pixel_lengths[0]!=s_pixel_lengths[1]||memcmp(s_pixels[0],s_pixels[1],s_pixel_lengths[0])){
                for(s_side=0;s_side<2;++s_side)world_save_pixels("copy-difference",cycle,s_step);
                fprintf(stderr,"COPY UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d\nRETAIL:\n%s%sOURS:\n%s%s",cycle,s_case,s_step,
                    !!memcmp(&results[0],&results[1],sizeof(results[0])),!!strcmp(s_snapshots[0],s_snapshots[1]),!!strcmp(s_logs[0],s_logs[1]),s_snapshots[0],s_logs[0],s_snapshots[1],s_logs[1]);return 1;
            }
            ++comparisons;
        }
        for(s_side=0;s_side<2;++s_side){
            s_current=modules[s_side];linkages[s_side]->deinit();linkages[s_side]->deinit();
            if(s_registrations[s_side]!=(cycle+1)*5||s_releases[s_side]!=(cycle+1)*5){fprintf(stderr,"COPY UI DIFFER module lifetime\n");return 1;}
            if(!FreeLibrary(modules[s_side]))return 2;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(const char *)modules[s_side],&s_current))return 2;
        }
    }
    s_auxiliary=1;s_current=s_alerts;s_alert_linkage->deinit();FreeLibrary(s_alerts);
    printf("World copy native: %u pixel/state/callback pairs, %u witnesses, 2 unload cycles, 0 differ\n",comparisons,s_checks);return 0;
}
