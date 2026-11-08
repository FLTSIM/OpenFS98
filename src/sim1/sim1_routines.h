/* SIM1.sim, decompiled: the routines, by retail address (src/sim1/sim1_*.c). */
#ifndef FS98_SIM1_ROUTINES_H
#define FS98_SIM1_ROUTINES_H
#include <stdint.h>

/* sim1_small.c */
void sim1_part_bits(void);                                                      /* 0x22004b30 */
void sim1_part_clear(void);                                                     /* 0x22004b80 */
void sim1_clear_pairs(void);                                                    /* 0x22006e20 */
uint16_t sim1_fixed_step(uint16_t fraction, uint16_t delta, uint16_t base, uint16_t *dx_out); /* 0x2200a460 */
uint16_t sim1_table32(const uint8_t *table, uint16_t position);                 /* 0x2200a470 */
uint16_t sim1_table16(const uint8_t *table, uint16_t position);                 /* 0x2200a498 */
uint32_t sim1_isqrt(uint32_t value);                                            /* 0x22007e5f */
uint32_t sim1_square_term(uint32_t value, uint32_t scale, uint32_t *high_out);  /* 0x220096a2 */
void sim1_cross_term(void);                                                     /* 0x2200a3db */
uint32_t sim1_isqrt64(int64_t value);                                           /* 0x2200a628 */
void sim1_hold_check(void);                                                     /* 0x22001ee0 */
void sim1_surfaces(void);                                                       /* 0x22001fc0 */
void sim1_vector_sums(void);                                                    /* 0x22002de0 */
void __stdcall sim1_countdown(uint8_t *record, uint8_t *owner, double step);    /* 0x22004940 */
uint32_t __stdcall sim1_weighted(const uint8_t *list);                          /* 0x22005129 */
void sim1_pitch_sum(void);                                                      /* 0x22005640 */
void sim1_side_sum(void);                                                       /* 0x220056e0 */
void sim1_thrust_sum(void);                                                     /* 0x22005740 */
void sim1_turn_pair(void);                                                      /* 0x22006100 */
void sim1_flag_follow(void);                                                    /* 0x22006870 */
long double __stdcall sim1_wrap(double a, double b);                            /* 0x22006a50 */
void __stdcall sim1_part_axes(uint8_t *part);                                   /* 0x22006d10 */
void __stdcall sim1_part_rotate(uint8_t *part);                                 /* 0x22006d80 */
void __stdcall sim1_part_add(const uint8_t *part);                              /* 0x22006ea0 */
uint64_t sim1_scale48(uint32_t value);                                          /* 0x22009dfa */
void sim1_mark_part(uint8_t *global, const uint8_t *part);                      /* 0x22009e62 */
uint64_t sim1_mul48(uint16_t low, uint32_t high, uint16_t factor);              /* 0x2200a41b */
void sim1_set_7c0(void);                                                        /* 0x220024d5 */
uint16_t sim1_clamp16(uint16_t value);                                          /* 0x2200fb1d */
uint32_t sim1_clamp32(uint32_t value);                                          /* 0x2200fb33 */
uint32_t sim1_global_384(void);                                                 /* 0x2200db61 */
void sim1_set_limits(uint8_t *global);                                          /* 0x2200f9d8 */
long double sim1_fold(long double a, long double b);                            /* 0x2200fb4d */
void __stdcall sim1_rudder_input(uint32_t value);                               /* 0x220051dc */
void __stdcall sim1_elevator_input(uint32_t value);                             /* 0x2200518c */
void __stdcall sim1_aileron_input(uint32_t value);                              /* 0x220051b4 */
void sim1_elevator_control(uint8_t *global);                                    /* 0x22005229 */
void sim1_aileron_control(uint8_t *global);                                     /* 0x22005259 */
void sim1_rudder_control(uint8_t *global);                                      /* 0x22005276 */
void sim1_mode3_zero(void);                                                     /* 0x2200a3fc */
void sim1_ratio_968(void);                                                      /* 0x2200d8a0 */
long double __stdcall sim1_table_scaled(const uint8_t *table);                  /* 0x2200a786 */
uint16_t sim1_bearing(int16_t y, int16_t x);                                    /* 0x2200a5ee */
void sim1_angle_2b68(void);                                                     /* 0x2200d750 */
void sim1_check_874(uint8_t *global);                                           /* 0x2200da32 */
long double sim1_band_value(const uint8_t *global);                             /* 0x2200fb69 */
void sim1_overlimit_alert(uint8_t *global);                                     /* 0x2200da83 */
void sim1_heading_9e6(uint8_t *global);                                         /* 0x2200e6d6 */
void sim1_reset_886(uint8_t *global);                                           /* 0x22008ad0 */
void sim1_ground_attitude(uint8_t *global);                                     /* 0x2200faac */
uint64_t sim1_height(uint8_t *global);                                          /* 0x2200aca9 */
long double sim1_filter(uint8_t *record, long double x);                         /* 0x2200fbc5 */
void sim1_heading_hold(void);                                                    /* 0x2200df6f */
uint32_t __stdcall sim1_contact_offsets(uint8_t *contact, uint32_t value);       /* 0x2200ad20 */
void __stdcall sim1_part_limit(const uint8_t *part);                             /* 0x22010bb0 */
uint16_t sim1_table2d(const uint8_t *table);                                     /* 0x220089e4 */
void __stdcall sim1_trim_hold(uint8_t *global, uint32_t value, uint32_t which);  /* 0x22008b40, edi */
void sim1_gear_model(uint8_t *global);                                           /* 0x2200ab78 */
void sim1_stall_warning(void);                                                   /* 0x220063b0 */
uint32_t sim1_pitch_schedule(void);                                              /* 0x2200d7f0 */

