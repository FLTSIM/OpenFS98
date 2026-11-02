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
    int32_t (*get_window_text)(void *window, char *buffer, int32_t count);
    int32_t (*set_checked)(void *control, int32_t checked);
    int32_t (*error_dialog)(void *object, void *control, uint32_t code);
    int32_t (*post_message)(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param);
    char *(*copy_text)(char *out, const char *text);
    int32_t (*compare_text)(const char *a, const char *b);
    void (*close)(void *object);
    int32_t (*get_window_long)(uint32_t window, int32_t index);
    uint32_t (*get_focus)(void);
    int32_t (*enable_control)(void *window, int32_t enable);       /* CWnd::EnableWindow */
    int32_t (*set_radio)(void *control, int32_t checked);
    int32_t (*init_slider)(void *control, uint32_t bitmap, uint32_t thumb, void *dib, void *palette,
                           uint32_t a, uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f,
                           uint32_t module);
    void (*slider_range)(void *control, int32_t low, int32_t high);
    int32_t (*slider_position)(void *control);
    int32_t (*set_slider_position)(void *control, int32_t position);
    void (*init_group)(void *control, void *font);
    double (*to_number)(const char *text);                           /* MSVCRT atof */
    int32_t (*format_number)(char *out, const char *format, double value);   /* sprintf */
    int32_t (*init_picture)(void *control, uint32_t bitmap, void *dib, void *palette,
                            uint32_t module, uint16_t color);
    void *(*set_focus)(void *window);                               /* CWnd::SetFocus */
    void (*set_sims_text)(void *control, const char *text);         /* CSimsText::SetWindowTextA */
    void (*font_construct)(void *font, void *base_font);
    void (*font_destroy)(void *font);
    void (*register_exit)(void (*handler)(void));                   /* atexit, through __dllonexit */
    void (*default_paint)(void *object);                            /* MFC's WM_PAINT handler */
};

/* Create a Flight (0x1101, MAIN command 0x1101): answer +0, callbacks (stdcall, given the
 * descriptor) Save +8, Aircraft +0xc, Location +0x10, Weather +0x14, Time +0x18; the four
 * summaries +0x1c..+0x28; +0x2c set once there is something to fly, enabling OK and Save. */
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_create_fn)(void *descriptor);
/* Its labels' font: a function-local static CSimsFont built on the first OnInitDialog
 * (.data 0x22322150, guard bit in 0x223221c0) and destroyed at module exit. */
struct fs98_flights_create_state { uint8_t font[0x70]; uint8_t guard; };
void fs98_flights_set_create_state(struct fs98_flights_create_state *state);
void fs98_flights_create_font_destroy(void);

/* Communications (0x1203, MAIN command 0x1203): answer +0, check callback +4 (stdcall,
 * given the descriptor; its answer is ignored), status +0xc, frequency text +0x10 with
 * room +0x14, "slow" box +0x2c, request +0x34 (0 none, 1 take off, 2 land), rate +0x3c. */
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_comms_check_fn)(void *descriptor);
/* "%.3f" in Flights' .data: the frequency's format */
void fs98_flights_set_number_format(const char *format);

/* Save Flight (0x1104, MAIN's Linkage +0xac): answer +0, check callback +4 (stdcall,
 * given the descriptor; MAIN's save_dialog_check), name status +8, name +0xc with room
 * +0x10, description status +0x1c, description +0x20 with room +0x24, "make it the
 * default" +0x30. */
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_save_check_fn)(void *descriptor);

void fs98_flights_set_services(const struct fs98_flights_services *services);
const struct fs98_flights_services *fs98_flights_services(void);
uint32_t fs98_flights_resource_module(void);
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
FS98_FLIGHTS_EVENT(save_populate);
FS98_FLIGHTS_EVENT(save_accept);
FS98_FLIGHTS_EVENT(save_cancel);
FS98_FLIGHTS_EVENT(save_close);
FS98_FLIGHTS_EVENT(save_read);
FS98_FLIGHTS_EVENT(comms_none);
FS98_FLIGHTS_EVENT(comms_take_off);
FS98_FLIGHTS_EVENT(comms_land);
FS98_FLIGHTS_EVENT(comms_cancel);
FS98_FLIGHTS_EVENT(comms_accept);
FS98_FLIGHTS_EVENT(comms_read);
FS98_FLIGHTS_EVENT(comms_populate);
FS98_FLIGHTS_EVENT(create_save);
FS98_FLIGHTS_EVENT(create_aircraft);
FS98_FLIGHTS_EVENT(create_location);
FS98_FLIGHTS_EVENT(create_weather);
FS98_FLIGHTS_EVENT(create_time);
FS98_FLIGHTS_EVENT(create_cancel);
FS98_FLIGHTS_EVENT(create_accept);
FS98_FLIGHTS_EVENT(create_refresh);
FS98_FLIGHTS_EVENT(create_paint);
#undef FS98_FLIGHTS_EVENT
int32_t FS98_FLIGHTS_THISCALL fs98_flights_create_init(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_comms_init(void *object);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_comms_validate(void *object, uint32_t control);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_comms_error(void *object, uint32_t code);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_save_init(void *object);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_save_validate(void *object, uint32_t unused);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_save_error(void *object, uint32_t code);
/* Save's constructor clears the name it remembers; the empty string is Flights' own */
extern const char fs98_flights_empty_text[];
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
FS98_FLIGHTS_CLASS(save);
FS98_FLIGHTS_CLASS(comms);
FS98_FLIGHTS_CLASS(create);
#undef FS98_FLIGHTS_CLASS
#endif
