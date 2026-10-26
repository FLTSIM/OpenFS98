/* Partial World native comparison: actual MFC/SIMSUI controls and GDI pixels. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/world/world_internal.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define PTR WORLD_PTR
#define U32 WORLD_U32
struct world_ui_import { uint32_t id; void *link; };
struct world_ui_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
struct world_ui_linkage { uint32_t id; void (*init)(void), (*deinit)(void); void *reserved; uint32_t flags; struct world_ui_dialog *dialogs; };
static uint8_t s_host_services[0x340], s_global_data[0x1400], s_application[0x400];
static HMODULE s_mfc, s_fll, s_current, s_alerts;
static struct world_ui_linkage *s_alert_linkage;
static int s_auxiliary, s_force_path_error;
static uint32_t s_side, s_case, s_callbacks, s_length, s_log_length, s_checks;
static uint8_t s_descriptor[0x30], s_row[0x40];
static char s_path[160], s_title[160], s_snapshots[2][8192], s_logs[2][8192];
static uint8_t *s_pixels[2];
static uint32_t s_pixel_lengths[2], s_pixel_widths[2], s_pixel_heights[2];
static uint32_t s_registrations[2], s_releases[2];
/* Module initialization uses actual Windows unwinding through both modules. */
#define PROBE_THROW 0xe0429814
static int s_probe;
static uint32_t s_probe_case, s_probe_length, s_probe_seh;
static char s_probe_logs[2][64];
static uint8_t s_probe_dynamic[0x40];
static void *s_probe_jump[5];
static void world_probe_put(char event)
{
    if (s_probe_length >= 63) exit(2);
    s_probe_logs[s_side][s_probe_length++] = event;
    s_probe_logs[s_side][s_probe_length] = 0;
}
static void *world_probe_allocate(uint32_t size)
{
    if (size != 0x40) exit(2);
    world_probe_put('A');
    if (s_probe_case == 2) RaiseException(PROBE_THROW, 0, 0, NULL);
    return s_probe_case == 1 ? NULL : s_probe_dynamic;
}
static void *TC world_probe_construct(void *object, void *extension, int system)
{
    if (object != s_probe_dynamic || !extension || system) exit(2);
    world_probe_put('C');
    if (s_probe_case == 3) RaiseException(PROBE_THROW, 0, 0, NULL);
    return object;
}
static void world_probe_delete(void *object)
{ if (object != s_probe_dynamic) exit(2); world_probe_put('D'); }
static EXCEPTION_DISPOSITION __cdecl world_probe_catch(EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == PROBE_THROW && !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, NULL, record, NULL); world_probe_put('X');
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_probe_seh) : "memory");
        __builtin_longjmp(s_probe_jump, 1);
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) void world_probe_run(struct world_ui_linkage *linkage)
{
    struct {void *previous, *handler;} catcher;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_probe_seh));
    if (__builtin_setjmp(s_probe_jump) == 0) {
        catcher.previous = (void *)s_probe_seh; catcher.handler = world_probe_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        linkage->init(); world_probe_put('N');
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_probe_seh) : "memory");
    linkage->deinit(); linkage->deinit();
}
static void world_module_probe(struct world_ui_linkage **linkages, HMODULE *modules)
{
    const uint32_t ordinals[] = {823, 342, 825};
    void *targets[] = {world_probe_allocate, world_probe_construct, world_probe_delete};
    uint8_t *entries[3], saved[3][5];
    DWORD old; uint32_t index; int differ = 0;
    static const char *s_expected[] = {"RACNL", "RANL", "RAXL", "RACDXL"};
    for (index = 0; index < 3; ++index) {
        entries[index] = (void *)GetProcAddress(s_mfc, (const char *)ordinals[index]);
        if (!entries[index] || !VirtualProtect(entries[index], 5, PAGE_EXECUTE_READWRITE, &old)) exit(2);
        memcpy(saved[index], entries[index], 5); entries[index][0] = 0xe9;
        *(uint32_t *)(entries[index] + 1) = (uint32_t)targets[index] - (uint32_t)entries[index] - 5;
        FlushInstructionCache(GetCurrentProcess(), entries[index], 5); VirtualProtect(entries[index], 5, old, &old);
    }
    s_probe = 1;
    for (s_probe_case = 0; s_probe_case < 4; ++s_probe_case) {
        for (s_side = 0; s_side < 2; ++s_side) {
            s_current = modules[s_side]; s_probe_length = 0; s_probe_logs[s_side][0] = 0;
            world_probe_run(linkages[s_side]);
        }
        if (strcmp(s_probe_logs[0], s_probe_logs[1]) || strcmp(s_probe_logs[0], s_expected[s_probe_case])) {
            fprintf(stderr, "WORLD INIT DIFFER case=%u retail=%s ours=%s expected=%s\n", s_probe_case,
                s_probe_logs[0], s_probe_logs[1], s_expected[s_probe_case]); differ = 1; break;
        }
    }
    s_probe = 0;
    for (index = 0; index < 3; ++index) {
        VirtualProtect(entries[index], 5, PAGE_EXECUTE_READWRITE, &old);
        memcpy(entries[index], saved[index], 5);
        FlushInstructionCache(GetCurrentProcess(), entries[index], 5); VirtualProtect(entries[index], 5, old, &old);
    }
    if (differ) exit(1);
    printf("World native module init: 4 allocation/throw/double-deinit cases, 0 differ\n");
}

