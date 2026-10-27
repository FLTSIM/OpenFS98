#ifndef FS98_WORLD_POSITION_H
#define FS98_WORLD_POSITION_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_position_services {
    const struct fs98_world_services *events;
    const struct fs98_world_init_services *init;
    void (*group)(void *control, void *font);
    void (*remap)(struct fs98_world_rect *rect, void *control);
    void (*move)(void *control, int x, int y, int width, int height, int repaint);
    int (*message)(const char *text, uint32_t flags, uint32_t help);
};
void fs98_world_position_set_services(const struct fs98_world_position_services *services);
void fs98_world_position_set_object_services(const struct fs98_world_object_services *services, void *vtable,
    void *(*spin_vtable)(void), void (*base_exchange)(void *, void *));
void fs98_world_position_set_module(uint32_t module);
void *fs98_world_position_construct(void *object, void *parent);
void fs98_world_position_destroy(void *object);
void *fs98_world_position_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_position_create(uint32_t parent, void *descriptor);
void fs98_world_position_exchange(void *object, void *exchange);
int fs98_world_position_help(void *object, uint32_t unused, void *help);
int fs98_world_position_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_position_post_destroy(void *object);
void fs98_world_position_collect(void *object);
void fs98_world_position_populate(void *object);
void fs98_world_position_snapshot(void *object, uint32_t enabled);
int fs98_world_position_validate(void *object, uint32_t unused);
int fs98_world_position_error(void *object, uint32_t unused);
void fs98_world_position_reload(void *object);
void fs98_world_position_revert(void *object);
void fs98_world_position_facilities(void *object);
void fs98_world_position_changed(void *object, uint32_t offset);
void fs98_world_position_accept(void *object);
void fs98_world_position_cancel(void *object);
int fs98_world_position_init(void *object);
#endif
