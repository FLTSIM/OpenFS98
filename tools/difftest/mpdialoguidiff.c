/* Native differential for isolated MP_DIALOG 0x110a, 0x110b and 0x110c slices.
 * Retail and replacement share CD MFC42/SIMSUI and USER32 controls. HOST
 * idle dispatch and MULTIP-owned callbacks/storage are explicit test models. */
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/mpdialog/mpdialog_internal.h"

#ifndef UDM_GETRANGE32
#define UDM_GETRANGE32 (WM_USER + 106)
#endif

#define TC FS98_MPDIALOG_THISCALL
#define SC FS98_MPDIALOG_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))

struct fs98_test_import { uint32_t id; void *link; };
struct fs98_test_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
struct fs98_test_linkage {
    uint32_t id; void (*init)(void); void (*deinit)(void); void *reserved;
    uint32_t flags; struct fs98_test_dialog *dialogs;
};

#define FS98_FLOW_MAX_CHILD_CONTROLS 96

struct fs98_flow_child_control {
    uint32_t id, style, exstyle, enabled, style_visible, effective_visible;
    int32_t x, y, width, height;
    char class_name[48];
    char text[128];
};

struct fs98_callback_event {
    uint32_t id, arg_is_block, result, reserved;
    uint32_t value, status_pilot, status_observer;
};
struct fs98_result {
    uint32_t initial_checks[6];
    uint32_t initial_buttons[3][2];
    uint32_t initial_button_styles[3];
    uint32_t initial_button_parent_visible[3];
    uint32_t create_button_styles[2];
    uint32_t create_parent_visible[2];
    uint32_t create_seen[2];
    uint32_t member_button_ids[3];
    uint32_t dialog_visible_after_create;
    uint32_t descriptor_visibility[2];
    uint32_t descriptor_after_init[2];
    uint32_t direct_update_buttons[3][2];
    uint32_t direct_update_button_styles[3];
    uint32_t direct_update_parent_visible[3];
    uint32_t after_checks[6];
    uint32_t descriptor[0x54 / 4];
    struct fs98_callback_event callbacks[8];
    uint32_t callback_count, validate_count, post_count, delete_count;
    uint32_t nc_destroy_count, nc_destroy_order, post_order, delete_order;
    uint32_t host_init_count, host_release_count, hwnd_state;
    uint32_t native_child_count, native_child_overflow;
    struct fs98_flow_child_control native_children[FS98_FLOW_MAX_CHILD_CONTROLS];
};

static uint8_t s_host_services[0x340];
static uint8_t s_global[0x1400];
static uint8_t s_application[0x400];
static HMODULE s_mfc, s_ui, s_current_module;
static uint32_t s_side, s_case, s_callback_count, s_host_init[2], s_host_release[2];
static uint32_t s_details_active;
static uint32_t s_callback_returns[5];
static struct fs98_mpdialog_owner_block s_blocks[2];
static struct fs98_callback_event s_callbacks[2][8];
static struct fs98_result s_results[2];
static void *s_validate_original, *s_post_original, *s_delete_original;
static void **s_validate_slot, **s_post_slot, **s_delete_slot;
static void *s_test_owner_block[2];
static void *s_test_dialog[2];
static WNDPROC s_previous_wndproc;
static uint32_t s_lifecycle_tick;
static HHOOK s_create_hook;
static HHOOK s_flow_modal_hook;
static HHOOK s_flow_callwnd_hook, s_flow_callwndret_hook;
static HHOOK s_geometry_cbt_hook, s_geometry_callwnd_hook;
static uint32_t s_geometry_trace_active, s_geometry_trace_order;
static uint32_t s_flow_modal_seen[2];
static HWND s_flow_saved_main_window;
static void *s_flow_saved_main_cwnd;
static void **s_flow_showwindow_slot[2];
static const int s_visibility_ids[2] = {1003, 1006};
static uint32_t s_create_styles[2][2], s_create_parents[2][2], s_create_counts[2][2];

struct fs98_details_result {
    uint32_t mode, block_result, visible, hwnd_state;
    uint32_t ok_enabled, cancel_enabled, close_enabled, remove_enabled;
    uint32_t button_visible[4], button_members[4], focus_after_validation;
    uint32_t list_count, selected, row_ids[4], opaque_bc, opaque_d0;
    uint32_t initial_buttons[4], initial_visible[4], initial_list_count;
    uint32_t initial_selected, initial_row_ids[4], initial_ranges[4];
    uint32_t initial_opaque_bc, initial_opaque_d0, opaque_changed;
    uint32_t pilot_range[2], observer_range[2];
    char rows[4][32], pilot[32], observer[32], session[32];
    char initial_rows[4][32], initial_pilot[32], initial_observer[32], initial_session[32];
    uint32_t callbacks, callback_ids[16], callback_args[16], callback_results[16];
    uint32_t callback_values[16], pilot_status, observer_status;
    uint32_t refresh_updates, removal_confirmed, removed_player;
    uint32_t nc_destroy_count, post_count, delete_count;
    uint32_t nc_order, post_order, delete_order, owner_cleanup_order;
    uint32_t native_child_count, native_child_overflow;
    struct fs98_flow_child_control native_children[FS98_FLOW_MAX_CHILD_CONTROLS];
};

struct fs98_flow_event {
    uint32_t id, owner_argument, result, value;
    uint32_t mode, provider, selection, status_callsign, status_name, status_pilot, status_observer;
};

struct fs98_flow_timer_event {
    uint32_t action, page, id, interval, callback_present, result;
};

struct fs98_flow_trace {
    uint32_t kind, page, message, wparam, lparam;
    uint32_t focus_id, focus_page, active_page, foreground_page;
    uint32_t visible_mask, depth, site;
};

#define FS98_FLOW_MAX_RENDERED_CONTROLS 97

struct fs98_flow_control_render {
    uint32_t id, order, width, height, hash, painted_bytes;
    uint32_t style, exstyle, enabled, style_visible, effective_visible, focused;
    uint32_t font_valid, button_state, list_items_hash;
    int32_t check_state, selection;
    int32_t list_count, list_top, list_caret, list_anchor, list_selection_count;
    LOGFONTA font;
    char class_name[48], text[128];
    char list_items[4][64];
};

struct fs98_flow_render_capture {
    uint32_t width, height, hash, painted_bytes, control_count;
    struct fs98_flow_control_render controls[FS98_FLOW_MAX_RENDERED_CONTROLS];
};

struct fs98_flow_result {
    uint32_t array_valid, posts, post_message, post_wparam, post_lparam;
    uint32_t before_host_mask, after_host_show_mask, after_initial_mask;
    uint32_t checkpoints[24], checkpoint_count, controls[5];
    uint32_t child_control_count[5], child_control_overflow[5];
    struct fs98_flow_child_control child_controls[5][FS98_FLOW_MAX_CHILD_CONTROLS];
    uint32_t descriptor_result, always, mode, provider, transient, observer, browser_selection;
    uint32_t status_callsign, status_name, status_pilot, status_observer;
    uint32_t validation_count, validation_focus[4], blocked_next[4];
    uint32_t callback_count, callback_overflow;
    struct fs98_flow_event callbacks[24];
    char callsign[32], session[64], pilot[16], observer_text[16];
    uint32_t idle_ticks, accepted_hidden_mask, hwnd_alive_mask, hwnd_alive_after_mask;
    uint32_t browser_timer_count, browser_timer_overflow, browser_timer_dispatches;
    uint32_t browser_refresh_callbacks, browser_nested_refresh_attempts;
    struct fs98_flow_timer_event browser_timers[16];
    uint32_t nc_destroy[5], post_destroy[5], deleting_dtor[5];
    uint32_t nc_order[5], post_order[5], delete_order[5], delete_flags[5];
    uint32_t error_count[5], error_field[5];
    uint32_t owner_cleanup_order, lifetime_ok;
};

static uint8_t s_flow_blocks[2][0xbc];
static uint32_t s_flow_providers[2][65][7];
static char s_flow_provider_names[2][65][32];
static uint8_t s_flow_rows[2][2][2][84];
static char s_flow_session_names[2][2][32];
static char s_flow_callsigns[2][32], s_flow_host_names[2][64];
static char s_flow_pilot_caps[2][16], s_flow_observer_caps[2][16];
static char s_flow_limits[2][4][8];
static struct fs98_flow_event s_flow_events[2][24];
static struct fs98_flow_result s_flow_results[2];
static uint32_t s_flow_event_count[2], s_flow_provider_failures[2];
static uint32_t s_flow_refresh_reentry_pending[2];
static uint32_t s_flow_accept_result[2], s_flow_owner_active[2], s_flow_idle_ticks[2];
static HWND *s_flow_windows[2];
static void *s_flow_dialog_objects[2][5];
static void **s_flow_post_slots[2][5], **s_flow_delete_slots[2][5];
static void **s_flow_close_slots[2][5];
static void *s_flow_post_originals[2][5], *s_flow_delete_originals[2][5];
static void *s_flow_close_originals[2][5];
static void **s_flow_error_slots[2][5];
static void *s_flow_error_originals[2][5];
static struct fs98_mpdialog_message_entry *s_flow_activate_entry[2];
static void *s_flow_activate_original[2];
static struct fs98_mpdialog_message_entry *s_flow_base_activate_entry[2][5];
static void *s_flow_base_activate_original[2][5];
static void **s_flow_oncommand_slots[2][5];
static void *s_flow_oncommand_originals[2][5];
static void **s_flow_connection_next_slots[2];
static void *s_flow_connection_next_originals[2];
static struct fs98_mpdialog_message_entry *s_flow_provider_entry[2];
static void *s_flow_provider_original[2];
static struct fs98_mpdialog_message_entry *s_flow_browser_timer_entry[2];
static void *s_flow_browser_timer_original[2];
static void **s_flow_validate_slots[2];
static void *s_flow_validate_original[2];
static WNDPROC s_flow_previous_proc[2][5];
static WNDPROC s_flow_mfc_proc[2][5];
static WNDPROC s_flow_main_previous_proc;
static struct fs98_flow_trace s_flow_trace[2][512];
static uint32_t s_flow_trace_count[2], s_flow_dispatch_depth[2];
static uint32_t s_flow_lifecycle_tick, s_flow_active;
static uint32_t s_flow_child_inventory_pairs, s_flow_child_inventory_total_pairs;
static uint32_t s_session_child_tree_matches, s_session_child_tree_total;
static uint32_t s_details_child_tree_matches, s_details_child_tree_total;
static uint32_t s_session_child_tree_differences, s_details_child_tree_differences;
static uint32_t s_flow_render_pairs, s_flow_render_total_pairs;
static uint32_t s_flow_render_page_pairs, s_flow_render_page_total_pairs;
static uint32_t s_session_render_pairs, s_details_render_pairs;
static uint32_t s_session_render_total, s_details_render_total;
static uint32_t s_render_repeat_total, s_render_repeat_matches;
static uint32_t s_render_repeat_differences;
static uint32_t s_flow_rendered_page_mask;
static struct fs98_flow_render_capture s_flow_render_results[2][5];
static struct fs98_flow_render_capture s_flow_render_repeat[2][5];
static struct fs98_flow_render_capture s_session_render_results[2];
static struct fs98_flow_render_capture s_session_render_repeat[2];
static struct fs98_flow_render_capture s_details_render_results[2];
static struct fs98_flow_render_capture s_details_render_repeat[2];
static uint32_t s_render_only;
static uint32_t s_cleanup_flow_lifetime_leaks;
static int32_t s_flow_mutation_case = -1;
static int32_t s_session_mutation_case = -1;
static int32_t s_details_mutation_case = -1;
static HWND s_main_window;
static BOOL (WINAPI *s_flow_showwindow_original[2])(HWND, int);
static UINT_PTR (WINAPI *s_flow_settimer_original[2])(HWND, UINT_PTR, UINT, TIMERPROC);
static BOOL (WINAPI *s_flow_killtimer_original[2])(HWND, UINT_PTR);
static void **s_flow_settimer_slot[2], **s_flow_killtimer_slot[2];

static void fs98_fail(const char *message);
static void fs98_pump(void);
static int32_t fs98_flow_page_for_object(void *object);
static int32_t fs98_flow_page_for_hwnd(uint32_t side, HWND window);
static uint32_t fs98_flow_visible_mask(uint32_t side);
static void TC fs98_flow_provider_map_probe(void *object);
static void TC fs98_flow_browser_timer_probe(void *object, uint32_t timer_id);

static void **fs98_find_import_slot(HMODULE module, const char *dll_name,
                                    const char *symbol_name)
{
    uint8_t *base = (uint8_t *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_DATA_DIRECTORY directory;
    IMAGE_IMPORT_DESCRIPTOR *descriptor;
    uint32_t descriptor_index;
    if (!module || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return 0;
    directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress || !directory.Size) return 0;
    descriptor = (IMAGE_IMPORT_DESCRIPTOR *)(base + directory.VirtualAddress);
    for (descriptor_index = 0; descriptor_index < directory.Size / sizeof(*descriptor) &&
         descriptor[descriptor_index].Name; ++descriptor_index) {
        IMAGE_THUNK_DATA32 *names;
        IMAGE_THUNK_DATA32 *iat;
        uint32_t thunk_index;
        const char *import_dll = (const char *)(base + descriptor[descriptor_index].Name);
        if (lstrcmpiA(import_dll, dll_name) != 0) continue;
        names = (IMAGE_THUNK_DATA32 *)(base +
            (descriptor[descriptor_index].OriginalFirstThunk ?
                descriptor[descriptor_index].OriginalFirstThunk :
                descriptor[descriptor_index].FirstThunk));
        iat = (IMAGE_THUNK_DATA32 *)(base + descriptor[descriptor_index].FirstThunk);
        for (thunk_index = 0; names[thunk_index].u1.AddressOfData; ++thunk_index) {
            IMAGE_IMPORT_BY_NAME *import_name;
            if (IMAGE_SNAP_BY_ORDINAL32(names[thunk_index].u1.Ordinal)) continue;
            import_name = (IMAGE_IMPORT_BY_NAME *)(base + names[thunk_index].u1.AddressOfData);
            if (lstrcmpA((const char *)import_name->Name, symbol_name) == 0)
                return (void **)&iat[thunk_index].u1.Function;
        }
    }
    return 0;
}

static void fs98_flow_record_browser_timer(uint32_t action, HWND window,
                                           UINT_PTR id, UINT interval,
                                           TIMERPROC callback, UINT_PTR result)
{
    struct fs98_flow_result *flow_result = &s_flow_results[s_side];
    struct fs98_flow_timer_event *event;
    if (!s_flow_active || !s_flow_windows[s_side] ||
        window != s_flow_windows[s_side][4]) return;
    if (flow_result->browser_timer_count >=
        sizeof(flow_result->browser_timers) / sizeof(flow_result->browser_timers[0])) {
        flow_result->browser_timer_overflow = 1;
        return;
    }
    event = &flow_result->browser_timers[flow_result->browser_timer_count++];
    event->action = action;
    event->page = 4;
    event->id = (uint32_t)id;
    event->interval = interval;
    event->callback_present = callback != 0;
    event->result = (uint32_t)result;
}

static UINT_PTR WINAPI fs98_flow_settimer_probe(HWND window, UINT_PTR id,
                                                 UINT interval, TIMERPROC callback)
{
    UINT_PTR result = s_flow_settimer_original[s_side](window, id, interval, callback);
    fs98_flow_record_browser_timer(1, window, id, interval, callback, result);
    return result;
}

static BOOL WINAPI fs98_flow_killtimer_probe(HWND window, UINT_PTR id)
{
    BOOL result = s_flow_killtimer_original[s_side](window, id);
    fs98_flow_record_browser_timer(2, window, id, 0, 0, (UINT_PTR)result);
    return result;
}

static void fs98_flow_patch_timer_imports(uint32_t side, HMODULE module)
{
    DWORD previous;
    s_flow_settimer_slot[side] = fs98_find_import_slot(module, "USER32.dll", "SetTimer");
    s_flow_killtimer_slot[side] = fs98_find_import_slot(module, "USER32.dll", "KillTimer");
    s_flow_settimer_original[side] = s_flow_settimer_slot[side] ?
        (UINT_PTR (WINAPI *)(HWND, UINT_PTR, UINT, TIMERPROC))*s_flow_settimer_slot[side] : 0;
    s_flow_killtimer_original[side] = s_flow_killtimer_slot[side] ?
        (BOOL (WINAPI *)(HWND, UINT_PTR))*s_flow_killtimer_slot[side] : 0;
    if ((s_flow_settimer_slot[side] && !s_flow_settimer_original[side]) ||
        (s_flow_killtimer_slot[side] && !s_flow_killtimer_original[side]))
        fs98_fail("invalid USER32 timer imports");
    if (s_flow_settimer_slot[side]) {
        if (!VirtualProtect(s_flow_settimer_slot[side], sizeof(void *), PAGE_READWRITE, &previous))
            fs98_fail("protect SetTimer import");
        *s_flow_settimer_slot[side] = (void *)fs98_flow_settimer_probe;
        FlushInstructionCache(GetCurrentProcess(), s_flow_settimer_slot[side], sizeof(void *));
        if (!VirtualProtect(s_flow_settimer_slot[side], sizeof(void *), previous, &previous))
            fs98_fail("restore SetTimer import protection");
    }
    if (s_flow_killtimer_slot[side]) {
        if (!VirtualProtect(s_flow_killtimer_slot[side], sizeof(void *), PAGE_READWRITE, &previous))
            fs98_fail("protect KillTimer import");
        *s_flow_killtimer_slot[side] = (void *)fs98_flow_killtimer_probe;
        FlushInstructionCache(GetCurrentProcess(), s_flow_killtimer_slot[side], sizeof(void *));
        if (!VirtualProtect(s_flow_killtimer_slot[side], sizeof(void *), previous, &previous))
            fs98_fail("restore KillTimer import protection");
    }
    fprintf(stderr, "FLOW TIMER IMPORTS side=%lu module=%p set_slot=%p set_target=%p "
        "kill_slot=%p kill_target=%p\n", (unsigned long)side, module,
        s_flow_settimer_slot[side], (void *)s_flow_settimer_original[side],
        s_flow_killtimer_slot[side], (void *)s_flow_killtimer_original[side]);
}

static void fs98_flow_restore_timer_imports(uint32_t side)
{
    DWORD previous;
    if (s_flow_settimer_slot[side]) {
        if (!VirtualProtect(s_flow_settimer_slot[side], sizeof(void *), PAGE_READWRITE, &previous))
            fs98_fail("protect SetTimer restore");
        *s_flow_settimer_slot[side] = (void *)s_flow_settimer_original[side];
        FlushInstructionCache(GetCurrentProcess(), s_flow_settimer_slot[side], sizeof(void *));
        if (!VirtualProtect(s_flow_settimer_slot[side], sizeof(void *), previous, &previous))
            fs98_fail("restore SetTimer protection");
    }
    if (s_flow_killtimer_slot[side]) {
        if (!VirtualProtect(s_flow_killtimer_slot[side], sizeof(void *), PAGE_READWRITE, &previous))
            fs98_fail("protect KillTimer restore");
        *s_flow_killtimer_slot[side] = (void *)s_flow_killtimer_original[side];
        FlushInstructionCache(GetCurrentProcess(), s_flow_killtimer_slot[side], sizeof(void *));
        if (!VirtualProtect(s_flow_killtimer_slot[side], sizeof(void *), previous, &previous))
            fs98_fail("restore KillTimer protection");
    }
    s_flow_settimer_slot[side] = s_flow_killtimer_slot[side] = 0;
    s_flow_settimer_original[side] = 0;
    s_flow_killtimer_original[side] = 0;
}

static BOOL WINAPI fs98_flow_showwindow_probe(HWND window, int command)
{
    int32_t page = fs98_flow_page_for_hwnd(s_side, window);
    BOOL result;
    if (s_flow_active && s_case == 200 && page >= 0)
        fprintf(stderr, "FLOW SHOW ENTER side=%lu page=%ld hwnd=%p command=%d "
            "visible=%lu active=%p focus=%p parent=%p owner=%p prev=%p next=%p "
            "last_popup=%p style=%08lx\n", (unsigned long)s_side, (long)page,
            window, command, (unsigned long)IsWindowVisible(window),
            GetActiveWindow(), GetFocus(), GetParent(window),
            GetWindow(window, GW_OWNER), GetWindow(window, GW_HWNDPREV),
            GetWindow(window, GW_HWNDNEXT), GetLastActivePopup(s_main_window),
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_STYLE));
    result = s_flow_showwindow_original[s_side](window, command);
    if (s_flow_active && s_case == 200 && page >= 0)
        fprintf(stderr, "FLOW SHOW EXIT side=%lu page=%ld hwnd=%p command=%d "
            "return=%d visible=%lu active=%p focus=%p parent=%p owner=%p "
            "prev=%p next=%p last_popup=%p style=%08lx\n",
            (unsigned long)s_side, (long)page, window, command, (int)result,
            (unsigned long)IsWindowVisible(window), GetActiveWindow(), GetFocus(),
            GetParent(window), GetWindow(window, GW_OWNER),
            GetWindow(window, GW_HWNDPREV), GetWindow(window, GW_HWNDNEXT),
            GetLastActivePopup(s_main_window),
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_STYLE));
    return result;
}

static void fs98_flow_patch_showwindow_import(uint32_t side, HMODULE module)
{
    DWORD previous;
    if (s_flow_showwindow_original[side] || s_flow_showwindow_slot[side])
        fs98_fail("ShowWindow probe already installed");
    s_flow_showwindow_slot[side] = fs98_find_import_slot(module, "USER32.dll", "ShowWindow");
    if (!s_flow_showwindow_slot[side]) fs98_fail("resolve USER32!ShowWindow import");
    s_flow_showwindow_original[side] =
        (BOOL (WINAPI *)(HWND, int))*s_flow_showwindow_slot[side];
    if (!s_flow_showwindow_original[side] ||
        !VirtualProtect(s_flow_showwindow_slot[side], sizeof(void *), PAGE_READWRITE, &previous))
        fs98_fail("patch ShowWindow import for activation probe");
    *s_flow_showwindow_slot[side] = (void *)fs98_flow_showwindow_probe;
    FlushInstructionCache(GetCurrentProcess(), s_flow_showwindow_slot[side], sizeof(void *));
    if (!VirtualProtect(s_flow_showwindow_slot[side], sizeof(void *), previous, &previous))
        fs98_fail("restore ShowWindow import protection");
    fprintf(stderr, "FLOW SHOW IMPORT side=%lu module=%p slot=%p target=%p\n",
        (unsigned long)side, module, s_flow_showwindow_slot[side],
        (void *)s_flow_showwindow_original[side]);
}

static void fs98_flow_restore_showwindow_import(uint32_t side, HMODULE module)
{
    DWORD previous;
    (void)module;
    if (!s_flow_showwindow_original[side]) return;
    if (!s_flow_showwindow_slot[side] ||
        !VirtualProtect(s_flow_showwindow_slot[side], sizeof(void *), PAGE_READWRITE, &previous))
        fs98_fail("restore ShowWindow import for activation probe");
    *s_flow_showwindow_slot[side] = (void *)s_flow_showwindow_original[side];
    FlushInstructionCache(GetCurrentProcess(), s_flow_showwindow_slot[side], sizeof(void *));
    if (!VirtualProtect(s_flow_showwindow_slot[side], sizeof(void *), previous, &previous))
        fs98_fail("restore ShowWindow import protection");
    s_flow_showwindow_original[side] = 0;
    s_flow_showwindow_slot[side] = 0;
}

static int32_t fs98_flow_message_page(HWND window)
{
    HWND parent = window;
    int32_t page;
    while (parent) {
        page = fs98_flow_page_for_hwnd(s_side, parent);
        if (page >= 0) return page;
        if (parent == s_main_window) return -1;
        parent = GetParent(parent);
        if (!parent) parent = GetWindow(window, GW_OWNER);
    }
    return -1;
}

static int32_t fs98_flow_focus_page(void)
{
    HWND focus = GetFocus();
    while (focus) {
        int32_t page = fs98_flow_page_for_hwnd(s_side, focus);
        if (page >= 0) return page;
        focus = GetParent(focus);
    }
    return -1;
}

static void fs98_flow_log_ui_message(uint32_t phase, HWND window,
                                     uint32_t message, uint32_t wparam,
                                     uint32_t lparam, uint32_t result)
{
    int32_t page = fs98_flow_message_page(window);
    int32_t focus_page = fs98_flow_focus_page();
    int32_t active_page = fs98_flow_page_for_hwnd(s_side, GetActiveWindow());
    HWND focus = GetFocus();
    uint32_t focus_id = focus ? (uint32_t)GetDlgCtrlID(focus) : 0;
    uint32_t style = (uint32_t)GetWindowLongA(window, GWL_STYLE);
    if (page < 0 && window != s_main_window && !IsChild(s_main_window, window)) return;
    fprintf(stderr, "FLOW UI EVENT side=%lu case=%lu phase=%s page=%ld msg=%04lx wp=%08lx lp=%08lx result=%08lx visible=%08lx focus=%ld/%lu active=%ld style=%08lx\n",
        (unsigned long)s_side, (unsigned long)s_case, phase ? "post" : "pre",
        (long)page, (unsigned long)message, (unsigned long)wparam,
        (unsigned long)lparam, (unsigned long)result,
        (unsigned long)fs98_flow_visible_mask(s_side), (long)focus_page,
        (unsigned long)focus_id, (long)active_page, (unsigned long)style);
    if (!phase && message == WM_ACTIVATE && LOWORD(wparam) == WA_INACTIVE && page >= 0) {
        PVOID frames[16];
        USHORT count = RtlCaptureStackBackTrace(0, 16, frames, 0);
        USHORT index;
        fprintf(stderr, "FLOW ACTIVATE STACK side=%lu case=%lu page=%ld count=%u",
            (unsigned long)s_side, (unsigned long)s_case, (long)page, (unsigned)count);
        for (index = 0; index < count; ++index)
            fprintf(stderr, " %p", frames[index]);
        fputc('\n', stderr);
    }
}

static uint32_t fs98_flow_watch_message(uint32_t message)
{
    return message == WM_INITDIALOG || message == WM_ACTIVATE ||
        message == WM_SETFOCUS || message == WM_KILLFOCUS ||
        message == WM_COMMAND || message == WM_SHOWWINDOW ||
        message == WM_TIMER ||
        message == WM_NCACTIVATE || message == WM_WINDOWPOSCHANGING ||
        message == WM_WINDOWPOSCHANGED || message == WM_ACTIVATEAPP ||
        message == WM_ENABLE || message == WM_CANCELMODE ||
        message == WM_PARENTNOTIFY || message == WM_NEXTDLGCTL ||
        message == WM_SYSCOMMAND;
}

static LRESULT CALLBACK fs98_flow_callwnd_hook(int code, WPARAM wparam, LPARAM lparam)
{
    if (code >= 0 && s_flow_active &&
        (s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210)) {
        const CWPSTRUCT *call = (const CWPSTRUCT *)lparam;
        if (fs98_flow_watch_message(call->message))
            fs98_flow_log_ui_message(0, call->hwnd, call->message,
                (uint32_t)call->wParam, (uint32_t)call->lParam, 0);
    }
    return CallNextHookEx(0, code, wparam, lparam);
}

static LRESULT CALLBACK fs98_flow_callwndret_hook(int code, WPARAM wparam, LPARAM lparam)
{
    if (code >= 0 && s_flow_active &&
        (s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210)) {
        const CWPRETSTRUCT *call = (const CWPRETSTRUCT *)lparam;
        if (fs98_flow_watch_message(call->message))
            fs98_flow_log_ui_message(1, call->hwnd, call->message,
                (uint32_t)call->wParam, (uint32_t)call->lParam,
                (uint32_t)call->lResult);
    }
    return CallNextHookEx(0, code, wparam, lparam);
}

static uint8_t s_details_blocks[2][0xd4];
static uint8_t s_details_rows[2][2][4][16];
static char s_details_names[2][2][4][32];
static char s_details_pilot[2][32], s_details_observer[2][32];
static char s_details_name[2][32], s_details_count_a[2][16], s_details_count_b[2][16];
static char s_details_limits[2][4][8];
static uint32_t s_details_refresh[2], s_details_confirm[2], s_details_removed[2];
static uint32_t s_details_event_count[2];
static uint32_t s_details_owner_active[2];
static struct fs98_callback_event s_details_events[2][16];
static struct fs98_details_result s_details_results[2];

static int32_t fs98_flow_page_for_hwnd(uint32_t side, HWND window)
{
    uint32_t page;
    for (page = 0; page < 5; ++page)
        if (s_flow_windows[side] && s_flow_windows[side][page] == window)
            return (int32_t)page;
    return -1;
}

static uint32_t fs98_flow_visible_mask(uint32_t side)
{
    uint32_t page, mask = 0;
    for (page = 0; page < 5; ++page)
        if (s_flow_windows[side] && s_flow_windows[side][page] &&
            IsWindow(s_flow_windows[side][page]) && IsWindowVisible(s_flow_windows[side][page]))
            mask |= 1u << page;
    return mask;
}

