#define main fs98_existing_mpdialog_main
#include "mpdialoguidiff.c"
#undef main

struct probe_row { HWND hwnd; WNDPROC previous; void **vtable; void *post_original; void *delete_original; void *error_original; };
static struct probe_row probe_rows[5];
static HMODULE probe_fll, probe_dialog, probe_module;
static HWND probe_parent;
static void *probe_block;
static HHOOK probe_call_hook, probe_cbt_hook;
static unsigned probe_resource_errors, probe_callback_counts[16];
static int probe_provider_result;
static uint32_t probe_validation_field, probe_display_error_count;
static BOOL (WINAPI *probe_post_original)(HWND, UINT, WPARAM, LPARAM);
static int32_t (TC *probe_mfc_create_original)(void *, uint32_t, void *);
static uint32_t probe_fail_create_id, probe_fail_allocate_size, probe_failed_allocate;
static unsigned probe_create_calls, probe_allocate_calls;
static unsigned probe_row_count, probe_sequence;
static unsigned probe_nc[5], probe_post[5], probe_delete_count[5];
static LRESULT CALLBACK probe_callwndproc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION) {
        CWPSTRUCT *message = (CWPSTRUCT *)lparam;
        HWND intro = (HWND)(uintptr_t)*(uint32_t *)((uint8_t *)probe_module + 0xa0b8);
        if (message->message == 0x5f4) {
            printf("  SIMSUI validation-focus message hwnd=%p wp=%08lx lp=%08lx\n",
                   message->hwnd, (unsigned long)message->wParam, (unsigned long)message->lParam);
        }
        if (message->message == WM_COMMAND &&
            ((LOWORD(message->wParam) == 0x3e8) ||
             (LOWORD(message->wParam) == 0x419) ||
             (LOWORD(message->wParam) == 0x41a)))
            printf("  dispatched dialog WM_COMMAND hwnd=%p wp=%08lx lp=%08lx state=%u current=%u\n",
                   message->hwnd, (unsigned long)message->wParam, (unsigned long)message->lParam,
                   *(uint32_t *)((uint8_t *)probe_module + 0xa0ec),
                   *(uint32_t *)((uint8_t *)probe_module + 0xa0ec));
        if (message->hwnd == intro && message->message == WM_COMMAND) {
            void *object = *(void **)((uint8_t *)probe_module + 0xa0a0);
            void *descriptor = object ? *(void **)((uint8_t *)object + 0x19ec) : 0;
            printf("  dispatched intro WM_COMMAND hwnd=%p wp=%08lx lp=%08lx state=%u prev=%u descriptor=%p +1c=%u object=%p hwnd_member=%p accept_state=%u check=%u callbacks(+04,+08,+0c,+10)=%u,%u,%u,%u\n",
                   message->hwnd, (unsigned long)message->wParam, (unsigned long)message->lParam,
                   *(uint32_t *)((uint8_t *)probe_module + 0xa0ec),
                   *(uint32_t *)((uint8_t *)probe_module + 0xa0f0), descriptor,
                   descriptor ? *(uint32_t *)((uint8_t *)descriptor + 0x1c) : 0xffffffffu,
                   object, object ? *(void **)((uint8_t *)object + 0x20) : 0,
                   object ? *(uint32_t *)((uint8_t *)object + 0x19e8) : 0,
                   IsDlgButtonChecked(message->hwnd, 0x42b),
                   probe_callback_counts[0], probe_callback_counts[1],
                   probe_callback_counts[2], probe_callback_counts[3]);
        }
    }
    return CallNextHookEx(probe_call_hook, code, wparam, lparam);
}
static char probe_modal_text[2048];
static BOOL CALLBACK probe_dump_child(HWND child, LPARAM context)
{
    char class_name[96], text[256];
    size_t used = strlen(probe_modal_text);
    (void)context;
    GetClassNameA(child, class_name, sizeof(class_name));
    GetWindowTextA(child, text, sizeof(text));
    if (*text && used + strlen(text) + 2 < sizeof(probe_modal_text)) {
        if (used) strcat(probe_modal_text, " | ");
        strcat(probe_modal_text, text);
    }
    if (*text) fprintf(stderr, "  modal-child class=%s text=%s hwnd=%p\n", class_name, text, child);
    return TRUE;
}
static int probe_contains_error_phrase(const char *text)
{
    char lower[sizeof(probe_modal_text)];
    size_t index;
    strncpy(lower, text, sizeof(lower) - 1); lower[sizeof(lower) - 1] = 0;
    for (index = 0; lower[index]; ++index) lower[index] = (char)tolower((unsigned char)lower[index]);
    return strstr(lower, "resource data not found") != 0;
}
static LRESULT CALLBACK probe_cbt(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_ACTIVATE) {
        HWND window = (HWND)wparam;
        char class_name[64], title[256];
        GetClassNameA(window, class_name, sizeof(class_name));
        if (!strcmp(class_name, "#32770")) {
            GetWindowTextA(window, title, sizeof(title));
            probe_modal_text[0] = 0;
            EnumChildWindows(window, probe_dump_child, 0);
            if (probe_contains_error_phrase(title) || probe_contains_error_phrase(probe_modal_text)) {
                ++probe_resource_errors;
                fprintf(stderr, "RESOURCE ERROR DIALOG title=%s body=%s hwnd=%p (probe fails, no dismissal accepted)\n", title, probe_modal_text, window);
                if (probe_module) {
                    unsigned j;
                    HWND *array = (HWND *)((uint8_t *)probe_module + 0xa0b8);
                    fprintf(stderr, "  partial factory globals array=%p handles=%p,%p,%p,%p,%p terminator=%p objects=%p,%p,%p,%p,%p\n",
                            array, array[0], array[1], array[2], array[3], array[4], array[5],
                            *(void **)((uint8_t *)probe_module + 0xa0a0), *(void **)((uint8_t *)probe_module + 0xa0a4),
                            *(void **)((uint8_t *)probe_module + 0xa0a8), *(void **)((uint8_t *)probe_module + 0xa0ac),
                            *(void **)((uint8_t *)probe_module + 0xa0b0));
                    for (j = 0; j < 5; ++j) if (array[j]) fprintf(stderr, "  partial array[%u] valid=%u style=%08lx effective=%u\n", j,
                        IsWindow(array[j]), (unsigned long)GetWindowLongA(array[j], GWL_STYLE), IsWindowVisible(array[j]));
                }
                ExitProcess(4);
            }
        }
    }
    return CallNextHookEx(probe_cbt_hook, code, wparam, lparam);
}
static unsigned probe_find(HWND window)
{
    unsigned i;
    for (i = 0; i < probe_row_count; ++i) if (probe_rows[i].hwnd == window) return i;
    return 5;
}
static LRESULT CALLBACK probe_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    unsigned index = probe_find(window);
    if (message == WM_NCDESTROY && index < 5) {
        ++probe_nc[index];
        printf("  lifecycle[%u] step=%u WM_NCDESTROY\n", index, ++probe_sequence);
    }
    return CallWindowProcA(index < 5 ? probe_rows[index].previous : DefWindowProcA,
                           window, message, wparam, lparam);
}
static void TC probe_post_destroy(void *object)
{
    unsigned i;
    for (i = 0; i < probe_row_count; ++i) if (probe_rows[i].vtable == PTR(object, 0)) break;
    if (i < 5) { ++probe_post[i]; printf("  lifecycle[%u] step=%u PostNcDestroy\n", i, ++probe_sequence); }
    ((void (TC *)(void *))probe_rows[i].post_original)(object);
}
static void *TC probe_delete(void *object, uint32_t flags)
{
    unsigned i;
    void *result;
    for (i = 0; i < probe_row_count; ++i) if (probe_rows[i].vtable == PTR(object, 0)) break;
    if (i < 5) { ++probe_delete_count[i]; printf("  lifecycle[%u] step=%u deleting destructor flags=%u\n", i, ++probe_sequence, flags); }
    result = ((void *(TC *)(void *, uint32_t))probe_rows[i].delete_original)(object, flags);
    return result;
}
static int32_t TC probe_display_error(void *object, uint32_t control_id)
{
    unsigned i;
    for (i = 0; i < probe_row_count; ++i) if (probe_rows[i].vtable == PTR(object, 0)) break;
    ++probe_display_error_count;
    printf("  intercepted DisplayError page=%u object=%p control_id=%04x hwnd=%p\n",
           i, object, control_id, *(void **)((uint8_t *)object + 0x20));
    return 1;
}
static void probe_patch(void **slot, void *replacement, void **original)
{
    DWORD old;
    MEMORY_BASIC_INFORMATION info;
    SIZE_T queried = VirtualQuery(slot, &info, sizeof(info));
    fprintf(stderr, "vtable patch slot=%p query=%lu base=%p alloc=%p size=%lu state=%08lx protect=%08lx type=%08lx value=%p\n",
            slot, (unsigned long)queried, queried ? info.BaseAddress : 0,
            queried ? info.AllocationBase : 0, queried ? (unsigned long)info.RegionSize : 0,
            queried ? (unsigned long)info.State : 0, queried ? (unsigned long)info.Protect : 0,
            queried ? (unsigned long)info.Type : 0, queried ? *slot : 0);
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) {
        DWORD error = GetLastError();
        fprintf(stderr, "probe VirtualProtect slot=%p size=%lu new=PAGE_READWRITE old=%08lx error=%lu\n",
                slot, (unsigned long)sizeof(*slot), (unsigned long)old, (unsigned long)error);
        fflush(stderr); ExitProcess(3);
    }
    if (original) *original = *slot;
    *slot = replacement;
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    { DWORD restored; if (!VirtualProtect(slot, sizeof(*slot), old, &restored)) { fprintf(stderr, "restore VirtualProtect slot=%p old=%08lx error=%lu\n", slot, (unsigned long)old, (unsigned long)GetLastError()); ExitProcess(3); } }
}
static void probe_restore_slot(void **slot, void *original)
{
    DWORD old, restored;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) {
        fprintf(stderr, "probe restore patch slot=%p error=%lu\n", slot, (unsigned long)GetLastError()); ExitProcess(3);
    }
    *slot = original;
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    if (!VirtualProtect(slot, sizeof(*slot), old, &restored)) {
        fprintf(stderr, "probe restore protection slot=%p error=%lu\n", slot, (unsigned long)GetLastError()); ExitProcess(3);
    }
}
static void probe_dispatch_posted(HWND window, UINT message)
{
    MSG queued;
    if (!PeekMessageA(&queued, window, message, message, PM_REMOVE)) {
        printf("  posted-message check hwnd=%p message=%04x found=0\n", window, message);
        return;
    }
    printf("  posted-message check hwnd=%p found=1 message=%04x wp=%08lx lp=%08lx\n",
           queued.hwnd, queued.message, (unsigned long)queued.wParam, (unsigned long)queued.lParam);
    printf("  posted-message dispatch return=%ld\n", (long)DispatchMessageA(&queued));
}
static BOOL WINAPI probe_post_api(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_COMMAND && probe_module &&
        window == (HWND)(uintptr_t)*(uint32_t *)((uint8_t *)probe_module + 0xa0b8)) {
        void *object = *(void **)((uint8_t *)probe_module + 0xa0a0);
        void *descriptor = object ? *(void **)((uint8_t *)object + 0x19ec) : 0;
        DWORD target_thread = GetWindowThreadProcessId(window, 0);
        printf("  factory PostMessageA intro hwnd=%p wp=%08lx lp=%08lx descriptor=%p +1c=%u state=%u prev=%u object=%p hwnd_member=%p accept_state=%u check=%u callback_ptrs=%p,%p,%p,%p callback_counts=%u,%u,%u,%u iswindow=%u target_thread=%lu caller_thread=%lu wndproc=%p style=%08lx return_site=%p\n",
               window, (unsigned long)wparam, (unsigned long)lparam,
               descriptor,
               descriptor ? *(uint32_t *)((uint8_t *)descriptor + 0x1c) : 0xffffffffu,
               *(uint32_t *)((uint8_t *)probe_module + 0xa0ec),
               *(uint32_t *)((uint8_t *)probe_module + 0xa0f0), object,
               object ? *(void **)((uint8_t *)object + 0x20) : 0,
               object ? *(uint32_t *)((uint8_t *)object + 0x19e8) : 0,
               IsDlgButtonChecked(window, 0x42b),
               descriptor ? *(void **)((uint8_t *)descriptor + 0x04) : 0,
               descriptor ? *(void **)((uint8_t *)descriptor + 0x08) : 0,
               descriptor ? *(void **)((uint8_t *)descriptor + 0x0c) : 0,
               descriptor ? *(void **)((uint8_t *)descriptor + 0x10) : 0,
               probe_callback_counts[0], probe_callback_counts[1],
               probe_callback_counts[2], probe_callback_counts[3],
               IsWindow(window), (unsigned long)target_thread, (unsigned long)GetCurrentThreadId(),
               (void *)(uintptr_t)GetWindowLongA(window, GWL_WNDPROC),
               (unsigned long)GetWindowLongA(window, GWL_STYLE),
               __builtin_return_address(0));
    }
    {
        BOOL result = probe_post_original(window, message, wparam, lparam);
        if (message == WM_COMMAND && probe_module &&
            window == (HWND)(uintptr_t)*(uint32_t *)((uint8_t *)probe_module + 0xa0b8))
            printf("  factory PostMessageA returned=%d (queue success, not handler result) state_after=%u prev_after=%u\n",
                   result, *(uint32_t *)((uint8_t *)probe_module + 0xa0ec),
                   *(uint32_t *)((uint8_t *)probe_module + 0xa0f0));
        return result;
    }
}
static int32_t SC probe_string(uint32_t id, char *buffer, int32_t capacity)
{
    int32_t length = probe_fll ? LoadStringA(probe_fll, id, buffer, capacity) : 0;
    if (!length && s_current_module) length = LoadStringA(s_current_module, id, buffer, capacity);
    if (!length && buffer && capacity > 0) buffer[0] = 0;
    fprintf(stderr, "HOST +2a0 string id=%u length=%d source=%s module=%p\n", id, length,
            length && probe_fll ? "ENGLISH.FLL" : (length ? "MP_DIALOG" : "none"), s_current_module);
    return length;
}
static uint32_t SC probe_allocate(uint32_t flags, uint32_t size, void **block_out, int32_t zero)
{
    ++probe_allocate_calls;
    if (probe_fail_allocate_size == size && !probe_failed_allocate) {
        probe_failed_allocate = 1;
        *block_out = 0;
        fprintf(stderr, "injected HOST allocation failure size=%08x call=%u return_site=%p\n",
                size, probe_allocate_calls, __builtin_return_address(0));
        return (uint32_t)-1;
    }
    return fs98_host_allocate(flags, size, block_out, zero);
}
static int32_t TC probe_mfc_create(void *object, uint32_t template_id, void *parent)
{
    ++probe_create_calls;
    int32_t result;
    fprintf(stderr, "MFC ordinal 2086 this=%p template=%04x parent=%p call=%u return_site=%p\n",
            object, template_id, parent, probe_create_calls, __builtin_return_address(0));
    if (template_id == probe_fail_create_id) {
        fprintf(stderr, "injected CDialog::Create false return template=%04x object_hwnd=%p\n",
                template_id, *(void **)((uint8_t *)object + 0x20));
        return 0;
    }
    result = probe_mfc_create_original(object, template_id, parent);
    fprintf(stderr, "MFC ordinal 2086 returned=%d object_hwnd=%p\n", result,
            *(void **)((uint8_t *)object + 0x20));
    return result;
}
static LONG WINAPI probe_fault_filter(EXCEPTION_POINTERS *exception)
{
    CONTEXT *context = exception->ContextRecord;
    HWND *array = probe_module ? (HWND *)((uint8_t *)probe_module + 0xa0b8) : 0;
    void **objects = probe_module ? (void **)((uint8_t *)probe_module + 0xa0a0) : 0;
    unsigned i;
    fprintf(stderr, "MP_DIALOG 0x110a fault code=%08lx pc=%p module=%p block=%p create_calls=%u allocate_calls=%u\n",
            (unsigned long)exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress, probe_module, probe_block,
            probe_create_calls, probe_allocate_calls);
    if (context) fprintf(stderr, " eip=%08lx eax=%08lx ecx=%08lx edx=%08lx esp=%08lx\n",
            (unsigned long)context->Eip, (unsigned long)context->Eax,
            (unsigned long)context->Ecx, (unsigned long)context->Edx,
            (unsigned long)context->Esp);
    if (array && objects) for (i = 0; i < 5; ++i)
        fprintf(stderr, " partial[%u] hwnd=%p valid=%u object=%p object_hwnd=%p\n", i,
                array[i], array[i] ? IsWindow(array[i]) : 0, objects[i],
                objects[i] ? *(void **)((uint8_t *)objects[i] + 0x20) : 0);
    fflush(stderr);
    ExitProcess(6);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void probe_record_callback(unsigned id, const char *name, void *block, uint32_t value)
{
    ++probe_callback_counts[id];
    printf("  callback %s descriptor=%p expected=%p arg2=%08x result=%u\n", name, block,
           probe_block, value, *(uint32_t *)block);
    if (block != probe_block) fs98_fail("callback descriptor pointer");
}
static int32_t SC probe_provider(void *block) { probe_record_callback(0, "+04 provider", block, 0); return probe_provider_result; }
static int32_t SC probe_reset(void *block) { probe_record_callback(1, "+08 reset", block, 0); return 0; }
static int32_t SC probe_accept(void *block) { probe_record_callback(2, "+0c accept", block, 0); return 0; }
/* Modeled MULTIP failure fields for routing probes; capacity parsing is not emulated. */
static int32_t SC probe_validate(void *block)
{
    uint32_t mode = *(uint32_t *)((uint8_t *)block + 0x24);
    const char *callsign = *(const char **)((uint8_t *)block + 0x2c);
    const char *session = *(const char **)((uint8_t *)block + 0x4c);
    const char *pilots = *(const char **)((uint8_t *)block + 0x60);
    const char *observers = *(const char **)((uint8_t *)block + 0x74);
    int32_t result = 0;
    *(uint32_t *)((uint8_t *)block + 0x28) = 0;
    *(uint32_t *)((uint8_t *)block + 0x48) = 0;
    *(uint32_t *)((uint8_t *)block + 0x5c) = 0;
    *(uint32_t *)((uint8_t *)block + 0x70) = 0;
    if (mode != 2 && (!callsign || !*callsign)) {
        *(uint32_t *)((uint8_t *)block + 0x28) = 0x1003;
        result = -1;
    }
    if (mode == 1) {
        if (!session || !*session) {
            *(uint32_t *)((uint8_t *)block + 0x48) = 0x1003;
            result = -1;
        }
        if (!pilots || !*pilots) {
            *(uint32_t *)((uint8_t *)block + 0x5c) = 1;
            result = -1;
        }
        if (!observers || !*observers) {
            *(uint32_t *)((uint8_t *)block + 0x70) = 1;
            result = -1;
        }
    }
    probe_record_callback(3, "+10 validate", block, 0);
    printf("    modeled MULTIP validation output mode=%u return=%d status28=%08x status48=%08x status5c=%08x status70=%08x input='%s'/'%s'/'%s'/'%s'\n",
           mode, result, *(uint32_t *)((uint8_t *)block + 0x28),
           *(uint32_t *)((uint8_t *)block + 0x48), *(uint32_t *)((uint8_t *)block + 0x5c),
           *(uint32_t *)((uint8_t *)block + 0x70), callsign ? callsign : "(null)",
           session ? session : "(null)", pilots ? pilots : "(null)", observers ? observers : "(null)");
    return result;
}
static int32_t SC probe_host_options(void *block) { probe_record_callback(4, "+44 host-options", block, 0); return 0; }
static int32_t SC probe_refresh_discovery(void *block) { probe_record_callback(5, "+94 discovery", block, 0); return 0; }
static int32_t SC probe_players(void *block, uint32_t index) { probe_record_callback(6, "+98 players", block, index); return 0; }
static int32_t SC probe_details(void *block) { probe_record_callback(7, "+9c details", block, 0); return 0; }
static int32_t SC probe_refresh_all(void *block) { probe_record_callback(8, "+a0 refresh-all", block, 0); return 1; }
static int32_t SC probe_browser_options(void *block) { probe_record_callback(9, "+a4 browser-options", block, 0); return 0; }
static void probe_bind_imports(HMODULE module)
{
    struct fs98_test_import *imports = (void *)GetProcAddress(module, "ImportTable");
    if (!imports || imports[0].id != 1 || imports[1].id != 2) fs98_fail("probe ImportTable");
    imports[0].link = s_host_services; imports[1].link = s_global;
}
static void probe_one(HMODULE module, void *factory, uint32_t show_intro, uint32_t action)
{
    struct { char *name; unsigned char guid[16]; uint32_t major, minor; } provider;
    uint8_t block[0xc0] = {0};
    char callsign[24] = "ProbePilot", session[32] = "ProbeSession";
    char pilots[8] = "8", observers[8] = "2";
    char err0[32] = "", err1[32] = "", err2[32] = "", err3[32] = "";
    HWND *array;
    MSG queued_next;
    BOOL queued_next_found = FALSE;
    int queue_flow_done = 0;
    unsigned i;
    memset(&provider, 0, sizeof(provider)); provider.name = "Probe Provider";
    if (probe_validation_field == 1) callsign[0] = 0;
    if (probe_validation_field == 2) session[0] = 0;
    if (probe_validation_field == 3) pilots[0] = 0;
    if (probe_validation_field == 4) observers[0] = 0;
    *(void **)(block + 0x04) = (void *)probe_provider;
    *(void **)(block + 0x08) = (void *)probe_reset;
    *(void **)(block + 0x0c) = (void *)probe_accept;
    *(void **)(block + 0x10) = (void *)probe_validate;
    *(uint32_t *)(block + 0x1c) = show_intro ? 1u : 0u;
    *(uint32_t *)(block + 0x24) = action == 2 ? 1u : 0u;
    probe_provider_result = action == 3 ? 1 : 0;
    probe_block = block;
    *(char **)(block + 0x2c) = callsign; *(uint32_t *)(block + 0x30) = 20;
    *(void **)(block + 0x44) = (void *)probe_host_options;
    *(char **)(block + 0x4c) = session; *(uint32_t *)(block + 0x50) = 30;
    *(char **)(block + 0x60) = pilots; *(uint32_t *)(block + 0x64) = 4;
    *(char **)(block + 0x68) = err0; *(char **)(block + 0x6c) = err1;
    *(char **)(block + 0x74) = observers; *(uint32_t *)(block + 0x78) = 4;
    *(char **)(block + 0x7c) = err2; *(char **)(block + 0x80) = err3;
    *(void **)(block + 0x84) = &provider; *(uint32_t *)(block + 0x88) = 1;
    *(uint32_t *)(block + 0x90) = 0;
    *(void **)(block + 0x94) = (void *)probe_refresh_discovery;
    *(void **)(block + 0x98) = (void *)probe_players;
    *(void **)(block + 0x9c) = (void *)probe_details;
    *(void **)(block + 0xa0) = (void *)probe_refresh_all;
    *(void **)(block + 0xa4) = (void *)probe_browser_options;
    *(uint32_t *)(block + 0xb8) = 0xffffffffu;
    printf("fixture before factory descriptor=%p result=%u callbacks=%p,%p,%p,%p +1c=%u mode=%u callsign=%p session=%p pilots=%p observers=%p\n",
           block, *(uint32_t *)(block + 0x00), *(void **)(block + 0x04),
           *(void **)(block + 0x08), *(void **)(block + 0x0c),
           *(void **)(block + 0x10), *(uint32_t *)(block + 0x1c),
           *(uint32_t *)(block + 0x24), *(void **)(block + 0x2c),
           *(void **)(block + 0x4c), *(void **)(block + 0x60), *(void **)(block + 0x74));
    array = (HWND *)(uintptr_t)((uint32_t (SC *)(uint32_t, void *))factory)((uint32_t)(uintptr_t)probe_parent, block);
    printf("case action=%u always_show=%u array=%p result=%u state=%u previous=%u descriptor_1c=%u mode=%u DIALOG_handle=%p\n", action, show_intro, array, *(uint32_t *)block,
           *(uint32_t *)((uint8_t *)module + 0xa0ec), *(uint32_t *)((uint8_t *)module + 0xa0f0),
           *(uint32_t *)(block + 0x1c), *(uint32_t *)(block + 0x24), GetModuleHandleA("DIALOG.DLL"));
    probe_row_count = 0;
    for (i = 0; i < 6; ++i) {
        HWND hwnd = array[i];
        if (!hwnd) { printf("  array[%u]=NULL terminator\n", i); break; }
        printf("  array[%u]=%p valid=%u style_visible=%u effective_visible=%u userdata=%08lx\n", i, hwnd,
               IsWindow(hwnd), (GetWindowLongA(hwnd, GWL_STYLE) & WS_VISIBLE) != 0,
               IsWindowVisible(hwnd), (unsigned long)GetWindowLongA(hwnd, GWL_USERDATA));
        probe_rows[probe_row_count].hwnd = hwnd;
        ++probe_row_count;
    }
    if (probe_row_count != 5) fs98_fail("expected five non-null array handles");
    if (!show_intro) {
        queued_next_found = PeekMessageA(&queued_next, array[0], WM_COMMAND, WM_COMMAND, PM_NOREMOVE);
        printf("  post-factory queue check found=%u hwnd=%p message=%04x wp=%08lx lp=%08lx state=%u prev=%u descriptor_1c=%u\n",
               queued_next_found, queued_next_found ? queued_next.hwnd : 0,
               queued_next_found ? queued_next.message : 0,
               queued_next_found ? (unsigned long)queued_next.wParam : 0,
               queued_next_found ? (unsigned long)queued_next.lParam : 0,
               *(uint32_t *)((uint8_t *)module + 0xa0ec),
               *(uint32_t *)((uint8_t *)module + 0xa0f0), *(uint32_t *)(block + 0x1c));
        if (action == 4 || action == 5) {
            MSG dispatch_next;
            if (action == 5) {
                LRESULT immediate_result = SendMessageA(array[0], WM_COMMAND, 0x3e8, 0);
                {
                    void *intro_object = *(void **)((uint8_t *)module + 0xa0a0);
                    printf("  post-return synchronous SendMessageA hwnd=%p wp=%04x lp=0 return=%ld descriptor=%p +1c=%u mode=%u state=%u prev=%u object=%p object_hwnd=%p callbacks(+04,+08,+0c,+10)=%u,%u,%u,%u\n",
                       array[0], 0x3e8, (long)immediate_result,
                       block, *(uint32_t *)(block + 0x1c), *(uint32_t *)(block + 0x24),
                       *(uint32_t *)((uint8_t *)module + 0xa0ec),
                       *(uint32_t *)((uint8_t *)module + 0xa0f0), intro_object,
                       intro_object ? *(void **)((uint8_t *)intro_object + 0x20) : 0,
                       probe_callback_counts[0], probe_callback_counts[1],
                       probe_callback_counts[2], probe_callback_counts[3]);
                }
            }
            ShowWindow(array[0], SW_SHOW);
            UpdateWindow(array[0]);
            if (PeekMessageA(&dispatch_next, array[0], WM_COMMAND, WM_COMMAND, PM_REMOVE) &&
                dispatch_next.wParam == 0x3e8 && dispatch_next.lParam == 0) {
                LRESULT dispatch_result = DispatchMessageA(&dispatch_next);
                printf("  dispatched queued factory Next hwnd=%p message=%04x wp=%04lx lp=%08lx return=%ld state=%u prev=%u descriptor_1c=%u callbacks(+04,+08,+0c,+10)=%u,%u,%u,%u visible=",
                       dispatch_next.hwnd, dispatch_next.message,
                       (unsigned long)dispatch_next.wParam, (unsigned long)dispatch_next.lParam,
                       (long)dispatch_result, *(uint32_t *)((uint8_t *)module + 0xa0ec),
                       *(uint32_t *)((uint8_t *)module + 0xa0f0), *(uint32_t *)(block + 0x1c),
                       probe_callback_counts[0], probe_callback_counts[1],
                       probe_callback_counts[2], probe_callback_counts[3]);
                for (i = 0; i < 5; ++i) printf(" %u/%u",
                    (GetWindowLongA(array[i], GWL_STYLE) & WS_VISIBLE) != 0,
                    IsWindowVisible(array[i]) ? 1u : 0u);
                printf("\n");
            } else {
                printf("  queued factory Next not found during dispatch\n");
            }
            queue_flow_done = 1;
        }
    }
    if (!queue_flow_done) { ShowWindow(array[0], SW_SHOW); UpdateWindow(array[0]); }
    printf("  after HOST's unconditional first-handle ShowWindow:");
    for (i = 0; i < 5; ++i) printf(" %u/%u", (GetWindowLongA(array[i], GWL_STYLE) & WS_VISIBLE) != 0,
                                     IsWindowVisible(array[i]) ? 1u : 0u);
    printf("\n");
    for (i = 0; i < 5; ++i) {
        void *object = ((void *(SC *)(HWND))GetProcAddress(s_mfc, (const char *)(uintptr_t)2864))(array[i]);
        void **vtable = PTR(object, 0);
        HMODULE owner = 0;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (const char *)vtable[0x30/4], &owner);
        fprintf(stderr, "page=%u hwnd=%p object=%p vtable=%p owner=%p entry30=%p post=%p deleting=%p\n", i, array[i], object, vtable, owner, vtable[0x30/4], vtable[0xac/4], vtable[1]);
        probe_rows[i].vtable = vtable;
        probe_rows[i].post_original = vtable[0xac / 4];
        probe_rows[i].delete_original = vtable[4 / 4];
        probe_rows[i].error_original = vtable[0xe0 / 4];
        probe_patch(&vtable[0xac / 4], (void *)probe_post_destroy, 0);
        probe_patch(&vtable[4 / 4], (void *)probe_delete, 0);
        probe_patch(&vtable[0xe0 / 4], (void *)probe_display_error, 0);
        probe_rows[i].previous = (WNDPROC)(uintptr_t)GetWindowLongA(array[i], GWL_WNDPROC);
        SetWindowLongA(array[i], GWL_WNDPROC, (LONG)(uintptr_t)probe_window_proc);
    }
    if (action == 0 || action == 4 || action == 5 || action == 6) {
        if (show_intro && action != 6) {
            SendMessageA(array[0], WM_COMMAND, 0x3e8, 0);
            printf("  after Intro Next: state=%u visible=", *(uint32_t *)((uint8_t *)module + 0xa0ec));
            for (i = 0; i < 5; ++i) printf("%u/%u ", (GetWindowLongA(array[i], GWL_STYLE) & WS_VISIBLE) != 0, IsWindowVisible(array[i]));
            printf("\n");
            SendMessageA(array[1], WM_COMMAND, 0x3e9, 0);
            printf("  after Choice Back: state=%u\n", *(uint32_t *)((uint8_t *)module + 0xa0ec));
            SendMessageA(array[0], WM_COMMAND, 0x3e8, 0);
            printf("  after Intro Next again: state=%u\n", *(uint32_t *)((uint8_t *)module + 0xa0ec));
        } else if (action == 6) {
            HWND choice = array[1], edit = GetDlgItem(array[1], 0x419);
            SendMessageA(array[0], WM_COMMAND, 0x3e8, 0);
            printf("  choice-invalid setup state=%u edit=%p text='%s'\n",
                   *(uint32_t *)((uint8_t *)module + 0xa0ec), edit, callsign);
            SendMessageA(choice, WM_COMMAND,
                         MAKEWPARAM(0x419, EN_KILLFOCUS), (LPARAM)edit);
            printf("  choice focus-loss returned state=%u status28=%08x focus=%p focus_id=%04lx\n",
                   *(uint32_t *)((uint8_t *)module + 0xa0ec), *(uint32_t *)(block + 0x28),
                   GetFocus(), (unsigned long)(GetFocus() ? GetDlgCtrlID(GetFocus()) : 0));
            probe_dispatch_posted(choice, 0x5f4);
            SendMessageA(choice, WM_COMMAND, 0x3e8, 0);
            printf("  choice Next after invalid validation state=%u result=%u display_errors=%u status28=%08x\n",
                   *(uint32_t *)((uint8_t *)module + 0xa0ec), *(uint32_t *)block,
                   probe_display_error_count, *(uint32_t *)(block + 0x28));
        }
        SendMessageA(array[*(uint32_t *)((uint8_t *)module + 0xa0ec)], WM_COMMAND, IDCANCEL, 0);
    } else {
        SendMessageA(array[0], WM_COMMAND, 0x3e8, 0);
        printf("  after Intro Next: state=%u\n", *(uint32_t *)((uint8_t *)module + 0xa0ec));
        SendMessageA(array[1], WM_COMMAND, 0x3e8, 0);
        printf("  after Choice Next: state=%u\n", *(uint32_t *)((uint8_t *)module + 0xa0ec));
        if (action == 2) {
            HWND host = array[2];
            HWND edit = GetDlgItem(host, 0x41a);
            SendMessageA(host, WM_COMMAND, 0x426, 0);
            SendMessageA(host, WM_COMMAND, MAKEWPARAM(0x41a, EN_KILLFOCUS), (LPARAM)edit);
            printf("  host focus-loss state=%u statuses=%08x/%08x/%08x/%08x focus=%p focus_id=%04lx\n",
                   *(uint32_t *)((uint8_t *)module + 0xa0ec), *(uint32_t *)(block + 0x28),
                   *(uint32_t *)(block + 0x48), *(uint32_t *)(block + 0x5c),
                   *(uint32_t *)(block + 0x70), GetFocus(),
                   (unsigned long)(GetFocus() ? GetDlgCtrlID(GetFocus()) : 0));
            probe_dispatch_posted(host, 0x5f4);
            SendMessageA(host, WM_COMMAND, 0x3e8, 0);
            printf("  after Host Options focus-loss + Next: state=%u result=%u display_errors=%u\n",
                   *(uint32_t *)((uint8_t *)module + 0xa0ec), *(uint32_t *)block,
                   probe_display_error_count);
            SendMessageA(array[3], WM_COMMAND, 0x433, 0);
        } else {
            if (action == 1) {
                SendMessageA(array[3], WM_COMMAND, 0x3e9, 0);
                printf("  after Connection Back: state=%u\n", *(uint32_t *)((uint8_t *)module + 0xa0ec));
                SendMessageA(array[1], WM_COMMAND, 0x3e8, 0);
            }
            SendMessageA(array[3], WM_COMMAND, 0x3e8, 0);
            printf("  after Connection Next: state=%u\n", *(uint32_t *)((uint8_t *)module + 0xa0ec));
            if (action == 1) {
                HWND browser = array[4];
                HWND list = GetDlgItem(browser, 0x3fd);
                SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)"Probe Session");
                SendMessageA(list, LB_SETCURSEL, 0, 0);
                SendMessageA(browser, WM_COMMAND, MAKEWPARAM(0x3fd, LBN_SELCHANGE), (LPARAM)list);
                SendMessageA(browser, WM_COMMAND, 0x401, 0);
                SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)"Probe Session");
                SendMessageA(list, LB_SETCURSEL, 0, 0);
                SendMessageA(browser, WM_COMMAND, MAKEWPARAM(0x3fd, LBN_SELCHANGE), (LPARAM)list);
                SendMessageA(browser, WM_COMMAND, 0x400, 0);
                SendMessageA(browser, WM_COMMAND, 0x3ec, 0);
                SendMessageA(browser, WM_COMMAND, 0x3ff, 0);
            } else {
                SendMessageA(array[3], WM_COMMAND, IDCANCEL, 0);
            }
        }
    }
    printf("  after accept/cancel path result=%u:", *(uint32_t *)block);
    for (i = 0; i < 5; ++i) printf(" [%u vis=%u done=%u]", i, IsWindowVisible(array[i]) ? 1u : 0u,
        GetWindowLongA(array[i], GWL_USERDATA) == 1 ? 1u : 0u);
    printf(" callbacks provider=%u reset=%u accept=%u validate=%u host-options=%u discovery=%u players=%u details=%u refresh-all=%u browser-options=%u\n",
        probe_callback_counts[0], probe_callback_counts[1], probe_callback_counts[2], probe_callback_counts[3],
        probe_callback_counts[4], probe_callback_counts[5], probe_callback_counts[6], probe_callback_counts[7],
        probe_callback_counts[8], probe_callback_counts[9]);
    for (i = 0; i < 5; ++i) if (IsWindow(array[i])) DestroyWindow(array[i]);
    fs98_pump();
    printf("  after HOST array teardown: nc=%u,%u,%u,%u,%u post=%u,%u,%u,%u,%u delete=%u,%u,%u,%u,%u\n",
        probe_nc[0],probe_nc[1],probe_nc[2],probe_nc[3],probe_nc[4],
        probe_post[0],probe_post[1],probe_post[2],probe_post[3],probe_post[4],
        probe_delete_count[0],probe_delete_count[1],probe_delete_count[2],probe_delete_count[3],probe_delete_count[4]);
    for (i = 0; i < 5; ++i) {
        probe_restore_slot(&probe_rows[i].vtable[0xac / 4], probe_rows[i].post_original);
        probe_restore_slot(&probe_rows[i].vtable[4 / 4], probe_rows[i].delete_original);
        probe_restore_slot(&probe_rows[i].vtable[0xe0 / 4], probe_rows[i].error_original);
        if (probe_rows[i].vtable[0xac / 4] != probe_rows[i].post_original ||
            probe_rows[i].vtable[4 / 4] != probe_rows[i].delete_original ||
            probe_rows[i].vtable[0xe0 / 4] != probe_rows[i].error_original) fs98_fail("vtable hook restoration");
    }
    memset(probe_nc, 0, sizeof(probe_nc)); memset(probe_post, 0, sizeof(probe_post));
    memset(probe_delete_count, 0, sizeof(probe_delete_count));
    memset(probe_callback_counts, 0, sizeof(probe_callback_counts)); probe_sequence = 0;
    probe_display_error_count = 0;
}
static void probe_partial_create(HMODULE module, void *factory)
{
    struct { char *name; unsigned char guid[16]; uint32_t major, minor; } provider;
    uint8_t block[0xc0] = {0};
    char callsign[24] = "ProbePilot", session[32] = "ProbeSession";
    char pilots[8] = "8", observers[8] = "2";
    HWND *array;
    void **objects = (void **)((uint8_t *)module + 0xa0a0);
    void *failed_object = 0;
    unsigned i, host_walk = 0;
    memset(&provider, 0, sizeof(provider)); provider.name = "Probe Provider";
    *(void **)(block + 0x04) = (void *)probe_provider;
    *(void **)(block + 0x08) = (void *)probe_reset;
    *(void **)(block + 0x0c) = (void *)probe_accept;
    *(void **)(block + 0x10) = (void *)probe_validate;
    *(uint32_t *)(block + 0x1c) = 1;
    *(char **)(block + 0x2c) = callsign; *(uint32_t *)(block + 0x30) = sizeof(callsign);
    *(void **)(block + 0x44) = (void *)probe_host_options;
    *(char **)(block + 0x4c) = session; *(uint32_t *)(block + 0x50) = sizeof(session);
    *(char **)(block + 0x60) = pilots; *(uint32_t *)(block + 0x64) = sizeof(pilots);
    *(char **)(block + 0x74) = observers; *(uint32_t *)(block + 0x78) = sizeof(observers);
    *(void **)(block + 0x84) = &provider; *(uint32_t *)(block + 0x88) = 1;
    *(void **)(block + 0x94) = (void *)probe_refresh_discovery;
    *(void **)(block + 0x98) = (void *)probe_players;
    *(void **)(block + 0x9c) = (void *)probe_details;
    *(void **)(block + 0xa0) = (void *)probe_refresh_all;
    *(void **)(block + 0xa4) = (void *)probe_browser_options;
    probe_block = block;
    printf("partial-failure fixture template=%04x descriptor=%p +1c=%u mode=%u\n",
           probe_fail_create_id, block, *(uint32_t *)(block + 0x1c), *(uint32_t *)(block + 0x24));
    array = (HWND *)(uintptr_t)((uint32_t (SC *)(uint32_t, void *))factory)
        ((uint32_t)(uintptr_t)probe_parent, block);
    printf("partial-failure factory returned array=%p create_calls=%u\n", array, probe_create_calls);
    for (i = 0; i < 5; ++i) {
        void *object = objects[i];
        HWND hwnd = array[i];
        printf(" partial[%u] array_hwnd=%p valid=%u object=%p object_hwnd=%p",
               i, hwnd, hwnd ? IsWindow(hwnd) : 0, object,
               object ? *(void **)((uint8_t *)object + 0x20) : 0);
        if (!object || !*(void **)((uint8_t *)object + 0x20)) failed_object = object;
        if (hwnd) printf(" style=%08lx effective_visible=%u",
                         (unsigned long)GetWindowLongA(hwnd, GWL_STYLE), IsWindowVisible(hwnd));
        printf("\n");
    }
    while (host_walk < 5 && array[host_walk]) ++host_walk;
    printf(" modeled HOST null-terminated walk stops at array[%u]; later entries still-present=%u\n",
           host_walk, host_walk < 5 && array[host_walk + (host_walk < 4)] != 0);
    for (i = 0; i < 5; ++i) if (array[i] && IsWindow(array[i])) DestroyWindow(array[i]);
    if (failed_object) {
        void **vtable = *(void ***)failed_object;
        printf(" probe-only cleanup of object without HWND object=%p deleting=%p\n",
               failed_object, vtable[1]);
        ((void *(TC *)(void *, uint32_t))vtable[1])(failed_object, 1);
    }
    fs98_pump();
}
int main(int argc, char **argv)
{
    HMODULE global, retail; struct fs98_test_linkage *linkage; unsigned i;
    int no_dialog = 0, no_fll = 0, no_string = 0, no_dll_directory = 0, queue_only = 0;
    setvbuf(stdout, 0, _IONBF, 0);
    for (i = 1; i < (unsigned)argc; ++i) {
        if (!strcmp(argv[i], "--no-dialog")) no_dialog = 1;
        if (!strcmp(argv[i], "--no-fll")) no_fll = 1;
        if (!strcmp(argv[i], "--no-string")) no_string = 1;
        if (!strcmp(argv[i], "--no-dll-directory")) no_dll_directory = 1;
        if (!strcmp(argv[i], "--queue-only")) queue_only = 1;
        if (!strcmp(argv[i], "--validation-field") && i + 1 < (unsigned)argc)
            probe_validation_field = strtoul(argv[++i], 0, 0);
        if (!strcmp(argv[i], "--fail-create") && i + 1 < (unsigned)argc)
            probe_fail_create_id = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--fail-allocate") && i + 1 < (unsigned)argc)
            probe_fail_allocate_size = strtoul(argv[++i], 0, 0);
    }
    SetUnhandledExceptionFilter(probe_fault_filter);
    if (!no_dll_directory) {
        char orig[MAX_PATH];
        if (!GetFullPathNameA("orig", MAX_PATH, orig, 0) || !SetDllDirectoryA(orig)) {
            fprintf(stderr, "SetDllDirectoryA(orig) failed error=%lu\n", (unsigned long)GetLastError());
            return 2;
        }
    }
    s_mfc=(HMODULE)fs98_load("cd\\mfc42.dll"); fs98_application(s_mfc);
    probe_parent = CreateWindowExA(0, "STATIC", "MP_DIALOG 0x110a parent", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                   0, 0, 640, 480, 0, 0, GetModuleHandleA(0), 0);
    if (!probe_parent) fs98_fail("probe parent creation");
    s_ui=(HMODULE)fs98_load("orig\\SIMSUI.DLL");
    probe_fll = no_fll ? 0 : (HMODULE)fs98_load("orig\\ENGLISH.FLL");
    probe_dialog = no_dialog ? 0 : (HMODULE)fs98_load("orig\\DIALOG.DLL");
    global=(HMODULE)fs98_load("orig\\Modules\\GLOBAL.DLL");
    fprintf(stderr, "deps MFC=%p SIMSUI=%p ENGLISH=%p DIALOG=%p GLOBAL=%p\n", s_mfc, s_ui, probe_fll, probe_dialog, global);
    memcpy(s_global,GetProcAddress(global,"Linkage"),4764);
    for(i=0;i<sizeof(s_host_services);i+=4) *(void **)(s_host_services+i)=(void *)fs98_host_unexpected;
    *(void **)(s_host_services+0x298)=(void *)fs98_host_register; *(void **)(s_host_services+0x29c)=(void *)fs98_host_release;
    *(void **)(s_host_services+0x10c)=(void *)fs98_host_dialog; *(void **)(s_host_services+0x20c)=(void *)fs98_host_allocate;
    *(void **)(s_host_services+0x20c)=(void *)probe_allocate;
    *(void **)(s_host_services+0x210)=(void *)fs98_host_reallocate; *(void **)(s_host_services+0x214)=(void *)fs98_host_free;
    if (!no_string) *(void **)(s_host_services+0x2a0)=(void *)probe_string;
    probe_bind_imports(s_ui);
    retail=(HMODULE)fs98_load("orig\\English\\mpdialog.dll"); probe_bind_imports(retail);
    linkage=(void *)GetProcAddress(retail,"Linkage"); if(!linkage)fs98_fail("retail Linkage");
    s_current_module=retail; linkage->init();
    probe_module=retail;
    probe_patch((void **)((uint8_t *)retail + 0xb4ec), (void *)probe_post_api, (void **)&probe_post_original);
    probe_patch((void **)((uint8_t *)retail + 0xb3cc), (void *)probe_mfc_create,
                (void **)&probe_mfc_create_original);
    probe_call_hook=SetWindowsHookExA(WH_CALLWNDPROC, probe_callwndproc, 0, GetCurrentThreadId());
    probe_cbt_hook=SetWindowsHookExA(WH_CBT, probe_cbt, 0, GetCurrentThreadId());
    if (!probe_call_hook || !probe_cbt_hook) fs98_fail("install retail observation hooks");
    if (probe_fail_allocate_size) {
        probe_one(retail, (void *)linkage->dialogs[0].create, 1, 0);
    } else if (probe_fail_create_id) {
        probe_partial_create(retail, (void *)linkage->dialogs[0].create);
    } else if (queue_only) {
        probe_one(retail, (void *)linkage->dialogs[0].create, 0, 4);
        probe_one(retail, (void *)linkage->dialogs[0].create, 0, 5);
    } else if (probe_validation_field) {
        if (probe_validation_field > 4) fs98_fail("validation field must be 1..4");
        probe_one(retail, (void *)linkage->dialogs[0].create, 1,
                  probe_validation_field == 1 ? 6 : 2);
    } else {
        probe_one(retail, (void *)linkage->dialogs[0].create, 1, 0);
        probe_one(retail, (void *)linkage->dialogs[0].create, 1, 1);
        probe_one(retail, (void *)linkage->dialogs[0].create, 1, 2);
        probe_one(retail, (void *)linkage->dialogs[0].create, 1, 3);
        probe_one(retail, (void *)linkage->dialogs[0].create, 0, 4);
        probe_one(retail, (void *)linkage->dialogs[0].create, 0, 5);
    }
    UnhookWindowsHookEx(probe_call_hook);
    UnhookWindowsHookEx(probe_cbt_hook);
    probe_restore_slot((void **)((uint8_t *)retail + 0xb3cc), (void *)probe_mfc_create_original);
    probe_restore_slot((void **)((uint8_t *)retail + 0xb4ec), (void *)probe_post_original);
    linkage->deinit();
    return 0;
}
