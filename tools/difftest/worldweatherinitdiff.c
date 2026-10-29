/* Independent guarded initializer fixture. Imported services only are patched;
 * retail keeps its CString unwind metadata and native initializer. */
#define main world_weather_event_main
#include "worldweatherdiff.c"
#undef main
static struct fs98_world_weather_font_state s_font_initial, s_font_reference;
static uint32_t s_init_saved_seh, s_init_unwinds, s_init_checks;
static char s_init_title[]="Independent artwork title";
static void (*s_registered_exit)(void);
static uint32_t init_offset(const void *pointer)
{
    const uint8_t *font=s_side?(void *)fs98_world_weather_get_font_state()->font:(uint8_t *)s_retail+0x24978;
    if((const uint8_t *)pointer>=font&&(const uint8_t *)pointer<font+0x70)return 0xf0000000+(uint32_t)((const uint8_t *)pointer-font);
    return world_offset(pointer);
}
static int init_base(void *object)
{ world_log("base.init %x\n",world_offset(object));return -1; }
static int TC init_base_hook(void *object){return init_base(object);}
static uint32_t init_resources(void){return 0xbad98765;}
static int TC init_skin(void *object,uint32_t bitmap,uint32_t module,uint32_t flags)
{ world_log("skin %x %x %x %x\n",world_offset(object),bitmap,module,flags);return s_scenario%5!=0; }
static int init_art(void *control,uint32_t bitmap,void *dib,void *palette,uint32_t flags)
{ world_log("art %x %x %x %x %x\n",world_offset(control),bitmap,world_offset(dib),world_offset(palette),flags);return -1; }
static int TC init_art_hook(void *control,uint32_t bitmap,void *dib,void *palette,uint32_t flags)
{ return init_art(control,bitmap,dib,palette,flags); }
static int init_button(void *control,void *font,void *alternate,uint32_t bitmap,void *dib,void *palette,
    uint32_t flags,int color,uint32_t module,int x,int y,struct fs98_world_rect rect)
{
    world_log("button %x fonts=%x/%x bitmap=%x dib=%x palette=%x flags=%x color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        world_offset(control),world_offset(font),world_offset(alternate),bitmap,world_offset(dib),world_offset(palette),flags,
        (uint32_t)color,module,x,y,rect.left,rect.top,rect.right,rect.bottom);return -1;
}
static int TC init_button_hook(void *control,void *font,void *alternate,uint32_t bitmap,void *dib,void *palette,
    uint32_t flags,int color,uint32_t module,int x,int y,struct fs98_world_rect rect)
{ return init_button(control,font,alternate,bitmap,dib,palette,flags,color,module,x,y,rect); }
static void init_text(void *control,void *font)
{
    world_log("text %x font=%x\n",world_offset(control),init_offset(font));
    if(control==(void *)(s_fixture.object+0x1154)&&(s_scenario&16))PTR(s_fixture.object,0x10e0)=NULL;
}
static void TC init_text_hook(void *control,void *font){init_text(control,font);}
static void init_combo(void *control,void *font,void *label)
{ world_log("combo %x font=%x label=%x\n",world_offset(control),world_offset(font),world_offset(label)); }
static void TC init_combo_hook(void *control,void *font,void *label){init_combo(control,font,label);}
static void *SC init_base_font(int font)
{ world_log("font %x\n",font);return s_fixture.descriptors[1]+0x30; }
static void *init_font(int font){return init_base_font(font);}
static void init_title(void *control,const char *title)
{ world_log("title %x [%s]\n",world_offset(control),title); }
static void TC init_title_hook(void *control,const char *title){init_title(control,title);}
static void TC init_string_construct(void *string)
{ world_log("string.construct\n");PTR(string,0)=s_init_title; }
static void TC init_get_title(void *object,void *string)
{ world_log("get.title %x\n",world_offset(object));PTR(string,0)=s_init_title; }
static void TC init_set_title(void *object,const char *title)
{ world_log("set.title %x [%s]\n",world_offset(object),title); }
static void TC init_string_destroy(void *string)
{ world_log("string.destroy [%s]\n",(const char *)PTR(string,0)); }
static void TC init_font_construct(void *font,void *base)
{ world_log("font.construct %x %x\n",init_offset(font),world_offset(base));U32(font,4)=0x12345678; }
static void TC init_font_destroy(void *font)
{ world_log("font.destroy %x\n",init_offset(font));U32(font,4)=0; }
static int init_at_exit(void (*callback)(void))
{ world_log("at.exit\n");s_registered_exit=callback;return -1; }
static void *init_on_exit(void (*callback)(void))
{ init_at_exit(callback);return (void *)callback; }
static int TC init_show(void *control,int show)
{
    world_log("show %x %x\n",world_offset(control),(uint32_t)show);U32(control,0xc)=(uint32_t)show;
    if(control==(void *)(s_fixture.object+0x1154)&&(s_scenario&16))PTR(s_fixture.object,0x10e0)=NULL;
    return 0;
}
static void *init_resolve(uint32_t slot)
{
    switch(slot){
        case 0x258b0:return world_send;case 0x255c8:return world_enable;
        case 0x255b4:return init_string_construct;case 0x255b8:return init_get_title;
        case 0x255bc:return init_set_title;case 0x255c0:return init_string_destroy;
        case 0x2578c:return init_font_construct;case 0x2579c:return init_font_destroy;
        case 0x25500:return init_show;default:exit(2);
    }
}
static const struct fs98_world_init_services s_init_ui={.base_init=init_base,.art=init_art,.button=init_button,
    .text=init_text,.combo=init_combo,.base_font=init_font,.title=init_title};
