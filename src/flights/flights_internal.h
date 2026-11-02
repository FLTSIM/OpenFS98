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
    void (*string_construct)(void *string);                         /* CString() */
    void (*string_destroy)(void *string);                           /* ~CString */
    void (*list_text)(void *list, int32_t index, void *string);     /* CListBox::GetText */
    void *(*dialog_item)(void *object, int32_t id);                 /* CWnd::GetDlgItem */
    uint32_t (*dialog_item_window)(uint32_t window, int32_t id);    /* USER32 GetDlgItem */
    void (*remap_rect)(struct fs98_flights_rect *rect, void *control);  /* SIMSUI RemapCtrlRect */
    void (*move_window)(void *window, int32_t x, int32_t y, int32_t width, int32_t height,
                        int32_t repaint);                           /* CWnd::MoveWindow */
    int32_t (*redraw_window)(uint32_t window, void *rect, void *region, uint32_t flags);
    void *(*window_from_handle)(uint32_t window);                   /* CWnd::FromHandle */
    int32_t (*load_string)(void *string, uint32_t id);              /* CString::LoadString */
    int32_t (*init_shadow_button)(void *control, void *font, void *alternate_font, uint32_t bitmap,
                                  void *dib, void *palette, uint32_t flags, uint32_t color,
                                  uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect);
    int32_t (*window_enabled)(void *window);                        /* CWnd::IsWindowEnabled */
    void (*init_combo)(void *control, void *font, void *label);     /* CSimsComboBox::Init */
};

/* Select Flight (0x1102, MAIN's flight_dialog): answer +0, delete +8 (stdcall, given the
 * descriptor), edit +0xc (stdcall, given the descriptor and a copy of the record with a
 * new title, or with no title and a new description), "make it the default" +0x10,
 * selection +0x14, count +0x18, the 0xa08-byte records +0x1c. The dialog also uses +0x20,
 * the entry that may not be edited or deleted, which is past the end of MAIN's block. */
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_select_delete_fn)(void *descriptor);
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_select_edit_fn)(void *descriptor, void *record);

/* Autopilot (0x1204, MAIN command 0x1204, filled by UTIL's exchange): answer +0, check
 * +4 (stdcall, given the descriptor; its answer is ignored), active +0x10, heading mode
 * +0x18 (0 none, 1 LVL, 2 ATT, 3 HDG) with its status +0x1c and text +0x20 (room +0x24),
 * an altitude label in place +0x30, altitude hold +0x44 with status +0x48 and text +0x4c
 * (room +0x50), capabilities +0x5c (2 ASH, 4 MNH, 8 vertical speed, 0x10 AAT, 0x20
 * TO/GA, 1 ...), TO/GA +0x64, AAT status +0x68 and set +0x6c, speed hold +0x74, airspeed
 * status +0x78 text +0x7c room +0x80, Mach status +0x8c text +0x90 room +0x94, Mach +0xa0,
 * airspeed +0xa4, a label in place +0xa8, vertical speed set +0xbc status +0xc0 text +0xc4
 * room +0xc8, yaw damper +0xd8, navigation mode +0xe0 (0..4). */
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_autopilot_check_fn)(void *descriptor);

/* The welcome dialog (0x1109, MAIN's start and command 0x1109): answer +0, callbacks
 * (stdcall, given the descriptor) more choices +8, lessons +0xc, Create a Flight +0x10,
 * multiplayer +0x14; multiplayer available +0x18; quit asked +0x1c; "show at start" +0x20.
 * Its fonts: three function-local static CSimsFonts in .data (0x22322240, 0x223221c8,
 * 0x223222b0) under one guard byte (0x22322238, bits 1, 2, 4), and the hint shown last,
 * 0x22322320, laid out here as in .data from 0x223221c8. */
struct fs98_flights_opening_state {
    uint8_t shadow_alternate[0x70];     /* 0x223221c8, guard bit 2 */
    uint8_t guard, padding[7];          /* 0x22322238 */
    uint8_t shadow[0x70];               /* 0x22322240, guard bit 1 */
    uint8_t hint[0x70];                 /* 0x223222b0, guard bit 4 */
    uint32_t shown;                     /* 0x22322320: the button whose hint is shown */
};
void fs98_flights_set_opening_state(struct fs98_flights_opening_state *state);
void fs98_flights_opening_shadow_destroy(void);
void fs98_flights_opening_alternate_destroy(void);
void fs98_flights_opening_hint_destroy(void);

/* The two sub-menus, flights (0x11090002) and learning (0x11090001), from the welcome
 * dialog's More and Lessons: answer +0, then callbacks (stdcall, given the descriptor).
 * Flights: dual-pilot +8, review a challenge +0x10, watch a video +0x14. Learning:
 * lessons +8, quit +0xc, about +0x10. Their statics run on in .data from 0x22322328. */
struct fs98_flights_menus_state {
    uint8_t flight_alternate[0x70];     /* 0x22322328, flights guard bit 2 */
    uint8_t flight_hint[0x70];          /* 0x22322398, bit 4 */
    uint8_t flight_shadow[0x70];        /* 0x22322408, bit 1 */
    uint8_t flight_guard, padding0[3];  /* 0x22322478 */
    uint32_t flight_shown;              /* 0x2232247c */
    uint8_t learning_guard, padding1[7];/* 0x22322480 */
    uint8_t learning_shadow[0x70];      /* 0x22322488, learning guard bit 1 */
    uint8_t learning_hint[0x70];        /* 0x223224f8, bit 4 */
    uint8_t learning_alternate[0x70];   /* 0x22322568, bit 2 */
    uint32_t learning_shown;            /* 0x223225d8 */
};
void fs98_flights_set_menus_state(struct fs98_flights_menus_state *state);
void fs98_flights_flight_menu_shadow_destroy(void);
void fs98_flights_flight_menu_alternate_destroy(void);
void fs98_flights_flight_menu_hint_destroy(void);
void fs98_flights_learning_menu_shadow_destroy(void);
void fs98_flights_learning_menu_alternate_destroy(void);
void fs98_flights_learning_menu_hint_destroy(void);

