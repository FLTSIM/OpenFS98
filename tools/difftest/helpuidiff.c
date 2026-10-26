/* Public factory/message/rendering comparison with the CD's actual MFC/SIMSUI.
 * The host boundary is small: memory, module stack and string resources.
 * This is deliberately separate from the synthetic high-volume suites. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/help/help_internal.h"
#include "../../src/help/help_unwind.h"

#define SC FS98_HELP_STDCALL
#define TC FS98_HELP_THISCALL
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
static uint8_t s_descriptor[0x40], s_records[4][0xa08];
static char s_snapshot[2][65536], s_callback_log[2][4096];
static uint32_t s_callback_length, s_snapshot_length;
static uint32_t s_case, s_registry_case, s_registry_opens[2], s_registry_queries[2], s_font_destroys[2];
static uint32_t s_timer_attempts[2], s_timer_kills[2], s_timer_failures;
static struct fs98_ui_linkage *s_linkages[2];
static HWND s_graph_child;
static void *s_font_destroy;
static uint8_t s_font_original[5];
static HWND s_nested_window;
static uint8_t s_application[0x400];
static HMODULE s_mfc;
static uint8_t *s_pixels[2];
static uint32_t s_pixel_lengths[2];
static int s_init_probe;
static uint32_t s_init_case, s_init_length;
static char s_init_log[2][64];
static uint8_t s_dynamic[0x40];
static void *s_init_jump[5];
static struct fs98_help_unwind *s_init_previous;
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

static void *FS98_HELP_THISCALL fs98_init_construct(void *self, void *extension, int32_t system)
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

static FS98_HELP_NOINLINE void fs98_init_run(struct fs98_ui_linkage *linkage)
{
    struct fs98_help_unwind catch;
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
    printf("Help native module init: 4 allocation/throw/deinit cases, 0 differ\n");
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
 * that MFC gets from the helpulator. The CD's app ctor is ordinal 561 at
 * 0x5f40b308; its CWinThread::GetMainWnd (0x5f402a15) reads +0x24/+0x20.
 * Pin those callees instead of patching a failing UI dependency. */
static void fs98_application(HMODULE mfc)
{
    HWND main = CreateWindowExA(0, "STATIC", "OpenFS98 Help harness",
                                  WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0,
                                  GetModuleHandleA(0), 0);
    void *constructor = (void *)GetProcAddress(mfc, (const char *)561);
    void *win_init = (void *)GetProcAddress(mfc, (const char *)1575);
    void *from_handle = (void *)GetProcAddress(mfc, (const char *)2864);
    if (!main || !constructor || !win_init || !from_handle) exit(2);
    ((void *(FS98_HELP_THISCALL *)(void *, const char *))constructor)(s_application, 0);
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
static void *fs98_object(HWND window)
{ return ((void *(SC *)(HWND))(void *)GetProcAddress(s_mfc, (const char *)2864))(window); }
static void fs98_patch_code(void *entry, const void *bytes)
{
    DWORD previous;
    if (!VirtualProtect(entry, 5, PAGE_EXECUTE_READWRITE, &previous)) exit(2);
    memcpy(entry, bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), entry, 5);
    VirtualProtect(entry, 5, previous, &previous);
}
static void TC fs98_font_destroy(void *font);
static void fs98_font_hook(void)
{
    uint8_t jump[5] = {0xe9};
    *(uint32_t *)(jump + 1) = (uint32_t)fs98_font_destroy - (uint32_t)s_font_destroy - 5;
    fs98_patch_code(s_font_destroy, jump);
}
static void TC fs98_font_destroy(void *font)
{
    s_font_destroys[s_side]++;
    fs98_patch_code(s_font_destroy, s_font_original);
    ((void (TC *)(void *))s_font_destroy)(font);
    fs98_font_hook();
}
/* Both implementations see the same private registry fixture. No real key
 * is opened; About's intentional retail handle leak cannot escape this test. */
