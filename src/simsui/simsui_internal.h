#ifndef FS98_SIMSUI_INTERNAL_H
#define FS98_SIMSUI_INTERNAL_H

#include <stdint.h>

/* SIMSUI.DLL, FS98's widget library, preferred base 0x20470000 (docs/modules/simsui.md).
 *
 * Offsets are the retail PE32 object layouts, which every English dialog DLL depends on.
 * Every import is called through fs98_simsui_imports, in retail's import order, so a
 * harness can point retail's import slots and ours at the same logging stubs
 * (tools/difftest/simsuidiff.c). Retail's data (vtables, message maps, strings, statics)
 * is reached through fs98_simsui_image at retail's RVAs: in the harness it is retail's
 * mapped image, so both sides share one copy; in the native DLL it is our own image of
 * those sections. */

#define FS98_SIMSUI_THISCALL __attribute__((thiscall))
#define FS98_SIMSUI_STDCALL __attribute__((stdcall))
#define FS98_SIMSUI_NOINLINE __attribute__((noinline))
#define TC FS98_SIMSUI_THISCALL
#define SC FS98_SIMSUI_STDCALL

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define I32(object, offset) (*(int32_t *)((uint8_t *)(object) + (offset)))
#define D16(object, offset) (*(uint16_t *)((uint8_t *)(object) + (offset)))
#define D8(object, offset) (*(uint8_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((void *)((uint8_t *)(object) + (offset)))
/* the stores before it done before what follows (a division that may fault, as retail's
 * idiv does after the stores it makes first) */
#define FS98_SIMSUI_BARRIER() __asm__ volatile("" ::: "memory")

enum fs98_simsui_import_index {
#define SIMSUI_IMPORT(index, ident, dll, name, slot, pop, thiscall, arguments) SIMSUI_IMPORT_##ident = index,
#include "simsui_tables.inc"
#undef SIMSUI_IMPORT
    SIMSUI_IMPORT_COUNT
};

extern void *fs98_simsui_imports[SIMSUI_IMPORT_COUNT];
extern uint8_t *fs98_simsui_image;

/* retail's data at an RVA: a vtable, a message map, a string, a static */
#define IMAGE(rva) ((void *)(fs98_simsui_image + (rva)))
#define G32(rva) (*(uint32_t *)(fs98_simsui_image + (rva)))
#define G_PTR(rva) (*(void **)(fs98_simsui_image + (rva)))

/* an import, typed at its call: CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 0x26) */
#define CALL_STD(ident, result, parameters) ((result (SC *) parameters)fs98_simsui_imports[SIMSUI_IMPORT_##ident])
#define CALL_THIS(ident, result, parameters) ((result (TC *) parameters)fs98_simsui_imports[SIMSUI_IMPORT_##ident])
#define CALL_C(ident, result, parameters) ((result (*) parameters)fs98_simsui_imports[SIMSUI_IMPORT_##ident])
/* a virtual call: slot is the byte offset in the vtable */
#define VIRTUAL(object, slot, result, parameters) ((result (TC *) parameters)PTR(PTR(object, 0), slot))

/* MFC42's ordinals SIMSUI uses, by what they are */
#define mfc_operator_new(size) CALL_C(MFC_823, void *, (uint32_t))(size)
#define mfc_operator_delete(block) CALL_C(MFC_825, void, (void *))(block)
#define mfc_gdi_delete_object(object) CALL_THIS(MFC_2414, int32_t, (void *))(object)
#define mfc_wnd_construct(window) CALL_THIS(MFC_567, void *, (void *))(window)

struct fs98_simsui_rect { int32_t left, top, right, bottom; };

/* src/simsui/simsui_crt.c */
void SC fs98_simsui_vector_destroy(void *array, uint32_t size, int32_t count, void (TC *destroy)(void *));
void SC fs98_simsui_vector_construct(void *array, uint32_t size, int32_t count, void *(TC *construct)(void *),
                                     void (TC *destroy)(void *));

/* src/simsui/simsui_util.c */
void *fs98_simsui_ui_new(uint32_t size);
void fs98_simsui_ui_delete(void *block);
char *fs98_simsui_find_char(char *text, int32_t wanted);
char *fs98_simsui_next_token(char *text, char *out, char separator);
int32_t fs98_simsui_parse_point(const char *spec, void *name, int32_t *point);
int32_t fs98_simsui_parse_point_and(const char *spec, void *name, int32_t *point, int32_t *value);
int32_t fs98_simsui_parse_rect(const char *spec, void *name, int32_t *rect);
int32_t fs98_simsui_parse_rect_and(const char *spec, void *name, int32_t *rect, int32_t *value);
int32_t fs98_simsui_parse_font_spec(uint32_t id, void *name, int32_t *a, uint8_t *b, int32_t *c);

