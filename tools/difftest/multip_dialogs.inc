/* Host dialog/COM stand-ins execute the recovered callbacks. Stable-address
 * heap blocks retain leaks and partial writes; ShellExecuteA is always patched. */
#define DIALOG_BLOCKS 128
#define DIALOG_BLOCK_SIZE 8192
#define DIALOG_LOGS 1024
#define DIALOG_STEPS 64
struct dialog_results {
    uint32_t calls, blocks, log[DIALOG_LOGS][8], live[DIALOG_BLOCKS], sizes[DIALOG_BLOCKS];
    uint32_t returns[DIALOG_STEPS], state_hash[DIALOG_STEPS], heap_hash[DIALOG_STEPS];
    uint32_t data[1024];
    uint8_t heap[DIALOG_BLOCKS][DIALOG_BLOCK_SIZE];
};
static volatile LONG s_dialog_active, s_dialog_host_access;
static uint32_t s_dialog_outcome;
static void *s_dialog_jump[5];
static void dialog_escape(void) { __builtin_longjmp(s_dialog_jump, 1); }
static struct dialog_results s_dialogs;
static const struct multip_module *s_dialog_module;
static uint32_t s_dialog_number, s_dialog_fail_call, s_dialog_clock, s_dialog_step;
static struct send_queue s_dialog_queues[3];
static struct { void *list; uint32_t index; } s_dialog_removals[64];
static uint32_t s_dialog_removal_count;
static uint32_t s_dialog_mode, s_dialog_enum_pass, s_dialog_name_calls;
static uint8_t s_dialog_global[0x1200], s_dialog_local[256];
static uint8_t s_dialog_host[0x320], s_dialog_util[0xb0], s_dialog_numeric[0x24];
static void *s_dialog_vtable[40];
static void **s_dialog_object = s_dialog_vtable;
static void **s_dialog_connected_object = s_dialog_vtable;
static uint32_t s_dialog_application[4] = {0x12345678,0x9abcdef0,0x87654321,0xabcdef01};
static const char s_dialog_player_name[] = "fixture player";
static const char s_dialog_name_code[] = "CP";
static uint8_t s_dialog_description[80];
static uint32_t dialog_code_id(const void *pointer)
{
    uint32_t index;
    for (index = 0; index <= 265; index++)
        if (pointer == send_internal(s_dialog_module, index)) return index;
    return 0;
}
static uint32_t dialog_address(const void *pointer)
{
    uintptr_t value = (uintptr_t)pointer, base = (uintptr_t)s_dialog_module->data;
    if (value >= base && value < base + 0x1000) return 0xe000 + value - base;
    base = (uintptr_t)s_dialogs.heap;
    if (value >= base && value < base + sizeof(s_dialogs.heap)) return 0x100000 + value - base;
    if (value >= (uintptr_t)s_dialog_module->module_handle &&
        value < (uintptr_t)s_dialog_module->module_handle + 0x50000) {
        uint32_t id = dialog_code_id(pointer);
        if (id != 0) return 0x200000 + id;
    }
    if (value == 0 || value < 0x10000) return value;
    /* Logging needs the destination's identity only for owned/global cells.
     * Caller-local scratch cells are normalized to a common stack marker. */
    if (value >= (uintptr_t)s_dialog_global && value < (uintptr_t)s_dialog_global + sizeof(s_dialog_global))
        return 0x300000 + value - (uintptr_t)s_dialog_global;
    if (value >= (uintptr_t)s_dialog_local && value < (uintptr_t)s_dialog_local + sizeof(s_dialog_local))
        return 0x400000 + value - (uintptr_t)s_dialog_local;
    {
        NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
        if (value >= (uintptr_t)tib->StackLimit && value < (uintptr_t)tib->StackBase) return 0x500000;
    }
    return 0xffffffffu;
}
static uint32_t dialog_word(uint32_t value)
{
    uint32_t address = dialog_address((void *)(uintptr_t)value);
    return address == 0xffffffffu ? value : address;
}
static uint32_t *dialog_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_dialogs.calls >= DIALOG_LOGS) { fprintf(stderr, "dialog fixture log exhausted\n"); exit(2); }
    entry = s_dialogs.log[s_dialogs.calls++]; entry[0] = tag;
    entry[6] = get_u32(s_dialog_module->data, M_CONNECTED);
    entry[7] = get_u32(s_dialog_module->data, 0xaa8);
    return entry;
}
static int dialog_fail(void) { return s_dialogs.calls == s_dialog_fail_call; }
static int32_t STDCALL dialog_alloc(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    uint32_t *entry = dialog_log(1), index = s_dialogs.blocks;
    entry[1] = flags; entry[2] = size; entry[3] = zero; entry[4] = dialog_address(out);
    if (dialog_fail()) { *out = NULL; return -1; }
    if (index >= DIALOG_BLOCKS || size > DIALOG_BLOCK_SIZE) {
        fprintf(stderr, "dialog heap exhausted: %lu blocks, size %lu\n", (unsigned long)index, (unsigned long)size); exit(2);
    }
    *out = s_dialogs.heap[index]; s_dialogs.live[index] = 1; s_dialogs.sizes[index] = size;
    memset(*out, zero ? 0 : 0xa5, size); s_dialogs.blocks++;
    return 0;
}
static int32_t STDCALL dialog_free(void **out)
{
    uint32_t *entry = dialog_log(2), index;
    entry[1] = dialog_address(out); s_dialog_host_access = 1;
    entry[2] = dialog_address(*out); s_dialog_host_access = 0;
    if (dialog_fail()) return -1;
    if (*out == NULL) return 0;
    for (index = 0; index < s_dialogs.blocks; index++) if (*out == s_dialogs.heap[index]) {
        /* Stale references can be freed after a prior ignored cleanup failure.
         * The host allocator does not expose a liveness check at this boundary. */
        s_dialogs.live[index] = 0; *out = NULL; return 0;
    }
    /* Failed local CreatePlayer leaves its name pointing into MULTIP data
     * and +0x0c clear. Disconnect then treats it as a remote heap name.
     * The real host passes this invalid address to MSVCRT free. */
    if (*out == s_dialog_module->data + 0x9a8) {
        s_dialog_outcome = 2; s_dialog_active = 0; dialog_escape();
    }
    fprintf(stderr, "dialog invalid free %p scenario %lu caller %p\n", *out, (unsigned long)s_dialog_number, __builtin_return_address(0)); exit(2);
}
static int32_t STDCALL dialog_realloc(int32_t flags, uint32_t size, void **out)
{
    void *old = *out;
    uint32_t index, old_size = 0;
    for (index = 0; index < s_dialogs.blocks; index++) if (old == s_dialogs.heap[index]) old_size = s_dialogs.sizes[index];
    if (dialog_alloc(flags, size, out, 0) != 0) { *out = old; return -1; }
    if (old != NULL) {
        memcpy(*out, old, size < old_size ? size : old_size);
        for (index = 0; index < s_dialogs.blocks - 1; index++) if (old == s_dialogs.heap[index]) s_dialogs.live[index] = 0;
    }
    return 0;
}
static uint32_t dialog_hash(const void *bytes, uint32_t size)
{
    s_dialog_host_access = 1;
    const uint8_t *data = bytes;
    uint32_t index, hash = 2166136261u;
    for (index = 0; index + 4 <= size; index += 4) {
        uint32_t word = dialog_word(get_u32(data, index));
        hash = (hash ^ word) * 16777619u;
    }
    for (; index < size; index++) hash = (hash ^ data[index]) * 16777619u;
    s_dialog_host_access = 0; return hash;
}
/* Valid host copy/set/string services write their outputs before an injected
 * error return. This retains deterministic caller-stack bytes. */
