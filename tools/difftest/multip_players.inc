/* Stable-address heap/list banks let both DLLs expose the same pointer bugs. */
struct player_fixture {
    uint32_t uiNumber, uiCalls, uiBlocks, uiModels, uiBank;
    uint32_t auiLog[256][6], auiSizes[32], auiLive[32];
    uint8_t abyHeap[32][128], abyBanks[2][6][256];
    uint8_t abyHeaders[6][36], abyModels[6][128], abyGlobal[0x1200];
    uint32_t auiReturns[12], auiState[12][9], auiRecordHash[12][6];
};
static struct player_fixture s_stPlayers;
static const struct multip_module *s_pPlayersModule;
static uint32_t *player_log(uint32_t uiTag)
{
    uint32_t *puiLog;
    if (s_stPlayers.uiCalls >= 256) exit(2);
    puiLog = s_stPlayers.auiLog[s_stPlayers.uiCalls++];
    puiLog[0] = uiTag;
    return puiLog;
}
static int player_fail(void) { return s_stPlayers.uiNumber % 67 == s_stPlayers.uiCalls; }
static uint32_t player_address(const void *pAddress)
{
    uintptr_t uiAddress = (uintptr_t)pAddress, uiBase = (uintptr_t)s_pPlayersModule->data;
    return uiAddress >= uiBase && uiAddress < uiBase + 0x1000 ?
        0xe000 + uiAddress - uiBase : (uint32_t)uiAddress;
}
static int32_t STDCALL player_alloc(int32_t iKind, uint32_t uiSize, void **ppOut, int32_t iZero)
{
    uint32_t *puiLog = player_log(1), uiBlock = s_stPlayers.uiBlocks;
    puiLog[1] = iKind; puiLog[2] = uiSize; puiLog[3] = iZero;
    if (player_fail()) { *ppOut = NULL; return 1; }
    if (uiBlock >= 32 || uiSize > 128) exit(2);
    *ppOut = s_stPlayers.abyHeap[uiBlock];
    memset(*ppOut, iZero ? 0 : 0x97, uiSize);
    s_stPlayers.auiSizes[uiBlock] = uiSize; s_stPlayers.auiLive[uiBlock] = 1;
    s_stPlayers.uiBlocks++;
    return 0;
}
static int32_t STDCALL player_free(void **ppBlock)
{
    uint32_t *puiLog = player_log(2), uiIndex;
    puiLog[1] = player_address(*ppBlock);
    if (player_fail()) return 1;
    for (uiIndex = 0; uiIndex < s_stPlayers.uiBlocks; uiIndex++)
        if (*ppBlock == s_stPlayers.abyHeap[uiIndex]) s_stPlayers.auiLive[uiIndex] = 0;
    *ppBlock = NULL;
    return 0;
}
static int32_t STDCALL player_copy(void *pDest, const void *pSource, uint32_t uiSize)
{
    uint32_t *puiLog = player_log(3), uiIndex;
    puiLog[1] = player_address(pDest); puiLog[2] = uiSize; puiLog[3] = 2166136261u;
    for (uiIndex = 0; uiIndex < uiSize; uiIndex++)
        puiLog[3] = (puiLog[3] ^ ((const uint8_t *)pSource)[uiIndex]) * 16777619u;
    memmove(pDest, pSource, uiSize);
    return player_fail();
}
static int32_t STDCALL player_strcpy(char *pcDest, const char *pcSource)
{
    uint32_t *puiLog = player_log(4);
    puiLog[1] = player_address(pcDest); puiLog[2] = profile_hash(pcSource);
    strcpy(pcDest, pcSource); return player_fail();
}
static void *STDCALL player_append(void *pList)
{
    uint32_t *puiLog = player_log(5), uiCount = get_u32(pList, 0);
    uint32_t uiNext = s_stPlayers.uiBank ^ 1;
    if (uiCount >= 6) exit(2);
    puiLog[1] = player_address(pList); puiLog[2] = uiCount;
    memcpy(s_stPlayers.abyBanks[uiNext], s_stPlayers.abyBanks[s_stPlayers.uiBank], uiCount * 256);
    memset(s_stPlayers.abyBanks[uiNext][uiCount], 0, 256);
    s_stPlayers.uiBank = uiNext;
    put_u32(pList, 0, uiCount + 1); put_u32(pList, 4, 6);
    put_u32(pList, 0x18, (uint32_t)(uintptr_t)s_stPlayers.abyBanks[uiNext]);
    return s_stPlayers.abyBanks[uiNext][uiCount];
}
static void STDCALL player_remove(void *pList, void *pRecord)
{
    uint32_t *puiLog = player_log(6), uiCount = get_u32(pList, 0);
    uint8_t *pbyData = (uint8_t *)(uintptr_t)get_u32(pList, 0x18);
    uint32_t uiIndex = ((uint8_t *)pRecord - pbyData) / 256;
    puiLog[1] = uiCount; puiLog[2] = uiIndex;
    if (uiIndex >= uiCount) exit(2);
    memmove(pRecord, (uint8_t *)pRecord + 256, (uiCount - uiIndex - 1) * 256);
    put_u32(pList, 0, uiCount - 1);
}
static int32_t STDCALL player_title(void *pAircraft, uint32_t uiKind, char *pcTitle, uint32_t uiSize)
{
    uint32_t *puiLog = player_log(7);
    puiLog[1] = pAircraft == s_stPlayers.abyGlobal + 0x11e0; puiLog[2] = uiKind; puiLog[3] = uiSize;
    strcpy(pcTitle, "fixture aircraft"); return player_fail();
}
static int32_t STDCALL player_select(void *pAircraft, const char *pcTitle)
{
    uint32_t *puiLog = player_log(8), uiIndex = s_stPlayers.uiModels++;
    if (uiIndex >= 6) exit(2);
    puiLog[1] = profile_hash(pcTitle);
    put_u32(pAircraft, 0, (uint32_t)(uintptr_t)s_stPlayers.abyHeaders[uiIndex]);
    put_u32(pAircraft, 4, (uint32_t)(uintptr_t)s_stPlayers.abyModels[uiIndex]);
    put_u32(s_stPlayers.abyHeaders[uiIndex], 0x18, 128);
    put_u32(s_stPlayers.abyHeaders[uiIndex], 4, 0x12345678);
    put_u32(s_stPlayers.abyHeaders[uiIndex], 8, 64);
    put_u32(s_stPlayers.abyModels[uiIndex], 0, 128);
    return !player_fail();
}
static int32_t STDCALL player_model(void *pAircraft, uint32_t uiKind)
{
    uint32_t *puiLog = player_log(9); (void)pAircraft; puiLog[1] = uiKind; return !player_fail();
}
static int32_t STDCALL player_unload(void *pAircraft)
{ (void)pAircraft; player_log(10); return !player_fail(); }
static int32_t STDCALL player_release(void *pAircraft)
{ (void)pAircraft; player_log(11); return !player_fail(); }
static uint32_t STDCALL player_dispatch(uint32_t uiCode, uint32_t uiEvent, void *pList)
{
    uint32_t *puiLog = player_log(12);
    puiLog[1] = uiCode; puiLog[2] = uiEvent; puiLog[3] = player_address(pList);
    puiLog[4] = get_u32(pList, 0); puiLog[5] = player_address((void *)(uintptr_t)get_u32(pList, 4));
    return 0xabcdef01;
}
static int32_t STDCALL player_description(void *pObject, void *pDesc, uint32_t *puiSize)
{
    uint32_t *puiLog = player_log(13); (void)pObject;
    puiLog[1] = pDesc != NULL; puiLog[2] = *puiSize; *puiSize = 80;
    if (pDesc == NULL) return player_fail() ? -1 : (int32_t)0x8877001eu;
    memset(pDesc, 0, 80); return player_fail();
}
static int32_t STDCALL player_set_description(void *pObject, void *pDesc, uint32_t uiFlags)
{
    uint32_t *puiLog = player_log(14); (void)pObject;
    puiLog[1] = get_u32(pDesc, 0x40); puiLog[2] = uiFlags;
    return player_fail() ? -1 : s_stPlayers.uiNumber & 1 ? (int32_t)0x80004001u : 0;
}
static void player_snapshot(uint32_t uiStep, uint32_t uiResult)
{
    static const uint32_t s_auiOffsets[] = {0x660,0xa3c,0x9a0,0x9a4,0xa5c,0xa60,0xef8,0xefc,0xf1c};
    uint32_t uiIndex;
    s_stPlayers.auiReturns[uiStep] = uiResult;
    for (uiIndex = 0; uiIndex < 9; uiIndex++)
        s_stPlayers.auiState[uiStep][uiIndex] = get_u32(s_pPlayersModule->data, s_auiOffsets[uiIndex]);
    for (uiIndex = 0; uiIndex < get_u32(s_pPlayersModule->data, M_PLAYER_COUNT); uiIndex++) {
        const uint8_t *pbyRecord = (const uint8_t *)(uintptr_t)get_u32(s_pPlayersModule->data, M_PLAYERS) + uiIndex * 256;
        uint32_t uiByte, uiHash = 2166136261u;
        for (uiByte = 0; uiByte < 256; uiByte++) uiHash = (uiHash ^ pbyRecord[uiByte]) * 16777619u;
        s_stPlayers.auiRecordHash[uiStep][uiIndex] = uiHash;
    }
}