static HMODULE world_load(const char *path)
{
    char path_buffer[MAX_PATH]; HMODULE module;
    GetFullPathNameA(path, MAX_PATH, path_buffer, NULL); module = LoadLibraryA(path_buffer);
    if (!module) { fprintf(stderr, "LoadLibrary %s error=%lu\n", path, GetLastError()); exit(2); }
    return module;
}
static LONG WINAPI world_fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "World UI FAULT side=%u case=%u code=%lx pc=%p\n", s_side, s_case,
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(3); return EXCEPTION_EXECUTE_HANDLER;
}
static void world_unknown(void)
{ fprintf(stderr, "World unknown host call from %p\n", __builtin_return_address(0)); ExitProcess(4); }
static int SC world_allocate(int flags, uint32_t size, void **out, int zero)
{ (void)flags; *out = zero ? calloc(1, size) : malloc(size); return *out ? 0 : -1; }
static int SC world_free(void **out) { free(*out); *out = NULL; return 0; }
static int SC world_reallocate(int flags, uint32_t size, void **out)
{ (void)flags; *out = realloc(*out, size); return *out ? 0 : -1; }
static void SC world_register(void *linkage, HMODULE *instance)
{
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)linkage, instance);
    if (!s_auxiliary) ++s_registrations[s_side];
    if (s_probe) world_probe_put('R');
}
static void SC world_release(uint32_t module)
{ if (module != (uint32_t)s_current) exit(2); if (!s_auxiliary) ++s_releases[s_side]; if (s_probe) world_probe_put('L'); }
static int SC world_string(uint32_t id, char *buffer, int capacity)
{
    int length = LoadStringA(s_fll, id, buffer, capacity);
    if (!length) length = LoadStringA(s_current, id, buffer, capacity);
    if (!length) buffer[0] = 0;
    return length;
}
static void *world_object(HWND window)
{ return ((void *(SC *)(HWND))(void *)GetProcAddress(s_mfc, (const char *)2864))(window); }
static void world_application(void)
{
    HWND main = CreateWindowExA(0, "STATIC", "World harness", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);
    ((void *(TC *)(void *, const char *))(void *)GetProcAddress(s_mfc, (const char *)561))(s_application, NULL);
    ((int (SC *)(HINSTANCE, HINSTANCE, char *, int))(void *)GetProcAddress(s_mfc, (const char *)1575))(GetModuleHandleA(NULL), NULL, GetCommandLineA(), 0);
    PTR(s_application, 0x20) = world_object(main);
}
static void world_classes(void)
{
    WNDCLASSA window_class; HMODULE common = LoadLibraryA("comctl32.dll");
    ((void (WINAPI *)(void))(void *)GetProcAddress(common, "InitCommonControls"))();
    memset(&window_class, 0, sizeof(window_class)); window_class.style = CS_GLOBALCLASS;
    window_class.lpfnWndProc = DefWindowProcA; window_class.hInstance = GetModuleHandleA(NULL);
    window_class.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1); window_class.lpszClassName = "FS98DLG3D";
    if (!RegisterClassA(&window_class)) exit(2);
}
static void world_pump(void)
{
    MSG message; uint32_t count = 0;
    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE) && ++count < 1000) {
        TranslateMessage(&message); DispatchMessageA(&message);
    }
}
static uint32_t SC world_check(void *descriptor)
{
    void *row = PTR(descriptor, 0xc); const char *path = PTR(row, 4);
    const char *title = PTR(row, 0x18); int length;
    ++s_callbacks;
    U32(row, 0) = s_force_path_error || !*path || strstr(path, "invalid") ? 0x1007 : 0;
    U32(row, 0x14) = !*title ? 1 : 0;
    U32(descriptor, 0x10) = U32(descriptor, 0x14) < 1 ? 5 : U32(descriptor, 0x14) > U32(descriptor, 0x2c) ? 6 : 0;
    length = snprintf(s_logs[s_side] + s_log_length, 8192 - s_log_length,
        "check path=[%s] title=[%s] layer=%u active=%u position=%u statuses=%u/%u/%u\n",
        path, title, U32(row, 0x2c), U32(row, 0x34), U32(descriptor, 0x14),
        U32(row, 0), U32(row, 0x14), U32(descriptor, 0x10));
    if (length < 0 || (uint32_t)length >= 8192 - s_log_length) exit(2);
    s_log_length += (uint32_t)length;
    return U32(row, 0) || U32(row, 0x14) || U32(descriptor, 0x10) ? 0xffffffff : 0;
}
static void world_fixture(void)
{
    memset(s_descriptor, 0, sizeof(s_descriptor)); memset(s_row, 0, sizeof(s_row));
    memset(s_path, 0, sizeof(s_path)); memset(s_title, 0, sizeof(s_title));
    PTR(s_descriptor, 4) = world_check; PTR(s_descriptor, 8) = "Fixture scenery area";
    PTR(s_descriptor, 0xc) = s_row; U32(s_descriptor, 0x14) = 2; U32(s_descriptor, 0x2c) = 12;
    PTR(s_row, 4) = s_path; U32(s_row, 8) = sizeof(s_path);
    PTR(s_row, 0x18) = s_title; U32(s_row, 0x1c) = sizeof(s_title);
    U32(s_row, 0x2c) = (s_case % 5 + 1) * 10;
    U32(s_row, 0x34) = s_case & 1; U32(s_row, 0x3c) = !!(s_case & 2);
    if (!(s_case & 4)) strcpy(s_path, "C:\\fixture\\scenery\\area.bgl");
    strcpy(s_title, "Fixture area"); s_callbacks = s_log_length = 0; s_logs[s_side][0] = 0;
}
static BOOL CALLBACK world_snapshot(HWND window, LPARAM l_param)
{
    char class_name[80], text_buffer[512]; RECT rect; int length;
    GetClassNameA(window, class_name, sizeof(class_name)); GetWindowTextA(window, text_buffer, sizeof(text_buffer));
    GetWindowRect(window, &rect); MapWindowPoints(NULL, (HWND)l_param, (POINT *)&rect, 2);
    length = snprintf(s_snapshots[s_side] + s_length, 8192 - s_length,
        "id=%x class=%s text=[%s] xy=%ld/%ld/%ld/%ld enabled=%d visible=%d limit=%ld\n",
        GetDlgCtrlID(window), class_name, text_buffer, rect.left, rect.top, rect.right, rect.bottom,
        IsWindowEnabled(window), IsWindowVisible(window),
        !strcmp(class_name, "Edit") ? (long)SendMessageA(window, EM_GETLIMITTEXT, 0, 0) : 0L);
    if (length < 0 || (uint32_t)length >= 8192 - s_length) exit(2);
    s_length += (uint32_t)length; return TRUE;
}
static void world_pixels(HWND window)
{
    RECT rect; BITMAPINFO info; HDC source_dc = GetDC(window), memory = CreateCompatibleDC(source_dc);
    HBITMAP bitmap, previous; void *bits; uint32_t length, index, painted = 0;
    GetClientRect(window, &rect); memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = rect.right;
    info.bmiHeader.biHeight = -rect.bottom; info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    if (rect.right <= 0 || rect.bottom <= 0 || rect.right > 2048 || rect.bottom > 2048) exit(2);
    length = (uint32_t)rect.right * rect.bottom * 4;
    bitmap = CreateDIBSection(source_dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !memory) exit(2);
    previous = SelectObject(memory, bitmap); memset(bits, 0xa5, length);
    BitBlt(memory, 0, 0, rect.right, rect.bottom, source_dc, 0, 0, SRCCOPY); GdiFlush();
    for (index = 0; index < length; ++index) if (((uint8_t *)bits)[index] != 0xa5) ++painted;
    if (!painted) exit(2);
    s_pixels[s_side] = realloc(s_pixels[s_side], length);
    if (!s_pixels[s_side]) exit(2);
    memcpy(s_pixels[s_side], bits, length); s_pixel_lengths[s_side] = length; s_pixel_widths[s_side] = (uint32_t)rect.right; s_pixel_heights[s_side] = (uint32_t)rect.bottom;
    SelectObject(memory, previous); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window, source_dc);
}
static HWND s_error_owner;
static UINT_PTR s_error_timer;
static uint32_t s_error_ticks, s_error_seen;
static char s_error_snapshots[2][8192];
static uint8_t *s_error_pixels[2];
static uint32_t s_error_lengths[2];
static BOOL CALLBACK world_error_window(HWND window, LPARAM l_param)
{
    char class_name[80]; uint32_t length;
    (void)l_param;
    GetClassNameA(window, class_name, sizeof(class_name));
    if (window == s_error_owner || !IsWindowVisible(window)) return TRUE;
    KillTimer(NULL, s_error_timer); s_error_timer = 0;
    SetForegroundWindow(window); RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    s_length = 0; s_snapshots[s_side][0] = 0;
    EnumChildWindows(window, world_snapshot, (LPARAM)window);
    strcpy(s_error_snapshots[s_side], s_snapshots[s_side]);
    world_pixels(window); length = s_pixel_lengths[s_side];
    s_error_pixels[s_side] = realloc(s_error_pixels[s_side], length);
    if (!s_error_pixels[s_side]) exit(2);
    memcpy(s_error_pixels[s_side], s_pixels[s_side], length); s_error_lengths[s_side] = length;
    ++s_error_seen; PostMessageA(window, WM_COMMAND, 0x7d3, (LPARAM)GetDlgItem(window, 0x7d3));
    return FALSE;
}
static void CALLBACK world_error_timer(HWND window, UINT message, UINT_PTR timer, DWORD time)
{
    (void)window; (void)message; (void)timer; (void)time;
    if (++s_error_ticks > 200) { fprintf(stderr, "World error dialog did not appear\n"); ExitProcess(2); }
    EnumThreadWindows(GetCurrentThreadId(), world_error_window, 0);
}
static uint32_t world_error_probe(HWND window, uint32_t step)
{
    uint32_t focus;
    HWND path_control = GetDlgItem(window, 0x1b05), title_control = GetDlgItem(window, 0x1b07), position_control = GetDlgItem(window, 0x1aef);
    SetWindowTextA(path_control, "C:\\fixture\\scenery\\valid.bgl");
    SetWindowTextA(title_control, step == 5 ? "" : "Error fixture");
    SetWindowTextA(position_control, step == 4 ? "9999" : "0003");
    s_error_owner = window; s_error_ticks = s_error_seen = 0;
    s_error_timer = SetTimer(NULL, 0, 10, world_error_timer);
    /* A path can become invalid after editing (e.g. files removed). This
     * keeps the title enabled until OK, making its focus effect observable. */
    s_force_path_error = step == 3;
    if (step == 3) {
        void *object = world_object(window);
        void **vtable = PTR(object, 0);
        ((void (TC *)(void *))vtable[0xcc / 4])(object);
    } else SendMessageA(window, WM_COMMAND, IDOK, 0);
    if (s_error_timer) KillTimer(NULL, s_error_timer);
    if (s_error_seen != 1 || !IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA)) {
        fprintf(stderr, "World error/keep-open branch failed step=%u seen=%u\n", step, s_error_seen); exit(2);
    }
    s_force_path_error = 0;
    focus = (uint32_t)GetDlgCtrlID(GetFocus());
    if (!s_side && step == 3 && focus != 0x1b07) {
        fprintf(stderr, "retail focus witness missing: %x\n", focus); exit(2);
    }
    SetWindowTextA(path_control, "C:\\fixture\\scenery\\fixed.bgl"); SetWindowTextA(title_control, "Fixed title"); SetWindowTextA(position_control, "0003");
    ++s_checks;
    return focus;
}

