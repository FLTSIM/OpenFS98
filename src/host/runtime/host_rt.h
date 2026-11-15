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

#ifndef HOST_RT_STANDALONE
/* retail's data at a VA (0x41xxxx), as its code names it */
#define VA(va) ((void *)(fs98_host_rt_image + (va) - 0x400000u))
/* an import's address */
#define IMPORT(ident) fs98_host_rt_imports[HOST_RT_IMPORT_##ident]
/* a HOST service, by its offset in the service table */
#define SERVICE(offset, result, parameters) ((result (SC *) parameters)G_PTR(0x417ba0u + (offset)))
#else
/* The routines the services share with the runtime (host_rt_ddraw.c, host_rt_config.c,
 * host_rt_modules.c, host_rt_display.c, host_rt_common.c), compiled for a build of the services
 * without the runtime (src/host/host.c alone: HOST.DLL for hostdiff, the lifted executable).
 * There, .data is host.c's host_image and .rdata the generated copy (host.c's too); an import
 * is called through that build's own import table, read at the call, so a harness that hooks
 * the table sees it; a service through host.c's table, since host_image's 0x417ba0 keeps
 * retail's addresses. */
#define HOST_RT_DIRECT(ident, symbol) extern void *const fs98_host_rt_direct_##ident __asm__(symbol);
#include "host_rt_tables.inc"
#undef HOST_RT_DIRECT
extern uint8_t fs98_host_rt_data[] __asm__("_host_image");
extern const uint8_t fs98_host_rt_rdata[];
extern void *HostTable[];
#define VA(va) ((void *)((uint32_t)(va) < 0x417000u ? (uint8_t *)fs98_host_rt_rdata + ((uint32_t)(va) - 0x416000u) \
                                                   : fs98_host_rt_data + ((uint32_t)(va) - 0x417000u)))
#define IMPORT(ident) ((void *)fs98_host_rt_direct_##ident)
#define SERVICE(offset, result, parameters) ((result (SC *) parameters)HostTable[(offset) / 4u])
#endif
#define G32(va) (*(uint32_t *)VA(va))
#define GI32(va) (*(int32_t *)VA(va))
#define G16(va) (*(uint16_t *)VA(va))
#define G8(va) (*(uint8_t *)VA(va))
#define G_PTR(va) (*(void **)VA(va))
#define STR(va) ((const char *)VA(va))

/* an import, typed at its call: CALL_STD(GetDC, uint32_t, (uint32_t))(window) */
#define CALL_STD(ident, result, parameters) ((result (SC *) parameters)IMPORT(ident))
#define CALL_THIS(ident, result, parameters) ((result (TC *) parameters)IMPORT(ident))
#define CALL_C(ident, result, parameters) ((result (*) parameters)IMPORT(ident))
/* a virtual call: slot is the byte offset in the vtable */
#define VIRTUAL(object, slot, result, parameters) ((result (TC *) parameters)PTR(PTR(object, 0), slot))
/* a COM method: stdcall, the object first */
#define COM_METHOD(object, slot, result, parameters) ((result (SC *) parameters)PTR(PTR(object, 0), slot))

/* rep movsd, as retail's (not memcpy: a fault part way must leave the same words written) */
static inline void fs98_host_rt_copy_words(void *to, const void *from, uint32_t count)
{
    __asm__ volatile("cld\n\trep movsl" : "+D"(to), "+S"(from), "+c"(count) : : "memory");
}

/* rep stosd then rep stosb: count bytes of a byte value, as retail's inlined memset */
static inline void fs98_host_rt_fill_bytes(void *to, uint8_t value, uint32_t count)
{
    uint32_t words = count >> 2, rest = count & 3, pattern = value * 0x01010101u;
    __asm__ volatile("cld\n\trep stosl\n\tmovl %3, %%ecx\n\trep stosb"
                     : "+D"(to), "+c"(words) : "a"(pattern), "r"(rest) : "memory");
}

/* src/host/runtime/host_rt_window.c */
struct window_visit;
int32_t SC fs98_host_rt_menu_length(const uint32_t *entries);
void SC fs98_host_rt_popup_menu(uint8_t *window, const int32_t *point, int32_t kind);

