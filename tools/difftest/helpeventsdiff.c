/* Help's event boundary, original PE32 routines versus C. Every operation
 * starts from the same guarded fixture, including side-effecting imports. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/help/help_internal.h"
#include "../../src/help/help_objects.h"
#include "../../src/help/help_objects_tables.inc"

#define TC FS98_HELP_THISCALL
#define SC FS98_HELP_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define TEST_THROW 0x0e041a22
#define LOG_SIZE 32768
struct fixture {
    uint8_t guard_before[32];
    uint8_t object[0x197c];
    uint8_t descriptor[2][0x30];
    uint8_t records[4][0xa08];
    uint8_t help[32];
    uint8_t exchange[32];
    uint8_t buffer[64];
    uint32_t read;
    struct fs98_help_about_state about_state;
    uint8_t guard_after[32];
};
static struct fixture s_fixture, s_initial, s_retail_result;
static HMODULE s_retail;
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_script;
static uint32_t s_calls, s_log_length, s_throw_at, s_saved_seh;
static uint32_t s_max_calls, s_throws, s_comparisons, s_swaps, s_reads, s_resets, s_allocation_faults;
static int s_side;
static char s_logs[2][LOG_SIZE];
static void *s_vtable[60], *s_other_vtable[60], *s_control_vtable[60], *s_jump[5];

typedef void (TC *event_fn)(void *);
struct operation { uint32_t rva; event_fn event; uint32_t signature; const char *name; };
#define EVENT(rva, name) {rva, fs98_help_##name, 0, #name}
#define SPECIAL(rva, sig, name) {rva, (event_fn)0, sig, #name}
#define CLASS(help, context, ddx, name) \
    SPECIAL(help, 1, name##_help), SPECIAL(context, 2, name##_context), \
    SPECIAL(ddx, 3, name##_exchange)
static const struct operation s_operations[] = {
    EVENT(0x18c0, about_cancel), EVENT(0x18e0, about_accept), EVENT(0x1900, about_credits),
    EVENT(0x1ef0, credits_close), EVENT(0x1f60, credits_cancel), EVENT(0x1f80, credits_accept),
    EVENT(0x2010, credits_load), SPECIAL(0x2130, 4, credits_start_timer),
    SPECIAL(0x2180, 5, credits_timer), EVENT(0x21f0, credits_toggle),
    EVENT(0x2770, lessons_cancel), EVENT(0x2790, lessons_accept), EVENT(0x2800, lessons_sync),
    EVENT(0x2870, lessons_populate), SPECIAL(0x2930, 4, lessons_no_error),
    SPECIAL(0x2940, 4, lessons_validate), EVENT(0x2950, lessons_select),
    EVENT(0x2a80, lessons_double_click), SPECIAL(0x2c20, 6, graph_tick),
    EVENT(0x2c50, graph_accept), EVENT(0x2de0, graph_notify), SPECIAL(0x2e00, 4, graph_validate),
    EVENT(0x3090, completion_accept), EVENT(0x30b0, completion_cancel),
    EVENT(0x2760, post_destroy), EVENT(0x1b50, disable), EVENT(0x1b60, enable),
    EVENT(0x1b40, empty), SPECIAL(0x2250, 5, empty_arg), SPECIAL(0x1fe0, 7, credits_read),
    CLASS(0x1910, 0x1930, 0x1620, about),
    CLASS(0x1fa0, 0x1fc0, 0x1ce0, credits),
    CLASS(0x2aa0, 0x2ac0, 0x2480, lessons),
    CLASS(0x2da0, 0x2dc0, 0x2be0, graph),
    CLASS(0x30d0, 0x30f0, 0x2f50, completion),
    SPECIAL(0x2ca0, 4, graph_init), SPECIAL(0x2fa0, 4, completion_init),
    SPECIAL(0x2570, 4, lessons_init), SPECIAL(0x1d50, 4, credits_init),
#define LIFETIME(ctor, dtor, delete, factory, name) \
    SPECIAL(ctor, 8, name##_construct), SPECIAL(dtor, 9, name##_destroy), \
    SPECIAL(delete, 10, name##_delete), SPECIAL(factory, 11, name##_create), \
    SPECIAL(ctor, 12, name##_roundtrip), SPECIAL(ctor, 13, name##_construct_delete)
    LIFETIME(0x1410, 0x1530, 0x1510, 0x11c0, about),
    LIFETIME(0x1b70, 0x1c50, 0x1c30, 0x1250, credits),
    LIFETIME(0x2280, 0x2390, 0x2370, 0x1130, lessons),
    LIFETIME(0x2ae0, 0x2b70, 0x2b50, 0x12e0, graph),
    LIFETIME(0x2e30, 0x2ed0, 0x2eb0, 0x1370, completion),
    SPECIAL(0x2260, 14, placeholder_delete), SPECIAL(0x1400, 15, class_delete),
    SPECIAL(0x1950, 16, about_populate), SPECIAL(0x1710, 18, about_init),
    SPECIAL(0x18b0, 17, about_font_destroy), SPECIAL(0x1710, 19, about_init_twice)
#undef LIFETIME
};
#define OP_COUNT (sizeof(s_operations) / sizeof(s_operations[0]))
static uint32_t next_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t offset(const void *pointer)
{
    if (pointer == (uint8_t *)s_retail + 0x51c8) pointer = s_fixture.about_state.font;
    return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture); }
static void put(const char *format, ...)
{
    va_list args;
    int length;
    if (s_log_length > LOG_SIZE - 2048) exit(2);
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_log_length, LOG_SIZE - s_log_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= LOG_SIZE - s_log_length) exit(2);
    s_log_length += (uint32_t)length;
    ++s_calls;
    if (s_throw_at && s_calls == s_throw_at) RaiseException(TEST_THROW, 0, 0, 0);
}
/* One boundary per operation can replace the descriptor/vtable, change list
 * count/selection and alter a handle. All choices remain valid fixtures. */
