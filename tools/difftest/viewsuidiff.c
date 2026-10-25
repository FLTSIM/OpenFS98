/* Public factory/message smoke comparison with the CD's actual MFC/SIMSUI.
 * The host boundary is small: memory, module stack and string resources.
 * This is deliberately separate from the synthetic high-volume suites. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/views/views_objects.h"
#include "../../src/views/views_unwind.h"

#define SC FS98_VIEWS_STDCALL
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
struct fs98_ui_import { uint32_t id; void *link; };
struct fs98_ui_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
struct fs98_ui_linkage {
    uint32_t id; void (*init)(void), (*deinit)(void); void *reserved;
    uint32_t flags; struct fs98_ui_dialog *dialogs;
};
static uint8_t s_host_services[0x340], s_global[0x1400];
static HMODULE s_fll, s_current_module;
static uint32_t s_side, s_dialog;
static uint32_t s_descriptor_words[10];
static char s_snapshot[2][8192];
static uint8_t s_records[4][0xb4];
static char s_buffers[4][7][64];
static HWND s_preview_host[2], s_preview_child[2];
static char s_callback_log[2][4096];
static uint32_t s_callback_length;
static uint32_t s_snapshot_length;
static uint8_t s_application[0x400];
static HMODULE s_mfc;
static uint8_t *s_pixels[2];
static uint32_t s_pixel_lengths[2];
static int s_init_probe;
static uint32_t s_init_case, s_init_length;
static char s_init_log[2][64];
static uint8_t s_dynamic[0x40];
static void *s_init_jump[5];
static struct fs98_views_unwind *s_init_previous;
#define INIT_THROW 0x0e041a12

static void fs98_init_put(char event)
{
    if (s_init_length >= sizeof(s_init_log[0]) - 1) exit(2);
    s_init_log[s_side][s_init_length++] = event;
    s_init_log[s_side][s_init_length] = 0;
}

static void *fs98_init_allocate(uint32_t size)
{
    if (size != sizeof(s_dynamic)) exit(2);
    fs98_init_put('A');
    if (s_init_case == 2) RaiseException(INIT_THROW, 0, 0, 0);
    return s_init_case == 1 ? 0 : s_dynamic;
}

static void *FS98_VIEWS_THISCALL fs98_init_construct(void *self, void *extension, int32_t system)
{
    if (self != s_dynamic || !extension || system) exit(2);
    fs98_init_put('C');
    if (s_init_case == 3) RaiseException(INIT_THROW, 0, 0, 0);
    return self;
}

static void fs98_init_delete(void *self)
{
    if (self != s_dynamic) exit(2);
    fs98_init_put('D');
}

static EXCEPTION_DISPOSITION __cdecl fs98_init_catch(
    EXCEPTION_RECORD *record, void *establisher, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == INIT_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(establisher, 0, record, 0);
        fs98_init_put('X');
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_previous) : "memory");
        __builtin_longjmp(s_init_jump, 1);
    }
    return ExceptionContinueSearch;
}

static FS98_VIEWS_NOINLINE void fs98_init_run(struct fs98_ui_linkage *linkage)
{
    struct fs98_views_unwind catch;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_init_previous));
    if (!__builtin_setjmp(s_init_jump)) {
        catch.previous = s_init_previous;
        catch.handler = fs98_init_catch;
        catch.cleanup = 0;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catch) : "memory");
        linkage->init();
        fs98_init_put('N');
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_previous) : "memory");
    linkage->deinit();
    /* The second call must observe the cleared registration handle. */
    linkage->deinit();
}

/* Patch only this process's native MFC entry points, after DLL attachment.
 * GetProcAddress in our adapter and retail's IAT must see the same hooks. */
