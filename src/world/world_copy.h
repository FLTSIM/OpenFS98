#ifndef FS98_WORLD_COPY_H
#define FS98_WORLD_COPY_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_copy_services {
    int (*send)(uint32_t window, uint32_t message, uint32_t first, uint32_t second);
    int (*check)(void *control, int checked);
    int (*error)(void *object, void *control, uint32_t message);
    int (*enable)(void *control, int enable);
    int (*post)(uint32_t window, uint32_t message, uint32_t first, uint32_t second);
};
void fs98_world_copy_set_services(const struct fs98_world_copy_services *services);
void fs98_world_copy_collect(void *object);
void fs98_world_copy_populate(void *object);
void fs98_world_copy_cancel(void *object);
void fs98_world_copy_accept(void *object);
int fs98_world_copy_has_descriptor(void *object);
int fs98_world_copy_check(void *object);
int fs98_world_copy_error(void *object);
void fs98_world_copy_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_copy_set_module(uint32_t module);
void *fs98_world_copy_construct(void *object, void *parent);
void fs98_world_copy_destroy(void *object);
void *fs98_world_copy_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_copy_create(uint32_t parent, void *descriptor);
void fs98_world_copy_exchange(void *object, void *exchange);
int fs98_world_copy_help(void *object, uint32_t unused, void *help);
int fs98_world_copy_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_copy_post_destroy(void *object);
struct fs98_world_copy_init_services {
    const struct fs98_world_init_services *ui;
    uint32_t (*resources)(void);
};
void fs98_world_copy_set_init_services(const struct fs98_world_copy_init_services *services);
int fs98_world_copy_init(void *object);
#endif
