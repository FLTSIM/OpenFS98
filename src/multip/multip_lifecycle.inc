/* Session callback ownership and teardown. Public lifecycle stays guarded. */
static uint32_t FS98_STDCALL multip_receive_callback(uint32_t code, uint32_t argument);

/* .text+0x27c0 -- flag is set even if the first registration fails. */
int32_t multip_receive_callbacks_start(void)
{
    M32(0x950) = 1;
    if (CHAIN_FN(0x18, multip_chain_register_fn)(1, multip_receive_callback) != 0) return -1;
    if (CHAIN_FN(0x18, multip_chain_register_fn)(9, multip_receive_callback) != 0) return -1;
    return CHAIN_FN(0x18, multip_chain_register_fn)(1, multip_control_callback) != 0 ? -1 : 0;
}

/* .text+0x2810 -- partial removal retains the owner flag. */
int32_t multip_receive_callbacks_stop(void)
{
    if (M32(0x950) != 0) {
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(1, multip_control_callback) != 0) return -1;
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(9, multip_receive_callback) != 0) return -1;
        if (CHAIN_FN(0x1c, multip_chain_register_fn)(1, multip_receive_callback) != 0) return -1;
        M32(0x950) = 0;
    }
    return 0;
}

/* .text+0x8220/+0x8270 -- abort at the first failed owner, without rollback. */
int32_t multip_callbacks_start(void)
{
    if (multip_session_frame_start() != 0) return -1;
    if (multip_tick_start() != 0) return -1;
    if (multip_players_draw_start() != 0) return -1;
    if (multip_receive_callbacks_start() != 0) return -1;
    if (multip_commands_start() != 0) return -1;
    return multip_selection_frame_start() != 0 ? -1 : 0;
}
int32_t multip_callbacks_stop(void)
{
    if (multip_selection_frame_stop() != 0) return -1;
    if (multip_commands_stop() != 0) return -1;
    if (multip_receive_callbacks_stop() != 0) return -1;
    if (multip_players_draw_stop() != 0) return -1;
    if (multip_tick_stop() != 0) return -1;
    return multip_session_frame_stop() != 0 ? -1 : 0;
}

typedef void (FS98_STDCALL *multip_progress_open_fn)(void **, uint32_t,
    uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (FS98_STDCALL *multip_progress_close_fn)(void **);
typedef int32_t (FS98_STDCALL *multip_dp_close_fn)(void *);
typedef int32_t (FS98_STDCALL *multip_dp_destroy_player_fn)(void *, uint32_t);

/* .text+0x8cf0 -- early failures intentionally leave partial teardown behind. */
int32_t multip_session_disconnect(void)
{
    uint32_t start;
    int32_t delay, result = 0;
    void *progress;

    if (M32(M_CONNECTED) == 0) return 0;
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 2, NULL);
    if (M32(0xaa8) != 0 && multip_message_1012() != 0) return -1;
    start = HOST_FN(0x38, multip_ticks_fn)();
    UTIL_FN(0xa8, multip_progress_open_fn)(&progress, 1, 0xf53f, 0, 0, 0, 0, 0);
    if (multip_send_drain_all() != 0) return -1;
    if (multip_queue_clear((struct multip_queue *)(uintptr_t)M32(M_RECEIVE_QUEUE)) != 0) return -1;
    delay = (int32_t)(HOST_FN(0x38, multip_ticks_fn)() - start + 3000);
    if (delay > 0) Sleep((uint32_t)delay);
    UTIL_FN(0xac, multip_progress_close_fn)(&progress);
    if (multip_callbacks_stop() != 0) return -1;
    if (multip_workers_stop() != 0) return -1;
    if (multip_selection_reset() != 0) return -1;
    if (M32(0xa40) != 0)
        DP_FN(0x24, multip_dp_destroy_player_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
            A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4));
    if (multip_player_remove_all() != 0) return -1;
    M32(0xa40) = 0; M32(M_LOCAL_PLAYER) = 0;
    if (M32(M_CONNECTED) != 0)
        result = DP_FN(0x10, multip_dp_close_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY));
    M32(M_CONNECTED) = 0; M32(0x984) = 0; M32(0xa64) = 0;
    M32(0x9a0) = 0; M32(0x9a4) = 0;
    if (multip_directplay_unload() != 0) return -1;
    return result;
}

/* .text+0x9c70 -- keep retail's nesting until local startup reads the
 * reserved DPNAME word. Earlier worker/CHAIN calls can leave that word behind. */
#if defined(__i386__)
__attribute__((naked)) int32_t multip_session_complete(void)
{
    __asm__ volatile("movl _s_multip_image+0x1c, %eax\n\tpushl $0\n\tpushl $1\n\tpushl $0x1d\n\t"
        "call *0x20(%eax)\n\tcall _multip_workers_start\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "call _multip_callbacks_start\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "call _multip_player_start\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "call _multip_session_apply_receive_option\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "movl $1, _s_multip_image+0xa40\n\tcall _multip_player_enumerate\n\t"
        "testl %eax, %eax\n\tjnz 1f\n\tpushl $0\n\tcall _multip_message_1007@4\n\t"
        "testl %eax, %eax\n\tjnz 1f\n\tpushl $0\n\tcall _multip_message_100f@4\n\t"
        "negl %eax\n\tsbbl %eax, %eax\n\tret\n\t1: orl $-1, %eax\n\tret");
}
#else
int32_t multip_session_complete(void)
{
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 1, NULL);
    if (multip_workers_start() != 0) return -1;
    if (multip_callbacks_start() != 0) return -1;
    if (multip_player_start() != 0) return -1;
    if (multip_session_apply_receive_option() != 0) return -1;
    M32(0xa40) = 1;
    if (multip_player_enumerate() != 0) return -1;
    if (multip_message_1007(0) != 0) return -1;
    return multip_message_100f(0) != 0 ? -1 : 0;
}
#endif
