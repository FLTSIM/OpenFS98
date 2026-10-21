/* Renderer stand-ins record arguments and mutate published state, exposing
 * reverse traversal, gating, and reloads between model and label draws. */
struct draw_results {
    struct player_fixture players;
    uint32_t returns[7], state[7][7];
};
static uint32_t s_draw_tick_calls;
static uint32_t draw_hash(const void *buffer, uint32_t size)
{
    const uint8_t *data = buffer;
    uint32_t index, hash = 2166136261u;
    for (index = 0; index < size; index++) hash = (hash ^ data[index]) * 16777619u;
    return hash;
}
static uint32_t STDCALL draw_ticks(void)
{
    player_log(31);
    if (s_players.number & 0x100) put_u32(s_players_module->data, M_PLAYER_COUNT, 0);
    s_draw_tick_calls++;
    return ++s_players.ticks;
}
static void draw_side_effects(void)
{
    if (s_players.number & 0x40)
        put_u32(s_players_module->data, 0x188, get_u32(s_players_module->data, 0x188) == 0);
    if (s_players.number & 0x80) put_u32(s_players_module->data, 0x184, 0);
}
static int32_t STDCALL draw_aircraft(void *aircraft)
{
    uint8_t *player = (uint8_t *)aircraft - 16;
    uint32_t index = (player - (uint8_t *)s_players.banks[0]) / 256;
    uint32_t *entry = player_log(32);
    entry[1] = player_address(aircraft); entry[2] = get_u32(player, 4);
    entry[3] = draw_hash((const void *)(uintptr_t)get_u32(aircraft, 4), 36);
    entry[4] = get_u32((const void *)(uintptr_t)get_u32(aircraft, 8), 4);
    if (s_players.number & 0x20)
        put_u32(player, 0x28, (uint32_t)(uintptr_t)s_players.models[index + 6]);
    draw_side_effects();
    return -1;
}
static uint32_t STDCALL draw_model(const void *header, void *state)
{
    uintptr_t header_address = (uintptr_t)header, headers_address = (uintptr_t)s_players.heap[6];
    uint32_t *entry = player_log(33);
    entry[1] = player_address(header); entry[2] = player_address(state);
    entry[3] = draw_hash(header, 16); entry[4] = draw_hash(state, 36);
    if (header_address >= headers_address && header_address < (uintptr_t)s_players.heap[12]) draw_side_effects();
    return 0x80004005u;
}
static void STDCALL draw_add_vector(void *dest_buffer, const void *left, const void *right)
{
    uint32_t *entry = player_log(34), index, low, high;
    uint8_t *dest = dest_buffer;
    const uint8_t *a = left, *b = right;
    entry[1] = player_address(dest); entry[2] = player_address(a); entry[3] = player_address(b);
    entry[4] = draw_hash(a, 24); entry[5] = draw_hash(b, 24);
    for (index = 0; index < 24; index += 8) {
        low = *(const uint16_t *)(const void *)(a + index + 2) +
                (uint32_t)*(const uint16_t *)(const void *)(b + index + 2);
        high = get_u32(a, index + 4) + get_u32(b, index + 4) + (low >> 16);
        dest[index + 2] = (uint8_t)low; dest[index + 3] = (uint8_t)(low >> 8);
        put_u32(dest, index + 4, high);
    }
}
static int32_t draw_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t *entry = player_log(tag);
    entry[1] = code; entry[2] = (void *)callback == send_internal(s_players_module, 89);
    entry[3] = get_u32(s_players_module->data, 0x958);
    return (int32_t)s_frame_chain_result;
}
static int32_t STDCALL draw_add(uint32_t code, track_callback_fn callback)
{ return draw_registration(35, code, callback); }
static int32_t STDCALL draw_remove(uint32_t code, track_callback_fn callback)
{ return draw_registration(36, code, callback); }
static void draw_snapshot(struct draw_results *results, uint32_t step, uint32_t return_value)
{
    static const uint32_t s_fields[] = {0x958,0x95c,0x960,0x964,0x184,0x188,0x660};
    uint32_t index;
    results->returns[step] = return_value;
    for (index = 0; index < 7; index++)
        results->state[step][index] = get_u32(s_players_module->data, s_fields[index]);
}
static void run_draw(const struct multip_module *module, uint32_t number, struct draw_results *results)
{
    uint8_t host[0x320] = {0};
    void *aircraft_linkage[16] = {0}, *g3d[16] = {0}, *util[32] = {0}, *chain[9] = {0};
    uint32_t index, byte, mode, random_state = number ^ 0x75621983u;
    uint8_t *player;
    void *function = draw_ticks;
    track_callback_fn callback = (track_callback_fn)send_internal(module, 89);

    memset(results, 0, sizeof(*results)); memset(&s_players, 0, sizeof(s_players));
    s_players.number = number; s_players_module = module; s_draw_tick_calls = 0;
    s_players.ticks = number & 1 ? 0xffffffffu : random_u32(&random_state);
    memcpy(module->data, module->initial_data, 0x1000);
    memcpy(host + 0x38, &function, 4);
    aircraft_linkage[0x18 / 4] = (void *)draw_aircraft;
    g3d[0x38 / 4] = (void *)draw_model; util[0x74 / 4] = (void *)draw_add_vector;
    chain[0x18 / 4] = (void *)draw_add; chain[0x1c / 4] = (void *)draw_remove;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft_linkage);
    put_u32(module->data, 0x2c, (uint32_t)(uintptr_t)g3d);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0x984, (number / 7) % 4 == 0);
    put_u32(module->data, 0x184, number % 3 != 0);
    put_u32(module->data, 0x188, (number / 3) % 3 != 0);
    put_u32(module->data, 0x95c, (number / 28) % 3);
    put_u32(module->data, 0x960, random_u32(&random_state));
    put_u32(module->data, 0x964, random_u32(&random_state));
    put_u32(module->data, M_PLAYER_COUNT, number % 7);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_players.banks[0]);
    for (index = 0; index < 12; index++)
        for (byte = 0; byte < 128; byte++) s_players.models[index][byte] = random_u32(&random_state);
    for (index = 0; index < 6; index++) {
        player = s_players.banks[0][index];
        for (byte = 0; byte < 256; byte++) player[byte] = random_u32(&random_state);
        mode = (number / 3 + index) % 8;
        put_u32(player, 4, 0x6000 + index);
        put_u32(player, 8, mode & 2 ? 2 : 1);
        put_u32(player, 0xc, mode & 1);
        put_u32(player, 0x4c, mode & 4 ? 0 : 1);
        put_u32(player, 0x10, (uint32_t)(uintptr_t)s_players.headers[index]);
        put_u32(player, 0x14, (uint32_t)(uintptr_t)s_players.models[index]);
        put_u32(player, 0x18, (uint32_t)(uintptr_t)s_players.headers[index + 6]);
        put_u32(player, 0x1c, (number + index) / 4 % 2);
        put_u32(player, 0x20, (uint32_t)(uintptr_t)s_players.heap[index + 6]);
        put_u32(player, 0x28, (number / 5 + index) % 5 == 0 ? 0 :
            (uint32_t)(uintptr_t)s_players.models[index]);
        put_u32(player, 0xd0, (uint32_t)(uintptr_t)s_players.heap[index]);
        put_u32(player, 0xd4, (uint32_t)(uintptr_t)"peer label");
        memset(s_players.heap[index], 0x40 + index, 42);
        memset(s_players.headers[index], 0x20 + index, 36);
        put_u32(s_players.headers[index], 4, (uint32_t)(uintptr_t)s_players.heap[index + 6]);
        put_u32(s_players.headers[index + 6], 4, 1);
        memset(s_players.heap[index + 6], 0x60 + index, 42);
    }
    s_frame_chain_result = number % 3 == 0 ? 1 : 0;
    draw_snapshot(results, 0, ((multip_worker_fn)send_internal(module, 90))());
    draw_snapshot(results, 1, ((multip_worker_fn)send_internal(module, 88))());
    put_u32(module->data, 0x984, 0);
    draw_snapshot(results, 2, callback(0x1234, 0xffffffffu));
    draw_snapshot(results, 3, ((multip_worker_fn)send_internal(module, 91))());
    s_frame_chain_result = 0;
    draw_snapshot(results, 4, ((multip_worker_fn)send_internal(module, 91))());
    draw_snapshot(results, 5, ((multip_worker_fn)send_internal(module, 91))());
    draw_snapshot(results, 6, ((multip_worker_fn)send_internal(module, 90))());
    memcpy(&results->players, &s_players, sizeof(s_players));
}
static uint32_t test_draw(const struct multip_module *retail, const struct multip_module *ours,
                          uint32_t scenarios)
{
    static struct draw_results s_retail, s_ours;
    uint32_t number, bad = 0, owned_draws = 0, direct_draws = 0, label_draws = 0, call;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("draw", "retail", number); run_draw(retail, number, &s_retail);
        multip_test_context("draw", "ours", number); run_draw(ours, number, &s_ours);
        for (call = 0; call < s_retail.players.call_count; call++) {
            const uint32_t *entry = s_retail.players.log[call];
            if (entry[0] == 32) owned_draws++;
            if (entry[0] == 33) {
                if (entry[1] >= (uint32_t)(uintptr_t)s_players.heap[6]) direct_draws++;
                else label_draws++;
            }
        }
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("player draw scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf(" first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    if (scenarios >= 1200 && (owned_draws == 0 || direct_draws == 0 || label_draws == 0)) {
        fprintf(stderr, "draw fixture missed a renderer branch: %lu/%lu/%lu\n", (unsigned long)owned_draws,
            (unsigned long)direct_draws, (unsigned long)label_draws); exit(2);
    }
    printf("draw coverage: ACONTAIN %lu, direct %lu, labels %lu\n", (unsigned long)owned_draws,
        (unsigned long)direct_draws, (unsigned long)label_draws);
    printf("MULTIP player drawing/CHAIN 0x10: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
