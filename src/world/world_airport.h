#ifndef FS98_WORLD_AIRPORT_H
#define FS98_WORLD_AIRPORT_H
#include "world_objects.h"
void fs98_world_airport_set_object_services(const struct fs98_world_object_services *services, void *vtable,
    void (*base_exchange)(void *, void *));
void fs98_world_airport_set_module(uint32_t module);
void *fs98_world_airport_construct(void *object, void *parent);
void fs98_world_airport_destroy(void *object);
void *fs98_world_airport_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_airport_create(uint32_t parent, void *descriptor);
void fs98_world_airport_exchange(void *object, void *exchange);
int fs98_world_airport_help(void *object, uint32_t unused, void *help);
int fs98_world_airport_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_airport_post_destroy(void *object);
struct fs98_world_airport_services {
    void *(*resolve)(uint32_t slot);
    void *(*host)(void);
    uint32_t (*resource_module)(void);
};
void fs98_world_airport_set_services(const struct fs98_world_airport_services *services);
void fs98_world_airport_allocate(void *object);
void fs98_world_airport_release(void *object);
void fs98_world_airport_populate(void *object);
void fs98_world_airport_runways(void *object);
void fs98_world_airport_clear(void *object);
void fs98_world_airport_restore(void *object);
void fs98_world_airport_refresh(void *object);
void fs98_world_airport_facilities(void *object);
void fs98_world_airport_airport(void *object);
void fs98_world_airport_double_click(void *object);
void fs98_world_airport_airport_edit(void *object);
void fs98_world_airport_name(void *object);
void fs98_world_airport_name_edit(void *object);
void fs98_world_airport_accept(void *object);
void fs98_world_airport_cancel(void *object);
void fs98_world_airport_mode(void *object, uint32_t mode);
void fs98_world_airport_mode_button(void *object, uint32_t mode);
void fs98_world_airport_place(void *object, uint32_t index);
void fs98_world_airport_dropdown(void *object, uint32_t index, int close);
int fs98_world_airport_search(void *object, int32_t *low, int32_t *high, const char *text, void *rows);
int fs98_world_airport_init(void *object);
#endif
