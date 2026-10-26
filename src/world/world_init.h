#ifndef FS98_WORLD_INIT_H
#define FS98_WORLD_INIT_H
#include "world_internal.h"
struct fs98_world_rect { int left, top, right, bottom; };
struct fs98_world_init_services {
    int (*base_init)(void *object);
    void (*string_construct)(void *string);
    char *(*string_buffer)(void *string, int minimum);
    void (*string_release)(void *string, int length);
    void (*string_destroy)(void *string);
    int (*load_string)(uint32_t module, uint32_t id, char *buffer, int capacity);
    int (*art)(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags);
    int (*button)(void *control, void *font, void *alternate, uint32_t bitmap, void *dib,
        void *palette, uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect);
    void (*text)(void *control, void *font);
    void (*combo)(void *control, void *font, void *label);
    void *(*base_font)(int font);
    void (*edit)(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error, void *label, uint32_t flags);
    void (*title)(void *control, const char *text);
};
const struct fs98_world_services *fs98_world_get_services(void);
void fs98_world_set_init_services(const struct fs98_world_init_services *services);
void fs98_world_set_resource_module(uint32_t module);
int fs98_world_area_init(void *object);
#endif
