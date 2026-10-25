/* Alerts slice 1: real command handlers and modeless progress tick against C.
 * Maps retail without resolving imports or running its CRT/DllMain. Only the
 * routines named in s_retail_entries are invoked. Dependency calls reached
 * by those routines are patched to a deterministic MFC/Win32 model. This
 * tests event logic, descriptor mutations, virtual-call order and drawing
 * transcripts; it does not test construction, MFC dispatch or real pixels. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../src/alerts/alerts_internal.h"
#include "../../src/alerts/alerts_unwind.h"

#define STDCALL __attribute__((stdcall))
#define THISCALL FS98_ALERTS_THISCALL
#define D32(val, offset) (*(uint32_t *)((uint8_t *)(val) + (offset)))
#define PTR(val, offset) (*(void **)((uint8_t *)(val) + (offset)))
#define OBJECT_SIZE 0x1560
#define DESCRIPTOR_SIZE 0x40
#define LOG_SIZE 8192

typedef void (THISCALL *fs98_event_fn)(void *object);
typedef int32_t (THISCALL *fs98_tick_fn)(void *object, uint32_t w_param, uint32_t l_param);
typedef void (THISCALL *fs98_exchange_fn)(void *object, void *exchange);
typedef int32_t (THISCALL *fs98_init_event_fn)(void *object);

static uint8_t s_object_storage[OBJECT_SIZE] __attribute__((aligned(16)));
static uint8_t s_descriptor_bytes[DESCRIPTOR_SIZE] __attribute__((aligned(16)));
static uint8_t s_initial_object[OBJECT_SIZE], s_initial_descriptor[DESCRIPTOR_SIZE];
static uint8_t s_retail_object[OBJECT_SIZE], s_retail_descriptor[DESCRIPTOR_SIZE];
static void *s_vtable[60], *s_other_vtable[60];
static void *s_check_vtable[60];
static char s_log[2][LOG_SIZE];
static uint32_t s_log_length[2];
static uint32_t s_rng, s_scenario, s_seed;
static int s_side, s_entry, s_replace_vtable;
static int s_running;
static struct fs98_alerts_rect s_rect;
static struct fs98_alerts_size s_size;
static int32_t s_child_x, s_child_y, s_parent_x, s_parent_y;
static uint32_t s_foreground, s_background;
static struct {
    uint32_t references, length, capacity;
    char text_buffer[128];
} s_string;
static uint32_t s_enabled, s_disabled, s_closed, s_wrapped;
static HMODULE s_retail;
static uint32_t s_exchange[4];
static uint32_t s_product_witness;
static uint8_t s_host_services[0x300];
static const char s_message[] = "Alerts fixture: line one\nline two";
static int s_base_init_ok;
static uint32_t s_init_success_count, s_init_failure;
static uint32_t s_text_resources, s_direct_text;
static uint32_t s_button_flags[256];
static int s_throw_at, s_calls;
static uint32_t s_saved_seh, s_throws;
static void *s_fault_jump[5];
#define TEST_THROW 0x0e041a11

static uint32_t fs98_rnd(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int32_t fs98_add(int32_t left, int32_t right)
{
    return (int32_t)((uint32_t)left + (uint32_t)right);
}

static int32_t fs98_sub(int32_t left, int32_t right)
{
    return (int32_t)((uint32_t)left - (uint32_t)right);
}

static void fs98_put(const char *format, ...)
{
    va_list args;
    int written;
    uint32_t length = s_log_length[s_side];
    if (length > LOG_SIZE - 1024) {
        fprintf(stderr, "transcript overflow\n");
        exit(2);
    }
    va_start(args, format);
    written = vsnprintf(s_log[s_side] + length, LOG_SIZE - length, format, args);
    va_end(args);
    if (written < 0)
        exit(2);
    s_log_length[s_side] += (uint32_t)written;
    s_calls++;
    if (s_running && s_throw_at && s_calls == s_throw_at) {
        written = snprintf(s_log[s_side] + s_log_length[s_side],
                             LOG_SIZE - s_log_length[s_side], "THROW at=%d\n", s_throw_at);
        s_log_length[s_side] += (uint32_t)written;
        RaiseException(TEST_THROW, 0, 0, 0);
    }
}

static void fs98_string_construct(void *string)
{
    fs98_put("string.construct\n");
    PTR(string, 0) = s_string.text_buffer;
    s_string.text_buffer[0] = 0;
    s_string.length = 0;
}

static void fs98_string_destroy(void *string)
{
    fs98_put("string.destroy %s\n", (char *)PTR(string, 0));
}

static void fs98_dc_construct(void *dc, void *window)
{
    fs98_put("dc.construct %u\n", D32(window, 0x20));
    memset(dc, 0, 0x14);
    D32(dc, 4) = 0xd001;
    D32(dc, 8) = 0xd002;
}

static void fs98_dc_destroy(void *dc)
{
    fs98_put("dc.destroy %u\n", D32(dc, 4));
}

static uint32_t fs98_stock(int32_t object)
{
    fs98_put("stock %d\n", object);
    return 0xf001;
}

static void *fs98_font(uint32_t font)
{
    fs98_put("font %u\n", font);
    return (void *)0xf002;
}

static void *fs98_select(void *dc, void *font)
{
    fs98_put("select %u %u\n", D32(dc, 4), (uint32_t)font);
    return (void *)0xf003;
}

static int32_t STDCALL fs98_client_rect(uint32_t window, struct fs98_alerts_rect *rect)
{
    *rect = s_rect;
    fs98_put("rect %u %d %d %d %d\n", window, rect->left, rect->top,
             rect->right, rect->bottom);
    return 1;
}

static int32_t STDCALL fs98_client_screen(uint32_t window, struct fs98_alerts_point *point)
{
    fs98_put("to.screen %u %d %d\n", window, point->x, point->y);
    point->x = fs98_add(point->x, s_child_x);
    point->y = fs98_add(point->y, s_child_y);
    return 1;
}

static int32_t STDCALL fs98_screen_client(uint32_t window, struct fs98_alerts_point *point)
{
    fs98_put("to.client %u %d %d\n", window, point->x, point->y);
    point->x = fs98_sub(point->x, s_parent_x);
    point->y = fs98_sub(point->y, s_parent_y);
    return 1;
}

static void fs98_format(void *string, const char *format, int32_t value)
{
    fs98_put("format %s %d\n", format, value);
    snprintf(s_string.text_buffer, sizeof(s_string.text_buffer), format, value);
    s_string.length = (uint32_t)strlen(s_string.text_buffer);
    PTR(string, 0) = s_string.text_buffer;
}

static int32_t STDCALL fs98_extent(uint32_t dc, const char *text, int32_t length,
                                struct fs98_alerts_size *size_out)
{
    fs98_put("extent %u %d %s\n", dc, length, text);
    *size_out = s_size;
    return 1;
}

static uint32_t fs98_text_color(void *dc, uint32_t color)
{
    uint32_t previous = s_foreground;
    fs98_put("foreground %u %u\n", D32(dc, 4), color);
    s_foreground = color;
    return previous;
}

static uint32_t fs98_background(void *dc, uint32_t color)
{
    uint32_t previous = s_background;
    fs98_put("background %u %u\n", D32(dc, 4), color);
    s_background = color;
    return previous;
}

static int32_t STDCALL fs98_text_out(uint32_t dc, int32_t x, int32_t y, uint32_t options,
                                  const struct fs98_alerts_rect *rect, const char *text,
                                  uint32_t length, const int32_t *spacing)
{
    fs98_put("text %u %d %d %u rect=%d,%d,%d,%d len=%u %s spacing=%u colors=%u,%u\n",
             dc, x, y, options, rect->left, rect->top, rect->right,
             rect->bottom, length, text, (uint32_t)spacing, s_foreground, s_background);
    return 1;
}

static int32_t STDCALL fs98_help(uint32_t window, const char *file,
                              uint32_t command, uint32_t context)
{
    fs98_put("help %u %s %u %u\n", window, file, command, context);
    return (int32_t)(s_seed & 1);
}

/* Regular-C service adapters for WINAPI imports. */
static int32_t fs98_core_rect(uint32_t window, struct fs98_alerts_rect *rect)
{ return fs98_client_rect(window, rect); }
static int32_t fs98_core_screen(uint32_t window, struct fs98_alerts_point *point)
{ return fs98_client_screen(window, point); }
static int32_t fs98_core_client(uint32_t window, struct fs98_alerts_point *point)
{ return fs98_screen_client(window, point); }
static int32_t fs98_core_extent(uint32_t dc, const char *text, int32_t length,
                              struct fs98_alerts_size *size_out)
{ return fs98_extent(dc, text, length, size_out); }
static int32_t fs98_core_text(uint32_t dc, int32_t x, int32_t y, uint32_t options,
                            const struct fs98_alerts_rect *rect, const char *text,
                            uint32_t length, const int32_t *spacing)
{ return fs98_text_out(dc, x, y, options, rect, text, length, spacing); }
static int32_t fs98_core_help(uint32_t window, const char *file, uint32_t command, uint32_t context)
{ return fs98_help(window, file, command, context); }

