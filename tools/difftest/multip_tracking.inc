/* The CHAIN 0x19 display callback, including its leftover return value. */
struct track_fixture {
    uint32_t uiCalls;
    uint32_t uiChainResult;
    uint32_t uiCopyResult;
    uint32_t uiDrawResult;
    uint32_t uiMemoryCalls;
    uint32_t uiNumber;
    uint32_t auiLog[32][6];
    uint32_t auiResults[16];
    uint32_t auiState[16][8];
    uint8_t abyNames[16][20];
    uint8_t abyView[0x60];
    uint8_t abyPosition[24];
    uint32_t auiLocal[7];
};
static struct track_fixture s_track;
static const struct multip_module *s_track_module;
static uint8_t *s_track_global;

typedef uint32_t (STDCALL *track_callback_fn)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *track_show_fn)(uint32_t);

static uint32_t *track_log(uint32_t uiTag)
{
    uint32_t *puiEntry;
    if (s_track.uiCalls >= 32) exit(2);
    puiEntry = s_track.auiLog[s_track.uiCalls++];
    puiEntry[0] = uiTag;
    return puiEntry;
}

static int32_t STDCALL track_copy(char *pcDest, const char *pcSource)
{
    uint32_t *puiEntry = track_log(1);
    puiEntry[1] = pcDest == (char *)s_track_module->data + 0xf08;
    puiEntry[2] = profile_hash(pcSource);
    strcpy(pcDest, pcSource);
    return (int32_t)s_track.uiCopyResult;
}

static int32_t track_registration(uint32_t uiTag, uint32_t uiCode, track_callback_fn pCallback)
{
    uint32_t *puiEntry = track_log(uiTag);
    puiEntry[1] = uiCode;
    puiEntry[2] = (void *)pCallback == send_internal(s_track_module, 54);
    puiEntry[3] = get_u32(s_track_module->data, 0xf20);
    puiEntry[4] = get_u32(s_track_module->data, 0xf1c);
    return (int32_t)s_track.uiChainResult;
}

static int32_t STDCALL track_add(uint32_t uiCode, track_callback_fn pCallback)
{
    return track_registration(2, uiCode, pCallback);
}
static int32_t STDCALL track_remove(uint32_t uiCode, track_callback_fn pCallback)
{
    return track_registration(3, uiCode, pCallback);
}
static uint32_t STDCALL track_draw(uint32_t uiWindow, uint32_t uiA,
    uint32_t uiB, const char *pcName, uint32_t uiC)
{
    uint32_t *puiEntry = track_log(4);
    puiEntry[1] = uiWindow; puiEntry[2] = uiA;
    puiEntry[3] = uiB; puiEntry[4] = profile_hash(pcName); puiEntry[5] = uiC;
    return s_track.uiDrawResult;
}

static int32_t STDCALL track_memory(void *pDest, const void *pSource, uint32_t uiSize)
{
    uint32_t *puiEntry = track_log(5);
    uintptr_t uiSource = (uintptr_t)pSource;
    uintptr_t uiGlobal = (uintptr_t)s_track_global;
    uint32_t uiIndex;
    puiEntry[1] = (uint8_t *)pDest - s_track_global;
    puiEntry[2] = uiSource >= uiGlobal && uiSource < uiGlobal + 0x1200 ?
        uiSource - uiGlobal : 0x80000000u;
    puiEntry[3] = uiSize;
    puiEntry[4] = 2166136261u;
    for (uiIndex = 0; uiIndex < uiSize; uiIndex++)
        puiEntry[4] = (puiEntry[4] ^ ((const uint8_t *)pSource)[uiIndex]) * 16777619u;
    memmove(pDest, pSource, uiSize);
    s_track.uiMemoryCalls++;
    return s_track.uiNumber % 7 == s_track.uiMemoryCalls;
}

