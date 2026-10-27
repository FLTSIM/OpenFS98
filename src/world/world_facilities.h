#ifndef FS98_WORLD_FACILITIES_H
#define FS98_WORLD_FACILITIES_H
#include "world_objects.h"
#include "world_init.h"
/* USER32 geometry used to suppress help over the artwork control. */
struct fs98_world_facilities_help_services {
    int (*cursor)(int32_t *point);
    int (*window_rect)(uint32_t window, struct fs98_world_rect *rect);
    int (*contains)(const struct fs98_world_rect *rect, int32_t x, int32_t y);
};
void fs98_world_facilities_set_help_services(const struct fs98_world_facilities_help_services *services);
int fs98_world_facilities_context(void *object, uint32_t window, uint32_t unused);
void fs98_world_facilities_set_object_services(const struct fs98_world_object_services *services, void *vtable,
    void *spin_vtable, void (*base_exchange)(void *, void *));
void fs98_world_facilities_set_module(uint32_t module);
void *fs98_world_facilities_construct(void *object, void *parent);
void fs98_world_facilities_destroy(void *object);
void *fs98_world_facilities_scalar_delete(void *object, uint32_t flags);
uint32_t FS98_WORLD_STDCALL fs98_world_facilities_create(uint32_t parent, void *descriptor);
void fs98_world_facilities_exchange(void *object, void *exchange);
int fs98_world_facilities_help(void *object, uint32_t unused, void *help);
void fs98_world_facilities_post_destroy(void *object);
/* Resolve imported UI/CRT/Win32 services at the retail ABI boundary. */
struct fs98_world_facilities_services {
    void *(*resolve)(uint32_t slot);
    void *(*host)(void);
    uint32_t (*module)(void);
    uint32_t (*resource_module)(void);
    void *output_vtable;
    int (*at_exit)(void (*callback)(void));
};
void fs98_world_facilities_set_services(const struct fs98_world_facilities_services *services);
void fs98_world_facilities_release(void *object);
void fs98_world_facilities_output(void *object, uint32_t resource, uint32_t mode);
void fs98_world_facilities_transition(void *object, uint32_t mode, uint32_t resource, uint32_t back);
int fs98_world_facilities_init(void *object);
void fs98_world_facilities_close(void *object);
void fs98_world_facilities_accept(void *object);
void fs98_world_facilities_home(void *object);
void fs98_world_facilities_back(void *object);
void fs98_world_facilities_refresh(void *object);
int fs98_world_facilities_command(void *object, uint32_t command, uint32_t parameter);
void fs98_world_facilities_mouse(void *object, uint32_t hit, int x, int y, int click);
int fs98_world_facilities_hit(void *object, int x, int y);
int fs98_world_facilities_paint(void *object, uint32_t id);
int fs98_world_facilities_pixel(void *object, uint32_t bitmap, int x, int y);
void fs98_world_facilities_deinit(void);
void *fs98_world_facilities_control_delete(void *object, uint32_t flags, int output);
#endif
