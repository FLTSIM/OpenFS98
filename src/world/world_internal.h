#ifndef FS98_WORLD_INTERNAL_H
#define FS98_WORLD_INTERNAL_H
#include <stdint.h>
#define FS98_WORLD_THISCALL __attribute__((thiscall))
#define FS98_WORLD_STDCALL __attribute__((stdcall))
#define WORLD_U32(ptr, offset) (*(uint32_t *)((uint8_t *)(ptr) + (offset)))
#define WORLD_PTR(ptr, offset) (*(void **)((uint8_t *)(ptr) + (offset)))
/* Area-dialog core; the native adapter registers only area add/edit. */
struct fs98_world_services {
    int (*get_text)(void *control, char *text, int capacity);
    void (*set_text)(void *control, const char *text);
    int (*parse_int)(const char *text);
    int (*format_int)(char *text, const char *format, int value);
    int (*send)(uint32_t window, uint32_t message, uint32_t first, uint32_t second);
    int (*check)(void *control, int checked);
    int (*enable)(void *control, int enabled);
    int (*error)(void *object, void *control, void *status);
    void (*focus_control)(void *control);
};
void fs98_world_set_services(const struct fs98_world_services *services);
void fs98_world_area_collect(void *object);
void fs98_world_area_populate(void *object);
void fs98_world_area_accept(void *object);
void fs98_world_area_cancel(void *object);
int fs98_world_area_validate(void *object, uint32_t unused);
int fs98_world_area_error(void *object, uint32_t unused);
void fs98_world_area_path_changed(void *object);
#endif
