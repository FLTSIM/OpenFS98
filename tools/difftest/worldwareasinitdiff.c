/* Guarded initializer, including genuine notification-construction unwinds. */
#define main world_wareas_core_main
#include "worldwareasdiff.c"
#undef main
static uint8_t s_notify[0x29c],s_notify_initial[0x29c],s_notify_reference[0x29c];
static uint32_t s_init_seh;
static void *s_check_vtable[50];
static uint32_t s_font_marker[28];
static struct fs98_world_wareas_font_state s_font_initial,s_font_reference;
static uint32_t init_offset(const void *p)
{
    if(p==(void *)((uint8_t *)s_retail+0x24a70)||p==fs98_world_wareas_get_font_state()->font)return 0xf000;
    if(p==s_notify)return 0xf100;
    if(p==s_font_marker)return 0xf200;
    return world_offset(p);
}
static int init_base(void *object) { world_log("base.init %x\n",world_offset(object));return 0; }
static int TC init_base_hook(void *object) { return init_base(object); }
static int TC init_skin(void *object,uint32_t bitmap,uint32_t module,uint32_t flags)
{ world_log("skin %x %x %x %x\n",world_offset(object),bitmap,module,flags);return s_scenario%7!=0; }
static uint32_t init_resources(void) { return 0x7311; }
static void *init_font(int id) { world_log("font %x\n",id);return s_font_marker; }
static void *SC init_font_hook(int id) { return init_font(id); }
static void TC init_font_construct(void *object,void *font)
{ world_log("font.construct %x %x\n",init_offset(object),init_offset(font));memset(object,0x59,112); }
static int init_exit(void (*callback)(void)) { (void)callback;world_log("atexit\n");return 0; }
static int init_art(void *control,uint32_t bitmap,void *dib,void *palette,uint32_t flags)
{ world_log("art %x %x %x %x %x\n",world_offset(control),bitmap,world_offset(dib),world_offset(palette),flags);return -1; }
static int TC init_art_hook(void *c,uint32_t b,void *d,void *p,uint32_t f) { return init_art(c,b,d,p,f); }
static int init_button(void *c,void *f,void *a,uint32_t b,void *d,void *p,uint32_t flags,int color,uint32_t module,int x,int y,struct fs98_world_rect r)
{ world_log("button %x %x %x %x %x %x %x %x %x %x %x rect=%d,%d,%d,%d\n",world_offset(c),world_offset(f),world_offset(a),b,world_offset(d),world_offset(p),flags,(uint32_t)color,module,x,y,r.left,r.top,r.right,r.bottom);return -1; }
static int TC init_button_hook(void *c,void *f,void *a,uint32_t b,void *d,void *p,uint32_t flags,int color,uint32_t module,int x,int y,struct fs98_world_rect r)
{ return init_button(c,f,a,b,d,p,flags,color,module,x,y,r); }
static void init_text(void *control,void *font)
{ world_log("bind.text %x %x\n",world_offset(control),init_offset(font)); }
static void TC init_text_hook(void *control,void *font) { init_text(control,font); }
static void init_title(void *control,const char *text) { world_log("title %x [%s]\n",world_offset(control),text); }
static void TC init_title_hook(void *control,const char *text) { init_title(control,text); }
static void TC init_list(void *control,void *font,void *label)
{ world_log("list %x %x %x\n",world_offset(control),init_offset(font),world_offset(label)); }
static void init_edit(void *control,void *font,uint32_t foreground,uint32_t background,uint32_t error,void *label,uint32_t flags)
{ world_log("edit %x %x %x %x %x %x %x\n",world_offset(control),init_offset(font),foreground,background,error,label?world_offset(label):0,flags); }
static void TC init_edit_hook(void *c,void *f,uint32_t fg,uint32_t bg,uint32_t e,void *l,uint32_t flags) { init_edit(c,f,fg,bg,e,l,flags); }
static void TC init_group(void *control,void *font) { world_log("group %x %x\n",world_offset(control),init_offset(font)); }
static int TC init_check(void *control,void *font,uint32_t bitmap,void *dib,void *palette,uint32_t module)
{ world_log("check.init %x %x %x %x %x %x\n",world_offset(control),world_offset(font),bitmap,world_offset(dib),world_offset(palette),module);return -1; }
static int SC init_string(uint32_t module,uint32_t id,char *text,int capacity)
{ world_log("string %x %x %x %x\n",module,id,world_offset(text),capacity);strcpy(text,"units");return 5; }
static void *init_allocate(uint32_t size)
{ world_log("allocate %x\n",size);return s_scenario%5?s_notify:NULL; }
static void *TC init_notify(void *object,uint32_t first,uint32_t second)
{ world_log("notify.construct %x %x %x\n",init_offset(object),first,second);U32(object,0)=0x29c;return object; }
static void SC init_notify_delete(void *object) { world_log("notify.delete %x\n",init_offset(object)); }
static int SC init_length(const char *text) { world_log("length [%s]\n",text);return strlen(text); }
static void *init_resolve(uint32_t slot)
{
    switch(slot){case 0x2578c:return init_font_construct;case 0x25890:return init_string;
    case 0x25718:return init_list;case 0x25730:return init_group;case 0x2584c:return init_allocate;
    case 0x257cc:return init_notify;case 0x257f4:return init_notify_delete;case 0x254dc:return init_length;
    default:return world_resolve(slot);}
}
static const struct fs98_world_init_services s_init_ui={
    .base_init=init_base,.art=init_art,.button=init_button,.text=init_text,.base_font=init_font,.edit=init_edit,.title=init_title
};
static struct fs98_world_wareas_services s_init_services={&s_init_ui,init_resolve,init_resources,NULL,init_exit};
static EXCEPTION_DISPOSITION __cdecl init_catch(EXCEPTION_RECORD *record,void *frame,CONTEXT *context,void *dispatcher)
{
    (void)context;(void)dispatcher;
    if(record->ExceptionCode==THROW_CODE&&!(record->ExceptionFlags&(EXCEPTION_UNWINDING|EXCEPTION_EXIT_UNWIND))){
        RtlUnwind(frame,NULL,record,NULL);s_throw_at=0;world_log("caught\n");if(!s_side)++s_throws;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_seh) : "memory");__builtin_longjmp(s_jump,1);
    }
    return ExceptionContinueSearch;
}
static LONG WINAPI init_fatal(EXCEPTION_POINTERS *e)
{ fprintf(stderr,"INIT FAULT code=%lx pc=%p case=%u side=%d throw=%u\n",e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress,s_scenario,s_side,s_throw_at);ExitProcess(2);return 0; }
static __attribute__((noinline)) void init_run(uint32_t throw_at)
{
    struct {void *previous,*handler;} catcher;
    s_fixture=s_initial;memcpy(s_notify,s_notify_initial,sizeof(s_notify));
    memcpy((uint8_t *)s_retail+0x24a70,s_font_initial.font,112);U32(s_retail,0x24ae0)=s_font_initial.flags;
    *fs98_world_wareas_get_font_state()=s_font_initial;
    s_calls=s_length=0;s_logs[s_side][0]=0;s_throw_at=throw_at;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_init_seh));
    if(__builtin_setjmp(s_jump)==0){
        int result;catcher.previous=(void *)s_init_seh;catcher.handler=init_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        result=s_side?fs98_world_wareas_init(s_fixture.object):((int(TC *)(void *))((uint8_t *)s_retail+0x10810))(s_fixture.object);
        world_log("return %x\n",result);
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_seh) : "memory");
}
static int init_compare(uint32_t throw_at)
{
    struct fs98_world_wareas_font_state font;
    s_side=0;init_run(throw_at);s_reference=s_fixture;memcpy(s_notify_reference,s_notify,sizeof(s_notify));
    memcpy(s_font_reference.font,(uint8_t *)s_retail+0x24a70,112);s_font_reference.flags=U32(s_retail,0x24ae0)&0xff;
    s_side=1;init_run(throw_at);font=*fs98_world_wareas_get_font_state();++s_comparisons;
    if(!memcmp(&s_reference,&s_fixture,sizeof(s_fixture))&&!memcmp(s_notify_reference,s_notify,sizeof(s_notify))&&!memcmp(&s_font_reference,&font,sizeof(font))&&!strcmp(s_logs[0],s_logs[1]))return 0;
    fprintf(stderr,"INIT DIFFER scenario=%u throw=%u state=%d notify=%d font=%d\nRETAIL:\n%sOURS:\n%s",s_scenario,throw_at,!!memcmp(&s_reference,&s_fixture,sizeof(s_fixture)),!!memcmp(s_notify_reference,s_notify,sizeof(s_notify)),!!memcmp(&s_font_reference,&font,sizeof(font)),s_logs[0],s_logs[1]);return 1;
}
int main(int argc,char **argv)
{
    uint32_t count=argc>1?strtoul(argv[1],NULL,0):2000,seed=argc>2?strtoul(argv[2],NULL,0):0x4a971b63,index,limit;
    int unwind=argc>3&&!strcmp(argv[3],"unwind");
    if(!count||!seed)return 2;
    s_rng=seed;SetErrorMode(SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(init_fatal);
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);if(!s_retail)return 2;
    s_init_services.notify_vtable=(uint8_t *)s_retail+0x1e1e8;
    world_patch(0x257c0,init_base_hook);world_patch(0x2578c,init_font_construct);world_patch(0x25890,init_string);
    world_patch(0x257c4,init_art_hook);world_patch(0x257c8,init_button_hook);world_patch(0x257dc,init_text_hook);
    world_patch(0x257e0,init_title_hook);world_patch(0x25718,init_list);world_patch(0x25740,init_edit_hook);
    world_patch(0x25730,init_group);world_patch(0x2584c,init_allocate);world_patch(0x257cc,init_notify);world_patch(0x257f4,init_notify_delete);
    world_patch(0x254dc,init_length);world_patch(0x257d0,init_font_hook);world_patch(0x257a8,world_diagnostic);
    world_patch(0x258b0,world_send);world_patch(0x255bc,world_set_text);world_patch(0x255c8,world_enable);world_patch(0x2577c,world_check);
    world_patch(0x256bc,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"__CxxFrameHandler"));
    /* The retail atexit thunk takes one cdecl argument. */
    {
        DWORD old;uint8_t *entry=(uint8_t *)s_retail+0x19b70;
        VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&old);entry[0]=0xe9;*(uint32_t *)(entry+1)=(uint32_t)init_exit-(uint32_t)entry-5;VirtualProtect(entry,5,old,&old);
    }
    fs98_world_wareas_set_services(&s_init_services);s_vtable[0xe4/4]=init_skin;s_check_vtable[0xc4/4]=init_check;
    memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));
    for(s_scenario=0;s_scenario<count;++s_scenario){
        s_operation=12;world_initialize();
        PTR(s_fixture.object,0x2590)=PTR(s_fixture.object,0x2658)=s_check_vtable;
        PTR(s_fixture.object,0x10f8)=s_fixture.descriptors[0];
        s_initial=s_fixture;
        for(index=0;index<112;++index)((uint8_t *)s_font_initial.font)[index]=(uint8_t)world_random();
        s_font_initial.flags=(s_scenario&1);memset(s_notify_initial,0x76,sizeof(s_notify_initial));
        U32(s_retail,0x2495c)=init_resources();
        if(init_compare(0))return 1;
        limit=s_calls-1;
        if(unwind)for(index=1;index<=limit;++index)if(init_compare(index))return 1;
    }
    printf("World weather areas initializer: %u scenarios, %u comparisons, %u real unwinds, 0 differ; seed=%x\n",count,s_comparisons,s_throws,seed);return 0;
}