static const struct fs98_world_weather_services s_initializer_services={&s_init_ui,init_resolve,init_resources,init_at_exit};
static void init_land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_saved_seh) : "memory");
    __builtin_longjmp(s_jump,1);
}
static EXCEPTION_DISPOSITION __cdecl init_catch(EXCEPTION_RECORD *record,void *frame,CONTEXT *context,void *dispatcher)
{
    (void)context;(void)dispatcher;
    if(record->ExceptionCode==THROW_CODE&&!(record->ExceptionFlags&(EXCEPTION_UNWINDING|EXCEPTION_EXIT_UNWIND))){
        RtlUnwind(frame,NULL,record,NULL);s_throw_at=0;
        if(!s_side)++s_init_unwinds;
        world_log("caught\n");init_land();
    }
    return ExceptionContinueSearch;
}
static LONG CALLBACK init_fault(EXCEPTION_POINTERS *exception)
{
    if(exception->ExceptionRecord->ExceptionCode==THROW_CODE)return EXCEPTION_CONTINUE_SEARCH;
    fprintf(stderr,"INIT FAULT side=%d case=%u pc=%p code=%lx\n",s_side,s_scenario,
        exception->ExceptionRecord->ExceptionAddress,exception->ExceptionRecord->ExceptionCode);ExitProcess(2);
}
static void init_invoke(void)
{
    int result=s_side?fs98_world_weather_init(s_fixture.object):((int(TC *)(void *))((uint8_t *)s_retail+0x3080))(s_fixture.object);
    if(!s_side&&!s_throw_at){
        if(result!=(s_scenario%5!=0&&!(s_scenario&16))||((s_scenario%5==0)&&s_calls!=2)||
            ((s_scenario%5!=0)&&(!strstr(s_logs[0],"combo ")||!strstr(s_logs[0],"string.destroy ")||U32(s_fixture.object,0x84)!=11))){
            fprintf(stderr,"initializer witness failed\n");exit(2);
        }
        ++s_init_checks;
    }
    if(s_registered_exit){void (*callback)(void)=s_registered_exit;s_registered_exit=NULL;callback();}
    world_log("return %x\n",(uint32_t)result);
}
static __attribute__((noinline)) void init_run(uint32_t throw_at)
{
    struct {void *previous,*handler;} catcher;
    s_fixture=s_initial;s_calls=s_length=0;s_logs[s_side][0]=0;s_throw_at=throw_at;s_registered_exit=NULL;
    if(s_side)*fs98_world_weather_get_font_state()=s_font_initial;
    else {memcpy((uint8_t *)s_retail+0x24978,s_font_initial.font,0x70);*(uint8_t *)((uint8_t *)s_retail+0x249e8)=(uint8_t)s_font_initial.flags;}
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_init_saved_seh));
    if(__builtin_setjmp(s_jump)==0){
        catcher.previous=(void *)s_init_saved_seh;catcher.handler=init_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");init_invoke();
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_saved_seh) : "memory");
}
static int init_compare(uint32_t throw_at)
{
    struct fs98_world_weather_font_state retail_font;
    s_side=0;init_run(throw_at);s_reference=s_fixture;
    memcpy(retail_font.font,(uint8_t *)s_retail+0x24978,0x70);retail_font.flags=*(uint8_t *)((uint8_t *)s_retail+0x249e8);
    s_font_reference=retail_font;
    s_side=1;init_run(throw_at);++s_comparisons;
    if(!memcmp(&s_reference,&s_fixture,sizeof(s_fixture))&&!strcmp(s_logs[0],s_logs[1])&&
        !memcmp(&s_font_reference,fs98_world_weather_get_font_state(),sizeof(s_font_reference))&&
        !memcmp(s_fixture.before,s_initial.before,32)&&!memcmp(s_fixture.after,s_initial.after,32))return 0;
    fprintf(stderr,"INIT DIFFER case=%u throw=%u state=%d font=%d\nRETAIL:\n%sOURS:\n%s",s_scenario,throw_at,
        !!memcmp(&s_reference,&s_fixture,sizeof(s_fixture)),!!memcmp(&s_font_reference,fs98_world_weather_get_font_state(),sizeof(s_font_reference)),s_logs[0],s_logs[1]);return 1;
}
int main(int argc,char **argv)
{
    uint32_t count=argc>1?strtoul(argv[1],NULL,0):2000,seed=argc>2?strtoul(argv[2],NULL,0):0x4a971b63;
    uint32_t position,limit,index;int unwind=argc>3&&!strcmp(argv[3],"unwind");
    if(!count||!seed)return 2;
    s_rng=seed;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);AddVectoredExceptionHandler(1,init_fault);
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);if(!s_retail)return 2;
    world_patch(0x258b0,world_send);world_patch(0x255c8,world_enable);
    world_patch(0x257c0,init_base_hook);world_patch(0x257c4,init_art_hook);world_patch(0x257c8,init_button_hook);
    world_patch(0x257dc,init_text_hook);world_patch(0x25790,init_combo_hook);world_patch(0x257d0,init_base_font);world_patch(0x257e0,init_title_hook);
    world_patch(0x255b4,init_string_construct);world_patch(0x255b8,init_get_title);world_patch(0x255bc,init_set_title);world_patch(0x255c0,init_string_destroy);
    world_patch(0x2578c,init_font_construct);world_patch(0x2579c,init_font_destroy);world_patch(0x256a8,init_on_exit);U32(s_retail,0x24b7c)=0xffffffff;world_patch(0x25500,init_show);
    world_patch(0x256bc,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"__CxxFrameHandler"));
    U32(s_retail,0x2495c)=init_resources();s_vtable[0xe4/4]=init_skin;memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));
    fs98_world_weather_set_services(&s_initializer_services);
    for(s_scenario=0;s_scenario<count;++s_scenario){
        s_operation=1;world_initialize();
        for(index=0;index<2;++index){PTR(s_fixture.descriptors[index],8)=s_init_title;
            U32(s_fixture.descriptors[index],0xc)=(s_scenario+index)&4;U32(s_fixture.descriptors[index],0x10)=(s_scenario+index)&8;}
        s_initial=s_fixture;
        for(index=0;index<28;++index)s_font_initial.font[index]=world_random();
        s_font_initial.flags=(s_scenario&1)|((s_scenario&2)?0x80:0);
        if(init_compare(0))return 1;
        limit=s_calls-1;
        if(unwind)for(position=1;position<=limit;++position)if(init_compare(position))return 1;
    }
    printf("World weather initializer: %u scenarios, %u comparisons, 0 differ; seed=%x unwinds=%u witnesses=%u\n",count,s_comparisons,seed,s_init_unwinds,s_init_checks);
    FreeLibrary(s_retail);return 0;
}
