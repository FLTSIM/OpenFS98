/* SIM1.sim's .data image, decompiled: every non-zero value of the file-backed part of retail's .data, at retail's offsets, one typed
 * value per macro.  Included by sim1.c (the harness DLLs' sim1_image) and sim1_native.c (sim1_native_image) as the initialiser of
 * their `data` member, which starts at .data+0x94 (past the ImportTable at +0x00 and the Linkage at +0x58, which those files write
 * themselves); so AT(off) takes retail's .data offset and subtracts 0x94, and every comment shows retail's offset.
 *
 * SIM1 is module 25, image base 0x22000000, reference SIM1.sim sha256
 * 66ebc70ff63fd6beb51a146b9fa2d1ce5d92a5cdff99af92960eed9e2ab68c1b (tools/ofs sim1-reference checks it).  Its sections: .rdata
 * 0x22015000 (0x759 bytes), .data 0x22016000 with virtual size 0x43f8 and 0x1200 raw bytes.  The ImportTable holds the ids
 * 1, 2, 23, 4, 16, 3, 32, 39, 20, 10 and a 0 terminator (HOST, GLOBAL, ACONTAIN, CHAIN, CONTROLS, MAIN, REALITY, SOUND, UTIL,
 * WINDOW) as (id, link) pairs; the Linkage has 15 words.  Retail's relocations put pointers in .data only at the Linkage's eleven
 * words (+0x5c, +0x60, +0x70..+0x90) and none in .rdata, so everything below is plain data.
 * `tools/ofs data-images` compares the image with retail (tools/check_data_image.py, row SIM1: 11 relocated, all ours).
 *
 * Regions follow docs/modules/sim1.md: the flags at +0x07c0, the conversion constants at +0x0680/+0x0690 and +0x0710..+0x0728, and the
 * thirteen interpolation tables sim1_table_lookup reads in this part of the file (a count, then packed (x, y) doubles, 4 bytes past a
 * multiple of 8), each labelled with its callers.  A value is a D64 only where sim1_small.c reads or writes that offset as a double
 * (SF64) and in a table; every other word is a U32 "meaning not yet known".  Runs of 16 or more zero bytes outside the tables are not
 * written.
 *
 * Past +0x1200 the loader zero-fills .data, and nothing is initialised here: the serialized state layout (sim1.md, "Saved state and
 * shutdown": doubles at +0x1328..+0x15a8, DWORDs at +0x1d18), the publication block at +0x1730 (0x24 bytes copied from GLOBAL +0x560,
 * then four words at +0x1754..+0x175a making the 0x2c-byte descriptor), the flag at +0x1778 and the restore dword at +0x1c80
 * (uint32_t, sim1_state.inc).  The Linkage's two data pointers point at +0x1150 and +0x1158 (VAs 0x22017150, 0x22017158), which lie in the zero part of this file.
 *
 * Macros: AT(off) designates retail's .data offset; U8/U16/U32 emit one little-endian value; D64 emits a double's bit pattern. */
#define AT(off) [(off) - 0x94] =
#define U8(x)  (uint8_t)(x)
#define U16(x) U8(x), U8((x) >> 8)
#define U32(x) U16(x), U16((x) >> 16)
#define D64(x) U32((uint32_t)(x)), U32((x) >> 32)  /* a double, as its bit pattern; the comment shows its value */

/* +0x0094  meaning not yet known; the file begins past the Linkage (+0x0058..+0x0093), which sim1.c and sim1_native.c write themselves */
AT(0x00bc) U32(0x00000001), U32(0x00000001), U32(0x00000001), U32(0x00000001),                                /* +0x00bc */
          U32(0x00000001), U32(0x00000001),                                                                   /* +0x00cc */
AT(0x00e8) U32(0x432d0000), U32(0x36e2eb1c), U32(0x432d3f7a), U32(0x36e2eb1c),                                /* +0x00e8 */
          U32(0x432d3f2a), U32(0x36e2eb1c), U32(0xa1cb3f7a), U32(0xfdf3b645),                                 /* +0x00f8 */
          U32(0xa1cb3fd4), U32(0xfdf3b645), U32(0xa1cb3fd4), U32(0xfdf3b645),                                 /* +0x0108 */
          U32(0x00003fd4), U32(0x00000000), U32(0x00000000), U32(0x00000000),                                 /* +0x0118 */
          D64(0x3e8514440056c1dcull), D64(0x3e69b2cddb725533ull),                                             /* +0x0128  0x1.514440056c1dcp-23 = 1.57052e-07; used as a double by sim1_simulation_step; 0x1.9b2cddb725533p-25 = 4.78671e-08; used as a double by sim1_simulation_step */
