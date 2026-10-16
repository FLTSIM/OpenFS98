/* Differential test: retail LANDME.DLL against the OpenFS98 build.
 *
 * Each side gets a separately loaded copy of retail FE and both copies read
 * the same installed scenery fixture.  Retail UTIL supplies the six arithmetic
 * services LANDME uses.  MAIN, WINMSG, CHAIN and host PostMessage are logging
 * stubs.  Every scenario starts from pristine FE and LANDME images, starts the
 * landing assistant, and dispatches a script of frames while moving the
 * aircraft onto and around LANDME's own published target.
 *
 * Compared after each scenario: every imported call and message, GLOBAL, the
 * host arena, FE's complete state image and LANDME's complete state image.
 * Module-local pointers compare by image offset; relocated code pointers are
 * ignored explicitly.
 *
 * The FE host model and fixture plumbing are shared with fediff by including
 * that harness under a renamed main entry point.
 */
#define main fediff_reference_main
#include "fediff.c"
#undef main

#define LANDME_IMAGE_SIZE 0x854u
#define LANDME_IMPORTS    0x7f8u
#define LANDME_LINKAGE    0x838u
#define LANDME_FRAMES     24u
#define GLOBAL16(offset)  (*(uint16_t *)(global + (offset)))
#define GLOBAL32(offset)  (*(uint32_t *)(global + (offset)))

struct landme_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved0c;
    uint32_t reserved10;
    void *dialogs;
    void (*start)(void);
};

struct landme_side {
    const char *name;
    HMODULE module;
    struct fs98_import *imports;
    struct landme_linkage *link;
    uint8_t *image;
    uint8_t *module_lo;
    uint8_t *module_hi;
    uint8_t pristine[LANDME_IMAGE_SIZE];
};

struct landme_scenario {
    struct fe_scenario fe;
    uint32_t active;
    uint32_t abort_frame;
    uint32_t stop_frame;
    uint32_t steer_mode;
    uint32_t groundspeed;
    uint32_t saved_278;
    uint16_t wind_speed;
    uint16_t wind_direction;
    uint16_t heading_error;
    uint16_t turn_scale;
};

static struct landme_side landme_retail;
static struct landme_side landme_ours;
static struct landme_side *landme_current;
static uint8_t main_stub[0x80];
static uint8_t winmsg_stub[0x20];
static void (STDCALL *fe_frame_callback)(uint32_t, uint32_t);
static void (STDCALL *landme_frame_callback)(uint32_t, uint32_t);
static uint32_t state_coverage[7];
static uint32_t phase_coverage[12];
static uint32_t runway_found;
static uint32_t runway_missing;
static uint32_t messages_shown;
static uint32_t commands_posted;

