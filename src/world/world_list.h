#ifndef FS98_WORLD_LIST_H
#define FS98_WORLD_LIST_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_list_services {
    const struct fs98_world_services *events;
    const struct fs98_world_init_services *init;
    void (*string_assign)(void *string, const char *text);
    void (*string_append)(void *string, const char *text);
    int (*font_tabs)(void *font, int count, const int *tabs);
    void (*list_init)(void *control, void *font, void *label);
};
void fs98_world_list_set_services(const struct fs98_world_list_services *services);
void fs98_world_list_collect(void *object);
void fs98_world_list_populate(void *object);
void fs98_world_list_accept(void *object);
void fs98_world_list_cancel(void *object);
void fs98_world_list_action(void *object, uint32_t callback_offset);
void fs98_world_list_delete(void *object);
void fs98_world_list_selection(void *object);
void fs98_world_list_double_click(void *object);
int fs98_world_list_init(void *object);
void fs98_world_list_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_list_set_module(uint32_t module);
void *fs98_world_list_construct(void *object, void *parent);
void fs98_world_list_destroy(void *object);
void *fs98_world_list_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_list_create(uint32_t parent, void *descriptor);
void fs98_world_list_exchange(void *object, void *exchange);
int fs98_world_list_help(void *object, uint32_t unused, void *help);
int fs98_world_list_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_list_post_destroy(void *object);
#endif
