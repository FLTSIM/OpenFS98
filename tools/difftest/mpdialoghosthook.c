/* Thread-local HOST/MULTIP integration trace for a private Wine game copy. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define HOSTHOOK_MAX_BREAKPOINTS 64
#define HOSTHOOK_PAGE_COUNT 5

enum hosthook_break_kind {
    HOSTHOOK_BP_OTHER,
    HOSTHOOK_BP_LOADLIBRARY_ENTRY,
    HOSTHOOK_BP_LOADLIBRARY_RETURN,
    HOSTHOOK_BP_FLOW_FACTORY,
    HOSTHOOK_BP_FLOW_RETURN,
    HOSTHOOK_BP_OWNER_DIALOG_ENTRY,
    HOSTHOOK_BP_OWNER_HOST_RETURN,
    HOSTHOOK_BP_OWNER_CLEAR,
    HOSTHOOK_BP_OWNER_CLEAR_RETURN,
    HOSTHOOK_BP_OWNER_FREE,
    HOSTHOOK_BP_OWNER_FREE_RETURN,
    HOSTHOOK_BP_MULTIP_CALLBACK,
    HOSTHOOK_BP_MULTIP_RUNTIME,
    HOSTHOOK_BP_MULTIP_RUNTIME_RETURN,
    HOSTHOOK_BP_POST_NCDESTROY,
    HOSTHOOK_BP_DELETING_DESTRUCTOR,
    HOSTHOOK_BP_OTHER_DIALOG_FACTORY,
    HOSTHOOK_BP_OTHER_DIALOG_RETURN
};

struct hosthook_breakpoint {
    DWORD address;
    BYTE original;
    BOOL armed;
    enum hosthook_break_kind kind;
    const char *label;
};

static HWND s_main_window;
static HWND s_created_pages[HOSTHOOK_PAGE_COUNT];
static unsigned s_created_page_count;
static DWORD s_flow_descriptor;
static DWORD s_flow_array;
static DWORD s_flow_objects;
static DWORD s_flow_object_addresses[HOSTHOOK_PAGE_COUNT];
static DWORD s_flow_object_hwnds[HOSTHOOK_PAGE_COUNT];
static DWORD s_mp_base;
static DWORD s_multip_base;
static BOOL s_owner_return_hook_set;
static DWORD s_mp_array_offset;
static DWORD s_mp_objects_offset;
static const char *s_mp_layout;
static BOOL s_flow_snapshot_done;
static BOOL s_flow_snapshot_deferred_logged;
static volatile LONG s_callback_wrapped;
static DWORD s_callback_descriptor;
static DWORD s_callback_offsets[10] = {
    0x04, 0x08, 0x0c, 0x10, 0x44, 0x94, 0x9c, 0xa0, 0xa4, 0x98
};
static DWORD s_original_callbacks[10];
static DWORD *s_callback_slots[10];
static DWORD *s_postmessage_slot;
static DWORD s_original_postmessage;
static volatile LONG s_postmessage_patched;
static volatile LONG s_mp_probes_installed;
static volatile LONG s_multip_probes_installed;
static volatile LONG s_loader_probe_installed;
static struct hosthook_breakpoint s_breakpoints[HOSTHOOK_MAX_BREAKPOINTS];
static volatile LONG s_step_active;
static DWORD s_step_address;
static DWORD s_step_thread;
static PVOID s_vectored_handler;
static volatile LONG s_owner_clear_depth;
static DWORD s_pending_free_slot;
static DWORD s_other_dialog_descriptor;
static const char *s_other_dialog_label;
static HWND s_other_dialog_window;

#define HOSTHOOK_QUIESCE_MESSAGE (WM_APP + 0x384)

static void hosthook_install_runtime_probes(void);
static void hosthook_install_loader_probe(void);
static void hosthook_install_mp_probes(HMODULE module);
static void hosthook_install_multip_probes(HMODULE module);
static void hosthook_restore_runtime_patches(void);
static void hosthook_snapshot_flow(void);
static void hosthook_log_flow_visibility(const char *stage);
static void hosthook_install_descriptor_callback_probes(DWORD descriptor);
static void hosthook_trace_owner_return(CONTEXT *context);
static void hosthook_restore_callbacks(void);
static void hosthook_restore_one_breakpoint(struct hosthook_breakpoint *bp);
static void hosthook_log_multip_state(const char *stage);
static void hosthook_rearm_module_breakpoints(DWORD base, DWORD size);
static BOOL hosthook_write_protected(void *address, const void *bytes, SIZE_T size);
static BOOL WINAPI hosthook_postmessage(HWND window, UINT message,
    WPARAM wparam, LPARAM lparam);

static void hosthook_log(const char *format, ...)
{
    char line[512];
    DWORD written;
    HANDLE file;
    va_list args;
    va_start(args, format);
    wvsprintfA(line, format, args);
    va_end(args);
    file = CreateFileA("Z:\\tmp\\mpdialog-host-integration.log",
        FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file, line, lstrlenA(line), &written, NULL);
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);
}

static BOOL hosthook_readable(const void *address, SIZE_T size)
{
    MEMORY_BASIC_INFORMATION info;
    DWORD protect;
    if (!address || !VirtualQuery(address, &info, sizeof(info)) ||
        info.State != MEM_COMMIT || size > info.RegionSize -
        ((const BYTE *)address - (const BYTE *)info.BaseAddress)) return FALSE;
    protect = info.Protect & 0xff;
    return protect != PAGE_NOACCESS && (info.Protect & PAGE_GUARD) == 0;
}

static DWORD hosthook_read_dword(const void *address)
{
    return hosthook_readable(address, sizeof(DWORD)) ? *(const DWORD *)address : 0;
}

static void hosthook_read_text(DWORD address, char *buffer, SIZE_T capacity)
{
    SIZE_T index;
    if (!capacity) return;
    for (index = 0; index + 1 < capacity; ++index) {
        BYTE value;
        if (!hosthook_readable((const void *)(uintptr_t)(address + (DWORD)index), 1)) {
            buffer[index] = 0;
            return;
        }
        value = *(const BYTE *)(uintptr_t)(address + (DWORD)index);
        if (!value) break;
        buffer[index] = value >= 0x20 && value <= 0x7e ? (char)value : '?';
    }
    buffer[index] = 0;
}

static DWORD hosthook_module_size(HMODULE module)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module;
    IMAGE_NT_HEADERS32 *nt;
    if (!module || !hosthook_readable(dos, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS32 *)((BYTE *)module + dos->e_lfanew);
    if (!hosthook_readable(nt, sizeof(*nt)) ||
        nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->OptionalHeader.SizeOfImage;
}

static void hosthook_log_descriptor(const char *where, DWORD descriptor)
{
    char callsign[33], host_name[33], pilot_limit[33], observer_limit[33];
    if (!hosthook_readable((const void *)(uintptr_t)descriptor, 0xbc)) {
        hosthook_log("DESC %s ptr=%08lx unreadable", where,
            (unsigned long)descriptor);
        return;
    }
    hosthook_log("DESC %s ptr=%08lx result=%08lx +1c=%08lx mode=%08lx callsign=%08lx cb04=%08lx cb08=%08lx cb0c=%08lx cb10=%08lx hostName=%08lx pilot=%08lx observer=%08lx cb44=%08lx providers=%08lx/%lu selected=%08lx cb94=%08lx sessions=%08lx/%lu selectedSession=%08lx cb98=%08lx cb9c=%08lx cba0=%08lx cba4=%08lx",
        where, (unsigned long)descriptor,
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x00)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x1c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x24)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x2c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x04)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x08)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x0c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x10)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x4c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x60)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x74)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x44)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x84)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x88)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x90)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x94)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xac)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xb0)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xb8)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x98)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x9c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xa0)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xa4)));
    hosthook_read_text(hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x2c)),
        callsign, sizeof(callsign));
    hosthook_read_text(hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x4c)),
        host_name, sizeof(host_name));
    hosthook_read_text(hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x60)),
        pilot_limit, sizeof(pilot_limit));
    hosthook_read_text(hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x74)),
        observer_limit, sizeof(observer_limit));
    hosthook_log("DESC TEXT %s callsign=\"%s\" hostName=\"%s\" pilot=\"%s\" observer=\"%s\"",
        where, callsign, host_name, pilot_limit, observer_limit);
}

static void hosthook_log_region(const char *where, DWORD address)
{
    MEMORY_BASIC_INFORMATION info;
    if (!address || !VirtualQuery((void *)(uintptr_t)address, &info, sizeof(info))) {
        hosthook_log("REGION %s address=%08lx unavailable", where,
            (unsigned long)address);
        return;
    }
    hosthook_log("REGION %s address=%08lx base=%p size=%lu state=%08lx type=%08lx protect=%08lx",
        where, (unsigned long)address, info.BaseAddress,
        (unsigned long)info.RegionSize, (unsigned long)info.State,
        (unsigned long)info.Type, (unsigned long)info.Protect);
}

static void hosthook_log_multip_state(const char *stage)
{
    DWORD directplay, directplay_vtable = 0, global;
    if (!s_multip_base) return;
    directplay = hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbb88));
    if (directplay)
        directplay_vtable = hosthook_read_dword((void *)(uintptr_t)directplay);
    global = hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xb20c));
    hosthook_log("MULTIP STATE stage=%s directplay=%08lx vtable=%08lx queried=%08lx loaded=%08lx host_intent=%08lx connected=%08lx flow_state=%08lx player_count=%08lx players=%08lx",
        stage, (unsigned long)directplay, (unsigned long)directplay_vtable,
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbc44)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbc48)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbca8)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbcac)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xb3a4)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xb860)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xb878)));
    hosthook_log("MULTIP OPTIONS INPUT stage=%s global=%08lx global_764=%08lx global_84c=%08lx enable_word=%08lx",
        stage, (unsigned long)global,
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(global + 0x764)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(global + 0x84c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xb410)));
}

static void hosthook_log_options_block(const char *stage)
{
    DWORD descriptor;
    if (!s_multip_base) return;
    descriptor = s_multip_base + 0xb3e0;
    if (!hosthook_readable((void *)(uintptr_t)descriptor, 0x34)) return;
    hosthook_log("MULTIP 0x110b block stage=%s ptr=%08lx +00=%08lx +04=%08lx +08=%08lx +0c=%08lx +10=%08lx +14=%08lx +18=%08lx +1c=%08lx +20=%08lx +24=%08lx +28=%08lx +2c=%08lx +30=%08lx",
        stage, (unsigned long)descriptor,
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x00)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x04)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x08)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x0c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x10)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x14)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x18)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x1c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x20)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x24)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x28)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x2c)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x30)));
}

static void hosthook_log_function_owner(const char *label, DWORD address)
{
    HMODULE owner = NULL;
    char path[MAX_PATH] = {0};
    if (address && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)(uintptr_t)address, &owner))
        GetModuleFileNameA(owner, path, sizeof(path));
    hosthook_log("MULTIP TARGET OWNER label=%s address=%08lx module=%p path=%s",
        label, (unsigned long)address, owner, path);
}

static void hosthook_log_selected_provider(DWORD descriptor)
{
    DWORD rows = hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x84));
    DWORD index = hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x90));
    DWORD row = rows + index * 28;
    DWORD name = hosthook_read_dword((void *)(uintptr_t)row);
    char provider_name[128] = {0};
    if (rows && hosthook_readable((void *)(uintptr_t)(row + 4), 16))
        hosthook_read_text(name, provider_name, sizeof(provider_name));
    hosthook_log("MULTIP PROVIDER selected index=%lu rows=%08lx row=%08lx name_ptr=%08lx name=\"%s\" guid=%08lx-%04lx-%04lx-%08lx-%08lx",
        (unsigned long)index, (unsigned long)rows, (unsigned long)row,
        (unsigned long)name, provider_name,
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(row + 4)),
        (unsigned long)(hosthook_read_dword((void *)(uintptr_t)(row + 8)) & 0xffff),
        (unsigned long)(hosthook_read_dword((void *)(uintptr_t)(row + 8)) >> 16),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(row + 12)),
        (unsigned long)hosthook_read_dword((void *)(uintptr_t)(row + 16)));
}

static int hosthook_call_callback1(unsigned index, void *descriptor)
{
    typedef int (WINAPI *callback_fn)(void *);
    int result;
    DWORD original = s_original_callbacks[index];
    hosthook_log("MULTIP CALLBACK BEGIN field=+%02lx fn=%08lx descriptor=%p",
        (unsigned long)s_callback_offsets[index], (unsigned long)original,
        descriptor);
    hosthook_log_descriptor("callback-args", (DWORD)(uintptr_t)descriptor);
    result = ((callback_fn)(uintptr_t)original)(descriptor);
    hosthook_log("MULTIP CALLBACK END field=+%02lx result=%08lx",
        (unsigned long)s_callback_offsets[index], (unsigned long)result);
    return result;
}

static int hosthook_call_callback98(void *descriptor, DWORD row_index)
{
    typedef int (WINAPI *callback_fn)(void *, DWORD);
    int result;
    hosthook_log("MULTIP CALLBACK BEGIN field=+98 fn=%08lx descriptor=%p index=%lu",
        (unsigned long)s_original_callbacks[9], descriptor,
        (unsigned long)row_index);
    hosthook_log_descriptor("callback-args", (DWORD)(uintptr_t)descriptor);
    result = ((callback_fn)(uintptr_t)s_original_callbacks[9])(descriptor, row_index);
    hosthook_log("MULTIP CALLBACK END field=+98 result=%08lx",
        (unsigned long)result);
    return result;
}

static int WINAPI hosthook_cb04(void *value) { return hosthook_call_callback1(0, value); }
static int WINAPI hosthook_cb08(void *value) { return hosthook_call_callback1(1, value); }
static int WINAPI hosthook_cb0c(void *value) { return hosthook_call_callback1(2, value); }
static int WINAPI hosthook_cb10(void *value) { return hosthook_call_callback1(3, value); }
static int WINAPI hosthook_cb44(void *value) { return hosthook_call_callback1(4, value); }
static int WINAPI hosthook_cb94(void *value) { return hosthook_call_callback1(5, value); }
static int WINAPI hosthook_cb9c(void *value) { return hosthook_call_callback1(6, value); }
static int WINAPI hosthook_cba0(void *value) { return hosthook_call_callback1(7, value); }
static int WINAPI hosthook_cba4(void *value) { return hosthook_call_callback1(8, value); }
static int WINAPI hosthook_cb98(void *value, DWORD row_index)
{ return hosthook_call_callback98(value, row_index); }

static DWORD hosthook_callback_wrapper(unsigned index)
{
    static DWORD wrappers[10];
    switch (index) {
    case 0: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb04; break;
    case 1: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb08; break;
    case 2: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb0c; break;
    case 3: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb10; break;
    case 4: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb44; break;
    case 5: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb94; break;
    case 6: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb9c; break;
    case 7: wrappers[index] = (DWORD)(uintptr_t)hosthook_cba0; break;
    case 8: wrappers[index] = (DWORD)(uintptr_t)hosthook_cba4; break;
    case 9: wrappers[index] = (DWORD)(uintptr_t)hosthook_cb98; break;
    default: return 0;
    }
    return wrappers[index];
}

static void hosthook_install_callbacks(DWORD descriptor)
{
    unsigned index;
    if (InterlockedCompareExchange(&s_callback_wrapped, 1, 0) != 0) return;
    s_callback_descriptor = descriptor;
    if (!hosthook_readable((void *)(uintptr_t)descriptor, 0xbc)) {
        InterlockedExchange(&s_callback_wrapped, 0);
        return;
    }
    for (index = 0; index < 10; ++index) {
        DWORD *slot = (DWORD *)(uintptr_t)(descriptor + s_callback_offsets[index]);
        DWORD original = *slot;
        DWORD wrapper = hosthook_callback_wrapper(index);
        s_callback_slots[index] = slot;
        s_original_callbacks[index] = original;
        if (!original) continue;
        if (hosthook_write_protected(slot, &wrapper, sizeof(wrapper)))
            hosthook_log("MULTIP CALLBACK WRAP field=+%02lx slot=%p original=%08lx wrapper=%08lx",
                (unsigned long)s_callback_offsets[index], slot,
                (unsigned long)original, (unsigned long)wrapper);
    }
}

static void hosthook_restore_callbacks(void)
{
    unsigned index;
    if (InterlockedExchange(&s_callback_wrapped, 0) == 0) return;
    for (index = 0; index < 10; ++index) {
        DWORD *slot = s_callback_slots[index];
        DWORD wrapper = hosthook_callback_wrapper(index);
        if (slot && hosthook_readable(slot, sizeof(DWORD)) && *slot == wrapper)
            hosthook_write_protected(slot, &s_original_callbacks[index], sizeof(DWORD));
    }
    hosthook_log("MULTIP CALLBACKS RESTORED descriptor=%08lx",
        (unsigned long)s_callback_descriptor);
    memset(s_callback_slots, 0, sizeof(s_callback_slots));
}

static void hosthook_restore_runtime_patches(void)
{
    unsigned index;
    hosthook_restore_callbacks();
    for (index = 0; index < HOSTHOOK_MAX_BREAKPOINTS; ++index)
        hosthook_restore_one_breakpoint(&s_breakpoints[index]);
    if (InterlockedExchange(&s_postmessage_patched, 0) != 0 &&
        s_postmessage_slot &&
        hosthook_readable(s_postmessage_slot, sizeof(DWORD)) &&
        *s_postmessage_slot == (DWORD)(uintptr_t)hosthook_postmessage) {
        DWORD old_protect, ignored;
        if (VirtualProtect(s_postmessage_slot, sizeof(DWORD), PAGE_READWRITE,
                &old_protect)) {
            *s_postmessage_slot = s_original_postmessage;
            FlushInstructionCache(GetCurrentProcess(), s_postmessage_slot,
                sizeof(DWORD));
            VirtualProtect(s_postmessage_slot, sizeof(DWORD), old_protect,
                &ignored);
            hosthook_log("POSTMESSAGE IAT restored slot=%p original=%08lx",
                s_postmessage_slot, (unsigned long)s_original_postmessage);
        } else {
            hosthook_log("POSTMESSAGE IAT restore failed slot=%p error=%lu",
                s_postmessage_slot, (unsigned long)GetLastError());
        }
    }
}

static BOOL hosthook_write_protected(void *address, const void *bytes, SIZE_T size)
{
    DWORD old_protect, ignored;
    if (!address || !VirtualProtect(address, size, PAGE_EXECUTE_READWRITE,
            &old_protect)) return FALSE;
    memcpy(address, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), address, size);
    VirtualProtect(address, size, old_protect, &ignored);
    return TRUE;
}

static BOOL hosthook_add_breakpoint(DWORD address,
    enum hosthook_break_kind kind, const char *label)
{
    unsigned index;
    BYTE original, breakpoint = 0xcc;
    if (!address) return FALSE;
    for (index = 0; index < HOSTHOOK_MAX_BREAKPOINTS; ++index) {
        if (s_breakpoints[index].address == address) {
            struct hosthook_breakpoint *bp = &s_breakpoints[index];
            if (bp->armed && hosthook_readable((void *)(uintptr_t)address, 1) &&
                *(BYTE *)(uintptr_t)address == 0xcc) return TRUE;
            if (!hosthook_readable((void *)(uintptr_t)address, 1)) return FALSE;
            original = *(BYTE *)(uintptr_t)address;
            if (original == 0xcc ||
                !hosthook_write_protected((void *)(uintptr_t)address,
                    &breakpoint, 1)) return FALSE;
            bp->original = original;
            bp->armed = TRUE;
            hosthook_log("BP REARM addr=%08lx label=%s original=%02x",
                (unsigned long)address, bp->label ? bp->label : "?", original);
            return TRUE;
        }
        if (!s_breakpoints[index].address) break;
    }
    if (index == HOSTHOOK_MAX_BREAKPOINTS ||
        !hosthook_readable((const void *)(uintptr_t)address, 1)) return FALSE;
    original = *(BYTE *)(uintptr_t)address;
    if (original == 0xcc ||
        !hosthook_write_protected((void *)(uintptr_t)address, &breakpoint, 1))
        return FALSE;
    s_breakpoints[index].address = address;
    s_breakpoints[index].original = original;
    s_breakpoints[index].armed = TRUE;
    s_breakpoints[index].kind = kind;
    s_breakpoints[index].label = label;
    hosthook_log("BP SET addr=%08lx label=%s original=%02x",
        (unsigned long)address, label ? label : "?", original);
    return TRUE;
}

static void hosthook_rearm_module_breakpoints(DWORD base, DWORD size)
{
    unsigned index;
    DWORD end = base + size;
    for (index = 0; index < HOSTHOOK_MAX_BREAKPOINTS; ++index) {
        struct hosthook_breakpoint *bp = &s_breakpoints[index];
        BYTE current, breakpoint = 0xcc;
        if (!bp->address) continue;
        if (bp->address < base || bp->address >= end ||
            !hosthook_readable((void *)(uintptr_t)bp->address, 1)) continue;
        current = *(BYTE *)(uintptr_t)bp->address;
        if (current == 0xcc) {
            bp->armed = TRUE;
            continue;
        }
        if (hosthook_write_protected((void *)(uintptr_t)bp->address,
                &breakpoint, 1)) {
            bp->original = current;
            bp->armed = TRUE;
            hosthook_log("BP REARM module addr=%08lx label=%s original=%02x",
                (unsigned long)bp->address, bp->label ? bp->label : "?",
                current);
        }
    }
}

static void hosthook_restore_one_breakpoint(struct hosthook_breakpoint *bp)
{
    if (bp->address && bp->armed) {
        hosthook_write_protected((void *)(uintptr_t)bp->address,
            &bp->original, 1);
        bp->armed = FALSE;
    }
}

static const char *hosthook_loaded_library_kind(DWORD name_address)
{
    static char name[MAX_PATH];
    const char *base;
    unsigned index;
    memset(name, 0, sizeof(name));
    for (index = 0; index + 1 < sizeof(name); ++index) {
        const char *source = (const char *)(uintptr_t)(name_address + index);
        if (!hosthook_readable(source, 1)) return NULL;
        name[index] = *source;
        if (!name[index]) break;
    }
    if (index + 1 >= sizeof(name)) return NULL;
    base = strrchr(name, '\\');
    if (!base) base = strrchr(name, '/');
    if (base) ++base;
    else base = name;
    if (lstrcmpiA(base, "MP_DIALOG.DLL") == 0) return "MP_DIALOG.DLL";
    if (lstrcmpiA(base, "MULTIPLAYER.DLL") == 0 ||
        lstrcmpiA(base, "MULTIP.DLL") == 0) return "MULTIPLAYER.DLL";
    return NULL;
}

static LONG CALLBACK hosthook_veh(PEXCEPTION_POINTERS exception)
{
    DWORD exception_code = exception->ExceptionRecord->ExceptionCode;
    DWORD thread_id = GetCurrentThreadId();
    unsigned index;
    if (exception_code == EXCEPTION_SINGLE_STEP &&
        s_step_active && s_step_thread == thread_id) {
        BYTE breakpoint = 0xcc;
        for (index = 0; index < HOSTHOOK_MAX_BREAKPOINTS; ++index) {
            struct hosthook_breakpoint *bp = &s_breakpoints[index];
            if (bp->address == s_step_address) {
                hosthook_write_protected((void *)(uintptr_t)bp->address,
                    &breakpoint, 1);
                bp->armed = TRUE;
                break;
            }
        }
        exception->ContextRecord->EFlags &= ~0x100;
        s_step_address = 0;
        s_step_thread = 0;
        InterlockedExchange(&s_step_active, 0);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (exception_code != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    for (index = 0; index < HOSTHOOK_MAX_BREAKPOINTS; ++index) {
        struct hosthook_breakpoint *bp = &s_breakpoints[index];
        DWORD address = (DWORD)(uintptr_t)exception->ExceptionRecord->ExceptionAddress;
        DWORD *stack = (DWORD *)(uintptr_t)exception->ContextRecord->Esp;
        if (bp->address != address || !bp->armed) continue;
        hosthook_restore_one_breakpoint(bp);
        if (bp->kind == HOSTHOOK_BP_LOADLIBRARY_ENTRY &&
            hosthook_readable(stack, 2 * sizeof(DWORD))) {
            const char *kind = hosthook_loaded_library_kind(stack[1]);
            if (kind) {
                hosthook_log("LOADLIB BEGIN library=%s filename=%08lx return=%08lx",
                    kind, (unsigned long)stack[1], (unsigned long)stack[0]);
                hosthook_add_breakpoint(stack[0], HOSTHOOK_BP_LOADLIBRARY_RETURN,
                    "LoadLibraryA return after MP module load");
            }
        } else if (bp->kind == HOSTHOOK_BP_LOADLIBRARY_RETURN) {
            hosthook_log("LOADLIB RETURN module=%08lx",
                (unsigned long)exception->ContextRecord->Eax);
            hosthook_install_runtime_probes();
        } else if (bp->kind == HOSTHOOK_BP_FLOW_FACTORY &&
            hosthook_readable(stack, 3 * sizeof(DWORD))) {
            DWORD parent = stack[1];
            DWORD descriptor = stack[2];
            s_flow_descriptor = descriptor;
            hosthook_log("FACTORY ENTRY label=%s parent=%08lx descriptor=%08lx return=%08lx",
                bp->label ? bp->label : "?", (unsigned long)parent,
                (unsigned long)descriptor, (unsigned long)stack[0]);
            hosthook_log_descriptor("factory-entry", descriptor);
            hosthook_add_breakpoint(s_multip_base + 0x16f9,
                HOSTHOOK_BP_OWNER_CLEAR, "multip_configuration_clear after modal");
            hosthook_add_breakpoint(s_multip_base + 0x14e0,
                HOSTHOOK_BP_OWNER_CLEAR, "multip_browser_clear nested cleanup");
            hosthook_trace_owner_return(exception->ContextRecord);
            hosthook_install_descriptor_callback_probes(descriptor);
            hosthook_add_breakpoint(stack[0], HOSTHOOK_BP_FLOW_RETURN,
                "MP_DIALOG factory return");
        } else if (bp->kind == HOSTHOOK_BP_FLOW_RETURN) {
            DWORD *array = (DWORD *)(uintptr_t)exception->ContextRecord->Eax;
            unsigned page;
            s_flow_array = (DWORD)(uintptr_t)array;
            hosthook_log("FACTORY RETURN eax=%08lx", (unsigned long)s_flow_array);
            if (hosthook_readable(array, 6 * sizeof(DWORD))) {
                hosthook_log("FACTORY ARRAY %08lx [%08lx,%08lx,%08lx,%08lx,%08lx,%08lx]",
                    (unsigned long)s_flow_array,
                    (unsigned long)array[0], (unsigned long)array[1],
                    (unsigned long)array[2], (unsigned long)array[3],
                    (unsigned long)array[4], (unsigned long)array[5]);
                for (page = 0; page < 5; ++page) {
                    HWND window = (HWND)(uintptr_t)array[page];
                    DWORD style = (DWORD)GetWindowLongA(window, GWL_STYLE);
                    hosthook_log("FACTORY PAGE index=%u hwnd=%08lx parent=%08lx style=%08lx WS_VISIBLE=%u IsWindowVisible=%u",
                        page, (unsigned long)array[page],
                        (unsigned long)(uintptr_t)GetParent(window),
                        (unsigned long)style,
                        (unsigned)((style & WS_VISIBLE) != 0),
                        (unsigned)IsWindowVisible(window));
                }
            } else {
                HWND dialog = (HWND)(uintptr_t)exception->ContextRecord->Eax;
                char class_name[64] = {0}, title[128] = {0};
                hosthook_log("FACTORY ARRAY unreadable at return");
                if (IsWindow(dialog)) {
                    GetClassNameA(dialog, class_name, sizeof(class_name));
                    GetWindowTextA(dialog, title, sizeof(title));
                    s_other_dialog_window = dialog;
                    hosthook_log("HOST FACTORY SINGLE WINDOW hwnd=%08lx class=%s title=%s style=%08lx WS_VISIBLE=%u IsWindowVisible=%u id1027=%08lx id1030=%08lx",
                        (unsigned long)(uintptr_t)dialog, class_name, title,
                        (unsigned long)GetWindowLongA(dialog, GWL_STYLE),
                        (unsigned)((GetWindowLongA(dialog, GWL_STYLE) & WS_VISIBLE) != 0),
                        (unsigned)IsWindowVisible(dialog),
                        (unsigned long)(uintptr_t)GetDlgItem(dialog, 1027),
                        (unsigned long)(uintptr_t)GetDlgItem(dialog, 1030));
                }
            }
            hosthook_log_descriptor("factory-return", s_flow_descriptor);
            hosthook_snapshot_flow();
            if (s_flow_snapshot_done)
                hosthook_log_flow_visibility("FACTORY_RETURN");
        } else if (bp->kind == HOSTHOOK_BP_OTHER_DIALOG_FACTORY &&
                   hosthook_readable(stack, 3 * sizeof(DWORD))) {
            static const DWORD options_offsets[5] = { 0x08, 0x0c, 0x10, 0x14, 0x04 };
            static const char *options_labels[5] = {
                "0x110b OK", "0x110b Host Details", "0x110b Get Weather",
                "0x110b Set Weather", "0x110b Read"
            };
            static const DWORD details_offsets[5] = { 0x04, 0x08, 0x0c, 0x10, 0x14 };
            static const char *details_labels[5] = {
                "0x110c validation", "0x110c accept", "0x110c remove-confirm",
                "0x110c refresh", "0x110c player-provider"
            };
            const DWORD *offsets;
            const char **labels;
            unsigned count, callback_index;
            DWORD parent = stack[1];
            s_other_dialog_descriptor = stack[2];
            s_other_dialog_label = bp->label;
            s_other_dialog_window = NULL;
            hosthook_log("DIALOG FACTORY ENTRY label=%s addr=%08lx parent=%08lx descriptor=%08lx return=%08lx",
                bp->label ? bp->label : "?", (unsigned long)bp->address,
                (unsigned long)parent,
                (unsigned long)s_other_dialog_descriptor,
                (unsigned long)stack[0]);
            hosthook_log("DIALOG BLOCK label=%s result=%08lx +04=%08lx +08=%08lx +0c=%08lx +10=%08lx +14=%08lx +18=%08lx +1c=%08lx +24=%08lx +4c=%08lx +50=%08lx +bc=%08lx +d0=%08lx",
                bp->label ? bp->label : "?",
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x00)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x04)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x08)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x0c)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x10)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x14)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x18)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x1c)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x24)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x4c)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0x50)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0xbc)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_other_dialog_descriptor + 0xd0)));
            if (bp->label && strstr(bp->label, "0x110b"))
                hosthook_log_multip_state("0x110b-factory-entry");
            if (bp->label && strstr(bp->label, "0x110b")) {
                offsets = options_offsets; labels = options_labels; count = 5;
            } else {
                offsets = details_offsets; labels = details_labels; count = 5;
            }
            for (callback_index = 0; callback_index < count; ++callback_index) {
                DWORD target = hosthook_read_dword((void *)(uintptr_t)
                    (s_other_dialog_descriptor + offsets[callback_index]));
                if (target) {
                    hosthook_log("DIALOG CALLBACK TARGET label=%s field=+%02lx target=%08lx",
                        labels[callback_index], (unsigned long)offsets[callback_index],
                        (unsigned long)target);
                    hosthook_add_breakpoint(target, HOSTHOOK_BP_MULTIP_CALLBACK,
                        labels[callback_index]);
                }
            }
            hosthook_add_breakpoint(stack[0], HOSTHOOK_BP_OTHER_DIALOG_RETURN,
                "MP_DIALOG single-dialog factory return");
        } else if (bp->kind == HOSTHOOK_BP_OTHER_DIALOG_RETURN) {
            s_other_dialog_window = (HWND)(uintptr_t)exception->ContextRecord->Eax;
            hosthook_log("DIALOG FACTORY RETURN label=%s hwnd=%08lx class=%s style=%08lx WS_VISIBLE=%u IsWindowVisible=%u descriptor=%08lx",
                s_other_dialog_label ? s_other_dialog_label : "?",
                (unsigned long)(uintptr_t)s_other_dialog_window,
                hosthook_readable(s_other_dialog_window, sizeof(WORD)) ? "valid" : "unreadable",
                (unsigned long)GetWindowLongA(s_other_dialog_window, GWL_STYLE),
                (unsigned)((GetWindowLongA(s_other_dialog_window, GWL_STYLE) & WS_VISIBLE) != 0),
                (unsigned)IsWindowVisible(s_other_dialog_window),
                (unsigned long)s_other_dialog_descriptor);
        } else if (bp->kind == HOSTHOOK_BP_OWNER_DIALOG_ENTRY) {
            DWORD descriptor = s_flow_descriptor;
            hosthook_log("OWNER CONFIGURATION ENTRY descriptor=%08lx",
                (unsigned long)descriptor);
            hosthook_log_descriptor("owner-dialog-entry", descriptor);
        } else if (bp->kind == HOSTHOOK_BP_OWNER_HOST_RETURN) {
            DWORD descriptor = s_flow_descriptor;
            hosthook_log("OWNER HOST RETURN addr=%08lx eax=%08lx descriptor=%08lx",
                (unsigned long)bp->address,
                (unsigned long)exception->ContextRecord->Eax,
                (unsigned long)descriptor);
            hosthook_log_descriptor("host-return", descriptor);
            hosthook_log_region("provider-list-at-host-return",
                hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x84)));
            hosthook_log_region("session-list-at-host-return",
                hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xac)));
        } else if (bp->kind == HOSTHOOK_BP_OWNER_CLEAR) {
            DWORD descriptor = s_flow_descriptor;
            DWORD *stack_values = (DWORD *)(uintptr_t)exception->ContextRecord->Esp;
            LONG depth = InterlockedIncrement(&s_owner_clear_depth);
            hosthook_log("OWNER CLEAR ENTRY addr=%08lx label=%s descriptor=%08lx count=%lu list=%08lx return=%08lx",
                (unsigned long)bp->address, bp->label ? bp->label : "?",
                (unsigned long)descriptor,
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xb630)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xb62c)),
                (unsigned long)(hosthook_readable(stack_values, sizeof(DWORD)) ? stack_values[0] : 0));
            if (depth == 1) {
                DWORD service_table = hosthook_read_dword(
                    (void *)(uintptr_t)(s_multip_base + 0xb204));
                DWORD free_service = hosthook_read_dword(
                    (void *)(uintptr_t)(service_table + 0x214));
                if (free_service)
                    hosthook_add_breakpoint(free_service, HOSTHOOK_BP_OWNER_FREE,
                        "HOST free service +0x214 during owner cleanup");
                hosthook_log("OWNER CLEANUP HOST service_table=%08lx free_0214=%08lx",
                    (unsigned long)service_table, (unsigned long)free_service);
            }
            hosthook_log_descriptor("before-owner-clear", descriptor);
            {
                DWORD providers = hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x84));
                DWORD sessions = hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xac));
                hosthook_log("OWNER CLEAR storage provider=%08lx providerCount=%lu sessions=%08lx sessionCount=%lu",
                    (unsigned long)providers,
                    (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x88)),
                    (unsigned long)sessions,
                    (unsigned long)hosthook_read_dword((void *)(uintptr_t)(descriptor + 0xb0)));
                hosthook_log_region("provider-list-before-free", providers);
                hosthook_log_region("session-list-before-free", sessions);
            }
            if (hosthook_readable(stack_values, sizeof(DWORD)))
                hosthook_add_breakpoint(stack_values[0], HOSTHOOK_BP_OWNER_CLEAR_RETURN,
                    "runtime owner cleanup return");
        } else if (bp->kind == HOSTHOOK_BP_OWNER_CLEAR_RETURN) {
            LONG depth;
            hosthook_log("OWNER CLEAR RETURN eax=%08lx descriptor=%08lx",
                (unsigned long)exception->ContextRecord->Eax,
                (unsigned long)s_flow_descriptor);
            hosthook_log_descriptor("after-owner-clear", s_flow_descriptor);
            hosthook_log_region("provider-list-after-free",
                hosthook_read_dword((void *)(uintptr_t)(s_flow_descriptor + 0x84)));
            hosthook_log_region("session-list-after-free",
                hosthook_read_dword((void *)(uintptr_t)(s_flow_descriptor + 0xac)));
            depth = InterlockedDecrement(&s_owner_clear_depth);
            hosthook_log("OWNER CLEAR DEPTH after-return=%ld",
                (long)depth);
        } else if (bp->kind == HOSTHOOK_BP_OWNER_FREE &&
                   hosthook_readable(stack, 2 * sizeof(DWORD))) {
            if (s_owner_clear_depth > 0) {
                DWORD slot = stack[1];
                DWORD value = hosthook_read_dword((void *)(uintptr_t)slot);
                s_pending_free_slot = slot;
                hosthook_log("HOST FREE BEGIN clear_depth=%ld slot=%08lx value=%08lx return=%08lx",
                    s_owner_clear_depth, (unsigned long)slot,
                    (unsigned long)value, (unsigned long)stack[0]);
                hosthook_log_region("host-free-payload", value);
                hosthook_add_breakpoint(stack[0], HOSTHOOK_BP_OWNER_FREE_RETURN,
                    "HOST free service return during owner cleanup");
            }
        } else if (bp->kind == HOSTHOOK_BP_OWNER_FREE_RETURN) {
            if (s_owner_clear_depth > 0)
                hosthook_log("HOST FREE END clear_depth=%ld result=%08lx slot=%08lx now=%08lx",
                    s_owner_clear_depth,
                    (unsigned long)exception->ContextRecord->Eax,
                    (unsigned long)s_pending_free_slot,
                    (unsigned long)hosthook_read_dword((void *)(uintptr_t)s_pending_free_slot));
        } else if (bp->kind == HOSTHOOK_BP_MULTIP_CALLBACK &&
                   hosthook_readable(stack, 3 * sizeof(DWORD))) {
            DWORD descriptor = stack[1];
            DWORD argument3 = hosthook_readable(stack, 4 * sizeof(DWORD)) ? stack[3] : 0;
            DWORD argument4 = hosthook_readable(stack, 5 * sizeof(DWORD)) ? stack[4] : 0;
            hosthook_log("MULTIP CALLBACK args entry=%s return=%08lx a0=%08lx a1=%08lx a2=%08lx a3=%08lx",
                bp->label ? bp->label : "?", (unsigned long)stack[0],
                (unsigned long)stack[1], (unsigned long)stack[2],
                (unsigned long)argument3, (unsigned long)argument4);
            if (bp->label && strstr(bp->label, "+0x98"))
                hosthook_log("MULTIP CALLBACK entry=%s descriptor=%08lx index=%08lx",
                    bp->label, (unsigned long)descriptor,
                    (unsigned long)stack[2]);
            else
                hosthook_log("MULTIP CALLBACK entry=%s descriptor=%08lx",
                    bp->label ? bp->label : "?", (unsigned long)descriptor);
            hosthook_log_descriptor("callback-entry", descriptor);
            if (bp->label && strstr(bp->label, "+0x04"))
                hosthook_log_selected_provider(descriptor);
        } else if (bp->kind == HOSTHOOK_BP_MULTIP_RUNTIME) {
            DWORD *runtime_stack = (DWORD *)(uintptr_t)exception->ContextRecord->Esp;
            DWORD esp = exception->ContextRecord->Esp;
            const char *label = bp->label ? bp->label : "MULTIP runtime";
            if (strstr(label, "DirectPlayCreate API return")) {
                hosthook_log("MULTIP DirectPlayCreate API return hr=%08lx esp=%08lx api=%08lx",
                    (unsigned long)exception->ContextRecord->Eax,
                    (unsigned long)esp,
                    (unsigned long)hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbb8c)));
                hosthook_log_function_owner("DirectPlayCreate API",
                    hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbb8c)));
            } else if (strstr(label, "QueryInterface")) {
                hosthook_log("MULTIP %s hr=%08lx esp=%08lx",
                    label, (unsigned long)exception->ContextRecord->Eax,
                    (unsigned long)esp);
                hosthook_log_multip_state(label);
            } else if (strstr(label, "DirectPlay create return")) {
                hosthook_log("MULTIP %s result=%08lx", label,
                    (unsigned long)exception->ContextRecord->Eax);
                hosthook_log_multip_state("directplay-create-return");
            } else if (strstr(label, "multip_directplay_create entry")) {
                DWORD provider = hosthook_readable(runtime_stack, 2 * sizeof(DWORD)) ?
                    runtime_stack[1] : 0;
                hosthook_log("MULTIP %s eip=%08lx return=%08lx provider=%08lx guid=%08lx-%04lx-%04lx-%08lx-%08lx",
                    label, (unsigned long)bp->address,
                    (unsigned long)hosthook_readable(runtime_stack, sizeof(DWORD)) ? runtime_stack[0] : 0,
                    (unsigned long)provider,
                    (unsigned long)hosthook_read_dword((void *)(uintptr_t)provider),
                    (unsigned long)(hosthook_read_dword((void *)(uintptr_t)(provider + 4)) & 0xffff),
                    (unsigned long)(hosthook_read_dword((void *)(uintptr_t)(provider + 4)) >> 16),
                    (unsigned long)hosthook_read_dword((void *)(uintptr_t)(provider + 8)),
                    (unsigned long)hosthook_read_dword((void *)(uintptr_t)(provider + 12)));
                hosthook_log_multip_state("multip_directplay_create-entry");
            } else if (strstr(label, "provider callback +04 return")) {
                DWORD descriptor = hosthook_readable(runtime_stack, 2 * sizeof(DWORD)) ?
                    runtime_stack[1] : 0;
                hosthook_log("MULTIP %s result=%08lx descriptor=%08lx",
                    label, (unsigned long)exception->ContextRecord->Eax,
                    (unsigned long)descriptor);
                hosthook_log_descriptor("provider-callback-return", descriptor);
                hosthook_log_multip_state("provider-callback-return");
            } else if (strstr(label, "session host Open return")) {
                DWORD directplay = hosthook_read_dword((void *)(uintptr_t)(s_multip_base + 0xbb88));
                DWORD vtable = hosthook_read_dword((void *)(uintptr_t)directplay);
                DWORD open_target = hosthook_read_dword((void *)(uintptr_t)(vtable + 0x60));
                hosthook_log("MULTIP %s HRESULT=%08lx",
                    label, (unsigned long)exception->ContextRecord->Eax);
                hosthook_log_function_owner("IDirectPlay3::Open", open_target);
                hosthook_log_multip_state("session-host-open-return");
            } else if (strstr(label, "session group create")) {
                hosthook_log("MULTIP %s HRESULT=%08lx",
                    label, (unsigned long)exception->ContextRecord->Eax);
                hosthook_log_multip_state(label);
            } else if (strstr(label, "multip_session_toggle entry") ||
                       strstr(label, "multip_session_host entry") ||
                       strstr(label, "multip_session_create_groups entry") ||
                       strstr(label, "multip_options_init entry") ||
                       strstr(label, "multip_options_dialog entry")) {
                DWORD return_address = hosthook_readable(runtime_stack, sizeof(DWORD)) ?
                    runtime_stack[0] : 0;
                hosthook_log("MULTIP %s eip=%08lx return=%08lx eax=%08lx",
                    label, (unsigned long)bp->address,
                    (unsigned long)return_address,
                    (unsigned long)exception->ContextRecord->Eax);
                hosthook_log_multip_state(label);
                if (return_address)
                    hosthook_add_breakpoint(return_address,
                        HOSTHOOK_BP_MULTIP_RUNTIME_RETURN,
                        strstr(label, "multip_session_toggle") ? "multip_session_toggle return" :
                        strstr(label, "multip_session_host") ? "multip_session_host return" :
                        strstr(label, "multip_session_create_groups") ? "multip_session_create_groups return" :
                        strstr(label, "multip_options_init") ? "multip_options_init return" :
                        "multip_options_dialog return");
            } else if (strstr(label, "options init return")) {
                hosthook_log("MULTIP %s result=%08lx",
                    label, (unsigned long)exception->ContextRecord->Eax);
                hosthook_log_options_block(label);
                hosthook_log_multip_state(label);
            } else {
                hosthook_log("MULTIP %s eip=%08lx eax=%08lx",
                    label, (unsigned long)bp->address,
                    (unsigned long)exception->ContextRecord->Eax);
                hosthook_log_multip_state(label);
            }
        } else if (bp->kind == HOSTHOOK_BP_MULTIP_RUNTIME_RETURN) {
            hosthook_log("MULTIP %s eip=%08lx result=%08lx",
                bp->label ? bp->label : "runtime return",
                (unsigned long)bp->address,
                (unsigned long)exception->ContextRecord->Eax);
            if (bp->label && strstr(bp->label, "multip_options_init return"))
                hosthook_log_options_block(bp->label);
            hosthook_log_multip_state(bp->label ? bp->label : "runtime-return");
        } else if (bp->kind == HOSTHOOK_BP_POST_NCDESTROY) {
            DWORD object = exception->ContextRecord->Ecx;
            unsigned page_index;
            DWORD initial_hwnd = 0;
            for (page_index = 0; page_index < HOSTHOOK_PAGE_COUNT; ++page_index)
                if (s_flow_object_addresses[page_index] == object)
                    initial_hwnd = s_flow_object_hwnds[page_index];
            hosthook_log("MP_DIALOG PostNcDestroy addr=%08lx object=%08lx initial-hwnd=%08lx current-hwnd-member=%08lx vtable=%08lx",
                (unsigned long)bp->address, (unsigned long)object,
                (unsigned long)initial_hwnd,
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(object + 0x20)),
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)object));
        } else if (bp->kind == HOSTHOOK_BP_DELETING_DESTRUCTOR) {
            DWORD object = exception->ContextRecord->Ecx;
            DWORD *arguments = (DWORD *)(uintptr_t)exception->ContextRecord->Esp;
            unsigned page_index;
            DWORD initial_hwnd = 0;
            for (page_index = 0; page_index < HOSTHOOK_PAGE_COUNT; ++page_index)
                if (s_flow_object_addresses[page_index] == object)
                    initial_hwnd = s_flow_object_hwnds[page_index];
            hosthook_log("MP_DIALOG deleting-destructor addr=%08lx object=%08lx initial-hwnd=%08lx current-hwnd-member=%08lx flag=%08lx",
                (unsigned long)bp->address, (unsigned long)object,
                (unsigned long)initial_hwnd,
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)(object + 0x20)),
                (unsigned long)(hosthook_readable(arguments, 2 * sizeof(DWORD)) ? arguments[1] : 0));
        }
        s_step_address = bp->address;
        s_step_thread = thread_id;
        InterlockedExchange(&s_step_active, 1);
        exception->ContextRecord->Eip = bp->address;
        exception->ContextRecord->EFlags |= 0x100;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static BOOL WINAPI hosthook_postmessage(HWND window, UINT message,
                                         WPARAM wparam, LPARAM lparam)
{
    typedef BOOL (WINAPI *post_message_fn)(HWND, UINT, WPARAM, LPARAM);
    post_message_fn original = (post_message_fn)(uintptr_t)s_original_postmessage;
    BOOL result;
    DWORD error;
    if (message == WM_COMMAND && LOWORD(wparam) == 0x3e8)
        hosthook_log("POSTMESSAGEA target=%p msg=%04x wp=%08lx lp=%08lx",
            (void *)window, message, (unsigned long)wparam,
            (unsigned long)lparam);
    if (!original) return FALSE;
    SetLastError(ERROR_SUCCESS);
    result = original(window, message, wparam, lparam);
    error = GetLastError();
    if (message == WM_COMMAND && LOWORD(wparam) == 0x3e8)
        hosthook_log("POSTMESSAGEA result=%u error=%lu",
            (unsigned)result, (unsigned long)error);
    return result;
}

static BOOL hosthook_patch_postmessage_iat(HMODULE module)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_DATA_DIRECTORY directory;
    IMAGE_IMPORT_DESCRIPTOR *imports;
    DWORD image_size = hosthook_module_size(module);
    if (!module || !image_size || !hosthook_readable(dos, sizeof(*dos))) return FALSE;
    nt = (IMAGE_NT_HEADERS32 *)((BYTE *)module + dos->e_lfanew);
    directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress || directory.VirtualAddress >= image_size) return FALSE;
    imports = (IMAGE_IMPORT_DESCRIPTOR *)((BYTE *)module + directory.VirtualAddress);
    while (hosthook_readable(imports, sizeof(*imports)) && imports->Name) {
        const char *dll_name = (const char *)((BYTE *)module + imports->Name);
        IMAGE_THUNK_DATA32 *originals = (IMAGE_THUNK_DATA32 *)
            ((BYTE *)module + imports->OriginalFirstThunk);
        IMAGE_THUNK_DATA32 *first = (IMAGE_THUNK_DATA32 *)
            ((BYTE *)module + imports->FirstThunk);
        if (lstrcmpiA(dll_name, "USER32.dll") == 0) {
            while (hosthook_readable(originals, sizeof(*originals)) &&
                   originals->u1.AddressOfData) {
                if (!(originals->u1.Ordinal & IMAGE_ORDINAL_FLAG32)) {
                    IMAGE_IMPORT_BY_NAME *name = (IMAGE_IMPORT_BY_NAME *)
                        ((BYTE *)module + originals->u1.AddressOfData);
                    if (lstrcmpiA((const char *)name->Name, "PostMessageA") == 0) {
                        DWORD replacement = (DWORD)(uintptr_t)hosthook_postmessage;
                        DWORD old_protect, ignored;
                        s_postmessage_slot = &first->u1.Function;
                        s_original_postmessage = first->u1.Function;
                        if (!VirtualProtect(s_postmessage_slot, sizeof(DWORD),
                                PAGE_READWRITE, &old_protect)) return FALSE;
                        *s_postmessage_slot = replacement;
                        FlushInstructionCache(GetCurrentProcess(), s_postmessage_slot,
                            sizeof(DWORD));
                        VirtualProtect(s_postmessage_slot, sizeof(DWORD),
                            old_protect, &ignored);
                        InterlockedExchange(&s_postmessage_patched, 1);
                        hosthook_log("POSTMESSAGE IAT slot=%p original=%08lx replacement=%08lx",
                            s_postmessage_slot,
                            (unsigned long)s_original_postmessage,
                            (unsigned long)replacement);
                        return TRUE;
                    }
                }
                ++originals;
                ++first;
            }
        }
        ++imports;
    }
    hosthook_log("POSTMESSAGE IAT not found module=%p", module);
    return FALSE;
}

static void hosthook_snapshot_flow(void)
{
    static const DWORD object_descriptor_offsets[5] = {
        0x19ec, 0x1cac, 0x1d58, 0x18f8, 0x1ae8
    };
    DWORD expected[6];
    DWORD *array, *objects;
    DWORD descriptor = 0;
    unsigned index;
    if (s_flow_snapshot_done || !s_mp_base || s_created_page_count != 5) return;
    expected[0] = (DWORD)(uintptr_t)s_created_pages[4]; /* Intro */
    expected[1] = (DWORD)(uintptr_t)s_created_pages[3]; /* Choice */
    expected[2] = (DWORD)(uintptr_t)s_created_pages[1]; /* Host */
    expected[3] = (DWORD)(uintptr_t)s_created_pages[0]; /* Connection */
    expected[4] = (DWORD)(uintptr_t)s_created_pages[2]; /* Browser */
    expected[5] = 0;
    array = (DWORD *)(uintptr_t)(s_mp_base + s_mp_array_offset);
    objects = (DWORD *)(uintptr_t)(s_mp_base + s_mp_objects_offset);
    if (!hosthook_readable(array, 6 * sizeof(DWORD))) {
        hosthook_log("FLOW ARRAY unreadable layout=%s address=%p",
            s_mp_layout, array);
        return;
    }
    for (index = 0; index < 6; ++index) {
        if (array[index] != expected[index]) {
            if (!s_flow_snapshot_deferred_logged) {
                hosthook_log("FLOW ARRAY snapshot deferred layout=%s index=%u actual=%08lx expected=%08lx",
                    s_mp_layout, index, (unsigned long)array[index],
                    (unsigned long)expected[index]);
                s_flow_snapshot_deferred_logged = TRUE;
            }
            return;
        }
    }
    s_flow_snapshot_done = TRUE;
    s_flow_array = (DWORD)(uintptr_t)array;
    hosthook_log("FLOW ARRAY layout=%s returned=%p expected=[%08lx,%08lx,%08lx,%08lx,%08lx,00000000] actual=[%08lx,%08lx,%08lx,%08lx,%08lx,%08lx]",
        s_mp_layout, array,
        (unsigned long)expected[0], (unsigned long)expected[1],
        (unsigned long)expected[2], (unsigned long)expected[3],
        (unsigned long)expected[4],
        (unsigned long)array[0], (unsigned long)array[1],
        (unsigned long)array[2], (unsigned long)array[3],
        (unsigned long)array[4], (unsigned long)array[5]);
    for (index = 0; index < 5; ++index) {
        HWND window = (HWND)(uintptr_t)array[index];
        DWORD style = (DWORD)GetWindowLongA(window, GWL_STYLE);
        hosthook_log("FLOW PAGE index=%u hwnd=%p created=%p style=%08lx WS_VISIBLE=%u IsWindowVisible=%u",
            index, window, (void *)(uintptr_t)expected[index],
            (unsigned long)style, (unsigned)((style & WS_VISIBLE) != 0),
            (unsigned)IsWindowVisible(window));
    }
    if (!hosthook_readable(objects, 5 * sizeof(DWORD))) {
        hosthook_log("FLOW OBJECTS unreadable layout=%s address=%p",
            s_mp_layout, objects);
        return;
    }
    s_flow_objects = (DWORD)(uintptr_t)objects;
    for (index = 0; index < 5; ++index) {
        DWORD object = objects[index];
        DWORD vtable = hosthook_read_dword((void *)(uintptr_t)object);
        DWORD post_nc_destroy = hosthook_read_dword((void *)(uintptr_t)(vtable + 0xac));
        DWORD deleting_destructor = hosthook_read_dword((void *)(uintptr_t)(vtable + 0x04));
        DWORD object_hwnd = hosthook_read_dword((void *)(uintptr_t)(object + 0x20));
        DWORD page_descriptor = hosthook_read_dword((void *)(uintptr_t)
            (object + object_descriptor_offsets[index]));
        s_flow_object_addresses[index] = object;
        s_flow_object_hwnds[index] = object_hwnd;
        hosthook_log("FLOW OBJECT index=%u object=%08lx hwnd=%08lx descriptor=%08lx vtable=%08lx post-ncdestroy=%08lx deleting-dtor=%08lx",
            index, (unsigned long)object, (unsigned long)object_hwnd,
            (unsigned long)page_descriptor, (unsigned long)vtable,
            (unsigned long)post_nc_destroy,
            (unsigned long)deleting_destructor);
        if (post_nc_destroy)
            hosthook_add_breakpoint(post_nc_destroy,
                HOSTHOOK_BP_POST_NCDESTROY,
                "MP_DIALOG page vtable +0xac PostNcDestroy");
        if (deleting_destructor)
            hosthook_add_breakpoint(deleting_destructor,
                HOSTHOOK_BP_DELETING_DESTRUCTOR,
                "MP_DIALOG page vtable +0x04 deleting destructor");
        if (index == 0) descriptor = page_descriptor;
        else if (page_descriptor != descriptor)
            hosthook_log("FLOW DESCRIPTOR POINTER MISMATCH index=%u first=%08lx current=%08lx",
                index, (unsigned long)descriptor,
                (unsigned long)page_descriptor);
    }
    {
        DWORD factory_descriptor = s_flow_descriptor;
        if (factory_descriptor && factory_descriptor != descriptor)
            hosthook_log("FLOW DESCRIPTOR POINTER MISMATCH factory=%08lx objects=%08lx",
                (unsigned long)factory_descriptor, (unsigned long)descriptor);
    }
    s_flow_descriptor = descriptor;
    if (descriptor && hosthook_readable((void *)(uintptr_t)descriptor, 0xbc)) {
        DWORD snapshot_value = hosthook_read_dword((void *)(uintptr_t)(descriptor + 0x1c));
        hosthook_log("FLOW DESCRIPTOR compare factory=%08lx snapshot=%08lx +1c=%08lx",
            (unsigned long)(uintptr_t)s_flow_descriptor, (unsigned long)descriptor,
            (unsigned long)snapshot_value);
    }
    hosthook_log_descriptor("visible-first-page", descriptor);
    hosthook_install_callbacks(descriptor);
}

