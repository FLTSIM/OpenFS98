/* sim1rdiff's operations: each retail routine and the adapter that runs ours the way its
 * retail callers use it. r holds the entry registers and, after, what retail leaves where its
 * callers read it (registers an adapter does not set keep their entry values, as retail's
 * saved ones do); arguments are the stack arguments. x87 inputs are popped with X87_POP. */
#define ADAPTER(name) static void name(struct regs *r, const uint32_t *arguments)
#define UNUSED_ARGS (void)arguments
#define AX(value) (r->eax = (r->eax & 0xffff0000u) | (uint16_t)(value))
#define CX(value) (r->ecx = (r->ecx & 0xffff0000u) | (uint16_t)(value))
/* a double argument by its bits: loading it on the x87 here could raise flags retail does not */
#define DOUBLE_AT(index) (*(const double *)(arguments + (index)))
#define X87_PUSH(value) do { long double pushed_ = (value); __asm__ volatile("fldt %0" : : "m"(pushed_)); } while (0)
#define DX(value) (r->edx = (r->edx & 0xffff0000u) | (uint16_t)(value))

ADAPTER(op_part_bits) { (void)r; UNUSED_ARGS; sim1_part_bits(); }
ADAPTER(op_part_clear) { (void)r; UNUSED_ARGS; sim1_part_clear(); }
ADAPTER(op_clear_pairs) { (void)r; UNUSED_ARGS; sim1_clear_pairs(); }
ADAPTER(op_fixed_step)
{
    uint16_t dx, value = sim1_fixed_step((uint16_t)r->eax, (uint16_t)r->edx, (uint16_t)r->ecx, &dx);
    UNUSED_ARGS;
    AX(value); CX(value); DX(dx);
}
ADAPTER(op_table32) { UNUSED_ARGS; AX(sim1_table32((const uint8_t *)(uintptr_t)r->esi, (uint16_t)r->eax)); }
ADAPTER(op_table16) { UNUSED_ARGS; AX(sim1_table16((const uint8_t *)(uintptr_t)r->esi, (uint16_t)r->eax)); }
ADAPTER(op_isqrt) { UNUSED_ARGS; r->eax = sim1_isqrt(r->eax); }
ADAPTER(op_square_term)
{
    uint32_t high, term, value = r->eax;
    UNUSED_ARGS;
    if (!value) return;
    term = sim1_square_term(value, r->ecx, &high);
    r->ebx += term;
    r->eax = term; r->ecx = 14; r->edx = high;
}
ADAPTER(op_cross_term) { (void)r; UNUSED_ARGS; sim1_cross_term(); }
#define VOID_OP(adapter, routine) ADAPTER(adapter) { (void)r; UNUSED_ARGS; routine(); }
VOID_OP(op_hold_check, sim1_hold_check)
VOID_OP(op_surfaces, sim1_surfaces)
VOID_OP(op_vector_sums, sim1_vector_sums)
VOID_OP(op_pitch_sum, sim1_pitch_sum)
VOID_OP(op_side_sum, sim1_side_sum)
VOID_OP(op_thrust_sum, sim1_thrust_sum)
VOID_OP(op_turn_pair, sim1_turn_pair)
VOID_OP(op_flag_follow, sim1_flag_follow)
ADAPTER(op_isqrt64) { UNUSED_ARGS; r->eax = sim1_isqrt64((int64_t)((uint64_t)r->edx << 32 | r->eax)); }
ADAPTER(op_isqrt_low) { UNUSED_ARGS; r->eax = sim1_isqrt64((int64_t)(uint64_t)r->eax); r->edx = 0; }
ADAPTER(op_countdown) { (void)r; sim1_countdown((uint8_t *)(uintptr_t)arguments[0], (uint8_t *)(uintptr_t)arguments[1], DOUBLE_AT(2)); }
ADAPTER(op_weighted) { r->eax = sim1_weighted((const uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_wrap) { (void)r; X87_PUSH(sim1_wrap(DOUBLE_AT(0), DOUBLE_AT(2))); }
ADAPTER(op_part_axes) { (void)r; sim1_part_axes((uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_part_rotate) { (void)r; sim1_part_rotate((uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_part_add) { (void)r; sim1_part_add((const uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_scale48) { uint64_t value = sim1_scale48(r->eax); UNUSED_ARGS; r->eax = (uint32_t)value; r->edx = (uint32_t)(value >> 32); }
ADAPTER(op_mark_part) { UNUSED_ARGS; sim1_mark_part((uint8_t *)(uintptr_t)r->edi, (const uint8_t *)(uintptr_t)r->esi); }
ADAPTER(op_mul48)
{
    uint64_t value = sim1_mul48((uint16_t)r->eax, r->edx, (uint16_t)r->ecx);
    UNUSED_ARGS;
    r->eax = (uint32_t)value; r->edx = (uint32_t)(value >> 32);
}

#define X87_POP(var) __asm__ volatile("fstpt %0" : "=m"(var))
#define GLOBAL_IN ((uint8_t *)(uintptr_t)r->edi)
#define LOGIC_FLAGS(value) do { uint32_t v_ = (value), p_ = v_ & 0xff; p_ ^= p_ >> 4; p_ ^= p_ >> 2; p_ ^= p_ >> 1; \
    r->eflags = (r->eflags & ~0x8d5u) | (v_ ? 0 : 0x40u) | (v_ >> 31 ? 0x80u : 0) | (p_ & 1 ? 0 : 4u); } while (0)
VOID_OP(op_set_7c0, sim1_set_7c0)
VOID_OP(op_mode3_zero, sim1_mode3_zero)
VOID_OP(op_ratio_968, sim1_ratio_968)
VOID_OP(op_angle_2b68, sim1_angle_2b68)
ADAPTER(op_clamp16) { UNUSED_ARGS; AX(sim1_clamp16((uint16_t)r->eax)); }
ADAPTER(op_clamp32) { UNUSED_ARGS; r->eax = sim1_clamp32(r->eax); }
ADAPTER(op_global_384) { UNUSED_ARGS; r->eax = sim1_global_384(); LOGIC_FLAGS(r->eax); }
ADAPTER(op_set_limits) { UNUSED_ARGS; sim1_set_limits(GLOBAL_IN); }
ADAPTER(op_fold) { long double a, b; UNUSED_ARGS; (void)r; X87_POP(a); X87_POP(b); X87_PUSH(sim1_fold(a, b)); }
ADAPTER(op_rudder_input) { sim1_rudder_input(arguments[0]); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }
ADAPTER(op_elevator_input) { sim1_elevator_input(arguments[0]); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }
ADAPTER(op_aileron_input) { sim1_aileron_input(arguments[0]); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }
ADAPTER(op_elevator_control) { UNUSED_ARGS; sim1_elevator_control(GLOBAL_IN); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }
ADAPTER(op_aileron_control) { UNUSED_ARGS; sim1_aileron_control(GLOBAL_IN); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }
ADAPTER(op_rudder_control) { UNUSED_ARGS; sim1_rudder_control(GLOBAL_IN); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }
ADAPTER(op_table_scaled)
{
    long double value = sim1_table_scaled((const uint8_t *)(uintptr_t)arguments[0]);
    r->esi = arguments[0]; AX(S16(0x1718));
    X87_PUSH(value);
}
ADAPTER(op_bearing) { UNUSED_ARGS; DX(sim1_bearing((int16_t)r->ebx, (int16_t)r->ecx)); }
ADAPTER(op_check_874) { UNUSED_ARGS; sim1_check_874(GLOBAL_IN); }
ADAPTER(op_band_value) { UNUSED_ARGS; X87_PUSH(sim1_band_value(GLOBAL_IN)); }
ADAPTER(op_overlimit_alert) { UNUSED_ARGS; sim1_overlimit_alert(GLOBAL_IN); }
ADAPTER(op_heading_9e6) { UNUSED_ARGS; sim1_heading_9e6(GLOBAL_IN); }
ADAPTER(op_reset_886) { UNUSED_ARGS; sim1_reset_886(GLOBAL_IN); }
ADAPTER(op_ground_attitude) { UNUSED_ARGS; sim1_ground_attitude(GLOBAL_IN); }
ADAPTER(op_height) { uint64_t value = sim1_height(GLOBAL_IN); UNUSED_ARGS; r->eax = (uint32_t)value; r->edx = (uint32_t)(value >> 32); }

ADAPTER(op_filter) { long double x; UNUSED_ARGS; X87_POP(x); X87_PUSH(sim1_filter((uint8_t *)(uintptr_t)r->esi, x)); }
VOID_OP(op_heading_hold, sim1_heading_hold)
ADAPTER(op_contact_offsets) { r->eax = sim1_contact_offsets((uint8_t *)(uintptr_t)arguments[0], arguments[1]); }
ADAPTER(op_part_limit) { (void)r; sim1_part_limit((const uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_table2d) { UNUSED_ARGS; AX(sim1_table2d((const uint8_t *)(uintptr_t)r->esi)); }
ADAPTER(op_trim_hold) { sim1_trim_hold(GLOBAL_IN, arguments[0], arguments[1]); }
ADAPTER(op_gear_model) { UNUSED_ARGS; sim1_gear_model(GLOBAL_IN); }
VOID_OP(op_stall_warning, sim1_stall_warning)
ADAPTER(op_pitch_schedule) { UNUSED_ARGS; r->eax = sim1_pitch_schedule(); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }

ADAPTER(op_contact_rise) { UNUSED_ARGS; sim1_contact_rise((uint8_t *)(uintptr_t)r->ebx); }

VOID_OP(op_power_7b8, sim1_power_7b8)
ADAPTER(op_bearing_of) { uint16_t value = sim1_bearing_of(DOUBLE_AT(0), DOUBLE_AT(2)); AX(value); DX(value); }
ADAPTER(op_turn_hold) { UNUSED_ARGS; sim1_turn_hold(GLOBAL_IN); }
ADAPTER(op_record_words) { UNUSED_ARGS; r->eax = sim1_record_words(); }
ADAPTER(op_rudder_hold) { int held = S16(0x996) != 0; UNUSED_ARGS; sim1_rudder_hold(GLOBAL_IN); if (held) r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }
ADAPTER(op_crash_count) { UNUSED_ARGS; sim1_crash_count(GLOBAL_IN); }
ADAPTER(op_part_damping) { UNUSED_ARGS; sim1_part_damping((uint8_t *)(uintptr_t)r->esi); }
ADAPTER(op_ground_stop) { UNUSED_ARGS; sim1_ground_stop(GLOBAL_IN); }

VOID_OP(op_attitude_trig, sim1_attitude_trig)
ADAPTER(op_climb_history) { UNUSED_ARGS; sim1_climb_history(GLOBAL_IN, (uint16_t)r->eax); }
ADAPTER(op_yaw_damper)
{
    UNUSED_ARGS;
    sim1_yaw_damper(GLOBAL_IN);
    if (S32(0x308 + 0x10) == 1) r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL;   /* on: the rudder input loaded it */
}
VOID_OP(op_gear_force, sim1_gear_force)
VOID_OP(op_drag_sums, sim1_drag_sums)
ADAPTER(op_station_distance) { UNUSED_ARGS; sim1_station_distance(GLOBAL_IN); }

VOID_OP(op_track, sim1_track)
ADAPTER(op_air_data) { UNUSED_ARGS; sim1_air_data(GLOBAL_IN); }
VOID_OP(op_limit_fraction, sim1_limit_fraction)
ADAPTER(op_wing_leveller)
{
    UNUSED_ARGS;
    if (sim1_wing_leveller(GLOBAL_IN)) r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL;
}
ADAPTER(op_flare_throttle) { UNUSED_ARGS; sim1_flare_throttle(GLOBAL_IN); }
ADAPTER(op_copy_contact) { (void)r; sim1_copy_contact((uint8_t *)(uintptr_t)arguments[0], (const uint8_t *)(uintptr_t)arguments[1]); }

VOID_OP(op_attitude_matrix, sim1_attitude_matrix)
ADAPTER(op_part_wind) { (void)r; sim1_part_wind((uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_annunciators) { UNUSED_ARGS; sim1_annunciators(GLOBAL_IN); }
ADAPTER(op_part_slip) { UNUSED_ARGS; sim1_part_slip((uint8_t *)(uintptr_t)r->esi); }
ADAPTER(op_yaw_rate) { UNUSED_ARGS; sim1_yaw_rate(GLOBAL_IN); }
ADAPTER(op_part_side_force) { UNUSED_ARGS; sim1_part_side_force(GLOBAL_IN, (uint8_t *)(uintptr_t)r->esi); }

VOID_OP(op_icing, sim1_icing)
/* retail's saved-ecx slot is its local for the word +0x151c, so pop ecx returns that */
ADAPTER(op_turn_climb_rates) { UNUSED_ARGS; sim1_turn_climb_rates(); r->ecx = (uint32_t)(int32_t)SI16(0x151c); }
ADAPTER(op_pitch_hold) { UNUSED_ARGS; sim1_pitch_hold(GLOBAL_IN); }
ADAPTER(op_autothrottle) { UNUSED_ARGS; sim1_autothrottle(GLOBAL_IN); }

VOID_OP(op_heading_term, sim1_heading_term)
ADAPTER(op_turbulence) { UNUSED_ARGS; sim1_turbulence(GLOBAL_IN); }
ADAPTER(op_part_timers) { (void)r; sim1_part_timers((uint8_t *)(uintptr_t)arguments[0]); }

ADAPTER(op_coupler) { UNUSED_ARGS; sim1_coupler(GLOBAL_IN); }
VOID_OP(op_structure, sim1_structure)

VOID_OP(op_engine_ratio, sim1_engine_ratio)
ADAPTER(op_governor) { UNUSED_ARGS; sim1_governor(GLOBAL_IN); }
ADAPTER(op_steering)
{
    (void)r;
    sim1_steering((uint8_t *)(uintptr_t)arguments[0], arguments[1], (uint16_t *)(uintptr_t)arguments[2], (uint16_t *)(uintptr_t)arguments[3]);
}

VOID_OP(op_gear_travel, sim1_gear_travel)
ADAPTER(op_glideslope) { UNUSED_ARGS; sim1_glideslope(GLOBAL_IN); }

ADAPTER(op_situation_record) { (void)r; sim1_situation_record((uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_table_lookup) { (void)r; X87_PUSH(sim1_table_lookup((const uint8_t *)(uintptr_t)arguments[0], DOUBLE_AT(1))); }
ADAPTER(op_glideslope_capture) { UNUSED_ARGS; sim1_glideslope_capture(GLOBAL_IN); }

ADAPTER(op_rotate_a)
{
    (void)r;
    sim1_rotate_a(DOUBLE_AT(0), DOUBLE_AT(2), DOUBLE_AT(4), (double *)(uintptr_t)arguments[6], (double *)(uintptr_t)arguments[7], (double *)(uintptr_t)arguments[8]);
}
ADAPTER(op_rotate_b)
{
    (void)r;
    sim1_rotate_b(DOUBLE_AT(0), DOUBLE_AT(2), DOUBLE_AT(4), (double *)(uintptr_t)arguments[6], (double *)(uintptr_t)arguments[7], (double *)(uintptr_t)arguments[8]);
}

VOID_OP(op_inertia, sim1_inertia)
ADAPTER(op_quaternion) { UNUSED_ARGS; sim1_quaternion(); r->eflags &= ~0x40u; }   /* add esp's ZF: clear */

VOID_OP(op_park, sim1_park)
ADAPTER(op_force_feedback) { UNUSED_ARGS; sim1_force_feedback(GLOBAL_IN); }

VOID_OP(op_record_alloc, sim1_record_alloc)
VOID_OP(op_record_save, sim1_record_save)
VOID_OP(op_engine_terms, sim1_engine_terms)
ADAPTER(op_engine_power) { (void)r; sim1_engine_power((uint8_t *)(uintptr_t)arguments[0]); }
ADAPTER(op_prop_thrust) { (void)r; X87_PUSH(sim1_prop_thrust(DOUBLE_AT(0))); }
ADAPTER(op_prop_efficiency) { (void)r; X87_PUSH(sim1_prop_efficiency(DOUBLE_AT(0))); }

VOID_OP(op_pitch_moment, sim1_pitch_moment)

ADAPTER(op_pass_setup) { (void)r; UNUSED_ARGS; sim1_pass_setup(); }
ADAPTER(op_aircraft_setup) { UNUSED_ARGS; r->eax = sim1_aircraft_setup(); }

ADAPTER(op_wheel_force) { UNUSED_ARGS; sim1_wheel_force(GLOBAL_IN, (uint8_t *)(uintptr_t)r->esi); }
ADAPTER(op_flare) { UNUSED_ARGS; sim1_flare(GLOBAL_IN); }

ADAPTER(op_track_course) { UNUSED_ARGS; sim1_track_course(GLOBAL_IN); }
ADAPTER(op_altitude_capture) { UNUSED_ARGS; sim1_altitude_capture(GLOBAL_IN); }

VOID_OP(op_yaw_moment, sim1_yaw_moment)

ADAPTER(op_carburettor) { (void)r; X87_PUSH(sim1_carburettor(DOUBLE_AT(0))); }

VOID_OP(op_rotor_mixing, sim1_rotor_mixing)

ADAPTER(op_prop_pitch) { UNUSED_ARGS; sim1_prop_pitch(GLOBAL_IN); }
ADAPTER(op_part_pass) { UNUSED_ARGS; sim1_part_pass(GLOBAL_IN, (uint8_t *)(uintptr_t)r->esi, (uint16_t)r->eax); }
ADAPTER(op_deinit) { UNUSED_ARGS; r->eax = sim1_deinit_core(); }

ADAPTER(op_rotate16) { UNUSED_ARGS; r->edi = (r->edi & 0xffff0000u) | sim1_rotate16((uint16_t)r->ecx, (uint16_t)r->edi, (uint16_t)r->esi, 0, 0); }
ADAPTER(op_situation_restore) { (void)r; sim1_situation_restore((const uint8_t *)(uintptr_t)arguments[0]); }

ADAPTER(op_situation_set) { (void)r; sim1_situation_set((const uint8_t *)(uintptr_t)arguments[0], arguments[1]); }

VOID_OP(op_roll_moment, sim1_roll_moment)

ADAPTER(op_governor_pass) { UNUSED_ARGS; sim1_governor_pass(GLOBAL_IN); }

ADAPTER(op_contacts_below) { UNUSED_ARGS; sim1_contacts_below(GLOBAL_IN); r->edi = (uint32_t)(uintptr_t)SIM1_GLOBAL; }   /* edi left GLOBAL, reloaded */

ADAPTER(op_autothrottle_modes) { UNUSED_ARGS; sim1_autothrottle_modes(GLOBAL_IN); }

VOID_OP(op_contact_terms, sim1_contact_terms)

ADAPTER(op_rate_limit) { (void)r; X87_PUSH(sim1_rate_limit(DOUBLE_AT(0), DOUBLE_AT(2), DOUBLE_AT(4), DOUBLE_AT(6))); }

VOID_OP(op_ground_bumps, sim1_ground_bumps)

VOID_OP(op_body_accelerations, sim1_body_accelerations)

VOID_OP(op_angle_follow, sim1_angle_follow)

VOID_OP(op_rotor_forces, sim1_rotor_forces)

ADAPTER(op_situation_message) { (void)r; sim1_situation_message(arguments[0], (const uint8_t *)(uintptr_t)arguments[1]); }

VOID_OP(op_situation_start, sim1_situation_start)

/* shapers: steer the fixture onto edges random snapshots rarely reach */
#define RANDOM() next_random()
static void set_double(uint32_t *words, double value) { memcpy(words, &value, 8); }
static void shape_hold(void)
{
    static const int16_t words[4] = {-1, 0, 1, 2};
    if (RANDOM() % 2) return;
    FD32(0x36c0) = 1;
    FD32(0x1624) = RANDOM() % 4 ? 2 : 3;
    FD32(0x1010) = RANDOM() % 3 ? 1 : 0;
    FG8(0x366) = (uint8_t)(RANDOM() % 2);
    FG16(0x88c) = (uint16_t)words[RANDOM() % 4]; FG16(0x924) = (uint16_t)words[RANDOM() % 4];
    FG16(0x9bc) = (uint16_t)words[RANDOM() % 4]; FG16(0xa54) = (uint16_t)words[RANDOM() % 4];
    if (RANDOM() % 3 == 0) {     /* the scaled speed exactly at the constant 1.0, or the high bit set */
        uint32_t shift = RANDOM() % 32;
        FG32(0x2b4) = 1u << shift;
        FD64(0x720) = 1.0 / (double)(1u << shift);
        FD64(0x690) = RANDOM() % 2 ? 1.0 : -1.0;
    }
}
static void shape_surfaces(void)
{
    static const int16_t words[5] = {-1, 0, 1, 0x7fff, -0x8000};
    uint32_t at;
    FG8(0x610) = RANDOM() % 2 ? 1 : (uint8_t)RANDOM();
    for (at = 0xbec; at <= 0xbf4; at += 4) if (RANDOM() % 3 == 0) FG32(at) |= 0x80000000u;
    if (RANDOM() % 2) FG16(0x11c0) = (uint16_t)words[RANDOM() % 5];
    if (RANDOM() % 2) FG16(0x11c2) = (uint16_t)words[RANDOM() % 5];
    for (at = 0x1630; at <= 0x1648; at += 8) if (RANDOM() % 4 == 0) { uint64_t snan = 0x7ff0000000000001ull + RANDOM() % 7; memcpy(s_fixture.data + at, &snan, 8); }
}
static void shape_countdown(void)
{   /* the step leaves the countdown just above its floor in 64 bits, on it in 53 */
    uint8_t *record = (uint8_t *)(uintptr_t)s_arguments[0], *owner = (uint8_t *)(uintptr_t)s_arguments[1];
    if (RANDOM() % 3) return;
    P32(record, 0x14) = 1; P32(owner, 0x84) = 1;
    PF64(record, 0) = 1.5 + 0x1p-51;
    PF64(record, 8) = 1.5;
    set_double(s_arguments + 2, 0x1p-51 - 0x1p-60);
    if (RANDOM() % 2) s_control_word = 0x037f;
}
static void shape_flag_follow(void)
{
    static const uint64_t values[4] = {0, 0x3ff0000000000000ull, 0xfff8000000000000ull, 0x4000000000000000ull};
    FD32(0x118c) = RANDOM() % 3;
    FG8(0xb6d) = (uint8_t)(RANDOM() % 3);
    memcpy(s_fixture.data + 0x11b0, &values[RANDOM() % 4], 8);
}
static void shape_wrap(void)
{   /* a + b exactly at either end, or just past */
    static const double ends[4] = {3.14159265359, -3.14159265359, 3.14159265359 + 0x1p-40, -3.14159265359 - 0x1p-40};
    if (RANDOM() % 2) return;
    set_double(s_arguments, ends[RANDOM() % 4]);
    set_double(s_arguments + 2, 0.0);
}
static void shape_scale48(void)
{   /* divisors of 0x1a3f7 + 1, the overflowing ones, the shift word */
    static const uint16_t divisors[9] = {1, 2, 4, 8, 13439, 26878, 53756, 0, 3};
    if (RANDOM() % 2) FD16(0x16f8) = divisors[RANDOM() % 9];
    if (RANDOM() % 2) FD16(0x7ea) = (uint16_t)(RANDOM() % 3 ? 0 : RANDOM());
}
static void shape_mark(void)
{
    uint8_t *part = (uint8_t *)(uintptr_t)s_entry_regs.esi;
    if (RANDOM() % 2) FD32(0x812) = RANDOM() & 0x3f;
    if (RANDOM() % 2) FG32(0x368) = 0;
    if (RANDOM() % 2) { P32(part, 0xc) = RANDOM() % 4; P32(part, 8) = RANDOM() % 4; }
}

static void shape_attitude(void)
{   /* the pitch and bank words near 0x2aab, both signs, 0x80000000; the height near 30 */
    static const uint32_t words[6] = {0x2aab0000u, 0x2aac0000u, 0xd5550000u, 0xd5540000u, 0x80000000u, 0x2aabffffu};
    if (RANDOM() % 2) FG32(0x854) = 1;
    if (RANDOM() % 2) FG32(0x578) = words[RANDOM() % 6];
    if (RANDOM() % 2) FG32(0x57c) = words[RANDOM() % 6];
    if (RANDOM() % 2) { FG32(0x20) = (RANDOM() % 64) << 8; FG32(0x574) = (FG32(0x20) >> 8) + RANDOM() % 64 - 16; }
    if (RANDOM() % 4 == 0) { FG32(0x20) = 0x80000000u | (RANDOM() % 64) << 8; FG32(0x574) = (FG32(0x20) >> 8) + RANDOM() % 64 - 16; }
}
static void shape_height(void)
{   /* near the cap and around the ground fraction's borrow, the comparison word, both branches */
    if (RANDOM() % 2) FG32(0x574) = 0x7fe8 + RANDOM() % 8;
    if (RANDOM() % 2) FG32(0x20) = RANDOM() % 2 ? RANDOM() & 0xffff : (uint32_t)-(int32_t)(RANDOM() % 0x10000);
    if (RANDOM() % 2) FG32(0x270) = RANDOM() % 2;
    if (RANDOM() % 2) { FG16(0x120e) = (uint16_t)(RANDOM() % 0x8000); FG16(0x120c) = (uint16_t)RANDOM(); }
}
static void shape_clamp(void)
{   /* the input near either bound */
    static const int32_t bounds[2] = {0x3fff, -0x3fff};
    if (RANDOM() % 2) s_entry_regs.eax = (s_entry_regs.eax & (RANDOM() % 2 ? 0xffff0000u : 0)) |
        ((uint32_t)(bounds[RANDOM() % 2] + (int32_t)(RANDOM() % 5) - 2) & (RANDOM() % 2 ? 0xffffffffu : 0xffffu));
}
static void shape_global_384(void)
{
    switch (RANDOM() % 3) {
    case 0: FG32(0x384) = 0; break;
    case 1: FG32(0x384) = (uint32_t)(uintptr_t)s_fixture.blocks[RANDOM() % BLOCKS]; break;
    default: break;
    }
}
static void shape_limit(void)
{
    static const uint16_t values[6] = {0xff, 0x100, 0x101, 0x3ff, 0x400, 0x401};
    FG32(0x854) = RANDOM() % 2;
    FG32(0x7bc) = RANDOM() % 3 ? 1 : 0;
    FG16(0xc1a) = values[RANDOM() % 6];
}
static void shape_band(void)
{   /* the speed exactly on either band edge, the band the right way round */
    if (RANDOM() % 3) return;
    FG32(0x854) = 0;
    FD64(0x748) = 1.0;
    if (FD64(0x1790) <= FD64(0x1798)) FD64(0x1790) = FD64(0x1798) + 1.0;
    FD64(0x1388) = RANDOM() % 2 ? FD64(0x1790) : FD64(0x1798);
}
static void shape_check_874(void)
{
    if (RANDOM() % 2) { memset(s_fixture.data + 0xaef, 0, 4); FG32(0x84c) = 1; }
    if (RANDOM() % 2) FD32(0x162c) = 3 + RANDOM() % 2 - 1;
    if (RANDOM() % 3 == 0) { double limit = FD64(0x1470); if (RANDOM() % 2) limit = -limit; memcpy(s_fixture.data + 0x976, &limit, 8); }
}
static void shape_heading(void)
{
    uint32_t at;
    for (at = 0xad3; at <= 0xadf; at += 4) if (RANDOM() % 2) { uint32_t zero = RANDOM() % 3 ? 0 : 1; memcpy(s_fixture.data + at, &zero, 4); }
}
static void shape_angle_2b68(void)
{   /* the integer at the limit: 0 to the constants' crossing */
    if (RANDOM() % 2) FD32(0x242c) = (uint32_t)((int32_t)(RANDOM() % 400) - 200);
}
static void shape_control(void)
{
    if (RANDOM() % 2) FD32(0x3b68) = RANDOM() % 2;
}

static void shape_filter(void)
{   /* x on the limits */
    uint8_t *record = (uint8_t *)(uintptr_t)s_entry_regs.esi;
    if (RANDOM() % 3 == 0) PF64(record, 0x20 + (RANDOM() % 3) * 8) = fabsl(s_x87_in[0]) == 0 ? 0 : (double)fabsl(s_x87_in[0]);
}
static void shape_heading_hold(void)
{
    if (RANDOM() % 2) FD16(0x996) = 1;
}
static void shape_contact(void)
{
    uint8_t *contact = (uint8_t *)(uintptr_t)s_arguments[0];
    if (RANDOM() % 2) { P32(contact, 0x24) = 0; P32(contact, 0x28) = 0; P32(contact, 0x2c) = RANDOM() % 3 ? 0 : 1u << RANDOM() % 32; }
}
static void shape_part_limit(void)
{
    uint8_t *part = (uint8_t *)(uintptr_t)s_arguments[0];
    if (RANDOM() % 4 == 0) return;
    P32(part, 4) = RANDOM() % 4 ? 1 : 2; P32(part, 8) = RANDOM() % 4 ? 1 : 0; P32(part, 0xc) = RANDOM() % 4 ? 0 : 1;
    P32(part, 0x20) = RANDOM() % 3 ? RANDOM() % 0x10000 : RANDOM();
    FG16(0x832) = (uint16_t)(RANDOM() % 2 ? 0 : 1 + RANDOM() % 12);
}
static void shape_trim(void)
{
    if (RANDOM() % 2) { uint32_t zero = 0; memcpy(s_fixture.data + 0x656, &zero, 4); }
    if (RANDOM() % 2) { uint32_t zero = 0; memcpy(s_fixture.data + 0x65a, &zero, 4); }
    s_arguments[1] = RANDOM() % 3;
    if (RANDOM() % 2) s_arguments[0] = 0;
}
static void shape_gear(void)
{
    static const uint8_t states[6] = {1, 5, 6, 7, 8, 0x80};
    FG8(0x838) = states[RANDOM() % 6];
    if (RANDOM() % 2) FG16(0x846) = 0;
    if (RANDOM() % 2) FG32(0x82c) = 0;
}
static void shape_stall(void)
{   /* both signs of the level, odd ones too */
    if (RANDOM() % 2) FG32(0xbe0) = RANDOM() % 2;
    if (RANDOM() % 2) FD64(0x1278) = -FD64(0x1278);
    if (RANDOM() % 3 == 0) {     /* a negative odd level (truncated, not floored, by 2) on the low threshold */
        int32_t odd = 2 * (int32_t)(RANDOM() % 20) + 1, level = -3 * odd;
        FD32(0x1728) = 1; FD64(0x788) = 1.0; FG32(0xbe0) = 1;
        FD64(0x1278) = -1.0 - ((double)odd + 0.5) / 624.0;
        FD16(0x363c) = (uint16_t)(int16_t)(level / 2);
    }
}
static void shape_schedule(void)
{
    static const int16_t modes[3] = {4, 0, 3};
    FG16(0x892) = (uint16_t)modes[RANDOM() % 3];
    if (RANDOM() % 2) FD32(0x91c) = RANDOM() % 2 ? 4 : 0;
    if (RANDOM() % 2) FD32(0x910) = RANDOM() % 14;
    if (RANDOM() % 2) FD32(0x918) = RANDOM() % 3;
    if (RANDOM() % 2) FD16(0x16d0) = (uint16_t)(0x18e + RANDOM() % 5);
}

static void shape_rise(void)
{
    uint8_t *contact = (uint8_t *)(uintptr_t)s_entry_regs.ebx;
    if (RANDOM() % 2) P32(contact, 0x1c) = (uint32_t)((int32_t)(RANDOM() % 2000) - 500);
}

static void shape_holds(void)
{
    if (RANDOM() % 2) FD16(0x996) = (uint16_t)(RANDOM() % 2);
    if (RANDOM() % 2) FG32(0x854) = RANDOM() % 2;
    if (RANDOM() % 2) { FD8(0xa28) = (uint8_t)(RANDOM() % 2); FD8(0x9ae) = (uint8_t)(RANDOM() % 2); }
}
static void shape_crash(void)
{
    FD32(0x1624) = RANDOM() % 3 ? 2 : 1;
    if (RANDOM() % 2) FD32(0x1618) = RANDOM() % 2;
    if (RANDOM() % 2) FD32(0x2224) = RANDOM() % 2;
    FG32(0x348) = 3 + RANDOM() % 4;
    if (RANDOM() % 2) FG16(0xbc8) = (uint16_t)(RANDOM() % 2);
    if (RANDOM() % 2) FD16(0x7d6) = (uint16_t)(0x12 + RANDOM() % 3);
    if (RANDOM() % 3 == 0) { double limit = FD64(0x13b0); memcpy(s_fixture.data + 0x7dc, &limit, 8); }
}
static void shape_damping(void)
{
    uint8_t *part = (uint8_t *)(uintptr_t)s_entry_regs.esi;
    uint32_t base = (uint32_t)(((uint64_t)0x55f0 * FD32(0x1724)) >> 16) + 0x6180;
    if (RANDOM() % 2) P32(part, 0x10) = (uint32_t)-(int32_t)(RANDOM() % 0x10000);
    if (RANDOM() % 3 == 0) P32(part, 0x10) = 0u - base + RANDOM() % 3 - 1;     /* the sum at zero */
}
static void shape_gear_force(void)
{   /* compressed: every input a value of its own */
    uint32_t at;
    if (RANDOM() % 2) for (at = 0x10b8; at <= 0x1108; at += 8) FD64(at) = (double)((int32_t)(RANDOM() % 2000) - 1000) / 64.0;
}
static void shape_power(void)
{
    if (RANDOM() % 2) FD64(0x1458) = (double)(RANDOM() % 4000) / 37.0;
    if (RANDOM() % 2) s_control_word = 0x037f;
}
static void shape_ground_stop(void)
{   /* the scaled speed around +-0x200 in 16.16 */
    static const int32_t values[8] = {0x200, 0x201, -0x200, -0x201, 0, 0x1ffff, -0x10000, 0x10000};
    FG32(0x854) = RANDOM() % 4 ? 1 : 0;
    FD32(0x162c) = 2 + RANDOM() % 3;
    if (RANDOM() % 2) { FD64(0x728) = 1.0; FD64(0x6d8) = 1.0; FD64(0x1470) = values[RANDOM() % 8]; }
}

static void shape_yaw(void)
{
    if (RANDOM() % 2) FD16(0x996) = 0;
    if (RANDOM() % 2) FG8(0x366) = 0;
    if (RANDOM() % 2) FG32(0x854) = RANDOM() % 2;
    if (RANDOM() % 2) FG8(0x36d) = (uint8_t)(RANDOM() % 2);
    if (RANDOM() % 3 == 0) FD16(0x308 + 0x14) = (uint16_t)(RANDOM() % 2 ? 0x7ff0 + RANDOM() % 16 : 0x8000 + RANDOM() % 16);
}
static void shape_climb(void)
{
    if (RANDOM() % 4 == 0) { uint32_t zero = RANDOM() % 2 ? 0 : 1; memcpy(s_fixture.data + 0x16fc, &zero, 4); }
}
static void shape_station(void)
{
    switch (RANDOM() % 3) {
    case 0: FG32(0x384) = 0; break;
    case 1: {
        uint8_t *outer = s_fixture.blocks[RANDOM() % BLOCKS], *inner = s_fixture.blocks[RANDOM() % BLOCKS];
        P32(outer, 0) = RANDOM() % 3 ? (uint32_t)(uintptr_t)inner : 0;
        FG32(0x384) = (uint32_t)(uintptr_t)outer;
        break;
    }
    default: break;
    }
}

static void shape_leveller(void)
{
    if (RANDOM() % 2) FG32(0x854) = 0;
    if (RANDOM() % 2) { FD8(0xa24) = 0; FD8(0xa28) = 0; FD8(0xa2a) = (uint8_t)(RANDOM() % 2); FD8(0x9ae) = 0; }
}
static void shape_flare(void)
{
    uint32_t one = 1, value;
    if (RANDOM() % 4) FD32(0x4040) = 1;
    if (RANDOM() % 4) memcpy(s_fixture.data + 0xaff, &one, 4);
    if (RANDOM() % 2) { value = FD32(0x1d18) + RANDOM() % 3 - 1; memcpy(s_fixture.data + 0xafb, &value, 4); }
    if (RANDOM() % 2) FD32(0x1624) = RANDOM() % 4;
    if (RANDOM() % 2) { value = RANDOM() % 2; memcpy(s_fixture.data + 0xb03, &value, 4); }
    {   /* the speed at each of the three thresholds (+0xb0b, +0xb0f, +0xb13) */
        static const uint16_t thresholds[3] = {0xb0b, 0xb0f, 0xb13};
        uint32_t at;
        for (at = 0; at < 3; ++at)
            if (RANDOM() % 2) { value = FG32(0x2bc) + RANDOM() % 3 - 1; memcpy(s_fixture.data + thresholds[at], &value, 4); }
    }
    FG16(0xaec) = (uint16_t)(1 + RANDOM() % 4);
    if (RANDOM() % 2) FG16(0xb00) = (uint16_t)(RANDOM() % 0x4000);
}
static void shape_fraction(void)
{   /* the angle at, between and past the limits, which way round */
    if (RANDOM() % 2) { FD64(0x680) = 1.0; FD16(0x3802) = (uint16_t)(int16_t)((int32_t)(RANDOM() % 9) - 4);
                        FD64(0x14c8) = (double)((int32_t)(RANDOM() % 9) - 4); FD64(0x14d0) = (double)((int32_t)(RANDOM() % 9) - 4); }
}

static void shape_annunciators(void)
{
    static const uint16_t switches[10] = {0xb71, 0xb70, 0xb72, 0xb6d, 0xb67, 0x7d0, 0x7c0, 0x7c8, 0x7d8, 0x7c4};
    uint32_t at;
    if (RANDOM() % 4) FG32(0x854) = 0;
    if (RANDOM() % 4) FG32(0x7bc) = 1;
    for (at = 0; at < 10; ++at) if (RANDOM() % 2) {
        if (switches[at] >= 0xb00) FG8(switches[at]) = (uint8_t)(RANDOM() % 2); else FG32(switches[at]) = RANDOM() % 2;
    }
    if (RANDOM() % 2) {
        uint8_t *outer = s_fixture.blocks[RANDOM() % BLOCKS], *station = s_fixture.blocks[RANDOM() % BLOCKS];
        P32(outer, 0) = (uint32_t)(uintptr_t)station;
        P16(station, 0x18) = (uint16_t)(RANDOM() % 3);
        P8(station, 0x1c) = (uint8_t)RANDOM();
        FG32(0x384) = (uint32_t)(uintptr_t)outer;
        FG32(0x7fc) = RANDOM() % 2 ? 0 : RANDOM() % 2 ? 0x101 : 0x100; FG32(0x800) = RANDOM() % 2; FG32(0x804) = RANDOM() % 2;
        FD8(0xa27) = (uint8_t)(RANDOM() % 2); FG8(0xc4a) = (uint8_t)(RANDOM() % 2);
    }
}
static void shape_slip(void)
{
    uint8_t *part = (uint8_t *)(uintptr_t)s_entry_regs.esi;
    if (RANDOM() % 2) { PF64(part, 0x58) = (double)((int32_t)(RANDOM() % 400) - 200) / 65536.0; FD64(0x728) = 65536.0; FD64(0x6d8) = 1.0; }
}
static void shape_yaw_rate(void)
{
    uint32_t previous = (uint32_t)((int32_t)(RANDOM() % 300) - 150);
    memcpy(s_fixture.data + 0x8b6, &previous, 4);
    if (RANDOM() % 2) FG32(0x278) = RANDOM() % 2;
}
static void shape_side_force(void)
{
    uint8_t *part = (uint8_t *)(uintptr_t)s_entry_regs.esi;
    if (RANDOM() % 2) P16(part, 0x14) = 0;
    if (RANDOM() % 2) FG32(0x60c) = RANDOM() % 3;
}

static void shape_icing(void)
{
    if (RANDOM() % 2) FG16(0xe8c) = (uint16_t)(int16_t)((int32_t)(RANDOM() % 80) - 40);
    if (RANDOM() % 2) { FG16(0xf10) = (uint16_t)(RANDOM() % 2); FG16(0xf0e) = (uint16_t)(RANDOM() % 3); }
    if (RANDOM() % 2) FD64(0x1760) = (double)((int32_t)(RANDOM() % 12) - 2) / 2.0;
    if (RANDOM() % 2) FG16(0xc44) = (uint16_t)(RANDOM() % 0x200);
}
static void shape_pitch_hold(void)
{
    if (RANDOM() % 2) { FG32(0x854) = 0; FD16(0x996) = 0; FD8(0xa22) = 0; FD8(0xa24) = 0; }
    if (RANDOM() % 2) { FG32(0x7fc) = RANDOM() % 2; FD8(0xa26) = (uint8_t)(RANDOM() % 2); FD8(0xa2b) = (uint8_t)(RANDOM() % 2); }
}
static void shape_autothrottle(void)
{
    if (RANDOM() % 2) { FG32(0x854) = 1; FD16(0x996) = 0; FD32(0x1624) = RANDOM() % 4; }
    if (RANDOM() % 2) FG16(0x878) = RANDOM() % 4 ? (uint16_t)(RANDOM() % 0x400) : 0xffff;
    if (RANDOM() % 2) FG16(0x87a) = RANDOM() % 3 ? (uint16_t)(RANDOM() % 4) : 0xffff;
    if (RANDOM() % 2) FG32(0xbdc) = RANDOM() % 4;
    if (RANDOM() % 2) { FG32(0x60c) = 1; FG16(0x87c) = (uint16_t)(RANDOM() % 3); FG32(0xbe8) = RANDOM() % 3; FG8(0x366) = (uint8_t)(RANDOM() % 2); }
    if (RANDOM() % 3 == 0) FG16(0x88c) = (uint16_t)(RANDOM() % 2 ? 0x3ff0 + RANDOM() % 32 : RANDOM() % 16);
}

static void shape_timers(void)
{
    uint8_t *owner = (uint8_t *)(uintptr_t)s_arguments[0];
    if (RANDOM() % 2) FG16(0xc1a) = (uint16_t)(RANDOM() % 2 ? 0x3ff + RANDOM() % 3 : RANDOM());
    if (RANDOM() % 2) { P32(owner, 0x88) = 1 + RANDOM() % 100; PF64(owner, 0x8c) = (double)(RANDOM() % 1000) / 10.0; P32(owner, 0x84) = RANDOM(); }
}
static void shape_turbulence(void)
{
    if (RANDOM() % 2) FG32(0x854) = 0;
    if (RANDOM() % 2) FG8(0x1168) = (uint8_t)(0xf0 + RANDOM() % 16);
}

static void shape_coupler(void)
{
    if (RANDOM() % 4) { FD8(0xa23) = (uint8_t)(RANDOM() % 2); FD8(0xa25) = (uint8_t)(RANDOM() % 2); FD8(0xa29) = (uint8_t)(RANDOM() % 2); }
    if (RANDOM() % 2) FD16(0x9ae) = (uint16_t)(RANDOM() % 2);
    if (RANDOM() % 2) { uint16_t band = (uint16_t)(RANDOM() % 0x300); memcpy(s_fixture.data + 0xa93, &band, 2); }
    if (RANDOM() % 2) {     /* MAIN's read-out (the block word its model may answer) at the band */
        uint16_t band = (uint16_t)(RANDOM() % 0x300), read = (uint16_t)(RANDOM() % 2 ? band : -band);
        memcpy(s_fixture.data + 0xa93, &band, 2);
        memcpy(s_fixture.blocks[0], &read, 2);
    }
    if (RANDOM() % 3 == 0) {     /* no airspeed: the climb history's band is 0; a 0 read-out sits on it */
        uint16_t zero = 0;
        FG32(0x2bc) = 0; FD16(0x9ae) = 0; FD8(0xa23) = 1;
        memcpy(s_fixture.blocks[0], &zero, 2);
    }
}
static void shape_structure(void)
{
    if (RANDOM() % 2) { FD32(0x162c) = RANDOM() % 3; FG16(0x830) = (uint16_t)(RANDOM() % 2); }
    if (RANDOM() % 2) { FG16(0x832) = 0; FG8(0x838) = 0; }
    if (RANDOM() % 2) FG16(0x338) = (uint16_t)(RANDOM() % 2);
    if (RANDOM() % 2) FG32(0xbe0) = RANDOM() % 2;
    if (RANDOM() % 2) { FD32(0x2510) = 0xbb7e88fu + RANDOM() % 3; }
    if (RANDOM() % 2) { FD32(0x1624) = RANDOM() % 3; FD32(0x1618) = RANDOM() % 2; FG8(0x11ec) = (uint8_t)(RANDOM() % 3); FG32(0xbec) = RANDOM() % 2; }
    if (RANDOM() % 2) FD32(0x18e8) = RANDOM() % 0x20000;
}

static void shape_engine_ratio(void)
{
    if (RANDOM() % 2) FG8(0xb73) = (uint8_t)(RANDOM() % 4 == 0);
    if (RANDOM() % 3 == 0) FD16(0x2b54) = (uint16_t)(RANDOM() % 3);
}
static void shape_governor(void)
{
    uint32_t value;
    if (RANDOM() % 2) { value = RANDOM() % 2; memcpy(s_fixture.data + 0xacf, &value, 4); }
    if (RANDOM() % 2) { value = 0; memcpy(s_fixture.data + 0xad3, &value, 4); memcpy(s_fixture.data + 0xadb, &value, 4); FD8(0xa21) = (uint8_t)(RANDOM() % 2); }
    if (RANDOM() % 2) { value = RANDOM() % 0x800; memcpy(s_fixture.data + 0xac7, &value, 4); }
    FG16(0xaec) = (uint16_t)(1 + RANDOM() % 4);
    if (RANDOM() % 3 == 0) {     /* a large negative correction onto small throttles */
        uint32_t target = (uint32_t)-(int32_t)(0x1000 + RANDOM() % 0x10000), on = 1, at;
        memcpy(s_fixture.data + 0xacf, &on, 4); memcpy(s_fixture.data + 0xacb, &target, 4);
        value = 0; memcpy(s_fixture.data + 0xac7, &value, 4);
        for (at = 0; at < 4; ++at) FG16(0x88c + at * 0x98) = (uint16_t)(RANDOM() % 8);
    }
}
static void shape_steering(void)
{
    uint8_t *part = (uint8_t *)(uintptr_t)s_arguments[0];
    s_arguments[1] = RANDOM() % 4 == 0;
    if (RANDOM() % 4) FD32(0x23d0) = 1;
    if (RANDOM() % 2) FG32(0x278) = RANDOM() % 2;
    if (RANDOM() % 2) PF64(part, 0x58) = (double)((int32_t)(RANDOM() % 200) - 50) / 4.0;
    if (RANDOM() % 3 == 0) { FD64(0x728) = 1.0; FD64(0x6d8) = 1.0; PF64(part, 0x58) = RF64(0x15218); }   /* slow, exactly */
}

static void shape_gear_travel(void)
{
    static const uint16_t parts[6] = {0x23e0, 0x26c0, 0x1d00, 0x1de0, 0x2580, 0x2aa0};
    uint32_t at;
    FG8(0x364) = (uint8_t)(RANDOM() & 0xc);
    for (at = 0; at < 6; ++at) if (RANDOM() % 2) { FD64(parts[at]) = (double)(RANDOM() % 8) / 4.0; FD64(parts[at] + 8) = (double)(RANDOM() % 8) / 4.0; }
    if (RANDOM() % 3 == 0) {     /* the first's floor at its travel rounded, under 64-bit precision */
        long double travel;
        FD64(0x23e0) = 1.0 + (double)(RANDOM() % 1000) / 3.0;
        travel = (long double)FD64(0x23e0) + (long double)FD64(0x16f0) * RF64(0x150e8);
        FD64(0x23e8) = (double)travel;
        FG8(0x364) |= 4;
        s_control_word = 0x037f;
    }
}
/* a zero angle (sine 0, cosine 0x7fff) and no height: the deviation is about -+0x9c2, so it
 * can be put on the window's and the caps' edges */
static void shape_deviation(int32_t target)
{
    uint32_t one = 1, base = (uint32_t)-target;
    FG16(0x872) = 0; FG32(0x86c) = 0;
    memcpy(s_fixture.data + 0x9f4, &one, 4);
    memcpy(s_fixture.data + 0x9c2, &base, 4);
}
static void shape_glideslope(void)
{
    uint16_t word;
    if (RANDOM() % 3 == 0) { static const int32_t edges[6] = {-0x19, -0x1a, 0x19, 0x1a, 0x38e, -0x38e}; shape_deviation(edges[RANDOM() % 6]); }
    if (RANDOM() % 3) FD8(0xa26) = 1;
    if (RANDOM() % 2) { word = (uint16_t)(RANDOM() % 2); memcpy(s_fixture.data + 0xa2b, &word, 2); }
    if (RANDOM() % 2) { uint32_t distance = RANDOM() % 0x10000; memcpy(s_fixture.data + 0x9f4, &distance, 4); }
}

static void shape_record(void)
{
    if (RANDOM() % 3 == 0) { uint64_t snan = 0x7ff0000000000001ull + RANDOM() % 5; memcpy(s_fixture.data + 0x1538, &snan, 8); }
}
static void shape_table(void)
{   /* a table of 0 to 9 ascending x (some equal), and x on, between and past them */
    uint8_t *table = (uint8_t *)(uintptr_t)s_arguments[0];
    uint32_t count = RANDOM() % 10, at;
    double x = 0, value;
    P32(table, 0) = count;
    for (at = 0; at < count; ++at) {
        x += (double)(RANDOM() % 4);
        PF64(table, 4 + at * 16) = x;
        PF64(table, 0xc + at * 16) = (double)((int32_t)(RANDOM() % 200) - 100) / 7.0;
    }
    value = count && RANDOM() % 2 ? PF64(table, 4 + (RANDOM() % count) * 16) : (double)((int32_t)(RANDOM() % 50) - 10) / 3.0;
    if (RANDOM() % 3 == 0) value += 0.5;
    set_double(s_arguments + 1, value);
}
static void shape_capture(void)
{
    if (RANDOM() % 3 == 0) { static const int32_t edges[4] = {0x38e, 0x38f, -0x38e, -0x38f}; shape_deviation(edges[RANDOM() % 4]); FD16(0x9de) = 7; FD16(0x9e0) = 9; }
    if (RANDOM() % 2) { uint32_t distance = RANDOM() % 0x10000; memcpy(s_fixture.data + 0x9f4, &distance, 4); }
    if (RANDOM() % 2) { FG16(0x88c) = 0; FG32(0x2bc) = FD32(0x3d04) - 0xa00 + RANDOM() % 0x1400 - 0xa00; }
}

static void shape_rotate(void)
{
    FD32(0x7c0) = RANDOM() % 2;
}

static void shape_park(void)
{   /* slow and still: each velocity below its floor, or one not */
    uint32_t at;
    if (RANDOM() % 4 == 0) return;
    FD32(0x162c) = 2 + RANDOM() % 3;
    FG32(0x2b4) = RANDOM() % 4;
    for (at = 0; at < 9; ++at) {
        static const uint16_t vectors[3] = {0x1358, 0x1418, 0x1328};
        if (RANDOM() % 3) FD64(vectors[at / 3] + (at % 3) * 8) = (double)((int32_t)(RANDOM() % 9) - 4) / 1024.0;
    }
}
static void shape_feedback(void)
{
    if (RANDOM() % 2) FG32(0x604) = RANDOM() % 2;
}

static void shape_engine_power(void)
{
    FG8(0x609) = (uint8_t)(RANDOM() % 3);
}
static void shape_alloc(void)
{
    if (RANDOM() % 2) FG32(0x11d8) = 0;
    else FG32(0x11d8) = (uint32_t)(uintptr_t)s_fixture.blocks[RANDOM() % BLOCKS];
}

static void shape_setup(void)
{
    if (RANDOM() % 2) FG32(0xbe8) = RANDOM() % 2 ? 0x3fff : 0;
    if (RANDOM() % 2) { FG32(0x120c) = RANDOM(); FG16(0x572) = (uint16_t)(0xff00 + RANDOM() % 0x100); }
}

static void shape_wheel(void)
{
    if (RANDOM() % 4 == 0) {     /* the parked wheel, its brakes even in mode 2 */
        s_entry_regs.esi = BASE + DATA_RVA + 0x2200;
        if (RANDOM() % 2) { FD16(0x7fc) = 1; FG16(0xbc6) = FG16(0xbc4); FD32(0x1624) = 2; return; }
    }
    if (RANDOM() % 2) { FD16(0x7fc) = (uint16_t)(RANDOM() % 2); FG16(0xbc6) = FG16(0xbc4); FD32(0x1624) = RANDOM() % 3; }
    if (RANDOM() % 2) { uint32_t count = RANDOM() % 6; memcpy(s_fixture.data + 0x812, &count, 4); FG32(0x60c) = RANDOM() % 3; }
    if (RANDOM() % 2) { FG8(0x11ec) = (uint8_t)(RANDOM() % 2); FG32(0x790) = RANDOM() % 2; FG32(0x79c) = RANDOM() % 2; }
}
static void shape_flare_land(void)
{
    if (RANDOM() % 2) { FD32(0x1d18) = FG32(0x120c) + RANDOM() % 0x20000; FG16(0x872) = (uint16_t)-(int16_t)(1 + RANDOM() % 100); }
    if (RANDOM() % 2) FD16(0x996) = (uint16_t)(RANDOM() % 2);
    if (RANDOM() % 2) { FG32(0x874) = 0; FG32(0x854) = 1; }
    if (RANDOM() % 2) FD32(0x1624) = 2;
    if (RANDOM() % 3 == 0) FG16(0x88c) = (uint16_t)(RANDOM() % 2 ? 0x8000 : 0x7fff);
    if (RANDOM() % 3 == 0) FD16(0xa18) = (uint16_t)(0x7ff0 + RANDOM() % 32);
}

static void shape_course(void)
{
    if (RANDOM() % 3 == 0) {     /* no offsets: the bearing is UTIL's atan2(0, 0) turned, so the error can sit on +-0x38e */
        static const int16_t edges[4] = {0x38e, 0x38d, -0x38e, -0x38d};
        uint32_t zero = 0;
        uint16_t bearing;
        memcpy(s_fixture.data + 0x97e, &zero, 4); memcpy(s_fixture.data + 0x982, &zero, 4);
        bearing = (uint16_t)(0x4000 - (uint16_t)((int32_t (__stdcall *)(int32_t, int32_t))s_util[0x3c / 4])(0, 0));
        FG16(0x870) = (uint16_t)(bearing - 0x8000 - (uint16_t)edges[RANDOM() % 4]);
        FD16(0x996) = 0;
    }
    if (RANDOM() % 2) FD16(0x996) = 0;
    if (RANDOM() % 2) FD16(0x9aa) = (uint16_t)(RANDOM() % 2);
    if (RANDOM() % 2) FD16(0x9ae) = (uint16_t)(RANDOM() % 2);
}
static void shape_altitude(void)
{
    if (RANDOM() % 4 == 0) {     /* the target's sign test where the subtraction overflows */
        FG32(0x768) = 1; FG32(0x7f0) = 0; FG32(0x7d4) = (0x7000u + RANDOM() % 0x1000) << 16;
        FG32(0x574) = 0x80000000u + RANDOM() % 0x10000;
        memset(s_fixture.data + 0xb2f, 0xff, 4);
    }
    if (RANDOM() % 2) FD8(0xa22) = 1;
    if (RANDOM() % 2) { FG32(0x768) = 1; FG32(0x7f0) = RANDOM() % 2 ? 0 : RANDOM(); }
    if (RANDOM() % 2) { uint32_t value = RANDOM() % 2; memcpy(s_fixture.data + 0xb2b, &value, 4); }
    if (RANDOM() % 2) { uint32_t value = RANDOM() % 0x400; memcpy(s_fixture.data + 0xb37, &value, 4); }
}

static void shape_yaw_moment(void)
{
    if (RANDOM() % 3 == 0) { FD64(0x1388) = 0.0; FG16(0xc44) = (uint16_t)(1 + RANDOM() % 0x400); }
    if (RANDOM() % 2) { FG8(0x609) = 0; FG32(0x278) = 0; }
    FG16(0xaec) = (uint16_t)(1 + RANDOM() % 4);
}

static void shape_carburettor(void)
{
    if (RANDOM() % 3 == 0) {     /* the ice about to reach 1 */
        FG16(0xc44) = 0x100; FD64(0x1900) = 10.0; FG32(0x788) = 0; FD64(0x16e0) = 10.0;
        FG16(0xf10) = 1; FG16(0xf0e) = 1; FD64(0x1450) = 25.0;
        FD64(0x18f0) = 1.0 - (double)(RANDOM() % 10) / 1000.0; FD32(0x18f8) = 0;
        return;
    }
    if (RANDOM() % 4) FG16(0xc44) = (uint16_t)(1 + RANDOM() % 0x400);
    if (RANDOM() % 2) { FG32(0x788) = RANDOM() % 2; FG32(0x29c) = RANDOM() % 2; }
    if (RANDOM() % 2) { FG16(0xf10) = 1; FG16(0xf0e) = 1; }
    if (RANDOM() % 2) FD32(0x18f8) = RANDOM() % 2;
    if (RANDOM() % 2) FD64(0x1900) = (double)(RANDOM() % 120) - 10.0;
}

static void shape_rotor(void)
{
    if (RANDOM() % 3 == 0) { FG16(0xc44) = 0; s_control_word = 0x037f; FD64(0x1ac0) = (double)(RANDOM() % 1000) / 777.0; }
    if (RANDOM() % 2) { FG32(0x920) = RANDOM() % 2; FG16(0x33c) = (uint16_t)(RANDOM() % 2); }
    if (RANDOM() % 3 == 0) FD64(0x1ac0) = RF64(0x153d0) / RF64(0x153c8) * (RANDOM() % 2 ? 1.0 : -1.0);
}

static void shape_prop_pitch(void)
{
    if (RANDOM() % 2) FG32(0x7e4) = RANDOM() % 2;
    if (RANDOM() % 2) FG16(0x612) = (uint16_t)(RANDOM() % 2);
}
static void shape_part_pass(void)
{
    uint8_t *part = (uint8_t *)(uintptr_t)s_entry_regs.esi;
    if (RANDOM() % 4) { P32(part, 4) = 1; P32(part, 8) = 1; P32(part, 0) = RANDOM() % 2; }
}
static void shape_deinit(void)
{
    if (RANDOM() % 2) FG32(0x604) = RANDOM() % 2;
    if (RANDOM() % 2) { FG8(0x609) = (uint8_t)(RANDOM() % 2); FD32(0x1778) = RANDOM() % 2; }
    if (RANDOM() % 2) FG32(0x11d8) = 0;
}

static void shape_rotate16(void)
{
    FD32(0x7c0) = RANDOM() % 2;
}

static void shape_situation_set(void)
{
    static const uint32_t sizes[6] = {0xc0, 0xc0, 0x58, 0xbf, 0xc1, 0xb8};   /* full, short, and either side of full */
    s_arguments[1] = sizes[RANDOM() % 6];
}

static void shape_throttle_records(void)
{   /* the throttle records' addresses, as the engine terms keep them, into the fixture's GLOBAL */
    static const uint16_t throttles[4] = {0x88c, 0x924, 0x9bc, 0xa54};
    uint32_t at;
    for (at = 0; at < 4; ++at) FD32(0x1fd0 + at * 4) = (uint32_t)(uintptr_t)(s_fixture.global + throttles[at]);
    FG16(0xaec) = (uint16_t)(RANDOM() % 5);
    if (RANDOM() % 2) {   /* the propellers' torque: running, with rpm */
        FG8(0x609) = 0; FG32(0x278) = 0;
        if (!FG16(0xc44)) FG16(0xc44) = (uint16_t)(1 + RANDOM() % 0x4000);
        if (RANDOM() % 2) { FG16(0x8c8) = (uint16_t)RANDOM(); FG16(0x898) = (uint16_t)RANDOM(); FG16(0x930) = (uint16_t)RANDOM();
                            FG16(0x9c8) = (uint16_t)RANDOM(); FG16(0xa60) = (uint16_t)RANDOM(); }
    }
    else if (RANDOM() % 2) { FG8(0x609) = 0; FG32(0x278) = 1 + RANDOM() % 3; if (!FG16(0xc44)) FG16(0xc44) = 0x100; }   /* +0x278 alone gates it */
    if (RANDOM() % 3 == 0) {   /* the stall band |+0x1490| < +0x768 * pi: on an edge, exactly (a power of two keeps it exact) */
        double scale = (double)(1u << (RANDOM() % 8)) / 16.0;
        FD64(0x768) = scale;
        FD64(0x1490) = (RANDOM() % 2 ? 3.14159265359 : -3.14159265359) * scale;
        if (RANDOM() % 2) FD64(0x1490) = FD64(0x1490) * 0.5;
    }
}

static void shape_situation_message(void)
{   /* the messages (load, save, others) and a load's event: its tag, size and record */
    static const uint32_t messages[5] = {0x22, 0x22, 0x23, 0x21, 0x24};
    static const uint32_t tags[5] = {0x5a, 0x38e, 0x38e, 0x5b, 0};
    static const uint32_t sizes[4] = {0xc0, 0x58, 0xbf, 0xc1};
    uint8_t *event = s_fixture.blocks[RANDOM() % BLOCKS] + RANDOM() % 0x100 * 4;
    uint32_t record = (uint32_t)(uintptr_t)s_fixture.blocks[RANDOM() % BLOCKS] + RANDOM() % 0x40 * 4;
    s_arguments[0] = messages[RANDOM() % 5];
    s_arguments[1] = (uint32_t)(uintptr_t)event;
    memcpy(event, &tags[RANDOM() % 5], 4);
    memcpy(event + 8, &sizes[RANDOM() % 4], 4);
    memcpy(event + 0xc, &record, 4);
}

static void shape_situation_start(void)
{   /* a kept full record, or none (then MAIN's, scripted) */
    FG32(0x11d8) = RANDOM() % 3 ? 0 : (uint32_t)(uintptr_t)s_fixture.blocks[RANDOM() % BLOCKS] + RANDOM() % 0x40 * 4;
}

static void shape_governor_pass(void)
{   /* on (mostly), each rpm source; the cap +0xac3 at 0, below 0 or small (so the step sits at
     * it); the rpm limit +0xacb * 65536 / +0x439a placed, and the engines' rpm about it; a
     * zero divisor now and then; and the governor's own */
    static const int32_t caps[5] = {0, -3, 2, 0x40, 0x7fff};
    uint32_t value, limit, at;
    shape_governor();     /* first: it sets +0xacb too */
    FD16(0x996) = RANDOM() % 5 ? 0 : (uint16_t)(1 + RANDOM() % 3);
    if (RANDOM() % 4 && FD32(0x1624) == 2) FD32(0x1624) = 0;
    if (RANDOM() % 2) FG32(0x7e4) = RANDOM() % 2;
    if (RANDOM() % 2) FG32(0x7dc) = RANDOM() % 2;
    if (RANDOM() % 2) FG16(0x612) = (uint16_t)(RANDOM() % 2);
    if (RANDOM() % 2) FG32(0x2bc) = RANDOM() % 0x100000;
    if (RANDOM() % 2) {   /* a positive step: the pitch's rpm, a fixed prop, a small load */
        FG32(0x7e4) = 0; FG32(0x7dc) = 1; FG16(0x612) = 1;
        FG32(0x7e0) = (0x100 + RANDOM() % 0x4000) << 16; FG32(0x2bc) = RANDOM() % 0x1000;
    }
    if (RANDOM() % 2) {   /* the filter at +0x24c0 live: wide limits, small gains, so its integrators move */
        FD64(0x24c8) = 0.001; FD64(0x24d0) = 0.002; FD64(0x24e0) = 1e9; FD64(0x24e8) = 1e9;
    }
    if (RANDOM() % 4) { value = (uint32_t)caps[RANDOM() % 5]; memcpy(s_fixture.data + 0xac3, &value, 4); }
    if (RANDOM() % 8 == 0) FD16(0x439a) = 0;
    else {
        FD16(0x439a) = 0x8000;                     /* limit = +0xacb * 2 */
        limit = 0x100 + RANDOM() % 0x1000;
        value = limit / 2; memcpy(s_fixture.data + 0xacb, &value, 4);
        limit = value * 2;
        for (at = 0; at < 4; ++at) {
            static const int32_t offsets[6] = {-2, -1, 0, 1, 2, -0x100};
            FG16(0x898 + at * 0x98) = (uint16_t)(limit + offsets[RANDOM() % 6]);
        }
    }
}

static void shape_contacts_below(void)
{   /* each point on or off, each gear up or down, and heights about the ground */
    static const uint16_t points[10] = {0x2200, 0x1f20, 0x2400, 0x29e0, 0x2020, 0x2140, 0x27a0, 0x22c0, 0x1e00, 0x26e0};
    uint32_t at;
    for (at = 0; at < 10; ++at) if (RANDOM() % 2) FD32(points[at] + 4) = RANDOM() % 3 ? 1 : 0;
    for (at = 0; at < 3; ++at) if (RANDOM() % 2) FG32(0xbec + at * 4) = RANDOM() % 2;
    if (RANDOM() % 2) FD32(0x1d18) = (uint32_t)((int32_t)(RANDOM() % 0x20000) - 0x10000);
}

static void shape_autothrottle_modes(void)
{   /* the modes' words each set or not, the last pass's flags, the approach height about the
     * aircraft's, the rpm about +0xaeb; and the governor's pass's own */
    static const uint16_t words[8] = {0x774, 0x76c, 0x770, 0x810, 0x80c, 0x7dc, 0x7e4, 0x366};
    uint32_t at, value;
    shape_governor_pass();
    for (at = 0; at < 8; ++at) if (RANDOM() % 2) { if (words[at] == 0x366) FG8(0x366) = (uint8_t)(RANDOM() % 2); else FG32(words[at]) = RANDOM() % 3 ? (RANDOM() % 2) : 0; }
    if (RANDOM() % 2) { FG32(0x7dc) = 0; FG32(0x7e4) = 0; }
    for (at = 0; at < 3; ++at) if (RANDOM() % 2) { static const uint16_t flags[3] = {0xad3, 0xadb, 0xae7}; value = RANDOM() % 2; memcpy(s_fixture.data + flags[at], &value, 4); }
    if (RANDOM() % 2) { value = FD32(0x1d18) + (RANDOM() % 3) - 1; memcpy(s_fixture.data + 0xae3, &value, 4); }
    if (RANDOM() % 2) { value = RANDOM() % 0x1000; memcpy(s_fixture.data + 0xaeb, &value, 4); }
    FG16(0xaec) = (uint16_t)(RANDOM() % 6);
    if (RANDOM() % 2) {   /* armed, no hold: the mode changes and the engagement test */
        FG32(0x774) = 1; FG32(0x810) = RANDOM() % 4 ? 1 : 0; FG32(0x80c) = RANDOM() % 2; FG32(0x7dc) = 0; FG32(0x7e4) = 0;
        value = RANDOM() % 2; memcpy(s_fixture.data + 0xae7, &value, 4);
        value = RANDOM() % 3 ? 0 : 2 + RANDOM() % 4; memcpy(s_fixture.data + 0xad3, &value, 4);
        value = RANDOM() % 2 ? 0 : 2 + RANDOM() % 4; memcpy(s_fixture.data + 0xadb, &value, 4);
        FD8(0xa21) = (uint8_t)(RANDOM() % 3);
        FG8(0x366) = (uint8_t)(RANDOM() % 2);
        if (RANDOM() % 2) {   /* engines' rpm apart, the threshold between them */
            uint32_t rpm[4];
            for (at = 0; at < 4; ++at) FG16(0x898 + at * 0x98) = (uint16_t)(rpm[at] = 0x100 + RANDOM() % 0x2000);
            FD16(0x439a) = 0x8000;
            if (RANDOM() % 2) { FG16(0xaec) = (uint16_t)(RANDOM() % 2); FG16(0x930) = (uint16_t)(rpm[1] = 0x2200 + RANDOM() % 0x100); }   /* a single, engine 2's word the highest */
            value = rpm[0];     /* the rpm retail looks at (SIM1-AUTOTHROTTLE-ENGINES): all four for a single */
            if ((int16_t)FG16(0xaec) < 2) for (at = 1; at < 4; ++at) if (rpm[at] > value) value = rpm[at];
            value = value / 2 + (RANDOM() % 3) - 1; memcpy(s_fixture.data + 0xaeb, &value, 4);
        }
    }
}

static void shape_contact_terms(void)
{   /* the scale word signed either way */
    if (RANDOM() % 2) FG16(0xaf4) = (uint16_t)RANDOM();
}

static void shape_rate_limit(void)
{   /* x at the target, a step short of it, exactly onto it, or past; a NaN now and then */
    static const double xs[6] = {1.0, -2.5, 100.0, 0.0, -0.0, 1e-300};
    double x = xs[RANDOM() % 6], step = (double)(RANDOM() % 8) / 4.0, target;
    switch (RANDOM() % 7) {
    case 6: x = 1.0; step = 1.0; target = -0.0; break;   /* stepping down onto a zero of the other sign */
    case 0: target = x; break;
    case 1: target = x + step; break;
    case 2: target = x - step; break;
    case 3: target = x + step * 2; break;
    case 4: target = x - step / 2; break;
    default: { uint64_t nan = 0x7ff8000000000000ull; memcpy(&target, &nan, 8); } break;
    }
    set_double(s_arguments, x);
    set_double(s_arguments + 2, target);
    if (RANDOM() % 4) {   /* rate * +0x16f0 / divisor = step, exactly */
        set_double(s_arguments + 4, FD64(0x16f0));
        set_double(s_arguments + 6, step);
    }
}

static void shape_ground_bumps(void)
{   /* each surface type, changed or not; heights about the bound and the floor */
    static const uint16_t heights[3] = {0x24a0, 0x1fc0, 0x22a0};
    uint32_t at;
    FG8(0x11ec) = (uint8_t)(RANDOM() % 10);
    FD8(0x11b8) = RANDOM() % 2 ? FG8(0x11ec) : (uint8_t)(RANDOM() % 10);
    for (at = 0; at < 3; ++at) switch (RANDOM() % 4) {
    case 0: FD64(heights[at]) = FD64(0x11d0); break;
    case 1: FD64(heights[at]) = -0.001; break;
    case 2: FD64(heights[at]) = 0.0; break;
    default: break;
    }
    if (RANDOM() % 2) FG32(0x2b4) = RANDOM() % 4 ? RANDOM() % 0x1000000 : 0x80000000u + RANDOM() % 0x1000000;
    if (RANDOM() % 4 == 0) {   /* a signalling NaN rate, kept (the type unchanged) */
        static const uint16_t rates[3] = {0x11d8, 0x11c8, 0x11c0};
        uint64_t snan = 0x7ff0000000000001ull + RANDOM() % 5;
        FD8(0x11b8) = FG8(0x11ec);
        memcpy(s_fixture.data + rates[RANDOM() % 3], &snan, 8);
    }
}

static void shape_body_accelerations(void)
{   /* signalling NaNs in the kept values; a velocity or rate past a bound */
    static const uint16_t kept[6] = {0x1328, 0x1330, 0x1338, 0x1358, 0x1360, 0x1368};
    if (RANDOM() % 3 == 0) { uint64_t snan = 0x7ff0000000000001ull + RANDOM() % 5; memcpy(s_fixture.data + kept[RANDOM() % 6], &snan, 8); }
    if (RANDOM() % 3 == 0) FD64(0x1270) = (RANDOM() % 2 ? 1e9 : -1e9) * (double)(1 + RANDOM() % 4);
    if (RANDOM() % 3 == 0) FD64(0x1310 + (RANDOM() % 3) * 8) = (RANDOM() % 2 ? 1e12 : -1e12);
}

static void shape_angle_follow(void)
{   /* each path: tracking (+0xb67 or +0x118c), started and timing, or reset; outputs and
     * offsets about their bounds; the gain about its ends */
    switch (RANDOM() % 3) {
    case 0: FG8(0xb67) = (uint8_t)(RANDOM() % 2); FD32(0x118c) = FG8(0xb67) ? 0 : 1; break;
    case 1: FG8(0xb67) = 0; FD32(0x118c) = 0; FD32(0x1188) = 1; FD64(0x1180) = RANDOM() % 2 ? 0.0 : FD64(0x410) - (double)(RANDOM() % 3) * 0.5; break;
    default: FG8(0xb67) = 0; FD32(0x118c) = 0; FD32(0x1188) = RANDOM() % 2; break;
    }
    if (RANDOM() % 3 == 0) FD64(0x3d0) = RANDOM() % 2 ? 1.0 : 0.0;
    if (RANDOM() % 3 == 0) FD64(0x1160) = FD64(0x400) - FD64(0x3e0);
    if (RANDOM() % 3 == 0) FD64(0x1168) = FD64(0x408) - FD64(0x3e8);
    if (RANDOM() % 3 == 0) FD64(0x1178) = FD64(0x408);
    if (RANDOM() % 2) FG32(0x578) = RANDOM();
    if (RANDOM() % 2) FG32(0x57c) = RANDOM();
    if (FG8(0xb67) || FD32(0x118c)) switch (RANDOM() % 4) {
    case 0: {   /* the first offset a sixteenth of an ulp past its bound: above it unrounded, on it rounded */
        double bound = FD64(0x400), next;
        uint64_t bits;
        if (bound == 0.0) bound = FD64(0x400) = 1.0;
        memcpy(&bits, &bound, 8); if (bound > 0) ++bits; else --bits; memcpy(&next, &bits, 8);   /* the next double up */
        FD64(0x1170) = bound;
        if (FD64(0x16f0) != 0.0) FD64(0x3e0) = (next - bound) / 16.0 / FD64(0x16f0);
        break;
    }
    case 1:     /* stepping onto a zero bound from the other zero */
        FD64(0x400) = 0.0; FD64(0x3e0) = -0.0; FD64(0x1160) = -0.0; FD64(0x1170) = -1.0;
        break;
    default: break;
    }
}

static struct operation s_operations[] = {
    OP(0x4b30, op_part_bits, 0, 0, 1, ARG_ANY, 0),
    OP(0x4b80, op_part_clear, 0, 0, 1, ARG_ANY, 0),
    OP(0x6e20, op_clear_pairs, 0, 0, 1, ARG_ANY, 0),
    OP(0xa460, op_fixed_step, 0, 0x07, 0, ARG_ANY, 0),
    OP(0xa470, op_table32, 0, 0x07, 0, ARG_DATA, 0),
    OP(0xa498, op_table16, 0, 0x07, 0, ARG_DATA, 0),
    OP(0x7e5f, op_isqrt, 0, 0x05, 0, ARG_ANY, 0),
    OP(0x96a2, op_square_term, 0, 0x0f, 0, ARG_ANY, 0),
    OP(0xa3db, op_cross_term, 0, 0, 1, ARG_ANY, 0),
    OP(0xa628, op_isqrt64, 0, 0x01, 0, ARG_ANY, 0),
    OP(0xa620, op_isqrt_low, 0, 0x01, 0, ARG_ANY, 0),
    OPS(0x1ee0, op_hold_check, shape_hold, 0, 0, 1, ARG_ANY, 0),
    OPS(0x1fc0, op_surfaces, shape_surfaces, 0, 0, 1, ARG_ANY, 0),
    OP(0x2de0, op_vector_sums, 0, 0, 1, ARG_ANY, 0),
    OPS(0x4940, op_countdown, shape_countdown, 0, 0, 1, ARG_ANY, ARG_BLOCK, ARG_BLOCK, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OP(0x5129, op_weighted, 0, 0x01, 1, ARG_ANY, ARG_LIST, 0),
    OP(0x5640, op_pitch_sum, 0, 0, 1, ARG_ANY, 0),
    OP(0x56e0, op_side_sum, 0, 0, 1, ARG_ANY, 0),
    OP(0x5740, op_thrust_sum, 0, 0, 1, ARG_ANY, 0),
    OP(0x6100, op_turn_pair, 0, 0, 1, ARG_ANY, 0),
    OPS(0x6870, op_flag_follow, shape_flag_follow, 0, 0, 1, ARG_ANY, 0),
    OPS(0x6a50, op_wrap, shape_wrap, 0, 0, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OP(0x6d10, op_part_axes, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OP(0x6d80, op_part_rotate, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OP(0x6ea0, op_part_add, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OPS(0x9dfa, op_scale48, shape_scale48, 0, 0x05, 1, ARG_ANY, 0),
    OPS(0x9e62, op_mark_part, shape_mark, 0, 0, 1, ARG_BLOCK, 0),
    OP(0xa41b, op_mul48, 0, 0x05, 1, ARG_ANY, 0),
    OP(0x24d5, op_set_7c0, 0, 0, 1, ARG_ANY, 0),
    OPS(0xfb1d, op_clamp16, shape_clamp, 0, 0x01, 0, ARG_ANY, 0),
    OPS(0xfb33, op_clamp32, shape_clamp, 0, 0x01, 0, ARG_ANY, 0),
    OPS(0xdb61, op_global_384, shape_global_384, 0, 0x01, 1, ARG_ANY, 0),
    OP(0xf9d8, op_set_limits, 0, 0, 1, ARG_ANY, 0),
    OP(0xfb4d, op_fold, 2, 0, 1, ARG_ANY, 0),
    OP(0x51dc, op_rudder_input, 0, 0, 1, ARG_ANY, ARG_ANY, 0),
    OP(0x518c, op_elevator_input, 0, 0, 1, ARG_ANY, ARG_ANY, 0),
    OP(0x51b4, op_aileron_input, 0, 0, 1, ARG_ANY, ARG_ANY, 0),
    OPS(0x5229, op_elevator_control, shape_control, 0, 0, 1, ARG_ANY, 0),
    OP(0x5259, op_aileron_control, 0, 0, 1, ARG_ANY, 0),
    OP(0x5276, op_rudder_control, 0, 0, 1, ARG_ANY, 0),
    OP(0xa3fc, op_mode3_zero, 0, 0, 1, ARG_ANY, 0),
    OP(0xd8a0, op_ratio_968, 0, 0, 1, ARG_ANY, 0),
    OP(0xa786, op_table_scaled, 0, 0, 1, ARG_ANY, ARG_DATA, 0),
    OPN(0xa5ee, op_bearing, 0, 0, 0, 0x04, 1, ARG_ANY, 0),
    OPS(0xd750, op_angle_2b68, shape_angle_2b68, 0, 0, 1, ARG_ANY, 0),
    OPS(0xda32, op_check_874, shape_check_874, 0, 0, 1, ARG_ANY, 0),
    OPS(0xfb69, op_band_value, shape_band, 0, 0, 1, ARG_ANY, 0),
    OPS(0xda83, op_overlimit_alert, shape_limit, 0, 0, 1, ARG_ANY, 0),
    OPS(0xe6d6, op_heading_9e6, shape_heading, 0, 0, 1, ARG_ANY, 0),
    OP(0x8ad0, op_reset_886, 0, 0, 1, ARG_ANY, 0),
    OPS(0xfaac, op_ground_attitude, shape_attitude, 0, 0, 1, ARG_ANY, 0),
    OPS(0xaca9, op_height, shape_height, 0, 0, 1, ARG_ANY, 0),
    OPS(0xfbc5, op_filter, shape_filter, 1, 0, 1, ARG_BLOCK, 0),
    OPS(0xdf6f, op_heading_hold, shape_heading_hold, 0, 0, 1, ARG_ANY, 0),
    OPS(0xad20, op_contact_offsets, shape_contact, 0, 0, 1, ARG_ANY, ARG_BLOCK, ARG_ANY, 0),
    OPS(0x10bb0, op_part_limit, shape_part_limit, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OP(0x89e4, op_table2d, 0, 0, 1, ARG_DATA, 0),
    OPS(0x8b40, op_trim_hold, shape_trim, 0, 0, 1, ARG_ANY, ARG_ANY, ARG_SMALL, 0),
    OPS(0xab78, op_gear_model, shape_gear, 0, 0, 1, ARG_ANY, 0),
    OPS(0x63b0, op_stall_warning, shape_stall, 0, 0, 1, ARG_ANY, 0),
    OPS(0xd7f0, op_pitch_schedule, shape_schedule, 0, 0, 1, ARG_ANY, 0),
    OPB(0x8a77, op_contact_rise, shape_rise, 0, 0, 1, ARG_BLOCK, 0),
    OPS(0x11810, op_power_7b8, shape_power, 0, 0, 1, ARG_ANY, 0),
    OPN(0xa599, op_bearing_of, 0, 0, 0x05, 0x05, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OPS(0xdfc8, op_turn_hold, shape_holds, 0, 0, 1, ARG_ANY, 0),
    OP(0x10c40, op_record_words, 0, 0, 1, ARG_ANY, 0),
    OPS(0xdeea, op_rudder_hold, shape_holds, 0, 0, 1, ARG_ANY, 0),
    OPS(0x9d48, op_crash_count, shape_crash, 0, 0, 1, ARG_ANY, 0),
    OPS(0x9e92, op_part_damping, shape_damping, 0, 0, 1, ARG_BLOCK, 0),
    OPS(0xf9f4, op_ground_stop, shape_ground_stop, 0, 0, 1, ARG_ANY, 0),
    OP(0xa4c0, op_attitude_trig, 0, 0, 1, ARG_ANY, 0),
    OPS(0xf6ab, op_climb_history, shape_climb, 0, 0, 1, ARG_ANY, 0),
    OPS(0xddf6, op_yaw_damper, shape_yaw, 0, 0, 1, ARG_ANY, 0),
    OPS(0x6160, op_gear_force, shape_gear_force, 0, 0, 1, ARG_ANY, 0),
    OP(0x955d, op_drag_sums, 0, 0, 1, ARG_ANY, 0),
    OPS(0xdb7c, op_station_distance, shape_station, 0, 0, 1, ARG_ANY, 0),
    OP(0x11620, op_track, 0, 0, 1, ARG_ANY, 0),
    OP(0x948e, op_air_data, 0, 0, 1, ARG_ANY, 0),
    OPS(0x10920, op_limit_fraction, shape_fraction, 0, 0, 1, ARG_ANY, 0),
    OPS(0xe050, op_wing_leveller, shape_leveller, 0, 0, 1, ARG_ANY, 0),
    OPS(0xf387, op_flare_throttle, shape_flare, 0, 0, 1, ARG_ANY, 0),
    OP(0xada0, op_copy_contact, 0, 0, 1, ARG_ANY, ARG_BLOCK, ARG_BLOCK, 0),
    OP(0x6f30, op_attitude_matrix, 0, 0, 1, ARG_ANY, 0),
    OP(0x6aa0, op_part_wind, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OPS(0xdc7e, op_annunciators, shape_annunciators, 0, 0, 1, ARG_ANY, 0),
    OPS(0x9f11, op_part_slip, shape_slip, 0, 0, 1, ARG_BLOCK, 0),
    OPS(0xa66c, op_yaw_rate, shape_yaw_rate, 0, 0, 1, ARG_ANY, 0),
    OPS(0xa263, op_part_side_force, shape_side_force, 0, 0, 1, ARG_BLOCK, 0),
    OPS(0x6250, op_icing, shape_icing, 0, 0, 1, ARG_ANY, 0),
    OP(0x11e30, op_turn_climb_rates, 0, 0, 1, ARG_ANY, 0),
    OPS(0xe173, op_pitch_hold, shape_pitch_hold, 0, 0, 1, ARG_ANY, 0),
    OPS(0xec31, op_autothrottle, shape_autothrottle, 0, 0, 1, ARG_ANY, 0),
    OP(0x11cf0, op_heading_term, 0, 0, 1, ARG_ANY, 0),
    OPS(0x9334, op_turbulence, shape_turbulence, 0, 0, 1, ARG_ANY, 0),
    OPS(0x4840, op_part_timers, shape_timers, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OPS(0xf510, op_coupler, shape_coupler, 0, 0, 1, ARG_ANY, 0),
    OPS(0x10a00, op_structure, shape_structure, 0, 0, 1, ARG_ANY, 0),
    OPS(0x68e0, op_engine_ratio, shape_engine_ratio, 0, 0, 1, ARG_ANY, 0),
    OPS(0xea69, op_governor, shape_governor, 0, 0, 1, ARG_ANY, 0),
    OPS(0x6bb0, op_steering, shape_steering, 0, 0, 1, ARG_ANY, ARG_BLOCK, ARG_SMALL, ARG_BLOCK, ARG_BLOCK, 0),
    OPS(0x4990, op_gear_travel, shape_gear_travel, 0, 0, 1, ARG_ANY, 0),
    OPS(0xed94, op_glideslope, shape_glideslope, 0, 0, 1, ARG_ANY, 0),
    OPS(0x110a0, op_situation_record, shape_record, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OPS(0x11660, op_table_lookup, shape_table, 0, 0, 1, ARG_ANY, ARG_BLOCK, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OPS(0xef53, op_glideslope_capture, shape_capture, 0, 0, 1, ARG_ANY, 0),
    OPS(0x2c60, op_rotate_a, shape_rotate, 0, 0, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_BLOCK, ARG_BLOCK, ARG_BLOCK, 0),
    OPS(0x2ac0, op_rotate_b, shape_rotate, 0, 0, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_BLOCK, ARG_BLOCK, ARG_BLOCK, 0),
    OP(0x52b0, op_inertia, 0, 0, 1, ARG_ANY, 0),
    OPN(0x40d0, op_quaternion, 0, 0, 0, 0x100, 1, ARG_ANY, 0),
    OPS(0x3700, op_park, shape_park, 0, 0, 1, ARG_ANY, 0),
    OPS(0x4f00, op_force_feedback, shape_feedback, 0, 0, 1, ARG_ANY, 0),
    OPS(0x10d50, op_record_alloc, shape_alloc, 0, 0, 1, ARG_ANY, 0),
    OP(0x11050, op_record_save, 0, 0, 1, ARG_ANY, 0),
    OP(0x11850, op_engine_terms, 0, 0, 1, ARG_ANY, 0),
    OPS(0x4770, op_engine_power, shape_engine_power, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OP(0x11910, op_prop_thrust, 0, 0, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OP(0x119c0, op_prop_efficiency, 0, 0, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OP(0x54a0, op_pitch_moment, 0, 0, 1, ARG_ANY, 0),
    OP(0xa7b0, op_pass_setup, 0, 0, 1, ARG_ANY, 0),
    OPS(0xfc40, op_aircraft_setup, shape_setup, 0, 0, 1, ARG_ANY, 0),
    OPS(0xa000, op_wheel_force, shape_wheel, 0, 0, 1, ARG_BLOCK, 0),
    OPS(0xf153, op_flare, shape_flare_land, 0, 0, 1, ARG_ANY, 0),
    OPS(0xf774, op_track_course, shape_course, 0, 0, 1, ARG_ANY, 0),
    OPS(0xe2cf, op_altitude_capture, shape_altitude, 0, 0, 1, ARG_ANY, 0),
    OPS(0x5c00, op_yaw_moment, shape_yaw_moment, 0, 0, 1, ARG_ANY, 0),
    OPS(0x11a90, op_carburettor, shape_carburettor, 0, 0, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OPS(0x125c0, op_rotor_mixing, shape_rotor, 0, 0, 1, ARG_ANY, 0),
    OPS(0xe9d3, op_prop_pitch, shape_prop_pitch, 0, 0, 1, ARG_ANY, 0),
    OPS(0x9cd9, op_part_pass, shape_part_pass, 0, 0, 1, ARG_BLOCK, 0),
    OPS(0x10a0, op_deinit, shape_deinit, 0, 0, 1, ARG_ANY, 0),
    OPN(0x24e0, op_rotate16, shape_rotate16, 0, 0, 0x80, 0, ARG_ANY, 0),
    OP(0x10df0, op_situation_restore, 0, 0, 1, ARG_ANY, ARG_BLOCK, 0),
    OPS(0x11290, op_situation_set, shape_situation_set, 0, 0, 1, ARG_ANY, ARG_BLOCK, ARG_ANY, 0),
    OPS(0x5e20, op_roll_moment, shape_throttle_records, 0, 0, 1, ARG_ANY, 0),
    OPS(0x10da0, op_situation_message, shape_situation_message, 0, 0, 1, ARG_ANY, ARG_ANY, ARG_ANY, 0),
    OPS(0x10cc0, op_situation_start, shape_situation_start, 0, 0, 1, ARG_ANY, 0),
    OPS(0xe744, op_governor_pass, shape_governor_pass, 0, 0, 1, ARG_ANY, 0),
    OPS(0x97e5, op_contacts_below, shape_contacts_below, 0, 0, 1, ARG_ANY, 0),
    OPS(0xe4fa, op_autothrottle_modes, shape_autothrottle_modes, 0, 0, 1, ARG_ANY, 0),
    OPS(0x4c10, op_contact_terms, shape_contact_terms, 0, 0, 1, ARG_ANY, 0),
    OPS(0x2200, op_rate_limit, shape_rate_limit, 0, 0, 1, ARG_ANY, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, ARG_DOUBLE, ARG_DOUBLE_HIGH, 0),
    OPS(0x7050, op_ground_bumps, shape_ground_bumps, 0, 0, 1, ARG_ANY, 0),
    OPS(0x2eb0, op_body_accelerations, shape_body_accelerations, 0, 0, 1, ARG_ANY, 0),
    OPS(0x6460, op_angle_follow, shape_angle_follow, 0, 0, 1, ARG_ANY, 0),
    OP(0x13030, op_rotor_forces, 0, 0, 1, ARG_ANY, 0),
};