static uint32_t landme_mix(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static int landme_in_module(const struct landme_side *side, const void *pointer)
{
    const uint8_t *value = (const uint8_t *)pointer;
    return value >= side->module_lo && value < side->module_hi;
}

static int landme_in_image(const struct landme_side *side, uint32_t pointer)
{
    const uint8_t *value = (const uint8_t *)(uintptr_t)pointer;
    return value >= side->image && value < side->image + LANDME_IMAGE_SIZE;
}

static void landme_put_pointer(const void *pointer)
{
    const uint8_t *value = (const uint8_t *)pointer;

    if (landme_current != NULL &&
        value >= landme_current->image &&
        value < landme_current->image + LANDME_IMAGE_SIZE) {
        fe_put("landme+%#x", (uint32_t)(value - landme_current->image));
    } else {
        fe_put_pointer(fe_ptr(pointer));
    }
}

static int32_t STDCALL landme_host_post(uint32_t command, uint32_t parameter)
{
    fe_put(" post(%#x,%#x)", command, parameter);
    if (counting)
        commands_posted++;
    return 0;
}

static int32_t STDCALL landme_host_loadstring(uint32_t id, char *out,
                                               int32_t size)
{
    char text[64];
    uint32_t length;

    if (id < 0x1f4 || id > 0x214)
        return host_loadstring(id, out, size);
    snprintf(text, sizeof(text), "LANDME message %03x", id);
    length = (uint32_t)strlen(text);
    while (length < 30)
        text[length++] = ' ';
    text[length] = 0;
    if (size > 0) {
        strncpy(out, text, (size_t)size - 1);
        out[size - 1] = 0;
    }
    fe_put(" string(%#x)='%.40s'", id, text);
    return (int32_t)length;
}

static int32_t STDCALL landme_chain_add(
    uint32_t list, void (STDCALL *callback)(uint32_t, uint32_t))
{
    fe_put(" chain.add(%u,", list);
    if (landme_current != NULL &&
        landme_in_module(landme_current, (const void *)callback)) {
        fe_put("landme.frame");
        if (list == 3)
            landme_frame_callback = callback;
    } else if (current != NULL &&
               (uint8_t *)callback >= current->module_lo &&
               (uint8_t *)callback < current->module_hi) {
        fe_put("fe.frame");
        if (list == 3)
            fe_frame_callback = callback;
    } else {
        landme_put_pointer((const void *)callback);
    }
    fe_put(")");
    return 0;
}

static int32_t STDCALL landme_chain_remove(
    uint32_t list, void (STDCALL *callback)(uint32_t, uint32_t))
{
    fe_put(" chain.remove(%u,", list);
    if (landme_current != NULL &&
        landme_in_module(landme_current, (const void *)callback)) {
        fe_put("landme.frame");
        landme_frame_callback = NULL;
    } else if (current != NULL &&
               (uint8_t *)callback >= current->module_lo &&
               (uint8_t *)callback < current->module_hi) {
        fe_put("fe.frame");
        fe_frame_callback = NULL;
    } else {
        landme_put_pointer((const void *)callback);
    }
    fe_put(")");
    return 0;
}

static void STDCALL landme_main_land(void *target, void *position,
                                      void *guidance, void *result)
{
    (void)result;
    fe_put(" main.land(");
    landme_put_pointer(target);
    fe_put(",");
    fe_put_pointer(fe_ptr(position));
    fe_put(",");
    fe_put_pointer(fe_ptr(guidance));
    fe_put(")");
}

static void STDCALL landme_winmsg_show(const char *message, uint32_t slot,
                                        uint32_t duration, uint32_t mode)
{
    fe_put(" winmsg.show(");
    if (landme_current != NULL &&
        (const uint8_t *)message >= landme_current->image &&
        (const uint8_t *)message < landme_current->image + LANDME_IMAGE_SIZE)
        landme_put_pointer(message);
    else
        fe_put("temporary");
    fe_put(",%u,%u,%u)=", slot, duration, mode);
    fe_put_string(message);
    if (counting)
        messages_shown++;
}

static void landme_build_stubs(void)
{
    fe_build_stubs();
    memset(main_stub, 0, sizeof(main_stub));
    memset(winmsg_stub, 0, sizeof(winmsg_stub));
    *(uint32_t *)main_stub = 3;
    *(uint32_t *)winmsg_stub = 22;
    SET_SLOT(host, 0x03c, landme_host_post);
    SET_SLOT(host, 0x2a0, landme_host_loadstring);
    SET_SLOT(chain, 0x18, landme_chain_add);
    SET_SLOT(chain, 0x1c, landme_chain_remove);
    SET_SLOT(main_stub, 0x70, landme_main_land);
    SET_SLOT(winmsg_stub, 0x18, landme_winmsg_show);
}

static void landme_bind_imports(struct landme_side *side,
                                struct fe_side *fe_side, uint8_t *util)
{
    struct fs98_import *import;

    for (import = side->imports; import->id != 0; import++) {
        switch (import->id) {
        case 1:  import->link = host; break;
        case 2:  import->link = global; break;
        case 3:  import->link = main_stub; break;
        case 4:  import->link = chain; break;
        case 18: import->link = fe_side->link; break;
        case 20: import->link = util; break;
        case 22: import->link = winmsg_stub; break;
        default:
            printf("LANDME: unexpected import %u\n", import->id);
            exit(2);
        }
    }
}

static int landme_load_side(struct landme_side *side, const char *path,
                            const char *name, struct fe_side *fe_side,
                            uint8_t *util)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;

    memset(side, 0, sizeof(*side));
    side->name = name;
    side->module = LoadLibraryA(path);
    if (side->module == NULL) {
        printf("cannot load %s (%lu)\n", path, GetLastError());
        return 0;
    }
    side->imports = (struct fs98_import *)GetProcAddress(side->module,
                                                         "ImportTable");
    side->link = (struct landme_linkage *)GetProcAddress(side->module,
                                                         "Linkage");
    if (side->imports == NULL || side->link == NULL || side->link->id != 26) {
        printf("%s: not LANDME\n", path);
        return 0;
    }
    side->image = (uint8_t *)side->imports - LANDME_IMPORTS;
    if ((uint8_t *)side->link != side->image + LANDME_LINKAGE) {
        printf("%s: LANDME image layout differs\n", path);
        return 0;
    }
    dos = (IMAGE_DOS_HEADER *)side->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)side->module + dos->e_lfanew);
    side->module_lo = (uint8_t *)side->module;
    side->module_hi = side->module_lo + nt->OptionalHeader.SizeOfImage;
    landme_bind_imports(side, fe_side, util);
    memcpy(side->pristine, side->image, LANDME_IMAGE_SIZE);
    return 1;
}