/* src/simsui/simsui_fonts.c */
int32_t SC fs98_simsui_create_font(const char *face, int32_t height, uint32_t charset, int32_t weight);
int32_t SC fs98_simsui_add_font(uint32_t id);
void SC fs98_simsui_clear_fonts(void);
void *SC fs98_simsui_base_font(int32_t index);
void fs98_simsui_exit_cleanly(void);
void fs98_simsui_append(char *text, const char *more);
void *TC fs98_simsui_remap_dialog_construct(void *dialog, void *parent);
void *TC fs98_simsui_remap_dialog_delete(void *dialog, uint32_t flags);
const void *TC fs98_simsui_remap_dialog_message_map(const void *dialog);
int32_t TC fs98_simsui_remap_dialog_init(void *dialog);
void SC fs98_simsui_remap_rect(int32_t *rect, void *control);
void SC fs98_simsui_remap_point(int32_t *point, void *control);
void SC fs98_simsui_remap_point_width(int32_t *point, int32_t *width, void *control);
void SC fs98_simsui_font_free(void *font);
void *TC fs98_simsui_font_delete(void *font, uint32_t flags);
int32_t SC fs98_simsui_dll_main(uint32_t instance, uint32_t reason, void *reserved);
int32_t SC fs98_simsui_raw_dll_main(uint32_t instance, uint32_t reason, void *reserved);

/* src/simsui/simsui_dib.c */
void TC fs98_simsui_pal_set_color_table(void *pal, void *dc);
void *TC fs98_simsui_pal_construct(void *pal);
void TC fs98_simsui_nothing(void *object);
void TC fs98_simsui_pal_destroy(void *pal);
void *TC fs98_simsui_object_delete(void *object, uint32_t flags);
void *TC fs98_simsui_gdi_object_delete(void *object, uint32_t flags);
void *TC fs98_simsui_pal_delete(void *pal, uint32_t flags);
void *TC fs98_simsui_dib_bitmap(void *dib);
uint32_t TC fs98_simsui_dib_handle(void *dib);
void *TC fs98_simsui_dib_bits(void *dib);
int32_t TC fs98_simsui_dib_height(void *dib);
int32_t TC fs98_simsui_dib_bmi_height(void *dib);
int32_t TC fs98_simsui_dib_width(void *dib);
int32_t TC fs98_simsui_dib_width_bytes(void *dib);
int32_t TC fs98_simsui_dib_colour_count(void *dib);
void *TC fs98_simsui_dib_info(void *dib);
int32_t TC fs98_simsui_dib_is_pointer_valid(void *dib, const uint8_t *pointer);
int32_t SC fs98_simsui_info_is_header(const void *info);
int32_t SC fs98_simsui_info_colour_count(const void *info);
int32_t TC fs98_simsui_pal_create(void *pal, const void *info);
int32_t TC fs98_simsui_pal_colour_count(void *pal);
void TC fs98_simsui_pal_draw(void *pal, void *dc, const int32_t *rect, int32_t background);
int32_t TC fs98_simsui_pal_set_static_colours(void *pal);
void fs98_simsui_copy(void *to, const void *from, uint32_t size);
void fs98_simsui_fill(void *to, uint8_t value, uint32_t size);
void TC fs98_simsui_pal_init_tables(void *pal, const uint8_t *entries);
int32_t TC fs98_simsui_pal_nearest(void *pal, uint32_t colour, const uint8_t *entries);

/* src/simsui/simsui_dibsection.c */
void *TC fs98_simsui_dib_construct(void *dib);
void TC fs98_simsui_dib_destroy(void *dib);
void *fs98_simsui_delete_object(void *object, uint32_t flags, uint32_t size, void (TC *destroy)(void *),
                                void (*free_one)(void *));
void fs98_simsui_mfc_free(void *object);
void *TC fs98_simsui_dib_delete(void *dib, uint32_t flags);
int32_t TC fs98_simsui_dib_create(void *dib, uint8_t *info, void *pal, uint32_t usage);
int32_t TC fs98_simsui_dib_load(void *dib, int32_t id, uint32_t instance, void *pal, uint32_t flags);
void TC fs98_simsui_dib_fill_rect(void *dib, struct fs98_simsui_rect rect, uint32_t colour);
void TC fs98_simsui_dib_fill(void *dib, uint32_t colour);
int32_t TC fs98_simsui_dib_clip(void *self, void *dest, void *source, int32_t *dest_x, int32_t *dest_y,
                                int32_t *source_x, int32_t *source_y, int32_t *width, int32_t *height);
