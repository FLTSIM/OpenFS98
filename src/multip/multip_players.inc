/* Player-array ownership and notifications. */
#include "multip_nametag.inc"

typedef uint32_t (FS98_STDCALL *multip_chain_dispatch_fn)(uint32_t, uint32_t, void *);
typedef void (FS98_STDCALL *multip_list_remove_fn)(void *, void *);

/* .text+0x8930 -- find checks the old local pointer before the moved array.
 * Preserve that stale-pointer behavior if the allocator moved the storage. */
void *multip_player_append(void)
{
    uint32_t local_id = 0;
    void *record;

    if (M32(M_LOCAL_PLAYER) != 0)
        local_id = A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4);
    record = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0x660));
    if (M32(M_LOCAL_PLAYER) != 0)
        M32(M_LOCAL_PLAYER) = ADDR(multip_find_player(local_id));
    return record;
}

/* .text+0x8980 */
int32_t FS98_STDCALL multip_player_mark_unknown(void *player)
{
    A32(player, 8) = 4;
    multip_player_reset_collision(player);
    return 0;
}

static void multip_player_publish_list(void)
{
    M32(0xef8) = M32(M_PLAYER_COUNT);
    M32(0xefc) = M32(M_PLAYERS);
}

/* .text+0x82c0 -- append before allocations, retaining partial records on
 * failure. Duplicate IDs do not change the existing record. */
int32_t FS98_STDCALL multip_player_add_remote(const void *name, uint32_t id)
{
    uint8_t *player;
    char title[0x104];
    uint8_t *global;

    if (multip_find_player(id) != NULL)
        return 0;
    player = multip_player_append();
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 21, (void **)player, 1) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)player,
        *(const char *const *)((const uint8_t *)name + 8));
    A32(player, 4) = id;
    AC_FN(0x30, multip_aircraft_title_fn)((void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11e0),
        1, title, sizeof(title));
    if (multip_aircraft_select(title, player + 0x10) != 0)
        return -1;
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(A32(player, 0x28) + 4),
        global + 0x560, 36) != 0)
        return -1;
    if (multip_player_mark_unknown(player) != 0)
        return -1;
    A32(player, 8) = 4;
    A32(player, 0x4c) = 0;
    A32(player, 0x44) = 0;
    A32(player, 0x3c) = 0;
    A32(player, 0x40) = 0;
    multip_player_publish_list();
    A32(player, 0xd4) = A32(player, 0);
    if (HOST_FN(0x20c, multip_alloc_fn)(1, sizeof(s_nametag),
        (void **)(void *)(player + 0xd0), 0) != 0)
        return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(*(void **)(void *)(player + 0xd0),
        s_nametag, sizeof(s_nametag)) != 0)
        return -1;
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 3, (void *)(uintptr_t)(D + 0xef8));
    return 0;
}

/* .text+0xa8a0 -- removing the local list entry leaves its pointer intact. */
int32_t multip_player_remove_local(void)
{
    uint32_t id = A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4);
    void *player = multip_find_player(id);

    if (player != NULL)
        HOST_FN(0x300, multip_list_remove_fn)((void *)(uintptr_t)(D + 0x660), player);
    return 0;
}

/* .text+0x84b0 */
int32_t FS98_STDCALL multip_player_remove(uint32_t id)
{
    uint8_t *player = multip_find_player(id);
    uint32_t local_id = 0;
    uint32_t type;

    if (player == NULL)
        return -1;
    if (A32(player, 0xc) != 0)
        return multip_player_remove_local() != 0 ? -1 : 0;
    if (M32(M_LOCAL_PLAYER) != 0)
        local_id = A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4);
    if (multip_aircraft_release(player + 0x10) != 0)
        return -1;
    if (A32(player, 0) != 0 && HOST_FN(0x214, multip_free_fn)((void **)player) != 0)
        return -1;
    if (HOST_FN(0x214, multip_free_fn)((void **)(void *)(player + 0xd0)) != 0)
        return -1;
    A32(player, 0xd4) = 0;
    if (M32(0xaa8) != 0) {
        type = A32(player, 8);
        if (type == 1) M32(0x9a0)--;
        else if (type == 2) M32(0x9a4)--;
        if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0)
            return -1;
    }
    if (M32(M_PLAYER_MODE) != 0 && A32(player, 4) == M32(M_ACTIVE_PLAYER_ID)) {
        if (multip_selection_reset() != 0)
            return -1;
        *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 0;
    }
    HOST_FN(0x300, multip_list_remove_fn)((void *)(uintptr_t)(D + 0x660), player);
    multip_player_publish_list();
    if (M32(M_LOCAL_PLAYER) != 0)
        M32(M_LOCAL_PLAYER) = ADDR(multip_find_player(local_id));
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 3, (void *)(uintptr_t)(D + 0xef8));
    return 0;
}