static void landme_make_scenario(struct landme_scenario *scenario,
                                 uint32_t number)
{
    static const uint32_t speeds[] = {
        0, 0x0fff, 0x1000, 0x3200, 0x3201, 0x3c00, 0x3c01, 0x4600, 0x4601
    };

    fe_make_scenario(&scenario->fe, number);
    scenario->fe.operation = number % 6;
    scenario->active = number % 13 != 0;
    scenario->abort_frame = number % 17 == 0 ? 9 : 0xffffffffu;
    scenario->stop_frame = number % 19 == 0 ? 11 : 0xffffffffu;
    scenario->steer_mode = number % 7;
    scenario->groundspeed = speeds[number % (sizeof(speeds) / sizeof(speeds[0]))];
    scenario->saved_278 = landme_mix(number ^ 0x278278u);
    scenario->wind_speed = (uint16_t)(number % 4 == 0 ? 4 : 5 + number % 20);
    scenario->wind_direction = (uint16_t)landme_mix(number ^ 0xef2u);
    scenario->heading_error = (uint16_t)landme_mix(number ^ 0xe92u);
    scenario->turn_scale = (uint16_t)landme_mix(number ^ 0xe90u);
}

static void landme_seed_global(const struct landme_scenario *scenario)
{
    GLOBAL32(0x278) = scenario->saved_278;
    GLOBAL32(0x2bc) = scenario->groundspeed;
    GLOBAL32(0x84c) = scenario->active;
    GLOBAL32(0x874) = 0;
    GLOBAL16(0xe90) = scenario->turn_scale;
    GLOBAL16(0xe92) = scenario->heading_error;
    GLOBAL16(0xef0) = scenario->wind_speed;
    GLOBAL16(0xef2) = scenario->wind_direction;
    GLOBAL16(0x116e) = (uint16_t)(scenario->fe.number & 1);
    GLOBAL32(0x28c) = scenario->fe.number % 3 == 0;
    GLOBAL32(0x580) = landme_mix(scenario->fe.number ^ 0x580580u);
    GLOBAL32(0x020) = scenario->fe.altitude << 8;
}

