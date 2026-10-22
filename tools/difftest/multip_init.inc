/* Registration plus real CHAIN startup, compound-connection and shutdown
 * owners. Win32 events/locks are scripted; no OS workers or providers run. */
static uint32_t s_init_events;
static uint32_t s_init_starting, s_init_start_alloc;
static uint8_t s_init_payload[0x314], s_init_event[16];
static void *s_init_callbacks[2];

static int32_t init_status(void)
{
    static const uint32_t s_results[] = {0, 1, 0xffffffffu, 0x81234567u};
    return dialog_fail() ? 0x12345678 : s_results[s_dialog_number / 8 % 4];
}
static int32_t STDCALL init_register(void *linkage, uint32_t *token)
{
    uint32_t *entry = dialog_log(70);
    entry[1] = dialog_address(linkage); entry[2] = dialog_address(token);
    *token = 0x76543210 + s_dialog_number;
    return init_status();
}
static int32_t STDCALL init_unregister(uint32_t token)
{
    dialog_log(71)[1] = token;
    return init_status();
}
static void *STDCALL init_application(void)
{
    dialog_log(72); return s_dialog_application;
}
static int32_t STDCALL init_add(uint32_t code, void *callback)
{
    uint32_t id = dialog_code_id(callback), *entry = dialog_log(45);
    entry[1] = code; entry[2] = id;
    if (code == 0x14 && (id == 268 || id == 269)) {
        int32_t result = init_status();
        s_init_callbacks[id - 268] = callback;
        if (id == 269) { s_init_starting = result == 0; s_init_start_alloc = 0; }
        return result;
    }
    if (code == 0x13 && s_lobby_reserved_word != NULL) *s_lobby_reserved_word = 0x89abcdef;
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL init_load_exports(void *table)
{
    int32_t result = dialog_export_loader(table);
    put_u32(s_dialog_module->data, 0x980, (uint32_t)(uintptr_t)lobby_factory);
    return result;
}
static int32_t STDCALL init_alloc(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    uint32_t fail_call = s_dialog_fail_call, call = s_dialogs.calls;
    int32_t result;
    if (s_init_starting && ++s_init_start_alloc == 1 + s_dialog_number / 256 % 3 && (s_dialog_number & 128))
        s_dialog_fail_call = call + 1;
    result = dialog_alloc(flags, size, out, zero) != 0 ? 0x12345678 : 0;
    s_dialog_fail_call = fail_call; s_dialogs.log[call][5] = result;
    return result;
}
static int32_t STDCALL init_copy(void *out, const void *source, uint32_t size)
{
    return dialog_copy(out, source, size) != 0 ? 0x23456789 : 0;
}
static int32_t STDCALL init_free(void **out)
{
    return dialog_free(out) != 0 ? 0x3456789a : 0;
}
static int32_t STDCALL init_read_int(const char *section, const char *key, uint32_t *out)
{
    uint32_t *entry = dialog_log(73);
    entry[1] = dialog_text_hash(section); entry[2] = dialog_text_hash(key); entry[3] = dialog_address(out);
    *out = s_profile_values[(profile_hash(key) + s_dialog_number / 32) % 32];
    /* Keep completion's player capacities bounded, independently of other
     * profile values. Their own profile suite tests the extreme capacities. */
    if (out == (uint32_t *)(void *)(s_dialog_module->data + 0x998)) *out = 8;
    if (out == (uint32_t *)(void *)(s_dialog_module->data + 0x99c)) *out = 2;
    entry[4] = *out;
    return dialog_fail() ? -1 : 0;
}
static uint32_t STDCALL init_read_string(const char *section, const char *key, char *out, uint32_t size)
{
    uint32_t *entry = dialog_log(74);
    entry[1] = dialog_text_hash(section); entry[2] = dialog_text_hash(key);
    entry[3] = dialog_address(out); entry[4] = size;
    snprintf(out, size, "init-%lu-%s", (unsigned long)(s_dialog_number / 32), key);
    return dialog_fail() ? 0 : strlen(out);
}
static int32_t STDCALL init_write_int(const char *section, const char *key, uint32_t value)
{
    uint32_t *entry = dialog_log(75);
    entry[1] = dialog_text_hash(section); entry[2] = dialog_text_hash(key); entry[3] = value;
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL init_write_string(const char *section, const char *key, const char *value)
{
    uint32_t *entry = dialog_log(76);
    entry[1] = dialog_text_hash(section); entry[2] = dialog_text_hash(key); entry[3] = dialog_text_hash(value);
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL init_resource(uint32_t id, char *out, uint32_t size)
{
    uint32_t *entry = dialog_log(77);
    entry[1] = id; entry[2] = dialog_address(out); entry[3] = size;
    snprintf(out, size, "%s", id == 0xf60a ? "fixture-command-never-executed" : "texture");
    return init_status();
}
static int32_t STDCALL init_current_path(char *out)
{
    dialog_log(83); strcpy(out, "C:\\fixture");
    return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL init_split_path(const char *path, char *drive, char *directory, char *base, int32_t *kind)
{
    uint32_t *entry = dialog_log(84);
    entry[1] = dialog_text_hash(path); entry[2] = drive != NULL; entry[3] = directory != NULL; entry[4] = kind != NULL;
    strcpy(base, "init"); return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL init_make_path(char *out, const char *current, const char *directory, const char *base)
{
    uint32_t *entry = dialog_log(85);
    entry[1] = dialog_text_hash(current); entry[2] = dialog_text_hash(directory); entry[3] = dialog_text_hash(base);
    sprintf(out, "%s\\%s\\%s", current, directory, base); return dialog_fail() ? -1 : 0;
}
static int32_t STDCALL init_strcat(char *out, const char *text)
{
    dialog_log(86)[1] = dialog_text_hash(text); strcat(out, text); return init_status();
}
static int32_t STDCALL init_unlink(const char *path, uint32_t flags)
{
    uint32_t *entry = dialog_log(87);
    entry[1] = dialog_text_hash(path); entry[2] = flags;
    return dialog_fail() || s_dialog_number / 64 % 3 == 1 ? -1 : 0;
}
static HANDLE WINAPI init_event(LPSECURITY_ATTRIBUTES attributes, BOOL manual, BOOL initial, LPCSTR name)
{
    uint32_t *entry = dialog_log(78);
    entry[1] = attributes != NULL; entry[2] = manual; entry[3] = initial; entry[4] = name != NULL;
    s_init_events++;
    return dialog_fail() ? NULL : (HANDLE)(uintptr_t)(0x3300 + s_init_events);
}
static void WINAPI init_lock_create(LPCRITICAL_SECTION lock)
{
    dialog_log(79)[1] = dialog_address(lock); memset(lock, 0, sizeof(*lock));
}
static void WINAPI init_lock_enter(LPCRITICAL_SECTION lock) { dialog_log(80)[1] = dialog_address(lock); }
static void WINAPI init_lock_leave(LPCRITICAL_SECTION lock) { dialog_log(81)[1] = dialog_address(lock); }
static void WINAPI init_lock_delete(LPCRITICAL_SECTION lock) { dialog_log(82)[1] = dialog_address(lock); }

static void init_setup(const struct multip_module *module, uint32_t number)
{
    lobby_setup(module, number);
    s_init_events = 0; s_init_starting = 0; s_init_start_alloc = 0;
    memset(s_init_callbacks, 0, sizeof(s_init_callbacks));
#define INIT_SLOT(table, offset, function) put_u32(table, offset, (uint32_t)(uintptr_t)function)
    INIT_SLOT(s_dialog_host, 0x298, init_register); INIT_SLOT(s_dialog_host, 0x29c, init_unregister);
    INIT_SLOT(s_dialog_host, 0x58, init_application); INIT_SLOT(s_dialog_host, 0x2a0, init_resource);
    INIT_SLOT(s_dialog_host, 0xa8, init_read_int); INIT_SLOT(s_dialog_host, 0xc8, init_read_string);
    INIT_SLOT(s_dialog_host, 0xac, init_write_int); INIT_SLOT(s_dialog_host, 0xcc, init_write_string);
    INIT_SLOT(s_dialog_host, 0x124, init_load_exports); INIT_SLOT(s_lobby_chain, 0x18, init_add);
    INIT_SLOT(s_dialog_host, 0x20c, init_alloc); INIT_SLOT(s_dialog_host, 0x21c, init_copy);
    INIT_SLOT(s_dialog_host, 0x214, init_free);
    INIT_SLOT(s_dialog_host, 0x130, init_current_path); INIT_SLOT(s_dialog_host, 0x188, init_split_path);
    INIT_SLOT(s_dialog_host, 0x184, init_make_path); INIT_SLOT(s_dialog_host, 0x2b0, init_strcat);
    INIT_SLOT(s_dialog_host, 0x14c, init_unlink);
#undef INIT_SLOT
    dialog_patch(module, "CreateEventA", init_event);
    dialog_patch(module, "InitializeCriticalSection", init_lock_create);
    dialog_patch(module, "EnterCriticalSection", init_lock_enter);
    dialog_patch(module, "LeaveCriticalSection", init_lock_leave);
    dialog_patch(module, "DeleteCriticalSection", init_lock_delete);
    put_u32(module->data, 0xa48, number / 4 % 2);
    put_u32(module->data, 0x9a0, 0x91234567); put_u32(module->data, 0x9a4, 0x82345678);
    put_u32(module->data, 0x984, 0x73456789); put_u32(module->data, 0xa40, 0x6456789a);
    memset(s_init_payload, 0, sizeof(s_init_payload)); memset(s_init_event, 0, sizeof(s_init_event));
    put_u32(s_init_event, 8, (uint32_t)(uintptr_t)s_init_payload);
    put_u32(s_init_payload, 0x110, number / 32 % 7 == 0 ? 0 : 0x200);
    {
        uint8_t *connection = s_init_payload + 0x114;
        put_u32(connection, 0, 0x24); put_u32(connection, 4, number / 2 % 4);
        put_u32(connection, 8, 0x40); put_u32(connection, 12, 0x90); put_u32(connection, 0x20, 0x100);
        memcpy(connection + 0x40, s_lobby_session, 80);
        put_u32(connection + 0x40, 0x30, 0xb0); put_u32(connection + 0x40, 0x34, 0x12345678);
        put_u32(connection + 0x90, 0, 16); put_u32(connection + 0x90, 8, 0xd0); put_u32(connection + 0x90, 12, 0xe0);
        strcpy((char *)connection + 0xb0, "compound session");
        strcpy((char *)connection + 0xd0, "compound short"); strcpy((char *)connection + 0xe0, "compound long");
        if (number % 8 == 7) {
            /* Fault after different relocation stores, with a retained block. */
            uint32_t malformed = number / 64 % 3;
            put_u32(connection, malformed == 0 ? 8 : 12, malformed == 2 ? 0 : 0x40000000);
        }
    }
}

static void run_init(const struct multip_module *module, uint32_t number, struct dialog_results *results)
{
    uint32_t kind = number % 8, index, offset;
    init_setup(module, number);
    if (__builtin_setjmp(s_dialog_jump) != 0) {
        s_dialogs.returns[63] = s_dialog_outcome; dialog_snapshot(-1); goto finish;
    }
    s_dialog_active = 1;
    put_u32(module->data, 0xe60, 0); put_u32(module->data, 0xe64, 0); put_u32(module->data, 0xdf8, 0);
    dialog_snapshot(((dialog_entry_fn)send_internal(module, 18))());
    if (number / 32 % 2) {
        void *name;
        if (dialog_alloc(1, 15, &name, 1) == 0) {
            strcpy(name, "init.bmp"); put_u32(module->data, 0xc0, 1); put_u32(module->data, 0xd8, (uint32_t)(uintptr_t)name);
        }
    }
    dialog_snapshot(((dialog_entry_fn)get_slot(module, 4))());
    if (kind == 0) {
        dialog_snapshot(((dialog_entry_fn)get_slot(module, 4))());
    } else if (kind == 1 || kind == 4) {
        uint32_t hosting = s_lobby_connection[1] & 2;
        dialog_snapshot(lobby_call_painted(s_init_callbacks[0] != NULL ? s_init_callbacks[0] : send_internal(module, 268), 2, (void *)0x36, NULL, hosting ? 0x450 : 0x44c));
        if (kind == 1 && number / 32 % 2)
            dialog_snapshot(lobby_call_painted(s_init_callbacks[0] != NULL ? s_init_callbacks[0] : send_internal(module, 268), 2, (void *)0x36, NULL, hosting ? 0x450 : 0x44c));
    } else if (kind == 5) {
        /* Painter leaves EAX=0x800; the shutdown callback must preserve it. */
        dialog_snapshot(lobby_call_painted(s_init_callbacks[0] != NULL ? s_init_callbacks[0] : send_internal(module, 268), 2, (void *)0x12345678, NULL, 0));
        dialog_snapshot(lobby_call_painted(send_internal(module, 269), 2, (void *)0x36, NULL, 0));
    }
    if (kind == 2 || kind == 3 || kind == 4 || kind == 7) {
        put_u32(module->data, 0xa48, kind == 2 ? 0 : 1);
        /* Init clears export addresses; a loaded instance retains its lobby
         * entry in the real host before receiving this event. */
        put_u32(module->data, 0x980, (uint32_t)(uintptr_t)lobby_factory);
        dialog_snapshot(lobby_call_painted(s_init_callbacks[0] != NULL ? s_init_callbacks[0] : send_internal(module, 268), 2, (void *)0x3a,
            number / 256 % 5 == 4 ? NULL : s_init_event, number / 2 % 4 & 2 ? 0x44c : 0x448));
    }
    if (kind == 1 || kind == 4 || kind == 6) {
        void *callback = s_init_callbacks[1] != NULL ? s_init_callbacks[1] : send_internal(module, 269);
        dialog_snapshot(lobby_call_painted(callback, 2, (void *)0x37, NULL, 0));
        dialog_snapshot(lobby_call_painted(callback, 2, (void *)0x37, NULL, 0));
    }
    dialog_snapshot(((dialog_entry_fn)get_slot(module, 8))());
    dialog_snapshot(((dialog_entry_fn)get_slot(module, 8))());
finish:
    s_dialog_active = 0; s_dialog_host_access = 0; s_lobby_reserved_word = NULL;
    for (index = 0; index < s_dialogs.blocks; index++)
        for (offset = 0; offset + 4 <= s_dialogs.sizes[index]; offset += 4)
            put_u32(s_dialogs.heap[index], offset, dialog_word(get_u32(s_dialogs.heap[index], offset)));
    memcpy(results, &s_dialogs, sizeof(*results)); dialog_queues_finish();
}

static uint32_t test_init(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct dialog_results s_retail, s_ours;
    uint32_t number, bad = 0, faults = 0, startups = 0, shutdowns = 0, padded = 0, copies = 0, queue_failures = 0;
    void *handler = AddVectoredExceptionHandler(1, dialog_fault);
    if (handler == NULL) exit(2);
    for (number = 0; number < scenarios; number++) {
        uint32_t index;
        multip_test_context("init", "retail", number); run_init(retail, number, &s_retail);
        multip_test_context("init", "ours", number); run_init(ours, number, &s_ours);
        for (index = 0; index < s_retail.calls; index++) {
            uint32_t *entry = s_retail.log[index];
            startups += entry[0] == 45 && entry[2] == 269;
            shutdowns += entry[0] == 75;
            copies += entry[0] == 3 && entry[2] == 0x200;
            padded += entry[0] == 52 && entry[3] == 0x89abcdef;
            queue_failures += entry[0] == 1 && entry[2] == 36 && entry[5] != 0;
        }
        faults += s_retail.returns[63] != 0;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t shown = 0;
                printf("init scenario %lu differs\n", (unsigned long)number);
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
    if (scenarios >= 1200 && (startups == 0 || shutdowns == 0 || copies == 0 || padded == 0 || faults == 0 || queue_failures == 0)) {
        fprintf(stderr, "init fixture missed a required lifecycle boundary\n"); exit(2);
    }
    printf("init coverage: startups %lu, profile writes %lu, compound copies %lu, completion stack words %lu, queue failures %lu, fault outcomes %lu\n",
        (unsigned long)startups, (unsigned long)shutdowns, (unsigned long)copies, (unsigned long)padded, (unsigned long)queue_failures, (unsigned long)faults);
    printf("MULTIP init/deinit owners: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
