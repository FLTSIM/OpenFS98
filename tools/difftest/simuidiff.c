/* Public factory/message/rendering comparison with the CD's actual MFC/SIMSUI.
 * The host boundary is small: memory, module stack and string resources.
 * This is deliberately separate from the synthetic high-volume suites. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/sim/sim_internal.h"
#include "../../src/sim/sim_unwind.h"

#define SC FS98_SIM_STDCALL
#define TC FS98_SIM_THISCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
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
static uint8_t s_descriptor[0x348];
static char s_snapshot[2][65536];
static char s_callback_log[2][4096];
static uint32_t s_callback_length, s_snapshot_length;
static char s_numbers[3][5][64], s_capacities[5][32], s_realism_numbers[2][64];
static uint8_t s_application[0x400];
static HMODULE s_mfc;
static uint8_t *s_pixels[2];
static uint32_t s_pixel_lengths[2];
static int s_init_probe;
static uint32_t s_init_case, s_init_length;
static char s_init_log[2][64];
static uint8_t s_dynamic[0x40];
static void *s_init_jump[5];
static struct fs98_sim_unwind *s_init_previous;
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

static void *FS98_SIM_THISCALL fs98_init_construct(void *self, void *extension, int32_t system)
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

static FS98_SIM_NOINLINE void fs98_init_run(struct fs98_ui_linkage *linkage)
{
    struct fs98_sim_unwind catch;
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
    printf("Sim native module init: 4 allocation/throw/deinit cases, 0 differ\n");
}

static void fs98_check_dispatch(HWND window, HMODULE expected, int page)
{
    void *from_handle = (void *)GetProcAddress(s_mfc, (const char *)2864);
    void *object = ((void *(SC *)(HWND))from_handle)(window);
    void **vtable = PTR(object, 0);
    uint32_t slots[] = {0x30, page ? 0xf0 : 0xc4, 4};
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
    HWND main = CreateWindowExA(0, "STATIC", "OpenFS98 Sim harness",
                                  WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0,
                                  GetModuleHandleA(0), 0);
    void *constructor = (void *)GetProcAddress(mfc, (const char *)561);
    void *win_init = (void *)GetProcAddress(mfc, (const char *)1575);
    void *from_handle = (void *)GetProcAddress(mfc, (const char *)2864);
    if (!main || !constructor || !win_init || !from_handle) exit(2);
    ((void *(FS98_SIM_THISCALL *)(void *, const char *))constructor)(s_application, 0);
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

static void fs98_log(uint32_t operation, uint32_t value)
{
    int length = snprintf(s_callback_log[s_side] + s_callback_length,
        sizeof(s_callback_log[0]) - s_callback_length, "%u:%u;", operation, value);
    if (length < 0 || (uint32_t)length >= sizeof(s_callback_log[0]) - s_callback_length) exit(2);
    s_callback_length += length;
}
static int32_t SC fs98_validate(void *descriptor)
{ fs98_log(10, D32(descriptor, 0x20)); return 0; }
static int32_t SC fs98_convert(void *descriptor)
{ fs98_log(11, D32(descriptor, 0x20)); return 0; }
static int32_t SC fs98_fuel_convert(void *descriptor, uint32_t row)
{ (void)descriptor; fs98_log(12, row); return 0; }
static int32_t SC fs98_graph(uint32_t operation, void *descriptor, uint32_t window)
{
    void *object = ((void *(SC *)(HWND))(void *)GetProcAddress(s_mfc, (const char *)2864))((HWND)GetParent((HWND)window));
    fs98_log(operation, D32(descriptor, 0));
    if (!IsWindow((HWND)window) || !object) exit(2);
    if (!operation) {
        D32(object, 0x10e0) = 44; D32(object, 0x10e4) = 90;
        D32(object, 0x10e8) = 344; D32(object, 0x10ec) = 250;
        D32(object, 0x10f4) = 0;
        strcpy((char *)object + 0x10f8, "Vertical velocity");
        strcpy((char *)object + 0x1116, "Time");
    }
    return 0;
}
static uint32_t SC fs98_length(const char *text) { return lstrlenA(text); }
static uint32_t SC fs98_string_alloc(uint32_t id, char **out)
{
    char buffer[512];
    uint32_t length = fs98_string(id, buffer, sizeof(buffer));
    *out = malloc(length + 1);
    if (!*out) exit(2);
    memcpy(*out, buffer, length + 1);
    return length;
}
static void fs98_graph_class(void)
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
static void fs98_fixture(uint32_t case_index, int crash)
{
    uint32_t index, group, row;
    static const char *s_axis[] = {"0", "50", "100"};
    memset(s_descriptor, 0, sizeof(s_descriptor));
    memset(s_numbers, 0, sizeof(s_numbers));
    memset(s_capacities, 0, sizeof(s_capacities));
    memset(s_realism_numbers, 0, sizeof(s_realism_numbers));
    s_callback_length = 0; s_callback_log[s_side][0] = 0;
    if (crash) {
        PTR(s_descriptor, 8) = fs98_graph;
        D32(s_descriptor, 0x10) = case_index % 2;
        PTR(s_descriptor, 0x14) = "Native crash fixture";
        PTR(s_descriptor, 0x18) = "Flight data";
        D32(s_descriptor, 0x1c) = D32(s_descriptor, 0x24) = 3;
        PTR(s_descriptor, 0x20) = PTR(s_descriptor, 0x28) = s_axis;
        D32(s_descriptor, 0x2c) = 1;
        return;
    }
    PTR(s_descriptor, 4) = fs98_validate;
    D32(s_descriptor, 8) = case_index % 6;
    D32(s_descriptor, 0xc) = case_index & 1;
    D32(s_descriptor, 0x10) = case_index % 4;
    D32(s_descriptor, 0x14) = case_index & 1;
    D32(s_descriptor, 0x18) = case_index & 1 ? 0xffffffff : 0;
    D32(s_descriptor, 0x1c) = case_index & 1 ? 0xffffffff : 0;
    for (index = 0; index < 6; index++) {
        static const uint32_t s_offsets[] = {0x28, 0x9c, 0xc0, 0xec, 0x164, 0x2c8};
        D32(s_descriptor, s_offsets[index]) = 0;
    }
    D32(s_descriptor, 0x28 + 8) = case_index % 10;
    D32(s_descriptor, 0x28 + 0xc) = 0; D32(s_descriptor, 0x28 + 0x10) = 10;
    PTR(s_descriptor, 0x28 + 0x3c) = "Airspeed";
    PTR(s_descriptor, 0x28 + 0x40) = "Vertical speed";
    for (index = 0; index < 2; index++) {
        uint32_t offset = index ? 0x5c : 0x48;
        strcpy(s_realism_numbers[index], index ? "1000" : "100");
        PTR(s_descriptor, 0x28 + offset) = s_realism_numbers[index];
        D32(s_descriptor, 0x28 + offset + 4) = 64;
    }
    PTR(s_descriptor, 0x28 + 0x6c) = PTR(s_descriptor, 0x28 + 0x70) = fs98_convert;
    D32(s_descriptor, 0x9c + 8) = case_index % 10;
    D32(s_descriptor, 0x9c + 0xc) = 0; D32(s_descriptor, 0x9c + 0x10) = 10;
    D32(s_descriptor, 0xc0 + 8) = case_index % 3;
    *(uint16_t *)(s_descriptor + 0xec + 0x24) = 1 + case_index % 4;
    D32(s_descriptor, 0xec + 0x58) = D32(s_descriptor, 0xec + 0x6c) = case_index & 1;
    PTR(s_descriptor, 0x164 + 0x14) = " gallons";
    D32(s_descriptor, 0x164 + 0x10) = case_index % 7;
    for (group = 0; group < 3; group++) for (row = 0; row < 5; row++) {
        uint32_t offset = 0x164 + 0x1c + group * 0x64 + row * 0x14;
        snprintf(s_numbers[group][row], 64, "%u", 10 + group + row);
        PTR(s_descriptor, offset) = s_numbers[group][row];
        D32(s_descriptor, offset + 4) = 64;
    }
    for (row = 0; row < 5; row++) {
        snprintf(s_capacities[row], 32, "%u", (case_index + row) % 3 ? 100 : 0);
        PTR(s_descriptor, 0x164 + 0x144 + row * 4) = s_capacities[row];
    }
    PTR(s_descriptor, 0x164 + 0x158) = PTR(s_descriptor, 0x164 + 0x15c) =
        PTR(s_descriptor, 0x164 + 0x160) = fs98_fuel_convert;
}
static void *fs98_object(HWND window)
{ return ((void *(SC *)(HWND))(void *)GetProcAddress(s_mfc, (const char *)2864))(window); }
static void fs98_capture(HWND window)
{
    s_snapshot_length = 0; s_snapshot[s_side][0] = 0;
    EnumChildWindows(window, fs98_snapshot, (LPARAM)window);
    BringWindowToTop(window); SetForegroundWindow(window);
    RedrawWindow(window, 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    fs98_pump(); fs98_pixels(window, 1);
}
static void fs98_compare(uint32_t case_index, const char *phase)
{
    if (strcmp(s_snapshot[0], s_snapshot[1]) || strcmp(s_callback_log[0], s_callback_log[1]) ||
        s_pixel_lengths[0] != s_pixel_lengths[1] || memcmp(s_pixels[0], s_pixels[1], s_pixel_lengths[0])) {
        fprintf(stderr, "SIM UI DIFFER case=%u phase=%s controls=%d callbacks=%d pixels=%u/%u\nRETAIL:\n%s%sOURS:\n%s%s\n",
            case_index, phase, !!strcmp(s_snapshot[0], s_snapshot[1]),
            !!strcmp(s_callback_log[0], s_callback_log[1]), s_pixel_lengths[0], s_pixel_lengths[1],
            s_callback_log[0], s_snapshot[0], s_callback_log[1], s_snapshot[1]);
        exit(1);
    }
}
/* Keep every page's bytes until the other implementation reaches the same
 * checkpoint; a later repaint must not hide a difference on an earlier tab. */
