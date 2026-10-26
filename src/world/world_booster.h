#ifndef FS98_WORLD_BOOSTER_H
#define FS98_WORLD_BOOSTER_H
#include "world_objects.h"
#include "world_init.h"
/* Shared add/edit performance-booster class, 0x13080007/8. */
struct fs98_world_booster_services {
    const struct fs98_world_services *ui;
    void (*title)(void *control, const char *text);
};
void fs98_world_booster_set_services(const struct fs98_world_booster_services *services);
void fs98_world_booster_collect(void *object);
void fs98_world_booster_populate(void *object);
void fs98_world_booster_accept(void *object);
void fs98_world_booster_path_changed(void *object);
int fs98_world_booster_validate(void *object, uint32_t unused);
int fs98_world_booster_error(void *object, uint32_t unused);
void fs98_world_booster_set_object_services(const struct fs98_world_object_services *services, void *vtable);
void fs98_world_booster_set_module(uint32_t module);
void *fs98_world_booster_construct(void *object, void *parent);
void fs98_world_booster_destroy(void *object);
void *fs98_world_booster_delete(void *object, uint32_t flags);
void fs98_world_booster_exchange(void *object, void *exchange);
int fs98_world_booster_help(void *object, uint32_t unused, void *help);
int fs98_world_booster_context(void *object, uint32_t window, uint32_t unused);
uint32_t FS98_WORLD_STDCALL fs98_world_booster_create_add(uint32_t parent, void *descriptor);
uint32_t FS98_WORLD_STDCALL fs98_world_booster_create_edit(uint32_t parent, void *descriptor);
void fs98_world_booster_post_destroy(void *object);
void fs98_world_booster_cancel(void *object);
struct fs98_world_booster_font_state { uint32_t flags, padding, font[28]; };
struct fs98_world_booster_init_services {
    const struct fs98_world_init_services *ui;
    const struct fs98_world_services *events;
    void (*font_construct)(void *font, void *base_font);
    void (*font_destroy)(void *font);
    int (*at_exit)(void (*callback)(void));
    void (*group)(void *control, void *font);
};
void fs98_world_booster_set_init_services(const struct fs98_world_booster_init_services *services);
struct fs98_world_booster_font_state *fs98_world_booster_get_font_state(void);
int fs98_world_booster_init(void *object);
void fs98_world_booster_font_cleanup(void);
#endif
