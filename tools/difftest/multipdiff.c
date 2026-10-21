/* Differential test for MULTIP's recovered public queries and transport units.
 *
 * The retail and OpenFS98 DLLs are loaded together. Their private runtime
 * fields and imported GLOBAL/UTIL pointers are seeded identically, then slots
 * +0x24..+0x38 are compared after normalising pointers to record indexes.
 * multip_send.inc and multip_receive.inc add deterministic transport scenarios.
 * This is intentionally slice coverage, not a whole-module claim.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *s_fault_suite = "load";
static const char *s_fault_side = "unknown";
static uint32_t s_fault_scenario;

static void multip_test_context(const char *suite, const char *side, uint32_t scenario)
{
    s_fault_suite = suite;
    s_fault_side = side;
    s_fault_scenario = scenario;
}

static LONG WINAPI multip_test_fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "MULTIP fault %#lx at %p: %s scenario %lu (%s)\n",
        (unsigned long)exception->ExceptionRecord->ExceptionCode,
        exception->ExceptionRecord->ExceptionAddress, s_fault_suite,
        (unsigned long)s_fault_scenario, s_fault_side);
    fflush(stderr);
    ExitProcess(2);
    return EXCEPTION_EXECUTE_HANDLER;
}

#define STDCALL __attribute__((stdcall))

#define MULTIP_LINKAGE          0x0058
#define MULTIP_PLAYER_SIZE      0x0100
#define MULTIP_MAX_PLAYERS      8
#define MULTIP_FIND_CASES       8
#define M_PLAYER_COUNT          0x0660
#define M_PLAYERS               0x0678
#define M_LOCAL_PLAYER          0x0a3c
#define M_DIRECTPLAY_LOADED     0x0a48
#define M_PLAYER_MODE           0x0a5c
#define M_ACTIVE_PLAYER_ID      0x0a60
#define M_CONNECTED             0x0aac
#define M_AIRCRAFT_ACTIVE       0x0ab4
#define M_PLAYERS_RESULT        0x0ef8
#define IMPORT_GLOBAL           0x000c
#define IMPORT_UTIL             0x0044

typedef uint32_t *(STDCALL *multip_players_fn)(void);
typedef uint8_t *(STDCALL *multip_player_fn)(uint32_t player_id);
typedef uint8_t *(STDCALL *multip_local_fn)(void);
typedef uint32_t (STDCALL *multip_status_fn)(void);
typedef uint32_t (STDCALL *multip_output_fn)(int32_t *result);

struct multip_module {
    HMODULE module_handle;
    uint8_t *data;
    uint8_t *linkage;
    uint8_t initial_data[0x1000];
};

struct multip_scenario {
    uint32_t count;
    uint8_t player_records[MULTIP_MAX_PLAYERS][MULTIP_PLAYER_SIZE];
    uint8_t local_player_record[MULTIP_PLAYER_SIZE];
    uint8_t remote_has_aircraft[MULTIP_MAX_PLAYERS];
    uint8_t local_has_aircraft;
    uint8_t local_present;
    uint32_t directplay_loaded;
    uint32_t connected;
    uint32_t aircraft_active;
    uint32_t player_mode;
    uint32_t active_player_id;
    int32_t collision_initial;
    int32_t range_initial;
    uint32_t util_distance;
    uint32_t find_ids[MULTIP_FIND_CASES];
};

struct multip_results {
    uint32_t returned_count;
    uint32_t returned_players;
    uint32_t returned_block;
    uint32_t find[MULTIP_FIND_CASES];
    uint32_t get_local;
    uint32_t status;
    uint32_t collision_return;
    int32_t collision_result;
    uint32_t range_return;
    int32_t range_result;
    uint32_t range_calls;
    uint32_t range_args_ok;
};

struct util_trace {
    const void *expected_a;
    const void *expected_b;
    uint32_t distance;
    uint32_t calls;
    uint32_t args_ok;
};

static struct util_trace s_util_trace;
static uint8_t s_util_linkage[0x68];

static void put_u32(uint8_t *data, uint32_t offset, uint32_t value)
{
    memcpy(data + offset, &value, sizeof(value));
}

static uint32_t get_u32(const uint8_t *data, uint32_t offset)
{
    uint32_t value;

    memcpy(&value, data + offset, sizeof(value));
    return value;
}

static uint32_t random_u32(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void *get_slot(const struct multip_module *module, uint32_t offset)
{
    void *function;

    memcpy(&function, module->linkage + offset, sizeof(function));
    return function;
}

static uint32_t normalise_player(const uint8_t *player,
                                 const uint8_t *local_player,
                                 const uint8_t *players, uint32_t count)
{
    uintptr_t player_address;
    uintptr_t players_address;

    if (player == NULL)
        return 0;
    if (player == local_player)
        return 1;
    player_address = (uintptr_t)player;
    players_address = (uintptr_t)players;
    if (player_address >= players_address &&
        player_address < players_address + count * MULTIP_PLAYER_SIZE)
        return 2 + (uint32_t)(player_address - players_address) /
                   MULTIP_PLAYER_SIZE;
    return 0xffffffffu;
}

static uint32_t unused_player_id(const struct multip_scenario *scenario)
{
    uint32_t id = 0xf0000000u;
    uint32_t index;

    for (;;) {
        int used = scenario->local_present &&
                   get_u32(scenario->local_player_record, 0x04) == id;
        for (index = 0; index < scenario->count; index++)
            used |= get_u32(scenario->player_records[index], 0x04) == id;
        if (!used)
            return id;
        id++;
    }
}

static void make_scenario(struct multip_scenario *scenario, uint32_t number)
{
    uint32_t state = 0x9e3779b9u ^ (number * 0x85ebca6bu);
    uint32_t index;
    uint32_t byte_index;

    memset(scenario, 0, sizeof(*scenario));
    scenario->count = random_u32(&state) % (MULTIP_MAX_PLAYERS + 1);
    scenario->local_present = (uint8_t)(random_u32(&state) & 1);
    for (byte_index = 0; byte_index < MULTIP_PLAYER_SIZE; byte_index++)
        scenario->local_player_record[byte_index] = (uint8_t)random_u32(&state);
    put_u32(scenario->local_player_record, 0x04, random_u32(&state));
    scenario->local_has_aircraft = (uint8_t)(random_u32(&state) & 1);

    for (index = 0; index < MULTIP_MAX_PLAYERS; index++) {
        for (byte_index = 0; byte_index < MULTIP_PLAYER_SIZE; byte_index++)
            scenario->player_records[index][byte_index] =
                (uint8_t)random_u32(&state);
        put_u32(scenario->player_records[index], 0x04, random_u32(&state));
        put_u32(scenario->player_records[index], 0x08,
                random_u32(&state) % 4);
        put_u32(scenario->player_records[index], 0x0c,
                random_u32(&state) & 1);
        scenario->remote_has_aircraft[index] =
            (uint8_t)(random_u32(&state) & 1);
    }

    if (scenario->count >= 2 && number % 4 == 0)
        put_u32(scenario->player_records[scenario->count - 1], 0x04,
                get_u32(scenario->player_records[0], 0x04));

    scenario->directplay_loaded = random_u32(&state) & 1;
    scenario->connected = random_u32(&state) & 1;
    scenario->aircraft_active = random_u32(&state) & 1;
    scenario->player_mode = random_u32(&state) % 4;
    scenario->collision_initial = (int32_t)random_u32(&state);
    scenario->range_initial = (int32_t)random_u32(&state);
    scenario->util_distance = random_u32(&state);

    scenario->find_ids[0] = get_u32(scenario->local_player_record, 0x04);
    scenario->find_ids[1] = scenario->count != 0 ?
        get_u32(scenario->player_records[0], 0x04) : random_u32(&state);
    scenario->find_ids[2] = scenario->count != 0 ?
        get_u32(scenario->player_records[scenario->count - 1], 0x04) :
        random_u32(&state);
    scenario->find_ids[3] = unused_player_id(scenario);
    for (index = 4; index < MULTIP_FIND_CASES; index++)
        scenario->find_ids[index] = random_u32(&state);

    switch (number % 5) {
    case 0:
        scenario->active_player_id = scenario->find_ids[0];
        break;
    case 1:
        scenario->active_player_id = scenario->find_ids[2];
        break;
    case 2:
        scenario->active_player_id = scenario->find_ids[3];
        break;
    default:
        scenario->active_player_id = random_u32(&state);
        break;
    }

    /* Force regular successful paths, including modes other than exactly 1. */
    if (number % 16 == 0) {
        scenario->connected = 1;
        scenario->aircraft_active = 1;
        scenario->player_mode = (number & 16) ? 2 : 1;
        if (scenario->count != 0) {
            index = number % scenario->count;
            scenario->active_player_id =
                get_u32(scenario->player_records[index], 0x04);
            scenario->remote_has_aircraft[index] = 1;
            put_u32(scenario->player_records[index], 0x28, 1);
        } else {
            scenario->local_present = 1;
            scenario->active_player_id =
                get_u32(scenario->local_player_record, 0x04);
            scenario->local_has_aircraft = 1;
            put_u32(scenario->local_player_record, 0x28, 1);
        }
    }
}