static void world_save_pixels(const char *kind, uint32_t cycle, uint32_t step)
{
    char path_buffer[MAX_PATH]; BITMAPFILEHEADER file_header; BITMAPINFOHEADER info; FILE *file;
    CreateDirectoryA("bin/world-diff/ui-images", NULL);
    snprintf(path_buffer, sizeof(path_buffer), "bin/world-diff/ui-images/%s-cycle%u-case%u-step%u-side%u.bmp", kind, cycle, s_case, step, s_side);
    memset(&file_header, 0, sizeof(file_header)); memset(&info, 0, sizeof(info));
    file_header.bfType = 0x4d42; file_header.bfOffBits = sizeof(file_header) + sizeof(info); file_header.bfSize = file_header.bfOffBits + s_pixel_lengths[s_side];
    info.biSize = sizeof(info); info.biWidth = (LONG)s_pixel_widths[s_side]; info.biHeight = -(LONG)s_pixel_heights[s_side];
    info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
    file = fopen(path_buffer, "wb"); if (!file) exit(2);
    if (fwrite(&file_header, sizeof(file_header), 1, file) != 1 || fwrite(&info, sizeof(info), 1, file) != 1 ||
        fwrite(s_pixels[s_side], s_pixel_lengths[s_side], 1, file) != 1) exit(2);
    fclose(file);
}