static void landme_steer(const struct landme_scenario *scenario,
                         uint32_t frame_number)
{
    uint8_t *image = landme_current->image;
    uint32_t state = *(uint32_t *)(image + 0x088);
    uint32_t noise = landme_mix(scenario->fe.number * 37u + frame_number);
    int32_t delta = (int32_t)(noise % 1201) - 600;

    if (*(uint32_t *)(global + 0x854) != 0 && state >= 2) {
        *(uint16_t *)(global + 0x562) = *(uint16_t *)(global + 0x85a);
        *(uint32_t *)(global + 0x564) = *(uint32_t *)(global + 0x85c);
        *(uint16_t *)(global + 0x56a) = *(uint16_t *)(global + 0x862);
        *(uint32_t *)(global + 0x56c) = *(uint32_t *)(global + 0x864);
        *(uint16_t *)(global + 0x572) = *(uint16_t *)(global + 0x86a);
        *(uint32_t *)(global + 0x574) = *(uint32_t *)(global + 0x86c);
        *(uint32_t *)(global + 0x580) =
            (uint32_t)*(uint16_t *)(global + 0x870) << 16;
        if (scenario->steer_mode & 1)
            *(uint32_t *)(global + 0x564) += (uint32_t)delta;
        if (scenario->steer_mode & 2)
            *(uint32_t *)(global + 0x56c) -= (uint32_t)delta * 64u;
        if (scenario->steer_mode & 4)
            *(uint32_t *)(global + 0x574) += (uint32_t)(delta / 4);
    } else {
        *(uint32_t *)(global + 0x580) += noise << 13;
    }
    *(uint16_t *)(global + 0xe92) += (uint16_t)(noise >> 16);
    *(uint32_t *)(global + 0x2bc) =
        scenario->groundspeed + (frame_number % 3 == 0 ? 1 : 0);
    if (frame_number == scenario->abort_frame)
        *(uint32_t *)(global + 0x874) = 1;
    if (frame_number == scenario->stop_frame)
        *(uint32_t *)(global + 0x84c) = 0;
}

static void landme_count_entry(void)
{
    uint32_t state = *(uint32_t *)(landme_current->image + 0x088);

    if (state != 0 && state != 4) {
        if (*(uint32_t *)(global + 0x84c) == 0)
            state = 4;
        else if (*(uint32_t *)(global + 0x874) != 0)
            state = 6;
    }

    if (state < sizeof(state_coverage) / sizeof(state_coverage[0]))
        state_coverage[state]++;
}

static void landme_count_phase(void)
{
    uint32_t phase = *(uint32_t *)(landme_current->image + 0x06e) >> 4;

    if (phase < sizeof(phase_coverage) / sizeof(phase_coverage[0]))
        phase_coverage[phase]++;
}

static void landme_run_side(struct landme_side *side, struct fe_side *fe_side,
                            const struct landme_scenario *scenario,
                            uint8_t *landme_out, uint8_t *fe_out,
                            uint8_t *global_out, uint8_t *arena_out,
                            uint32_t *arena_used_out)
{
    uint8_t position[0x18];
    uint32_t frame_number;

    landme_current = side;
    fe_prepare_run(fe_side, &scenario->fe, position);
    landme_current = side;
    memcpy(side->image, side->pristine, LANDME_IMAGE_SIZE);
    fe_frame_callback = NULL;
    landme_frame_callback = NULL;
    landme_seed_global(scenario);
    fe_put("scenario=%u active=%u", scenario->fe.number, scenario->active);

    fe_side->link->init();
    side->link->init();
    side->link->start();
    if (scenario->fe.number % 11 == 0)
        side->link->start();

    for (frame_number = 0; frame_number < LANDME_FRAMES; frame_number++) {
        landme_steer(scenario, frame_number);
        fe_put(" F%u", frame_number);
        if (landme_frame_callback == NULL) {
            fe_put(" NO_LANDME_FRAME");
            break;
        }
        if (counting)
            landme_count_entry();
        landme_frame_callback(0x11223344u, 0x55667788u);
        if (counting)
            landme_count_phase();
    }
    if (counting) {
        if (*(uint16_t *)(side->image + 0x058) == 0xffff)
            runway_missing++;
        else
            runway_found++;
    }

    memcpy(landme_out, side->image, LANDME_IMAGE_SIZE);
    memcpy(fe_out, fe_side->image, FE_IMAGE_SIZE);
    memcpy(global_out, global, GLOBAL_SIZE);
    memcpy(arena_out, arena, arena_used);
    *arena_used_out = arena_used;

    side->link->deinit();
    fe_side->link->deinit();
    fe_reset_handles();
}