static uint8_t *expected_active_player(const struct multip_scenario *scenario,
                                       uint8_t *local_player,
                                       uint8_t players[][MULTIP_PLAYER_SIZE])
{
    uint32_t index;

    if (scenario->local_present &&
        get_u32(local_player, 0x04) == scenario->active_player_id)
        return local_player;
    index = scenario->count;
    while (index != 0) {
        index--;
        if (get_u32(players[index], 0x04) == scenario->active_player_id)
            return players[index];
    }
    return NULL;
}

static void STDCALL fake_range48(const void *a, const void *b, int32_t flags,
                                 int32_t *distance, int16_t *bearing,
                                 uint8_t *scale)
{
    s_util_trace.calls++;
    if (a == s_util_trace.expected_a && b == s_util_trace.expected_b &&
        flags == 3 && distance != NULL && bearing != NULL && scale != NULL)
        s_util_trace.args_ok++;
    *distance = (int32_t)s_util_trace.distance;
    *bearing = (int16_t)0x6a5a;
    *scale = 0xc3;
}

static int load_module(struct multip_module *module, const char *path)
{
    uint32_t id;

    module->module_handle = LoadLibraryA(path);
    if (module->module_handle == NULL) {
        printf("cannot load %s (%lu)\n", path, GetLastError());
        return 0;
    }
    module->data = (uint8_t *)GetProcAddress(module->module_handle, "ImportTable");
    module->linkage = (uint8_t *)GetProcAddress(module->module_handle, "Linkage");
    if (module->data == NULL || module->linkage == NULL) {
        printf("%s: exports missing\n", path);
        return 0;
    }
    memcpy(&id, module->linkage, sizeof(id));
    if (id != 35 || module->linkage != module->data + MULTIP_LINKAGE) {
        printf("%s: invalid MULTIP image layout\n", path);
        return 0;
    }
    memcpy(module->initial_data, module->data, sizeof(module->initial_data));
    return 1;
}

