#ifndef FS98_WORLD_ADVANCED_H
#define FS98_WORLD_ADVANCED_H
#include "world_objects.h"
/* Six classes in parent allocation order; tab order differs. */
enum fs98_world_advanced_kind {
    WORLD_ADVANCED_PARENT, WORLD_ADVANCED_CLOUDS, WORLD_ADVANCED_TEMPERATURE,
    WORLD_ADVANCED_VISIBILITY, WORLD_ADVANCED_WIND, WORLD_ADVANCED_PRESSURE,
    WORLD_ADVANCED_CLASS_COUNT
};
struct world_advanced_class {
    const struct fs98_world_control *controls;
    const uint32_t *cleanup;
    const uint16_t (*ddx)[2];
    const uint32_t *help;
    uint32_t count, template_id, size, base_ctor, base_dtor;
    uint32_t vtable_rva, constructor_rva, destructor_rva, scalar_rva;
};
void fs98_world_advanced_set_object_services(const struct fs98_world_object_services *services, void *const *vtables);
void fs98_world_advanced_set_resolver(void *(*resolve)(uint32_t));
void fs98_world_advanced_set_base_exchange(void (*exchange)(void *, void *));
void fs98_world_advanced_set_module(uint32_t module);
void *fs98_world_advanced_construct(void *object, void *parent, uint32_t kind);
void fs98_world_advanced_destroy(void *object, uint32_t kind);
void *fs98_world_advanced_delete(void *object, uint32_t flags, uint32_t kind);
uint32_t FS98_WORLD_STDCALL fs98_world_advanced_create(uint32_t parent, void *descriptor);
void fs98_world_advanced_exchange(void *object, void *exchange, uint32_t kind);
int fs98_world_advanced_help(void *object, uint32_t window, uint32_t command, uint32_t kind);
int fs98_world_advanced_context(void *object, uint32_t window, uint32_t kind);
void fs98_world_advanced_post_destroy(void *object);
#endif