static void world_ownership(HWND window, HMODULE expected_module)
{
    void **vtable = PTR(world_object(window), 0); HMODULE owner;
    const uint32_t slots[] = {4, 0x30, 0x8c, 0xc4, 0xcc, 0xd0, 0xd8, 0xdc}; uint32_t index;
    for (index = 0; index < sizeof(slots) / sizeof(slots[0]); ++index) {
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (const char *)vtable[slots[index] / 4], &owner) || owner != expected_module) {
            fprintf(stderr, "World dispatch ownership failure slot=%x\n", slots[index]); exit(2);
        }
    }
}
static uint32_t (SC *world_factory(struct world_ui_linkage *linkage, uint32_t id))(uint32_t, void *)
{
    uint32_t index;
    for (index = 0; linkage->dialogs[index].id; ++index) {
        if (index > 64) exit(2);
        if (linkage->dialogs[index].id == id) return linkage->dialogs[index].create;
    }
    exit(2);
}
static int SC world_modal(uint32_t id, void *descriptor)
{
    HMODULE previous = s_current;
    HWND window; DWORD started;
    if (id != 2) { fprintf(stderr, "unexpected nested dialog %x\n", id); exit(2); }
    s_current = s_alerts;
    EnableWindow(s_error_owner, FALSE);
    window = (HWND)world_factory(s_alert_linkage, 2)(0, descriptor);
    started = GetTickCount();
    while (IsWindow(window) && !GetWindowLongA(window, GWL_USERDATA)) {
        world_pump(); Sleep(1);
        if (GetTickCount() - started > 5000) { fprintf(stderr, "nested alert timeout\n"); exit(2); }
    }
    DestroyWindow(window); world_pump();
    EnableWindow(s_error_owner, TRUE); SetForegroundWindow(s_error_owner); SetActiveWindow(s_error_owner); SetFocus(s_error_owner);
    s_current = previous;
    return 0;
}