static void run_module(const struct multip_module *module,
                       const struct multip_scenario *scenario,
                       struct multip_results *results)
{
    uint8_t player_records[MULTIP_MAX_PLAYERS][MULTIP_PLAYER_SIZE];
    uint8_t local_player_record[MULTIP_PLAYER_SIZE];
    uint8_t aircraft_states[MULTIP_MAX_PLAYERS + 1][0x20];
    uint8_t global_data[0x580];
    uint8_t *active_player;
    uint32_t *players_result;
    uint32_t index;
    multip_players_fn get_players;
    multip_player_fn get_player;
    multip_local_fn get_local_player;
    multip_status_fn get_status;
    multip_output_fn override_collision;
    multip_output_fn get_active_range;

    memcpy(player_records, scenario->player_records, sizeof(player_records));
    memcpy(local_player_record, scenario->local_player_record,
           sizeof(local_player_record));
    memset(aircraft_states, 0x5a, sizeof(aircraft_states));
    memset(global_data, 0xa5, sizeof(global_data));
    memset(results, 0, sizeof(*results));
    memset(module->data + 0x600, 0, 0xa00);

    for (index = 0; index < scenario->count; index++)
        put_u32(player_records[index], 0x28,
                scenario->remote_has_aircraft[index] ?
                (uint32_t)(uintptr_t)aircraft_states[index] : 0);
    put_u32(local_player_record, 0x28, scenario->local_has_aircraft ?
            (uint32_t)(uintptr_t)aircraft_states[MULTIP_MAX_PLAYERS] : 0);

