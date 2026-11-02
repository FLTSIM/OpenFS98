#ifndef FS98_FLIGHTS_INTERNAL_H
#define FS98_FLIGHTS_INTERNAL_H

#include <stdint.h>
#define FS98_FLIGHTS_THISCALL __attribute__((thiscall))
#define FS98_FLIGHTS_STDCALL __attribute__((stdcall))

/* Flights.DLL's dialogs, module 0x80000002. Offsets are the retail PE32 object layouts.
 *
 * The picker descriptor (Adventures 0x1108 from ADVDRV +0x28, Challenges 0x1107 from
 * MAIN command 0x1106): answer +0, an unused word +4 (MAIN stores a callback there,
 * ADVDRV leaves it unset; neither dialog reads it), count +8, selection +0xc, and the
 * list +0x10 of 0xa08-byte records: title +0, description +0x100, path +0x900. */
struct fs98_flights_rect { int32_t left, top, right, bottom; };
struct fs98_flights_services {
    int32_t (*send_message)(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param);
    int32_t (*win_help)(uint32_t window, const char *file, uint32_t command, const uint32_t *table);
    int32_t (*enable_window)(uint32_t window, int32_t enable);
    void (*base_exchange)(void *object, void *exchange);
    void (*ddx_control)(void *exchange, uint32_t id, void *control);
    int32_t (*base_init)(void *object);
    int32_t (*init_art)(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags);
    int32_t (*init_button)(void *control, void *font, void *alternate_font, uint32_t bitmap,
                         void *dib, void *palette, uint32_t flags, uint32_t color,
                         uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect);
    void (*init_text)(void *control, void *font);
    void (*init_list)(void *control, void *font, void *label);
    void (*init_edit)(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags);
    void *(*base_font)(int32_t id);
    void (*set_window_text)(void *window, const char *text);
};

void fs98_flights_set_services(const struct fs98_flights_services *services);
/* DIALOG.DLL, loaded by module init: the skins' and buttons' resource module */
void fs98_flights_set_resource_module(uint32_t module);
/* The context-help file the module passes to WinHelp: "fs98.hlp" in its .data */
void fs98_flights_set_help_file(const char *file);

#define FS98_FLIGHTS_EVENT(name) void FS98_FLIGHTS_THISCALL fs98_flights_##name(void *object)
FS98_FLIGHTS_EVENT(picker_select);          /* 0x22313040, both pickers' LBN_SELCHANGE */
FS98_FLIGHTS_EVENT(picker_cancel);          /* 0x22315240, both pickers' OnCancel */
FS98_FLIGHTS_EVENT(picker_sync);            /* 0x223154c0 */
FS98_FLIGHTS_EVENT(adventures_accept);
FS98_FLIGHTS_EVENT(adventures_populate);
FS98_FLIGHTS_EVENT(adventures_double_click);
FS98_FLIGHTS_EVENT(challenges_accept);
FS98_FLIGHTS_EVENT(challenges_populate);
FS98_FLIGHTS_EVENT(challenges_double_click);
FS98_FLIGHTS_EVENT(post_destroy);
FS98_FLIGHTS_EVENT(disable);
FS98_FLIGHTS_EVENT(enable);
FS98_FLIGHTS_EVENT(empty);
#undef FS98_FLIGHTS_EVENT
void FS98_FLIGHTS_THISCALL fs98_flights_empty_arg(void *object, uint32_t unused);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_picker_no_error(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_picker_validate(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_adventures_init(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_challenges_init(void *object);
#define FS98_FLIGHTS_CLASS(name) \
    int32_t FS98_FLIGHTS_THISCALL fs98_flights_##name##_help(void *object, uint32_t unused, void *info); \
    int32_t FS98_FLIGHTS_THISCALL fs98_flights_##name##_context(void *object, uint32_t window, uint32_t unused); \
    void FS98_FLIGHTS_THISCALL fs98_flights_##name##_exchange(void *object, void *exchange)
FS98_FLIGHTS_CLASS(adventures);
FS98_FLIGHTS_CLASS(challenges);
#undef FS98_FLIGHTS_CLASS
#endif
