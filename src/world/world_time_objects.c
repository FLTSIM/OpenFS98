/* Time/season parent and both owned pages; no retail class code is reused. */
#include "world_time.h"
#include "world_unwind.h"
#include "world_time_tables.inc"
#define AT(p,n) ((uint8_t *)(p)+(n))
#define PTR WORLD_PTR
#define U32 WORLD_U32
#define TC FS98_WORLD_THISCALL
static const struct fs98_world_object_services *s_services;
static void *s_vtables[3];
static uint32_t s_module;
void fs98_world_time_set_object_services(const struct fs98_world_object_services *services, void *parent, void *season, void *clock)
{ s_services=services;s_vtables[0]=parent;s_vtables[1]=season;s_vtables[2]=clock; }
void fs98_world_time_set_module(uint32_t module){s_module=module;}
struct time_object_unwind {
    struct fs98_world_unwind frame;
    void *object;
    const struct world_time_class *class;
    volatile uint32_t constructed, base;
};
static void time_destroy_remaining(struct time_object_unwind *state,int unwinding)
{
    while(state->constructed){
        const struct fs98_world_control *control=&state->class->controls[--state->constructed];
        s_services->destroy(AT(state->object,control->offset),unwinding?state->class->cleanup[state->constructed]:control->destructor);
    }
    if(state->base){state->base=0;s_services->destroy(state->object,state->class->base_dtor);}
}
static void time_object_cleanup(struct fs98_world_unwind *frame)
{ time_destroy_remaining((void *)frame,1); }
WORLD_NOINLINE void *fs98_world_time_construct(void *object,void *parent,uint32_t kind)
{
    struct time_object_unwind state;
    state.object=object;state.class=&s_time_classes[kind];state.constructed=state.base=0;
    world_unwind_enter(&state.frame,time_object_cleanup);
    if(kind)((void *(TC *)(void *,uint32_t,void *))fs98_world_time_api(state.class->base_ctor))(object,state.class->template_id,parent);
    else s_services->base_construct(object,state.class->template_id,parent);
    state.base=1;
    while(state.constructed<state.class->count){
        const struct fs98_world_control *control=&state.class->controls[state.constructed];
        s_services->construct(AT(object,control->offset),control->constructor);++state.constructed;
    }
    if(kind)PTR(object,kind==1?0x1bc8:0x1db0)=fs98_world_time_api(0x25758);
    PTR(object,0)=s_vtables[kind];
    if(kind==2)U32(object,0x1118)=0;
    world_unwind_leave(&state.frame);return object;
}
WORLD_NOINLINE void fs98_world_time_destroy(void *object,uint32_t kind)
{
    struct time_object_unwind state;
    state.object=object;state.class=&s_time_classes[kind];state.constructed=state.class->count;state.base=1;
    world_unwind_enter(&state.frame,time_object_cleanup);
    if(!kind)PTR(object,0)=s_vtables[0];
    else PTR(object,kind==1?0x1bc8:0x1db0)=fs98_world_time_api(0x25758);
    time_destroy_remaining(&state,0);world_unwind_leave(&state.frame);
}
void *fs98_world_time_delete(void *object,uint32_t flags,uint32_t kind)
{ fs98_world_time_destroy(object,kind);if(flags&1)s_services->ui_delete(object);return object; }
struct time_factory_unwind {struct fs98_world_unwind frame;void *object;volatile int live;};
static void time_factory_cleanup(struct fs98_world_unwind *frame)
{
    struct time_factory_unwind *state=(void *)frame;
    if(state->live){state->live=0;s_services->dialog_delete(state->object);}
}
static WORLD_NOINLINE uint32_t time_create(uint32_t parent,void *descriptor)
{
    struct time_factory_unwind state;void *object;(void)parent;
    state.object=NULL;state.live=0;world_unwind_enter(&state.frame,time_factory_cleanup);
    object=s_services->allocate(0x212c);state.object=object;state.live=1;
    if(object)object=fs98_world_time_construct(object,NULL,0);
    *(void *volatile *)AT(object,0x10dc)=descriptor;U32(object,0x80)=s_module;state.live=0;
    s_services->create(object,0x6aa,NULL);s_services->show(object,1);
    world_unwind_leave(&state.frame);return U32(object,0x20);
}
uint32_t FS98_WORLD_STDCALL fs98_world_time_create(uint32_t parent,void *descriptor){return time_create(parent,descriptor);}
void fs98_world_time_exchange(void *object,void *exchange,uint32_t kind)
{
    uint32_t index;
    for(index=0;index<s_time_classes[kind].count;++index)
        s_services->ddx(exchange,s_time_classes[kind].ddx[index][1],AT(object,s_time_classes[kind].ddx[index][0]));
}
int fs98_world_time_help(void *object,uint32_t window,uint32_t command,uint32_t kind)
{ (void)object;return s_services->help(window,"fs98.hlp",command,s_time_classes[kind].help); }