    put_u32(module->data, M_PLAYER_COUNT, scenario->count);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)player_records);
    put_u32(module->data, M_LOCAL_PLAYER, scenario->local_present ?
            (uint32_t)(uintptr_t)local_player_record : 0);
    put_u32(module->data, M_DIRECTPLAY_LOADED, scenario->directplay_loaded);
    put_u32(module->data, M_CONNECTED, scenario->connected);
    put_u32(module->data, M_AIRCRAFT_ACTIVE, scenario->aircraft_active);
    put_u32(module->data, M_PLAYER_MODE, scenario->player_mode);
    put_u32(module->data, M_ACTIVE_PLAYER_ID, scenario->active_player_id);
    put_u32(module->data, IMPORT_GLOBAL, (uint32_t)(uintptr_t)global_data);
    put_u32(module->data, IMPORT_UTIL, (uint32_t)(uintptr_t)s_util_linkage);

    get_players = (multip_players_fn)get_slot(module, 0x24);
    get_player = (multip_player_fn)get_slot(module, 0x28);
    get_local_player = (multip_local_fn)get_slot(module, 0x2c);
    get_status = (multip_status_fn)get_slot(module, 0x30);
    override_collision = (multip_output_fn)get_slot(module, 0x34);
    get_active_range = (multip_output_fn)get_slot(module, 0x38);

    players_result = get_players();
    results->returned_count = players_result[0];
    results->returned_players =
        players_result[1] == (uint32_t)(uintptr_t)player_records;
    results->returned_block =
        players_result == (uint32_t *)(void *)(module->data + M_PLAYERS_RESULT);
    for (index = 0; index < MULTIP_FIND_CASES; index++)
        results->find[index] = normalise_player(
            get_player(scenario->find_ids[index]),
            scenario->local_present ? local_player_record : NULL,
            player_records[0], scenario->count);
    results->get_local = normalise_player(
        get_local_player(), scenario->local_present ? local_player_record : NULL,
        player_records[0], scenario->count);
    results->status = get_status();

    results->collision_result = scenario->collision_initial;
    results->collision_return =
        override_collision(&results->collision_result);

    active_player = expected_active_player(scenario, local_player_record,
                                            player_records);
    memset(&s_util_trace, 0, sizeof(s_util_trace));
    s_util_trace.expected_a = global_data + 0x560;
    s_util_trace.expected_b = active_player != NULL &&
        get_u32(active_player, 0x28) != 0 ?
        (uint8_t *)(uintptr_t)get_u32(active_player, 0x28) + 4 : NULL;
    s_util_trace.distance = scenario->util_distance;
    results->range_result = scenario->range_initial;
    results->range_return = get_active_range(&results->range_result);
    results->range_calls = s_util_trace.calls;
    results->range_args_ok = s_util_trace.args_ok;

    /* Check the returned block's contents independently of its pointer. */
    if (get_u32(module->data, M_PLAYERS_RESULT) != scenario->count ||
        get_u32(module->data, M_PLAYERS_RESULT + 4) !=
            (uint32_t)(uintptr_t)player_records)
        results->returned_block = 0;
}

