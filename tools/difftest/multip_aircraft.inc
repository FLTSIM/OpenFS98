/* Aircraft ownership fixture: mutable ACONTAIN records and partial failures. */
struct aircraft_fixture {
    uint32_t auiReturns[3];
    uint32_t auiLog[8][5];
    uint32_t uiCalls;
    uint32_t uiNumber;
    uint32_t uiFreeCalls;
    uint8_t abyAircraft[3][0x30];
    uint8_t abyHeader[0x24];
    uint8_t abyModel[0x80];
};
static struct aircraft_fixture s_aircraft;

static uint32_t *aircraft_log(uint32_t uiTag, const void *pAircraft)
{
    uint32_t *puiEntry;
    if (s_aircraft.uiCalls >= 8) exit(2);
    puiEntry = s_aircraft.auiLog[s_aircraft.uiCalls++];
    puiEntry[0] = uiTag;
    puiEntry[1] = get_u32(pAircraft, 0xc);
    puiEntry[2] = get_u32(pAircraft, 0x10);
    puiEntry[3] = get_u32(pAircraft, 0x18);
    return puiEntry;
}

static int32_t STDCALL aircraft_select(void *pAircraft, const char *pcTitle)
{
    uint32_t *puiEntry = aircraft_log(1, pAircraft);
    puiEntry[4] = profile_hash(pcTitle);
    put_u32(pAircraft, 0, (uint32_t)(uintptr_t)s_aircraft.abyHeader);
    return s_aircraft.uiNumber % 5 == 0 ? 0 : -2;
}

static int32_t STDCALL aircraft_model_load(void *pAircraft, uint32_t uiKind)
{
    uint32_t *puiEntry = aircraft_log(2, pAircraft);
    puiEntry[4] = uiKind;
    put_u32(pAircraft, 4, (uint32_t)(uintptr_t)s_aircraft.abyModel);
    return s_aircraft.uiNumber % 7 == 0 ? 0 : 3;
}

static int32_t STDCALL aircraft_model_unload(void *pAircraft)
{
    aircraft_log(3, pAircraft);
    /* Real ACONTAIN can alter the record before reporting failure. */
    put_u32(pAircraft, 8, 0x81726354u);
    return s_aircraft.uiNumber % 11 == 0 ? 0 : 1;
}

static int32_t STDCALL aircraft_release(void *pAircraft)
{
    aircraft_log(4, pAircraft);
    put_u32(pAircraft, 0, 0);
    return s_aircraft.uiNumber % 13 == 0 ? 0 : -1;
}

static int32_t STDCALL aircraft_free(void **ppBlock)
{
    uint32_t uiCall = s_aircraft.uiFreeCalls++;
    uint32_t *puiEntry;
    if (s_aircraft.uiCalls >= 8) exit(2);
    puiEntry = s_aircraft.auiLog[s_aircraft.uiCalls++];
    puiEntry[0] = 5; puiEntry[1] = (uint32_t)(uintptr_t)*ppBlock;
    if ((s_aircraft.uiNumber / 3) % 4 == uiCall)
        return 1;
    *ppBlock = NULL;
    return 0;
}

typedef int32_t (STDCALL *aircraft_select_fn)(const char *, void *);
typedef int32_t (STDCALL *aircraft_release_fn)(void *);

static void run_aircraft(const struct multip_module *pModule, uint32_t uiNumber,
                         struct aircraft_fixture *pResult)
{
    uint8_t abyHost[0x320] = {0};
    void *apAc[16] = {0};
    uint8_t abyAircraft[0x30];
    uint32_t uiState = uiNumber ^ 0x48239176u;
    uint32_t uiIndex;
    void *pFunction = aircraft_free;

