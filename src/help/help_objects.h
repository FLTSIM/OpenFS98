#ifndef FS98_HELP_OBJECTS_H
#define FS98_HELP_OBJECTS_H
#include "help_internal.h"

struct fs98_help_subobject { uint16_t offset, constructor, destructor; };
struct fs98_help_class {
    uint16_t size, template, descriptor;
    uint16_t constructor, destructor, delete, factory, vtable;
    uint16_t count;
    const struct fs98_help_subobject *controls;
};
struct fs98_help_object_services {
    void *(*dialog_construct)(void *object, int32_t resource, void *parent);
    void (*dialog_destroy)(void *object);
    void (*construct)(void *control, uint32_t import);
    void (*destroy)(void *control, uint32_t import);
    void *(*allocate)(uint32_t size);
    void (*ui_delete)(void *object);
    void (*mfc_delete)(void *object);
    void (*dialog_delete)(void *object);
    int32_t (*create)(void *object, uint32_t resource, void *parent);
    int32_t (*show)(void *object, int32_t show);
};
void fs98_help_set_object_services(const struct fs98_help_object_services *services,
                                   void *const *vtables, void *placeholder_vtable);
void fs98_help_set_object_module(uint32_t module);
#define FS98_HELP_OBJECT(name) \
    void *FS98_HELP_THISCALL fs98_help_##name##_construct(void *object, void *parent); \
    void FS98_HELP_THISCALL fs98_help_##name##_destroy(void *object); \
    void *FS98_HELP_THISCALL fs98_help_##name##_delete(void *object, uint32_t flags); \
    uint32_t FS98_HELP_STDCALL fs98_help_##name##_create(uint32_t parent, void *descriptor)
FS98_HELP_OBJECT(about);
FS98_HELP_OBJECT(credits);
FS98_HELP_OBJECT(lessons);
FS98_HELP_OBJECT(graph);
FS98_HELP_OBJECT(completion);
#undef FS98_HELP_OBJECT
void *FS98_HELP_THISCALL fs98_help_placeholder_delete(void *object, uint32_t flags);
void FS98_HELP_STDCALL fs98_help_class_delete(void *object);
#endif
