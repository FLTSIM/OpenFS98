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

#endif
