/* Public factory/message smoke comparison with the CD's actual MFC/SIMSUI.
 * The host boundary is small: memory, module stack and string resources.
 * This is deliberately separate from the synthetic high-volume suites. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/alerts/alerts_objects.h"
#include "../../src/alerts/alerts_unwind.h"

#define SC FS98_ALERTS_STDCALL
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
static const char s_message[] = "OpenFS98 Alerts comparison\nSecond line";
static char s_snapshot[2][8192];
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
static struct fs98_alerts_unwind *s_init_previous;
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

static void *FS98_ALERTS_THISCALL fs98_init_construct(void *self, void *extension, int32_t system)
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

static FS98_ALERTS_NOINLINE void fs98_init_run(struct fs98_ui_linkage *linkage)
{
    struct fs98_alerts_unwind catch;
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
    printf("Alerts native module init: 4 allocation/throw/deinit cases, 0 differ\n");
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
    HWND main = CreateWindowExA(0, "STATIC", "OpenFS98 Alerts harness",
                                  WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0,
                                  GetModuleHandleA(0), 0);
    void *constructor = (void *)GetProcAddress(mfc, (const char *)561);
    void *win_init = (void *)GetProcAddress(mfc, (const char *)1575);
    void *from_handle = (void *)GetProcAddress(mfc, (const char *)2864);
    if (!main || !constructor || !win_init || !from_handle) exit(2);
    ((void *(FS98_ALERTS_THISCALL *)(void *, const char *))constructor)(s_application, 0);
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
static int32_t SC fs98_handler(void *descriptor) { (void)descriptor; return 0; }

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

int main(int argc, char **argv)
{
    HMODULE ui, modules[2], global;
    char directory[MAX_PATH];
    uint32_t slot, case_index, cycle, comparisons = 0;
    uint32_t cases = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 48;
    struct fs98_ui_linkage *linkages[2];
    uint32_t answers[2][10];
    uint32_t window_states[2][3];
    uint32_t final_states[2][3];
    static const int32_t s_current_cases[] = {0, 25, 100, 101, -1, 0x7fffffff};
    static const uint32_t s_tags[] = {0, 1, 2, 3, 4, 0xffffffff};
    setvbuf(stdout, 0, _IONBF, 0);
    SetUnhandledExceptionFilter(fs98_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, 0);
    SetDllDirectoryA(directory);
    s_mfc = fs98_load("cd\\mfc42.dll");
    fs98_application(s_mfc);
    ui = fs98_load("orig\\SIMSUI.DLL");
    s_fll = fs98_load("orig\\ENGLISH.FLL");
    global = fs98_load("orig\\Modules\\GLOBAL.DLL");
    memcpy(s_global, GetProcAddress(global, "Linkage"), 4764);
    for (slot = 0; slot < sizeof(s_host_services); slot += 4)
        PTR(s_host_services, slot) = fs98_unknown;
    PTR(s_host_services, 0x20c) = fs98_allocate;
    PTR(s_host_services, 0x210) = fs98_reallocate;
    PTR(s_host_services, 0x214) = fs98_free;
    PTR(s_host_services, 0x298) = fs98_register;
    PTR(s_host_services, 0x29c) = fs98_release;
    PTR(s_host_services, 0x2a0) = fs98_string;
    {
        struct fs98_ui_import *imports = (void *)GetProcAddress(ui, "ImportTable");
        imports[0].link = s_host_services;
        imports[1].link = s_global;
    }
    for (cycle = 0; cycle < 2; cycle++) {
        modules[0] = fs98_load("orig\\English\\Alerts.DLL");
        modules[1] = fs98_load(argc > 1 ? argv[1] : "bin\\alerts-diff\\ALERTS_ours.dll");
        for (s_side = 0; s_side < 2; s_side++) {
            struct fs98_ui_import *imports = (void *)GetProcAddress(modules[s_side], "ImportTable");
            imports[0].link = s_host_services; imports[1].link = s_global;
            linkages[s_side] = (void *)GetProcAddress(modules[s_side], "Linkage");
        }
        fs98_init_probe(linkages, modules);
        for (s_side = 0; s_side < 2; s_side++) {
            s_current_module = modules[s_side];
            linkages[s_side]->init();
            printf("UI module %u initialized at %p\n", s_side, s_current_module);
        }
        if (!cases || cases > 4096) return 2;
        for (case_index = 0; case_index < cases; case_index++) {
            for (s_side = 0; s_side < 2; s_side++) {
                struct fs98_ui_dialog *dialog = &linkages[s_side]->dialogs[case_index % 3];
                HWND window;
                uint32_t command;
                s_dialog = dialog->id;
                s_current_module = modules[s_side];
                memset(s_descriptor_words, 0, sizeof(s_descriptor_words));
                s_descriptor_words[1] = (uint32_t)fs98_handler;
                if (s_dialog == 2) {
                    s_descriptor_words[2] = (case_index / 3) % 8;
                    s_descriptor_words[3] = 0xdeadbeef;
                    s_descriptor_words[4] = s_tags[(case_index / 3) % 6];
                    s_descriptor_words[5] = case_index & 1 ? (uint32_t)s_message : 0xf6f0;
                } else if (s_dialog == 3) {
                    s_descriptor_words[2] = s_tags[(case_index / 3) % 6];
                    s_descriptor_words[3] = 1;
                    s_descriptor_words[4] = (case_index / 3) & 1;
                    s_descriptor_words[5] = (uint32_t)s_current_cases[(case_index / 3) % 6];
                    s_descriptor_words[6] = (case_index / 3) % 5 ? 100 : 0;
                    s_descriptor_words[8] = (case_index / 3) % 4 ? 100 : 0xffffffff;
                    s_descriptor_words[9] = (uint32_t)s_message;
                } else s_descriptor_words[4] = case_index & 1;
                printf("UI create side=%u id=%u case=%u\n", s_side, s_dialog, case_index);
                window = (HWND)dialog->create(0, s_descriptor_words);
                if (!IsWindow(window)) {
                    fprintf(stderr, "factory returned invalid HWND %p\n", window);
                    return 2;
                }
                fs98_pump();
                fs98_check_dispatch(window, modules[s_side]);
                s_snapshot_length = 0;
                s_snapshot[s_side][0] = 0;
                EnumChildWindows(window, fs98_snapshot, (LPARAM)window);
                if (s_dialog != 3) fs98_pixels(window, 0);
                if (s_dialog == 2) {
                    /* Exercise the same default key routing the host uses, then
                     * finish through a mapped button if the window remains. */
                    MSG key;
                    memset(&key, 0, sizeof(key));
                    key.hwnd = window;
                    key.message = WM_KEYDOWN;
                    key.wParam = (case_index / 3) & 1 ? VK_ESCAPE : VK_RETURN;
                    key.lParam = 1;
                    IsDialogMessageA(window, &key);
                    window_states[s_side][0] = IsWindow(window);
                    window_states[s_side][1] = (uint32_t)GetWindowLongA(window, GWL_USERDATA);
                    window_states[s_side][2] = s_descriptor_words[3];
                    command = 0x7d3 + ((case_index / 3) % 3);
                    SendMessageA(window, WM_COMMAND, command, (LPARAM)GetDlgItem(window, command));
                } else if (s_dialog == 3) {
                    SendMessageA(window, WM_CLOSE, 0, 0);
                    if (!IsWindow(window)) { fprintf(stderr, "modeless WM_CLOSE destroyed window\n"); return 2; }
                    SendMessageA(window, 0x464, 0, 0);
                    fs98_pixels(window, 1);
                    s_descriptor_words[3] = 0;
                    SendMessageA(window, 0x464, 0, 0);
                } else {
                    if ((case_index / 3) & 1)
                        SendMessageA(GetDlgItem(window, 0x7d7), BM_CLICK, 0, 0);
                    command = case_index & 1 ? 0x7d8 : 0x7da;
                    SendMessageA(window, WM_COMMAND, command, (LPARAM)GetDlgItem(window, command));
                }
                memcpy(answers[s_side], s_descriptor_words, sizeof(s_descriptor_words));
                final_states[s_side][0] = IsWindow(window);
                final_states[s_side][1] = IsWindowVisible(window);
                final_states[s_side][2] = final_states[s_side][0] ?
                    (uint32_t)GetWindowLongA(window, GWL_USERDATA) : 0;
                /* CSimsDialog::Close hides and signals modal completion;
                 * the modeless tick also calls CWnd::DestroyWindow. */
                if (s_dialog == 3 ? final_states[s_side][0] != 0 :
                    (!final_states[s_side][0] || final_states[s_side][1] ||
                     final_states[s_side][2] != 1)) {
                    fprintf(stderr, "unexpected close state side=%u id=%u alive=%u visible=%u signal=%u\n",
                             s_side, s_dialog, final_states[s_side][0],
                             final_states[s_side][1], final_states[s_side][2]);
                    return 2;
                }
                /* Stand in for the host's final modal-window destruction. */
                if (IsWindow(window)) DestroyWindow(window);
                fs98_pump();
            }
            if (strcmp(s_snapshot[0], s_snapshot[1]) || memcmp(answers[0], answers[1], sizeof(answers[0])) ||
                memcmp(final_states[0], final_states[1], sizeof(final_states[0])) ||
                (case_index % 3 == 0 && memcmp(window_states[0], window_states[1], sizeof(window_states[0]))) ||
                s_pixel_lengths[0] != s_pixel_lengths[1] ||
                memcmp(s_pixels[0], s_pixels[1], s_pixel_lengths[0])) {
                fprintf(stderr, "UI DIFFER case=%u\nRETAIL:\n%sOURS:\n%sanswers=%u,%u,%u / %u,%u,%u\n",
                         case_index, s_snapshot[0], s_snapshot[1],
                         answers[0][0], answers[0][3], answers[0][4],
                         answers[1][0], answers[1][3], answers[1][4]);
                if (s_pixel_lengths[0] == s_pixel_lengths[1]) {
                    uint32_t byte;
                    for (byte = 0; byte < s_pixel_lengths[0]; byte++)
                        if (s_pixels[0][byte] != s_pixels[1][byte]) {
                            fprintf(stderr, "first pixel difference byte=%u values=%u/%u\n", byte,
                                     s_pixels[0][byte], s_pixels[1][byte]);
                            break;
                        }
                }
                return 1;
            }
            comparisons++;
        }
        for (s_side = 0; s_side < 2; s_side++) {
            linkages[s_side]->deinit();
            if (!FreeLibrary(modules[s_side])) exit(2);
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (const char *)modules[s_side], &s_current_module)) {
                fprintf(stderr, "Alerts module was retained after unloading side=%u\n", s_side);
                exit(2);
            }
        }
    }
    printf("Alerts real MFC/SIMSUI: %u public-factory/message cases over 2 load/unload cycles, 0 differ\n", comparisons);
    return 0;
}
