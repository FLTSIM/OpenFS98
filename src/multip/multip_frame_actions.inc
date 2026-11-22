/* CHAIN 0x14's compressed switch (retail: byte table at RVA 0x3c90, jump table at 0x3c68). The five jump
 * targets map to the recovered actions 0x3c63 -> 0 (default), 0x3c2b -> 1, 0x3c3d -> 2, 0x3c4c -> 3, 0x3c59 -> 4.
 * Actions: 0=default, 1=aircraft, 2=collision, 3=block, 4=unblock. */
static const uint8_t s_frame_actions[64] = {
    2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,3,
    4,3,4,3,4,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
};