static void fs98_base_exchange(void *object, void *exchange)
{
    fs98_put("base.exchange object=%u exchange=%u\n",
             (uint32_t)((uint8_t *)object - s_object_storage), D32(exchange, 0));
}

static void fs98_ddx_control(void *exchange, uint32_t control_id, void *control)
{
    fs98_put("ddx.control exchange=%u id=%u object=%u\n", D32(exchange, 0),
             control_id, (uint32_t)((uint8_t *)control - s_object_storage));
    D32(control, 0x20) = 0x10000 + control_id;
}

static void fs98_dib_construct(void *dib)
{
    fs98_put("dib.construct\n");
    memset(dib, 0, 0x70);
}

static void fs98_dib_destroy(void *dib)
{
    fs98_put("dib.destroy %d %d\n", (int32_t)D32(dib, 8), (int32_t)D32(dib, 0xc));
}

static int32_t fs98_base_init(void *object)
{
    fs98_put("base.init %u\n", D32(object, 0x20));
    return (int32_t)s_seed;
}

static int32_t STDCALL fs98_load_string(uint32_t id, char *buffer, uint32_t capacity)
{
    int length;
    fs98_put("load.string %u %u\n", id, capacity);
    length = snprintf(buffer, capacity, "fixture resource:%08x", id);
    return length;
}