static void fs98_flow_trace_event(uint32_t side, uint32_t kind, int32_t page,
                                  uint32_t message, uint32_t wparam,
                                  uint32_t lparam, uint32_t depth, uint32_t site)
{
    struct fs98_flow_trace *event;
    HWND focus;
    HWND focus_parent;
    HWND active;
    HWND foreground;
    uint32_t focus_id = 0;
    int32_t focus_page = -1;
    uint32_t index = s_flow_trace_count[side];
    if (index >= sizeof(s_flow_trace[side]) / sizeof(s_flow_trace[side][0])) return;
    s_flow_trace_count[side] = index + 1;
    event = &s_flow_trace[side][index];
    memset(event, 0, sizeof(*event));
    event->kind = kind;
    event->page = page < 0 ? 0xffffffffu : (uint32_t)page;
    event->message = message;
    event->wparam = wparam;
    event->lparam = lparam;
    event->depth = depth;
    event->site = site;
    event->visible_mask = fs98_flow_visible_mask(side);
    active = GetActiveWindow();
    foreground = GetForegroundWindow();
    event->active_page = (uint32_t)fs98_flow_page_for_hwnd(side, active);
    event->foreground_page = (uint32_t)fs98_flow_page_for_hwnd(side, foreground);
    focus = GetFocus();
    if (focus) {
        focus_id = (uint32_t)GetDlgCtrlID(focus);
        focus_parent = GetParent(focus);
        focus_page = fs98_flow_page_for_hwnd(side, focus_parent);
    }
    event->focus_id = focus_id;
    event->focus_page = focus_page < 0 ? 0xffffffffu : (uint32_t)focus_page;
    if (kind == 3 && !event->depth) event->depth = s_flow_dispatch_depth[side];
}

static uint32_t fs98_initial_enabled_diff(const struct fs98_result *left,
                                           const struct fs98_result *right)
{
    uint32_t index;
    for (index = 0; index < 3; ++index)
        if (left->initial_buttons[index][0] != right->initial_buttons[index][0]) return 1;
    return 0;
}

static uint32_t fs98_visibility_diff(const struct fs98_result *left,
                                      const struct fs98_result *right)
{
    uint32_t index;
    for (index = 0; index < 3; ++index) {
        if (left->initial_buttons[index][1] != right->initial_buttons[index][1] ||
            ((left->initial_button_styles[index] ^ right->initial_button_styles[index]) & WS_VISIBLE) != 0 ||
            left->initial_button_parent_visible[index] != right->initial_button_parent_visible[index] ||
            left->direct_update_buttons[index][1] != right->direct_update_buttons[index][1] ||
            ((left->direct_update_button_styles[index] ^ right->direct_update_button_styles[index]) & WS_VISIBLE) != 0 ||
            left->direct_update_parent_visible[index] != right->direct_update_parent_visible[index]) return 1;
    }
    return 0;
}

static uint32_t fs98_direct_update_diff(const struct fs98_result *left,
                                        const struct fs98_result *right)
{
    uint32_t index;
    for (index = 0; index < 3; ++index)
        if (left->direct_update_buttons[index][0] != right->direct_update_buttons[index][0]) return 1;
    return 0;
}

static uint32_t fs98_visibility_checkpoints_equal(const struct fs98_result *left,
                                                  const struct fs98_result *right)
{
    uint32_t index;
    if (memcmp(left->create_button_styles, right->create_button_styles,
               sizeof(left->create_button_styles)) != 0 ||
        memcmp(left->create_parent_visible, right->create_parent_visible,
               sizeof(left->create_parent_visible)) != 0 ||
        memcmp(left->create_seen, right->create_seen,
               sizeof(left->create_seen)) != 0 ||
        memcmp(left->member_button_ids, right->member_button_ids,
               sizeof(left->member_button_ids)) != 0 ||
        left->dialog_visible_after_create != right->dialog_visible_after_create ||
        memcmp(left->descriptor_visibility, right->descriptor_visibility,
               sizeof(left->descriptor_visibility)) != 0 ||
        memcmp(left->descriptor_after_init, right->descriptor_after_init,
               sizeof(left->descriptor_after_init)) != 0 ||
        memcmp(left->initial_button_styles, right->initial_button_styles,
               sizeof(left->initial_button_styles)) != 0 ||
        memcmp(left->initial_button_parent_visible, right->initial_button_parent_visible,
               sizeof(left->initial_button_parent_visible)) != 0 ||
        memcmp(left->direct_update_button_styles, right->direct_update_button_styles,
               sizeof(left->direct_update_button_styles)) != 0 ||
        memcmp(left->direct_update_parent_visible, right->direct_update_parent_visible,
               sizeof(left->direct_update_parent_visible)) != 0) return 0;
    for (index = 0; index < 3; ++index)
        if (left->initial_buttons[index][1] != right->initial_buttons[index][1] ||
            left->direct_update_buttons[index][1] != right->direct_update_buttons[index][1]) return 0;
    return 1;
}

static void fs98_fail(const char *message)
{
    fprintf(stderr, "MP_DIALOG UI FAIL side=%lu case=%lu: %s\n",
            (unsigned long)s_side, (unsigned long)s_case, message);
    ExitProcess(2);
}

static void fs98_host_unexpected(void)
{
    fprintf(stderr, "unexpected modeled HOST service side=%lu caller=%p\n",
            (unsigned long)s_side, __builtin_return_address(0));
    ExitProcess(2);
}

static uint32_t SC fs98_host_allocate(uint32_t flags, uint32_t size,
                                      void **block_out, int32_t zero)
{
    void *block;
    (void)flags;
    if (!block_out || !size) return (uint32_t)-1;
    block = zero ? calloc(1, size) : malloc(size);
    *block_out = block;
    return block ? 0 : (uint32_t)-1;
}

static uint32_t SC fs98_host_reallocate(uint32_t flags, uint32_t size, void **block_out)
{
    void *block;
    (void)flags;
    if (!block_out || !size) return (uint32_t)-1;
    block = realloc(*block_out, size);
    if (!block) return (uint32_t)-1;
    *block_out = block;
    return 0;
}

static uint32_t SC fs98_host_free(void **block_out)
{
    if (!block_out) return (uint32_t)-1;
    free(*block_out);
    *block_out = 0;
    return 0;
}

static uint32_t SC fs98_host_register(void *linkage, uint32_t *module_out)
{
    struct fs98_test_linkage *entry = linkage;
    if (!entry || entry->id != 0x8000000a || !module_out || !s_current_module)
        fs98_fail("HOST +0x298 registration arguments");
    *module_out = (uint32_t)s_current_module;
    ++s_host_init[s_side];
    return 0;
}

static uint32_t SC fs98_host_release(uint32_t module)
{
    if (module != (uint32_t)s_current_module) fs98_fail("HOST +0x29c release handle");
    ++s_host_release[s_side];
    return 0;
}

static uint32_t SC fs98_host_dialog(uint32_t dialog_id, void *descriptor)
{
    (void)descriptor;
    /* Model the HOST +0x10c error-sheet path: the harness user presses OK. */
    if (dialog_id == 2) return 2;
    fs98_host_unexpected();
    return 0;
}

static int32_t SC fs98_callback(void *block, uint32_t id)
{
    struct fs98_callback_event *event;
    uint32_t index = s_callback_count++;
    if (index >= 8) fs98_fail("callback event overflow");
    event = &s_callbacks[s_side][index];
    event->id = id;
    event->arg_is_block = block == s_test_owner_block[s_side];
    event->result = ((struct fs98_mpdialog_owner_block *)block)->result;
    if (!event->arg_is_block) fs98_fail("MULTIP callback argument is not the owner block");
    return (int32_t)s_callback_returns[id];
}

static int32_t SC fs98_read_callback(void *block) { return fs98_callback(block, 0); }
static int32_t SC fs98_accept_callback(void *block) { return fs98_callback(block, 1); }
static int32_t SC fs98_details_callback(void *block) { return fs98_callback(block, 2); }
static int32_t SC fs98_weather_callback(void *block) { return fs98_callback(block, 3); }
static int32_t SC fs98_sync_callback(void *block) { return fs98_callback(block, 4); }

static void fs98_details_record(void *block, uint32_t id, uint32_t value)
{
    struct fs98_callback_event *event;
    uint32_t index = s_details_event_count[s_side]++;
    if (index >= 16) fs98_fail("0x110c callback event overflow");
    event = &s_details_events[s_side][index];
    event->id = id;
    event->arg_is_block = block == s_details_blocks[s_side];
    event->result = D32(block, 0);
    event->value = value;
    event->status_pilot = D32(block, 0x1c);
    event->status_observer = D32(block, 0x30);
    if (!s_details_owner_active[s_side]) fs98_fail("MULTIP owner block used after HOST modal return");
    if (!event->arg_is_block) fs98_fail("MULTIP 0x110c callback argument is not owner block");
}

static int32_t fs98_parse_capacity(const char *text, uint32_t minimum, uint32_t maximum)
{
    char *end;
    long value;
    if (!text || !*text) return 1;
    value = strtol(text, &end, 10);
    if (*end || value < (long)minimum || value > (long)maximum) return 1;
    return 0;
}

static int32_t SC fs98_details_validate_callback(void *block)
{
    uint32_t mode = D32(block, 0x14);
    int32_t result = 0;
    D32(block, 0x1c) = 0;
    D32(block, 0x30) = 0;
    if (mode == 1) {
        D32(block, 0x1c) = (uint32_t)fs98_parse_capacity((char *)D32(block, 0x20),
            (uint32_t)strtoul((char *)D32(block, 0x28), 0, 10),
            (uint32_t)strtoul((char *)D32(block, 0x2c), 0, 10));
        D32(block, 0x30) = (uint32_t)fs98_parse_capacity((char *)D32(block, 0x34),
            (uint32_t)strtoul((char *)D32(block, 0x3c), 0, 10),
            (uint32_t)strtoul((char *)D32(block, 0x40), 0, 10));
        if (D32(block, 0x1c) || D32(block, 0x30)) result = -1;
    }
    fs98_details_record(block, 4, 0);
    s_details_events[s_side][s_details_event_count[s_side] - 1].reserved = (uint32_t)result;
    return result;
}

static int32_t SC fs98_details_accept_callback(void *block)
{
    uint32_t value = (uint32_t)strtoul((char *)D32(block, 0x20), 0, 10) |
        ((uint32_t)strtoul((char *)D32(block, 0x34), 0, 10) << 16);
    fs98_details_record(block, 8, value);
    s_details_events[s_side][s_details_event_count[s_side] - 1].reserved = 0;
    return 0;
}

static int32_t SC fs98_details_remove_callback(void *block)
{
    uint32_t index = D32(block, 0x58), count = D32(block, 0x50);
    uint8_t *rows = (uint8_t *)D32(block, 0x4c);
    uint32_t player = rows && index < count ? D32(rows + index * 16, 0) : 0xffffffff;
    s_details_removed[s_side] = s_details_confirm[s_side] && player != 0xffffffff ? player : 0;
    fs98_details_record(block, 0x0c, player);
    s_details_events[s_side][s_details_event_count[s_side] - 1].reserved = 0;
    return 0;
}

static int32_t SC fs98_details_refresh_callback(void *block)
{
    if (s_details_refresh[s_side]) {
        D32(block, 0x4c) = (uint32_t)(uintptr_t)s_details_rows[s_side][1];
        D32(block, 0x50) = 3;
        D32(block, 0x44) = (uint32_t)(uintptr_t)s_details_count_a[s_side];
        D32(block, 0x48) = (uint32_t)(uintptr_t)s_details_count_b[s_side];
        ++s_details_results[s_side].refresh_updates;
    }
    fs98_details_record(block, 0x10, s_details_refresh[s_side]);
    s_details_events[s_side][s_details_event_count[s_side] - 1].reserved = s_details_refresh[s_side];
    return (int32_t)s_details_refresh[s_side];
}

static void fs98_flow_record(void *block, uint32_t id, uint32_t value)
{
    struct fs98_flow_event *event;
    uint32_t index = s_flow_event_count[s_side]++;
    struct fs98_flow_result *result = &s_flow_results[s_side];
    if (index >= 24) {
        result->callback_overflow = 1;
        return;
    }
    event = &s_flow_events[s_side][index];
    event->id = id;
    event->owner_argument = block == s_flow_blocks[s_side];
    event->result = event->owner_argument ? D32(block, 0) : 0;
    event->value = value;
    event->mode = event->owner_argument ? D32(block, 0x24) : 0;
    event->provider = event->owner_argument ? D32(block, 0x90) : 0;
    event->selection = event->owner_argument ? D32(block, 0xb8) : 0;
    event->status_callsign = event->owner_argument ? D32(block, 0x28) : 0;
    event->status_name = event->owner_argument ? D32(block, 0x48) : 0;
    event->status_pilot = event->owner_argument ? D32(block, 0x5c) : 0;
    event->status_observer = event->owner_argument ? D32(block, 0x70) : 0;
    if (!s_flow_owner_active[s_side]) fs98_fail("MULTIP flow descriptor used after HOST teardown");
}

static int32_t SC fs98_flow_provider_callback(void *block)
{
    uint32_t result = 0;
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW PROVIDER CALLBACK ENTER side=%lu case=%lu block=%p "
            "mode=%lu provider=%lu selected=%lu hwnd=%p visible=%08lx depth=%lu ret=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, block,
            (unsigned long)D32(block, 0x24), (unsigned long)D32(block, 0x90),
            (unsigned long)D32(block, 0xb8), (void *)(uintptr_t)D32(block, 0x14),
            (unsigned long)fs98_flow_visible_mask(s_side),
            (unsigned long)s_flow_dispatch_depth[s_side],
            __builtin_return_address(0));
    if (s_flow_provider_failures[s_side]) {
        --s_flow_provider_failures[s_side];
        result = (uint32_t)-1;
    }
    fs98_flow_trace_event(s_side, 3, -1, 0, result, D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 0, result);
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW PROVIDER CALLBACK EXIT side=%lu case=%lu block=%p "
            "result=%08lx mode=%lu provider=%lu selected=%lu visible=%08lx depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, block,
            (unsigned long)result, (unsigned long)D32(block, 0x24),
            (unsigned long)D32(block, 0x90), (unsigned long)D32(block, 0xb8),
            (unsigned long)fs98_flow_visible_mask(s_side),
            (unsigned long)s_flow_dispatch_depth[s_side]);
    return (int32_t)result;
}

static int32_t SC fs98_flow_reset_callback(void *block)
{
    fs98_flow_trace_event(s_side, 3, -1, 1, 0, D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 1, 0);
    return 0;
}

static int32_t SC fs98_flow_accept_callback(void *block)
{
    uint32_t result = s_flow_accept_result[s_side];
    if (result) --s_flow_accept_result[s_side];
    fs98_flow_trace_event(s_side, 3, -1, 2, result, D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 2, result);
    return (int32_t)result;
}

static int32_t SC fs98_flow_validate_callback(void *block)
{
    uint32_t mode = D32(block, 0x24);
    int32_t failed = 0;
    ++s_flow_results[s_side].validation_count;
    D32(block, 0x28) = D32(block, 0x48) = D32(block, 0x5c) = D32(block, 0x70) = 0;
    if (mode != 2 && (!D32(block, 0x2c) || !*(char *)(uintptr_t)D32(block, 0x2c))) {
        D32(block, 0x28) = 0x1003;
        failed = 1;
    }
    if (mode == 1) {
        if (!D32(block, 0x4c) || !*(char *)(uintptr_t)D32(block, 0x4c)) {
            D32(block, 0x48) = 0x1003;
            failed = 1;
        }
        if (fs98_parse_capacity((const char *)(uintptr_t)D32(block, 0x60),
                (uint32_t)strtoul((const char *)(uintptr_t)D32(block, 0x6c), 0, 10),
                (uint32_t)strtoul((const char *)(uintptr_t)D32(block, 0x68), 0, 10))) {
            D32(block, 0x5c) = 0x1004;
            failed = 1;
        }
        if (fs98_parse_capacity((const char *)(uintptr_t)D32(block, 0x74),
                (uint32_t)strtoul((const char *)(uintptr_t)D32(block, 0x7c), 0, 10),
                (uint32_t)strtoul((const char *)(uintptr_t)D32(block, 0x80), 0, 10))) {
            D32(block, 0x70) = 0x1004;
            failed = 1;
        }
    }
    fs98_flow_trace_event(s_side, 3, -1, 3, (uint32_t)(failed ? -1 : 0),
        D32(block, 0x24), 0, (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 3, (uint32_t)(failed ? -1 : 0));
    return failed ? -1 : 0;
}

static int32_t SC fs98_flow_host_settings_callback(void *block)
{
    fs98_flow_trace_event(s_side, 3, -1, 4, 0, D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 4, 0);
    return 0;
}

static int32_t SC fs98_flow_selection_callback(void *block, uint32_t selection)
{
    fs98_flow_trace_event(s_side, 3, -1, 5, selection, D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 5, selection);
    return 0;
}

static int32_t SC fs98_flow_details_callback(void *block)
{
    fs98_flow_trace_event(s_side, 3, -1, 6, D32(block, 0xb8), D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 6, D32(block, 0xb8));
    return 0;
}

static int32_t SC fs98_flow_refresh_callback(void *block)
{
    struct fs98_flow_result *result = &s_flow_results[s_side];
    D32(block, 0xac) = (uint32_t)(uintptr_t)s_flow_rows[s_side][1][0];
    D32(block, 0xb0) = 2;
    ++result->browser_refresh_callbacks;
    fs98_flow_trace_event(s_side, 3, -1, 7, 1, D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 7, 1);
    if (s_flow_refresh_reentry_pending[s_side]) {
        void *browser_object = s_flow_dialog_objects[s_side][4];
        HWND browser = browser_object ? (HWND)D32(browser_object, 0x20) : 0;
        s_flow_refresh_reentry_pending[s_side] = 0;
        if (!browser_object || !IsWindow(browser) || D32(browser_object, 0x1aec) != 1)
            fs98_fail("Browser refresh callback reentry guard state");
        ++result->browser_nested_refresh_attempts;
        fprintf(stderr, "FLOW BROWSER CALLBACK REENTER side=%lu case=%lu "
            "object=%p hwnd=%p guard=%lu message=WM_TIMER wp=1 lp=0\n",
            (unsigned long)s_side, (unsigned long)s_case, browser_object, browser,
            (unsigned long)D32(browser_object, 0x1aec));
        SendMessageA(browser, WM_TIMER, 1, 0);
        if (D32(browser_object, 0x1aec) != 1)
            fs98_fail("nested Browser refresh cleared outer guard");
    }
    return 1;
}

static int32_t SC fs98_flow_browser_settings_callback(void *block)
{
    fs98_flow_trace_event(s_side, 3, -1, 8, 0, D32(block, 0x24), 0,
        (uint32_t)(uintptr_t)__builtin_return_address(0));
    fs98_flow_record(block, 8, 0);
    return 0;
}

static void *fs98_load(const char *path)
{
    char full_path[MAX_PATH];
    HMODULE module;
    GetFullPathNameA(path, MAX_PATH, full_path, 0);
    module = LoadLibraryA(full_path);
    if (!module) {
        fprintf(stderr, "LoadLibrary %s error=%lu\n", path, GetLastError());
        ExitProcess(2);
    }
    return module;
}

static void fs98_check_public_factories(uint32_t side, HMODULE module,
                                        struct fs98_test_linkage *linkage)
{
    static const uint32_t s_ids[3] = {0x110a, 0x110b, 0x110c};
    static const uint32_t s_flags[3] = {2, 0, 0};
    static const uintptr_t s_retail_factories[3] = {
        0x22341130u, 0x223411e0u, 0x22341270u
    };
    uint32_t index;
    for (index = 0; index < 3; ++index) {
        HMODULE owner = 0;
        FARPROC factory = (FARPROC)linkage->dialogs[index].create;
        if (linkage->dialogs[index].id != s_ids[index] ||
            linkage->dialogs[index].flags != s_flags[index] || !factory)
            fs98_fail("public MP_DIALOG factory registration");
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (const char *)factory, &owner) || owner != module)
            fs98_fail("public factory pointer ownership");
        if (side == 0 && (uintptr_t)factory != s_retail_factories[index])
            fs98_fail("retail public factory address");
    }
    if (linkage->dialogs[3].id || linkage->dialogs[3].create ||
        linkage->dialogs[3].flags)
        fs98_fail("public MP_DIALOG factory terminator");
}

static void fs98_application(HMODULE mfc)
{
    HWND main = CreateWindowExA(0, "STATIC", "OpenFS98 MP_DIALOG harness",
        WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0, GetModuleHandleA(0), 0);
    void *constructor = (void *)GetProcAddress(mfc, (const char *)(uintptr_t)561);
    void *win_init = (void *)GetProcAddress(mfc, (const char *)(uintptr_t)1575);
    void *from_handle = (void *)GetProcAddress(mfc, (const char *)(uintptr_t)2864);
    if (!main || !constructor || !win_init || !from_handle) fs98_fail("MFC app bootstrap");
    s_main_window = main;
    ((void *(TC *)(void *, const char *))constructor)(s_application, 0);
    ((int32_t (SC *)(HINSTANCE, HINSTANCE, char *, int32_t))win_init)(
        GetModuleHandleA(0), 0, GetCommandLineA(), 0);
    PTR(s_application, 0x20) = ((void *(SC *)(HWND))from_handle)(main);
}

static HWND fs98_flow_create_isolated_host(void)
{
    void *(SC *from_handle)(HWND) = (void *)GetProcAddress(
        s_mfc, (const char *)(uintptr_t)2864);
    HWND window;
    void *cwnd;
    s_flow_saved_main_window = s_main_window;
    s_flow_saved_main_cwnd = PTR(s_application, 0x20);
    if (!from_handle || !s_flow_saved_main_window || !s_flow_saved_main_cwnd)
        fs98_fail("flow isolated HOST saved main window");
    (void)ShowWindow(s_flow_saved_main_window, SW_HIDE);
    window = CreateWindowExA(0, "STATIC", "OpenFS98 MP_DIALOG isolated host",
        WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0, GetModuleHandleA(0), 0);
    if (!window) fs98_fail("create isolated flow HOST window");
    cwnd = from_handle(window);
    if (!cwnd) fs98_fail("MFC CWnd wrapper for isolated flow HOST");
    s_main_window = window;
    PTR(s_application, 0x20) = cwnd;
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    SetActiveWindow(window);
    SetFocus(window);
    fs98_pump();
    return window;
}

static void fs98_flow_destroy_isolated_host(HWND window)
{
    if (!window || window != s_main_window || !s_flow_saved_main_window ||
        !s_flow_saved_main_cwnd) fs98_fail("flow isolated HOST restore state");
    /* Keep this auxiliary STATIC HWND alive until process exit.  It has an
     * MFC temporary CWnd wrapper from FromHandle; destroying it here caused
     * the wrapper map to be revisited during a later dialog cycle. */
    ShowWindow(window, SW_HIDE);
    PTR(s_application, 0x20) = s_flow_saved_main_cwnd;
    s_main_window = s_flow_saved_main_window;
    s_flow_saved_main_window = 0;
    s_flow_saved_main_cwnd = 0;
    ShowWindow(s_main_window, SW_SHOW);
    SetForegroundWindow(s_main_window);
    SetActiveWindow(s_main_window);
    SetFocus(s_main_window);
    fs98_pump();
}

static void fs98_patch_slot(void **slot, void *replacement, void **original)
{
    DWORD previous;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &previous)) fs98_fail("VirtualProtect vtable");
    *original = *slot;
    *slot = replacement;
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    VirtualProtect(slot, sizeof(*slot), previous, &previous);
}

static void fs98_restore_slot(void **slot, void *original)
{
    DWORD previous;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &previous)) fs98_fail("restore vtable protection");
    *slot = original;
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    VirtualProtect(slot, sizeof(*slot), previous, &previous);
}

static int32_t TC fs98_validate_hook(void *object, uint32_t control_id)
{
    (void)object;
    if (control_id != 0x419) fs98_fail("inherited ValidateData control id");
    ++s_results[s_side].validate_count;
    return 0;
}

static void TC fs98_post_hook(void *object)
{
    if (s_details_active) {
        ++s_details_results[s_side].post_count;
        s_details_results[s_side].post_order = ++s_lifecycle_tick;
    } else {
        ++s_results[s_side].post_count;
        s_results[s_side].post_order = ++s_lifecycle_tick;
    }
    ((void (TC *)(void *))s_post_original)(object);
}

static void *TC fs98_delete_hook(void *object, uint32_t flags)
{
    if (!(flags & 1)) fs98_fail("PostNcDestroy omitted deleting flag");
    if (s_details_active) {
        ++s_details_results[s_side].delete_count;
        s_details_results[s_side].delete_order = ++s_lifecycle_tick;
    } else {
        ++s_results[s_side].delete_count;
        s_results[s_side].delete_order = ++s_lifecycle_tick;
    }
    return ((void *(TC *)(void *, uint32_t))s_delete_original)(object, flags);
}

static LRESULT CALLBACK fs98_nc_destroy_proc(HWND window, UINT message,
                                              WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCDESTROY) {
        if (s_details_active) {
            ++s_details_results[s_side].nc_destroy_count;
            s_details_results[s_side].nc_order = ++s_lifecycle_tick;
        } else {
            ++s_results[s_side].nc_destroy_count;
            s_results[s_side].nc_destroy_order = ++s_lifecycle_tick;
        }
    }
    return CallWindowProcA(s_previous_wndproc, window, message, wparam, lparam);
}

static void fs98_patch_window_proc(HWND window)
{
    LONG previous;
    s_previous_wndproc = (WNDPROC)(uintptr_t)GetWindowLongA(window, GWL_WNDPROC);
    if (!s_previous_wndproc) fs98_fail("MFC window procedure");
    previous = SetWindowLongA(window, GWL_WNDPROC,
                              (LONG)(uintptr_t)fs98_nc_destroy_proc);
    if (!previous) fs98_fail("patch window procedure");
}

static void fs98_patch_dialog_vtable(HWND window, HMODULE expected, int patch_validate)
{
    void *from_handle = (void *)GetProcAddress(s_mfc, (const char *)(uintptr_t)2864);
    void *object = ((void *(SC *)(HWND))from_handle)(window);
    void **vtable;
    HMODULE owner = 0;
    if (!object) fs98_fail("MFC CWnd::FromHandle returned null");
    vtable = PTR(object, 0);
    s_test_dialog[s_side] = object;
    if (!vtable || vtable[0x30 / 4] == 0) fs98_fail("session class vtable");
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (const char *)vtable[0x30 / 4], &owner);
    if (owner != expected) fs98_fail("message-map vtable entry owned by wrong DLL");
    s_validate_slot = patch_validate ? &vtable[0xd8 / 4] : 0;
    s_post_slot = &vtable[0xac / 4];
    s_delete_slot = &vtable[4 / 4];
    if (s_validate_slot)
        fs98_patch_slot(s_validate_slot, (void *)fs98_validate_hook, &s_validate_original);
    fs98_patch_slot(s_post_slot, (void *)fs98_post_hook, &s_post_original);
    fs98_patch_slot(s_delete_slot, (void *)fs98_delete_hook, &s_delete_original);
}

static void fs98_restore_dialog_vtable(void)
{
    if (s_validate_slot) fs98_restore_slot(s_validate_slot, s_validate_original);
    fs98_restore_slot(s_post_slot, s_post_original);
    fs98_restore_slot(s_delete_slot, s_delete_original);
    s_validate_slot = s_post_slot = s_delete_slot = 0;
}

static int32_t fs98_flow_page_for_object(void *object)
{
    uint32_t index;
    for (index = 0; index < 5; ++index)
        if (s_flow_dialog_objects[s_side][index] == object) return (int32_t)index;
    return -1;
}

static uint32_t fs98_flow_probe_case(void)
{
    uint32_t case_index = s_case >= 200 ? (s_case - 200) % 12 : 12;
    return case_index == 3 || case_index == 4 || case_index == 10;
}

static void TC fs98_flow_activate_hook(void *object, uint32_t state,
                                       void *other, uint32_t minimized)
{
    void *other_hwnd = 0;
    void *block = PTR(object, 0x18f8);
    uint32_t mode = block ? D32(block, 0x24) : 0;
    SIZE_T bytes_read = 0;
    if (other && ReadProcessMemory(GetCurrentProcess(), (uint8_t *)other + 0x20,
            &other_hwnd, sizeof(other_hwnd), &bytes_read) && bytes_read != sizeof(other_hwnd))
        other_hwnd = 0;
    if (fs98_flow_page_for_object(object) != 3)
        fs98_fail("flow OnActivate object identity");
    if (fs98_flow_probe_case())
        fprintf(stderr, "FLOW ACTIVATE ENTER side=%lu case=%lu this=%p state=%08lx other=%p other_hwnd=%p minimized=%lu mode=%lu active=%p focus=%p visible=%08lx depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (unsigned long)state, other, other_hwnd, (unsigned long)minimized,
            (unsigned long)mode, GetActiveWindow(), GetFocus(),
            (unsigned long)fs98_flow_visible_mask(s_side),
            (unsigned long)s_flow_dispatch_depth[s_side]);
    ((void (TC *)(void *, uint32_t, void *, uint32_t))s_flow_activate_original[s_side])(
        object, state, other, minimized);
    if (fs98_flow_probe_case())
        fprintf(stderr, "FLOW ACTIVATE EXIT side=%lu case=%lu this=%p state=%08lx other=%p other_hwnd=%p minimized=%lu mode=%lu active=%p focus=%p visible=%08lx depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (unsigned long)state, other, other_hwnd, (unsigned long)minimized,
            (unsigned long)mode, GetActiveWindow(), GetFocus(),
            (unsigned long)fs98_flow_visible_mask(s_side),
            (unsigned long)s_flow_dispatch_depth[s_side]);
}

