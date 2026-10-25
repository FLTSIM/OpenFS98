#ifndef FS98_ALERTS_OBJECTS_H
#define FS98_ALERTS_OBJECTS_H

#include "alerts_internal.h"

#define FS98_ALERTS_STDCALL __attribute__((stdcall))

/* Derived class identities are supplied by the runtime adapter. The object
 * harness pairs these identities with retail; no original code is used by
 * the C constructors or destructors. */
struct fs98_alerts_object_tables {
    void *modal;
    void *modeless;
    void *progress;
    void *advisory;
};

struct fs98_alerts_object_services {
    void *(*dialog_construct)(void *object, int32_t template, void *parent);
    void (*dialog_destroy)(void *object);
    void (*string_construct)(void *object);
    void (*string_destroy)(void *object);
    void (*text_construct)(void *object);
    void (*text_destroy)(void *object);
    void (*button_construct)(void *object);
    void (*button_destroy)(void *object);
    void (*window_construct)(void *object);
    void (*window_destroy)(void *object);
    void (*check_construct)(void *object);
    void (*check_destroy)(void *object);
    void *(*allocate)(uint32_t size);
    void (*ui_delete)(void *object);
    void (*mfc_delete)(void *object);
    int32_t (*create)(void *object, uint32_t template, void *parent);
    int32_t (*show)(void *object, int32_t show);
    int32_t (*enable)(uint32_t window, int32_t enable);
    void (*dialog_delete)(void *object);
};

void fs98_alerts_set_object_services(const struct fs98_alerts_object_services *services,
                                    const struct fs98_alerts_object_tables *tables);
void fs98_alerts_set_object_module(uint32_t module);
void *FS98_ALERTS_THISCALL fs98_alerts_modal_construct(void *object, void *parent);
void *FS98_ALERTS_THISCALL fs98_alerts_modeless_construct(void *object, void *parent);
void *FS98_ALERTS_THISCALL fs98_alerts_advisory_construct(void *object, void *parent);
void FS98_ALERTS_THISCALL fs98_alerts_modal_destroy(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_modeless_destroy(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_destroy(void *object);
void *FS98_ALERTS_THISCALL fs98_alerts_modal_delete(void *object, uint32_t flags);
void *FS98_ALERTS_THISCALL fs98_alerts_modeless_delete(void *object, uint32_t flags);
void *FS98_ALERTS_THISCALL fs98_alerts_advisory_delete(void *object, uint32_t flags);
void *FS98_ALERTS_THISCALL fs98_alerts_progress_delete(void *object, uint32_t flags);
void FS98_ALERTS_THISCALL fs98_alerts_post_destroy(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_disable(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_enable(void *object);
void FS98_ALERTS_THISCALL fs98_alerts_empty_arg(void *object, uint32_t argument);
uint32_t FS98_ALERTS_STDCALL fs98_alerts_modal_create(uint32_t parent, void *descriptor);
uint32_t FS98_ALERTS_STDCALL fs98_alerts_modeless_create(uint32_t parent, void *descriptor);
uint32_t FS98_ALERTS_STDCALL fs98_alerts_advisory_create(uint32_t parent, void *descriptor);

#endif
