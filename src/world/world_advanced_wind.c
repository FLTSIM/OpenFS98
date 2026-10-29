/* Wind layers and the separately represented surface-wind row. */
#include "world_advanced_internal.h"
#include "world_unwind.h"
struct wind_string {struct fs98_world_unwind frame;void *text;};
static void wind_string_cleanup(struct fs98_world_unwind *frame)
{struct wind_string *string=(void *)frame;API(0x255c0,void(TC *)(void *))(&string->text);}
static void wind_enable(void *object,uint32_t enable,uint32_t list)
{
    uint32_t index;static const uint32_t controls[]={0x1a38,0x1eb8,0x1f20,0x1f88,0x1ff0,0x21c8,0x15e8,0x1478,0x1530,0x1308,0x1810,0x1250,0x13c0,0x2058,0x16a0,0x1758,0x18c8,0x1980};
    for(index=0;index<18;++index)advanced_enable(object,controls[index],(int)enable);
    if(list){advanced_enable(object,0x1208,(int)enable);advanced_enable(object,0x2110,(int)enable);}
}
static WORLD_NOINLINE void wind_list(void *object)
{
    struct wind_string string;int index;uint32_t row,item,field;char text[39];void *data;
    API(0x255b4,void(TC *)(void *))(&string.text);world_unwind_enter(&string.frame,wind_string_cleanup);
    advanced_send(object,0x1208,0x184,0,0);U32(PTR(object,0x11ec),0x138)=0;
    for(index=2;index>=-1;--index){
        row=index>=0?(uint32_t)index*0x64:0;data=PTR(object,0x11ec);
        if(index>=0?!U32(data,row+0x68):!U32(data,0x140))continue;
        API(0x25578,void *(TC *)(void *,const char *))(&string.text,index>=0?PTR(data,row+0xc):"000000\t");
        if(index>=0)API(0x2557c,void *(TC *)(void *,int))(&string.text,9);
        for(field=0;field<(index>=0?3u:3u);++field){
            API(0x25580,void *(TC *)(void *,const char *))(&string.text,PTR(PTR(object,0x11ec),index>=0?row+0x20+field*0x14:0x148+field*0x14));
            API(0x2557c,void *(TC *)(void *,int))(&string.text,9);
        }
        advanced_send(object,0x21c8,0x148,U32(PTR(object,0x11ec),index>=0?row+0x5c:0x184),(uint32_t)text);
        API(0x25580,void *(TC *)(void *,const char *))(&string.text,text);API(0x2557c,void *(TC *)(void *,int))(&string.text,9);
        API(0x25580,void *(TC *)(void *,const char *))(&string.text,(char *)AT(object,index>=0?0x114c+(uint32_t)index*0x28:0x1124));
        item=advanced_send(object,0x1208,0x180,0,(uint32_t)string.text);advanced_send(object,0x1208,0x19a,item,index>=0?(uint32_t)index:3);
        if(index>=0)U32(PTR(object,0x11ec),0x138)=1;
    }
    if(!advanced_send(object,0x1208,0x18b,0,0)){wind_enable(object,0,0);U32(object,0x1118)=0xffffffff;advanced_buttons(PTR(object,0x11fc),0);}
    else{wind_enable(object,1,0);advanced_send(object,0x1208,0x197,0,0);}
    advanced_send(object,0x1208,0x186,U32(object,0x1118),0);world_unwind_leave(&string.frame);wind_string_cleanup(&string.frame);
}
static void wind_selection(void *object)
{
    uint32_t layer,index,value,count,row;char text[19];const struct fs98_world_init_services *ui=fs98_world_advanced_services()->ui;
    if(!advanced_send(object,0x1208,0x18b,0,0))return;
    if(advanced_send(object,0x1208,0x188,0,0)==0xffffffff)advanced_send(object,0x1208,0x186,0,0);
    U32(object,0x1118)=advanced_send(object,0x1208,0x188,0,0);layer=advanced_send(object,0x1208,0x199,U32(object,0x1118),0);U32(object,0x111c)=layer;U32(PTR(object,0x11ec),4)=layer<=2?layer:3;
    if(U32(PTR(object,0x11f0),0x18))advanced_buttons(PTR(object,0x11fc),5);
    else{count=advanced_send(object,0x1208,0x18b,0,0);if(count==4)advanced_buttons(PTR(object,0x11fc),1);else{count=advanced_send(object,0x1208,0x18b,0,0);advanced_buttons(PTR(object,0x11fc),count?-5:0);}}
    layer=U32(object,0x111c);if((int)layer>3)return;
    for(index=0;index<3;++index)API(0x25500,int(TC *)(void *,int))(AT(object,index==0?0x1ff0:index==1?0x1980:0x13c0),layer<3);
    ui->load_string(U32(object,0x80),layer<3?0xf91d:0xf8f2,text,19);advanced_title(object,0x1250,text);
    ui->load_string(U32(object,0x80),layer<3?0xf91e:0xf8f4,text,19);advanced_title(object,layer<3?0x18c8:0x16a0,text);
    if(layer<3)advanced_title(object,0x16a0,(char *)AT(object,0x11d8));else{ui->load_string(U32(object,0x80),0xf8f7,text,19);advanced_title(object,0x18c8,text);}
    row=layer*0x64;
    advanced_text(object,0x1f88,PTR(PTR(object,0x11ec),layer<3?row+0x20:0x148));
    if(layer<3)advanced_text(object,0x1ff0,PTR(PTR(object,0x11ec),row+0xc));
    advanced_text(object,0x1f20,PTR(PTR(object,0x11ec),layer<3?row+0x34:0x15c));
    advanced_text(object,0x1eb8,PTR(PTR(object,0x11ec),layer<3?row+0x48:0x170));
    advanced_send(object,0x21c8,0x14e,U32(PTR(object,0x11ec),layer<3?row+0x5c:0x184),0);
    value=U32(PTR(object,0x11ec),layer<3?(layer+1)*0x64:0x18c);API(0x25710,void(TC *)(void *,uint32_t))(AT(object,0x1a38),value);
}
static void wind_populate(void *object)
{
    const struct fs98_world_init_services *ui=fs98_world_advanced_services()->ui;uint32_t units;
    wind_list(object);units=U32(PTR(object,0x11f0),0x10);
    ui->load_string(U32(object,0x80),units<=1?0xf8f5:0xf8f9,(char *)AT(object,0x11d8),19);
    ui->load_string(U32(object,0x80),units<=1?0xf8f3:0xf8f8,(char *)AT(object,0x11c4),19);
    advanced_title(object,0x1980,(char *)AT(object,0x11d8));
    if(U32(PTR(object,0x11f0),0x18)){wind_enable(object,0,0);advanced_buttons(PTR(object,0x11fc),5);}
    advanced_send(object,0x1208,0x186,U32(object,0x1118),0);wind_selection(object);
}
static void wind_init(void *object)
{
    uint32_t index;int tabs[]={0x2d,0x55,0x7d,0xd7,0x122};const struct fs98_world_init_services *ui=fs98_world_advanced_services()->ui;
    static const uint32_t labels[]={0x1980,0x18c8,0x1758,0x16a0,0x15e8,0x1530,0x1478};
    API(0x25714,int(TC *)(void *,int,const int *))(AT(object,0x450),5,tabs);
    advanced_label(object,0x2058,0x370);ui->combo(AT(object,0x21c8),AT(object,0x3e0),AT(object,0x2058));advanced_send(object,0x21c8,0x14e,0,0);
    advanced_label(object,0x2110,0x370);API(0x25718,void(TC *)(void *,void *,void *))(AT(object,0x1208),AT(object,0x450),AT(object,0x2110));
    advanced_label(object,0x13c0,0x220);advanced_edit_init(object,0x1ff0,0x13c0,1,6);
    advanced_label(object,0x1250,0x220);ui->edit(AT(object,0x1f88),ui->base_font(2),0xffffff,0,0xff0000,AT(object,0x1250),1);
    advanced_label(object,0x1810,0x220);ui->edit(AT(object,0x1f20),ui->base_font(2),0xffffff,0,0xff0000,AT(object,0x1810),1);
    advanced_label(object,0x1308,0x220);ui->edit(AT(object,0x1eb8),ui->base_font(2),0xffffff,0,0xff0000,AT(object,0x1308),1);
    for(index=0;index<7;++index)advanced_label(object,labels[index],0x220);
    API(0x2571c,void(TC *)(void *,uint32_t,uint32_t,uint32_t,uint32_t,int,int,int,int,int,int,uint32_t))(AT(object,0x1a38),0x135,0x136,U32(object,0x10fc),U32(object,0x10dc),6,0x1a,6,8,1,-1,fs98_world_advanced_services()->resources());
    for(index=0;index<4;++index)ui->load_string(U32(object,0x80),index==0?0xf8da:index==1?0xf8d8:index==2?0xf8d9:0xf8d7,(char *)AT(object,0x1124+index*0x28),39);
    PTR(object,0x1204)=fs98_world_advanced_allocate(object,0);
    advanced_send(object,0x1ff0,0xc5,U32(PTR(object,0x11ec),0x10)-1,0);advanced_send(object,0x1f20,0xc5,U32(PTR(object,0x11ec),0x38)-1,0);
    advanced_send(object,0x1eb8,0xc5,U32(PTR(object,0x11ec),0x4c)-1,0);advanced_send(object,0x1f88,0xc5,U32(PTR(object,0x11ec),0x24)-1,0);wind_populate(object);
}
static void wind_find(void *object,uint32_t layer)
{
    uint32_t index,count=advanced_send(object,0x1208,0x18b,0,0);
    for(index=0;(int)index<(int)count;++index){if(advanced_send(object,0x1208,0x199,index,0)==layer){advanced_send(object,0x1208,0x186,index,0);break;}count=advanced_send(object,0x1208,0x18b,0,0);}
    if(index==advanced_send(object,0x1208,0x18b,0,0))advanced_send(object,0x1208,0x186,U32(object,0x1118),0);
}
static void wind_add(void *object,uint32_t layer)
{
    uint32_t index;
    if(layer==0xffffffff){
        if(advanced_send(object,0x1208,0x18b,0,0)==3){
            if(!U32(PTR(object,0x11ec),0x140)){U32(object,0x1120)=3;U32(PTR(object,0x11ec),0x140)=1;U32(PTR(object,0x11ec),4)=U32(object,0x1120)<=2?U32(object,0x1120):3;advanced_callback(object,0x11f0,0x3fc);}
            else{for(index=0;index<3;++index)if(!U32(PTR(object,0x11ec),index*0x64+0x68))break;U32(PTR(object,0x11ec),4)=index<=2?index:3;advanced_callback(object,0x11f0,0x3fc);U32(PTR(object,0x11ec),index*0x64+0x68)=1;U32(object,0x1120)=index;U32(PTR(object,0x11ec),0x138)=1;}
            wind_list(object);wind_find(object,U32(PTR(object,0x11ec),4));wind_selection(object);advanced_post(object,U32(object,0x1228));wind_enable(object,1,1);return;
        }
        API(0x256f4,void(TC *)(void *,uint32_t))(PTR(object,0x1200),U32(object,0x80));
        if(!U32(PTR(object,0x11ec),0x140))API(0x256f8,void(TC *)(void *,uint32_t,uint32_t,uint32_t))(PTR(object,0x1200),0x1e61,0xf8da,0);
        for(index=0;index<3;++index)if(!U32(PTR(object,0x11ec),index*0x64+0x68))API(0x256f8,void(TC *)(void *,uint32_t,uint32_t,uint32_t))(PTR(object,0x1200),index+0x1e62,index==0?0xf8d8:index==1?0xf8d9:0xf8d7,0);
        API(0x256fc,void(TC *)(void *))(PTR(object,0x1200));return;
    }
    API(0x2585c,void(TC *)(void *))(PTR(object,0x1200));U32(object,0x1120)=layer;
    U32(PTR(object,0x11ec),layer==3?0x140:layer*0x64+0x68)=1;U32(PTR(object,0x11ec),4)=layer<=2?layer:3;advanced_callback(object,0x11f0,0x3fc);
    wind_list(object);wind_find(object,layer);wind_enable(object,1,1);wind_selection(object);advanced_post(object,U32(object,0x1228));
}
static void wind_delete(void *object)
{
    uint32_t row;U32(PTR(object,0x11ec),4)=U32(object,0x111c)<=2?U32(object,0x111c):3;advanced_callback(object,0x11f0,0x400);
    row=advanced_send(object,0x1208,0x188,0,0);U32(object,0x1120)=0;wind_list(object);
    if(!advanced_send(object,0x1208,0x18b,0,0)){U32(object,0x1118)=0xffffffff;advanced_buttons(PTR(object,0x11fc),0);advanced_text(object,0x1ff0,"");advanced_text(object,0x1f88,"");advanced_text(object,0x1f20,"");advanced_text(object,0x1eb8,"");wind_enable(object,0,1);advanced_post(object,U32(PTR(object,0x11fc),0x15b4));return;}
    advanced_send(object,0x1208,0x186,row,0);if(advanced_send(object,0x1208,0x188,0,0)==0xffffffff)advanced_send(object,0x1208,0x186,row-1,0);
    wind_selection(object);advanced_post(object,U32(PTR(object,0x11fc),0x14b8));
}
static uint32_t wind_validate(void *object,uint32_t id)
{
    uint32_t layer,row,result=0,value;void *data;
    if(!U32(object,0x1100)||!advanced_send(object,0x1208,0x18b,0,0))return 0;
    U32(object,0x1118)=advanced_send(object,0x1208,0x188,0,0);layer=U32(object,0x111c);row=layer*0x64;
    if((int)layer<3){
        advanced_get(object,0x1ff0,AT(PTR(object,0x11ec),row+8));advanced_get(object,0x1f20,AT(PTR(object,0x11ec),row+0x30));advanced_get(object,0x1eb8,AT(PTR(object,0x11ec),row+0x44));advanced_get(object,0x1f88,AT(PTR(object,0x11ec),row+0x1c));
        value=API(0x25708,uint32_t(TC *)(void *))(AT(object,0x1a38));U32(PTR(object,0x11ec),(U32(object,0x111c)+1)*0x64)=value;
        value=advanced_send(object,0x21c8,0x147,0,0);U32(PTR(object,0x11ec),U32(object,0x111c)*0x64+0x5c)=value;
    }else{
        advanced_get(object,0x1eb8,AT(PTR(object,0x11ec),0x16c));advanced_get(object,0x1f20,AT(PTR(object,0x11ec),0x158));advanced_get(object,0x1f88,AT(PTR(object,0x11ec),0x144));
        value=API(0x25708,uint32_t(TC *)(void *))(AT(object,0x1a38));U32(PTR(object,0x11ec),0x18c)=value;
        value=advanced_send(object,0x21c8,0x147,0,0);U32(PTR(object,0x11ec),0x184)=value;
    }
    advanced_callback(object,0x11f0,4);layer=U32(object,0x111c);data=PTR(object,0x11ec);row=layer*0x64;
    if((int)layer<3){if(U32(data,row+0x30))result=0x1b09;else if(U32(data,row+0x44))result=0x1b0d;else if(U32(data,row+0x1c))result=0x1b0e;else if(U32(data,row+8))result=0x1b0c;}
    else{if(U32(data,0x158))result=0x1b09;else if(U32(data,0x16c))result=0x1b0d;else if(U32(data,0x144))result=0x1b0e;}
    if(result)return result;
    wind_list(object);layer=U32(object,0x111c);row=layer*0x64;
    if(id==0x1b09&&(int)layer<3){advanced_text(object,0x1f88,PTR(PTR(object,0x11ec),row+0x20));advanced_text(object,0x1ff0,PTR(PTR(object,0x11ec),U32(object,0x111c)*0x64+0xc));advanced_text(object,0x1f20,PTR(PTR(object,0x11ec),U32(object,0x111c)*0x64+0x34));}
    else if((id==0x1b0d&&(int)layer<3)||id==0x1b0e||id==0x1b0c){advanced_text(object,0x1f88,PTR(PTR(object,0x11ec),(int)layer<3?row+0x20:0x148));if((int)layer<3)advanced_text(object,0x1ff0,PTR(PTR(object,0x11ec),U32(object,0x111c)*0x64+0xc));}
    advanced_send(object,0x1208,0x186,U32(object,0x1118),0);return 0;
}
static uint32_t wind_error(void *object)
{
    uint32_t index,layer=U32(object,0x111c),result=0,value,offset;void *data;
    static const uint32_t ids[]={0x1b09,0x1b0d,0x1b0e,0x1b0c},controls[]={0x1f20,0x1eb8,0x1f88,0x1ff0},upper[]={0x30,0x44,0x1c,8},ground[]={0x158,0x16c,0x144};
    data=PTR(object,0x11ec);
    for(index=0;index<((int)layer<3?4u:3u);++index){offset=(int)layer<3?layer*0x64+upper[index]:ground[index];if(!U32(data,offset))continue;value=API(0x25778,uint32_t(TC *)(void *,void *,void *))(object,AT(object,controls[index]),AT(data,offset));U32(object,0x8c)=value;if(value)result=ids[index];break;}
    return advanced_validate_again(object,result);
}
uint32_t fs98_world_advanced_wind(void *object,uint32_t rva,uint32_t first,uint32_t second)
{
    uint32_t layer,value,count;(void)second;
    switch(rva){
    case 0x11e40:wind_init(object);break;
    case 0x121b0:wind_list(object);break;
    case 0x124b0:wind_populate(object);break;
    case 0x125b0:wind_selection(object);break;
    case 0x12930:wind_add(object,first);break;
    case 0x12c80:wind_delete(object);break;
    case 0x12e00:case 0x12e60:
        if(!advanced_send(object,0x1208,0x18b,0,0))break;
        layer=U32(object,0x111c);
        if(rva==0x12e00){value=API(0x25708,uint32_t(TC *)(void *))(AT(object,0x1a38));U32(PTR(object,0x11ec),(int)layer<3?(U32(object,0x111c)+1)*0x64:0x18c)=value;break;}
        value=advanced_send(object,0x21c8,0x147,0,0);U32(PTR(object,0x11ec),layer==3?0x184:U32(object,0x111c)*0x64+0x5c)=value;advanced_callback(object,0x11f0,4);layer=U32(object,0x111c);
        advanced_text(object,0x1f88,PTR(PTR(object,0x11ec),layer==3?0x148:layer*0x64+0x20));if(layer!=3)advanced_text(object,0x1ff0,PTR(PTR(object,0x11ec),U32(object,0x111c)*0x64+0xc));wind_list(object);break;
    case 0x12f50:wind_enable(object,first,second);break;
    case 0x13060:return wind_validate(object,first);
    case 0x13470:return wind_error(object);
    case 0x13600:
        API(0x258c0,int(SC *)(int))(9);
        if(U32(PTR(object,0x11f0),0x18)){advanced_buttons(PTR(object,0x11fc),2);wind_enable(object,0,1);if(!(first&1))advanced_post(object,U32(PTR(object,0x11f8),0x20));}
        else{count=advanced_send(object,0x1208,0x18b,0,0);if(count==4){advanced_buttons(PTR(object,0x11fc),1);wind_enable(object,1,1);value=U32(object,0x1228);}else if(!advanced_send(object,0x1208,0x18b,0,0)){advanced_buttons(PTR(object,0x11fc),0);wind_enable(object,0,1);value=U32(PTR(object,0x11fc),0x15b4);}else{advanced_buttons(PTR(object,0x11fc),-2);wind_enable(object,1,1);value=U32(object,0x1228);}if(!(first&1))advanced_post(object,value);}
        if(first){wind_selection(object);API(0x25894,int(SC *)(uint32_t,void *,int))(U32(object,0x20),NULL,1);}return 0;
    }return 0;
}