static void track_snapshot(const struct multip_module *pModule,
    const uint8_t *pbyGlobal, uint32_t uiStep, uint32_t uiReturn)
{
    uint32_t *puiState = s_track.auiState[uiStep];
    s_track.auiResults[uiStep] = uiReturn;
    puiState[0] = get_u32(pModule->data, 0xf1c);
    puiState[1] = get_u32(pModule->data, 0xf20);
    puiState[2] = get_u32(pModule->data, M_PLAYER_MODE);
    puiState[3] = get_u32(pModule->data, M_ACTIVE_PLAYER_ID);
    puiState[4] = get_u32(pbyGlobal, 0x11b0);
    puiState[5] = get_u32(pbyGlobal, 0x11dc);
    puiState[6] = get_u32(pbyGlobal, 0x5dc);
    puiState[7] = get_u32(pbyGlobal, 0xdc2);
    memcpy(s_track.abyNames[uiStep], pModule->data + 0xf08, 20);
}

static void run_tracking(const struct multip_module *pModule, uint32_t uiNumber,
                         struct track_fixture *pResult)
{
    uint8_t abyHost[0x320] = {0};
    void *apChain[9] = {0};
    uint8_t abyGlobal[0x1200];
    uint8_t abyLocal[0x100] = {0};
    uint8_t abyObject[32];
    uint8_t abyModel[40];
    void *pFunction;
    uint32_t uiState = uiNumber ^ 0x18234567u;
    uint32_t uiIndex;
    uint32_t uiWindow = random_u32(&uiState);
    uint32_t uiResult;
    track_callback_fn pCallback = (track_callback_fn)send_internal(pModule, 54);

    memset(&s_track, 0, sizeof(s_track));
    s_track.uiNumber = uiNumber;
    s_track_module = pModule;
    s_track_global = abyGlobal;
    memcpy(pModule->data, pModule->initial_data, 0x1000);
    for (uiIndex = 0; uiIndex < sizeof(abyGlobal); uiIndex += 4)
        put_u32(abyGlobal, uiIndex, random_u32(&uiState));
    put_u32(abyGlobal, 0x119c, uiWindow);
    put_u32(pModule->data, 4, (uint32_t)(uintptr_t)abyHost);
    put_u32(pModule->data, 0xc, (uint32_t)(uintptr_t)abyGlobal);
    put_u32(pModule->data, 0x1c, (uint32_t)(uintptr_t)apChain);
    put_u32(pModule->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)abyLocal);
    put_u32(abyLocal, 0, (uint32_t)(uintptr_t)"local track name");
    put_u32(abyLocal, 4, 0x1234);
    put_u32(pModule->data, M_PLAYER_MODE, random_u32(&uiState));
    put_u32(pModule->data, M_ACTIVE_PLAYER_ID, random_u32(&uiState));
    put_u32(pModule->data, 0xf20, uiNumber % 3);
    put_u32(pModule->data, 0xa10, uiNumber & 4 ? random_u32(&uiState) : uiNumber % 3);
    pFunction = track_copy; memcpy(abyHost + 0x2a8, &pFunction, 4);
    pFunction = track_draw; memcpy(abyHost + 0x8c, &pFunction, 4);
    pFunction = track_memory; memcpy(abyHost + 0x21c, &pFunction, 4);
    apChain[0x18 / 4] = (void *)track_add;
    apChain[0x1c / 4] = (void *)track_remove;
    s_track.uiChainResult = uiNumber & 1 ? random_u32(&uiState) | 1 : 0;
    s_track.uiCopyResult = random_u32(&uiState);
    s_track.uiDrawResult = random_u32(&uiState);
    uiResult = ((track_show_fn)send_internal(pModule, 53))(0x1234);
    track_snapshot(pModule, abyGlobal, 0, uiResult);
    uiResult = ((track_show_fn)send_internal(pModule, 53))(0x1234);
    track_snapshot(pModule, abyGlobal, 1, uiResult);
    track_snapshot(pModule, abyGlobal, 2, pCallback(0x19, uiWindow));
    track_snapshot(pModule, abyGlobal, 3, pCallback(0x7777, uiWindow ^ 1));
    uiResult = ((multip_worker_fn)send_internal(pModule, 51))();
    track_snapshot(pModule, abyGlobal, 4, uiResult);
    track_snapshot(pModule, abyGlobal, 5, pCallback(0x19, uiWindow));
    s_track.uiChainResult = 0;
    track_snapshot(pModule, abyGlobal, 6, pCallback(0x19, uiWindow));
    uiResult = ((multip_worker_fn)send_internal(pModule, 52))();
    track_snapshot(pModule, abyGlobal, 7, uiResult);
    track_snapshot(pModule, abyGlobal, 8, pCallback(0x19, uiWindow));
    for (uiIndex = 0; uiIndex < sizeof(abyObject); uiIndex += 4)
        put_u32(abyObject, uiIndex, random_u32(&uiState));
    for (uiIndex = 0; uiIndex < sizeof(abyModel); uiIndex += 4)
        put_u32(abyModel, uiIndex, random_u32(&uiState));
    put_u32(abyGlobal, 0x11e0, (uint32_t)(uintptr_t)abyObject);
    put_u32(abyGlobal, 0x11e4, uiNumber & 16 ? (uint32_t)(uintptr_t)abyModel : 0);
    uiResult = ((multip_worker_fn)send_internal(pModule, 57))();
    track_snapshot(pModule, abyGlobal, 9, uiResult);
    for (uiIndex = 0; uiIndex < 7; uiIndex++)
        s_track.auiLocal[uiIndex] = uiIndex == 2 ? get_u32(abyLocal, 0x28) != 0 :
            get_u32(abyLocal, 0x20 + uiIndex * 4);
    put_u32(pModule->data, M_PLAYER_COUNT, uiNumber & 8 ? 0 : 1);
    put_u32(pModule->data, M_PLAYERS, (uint32_t)(uintptr_t)abyLocal);
    put_u32(pModule->data, M_PLAYER_MODE, uiNumber % 3);
    put_u32(pModule->data, M_ACTIVE_PLAYER_ID, uiNumber & 2 ? 0x1234 : 0xdead);
    put_u32(pModule->data, 0xa58, (uiNumber / 3) % 3);
    uiResult = ((multip_local_fn)send_internal(pModule, 55))() == abyLocal;
    track_snapshot(pModule, abyGlobal, 10, uiResult);
    put_u32(abyGlobal, 0x5dc, (uiNumber & 1 ? 0x10000 : 0) | (uiNumber & 4 ? 1 : 0));
    put_u32(abyGlobal, 0x268, uiNumber & 2);
    put_u32(pModule->data, 0xad4, uiNumber & 8);
    uiResult = ((multip_worker_fn)send_internal(pModule, 56))();
    track_snapshot(pModule, abyGlobal, 11, uiResult);
    uiResult = ((multip_worker_fn)send_internal(pModule, 58))();
    track_snapshot(pModule, abyGlobal, 12, uiResult);
    memcpy(s_track.abyView, abyGlobal + 0x560, sizeof(s_track.abyView));
    memcpy(s_track.abyPosition, abyGlobal + 0xd74, sizeof(s_track.abyPosition));
    memcpy(pResult, &s_track, sizeof(*pResult));
}