/* +0x0138  interpolation table (sim1_table_lookup): count 12, then 12 (x, y) doubles; called from sim1_engine_power */
          U32(0x0000000c),                                                                                    /* +0x0138 */
          D64(0x0000000000000000ull), D64(0x0000000000000000ull),                                             /* +0x013c  x, y = 0.0, 0.0 */
          D64(0x4024000000000000ull), D64(0x4024ae147ae147aeull),                                             /* +0x014c  x, y = 10.0, 10.34 */
          D64(0x4034000000000000ull), D64(0x4034ae147ae147aeull),                                             /* +0x015c  x, y = 20.0, 20.68 */
          D64(0x403e000000000000ull), D64(0x4044ae147ae147aeull),                                             /* +0x016c  x, y = 30.0, 41.36 */
          D64(0x4044000000000000ull), D64(0x405217ae147ae148ull),                                             /* +0x017c  x, y = 40.0, 72.37 */
          D64(0x4049000000000000ull), D64(0x405db9999999999aull),                                             /* +0x018c  x, y = 50.0, 118.9 */
          D64(0x404e000000000000ull), D64(0x4067433333333333ull),                                             /* +0x019c  x, y = 60.0, 186.1 */
          D64(0x4051800000000000ull), D64(0x4072180000000000ull),                                             /* +0x01ac  x, y = 70.0, 289.5 */
          D64(0x4054000000000000ull), D64(0x4080380000000000ull),                                             /* +0x01bc  x, y = 80.0, 519.0 */
          D64(0x4056800000000000ull), D64(0x408f000000000000ull),                                             /* +0x01cc  x, y = 90.0, 992.0 */
          D64(0x4059000000000000ull), D64(0x409f780000000000ull),                                             /* +0x01dc  x, y = 100.0, 2014.0 */
          D64(0x405b800000000000ull), D64(0x40a4fa0000000000ull),                                             /* +0x01ec  x, y = 110.0, 2685.0 */
          U32(0x00000000), U32(0x00000000), U32(0x3ff00000), U32(0x00000000),                                 /* +0x01fc */
          U32(0x402c0000), U32(0x00000000), U32(0x402c0000),                                                  /* +0x020c */
AT(0x02d8) U32(0xffffffff), U32(0x00000000), U32(0x00000001), U32(0x20000000),                                /* +0x02d8 */
          U32(0xffffffff), U32(0x20000000), U32(0xffffffff), U32(0x00000000),                                 /* +0x02e8 */
          U32(0xffffffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),                                 /* +0x02f8 */
          U32(0xffffffff), U32(0x00000000), U32(0x00000001), U32(0x20000000),                                 /* +0x0308 */
          U32(0xffffffff), U32(0x20000000), U32(0xffffffff), U32(0x00000000),                                 /* +0x0318 */
          U32(0xffffffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),                                 /* +0x0328 */
          U32(0xffffffff), U32(0x00000000), U32(0x00000001), U32(0x20000000),                                 /* +0x0338 */
          U32(0xffffffff), U32(0x20000000), U32(0xffffffff), U32(0x00000000),                                 /* +0x0348 */
          U32(0xffffffff), U32(0x00000000), U32(0x00000000), U32(0x00000000),                                 /* +0x0358 */
          U32(0x08000000), U32(0x28001400), U32(0x40004000), U32(0x40004000),                                 /* +0x0368 */
          U32(0xc0004000), U32(0xc000c000), U32(0xd800c000), U32(0xf800ec00),                                 /* +0x0378 */
          U32(0x00000000), U32(0x00000000), U32(0x00004018),                                                  /* +0x0388 */
AT(0x03b8) U32(0x0000401c), U32(0x00000000), D64(0xbfc3333333333333ull),                                      /* +0x03b8  -0x1.3333333333333p-3 = -0.15; used as a double by sim1_pitch_moment */
          D64(0x4097700000000000ull), D64(0x3ff0000000000000ull),                                             /* +0x03c8  0x1.7700000000000p+10 = 1500.0; used as a double by sim1_pitch_sum_moment; 0x1.0000000000000p+0 = 1.0; used as a double by sim1_angle_follow */
          D64(0x3fbeb851eb851eb8ull), D64(0xbf7571ed3c506b3aull),                                             /* +0x03d8  0x1.eb851eb851eb8p-4 = 0.12; used as a double by sim1_angle_follow; -0x1.571ed3c506b3ap-8 = -0.005235602094240838; used as a double by sim1_angle_follow */
          D64(0x3f81def05cedaeb0ull), D64(0xbfc656ac74291a5cull),                                             /* +0x03e8  0x1.1def05cedaeb0p-7 = 0.008726003490401396; used as a double by sim1_angle_follow; -0x1.656ac74291a5cp-3 = -0.17452006980802792; used as a double by sim1_angle_follow */
          D64(0xbfc656ac74291a5cull), D64(0xbfd656ac74291a5cull),                                             /* +0x03f8  -0x1.656ac74291a5cp-3 = -0.17452006980802792; used as a double by sim1_angle_follow; -0x1.656ac74291a5cp-2 = -0.34904013961605584; used as a double by sim1_angle_follow */
          D64(0x3fe03209d4376650ull), D64(0x403e000000000000ull),                                             /* +0x0408  0x1.03209d4376650p-1 = 0.506108202443281; used as a double by sim1_angle_follow; 0x1.e000000000000p+4 = 30.0; used as a double by sim1_angle_follow */
          D64(0x401199999999999aull), D64(0x4072c00000000000ull),                                             /* +0x0418  0x1.199999999999ap+2 = 4.4; used as a double by sim1_engine_ratio; 0x1.2c00000000000p+8 = 300.0; used as a double by sim1_engine_ratio */
          D64(0x3f34b213b8ef7543ull), D64(0x4011666666666666ull),                                             /* +0x0428  0x1.4b213b8ef7543p-12 = 0.00031579; used as a double by sim1_engine_ratio; 0x1.1666666666666p+2 = 4.35; used as a double by sim1_engine_ratio */
