/* Flights' dialog routines, the original PE32 code against C (src/flights/).
 *
 *   flightseventsdiff.exe [scenarios] [seed] [normal|unwind]
 *
 * Retail Flights.DLL is mapped without resolving its imports, and every import slot the
 * tested routines use is pointed at a logging stub. Each operation then runs twice from
 * the same randomised fixture, retail then ours, and the call logs, return values and
 * every fixture byte must agree. A stub may swap the descriptor, change the list's count
 * and selection, replace the vtable or alter a window handle mid-call, at a position the
 * scenario chooses. In unwind mode each operation is repeated with a C++-style throw at
 * every call position, comparing what the cleanup frames destroy.
 *
 * Covered: the pickers (Adventures 0x1108, Challenges 0x1107), Save Flight (0x1104),
 * Communications (0x1203), Create a Flight (0x1101), Select Aircraft (0x1201), the welcome
 * dialog (0x1109) and its two sub-menus (0x11090001, 0x11090002), the modeless dialog
 * (id 1), Select Flight (0x1102), Autopilot (0x1204), Navigation (0x1202) and its EFIS page, and
 * the helpers every class shares. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/flights/flights_internal.h"
#include "../../src/flights/flights_objects.h"
#include "../../src/flights/flights_objects_tables.inc"

#define TC FS98_FLIGHTS_THISCALL
#define SC FS98_FLIGHTS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define W16(object, offset) (*(uint16_t *)((uint8_t *)(object) + (offset)))
#define TEST_THROW 0x0e041a22
/* a harness limit reached: say which, and where */
#define GIVE_UP() do { fprintf(stderr, "GIVE UP line %d scenario=%u op=%u side=%d\n", __LINE__, s_scenario, s_operation, s_side); exit(2); } while (0)
#define LOG_SIZE 32768
#define BASE 0x22310000
#define RESOURCE_MODULE 0x31415926
#define OBJECT_MODULE 0x27182818
#define STATIC_VTABLE 0x5a71c000     /* SIMSUI's CSimsStatic vtable, a data import */
#define EDIT_CLASS 0xed17c1a5        /* MFC's CEdit::classCEdit, likewise */

struct fixture {
    uint8_t guard_before[32];
    uint8_t object[0x2178];
    uint8_t descriptor[2][0x200];   /* the NAV page's slice reaches +0x10c */
    char names[2][0x110];
    char texts[4][0x110];           /* Autopilot's heading, altitude, airspeed and Mach; the EFIS page's altitude */
    uint8_t pages[3][0x2450];       /* Navigation's tab pages: NAV, ADF, EFIS, as ui_new hands them out */
    uint8_t whole[16];              /* the EFIS page's whole descriptor: answer, check */
    uint8_t record_before[0xa08];   /* record -1: Select Aircraft indexes with an empty selection */
    uint8_t records[5][0xa08];
    uint8_t info[32];
    uint8_t exchange[32];
    uint8_t font[32];
    struct fs98_flights_create_state create;   /* Create's static font, retail's copied in and out */
    struct fs98_flights_opening_state opening; /* the welcome dialog's statics, likewise */
    struct fs98_flights_menus_state menus;     /* the sub-menus' */
    uint8_t guard_after[32];
};
static struct fixture s_fixture, s_initial, s_retail_result;
static HMODULE s_retail;
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_script;
static uint32_t s_calls, s_log_length, s_throw_at, s_saved_seh;
static uint32_t s_max_calls, s_throws, s_comparisons, s_swaps, s_allocation_faults, s_no_selection;
static int s_side;
static char s_logs[2][LOG_SIZE];
static void *s_vtable[64], *s_other_vtable[64], *s_control_vtable[64], *s_jump[5];

