/* Options' handlers shared by every class, and each class's help, context-help and data
 * exchange. Offsets are the retail PE32 object layouts; every MFC, SIMSUI and Win32 call
 * goes through the service table, in retail's order. */
#include "options_internal.h"
#include "options_tables.inc"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL

static const struct fs98_options_services *s_services;
static uint32_t s_resource_module;
static const char *s_help_file = "fs98.hlp";
const char fs98_options_empty_text[] = "";

void fs98_options_set_services(const struct fs98_options_services *services) { s_services = services; }
const struct fs98_options_services *fs98_options_services(void) { return s_services; }
uint32_t fs98_options_resource_module(void) { return s_resource_module; }
void fs98_options_set_resource_module(uint32_t module) { s_resource_module = module; }
void fs98_options_set_help_file(const char *file) { s_help_file = file; }

/* ---- shared by every class --------------------------------------------------------- */

/* 0x22392400: an empty virtual */
void TC fs98_options_empty(void *object) { (void)object; }
/* 0x22392410, 0x22392420: the dialog's window disabled and enabled */
void TC fs98_options_disable(void *object) { s_services->enable_window(D32(object, 0x20), 0); }
void TC fs98_options_enable(void *object) { s_services->enable_window(D32(object, 0x20), 1); }
/* 0x223943d0: an empty virtual that takes one argument */
void TC fs98_options_empty_arg(void *object, uint32_t unused) { (void)object; (void)unused; }
/* 0x22397c30: delete through the deleting destructor */
void TC fs98_options_post_destroy(void *object)
{
    if (object) ((void *(TC *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1);
}
/* 0x22392f20: the error check after a failed descriptor check; it never finds one */
uint32_t TC fs98_options_no_error(void *object) { (void)object; return 0; }
/* 0x223943c0: the descriptor's own check, given the descriptor */
uint32_t TC fs98_options_check(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    return ((fs98_options_check_fn)PTR(descriptor, 4))(descriptor);
}

/* WM_HELP and WM_CONTEXTMENU: WinHelp over the class's table of control and topic */
#define OPTIONS_HELP(name) \
int32_t TC fs98_options_##name##_help(void *object, uint32_t unused, void *info) \
{ \
    (void)object; (void)unused; \
    return s_services->win_help(D32(info, 0xc), s_help_file, 0xc, s_help_##name); \
} \
int32_t TC fs98_options_##name##_context(void *object, uint32_t window, uint32_t unused) \
{ \
    (void)object; (void)unused; \
    s_services->win_help(window, s_help_file, 0xa, s_help_##name); \
    return 1; \
}
/* DoDataExchange: each control bound in order, without CSimsDialog's own */
#define OPTIONS_CLASS(name) \
OPTIONS_HELP(name) \
void TC fs98_options_##name##_exchange(void *object, void *exchange) \
{ \
    uint32_t index; \
    for (index = 0; (int32_t)index < (int32_t)s_ddx_count_##name; ++index) \
        s_services->ddx_control(exchange, s_ddx_##name[index][1], \
                                AT(object, s_ddx_##name[index][0])); \
}
OPTIONS_CLASS(video_finished)   /* 0x2239dff0, 0x2239e010; 0x2239df00 */
OPTIONS_CLASS(video_message)    /* 0x2239e430, 0x2239e450; 0x2239e170 */
OPTIONS_CLASS(landing)          /* 0x22397fd0, 0x22397ff0; 0x22397ab0 */
OPTIONS_CLASS(manoeuvre)        /* 0x2239b050, 0x2239b070; 0x2239ace0 */
OPTIONS_CLASS(demo)             /* 0x2239c6c0, 0x2239c6e0; 0x2239c300 */
OPTIONS_CLASS(display_aircraft)   /* 0x22392e30, 0x22392e50; 0x22392c30 */
OPTIONS_CLASS(display_map)        /* 0x22393180, 0x223931a0; 0x22393070 */
OPTIONS_CLASS(display_scenery)    /* 0x223948a0, 0x223948c0; 0x223945d0 */
OPTIONS_CLASS(display_hardware)   /* 0x22393710, 0x22393730; 0x22393450 */
OPTIONS_CLASS(prefs_general)      /* 0x22395ea0, 0x22395ec0; 0x22395a80 */
OPTIONS_CLASS(prefs_instrument)   /* 0x223977d0, 0x223977f0; 0x22397650 */
OPTIONS_CLASS(prefs_international) /* 0x223967e0, 0x22396800; 0x223965d0 */
OPTIONS_CLASS(prefs_display)      /* 0x22392850, 0x22392870; 0x22392620 */
OPTIONS_CLASS(prefs_sound)        /* 0x22391f70, 0x22391f90; 0x22391c00 */
OPTIONS_CLASS(prefs_flight)       /* 0x2239b390, 0x2239b3b0; 0x2239b230 */
OPTIONS_CLASS(controls_sensitivity) /* 0x2239f380, 0x2239f3a0; 0x2239eef0 */
OPTIONS_CLASS(controls_keys)      /* 0x223a0ef0, 0x223a0f10; 0x223a0db0 */
OPTIONS_CLASS(controls_force)     /* 0x223a29f0, 0x223a2a10; 0x223a2950 */
#undef OPTIONS_CLASS
/* the same, after CSimsDialog's own exchange */
#define OPTIONS_CLASS_BASE(name) \
OPTIONS_HELP(name) \
void TC fs98_options_##name##_exchange(void *object, void *exchange) \
{ \
    uint32_t index; \
    s_services->base_exchange(object, exchange); \
    for (index = 0; (int32_t)index < (int32_t)s_ddx_count_##name; ++index) \
        s_services->ddx_control(exchange, s_ddx_##name[index][1], \
                                AT(object, s_ddx_##name[index][0])); \
}
OPTIONS_CLASS_BASE(save_video)    /* 0x2239dbd0, 0x2239dbf0; 0x2239d6f0 */
OPTIONS_CLASS_BASE(flight_video)  /* 0x2239cf50, 0x2239cf70; 0x2239ca60 */
OPTIONS_CLASS_BASE(analysis)      /* 0x22395140, 0x22395160; 0x22394d00 */
OPTIONS_CLASS_BASE(display)       /* 0x22393e60, 0x22393e80; 0x22393cf0 */
OPTIONS_CLASS_BASE(prefs)         /* 0x2239b840, 0x2239b860; 0x2239b690 */
OPTIONS_CLASS_BASE(controls)      /* 0x2239e750, 0x2239e770; 0x2239e610 */
#undef OPTIONS_CLASS_BASE
/* the controls, then each CString with its edit and its limit */
static void exchange_texts(void *object, void *exchange, const uint16_t (*controls)[2], uint32_t count,
                           const uint32_t (*texts)[3], uint32_t text_count)
{
    uint32_t index;
    for (index = 0; index < count; ++index)
        s_services->ddx_control(exchange, controls[index][1], AT(object, controls[index][0]));
    for (index = 0; index < text_count; ++index) {
        s_services->ddx_text(exchange, texts[index][1], AT(object, texts[index][0]));
        s_services->ddv_max_chars(exchange, AT(object, texts[index][0]), (int32_t)texts[index][2]);
    }
}
OPTIONS_HELP(add_logbook)         /* 0x2239ab50, 0x2239ab70 */
/* 0x2239a7d0: the name's CString, at most 0xf0 */
void TC fs98_options_add_logbook_exchange(void *object, void *exchange)
{
    static const uint32_t texts[][3] = {{0x1400, 0x11a4, 0xf0}};
    exchange_texts(object, exchange, s_ddx_add_logbook, s_ddx_count_add_logbook, texts, 1);
}
OPTIONS_HELP(replay)              /* 0x22397110, 0x22397130 */
/* 0x22396c30: CSimsDialog's own exchange, the controls, then the speed as an int, 0..100 */
void TC fs98_options_replay_exchange(void *object, void *exchange)
{
    uint32_t index;
    s_services->base_exchange(object, exchange);
    for (index = 0; index < (uint32_t)s_ddx_count_replay; ++index)
        s_services->ddx_control(exchange, s_ddx_replay[index][1], AT(object, s_ddx_replay[index][0]));
    s_services->ddx_int(exchange, 0x119d, (int32_t *)AT(object, 0x1a34));
    s_services->ddv_min_max(exchange, *(int32_t *)AT(object, 0x1a34), 0, 0x64);
}
OPTIONS_HELP(book)                /* 0x22398a50, 0x22398a70 */
/* 0x223983a0: CSimsDialog's own exchange, the controls, then the title's CString (0x1e) */
void TC fs98_options_book_exchange(void *object, void *exchange)
{
    static const uint32_t texts[][3] = {{0x1af0, 0x119a, 0x1e}};
    s_services->base_exchange(object, exchange);
    exchange_texts(object, exchange, s_ddx_book, s_ddx_count_book, texts, 1);
}
OPTIONS_HELP(logbook_entry)       /* 0x22399d40, 0x22399d60 */
/* 0x22399ae0: the day, instrument and night times (6), the remarks (0x18), the date (8) */
void TC fs98_options_logbook_entry_exchange(void *object, void *exchange)
{
    static const uint32_t texts[][3] = {
        {0x1c90, 0x1194, 6}, {0x1c94, 0x1195, 6}, {0x1c98, 0x1196, 6}, {0x1c9c, 0x1198, 0x18}, {0x1ca0, 0x1199, 8}
    };
    exchange_texts(object, exchange, s_ddx_logbook_entry, s_ddx_count_logbook_entry, texts, 5);
}
#undef OPTIONS_HELP
