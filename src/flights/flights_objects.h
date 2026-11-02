#ifndef FS98_FLIGHTS_OBJECTS_H
#define FS98_FLIGHTS_OBJECTS_H
#include "flights_internal.h"

/* A member control: its offset and the RVAs of the import slots that construct and
 * destroy it (SIMSUI's CSimsText, CSimsListBox, ...). */
struct fs98_flights_subobject { uint32_t offset, constructor, destructor; };
struct fs98_flights_class {
    uint32_t size, template, descriptor;
    uint32_t constructor, destructor, delete, factory, vtable;
    uint32_t count;
    const struct fs98_flights_subobject *controls;
};
struct fs98_flights_object_services {
    void *(*dialog_construct)(void *object, int32_t resource, void *parent);
    void (*dialog_destroy)(void *object);
    void (*construct)(void *control, uint32_t import);
    void (*destroy)(void *control, uint32_t import);
    void *(*allocate)(uint32_t size);
    void (*ui_delete)(void *object);
    void (*dialog_delete)(void *object);
    int32_t (*create)(void *object, uint32_t resource, void *parent);
    int32_t (*show)(void *object, int32_t show);
    char *(*copy_text)(char *out, const char *text);
};
/* Class indices, in s_classes order (tools/gen_flights_tables.py) */
enum { FS98_FLIGHTS_ADVENTURES, FS98_FLIGHTS_CHALLENGES, FS98_FLIGHTS_SAVE, FS98_FLIGHTS_COMMS, FS98_FLIGHTS_CREATE, FS98_FLIGHTS_AIRCRAFT, FS98_FLIGHTS_MODELESS, FS98_FLIGHTS_OPENING, FS98_FLIGHTS_CLASS_COUNT };
void fs98_flights_set_object_services(const struct fs98_flights_object_services *services,
                                      void *const *vtables);
/* Flights' own instance, which each factory stores at object +0x80 */
void fs98_flights_set_object_module(uint32_t module);
#define FS98_FLIGHTS_OBJECT(name) \
    void *FS98_FLIGHTS_THISCALL fs98_flights_##name##_construct(void *object, void *parent); \
    void FS98_FLIGHTS_THISCALL fs98_flights_##name##_destroy(void *object); \
    void *FS98_FLIGHTS_THISCALL fs98_flights_##name##_delete(void *object, uint32_t flags); \
    uint32_t FS98_FLIGHTS_STDCALL fs98_flights_##name##_create(uint32_t parent, void *descriptor)
FS98_FLIGHTS_OBJECT(adventures);
FS98_FLIGHTS_OBJECT(challenges);
FS98_FLIGHTS_OBJECT(save);
FS98_FLIGHTS_OBJECT(comms);
FS98_FLIGHTS_OBJECT(create);
FS98_FLIGHTS_OBJECT(aircraft);
FS98_FLIGHTS_OBJECT(modeless);
FS98_FLIGHTS_OBJECT(opening);
#undef FS98_FLIGHTS_OBJECT
#endif
