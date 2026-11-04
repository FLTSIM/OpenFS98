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
 * remarks alone can be edited.
 *
 * The Logbook (0x1407, MAIN command 0x1407) is given the block itself: answer +0, check +4,
 * the name's status +8 and a name +0xc (room +0x10), the logbooks' count +0x1c, the one
 * chosen +0x20, the logbooks +0x24 (0x124 each: +0 the title, +0x1f the name, +0x120
 * deletable), the rows' count +0x48, the one chosen +0x4c, the rows +0x50 (0x4f each: the
 * date +0, the aircraft +9, the remarks +0x1a, the times +0x33, +0x3a, +0x41, +0x48), the
 * entry's fields from +0x54 (above) with what a row opens with in +0x60, +0x74, +0x88,
 * +0x9c, +0xb0, +0xc4, +0xd8, the mode +0xe8 (4 a new row, 5 an edit), the four totals
 * +0x100, +0x114, +0x128, +0x13c, and +0x150 MAIN's lb_button (stdcall: an operation, an
 * index, the block): 1 a new logbook, 2 delete one, 3 choose one, 4 a new row, 5 edit
 * one, 6 delete one, 7 choose one, 8 rename the logbook.
 *
 * The two graphs are given ANALYSIS's blocks (analysis.c): answer +0, check +4, the
 * graph's control +8 (stdcall: an operation, the block, the graph's window; 0 set up, 1
 * draw, 2 done). The landing graph's (0x14050002, S_LBLOCK): +0xc set when there is a
 * landing to draw, its four texts +0x10 (the touchdown label), +0x14 and +0x18 (the
 * scale's two ends, as numbers), +0x1c (the caption). The manoeuvre graph's (0x14050003,
 * S_MBLOCK): +0xc which caption (string 0xf751, or 0xf750), +0x10 set when there is a
 * scale, +0x14 its text.
 *
 * Flight analysis (0x1405, ANALYSIS's options, S_OBLOCK, filled by UTIL's exchange):
 * answer +0, check +4, then each choice with the status its failure is reported by: on
 * +0xc (status +8), the mode +0x14 (0, 1 or 2; -1 none; status +0x10, reported against no
 * control), the first box +0x1c (+0x18), the third mode as a flag +0x24, the second box
 * +0x2c (+0x28), the two combos' choices +0x34 (+0x30) and +0x3c (+0x38).
 *
 * Instant replay (0x1403, DEMO's replay_dialog, D+0xe80): answer +0, check +4, +8 set
 * when a replay is already running (then nothing can be changed), the length's maximum
 * +0xc, its status +0x10, the length +0x14 and an error block from +0x18 (the status, the
 * text and the maximum), the speed's status +0x2c, the speed +0x30 and its error block
 * from +0x34, and the loop flag +0x4c.
 *
 * Select demo (0x14090002, MAIN's pf_aircraft, main_prefdlg.inc): answer +0, +4..+0xc
 * unused here (MAIN sets them to do nothing), the pick +0x10, the count +0x14 and the
 * 0xa08-byte records +0x18 (the name shown +0, the path +0x900).
 *
 * Display options (0x14090001, MAIN's pf_display, its block PF_DISP filled by UTIL's
 * exchange): answer +0, check +4 (stdcall, given the block), the tab shown first +8, and each
 * page's part: Aircraft from +0xc (its first word, +0xc, also given to AddPage), Map from
 * +0x38, Scenery from +0x4c, Hardware Acceleration from +0xa0 (MAIN's driver block; AddPage
 * is given +0xac). The parts' words are listed with each page in options_display.c.
 *
 * Preferences (0x1409, MAIN's PF_DLG, main_prefdlg.inc, filled by UTIL's exchange): answer +0,
 * check +4, the tab shown first +8, MAIN's defaults +0xc (stdcall, given the block), and the
 * pages' parts: General from +0x10, Instrument +0x6c, International +0x98, Display +0xc0
 * (MAIN's presets), Sound +0xf8 and Flight Options +0x124 (options_prefs.c). */
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
    void (*font_construct)(void *font, void *base_font);            /* CSimsFont(CFont *) */
    void (*font_destroy)(void *font);                               /* ~CSimsFont */
    void (*register_exit)(void (*handler)(void));                   /* atexit, through _onexit */
    int32_t (*set_tab_stops)(void *font, int32_t count, int32_t *stops);   /* CSimsFont::SetTabStops */
    int32_t (*get_client_rect)(uint32_t window, struct fs98_options_rect *rect);   /* USER32 */
    int32_t (*get_sims_text_buffer)(void *control, char *buffer, int32_t count);   /* CSimsText::GetWindowTextA */
    int32_t (*text_length)(const char *text);                       /* lstrlenA */
    void (*string_append)(void *string, const char *text);          /* CString::operator+= */
    void (*string_append_char)(void *string, char character);       /* CString::operator+=(char) */
    void (*list_text)(void *list, int32_t index, void *string);     /* CListBox::GetText */
    int32_t (*format_text)(char *out, const char *format, const char *text);   /* MSVCRT sprintf */
    void (*list_destroy)(void *list);                               /* ~CSimsListBox */
    void *(*dialog_item)(void *object, int32_t id);                 /* CWnd::GetDlgItem */
    void (*remap_rect)(struct fs98_options_rect *rect, void *control);  /* SIMSUI RemapCtrlRect */
    void (*move_window)(void *window, int32_t x, int32_t y, int32_t width, int32_t height,
                        int32_t repaint);                           /* CWnd::MoveWindow */
    char *(*get_buffer)(void *string, int32_t size);                /* CString::GetBuffer */
    void (*release_buffer)(void *string, int32_t size);             /* CString::ReleaseBuffer */
    void (*string_append_string)(void *string, const void *other);  /* CString::operator+=(CString &) */
    int32_t (*message_box)(const char *text, uint32_t type, uint32_t help);   /* AfxMessageBox */
    void (*paint_construct)(void *dc, void *window);                /* CPaintDC(CWnd *) */
    void (*paint_destroy)(void *dc);                                /* ~CPaintDC */
    int32_t (*dc_mode)(void *dc, int32_t mode);                     /* MFC #5875, given 1 */
    uint32_t (*dc_align)(void *dc, uint32_t align);                 /* MFC #6170, given 2 */
    int32_t (*get_window_rect)(uint32_t window, struct fs98_options_rect *rect);   /* USER32 */
    int32_t (*screen_to_client)(uint32_t window, void *point);      /* USER32 */
    int32_t (*to_int)(const char *text);                            /* MSVCRT atoi */
    int32_t (*format_int)(char *out, const char *format, int32_t value);   /* MSVCRT sprintf */
    int32_t (*get_window_long)(uint32_t window, int32_t index);     /* USER32 GetWindowLongA */
    int32_t (*set_radio)(void *control, int32_t checked);           /* CSimsRadioButton::SetChecked */
    char *(*long_to_text)(int32_t value, char *out, int32_t radix); /* MSVCRT _ltoa */
    int32_t (*to_long)(const char *text);                           /* MSVCRT atol */
    void (*ddx_int)(void *exchange, uint32_t id, int32_t *value);   /* DDX_Text into an int */
    void (*ddv_min_max)(void *exchange, int32_t value, int32_t low, int32_t high);   /* DDV_MinMaxInt */
    /* CNotify::Send(title, format, ...), cdecl with the CNotify first; given one value */
    int32_t (*notify_send)(void *notify, const char *title, const char *format, void *value);
    int16_t (*async_key_state)(int32_t key);                        /* USER32 GetAsyncKeyState */
    int32_t (*tab_init)(void *tab, void *dialog, void *font, uint32_t bitmap, void *dib, void *palette,
                        uint32_t a, uint32_t color, int32_t x, int32_t y, int32_t width, int32_t height,
                        uint32_t module, uint32_t b);                /* CSimsTabCtrl::Init */
    int32_t (*page_init)(void *page, int32_t value);               /* CSimsTabPage::Init */
    void (*add_page)(void *tab, void *page, char *title, int32_t value);   /* CSimsTabCtrl::AddPage */
    void (*helper_init)(void *helper, void *tab);                   /* CSimsTabCtrlHelper::Init */
    int32_t (*set_last_page)(void *tab, void *page);                /* CSimsTabCtrl::SetLastPage */
    int32_t (*set_active_tab)(void *tab, int32_t index, uint32_t window);  /* CSimsTabCtrl::SetActiveTab */
    int32_t (*get_active_tab)(void *tab);                           /* CSimsTabCtrl::GetActiveTab */
    int32_t (*slider_init)(void *slider, uint32_t bitmap, uint32_t thumb, void *dib, void *palette, uint32_t a,
                           uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f, uint32_t module);   /* CSimsSlider::Init */
    void (*set_scroll_range)(void *slider, int32_t low, int32_t high);   /* CSimsSlider::SetScrollRange */
    int32_t (*get_scroll_pos)(void *slider);                        /* CSimsSlider::GetScrollPos */
    int32_t (*set_scroll_pos)(void *slider, int32_t position);      /* CSimsSlider::SetScrollPos */
    int32_t (*string_load)(void *string, uint32_t id);              /* CString::LoadStringA */
    int32_t (*redraw_window)(uint32_t window, const void *rect, uint32_t region, uint32_t flags);  /* USER32 */
    uint32_t (*load_library)(const char *name);                     /* KERNEL32 LoadLibraryA */
    void *(*get_proc_address)(uint32_t library, const char *name);  /* KERNEL32 GetProcAddress */
    int32_t (*free_library)(uint32_t library);                      /* KERNEL32 FreeLibrary */
    uint32_t (*host_joystick_count)(void);                          /* HOST +0x1c0 */
    int32_t (*set_scroll_pos_window)(uint32_t window, int32_t bar, int32_t position, int32_t redraw);  /* USER32 SetScrollPos */
    int32_t (*get_scroll_pos_window)(uint32_t window, int32_t bar);  /* USER32 GetScrollPos */
    int32_t (*set_scroll_range_window)(uint32_t window, int32_t bar, int32_t low, int32_t high, int32_t redraw);
    int32_t (*get_scroll_range_window)(uint32_t window, int32_t bar, int32_t *low, int32_t *high);
    int32_t (*enable_scroll_bar)(uint32_t window, uint32_t bar, uint32_t arrows);   /* USER32 EnableScrollBar */
    int32_t (*client_to_screen)(uint32_t window, void *point);      /* USER32 ClientToScreen */
    int32_t (*update_window)(uint32_t window);                       /* USER32 UpdateWindow */
    int32_t (*format_two)(char *out, const char *format, const char *a, const char *b);   /* USER32 wsprintfA */
    void (*string_format)(void *string, uint32_t id, uint32_t value);   /* CString::Format(UINT, ...) */
    void (*check_list_init)(void *list, void *font, void *label);    /* CSimsCheckListBox::Init */
    void (*check_list_enable)(void *list, int32_t index, int32_t enable);   /* CCheckListBox::Enable */
    void (*check_list_set_check)(void *list, int32_t index, int32_t check); /* CCheckListBox::SetCheck */
    int32_t (*check_list_get_check)(void *list, int32_t index);     /* CCheckListBox::GetCheck */
    int32_t (*hot_key_create)(void *control, uint32_t style, const struct fs98_options_rect *rect, void *parent, uint32_t id);
    void (*hot_key_init)(void *control, void *font, uint32_t flags);
    int32_t (*hot_key_get)(void *control);
    void (*hot_key_set)(void *control, int32_t key);
    int32_t (*hot_joy_create)(void *control, uint32_t style, const struct fs98_options_rect *rect, void *parent, uint32_t id);
    void (*hot_joy_init)(void *control, void *font, uint32_t flags);
    uint32_t (*hot_joy_get)(void *control);
    void (*hot_joy_set)(void *control, uint32_t joystick, uint32_t sense);
    void (*host_key_name)(uint32_t key, char *out, uint32_t size);   /* HOST +0x200 */
    void (*host_joystick_name)(uint32_t input, char *out, uint32_t size);   /* HOST +0x1d0 */
    uint32_t (*host_dialog)(uint32_t id, void *block);               /* HOST +0x10c */
};
/* the Logbook's two function-local static CSimsFonts, laid out as in .data from
 * 0x223ac368, and the guard byte they share (0x223ac448: bit 1 the first, bit 2 the
 * second) */
struct fs98_options_book_state {
    uint8_t rows_font[0x70];        /* 0x223ac368, guard bit 2: the rows' six tab stops */
    uint8_t total_font[0x70];       /* 0x223ac3d8, guard bit 1: the totals line's four */
    uint8_t guard, padding[7];      /* 0x223ac448 */
};
void fs98_options_set_book_state(struct fs98_options_book_state *state);
void fs98_options_book_total_font_destroy(void);
void fs98_options_book_rows_font_destroy(void);
/* the rows list's own class: CSimsListBox with Options' vtable (0x223a7bb0) */
void fs98_options_set_book_list_vtable(void *vtable);
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
FS98_OPTIONS_HANDLERS(book);
FS98_OPTIONS_HANDLERS(landing);
FS98_OPTIONS_HANDLERS(manoeuvre);
FS98_OPTIONS_HANDLERS(analysis);
FS98_OPTIONS_HANDLERS(replay);
FS98_OPTIONS_HANDLERS(demo);
FS98_OPTIONS_HANDLERS(display);
FS98_OPTIONS_HANDLERS(display_aircraft);
FS98_OPTIONS_HANDLERS(display_map);
FS98_OPTIONS_HANDLERS(display_scenery);
FS98_OPTIONS_HANDLERS(display_hardware);
FS98_OPTIONS_HANDLERS(prefs);
FS98_OPTIONS_HANDLERS(prefs_general);
FS98_OPTIONS_HANDLERS(prefs_instrument);
FS98_OPTIONS_HANDLERS(prefs_international);
FS98_OPTIONS_HANDLERS(prefs_display);
FS98_OPTIONS_HANDLERS(prefs_sound);
FS98_OPTIONS_HANDLERS(prefs_flight);
FS98_OPTIONS_HANDLERS(controls);
FS98_OPTIONS_HANDLERS(controls_sensitivity);
FS98_OPTIONS_HANDLERS(controls_keys);
FS98_OPTIONS_HANDLERS(controls_force);
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

/* the Logbook (options_book.c) */
typedef uint32_t (FS98_OPTIONS_STDCALL *fs98_options_book_fn)(uint32_t operation, uint32_t index, void *block);
void FS98_OPTIONS_THISCALL fs98_options_book_close(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_accept(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_book_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_new_row(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_edit_row(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_delete_row(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_new_book(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_delete_book(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_show_book(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_choose_row(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_fill_books(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_refresh(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_book_has_descriptor(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_book_check(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_rename(void *object);
void FS98_OPTIONS_THISCALL fs98_options_book_scroll(void *object, uint32_t a, uint32_t b, uint32_t c);
void *FS98_OPTIONS_THISCALL fs98_options_book_list_delete(void *list, uint32_t flags);

/* the landing and manoeuvre graphs (options_graphs.c) */
typedef uint32_t (FS98_OPTIONS_STDCALL *fs98_options_graph_fn)(uint32_t operation, void *block, uint32_t window);
void FS98_OPTIONS_THISCALL fs98_options_landing_accept(void *object);
void FS98_OPTIONS_THISCALL fs98_options_landing_cancel(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_landing_init(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_landing_draw(void *object, uint32_t w_param, uint32_t l_param);
void FS98_OPTIONS_THISCALL fs98_options_landing_scale(void *object, void *dc);
void FS98_OPTIONS_THISCALL fs98_options_landing_paint(void *object);
void FS98_OPTIONS_THISCALL fs98_options_landing_set_up(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_landing_has_descriptor(void *object);
void FS98_OPTIONS_THISCALL fs98_options_manoeuvre_accept(void *object);
void FS98_OPTIONS_THISCALL fs98_options_manoeuvre_cancel(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_manoeuvre_init(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_manoeuvre_draw(void *object, uint32_t w_param, uint32_t l_param);
void FS98_OPTIONS_THISCALL fs98_options_manoeuvre_set_up(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_manoeuvre_has_descriptor(void *object);

/* Flight analysis (options_analysis.c); 0x223957e0 is shared with Display options,
 * Preferences and Customise controls */
uint32_t FS98_OPTIONS_THISCALL fs98_options_has_descriptor(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_accept(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_analysis_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_off(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_third(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_first(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_second(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_nothing(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_analysis_populate(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_analysis_error(void *object);

/* Instant replay (options_replay.c); its close is the shared fs98_options_notify_close */
void FS98_OPTIONS_THISCALL fs98_options_replay_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_replay_accept(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_replay_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_replay_populate(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_replay_has_descriptor(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_replay_validate(void *object, uint32_t unused);
uint32_t FS98_OPTIONS_THISCALL fs98_options_replay_error(void *object, uint32_t code);
void FS98_OPTIONS_THISCALL fs98_options_flight_video_rate(void *object);
/* Select demo (options_demo.c) */
void FS98_OPTIONS_THISCALL fs98_options_demo_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_demo_accept(void *object);
void FS98_OPTIONS_THISCALL fs98_options_demo_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_demo_select(void *object);
void FS98_OPTIONS_THISCALL fs98_options_demo_populate(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_demo_has_descriptor(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_demo_init(void *object);

/* Display options and its pages (options_display.c) */
void fs98_options_dialog_delete(void *object);                      /* CSimsDialog::operator delete */
void FS98_OPTIONS_THISCALL fs98_options_display_aircraft_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_aircraft_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_aircraft_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_map_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_map_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_map_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_scenery_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_scenery_enable(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_scenery_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_scenery_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_fill_drivers(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_fill_modes(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_list_drivers(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_list_modes(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_accelerate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_refresh(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_driver(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_changed(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_hardware_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_accept(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_close(void *object);
void FS98_OPTIONS_THISCALL fs98_options_display_attach(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_display_error(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_display_init(void *object);

/* Preferences and its pages (options_prefs.c) */
void FS98_OPTIONS_THISCALL fs98_options_prefs_general_choose(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_general_first(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_general_second(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_general_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_general_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_general_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_instrument_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_instrument_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_instrument_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_international_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_international_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_international_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_display_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_display_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_display_button(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_display_preset(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_display_other(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_display_box(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_display_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_box0(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_box1(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_box2(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_box3(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_box4(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_flight_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_flight_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_flight_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_accept(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_close(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_defaults(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_attach(void *object);
uint32_t FS98_OPTIONS_THISCALL fs98_options_prefs_general_error(void *page);
uint32_t FS98_OPTIONS_THISCALL fs98_options_prefs_error(void *object);
void FS98_OPTIONS_THISCALL fs98_options_prefs_sound_box(void *page, uint32_t index);
int32_t FS98_OPTIONS_THISCALL fs98_options_prefs_init(void *object);

/* Customise controls and its pages (options_controls.c, options_keys.c) */
void FS98_OPTIONS_THISCALL fs98_options_controls_force_master(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_box1(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_box2(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_box3(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_box4(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_box5(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_fill_devices(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_show_mouse(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_show_keyboard(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_show_joystick(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_sensitivity_populate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_reset(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_axis(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_device(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_scale(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_null_zone(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_reverse(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_joysticks(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_calibrate(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_sensitivity_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_cancel(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_read(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_accept(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_close(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_attach(void *object);
void FS98_OPTIONS_THISCALL fs98_options_controls_force_box(void *page, uint32_t index);
uint32_t FS98_OPTIONS_THISCALL fs98_options_controls_keyboard_axis(void *page, uint32_t k, uint32_t list);
uint32_t FS98_OPTIONS_THISCALL fs98_options_controls_mouse_axis(void *page, uint32_t k, uint32_t list);
uint32_t FS98_OPTIONS_THISCALL fs98_options_controls_row_axis(void *page, uint32_t n);
int32_t FS98_OPTIONS_THISCALL fs98_options_controls_axis_row(void *page);
void FS98_OPTIONS_THISCALL fs98_options_controls_fill_axes(void *page, uint32_t unused);
int32_t FS98_OPTIONS_THISCALL fs98_options_controls_changed(void *page, uint32_t w_param, uint32_t l_param);
uint32_t FS98_OPTIONS_THISCALL fs98_options_controls_error(void *object);
int32_t FS98_OPTIONS_THISCALL fs98_options_controls_init(void *object);
void FS98_OPTIONS_THISCALL fs98_options_keys_flight(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_slew(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_command(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_key(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_stick(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_check(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_clear(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_assign_key(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_assign_stick(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_assign(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_key_captured(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_stick_captured(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_reset(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_populate(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_init(void *page);
void FS98_OPTIONS_THISCALL fs98_options_keys_top(void *page, int32_t top);
void FS98_OPTIONS_THISCALL fs98_options_keys_joystick(void *page, uint32_t joystick);
void FS98_OPTIONS_THISCALL fs98_options_keys_vertical(void *page, uint32_t code, uint32_t position, void *bar);
void FS98_OPTIONS_THISCALL fs98_options_keys_horizontal(void *page, uint32_t code, uint32_t position, void *bar);
int32_t FS98_OPTIONS_THISCALL fs98_options_keys_changed(void *page, uint32_t w_param, uint32_t l_param);

#endif