AT(0x0448) U32(0x00000200),                                                                                   /* +0x0448 */
AT(0x04a4) U32(0x2b3d0000), U32(0x25300000), U32(0x86a00000), U32(0x4e200001),                                /* +0x04a4 */
          U32(0x07080000), U32(0x00000000), U32(0xf8f5ffff), U32(0xc8b4e546),                                 /* +0x04b4 */
          U32(0x873da7f0), U32(0x50726991), U32(0x2c8e3c3a), U32(0x17f120b8),                                 /* +0x04c4 */
          U32(0x0cd41183), U32(0x06f8096f), U32(0x0000052e), U32(0x04180106),                                 /* +0x04d4 */
          U32(0x10620937), U32(0x24dd1999), U32(0x4189322d), U32(0x666652f1),                                 /* +0x04e4 */
          U32(0x93747be6), U32(0xc8b3ad0d), U32(0xffffe665), U32(0x00010600),                                 /* +0x04f4 */
AT(0x051c) U32(0x00407f40), U32(0x00000000), U32(0x00000000), U32(0x00000000),                                /* +0x051c */
          U32(0xff000000), U32(0x007fffff), U32(0x00000000), U32(0x00000000),                                 /* +0x052c */
          U32(0xff000000), U32(0x007fffff), U32(0x00000000), U32(0x00000000),                                 /* +0x053c */
          U32(0xff000000), U32(0x007fffff), U32(0x00000000), U32(0x00000000),                                 /* +0x054c */
          U32(0xff000000), U32(0x007fffff), U32(0x00000000), U32(0x33000000),                                 /* +0x055c */
          U32(0x33333333), U32(0x104059d3), U32(0x3eab367a), U32(0xfa3f9b57),                                 /* +0x056c */
          U32(0x74bc6a7e), U32(0x003f7893), U32(0x00000000), U32(0x00000000),                                 /* +0x057c */
          U32(0x00000000), U32(0x00011126), U32(0x5579af19), U32(0x3fc0b567),                                 /* +0x058c */
          U32(0x00000000), U32(0x409ad000), U32(0xcccccccd), U32(0x3ffccccc),                                 /* +0x059c */
          U32(0x00000000), U32(0x40400000), U32(0x33333333), U32(0x407cbb33),                                 /* +0x05ac */
          U32(0x1eb851ec), U32(0x40803585), U32(0x27ababf0), U32(0x3f5f9657),                                 /* +0x05bc */
          U32(0x66666666), U32(0x40a08866), U32(0xc0193c0f), U32(0x3f3ef7fd),                                 /* +0x05cc */
          U32(0x7881f1dd), U32(0x3f6378b8), U32(0xf9f7a843), U32(0x407a4b74),                                 /* +0x05dc */
          U32(0x66666666), U32(0x3ff66666),                                                                   /* +0x05ec */
AT(0x064c) U32(0x55550000), U32(0xffffabb9), U32(0x00000600), U32(0x00000000),                                /* +0x064c */
          U32(0x00000000), U32(0x00000000), U32(0x999a40d4), U32(0x99999999),                                 /* +0x065c */
          U32(0x00003f09), U32(0x00000000), U32(0x00000000), D64(0x40c45f306dc5d639ull),                      /* +0x066c  0x1.45f306dc5d639p+13 = 10430.37835; used as a double by sim1_air_motion, sim1_attitude_step, sim1_simulation_step, ... */