static int landme_skip_code_pointer(uint32_t offset)
{
    static const uint16_t pointers[] = {
        0x08c, 0x090, 0x094, 0x098, 0x09c, 0x0a0, 0x0a4,
        0x683, 0x687, 0x693, 0x697, 0x6a3, 0x6a7, 0x6b3, 0x6b7,
        0x6c3, 0x6c7, 0x6d3, 0x6d7, 0x6e3, 0x6e7, 0x6f3, 0x6f7,
        0x703, 0x707, 0x713, 0x717, 0x723, 0x727, 0x733, 0x737,
        0x83c, 0x840, 0x850
    };
    uint32_t index;

    for (index = 0; index < sizeof(pointers) / sizeof(pointers[0]); index++)
        if (offset >= pointers[index] &&
            offset < (uint32_t)pointers[index] + 4)
            return 1;
    return 0;
}

static int landme_same_pointer(uint32_t retail_value, uint32_t ours_value)
{
    if (landme_in_image(&landme_retail, retail_value) &&
        landme_in_image(&landme_ours, ours_value)) {
        return (uint8_t *)(uintptr_t)retail_value - landme_retail.image ==
               (uint8_t *)(uintptr_t)ours_value - landme_ours.image;
    }
    return fe_same_image_pointer(retail_value, ours_value);
}

static int32_t landme_compare_image(const uint8_t *retail_image,
                                    const uint8_t *ours_image)
{
    uint32_t offset = 0;

    while (offset < LANDME_IMAGE_SIZE) {
        if (landme_skip_code_pointer(offset)) {
            offset++;
            continue;
        }
        if (offset + 4 <= LANDME_IMAGE_SIZE &&
            !landme_skip_code_pointer(offset + 1) &&
            !landme_skip_code_pointer(offset + 2) &&
            !landme_skip_code_pointer(offset + 3) &&
            landme_same_pointer(*(const uint32_t *)(retail_image + offset),
                                *(const uint32_t *)(ours_image + offset))) {
            offset += 4;
            continue;
        }
        if (retail_image[offset] != ours_image[offset])
            return (int32_t)offset;
        offset++;
    }
    return -1;
}