    memset(&s_aircraft, 0, sizeof(s_aircraft));
    s_aircraft.uiNumber = uiNumber;
    memcpy(pModule->data, pModule->initial_data, 0x1000);
    memcpy(abyHost + 0x214, &pFunction, 4);
    apAc[0x20 / 4] = (void *)aircraft_select;
    apAc[0x24 / 4] = (void *)aircraft_release;
    apAc[0x34 / 4] = (void *)aircraft_model_load;
    apAc[0x38 / 4] = (void *)aircraft_model_unload;
    put_u32(pModule->data, 4, (uint32_t)(uintptr_t)abyHost);
    put_u32(pModule->data, 0x14, (uint32_t)(uintptr_t)apAc);
    for (uiIndex = 0; uiIndex < sizeof(abyAircraft); uiIndex += 4)
        put_u32(abyAircraft, uiIndex, random_u32(&uiState));
    put_u32(abyAircraft, 0xc, 0);
    for (uiIndex = 0; uiIndex < sizeof(s_aircraft.abyHeader); uiIndex += 4)
        put_u32(s_aircraft.abyHeader, uiIndex, random_u32(&uiState));
    put_u32(s_aircraft.abyHeader, 0x18, uiNumber & 1);
    for (uiIndex = 0; uiIndex < sizeof(s_aircraft.abyModel); uiIndex += 4)
        put_u32(s_aircraft.abyModel, uiIndex, random_u32(&uiState));
    s_aircraft.auiReturns[0] = ((aircraft_select_fn)send_internal(pModule, 42))(
        "fixture aircraft", abyAircraft);
    memcpy(s_aircraft.abyAircraft[0], abyAircraft, sizeof(abyAircraft));
    put_u32(abyAircraft, 0xc, uiNumber & 3);
    s_aircraft.auiReturns[1] = ((aircraft_release_fn)send_internal(pModule, 43))(abyAircraft);
    memcpy(s_aircraft.abyAircraft[1], abyAircraft, sizeof(abyAircraft));
    /* A separately received model owns its buffers directly. */
    put_u32(abyAircraft, 0xc, 0);
    put_u32(abyAircraft, 0x10, 0x76543210u);
    put_u32(abyAircraft, 0x18, 0x65432109u);
    s_aircraft.auiReturns[2] = ((aircraft_release_fn)send_internal(pModule, 43))(abyAircraft);
    memcpy(s_aircraft.abyAircraft[2], abyAircraft, sizeof(abyAircraft));
    memcpy(pResult, &s_aircraft, sizeof(*pResult));
}

static uint32_t test_aircraft(const struct multip_module *pRetail,
                              const struct multip_module *pOurs, uint32_t uiScenarios)
{
    static struct aircraft_fixture s_Retail;
    static struct aircraft_fixture s_Ours;
    uint32_t uiNumber;
    uint32_t uiBad = 0;
    for (uiNumber = 0; uiNumber < uiScenarios; uiNumber++) {
        run_aircraft(pRetail, uiNumber, &s_Retail);
        run_aircraft(pOurs, uiNumber, &s_Ours);
        if (memcmp(&s_Retail, &s_Ours, sizeof(s_Ours)) != 0) {
            if (uiBad < 4) {
                uint32_t uiByte;
                printf("aircraft scenario %lu differs\n", (unsigned long)uiNumber);
                for (uiByte = 0; uiByte < sizeof(s_Ours); uiByte++)
                    if (((uint8_t *)&s_Retail)[uiByte] != ((uint8_t *)&s_Ours)[uiByte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)uiByte,
                            ((uint8_t *)&s_Retail)[uiByte], ((uint8_t *)&s_Ours)[uiByte]); break;
                    }
            }
            uiBad++;
        }
    }
    printf("MULTIP aircraft select/release: %lu scenarios, %lu differ\n",
           (unsigned long)uiScenarios, (unsigned long)uiBad);
    return uiBad;
}

