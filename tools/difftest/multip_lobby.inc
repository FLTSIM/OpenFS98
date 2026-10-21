/* Lobby/connection callbacks run into real completion/teardown owners over
 * deterministic COM, CHAIN and Win32 worker services. */
static uint8_t s_lobby_chain[0x24], s_lobby_aircraft[0x3c];
static uint32_t s_lobby_connection[8], s_lobby_name[4];
static uint8_t s_lobby_session[80];
static void *s_lobby_vtable[16];
static void **s_lobby_object = s_lobby_vtable;
static uint32_t s_lobby_threads, s_lobby_models;
static uint32_t *volatile s_lobby_reserved_word;
static int32_t STDCALL lobby_settings(void *object, uint32_t flags, uint32_t id, void *connection)
{
    uint32_t *entry = dialog_log(40);
    entry[1] = object == &s_lobby_object; entry[2] = flags; entry[3] = id;
    entry[4] = dialog_hash(connection, 32); entry[5] = dialog_hash((void *)(uintptr_t)get_u32(connection, 8), 80);
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL lobby_connect(void *object, uint32_t flags, void **out, void *outer)
{
    uint32_t *entry = dialog_log(41);
    entry[1] = object == &s_lobby_object; entry[2] = flags; entry[3] = outer != NULL;
    *out = &s_dialog_object; return dialog_fail() ? -1 : 0;
}
static uint32_t STDCALL lobby_release(void *object)
{ dialog_log(42)[1] = object == &s_lobby_object; return 0x76543210; }
static int32_t STDCALL lobby_factory(void *guid, void **out, void *outer, void *reserved, uint32_t flags)
{
    uint32_t *entry = dialog_log(43);
    entry[1] = guid != NULL; entry[2] = outer != NULL; entry[3] = reserved != NULL; entry[4] = flags;
    *out = &s_lobby_object; return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL lobby_get_connection(void **out, uint32_t *size, uint32_t flags)
{
    uint32_t *entry = dialog_log(44);
    entry[1] = *out != NULL; entry[2] = *size; entry[3] = flags;
    *out = s_dialog_number / 6 % 3 == 0 ? NULL : s_lobby_connection;
    *size = 32; return dialog_fail() ? -1 : 0;
}
static int32_t lobby_registration(uint32_t tag, uint32_t code, void *callback)
{
    uint32_t *entry = dialog_log(tag);
    entry[1] = code; entry[2] = dialog_code_id(callback);
    if (tag == 45 && code == 0x13 && s_lobby_reserved_word != NULL) *s_lobby_reserved_word = 0x89abcdef;
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL lobby_add(uint32_t code, void *callback) { return lobby_registration(45, code, callback); }
static int32_t STDCALL lobby_remove(uint32_t code, void *callback) { return lobby_registration(46, code, callback); }
static uint32_t STDCALL lobby_dispatch(uint32_t code, uint32_t event, const void *list)
{
    uint32_t *entry = dialog_log(47);
    entry[1] = code; entry[2] = event;
    entry[3] = list ? get_u32(list, 0) : 0;
    entry[4] = list ? dialog_address((void *)(uintptr_t)get_u32(list, 4)) : 0;
    return 0x56781234;
}
static HANDLE WINAPI lobby_thread(LPSECURITY_ATTRIBUTES attributes, SIZE_T stack,
    LPTHREAD_START_ROUTINE callback, void *argument, DWORD flags, DWORD *id)
{
    uint32_t *entry = dialog_log(48);
    entry[1] = attributes != NULL; entry[2] = stack; entry[3] = dialog_code_id((void *)callback);
    entry[4] = argument != NULL; entry[5] = flags;
    *id = 0x1110 + s_lobby_threads++;
    return dialog_fail() ? NULL : (HANDLE)(uintptr_t)(0x2220 + s_lobby_threads);
}
static DWORD WINAPI lobby_wait(HANDLE handle, DWORD timeout)
{ uint32_t *entry = dialog_log(49); entry[1] = (uint32_t)(uintptr_t)handle; entry[2] = timeout; return WAIT_TIMEOUT; }
static BOOL WINAPI lobby_close_handle(HANDLE handle)
{ dialog_log(50)[1] = (uint32_t)(uintptr_t)handle; return !dialog_fail(); }
static void WINAPI lobby_sleep(DWORD delay) { dialog_log(51)[1] = delay; }
static int32_t STDCALL lobby_create_player(void *object, uint32_t *id, const void *name,
    HANDLE event, void *data, uint32_t size, uint32_t flags)
{
    uint32_t *entry = dialog_log(52);
    entry[1] = object == &s_dialog_object; entry[2] = get_u32(name, 0); entry[3] = get_u32(name, 4);
    entry[4] = dialog_text_hash((const char *)(uintptr_t)get_u32(name, 8));
    entry[5] = dialog_text_hash((const char *)(uintptr_t)get_u32(name, 12));
    entry = dialog_log(53); entry[1] = (uint32_t)(uintptr_t)event; entry[2] = data != NULL; entry[3] = size; entry[4] = flags;
    *id = 0x4500; return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL lobby_create_group(void *object, uint32_t *id, const void *name,
    void *data, uint32_t size, uint32_t flags)
{
    uint32_t *entry = dialog_log(54);
    entry[1] = object == &s_dialog_object; entry[2] = dialog_address(id);
    entry[3] = dialog_text_hash((const char *)(uintptr_t)get_u32(name, 8)); entry[4] = data != NULL; entry[5] = size | flags;
    *id = 0x5000 + (id == (uint32_t *)(s_dialog_module->data + 0xaa4));
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL lobby_open(void *object, void *description, uint32_t flags)
{
    uint32_t *entry = dialog_log(64);
    entry[1] = object == &s_dialog_object; entry[2] = dialog_hash(description, 80); entry[3] = flags;
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL lobby_enum_groups(void *object, const GUID *instance,
    session_enum_callback callback, void *context, uint32_t flags)
{
    uint32_t *entry = dialog_log(55);
    uint32_t name[4] = {16,0,0,0}, index;
    entry[1] = object == &s_dialog_object; entry[2] = instance != NULL; entry[3] = context != NULL; entry[4] = flags;
    for (index = 0; index < 2; index++) {
        name[2] = (uint32_t)(uintptr_t)(s_dialog_module->data + (index ? 0x1c8 : 0x1b0));
        callback(0x5000 + index, 0, name, 0, context);
    }
    return s_dialog_number % 37 == 36 ? (int32_t)0x80004001u : dialog_fail() ? 1 : 0;
}
static int32_t STDCALL lobby_strcmp(const char *left, const char *right)
{
    uint32_t *entry = dialog_log(56);
    int32_t result = strcmp(left, right);
    entry[1] = dialog_text_hash(left); entry[2] = dialog_text_hash(right);
    return result < 0 ? -1 : result > 0;
}
static int32_t STDCALL lobby_aircraft_select(void *aircraft, const char *title)
{
    uint32_t *entry = dialog_log(57);
    void *header, *model;
    entry[1] = dialog_text_hash(title);
    if (dialog_alloc(1, 36, &header, 1) != 0 || dialog_alloc(1, 128, &model, 1) != 0) return 0;
    put_u32(header, 0x18, 1); put_u32(header, 0xc, ++s_lobby_models);
    put_u32(model, 0, 128); put_u32(model, 0x64, (uint32_t)(uintptr_t)"lobby aircraft");
    put_u32(aircraft, 0, (uint32_t)(uintptr_t)header); put_u32(aircraft, 4, (uint32_t)(uintptr_t)model);
    return 1;
}
static int32_t STDCALL lobby_aircraft_model(void *aircraft, uint32_t kind)
{ uint32_t *entry = dialog_log(58); entry[1] = dialog_address(aircraft); entry[2] = kind; return !dialog_fail(); }
static int32_t STDCALL lobby_aircraft_release(void *aircraft)
{ dialog_log(59)[1] = dialog_address(aircraft); return !dialog_fail(); }
static uint32_t STDCALL lobby_aircraft_title(void *aircraft, uint32_t kind, char *title, uint32_t size)
{
    uint32_t *entry = dialog_log(60);
    entry[1] = aircraft == s_dialog_global + 0x11e0; entry[2] = kind; entry[3] = size;
    strcpy(title, "lobby aircraft"); return 1;
}
static int32_t STDCALL lobby_close(void *object)
{ dialog_log(61)[1] = object == &s_dialog_object; return dialog_fail() ? -1 : 0; }
static int32_t STDCALL lobby_destroy(void *object, uint32_t id)
{ uint32_t *entry = dialog_log(62); entry[1] = object == &s_dialog_object; entry[2] = id; return -1; }
static int32_t STDCALL lobby_send(void *object, uint32_t from, uint32_t to, uint32_t flags, void *packet, uint32_t size)
{
    uint32_t *entry = dialog_log(63);
    entry[1] = object == &s_dialog_object; entry[2] = from; entry[3] = to; entry[4] = flags; entry[5] = dialog_hash(packet, size);
    return (int32_t)0x80004005u;
}
static void lobby_setup(const struct multip_module *module, uint32_t number)
{
    static const uint32_t s_flags[] = {0,1,2,3,0x80000001u,0x80000002u};
    dialog_setup(module, number); dialog_queues_init();
    s_lobby_threads = 0; s_lobby_models = 0;
    memset(s_lobby_connection, 0, sizeof(s_lobby_connection));
    memcpy(s_lobby_session, s_dialog_description, 80);
    s_lobby_connection[0] = 32; s_lobby_connection[1] = s_flags[number / 6 % 6];
    s_lobby_connection[2] = (uint32_t)(uintptr_t)s_lobby_session;
    s_lobby_connection[3] = (uint32_t)(uintptr_t)s_lobby_name;
    s_lobby_name[0] = 16; s_lobby_name[1] = 0; s_lobby_name[2] = (uint32_t)(uintptr_t)"lobby local"; s_lobby_name[3] = 0;
    memset(s_lobby_chain, 0, sizeof(s_lobby_chain)); memset(s_lobby_aircraft, 0, sizeof(s_lobby_aircraft));
#define LOBBY_SLOT(table, offset, function) put_u32(table, offset, (uint32_t)(uintptr_t)function)
    LOBBY_SLOT(s_lobby_chain, 0x18, lobby_add); LOBBY_SLOT(s_lobby_chain, 0x1c, lobby_remove); LOBBY_SLOT(s_lobby_chain, 0x20, lobby_dispatch);
    LOBBY_SLOT(s_lobby_aircraft, 0x20, lobby_aircraft_select); LOBBY_SLOT(s_lobby_aircraft, 0x24, lobby_aircraft_release);
    LOBBY_SLOT(s_lobby_aircraft, 0x30, lobby_aircraft_title); LOBBY_SLOT(s_lobby_aircraft, 0x34, lobby_aircraft_model);
    LOBBY_SLOT(s_lobby_aircraft, 0x38, lobby_aircraft_release);
    LOBBY_SLOT(s_dialog_host, 0x120, lobby_get_connection); LOBBY_SLOT(s_dialog_host, 0x2bc, lobby_strcmp);
#undef LOBBY_SLOT
    s_dialog_vtable[0x60/4] = lobby_open; s_dialog_vtable[0x14/4] = lobby_create_group; s_dialog_vtable[0x2c/4] = lobby_enum_groups;
    s_dialog_vtable[0x18/4] = lobby_create_player; s_dialog_vtable[0x10/4] = lobby_close;
    s_dialog_vtable[0x24/4] = lobby_destroy; s_dialog_vtable[0x68/4] = lobby_send;
    s_lobby_vtable[2] = lobby_release; s_lobby_vtable[3] = lobby_connect; s_lobby_vtable[12] = lobby_settings;
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)s_lobby_chain);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)s_lobby_aircraft);
    put_u32(module->data, 0x980, (uint32_t)(uintptr_t)lobby_factory);
    put_u32(module->data, M_LOCAL_PLAYER, 0); put_u32(module->data, M_PLAYER_COUNT, 0); put_u32(module->data, 0x678, 0);
    put_u32(module->data, 0xdf0, 0x1100); put_u32(module->data, 0xdf4, 0x1101);
    put_u32(module->data, 0xe58, 0x1102); put_u32(module->data, 0x1a4, 1 + number / 36 % 3);
    put_u32(module->data, 0xa44, 0x98765432); put_u32(module->data, 0x190, 0x24);
    put_u32(module->data, 0x998, number & 0x100 ? 0xffffffffu : 8); put_u32(module->data, 0x99c, 2);
    dialog_patch(module, "CreateThread", lobby_thread); dialog_patch(module, "WaitForSingleObject", lobby_wait);
    dialog_patch(module, "CloseHandle", lobby_close_handle); dialog_patch(module, "Sleep", lobby_sleep);
}
/* Different words across the deep caller frame expose wrong nesting in the
 * lobby helpers and common completion, rather than hiding DPNAME padding. */
__attribute__((naked)) static int32_t lobby_call_painted(void *function __attribute__((unused)),
    uint32_t count __attribute__((unused)), void *first __attribute__((unused)), void *second __attribute__((unused)), uint32_t reserved_offset __attribute__((unused)))
{
    __asm__ volatile("pushl %esi\n\tpushl %edi\n\tpushl %ebx\n\tmovl 16(%esp), %esi\n\tmovl 20(%esp), %ebx\n\t"
        "movl 24(%esp), %edx\n\tmovl 28(%esp), %ecx\n\tsubl $0x1000, %esp\n\txorl %eax, %eax\n\t"
        "1: leal 0x45670000(%eax), %edi\n\tmovl %edi, (%esp,%eax)\n\taddl $4, %eax\n\tcmpl $0x800, %eax\n\tjb 1b\n\t"
        "addl $0x800, %esp\n\tmovl %esp, %edi\n\tsubl 0x820(%esp), %edi\n\tmovl %edi, _s_lobby_reserved_word\n\tcmpl $2, %ebx\n\tjne 2f\n\tpushl %ecx\n\t2: testl %ebx, %ebx\n\tjz 3f\n\tpushl %edx\n\t"
        "3: call *%esi\n\tmovl $0, _s_lobby_reserved_word\n\taddl $0x800, %esp\n\tpopl %ebx\n\tpopl %edi\n\tpopl %esi\n\tret");
}
static void run_lobby(const struct multip_module *module, uint32_t number, struct dialog_results *results)
{
    uint32_t index, offset, kind = number % 7, entry = 259 + kind;
    uint32_t reserved_offset;
    lobby_setup(module, number);
    if (__builtin_setjmp(s_dialog_jump) != 0) {
        s_dialogs.returns[63] = s_dialog_outcome; dialog_snapshot(-1); goto finish;
    }
    s_dialog_active = 1;
    if (kind == 6) {
        reserved_offset = 0x410;
        put_u32(module->data, M_CONNECTED, number & 0x200 ? 1 : 0);
        put_u32(module->data, M_LOCAL_PLAYER, number & 0x200 ? (uint32_t)(uintptr_t)s_dialog_local : 0);
    } else {
        uint32_t hosting = s_lobby_connection[1] & 2;
        reserved_offset = kind == 1 ? 0x418 : kind == 2 ? 0x414 :
            kind == 3 ? (hosting ? 0x42c : 0x428) : kind == 4 ? (hosting ? 0x43c : 0x438) :
            kind == 5 ? (hosting ? 0x438 : 0x434) : 0;
    }
    dialog_snapshot(lobby_call_painted((kind == 6 ? get_slot(module, 0x18) : send_internal(module, entry)), kind == 4 || kind == 6 ? 0 : kind == 1 || kind == 3 ? 2 : 1,
        kind == 5 ? (number / 216 % 2 ? NULL : (void *)s_lobby_connection) : (void *)&s_lobby_object, s_lobby_connection, reserved_offset));
    if (get_u32(module->data, M_CONNECTED) != 0 && get_u32(module->data, M_LOCAL_PLAYER) != 0)
        dialog_snapshot(((dialog_entry_fn)send_internal(module, 244))());
    dialog_snapshot(((dialog_entry_fn)send_internal(module, 210))());
    dialog_snapshot(((dialog_entry_fn)send_internal(module, 210))());
    if (number % 37 == 0) {
        /* Recover the vtable before capacity/name stores, even with no live
         * interface after teardown. A reordered store must differ on fault. */
        put_u32(module->data, 0x988, 0); put_u32(module->data, 0x9e8, 0x87654321);
        put_u32(module->data, 0x9f0, 0);
        dialog_snapshot(((dialog_entry_fn)send_internal(module, 31))());
    }
finish:
    s_dialog_active = 0; s_dialog_host_access = 0; s_lobby_reserved_word = NULL;
    s_dialogs.state_hash[62] = dialog_hash(s_lobby_session, 80);
    for (index = 0; index < s_dialogs.blocks; index++)
        for (offset = 0; offset + 4 <= s_dialogs.sizes[index]; offset += 4)
            put_u32(s_dialogs.heap[index], offset, dialog_word(get_u32(s_dialogs.heap[index], offset)));
    memcpy(results, &s_dialogs, sizeof(*results)); dialog_queues_finish();
}
static uint32_t test_lobby(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct dialog_results s_retail, s_ours;
    uint32_t number, bad = 0, faults = 0, creates = 0, releases = 0, padded = 0, switches = 0;
    void *handler = AddVectoredExceptionHandler(1, dialog_fault);
    if (handler == NULL) exit(2);
    for (number = 0; number < scenarios; number++) {
        multip_test_context("lobby", "retail", number); run_lobby(retail, number, &s_retail);
        multip_test_context("lobby", "ours", number); run_lobby(ours, number, &s_ours);
        {
            uint32_t index;
            for (index = 0; index < s_retail.calls; index++) {
                uint32_t tag = s_retail.log[index][0];
                creates += tag == 43; releases += tag == 42;
                padded += tag == 52 && s_retail.log[index][3] == 0x89abcdef;
                switches += tag == 38 && s_retail.log[index][1] == 0x110a;
            }
        }
        faults += s_retail.returns[63] != 0;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t index, shown = 0;
                printf("lobby scenario %lu differs\n", (unsigned long)number);
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
    if (scenarios >= 1200 && (creates == 0 || releases == 0 || padded == 0 || switches == 0)) {
        fprintf(stderr, "lobby fixture missed a required startup boundary\n"); exit(2);
    }
    printf("lobby coverage: creates %lu, releases %lu, completion stack words %lu, public configuration %lu\n",
        (unsigned long)creates, (unsigned long)releases, (unsigned long)padded, (unsigned long)switches);
    printf("lobby outcomes: %lu fault outcomes\n", (unsigned long)faults);
    printf("MULTIP lobby connection owners: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
