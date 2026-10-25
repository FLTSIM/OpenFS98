#ifndef FS98_HELP_INTERNAL_H
#define FS98_HELP_INTERNAL_H

#include <stdint.h>
#define FS98_HELP_THISCALL __attribute__((thiscall))
#define FS98_HELP_STDCALL __attribute__((stdcall))

/* Descriptor callback used by About: MAIN 0x207e10f0 ends in ret 4.
 * The unused graph descriptor's callback consumes twelve bytes at its calls;
 * no caller of that dialog id exists in the reference module set. */
typedef int32_t (FS98_HELP_STDCALL *fs98_help_about_fn)(void *descriptor);
typedef int32_t (FS98_HELP_STDCALL *fs98_help_graph_fn)(uint32_t operation,
                                                     void *descriptor, uint32_t window);
struct fs98_help_rect { int32_t left, top, right, bottom; };
struct fs98_help_about_state {
    uint8_t initialized, reserved[7], font[0x70];
};
struct fs98_help_services {
    int32_t (*send_message)(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param);
    int32_t (*win_help)(uint32_t window, const char *file, uint32_t command, const uint32_t *table);
    int32_t (*enable_window)(uint32_t window, int32_t enable);
    int32_t (*kill_timer)(uint32_t window, uint32_t timer);
    uint32_t (*set_timer)(uint32_t window, uint32_t timer, uint32_t period, void *callback);
    int32_t (*destroy_window)(uint32_t window);
    int32_t (*free_library)(uint32_t module);
    void (*close)(void *object);
    int32_t (*message_box)(const char *text, uint32_t type, uint32_t help);
    int32_t (*get_window_long)(uint32_t window, int32_t index);
    int32_t (*set_checked)(void *control, int32_t checked);
    void (*set_text)(void *control, const char *text);
    int32_t (*enable_control)(void *control, int32_t enabled);
    int32_t (*load_host_string)(uint32_t id, char *buffer, int32_t capacity);
    int32_t (*split_path)(const char *path, char *drive, char *dir, char *name, int32_t *ext);
    int32_t (*join_path)(char *out, const char *drive, const char *dir, const char *name);
    int32_t (*exists)(const char *path, int32_t extension);
    void (*base_exchange)(void *object, void *exchange);
    void (*ddx_control)(void *exchange, uint32_t id, void *control);
    int32_t (*read_file)(uint32_t file, void *buffer, uint32_t count, uint32_t *read, void *overlapped);
    char *(*copy_text)(char *out, const char *text);
    int32_t (*open_file)(const char *name, void *info, uint32_t mode);
    int32_t (*close_handle)(uint32_t handle);
    int32_t (*base_init)(void *object);
    int32_t (*dialog_init)(void *object);
    int32_t (*init_art)(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags);
    int32_t (*init_button)(void *control, void *font, void *alternate_font, uint32_t bitmap,
                         void *dib, void *palette, uint32_t flags, uint32_t color,
                         uint32_t module, int32_t x, int32_t y, struct fs98_help_rect rect);
    void (*init_text)(void *control, void *font);
    void *(*get_control)(void *object, uint32_t id);
    void (*remap_rect)(struct fs98_help_rect *rect, void *control);
    void (*init_group)(void *control, void *font);
    void *(*base_font)(int32_t id);
    void (*init_list)(void *control, void *font, void *label);
    void (*init_edit)(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags);
    int32_t (*copy_rect)(struct fs98_help_rect *out, const struct fs98_help_rect *in);
    void (*common_controls)(void);
    uint32_t (*load_library)(const char *name);
    void *(*module_state)(void);
    uint32_t (*create_window)(uint32_t ex_style, const char *class_info, const char *title, uint32_t style,
                            int32_t x, int32_t y, int32_t width, int32_t height, uint32_t parent,
                            uint32_t menu, uint32_t instance, void *param);
    void (*string_construct)(void *string);
    void (*string_destroy)(void *string);
    int32_t (*reg_open)(uint32_t key, const char *subkey, uint32_t options, uint32_t access, uint32_t *key_out);
    int32_t (*reg_query)(uint32_t key, const char *name, void *reserved, uint32_t *type, void *data, uint32_t *size);
    int32_t (*load_string)(uint32_t module, uint32_t id, char *buffer, int32_t capacity);
    void (*set_sims_text)(void *control, const char *text);
    int32_t (*format_version)(char *out, const char *format, int32_t major, int32_t minor,
                            const char *prefix, int32_t build);
    char *(*append_text)(char *out, const char *text);
    void (*font_construct)(void *font, void *base_font);
    void (*font_destroy)(void *font);
    void (*register_exit)(void (*handler)(void));
    void (*move_control)(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint);
};

