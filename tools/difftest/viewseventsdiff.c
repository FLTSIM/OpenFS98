/* Views event boundary: original PE32 routines versus C over the same object,
 * descriptor, records, numeric buffers and side-effecting dependency scripts.
 * The native MFC creation/rendering boundary is a separate acceptance task. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/views/views_internal.h"
#include "../../src/views/views_objects.h"
#include "../../src/views/views_unwind.h"
#include "../../src/views/views_objects_tables.inc"

#define TC FS98_VIEWS_THISCALL
#define SC FS98_VIEWS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define RECORDS 4
#define LOG_SIZE 65536

struct fixture {
    uint8_t guard_before[16];
    uint8_t object[0x2808];
    uint8_t descriptors[2][0x28];
    uint8_t records[RECORDS][0xb4];
    char buffers[RECORDS][7][64];
    uint8_t help_info[32];
    uint8_t preview_control[64];
    uint8_t guard_after[16];
};
static struct fixture s_fixture, s_initial, s_retail_result;
static HMODULE s_retail, s_mfc;
static void *s_ftol;
static void *s_fault_jump[5];
static uint32_t s_saved_seh, s_allocation_faults;
static volatile int s_running;
static uint32_t s_throw_position, s_throw_at, s_dependency_calls, s_throws;
static uint16_t s_control_word, s_final_control_word;
static uint32_t s_malformed_zoom_hits, s_invalid_units_hits;
extern void fs98_views_test_call(void *entry, void *object, uint32_t legacy_bits);
#define TEST_THROW 0x0e041a21
static uint32_t s_rng, s_seed, s_scenario, s_operation, s_script, s_calls;
static uint32_t s_callback_hits, s_selection_changes, s_error_hits, s_text_hits, s_map_tail_hits;
static int s_side;
static void *s_vtable[60], *s_other_vtable[60], *s_control_vtable[60];
static char s_logs[2][LOG_SIZE];
static uint32_t s_log_length;

typedef void (TC *event_fn)(void *object);
typedef int32_t (TC *arg_fn)(void *object, uint32_t argument);
typedef int32_t (TC *message_fn)(void *object, uint32_t first, uint32_t second);
static const uint32_t s_entries[] = {
    0x34c0, 0x3500, 0x3540, 0x3580, 0x35b0, 0x35d0, 0x35f0,
    0x1ec0, 0x1ee0, 0x1f00, 0x1f20, 0x21a0, 0x3600, 0x36f0,
    0x3a50, 0x3a60, 0x3a40, 0x24f0, 0x26f0, 0x28f0, 0x2d50, 0x2ad0, 0x1920, 0x1940, 0x1980, 0x1690, 0x2f30, 0x22b0, 0x3830, 0x38e0, 0x1f40, 0x1990, 0x11d0, 0x1450, 0x1430, 0x3a70, 0x11c0, 0x1130, 0x11d0, 0x11d0
};
static const event_fn s_events[] = {
    fs98_views_direction, fs98_views_transition, fs98_views_altitude_reference,
    fs98_views_axis, fs98_views_fullscreen, fs98_views_gradual, fs98_views_titles,
    0, 0, 0, fs98_views_close, fs98_views_sync, 0, 0,
    fs98_views_disable, fs98_views_enable, 0, fs98_views_cockpit,
    fs98_views_virtual_cockpit, fs98_views_tower, fs98_views_track, fs98_views_spot,
    fs98_views_cancel, fs98_views_accept, fs98_views_post_destroy, 0,
    fs98_views_map, fs98_views_select, fs98_views_zoom, 0, fs98_views_populate, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static uint32_t next_random(void)
{
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static uint32_t offset(const void *pointer)
{ return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture); }
static void put(const char *format, ...)
{
    va_list args;
    int length;
    if (s_log_length > LOG_SIZE - 1024) exit(2);
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_log_length, LOG_SIZE - s_log_length, format, args);
    va_end(args);
    if (length < 0) exit(2);
    s_log_length += (uint32_t)length;
    s_dependency_calls++;
    if (s_throw_at && s_dependency_calls == s_throw_at) RaiseException(TEST_THROW, 0, 0, 0);
}
static void change_selection(void)
{
    if (s_script & 4) PTR(s_fixture.object, 0) = s_other_vtable;
    void *descriptor = PTR(s_fixture.object, 0x2804);
    if (s_script & 1) {
        D32(descriptor, 0x14) = (D32(descriptor, 0x14) + 1) % RECORDS;
        if (!s_side) s_selection_changes++;
    }
    if (s_script & 2)
        PTR(s_fixture.object, 0x2804) = descriptor == s_fixture.descriptors[0] ?
            s_fixture.descriptors[1] : s_fixture.descriptors[0];
}
static int32_t SC preview(uint32_t operation, void *descriptor, uint32_t window)
{
    put("preview %u descriptor=%u window=%u selected=%u\n", operation, offset(descriptor),
         window, D32(descriptor, 0x14));
    change_selection();
    if (!s_side) s_callback_hits++;
    return (int32_t)s_seed;
}
static int32_t SC preview_alternate(uint32_t operation, void *descriptor, uint32_t window)
{ put("preview.alternate\n"); return preview(operation, descriptor, window); }
static int32_t SC validate(void *descriptor)
{
    put("validate descriptor=%u selected=%u\n", offset(descriptor), D32(descriptor, 0x14));
    change_selection();
    if (!s_side) s_callback_hits++;
    return (int32_t)s_seed;
}
static int32_t send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{
    if (message == 0x143)
        put("message %u %u %u string=%s\n", window, message, w_param, (const char *)l_param);
    else put("message %u %u %u %u\n", window, message, w_param, l_param);
    if (message == 0x147 && window == D32(s_fixture.object, 0x1494)) {
        change_selection();
        return (int32_t)((s_scenario + 1) % RECORDS);
    }
    change_selection();
    return s_scenario % 3 ? (int32_t)(s_seed & 7) : -1;
}
static int32_t SC native_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return send_message(window, message, w_param, l_param); }
static int32_t help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{
    uint32_t index, hash = 2166136261u;
    for (index = 0; index < 128; index += 2) {
        hash = (hash ^ table[index]) * 16777619u;
        hash = (hash ^ table[index + 1]) * 16777619u;
        if (!table[index] && !table[index + 1]) break;
    }
    if (index == 128) exit(2);
    put("help %u %s %u entries=%u hash=%08x\n", window, file, command, index / 2, hash);
    return (int32_t)(s_seed & 1);
}
static int32_t SC native_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{ return help(window, file, command, table); }
static void close_dialog(void *object)
{
    put("close object=%u descriptor=%u\n", offset(object), offset(PTR(object, 0x2804)));
    D32(object, 0x10d0) ^= 0x11335577;
}
static void TC native_close(void *object) { close_dialog(object); }
static int32_t get_text(void *control, char *buffer, int32_t capacity)
{
    int length;
    put("get.text control=%u buffer=%u capacity=%d\n", offset(control), offset(buffer), capacity);
    if (capacity < 0 || capacity > 64 || offset(buffer) < offsetof(struct fixture, buffers) ||
        offset(buffer) >= offsetof(struct fixture, help_info)) exit(2);
    length = capacity ? snprintf(buffer, (size_t)capacity, "%u.%u", s_seed & 255, s_calls++) : 0;
    change_selection();
    if (!s_side) s_text_hits++;
    return length;
}
static int32_t TC native_get_text(void *control, char *buffer, int32_t capacity)
{ return get_text(control, buffer, capacity); }
static int32_t error_dialog(void *object, void *control, void *descriptor)
{
    put("error.dialog object=%u control=%u descriptor=%u error=%u\n",
         offset(object), offset(control), offset(descriptor), D32(descriptor, 0));
    change_selection();
    if (!s_side) s_error_hits++;
    return s_scenario & 1 ? (int32_t)s_seed | 1 : 0;
}
static int32_t TC native_error(void *object, void *control, void *descriptor)
{ return error_dialog(object, control, descriptor); }
static int32_t TC finish_error(void *object, uint32_t control)
{
    put("error.virtual object=%u control=%u\n", offset(object), control);
    return (int32_t)(s_seed ^ control);
}
static int32_t enable_window(uint32_t window, int32_t enabled)
{ put("enable.window %u %d\n", window, enabled); return (int32_t)s_seed; }
static int32_t SC native_enable(uint32_t window, int32_t enabled)
{ return enable_window(window, enabled); }
static void *get_control(void *object, uint32_t id)
{
    put("get.control %u %u\n", offset(object), id);
    change_selection();
    return s_fixture.preview_control;
}
static void *TC native_get_control(void *object, uint32_t id) { return get_control(object, id); }
static int32_t enable_control(void *control, int32_t enabled)
{
    put("enable.control %u %d\n", offset(control), enabled);
    D32(control, 0x30) = (uint32_t)enabled;
    change_selection();
    return (int32_t)s_seed;
}
static int32_t TC native_enable_control(void *control, int32_t enabled)
{ return enable_control(control, enabled); }
static int32_t show_control(void *control, int32_t show)
{
    put("show.control %u %d\n", offset(control), show);
    D32(control, 0x34) = (uint32_t)show;
    change_selection();
    return (int32_t)s_seed;
}
static void set_text(void *control, const char *text)
{
    put("set.text %u %s\n", offset(control), text);
    D32(control, 0x38) = (uint32_t)strlen(text);
    change_selection();
}
static void TC native_set_text(void *control, const char *text) { set_text(control, text); }
static int32_t set_checked(void *control, int32_t checked)
{
    int32_t previous = (int32_t)D32(control, 0xbc);
    put("set.checked %u %d\n", offset(control), checked);
    D32(control, 0xbc) = (uint32_t)checked;
    change_selection();
    return previous;
}
static int32_t TC native_check(void *control, int32_t checked) { return set_checked(control, checked); }
static void ddx_control(void *exchange, uint32_t id, void *control)
{
    put("ddx exchange=%u id=%u control=%u\n", offset(exchange), id, offset(control));
    D32(control, 0x20) = id + 0x10000;
    change_selection();
}
static void SC native_ddx(void *exchange, uint32_t id, void *control)
{ ddx_control(exchange, id, control); }
static int32_t TC bridge_validate(void *object, uint32_t argument)
{
    return s_side ? fs98_views_validate(object, argument) :
        ((arg_fn)((uint8_t *)s_retail + 0x3600))(object, argument);
}
static int32_t TC bridge_error(void *object, uint32_t argument)
{
    return s_side ? fs98_views_report_error(object, (int32_t)argument) :
        ((arg_fn)((uint8_t *)s_retail + 0x36f0))(object, argument);
}
static void TC bridge_close(void *object)
{
    put("close.virtual original\n");
    if (s_side) fs98_views_close(object);
    else ((event_fn)((uint8_t *)s_retail + 0x1f20))(object);
}
static void TC other_close(void *object)
{ put("close.virtual replaced\n"); bridge_close(object); }
static void *TC delete_object(void *object, uint32_t flags)
{
    put("delete.virtual object=%u flags=%u\n", offset(object), flags);
    return object;
}
static void *mfc(uint32_t ordinal)
{
    void *function = (void *)GetProcAddress(s_mfc, (const char *)ordinal);
    if (!function) exit(2);
    return function;
}
static const char *string_text(const void *string) { return PTR(string, 0); }
static void string_construct(void *string)
{
    put("string.construct\n");
    ((void (TC *)(void *))mfc(540))(string);
}
static void TC native_string_construct(void *string) { string_construct(string); }
static void string_destroy(void *string)
{
    put("string.destroy %s\n", string_text(string));
    ((void (TC *)(void *))mfc(800))(string);
}
static void TC native_string_destroy(void *string) { string_destroy(string); }
static char *string_buffer(void *string, int32_t capacity)
{
    put("string.buffer %s capacity=%d\n", string_text(string), capacity);
    return ((char *(TC *)(void *, int32_t))mfc(2915))(string, capacity);
}
static char *TC native_string_buffer(void *string, int32_t capacity) { return string_buffer(string, capacity); }
static void string_release(void *string, int32_t length)
{
    put("string.release length=%d text=%s\n", length, string_text(string));
    ((void (TC *)(void *, int32_t))mfc(5572))(string, length);
}
static void TC native_string_release(void *string, int32_t length) { string_release(string, length); }
static int32_t load_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{
    const char *text;
    switch (id) {
    case 0xf870: text = s_scenario % 7 ? "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,cloud,water,feet" : "missing separators"; break;
    case 0xf87d: text = "1,2,3,4,5,6,7,8,9,10,11,12,ground,metres"; break;
    case 0xf881: text = "feet"; break;
    case 0xf883: text = "metres"; break;
    case 0xf884: text = s_scenario % 3 ? "ground,sea,absolute" : ""; break;
    case 0xf882: text = "AGL,MSL"; break;
    default: exit(2);
    }
    put("load.string module=%u id=%u capacity=%d text=%s\n", module, id, capacity, text);
    if (capacity <= (int32_t)strlen(text)) exit(2);
    strcpy(buffer, text);
    change_selection();
    return (int32_t)strlen(text);
}
static int32_t SC native_load_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{ return load_string(module, id, buffer, capacity); }
static int32_t string_find_char(void *string, uint32_t character)
{
    put("string.find.char text=%s char=%u\n", string_text(string), character);
    int32_t result = ((int32_t (TC *)(void *, uint32_t))mfc(2763))(string, character);
    if (!s_side && result != -1 && (string_text(string)[0] == 'c' || string_text(string)[0] == 'w' || string_text(string)[0] == 'g'))
        s_map_tail_hits++;
    return result;
}
static int32_t TC native_find_char(void *string, uint32_t character) { return string_find_char(string, character); }
static int32_t string_find_text(void *string, const char *text)
{
    put("string.find.text text=%s needle=%s\n", string_text(string), text);
    return ((int32_t (TC *)(void *, const char *))mfc(2764))(string, text);
}
static int32_t TC native_find_text(void *string, const char *text) { return string_find_text(string, text); }
static void *string_mid(void *string, void *result, int32_t start, int32_t count)
{
    put("string.mid text=%s start=%d count=%d\n", string_text(string), start, count);
    return ((void *(TC *)(void *, void *, int32_t, int32_t))mfc(4278))(string, result, start, count);
}
static void *TC native_mid(void *string, void *result, int32_t start, int32_t count)
{ return string_mid(string, result, start, count); }
static void *string_tail(void *string, void *result, int32_t start)
{
    put("string.tail text=%s start=%d\n", string_text(string), start);
    return ((void *(TC *)(void *, void *, int32_t))mfc(4277))(string, result, start);
}
static void *TC native_tail(void *string, void *result, int32_t start) { return string_tail(string, result, start); }
static void string_assign(void *string, const void *source)
{
    put("string.assign %s = %s\n", string_text(string), string_text(source));
    ((void *(TC *)(void *, const void *))mfc(858))(string, source);
}
static void *TC native_assign(void *string, const void *source) { string_assign(string, source); return string; }
static void string_append(void *string, const void *source)
{
    put("string.append %s += %s\n", string_text(string), string_text(source));
    ((void *(TC *)(void *, const void *))mfc(939))(string, source);
}
static void *TC native_append(void *string, const void *source) { string_append(string, source); return string; }
static void string_append_text(void *string, const char *text)
{
    put("string.append.text %s += %s\n", string_text(string), text);
    ((void *(TC *)(void *, const char *))mfc(941))(string, text);
}
static void *TC native_append_text(void *string, const char *text) { string_append_text(string, text); return string; }
static void string_assign_text(void *string, const char *text)
{
    put("string.assign.text %s = %s\n", string_text(string), text);
    ((void *(TC *)(void *, const char *))mfc(860))(string, text);
}
static void *TC native_assign_text(void *string, const char *text) { string_assign_text(string, text); return string; }
static void get_text_string(void *control, void *string)
{
    static const char *s_inputs[] = {"1.234", "0", "-0.005", "6553.599", "nan", "inf", "1e30", "-1.25", ".51", "42.9", "1.2.3", "9999.12345"};
    static const char *s_extra[] = {"", "invalid", "1,234", ".123"};
    const char *text = s_operation == 29 && s_scenario % 3 == 0 ? s_extra[s_scenario % 4] : s_inputs[s_scenario % 12];
    if (s_operation == 28 && s_scenario % 7 == 0) {
        text = s_extra[s_scenario % 2];
        if (!s_side) s_malformed_zoom_hits++;
    }
    put("get.text.string %u text=%s\n", offset(control), text);
    ((void *(TC *)(void *, const char *))mfc(860))(string, text);
    change_selection();
}
static void TC native_get_text_string(void *control, void *string) { get_text_string(control, string); }
static void string_format(void *string, const char *format, double value)
{
    put("string.format %s format=%s value=%.17g\n", string_text(string), format, value);
    ((void (*)(void *, const char *, ...))mfc(2818))(string, format, value);
}
static int32_t scan_float(const char *text, float *value)
{
    put("scan.float %s\n", text);
    return sscanf(text, "%f", value);
}
static int32_t native_scan_float(const char *text, const char *format, float *value)
{
    if (strcmp(format, "%f")) exit(2);
    return scan_float(text, value);
}
static int64_t float_to_integer(long double value)
{
    uint32_t low, high;
    __asm__ volatile("fldt %3\n\tcall *%2" : "=a"(low), "=d"(high) : "r"(s_ftol), "m"(value) : "ecx", "cc", "memory", "st");
    return (int64_t)(((uint64_t)high << 32) | low);
}
static int32_t set_radio(void *control, int32_t checked)
{
    put("set.radio %u %d\n", offset(control), checked);
    D32(control, 0xec) = (uint32_t)checked;
    change_selection();
    return (int32_t)s_seed;
}
static int32_t TC native_radio(void *control, int32_t checked) { return set_radio(control, checked); }
static int32_t compare_text(const char *left, const char *right)
{
    put("compare.text %s / %s\n", left, right);
    return ((int32_t (*)(const char *, const char *))GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_mbscmp"))(left, right);
}
static int32_t base_init(void *object)
{
    put("base.init %u\n", offset(object));
    if (s_side) fs98_views_exchange(object, s_fixture.help_info);
    else ((void (TC *)(void *, void *))((uint8_t *)s_retail + 0x1690))(object, s_fixture.help_info);
    change_selection();
    return (int32_t)s_seed;
}
static int32_t TC native_base_init(void *object) { return base_init(object); }
static int32_t TC dialog_init(void *object, int32_t id, uint32_t module, uint32_t flags)
{
    put("dialog.init %u %d %u %u\n", offset(object), id, module, flags);
    change_selection();
    return s_scenario % 9 != 0;
}
static int32_t init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{
    put("init.art %u %d %u %u %u style=%u\n", offset(control), bitmap, offset(dib), offset(palette), flags, D32(s_fixture.object, 0x84));
    D32(control, 0x28) = flags;
    change_selection();
    return (int32_t)s_seed;
}
static int32_t TC native_init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return init_art(control, bitmap, dib, palette, flags); }
static int32_t init_button(void *control, void *font, void *alternate, uint32_t bitmap,
                           void *dib, void *palette, uint32_t flags, uint32_t color,
                           uint32_t module, int32_t x, int32_t y, struct fs98_views_rect rect)
{
    put("init.button %u font=%u alternate=%u bitmap=%u dib=%u palette=%u flags=%u color=%u module=%u xy=%d,%d rect=%d,%d,%d,%d\n",
         offset(control), offset(font), offset(alternate), bitmap, offset(dib), offset(palette), flags, color, module,
         x, y, rect.left, rect.top, rect.right, rect.bottom);
    D32(control, 0x28) = bitmap;
    change_selection();
    return (int32_t)s_seed;
}
static int32_t TC native_init_button(void *control, void *font, void *alternate, uint32_t bitmap,
                                    void *dib, void *palette, uint32_t flags, uint32_t color,
                                    uint32_t module, int32_t x, int32_t y, struct fs98_views_rect rect)
{ return init_button(control, font, alternate, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void init_text(void *control, void *font)
{
    put("init.text %u font=%u\n", offset(control), offset(font));
    D32(control, 0x28) = offset(font);
    change_selection();
}
static void TC native_init_text(void *control, void *font) { init_text(control, font); }
static void init_combo(void *control, void *font, void *label)
{
    put("init.combo %u font=%u label=%u\n", offset(control), offset(font), offset(label));
    D32(control, 0x28) = offset(label);
    change_selection();
}
static void TC native_init_combo(void *control, void *font, void *label) { init_combo(control, font, label); }
static void init_group(void *control, void *font)
{
    put("init.group %u font=%u\n", offset(control), offset(font));
    D32(control, 0x28) = offset(font);
    change_selection();
}
static void TC native_init_group(void *control, void *font) { init_group(control, font); }
static void remap_rect(struct fs98_views_rect *rect, void *control)
{
    put("remap.rect %u\n", offset(control));
    rect->left = (int32_t)s_seed;
    rect->top = (int32_t)~s_seed;
    rect->right = (int32_t)(s_seed + (s_scenario % 3 ? 100 : 0));
    rect->bottom = (int32_t)(~s_seed + 99);
    change_selection();
}
static void SC native_remap(struct fs98_views_rect *rect, void *control) { remap_rect(rect, control); }
static void move_control(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{
    put("move.control %u xy=%d,%d size=%d,%d repaint=%d\n", offset(control), x, y, width, height, repaint);
    D32(control, 0x28) = (uint32_t)width;
    change_selection();
}
static void TC native_move(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ move_control(control, x, y, width, height, repaint); }
static uint32_t get_dlg_item(uint32_t window, uint32_t id)
{
    put("get.dlg.item %u %u\n", window, id);
    change_selection();
    return 0x70000 + id;
}
static uint32_t SC native_dlg_item(uint32_t window, uint32_t id) { return get_dlg_item(window, id); }
static void *base_font(int32_t id)
{ put("base.font %d\n", id); return (void *)(0x60000000 + (uint32_t)id); }
static void *SC native_base_font(int32_t id) { return base_font(id); }
static void init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags)
{
    put("init.edit %u font=%u colors=%u,%u,%u label=%u flags=%u\n",
         offset(control), (uint32_t)font, foreground, background, error_color, offset(label), flags);
    D32(control, 0x28) = flags;
    change_selection();
}
static void TC native_init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                               uint32_t error_color, void *label, uint32_t flags)
{ init_edit(control, font, foreground, background, error_color, label, flags); }
static int32_t TC init_check(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{
    put("init.check %u font=%u bitmap=%u dib=%u palette=%u module=%u\n",
         offset(control), offset(font), bitmap, offset(dib), offset(palette), module);
    D32(control, 0x28) = bitmap;
    change_selection();
    return (int32_t)s_seed;
}
static const struct fs98_views_services s_services = {
    send_message, help, close_dialog, get_text, error_dialog, enable_window,
    get_control, enable_control, show_control, set_text, set_checked, ddx_control,
    string_construct, string_destroy, string_buffer, string_release, load_string,
    string_find_char, string_find_text, string_mid, string_tail, string_assign,
    string_append, string_append_text, get_text_string, string_format, string_assign_text,
    scan_float, float_to_integer, set_radio,
    compare_text, base_init, init_art, init_button, init_text, init_combo, init_group, remap_rect,
    move_control, get_dlg_item, base_font, init_edit
};
static uint32_t control_size(uint32_t slot)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_subobjects) / sizeof(s_subobjects[0]); index++)
        if (s_subobjects[index].constructor == slot || s_subobjects[index].destructor == slot)
            return s_subobjects[index].size;
    exit(2);
}
static void construct_control(void *object, uint32_t slot)
{
    put("construct %u slot=%x size=%u\n", offset(object), slot, control_size(slot));
    memset(object, (uint8_t)slot, control_size(slot));
}
static void destroy_control(void *object, uint32_t slot)
{
    put("destroy %u slot=%x size=%u vtable=%u\n", offset(object), slot, control_size(slot),
         PTR(object, 0) == s_control_vtable);
    memset(object, (uint8_t)slot, control_size(slot));
}
#define CONTROL(name, constructor, destructor) \
    static void *TC construct_##name(void *object) { construct_control(object, constructor); return object; } \
    static void TC destroy_##name(void *object) { destroy_control(object, destructor); }
CONTROL(art, 0x743c, 0x7464)
CONTROL(button, 0x7440, 0x7468)
CONTROL(text, 0x7444, 0x746c)
CONTROL(combo, 0x7448, 0x7470)
CONTROL(group, 0x74a8, 0x7474)
CONTROL(radio, 0x74ac, 0x7478)
CONTROL(check, 0x74b0, 0x747c)
CONTROL(edit, 0x7458, 0x7480)
CONTROL(spin, 0x7318, 0x7314)
#undef CONTROL
static void *construct_dialog(void *object, int32_t resource, void *parent)
{
    put("construct.dialog %u resource=%d parent=%u\n", offset(object), resource, (uint32_t)parent);
    memset(object, 0xa5, 0x10dc);
    return object;
}
static void *TC native_construct_dialog(void *object, int32_t resource, void *parent)
{ return construct_dialog(object, resource, parent); }
static void destroy_dialog(void *object)
{ put("destroy.dialog %u\n", offset(object)); memset(object, 0xd0, 0x10dc); }
static void TC native_destroy_dialog(void *object) { destroy_dialog(object); }
static void *allocate_object(uint32_t size)
{
    put("allocate %u failed=%u\n", size, s_scenario % 11 == 0);
    return s_scenario % 11 == 0 ? 0 : s_fixture.object;
}
static void ui_delete(void *object) { put("ui.delete %u\n", offset(object)); }
static void mfc_delete(void *object) { put("mfc.delete %u\n", offset(object)); }
static void dialog_delete(void *object) { put("dialog.delete %u\n", offset(object)); ui_delete(object); }
static void SC native_dialog_delete(void *object) { dialog_delete(object); }
static int32_t create_window(void *object, uint32_t resource, void *parent)
{
    put("create object=%u resource=%u parent=%u descriptor=%u module=%u\n", offset(object), resource,
         (uint32_t)parent, offset(PTR(object, 0x2804)), D32(object, 0x80));
    return (int32_t)(s_scenario % 3);
}
static int32_t TC native_create_window(void *object, uint32_t resource, void *parent)
{ return create_window(object, resource, parent); }
static int32_t show_window(void *object, int32_t show)
{
    put("show.window object=%u show=%d\n", offset(object), show);
    D32(object, 0x20) = s_seed;
    return (int32_t)(s_scenario & 1);
}
static int32_t TC dispatch_show(void *object, int32_t show)
{ return s_operation >= 32 ? show_window(object, show) : show_control(object, show); }
static const struct fs98_views_object_services s_object_services = {
    construct_dialog, destroy_dialog, construct_control, destroy_control,
    allocate_object, ui_delete, mfc_delete, dialog_delete, create_window, show_window
};
static void patch(uint32_t rva, void *function)
{
    DWORD previous;
    void **slot = (void *)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &previous)) exit(2);
    *slot = function;
    VirtualProtect(slot, 4, previous, &previous);
}
static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "VIEWS FAULT side=%d scenario=%u operation=%u code=%08lx pc=%p\n",
             s_side, s_scenario, s_operation, exception->ExceptionRecord->ExceptionCode,
             exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void load(void)
{
    char path[MAX_PATH];
    GetFullPathNameA("orig\\English\\Views.DLL", MAX_PATH, path, 0);
    s_mfc = LoadLibraryA("cd\\mfc42.dll");
    if (!s_mfc) { fprintf(stderr, "native MFC42 missing: %lu\n", GetLastError()); exit(2); }
    s_ftol = (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_ftol");
    if (!s_ftol) exit(2);
    s_retail = LoadLibraryExA(path, 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || D32(s_retail, 0x6048) != 0x80000005) exit(2);
    patch(0x74b8, native_message); patch(0x74c4, native_help);
    patch(0x7450, native_close); patch(0x7300, native_get_text);
    patch(0x74a0, native_error); patch(0x74bc, native_enable);
    patch(0x7308, native_get_control); patch(0x7304, native_enable_control);
    patch(0x7374, dispatch_show); patch(0x72fc, native_set_text); patch(0x7498, native_check); patch(0x749c, native_radio);
    fs98_views_set_services(&s_services);
    patch(0x7310, native_ddx);
    patch(0x72d4, native_string_construct); patch(0x72ec, native_string_destroy);
    patch(0x72d8, native_string_buffer); patch(0x72dc, native_string_release);
    patch(0x74c0, native_load_string); patch(0x72e0, native_find_char); patch(0x72c4, native_find_text);
    patch(0x72e4, native_mid); patch(0x72f8, native_tail);
    patch(0x72e8, native_assign); patch(0x72f4, native_append); patch(0x72f0, native_append_text);
    patch(0x72c8, native_assign_text); patch(0x72d0, native_get_text_string);
    patch(0x72cc, string_format); patch(0x7404, native_scan_float); patch(0x7400, s_ftol);
    patch(0x7408, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "__CxxFrameHandler"));
    patch(0x73fc, compare_text);
    patch(0x7454, native_base_init); patch(0x7428, native_init_art); patch(0x7410, native_init_button);
    patch(0x7414, native_init_text); patch(0x7418, native_init_combo); patch(0x741c, native_init_group);
    patch(0x7420, native_remap); patch(0x730c, native_move); patch(0x74c8, native_dlg_item);
    patch(0x7424, native_base_font); patch(0x742c, native_init_edit);
#define CONTROL_PATCH(name, constructor, destructor) patch(constructor, construct_##name); patch(destructor, destroy_##name)
    CONTROL_PATCH(art, 0x743c, 0x7464); CONTROL_PATCH(button, 0x7440, 0x7468);
    CONTROL_PATCH(text, 0x7444, 0x746c); CONTROL_PATCH(combo, 0x7448, 0x7470);
    CONTROL_PATCH(group, 0x74a8, 0x7474); CONTROL_PATCH(radio, 0x74ac, 0x7478);
    CONTROL_PATCH(check, 0x74b0, 0x747c); CONTROL_PATCH(edit, 0x7458, 0x7480);
    CONTROL_PATCH(spin, 0x7318, 0x7314);
#undef CONTROL_PATCH
    patch(0x7438, native_construct_dialog); patch(0x7460, native_destroy_dialog);
    patch(0x744c, allocate_object); patch(0x7434, ui_delete); patch(0x737c, mfc_delete);
    patch(0x74a4, native_dialog_delete); patch(0x7378, native_create_window);
    patch(0x745c, s_control_vtable);
    D32(s_retail, 0x60ac) = 0x50060;
    fs98_views_set_object_services(&s_object_services, (uint8_t *)s_retail + 0x5320, s_control_vtable);
    fs98_views_set_object_module(0x50060);
    D32(s_retail, 0x60b0) = 0x50040;
    fs98_views_set_resource_module(0x50040);
    s_vtable[0xe4 / 4] = dialog_init;
    s_control_vtable[0xc4 / 4] = init_check;
    s_vtable[0xe0 / 4] = finish_error;
    s_vtable[0xd8 / 4] = bridge_validate;
    s_vtable[0xdc / 4] = bridge_error;
    s_vtable[0xec / 4] = bridge_close;
    s_vtable[1] = delete_object;
    memcpy(s_other_vtable, s_vtable, sizeof(s_vtable));
    s_other_vtable[0xec / 4] = other_close;
}
static void initialize(void)
{
    static const uint32_t modes[] = {0, 1, 2, 3, 4, 5, 0xffffffff, 0x80000000};
    static const uint32_t slots[] = {0xc, 0x20, 0x34, 0x48, 0x5c, 0x7c, 0x90};
    uint32_t index, record, field;
    uint8_t *bytes = (void *)&s_fixture;
    for (index = 0; index < sizeof(s_fixture); index++) bytes[index] = (uint8_t)next_random();
    for (index = 0; index < 6; index++)
        PTR(s_fixture.object, 0x1570 + index * 0xf4) = s_control_vtable;
    PTR(s_fixture.object, 0x1c94) = s_control_vtable;
    PTR(s_fixture.object, 0x242c) = s_control_vtable;
    PTR(s_fixture.object, 0x26f4) = s_control_vtable;
    PTR(s_fixture.object, 0) = s_vtable;
    PTR(s_fixture.object, 0x2804) = s_fixture.descriptors[0];
    for (index = 0; index < 2; index++) {
        void *descriptor = s_fixture.descriptors[index];
        PTR(descriptor, 4) = validate; PTR(descriptor, 8) = index ? preview_alternate : preview;
        D32(descriptor, 0x10) = s_scenario % 3;
        D32(descriptor, 0x14) = (s_scenario + index) % RECORDS;
        D32(descriptor, 0x18) = s_scenario % 9 ? (s_scenario % 11 ? RECORDS : 0xffffffff) : 0;
        PTR(descriptor, 0x1c) = s_fixture.records;
    }
    for (record = 0; record < RECORDS; record++) {
        PTR(s_fixture.records[record], 0) = s_fixture.buffers[record][0];
        D32(s_fixture.records[record], 4) = s_scenario % 5 == 0 ? (uint32_t[]){0, 1, 2, 0xffffffff}[(s_scenario / 5) % 4] : 0;
        D32(s_fixture.records[record], 8) = modes[(s_scenario + record) % 8];
        for (field = 0; field < 7; field++) {
            uint32_t slot = slots[field];
            D32(s_fixture.records[record], slot) = (next_random() & 3) ? 0 : next_random() | 1;
            PTR(s_fixture.records[record], slot + 4) = s_fixture.buffers[record][field];
            D32(s_fixture.records[record], slot + 8) = s_scenario % 4 ? 64 : 0;
            snprintf(s_fixture.buffers[record][field], 64, "record-%u-field-%u", record, field);
        }
    }
    s_initial = s_fixture;
}
static void fault_land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_fault_jump, 1);
}
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    if (s_running && s_scenario % 11 == 0 && s_operation == 37 &&
        record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        put("allocation.fault access=%u address=%u\n", (uint32_t)record->ExceptionInformation[0],
             (uint32_t)record->ExceptionInformation[1]);
        if (!s_side) s_allocation_faults++;
        exception->ContextRecord->Eip = (DWORD)fault_land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static EXCEPTION_DISPOSITION __cdecl catch_throw(EXCEPTION_RECORD *record, void *establisher,
                                                 CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == TEST_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(establisher, 0, record, 0);
        s_throw_at = 0;
        put("CAUGHT\n");
        if (!s_side) s_throws++;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
        __builtin_longjmp(s_fault_jump, 1);
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) int32_t run(void)
{
    void *entry = (uint8_t *)s_retail + s_entries[s_operation];
    void *object = s_fixture.object;
    volatile int32_t result = 0;
    struct fs98_views_unwind catcher;
    uint32_t argument = s_scenario % 3 ? s_seed : 0;
    s_fixture = s_initial;
    s_log_length = 0; s_logs[s_side][0] = 0; s_calls = 0;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    s_running = 1;
    s_throw_at = s_throw_position;
    s_dependency_calls = 0;
    __asm__ volatile("fldcw %0" : : "m"(s_control_word));
    if (__builtin_setjmp(s_fault_jump) == 0) {
    catcher.previous = (void *)s_saved_seh; catcher.handler = catch_throw; catcher.cleanup = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
    switch (s_operation) {
    case 26: {
        uint32_t legacy_bits = s_scenario & 1 ? 0x80000000 : 0;
        if (s_scenario % 5 >= 3) {
            D32(s_fixture.descriptors[0], 0x10) = D32(s_fixture.descriptors[1], 0x10) =
                s_scenario % 5 == 3 ? 3 : 0xffffffff;
            if (!s_side) s_invalid_units_hits++;
        }
        fs98_views_test_call(s_side ? (void *)fs98_views_map : entry, object, legacy_bits);
        break;
    }
    case 28: {
        static const uint32_t bits[] = {0x3f800000, 0xbf800000, 0x7fc00000, 0x7f800000, 0, 0x7f7fffff};
        fs98_views_test_call(s_side ? (void *)fs98_views_zoom : entry, object, bits[s_scenario % 6]);
        break;
    }
    case 32: case 38: case 39:
        if (s_side) fs98_views_construct(object, (void *)s_seed);
        else ((void *(TC *)(void *, void *))entry)(object, (void *)s_seed);
        if (s_operation == 38) {
            if (s_side) fs98_views_destroy(object);
            else ((event_fn)((uint8_t *)s_retail + 0x1450))(object);
        } else if (s_operation == 39) {
            if (s_side) fs98_views_delete(object, s_seed);
            else ((void *(TC *)(void *, uint32_t))((uint8_t *)s_retail + 0x1430))(object, s_seed);
        }
        break;
    case 33:
        if (s_side) fs98_views_destroy(object);
        else ((event_fn)entry)(object);
        break;
    case 34: case 35: {
        void *returned;
        if (s_operation == 35) object = (uint8_t *)object + 0x27bc;
        returned = s_side ? (s_operation == 34 ? fs98_views_delete(object, s_seed) :
                            fs98_views_spin_delete(object, s_seed)) :
            ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
        put("return.object %u\n", offset(returned));
        break;
    }
    case 36:
        if (s_side) fs98_views_class_delete(object);
        else ((void (SC *)(void *))entry)(object);
        break;
    case 37: {
        uint32_t returned = s_side ? fs98_views_create(s_seed, s_fixture.descriptors[0]) :
            ((uint32_t (SC *)(uint32_t, void *))entry)(s_seed, s_fixture.descriptors[0]);
        put("return.window %u\n", returned);
        break;
    }
    case 7:
        result = s_side ? fs98_views_help(object, argument, s_fixture.help_info) :
            ((message_fn)entry)(object, argument, (uint32_t)s_fixture.help_info);
        break;
    case 8: case 9:
        result = s_side ? (s_operation == 8 ? fs98_views_context_help(object, argument, s_seed) :
                  fs98_views_tick(object, argument, s_seed)) : ((message_fn)entry)(object, argument, s_seed);
        break;
    case 12: case 13:
        result = s_side ? (s_operation == 12 ? fs98_views_validate(object, argument) :
                  fs98_views_report_error(object, (int32_t)argument)) : ((arg_fn)entry)(object, argument);
        break;
    case 31:
        result = s_side ? fs98_views_init(object) : ((int32_t (TC *)(void *))entry)(object);
        break;
    case 29:
        if (s_side) fs98_views_spin(object, argument, s_seed, s_fixture.preview_control);
        else ((void (TC *)(void *, uint32_t, uint32_t, void *))entry)(object, argument, s_seed, s_fixture.preview_control);
        break;
    case 25:
        if (s_side) fs98_views_exchange(object, s_fixture.help_info);
        else ((void (TC *)(void *, void *))entry)(object, s_fixture.help_info);
        break;
    case 24:
        if (s_scenario % 3 == 0) object = 0;
        if (s_side) fs98_views_post_destroy(object);
        else ((event_fn)entry)(object);
        break;
    case 16:
        if (s_side) fs98_views_empty_arg(object, argument);
        else ((arg_fn)entry)(object, argument);
        break;
    default:
        if (s_side) s_events[s_operation](object);
        else ((event_fn)entry)(object);
        break;
    }
    }
    s_running = 0;
    __asm__ volatile("fnstcw %0" : "=m"(s_final_control_word));
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    return result;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 0) : 20000;
    uint32_t comparisons = 0, initial_seed, operation_count = sizeof(s_entries) / sizeof(s_entries[0]);
    if (!count) return 2;
    s_rng = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 0x7e51a23b;
    if (!s_rng) return 2;
    initial_seed = s_rng;
    setvbuf(stdout, 0, _IONBF, 0);
    SetUnhandledExceptionFilter(fault);
    AddVectoredExceptionHandler(1, expected_fault);
    load();
    for (s_scenario = 0; s_scenario < count; s_scenario++) {
        s_seed = s_rng; s_script = s_scenario % 8;
        s_throw_position = argc > 3 && !strcmp(argv[3], "unwind") ? s_scenario % 512 + 1 : 0;
        s_control_word = (uint16_t)(0x007f | ((s_scenario % 3 == 0 ? 0 : s_scenario % 3 == 1 ? 2 : 3) << 8) |
                                     (((s_scenario / 3) % 4) << 10));
        initialize();
        for (s_operation = 0; s_operation < operation_count; s_operation++) {
            int32_t expected, actual;
            uint16_t expected_control_word;
            s_side = 0; expected = run(); s_retail_result = s_fixture; expected_control_word = s_final_control_word;
            s_side = 1; actual = run();
            if (expected != actual || expected_control_word != s_final_control_word || memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) ||
                strcmp(s_logs[0], s_logs[1])) {
                fprintf(stderr, "DIFFER scenario=%u seed=%08x operation=%u return=%d/%d\nRETAIL:\n%sOURS:\n%s",
                         s_scenario, s_seed, s_operation, expected, actual, s_logs[0], s_logs[1]);
                return 1;
            }
            comparisons++;
        }
    }
    printf("Views events: %u scenarios, %u comparisons, 0 differ; seed=%08x\n", count, comparisons, initial_seed);
    printf("allocation faults=%u caught dependency throws=%u map tail parses=%u\n", s_allocation_faults, s_throws, s_map_tail_hits);
    printf("painted-stack malformed zoom=%u invalid units=%u\n", s_malformed_zoom_hits, s_invalid_units_hits);
    printf("callback=%u selection changes=%u error dialogs=%u numeric reads=%u\n",
            s_callback_hits, s_selection_changes, s_error_hits, s_text_hits);
    return 0;
}