struct aircraft_copy_fixture {
    uint32_t uiNumber;
    uint32_t uiCalls;
    uint32_t uiBlocks;
    uint32_t auiResults[9];
    uint32_t auiLog[128][5];
    uint32_t auiSizes[32];
    uint8_t abyHeap[32][160];
    uint8_t abySource[0x30];
    uint8_t abyTarget[0x30];
    uint8_t abySnapshots[2][0x30];
    uint8_t abyFlight[64];
    uint8_t abyModel[128];
    uint8_t abyDescription[80];
    uint8_t abySessionRecord[84];
    uint8_t abyPlayerRecord[16];
    uint8_t abyPlayerCopy[16];
    uint8_t abyEnumerated[4][16];
    uint8_t abyListSnapshots[2][84];
    uint8_t abyInitialSession[84];
};
static struct aircraft_copy_fixture s_aircraft_copy;
static const char s_acTitle[] = "received aircraft";

static uint32_t *aircraft_copy_log(uint32_t uiTag)
{
    uint32_t *puiEntry;
    if (s_aircraft_copy.uiCalls >= 128) exit(2);
    puiEntry = s_aircraft_copy.auiLog[s_aircraft_copy.uiCalls++];
    puiEntry[0] = uiTag;
    return puiEntry;
}

static int aircraft_copy_fail(void)
{
    return s_aircraft_copy.uiNumber % 97 == s_aircraft_copy.uiCalls;
}

static int32_t STDCALL aircraft_copy_alloc(int32_t iKind, uint32_t uiSize,
                                          void **ppOut, int32_t iZero)
{
    uint32_t *puiEntry = aircraft_copy_log(1);
    uint32_t uiBlock = s_aircraft_copy.uiBlocks;
    puiEntry[1] = iKind; puiEntry[2] = uiSize; puiEntry[3] = iZero;
    if (aircraft_copy_fail()) return 1;
    if (uiBlock >= 32 || uiSize > 160) exit(2);
    *ppOut = s_aircraft_copy.abyHeap[uiBlock];
    memset(*ppOut, iZero ? 0 : 0x9a, uiSize);
    s_aircraft_copy.auiSizes[uiBlock] = uiSize;
    s_aircraft_copy.uiBlocks++;
    return 0;
}

static int32_t STDCALL aircraft_copy_realloc(int32_t iKind, uint32_t uiSize, void **ppOut)
{
    uint32_t *puiEntry = aircraft_copy_log(2);
    uint32_t uiBlock = s_aircraft_copy.uiBlocks;
    puiEntry[1] = iKind; puiEntry[2] = uiSize;
    puiEntry[3] = (uint32_t)(uintptr_t)*ppOut;
    if (aircraft_copy_fail()) return 1;
    if (uiBlock >= 32 || uiSize > 160) exit(2);
    memset(s_aircraft_copy.abyHeap[uiBlock], 0xa7, uiSize);
    if (*ppOut != NULL) memcpy(s_aircraft_copy.abyHeap[uiBlock], *ppOut, uiSize);
    *ppOut = s_aircraft_copy.abyHeap[uiBlock];
    s_aircraft_copy.auiSizes[uiBlock] = uiSize;
    s_aircraft_copy.uiBlocks++;
    return 0;
}

static int32_t STDCALL aircraft_copy_memory(void *pDest, const void *pSource, uint32_t uiSize)
{
    uint32_t *puiEntry = aircraft_copy_log(3);
    puiEntry[1] = (uint32_t)(uintptr_t)pDest;
    puiEntry[2] = (uint32_t)(uintptr_t)pSource; puiEntry[3] = uiSize;
    memmove(pDest, pSource, uiSize);
    return aircraft_copy_fail();
}

static uint32_t STDCALL aircraft_copy_strlen(const char *pcText)
{
    uint32_t *puiEntry = aircraft_copy_log(4);
    puiEntry[1] = profile_hash(pcText);
    return strlen(pcText);
}

static int32_t STDCALL aircraft_copy_strcpy(char *pcDest, const char *pcSource)
{
    uint32_t *puiEntry = aircraft_copy_log(5);
    puiEntry[1] = (uint32_t)(uintptr_t)pcDest;
    puiEntry[2] = profile_hash(pcSource);
    strcpy(pcDest, pcSource);
    return aircraft_copy_fail();
}