void TC fs98_simsui_dib_sprite(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                               int32_t source_x, int32_t source_y, int32_t key);
void TC fs98_simsui_dib_sprite_rect(void *dib, void *dest, const int32_t *rect, int32_t key);
void TC fs98_simsui_dib_sprite_at(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                                  int32_t key);
void TC fs98_simsui_dib_sprite_from(void *dib, void *dest, const int32_t *rect, const int32_t *point, int32_t key);
void TC fs98_simsui_dib_mask(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                             int32_t source_x, int32_t source_y, uint32_t colour, int32_t key, int32_t all);
void TC fs98_simsui_dib_copy(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                             int32_t source_x, int32_t source_y);
void TC fs98_simsui_dib_copy_rect(void *dib, void *dest, const int32_t *rect);
void TC fs98_simsui_dib_copy_at(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height);
void TC fs98_simsui_dib_copy_from(void *dib, void *dest, const int32_t *rect, const int32_t *point);
void TC fs98_simsui_dib_tile(void *dib, void *dest);
void TC fs98_simsui_dib_blit(void *dib, void *dc, int32_t x, int32_t y, int32_t width, int32_t height,
                             int32_t source_x, int32_t source_y, uint32_t rop);
void TC fs98_simsui_dib_blit_rect(void *dib, void *dc, const int32_t *rect, uint32_t rop);
void TC fs98_simsui_dib_blit_at(void *dib, void *dc, int32_t x, int32_t y, int32_t width, int32_t height,
                                uint32_t rop);
void TC fs98_simsui_dib_blit_from(void *dib, void *dc, const int32_t *rect, const int32_t *point, uint32_t rop);
int32_t TC fs98_simsui_dib_unused_colour(void *dib);
int32_t TC fs98_simsui_dib_blur_shadow(void *dib, void *mask, int32_t row, void *pal);

/* src/simsui/simsui_small.c */
void *SC fs98_simsui_class_new(uint32_t size);
void SC fs98_simsui_class_free(void *block);
void *TC fs98_simsui_notify_default(void *notify);
uint32_t TC fs98_simsui_picture_char(void *picture);
uint32_t TC fs98_simsui_picture_repeat(void *picture);
uint32_t TC fs98_simsui_picture_flags(void *picture);
int32_t TC fs98_simsui_radio_is_checked(void *button);
int32_t TC fs98_simsui_button_height(void *button);
int32_t TC fs98_simsui_button_width(void *button);
int32_t TC fs98_simsui_check_is_checked(void *button);
void TC fs98_simsui_check_toggle(void *button);
void TC fs98_simsui_nothing_with(void *object, uint32_t argument);
void TC fs98_simsui_window_disable(void *window);
void TC fs98_simsui_window_enable(void *window);
void *TC fs98_simsui_spin_construct(void *control);
void TC fs98_simsui_spin_destroy(void *control);
void *TC fs98_simsui_spin_delete(void *control, uint32_t flags);
void *TC fs98_simsui_scroll_construct(void *control);
void TC fs98_simsui_scroll_destroy(void *control);
void *TC fs98_simsui_scroll_delete(void *control, uint32_t flags);
void *TC fs98_simsui_static_construct(void *control);
void TC fs98_simsui_static_destroy(void *control);
void *TC fs98_simsui_static_delete(void *control, uint32_t flags);
void TC fs98_simsui_control_init(void *control);
void TC fs98_simsui_edit_set_limit(void *edit, int32_t limit);
void TC fs98_simsui_dialog_set_tabs(void *dialog, void *tabs);
void *TC fs98_simsui_dialog_tabs(void *dialog);
void TC fs98_simsui_safe_set_focus(void *window, uint32_t focus);
int32_t TC fs98_simsui_dialog_last_edit(void *dialog);
int32_t TC fs98_simsui_no_error(void *dialog, int32_t code);
void *TC fs98_simsui_dialog_pal(void *dialog);
void *TC fs98_simsui_dialog_background(void *dialog);
void *TC fs98_simsui_dialog_tab_dib(void *dialog);
void *TC fs98_simsui_page_pal(void *page);
void *TC fs98_simsui_page_background(void *page);
void *TC fs98_simsui_notify_construct(void *notify, uint32_t mask, uint32_t beep);
void TC fs98_simsui_notify_set(void *notify, uint32_t mask, uint32_t beep);
uint32_t TC fs98_simsui_notify_mask(void *notify);
void TC fs98_simsui_notify_destroy(void *notify);
void *TC fs98_simsui_notify_delete(void *notify, uint32_t flags);
int32_t fs98_simsui_notify_send(void *notify, const char *title, const char *format, ...);
void fs98_simsui_fonts_static(void);
void fs98_simsui_fonts_static_end(void);
void *fs98_simsui_onexit(void (*handler)(void));
int32_t fs98_simsui_atexit(void (*handler)(void));

