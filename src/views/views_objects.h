#ifndef FS98_VIEWS_OBJECTS_H
#define FS98_VIEWS_OBJECTS_H

#include "views_internal.h"

/* Import slots identify the precise shared constructor/destructor ABI. */
struct fs98_views_subobject {
    uint16_t offset, constructor, destructor, size;
};
struct fs98_views_object_services {
    void *(*dialog_construct)(void *object, int32_t resource, void *parent);
    void (*dialog_destroy)(void *object);
    void (*construct)(void *object, uint32_t import_slot);
    void (*destroy)(void *object, uint32_t import_slot);
    void *(*allocate)(uint32_t size);
    void (*ui_delete)(void *object);
    void (*mfc_delete)(void *object);
    void (*dialog_delete)(void *object);
    int32_t (*create)(void *object, uint32_t resource, void *parent);
    int32_t (*show)(void *object, int32_t show);
};
void fs98_views_set_object_services(const struct fs98_views_object_services *services,
                                   void *dialog_vtable, void *spin_vtable);
void fs98_views_set_object_module(uint32_t module);
void *FS98_VIEWS_THISCALL fs98_views_construct(void *object, void *parent);
void FS98_VIEWS_THISCALL fs98_views_destroy(void *object);
void *FS98_VIEWS_THISCALL fs98_views_delete(void *object, uint32_t flags);
void *FS98_VIEWS_THISCALL fs98_views_spin_delete(void *object, uint32_t flags);
void FS98_VIEWS_STDCALL fs98_views_class_delete(void *object);
uint32_t FS98_VIEWS_STDCALL fs98_views_create(uint32_t parent, void *descriptor);

#endif