static void fs98_set_text(void *control, const char *text)
{
    fs98_put("set.text %u %s\n", (uint32_t)((uint8_t *)control - s_object_storage), text);
    D32(control, 0x24) = (uint32_t)strlen(text);
}

static void fs98_init_text(void *control, void *font)
{
    fs98_put("init.text %u font=%u\n", (uint32_t)((uint8_t *)control - s_object_storage),
             (uint32_t)((uint8_t *)font - s_object_storage));
}

static int32_t fs98_load_dib(void *dib, int32_t id, uint32_t module, void *palette, uint32_t flags)
{
    fs98_put("load.dib %d module=%u palette=%u flags=%u\n", id, module,
             (uint32_t)((uint8_t *)palette - s_object_storage), flags);
    D32(dib, 8) = s_seed % 129;
    D32(dib, 0xc) = (s_seed >> 8) % 97;
    return (int32_t)(s_seed & 1);
}

static int32_t fs98_sprite(void *dib, void *destination, int32_t x, int32_t y,
                         int32_t width, int32_t height, int32_t source_x, int32_t source_y,
                         uint32_t raster)
{
    fs98_put("sprite dst=%u xy=%d,%d wh=%d,%d src=%d,%d raster=%u dib=%u,%u\n",
             (uint32_t)((uint8_t *)destination - s_object_storage), x, y, width, height,
             source_x, source_y, raster, D32(dib, 8), D32(dib, 0xc));
    return 1;
}

static int32_t fs98_show(void *control, int32_t show)
{
    fs98_put("show object=%u flag=%d\n", (uint32_t)((uint8_t *)control - s_object_storage), show);
    return 1;
}

static int32_t fs98_core_load_string(uint32_t id, char *buffer, uint32_t capacity)
{ return fs98_load_string(id, buffer, capacity); }

static int32_t fs98_window_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{
    fs98_put("window.string module=%u id=%u capacity=%d\n", module, id, capacity);
    return snprintf(buffer, (uint32_t)capacity, "window resource:%08x", id);
}

static void fs98_button_text(void *control, const char *text)
{
    fs98_put("button.text %u %s\n", (uint32_t)((uint8_t *)control - s_object_storage), text);
    D32(control, 0x24) = (uint32_t)strlen(text);
}

static int32_t fs98_button_init(void *control, void *font, void *alternate_font, uint32_t bitmap,
                              void *dib, void *palette, uint32_t flags, uint32_t color,
                              uint32_t module, int32_t x, int32_t y, struct fs98_alerts_rect rect)
{
    fs98_put("button.init obj=%u fonts=%u,%u bitmap=%u dib=%u pal=%u flags=%u color=%u "
             "module=%u xy=%d,%d rect=%d,%d,%d,%d\n",
             (uint32_t)((uint8_t *)control - s_object_storage), (uint32_t)((uint8_t *)font - s_object_storage),
             (uint32_t)((uint8_t *)alternate_font - s_object_storage), bitmap,
             (uint32_t)((uint8_t *)dib - s_object_storage), (uint32_t)((uint8_t *)palette - s_object_storage),
             flags, color, module, x, y,
             rect.left, rect.top, rect.right, rect.bottom);
    return (int32_t)s_seed;
}

static int32_t fs98_set_checked(void *control, int32_t checked)
{
    int32_t previous = (int32_t)D32(control, 0xbc);
    fs98_put("set.checked object=%u value=%d\n", (uint32_t)((uint8_t *)control - s_object_storage), checked);
    D32(control, 0xbc) = (uint32_t)checked;
    return previous;
}

