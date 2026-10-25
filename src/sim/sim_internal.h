#ifndef FS98_SIM_INTERNAL_H
#define FS98_SIM_INTERNAL_H
#include <stdint.h>
#define FS98_SIM_THISCALL __attribute__((thiscall))
#define FS98_SIM_STDCALL __attribute__((stdcall))
#define FS98_SIM_CLASS_COUNT 8
struct fs98_sim_control { uint16_t offset, constructor, destructor, constructor_vtable, destructor_vtable; };
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
};
void fs98_sim_set_services(const struct fs98_sim_services *services, void *const *vtables, void *static_vtable, void *control_vtable);
void fs98_sim_set_module(uint32_t module);
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
void fs98_sim_crash_accept(void *object);
void fs98_sim_crash_cancel(void *object);
void fs98_sim_crash_close(void *object);
void fs98_sim_crash_notify(void *object);
void fs98_sim_post_destroy(void *object);
void fs98_sim_disable(void *object);
void fs98_sim_enable(void *object);
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
