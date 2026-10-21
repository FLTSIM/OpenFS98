/* Small application-message constructors. The retail 1000-byte stack buffers
 * hold only these initialized words; their unused tails are never sent. */
static int32_t multip_message_request(uint32_t tag)
{
    uint32_t packet[2] = {tag, 0};

    if (multip_send(0, packet, sizeof(packet), 1) != 0)
        return -1;
    M32(0x984) = 1;
    M32(0xadc) = HOST_FN(0x38, multip_ticks_fn)();
    return 0;
}

/* .text+0x5a80/+0x5ae0/+0x5b40/+0x5ba0 */
int32_t multip_message_1001(void) { return multip_message_request(0x1001); }
int32_t multip_message_1008(void) { return multip_message_request(0x1008); }
int32_t multip_message_1002(void) { return multip_message_request(0x1002); }
int32_t multip_message_1009(void) { return multip_message_request(0x1009); }

static int32_t multip_message_value(uint32_t id, uint32_t tag, uint32_t value)
{
    uint32_t packet[3] = {tag, 4, value};
    return multip_send(id, packet, sizeof(packet), 1) != 0 ? -1 : 0;
}

/* .text+0x5c00/+0x5c50 */
int32_t FS98_STDCALL multip_message_1003(uint32_t id, uint32_t value)
{ return multip_message_value(id, 0x1003, value); }
int32_t FS98_STDCALL multip_message_1004(uint32_t id, uint32_t value)
{ return multip_message_value(id, 0x1004, value); }

static int32_t multip_message_empty(uint32_t id, uint32_t tag)
{
    uint32_t packet[2] = {tag, 0};
    return multip_send(id, packet, sizeof(packet), 1) != 0 ? -1 : 0;
}

/* .text+0x5ca0/+0x5ce0/+0x5dd0/+0x5e10 */
int32_t FS98_STDCALL multip_message_1005(uint32_t id)
{ return multip_message_empty(id, 0x1005); }
int32_t FS98_STDCALL multip_message_1006(uint32_t id)
{ return multip_message_empty(id, 0x1006); }
int32_t FS98_STDCALL multip_message_100f(uint32_t id)
{ return multip_message_empty(id, 0x100f); }
int32_t FS98_STDCALL multip_message_1010(uint32_t id)
{ return multip_message_empty(id, 0x1010); }

/* Unlike the stack variants, .text+0x5e50/+0x5ec0/+0x5f30 allocate a packet
 * before enqueueing its copy. An enqueue failure leaks that first packet. */
static int32_t multip_message_allocated(uint32_t id, uint32_t tag)
{
    uint32_t *packet;

    if (HOST_FN(0x20c, multip_alloc_fn)(1, 8, (void **)&packet, 1) != 0)
        return -1;
    packet[0] = tag;
    packet[1] = 0;
    if (multip_send(id, packet, 8, 1) != 0)
        return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;
}

int32_t FS98_STDCALL multip_message_1011(uint32_t id)
{ return multip_message_allocated(id, 0x1011); }
int32_t multip_message_100a(void)
{ return multip_message_allocated(0, 0x100a); }
int32_t FS98_STDCALL multip_message_100c(uint32_t id)
{ return multip_message_allocated(id, 0x100c); }

typedef int32_t (FS98_STDCALL *multip_aircraft_title_fn)(void *, uint32_t, char *, uint32_t);

/* .text+0x5d20 -- aircraft title and visibility. Title-service and strcpy
 * return values are ignored. The length includes the title's terminator. */
int32_t FS98_STDCALL multip_message_100e(uint32_t id)
{
    char title[0x104];
    uint32_t packet[250];
    uint32_t size;

    packet[0] = 0x100e;
    AC_FN(0x30, multip_aircraft_title_fn)((void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11e0),
        1, title, sizeof(title));
    size = HOST_FN(0x2b8, multip_strlen_fn)(title) + 1;
    packet[1] = size;
    HOST_FN(0x2a8, multip_strcpy_fn)((char *)(packet + 3), title);
    packet[2] = *(uint8_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x609);
    return multip_send(id, packet, size + 12, 1) != 0 ? -1 : 0;
}

/* .text+0x6010 -- timestamp an existing peer before enqueueing its ping.
 * Broadcast ID zero and absent IDs still produce a message. */
int32_t FS98_STDCALL multip_message_1007(uint32_t id)
{
    uint32_t packet[2] = {0x1007, 0};
    uint8_t *player = multip_find_player(id);

    if (player != NULL)
        A32(player, 0xfc) = HOST_FN(0x38, multip_ticks_fn)();
    return multip_send(id, packet, sizeof(packet), 0) != 0 ? -1 : 0;
}

/* .text+0x6060 -- host departure broadcast. */
int32_t multip_message_1012(void)
{ return multip_message_allocated(0, 0x1012); }
