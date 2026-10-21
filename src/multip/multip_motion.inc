/* Aircraft extrapolation, .text+0x3330..+0x3880. The x87 expressions retain
 * retail's intermediate stores and extended precision between those stores. */
typedef void (FS98_STDCALL *multip_fixed_to_double_fn)(double *, const void *);
typedef void (FS98_STDCALL *multip_double_to_fixed_fn)(const double *, void *);
typedef void (FS98_STDCALL *multip_fixed_add_fn)(void *, const void *, const void *);
#define UTIL_FN(offset, type) (*(type *)(uintptr_t)(M32(IMPORT_UTIL) + (offset)))
#define AD(p, o) (*(double *)((uint8_t *)(p) + (o)))

/* .text+0x37f0/+0x3840 -- metres/longitude conversion at the stored latitude. */
static void multip_motion_convert_longitude(double *result, const double *offset,
    const void *origin, uint32_t to_metres)
{
    uint8_t fixed[8], adjusted[8];

    UTIL_FN(0xc4, multip_double_to_fixed_fn)(offset, fixed);
    UTIL_FN(to_metres ? 0x48 : 0x44, multip_fixed_add_fn)(fixed, origin, adjusted);
    UTIL_FN(0xc8, multip_fixed_to_double_fn)(result, adjusted);
}

static void multip_motion_advance(double *position, const double *velocity)
{
    __asm__ volatile("fldl %1\n\tfmull %2\n\tfaddl %0\n\tfstpl %0"
        : "+m"(*position) : "m"(MD(0xa78)), "m"(*velocity) : "st");
}

static void multip_motion_feedback(double *velocity, const double *previous,
    double *error, const double *anchor, const double *position, const double *age)
{
    double predicted;

    __asm__ volatile(
        "fldl %6\n\tfmull %3\n\tfldl %0\n\tfsubl %3\n\t"
        "fxch %%st(1)\n\tfaddl %4\n\tfstl %1\n\tfsubl %5\n\t"
        "fxch %%st(1)\n\tfld %%st(1)\n\tfstpl %2\n\t"
        "fmull %7\n\tfaddl %0\n\tfstl %0\n\t"
        "fldl %8\n\tfmul %%st(2), %%st\n\tfadd %%st(1), %%st\n\t"
        "fstpl %0\n\tfstp %%st(0)\n\tfstp %%st(0)"
        : "+m"(*velocity), "=m"(predicted), "=m"(*error)
        : "m"(*previous), "m"(*anchor), "m"(*position), "m"(*age),
          "m"(MD(0x118)), "m"(MD(0x130)) : "st", "st(1)", "st(2)");
}

