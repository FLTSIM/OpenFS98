#ifndef FS98_HOST_RT_H
#define FS98_HOST_RT_H

#include <stdint.h>

/* FLTSIM98.EXE's runtime -- startup, the window procedures, the timer, the message loop,
 * shutdown -- decompiled (docs/modules/host.md, "The runtime, decompiled"). The 194 services
 * are src/host/host.c; this is the code around them.
 *
 * As SIMSUI's (src/simsui/simsui_internal.h): every import is called through
 * fs98_host_rt_imports, in retail's import order, so a harness can point retail's import
 * slots and ours at the same logging stubs (tools/difftest/hostrtdiff.c); retail's data
 * (.rdata and .data: vtables, message maps, strings, statics) is reached through
 * fs98_host_rt_image at retail's RVAs. A service is called through the service table in that
 * image (0x417ba0), as retail's code calls it directly. Names are retail's addresses: the C for
 * the routine at 0x412210 is fs98_host_rt_412210 until what it does is known well enough to
 * name it, and then that name with its address in the comment above it. */

#define HOST_RT_THISCALL __attribute__((thiscall))
#define HOST_RT_STDCALL __attribute__((stdcall))
#define TC HOST_RT_THISCALL
#define SC HOST_RT_STDCALL

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define I32(object, offset) (*(int32_t *)((uint8_t *)(object) + (offset)))
#define D16(object, offset) (*(uint16_t *)((uint8_t *)(object) + (offset)))
#define D8(object, offset) (*(uint8_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((void *)((uint8_t *)(object) + (offset)))

enum fs98_host_rt_import_index {
#define HOST_RT_IMPORT(index, ident, dll, name, slot, pop, thiscall, arguments) HOST_RT_IMPORT_##ident = index,
#include "host_rt_tables.inc"
#undef HOST_RT_IMPORT
    HOST_RT_IMPORT_COUNT
};

extern void *fs98_host_rt_imports[HOST_RT_IMPORT_COUNT];
extern uint8_t *fs98_host_rt_image;

/* retail's data at a VA (0x41xxxx), as its code names it */
#define VA(va) ((void *)(fs98_host_rt_image + (va) - 0x400000u))
#define G32(va) (*(uint32_t *)VA(va))
#define GI32(va) (*(int32_t *)VA(va))
#define G16(va) (*(uint16_t *)VA(va))
#define G8(va) (*(uint8_t *)VA(va))
#define G_PTR(va) (*(void **)VA(va))
#define STR(va) ((const char *)VA(va))

/* an import, typed at its call: CALL_STD(GetDC, uint32_t, (uint32_t))(window) */
#define CALL_STD(ident, result, parameters) ((result (SC *) parameters)fs98_host_rt_imports[HOST_RT_IMPORT_##ident])
#define CALL_THIS(ident, result, parameters) ((result (TC *) parameters)fs98_host_rt_imports[HOST_RT_IMPORT_##ident])
#define CALL_C(ident, result, parameters) ((result (*) parameters)fs98_host_rt_imports[HOST_RT_IMPORT_##ident])
/* a HOST service, by its offset in the service table */
#define SERVICE(offset, result, parameters) ((result (SC *) parameters)G_PTR(0x417ba0u + (offset)))
/* a virtual call: slot is the byte offset in the vtable */
#define VIRTUAL(object, slot, result, parameters) ((result (TC *) parameters)PTR(PTR(object, 0), slot))

/* rep movsd */
static inline void fs98_host_rt_copy_words(void *to, const void *from, uint32_t count)
{
    uint32_t *out = (uint32_t *)to;
    const uint32_t *in = (const uint32_t *)from;
    while (count--)
        *out++ = *in++;
}

/* src/host/runtime/host_rt_window.c */
struct window_visit;
int32_t SC fs98_host_rt_visit_floating(uint32_t window, struct window_visit *visit);
void SC fs98_host_rt_for_each_window(uint32_t window, int32_t (SC *callback)(uint32_t, uint32_t), uint32_t parameter);

/* src/host/runtime/host_rt_ddraw.c */
struct device_search;
char *SC fs98_host_rt_device_suffix(char *name, char *out, int32_t cut);
int32_t SC fs98_host_rt_device_found(void *guid, char *description, char *driver, struct device_search *search);
uint32_t SC fs98_host_rt_device_create(const char *name, void **object);
void SC fs98_host_rt_display_size(int32_t *width, int32_t *height, int32_t *depth);
int32_t fs98_host_rt_device_3d(void);
int32_t SC fs98_host_rt_tell_window(uint32_t window, uint32_t message);
void fs98_host_rt_leave_full_screen(void);
void fs98_host_rt_release_surfaces(void);
void fs98_host_rt_release_directdraw(void);
int32_t SC fs98_host_rt_d3d_device_found(void *guid, char *description, char *name, uint32_t *hardware,
                                          uint32_t *software, uint32_t *kept);
int32_t fs98_host_rt_d3d_hardware(void);
void SC fs98_host_rt_device_name(char *out, const char *description, const char *driver, const char *name);
int32_t SC fs98_host_rt_device_is(const void *guid, const char *unused, const char *driver);
void SC fs98_host_rt_set_palette(void *entries);
int32_t SC fs98_host_rt_lock(void *surface, void **memory, int32_t *pitch);
int32_t SC fs98_host_rt_unlock(void *surface);
int32_t SC fs98_host_rt_fill(void *surface, void *rect, uint32_t colour);
int32_t SC fs98_host_rt_fill_clipped(void *surface, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t colour);
int32_t SC fs98_host_rt_blit(void *to, int32_t to_x, int32_t to_y, int32_t to_width, int32_t to_height, void *from,
                             int32_t from_x, int32_t from_y, int32_t from_width, int32_t from_height, int32_t keyed);