static uint32_t test_tracking(const struct multip_module *pRetail,
                              const struct multip_module *pOurs, uint32_t uiScenarios)
{
    static struct track_fixture s_Retail;
    static struct track_fixture s_Ours;
    uint32_t uiNumber;
    uint32_t uiBad = 0;
    for (uiNumber = 0; uiNumber < uiScenarios; uiNumber++) {
        run_tracking(pRetail, uiNumber, &s_Retail);
        run_tracking(pOurs, uiNumber, &s_Ours);
        if (memcmp(&s_Retail, &s_Ours, sizeof(s_Ours)) != 0) {
            if (uiBad < 4) {
                uint32_t uiByte;
                printf("tracking scenario %lu differs\n", (unsigned long)uiNumber);
                for (uiByte = 0; uiByte < sizeof(s_Ours); uiByte++)
                    if (((uint8_t *)&s_Retail)[uiByte] != ((uint8_t *)&s_Ours)[uiByte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)uiByte,
                            ((uint8_t *)&s_Retail)[uiByte], ((uint8_t *)&s_Ours)[uiByte]); break;
                    }
            }
            uiBad++;
        }
    }
    printf("MULTIP track-name display: %lu scenarios, %lu differ\n",
           (unsigned long)uiScenarios, (unsigned long)uiBad);
    return uiBad;
}