static int32_t THISCALL fs98_check_init(void *self, void *font, uint32_t bitmap,
                                     void *dib, void *palette, uint32_t module)
{
    fs98_put("check.init object=%u font=%u bitmap=%u dib=%u palette=%u module=%u\n",
             (uint32_t)((uint8_t *)self - s_object_storage), (uint32_t)((uint8_t *)font - s_object_storage),
             bitmap, (uint32_t)((uint8_t *)dib - s_object_storage),
             (uint32_t)((uint8_t *)palette - s_object_storage), module);
    return (int32_t)s_seed;
}

static const struct fs98_alerts_services s_runtime_services = {
    fs98_string_construct, fs98_string_destroy, fs98_dc_construct, fs98_dc_destroy,
    fs98_stock, fs98_font, fs98_select, fs98_core_rect, fs98_core_screen,
    fs98_core_client, fs98_format, fs98_core_extent, fs98_text_color,
    fs98_background, fs98_core_text, fs98_core_help,
    fs98_base_exchange, fs98_ddx_control, fs98_dib_construct, fs98_dib_destroy,
    fs98_base_init, fs98_core_load_string, fs98_set_text, fs98_init_text,
    fs98_load_dib, fs98_sprite, fs98_show, fs98_window_string,
    fs98_button_text, fs98_button_init, fs98_set_checked
};

/* The retail imports use thiscall/stdcall; keep those adapters at the actual
 * PE IAT boundary so the dependency transcript is shared with the C slice. */
static void *THISCALL fs98_mfc_string(void *self)
{ fs98_string_construct(self); return self; }
static void THISCALL fs98_mfc_string_destroy(void *self)
{ fs98_string_destroy(self); }
static void *THISCALL fs98_mfc_dc(void *self, void *window)
{ fs98_dc_construct(self, window); return self; }
static void THISCALL fs98_mfc_dc_destroy(void *self)
{ fs98_dc_destroy(self); }
static uint32_t STDCALL fs98_mfc_stock(int32_t object)
{ return fs98_stock(object); }
static void *STDCALL fs98_mfc_font(uint32_t font)
{ return fs98_font(font); }
static void *THISCALL fs98_mfc_select(void *self, void *font)
{ return fs98_select(self, font); }
static uint32_t THISCALL fs98_mfc_text_color(void *self, uint32_t color)
{ return fs98_text_color(self, color); }
static uint32_t THISCALL fs98_mfc_background(void *self, uint32_t color)
{ return fs98_background(self, color); }
static void THISCALL fs98_mfc_base_exchange(void *self, void *exchange)
{ fs98_base_exchange(self, exchange); }
static void STDCALL fs98_mfc_ddx(void *exchange, uint32_t control_id, void *control)
{ fs98_ddx_control(exchange, control_id, control); }
static void *THISCALL fs98_mfc_dib_construct(void *self)
{ fs98_dib_construct(self); return self; }
static void THISCALL fs98_mfc_dib_destroy(void *self)
{ fs98_dib_destroy(self); }
static int32_t THISCALL fs98_mfc_base_init(void *self)
{ return fs98_base_init(self); }
static void THISCALL fs98_mfc_set_text(void *self, const char *text)
{ fs98_set_text(self, text); }
static void THISCALL fs98_mfc_init_text(void *self, void *font)
{ fs98_init_text(self, font); }
static int32_t THISCALL fs98_mfc_load_dib(void *self, int32_t id, uint32_t module,
                                      void *palette, uint32_t flags)
{ return fs98_load_dib(self, id, module, palette, flags); }
static int32_t THISCALL fs98_mfc_sprite(void *self, void *destination, int32_t x, int32_t y,
                                    int32_t width, int32_t height, int32_t source_x,
                                    int32_t source_y, uint32_t raster)
{ return fs98_sprite(self, destination, x, y, width, height, source_x, source_y, raster); }
static int32_t THISCALL fs98_mfc_show(void *self, int32_t show)
{ return fs98_show(self, show); }
static int32_t STDCALL fs98_mfc_window_string(uint32_t module, uint32_t id,
                                           char *buffer, int32_t capacity)
{ return fs98_window_string(module, id, buffer, capacity); }
static void THISCALL fs98_mfc_button_text(void *self, const char *text)
{ fs98_button_text(self, text); }
static int32_t THISCALL fs98_mfc_button_init(void *self, void *font, void *alternate_font,
                                         uint32_t bitmap, void *dib, void *palette,
                                         uint32_t flags, uint32_t color, uint32_t module,
                                         int32_t x, int32_t y, struct fs98_alerts_rect rect)
{ return fs98_button_init(self, font, alternate_font, bitmap, dib, palette,
                          flags, color, module, x, y, rect); }