/* src/simsui/simsui_font.c */
void TC fs98_simsui_font_defaults(void *font);
void *TC fs98_simsui_font_construct(void *font);
void *TC fs98_simsui_font_construct_with(void *font, void *base);
void TC fs98_simsui_font_destroy(void *font);
void *TC fs98_simsui_sims_font_delete(void *font, uint32_t flags);
int32_t TC fs98_simsui_font_tab_stops(void *font, int32_t count, const uint32_t *stops);
void TC fs98_simsui_font_draw_text(void *font, void *dc, uint32_t text, struct fs98_simsui_rect rect, int32_t state);


/* src/simsui/simsui_button.c */
void *TC fs98_simsui_button_construct(void *button);
void TC fs98_simsui_button_destroy(void *button);
void *TC fs98_simsui_button_delete(void *button, uint32_t flags);
void *TC fs98_simsui_cbutton_delete(void *button, uint32_t flags);
const void *SC fs98_simsui_slider_base_message_map(void);
int32_t TC fs98_simsui_button_init(void *button, int32_t id, void *pal, uint32_t instance);
void TC fs98_simsui_button_draw_item(void *button, const uint8_t *item);
void *SC fs98_simsui_3piece_base_class(void);
const void *TC fs98_simsui_3piece_runtime_class(const void *button);
const void *TC fs98_simsui_3piece_message_map(const void *button);
void *TC fs98_simsui_3piece_construct(void *button);
void TC fs98_simsui_3piece_destroy(void *button);
void *TC fs98_simsui_3piece_delete(void *button, uint32_t flags);
int32_t TC fs98_simsui_3piece_init(void *button, void *font, void *hot_font, int32_t id, void *background,
                                   void *pal, uint32_t flags, int32_t key, uint32_t instance, int32_t hot_x,
                                   int32_t hot_y, struct fs98_simsui_rect text_rect);