static int32_t STDCALL aircraft_copy_free(void **ppBlock)
{
    uint32_t *puiEntry = aircraft_copy_log(6);
    puiEntry[1] = (uint32_t)(uintptr_t)*ppBlock;
    if (aircraft_copy_fail()) return 1;
    *ppBlock = NULL;
    return 0;
}

typedef int32_t (STDCALL *record_visitor_fn)(void *, void *);

static int32_t STDCALL record_walk(void *pList, void *pContext, record_visitor_fn pVisit)
{
    uint32_t *puiEntry = aircraft_copy_log(7);
    uint32_t uiCount = get_u32(pList, 0);
    uint32_t uiIndex;
    uint8_t *pbyData = (uint8_t *)(uintptr_t)get_u32(pList, 0x18);
    int32_t iResult;

    puiEntry[1] = uiCount; puiEntry[2] = pContext != NULL;
    if (uiCount > 4 || get_u32(pList, 0xc) != 16) exit(2);
    for (uiIndex = 0; uiIndex < uiCount; uiIndex++) {
        iResult = pVisit(pbyData + uiIndex * 16, pContext);
        if (iResult != 0) return iResult;
    }
    return 0;
}

static void *STDCALL record_add(void *pList)
{
    uint32_t *puiEntry = aircraft_copy_log(8);
    uint32_t uiCount = get_u32(pList, 0);
    void *pRecord;
    puiEntry[1] = uiCount; puiEntry[2] = get_u32(pList, 0xc);
    if (uiCount >= 4 || puiEntry[2] != 16) exit(2);
    pRecord = s_aircraft_copy.abyEnumerated[uiCount];
    memset(pRecord, 0, 16);
    put_u32(pList, 0, uiCount + 1);
    put_u32(pList, 4, 4);
    put_u32(pList, 0x18, (uint32_t)(uintptr_t)s_aircraft_copy.abyEnumerated);
    return pRecord;
}

static int32_t STDCALL record_enum(void *pObject, const GUID *pInstance,
    session_enum_callback pCallback, void *pContext, uint32_t uiFlags)
{
    uint32_t *puiEntry = aircraft_copy_log(9);
    uint32_t auiName[4] = {16, 0, 0, 0};
    char acLongName[4] = {'C', 'N', 0, 0};
    uint32_t uiIndex;
    puiEntry[1] = directplay_identity(pObject);
    puiEntry[2] = pInstance != NULL;
    puiEntry[3] = uiFlags;
    puiEntry[4] = 2166136261u;
    for (uiIndex = 0; uiIndex < 16; uiIndex++)
        puiEntry[4] = (puiEntry[4] ^ ((const uint8_t *)pInstance)[uiIndex]) * 16777619u;
    auiName[2] = (uint32_t)(uintptr_t)s_acTitle;
    auiName[3] = (uint32_t)(uintptr_t)acLongName;
    for (uiIndex = 0; uiIndex < 4; uiIndex++) {
        uint32_t *puiCallback;
        int32_t iContinue;
        acLongName[0] = (s_aircraft_copy.uiNumber & 1) ? 'H' : 'C';
        acLongName[1] = "POUN"[uiIndex];
        if (uiIndex == s_aircraft_copy.uiNumber % 5) {
            if (s_aircraft_copy.uiNumber & 2) acLongName[0] = 'x';
            else acLongName[1] = 'x';
        }
        iContinue = pCallback(0x2000 + uiIndex, uiIndex & 1, auiName, 0x40, pContext);
        puiCallback = aircraft_copy_log(10);
        puiCallback[1] = uiIndex; puiCallback[2] = iContinue;
        if (iContinue == 0) break;
    }
    return (int32_t)(s_aircraft_copy.uiNumber & 4 ? 0x80004005u : 0);
}