void fs98_help_set_about_state(struct fs98_help_about_state *state);
void fs98_help_about_font_destroy(void);
int32_t FS98_HELP_THISCALL fs98_help_about_init(void *object);
void fs98_help_set_resource_module(uint32_t module);
int32_t FS98_HELP_THISCALL fs98_help_lessons_init(void *object);
int32_t FS98_HELP_THISCALL fs98_help_credits_init(void *object);
int32_t FS98_HELP_THISCALL fs98_help_graph_init(void *object);
int32_t FS98_HELP_THISCALL fs98_help_completion_init(void *object);
void fs98_help_set_services(const struct fs98_help_services *services);
#define FS98_HELP_EVENT(name) void FS98_HELP_THISCALL fs98_help_##name(void *object)
FS98_HELP_EVENT(about_populate);
FS98_HELP_EVENT(about_cancel);
FS98_HELP_EVENT(about_accept);
FS98_HELP_EVENT(about_credits);
FS98_HELP_EVENT(credits_cancel);
FS98_HELP_EVENT(credits_accept);
FS98_HELP_EVENT(credits_close);
FS98_HELP_EVENT(credits_toggle);
FS98_HELP_EVENT(credits_load);
FS98_HELP_EVENT(lessons_cancel);
FS98_HELP_EVENT(lessons_accept);
FS98_HELP_EVENT(lessons_sync);
FS98_HELP_EVENT(lessons_populate);
FS98_HELP_EVENT(lessons_select);
FS98_HELP_EVENT(lessons_double_click);
FS98_HELP_EVENT(graph_accept);
FS98_HELP_EVENT(graph_notify);
FS98_HELP_EVENT(completion_accept);
FS98_HELP_EVENT(completion_cancel);
FS98_HELP_EVENT(post_destroy);
FS98_HELP_EVENT(disable);
FS98_HELP_EVENT(enable);
FS98_HELP_EVENT(empty);
#undef FS98_HELP_EVENT
int32_t FS98_HELP_THISCALL fs98_help_credits_start_timer(void *object);
void FS98_HELP_THISCALL fs98_help_credits_timer(void *object, uint32_t timer);
void FS98_HELP_THISCALL fs98_help_empty_arg(void *object, uint32_t unused);
int32_t FS98_HELP_THISCALL fs98_help_graph_tick(void *object, uint32_t w_param, uint32_t l_param);
int32_t FS98_HELP_THISCALL fs98_help_graph_validate(void *object);
int32_t FS98_HELP_THISCALL fs98_help_lessons_validate(void *object);
int32_t FS98_HELP_THISCALL fs98_help_lessons_no_error(void *object);
uint32_t FS98_HELP_STDCALL fs98_help_credits_read(uint32_t cookie, void *buffer,
                                               int32_t count, uint32_t *read);
#define FS98_HELP_CLASS(name) \
    int32_t FS98_HELP_THISCALL fs98_help_##name##_help(void *object, uint32_t unused, void *info); \
    int32_t FS98_HELP_THISCALL fs98_help_##name##_context(void *object, uint32_t window, uint32_t unused); \
    void FS98_HELP_THISCALL fs98_help_##name##_exchange(void *object, void *exchange)
FS98_HELP_CLASS(about);
FS98_HELP_CLASS(credits);
FS98_HELP_CLASS(lessons);
FS98_HELP_CLASS(graph);
FS98_HELP_CLASS(completion);
#undef FS98_HELP_CLASS
#endif
