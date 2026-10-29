#ifndef FS98_WORLD_ADVANCED_INTERNAL_H
#define FS98_WORLD_ADVANCED_INTERNAL_H
#include "world_advanced.h"
#include "world_init.h"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot, type) ((type)fs98_world_advanced_api(slot))
#define RUN(rva, a, b) fs98_world_advanced_call(object, rva, (uint32_t)(a), (uint32_t)(b))
struct fs98_world_advanced_services {
    const struct fs98_world_init_services *ui;
    void *(*resolve)(uint32_t);
    uint32_t (*resources)(void);
    void *notify_vtable;
};
void fs98_world_advanced_set_services(const struct fs98_world_advanced_services *services);
const struct fs98_world_advanced_services *fs98_world_advanced_services(void);
void *fs98_world_advanced_api(uint32_t slot);
const struct world_advanced_class *fs98_world_advanced_class(uint32_t kind);
uint32_t fs98_world_advanced_call(void *object, uint32_t rva, uint32_t first, uint32_t second);
uint32_t fs98_world_advanced_cloud(void *object, uint32_t rva, uint32_t first, uint32_t second);
uint32_t fs98_world_advanced_temperature(void *object, uint32_t rva, uint32_t first, uint32_t second);
uint32_t fs98_world_advanced_wind(void *object, uint32_t rva, uint32_t first, uint32_t second);
void *fs98_world_advanced_allocate(void *parent, uint32_t kind);
static inline uint32_t advanced_send(void *object, uint32_t control, uint32_t message, uint32_t first, uint32_t second)
{ return API(0x258b0, uint32_t(SC *)(uint32_t,uint32_t,uint32_t,uint32_t))(U32(object,control+0x20),message,first,second); }
static inline void advanced_text(void *object,uint32_t control,const char *text)
{ API(0x255bc,void(TC *)(void *,const char *))(AT(object,control),text); }
static inline void advanced_get(void *object,uint32_t control,void *record)
{ API(0x25598,int(TC *)(void *,char *,uint32_t))(AT(object,control),PTR(record,4),U32(record,8)); }
static inline void advanced_enable(void *object,uint32_t control,int enable)
{ API(0x255c8,int(TC *)(void *,int))(AT(object,control),enable); }
static inline void advanced_check(void *object,uint32_t control,uint32_t value)
{ API(0x2577c,int(TC *)(void *,uint32_t))(AT(object,control),value); }
static inline uint32_t advanced_callback(void *object,uint32_t owner_offset,uint32_t callback)
{ void *owner=PTR(object,owner_offset);return ((uint32_t(SC *)(void *))PTR(owner,callback))(owner); }
static inline void advanced_post(void *object,uint32_t window)
{ API(0x258a0,int(SC *)(uint32_t,uint32_t,uint32_t,uint32_t))(U32(object,0x20),0x5fa,window,0); }
static inline uint32_t advanced_validate_again(void *object,uint32_t id)
{ return ((uint32_t(TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,id); }
static inline void advanced_buttons(void *parent,int mode)
{ fs98_world_advanced_call(parent,0x2bf0,(uint32_t)mode,0); }
static inline void advanced_edit_init(void *object,uint32_t edit,uint32_t label,uint32_t flags,uint32_t limit)
{
    fs98_world_advanced_services()->ui->edit(AT(object,edit),fs98_world_advanced_services()->ui->base_font(2),0xffffff,0,0xff0000,AT(object,label),flags);
    U32(object,edit+0x64)=limit;
}
static inline void advanced_label(void *object,uint32_t control,uint32_t font)
{ fs98_world_advanced_services()->ui->text(AT(object,control),AT(object,font)); }
static inline void advanced_title(void *object,uint32_t control,const char *title)
{ fs98_world_advanced_services()->ui->title(AT(object,control),title); }
static inline void advanced_sprite(void *object,uint32_t control,uint32_t bitmap,uint32_t font)
{
    void *child=AT(object,control);
    ((int(TC *)(void *,void *,uint32_t,uint32_t,uint32_t,uint32_t))PTR(PTR(child,0),0xc4))(child,AT(object,font),bitmap,U32(object,0x10fc),U32(object,0x10dc),fs98_world_advanced_services()->resources());
}
#endif