static void hosthook_log_flow_visibility(const char *stage)
{
    DWORD *array = (DWORD *)(uintptr_t)s_flow_array;
    unsigned index;
    if (!hosthook_readable(array, 6 * sizeof(DWORD))) return;
    for (index = 0; index < 5; ++index) {
        HWND window = (HWND)(uintptr_t)array[index];
        DWORD style = (DWORD)GetWindowLongA(window, GWL_STYLE);
        hosthook_log("FLOW VISIBILITY stage=%s index=%u hwnd=%p style=%08lx WS_VISIBLE=%u IsWindowVisible=%u parent=%08lx",
            stage, index, window, (unsigned long)style,
            (unsigned)((style & WS_VISIBLE) != 0),
            (unsigned)IsWindowVisible(window),
            (unsigned long)(uintptr_t)GetParent(window));
    }
}

static BOOL hosthook_is_flow_page(HWND window)
{
    unsigned index;
    for (index = 0; index < s_created_page_count; ++index)
        if (s_created_pages[index] == window) return TRUE;
    return FALSE;
}

static void hosthook_install_mp_probes(HMODULE module)
{
    FARPROC export_one;
    DWORD module_base, export_rva, image_size;
    char path[MAX_PATH] = {0};
    if (!module) return;
    module_base = (DWORD)(uintptr_t)module;
    image_size = hosthook_module_size(module);
    if (InterlockedCompareExchange(&s_mp_probes_installed, 1, 0) != 0) {
        if (s_mp_base == module_base) {
            hosthook_rearm_module_breakpoints(module_base, image_size);
            hosthook_patch_postmessage_iat(module);
        }
        return;
    }
    export_one = GetProcAddress(module, (LPCSTR)1);
    export_rva = export_one ? (DWORD)(uintptr_t)export_one - module_base : 0;
    s_mp_base = module_base;
    if (export_rva == 0xa048) {
        s_mp_layout = "retail";
        s_mp_array_offset = 0xa0b8;
        s_mp_objects_offset = 0xa0a0;
        hosthook_add_breakpoint(module_base + 0x1130,
            HOSTHOOK_BP_FLOW_FACTORY, "retail 0x110a factory");
        hosthook_add_breakpoint(module_base + 0x11e0,
            HOSTHOOK_BP_OTHER_DIALOG_FACTORY, "retail 0x110b settings factory");
        hosthook_add_breakpoint(module_base + 0x1270,
            HOSTHOOK_BP_OTHER_DIALOG_FACTORY, "retail 0x110c details factory");
    } else if (export_rva == 0x6018) {
        s_mp_layout = "replacement";
        s_mp_array_offset = 0xd01c;
        s_mp_objects_offset = 0xd008;
        hosthook_add_breakpoint(module_base + 0x1b50,
            HOSTHOOK_BP_FLOW_FACTORY, "replacement 0x110a factory");
        hosthook_add_breakpoint(module_base + 0x4180,
            HOSTHOOK_BP_OTHER_DIALOG_FACTORY, "replacement 0x110b settings factory");
        hosthook_add_breakpoint(module_base + 0x51a0,
            HOSTHOOK_BP_OTHER_DIALOG_FACTORY, "replacement 0x110c details factory");
    } else {
        hosthook_log("RUNTIME MP_DIALOG layout unresolved module=%p size=%08lx export1=%08lx rva=%08lx",
            module, (unsigned long)image_size, (unsigned long)(uintptr_t)export_one,
            (unsigned long)export_rva);
        InterlockedExchange(&s_mp_probes_installed, 0);
        return;
    }
    GetModuleFileNameA(module, path, sizeof(path));
    hosthook_log("RUNTIME MODULE MP_DIALOG=%p size=%08lx path=%s layout=%s export1_rva=%08lx",
        module, (unsigned long)image_size, path, s_mp_layout,
        (unsigned long)export_rva);
    hosthook_patch_postmessage_iat(module);
}

