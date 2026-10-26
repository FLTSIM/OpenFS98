/* MPCHAT against retail with actual USER32/GDI/RichEdit windows. Only the
 * simulator's numeric imports are fixtures; no Windows imports are hooked.
 * The two DLLs own FS98MPCHAT in turn so each live WndProc is verified. */
#include <windows.h>
#include <richedit.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SC __attribute__((stdcall))
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define STATE_CAPACITY 262144
#define LOG_CAPACITY 65536
#define MAX_CHECKPOINTS 32

typedef void (SC *callback_fn)(uint32_t, uint32_t);
struct native_import { uint32_t id; void *link; };
struct native_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, uint32_t); uint32_t flags; };
struct native_linkage {
    uint32_t id; uint32_t (SC *init)(void), (SC *deinit)(void); void *reserved;
    uint32_t flags; struct native_dialog *dialogs;
};
struct player_list { uint32_t count; uint8_t *records; };
struct capture {
    char state[STATE_CAPACITY];
    uint8_t *pixels;
    uint32_t bytes, width, height, state_bytes;
};
static HMODULE s_modules[2];
static struct native_linkage *s_linkages[2];
static uint8_t s_host_services[0x320], s_global[16], s_chain[0x24], s_multip[0x40], s_winmsg[0x28];
static HWND s_main, s_window;
static HMENU s_menu;
static callback_fn s_callbacks[32];
static uint8_t s_records[3][4][0x100];
static char s_names[3][4][128];
static struct player_list s_lists[3];
static char *s_strings[3];
static uint32_t s_side, s_case, s_cycle, s_phase, s_log_length, s_state_length;
static char s_log[LOG_CAPACITY], s_state[STATE_CAPACITY];
static uint32_t s_sends, s_bars, s_posts, s_registrations;
static uint32_t s_sent_bytes;
static uint8_t s_sent[0x408];
static int s_running;
static struct capture s_captures[MAX_CHECKPOINTS], s_current;
static uint32_t s_comparisons;

