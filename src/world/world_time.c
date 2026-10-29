/* Time/clock events: text buffers and calendar conversion belong to MAIN. */
#include "world_time.h"
#define AT(p,n) ((uint8_t *)(p)+(n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot,type) ((type)s_services->resolve(slot))
static const struct fs98_world_time_services *s_services;
void fs98_world_time_set_services(const struct fs98_world_time_services *services){s_services=services;}
const struct fs98_world_time_services *fs98_world_time_get_services(void){return s_services;}
void *fs98_world_time_api(uint32_t slot){return s_services->resolve(slot);}
static int time_send(void *object,uint32_t control,uint32_t message,uint32_t first)
{ return API(0x258b0,int(SC *)(uint32_t,uint32_t,uint32_t,uint32_t))(U32(object,control+0x20),message,first,0); }
static void time_enable(void *object,uint32_t control,int enable)
{ API(0x255c8,int(TC *)(void *,int))(AT(object,control),enable); }
static int time_checked(void *object,uint32_t control)
{
    uint32_t window=U32(object,control+0x20);
    uint32_t value=API(0x2589c,uint32_t(SC *)(uint32_t,int))(window,-21);
    return value==U32(object,control+0x20);
}
static int time_callback(void *object,uint32_t owner_offset,uint32_t callback_offset)
{
    void *owner=PTR(object,owner_offset);
    return ((int(SC *)(void *))PTR(owner,callback_offset))(owner);
}
static void time_set_text(void *object,uint32_t control,uint32_t descriptor,uint32_t field,int inline_text)
{
    void *data=PTR(object,descriptor);
    API(0x257e0,void(TC *)(void *,const char *))(AT(object,control),inline_text?(char *)AT(data,field):PTR(data,field));
}
static void time_set_edit(void *object,uint32_t control,uint32_t descriptor,uint32_t field)
{ API(0x255bc,void(TC *)(void *,const char *))(AT(object,control),PTR(PTR(object,descriptor),field)); }
static void time_get_edit(void *object,uint32_t control,uint32_t descriptor,uint32_t field)
{
    void *data=PTR(object,descriptor);
    API(0x25598,int(TC *)(void *,char *,int))(AT(object,control),PTR(data,field),(int)U32(data,field+4));
}
void fs98_world_time_bind_pages(void *object)
{
    void *page=PTR(object,0x211c);
    PTR(page,0x111c)=AT(PTR(object,0x10dc),0x74);
    PTR(PTR(object,0x211c),0x1120)=PTR(object,0x10dc);
    PTR(PTR(object,0x211c),0x1124)=AT(object,0x1170);
    PTR(PTR(object,0x211c),0x1128)=AT(object,0x126c);
    PTR(PTR(object,0x211c),0x112c)=AT(object,0x1450);
    PTR(PTR(object,0x2118),0x1118)=AT(PTR(object,0x10dc),0x10);
    PTR(PTR(object,0x2118),0x111c)=PTR(object,0x10dc);
    PTR(PTR(object,0x2118),0x1120)=AT(PTR(object,0x10dc),0x74);
    PTR(PTR(object,0x2118),0x1124)=AT(object,0x1170);
    PTR(PTR(object,0x2118),0x1128)=AT(object,0x126c);
    PTR(PTR(object,0x2118),0x112c)=AT(object,0x1450);
    PTR(object,0x2120)=AT(object,0x126c);
}
int fs98_world_time_has_descriptor(void *object)
{
    uint32_t descriptor=U32(object,0x10dc);
    if(descriptor&&descriptor!=0xcdcdcdcd)return 1;
    API(0x257a8,int(*)(void *,const char *,const char *,uint32_t))(PTR(object,0x2128),
        "In CDateAndTime::InitializeData (Frame)","Bad m_pliveInfo = %LX",descriptor);
    return 0;
}
void fs98_world_time_populate(void *object,uint32_t kind)
{
    if(kind==1){
        time_send(object,0x1b80,0x14e,U32(PTR(object,0x1118),0x60));
        time_set_edit(object,0x1ab0,0x1118,0x10);time_set_edit(object,0x1b18,0x1118,0x24);time_set_edit(object,0x1a48,0x1118,0x38);
        time_set_text(object,0x1580,0x1120,4,0);time_set_text(object,0x16f0,0x1120,8,1);time_set_text(object,0x1410,0x1120,0x11,1);
        if(U32(PTR(object,0x1118),8)==1){
            API(0x2573c,int(TC *)(void *,int))(AT(object,0x1954),1);fs98_world_time_radio(object,1,1);
        }else if(U32(PTR(object,0x1118),8)==0){
            API(0x2573c,int(TC *)(void *,int))(AT(object,0x1860),1);fs98_world_time_radio(object,0,1);
        }
    }else{
        time_send(object,0x1ca0,0x14e,U32(PTR(object,0x111c),0x30));
        time_set_edit(object,0x1c38,0x111c,0x68);time_set_edit(object,0x1bd0,0x111c,0x7c);
        time_set_edit(object,0x1b68,0x111c,0x40);time_set_edit(object,0x1b00,0x111c,0x54);
        time_set_text(object,0x1638,0x111c,4,0);time_set_text(object,0x17a8,0x111c,8,1);time_set_text(object,0x14c8,0x111c,0x11,1);
        API(0x2577c,int(TC *)(void *,int))(AT(object,0x1ce8),(int)U32(PTR(object,0x111c),0x90));
        if(U32(PTR(object,0x111c),0x28)==1){
            API(0x2573c,int(TC *)(void *,int))(AT(object,0x1a0c),1);fs98_world_time_radio(object,1,2);
        }else if(U32(PTR(object,0x111c),0x28)==0){
            API(0x2573c,int(TC *)(void *,int))(AT(object,0x1918),1);fs98_world_time_radio(object,0,2);
        }
    }
}
void fs98_world_time_radio(void *object,uint32_t manual,uint32_t kind)
{
    uint32_t index;
    static const uint32_t season_controls[]={0x1a48,0x1b18,0x1ab0,0x1b80,0x11e8,0x1358,0x12a0,0x1130};
    static const uint32_t clock_controls[]={0x1b68,0x1b00,0x1c38,0x1bd0,0x1ca0,0x12a0,0x11e8,0x1410,0x1358,0x1130};
    if(kind==1){
        for(index=0;index<8;++index)time_enable(object,season_controls[index],index==3||index==7?!manual:(int)manual);
        if(time_checked(object,0x1860))U32(PTR(object,0x1118),8)=0;
        else if(time_checked(object,0x1954))U32(PTR(object,0x1118),8)=1;
    }else{
        U32(PTR(object,0x111c),0x28)=manual;
        for(index=0;index<10;++index)time_enable(object,clock_controls[index],index==4||index==9?!manual:(int)manual);
    }
}
int fs98_world_time_validate(void *object,uint32_t id,uint32_t kind)
{
    int result;
    if(!U32(object,0x1100))return 0;
    if(kind==1){
        time_get_edit(object,0x1ab0,0x1118,0x10);time_get_edit(object,0x1b18,0x1118,0x24);time_get_edit(object,0x1a48,0x1118,0x38);
        result=time_send(object,0x1b80,0x147,0);U32(PTR(object,0x1118),0x60)=(uint32_t)result;
        U32(PTR(object,0x1118),8)=time_checked(object,0x1860)?0:1;
        U32(PTR(object,0x111c),0xc)=id==0x1afc?7:id==0x1afb?5:id==0x1afa?6:0;
        result=time_callback(object,0x111c,4);
        if(result){
            void *data=PTR(object,0x1118);
            if(U32(data,0x34))result=0x1afc;
            else if(U32(data,0xc))result=0x1afb;
            else if(U32(data,0x20))result=0x1afa;
        }
        if(!result&&U32(PTR(object,0x111c),0xc)){
            uint32_t offset=time_checked(object,0x1954)?0x114:0x110;
            time_callback(object,0x111c,offset);fs98_world_time_populate(object,1);U32(PTR(object,0x111c),0xc)=0;
        }
    }else{
        time_get_edit(object,0x1c38,0x111c,0x68);time_get_edit(object,0x1bd0,0x111c,0x7c);
        time_get_edit(object,0x1b68,0x111c,0x40);time_get_edit(object,0x1b00,0x111c,0x54);
        result=time_send(object,0x1ca0,0x147,0);U32(PTR(object,0x111c),0x30)=(uint32_t)result;
        U32(PTR(object,0x111c),0x90)=U32(object,0x1da4);
        U32(PTR(object,0x111c),0x28)=time_checked(object,0x1918)?0:1;
        if(id==0x1af8||id==0x1af9){U32(PTR(object,0x1120),0xc)=id==0x1af8?3:4;U32(object,0x1118)=1;}
        else if(id==0x1afd||id==0x1afe){U32(PTR(object,0x1120),0xc)=id==0x1afd?1:2;U32(object,0x1118)=0;}
        else U32(PTR(object,0x1120),0xc)=0;
        result=time_callback(object,0x1120,4);
        if(result){
            void *data=PTR(object,0x111c);
            if(U32(data,0x64))result=0x1af8;
            else if(U32(data,0x78))result=0x1af9;
            else if(U32(data,0x3c))result=0x1afd;
            else if(U32(data,0x50))result=0x1afe;
        }
        if(!result&&U32(PTR(object,0x1120),0xc)){
            if(U32(object,0x1118)){
                time_callback(object,0x1120,0x120);time_set_edit(object,0x1bd0,0x111c,0x7c);time_set_edit(object,0x1b68,0x111c,0x40);
            }else{
                time_callback(object,0x1120,0x11c);time_set_edit(object,0x1c38,0x111c,0x68);time_set_edit(object,0x1bd0,0x111c,0x7c);
            }
            time_set_edit(object,0x1b00,0x111c,0x54);
            time_send(object,0x1ca0,0x14e,U32(PTR(object,0x111c),0x30));U32(PTR(object,0x1120),0xc)=0;
        }
    }
    return result;
}
int fs98_world_time_error(void *object,uint32_t id,uint32_t kind)
{
    uint32_t index, result=id;
    static const uint32_t season_status[]={0x34,0xc,0x20},season_controls[]={0x1a48,0x1ab0,0x1b18},season_ids[]={0x1afc,0x1afb,0x1afa};
    static const uint32_t clock_status[]={0x64,0x78,0x3c,0x50},clock_controls[]={0x1c38,0x1bd0,0x1b68,0x1b00},clock_ids[]={0x1af8,0x1af9,0x1afd,0x1afe};
    const uint32_t *status=kind==1?season_status:clock_status,*controls=kind==1?season_controls:clock_controls,*ids=kind==1?season_ids:clock_ids;
    uint32_t descriptor=kind==1?0x1118:0x111c,count=kind==1?3:4;
    if(!id)return 0;
    for(index=0;index<count;++index){
        void *data=PTR(object,descriptor);
        if(U32(data,status[index])){
            int value=API(0x25778,int(TC *)(void *,void *,void *))(object,AT(object,controls[index]),AT(data,status[index]));
            U32(object,0x8c)=(uint32_t)value;
            if(value)result=ids[index];
            break;
        }
    }
    return ((int(TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,result);
}
void fs98_world_time_selection(void *object,uint32_t kind)
{
    int result=time_send(object,kind==1?0x1b80:0x1ca0,0x147,0);
    U32(PTR(object,kind==1?0x1118:0x111c),kind==1?0x60:0x30)=(uint32_t)result;
    time_callback(object,kind==1?0x111c:0x1120,kind==1?0x110:0x118);fs98_world_time_populate(object,kind);
}
int fs98_world_time_reload(void *object,uint32_t kind){fs98_world_time_populate(object,kind);return 0;}
void fs98_world_time_cancel(void *object)
{
    void *data=PTR(object,0x10dc);void **vtable=PTR(object,0);
    U32(object,0x2124)=2;U32(data,0)=0;((void(TC *)(void *))vtable[0xec/4])(object);
}
void fs98_world_time_accept(void *object)
{
    void *page=API(0x257b8,void *(TC *)(void *))(AT(object,0x1450));
    uint32_t focus=API(0x258bc,uint32_t(SC *)(void))( );
    void *window=API(0x255cc,void *(SC *)(uint32_t))(focus);
    uint32_t id;int status;void **vtable;
    if(window&&API(0x255d4,int(TC *)(void *,void *))(window,fs98_world_time_api(0x255d0)))id=API(0x255d8,uint32_t(TC *)(void *))(window);
    else id=U32(page,0x7d0);
    vtable=PTR(page,0);status=((int(TC *)(void *,uint32_t))vtable[0xd8/4])(page,id);
    if(!status||!((int(TC *)(void *,uint32_t))vtable[0xdc/4])(page,(uint32_t)status)){
        void *data=PTR(object,0x10dc);vtable=PTR(object,0);U32(data,0)=1;((void(TC *)(void *))vtable[0xec/4])(object);
    }
}
void fs98_world_time_close(void *object)
{
    void *tab=AT(object,0x1450);void **vtable=PTR(tab,0);void *notify;
    ((void(TC *)(void *))vtable[0xc4/4])(tab);
    notify=PTR(object,0x2128);
    if(notify){vtable=PTR(notify,0);((void *(TC *)(void *,uint32_t))vtable[1])(notify,1);}
    API(0x257b4,void(TC *)(void *))(object);
}

int fs98_world_time_frame_validate(void *object,uint32_t unused)
{ (void)object;(void)unused;API(0x2551c,int(SC *)(const char *,uint32_t,uint32_t))("CDateAndTime::ValidateData()",0,0);return 0; }
