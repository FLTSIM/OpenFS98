/* Session CHAIN callbacks. Their owners are wired only after each callback
 * is recovered; these registrations do not enable the public lifecycle. */
#include "multip_frame_actions.inc"

/* .text+0x2c10 -- default and flag-only paths leave code-0x21 in eax. */
uint32_t FS98_STDCALL multip_session_frame_callback(uint32_t code, uint32_t argument)
{
    uint32_t case_index = code - 0x21;
    int32_t result;

    (void)argument;
    if (case_index >= sizeof(s_frame_actions))
        return case_index;
    switch (s_frame_actions[case_index]) {
    case 1:
        result = multip_player_refresh_local();
        if (result != 0) return (uint32_t)result;
        return (uint32_t)multip_message_100e(0);
    case 2:
        return (uint32_t)multip_player_reset_collision((void *)(uintptr_t)M32(M_LOCAL_PLAYER));
    case 3: M32(0xad4) = 1; break;
    case 4: M32(0xad4) = 0; break;
    }
    return case_index;
}

/* .text+0x2bc0 -- set the flag before registration, retaining it on failure. */
int32_t multip_session_frame_start(void)
{
    M32(0x954) = 1;
    return CHAIN_FN(0x18, multip_chain_register_fn)(0x14, multip_session_frame_callback) != 0 ? -1 : 0;
}

/* .text+0x2be0 -- repeated stop with a cleared flag makes no CHAIN call. */
int32_t multip_session_frame_stop(void)
{
    if (M32(0x954) != 0) {
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(0x14, multip_session_frame_callback) != 0)
            return -1;
        M32(0x954) = 0;
    }
    return 0;
}

typedef uint32_t (FS98_STDCALL *multip_g3d_draw_fn)(const void *, void *);
typedef void (FS98_STDCALL *multip_vector_add_fn)(void *, const void *, const void *);

/* .text+0x2d30 -- render peers in reverse order. All renderer results are
 * ignored. ACONTAIN-owned and received models use different draw paths. */
int32_t multip_players_draw(void)
{
    uint32_t count;
    uint8_t *players, *player;
    void *state;

    if (M32(0x984) != 0)
        return 0;
    count = M32(M_PLAYER_COUNT);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    M32(0x964) = M32(0x960);
    M32(0x960) = HOST_FN(0x38, multip_ticks_fn)();
    if (M32(0x95c) == 0) {
        M32(0x95c) = 1;
        M32(0x964) = M32(0x960) - 200;
    }
    while (count != 0) {
        player = players + --count * 256;
        state = *(void **)(void *)(player + 0x28);
        if (A32(player, 0xc) != 0 || state == NULL ||
            A32(player, 8) != 1 || A32(player, 0x4c) == 0 || M32(0x184) == 0)
            continue;
        if (A32(player, 0x1c) != 0)
            AC_FN(0x18, multip_aircraft_release_fn)(player + 0x10);
        else
            (*(multip_g3d_draw_fn *)(uintptr_t)(M32(0x2c) + 0x38))(
                *(const void **)(void *)(player + 0x20), state);
        if (M32(0x188) != 0) {
            state = *(void **)(void *)(player + 0x28);
            (*(multip_vector_add_fn *)(uintptr_t)(M32(IMPORT_UTIL) + 0x74))(
                player + 0xd8, (uint8_t *)state + 4, (const void *)(uintptr_t)(D + 0x100));
            (*(multip_g3d_draw_fn *)(uintptr_t)(M32(0x2c) + 0x38))(
                *(const void **)(void *)(player + 0xd0), player + 0xd4);
        }
    }
    return 0;
}

/* .text+0x2d20 */
uint32_t FS98_STDCALL multip_players_draw_callback(uint32_t code, uint32_t argument)
{
    (void)code;
    (void)argument;
    return (uint32_t)multip_players_draw();
}

/* .text+0x2cd0/+0x2cf0 */
int32_t multip_players_draw_start(void)
{
    M32(0x958) = 1;
    return CHAIN_FN(0x18, multip_chain_register_fn)(0x10, multip_players_draw_callback) != 0 ? -1 : 0;
}
int32_t multip_players_draw_stop(void)
{
    if (M32(0x958) != 0) {
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(0x10, multip_players_draw_callback) != 0)
            return -1;
        M32(0x958) = 0;
    }
    return 0;
}