/* +0x0680  conversion constants (sim1.md, "Legacy loading"): +0x0680 = 9.5873e-05, +0x0688 = 57.29578, +0x0690 = 1.52590219e-05; the scale product +0x0690 * +0x0680 stays on the x87 stack */
          D64(0x3f1921ed9928b90full), D64(0x404ca5dc1a5d2372ull),                                             /* +0x0680  0x1.921ed9928b90fp-14 = 9.5873e-05; used as a double by sim1_aero_coefficients, sim1_air_motion, sim1_aircraft_setup, ...; 0x1.ca5dc1a5d2372p+5 = 57.29577951; used as a double by sim1_aero_coefficients, sim1_heading_term, sim1_heli_power, ... */
          D64(0x3ef00010001ee0d4ull), D64(0x3f000020004903f6ull),                                             /* +0x0690  0x1.00010001ee0d4p-16 = 1.52590219e-05; used as a double by sim1_aero_coefficients, sim1_angle_follow, sim1_attitude_trig, ...; 0x1.00020004903f6p-15 = 3.051850948e-05; used as a double by sim1_chain_tick, sim1_climb_history, sim1_contact_terms, ... */
          D64(0x3f10000000000000ull), U32(0x0380be01), U32(0x3f3000ff),                                       /* +0x06a0  0x1.0000000000000p-14 = 6.103515625e-05; used as a double by sim1_flaps, sim1_rotor_mixing, sim1_spoilers, ... */
          D64(0x3f400200400edbccull), U32(0x01f1fde6), U32(0x3f600804),                                       /* +0x06b0  0x1.00200400edbccp-11 = 0.0004885197851; used as a double by sim1_aero_coefficients, sim1_table_scaled, sim1_table_term */
          D64(0x3f70101010082107ull), D64(0x3f8020408101bd1cull),                                             /* +0x06c0  0x1.0101010082107p-8 = 0.003921568627; used as a double by sim1_air_data, sim1_air_decode, sim1_air_motion, ...; 0x1.020408101bd1cp-7 = 0.007874015748; used as a double by sim1_air_motion, sim1_climb_history, sim1_ground_or_turbulence */
          D64(0x3f847ae147ae147bull), D64(0x40efffe000000000ull),                                             /* +0x06d0  0x1.47ae147ae147bp-7 = 0.01; used as a double by sim1_part_limit; 0x1.fffe000000000p+15 = 65535.0; used as a double by sim1_air_data, sim1_attitude_step, sim1_bearing_of, ... */
          D64(0x40dfffc000000000ull), D64(0x40cfff8000000000ull),                                             /* +0x06e0  0x1.fffc000000000p+14 = 32767.0; used as a double by sim1_air_motion, sim1_attitude_trig, sim1_engines_pass, ...; 0x1.fff8000000000p+13 = 16383.0; used as a double by sim1_flaps, sim1_landing_gear, sim1_rotor_frame, ... */
          U32(0x00000000), U32(0x409ffc00), D64(0x406fe00000000000ull),                                       /* +0x06f0  0x1.fe00000000000p+7 = 255.0; used as a double by sim1_air_motion, sim1_yaw_rate */
          D64(0x405fc00000000000ull), U32(0x00000000), U32(0x40590000),                                       /* +0x0700  0x1.fc00000000000p+6 = 127.0; used as a double by sim1_air_motion */
/* +0x0710  +0x0710 = 3.28083 and +0x0718 = 0.3048; +0x0720 = 3.28083 multiplies body velocities first, then +0x0690; +0x0728 = 0.3048 is the scale publication reads (sim1.md) */
          D64(0x400a3f23cc8de2acull), U32(0xdbf487fd), U32(0x3fd381d7),                                       /* +0x0710  0x1.a3f23cc8de2acp+1 = 3.28083; used as a double by sim1_height, sim1_part_limit */
          D64(0x400a3f23cc8de2acull), D64(0x3fd381d7dbf487fdull),                                             /* +0x0720  0x1.a3f23cc8de2acp+1 = 3.28083; used as a double by sim1_air_motion, sim1_ground_bumps, sim1_hold_check, ...; 0x1.381d7dbf487fdp-2 = 0.3048; used as a double by sim1_air_data, sim1_air_motion, sim1_chain_tick, ... */
          U32(0x40639d5e), U32(0x403356f6), D64(0x3fb55554c62af21cull),                                       /* +0x0730  0x1.55554c62af21cp-4 = 0.0833333; used as a double by sim1_aero_coefficients, sim1_contact_offsets, sim1_contact_rise, ... */
          D64(0x3ffb024696bd4647ull), D64(0x3fe2f4ebcfc75310ull),                                             /* +0x0740  0x1.b024696bd4647p+0 = 1.6880556; used as a double by sim1_aero_coefficients, sim1_ground_or_turbulence, sim1_prop_pitch, ...; 0x1.2f4ebcfc75310p-1 = 0.5923976; used as a double by sim1_air_motion, sim1_band_value, sim1_governor_pass, ... */
          U32(0x00000000), U32(0x40ac2000), D64(0x3f32345623163d56ull),                                       /* +0x0750  0x1.2345623163d56p-12 = 0.0002777777; used as a double by sim1_part_timers */
          D64(0x3fbacee9f3664e1aull), D64(0x3fe0000000000000ull),                                             /* +0x0760  0x1.acee9f3664e1ap-4 = 0.1047197551; used as a double by sim1_pitch_sum_moment, sim1_roll_moment; 0x1.0000000000000p-1 = 0.5; used as a double by sim1_aero_coefficients, sim1_air_motion, sim1_attitude_step, ... */
          D64(0x4000000000000000ull), U32(0xffc00000), U32(0x41dfffff),                                       /* +0x0770  0x1.0000000000000p+1 = 2.0; used as a double by sim1_angle_follow, sim1_attitude_step, sim1_wrap */
          U32(0xffc00000), U32(0xc1dfffff), D64(0x40401645a1cac083ull),                                       /* +0x0780  0x1.01645a1cac083p+5 = 32.174; used as a double by sim1_stall_warning, sim1_wheel_force, sim1_yaw_rate */
          U32(0x00000001), U32(0x00000000), D64(0x3f6378b87881f1ddull),                                       /* +0x0790  0x1.378b87881f1ddp-9 = 0.0023769; used as a double by sim1_aero_coefficients, sim1_air_data, sim1_air_motion, ... */
          D64(0x3ff0000000000000ull), D64(0x3ff0000000000000ull),                                             /* +0x07a0  0x1.0000000000000p+0 = 1.0; used as a double by sim1_air_data, sim1_engine_power, sim1_heli_power, ...; 0x1.0000000000000p+0 = 1.0; used as a double by sim1_air_data, sim1_engine_power, sim1_heli_power, ... */
          D64(0x3ff0000000000000ull), D64(0x3ff0000000000000ull),                                             /* +0x07b0  0x1.0000000000000p+0 = 1.0; used as a double by sim1_engine_power, sim1_power_7b8; 0x1.0000000000000p+0 = 1.0; used as a double by sim1_engine_power, sim1_power_7b8, sim1_prop_efficiency, ... */
