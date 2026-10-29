/* Drive the real game's windows through ordinary Win32 messages. No injection. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DWORD game_pid;
static HANDLE game_process;
static uint32_t host_base;
static int first_window, first_control, first_menu;

static void json_string(const char *text)
{
    const unsigned char *cursor=(const unsigned char *)text;
    putchar('"');
    for(;*cursor;++cursor){
        if(*cursor=='"'||*cursor=='\\')printf("\\%c",*cursor);
        else if(*cursor<32)printf("\\u%04x",*cursor);
        else putchar(*cursor);
    }
    putchar('"');
}

static DWORD window_pid(HWND window)
{DWORD pid=0;GetWindowThreadProcessId(window,&pid);return pid;}

static LRESULT message(HWND window,UINT id,WPARAM first,LPARAM second)
{
    DWORD_PTR result=0;
    if(!SendMessageTimeoutA(window,id,first,second,SMTO_ABORTIFHUNG,3000,&result)){
        fprintf(stderr,"message timeout hwnd=%lx id=%x error=%lu\n",(unsigned long)window,id,GetLastError());exit(2);
    }
    return (LRESULT)result;
}

static void describe(HWND window)
{
    char text[1024]={0},class_name[128]={0};RECT rect;
    GetClassNameA(window,class_name,sizeof(class_name));
    message(window,WM_GETTEXT,sizeof(text),(LPARAM)text);GetWindowRect(window,&rect);
    printf("{\"hwnd\":%lu,\"parent\":%lu,\"id\":%d,\"visible\":%d,\"enabled\":%d,\"class\":",
        (unsigned long)window,(unsigned long)GetParent(window),GetDlgCtrlID(window),!!IsWindowVisible(window),!!IsWindowEnabled(window));
    json_string(class_name);printf(",\"text\":");json_string(text);
    printf(",\"rect\":[%ld,%ld,%ld,%ld]",rect.left,rect.top,rect.right,rect.bottom);
    if(!strcmp(class_name,"ComboBox"))printf(",\"count\":%ld,\"selection\":%ld",(long)message(window,CB_GETCOUNT,0,0),(long)message(window,CB_GETCURSEL,0,0));
    if(!strcmp(class_name,"ListBox"))printf(",\"count\":%ld,\"selection\":%ld",(long)message(window,LB_GETCOUNT,0,0),(long)message(window,LB_GETCURSEL,0,0));
    if(!strcmp(class_name,"ComboBox")||!strcmp(class_name,"ListBox")){
        int combo=!strcmp(class_name,"ComboBox");
        int32_t index,count=message(window,combo?CB_GETCOUNT:LB_GETCOUNT,0,0);
        printf(",\"rows\":[");
        for(index=0;index<count&&index<64;++index){
            char row[512]={0};int32_t length=message(window,combo?CB_GETLBTEXTLEN:LB_GETTEXTLEN,index,0);
            if(index)putchar(',');
            if(length>=0&&length<(int32_t)sizeof(row))message(window,combo?CB_GETLBTEXT:LB_GETTEXT,index,(LPARAM)row);
            json_string(row);
        }
        putchar(']');
    }
    putchar('}');
}

static BOOL CALLBACK control(HWND window,LPARAM unused)
{(void)unused;if(!first_control)putchar(',');first_control=0;describe(window);return TRUE;}

static void menus(HMENU menu)
{
    int index,count=GetMenuItemCount(menu);char text[256];
    for(index=0;index<count;++index){
        HMENU child=GetSubMenu(menu,index);UINT id=GetMenuItemID(menu,index);
        if(child){menus(child);continue;}
        if(!id||id==(UINT)-1)continue;
        GetMenuStringA(menu,index,text,sizeof(text),MF_BYPOSITION);
        if(!first_menu)putchar(',');
        first_menu=0;
        printf("{\"id\":%u,\"flags\":%u,\"text\":",id,GetMenuState(menu,index,MF_BYPOSITION));json_string(text);putchar('}');
    }
}

static int read_memory(uint32_t address,void *buffer,uint32_t size)
{
    SIZE_T copied=0;
    return address>=0x10000&&ReadProcessMemory(game_process,(void *)(uintptr_t)address,buffer,size,&copied)&&copied==size;
}

/* Wine does not expose another process's HMENU. Read the host's verified,
 * bounded 0x14-byte menu records instead; this never writes game memory. */
static void host_menus(uint32_t items,uint32_t depth)
{
    uint32_t index,record[5];HMODULE strings;
    if(depth>8)return;
    strings=LoadLibraryExA("C:\\FS98\\ENGLISH.FLL",NULL,LOAD_LIBRARY_AS_DATAFILE);
    for(index=0;index<64;++index){
        uint32_t id;char text[256]={0};
        if(!read_memory(items+index*20,record,sizeof(record)))break;
        id=record[0]>>16;if(id==0xffff)break;
        if(record[3]){host_menus(record[3],depth+1);continue;}
        if(id<=1)continue;
        if(record[1]<0x10000)LoadStringA(strings,record[1],text,sizeof(text));
        else read_memory(record[1],text,sizeof(text)-1);
        if(!first_menu)putchar(',');
        first_menu=0;
        printf("{\"id\":%u,\"flags\":%u,\"text\":",id,record[2]);json_string(text);putchar('}');
    }
    if(strings)FreeLibrary(strings);
}