/* src/host/runtime/host_rt_ddraw.c */
struct device_search;
char *SC fs98_host_rt_device_suffix(char *name, char *out, int32_t cut);
int32_t SC fs98_host_rt_device_found(void *guid, char *description, char *driver, struct device_search *search);
uint32_t SC fs98_host_rt_device_create(const char *name, void **object);
void SC fs98_host_rt_display_size(int32_t *width, int32_t *height, int32_t *depth);
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
int32_t SC fs98_host_rt_fill(void *surface, void *rect, uint32_t colour);
int32_t fs98_host_rt_open_directdraw(void);

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
int32_t fs98_host_rt_guard_depth(void);

/* src/host/runtime/host_rt_modules.c */
struct module;
struct module *SC fs98_host_rt_module_named(const char *name);
struct module *SC fs98_host_rt_module_with_id(uint32_t id);
struct module *SC fs98_host_rt_module_with_linkage(uint32_t *linkage);
void SC fs98_host_rt_resolve_imports(struct module *module);
void SC fs98_host_rt_release_imports(struct module *module);
uint32_t *SC fs98_host_rt_acquire(struct module *module);
void SC fs98_host_rt_release(struct module *module);
uint32_t *SC fs98_host_rt_acquire_id(uint32_t id);
void SC fs98_host_rt_release_id(uint32_t id);
void SC fs98_host_rt_register(uint32_t id, uint32_t *records);
int32_t SC fs98_host_rt_add_module(const char *path, int32_t keep);

/* src/host/runtime/host_rt_main.c */
extern void (*fs98_host_rt_loop_pass)(void);
int32_t fs98_host_rt_application_init(void);
int32_t TC fs98_host_rt_application_exit(void *application);
int32_t TC fs98_host_rt_application_run(void *application);
void SC fs98_host_rt_report(uint32_t message);
void fs98_host_rt_new_configuration(void);
int32_t SC fs98_host_rt_startup(uint32_t instance);
void fs98_host_rt_shut_down(void);
int32_t fs98_host_rt_load_language(void);
int32_t SC fs98_host_rt_401f60(uint32_t unused);
int32_t fs98_host_rt_cpu_speed(void);
void SC fs98_host_rt_tell_running(uint32_t window);
int32_t SC fs98_host_rt_logo_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l);
int32_t SC fs98_host_rt_show_logo(uint32_t unused);
int32_t fs98_host_rt_init_instance(void);
int32_t fs98_host_rt_exit_instance(void);
int32_t fs98_host_rt_run(void);
void SC fs98_host_rt_timer(uint32_t id, uint32_t message, uint32_t user, uint32_t one, uint32_t two);
void SC fs98_host_rt_save_placement(uint32_t window);
uint32_t fs98_host_rt_covered(void);
void fs98_host_rt_4025b0(void);
void SC fs98_host_rt_4025c0(uint32_t unused);

/* src/host/runtime/host_rt_frame.c */
int32_t SC fs98_host_rt_help(uint32_t window, const char *file, uint32_t command, uint32_t data);
int32_t SC fs98_host_rt_message_help(const uint32_t *info);
int32_t SC fs98_host_rt_tell_resized(uint32_t window, uint32_t unused);
int32_t SC fs98_host_rt_tell_size_move_ended(uint32_t window, uint32_t unused);
int32_t SC fs98_host_rt_tell_moved(uint32_t window, uint32_t unused);
int32_t SC fs98_host_rt_activate_floating(uint32_t window, uint32_t active);
int32_t SC fs98_host_rt_recheck_topmost(uint32_t window, uint32_t unused);
void SC fs98_host_rt_copied(uint32_t unused, const uint32_t *data);
void SC fs98_host_rt_place_bitmap(uint8_t *window);
void SC fs98_host_rt_release_clipper(uint8_t *window);
int32_t SC fs98_host_rt_command(uint32_t window, uint32_t id, uint32_t code, uint32_t parameter);
int32_t SC fs98_host_rt_main_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l);

/* src/host/runtime/host_rt_child.c */
int32_t SC fs98_host_rt_child_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l);

/* src/host/runtime/host_rt_floating.c */
int32_t SC fs98_host_rt_floating_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l);
int32_t SC fs98_host_rt_dialog_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l);