static int32_t THISCALL fs98_mfc_set_checked(void *self, int32_t checked)
{ return fs98_set_checked(self, checked); }

static int32_t THISCALL fs98_virtual_init(void *self, int32_t id, uint32_t module, uint32_t flags)
{
    fs98_put("virtual.init id=%d module=%u flags=%u object=%u\n", id, module,
             flags, (uint32_t)((uint8_t *)self - s_object_storage));
    return s_base_init_ok;
}

static void THISCALL fs98_close(void *self)
{
    void *descriptor = PTR(self, 0x10dc);
    fs98_put("close answer=%u live=%u flag=%u\n", D32(descriptor, 0),
             D32(descriptor, 0x0c), D32(descriptor, 0x10));
    if (s_replace_vtable)
        PTR(self, 0) = s_other_vtable;
}

static void THISCALL fs98_destroy(void *self)
{
    fs98_put("destroy %u\n", D32(self, 0x20));
}

static void THISCALL fs98_other_destroy(void *self)
{
    fs98_put("other.destroy %u\n", D32(self, 0x20));
}

static const uint32_t s_retail_entries[] = {
    0x19c0, 0x19e0, 0x1a00, 0x2560, 0x25d0, 0x25b0, 0x25a0, 0x1db0,
    0x1450, 0x1b70, 0x2260, 0x1bc0, 0x14c0, 0x22f0
};
static const fs98_exchange_fn s_own_exchange[] = {
    fs98_alerts_modal_exchange, fs98_alerts_modeless_exchange, fs98_alerts_advisory_exchange
};
static const fs98_event_fn s_own_entries[] = {
    fs98_alerts_modal_first, fs98_alerts_modal_second, fs98_alerts_modal_third,
    fs98_alerts_advisory_accept, fs98_alerts_advisory_quit,
    fs98_alerts_advisory_help, fs98_alerts_noop
};

static void fs98_patch(uint32_t rva, void *function)
{
    DWORD old_protect;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old_protect))
        exit(2);
    *slot = function;
    VirtualProtect(slot, 4, old_protect, &old_protect);
}

static void fs98_load(void)
{
    char path[MAX_PATH];
    GetFullPathNameA("orig\\English\\Alerts.DLL", MAX_PATH, path, 0);
    s_retail = LoadLibraryExA(path, 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail) {
        fprintf(stderr, "retail Alerts load failed: %lu path=%s\n", GetLastError(), path);
        exit(2);
    }
    /* Validate reference layout before patching or executing its instructions. */
    if (D32(s_retail, 0x4060) != 0x80000009 ||
        D32(s_retail, 0x4018) != 2 || D32(s_retail, 0x4024) != 3 ||
        D32(s_retail, 0x4030) != 4 ||
        D32(s_retail, 0x401c) != (uint32_t)s_retail + 0x1130) {
        fprintf(stderr, "retail Alerts reference layout changed\n");
        exit(2);
    }
    fs98_patch(0x5300, fs98_mfc_string);
    fs98_patch(0x5304, fs98_mfc_string_destroy);
    fs98_patch(0x52b0, fs98_mfc_dc);
    fs98_patch(0x52b4, fs98_mfc_font);
    fs98_patch(0x52b8, fs98_mfc_select);
    fs98_patch(0x52bc, fs98_format);
    fs98_patch(0x52c0, fs98_mfc_text_color);
    fs98_patch(0x52c4, fs98_mfc_background);
    fs98_patch(0x52c8, fs98_mfc_dc_destroy);
    fs98_patch(0x5280, fs98_mfc_stock);
    fs98_patch(0x527c, fs98_extent);
    fs98_patch(0x5284, fs98_text_out);
    fs98_patch(0x5458, fs98_client_rect);
    fs98_patch(0x545c, fs98_client_screen);
    fs98_patch(0x5460, fs98_screen_client);
    fs98_patch(0x5454, fs98_help);
    fs98_patch(0x53f8, fs98_mfc_base_exchange);
    fs98_patch(0x52fc, fs98_mfc_ddx);
    fs98_patch(0x53e0, fs98_mfc_dib_construct);
    fs98_patch(0x53d8, fs98_mfc_dib_destroy);
    fs98_patch(0x53d4, fs98_mfc_base_init);
    fs98_patch(0x53dc, fs98_mfc_set_text);
    fs98_patch(0x53e8, fs98_mfc_init_text);
    fs98_patch(0x53ec, fs98_mfc_load_dib);
    fs98_patch(0x53f0, fs98_mfc_sprite);
    fs98_patch(0x52ac, fs98_mfc_show);
    fs98_patch(0x5450, fs98_mfc_window_string);
    fs98_patch(0x52f8, fs98_mfc_button_text);
    fs98_patch(0x5400, fs98_mfc_button_init);
    fs98_patch(0x5438, fs98_mfc_set_checked);
    fs98_patch(0x53c8, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "__CxxFrameHandler"));
    fs98_patch(0x53cc, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_except_handler3"));
    PTR(s_host_services, 0x2a0) = fs98_load_string;
    PTR(s_retail, 0x404c) = s_host_services;
    D32(s_retail, 0x40a4) = 0x50050;
}