/* Select Aircraft (0x1201, MAIN command 0x1201): answer +0, +4 and +0xc unused here,
 * +8 the details callback (stdcall, given the descriptor; zero means it changed the list),
 * +0x10 the preview (stdcall: 1 open given the picture's window, 2 close, 3 draw a frame),
 * selection +0x14, count +0x18, ACONTAIN's 0xa08-byte records +0x1c: title +0, two texts
 * +0x100 and +0x500, and +0xa04, whether the details button is usable. */
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_aircraft_details_fn)(void *descriptor);
typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_aircraft_preview_fn)(uint32_t operation,
                                                                       void *descriptor, uint32_t window);
/* An MFC CString as Flights' code sees it: one pointer to its text */
struct fs98_flights_string { char *text; };

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
FS98_FLIGHTS_EVENT(aircraft_cancel);
FS98_FLIGHTS_EVENT(aircraft_accept);
FS98_FLIGHTS_EVENT(aircraft_populate);
FS98_FLIGHTS_EVENT(aircraft_select);
FS98_FLIGHTS_EVENT(aircraft_double_click);
FS98_FLIGHTS_EVENT(aircraft_details);
FS98_FLIGHTS_EVENT(nothing);                /* 0x22312b50, shared */
FS98_FLIGHTS_EVENT(opening_multiplayer);
FS98_FLIGHTS_EVENT(opening_lessons);
FS98_FLIGHTS_EVENT(opening_more);
FS98_FLIGHTS_EVENT(opening_new_flight);
FS98_FLIGHTS_EVENT(opening_fly);
FS98_FLIGHTS_EVENT(opening_quit);
FS98_FLIGHTS_EVENT(opening_refresh);
FS98_FLIGHTS_EVENT(opening_save);
FS98_FLIGHTS_EVENT(menu_back);                /* 0x2231b920: both sub-menus' OK, Cancel, WM_CLOSE */
FS98_FLIGHTS_EVENT(flight_menu_dual);
FS98_FLIGHTS_EVENT(flight_menu_review);
FS98_FLIGHTS_EVENT(flight_menu_video);
FS98_FLIGHTS_EVENT(learning_menu_lessons);
FS98_FLIGHTS_EVENT(learning_menu_quit);
FS98_FLIGHTS_EVENT(learning_menu_about);
FS98_FLIGHTS_EVENT(select_cancel);
FS98_FLIGHTS_EVENT(select_accept);
FS98_FLIGHTS_EVENT(select_remove);
FS98_FLIGHTS_EVENT(select_choose);
FS98_FLIGHTS_EVENT(select_double_click);
FS98_FLIGHTS_EVENT(select_populate);
FS98_FLIGHTS_EVENT(autopilot_cancel);
FS98_FLIGHTS_EVENT(autopilot_accept);
FS98_FLIGHTS_EVENT(autopilot_active);
FS98_FLIGHTS_EVENT(autopilot_level);
FS98_FLIGHTS_EVENT(autopilot_heading);
FS98_FLIGHTS_EVENT(autopilot_altitude);
FS98_FLIGHTS_EVENT(autopilot_attitude);
FS98_FLIGHTS_EVENT(autopilot_airspeed);
FS98_FLIGHTS_EVENT(autopilot_mach);
FS98_FLIGHTS_EVENT(autopilot_throttle);
FS98_FLIGHTS_EVENT(autopilot_takeoff);
FS98_FLIGHTS_EVENT(autopilot_read);
FS98_FLIGHTS_EVENT(autopilot_populate);
#undef FS98_FLIGHTS_EVENT
int32_t FS98_FLIGHTS_THISCALL fs98_flights_autopilot_init(void *object);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_autopilot_validate(void *object, uint32_t control);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_autopilot_error(void *object, uint32_t code);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_select_init(void *object);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_select_validate(void *object, uint32_t control);
uint32_t FS98_FLIGHTS_THISCALL fs98_flights_select_error(void *object, uint32_t code);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_flight_menu_init(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_learning_menu_init(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_flight_menu_hover(void *object, uint32_t w_param, uint32_t l_param);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_learning_menu_hover(void *object, uint32_t w_param, uint32_t l_param);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_opening_init(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_opening_hover(void *object, uint32_t w_param, uint32_t l_param);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_modeless_init(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_modeless_check(void *object, uint32_t w_param, uint32_t l_param);
void FS98_FLIGHTS_THISCALL fs98_flights_modeless_palette(void *object, void *window);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_aircraft_init(void *object);
int32_t FS98_FLIGHTS_THISCALL fs98_flights_aircraft_frame(void *object, uint32_t w_param, uint32_t l_param);
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
FS98_FLIGHTS_CLASS(aircraft);
FS98_FLIGHTS_CLASS(modeless);
FS98_FLIGHTS_CLASS(opening);
FS98_FLIGHTS_CLASS(flight_menu);
FS98_FLIGHTS_CLASS(learning_menu);
FS98_FLIGHTS_CLASS(select);
FS98_FLIGHTS_CLASS(autopilot);
#undef FS98_FLIGHTS_CLASS
#endif
