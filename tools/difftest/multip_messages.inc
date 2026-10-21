/* Fixed-header application messages through the real port's send queues. */
struct message_results {
    struct send_results heap;
    uint32_t auiReturns[14];
    uint32_t auiSignals[14];
    uint32_t auiState[14][4];
    uint32_t uiTicks;
};
static uint32_t s_uiMessageTicks;
static char s_acMessageTitle[80];
static uint8_t *s_pbyMessageGlobal;
static uint32_t s_uiMessageNumber;
static uint32_t STDCALL message_ticks(void) { return ++s_uiMessageTicks; }

static int32_t STDCALL message_title(void *pAircraft, uint32_t uiKind, char *pcOut, uint32_t uiCapacity)
{
    uint32_t *puiEntry = s_send_trace.log[s_send_trace.calls++];
    puiEntry[0] = 10; puiEntry[1] = pAircraft == s_pbyMessageGlobal + 0x11e0;
    puiEntry[2] = uiKind; puiEntry[3] = uiCapacity;
    strcpy(pcOut, s_acMessageTitle);
    return (int32_t)s_uiMessageNumber;
}
static uint32_t STDCALL message_strlen(const char *pcText)
{
    uint32_t *puiEntry = s_send_trace.log[s_send_trace.calls++];
    puiEntry[0] = 11; puiEntry[1] = profile_hash(pcText);
    return strlen(pcText);
}
static int32_t STDCALL message_strcpy(char *pcDest, const char *pcSource)
{
    uint32_t *puiEntry = s_send_trace.log[s_send_trace.calls++];
    puiEntry[0] = 12; puiEntry[1] = profile_hash(pcSource);
    strcpy(pcDest, pcSource);
    return (int32_t)s_uiMessageNumber;
}

typedef int32_t (STDCALL *message_id_fn)(uint32_t);
typedef int32_t (STDCALL *message_value_fn)(uint32_t, uint32_t);

static void run_messages(const struct multip_module *pModule, uint32_t uiNumber,
                         struct message_results *pResult)
{
    struct send_queue aQueues[2];
    uint8_t abyHost[0x320] = {0};
    uint8_t abyGlobal[0x1200] = {0};
    void *apAc[16] = {0};
    HANDLE hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t uiState = uiNumber ^ 0x15673824u;
    uint32_t uiIndex;
    uint32_t uiId;
    void *pFunction;