static void mutate(void)
{
    uint8_t *object = s_fixture.object;
    uint32_t descriptor_offset;
    void *descriptor;
    if (s_calls != 1 + (s_scenario % 4)) return;
    descriptor_offset = s_operation == 4 || s_operation == 5 ? 0x1174 :
                         ((s_operation >= 18 && s_operation <= 21) || s_operation == 45) ? 0x10e0 : 0x10dc;
    if (s_operation == 3 || s_operation == 6 || s_operation == 7 ||
        s_operation == 8 || s_operation == 9 || s_operation == 29 || s_operation == 48) {
        if (s_script & 1) D32(object, 0x1108) ^= 1;
        if (s_script & 2) D32(object, 0x10ec) = s_seed;
    } else {
        descriptor = PTR(object, descriptor_offset);
        if ((s_script & 1) && (descriptor == s_fixture.descriptor[0] ||
                                descriptor == s_fixture.descriptor[1])) {
            PTR(object, descriptor_offset) = descriptor == s_fixture.descriptor[0] ?
                s_fixture.descriptor[1] : s_fixture.descriptor[0];
            if (!s_side) ++s_swaps;
        }
        if ((s_script & 2) && ((s_operation >= 10 && s_operation <= 17) || s_operation == 47)) {
            descriptor = PTR(object, 0x10dc);
            if (descriptor) {
                D32(descriptor, 8) = 2;
                D32(descriptor, 0xc) = 1;
            }
        }
    }
    if (s_script & 4) PTR(object, 0) = s_other_vtable;
    if (s_script & 8) D32(object, 0x17a8) = s_seed ^ 0x12345678;
}
static void TC virtual_close(void *object)
{
    put("virtual.close object=%u state=%u/%u/%u/%u desc=%u/%u\n", offset(object),
        D32(object, 0x1978), D32(object, 0x1498), D32(object, 0x187c), D32(object, 0x12cc),
        D32(s_fixture.descriptor[0], 0), D32(s_fixture.descriptor[1], 0));
    mutate();
}
static void TC virtual_close_other(void *object) { put("other.close\n"); virtual_close(object); }
static void TC virtual_accept(void *object) { put("virtual.accept %u\n", offset(object)); mutate(); }
static void TC virtual_accept_other(void *object) { put("other.accept\n"); virtual_accept(object); }
static void TC virtual_cancel(void *object) { put("virtual.cancel %u\n", offset(object)); mutate(); }
static void TC virtual_cancel_other(void *object) { put("other.cancel\n"); virtual_cancel(object); }
static void *TC virtual_delete(void *object, uint32_t flags)
{ put("virtual.delete %u flags=%u\n", offset(object), flags); mutate(); return object; }
static int32_t SC about_callback(void *descriptor)
{ put("about.callback %u\n", offset(descriptor)); mutate(); return (int32_t)s_seed; }
static int32_t SC graph_callback(uint32_t operation, void *descriptor, uint32_t window)
{
    put("graph.callback op=%u desc=%u hwnd=%u state=%u answer=%u\n", operation,
        offset(descriptor), window, D32(s_fixture.object, 0x10e4), D32(descriptor, 0));
    mutate(); return (int32_t)s_seed;
}
static int32_t send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{
    uint32_t result = s_seed;
    if (message == 0x180) put("message %u %x %u text=%s\n", window, message, w_param, (char *)l_param);
    else if (message == 0x449) {
        uint32_t read = 0xdeadbeef;
        uint8_t buffer[32];
        put("stream %u %x %u ptr=%u file=%u error=%u callback=%u\n", window, message,
            w_param, offset((void *)l_param), D32((void *)l_param, 0), D32((void *)l_param, 4),
            PTR((void *)l_param, 8) == (s_side ? (void *)fs98_help_credits_read : (uint8_t *)s_retail + 0x1fe0));
        mutate();
        memset(buffer, 0xcd, sizeof(buffer));
        result = ((uint32_t (SC *)(uint32_t, void *, int32_t, uint32_t *))PTR((void *)l_param, 8))(
            D32((void *)l_param, 0), buffer, sizeof(buffer), &read);
        put("stream.result %u read=%u byte=%u/%u\n", result, read, buffer[0], buffer[31]);
        return (int32_t)result;
    } else put("message %u %x %u %u\n", window, message, w_param, l_param);
    mutate();
    if (message == 0x188) result = s_scenario % 5 == 0 ? 0xffffffff : (s_scenario + s_calls) % 4;
    if (message == 0xce) {
        static const uint32_t lines[] = {0x80000000, 0xffffffff, 0, 1, 6, 0x7fffffff};
        result = lines[s_scenario % 6];
    }
    if (message == 0x115 && w_param == 6 && !s_side) ++s_resets;
    return (int32_t)result;
}
static int32_t win_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{
    uint32_t index = 0;
    char pairs[1024];
    int length = 0;
    do {
        if (index >= 128 || length > 900) exit(2);
        length += snprintf(pairs + length, sizeof(pairs) - length, "%x:%x,", table[index], table[index + 1]);
        index += 2;
    } while (table[index - 2] || table[index - 1]);
    put("help %u %s %u [%s]\n", window, file, command, pairs);
    mutate(); return (int32_t)s_seed;
}
static int32_t enable_window(uint32_t window, int32_t enable)
{ put("enable.window %u %d\n", window, enable); mutate(); return (int32_t)s_seed; }
static int32_t kill_timer(uint32_t window, uint32_t timer)
{ put("kill.timer %u %u state=%u\n", window, timer, D32(s_fixture.object, 0x10e8)); mutate(); return (int32_t)s_seed; }
static uint32_t set_timer(uint32_t window, uint32_t timer, uint32_t period, void *callback)
{
    put("set.timer %u %u %u callback=%u old=%u\n", window, timer, period,
        (uint32_t)callback, D32(s_fixture.object, 0x10e8));
    mutate();
    return timer >= 3 + s_scenario % 14 ? s_seed | 1 : 0;
}
static int32_t destroy_window(uint32_t window)
{ put("destroy.window %u timer=%u\n", window, D32(s_fixture.object, 0x10e8)); mutate(); return (int32_t)s_seed; }
static int32_t free_library(uint32_t module)
{ put("free.library %u rich=%u\n", module, D32(s_fixture.object, 0x10ec)); mutate(); return (int32_t)s_seed; }
static void close_dialog(void *object) { put("base.close %u\n", offset(object)); mutate(); }
static int32_t message_box(const char *text, uint32_t type, uint32_t help)
{ put("message.box [%s] %u %u\n", text, type, help); mutate(); return (int32_t)s_seed; }
static int32_t get_window_long(uint32_t window, int32_t index)
{ put("get.window.long %u %d\n", window, index); mutate(); return s_scenario & 1 ? (int32_t)window : (int32_t)(window ^ 1); }
static int32_t set_checked(void *control, int32_t checked)
{ put("set.checked %u %d\n", offset(control), checked); mutate(); return (int32_t)s_seed; }
static void set_text(void *control, const char *text)
{ put("set.text %u [%s]\n", offset(control), text); mutate(); }
static int32_t enable_control(void *control, int32_t enabled)
{ put("enable.control %u %d\n", offset(control), enabled); mutate(); return (int32_t)s_seed; }
static int32_t load_string(uint32_t id, char *buffer, int32_t capacity)
{ put("load.string %x %d\n", id, capacity); strcpy(buffer, "lessons"); mutate(); return 7; }
static int32_t split_path(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{
    put("split.path [%s] drive=%u dir=%u name=%u ext=%u\n", path, !!drive, !!dir, !!name, !!ext);
    if (drive) strcpy(drive, "C");
    if (dir) strcpy(dir, "training");
    if (name) snprintf(name, 32, "lesson%u", s_scenario % 9);
    if (ext) *ext = 0x1d;
    mutate(); return (int32_t)s_seed;
}
static int32_t join_path(char *out, const char *drive, const char *dir, const char *name)
{
    char result[256];
    put("join.path drive=%s dir=%s name=%s alias=%u/%u\n", drive ? drive : "NULL",
        dir ? dir : "NULL", name ? name : "NULL", out == dir, out == name);
    snprintf(result, sizeof(result), "%s/%s/%s", drive ? drive : "", dir ? dir : "", name ? name : "");
    strcpy(out, result); mutate(); return (int32_t)s_seed;
}
static int32_t exists(const char *path, int32_t extension)
{ put("exists [%s] %d\n", path, extension); mutate(); return s_scenario % 3 ? 0 : -1; }
static void base_exchange(void *object, void *exchange)
{ put("base.exchange %u %u\n", offset(object), offset(exchange)); mutate(); }
static void ddx_control(void *exchange, uint32_t id, void *control)
{ put("ddx %u %u %u\n", offset(exchange), id, offset(control)); mutate(); }
static int32_t read_file(uint32_t file, void *buffer, uint32_t count, uint32_t *read, void *overlapped)
{
    put("read.file %u count=%u overlapped=%u\n", file, count, (uint32_t)overlapped);
    memset(buffer, s_seed & 255, count > 64 ? 64 : count);
    *read = s_scenario % 2 ? count : 0xbad;
    mutate(); if (!s_side) ++s_reads;
    return s_scenario % 2;
}
static char *copy_text(char *out, const char *text)
{ put("copy.text %u [%s] descriptor=%u\n", offset(out), text, offset(PTR(s_fixture.object, 0x1174))); strcpy(out, text); mutate(); return out; }
static int32_t open_file(const char *name, void *info, uint32_t mode)
{
    put("open.file [%s] mode=%u descriptor=%u name=%u\n", name, mode,
        offset(PTR(s_fixture.object, 0x1174)), offset(PTR(PTR(s_fixture.object, 0x1174), 0xc)));
    memset(info, 0, 0x88); mutate(); return s_scenario % 3 ? (int32_t)(s_seed & 0xffff) : -1;
}
static int32_t close_handle(uint32_t handle)
{ put("close.handle %u\n", handle); mutate(); return (int32_t)s_seed; }
static int32_t TC virtual_skin(void *object, int32_t resource, uint32_t module, uint32_t flags)
{
    put("skin %u resource=%x module=%u flags=%u\n", offset(object), resource, module, flags);
    mutate(); return s_scenario % 4 ? (int32_t)s_seed | 1 : 0;
}
static int32_t base_init(void *object)
{ put("base.init %u\n", offset(object)); mutate(); return (int32_t)s_seed; }
static int32_t dialog_init(void *object)
{ put("dialog.init %u\n", offset(object)); mutate(); return (int32_t)s_seed; }
static int32_t init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{
    put("init.art %u bitmap=%x dib=%u pal=%u flags=%u\n", offset(control), bitmap, offset(dib), offset(palette), flags);
    mutate(); return (int32_t)s_seed;
}
static int32_t init_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                          void *dib, void *palette, uint32_t flags, uint32_t color,
                          uint32_t module, int32_t x, int32_t y, struct fs98_help_rect rect)
{
    put("init.button %u font=%u/%u bmp=%x dib=%u pal=%u flags=%u color=%x module=%u xy=%d/%d rect=%d/%d/%d/%d\n",
        offset(control), offset(font), offset(alternate_font), bitmap, offset(dib), offset(palette), flags,
        color, module, x, y, rect.left, rect.top, rect.right, rect.bottom);
    mutate(); return (int32_t)s_seed;
}
static void init_text(void *control, void *font)
{ put("init.text %u font=%u\n", offset(control), offset(font)); mutate(); }
static void *get_control(void *object, uint32_t id)
{ put("get.control %u id=%x\n", offset(object), id); mutate(); return s_fixture.exchange; }
static void remap_rect(struct fs98_help_rect *rect, void *control)
{
    put("remap.rect %u\n", offset(control));
    rect->left = (int32_t)s_seed; rect->top = (int32_t)(s_seed ^ 0xdeadbeef);
    rect->right = (int32_t)(s_seed + 0x87654321); rect->bottom = (int32_t)(s_seed ^ 0x12345678);
    mutate();
}
static void move_control(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ put("move.control %u %d/%d %d/%d repaint=%d\n", offset(control), x, y, width, height, repaint); mutate(); }
static void init_group(void *control, void *font)
{ put("init.group %u font=%u\n", offset(control), offset(font)); mutate(); }
static void *base_font(int32_t id)
{ put("base.font %d\n", id); mutate(); return s_fixture.help; }
static void init_list(void *control, void *font, void *label)
{ put("init.list %u font=%u label=%u\n", offset(control), offset(font), offset(label)); mutate(); }
static void init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags)
{
    put("init.edit %u font=%u fg=%x bg=%x error=%x label=%u flags=%u\n", offset(control),
        offset(font), foreground, background, error_color, offset(label), flags);
    mutate();
}
static int32_t TC virtual_radio(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{
    put("init.radio %u font=%u bmp=%x dib=%u pal=%u module=%u\n", offset(control), offset(font),
        bitmap, offset(dib), offset(palette), module);
    mutate(); return (int32_t)s_seed;
}
static int32_t copy_rect(struct fs98_help_rect *out, const struct fs98_help_rect *in)
{ put("copy.rect input=%u\n", offset(in)); *out = *in; mutate(); return (int32_t)s_seed; }
static int32_t TC placeholder_destroy(void *control)
{ put("placeholder.destroy %u\n", offset(control)); mutate(); return (int32_t)s_seed; }
static void common_controls(void) { put("common.controls\n"); mutate(); }
static uint32_t load_library(const char *name)
{ put("load.library [%s]\n", name); mutate(); return s_scenario % 3 ? s_seed : 0; }
static void *module_state(void)
{ put("module.state\n"); mutate(); return s_fixture.help; }
static uint32_t create_window(uint32_t ex_style, const char *class_info, const char *title, uint32_t style,
                             int32_t x, int32_t y, int32_t width, int32_t height, uint32_t parent,
                             uint32_t menu, uint32_t instance, void *param)
{
    put("create.window ex=%x class=[%s] title=[%s] style=%x xy=%d/%d size=%d/%d parent=%u menu=%u instance=%u param=%u\n",
        ex_style, class_info, title, style, x, y, width, height, parent, menu, instance, (uint32_t)param);
    mutate(); return s_scenario % 5 ? s_seed ^ 0x123 : 0;
}
static void string_construct(void *string)
{ put("string.construct %u\n", offset(string)); D32(string, 0) = 0x1234; mutate(); }
static void string_destroy(void *string)
{ put("string.destroy %u value=%u\n", offset(string), D32(string, 0)); mutate(); }
static int32_t reg_open(uint32_t key, const char *subkey, uint32_t options, uint32_t access, uint32_t *key_out)
{
    put("reg.open key=%x subkey=[%s] options=%u access=%x\n", key, subkey, options, access);
    *key_out = s_seed; mutate(); return s_scenario % 4 == 0 ? 2 : 0;
}
static int32_t reg_query(uint32_t key, const char *name, void *reserved, uint32_t *type, void *data, uint32_t *size)
{
    uint32_t capacity = *size;
    uint32_t length = s_scenario % 5 == 0 ? capacity - 1 : 6;
    int fail = s_scenario & (!strcmp(name, "Name") ? 1 : 2);
    put("reg.query key=%u name=[%s] reserved=%u type=%u capacity=%u\n", key, name, (uint32_t)reserved, (uint32_t)type, capacity);
    memset(data, !strcmp(name, "Name") ? 'N' : 'P', length);
    ((char *)data)[length] = 0; *size = length + 1;
    mutate(); return fail ? 234 : 0;
}
static int32_t load_ui_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{
    const char *text = id == 0xf70b ? "Version " : id == 0xf6fd ? "Build " : "Copyright fixture";
    put("load.ui.string module=%u id=%x capacity=%d\n", module, id, capacity);
    snprintf(buffer, capacity, "%s", s_scenario % 7 ? text : "");
    mutate(); return strlen(buffer);
}
static void set_sims_text(void *control, const char *text)
{
    put("set.sims.text %u [%s]\n", offset(control), text);
    mutate();
}
static int32_t format_version(char *out, const char *format, int32_t major, int32_t minor,
                            const char *prefix, int32_t build)
{
    put("format.version [%s] major=%d minor=%d prefix=[%s] build=%d\n", format, major, minor, prefix, build);
    mutate(); return sprintf(out, format, major, minor, prefix, build);
}
static char *append_text(char *out, const char *text)
{ put("append.text [%s] [%s]\n", out, text); mutate(); return strcat(out, text); }
static void font_construct(void *font, void *base_font)
{
    put("font.construct %u base=%u\n", offset(font), offset(base_font));
    memset(font, s_seed & 255, 0x70); mutate();
}
static void font_destroy(void *font)
{ put("font.destroy %u value=%x\n", offset(font), D32(font, 0)); mutate(); }
static void register_exit(void (*handler)(void))
{
    put("register.exit owns=%u\n", handler == (s_side ? fs98_help_about_font_destroy : (void (*)(void))((uint8_t *)s_retail + 0x18b0)));
    mutate();
}
static const struct fs98_help_services s_services = {
    send_message, win_help, enable_window, kill_timer, set_timer, destroy_window, free_library,
    close_dialog, message_box, get_window_long, set_checked, set_text, enable_control, load_string,
    split_path, join_path, exists, base_exchange, ddx_control, read_file, copy_text, open_file, close_handle,
    base_init, dialog_init, init_art, init_button, init_text, get_control, remap_rect,
    init_group, base_font, init_list, init_edit, copy_rect, common_controls, load_library, module_state, create_window,
    string_construct, string_destroy, reg_open, reg_query, load_ui_string, set_sims_text, format_version, append_text, font_construct, font_destroy, register_exit, move_control
};

static void *dialog_construct(void *object, int32_t resource, void *parent)
{
    put("dialog.construct %u resource=%x parent=%u\n", offset(object), resource, (uint32_t)parent);
    D32(object, 0) = 0xba5e0000; D32(object, 0x20) = s_seed;
    mutate(); return object;
}
static void dialog_destroy(void *object)
{ put("dialog.destroy %u vtable=%x\n", offset(object), D32(object, 0)); D32(object, 0) = 0xdead0000; mutate(); }
static void construct_control(void *control, uint32_t import)
{
    put("control.construct %u import=%x\n", offset(control), import);
    D32(control, 0) = import; D32(control, 4) = s_seed;
    mutate();
}
static void destroy_control(void *control, uint32_t import)
{
    put("control.destroy %u import=%x vtable=%x\n", offset(control), import, D32(control, 0));
    D32(control, 0) = import; mutate();
}
static void *allocate_object(uint32_t size)
{
    put("allocate %u\n", size); mutate();
    return s_scenario % 11 ? s_fixture.object : 0;
}
static void ui_delete(void *object) { put("ui.delete %u\n", offset(object)); mutate(); }
static void mfc_delete(void *object) { put("mfc.delete %u\n", offset(object)); mutate(); }
static void dialog_delete(void *object) { put("dialog.delete %u\n", offset(object)); mutate(); }
static int32_t create_dialog(void *object, uint32_t resource, void *parent)
{
    put("dialog.create %u resource=%x parent=%u module=%u\n", offset(object), resource, (uint32_t)parent, D32(object, 0x80));
    mutate(); return s_scenario & 1 ? 0 : (int32_t)s_seed;
}
static int32_t show_dialog(void *object, int32_t show)
{ put("dialog.show %u %d\n", offset(object), show); mutate(); return s_scenario & 2 ? 0 : (int32_t)s_seed; }
static const struct fs98_help_object_services s_object_services = {
    dialog_construct, dialog_destroy, construct_control, destroy_control, allocate_object,
    ui_delete, mfc_delete, dialog_delete, create_dialog, show_dialog
};
#define CONTROL_HOOK(name, ctor, dtor) \
static void *TC hook_construct_##name(void *control) { construct_control(control, ctor); return control; } \
static void TC hook_destroy_##name(void *control) { destroy_control(control, dtor); }
CONTROL_HOOK(text, 0x64a4, 0x64b8)
CONTROL_HOOK(group, 0x64a8, 0x64bc)
CONTROL_HOOK(button, 0x64ac, 0x64c0)
CONTROL_HOOK(art, 0x64b0, 0x6498)
CONTROL_HOOK(list, 0x644c, 0x645c)
CONTROL_HOOK(edit, 0x6458, 0x6460)
CONTROL_HOOK(radio, 0x6468, 0x6464)
CONTROL_HOOK(window, 0x6348, 0x634c)
#undef CONTROL_HOOK
static void *TC hook_dialog_construct(void *object, int32_t resource, void *parent)
{ return dialog_construct(object, resource, parent); }
static void TC hook_dialog_destroy(void *object) { dialog_destroy(object); }
static void SC hook_dialog_delete(void *object) { dialog_delete(object); }
static int32_t TC hook_create_dialog(void *object, uint32_t resource, void *parent)
{ return create_dialog(object, resource, parent); }
static int32_t TC hook_show_dialog(void *object, int32_t show) { return show_dialog(object, show); }

/* Original Win32/MFC import ABIs, separate from the plain-C service boundary. */
static int32_t SC hook_send(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return send_message(a,b,c,d); }
static int32_t SC hook_help(uint32_t a, const char *b, uint32_t c, const uint32_t *d) { return win_help(a,b,c,d); }
static int32_t SC hook_enable(uint32_t a, int32_t b) { return enable_window(a,b); }
static int32_t SC hook_kill(uint32_t a, uint32_t b) { return kill_timer(a,b); }
static uint32_t SC hook_timer(uint32_t a, uint32_t b, uint32_t c, void *d) { return set_timer(a,b,c,d); }
static int32_t SC hook_destroy(uint32_t a) { return destroy_window(a); }
static int32_t SC hook_free(uint32_t a) { return free_library(a); }
static void TC hook_close(void *object) { close_dialog(object); }
static int32_t SC hook_box(const char *a, uint32_t b, uint32_t c) { return message_box(a,b,c); }
static int32_t SC hook_long(uint32_t a, int32_t b) { return get_window_long(a,b); }
static int32_t TC hook_checked(void *a, int32_t b) { return set_checked(a,b); }
static void TC hook_text(void *a, const char *b) { set_text(a,b); }
static int32_t TC hook_control(void *a, int32_t b) { return enable_control(a,b); }
static int32_t SC hook_load(uint32_t a, char *b, int32_t c) { return load_string(a,b,c); }
static int32_t SC hook_split(const char *a, char *b, char *c, char *d, int32_t *e) { return split_path(a,b,c,d,e); }
static int32_t SC hook_join(char *a, const char *b, const char *c, const char *d) { return join_path(a,b,c,d); }
static int32_t SC hook_exists(const char *a, int32_t b) { return exists(a,b); }
static void TC hook_exchange(void *a, void *b) { base_exchange(a,b); }
static void SC hook_ddx(void *a, uint32_t b, void *c) { ddx_control(a,b,c); }
static int32_t SC hook_read(uint32_t a, void *b, uint32_t c, uint32_t *d, void *e) { return read_file(a,b,c,d,e); }
static char *SC hook_copy(char *a, const char *b) { return copy_text(a,b); }
static int32_t SC hook_open(const char *a, void *b, uint32_t c) { return open_file(a,b,c); }
static int32_t SC hook_close_handle(uint32_t a) { return close_handle(a); }
static int32_t TC hook_base_init(void *object) { return base_init(object); }
static int32_t TC hook_dialog_init(void *object) { return dialog_init(object); }
static int32_t TC hook_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return init_art(control, bitmap, dib, palette, flags); }
static int32_t TC hook_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                            void *dib, void *palette, uint32_t flags, uint32_t color,
                            uint32_t module, int32_t x, int32_t y, struct fs98_help_rect rect)
{ return init_button(control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void TC hook_init_text(void *control, void *font) { init_text(control, font); }
static void *TC hook_get_control(void *object, uint32_t id) { return get_control(object, id); }
static void SC hook_remap(struct fs98_help_rect *rect, void *control) { remap_rect(rect, control); }
static void TC hook_move(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ move_control(control, x, y, width, height, repaint); }
static void TC hook_init_group(void *control, void *font) { init_group(control, font); }
static void *SC hook_base_font(int32_t id) { return base_font(id); }
static void TC hook_init_list(void *control, void *font, void *label) { init_list(control, font, label); }
static void TC hook_init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                              uint32_t error_color, void *label, uint32_t flags)
{ init_edit(control, font, foreground, background, error_color, label, flags); }
static int32_t SC hook_copy_rect(struct fs98_help_rect *out, const struct fs98_help_rect *in) { return copy_rect(out, in); }
static void SC hook_common_controls(void) { common_controls(); }
static uint32_t SC hook_load_library(const char *name) { return load_library(name); }
static void *SC hook_module_state(void) { return module_state(); }
static uint32_t SC hook_create_window(uint32_t ex_style, const char *class_info, const char *title, uint32_t style,
                                    int32_t x, int32_t y, int32_t width, int32_t height, uint32_t parent,
                                    uint32_t menu, uint32_t instance, void *param)
{ return create_window(ex_style, class_info, title, style, x, y, width, height, parent, menu, instance, param); }
static void TC hook_string_construct(void *string) { string_construct(string); }
static void TC hook_string_destroy(void *string) { string_destroy(string); }
static int32_t SC hook_reg_open(uint32_t key, const char *subkey, uint32_t options, uint32_t access, uint32_t *key_out)
{ return reg_open(key, subkey, options, access, key_out); }
static int32_t SC hook_reg_query(uint32_t key, const char *name, void *reserved, uint32_t *type, void *data, uint32_t *size)
{ return reg_query(key, name, reserved, type, data, size); }
static int32_t SC hook_load_ui_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{ return load_ui_string(module, id, buffer, capacity); }
static void TC hook_set_sims_text(void *control, const char *text) { set_sims_text(control, text); }
static char *SC hook_append_text(char *out, const char *text) { return append_text(out, text); }
static void TC hook_font_construct(void *font, void *base_font) { font_construct(font, base_font); }
static void TC hook_font_destroy(void *font) { font_destroy(font); }
static void *hook_dll_onexit(void (*handler)(void), void **begin, void **end)
{
    if (begin != (void **)((uint8_t *)s_retail + 0x5250) || end != (void **)((uint8_t *)s_retail + 0x524c)) exit(2);
    register_exit(handler); return (void *)handler;
}
static void patch(uint32_t rva, void *target)
{
    DWORD old;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) exit(2);
    *slot = target;
    VirtualProtect(slot, 4, old, &old);
}
static void setup_imports(void)
{
    static void *host[0x2a4 / 4];
    patch(0x6508, hook_send); patch(0x64fc, hook_help); patch(0x6518, hook_enable);
    patch(0x650c, hook_kill); patch(0x6504, hook_timer); patch(0x64f8, hook_destroy);
    patch(0x6300, hook_free); patch(0x6494, hook_close); patch(0x6344, hook_box);
    patch(0x6500, hook_long); patch(0x64e0, hook_checked); patch(0x633c, hook_text);
    patch(0x6340, hook_control); patch(0x64ec, hook_exchange); patch(0x6378, hook_ddx);
    patch(0x6310, hook_read); patch(0x630c, hook_copy); patch(0x6308, hook_open);
    patch(0x62f8, hook_close_handle);
    host[0x2a0 / 4] = hook_load; host[0x188 / 4] = hook_split;
    host[0x184 / 4] = hook_join; host[0x158 / 4] = hook_exists;
    patch(0x5064, host);
    patch(0x6474, hook_base_init); patch(0x632c, hook_dialog_init); patch(0x6480, hook_art);
    patch(0x6484, hook_button); patch(0x6488, hook_init_text); patch(0x6330, hook_get_control);
    patch(0x646c, hook_remap); patch(0x6334, hook_move); patch(0x648c, hook_init_group);
    patch(0x6478, hook_base_font); patch(0x6454, hook_init_list); patch(0x6450, hook_init_edit);
    patch(0x6510, hook_copy_rect); patch(0x62f0, hook_common_controls); patch(0x6304, hook_load_library);
    patch(0x63c8, hook_module_state); patch(0x6514, hook_create_window);
    patch(0x6370, hook_string_construct); patch(0x6374, hook_string_destroy);
    patch(0x62e4, hook_reg_open); patch(0x62e8, hook_reg_query); patch(0x651c, hook_load_ui_string);
    patch(0x6490, hook_set_sims_text); patch(0x6520, format_version); patch(0x62fc, hook_append_text);
    patch(0x647c, hook_font_construct); patch(0x6470, hook_font_destroy); patch(0x6444, hook_dll_onexit);
    fs98_help_set_about_state(&s_fixture.about_state);
    patch(0x51a4, (void *)0x31415926); fs98_help_set_resource_module(0x31415926);
    patch(0x51bc, (void *)0x27182818); fs98_help_set_object_module(0x27182818);
    patch(0x64a0, hook_dialog_construct); patch(0x64b4, hook_dialog_destroy);
#define PATCH_CONTROL(name, ctor, dtor) patch(ctor, hook_construct_##name); patch(dtor, hook_destroy_##name)
    PATCH_CONTROL(text, 0x64a4, 0x64b8); PATCH_CONTROL(group, 0x64a8, 0x64bc);
    PATCH_CONTROL(button, 0x64ac, 0x64c0); PATCH_CONTROL(art, 0x64b0, 0x6498);
    PATCH_CONTROL(list, 0x644c, 0x645c); PATCH_CONTROL(edit, 0x6458, 0x6460);
    PATCH_CONTROL(radio, 0x6468, 0x6464); PATCH_CONTROL(window, 0x6348, 0x634c);
#undef PATCH_CONTROL
    patch(0x64e8, allocate_object); patch(0x649c, ui_delete); patch(0x63e4, mfc_delete);
    patch(0x64f0, hook_dialog_delete); patch(0x63e0, hook_create_dialog); patch(0x63dc, hook_show_dialog);
    {
        void *vtables[5]; uint32_t index;
        HMODULE crt = LoadLibraryA("msvcrt.dll");
        void *handler = (void *)GetProcAddress(crt, "__CxxFrameHandler");
        if (!handler) exit(2);
        patch(0x6440, handler);
        for (index = 0; index < 5; ++index) vtables[index] = (uint8_t *)s_retail + s_classes[index].vtable;
        fs98_help_set_object_services(&s_object_services, vtables, (uint8_t *)s_retail + 0x4350);
    }
}
static void initialize(void)
{
    uint32_t index;
    uint8_t *object = s_fixture.object;
    uint32_t descriptor_offset = s_operation == 4 || s_operation == 5 ? 0x1174 :
        ((s_operation >= 18 && s_operation <= 21) || s_operation == 45) ? 0x10e0 : 0x10dc;
    for (index = 0; index < sizeof(s_fixture); ++index) ((uint8_t *)&s_fixture)[index] = (uint8_t)next_random();
    PTR(object, 0) = s_vtable;
    PTR(object, 0x1694) = PTR(object, 0x1788) = PTR(object, 0x1274) = s_control_vtable;
    D32(object, 0x20) = s_seed;
    PTR(object, descriptor_offset) = s_fixture.descriptor[0];
    for (index = 0; index < 2; ++index) {
        uint8_t *descriptor = s_fixture.descriptor[index];
        D32(descriptor, 8) = s_scenario % 5;
        D32(descriptor, 0xc) = s_scenario % 6 - 1;
        PTR(descriptor, 0x10) = s_fixture.records;
        D32(descriptor, 0x14) = s_scenario % 3;
        if (s_operation == 2) PTR(descriptor, 0x10) = about_callback;
        if ((s_operation >= 18 && s_operation <= 21) || s_operation == 45) PTR(descriptor, 8) = graph_callback;
    }
    for (index = 0; index < 4; ++index) {
        snprintf((char *)s_fixture.records[index], 64, "Lesson %u vector %u", index, s_scenario);
        snprintf((char *)s_fixture.records[index] + 0x100, 64, "Description %u seed %x", index, s_seed);
        snprintf((char *)s_fixture.records[index] + 0x900, 64, "C:\\lessons\\lesson%u.adv", index);
    }
    if (s_operation == 8) {
        static const uint32_t limits[] = {0x80000000, 0xffffffff, 0, 1, 5, 0x7fffffff};
        D32(object, 0x10f0) = limits[(s_scenario / 6) % 6];
        D32(object, 0x10f4) = s_scenario & 1 ? 0xffffffff : s_seed;
    }
    if (s_operation == 3) {
        if (!(s_scenario & 1)) D32(object, 0x10e8) = 0;
        if (!(s_scenario & 2)) D32(object, 0x10ec) = 0;
        if (!(s_scenario & 4)) D32(object, 0x110c) = 0;
    }
    if (s_operation == 9) D32(object, 0x1108) = s_scenario % 4;
    if ((s_operation == 15 || s_operation == 47) && s_scenario % 3 == 0) PTR(object, 0x10dc) = 0;
    if ((s_operation == 21 || s_operation == 45) && s_scenario % 3 != 0)
        PTR(object, 0x10e0) = s_scenario % 3 == 1 ? 0 : (void *)0xcdcdcdcd;
    s_initial = s_fixture;
}
static LONG WINAPI fatal(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT scenario=%u op=%u (%s) side=%d code=%lx address=%p\n", s_scenario,
        s_operation, s_operations[s_operation].name, s_side,
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    fflush(stderr); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static void land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_jump, 1);
}
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    if (s_operations[s_operation].signature == 11 && s_scenario % 11 == 0 &&
        record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        s_throw_at = 0;
        put("allocation.fault access=%u address=%u\n", (uint32_t)record->ExceptionInformation[0],
            (uint32_t)record->ExceptionInformation[1]);
        if (!s_side) ++s_allocation_faults;
        exception->ContextRecord->Eip = (DWORD)land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static EXCEPTION_DISPOSITION __cdecl catch_throw(EXCEPTION_RECORD *record, void *frame,
                                                 CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == TEST_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, 0, record, 0);
        s_throw_at = 0; put("CAUGHT\n"); if (!s_side) ++s_throws;
        land();
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) int32_t run(uint32_t throw_at)
{
    struct { void *previous; void *handler; } catcher;
    void *entry = (uint8_t *)s_retail + s_operations[s_operation].rva;
    void *object = s_fixture.object;
    volatile int32_t result = 0;
    uint32_t argument;
    s_fixture = s_initial;
    memcpy((uint8_t *)s_retail + 0x51c0, &s_fixture.about_state, sizeof(s_fixture.about_state));
    s_log_length = s_calls = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_saved_seh; catcher.handler = catch_throw;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        switch (s_operations[s_operation].signature) {
        case 0:
            if (s_operation == 24 && s_scenario % 2 == 0) object = 0;
            (s_side ? s_operations[s_operation].event : (event_fn)entry)(object);
            break;
        case 1: case 2: case 3: {
            uint32_t kind = (s_operation - 30) / 3;
            uint32_t which = (s_operation - 30) % 3;
            static void *ours[5][3] = {
#define OUR_CLASS(name) {fs98_help_##name##_help, fs98_help_##name##_context, fs98_help_##name##_exchange}
                OUR_CLASS(about), OUR_CLASS(credits), OUR_CLASS(lessons), OUR_CLASS(graph), OUR_CLASS(completion)
#undef OUR_CLASS
            };
            if (s_side) entry = ours[kind][which];
            if (which == 0)
                result = ((int32_t (TC *)(void *, uint32_t, void *))entry)(object, s_seed, s_fixture.help);
            else if (which == 1)
                result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 1);
            else ((void (TC *)(void *, void *))entry)(object, s_fixture.exchange);
            break;
        }
        case 4:
            if (s_side) {
                if (s_operation == 7) entry = fs98_help_credits_start_timer;
                if (s_operation == 14) entry = fs98_help_lessons_no_error;
                if (s_operation == 15) entry = fs98_help_lessons_validate;
                if (s_operation == 21) entry = fs98_help_graph_validate;
                if (s_operation == 45) entry = fs98_help_graph_init;
                if (s_operation == 46) entry = fs98_help_completion_init;
                if (s_operation == 47) entry = fs98_help_lessons_init;
                if (s_operation == 48) entry = fs98_help_credits_init;
            }
            result = ((int32_t (TC *)(void *))entry)(object); break;
        case 5:
            argument = s_scenario & 1 ? D32(object, 0x10e8) : D32(object, 0x10e8) ^ 1;
            if (s_side) entry = s_operation == 8 ? (void *)fs98_help_credits_timer : (void *)fs98_help_empty_arg;
            ((void (TC *)(void *, uint32_t))entry)(object, argument); break;
        case 6:
            if (s_side) entry = fs98_help_graph_tick;
            result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 1); break;
        case 7:
            if (s_side) entry = fs98_help_credits_read;
            result = ((int32_t (SC *)(uint32_t, void *, int32_t, uint32_t *))entry)(s_seed,
                s_fixture.buffer, s_scenario % 65, &s_fixture.read); break;
        case 8: case 9: case 10: case 11: case 12: case 13: {
            uint32_t kind = (s_operation - 49) / 6;
            uint32_t which = (s_operation - 49) % 6;
            void *returned = 0;
            static void *ours[5][4] = {
#define OUR_OBJECT(name) {fs98_help_##name##_construct, fs98_help_##name##_destroy, fs98_help_##name##_delete, fs98_help_##name##_create}
                OUR_OBJECT(about), OUR_OBJECT(credits), OUR_OBJECT(lessons), OUR_OBJECT(graph), OUR_OBJECT(completion)
#undef OUR_OBJECT
            };
            const struct fs98_help_class *class_info = &s_classes[kind];
            if (s_side) entry = ours[kind][which < 4 ? which : 0];
            if (which == 0 || which >= 4)
                returned = ((void *(TC *)(void *, void *))entry)(object, (void *)s_seed);
            else if (which == 1) ((void (TC *)(void *))entry)(object);
            else if (which == 2) returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
            else result = ((int32_t (SC *)(uint32_t, void *))entry)(s_seed, s_fixture.descriptor[0]);
            if (which >= 4) {
                entry = s_side ? ours[kind][which == 4 ? 1 : 2] :
                    (uint8_t *)s_retail + (which == 4 ? class_info->destructor : class_info->delete);
                if (which == 4) ((void (TC *)(void *))entry)(object);
                else returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
            }
            if (returned) put("return.object %u\n", offset(returned));
            break;
        }
        case 14:
            if (s_side) entry = fs98_help_placeholder_delete;
            put("return.object %u\n", offset(((void *(TC *)(void *, uint32_t))entry)(
                s_fixture.object + 0x1274, s_seed)));
            break;
        case 15:
            if (s_side) entry = fs98_help_class_delete;
            ((void (SC *)(void *))entry)(object); break;
        case 16:
            if (s_side) entry = fs98_help_about_populate;
            ((event_fn)entry)(object); break;
        case 17:
            if (s_side) entry = fs98_help_about_font_destroy;
            ((void (*)(void))entry)(); break;
        case 18: case 19:
            if (s_side) entry = fs98_help_about_init;
            result = ((int32_t (TC *)(void *))entry)(object);
            if (s_operations[s_operation].signature == 19)
                result = ((int32_t (TC *)(void *))entry)(object);
            break;
        }
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    if (!s_side) memcpy(&s_fixture.about_state, (uint8_t *)s_retail + 0x51c0, sizeof(s_fixture.about_state));
    /* A persistent function pointer is compared by ownership, then normalized.
     * All other bytes, including the embedded descriptor/name, stay exact. */
    if ((s_operation == 6 || s_operation == 48) && PTR(s_fixture.object, 0x10e4) ==
        (s_side ? (void *)fs98_help_credits_read : (uint8_t *)s_retail + 0x1fe0))
        PTR(s_fixture.object, 0x10e4) = (void *)0xfeed1234;
    if (s_calls > s_max_calls) s_max_calls = s_calls;
    return result;
}
static int compare(uint32_t throw_at)
{
    int32_t retail, ours;
    uint32_t offset;
    s_side = 0; retail = run(throw_at); s_retail_result = s_fixture;
    s_side = 1; ours = run(throw_at);
    ++s_comparisons;
    if (retail == ours && !memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) &&
        !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s) throw=%u seed=%x script=%u returns=%d/%d\n",
        s_scenario, s_operation, s_operations[s_operation].name, throw_at, s_seed, s_script, retail, ours);
    for (offset = 0; offset < sizeof(s_fixture); ++offset)
        if (((uint8_t *)&s_retail_result)[offset] != ((uint8_t *)&s_fixture)[offset]) {
            fprintf(stderr, "byte offset=%x retail=%02x ours=%02x\n", offset,
                ((uint8_t *)&s_retail_result)[offset], ((uint8_t *)&s_fixture)[offset]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], 0, 0) : 20000;
    uint32_t initial_seed = argc > 2 ? strtoul(argv[2], 0, 0) : 0x19a847e3;
    int unwind = argc > 3 && !strcmp(argv[3], "unwind");
    uint32_t throw, limit;
    if (!count || !initial_seed) return 2;
    s_rng = initial_seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal);
    AddVectoredExceptionHandler(1, expected_fault);
    s_retail = LoadLibraryExA("orig/English/Help.DLL", 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x5078)) return 2;
    setup_imports(); fs98_help_set_services(&s_services);
    s_vtable[0xec / 4] = virtual_close; s_other_vtable[0xec / 4] = virtual_close_other;
    s_vtable[0xcc / 4] = virtual_accept; s_other_vtable[0xcc / 4] = virtual_accept_other;
    s_vtable[0xd0 / 4] = virtual_cancel; s_other_vtable[0xd0 / 4] = virtual_cancel_other;
    s_vtable[1] = s_other_vtable[1] = virtual_delete;
    s_vtable[0xe4 / 4] = s_other_vtable[0xe4 / 4] = virtual_skin;
    s_control_vtable[0xc4 / 4] = virtual_radio; s_control_vtable[0x60 / 4] = placeholder_destroy;
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        s_seed = next_random(); s_script = s_scenario & 15;
        for (s_operation = 0; s_operation < OP_COUNT; ++s_operation) {
            initialize();
            if (compare(0)) return 1;
            limit = s_calls;
            if (unwind) for (throw = 1; throw <= limit; ++throw)
                if (compare(throw)) return 1;
        }
    }
    printf("Help events: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x swaps=%u reads=%u resets=%u throws=%u max_calls=%u allocation_faults=%u\n",
        count, (uint32_t)OP_COUNT, s_comparisons, initial_seed, s_swaps, s_reads, s_resets, s_throws, s_max_calls, s_allocation_faults);
    FreeLibrary(s_retail); return 0;
}