static LSTATUS WINAPI fs98_reg_open(HKEY key, const char *subkey, DWORD options, REGSAM access, HKEY *out)
{
    if (key != HKEY_LOCAL_MACHINE || strcmp(subkey,
        "Software\\Microsoft\\Microsoft Games\\Flight Simulator\\6.1") || options || access != 0x20019) exit(2);
    s_registry_opens[s_side]++; fs98_log(20, s_registry_case);
    if (s_registry_case == 1) return ERROR_ACCESS_DENIED;
    *out = (HKEY)0x12345678;
    return ERROR_SUCCESS;
}
static LSTATUS WINAPI fs98_reg_query(HKEY key, const char *name, DWORD *reserved, DWORD *type, BYTE *data, DWORD *size)
{
    const char *text;
    int product = !strcmp(name, "ProductID");
    if (key != (HKEY)0x12345678 || reserved || type || (!product && strcmp(name, "Name")) ||
        *size != (product ? 49u : 99u)) exit(2);
    s_registry_queries[s_side]++; fs98_log(21, product);
    if (s_registry_case == (product ? 3u : 2u)) return ERROR_FILE_NOT_FOUND;
    text = product ? "Fixture Product 12345" : "OpenFS98 Native Harness";
    memcpy(data, text, strlen(text) + 1); *size = strlen(text) + 1;
    return ERROR_SUCCESS;
}
/* Drive native WM_TIMER deterministically. USER32's unrelated timers remain
 * live, while Help's credits timer ids can fail or succeed on a chosen try. */
