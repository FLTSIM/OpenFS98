#ifndef FS98_OPTIONS_INTERNAL_H
#define FS98_OPTIONS_INTERNAL_H

#include <stdint.h>
#define FS98_OPTIONS_THISCALL __attribute__((thiscall))
#define FS98_OPTIONS_STDCALL __attribute__((stdcall))

/* Options.DLL's dialogs, module 0x80000007, preferred base 0x22390000. Offsets are the
 * retail PE32 object layouts. Every MFC, SIMSUI and Win32 call goes through this table,
 * in retail's order, so a harness can run the original routines and ours over the same
 * logged stubs (tools/difftest/optionseventsdiff.c), as Flights' does.
 *
 * Video message (0x14040002, DEMO's dialog_ask while recording): answer +0, check +4
 * (stdcall, given the descriptor), the message +8 (0x38 bytes). Video finished
 * (0x14040003, DEMO's dialog_stopped) is given no descriptor.
 *
 * Save video (0x14040001, DEMO when a recording stops, D+0xed0): answer +0, check +4, the
 * choice words +8 (save) +0xc (go on) +0x10 (review) +0x14 (discard), the name's status
 * +0x18 (0x1004: already there), the name +0x1c (room +0x20), the description's status
 * +0x2c, the description +0x30 (room +0x34).
 *
 * Flight video (0x1404, DEMO's video_dialog): answer +0, check +4, delete +8 (stdcall,
 * given the descriptor), edit +0xc (stdcall, given the descriptor and a copy of the
 * record with a new name, or with no name and a new title), play +0x10, record +0x14,
 * "can record" +0x18, +0x1c unused, the rate +0x20, loop +0x24, the pick +0x28, the count
 * +0x2c and the 0xa08-byte records +0x30 (+0 the name shown, +0x100 the title, +0x500
 * the description, +0x900 the path, +0xa04 writable).
 *
 * The logbook dialogs are given MAIN's one block (main_logbook.inc). Add logbook
 * (0x14070001) uses it from +0x28: answer +0, check +4, the name's status +8 (0x1004:
 * already there), the name +0xc (room +0x10), and +0x1c, which passes a name already
 * there when it is zero. Logbook entry (0x14070002) uses it from +0x54: answer +0, check
 * +4, then five fields, each a status, a text and its room: the date +8/+0xc/+0x10, the
 * remarks +0x30/+0x34/+0x38, and the three times +0x44 (day), +0x58 (night), +0x6c
 * (instrument), each +4 and +8 on; the aircraft's name +0x20, the aircraft (0xa08-byte
 * records) +0x98, their count +0x9c and the pick +0xa0, and +0xa4: the totals row, whose
 * remarks alone can be edited. */