enum signature {
    SIG_EVENT, SIG_HELP, SIG_CONTEXT, SIG_EXCHANGE, SIG_INT, SIG_EMPTY_ARG,
    SIG_INT_ARG, SIG_INT_ARG2, SIG_EXIT, SIG_INT_TWICE, SIG_CONSTRUCT, SIG_DESTROY, SIG_DELETE, SIG_CREATE, SIG_ROUNDTRIP, SIG_CONSTRUCT_DELETE
};
struct operation { uint32_t rva; void *ours; enum signature signature; uint32_t kind; const char *name; };
#define EVENT(rva, name) {rva, (void *)fs98_flights_##name, SIG_EVENT, 0, #name}
#define INT(rva, name) {rva, (void *)fs98_flights_##name, SIG_INT, 0, #name}
#define SAVE(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_SAVE, #name}
#define OPENING(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_OPENING, #name}
#define AUTOPILOT(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_AUTOPILOT, #name}
#define SELECT(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_SELECT, #name}
#define FLIGHT_MENU(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_FLIGHT_MENU, #name}
#define LEARNING_MENU(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_LEARNING_MENU, #name}
#define MODELESS(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_MODELESS, #name}
#define AIRCRAFT(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_AIRCRAFT, #name}
#define CREATE(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_CREATE, #name}
#define NAVIGATION(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_NAVIGATION, #name}
#define NAV_RADIOS(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_NAV_RADIOS, #name}
#define NAV_ADF(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_NAV_ADF, #name}
#define PAGE_LIFETIME(kind, name) \
    {0, (void *)fs98_flights_##name##_construct, SIG_CONSTRUCT, kind, #name "_construct"}, \
    {0, (void *)fs98_flights_##name##_destroy, SIG_DESTROY, kind, #name "_destroy"}, \
    {0, (void *)fs98_flights_##name##_delete, SIG_DELETE, kind, #name "_delete"}, \
    {0, (void *)fs98_flights_##name##_construct, SIG_CONSTRUCT, kind, #name "_construct_again"}, \
    {0, (void *)fs98_flights_##name##_construct, SIG_ROUNDTRIP, kind, #name "_roundtrip"}, \
    {0, (void *)fs98_flights_##name##_construct, SIG_CONSTRUCT_DELETE, kind, #name "_construct_delete"}
#define NAV_EFIS(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_NAV_EFIS, #name}
#define COMMS(rva, name, signature) {rva, (void *)fs98_flights_##name, signature, FS98_FLIGHTS_COMMS, #name}
#define CLASS(help, context, ddx, name, kind) \
    {help, (void *)fs98_flights_##name##_help, SIG_HELP, kind, #name "_help"}, \
    {context, (void *)fs98_flights_##name##_context, SIG_CONTEXT, kind, #name "_context"}, \
    {ddx, (void *)fs98_flights_##name##_exchange, SIG_EXCHANGE, kind, #name "_exchange"}
#define LIFETIME(kind, name) \
    {0, (void *)fs98_flights_##name##_construct, SIG_CONSTRUCT, kind, #name "_construct"}, \
    {0, (void *)fs98_flights_##name##_destroy, SIG_DESTROY, kind, #name "_destroy"}, \
    {0, (void *)fs98_flights_##name##_delete, SIG_DELETE, kind, #name "_delete"}, \
    {0, (void *)fs98_flights_##name##_create, SIG_CREATE, kind, #name "_create"}, \
    {0, (void *)fs98_flights_##name##_construct, SIG_ROUNDTRIP, kind, #name "_roundtrip"}, \
    {0, (void *)fs98_flights_##name##_construct, SIG_CONSTRUCT_DELETE, kind, #name "_construct_delete"}
static const struct operation s_operations[] = {
    EVENT(0x3040, picker_select), EVENT(0x5240, picker_cancel), EVENT(0x54c0, picker_sync),
    EVENT(0x2e50, adventures_accept), EVENT(0x30f0, adventures_populate),
    EVENT(0x3090, adventures_double_click),
    EVENT(0x5260, challenges_accept), EVENT(0x54f0, challenges_populate),
    EVENT(0x5460, challenges_double_click),
    EVENT(0x26a0, post_destroy), EVENT(0x2330, disable), EVENT(0x2340, enable), EVENT(0x2320, empty),
    {0x2310, (void *)fs98_flights_empty_arg, SIG_EMPTY_ARG, 0, "empty_arg"},
    INT(0x5580, picker_no_error), INT(0x5590, picker_validate),
    INT(0x2ec0, adventures_init), INT(0x52e0, challenges_init),
    CLASS(0x30b0, 0x30d0, 0x2db0, adventures, FS98_FLIGHTS_ADVENTURES),
    CLASS(0x5480, 0x54a0, 0x51a0, challenges, FS98_FLIGHTS_CHALLENGES),
    LIFETIME(FS98_FLIGHTS_ADVENTURES, adventures),
    LIFETIME(FS98_FLIGHTS_CHALLENGES, challenges),
    SAVE(0x6490, save_populate, SIG_EVENT), SAVE(0x6440, save_read, SIG_EVENT),
    SAVE(0x6390, save_accept, SIG_EVENT), SAVE(0x63e0, save_cancel, SIG_EVENT),
    SAVE(0x63d0, save_close, SIG_EVENT), SAVE(0x61f0, save_init, SIG_INT),
    SAVE(0x6500, save_validate, SIG_INT_ARG), SAVE(0x6540, save_error, SIG_INT_ARG),
    CLASS(0x6400, 0x6420, 0x6140, save, FS98_FLIGHTS_SAVE),
    LIFETIME(FS98_FLIGHTS_SAVE, save),
    COMMS(0x5bc0, comms_none, SIG_EVENT), COMMS(0x5bf0, comms_take_off, SIG_EVENT),
    COMMS(0x5b80, comms_land, SIG_EVENT), COMMS(0x5c30, comms_cancel, SIG_EVENT),
    COMMS(0x5c50, comms_accept, SIG_EVENT), COMMS(0x5cd0, comms_read, SIG_EVENT),
    COMMS(0x5d90, comms_populate, SIG_EVENT), COMMS(0x5930, comms_init, SIG_INT),
    COMMS(0x5e90, comms_validate, SIG_INT_ARG), COMMS(0x5f20, comms_error, SIG_INT_ARG),
    CLASS(0x5c90, 0x5cb0, 0x5810, comms, FS98_FLIGHTS_COMMS),
    LIFETIME(FS98_FLIGHTS_COMMS, comms),
    CREATE(0x29d0, create_save, SIG_EVENT), CREATE(0x29f0, create_aircraft, SIG_EVENT),
    CREATE(0x2a20, create_location, SIG_EVENT), CREATE(0x2a50, create_weather, SIG_EVENT),
    CREATE(0x2a80, create_time, SIG_EVENT), CREATE(0x2ab0, create_cancel, SIG_EVENT),
    CREATE(0x2ae0, create_accept, SIG_EVENT), CREATE(0x2b60, create_refresh, SIG_EVENT),
    CREATE(0x2c00, create_paint, SIG_EVENT), CREATE(0x26b0, create_init, SIG_INT),
    CREATE(0x26b0, create_init, SIG_INT_TWICE), CREATE(0x29c0, create_font_destroy, SIG_EXIT),
    CLASS(0x2b10, 0x2b30, 0x2590, create, FS98_FLIGHTS_CREATE),
    LIFETIME(FS98_FLIGHTS_CREATE, create),
    AIRCRAFT(0x1ba0, aircraft_cancel, SIG_EVENT), AIRCRAFT(0x1be0, aircraft_accept, SIG_EVENT),
    AIRCRAFT(0x1c40, aircraft_frame, SIG_INT_ARG2), AIRCRAFT(0x1f40, aircraft_populate, SIG_EVENT),
    AIRCRAFT(0x20a0, aircraft_select, SIG_EVENT), AIRCRAFT(0x21c0, aircraft_double_click, SIG_EVENT),
    AIRCRAFT(0x2220, aircraft_details, SIG_EVENT), AIRCRAFT(0x1c80, aircraft_init, SIG_INT),
    CLASS(0x21e0, 0x2200, 0x1ab0, aircraft, FS98_FLIGHTS_AIRCRAFT),
    LIFETIME(FS98_FLIGHTS_AIRCRAFT, aircraft),
    EVENT(0x2b50, nothing),
    OPENING(0xa420, opening_multiplayer, SIG_EVENT), OPENING(0xa500, opening_lessons, SIG_EVENT),
    OPENING(0xa660, opening_more, SIG_EVENT), OPENING(0xa740, opening_new_flight, SIG_EVENT),
    OPENING(0xa5e0, opening_fly, SIG_EVENT), OPENING(0xa870, opening_fly, SIG_EVENT),
    OPENING(0xa620, opening_quit, SIG_EVENT), OPENING(0xa820, opening_refresh, SIG_EVENT),
    OPENING(0xa860, opening_save, SIG_EVENT), OPENING(0xa880, opening_hover, SIG_INT_ARG2),
    OPENING(0xa0c0, opening_init, SIG_INT), OPENING(0xa0c0, opening_init, SIG_INT_TWICE),
    OPENING(0xa410, opening_shadow_destroy, SIG_EXIT), OPENING(0xa400, opening_alternate_destroy, SIG_EXIT),
    OPENING(0xa3f0, opening_hint_destroy, SIG_EXIT),
    CLASS(0x9e00, 0x9e20, 0xa000, opening, FS98_FLIGHTS_OPENING),
    LIFETIME(FS98_FLIGHTS_OPENING, opening),
    MODELESS(0x9d80, modeless_init, SIG_INT), MODELESS(0x9db0, modeless_check, SIG_INT_ARG2),
    MODELESS(0x9de0, modeless_palette, SIG_EMPTY_ARG),
    {0x9d60, (void *)fs98_flights_modeless_exchange, SIG_EXCHANGE, FS98_FLIGHTS_MODELESS, "modeless_exchange"},
    LIFETIME(FS98_FLIGHTS_MODELESS, modeless),
    FLIGHT_MENU(0xaea0, flight_menu_dual, SIG_EVENT), FLIGHT_MENU(0xaf60, flight_menu_video, SIG_EVENT),
    FLIGHT_MENU(0xb020, flight_menu_review, SIG_EVENT), FLIGHT_MENU(0xb920, menu_back, SIG_EVENT),
    FLIGHT_MENU(0xb120, flight_menu_hover, SIG_INT_ARG2), FLIGHT_MENU(0xabf0, flight_menu_init, SIG_INT),
    FLIGHT_MENU(0xabf0, flight_menu_init, SIG_INT_TWICE),
    FLIGHT_MENU(0xae90, flight_menu_shadow_destroy, SIG_EXIT), FLIGHT_MENU(0xae80, flight_menu_alternate_destroy, SIG_EXIT),
    FLIGHT_MENU(0xae70, flight_menu_hint_destroy, SIG_EXIT),
    CLASS(0xb0e0, 0xb100, 0xab60, flight_menu, FS98_FLIGHTS_FLIGHT_MENU),
    LIFETIME(FS98_FLIGHTS_FLIGHT_MENU, flight_menu),
    LEARNING_MENU(0xb710, learning_menu_lessons, SIG_EVENT), LEARNING_MENU(0xb7c0, learning_menu_about, SIG_EVENT),
    LEARNING_MENU(0xb870, learning_menu_quit, SIG_EVENT), LEARNING_MENU(0xb920, menu_back, SIG_EVENT),
    LEARNING_MENU(0xb980, learning_menu_hover, SIG_INT_ARG2), LEARNING_MENU(0xb460, learning_menu_init, SIG_INT),
    LEARNING_MENU(0xb460, learning_menu_init, SIG_INT_TWICE),
    LEARNING_MENU(0xb700, learning_menu_shadow_destroy, SIG_EXIT),
    LEARNING_MENU(0xb6f0, learning_menu_alternate_destroy, SIG_EXIT),
    LEARNING_MENU(0xb6e0, learning_menu_hint_destroy, SIG_EXIT),
    CLASS(0xb940, 0xb960, 0xb3d0, learning_menu, FS98_FLIGHTS_LEARNING_MENU),
    LIFETIME(FS98_FLIGHTS_LEARNING_MENU, learning_menu),
    SELECT(0x6b60, select_cancel, SIG_EVENT), SELECT(0x6b80, select_accept, SIG_EVENT),
    SELECT(0x6bd0, select_remove, SIG_EVENT), SELECT(0x6c40, select_choose, SIG_EVENT),
    SELECT(0x6d30, select_double_click, SIG_EVENT), SELECT(0x6d50, select_populate, SIG_EVENT),
    SELECT(0x6960, select_init, SIG_INT), SELECT(0x6e30, select_validate, SIG_INT_ARG),
    SELECT(0x6f50, select_error, SIG_INT_ARG),
    CLASS(0x6920, 0x6940, 0x6850, select, FS98_FLIGHTS_SELECT),
    LIFETIME(FS98_FLIGHTS_SELECT, select),
    AUTOPILOT(0x3af0, autopilot_cancel, SIG_EVENT), AUTOPILOT(0x3b10, autopilot_accept, SIG_EVENT),
    AUTOPILOT(0x3b50, autopilot_active, SIG_EVENT), AUTOPILOT(0x3ea0, autopilot_level, SIG_EVENT),
    AUTOPILOT(0x42f0, autopilot_heading, SIG_EVENT), AUTOPILOT(0x43c0, autopilot_altitude, SIG_EVENT),
    AUTOPILOT(0x44e0, autopilot_attitude, SIG_EVENT), AUTOPILOT(0x3f30, autopilot_airspeed, SIG_EVENT),
    AUTOPILOT(0x3ff0, autopilot_mach, SIG_EVENT), AUTOPILOT(0x40c0, autopilot_throttle, SIG_EVENT),
    AUTOPILOT(0x4230, autopilot_takeoff, SIG_EVENT), AUTOPILOT(0x4630, autopilot_read, SIG_EVENT),
    AUTOPILOT(0x48d0, autopilot_populate, SIG_EVENT), AUTOPILOT(0x36c0, autopilot_init, SIG_INT),
    AUTOPILOT(0x4db0, autopilot_validate, SIG_INT_ARG), AUTOPILOT(0x4e90, autopilot_error, SIG_INT_ARG),
    CLASS(0x45f0, 0x4610, 0x34f0, autopilot, FS98_FLIGHTS_AUTOPILOT),
    LIFETIME(FS98_FLIGHTS_AUTOPILOT, autopilot),
    NAVIGATION(0x8350, navigation_close, SIG_EVENT), NAVIGATION(0x8370, navigation_cancel, SIG_EVENT),
    NAVIGATION(0x8390, navigation_accept, SIG_EVENT), NAVIGATION(0x8460, navigation_wire, SIG_EVENT),
    NAVIGATION(0x7f20, navigation_validate, SIG_INT_ARG), NAVIGATION(0x7ff0, navigation_init, SIG_INT),
    CLASS(0x8420, 0x8440, 0x7f40, navigation, FS98_FLIGHTS_NAVIGATION),
    LIFETIME(FS98_FLIGHTS_NAVIGATION, navigation),
    NAV_EFIS(0x7480, nav_efis_init, SIG_EVENT), NAV_EFIS(0x76b0, nav_efis_four, SIG_EVENT),
    NAV_EFIS(0x76f0, nav_efis_two, SIG_EVENT), NAV_EFIS(0x7750, nav_efis_first, SIG_EVENT),
    NAV_EFIS(0x7760, nav_efis_second, SIG_EVENT), NAV_EFIS(0x7770, nav_efis_enable, SIG_EVENT),
    NAV_EFIS(0x7b00, nav_efis_refill, SIG_EVENT), NAV_EFIS(0x7d70, nav_efis_message, SIG_INT_ARG2),
    NAV_EFIS(0x7980, nav_efis_validate, SIG_INT_ARG), NAV_EFIS(0x7af0, nav_efis_error, SIG_INT_ARG),
    CLASS(0x7440, 0x7460, 0x72f0, nav_efis, FS98_FLIGHTS_NAV_EFIS),
    /* a page has no factory: its construct stands in that slot, so the round trips line up */
    PAGE_LIFETIME(FS98_FLIGHTS_NAV_EFIS, nav_efis),
    NAV_RADIOS(0x8b80, nav_radios_init, SIG_EVENT), NAV_RADIOS(0x9090, nav_radios_fill, SIG_EVENT),
    NAV_RADIOS(0x8e20, nav_radios_validate, SIG_INT_ARG), NAV_RADIOS(0x8f90, nav_radios_error, SIG_INT_ARG),
    CLASS(0x8de0, 0x8e00, 0x8940, nav_radios, FS98_FLIGHTS_NAV_RADIOS),
    PAGE_LIFETIME(FS98_FLIGHTS_NAV_RADIOS, nav_radios),
    NAV_ADF(0x9810, nav_adf_init, SIG_EVENT), NAV_ADF(0x9ac0, nav_adf_fill, SIG_EVENT),
    NAV_ADF(0x99b0, nav_adf_validate, SIG_INT_ARG), NAV_ADF(0x9a40, nav_adf_error, SIG_INT_ARG),
    CLASS(0x9970, 0x9990, 0x96f0, nav_adf, FS98_FLIGHTS_NAV_ADF),
    PAGE_LIFETIME(FS98_FLIGHTS_NAV_ADF, nav_adf),
};
#define OP_COUNT (sizeof(s_operations) / sizeof(s_operations[0]))
#define OP s_operations[s_operation]

static uint32_t next_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
#define RETAIL_FONT 0x12150      /* Create's static CSimsFont in retail .data, guard byte at +0x70 */
#define RETAIL_OPENING 0x121c8   /* the welcome dialog's statics */
#define RETAIL_MENUS 0x12328     /* the sub-menus' */
static uint32_t offset(const void *pointer)
{
    if (s_retail && pointer == (uint8_t *)s_retail + RETAIL_FONT) pointer = s_fixture.create.font;
    if (s_retail && (const uint8_t *)pointer >= (uint8_t *)s_retail + RETAIL_OPENING &&
        (const uint8_t *)pointer < (uint8_t *)s_retail + RETAIL_OPENING + sizeof(struct fs98_flights_opening_state))
        pointer = (const uint8_t *)&s_fixture.opening + ((const uint8_t *)pointer - ((uint8_t *)s_retail + RETAIL_OPENING));
    if (s_retail && (const uint8_t *)pointer >= (uint8_t *)s_retail + RETAIL_MENUS &&
        (const uint8_t *)pointer < (uint8_t *)s_retail + RETAIL_MENUS + sizeof(struct fs98_flights_menus_state))
        pointer = (const uint8_t *)&s_fixture.menus + ((const uint8_t *)pointer - ((uint8_t *)s_retail + RETAIL_MENUS));
    return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture);
}
static void put(const char *format, ...)
{
    va_list args;
    int length;
    if (s_log_length > LOG_SIZE - 2048) GIVE_UP();
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_log_length, LOG_SIZE - s_log_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= LOG_SIZE - s_log_length) GIVE_UP();
    s_log_length += (uint32_t)length;
    ++s_calls;
    if (s_throw_at && s_calls == s_throw_at) RaiseException(TEST_THROW, 0, 0, 0);
}
/* At one call per operation: swap the descriptor for the other, shrink the list to two
 * entries with the second selected, replace the vtable, change the list's window. */
static uint32_t descriptor_offset(void) { return s_classes[s_operations[s_operation].kind].descriptor; }
static void mutate(void)
{
    uint8_t *object = s_fixture.object;
    uint32_t at = descriptor_offset();
    void *descriptor = PTR(object, at);
    int save = s_operations[s_operation].kind == FS98_FLIGHTS_SAVE;
    int comms = s_operations[s_operation].kind == FS98_FLIGHTS_COMMS;
    int create = s_operations[s_operation].kind == FS98_FLIGHTS_CREATE;
    int aircraft = s_operations[s_operation].kind == FS98_FLIGHTS_AIRCRAFT;
    int efis = s_operations[s_operation].kind == FS98_FLIGHTS_NAV_EFIS;
    int radios = s_operations[s_operation].kind == FS98_FLIGHTS_NAV_RADIOS;
    int adf = s_operations[s_operation].kind == FS98_FLIGHTS_NAV_ADF;
    int opening = s_operations[s_operation].kind == FS98_FLIGHTS_OPENING ||
                  s_operations[s_operation].kind == FS98_FLIGHTS_FLIGHT_MENU ||
                  s_operations[s_operation].kind == FS98_FLIGHTS_LEARNING_MENU;
    /* one point among the first calls and one anywhere in the first 32, any script */
    if (s_calls != 1 + ((s_scenario ^ (s_seed >> 7)) % 4) && s_calls != 5 + ((s_seed >> 12) % 28)) return;
    if ((s_script & 1) && (descriptor == s_fixture.descriptor[0] || descriptor == s_fixture.descriptor[1])) {
        PTR(object, at) = descriptor == s_fixture.descriptor[0] ? s_fixture.descriptor[1]
                                                                : s_fixture.descriptor[0];
        if (!s_side) ++s_swaps;
    }
    descriptor = PTR(object, at);
    if ((s_script & 2) && (descriptor == s_fixture.descriptor[0] || descriptor == s_fixture.descriptor[1])) {
        if (radios) {
            D32(descriptor, 0x18) ^= 1;
            D32(descriptor, 4) ^= 1;
            W16(descriptor, 0x2c) ^= 1;
            W16(descriptor, 0xb4) ^= 1;
        } else if (adf) {
            D32(descriptor, 0x44) ^= 1;
            D32(descriptor, 8) ^= 1;
            W16(descriptor, 0x5a) ^= 1;
        } else if (efis) {
            D32(descriptor, 0x30) ^= 6;     /* 2 and 4 trade places */
            D32(descriptor, 0x40) ^= 1;
            D32(descriptor, 0x4c) ^= 1;
            D32(descriptor, 0x44) ^= 1;
        } else if (save) {
            D32(descriptor, 8) = D32(descriptor, 8) ? 0 : 0x1004;
            D32(descriptor, 0x1c) ^= 0x1003;
        } else if (opening) {
            D32(descriptor, 0x18) ^= 1;
            D32(descriptor, 0x20) ^= 1;
            D32(object, 0x10dc) ^= (s_seed >> 2) & 1;
        } else if (aircraft) {
            D32(descriptor, 0x18) = (s_seed >> 3) & 1 ? 0xffffffff : 2;
            D32(descriptor, 0x14) = 1;
        } else if (create) {
            D32(descriptor, 0x2c) ^= 1;
            PTR(descriptor, 0x20) = 0;
        } else if (comms) {
            D32(descriptor, 0x34) ^= 1;
            D32(descriptor, 0xc) ^= 1;
            D32(descriptor, 0x2c) ^= 1;
        } else {
            D32(descriptor, 8) = 2;
            D32(descriptor, 0xc) = 1;
        }
    }
    if (s_script & 4) PTR(object, 0) = s_other_vtable;
    if (s_script & 8) D32(object, save ? 0x1558 : comms ? 0x1d24 : aircraft ? 0x151c : efis ? 0x1a8c : radios ? 0x2424 : adf ? 0x199c : 0x1270) = s_seed ^ 0x12345678;
    /* Navigation: its second page swapped for the third, so a stale page pointer shows */
    if ((s_script & 2) && s_operations[s_operation].kind == FS98_FLIGHTS_NAVIGATION && PTR(object, 0x2164) == s_fixture.pages[1])
        PTR(object, 0x2164) = s_fixture.pages[2];
}

/* ---- the object's own virtuals --------------------------------------------------------- */
static void TC virtual_close(void *object)
{
    put("virtual.close %u state=%u answers=%u/%u\n", offset(object), D32(object, 0x15e0),
        D32(s_fixture.descriptor[0], 0), D32(s_fixture.descriptor[1], 0));
    mutate();
}
static void TC virtual_close_other(void *object) { put("other.close\n"); virtual_close(object); }
static void TC virtual_accept(void *object) { put("virtual.accept %u\n", offset(object)); mutate(); }
static void TC virtual_accept_other(void *object) { put("other.accept\n"); virtual_accept(object); }
static void TC virtual_cancel(void *object) { put("virtual.cancel %u\n", offset(object)); mutate(); }
static void TC virtual_cancel_other(void *object) { put("other.cancel\n"); virtual_cancel(object); }
static void *TC virtual_delete(void *object, uint32_t flags)
{ put("virtual.delete %u flags=%u\n", offset(object), flags); mutate(); return object; }
/* Every other slot: a wrong slot shows up in the log instead of jumping through null */
#define OTHER_SLOT(n) static void TC virtual_slot_##n(void *object) \
{ put("virtual.slot %x %u\n", n * 4, offset(object)); mutate(); }
#define OTHER_SLOTS(n) OTHER_SLOT(n##0) OTHER_SLOT(n##1) OTHER_SLOT(n##2) OTHER_SLOT(n##3) \
    OTHER_SLOT(n##4) OTHER_SLOT(n##5) OTHER_SLOT(n##6) OTHER_SLOT(n##7) \
    OTHER_SLOT(n##8) OTHER_SLOT(n##9) OTHER_SLOT(n##a) OTHER_SLOT(n##b) \
    OTHER_SLOT(n##c) OTHER_SLOT(n##d) OTHER_SLOT(n##e) OTHER_SLOT(n##f)
OTHER_SLOTS(0x0) OTHER_SLOTS(0x1) OTHER_SLOTS(0x2) OTHER_SLOTS(0x3)
#define SLOT_ROW(n) virtual_slot_##n##0, virtual_slot_##n##1, virtual_slot_##n##2, virtual_slot_##n##3, \
    virtual_slot_##n##4, virtual_slot_##n##5, virtual_slot_##n##6, virtual_slot_##n##7, \
    virtual_slot_##n##8, virtual_slot_##n##9, virtual_slot_##n##a, virtual_slot_##n##b, \
    virtual_slot_##n##c, virtual_slot_##n##d, virtual_slot_##n##e, virtual_slot_##n##f
static void (TC *const s_other_slots[64])(void *) = {
    SLOT_ROW(0x0), SLOT_ROW(0x1), SLOT_ROW(0x2), SLOT_ROW(0x3)
};
/* The replacement vtable's own versions, so a call through the wrong table shows */
#undef OTHER_SLOT
#define OTHER_SLOT(n) static void TC replaced_slot_##n(void *object) \
{ put("replaced.slot %x %u\n", n * 4, offset(object)); mutate(); }
OTHER_SLOTS(0x0) OTHER_SLOTS(0x1) OTHER_SLOTS(0x2) OTHER_SLOTS(0x3)
#undef SLOT_ROW
#define SLOT_ROW(n) replaced_slot_##n##0, replaced_slot_##n##1, replaced_slot_##n##2, replaced_slot_##n##3, \
    replaced_slot_##n##4, replaced_slot_##n##5, replaced_slot_##n##6, replaced_slot_##n##7, \
    replaced_slot_##n##8, replaced_slot_##n##9, replaced_slot_##n##a, replaced_slot_##n##b, \
    replaced_slot_##n##c, replaced_slot_##n##d, replaced_slot_##n##e, replaced_slot_##n##f
static void (TC *const s_replaced_slots[64])(void *) = {
    SLOT_ROW(0x0), SLOT_ROW(0x1), SLOT_ROW(0x2), SLOT_ROW(0x3)
};
static uint32_t TC virtual_validate(void *object, uint32_t argument)
{
    put("virtual.validate %u %x\n", offset(object), argument); mutate();
    return s_scenario % 3 == 0 ? 0 : 0xcc8 + (s_scenario & 1) * 0x10;
}
static uint32_t TC virtual_error(void *object, uint32_t code)
{ put("virtual.error %u %x\n", offset(object), code); mutate(); return s_scenario & 4 ? 0 : 0xcbd; }
static int32_t TC virtual_init_tab(void *object, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, uint32_t module)
{ put("virtual.init.tab %u %d/%d/%d/%d/%d module=%x\n", offset(object), a, b, c, d, e, module); mutate(); return (int32_t)s_seed; }
static int32_t TC virtual_check(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{
    put("init.check %u font=%u bmp=%x dib=%u pal=%u module=%x\n", offset(control), offset(font),
        bitmap, offset(dib), offset(palette), module);
    mutate(); return (int32_t)s_seed;
}
#define OPENING_CALLBACK(slot) static uint32_t SC opening_callback_##slot(void *descriptor) \
{ \
    put("opening.callback %x %u busy=%u\n", slot, offset(descriptor), D32(s_fixture.object, 0x10dc)); mutate(); \
    return (s_seed >> (slot / 4)) & 1 ? s_seed : 0; \
}
OPENING_CALLBACK(0x8) OPENING_CALLBACK(0xc) OPENING_CALLBACK(0x10) OPENING_CALLBACK(0x14)
static uint32_t SC aircraft_details(void *descriptor)
{
    put("aircraft.details %u selection=%d\n", offset(descriptor), (int32_t)D32(descriptor, 0x14)); mutate();
    return (s_seed >> 15) % 3 ? 0 : s_seed;
}
static uint32_t SC aircraft_preview(uint32_t operation, void *descriptor, uint32_t window);
static uint32_t SC aircraft_preview_other(uint32_t operation, void *descriptor, uint32_t window)
{ put("other.preview\n"); return aircraft_preview(operation, descriptor, window); }
static uint32_t SC aircraft_preview(uint32_t operation, void *descriptor, uint32_t window)
{
    put("aircraft.preview op=%u %u window=%u selection=%d answer=%u\n", operation, offset(descriptor),
        window, (int32_t)D32(descriptor, 0x14), D32(descriptor, 0));
    mutate(); return s_seed;
}
#define CREATE_CALLBACK(slot) static uint32_t SC create_callback_##slot(void *descriptor) \
{ \
    put("create.callback %x %u ready=%u\n", slot, offset(descriptor), D32(descriptor, 0x2c)); mutate(); \
    D32(descriptor, 0x2c) = (s_seed >> 9) & 1; \
    return s_seed; \
}
CREATE_CALLBACK(0x8) CREATE_CALLBACK(0xc) CREATE_CALLBACK(0x10) CREATE_CALLBACK(0x14) CREATE_CALLBACK(0x18)
static uint32_t TC virtual_focus(void *object, uint32_t control)
{ put("virtual.e0 %u %x\n", offset(object), control); mutate(); return s_seed ^ control; }
static uint32_t SC comms_check(void *descriptor)
{
    put("comms.check %u frequency=[%s] request=%u slow=%u rate=%u\n", offset(descriptor),
        (char *)PTR(descriptor, 0x10), D32(descriptor, 0x34), D32(descriptor, 0x2c), D32(descriptor, 0x3c));
    mutate();
    if (s_scenario % 5 == 2) D32(descriptor, 0xc) = 0xc;
    return s_scenario & 1 ? 0xffffffff : 0;
}
static uint32_t SC save_check(void *descriptor)
{
    put("save.check %u name=[%s] description=[%s] default=%u\n", offset(descriptor),
        (char *)PTR(descriptor, 0xc), (char *)PTR(descriptor, 0x20), D32(descriptor, 0x30));
    mutate();
    if (s_scenario % 5 == 1) D32(descriptor, 8) = 0x1004;
    return s_scenario % 7 == 0 ? 0 : 0x1000 + s_scenario % 0x10;
}
static int32_t TC virtual_skin(void *object, int32_t resource, uint32_t module, uint32_t flags);
static int32_t TC replaced_skin(void *object, int32_t resource, uint32_t module, uint32_t flags)
{ put("replaced.skin\n"); return virtual_skin(object, resource, module, flags); }
static int32_t TC virtual_skin(void *object, int32_t resource, uint32_t module, uint32_t flags)
{
    put("skin %u resource=%x module=%x flags=%u\n", offset(object), resource, module, flags);
    mutate(); return s_scenario % 4 ? (int32_t)s_seed | 1 : 0;
}

/* ---- services ------------------------------------------------------------------------- */
static int32_t send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{
    uint32_t result = s_seed;
    if (message == 0x180) put("message %u %x %u text=%s\n", window, message, w_param, (char *)(uintptr_t)l_param);
    else put("message %u %x %u %u\n", window, message, w_param, l_param);
    mutate();
    if (message == 0x147)   /* CB_GETCURSEL: none, a valid one, or one past the last */
        return (int32_t)((s_seed >> (s_calls % 8)) % 7) - 1;
    if (message == 0xb8)    /* EM_GETMODIFY: unmodified, one time in three */
        return (s_seed >> (s_calls % 16)) % 3 ? 1 : 0;
    if (message == 0x199)   /* LB_GETITEMDATA: the record the item was tagged with */
        return (int32_t)(uintptr_t)s_fixture.records[w_param % 5];
    if (message == 0x188) {
        result = s_scenario % 5 == 0 ? 0xffffffff : (s_scenario + s_calls) % 4;
        if (result == 0xffffffff && !s_side) ++s_no_selection;
    }
    return (int32_t)result;
}
static int32_t win_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{
    uint32_t index = 0;
    char pairs[1024];
    int length = 0;
    do {
        if (index >= 128 || length > 900) GIVE_UP();
        length += snprintf(pairs + length, sizeof(pairs) - length, "%x:%x,", table[index], table[index + 1]);
        index += 2;
    } while (table[index - 2] || table[index - 1]);
    put("help %u %s %u [%s]\n", window, file, command, pairs);
    mutate(); return (int32_t)s_seed;
}
static int32_t enable_window(uint32_t window, int32_t enable)
{ put("enable.window %u %d\n", window, enable); mutate(); return (int32_t)s_seed; }
static void base_exchange(void *object, void *exchange)
{ put("base.exchange %u %u\n", offset(object), offset(exchange)); mutate(); }
static void ddx_control(void *exchange, uint32_t id, void *control)
{ put("ddx %u %x %u\n", offset(exchange), id, offset(control)); mutate(); }
static int32_t base_init(void *object)
{ put("base.init %u\n", offset(object)); mutate(); return (int32_t)s_seed; }
static int32_t init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{
    put("init.art %u bitmap=%x dib=%u pal=%u flags=%u\n", offset(control), bitmap, offset(dib), offset(palette), flags);
    mutate(); return (int32_t)s_seed;
}
static int32_t init_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                          void *dib, void *palette, uint32_t flags, uint32_t color,
                          uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{
    put("init.button %u font=%u/%u bmp=%x dib=%u pal=%u flags=%u color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        offset(control), offset(font), offset(alternate_font), bitmap, offset(dib), offset(palette), flags,
        color, module, x, y, rect.left, rect.top, rect.right, rect.bottom);
    mutate(); return (int32_t)s_seed;
}
static void init_text(void *control, void *font)
{ put("init.text %u font=%u\n", offset(control), offset(font)); mutate(); }
static void init_list(void *control, void *font, void *label)
{ put("init.list %u font=%u label=%u\n", offset(control), offset(font), offset(label)); mutate(); }
static void init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags)
{
    put("init.edit %u font=%u fg=%x bg=%x error=%x label=%u flags=%u\n", offset(control),
        offset(font), foreground, background, error_color, offset(label), flags);
    mutate();
}
static void *base_font(int32_t id) { put("base.font %d\n", id); mutate(); return s_fixture.font; }
static void set_window_text(void *window, const char *text)
{ put("set.text %u [%s]\n", offset(window), text); mutate(); }
static int32_t get_window_text(void *window, char *buffer, int32_t count)
{
    char text[64];
    if ((uint8_t *)buffer >= (uint8_t *)&s_fixture && (uint8_t *)buffer < (uint8_t *)(&s_fixture + 1))
        put("get.text %u %u count=%d\n", offset(window), offset(buffer), count);
    else put("get.text %u local count=%d\n", offset(window), count);   /* a caller's stack buffer */
    if (s_operations[s_operation].kind == FS98_FLIGHTS_COMMS)    /* a frequency, or not quite one */
        snprintf(text, sizeof(text), s_scenario % 6 ? "1%02u.%u%u5" : "%u..x", s_scenario % 100, s_seed % 10, s_calls % 10);
    else snprintf(text, sizeof(text), "%s %u", s_scenario % 4 ? "Typed" : "Flight 0 vector", s_scenario);
    if (count > 0) { strncpy(buffer, text, (size_t)count - 1); buffer[count - 1] = 0; }
    mutate(); return count > 0 ? (int32_t)strlen(buffer) : 0;
}
static int32_t set_checked(void *control, int32_t checked)
{ put("set.checked %u %d\n", offset(control), checked); mutate(); return (int32_t)s_seed; }
static int32_t error_dialog(void *object, void *control, uint32_t code)
{
    put("error.dialog %u control=%u code=%x fields=%x/%x\n", offset(object), offset(control), code,
        D32(object, 0x90), D32(object, 0x94));
    mutate(); return s_scenario & 8 ? 0 : (int32_t)(s_seed | 1);
}
static int32_t post_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ put("post %u %x %u %u\n", window, message, w_param, l_param); mutate(); return (int32_t)s_seed; }
static char *copy_text(char *out, const char *text)
{ put("copy.text %u [%s]\n", offset(out), text); strcpy(out, text); mutate(); return out; }
static int32_t compare_text(const char *a, const char *b)
{ put("compare.text [%s] [%s]\n", a, b); mutate(); return strcmp(a, b); }
static void close_dialog(void *object) { put("base.close %u\n", offset(object)); mutate(); }
static int32_t get_window_long(uint32_t window, int32_t index)
{
    put("get.window.long %u %d\n", window, index); mutate();
    return (s_scenario + s_calls) % 3 ? (int32_t)window : (int32_t)(window ^ 1);
}
static uint32_t get_focus(void) { put("get.focus\n"); mutate(); return s_seed ^ 0xf0c5; }
static int32_t enable_control(void *window, int32_t enable)
{ put("enable.control %u %d\n", offset(window), enable); mutate(); return (int32_t)s_seed; }
static int32_t set_radio(void *control, int32_t checked)
{ put("set.radio %u %d\n", offset(control), checked); mutate(); return (int32_t)s_seed; }
static int32_t init_slider(void *control, uint32_t bitmap, uint32_t thumb, void *dib, void *palette,
                           uint32_t a, uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f, uint32_t module)
{
    put("init.slider %u bmp=%x/%x dib=%u pal=%u %u/%u/%u/%u/%d/%d module=%x\n", offset(control), bitmap,
        thumb, offset(dib), offset(palette), a, b, c, d, e, f, module);
    mutate(); return (int32_t)s_seed;
}
static void slider_range(void *control, int32_t low, int32_t high)
{ put("slider.range %u %d..%d\n", offset(control), low, high); mutate(); }
static int32_t slider_position(void *control)
{ put("slider.position %u\n", offset(control)); mutate(); return (int32_t)(s_seed % 15); }
static int32_t set_slider_position(void *control, int32_t position)
{ put("slider.set %u %d\n", offset(control), position); mutate(); return (int32_t)s_seed; }
static void init_group(void *control, void *font)
{ put("init.group %u font=%u\n", offset(control), offset(font)); mutate(); }
static int32_t init_picture(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t module, uint16_t color)
{
    put("init.picture %u bmp=%x dib=%u pal=%u module=%x color=%x\n", offset(control), bitmap,
        offset(dib), offset(palette), module, color);
    mutate(); return (int32_t)s_seed;
}
static void *set_focus(void *window) { put("set.focus %u\n", offset(window)); mutate(); return s_fixture.info; }
static void set_sims_text(void *control, const char *text)
{ put("set.sims.text %u [%s]\n", offset(control), text ? text : "(null)"); mutate(); }
static void font_construct(void *font, void *base_font)
{
    uint8_t guard = s_side ? s_fixture.create.guard : *((uint8_t *)s_retail + RETAIL_FONT + 0x70);
    uint8_t opening = s_side ? s_fixture.opening.guard : *((uint8_t *)s_retail + RETAIL_OPENING + 0x70);
    uint8_t flight = s_side ? s_fixture.menus.flight_guard : *((uint8_t *)s_retail + RETAIL_MENUS + 0x150);
    uint8_t learning = s_side ? s_fixture.menus.learning_guard : *((uint8_t *)s_retail + RETAIL_MENUS + 0x158);
    put("font.construct %u base=%u guard=%u/%u/%u/%u\n", offset(font), offset(base_font), guard, opening, flight, learning);
    memset(font, s_seed & 255, 0x70); mutate();
}
static void font_destroy(void *font)
{ put("font.destroy %u value=%x/%x\n", offset(font), D32(font, 4), D32(font, 0x50)); mutate(); }
static void register_exit(void (*handler)(void))
{
    static void (*const ours[])(void) = {
        fs98_flights_create_font_destroy, fs98_flights_opening_shadow_destroy,
        fs98_flights_opening_alternate_destroy, fs98_flights_opening_hint_destroy,
        fs98_flights_flight_menu_shadow_destroy, fs98_flights_flight_menu_alternate_destroy,
        fs98_flights_flight_menu_hint_destroy, fs98_flights_learning_menu_shadow_destroy,
        fs98_flights_learning_menu_alternate_destroy, fs98_flights_learning_menu_hint_destroy
    };
    static const uint32_t retail[] = {0x29c0, 0xa410, 0xa400, 0xa3f0, 0xae90, 0xae80, 0xae70, 0xb700, 0xb6f0, 0xb6e0};
    uint32_t which;
    for (which = 0; which < 10; ++which)
        if (handler == (s_side ? ours[which] : (void (*)(void))((uint8_t *)s_retail + retail[which]))) break;
    put("register.exit which=%u\n", which);
    mutate();
}
static uint32_t text_offset(const void *text)
{
    if ((const uint8_t *)text >= (const uint8_t *)&s_fixture && (const uint8_t *)text < (const uint8_t *)(&s_fixture + 1))
        return offset(text);
    return 0xffffffff;
}
static void string_construct(void *string)
{ put("string.construct\n"); ((struct fs98_flights_string *)string)->text = (char *)0x5717; mutate(); }
static void string_destroy(void *string)
{ put("string.destroy text=%x\n", text_offset(((struct fs98_flights_string *)string)->text)); mutate(); }
static void list_text(void *list, int32_t index, void *string)
{
    put("list.text %u %d\n", offset(list), index);
    ((struct fs98_flights_string *)string)->text = (char *)s_fixture.records[(uint32_t)index % 5];
    mutate();
}
static void *dialog_item(void *object, int32_t id)
{
    put("dialog.item %u %x\n", offset(object), id); mutate();
    /* the NAV page's focus: none, one time in four */
    if (s_operations[s_operation].kind == FS98_FLIGHTS_NAV_RADIOS && (s_seed >> 20) % 4 == 0) return 0;
    return s_fixture.info;
}
static uint32_t dialog_item_window(uint32_t window, int32_t id)
{ put("dialog.item.window %u %x\n", window, id); mutate(); return s_seed ^ (uint32_t)id; }
static void remap_rect(struct fs98_flights_rect *rect, void *control)
{
    put("remap.rect %u\n", offset(control));
    rect->left = (int32_t)(s_seed & 0x3ff); rect->top = (int32_t)((s_seed >> 10) & 0x3ff);
    rect->right = (int32_t)((s_seed >> 4) & 0x7ff); rect->bottom = (int32_t)((s_seed >> 20) & 0x7ff);
    mutate();
}
static void move_window(void *window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ put("move.window %u %d/%d %d/%d repaint=%d\n", offset(window), x, y, width, height, repaint); mutate(); }
static int32_t redraw_window(uint32_t window, void *rect, void *region, uint32_t flags)
{
    put("redraw %u %u %u %x\n", window, (uint32_t)(uintptr_t)rect, (uint32_t)(uintptr_t)region, flags);
    mutate(); return (int32_t)s_seed;
}
static void *window_from_handle(uint32_t window)
{
    static const uint32_t welcome[] = {0x11a8, 0x129c, 0x1390, 0x1484, 0x1578, 0x10e0};
    static const uint32_t menus[] = {0x12c4, 0x13b8, 0x14ac, 0x11c8, 0x10e0, 0x13b8};
    const uint32_t *buttons = s_operations[s_operation].kind == FS98_FLIGHTS_OPENING ? welcome : menus;
    put("from.handle %u\n", window); mutate();
    return s_fixture.object + buttons[((s_seed >> 4) + s_calls) % 6];
}
static int32_t load_string_resource(void *string, uint32_t id)
{
    put("load.string %x\n", id);
    ((struct fs98_flights_string *)string)->text = s_fixture.names[id & 1];
    mutate(); return 1;
}
static int32_t init_shadow_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                                  void *dib, void *palette, uint32_t flags, uint32_t color,
                                  uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{
    put("init.shadow %u font=%u/%u bmp=%x dib=%u pal=%u flags=%x color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        offset(control), offset(font), offset(alternate_font), bitmap, offset(dib), offset(palette), flags,
        color, module, x, y, rect.left, rect.top, rect.right, rect.bottom);
    mutate(); return (int32_t)s_seed;
}
static void init_combo(void *control, void *font, void *label)
{ put("init.combo %u font=%u label=%u\n", offset(control), offset(font), offset(label)); mutate(); }
static uint32_t SC autopilot_check(void *descriptor)
{
    put("autopilot.check %u active=%u mode=%u hold=%u speed=%u heading=[%s] altitude=[%s]\n", offset(descriptor),
        D32(descriptor, 0x10), D32(descriptor, 0x18), D32(descriptor, 0x44), D32(descriptor, 0x74),
        (char *)PTR(descriptor, 0x20), (char *)PTR(descriptor, 0x4c));
    mutate();
    D32(descriptor, 0x1c) = (s_seed >> 3) % 3 == 0;
    D32(descriptor, 0x48) = (s_seed >> 5) % 3 == 0;
    return s_seed;
}
static uint32_t SC navigation_check(void *descriptor)
{
    uint8_t *slice = PTR(s_fixture.object, 0x1118);
    if (s_operations[s_operation].kind == FS98_FLIGHTS_NAV_EFIS)
        put("navigation.check %u efis=%u/%u/%u altitude=[%s]\n", offset(descriptor), D32(slice, 0x30),
            D32(slice, 0x40), D32(slice, 0x10), (char *)PTR(slice, 0x50));
    else if (s_operations[s_operation].kind == FS98_FLIGHTS_NAV_RADIOS)
        put("navigation.check %u nav=%u [%s] [%s] [%s] [%s]\n", offset(descriptor), D32(slice, 0x9c),
            (char *)PTR(slice, 0x1c), (char *)PTR(slice, 0x34), (char *)PTR(slice, 0xa4), (char *)PTR(slice, 0xbc));
    else
        put("navigation.check %u adf=[%s] transponder=[%s]\n", offset(descriptor), (char *)PTR(slice, 0x48),
            (char *)PTR(slice, 0x34));
    mutate();
    return (s_seed >> 9) % 3 ? s_seed : 0;
}
static void ddx_text(void *exchange, uint32_t id, void *string)
{ put("ddx.text %u %x %u\n", offset(exchange), id, offset(string)); mutate(); }
static void ddv_max_chars(void *exchange, void *string, int32_t count)
{ put("ddv.max %u %u %d\n", offset(exchange), offset(string), count); mutate(); }
static int32_t message_box(const char *text, uint32_t type, uint32_t help)
{ put("message.box [%s] %x %x\n", text, type, help); mutate(); return (int32_t)s_seed; }
static int32_t is_kind_of(void *object, const void *runtime_class)
{
    put("is.kind.of %u class=%x\n", offset(object), (uint32_t)(uintptr_t)runtime_class); mutate();
    return (s_seed >> 19) & 1;
}
static int32_t show_dialog(void *object, int32_t show);
static int32_t tab_init(void *tabs, void *dialog, void *font, uint32_t bitmap, void *dib, void *palette,
                        uint32_t a, uint32_t color, int32_t b, int32_t c, int32_t d, int32_t e,
                        uint32_t module, uint32_t flags)
{
    put("tab.init %u dialog=%u font=%u bmp=%x dib=%u pal=%u %u color=%x %d/%d/%d/%d module=%x flags=%u\n",
        offset(tabs), offset(dialog), offset(font), bitmap, offset(dib), offset(palette), a, color, b, c, d, e,
        module, flags);
    mutate(); return (int32_t)s_seed;
}
static void tab_add_page(void *tabs, void *page, char *caption, int32_t flag)
{ put("tab.add %u page=%u [%s] flag=%d\n", offset(tabs), offset(page), caption, flag); mutate(); }
static void tab_helper_init(void *helper, void *tabs)
{ put("tab.helper %u tabs=%u\n", offset(helper), offset(tabs)); mutate(); }
static int32_t tab_set_last_page(void *tabs, void *page)
{
    put("tab.last %u page=%u\n", offset(tabs), offset(page)); mutate();
    /* the dialog's descriptor gone, now and then, after the tabs have read it */
    if ((s_seed >> 29) % 3 == 0) PTR(s_fixture.object, 0x10dc) = 0;
    return (int32_t)s_seed;
}
static int32_t tab_set_active(void *tabs, int32_t index, uint32_t window)
{ put("tab.active %u %d window=%u\n", offset(tabs), index, window); mutate(); return (int32_t)s_seed; }
static int32_t page_init(void *page, int32_t value)
{ put("page.init %u %d\n", offset(page), value); mutate(); return (int32_t)s_seed; }
static int32_t load_string_a(uint32_t instance, uint32_t id, char *buffer, int32_t size)
{
    put("load.string.a %x %x size=%d\n", instance, id, size);
    snprintf(buffer, (size_t)size, "Tab %x %u", id, s_scenario);
    mutate(); return (int32_t)strlen(buffer);
}
static int32_t window_enabled(void *window)
{ put("window.enabled %u\n", offset(window)); mutate(); return (s_seed >> (s_calls % 16)) & 1; }
static uint32_t SC select_delete_callback(void *descriptor)
{
    put("select.delete %u selection=%d protected=%d\n", offset(descriptor), (int32_t)D32(descriptor, 0x14),
        (int32_t)D32(descriptor, 0x20));
    mutate(); return s_seed;
}
static uint32_t SC select_edit(void *descriptor, void *record)
{
    static const uint32_t answers[] = {0, 0, 0x1004, 0x1001, 0x100d};
    put("select.edit %u title=[%s] description=[%.40s] path=[%.20s]\n", offset(descriptor), (char *)record,
        (char *)record + 0x100, (char *)record + 0x900);
    mutate(); return answers[(s_seed >> (s_calls % 8)) % 5];
}
static void default_paint(void *object) { put("default.paint %u\n", offset(object)); mutate(); }
static double to_number(const char *text)
{ put("atof [%s]\n", text); mutate(); return atof(text); }
static int32_t format_number(char *out, const char *format, double value)
{
    int32_t length = sprintf(out, format, value);
    put("sprintf [%s] -> [%s]\n", format, out); mutate();
    return length;
}
static const struct fs98_flights_services s_services = {
    send_message, win_help, enable_window, base_exchange, ddx_control, base_init, init_art,
    init_button, init_text, init_list, init_edit, base_font, set_window_text,
    get_window_text, set_checked, error_dialog, post_message, copy_text, compare_text, close_dialog,
    get_window_long, get_focus, enable_control, set_radio, init_slider, slider_range, slider_position,
    set_slider_position, init_group, to_number, format_number,
    init_picture, set_focus, set_sims_text, font_construct, font_destroy, register_exit, default_paint,
    string_construct, string_destroy, list_text, dialog_item, dialog_item_window, remap_rect, move_window,
    redraw_window, window_from_handle, load_string_resource, init_shadow_button, window_enabled, init_combo,
    ddx_text, ddv_max_chars, message_box, is_kind_of, show_dialog, tab_init, tab_add_page,
    tab_helper_init, tab_set_last_page, tab_set_active, page_init, load_string_a
};

static void *dialog_construct(void *object, int32_t resource, void *parent)
{
    put("dialog.construct %u resource=%x parent=%u\n", offset(object), resource, (uint32_t)(uintptr_t)parent);
    D32(object, 0) = 0xba5e0000; D32(object, 0x20) = s_seed;
    mutate(); return object;
}
static void dialog_destroy(void *object)
{ put("dialog.destroy %u vtable=%x\n", offset(object), D32(object, 0)); D32(object, 0) = 0xdead0000; mutate(); }
static void *string_assign(void *string, const char *text)
{
    put("string.assign %u [%s] was=%x\n", offset(string), text, text_offset(((struct fs98_flights_string *)string)->text));
    mutate(); return string;
}
static void *page_construct(void *object, int32_t resource, void *parent)
{
    put("page.construct %u resource=%x parent=%u\n", offset(object), resource, (uint32_t)(uintptr_t)parent);
    D32(object, 0) = 0xba5e1111; D32(object, 0x20) = s_seed;
    mutate(); return object;
}
static void page_destroy(void *object)
{ put("page.destroy %u vtable=%x\n", offset(object), D32(object, 0)); D32(object, 0) = 0xdead1111; mutate(); }
static void construct_control(void *control, uint32_t import)
{
    if (import == 0x133dc) { string_construct(control); return; }    /* a CString member */
    put("control.construct %u import=%x\n", offset(control), import);
    D32(control, 0) = import; D32(control, 4) = s_seed;
    mutate();
}
static void destroy_control(void *control, uint32_t import)
{
    if (import == 0x133e8) { string_destroy(control); return; }
    put("control.destroy %u import=%x vtable=%x\n", offset(control), import, D32(control, 0));
    D32(control, 0) = import; mutate();
}
static void *allocate_object(uint32_t size)
{
    put("allocate %x\n", size); mutate();
    if (s_operations[s_operation].kind == FS98_FLIGHTS_NAVIGATION && s_operations[s_operation].rva == 0x7ff0)
        /* Navigation's pages, each in its own block; any of them may fail */
        return (s_seed >> (size % 7)) % 9 == 0 ? 0 : s_fixture.pages[size == 0x244c ? 0 : size == 0x1a24 ? 1 : 2];
    return s_scenario % 11 ? s_fixture.object : 0;
}
static void ui_delete(void *object) { put("ui.delete %u\n", offset(object)); mutate(); }
static void dialog_delete(void *object) { put("dialog.delete %u\n", offset(object)); mutate(); }
static int32_t create_dialog(void *object, uint32_t resource, void *parent)
{
    put("dialog.create %u resource=%x parent=%u module=%x descriptor=%u\n", offset(object), resource,
        (uint32_t)(uintptr_t)parent, D32(object, 0x80), offset(PTR(object, 0x10dc)));
    mutate(); return s_scenario & 1 ? 0 : (int32_t)s_seed;
}
static int32_t show_dialog(void *object, int32_t show)
{ put("dialog.show %u %d\n", offset(object), show); mutate(); return s_scenario & 2 ? 0 : (int32_t)s_seed; }
static const struct fs98_flights_object_services s_object_services = {
    dialog_construct, dialog_destroy, construct_control, destroy_control, allocate_object,
    ui_delete, dialog_delete, create_dialog, show_dialog, copy_text, page_construct, page_destroy, string_assign
};

/* ---- the same stubs behind retail's import ABIs ----------------------------------------- */
#define CONTROL_HOOK(name, ctor, dtor) \
static void *TC hook_construct_##name(void *control) { construct_control(control, ctor); return control; } \
static void TC hook_destroy_##name(void *control) { destroy_control(control, dtor); }
CONTROL_HOOK(text, 0x135b8, 0x135d0)
CONTROL_HOOK(list, 0x135bc, 0x135d4)
CONTROL_HOOK(edit, 0x135c0, 0x135d8)
CONTROL_HOOK(button, 0x135b4, 0x135cc)
CONTROL_HOOK(art, 0x135c4, 0x135a8)
CONTROL_HOOK(check, 0x13558, 0x13560)
CONTROL_HOOK(radio, 0x1353c, 0x13548)
CONTROL_HOOK(slider, 0x13538, 0x13544)
CONTROL_HOOK(group, 0x13540, 0x13534)
CONTROL_HOOK(picture, 0x13578, 0x1357c)
CONTROL_HOOK(shadow, 0x13574, 0x1360c)
CONTROL_HOOK(combo, 0x1355c, 0x13564)
CONTROL_HOOK(tabs, 0x134f0, 0x134f8)
CONTROL_HOOK(static, 0x1338c, 0x13388)
CONTROL_HOOK(tab_helper, 0x134f4, 0x134fc)
#undef CONTROL_HOOK
static void *TC hook_dialog_construct(void *object, int32_t resource, void *parent)
{ return dialog_construct(object, resource, parent); }
static void TC hook_dialog_destroy(void *object) { dialog_destroy(object); }
static void SC hook_dialog_delete(void *object) { dialog_delete(object); }
static void *hook_allocate(uint32_t size) { return allocate_object(size); }
static void hook_ui_delete(void *object) { ui_delete(object); }
static int32_t TC hook_create_dialog(void *object, uint32_t resource, void *parent)
{ return create_dialog(object, resource, parent); }
static int32_t TC hook_show_dialog(void *object, int32_t show) { return show_dialog(object, show); }
static int32_t SC hook_send(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return send_message(a, b, c, d); }
static int32_t SC hook_help(uint32_t a, const char *b, uint32_t c, const uint32_t *d) { return win_help(a, b, c, d); }
static int32_t SC hook_enable(uint32_t a, int32_t b) { return enable_window(a, b); }
static void TC hook_exchange(void *a, void *b) { base_exchange(a, b); }
static void SC hook_ddx(void *a, uint32_t b, void *c) { ddx_control(a, b, c); }
static int32_t TC hook_base_init(void *object) { return base_init(object); }
static int32_t TC hook_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return init_art(control, bitmap, dib, palette, flags); }
static int32_t TC hook_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                             void *dib, void *palette, uint32_t flags, uint32_t color,
                             uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{ return init_button(control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void TC hook_init_text(void *control, void *font) { init_text(control, font); }
static void TC hook_init_list(void *control, void *font, void *label) { init_list(control, font, label); }
static void TC hook_init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                              uint32_t error_color, void *label, uint32_t flags)
{ init_edit(control, font, foreground, background, error_color, label, flags); }
static void *SC hook_base_font(int32_t id) { return base_font(id); }
static void TC hook_set_text(void *window, const char *text) { set_window_text(window, text); }
static int32_t TC hook_get_text(void *window, char *buffer, int32_t count) { return get_window_text(window, buffer, count); }
static int32_t TC hook_set_checked(void *control, int32_t checked) { return set_checked(control, checked); }
static int32_t TC hook_error_dialog(void *object, void *control, uint32_t code) { return error_dialog(object, control, code); }
static int32_t SC hook_post(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return post_message(a, b, c, d); }
static char *SC hook_copy(char *out, const char *text) { return copy_text(out, text); }
static int32_t SC hook_compare(const char *a, const char *b) { return compare_text(a, b); }
static void TC hook_close(void *object) { close_dialog(object); }
static int32_t SC hook_get_long(uint32_t window, int32_t index) { return get_window_long(window, index); }
static uint32_t SC hook_get_focus(void) { return get_focus(); }
static int32_t TC hook_enable_control(void *window, int32_t enable) { return enable_control(window, enable); }
static int32_t TC hook_set_radio(void *control, int32_t checked) { return set_radio(control, checked); }
static int32_t TC hook_init_slider(void *control, uint32_t bitmap, uint32_t thumb, void *dib, void *palette,
                                   uint32_t a, uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f, uint32_t module)
{ return init_slider(control, bitmap, thumb, dib, palette, a, b, c, d, e, f, module); }
static void TC hook_slider_range(void *control, int32_t low, int32_t high) { slider_range(control, low, high); }
static int32_t TC hook_slider_position(void *control) { return slider_position(control); }
static int32_t TC hook_set_slider_position(void *control, int32_t position) { return set_slider_position(control, position); }
static void TC hook_init_group(void *control, void *font) { init_group(control, font); }
static double hook_atof(const char *text) { return to_number(text); }
static int32_t TC hook_init_picture(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t module, uint16_t color)
{ return init_picture(control, bitmap, dib, palette, module, color); }
static void *TC hook_set_focus(void *window) { return set_focus(window); }
static void TC hook_set_sims_text(void *control, const char *text) { set_sims_text(control, text); }
static void *TC hook_font_construct(void *font, void *base_font) { font_construct(font, base_font); return font; }
static void TC hook_font_destroy(void *font) { font_destroy(font); }
static void *hook_dll_onexit(void (*handler)(void), void **begin, void **end)
{ (void)begin; (void)end; register_exit(handler); return (void *)handler; }
static void *hook_onexit(void (*handler)(void)) { register_exit(handler); return (void *)handler; }
static void TC hook_paint(void *object) { default_paint(object); }
static int32_t TC hook_window_enabled(void *window) { return window_enabled(window); }
static void TC hook_init_combo(void *control, void *font, void *label) { init_combo(control, font, label); }
static int32_t SC hook_redraw(uint32_t window, void *rect, void *region, uint32_t flags)
{ return redraw_window(window, rect, region, flags); }
static void *SC hook_from_handle(uint32_t window) { return window_from_handle(window); }
static int32_t TC hook_load_string(void *string, uint32_t id) { return load_string_resource(string, id); }
static int32_t TC hook_shadow(void *control, void *font, void *alternate_font, uint32_t bitmap,
                              void *dib, void *palette, uint32_t flags, uint32_t color,
                              uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{ return init_shadow_button(control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void *TC hook_string_construct(void *string) { string_construct(string); return string; }
static void TC hook_string_destroy(void *string) { string_destroy(string); }
static void TC hook_list_text(void *list, int32_t index, void *string) { list_text(list, index, string); }
static void *TC hook_dialog_item(void *object, int32_t id) { return dialog_item(object, id); }
static uint32_t SC hook_dialog_item_window(uint32_t window, int32_t id) { return dialog_item_window(window, id); }
static void SC hook_remap_rect(struct fs98_flights_rect *rect, void *control) { remap_rect(rect, control); }
static int32_t TC hook_move_window(void *window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ move_window(window, x, y, width, height, repaint); return 1; }
static int32_t TC hook_tab_init(void *tabs, void *dialog, void *font, uint32_t bitmap, void *dib, void *palette,
                                uint32_t a, uint32_t color, int32_t b, int32_t c, int32_t d, int32_t e,
                                uint32_t module, uint32_t flags)
{ return tab_init(tabs, dialog, font, bitmap, dib, palette, a, color, b, c, d, e, module, flags); }
static void TC hook_tab_add_page(void *tabs, void *page, char *caption, int32_t flag) { tab_add_page(tabs, page, caption, flag); }
static void TC hook_tab_helper_init(void *helper, void *tabs) { tab_helper_init(helper, tabs); }
static int32_t TC hook_tab_set_last_page(void *tabs, void *page) { return tab_set_last_page(tabs, page); }
static int32_t TC hook_tab_set_active(void *tabs, int32_t index, uint32_t window) { return tab_set_active(tabs, index, window); }
static int32_t TC hook_page_init(void *page, int32_t value) { return page_init(page, value); }
static int32_t SC hook_load_string_a(uint32_t instance, uint32_t id, char *buffer, int32_t size)
{ return load_string_a(instance, id, buffer, size); }
static int32_t TC hook_is_kind_of(void *object, const void *runtime_class) { return is_kind_of(object, runtime_class); }
static void SC hook_ddx_text(void *exchange, uint32_t id, void *string) { ddx_text(exchange, id, string); }
static void SC hook_ddv_max(void *exchange, void *string, int32_t count) { ddv_max_chars(exchange, string, count); }
static int32_t SC hook_message_box(const char *text, uint32_t type, uint32_t help) { return message_box(text, type, help); }
static void *TC hook_page_construct(void *object, int32_t resource, void *parent) { return page_construct(object, resource, parent); }
static void TC hook_page_destroy(void *object) { page_destroy(object); }
static void *TC hook_string_assign(void *string, const char *text) { return string_assign(string, text); }
static int hook_sprintf(char *out, const char *format, ...)
{
    va_list args;
    double value;
    va_start(args, format);
    value = va_arg(args, double);
    va_end(args);
    return format_number(out, format, value);
}

static void patch(uint32_t rva, void *target)
{
    DWORD old;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) GIVE_UP();
    *slot = target;
    VirtualProtect(slot, 4, old, &old);
}
static void setup_imports(void)
{
    void *vtables[FS98_FLIGHTS_CLASS_COUNT];
    uint32_t index;
    HMODULE crt = LoadLibraryA("msvcrt.dll");
    void *handler = (void *)GetProcAddress(crt, "__CxxFrameHandler");
    if (!handler) GIVE_UP();
    patch(0x134c4, handler);
    patch(0x13628, hook_send); patch(0x1363c, hook_help); patch(0x13638, hook_enable);
    patch(0x133fc, hook_ddx); patch(0x133e0, hook_set_text);
    patch(0x13378, hook_create_dialog); patch(0x134a4, hook_show_dialog);
    patch(0x135a4, hook_exchange); patch(0x13580, hook_base_init); patch(0x13584, hook_art);
    patch(0x13588, hook_button); patch(0x1358c, hook_init_text); patch(0x13594, hook_init_edit);
    patch(0x13590, hook_base_font); patch(0x13598, hook_init_list);
    patch(0x135fc, hook_allocate); patch(0x135ac, hook_ui_delete); patch(0x13600, hook_dialog_delete);
    patch(0x135b0, hook_dialog_construct); patch(0x135c8, hook_dialog_destroy);
#define PATCH_CONTROL(name, ctor, dtor) patch(ctor, hook_construct_##name); patch(dtor, hook_destroy_##name)
    PATCH_CONTROL(text, 0x135b8, 0x135d0); PATCH_CONTROL(list, 0x135bc, 0x135d4);
    PATCH_CONTROL(edit, 0x135c0, 0x135d8); PATCH_CONTROL(button, 0x135b4, 0x135cc);
    PATCH_CONTROL(art, 0x135c4, 0x135a8); PATCH_CONTROL(check, 0x13558, 0x13560);
    patch(0x133d4, hook_get_text); patch(0x13550, hook_set_checked); patch(0x1354c, hook_error_dialog);
    patch(0x13630, hook_post); patch(0x13364, hook_copy); patch(0x13360, hook_compare);
    patch(0x135f8, hook_close);
    PATCH_CONTROL(radio, 0x1353c, 0x13548); PATCH_CONTROL(slider, 0x13538, 0x13544);
    PATCH_CONTROL(group, 0x13540, 0x13534);
    patch(0x1362c, hook_get_long); patch(0x13620, hook_get_focus); patch(0x133ec, hook_enable_control);
    patch(0x1351c, hook_set_radio); patch(0x13528, hook_init_slider); patch(0x1352c, hook_slider_range);
    patch(0x13524, hook_slider_position); patch(0x13520, hook_set_slider_position);
    patch(0x13530, hook_init_group);
    PATCH_CONTROL(picture, 0x13578, 0x1357c);
    patch(0x13618, hook_init_picture); patch(0x133f8, hook_set_focus); patch(0x13610, hook_set_sims_text);
    patch(0x13604, hook_font_construct); patch(0x13614, hook_font_destroy);
    patch(0x134b8, hook_dll_onexit); patch(0x134c0, hook_onexit); patch(0x133d8, hook_paint);
    fs98_flights_set_create_state(&s_fixture.create);
    patch(0x133dc, hook_string_construct); patch(0x133e8, hook_string_destroy);
    patch(0x133e4, hook_list_text); patch(0x133f0, hook_dialog_item); patch(0x13640, hook_dialog_item_window);
    patch(0x1359c, hook_remap_rect); patch(0x133f4, hook_move_window);
    PATCH_CONTROL(shadow, 0x13574, 0x1360c); PATCH_CONTROL(combo, 0x1355c, 0x13564);
    patch(0x13634, hook_redraw); patch(0x13380, hook_from_handle); patch(0x13384, hook_load_string);
    patch(0x13608, hook_shadow); patch(0x133d0, hook_window_enabled); patch(0x13554, hook_init_combo);
    fs98_flights_set_opening_state(&s_fixture.opening);
    fs98_flights_set_menus_state(&s_fixture.menus); patch(0x134bc, hook_atof); patch(0x134cc, hook_sprintf);
    PATCH_CONTROL(tabs, 0x134f0, 0x134f8); PATCH_CONTROL(static, 0x1338c, 0x13388);
    patch(0x133b4, hook_is_kind_of);
    patch(0x1356c, hook_tab_init); patch(0x134e4, hook_tab_add_page); patch(0x134e0, hook_tab_helper_init);
    patch(0x134e8, hook_tab_set_last_page); patch(0x134ec, hook_tab_set_active); patch(0x13570, hook_page_init);
    patch(0x13624, hook_load_string_a);
    patch(0x13568, (void *)STATIC_VTABLE); fs98_flights_set_static_vtable((void *)STATIC_VTABLE);
    patch(0x133b0, (void *)EDIT_CLASS); fs98_flights_set_edit_class((void *)EDIT_CLASS); PATCH_CONTROL(tab_helper, 0x134f4, 0x134fc);
    patch(0x133bc, hook_ddx_text); patch(0x133c0, hook_ddv_max); patch(0x133b8, hook_message_box);
    patch(0x13504, hook_page_construct); patch(0x13508, hook_page_destroy); patch(0x133c4, hook_string_assign);
#undef PATCH_CONTROL
    patch(0x12134, (void *)RESOURCE_MODULE); fs98_flights_set_resource_module(RESOURCE_MODULE);
    patch(0x1214c, (void *)OBJECT_MODULE); fs98_flights_set_object_module(OBJECT_MODULE);
    for (index = 0; index < FS98_FLIGHTS_CLASS_COUNT; ++index)
        vtables[index] = (uint8_t *)s_retail + s_classes[index].vtable;
    fs98_flights_set_object_services(&s_object_services, vtables);
    fs98_flights_set_services(&s_services);
}

static void initialize(void)
{
    uint32_t index;
    uint8_t *object = s_fixture.object;
    for (index = 0; index < sizeof(s_fixture); ++index) ((uint8_t *)&s_fixture)[index] = (uint8_t)next_random();
    PTR(object, 0) = s_vtable;
    D32(object, 0x20) = s_seed;
    PTR(object, descriptor_offset()) = s_fixture.descriptor[0];
    s_fixture.create.guard = (s_seed >> 13) & 3;     /* bit 0: the font already built */
    if (OP.kind == FS98_FLIGHTS_AUTOPILOT) {
        static const uint32_t boxes[] = {0x15e4, 0x16ac, 0x1774, 0x183c, 0x1ae4, 0x1a1c, 0x1bac, 0x1c74, 0x1d3c, 0x1e04};
        uint32_t box;
        for (box = 0; box < 10; ++box) {     /* each box's vtable, and its state from its own seed bit */
            PTR(object, boxes[box]) = s_control_vtable;
            D32(object, boxes[box] + 0xbc) = (s_seed >> box) & 1;
        }
        if ((s_seed >> 31) == 0) D32(object, 0x1a1c + 0xbc) = 1;     /* active, three times in four */
    }
    if (OP.kind == FS98_FLIGHTS_NAVIGATION) {
        for (index = 0; index < 3; ++index) {   /* each page's own vtable: a wrong page shows */
            PTR(object, 0x2160 + index * 4) = s_fixture.pages[index];
            PTR(s_fixture.pages[index], 0) = s_vtable;
        }
        PTR(object, 0x12d8) = s_vtable;     /* the tab control: its +0xc4 takes nothing */
        D32(object, 0x2174) = s_seed;
    }
    if (OP.kind == FS98_FLIGHTS_NAV_EFIS) {
        static const uint32_t controls[] = {0x1600, 0x16c8, 0x1790, 0x1884, 0x1978, 0x1a6c};
        static const uint32_t modes[] = {2, 4, 2, 3};
        uint8_t *slice = s_fixture.descriptor[0];
        uint32_t control;
        for (control = 0; control < 6; ++control) PTR(object, controls[control]) = s_control_vtable;
        PTR(object, 0x111c) = s_fixture.whole;
        PTR(s_fixture.whole, 4) = navigation_check;
        D32(object, 0x1100) = (s_seed >> 28) != 0;          /* not yet created, one time in sixteen */
        D32(object, 0x16bc) = (s_seed >> 3) & 1;
        D32(object, 0x1784) = (s_seed >> 4) % 4;            /* the master switch on, any nonzero state */
        D32(object, 0x1a8c) = s_seed ^ 0x1a8c;
        D32(object, 0x18a4) = s_seed ^ 0x18a4;
        for (index = 0; index < 2; ++index) {
            slice = s_fixture.descriptor[index];
            D32(slice, 8) = (s_seed >> (5 + index)) & 3;     /* passed through as it is */
            D32(slice, 0x10) = (s_seed >> 7) % 3;
            D32(slice, 0x18) = (s_seed >> 8) % 5;
            D32(slice, 0x20) = (s_seed >> 9) % 5 + index;
            D32(slice, 0x28) = (s_seed >> 10) % 5;
            D32(slice, 0x30) = modes[(s_seed >> (11 + index)) % 4];
            D32(slice, 0x34) = (s_seed >> 13) & 1;
            D32(slice, 0x38) = (s_seed >> 14) & 1;
            D32(slice, 0x40) = ((s_seed >> (15 + index)) & 3) == 3 ? 2 : (s_seed >> (15 + index)) & 1;
            D32(slice, 0x44) = (s_seed >> 17) & 1;
            D32(slice, 0x48) = (s_seed >> 18) & 1;
            D32(slice, 0x4c) = (s_seed >> (19 + index)) & 1;
            PTR(slice, 0x50) = s_fixture.texts[index];
            D32(slice, 0x54) = 1 + (s_seed >> 21) % 0x20 + index;
        }
    }
    if (OP.kind == FS98_FLIGHTS_NAV_RADIOS || OP.kind == FS98_FLIGHTS_NAV_ADF) {
        static const uint32_t radio_texts[] = {0x1c, 0x34, 0xa4, 0xbc}, radio_names[] = {0x6c, 0x80, 0xf4, 0x108};
        int radios = OP.kind == FS98_FLIGHTS_NAV_RADIOS;
        uint32_t field;
        PTR(object, 0x111c) = s_fixture.whole;
        PTR(s_fixture.whole, 4) = navigation_check;
        D32(object, 0x1100) = (s_seed >> 28) != 0;
        for (index = 0; index < 2; ++index) {
            uint8_t *slice = s_fixture.descriptor[index];
            for (field = 0; field < 4; ++field)     /* which radios are fitted, each from its own bits */
                D32(slice, 4 + field * 4) = (s_seed >> (field + index)) % 4 != 0;
            if (radios) {
                static const uint32_t statuses[] = {0x18, 0x30, 0xa0, 0xb8};
                for (field = 0; field < 4; ++field) {
                    D32(slice, statuses[field]) = (s_seed >> (6 + field * 2 + index)) % 3 == 0 ? 0x1000 + field : 0;
                    PTR(slice, radio_texts[field]) = s_fixture.texts[(field + index) % 4];
                    D32(slice, radio_texts[field] + 4) = 1 + (s_seed >> (field * 3)) % 0x20 + index;
                    PTR(slice, radio_names[field]) = s_fixture.names[(field + index) & 1];
                }
                snprintf((char *)slice + 0x44, 6, "I%u", index);
                snprintf((char *)slice + 0x4a, 0x20, "Station %u", index);
                snprintf((char *)slice + 0xcc, 6, "J%u", index);
                snprintf((char *)slice + 0xd2, 0x20, "Other %u", index);
                W16(slice, 0x2c) = (s_seed >> 14) & 1;
                D32(slice, 0x90) = (s_seed >> 15) & 1;
                W16(slice, 0x94) = (s_seed >> 16) % 3 ? 7 : 0;
                D32(slice, 0x9c) = (s_seed >> 17) % 4 + index;
                W16(slice, 0xb4) = (s_seed >> (18 + index)) & 1;
            } else {
                static const uint8_t digits[] = {3, 4, 3, 5};
                D32(slice, 0x30) = (s_seed >> (6 + index)) % 3 == 0;
                PTR(slice, 0x34) = s_fixture.texts[index];
                D32(slice, 0x38) = 1 + (s_seed >> 9) % 8 + index;
                D32(slice, 0x44) = (s_seed >> (10 + index)) % 3 == 0;
                PTR(slice, 0x48) = s_fixture.texts[2 + index];
                D32(slice, 0x4c) = 1 + (s_seed >> 12) % 8 + index;
                snprintf((char *)slice + 0xc, 6, "A%u", index);
                snprintf((char *)slice + 0x12, 0x20, "Beacon %u", index);
                slice[0x58] = digits[(s_seed >> (13 + index)) % 4];
                W16(slice, 0x5a) = (s_seed >> 15) & 1;
                PTR(slice, 0x60) = s_fixture.names[index];
                PTR(slice, 0x74) = s_fixture.names[index ^ 1];
            }
        }
    }
    if (OP.kind == FS98_FLIGHTS_SELECT) {
        static const uint32_t statuses[] = {0, 0x1004, 0x1003, 0x1004};
        D32(object, 0x10dc) = statuses[(s_seed >> 25) % 4];
        PTR(object, 0x1744) = s_control_vtable;
    }
    {
        static const uint32_t shown[] = {0, 0x1f5, 0x1f6, 0x1f8, 0x1f9, 7};
        s_fixture.opening.guard = (uint8_t)((s_seed >> 24) & 7);
        static const uint32_t menu_shown[] = {0, 0xcb9, 0xc81, 0xc82, 0xc83, 0xc84, 0xc85, 7};
        s_fixture.opening.shown = shown[(s_seed >> 27) % 6];
        s_fixture.menus.flight_guard = (uint8_t)((s_seed >> 20) & 7);
        s_fixture.menus.learning_guard = (uint8_t)((s_seed >> 23) & 7);
        s_fixture.menus.flight_shown = menu_shown[(s_seed >> 8) % 8];
        s_fixture.menus.learning_shown = menu_shown[(s_seed >> 11) % 8];
        if (OP.kind == FS98_FLIGHTS_FLIGHT_MENU || OP.kind == FS98_FLIGHTS_LEARNING_MENU)
            D32(object, 0x10dc) = (s_seed >> 30) == 0;
        if (OP.kind == FS98_FLIGHTS_OPENING) {
            PTR(object, 0x10e0) = s_control_vtable;
            D32(object, 0x10dc) = (s_seed >> 30) == 0;     /* a button's callback running, one time in four */
        }
    }
    if (OP.kind == FS98_FLIGHTS_COMMS) {
        PTR(object, 0x1b1c) = PTR(object, 0x1c10) = PTR(object, 0x1d04) = PTR(object, 0x1e60) = s_control_vtable;
        D32(object, 0x1c30) = s_seed ^ 0x1c30;
        D32(object, 0x1d24) = (s_seed >> 5) % 3 ? s_seed ^ 0x1d24 : D32(object, 0x1c30);
    } else PTR(object, 0x1704) = s_control_vtable;
    for (index = 0; index < 2; ++index) {
        uint8_t *descriptor = s_fixture.descriptor[index];
        if (OP.kind == FS98_FLIGHTS_AUTOPILOT) {
            static const uint32_t text_at[] = {0x20, 0x4c, 0x7c, 0x90, 0xc4};
            uint32_t field;
            PTR(descriptor, 4) = autopilot_check;
            for (field = 0; field < 5; ++field)      /* the second descriptor's buffers are other ones */
                PTR(descriptor, text_at[field]) = s_fixture.texts[(field + index) % 4];
            D32(descriptor, 0x10) = (s_seed >> 10) & 1;
            D32(descriptor, 0x18) = (s_seed >> (11 + index)) % 5;          /* 4: no mode */
            D32(descriptor, 0x1c) = (s_seed >> 13) & 1;
            D32(descriptor, 0x24) = 1 + (s_seed >> 14) % 0x20 + index;
            snprintf((char *)descriptor + 0x30, 20, "feet %u", index);
            D32(descriptor, 0x44) = (s_seed >> 15) & 1;
            D32(descriptor, 0x48) = (s_seed >> 16) & 1;
            D32(descriptor, 0x50) = 0x30 + index;
            D32(descriptor, 0x5c) = (s_seed >> (17 + index)) & 0x3f;
            D32(descriptor, 0x64) = (s_seed >> 23) & 1;
            D32(descriptor, 0x68) = (s_seed >> 24) & 1;
            D32(descriptor, 0x6c) = (s_seed >> 25) & 1;
            D32(descriptor, 0x78) = (s_seed >> 26) & 1;
            D32(descriptor, 0x80) = 0x10 + index;
            D32(descriptor, 0x8c) = (s_seed >> 27) & 1;
            D32(descriptor, 0x94) = 0x18 - index;
            D32(descriptor, 0xa0) = (s_seed >> 28) & 1;
            D32(descriptor, 0xa4) = (s_seed >> 29) & 1;
            snprintf((char *)descriptor + 0xa8, 20, "fpm %u", index);
            D32(descriptor, 0xc0) = (s_seed >> 30) & 1;
            D32(descriptor, 0xc8) = 0x20 + index;
            D32(descriptor, 0xd8) = (s_seed >> 1) & 1;
            D32(descriptor, 0xe0) = (s_seed >> (2 + index)) % 6;           /* 5: no mode */
        } else if (OP.kind == FS98_FLIGHTS_SELECT) {
            PTR(descriptor, 8) = select_delete_callback;
            PTR(descriptor, 0xc) = select_edit;
            D32(descriptor, 0x10) = (s_seed >> (12 + index)) & 1;
            D32(descriptor, 0x14) = (s_seed >> (14 + index)) % 6;      /* 5: past the list */
            /* the second descriptor's list starts one record on, so a stale base shows */
            D32(descriptor, 0x18) = (s_scenario + index) % (6 - index);
            PTR(descriptor, 0x1c) = s_fixture.records[index];
            D32(descriptor, 0x20) = (s_seed >> (17 + index)) % 6;
        } else if (OP.kind == FS98_FLIGHTS_OPENING || OP.kind == FS98_FLIGHTS_FLIGHT_MENU ||
            OP.kind == FS98_FLIGHTS_LEARNING_MENU) {
            static uint32_t (SC *const callbacks[])(void *) = {
                opening_callback_0x8, opening_callback_0xc, opening_callback_0x10, opening_callback_0x14
            };
            uint32_t slot;
            for (slot = 0; slot < 4; ++slot)
                PTR(descriptor, 8 + slot * 4) = ((s_seed >> (16 + slot)) & 3) ? (void *)callbacks[slot] : 0;
            D32(descriptor, 0x18) = (s_seed >> 21) & 1;
            D32(descriptor, 0x1c) = 0x77;
            D32(descriptor, 0x20) = (s_seed >> (22 + index)) & 1;
        } else if (OP.kind == FS98_FLIGHTS_MODELESS) {
            D32(descriptor, 8) = (s_seed >> (9 + index)) & 1;
        } else if (OP.kind == FS98_FLIGHTS_AIRCRAFT) {
            static const int32_t counts[] = {0, 1, 3, 5, -1, (int32_t)0x80000000};
            PTR(descriptor, 8) = aircraft_details;
            PTR(descriptor, 0x10) = index ? (void *)aircraft_preview_other : (void *)aircraft_preview;
            D32(descriptor, 0x14) = (uint32_t)((int32_t)((s_seed >> (5 + index)) % 6) - 1);   /* -1..4 */
            D32(descriptor, 0x18) = (uint32_t)counts[(s_scenario + index) % 6];
            PTR(descriptor, 0x1c) = s_fixture.records;
        } else if (OP.kind == FS98_FLIGHTS_CREATE) {
            static uint32_t (SC *const callbacks[])(void *) = {
                create_callback_0x8, create_callback_0xc, create_callback_0x10, create_callback_0x14,
                create_callback_0x18
            };
            uint32_t slot;
            for (slot = 0; slot < 5; ++slot)    /* any of them may be missing */
                PTR(descriptor, 8 + slot * 4) = ((s_seed >> (17 + slot)) & 3) ? (void *)callbacks[slot] : 0;
            PTR(descriptor, 0x1c) = s_fixture.names[0];
            PTR(descriptor, 0x20) = s_fixture.names[1];
            PTR(descriptor, 0x24) = (char *)s_fixture.records[index];
            PTR(descriptor, 0x28) = (char *)s_fixture.records[index] + 0x100;
            D32(descriptor, 0x2c) = (s_seed >> (23 + index)) & 1;
        } else if (OP.kind == FS98_FLIGHTS_COMMS) {
            PTR(descriptor, 4) = comms_check;
            D32(descriptor, 0xc) = (s_scenario + index) % 4 == 0 ? 0xc : 0;
            PTR(descriptor, 0x10) = s_fixture.names[index];
            D32(descriptor, 0x14) = 1 + (s_scenario + index * 0x11) % 0x40;
            D32(descriptor, 0x2c) = (s_seed >> 11) & 1;     /* independent of the request */
            D32(descriptor, 0x34) = (s_scenario + index) % 4;
            D32(descriptor, 0x3c) = (s_scenario + index * 3) % 15;
        } else if (OP.kind == FS98_FLIGHTS_NAVIGATION) {
            /* each slice's first word (the tabs' flags), the tab to show */
            D32(descriptor, 8) = (s_seed >> (21 + index)) % 4;
            D32(descriptor, 0x10) = (s_seed >> 23) & 1;
            D32(descriptor, 0x130) = (s_seed >> (24 + index)) & 1;
            D32(descriptor, 0x1bc) = (s_seed >> 26) & 1;
        } else if (OP.kind == FS98_FLIGHTS_NAV_EFIS || OP.kind == FS98_FLIGHTS_NAV_RADIOS ||
                   OP.kind == FS98_FLIGHTS_NAV_ADF) {
            /* the slices are filled above */
        } else if (OP.kind == FS98_FLIGHTS_SAVE) {
            static const uint32_t statuses[] = {0, 0x1003, 0x1004, 0x100d, 0x1001, 0x1004};
            PTR(descriptor, 4) = save_check;
            D32(descriptor, 8) = statuses[(s_scenario + index) % 6];
            PTR(descriptor, 0xc) = s_fixture.names[0];
            D32(descriptor, 0x10) = 0x10 + (s_scenario + index * 0x35) % 0x100;   /* the two differ */
            D32(descriptor, 0x1c) = s_scenario % 9 < 4 ? 0 : 0x1003;
            PTR(descriptor, 0x20) = s_fixture.names[1];
            D32(descriptor, 0x24) = 1 + (s_scenario + index * 0x13) % 0x40;
            D32(descriptor, 0x30) = s_scenario % 3;
        } else {
            D32(descriptor, 8) = s_scenario % 6;
            D32(descriptor, 0xc) = s_scenario % 7 - 1;
            PTR(descriptor, 0x10) = s_fixture.records;
        }
    }
    for (index = 0; index < 4; ++index)
        snprintf(s_fixture.texts[index], 64, "%u%02u", index, (s_seed >> index) % 100);
    snprintf(s_fixture.names[0], 64, "Flight %u vector %u", s_scenario % 2, s_scenario);
    snprintf(s_fixture.names[1], 64, "About flight %x", s_seed);
    if (OP.kind == FS98_FLIGHTS_SAVE)   /* the name already agreed to: often the one typed */
        snprintf((char *)object + 0x10dc, 0x100, "%s", (s_seed >> 3) % 3 ? s_fixture.names[0] : "Other");
    snprintf((char *)s_fixture.record_before, 64, "Before %u", s_scenario);
    snprintf((char *)s_fixture.record_before + 0x100, 64, "Before description");
    snprintf((char *)s_fixture.record_before + 0x500, 64, "Before notes %x", s_seed);
    for (index = 0; index < 5; ++index) {
        snprintf((char *)s_fixture.records[index] + 0x500, 64, "Notes %u %x", index, s_seed);
        snprintf((char *)s_fixture.records[index], 64, "Flight %u vector %u", index, s_scenario);
        snprintf((char *)s_fixture.records[index] + 0x100, 64, "Description %u seed %x", index, s_seed);
        snprintf((char *)s_fixture.records[index] + 0x900, 64, "C:\\flights\\flight%u.stn", index);
    }
    if (OP.signature == SIG_INT && OP.rva == 0x5590 && s_scenario % 3 == 0) PTR(object, 0x10dc) = 0;
    if ((OP.rva == 0x2ec0 || OP.rva == 0x52e0) && s_scenario % 3 == 0) PTR(object, 0x10dc) = 0;
    /* Select Flight's OnInitDialog alone tests for one */
    if (OP.kind == FS98_FLIGHTS_SELECT && OP.rva == 0x6960 && (s_seed >> 27) % 5 == 0)
        PTR(object, 0x180c) = 0;
    /* Select Aircraft's do too, but for populate and the preview frame */
    if (OP.kind == FS98_FLIGHTS_AIRCRAFT && OP.rva != 0x1f40 && OP.rva != 0x1c40 &&
        OP.signature < SIG_CONSTRUCT && (s_seed >> 27) % 5 == 0)
        PTR(object, 0x1864) = 0;
    /* Create's handlers test for no descriptor, all but Save and the choices */
    if (OP.kind == FS98_FLIGHTS_CREATE && OP.rva != 0x29d0 && OP.rva != 0x29f0 && OP.rva != 0x2a20 &&
        OP.rva != 0x2a50 && OP.rva != 0x2a80 && OP.signature < SIG_CONSTRUCT && (s_seed >> 27) % 5 == 0)
        PTR(object, 0x10e0) = 0;
    s_initial = s_fixture;
}
static LONG WINAPI fatal(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT scenario=%u op=%u (%s) side=%d code=%lx address=%p\n", s_scenario,
        s_operation, OP.name, s_side, exception->ExceptionRecord->ExceptionCode,
        exception->ExceptionRecord->ExceptionAddress);
    fflush(stderr); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static void land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_jump, 1);
}
/* A fault inside an operation is an outcome to compare, not the end of the run: a failed
 * allocation makes the factory store through null on both sides, and a break that faults
 * on one side only must show up as a difference. Addresses above the first page are where
 * each side's code or stack happens to be, so only low ones are logged. */
static int s_running;
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    uint32_t address;
    if (!s_running || (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
                       record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
                       record->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION))
        return EXCEPTION_CONTINUE_SEARCH;
    s_throw_at = 0;
    address = record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? (uint32_t)record->ExceptionInformation[1] : 0;
    put("fault code=%lx access=%u address=%x\n", record->ExceptionCode,
        record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? (uint32_t)record->ExceptionInformation[0] : 0,
        address < 0x10000 ? address : 0xffffffff);
    if (!s_side && OP.signature == SIG_CREATE) ++s_allocation_faults;
    exception->ContextRecord->Eip = (DWORD)land;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static EXCEPTION_DISPOSITION __cdecl catch_throw(EXCEPTION_RECORD *record, void *frame,
                                                 CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == TEST_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, 0, record, 0);
        s_throw_at = 0; put("CAUGHT\n"); if (!s_side) ++s_throws;
        land();
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) int32_t run(uint32_t throw_at)
{
    struct { void *previous; void *handler; } catcher;
    const struct fs98_flights_class *class_info = &s_classes[OP.kind];
    void *entry = s_side ? OP.ours : (uint8_t *)s_retail + OP.rva;
    void *object = s_fixture.object;
    volatile int32_t result = 0;
    s_fixture = s_initial;
    if (!s_side) memcpy((uint8_t *)s_retail + RETAIL_FONT, &s_fixture.create, sizeof(s_fixture.create));
    if (!s_side) memcpy((uint8_t *)s_retail + RETAIL_OPENING, &s_fixture.opening, sizeof(s_fixture.opening));
    if (!s_side) memcpy((uint8_t *)s_retail + RETAIL_MENUS, &s_fixture.menus, sizeof(s_fixture.menus));
    s_log_length = s_calls = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    if (OP.signature >= SIG_CONSTRUCT && !s_side) {
        uint32_t rva = OP.signature == SIG_DESTROY ? class_info->destructor :
                       OP.signature == SIG_DELETE ? class_info->delete :
                       OP.signature == SIG_CREATE ? class_info->factory : class_info->constructor;
        entry = (uint8_t *)s_retail + rva;
    }
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    s_running = 1;
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_saved_seh; catcher.handler = catch_throw;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        switch (OP.signature) {
        case SIG_EVENT:
            if (OP.rva == 0x26a0 && s_scenario % 2 == 0) object = 0;
            ((void (TC *)(void *))entry)(object);
            break;
        case SIG_HELP:
            result = ((int32_t (TC *)(void *, uint32_t, void *))entry)(object, s_seed, s_fixture.info);
            break;
        case SIG_CONTEXT:
            result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 1);
            break;
        case SIG_EXCHANGE:
            ((void (TC *)(void *, void *))entry)(object, s_fixture.exchange);
            break;
        case SIG_INT:
            result = ((int32_t (TC *)(void *))entry)(object);
            break;
        case SIG_INT_ARG: { /* the validators behave differently for their own edits' ids */
            uint32_t argument = s_scenario & 2 ? 0xcb3 : s_seed;
            if (OP.kind == FS98_FLIGHTS_AUTOPILOT) argument = (s_seed >> 7) & 1 ? s_seed : 0;
            /* the pages' reports do nothing for no failure */
            if ((OP.kind == FS98_FLIGHTS_NAV_RADIOS || OP.kind == FS98_FLIGHTS_NAV_ADF ||
                 OP.kind == FS98_FLIGHTS_NAV_EFIS) && (s_seed >> 5) % 3 == 0) argument = 0;
            if (OP.kind == FS98_FLIGHTS_SELECT) {
                static const uint32_t ids[] = {0, 0xcbe, 0x1234, 0};
                argument = ids[(s_seed >> 3) % 4];
            }
            result = ((int32_t (TC *)(void *, uint32_t))entry)(object, argument);
            break;
        }
            break;
        case SIG_INT_ARG2:
            result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 7);
            break;
        case SIG_INT_TWICE:     /* the second sees the font already built */
            result = ((int32_t (TC *)(void *))entry)(object);
            result = ((int32_t (TC *)(void *))entry)(object);
            break;
        case SIG_EXIT:
            ((void (*)(void))entry)();
            break;
        case SIG_EMPTY_ARG:
            ((void (TC *)(void *, uint32_t))entry)(object, s_seed);
            break;
        default: {
            void *returned = 0;
            if (OP.signature == SIG_CONSTRUCT || OP.signature >= SIG_ROUNDTRIP)
                returned = ((void *(TC *)(void *, void *))entry)(object, (void *)(uintptr_t)s_seed);
            else if (OP.signature == SIG_DESTROY) ((void (TC *)(void *))entry)(object);
            else if (OP.signature == SIG_DELETE) returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
            else result = ((int32_t (SC *)(uint32_t, void *))entry)(s_seed, s_fixture.descriptor[0]);
            if (returned) put("return.object %u\n", offset(returned));
            if (OP.signature >= SIG_ROUNDTRIP) {
                /* this class's construct entry, then destroy and delete after it */
                const struct operation *ours = &OP - (OP.signature - SIG_CONSTRUCT);
                if (OP.signature == SIG_ROUNDTRIP) {
                    entry = s_side ? (ours + 1)->ours : (uint8_t *)s_retail + class_info->destructor;
                    ((void (TC *)(void *))entry)(object);
                } else {
                    entry = s_side ? (ours + 2)->ours : (uint8_t *)s_retail + class_info->delete;
                    returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
                    put("return.object %u\n", offset(returned));
                }
            }
            break;
        }
        }
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    s_running = 0;
    if (!s_side) memcpy(&s_fixture.create, (uint8_t *)s_retail + RETAIL_FONT, sizeof(s_fixture.create));
    if (!s_side) memcpy(&s_fixture.opening, (uint8_t *)s_retail + RETAIL_OPENING, sizeof(s_fixture.opening));
    if (!s_side) memcpy(&s_fixture.menus, (uint8_t *)s_retail + RETAIL_MENUS, sizeof(s_fixture.menus));
    if (s_calls > s_max_calls) s_max_calls = s_calls;
    return result;
}
static int compare(uint32_t throw_at)
{
    int32_t retail, ours;
    uint32_t at;
    s_side = 0; retail = run(throw_at); s_retail_result = s_fixture;
    s_side = 1; ours = run(throw_at);
    ++s_comparisons;
    if (retail == ours && !memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) &&
        !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s) throw=%u seed=%x script=%u returns=%d/%d\n",
        s_scenario, s_operation, OP.name, throw_at, s_seed, s_script, retail, ours);
    for (at = 0; at < sizeof(s_fixture); ++at)
        if (((uint8_t *)&s_retail_result)[at] != ((uint8_t *)&s_fixture)[at]) {
            fprintf(stderr, "byte offset=%x retail=%02x ours=%02x\n", at,
                ((uint8_t *)&s_retail_result)[at], ((uint8_t *)&s_fixture)[at]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], 0, 0) : 20000;
    uint32_t initial_seed = argc > 2 ? strtoul(argv[2], 0, 0) : 0x5f1a6c2d;
    int unwind = argc > 3 && !strcmp(argv[3], "unwind");
    uint32_t throw, limit;
    if (!count || !initial_seed) return 2;
    s_rng = initial_seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal);
    AddVectoredExceptionHandler(1, expected_fault);
    s_retail = LoadLibraryExA("orig/English/Flights.DLL", 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || (uint32_t)(uintptr_t)s_retail != BASE ||
        GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x120d8)) {
        fprintf(stderr, "orig/English/Flights.DLL not mapped at its base: %p, error %lu\n",
                (void *)s_retail, GetLastError());
        return 2;
    }
    fs98_flights_set_help_file((const char *)s_retail + 0x120fc);
    setup_imports();
    {
        uint32_t slot;
        for (slot = 0; slot < 64; ++slot)
        {
            s_vtable[slot] = (void *)s_other_slots[slot];
            s_other_vtable[slot] = (void *)s_replaced_slots[slot];
        }
    }
    s_vtable[0xec / 4] = virtual_close; s_other_vtable[0xec / 4] = virtual_close_other;
    s_vtable[0xcc / 4] = virtual_accept; s_other_vtable[0xcc / 4] = virtual_accept_other;
    s_vtable[0xd0 / 4] = virtual_cancel; s_other_vtable[0xd0 / 4] = virtual_cancel_other;
    s_vtable[1] = s_other_vtable[1] = virtual_delete;
    s_vtable[0xe4 / 4] = virtual_skin;
    s_vtable[0xe8 / 4] = s_other_vtable[0xe8 / 4] = virtual_init_tab;
    s_other_vtable[0xe4 / 4] = replaced_skin;
    s_vtable[0xd8 / 4] = s_other_vtable[0xd8 / 4] = virtual_validate;
    s_vtable[0xdc / 4] = s_other_vtable[0xdc / 4] = virtual_error;
    s_control_vtable[0xc4 / 4] = virtual_check;
    s_vtable[0xe0 / 4] = s_other_vtable[0xe0 / 4] = virtual_focus;
    fs98_flights_set_number_format((const char *)s_retail + 0x12108);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        s_seed = next_random(); s_script = s_scenario & 15;
        for (s_operation = 0; s_operation < OP_COUNT; ++s_operation) {
            initialize();
            if (compare(0)) return 1;
            limit = s_calls;
            if (unwind) for (throw = 1; throw <= limit; ++throw)
                if (compare(throw)) return 1;
        }
    }
    printf("Flights events: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x swaps=%u "
           "no_selection=%u throws=%u max_calls=%u allocation_faults=%u\n",
        count, (uint32_t)OP_COUNT, s_comparisons, initial_seed, s_swaps, s_no_selection, s_throws,
        s_max_calls, s_allocation_faults);
    FreeLibrary(s_retail); return 0;
}
