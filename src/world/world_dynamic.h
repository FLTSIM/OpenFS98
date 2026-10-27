#ifndef FS98_WORLD_DYNAMIC_H
#define FS98_WORLD_DYNAMIC_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_dynamic_services {
    int (*send)(uint32_t window, uint32_t message, uint32_t first, uint32_t second);
    int (*check)(void *control, int checked);
    int (*error)(void *object, void *control, uint32_t message);
    void (*focus)(void *control);
    int (*post)(uint32_t window, uint32_t message, uint32_t first, uint32_t second);
};
void fs98_world_dynamic_set_services(const struct fs98_world_dynamic_services *services);
void fs98_world_dynamic_collect(void *object);
void fs98_world_dynamic_populate(void *object);
void fs98_world_dynamic_cancel(void *object);
void fs98_world_dynamic_accept(void *object);
int fs98_world_dynamic_has_descriptor(void *object);
int fs98_world_dynamic_check(void *object);
int fs98_world_dynamic_error(void *object);
void fs98_world_dynamic_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_dynamic_set_module(uint32_t module);
void *fs98_world_dynamic_construct(void *object, void *parent);
void fs98_world_dynamic_destroy(void *object);
void *fs98_world_dynamic_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_dynamic_create(uint32_t parent, void *descriptor);
void fs98_world_dynamic_exchange(void *object, void *exchange);
int fs98_world_dynamic_help(void *object, uint32_t unused, void *help);
int fs98_world_dynamic_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_dynamic_post_destroy(void *object);
void fs98_world_dynamic_close(void *object);
struct fs98_world_dynamic_init_services {
    const struct fs98_world_init_services *ui;
    void *(*resolve)(uint32_t slot);
    uint32_t (*resources)(void);
    void *notify_vtable;
};
void fs98_world_dynamic_set_init_services(const struct fs98_world_dynamic_init_services *services);
int fs98_world_dynamic_init(void *object);
void *fs98_world_dynamic_notify_delete(void *object, uint32_t flags);
#endif