static void hosthook_install_multip_probes(HMODULE module)
{
    DWORD mult_base, image_size;
    if (!module) return;
    mult_base = (DWORD)(uintptr_t)module;
    image_size = hosthook_module_size(module);
    if (InterlockedCompareExchange(&s_multip_probes_installed, 1, 0) != 0) {
        if (s_multip_base == mult_base)
            hosthook_rearm_module_breakpoints(mult_base, image_size);
        return;
    }
    s_multip_base = mult_base;
    hosthook_log("RUNTIME MODULE MULTIPLAYER=%p size=%08lx",
        module, (unsigned long)image_size);
    hosthook_log("RUNTIME MULTIPLAYER base=%08lx owner-clear-bps=deferred-to-factory",
        (unsigned long)mult_base);
    hosthook_add_breakpoint(mult_base + 0x1d1d,
        HOSTHOOK_BP_MULTIP_RUNTIME, "multip_options_init entry");
    hosthook_add_breakpoint(mult_base + 0x1d84,
        HOSTHOOK_BP_MULTIP_RUNTIME, "multip_options_dialog entry");
    hosthook_add_breakpoint(mult_base + 0x1e15,
        HOSTHOOK_BP_MULTIP_RUNTIME, "multip_session_toggle entry");
    hosthook_add_breakpoint(mult_base + 0x2e60,
        HOSTHOOK_BP_MULTIP_RUNTIME, "multip_directplay_create entry");
    hosthook_add_breakpoint(mult_base + 0x2e85,
        HOSTHOOK_BP_MULTIP_RUNTIME, "DirectPlayCreate API return");
    hosthook_add_breakpoint(mult_base + 0x2eac,
        HOSTHOOK_BP_MULTIP_RUNTIME, "DirectPlay QueryInterface 2A return");
    hosthook_add_breakpoint(mult_base + 0x2ed0,
        HOSTHOOK_BP_MULTIP_RUNTIME, "DirectPlay QueryInterface 3A return");
    hosthook_add_breakpoint(mult_base + 0x2f01,
        HOSTHOOK_BP_MULTIP_RUNTIME, "DirectPlay create return");
    hosthook_add_breakpoint(mult_base + 0x2f69,
        HOSTHOOK_BP_MULTIP_RUNTIME, "provider callback +04 return");
    hosthook_add_breakpoint(mult_base + 0x3394,
        HOSTHOOK_BP_MULTIP_RUNTIME, "multip_session_create_groups entry");
    hosthook_add_breakpoint(mult_base + 0x33d2,
        HOSTHOOK_BP_MULTIP_RUNTIME, "session group create first return");
    hosthook_add_breakpoint(mult_base + 0x3419,
        HOSTHOOK_BP_MULTIP_RUNTIME, "session group create second return");
    hosthook_add_breakpoint(mult_base + 0x3424,
        HOSTHOOK_BP_MULTIP_RUNTIME, "multip_session_host entry");
    hosthook_add_breakpoint(mult_base + 0x34ca,
        HOSTHOOK_BP_MULTIP_RUNTIME, "session host Open return");
    hosthook_log_multip_state("multip-probes-installed");
}