struct fs98_options_rect { int32_t left, top, right, bottom; };
struct fs98_options_services {
    int32_t (*send_message)(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param);
    int32_t (*win_help)(uint32_t window, const char *file, uint32_t command, const uint32_t *table);
    int32_t (*enable_window)(uint32_t window, int32_t enable);
    void (*ddx_control)(void *exchange, uint32_t id, void *control);
    int32_t (*base_init)(void *object);                             /* CSimsDialog::OnInitDialog */
    int32_t (*init_button)(void *control, void *font, void *alternate_font, uint32_t bitmap,
                           void *dib, void *palette, uint32_t flags, uint32_t color,
                           uint32_t module, int32_t x, int32_t y, struct fs98_options_rect rect);
    void (*init_text)(void *control, void *font);                   /* CSimsText::Init */
    void (*init_edit)(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags);   /* CSimsEdit::Init */
    void *(*base_font)(int32_t id);                                 /* GetSimsBaseFont */
    void (*set_window_text)(void *window, const char *text);        /* CWnd::SetWindowTextA */
    int32_t (*get_window_text)(void *window, char *buffer, int32_t count);  /* CWnd::GetWindowTextA */
    void *(*set_focus)(void *window);                               /* CWnd::SetFocus */
    int32_t (*is_lead_byte)(int32_t character);                     /* MSVCRT isleadbyte */
    void (*base_exchange)(void *object, void *exchange);            /* CSimsDialog::DoDataExchange */
    int32_t (*init_art)(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags);
    void (*init_list)(void *control, void *font, void *label);      /* CSimsListBox::Init */
    void (*init_combo)(void *control, void *font, void *label);     /* CSimsComboBox::Init */
    int32_t (*error_dialog)(void *object, void *control, uint32_t code);   /* DoErrorDialog */
    uint32_t (*get_focus)(void);                                    /* USER32 GetFocus */
    void *(*window_from_handle)(uint32_t window);                   /* CWnd::FromHandle */
    int32_t (*compare_text)(const char *a, const char *b);          /* lstrcmpA */
    char *(*copy_text)(char *out, const char *text);                /* lstrcpyA */
    int32_t (*post_message)(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param);
    int32_t (*enable_control)(void *window, int32_t enable);        /* CWnd::EnableWindow */
    int32_t (*window_enabled)(void *window);                        /* CWnd::IsWindowEnabled */
    int32_t (*set_checked)(void *control, int32_t checked);         /* CSimsCheckButton::SetChecked */
    char *(*host_strcpy)(char *out, const char *text);              /* HOST +0x2a8 */
    int32_t (*host_strcmpi)(const char *a, const char *b);          /* HOST +0x2c0 */
    void *(*allocate)(uint32_t size);                               /* ui_new */
    void (*notify_construct)(void *notify, uint32_t a, uint32_t b); /* CNotify(a, b) */
    void (*notify_free)(void *notify);                              /* CNotify::operator delete */
    void (*notify_destroy)(void *notify);                           /* ~CNotify */
    void (*ui_delete)(void *object);
    void (*close)(void *object);                                    /* CSimsDialog::Close */
    void (*string_construct)(void *string);                         /* CString() */
    void (*string_destroy)(void *string);                           /* ~CString */
    void (*string_assign)(void *string, const char *text);          /* CString::operator= */
    void (*ddx_text)(void *exchange, uint32_t id, void *string);    /* DDX_Text into a CString */
    void (*ddv_max_chars)(void *exchange, void *string, int32_t count);
    int32_t (*show_window)(void *window, int32_t show);             /* CWnd::ShowWindow */
    int32_t (*load_string_a)(uint32_t instance, uint32_t id, char *buffer, int32_t size);   /* USER32 */
    void (*init_group)(void *control, void *font);                  /* CSimsGroupbox::Init */
    void (*set_sims_text)(void *control, const char *text);         /* CSimsText::SetWindowTextA */
    void (*get_sims_text)(void *control, void *string);             /* CSimsText::GetWindowTextA */
    void (*window_text_string)(void *window, void *string);         /* CWnd::GetWindowText(CString &) */
};
/* a CString: its text */
struct fs98_options_string { char *text; };
/* OnInitDialog's CNotify (options_videos.c): ui_new, CNotify(0x30, 1), Options' vtable */
void *fs98_options_new_notify(void);
/* Options' CNotify, which Save video and Flight video make in OnInitDialog: SIMSUI's,
 * with its own vtable (0x223a6ab0) whose deleting destructor is ours */
void fs98_options_set_notify_vtable(void *vtable);
void *FS98_OPTIONS_THISCALL fs98_options_notify_delete(void *notify, uint32_t flags);
/* The name Flight video last saw kept, at .data 0x223ac450 */
struct fs98_options_video_state { char last[0x108]; };
void fs98_options_set_video_state(struct fs98_options_video_state *state);
typedef uint32_t (FS98_OPTIONS_STDCALL *fs98_options_delete_fn)(void *descriptor);
typedef uint32_t (FS98_OPTIONS_STDCALL *fs98_options_edit_fn)(void *descriptor, void *record);
void fs98_options_set_services(const struct fs98_options_services *services);
const struct fs98_options_services *fs98_options_services(void);
/* the resource instance the skin and buttons load from (.data 0x223ac344), and Options'
 * own instance, which each factory stores at object +0x80 (0x223ac35c) */