void sim1_contact_rise(uint8_t *contact);                                         /* 0x22008a77, ebx */
void sim1_power_7b8(void);                                                         /* 0x22011810 */
uint16_t __stdcall sim1_bearing_of(double y, double x);                            /* 0x2200a599 */
void sim1_turn_hold(uint8_t *global);                                              /* 0x2200dfc8 */
uint32_t sim1_record_words(void);                                                  /* 0x22010c40 */
void sim1_rudder_hold(uint8_t *global);                                            /* 0x2200deea */
void sim1_crash_count(uint8_t *global);                                            /* 0x22009d48 */
void sim1_part_damping(uint8_t *part);                                             /* 0x22009e92, esi */
void sim1_ground_stop(uint8_t *global);                                            /* 0x2200f9f4 */
void sim1_attitude_trig(void);                                                     /* 0x2200a4c0 */
void sim1_climb_history(uint8_t *global, uint16_t rate);                           /* 0x2200f6ab, edi, ax */
void sim1_yaw_damper(uint8_t *global);                                             /* 0x2200ddf6 */
void sim1_gear_force(void);                                                        /* 0x22006160 */
void sim1_drag_sums(void);                                                         /* 0x2200955d */
void sim1_station_distance(uint8_t *global);                                       /* 0x2200db7c */
void sim1_track(void);                                                             /* 0x22011620 */
void sim1_air_data(uint8_t *global);                                               /* 0x2200948e */
void sim1_limit_fraction(void);                                                    /* 0x22010920 */
int sim1_wing_leveller(uint8_t *global);                                           /* 0x2200e050 */
void sim1_flare_throttle(uint8_t *global);                                         /* 0x2200f387 */
void __stdcall sim1_copy_contact(uint8_t *to, const uint8_t *from);                /* 0x2200ada0 */
void sim1_attitude_matrix(void);                                                   /* 0x22006f30 */
void __stdcall sim1_part_wind(uint8_t *part);                                      /* 0x22006aa0 */
void sim1_annunciators(uint8_t *global);                                           /* 0x2200dc7e */
void sim1_part_slip(uint8_t *part);                                                /* 0x22009f11, esi */
void sim1_yaw_rate(uint8_t *global);                                               /* 0x2200a66c */
void sim1_part_side_force(uint8_t *global, uint8_t *part);                         /* 0x2200a263, edi, esi */
void sim1_icing(void);                                                             /* 0x22006250 */
void sim1_turn_climb_rates(void);                                                  /* 0x22011e30 */
void sim1_pitch_hold(uint8_t *global);                                             /* 0x2200e173 */
void sim1_autothrottle(uint8_t *global);                                           /* 0x2200ec31 */
void sim1_heading_term(void);                                                      /* 0x22011cf0 */
void sim1_turbulence(uint8_t *global);                                             /* 0x22009334 */
void __stdcall sim1_part_timers(uint8_t *owner);                                   /* 0x22004840 */
void sim1_coupler(uint8_t *global);                                                /* 0x2200f510 */
void sim1_structure(void);                                                         /* 0x22010a00 */
void sim1_engine_ratio(void);                                                      /* 0x220068e0 */
void sim1_governor(uint8_t *global);                                               /* 0x2200ea69 */
void __stdcall sim1_steering(uint8_t *part, uint32_t off, uint16_t *sine_out, uint16_t *cosine_out); /* 0x22006bb0 */
void sim1_gear_travel(void);                                                       /* 0x22004990 */
void sim1_glideslope(uint8_t *global);                                             /* 0x2200ed94 */
void __stdcall sim1_situation_record(uint8_t *record);                             /* 0x220110a0 */
long double __stdcall sim1_table_lookup(const uint8_t *table, double x);           /* 0x22011660 */
void sim1_glideslope_capture(uint8_t *global);                                     /* 0x2200ef53 */
void __stdcall sim1_rotate_a(double a, double b, double c, double *first, double *second, double *third); /* 0x22002c60 */
void __stdcall sim1_rotate_b(double a, double b, double c, double *first, double *second, double *third); /* 0x22002ac0 */
void sim1_inertia(void);                                                           /* 0x220052b0 */
void sim1_quaternion(void);                                                        /* 0x220040d0 */
void sim1_park(void);                                                              /* 0x22003700 */
void sim1_force_feedback(uint8_t *global);                                         /* 0x22004f00 */
void sim1_record_alloc(void);                                                      /* 0x22010d50 */
void sim1_record_save(void);                                                       /* 0x22011050 */
void sim1_engine_terms(void);                                                      /* 0x22011850 */
void __stdcall sim1_engine_power(uint8_t *engine);                                 /* 0x22004770 */
long double __stdcall sim1_prop_thrust(double x);                                  /* 0x22011910 */
long double __stdcall sim1_prop_efficiency(double v);                              /* 0x220119c0 */
void sim1_pitch_moment(void);                                                      /* 0x220054a0 */
void sim1_pass_setup(void);                                                        /* 0x2200a7b0 */
uint32_t sim1_aircraft_setup(void);                                                /* 0x2200fc40, Linkage +0x1c */
void sim1_wheel_force(uint8_t *global, uint8_t *wheel);                            /* 0x2200a000, edi, esi */
void sim1_flare(uint8_t *global);                                                  /* 0x2200f153 */
void sim1_track_course(uint8_t *global);                                           /* 0x2200f774 */
void sim1_altitude_capture(uint8_t *global);                                       /* 0x2200e2cf */
void sim1_yaw_moment(void);                                                        /* 0x22005c00 */
long double __stdcall sim1_carburettor(double demand);                              /* 0x22011a90 */
void sim1_rotor_mixing(void);                                                      /* 0x220125c0 */
void sim1_prop_pitch(uint8_t *global);                                             /* 0x2200e9d3 */
void sim1_part_pass(uint8_t *global, uint8_t *part, uint16_t index);               /* 0x22009cd9, edi, esi, ax */
uint32_t sim1_deinit_core(void);                                                   /* 0x220010a0, Linkage +0x08 (and 0x2200a7fe) */
uint16_t sim1_rotate16(uint16_t cx, uint16_t di, uint16_t si, uint16_t *cx_out, uint16_t *si_out); /* 0x220024e0 */
void __stdcall sim1_situation_restore(const uint8_t *record);                      /* 0x22010df0 */
void __stdcall sim1_situation_set(const uint8_t *record, uint32_t size);           /* 0x22011290 */
void sim1_roll_moment(void);                                                       /* 0x22005e20 */
void __stdcall sim1_situation_message(uint32_t message, const uint8_t *event);       /* 0x22010da0, CHAIN callback */
void sim1_situation_start(void);                                                   /* 0x22010cc0 */
void sim1_governor_pass(uint8_t *global);                                          /* 0x2200e744, edi */
void sim1_contacts_below(uint8_t *global);                                         /* 0x220097e5, edi */
void sim1_autothrottle_modes(uint8_t *global);                                     /* 0x2200e4fa, edi */
void sim1_contact_terms(void);                                                     /* 0x22004c10 */
long double __stdcall sim1_rate_limit(double x, double target, double divisor, double rate); /* 0x22002200 */
void sim1_ground_bumps(void);                                                      /* 0x22007050 */
void sim1_body_accelerations(void);                                                /* 0x22002eb0 */
void sim1_angle_follow(void);                                                      /* 0x22006460 */
void sim1_rotor_forces(void);                                                      /* 0x22013030 */
void sim1_integrate(void);                                                         /* 0x220032a0 */
void __stdcall sim1_engine_strike(uint8_t *record);                                /* 0x22004380 */
void sim1_pitch_sum_moment(void);                                                  /* 0x220057b0 */
void sim1_flaps(void);                                                             /* 0x220016d0 */
uint8_t *sim1_autopilot_pass(uint8_t *global);                                     /* 0x2200daf1, edi in and out */
void sim1_ground_pass(uint8_t *global);                                            /* 0x220096d0, edi */
void sim1_engines_pass(void);                                                      /* 0x22004260 (and its tail 0x22004d80) */
void sim1_autopilot(void);                                                         /* 0x2200d8c7 */
void sim1_spoilers(void);                                                          /* 0x22001360 */
void sim1_landing_gear(void);                                                      /* 0x220017f0 */
void sim1_air_motion(uint8_t *global);                                             /* 0x22008bd4, edi */
void sim1_rotor_terms(void);                                                       /* 0x22011f50 */
void sim1_ground_systems(uint8_t *global);                                         /* 0x2200ac8a, edi */
uint8_t *sim1_controls_pass(uint8_t *global);                                      /* 0x22001320 (and 0x22002260), edi in and out */
void sim1_jet_engine(uint8_t *global, uint8_t *engine);                            /* 0x2200769d, edi, ebx */
void sim1_jets(uint8_t *global);                                                   /* 0x22007530, edi */
void sim1_ground_or_turbulence(uint8_t *global);                                   /* 0x2200ac4a, edi */
void sim1_attitude_step(void);                                                     /* 0x220039d0 */
void sim1_piston_engine(uint8_t *global, uint8_t *engine, uint8_t *extra);         /* 0x22007ef1, edi, ebx, esi */
void sim1_rotor_blades(void);                                                      /* 0x220127b0 */
void sim1_aero_coefficients(void);                                                 /* 0x2200fe80 */
void sim1_pistons(uint8_t *global);                                                /* 0x22007e80, edi */
void sim1_local_flow(uint8_t *global);                                             /* 0x22001100, edi */
uint32_t __stdcall sim1_air_decode(const uint8_t *data, int32_t size);              /* 0x2200af90 */
void __stdcall sim1_engines_frame(uint32_t first, uint32_t second);                 /* 0x2200d790 */
uint32_t sim1_load_air(void);                                                      /* 0x2200ae90 */
void sim1_tail_rotor_blades(void);                                                 /* 0x22013b30 */
void sim1_heli_power(void);                                                       /* 0x220135f0 */
void sim1_rotor_frame(void);                                                      /* 0x22013340 */
uint8_t *sim1_simulation_step(uint8_t *edi);                                      /* 0x22002752, edi in and out */
uint8_t *sim1_tick_step(uint8_t *edi);                                              /* 0x2200ab16, edi in and out */
void __stdcall sim1_chain_tick(uint32_t first, uint32_t second);                    /* 0x2200a84b (and its wrapper 0x22001020), CHAIN list 1 */
uint32_t sim1_init_core(void);                                                     /* 0x22001060, Linkage +0x04 */
void __stdcall sim1_service_descriptor(uint8_t *out);                             /* 0x2200ac0d, Linkage +0x18 */
uint16_t __stdcall sim1_service_word(uint8_t *out);                                /* 0x2200ac2f, Linkage +0x28 */
uint32_t __stdcall sim1_service_scaled(double *out);                               /* 0x220117c0, Linkage +0x2c */
uint32_t __stdcall sim1_service_vector(double *a, double *b, double *c);           /* 0x220117e0, Linkage +0x30 */
uint32_t __stdcall sim1_service_reset(uint32_t value);                             /* 0x22011750, Linkage +0x34 */
long double sim1_service_wind(void);                                               /* 0x220096b8, Linkage +0x38 */

#endif
