/* Received aircraft state and timing, .text+0x0ac0/+0x1810..+0x1ce9. */
int32_t FS98_STDCALL multip_handle_1000(uint32_t id, const void *packet, uint32_t size)
{
    uint8_t *player = multip_find_player(id);
    (void)size;
    if (player == NULL || A32(player, 0x28) == 0) return -1;
    if (A32(packet, 0xc) > A32(player, 0x50)) {
        if (A32(player, 0x2c) == A32(packet, 0x10)) {
            if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(A32(player, 0x28) + 0x6c),
                (const uint8_t *)packet + 0x14, A32(packet, 0x10) - 0x6c) != 0)
                return -1;
        }
        A32(player, 0x50) = A32(packet, 0xc);
    }
    return 0;
}

/* .text+0x1a80 -- third argument is unused, including its clock value. */
int32_t FS98_STDCALL multip_player_motion_init(void *player, const void *position, uint32_t timestamp)
{
    (void)timestamp;
    if (A32(player, 0x28) != 0 && HOST_FN(0x21c, multip_copy_fn)(
        (void *)(uintptr_t)(A32(player, 0x28) + 4), position, 36) != 0)
        return -1;
    AD(player, 0xb4) = 0; AD(player, 0xbc) = 0; AD(player, 0xc4) = 0;
    if (HOST_FN(0x218, multip_memset_fn)((uint8_t *)player + 0x7c, 0, 24) != 0)
        return -1;
    if (HOST_FN(0x218, multip_memset_fn)((uint8_t *)player + 0x9c, 0, 24) != 0)
        return -1;
    A32(player, 0x50) = 0;
    A32(player, 0x4c) = 1;
    return 0;
}

static void multip_unpack_position(void *position, const void *packet, uint32_t coordinates, uint32_t angles)
{
    uint32_t index, packed = A32(packet, angles);
    for (index = 0; index < 3; index++) {
        A32(position, index * 8 + 4) = A32(packet, coordinates + index * 4);
        *(uint16_t *)((uint8_t *)position + index * 8 + 2) =
            *(const uint16_t *)((const uint8_t *)packet + coordinates + 12 + index * 2);
    }
    A32(position, 24) = packed & 0xffc00000u;
    A32(position, 28) = (packed & 0xfffff000u) << 10;
    A32(position, 32) = (packed & 0xfffffffcu) << 20;
}

/* .text+0x18b0 -- absent peers are ignored, and the old-anchor copy really
 * has length zero. New sequence values reset both velocity blocks. */
int32_t FS98_STDCALL multip_handle_1018(uint32_t id, const void *packet, uint32_t size)
{
    uint8_t position[36], *player = multip_find_player(id);
    (void)size;
    if (player == NULL) return 0;
    if (HOST_FN(0x218, multip_memset_fn)(position, 0, sizeof(position)) != 0)
        return -1;
    multip_unpack_position(position, packet, 0x14, 0x10);
    if (A32(player, 0x4c) == 0) {
        uint32_t now = HOST_FN(0x38, multip_ticks_fn)();
        if (multip_player_motion_init(player, position, now) != 0) return -1;
    }
    if (A32(packet, 0xc) > A32(player, 0x50)) {
        if (HOST_FN(0x21c, multip_copy_fn)(player + 0x7c, player + 0x54, 0) != 0) return -1;
        if (HOST_FN(0x21c, multip_copy_fn)(player + 0x54, position, sizeof(position)) != 0) return -1;
        if (A32(player, 0x28) != 0 && HOST_FN(0x21c, multip_copy_fn)(
            (void *)(uintptr_t)(A32(player, 0x28) + 4), position, sizeof(position)) != 0)
            return -1;
        A32(player, 0x78) = HOST_FN(0x38, multip_ticks_fn)();
        A32(player, 0x34) = 0;
        if (HOST_FN(0x218, multip_memset_fn)(player + 0x7c, 0, 24) != 0) return -1;
        if (HOST_FN(0x218, multip_memset_fn)(player + 0x9c, 0, 24) != 0) return -1;
        A32(player, 0x50) = A32(packet, 0xc);
    }
    return multip_player_reset_collision(player) != 0 ? -1 : 0;
}

static void multip_unpack_velocity(double *velocity, const int32_t *packed)
{
    __asm__ volatile("fildl %1\n\tfmull %2\n\tfmull %3\n\tfstpl %0"
        : "=m"(*velocity) : "m"(*packed),
          "m"(*(double *)(void *)(s_multip_rdata + 0)),
          "m"(*(double *)(void *)(s_multip_rdata + 8)) : "st");
}

/* .text+0x1b30 -- unlike 1018, requires a model and leaves the received
 * position's padding words untouched. The entry shim supplies those words. */
