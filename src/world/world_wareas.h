#ifndef FS98_WORLD_WAREAS_H
#define FS98_WORLD_WAREAS_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_wareas_services {
    const struct fs98_world_init_services *ui;
    void *(*resolve)(uint32_t slot);
    uint32_t (*resources)(void);
    void *notify_vtable;
    int (*at_exit)(void (*callback)(void));
};
void fs98_world_wareas_set_services(const struct fs98_world_wareas_services *services);
void fs98_world_wareas_populate(void *object);
void fs98_world_wareas_selection(void *object);
void fs98_world_wareas_action(void *object, uint32_t action);
void fs98_world_wareas_check(void *object, uint32_t field, uint32_t offset);
void fs98_world_wareas_cancel(void *object);
void fs98_world_wareas_accept(void *object);
int fs98_world_wareas_has_descriptor(void *object);
int fs98_world_wareas_validate(void *object, uint32_t id);
int fs98_world_wareas_error(void *object, uint32_t id);
int fs98_world_wareas_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_wareas_context_map(uint32_t window);
int fs98_world_wareas_init(void *object);
struct fs98_world_wareas_font_state { uint32_t font[28], flags; };
struct fs98_world_wareas_font_state *fs98_world_wareas_get_font_state(void);
void fs98_world_wareas_font_cleanup(void);
void fs98_world_wareas_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_wareas_set_module(uint32_t module);
void *fs98_world_wareas_construct(void *object, void *parent);
void fs98_world_wareas_destroy(void *object);
void *fs98_world_wareas_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_wareas_create(uint32_t parent, void *descriptor);
void fs98_world_wareas_exchange(void *object, void *exchange);
int fs98_world_wareas_help(void *object, uint32_t unused, void *help);
void fs98_world_wareas_post_destroy(void *object);
#endif