/* .text+0x8f00 */
int32_t multip_player_remove_all(void)
{
    uint32_t count = M32(M_PLAYER_COUNT);

    while (count-- != 0)
        if (multip_player_remove(A32((void *)(uintptr_t)M32(M_PLAYERS), 4)) != 0)
            return -1;
    return 0;
}

typedef int32_t (FS98_STDCALL *multip_dp_create_player_fn)(void *, uint32_t *,
    const void *, HANDLE, void *, uint32_t, uint32_t);

/* .text+0x8820. The reserved DPNAME word comes from the entry shim. */
static int32_t __attribute__((used, noinline)) multip_player_create_local_impl(uint32_t reserved)
{
    uint32_t name_fields[4];
    char short_name[1000];
    char long_name[10];
    uint8_t *local = multip_player_append();

    M32(M_LOCAL_PLAYER) = ADDR(local);
    if (HOST_FN(0x218, multip_memset_fn)(local, 0, 256) != 0)
        return -1;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    A32(local, 0) = D + 0x9a8;
    name_fields[0] = 16;
    name_fields[1] = reserved;
    name_fields[2] = ADDR(short_name);
    name_fields[3] = ADDR(long_name);
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    HOST_FN(0x2a8, multip_strcpy_fn)(short_name, *(const char **)local);
    if (HOST_FN(0x218, multip_memset_fn)(long_name, 0, 10) != 0)
        return -1;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    if (DP_FN(0x18, multip_dp_create_player_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        (uint32_t *)(void *)(local + 4), name_fields,
        (HANDLE)(uintptr_t)M32(M_RECEIVE_EVENT), NULL, 0, 0) != 0)
        return -1;
    multip_player_publish_list();
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 3, (void *)(uintptr_t)(D + 0xef8));
    return 0;
}

/* .text+0x8700 -- common host/join completion after creating the player. */
static int32_t __attribute__((used, noinline)) multip_player_start_impl(uint32_t reserved)
{
    uint8_t *local;
    uint32_t type;

    if (multip_player_create_local_impl(reserved) != 0)
        return -1;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    A32(local, 0xc) = 1;
    if (M32(0xaa8) != 0) {
        if (multip_player_mark_unknown(local) != 0)
            return -1;
        local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        A32(local, 8) = M32(0x1a4);
        local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        type = A32(local, 8);
        if (type == 1) M32(0x9a0)++;
        else if (type == 2) M32(0x9a4)++;
    } else {
        type = M32(0x1a4);
        if (type == 1) {
            if (multip_message_1001() != 0) return -1;
        } else if (type == 2) {
            if (multip_message_1002() != 0) return -1;
        }
    }
    if (multip_player_publish_name() != 0)
        return -1;
    if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0)
        return -1;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    if (multip_player_reset_collision(local) != 0)
        return -1;
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 3, (void *)(uintptr_t)(D + 0xef8));
    return 0;
}

#if defined(__i386__)
/* Retail allocates 0x404 bytes and leaves DPNAME+4 unwritten. Its caller
 * at +0x8700 contributes one more return address. Read those slots before
 * compiler frames or nested calls can overwrite them. */
__attribute__((naked)) int32_t multip_player_create_local(void)
{
    __asm__ volatile("movl -0x400(%esp), %eax\n\tpushl %eax\n\t"
                     "call _multip_player_create_local_impl\n\taddl $4, %esp\n\tret");
}
__attribute__((naked)) int32_t multip_player_start(void)
{
    __asm__ volatile("movl -0x404(%esp), %eax\n\tpushl %eax\n\t"
                     "call _multip_player_start_impl\n\taddl $4, %esp\n\tret");
}
#else
/* There is no original x86 caller stack on native targets. */
int32_t multip_player_create_local(void) { return multip_player_create_local_impl(0); }
int32_t multip_player_start(void) { return multip_player_start_impl(0); }
#endif

/* .text+0x86d0 -- only player records (type 1) are added; group names may
 * be absent and must not be inspected. */
static int32_t FS98_STDCALL multip_player_enumerate_callback(uint32_t id,
    uint32_t type, const void *name, uint32_t flags, void *context)
{
    (void)flags;
    (void)context;
    if (type == 1 && multip_player_add_remote(name, id) != 0)
        return 0;
    return 1;
}

/* .text+0x86b0 -- unlike most session helpers, return the raw HRESULT. */
int32_t multip_player_enumerate(void)
{
    return DP_FN(0x30, multip_dp_enum_players_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        (const GUID *)(uintptr_t)(D + 0x9c8), multip_player_enumerate_callback, NULL, 16);
}

/* .text+0x9960 -- descriptive-receive option selects the membership. */
int32_t multip_session_apply_receive_option(void)
{
    if (M32(0xac0) != 0)
        return multip_session_set_player() != 0 ? -1 : 0;
    return multip_session_set_observer() != 0 ? -1 : 0;
}