static void fs98_flow_patch_activate(void *object, void **vtable)
{
    struct fs98_mpdialog_message_map *map;
    struct fs98_mpdialog_message_entry *entry;
    void *(TC *get_map)(void *) = (void *)vtable[0x30 / 4];
    uint32_t index;
    DWORD previous;
    map = (struct fs98_mpdialog_message_map *)get_map(object);
    if (!map || !map->entries) fs98_fail("flow message map probe");
    for (index = 0, entry = (struct fs98_mpdialog_message_entry *)map->entries;
         index < 128 && (entry->message || entry->handler); ++index, ++entry)
        if (entry->message == WM_ACTIVATE && entry->code == 0) break;
    if (index == 128 || !entry->handler) fs98_fail("Connection WM_ACTIVATE map entry");
    if (s_flow_activate_entry[s_side]) fs98_fail("nested flow activate probe patch");
    s_flow_activate_entry[s_side] = entry;
    s_flow_activate_original[s_side] = entry->handler;
    if (s_side == 0 && (uint32_t)(uintptr_t)entry->handler != 0x22342010)
        fs98_fail("retail Connection OnActivate address");
    if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE, &previous))
        fs98_fail("Connection WM_ACTIVATE handler protection");
    entry->handler = (void *)fs98_flow_activate_hook;
    FlushInstructionCache(GetCurrentProcess(), &entry->handler, sizeof(entry->handler));
    VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
    fprintf(stderr, "FLOW ACTIVATE METHOD side=%lu case=%lu map=%p entry=%p sig=%lu target=%p hooked=%p\n",
        (unsigned long)s_side, (unsigned long)s_case, map, entry,
        (unsigned long)entry->signature, s_flow_activate_original[s_side],
        (void *)fs98_flow_activate_hook);
}

static void fs98_flow_restore_activate(void)
{
    struct fs98_mpdialog_message_entry *entry = s_flow_activate_entry[s_side];
    DWORD previous;
    if (!entry) return;
    if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE, &previous))
        fs98_fail("restore Connection WM_ACTIVATE handler protection");
    entry->handler = s_flow_activate_original[s_side];
    FlushInstructionCache(GetCurrentProcess(), &entry->handler, sizeof(entry->handler));
    VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
    s_flow_activate_entry[s_side] = 0;
    s_flow_activate_original[s_side] = 0;
}

static void TC fs98_flow_base_activate_probe(void *object, uint32_t state,
                                             void *other, uint32_t minimized)
{
    int32_t page = fs98_flow_page_for_object(object);
    void *other_hwnd = 0;
    SIZE_T bytes_read = 0;
    void *original;
    if (other && ReadProcessMemory(GetCurrentProcess(), (uint8_t *)other + 0x20,
            &other_hwnd, sizeof(other_hwnd), &bytes_read) && bytes_read != sizeof(other_hwnd))
        other_hwnd = 0;
    if (page < 0 || page >= 5) fs98_fail("inherited OnActivate object identity");
    original = s_flow_base_activate_original[s_side][page];
    if (!original) fs98_fail("inherited OnActivate original target");
    if (s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW BASE ACTIVATE ENTER side=%lu case=%lu page=%ld this=%p "
            "state=%08lx other=%p other_hwnd=%p minimized=%lu visible=%08lx "
            "active=%p focus=%p depth=%lu target=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, (long)page, object,
            (unsigned long)state, other, other_hwnd, (unsigned long)minimized,
            (unsigned long)fs98_flow_visible_mask(s_side), GetActiveWindow(), GetFocus(),
            (unsigned long)s_flow_dispatch_depth[s_side],
            original);
    ((void (TC *)(void *, uint32_t, void *, uint32_t))
        original)(object, state, other, minimized);
    if (s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW BASE ACTIVATE EXIT side=%lu case=%lu page=%ld this=%p "
            "state=%08lx active=%p focus=%p depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, (long)page, object,
            (unsigned long)state, GetActiveWindow(), GetFocus(),
            (unsigned long)s_flow_dispatch_depth[s_side]);
}

static void fs98_flow_patch_base_activate(void *object, void **vtable)
{
    struct fs98_mpdialog_message_map *map;
    struct fs98_mpdialog_message_entry *entry;
    void *(TC *get_map)(void *) = (void *)vtable[0x30 / 4];
    uint32_t level, index;
    DWORD previous;
    map = (struct fs98_mpdialog_message_map *)get_map(object);
    for (level = 0; map && level < 8; ++level) {
        for (index = 0, entry = (struct fs98_mpdialog_message_entry *)map->entries;
             entry && index < 128 && (entry->message || entry->handler); ++index, ++entry)
            if (entry->message == WM_ACTIVATE && entry->code == 0) break;
        if (entry && index < 128 && entry->handler) break;
        map = map->base ? (struct fs98_mpdialog_message_map *)map->base() : 0;
    }
    if (!map || !entry || index == 128 || !entry->handler)
        fs98_fail("inherited WM_ACTIVATE map entry");
    {
        int32_t page = fs98_flow_page_for_object(object);
        uint32_t other_page;
        void *original = entry->handler;
        if (page < 0 || page >= 5) fs98_fail("inherited WM_ACTIVATE page");
        if (s_flow_base_activate_entry[s_side][page]) {
            if (entry != s_flow_base_activate_entry[s_side][page] ||
                entry->handler != (void *)fs98_flow_base_activate_probe)
                fs98_fail("inherited WM_ACTIVATE map identity differs");
            return;
        }
        if (original == (void *)fs98_flow_base_activate_probe) {
            for (other_page = 0; other_page < 5; ++other_page)
                if (s_flow_base_activate_entry[s_side][other_page] == entry) {
                    original = s_flow_base_activate_original[s_side][other_page];
                    break;
                }
            if (original == (void *)fs98_flow_base_activate_probe)
                fs98_fail("shared inherited WM_ACTIVATE original target");
        }
        s_flow_base_activate_entry[s_side][page] = entry;
        s_flow_base_activate_original[s_side][page] = original;
        if (entry->handler == (void *)fs98_flow_base_activate_probe) return;
    }
    fprintf(stderr, "FLOW BASE ACTIVATE METHOD side=%lu case=%lu level=%lu map=%p "
        "entry=%p sig=%lu target=%p\n", (unsigned long)s_side,
        (unsigned long)s_case, (unsigned long)level, map, entry,
        (unsigned long)entry->signature, entry->handler);
    if (s_side == 0 && (uint32_t)(uintptr_t)s_flow_base_activate_original[s_side][fs98_flow_page_for_object(object)] != 0x20476290)
        fs98_fail("retail inherited CSimsDialog OnActivate address");
    if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE, &previous))
        fs98_fail("inherited WM_ACTIVATE handler protection");
    entry->handler = (void *)fs98_flow_base_activate_probe;
    FlushInstructionCache(GetCurrentProcess(), &entry->handler, sizeof(entry->handler));
    VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
}

static void fs98_flow_restore_base_activate(void)
{
    uint32_t page;
    for (page = 0; page < 5; ++page) {
        struct fs98_mpdialog_message_entry *entry = s_flow_base_activate_entry[s_side][page];
        DWORD previous;
        if (!entry) continue;
        if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE, &previous))
            fs98_fail("restore inherited WM_ACTIVATE handler protection");
        entry->handler = s_flow_base_activate_original[s_side][page];
        FlushInstructionCache(GetCurrentProcess(), &entry->handler, sizeof(entry->handler));
        VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
        s_flow_base_activate_entry[s_side][page] = 0;
        s_flow_base_activate_original[s_side][page] = 0;
    }
}

static void fs98_flow_patch_provider_map(void *object, void **vtable)
{
    struct fs98_mpdialog_message_map *map;
    struct fs98_mpdialog_message_entry *entry;
    void *(TC *get_map)(void *) = (void *)vtable[0x30 / 4];
    uint32_t index;
    DWORD previous;
    map = (struct fs98_mpdialog_message_map *)get_map(object);
    if (!map || !map->entries) fs98_fail("Connection provider message map");
    for (index = 0, entry = (struct fs98_mpdialog_message_entry *)map->entries;
         index < 128 && (entry->message || entry->handler); ++index, ++entry)
        if (entry->message == WM_COMMAND && entry->code == 2 &&
            entry->first == 0x41b && entry->last == 0x41b) break;
    if (index == 128 || !entry->handler) fs98_fail("Connection provider notification entry");
    if (entry->signature != 0xc)
        fs98_fail("Connection provider notification signature");
    if (s_flow_provider_entry[s_side]) fs98_fail("duplicate Connection provider probe");
    if (s_side == 0 && (uint32_t)(uintptr_t)entry->handler != 0x22341c10)
        fs98_fail("retail Connection provider thunk address");
    if (s_side == 1) {
        HMODULE handler_owner = 0;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (const char *)entry->handler, &handler_owner) ||
            handler_owner != s_current_module)
            fs98_fail("replacement Connection provider thunk owner");
    }
    s_flow_provider_entry[s_side] = entry;
    s_flow_provider_original[s_side] = entry->handler;
    if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE, &previous))
        fs98_fail("Connection provider map protection");
    entry->handler = (void *)fs98_flow_provider_map_probe;
    FlushInstructionCache(GetCurrentProcess(), &entry->handler, sizeof(entry->handler));
    VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
    fprintf(stderr, "FLOW PROVIDER MAP METHOD side=%lu case=%lu map=%p entry=%p "
        "sig=%lu target=%p vtablecc=%p hooked=%p\n",
        (unsigned long)s_side, (unsigned long)s_case, map, entry,
        (unsigned long)entry->signature, s_flow_provider_original[s_side],
        vtable[0xcc / 4], (void *)fs98_flow_provider_map_probe);
}

static void fs98_flow_restore_provider_map(void)
{
    struct fs98_mpdialog_message_entry *entry = s_flow_provider_entry[s_side];
    DWORD previous;
    if (!entry) return;
    if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE, &previous))
        fs98_fail("restore Connection provider map protection");
    entry->handler = s_flow_provider_original[s_side];
    FlushInstructionCache(GetCurrentProcess(), &entry->handler, sizeof(entry->handler));
    VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
    s_flow_provider_entry[s_side] = 0;
    s_flow_provider_original[s_side] = 0;
}

static void fs98_flow_patch_browser_timer(void *object, void **vtable,
                                          HMODULE expected)
{
    struct fs98_mpdialog_message_map *map;
    struct fs98_mpdialog_message_entry *entry;
    void *(TC *get_map)(void *) = (void *)vtable[0x30 / 4];
    HMODULE handler_owner = 0;
    uint32_t index;
    DWORD previous;
    map = (struct fs98_mpdialog_message_map *)get_map(object);
    if (!map || !map->entries) fs98_fail("Browser message map");
    for (index = 0, entry = (struct fs98_mpdialog_message_entry *)map->entries;
         index < 128 && (entry->message || entry->handler); ++index, ++entry)
        if (entry->message == WM_TIMER && entry->code == 0) break;
    if (index == 128 || !entry->handler)
        fs98_fail("Browser WM_TIMER map entry");
    if (s_flow_browser_timer_entry[s_side])
        fs98_fail("duplicate Browser WM_TIMER probe");
    if (entry->signature != 0xd)
        fs98_fail("Browser WM_TIMER handler signature");
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const char *)entry->handler, &handler_owner) ||
        handler_owner != expected)
        fs98_fail("Browser WM_TIMER handler owner");
    s_flow_browser_timer_entry[s_side] = entry;
    s_flow_browser_timer_original[s_side] = entry->handler;
    if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE,
                        &previous))
        fs98_fail("Browser WM_TIMER map protection");
    entry->handler = (void *)fs98_flow_browser_timer_probe;
    FlushInstructionCache(GetCurrentProcess(), &entry->handler,
                          sizeof(entry->handler));
    VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
    fprintf(stderr, "FLOW BROWSER TIMER METHOD side=%lu case=%lu map=%p entry=%p "
        "sig=%lu target=%p owner=%p hooked=%p\n", (unsigned long)s_side,
        (unsigned long)s_case, map, entry, (unsigned long)entry->signature,
        s_flow_browser_timer_original[s_side], handler_owner,
        (void *)fs98_flow_browser_timer_probe);
}

static void fs98_flow_restore_browser_timer(void)
{
    struct fs98_mpdialog_message_entry *entry =
        s_flow_browser_timer_entry[s_side];
    DWORD previous;
    if (!entry) return;
    if (!VirtualProtect(&entry->handler, sizeof(entry->handler), PAGE_READWRITE,
                        &previous))
        fs98_fail("restore Browser WM_TIMER map protection");
    entry->handler = s_flow_browser_timer_original[s_side];
    FlushInstructionCache(GetCurrentProcess(), &entry->handler,
                          sizeof(entry->handler));
    VirtualProtect(&entry->handler, sizeof(entry->handler), previous, &previous);
    s_flow_browser_timer_entry[s_side] = 0;
    s_flow_browser_timer_original[s_side] = 0;
}

static int32_t TC fs98_flow_validate_probe(void *object, uint32_t control_id)
{
    int32_t page = fs98_flow_page_for_object(object);
    int32_t result;
    HWND window = (HWND)D32(object, 0x20);
    if (page != 1) fs98_fail("flow Choice ValidateData object identity");
    if (s_case == 200 || s_case == 201)
        fprintf(stderr, "FLOW VALIDATE_METHOD ENTER side=%lu case=%lu this=%p id=%lu visible=%lu style=%08lx focus=%p active=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (unsigned long)control_id, (unsigned long)IsWindowVisible(window),
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_STYLE),
            GetFocus(), GetActiveWindow());
    result = ((int32_t (TC *)(void *, uint32_t))s_flow_validate_original[s_side])(object, control_id);
    if (s_case == 200 || s_case == 201)
        fprintf(stderr, "FLOW VALIDATE_METHOD EXIT side=%lu case=%lu this=%p id=%lu result=%08lx visible=%lu focus=%p active=%p callbacks=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (unsigned long)control_id, (unsigned long)result,
            (unsigned long)IsWindowVisible(window), GetFocus(), GetActiveWindow(),
            (unsigned long)s_flow_results[s_side].validation_count);
    return result;
}

static int32_t TC fs98_flow_oncommand_probe(void *object, uint32_t wparam,
                                             uint32_t lparam)
{
    int32_t page = fs98_flow_page_for_object(object);
    int32_t result;
    void *control = (void *)(uintptr_t)lparam;
    char control_class[64] = "<none>";
    if (control && IsWindow(control)) GetClassNameA(control, control_class, sizeof(control_class));
    if ((s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210) &&
        ((page == 1 && LOWORD(wparam) == 0x419) ||
         (page == 4 && (LOWORD(wparam) == 0x3ff || LOWORD(wparam) == 2 ||
                        LOWORD(wparam) == 0x3e9))))
        fprintf(stderr, "FLOW ONCOMMAND ENTER side=%lu case=%lu this=%p page=%ld "
            "wp=%08lx id=%lu code=%04lx lp=%08lx ctlclass=%s visible=%lu "
            "active=%p focus=%p depth=%lu validate=%p context=%08lx/%08lx/%08lx\n",
            (unsigned long)s_side, (unsigned long)s_case, object, (long)page,
            (unsigned long)wparam, (unsigned long)LOWORD(wparam),
            (unsigned long)HIWORD(wparam), (unsigned long)lparam, control_class,
            (unsigned long)IsWindowVisible((HWND)D32(object, 0x20)),
            GetActiveWindow(), GetFocus(),
            (unsigned long)s_flow_dispatch_depth[s_side],
            PTR(object, 0xd8), (unsigned long)D32(object, 0x34),
            (unsigned long)D32(object, 0x38), (unsigned long)D32(object, 0x3c));
    result = ((int32_t (TC *)(void *, uint32_t, uint32_t))
        s_flow_oncommand_originals[s_side][page])(object, wparam, lparam);
    if ((s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210) &&
        ((page == 1 && LOWORD(wparam) == 0x419) ||
         (page == 4 && (LOWORD(wparam) == 0x3ff || LOWORD(wparam) == 2 ||
                        LOWORD(wparam) == 0x3e9))))
        fprintf(stderr, "FLOW ONCOMMAND EXIT side=%lu case=%lu this=%p page=%ld "
            "wp=%08lx result=%08lx visible=%lu active=%p focus=%p callbacks=%lu "
            "depth=%lu context=%08lx/%08lx/%08lx\n",
            (unsigned long)s_side, (unsigned long)s_case, object, (long)page,
            (unsigned long)wparam, (unsigned long)result,
            (unsigned long)IsWindowVisible((HWND)D32(object, 0x20)),
            GetActiveWindow(), GetFocus(),
            (unsigned long)s_flow_results[s_side].validation_count,
            (unsigned long)s_flow_dispatch_depth[s_side],
            (unsigned long)D32(object, 0x34), (unsigned long)D32(object, 0x38),
            (unsigned long)D32(object, 0x3c));
    return result;
}

static void TC fs98_flow_connection_next_probe(void *object)
{
    void *block = PTR(object, 0x18f8);
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW CONNECTION +cc ENTER side=%lu case=%lu this=%p "
            "hwnd=%p block=%p mode=%lu result=%lu selected=%lu vis=%08lx "
            "depth=%lu active=%p focus=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (void *)(uintptr_t)D32(object, 0x20), block,
            block ? (unsigned long)D32(block, 0x24) : 0,
            block ? (unsigned long)D32(block, 0) : 0,
            block ? (unsigned long)D32(block, 0x90) : 0,
            (unsigned long)fs98_flow_visible_mask(s_side),
            (unsigned long)s_flow_dispatch_depth[s_side],
            GetActiveWindow(), GetFocus());
    ((void (TC *)(void *))s_flow_connection_next_originals[s_side])(object);
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW CONNECTION +cc EXIT side=%lu case=%lu this=%p "
            "mode=%lu result=%lu selected=%lu vis=%08lx depth=%lu active=%p focus=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            block ? (unsigned long)D32(block, 0x24) : 0,
            block ? (unsigned long)D32(block, 0) : 0,
            block ? (unsigned long)D32(block, 0x90) : 0,
            (unsigned long)fs98_flow_visible_mask(s_side),
            (unsigned long)s_flow_dispatch_depth[s_side],
            GetActiveWindow(), GetFocus());
}

static void TC fs98_flow_provider_map_probe(void *object)
{
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW PROVIDER MAP ENTER side=%lu case=%lu this=%p "
            "hwnd=%p target=%p slotcc=%p msg=%08lx\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (void *)(uintptr_t)D32(object, 0x20), s_flow_provider_original[s_side],
            ((void **)PTR(object, 0))[0xcc / 4], (unsigned long)D32(object, 0x38));
    ((void (TC *)(void *))s_flow_provider_original[s_side])(object);
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW PROVIDER MAP EXIT side=%lu case=%lu this=%p mode=%lu "
            "result=%lu vis=%08lx depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (unsigned long)D32(PTR(object, 0x18f8), 0x24),
            (unsigned long)D32(PTR(object, 0x18f8), 0),
            (unsigned long)fs98_flow_visible_mask(s_side),
            (unsigned long)s_flow_dispatch_depth[s_side]);
}

static void TC fs98_flow_browser_timer_probe(void *object, uint32_t timer_id)
{
    struct fs98_flow_result *result = &s_flow_results[s_side];
    HWND window = (HWND)D32(object, 0x20);
    ++result->browser_timer_dispatches;
    fprintf(stderr, "FLOW BROWSER TIMER ENTER side=%lu case=%lu object=%p hwnd=%p "
        "id=%lu guard=%lu visible=%lu focus=%p active=%p callbacks=%lu\n",
        (unsigned long)s_side, (unsigned long)s_case, object, window,
        (unsigned long)timer_id, (unsigned long)D32(object, 0x1aec),
        (unsigned long)IsWindowVisible(window), GetFocus(), GetActiveWindow(),
        (unsigned long)result->callback_count);
    ((void (TC *)(void *, uint32_t))s_flow_browser_timer_original[s_side])(
        object, timer_id);
}

static void TC fs98_flow_post_hook(void *object)
{
    int32_t page = fs98_flow_page_for_object(object);
    struct fs98_flow_result *result = &s_flow_results[s_side];
    if (page < 0) fs98_fail("flow PostNcDestroy object identity");
    if (page == 4 && (s_case == 203 || s_case == 204 || s_case == 210))
        fprintf(stderr, "FLOW POST side=%lu case=%lu object=%p tick=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (unsigned long)s_flow_lifecycle_tick);
    ++result->post_destroy[page];
    result->post_order[page] = ++s_flow_lifecycle_tick;
    ((void (TC *)(void *))s_flow_post_originals[s_side][page])(object);
}

static void *TC fs98_flow_delete_hook(void *object, uint32_t flags)
{
    int32_t page = fs98_flow_page_for_object(object);
    struct fs98_flow_result *result = &s_flow_results[s_side];
    void *returned;
    if (page < 0) fs98_fail("flow deleting-destructor object identity");
    if (page == 4 && (s_case == 203 || s_case == 204 || s_case == 210))
        fprintf(stderr, "FLOW DELETE side=%lu case=%lu object=%p flags=%lu tick=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, object,
            (unsigned long)flags, (unsigned long)s_flow_lifecycle_tick);
    ++result->deleting_dtor[page];
    result->delete_flags[page] = flags;
    result->delete_order[page] = ++s_flow_lifecycle_tick;
    returned = ((void *(TC *)(void *, uint32_t))s_flow_delete_originals[s_side][page])(object, flags);
    return returned;
}

static void TC fs98_flow_close_hook(void *object)
{
    int32_t page = fs98_flow_page_for_object(object);
    HWND window = (HWND)D32(object, 0x20);
    HWND auxiliary = (HWND)D32(object, 0x10d4);
    if (page < 0) fs98_fail("flow Close object identity");
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW CLOSE ENTER side=%lu case=%lu object=%p hwnd=%p aux=%p aux_iswindow=%lu userdata=%08lx mfc=%08lx/%08lx/%08lx\n",
            (unsigned long)s_side, (unsigned long)s_case, object, window, auxiliary,
            (unsigned long)(auxiliary ? IsWindow(auxiliary) : 0),
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_USERDATA),
            (unsigned long)D32(object, 0x34), (unsigned long)D32(object, 0x38),
            (unsigned long)D32(object, 0x3c));
    ((void (TC *)(void *))s_flow_close_originals[s_side][page])(object);
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW CLOSE EXIT side=%lu case=%lu object=%p hwnd=%p userdata=%08lx mfc=%08lx/%08lx/%08lx active=%p focus=%p visible=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, object, window,
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_USERDATA),
            (unsigned long)D32(object, 0x34), (unsigned long)D32(object, 0x38),
            (unsigned long)D32(object, 0x3c), GetActiveWindow(), GetFocus(),
            (unsigned long)IsWindowVisible(window));
}

static int32_t TC fs98_flow_error_hook(void *object, int32_t field)
{
    int32_t page = fs98_flow_page_for_object(object);
    struct fs98_flow_result *result = &s_flow_results[s_side];
    if (page < 0) fs98_fail("flow DisplayError object identity");
    ++result->error_count[page];
    result->error_field[page] = (uint32_t)field;
    if (field) SetFocus(GetDlgItem((HWND)D32(object, 0x20), field));
    return 1;
}

static LRESULT CALLBACK fs98_flow_nc_destroy_proc(HWND window, UINT message,
                                                   WPARAM wparam, LPARAM lparam)
{
    WNDPROC previous = (WNDPROC)GetPropA(window, "MPD_FLOW_PREVPROC");
    int32_t page = fs98_flow_page_for_hwnd(s_side, window);
    uint32_t trace = s_case == 203 || message == WM_COMMAND || message == WM_ACTIVATE ||
        message == WM_ACTIVATEAPP ||
        message == WM_ENABLE || message == WM_SETFOCUS || message == WM_KILLFOCUS ||
        message == WM_SHOWWINDOW || message == WM_WINDOWPOSCHANGING ||
        message == WM_WINDOWPOSCHANGED || message == WM_NCACTIVATE ||
        message == WM_ACTIVATEAPP || message == WM_CANCELMODE ||
        message == WM_PARENTNOTIFY || message == WM_NEXTDLGCTL ||
        message == WM_SYSCOMMAND || message == WM_DESTROY ||
        message == WM_NCDESTROY || message == 0x464 ||
        message == 0x5f4;
    uint32_t depth = s_flow_dispatch_depth[s_side];
    LRESULT result;
    if (message == WM_ACTIVATE &&
        (s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210)) {
        FARPROC from_handle_proc = GetProcAddress(s_mfc, (const char *)(uintptr_t)2864);
        void *(SC *from_handle)(HWND);
        void *mapped;
        void **mapped_vtable;
        void *mapped_map;
        memcpy(&from_handle, &from_handle_proc, sizeof(from_handle));
        mapped = from_handle(window);
        mapped_vtable = mapped ? PTR(mapped, 0) : 0;
        mapped_map = mapped_vtable && mapped_vtable[0x30 / 4] ?
            ((void *(TC *)(void *))mapped_vtable[0x30 / 4])(mapped) : 0;
        fprintf(stderr, "FLOW ACTIVATE MAP side=%lu case=%lu page=%ld hwnd=%p mapped=%p "
            "expected=%p vtable=%p getter=%p tuple=%08lx/%08lx/%08lx userdata=%08lx\n",
            (unsigned long)s_side, (unsigned long)s_case, (long)page, window, mapped,
            page >= 0 ? s_flow_dialog_objects[s_side][page] : 0,
            mapped_vtable, mapped_map,
            mapped ? (unsigned long)D32(mapped, 0x34) : 0,
            mapped ? (unsigned long)D32(mapped, 0x38) : 0,
            mapped ? (unsigned long)D32(mapped, 0x3c) : 0,
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_USERDATA));
    }
    if (trace) fs98_flow_trace_event(s_side, 1, page, message,
        (uint32_t)wparam, (uint32_t)lparam, depth, 0);
    if (message == WM_NCDESTROY && page == 4 &&
        (s_case == 203 || s_case == 204 || s_case == 210)) {
        void *object = s_flow_dialog_objects[s_side][4];
        void **vtable = object ? PTR(object, 0) : 0;
        FARPROC from_handle_proc = GetProcAddress(s_mfc,
            (const char *)(uintptr_t)2864);
        void *(SC *from_handle)(HWND);
        void *mapped;
        memcpy(&from_handle, &from_handle_proc, sizeof(from_handle));
        mapped = from_handle(window);
        fprintf(stderr, "FLOW NC side=%lu case=%lu hwnd=%p object=%p mapped=%p "
            "style=%08lx exstyle=%08lx parent=%p owner=%p active=%p focus=%p "
            "objhwnd=%p vtable=%p post=%p delete=%p depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, window, object, mapped,
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_STYLE),
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_EXSTYLE),
            GetParent(window), GetWindow(window, GW_OWNER), GetActiveWindow(), GetFocus(),
            object ? (void *)(uintptr_t)D32(object, 0x20) : 0, vtable,
            vtable ? vtable[0xac / 4] : 0, vtable ? vtable[4 / 4] : 0,
            (unsigned long)depth);
    }
    if (message == WM_COMMAND && page == 1 && HIWORD(wparam) == 0x0200 &&
        (s_case == 200 || s_case == 201)) {
        void *object = s_flow_dialog_objects[s_side][1];
        void *block = object ? PTR(object, 0x1cac) : 0;
        fprintf(stderr, "FLOW VALIDATE side=%lu case=%lu hwnd=%p wp=%08lx lp=%08lx "
            "focus=%p active=%p visible=%lu style=%08lx callsign=%s status=%08lx pending=%08lx pending_hwnd=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, window,
            (unsigned long)(uint32_t)wparam, (unsigned long)(uint32_t)lparam,
            GetFocus(), GetActiveWindow(), (unsigned long)IsWindowVisible(window),
            (unsigned long)(uint32_t)GetWindowLongA(window, GWL_STYLE), block && D32(block, 0x2c) ?
                (const char *)(uintptr_t)D32(block, 0x2c) : "<null>",
            block ? (unsigned long)D32(block, 0x28) : 0,
            object ? (unsigned long)D32(object, 0x7d0) : 0,
            object ? (void *)(uintptr_t)D32(object, 0x7d4) : 0);
    }
    if (message == WM_NCDESTROY) {
        if (page >= 0) {
            ++s_flow_results[s_side].nc_destroy[page];
            s_flow_results[s_side].nc_order[page] = ++s_flow_lifecycle_tick;
        }
    }
    if (!previous) fs98_fail("flow prior window procedure property");
    ++s_flow_dispatch_depth[s_side];
    result = CallWindowProcA(previous, window, message, wparam, lparam);
    --s_flow_dispatch_depth[s_side];
    if (trace) fs98_flow_trace_event(s_side, 2, page, message,
        (uint32_t)wparam, (uint32_t)lparam, s_flow_dispatch_depth[s_side], 0);
    return result;
}