static int32_t STDCALL dialog_copy(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = dialog_log(3);
    entry[1] = dialog_address(dest); entry[2] = size; entry[3] = dialog_hash(source, size);
    s_dialog_host_access = 1; memmove(dest, source, size); s_dialog_host_access = 0;
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL dialog_memset(void *dest, int32_t byte, uint32_t size)
{
    uint32_t *entry = dialog_log(4);
    entry[1] = dialog_address(dest); entry[2] = byte; entry[3] = size;
    s_dialog_host_access = 1; memset(dest, byte, size); s_dialog_host_access = 0;
    return dialog_fail() ? -1 : 0;
}
static uint32_t dialog_text_hash(const char *text)
{
    uint32_t hash;
    s_dialog_host_access = 1; hash = profile_hash(text); s_dialog_host_access = 0;
    return hash;
}
static uint32_t STDCALL dialog_strlen(const char *text)
{ dialog_log(5)[1] = dialog_text_hash(text); return strlen(text); }
static int32_t STDCALL dialog_strcpy(char *dest, const char *text)
{
    uint32_t *entry = dialog_log(6);
    entry[1] = dialog_address(dest); entry[2] = dialog_text_hash(text);
    s_dialog_host_access = 1; strcpy(dest, text); s_dialog_host_access = 0;
    return dialog_fail() ? -1 : 0;
}
static uint32_t STDCALL dialog_ticks(void)
{ dialog_log(7); s_dialog_clock += s_dialog_number & 1 ? 1001 : 0xfffffff0u; return s_dialog_clock; }
static void *STDCALL dialog_add(void *list)
{
    uint32_t *entry = dialog_log(8), count = get_u32(list, 0), size = get_u32(list, 12);
    uint8_t *data = (uint8_t *)(uintptr_t)get_u32(list, 24);
    entry[1] = dialog_address(list); entry[2] = count; entry[3] = size;
    if (size == 0 || count >= 8 || size > 256) { fprintf(stderr, "dialog list limit\n"); exit(2); }
    if (count == get_u32(list, 4)) {
        uint8_t *next;
        uint32_t capacity = count + get_u32(list, 8);
        /* Host's fatal list growth is outside injected fallible calls. */
        uint32_t saved_failure = s_dialog_fail_call;
        s_dialog_fail_call = 0;
        dialog_alloc(1, capacity * size, (void **)&next, 1);
        s_dialog_fail_call = saved_failure;
        if (count) memcpy(next, data, count * size);
        if (data != NULL) {
            uint32_t index;
            for (index = 0; index < s_dialogs.blocks; index++) if (data == s_dialogs.heap[index]) s_dialogs.live[index] = 0;
        }
        data = next; put_u32(list, 4, capacity); put_u32(list, 24, (uint32_t)(uintptr_t)data);
    }
    memset(data + count * size, 0, size); put_u32(list, 0, count + 1);
    return data + count * size;
}
static void dialog_remove_index(void *list, uint32_t index)
{
    uint32_t count = get_u32(list, 0) - 1, size = get_u32(list, 12);
    uint8_t *data = (uint8_t *)(uintptr_t)get_u32(list, 24);
    put_u32(list, 0, count);
    if ((int32_t)index < (int32_t)count)
        memmove(data + index * size, data + (index + 1) * size, (count - index) * size);
}
static void STDCALL dialog_remove(void *list, void *record)
{
    uint32_t *entry = dialog_log(9), size = get_u32(list, 12);
    uint8_t *data = (uint8_t *)(uintptr_t)get_u32(list, 24);
    uint32_t index = ((uint32_t)(uintptr_t)record - (uint32_t)(uintptr_t)data) / size;
    entry[1] = dialog_address(list); entry[2] = index;
    if (get_u32(list, 16) != 0) {
        if (s_dialog_removal_count == 64) exit(2);
        put_u32(list, 20, 1);
        s_dialog_removals[s_dialog_removal_count].list = list;
        s_dialog_removals[s_dialog_removal_count++].index = index;
    } else dialog_remove_index(list, index);
}
static int32_t STDCALL dialog_walk(void *list, void *context, record_visitor_fn callback)
{
    uint32_t *entry = dialog_log(10), index = 0;
    int32_t result = 0;
    entry[1] = dialog_address(list); entry[2] = dialog_address(context); entry[3] = dialog_code_id((void *)callback);
    put_u32(list, 16, get_u32(list, 16) + 1);
    while (index < get_u32(list, 0)) {
        result = callback((void *)(uintptr_t)(get_u32(list, 24) + index * get_u32(list, 12)), context);
        if (result != 0) break;
        index++;
    }
    put_u32(list, 16, get_u32(list, 16) - 1);
    /* HOST 0x4102a0 applies queued indexes in reverse after the visitor exits,
     * even when the visitor returned an error. Removals do not call +0x300. */
    if (get_u32(list, 20) != 0) {
        index = s_dialog_removal_count;
        while (index != 0) {
            index--;
            if (s_dialog_removals[index].list == list) {
                dialog_remove_index(list, s_dialog_removals[index].index);
                memmove(s_dialog_removals + index, s_dialog_removals + index + 1,
                    (--s_dialog_removal_count - index) * sizeof(s_dialog_removals[0]));
            }
        }
        put_u32(list, 20, 0);
    }
    return result;
}
static void *STDCALL dialog_find(void *list, uint32_t offset, const void *key, uint32_t size, void *compare)
{
    uint32_t *entry = dialog_log(11), index;
    entry[1] = dialog_address(list); entry[2] = offset; entry[3] = size; entry[4] = compare != NULL;
    for (index = 0; index < get_u32(list, 0); index++) {
        uint8_t *record = (uint8_t *)(uintptr_t)get_u32(list, 24) + index * get_u32(list, 12);
        if (memcmp(record + offset, key, size) == 0) return record;
    }
    return NULL;
}
static uint32_t STDCALL dialog_clear_list(void *list)
{ dialog_log(12)[1] = dialog_address(list); put_u32(list, 0, 0); return (uint32_t)(uintptr_t)list; }
static uint32_t STDCALL dialog_format(const void *value, uint32_t kind, void *text)
{
    uint32_t *entry = dialog_log(13);
    entry[1] = dialog_address(value); entry[2] = get_u32(value, 0); entry[3] = kind; entry[4] = dialog_address(text);
    snprintf(text, 8, "%lu", (unsigned long)(get_u32(value, 0) % 1000000)); return 0x12345678;
}
static int32_t STDCALL dialog_parse(const void *text, uint32_t kind, void *scratch, void *value)
{
    uint32_t *entry = dialog_log(14);
    entry[1] = dialog_text_hash(text); entry[2] = kind; entry[3] = dialog_address(value); entry[4] = get_u32(value, 0);
    *(uint32_t *)scratch = 0x654321;
    *(uint32_t *)value = (uint32_t)strtoul(text, NULL, 10);
    return dialog_fail() ? 0x1003 : 0;
}
static void STDCALL dialog_read(void *table, uint32_t count, void *records)
{
    uint32_t *entry = dialog_log(15);
    entry[1] = dialog_address(table); entry[2] = count; entry[3] = dialog_address(records);
    /* This is an exchange-boundary model: callbacks are driven explicitly by
     * the dialog engine below, not by pretending to implement UTIL's engine. */
    if (table == s_dialog_module->data + 0x468) {
        put_u32(records, 0x24, s_dialog_mode); put_u32(records, 0x2c, (uint32_t)(uintptr_t)(s_dialog_number & 2 ? "" : "local"));
        put_u32(records, 0x4c, (uint32_t)(uintptr_t)(s_dialog_number & 4 ? "" : "session"));
        put_u32(records, 0x60, (uint32_t)(uintptr_t)"8"); put_u32(records, 0x74, (uint32_t)(uintptr_t)"3");
    } else if (table == s_dialog_module->data + 0x5e8 || table == s_dialog_module->data + 0x638) {
        put_u32(records, 0x20, (uint32_t)(uintptr_t)"8"); put_u32(records, 0x34, (uint32_t)(uintptr_t)"3");
    }
}
static int32_t STDCALL dialog_write(void *table, uint32_t count, void *records, uint32_t mode)
{
    uint32_t *entry = dialog_log(16);
    entry[1] = dialog_address(table); entry[2] = count; entry[3] = dialog_address(records); entry[4] = mode;
    if (table == s_dialog_module->data + 0x468) put_u32(s_dialog_module->data, 0x1a4, s_dialog_mode + 1);
    return dialog_fail();
}
static uint32_t STDCALL dialog_confirm(uint32_t flags, uint32_t tag, uint32_t text,
    uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t *entry = dialog_log(17);
    entry[1] = flags; entry[2] = tag; entry[3] = text < 0x10000 ? text : dialog_text_hash((const char *)(uintptr_t)text);
    entry[4] = a | b | c | d | e; return s_dialog_number & 1 ? 0x40 : 0;
}
static void STDCALL dialog_progress(void **out, uint32_t tag, uint32_t text,
    uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t *entry = dialog_log(18);
    entry[1] = tag; entry[2] = text; entry[3] = a | b | c | d | e;
    *out = (void *)0x1234;
}
static void STDCALL dialog_progress_close(void **out)
{ dialog_log(19)[1] = (uint32_t)(uintptr_t)*out; *out = NULL; }
static HINSTANCE WINAPI dialog_shell(HWND window, const char *verb, const char *file,
    const char *parameters, const char *directory, int show)
{
    uint32_t *entry = dialog_log(20);
    entry[1] = window != NULL; entry[2] = verb != NULL; entry[3] = dialog_text_hash(file);
    entry[4] = parameters != NULL || directory != NULL; entry[5] = show;
    return (HINSTANCE)(uintptr_t)(s_dialog_number & 1 ? 33 : 2);
}
/* Patch by import name across all DLLs, including USER32/SHELL32. */
static void dialog_patch(const struct multip_module *module, const char *name, void *replacement)
{
    uint8_t *base = (uint8_t *)module->module_handle;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *descriptor = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[1].VirtualAddress);
    for (; descriptor->Name; descriptor++) {
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + descriptor->OriginalFirstThunk);
        IMAGE_THUNK_DATA *slots = (IMAGE_THUNK_DATA *)(base + descriptor->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) if (!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) {
            IMAGE_IMPORT_BY_NAME *import = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (strcmp((char *)import->Name, name) == 0) {
                DWORD protection;
                if (!VirtualProtect(&slots->u1.Function, 4, PAGE_READWRITE, &protection)) exit(2);
                slots->u1.Function = (DWORD)(uintptr_t)replacement;
                VirtualProtect(&slots->u1.Function, 4, protection, &protection); return;
            }
        }
    }
    if (GetProcAddress(module->module_handle, "multip_test_entries") == NULL) { fprintf(stderr, "missing dialog import %s\n", name); exit(2); }
}
static BOOL WINAPI dialog_signal(HANDLE handle)
{
    dialog_log(39)[1] = (uint32_t)(uintptr_t)handle;
    return !dialog_fail();
}
__attribute__((naked)) static int32_t dialog_sync_painted(void *function __attribute__((unused)))
{
    __asm__ volatile("movl 4(%esp), %eax\n\tsubl $0x400, %esp\n\t"
        "movl $0x98765432, 0x248(%esp)\n\tmovl $0x12345678, 0x244(%esp)\n\t"
        "movl $0xabcdef01, 0x24c(%esp)\n\taddl $0x300, %esp\n\tpushl $0\n\t"
        "call *%eax\n\taddl $0x100, %esp\n\tret");
}
static void dialog_queues_init(void)
{
    uint32_t index;
    memset(s_dialog_queues, 0, sizeof(s_dialog_queues));
    for (index = 0; index < 3; index++) InitializeCriticalSection(&s_dialog_queues[index].lock);
    put_u32(s_dialog_module->data, 0xe60, (uint32_t)(uintptr_t)&s_dialog_queues[0]);
    put_u32(s_dialog_module->data, 0xe64, (uint32_t)(uintptr_t)&s_dialog_queues[1]);
    put_u32(s_dialog_module->data, 0xdf8, (uint32_t)(uintptr_t)&s_dialog_queues[2]);
    put_u32(s_dialog_module->data, 0xe5c, 0x7777);
    dialog_patch(s_dialog_module, "SetEvent", dialog_signal);
}
static void dialog_queues_finish(void)
{
    uint32_t index;
    for (index = 0; index < 3; index++) {
        while (s_dialog_queues[index].lock.RecursionCount) LeaveCriticalSection(&s_dialog_queues[index].lock);
        DeleteCriticalSection(&s_dialog_queues[index].lock);
    }
}
static int32_t STDCALL dialog_factory(const GUID *guid, void **out, void *outer)
{
    uint32_t *entry = dialog_log(21);
    entry[1] = guid ? dialog_hash(guid, 16) : 0; entry[2] = outer != NULL;
    if (dialog_fail()) { *out = NULL; return -1; }
    *out = &s_dialog_object; return 0;
}
static int32_t STDCALL dialog_query(void *object, const GUID *iid, void **out)
{
    uint32_t *entry = dialog_log(22);
    entry[1] = object == &s_dialog_object ? 1 : object == &s_dialog_connected_object ? 2 : 0; entry[2] = dialog_hash(iid, 16); entry[3] = dialog_address(out);
    *out = &s_dialog_object; return dialog_fail() ? -1 : 0;
}
static uint32_t STDCALL dialog_release(void *object)
{ dialog_log(23)[1] = object == &s_dialog_object ? 1 : object == &s_dialog_connected_object ? 2 : 0; return 0xabcdef01; }
static int32_t STDCALL dialog_provider_enum(provider_callback_fn callback, void *context)
{
    uint32_t *entry = dialog_log(24), index;
    entry[1] = callback == (provider_callback_fn)send_internal(s_dialog_module, 222); entry[2] = context != NULL;
    for (index = 0; index < 3; index++) {
        GUID guid = {0x12345678 + index,0x4321,0x9876,{1,2,3,4,5,6,7,8}};
        if (!callback(&guid, index == 0 ? "TCP/IP" : index == 1 ? "IPX" : "Modem", 5, index, context)) break;
    }
    return s_dialog_number % 29 == 28 ? 1 : 0;
}
static int32_t STDCALL dialog_session_enum(void *object, void *description, uint32_t timeout,
    int32_t (STDCALL *callback)(const void *, uint32_t *, uint32_t, void *), void *context, uint32_t flags)
{
    uint32_t *entry = dialog_log(25), index, pass = s_dialog_enum_pass++;
    uint32_t callback_timeout = 999;
    entry[1] = object == &s_dialog_object; entry[2] = dialog_hash(description, 80);
    entry[3] = timeout; entry[4] = flags; entry[5] = dialog_code_id((void *)callback);
    for (index = 0; index < 3; index++) {
        uint8_t session[80];
        int32_t result;
        memcpy(session, s_dialog_description, 80);
        put_u32(session, 8, 0x65432100u + (pass & 1 ? (index + 1) % 3 : index));
        put_u32(session, 0x30, (uint32_t)(uintptr_t)(index & 1 ? "second session" : "first session"));
        if (pass & 1 && index == 2) continue;
        result = callback(session, &callback_timeout, 0, context);
        dialog_log(26)[1] = result;
        if (result == 0) break;
    }
    dialog_log(26)[1] = callback(NULL, &callback_timeout, 1, context);
    return s_dialog_number % 31 == 30 ? 1 : 0;
}
static int32_t STDCALL dialog_player_enum(void *object, const GUID *instance,
    session_enum_callback callback, void *context, uint32_t flags)
{
    uint32_t *entry = dialog_log(27), index;
    uint32_t name[4] = {16,0,(uint32_t)(uintptr_t)s_dialog_player_name,(uint32_t)(uintptr_t)s_dialog_name_code};
    entry[1] = object == &s_dialog_object; entry[2] = instance ? dialog_hash(instance,16) : 0;
    entry[3] = flags; entry[4] = context != NULL;
    for (index = 0; index < 2; index++) {
        int32_t result = callback(0x8000 + index, 1, name, 0, context);
        dialog_log(28)[1] = result;
        if (result == 0) break;
    }
    return s_dialog_number % 7 == 6 ? -1 : 0;
}
static int32_t STDCALL dialog_get_name(void *object, uint32_t id, void *name, uint32_t *size)
{
    uint32_t *entry = dialog_log(29);
    entry[1] = object == &s_dialog_object; entry[2] = id; entry[3] = *size; entry[4] = name != NULL;
    s_dialog_name_calls++;
    if (name == NULL || *size < 16) { *size = 16; return (int32_t)0x8877001eu; }
    put_u32(name, 0, 16); put_u32(name, 4, 0x13579bdf);
    put_u32(name, 8, (uint32_t)(uintptr_t)s_dialog_player_name); put_u32(name, 12, (uint32_t)(uintptr_t)s_dialog_name_code);
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL dialog_set_name(void *object, uint32_t id, const void *name, uint32_t flags)
{
    uint32_t *entry = dialog_log(30);
    entry[1] = object == &s_dialog_object; entry[2] = id; entry[3] = flags;
    entry[4] = dialog_text_hash((const char *)(uintptr_t)get_u32(name, 12));
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL dialog_get_description(void *object, void *description, uint32_t *size)
{
    uint32_t *entry = dialog_log(31);
    entry[1] = object == &s_dialog_object; entry[2] = description != NULL; entry[3] = *size;
    *size = 80;
    if (description == NULL) return (int32_t)0x8877001eu;
    memcpy(description, s_dialog_description, 80); return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL dialog_set_description(void *object, void *description, uint32_t flags)
{
    uint32_t *entry = dialog_log(32);
    entry[1] = object == &s_dialog_object; entry[2] = dialog_hash(description, 80); entry[3] = flags;
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL dialog_group(void *object, uint32_t group, uint32_t id)
{
    uint32_t *entry = dialog_log(33);
    entry[1] = object == &s_dialog_object; entry[2] = group; entry[3] = id;
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL dialog_string(uint32_t id, char **out)
{
    dialog_log(34)[1] = id;
    /* String lookup returns a host-owned copy, just as +0x2a4 does. */
    if (dialog_alloc(1, 16, (void **)out, 0) != 0) return 0;
    strcpy(*out, "Remove %s?"); return 10;
}
static int32_t STDCALL dialog_export_loader(void *descriptor)
{
    dialog_log(35)[1] = dialog_address(descriptor);
    put_u32(s_dialog_module->data, 0x98c, (uint32_t)(uintptr_t)dialog_factory);
    put_u32(s_dialog_module->data, 0x990, (uint32_t)(uintptr_t)dialog_provider_enum);
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL dialog_unload(void *descriptor)
{ dialog_log(36)[1] = dialog_address(descriptor); return dialog_fail() ? -1 : 0; }
static void dialog_snapshot(int32_t result)
{
    uint32_t index, offset, hash = 2166136261u;
    uint32_t step = s_dialog_step++;
    if (step >= DIALOG_STEPS) exit(2);
    s_dialogs.returns[step] = result;
    for (index = 0xa0; index < 0xf38; index += 4) {
        uint32_t value = get_u32(s_dialog_module->data, index);
        s_dialogs.data[index / 4] = value == get_u32(s_dialog_module->initial_data, index) ?
            0xb0000000u + index : dialog_word(value);
        hash = (hash ^ s_dialogs.data[index / 4]) * 16777619u;
    }
    for (index = 0; index < 3; index++) {
        hash = (hash ^ s_dialog_queues[index].count) * 16777619u;
        hash = (hash ^ dialog_word((uint32_t)(uintptr_t)s_dialog_queues[index].head)) * 16777619u;
        hash = (hash ^ dialog_word((uint32_t)(uintptr_t)s_dialog_queues[index].tail)) * 16777619u;
    }
    s_dialogs.state_hash[step] = hash; hash = 2166136261u;
    for (index = 0; index < s_dialogs.blocks; index++) {
        hash = (hash ^ s_dialogs.live[index]) * 16777619u;
        for (offset = 0; offset + 4 <= s_dialogs.sizes[index]; offset += 4)
            hash = (hash ^ dialog_word(get_u32(s_dialogs.heap[index], offset))) * 16777619u;
        for (; offset < s_dialogs.sizes[index]; offset++) hash = (hash ^ s_dialogs.heap[index][offset]) * 16777619u;
    }
    s_dialogs.heap_hash[step] = hash;
}
typedef int32_t (STDCALL *dialog_callback_fn)(void *);
typedef int32_t (STDCALL *dialog_index_fn)(void *, uint32_t);
typedef int32_t (*dialog_entry_fn)(void);
static int32_t dialog_callback(void *records, uint32_t offset, uint32_t expected)
{
    uint32_t address = get_u32(records, offset);
    uint32_t *entry = dialog_log(37);
    entry[1] = offset; entry[2] = dialog_code_id((void *)(uintptr_t)address);
    if (entry[2] != expected) { fprintf(stderr, "dialog callback mapping %lu expected %lu\n", (unsigned long)entry[2], (unsigned long)expected); exit(2); }
    return ((dialog_callback_fn)(uintptr_t)address)(records);
}
static int32_t STDCALL dialog_engine(uint32_t id, void *records)
{
    uint32_t *entry = dialog_log(38);
    entry[1] = id; entry[2] = dialog_address(records);
    if (dialog_fail()) return -1;
    if (id == 0x110a) {
        uint32_t result;
        dialog_snapshot(dialog_callback(records, 8, 235));
        if (get_u32(records, 0x88) != 0) {
            dialog_snapshot(dialog_callback(records, 4, 234));
            if (get_u32(s_dialog_module->data, 0x988) != 0) {
                dialog_snapshot(dialog_callback(records, 0x94, 229));
                dialog_snapshot(dialog_callback(records, 0xa0, 231));
                if (get_u32(records, 0xb0) != 0) dialog_snapshot(dialog_callback(records, 0x9c, 239));
            }
        }
        dialog_snapshot(dialog_callback(records, 0x44, 238));
        dialog_snapshot(dialog_callback(records, 0x10, 236));
        result = dialog_callback(records, 0xc, 237);
        dialog_snapshot(result);
        put_u32(records, 0, s_dialog_number % 4 == 0 ? 0 : result == 0);
    } else if (id == 0x110b) {
        dialog_snapshot(dialog_callback(records, 4, 249));
        dialog_snapshot(dialog_callback(records, 8, 245));
        if (get_u32(s_dialog_module->data, M_CONNECTED) != 0) {
            dialog_snapshot(dialog_callback(records, 12, 248));
            dialog_snapshot(dialog_callback(records, 16, 246));
            dialog_snapshot(dialog_sync_painted((void *)(uintptr_t)get_u32(records, 20)));
        }
        put_u32(records, 0, s_dialog_number & 1);
    } else if (id == 0x110c) {
        dialog_snapshot(dialog_callback(records, 4, 256));
        dialog_snapshot(dialog_callback(records, 8, 253));
        /* Aim at the interval boundary using this side's published period. */
        put_u32(s_dialog_module->data, 0xe8c,
            s_dialog_clock + (s_dialog_number & 1 ? 1001 : 0xfffffff0u) -
            (get_u32(s_dialog_module->data, 0x178) * 1000 + s_dialog_number % 3 - 1));
        dialog_snapshot(dialog_callback(records, 16, 255));
        if (get_u32(records, 0x50) != 0) dialog_snapshot(dialog_callback(records, 12, 254));
    } else { fprintf(stderr, "unexpected dialog %lu\n", (unsigned long)id); exit(2); }
    return s_dialog_number % 23 == 22 ? -1 : 0;
}
static void dialog_setup(const struct multip_module *module, uint32_t number)
{
    s_dialog_module = module; s_dialog_number = number; s_dialog_step = 0; s_dialog_outcome = 0; s_dialog_removal_count = 0;
    s_dialog_fail_call = number % 211; s_dialog_clock = number * 0x1234567u;
    s_dialog_mode = (number / 5) % 4; s_dialog_enum_pass = 0; s_dialog_name_calls = 0;
    memset(&s_dialogs, 0, sizeof(s_dialogs)); memset(s_dialog_host, 0, sizeof(s_dialog_host));
    memset(s_dialog_util, 0, sizeof(s_dialog_util)); memset(s_dialog_numeric, 0, sizeof(s_dialog_numeric));
    memset(s_dialog_vtable, 0, sizeof(s_dialog_vtable)); memset(s_dialog_global, 0, sizeof(s_dialog_global));
    memset(s_dialog_local, 0, sizeof(s_dialog_local)); memcpy(module->data, module->initial_data, 0x1000);
#define DIALOG_SLOT(table, offset, function) put_u32(table, offset, (uint32_t)(uintptr_t)function)
    DIALOG_SLOT(s_dialog_host, 0x20c, dialog_alloc); DIALOG_SLOT(s_dialog_host, 0x210, dialog_realloc);
    DIALOG_SLOT(s_dialog_host, 0x214, dialog_free); DIALOG_SLOT(s_dialog_host, 0x218, dialog_memset);
    DIALOG_SLOT(s_dialog_host, 0x21c, dialog_copy); DIALOG_SLOT(s_dialog_host, 0x2a8, dialog_strcpy);
    DIALOG_SLOT(s_dialog_host, 0x2b8, dialog_strlen); DIALOG_SLOT(s_dialog_host, 0x2a4, dialog_string);
    DIALOG_SLOT(s_dialog_host, 0x2f8, dialog_add); DIALOG_SLOT(s_dialog_host, 0x300, dialog_remove);
    DIALOG_SLOT(s_dialog_host, 0x304, dialog_clear_list); DIALOG_SLOT(s_dialog_host, 0x308, dialog_walk);
    DIALOG_SLOT(s_dialog_host, 0x310, dialog_find); DIALOG_SLOT(s_dialog_host, 0x38, dialog_ticks);
    DIALOG_SLOT(s_dialog_host, 0x10c, dialog_engine); DIALOG_SLOT(s_dialog_host, 0x118, dialog_engine);
    DIALOG_SLOT(s_dialog_host, 0x124, dialog_export_loader); DIALOG_SLOT(s_dialog_host, 0x128, dialog_unload);
    DIALOG_SLOT(s_dialog_util, 0x8c, dialog_write); DIALOG_SLOT(s_dialog_util, 0x90, dialog_read);
    DIALOG_SLOT(s_dialog_util, 0xa4, dialog_confirm); DIALOG_SLOT(s_dialog_util, 0xa8, dialog_progress);
    DIALOG_SLOT(s_dialog_util, 0xac, dialog_progress_close);
    DIALOG_SLOT(s_dialog_numeric, 0x18, dialog_format); DIALOG_SLOT(s_dialog_numeric, 0x20, dialog_parse);
#undef DIALOG_SLOT
    s_dialog_vtable[0] = dialog_query; s_dialog_vtable[2] = dialog_release;
    s_dialog_vtable[0x30/4] = dialog_player_enum; s_dialog_vtable[0x34/4] = dialog_session_enum;
    s_dialog_vtable[0x54/4] = dialog_get_name; s_dialog_vtable[0x78/4] = dialog_set_name;
    s_dialog_vtable[0x58/4] = dialog_get_description; s_dialog_vtable[0x7c/4] = dialog_set_description;
    s_dialog_vtable[0xc/4] = dialog_group; s_dialog_vtable[0x1c/4] = dialog_group;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)s_dialog_host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_dialog_global);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)s_dialog_util);
    put_u32(module->data, 0x3c, (uint32_t)(uintptr_t)s_dialog_numeric);
    put_u32(module->data, 0xa38, (uint32_t)(uintptr_t)s_dialog_application);
    put_u32(module->data, 0x98c, (uint32_t)(uintptr_t)dialog_factory);
    put_u32(module->data, 0x990, (uint32_t)(uintptr_t)dialog_provider_enum);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_dialog_object);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_dialog_local);
    put_u32(s_dialog_local, 4, 0x1234); put_u32(s_dialog_local, 8, 1);
    put_u32(module->data, M_CONNECTED, 0); put_u32(module->data, 0xaa8, number & 1);
    put_u32(module->data, 0xac0, number & 1); put_u32(module->data, 0x178, number % 4);
    put_u32(module->data, 0xe8c, s_dialog_clock - number % 3 * 1000);
    put_u32(s_dialog_global, 0x764, number & 1); put_u32(s_dialog_global, 0x84c, number & 2);
    memset(s_dialog_description, 0, 80); put_u32(s_dialog_description, 0, 80);
    memcpy(s_dialog_description + 0x18, s_dialog_application, 16);
    put_u32(s_dialog_description, 0x30, (uint32_t)(uintptr_t)"current session");
    put_u32(s_dialog_description, 0x40, 0x08020301);
    strcpy((char *)module->data + 0xb00, "fixture-command-never-executed");
    dialog_patch(module, "ShellExecuteA", dialog_shell);
}
static LONG CALLBACK dialog_fault(EXCEPTION_POINTERS *exception)
{
    uintptr_t ip = exception->ContextRecord->Eip, base = (uintptr_t)s_dialog_module->module_handle;
    if (s_dialog_active && exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ((ip >= base + 0x1000 && ip < base + 0xd000) || s_dialog_host_access)) {
        s_dialog_active = 0; s_dialog_host_access = 0; s_dialog_outcome = 1;
        exception->ContextRecord->Eip = (DWORD)(uintptr_t)dialog_escape;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void run_dialogs(const struct multip_module *module, uint32_t number, struct dialog_results *results)
{
    uint32_t index, offset;
    dialog_setup(module, number); dialog_queues_init();
    if (__builtin_setjmp(s_dialog_jump) != 0) {
        s_dialogs.returns[63] = s_dialog_outcome;
        dialog_snapshot(-1);
        goto finish;
    }
    s_dialog_active = 1;
    dialog_snapshot(((dialog_entry_fn)send_internal(module, 242))());
    dialog_snapshot(((dialog_entry_fn)get_slot(module, 0x1c))());
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_dialog_object);
    put_u32(module->data, M_CONNECTED, 1);
    dialog_snapshot(((dialog_entry_fn)get_slot(module, 0x1c))());
    dialog_snapshot(((dialog_entry_fn)send_internal(module, 233))());
    /* The shell return never affects the configuration result. */
    put_u32(module->data, 0x3cc, 2);
    dialog_snapshot(((dialog_entry_fn)send_internal(module, 241))());
finish:
    s_dialog_active = 0; s_dialog_host_access = 0;
    for (index = 0; index < s_dialogs.blocks; index++)
        for (offset = 0; offset + 4 <= s_dialogs.sizes[index]; offset += 4)
            put_u32(s_dialogs.heap[index], offset, dialog_word(get_u32(s_dialogs.heap[index], offset)));
    memcpy(results, &s_dialogs, sizeof(*results));
    dialog_queues_finish();
}
static uint32_t test_dialogs(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct dialog_results s_retail, s_ours;
    uint32_t number, bad = 0, faults = 0, hits[65] = {0}, dialogs[3] = {0};
    void *handler = AddVectoredExceptionHandler(1, dialog_fault);
    if (handler == NULL) exit(2);
    for (number = 0; number < scenarios; number++) {
        multip_test_context("dialogs", "retail", number); run_dialogs(retail, number, &s_retail);
        multip_test_context("dialogs", "ours", number); run_dialogs(ours, number, &s_ours);
        {
            uint32_t index;
            for (index = 0; index < s_retail.calls; index++) {
                uint32_t tag = s_retail.log[index][0];
                if (tag < 65) hits[tag]++;
                if (tag == 38 && s_retail.log[index][1] >= 0x110a && s_retail.log[index][1] <= 0x110c)
                    dialogs[s_retail.log[index][1] - 0x110a]++;
            }
        }
        faults += s_retail.returns[63] != 0;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t index, shown = 0;
                printf("dialog scenario %lu differs\n", (unsigned long)number);
                for (index = 0; index < sizeof(s_ours)/4 && shown < 12; index++)
                    if (((uint32_t *)&s_retail)[index] != ((uint32_t *)&s_ours)[index]) {
                        printf("  word %lu: %#lx / %#lx\n", (unsigned long)index,
                            (unsigned long)((uint32_t *)&s_retail)[index], (unsigned long)((uint32_t *)&s_ours)[index]); shown++;
                    }
            }
            bad++;
        }
    }
    RemoveVectoredExceptionHandler(handler);
    if (scenarios >= 1200 && (hits[9] == 0 || hits[20] == 0 || hits[34] == 0 || hits[39] == 0 ||
        dialogs[0] == 0 || dialogs[1] == 0 || dialogs[2] == 0)) {
        fprintf(stderr, "dialog fixture missed a required callback family\n"); exit(2);
    }
    printf("dialog coverage: configuration %lu, options %lu, details %lu, prune %lu, kick %lu, shell %lu\n",
        (unsigned long)dialogs[0], (unsigned long)dialogs[1], (unsigned long)dialogs[2],
        (unsigned long)hits[9], (unsigned long)hits[34], (unsigned long)hits[20]);
    printf("dialog outcomes: %lu fault outcomes\n", (unsigned long)faults);
    printf("MULTIP configuration/options dialogs: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
