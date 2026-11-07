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

#endif