static void fs98_checkpoint(HWND window, uint32_t case_index, uint32_t phase)
{
    static struct {
        char snapshot[65536], callbacks[4096];
        uint8_t *pixels;
        uint32_t length;
    } s_captures[8];
    fs98_capture(window);
    if (case_index == 0 && !s_side) {
        char path[MAX_PATH];
        FILE *file;
        snprintf(path, sizeof(path), "bin/sim-diff/native-page-%u.txt", phase);
        file = fopen(path, "w");
        if (!file) exit(2);
        fputs(s_snapshot[0], file);
        fclose(file);
        snprintf(path, sizeof(path), "bin/sim-diff/native-page-%u.bmp", phase);
        file = fopen(path, "wb");
        if (!file) exit(2);
        {
            RECT rect;
            BITMAPFILEHEADER header = {0};
            BITMAPINFOHEADER info = {0};
            GetClientRect(window, &rect);
            header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info);
            header.bfSize = header.bfOffBits + s_pixel_lengths[0];
            info.biSize = sizeof(info); info.biWidth = rect.right; info.biHeight = -rect.bottom;
            info.biPlanes = 1; info.biBitCount = 32;
            fwrite(&header, sizeof(header), 1, file); fwrite(&info, sizeof(info), 1, file);
            fwrite(s_pixels[0], s_pixel_lengths[0], 1, file);
        }
        fclose(file);
    }
    if (!s_side) {
        strcpy(s_captures[phase].snapshot, s_snapshot[0]);
        strcpy(s_captures[phase].callbacks, s_callback_log[0]);
        s_captures[phase].pixels = realloc(s_captures[phase].pixels, s_pixel_lengths[0]);
        if (!s_captures[phase].pixels) exit(2);
        s_captures[phase].length = s_pixel_lengths[0];
        memcpy(s_captures[phase].pixels, s_pixels[0], s_pixel_lengths[0]);
    } else {
        char label[32];
        strcpy(s_snapshot[0], s_captures[phase].snapshot);
        strcpy(s_callback_log[0], s_captures[phase].callbacks);
        s_pixels[0] = realloc(s_pixels[0], s_captures[phase].length);
        if (!s_pixels[0]) exit(2);
        s_pixel_lengths[0] = s_captures[phase].length;
        memcpy(s_pixels[0], s_captures[phase].pixels, s_pixel_lengths[0]);
        snprintf(label, sizeof(label), "checkpoint-%u", phase);
        fs98_compare(case_index, label);
    }
}
static void fs98_check(HWND window, uint32_t id, int checked, int radio)
{
    void *function = (void *)GetProcAddress(GetModuleHandleA("SIMSUI.DLL"), radio ?
        "?SetChecked@CSimsRadioButton@@QAEHH@Z" : "?SetChecked@CSimsCheckButton@@QAEHH@Z");
    HWND control = GetDlgItem(window, id);
    if (!control || !function) exit(2);
    ((int (TC *)(void *, int))function)(fs98_object(control), checked);
    SendMessageA(window, WM_COMMAND, id, (LPARAM)control);
}
static void fs98_combo(HWND window, uint32_t id, uint32_t selected)
{
    HWND control = GetDlgItem(window, id);
    if (!control) exit(2);
    SendMessageA(control, CB_SETCURSEL, selected, 0);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), (LPARAM)control);
}
static void fs98_edit(HWND window, uint32_t id, const char *text)
{
    HWND control = GetDlgItem(window, id);
    if (!control) exit(2);
    SetWindowTextA(control, text);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, EN_KILLFOCUS), (LPARAM)control);
}
static void fs98_page_events(HWND window, uint32_t page, uint32_t case_index)
{
    uint32_t index;
    if (page == 0) {
        fs98_edit(window, 0x13bd, "125"); fs98_edit(window, 0x13be, "1500");
        fs98_check(window, 0x13ac, case_index & 1, 0);
        SendMessageA(window, 0x5f9, 0, 0);
    } else if (page == 1) {
        fs98_check(window, 0x13a6, case_index & 1, 0);
        fs98_check(window, 0x1393, !(case_index & 1), 0);
    } else if (page == 2) {
        const uint32_t ids[] = {0x13a0, 0x1390, 0x138f};
        fs98_check(window, ids[case_index % 3], 1, 1);
        fs98_check(window, 0x13ad, case_index & 1, 0);
    } else if (page == 3) {
        for (index = 0; index < 5; index++) fs98_check(window, 0x138a + index, (case_index + index) & 1, 0);
        for (index = 0; index < 4; index++) fs98_combo(window, 0x13b1 + index, (case_index + index) % 3);
    } else if (page == 4) {
        fs98_check(window, 0x13ae, 1, 0);
        fs98_combo(window, 0x13b5, case_index % 3);
        fs98_edit(window, 0x13c1, "23");
    } else {
        const uint32_t ids[] = {0x13af, 0x13b0, 0x13b6, 0x13b7, 0x13b8, 0x13ba, 0x13bb};
        for (index = 0; index < 7; index++) fs98_combo(window, ids[index], (case_index + index) % 3);
        fs98_check(window, 0x139b, case_index & 1, 0);
    }
}
int main(int argc, char **argv)
{
    HMODULE ui, modules[2], global;
    struct fs98_ui_linkage *linkages[2];
    char directory[MAX_PATH];
    uint32_t index, case_index, cycle, comparisons = 0;
    uint32_t cases = argc > 2 ? strtoul(argv[2], 0, 0) : 30;
    struct result { uint8_t descriptor[0x348]; char numbers[3][5][64], realism[2][64], callbacks[4096]; } results[2];
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, 0, _IONBF, 0); SetUnhandledExceptionFilter(fs98_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, 0); SetDllDirectoryA(directory);
    s_mfc = fs98_load("cd\\mfc42.dll"); fs98_application(s_mfc); fs98_graph_class();
    ui = fs98_load("orig\\SIMSUI.DLL"); s_fll = fs98_load("orig\\ENGLISH.FLL");
    global = fs98_load("orig\\Modules\\GLOBAL.DLL"); memcpy(s_global, GetProcAddress(global, "Linkage"), 4764);
    for (index = 0; index < sizeof(s_host_services); index += 4) PTR(s_host_services, index) = fs98_unknown;
    PTR(s_host_services, 0x20c) = fs98_allocate; PTR(s_host_services, 0x210) = fs98_reallocate;
    PTR(s_host_services, 0x214) = fs98_free; PTR(s_host_services, 0x298) = fs98_register;
    PTR(s_host_services, 0x29c) = fs98_release; PTR(s_host_services, 0x2a0) = fs98_string;
    PTR(s_host_services, 0x2a4) = fs98_string_alloc; PTR(s_host_services, 0x2b8) = fs98_length;
    {
        struct fs98_ui_import *imports = (void *)GetProcAddress(ui, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global;
    }
    for (cycle = 0; cycle < 2; cycle++) {
        modules[0] = fs98_load("orig\\English\\Sim.DLL");
        modules[1] = fs98_load(argc > 1 ? argv[1] : "bin\\sim-diff\\SIM_ours.dll");
        if (modules[0] == modules[1]) return 2;
        for (s_side = 0; s_side < 2; s_side++) {
            struct fs98_ui_import *imports = (void *)GetProcAddress(modules[s_side], "ImportTable");
            imports[0].link = s_host_services; imports[1].link = s_global;
            linkages[s_side] = (void *)GetProcAddress(modules[s_side], "Linkage");
            if (!linkages[s_side] || linkages[s_side]->id != 0x80000004 ||
                imports[0].id != 1 || imports[1].id != 2 || imports[2].id || imports[2].link) return 2;
            for (index = 0; index < 5; index++) {
                const uint32_t ids[] = {0x1206, 0x1207, 0x1208, 0x1209, 0x14050001};
                if (linkages[s_side]->dialogs[index].id != ids[index] ||
                    !linkages[s_side]->dialogs[index].create || linkages[s_side]->dialogs[index].flags) return 2;
            }
            if (linkages[s_side]->dialogs[5].id) return 2;
        }
        fs98_init_probe(linkages, modules);
        for (s_side = 0; s_side < 2; s_side++) { s_current_module = modules[s_side]; linkages[s_side]->init(); }
        for (case_index = 0; case_index < cases; case_index++) {
            int crash = case_index % 5 == 4;
            for (s_side = 0; s_side < 2; s_side++) {
                HWND window;
                void *object;
                uint32_t page;
                struct fs98_ui_dialog *dialog = &linkages[s_side]->dialogs[case_index % 5];
                s_current_module = modules[s_side]; s_dialog = dialog->id;
                fs98_fixture(case_index, crash);
                printf("Sim UI cycle=%u case=%u side=%u dialog=%x create\n", cycle, case_index, s_side, s_dialog);
                window = (HWND)dialog->create(0, s_descriptor);
                if (!IsWindow(window)) { fprintf(stderr, "invalid Sim HWND=%p error=%lu\n", window, GetLastError()); return 2; }
                fs98_pump(); fs98_check_dispatch(window, modules[s_side], 0); object = fs98_object(window);
                fs98_checkpoint(window, case_index, 0);
                if (!crash) {
                    void *tabs = (uint8_t *)object + 0x12e8;
                    void *active = (void *)GetProcAddress(ui, "?SetActiveTab@CSimsTabCtrl@@QAEHHPAUHWND__@@@Z");
                    for (page = 0; page < 6; page++) {
                        ((int (TC *)(void *, int, HWND))active)(tabs, page, 0);
                        fs98_pump();
                        HWND page_window = (HWND)D32(PTR(object, 0x224c + page * 4), 0x20);
                        if (!IsWindow(page_window) || !IsWindowVisible(page_window)) {
                            fprintf(stderr, "page %u is not visible after activation\n", page); return 2;
                        }
                        fs98_check_dispatch(page_window, modules[s_side], 1);
                        fs98_page_events(page_window, page, case_index);
                        fs98_checkpoint(window, case_index, page + 1);
                    }
                    ((int (TC *)(void *, int, HWND))active)(tabs, case_index % 6, 0);
                } else SendMessageA(window, 0x464, 0, 0);
                fs98_pump(); fs98_checkpoint(window, case_index, 7);
                SendMessageA(window, WM_COMMAND, case_index & 1 ? IDCANCEL : IDOK, 0);
                fs98_pump();
                if (!IsWindow(window) || IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA) != 1) {
                    fprintf(stderr, "Sim close state invalid\n"); return 2;
                }
                if (D32(s_descriptor, 0) != !(case_index & 1)) {
                    fprintf(stderr, "unexpected dialog answer\n"); return 2;
                }
                memcpy(results[s_side].descriptor, s_descriptor, sizeof(s_descriptor));
                memcpy(results[s_side].numbers, s_numbers, sizeof(s_numbers));
                memcpy(results[s_side].realism, s_realism_numbers, sizeof(s_realism_numbers));
                memset(results[s_side].callbacks, 0, sizeof(results[s_side].callbacks));
                strcpy(results[s_side].callbacks, s_callback_log[s_side]);
                DestroyWindow(window); fs98_pump();
                if (IsWindow(window)) return 2;
            }
            if (memcmp(&results[0], &results[1], sizeof(results[0]))) {
                fprintf(stderr, "SIM UI DIFFER case=%u descriptor/buffers\n", case_index); return 1;
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
    printf("Sim real MFC/SIMSUI: %u public-factory/message cases over 2 load/unload cycles, 0 differ\n", comparisons);
    return 0;
}