static void hosthook_trace_owner_return(CONTEXT *context)
{
    DWORD *stack = (DWORD *)(uintptr_t)context->Esp;
    unsigned index;
    unsigned candidates = 0;
    if (!s_multip_base || !hosthook_readable(stack, 24 * sizeof(DWORD))) {
        hosthook_log("OWNER STACK unavailable multip=%08lx esp=%08lx",
            (unsigned long)s_multip_base, (unsigned long)context->Esp);
        return;
    }
    hosthook_log("OWNER STACK factory-entry eip=%08lx esp=%08lx ebp=%08lx",
        (unsigned long)context->Eip, (unsigned long)context->Esp,
        (unsigned long)context->Ebp);
    for (index = 0; index < 24; ++index) {
        DWORD value = stack[index];
        if (value >= s_multip_base + 0x1000 && value < s_multip_base + 0xab74) {
            hosthook_log("OWNER STACK MULTIP candidate index=%u slot=%08lx value=%08lx",
                index, (unsigned long)(uintptr_t)&stack[index],
                (unsigned long)value);
            /* Several older return-looking values remain in the caller's
             * stack. Instrument all code-range candidates so the executed
             * return identifies the live owner boundary. */
            if (hosthook_add_breakpoint(value, HOSTHOOK_BP_OWNER_HOST_RETURN,
                    "runtime stack candidate after HOST modal"))
                ++candidates;
        }
    }
    s_owner_return_hook_set = candidates != 0;
    hosthook_log("OWNER RETURN HOOK candidates=%u installed=%u",
        candidates, (unsigned)s_owner_return_hook_set);
}

