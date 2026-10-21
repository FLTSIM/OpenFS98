/* View commands, .text+0x2ea0..+0x3329. */
typedef uint32_t (FS98_STDCALL *multip_confirm_fn)(uint32_t, uint32_t, uint32_t,
    uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* .text+0x3200 -- only observers can toggle following/locked view. */
int32_t multip_selection_toggle_mode(void)
{
    if (A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 8) == 2) {
        if (M32(0xa58) == 0) {
            M32(0xa58) = 1;
            *(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11dc) = 0;
        } else {
            M32(0xa58) = 0;
            *(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11dc) = 1;
        }
    }
    return 0;
}

/* .text+0x2f40 -- retail has no empty-list/no-eligible guard.
 * With no eligible entry this can run forever, or read entry zero at count 0. */
int32_t multip_selection_cycle(void)
{
    uint32_t count = M32(M_PLAYER_COUNT), index = 0, found = 0;
    uint8_t *players = (uint8_t *)(uintptr_t)M32(M_PLAYERS), *player;

    if (M32(M_PLAYER_MODE) != 0) {
        while (index < count && A32(players + index * 256, 4) != M32(M_ACTIVE_PLAYER_ID))
            index++;
    } else {
        while (index < count && A32(players + index * 256, 0xc) == 0)
            index++;
    }
    while (index <= count && found == 0) {
        index++;
        if (index >= count) index = 0;
        player = players + index * 256;
        if (A32(player, 8) == 1 || A32(player, 0xc) == 1)
            found = 1;
    }
    player = players + index * 256;
    if (A32(player, 0xc) == 0) {
        M32(M_ACTIVE_PLAYER_ID) = A32(player, 4);
        M32(M_PLAYER_MODE) = 1;
        *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 1;
        if (multip_track_show(M32(M_ACTIVE_PLAYER_ID)) != 0)
            return -1;
    } else if (multip_selection_reset() != 0)
        return -1;
    return 0;
}

/* .text+0x3250 -- host conversion publishes partial state before its calls;
 * a client retains the pending flag even if the request cannot be enqueued. */
int32_t multip_player_become_observer(void)
{
    if (A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 8) != 1 || M32(0xa64) != 0)
        return 0;
    if ((UTIL_FN(0xa4, multip_confirm_fn)(0xc0, 1, 0xf539, 0, 0, 0, 0, 0) & 0x40) == 0)
        return 0;
    if (M32(0xaa8) != 0) {
        if (M32(0x9a4) == M32(0x99c)) M32(0x99c)++;
        A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 8) = 2;
        M32(0x9a0)--;
        M32(0x9a4)++;
        if (multip_player_publish_name() != 0)
            return -1;
        if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0)
            return -1;
        if (multip_message_1004(0, A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4)) != 0)
            return -1;
    } else {
        M32(0xa64) = 1;
        if (multip_message_100a() != 0)
            return -1;
    }
    return 0;
}

/* .text+0x3020 -- position behind the selected aircraft. Two padding words
 * survive from the retail caller stack and are copied into GLOBAL. */
