#ifndef FS98_WORLD_FILES_H
#define FS98_WORLD_FILES_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_files_services {
    const struct fs98_world_services *events;
    const struct fs98_world_init_services *init;
    void (*list_init)(void *control, void *font, void *label);
    void (*group)(void *control, void *font);
    int (*post)(uint32_t window, uint32_t message, uint32_t first, uint32_t second);
    int (*erase)(void *object, void *dc);
};
void fs98_world_files_set_services(const struct fs98_world_files_services *services);
void fs98_world_files_collect(void *object);
void fs98_world_files_populate(void *object);
void fs98_world_files_accept(void *object);
void fs98_world_files_cancel(void *object);
void fs98_world_files_action(void *object, uint32_t callback_offset);
void fs98_world_files_enable(void *object);
int fs98_world_files_erase(void *object, void *dc);
void fs98_world_files_selection(void *object);
int fs98_world_files_validate(void *object);
uint32_t fs98_world_files_check(void *object);
int fs98_world_files_init(void *object);
void fs98_world_files_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_files_set_module(uint32_t module);
void *fs98_world_files_construct(void *object, void *parent);
void fs98_world_files_destroy(void *object);
void *fs98_world_files_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_files_create(uint32_t parent, void *descriptor);
void fs98_world_files_exchange(void *object, void *exchange);
int fs98_world_files_help(void *object, uint32_t unused, void *help);
int fs98_world_files_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_files_post_destroy(void *object);
#endif