    if (hEvent == NULL) exit(2);
    memset(pResult, 0, sizeof(*pResult));
    memset(&s_send_trace, 0, sizeof(s_send_trace));
    memset(aQueues, 0, sizeof(aQueues));
    memcpy(pModule->data, pModule->initial_data, 0x1000);
    s_send_trace.fail_call = uiNumber % 3 == 0 ? 1 + (uiNumber / 3) % 65 : 0;
    s_send_free_calls = 0;
    s_send_fail_free = uiNumber % 5 == 0 ? 1 + (uiNumber / 5) % 3 : 0;
    s_uiMessageTicks = random_u32(&uiState);
    pFunction = message_ticks; memcpy(s_send_host + 0x38, &pFunction, 4);
    pFunction = send_alloc; memcpy(s_send_host + 0x20c, &pFunction, 4);
    pFunction = send_free; memcpy(s_send_host + 0x214, &pFunction, 4);
    pFunction = send_copy; memcpy(s_send_host + 0x21c, &pFunction, 4);
    memcpy(abyHost, s_send_host, sizeof(s_send_host));
    pFunction = message_strlen; memcpy(abyHost + 0x2b8, &pFunction, 4);
    pFunction = message_strcpy; memcpy(abyHost + 0x2a8, &pFunction, 4);
    apAc[0x30 / 4] = (void *)message_title;
    s_pbyMessageGlobal = abyGlobal;
    s_uiMessageNumber = uiNumber;
    for (uiIndex = 0; uiIndex < uiNumber % 80; uiIndex++)
        s_acMessageTitle[uiIndex] = 'A' + random_u32(&uiState) % 26;
    s_acMessageTitle[uiNumber % 80] = 0;
    abyGlobal[0x609] = (uint8_t)random_u32(&uiState);
    put_u32(pModule->data, 0xc, (uint32_t)(uintptr_t)abyGlobal);
    put_u32(pModule->data, 0x14, (uint32_t)(uintptr_t)apAc);
    for (uiIndex = 0; uiIndex < 2; uiIndex++) InitializeCriticalSection(&aQueues[uiIndex].lock);
    put_u32(pModule->data, 4, (uint32_t)(uintptr_t)abyHost);
    put_u32(pModule->data, 0xe60, uiNumber % 7 == 0 ? 0 : (uint32_t)(uintptr_t)&aQueues[0]);
    put_u32(pModule->data, 0xe64, (uint32_t)(uintptr_t)&aQueues[1]);
    put_u32(pModule->data, 0xe5c, (uint32_t)(uintptr_t)hEvent);
    put_u32(pModule->data, 0xf00, uiNumber % 4);
    put_u32(pModule->data, 0x984, random_u32(&uiState));
    put_u32(pModule->data, 0xadc, random_u32(&uiState));
    for (uiIndex = 0; uiIndex < 14; uiIndex++) {
        uiId = random_u32(&uiState);
        if (uiIndex < 4 || uiIndex == 11)
            pResult->auiReturns[uiIndex] = ((multip_worker_fn)send_internal(pModule, 59 + uiIndex))();
        else if (uiIndex < 6)
            pResult->auiReturns[uiIndex] = ((message_value_fn)send_internal(pModule, 59 + uiIndex))(
                uiId, random_u32(&uiState));
        else
            pResult->auiReturns[uiIndex] = ((message_id_fn)send_internal(pModule, 59 + uiIndex))(uiId);
        pResult->auiSignals[uiIndex] = WaitForSingleObject(hEvent, 0);
        pResult->auiState[uiIndex][0] = get_u32(pModule->data, 0x984);
        pResult->auiState[uiIndex][1] = get_u32(pModule->data, 0xadc);
        pResult->auiState[uiIndex][2] = aQueues[0].count;
        pResult->auiState[uiIndex][3] = aQueues[1].count;
    }
    for (uiIndex = 0; uiIndex < 2; uiIndex++) {
        pResult->heap.queues[uiIndex][0] = aQueues[uiIndex].count;
        pResult->heap.queues[uiIndex][1] = send_normalise(aQueues[uiIndex].head);
        pResult->heap.queues[uiIndex][2] = send_normalise(aQueues[uiIndex].tail);
        pResult->heap.queues[uiIndex][3] = aQueues[uiIndex].lock.LockCount;
        pResult->heap.queues[uiIndex][4] = aQueues[uiIndex].lock.RecursionCount;
        while (aQueues[uiIndex].lock.RecursionCount != 0)
            LeaveCriticalSection(&aQueues[uiIndex].lock);
        DeleteCriticalSection(&aQueues[uiIndex].lock);
    }
    snapshot_send_heap(&pResult->heap);
    pResult->uiTicks = s_uiMessageTicks;
    CloseHandle(hEvent);
}

static uint32_t test_messages(const struct multip_module *pRetail,
                              const struct multip_module *pOurs, uint32_t uiScenarios)
{
    static struct message_results s_Retail;
    static struct message_results s_Ours;
    uint32_t uiNumber;
    uint32_t uiBad = 0;
    for (uiNumber = 0; uiNumber < uiScenarios; uiNumber++) {
        run_messages(pRetail, uiNumber, &s_Retail);
        run_messages(pOurs, uiNumber, &s_Ours);
        if (memcmp(&s_Retail, &s_Ours, sizeof(s_Ours)) != 0) {
            if (uiBad < 4) {
                uint32_t uiByte;
                printf("message scenario %lu differs\n", (unsigned long)uiNumber);
                for (uiByte = 0; uiByte < sizeof(s_Ours); uiByte++)
                    if (((uint8_t *)&s_Retail)[uiByte] != ((uint8_t *)&s_Ours)[uiByte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)uiByte,
                            ((uint8_t *)&s_Retail)[uiByte], ((uint8_t *)&s_Ours)[uiByte]); break;
                    }
            }
            uiBad++;
        }
    }
    printf("MULTIP application message constructors: %lu scenarios, %lu differ\n",
           (unsigned long)uiScenarios, (unsigned long)uiBad);
    return uiBad;
}