/* .text+0x33f0 -- failure retains the changed model and feedback fields. */
int32_t FS98_STDCALL multip_player_predict(void *player)
{
    uint8_t *state = *(uint8_t **)((uint8_t *)player + 0x28);
    double age, position, anchor, relative, heading, unused;
    uint64_t now = M32(0xad8), timestamp = A32(player, 0x94);
    uint32_t unsigned_heading;

    __asm__ volatile("fildll %1\n\tfildll %2\n\tfsubrp %%st, %%st(1)\n\t"
        "fmull %3\n\tfsubl %4\n\tfstpl %0"
        : "=m"(age) : "m"(now), "m"(timestamp),
          "m"(*(double *)(void *)(s_multip_rdata + 0x38)), "m"(MD(0xa80)) : "st", "st(1)");
    UTIL_FN(0xc8, multip_fixed_to_double_fn)(&position, state + 4);
    multip_motion_advance(&position, &AD(player, 0x9c));
    if (A32(player, 0x98) != 0) {
        UTIL_FN(0xc8, multip_fixed_to_double_fn)(&anchor, (uint8_t *)player + 0x54);
        multip_motion_feedback(&AD(player, 0x9c), &AD(player, 0x7c),
            &AD(player, 0xb4), &anchor, &position, &age);
    }
    state = *(uint8_t **)((uint8_t *)player + 0x28);
    UTIL_FN(0xc4, multip_double_to_fixed_fn)(&position, state + 4);
    state = *(uint8_t **)((uint8_t *)player + 0x28);
    UTIL_FN(0xc8, multip_fixed_to_double_fn)(&position, state + 0x14);
    multip_motion_advance(&position, &AD(player, 0xac));
    if (A32(player, 0x98) != 0) {
        UTIL_FN(0xc8, multip_fixed_to_double_fn)(&anchor, (uint8_t *)player + 0x64);
        multip_motion_feedback(&AD(player, 0xac), &AD(player, 0x8c),
            &AD(player, 0xc4), &anchor, &position, &age);
    }
    state = *(uint8_t **)((uint8_t *)player + 0x28);
    UTIL_FN(0xc4, multip_double_to_fixed_fn)(&position, state + 0x14);
    state = *(uint8_t **)((uint8_t *)player + 0x28);
    __asm__ volatile("fldl %1\n\tfmull %2\n\tfstpl %0"
        : "=m"(heading) : "m"(MD(0xa78)), "m"(AD(player, 0xa4)) : "st");
    unsigned_heading = A32(state, 0x10) >= 0x4000 && A32(state, 0x10) <= 0xc000;
    UTIL_FN(unsigned_heading ? 0xc0 : 0xc8, multip_fixed_to_double_fn)(&position, state + 0xc);
    multip_motion_convert_longitude(&relative, &position, (uint8_t *)player + 0x54, 1);
    __asm__ volatile("fldl %1\n\tfaddl %0\n\tfstpl %0"
        : "+m"(relative) : "m"(heading) : "st");
    if (A32(player, 0x98) != 0) {
        UTIL_FN(unsigned_heading ? 0xc0 : 0xc8, multip_fixed_to_double_fn)(
            &position, (uint8_t *)player + 0x5c);
        multip_motion_convert_longitude(&anchor, &position, (uint8_t *)player + 0x54, 1);
        multip_motion_feedback(&AD(player, 0xa4), &AD(player, 0x84),
            &AD(player, 0xbc), &anchor, &relative, &age);
    }
    multip_motion_convert_longitude(&position, &relative, (uint8_t *)player + 0x54, 0);
    state = *(uint8_t **)((uint8_t *)player + 0x28);
    UTIL_FN(unsigned_heading ? 0xbc : 0xc4, multip_double_to_fixed_fn)(&position, state + 0xc);
    state = *(uint8_t **)((uint8_t *)player + 0x28);
    UTIL_FN(0xc8, multip_fixed_to_double_fn)(&unused, state + 4);
    state = *(uint8_t **)((uint8_t *)player + 0x28);
    if (HOST_FN(0x21c, multip_copy_fn)(state + 0x1c, (uint8_t *)player + 0x6c, 12) != 0)
        return -1;
    A32(player, 0x98) = 0;
    return 0;
}

/* .text+0x3330 -- snapshot the list before ticks; render and predict use
 * separate clocks and independent first-frame flags. */
int32_t multip_players_predict(void)
{
    uint32_t count;
    uint8_t *players, *player;

    if (M32(0x984) != 0)
        return 0;
    count = M32(M_PLAYER_COUNT);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    M32(0x978) = M32(0x974);
    M32(0x974) = HOST_FN(0x38, multip_ticks_fn)();
    if (M32(0x970) == 0) {
        M32(0x970) = 1;
        M32(0x978) = M32(0x974) - 200;
    }
    while (count != 0) {
        player = players + --count * 256;
        if (A32(player, 0xc) == 0 && A32(player, 0x28) != 0 &&
            A32(player, 8) == 1 && A32(player, 0x4c) != 0 && M32(0x184) != 0)
            if (multip_player_predict(player) != 0)
                return -1;
    }
    return 0;
}

/* .text+0x2e80 and its .text+0x2e30/+0x2e50 owner. */
uint32_t FS98_STDCALL multip_selection_frame_callback(uint32_t code, uint32_t argument)
{
    int32_t result;
    (void)code;
    (void)argument;
    result = multip_players_predict();
    return (uint32_t)(result != 0 ? result : multip_selection_apply());
}

int32_t multip_selection_frame_start(void)
{
    M32(0x968) = 1;
    return CHAIN_FN(0x18, multip_chain_register_fn)(0x13, multip_selection_frame_callback) != 0 ? -1 : 0;
}

int32_t multip_selection_frame_stop(void)
{
    if (M32(0x968) != 0) {
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(0x13, multip_selection_frame_callback) != 0)
            return -1;
        M32(0x968) = 0;
    }
    return 0;
}
