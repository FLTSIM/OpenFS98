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
 * (0x14040003, DEMO's dialog_stopped) is given no descriptor. */
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
};
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
#endif
