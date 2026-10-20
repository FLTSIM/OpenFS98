/* MULTIP.DLL -- module id 35: multiplayer sessions and remote aircraft.
 *
 * This is the first, deliberately limited slice of the module.  It implements
 * the six small public queries at +0x24..+0x38 exactly; the session setup,
 * DirectPlay transport and per-frame work remain stubs.  MULTIP is therefore
 * intentionally not part of MODULES yet and must not be installed in-game.
 *
 * The retail module keeps its runtime state in the uninitialised tail of its
 * .data page.  Keeping a page here, with ImportTable at +0x000 and Linkage at
 * +0x058, lets the differential harness set up that state at the same offsets.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"

#ifndef FS98_STDCALL
#define FS98_STDCALL __attribute__((stdcall))
#endif

#define MULTIP_IMAGE_SIZE       0x1000
#define MULTIP_LINKAGE          0x0058
#define MULTIP_PLAYER_SIZE      0x0100

#define M_LOCAL_PLAYER          0x0a3c
#define M_DIRECTPLAY_LOADED     0x0a48
#define M_PLAYER_MODE           0x0a5c
#define M_ACTIVE_PLAYER_ID      0x0a60
#define M_CONNECTED             0x0aac
#define M_AIRCRAFT_ACTIVE       0x0ab4
#define M_PLAYER_COUNT          0x0660
#define M_PLAYERS               0x0678
#define M_PLAYERS_RESULT        0x0ef8

#define IMPORT_GLOBAL           0x000c
#define IMPORT_UTIL             0x0044

struct multip_image {
    uint8_t data[MULTIP_IMAGE_SIZE];
};

static struct multip_image s_multip_image = {{
    [0x000] = FS98_MOD_HOST,
    [0x008] = FS98_MOD_GLOBAL,
    [0x010] = FS98_MOD_ACONTAIN,
    [0x018] = FS98_MOD_CHAIN,
    [0x020] = FS98_MOD_HG2D,
    [0x028] = FS98_MOD_G3D,
    [0x030] = FS98_MOD_MAIN,
    [0x038] = FS98_MOD_NUMERIC,
    [0x040] = FS98_MOD_UTIL,
    [0x048] = FS98_MOD_WEATHERM,
    [MULTIP_LINKAGE] = FS98_MOD_MULTIP
}};

#define D       ((uint32_t)(uintptr_t)s_multip_image.data)
#define M32(o)  (*(volatile uint32_t *)(uintptr_t)(D + (o)))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

__asm__(".globl _ImportTable\n\t.set _ImportTable, _s_multip_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _s_multip_image + 0x58");

static uint32_t FS98_STDCALL multip_unimplemented(void)
{
    return 0xffffffffu;
}

/* .text+0x8620 -- local player first, then remote records in reverse order. */
static uint8_t *FS98_STDCALL multip_find_player(uint32_t player_id)
{
    uint8_t *player = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    uint8_t *players;
    uint32_t count;

    if (player != NULL &&
        *(uint32_t *)(void *)(player + 0x04) == player_id)
        return player;

    count = M32(M_PLAYER_COUNT);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    while (count != 0) {
        player = players + (--count * MULTIP_PLAYER_SIZE);
        if (*(uint32_t *)(void *)(player + 0x04) == player_id)
            return player;
    }
    return NULL;
}

/* +0x24, .text+0x9de0 -- the list consumed by DYNAMIC and MPCHAT. */
static uint32_t *multip_get_players(void)
{
    M32(M_PLAYERS_RESULT + 0x00) = M32(M_PLAYER_COUNT);
    M32(M_PLAYERS_RESULT + 0x04) = M32(M_PLAYERS);
    return (uint32_t *)(uintptr_t)(D + M_PLAYERS_RESULT);
}

/* +0x28, .text+0x8680. */
static uint8_t *FS98_STDCALL multip_get_player(uint32_t player_id)
{
    return multip_find_player(player_id);
}