static int32_t __attribute__((used, noinline)) multip_handle_1019_impl(uint32_t id,
    const void *packet, uint32_t size, uint32_t latitude_padding, uint32_t longitude_padding, uint32_t altitude_padding)
{
    uint8_t position[36], *player = multip_find_player(id);
    (void)size;
    if (player == NULL || A32(player, 0x28) == 0) return -1;
    *(uint16_t *)(void *)position = (uint16_t)latitude_padding;
    *(uint16_t *)(void *)(position + 8) = (uint16_t)longitude_padding;
    *(uint16_t *)(void *)(position + 16) = (uint16_t)altitude_padding;
    multip_unpack_position(position, packet, 0x24, 0x20);
    if (A32(player, 0x4c) == 0) {
        if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(A32(player, 0x28) + 4),
            position, sizeof(position)) != 0) return -1;
        {
            uint32_t now = HOST_FN(0x38, multip_ticks_fn)();
            if (multip_player_motion_init(player, (const void *)(uintptr_t)(A32(player, 0x28) + 4), now) != 0)
                return -1;
        }
    }
    if (A32(packet, 8) > A32(player, 0x50)) {
        if (HOST_FN(0x21c, multip_copy_fn)(player + 0x7c, player + 0x54, 0) != 0) return -1;
        if (HOST_FN(0x21c, multip_copy_fn)(player + 0x54, position, sizeof(position)) != 0) return -1;
        A32(player, 0x78) = HOST_FN(0x38, multip_ticks_fn)();
        A32(player, 0x34) = A32(packet, 0x1c);
        A32(player, 0x50) = A32(packet, 8);
        multip_unpack_velocity(&AD(player, 0x7c), (const int32_t *)((const uint8_t *)packet + 0x10));
        multip_unpack_velocity(&AD(player, 0x84), (const int32_t *)((const uint8_t *)packet + 0x14));
        multip_unpack_velocity(&AD(player, 0x8c), (const int32_t *)((const uint8_t *)packet + 0x18));
        A32(player, 0x94) = HOST_FN(0x38, multip_ticks_fn)();
        A32(player, 0x98) = 1;
    }
    return 0;
}
#if defined(__i386__)
__attribute__((naked)) int32_t FS98_STDCALL multip_handle_1019(uint32_t id __attribute__((unused)),
    const void *packet __attribute__((unused)), uint32_t size __attribute__((unused)))
{
    __asm__ volatile("movl %esp, %eax\n\tmovzwl -0x14(%eax), %edx\n\tpushl %edx\n\t"
        "movzwl -0x1c(%eax), %edx\n\tpushl %edx\n\tmovzwl -0x24(%eax), %edx\n\tpushl %edx\n\t"
        "pushl 12(%eax)\n\tpushl 8(%eax)\n\tpushl 4(%eax)\n\t"
        "call _multip_handle_1019_impl\n\taddl $24, %esp\n\tret $12");
}
#else
int32_t FS98_STDCALL multip_handle_1019(uint32_t id, const void *packet, uint32_t size)
{ return multip_handle_1019_impl(id, packet, size, 0, 0, 0); }
#endif

/* .text+0x1810 -- running signed timing average. Timestamp arithmetic wraps
 * before signed division by two; count conversion is unsigned, including wrap. */
int32_t FS98_STDCALL multip_handle_1017(uint32_t id, const void *packet, uint32_t size, uint32_t timestamp)
{
    uint8_t *player = multip_find_player(id);
    (void)size;
    if (player == NULL) return -1;
    if (A32(player, 0x40) != 0 && A32(packet, 0xc) != 0) {
        uint32_t delta = A32(packet, 0x10) - A32(packet, 8) - timestamp + A32(packet, 0xc);
        int32_t half = (int32_t)delta / 2;
        uint64_t old_count = A32(player, 0x48), new_count;
        int64_t result;
        int32_t previous = (int32_t)A32(player, 0x44);
        uint16_t control_word, truncate_word;
        A32(player, 0x48) = (uint32_t)old_count + 1;
        new_count = A32(player, 0x48);
        __asm__ volatile("fnstcw %0" : "=m"(control_word));
        truncate_word = control_word | 0x0c00;
        __asm__ volatile("fildll %1\n\tfildl %2\n\tfmulp %%st, %%st(1)\n\t"
            "fiaddl %3\n\tfildll %4\n\tfdivrp %%st, %%st(1)\n\t"
            "fldcw %5\n\tfistpll %0\n\tfldcw %6"
            : "=m"(result) : "m"(old_count), "m"(previous), "m"(half), "m"(new_count),
              "m"(truncate_word), "m"(control_word) : "st", "st(1)");
        A32(player, 0x44) = (uint32_t)result;
    }
    A32(player, 0x40) = timestamp;
    A32(player, 0x3c) = A32(packet, 0x10);
    return 0;
}