void TC fs98_simsui_3piece_button_down(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_3piece_button_up(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_3piece_mouse_move(void *button, uint32_t keys, int32_t x, int32_t y);
uint32_t TC fs98_simsui_3piece_create_menu(void *button, uint32_t instance);
int32_t TC fs98_simsui_3piece_append_menu(void *button, int16_t id, uint32_t string, int16_t flags);
uint32_t TC fs98_simsui_3piece_check_menu(void *button, int16_t item, int16_t check);
void TC fs98_simsui_3piece_delete_menu(void *button);
void TC fs98_simsui_3piece_track_menu(void *button);
void TC fs98_simsui_3piece_kill_focus(void *button, void *window);
uint32_t TC fs98_simsui_3piece_dialog_code(void *button);
uint32_t TC fs98_simsui_default_3(void *window, uint32_t a, uint32_t b, uint32_t c);
void *TC fs98_simsui_help_construct(void *button);
void TC fs98_simsui_help_destroy(void *button);
void *TC fs98_simsui_help_delete(void *button, uint32_t flags);
const void *TC fs98_simsui_help_message_map(const void *button);
uint32_t TC fs98_simsui_hit_client(void *window, int32_t x, int32_t y);
int32_t TC fs98_simsui_help_init(void *button, void *pal, int32_t x, int32_t y);
void TC fs98_simsui_help_button_up(void *button, uint32_t keys, int32_t x, int32_t y);
void *TC fs98_simsui_exit_construct(void *button);
void TC fs98_simsui_exit_destroy(void *button);
void *TC fs98_simsui_exit_delete(void *button, uint32_t flags);
const void *TC fs98_simsui_exit_message_map(const void *button);
int32_t TC fs98_simsui_exit_init(void *button, void *pal, int32_t x, int32_t y);
void TC fs98_simsui_exit_button_up(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_small_button_set_focus(void *button, void *previous);


/* src/simsui/simsui_check.c */
void *TC fs98_simsui_check_construct(void *button);
void TC fs98_simsui_check_destroy(void *button);
void *TC fs98_simsui_check_delete(void *button, uint32_t flags);
const void *TC fs98_simsui_check_message_map(const void *button);
int32_t TC fs98_simsui_check_init(void *button, void *font, int32_t id, void *background, void *pal, uint32_t instance);
void TC fs98_simsui_check_draw_item(void *button, const uint8_t *item);
int32_t TC fs98_simsui_check_set(void *button, int32_t checked);
int32_t TC fs98_simsui_erase_nothing(void *window, void *dc);
void TC fs98_simsui_check_button_up(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_check_button_down(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_check_double_click(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_check_key_up(void *button, uint32_t key, uint32_t repeat, uint32_t flags);
void TC fs98_simsui_check_kill_focus(void *button, void *window);
int32_t TC fs98_simsui_check_checked(void *button, uint32_t wparam, uint32_t lparam);


/* src/simsui/simsui_combo.c */
void TC fs98_simsui_text_window_text(void *text, void *out);
void *TC fs98_simsui_rect_copy(void *rect, const void *from);
void TC fs98_simsui_gdi_object_destroy(void *object);
void *TC fs98_simsui_combo_construct(void *combo);
void TC fs98_simsui_combo_destroy(void *combo);
void *TC fs98_simsui_combo_delete(void *combo, uint32_t flags);
void TC fs98_simsui_combo_init(void *combo, void *font, void *text);
void TC fs98_simsui_combo_draw_item(void *combo, const uint8_t *item);
int32_t TC fs98_simsui_combo_compare(void *combo, const uint8_t *compare);
const void *TC fs98_simsui_combo_message_map(const void *combo);
int32_t TC fs98_simsui_combo_erase(void *combo, void *dc);
void TC fs98_simsui_combo_measure(void *combo, uint8_t *measure);


/* src/simsui/simsui_dialog.c */
void *SC fs98_simsui_dialog_base_class(void);
void *TC fs98_simsui_dialog_construct(void *dialog, uint32_t id, void *parent);
void TC fs98_simsui_dialog_destroy(void *dialog);
void *TC fs98_simsui_dialog_delete(void *dialog, uint32_t flags);
const void *SC fs98_simsui_dialog_base_message_map(void);
uint32_t TC fs98_simsui_dialog_default_id(void *dialog, uint32_t wparam, uint32_t lparam);
uint32_t TC fs98_simsui_safe_focus(void *window, uint32_t wparam, uint32_t lparam);
uint32_t TC fs98_simsui_dialog_parent(void *dialog, uint32_t wparam, uint32_t window);
void TC fs98_simsui_dialog_close(void *dialog);
void TC fs98_simsui_dialog_draw_palette(void *dialog);
int32_t TC fs98_simsui_dialog_init(void *dialog, int32_t id, uint32_t instance, uint32_t flags);
int32_t TC fs98_simsui_dialog_on_init(void *dialog);
int32_t TC fs98_simsui_dialog_erase(void *dialog, void *dc);
void TC fs98_simsui_dialog_measure(void *dialog, int32_t id, uint8_t *measure);
void TC fs98_simsui_dialog_sys_exit(void *dialog);
void TC fs98_simsui_dialog_sys_help(void *dialog);
int32_t TC fs98_simsui_dialog_query_palette(void *dialog);
void TC fs98_simsui_palette_changed(void *window, void *focus);
uint32_t TC fs98_simsui_dialog_hit_test(void *dialog, int32_t x, int32_t y);
int32_t TC fs98_simsui_dialog_error(void *dialog, void *control, uint32_t code);
uint32_t TC fs98_simsui_dialog_edit_set_focus(void *dialog, uint32_t id, uint32_t lparam);
uint32_t TC fs98_simsui_dialog_edit_kill_focus(void *dialog, uint32_t id, uint32_t lparam);
int32_t TC fs98_simsui_dialog_validate(void *dialog, int32_t id);
void TC fs98_simsui_dialog_parent_notify(void *dialog, uint32_t message, uint32_t lparam);
void TC fs98_simsui_dialog_post_kill_focus(void *dialog, uint32_t id);
const void *TC fs98_simsui_dialog_runtime_class(const void *dialog);
void TC fs98_simsui_dialog_exchange(void *dialog, void *exchange);
const void *TC fs98_simsui_dialog_message_map(const void *dialog);
void TC fs98_simsui_dialog_set_validate(void *dialog, uint32_t value);
void TC fs98_simsui_dialog_set_edit(void *dialog, uint32_t value);
void TC fs98_simsui_dialog_set_clicked(void *dialog, uint32_t value);
int32_t TC fs98_simsui_dialog_init_tab(void *dialog, int32_t id, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t instance);

/* src/simsui/simsui_tabs.c */
void *TC fs98_simsui_tabs_active_page(const void *tabs);
void *TC fs98_simsui_helper_construct(void *helper);
void TC fs98_simsui_helper_destroy(void *helper);
void *TC fs98_simsui_helper_delete(void *helper, uint32_t flags);
void TC fs98_simsui_helper_init(void *helper, void *tabs);
const void *TC fs98_simsui_helper_message_map(const void *helper);
void TC fs98_simsui_helper_draw_item(void *helper, int32_t id, void *item);
void TC fs98_simsui_helper_button_down(void *helper, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_helper_sys_key_up(void *helper, uint32_t key, uint32_t repeat, uint32_t flags);
const void *TC fs98_simsui_page_runtime_class(const void *page);
void *TC fs98_simsui_page_construct(void *page, uint32_t id, void *owner);
void TC fs98_simsui_page_destroy(void *page);
void *TC fs98_simsui_page_delete(void *page, uint32_t flags);
int32_t TC fs98_simsui_page_init(void *page, int32_t narrower);
int32_t TC fs98_simsui_page_init_with(void *page, int32_t id, void *background, void *pal, uint32_t instance);
void TC fs98_simsui_page_exchange(void *page, void *exchange);
const void *TC fs98_simsui_page_message_map(const void *page);
int32_t TC fs98_simsui_page_on_init(void *page);
int32_t TC fs98_simsui_page_query_palette(void *page);
void TC fs98_simsui_page_paint(void *page);
void TC fs98_simsui_page_activate(void *page, uint32_t state, void *other, int32_t minimised);
int32_t TC fs98_simsui_page_erase(void *page, void *dc);
uint32_t TC fs98_simsui_page_dialog_code(void *page);
void TC fs98_simsui_page_post_destroy(void *page);
int32_t TC fs98_simsui_page_pre_translate(void *page, const uint32_t *message);
int32_t SC fs98_simsui_find_focusable(uint32_t window, uint32_t *found);
void *TC fs98_simsui_tab_entry_construct(void *entry);
void TC fs98_simsui_tab_entry_destroy(void *entry);
void *TC fs98_simsui_tabs_construct(void *tabs);
void TC fs98_simsui_tabs_destroy(void *tabs);
void *TC fs98_simsui_tabs_delete(void *tabs, uint32_t flags);
void TC fs98_simsui_tabs_enable_page(void *tabs, void *page, int32_t enable);
int32_t TC fs98_simsui_tabs_init(void *tabs, void *dialog, void *font, int32_t id, void *background, void *pal, uint32_t colour, uint32_t current, int32_t shadow, int32_t page_x, int32_t page_y, int32_t unused, uint32_t instance, uint32_t flags);
void TC fs98_simsui_tabs_add_page(void *tabs, void *page, const char *title, int32_t disabled);
int32_t TC fs98_simsui_tabs_set_active_page(void *tabs, void *page);
int32_t TC fs98_simsui_tabs_set_last_page(void *tabs, void *page);
int32_t TC fs98_simsui_tabs_active_tab(const void *tabs);
int32_t TC fs98_simsui_tabs_set_active_tab(void *tabs, int32_t index, uint32_t focus);
const void *TC fs98_simsui_tabs_message_map(const void *tabs);
void TC fs98_simsui_tabs_on_draw_item(void *tabs, int32_t id, void *item);
void TC fs98_simsui_tabs_draw_item(void *tabs, const uint8_t *item);
void TC fs98_simsui_tabs_button_down(void *tabs, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_tabs_button_up(void *tabs, uint32_t keys, int32_t x, int32_t y);
int32_t TC fs98_simsui_tabs_erase(void *tabs, void *dc);
uint32_t TC fs98_simsui_tabs_dialog_code(void *tabs);
void TC fs98_simsui_tabs_nc_paint(void *tabs);
void TC fs98_simsui_tabs_key_down(void *tabs, uint32_t key, uint32_t repeat, uint32_t flags);
void TC fs98_simsui_tabs_next(void *tabs);
void TC fs98_simsui_tabs_previous(void *tabs);
void TC fs98_simsui_tabs_close(void *tabs);
void TC fs98_simsui_tabs_sys_key_up(void *tabs, uint32_t key, uint32_t repeat, uint32_t flags);


/* src/simsui/simsui_edit.c */
void TC fs98_simsui_gdi_object_destroy_too(void *object);
void *TC fs98_simsui_brush_delete(void *brush, uint32_t flags);
void *TC fs98_simsui_edit_construct(void *edit);
void TC fs98_simsui_edit_destroy(void *edit);
void *TC fs98_simsui_edit_delete(void *edit, uint32_t flags);
const void *TC fs98_simsui_edit_message_map(const void *edit);
void TC fs98_simsui_edit_init(void *edit, void *font, uint32_t back, uint32_t text_colour, uint32_t fourth, void *text, uint32_t flags);
uint32_t TC fs98_simsui_edit_colour(void *edit, void *dc, uint32_t type);
void TC fs98_simsui_edit_char(void *edit, uint32_t key, uint32_t repeat, uint32_t flags);
void TC fs98_simsui_edit_set_focus(void *edit, void *previous);
void TC fs98_simsui_edit_kill_focus(void *edit, void *next);


/* src/simsui/simsui_groupbox.c */
void *TC fs98_simsui_groupbox_construct(void *box);
void TC fs98_simsui_groupbox_destroy(void *box);
void *TC fs98_simsui_groupbox_delete(void *box, uint32_t flags);
void *TC fs98_simsui_cstatic_delete(void *window, uint32_t flags);
const void *TC fs98_simsui_groupbox_message_map(const void *box);
void TC fs98_simsui_groupbox_init(void *box, void *font);
void TC fs98_simsui_groupbox_paint(void *box);


/* src/simsui/simsui_listbox.c */
void *TC fs98_simsui_list_construct(void *box);
void TC fs98_simsui_list_destroy(void *box);
void *TC fs98_simsui_list_delete(void *box, uint32_t flags);
const void *SC fs98_simsui_button_base_message_map(void);
const void *TC fs98_simsui_list_message_map(const void *box);
uint32_t TC fs98_simsui_help_to_parent(void *window, uint32_t wparam, uint32_t lparam);
void TC fs98_simsui_list_init(void *box, void *font, void *text);
void TC fs98_simsui_list_draw_item(void *box, const uint8_t *item);
int32_t TC fs98_simsui_list_compare(void *box, const uint8_t *compare);
int32_t TC fs98_simsui_check_list_erase(void *box, void *dc);
void *TC fs98_simsui_check_list_construct(void *box);
void TC fs98_simsui_check_list_destroy(void *box);
void *TC fs98_simsui_check_list_delete(void *box, uint32_t flags);
const void *SC fs98_simsui_check_list_base_message_map(void);
const void *TC fs98_simsui_check_list_message_map(const void *box);
void TC fs98_simsui_check_list_init(void *box, void *font, void *text);
void TC fs98_simsui_check_list_draw_item(void *box, const uint8_t *item);
int32_t TC fs98_simsui_check_list_compare(void *box, const uint8_t *compare);


/* src/simsui/simsui_picture.c */
void *TC fs98_simsui_picture_construct(void *picture);
void TC fs98_simsui_picture_destroy(void *picture);
void *TC fs98_simsui_picture_delete(void *picture, uint32_t flags);
void TC fs98_simsui_picture_set_image(void *picture, int32_t id, void *background, void *pal, uint32_t instance, uint16_t key);
int32_t TC fs98_simsui_picture_init(void *picture, int32_t id, void *background, void *pal, uint32_t instance, uint16_t key);
const void *TC fs98_simsui_picture_message_map(const void *picture);
void TC fs98_simsui_picture_on_char(void *picture, uint32_t key, uint32_t repeat, uint32_t flags);
void TC fs98_simsui_picture_key_down(void *picture, uint32_t key, uint32_t repeat, uint32_t flags);
void TC fs98_simsui_picture_set_focus(void *picture, void *previous);
void TC fs98_simsui_picture_kill_focus(void *picture, void *next);
int32_t TC fs98_simsui_picture_erase(void *picture, void *dc);
void TC fs98_simsui_picture_paint(void *picture);
void *TC fs98_simsui_view_construct(void *view);
void TC fs98_simsui_view_destroy(void *view);
void *TC fs98_simsui_view_delete(void *view, uint32_t flags);
const void *TC fs98_simsui_view_message_map(const void *view);


/* src/simsui/simsui_radio.c */
void *TC fs98_simsui_radio_construct(void *button);
void TC fs98_simsui_radio_destroy(void *button);
void *TC fs98_simsui_radio_delete(void *button, uint32_t flags);
const void *TC fs98_simsui_radio_message_map(const void *button);
int32_t TC fs98_simsui_radio_init(void *button, void *font, int32_t id, void *background, void *pal, uint32_t instance);
void TC fs98_simsui_radio_draw_item(void *button, const uint8_t *item);
int32_t TC fs98_simsui_radio_set(void *button, int32_t checked);
void TC fs98_simsui_radio_undo(void *button);
void TC fs98_simsui_radio_button_down(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_radio_button_up(void *button, uint32_t keys, int32_t x, int32_t y);
uint32_t TC fs98_simsui_radio_checked(void *button, uint32_t wparam, uint32_t lparam);
void TC fs98_simsui_radio_key_up(void *button, uint32_t key, uint32_t repeat, uint32_t flags);
void TC fs98_simsui_radio_kill_focus(void *button, void *next);


/* src/simsui/simsui_shadowbutton.c */
void *TC fs98_simsui_shadow_button_construct(void *button);
void TC fs98_simsui_shadow_button_destroy(void *button);
void *TC fs98_simsui_shadow_button_delete(void *button, uint32_t flags);
const void *TC fs98_simsui_shadow_button_message_map(const void *button);
int32_t TC fs98_simsui_shadow_button_init(void *button, void *font, void *hot_font, int32_t id, void *background, void *pal, uint32_t flags, int32_t key, uint32_t instance, int32_t hot_x, int32_t hot_y, struct fs98_simsui_rect text_rect);
void TC fs98_simsui_shadow_button_down(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_shadow_button_up(void *button, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_shadow_button_move(void *button, uint32_t keys, int32_t x, int32_t y);


/* src/simsui/simsui_shadow.c */
void *TC fs98_simsui_shadow_construct(void *shadow);
void TC fs98_simsui_shadow_destroy(void *shadow);
void *TC fs98_simsui_shadow_delete(void *shadow, uint32_t flags);
void TC fs98_simsui_shadow_init(void *shadow, int32_t id, void *background, void *pal, uint32_t instance, int32_t x, int32_t y, uint32_t flags, int32_t shadow_x, int32_t shadow_y);
int32_t TC fs98_simsui_shadow_draw(void *shadow, void *dc, int32_t x, int32_t y);
void *TC fs98_simsui_shadow_buffer(void *shadow);


/* src/simsui/simsui_slider.c */
void *TC fs98_simsui_slider_construct(void *slider);
void TC fs98_simsui_slider_destroy(void *slider);
void *TC fs98_simsui_slider_delete(void *slider, uint32_t flags);
const void *TC fs98_simsui_slider_message_map(const void *slider);
int32_t TC fs98_simsui_slider_init(void *slider, int32_t track_id, int32_t thumb_id, void *background, void *pal, int32_t margin, int32_t tick_top, int32_t tick_height, int32_t positions, int32_t no_snap, int32_t key, uint32_t instance);
void TC fs98_simsui_slider_button_down(void *slider, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_slider_button_up(void *slider, uint32_t keys, int32_t x, int32_t y);
void TC fs98_simsui_slider_move(void *slider, uint32_t keys, int32_t x, int32_t y);
uint32_t TC fs98_simsui_slider_key_down(void *slider, uint32_t key, uint32_t lparam);
int32_t TC fs98_simsui_slider_position(const void *slider);
int32_t TC fs98_simsui_slider_set_position(void *slider, int32_t position);
void TC fs98_simsui_slider_set_range(void *slider, int32_t first, int32_t last);
uint32_t TC fs98_simsui_slider_hit_test(void *slider, int32_t x, int32_t y);
void TC fs98_simsui_slider_set_thumb(void *slider, int32_t offset);
void TC fs98_simsui_slider_draw_item(void *slider, const uint8_t *item);
int32_t TC fs98_simsui_slider_erase(void *slider, void *dc);
uint32_t TC fs98_simsui_slider_dialog_code(void *slider);
void TC fs98_simsui_slider_nc_paint(void *slider);
void TC fs98_simsui_slider_kill_focus(void *slider, void *next);
uint32_t TC fs98_simsui_slider_clicked(void *slider, uint32_t part, uint32_t lparam);


/* src/simsui/simsui_text.c */
const void *TC fs98_simsui_static_message_map(const void *window);
const void *TC fs98_simsui_spin_message_map(const void *window);
void *TC fs98_simsui_text_construct(void *text);
void TC fs98_simsui_text_destroy(void *text);
void *TC fs98_simsui_text_delete(void *text, uint32_t flags);
const void *TC fs98_simsui_text_message_map(const void *text);
void TC fs98_simsui_text_init(void *text, void *font);
void TC fs98_simsui_text_set_window_text(void *text, const char *string);
int32_t TC fs98_simsui_text_get_window_text(void *text, char *buffer, int32_t room);
void TC fs98_simsui_text_paint(void *text);


/* src/simsui/simsui_textart.c */
void *TC fs98_simsui_text_art_construct(void *art);
void TC fs98_simsui_text_art_destroy(void *art);
void *TC fs98_simsui_text_art_delete(void *art, uint32_t flags);
const void *TC fs98_simsui_text_art_message_map(const void *art);
int32_t TC fs98_simsui_text_art_init(void *art, uint32_t spec_id, void *background, void *pal, uint32_t flags);
void TC fs98_simsui_text_art_paint(void *art);

#endif
