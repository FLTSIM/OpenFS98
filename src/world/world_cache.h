#ifndef FS98_WORLD_CACHE_H
#define FS98_WORLD_CACHE_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_cache_services {
    const struct fs98_world_services *events;
    const struct fs98_world_init_services *init;
    void (*string_assign)(void *string, const char *text);
    void (*string_append)(void *string, const char *text);
    int (*font_tabs)(void *font, int count, const int *tabs);
    void (*list_init)(void *control, void *font, void *label);
    void (*group)(void *control, void *font);
};
void fs98_world_cache_set_services(const struct fs98_world_cache_services *services);
void fs98_world_cache_collect(void *object);
void fs98_world_cache_populate(void *object);
void fs98_world_cache_accept(void *object);
void fs98_world_cache_cancel(void *object);
void fs98_world_cache_action(void *object, uint32_t callback_offset);
void fs98_world_cache_delete(void *object);
void fs98_world_cache_selection(void *object);
int fs98_world_cache_validate(void *object);
uint32_t fs98_world_cache_check(void *object);
int fs98_world_cache_init(void *object);
void fs98_world_cache_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_cache_set_module(uint32_t module);
void *fs98_world_cache_construct(void *object, void *parent);
void fs98_world_cache_destroy(void *object);
void *fs98_world_cache_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_cache_create(uint32_t parent, void *descriptor);
void fs98_world_cache_exchange(void *object, void *exchange);
int fs98_world_cache_help(void *object, uint32_t unused, void *help);
int fs98_world_cache_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_cache_post_destroy(void *object);
#endif
