#ifndef FS98_OPTIONS_OBJECTS_H
#define FS98_OPTIONS_OBJECTS_H
#include "options_internal.h"

/* A member control: its offset and the RVAs of the import slots that construct and
 * destroy it (SIMSUI's CSimsText, CSims3PieceButton, ...). */
struct fs98_options_subobject { uint32_t offset, constructor, destructor; };
struct fs98_options_class {
    uint32_t size, template, descriptor;     /* descriptor 0: the factory stores none */
    uint32_t constructor, destructor, delete, factory, vtable;
    uint32_t count;
    const struct fs98_options_subobject *controls;
    uint32_t page;              /* built on CSimsTabPage rather than CSimsDialog */
};
struct fs98_options_object_services {
    void *(*dialog_construct)(void *object, int32_t resource, void *parent);
    void (*dialog_destroy)(void *object);
    void (*construct)(void *control, uint32_t import);
    void (*destroy)(void *control, uint32_t import);
    void *(*allocate)(uint32_t size);
    void (*ui_delete)(void *object);
    void (*dialog_delete)(void *object);
    int32_t (*create)(void *object, uint32_t resource, void *parent);
    int32_t (*show)(void *object, int32_t show);
    void *(*page_construct)(void *object, int32_t resource, void *parent);   /* CSimsTabPage */
    void (*page_destroy)(void *object);
    char *(*copy_text)(char *out, const char *text);                         /* lstrcpyA */
    void (*string_assign)(void *string, const char *text);                   /* CString::operator= */
    int32_t (*message_box)(const char *text, uint32_t type, uint32_t help);  /* AfxMessageBox */
};
/* Class indices, in s_classes order (tools/gen_options_tables.py) */
enum { FS98_OPTIONS_VIDEO_FINISHED, FS98_OPTIONS_VIDEO_MESSAGE, FS98_OPTIONS_SAVE_VIDEO,
       FS98_OPTIONS_FLIGHT_VIDEO, FS98_OPTIONS_ADD_LOGBOOK, FS98_OPTIONS_LOGBOOK_ENTRY,
       FS98_OPTIONS_BOOK, FS98_OPTIONS_LANDING, FS98_OPTIONS_MANOEUVRE, FS98_OPTIONS_ANALYSIS,
       FS98_OPTIONS_REPLAY, FS98_OPTIONS_DEMO,
       FS98_OPTIONS_DISPLAY, FS98_OPTIONS_DISPLAY_AIRCRAFT, FS98_OPTIONS_DISPLAY_MAP,
       FS98_OPTIONS_DISPLAY_SCENERY, FS98_OPTIONS_DISPLAY_HARDWARE,
       FS98_OPTIONS_PREFS, FS98_OPTIONS_PREFS_GENERAL, FS98_OPTIONS_PREFS_INSTRUMENT,
       FS98_OPTIONS_PREFS_INTERNATIONAL, FS98_OPTIONS_PREFS_DISPLAY, FS98_OPTIONS_PREFS_SOUND,
       FS98_OPTIONS_PREFS_FLIGHT, FS98_OPTIONS_CLASS_COUNT };
void fs98_options_set_object_services(const struct fs98_options_object_services *services,
                                      void *const *vtables);
/* Options' own instance, which each factory stores at object +0x80 */
void fs98_options_set_object_module(uint32_t module);
/* the Logbook's rows list: its class's vtable, stored over CSimsListBox's */
void fs98_options_set_object_list_vtable(void *vtable);
/* SIMSUI's CSimsSpinButtonCtrl vtable, a data import: Instant replay builds two inline */
void fs98_options_set_spin_vtable(void *vtable);
#define FS98_OPTIONS_OBJECT(name) \
    void *FS98_OPTIONS_THISCALL fs98_options_##name##_construct(void *object, void *parent); \
    void FS98_OPTIONS_THISCALL fs98_options_##name##_destroy(void *object); \
    void *FS98_OPTIONS_THISCALL fs98_options_##name##_delete(void *object, uint32_t flags); \
    uint32_t FS98_OPTIONS_STDCALL fs98_options_##name##_create(uint32_t parent, void *descriptor)
FS98_OPTIONS_OBJECT(video_finished);
FS98_OPTIONS_OBJECT(video_message);
FS98_OPTIONS_OBJECT(save_video);
FS98_OPTIONS_OBJECT(flight_video);
FS98_OPTIONS_OBJECT(add_logbook);
FS98_OPTIONS_OBJECT(logbook_entry);
FS98_OPTIONS_OBJECT(book);
FS98_OPTIONS_OBJECT(landing);
FS98_OPTIONS_OBJECT(manoeuvre);
FS98_OPTIONS_OBJECT(analysis);
FS98_OPTIONS_OBJECT(replay);
FS98_OPTIONS_OBJECT(demo);
FS98_OPTIONS_OBJECT(display);
FS98_OPTIONS_OBJECT(display_aircraft);
FS98_OPTIONS_OBJECT(display_map);
FS98_OPTIONS_OBJECT(display_scenery);
FS98_OPTIONS_OBJECT(display_hardware);
FS98_OPTIONS_OBJECT(prefs);
FS98_OPTIONS_OBJECT(prefs_general);
FS98_OPTIONS_OBJECT(prefs_instrument);
FS98_OPTIONS_OBJECT(prefs_international);
FS98_OPTIONS_OBJECT(prefs_display);
FS98_OPTIONS_OBJECT(prefs_sound);
FS98_OPTIONS_OBJECT(prefs_flight);
#undef FS98_OPTIONS_OBJECT
#endif