/* +0x07c0  the flags: +0x07c0 is the selector (1 takes one velocity convention, any other value the other: sim1.md, "Legacy loading"); the words after it are not yet known */
          U32(0x00000001),                                                                                    /* +0x07c0 */
AT(0x07dc) U32(0x47ae147b), U32(0x3f847ae1),                                                                  /* +0x07dc */
AT(0x0818) U32(0x34533400), U32(0xe4490c53), U32(0xe43ee43e), U32(0xe43ee43e),                                /* +0x0818 */
          U32(0xe43ee43e), U32(0xe43ee43e), U32(0xe43ee43e), U32(0xe43ee43e),                                 /* +0x0828 */
          U32(0xe43ee43e), U32(0xe43ee43e), U32(0xe43ee43e), U32(0xe43ee43e),                                 /* +0x0838 */
          U32(0xe43ee43e), U32(0xe43ee43e), U32(0x34490c3e), U32(0x00533453),                                 /* +0x0848 */
AT(0x0880) U32(0x00080000),                                                                                   /* +0x0880 */
AT(0x0894) U32(0x7b740000), U32(0xbeb46349), U32(0x00004026), U32(0x00000000),                                /* +0x0894 */
          U32(0x1eb84026), U32(0xb851eb85), U32(0x00003f9e), U32(0x00000000),                                 /* +0x08a4 */
          U32(0x00000000), U32(0x999a0000), U32(0x99999999), U32(0x999a3fe9),                                 /* +0x08b4 */
          U32(0x99999999), U32(0x00003fd9),                                                                   /* +0x08c4 */
AT(0x08e0) U32(0x00000006),                                                                                   /* +0x08e0 */
AT(0x0910) U32(0x00000001),                                                                                   /* +0x0910 */
AT(0x0928) D64(0x4066c16bb98c7e28ull), D64(0x3f767ffffbb47d06ull),                                            /* +0x0928  0x1.6c16bb98c7e28p+7 = 182.0444; used as a double by sim1_altitude_capture, sim1_coupler, sim1_glideslope, ...; 0x1.67ffffbb47d06p-8 = 0.005493164; used as a double by sim1_coupler, sim1_glideslope, sim1_glideslope_capture, ... */
          U32(0x45847726), U32(0x3effffd8), U32(0x740a93d8), U32(0x3fff1937),                                 /* +0x0938 */
          D64(0x404e000000000000ull),                                                                         /* +0x0948  0x1.e000000000000p+5 = 60.0; used as a double by sim1_altitude_capture */
AT(0x0960) D64(0x3fa9afcbb51fb1f7ull), U32(0x00000000), U32(0x00000000),                                      /* +0x0960  0x1.9afcbb51fb1f7p-5 = 0.05016934; used as a double by sim1_climb_history */
          U32(0xd8000000), U32(0xb4390002), U32(0x9fbe76c8), U32(0x00003f9a),                                 /* +0x0970 */
AT(0x0a00) U32(0x403e0000), U32(0xeaac1554), U32(0xe3901c70),                                                 /* +0x0a00 */
AT(0x0ac0) U32(0x33000000), U32(0x0a000003), U32(0x63000000),                                                 /* +0x0ac0 */
AT(0x0ae0) U32(0xfc000000), U32(0x00026191), U32(0x28000000), U32(0x00000000),                                /* +0x0ae0 */
          U32(0x00000000), U32(0xd0000000), U32(0xc0000007), U32(0x00002db7),                                 /* +0x0af0 */
          U32(0x00000000), U32(0x00000000), U32(0x00000000), U32(0x80000032),                                 /* +0x0b00 */
          U32(0x8000001b), U32(0x00000002), U32(0x00000000), U32(0x00400000),                                 /* +0x0b10 */