static LONG WINAPI fs98_fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT scenario=%u seed=%08x side=%d entry=%d code=%08lx at=%p running=%d\n",
            s_scenario, s_seed, s_side, s_entry,
            exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress, s_running);
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

static EXCEPTION_DISPOSITION __cdecl fs98_catch(
    EXCEPTION_RECORD *record, void *establisher, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == TEST_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(establisher, 0, record, 0);
        s_throw_at = 0;
        fs98_put("CAUGHT\n");
        if (!s_side) s_throws++;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
        __builtin_longjmp(s_fault_jump, 1);
    }
    return ExceptionContinueSearch;
}

static void fs98_prepare(void)
{
    uint32_t index;
    static const uint32_t s_edges[] = {
        0, 1, 2, 99, 100, 101, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff
    };
    for (index = 0; index < OBJECT_SIZE; index++)
        s_object_storage[index] = (uint8_t)fs98_rnd();
    for (index = 0; index < DESCRIPTOR_SIZE; index++)
        s_descriptor_bytes[index] = (uint8_t)fs98_rnd();
    PTR(s_object_storage, 0) = s_vtable;
    PTR(s_object_storage, 0x10dc) = s_descriptor_bytes;
    PTR(s_object_storage, 0x1494) = s_check_vtable;
    D32(s_object_storage, 0x20) = 0x10020;
    D32(s_object_storage, 0x1104) = 0x10030;
    D32(s_object_storage, 0x10e0) = 0x50060;
    D32(s_descriptor_bytes, 8) = s_scenario % 7;
    if (s_scenario % 7 == 6)
        D32(s_descriptor_bytes, 8) = 0xffffffff;
    switch (s_scenario % 6) {
    case 0: D32(s_descriptor_bytes, 0x24) = 0; break;
    case 1: D32(s_descriptor_bytes, 0x24) = 1; break;
    case 2: D32(s_descriptor_bytes, 0x24) = 0xffff; break;
    case 3: D32(s_descriptor_bytes, 0x24) = 0x80000000; break;
    case 4: D32(s_descriptor_bytes, 0x24) = 0xffffffff; break;
    default: PTR(s_descriptor_bytes, 0x24) = (void *)s_message; break;
    }
    s_base_init_ok = s_scenario % 9 != 0;
    D32(s_descriptor_bytes, 0x0c) = s_scenario % 7 ? fs98_rnd() | 1 : 0;
    D32(s_descriptor_bytes, 0x10) = s_scenario % 5 ? fs98_rnd() | 1 : 0;
    D32(s_descriptor_bytes, 0x14) = s_edges[s_scenario % 10];
    D32(s_descriptor_bytes, 0x18) = s_edges[(s_scenario / 10) % 10];
    D32(s_descriptor_bytes, 0x1c) = s_edges[(s_scenario / 100) % 10];
    D32(s_descriptor_bytes, 0x20) = s_edges[(s_scenario / 1000) % 10];
    if (s_scenario % 3 == 2) {
        D32(s_descriptor_bytes, 0x14) = fs98_rnd();
        D32(s_descriptor_bytes, 0x18) = fs98_rnd();
        D32(s_descriptor_bytes, 0x1c) = fs98_rnd();
        D32(s_descriptor_bytes, 0x20) = fs98_rnd();
    }
    /* A self-checking product-wrap witness: 100*INT_MAX wraps to 0xffffff9c,
     * then unsigned division by INT_MAX yields 1. A widened product gives
     * 100 instead. Broad random ranges usually clamp start to limit-1,
     * leaving a multiplier of one and never exercising this distinction. */
    if (s_scenario % 17 == 16) {
        D32(s_descriptor_bytes, 0x0c) = 1;
        D32(s_descriptor_bytes, 0x10) = 1;
        D32(s_descriptor_bytes, 0x14) = 0x7fffffff;
        D32(s_descriptor_bytes, 0x18) = 0x7fffffff;
        D32(s_descriptor_bytes, 0x1c) = 0;
        D32(s_descriptor_bytes, 0x20) = 100;
    }
    s_replace_vtable = s_scenario & 1;
    s_rect.left = (int32_t)(fs98_rnd() % 31) - 15;
    s_rect.top = (int32_t)(fs98_rnd() % 31) - 15;
    s_rect.right = (int32_t)(fs98_rnd() % 2049);
    s_rect.bottom = (int32_t)(fs98_rnd() % 129);
    s_size.width = (int32_t)(fs98_rnd() % 251);
    s_size.height = (int32_t)(fs98_rnd() % 101);
    s_child_x = (int32_t)(fs98_rnd() % 1001) - 500;
    s_child_y = (int32_t)(fs98_rnd() % 1001) - 500;
    s_parent_x = (int32_t)(fs98_rnd() % 1001) - 500;
    s_parent_y = (int32_t)(fs98_rnd() % 1001) - 500;
    memcpy(s_initial_object, s_object_storage, OBJECT_SIZE);
    memcpy(s_initial_descriptor, s_descriptor_bytes, DESCRIPTOR_SIZE);
}