struct world_ui_result { uint8_t descriptor[0x30], row[0x40]; char path_buffer[160], title[160]; uint32_t callbacks, validate, closed, focus; };
int main(int argc, char **argv)
{
    HMODULE ui, global_module, modules[2]; struct world_ui_linkage *linkages[2];
    char directory[MAX_PATH]; uint32_t index, cycle, step, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], NULL, 0) : 24;
    struct world_ui_result results[2];
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, NULL, _IONBF, 0); SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(world_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, NULL); SetDllDirectoryA(directory);
    s_mfc = world_load("cd/mfc42.dll"); world_application(); world_classes();
    ui = world_load("orig/SIMSUI.DLL"); s_fll = world_load("orig/ENGLISH.FLL");
    global_module = world_load("orig/Modules/GLOBAL.DLL"); memcpy(s_global_data, GetProcAddress(global_module, "Linkage"), 4764);
    for (index = 0; index < sizeof(s_host_services); index += 4) PTR(s_host_services, index) = world_unknown;
    PTR(s_host_services, 0x20c) = world_allocate; PTR(s_host_services, 0x210) = world_reallocate; PTR(s_host_services, 0x214) = world_free;
    PTR(s_host_services, 0x10c) = world_modal; PTR(s_host_services, 0x298) = world_register; PTR(s_host_services, 0x29c) = world_release; PTR(s_host_services, 0x2a0) = world_string;
    {
        struct world_ui_import *imports = (void *)GetProcAddress(ui, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global_data;
    }
    s_auxiliary = 1; s_alerts = world_load("orig/English/Alerts.DLL"); s_current = s_alerts;
    {
        struct world_ui_import *imports = (void *)GetProcAddress(s_alerts, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global_data;
        s_alert_linkage = (void *)GetProcAddress(s_alerts, "Linkage"); s_alert_linkage->init();
    }
    s_auxiliary = 0;
    for (cycle = 0; cycle < 2; ++cycle) {
        modules[0] = world_load("orig/English/World.DLL");
        modules[1] = world_load(argc > 1 ? argv[1] : "bin/world-diff/WORLD_area.dll");
        if (modules[0] == modules[1]) return 2;
        for (s_side = 0; s_side < 2; ++s_side) {
            struct world_ui_import *imports = (void *)GetProcAddress(modules[s_side], "ImportTable");
            imports[0].link = s_host_services; imports[1].link = s_global_data;
            linkages[s_side] = (void *)GetProcAddress(modules[s_side], "Linkage");
            if (linkages[s_side]->id != 0x80000006) return 2;
        }
        world_module_probe(linkages, modules);
        for (s_side = 0; s_side < 2; ++s_side) {
            s_current = modules[s_side]; linkages[s_side]->init();
        }
        for (s_case = 0; s_case < cases; ++s_case) for (step = 0; step < 6; ++step) {
            for (s_side = 0; s_side < 2; ++s_side) {
                HWND window; void *object; void **vtable; uint32_t validate = 0, focus = 0;
                uint32_t dialog_id = s_case & 1 ? 0x13080003 : 0x13080004;
                s_current = modules[s_side]; world_fixture();
                s_error_snapshots[s_side][0] = 0; s_error_lengths[s_side] = 0;
                window = (HWND)world_factory(linkages[s_side], dialog_id)(0, s_descriptor);
                if (!IsWindow(window)) return 2;
                world_pump(); world_ownership(window, modules[s_side]);
                object = world_object(window); vtable = PTR(object, 0);
                if (step >= 1) {
                    SetWindowTextA(GetDlgItem(window, 0x1b05), "C:\\fixture\\scenery\\edited.bgl");
                    SetWindowTextA(GetDlgItem(window, 0x1b07), "Edited area");
                    SetWindowTextA(GetDlgItem(window, 0x1aef), "0003");
                    SendMessageA(GetDlgItem(window, 0x1ad8), CB_SETCURSEL, (s_case + 1) % 5, 0);
                    SendMessageA(window, WM_COMMAND, MAKEWPARAM(0x1b05, 0x400), (LPARAM)GetDlgItem(window, 0x1b05));
                    if (step == 2) {
                        SetWindowTextA(GetDlgItem(window, 0x1b07), "");
                        validate = ((int (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
                        if (validate != 0x1b07) { fprintf(stderr, "World validation branch missing\n"); return 2; }
                        SetWindowTextA(GetDlgItem(window, 0x1b07), "Valid title"); ++s_checks;
                    }
                }
                if (step >= 3) { focus = world_error_probe(window, step); }
                s_length = 0; s_snapshots[s_side][0] = 0;
                EnumChildWindows(window, world_snapshot, (LPARAM)window);
                BringWindowToTop(window); SetForegroundWindow(window); SetFocus(NULL);
                RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                world_pump(); world_pixels(window);
                if (cycle == 0 && (s_case == 0 || s_case == 5) && step == 0)
                    world_save_pixels("baseline", cycle, step);
                {
                    MSG key;
                    memset(&key, 0, sizeof(key)); key.hwnd = window; key.message = WM_KEYDOWN;
                    key.wParam = step >= 1 && !(s_case & 1) ? VK_RETURN : VK_ESCAPE; key.lParam = 1;
                    IsDialogMessageA(window, &key);
                }
                if (!GetWindowLongA(window, GWL_USERDATA)) SendMessageA(window, WM_COMMAND, step >= 1 && !(s_case & 1) ? IDOK : IDCANCEL, 0);
                if (!IsWindow(window) || IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA) != 1) {
                    fprintf(stderr, "World close state invalid\n"); return 2;
                }
                memset(&results[s_side], 0, sizeof(results[0]));
                memcpy(results[s_side].descriptor, s_descriptor, sizeof(s_descriptor));
                PTR(results[s_side].descriptor, 0x1c) = NULL; /* expired error-buffer address */
                memcpy(results[s_side].row, s_row, sizeof(s_row));
                memcpy(results[s_side].path_buffer, s_path, sizeof(s_path)); memcpy(results[s_side].title, s_title, sizeof(s_title));
                results[s_side].callbacks = s_callbacks; results[s_side].validate = validate;
                results[s_side].closed = 1; results[s_side].focus = focus;
                DestroyWindow(window); world_pump();
                /* Area PostNcDestroy is empty; its caller owns deletion. */
                ((void *(TC *)(void *, uint32_t))vtable[1])(object, 1);
            }
            if (memcmp(&results[0], &results[1], sizeof(results[0])) || strcmp(s_snapshots[0], s_snapshots[1]) ||
                strcmp(s_logs[0], s_logs[1]) || s_pixel_lengths[0] != s_pixel_lengths[1] ||
                memcmp(s_pixels[0], s_pixels[1], s_pixel_lengths[0]) ||
                strcmp(s_error_snapshots[0], s_error_snapshots[1]) || s_error_lengths[0] != s_error_lengths[1] ||
                (s_error_lengths[0] && memcmp(s_error_pixels[0], s_error_pixels[1], s_error_lengths[0]))) {
                for (s_side = 0; s_side < 2; ++s_side) world_save_pixels("difference", cycle, step);
                fprintf(stderr, "WORLD UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d pixels=%u/%u\nRETAIL:\n%s%sOURS:\n%s%s",
                    cycle, s_case, step, !!memcmp(&results[0], &results[1], sizeof(results[0])),
                    !!strcmp(s_snapshots[0], s_snapshots[1]), !!strcmp(s_logs[0], s_logs[1]),
                    s_pixel_lengths[0], s_pixel_lengths[1], s_snapshots[0], s_logs[0], s_snapshots[1], s_logs[1]);
                return 1;
            }
            ++comparisons;
        }
        for (s_side = 0; s_side < 2; ++s_side) {
            s_current = modules[s_side]; linkages[s_side]->deinit(); linkages[s_side]->deinit();
            if (s_registrations[s_side] != (cycle + 1) * 5 || s_releases[s_side] != (cycle + 1) * 5) {
                fprintf(stderr, "World module lifetime mismatch\n"); return 1;
            }
            if (!FreeLibrary(modules[s_side])) return 2;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (const char *)modules[s_side], &s_current)) return 2;
        }
    }
    s_auxiliary = 1; s_current = s_alerts; s_alert_linkage->deinit(); FreeLibrary(s_alerts);
    printf("World native area: %u paired pixel/state/callback comparisons, 2 unload cycles, %u validation checks, 0 differ\n", comparisons, s_checks);
    return 0;
}