static void hosthook_install_descriptor_callback_probes(DWORD descriptor)
{
    static const DWORD offsets[10] = {
        0x04, 0x08, 0x0c, 0x10, 0x44, 0x94, 0x9c, 0xa0, 0xa4, 0x98
    };
    static const char *labels[10] = {
        "+0x04 provider callback (descriptor target)",
        "+0x08 reset callback (descriptor target)",
        "+0x0c accept callback (descriptor target)",
        "+0x10 validate callback (descriptor target)",
        "+0x44 options callback (descriptor target)",
        "+0x94 discovery callback (descriptor target)",
        "+0x9c details callback (descriptor target)",
        "+0xa0 refresh callback (descriptor target)",
        "+0xa4 options callback (descriptor target)",
        "+0x98 player callback (descriptor target)"
    };
    unsigned index;
    if (!hosthook_readable((const void *)(uintptr_t)descriptor, 0xbc)) return;
    for (index = 0; index < 10; ++index) {
        DWORD target = hosthook_read_dword((void *)(uintptr_t)(descriptor + offsets[index]));
        hosthook_log("MULTIP CALLBACK TARGET field=+%02lx descriptor=%08lx function=%08lx",
            (unsigned long)offsets[index], (unsigned long)descriptor,
            (unsigned long)target);
        if (target)
            hosthook_add_breakpoint(target, HOSTHOOK_BP_MULTIP_CALLBACK,
                labels[index]);
    }
}