static void fs98_patch_flow_page(HWND window, uint32_t page, HMODULE expected)
{
    void *from_handle = (void *)GetProcAddress(s_mfc, (const char *)(uintptr_t)2864);
    void *object = ((void *(SC *)(HWND))from_handle)(window);
    void **vtable;
    HMODULE owner = 0, post_owner = 0, delete_owner = 0, close_owner = 0, wndproc_owner = 0;
    void *post_target, *delete_target, *close_target;
    LONG previous;
    if (!object || page >= 5) fs98_fail("flow page FromHandle");
    vtable = PTR(object, 0);
    if (!vtable || !vtable[0x30 / 4]) fs98_fail("flow page vtable");
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (const char *)vtable[0x30 / 4], &owner);
    if (owner != expected) fs98_fail("flow page map getter belongs to wrong module");
    s_flow_dialog_objects[s_side][page] = object;
    if (page != 3) fs98_flow_patch_base_activate(object, vtable);
    post_target = vtable[0xac / 4];
    delete_target = vtable[4 / 4];
    close_target = vtable[0xec / 4];
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const char *)post_target, &post_owner) || post_owner != expected)
        fs98_fail("flow PostNcDestroy slot owner");
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const char *)delete_target, &delete_owner) || delete_owner != expected)
        fs98_fail("flow deleting-destructor slot owner");
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const char *)close_target, &close_owner) ||
        close_owner != expected) {
        fprintf(stderr, "FLOW CLOSE OWNER side=%lu page=%lu target=%p actual=%p expected=%p\n",
            (unsigned long)s_side, (unsigned long)page, close_target, close_owner,
            expected);
        fs98_fail("flow Close slot owner");
    }
    s_flow_post_slots[s_side][page] = &vtable[0xac / 4];
    s_flow_delete_slots[s_side][page] = &vtable[4 / 4];
    s_flow_close_slots[s_side][page] = &vtable[0xec / 4];
    s_flow_error_slots[s_side][page] = &vtable[0xe0 / 4];
    fs98_patch_slot(s_flow_post_slots[s_side][page], (void *)fs98_flow_post_hook,
                    &s_flow_post_originals[s_side][page]);
    fs98_patch_slot(s_flow_delete_slots[s_side][page], (void *)fs98_flow_delete_hook,
                    &s_flow_delete_originals[s_side][page]);
    fs98_patch_slot(s_flow_close_slots[s_side][page], (void *)fs98_flow_close_hook,
                    &s_flow_close_originals[s_side][page]);
    fs98_patch_slot(s_flow_error_slots[s_side][page], (void *)fs98_flow_error_hook,
                    &s_flow_error_originals[s_side][page]);
    if (page == 1) {
        void **slot = &vtable[0xd8 / 4];
        HMODULE validate_owner = 0;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (const char *)*slot, &validate_owner) ||
            validate_owner != expected)
            fs98_fail("flow Choice validation slot owner");
        if (s_side == 0 && (uint32_t)(uintptr_t)*slot != 0x22345ab0)
            fs98_fail("retail Choice validation method address");
        s_flow_validate_slots[s_side] = slot;
        fs98_patch_slot(slot, (void *)fs98_flow_validate_probe,
                        &s_flow_validate_original[s_side]);
        fprintf(stderr, "FLOW VALIDATE METHOD side=%lu case=%lu slot=%p target=%p hooked=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, slot,
            s_flow_validate_original[s_side], (void *)fs98_flow_validate_probe);
    }
    if (page == 1 || page == 3 || page == 4) {
        void **slot = &vtable[0x80 / 4];
        s_flow_oncommand_slots[s_side][page] = slot;
        fs98_patch_slot(slot, (void *)fs98_flow_oncommand_probe,
                        &s_flow_oncommand_originals[s_side][page]);
        fprintf(stderr, "FLOW ONCOMMAND METHOD side=%lu case=%lu page=%lu slot=%p target=%p hooked=%p\n",
            (unsigned long)s_side, (unsigned long)s_case, (unsigned long)page,
            slot, s_flow_oncommand_originals[s_side][page],
            (void *)fs98_flow_oncommand_probe);
    }
    if (page == 3) {
        HMODULE connection_handler_owner = 0;
        void *connection_handler = vtable[0xcc / 4];
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (const char *)connection_handler,
                                &connection_handler_owner) ||
            connection_handler_owner != expected)
            fs98_fail("Connection +0xcc handler owner");
        if (s_side == 0 &&
            (uint32_t)(uintptr_t)connection_handler != 0x22341c00)
            fs98_fail("retail Connection +0xcc handler address");
        fprintf(stderr, "FLOW CONNECTION HANDLER METHOD side=%lu case=%lu "
            "slot=+0xcc target=%p owner=%p\n", (unsigned long)s_side,
            (unsigned long)s_case, connection_handler, connection_handler_owner);
        s_flow_connection_next_slots[s_side] = &vtable[0xcc / 4];
        fs98_patch_slot(s_flow_connection_next_slots[s_side],
                        (void *)fs98_flow_connection_next_probe,
                        &s_flow_connection_next_originals[s_side]);
        fs98_flow_patch_provider_map(object, vtable);
    }
    if (page == 4) fs98_flow_patch_browser_timer(object, vtable, expected);
    if (page == 3) fs98_flow_patch_activate(object, vtable);
    previous = GetWindowLongA(window, GWL_WNDPROC);
    if (!previous) fs98_fail("flow page window procedure");
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const char *)(uintptr_t)previous, &wndproc_owner) || wndproc_owner != s_mfc)
        fs98_fail("flow page MFC window-procedure owner");
    s_flow_mfc_proc[s_side][page] = (WNDPROC)(uintptr_t)previous;
    s_flow_previous_proc[s_side][page] = (WNDPROC)(uintptr_t)previous;
    if (!SetPropA(window, "MPD_FLOW_PREVPROC", (HANDLE)s_flow_previous_proc[s_side][page]))
        fs98_fail("flow page prior proc property");
    previous = SetWindowLongA(window, GWL_WNDPROC, (LONG)(uintptr_t)fs98_flow_nc_destroy_proc);
    if (!previous) fs98_fail("flow page subclass");
    if (s_case == 200 || s_case == 203)
        fprintf(stderr, "FLOW HOOK side=%lu page=%lu hwnd=%p object=%p wndproc=%p post=%p deleting=%p close=%p owners=%p,%p,%p,%p\n",
            (unsigned long)s_side, (unsigned long)page, window, object,
            (void *)s_flow_mfc_proc[s_side][page], post_target, delete_target, close_target,
            expected, post_owner, delete_owner, close_owner);
}

static void fs98_patch_flow_main(void)
{
    LONG previous;
    s_flow_main_previous_proc = (WNDPROC)(uintptr_t)GetWindowLongA(
        s_main_window, GWL_WNDPROC);
    if (!s_flow_main_previous_proc) fs98_fail("flow main prior window procedure");
    if (!SetPropA(s_main_window, "MPD_FLOW_PREVPROC",
                  (HANDLE)s_flow_main_previous_proc))
        fs98_fail("flow main prior proc property");
    previous = SetWindowLongA(s_main_window, GWL_WNDPROC,
                              (LONG)(uintptr_t)fs98_flow_nc_destroy_proc);
    if (!previous) fs98_fail("flow main subclass");
}

static void fs98_restore_flow_main(void)
{
    if (s_flow_main_previous_proc) {
        SetWindowLongA(s_main_window, GWL_WNDPROC,
                       (LONG)(uintptr_t)s_flow_main_previous_proc);
        RemovePropA(s_main_window, "MPD_FLOW_PREVPROC");
        s_flow_main_previous_proc = 0;
    }
}

static void fs98_restore_flow_pages(void)
{
    uint32_t index;
    fs98_flow_restore_base_activate();
    fs98_flow_restore_activate();
    fs98_flow_restore_provider_map();
    fs98_flow_restore_browser_timer();
    if (s_flow_connection_next_slots[s_side]) {
        fs98_restore_slot(s_flow_connection_next_slots[s_side],
                          s_flow_connection_next_originals[s_side]);
        s_flow_connection_next_slots[s_side] = 0;
        s_flow_connection_next_originals[s_side] = 0;
    }
    for (index = 0; index < 5; ++index) {
        if (s_flow_oncommand_slots[s_side][index]) {
            fs98_restore_slot(s_flow_oncommand_slots[s_side][index],
                              s_flow_oncommand_originals[s_side][index]);
            s_flow_oncommand_slots[s_side][index] = 0;
            s_flow_oncommand_originals[s_side][index] = 0;
        }
    }
    if (s_flow_validate_slots[s_side]) {
        fs98_restore_slot(s_flow_validate_slots[s_side], s_flow_validate_original[s_side]);
        s_flow_validate_slots[s_side] = 0;
        s_flow_validate_original[s_side] = 0;
    }
    for (index = 0; index < 5; ++index) {
        if (s_flow_post_slots[s_side][index]) {
            fs98_restore_slot(s_flow_post_slots[s_side][index], s_flow_post_originals[s_side][index]);
            fs98_restore_slot(s_flow_delete_slots[s_side][index], s_flow_delete_originals[s_side][index]);
            fs98_restore_slot(s_flow_close_slots[s_side][index], s_flow_close_originals[s_side][index]);
            fs98_restore_slot(s_flow_error_slots[s_side][index], s_flow_error_originals[s_side][index]);
            s_flow_post_slots[s_side][index] = s_flow_delete_slots[s_side][index] =
                s_flow_close_slots[s_side][index] = 0;
            s_flow_error_slots[s_side][index] = 0;
        }
        if (s_flow_windows[s_side][index] && IsWindow(s_flow_windows[s_side][index]))
            RemovePropA(s_flow_windows[s_side][index], "MPD_FLOW_PREVPROC");
    }
    fs98_restore_flow_main();
}

static void fs98_pump(void)
{
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

static void fs98_setup_block(uint32_t case_index)
{
    struct fs98_mpdialog_owner_block *block = &s_blocks[s_side];
    memset(block, 0, sizeof(*block));
    block->read = fs98_read_callback;
    block->accept = fs98_accept_callback;
    block->details = fs98_details_callback;
    block->weather = fs98_weather_callback;
    block->sync = fs98_sync_callback;
    block->reserved18 = (case_index >> 1) & 1;
    block->flight_conditions = case_index & 1;
    block->adf_lock = (case_index >> 1) & 1;
    block->dme_lock = (case_index >> 2) & 1;
    block->reserved30 = (case_index >> 3) & 1;
    block->autopilot_lock = (case_index >> 1) & 1;
    block->send_details = (case_index >> 2) & 1;
    block->receive_details = (case_index >> 3) & 1;
    block->player_names = (case_index >> 2) & 1;
    s_test_owner_block[s_side] = block;
}

static int32_t fs98_is_checked(void *control)
{
    int32_t (TC *is_checked)(void *) = (void *)GetProcAddress(s_ui,
        "?IsChecked@CSimsCheckButton@@QAEHXZ");
    if (!is_checked) fs98_fail("SIMSUI CSimsCheckButton::IsChecked");
    return is_checked(control);
}

static int32_t fs98_set_checked(void *control, int32_t checked)
{
    int32_t (TC *set_checked)(void *, int32_t) = (void *)GetProcAddress(s_ui,
        "?SetChecked@CSimsCheckButton@@QAEHH@Z");
    if (!set_checked) fs98_fail("SIMSUI CSimsCheckButton::SetChecked");
    return set_checked(control, checked);
}

static const uint32_t s_check_offsets[6] = {
    0x1d10, 0x1b80, 0x1c48, 0x1f68, 0x1dd8, 0x1ea0
};

static void fs98_capture_checks(void *object, uint32_t checks[6])
{
    uint32_t index;
    for (index = 0; index < 6; ++index)
        checks[index] = (uint32_t)fs98_is_checked((uint8_t *)object + s_check_offsets[index]);
}

static void fs98_capture_init(HWND window, void *object, struct fs98_result *result)
{
    static const int s_buttons[] = {1027, 1003, 1006};
    static const uint32_t s_offsets[] = {0x12d4, 0x13d0, 0x14cc};
    uint32_t index;
    result->dialog_visible_after_create = (uint32_t)IsWindowVisible(window);
    fs98_capture_checks(object, result->initial_checks);
    for (index = 0; index < 3; ++index) {
        HWND control = GetDlgItem(window, s_buttons[index]);
        HWND member = *(HWND *)((uint8_t *)object + s_offsets[index] + 0x20);
        if (!control) fs98_fail("template control missing");
        result->member_button_ids[index] = member && IsWindow(member) ?
            (uint32_t)GetDlgCtrlID(member) : 0;
        result->initial_buttons[index][0] = (uint32_t)IsWindowEnabled(control);
        result->initial_buttons[index][1] = (uint32_t)IsWindowVisible(control);
        result->initial_button_styles[index] = (uint32_t)GetWindowLongA(control, GWL_STYLE);
        result->initial_button_parent_visible[index] =
            (uint32_t)IsWindowVisible(GetParent(control));
    }
}

static void fs98_capture_buttons(HWND window, uint32_t buttons[3][2])
{
    static const int s_ids[] = {1027, 1003, 1006};
    uint32_t index;
    for (index = 0; index < 3; ++index) {
        HWND control = GetDlgItem(window, s_ids[index]);
        buttons[index][0] = (uint32_t)IsWindowEnabled(control);
        buttons[index][1] = (uint32_t)IsWindowVisible(control);
    }
}

static void fs98_capture_update_styles(HWND window, struct fs98_result *result)
{
    static const int s_ids[] = {1027, 1003, 1006};
    uint32_t index;
    for (index = 0; index < 3; ++index) {
        HWND control = GetDlgItem(window, s_ids[index]);
        result->direct_update_button_styles[index] =
            (uint32_t)GetWindowLongA(control, GWL_STYLE);
        result->direct_update_parent_visible[index] =
            (uint32_t)IsWindowVisible(GetParent(control));
    }
}

static LRESULT CALLBACK fs98_create_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_CREATEWND && lparam) {
        const CBT_CREATEWND *create = (const CBT_CREATEWND *)lparam;
        UINT_PTR control_id = (UINT_PTR)create->lpcs->hMenu;
        uint32_t index;
        for (index = 0; index < 2; ++index) {
            if (control_id == (UINT_PTR)s_visibility_ids[index]) {
                uint32_t count = s_create_counts[s_side][index]++;
                if (count == 0) {
                    s_create_styles[s_side][index] = create->lpcs->style;
                    s_create_parents[s_side][index] = create->lpcs->hwndParent &&
                        IsWindowVisible(create->lpcs->hwndParent);
                }
                break;
            }
        }
    }
    return CallNextHookEx(s_create_hook, code, wparam, lparam);
}

static BOOL CALLBACK fs98_flow_modal_child(HWND child, LPARAM context)
{
    char text[256];
    HWND dialog = (HWND)context;
    text[0] = 0;
    GetWindowTextA(child, text, sizeof(text));
    if (strstr(text, "resource") || strstr(text, "Resource") || strstr(text, "data not found")) {
        char title[256];
        char class_name[64];
        title[0] = 0;
        class_name[0] = 0;
        GetClassNameA(child, class_name, sizeof(class_name));
        GetWindowTextA(dialog, title, sizeof(title));
        fprintf(stderr, "MP_DIALOG 0x110a resource child side=%lu dialog='%s' class=%s id=%d text='%s'\n",
            (unsigned long)s_side, title, class_name, GetDlgCtrlID(child), text);
        s_flow_modal_seen[s_side] = 1;
        PostMessageA(dialog, WM_COMMAND, IDOK, 0);
    }
    return TRUE;
}

static LRESULT CALLBACK fs98_flow_modal_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_ACTIVATE && wparam && s_flow_active &&
        (s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210)) {
        HWND window = (HWND)wparam;
        int32_t page = fs98_flow_message_page(window);
        if (page >= 0)
            fprintf(stderr, "FLOW CBT ACTIVATE side=%lu case=%lu page=%ld hwnd=%p active=%p focus=%p last_popup=%p visible=%08lx\n",
                (unsigned long)s_side, (unsigned long)s_case, (long)page, window,
                GetActiveWindow(), GetFocus(), GetLastActivePopup(s_main_window),
                (unsigned long)fs98_flow_visible_mask(s_side));
    }
    if (code == HCBT_SETFOCUS && wparam && s_flow_active &&
        (s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210)) {
        HWND window = (HWND)wparam;
        int32_t page = fs98_flow_message_page(window);
        if (page >= 0)
            fprintf(stderr, "FLOW CBT SETFOCUS side=%lu case=%lu page=%ld hwnd=%p old=%p active=%p\n",
                (unsigned long)s_side, (unsigned long)s_case, (long)page, window,
                (HWND)lparam, GetActiveWindow());
    }
    if (code == HCBT_ACTIVATE && wparam) {
        char class_name[64];
        GetClassNameA((HWND)wparam, class_name, sizeof(class_name));
        if (!strcmp(class_name, "#32770")) {
            char caption[256];
            caption[0] = 0;
            GetWindowTextA((HWND)wparam, caption, sizeof(caption));
            if (strstr(caption, "resource") || strstr(caption, "Resource") ||
                strstr(caption, "Error")) {
                fprintf(stderr, "MP_DIALOG 0x110a modal diagnostic side=%lu caption='%s'\n",
                    (unsigned long)s_side, caption);
                s_flow_modal_seen[s_side] = 1;
                PostMessageA((HWND)wparam, WM_COMMAND, IDOK, 0);
                EnumChildWindows((HWND)wparam, fs98_flow_modal_child, (LPARAM)wparam);
            }
        }
    }
    return CallNextHookEx(s_flow_modal_hook, code, wparam, lparam);
}

static void fs98_set_checks(void *object, uint32_t case_index)
{
    uint32_t index;
    for (index = 0; index < 6; ++index)
        fs98_set_checked((uint8_t *)object + s_check_offsets[index],
                         (int32_t)((case_index + index) & 1));
}

static void fs98_send_command(HWND window, uint32_t id)
{
    HWND control = GetDlgItem(window, (int)id);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)control);
}