static void fs98_init_probe(struct fs98_ui_linkage **linkages, HMODULE *modules)
{
    const uint16_t ordinals[] = {823, 342, 825};
    void *targets[] = {fs98_init_allocate, fs98_init_construct, fs98_init_delete};
    uint8_t *entries[3], saved[3][5];
    uint32_t index;
    int differ = 0;
    DWORD previous;
    static const char *s_expected[] = {"RACNL", "RANL", "RAXL", "RACDXL"};
    for (index = 0; index < 3; index++) {
        entries[index] = (void *)GetProcAddress(s_mfc, (const char *)(uint32_t)ordinals[index]);
        if (!entries[index] || !VirtualProtect(entries[index], 5, PAGE_EXECUTE_READWRITE, &previous)) exit(2);
        memcpy(saved[index], entries[index], 5);
        entries[index][0] = 0xe9;
        *(uint32_t *)(entries[index] + 1) = (uint32_t)targets[index] - (uint32_t)entries[index] - 5;
        FlushInstructionCache(GetCurrentProcess(), entries[index], 5);
        VirtualProtect(entries[index], 5, previous, &previous);
    }
    s_init_probe = 1;
    for (s_init_case = 0; s_init_case < 4; s_init_case++) {
        for (s_side = 0; s_side < 2; s_side++) {
            s_current_module = modules[s_side];
            s_init_length = 0;
            s_init_log[s_side][0] = 0;
            fs98_init_run(linkages[s_side]);
        }
        if (strcmp(s_init_log[0], s_init_log[1]) ||
            strcmp(s_init_log[0], s_expected[s_init_case])) {
            fprintf(stderr, "INIT DIFFER case=%u retail=%s ours=%s expected=%s\n",
                     s_init_case, s_init_log[0], s_init_log[1], s_expected[s_init_case]);
            differ = 1;
            break;
        }
    }
    s_init_probe = 0;
    for (index = 0; index < 3; index++) {
        VirtualProtect(entries[index], 5, PAGE_EXECUTE_READWRITE, &previous);
        memcpy(entries[index], saved[index], 5);
        FlushInstructionCache(GetCurrentProcess(), entries[index], 5);
        VirtualProtect(entries[index], 5, previous, &previous);
    }
    /* Restore native allocators before CRT/process teardown on failure too. */
    if (differ) exit(1);
    printf("Views native module init: 4 allocation/throw/deinit cases, 0 differ\n");
}

static void fs98_check_dispatch(HWND window, HMODULE expected)
{
    void *from_handle = (void *)GetProcAddress(s_mfc, (const char *)2864);
    void *object = ((void *(SC *)(HWND))from_handle)(window);
    void **vtable = PTR(object, 0);
    uint32_t slots[] = {0x30, 0xc4, 4};
    uint32_t index;
    HMODULE owner = 0;
    for (index = 0; index < sizeof(slots) / sizeof(slots[0]); index++) {
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (const char *)vtable[slots[index] / 4], &owner);
        if (owner != expected) {
            fprintf(stderr, "wrong dispatch owner side=%u slot=%u owner=%p expected=%p\n",
                     s_side, slots[index], owner, expected);
            exit(2);
        }
    }
}