static void hosthook_install_loader_probe(void)
{
    HMODULE kernel32 = GetModuleHandleA("KERNEL32.DLL");
    FARPROC load_library = kernel32 ? GetProcAddress(kernel32, "LoadLibraryA") : NULL;
    if (InterlockedCompareExchange(&s_loader_probe_installed, 1, 0) != 0)
        return;
    if (!load_library || !hosthook_add_breakpoint((DWORD)(uintptr_t)load_library,
            HOSTHOOK_BP_LOADLIBRARY_ENTRY, "KERNEL32 LoadLibraryA")) {
        hosthook_log("RUNTIME LoadLibraryA probe unavailable kernel32=%p function=%p error=%lu",
            kernel32, (void *)load_library, (unsigned long)GetLastError());
        InterlockedExchange(&s_loader_probe_installed, 0);
        return;
    }
    hosthook_log("RUNTIME LoadLibraryA entry=%08lx", (unsigned long)(uintptr_t)load_library);
}

static void hosthook_install_runtime_probes(void)
{
    hosthook_install_loader_probe();
    hosthook_install_mp_probes(GetModuleHandleA("MP_DIALOG.DLL"));
    hosthook_install_multip_probes(GetModuleHandleA("MULTIPLAYER.DLL"));
}

static BOOL hosthook_idle_sample(HWND window)
{
    static HWND windows[16];
    static LONG counts[16];
    unsigned index;
    for (index = 0; index < 16; ++index) {
        if (windows[index] == window) break;
        if (!windows[index]) {
            windows[index] = window;
            break;
        }
    }
    if (index == 16) return FALSE;
    ++counts[index];
    return counts[index] <= 4 || counts[index] % 1000 == 0;
}