static FS98_ALERTS_NOINLINE void fs98_run(void)
{
    int32_t result;
    struct fs98_alerts_unwind catch;
    s_log_length[s_side] = 0;
    s_log[s_side][0] = 0;
    memcpy(s_object_storage, s_initial_object, OBJECT_SIZE);
    memcpy(s_descriptor_bytes, s_initial_descriptor, DESCRIPTOR_SIZE);
    if (s_entry == 12 || s_entry == 14) {
        D32(s_object_storage, 0x10e4) = 0;
        D32(s_object_storage, 0x10e8) = 0;
        D32(s_object_storage, 0x10ec) = 0;
        D32(s_object_storage, 0x10f4) = 0xffffffff;
        D32(s_object_storage, 0x10f8) = 0xffffffff;
        D32(s_descriptor_bytes, 8) = (s_seed & 0xffffff00) | (s_scenario & 255);
        D32(s_descriptor_bytes, 0x14) = D32(s_descriptor_bytes, 0x24);
        D32(s_descriptor_bytes, 0x10) = s_scenario % 7;
    }
    memset(&s_string, 0xa5, sizeof(s_string));
    s_foreground = 0x556677;
    s_background = 0x112233;
    s_calls = 0;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    s_running = 1;
    if (__builtin_setjmp(s_fault_jump)) goto complete;
    catch.previous = (void *)s_saved_seh;
    catch.handler = fs98_catch; catch.cleanup = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(&catch) : "memory");
    if (s_entry >= 14) {
        int init_entry = s_entry == 14 ? 12 : s_entry == 15 ? 13 : 11;
        fs98_init_event_fn init = s_side ?
            (init_entry == 11 ? fs98_alerts_modeless_init : init_entry == 12 ?
             fs98_alerts_modal_init : fs98_alerts_advisory_init) :
            (fs98_init_event_fn)((uint8_t *)s_retail + s_retail_entries[init_entry]);
        int32_t ready = init(s_object_storage);
        fs98_put("sequence.init %d\n", ready);
        if (ready && s_entry == 14) {
            int button = (int)(s_scenario % 3);
            fs98_event_fn event = s_side ? s_own_entries[button] :
                (fs98_event_fn)((uint8_t *)s_retail + s_retail_entries[button]);
            event(s_object_storage);
        } else if (ready && s_entry == 15) {
            int button = s_scenario & 1 ? 3 : 4;
            fs98_event_fn event = s_side ? s_own_entries[button] :
                (fs98_event_fn)((uint8_t *)s_retail + s_retail_entries[button]);
            /* User edits the checkbox after init; the real SIMSUI setter's
             * +0xbc write is independently pinned in its retail callee. */
            fs98_set_checked(s_object_storage + 0x1494, (int32_t)(s_seed ^ 0x5a5a5a5a));
            event(s_object_storage);
        } else if (ready) {
            fs98_tick_fn tick = s_side ? fs98_alerts_progress_tick :
                (fs98_tick_fn)((uint8_t *)s_retail + s_retail_entries[7]);
            int was_live = D32(s_descriptor_bytes, 0xc) != 0;
            fs98_put("sequence.tick %d\n", tick(s_object_storage, s_seed, ~s_seed));
            if (was_live) {
                D32(s_descriptor_bytes, 0xc) = 0;
                fs98_put("sequence.close %d\n", tick(s_object_storage, 0, 0));
            }
        }
    } else if (s_entry >= 11) {
        fs98_init_event_fn init = s_side ?
            (s_entry == 11 ? fs98_alerts_modeless_init : s_entry == 12 ?
             fs98_alerts_modal_init : fs98_alerts_advisory_init) :
            (fs98_init_event_fn)((uint8_t *)s_retail + s_retail_entries[s_entry]);
        result = init(s_object_storage);
        fs98_put("return %d\n", result);
        if (s_entry == 12 && s_side == 0 && result)
            s_button_flags[s_scenario & 255]++;
    } else if (s_entry >= 8) {
        fs98_exchange_fn exchange = s_side ? s_own_exchange[s_entry - 8] :
            (fs98_exchange_fn)((uint8_t *)s_retail + s_retail_entries[s_entry]);
        s_exchange[0] = s_seed;
        exchange(s_object_storage, s_exchange);
    } else if (s_entry == 7) {
        fs98_tick_fn tick = s_side ? fs98_alerts_progress_tick :
                            (fs98_tick_fn)((uint8_t *)s_retail + s_retail_entries[7]);
        result = tick(s_object_storage, s_seed, ~s_seed);
        fs98_put("return %d\n", result);
        if (s_scenario % 17 == 16 && s_side == 0) {
            if (!strstr(s_log[0], "format %3d%% 1\n")) {
                fprintf(stderr, "product-wrap witness not reached\n");
                exit(2);
            }
            s_product_witness++;
        }
    } else {
        fs98_event_fn event = s_side ? s_own_entries[s_entry] :
                              (fs98_event_fn)((uint8_t *)s_retail + s_retail_entries[s_entry]);
        event(s_object_storage);
    }