static BOOL CALLBACK window(HWND window,LPARAM unused)
{
    (void)unused;
    if(window_pid(window)!=game_pid||!IsWindowVisible(window))return TRUE;
    if(!first_window)putchar(',');
    first_window=0;
    printf("{\"window\":");describe(window);printf(",\"controls\":[");first_control=1;EnumChildWindows(window,control,0);
    printf("],\"menus\":[");first_menu=1;
    if(GetMenu(window))menus(GetMenu(window));
    if(first_menu){
        char class_name[64];uint32_t items;
        GetClassNameA(window,class_name,sizeof(class_name));
        if(!strcmp(class_name,"FS98MAIN")&&read_memory(host_base+0x182a4,&items,4))host_menus(items,0);
    }
    printf("]}");return TRUE;
}

int main(int argc,char **argv)
{
    HANDLE snapshot;PROCESSENTRY32 process;MODULEENTRY32 module;HWND target;
    HDESK desktop=OpenDesktopA("WorldIntegration",0,FALSE,GENERIC_ALL);
    if(!desktop||!SetThreadDesktop(desktop)){
        fprintf(stderr,"cannot attach to WorldIntegration desktop: %lu\n",GetLastError());return 2;
    }
    snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);memset(&process,0,sizeof(process));process.dwSize=sizeof(process);
    if(Process32First(snapshot,&process))do{
        if(!lstrcmpiA(process.szExeFile,"FLTSIM98.EXE")){
            if(game_pid){fprintf(stderr,"multiple game processes in the test prefix\n");return 2;}
            game_pid=process.th32ProcessID;
        }
    }while(Process32Next(snapshot,&process));CloseHandle(snapshot);
    if(!game_pid){fprintf(stderr,"game process not found\n");return 2;}
    game_process=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,game_pid);
    if(!game_process)return 2;
    if(argc==4&&!strcmp(argv[1],"memory")){
        uint32_t size=strtoul(argv[3],NULL,0),index;uint8_t bytes[8192];
        if(size>sizeof(bytes)||!read_memory(strtoul(argv[2],NULL,0),bytes,size))return 2;
        for(index=0;index<size;++index)printf("%02x",bytes[index]);
        putchar('\n');return 0;
    }
    if(argc>1&&strcmp(argv[1],"snapshot")){
        if(argc<3)return 2;
        target=(HWND)(uintptr_t)strtoul(argv[2],NULL,0);
        if(!IsWindow(target)||window_pid(target)!=game_pid)return 2;
        if(!strcmp(argv[1],"command")&&argc==4)PostMessageA(target,WM_COMMAND,strtoul(argv[3],NULL,0),0);
        else if(!strcmp(argv[1],"text")&&argc==4)message(target,WM_SETTEXT,0,(LPARAM)argv[3]);
        else if(!strcmp(argv[1],"select")&&argc==4){
            char class_name[64];GetClassNameA(target,class_name,sizeof(class_name));
            int combo=!strcmp(class_name,"ComboBox");
            message(target,combo?CB_SETCURSEL:LB_SETCURSEL,strtoul(argv[3],NULL,0),0);
            PostMessageA(GetParent(target),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(target),combo?CBN_SELCHANGE:LBN_SELCHANGE),(LPARAM)target);
        }else if(!strcmp(argv[1],"click")&&argc==3){
            RECT rect;GetClientRect(target,&rect);
            PostMessageA(target,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM((rect.right-rect.left)/2,(rect.bottom-rect.top)/2));
            PostMessageA(target,WM_LBUTTONUP,0,MAKELPARAM((rect.right-rect.left)/2,(rect.bottom-rect.top)/2));
        }else if(!strcmp(argv[1],"close")&&argc==3)PostMessageA(target,WM_CLOSE,0,0);
        else return 2;
        return 0;
    }
    printf("{\"pid\":%lu,\"modules\":[",game_pid);first_window=1;
    snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,game_pid);memset(&module,0,sizeof(module));module.dwSize=sizeof(module);
    if(Module32First(snapshot,&module))do{
        if(!lstrcmpiA(module.szModule,"FLTSIM98.EXE"))host_base=(uint32_t)module.modBaseAddr;
        if(!first_window)putchar(',');
    first_window=0;
        printf("{\"name\":");json_string(module.szModule);printf(",\"path\":");json_string(module.szExePath);printf(",\"base\":%lu}",(unsigned long)module.modBaseAddr);
    }while(Module32Next(snapshot,&module));CloseHandle(snapshot);
    printf("],\"windows\":[");first_window=1;EnumWindows(window,0);printf("]}\n");return 0;
}
