#ifndef FS98_MPDIALOG_INTERNAL_H
#define FS98_MPDIALOG_INTERNAL_H

#include <stdint.h>

#define FS98_MPDIALOG_THISCALL __attribute__((thiscall))
#define FS98_MPDIALOG_STDCALL __attribute__((stdcall))

struct fs98_mpdialog_import_binding {
    const char *dll;
    const char *name;
    uint16_t ordinal;
    uint16_t reserved;
    uint32_t iat_va;
};

struct fs98_mpdialog_virtual_binding {
    uint16_t slot;
    int16_t import_index;
    void *local;
};

struct fs98_mpdialog_message_entry {
    uint32_t message, code, first, last, signature;
    void *handler;
};

struct fs98_mpdialog_message_map {
    void *(*base)(void);
    const struct fs98_mpdialog_message_entry *entries;
};

struct fs98_mpdialog_dialog {
    uint32_t id;
    uint32_t (FS98_MPDIALOG_STDCALL *create)(uint32_t parent, void *descriptor);
    uint32_t flags;
};

struct fs98_mpdialog_import {
    uint32_t id;
    void *link;
};

struct fs98_mpdialog_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    void *reserved;
    uint32_t flags;
    const struct fs98_mpdialog_dialog *dialogs;
};

struct fs98_mpdialog_owner_block {
    uint32_t result;
    int32_t (FS98_MPDIALOG_STDCALL *read)(void *block);
    int32_t (FS98_MPDIALOG_STDCALL *accept)(void *block);
    int32_t (FS98_MPDIALOG_STDCALL *details)(void *block);
    int32_t (FS98_MPDIALOG_STDCALL *weather)(void *block);
    int32_t (FS98_MPDIALOG_STDCALL *sync)(void *block);
    uint32_t reserved18;
    uint32_t flight_conditions;
    uint32_t reserved20;
    uint32_t adf_lock;
    uint32_t reserved28;
    uint32_t dme_lock;
    uint32_t reserved30;
    uint32_t reserved34;
    uint32_t autopilot_lock;
    uint32_t reserved3c;
    uint32_t send_details;
    uint32_t reserved44;
    uint32_t receive_details;
    uint32_t reserved4c;
    uint32_t player_names;
};

typedef int32_t (FS98_MPDIALOG_STDCALL *fs98_mpdialog_callback)(void *block);

void *fs98_mpdialog_import_va(uint32_t iat_va);

void FS98_MPDIALOG_THISCALL fs98_mpdialog_post_destroy(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_delete(void *object, uint32_t flags);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_exchange(void *object, void *exchange);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_session_init(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_on_init(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_on_ok(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_on_cancel(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_update(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_save(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_host_settings(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_get_conditions(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_set_conditions(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_empty(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_empty_arg(void *object, uint32_t argument);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_hide(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_show(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_message_map(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_details_message_map(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_help(void *object, void *info);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_context(void *object, uint32_t window, uint32_t unused);
uint32_t FS98_MPDIALOG_STDCALL fs98_mpdialog_create(uint32_t parent, void *descriptor);
uint32_t FS98_MPDIALOG_STDCALL fs98_mpdialog_details_create(uint32_t parent, void *descriptor);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_delete_body(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_details_delete(void *object, uint32_t flags);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_post_destroy(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_exchange(void *object, void *exchange);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_init(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_update(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_capture(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_details_validate(void *object, uint32_t control_id);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_details_validation_result(void *object, int32_t result);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_ok(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_cancel(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_remove(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_refresh(void *object, uint32_t wparam, uint32_t lparam);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_selection(void *object);

uint32_t FS98_MPDIALOG_STDCALL fs98_mpdialog_flow_create(uint32_t parent, void *descriptor);
extern void *fs98_mpdialog_flow_vtables[5][62];
extern struct fs98_mpdialog_message_map fs98_mpdialog_maps[7];
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_post_destroy(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_message_map_intro(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_message_map_choice(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_message_map_host(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_message_map_connection(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_message_map_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_exchange_intro(void *object, void *exchange);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_exchange_choice(void *object, void *exchange);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_exchange_host(void *object, void *exchange);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_exchange_connection(void *object, void *exchange);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_exchange_browser(void *object, void *exchange);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_on_init_intro(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_on_init_choice(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_on_init_host(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_on_init_connection(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_on_init_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_body_intro(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_body_choice(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_body_host(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_body_connection(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_body_browser(void *object);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_intro(void *object, uint32_t flags);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_choice(void *object, uint32_t flags);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_host(void *object, uint32_t flags);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_connection(void *object, uint32_t flags);
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_delete_browser(void *object, uint32_t flags);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_next_intro(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_back_intro(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_cancel_intro(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_next_choice(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_back_choice(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_cancel_choice(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_route_standard(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_route_join(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_route_host(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_route_online(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_validate_choice(void *object, uint32_t control_id);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_error_choice(void *object, int32_t field);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_update_intro(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_close(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_capture_intro(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_update_choice(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_capture_choice(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_next_host(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_back_host(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_cancel_host(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_settings_host(void *object);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_validate_host(void *object, uint32_t control_id);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_error_host(void *object, int32_t field);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_update_host(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_capture_host(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_next_connection(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_back_connection(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_cancel_connection(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_provider_connection(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_activate_connection(void *object, uint32_t state, uint32_t window, uint32_t minimized);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_activate_base(void *object, uint32_t state, uint32_t window, uint32_t minimized);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_update_connection(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_capture_connection(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_next_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_back_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_cancel_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_details_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_settings_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_refresh_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_observer_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_select_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_select_type_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_idle_browser(void *object, uint32_t wparam, uint32_t lparam);
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_mouse_activate_browser(void *object, uint32_t active, uint32_t hit_test, uint32_t message);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_show_browser(void *object, uint32_t show, uint32_t status);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_timer_browser(void *object, uint32_t timer_id);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_update_browser(void *object);
void FS98_MPDIALOG_THISCALL fs98_mpdialog_flow_capture_browser(void *object);

#endif