complete:
    s_running = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
}

int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 0) : 20000;
    uint32_t initial_seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 0x41e2753b;
    uint32_t mismatch = 0;
    if (count == 0 || initial_seed == 0)
        return 2;
    setvbuf(stdout, 0, _IONBF, 0);
    SetUnhandledExceptionFilter(fs98_fault);
    s_vtable[0xec / 4] = fs98_close;
    s_vtable[0x60 / 4] = fs98_destroy;
    s_vtable[0xe4 / 4] = fs98_virtual_init;
    s_check_vtable[0xc4 / 4] = fs98_check_init;
    memcpy(s_other_vtable, s_vtable, sizeof(s_vtable));
    s_other_vtable[0x60 / 4] = fs98_other_destroy;
    fs98_alerts_set_services(&s_runtime_services);
    fs98_alerts_set_resource_module(0x50050);
    fs98_load();
    s_rng = initial_seed;
    for (s_scenario = 0; s_scenario < count; s_scenario++) {
        s_seed = s_rng;
        fs98_prepare();
        if (!D32(s_descriptor_bytes, 0xc)) s_closed++;
        else if (!D32(s_descriptor_bytes, 0x10)) s_disabled++;
        else s_enabled++;
        if ((int32_t)D32(s_descriptor_bytes, 0x14) < 0) s_wrapped++;
        if (s_base_init_ok) s_init_success_count++; else s_init_failure++;
        if (s_scenario % 6 == 5) s_direct_text++; else s_text_resources++;
        for (s_entry = 0; s_entry < 17; s_entry++) {
            s_side = 0;
            s_throw_at = argc > 3 && !strcmp(argv[3], "unwind") ?
                (int)(s_scenario % 32) + 1 : 0;
            fs98_run();
            memcpy(s_retail_object, s_object_storage, OBJECT_SIZE);
            memcpy(s_retail_descriptor, s_descriptor_bytes, DESCRIPTOR_SIZE);
            s_side = 1;
            s_throw_at = argc > 3 && !strcmp(argv[3], "unwind") ?
                (int)(s_scenario % 32) + 1 : 0;
            fs98_run();
            if (strcmp(s_log[0], s_log[1]) ||
                memcmp(s_retail_object, s_object_storage, OBJECT_SIZE) ||
                memcmp(s_retail_descriptor, s_descriptor_bytes, DESCRIPTOR_SIZE)) {
                fprintf(stderr, "DIFFER scenario=%u seed=%08x entry=%d\nRETAIL:\n%sOURS:\n%s",
                        s_scenario, s_seed, s_entry, s_log[0], s_log[1]);
                mismatch++;
                goto done;
            }
        }
    }
done:
    printf("Alerts events: %u scenarios, %u event comparisons, %u differ; seed=%08x\n",
           s_scenario, s_scenario * 17, mismatch, initial_seed);
    printf("progress enabled=%u disabled=%u closed=%u negative-current=%u\n",
           s_enabled, s_disabled, s_closed, s_wrapped);
    printf("self-checked product-wrap witnesses=%u\n", s_product_witness);
    printf("init success=%u failure=%u; text resource=%u direct=%u\n",
           s_init_success_count, s_init_failure, s_text_resources, s_direct_text);
    {
        uint32_t flags, seen = 0;
        for (flags = 0; flags < 256; flags++)
            if (s_button_flags[flags]) seen++;
        printf("modal low-byte flag combinations reached=%u/256\n", seen);
    }
    printf("caught dependency throws=%u\n", s_throws);
    return mismatch ? 1 : 0;
}