AT(0x0b34) U32(0x2c000000), U32(0x00000001), U32(0xfe000000), U32(0x00000084),                                /* +0x0b34 */
          U32(0x00000000), U32(0x00405680), U32(0x00000000), D64(0x3fe0c101571ed3c5ull),                      /* +0x0b44  0x1.0c101571ed3c5p-1 = 0.5235602094240838; used as a double by sim1_aero_coefficients, sim1_fold_deflection */
          D64(0x3fd999999999999aull),                                                                         /* +0x0b58  0x1.999999999999ap-2 = 0.4; used as a double by sim1_aero_coefficients */
/* +0x0b60  interpolation table (sim1_table_lookup): count 10, then 10 (x, y) doubles; called from sim1_prop_efficiency, sim1_prop_thrust */
          U32(0x0000000a),                                                                                    /* +0x0b60 */
          D64(0x0000000000000000ull), D64(0x3ff0000000000000ull),                                             /* +0x0b64  x, y = 0.0, 1.0 */
          D64(0x3fb999999999999aull), D64(0x3fef7ced916872b0ull),                                             /* +0x0b74  x, y = 0.1, 0.984 */
          D64(0x3fc999999999999aull), D64(0x3feef9db22d0e560ull),                                             /* +0x0b84  x, y = 0.2, 0.968 */
          D64(0x3fd3333333333333ull), D64(0x3fee7ef9db22d0e5ull),                                             /* +0x0b94  x, y = 0.3, 0.953 */
          D64(0x3fd999999999999aull), D64(0x3fee147ae147ae14ull),                                             /* +0x0ba4  x, y = 0.4, 0.94 */
          D64(0x3fe0000000000000ull), D64(0x3feda9fbe76c8b44ull),                                             /* +0x0bb4  x, y = 0.5, 0.927 */
          D64(0x3fe3333333333333ull), D64(0x3fed4fdf3b645a1dull),                                             /* +0x0bc4  x, y = 0.6, 0.916 */
          D64(0x3fe6666666666666ull), D64(0x3fecf5c28f5c28f6ull),                                             /* +0x0bd4  x, y = 0.7, 0.905 */
          D64(0x3fe999999999999aull), D64(0x3feca3d70a3d70a4ull),                                             /* +0x0be4  x, y = 0.8, 0.895 */
          D64(0x3feccccccccccccdull), D64(0x3fec51eb851eb852ull),                                             /* +0x0bf4  x, y = 0.9, 0.885 */
          U32(0x00000000),                                                                                    /* +0x0c04 */
/* +0x0c08  interpolation table (sim1_table_lookup): count 6, then 6 (x, y) doubles; called from sim1_rotor_terms */
          U32(0x00000006),                                                                                    /* +0x0c08 */
          D64(0x406738f5c28f5c29ull), D64(0x0000000000000000ull),                                             /* +0x0c0c  x, y = 185.78, 0.0 */
          D64(0x40695570a3d70a3dull), D64(0x3faeb851eb851eb8ull),                                             /* +0x0c1c  x, y = 202.67, 0.06 */
          D64(0x406b71eb851eb852ull), D64(0x3fc70a3d70a3d70aull),                                             /* +0x0c2c  x, y = 219.56, 0.18 */
          D64(0x406d8e147ae147aeull), D64(0x3fd3d70a3d70a3d7ull),                                             /* +0x0c3c  x, y = 236.44, 0.31 */
          D64(0x406faa8f5c28f5c3ull), D64(0x3fe0000000000000ull),                                             /* +0x0c4c  x, y = 253.33, 0.5 */
          D64(0x4070e3851eb851ecull), D64(0x3fe8f5c28f5c28f6ull),                                             /* +0x0c5c  x, y = 270.22, 0.78 */
          U32(0x00000000),                                                                                    /* +0x0c6c */
/* +0x0c70  interpolation table (sim1_table_lookup): count 3, then 3 (x, y) doubles; called from sim1_rotor_terms */
          U32(0x00000003),                                                                                    /* +0x0c70 */
          D64(0xc031000000000000ull), D64(0x0000000000000000ull),                                             /* +0x0c74  x, y = -17.0, 0.0 */
          D64(0x0000000000000000ull), D64(0x40b1e40000000000ull),                                             /* +0x0c84  x, y = 0.0, 4580.0 */
          D64(0x4040e28f5c28f5c3ull), D64(0x0000000000000000ull),                                             /* +0x0c94  x, y = 33.77, 0.0 */
          U32(0x00000000),                                                                                    /* +0x0ca4 */
/* +0x0ca8  interpolation table (sim1_table_lookup): count 3, then 3 (x, y) doubles; called from sim1_rotor_terms */
          U32(0x00000003),                                                                                    /* +0x0ca8 */
          D64(0xbffb333333333333ull), D64(0x0000000000000000ull),                                             /* +0x0cac  x, y = -1.7, 0.0 */
          D64(0x0000000000000000ull), D64(0x0000000000000000ull),                                             /* +0x0cbc  x, y = 0.0, 0.0 */
          D64(0x3ffb333333333333ull), D64(0x4024800000000000ull),                                             /* +0x0ccc  x, y = 1.7, 10.25 */
          U32(0x00000000),                                                                                    /* +0x0cdc */