static LONG WINAPI landme_unhandled(EXCEPTION_POINTERS *exception)
{
    printf("FAULT %#lx at %p in %s scenario %u step %u\n",
           (unsigned long)exception->ExceptionRecord->ExceptionCode,
           exception->ExceptionRecord->ExceptionAddress,
           landme_current != NULL ? landme_current->name : "setup",
           current_scenario, (uint32_t)service_step);
    fflush(stdout);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char **argv)
{
    static struct landme_scenario scenario_data;
    static uint8_t retail_landme[LANDME_IMAGE_SIZE];
    static uint8_t ours_landme[LANDME_IMAGE_SIZE];
    static uint8_t retail_fe[FE_IMAGE_SIZE];
    static uint8_t ours_fe[FE_IMAGE_SIZE];
    static uint8_t retail_global[GLOBAL_SIZE];
    static uint8_t ours_global[GLOBAL_SIZE];
    static char retail_transcript[TRANSCRIPT_SIZE];
    uint8_t *retail_arena;
    uint8_t *ours_arena;
    uint8_t *util;
    uint32_t scenarios;
    uint32_t scenario;
    uint32_t bad = 0;
    uint32_t retail_arena_used;
    uint32_t ours_arena_used;
    int32_t difference;

    SetUnhandledExceptionFilter(landme_unhandled);
    scenarios = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 100;
    if (argc < 3) {
        printf("usage: landmediff.exe [scenarios] fixture-root\n");
        return 2;
    }
    if (GetFullPathNameA(argv[2], sizeof(fixture), fixture, NULL) == 0 ||
        GetCurrentDirectoryA(sizeof(original_dir), original_dir) == 0) {
        printf("cannot resolve fixture root\n");
        return 2;
    }
    if (!fe_validate_fixture(fixture))
        return 2;

    retail_arena = (uint8_t *)malloc(ARENA_SIZE);
    ours_arena = (uint8_t *)malloc(ARENA_SIZE);
    if (retail_arena == NULL || ours_arena == NULL) {
        printf("cannot allocate comparison arenas\n");
        return 2;
    }

    landme_build_stubs();
    util = fe_load_support("UTIL_retail.dll", 20);
    if (!fe_load_side(&retail, "FE_landme_retail.dll", "retail-fe", util) ||
        !fe_load_side(&ours, "FE_landme_ours.dll", "ours-fe", util) ||
        !landme_load_side(&landme_retail, "LANDME_retail.dll", "retail",
                          &retail, util) ||
        !landme_load_side(&landme_ours, "LANDME_ours.dll", "openfs98",
                          &ours, util))
        return 2;
    if (!SetCurrentDirectoryA(fixture)) {
        printf("cannot enter fixture root\n");
        return 2;
    }

    difference = fe_compare_image(retail.pristine, ours.pristine);
    if (difference >= 0) {
        printf("initial FE images differ at +%#x\n", (uint32_t)difference);
        return 2;
    }
    difference = landme_compare_image(landme_retail.pristine,
                                      landme_ours.pristine);
    if (difference >= 0) {
        printf("initial LANDME images differ at +%#x\n", (uint32_t)difference);
        return 2;
    }

    for (scenario = 0; scenario < scenarios; scenario++) {
        landme_make_scenario(&scenario_data, scenario);
        counting = 1;
        landme_run_side(&landme_retail, &retail, &scenario_data,
                        retail_landme, retail_fe, retail_global,
                        retail_arena, &retail_arena_used);
        strncpy(retail_transcript, transcript, sizeof(retail_transcript) - 1);
        retail_transcript[sizeof(retail_transcript) - 1] = 0;

        counting = 0;
        landme_run_side(&landme_ours, &ours, &scenario_data,
                        ours_landme, ours_fe, ours_global,
                        ours_arena, &ours_arena_used);
        difference = landme_compare_image(retail_landme, ours_landme);
        if (strcmp(retail_transcript, transcript) != 0 ||
            difference >= 0 ||
            fe_compare_image(retail_fe, ours_fe) >= 0 ||
            memcmp(retail_global, ours_global, GLOBAL_SIZE) != 0 ||
            retail_arena_used != ours_arena_used ||
            (retail_arena_used != 0 &&
             memcmp(retail_arena, ours_arena, retail_arena_used) != 0)) {
            int32_t fe_difference = fe_compare_image(retail_fe, ours_fe);
            printf("scenario %u differs\n", scenario);
            if (strcmp(retail_transcript, transcript) != 0)
                fe_dump_transcript_difference(retail_transcript, transcript);
            if (difference >= 0)
                printf("  LANDME+%#x: retail %02x ours %02x\n",
                       (uint32_t)difference, retail_landme[difference],
                       ours_landme[difference]);
            if (fe_difference >= 0)
                printf("  FE+%#x: retail %02x ours %02x\n",
                       (uint32_t)fe_difference, retail_fe[fe_difference],
                       ours_fe[fe_difference]);
            if (memcmp(retail_global, ours_global, GLOBAL_SIZE) != 0)
                fe_dump_difference("GLOBAL", retail_global, ours_global,
                                   GLOBAL_SIZE);
            if (retail_arena_used != ours_arena_used)
                printf("  arena used: retail %#x ours %#x\n",
                       retail_arena_used, ours_arena_used);
            bad++;
            if (getenv("LANDMESTOP") != NULL)
                break;
        }
        if ((scenario + 1) % 25 == 0)
            printf("%u/%u\r", scenario + 1, scenarios);
    }

    printf("\nstates:");
    for (scenario = 0; scenario < 7; scenario++)
        printf(" %u", state_coverage[scenario]);
    printf("\nphases:");
    for (scenario = 1; scenario < 12; scenario++)
        printf(" %u", phase_coverage[scenario]);
    printf("\nrunways found %u, missing %u, messages %u, commands %u\n",
           runway_found, runway_missing, messages_shown, commands_posted);
    printf("%u scenarios, %u differ\n", scenarios, bad);
    if (bad == 0)
        printf("IDENTICAL\n");

    free(retail_arena);
    free(ours_arena);
    SetCurrentDirectoryA(original_dir);
    return bad != 0;
}