static UINT_PTR WINAPI fs98_set_timer(HWND window, UINT_PTR timer, UINT period, TIMERPROC callback)
{
    if (!IsWindow(window) || timer < 3 || timer > 14 || period != 500 || callback) exit(2);
    s_timer_attempts[s_side]++; fs98_log(30, timer);
    return timer < 3 + s_timer_failures ? 0 : timer;
}
static BOOL WINAPI fs98_kill_timer(HWND window, UINT_PTR timer)
{
    if (!IsWindow(window) || timer < 3 || timer > 14) exit(2);
    s_timer_kills[s_side]++; fs98_log(31, timer);
    return TRUE;
}
static void fs98_patch_import(HMODULE module, const char *dll, const char *name, void *function)
{
    uint8_t *base = (void *)module;
    IMAGE_DOS_HEADER *dos = (void *)base;
    IMAGE_NT_HEADERS32 *nt = (void *)(base + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *imports = (void *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imports->Name; imports++) {
        IMAGE_THUNK_DATA32 *names, *targets;
        if (lstrcmpiA((char *)base + imports->Name, dll)) continue;
        if (!imports->OriginalFirstThunk) exit(2);
        names = (void *)(base + imports->OriginalFirstThunk);
        targets = (void *)(base + imports->FirstThunk);
        for (; names->u1.AddressOfData; names++, targets++) {
            IMAGE_IMPORT_BY_NAME *entry;
            DWORD previous;
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) continue;
            entry = (void *)(base + names->u1.AddressOfData);
            if (strcmp((char *)entry->Name, name)) continue;
            if (!VirtualProtect(&targets->u1.Function, 4, PAGE_READWRITE, &previous)) exit(2);
            targets->u1.Function = (uint32_t)function;
            VirtualProtect(&targets->u1.Function, 4, previous, &previous);
            return;
        }
    }
    fprintf(stderr, "missing native import %s:%s\n", dll, name); exit(2);
}
static int32_t SC fs98_split(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{
    char ignored_drive[8], ignored_dir[260], ignored_name[260], extension[32];
    _splitpath(path, drive ? drive : ignored_drive, dir ? dir : ignored_dir,
        name ? name : ignored_name, extension);
    if (ext) *ext = 0;
    fs98_log(40, strlen(path)); return 0;
}
static int32_t SC fs98_join(char *out, const char *drive, const char *dir, const char *name)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s%s%s", drive ? drive : "", dir ? dir : "", name ? name : "");
    strcpy(out, buffer); fs98_log(41, strlen(out)); return 0;
}
static int32_t SC fs98_exists(const char *path, int32_t extension)
{
    if (extension != 0x1d) exit(2);
    fs98_log(42, strlen(path)); return s_case & 1 ? -1 : 0;
}
static int32_t SC fs98_about_callback(void *descriptor)
{
    uint32_t credits[8] = {0};
    fs98_log(50, D32(descriptor, 0));
    if (s_nested_window) exit(2);
    s_nested_window = (HWND)s_linkages[s_side]->dialogs[1].create(0, credits);
    if (!IsWindow(s_nested_window)) exit(2);
    fs98_check_dispatch(s_nested_window, s_current_module);
    SendMessageA(s_nested_window, WM_COMMAND, IDOK, 0);
    if (IsWindowVisible(s_nested_window)) exit(2);
    DestroyWindow(s_nested_window); s_nested_window = 0;
    return 0;
}
static BOOL CALLBACK fs98_find_graph(HWND window, LPARAM output)
{
    HWND surface = GetDlgItem(window, 0xee2);
    if (surface) { *(HWND *)output = surface; return FALSE; }
    return TRUE;
}
static int32_t SC fs98_graph(uint32_t operation, void *descriptor, uint32_t window)
{
    RECT rect;
    fs98_log(operation, D32(descriptor, 0));
    (void)window;
    if (!operation) {
        HWND surface = 0;
        /* Retail never initializes the saved +0x10dc argument here. The
         * unused graph owner fixture locates its native surface independently;
         * exact argument forwarding is covered by the guarded core harness. */
        EnumThreadWindows(GetCurrentThreadId(), fs98_find_graph, (LPARAM)&surface);
        if (!surface) exit(2);
        GetClientRect(surface, &rect);
        s_graph_child = CreateWindowExA(0, "STATIC", "Lesson graph fixture",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, rect.right, rect.bottom,
            surface, (HMENU)0x7001, GetModuleHandleA(0), 0);
        if (!s_graph_child) exit(2);
    } else if (operation == 1) {
        if (!IsWindow(s_graph_child)) exit(2);
        SetWindowTextA(s_graph_child, "Updated lesson graph");
    } else if (operation == 2) {
        if (!IsWindow(s_graph_child)) exit(2);
        DestroyWindow(s_graph_child); s_graph_child = 0;
    } else exit(2);
    return 0;
}
static void fs98_graph_class(void)
{
    WNDCLASSA info;
    memset(&info, 0, sizeof(info));
    info.style = CS_GLOBALCLASS; info.lpfnWndProc = DefWindowProcA;
    info.hInstance = GetModuleHandleA(0); info.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    info.lpszClassName = "FS98DLG3D";
    if (!RegisterClassA(&info)) exit(2);
}
static void fs98_fixture(void)
{
    uint32_t record;
    memset(s_descriptor, 0, sizeof(s_descriptor)); memset(s_records, 0, sizeof(s_records));
    s_callback_length = 0; s_callback_log[s_side][0] = 0;
    s_registry_case = (s_case / 5) % 4; s_timer_failures = (s_case / 5) % 4;
    s_registry_opens[s_side] = s_registry_queries[s_side] = 0;
    s_timer_attempts[s_side] = s_timer_kills[s_side] = 0;
    PTR(s_descriptor, 0x10) = fs98_about_callback;
    if (s_dialog == 0x1106) {
        D32(s_descriptor, 8) = s_case / 5 % 5; D32(s_descriptor, 0xc) = s_case % 4;
        PTR(s_descriptor, 0x10) = s_records; D32(s_descriptor, 0x14) = s_case & 1;
        for (record = 0; record < 4; record++) {
            snprintf((char *)s_records[record], 0x100, "Fixture lesson %u", record);
            snprintf((char *)s_records[record] + 0x100, 0x800, "Description for fixture lesson %u", record);
            snprintf((char *)s_records[record] + 0x900, 0x100, "LESSONS\\LESSON%u.ADV", record);
        }
    } else if (s_dialog == 0x14050004) PTR(s_descriptor, 8) = fs98_graph;
}
static void fs98_capture(HWND window, uint32_t phase)
{
    s_snapshot_length = 0; s_snapshot[s_side][0] = 0;
    EnumChildWindows(window, fs98_snapshot, (LPARAM)window);
    BringWindowToTop(window); SetForegroundWindow(window);
    RedrawWindow(window, 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    fs98_pump(); fs98_pixels(window, 1);
    if (s_case < 5 && !s_side) {
        char path[MAX_PATH];
        FILE *file;
        snprintf(path, sizeof(path), "native-dialog-%u-%u.txt", s_case, phase);
        file = fopen(path, "w"); if (!file) exit(2); fputs(s_snapshot[0], file); fclose(file);
    }
}
static void fs98_checkpoint(HWND window, uint32_t phase)
{
    static struct { char snapshot[65536], callbacks[4096]; uint8_t *pixels; uint32_t length; } s_captures[2];
    fs98_capture(window, phase);
    if (!s_side) {
        strcpy(s_captures[phase].snapshot, s_snapshot[0]); strcpy(s_captures[phase].callbacks, s_callback_log[0]);
        s_captures[phase].pixels = realloc(s_captures[phase].pixels, s_pixel_lengths[0]);
        if (!s_captures[phase].pixels) exit(2);
        s_captures[phase].length = s_pixel_lengths[0];
        memcpy(s_captures[phase].pixels, s_pixels[0], s_pixel_lengths[0]);
    } else if (strcmp(s_captures[phase].snapshot, s_snapshot[1]) ||
        strcmp(s_captures[phase].callbacks, s_callback_log[1]) || s_captures[phase].length != s_pixel_lengths[1] ||
        memcmp(s_captures[phase].pixels, s_pixels[1], s_pixel_lengths[1])) {
        fprintf(stderr, "HELP UI DIFFER case=%u phase=%u controls=%d callbacks=%d pixels=%u/%u\nRETAIL:\n%s%sOURS:\n%s%s\n",
            s_case, phase, !!strcmp(s_captures[phase].snapshot, s_snapshot[1]),
            !!strcmp(s_captures[phase].callbacks, s_callback_log[1]), s_captures[phase].length, s_pixel_lengths[1],
            s_captures[phase].callbacks, s_captures[phase].snapshot, s_callback_log[1], s_snapshot[1]);
        exit(1);
    }
}
static void fs98_credit_file(void)
{
    FILE *file = fopen("fltcred.rtf", "w");
    uint32_t line;
    if (!file) exit(2);
    fputs("{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Arial;}}\\f0\\fs24\n", file);
    for (line = 0; line < 40; line++) fprintf(file, "Native credits fixture line %u\\par\n", line);
    fputs("}\n", file); fclose(file);
}
int main(int argc, char **argv)
{
    HMODULE ui, modules[2], global;
    char directory[MAX_PATH];
    uint32_t index, cycle, comparisons = 0, cases = argc > 2 ? strtoul(argv[2], 0, 0) : 30;
    struct result { uint8_t descriptor[0x40]; char callbacks[4096]; uint32_t opens, queries, attempts, kills, scroll; } results[2];
    char module_path[MAX_PATH], retail_path[MAX_PATH];
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, 0, _IONBF, 0); SetUnhandledExceptionFilter(fs98_fault);
    GetFullPathNameA("orig", MAX_PATH, directory, 0); SetDllDirectoryA(directory);
    GetFullPathNameA(argc > 1 ? argv[1] : "bin/help-diff/HELP_ours.dll", MAX_PATH, module_path, 0);
    GetFullPathNameA("orig/English/Help.DLL", MAX_PATH, retail_path, 0);
    s_mfc = fs98_load("cd\\mfc42.dll"); fs98_application(s_mfc); fs98_graph_class();
    ui = fs98_load("orig\\SIMSUI.DLL"); s_fll = fs98_load("orig\\ENGLISH.FLL");
    global = fs98_load("orig\\Modules\\GLOBAL.DLL"); memcpy(s_global, GetProcAddress(global, "Linkage"), 4764);
    s_font_destroy = (void *)GetProcAddress(ui, "??1CSimsFont@@UAE@XZ");
    if (!s_font_destroy) return 2;
    memcpy(s_font_original, s_font_destroy, 5); fs98_font_hook();
    for (index = 0; index < sizeof(s_host_services); index += 4) PTR(s_host_services, index) = fs98_unknown;
    PTR(s_host_services, 0x20c) = fs98_allocate; PTR(s_host_services, 0x210) = fs98_reallocate;
    PTR(s_host_services, 0x214) = fs98_free; PTR(s_host_services, 0x298) = fs98_register;
    PTR(s_host_services, 0x29c) = fs98_release; PTR(s_host_services, 0x2a0) = fs98_string;
    PTR(s_host_services, 0x188) = fs98_split; PTR(s_host_services, 0x184) = fs98_join;
    PTR(s_host_services, 0x158) = fs98_exists;
    {
        struct fs98_ui_import *imports = (void *)GetProcAddress(ui, "ImportTable");
        imports[0].link = s_host_services; imports[1].link = s_global;
    }
    GetFullPathNameA("bin/help-diff", MAX_PATH, directory, 0);
    if (!SetCurrentDirectoryA(directory)) return 2;
    /* Artifacts and the RTF stay private even when the loader opens a relative filename. */
    fs98_credit_file();
    for (cycle = 0; cycle < 2; cycle++) {
        uint32_t before_destroy[2], after_destroy[2];
        modules[0] = fs98_load(retail_path); modules[1] = fs98_load(module_path);
        if (modules[0] == modules[1]) return 2;
        for (s_side = 0; s_side < 2; s_side++) {
            const uint32_t ids[] = {0x1607, 0x16070001, 0x1106, 0x14050004, 0x11060001};
            struct fs98_ui_import *imports = (void *)GetProcAddress(modules[s_side], "ImportTable");
            imports[0].link = s_host_services; imports[1].link = s_global;
            s_linkages[s_side] = (void *)GetProcAddress(modules[s_side], "Linkage");
            if (!s_linkages[s_side] || s_linkages[s_side]->id != 0x80000008 || imports[0].id != 1 ||
                imports[1].id != 2 || imports[2].id || imports[2].link) return 2;
            for (index = 0; index < 5; index++) if (s_linkages[s_side]->dialogs[index].id != ids[index] ||
                !s_linkages[s_side]->dialogs[index].create || s_linkages[s_side]->dialogs[index].flags) return 2;
            if (s_linkages[s_side]->dialogs[5].id) return 2;
            fs98_patch_import(modules[s_side], "ADVAPI32.dll", "RegOpenKeyExA", fs98_reg_open);
            fs98_patch_import(modules[s_side], "ADVAPI32.dll", "RegQueryValueExA", fs98_reg_query);
            fs98_patch_import(modules[s_side], "USER32.dll", "SetTimer", fs98_set_timer);
            fs98_patch_import(modules[s_side], "USER32.dll", "KillTimer", fs98_kill_timer);
        }
        fs98_init_probe(s_linkages, modules);
        for (s_side = 0; s_side < 2; s_side++) { s_current_module = modules[s_side]; s_linkages[s_side]->init(); }
        for (s_case = 0; s_case < cases; s_case++) {
            for (s_side = 0; s_side < 2; s_side++) {
                HWND window;
                void *object;
                s_current_module = modules[s_side]; s_dialog = s_linkages[s_side]->dialogs[s_case % 5].id;
                fs98_fixture(); memset(&results[s_side], 0, sizeof(results[0]));
                printf("Help UI cycle=%u case=%u side=%u dialog=%x create\n", cycle, s_case, s_side, s_dialog);
                window = (HWND)s_linkages[s_side]->dialogs[s_case % 5].create(0, s_descriptor);
                if (!IsWindow(window) || !IsWindowVisible(window)) { fprintf(stderr, "invalid Help HWND=%p error=%lu\n", window, GetLastError()); return 2; }
                fs98_pump(); fs98_check_dispatch(window, modules[s_side]); object = fs98_object(window);
                fs98_checkpoint(window, 0);
                if (s_dialog == 0x1607) {
                    SendMessageA(window, WM_COMMAND, 0xedd, (LPARAM)GetDlgItem(window, 0xedd));
                } else if (s_dialog == 0x16070001) {
                    HWND rich = (HWND)D32(object, 0x10ec);
                    uint32_t tick;
                    if (!IsWindow(rich) || SendMessageA(rich, EM_GETLINECOUNT, 0, 0) < 20) return 2;
                    /* Force a reachable wrap boundary even when the control's
                     * viewport limits its first visible line below line_count-6. */
                    D32(object, 0x10f0) = 3;
                    for (tick = 0; tick < 45; tick++) SendMessageA(window, WM_TIMER, D32(object, 0x10e8), 0);
                    SendMessageA(window, WM_COMMAND, 0xedc, (LPARAM)GetDlgItem(window, 0xedc));
                    SendMessageA(window, WM_COMMAND, 0xedc, (LPARAM)GetDlgItem(window, 0xedc));
                    SendMessageA(window, WM_TIMER, D32(object, 0x10e8), 0);
                    results[s_side].scroll = D32(object, 0x10f4);
                } else if (s_dialog == 0x1106) {
                    HWND list = GetDlgItem(window, 0xee1);
                    uint32_t count = D32(s_descriptor, 8);
                    SendMessageA(list, LB_SETCURSEL, count ? (s_case / 5 + 1) % count : (WPARAM)-1, 0);
                    SendMessageA(window, WM_COMMAND, MAKEWPARAM(0xee1, LBN_SELCHANGE), (LPARAM)list);
                    if (count) {
                        HWND radio = (HWND)D32(object, 0x17a8);
                        void *checked = (void *)GetProcAddress(ui, "?SetChecked@CSimsRadioButton@@QAEHH@Z");
                        ((int (TC *)(void *, int))checked)(fs98_object(radio), s_case & 1);
                    }
                } else if (s_dialog == 0x14050004) SendMessageA(window, 0x464, 0, 0);
                fs98_pump(); fs98_checkpoint(window, 1);
                if (s_dialog == 0x1106 && s_case % 3 == 0)
                    SendMessageA(window, WM_COMMAND, MAKEWPARAM(0xee1, LBN_DBLCLK), (LPARAM)GetDlgItem(window, 0xee1));
                else SendMessageA(window, WM_COMMAND, s_dialog == 0x14050004 || !(s_case & 1) ? IDOK : IDCANCEL, 0);
                fs98_pump();
                if (!IsWindow(window) || IsWindowVisible(window) || GetWindowLongA(window, GWL_USERDATA) != 1) {
                    fprintf(stderr, "Help close state invalid\n"); return 2;
                }
                if (s_dialog == 0x16070001 && (D32(object, 0x10e8) || D32(object, 0x10ec) || D32(object, 0x110c))) return 2;
                memcpy(results[s_side].descriptor, s_descriptor, sizeof(s_descriptor));
                /* About's descriptor+8 is an expired stack pointer: compare its
                 * publication in the core harness, and its live text above. */
                if (s_dialog == 0x1607) D32(results[s_side].descriptor, 8) = 0;
                strcpy(results[s_side].callbacks, s_callback_log[s_side]);
                results[s_side].opens = s_registry_opens[s_side]; results[s_side].queries = s_registry_queries[s_side];
                results[s_side].attempts = s_timer_attempts[s_side]; results[s_side].kills = s_timer_kills[s_side];
                DestroyWindow(window); fs98_pump(); if (IsWindow(window) || s_graph_child) return 2;
            }
            if (memcmp(&results[0], &results[1], sizeof(results[0]))) {
                fprintf(stderr, "HELP UI DIFFER case=%u descriptor/callback/lifetime state\n", s_case); return 1;
            }
            comparisons++;
        }
        for (s_side = 0; s_side < 2; s_side++) {
            before_destroy[s_side] = s_font_destroys[s_side];
            s_linkages[s_side]->deinit();
            if (!FreeLibrary(modules[s_side])) return 2;
            after_destroy[s_side] = s_font_destroys[s_side] - before_destroy[s_side];
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (const char *)modules[s_side], &s_current_module)) return 2;
        }
        if (after_destroy[0] != after_destroy[1] || after_destroy[0] != 1) {
            fprintf(stderr, "HELP UI DIFFER unload font destruction %u/%u\n", after_destroy[0], after_destroy[1]); return 1;
        }
    }
    fs98_patch_code(s_font_destroy, s_font_original);
    printf("Help real MFC/SIMSUI: %u public-factory/message cases over 2 load/unload cycles, 0 differ\n", comparisons);
    return 0;
}