static void fail(const char *format, ...)
{
    va_list arguments;
    fprintf(stderr, "MPCHAT UI FIXTURE case=%u side=%u phase=%u: ", s_case, s_side, s_phase);
    va_start(arguments, format); vfprintf(stderr, format, arguments); va_end(arguments);
    fputc('\n', stderr); exit(2);
}
static void log_call(const char *format, ...)
{
    int length;
    va_list arguments;
    va_start(arguments, format);
    length = vsnprintf(s_log + s_log_length, sizeof(s_log) - s_log_length, format, arguments);
    va_end(arguments);
    if (length < 0 || (uint32_t)length >= sizeof(s_log) - s_log_length) fail("host log overflow");
    s_log_length += length;
}
static void state_put(const char *format, ...)
{
    int length;
    va_list arguments;
    va_start(arguments, format);
    length = vsnprintf(s_state + s_state_length, sizeof(s_state) - s_state_length, format, arguments);
    va_end(arguments);
    if (length < 0 || (uint32_t)length >= sizeof(s_state) - s_state_length) fail("state overflow");
    s_state_length += length;
}
static uint32_t window_id(HWND window)
{
    if (!window) return 0;
    if (window == s_main) return 1;
    if (window == s_window) return 2;
    if (GetParent(window) == s_window) return GetDlgCtrlID(window);
    return 0xffffffff;
}
static void dispatch(uint32_t list, uint32_t message, uint32_t argument)
{
    if (list >= 32 || !s_callbacks[list]) fail("missing CHAIN list %x", list);
    s_callbacks[list](message, argument);
}
static void pump(void)
{
    MSG message;
    uint32_t count = 0, settled = 0;
    /* Activation/focus messages can arrive from Wine's window server after
     * the first queue drain. Wait for two quiet intervals before observing. */
    while (settled < 2) {
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
            if (++count > 2000) fail("message pump did not drain");
            TranslateMessage(&message); DispatchMessageA(&message);
        }
        if (MsgWaitForMultipleObjects(0, 0, FALSE, 10, QS_ALLINPUT) == WAIT_TIMEOUT) settled++;
        else settled = 0;
    }
}
static LRESULT CALLBACK main_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (message == WM_COMMAND && LOWORD(w_param) == 0x1183 && s_running) {
        log_call("main.command 1183\n");
        dispatch(0xb, 0x1183, l_param); return 0;
    }
    return DefWindowProcA(window, message, w_param, l_param);
}
static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "MPCHAT UI FAULT case=%u side=%u phase=%u code=%08lx pc=%p\n",
        s_case, s_side, s_phase, exception->ExceptionRecord->ExceptionCode,
        exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(3); return EXCEPTION_EXECUTE_HANDLER;
}
static void unknown(void) { fail("unexpected simulator service at %p", __builtin_return_address(0)); }
static uint32_t SC host_module_enter(void *linkage, uint32_t *out)
{
    if (linkage != s_linkages[s_side]) fail("wrong module linkage");
    *out = (uint32_t)s_modules[s_side]; s_registrations++;
    log_call("module.enter\n"); return 0;
}
static uint32_t SC host_module_leave(uint32_t module)
{
    if (module != (uint32_t)s_modules[s_side] || s_registrations != 1) fail("module registration mismatch");
    s_registrations--; log_call("module.leave\n"); return 0;
}
static uint32_t SC host_window(uint32_t *window, uint32_t *instance)
{
    *window = (uint32_t)s_main; *instance = (uint32_t)GetModuleHandleA(0);
    log_call("main.window\n"); return *instance;
}
static uint32_t SC host_string(uint32_t id, char **out)
{
    const char *texts[] = {"Multiplayer Chat fixture", "Send", "Unknown player"};
    uint32_t index = id - 0x9400, length;
    if (index >= 3 || s_strings[index]) fail("invalid string request %x", id);
    length = strlen(texts[index]); *out = malloc(length + 1);
    if (!*out) fail("allocation failed");
    memcpy(*out, texts[index], length + 1); s_strings[index] = *out;
    log_call("string %x\n", id); return length;
}
static uint32_t SC host_free(void **out)
{
    uint32_t index;
    for (index = 0; index < 3; index++) if (*out && *out == s_strings[index]) {
        log_call("free %u\n", index); free(*out); s_strings[index] = 0; *out = 0; return 0;
    }
    fail("unrecognized string allocation"); return 0;
}
static char *SC host_copy(char *out, const char *text)
{ log_call("copy [%s]\n", text); return strcpy(out, text); }
static char *SC host_cat(char *out, const char *text)
{ log_call("cat [%s]\n", text); return strcat(out, text); }
static uint32_t SC host_length(const char *text)
{ uint32_t length = strlen(text); log_call("length %u\n", length); return length; }
static uint32_t SC host_dialog(uint32_t id, uint32_t parameter)
{
    struct native_dialog *dialog = s_linkages[s_side]->dialogs;
    if (id != 0xff00 || dialog->id != id || dialog->flags != 0xc || dialog[1].id || parameter || IsWindow(s_window))
        fail("invalid dialog registry request");
    log_call("dialog %x flags=%x\n", id, dialog->flags);
    s_window = (HWND)dialog->create((uint32_t)s_main, parameter);
    if (!IsWindow(s_window)) fail("factory returned no window (%lu)", GetLastError());
    ShowWindow(s_window, SW_SHOW); UpdateWindow(s_window); return 0;
}
static uint32_t SC host_menu_add(uint32_t parent, uint32_t id, uint32_t text, uint32_t flags)
{
    log_call("menu.add %x %x %x %x\n", parent, id, text, flags);
    if (!AppendMenuA(s_menu, MF_STRING, id, "Chat fixture")) fail("menu add");
    return 0;
}
static uint32_t SC host_menu_remove(uint32_t id)
{ log_call("menu.remove %x\n", id); if (!DeleteMenu(s_menu, id, MF_BYCOMMAND)) fail("menu remove"); return 0; }
static uint32_t SC host_menu_state(uint32_t id, uint32_t flags)
{
    log_call("menu.state %x %x\n", id, flags);
    CheckMenuItem(s_menu, id, MF_BYCOMMAND | (flags & 1 ? MF_CHECKED : MF_UNCHECKED)); return 0;
}
static uint32_t SC host_post(uint32_t command, uint32_t parameter)
{
    log_call("post %x %x\n", command, parameter); s_posts++;
    return PostMessageA(s_main, WM_COMMAND, command, parameter);
}
static uint32_t SC chain_add(uint32_t list, callback_fn function)
{
    HMODULE owner = 0;
    if (list >= 32 || s_callbacks[list] || !function) fail("invalid CHAIN add %x", list);
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)function, &owner);
    if (owner != s_modules[s_side]) fail("CHAIN callback belongs to another module");
    s_callbacks[list] = function; log_call("chain.add %x\n", list); return 0;
}
static uint32_t SC chain_remove(uint32_t list, callback_fn function)
{
    if (list >= 32 || s_callbacks[list] != function) fail("invalid CHAIN remove %x", list);
    s_callbacks[list] = 0; log_call("chain.remove %x\n", list); return 0;
}
static uint32_t SC multiplayer_list(void)
{ log_call("players.list\n"); return (uint32_t)&s_lists[0]; }
static uint32_t SC multiplayer_player(uint32_t id)
{
    uint32_t group, index;
    log_call("player.lookup %u\n", id);
    for (group = 0; group < 3; group++) for (index = 0; index < s_lists[group].count; index++)
        if (D32(s_lists[group].records + index * 0x100, 4) == id) return (uint32_t)(s_lists[group].records + index * 0x100);
    return 0;
}
static uint32_t SC multiplayer_local(void)
{ log_call("player.local %u\n", s_case & 1); return s_case & 1 ? 0 : (uint32_t)s_records[0][0]; }
static uint32_t SC multiplayer_send(uint32_t to, const void *payload, uint32_t size, uint32_t flags)
{
    if (size > sizeof(s_sent) || size < 9) fail("packet size outside fixture");
    s_sends++; s_sent_bytes = size; memcpy(s_sent, payload, size);
    log_call("send to=%u size=%u flags=%u type=%x length=%u text=[%s]\n",
        to, size, flags, D32(payload, 0), D32(payload, 4), (const char *)payload + 8);
    return 0;
}
static uint32_t SC winmsg_show(const char *text, uint32_t slot, uint32_t time, uint32_t append)
{ s_bars++; log_call("winmsg %u %u %u [%s]\n", slot, time, append, text); return 0x5150 + append; }
static void setup_services(void)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_host_services); index += 4) PTR(s_host_services, index) = unknown;
    PTR(s_host_services, 0x3c) = host_post; PTR(s_host_services, 0x54) = host_window; PTR(s_host_services, 0x110) = host_dialog;
    PTR(s_host_services, 0x214) = host_free; PTR(s_host_services, 0x244) = host_menu_add;
    PTR(s_host_services, 0x254) = host_menu_state; PTR(s_host_services, 0x25c) = host_menu_remove;
    PTR(s_host_services, 0x298) = host_module_enter; PTR(s_host_services, 0x29c) = host_module_leave;
    PTR(s_host_services, 0x2a4) = host_string; PTR(s_host_services, 0x2a8) = host_copy;
    PTR(s_host_services, 0x2b0) = host_cat; PTR(s_host_services, 0x2b8) = host_length;
    PTR(s_chain, 0x18) = chain_add; PTR(s_chain, 0x1c) = chain_remove;
    PTR(s_multip, 0x20) = multiplayer_send; PTR(s_multip, 0x24) = multiplayer_list;
    PTR(s_multip, 0x28) = multiplayer_player; PTR(s_multip, 0x2c) = multiplayer_local;
    PTR(s_winmsg, 0x18) = winmsg_show;
}
static HMODULE load_module(const char *path)
{
    char full[MAX_PATH]; HMODULE module;
    GetFullPathNameA(path, MAX_PATH, full, 0); module = LoadLibraryA(full);
    if (!module) fail("LoadLibrary %s error=%lu", path, GetLastError());
    return module;
}
static void bind_module(uint32_t side)
{
    struct native_import *imports = (void *)GetProcAddress(s_modules[side], "ImportTable");
    struct native_linkage *linkage = (void *)GetProcAddress(s_modules[side], "Linkage");
    const uint32_t ids[] = {1, 2, 4, 35, 22, 0};
    void *tables[] = {s_host_services, s_global, s_chain, s_multip, s_winmsg};
    uint32_t index;
    if (!imports || !linkage || linkage->id != 0xffffffff || (uint8_t *)linkage != (uint8_t *)imports + 0x48)
        fail("not the reference MPCHAT ABI");
    s_linkages[side] = linkage;
    for (index = 0; index < 6; index++) {
        if (imports[index].id != ids[index]) fail("unexpected numeric import");
        if (index < 5) imports[index].link = tables[index];
    }
    if (!linkage->dialogs || linkage->dialogs->id != 0xff00 || linkage->dialogs->flags != 0xc || linkage->dialogs[1].id)
        fail("unexpected MPCHAT registry");
}
static void fixture(void)
{
    uint32_t group, index;
    memset(s_records, 0, sizeof(s_records)); memset(s_names, 0, sizeof(s_names));
    for (group = 0; group < 3; group++) {
        s_lists[group].count = group ? 1 + (s_case + group) % 3 : 2 + s_case % 3;
        s_lists[group].records = s_records[group][0];
        for (index = 0; index < 4; index++) {
            snprintf(s_names[group][index], sizeof(s_names[0][0]), "Pilot %u/%u case %u", group, index, s_case);
            PTR(s_records[group][index], 0) = s_names[group][index];
            D32(s_records[group][index], 4) = 100 + group * 10 + index;
        }
    }
    memset(s_callbacks, 0, sizeof(s_callbacks)); memset(s_sent, 0, sizeof(s_sent));
    s_sends = s_bars = s_posts = s_registrations = 0; s_sent_bytes = 0;
    s_log_length = 0; s_log[0] = 0; s_window = 0; s_phase = 0;
    s_menu = CreatePopupMenu(); if (!s_menu) fail("menu creation");
    SetForegroundWindow(s_main); SetFocus(s_main); pump();
}
static void receive_line(uint32_t sender, const char *text, uint32_t type)
{
    struct { uint32_t type, length; char text[1024]; } payload;
    uint32_t arguments[3];
    payload.type = type; payload.length = strlen(text) + 1;
    if (payload.length > sizeof(payload.text)) fail("receive fixture too long");
    memcpy(payload.text, text, payload.length);
    arguments[0] = sender; arguments[1] = (uint32_t)&payload; arguments[2] = payload.length + 8;
    dispatch(0x1d, 5, (uint32_t)arguments);
}
static void send_text(const char *text, int click)
{
    HWND entry = GetDlgItem(s_window, 0x101);
    if (!entry) fail("entry missing");
    SetFocus(entry); SetWindowTextA(entry, text);
    if (click) SendMessageA(GetDlgItem(s_window, 0x103), BM_CLICK, 0, 0);
    else SendMessageA(s_window, WM_COMMAND, MAKEWPARAM(0x103, BN_CLICKED), (LPARAM)GetDlgItem(s_window, 0x103));
    pump();
}
static void capture_pixels(HWND window)
{
    RECT rect;
    BITMAPINFO info;
    HDC source, memory;
    HBITMAP bitmap, previous;
    GUITHREADINFO gui;
    BOOL hidden;
    void *bits;
    GetClientRect(window, &rect);
    if (rect.right <= 0 || rect.bottom <= 0 || rect.right > 1200 || rect.bottom > 900) fail("invalid capture size");
    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    RedrawWindow(window, 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    pump();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = rect.right;
    info.bmiHeader.biHeight = -rect.bottom; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    source = GetDC(window); memory = CreateCompatibleDC(source);
    bitmap = CreateDIBSection(source, &info, DIB_RGB_COLORS, &bits, 0, 0);
    if (!source || !memory || !bitmap) fail("capture allocation");
    previous = SelectObject(memory, bitmap);
    s_current.width = rect.right; s_current.height = rect.bottom; s_current.bytes = rect.right * rect.bottom * 4;
    memset(bits, 0xa5, s_current.bytes);
    /* Hide the actual native caret after repainting has settled. WM_PRINT
     * on this DefDlgProc window paints only its background, omitting the
     * RichEdit children, so compare complete client screen pixels instead. */
    memset(&gui, 0, sizeof(gui)); gui.cbSize = sizeof(gui);
    if (!GetGUIThreadInfo(GetCurrentThreadId(), &gui)) fail("GUI thread info");
    hidden = HideCaret(gui.hwndCaret);
    if (!BitBlt(memory, 0, 0, rect.right, rect.bottom, source, 0, 0, SRCCOPY)) fail("capture BitBlt");
    GdiFlush();
    s_current.pixels = realloc(s_current.pixels, s_current.bytes);
    if (!s_current.pixels) fail("pixel allocation");
    memcpy(s_current.pixels, bits, s_current.bytes);
    SelectObject(memory, previous); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window, source);
    if (hidden) ShowCaret(gui.hwndCaret);
}
static BOOL CALLBACK capture_control(HWND window, LPARAM root)
{
    RECT rect;
    char class_name[128], text[4096];
    GetClassNameA(window, class_name, sizeof(class_name)); GetWindowTextA(window, text, sizeof(text));
    GetWindowRect(window, &rect); MapWindowPoints(0, (HWND)root, (POINT *)&rect, 2);
    state_put("control %d class=%s text=[%s] rect=%ld,%ld,%ld,%ld visible=%d enabled=%d style=%08lx ex=%08lx\n",
        GetDlgCtrlID(window), class_name, text, rect.left, rect.top, rect.right, rect.bottom,
        IsWindowVisible(window), IsWindowEnabled(window), GetWindowLongA(window, GWL_STYLE), GetWindowLongA(window, GWL_EXSTYLE));
    if (!lstrcmpiA(class_name, "ListBox")) {
        int count = SendMessageA(window, LB_GETCOUNT, 0, 0), index;
        state_put("list count=%d selected=%ld\n", count, SendMessageA(window, LB_GETCURSEL, 0, 0));
        for (index = 0; index < count; index++) {
            if (SendMessageA(window, LB_GETTEXTLEN, index, 0) >= (LRESULT)sizeof(text)) fail("list text too long");
            SendMessageA(window, LB_GETTEXT, index, (LPARAM)text); state_put("row %d [%s]\n", index, text);
        }
    }
    return TRUE;
}
static DWORD CALLBACK stream_rtf(DWORD_PTR cookie, BYTE *buffer, LONG count, LONG *written)
{
    (void)cookie;
    if (count < 0 || (uint32_t)count >= sizeof(s_state) - s_state_length) fail("RTF exceeds capture");
    memcpy(s_state + s_state_length, buffer, count); s_state_length += count; s_state[s_state_length] = 0;
    *written = count; return 0;
}
static void capture_rich(HWND rich)
{
    char text[65536];
    CHARRANGE selection, probe;
    SCROLLINFO scroll;
    EDITSTREAM stream;
    uint32_t index, probes = 0, length;
    int first_line = SendMessageA(rich, EM_GETFIRSTVISIBLELINE, 0, 0);
    SendMessageA(rich, EM_EXGETSEL, 0, (LPARAM)&selection);
    memset(&scroll, 0, sizeof(scroll)); scroll.cbSize = sizeof(scroll); scroll.fMask = SIF_ALL;
    GetScrollInfo(rich, SB_VERT, &scroll);
    length = GetWindowTextA(rich, text, sizeof(text));
    state_put("rich length=%u text=[%s] selection=%ld,%ld scroll=%d,%d,%u,%d first=%ld\n", length, text,
        selection.cpMin, selection.cpMax, scroll.nMin, scroll.nMax, scroll.nPage, scroll.nPos,
        SendMessageA(rich, EM_GETFIRSTVISIBLELINE, 0, 0));
    /* Query actual native formatting at paragraph boundaries. Preserve the
     * user's selection; the module itself must restore it after an append. */
    for (index = 0; index < length && probes < 128; index++) {
        CHARFORMATA character;
        PARAFORMAT2 paragraph;
        if (index && text[index - 1] != '\n' && text[index - 1] != '\r') continue;
        memset(&character, 0, sizeof(character)); character.cbSize = sizeof(character);
        memset(&paragraph, 0, sizeof(paragraph)); paragraph.cbSize = sizeof(paragraph);
        probe.cpMin = index; probe.cpMax = index + 1;
        SendMessageA(rich, EM_EXSETSEL, 0, (LPARAM)&probe);
        SendMessageA(rich, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&character);
        SendMessageA(rich, EM_GETPARAFORMAT, 0, (LPARAM)&paragraph);
        state_put("format %u color=%08lx effects=%08lx height=%ld face=[%s] indent=%ld offset=%ld\n",
            index, character.crTextColor, character.dwEffects, character.yHeight, character.szFaceName,
            paragraph.dxStartIndent, paragraph.dxOffset);
        probes++;
    }
    SendMessageA(rich, EM_EXSETSEL, 0, (LPARAM)&selection);
    /* Selecting text can move the native viewport. Restore its scroll line,
     * including the state being inspected for the next incoming message. */
    {
        int line = SendMessageA(rich, EM_GETFIRSTVISIBLELINE, 0, 0);
        SendMessageA(rich, EM_LINESCROLL, 0, first_line - line);
    }
    memset(&stream, 0, sizeof(stream)); stream.pfnCallback = stream_rtf;
    state_put("rtf=["); SendMessageA(rich, EM_STREAMOUT, SF_RTF, (LPARAM)&stream);
    if (stream.dwError) fail("native RTF stream failed");
    state_put("]\n");
}
static void save_pixels(const struct capture *capture, const char *name)
{
    BITMAPFILEHEADER header = {0};
    BITMAPINFOHEADER info = {0};
    FILE *file;
    if (!capture->bytes) { DeleteFileA(name); return; }
    header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + capture->bytes;
    info.biSize = sizeof(info); info.biWidth = capture->width; info.biHeight = -(int32_t)capture->height;
    info.biPlanes = 1; info.biBitCount = 32;
    file = fopen(name, "wb"); if (!file) fail("BMP output");
    fwrite(&header, sizeof(header), 1, file); fwrite(&info, sizeof(info), 1, file);
    fwrite(capture->pixels, capture->bytes, 1, file); fclose(file);
}
static void compare_checkpoint(const char *label)
{
    struct capture *expected;
    uint32_t bytes, index;
    HWND focus;
    if (s_phase >= MAX_CHECKPOINTS) fail("too many checkpoints");
    pump(); focus = GetFocus(); s_state_length = 0; s_state[0] = 0;
    state_put("phase=%u %s window=%d visible=%d focus=%u menu=%x posts=%u sends=%u bars=%u packet=%u\n",
        s_phase, label, IsWindow(s_window), IsWindowVisible(s_window), window_id(focus),
        GetMenuState(s_menu, 0x1183, MF_BYCOMMAND), s_posts, s_sends, s_bars, s_sent_bytes);
    if (IsWindow(s_window)) {
        RECT rect;
        GetClientRect(s_window, &rect);
        state_put("window client=%ld,%ld style=%08lx ex=%08lx owner=%d default=%ld\n", rect.right, rect.bottom,
            GetWindowLongA(s_window, GWL_STYLE), GetWindowLongA(s_window, GWL_EXSTYLE),
            GetWindow(s_window, GW_OWNER) == s_main, SendMessageA(s_window, DM_GETDEFID, 0, 0));
        EnumChildWindows(s_window, capture_control, (LPARAM)s_window);
        capture_rich(GetDlgItem(s_window, 0x100));
    }
    state_put("HOST:\n%s", s_log);
    bytes = s_state_length;
    memcpy(s_current.state, s_state, bytes + 1); s_current.state_bytes = bytes;
    s_current.width = s_current.height = s_current.bytes = 0;
    if (IsWindowVisible(s_window)) capture_pixels(s_window);
    expected = &s_captures[s_phase];
    if (!s_side) {
        memcpy(expected->state, s_current.state, bytes + 1); expected->state_bytes = bytes;
        expected->width = s_current.width; expected->height = s_current.height; expected->bytes = s_current.bytes;
        if (s_current.bytes) {
            expected->pixels = realloc(expected->pixels, s_current.bytes);
            if (!expected->pixels) fail("checkpoint allocation");
            memcpy(expected->pixels, s_current.pixels, s_current.bytes);
        }
        if (!s_case) {
            char path[MAX_PATH]; FILE *file;
            snprintf(path, sizeof(path), "bin/mpchat-ui/checkpoint-%02u.txt", s_phase);
            file = fopen(path, "wb"); if (!file) fail("checkpoint file");
            fwrite(s_state, bytes, 1, file); fclose(file);
            snprintf(path, sizeof(path), "bin/mpchat-ui/checkpoint-%02u.bmp", s_phase);
            save_pixels(&s_current, path);
        }
    } else if ((expected->state_bytes != bytes || memcmp(expected->state, s_current.state, bytes)) || expected->width != s_current.width ||
        expected->height != s_current.height || expected->bytes != s_current.bytes ||
        (s_current.bytes && memcmp(expected->pixels, s_current.pixels, s_current.bytes))) {
        char path[MAX_PATH]; FILE *file;
        fprintf(stderr, "MPCHAT UI DIFFER cycle=%u case=%u phase=%u %s state=%d pixels=%u/%u\n",
            s_cycle, s_case, s_phase, label, !!(expected->state_bytes != bytes || memcmp(expected->state, s_current.state, bytes)), expected->bytes, s_current.bytes);
        for (index = 0; index < s_current.bytes && index < expected->bytes; index++) if (expected->pixels[index] != s_current.pixels[index]) {
            fprintf(stderr, "first pixel difference byte=%u retail=%u ours=%u\n", index, expected->pixels[index], s_current.pixels[index]); break;
        }
        snprintf(path, sizeof(path), "bin/mpchat-ui/differ-retail.txt"); file = fopen(path, "wb");
        if (file) { fwrite(expected->state, expected->state_bytes, 1, file); fclose(file); }
        snprintf(path, sizeof(path), "bin/mpchat-ui/differ-ours.txt"); file = fopen(path, "wb");
        if (file) { fwrite(s_current.state, bytes, 1, file); fclose(file); }
        save_pixels(expected, "bin/mpchat-ui/differ-retail.bmp");
        save_pixels(&s_current, "bin/mpchat-ui/differ-ours.bmp");
        exit(1);
    } else s_comparisons++;
    s_phase++;
}
static void verify_dispatch(void)
{
    WNDCLASSA info;
    HMODULE owner = 0;
    HWND rich = GetDlgItem(s_window, 0x100);
    if (!GetClassInfoA(s_modules[s_side], "FS98MPCHAT", &info) || !rich ||
        !GetDlgItem(s_window, 0x101) || !GetDlgItem(s_window, 0x102) || !GetDlgItem(s_window, 0x103)) fail("class or controls missing");
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)GetWindowLongA(s_window, GWL_WNDPROC), &owner);
    if (owner != s_modules[s_side]) fail("live WndProc belongs to another implementation");
    if (!GetModuleHandleA("RICHED32.DLL")) fail("native RichEdit dependency missing");
}
static void run_script(void)
{
    uint32_t index, answer, initial_phase_count;
    CHARRANGE selection;
    HWND rich;
    WNDCLASSA info;
    char long_text[1300];
    fixture();
    if (GetClassInfoA(s_modules[s_side], "FS98MPCHAT", &info)) fail("class retained before init");
    if (s_linkages[s_side]->init()) fail("module init failed");
    dispatch(0x1d, 1, 0); s_running = 1; pump(); verify_dispatch();
    dispatch(0x14, 0x40, 0); compare_checkpoint("session-created");
    /* A secondary list, then a renamed/replaced primary list. */
    dispatch(0x1d, 4, (uint32_t)&s_lists[1]);
    snprintf(s_names[2][0], sizeof(s_names[2][0]), "Renamed pilot case %u", s_case);
    dispatch(0x1d, 3, (uint32_t)&s_lists[2]);
    compare_checkpoint("player-lists");
    receive_line(120, "A native incoming line", 0x1020);
    receive_line(999, "An unknown sender", 0x1020);
    receive_line(120, "Ignored payload type", 0x1021);
    compare_checkpoint("received-lines");
    send_text("", s_case & 1); compare_checkpoint("empty-send-focus");
    send_text("A typed local line", s_case & 1); compare_checkpoint("send-packet-focus");
    rich = GetDlgItem(s_window, 0x100);
    SendMessageA(rich, EM_SETSEL, 2, 8);
    receive_line(120, "Selection must survive this append", 0x1020);
    SendMessageA(rich, EM_EXGETSEL, 0, (LPARAM)&selection);
    log_call("selection.after.receive %ld,%ld\n", selection.cpMin, selection.cpMax);
    compare_checkpoint("selection-preserved");
    /* The real EN_SETFOCUS handler drops a transcript selection. */
    SetFocus(rich); pump(); compare_checkpoint("transcript-focus");
    answer = 0; dispatch(0xc, 0x10171, (uint32_t)&answer);
    log_call("command.answer %u\n", answer); compare_checkpoint("key-focus-entry");
    SetWindowPos(s_window, 0, 20, 20, 480 + s_case % 4 * 65, 260 + s_case % 3 * 45,
        SWP_NOZORDER | SWP_NOACTIVATE); pump(); compare_checkpoint("resized-layout");
    for (index = 0; index < 24; index++) {
        char line[160];
        snprintf(line, sizeof(line), "Transcript line %u: native scrolling fixture, case %u", index, s_case);
        receive_line(index & 1 ? 999 : 120, line, 0x1020);
    }
    compare_checkpoint("scroll-at-bottom");
    SendMessageA(rich, WM_VSCROLL, SB_TOP, 0);
    SendMessageA(rich, EM_SETSEL, 1, 5);
    receive_line(120, "Reading old messages while a new line arrives", 0x1020);
    compare_checkpoint("scroll-and-selection-away-from-bottom");
    dispatch(0xb, 0x1183, 0); dispatch(0x14, 0x40, 0);
    receive_line(999, "Hidden window must notify WINMSG", 0x1020);
    compare_checkpoint("hidden-receive-winmsg");
    answer = 0; dispatch(0xc, 0x10170, (uint32_t)&answer); log_call("command.answer %u\n", answer);
    dispatch(0x14, 0x40, 0); compare_checkpoint("key-toggle-visible");
    SendMessageA(s_window, WM_CLOSE, 0, 0); pump(); dispatch(0x14, 0x40, 0);
    compare_checkpoint("close-posts-menu-command");
    answer = 0; dispatch(0xc, 0x10171, (uint32_t)&answer); log_call("command.answer %u\n", answer);
    memset(long_text, 'a' + s_case % 26, sizeof(long_text) - 1); long_text[sizeof(long_text) - 1] = 0;
    SendMessageA(rich, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    send_text(long_text, 1); compare_checkpoint("send-input-limit");
    {
        HWND old_window = s_window;
        dispatch(0x1d, 2, 0); s_running = 0; pump();
        if (IsWindow(old_window)) fail("session window not destroyed");
    }
    compare_checkpoint("session-ended");
    /* A second session in the same module verifies stale control handles,
     * player lists and previous local-player state are not reused. */
    dispatch(0x1d, 1, 0); s_running = 1; pump(); verify_dispatch();
    receive_line(100, "A new session after teardown", 0x1020); compare_checkpoint("session-restarted");
    dispatch(0x1d, 2, 0); s_running = 0; pump();
    s_linkages[s_side]->deinit();
    initial_phase_count = 0;
    for (index = 0; index < 32; index++) if (s_callbacks[index]) initial_phase_count++;
    log_call("cleanup callbacks=%u strings=%u registration=%u\n", initial_phase_count,
        !!s_strings[0] + !!s_strings[1] + !!s_strings[2], s_registrations);
    if (GetClassInfoA(s_modules[s_side], "FS98MPCHAT", &info)) log_call("cleanup class-retained\n");
    compare_checkpoint("module-deinitialized");
    if (initial_phase_count || s_strings[0] || s_strings[1] || s_strings[2] || s_registrations ||
        GetClassInfoA(s_modules[s_side], "FS98MPCHAT", &info)) fail("module cleanup incomplete");
    if (GetMenuItemCount(s_menu)) fail("session menu retained");
    DestroyMenu(s_menu); s_menu = 0;
}
int main(int argc, char **argv)
{
    WNDCLASSA info;
    uint32_t cases = argc > 2 ? strtoul(argv[2], 0, 0) : 24;
    const char *replacement = argc > 1 ? argv[1] : "bin/mpchat-ui/MPCHAT_ours.dll";
    if (!cases || cases > 1000) return 2;
    setvbuf(stdout, 0, _IONBF, 0); SetUnhandledExceptionFilter(fault);
    setup_services();
    memset(&info, 0, sizeof(info)); info.lpfnWndProc = main_proc;
    info.hInstance = GetModuleHandleA(0); info.lpszClassName = "OpenFS98MPChatOwner";
    info.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    if (!RegisterClassA(&info)) fail("owner class registration");
    s_main = CreateWindowExA(0, info.lpszClassName, "OpenFS98 MPCHAT native owner",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 1000, 700, 0, 0, info.hInstance, 0);
    if (!s_main) fail("owner creation");
    for (s_cycle = 0; s_cycle < 2; s_cycle++) {
        s_modules[0] = load_module("orig/Modules/mpchat.DLL"); s_modules[1] = load_module(replacement);
        if (s_modules[0] == s_modules[1]) fail("comparison requires two implementations");
        bind_module(0); bind_module(1);
        for (s_case = 0; s_case < cases; s_case++) for (s_side = 0; s_side < 2; s_side++) {
            printf("MPCHAT UI cycle=%u case=%u side=%u\n", s_cycle, s_case, s_side);
            run_script();
        }
        for (s_side = 0; s_side < 2; s_side++) {
            HMODULE remaining = 0;
            if (!FreeLibrary(s_modules[s_side])) fail("DLL unload failed");
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (const char *)s_modules[s_side], &remaining)) fail("DLL still mapped");
        }
    }
    DestroyWindow(s_main);
    printf("MPCHAT real USER32/GDI/RichEdit: %u scripts, %u checkpoints, 2 DLL load/unload cycles, 0 differ\n",
        cases * 2, s_comparisons);
    return 0;
}
