#ifndef FS98_WORLD_WEATHER_H
#define FS98_WORLD_WEATHER_H
#include "world_objects.h"
#include "world_init.h"
struct fs98_world_weather_services {
    const struct fs98_world_init_services *ui;
    void *(*resolve)(uint32_t slot);
    uint32_t (*resources)(void);
    int (*at_exit)(void (*callback)(void));
};
void fs98_world_weather_set_services(const struct fs98_world_weather_services *services);
void fs98_world_weather_base_exchange(void *object, void *exchange);
void fs98_world_weather_collect(void *object);
void fs98_world_weather_populate(void *object);
void fs98_world_weather_changed(void *object);
void fs98_world_weather_cancel(void *object);
void fs98_world_weather_accept(void *object);
int fs98_world_weather_has_descriptor(void *object);
int fs98_world_weather_validate(void *object);
int fs98_world_weather_error(void *object);
int fs98_world_weather_measure(void *object, uint32_t id, void *measure);
void fs98_world_weather_context_map(uint32_t window);
int fs98_world_weather_context(void *object, uint32_t window, uint32_t unused);
int fs98_world_weather_init(void *object);
struct fs98_world_weather_font_state { uint32_t font[28], flags; };
struct fs98_world_weather_font_state *fs98_world_weather_get_font_state(void);
void fs98_world_weather_font_cleanup(void);
void fs98_world_weather_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_weather_set_module(uint32_t module);
void *fs98_world_weather_construct(void *object, void *parent);
void fs98_world_weather_destroy(void *object);
void *fs98_world_weather_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_weather_create(uint32_t parent, void *descriptor);
void fs98_world_weather_exchange(void *object, void *exchange);
int fs98_world_weather_help(void *object, uint32_t unused, void *help);
void fs98_world_weather_post_destroy(void *object);
#endif