typedef int32_t (STDCALL *aircraft_copy_fn)(void *, const void *);
typedef int32_t (STDCALL *session_record_fn)(const void *, void *);
typedef int32_t (STDCALL *player_record_fn)(uint32_t, uint32_t, const void *, void *);
typedef int32_t (STDCALL *player_record_free_fn)(void *, void *);
typedef int32_t (STDCALL *record_enum_fn)(void *, const GUID *, uint32_t);

static void run_aircraft_copy(const struct multip_module *pModule, uint32_t uiNumber,
                              struct aircraft_copy_fixture *pResult)
{
    uint8_t abyHost[0x320] = {0};
    uint32_t uiState = uiNumber ^ 0x19872345u;
    uint32_t uiIndex;
    uint32_t auiName[4] = {16, 0, 0, 0};
    void *pFunction;

    memset(&s_aircraft_copy, 0, sizeof(s_aircraft_copy));
    s_aircraft_copy.uiNumber = uiNumber;
    memcpy(pModule->data, pModule->initial_data, 0x1000);
    pFunction = aircraft_copy_alloc; memcpy(abyHost + 0x20c, &pFunction, 4);
    pFunction = aircraft_copy_realloc; memcpy(abyHost + 0x210, &pFunction, 4);
    pFunction = aircraft_copy_free; memcpy(abyHost + 0x214, &pFunction, 4);
    pFunction = aircraft_copy_memory; memcpy(abyHost + 0x21c, &pFunction, 4);
    pFunction = aircraft_copy_strlen; memcpy(abyHost + 0x2b8, &pFunction, 4);
    pFunction = aircraft_copy_strcpy; memcpy(abyHost + 0x2a8, &pFunction, 4);
    pFunction = record_walk; memcpy(abyHost + 0x308, &pFunction, 4);
    pFunction = record_add; memcpy(abyHost + 0x2f8, &pFunction, 4);
    put_u32(pModule->data, 4, (uint32_t)(uintptr_t)abyHost);
    for (uiIndex = 0; uiIndex < sizeof(s_aircraft_copy.abyFlight); uiIndex++)
        s_aircraft_copy.abyFlight[uiIndex] = (uint8_t)random_u32(&uiState);
    for (uiIndex = 0; uiIndex < sizeof(s_aircraft_copy.abyModel); uiIndex++)
        s_aircraft_copy.abyModel[uiIndex] = (uint8_t)random_u32(&uiState);
    put_u32(s_aircraft_copy.abySource, 0x10, (uint32_t)(uintptr_t)s_aircraft_copy.abyFlight);
    put_u32(s_aircraft_copy.abySource, 0x14, uiNumber % 65);
    put_u32(s_aircraft_copy.abySource, 0x18, (uint32_t)(uintptr_t)s_aircraft_copy.abyModel);
    put_u32(s_aircraft_copy.abySource, 0x1c, 128);
    put_u32(s_aircraft_copy.abySource, 0x20, random_u32(&uiState));
    put_u32(s_aircraft_copy.abySource, 0x28, random_u32(&uiState));
    put_u32(s_aircraft_copy.abyModel, 0x64, (uint32_t)(uintptr_t)s_acTitle);
    for (uiIndex = 0; uiIndex < 2; uiIndex++) {
        s_aircraft_copy.auiResults[uiIndex] = ((aircraft_copy_fn)send_internal(pModule, 44))(
            s_aircraft_copy.abyTarget, s_aircraft_copy.abySource);
        memcpy(s_aircraft_copy.abySnapshots[uiIndex], s_aircraft_copy.abyTarget, 0x30);
    }
    for (uiIndex = 0; uiIndex < 80; uiIndex += 4)
        put_u32(s_aircraft_copy.abyDescription, uiIndex, random_u32(&uiState));
    put_u32(s_aircraft_copy.abyDescription, 0x30, (uint32_t)(uintptr_t)s_acTitle);
    memset(s_aircraft_copy.abySessionRecord, 0x97, 84);
    s_aircraft_copy.auiResults[2] = ((session_record_fn)send_internal(pModule, 45))(
        s_aircraft_copy.abyDescription, s_aircraft_copy.abySessionRecord);
    memcpy(s_aircraft_copy.abyInitialSession, s_aircraft_copy.abySessionRecord, 84);
    auiName[2] = (uint32_t)(uintptr_t)s_acTitle;
    put_u32(s_aircraft_copy.abyPlayerRecord, 4, random_u32(&uiState));
    put_u32(s_aircraft_copy.abyPlayerRecord, 8, random_u32(&uiState));
    put_u32(s_aircraft_copy.abyPlayerRecord, 12, (uint32_t)(uintptr_t)s_acTitle);
    s_aircraft_copy.auiResults[3] = ((player_record_fn)send_internal(pModule, 46))(
        random_u32(&uiState), random_u32(&uiState), auiName, s_aircraft_copy.abyPlayerRecord);
    s_aircraft_copy.auiResults[4] = ((aircraft_copy_fn)send_internal(pModule, 47))(
        s_aircraft_copy.abyPlayerCopy, s_aircraft_copy.abyPlayerRecord);
    s_aircraft_copy.auiResults[5] = ((player_record_free_fn)send_internal(pModule, 48))(
        s_aircraft_copy.abyPlayerCopy, (void *)(uintptr_t)random_u32(&uiState));
    memset(s_aircraft_copy.abySessionRecord + 0x38, 0, 28);
    put_u32(s_aircraft_copy.abySessionRecord, 0x40, 32);
    put_u32(s_aircraft_copy.abySessionRecord, 0x44, 16);
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x30 / 4] = (void *)record_enum;
    put_u32(pModule->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    /* Compare the GUID bytes by hash in the COM call transcript. */
    for (uiIndex = 0; uiIndex < 2; uiIndex++) {
        s_aircraft_copy.auiResults[6 + uiIndex] = ((record_enum_fn)send_internal(pModule, 50))(
            s_aircraft_copy.abySessionRecord, (const GUID *)s_aircraft_copy.abyDescription,
            random_u32(&uiState));
        memcpy(s_aircraft_copy.abyListSnapshots[uiIndex], s_aircraft_copy.abySessionRecord, 84);
    }
    s_aircraft_copy.auiResults[8] = ((record_enum_fn)send_internal(pModule, 50))(NULL, NULL, 0);
    memcpy(pResult, &s_aircraft_copy, sizeof(*pResult));
}

