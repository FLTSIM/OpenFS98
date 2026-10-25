#ifndef FS98_VIEWS_INTERNAL_H
#define FS98_VIEWS_INTERNAL_H

#include <stdint.h>

#define FS98_VIEWS_THISCALL __attribute__((thiscall))
#define FS98_VIEWS_STDCALL __attribute__((stdcall))

/* Views' descriptor carries MAIN callbacks, not imports of MAIN itself.
 * Callees 0x207f8670 and 0x207f8800 end in ret 4 and ret 12 respectively. */
typedef int32_t (FS98_VIEWS_STDCALL *fs98_views_validate_fn)(void *descriptor);
typedef int32_t (FS98_VIEWS_STDCALL *fs98_views_preview_fn)(uint32_t operation,
                                                         void *descriptor, uint32_t window);

struct fs98_views_rect { int32_t left, top, right, bottom; };

struct fs98_views_services {
    int32_t (*send_message)(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param);
    int32_t (*win_help)(uint32_t window, const char *file, uint32_t command, const uint32_t *table);
    void (*close)(void *object);
    int32_t (*get_text)(void *control, char *buffer, int32_t capacity);
    int32_t (*error_dialog)(void *object, void *control, void *numeric_descriptor);
    int32_t (*enable_window)(uint32_t window, int32_t enabled);
    void *(*get_control)(void *object, uint32_t id);
    int32_t (*enable_control)(void *control, int32_t enabled);
    int32_t (*show_control)(void *control, int32_t show);
    void (*set_text)(void *control, const char *text);
    int32_t (*set_checked)(void *control, int32_t checked);
    void (*ddx_control)(void *exchange, uint32_t id, void *control);
    void (*string_construct)(void *string);
    void (*string_destroy)(void *string);
    char *(*string_buffer)(void *string, int32_t capacity);
    void (*string_release)(void *string, int32_t length);
    int32_t (*load_string)(uint32_t module, uint32_t id, char *buffer, int32_t capacity);
    int32_t (*string_find_char)(void *string, uint32_t character);
    int32_t (*string_find_text)(void *string, const char *text);
    void *(*string_mid)(void *string, void *result, int32_t start, int32_t count);
    void *(*string_tail)(void *string, void *result, int32_t start);
    void (*string_assign)(void *string, const void *source);
    void (*string_append)(void *string, const void *source);
    void (*string_append_text)(void *string, const char *text);
    void (*get_text_string)(void *control, void *string);
    void (*string_format)(void *string, const char *format, double value);
    void (*string_assign_text)(void *string, const char *text);
    int32_t (*scan_float)(const char *text, float *value);
    int64_t (*float_to_integer)(long double value);
    int32_t (*set_radio)(void *control, int32_t checked);
    int32_t (*compare_text)(const char *left, const char *right);
    int32_t (*base_init)(void *object);
    int32_t (*init_art)(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags);
    int32_t (*init_button)(void *control, void *font, void *alternate_font, uint32_t bitmap,
                           void *dib, void *palette, uint32_t flags, uint32_t color,
                           uint32_t module, int32_t x, int32_t y, struct fs98_views_rect rect);
    void (*init_text)(void *control, void *font);
    void (*init_combo)(void *control, void *font, void *label);
    void (*init_group)(void *control, void *font);
    void (*remap_rect)(struct fs98_views_rect *rect, void *control);
    void (*move_control)(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint);
    uint32_t (*get_dlg_item)(uint32_t window, uint32_t id);
    void *(*base_font)(int32_t id);
    void (*init_edit)(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags);

};

void fs98_views_set_services(const struct fs98_views_services *services);
void fs98_views_set_resource_module(uint32_t module);
int32_t FS98_VIEWS_THISCALL fs98_views_init(void *object);
void FS98_VIEWS_THISCALL fs98_views_direction(void *object);
void FS98_VIEWS_THISCALL fs98_views_transition(void *object);
void FS98_VIEWS_THISCALL fs98_views_altitude_reference(void *object);
void FS98_VIEWS_THISCALL fs98_views_axis(void *object);
void FS98_VIEWS_THISCALL fs98_views_fullscreen(void *object);
void FS98_VIEWS_THISCALL fs98_views_gradual(void *object);
void FS98_VIEWS_THISCALL fs98_views_titles(void *object);
int32_t FS98_VIEWS_THISCALL fs98_views_help(void *object, uint32_t unused, void *info);
int32_t FS98_VIEWS_THISCALL fs98_views_context_help(void *object, uint32_t window, uint32_t unused);
int32_t FS98_VIEWS_THISCALL fs98_views_tick(void *object, uint32_t w_param, uint32_t l_param);
void FS98_VIEWS_THISCALL fs98_views_close(void *object);
void FS98_VIEWS_THISCALL fs98_views_sync(void *object);
int32_t FS98_VIEWS_THISCALL fs98_views_validate(void *object, uint32_t unused);
int32_t FS98_VIEWS_THISCALL fs98_views_report_error(void *object, int32_t display);
void FS98_VIEWS_THISCALL fs98_views_disable(void *object);
void FS98_VIEWS_THISCALL fs98_views_enable(void *object);
void FS98_VIEWS_THISCALL fs98_views_empty_arg(void *object, uint32_t unused);
void FS98_VIEWS_THISCALL fs98_views_cockpit(void *object);
void FS98_VIEWS_THISCALL fs98_views_virtual_cockpit(void *object);
void FS98_VIEWS_THISCALL fs98_views_tower(void *object);
void FS98_VIEWS_THISCALL fs98_views_track(void *object);
void FS98_VIEWS_THISCALL fs98_views_spot(void *object);
void FS98_VIEWS_THISCALL fs98_views_cancel(void *object);
void FS98_VIEWS_THISCALL fs98_views_accept(void *object);
void FS98_VIEWS_THISCALL fs98_views_post_destroy(void *object);
void FS98_VIEWS_THISCALL fs98_views_exchange(void *object, void *exchange);

void FS98_VIEWS_THISCALL fs98_views_map(void *object);
void FS98_VIEWS_THISCALL fs98_views_select(void *object);
void FS98_VIEWS_THISCALL fs98_views_populate(void *object);
void FS98_VIEWS_THISCALL fs98_views_zoom(void *object);
void FS98_VIEWS_THISCALL fs98_views_spin(void *object, uint32_t code, uint32_t position, void *scrollbar);

#endif
