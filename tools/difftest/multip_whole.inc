/* Mixed public lifecycle with synchronous transport scheduling after every
 * action. Specialized suites retain exhaustive protocol/field boundaries. */
struct whole_results {
    struct dialog_results dialogs;
    uint32_t sent_count, sent_size[128], sent[128][256], public_mask, actions;
    uint32_t states, max_players;
};
static struct whole_results s_whole;
static uint32_t s_whole_receive_left, s_whole_send_calls;
static int32_t STDCALL whole_add(uint32_t code, void *callback)
{
    uint32_t id = dialog_code_id(callback), *entry = dialog_log(45);
    entry[1] = code; entry[2] = id;
    if (dialog_fail()) return 0x12345678;
    if (code == 0x14 && (id == 268 || id == 269)) {
        s_init_callbacks[id - 268] = callback;
        if (id == 269) { s_init_starting = 1; s_init_start_alloc = 0; }
    }
    if (code == 0x13 && s_lobby_reserved_word != NULL) *s_lobby_reserved_word = 0x89abcdef;
    return 0;
}
static void STDCALL whole_range(const void *a, const void *b, int32_t flags,
    int32_t *distance, int16_t *bearing, uint8_t *scale)
{
    uint32_t *entry = dialog_log(88);
    entry[1] = dialog_address(a); entry[2] = dialog_address(b); entry[3] = flags;
    *distance = s_dialog_number * 0x12345; *bearing = 123; *scale = 4;
}
static int32_t STDCALL whole_send(void *object, uint32_t from, uint32_t to, uint32_t flags, const void *packet, uint32_t size)
{
    uint32_t index = s_whole.sent_count++, offset, *entry = dialog_log(63);
    entry[1] = object == &s_dialog_object; entry[2] = from; entry[3] = to;
    entry[4] = flags; entry[5] = size;
    if (index >= 128 || size > sizeof(s_whole.sent[index])) { fprintf(stderr, "whole sent budget exhausted: %lu packets, %lu bytes, scenario %lu\n", (unsigned long)index, (unsigned long)size, (unsigned long)s_dialog_number); exit(2); }
    s_whole.sent_size[index] = size;
    for (offset = 0; offset + 4 <= size; offset += 4)
        s_whole.sent[index][offset / 4] = dialog_word(get_u32(packet, offset));
    for (; offset < size; offset++) ((uint8_t *)s_whole.sent[index])[offset] = ((const uint8_t *)packet)[offset];
    if (++s_whole_send_calls % 3 == 1) return (int32_t)0x8877010eu;
    return s_dialog_number & 1 ? (int32_t)0x80004005u : 0;
}
static int32_t STDCALL whole_receive(void *object, uint32_t *from, uint32_t *to, uint32_t flags, void *packet, uint32_t *size)
{
    uint32_t *entry = dialog_log(89);
    entry[1] = object == &s_dialog_object; entry[2] = flags; entry[3] = *size;
    entry[4] = packet != NULL; entry[5] = s_whole_receive_left;
    if (s_whole_receive_left == 0) return (int32_t)0x887700beu;
    if (packet == NULL || *size < 64) { *size = 64; return (int32_t)0x8877001eu; }
    memset(packet, 0, 64);
    put_u32(packet, 0, s_whole_receive_left == 2 ? 0x7f000001 : 0x1000);
    put_u32(packet, 0xc, 0x70000000u + s_dialog_number); put_u32(packet, 0x10, 0x6c);
    *from = 0x4500; *to = 0; *size = 64; s_whole_receive_left--;
    return 0;
}
static void whole_settle(void)
{
    uint32_t received, attempts = 0;
    const struct multip_module *module = s_dialog_module;
    if (get_u32(module->data, 0x988) != 0) {
        while (s_whole_receive_left != 0 && get_u32(module->data, 0xe04) != 0) {
            int32_t result = ((multip_receive_fn)send_internal(module, 3))(&received);
            dialog_log(90)[1] = result;
            if (result != 0 || received == 0) break;
            if (++attempts > 8) { fprintf(stderr, "whole receive did not drain\n"); exit(2); }
        }
        if (get_u32(module->data, 0xdf8) != 0)
            dialog_log(91)[1] = lobby_call_painted(send_internal(module, 220), 2, (void *)0x1e, NULL, 0);
        if (get_u32(module->data, M_LOCAL_PLAYER) != 0)
            dialog_log(92)[1] = ((dialog_entry_fn)send_internal(module, 2))();
    }
    dialog_log(93)[1] = s_whole_receive_left;
}
static void whole_step(int32_t result)
{
    uint32_t count;
    whole_settle(); dialog_snapshot(result); s_whole.actions++;
    s_whole.states |= get_u32(s_dialog_module->data, M_CONNECTED) == 0 ? 1 :
        get_u32(s_dialog_module->data, 0xaa8) != 0 ? 2 : 4;
    count = get_u32(s_dialog_module->data, M_PLAYER_COUNT);
    if (count > s_whole.max_players) s_whole.max_players = count;
}
static void whole_queries(void)
{
    uint32_t slot, local = get_u32(s_dialog_module->data, M_LOCAL_PLAYER);
    uint32_t id = local != 0 ? get_u32((void *)(uintptr_t)local, 4) : 0xdead;
    put_u32(s_dialog_module->data, M_ACTIVE_PLAYER_ID, id);
    put_u32(s_dialog_module->data, M_AIRCRAFT_ACTIVE, 1);
    put_u32(s_dialog_module->data, M_PLAYER_MODE, 1 + s_dialog_number % 2);
    for (slot = 0x24; slot <= 0x38; slot += 4) {
        uint32_t *entry = dialog_log(94), result;
        int32_t output = 0x13572468;
        entry[1] = slot;
        if (slot == 0x28) result = dialog_address(((multip_player_fn)get_slot(s_dialog_module, slot))(id));
        else if (slot >= 0x34) result = ((multip_output_fn)get_slot(s_dialog_module, slot))(&output);
        else {
            result = ((multip_status_fn)get_slot(s_dialog_module, slot))();
            if (slot != 0x30) result = dialog_address((void *)(uintptr_t)result);
        }
        entry[2] = result; entry[3] = output;
        s_whole.public_mask |= 1u << (slot / 4);
    }
}
static void run_whole(const struct multip_module *module, uint32_t number, struct whole_results *results)
{
    uint32_t index, offset, hosting;
    uint8_t payload[32];
    init_setup(module, number); memset(&s_whole, 0, sizeof(s_whole));
    s_whole_receive_left = 0; s_whole_send_calls = 0;
    put_u32(s_lobby_chain, 0x18, (uint32_t)(uintptr_t)whole_add);
    put_u32(s_dialog_util, 0x64, (uint32_t)(uintptr_t)whole_range);
    s_dialog_vtable[0x64 / 4] = whole_receive; s_dialog_vtable[0x68 / 4] = whole_send;
    put_u32(module->data, 0xa48, 0); put_u32(module->data, 0xa40, 0);
    put_u32(module->data, 0xdf8, 0); put_u32(module->data, 0xe60, 0); put_u32(module->data, 0xe64, 0);
    put_u32(module->data, 0x988, 0);
    hosting = s_lobby_connection[1] & 2;
    if (__builtin_setjmp(s_dialog_jump) != 0) {
        s_dialogs.returns[63] = s_dialog_outcome; dialog_snapshot(-1); goto finish;
    }
    s_dialog_active = 1;
    s_whole.public_mask |= 1u << 1;
    whole_step(((dialog_entry_fn)get_slot(module, 4))());
    if (s_init_callbacks[0] != NULL)
        whole_step(lobby_call_painted(s_init_callbacks[0], 2, (void *)0x36, NULL, hosting ? 0x450 : 0x44c));
    whole_queries(); whole_step(0);
    if (get_u32(module->data, M_CONNECTED) != 0 && get_u32(module->data, M_LOCAL_PLAYER) != 0) {
        uint32_t desired = number % 9;
        typedef int32_t (STDCALL *whole_peer_fn)(const void *, uint32_t);
        for (index = get_u32(module->data, M_PLAYER_COUNT); index < desired; index++)
            ((whole_peer_fn)send_internal(module, 75))(s_lobby_name, 0x9000 + index);
        /* Duplicate IDs use the actual add owner; observer/unready states
         * model peer-supplied type/readiness fields on the retained records. */
        if (get_u32(module->data, M_PLAYER_COUNT) != 0) {
            uint8_t *peers = (uint8_t *)(uintptr_t)get_u32(module->data, M_PLAYERS);
            ((whole_peer_fn)send_internal(module, 75))(s_lobby_name, get_u32(peers, 4));
            for (index = 0; index < get_u32(module->data, M_PLAYER_COUNT); index++) {
                uint8_t *peer = peers + index * 256;
                if (peer != (void *)(uintptr_t)get_u32(module->data, M_LOCAL_PLAYER)) {
                    put_u32(peer, 8, number & 1 ? 2 : 1); put_u32(peer, 0xc, number & 2);
                    put_u32(peer, 0x2c, 0x6c);
                }
            }
        }
        whole_step(0);
        for (index = 0; index < sizeof(payload); index++) payload[index] = number + index;
        for (index = 0; index < 3; index++) {
            s_whole.public_mask |= 1u << 8;
            whole_step(((multip_send_fn)get_slot(module, 0x20))(0x9000, payload, index * 16, index));
        }
        s_whole_receive_left = 2; whole_step(0);
    }
    s_whole.public_mask |= 1u << 7;
    whole_step(((dialog_entry_fn)get_slot(module, 0x1c))());
    whole_queries(); whole_step(0);
    if (number & 64 && s_init_callbacks[0] != NULL)
        whole_step(lobby_call_painted(s_init_callbacks[0], 2, (void *)0x36, NULL, hosting ? 0x450 : 0x44c));
    s_whole.public_mask |= 1u << 6;
    whole_step(lobby_call_painted(get_slot(module, 0x18), 0, NULL, NULL, 0x410));
    whole_queries(); whole_step(0);
    if (s_init_callbacks[1] != NULL) {
        s_whole.states |= 8;
        whole_step(lobby_call_painted(s_init_callbacks[1], 2, (void *)0x37, NULL, 0));
    }
    s_whole.public_mask |= 1u << 2;
    whole_step(((dialog_entry_fn)get_slot(module, 8))());
    whole_step(((dialog_entry_fn)get_slot(module, 8))());
finish:
    s_dialog_active = 0; s_dialog_host_access = 0; s_lobby_reserved_word = NULL;
    for (index = 0; index < s_dialogs.blocks; index++)
        for (offset = 0; offset + 4 <= s_dialogs.sizes[index]; offset += 4)
            put_u32(s_dialogs.heap[index], offset, dialog_word(get_u32(s_dialogs.heap[index], offset)));
    memcpy(&s_whole.dialogs, &s_dialogs, sizeof(s_dialogs));
    memcpy(results, &s_whole, sizeof(*results)); dialog_queues_finish();
}
static uint32_t test_whole(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct whole_results s_retail, s_ours;
    uint32_t number, bad = 0, mask = 0, actions = 0, sent = 0, received = 0, faults = 0, complete = 0;
    uint32_t states = 0, max_players = 0;
    void *handler = AddVectoredExceptionHandler(1, dialog_fault);
    if (handler == NULL) exit(2);
    for (number = 0; number < scenarios; number++) {
        uint32_t index;
        multip_test_context("whole", "retail", number); run_whole(retail, number, &s_retail);
        multip_test_context("whole", "ours", number); run_whole(ours, number, &s_ours);
        mask |= s_retail.public_mask; actions += s_retail.actions; sent += s_retail.sent_count;
        states |= s_retail.states;
        if (s_retail.max_players > max_players) max_players = s_retail.max_players;
        faults += s_retail.dialogs.returns[63] != 0;
        complete += s_retail.public_mask == 0x7fc6;
        for (index = 0; index < s_retail.dialogs.calls; index++)
            received += s_retail.dialogs.log[index][0] == 89 && s_retail.dialogs.log[index][4] != 0;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t shown = 0;
                printf("whole scenario %lu differs\n", (unsigned long)number);
                for (index = 0; index < sizeof(s_ours) / 4 && shown < 12; index++)
                    if (((uint32_t *)&s_retail)[index] != ((uint32_t *)&s_ours)[index]) {
                        printf("  word %lu: %#lx / %#lx\n", (unsigned long)index,
                            (unsigned long)((uint32_t *)&s_retail)[index], (unsigned long)((uint32_t *)&s_ours)[index]); shown++;
                    }
            }
            bad++;
        }
    }
    RemoveVectoredExceptionHandler(handler);
    if (scenarios >= 1200 && (mask != 0x7fc6 || states != 15 || max_players < 8 || sent == 0 || received == 0 || complete == 0)) {
        fprintf(stderr, "whole fixture missed public or transport coverage: mask %#lx\n", (unsigned long)mask); exit(2);
    }
    printf("whole coverage: public mask %#lx, %lu complete public traces, %lu actions, %lu sends, %lu receives, %lu fault outcomes\n",
        (unsigned long)mask, (unsigned long)complete, (unsigned long)actions, (unsigned long)sent, (unsigned long)received, (unsigned long)faults);
    printf("whole session states: mask %#lx, maximum %lu players\n", (unsigned long)states, (unsigned long)max_players);
    printf("MULTIP mixed lifecycle: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