static void fs98_pixels(HWND window, int screen)
{
    RECT rect;
    BITMAPINFO info;
    HDC source = GetDC(window), memory = CreateCompatibleDC(source);
    HBITMAP bitmap, previous;
    void *bits;
    uint32_t length, index, painted = 0;
    GetClientRect(window, &rect);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = rect.right;
    info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    if (rect.right <= 0 || rect.bottom <= 0 || rect.right > 2048 || rect.bottom > 2048)
        exit(2);
    length = (uint32_t)rect.right * (uint32_t)rect.bottom * 4;
    bitmap = CreateDIBSection(source, &info, DIB_RGB_COLORS, &bits, 0, 0);
    if (!bitmap || !memory) exit(2);
    previous = SelectObject(memory, bitmap);
    memset(bits, 0xa5, length);
    if (screen)
        BitBlt(memory, 0, 0, rect.right, rect.bottom, source, 0, 0, SRCCOPY);
    else
        SendMessageA(window, WM_PRINT, (WPARAM)memory,
                      PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    GdiFlush();
    for (index = 0; index < length; index++)
        if (((uint8_t *)bits)[index] != 0xa5) painted++;
    if (!painted) { fprintf(stderr, "window image was not painted\n"); exit(2); }
    s_pixels[s_side] = realloc(s_pixels[s_side], length);
    if (!s_pixels[s_side]) exit(2);
    memcpy(s_pixels[s_side], bits, length);
    s_pixel_lengths[s_side] = length;
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(window, source);
}

/* The console harness needs the same application/main-window relationship
 * that MFC gets from the simulator. The CD's app ctor is ordinal 561 at
 * 0x5f40b308; its CWinThread::GetMainWnd (0x5f402a15) reads +0x24/+0x20.
 * Pin those callees instead of patching a failing UI dependency. */
static void fs98_application(HMODULE mfc)
{
    HWND main = CreateWindowExA(0, "STATIC", "OpenFS98 Views harness",
                                  WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0,
                                  GetModuleHandleA(0), 0);
    void *constructor = (void *)GetProcAddress(mfc, (const char *)561);
    void *win_init = (void *)GetProcAddress(mfc, (const char *)1575);
    void *from_handle = (void *)GetProcAddress(mfc, (const char *)2864);
    if (!main || !constructor || !win_init || !from_handle) exit(2);
    ((void *(FS98_VIEWS_THISCALL *)(void *, const char *))constructor)(s_application, 0);
    ((int32_t (SC *)(HINSTANCE, HINSTANCE, char *, int32_t))win_init)(
        GetModuleHandleA(0), 0, GetCommandLineA(), 0);
    PTR(s_application, 0x20) = ((void *(SC *)(HWND))from_handle)(main);
}

static LONG WINAPI fs98_fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "UI FAULT side=%u dialog=%u code=%08lx pc=%p\n", s_side,
             s_dialog, exception->ExceptionRecord->ExceptionCode,
             exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void fs98_unknown(void)
{
    fprintf(stderr, "unimplemented host call from %p side=%u dialog=%u\n",
             __builtin_return_address(0), s_side, s_dialog);
    ExitProcess(4);
}

static int32_t SC fs98_allocate(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    (void)flags;
    *out = zero ? calloc(1, size) : malloc(size);
    return *out ? 0 : -1;
}
static int32_t SC fs98_free(void **out) { free(*out); *out = 0; return 0; }
static int32_t SC fs98_reallocate(int32_t flags, uint32_t size, void **out)
{ (void)flags; *out = realloc(*out, size); return *out ? 0 : -1; }
static void SC fs98_register(void *linkage, HMODULE *instance)
{
    if (s_init_probe) fs98_init_put('R');
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (const char *)linkage, instance);
}
static void SC fs98_release(uint32_t module)
{ if (s_init_probe) fs98_init_put('L'); (void)module; }
static int32_t SC fs98_string(uint32_t id, char *buffer, int32_t capacity)
{
    int32_t length = LoadStringA(s_fll, id, buffer, capacity);
    if (!length) length = LoadStringA(s_current_module, id, buffer, capacity);
    if (!length) { buffer[0] = 0; }
    return length;
}
static int32_t SC fs98_handler(void *descriptor)
{
    uint8_t *record = (uint8_t *)PTR(descriptor, 0x1c) + ((uint32_t *)descriptor)[5] * 0xb4;
    static const uint32_t offsets[] = {0xc, 0x20, 0x34, 0x48, 0x5c, 0x7c, 0x90};
    uint32_t index;
    for (index = 0; index < 7; index++) {
        char *end;
        double value = strtod(PTR(record, offsets[index] + 4), &end);
        *(uint32_t *)(record + offsets[index]) = !*end && value > 0 ? 0 : 1;
    }
    return 0;
}

static HMODULE fs98_load(const char *path)
{
    char full_path[MAX_PATH];
    HMODULE module;
    GetFullPathNameA(path, MAX_PATH, full_path, 0);
    module = LoadLibraryA(full_path);
    if (!module) {
        fprintf(stderr, "LoadLibrary %s error=%lu\n", path, GetLastError());
        exit(2);
    }
    return module;
}

static BOOL CALLBACK fs98_snapshot(HWND window, LPARAM param)
{
    char class[128], text_buffer[512];
    RECT rect;
    int length;
    HWND root = (HWND)param;
    GetClassNameA(window, class, sizeof(class));
    GetWindowTextA(window, text_buffer, sizeof(text_buffer));
    GetWindowRect(window, &rect);
    MapWindowPoints(0, root, (POINT *)&rect, 2);
    if (s_snapshot_length > sizeof(s_snapshot[0]) - 1024) exit(2);
    length = snprintf(s_snapshot[s_side] + s_snapshot_length,
                        sizeof(s_snapshot[0]) - s_snapshot_length,
                        "id=%d class=%s text=%s rect=%ld,%ld,%ld,%ld visible=%d enabled=%d\n",
                        GetDlgCtrlID(window), class, text_buffer, rect.left, rect.top,
                        rect.right, rect.bottom, IsWindowVisible(window), IsWindowEnabled(window));
    s_snapshot_length += (uint32_t)length;
    return TRUE;
}

static void fs98_pump(void)
{
    MSG message;
    uint32_t count = 0;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE) && count++ < 1000) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