static int32_t __attribute__((used, noinline)) multip_selection_follow_impl(
    uint32_t latitude_padding, uint32_t longitude_padding)
{
    uint8_t *player = multip_selection_get();
    uint8_t position[36], longitude_step[8];
    double angle, latitude, longitude;
    uint64_t direction, peer_radius, own_radius, multiplier;

    if (player == NULL)
        return 0;
    if (HOST_FN(0x218, multip_memset_fn)(position + 16, 0, 8) != 0)
        return -1;
    direction = A32((void *)(uintptr_t)A32(player, 0x28), 0x24);
    peer_radius = A32(player, 0x30);
    own_radius = A32((void *)(uintptr_t)A32((void *)(uintptr_t)M32(IMPORT_GLOBAL), 0x11e0), 0xc);
    multiplier = M32(0x168);
    __asm__ volatile(
        "fildll %3\n\tfmull %7\n\tfstpl %0\n\t"
        "fildll %4\n\tfildll %5\n\tfaddp %%st, %%st(1)\n\t"
        "fldl %8\n\tfld1\n\tfpatan\n\tfmull %0\n\tfmull %9\n\t"
        "fildll %6\n\tfld %%st(1)\n\tfcos\n\tfmul %%st(3), %%st\n\t"
        "fmul %%st(1), %%st\n\tfchs\n\tfstpl %1\n\t"
        "fxch %%st(1)\n\tfsin\n\tfmul %%st(2), %%st\n\t"
        "fmul %%st(1), %%st\n\tfchs\n\tfstpl %2\n\tfstp %%st(0)\n\tfstp %%st(0)"
        : "=m"(angle), "=m"(latitude), "=m"(longitude)
        : "m"(direction), "m"(peer_radius), "m"(own_radius), "m"(multiplier),
          "m"(*(double *)(void *)(s_multip_rdata + 0x20)),
          "m"(*(double *)(void *)(s_multip_rdata + 0x28)),
          "m"(*(double *)(void *)(s_multip_rdata + 0x30)) : "st", "st(1)", "st(2)", "st(3)");
    *(uint16_t *)(void *)position = (uint16_t)latitude_padding;
    *(uint16_t *)(void *)(position + 8) = (uint16_t)longitude_padding;
    UTIL_FN(0xc4, multip_double_to_fixed_fn)(&latitude, position);
    UTIL_FN(0xc4, multip_double_to_fixed_fn)(&longitude, longitude_step);
    UTIL_FN(0x44, multip_fixed_add_fn)(longitude_step,
        (void *)(uintptr_t)(A32(player, 0x28) + 4), position + 8);
    UTIL_FN(0x74, multip_vector_add_fn)(position,
        (void *)(uintptr_t)(A32(player, 0x28) + 4), position);
    if (HOST_FN(0x218, multip_memset_fn)(position + 24, 0, 4) != 0)
        return -1;
    if (HOST_FN(0x218, multip_memset_fn)(position + 28, 0, 4) != 0)
        return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(position + 32,
        (void *)(uintptr_t)(A32(player, 0x28) + 0x24), 4) != 0)
        return -1;
    if (multip_player_reset_collision((void *)(uintptr_t)M32(M_LOCAL_PLAYER)) != 0)
        return -1;
    return HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x560),
        position, sizeof(position)) != 0 ? -1 : 0;
}

static uint32_t __attribute__((used, noinline)) multip_command_callback_impl(
    uint32_t code, uint32_t argument, uint32_t latitude_padding, uint32_t longitude_padding)
{
    (void)argument;
    switch (code - 0x1016c) {
    case 0: return (uint32_t)multip_selection_toggle_mode();
    case 1: return (uint32_t)multip_selection_cycle();
    case 2: return (uint32_t)multip_selection_follow_impl(latitude_padding, longitude_padding);
    case 3: return (uint32_t)multip_player_become_observer();
    default: return code - 0x1016c;
    }
}

#if defined(__i386__)
__attribute__((naked)) int32_t multip_selection_follow(void)
{
    __asm__ volatile("movzwl -0x1c(%esp), %edx\n\tmovzwl -0x24(%esp), %eax\n\t"
        "pushl %edx\n\tpushl %eax\n\tcall _multip_selection_follow_impl\n\taddl $8, %esp\n\tret");
}
__attribute__((naked)) uint32_t FS98_STDCALL multip_command_callback(
    uint32_t code __attribute__((unused)), uint32_t argument __attribute__((unused)))
{
    __asm__ volatile("movl %esp, %eax\n\tmovzwl -0x20(%eax), %edx\n\tpushl %edx\n\t"
        "movzwl -0x28(%eax), %edx\n\tpushl %edx\n\tpushl 8(%eax)\n\tpushl 4(%eax)\n\t"
        "call _multip_command_callback_impl\n\taddl $16, %esp\n\tret $8");
}
#else
int32_t multip_selection_follow(void) { return multip_selection_follow_impl(0, 0); }
uint32_t FS98_STDCALL multip_command_callback(uint32_t code, uint32_t argument)
{ return multip_command_callback_impl(code, argument, 0, 0); }
#endif

/* .text+0x2ea0/+0x2ec0. */
int32_t multip_commands_start(void)
{
    M32(0x96c) = 1;
    return CHAIN_FN(0x18, multip_chain_register_fn)(0xc, multip_command_callback) != 0 ? -1 : 0;
}
int32_t multip_commands_stop(void)
{
    if (M32(0x96c) != 0) {
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(0xc, multip_command_callback) != 0)
            return -1;
        M32(0x96c) = 0;
    }
    return 0;
}