/* src/host/runtime/host_rt_layout.c */
void SC fs98_host_rt_dock_place(uint8_t *data);
void SC fs98_host_rt_dock_units(uint8_t *data);
void SC fs98_host_rt_align(uint8_t *data);
int32_t SC fs98_host_rt_draw_piece_gdi(int32_t *piece, uint8_t *data);
int32_t SC fs98_host_rt_grow_shared(uint8_t **entry, uint8_t *shared);
int32_t SC fs98_host_rt_take_shared(uint8_t **entry, uint8_t *shared);
int32_t SC fs98_host_rt_tell_owner(uint8_t *entry, uint8_t *data);
void SC fs98_host_rt_window_region(uint8_t *data);
void SC fs98_host_rt_make_picture(uint8_t *data, uint32_t unused);

/* src/host/runtime/host_rt_mouse.c */
struct mouse_context;
uint32_t SC fs98_host_rt_hot_spot(uint8_t *element, struct mouse_context *context);
int32_t SC fs98_host_rt_window_mouse(uint32_t window, uint32_t message, uint32_t w, uint32_t l, const int32_t *point,
                                     uint8_t *data, uint8_t *bitmap);

/* src/host/runtime/host_rt_misc.c */
void fs98_host_rt_40cc10(void);
void fs98_host_rt_40ee40(void);
void fs98_host_rt_410400(void);
struct key_press;
uint32_t SC fs98_host_rt_key_table(uint32_t *table, const struct key_press *press);
int32_t SC fs98_host_rt_keys(uint32_t key, uint32_t scan, uint32_t modifiers, uint32_t extended);
void fs98_host_rt_hold_menu_bar(void);
void fs98_host_rt_draw_menu_bar(void);
void SC fs98_host_rt_colour_table(uint8_t *data);
void SC fs98_host_rt_dither(uint8_t *to, int32_t to_pitch, const uint8_t *from, int32_t from_pitch, int32_t left,
                            int32_t right, int32_t top, int32_t bottom);
void SC fs98_host_rt_gdi_blit(uint32_t dc, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t from,
                              int32_t from_x, int32_t from_y, int32_t from_width, int32_t from_height, uint32_t flags);
void fs98_host_rt_sound_setup(void);
void fs98_host_rt_sound_end(void);
void fs98_host_rt_sound_tick(void);

/* src/host/runtime/host_rt_surface.c */
int32_t fs98_host_rt_device_3d(void);
int32_t SC fs98_host_rt_unlock(void *surface);
int32_t SC fs98_host_rt_fill_clipped(void *surface, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t colour);
int32_t SC fs98_host_rt_blit(void *to, int32_t to_x, int32_t to_y, int32_t to_width, int32_t to_height, void *from,
                             int32_t from_x, int32_t from_y, int32_t from_width, int32_t from_height, int32_t keyed);
void *SC fs98_host_rt_render_surface(int32_t width, int32_t height);

/* src/host/runtime/host_rt_display.c */
void SC fs98_host_rt_full_screen_gdi(int32_t back);
int32_t SC fs98_host_rt_set_display_mode(uint8_t *mode);
int32_t SC fs98_host_rt_change_display(uint8_t *mode, uint32_t flags);
int32_t fs98_host_rt_full_screen_on(void);
int32_t fs98_host_rt_full_screen_off(void);

/* src/host/runtime/host_rt_common.c */
int32_t SC fs98_host_rt_visit_floating(uint32_t window, struct window_visit *visit);
void SC fs98_host_rt_for_each_window(uint32_t window, int32_t (SC *callback)(uint32_t, uint32_t), uint32_t parameter);
int32_t SC fs98_host_rt_hook(uint32_t message, uint32_t parameter);
uint32_t SC fs98_host_rt_modeless_message(uint32_t *entry, void *message);
int32_t SC fs98_host_rt_modeless_took(void *message);
void fs98_host_rt_tint_palette(void);
void SC fs98_host_rt_cube(int32_t on);
void SC fs98_host_rt_pause_look(int32_t on);
void fs98_host_rt_start_sound(void);
void fs98_host_rt_restack(void);

/* src/host/runtime/host_rt_module_load.c */
uint32_t *SC fs98_host_rt_acquire_named(const char *name);
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