static uint32_t test_aircraft_copy(const struct multip_module *pRetail,
                                   const struct multip_module *pOurs, uint32_t uiScenarios)
{
    static struct aircraft_copy_fixture s_Retail;
    static struct aircraft_copy_fixture s_Ours;
    uint32_t uiNumber;
    uint32_t uiBad = 0;
    for (uiNumber = 0; uiNumber < uiScenarios; uiNumber++) {
        run_aircraft_copy(pRetail, uiNumber, &s_Retail);
        run_aircraft_copy(pOurs, uiNumber, &s_Ours);
        if (memcmp(&s_Retail, &s_Ours, sizeof(s_Ours)) != 0) {
            if (uiBad < 4) {
                uint32_t uiByte;
                printf("aircraft copy scenario %lu differs\n", (unsigned long)uiNumber);
                for (uiByte = 0; uiByte < sizeof(s_Ours); uiByte++)
                    if (((uint8_t *)&s_Retail)[uiByte] != ((uint8_t *)&s_Ours)[uiByte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)uiByte,
                            ((uint8_t *)&s_Retail)[uiByte], ((uint8_t *)&s_Ours)[uiByte]); break;
                    }
            }
            uiBad++;
        }
    }
    printf("MULTIP aircraft copy/session browser records: %lu scenarios, %lu differ\n",
           (unsigned long)uiScenarios, (unsigned long)uiBad);
    return uiBad;
}