uint32_t fs98_options_resource_module(void);
void fs98_options_set_resource_module(uint32_t module);
void fs98_options_set_help_file(const char *file);
extern const char fs98_options_empty_text[];

typedef uint32_t (FS98_OPTIONS_STDCALL *fs98_options_check_fn)(void *descriptor);

#define FS98_OPTIONS_HANDLERS(name) \
    int32_t FS98_OPTIONS_THISCALL fs98_options_##name##_help(void *object, uint32_t unused, void *info); \
    int32_t FS98_OPTIONS_THISCALL fs98_options_##name##_context(void *object, uint32_t window, uint32_t unused); \
    void FS98_OPTIONS_THISCALL fs98_options_##name##_exchange(void *object, void *exchange)
FS98_OPTIONS_HANDLERS(video_finished);
FS98_OPTIONS_HANDLERS(video_message);
FS98_OPTIONS_HANDLERS(save_video);
FS98_OPTIONS_HANDLERS(flight_video);
FS98_OPTIONS_HANDLERS(add_logbook);
FS98_OPTIONS_HANDLERS(logbook_entry);
#undef FS98_OPTIONS_HANDLERS

/* shared by every class (0x22392400, 0x22392410, 0x22392420, 0x223943d0, 0x22397c30,
 * 0x22392f20, 0x223943c0) */
void FS98_OPTIONS_THISCALL fs98_options_empty(void *object);
void FS98_OPTIONS_THISCALL fs98_options_disable(void *object);
void FS98_OPTIONS_THISCALL fs98_options_enable(void *object);
void FS98_OPTIONS_THISCALL fs98_options_empty_arg(void *object, uint32_t unused);
void FS98_OPTIONS_THISCALL fs98_options_post_destroy(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_no_error(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_check(void *object);

/* Video finished and Video message (options_video.c) */
int32_t FS98_OPTIONS_THISCALL fs98_options_video_finished_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_video_finished_accept(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_video_message_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_video_message_accept(void *object);
void FS98_OPTIONS_THISCALL fs98_options_video_message_cancel(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_video_message_populate(void *object);

/* Save video and Flight video (options_videos.c) */
void FS98_OPTIONS_THISCALL fs98_options_notify_close(void *object);
void FS98_OPTIONS_THISCALL fs98_options_save_video_review(void *object);
void FS98_OPTIONS_THISCALL fs98_options_save_video_resume(void *object);
void FS98_OPTIONS_THISCALL fs98_options_save_video_save(void *object);
void FS98_OPTIONS_THISCALL fs98_options_save_video_cancel(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_save_video_init(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_save_video_validate(void *object, uint32_t unused);
uint32_t FS98_OPTIONS_THISCALL fs98_options_save_video_error(void *object, uint32_t code);
void FS98_OPTIONS_THISCALL fs98_options_save_video_fill(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_save_video_has_descriptor(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_close(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_accept(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_flight_video_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_delete_entry(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_play(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_record(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_select(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_double_click(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_flight_video_validate(void *object, uint32_t control);
uint32_t FS98_OPTIONS_THISCALL fs98_options_flight_video_error(void *object, uint32_t code);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_populate(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_flight_video_has_descriptor(void *object);

/* Add logbook and Logbook entry (options_logbook.c) */
void FS98_OPTIONS_THISCALL fs98_options_add_logbook_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_add_logbook_accept(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_add_logbook_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_add_logbook_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_add_logbook_clear(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_add_logbook_error(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_add_logbook_has_descriptor(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_add_logbook_check(void *object);
void FS98_OPTIONS_THISCALL fs98_options_logbook_entry_close(void *object);
void FS98_OPTIONS_THISCALL fs98_options_logbook_entry_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_logbook_entry_accept(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_logbook_entry_init(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_logbook_entry_validate(void *object, uint32_t unused);
uint32_t FS98_OPTIONS_THISCALL fs98_options_logbook_entry_error(void *object, uint32_t code);
void FS98_OPTIONS_THISCALL fs98_options_logbook_entry_fill(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_logbook_entry_has_descriptor(void *object);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_rate(void *object);
#endif