/* +0x0ce0  interpolation table (sim1_table_lookup): count 3, then 3 (x, y) doubles; called from sim1_rotor_terms */
          U32(0x00000003),                                                                                    /* +0x0ce0 */
          D64(0xc031000000000000ull), D64(0x0000000000000000ull),                                             /* +0x0ce4  x, y = -17.0, 0.0 */
          D64(0x0000000000000000ull), D64(0x4069000000000000ull),                                             /* +0x0cf4  x, y = 0.0, 200.0 */
          D64(0x4040e28f5c28f5c3ull), D64(0x0000000000000000ull),                                             /* +0x0d04  x, y = 33.77, 0.0 */
          U32(0x00000000),                                                                                    /* +0x0d14 */
/* +0x0d18  interpolation table (sim1_table_lookup): count 3, then 3 (x, y) doubles; called from sim1_rotor_terms */
          U32(0x00000003),                                                                                    /* +0x0d18 */
          D64(0xc031000000000000ull), D64(0x0000000000000000ull),                                             /* +0x0d1c  x, y = -17.0, 0.0 */
          D64(0x0000000000000000ull), D64(0xc06f400000000000ull),                                             /* +0x0d2c  x, y = 0.0, -250.0 */
          D64(0x4040e28f5c28f5c3ull), D64(0x0000000000000000ull),                                             /* +0x0d3c  x, y = 33.77, 0.0 */
          U32(0x00000000),                                                                                    /* +0x0d4c */
/* +0x0d50  interpolation table (sim1_table_lookup): count 3, then 3 (x, y) doubles; called from sim1_rotor_terms */
          U32(0x00000003),                                                                                    /* +0x0d50 */
          D64(0xc025666666666666ull), D64(0x4087740000000000ull),                                             /* +0x0d54  x, y = -10.7, 750.5 */
          D64(0x0000000000000000ull), D64(0x0000000000000000ull),                                             /* +0x0d64  x, y = 0.0, 0.0 */
          D64(0x3ffb333333333333ull), D64(0xc04d400000000000ull),                                             /* +0x0d74  x, y = 1.7, -58.5 */
          U32(0x00000000), D64(0x4010000000000000ull),                                                        /* +0x0d84  0x1.0000000000000p+2 = 4.0; used as a double by sim1_rotor_blades */
/* +0x0d90  interpolation table (sim1_table_lookup): count 10, then 10 (x, y) doubles; called from sim1_rotor_forces */
          U32(0x0000000a),                                                                                    /* +0x0d90 */
          D64(0x0000000000000000ull), D64(0x3feeb851eb851eb8ull),                                             /* +0x0d94  x, y = 0.0, 0.96 */
          D64(0x4040e28f5c28f5c3ull), D64(0x3ff428f5c28f5c29ull),                                             /* +0x0da4  x, y = 33.77, 1.26 */
          D64(0x4050e3d70a3d70a4ull), D64(0x3ff4f5c28f5c28f6ull),                                             /* +0x0db4  x, y = 67.56, 1.31 */
          D64(0x4059551eb851eb85ull), D64(0x3ff5eb851eb851ecull),                                             /* +0x0dc4  x, y = 101.33, 1.37 */
          D64(0x4060e3851eb851ecull), D64(0x3ff4f5c28f5c28f6ull),                                             /* +0x0dd4  x, y = 135.11, 1.31 */
          D64(0x40651c7ae147ae14ull), D64(0x3ff35c28f5c28f5cull),                                             /* +0x0de4  x, y = 168.89, 1.21 */
          D64(0x406738f5c28f5c29ull), D64(0x3ff23d70a3d70a3dull),                                             /* +0x0df4  x, y = 185.78, 1.14 */
          D64(0x40695570a3d70a3dull), D64(0x3ff0f5c28f5c28f6ull),                                             /* +0x0e04  x, y = 202.67, 1.06 */
          D64(0x406b71eb851eb852ull), D64(0x3fef5c28f5c28f5cull),                                             /* +0x0e14  x, y = 219.56, 0.98 */
          D64(0x406d8e147ae147aeull), D64(0x3fec28f5c28f5c29ull),                                             /* +0x0e24  x, y = 236.44, 0.88 */
          U32(0x00000000), D64(0x4070c147ae147ae1ull), D64(0x4044ae147ae147aeull),                            /* +0x0e34  0x1.0c147ae147ae1p+8 = 268.08; used as a double by sim1_heli_power, sim1_tail_rotor_blades; 0x1.4ae147ae147aep+5 = 41.36; used as a double by sim1_heli_power, sim1_rotor_blades, sim1_rotor_terms */
          U32(0xc906404c), U32(0x3f0fffff),                                                                   /* +0x0e48 */