static int32_t SC fs98_preview(uint32_t operation, void *descriptor, uint32_t window)
{
    uint32_t selected = ((uint32_t *)descriptor)[5];
    uint8_t *record = (uint8_t *)PTR(descriptor, 0x1c) + selected * 0xb4;
    RECT rect;
    char text[128];
    int length;
    if (s_callback_length > sizeof(s_callback_log[0]) - 256) exit(2);
    length = snprintf(s_callback_log[s_side] + s_callback_length,
                       sizeof(s_callback_log[0]) - s_callback_length,
                       "preview operation=%u selected=%u mode=%u map=%u active=%d\n",
                       operation, selected, *(uint32_t *)(record + 8), *(uint32_t *)(record + 4),
                       IsWindow(s_preview_child[s_side]));
    s_callback_length += (uint32_t)length;
    if (operation == 1) {
        if (s_preview_child[s_side] || !IsWindow((HWND)window)) exit(2);
        s_preview_host[s_side] = (HWND)window;
        GetClientRect((HWND)window, &rect);
        s_preview_child[s_side] = CreateWindowExA(0, "STATIC", "Preview fixture", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                                 0, 0, rect.right, rect.bottom, (HWND)window,
                                                 (HMENU)0x7001, GetModuleHandleA(0), 0);
        if (!s_preview_child[s_side]) exit(2);
    } else if (operation == 2) {
        if (!IsWindow(s_preview_child[s_side])) exit(2);
        DestroyWindow(s_preview_child[s_side]);
        s_preview_child[s_side] = s_preview_host[s_side] = 0;
    } else if (operation == 3) {
        if (!IsWindow(s_preview_child[s_side])) exit(2);
        snprintf(text, sizeof(text), "Preview record %u / mode %u", selected, *(uint32_t *)(record + 8));
        SetWindowTextA(s_preview_child[s_side], text);
        ShowWindow(s_preview_child[s_side], *(uint32_t *)(record + 4) ? SW_HIDE : SW_SHOW);
    } else exit(2);
    return 0;
}
static void fs98_preview_class(void)
{
    WNDCLASSA window_class;
    HMODULE common = LoadLibraryA("comctl32.dll");
    void (WINAPI *initialize)(void) = (void *)GetProcAddress(common, "InitCommonControls");
    if (!initialize) exit(2);
    initialize();
    memset(&window_class, 0, sizeof(window_class));
    window_class.style = CS_GLOBALCLASS;
    window_class.lpfnWndProc = DefWindowProcA;
    window_class.hInstance = GetModuleHandleA(0);
    window_class.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    window_class.lpszClassName = "FS98DLG3D";
    if (!RegisterClassA(&window_class)) exit(2);
}
static void fs98_fixture(uint32_t case_index)
{
    static const uint32_t offsets[] = {0xc, 0x20, 0x34, 0x48, 0x5c, 0x7c, 0x90};
    static const char *names[] = {"First view", "Second view", "Map view", "Fourth view"};
    uint32_t record, field;
    memset(s_descriptor_words, 0, sizeof(s_descriptor_words));
    memset(s_records, 0, sizeof(s_records));
    memset(s_buffers, 0, sizeof(s_buffers));
    s_descriptor_words[1] = (uint32_t)fs98_handler;
    s_descriptor_words[2] = (uint32_t)fs98_preview;
    s_descriptor_words[3] = case_index & 1;
    s_descriptor_words[4] = case_index % 3;
    s_descriptor_words[5] = case_index % 4;
    s_descriptor_words[6] = 4;
    s_descriptor_words[7] = (uint32_t)s_records;
    s_descriptor_words[8] = case_index % 4;
    s_descriptor_words[9] = case_index & 1;
    for (record = 0; record < 4; record++) {
        PTR(s_records[record], 0) = (void *)names[record];
        *(uint32_t *)(s_records[record] + 4) = record == 2;
        *(uint32_t *)(s_records[record] + 8) = (case_index + record) % 5;
        for (field = 0; field < 7; field++) {
            uint32_t offset = offsets[field];
            PTR(s_records[record], offset + 4) = s_buffers[record][field];
            *(uint32_t *)(s_records[record] + offset + 8) = 64;
            snprintf(s_buffers[record][field], 64, field >= 5 ? "1000.0" : "00%u.234", record + 1);
        }
        *(uint32_t *)(s_records[record] + 0x70) = record & 1;
        *(uint32_t *)(s_records[record] + 0x74) = record % 3;
        *(uint32_t *)(s_records[record] + 0xa4) = record % 3;
        *(uint32_t *)(s_records[record] + 0xa8) = record & 1;
        *(uint32_t *)(s_records[record] + 0xac) = record;
    }
    s_callback_length = 0;
    s_callback_log[s_side][0] = 0;
}
static void *fs98_object(HWND window)
{ return ((void *(SC *)(HWND))(void *)GetProcAddress(s_mfc, (const char *)2864))(window); }
static void fs98_check(HWND window, uint32_t id, int32_t checked)
{
    HMODULE ui = GetModuleHandleA("SIMSUI.DLL");
    void *function = (void *)GetProcAddress(ui, "?SetChecked@CSimsCheckButton@@QAEHH@Z");
    HWND control = GetDlgItem(window, id);
    ((int32_t (FS98_VIEWS_THISCALL *)(void *, int32_t))function)(fs98_object(control), checked);
    SendMessageA(window, WM_COMMAND, id, (LPARAM)control);
}
static void fs98_combo(HWND window, uint32_t id, uint32_t selected)
{
    HWND control = GetDlgItem(window, id);
    SendMessageA(control, CB_SETCURSEL, selected, 0);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), (LPARAM)control);
}
int main(int argc, char **argv)
{
    HMODULE ui, modules[2], global;
    struct fs98_ui_linkage *linkages[2];
    char directory[MAX_PATH];
    uint32_t index, case_index, cycle, comparisons = 0;
    uint32_t cases = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 24;
    struct result { uint32_t descriptor[10]; uint8_t records[4][0xb4]; char buffers[4][7][64]; uint32_t answer, keyboard, invalid_control; } results[2];
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, 0, _IONBF, 0);
    SetUnhandledExceptionFilter(fs98_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, 0);
    SetDllDirectoryA(directory);
    s_mfc = fs98_load("cd\\mfc42.dll");
    fs98_application(s_mfc);
    fs98_preview_class();
    ui = fs98_load("orig\\SIMSUI.DLL");
    s_fll = fs98_load("orig\\ENGLISH.FLL");
    global = fs98_load("orig\\Modules\\GLOBAL.DLL");
    memcpy(s_global, GetProcAddress(global, "Linkage"), 4764);
    for (index = 0; index < sizeof(s_host_services); index += 4) PTR(s_host_services, index) = fs98_unknown;
    PTR(s_host_services, 0x20c) = fs98_allocate;
    PTR(s_host_services, 0x210) = fs98_reallocate;
    PTR(s_host_services, 0x214) = fs98_free;
    PTR(s_host_services, 0x298) = fs98_register;
    PTR(s_host_services, 0x29c) = fs98_release;
    PTR(s_host_services, 0x2a0) = fs98_string;
    {
        struct fs98_ui_import *imports = (void *)GetProcAddress(ui, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global;
    }
    for (cycle = 0; cycle < 2; cycle++) {
        modules[0] = fs98_load("orig\\English\\Views.DLL");
        modules[1] = fs98_load(argc > 1 ? argv[1] : "bin\\views-diff\\VIEWS_ours.dll");
        if (modules[0] == modules[1]) return 2;
        for (s_side = 0; s_side < 2; s_side++) {
            struct fs98_ui_import *imports = (void *)GetProcAddress(modules[s_side], "ImportTable");
            imports[0].link = s_host_services; imports[1].link = s_global;
            linkages[s_side] = (void *)GetProcAddress(modules[s_side], "Linkage");
            if (linkages[s_side]->id != 0x80000005 || linkages[s_side]->dialogs[0].id != 0x1508) return 2;
        }
        fs98_init_probe(linkages, modules);
        for (s_side = 0; s_side < 2; s_side++) { s_current_module = modules[s_side]; linkages[s_side]->init(); }
        for (case_index = 0; case_index < cases; case_index++) {
            for (s_side = 0; s_side < 2; s_side++) {
                HWND window;
                uint32_t command;
                MSG key;
                s_current_module = modules[s_side]; s_dialog = 0x1508;
                fs98_fixture(case_index);
                printf("Views UI cycle=%u case=%u side=%u create\n", cycle, case_index, s_side);
                window = (HWND)linkages[s_side]->dialogs[0].create(0, s_descriptor_words);
                if (!IsWindow(window)) { fprintf(stderr, "invalid Views HWND=%p error=%lu\n", window, GetLastError()); return 2; }
                fs98_pump();
                fs98_check_dispatch(window, modules[s_side]);
                fs98_combo(window, 0x1876, (case_index + 1) % 4);
                command = (uint32_t[]){0x1879, 0x187b, 0x187d, 0x187e, 0x187f, 0x1880}[case_index % 6];
                SendMessageA(window, WM_COMMAND, command, (LPARAM)GetDlgItem(window, command));
                fs98_combo(window, 0x1891, case_index % 3);
                fs98_combo(window, 0x1896, case_index % 3);
                fs98_combo(window, 0x1894, case_index % 3);
                fs98_combo(window, 0x1882, case_index % 3);
                fs98_check(window, 0x1887, !(case_index & 1));
                fs98_check(window, 0x1890, case_index & 1);
                fs98_check(window, 0x187c, !(case_index & 1));
                results[s_side].invalid_control = 0;
                if (command != 0x1880) {
                    void *object = fs98_object(window);
                    void **vtable = PTR(object, 0);
                    SetWindowTextA(GetDlgItem(window, 0x1886), "0.000");
                    results[s_side].invalid_control = (uint32_t)((int32_t (FS98_VIEWS_THISCALL *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
                    if (results[s_side].invalid_control != 0x1886) {
                        fprintf(stderr, "validation failure branch not reached\n"); return 2;
                    }
                    SetWindowTextA(GetDlgItem(window, 0x1886), "002.345");
                    SendMessageA(window, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 23), (LPARAM)GetDlgItem(window, 0x1897));
                }
                SendMessageA(window, 0x464, 0, 0);
                fs98_pump();
                s_snapshot_length = 0; s_snapshot[s_side][0] = 0;
                EnumChildWindows(window, fs98_snapshot, (LPARAM)window);
                BringWindowToTop(window);
                SetForegroundWindow(window);
                RedrawWindow(window, 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                fs98_pump();
                fs98_pixels(window, 1);
                memset(&key, 0, sizeof(key));
                key.hwnd = window; key.message = WM_KEYDOWN;
                key.wParam = case_index & 1 ? VK_ESCAPE : VK_RETURN; key.lParam = 1;
                IsDialogMessageA(window, &key);
                results[s_side].keyboard = (uint32_t)GetWindowLongA(window, GWL_USERDATA);
                if (!results[s_side].keyboard)
                    SendMessageA(window, WM_COMMAND, case_index & 1 ? IDCANCEL : IDOK, 0);
                if (!IsWindow(window) || IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA) != 1 ||
                    s_preview_child[s_side]) { fprintf(stderr, "Views close state invalid\n"); return 2; }
                memcpy(results[s_side].descriptor, s_descriptor_words, sizeof(s_descriptor_words));
                memcpy(results[s_side].records, s_records, sizeof(s_records));
                memcpy(results[s_side].buffers, s_buffers, sizeof(s_buffers));
                results[s_side].answer = s_descriptor_words[0];
                DestroyWindow(window);
                fs98_pump();
            }
            if (memcmp(&results[0], &results[1], sizeof(results[0])) || strcmp(s_snapshot[0], s_snapshot[1]) ||
                strcmp(s_callback_log[0], s_callback_log[1]) || s_pixel_lengths[0] != s_pixel_lengths[1] ||
                memcmp(s_pixels[0], s_pixels[1], s_pixel_lengths[0])) {
                fprintf(stderr, "VIEWS UI DIFFER cycle=%u case=%u\nRETAIL:\n%s%sOURS:\n%s%s\n",
                         cycle, case_index, s_callback_log[0], s_snapshot[0], s_callback_log[1], s_snapshot[1]);
                fprintf(stderr, "result differs=%d controls differ=%d callbacks differ=%d pixels=%u/%u\n",
                         !!memcmp(&results[0], &results[1], sizeof(results[0])),
                         !!strcmp(s_snapshot[0], s_snapshot[1]), !!strcmp(s_callback_log[0], s_callback_log[1]),
                         s_pixel_lengths[0], s_pixel_lengths[1]);
                if (s_pixel_lengths[0] == s_pixel_lengths[1]) {
                    uint32_t byte;
                    for (byte = 0; byte < s_pixel_lengths[0]; byte++)
                        if (s_pixels[0][byte] != s_pixels[1][byte]) {
                            fprintf(stderr, "first pixel difference byte=%u values=%u/%u\n", byte, s_pixels[0][byte], s_pixels[1][byte]);
                            break;
                        }
                }
                return 1;
            }
            comparisons++;
        }
        for (s_side = 0; s_side < 2; s_side++) {
            linkages[s_side]->deinit();
            if (!FreeLibrary(modules[s_side])) return 2;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (const char *)modules[s_side], &s_current_module)) return 2;
        }
    }
    printf("Views real MFC/SIMSUI: %u public-factory/message cases over 2 load/unload cycles, 0 differ\n", comparisons);
    return 0;
}