static void print_difference(uint32_t scenario_number,
                             const struct multip_results *retail,
                             const struct multip_results *ours)
{
    const uint32_t *retail_words =
        (const uint32_t *)(const void *)retail;
    const uint32_t *our_words = (const uint32_t *)(const void *)ours;
    uint32_t index;

    printf("scenario %lu differs\n", (unsigned long)scenario_number);
    for (index = 0; index < sizeof(*retail) / sizeof(uint32_t); index++)
        if (retail_words[index] != our_words[index])
            printf("  result %lu: retail %#lx, ours %#lx\n",
                   (unsigned long)index, (unsigned long)retail_words[index],
                   (unsigned long)our_words[index]);
}

#include "multip_send.inc"
#include "multip_receive.inc"
#include "multip_workers.inc"
#include "multip_directplay.inc"
#include "multip_profile.inc"
#include "multip_session.inc"
#include "multip_aircraft.inc"
#include "multip_tracking.inc"
#include "multip_messages.inc"
#include "multip_players.inc"
#include "multip_local.inc"
#include "multip_frame.inc"
#include "multip_draw.inc"
#include "multip_motion.inc"
#include "multip_commands.inc"
#include "multip_weather.inc"
#include "multip_tick.inc"
#include "multip_system.inc"
#include "multip_app_players.inc"
#include "multip_updates.inc"
#include "multip_textures.inc"
#include "multip_texture_receive.inc"
#include "multip_transfer_messages.inc"
#include "multip_texture_send.inc"
#include "multip_model_send.inc"
#include "multip_model_cache.inc"
#include "multip_model_receive.inc"
#include "multip_state_messages.inc"
#include "multip_control_updates.inc"

