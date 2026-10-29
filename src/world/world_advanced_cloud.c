/* Clouds, including retail's layer promotion/demotion and list rebuild order. */
#include "world_advanced_internal.h"
#include "world_unwind.h"
#include "world_advanced_data.inc"
#define WORD(p,n) (*(uint16_t *)AT(p,n))
struct cloud_string {struct fs98_world_unwind frame;void *text;};
static void cloud_string_cleanup(struct fs98_world_unwind *frame)
{struct cloud_string *string=(void *)frame;API(0x255c0,void(TC *)(void *))(&string->text);}
static uint32_t cloud_selected(void *object){return U32(PTR(object,0x1190),4);}
static void cloud_changed(void *object)
{void *data=PTR(object,0x1190);((void(SC *)(void *))PTR(data,0x13c))(PTR(object,0x1194));}
static void cloud_select(void *object,uint32_t layer)
{uint32_t index;for(index=0;index<3;++index)if(advanced_send(object,0x11a8,0x199,index,0)==layer){advanced_send(object,0x11a8,0x186,index,0);break;}}
static uint32_t cloud_type(uint32_t index){return s_cloud_types[index+1];}
static void cloud_amounts(void *object)
{
    uint32_t layer=cloud_selected(object),index,low=0,high=2;void *data;char text[39];
    advanced_send(object,0x1b20,0x14b,0,0);
    if(!layer){
        for(index=0;index<3;++index){fs98_world_advanced_services()->ui->load_string(U32(object,0x80),s_cloud_amount_titles[index],text,39);advanced_send(object,0x1b20,0x143,0,(uint32_t)text);}
        advanced_send(object,0x1b20,0x14e,WORD(PTR(object,0x1190),0x44),0);return;
    }
    data=PTR(object,0x1190);index=U32(data,layer*0x58+0x48);low=WORD(data,0x110+index*4);high=WORD(data,0x112+index*4);
    for(index=low;index<=high;++index){fs98_world_advanced_services()->ui->load_string(U32(object,0x80),index+0xf8e2,text,39);advanced_send(object,0x1b20,0x143,0,(uint32_t)text);}
    advanced_send(object,0x1b20,0x14e,WORD(PTR(object,0x1190),layer*0x58+0x44)-low,0);
}
static void cloud_types(void *object)
{
    uint32_t layer=cloud_selected(object);char text[39];
    advanced_send(object,0x20a0,0x144,10,0);
    if(!layer||!U32(PTR(object,0x1190),0x5c)){
        fs98_world_advanced_services()->ui->load_string(U32(object,0x80),0xf8ee,text,39);advanced_send(object,0x20a0,0x143,0,(uint32_t)text);
    }
}
static void cloud_enable(void *object)
{
    uint32_t index,count=0,value,layer;void *parent;
    static const uint32_t disabled[]={0x1920,0x1988,0x19f0,0x11f0,0x12a8,0x1360,0x1a58,0x1b20,0x1b68,0x20a0,0x11a8,0x1868,0x17b0,0x1640,0x16f8,0x1fe8,0x20e8};
    static const uint32_t enabled[]={0x1920,0x11f0,0x1418,0x1988,0x12a8,0x14d0,0x19f0,0x1360,0x1588,0x1a58,0x1b20,0x1868,0x1b68,0x17b0,0x16f8,0x1640,0x20a0,0x1fe8,0x11a8,0x20e8};
    if(U32(PTR(object,0x1194),0x14)){for(index=0;index<17;++index)advanced_enable(object,disabled[index],0);advanced_buttons(PTR(object,0x11a0),2);return;}
    for(index=0;index<3;++index)if(U32(PTR(object,0x1190),0x5c+index*0x58))++count;
    for(index=0;index<20;++index)advanced_enable(object,enabled[index],count!=0);
    advanced_buttons(PTR(object,0x11a0),-2);
    if(!count)advanced_buttons(PTR(object,0x11a0),0);
    if(count==3){advanced_buttons(PTR(object,0x11a0),1);if(!cloud_selected(object)){advanced_enable(object,0x20a0,0);advanced_enable(object,0x1fe8,0);}}
    if((int)count>0){value=cloud_type(advanced_send(object,0x20a0,0x147,0,0));if((int)value>0&&(int)value<=5){advanced_enable(object,0x1920,0);advanced_enable(object,0x11f0,0);advanced_enable(object,0x1418,0);}}
    parent=PTR(object,0x11a0);layer=count==1?U32(PTR(object,0x1190),0x5c):0;
    API(0x2570c,int(TC *)(void *,void *,int))(AT(parent,0x1690),PTR(parent,0x2644),count&&!(count==1&&layer));
}
static void cloud_populate(void *object)
{
    uint32_t row=0,layer;
    if(!advanced_send(object,0x11a8,0x18b,0,0)){advanced_text(object,0x1920,"");advanced_text(object,0x1988,"");advanced_text(object,0x19f0,"");cloud_enable(object);return;}
    layer=cloud_selected(object);row=layer*0x58;
    API(0x25710,void(TC *)(void *,uint32_t))(AT(object,0x1b68),U32(PTR(object,0x1190),row+0x50));
    advanced_text(object,0x19f0,PTR(PTR(object,0x1190),row+0xc));advanced_text(object,0x1988,PTR(PTR(object,0x1190),row+0x20));advanced_text(object,0x1920,PTR(PTR(object,0x1190),row+0x34));
    advanced_check(object,0x1a58,U32(PTR(object,0x1190),(layer+1)*0x58));cloud_types(object);
    advanced_send(object,0x20a0,0x14e,U32(PTR(object,0x1190),row+0x48),0);cloud_amounts(object);cloud_enable(object);
}
static WORLD_NOINLINE void cloud_list(void *object)
{
    struct cloud_string string;int index;uint32_t row,amount,type;char text[39];void *data;
    API(0x255b4,void(TC *)(void *))(&string.text);world_unwind_enter(&string.frame,cloud_string_cleanup);
    advanced_send(object,0x11a8,0x184,0,0);
    for(index=2;index>=0;--index){
        row=(uint32_t)index*0x58;data=PTR(object,0x1190);
        if(!U32(data,row+0x5c))continue;
        API(0x25578,void *(TC *)(void *,const char *))(&string.text,PTR(data,row+0xc));API(0x2557c,void *(TC *)(void *,int))(&string.text,9);
        API(0x25580,void *(TC *)(void *,const char *))(&string.text,PTR(PTR(object,0x1190),row+0x20));API(0x2557c,void *(TC *)(void *,int))(&string.text,9);
        API(0x25580,void *(TC *)(void *,const char *))(&string.text,PTR(PTR(object,0x1190),row+0x34));API(0x2557c,void *(TC *)(void *,int))(&string.text,9);
        amount=WORD(PTR(object,0x1190),row+0x44);if(amount>=9){API(0x25898,int(SC *)(uint32_t))(0);WORD(PTR(object,0x1190),row+0x44)=0;}
        data=PTR(object,0x1190);amount=WORD(data,row+0x44);type=U32(data,row+0x48);
        fs98_world_advanced_services()->ui->load_string(U32(object,0x80),type==10?s_cloud_amount_titles[amount]:amount+0xf8e2,text,39);
        API(0x25580,void *(TC *)(void *,const char *))(&string.text,text);API(0x25580,void *(TC *)(void *,const char *))(&string.text,"\t");
        type=U32(PTR(object,0x1190),row+0x48);
        if(type==10)fs98_world_advanced_services()->ui->load_string(U32(object,0x80),0xf8ee,text,39);else advanced_send(object,0x20a0,0x148,type,(uint32_t)text);
        API(0x25580,void *(TC *)(void *,const char *))(&string.text,text);API(0x25580,void *(TC *)(void *,const char *))(&string.text,"\t");
        API(0x25580,void *(TC *)(void *,const char *))(&string.text,(char *)AT(object,0x1118+(uint32_t)index*0x28));
        amount=advanced_send(object,0x11a8,0x180,0,(uint32_t)string.text);advanced_send(object,0x11a8,0x19a,amount,(uint32_t)index);
    }
    advanced_send(object,0x11a8,0x197,0,0);if(advanced_send(object,0x11a8,0x18b,0,0))cloud_select(object,cloud_selected(object));
    world_unwind_leave(&string.frame);cloud_string_cleanup(&string.frame);
}
static void cloud_initial_selection(void *object)
{
    uint32_t index;for(index=0;index<3;++index)if(U32(PTR(object,0x1190),0x5c+index*0x58)){U32(PTR(object,0x1190),4)=index;break;}
    cloud_list(object);if(!advanced_send(object,0x11a8,0x18b,0,0)){advanced_send(object,0x1b20,0x14e,0,0);advanced_send(object,0x20a0,0x14e,0,0);}cloud_populate(object);
}
static void cloud_init(void *object)
{
    const struct fs98_world_init_services *ui=fs98_world_advanced_services()->ui;int tabs[]={0x2d,0x55,0x7d,0xd7,0x122};char text[29];uint32_t index;
    static const uint32_t labels[]={0x17b0,0x1640,0x16f8,0x12a8,0x1360,0x11f0,0x14d0,0x1588,0x1418};
    if(!PTR(object,0x1190)||U32(object,0x1190)==0xcdcdcdcd){API(0x2551c,int(SC *)(const char *,uint32_t,uint32_t))("Bad m_pliveInfo in Clouds",0,0);return;}
    U32(object,0x84)=11;API(0x25714,int(TC *)(void *,int,const int *))(AT(object,0x450),5,tabs);
    advanced_label(object,0x20e8,0x370);API(0x25718,void(TC *)(void *,void *,void *))(AT(object,0x11a8),AT(object,0x450),AT(object,0x20e8));
    advanced_label(object,0x1fe8,0x370);ui->combo(AT(object,0x20a0),AT(object,0x3e0),AT(object,0x1fe8));advanced_send(object,0x20a0,0x14e,0,0);
    advanced_label(object,0x1868,0x370);ui->combo(AT(object,0x1b20),AT(object,0x3e0),AT(object,0x1868));advanced_send(object,0x1b20,0x14e,0,0);
    API(0x2571c,void(TC *)(void *,uint32_t,uint32_t,uint32_t,uint32_t,int,int,int,int,int,int,uint32_t))(AT(object,0x1b68),0x135,0x136,U32(object,0x10fc),U32(object,0x10dc),6,0x1a,6,8,1,-1,fs98_world_advanced_services()->resources());
    for(index=0;index<9;++index)advanced_label(object,labels[index],0x220);
    advanced_edit_init(object,0x1920,0x11f0,0x11,4);advanced_edit_init(object,0x1988,0x12a8,0x11,6);advanced_edit_init(object,0x19f0,0x1360,0x11,6);advanced_sprite(object,0x1a58,0x139,0x4c0);
    for(index=0;index<3;++index)ui->load_string(U32(object,0x80),index==0?0xf8db:index==1?0xf8d9:0xf8d7,(char *)AT(object,0x1118+index*0x28),39);
    ui->load_string(U32(object,0x80),U32(PTR(object,0x1194),0x10)<=1?0xf8f5:0xf8f9,text,29);
    advanced_title(object,0x14d0,text);advanced_title(object,0x1588,text);advanced_title(object,0x1418,text);cloud_initial_selection(object);API(0x258c0,int(SC *)(int))(9);
}
/* The original copies text with forward dword/byte moves and reloads owner. */
static void cloud_copy(void *object,uint32_t from,uint32_t to)
{
    uint32_t index,length;const char *source;char *target;
    for(index=0;index<3;++index){source=PTR(PTR(object,0x1190),from*0x58+0xc+index*0x14);target=PTR(PTR(object,0x1190),to*0x58+0xc+index*0x14);for(length=0;source[length];++length){}++length;while(length>=4){*(uint32_t *)target=*(const uint32_t *)source;target+=4;source+=4;length-=4;}while(length--)*target++=*source++;}
    WORD(PTR(object,0x1190),to*0x58+0x44)=WORD(PTR(object,0x1190),from*0x58+0x44);
    U32(PTR(object,0x1190),to*0x58+0x50)=U32(PTR(object,0x1190),from*0x58+0x50);
    U32(PTR(object,0x1190),(to+1)*0x58)=U32(PTR(object,0x1190),(from+1)*0x58);
}
static void cloud_type_changed(void *object)
{
    uint32_t layer,type,target;
    if(!advanced_send(object,0x11a8,0x18b,0,0))return;
    layer=cloud_selected(object);type=advanced_send(object,0x20a0,0x147,0,0);
    if((int)type<10){
        if(!layer){
            if(!U32(PTR(object,0x1190),0xb4))target=1;else if(!U32(PTR(object,0x1190),0x10c))target=2;else goto rebuild;
            cloud_copy(object,0,target);U32(PTR(object,0x1190),target*0x58+0x5c)=1;U32(PTR(object,0x1190),0x5c)=0;
            U32(PTR(object,0x1190),target*0x58+0x48)=cloud_type(type);U32(PTR(object,0x1190),4)=target;
        }else{U32(PTR(object,0x1190),layer*0x58+0x48)=cloud_type(type);U32(PTR(object,0x1190),4)=layer;}
        cloud_changed(object);
    }else if(type==10){
        if(layer>0&&layer<=2){cloud_copy(object,layer,0);U32(PTR(object,0x1190),0x5c)=1;U32(PTR(object,0x1190),layer*0x58+0x5c)=0;U32(PTR(object,0x1190),0x48)=10;U32(PTR(object,0x1190),4)=0;cloud_changed(object);
            if(layer==1&&U32(PTR(object,0x1190),0x10c)==1){cloud_copy(object,2,1);U32(PTR(object,0x1190),0xb4)=1;U32(PTR(object,0x1190),0x10c)=0;}}
        U32(PTR(object,0x1190),4)=0;
    }
rebuild:cloud_list(object);cloud_populate(object);
}
static void cloud_add(void *object)
{
    uint32_t layer,old,result;
    if(!U32(PTR(object,0x1190),0xb4))layer=1;else if(!U32(PTR(object,0x1190),0x10c))layer=2;else{layer=0;U32(PTR(object,0x1190),0x48)=10;}
    old=U32(PTR(object,0x1190),4);U32(PTR(object,0x1190),4)=layer;result=advanced_callback(object,0x1194,0x3fc);
    if(result){if(result==0x1008){U32(object,0x90)=1;U32(object,0x94)=2;}API(0x25778,int(TC *)(void *,void *,uint32_t))(object,AT(object,0x20a0),result);advanced_post(object,U32(object,0x20c0));if(result!=0x1008){U32(PTR(object,0x1190),4)=old;return;}}
    U32(PTR(object,0x1190),layer*0x58+0x5c)=1;cloud_changed(object);cloud_list(object);cloud_populate(object);advanced_post(object,U32(object,0x11c8));
}
static void cloud_delete(void *object)
{
    uint32_t layer=cloud_selected(object),selected;void *parent;
    U32(PTR(object,0x1190),4)=layer;advanced_callback(object,0x1194,0x400);
    selected=U32(PTR(object,0x1190),4);
    if(U32(PTR(object,0x1190),selected*0x58+0x5c)){advanced_post(object,U32(PTR(object,0x11a0),0x14b8));return;}
    if(layer==2){if(U32(PTR(object,0x1190),0xb4))U32(PTR(object,0x1190),4)=1;else if(U32(PTR(object,0x1190),0x5c))U32(PTR(object,0x1190),4)=0;}
    else if(layer==1){if(U32(PTR(object,0x1190),0x10c)){cloud_copy(object,2,1);U32(PTR(object,0x1190),0xb4)=1;U32(PTR(object,0x1190),0x10c)=0;U32(PTR(object,0x1190),4)=1;}else if(U32(PTR(object,0x1190),0x5c))U32(PTR(object,0x1190),4)=0;}
    else if(!layer){if(U32(PTR(object,0x1190),0xb4))U32(PTR(object,0x1190),4)=1;else if(U32(PTR(object,0x1190),0x10c))U32(PTR(object,0x1190),4)=2;}
    cloud_list(object);cloud_populate(object);parent=PTR(object,0x11a0);advanced_post(object,U32(parent,advanced_send(object,0x11a8,0x18b,0,0)?0x14b8:0x15b4));
}
uint32_t fs98_world_advanced_cloud(void *object,uint32_t rva,uint32_t first,uint32_t second)
{
    uint32_t layer,row,value,id=0,control=0,status=0,count;void *data;(void)second;
    switch(rva){
    case 0x7720:cloud_init(object);break;
    case 0x7a60:cloud_changed(object);break;
    case 0x7a80:return cloud_selected(object);
    case 0x7a90:cloud_select(object,first);break;
    case 0x7ae0:cloud_populate(object);break;
    case 0x7c00:cloud_types(object);break;
    case 0x7c70:cloud_amounts(object);break;
    case 0x7df0:cloud_enable(object);break;
    case 0x8140:cloud_list(object);break;
    case 0x8390:cloud_initial_selection(object);break;
    case 0x8410:
        if(!advanced_send(object,0x11a8,0x18b,0,0))break;
        layer=cloud_selected(object);value=advanced_send(object,0x1b20,0x147,0,0);data=PTR(object,0x1190);
        WORD(data,layer*0x58+0x44)=(uint16_t)(value+(layer?WORD(data,0x110+U32(data,layer*0x58+0x48)*4):0));cloud_list(object);break;
    case 0x84e0:cloud_type_changed(object);break;
    case 0x89d0:U32(PTR(object,0x1190),(cloud_selected(object)+1)*0x58)=U32(object,0x1b14);break;
    case 0x8a00:
        if(!advanced_send(object,0x11a8,0x18b,0,0))break;
        row=cloud_selected(object)*0x58;
        value=API(0x25708,uint32_t(TC *)(void *))(AT(object,0x1b68));U32(PTR(object,0x1190),row+0x50)=value;advanced_callback(object,0x1194,4);API(0x25710,void(TC *)(void *,uint32_t))(AT(object,0x1b68),U32(PTR(object,0x1190),row+0x50));break;
    case 0x8a70:
        if(!advanced_send(object,0x11a8,0x18b,0,0))break;
        if(advanced_send(object,0x11a8,0x188,0,0)==0xffffffff)advanced_send(object,0x11a8,0x186,0,0);
        value=advanced_send(object,0x11a8,0x188,0,0);if(value==0xffffffff)break;
        value=advanced_send(object,0x11a8,0x199,value,0);U32(PTR(object,0x1190),4)=value;cloud_populate(object);break;
    case 0x8b00:cloud_add(object);break;
    case 0x8c10:cloud_delete(object);break;
    case 0x8e50:
        if(!U32(object,0x1100)||!advanced_send(object,0x11a8,0x18b,0,0))return 0;
        layer=cloud_selected(object);row=layer*0x58;advanced_get(object,0x19f0,AT(PTR(object,0x1190),row+8));advanced_get(object,0x1988,AT(PTR(object,0x1190),row+0x1c));advanced_get(object,0x1920,AT(PTR(object,0x1190),row+0x30));
        U32(PTR(object,0x1190),(layer+1)*0x58)=U32(object,0x1b14);value=API(0x25708,uint32_t(TC *)(void *))(AT(object,0x1b68));U32(PTR(object,0x1190),row+0x50)=value;advanced_callback(object,0x1194,4);data=AT(PTR(object,0x1190),row);
        if(U32(data,8))return 0x1aeb;
        if(U32(data,0x1c))return 0x1aec;
        if(U32(data,0x30))return 0x1aee;
        cloud_list(object);cloud_populate(object);return 0;
    case 0x8f70:
        row=cloud_selected(object)*0x58;data=AT(PTR(object,0x1190),row);
        if(U32(data,8)){API(0x25574,void *(TC *)(void *,uint32_t))(object,0x1aeb);control=0x19f0;status=(uint32_t)AT(PTR(object,0x1190),row+8);id=0x1aeb;}
        else if(U32(data,0x1c)){control=0x1988;status=(uint32_t)AT(data,0x1c);id=0x1aec;}
        else if(U32(data,0x30)){control=0x1920;status=(uint32_t)AT(data,0x30);id=0x1aee;}
        if(control){value=API(0x25778,uint32_t(TC *)(void *,void *,uint32_t))(object,AT(object,control),status);U32(object,0x8c)=value;if(!value)id=0;}return advanced_validate_again(object,id);
    case 0x9050:
        API(0x258c0,int(SC *)(int))(9);
        if(U32(PTR(object,0x1194),0x14)){advanced_buttons(PTR(object,0x11a0),2);if(!(first&1))advanced_post(object,U32(PTR(object,0x119c),0x20));}
        else{count=advanced_send(object,0x11a8,0x18b,0,0);if(count==4){advanced_buttons(PTR(object,0x11a0),1);value=U32(object,0x11c8);}else if(!advanced_send(object,0x11a8,0x18b,0,0)){advanced_buttons(PTR(object,0x11a0),0);value=U32(PTR(object,0x11a0),0x15b4);}else{advanced_buttons(PTR(object,0x11a0),-2);value=U32(object,0x11c8);}if(!(first&1))advanced_post(object,value);}
        if(first){cloud_populate(object);API(0x25894,int(SC *)(uint32_t,void *,int))(U32(object,0x20),NULL,1);}return 0;
    }return 0;
}