/* +0x0e50  interpolation table (sim1_table_lookup): count 6, then 6 (x, y) doubles; called from sim1_heli_power */
          U32(0x00000006),                                                                                    /* +0x0e50 */
          D64(0x404f000000000000ull), D64(0x0000000000000000ull),                                             /* +0x0e54  x, y = 62.0, 0.0 */
          D64(0x405b800000000000ull), D64(0x4053a66666666666ull),                                             /* +0x0e64  x, y = 110.0, 78.6 */
          D64(0x4063000000000000ull), D64(0x4064300000000000ull),                                             /* +0x0e74  x, y = 152.0, 161.5 */
          D64(0x4068800000000000ull), D64(0x406e033333333333ull),                                             /* +0x0e84  x, y = 196.0, 240.1 */
          D64(0x406ac00000000000ull), D64(0x40714b3333333333ull),                                             /* +0x0e94  x, y = 214.0, 276.7 */
          D64(0x406ee9999999999aull), D64(0x40758b3333333333ull),                                             /* +0x0ea4  x, y = 247.3, 344.7 */
          U32(0x00000000),                                                                                    /* +0x0eb4 */
/* +0x0eb8  interpolation table (sim1_table_lookup): count 6, then 6 (x, y) doubles; called from sim1_heli_power */
          U32(0x00000006),                                                                                    /* +0x0eb8 */
          D64(0x0000000000000000ull), D64(0x404f000000000000ull),                                             /* +0x0ebc  x, y = 0.0, 62.0 */
          D64(0x4053a66666666666ull), D64(0x405b800000000000ull),                                             /* +0x0ecc  x, y = 78.6, 110.0 */
          D64(0x4064300000000000ull), D64(0x4063000000000000ull),                                             /* +0x0edc  x, y = 161.5, 152.0 */
          D64(0x406e033333333333ull), D64(0x4068800000000000ull),                                             /* +0x0eec  x, y = 240.1, 196.0 */
          D64(0x40714b3333333333ull), D64(0x406ac00000000000ull),                                             /* +0x0efc  x, y = 276.7, 214.0 */
          D64(0x40758b3333333333ull), D64(0x406ee9999999999aull),                                             /* +0x0f0c  x, y = 344.7, 247.3 */
          U32(0x00000000),                                                                                    /* +0x0f1c */
/* +0x0f20  interpolation table (sim1_table_lookup): count 6, then 6 (x, y) doubles; called from sim1_heli_power */
          U32(0x00000006),                                                                                    /* +0x0f20 */
          D64(0x404f000000000000ull), D64(0x40e0108000000000ull),                                             /* +0x0f24  x, y = 62.0, 32900.0 */
          D64(0x405b800000000000ull), D64(0x40e42a8000000000ull),                                             /* +0x0f34  x, y = 110.0, 41300.0 */
          D64(0x4063000000000000ull), D64(0x40e6508000000000ull),                                             /* +0x0f44  x, y = 152.0, 45700.0 */
          D64(0x4068800000000000ull), D64(0x40e7c78000000000ull),                                             /* +0x0f54  x, y = 196.0, 48700.0 */
          D64(0x406ac00000000000ull), D64(0x40e81f0000000000ull),                                             /* +0x0f64  x, y = 214.0, 49400.0 */
          D64(0x406ee00000000000ull), D64(0x40e8c18000000000ull),                                             /* +0x0f74  x, y = 247.0, 50700.0 */
          U32(0x00000000),                                                                                    /* +0x0f84 */
/* +0x0f88  interpolation table (sim1_table_lookup): count 6, then 6 (x, y) doubles; called from sim1_heli_power */
          U32(0x00000006),                                                                                    /* +0x0f88 */
          D64(0x40e0108000000000ull), D64(0x407b600000000000ull),                                             /* +0x0f8c  x, y = 32900.0, 438.0 */
          D64(0x40e42a8000000000ull), D64(0x4080326666666666ull),                                             /* +0x0f9c  x, y = 41300.0, 518.3 */
          D64(0x40e6508000000000ull), D64(0x4081d8cccccccccdull),                                             /* +0x0fac  x, y = 45700.0, 571.1 */
          D64(0x40e7c78000000000ull), D64(0x4084300000000000ull),                                             /* +0x0fbc  x, y = 48700.0, 646.0 */
          D64(0x40e81f0000000000ull), D64(0x4084f00000000000ull),                                             /* +0x0fcc  x, y = 49400.0, 670.0 */
          D64(0x40e8c18000000000ull), D64(0x4086500000000000ull),                                             /* +0x0fdc  x, y = 50700.0, 714.0 */
          U32(0x00000000), U32(0x00000000), U32(0x40e47580), D64(0x4064400000000000ull),                      /* +0x0fec  0x1.4400000000000p+7 = 162.0; used as a double by sim1_heli_power */
          U32(0xe147ae14), U32(0x408b227a), D64(0x4010000000000000ull),                                       /* +0x1000  0x1.0000000000000p+2 = 4.0; used as a double by sim1_tail_rotor_blades */

#undef U8
#undef U16
#undef U32
#undef D64
#undef AT