static LONG WINAPI fs98_check_fault(EXCEPTION_POINTERS *exception)
{
    CONTEXT *context = exception->ContextRecord;
    uint32_t *stack = (uint32_t *)context->Esp;
    fprintf(stderr, "MP_DIALOG UI FAULT side=%lu case=%lu code=%08lx pc=%p\n",
        (unsigned long)s_side, (unsigned long)s_case,
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    fprintf(stderr, " eip=%08lx esp=%08lx eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx module=%p stack=%08lx,%08lx,%08lx\n",
        (unsigned long)context->Eip, (unsigned long)context->Esp,
        (unsigned long)context->Eax, (unsigned long)context->Ebx,
        (unsigned long)context->Ecx, (unsigned long)context->Edx,
        s_current_module, (unsigned long)stack[0],
        (unsigned long)stack[1], (unsigned long)stack[2]);
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void fs98_details_set_row(uint32_t side, uint32_t set, uint32_t index,
                                 uint32_t player_id, uint32_t flags, const char *name)
{
    uint8_t *row = s_details_rows[side][set][index];
    D32(row, 0) = player_id;
    D32(row, 8) = flags;
    strcpy(s_details_names[side][set][index], name);
    D32(row, 0x0c) = (uint32_t)(uintptr_t)s_details_names[side][set][index];
}

static void fs98_setup_details(uint32_t side, uint32_t case_index)
{
    uint8_t *block = s_details_blocks[side];
    memset(block, 0, sizeof(s_details_blocks[side]));
    memset(s_details_rows[side], 0, sizeof(s_details_rows[side]));
    memset(s_details_names[side], 0, sizeof(s_details_names[side]));
    memset(s_details_events[side], 0, sizeof(s_details_events[side]));
    memset(&s_details_results[side], 0, sizeof(s_details_results[side]));
    s_details_event_count[side] = s_details_removed[side] = 0;
    s_details_owner_active[side] = 1;
    s_details_refresh[side] = case_index == 0 || case_index == 2;
    s_details_confirm[side] = case_index == 6;
    strcpy(s_details_name[side], "Sierra Session");
    strcpy(s_details_count_a[side], "2 pilots");
    strcpy(s_details_count_b[side], "0 observers");
    strcpy(s_details_pilot[side], "4");
    strcpy(s_details_observer[side], "2");
    strcpy(s_details_limits[side][0], "2");
    strcpy(s_details_limits[side][1], "8");
    strcpy(s_details_limits[side][2], "0");
    strcpy(s_details_limits[side][3], "4");
    fs98_details_set_row(side, 0, 0, 11, 0, "Alpha");
    fs98_details_set_row(side, 0, 1, 22, 0, "Bravo");
    fs98_details_set_row(side, 1, 0, 11, 0, "Alpha");
    fs98_details_set_row(side, 1, 1, 22, 0, "Bravo");
    fs98_details_set_row(side, 1, 2, 33, 0, "Charlie");
    D32(block, 4) = (uint32_t)(uintptr_t)fs98_details_validate_callback;
    D32(block, 8) = (uint32_t)(uintptr_t)fs98_details_accept_callback;
    D32(block, 0x0c) = (uint32_t)(uintptr_t)fs98_details_remove_callback;
    D32(block, 0x10) = (uint32_t)(uintptr_t)fs98_details_refresh_callback;
    D32(block, 0x14) = case_index == 0 ? 0 : 1;
    D32(block, 0x18) = (uint32_t)(uintptr_t)s_details_name[side];
    D32(block, 0x20) = (uint32_t)(uintptr_t)s_details_pilot[side];
    D32(block, 0x24) = sizeof(s_details_pilot[side]);
    D32(block, 0x28) = (uint32_t)(uintptr_t)s_details_limits[side][0];
    D32(block, 0x2c) = (uint32_t)(uintptr_t)s_details_limits[side][1];
    D32(block, 0x34) = (uint32_t)(uintptr_t)s_details_observer[side];
    D32(block, 0x38) = sizeof(s_details_observer[side]);
    D32(block, 0x3c) = (uint32_t)(uintptr_t)s_details_limits[side][2];
    D32(block, 0x40) = (uint32_t)(uintptr_t)s_details_limits[side][3];
    D32(block, 0x44) = (uint32_t)(uintptr_t)s_details_count_a[side];
    D32(block, 0x48) = (uint32_t)(uintptr_t)s_details_count_b[side];
    D32(block, 0x4c) = (uint32_t)(uintptr_t)s_details_rows[side][0];
    D32(block, 0x50) = 2;
    D32(block, 0x58) = 0;
    D32(block, 0xbc) = 0xbc110c00u;
    D32(block, 0xd0) = 0xd0110c00u;
    s_details_results[side].initial_opaque_bc = 0xbc110c00u;
    s_details_results[side].initial_opaque_d0 = 0xd0110c00u;
    s_details_results[side].mode = D32(block, 0x14);
    /* MULTIP's owner initialization calls +0x04 once before HOST creates UI. */
    (void)fs98_details_validate_callback(block);
}

static void fs98_modeled_host_idle_tick(HWND window)
{
    /* Static HOST evidence: single-modal idle calls SendMessageA(hwnd,0x464,0,0). */
    SendMessageA(window, 0x464, 0, 0);
}

static void fs98_modeled_host_flow_idle_tick(HWND *windows)
{
    uint32_t index;
    /* The flag-2 loop broadcasts the idle message over the terminating HWND array. */
    for (index = 0; index < 5; ++index)
        SendMessageA(windows[index], 0x464, 0, 0);
}

static void fs98_capture_details(HWND window, uint32_t side)
{
    static const int s_button_ids[4] = {IDOK, IDCANCEL, 0x3ea, 0x3ef};
    static const uint32_t s_button_offsets[4] = {0x12d4, 0x10dc, 0x11d8, 0x13d0};
    struct fs98_details_result *result = &s_details_results[side];
    uint8_t *block = s_details_blocks[side];
    HWND list = GetDlgItem(window, 0x406);
    uint8_t *rows = (uint8_t *)D32(block, 0x4c);
    uint32_t index, count;
    result->mode = D32(block, 0x14);
    result->block_result = D32(block, 0);
    result->visible = (uint32_t)IsWindowVisible(window);
    result->ok_enabled = (uint32_t)IsWindowEnabled(GetDlgItem(window, IDOK));
    result->cancel_enabled = (uint32_t)IsWindowEnabled(GetDlgItem(window, IDCANCEL));
    result->close_enabled = (uint32_t)IsWindowEnabled(GetDlgItem(window, 0x3ea));
    result->remove_enabled = (uint32_t)IsWindowEnabled(GetDlgItem(window, 0x3ef));
    for (index = 0; index < 4; ++index) {
        result->button_visible[index] = (uint32_t)IsWindowVisible(GetDlgItem(window, s_button_ids[index]));
        {
            HWND member = (HWND)D32(s_test_dialog[side], s_button_offsets[index] + 0x20);
            result->button_members[index] = member && IsWindow(member) ? (uint32_t)GetDlgCtrlID(member) : 0;
        }
    }
    result->list_count = list ? (uint32_t)SendMessageA(list, LB_GETCOUNT, 0, 0) : 0xffffffff;
    result->selected = list ? (uint32_t)SendMessageA(list, LB_GETCURSEL, 0, 0) : 0xffffffff;
    count = D32(block, 0x50);
    if (count > 4) count = 4;
    for (index = 0; index < count; ++index) result->row_ids[index] = D32(rows + index * 16, 0);
    for (index = 0; list && index < 4 && index < result->list_count; ++index)
        SendMessageA(list, LB_GETTEXT, index, (LPARAM)result->rows[index]);
    GetWindowTextA(GetDlgItem(window, 0x40b), result->pilot, sizeof(result->pilot));
    GetWindowTextA(GetDlgItem(window, 0x40d), result->observer, sizeof(result->observer));
    GetWindowTextA(GetDlgItem(window, 0x405), result->session, sizeof(result->session));
    {
        LRESULT packed = SendMessageA(GetDlgItem(window, 0x40c), WM_USER + 102, 0, 0);
        result->pilot_range[0] = LOWORD(packed); result->pilot_range[1] = HIWORD(packed);
        packed = SendMessageA(GetDlgItem(window, 0x40e), WM_USER + 102, 0, 0);
        result->observer_range[0] = LOWORD(packed); result->observer_range[1] = HIWORD(packed);
    }
    result->opaque_bc = D32(block, 0xbc);
    result->opaque_d0 = D32(block, 0xd0);
    result->opaque_changed = result->opaque_changed ||
        result->opaque_bc != result->initial_opaque_bc || result->opaque_d0 != result->initial_opaque_d0;
    result->pilot_status = D32(block, 0x1c);
    result->observer_status = D32(block, 0x30);
    result->removal_confirmed = s_details_confirm[side];
    result->removed_player = s_details_removed[side];
    result->callbacks = s_details_event_count[side];
    for (index = 0; index < result->callbacks && index < 16; ++index) {
        result->callback_ids[index] = s_details_events[side][index].id;
        result->callback_args[index] = s_details_events[side][index].arg_is_block;
        result->callback_results[index] = s_details_events[side][index].reserved;
        result->callback_values[index] = s_details_events[side][index].value;
    }
}

static void fs98_save_details_initial(uint32_t side)
{
    struct fs98_details_result *result = &s_details_results[side];
    memcpy(result->initial_buttons, (uint32_t[]){result->ok_enabled, result->cancel_enabled,
        result->close_enabled, result->remove_enabled}, sizeof(result->initial_buttons));
    memcpy(result->initial_visible, result->button_visible, sizeof(result->initial_visible));
    result->initial_list_count = result->list_count;
    result->initial_selected = result->selected;
    memcpy(result->initial_row_ids, result->row_ids, sizeof(result->initial_row_ids));
    memcpy(result->initial_rows, result->rows, sizeof(result->initial_rows));
    memcpy(result->initial_pilot, result->pilot, sizeof(result->initial_pilot));
    memcpy(result->initial_observer, result->observer, sizeof(result->initial_observer));
    memcpy(result->initial_session, result->session, sizeof(result->initial_session));
    memcpy(result->initial_ranges, (uint32_t[]){result->pilot_range[0], result->pilot_range[1],
        result->observer_range[0], result->observer_range[1]}, sizeof(result->initial_ranges));
    result->initial_opaque_bc = result->opaque_bc;
    result->initial_opaque_d0 = result->opaque_d0;
}

static void fs98_flow_setup(uint32_t side, uint32_t case_index)
{
    uint8_t *block = s_flow_blocks[side];
    uint32_t row;
    memset(block, 0, sizeof(s_flow_blocks[side]));
    memset(s_flow_providers[side], 0, sizeof(s_flow_providers[side]));
    memset(s_flow_rows[side], 0, sizeof(s_flow_rows[side]));
    memset(s_flow_events[side], 0, sizeof(s_flow_events[side]));
    memset(&s_flow_results[side], 0, sizeof(s_flow_results[side]));
    memset(s_flow_trace[side], 0, sizeof(s_flow_trace[side]));
    s_flow_event_count[side] = s_flow_idle_ticks[side] = 0;
    s_flow_trace_count[side] = s_flow_dispatch_depth[side] = 0;
    s_flow_windows[side] = 0;
    s_flow_refresh_reentry_pending[side] = case_index == 3;
    s_flow_owner_active[side] = 1;
    s_flow_provider_failures[side] = case_index == 4 ? 1 : 0;
    s_flow_accept_result[side] = case_index == 5 ? 1 : 0;
    strcpy(s_flow_callsigns[side], "OpenFS");
    strcpy(s_flow_host_names[side], "Sierra Session");
    strcpy(s_flow_pilot_caps[side], "4");
    strcpy(s_flow_observer_caps[side], "2");
    strcpy(s_flow_limits[side][0], "2"); /* pilot minimum, descriptor +0x6c */
    strcpy(s_flow_limits[side][1], "8"); /* pilot maximum, descriptor +0x68 */
    strcpy(s_flow_limits[side][2], "0"); /* observer minimum, descriptor +0x7c */
    strcpy(s_flow_limits[side][3], "4"); /* observer maximum, descriptor +0x80 */
    for (row = 0; row < 2; ++row) {
        strcpy(s_flow_provider_names[side][row], row ? "DirectPlay TCP/IP" : "Local Area Network");
        s_flow_providers[side][row][0] = (uint32_t)(uintptr_t)s_flow_provider_names[side][row];
        strcpy(s_flow_session_names[side][row], row ? "Bravo Session" : "Alpha Session");
        D32(s_flow_rows[side][0][row], 0) =
            (uint32_t)(uintptr_t)s_flow_session_names[side][row];
        D32(s_flow_rows[side][0][row], 0x20) = 2;
        D32(s_flow_rows[side][0][row], 0x24) = 0;
        D32(s_flow_rows[side][0][row], 0x28) = 8;
        D32(s_flow_rows[side][0][row], 0x2c) = 4;
        memcpy(s_flow_rows[side][1][row], s_flow_rows[side][0][row], 84);
    }
    D32(block, 4) = (uint32_t)(uintptr_t)fs98_flow_provider_callback;
    D32(block, 8) = (uint32_t)(uintptr_t)fs98_flow_reset_callback;
    D32(block, 0x0c) = (uint32_t)(uintptr_t)fs98_flow_accept_callback;
    D32(block, 0x10) = (uint32_t)(uintptr_t)fs98_flow_validate_callback;
    D32(block, 0x1c) = case_index == 0 ? 0 : 1;
    D32(block, 0x24) = 0;
    D32(block, 0x2c) = (uint32_t)(uintptr_t)s_flow_callsigns[side];
    D32(block, 0x30) = sizeof(s_flow_callsigns[side]);
    D32(block, 0x40) = 1;
    D32(block, 0x44) = (uint32_t)(uintptr_t)fs98_flow_host_settings_callback;
    D32(block, 0x48) = 0;
    D32(block, 0x4c) = (uint32_t)(uintptr_t)s_flow_host_names[side];
    D32(block, 0x50) = sizeof(s_flow_host_names[side]);
    D32(block, 0x5c) = 0;
    D32(block, 0x60) = (uint32_t)(uintptr_t)s_flow_pilot_caps[side];
    D32(block, 0x64) = sizeof(s_flow_pilot_caps[side]);
    D32(block, 0x68) = (uint32_t)(uintptr_t)s_flow_limits[side][1];
    D32(block, 0x6c) = (uint32_t)(uintptr_t)s_flow_limits[side][0];
    D32(block, 0x70) = 0;
    D32(block, 0x74) = (uint32_t)(uintptr_t)s_flow_observer_caps[side];
    D32(block, 0x78) = sizeof(s_flow_observer_caps[side]);
    D32(block, 0x7c) = (uint32_t)(uintptr_t)s_flow_limits[side][2];
    D32(block, 0x80) = (uint32_t)(uintptr_t)s_flow_limits[side][3];
    D32(block, 0x84) = (uint32_t)(uintptr_t)s_flow_providers[side];
    D32(block, 0x88) = case_index == 0 ? 65 : (case_index == 1 ? 64 : 2);
    for (row = 2; row < D32(block, 0x88); ++row) {
        sprintf(s_flow_provider_names[side][row], "Provider %02lu", (unsigned long)row);
        s_flow_providers[side][row][0] = (uint32_t)(uintptr_t)s_flow_provider_names[side][row];
    }
    D32(block, 0xa8) = case_index & 1;
    D32(block, 0x40) = (case_index & 2) ? 2 : 1;
    D32(block, 0x90) = 0;
    D32(block, 0x94) = (uint32_t)(uintptr_t)fs98_flow_refresh_callback;
    D32(block, 0x98) = (uint32_t)(uintptr_t)fs98_flow_selection_callback;
    D32(block, 0x9c) = (uint32_t)(uintptr_t)fs98_flow_details_callback;
    D32(block, 0xa0) = (uint32_t)(uintptr_t)fs98_flow_refresh_callback;
    D32(block, 0xa4) = (uint32_t)(uintptr_t)fs98_flow_browser_settings_callback;
    D32(block, 0xac) = (uint32_t)(uintptr_t)s_flow_rows[side][0][0];
    D32(block, 0xb0) = 2;
    D32(block, 0xb8) = 0;
}

static void fs98_flow_reset_host_focus(void)
{
    ShowWindow(s_main_window, SW_SHOW);
    SetForegroundWindow(s_main_window);
    SetActiveWindow(s_main_window);
    SetFocus(s_main_window);
    fs98_pump();
}

static uint32_t fs98_flow_visibility_mask(HWND *windows)
{
    uint32_t page, result = 0;
    for (page = 0; page < 5; ++page)
        if (windows[page] && IsWindow(windows[page]) && IsWindowVisible(windows[page]))
            result |= 1u << page;
    return result;
}

static uint32_t fs98_flow_try_read_u32(void *object, uint32_t offset,
                                       uint32_t *value)
{
    SIZE_T read = 0;
    if (!object || !ReadProcessMemory(GetCurrentProcess(),
            (const uint8_t *)object + offset, value, sizeof(*value), &read) ||
        read != sizeof(*value)) return 0;
    return 1;
}

static void fs98_flow_checkpoint(HWND *windows, struct fs98_flow_result *result)
{
    if (result->checkpoint_count < sizeof(result->checkpoints) / sizeof(result->checkpoints[0]))
        result->checkpoints[result->checkpoint_count++] = fs98_flow_visibility_mask(windows);
}

static void fs98_flow_stage(uint32_t side, uint32_t case_id, const char *stage,
                            HWND *windows)
{
    uint32_t index;
    if (case_id != 200 && case_id != 203 && case_id != 204 && case_id != 210) return;
    fprintf(stderr, "FLOW STAGE side=%lu case=%lu name=%s active=%p last_popup=%p "
        "focus=%p focusid=%lu depth=%lu visible=%08lx parent_enabled=%lu\n",
        (unsigned long)side, (unsigned long)case_id, stage, GetActiveWindow(),
        GetLastActivePopup(s_main_window), GetFocus(),
        (unsigned long)(GetFocus() ? GetDlgCtrlID(GetFocus()) : 0),
        (unsigned long)s_flow_dispatch_depth[side],
        (unsigned long)fs98_flow_visibility_mask(windows),
        (unsigned long)IsWindowEnabled(s_main_window));
    for (index = 0; index < 5; ++index) {
        void *object = s_flow_dialog_objects[side][index];
        uint32_t parent_field = 0, activate_a = 0, activate_b = 0;
        uint32_t pending_message = 0, pending_hwnd = 0;
        uint32_t readable = fs98_flow_try_read_u32(object, 0x7c, &parent_field) &
            fs98_flow_try_read_u32(object, 0x7dc, &activate_a) &
            fs98_flow_try_read_u32(object, 0xff0, &activate_b) &
            fs98_flow_try_read_u32(object, 0x7d0, &pending_message) &
            fs98_flow_try_read_u32(object, 0x7d4, &pending_hwnd);
        fprintf(stderr, " FLOW PAGE side=%lu case=%lu index=%lu hwnd=%p style=%08lx exstyle=%08lx owner=%p prev=%p next=%p visible=%lu enabled=%lu object=%p parent_field=%08lx act_flags=%08lx/%08lx pending=%08lx/%08lx\n",
            (unsigned long)side, (unsigned long)case_id, (unsigned long)index,
            windows[index], (unsigned long)(uint32_t)GetWindowLongA(windows[index], GWL_STYLE),
            (unsigned long)(uint32_t)GetWindowLongA(windows[index], GWL_EXSTYLE),
            GetWindow(windows[index], GW_OWNER), GetWindow(windows[index], GW_HWNDPREV),
            GetWindow(windows[index], GW_HWNDNEXT),
            (unsigned long)IsWindowVisible(windows[index]),
            (unsigned long)IsWindowEnabled(windows[index]), object,
            (unsigned long)(readable ? parent_field : 0),
            (unsigned long)(readable ? activate_a : 0),
            (unsigned long)(readable ? activate_b : 0),
            (unsigned long)(readable ? pending_message : 0),
            (unsigned long)(readable ? pending_hwnd : 0));
        if (!readable)
            fprintf(stderr, "  FLOW OBJECT READ side=%lu page=%lu object=%p: inaccessible state fields\n",
                (unsigned long)side, (unsigned long)index, object);
    }
}

static void fs98_flow_send(HWND *windows, uint32_t page, uint32_t control_id,
                           struct fs98_flow_result *result)
{
    HWND control = GetDlgItem(windows[page], (int)control_id);
    if (!control) fs98_fail("0x110a action control missing");
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW ACTION BEGIN side=%lu case=%lu page=%lu command=%04lx depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, (unsigned long)page,
            (unsigned long)control_id, (unsigned long)s_flow_dispatch_depth[s_side]);
    fs98_flow_trace_event(s_side, 4, (int32_t)page, WM_COMMAND,
        MAKEWPARAM(control_id, BN_CLICKED), (uint32_t)(uintptr_t)control, 0, 0);
    fs98_send_command(windows[page], control_id);
    if (s_case == 203 || s_case == 204 || s_case == 210)
        fprintf(stderr, "FLOW ACTION END side=%lu case=%lu page=%lu command=%04lx depth=%lu\n",
            (unsigned long)s_side, (unsigned long)s_case, (unsigned long)page,
            (unsigned long)control_id, (unsigned long)s_flow_dispatch_depth[s_side]);
    fs98_flow_checkpoint(windows, result);
}

static void fs98_flow_choose(HWND *windows, uint32_t route_id,
                             struct fs98_flow_result *result)
{
    HWND radio = GetDlgItem(windows[1], (int)route_id);
    if (!radio) fs98_fail("0x110a route radio missing");
    SendMessageA(radio, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(2, 2));
    SendMessageA(radio, WM_LBUTTONUP, 0, MAKELPARAM(2, 2));
    fs98_flow_checkpoint(windows, result);
}

static void fs98_flow_dispatch_validation_error(HWND window)
{
    MSG message;
    while (PeekMessageA(&message, window, 0x5f4, 0x5f4, PM_REMOVE))
        DispatchMessageA(&message);
}

static void fs98_flow_focus_loss(HWND *windows, uint32_t page, uint32_t control_id,
                                 const char *value, struct fs98_flow_result *result,
                                 uint32_t slot, uint32_t dispatch_error)
{
    HWND edit = GetDlgItem(windows[page], (int)control_id);
    if (!edit) fs98_fail("0x110a validation edit missing");
    SetWindowTextA(edit, value);
    SendMessageA(windows[page], WM_COMMAND, MAKEWPARAM(control_id, 0x0200), (LPARAM)edit);
    /* CSimsDialog posts 0x5f4 after a failed focus-loss validation. HOST's
     * message pump is modeled here only when the scenario asks to dispatch it. */
    if (dispatch_error) fs98_flow_dispatch_validation_error(windows[page]);
    result->validation_focus[slot] = (uint32_t)GetDlgCtrlID(GetFocus());
    fs98_flow_checkpoint(windows, result);
}

struct fs98_flow_child_capture {
    HWND parent;
    struct fs98_flow_result *result;
    uint32_t page;
};

static BOOL CALLBACK fs98_flow_capture_child(HWND child, LPARAM context_value)
{
    struct fs98_flow_child_capture *context =
        (struct fs98_flow_child_capture *)context_value;
    struct fs98_flow_child_control *snapshot;
    RECT rect;
    uint32_t index = context->result->child_control_count[context->page];
    if (index >= FS98_FLOW_MAX_CHILD_CONTROLS) {
        context->result->child_control_overflow[context->page] = 1;
        return FALSE;
    }
    snapshot = &context->result->child_controls[context->page][index];
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->id = (uint32_t)GetDlgCtrlID(child);
    snapshot->style = (uint32_t)GetWindowLongA(child, GWL_STYLE);
    snapshot->exstyle = (uint32_t)GetWindowLongA(child, GWL_EXSTYLE);
    snapshot->enabled = (uint32_t)IsWindowEnabled(child);
    snapshot->style_visible = (snapshot->style & WS_VISIBLE) != 0;
    snapshot->effective_visible = (uint32_t)IsWindowVisible(child);
    if (!GetWindowRect(child, &rect)) fs98_fail("capture flow child rectangle");
    MapWindowPoints(0, context->parent, (POINT *)&rect, 2);
    snapshot->x = rect.left;
    snapshot->y = rect.top;
    snapshot->width = rect.right - rect.left;
    snapshot->height = rect.bottom - rect.top;
    GetClassNameA(child, snapshot->class_name, sizeof(snapshot->class_name));
    GetWindowTextA(child, snapshot->text, sizeof(snapshot->text));
    context->result->child_control_count[context->page] = index + 1;
    return TRUE;
}

static void fs98_flow_capture_children(HWND *windows,
                                       struct fs98_flow_result *result)
{
    uint32_t page;
    for (page = 0; page < 5; ++page) {
        struct fs98_flow_child_capture context;
        context.parent = windows[page];
        context.result = result;
        context.page = page;
        if (!EnumChildWindows(windows[page], fs98_flow_capture_child,
                              (LPARAM)&context) ||
            result->child_control_overflow[page])
            fs98_fail("enumerate complete flow child-control tree");
    }
}

struct fs98_native_child_context {
    HWND parent;
    uint32_t *count;
    uint32_t *overflow;
    struct fs98_flow_child_control *children;
};

static BOOL CALLBACK fs98_capture_native_child(HWND child, LPARAM context_value)
{
    struct fs98_native_child_context *context =
        (struct fs98_native_child_context *)context_value;
    struct fs98_flow_child_control *snapshot;
    RECT rect;
    uint32_t index = *context->count;
    if (index >= FS98_FLOW_MAX_CHILD_CONTROLS) {
        *context->overflow = 1;
        return FALSE;
    }
    snapshot = &context->children[index];
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->id = (uint32_t)GetDlgCtrlID(child);
    snapshot->style = (uint32_t)GetWindowLongA(child, GWL_STYLE);
    snapshot->exstyle = (uint32_t)GetWindowLongA(child, GWL_EXSTYLE);
    snapshot->enabled = (uint32_t)IsWindowEnabled(child);
    snapshot->style_visible = (snapshot->style & WS_VISIBLE) != 0;
    snapshot->effective_visible = (uint32_t)IsWindowVisible(child);
    if (!GetWindowRect(child, &rect)) fs98_fail("capture native child rectangle");
    MapWindowPoints(0, context->parent, (POINT *)&rect, 2);
    snapshot->x = rect.left;
    snapshot->y = rect.top;
    snapshot->width = rect.right - rect.left;
    snapshot->height = rect.bottom - rect.top;
    GetClassNameA(child, snapshot->class_name, sizeof(snapshot->class_name));
    GetWindowTextA(child, snapshot->text, sizeof(snapshot->text));
    *context->count = index + 1;
    return TRUE;
}

static void fs98_capture_native_child_tree(HWND window, uint32_t *count,
    uint32_t *overflow, struct fs98_flow_child_control *children)
{
    struct fs98_native_child_context context;
    *count = 0;
    *overflow = 0;
    memset(children, 0, sizeof(*children) * FS98_FLOW_MAX_CHILD_CONTROLS);
    context.parent = window;
    context.count = count;
    context.overflow = overflow;
    context.children = children;
    if (!EnumChildWindows(window, fs98_capture_native_child, (LPARAM)&context) ||
        *overflow)
        fs98_fail("enumerate complete native dialog child tree");
}

struct fs98_flow_render_context {
    uint32_t hash, painted_bytes, window_count;
    struct fs98_flow_render_capture *capture;
};

static void fs98_flow_hash_rendered_window(HWND window,
                                           struct fs98_flow_render_context *context)
{
    RECT rect;
    BITMAPINFO info;
    HDC source, memory;
    HBITMAP bitmap, previous;
    uint8_t *pixels;
    uint32_t byte_count, index, local_hash = 2166136261u, local_painted = 0;
    int32_t control_id;
    pixels = 0;
    if (!GetWindowRect(window, &rect) || rect.right <= rect.left ||
        rect.bottom <= rect.top || rect.right - rect.left > 2048 ||
        rect.bottom - rect.top > 2048)
        fs98_fail("invalid 0x110a control render dimensions");
    rect.right -= rect.left;
    rect.bottom -= rect.top;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = rect.right;
    info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    source = GetWindowDC(window);
    memory = source ? CreateCompatibleDC(source) : 0;
    bitmap = source ? CreateDIBSection(source, &info, DIB_RGB_COLORS,
                                       (void **)&pixels, 0, 0) : 0;
    if (!source || !memory || !bitmap || !pixels)
        fs98_fail("allocate 0x110a page render surface");
    previous = SelectObject(memory, bitmap);
    if (!previous || previous == HGDI_ERROR)
        fs98_fail("select 0x110a control render surface");
    byte_count = (uint32_t)rect.right * (uint32_t)rect.bottom * 4;
    memset(pixels, 0xa5, byte_count);
    SendMessageA(window, WM_PRINT, (WPARAM)memory,
                 PRF_NONCLIENT | PRF_CLIENT | PRF_ERASEBKGND);
    GdiFlush();
    for (index = 0; index < byte_count; index += 4) {
        uint32_t channel;
        for (channel = 0; channel < 3; ++channel) {
            local_hash = (local_hash ^ pixels[index + channel]) * 16777619u;
            if (pixels[index + channel] != 0xa5) ++local_painted;
        }
    }
    control_id = GetDlgCtrlID(window);
    if (context->capture->control_count >= FS98_FLOW_MAX_RENDERED_CONTROLS)
        fs98_fail("overflow 0x110a rendered-control capture");
    {
        HFONT font;
        struct fs98_flow_control_render *render =
            &context->capture->controls[context->capture->control_count++];
        memset(render, 0, sizeof(*render));
        render->id = (uint32_t)control_id;
        render->order = context->capture->control_count - 1;
        render->width = (uint32_t)rect.right;
        render->height = (uint32_t)rect.bottom;
        render->hash = local_hash;
        render->painted_bytes = local_painted;
        render->style = (uint32_t)GetWindowLongA(window, GWL_STYLE);
        render->exstyle = (uint32_t)GetWindowLongA(window, GWL_EXSTYLE);
        render->enabled = (uint32_t)IsWindowEnabled(window);
        render->style_visible = (render->style & WS_VISIBLE) != 0;
        render->effective_visible = (uint32_t)IsWindowVisible(window);
        render->focused = (uint32_t)(GetFocus() == window);
        render->check_state = -1;
        render->selection = -1;
        render->list_count = render->list_top = render->list_caret = -1;
        render->list_anchor = render->list_selection_count = -1;
        render->list_items_hash = 2166136261u;
        GetClassNameA(window, render->class_name, sizeof(render->class_name));
        GetWindowTextA(window, render->text, sizeof(render->text));
        if (lstrcmpiA(render->class_name, "Button") == 0) {
            render->check_state = (int32_t)SendMessageA(window, BM_GETCHECK, 0, 0);
            render->button_state = (uint32_t)SendMessageA(window, BM_GETSTATE, 0, 0);
        } else if (lstrcmpiA(render->class_name, "ListBox") == 0) {
            int32_t item_index;
            render->selection = (int32_t)SendMessageA(window, LB_GETCURSEL, 0, 0);
            render->list_count = (int32_t)SendMessageA(window, LB_GETCOUNT, 0, 0);
            render->list_top = (int32_t)SendMessageA(window, LB_GETTOPINDEX, 0, 0);
            render->list_caret = (int32_t)SendMessageA(window, LB_GETCARETINDEX, 0, 0);
            render->list_anchor = (int32_t)SendMessageA(window, LB_GETANCHORINDEX, 0, 0);
            render->list_selection_count = (int32_t)SendMessageA(window, LB_GETSELCOUNT, 0, 0);
            for (item_index = 0; item_index < render->list_count && item_index < 256;
                 ++item_index) {
                char item_text[256];
                int32_t char_index, text_length;
                memset(item_text, 0, sizeof(item_text));
                text_length = (int32_t)SendMessageA(window, LB_GETTEXT, item_index,
                                                    (LPARAM)item_text);
                if (item_index < 4)
                    lstrcpynA(render->list_items[item_index], item_text,
                              sizeof(render->list_items[item_index]));
                render->list_items_hash = (render->list_items_hash ^ (uint32_t)text_length) * 16777619u;
                if (text_length > 0 && text_length < (int32_t)sizeof(item_text))
                    for (char_index = 0; char_index < text_length; ++char_index)
                        render->list_items_hash =
                            (render->list_items_hash ^ (uint8_t)item_text[char_index]) * 16777619u;
            }
        } else if (lstrcmpiA(render->class_name, "ComboBox") == 0) {
            render->selection = (int32_t)SendMessageA(window, CB_GETCURSEL, 0, 0);
        }
        font = (HFONT)SendMessageA(window, WM_GETFONT, 0, 0);
        if (font && GetObjectA(font, sizeof(render->font), &render->font) == sizeof(render->font))
            render->font_valid = 1;
    }
    context->hash = (context->hash ^ (uint32_t)control_id) * 16777619u;
    context->hash = (context->hash ^ (uint32_t)rect.right) * 16777619u;
    context->hash = (context->hash ^ (uint32_t)rect.bottom) * 16777619u;
    context->hash = (context->hash ^ local_hash) * 16777619u;
    context->painted_bytes += local_painted;
    ++context->window_count;
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(window, source);
}

static BOOL CALLBACK fs98_flow_hash_render_child(HWND child, LPARAM context_value)
{
    if (!IsWindowVisible(child)) return TRUE;
    fs98_flow_hash_rendered_window(child,
        (struct fs98_flow_render_context *)context_value);
    return TRUE;
}

static void fs98_flow_capture_render(HWND window,
                                    struct fs98_flow_render_capture *capture)
{
    RECT rect;
    GUITHREADINFO gui;
    HWND caret_window = 0;
    BOOL caret_hidden = FALSE;
    struct fs98_flow_render_context context;
    memset(&context, 0, sizeof(context));
    context.hash = 2166136261u;
    context.capture = capture;
    if (!GetClientRect(window, &rect) || rect.right <= 0 || rect.bottom <= 0 ||
        rect.right > 2048 || rect.bottom > 2048)
        fs98_fail("invalid 0x110a page render dimensions");
    memset(&gui, 0, sizeof(gui));
    gui.cbSize = sizeof(gui);
    if (GetGUIThreadInfo(GetCurrentThreadId(), &gui) && gui.hwndCaret) {
        caret_window = gui.hwndCaret;
        caret_hidden = HideCaret(caret_window);
    }
    fs98_flow_hash_rendered_window(window, &context);
    if (!EnumChildWindows(window, fs98_flow_hash_render_child,
                          (LPARAM)&context))
        fs98_fail("enumerate rendered 0x110a controls");
    if (caret_hidden) ShowCaret(caret_window);
    if (!context.painted_bytes)
        fs98_fail("0x110a page render painted no control pixels");
    capture->width = (uint32_t)rect.right;
    capture->height = (uint32_t)rect.bottom;
    capture->hash = context.hash;
    capture->painted_bytes = context.painted_bytes;
    capture->control_count = context.window_count;
}

static void fs98_render_print_control(const char *kind, const char *name,
    uint32_t index, const struct fs98_flow_control_render *control)
{
    fprintf(stderr,
        "MP_DIALOG RENDER CONTROL %s=%s index=%lu order=%lu id=%ld class=%s text='%s' size=%lux%lu hash=%08lx painted=%lu style=%08lx exstyle=%08lx enabled=%u visible(style/effective)=%u/%u focus=%u check=%ld button-state=%08lx selection=%ld list(count/top/caret/anchor/multisel/hash)=%ld/%ld/%ld/%ld/%ld/%08lx items='%s'|'%s'|'%s'|'%s' font=%s/%ld/%ld/%u/%u/%u\n",
        kind, name, (unsigned long)index, (unsigned long)control->order,
        (long)(int32_t)control->id, control->class_name, control->text,
        (unsigned long)control->width, (unsigned long)control->height,
        (unsigned long)control->hash, (unsigned long)control->painted_bytes,
        (unsigned long)control->style, (unsigned long)control->exstyle,
        (unsigned)control->enabled, (unsigned)control->style_visible,
        (unsigned)control->effective_visible, (unsigned)control->focused,
        (long)control->check_state, (unsigned long)control->button_state,
        (long)control->selection, (long)control->list_count,
        (long)control->list_top, (long)control->list_caret,
        (long)control->list_anchor, (long)control->list_selection_count,
        (unsigned long)control->list_items_hash,
        control->list_items[0], control->list_items[1],
        control->list_items[2], control->list_items[3],
        control->font_valid ? control->font.lfFaceName : "<none>",
        (long)control->font.lfHeight, (long)control->font.lfWidth,
        (unsigned)control->font.lfWeight, (unsigned)control->font.lfItalic,
        (unsigned)control->font.lfCharSet);
}

static uint32_t fs98_render_compare_repeat(const char *name,
    const struct fs98_flow_render_capture *first,
    const struct fs98_flow_render_capture *second)
{
    uint32_t index, limit, differs =
        memcmp(first, second, sizeof(*first)) != 0;
    ++s_render_repeat_total;
    if (!differs) {
        ++s_render_repeat_matches;
        return 0;
    }
    ++s_render_repeat_differences;
    fprintf(stderr,
        "MP_DIALOG RENDER REPEAT DIFFER name=%s size=%lux%lu/%lux%lu controls=%lu/%lu hash=%08lx/%08lx painted=%lu/%lu\n",
        name, (unsigned long)first->width, (unsigned long)first->height,
        (unsigned long)second->width, (unsigned long)second->height,
        (unsigned long)first->control_count, (unsigned long)second->control_count,
        (unsigned long)first->hash, (unsigned long)second->hash,
        (unsigned long)first->painted_bytes,
        (unsigned long)second->painted_bytes);
    limit = first->control_count < second->control_count ?
        first->control_count : second->control_count;
    for (index = 0; index < limit; ++index) {
        if (memcmp(&first->controls[index], &second->controls[index],
                   sizeof(first->controls[index])) != 0) {
            fs98_render_print_control("repeat-first", name, index,
                                      &first->controls[index]);
            fs98_render_print_control("repeat-second", name, index,
                                      &second->controls[index]);
        }
    }
    return 1;
}

static void fs98_flow_capture_result(uint32_t side, HWND *windows,
                                     struct fs98_flow_result *result)
{
    uint8_t *block = s_flow_blocks[side];
    static const uint32_t s_control_ids[5][8] = {
        {2, 0x3e8, 0x3e9, 0x42b, 0x434, 0x427, 0x421, 0x422},
        {2, 0x3e8, 0x3e9, 1, 0x419, 0x41c, 0x41e, 0x41d},
        {2, 0x3e8, 0x3e9, 0x426, 0x41a, 0x40f, 0x410, 0x3ed},
        {2, 0x3e8, 0x3e9, 0x433, 0x41b, 0x3f6, 0x3f7, 0x3f8},
        {2, 0x3e9, 0x3ff, 0x400, 0x3ec, 0x401, 0x3fd, 0x430}
    };
    uint32_t page, index;
    result->descriptor_result = D32(block, 0);
    result->always = D32(block, 0x1c);
    result->mode = D32(block, 0x24);
    result->provider = D32(block, 0x90);
    result->transient = D32(block, 0x14) != 0;
    result->observer = D32(block, 0x40);
    result->browser_selection = D32(block, 0xb8);
    result->status_callsign = D32(block, 0x28);
    result->status_name = D32(block, 0x48);
    result->status_pilot = D32(block, 0x5c);
    result->status_observer = D32(block, 0x70);
    if (D32(block, 0x2c)) strncpy(result->callsign, (const char *)(uintptr_t)D32(block, 0x2c), sizeof(result->callsign) - 1);
    if (D32(block, 0x4c)) strncpy(result->session, (const char *)(uintptr_t)D32(block, 0x4c), sizeof(result->session) - 1);
    if (D32(block, 0x60)) strncpy(result->pilot, (const char *)(uintptr_t)D32(block, 0x60), sizeof(result->pilot) - 1);
    if (D32(block, 0x74)) strncpy(result->observer_text, (const char *)(uintptr_t)D32(block, 0x74), sizeof(result->observer_text) - 1);
    result->callback_count = s_flow_event_count[side];
    result->callback_overflow = result->callback_overflow || result->callback_count > 24;
    if (result->callback_count > 24) result->callback_count = 24;
    memcpy(result->callbacks, s_flow_events[side], result->callback_count * sizeof(result->callbacks[0]));
    result->idle_ticks = s_flow_idle_ticks[side];
    result->accepted_hidden_mask = result->descriptor_result == 1 ?
        (fs98_flow_visibility_mask(windows) == 0 ? 0x1f : fs98_flow_visibility_mask(windows)) : 0;
    result->hwnd_alive_mask = 0;
    for (page = 0; page < 5; ++page) {
        if (windows[page] && IsWindow(windows[page])) result->hwnd_alive_mask |= 1u << page;
        for (index = 0; index < 8; ++index)
            if (GetDlgItem(windows[page], (int)s_control_ids[page][index]))
                result->controls[page] |= 1u << index;
    }
}

static void fs98_flow_dispatch_initial(HWND *windows, uint32_t side,
                                       struct fs98_flow_result *result)
{
    MSG message;
    while (PeekMessageA(&message, windows[0], WM_COMMAND, WM_COMMAND, PM_REMOVE)) {
        ++result->posts;
        result->post_message = message.message;
        result->post_wparam = (uint32_t)message.wParam;
        result->post_lparam = (uint32_t)message.lParam;
        fs98_flow_trace_event(side, 5, 0, message.message,
            (uint32_t)message.wParam, (uint32_t)message.lParam, 0, 0);
        DispatchMessageA(&message);
    }
    result->after_initial_mask = fs98_flow_visibility_mask(windows);
}

static void fs98_flow_to_choice(HWND *windows, struct fs98_flow_result *result)
{
    fs98_flow_send(windows, 0, 0x3e8, result);
}

static void fs98_flow_to_host(HWND *windows, struct fs98_flow_result *result)
{
    fs98_flow_to_choice(windows, result);
    fs98_flow_choose(windows, 0x41e, result);
    fs98_flow_send(windows, 1, 0x3e8, result);
}

static void fs98_flow_to_connection(HWND *windows, struct fs98_flow_result *result)
{
    fs98_flow_to_choice(windows, result);
    fs98_flow_choose(windows, 0x41c, result);
    fs98_flow_send(windows, 1, 0x3e8, result);
}

static void fs98_flow_provider_to_browser(HWND *windows, uint32_t side,
                                         struct fs98_flow_result *result)
{
    SendMessageA(GetDlgItem(windows[3], 0x41b), LB_SETCURSEL, 1, 0);
    SendMessageA(windows[3], WM_COMMAND, MAKEWPARAM(0x41b, 2),
                 (LPARAM)GetDlgItem(windows[3], 0x41b));
    fs98_flow_checkpoint(windows, result);
    fs98_modeled_host_flow_idle_tick(windows);
    ++s_flow_idle_ticks[side];
    fs98_flow_checkpoint(windows, result);
}

static void fs98_flow_dispatch_browser_timer_probe(HWND browser)
{
    MSG message;
    /* The real 500ms timer is recorded through the DLL's SetTimer import.
     * Post the same WM_TIMER payload and dispatch just that message so the
     * handler comparison is deterministic and cannot drain unrelated HOST UI. */
    if (!PostMessageA(browser, WM_TIMER, 1, 0) ||
        !PeekMessageA(&message, browser, WM_TIMER, WM_TIMER, PM_REMOVE) ||
        message.wParam != 1 || message.lParam != 0)
        fs98_fail("post and retrieve Browser WM_TIMER probe");
    TranslateMessage(&message);
    DispatchMessageA(&message);
}

static void fs98_flow_lifetime(uint32_t side, HWND *windows,
                               struct fs98_flow_result *result)
{
    uint32_t page;
    s_flow_lifecycle_tick = 0;
    for (page = 0; page < 5; ++page) {
        if ((s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210) &&
            page == 0) {
            fprintf(stderr, "FLOW DESTROY side=%lu case=%lu page=%lu hwnd=%p style=%08lx "
                "exstyle=%08lx parent=%p owner=%p active=%p focus=%p focusid=%lu "
                "visible=%lu\n",
                (unsigned long)side, (unsigned long)s_case, (unsigned long)page,
                windows[page], (unsigned long)(uint32_t)GetWindowLongA(windows[page], GWL_STYLE),
                (unsigned long)(uint32_t)GetWindowLongA(windows[page], GWL_EXSTYLE),
                GetParent(windows[page]), GetWindow(windows[page], GW_OWNER),
                GetActiveWindow(), GetFocus(),
                (unsigned long)(GetFocus() ? GetDlgCtrlID(GetFocus()) : 0),
                (unsigned long)IsWindowVisible(windows[page]));
        }
        fs98_flow_trace_event(side, 6, (int32_t)page, WM_DESTROY,
            (uint32_t)GetWindowLongA(windows[page], GWL_STYLE),
            (uint32_t)(uintptr_t)windows[page], 0, 0);
        if (IsWindow(windows[page]) && !DestroyWindow(windows[page]))
            fs98_fail("HOST-modeled 0x110a array teardown");
        if (page == 4 && (s_case == 203 || s_case == 204 || s_case == 210))
            fprintf(stderr, "FLOW LIFE AFTER side=%lu case=%lu page=%lu nc=%u post=%u delete=%u depth=%lu\n",
                (unsigned long)side, (unsigned long)s_case, (unsigned long)page,
                result->nc_destroy[page], result->post_destroy[page],
                result->deleting_dtor[page], (unsigned long)s_flow_dispatch_depth[side]);
        if ((s_case == 200 || s_case == 203 || s_case == 204 || s_case == 210) &&
            page == 0) {
            MSG pending;
            BOOL has_pending = PeekMessageA(&pending, 0, 0x5f4, 0x5f4, PM_NOREMOVE);
            fprintf(stderr, "FLOW DESTROY END side=%lu case=%lu page=%lu active=%p focus=%p "
                "pending5f4=%lu hwnd=%p wp=%08lx lp=%08lx depth=%lu\n",
                (unsigned long)side, (unsigned long)s_case, (unsigned long)page,
                GetActiveWindow(), GetFocus(), (unsigned long)has_pending,
                has_pending ? pending.hwnd : 0,
                (unsigned long)(has_pending ? (uint32_t)pending.wParam : 0),
                (unsigned long)(has_pending ? (uint32_t)pending.lParam : 0),
                (unsigned long)s_flow_dispatch_depth[side]);
        }
    }
    result->owner_cleanup_order = ++s_flow_lifecycle_tick;
    s_flow_owner_active[side] = 0;
    result->lifetime_ok = 1;
    for (page = 0; page < 5; ++page) {
        if (IsWindow(windows[page])) {
            result->hwnd_alive_after_mask |= 1u << page;
            result->lifetime_ok = 0;
        }
        if (result->nc_destroy[page] != 1 || result->post_destroy[page] != 1 ||
            result->deleting_dtor[page] != 1 || result->delete_flags[page] != 1 ||
            !(result->nc_order[page] < result->post_order[page] &&
              result->post_order[page] < result->delete_order[page] &&
              result->delete_order[page] < result->owner_cleanup_order)) result->lifetime_ok = 0;
    }
    fs98_restore_flow_pages();
    if (s_cleanup_flow_lifetime_leaks) {
        for (page = 0; page < 5; ++page) {
            void *object = s_flow_dialog_objects[side][page];
            void *delete_target = s_flow_delete_originals[side][page];
            if (object && result->deleting_dtor[page] == 0 && delete_target) {
                fprintf(stderr, "FLOW HARNESS CLEANUP side=%lu case=%lu page=%lu "
                    "reason=mutation-omitted-deleting-destructor\n",
                    (unsigned long)side, (unsigned long)s_case,
                    (unsigned long)page);
                ((void *(TC *)(void *, uint32_t))delete_target)(object, 1);
                s_flow_dialog_objects[side][page] = 0;
            }
        }
    }
    s_flow_windows[side] = 0;
}

static uint32_t fs98_run_flow_case(uint32_t cycle, uint32_t case_index,
                                   HMODULE *modules, struct fs98_test_linkage **linkages)
{
    uint32_t side, difference = 0;
    for (side = 0; side < 2; ++side) {
        struct fs98_flow_result *result = &s_flow_results[side];
        uint8_t *block = s_flow_blocks[side];
        HWND *windows;
        uint32_t page, pending;
        HWND isolated_host;
        s_side = side;
        s_case = 200 + cycle * 12 + case_index;
        s_current_module = modules[side];
        s_flow_active = 0;
        s_flow_modal_seen[side] = 0;
        fs98_flow_setup(side, case_index);
        isolated_host = case_index == 0 ? fs98_flow_create_isolated_host() : 0;
        fs98_patch_flow_main();
        fs98_flow_reset_host_focus();
        if (GetActiveWindow() != s_main_window || GetFocus() != s_main_window) {
            fprintf(stderr, "FLOW FOCUS RESET side=%lu case=%lu active=%p focus=%p main=%p\n",
                (unsigned long)side, (unsigned long)s_case, GetActiveWindow(),
                GetFocus(), s_main_window);
            fs98_fail("cannot establish common HOST focus state");
        }
        s_flow_active = 1;
        s_flow_callwnd_hook = SetWindowsHookExA(WH_CALLWNDPROC,
            fs98_flow_callwnd_hook, 0, GetCurrentThreadId());
        s_flow_callwndret_hook = SetWindowsHookExA(WH_CALLWNDPROCRET,
            fs98_flow_callwndret_hook, 0, GetCurrentThreadId());
        if (!s_flow_callwnd_hook || !s_flow_callwndret_hook)
            fs98_fail("install flow call-window probes");
        s_flow_modal_hook = SetWindowsHookExA(WH_CBT, fs98_flow_modal_hook_proc, 0,
                                               GetCurrentThreadId());
        if (!s_flow_modal_hook) fs98_fail("install 0x110a modal diagnostic hook");
        windows = (HWND *)(uintptr_t)linkages[side]->dialogs[0].create(
            (uint32_t)(uintptr_t)s_main_window, block);
        if (!UnhookWindowsHookEx(s_flow_modal_hook)) fs98_fail("remove 0x110a modal diagnostic hook");
        s_flow_modal_hook = 0;
        if (s_flow_modal_seen[side]) fs98_fail("retail/replacement displayed resource or error modal during page construction");
        s_flow_windows[side] = windows;
        if (!windows || windows[5] || !windows[0] || !windows[1] || !windows[2] || !windows[3] || !windows[4])
            fs98_fail("0x110a five-HWND factory array");
        if (case_index == 0 || case_index == 3 || case_index == 4 || case_index == 10)
            fs98_flow_stage(side, s_case, "after_factory_before_host_show", windows);
        result->array_valid = 1;
        for (page = 0; page < 5; ++page)
            if (!IsWindow(windows[page])) result->array_valid = 0;
        fs98_flow_capture_children(windows, result);
        result->before_host_mask = fs98_flow_visibility_mask(windows);
        for (page = 0; page < 5; ++page) fs98_patch_flow_page(windows[page], page, modules[side]);
        if (case_index == 3 || case_index == 4 || case_index == 10)
            fs98_flow_patch_timer_imports(side, modules[side]);
        if (s_case == 200) fs98_flow_patch_showwindow_import(side, modules[side]);
        /* Keep paired message probes active through dispatch, transitions, and teardown. */
        s_flow_modal_hook = SetWindowsHookExA(WH_CBT, fs98_flow_modal_hook_proc, 0,
                                               GetCurrentThreadId());
        if (!s_flow_modal_hook) fs98_fail("install flow activation probe");
        ShowWindow(windows[0], SW_SHOW);
        result->after_host_show_mask = fs98_flow_visibility_mask(windows);
        if (case_index == 0 || case_index == 3 || case_index == 4 || case_index == 10)
            fs98_flow_stage(side, s_case, "after_host_show_before_dispatch", windows);
        fs98_flow_dispatch_initial(windows, side, result);
        fs98_flow_stage(side, s_case, "after_initial_dispatch", windows);
        fs98_flow_checkpoint(windows, result);
        if ((uint32_t)SendDlgItemMessageA(windows[3], 0x41b, LB_GETCOUNT, 0, 0) !=
                D32(s_flow_blocks[side], 0x88))
            fs98_fail("provider list must contain every descriptor entry");
        if (!!IsWindowEnabled(GetDlgItem(windows[4], 0x430)) !=
                !!D32(s_flow_blocks[side], 0xa8))
            fs98_fail("observer availability must follow descriptor +0xa8");

        switch (case_index) {
        case 0: /* factory-posted initial Next, HOST shows Intro, then dispatches */
            break;
        case 1: /* descriptor suppresses initial post; Intro back and cancel */
            fs98_flow_to_choice(windows, result);
            fs98_flow_send(windows, 1, 0x3e9, result);
            fs98_flow_send(windows, 0, IDCANCEL, result);
            break;
        case 2: /* host route, settings callback, back/forward, accept on Connection */
            fs98_flow_to_host(windows, result);
            fs98_flow_send(windows, 2, 0x426, result);
            fs98_flow_send(windows, 2, 0x3e9, result);
            fs98_flow_choose(windows, 0x41e, result);
            fs98_flow_send(windows, 1, 0x3e8, result);
            fs98_flow_send(windows, 2, 0x3e8, result);
            fs98_flow_send(windows, 3, 0x433, result);
            break;
        case 3: /* provider selection, modeled HOST idle, browser callbacks, Join */
            fs98_flow_to_connection(windows, result);
            fs98_flow_provider_to_browser(windows, side, result);
            if (!IsWindowVisible(windows[4])) fs98_fail("Browser page not visible for timer probe");
            fs98_flow_dispatch_browser_timer_probe(windows[4]);
            fs98_flow_checkpoint(windows, result);
            SendMessageA(GetDlgItem(windows[4], 0x3fd), LB_SETCURSEL, 0, 0);
            SendMessageA(windows[4], WM_COMMAND, MAKEWPARAM(0x3fd, 1),
                         (LPARAM)GetDlgItem(windows[4], 0x3fd));
            fs98_flow_checkpoint(windows, result);
            fs98_flow_send(windows, 4, 0x400, result);
            fs98_flow_send(windows, 4, 0x3ec, result);
            fs98_flow_send(windows, 4, 0x401, result);
            fs98_flow_send(windows, 4, 0x430, result);
            fs98_flow_send(windows, 4, 0x3ff, result);
            break;
        case 4: /* failed provider path invokes reset; retry succeeds; cancel Browser */
            fs98_flow_to_connection(windows, result);
            SendMessageA(GetDlgItem(windows[3], 0x41b), LB_SETCURSEL, 1, 0);
            SendMessageA(windows[3], WM_COMMAND, MAKEWPARAM(0x41b, 2),
                         (LPARAM)GetDlgItem(windows[3], 0x41b));
            fs98_flow_send(windows, 3, 0x3e8, result);
            if (IsWindowVisible(windows[4])) {
                fs98_modeled_host_flow_idle_tick(windows);
                ++s_flow_idle_ticks[side];
                fs98_flow_checkpoint(windows, result);
                fs98_flow_send(windows, 4, IDCANCEL, result);
            }
            break;
        case 5: /* online mode accept callback rejects once, then accepts */
            fs98_flow_to_choice(windows, result);
            fs98_flow_choose(windows, 0x41d, result);
            fs98_flow_send(windows, 1, 0x3e8, result);
            fs98_flow_send(windows, 1, 0x3e8, result);
            break;
        case 6: /* callsign failure sets pending error, suppresses Next, then recovers */
            fs98_flow_to_choice(windows, result);
            fs98_flow_focus_loss(windows, 1, 0x419, "", result, 0, 0);
            pending = fs98_flow_visibility_mask(windows);
            fs98_flow_send(windows, 1, 0x3e8, result);
            result->blocked_next[0] = pending == fs98_flow_visibility_mask(windows);
            fs98_flow_checkpoint(windows, result);
            fs98_flow_dispatch_validation_error(windows[1]);
            fs98_flow_focus_loss(windows, 1, 0x419, "OpenFS", result, 1, 1);
            fs98_flow_send(windows, 1, 0x3e8, result);
            fs98_flow_send(windows, 3, IDCANCEL, result);
            break;
        case 7: /* host name failure and recovery */
            fs98_flow_to_host(windows, result);
            fs98_flow_focus_loss(windows, 2, 0x41a, "", result, 0, 0);
            pending = fs98_flow_visibility_mask(windows);
            fs98_flow_send(windows, 2, 0x3e8, result);
            result->blocked_next[0] = pending == fs98_flow_visibility_mask(windows);
            fs98_flow_checkpoint(windows, result);
            fs98_flow_dispatch_validation_error(windows[2]);
            fs98_flow_focus_loss(windows, 2, 0x41a, "Sierra Session", result, 1, 1);
            fs98_flow_send(windows, 2, 0x3e8, result);
            fs98_flow_send(windows, 3, IDCANCEL, result);
            break;
        case 8: /* pilot-capacity failure and recovery */
            fs98_flow_to_host(windows, result);
            fs98_flow_focus_loss(windows, 2, 0x40f, "1", result, 0, 0);
            pending = fs98_flow_visibility_mask(windows);
            fs98_flow_send(windows, 2, 0x3e8, result);
            result->blocked_next[0] = pending == fs98_flow_visibility_mask(windows);
            fs98_flow_checkpoint(windows, result);
            fs98_flow_dispatch_validation_error(windows[2]);
            fs98_flow_focus_loss(windows, 2, 0x40f, "5", result, 1, 1);
            fs98_flow_send(windows, 2, 0x3e8, result);
            fs98_flow_send(windows, 3, IDCANCEL, result);
            break;
        case 9: /* observer-capacity failure and recovery */
            fs98_flow_to_host(windows, result);
            fs98_flow_focus_loss(windows, 2, 0x410, "9", result, 0, 0);
            pending = fs98_flow_visibility_mask(windows);
            fs98_flow_send(windows, 2, 0x3e8, result);
            result->blocked_next[0] = pending == fs98_flow_visibility_mask(windows);
            fs98_flow_checkpoint(windows, result);
            fs98_flow_dispatch_validation_error(windows[2]);
            fs98_flow_focus_loss(windows, 2, 0x410, "3", result, 1, 1);
            fs98_flow_send(windows, 2, 0x3e8, result);
            fs98_flow_send(windows, 3, IDCANCEL, result);
            break;
        case 10: /* Browser -> Connection -> Choice -> Intro back chain; cancel Intro */
            fs98_flow_to_connection(windows, result);
            fs98_flow_provider_to_browser(windows, side, result);
            fs98_flow_send(windows, 4, 0x3e9, result);
            fs98_flow_send(windows, 3, 0x3e9, result);
            fs98_flow_send(windows, 1, 0x3e9, result);
            fs98_flow_send(windows, 0, IDCANCEL, result);
            break;
        default: /* host page Cancel path */
            fs98_flow_to_host(windows, result);
            fs98_flow_send(windows, 2, IDCANCEL, result);
            break;
        }
        /* Page input is sent synchronously. HOST's recovered idle dispatch is
         * modeled explicitly rather than draining unrelated WM_TIMER traffic. */
        fs98_flow_stage(side, s_case, "before_capture", windows);
        fs98_flow_capture_result(side, windows, result);
        fs98_flow_stage(side, s_case, "after_capture", windows);
        if (result->descriptor_result == 1)
            result->accepted_hidden_mask = (fs98_flow_visibility_mask(windows) == 0) ? 0x1f : 0;
        for (page = 0; page < 5; ++page)
            if (windows[page] && IsWindow(windows[page])) result->hwnd_alive_mask |= 1u << page;
        fs98_flow_stage(side, s_case, "before_lifetime", windows);
        fs98_flow_lifetime(side, windows, result);
        if (s_flow_settimer_slot[side] || s_flow_killtimer_slot[side])
            fs98_flow_restore_timer_imports(side);
        if (s_case == 200) fs98_flow_restore_showwindow_import(side, modules[side]);
        if (!UnhookWindowsHookEx(s_flow_modal_hook)) fs98_fail("remove flow activation probe");
        s_flow_modal_hook = 0;
        if (!UnhookWindowsHookEx(s_flow_callwnd_hook) ||
            !UnhookWindowsHookEx(s_flow_callwndret_hook))
            fs98_fail("remove flow call-window probes");
        s_flow_callwnd_hook = s_flow_callwndret_hook = 0;
        s_flow_active = 0;
        if (isolated_host) fs98_flow_destroy_isolated_host(isolated_host);
    }
    {
        const struct fs98_flow_result *retail = &s_flow_results[0];
        const struct fs98_flow_result *replacement = &s_flow_results[1];
        uint32_t timing_diff = retail->posts != replacement->posts ||
            retail->post_message != replacement->post_message ||
            retail->post_wparam != replacement->post_wparam || retail->post_lparam != replacement->post_lparam ||
            retail->before_host_mask != replacement->before_host_mask ||
            retail->after_host_show_mask != replacement->after_host_show_mask ||
            retail->after_initial_mask != replacement->after_initial_mask;
        uint32_t initial_diff = retail->array_valid != replacement->array_valid ||
            memcmp(retail->controls, replacement->controls, sizeof(retail->controls)) ||
            retail->before_host_mask != replacement->before_host_mask;
        uint32_t child_inventory_diff =
            memcmp(retail->child_control_count, replacement->child_control_count,
                   sizeof(retail->child_control_count)) ||
            memcmp(retail->child_control_overflow, replacement->child_control_overflow,
                   sizeof(retail->child_control_overflow)) ||
            memcmp(retail->child_controls, replacement->child_controls,
                   sizeof(retail->child_controls));
        uint32_t transition_diff = retail->checkpoint_count != replacement->checkpoint_count ||
            memcmp(retail->checkpoints, replacement->checkpoints, sizeof(retail->checkpoints)) ||
            retail->mode != replacement->mode || retail->always != replacement->always ||
            retail->provider != replacement->provider || retail->transient != replacement->transient ||
            retail->observer != replacement->observer || retail->browser_selection != replacement->browser_selection ||
            retail->descriptor_result != replacement->descriptor_result ||
            strcmp(retail->callsign, replacement->callsign) || strcmp(retail->session, replacement->session) ||
            strcmp(retail->pilot, replacement->pilot) || strcmp(retail->observer_text, replacement->observer_text) ||
            retail->accepted_hidden_mask != replacement->accepted_hidden_mask;
        uint32_t validation_diff = retail->validation_count != replacement->validation_count ||
            memcmp(retail->validation_focus, replacement->validation_focus, sizeof(retail->validation_focus)) ||
            memcmp(retail->blocked_next, replacement->blocked_next, sizeof(retail->blocked_next)) ||
            retail->status_callsign != replacement->status_callsign || retail->status_name != replacement->status_name ||
            retail->status_pilot != replacement->status_pilot || retail->status_observer != replacement->status_observer ||
            memcmp(retail->error_count, replacement->error_count, sizeof(retail->error_count)) ||
            memcmp(retail->error_field, replacement->error_field, sizeof(retail->error_field));
        uint32_t callback_diff = retail->callback_count != replacement->callback_count ||
            retail->callback_overflow != replacement->callback_overflow ||
            memcmp(retail->callbacks, replacement->callbacks, sizeof(retail->callbacks));
        uint32_t browser_dispatch_diff =
            retail->browser_timer_count != replacement->browser_timer_count ||
            retail->browser_timer_overflow != replacement->browser_timer_overflow ||
            retail->browser_timer_dispatches != replacement->browser_timer_dispatches ||
            retail->browser_refresh_callbacks != replacement->browser_refresh_callbacks ||
            retail->browser_nested_refresh_attempts !=
                replacement->browser_nested_refresh_attempts ||
            memcmp(retail->browser_timers, replacement->browser_timers,
                   sizeof(retail->browser_timers));
        uint32_t lifetime_diff = retail->lifetime_ok != replacement->lifetime_ok ||
            retail->owner_cleanup_order != replacement->owner_cleanup_order ||
            retail->hwnd_alive_after_mask != replacement->hwnd_alive_after_mask ||
            memcmp(retail->nc_destroy, replacement->nc_destroy, sizeof(retail->nc_destroy)) ||
            memcmp(retail->post_destroy, replacement->post_destroy, sizeof(retail->post_destroy)) ||
            memcmp(retail->deleting_dtor, replacement->deleting_dtor, sizeof(retail->deleting_dtor)) ||
            memcmp(retail->nc_order, replacement->nc_order, sizeof(retail->nc_order)) ||
            memcmp(retail->post_order, replacement->post_order, sizeof(retail->post_order)) ||
            memcmp(retail->delete_order, replacement->delete_order, sizeof(retail->delete_order)) ||
            memcmp(retail->delete_flags, replacement->delete_flags, sizeof(retail->delete_flags));
        ++s_flow_child_inventory_total_pairs;
        if (!child_inventory_diff) ++s_flow_child_inventory_pairs;
        difference = timing_diff || initial_diff || child_inventory_diff ||
            transition_diff || validation_diff || callback_diff ||
            browser_dispatch_diff || lifetime_diff;
        if (difference) {
            fprintf(stderr, "MP_DIALOG 0x110a DIFFER cycle=%lu case=%lu groups timing=%u initial=%u child_controls=%u transitions=%u validation=%u callbacks=%u browser_dispatch=%u lifetime=%u\n",
                (unsigned long)cycle, (unsigned long)case_index, timing_diff, initial_diff,
                child_inventory_diff,
                transition_diff, validation_diff, callback_diff, browser_dispatch_diff,
                lifetime_diff);
            if (child_inventory_diff) {
                uint32_t page;
                for (page = 0; page < 5; ++page) {
                    uint32_t child;
                    uint32_t count = retail->child_control_count[page] <
                        replacement->child_control_count[page] ?
                        retail->child_control_count[page] :
                        replacement->child_control_count[page];
                    fprintf(stderr, " FLOW CHILD TREE page=%lu count=%u/%u overflow=%u/%u\n",
                        (unsigned long)page, retail->child_control_count[page],
                        replacement->child_control_count[page],
                        retail->child_control_overflow[page],
                        replacement->child_control_overflow[page]);
                    for (child = 0; child < count; ++child) {
                        const struct fs98_flow_child_control *a =
                            &retail->child_controls[page][child];
                        const struct fs98_flow_child_control *b =
                            &replacement->child_controls[page][child];
                        if (memcmp(a, b, sizeof(*a)) != 0) {
                            fprintf(stderr, "  first child=%u\n", child);
                            fprintf(stderr, "   retail id=%08x class='%s' text='%s' style=%08x exstyle=%08x enabled=%u visible=%u/%u rect=%d,%d %dx%d\n",
                                a->id, a->class_name, a->text, a->style,
                                a->exstyle, a->enabled, a->style_visible,
                                a->effective_visible, a->x, a->y,
                                a->width, a->height);
                            fprintf(stderr, "   ours   id=%08x class='%s' text='%s' style=%08x exstyle=%08x enabled=%u visible=%u/%u rect=%d,%d %dx%d\n",
                                b->id, b->class_name, b->text, b->style,
                                b->exstyle, b->enabled, b->style_visible,
                                b->effective_visible, b->x, b->y,
                                b->width, b->height);
                            break;
                        }
                    }
                }
            }
            if (browser_dispatch_diff) {
                uint32_t timer_index = 0;
                uint32_t timer_limit = retail->browser_timer_count < replacement->browser_timer_count ?
                    retail->browser_timer_count : replacement->browser_timer_count;
                while (timer_index < timer_limit &&
                    memcmp(&retail->browser_timers[timer_index],
                           &replacement->browser_timers[timer_index],
                           sizeof(retail->browser_timers[timer_index])) == 0)
                    ++timer_index;
                if (timer_index < timer_limit ||
                    retail->browser_timer_count != replacement->browser_timer_count) {
                    const struct fs98_flow_timer_event *retail_event =
                        timer_index < retail->browser_timer_count ?
                            &retail->browser_timers[timer_index] : 0;
                    const struct fs98_flow_timer_event *replacement_event =
                        timer_index < replacement->browser_timer_count ?
                            &replacement->browser_timers[timer_index] : 0;
                    fprintf(stderr, " FLOW BROWSER FIRST DIVERGENCE event=%lu retail=%s",
                        (unsigned long)timer_index, retail_event ? "present" : "end");
                    if (retail_event)
                        fprintf(stderr, "(action=%u id=%u interval=%u callback=%u result=%u)",
                            retail_event->action, retail_event->id, retail_event->interval,
                            retail_event->callback_present, retail_event->result);
                    fprintf(stderr, " replacement=%s", replacement_event ? "present" : "end");
                    if (replacement_event)
                        fprintf(stderr, "(action=%u id=%u interval=%u callback=%u result=%u)",
                            replacement_event->action, replacement_event->id,
                            replacement_event->interval, replacement_event->callback_present,
                            replacement_event->result);
                    fprintf(stderr, " timer_dispatches=%u/%u refresh_callbacks=%u/%u "
                        "nested_attempts=%u/%u\n", retail->browser_timer_dispatches,
                        replacement->browser_timer_dispatches,
                        retail->browser_refresh_callbacks,
                        replacement->browser_refresh_callbacks,
                        retail->browser_nested_refresh_attempts,
                        replacement->browser_nested_refresh_attempts);
                } else {
                    fprintf(stderr, " FLOW BROWSER FIRST DIVERGENCE timer_dispatches=%u/%u "
                        "refresh_callbacks=%u/%u nested_attempts=%u/%u\n",
                        retail->browser_timer_dispatches, replacement->browser_timer_dispatches,
                        retail->browser_refresh_callbacks,
                        replacement->browser_refresh_callbacks,
                        retail->browser_nested_refresh_attempts,
                        replacement->browser_nested_refresh_attempts);
                }
            }
            if (cycle == 0 && (case_index == 0 || case_index == 3 ||
                               case_index == 4 || case_index == 10)) {
                for (side = 0; side < 2; ++side) {
                    uint32_t trace_index;
                    fprintf(stderr, " FLOW TRACE side=%lu count=%lu\n",
                        (unsigned long)side,
                        (unsigned long)s_flow_trace_count[side]);
                    for (trace_index = 0; trace_index < s_flow_trace_count[side]; ++trace_index) {
                        const struct fs98_flow_trace *event = &s_flow_trace[side][trace_index];
                        fprintf(stderr, "  trace=%lu kind=%lu page=%lu msg=%04lx wp=%08lx lp=%08lx vis=%08lx focus=%lu/%lu active=%lu foreground=%lu depth=%lu site=%08lx\n",
                            (unsigned long)trace_index, (unsigned long)event->kind,
                            (unsigned long)event->page, (unsigned long)event->message,
                            (unsigned long)event->wparam, (unsigned long)event->lparam,
                            (unsigned long)event->visible_mask,
                            (unsigned long)event->focus_page,
                            (unsigned long)event->focus_id,
                            (unsigned long)event->active_page,
                            (unsigned long)event->foreground_page,
                            (unsigned long)event->depth, (unsigned long)event->site);
                    }
                }
            }
            for (side = 0; side < 2; ++side) {
                const struct fs98_flow_result *result = &s_flow_results[side];
                uint32_t trace_index;
                fprintf(stderr, " side=%lu posts=%u/%u:%08x,%08x pre=%x show=%x init=%x checkpoints=%u result=%u mode=%u always=%u provider=%u transient=%u selection=%u callbacks=%u validations=%u focus=%u,%u blocked=%u,%u state=%u,%u,%u,%u browser_timer=%u/%u life=%u masks=%u,%u after=%u\n",
                    (unsigned long)side, result->posts, result->post_message, result->post_wparam, result->post_lparam,
                    result->before_host_mask, result->after_host_show_mask, result->after_initial_mask,
                    result->checkpoint_count, result->descriptor_result, result->mode, result->always,
                    result->provider, result->transient, result->browser_selection, result->callback_count,
                    result->validation_count, result->validation_focus[0], result->validation_focus[1],
                    result->blocked_next[0], result->blocked_next[1], result->status_callsign,
                    result->status_name, result->status_pilot, result->status_observer,
                    result->browser_timer_count, result->browser_timer_dispatches,
                    result->lifetime_ok, result->hwnd_alive_mask, result->accepted_hidden_mask,
                    result->hwnd_alive_after_mask);
                fprintf(stderr, "  path=");
                for (trace_index = 0; trace_index < result->checkpoint_count; ++trace_index)
                    fprintf(stderr, "%s%u", trace_index ? "," : "", result->checkpoints[trace_index]);
                fprintf(stderr, " callbacks=");
                for (trace_index = 0; trace_index < result->callback_count && trace_index < 24; ++trace_index)
                    fprintf(stderr, "%s%u:%u:%u:%u:%u", trace_index ? "," : "",
                        result->callbacks[trace_index].id, result->callbacks[trace_index].value,
                        result->callbacks[trace_index].mode, result->callbacks[trace_index].provider,
                        result->callbacks[trace_index].selection);
                fprintf(stderr, "\n");
                fprintf(stderr, "  life owner=%u nc=%u/%u/%u/%u/%u post=%u/%u/%u/%u/%u delete=%u/%u/%u/%u/%u flags=%u/%u/%u/%u/%u\n",
                    result->owner_cleanup_order,
                    result->nc_order[0], result->nc_order[1], result->nc_order[2], result->nc_order[3], result->nc_order[4],
                    result->post_order[0], result->post_order[1], result->post_order[2], result->post_order[3], result->post_order[4],
                    result->delete_order[0], result->delete_order[1], result->delete_order[2], result->delete_order[3], result->delete_order[4],
                    result->delete_flags[0], result->delete_flags[1], result->delete_flags[2], result->delete_flags[3], result->delete_flags[4]);
                for (trace_index = 0; trace_index < result->callback_count && trace_index < 24; ++trace_index) {
                    const struct fs98_flow_event *event = &result->callbacks[trace_index];
                    fprintf(stderr, "  callback[%lu]=id%u owner%u result=%08x value=%08x mode=%u provider=%u selection=%u status=%u/%u/%u/%u\n",
                        (unsigned long)trace_index, event->id, event->owner_argument,
                        event->result, event->value, event->mode, event->provider,
                        event->selection, event->status_callsign, event->status_name,
                        event->status_pilot, event->status_observer);
                }
            }
        }
    }
    return difference;
}

static void fs98_flow_render_probe(uint32_t side, HMODULE module,
                                   struct fs98_test_linkage *linkage)
{
    uint8_t *block = s_flow_blocks[side];
    HWND *windows;
    uint32_t page;
    s_side = side;
    s_case = 300;
    s_current_module = module;
    fs98_flow_setup(side, 0);
    D32(block, 0x1c) = 1;
    fs98_flow_reset_host_focus();
    windows = (HWND *)(uintptr_t)linkage->dialogs[0].create(
        (uint32_t)(uintptr_t)s_main_window, block);
    if (!windows || !windows[0] || !windows[1] || !windows[2] ||
        !windows[3] || !windows[4] || windows[5])
        fs98_fail("0x110a render probe factory array");
    fs98_flow_reset_host_focus();
    memset(s_flow_render_results[side], 0, sizeof(s_flow_render_results[side]));
    for (page = 0; page < 5; ++page) {
        HWND window = windows[page];
        ShowWindow(window, SW_SHOWNOACTIVATE);
        if (!SetWindowPos(window, HWND_TOP, 24, 24, 0, 0,
                SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER) ||
            !RedrawWindow(window, 0, 0,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW) ||
            !IsWindowVisible(window))
            fs98_fail("show and paint 0x110a render probe page");
        fs98_flow_capture_render(window, &s_flow_render_results[side][page]);
        if (!RedrawWindow(window, 0, 0,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW))
            fs98_fail("repeat paint 0x110a render probe page");
        fs98_flow_capture_render(window, &s_flow_render_repeat[side][page]);
        {
            char name[64];
            wsprintfA(name, "0x110a-side%u-page%u", side, page);
            fs98_render_compare_repeat(name,
                &s_flow_render_results[side][page],
                &s_flow_render_repeat[side][page]);
        }
        ShowWindow(window, SW_HIDE);
    }
    for (page = 0; page < 5; ++page)
        if (IsWindow(windows[page]) && !DestroyWindow(windows[page]))
            fs98_fail("destroy 0x110a render probe page");
    s_flow_owner_active[side] = 0;
    s_flow_windows[side] = 0;
    fs98_flow_reset_host_focus();
}

static uint32_t fs98_flow_compare_renders(uint32_t cycle)
{
    uint32_t page, differences = 0;
    ++s_flow_render_total_pairs;
    for (page = 0; page < 5; ++page) {
        const struct fs98_flow_render_capture *retail = &s_flow_render_results[0][page];
        const struct fs98_flow_render_capture *replacement = &s_flow_render_results[1][page];
        ++s_flow_render_page_total_pairs;
        s_flow_rendered_page_mask |= 1u << page;
        if (memcmp(retail, replacement, sizeof(*retail)) == 0) {
            ++s_flow_render_page_pairs;
        } else {
            ++differences;
            fprintf(stderr, "MP_DIALOG RENDER DIFFER cycle=%lu page=%lu retail=%lux%lu hash=%08lx painted=%lu replacement=%lux%lu hash=%08lx painted=%lu\n",
                (unsigned long)cycle, (unsigned long)page,
                (unsigned long)retail->width, (unsigned long)retail->height,
                (unsigned long)retail->hash, (unsigned long)retail->painted_bytes,
                (unsigned long)replacement->width, (unsigned long)replacement->height,
                (unsigned long)replacement->hash,
                (unsigned long)replacement->painted_bytes);
            {
                uint32_t index, limit = retail->control_count < replacement->control_count ?
                    retail->control_count : replacement->control_count;
                for (index = 0; index < limit; ++index) {
                    const struct fs98_flow_control_render *a = &retail->controls[index];
                    const struct fs98_flow_control_render *b = &replacement->controls[index];
                    if (memcmp(a, b, sizeof(*a)) != 0) {
                        fs98_render_print_control("retail", "0x110a", index, a);
                        fs98_render_print_control("replacement", "0x110a", index, b);
                    }
                }
            }
        }
    }
    if (!differences) ++s_flow_render_pairs;
    return differences;
}

static void fs98_prepare_render_window(HWND window)
{
    fs98_flow_reset_host_focus();
    if (!IsWindow(window) || !IsWindowVisible(window) ||
        !SetWindowPos(window, HWND_TOP, 24, 24, 0, 0,
            SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER))
        fs98_fail("position isolated dialog render probe");
    SetActiveWindow(s_main_window);
    SetFocus(s_main_window);
    if (!RedrawWindow(window, 0, 0,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW) ||
        !IsWindowVisible(window))
        fs98_fail("show and paint isolated dialog render probe");
    if (GetFocus() != s_main_window)
        fs98_fail("normalize isolated dialog render focus");
    if (GetActiveWindow() != s_main_window)
        fprintf(stderr, "MP_DIALOG RENDER ACTIVE owner=%p active=%p dialog=%p visible=%u\n",
            s_main_window, GetActiveWindow(), window,
            (unsigned)IsWindowVisible(window));
}

static BOOL fs98_geometry_control_id(uint32_t id)
{
    return id >= 1035 && id <= 1038;
}

static LRESULT CALLBACK fs98_geometry_callwnd_proc(int code, WPARAM wparam,
                                                    LPARAM lparam)
{
    if (code >= 0 && s_geometry_trace_active) {
        const CWPSTRUCT *call = (const CWPSTRUCT *)lparam;
        uint32_t id = call && call->hwnd ? (uint32_t)GetDlgCtrlID(call->hwnd) : 0;
        if (call && fs98_geometry_control_id(id)) {
            uint32_t order = s_geometry_trace_order++;
            if (call->message == WM_WINDOWPOSCHANGING ||
                call->message == WM_WINDOWPOSCHANGED) {
                const WINDOWPOS *position = (const WINDOWPOS *)call->lParam;
                if (position)
                    fprintf(stderr, "MP_DIALOG GEOMETRY side=%u order=%u id=%u msg=%04lx rect=%ld,%ld %ldx%ld flags=%08lx\n",
                        s_side, order, id, (unsigned long)call->message,
                        (long)position->x, (long)position->y,
                        (long)position->cx, (long)position->cy,
                        (unsigned long)position->flags);
            } else if (call->message == WM_SETFONT) {
                fprintf(stderr, "MP_DIALOG GEOMETRY side=%u order=%u id=%u msg=WM_SETFONT font=%p redraw=%lu\n",
                    s_side, order, id, (void *)call->wParam,
                    (unsigned long)call->lParam);
            } else if (call->message == WM_GETFONT) {
                fprintf(stderr, "MP_DIALOG GEOMETRY side=%u order=%u id=%u msg=WM_GETFONT\n",
                    s_side, order, id);
            } else if (call->message == WM_USER + 105) {
                fprintf(stderr, "MP_DIALOG GEOMETRY side=%u order=%u id=%u msg=UDM_SETBUDDY buddy=%p\n",
                    s_side, order, id, (void *)call->wParam);
            }
        }
    }
    return CallNextHookEx(s_geometry_callwnd_hook, code, wparam, lparam);
}

static LRESULT CALLBACK fs98_geometry_cbt_proc(int code, WPARAM wparam,
                                                LPARAM lparam)
{
    if (code == HCBT_CREATEWND && s_geometry_trace_active && lparam) {
        const CBT_CREATEWND *create = (const CBT_CREATEWND *)lparam;
        uint32_t id = create && create->lpcs ?
            (uint32_t)(uintptr_t)create->lpcs->hMenu : 0;
        if (create && create->lpcs && fs98_geometry_control_id(id))
            fprintf(stderr, "MP_DIALOG GEOMETRY side=%u order=%u id=%u msg=CREATE rect=%ld,%ld %ldx%ld style=%08lx parent=%p\n",
                s_side, s_geometry_trace_order++, id,
                (long)create->lpcs->x, (long)create->lpcs->y,
                (long)create->lpcs->cx, (long)create->lpcs->cy,
                (unsigned long)create->lpcs->style, create->lpcs->hwndParent);
    }
    return CallNextHookEx(s_geometry_cbt_hook, code, wparam, lparam);
}

static void fs98_start_details_geometry_trace(void)
{
    s_geometry_trace_order = 0;
    s_geometry_trace_active = 1;
    s_geometry_cbt_hook = SetWindowsHookExA(WH_CBT, fs98_geometry_cbt_proc,
                                             0, GetCurrentThreadId());
    s_geometry_callwnd_hook = SetWindowsHookExA(WH_CALLWNDPROC,
        fs98_geometry_callwnd_proc, 0, GetCurrentThreadId());
    if (!s_geometry_cbt_hook || !s_geometry_callwnd_hook)
        fs98_fail("install 0x110c geometry trace hooks");
}

static void fs98_stop_details_geometry_trace(void)
{
    s_geometry_trace_active = 0;
    if (!s_geometry_cbt_hook || !UnhookWindowsHookEx(s_geometry_cbt_hook))
        fs98_fail("remove 0x110c geometry creation hook");
    if (!s_geometry_callwnd_hook || !UnhookWindowsHookEx(s_geometry_callwnd_hook))
        fs98_fail("remove 0x110c geometry message hook");
    s_geometry_cbt_hook = s_geometry_callwnd_hook = 0;
}

static void fs98_session_render_probe(uint32_t side, HMODULE module,
                                      struct fs98_test_linkage *linkage)
{
    HWND window;
    s_side = side;
    s_case = 400;
    s_current_module = module;
    s_callback_count = 0;
    memset(s_callbacks[side], 0, sizeof(s_callbacks[side]));
    fs98_setup_block(0);
    window = (HWND)linkage->dialogs[1].create((uint32_t)(uintptr_t)s_main_window,
                                               &s_blocks[side]);
    if (!IsWindow(window) || !IsWindowVisible(window))
        fs98_fail("0x110b render probe factory HWND");
    fs98_pump();
    fs98_prepare_render_window(window);
    fs98_flow_capture_render(window, &s_session_render_results[side]);
    if (!RedrawWindow(window, 0, 0,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW))
        fs98_fail("repeat paint 0x110b render probe");
    fs98_flow_capture_render(window, &s_session_render_repeat[side]);
    {
        char name[64];
        wsprintfA(name, "0x110b-side%u", side);
        fs98_render_compare_repeat(name, &s_session_render_results[side],
                                   &s_session_render_repeat[side]);
    }
    if (!DestroyWindow(window) || IsWindow(window))
        fs98_fail("destroy 0x110b render probe window");
    fs98_pump();
}

static void fs98_details_render_probe(uint32_t side, HMODULE module,
                                      struct fs98_test_linkage *linkage)
{
    HWND window;
    s_side = side;
    s_case = 401;
    s_current_module = module;
    s_details_active = 1;
    fs98_setup_details(side, 0);
    fs98_start_details_geometry_trace();
    window = (HWND)linkage->dialogs[2].create((uint32_t)(uintptr_t)s_main_window,
                                               s_details_blocks[side]);
    if (!IsWindow(window) || !IsWindowVisible(window))
        fs98_fail("0x110c render probe factory HWND");
    fs98_pump();
    fs98_stop_details_geometry_trace();
    fs98_prepare_render_window(window);
    fs98_flow_capture_render(window, &s_details_render_results[side]);
    if (!RedrawWindow(window, 0, 0,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW))
        fs98_fail("repeat paint 0x110c render probe");
    fs98_flow_capture_render(window, &s_details_render_repeat[side]);
    {
        char name[64];
        wsprintfA(name, "0x110c-side%u", side);
        fs98_render_compare_repeat(name, &s_details_render_results[side],
                                   &s_details_render_repeat[side]);
    }
    if (!DestroyWindow(window) || IsWindow(window))
        fs98_fail("destroy 0x110c render probe window");
    fs98_pump();
    s_details_owner_active[side] = 0;
    memset(s_details_rows[side], 0, sizeof(s_details_rows[side]));
    memset(s_details_names[side], 0, sizeof(s_details_names[side]));
    s_details_active = 0;
}

static uint32_t fs98_compare_single_dialog_render(const char *factory,
    const struct fs98_flow_render_capture *retail,
    const struct fs98_flow_render_capture *replacement)
{
    uint32_t index, limit;
    if (memcmp(retail, replacement, sizeof(*retail)) == 0) return 0;
    fprintf(stderr, "MP_DIALOG RENDER DIFFER cycle=1 factory=%s retail=%lux%lu hash=%08lx painted=%lu replacement=%lux%lu hash=%08lx painted=%lu\n",
        factory, (unsigned long)retail->width, (unsigned long)retail->height,
        (unsigned long)retail->hash, (unsigned long)retail->painted_bytes,
        (unsigned long)replacement->width, (unsigned long)replacement->height,
        (unsigned long)replacement->hash,
        (unsigned long)replacement->painted_bytes);
    limit = retail->control_count < replacement->control_count ?
        retail->control_count : replacement->control_count;
    for (index = 0; index < limit; ++index) {
        const struct fs98_flow_control_render *a = &retail->controls[index];
        const struct fs98_flow_control_render *b = &replacement->controls[index];
        if (memcmp(a, b, sizeof(*a)) != 0) {
            fs98_render_print_control("retail", factory, index, a);
            fs98_render_print_control("replacement", factory, index, b);
        }
    }
    return 1;
}

static uint32_t fs98_compare_added_dialog_renders(uint32_t cycle)
{
    uint32_t differences = 0;
    (void)cycle;
    ++s_session_render_total;
    if (!fs98_compare_single_dialog_render("0x110b",
            &s_session_render_results[0], &s_session_render_results[1]))
        ++s_session_render_pairs;
    else
        ++differences;
    ++s_details_render_total;
    if (!fs98_compare_single_dialog_render("0x110c",
            &s_details_render_results[0], &s_details_render_results[1]))
        ++s_details_render_pairs;
    else
        ++differences;
    return differences;
}

static uint32_t fs98_run_flow_suite(uint32_t cycle, HMODULE *modules,
                                   struct fs98_test_linkage **linkages)
{
    uint32_t case_index, first_case = 0, end_case = 12, differences = 0;
    if (s_flow_mutation_case >= 0) {
        if (cycle != 0) return 0;
        first_case = (uint32_t)s_flow_mutation_case;
        end_case = first_case + 1;
    }
    for (case_index = first_case; case_index < end_case; ++case_index)
        differences += fs98_run_flow_case(cycle, case_index, modules, linkages);
    return differences;
}

static uint32_t fs98_run_details_suite(uint32_t cycle, HMODULE *modules,
                                      struct fs98_test_linkage **linkages)
{
    static const uint32_t s_scripts = 10;
    uint32_t case_index, first_case = 0, end_case = s_scripts, differences = 0;
    if (s_details_mutation_case >= 0) {
        if (cycle != 0) return 0;
        first_case = (uint32_t)s_details_mutation_case;
        end_case = first_case + 1;
    }
    for (case_index = first_case; case_index < end_case; ++case_index) {
        uint32_t side;
        memset(s_details_results, 0, sizeof(s_details_results));
        s_details_active = 1;
        for (side = 0; side < 2; ++side) {
            HWND window;
            uint32_t dialog_index = 2;
            uint8_t *block = s_details_blocks[side];
            struct fs98_details_result *result = &s_details_results[side];
            s_side = side;
            s_case = 100 + cycle * s_scripts + case_index;
            s_current_module = modules[side];
            fs98_setup_details(side, case_index);
            window = (HWND)linkages[side]->dialogs[dialog_index].create(0, block);
            if (!IsWindow(window) || !IsWindowVisible(window)) fs98_fail("0x110c factory HWND");
            fs98_pump();
            fs98_patch_dialog_vtable(window, modules[side], 0);
            fs98_capture_details(window, side);
            fs98_capture_native_child_tree(window, &result->native_child_count,
                &result->native_child_overflow, result->native_children);
            fs98_save_details_initial(side);
            if (case_index == 0 || case_index == 2 || case_index == 3)
                fs98_modeled_host_idle_tick(window);
            if (case_index == 2 || case_index == 3) fs98_capture_details(window, side);

            if (case_index == 4) {
                SetWindowTextA(GetDlgItem(window, 0x40b), "99");
                SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x40b, 0x0200),
                             (LPARAM)GetDlgItem(window, 0x40b));
                fs98_capture_details(window, side);
                result->focus_after_validation = (uint32_t)GetDlgCtrlID(GetFocus());
                fs98_send_command(window, IDOK);
                result->hwnd_state = (uint32_t)(IsWindow(window) && IsWindowVisible(window) && block[0] == 0);
                if (IsWindowVisible(window)) fs98_send_command(window, IDCANCEL);
            } else if (case_index == 5) {
                SetWindowTextA(GetDlgItem(window, 0x40d), "9");
                SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x40d, 0x0200),
                             (LPARAM)GetDlgItem(window, 0x40d));
                fs98_capture_details(window, side);
                result->focus_after_validation = (uint32_t)GetDlgCtrlID(GetFocus());
                SetWindowTextA(GetDlgItem(window, 0x40d), "3");
                fs98_send_command(window, IDOK);
            } else if (case_index == 6 || case_index == 7) {
                SendMessageA(GetDlgItem(window, 0x406), LB_SETCURSEL, 1, 0);
                SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x406, 1),
                             (LPARAM)GetDlgItem(window, 0x406));
                fs98_send_command(window, 0x3ef);
                fs98_send_command(window, IDCANCEL);
            } else if (case_index == 1 || case_index == 8) {
                SetWindowTextA(GetDlgItem(window, 0x40b), "5");
                SetWindowTextA(GetDlgItem(window, 0x40d), "3");
                fs98_send_command(window, IDOK);
            } else if (case_index == 9) {
                fs98_send_command(window, IDCANCEL);
            } else {
                fs98_send_command(window, IDCANCEL);
            }
            if (IsWindowVisible(window)) fs98_send_command(window, IDCANCEL);
            fs98_capture_details(window, side);
            result->hwnd_state = result->hwnd_state ||
                (uint32_t)(IsWindow(window) && !IsWindowVisible(window) && GetWindowLongA(window, GWL_USERDATA) == 1);
            if ((case_index == 1 || case_index == 5 || case_index == 8) && block[0] != 1)
                result->hwnd_state |= 2;
            if ((case_index == 4 || case_index == 6 || case_index == 7 || case_index == 9) && block[0] != 0)
                result->hwnd_state |= 4;
            fs98_patch_window_proc(window);
            s_lifecycle_tick = 0;
            if (!DestroyWindow(window) || IsWindow(window)) fs98_fail("0x110c HOST-style DestroyWindow");
            fs98_pump();
            result->hwnd_state |= (uint32_t)(IsWindow(window) == 0) << 8;
            s_details_owner_active[side] = 0;
            result->owner_cleanup_order = ++s_lifecycle_tick;
            memset(s_details_rows[side], 0, sizeof(s_details_rows[side]));
            memset(s_details_names[side], 0, sizeof(s_details_names[side]));
            /* Snapshot callbacks and opaque words before any mutation cleanup. */
            result->opaque_bc = D32(block, 0xbc);
            result->opaque_d0 = D32(block, 0xd0);
            fs98_restore_dialog_vtable();
            if (!result->delete_count) {
                void **vtable = PTR(s_test_dialog[side], 0);
                ((void *(TC *)(void *, uint32_t))vtable[1])(s_test_dialog[side], 1);
            }
        }
        s_details_active = 0;
        ++s_details_child_tree_total;
        if (s_details_results[0].native_child_count == s_details_results[1].native_child_count &&
            s_details_results[0].native_child_overflow == s_details_results[1].native_child_overflow &&
            !memcmp(s_details_results[0].native_children,
                    s_details_results[1].native_children,
                    sizeof(s_details_results[0].native_children)))
            ++s_details_child_tree_matches;
        else {
            ++s_details_child_tree_differences;
            if (s_details_child_tree_differences == 1) {
                const struct fs98_details_result *a = &s_details_results[0];
                const struct fs98_details_result *b = &s_details_results[1];
                uint32_t tree_index, tree_limit = a->native_child_count < b->native_child_count ?
                    a->native_child_count : b->native_child_count;
                fprintf(stderr, "MP_DIALOG CONTROL TREE DIFFER factory=0x110c cycle=%u case=%u count=%u/%u\n",
                    cycle, case_index, a->native_child_count, b->native_child_count);
                for (tree_index = 0; tree_index < tree_limit; ++tree_index) {
                    const struct fs98_flow_child_control *left = &a->native_children[tree_index];
                    const struct fs98_flow_child_control *right = &b->native_children[tree_index];
                    if (memcmp(left, right, sizeof(*left)) != 0)
                        fprintf(stderr, " control index=%u retail=id%u %s %ldx%ld at %ld,%ld text='%s'; replacement=id%u %s %ldx%ld at %ld,%ld text='%s'\n",
                            tree_index, left->id, left->class_name,
                            (long)left->width, (long)left->height, (long)left->x,
                            (long)left->y, left->text, right->id, right->class_name,
                            (long)right->width, (long)right->height, (long)right->x,
                            (long)right->y, right->text);
                }
            }
        }
        if (memcmp(&s_details_results[0], &s_details_results[1],
                offsetof(struct fs98_details_result, native_child_count)) != 0) {
            const struct fs98_details_result *a = &s_details_results[0];
            const struct fs98_details_result *b = &s_details_results[1];
            uint32_t refresh_diff = a->list_count != b->list_count ||
                memcmp(a->rows, b->rows, sizeof(a->rows)) != 0 ||
                a->refresh_updates != b->refresh_updates ||
                a->initial_list_count != b->initial_list_count ||
                memcmp(a->initial_rows, b->initial_rows, sizeof(a->initial_rows)) != 0;
            uint32_t capacity_diff = strcmp(a->pilot, b->pilot) || strcmp(a->observer, b->observer) ||
                a->pilot_status != b->pilot_status || a->observer_status != b->observer_status ||
                a->block_result != b->block_result || memcmp(a->pilot_range, b->pilot_range, sizeof(a->pilot_range)) ||
                memcmp(a->observer_range, b->observer_range, sizeof(a->observer_range)) ||
                a->focus_after_validation != b->focus_after_validation ||
                strcmp(a->initial_pilot, b->initial_pilot) || strcmp(a->initial_observer, b->initial_observer) ||
                memcmp(a->initial_ranges, b->initial_ranges, sizeof(a->initial_ranges));
            uint32_t removal_diff = a->removed_player != b->removed_player ||
                a->removal_confirmed != b->removal_confirmed;
            uint32_t callback_diff = a->callbacks != b->callbacks ||
                memcmp(a->callback_ids, b->callback_ids, sizeof(a->callback_ids)) ||
                memcmp(a->callback_args, b->callback_args, sizeof(a->callback_args)) ||
                memcmp(a->callback_results, b->callback_results, sizeof(a->callback_results)) ||
                memcmp(a->callback_values, b->callback_values, sizeof(a->callback_values));
            uint32_t control_tree_diff = a->native_child_count != b->native_child_count ||
                a->native_child_overflow != b->native_child_overflow ||
                memcmp(a->native_children, b->native_children, sizeof(a->native_children));
            uint32_t opaque_diff = a->opaque_bc != b->opaque_bc || a->opaque_d0 != b->opaque_d0;
            opaque_diff |= a->opaque_changed || b->opaque_changed;
            uint32_t lifetime_diff = a->nc_destroy_count != b->nc_destroy_count ||
                a->post_count != b->post_count || a->delete_count != b->delete_count ||
                a->nc_order != b->nc_order || a->post_order != b->post_order ||
                a->delete_order != b->delete_order || a->owner_cleanup_order != b->owner_cleanup_order ||
                !(a->nc_order < a->post_order && a->post_order < a->delete_order &&
                  a->delete_order < a->owner_cleanup_order) ||
                !(b->nc_order < b->post_order && b->post_order < b->delete_order &&
                  b->delete_order < b->owner_cleanup_order);
            fprintf(stderr, "MP_DIALOG 0x110c DIFFER cycle=%lu case=%lu groups refresh=%u capacity=%u removal=%u callbacks=%u controls=%u opaque=%u lifetime=%u state=%u initial=%u\n",
                (unsigned long)cycle, (unsigned long)case_index, refresh_diff, capacity_diff,
                removal_diff, callback_diff, control_tree_diff, opaque_diff, lifetime_diff,
                a->hwnd_state != b->hwnd_state,
                memcmp(a->initial_buttons, b->initial_buttons, sizeof(a->initial_buttons)) != 0 ||
                memcmp(a->initial_visible, b->initial_visible, sizeof(a->initial_visible)) != 0 ||
                strcmp(a->initial_session, b->initial_session) || a->initial_selected != b->initial_selected);
            if (control_tree_diff) {
                uint32_t tree_index, tree_limit = a->native_child_count < b->native_child_count ?
                    a->native_child_count : b->native_child_count;
                fprintf(stderr, "  native child trees count=%u/%u overflow=%u/%u\n",
                    a->native_child_count, b->native_child_count,
                    a->native_child_overflow, b->native_child_overflow);
                for (tree_index = 0; tree_index < tree_limit; ++tree_index) {
                    const struct fs98_flow_child_control *left = &a->native_children[tree_index];
                    const struct fs98_flow_child_control *right = &b->native_children[tree_index];
                    if (memcmp(left, right, sizeof(*left)) != 0) {
                        fprintf(stderr, "  native child first index=%u retail=%u/%s/%08x/%08x %ld,%ld %ldx%ld enabled=%u visible=%u/%u text='%s' replacement=%u/%s/%08x/%08x %ld,%ld %ldx%ld enabled=%u visible=%u/%u text='%s'\n",
                            tree_index, left->id, left->class_name, left->style, left->exstyle,
                            (long)left->x, (long)left->y, (long)left->width, (long)left->height,
                            left->enabled, left->style_visible, left->effective_visible, left->text,
                            right->id, right->class_name, right->style, right->exstyle,
                            (long)right->x, (long)right->y, (long)right->width, (long)right->height,
                            right->enabled, right->style_visible, right->effective_visible, right->text);
                        break;
                    }
                }
            }
            for (side = 0; side < 2; ++side) {
                const struct fs98_details_result *r = &s_details_results[side];
                fprintf(stderr, " side=%lu mode=%u result=%u vis=%u buttons=%u,%u,%u,%u list=%u sel=%u ids=%u,%u,%u rows='%s','%s','%s' caps='%s','%s' session='%s' ranges=%u,%u/%u,%u callbacks=%u opaque=%08x,%08x focus=%u hwnd=%u life=%u/%u/%u order=%u,%u,%u\n",
                    (unsigned long)side, r->mode, r->block_result, r->visible,
                    r->ok_enabled, r->cancel_enabled, r->close_enabled, r->remove_enabled,
                    r->list_count, r->selected, r->row_ids[0], r->row_ids[1], r->row_ids[2],
                    r->rows[0], r->rows[1], r->rows[2], r->pilot, r->observer, r->session,
                    r->pilot_range[0], r->pilot_range[1], r->observer_range[0], r->observer_range[1],
                    r->callbacks, r->opaque_bc, r->opaque_d0, r->focus_after_validation, r->hwnd_state,
                    r->nc_destroy_count, r->post_count, r->delete_count,
                    r->nc_order, r->post_order, r->delete_order);
                fprintf(stderr, "  callback-ids=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u results=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u values=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u focus_status=%u,%u\n",
                    r->callback_ids[0], r->callback_ids[1], r->callback_ids[2], r->callback_ids[3],
                    r->callback_ids[4], r->callback_ids[5], r->callback_ids[6], r->callback_ids[7],
                    r->callback_ids[8], r->callback_ids[9], r->callback_results[0], r->callback_results[1],
                    r->callback_results[2], r->callback_results[3], r->callback_results[4], r->callback_results[5],
                    r->callback_results[6], r->callback_results[7], r->callback_results[8], r->callback_results[9],
                    r->callback_values[0], r->callback_values[1], r->callback_values[2], r->callback_values[3],
                    r->callback_values[4], r->callback_values[5], r->callback_values[6], r->callback_values[7],
                    r->callback_values[8], r->callback_values[9], r->pilot_status, r->observer_status);
                fprintf(stderr, "  initial en=%u,%u,%u,%u vis=%u,%u,%u,%u members=%u,%u,%u,%u count=%u selected=%u session='%s' pilot='%s' observer='%s' rows='%s','%s','%s' range=%u,%u,%u,%u\n",
                    r->initial_buttons[0], r->initial_buttons[1], r->initial_buttons[2], r->initial_buttons[3],
                    r->initial_visible[0], r->initial_visible[1], r->initial_visible[2], r->initial_visible[3],
                    r->button_members[0], r->button_members[1], r->button_members[2], r->button_members[3],
                    r->initial_list_count, r->initial_selected, r->initial_session,
                    r->initial_pilot, r->initial_observer, r->initial_rows[0], r->initial_rows[1],
                    r->initial_rows[2], r->initial_ranges[0], r->initial_ranges[1],
                    r->initial_ranges[2], r->initial_ranges[3]);
            }
            ++differences;
        }
    }
    return differences;
}