static BOOL hosthook_relevant(HWND window, UINT message)
{
    return message == WM_COMMAND || message == WM_INITDIALOG ||
        message == WM_SHOWWINDOW || message == WM_ACTIVATE ||
        message == WM_ENABLE || message == WM_CLOSE || message == WM_DESTROY ||
        message == WM_NCDESTROY || message == 0x05f4 ||
        (message == 0x0464 && hosthook_idle_sample(window));
}

static void hosthook_window_state(HWND window, char *class_name,
                                  char *title, DWORD *id,
                                  DWORD *style, BOOL *visible)
{
    class_name[0] = 0;
    title[0] = 0;
    GetClassNameA(window, class_name, 63);
    GetWindowTextA(window, title, 127);
    *id = (DWORD)GetDlgCtrlID(window);
    *style = (DWORD)GetWindowLongA(window, GWL_STYLE);
    *visible = IsWindowVisible(window);
}

__declspec(dllexport) LRESULT CALLBACK cbt_hook(int code, WPARAM wparam,
                                                 LPARAM lparam)
{
    if (code == HCBT_CREATEWND && lparam) {
        const CBT_CREATEWND *create = (const CBT_CREATEWND *)lparam;
        const CREATESTRUCTA *params = create->lpcs;
        if (params && params->hwndParent == s_main_window &&
            params->lpszClass &&
            ((ULONG_PTR)params->lpszClass >> 16) == 0 &&
            LOWORD((ULONG_PTR)params->lpszClass) == 0x8002 &&
            s_created_page_count < HOSTHOOK_PAGE_COUNT) {
            /* HOST loads MP_DIALOG/MULTIP from the Connect command itself.
             * Retry here: the first WM_GETMESSAGE for Connect precedes both
             * LoadLibrary calls, while the page factory is still constructing
             * its windows and has not yet posted its initial Next command. */
            hosthook_install_runtime_probes();
            s_created_pages[s_created_page_count++] = (HWND)wparam;
            hosthook_log("FLOW PAGE CREATE ordinal=%u hwnd=%p parent=%p",
                s_created_page_count - 1, (void *)wparam,
                params->hwndParent);
        }
        hosthook_log("CBT CREATE hwnd=%p parent=%p id=%u class=%p title=%p style=%08lx rect=%ld,%ld,%ld,%ld",
            (void *)wparam, params ? params->hwndParent : NULL,
            params ? (unsigned)(UINT_PTR)params->hMenu : 0,
            params ? params->lpszClass : NULL,
            params ? params->lpszName : NULL,
            params ? (unsigned long)params->style : 0,
            params ? params->x : 0, params ? params->y : 0,
            params ? params->cx : 0, params ? params->cy : 0);
    } else if (code == HCBT_DESTROYWND) {
        char class_name[64], title[128];
        DWORD id, style;
        BOOL visible;
        hosthook_window_state((HWND)wparam, class_name, title, &id,
            &style, &visible);
        hosthook_log("CBT DESTROY hwnd=%p id=%lu class=%s title=%s style=%08lx visible=%u",
            (void *)wparam, (unsigned long)id, class_name, title,
            (unsigned long)style, (unsigned)visible);
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}

__declspec(dllexport) LRESULT CALLBACK callwnd_hook(int code, WPARAM wparam,
                                                      LPARAM lparam)
{
    if (code >= 0 && lparam) {
        const CWPSTRUCT *call = (const CWPSTRUCT *)lparam;
        if (call->message == HOSTHOOK_QUIESCE_MESSAGE) {
            hosthook_log("HOOK QUIESCE received hwnd=%p", (void *)call->hwnd);
            hosthook_restore_runtime_patches();
        }
        if (call->message == WM_COMMAND && LOWORD(call->wParam) == 0x1180) {
            s_main_window = call->hwnd;
            hosthook_log("HOST CONNECT dispatch hwnd=%p wParam=%08lx lParam=%08lx",
                (void *)call->hwnd, (unsigned long)call->wParam,
                (unsigned long)call->lParam);
            hosthook_install_runtime_probes();
        }
        if (call->message == WM_SHOWWINDOW && call->wParam &&
            s_created_page_count == HOSTHOOK_PAGE_COUNT) {
            if (!s_flow_snapshot_done) hosthook_snapshot_flow();
            if (s_flow_snapshot_done) hosthook_log_flow_visibility("HOST_SHOWWINDOW_MESSAGE");
        }
        if (call->message == WM_NCDESTROY && s_flow_descriptor &&
            hosthook_is_flow_page(call->hwnd)) {
            hosthook_log("FLOW WM_NCDESTROY hwnd=%p descriptor=%08lx result=%08lx",
                (void *)call->hwnd, (unsigned long)s_flow_descriptor,
                (unsigned long)hosthook_read_dword((void *)(uintptr_t)s_flow_descriptor));
            hosthook_log_descriptor("page-ncdestroy", s_flow_descriptor);
            hosthook_log_region("provider-list-at-page-ncdestroy",
                hosthook_read_dword((void *)(uintptr_t)(s_flow_descriptor + 0x84)));
            hosthook_log_region("session-list-at-page-ncdestroy",
                hosthook_read_dword((void *)(uintptr_t)(s_flow_descriptor + 0xac)));
            hosthook_restore_callbacks();
        }
        if (call->hwnd == s_other_dialog_window &&
            call->message == WM_NCDESTROY) {
            hosthook_log("HOST SINGLE DIALOG WM_NCDESTROY hwnd=%08lx label=%s",
                (unsigned long)(uintptr_t)call->hwnd,
                s_other_dialog_label ? s_other_dialog_label : "factory return fallback");
            s_other_dialog_window = NULL;
        }
        if (call->hwnd == s_main_window && call->message == WM_ENABLE &&
            call->wParam)
            hosthook_log("HOST MAIN REENABLED hwnd=%p; owner call is returning",
                (void *)call->hwnd);
        if (hosthook_relevant(call->hwnd, call->message)) {
            char class_name[64], title[128];
            DWORD id, style;
            BOOL visible;
            hosthook_window_state(call->hwnd, class_name, title, &id,
                &style, &visible);
            hosthook_log("CWP hwnd=%p id=%lu class=%s title=%s msg=%04lx wp=%08lx lp=%08lx style=%08lx visible=%u",
                (void *)call->hwnd, (unsigned long)id, class_name, title,
                (unsigned long)call->message,
                (unsigned long)call->wParam,
                (unsigned long)call->lParam,
                (unsigned long)style, (unsigned)visible);
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}

__declspec(dllexport) LRESULT CALLBACK getmsg_hook(int code, WPARAM wparam,
                                                     LPARAM lparam)
{
    if (code >= 0 && lparam) {
        const MSG *message = (const MSG *)lparam;
        if (message->message == WM_COMMAND &&
            LOWORD(message->wParam) == 0x1180) {
            s_main_window = message->hwnd;
            hosthook_log("HOST CONNECT dequeued hwnd=%p wParam=%08lx lParam=%08lx remove=%lu",
                (void *)message->hwnd, (unsigned long)message->wParam,
                (unsigned long)message->lParam, (unsigned long)wparam);
            hosthook_install_runtime_probes();
        }
        if (hosthook_relevant(message->hwnd, message->message)) {
            char class_name[64], title[128];
            DWORD id, style;
            BOOL visible;
            hosthook_window_state(message->hwnd, class_name, title, &id,
                &style, &visible);
            hosthook_log("GETMSG hwnd=%p id=%lu class=%s title=%s msg=%04lx wp=%08lx lp=%08lx remove=%lu style=%08lx visible=%u",
                (void *)message->hwnd, (unsigned long)id, class_name, title,
                (unsigned long)message->message,
                (unsigned long)message->wParam,
                (unsigned long)message->lParam,
                (unsigned long)wparam, (unsigned long)style,
                (unsigned)visible);
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        s_vectored_handler = AddVectoredExceptionHandler(1, hosthook_veh);
    } else if (reason == DLL_PROCESS_DETACH) {
        hosthook_restore_runtime_patches();
        if (s_vectored_handler) RemoveVectoredExceptionHandler(s_vectored_handler);
    }
    return TRUE;
}
