#ifndef FS98_SIM_INTERNAL_H
#define FS98_SIM_INTERNAL_H
#include <stdint.h>
#define FS98_SIM_THISCALL __attribute__((thiscall))
#define FS98_SIM_STDCALL __attribute__((stdcall))
#define FS98_SIM_CLASS_COUNT 8
struct fs98_sim_control { uint16_t offset, constructor, destructor, constructor_vtable, destructor_vtable, unwind_destructor; };
struct fs98_sim_store { uint32_t offset, value; };
struct fs98_sim_class {
    uint32_t size, template, constructor, destructor, delete, vtable;
    uint32_t base_constructor, base_destructor, reset_vtable, control_count;
    uint32_t exchange, help, context;
    const struct fs98_sim_control *controls;
    const struct fs98_sim_store *stores;
};
struct fs98_sim_rect { int32_t left, top, right, bottom; };
struct fs98_sim_services {
    void *(*base_construct)(void *object, uint32_t import, int32_t template, void *parent);
    void (*destroy)(void *object, uint32_t import);
    void (*construct)(void *object, uint32_t import);
    void *(*allocate)(uint32_t bytes);
    void (*ui_delete)(void *object);
    void (*mfc_delete)(void *object);
    void (*dialog_delete)(void *object);
    int32_t (*create)(void *object, uint32_t template, void *parent);
    int32_t (*show)(void *object, int32_t show);
    void (*ddx)(void *exchange, uint32_t id, void *control);
    int32_t (*help)(uint32_t window, const char *file, uint32_t command, const uint32_t *map);
    int32_t (*enable_window)(uint32_t window, int32_t enable);
    int32_t (*enable_control)(void *control, int32_t enable);
    int32_t (*window_long)(uint32_t window, int32_t index);
    int32_t (*check)(void *control, int32_t checked);
    int32_t (*slider_get)(void *control);
    void (*slider_range)(void *control, int32_t min, int32_t max);
    int32_t (*slider_set)(void *control, int32_t position);
    int32_t (*send)(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param);
    void (*text)(void *control, const char *text);
    void (*close)(void *object);
    int32_t (*radio_check)(void *control, int32_t checked);
    void *(*active_page)(void *tabs);
    uint32_t (*focus)(void);
    void *(*from_handle)(uint32_t window);
    int32_t (*is_edit)(void *control);
    int32_t (*control_id)(void *control);
    int32_t (*base_init)(void *object);
    int32_t (*init_button)(void *control, void *font, void *alternate_font, uint32_t bitmap,
                           void *dib, void *palette, uint32_t flags, uint32_t color,
                           uint32_t module, int32_t x, int32_t y, struct fs98_sim_rect rect);
    int32_t (*init_art)(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags);
    void (*init_text)(void *control, void *font);
    void (*notify_construct)(void *notify, uint32_t flags, uint32_t mode);
    void (*notify_delete)(void *notify);
    int32_t (*notify_send)(void *notify, const char *format, const char *title, void *argument);
    void *(*get_control)(void *object, uint32_t id);
    void (*remap_rect)(struct fs98_sim_rect *rect, void *control);
    void (*move_control)(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint);
    void (*paint_construct)(void *dc, void *object);
    void (*paint_destroy)(void *dc);
    int32_t (*load_string)(uint32_t module, uint32_t id, char *buffer, int32_t capacity);
    int32_t (*invalidate)(uint32_t window, const void *rect, int32_t erase);
    int32_t (*set_rect)(struct fs98_sim_rect *rect, int32_t left, int32_t top, int32_t right, int32_t bottom);
    void (*string_construct_text)(void *string, const char *text);
    void (*draw_text)(void *font, void *dc, uint32_t string, struct fs98_sim_rect rect, int32_t flags);
    void (*init_group)(void *control, void *font);
    void (*init_combo)(void *control, void *font, void *label);
    void (*control_text)(void *control, const char *text);
    int32_t (*init_slider)(void *control, uint32_t first_bitmap, uint32_t second_bitmap,
        void *dib, void *palette, uint32_t first, uint32_t second, uint32_t third, uint32_t fourth,
        int32_t x, int32_t y, uint32_t module);
};
void fs98_sim_set_services(const struct fs98_sim_services *services, void *const *vtables, void *static_vtable, void *control_vtable);
void fs98_sim_set_module(uint32_t module);
void fs98_sim_set_resource_module(uint32_t module);
extern uint32_t g_sim_resource_module;
void *fs98_sim_construct(void *object, void *parent, uint32_t kind);
void fs98_sim_destroy(void *object, uint32_t kind);
void *fs98_sim_delete(void *object, uint32_t flags, uint32_t kind);
uint32_t FS98_SIM_STDCALL fs98_sim_create_crash(uint32_t parent, void *descriptor);
uint32_t FS98_SIM_STDCALL fs98_sim_create_settings_0(uint32_t parent, void *descriptor);
uint32_t FS98_SIM_STDCALL fs98_sim_create_settings_2(uint32_t parent, void *descriptor);
uint32_t FS98_SIM_STDCALL fs98_sim_create_settings_3(uint32_t parent, void *descriptor);
uint32_t FS98_SIM_STDCALL fs98_sim_create_settings_5(uint32_t parent, void *descriptor);
void fs98_sim_exchange(void *object, void *exchange, uint32_t kind);
int32_t fs98_sim_help(void *object, uint32_t unused, void *info, uint32_t kind);
int32_t fs98_sim_context(void *object, uint32_t window, uint32_t unused, uint32_t kind);
int32_t fs98_sim_crash_tick(void *object, uint32_t w_param, uint32_t l_param);
void *fs98_sim_make_notify(void);
void fs98_sim_init_page_button(void *object, uint32_t offset, uint32_t font, uint32_t bitmap);
void fs98_sim_engines_init(void *object);
void fs98_sim_engines_populate(void *object);
void fs98_sim_set_crash_tables(void *notify_vtable);
void *fs98_sim_notify_delete(void *object, uint32_t flags);
void *fs98_sim_static_delete(void *object, uint32_t flags);
void fs98_sim_class_delete(void *object);
int32_t fs98_sim_crash_validate(void *object);
int32_t fs98_sim_crash_init(void *object);
void fs98_sim_crash_paint(void *object);
void fs98_sim_crash_accept(void *object);
void fs98_sim_crash_cancel(void *object);
void fs98_sim_crash_close(void *object);
void fs98_sim_crash_notify(void *object);
void fs98_sim_post_destroy(void *object);
void fs98_sim_disable(void *object);
void fs98_sim_enable(void *object);
int32_t fs98_sim_damage_commit(void *object, uint32_t unused);
void fs98_sim_damage_init(void *object);
void fs98_sim_reliability_init(void *object);
void fs98_sim_damage_populate(void *object);
void fs98_sim_settings_accept(void *object);
void fs98_sim_damage_radio(void *object, uint32_t mode);
uint32_t fs98_sim_engine_count(void *object);
void fs98_sim_engines_match(void *object, int32_t count);
void fs98_sim_engine_toggle(void *object, uint32_t index);
int32_t fs98_sim_engines_commit(void *object, uint32_t unused);
int32_t fs98_sim_reliability_commit(void *object, uint32_t unused);
void fs98_sim_reliability_populate(void *object);
int32_t fs98_sim_system_selection(void *object, int32_t selection);
void fs98_sim_settings_cancel(void *object);
void fs98_sim_settings_close(void *object);
int32_t fs98_sim_zero(void *object, uint32_t unused);
int32_t fs98_sim_zero_message(void *object, uint32_t first, uint32_t second);
void fs98_sim_empty(void *object);
void fs98_sim_empty_arg(void *object, uint32_t unused);
extern const struct fs98_sim_services *g_sim_services;
#endif