static void fs98_mutation_unload(HMODULE *modules,
                                 struct fs98_test_linkage **linkages)
{
    uint32_t side;
    for (side = 0; side < 2; ++side) {
        s_side = side;
        s_current_module = modules[side];
        linkages[side]->deinit();
        if (s_host_init[side] != 1 || s_host_release[side] != 1)
            fs98_fail("mutation HOST module registration/release count");
        if (!FreeLibrary(modules[side])) fs98_fail("mutation FreeLibrary module");
        if (GetModuleHandleA(side ? "MP_DIALOG_ours.dll" : "mpdialog.dll"))
            fs98_fail("mutation module survived unload");
    }
}

int main(int argc, char **argv)
{
    HMODULE modules[2], global;
    struct fs98_test_linkage *linkages[2];
    char directory[MAX_PATH], module_path[MAX_PATH], retail_path[MAX_PATH];
    uint32_t cases = argc > 2 ? strtoul(argv[2], 0, 0) : 12;
    uint32_t cycle, case_index, side, comparisons = 0, differences = 0;
    uint32_t details_differences = 0, flow_differences = 0, render_differences = 0;
    uint32_t visibility_checkpoint_matches = 0;
    int32_t arg_index;
    if (!cases || cases > 100) return 2;
    for (arg_index = 3; arg_index < argc; ++arg_index) {
        if (strcmp(argv[arg_index], "--cleanup-flow-lifetime-leaks") == 0) {
            s_cleanup_flow_lifetime_leaks = 1;
        } else if (strcmp(argv[arg_index], "--flow-mutation-case") == 0 &&
                   arg_index + 1 < argc) {
            uint32_t mutation_case = strtoul(argv[++arg_index], 0, 0);
            if (mutation_case >= 12) return 2;
            s_flow_mutation_case = (int32_t)mutation_case;
        } else if (strcmp(argv[arg_index], "--session-mutation-case") == 0 &&
                   arg_index + 1 < argc) {
            uint32_t mutation_case = strtoul(argv[++arg_index], 0, 0);
            if (mutation_case >= cases) return 2;
            s_session_mutation_case = (int32_t)mutation_case;
        } else if (strcmp(argv[arg_index], "--details-mutation-case") == 0 &&
                   arg_index + 1 < argc) {
            uint32_t mutation_case = strtoul(argv[++arg_index], 0, 0);
            if (mutation_case >= 10) return 2;
            s_details_mutation_case = (int32_t)mutation_case;
        } else if (strcmp(argv[arg_index], "--render-only") == 0) {
            s_render_only = 1;
        } else {
            return 2;
        }
    }
    if ((s_flow_mutation_case >= 0) + (s_session_mutation_case >= 0) +
        (s_details_mutation_case >= 0) + s_render_only > 1) return 2;
    setvbuf(stdout, 0, _IONBF, 0);
    SetUnhandledExceptionFilter(fs98_check_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, 0);
    SetDllDirectoryA(directory);
    s_mfc = (HMODULE)fs98_load("cd\\mfc42.dll");
    fs98_application(s_mfc);
    s_ui = (HMODULE)fs98_load("orig\\SIMSUI.DLL");
    global = (HMODULE)fs98_load("orig\\Modules\\GLOBAL.DLL");
    memcpy(s_global, GetProcAddress(global, "Linkage"), 4764);
    memset(s_host_services, 0, sizeof(s_host_services));
    for (case_index = 0; case_index < sizeof(s_host_services); case_index += 4)
        *(void **)(s_host_services + case_index) = (void *)fs98_host_unexpected;
    *(void **)(s_host_services + 0x298) = (void *)fs98_host_register;
    *(void **)(s_host_services + 0x29c) = (void *)fs98_host_release;
    *(void **)(s_host_services + 0x10c) = (void *)fs98_host_dialog;
    *(void **)(s_host_services + 0x20c) = (void *)fs98_host_allocate;
    *(void **)(s_host_services + 0x210) = (void *)fs98_host_reallocate;
    *(void **)(s_host_services + 0x214) = (void *)fs98_host_free;
    {
        struct fs98_test_import *imports = (void *)GetProcAddress(s_ui, "ImportTable");
        if (!imports || imports[0].id != 1 || imports[1].id != 2)
            fs98_fail("SIMSUI ImportTable");
        imports[0].link = s_host_services;
        imports[1].link = s_global;
    }
    GetFullPathNameA("bin/mpdialog-diff", MAX_PATH, directory, 0);
    if (!SetCurrentDirectoryA(directory)) return 2;
    GetFullPathNameA(argc > 1 ? argv[1] : "MP_DIALOG_ours.dll", MAX_PATH, module_path, 0);
    GetFullPathNameA("../../orig/English/mpdialog.dll", MAX_PATH, retail_path, 0);

    for (cycle = 0; cycle < 2; ++cycle) {
        modules[0] = (HMODULE)fs98_load(retail_path);
        modules[1] = (HMODULE)fs98_load(module_path);
        if (modules[0] == modules[1]) fs98_fail("retail and replacement resolved to same module");
        for (side = 0; side < 2; ++side) {
            struct fs98_test_import *imports = (void *)GetProcAddress(modules[side], "ImportTable");
            linkages[side] = (void *)GetProcAddress(modules[side], "Linkage");
            if (!imports || !linkages[side] || linkages[side]->id != 0x8000000a ||
                imports[0].id != 1 || imports[1].id != 2 || imports[2].id ||
                !linkages[side]->init || !linkages[side]->deinit) fs98_fail("module ABI");
            imports[0].link = s_host_services;
            imports[1].link = s_global;
            if (side == 0) {
                if (linkages[side]->dialogs[0].id != 0x110a ||
                    linkages[side]->dialogs[1].id != 0x110b ||
                    linkages[side]->dialogs[2].id != 0x110c ||
                    linkages[side]->dialogs[3].id != 0)
                    fs98_fail("retail registration list");
            } else if (linkages[side]->dialogs[0].id != 0x110a ||
                       linkages[side]->dialogs[0].flags != 2 ||
                       !linkages[side]->dialogs[0].create ||
                       linkages[side]->dialogs[1].id != 0x110b ||
                       linkages[side]->dialogs[1].flags != 0 ||
                       !linkages[side]->dialogs[1].create ||
                       linkages[side]->dialogs[2].id != 0x110c ||
                       linkages[side]->dialogs[2].flags != 0 ||
                       !linkages[side]->dialogs[2].create ||
                       linkages[side]->dialogs[3].id != 0) {
                fs98_fail("replacement registration scope");
            }
            fs98_check_public_factories(side, modules[side], linkages[side]);
        }
        for (side = 0; side < 2; ++side) {
            s_side = side;
            s_current_module = modules[side];
            linkages[side]->init();
        }
        if (s_render_only) {
            for (side = 0; side < 2; ++side)
                fs98_flow_render_probe(side, modules[side], linkages[side]);
            differences += fs98_flow_compare_renders(0);
            if (differences)
                fprintf(stderr, "MP_DIALOG RENDER MUTATION DIFFER cycle=0 groups rendering=1\n");
            fs98_mutation_unload(modules, linkages);
            printf("MP_DIALOG render-only comparison: %lu differing page(s); modules deinitialized and unloaded\n",
                (unsigned long)differences);
            return differences ? 1 : 0;
        }
        if (s_session_mutation_case < 0 && s_details_mutation_case < 0)
            flow_differences += fs98_run_flow_suite(cycle, modules, linkages);
        if (s_flow_mutation_case >= 0) {
            fs98_mutation_unload(modules, linkages);
            printf("MP_DIALOG 0x110a mutation comparison: case %ld, %lu differing script(s); modules deinitialized and unloaded\n",
                (long)s_flow_mutation_case, (unsigned long)flow_differences);
            return flow_differences ? 1 : 0;
        }
        if (s_details_mutation_case >= 0) {
            details_differences += fs98_run_details_suite(cycle, modules, linkages);
            fs98_mutation_unload(modules, linkages);
            printf("MP_DIALOG 0x110c mutation comparison: case %ld, %lu differing script(s); modules deinitialized and unloaded\n",
                (long)s_details_mutation_case, (unsigned long)details_differences);
            return details_differences ? 1 : 0;
        }
        if (s_session_mutation_case >= 0 && cycle != 0) break;
        {
            uint32_t first_case = 0, end_case = cases;
            if (s_details_mutation_case >= 0) {
                end_case = 0;
            } else if (s_session_mutation_case >= 0) {
                first_case = (uint32_t)s_session_mutation_case;
                end_case = first_case + 1;
            }
        for (case_index = first_case; case_index < end_case; ++case_index) {
            memset(s_results, 0, sizeof(s_results));
            memset(s_callbacks, 0, sizeof(s_callbacks));
            memset(s_callback_returns, 0, sizeof(s_callback_returns));
            s_case = case_index;
            for (side = 0; side < 2; ++side) {
                HWND window;
                struct fs98_mpdialog_owner_block *block;
                struct fs98_result *result = &s_results[side];
                struct fs98_test_import *imports;
                uint32_t dialog_index = 1;
                s_side = side;
                s_current_module = modules[side];
                s_callback_count = 0;
                s_callback_returns[2] = case_index & 1 ? (uint32_t)-1 : 0;
                s_callback_returns[3] = case_index & 2 ? (uint32_t)-1 : 0;
                s_callback_returns[4] = case_index & 4 ? (uint32_t)-1 : 0;
                fs98_setup_block(case_index);
                block = &s_blocks[side];
                result->descriptor_visibility[0] = block->reserved18;
                result->descriptor_visibility[1] = block->flight_conditions;
                imports = (void *)GetProcAddress(modules[side], "ImportTable");
                if (imports[0].link != s_host_services || imports[1].link != s_global)
                    fs98_fail("ImportTable side binding");
                memset(s_create_styles, 0, sizeof(s_create_styles));
                memset(s_create_parents, 0, sizeof(s_create_parents));
                memset(s_create_counts, 0, sizeof(s_create_counts));
                s_create_hook = SetWindowsHookExA(WH_CBT, fs98_create_hook_proc, 0,
                                                  GetCurrentThreadId());
                if (!s_create_hook) fs98_fail("install resource-child creation hook");
                window = (HWND)linkages[side]->dialogs[dialog_index].create(0, block);
                if (!UnhookWindowsHookEx(s_create_hook)) fs98_fail("remove resource-child creation hook");
                s_create_hook = 0;
                memcpy(result->create_button_styles, s_create_styles[side],
                       sizeof(result->create_button_styles[0]) * 2);
                memcpy(result->create_parent_visible, s_create_parents[side],
                       sizeof(result->create_parent_visible[0]) * 2);
                memcpy(result->create_seen, s_create_counts[side],
                       sizeof(result->create_seen[0]) * 2);
                if (result->create_seen[0] != 1 || result->create_seen[1] != 1)
                    fs98_fail("resource creation checkpoint missed a flight-condition button");
                result->descriptor_after_init[0] = block->reserved18;
                result->descriptor_after_init[1] = block->flight_conditions;
                if (!IsWindow(window) || !IsWindowVisible(window)) {
                    fprintf(stderr, "factory side=%lu hwnd=%p error=%lu module=%p\n",
                        (unsigned long)side, window, GetLastError(), modules[side]);
                    fs98_fail("factory did not create a visible HWND");
                }
                fs98_pump();
                fs98_patch_dialog_vtable(window, modules[side], 1);
                fs98_capture_init(window, s_test_dialog[side], result);
                fs98_capture_native_child_tree(window, &result->native_child_count,
                    &result->native_child_overflow, result->native_children);
                ((void (TC *)(void *))PTR(PTR(s_test_dialog[side], 0), 0xf0))(
                    s_test_dialog[side]);
                fs98_capture_buttons(window, result->direct_update_buttons);
                fs98_capture_update_styles(window, result);

                SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x419, 0x0200),
                             (LPARAM)GetDlgItem(window, 0x419));
                if (result->validate_count != 1) fs98_fail("focus-loss did not reach inherited ValidateData");
                fs98_send_command(window, 1027);
                fs98_send_command(window, 1003);
                fs98_send_command(window, 1006);
                fs98_set_checks(s_test_dialog[side], case_index);
                fs98_capture_checks(s_test_dialog[side], result->after_checks);

                if (case_index & 1) fs98_send_command(window, IDCANCEL);
                else fs98_send_command(window, IDOK);
                if (!IsWindow(window) || IsWindowVisible(window) ||
                    GetWindowLongA(window, GWL_USERDATA) != 1) fs98_fail("Close did not mark and hide HWND");
                if (block->result != ((case_index & 1) ? 0u : 1u))
                    fs98_fail("OK/cancel result word");
                memcpy(result->descriptor, block, sizeof(*block));
                memcpy(result->callbacks, s_callbacks[side],
                       s_callback_count * sizeof(s_callbacks[side][0]));
                result->callback_count = s_callback_count;
                result->hwnd_state = (uint32_t)(IsWindow(window) && !IsWindowVisible(window) &&
                    GetWindowLongA(window, GWL_USERDATA) == 1);
                if (s_callback_count && s_callbacks[side][0].id == 0)
                    fs98_fail("+0x04 read callback invoked by 0x110b");
                if (s_callback_count != ((case_index & 1) ? 3u : 4u))
                    fs98_fail("callback count/order through OK/cancel");

                fs98_patch_window_proc(window);
                s_lifecycle_tick = 0;
                if (!DestroyWindow(window) || IsWindow(window)) fs98_fail("HOST-style DestroyWindow");
                fs98_pump();
                if (result->nc_destroy_count != 1 || result->post_count != 1 ||
                    result->delete_count != 1 ||
                    !(result->nc_destroy_order < result->post_order &&
                      result->post_order < result->delete_order))
                    fs98_fail("WM_NCDESTROY/PostNcDestroy/deleting destructor counts");
                fs98_restore_dialog_vtable();
            }
            ++s_session_child_tree_total;
            if (s_results[0].native_child_count == s_results[1].native_child_count &&
                s_results[0].native_child_overflow == s_results[1].native_child_overflow &&
                !memcmp(s_results[0].native_children, s_results[1].native_children,
                        sizeof(s_results[0].native_children)))
                ++s_session_child_tree_matches;
            else {
                ++s_session_child_tree_differences;
                if (cycle == 0 && case_index == 0)
                    fprintf(stderr, "MP_DIALOG CONTROL TREE DIFFER factory=0x110b case=%lu count=%lu/%lu\n",
                        (unsigned long)case_index,
                        (unsigned long)s_results[0].native_child_count,
                        (unsigned long)s_results[1].native_child_count);
            }
            if (fs98_visibility_checkpoints_equal(&s_results[0], &s_results[1]))
                ++visibility_checkpoint_matches;
            if (memcmp(&s_results[0], &s_results[1],
                    offsetof(struct fs98_result, native_child_count)) != 0) {
                const struct fs98_result *retail = &s_results[0];
                const struct fs98_result *replacement = &s_results[1];
                fprintf(stderr, "MP_DIALOG UI DIFFER cycle=%lu case=%lu\n",
                        (unsigned long)cycle, (unsigned long)case_index);
                fprintf(stderr, " groups create_checkpoint=%u controls=%u checks=%u buttons=%u visibility=%u direct_update=%u checked_after_input=%u descriptor=%u callbacks=%u counters=%u hwnd=%u\n",
                    memcmp(retail->create_button_styles, replacement->create_button_styles,
                           sizeof(retail->create_button_styles)) != 0 ||
                    memcmp(retail->create_parent_visible, replacement->create_parent_visible,
                           sizeof(retail->create_parent_visible)) != 0 ||
                    memcmp(retail->create_seen, replacement->create_seen,
                           sizeof(retail->create_seen)) != 0 ||
                    memcmp(retail->member_button_ids, replacement->member_button_ids,
                           sizeof(retail->member_button_ids)) != 0 ||
                    retail->dialog_visible_after_create != replacement->dialog_visible_after_create ||
                    memcmp(retail->descriptor_visibility, replacement->descriptor_visibility,
                           sizeof(retail->descriptor_visibility)) != 0 ||
                    memcmp(retail->descriptor_after_init, replacement->descriptor_after_init,
                           sizeof(retail->descriptor_after_init)) != 0,
                    retail->native_child_count != replacement->native_child_count ||
                    retail->native_child_overflow != replacement->native_child_overflow ||
                    memcmp(retail->native_children, replacement->native_children,
                           sizeof(retail->native_children)) != 0,
                    memcmp(retail->initial_checks, replacement->initial_checks,
                           sizeof(retail->initial_checks)) != 0,
                    fs98_initial_enabled_diff(retail, replacement),
                    fs98_visibility_diff(retail, replacement),
                    fs98_direct_update_diff(retail, replacement),
                    memcmp(retail->after_checks, replacement->after_checks,
                           sizeof(retail->after_checks)) != 0,
                    memcmp(retail->descriptor, replacement->descriptor,
                           sizeof(retail->descriptor)) != 0,
                    memcmp(retail->callbacks, replacement->callbacks,
                           sizeof(retail->callbacks)) != 0,
                    retail->callback_count != replacement->callback_count ||
                    retail->validate_count != replacement->validate_count ||
                    retail->post_count != replacement->post_count ||
                    retail->delete_count != replacement->delete_count ||
                    retail->nc_destroy_count != replacement->nc_destroy_count ||
                    retail->nc_destroy_order != replacement->nc_destroy_order ||
                    retail->post_order != replacement->post_order ||
                    retail->delete_order != replacement->delete_order,
                    retail->hwnd_state != replacement->hwnd_state);
                for (side = 0; side < 2; ++side) {
                    const struct fs98_result *r = &s_results[side];
                    fprintf(stderr, " side=%lu create=1003:WS_VISIBLE=%u style=%08x parent=%u 1006:WS_VISIBLE=%u style=%08x parent=%u seen=%u,%u dialogvis=%u block=%u,%u->%u,%u memberids=%u,%u,%u\n",
                        (unsigned long)side,
                        (r->create_button_styles[0] & WS_VISIBLE) != 0,
                        r->create_button_styles[0], r->create_parent_visible[0],
                        (r->create_button_styles[1] & WS_VISIBLE) != 0,
                        r->create_button_styles[1], r->create_parent_visible[1],
                        r->create_seen[0], r->create_seen[1], r->dialog_visible_after_create,
                        r->descriptor_visibility[0], r->descriptor_visibility[1],
                        r->descriptor_after_init[0], r->descriptor_after_init[1],
                        r->member_button_ids[0], r->member_button_ids[1], r->member_button_ids[2]);
                    fprintf(stderr, "  after-init 1027=en%u ws%u eff%u style=%08x parent=%u 1003=en%u ws%u eff%u style=%08x parent=%u 1006=en%u ws%u eff%u style=%08x parent=%u\n",
                        r->initial_buttons[0][0], r->initial_buttons[0][1],
                        (r->initial_button_styles[0] & WS_VISIBLE) != 0,
                        r->initial_button_styles[0], r->initial_button_parent_visible[0],
                        r->initial_buttons[1][0], r->initial_buttons[1][1],
                        (r->initial_button_styles[1] & WS_VISIBLE) != 0,
                        r->initial_button_styles[1], r->initial_button_parent_visible[1],
                        r->initial_buttons[2][0], r->initial_buttons[2][1],
                        (r->initial_button_styles[2] & WS_VISIBLE) != 0,
                        r->initial_button_styles[2], r->initial_button_parent_visible[2]);
                    fprintf(stderr, "  after-direct-update 1027=en%u ws%u eff%u style=%08x parent=%u 1003=en%u ws%u eff%u style=%08x parent=%u 1006=en%u ws%u eff%u style=%08x parent=%u\n",
                        r->direct_update_buttons[0][0], r->direct_update_buttons[0][1],
                        (r->direct_update_button_styles[0] & WS_VISIBLE) != 0,
                        r->direct_update_button_styles[0], r->direct_update_parent_visible[0],
                        r->direct_update_buttons[1][0], r->direct_update_buttons[1][1],
                        (r->direct_update_button_styles[1] & WS_VISIBLE) != 0,
                        r->direct_update_button_styles[1], r->direct_update_parent_visible[1],
                        r->direct_update_buttons[2][0], r->direct_update_buttons[2][1],
                        (r->direct_update_button_styles[2] & WS_VISIBLE) != 0,
                        r->direct_update_button_styles[2], r->direct_update_parent_visible[2]);
                }
                ++differences;
            }
            ++comparisons;
        }
        }
        if (s_session_mutation_case >= 0) {
            fs98_mutation_unload(modules, linkages);
            printf("MP_DIALOG 0x110b mutation comparison: case %ld, %lu differing script(s); modules deinitialized and unloaded\n",
                (long)s_session_mutation_case, (unsigned long)differences);
            return differences ? 1 : 0;
        }
        details_differences += fs98_run_details_suite(cycle, modules, linkages);
        if (cycle == 1) {
            for (side = 0; side < 2; ++side) {
                fs98_flow_render_probe(side, modules[side], linkages[side]);
                fs98_session_render_probe(side, modules[side], linkages[side]);
                fs98_details_render_probe(side, modules[side], linkages[side]);
            }
            render_differences += fs98_flow_compare_renders(cycle);
            render_differences += fs98_compare_added_dialog_renders(cycle);
        }
        for (side = 0; side < 2; ++side) {
            s_side = side;
            s_current_module = modules[side];
            linkages[side]->deinit();
            if (s_host_init[side] != cycle + 1 || s_host_release[side] != cycle + 1)
                fs98_fail("HOST module registration/release count");
            if (!FreeLibrary(modules[side])) fs98_fail("FreeLibrary module");
            if (GetModuleHandleA(side ? "MP_DIALOG_ours.dll" : "mpdialog.dll"))
                fs98_fail("module survived unload");
        }
    }
    printf("MP_DIALOG 0x110a isolated native comparison: 24 paired scripts, 2 load/unload cycles, %lu differing scripts; HOST show/dispatch and idle ticks plus MULTIP owner callbacks/storage are modeled\n",
           (unsigned long)flow_differences);
    printf("0x110a complete child-control trees: %lu/%lu paired scripts match across all five pages (ID, class, style, text, geometry, enabled and visibility)\n",
           (unsigned long)s_flow_child_inventory_pairs,
           (unsigned long)s_flow_child_inventory_total_pairs);
    printf("0x110a native page rendering: %lu/%lu paired page sets match; %lu/%lu visible WM_PRINT client/child captures match (page mask=%02lx)\n",
           (unsigned long)s_flow_render_pairs, (unsigned long)s_flow_render_total_pairs,
           (unsigned long)s_flow_render_page_pairs,
           (unsigned long)s_flow_render_page_total_pairs,
           (unsigned long)s_flow_rendered_page_mask);
    printf("MP_DIALOG 0x110b native comparison: %lu paired scripts, 2 load/unload cycles, %lu differing scripts\n",
           (unsigned long)comparisons, (unsigned long)differences);
    printf("visibility checkpoints: %lu/%lu pairs match (resource styles, parent visibility, post-init and post-update style/effective visibility)\n",
           (unsigned long)visibility_checkpoint_matches, (unsigned long)comparisons);
    printf("0x110b complete child-control trees: %lu/%lu paired session-settings dialogs match (ID, class, style, text, geometry, enabled and visibility)\n",
           (unsigned long)s_session_child_tree_matches,
           (unsigned long)s_session_child_tree_total);
    printf("MP_DIALOG 0x110c native comparison: 20 paired scripts, 2 load/unload cycles, %lu differing scripts; HOST idle ticks and MULTIP owner callbacks/storage are modeled\n",
           (unsigned long)details_differences);
    printf("0x110c complete child-control trees: %lu/%lu paired session-details dialogs match (ID, class, style, text, geometry, enabled and visibility)\n",
           (unsigned long)s_details_child_tree_matches,
           (unsigned long)s_details_child_tree_total);
    printf("0x110b native rendering: %lu/%lu paired initial session-settings dialogs match\n",
           (unsigned long)s_session_render_pairs, (unsigned long)s_session_render_total);
    printf("0x110c native rendering: %lu/%lu paired initial session-details dialogs match\n",
           (unsigned long)s_details_render_pairs, (unsigned long)s_details_render_total);
    printf("native rendering repeatability: %lu/%lu identical same-window captures match under unchanged state (%lu unstable pairs)\n",
           (unsigned long)s_render_repeat_matches,
           (unsigned long)s_render_repeat_total,
           (unsigned long)s_render_repeat_differences);
    return flow_differences || differences || details_differences || render_differences ||
        s_session_child_tree_differences || s_details_child_tree_differences ||
        s_render_repeat_differences ? 1 : 0;
}