int main(int argc, char **argv)
{
    struct multip_module retail;
    struct multip_module ours;
    struct multip_scenario scenario;
    struct multip_results retail_results;
    struct multip_results our_results;
    uint32_t scenarios = 20000;
    uint32_t number;
    uint32_t bad = 0;
    uint32_t zero_count = 0;
    uint32_t duplicate_ids = 0;
    uint32_t collision_overrides = 0;
    uint32_t range_calls = 0;
    void *range_function = fake_range48;

    SetUnhandledExceptionFilter(multip_test_fault);

    if (argc > 1)
        scenarios = (uint32_t)strtoul(argv[1], NULL, 0);
    if (scenarios == 0)
        scenarios = 1;
    memset(s_util_linkage, 0, sizeof(s_util_linkage));
    memcpy(s_util_linkage + 0x64, &range_function, sizeof(range_function));

    if (!load_module(&retail, "MULTIP_retail.dll") ||
        !load_module(&ours, "MULTIP_ours.dll"))
        return 2;

    /* Local creation owns its fixture setup, so mutations can run it alone. */
    if (argc > 2) {
        if (strcmp(argv[2], "local") == 0)
            return test_local(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "frame") == 0)
            return test_frame(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "control_updates") == 0)
            return test_control_updates(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "state_messages") == 0)
            return test_state_messages(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "title_receive") == 0)
            return test_title_receive(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "model_receive") == 0)
            return test_model_receive(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "model_cache") == 0)
            return test_model_cache(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "model_send") == 0)
            return test_model_send(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "texture_send") == 0)
            return test_texture_send(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "transfer_messages") == 0)
            return test_transfer_messages(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "texture_receive") == 0)
            return test_texture_receive(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "textures") == 0)
            return test_textures(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "updates") == 0)
            return test_updates(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "app_players") == 0)
            return test_app_players(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "system") == 0)
            return test_system(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "tick") == 0)
            return test_tick(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "weather") == 0)
            return test_weather(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "commands") == 0)
            return test_commands(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "motion") == 0)
            return test_motion(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "draw") == 0)
            return test_draw(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "players") == 0)
            return test_players(&retail, &ours, scenarios) != 0;
        if (strcmp(argv[2], "messages") == 0)
            return test_messages(&retail, &ours, scenarios) != 0;
        fprintf(stderr, "unknown MULTIP suite: %s\n", argv[2]);
        return 2;
    }

    for (number = 0; number < scenarios; number++) {
        make_scenario(&scenario, number);
        multip_test_context("queries", "retail", number);
        run_module(&retail, &scenario, &retail_results);
        multip_test_context("queries", "ours", number);
        run_module(&ours, &scenario, &our_results);
        if (memcmp(&retail_results, &our_results, sizeof(retail_results)) != 0) {
            if (bad < 8)
                print_difference(number, &retail_results, &our_results);
            bad++;
        }
        zero_count += scenario.count == 0;
        duplicate_ids += scenario.count >= 2 &&
            get_u32(scenario.player_records[0], 0x04) ==
            get_u32(scenario.player_records[scenario.count - 1], 0x04);
        collision_overrides += retail_results.collision_result == -1 &&
            scenario.collision_initial != -1;
        range_calls += retail_results.range_calls != 0;
    }

    printf("coverage: zero-count %lu, duplicate-ID %lu, collision override %lu, "
           "range call %lu\n",
           (unsigned long)zero_count, (unsigned long)duplicate_ids,
           (unsigned long)collision_overrides, (unsigned long)range_calls);
    printf("MULTIP +0x24..+0x38: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    multip_test_context("send", "unknown", 0);
    bad += test_send(&retail, &ours, scenarios, 0);
    multip_test_context("send", "unknown", 0);
    bad += test_send(&retail, &ours, scenarios, 1);
    multip_test_context("receive", "unknown", 0);
    bad += test_receive(&retail, &ours, scenarios);
    multip_test_context("directplay", "unknown", 0);
    bad += test_directplay(&retail, &ours, scenarios);
    multip_test_context("profile", "unknown", 0);
    bad += test_profile(&retail, &ours, scenarios);
    multip_test_context("session", "unknown", 0);
    bad += test_session(&retail, &ours, scenarios);
    multip_test_context("metadata", "unknown", 0);
    bad += test_metadata(&retail, &ours, scenarios);
    multip_test_context("aircraft", "unknown", 0);
    bad += test_aircraft(&retail, &ours, scenarios);
    multip_test_context("aircraft_copy", "unknown", 0);
    bad += test_aircraft_copy(&retail, &ours, scenarios);
    multip_test_context("tracking", "unknown", 0);
    bad += test_tracking(&retail, &ours, scenarios);
    multip_test_context("messages", "unknown", 0);
    bad += test_messages(&retail, &ours, scenarios);
    multip_test_context("players", "unknown", 0);
    bad += test_players(&retail, &ours, scenarios);
    multip_test_context("local", "unknown", 0);
    bad += test_local(&retail, &ours, scenarios);
    multip_test_context("frame", "unknown", 0);
    bad += test_frame(&retail, &ours, scenarios);
    multip_test_context("draw", "unknown", 0);
    bad += test_draw(&retail, &ours, scenarios);
    bad += test_motion(&retail, &ours, scenarios);
    bad += test_commands(&retail, &ours, scenarios);
    bad += test_weather(&retail, &ours, scenarios);
    bad += test_tick(&retail, &ours, scenarios);
    bad += test_system(&retail, &ours, scenarios);
    bad += test_app_players(&retail, &ours, scenarios);
    bad += test_updates(&retail, &ours, scenarios);
    bad += test_textures(&retail, &ours, scenarios);
    bad += test_texture_receive(&retail, &ours, scenarios);
    bad += test_transfer_messages(&retail, &ours, scenarios);
    bad += test_texture_send(&retail, &ours, scenarios);
    bad += test_model_send(&retail, &ours, scenarios);
    bad += test_model_cache(&retail, &ours, scenarios);
    bad += test_model_receive(&retail, &ours, scenarios);
    bad += test_title_receive(&retail, &ours, scenarios);
    bad += test_state_messages(&retail, &ours, scenarios);
    bad += test_control_updates(&retail, &ours, scenarios);
    multip_test_context("workers", "unknown", 0);
    bad += test_workers(&retail, &ours, scenarios);
    return bad != 0;
}
