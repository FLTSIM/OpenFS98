#ifndef FS98_WORLD_PRIORITY_H
#define FS98_WORLD_PRIORITY_H

#include "world_objects.h"
#include "world_init.h"

struct fs98_world_priority_services {
    void (*string_assign)(void *string, const char *text);
    void (*string_append)(void *string, const char *text);
    int (*font_tabs)(void *font, int count, const int *tabs);
    void (*list_init)(void *control, void *font, void *label);
};
void fs98_world_priority_set_services(const struct fs98_world_priority_services *services);
void fs98_world_priority_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_priority_set_module(uint32_t module);
const struct fs98_world_init_services *fs98_world_get_init_services(void);
uint32_t fs98_world_get_resource_module(void);
void *fs98_world_priority_construct(void *object, void *parent);
void fs98_world_priority_destroy(void *object);
void *fs98_world_priority_delete(void *object, uint32_t flags);
void fs98_world_priority_exchange(void *object, void *exchange);
int fs98_world_priority_help(void *object, uint32_t unused, void *help);
int fs98_world_priority_context(void *object, uint32_t window, uint32_t unused);
uint32_t FS98_WORLD_STDCALL fs98_world_priority_create(uint32_t parent, void *descriptor);
void fs98_world_priority_post_destroy(void *object);
void fs98_world_priority_cancel(void *object);
int fs98_world_priority_init(void *object);
void fs98_world_priority_action(void *object, uint32_t callback_offset);
void fs98_world_priority_selection(void *object);
void fs98_world_priority_accept(void *object);
void fs98_world_priority_collect(void *object);
void fs98_world_priority_populate(void *object);
int fs98_world_priority_error(void *object, uint32_t unused);
int fs98_world_priority_validate(void *object, uint32_t unused);

#endif
