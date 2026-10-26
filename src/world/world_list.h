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
#endif
