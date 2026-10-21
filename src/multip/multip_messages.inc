/* Small application-message constructors. The retail 1000-byte stack buffers
 * hold only these initialized words; their unused tails are never sent. */
static int32_t multip_message_request(uint32_t uiTag)
{
    uint32_t auiPacket[2] = {uiTag, 0};

    if (multip_send(0, auiPacket, sizeof(auiPacket), 1) != 0)
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

static int32_t multip_message_value(uint32_t uiId, uint32_t uiTag, uint32_t uiValue)
{
    uint32_t auiPacket[3] = {uiTag, 4, uiValue};
    return multip_send(uiId, auiPacket, sizeof(auiPacket), 1) != 0 ? -1 : 0;
}

/* .text+0x5c00/+0x5c50 */
int32_t FS98_STDCALL multip_message_1003(uint32_t uiId, uint32_t uiValue)
{ return multip_message_value(uiId, 0x1003, uiValue); }
int32_t FS98_STDCALL multip_message_1004(uint32_t uiId, uint32_t uiValue)
{ return multip_message_value(uiId, 0x1004, uiValue); }

static int32_t multip_message_empty(uint32_t uiId, uint32_t uiTag)
{
    uint32_t auiPacket[2] = {uiTag, 0};
    return multip_send(uiId, auiPacket, sizeof(auiPacket), 1) != 0 ? -1 : 0;
}

/* .text+0x5ca0/+0x5ce0/+0x5dd0/+0x5e10 */
int32_t FS98_STDCALL multip_message_1005(uint32_t uiId)
{ return multip_message_empty(uiId, 0x1005); }
int32_t FS98_STDCALL multip_message_1006(uint32_t uiId)
{ return multip_message_empty(uiId, 0x1006); }
int32_t FS98_STDCALL multip_message_100f(uint32_t uiId)
{ return multip_message_empty(uiId, 0x100f); }
int32_t FS98_STDCALL multip_message_1010(uint32_t uiId)
{ return multip_message_empty(uiId, 0x1010); }

/* Unlike the stack variants, .text+0x5e50/+0x5ec0/+0x5f30 allocate a packet
 * before enqueueing its copy. An enqueue failure leaks that first packet. */
static int32_t multip_message_allocated(uint32_t uiId, uint32_t uiTag)
{
    uint32_t *puiPacket;

    if (HOST_FN(0x20c, multip_alloc_fn)(1, 8, (void **)&puiPacket, 1) != 0)
        return -1;
    puiPacket[0] = uiTag;
    puiPacket[1] = 0;
    if (multip_send(uiId, puiPacket, 8, 1) != 0)
        return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&puiPacket) != 0 ? -1 : 0;
}

int32_t FS98_STDCALL multip_message_1011(uint32_t uiId)
{ return multip_message_allocated(uiId, 0x1011); }
int32_t multip_message_100a(void)
{ return multip_message_allocated(0, 0x100a); }
int32_t FS98_STDCALL multip_message_100c(uint32_t uiId)
{ return multip_message_allocated(uiId, 0x100c); }

typedef int32_t (FS98_STDCALL *multip_aircraft_title_fn)(void *, uint32_t, char *, uint32_t);

/* .text+0x5d20 -- aircraft title and visibility. Title-service and strcpy
 * return values are ignored. The length includes the title's terminator. */
int32_t FS98_STDCALL multip_message_100e(uint32_t uiId)
{
    char acTitle[0x104];
    uint32_t auiPacket[250];
    uint32_t uiSize;

    auiPacket[0] = 0x100e;
    AC_FN(0x30, multip_aircraft_title_fn)((void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11e0),
        1, acTitle, sizeof(acTitle));
    uiSize = HOST_FN(0x2b8, multip_strlen_fn)(acTitle) + 1;
    auiPacket[1] = uiSize;
    HOST_FN(0x2a8, multip_strcpy_fn)((char *)(auiPacket + 3), acTitle);
    auiPacket[2] = *(uint8_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x609);
    return multip_send(uiId, auiPacket, uiSize + 12, 1) != 0 ? -1 : 0;
}