typedef int32_t (STDCALL *player_remote_fn)(const void *, uint32_t);
typedef void *(*player_append_fn)(void);
static void run_players(const struct multip_module *pModule, uint32_t uiNumber, struct player_fixture *pResult)
{
    uint8_t abyHost[0x320] = {0}; void *apAc[16] = {0}, *apChain[9] = {0};
    uint32_t auiName[4] = {16,0,0,0}, uiIndex, uiState = uiNumber ^ 0x87234516u;
    void *pFunction, *pRecord;
    memset(&s_stPlayers, 0, sizeof(s_stPlayers)); s_stPlayers.uiNumber = uiNumber; s_pPlayersModule = pModule;
    memcpy(pModule->data, pModule->initial_data, 0x1000);
    pFunction = player_alloc; memcpy(abyHost + 0x20c, &pFunction, 4);
    pFunction = player_free; memcpy(abyHost + 0x214, &pFunction, 4);
    pFunction = player_copy; memcpy(abyHost + 0x21c, &pFunction, 4);
    pFunction = player_strcpy; memcpy(abyHost + 0x2a8, &pFunction, 4);
    pFunction = player_append; memcpy(abyHost + 0x2f8, &pFunction, 4);
    pFunction = player_remove; memcpy(abyHost + 0x300, &pFunction, 4);
    apAc[8] = (void *)player_select; apAc[9] = (void *)player_release;
    apAc[12] = (void *)player_title; apAc[13] = (void *)player_model; apAc[14] = (void *)player_unload;
    apChain[8] = (void *)player_dispatch;
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x58 / 4] = (void *)player_description;
    s_directplay_vtable[0x7c / 4] = (void *)player_set_description;
    put_u32(pModule->data, 4, (uint32_t)(uintptr_t)abyHost);
    put_u32(pModule->data, 0xc, (uint32_t)(uintptr_t)s_stPlayers.abyGlobal);
    put_u32(pModule->data, 0x14, (uint32_t)(uintptr_t)apAc);
    put_u32(pModule->data, 0x1c, (uint32_t)(uintptr_t)apChain);
    put_u32(pModule->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(pModule->data, M_CONNECTED, 1); put_u32(pModule->data, 0xaa8, uiNumber & 2);
    put_u32(pModule->data, M_LOCAL_PLAYER, 0); put_u32(pModule->data, M_PLAYER_COUNT, 0);
    put_u32(pModule->data, M_PLAYERS, (uint32_t)(uintptr_t)s_stPlayers.abyBanks[0]);
    put_u32(pModule->data, M_PLAYER_MODE, 1); put_u32(pModule->data, M_ACTIVE_PLAYER_ID, 0x2001);
    for (uiIndex = 0; uiIndex < 4; uiIndex++) put_u32(pModule->data, 0x998 + uiIndex * 4, random_u32(&uiState));
    put_u32(pModule->data, 0x160, random_u32(&uiState));
    for (uiIndex = 0; uiIndex < 36; uiIndex++) s_stPlayers.abyGlobal[0x560 + uiIndex] = random_u32(&uiState);
    auiName[2] = (uint32_t)(uintptr_t)"remote name";
    player_snapshot(0, ((player_remote_fn)send_internal(pModule, 75))(auiName, 0x2001));
    if (get_u32(pModule->data, M_PLAYER_COUNT) != 0)
        put_u32((uint8_t *)(uintptr_t)get_u32(pModule->data, M_PLAYERS), 8, uiNumber % 3);
    player_snapshot(1, ((player_remote_fn)send_internal(pModule, 75))(auiName, 0x2002));
    player_snapshot(2, ((player_remote_fn)send_internal(pModule, 75))(auiName, 0x2001));
    player_snapshot(3, ((message_id_fn)send_internal(pModule, 77))(0x2002));
    player_snapshot(4, ((message_id_fn)send_internal(pModule, 77))(0x2001));
    player_snapshot(5, ((multip_worker_fn)send_internal(pModule, 78))());
    put_u32(pModule->data, M_LOCAL_PLAYER, 0);
    /* Clear partial survivors before independent append/local-removal cases. */
    put_u32(pModule->data, M_PLAYER_COUNT, 0);
    pRecord = ((player_append_fn)send_internal(pModule, 73))(); put_u32(pRecord, 4, 0x1234);
    put_u32(pModule->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)pRecord);
    pRecord = ((player_append_fn)send_internal(pModule, 73))(); put_u32(pRecord, 4, 0x5678);
    player_snapshot(6, get_u32(pModule->data, M_LOCAL_PLAYER));
    player_snapshot(7, ((metadata_collision_fn)send_internal(pModule, 74))(pRecord));
    put_u32(pModule->data, M_LOCAL_PLAYER, get_u32(pModule->data, M_PLAYERS));
    put_u32((uint8_t *)(uintptr_t)get_u32(pModule->data, M_LOCAL_PLAYER), 0xc, 1);
    player_snapshot(8, uiNumber & 1 ? ((message_id_fn)send_internal(pModule, 77))(0x1234) :
        ((multip_worker_fn)send_internal(pModule, 76))());
    memcpy(pResult, &s_stPlayers, sizeof(*pResult));
}
static uint32_t test_players(const struct multip_module *pRetail, const struct multip_module *pOurs, uint32_t uiScenarios)
{
    static struct player_fixture s_Retail, s_Ours; uint32_t uiNumber, uiBad = 0;
    for (uiNumber = 0; uiNumber < uiScenarios; uiNumber++) {
        run_players(pRetail, uiNumber, &s_Retail); run_players(pOurs, uiNumber, &s_Ours);
        if (memcmp(&s_Retail, &s_Ours, sizeof(s_Ours)) != 0) {
            if (uiBad < 4) {
                uint32_t uiByte;
                printf("player scenario %lu differs\n", (unsigned long)uiNumber);
                for (uiByte = 0; uiByte < sizeof(s_Ours); uiByte++)
                    if (((uint8_t *)&s_Retail)[uiByte] != ((uint8_t *)&s_Ours)[uiByte]) {
                        printf(" first byte %lu: retail %u ours %u\n", (unsigned long)uiByte, ((uint8_t *)&s_Retail)[uiByte], ((uint8_t *)&s_Ours)[uiByte]); break;
                    }
            } uiBad++;
        }
    }
    printf("MULTIP player lifecycle helpers: %lu scenarios, %lu differ\n", (unsigned long)uiScenarios, (unsigned long)uiBad); return uiBad;
}
