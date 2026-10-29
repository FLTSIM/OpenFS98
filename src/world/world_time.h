#ifndef FS98_WORLD_TIME_H
#define FS98_WORLD_TIME_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_time_services {
    const struct fs98_world_init_services *ui;
    void *(*resolve)(uint32_t slot);
    uint32_t (*resources)(void);
    void *notify_vtable;
};
struct world_time_class {
    const struct fs98_world_control *controls;
    const uint32_t *cleanup;
    const uint16_t (*ddx)[2];
    const uint32_t *help;
    uint32_t count, template_id, size, base_ctor, base_dtor;
};
void fs98_world_time_set_services(const struct fs98_world_time_services *services);
const struct fs98_world_time_services *fs98_world_time_get_services(void);
void *fs98_world_time_api(uint32_t slot);
void fs98_world_time_set_object_services(const struct fs98_world_object_services *services, void *parent, void *season, void *clock);
void fs98_world_time_set_module(uint32_t module);
void *fs98_world_time_construct(void *object, void *parent, uint32_t kind);
void fs98_world_time_destroy(void *object, uint32_t kind);
void *fs98_world_time_delete(void *object, uint32_t flags, uint32_t kind);
uint32_t FS98_WORLD_STDCALL fs98_world_time_create(uint32_t parent, void *descriptor);
void fs98_world_time_exchange(void *object, void *exchange, uint32_t kind);
int fs98_world_time_help(void *object, uint32_t window, uint32_t command, uint32_t kind);
int fs98_world_time_init(void *object);
int fs98_world_time_frame_validate(void *object, uint32_t unused);
void fs98_world_time_page_init(void *object, uint32_t kind);
void fs98_world_time_bind_pages(void *object);
int fs98_world_time_has_descriptor(void *object);
void fs98_world_time_populate(void *object, uint32_t kind);
int fs98_world_time_validate(void *object, uint32_t id, uint32_t kind);
int fs98_world_time_error(void *object, uint32_t id, uint32_t kind);
void fs98_world_time_radio(void *object, uint32_t manual, uint32_t kind);
void fs98_world_time_selection(void *object, uint32_t kind);
int fs98_world_time_reload(void *object, uint32_t kind);
void fs98_world_time_accept(void *object);
void fs98_world_time_cancel(void *object);
void fs98_world_time_close(void *object);
#endif
