#ifndef FS98_WORLD_OBJECTS_H
#define FS98_WORLD_OBJECTS_H
#include "world_internal.h"
struct fs98_world_control { uint32_t offset, constructor, destructor; };
struct fs98_world_object_services {
    void *(*base_construct)(void *object, uint32_t template_id, void *parent);
    void (*construct)(void *object, uint32_t import_slot);
    void (*destroy)(void *object, uint32_t import_slot);
    void *(*allocate)(uint32_t bytes);
    void (*ui_delete)(void *object);
    void (*dialog_delete)(void *object);
    int (*create)(void *object, uint32_t template_id, void *parent);
    int (*show)(void *object, int show);
    int (*message)(const char *text, uint32_t flags, uint32_t help);
    void (*ddx)(void *exchange, uint32_t id, void *control);
    void (*exit_cleanly)(void *exception);
    int (*help)(uint32_t window, const char *filename, uint32_t command, const uint32_t *map);
};
void fs98_world_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_set_module(uint32_t module);
void *fs98_world_area_construct(void *object, void *parent);
void fs98_world_area_destroy(void *object);
void *fs98_world_area_delete(void *object, uint32_t flags);
void fs98_world_area_exchange(void *object, void *exchange);
int fs98_world_area_help(void *object, uint32_t unused, void *help);
int fs98_world_area_context(void *object, uint32_t window, uint32_t unused);
uint32_t FS98_WORLD_STDCALL fs98_world_area_create_add(uint32_t parent, void *descriptor);
uint32_t FS98_WORLD_STDCALL fs98_world_area_create_edit(uint32_t parent, void *descriptor);
#endif
