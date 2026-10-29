/* Temperature layers: list rebuilds, owner callbacks and edit validation. */
#include "world_advanced_internal.h"
#include "world_unwind.h"
struct advanced_string {
    struct fs98_world_unwind frame;
    void *text;
};
static void advanced_string_cleanup(struct fs98_world_unwind *frame)
{ struct advanced_string *string=(void *)frame;API(0x255c0,void(TC *)(void *))(&string->text); }
static WORLD_NOINLINE void temperature_list(void *object)
{
    struct advanced_string string;int index;uint32_t row;void *data;
    API(0x255b4,void(TC *)(void *))(&string.text);world_unwind_enter(&string.frame,advanced_string_cleanup);
    advanced_send(object,0x120c,0x184,0,0);
    for(index=3;index>=0;--index){
        data=PTR(object,0x11f0);
        if(U32(data,(uint32_t)index*0x2c+0x30)){
            API(0x25578,void *(TC *)(void *,const char *))(&string.text,PTR(data,(uint32_t)index*0x2c+0xc));
            API(0x2557c,void *(TC *)(void *,int))(&string.text,9);
            API(0x25580,void *(TC *)(void *,const char *))(&string.text,PTR(PTR(object,0x11f0),(uint32_t)index*0x2c+0x20));
            API(0x25580,void *(TC *)(void *,const char *))(&string.text,"\t");
            API(0x25580,void *(TC *)(void *,const char *))(&string.text,(char *)AT(object,0x1124+(uint32_t)index*0x28));
            row=advanced_send(object,0x120c,0x180,0,(uint32_t)string.text);
            advanced_send(object,0x120c,0x19a,row,(uint32_t)index);
            if((uint32_t)index==U32(object,0x1120))U32(object,0x1118)=row;
        }
    }
    advanced_send(object,0x120c,0x197,0,0);
    world_unwind_leave(&string.frame);advanced_string_cleanup(&string.frame);
}
static void temperature_enable(void *object,uint32_t enable,uint32_t list)
{
    uint32_t index;static const uint32_t controls[]={0x182c,0x17c4,0x147c,0x16a4,0x1534,0x15ec};
    for(index=0;index<6;++index)advanced_enable(object,controls[index],(int)enable);
    if(list){advanced_enable(object,0x120c,(int)enable);advanced_enable(object,0x1254,(int)enable);}
    advanced_enable(object,0x1894,(int)enable);
    for(index=0;index<3;++index)advanced_enable(object,index==0?0x175c:index==1?0x130c:0x13c4,enable?(int)U32(object,0x1950):0);
}
static uint32_t temperature_layer(uint32_t index){return index<=2?index:3;}
static void temperature_selection(void *object)
{
    uint32_t row,count;void *data;
    if(!advanced_send(object,0x120c,0x18b,0,0))return;
    row=advanced_send(object,0x120c,0x188,0,0);
    if(row==0xffffffff)advanced_send(object,0x120c,0x186,0,0);
    U32(object,0x1118)=advanced_send(object,0x120c,0x188,0,0);
    row=advanced_send(object,0x120c,0x199,U32(object,0x1118),0);U32(object,0x111c)=row;
    advanced_text(object,0x182c,PTR(PTR(object,0x11f0),row*0x2c+0xc));
    advanced_text(object,0x17c4,PTR(PTR(object,0x11f0),U32(object,0x111c)*0x2c+0x20));
    U32(PTR(object,0x11f0),4)=temperature_layer(U32(object,0x111c));
    data=PTR(object,0x11f4);
    if(U32(data,0x18)||U32(data,0x14)){advanced_buttons(PTR(object,0x1200),5);return;}
    count=advanced_send(object,0x120c,0x18b,0,0);
    if(count==4){advanced_buttons(PTR(object,0x1200),1);return;}
    count=advanced_send(object,0x120c,0x18b,0,0);
    advanced_buttons(PTR(object,0x1200),count?-5:0);
}
static void temperature_check(void *object)
{
    if(U32(PTR(object,0x11f4),0x18)||U32(PTR(object,0x11f4),0x14))return;
    U32(PTR(object,0x11f0),0xbc)=U32(object,0x1950);
    advanced_enable(object,0x175c,(int)U32(PTR(object,0x11f0),0xbc));
    advanced_enable(object,0x130c,(int)U32(PTR(object,0x11f0),0xbc));
    advanced_enable(object,0x13c4,(int)U32(PTR(object,0x11f0),0xbc));
}
static void temperature_populate(void *object)
{
    uint32_t units=0;void *owner;const struct fs98_world_init_services *ui=fs98_world_advanced_services()->ui;
    temperature_list(object);advanced_text(object,0x175c,PTR(PTR(object,0x11f0),0xc4));
    owner=PTR(object,0x11f4);units=U32(owner,0x10);
    ui->load_string(U32(object,0x80),units<=1?0xf8f5:0xf8f9,(char *)AT(object,0x11c4),19);
    ui->load_string(U32(object,0x80),units?0xf8ef:0xf8f0,(char *)AT(object,0x11d8),19);
    advanced_title(object,0x13c4,(char *)AT(object,0x11d8));advanced_title(object,0x1534,(char *)AT(object,0x11d8));advanced_title(object,0x15ec,(char *)AT(object,0x11c4));
    if(U32(PTR(object,0x11f4),0x18)||U32(PTR(object,0x11f4),0x14)){temperature_enable(object,0,1);advanced_buttons(PTR(object,0x1200),2);}
    advanced_send(object,0x120c,0x186,0,0);temperature_selection(object);
    advanced_check(object,0x1894,U32(PTR(object,0x11f0),0xbc));temperature_check(object);
}
static void temperature_init(void *object)
{
    uint32_t index;struct fs98_world_rect rect;int tabs[]={0x2d,0x55,0x7d,0xd7,0x122};
    const struct fs98_world_init_services *ui=fs98_world_advanced_services()->ui;
    static const uint32_t labels[]={0x130c,0x13c4,0x147c,0x1534,0x15ec,0x16a4};
    API(0x25714,int(TC *)(void *,int,const int *))(AT(object,0x450),5,tabs);
    for(index=0;index<6;++index)advanced_label(object,labels[index],0x220);
    advanced_label(object,0x1254,0x370);
    advanced_edit_init(object,0x175c,0x130c,0x11,4);advanced_edit_init(object,0x17c4,0x147c,0x13,4);advanced_edit_init(object,0x182c,0x16a4,0x11,6);
    advanced_sprite(object,0x1894,0x139,0x4c0);
    API(0x25718,void(TC *)(void *,void *,void *))(AT(object,0x120c),AT(object,0x450),AT(object,0x1254));
    API(0x25744,void(SC *)(void *,void *))(&rect,AT(object,0x195c));
    API(0x2559c,void(TC *)(void *,int,int,int,int,int))(AT(object,0x195c),rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,1);
    for(index=0;index<4;++index)ui->load_string(U32(object,0x80),index==0?0xf8da:index==1?0xf8d8:index==2?0xf8d9:0xf8d7,(char *)AT(object,0x1124+index*0x28),39);
    PTR(object,0x1208)=fs98_world_advanced_allocate(object,0);temperature_populate(object);
}
static uint32_t temperature_validate(void *object,uint32_t id)
{
    uint32_t result=0,value,row;void *data;
    if(!U32(object,0x1100))return 0;
    if(!advanced_send(object,0x120c,0x18b,0,0))return advanced_validate_again(object,0);
    if(!U32(PTR(object,0x11f4),0x18)&&!U32(PTR(object,0x11f4),0x14))U32(PTR(object,0x11f0),0xbc)=U32(object,0x1950);
    advanced_get(object,0x182c,AT(PTR(object,0x11f0),U32(object,0x111c)*0x2c+8));
    advanced_get(object,0x17c4,AT(PTR(object,0x11f0),U32(object,0x111c)*0x2c+0x1c));advanced_get(object,0x175c,AT(PTR(object,0x11f0),0xc0));
    value=advanced_callback(object,0x11f4,4);U32(object,0x11ec)=value;
    if(value){
        if(value==0x100e)result=0x1ae1;
        else{data=PTR(object,0x11f0);row=U32(object,0x111c)*0x2c;if(U32(data,row+8))result=0x1ae1;else if(U32(data,row+0x1c))result=0x1aed;else if(U32(data,0xc0))result=0x1b0a;}
    }
    if(!result){
        advanced_text(object,0x182c,PTR(PTR(object,0x11f0),U32(object,0x111c)*0x2c+0xc));
        advanced_text(object,0x17c4,PTR(PTR(object,0x11f0),U32(object,0x111c)*0x2c+0x20));
        U32(object,0x1120)=U32(object,0x111c);temperature_list(object);
        if(id==0x1b0a&&*(char *)PTR(PTR(object,0x11f0),0xc4))U32(PTR(object,0x11f0),0xbc)|=1;
        advanced_send(object,0x120c,0x186,U32(object,0x1118),0);temperature_selection(object);
    }return result;
}
static uint32_t temperature_error(void *object)
{
    uint32_t control=0,id=0,status=0,result;void *data=PTR(object,0x11f0);uint32_t row=U32(object,0x111c)*0x2c;
    if(U32(object,0x11ec)==0x100e){control=0x182c;status=0x100e;id=0x1ae1;}
    else if(U32(data,row+8)){control=0x182c;status=(uint32_t)AT(data,row+8);id=0x1ae1;}
    else if(U32(data,row+0x1c)){control=0x17c4;status=(uint32_t)AT(data,row+0x1c);id=0x1aed;}
    else if(U32(data,0xc0)){control=0x175c;status=(uint32_t)AT(data,0xc0);id=0x1b0a;}
    if(control){result=API(0x25778,uint32_t(TC *)(void *,void *,uint32_t))(object,AT(object,control),status);U32(object,0x8c)=result;if(!result)id=0;}
    return advanced_validate_again(object,id);
}
static void temperature_add(void *object,uint32_t layer)
{
    uint32_t index;void *data;
    if(layer==0xffffffff){
        if(advanced_send(object,0x120c,0x18b,0,0)==3){
            for(index=0;index<4;++index)if(!U32(PTR(object,0x11f0),index*0x2c+0x30))break;
            advanced_send(object,0x120c,0x186,U32(object,0x1118),0);U32(PTR(object,0x11f0),4)=temperature_layer(index);
            advanced_callback(object,0x11f4,0x3fc);U32(PTR(object,0x11f0),index*0x2c+0x30)=1;advanced_callback(object,0x11f4,4);
            U32(object,0x1120)=index;temperature_list(object);advanced_send(object,0x120c,0x186,U32(object,0x1118),0);
            temperature_enable(object,1,1);temperature_selection(object);advanced_post(object,U32(object,0x122c));return;
        }
        API(0x256f4,void(TC *)(void *,uint32_t))(PTR(object,0x1204),U32(object,0x80));
        for(index=0;index<4;++index){data=PTR(object,0x11f0);if(!U32(data,index*0x2c+0x30))API(0x256f8,void(TC *)(void *,uint32_t,uint32_t,uint32_t))(PTR(object,0x1204),index+0x1e61,index==0?0xf8da:index==1?0xf8d8:index==2?0xf8d9:0xf8d7,0);}
        API(0x256fc,void(TC *)(void *))(PTR(object,0x1204));return;
    }
    API(0x2585c,void(TC *)(void *))(PTR(object,0x1204));U32(object,0x1120)=layer;U32(PTR(object,0x11f0),4)=temperature_layer(layer);
    advanced_callback(object,0x11f4,0x3fc);U32(PTR(object,0x11f0),U32(object,0x1120)*0x2c+0x30)=1;
    temperature_enable(object,1,1);temperature_list(object);advanced_send(object,0x120c,0x186,U32(object,0x1118),0);temperature_selection(object);advanced_post(object,U32(object,0x122c));
    if(advanced_send(object,0x120c,0x18b,0,0)==1){advanced_text(object,0x175c,PTR(PTR(object,0x11f0),0xc4));advanced_check(object,0x1894,U32(PTR(object,0x11f0),0xbc));}
}
static void temperature_delete(void *object)
{
    uint32_t row;
    U32(PTR(object,0x11f0),4)=temperature_layer(U32(object,0x111c));advanced_callback(object,0x11f4,0x400);
    if(U32(PTR(object,0x11f0),U32(PTR(object,0x11f0),4)*0x2c+0x30)){advanced_post(object,U32(PTR(object,0x1200),0x14b8));return;}
    U32(object,0x1120)=0;row=advanced_send(object,0x120c,0x188,0,0);temperature_list(object);
    if(!advanced_send(object,0x120c,0x18b,0,0)){
        U32(object,0x1118)=0xffffffff;advanced_buttons(PTR(object,0x1200),0);
        advanced_text(object,0x175c,"");advanced_text(object,0x17c4,"");advanced_text(object,0x182c,"");temperature_enable(object,0,1);advanced_post(object,U32(PTR(object,0x1200),0x15b4));return;
    }
    advanced_send(object,0x120c,0x186,row,0);if(advanced_send(object,0x120c,0x188,0,0)==0xffffffff)advanced_send(object,0x120c,0x186,row-1,0);
    temperature_selection(object);advanced_post(object,U32(PTR(object,0x1200),0x14b8));
}
static uint32_t temperature_reload(void *object,uint32_t first)
{
    uint32_t disabled,count;API(0x258c0,int(SC *)(int))(9);
    disabled=U32(PTR(object,0x11f4),0x18)||U32(PTR(object,0x11f4),0x14);
    if(disabled){advanced_buttons(PTR(object,0x1200),2);temperature_enable(object,0,1);if(!(first&1))advanced_post(object,U32(PTR(object,0x11fc),0x20));}
    else if((count=advanced_send(object,0x120c,0x18b,0,0))!=4&&!advanced_send(object,0x120c,0x18b,0,0)){advanced_buttons(PTR(object,0x1200),0);temperature_enable(object,0,1);if(!(first&1))advanced_post(object,U32(PTR(object,0x1200),0x15b4));}
    else{advanced_buttons(PTR(object,0x1200),count==4?1:-2);temperature_enable(object,1,1);if(!(first&1))advanced_post(object,U32(object,0x122c));}
    advanced_send(object,0x120c,0x186,U32(object,0x1118),0);temperature_selection(object);return 0;
}
uint32_t fs98_world_advanced_temperature(void *object,uint32_t rva,uint32_t first,uint32_t second)
{
    (void)second;
    switch(rva){
    case 0xeb40:temperature_init(object);break;
    case 0xee00:temperature_list(object);break;
    case 0xef60:return temperature_layer(first);
    case 0xef90:temperature_populate(object);break;
    case 0xf0d0:temperature_check(object);break;
    case 0xf150:temperature_selection(object);break;
    case 0xf2b0:temperature_add(object,first);break;
    case 0xf500:temperature_delete(object);break;
    case 0xf690:temperature_enable(object,first,second);break;
    case 0xf770:return temperature_validate(object,first);
    case 0xf970:return temperature_error(object);
    case 0xfa60:return temperature_reload(object,first);
    }return 0;
}