/* +0x2c, .text+0x8690. */
static uint8_t *multip_get_local_player(void)
{
    return (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
}

/* .text+0xa850 -- true when a connected type-1 player has its +0x0c clear. */
static uint32_t multip_has_unready_player(void)
{
    uint8_t *players;
    uint8_t *player;
    uint32_t count;

    if (M32(M_CONNECTED) == 0)
        return 0;

    count = M32(M_PLAYER_COUNT);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    while (count != 0) {
        player = players + (--count * MULTIP_PLAYER_SIZE);
        if (*(uint32_t *)(void *)(player + 0x08) == 1 &&
            *(uint32_t *)(void *)(player + 0x0c) == 0)
            return 1;
    }
    return 0;
}

/* +0x30, .text+0xa560 -- DirectPlay present, connected, unready player. */
static uint32_t multip_get_status(void)
{
    uint32_t status = 0;

    if (M32(M_DIRECTPLAY_LOADED) != 0)
        status |= 1;
    if (M32(M_CONNECTED) != 0)
        status |= 2;
    if (multip_has_unready_player() != 0)
        status |= 4;
    return status;
}

/* +0x34, .text+0xa5b0 -- MAIN's collision result override. */
static uint32_t FS98_STDCALL multip_override_collision(int32_t *result)
{
    uint8_t *player;

    if (M32(M_CONNECTED) != 0 && M32(M_AIRCRAFT_ACTIVE) != 0 &&
        M32(M_PLAYER_MODE) == 1) {
        player = multip_find_player(M32(M_ACTIVE_PLAYER_ID));
        if (player != NULL &&
            *(uint32_t *)(void *)(player + 0x28) != 0)
            *result = -1;
    }
    return 0;
}

typedef void (FS98_STDCALL *util_range48_fn)(const void *, const void *,
                                              int32_t, int32_t *, int16_t *,
                                              uint8_t *);

/* MSVCRT _ftol truncates the x87 value to a signed 64-bit integer. */
static int32_t multip_distance_to_tenths(uint32_t distance)
{
    static const double s_distance_factor = 0.0005396115152459395;
    static const double s_ten = 10.0;
    uint64_t input = distance;
    int64_t output;
    uint16_t control_word;
    uint16_t truncate_word;

    __asm__ volatile("fnstcw %0" : "=m"(control_word));
    truncate_word = control_word | 0x0c00;
    __asm__ volatile("fildll %1\n\t"
                     "fmull %2\n\t"
                     "fmull %3\n\t"
                     "fldcw %4\n\t"
                     "fistpll %0\n\t"
                     "fldcw %5"
                     : "=m"(output)
                     : "m"(input), "m"(s_distance_factor), "m"(s_ten),
                       "m"(truncate_word), "m"(control_word)
                     : "st", "memory");
    return (int32_t)output;
}

/* +0x38, .text+0xa5f0 -- range to the active remote aircraft. */
static uint32_t FS98_STDCALL multip_get_active_range(int32_t *result)
{
    uint8_t *player;
    uint8_t *aircraft;
    util_range48_fn range48;
    int32_t distance;
    int16_t bearing;
    uint8_t scale;

    if (M32(M_CONNECTED) == 0 || M32(M_AIRCRAFT_ACTIVE) == 0 ||
        M32(M_PLAYER_MODE) == 0)
        return 0;

    player = multip_find_player(M32(M_ACTIVE_PLAYER_ID));
    if (player == NULL)
        return 0;
    aircraft = (uint8_t *)(uintptr_t)*(uint32_t *)(void *)(player + 0x28);
    if (aircraft == NULL)
        return 0;

    range48 = *(util_range48_fn *)(uintptr_t)
        (M32(IMPORT_UTIL) + 0x64);
    range48((const void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x560),
            aircraft + 0x04, 3, &distance, &bearing, &scale);
    *result = multip_distance_to_tenths((uint32_t)distance);
    return 0;
}

static void multip_install(void)
{
    static const uint32_t s_imports[] = {
        FS98_MOD_HOST, FS98_MOD_GLOBAL, FS98_MOD_ACONTAIN, FS98_MOD_CHAIN,
        FS98_MOD_HG2D, FS98_MOD_G3D, FS98_MOD_MAIN, FS98_MOD_NUMERIC,
        FS98_MOD_UTIL, FS98_MOD_WEATHERM
    };
    uint32_t index;

    for (index = 0; index < sizeof(s_imports) / sizeof(s_imports[0]); index++)
        M32(index * 8) = s_imports[index];

    M32(MULTIP_LINKAGE + 0x00) = FS98_MOD_MULTIP;
    M32(MULTIP_LINKAGE + 0x04) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x08) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x18) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x1c) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x20) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x24) = ADDR(multip_get_players);
    M32(MULTIP_LINKAGE + 0x28) = ADDR(multip_get_player);
    M32(MULTIP_LINKAGE + 0x2c) = ADDR(multip_get_local_player);
    M32(MULTIP_LINKAGE + 0x30) = ADDR(multip_get_status);
    M32(MULTIP_LINKAGE + 0x34) = ADDR(multip_override_collision);
    M32(MULTIP_LINKAGE + 0x38) = ADDR(multip_get_active_range);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        multip_install();
    }
    return TRUE;
}