int32_t fs98_host_rt_open_directdraw(void);
void *SC fs98_host_rt_render_surface(int32_t width, int32_t height);

/* src/host/runtime/host_rt_startup.c */
void fs98_host_rt_413220(void);
void *fs98_host_rt_onexit(void *fn);
int32_t fs98_host_rt_atexit(void *fn);
int32_t SC fs98_host_rt_afx_initialize(int32_t dll, uint32_t version);
void *fs98_host_rt_construct_afx(void);
uint32_t fs98_host_rt_construct_418798(void);
uint32_t fs98_host_rt_destroy_418798(void);
int32_t fs98_host_rt_static_418798(void);
void *fs98_host_rt_401040(void);
void *fs98_host_rt_application_class(void);
uint32_t TC fs98_host_rt_destroy_application(void *application);
void *TC fs98_host_rt_delete_application(void *application, uint32_t flags);
void *fs98_host_rt_construct_application(void);
uint32_t fs98_host_rt_destroy_the_application(void);
int32_t fs98_host_rt_static_application(void);

/* src/host/runtime/host_rt_config.c */
void SC fs98_host_rt_flag_read(uint8_t *settings, uint32_t mask, const char *key, int32_t fallback);
int32_t SC fs98_host_rt_primary_found(void *guid, char *description, char *driver, uint8_t *settings);
int32_t SC fs98_host_rt_read_display(uint8_t *settings, int32_t defaults);

/* src/host/runtime/host_rt_cursor.c */
void fs98_host_rt_move_cursor(void);
void SC fs98_host_rt_draw_cursor(uint32_t region);
void SC fs98_host_rt_frame_rect(uint32_t unused);
void SC fs98_host_rt_copy_8_to_16(uint16_t *to, int32_t to_pitch, const uint8_t *from, int32_t from_pitch,
                                  int32_t width, int32_t rows, const uint16_t *palette);
void SC fs98_host_rt_copy_8_to_16_keyed(uint16_t *to, int32_t to_pitch, const uint8_t *from, int32_t from_pitch,
                                        int32_t width, int32_t rows, uint8_t key, const uint16_t *palette);
struct picture;
void SC fs98_host_rt_draw_piece(uint8_t *window, struct picture *to, const int32_t *point, struct picture *from,
                                const int32_t *part);
void SC fs98_host_rt_draw_floating(uint32_t region);

/* src/host/runtime/host_rt_fullscreen.c */
void fs98_host_rt_make_palettes(void);
void SC fs98_host_rt_full_screen_gdi(int32_t back);
void fs98_host_rt_begin_frame(void);
void fs98_host_rt_end_frame(void);
int32_t SC fs98_host_rt_redraw_part(const int32_t *rect, const int32_t *point);
void SC fs98_host_rt_redraw_window(uint32_t window, int32_t parts);
void SC fs98_host_rt_move_window(uint32_t window, const int32_t *rect);

/* src/host/runtime/host_rt_divide.c */
uint32_t SC fs98_host_rt_sib_address(uint32_t mod, uint32_t scale, uint32_t index, uint32_t base, void **pointers);
int32_t SC fs98_host_rt_divide_filter(void **pointers);
int32_t SC fs98_host_rt_guarded(int32_t (*fn)(void));
int32_t SC fs98_host_rt_divide_handler(void **pointers);

/* src/host/runtime/host_rt_modules.c */
struct module;
struct module *SC fs98_host_rt_module_named(const char *name);
struct module *SC fs98_host_rt_module_with_id(uint32_t id);
struct module *SC fs98_host_rt_module_with_linkage(uint32_t *linkage);
void SC fs98_host_rt_resolve_imports(struct module *module);
void SC fs98_host_rt_release_imports(struct module *module);
uint32_t *SC fs98_host_rt_acquire(struct module *module);
void SC fs98_host_rt_release(struct module *module);
uint32_t *SC fs98_host_rt_acquire_named(const char *name);
uint32_t *SC fs98_host_rt_acquire_id(uint32_t id);
void SC fs98_host_rt_release_id(uint32_t id);
void SC fs98_host_rt_release_linkage(uint32_t *linkage);
void SC fs98_host_rt_register(uint32_t id, uint32_t *records);
int32_t SC fs98_host_rt_add_module(const char *path, int32_t keep);
void fs98_host_rt_add_host(void);
void SC fs98_host_rt_add_directory(const char *directory, const char *pattern, int32_t keep);
void fs98_host_rt_find_modules(void);
void fs98_host_rt_auto_init_modules(void);
int32_t SC fs98_host_rt_order_module(struct module *module, uint32_t **order);
void fs98_host_rt_order_modules(void);
void fs98_host_rt_init_ordered(void);
void fs98_host_rt_deinit_ordered(void);
int32_t SC fs98_host_rt_link_imports(struct module *module, void *unused);
int32_t SC fs98_host_rt_init_module(struct module *module, void *unused);
int32_t SC fs98_host_rt_deinit_unordered(struct module *module, void *unused);
int32_t SC fs98_host_rt_unload_module(struct module *module, void *unused);
void fs98_host_rt_408110(void);
void fs98_host_rt_start_modules(void);
void fs98_host_rt_stop_modules(void);
uint32_t fs98_host_rt_407910(void);
uint32_t fs98_host_rt_409c80(void);

#endif
