#ifndef FS98_ALERTS_INTERNAL_H
#define FS98_ALERTS_INTERNAL_H

#include <stdint.h>

#if defined(__i386__)
#define FS98_ALERTS_THISCALL __attribute__((thiscall))
#else
#define FS98_ALERTS_THISCALL
#endif

struct fs98_alerts_point { int32_t x, y; };
struct fs98_alerts_rect { int32_t left, top, right, bottom; };
struct fs98_alerts_size { int32_t width, height; };

/* Dependency boundary for the event handlers, supplied by alerts_runtime.c
 * or by the synthetic differential harness. Locals
 * passed as MFC objects retain their retail storage sizes and field offsets. */
struct fs98_alerts_services {
    void (*string_construct)(void *string);
    void (*string_destroy)(void *string);
    void (*dc_construct)(void *dc, void *window);
    void (*dc_destroy)(void *dc);
    uint32_t (*stock_object)(int32_t object);
    void *(*font_from_handle)(uint32_t font);
    void *(*select_font)(void *dc, void *font);
    int32_t (*client_rect)(uint32_t window, struct fs98_alerts_rect *rect);
    int32_t (*client_to_screen)(uint32_t window, struct fs98_alerts_point *point);
    int32_t (*screen_to_client)(uint32_t window, struct fs98_alerts_point *point);
    void (*format)(void *string, const char *format, int32_t value);
    int32_t (*text_extent)(uint32_t dc, const char *text, int32_t length,
                        struct fs98_alerts_size *size_out);
    uint32_t (*text_color)(void *dc, uint32_t color);
    uint32_t (*background_color)(void *dc, uint32_t color);
    int32_t (*text_out)(uint32_t dc, int32_t x, int32_t y, uint32_t options,
                     const struct fs98_alerts_rect *rect, const char *text,
                     uint32_t length, const int32_t *spacing);
    int32_t (*win_help)(uint32_t window, const char *file, uint32_t command,
                     uint32_t context);
    void (*base_exchange)(void *object, void *exchange);
    void (*ddx_control)(void *exchange, uint32_t control_id, void *control);
    void (*dib_construct)(void *dib);
    void (*dib_destroy)(void *dib);
    int32_t (*base_init)(void *object);
    int32_t (*load_string)(uint32_t id, char *buffer, uint32_t capacity);
    void (*set_text)(void *control, const char *text);
    void (*init_text)(void *control, void *font);
    int32_t (*load_dib)(void *dib, int32_t id, uint32_t module, void *palette, uint32_t flags);
    int32_t (*sprite_copy)(void *dib, void *destination, int32_t x, int32_t y,
                         int32_t width, int32_t height, int32_t source_x, int32_t source_y,
                         uint32_t raster);
    int32_t (*show_window)(void *control, int32_t show);
    int32_t (*window_load_string)(uint32_t module, uint32_t id, char *buffer, int32_t capacity);
    void (*button_text)(void *control, const char *text);
    int32_t (*button_init)(void *control, void *font, void *alternate_font, uint32_t bitmap,
                         void *dib, void *palette, uint32_t flags, uint32_t color,
                         uint32_t module, int32_t x, int32_t y, struct fs98_alerts_rect rect);
    int32_t (*set_checked)(void *control, int32_t checked);
};

void fs98_alerts_set_services(const struct fs98_alerts_services *services);
void fs98_alerts_set_resource_module(uint32_t module);
void FS98_ALERTS_THISCALL fs98_alerts_modal_first(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_modal_second(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_modal_third(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_accept(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_quit(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_help(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_noop(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_modal_exchange(void *object, void *exchange);
void FS98_ALERTS_THISCALL fs98_alerts_modeless_exchange(void *object, void *exchange);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_exchange(void *object, void *exchange);
int32_t FS98_ALERTS_THISCALL fs98_alerts_modeless_init(void *object);
int32_t FS98_ALERTS_THISCALL fs98_alerts_modal_init(void *object);
int32_t FS98_ALERTS_THISCALL fs98_alerts_advisory_init(void *object);
int32_t FS98_ALERTS_THISCALL fs98_alerts_progress_tick(void *object,
                                                   uint32_t w_param, uint32_t l_param);

#endif
